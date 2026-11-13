"""总览原码差分工具的来源完整性门；合成字节不冒原机器码执行。"""
import copy
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools/ida'))
import verify_s3_overview as verify


class OverviewSourceGuards(unittest.TestCase):
    def source(self):
        image=bytes([0x90])*0x64000
        rows=[]
        for start,end in verify.FUNCTIONS.items():
            raw=image[start:end];rows.append({'va':start,'end':end,
                'chunks':[{'start':start,'end':end,'raw_hex':raw.hex(),'sha256':verify.digest(raw)}],
                'instructions':[{'ea':ea,'size':1,'raw_hex':'90'} for ea in range(start,end)]})
        source={'schema':'fd2-s3-overview-source-v1','functions':rows,'image_sha256':verify.digest(image),
            'database_before_sha256':'a'*64,'database_after_sha256':'a'*64}
        return image,source

    def test_source_identity_bytes_full_boundaries_and_instruction_coverage(self):
        image,source=self.source()
        with patch.object(verify,'IMAGE_SHA',verify.digest(image)):
            self.assertEqual(set(verify.validate(image,source)),set(verify.FUNCTIONS))
            mutations=[]
            wrong=copy.deepcopy(source);wrong['image_sha256']='b'*64;mutations.append(wrong)
            wrong=copy.deepcopy(source);wrong['database_after_sha256']='b'*64;mutations.append(wrong)
            wrong=copy.deepcopy(source);wrong.pop('database_before_sha256');wrong.pop('database_after_sha256');mutations.append(wrong)
            wrong=copy.deepcopy(source);wrong['functions'].append(wrong['functions'][0]);mutations.append(wrong)
            wrong=copy.deepcopy(source);wrong['functions'][0]['end']+=1;mutations.append(wrong)
            wrong=copy.deepcopy(source);wrong['functions'][0]['chunks'][0]['sha256']='b'*64;mutations.append(wrong)
            wrong=copy.deepcopy(source);wrong['functions'][0]['instructions'].pop();mutations.append(wrong)
            wrong=copy.deepcopy(source);wrong['functions'][0]['instructions'][1]['ea']+=1;mutations.append(wrong)
            wrong=copy.deepcopy(source);wrong['functions'][0]['instructions'][1]['raw_hex']='91';mutations.append(wrong)
            for wrong in mutations:
                with self.subTest(wrong=mutations.index(wrong)),self.assertRaises(ValueError):verify.validate(image,wrong)

    def test_vectors_keep_real_roster_first_last_and_zero_HP_hidden_domains(self):
        cases=list(verify.cases());self.assertEqual({c['stage'] for c in cases},set(range(30)))
        private=[c for c in cases if c['stage']==16]
        self.assertEqual({c['count'] for c in private},{0,1,32})
        records=[bytes.fromhex(c['roster']) for c in private if c['count']==32]
        self.assertTrue(any(r[8]==18 for r in records));self.assertTrue(any(r[31*80+8]==18 for r in records))
        self.assertTrue(any(r[5]==255 and r[8]==18 and r[64:66]==bytes(2) and r[6]==255 for r in records))
        self.assertTrue(all(c['active_text18'] for c in cases))


if __name__=='__main__':unittest.main()
