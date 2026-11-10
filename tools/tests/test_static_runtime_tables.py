"""共享运行表的原LE字节、来源身份与已解码LUT引用；不启动窗口。"""
import copy
import hashlib
import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/static_data'))
import generate
from verify_unit_tables import source_range


class RuntimeTablesTest(unittest.TestCase):
    def test_range_sequence_matches_independent_original_bytes(self):
        executable=ROOT/'original_game/FD2.EXE'
        if not executable.exists():self.skipTest('需要本地只读FD2.EXE，不声称原字节通过')
        raw=executable.read_bytes()
        document=json.loads((ROOT/'data/static/tables/runtime.json').read_text())
        generate.validate_document(document)
        table=next(t for t in document['tables'] if t['name']=='k_field_range_sequence')
        source=table['source'];original=source_range(raw,0x51a97,20)
        self.assertEqual(source['file_sha256'],hashlib.sha256(raw).hexdigest())
        self.assertEqual((source['canonical_va'],source['length'],source['object']),('0x51a97',20,2))
        self.assertEqual(source['sha256'],hashlib.sha256(original).hexdigest())
        self.assertEqual(bytes(table['values']),original)
        self.assertEqual((table['c_type'],table['dimensions']),('uint8_t',[20]))
        changed=bytearray(table['values']);changed[7]^=1
        self.assertNotEqual(bytes(changed),original) # 仍在23帧内的错源不能借引用校验放过。

    def test_decoded_lut_references_reject_missing_outside_or_wrong_length(self):
        tables={t['name']:t['values'] for p in (ROOT/'data/static/tables').glob('*.json')
            for t in json.loads(p.read_text())['tables']}
        generate.validate_references(tables)
        for invalid in (None,[-1]*20,[23]*20,[0]*19):
            with self.subTest(invalid=invalid):
                changed=copy.deepcopy(tables)
                if invalid is None:del changed['k_field_range_sequence']
                else:changed['k_field_range_sequence']=invalid
                with self.assertRaisesRegex(ValueError,'range LUT'):generate.validate_references(changed)


if __name__=='__main__':unittest.main()
