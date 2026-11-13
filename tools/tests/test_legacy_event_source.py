"""独立完整六callback CPU/原表对照及错误定义负例；不启动GUI。"""
import copy
import importlib.util
import json
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('legacy_source',ROOT/'tools/ida/verify_fd2_legacy_events.py')
source=importlib.util.module_from_spec(spec);spec.loader.exec_module(source)

class LegacySourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path=ROOT/'tools/fd2_le_relocated_relbase.bin'
        if not path.exists():path=ROOT.parents[1]/'tools/fd2_le_relocated_relbase.bin'
        if not path.exists():raise unittest.SkipTest('缺少已验strict镜像；不宣称原码通过')
        cls.image=path.read_bytes();cls.field=(ROOT/'original_game/FDFIELD.DAT').read_bytes()
        cls.doc=json.loads((ROOT/'data/static/flows/common_event.json').read_text())
        # 原六caller机码与写序独立检查；当前S3扩展收据门由共享验证器另测。
        cls.doc['bindings']=dict(source.LEGACY_BINDINGS)

    def test_complete_original_callers_and_direct_writes(self):
        r=source.check(self.image,self.field,self.doc)
        self.assertEqual(len(r['cases']),6)
        self.assertEqual([s['count'] for s in r['cases'][0]['stories']],[13,14])
        self.assertEqual(r['cases'][-1]['stories'][0]['ai'],[[160,176]]*6)

    def test_definition_cannot_advance_post_story_writes_or_clear_wrong_edge(self):
        for case in ('after_story','missing_delay20','early_group7','wrong_port','extra_binding'):
            d=copy.deepcopy(self.doc)
            f=next(f for f in d['flows'] if f['id']=='common.legacy.action.006')
            if case=='after_story':
                n=next(n for n in f['nodes'] if n['op']=='story');n['op']='ai_anchor';n['args']=[5,10,26,15]
            elif case=='missing_delay20':
                f=next(f for f in d['flows'] if f['id']=='common.legacy.action.001')
                next(n for n in f['nodes'] if n['op']=='delay_ms')['args']=[19]
            elif case=='early_group7':
                f=next(f for f in d['flows'] if f['id']=='common.legacy.action.000')
                n=next(n for n in f['nodes'] if n['op']=='story');n['op']='append_group';n['args']=[7,0]
            elif case=='wrong_port':
                n=next(n for n in f['nodes'] if n['op']=='story');n['on']['next']=n['on'].pop('ack')
            else:d['bindings']['stage.002.action.000']='common.legacy.action.000'
            with self.subTest(case=case),self.assertRaises(ValueError):source.check(self.image,self.field,d)

    def test_wrong_original_source_rejected(self):
        image=bytearray(self.image);image[0x34596]^=1
        with self.assertRaisesRegex(ValueError,'镜像身份'):source.check(image,self.field,self.doc)
        field=bytearray(self.field);field[-1]^=1
        with self.assertRaisesRegex(ValueError,'FDFIELD身份'):source.check(self.image,field,self.doc)

    def test_extra_binding_without_scope_does_not_borrow_six_caller_pass(self):
        document=copy.deepcopy(self.doc)
        document['bindings']['stage.002.action.009']='common.callback.009'
        with self.assertRaisesRegex(ValueError,'release-scope'):
            source.check(self.image,self.field,document)

if __name__=='__main__':unittest.main()
