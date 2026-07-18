#!/usr/bin/env python3
"""Regression tests for fail-closed Ghidra release identity checks."""

import hashlib
import sys
import tempfile
import unittest
from pathlib import Path
from unittest import mock

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import rebuild_fd2_ghidra as rebuild  # noqa: E402


class GhidraIdentityTest(unittest.TestCase):
    def make_install(self, root: Path, version: str = "11.3.2",
                     release: str = "PUBLIC") -> tuple[Path, Path]:
        headless = root / "support" / "analyzeHeadless"
        headless.parent.mkdir(parents=True)
        headless.write_text("#!/bin/sh\n", encoding="ascii")
        properties = root / "Ghidra" / "application.properties"
        properties.parent.mkdir(parents=True)
        properties.write_text(
            f"application.version={version}\n"
            f"application.release.name={release}\n",
            encoding="ascii")
        archive = root.parent / "ghidra.zip"
        archive.write_bytes(b"fixture")
        return headless, archive

    def test_accepts_matching_version_release_and_hash(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            headless, archive = self.make_install(Path(tmp) / "install")
            digest = hashlib.sha256(b"fixture").hexdigest()
            with mock.patch.object(rebuild, "EXPECTED_GHIDRA_RELEASE_SHA256",
                                   digest):
                rebuild.verify_ghidra_install(headless, archive)

    def test_rejects_wrong_version(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            headless, archive = self.make_install(
                Path(tmp) / "install", version="11.3.1")
            with self.assertRaisesRegex(RuntimeError, "unexpected Ghidra version"):
                rebuild.verify_ghidra_install(headless, archive)

    def test_rejects_wrong_archive_hash(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            headless, archive = self.make_install(Path(tmp) / "install")
            with self.assertRaisesRegex(RuntimeError, "SHA-256 mismatch"):
                rebuild.verify_ghidra_install(headless, archive)


if __name__ == "__main__":
    unittest.main()
