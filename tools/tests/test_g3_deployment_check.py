"""G3部署原生工具纯资源/计划回归；不启动窗口、RPC、CDB或构建。"""
from pathlib import Path
import copy
import ast
import hashlib
import json
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from PIL import Image
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_g3_deployment_check as runner
import fd2_g3_deployment_pages as pages


class DeploymentCheckTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.p={n:runner.prepare(ROOT,n) for n in runner.SCENARIOS}

    def test_selection_counts_cursor_auto_advance_and_no_precursor_save(self):
        for name,p in self.p.items():
            with self.subTest(name=name):
                steps=p['steps'];quota=19 if p['before']['stage']>26 else 15
                chooses=[s for s in steps if s['name'].startswith('selection-chosen-')]
                self.assertEqual(len(chooses),quota-1)
                for i,s in enumerate(chooses):
                    self.assertEqual(len(s['chosen']),i+1)
                    self.assertEqual(s['remaining'],quota-i-1)
                    self.assertEqual(s['cursor'],s['chosen'][-1]%(p['before']['count']-1))
                self.assertFalse(any('save' in s for s in steps))
                self.assertNotEqual(p['before']['roster'],p['after']['roster'])
                if p['before']['stage']!=1:
                    skip=next(s for s in steps if s['name']=='notown-save-no');self.assertEqual((skip['key'],skip['selected']),('RIGHT',1))

    def test_final_no_post_state_has_real_new_page_and_missing_member_stops_at657(self):
        p=self.p['p5-select21-final-no'];steps=p['steps']
        self.assertEqual([s['name'] for s in steps[-3:]],['deployment-final-confirm','deployment-final-no','notown-selection-reentered-after-no'])
        self.assertEqual(steps[-1]['chosen'],[]);self.assertEqual(steps[-1]['cursor'],0)
        last=self.p['p6-notown22-missing24']['steps'][-1]
        self.assertEqual((last['name'],last['fragment'],last['dynamic']),('required-member24-missing',657,25))
        self.assertFalse(any(s.get('fragment')==658 for s in self.p['p6-notown22-missing24']['steps']))

    def test_full_label_grid_rejects_wrong_selection_cursor_and_remaining(self):
        p=self.p['p5-select17-final-no'];state=copy.deepcopy(p['before'])
        source=pages.selection(p['assets'],state,[2],2,15)
        frame=Image.new('P',(320,200),0);frame.putpalette(p['assets']['palette'])
        frame.paste(source['label'],(5,7));frame.paste(source['grids'][1],(5,94));rgb=frame.convert('RGB')
        hit=pages.match(rgb,source);self.assertTrue(hit);self.assertEqual(hit['checked_pixels'],38086)
        for chosen,cursor in (([],2),([2],3),([1],2)):
            self.assertIsNone(pages.match(rgb,pages.selection(p['assets'],state,chosen,cursor,15)))
        self.assertEqual(state,p['before'])
        broken=rgb.copy();broken.putpixel((5,94),(255,255,255));self.assertIsNone(pages.match(broken,source))

    def test_prepare_only_has_no_native_observer_and_explicit_pending_status(self):
        with tempfile.TemporaryDirectory(prefix='g3-deployment-pure-') as tmp:
            args=SimpleNamespace(root=ROOT,scenario=runner.SCENARIOS[1],artifacts=Path(tmp)/'run',
                prepare_only=True,original_only=True,driver=None,dosbox=None,sdl=None,cdb=None,pdb=None)
            with patch.object(runner,'running',side_effect=AssertionError('不得开窗口')),\
                 patch.object(runner.snapshot,'OwnedRPC',side_effect=AssertionError('不得启动RPC')),\
                 patch.object(runner.cdb_snapshot,'take',side_effect=AssertionError('不得attach')):
                result=runner.run(args)
            self.assertFalse(result['ok']);self.assertFalse(result['original_case_passed'])
            self.assertEqual(result['status'],'ready_not_executed');self.assertTrue(result['final_sources_unchanged'])
            self.assertTrue(result['probe_only']);self.assertFalse(result['whole_manual_post_Save_claimed'])
            self.assertEqual((args.artifacts/'controlled.SAV').stat().st_size,22987)

    def test_original_only_probe_success_is_not_confused_with_failed_case(self):
        with tempfile.TemporaryDirectory(prefix='g3-deploy-reuse-') as tmp:
            path=Path(tmp)/'comparison.json';args=SimpleNamespace(root=ROOT,scenario=runner.SCENARIOS[1])
            base={'scenario':args.scenario,'original_case_passed':True,'ok':False,'probe_only':True,
                'final_sources_unchanged':True,'original_before':{},'display_requirement':{'released':True}}
            for case in ({'kind':'original','ok':False},{'kind':'original','ok':True,'business_sequence_completed':False}):
                path.write_text(json.dumps({**base,'cases':[case]}),encoding='utf-8')
                with self.assertRaisesRegex(ValueError,'真正失败或未完成'):
                    runner.inherit_original(path,self.p[args.scenario],args,{}, {})

    def test_original_reuse_protects_prepare_exercise_ast_and_final_files(self):
        module=ast.parse(Path(runner.__file__).read_text(encoding='utf-8'))
        protected=[ast.dump(n,include_attributes=False) for n in module.body if isinstance(n,ast.FunctionDef) and n.name in ('prepare','exercise')]
        self.assertEqual(hashlib.sha256('\n'.join(protected).encode()).hexdigest(),runner.PRE_REUSE_EXECUTION_AST)
        with tempfile.TemporaryDirectory(prefix='g3-deploy-seal-') as tmp:
            path=Path(tmp)/'ram.bin';path.write_bytes(b'original-single-snapshot')
            receipt={'sealed_files':{str(path):runner.sha(path)}}
            self.assertTrue(runner.inherited_unchanged(receipt));path.write_bytes(b'changed')
            self.assertFalse(runner.inherited_unchanged(receipt));path.unlink();self.assertFalse(runner.inherited_unchanged(receipt))

    def test_sdl_rpm_metadata_never_claims_old_writemem_or_changes_original_mapping(self):
        with tempfile.TemporaryDirectory(prefix='g3-deploy-rpm-label-') as tmp:
            work=Path(tmp);original={'kind':'original','mapping':{'single_snapshot_fields':True}}
            old=copy.deepcopy(original)
            self.assertTrue(runner.normalize_sdl_snapshot_metadata(original,work));self.assertEqual(original,old)
            self.assertFalse((work/'runtime-mapping.json').exists())
            read={'method':'ReadProcessMemory','single_ReadProcessMemory':True,'all_requested_bytes_read':True,
                'address':0x12340000,'size':2572,'sha256':'owned'}
            case={'kind':'sdl','mapping':{'source':'PDB','all_fields_from_one_writemem':True},'cdb':{
                'memory_read':read,'layout':{'address':0x12340000,'size':2572},'snapshot':{'sha256':'owned'},
                'all_fields_from_one_ReadProcessMemory':True,'all_reads_in_one_stopped_attach':True}}
            self.assertTrue(runner.normalize_sdl_snapshot_metadata(case,work))
            self.assertNotIn('all_fields_from_one_writemem',case['mapping'])
            self.assertTrue(case['mapping']['all_fields_from_one_ReadProcessMemory'])
            self.assertEqual(json.loads((work/'runtime-mapping.json').read_text(encoding='utf-8')),case['mapping'])
            for key,value in (('method','unknown'),('size',2571),('sha256','changed'),('single_ReadProcessMemory',False)):
                bad=copy.deepcopy(case);bad['cdb']['memory_read'][key]=value
                self.assertFalse(runner.normalize_sdl_snapshot_metadata(bad,work))
                self.assertFalse(bad['mapping']['all_fields_from_one_ReadProcessMemory'])


if __name__=='__main__':unittest.main()
