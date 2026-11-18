from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import struct
import subprocess
import tempfile
import unittest
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INSTALLER = ROOT / "release" / "Install-ModularFXSuite.ps1"
PRODUCT = "Modular FX Suite"
VENDOR = "SkellAudio"
BUNDLE = "Modular FX Suite.vst3"
MODULE_PATH = f"{BUNDLE}/Contents/x86_64-win/Modular FX Suite.vst3"
MODULEINFO_PATH = f"{BUNDLE}/Contents/Resources/moduleinfo.json"
ACTUAL_PACKAGE: Path | None = None


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def fake_pe64(machine: int = 0x8664) -> bytes:
    data = bytearray(512)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", data, 0x84, machine)
    struct.pack_into("<H", data, 0x94, 0xF0)
    struct.pack_into("<H", data, 0x98, 0x20B)
    return bytes(data)


def make_package(
    path: Path,
    version: str,
    *,
    corrupt_checksum: bool = False,
    duplicate_cid: bool = False,
    wrong_machine: bool = False,
    wrong_name: bool = False,
) -> Path:
    module_info = {
        "Name": "Wrong Product" if wrong_name else PRODUCT,
        "Version": version,
        "Factory Info": {"Vendor": VENDOR, "URL": "", "E-Mail": ""},
        "Classes": [
            {
                "CID": "ABCDEF019182FAEB536B656C4D667831",
                "Category": "Audio Module Class",
                "Name": PRODUCT,
                "Vendor": VENDOR,
                "Version": version,
            },
            {
                "CID": (
                    "ABCDEF019182FAEB536B656C4D667831"
                    if duplicate_cid
                    else "ABCDEF011234ABCD536B656C4D667831"
                ),
                "Category": "Component Controller Class",
                "Name": PRODUCT,
                "Vendor": VENDOR,
                "Version": version,
            },
        ],
    }
    build_info = {
        "schema_version": 1,
        "product": PRODUCT,
        "version": version,
        "vendor": VENDOR,
        "format": "VST3",
        "platform": "windows-x64",
        "architecture": "AMD64",
        "bundle": BUNDLE,
        "source_commit": "0123456789abcdef0123456789abcdef01234567",
        "ci_checkout_commit": "89abcdef0123456789abcdef0123456789abcdef",
        "workflow_run_id": "test-run",
        "workflow_attempt": "1",
        "ref": "refs/heads/test",
        "event_name": "workflow_dispatch",
        "pluginval_strictness_required": 5,
        "preview_only": True,
        "release_gates": [],
    }
    files = {
        MODULEINFO_PATH: json.dumps(module_info, separators=(",", ":")).encode("utf-8"),
        MODULE_PATH: fake_pe64(0x14C if wrong_machine else 0x8664),
        "build-info.json": (json.dumps(build_info, sort_keys=True) + "\n").encode("utf-8"),
        "PREVIEW-ONLY.txt": (
            "PRIVATE PREVIEW ONLY — NOT APPROVED FOR COMMERCIAL DISTRIBUTION.\n"
        ).encode("utf-8"),
    }
    records = []
    for name, payload in sorted(files.items()):
        records.append({"path": name, "size": len(payload), "sha256": sha256(payload)})
    checksums = []
    for record in records:
        digest = record["sha256"]
        if corrupt_checksum and record["path"] == MODULE_PATH:
            digest = "0" * 64
        checksums.append(f"{digest}  {record['path']}\n")
    files["SHA256SUMS.txt"] = "".join(checksums).encode("ascii")

    path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for name, payload in files.items():
            archive.writestr(name, payload)

    archive_hash = sha256(path.read_bytes())
    manifest = {
        **build_info,
        "moduleinfo_normalized": False,
        "moduleinfo_original_sha256": sha256(files[MODULEINFO_PATH]),
        "moduleinfo_packaged_sha256": sha256(files[MODULEINFO_PATH]),
        "binary": MODULE_PATH,
        "binary_sha256": sha256(files[MODULE_PATH]),
        "bundle_file_count": 2,
        "package_file_count": 5,
        "archive": path.name,
        "archive_size_bytes": path.stat().st_size,
        "archive_sha256": archive_hash,
        "factory_url_present": False,
        "support_email_present": False,
        "classes": module_info["Classes"],
        "files": records,
    }
    path.with_suffix(".manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    path.with_suffix(".sha256").write_text(
        f"{archive_hash}  {path.name}\n", encoding="ascii"
    )
    return path


