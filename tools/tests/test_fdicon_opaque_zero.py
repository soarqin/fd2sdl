import struct
import unittest
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from audit_fdicon_opaque_zero import audit_fdicon


def fixture(first_row):
    return struct.pack("<HHHI", 24, 24, 1, 10) + first_row + bytes([0xd7]) * 23


class OpaqueZeroTest(unittest.TestCase):
    def test_skip_is_not_opaque_zero(self):
        result = audit_fdicon(fixture(bytes([0xd7])), expected_frames=1)
        self.assertEqual(result["opaque_zero_pixels"], 0)
        self.assertEqual(result["counts"]["explicit_skip_pixels"], 576)
        self.assertEqual(result["counts"]["written_pixels"], 0)

    def test_each_writing_mode_detects_zero_separately_from_skip(self):
        for row, label, count in ((bytes([0x17, 0]), "repeat_zero_pixels", 24),
                                  (bytes([0x4b, 0]), "alternating_zero_pixels", 12),
                                  (bytes([0x97, *range(24)]), "literal_zero_pixels", 1)):
            with self.subTest(label=label):
                result = audit_fdicon(fixture(row), expected_frames=1)
                self.assertEqual(result["opaque_zero_pixels"], count)
                self.assertEqual(result["counts"][label], count)
                self.assertEqual(result["frames_with_opaque_zero"], [0])
                if label == "alternating_zero_pixels":
                    self.assertEqual(result["counts"]["alternating_skip_pixels"], 12)
                    self.assertEqual(result["examples"][0]["x"], 1)

    def test_rejects_truncation_cross_row_and_unused_tail(self):
        good = fixture(bytes([0xd7]))
        for data in (good[:5], good[:-1], fixture(bytes([0xd8])), good + b"\0",
                     struct.pack("<HHHI", 24, 24, 1, 9) + bytes([0xd7]) * 24):
            with self.subTest(data=data[:12]), self.assertRaises(ValueError):
                audit_fdicon(data, expected_frames=1)

    def test_actual_fdicon_all_frames_have_only_skip_zero(self):
        result = audit_fdicon((ROOT / "original_game/FDICON.B24").read_bytes())
        self.assertEqual(result["validated_frames"], 1680)
        self.assertEqual(result["opaque_zero_pixels"], 0)
        total = sum(result["counts"][key] for key in ("written_pixels", "explicit_skip_pixels", "alternating_skip_pixels"))
        self.assertEqual(total, 1680 * 24 * 24)


if __name__ == "__main__":
    unittest.main()
