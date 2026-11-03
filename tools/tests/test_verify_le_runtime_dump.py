"""Runtime verifier regression tests using small synthetic objects, not DOSBox."""
import argparse
import contextlib
import io
import struct
import sys
import tempfile
import unittest
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import verify_le_runtime_dump as verifier  # noqa: E402
from rebuild_fd2_analysis import FixupRecord, ObjectRecord  # noqa: E402


class RuntimeDumpVerifierTest(unittest.TestCase):
    def run_verifier(self, mutate=None):
        objects = [ObjectRecord(1, 4, 0, 0, 1, 1),
                   ObjectRecord(2, 8, 4, 0, 2, 1),
                   ObjectRecord(3, 4, 12, 0, 3, 1)]
        raw = bytes(4) + b"DATA" + bytes(4) + b"MORE"
        fixups = [FixupRecord(0, 0, 0, 0, 7, 0, 3, 0, 12)]
        runtime = bytearray(40)
        struct.pack_into("<I", runtime, 16, 36)
        runtime[24:28] = b"DATA"
        runtime[36:40] = b"MORE"
        if mutate:
            mutate(runtime)
        output = io.StringIO()
        with tempfile.TemporaryDirectory() as directory:
            dump = Path(directory) / "dump.bin"
            exe = Path(directory) / "fixture.exe"
            dump.write_bytes(runtime)
            exe.write_bytes(b"fixture")
            argv = ["verify", str(dump), "--exe", str(exe),
                    "--object1-base", "16", "--object2-base", "24",
                    "--object3-base", "36"]
            with patch.object(sys, "argv", argv), \
                 patch.object(verifier, "parse_le", return_value=SimpleNamespace(page_size=4)), \
                 patch.object(verifier, "parse_objects", return_value=objects), \
                 patch.object(verifier, "build_relbase_image", return_value=raw), \
                 patch.object(verifier, "parse_fixups", return_value=fixups), \
                 contextlib.redirect_stdout(output):
                result = verifier.main()
        return result, output.getvalue()

    def test_negative_base_is_rejected(self):
        self.assertEqual(verifier.parse_base("0x10"), 16)
        with self.assertRaises(argparse.ArgumentTypeError):
            verifier.parse_base("-1")

    def test_matching_full_objects_pass(self):
        result, output = self.run_verifier()
        self.assertEqual(result, 0, output)
        self.assertIn("runtime_dump_validation=PASS", output)

    def test_modified_bss_fails_instead_of_reporting_pass(self):
        result, output = self.run_verifier(lambda data: data.__setitem__(28, 1))
        self.assertEqual(result, 1, output)
        self.assertIn("BSS byte mismatch", output)
        self.assertNotIn("runtime_dump_validation=PASS", output)

    def test_file_byte_and_relocation_corruption_fail(self):
        for offset in (16, 24, 36):
            with self.subTest(offset=offset):
                result, output = self.run_verifier(
                    lambda data: data.__setitem__(offset, data[offset] ^ 1))
                self.assertEqual(result, 1, output)

    def test_short_dump_fails(self):
        result, output = self.run_verifier(lambda data: data.clear())
        self.assertEqual(result, 1, output)


if __name__ == "__main__":
    unittest.main()
