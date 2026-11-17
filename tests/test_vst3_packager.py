from __future__ import annotations

import json
import struct
import sys
import tempfile
import unittest
import zipfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from tools.package_vst3 import PackageError, package_vst3, strip_trailing_commas


PRODUCT = "Modular FX Suite"
VERSION = "1.0.0"
VENDOR = "SkellAudio"
PROCESSOR_CID = "0123456789ABCDEF0123456789ABCDEF"
CONTROLLER_CID = "FEDCBA9876543210FEDCBA9876543210"


def write_fake_pe64(path: Path, machine: int = 0x8664, optional_magic: int = 0x20B) -> None:
    data = bytearray(512)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", data, 0x84, machine)
    struct.pack_into("<H", data, 0x94, 0xF0)
    struct.pack_into("<H", data, 0x98, optional_magic)
    path.write_bytes(data)


def make_bundle(root: Path, name: str = "Modular FX Suite.vst3", machine: int = 0x8664, magic: int = 0x20B) -> Path:
    bundle = root / name
    info = bundle / "Contents" / "Resources" / "moduleinfo.json"
    binary = bundle / "Contents" / "x86_64-win" / "Modular FX Suite.vst3"
    info.parent.mkdir(parents=True)
    binary.parent.mkdir(parents=True)
    info.write_text(
        json.dumps(
            {
                "Name": PRODUCT,
                "Version": VERSION,
                "Factory Info": {"Vendor": VENDOR, "URL": "", "E-Mail": ""},
                "Classes": [
                    {"CID": PROCESSOR_CID, "Name": PRODUCT, "Category": "Audio Module Class"},
                    {"CID": CONTROLLER_CID, "Name": PRODUCT + " Controller", "Category": "Component Controller Class"},
                ],
                "Description": "quoted ,} text remains literal",
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    write_fake_pe64(binary, machine, magic)
    (bundle / "Contents" / "Resources" / "readme.txt").write_text("resource", encoding="utf-8")
    return bundle


class Vst3PackagerTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.build = self.root / "build"
        self.build.mkdir()
        self.output = self.root / "ModularFXSuite-Windows-VST3.zip"

    def tearDown(self) -> None:
        self.temp.cleanup()

    def package(self) -> dict:
        return package_vst3(self.build, self.output, PRODUCT, VERSION, VENDOR)

    def test_packages_one_flat_windows_vst3_with_provenance_and_hashes(self) -> None:
        make_bundle(self.build)
        manifest = self.package()
        self.assertEqual(manifest["format"], "VST3")
        self.assertEqual(manifest["architecture"], "AMD64")
        self.assertTrue(manifest["preview_only"])
        self.assertEqual(manifest["product"], PRODUCT)
        self.assertEqual(manifest["version"], VERSION)
        self.assertTrue(self.output.is_file())
        self.assertTrue(self.output.with_suffix(".manifest.json").is_file())
        self.assertTrue(self.output.with_suffix(".sha256").is_file())
        with zipfile.ZipFile(self.output) as archive:
            self.assertIsNone(archive.testzip())
            names = archive.namelist()
            self.assertIn("Modular FX Suite.vst3/Contents/x86_64-win/Modular FX Suite.vst3", names)
            self.assertIn("build-info.json", names)
            self.assertIn("PREVIEW-ONLY.txt", names)
            self.assertIn("SHA256SUMS.txt", names)
            self.assertFalse(any(name.endswith(".zip") for name in names))

    def test_normalizes_juce_trailing_commas_only_in_package_copy(self) -> None:
        bundle = make_bundle(self.build)
        info = bundle / "Contents" / "Resources" / "moduleinfo.json"
        original = info.read_bytes()
        value = json.loads(original)
        value["Extra"] = {"Nested": [1, 2, 3]}
        raw = json.dumps(value, indent=2)
        raw = raw.replace('"Category": "Component Controller Class"\n    }', '"Category": "Component Controller Class",\n    }')
        raw = raw.replace('"Description": "quoted ,} text remains literal"\n}', '"Description": "quoted ,} text remains literal",\n}')
        info.write_text(raw, encoding="utf-8")
        before = info.read_bytes()
        manifest = self.package()
        self.assertEqual(info.read_bytes(), before)
        self.assertTrue(manifest["moduleinfo_normalized"])
        with zipfile.ZipFile(self.output) as archive:
            packaged = archive.read("Modular FX Suite.vst3/Contents/Resources/moduleinfo.json")
            json.loads(packaged)
        self.assertNotEqual(original, before)

    def test_refuses_duplicate_bundles(self) -> None:
        make_bundle(self.build, "A.vst3")
        make_bundle(self.build, "B.vst3")
        with self.assertRaisesRegex(PackageError, "exactly one VST3 bundle"):
            self.package()

    def test_rejects_duplicate_class_ids(self) -> None:
        bundle = make_bundle(self.build)
        info = bundle / "Contents" / "Resources" / "moduleinfo.json"
        value = json.loads(info.read_text(encoding="utf-8"))
        value["Classes"][1]["CID"] = value["Classes"][0]["CID"]
        info.write_text(json.dumps(value), encoding="utf-8")
        with self.assertRaisesRegex(PackageError, "Duplicate VST3 class CID"):
            self.package()

    def test_rejects_non_amd64_binary(self) -> None:
        make_bundle(self.build, machine=0x14C)
        with self.assertRaisesRegex(PackageError, "Expected AMD64"):
            self.package()

    def test_rejects_pe32_binary(self) -> None:
        make_bundle(self.build, magic=0x10B)
        with self.assertRaisesRegex(PackageError, r"Expected PE32\+"):
            self.package()

    def test_rejects_wrong_product_or_version(self) -> None:
        bundle = make_bundle(self.build)
        info = bundle / "Contents" / "Resources" / "moduleinfo.json"
        value = json.loads(info.read_text(encoding="utf-8"))
        value["Version"] = "2.0.0"
        info.write_text(json.dumps(value), encoding="utf-8")
        with self.assertRaisesRegex(PackageError, "Expected product version"):
            self.package()

    def test_rejects_duplicate_json_keys(self) -> None:
        bundle = make_bundle(self.build)
        info = bundle / "Contents" / "Resources" / "moduleinfo.json"
        info.write_text('{"Name":"Modular FX Suite","Name":"duplicate"}', encoding="utf-8")
        with self.assertRaisesRegex(PackageError, "Duplicate JSON key"):
            self.package()

    def test_refuses_to_overwrite_existing_archive(self) -> None:
        make_bundle(self.build)
        self.output.write_bytes(b"preserve-existing-preview")
        with self.assertRaisesRegex(PackageError, "Refusing to overwrite"):
            self.package()
        self.assertEqual(self.output.read_bytes(), b"preserve-existing-preview")

    def test_trailing_comma_normalizer_preserves_string_contents(self) -> None:
        source = '{"text":"literal ,} and \\"quote\\"","values":[1,2,],}'
        normalized = strip_trailing_commas(source)
        self.assertEqual(json.loads(normalized)["text"], 'literal ,} and "quote"')
        self.assertEqual(json.loads(normalized)["values"], [1, 2])


if __name__ == "__main__":
    unittest.main()
