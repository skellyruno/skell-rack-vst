#!/usr/bin/env python3
"""Build a deterministic, preview-only Windows x64 VST3 package."""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import re
import struct
import sys
import zipfile
from pathlib import Path, PurePosixPath
from typing import Any


class PackageError(RuntimeError):
    """Raised when a release package does not meet the fail-closed contract."""


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _reject_duplicate_keys(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise PackageError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def _reject_non_finite(value: str) -> None:
    raise PackageError(f"Non-finite JSON number is not permitted: {value}")


def strip_trailing_commas(text: str) -> str:
    """Remove only commas before } or ] outside JSON strings."""
    output: list[str] = []
    in_string = False
    escaped = False
    index = 0
    while index < len(text):
        char = text[index]
        if in_string:
            output.append(char)
            if escaped:
                escaped = False
            elif char == "\\":
                escaped = True
            elif char == '"':
                in_string = False
            index += 1
            continue
        if char == '"':
            in_string = True
            output.append(char)
            index += 1
            continue
        if char == ",":
            lookahead = index + 1
            while lookahead < len(text) and text[lookahead].isspace():
                lookahead += 1
            if lookahead < len(text) and text[lookahead] in "}]":
                index += 1
                continue
        output.append(char)
        index += 1
    return "".join(output)


def _load_json(raw_bytes: bytes, path: Path) -> tuple[dict[str, Any], str | None, str]:
    try:
        raw_text = raw_bytes.decode("utf-8-sig")
    except UnicodeError as exc:
        raise PackageError(f"{path}: moduleinfo is not UTF-8: {exc}") from exc

    def parse(candidate: str) -> dict[str, Any]:
        try:
            value = json.loads(
                candidate,
                object_pairs_hook=_reject_duplicate_keys,
                parse_constant=_reject_non_finite,
            )
        except PackageError:
            raise
        except json.JSONDecodeError as exc:
            raise PackageError(f"{path}: invalid moduleinfo JSON: {exc}") from exc
        if not isinstance(value, dict):
            raise PackageError(f"{path}: moduleinfo root must be an object")
        return value

    try:
        return parse(raw_text), None, sha256_bytes(raw_bytes)
    except PackageError as strict_error:
        normalized = strip_trailing_commas(raw_text)
        if normalized == raw_text:
            raise strict_error
        try:
            value = parse(normalized)
        except PackageError as normalized_error:
            raise PackageError(
                f"{path}: invalid moduleinfo after JUCE trailing-comma normalization: {normalized_error}"
            ) from normalized_error
        normalized_text = json.dumps(value, indent=2, ensure_ascii=False) + "\n"
        return value, normalized_text, sha256_bytes(raw_bytes)


def _read_pe_identity(path: Path) -> tuple[int, int]:
    try:
        with path.open("rb") as stream:
            dos = stream.read(64)
            if len(dos) < 64 or dos[:2] != b"MZ":
                raise PackageError(f"Missing DOS MZ header: {path}")
            pe_offset = struct.unpack_from("<I", dos, 0x3C)[0]
            if pe_offset < 64:
                raise PackageError(f"Invalid PE header offset: {path}")
            stream.seek(pe_offset)
            if stream.read(4) != b"PE\0\0":
                raise PackageError(f"Invalid PE signature: {path}")
            coff = stream.read(20)
            if len(coff) != 20:
                raise PackageError(f"Truncated COFF header: {path}")
            machine = struct.unpack_from("<H", coff, 0)[0]
            optional_size = struct.unpack_from("<H", coff, 16)[0]
            if optional_size < 2:
                raise PackageError(f"Missing PE optional header: {path}")
            optional_magic = struct.unpack("<H", stream.read(2))[0]
    except OSError as exc:
        raise PackageError(f"Unable to inspect PE module {path}: {exc}") from exc
    if machine != 0x8664:
        raise PackageError(f"Expected AMD64 PE machine 0x8664, found 0x{machine:04x}: {path}")
    if optional_magic != 0x20B:
        raise PackageError(f"Expected PE32+ optional header 0x20b, found 0x{optional_magic:04x}: {path}")
    return machine, optional_magic


def _validate_moduleinfo(
    bundle: Path,
    expected_name: str,
    expected_version: str,
    expected_vendor: str | None,
) -> tuple[dict[str, Any], Path, str | None, str]:
    info_path = bundle / "Contents" / "Resources" / "moduleinfo.json"
    if not info_path.is_file():
        raise PackageError(f"Missing VST3 moduleinfo.json: {info_path}")
    raw = info_path.read_bytes()
    info, normalized_text, original_hash = _load_json(raw, info_path)

    name = info.get("Name")
    version = info.get("Version")
    if name != expected_name:
        raise PackageError(f"Expected product name {expected_name!r}, found {name!r}")
    if version != expected_version:
        raise PackageError(f"Expected product version {expected_version!r}, found {version!r}")

    factory = info.get("Factory Info")
    if not isinstance(factory, dict):
        raise PackageError("VST3 Factory Info must be an object")
    vendor = factory.get("Vendor")
    if not isinstance(vendor, str) or not vendor.strip():
        raise PackageError("VST3 Factory Info.Vendor must be a non-empty string")
    if expected_vendor and vendor != expected_vendor:
        raise PackageError(f"Expected vendor {expected_vendor!r}, found {vendor!r}")

    classes = info.get("Classes")
    if not isinstance(classes, list) or len(classes) < 2:
        raise PackageError("VST3 moduleinfo must contain processor and controller classes")
    seen_cids: set[str] = set()
    categories: set[str] = set()
    for index, entry in enumerate(classes):
        if not isinstance(entry, dict):
            raise PackageError(f"VST3 class {index} must be an object")
        cid = entry.get("CID")
        class_name = entry.get("Name")
        category = entry.get("Category")
        if not isinstance(cid, str) or not re.fullmatch(r"[0-9a-fA-F]{32}", cid):
            raise PackageError(f"VST3 class {index} has an invalid 32-hex CID")
        if cid.lower() in seen_cids:
            raise PackageError(f"Duplicate VST3 class CID: {cid}")
        if not isinstance(class_name, str) or not class_name.strip():
            raise PackageError(f"VST3 class {index} has no name")
        if not isinstance(category, str) or not category.strip():
            raise PackageError(f"VST3 class {index} has no category")
        seen_cids.add(cid.lower())
        categories.add(category.casefold())

    if "audio module class" not in categories or "component controller class" not in categories:
        raise PackageError("VST3 metadata must declare an Audio Module Class and Component Controller Class")

    return info, info_path, normalized_text, original_hash


def _validate_bundle(
    bundle: Path,
    expected_name: str,
    expected_version: str,
    expected_vendor: str | None,
) -> tuple[dict[str, Any], Path, str | None, str, Path, list[Path]]:
    if bundle.is_symlink() or not bundle.is_dir() or bundle.suffix.casefold() != ".vst3":
        raise PackageError(f"Not a safe VST3 bundle directory: {bundle}")
    for item in bundle.rglob("*"):
        if item.is_symlink():
            raise PackageError(f"VST3 bundle contains a symlink: {item}")
        if item.is_file() and item.suffix.casefold() in {".exe", ".zip", ".7z"}:
            raise PackageError(f"Unexpected executable or nested archive inside VST3 bundle: {item}")

    info, info_path, normalized_text, original_hash = _validate_moduleinfo(
        bundle, expected_name, expected_version, expected_vendor
    )
    binary_dir = bundle / "Contents" / "x86_64-win"
    binaries = sorted(p for p in binary_dir.glob("*.vst3") if p.is_file()) if binary_dir.is_dir() else []
    if len(binaries) != 1:
        raise PackageError(f"Expected exactly one x86_64-win VST3 module, found {len(binaries)}")
    _read_pe_identity(binaries[0])
    files = sorted(path for path in bundle.rglob("*") if path.is_file())
    if info_path not in files or binaries[0] not in files:
        raise PackageError("VST3 metadata or module is missing from the bundle file set")
    return info, info_path, normalized_text, original_hash, binaries[0], files


def _safe_archive_name(name: str) -> None:
    path = PurePosixPath(name)
    if not name or path.is_absolute() or ".." in path.parts or "\\" in name:
        raise PackageError(f"Unsafe archive path: {name!r}")


def package_vst3(
    build_root: Path,
    output: Path,
    expected_name: str,
    expected_version: str,
    expected_vendor: str | None = None,
) -> dict[str, Any]:
    build_root = build_root.resolve()
    output = output.resolve()
    if not build_root.is_dir():
        raise PackageError(f"Build root does not exist: {build_root}")
    manifest_path = output.with_suffix(".manifest.json")
    checksum_path = output.with_suffix(".sha256")
    for candidate in (output, manifest_path, checksum_path, output.with_name(output.name + ".tmp")):
        if candidate.exists():
            raise PackageError(f"Refusing to overwrite existing release output: {candidate}")

    bundles = sorted(path for path in build_root.rglob("*.vst3") if path.is_dir())
    if len(bundles) != 1:
        raise PackageError(f"Expected exactly one VST3 bundle below {build_root}; found {len(bundles)}")
    bundle = bundles[0]
    info, info_path, normalized_text, original_moduleinfo_hash, binary, files = _validate_bundle(
        bundle, expected_name, expected_version, expected_vendor
    )

    output.parent.mkdir(parents=True, exist_ok=True)
    temp_output = output.with_name(output.name + ".tmp")
    records: list[dict[str, Any]] = []
    source_commit = os.environ.get("SOURCE_COMMIT") or os.environ.get("GITHUB_SHA")
    factory = info.get("Factory Info", {})
    build_info = {
        "schema_version": 1,
        "product": expected_name,
        "version": expected_version,
        "vendor": factory.get("Vendor"),
        "format": "VST3",
        "platform": "windows-x64",
        "architecture": "AMD64",
        "bundle": bundle.name,
        "source_commit": source_commit,
        "ci_checkout_commit": os.environ.get("GITHUB_SHA"),
        "workflow_run_id": os.environ.get("GITHUB_RUN_ID"),
        "workflow_attempt": os.environ.get("GITHUB_RUN_ATTEMPT"),
        "ref": os.environ.get("GITHUB_REF"),
        "event_name": os.environ.get("GITHUB_EVENT_NAME"),
        "pluginval_strictness_required": 5,
        "preview_only": True,
        "release_gates": [
            "FL Studio scan/load and project save/reopen are not verified by this package workflow.",
            "Real musical A/B and target-hardware CPU/latency acceptance remain outstanding.",
            "Product and dependency licensing/attribution review remains outstanding.",
            "Vendor URL and support email require owner-approved values.",
            "Per-user Windows install/update/rollback helper is included and CI self-tested; user-machine FL Studio and SkellyHub integration remain outstanding.",
        ],
    }
    build_info_bytes = (json.dumps(build_info, indent=2, sort_keys=True) + "\n").encode("utf-8")
    preview_bytes = (
        "PRIVATE PREVIEW ONLY — NOT APPROVED FOR COMMERCIAL DISTRIBUTION.\n"
        "Windows x64 VST3 private preview. The per-user installer/rollback helper is CI-tested, "
        "but FL Studio, musical A/B, licensing, vendor contact and SkellyHub integration remain outstanding.\n"
    ).encode("utf-8")

    try:
        with zipfile.ZipFile(temp_output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
            for path in files:
                relative = path.relative_to(bundle).as_posix()
                archive_name = f"{bundle.name}/{relative}"
                _safe_archive_name(archive_name)
                if path == info_path and normalized_text is not None:
                    payload = normalized_text.encode("utf-8")
                else:
                    payload = path.read_bytes()
                archive.writestr(archive_name, payload)
                records.append({"path": archive_name, "size": len(payload), "sha256": sha256_bytes(payload)})

            archive.writestr("build-info.json", build_info_bytes)
            records.append({"path": "build-info.json", "size": len(build_info_bytes), "sha256": sha256_bytes(build_info_bytes)})
            archive.writestr("PREVIEW-ONLY.txt", preview_bytes)
            records.append({"path": "PREVIEW-ONLY.txt", "size": len(preview_bytes), "sha256": sha256_bytes(preview_bytes)})
            checksum_text = "".join(f"{record['sha256']}  {record['path']}\n" for record in records)
            archive.writestr("SHA256SUMS.txt", checksum_text.encode("ascii"))

        with zipfile.ZipFile(temp_output, "r") as archive:
            if archive.testzip() is not None:
                raise PackageError("Packaged ZIP failed its CRC check")
            names = archive.namelist()
            if len(names) != len(set(names)):
                raise PackageError("Package contains duplicate ZIP paths")
            for name in names:
                _safe_archive_name(name)
            roots = {PurePosixPath(name).parts[0] for name in names}
            if roots != {bundle.name, "build-info.json", "PREVIEW-ONLY.txt", "SHA256SUMS.txt"}:
                raise PackageError(f"Unexpected package roots: {sorted(roots)}")
            if any(name.casefold().endswith((".zip", ".7z", ".exe")) for name in names):
                raise PackageError("Package contains a nested archive or standalone executable")
            for record in records:
                if sha256_bytes(archive.read(record["path"])) != record["sha256"]:
                    raise PackageError(f"Packaged content hash mismatch: {record['path']}")
            packaged_info = json.loads(archive.read(f"{bundle.name}/Contents/Resources/moduleinfo.json"))
            if packaged_info.get("Name") != expected_name or packaged_info.get("Version") != expected_version:
                raise PackageError("Packaged moduleinfo identity does not match expected product/version")

        temp_output.replace(output)
    except Exception:
        if temp_output.exists():
            temp_output.unlink()
        raise

    archive_hash = sha256_file(output)
    binary_record = next(record for record in records if record["path"].endswith(f"/{binary.relative_to(bundle).as_posix()}"))
    moduleinfo_record = next(record for record in records if record["path"].endswith("/Contents/Resources/moduleinfo.json"))
    manifest = {
        **build_info,
        "moduleinfo_normalized": normalized_text is not None,
        "moduleinfo_original_sha256": original_moduleinfo_hash,
        "moduleinfo_packaged_sha256": moduleinfo_record["sha256"],
        "binary": binary_record["path"],
        "binary_sha256": binary_record["sha256"],
        "bundle_file_count": len(files),
        "package_file_count": len(records) + 1,
        "archive": output.name,
        "archive_size_bytes": output.stat().st_size,
        "archive_sha256": archive_hash,
        "factory_url_present": bool(factory.get("URL")),
        "support_email_present": bool(factory.get("E-Mail")),
        "classes": [
            {"cid": entry.get("CID"), "name": entry.get("Name"), "category": entry.get("Category")}
            for entry in info.get("Classes", [])
        ],
        "files": records,
    }
    temp_manifest = manifest_path.with_name(manifest_path.name + ".tmp")
    temp_checksum = checksum_path.with_name(checksum_path.name + ".tmp")
    try:
        temp_manifest.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        temp_checksum.write_text(f"{archive_hash}  {output.name}\n", encoding="ascii")
        temp_manifest.replace(manifest_path)
        temp_checksum.replace(checksum_path)
    except Exception:
        for candidate in (temp_manifest, temp_checksum, manifest_path, checksum_path, output):
            if candidate.exists():
                candidate.unlink()
        raise
    return manifest


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--expected-name", required=True)
    parser.add_argument("--expected-version", required=True)
    parser.add_argument("--expected-vendor", default=None)
    args = parser.parse_args(argv)
    try:
        manifest = package_vst3(
            args.build_root,
            args.output,
            args.expected_name,
            args.expected_version,
            args.expected_vendor,
        )
    except PackageError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2
    print(
        "PASS: packaged one Windows x64 VST3 bundle "
        f"{manifest['bundle']} v{manifest['version']} "
        f"({manifest['bundle_file_count']} bundle files, SHA-256 {manifest['archive_sha256']})"
    )
    print(f"Manifest: {args.output.with_suffix('.manifest.json')}")
    print(f"Checksum: {args.output.with_suffix('.sha256')}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
