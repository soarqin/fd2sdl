"""纯合成向量/来源故障门；不运行C/原CPU/GUI，不读取原资源。"""
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path
from unittest import mock

PATH=Path(__file__).resolve().parents[1]/"ida"/"derive_f4_movement_vectors.py"
spec=importlib.util.spec_from_file_location("movement_vectors",PATH)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)


def case(budget=2, mode=None, units=None):
    row=dict(width=3,height=1,budget=budget,classes=[0],costs=[1],
             units=units or [[2,1,0,0]],apply=0,cells=[0xff003c00]*3)
    if mode is not None: row.update(mode=mode,target=[0,0])
    return row


class SourceModelTests(unittest.TestCase):
    def test_signed_full_byte(self):
        self.assertEqual([m.signed(x) for x in (0,127,128,255)],[0,127,-128,-1])

    def test_129_first_neighbor_reject(self):
        e=m.range_expected(case(129));self.assertEqual(e["before_friend_raw3"],[255,129,255])
        self.assertEqual(e["cells"],[0xff000000,0x81000000,0xff000000])

    def test_128_backfills_start(self):
        e=m.range_expected(case(128));self.assertEqual(e["before_friend_raw3"],[127,126,127])
        self.assertEqual(e["distances"],[1,2,1])

    def test_255_is_real_ff_sentinel(self):
        e=m.range_expected(case(255));self.assertEqual(e["can_stop_mask"],0)
        self.assertEqual(e["distances"],[0xffffffff]*3)

    def test_zero_cost_still_improves_signed_ff(self):
        row=case(0);row["costs"]=[0]
        self.assertEqual(m.range_expected(row)["before_friend_raw3"],[0,0,0])

    def test_friend_does_not_block_transit(self):
        row=case(2,units=[[2,0,0,0],[1,1,0,0]])
        e=m.range_expected(row);self.assertEqual(e["before_friend_raw3"],[2,1,0])
        self.assertEqual(e["can_stop_mask"],5)
        self.assertEqual(e["distances"],[0,1,2])

    def test_zoc_compares_before_forcing_zero(self):
        row=case(5,units=[[2,0,0,0],[0,2,0,0]])
        e=m.range_expected(row);self.assertEqual(e["before_friend_raw3"],[5,0,255])
        self.assertEqual(e["cells"],[0x05000000,0x00800000,0xff400000])

    def test_apply_preserves_nonruntime_before(self):
        row=case(2);row.update(apply=1,cells=[0x7f203c00,0x80203c00,0xff203c00])
        e=m.range_expected(row);self.assertEqual(e["cells"],[0x7f203c00,0x02203c00,0x01203c00])
        self.assertEqual(e["distances"],[131,0,1])

    def test_source_literal_path_tie(self):
        row=case(2,mode=0);row.update(width=2,height=2,cells=[0xff003c00]*4,units=[[2,1,1,0]])
        e=m.path_expected(row);self.assertEqual(e["directions"],[1,2])
        self.assertEqual(e["cells"],[0x00000800,0x01000400,0x01000400,0x02000000])

    def test_mode_witness_uses_last_admitted_hostile(self):
        row=case(2,mode=2,units=[[2,0,0,0],[0,2,0,0]])
        e=m.path_expected(row);self.assertEqual(e["witness"],[2,0])
        self.assertEqual(e["directions"],[])


class SourceGuardTests(unittest.TestCase):
    def check(self, change=None):
        image=b"\0"*16+b"ABCD"+b"\0"*4
        row=dict(va=16,chunks=[dict(start=16,end=20,bytes=b"ABCD".hex(),sha256=m.sha(b"ABCD"))],
                 instructions=[dict(va=16,size=2),dict(va=18,size=2)])
        if change: change(row)
        raw=json.dumps(dict(functions=[row])).encode()
        with tempfile.TemporaryDirectory() as tmp:
            source=Path(tmp)/"source.json";ip=Path(tmp)/"image.bin"
            source.write_bytes(raw);ip.write_bytes(image)
            with mock.patch.multiple(m,SOURCE_SHA=m.sha(raw),IMAGE_SHA=m.sha(image),FUNCTIONS=(16,)):
                return m.source_guard(source,ip)

    def test_complete_chunk_and_instruction_coverage(self):
        self.assertEqual(self.check(),{"0x10":2})

    def test_duplicate_instruction_rejected(self):
        with self.assertRaisesRegex(ValueError,"duplicate"):
            self.check(lambda r:r["instructions"].append(dict(va=18,size=2)))

    def test_missing_tail_rejected(self):
        with self.assertRaises((KeyError,ValueError)):
            self.check(lambda r:r["instructions"].pop())

    def test_chunk_identity_corruption_rejected(self):
        with self.assertRaisesRegex(ValueError,"strict chunk"):
            self.check(lambda r:r["chunks"][0].update(bytes=b"ABCE".hex()))

    def test_extra_instruction_outside_chunk_rejected(self):
        with self.assertRaisesRegex(ValueError,"outside"):
            self.check(lambda r:r["instructions"].append(dict(va=20,size=1)))


if __name__=="__main__":unittest.main()
