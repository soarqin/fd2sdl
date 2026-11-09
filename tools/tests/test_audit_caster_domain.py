"""正常技能来源不可被 empty／错误分派归类吞掉；真实原资源另由CLI核对。"""
import importlib.util
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/static_data"))
spec = importlib.util.spec_from_file_location("audit_caster_domain", ROOT / "tools/static_data/audit_caster_domain.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


def entries():
    # 仅选槽/目录夹具；不声称这是可播放的原图像。
    part = bytes((1, 0, 1, 0, 0, 0, 0, 0)) + struct.pack("<I", 12) + bytes(13)
    return [(0, part), (25, part), (50, module.EMPTY)]


class CasterDomainTest(unittest.TestCase):
    def test_ordinary_fallback_does_not_hide_derived_empty(self):
        normal = {"unit_id": 0, "magic_id": 0, "kind": "ordinary", "sources": ["valid-learning"]}
        plans, errors = module.check_pairs(entries(), [normal])
        self.assertFalse(errors)
        self.assertEqual(plans[0]["attempted_entries"], [2, 1])
        derived = dict(normal, magic_id=30, kind="derived")
        plans, errors = module.check_pairs(entries(), [derived])
        self.assertEqual(len(errors), 1)
        self.assertEqual(plans[0]["attempted_entries"], [2])
        self.assertEqual(errors[0]["sources"], ["valid-learning"])

    def test_controller_dispatch_cannot_reclassify_a_real_full_pair(self):
        self.assertIsNone(module.full_kind(9, player=True))
        self.assertEqual(module.full_kind(9, player=False), "ordinary")
        self.assertIsNone(module.full_kind(30, player=False))
        self.assertEqual(module.full_kind(30, player=True), "derived")
        with self.assertRaisesRegex(ValueError, "dispatch"):
            module.casting_plan(entries(), 0, 30, "ordinary")
        for magic in (-1, 31, 36):
            with self.assertRaisesRegex(ValueError, "normal original record"):
                module.full_kind(magic, player=True)

    def test_legal_promotion_excludes_ff_item_and_old7(self):
        special = [255] * 18
        special[7], special[9] = 89, 88
        self.assertFalse(module.promotion_targets(7, special))
        self.assertEqual(module.promotion_targets(17, special), [(49, "ordinary", None)])
        self.assertEqual(module.promotion_targets(9, special),
                         [(41, "ordinary", None), (59, "special", 88), (52, "special-priority", 90)])

    def test_bad_source_identity_or_fake_empty_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "identity"):
            module.verify(b"wrong EXE", b"wrong FDFIELD", b"wrong FIGANI")
        bad = entries()
        bad[2] = (50, module.EMPTY + b"\0")
        with self.assertRaisesRegex(ValueError, "directory"):
            module.casting_plan(bad, 0, 0, "ordinary")

    def test_split_mode_is_separate_from_byte_count(self):
        data = bytearray(entries()[0][1])
        data[1] = 1
        plan = module.clip_info([(0, bytes(data))], 0)
        self.assertEqual((plan["part_count"], plan["split_mode"]), (1, 1))
        data[1] = 255
        with self.assertRaisesRegex(ValueError, "split mode"):
            module.clip_info([(0, bytes(data))], 0)
        for count in (2, 255):
            with self.subTest(count=count):
                data = bytearray(entries()[0][1])
                data[0] = count
                with self.assertRaisesRegex(ValueError, "directory"):
                    module.clip_info([(0, bytes(data))], 0)


if __name__ == "__main__":
    unittest.main()
