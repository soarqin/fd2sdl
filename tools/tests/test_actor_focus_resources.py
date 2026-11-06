"""同格焦点纯资源回归；依据12c0d首可见raw、11cac/127a9绘制序，不运行游戏。"""
from pathlib import Path
import sys
import unittest
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import run_original_magic_list as m
from run_original_battle_menu import match_indexed
from run_original_end_turn import META, UNITS, STRIDE, round_assets
from run_original_item_list import post_use_navigation_resources, post_use_navigation_state
from run_original_player import acted_sprite, cursor_assets, screen, sheet_frame
from run_graphics_tests import decode_save

ORIGINAL = Path(__file__).resolve().parents[2] / "original_game"


class ActorFocusResourcesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = decode_save(ORIGINAL / "START.SAV")
        cls.assets = round_assets(ORIGINAL)
        cls.sheet = (ORIGINAL / "FDICON.B24").read_bytes()

    def overlap(self):
        source = bytearray(self.source)
        source[META+1] = 4; source[META+3:META+5] = bytes((5,11))
        for raw in range(4): source[UNITS+raw*STRIDE+5] |= 1
        for raw, identity in ((0,0),(2,4)):
            at = UNITS+raw*STRIDE
            source[at:at+2] = bytes((7,14)); source[at+3:at+8] = bytes((0,0,0,2,identity))
        return source

    def legacy(self, source, raw):
        record = source[UNITS+raw*STRIDE:UNITS+(raw+1)*STRIDE]
        isolated = bytearray(source); isolated[META+1] = 1
        isolated[UNITS:UNITS+STRIDE] = record
        _, composites = cursor_assets(ORIGINAL / "FDICON.B24", isolated, self.assets)
        return [image.crop((0,0,24,24)) for image, offset in composites if offset == -6]

    def draw(self, source, phases, order=(0,2)):
        image = Image.new("P", (24,30), 0); image.putpalette(self.assets["palette"])
        cursor = self.assets["cursor"]
        image.paste(cursor, (0,6), cursor.point(lambda x: 255 if x else 0, mode="L"))
        for raw, phase in zip(order, phases):
            record = source[UNITS+raw*STRIDE:UNITS+(raw+1)*STRIDE]
            part = sheet_frame(self.sheet, record[7]*12+(record[3]&3)*3+phase)
            if record[5]&128: part = acted_sprite(part)
            image.paste(part, (0,0), part.point(lambda x: 255 if x else 0, mode="L"))
        return image.crop((0,0,24,24))

    def frame(self, source, image):
        frame = Image.new("RGB", (320,200))
        at = screen(source, (7,14)); frame.paste(image.convert("RGB"), (at[0],at[1]-6))
        return frame

    def matches(self, source, resource, image):
        frame = self.frame(source, image); at = screen(source, (7,14))
        return [match_indexed(frame, part, (at[0],at[1]-6))["ok"] for part in resource["templates"]]

    def test_nonoverlap_keeps_all_legacy_bytes_palette_and_order(self):
        for raw in range(4):
            for acted in (False,True):
                source = bytearray(self.source)
                source[UNITS+raw*STRIDE+5] = 128 if acted else 0
                actual = m.actor_focus_resources(ORIGINAL, source, self.assets, raw, self.assets["cursor"])
                expected = self.legacy(source, raw)
                self.assertEqual(len(actual["templates"]), 12)
                self.assertEqual([x.tobytes() for x in actual["templates"]], [x.tobytes() for x in expected])
                self.assertEqual([x.getpalette() for x in actual["templates"]], [x.getpalette() for x in expected])

    def test_same_cell_matches_cursor_then_raw_order_and_preserves_first_raw(self):
        source = self.overlap(); before = bytes(source)
        resource = m.actor_focus_resources(ORIGINAL, source, self.assets, 0, self.assets["cursor"])
        for phase in range(3): self.assertTrue(any(self.matches(source, resource, self.draw(source, (phase,phase)))))
        self.assertEqual(resource["composite_raw_actors"], [0,2])
        self.assertEqual(resource["shared_idle_phases"], [0,1,2])
        self.assertEqual((resource["raw_actor"],resource["unit_id"],resource["world"]), (0,0,[7,14]))
        self.assertEqual(resource["template_rows"], 24); self.assertFalse(resource["full_actor_compared"])
        self.assertEqual(bytes(source), before)

    def test_each_source_direction_and_acted_mapping_share_only_one_idle_phase(self):
        source = self.overlap(); source[UNITS+3] = 1; source[UNITS+2*STRIDE+3] = 3
        source[UNITS+5] = 128
        cursor_before = self.assets["cursor"].tobytes()
        resource = m.actor_focus_resources(ORIGINAL, source, self.assets, 0, self.assets["cursor"])
        for phase in resource["shared_idle_phases"]:
            self.assertTrue(any(self.matches(source, resource, self.draw(source, (phase,phase)))))
        self.assertFalse(any(self.matches(source, resource, self.draw(source, (0,1)))))
        self.assertEqual(self.assets["cursor"].tobytes(), cursor_before)

    def test_hidden_same_cell_is_not_a_layer_and_keeps_legacy_output(self):
        source = self.overlap(); source[UNITS+2*STRIDE+5] |= 1
        actual = m.actor_focus_resources(ORIGINAL, source, self.assets, 0, self.assets["cursor"])
        self.assertEqual([x.tobytes() for x in actual["templates"]], [x.tobytes() for x in self.legacy(source,0)])
        self.assertNotIn("composite_raw_actors", actual)

    def test_reverse_order_or_asynchronous_actor_frames_cannot_match(self):
        source = self.overlap()
        resource = m.actor_focus_resources(ORIGINAL, source, self.assets, 0, self.assets["cursor"])
        self.assertFalse(any(self.matches(source, resource, self.draw(source,(1,1),order=(2,0)))))
        self.assertFalse(any(self.matches(source, resource, self.draw(source,(0,1)))))

    def test_fully_occluded_first_actor_nonfirst_owner_or_walking_is_explicitly_unsupported(self):
        source = self.overlap()
        with self.assertRaises(ValueError): m.actor_focus_resources(ORIGINAL,source,self.assets,2,self.assets["cursor"])
        source[UNITS+4] = 1
        with self.assertRaises(ValueError): m.actor_focus_resources(ORIGINAL,source,self.assets,0,self.assets["cursor"])
        source[UNITS+4] = 0; source[UNITS+2*STRIDE+7] = 0
        with self.assertRaises(ValueError): m.actor_focus_resources(ORIGINAL,source,self.assets,0,self.assets["cursor"])

    def test_navigation_uses_first_visible_raw_without_accepting_bare_or_wrong_cell(self):
        source = self.overlap()
        resource = post_use_navigation_resources(ORIGINAL,source,self.assets,(7,14))
        image = self.draw(source,(1,1)); frame = self.frame(source,image)
        found = post_use_navigation_state(frame,source,self.assets,resource)
        self.assertEqual(found["post_use_actor"]["raw_actor"],0)
        self.assertEqual(found["post_use_actor"]["unit_id"],0)
        self.assertEqual(found["post_use_actor"]["focus_screen"],[52,76])
        frame = Image.new("RGB",(320,200)); frame.paste(self.assets["cursor"].convert("RGB"),(52,76))
        self.assertIsNone(post_use_navigation_state(frame,source,self.assets,resource))
        frame = Image.new("RGB",(320,200)); frame.paste(image.convert("RGB"),(76,70))
        self.assertIsNone(post_use_navigation_state(frame,source,self.assets,resource))


if __name__ == "__main__": unittest.main()
