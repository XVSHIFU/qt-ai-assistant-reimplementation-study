#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
import tempfile
import unittest
from pathlib import Path


SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))

from release_artifacts import (  # noqa: E402
    generate_metadata,
    publish_staging,
    read_version,
    sha256,
    verify_checksums,
)


class ReleaseFixture:
    def __init__(self, root: Path) -> None:
        self.root = root
        (root / "version.pri").write_text("SMARTKEY_APP_VERSION = 2.3.4\n", encoding="utf-8")
        self.manifest = root / "run_manifest.json"
        self.package = root / "staging"
        self.package.mkdir()
        (self.package / "SmartKeyAI.exe").write_bytes(b"fixture-executable")
        (self.package / "Qt5Core.dll").write_bytes(b"fixture-runtime")
        self.manifest.write_text(json.dumps({
            "run_id": "fixture-run-id",
            "source_hash": "a" * 64,
            "built_at": "2026-08-13T08:00:00Z",
            "exe_sha256": sha256(self.package / "SmartKeyAI.exe"),
            "toolchain": {"qt_version": "5.15.2", "compiler_version": "8.1.0"},
        }), encoding="utf-8")


class ReleaseArtifactTests(unittest.TestCase):
    def make_fixture(self) -> ReleaseFixture:
        temporary = tempfile.TemporaryDirectory(prefix="smartkey-release-")
        self.addCleanup(temporary.cleanup)
        return ReleaseFixture(Path(temporary.name))

    def test_single_version_source_flows_to_release_manifest(self):
        fixture = self.make_fixture()
        result = generate_metadata(
            fixture.root, fixture.package, fixture.manifest,
            "unsigned-development", "NotSigned",
        )
        self.assertEqual("2.3.4", read_version(fixture.root))
        self.assertEqual("2.3.4", result["version"])
        self.assertEqual("SmartKeyAI-2.3.4-win-x86", result["directory_name"])
        self.assertEqual("unsigned-development", result["release_status"])

    def test_checksum_manifest_detects_tampering(self):
        fixture = self.make_fixture()
        generate_metadata(fixture.root, fixture.package, fixture.manifest,
                          "unsigned-development", "NotSigned")
        self.assertEqual([], verify_checksums(fixture.package))
        (fixture.package / "Qt5Core.dll").write_bytes(b"tampered-runtime")
        self.assertTrue(any("Qt5Core.dll" in error for error in verify_checksums(fixture.package)))

    def test_publish_new_version_preserves_old_package(self):
        fixture = self.make_fixture()
        old = fixture.root / "dist" / "SmartKeyAI-2.3.3-win-x86"
        old.mkdir(parents=True)
        (old / "marker.txt").write_text("old-release", encoding="utf-8")
        final = fixture.root / "dist" / "SmartKeyAI-2.3.4-win-x86"
        publish_staging(fixture.package, final)
        self.assertTrue((final / "SmartKeyAI.exe").is_file())
        self.assertEqual("old-release", (old / "marker.txt").read_text(encoding="utf-8"))

    def test_failed_publish_never_replaces_existing_release(self):
        fixture = self.make_fixture()
        final = fixture.root / "dist" / "SmartKeyAI-2.3.4-win-x86"
        final.mkdir(parents=True)
        (final / "marker.txt").write_text("known-good", encoding="utf-8")
        with self.assertRaises(FileExistsError):
            publish_staging(fixture.package, final)
        self.assertEqual("known-good", (final / "marker.txt").read_text(encoding="utf-8"))
        self.assertTrue((fixture.package / "SmartKeyAI.exe").is_file())


if __name__ == "__main__":
    unittest.main(verbosity=2)
