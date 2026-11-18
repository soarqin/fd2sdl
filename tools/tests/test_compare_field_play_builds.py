"""SDL 新旧程序对照的文件保护、日志判据和 WGC 位图行对齐回归。"""
import contextlib
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import compare_field_play_builds as compare


class BuildComparisonTests(unittest.TestCase):
    def setUp(self):
        temporary = tempfile.TemporaryDirectory()
        self.addCleanup(temporary.cleanup)
        self.root = Path(temporary.name)
        self.output = self.root / "artifacts"

    def invoke(self, runner):
        args = ["compare", "--root", str(self.root), "--old-game", "old",
                "--new-game", "new", "--driver", "driver", "--artifacts",
                str(self.output), "--scenario", "cancel"]
        with patch.object(sys, "argv", args), patch.object(compare, "run", runner), \
                contextlib.redirect_stdout(io.StringIO()), contextlib.redirect_stderr(io.StringIO()):
            return compare.main()

    def test_unstable_baseline_fails_even_when_new_matches_first_run(self):
        def run(args, game, label, name, spec):
            directory = args.artifacts / name / label
            directory.mkdir(parents=True)
            # splitlines 会将两份日志显示为相同的一行。
            data = b"ok\r\n" if label == "old2" else b"ok\n"
            (directory / "game.log").write_bytes(data)
            return directory, {"trace": ["ok"], "save": None, "captures": [],
                               "exit_code": 0, "forced": False, "error": None}

        self.assertEqual(self.invoke(run), 1)
        summary = json.loads((self.output / "summary.json").read_text())["cancel"]
        self.assertTrue(summary["old_vs_new"]["trace_equal"])
        self.assertFalse(summary["old_vs_old"]["trace_equal"])
        self.assertEqual(summary["old_vs_old"]["first_trace_diff"]["line"], 0)
        self.assertFalse(summary["passed"])

    def test_protect_existing_outputs_and_original_resources(self):
        self.output.mkdir()
        sentinel = self.output / "summary.json"
        sentinel.write_bytes(b"KEEP")
        with self.assertRaises(SystemExit):
            self.invoke(lambda *args: self.fail("launched with occupied output"))
        self.assertEqual(sentinel.read_bytes(), b"KEEP")
        self.output = self.root / "original_game" / "new"
        with self.assertRaises(SystemExit):
            self.invoke(lambda *args: self.fail("launched inside original_game"))
        self.assertFalse(self.output.exists())

    def test_pixel_count_excludes_padding_and_respects_row_order(self):
        def bitmap(name, height, rows):
            header = bytearray(54)
            header[:2] = b"BM"
            struct.pack_into("<I", header, 10, 54)
            struct.pack_into("<IiiHH", header, 14, 40, 1, height, 1, 24)
            path = self.root / name
            path.write_bytes(header + rows)
            return path

        a = bitmap("a.bmp", 2, b"abc0def0")
        b = bitmap("b.bmp", -2, b"def1abc1")
        self.assertEqual(compare.pixel_diff(a, b), 0)
        c = bitmap("c.bmp", -2, b"def1xyz1")
        self.assertEqual(compare.pixel_diff(a, c), 1)


if __name__ == "__main__":
    unittest.main()
