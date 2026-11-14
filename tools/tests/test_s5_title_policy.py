"""S5有限标题UI发布政策与真实FDOTHER全图；不启动窗口、不新增cap。"""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock,patch

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_s5_stage_check as host
import run_s5_cinematic_check as cinematic
from native import s5_fixture,s5_cinematic_fixture
from run_original_comparison import compare,title_templates


class S5TitlePolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        original=ROOT/'original_game'
        cls.p21=s5_fixture.prepare(original)
        cls.p24=s5_cinematic_fixture.derive(original,s5_cinematic_fixture.SCENARIOS[0])
        cls.p20=s5_cinematic_fixture.derive(original,s5_cinematic_fixture.SCENARIOS[1])
        cls.titles=title_templates(original/'FDOTHER.DAT')
        definition=json.loads((ROOT/'data/static/flows/stage.json').read_text(encoding='utf8'))
        cls.cap20=next(flow for flow in definition['flows'] if flow['id']=='stage.capability.020')

    def definition(self,directory,bound29):
        flows=[self.cap20]
        if bound29:
            cap29=copy.deepcopy(self.cap20);cap29['id']='stage.capability.029';flows.append(cap29)
        path=Path(directory)/'stage.json';path.write_text(json.dumps({'flows':flows}),encoding='utf8')
        return path

    def test_current_and_future_capability_select_exact_full_resource_title(self):
        with tempfile.TemporaryDirectory() as d:
            for bound29 in (False,True):
                definition=self.definition(d,bound29)
                for p in (self.p21,self.p24):
                    for kind,count in (('original',3),('sdl',3 if bound29 else 2)):
                        with self.subTest(bound29=bound29,manual=p['manual_state']['stage'],kind=kind):
                            policy=host.title_case_policy(p,kind,definition)
                            self.assertEqual(policy['menu_count'],count)
                            self.assertIs(policy['active_capability_bound'],bound29)
                            self.assertEqual(policy['active_capability_id'],'stage.capability.029')
                            self.assertEqual(policy['stage_definition_sha256'],hashlib.sha256(definition.read_bytes()).hexdigest())
                            for selection in (0,1):
                                expected=self.titles[policy['menu_count']][selection]
                                self.assertEqual(expected.size,(320,200))
                                self.assertTrue(compare(expected,self.titles[count][selection])['ok'])
                                self.assertFalse(compare(expected,self.titles[5-count][selection])['ok'])

    def test_stage20_remains_three_and_requires_its_published_capability(self):
        with tempfile.TemporaryDirectory() as d:
            path=self.definition(d,False)
            for kind in ('original','sdl'):
                policy=host.title_case_policy(self.p20,kind,path)
                self.assertEqual(policy['menu_count'],3);self.assertTrue(policy['active_capability_bound'])
            path.write_text('{"flows":[]}',encoding='utf8')
            with self.assertRaises(ValueError):host.title_case_policy(self.p20,'sdl',path)

    def test_late_continue_domains_require_exact_published_capability(self):
        with tempfile.TemporaryDirectory() as d:
            path=self.definition(d,False)
            for stage in (26,29):
                p={**self.p20,'source':bytearray(self.p20['source']),
                   'manual_state':{**self.p20['manual_state'],'stage':stage}}
                p['source'][host.META+2]=stage
                with self.assertRaises(ValueError):host.title_case_policy(p,'sdl',path)
                cap=copy.deepcopy(self.cap20);cap['id']=f'stage.capability.{stage:03d}'
                path.write_text(json.dumps({'flows':[cap]}),encoding='utf8')
                for kind in ('original','sdl'):
                    policy=host.title_case_policy(p,kind,path)
                    self.assertEqual(policy['menu_count'],3)
                    self.assertEqual(policy['active_snapshot_stage'],stage)
                    self.assertEqual(policy['manual_slot0_stage'],stage)
                path=self.definition(d,False)

    def test_unknown_backend_or_fixture_and_malformed_capability_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            path=self.definition(d,False)
            with self.assertRaises(ValueError):host.title_case_policy(self.p21,'guess',path)
            for active,manual,available in ((29,22,1),(21,21,1),(29,21,0)):
                p={**self.p21,'source':bytearray(self.p21['source']),
                    'manual_state':{**self.p21['manual_state'],'stage':manual}}
                p['source'][host.META]=available;p['source'][host.META+2]=active
                with self.assertRaises(ValueError):host.title_case_policy(p,'sdl',path)
            path=self.definition(d,True);definition=json.loads(path.read_text(encoding='utf8'))
            definition['flows'][-1]['nodes'][0]['op']='end'
            path.write_text(json.dumps(definition),encoding='utf8')
            with self.assertRaises(ValueError):host.title_case_policy(self.p21,'sdl',path)
            path=self.definition(d,True);definition=json.loads(path.read_text(encoding='utf8'))
            definition['flows'].append(definition['flows'][-1])
            path.write_text(json.dumps(definition),encoding='utf8')
            with self.assertRaises(ValueError):host.title_case_policy(self.p21,'sdl',path)

    def test_both_manual_load_callers_use_policy_for_start_and_load_without_extra_keys(self):
        with tempfile.TemporaryDirectory() as d:
            for bound29 in (False,True):
                path=self.definition(d,bound29)
                for module,exercise,prepared in ((host,host.title_entry,self.p21),(cinematic,cinematic.entry24,self.p24)):
                    p={**prepared,'titles':self.titles,'prepare_assets':{}}
                    policy=host.title_case_policy(p,'sdl',path);record={'kind':'sdl','title_menu_policy':policy}
                    capture=Mock();capture.wait.side_effect=StopIteration('stop before LOAD confirmation')
                    with patch.object(module.shared,'checkpoint'),patch.object(host.shared,'preparation_town',return_value={'assets':{}}),\
                         patch.object(module,'wait_title') as wait:
                        with self.assertRaises(StopIteration):exercise(capture,p,None,None,record)
                    wait.assert_called_once_with(capture,self.titles[policy['menu_count']][0],timeout=90)
                    capture.key.assert_called_once_with('DOWN')
                    capture.wait.assert_called_once_with(self.titles[policy['menu_count']][1])


if __name__=='__main__':unittest.main()
