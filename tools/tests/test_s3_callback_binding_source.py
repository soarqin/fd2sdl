"""五个新增caller的受控输入矩阵边界；原CPU/C差分由独立CLI执行。"""
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"ida"))
import s3_callback_binding_source as source


class CallbackBindingSourceTest(unittest.TestCase):
    def test_matrix_keeps_all_sides_full_bag_and_once_branches(self):
        metadata=bytes(131);rows=source.cases(metadata,41)
        self.assertEqual(len(rows),36)
        self.assertEqual(len({r["id"] for r in rows}),36)
        self.assertEqual({r["caller"] for r in rows},{0,1})
        self.assertEqual({r["units"][80*r["caller"]+6] for r in rows},{0,1,2})
        self.assertEqual({r["state"][16] for r in rows},{0,1})
        self.assertEqual({sum(not(r["units"][80*r["caller"]+10+i*2]&128) for i in range(8)) for r in rows},{0,7,8})
        self.assertTrue(all(len(r["units"])==96*80 and r["count"]==80 for r in rows))
        rows[0]["units"][0]=255;rows[0]["state"][0]=255
        self.assertNotEqual(rows[1]["units"][0],255)
        self.assertEqual(rows[1]["state"][0],0)


if __name__=="__main__":unittest.main()
