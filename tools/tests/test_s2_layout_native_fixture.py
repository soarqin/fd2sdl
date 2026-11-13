"""S2 layout夹具：精确写域、说话人和owner跨stage拒绝；无窗口。"""
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch
import ast
import tempfile
from types import SimpleNamespace

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT / "tools"), str(ROOT / "tools/static_data")]
import s2_layout_native_fixture as fixture
import run_s2_layout_check as runner


def source():
    data = bytearray(0x59cb)
    data[fixture.META + 1:fixture.META + 3] = bytes((12, 0)); data[fixture.META + 9] = 4
    for raw in range(12):
        at = fixture.UNITS + raw * 80
        data[at:at + 9] = bytes((7, 14, 0, 0, 0, 0, 2 if raw < 4 else 0, raw, raw))
        struct.pack_into("<HH", data, at + 64, 42, 42)
    # START保留的两个实际说话人来自raw0和raw2；其余由显式copy/ID覆写提供。
    data[fixture.UNITS + 2 * 80 + 7:fixture.UNITS + 2 * 80 + 9] = bytes((4, 4))
    data[-4:] = sum(data[:-4]).to_bytes(4, "little")
    return bytes(data)


class LayoutFixtureTests(unittest.TestCase):
    def test_explicit_copies_speakers_and_exact_write_boundary(self):
        original = source(); metadata = bytes((2, 6, 40)) + bytes(0x83 + 40 * 26 - 3)
        plain, encoded, manifest = fixture.derive(original, metadata)
        self.assertEqual(len(encoded), len(original)); self.assertEqual(sum(plain[:-4]), int.from_bytes(plain[-4:], "little"))
        self.assertEqual(plain[fixture.META + 1:fixture.META + 3], bytes((15, 2)))
        self.assertEqual({int(identity) for identity in manifest["speakers"]}, {0, 1, 2, 4, 8})
        self.assertEqual([raw for raw in range(15) if not plain[fixture.UNITS + raw * 80 + 5] & 1], [0, 6])
        self.assertEqual(plain[0x8a3:fixture.UNITS], original[0x8a3:fixture.UNITS])
        self.assertEqual(plain[fixture.UNITS + 15 * 80:fixture.META + 1], original[fixture.UNITS + 15 * 80:fixture.META + 1])
        self.assertEqual(plain[fixture.META + 3:fixture.META + 9], bytes((2, 18, 8, 22, 6, 4)))
        self.assertEqual(plain[fixture.META + 9:-4], original[fixture.META + 9:-4])
        self.assertFalse(manifest["natural_history_claimed"]); self.assertFalse(manifest["new_runtime_binding"])

    def test_wrong_source_checksum_metadata_and_hp_rejected(self):
        metadata = bytes((2, 6, 40)) + bytes(0x83 + 40 * 26 - 3)
        bad = bytearray(source()); bad[3000] ^= 1
        with self.assertRaisesRegex(ValueError, "checksum"): fixture.derive(bad, metadata)
        with self.assertRaisesRegex(ValueError, "metadata"): fixture.derive(source(), b"\0" * len(metadata))
        bad = bytearray(source()); bad[fixture.UNITS + 64:fixture.UNITS + 66] = b"\0\0"
        bad[-4:] = sum(bad[:-4]).to_bytes(4, "little")
        with self.assertRaisesRegex(ValueError, "正HP"): fixture.derive(bad, metadata)

    def test_post_stage_change_rejected_before_owner_dereference(self):
        with patch.object(fixture.result_fixture, "original_owner", return_value=({"stage": 3, "count": 15}, {})):
            with self.assertRaisesRegex(ValueError, "禁止读取"): fixture.original_owner(b"", ROOT, {})

    def test_layout_prefix_rejects_wrong_direction_and_short_read(self):
        records = bytearray(15 * 80); records[:4] = bytes((8, 3, 0, 2))
        manifest = {"layout_source": {"expected_prefix": [{"raw": 0, "x": 8, "y": 3, "direction": 2}]}}
        state = {"stage": 2, "count": 15, "records_hex": records.hex(), "camera": [2, 0], "focus": [2, 0]}
        self.assertEqual(fixture.validate_layout_prefix(state, manifest)["complete_acted_count"], 15)
        records[3] = 3; state["records_hex"] = records.hex()
        with self.assertRaisesRegex(ValueError, "方向"): fixture.validate_layout_prefix(state, manifest)
        state["records_hex"] = "00"
        with self.assertRaisesRegex(ValueError, "长度"): fixture.validate_layout_prefix(state, manifest)

    def test_real_text_all_wait_resources_decodable(self):
        prepared = fixture.prepare(ROOT / "original_game")
        self.assertEqual(prepared["manifest"]["text_source"]["token_count"], 332)
        self.assertEqual(len(prepared["pages"]), 12)
        self.assertEqual(prepared["pages"][-1]["reason"], "fragment end")
        self.assertEqual(prepared["pages"][-1]["speaker_argument"], 1)
        self.assertTrue(all(page["known_text_pixels"] >= 256 and len(page["portraits"]) == 2 for page in prepared["pages"]))

    def test_runner_wait_log_uses_actual_three_argument_interface(self):
        calls = [node for node in ast.walk(ast.parse(Path(runner.__file__).read_text(encoding="utf8")))
                 if isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and node.func.id == "wait_log"]
        self.assertEqual(len(calls), 2)
        self.assertTrue(all(len(call.args) == 3 and isinstance(call.args[0], ast.Attribute) and call.args[0].attr == "process" for call in calls))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "game.log"; path.write_text("S2 layout source ready:\n", encoding="utf8")
            self.assertIn("source ready:", runner.wait_log(SimpleNamespace(poll=lambda: None), path, "S2 layout source ready:", timeout=0.1))

    def test_fade_requires_bright_dark_restore_and_preserves_hash(self):
        from PIL import Image
        with tempfile.TemporaryDirectory() as directory:
            frames = []
            for i, value in enumerate((100, 50, 0, 2, 120)):
                path = Path(directory) / f"frame{i}.png"; Image.new("RGB", (320, 200), (value,) * 3).save(path)
                frames.append({"path": str(path), "sha256": runner.sha(path), "index": i, "wgc_timestamp_100ns": i + 1})
            self.assertEqual(runner.fade_gate(frames)["exact_black_frames"], [2])
            with self.assertRaisesRegex(AssertionError, "恢复"): runner.fade_gate(frames[:4])
            with self.assertRaisesRegex(AssertionError, "恢复"): runner.fade_gate(frames[2:])
            frames[2]["sha256"] = "0" * 64
            with self.assertRaisesRegex(AssertionError, "hash"): runner.fade_gate(frames)

    def test_original_completion_accepts_only_source_prefix_order_and_bounds_wait(self):
        for stage, roster in ((2, 4), (2, 5)):
            self.assertFalse(runner.completion_status({"stage": stage, "roster_count": roster}))
        self.assertTrue(runner.completion_status({"stage": 3, "roster_count": 5}))
        for stage, roster in ((3, 4), (4, 5), (2, 3)):
            with self.subTest(stage=stage, roster=roster), self.assertRaisesRegex(AssertionError, "写序"):
                runner.completion_status({"stage": stage, "roster_count": roster})
        case = {}
        with patch.object(runner, "observe", side_effect=[{"stage": 2, "roster_count": 4}, {"stage": 2, "roster_count": 5}, {"stage": 3, "roster_count": 5}]) as observed:
            runner.wait_original_completion(None, None, None, None, None, case)
            self.assertEqual(observed.call_count, 3); self.assertFalse(case["static_completion_wait"]["owner_dereferenced"])
        with patch.object(runner, "observe", return_value={"stage": 2, "roster_count": 4}) as observed:
            with self.assertRaisesRegex(AssertionError, "耗尽"): runner.wait_original_completion(None, None, None, None, None, {})
            self.assertEqual(observed.call_count, 12)
        with patch.object(runner, "observe", side_effect=ValueError("unsafe owner")) as observed:
            with self.assertRaisesRegex(ValueError, "unsafe owner"): runner.wait_original_completion(None, None, None, None, None, {})
            self.assertEqual(observed.call_count, 1)


if __name__ == "__main__": unittest.main()
