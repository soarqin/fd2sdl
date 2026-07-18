#!/usr/bin/env python3
"""Regression tests for the canonical FD2 Ghidra input bundle."""

import hashlib
import json
import sys
import tempfile
import unittest
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
ROOT = TOOLS.parent
sys.path.insert(0, str(TOOLS))

from fd2_analysis_bundle import build_bundle  # noqa: E402


class CanonicalBundleTest(unittest.TestCase):
    def test_real_fd2_bundle_is_canonical_and_deterministic(self) -> None:
        exe = ROOT / "original_game" / "FD2.EXE"
        if not exe.exists():
            self.skipTest("original_game/FD2.EXE is developer-local")
        with tempfile.TemporaryDirectory() as first_tmp, tempfile.TemporaryDirectory() as second_tmp:
            first = Path(first_tmp)
            second = Path(second_tmp)
            manifest_a = build_bundle(exe, first)
            manifest_b = build_bundle(exe, second)
            self.assertEqual(manifest_a, manifest_b)
            self.assertEqual((first / "manifest.json").read_bytes(),
                             (second / "manifest.json").read_bytes())
            self.assertEqual((first / "SHA256SUMS").read_bytes(),
                             (second / "SHA256SUMS").read_bytes())

            manifest = json.loads((first / "manifest.json").read_text())
            self.assertEqual(manifest["schema"], "fd2-ghidra-bundle-v1")
            self.assertEqual(manifest["le"]["entry_va"], 0x3CCB4)
            self.assertEqual(
                [(row["object"], row["relbase"], row["initialized_size"],
                  row["bss_size"], row["permissions"])
                 for row in manifest["objects"]],
                [(1, 0x10000, 0x3EF29, 0, "r-x"),
                 (2, 0x50000, 0x4000, 0x16B0, "rw-"),
                 (3, 0x60000, 0x34D2, 0, "rw-")])
            summary = manifest["relocation_summary"]
            self.assertEqual(summary["record_count"], 7959)
            self.assertEqual(summary["unique_source_count"], 7948)
            self.assertEqual(summary["duplicate_record_count"], 11)
            self.assertEqual(summary["source_type_0x07_flags_0x00_count"], 7067)
            self.assertEqual(summary["source_type_0x07_flags_0x10_count"], 892)
            self.assertEqual(
                summary["raw_relbase_sha256"],
                "37bceab29c9ee9089b5c5f9c4abefdee26a0bb34ae47bb917783dcc23ee30fc2")
            self.assertEqual(
                summary["relocated_relbase_sha256"],
                "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1")

            checksums = {}
            for line in (first / "SHA256SUMS").read_text().splitlines():
                digest, name = line.split("  ", 1)
                checksums[name] = digest
            for name, digest in checksums.items():
                self.assertEqual(hashlib.sha256((first / name).read_bytes()).hexdigest(), digest)


if __name__ == "__main__":
    unittest.main()
