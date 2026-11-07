"""129ec/12ac6 静止角色前景：原资源合成纯测试，不作为原版运行证据。"""
from pathlib import Path
import struct
import sys
import unittest

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

import fd2_natural_oracle as oracle
from fd2_battle_animation_frames import rle_material
from run_graphics_tests import decode_save
from run_original_comparison import archive_entry
from run_original_end_turn import META, STRIDE, UNITS, round_assets
from run_original_magic_list import actor_focus_resources
from run_original_player import acted_sprite, screen, sheet_frame


class ForegroundTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original = ROOT / "original_game"
        cls.assets = round_assets(cls.original)
        cls.start = decode_save(cls.original / "START.SAV")
        cls.source = bytearray(cls.start)
        cls.source[UNITS:UNITS + 2] = bytes((10, 8))
        cls.source[UNITS + 5] |= 0x80
        cls.source = oracle.view(cls.source, (10, 8), (4, 7))
        # 期望图只从公开原资源primitive独立生成，不调用待测foreground helper。
        field = (cls.original / "FDFIELD.DAT").read_bytes()
        data, metadata = archive_entry(field, 0), archive_entry(field, 1)
        width, height = struct.unpack_from("<HH", data)
        if (width, height) != (24, 24):
            raise AssertionError("纯夹具需已确认stage0原图尺寸")
        shape = (cls.original / "FDSHAP.DAT").read_bytes()
        tiles = archive_entry(shape, 2 * metadata[0])
        attrs = archive_entry(shape, 2 * metadata[0] + 1)
        upper, own = [struct.unpack_from("<I", data, 4 + 4 * (y * width + 10))[0] & 1023 for y in (7, 8)]
        if (upper, own, attrs[4 * upper], attrs[4 * own]) != (39, 278, 0, 128):
            raise AssertionError("纯夹具需原39/278地形与279前景")
        cls.upper = sheet_frame(tiles, upper)
        cls.base = sheet_frame(tiles, own)
        begin, end = struct.unpack_from("<II", tiles, 6 + 4 * (own + 1))
        cls.foreground, cls.written = rle_material(struct.pack("<HH", 24, 24) + tiles[begin:end])
        cls.icons = (cls.original / "FDICON.B24").read_bytes()
        cls.frame = cls.compose(0)

    @classmethod
    def compose(cls, phase, *, acted=True, cursor=True, foreground=True):
        image = Image.new("P", (24, 30), 0)
        image.putpalette(cls.assets["palette"])
        image.paste(cls.upper.crop((0, 18, 24, 24)), (0, 0))
        image.paste(cls.base, (0, 6))
        if cursor:
            part = cls.assets["cursor"]
            image.paste(part, (0, 6), part.point(lambda value: 255 if value else 0, mode="L"))
        part = sheet_frame(cls.icons, phase)  # unit0：原12个角色帧。
        if acted:
            part = acted_sprite(part)
        image.paste(part, (0, 0), part.point(lambda value: 255 if value else 0, mode="L"))
        if foreground:
            image.paste(cls.foreground, (0, 6), cls.written)
        frame = Image.new("RGB", (320, 200))
        frame.paste(image.convert("RGB"), (148, 22))
        return frame

    def test_complete_foreground_resource_and_acted_browse(self):
        before = bytes(self.source)
        for phase in (0, 1, 2):
            with self.subTest(phase=phase):
                found = oracle.actor_browse_anywhere(self.compose(phase), self.original,
                    self.source, self.assets, 0, acted=True)
                self.assertIsNotNone(found)
                state, detail = found
                self.assertEqual(list(state[META + 3:META + 9]), [4, 7, 10, 8, 6, 1])
                self.assertEqual(detail["actor_local_foreground"]["foreground_entries"], [279])
                self.assertEqual(detail["cursor_actor"]["checked_pixels"], 720)
                self.assertEqual(detail["cursor_actor"]["bad_pixels"], 0)
                self.assertEqual(detail["cursor_actor"]["max_channel_delta"], 0)
                proof = detail["visible_acted_witness"]
                self.assertEqual(proof["all_unacted_candidates_rejected"], 12)
                self.assertGreaterEqual(proof["minimum_unacted_bad_pixels"], 24)
                self.assertTrue(detail["full_actor_compared"])
        self.assertEqual(bytes(self.source), before)

    def test_all_twelve_unacted_frames_cannot_prove_commit(self):
        for phase in range(12):
            with self.subTest(phase=phase):
                self.assertIsNone(oracle.focus_match(self.compose(phase, acted=False),
                    self.original, self.source, self.assets))

    def test_cursor_foreground_full_roi_and_geometry_are_required(self):
        for name, frame in (("no_cursor", self.compose(0, cursor=False)),
                            ("no_foreground", self.compose(0, foreground=False))):
            with self.subTest(name=name):
                self.assertIsNone(oracle.focus_match(frame, self.original, self.source, self.assets))
        # 上方6行是纯地形，不属于裸角色mask；其中任何坏像素仍须拒绝。
        damaged = self.frame.copy()
        color = damaged.getpixel((148, 22))
        damaged.putpixel((148, 22), tuple(255 - value for value in color))
        self.assertIsNone(oracle.focus_match(damaged, self.original, self.source, self.assets))
        wrong = oracle.view(self.source, (10, 8), (5, 7))
        self.assertIsNone(oracle.focus_match(self.frame, self.original, wrong, self.assets))
        walking = bytearray(self.source)
        walking[UNITS + 4] = 1
        with self.assertRaises(ValueError):
            oracle.actor_foreground_resources(self.original, walking, self.assets, 0, self.assets["cursor"])

    def test_no_foreground_keeps_existing_actor_and_acted_contract(self):
        state = bytearray(self.start)
        state[UNITS + 5] |= 0x80
        state = oracle.view(state, tuple(oracle.actor(state, 0)[:2]))
        self.assertIsNone(oracle.actor_foreground_resources(self.original, state, self.assets, 0, self.assets["cursor"]))
        resource = actor_focus_resources(self.original, state, self.assets, 0, self.assets["cursor"])["templates"][0]
        at = screen(state, oracle.actor(state, 0)[:2])
        frame = Image.new("RGB", (320, 200))
        frame.paste(resource.convert("RGB"), (at[0], at[1] - 6))
        found = oracle.actor_browse_anywhere(frame, self.original, state, self.assets, 0, acted=True)
        self.assertIsNotNone(found)
        result, detail = found
        self.assertEqual(result, state)
        self.assertEqual(detail["raw_actor"], 0)
        self.assertNotIn("actor_local_foreground", detail)
        self.assertNotIn("foreground_acted_witness", detail)
        self.assertTrue(detail["full_actor_compared"])
        self.assertEqual(detail["visible_acted_witness"]["bad_pixels"], 0)
        self.assertGreaterEqual(detail["visible_acted_witness"]["checked_pixels"], 24)


    def test_stage1_three_foreground_cells_use_declared_bank_and_full_acted_gate(self):
        # 与B7 prepare一致借round_assets；仅START actor0合成stage1单角色静止视图，
        # 不借私有entry/PNG/环境变量，不宣称自然路径或原版运行已覆盖这三格。
        before = bytes(self.start)
        assets = round_assets(self.original)
        field = (self.original / "FDFIELD.DAT").read_bytes()
        data, metadata = archive_entry(field, 3), archive_entry(field, 4)
        self.assertEqual(struct.unpack_from("<HH", data), (27, 21))
        self.assertEqual(metadata[0], 1)
        shape = (self.original / "FDSHAP.DAT").read_bytes()
        tiles, attrs = archive_entry(shape, 2 * metadata[0]), archive_entry(shape, 2 * metadata[0] + 1)

        def tile(x, y):
            return struct.unpack_from("<I", data, 4 + 4 * (y * 27 + x))[0] & 1023

        for x, expected in ((20, 85), (21, 87), (22, 89)):
            with self.subTest(world=(x, 15)):
                source = bytearray(self.start)
                source[0] = metadata[0]
                source[META + 1:META + 3] = bytes((1, 1))
                source[UNITS:UNITS + 2] = bytes((x, 15))
                source[UNITS + 3:UNITS + 5] = bytes(2)
                source[UNITS + 5] |= 0x80
                source = oracle.view(source, (x, 15), (14, 12))
                self.assertEqual(tile(x, 15), expected)
                self.assertEqual(attrs[4 * expected], 0x80)
                self.assertEqual(attrs[4 * tile(x, 14)], 0)
                begin, end = struct.unpack_from("<II", tiles, 6 + 4 * (expected + 1))
                foreground, written = rle_material(struct.pack("<HH", 24, 24) + tiles[begin:end])
                at = screen(source, (x, 15))

                def compose(phase, acted):
                    image = Image.new("P", (24, 30), 0)
                    image.putpalette(assets["palette"])
                    image.paste(sheet_frame(tiles, tile(x, 14)).crop((0, 18, 24, 24)), (0, 0))
                    image.paste(sheet_frame(tiles, expected), (0, 6))
                    cursor = assets["cursor"]
                    image.paste(cursor, (0, 6), cursor.point(lambda p: 255 if p else 0, mode="L"))
                    sprite = sheet_frame(self.icons, phase)
                    if acted:
                        sprite = acted_sprite(sprite)
                    image.paste(sprite, (0, 0), sprite.point(lambda p: 255 if p else 0, mode="L"))
                    # 原12ac6的RLE write mask，包含opaque0，不由实际坏点裁剪。
                    image.paste(foreground, (0, 6), written)
                    frame = Image.new("RGB", (320, 200))
                    frame.paste(image.convert("RGB"), (at[0], at[1] - 6))
                    return frame

                for phase in range(3):
                    found = oracle.focus_match(compose(phase, True), self.original, source, assets)
                    self.assertIsNotNone(found)
                    self.assertEqual(found["actor_local_foreground"]["foreground_entries"], [expected + 1])
                    self.assertEqual(found["cursor_actor"]["checked_pixels"], 720)
                    self.assertEqual(found["cursor_actor"]["bad_pixels"], 0)
                    self.assertEqual(found["cursor_actor"]["max_channel_delta"], 0)
                    self.assertEqual(found["foreground_acted_witness"]["all_unacted_candidates_rejected"], 12)
                    self.assertGreaterEqual(found["foreground_acted_witness"]["minimum_unacted_bad_pixels"], 24)
                for phase in range(12):
                    self.assertIsNone(oracle.focus_match(compose(phase, False), self.original, source, assets))
                damaged = compose(0, True)
                pixel = damaged.getpixel((at[0], at[1] - 6))
                damaged.putpixel((at[0], at[1] - 6), tuple(255 - p for p in pixel))
                self.assertIsNone(oracle.focus_match(damaged, self.original, source, assets))
        self.assertEqual(bytes(self.start), before)


if __name__ == "__main__":
    unittest.main()
