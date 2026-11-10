"""Suite设置页纯资源门；不开窗口、不投递输入、不替代正式配置/Load整档验收。"""
import itertools
from pathlib import Path
import sys
import unittest

from PIL import Image

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))
import run_graphics_tests as runner
from run_original_battle_menu import MENU_OFFSETS, Oracle, resource_templates


class GraphicsOptionsResourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        original = TOOLS.parent / "original_game"
        cls.assets = resource_templates(original / "FDOTHER.DAT", original / "FDTXT.DAT")
        cls.oracle = Oracle(cls.assets, (148, 76))

    def frame(self, ids, selected=0, disabled=()):
        image = Image.new("RGB", (320, 200), (8, 12, 16))
        for index, command in enumerate(ids):
            state = 2 if index in disabled else int(index == selected)
            at = tuple(a + b for a, b in zip(self.oracle.focus, MENU_OFFSETS[index]))
            image.paste(self.assets["commands"][command * 3 + state].convert("RGB"), at)
        return image

    def test_all_four_zero_nonzero_label_combinations_and_direction_selections(self):
        for music, sfx, effect, info in itertools.product((0, 1), repeat=4):
            # 独立列明1728c标签方向；第三项与另三项反向。
            ids = (19 if music == 0 else 18, 21 if sfx == 0 else 20,
                   22 if effect == 0 else 23, 25 if info == 0 else 24)
            values = [music, sfx, effect, info]
            for selected in range(4):
                with self.subTest(values=values, selected=selected):
                    result = runner.options_resource_state(self.frame(ids, selected), self.oracle, values, selected)
                    self.assertIsNotNone(result)
                    self.assertEqual([row["frame"] for row in result["matches"]],
                                     [command * 3 + (index == selected) for index, command in enumerate(ids)])
                    self.assertEqual(result["raw_values"], values)

    def test_raw_nonzero_is_label_projection_not_command_index_addition(self):
        values = [2, 255, 7, 0xa5]
        result = runner.options_resource_state(self.frame((18, 20, 23, 24), 2), self.oracle, values, 2)
        self.assertIsNotNone(result)
        self.assertEqual([row["frame"] for row in result["matches"]], [54, 60, 70, 72])
        self.assertEqual(result["raw_values"], values)  # 不把有效raw归一回写。

    def test_true_disabled_frame_of_any_item_cannot_be_reported_enabled(self):
        for item in range(4):
            with self.subTest(item=item):
                self.assertIsNone(runner.options_resource_state(
                    self.frame((18, 20, 22, 24), disabled=(item,)), self.oracle, [1, 1, 0, 1]))

    def test_old_two_disabled_or_missing_items_do_not_pass_current_four_item_page(self):
        self.assertIsNone(runner.options_resource_state(
            self.frame((18, 20, 22, 24), disabled=(2, 3)), self.oracle, [1, 1, 0, 1]))
        self.assertIsNone(runner.options_resource_state(Image.new("RGB", (320, 200)), self.oracle, [1, 1, 0, 1]))

    def test_wrong_label_at_each_slot_is_rejected(self):
        for item in range(4):
            ids = [18, 20, 22, 24]; ids[item] += 1
            with self.subTest(item=item):
                self.assertIsNone(runner.options_resource_state(self.frame(ids), self.oracle, [1, 1, 0, 1]))

    def test_different_or_not_yet_highlighted_selection_cannot_ack_selected_zero(self):
        for selected in (None, 1, 2, 3):
            with self.subTest(selected=selected):
                self.assertIsNone(runner.options_resource_state(
                    self.frame((18, 20, 22, 24), selected), self.oracle, [1, 1, 0, 1], 0))

    def test_invalid_values_and_selection_are_rejected(self):
        frame = self.frame((18, 20, 22, 24))
        for values in ([1, 1, 0], [1, 1, -1, 1], [1, 1, 256, 1], [1, 1, True, 1]):
            with self.subTest(values=values), self.assertRaises(ValueError):
                runner.options_resource_state(frame, self.oracle, values)
        for selected in (-1, 4, True):
            with self.subTest(selected=selected), self.assertRaises(ValueError):
                runner.options_resource_state(frame, self.oracle, [1, 1, 0, 1], selected)


if __name__ == "__main__":
    unittest.main()
