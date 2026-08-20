#!/usr/bin/env python3
from __future__ import annotations

import copy
import json
import subprocess
import sys
import tempfile
import unittest
from datetime import datetime, timedelta, timezone
from pathlib import Path


SCRIPTS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(SCRIPTS))

from artifact_provenance import file_sha256, source_tree_hash, utc_iso, validate_provenance  # noqa: E402


NOW = datetime(2026, 8, 13, 8, 0, tzinfo=timezone.utc)


class ProvenanceFixture:
    def __init__(self, root: Path) -> None:
        self.root = root
        self.reports = root / "reports"
        self.build = root / "build" / "release" / "SmartKey.exe"
        self.package = root / "dist" / "SmartKey" / "SmartKeyAI.exe"
        self.reports.mkdir(parents=True)
        self.build.parent.mkdir(parents=True)
        self.package.parent.mkdir(parents=True)
        (root / "source.txt").write_text("source-v1\n", encoding="utf-8")
        self.build.write_bytes(b"current-build-artifact")
        self.package.write_bytes(self.build.read_bytes())
        self.run_id = "20260813T075500000Z-0123456789abcdef0123456789abcdef"
        self.source_hash = source_tree_hash(root)
        self.exe_hash = file_sha256(self.build)
        built = NOW - timedelta(minutes=5)
        common = {
            "run_id": self.run_id,
            "source_hash": self.source_hash,
            "built_at": utc_iso(built),
        }
        self.documents = {
            "run_manifest.json": {
                **common, "schema_version": 1, "exe_sha256": self.exe_hash,
                "toolchain": {"qt": "5.15.2"}, "environment": {"os": "fixture"},
            },
            "build_verification.json": {
                **common, "status": "PASS", "exe_sha256": self.exe_hash,
                "executable": str(self.build),
            },
            "module_test_results.json": {
                **common, "status": "PASS", "tested_at": utc_iso(built + timedelta(minutes=1)),
                "exe_sha256": self.exe_hash, "environment": {"platform": "fixture"},
            },
            "qml_engine_validation.json": {
                **common, "status": "PASS", "tested_at": utc_iso(built + timedelta(minutes=2)),
                "exe_sha256": self.exe_hash, "environment": {"platform": "fixture"},
            },
            "package_verification.json": {
                **common, "status": "PASS", "packaged_at": utc_iso(built + timedelta(minutes=2)),
                "tested_at": utc_iso(built + timedelta(minutes=3)),
                "build_exe_sha256": self.exe_hash, "package_sha256": self.exe_hash,
                "package_executable": str(self.package), "environment": {"platform": "fixture"},
            },
            "live_deepseek_validation.json": {
                **common, "status": "PASS", "tested_at": utc_iso(built + timedelta(minutes=4)),
                "exe_sha256": self.exe_hash, "opt_in": True,
            },
        }
        self.write()

    def write(self) -> None:
        for name, document in self.documents.items():
            (self.reports / name).write_text(json.dumps(document), encoding="utf-8")

    def validate(self, **kwargs):
        return validate_provenance(self.root, now=NOW, **kwargs)


class ArtifactProvenanceTests(unittest.TestCase):
    def make_fixture(self):
        temporary = tempfile.TemporaryDirectory(prefix="smartkey-provenance-")
        self.addCleanup(temporary.cleanup)
        return ProvenanceFixture(Path(temporary.name))

    def test_current_consistent_run_passes(self):
        fixture = self.make_fixture()
        result = fixture.validate()
        self.assertEqual("PASS", result["status"], result["errors"])
        self.assertFalse(result["live"]["included"])

    def test_source_hash_is_deterministic_and_ignores_artifact_directories(self):
        fixture = self.make_fixture()
        first = source_tree_hash(fixture.root)
        (fixture.root / "reports" / "new-report.json").write_text("{}", encoding="utf-8")
        (fixture.root / "build_agent_other").mkdir()
        (fixture.root / "build_agent_other" / "temporary.o").write_bytes(b"ignored")
        second = source_tree_hash(fixture.root)
        self.assertEqual(first, second)

    def test_stale_reports_fail(self):
        fixture = self.make_fixture()
        result = fixture.validate(max_age_hours=0.01)
        self.assertEqual("FAIL", result["status"])
        self.assertFalse(result["checks"]["build_report_fresh"])

    def test_replaced_package_executable_fails(self):
        fixture = self.make_fixture()
        fixture.package.write_bytes(b"different-package-executable")
        result = fixture.validate()
        self.assertEqual("FAIL", result["status"])
        self.assertFalse(result["checks"]["package_executable_hash_matches"])

    def test_changed_source_tree_fails(self):
        fixture = self.make_fixture()
        (fixture.root / "source.txt").write_text("source-v2\n", encoding="utf-8")
        result = fixture.validate()
        self.assertEqual("FAIL", result["status"])
        self.assertFalse(result["checks"]["source_tree_matches_build"])

    def test_reversed_time_fails(self):
        fixture = self.make_fixture()
        fixture.documents["module_test_results.json"]["tested_at"] = utc_iso(NOW - timedelta(minutes=10))
        fixture.write()
        result = fixture.validate()
        self.assertEqual("FAIL", result["status"])
        self.assertFalse(result["checks"]["time_order_valid"])

    def test_run_or_hash_mismatch_fails(self):
        fixture = self.make_fixture()
        fixture.documents["qml_engine_validation.json"]["run_id"] = "another-run"
        fixture.write()
        result = fixture.validate()
        self.assertEqual("FAIL", result["status"])
        self.assertFalse(result["checks"]["reports_inherit_build_identity"])

    def test_old_live_report_fails_only_when_opted_in(self):
        fixture = self.make_fixture()
        fixture.documents["live_deepseek_validation.json"]["tested_at"] = utc_iso(NOW - timedelta(days=2))
        fixture.write()
        self.assertEqual("PASS", fixture.validate()["status"])
        required = fixture.validate(require_live=True)
        self.assertEqual("FAIL", required["status"])
        self.assertFalse(required["checks"]["live_vendor_smoke_current_and_bound"])

    def test_verify_delivery_provenance_only_cli_rejects_changed_source(self):
        fixture = self.make_fixture()
        (fixture.root / "source.txt").write_text("changed-after-build\n", encoding="utf-8")
        process = subprocess.run(
            [sys.executable, str(SCRIPTS / "verify_delivery.py"),
             "--project", str(fixture.root), "--provenance-only", "--now", utc_iso(NOW)],
            check=False, capture_output=True, text=True,
        )
        self.assertEqual(1, process.returncode, process.stdout + process.stderr)
        document = json.loads(process.stdout)
        self.assertFalse(document["checks"]["source_tree_matches_build"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
