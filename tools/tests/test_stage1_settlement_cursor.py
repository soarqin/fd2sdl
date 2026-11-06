"""B7 结算光标格的正反模型；不启动窗口或更改原资源。"""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import verify_stage1_settlement_cursor as v

ROOT = Path(__file__).resolve().parents[2]


class SettlementCursorTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.templates, cls.tile, cls.attrs = v.resource_templates(ROOT / "original_game")

    def test_no_cursor_accepts_three_independent_idle_phases(self):
        self.assertEqual(len(self.templates), 6)
        self.assertEqual((self.tile, self.attrs), (73, [0, 0, 10, 0]))
        for _, phase, image in self.templates[:3]:
            result = v.compare_roi(image, self.templates)
            self.assertTrue(result["unique_no_cursor_family"])
            self.assertEqual(result["accepted_cursor_families"], [False])
            self.assertEqual(result["matches"][phase]["checked_pixels"], 576)

    def test_cursor_is_positive_counterexample_not_an_idle_acceptance(self):
        for _, _, image in self.templates[3:]:
            result = v.compare_roi(image, self.templates)
            self.assertFalse(result["unique_no_cursor_family"])
            self.assertEqual(result["accepted_cursor_families"], [True])

    def test_bad_pixel_and_wrong_geometry_reject_without_smaller_roi(self):
        image = self.templates[0][2].copy()
        old = image.getpixel((0, 23))
        image.putpixel((0, 23), tuple(255 if value < 128 else 0 for value in old))
        self.assertFalse(v.compare_roi(image, self.templates)["unique_no_cursor_family"])
        with self.assertRaises(ValueError):
            v.compare_roi(image.crop((0, 0, 23, 24)), self.templates)
        plain = bytearray(0x30cc); plain[0x30c4:0x30c6] = bytes((7, 1))
        unit = memoryview(plain)[0x12a3 + 6 * 80:0x12a3 + 7 * 80]
        unit[:2] = bytes((15, 13)); unit[7:9] = bytes((133, 133))
        plain[0x30c6:0x30ca] = bytes((9, 8, 20, 14))
        report = {"cases": [{"kind": role, "checkpoints": [
            {"name": "b7-hero-focus", "camera": [9, 8], "world": [20, 14]},
            {"name": "b7-settlement-00", "fragment": 6, "speaker_control": -19,
             "speaker_argument": 6, "portrait_id": 133}]} for role in ("original", "sdl")]}
        self.assertEqual(v.validate_geometry(plain, report)[1]["camera"], [9, 8])
        unit[0] = 14
        with self.assertRaises(ValueError):
            v.validate_geometry(plain, report)
        unit[0] = 15; plain[0x12a3:0x12a5] = bytes((15, 14))
        with self.assertRaises(ValueError):
            v.validate_geometry(plain, report)

    def test_output_is_exclusive_resource_protected_and_late_guard_detects_drift(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder); original = root / "original_game"; original.mkdir()
            forbidden = original / "new" / "receipt.json"
            with patch.object(v, "verify") as verify:
                with self.assertRaises(ValueError):
                    v.main(["--root", str(root), "--artifacts", str(root), "--output", str(forbidden)])
                verify.assert_not_called()
            self.assertFalse(forbidden.parent.exists())
            source = root / "source.bin"; source.write_bytes(b"before")
            before = [v.fact(source)]; self.assertTrue(v.unchanged(before))
            source.write_bytes(b"after!"); self.assertFalse(v.unchanged(before))
            output = root / "new.json"
            with patch.object(v, "verify", return_value={"passed": True, "guards": True}):
                self.assertEqual(v.main(["--root", str(root), "--artifacts", str(root), "--output", str(output)]), 0)
                saved = output.read_bytes()
                with self.assertRaises(FileExistsError):
                    v.main(["--root", str(root), "--artifacts", str(root), "--output", str(output)])
                self.assertEqual(output.read_bytes(), saved)


if __name__ == "__main__":
    unittest.main()
