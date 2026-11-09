"""正常库存来源门拒绝洞／未知记录和错误身份；实际原资源另由CLI批量核对。"""
import importlib.util
from pathlib import Path
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/static_data"))
spec = importlib.util.spec_from_file_location("inventory_sources", ROOT / "tools/static_data/verify_inventory_sources.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)


class InventorySourceTest(unittest.TestCase):
    def test_all_dense_sizes_accept_stale_hidden_ids(self):
        for count in range(9):
            module.check_inventory([214] * count + [255] * (8 - count),
                                   [i < count for i in range(8)])

    def test_source_hole_or_occupied_unknown_cannot_be_silently_normalized(self):
        for flags in ([False, True] + [False] * 6, [True, False, True] + [False] * 5):
            with self.assertRaisesRegex(ValueError, "dense prefix"):
                module.check_inventory([0] * 8, flags)
        with self.assertRaisesRegex(ValueError, "no original record"):
            module.check_inventory([215] + [0] * 7, [True] + [False] * 7)

    def test_identity_and_extent_fail_before_original_inference(self):
        with self.assertRaisesRegex(ValueError, "identity"):
            module.verify(b"not EXE", b"not FDFIELD")
        with self.assertRaisesRegex(ValueError, "eight raw slots"):
            module.check_inventory([0] * 7, [True] * 7)


if __name__ == "__main__":
    unittest.main()