def module_version(bundle: Path) -> str:
    info = json.loads((bundle / "Contents" / "Resources" / "moduleinfo.json").read_text())
    return info["Version"]


class InstallerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.install_root = self.root / "install" / "VST3"
        self.backup_root = self.root / "backups"
        self.actual_package = ACTUAL_PACKAGE
        self.assertIsNotNone(self.actual_package)
        self.assertTrue(self.actual_package.is_file())
        self.assertTrue(self.actual_package.with_suffix(".manifest.json").is_file())
        self.assertTrue(self.actual_package.with_suffix(".sha256").is_file())

    def tearDown(self) -> None:
        self.temp.cleanup()

    def run_installer(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        command = [
            "pwsh",
            "-NoLogo",
            "-NoProfile",
            "-NonInteractive",
            "-File",
            str(INSTALLER),
            *map(str, arguments),
        ]
        return subprocess.run(command, capture_output=True, text=True, timeout=120)

    def install_args(self, package: Path) -> tuple[str, ...]:
        return (
            "-PackagePath", str(package),
            "-InstallRoot", str(self.install_root),
            "-BackupRoot", str(self.backup_root),
        )

    def assert_installer_ok(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        result = self.run_installer(*arguments)
        self.assertEqual(
            result.returncode,
            0,
            msg=f"Installer failed.\nSTDOUT:\n{result.stdout}\nSTDERR:\n{result.stderr}",
        )
        return result

    def assert_installer_rejects(self, *arguments: str) -> subprocess.CompletedProcess[str]:
        result = self.run_installer(*arguments)
        self.assertNotEqual(
            result.returncode,
            0,
            msg=f"Installer unexpectedly accepted an invalid request.\nSTDOUT:\n{result.stdout}",
        )
        return result

    def test_whatif_validates_real_package_without_creating_paths(self) -> None:
        result = self.assert_installer_ok(*self.install_args(self.actual_package), "-WhatIf")
        self.assertIn("What if:", result.stdout)
        self.assertFalse(self.install_root.exists())
        self.assertFalse(self.backup_root.exists())

    def test_real_package_fresh_install_and_repeat_are_safe(self) -> None:
        self.assert_installer_ok(*self.install_args(self.actual_package))
        bundle = self.install_root / BUNDLE
        self.assertTrue(bundle.is_dir())
        self.assertEqual(module_version(bundle), "1.0.0")
        self.assertFalse(self.backup_root.exists())
        result = self.assert_installer_ok(*self.install_args(self.actual_package))
        self.assertIn("already installed", result.stdout)
        self.assertFalse(self.backup_root.exists())

    def test_update_creates_verified_backup_and_rollback_restores_it(self) -> None:
        old_package = make_package(self.root / "old" / "ModularFXSuite-Windows-VST3.zip", "0.9.0")
        self.assert_installer_ok(*self.install_args(old_package))
        self.assertEqual(module_version(self.install_root / BUNDLE), "0.9.0")

        self.assert_installer_ok(*self.install_args(self.actual_package))
        self.assertEqual(module_version(self.install_root / BUNDLE), "1.0.0")
        backups = sorted(self.backup_root.glob("previous-*"))
        self.assertEqual(len(backups), 1)
        self.assertTrue((backups[0] / "backup-manifest.json").is_file())
        self.assertEqual(module_version(backups[0] / BUNDLE), "0.9.0")

        self.assert_installer_ok(
            "-Rollback",
            "-BackupPath", str(backups[0]),
            "-InstallRoot", str(self.install_root),
            "-BackupRoot", str(self.backup_root),
        )
        self.assertEqual(module_version(self.install_root / BUNDLE), "0.9.0")
        forward = list(self.backup_root.glob("forward-*"))
        self.assertEqual(len(forward), 1)
        self.assertEqual(module_version(forward[0] / BUNDLE), "1.0.0")

    def test_rollback_refuses_to_overwrite_a_modified_install(self) -> None:
        old_package = make_package(self.root / "old" / "ModularFXSuite-Windows-VST3.zip", "0.9.0")
        self.assert_installer_ok(*self.install_args(old_package))
        self.assert_installer_ok(*self.install_args(self.actual_package))
        backup = next(self.backup_root.glob("previous-*"))

        module = self.install_root / BUNDLE / "Contents" / "x86_64-win" / "Modular FX Suite.vst3"
        original = module.read_bytes()
        module.write_bytes(original + b"user-change")
        modified = module.read_bytes()
        result = self.assert_installer_rejects(
            "-Rollback",
            "-BackupPath", str(backup),
            "-InstallRoot", str(self.install_root),
            "-BackupRoot", str(self.backup_root),
        )
        self.assertIn("changed since the backup", result.stderr)
        self.assertEqual(module.read_bytes(), modified)

    def test_rejects_tampered_internal_checksum_before_writing(self) -> None:
        bad = make_package(self.root / "bad" / "ModularFXSuite-Windows-VST3.zip", "1.0.0", corrupt_checksum=True)
        result = self.assert_installer_rejects(*self.install_args(bad))
        self.assertIn("SHA-256 mismatch", result.stderr)
        self.assertFalse(self.install_root.exists())

    def test_rejects_duplicate_vst3_class_ids(self) -> None:
        bad = make_package(self.root / "duplicate" / "ModularFXSuite-Windows-VST3.zip", "1.0.0", duplicate_cid=True)
        result = self.assert_installer_rejects(*self.install_args(bad))
        self.assertIn("Duplicate VST3 class ID", result.stderr)
        self.assertFalse(self.install_root.exists())

    def test_rejects_non_amd64_module(self) -> None:
        bad = make_package(self.root / "x86" / "ModularFXSuite-Windows-VST3.zip", "1.0.0", wrong_machine=True)
        result = self.assert_installer_rejects(*self.install_args(bad))
        self.assertIn("AMD64 PE32+", result.stderr)
        self.assertFalse(self.install_root.exists())

    def test_rejects_wrong_product_identity(self) -> None:
        bad = make_package(self.root / "identity" / "ModularFXSuite-Windows-VST3.zip", "1.0.0", wrong_name=True)
        result = self.assert_installer_rejects(*self.install_args(bad))
        self.assertIn("VST3 identity", result.stderr)
        self.assertFalse(self.install_root.exists())

    def test_refuses_to_replace_an_unknown_existing_bundle(self) -> None:
        target = self.install_root / BUNDLE
        target.mkdir(parents=True)
        sentinel = target / "keep-me.txt"
        sentinel.write_text("not a Skelly bundle", encoding="utf-8")
        result = self.assert_installer_rejects(*self.install_args(self.actual_package))
        self.assertTrue(sentinel.is_file())
        self.assertEqual(sentinel.read_text(encoding="utf-8"), "not a Skelly bundle")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--package-path", required=True, type=Path)
    args, remaining = parser.parse_known_args()
    ACTUAL_PACKAGE = args.package_path.resolve()
    if not ACTUAL_PACKAGE.is_file():
        raise SystemExit(f"Package not found: {ACTUAL_PACKAGE}")
    unittest.main(argv=[__file__, *remaining], verbosity=2)
