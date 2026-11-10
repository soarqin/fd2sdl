"""效果页来源身份、完整chunks及篡改拒绝；不运行GUI或声称Domain重导。"""
import copy
import importlib.util
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from rebuild_fd2_analysis import parse_le,parse_objects,build_relbase_image,parse_fixups,apply_fixups
spec=importlib.util.spec_from_file_location('effect_owner_source',ROOT/'tools/ida/verify_effect_owner_source.py')
check=importlib.util.module_from_spec(spec);spec.loader.exec_module(check)


class EffectOwnerSourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        exe=(ROOT/'original_game/FD2.EXE').read_bytes();le=parse_le(exe);objects=parse_objects(exe,le)
        cls.image=bytes(apply_fixups(build_relbase_image(exe,le,objects),parse_fixups(exe,le,objects)))
        # 只构来源门的纯测试输入，不登记成新Domain提取或CPU执行。
        cls.readset={'functions':[{'va':va,'end':chunks[0][1],'name':'test-source','chunks':[
            {'start':start,'end':end,'sha256':sha,'raw_hex':cls.image[start:end].hex()}
            for start,end,sha in chunks]} for va,chunks in check.FUNCTION_CHUNKS.items()]}

    def test_original_image_and_twelve_full_functions_accept(self):
        functions,verified=check.validate_sources(self.readset,self.image)
        self.assertEqual((len(functions),len(verified)),(12,12))

    def test_image_identity_rejects_even_outside_target_span(self):
        changed=bytearray(self.image);changed[0x10010]^=1
        with self.assertRaisesRegex(ValueError,'镜像身份'):check.validate_sources(self.readset,changed)

    def test_missing_duplicate_and_shortened_domain_source_rejected(self):
        for kind in ('missing','duplicate','end','shared-tail'):
            data=copy.deepcopy(self.readset)
            if kind=='missing':data['functions'].pop()
            elif kind=='duplicate':data['functions'].append(copy.deepcopy(data['functions'][0]))
            elif kind=='end':data['functions'][0]['end']-=1
            else:next(row for row in data['functions'] if row['va']==0x22046)['chunks'].pop()
            with self.subTest(kind=kind),self.assertRaises(ValueError):check.validate_sources(data,self.image)

    def test_raw_data_hash_and_bounds_cannot_be_forged(self):
        for kind in ('raw','hash','bounds'):
            data=copy.deepcopy(self.readset);part=data['functions'][0]['chunks'][0]
            if kind=='raw':part['raw_hex']='00'+part['raw_hex'][2:]
            elif kind=='hash':part['sha256']='0'*64
            else:part['start']+=1
            with self.subTest(kind=kind),self.assertRaises(ValueError):check.validate_sources(data,self.image)


if __name__=='__main__':unittest.main()
