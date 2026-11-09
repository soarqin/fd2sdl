"""34有限资源阶段门的纯正／负例，不启动窗口或改原始采集。"""
from pathlib import Path
import sys
import unittest

from PIL import Image

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import fd2_g2_composite_frames as checker


class CompositeFramesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original=Path(__file__).resolve().parents[2]/"original_game"
        _,cls.source,_=checker.oracle.derive_fixture(cls.original,"composite34")
        cls.resources=checker.resources(cls.original,cls.source)

    def render(self,rule):
        actual=Image.new("RGB",(320,200))
        for anchor in rule["anchors"]:
            actual.paste(anchor["image"],anchor["at"],anchor["mask"])
        return actual

    def test_true_callee_color_and_unused_caller_color_stay_distinct(self):
        self.assertEqual(self.resources["solid_tints"],{"17":200,"18":200,"19":200})
        self.assertEqual(self.resources["unused_caller_table_arguments"],{"17":146,"18":72,"19":216})
        rule=self.resources["rules"]["flash17"][0]
        actual=self.render(rule)
        self.assertIn("flash17",checker.classify(actual,self.resources))
        actor=rule["anchors"][1]
        other=checker.frames.archive_entry((self.original/"FDOTHER.DAT").read_bytes(),0)
        old_color=tuple(v<<2 for v in other[192*3:192*3+3])
        replacement=Image.new("RGB",actor["image"].size,old_color)
        actual.paste(replacement,actor["at"],actor["mask"])
        self.assertFalse(checker.frames._anchor_match(actual,actor))
        self.assertNotIn("flash17",checker.classify(actual,self.resources))

    def test_black_is_not_a_positive_resource_phase(self):
        self.assertEqual(checker.classify(Image.new("RGB",(320,200)),self.resources),{})

    def test_short_numbers_keep_both_targets_and_independent_tile(self):
        for phase in ("popup17","popup18","popup19"):
            with self.subTest(phase=phase):
                rule=self.resources["rules"][phase][0];actual=self.render(rule)
                self.assertIn(phase,checker.classify(actual,self.resources))
                anchor=rule["anchors"][0]
                # 不以地形正锚代替数字：破坏一个真正数字写点即不能通过该资源姿态。
                px,py=next((x,y) for y in range(anchor["image"].height) for x in range(anchor["image"].width)
                    if x+anchor["at"][0]>130 and anchor["mask"].getpixel((x,y)) and
                    max(anchor["image"].getpixel((x,y)))>32)
                actual.putpixel((px+anchor["at"][0],py+anchor["at"][1]),(0,0,0))
                self.assertFalse(checker.frames._anchor_match(actual,anchor))

    def test_original_order_and_no_frame_reuse(self):
        rows=[{"row":{"index":i},"phases":{phase:{"variant":0}}} for i,phase in enumerate(checker.PHASES)]
        self.assertTrue(checker.ordered_witnesses(rows)["ok"])
        swapped=list(rows);swapped[2],swapped[3]=swapped[3],swapped[2]
        # 实际输入按index排序；时间次序交换而非重排数组伪造原index。
        swapped=[{"row":{"index":i},"phases":row["phases"]} for i,row in enumerate(swapped)]
        self.assertFalse(checker.ordered_witnesses(swapped)["ok"])
        same=[{"row":{"index":0},"phases":{phase:{"variant":0} for phase in checker.PHASES}}]
        self.assertFalse(checker.ordered_witnesses(same)["ok"])

    def test_identical_later_flash_cannot_hide_missing_first_flash(self):
        sequence=("fx17","popup17","fx18","flash17","popup18","fx19","flash19","popup19")
        rows=[{"row":{"index":i},"phases":{phase:{"variant":0}}} for i,phase in enumerate(sequence)]
        self.assertFalse(checker.ordered_witnesses(rows,checker.PHASES[2:])["ok"])


if __name__=="__main__":
    unittest.main()
