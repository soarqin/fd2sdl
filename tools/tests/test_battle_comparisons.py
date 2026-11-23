"""战场菜单和自然开场对照的离线回归。图像和存档均在测试内构造。"""
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import wave

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
try:
    from PIL import Image
except ModuleNotFoundError:
    Image = None


@unittest.skipUnless(Image is not None, "WGC 图像对照工具需要 Pillow")
class BattleComparisonTest(unittest.TestCase):
    def test_browse_requires_the_visible_lower_actor_in_its_saved_direction(self):
        from run_original_battle_menu import fixture_oracle
        from run_original_end_turn import META, STRIDE, UNITS

        palette = [(index * 31) % 256 for index in range(256) for _ in range(3)]
        cursor = Image.new("P", (24, 24), 250)
        cursor.putpalette(palette)
        marker = Image.new("P", (1, 1), 249)
        marker.putpalette(palette)
        assets = {"palette": palette, "cursor": cursor, "commands": [marker] * 54}
        source = bytearray(META + 18)
        source[META + 1] = 2
        for raw, identity, flags in ((0, 1, 1), (1, 4, 0)):
            start = UNITS + raw * STRIDE
            source[start:start + 8] = bytes((8, 16, 0, 1, 0, flags, 2, identity))

        # 每个 FDICON 帧用独立的实心色，预期画面直接画矩形。
        count = 1680
        frames = [bytes((23, i % 63 + 1)) * 24 for i in range(count)]
        start = 6 + count * 4
        offsets = [start + i * len(frames[0]) for i in range(count)]
        focus = (76, 100)
        with tempfile.TemporaryDirectory() as temporary:
            original = Path(temporary)
            (original / "FDICON.B24").write_bytes(
                struct.pack("<3H", 24, 24, count) + struct.pack(f"<{count}I", *offsets) + b"".join(frames))
            oracle, actor = fixture_oracle(original, source, assets, focus, (8, 15))
            self.assertEqual(actor, {"raw_actor": 1, "unit_id": 4, "direction": 1})
            bare = Image.new("RGB", (320, 200))
            bare.paste(cursor.convert("RGB"), focus)

            def with_actor(phase):
                result = bare.copy()
                color = ((4 * 12 + phase) % 63 + 1) * 31 % 256
                result.paste((color,) * 3, (76, 118, 100, 142))
                return result

            composite = with_actor(4)
            detail = oracle.browse(composite)
            self.assertEqual(detail["lower_actor_frame"], 4)
            self.assertEqual(detail["cursor"]["checked_pixels"], 24 * 42)
            self.assertIsNone(oracle.browse(bare))
            self.assertIsNone(oracle.browse(with_actor(1)))
            composite.putpixel((95, 137), (255, 255, 255))
            self.assertIsNone(oracle.browse(composite))

            source[UNITS + STRIDE + 5] = 1
            empty, actor = fixture_oracle(original, source, assets, focus, (8, 15))
            self.assertIsNone(actor)
            self.assertTrue(empty.browse(bare))

    def test_native_recording_checks_the_configured_rate(self):
        from run_original_transition import collect_native_audio

        with tempfile.TemporaryDirectory() as temporary:
            work = Path(temporary)
            (work / "captures").mkdir()
            path = work / "captures" / "fd2.wav"
            with wave.open(str(path), "wb") as recording:
                recording.setparams((2, 2, 48000, 0, "NONE", ""))
                recording.writeframes(bytes((1, 0)) * 48)
            case = {"native_audio": {"enabled": True, "stopped": True}, "business_sequence_completed": True}
            collect_native_audio(case, work, expected_rate=48000)
            self.assertEqual(case["native_audio"]["files"][0]["sample_rate"], 48000)
            with self.assertRaisesRegex(AssertionError, "44100"):
                collect_native_audio(case, work)

    def test_opening_title_and_load_use_the_pre_start_snapshot(self):
        from run_original_opening import META, load_menu_availability, title_menu_counts

        source = bytearray(META + 18)
        source[META + 1] = 27
        source[META + 2] = 29
        source[META + 9] = 31
        self.assertEqual(title_menu_counts(source), {"original": 3, "sdl": 3})
        self.assertEqual(load_menu_availability(source), {"original": True, "sdl": True})
        source[META + 2] = 0xff
        self.assertEqual(title_menu_counts(source), {"original": 2, "sdl": 2})
        self.assertEqual(load_menu_availability(source), {"original": True, "sdl": False})
