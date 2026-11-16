"""Finite source-model contracts only; no C/original/GUI/IDA or resources."""
import importlib.util
from pathlib import Path
import unittest

PATH=Path(__file__).resolve().parents[1]/"ida"/"derive_f4_path_layout_vectors.py"
spec=importlib.util.spec_from_file_location("path_layout_source",PATH)
m=importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)


class PathLayoutTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rows={r["name"]:r for r in m.cases()}

    def e(self,name):return self.rows[name]["expected"]

    def test_boundary31_has_no_alias(self):
        e=self.e("before32-0")
        self.assertEqual((e["result"],e["mode"],e["aliases"]),(255,0,[]))

    def test_pop_does_not_restore_mode(self):
        e=self.e("mode32-0")
        self.assertEqual((e["result"],e["mode"],e["output"][:2]),(1,2,[0,1]))
        self.assertEqual(e["writes"][-2:],[["4e711",0,0],["4e713",1,1]])

    def test_target_is_recorded_before_child_mode(self):
        e=self.e("target-before-mode-store")
        self.assertEqual((e["result"],e["written"],e["mode"]),(32,32,2))
        self.assertEqual(e["output"][:32],[3]*32)

    def test_shared_XY_overwrites_directions_but_not_tail(self):
        e=self.e("directions-then-XY")
        self.assertEqual((e["result"],e["written"],e["output"][:4]),(1,32,[0,1,3,3]))
        self.assertEqual(e["output"][32:],[165]*32)

    def test_requested_witness_can_write_direction_prefix(self):
        for name,mode in [("requested2-to-normal0",0),("requested2-to-equal1",1)]:
            e=self.e(name)
            self.assertEqual((e["result"],e["mode"],e["written"]),(33,mode,33))
            self.assertEqual(e["output"][:33],[3]*33)

    def test_other_y_is_source_normal_branch(self):
        self.assertEqual(self.e("requested0-to-other3")["mode"],3)

    def test_target_capacity_failure_keeps_prior_result(self):
        e=self.e("target-H-capacity31")
        self.assertEqual((e["status"],e["result"],e["mode"],e["written"]),(-2,32,0,31))
        self.assertEqual(e["first_error"],["4e78a",31])

    def test_witness_capacity_failure_before_result_store(self):
        e=self.e("witness-H-capacity1")
        self.assertEqual((e["status"],e["result"],e["written"],e["output"][:2]),(-2,255,1,[0,165]))

    def test_root_target_store_is_mode_independent(self):
        e=self.e("root-mode2-result0-no-bytes")
        self.assertEqual((e["result"],e["written"],e["typed_found"]),(0,0,0))

    def test_fixed_long28_remains_before_alias(self):
        e=self.e("fixed28-clearflags")
        self.assertEqual((e["mode"],e["aliases"]),(1,[]))

    def test_zero_cost_not_falsely_qualified(self):
        row=dict(self.rows["before32-0"],costs=[0,20])
        with self.assertRaisesRegex(ValueError,"positive"):
            m.expected(row)


if __name__=="__main__":unittest.main()
