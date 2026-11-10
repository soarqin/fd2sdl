"""准备原码差分工具的来源、覆盖、未知字节与分写拒绝门；不启动GUI。"""
import copy
import importlib.util
import json
import os
from pathlib import Path
import unittest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('preparation_machine',ROOT/'tools/ida/verify_fd2_preparation.py')
machine=importlib.util.module_from_spec(spec);spec.loader.exec_module(machine)

class PreparationMachineTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        image=Path(os.environ.get('FD2_PREPARATION_IMAGE',ROOT/'tools/fd2_le_relocated_relbase.bin'))
        readset=ROOT/'build/g3/preparation-domain-readset.json';results=ROOT/'build/g3/preparation-domain-C.jsonl'
        if not all(p.is_file() for p in (image,readset,results,ROOT/'original_game/FD2.EXE')):
            raise unittest.SkipTest('需本地strict镜像、只读Domain读集与真实C probe JSONL；不以缺源当通过')
        cls.image=image.read_bytes();cls.readset=json.loads(readset.read_text(encoding='utf-8'))
        cls.executable=(ROOT/'original_game/FD2.EXE').read_bytes();cls.raw=results.read_bytes();cls.cases=machine.load_cases(cls.raw)
        machine.validate_sources(cls.image,cls.readset,cls.executable)
    def case(self,name):return copy.deepcopy(next(c for c in self.cases if c['case_id']==name))
    def test_complete_source_identity_and_metadata_name_independence(self):
        broken=bytearray(self.image);broken[0x2b777]^=1
        with self.assertRaises(ValueError):machine.validate_sources(bytes(broken),self.readset,self.executable)
        broken=copy.deepcopy(self.readset);broken['functions'].pop()
        with self.assertRaises(ValueError):machine.validate_sources(self.image,broken,self.executable)
        broken=copy.deepcopy(self.readset);broken['functions'][0]['chunks'][0]['raw_hex']='00'+broken['functions'][0]['chunks'][0]['raw_hex'][2:]
        with self.assertRaises(ValueError):machine.validate_sources(self.image,broken,self.executable)
        renamed=copy.deepcopy(self.readset);renamed['functions'][0]['name']='confirmed_new_name'
        machine.validate_sources(self.image,renamed,self.executable)
    def test_missing_duplicate_unknown_kind_and_short_owner_rejected(self):
        for change in ('missing','duplicate','kind','roster','count','coverage-arguments'):
            broken=copy.deepcopy(self.cases)
            if change=='missing':broken.pop()
            elif change=='duplicate':broken[1]=copy.deepcopy(broken[0])
            elif change=='kind':broken[0]['kind']='unconfirmed'
            elif change=='roster':broken[0]['input_roster_hex']=broken[0]['input_roster_hex'][:-2]
            elif change=='count':broken[0]['count']=True
            else:broken[0]['args'][0]=20
            raw='\n'.join(json.dumps(c) for c in broken).encode()
            with self.assertRaises(ValueError,msg=change):machine.load_cases(raw)
    def test_original_last_match_and_unknown_byte_difference(self):
        case=self.case('front-last-match');result=machine.Original(self.image,case).run()
        original=bytes.fromhex(case['input_roster_hex']);finished=bytes.fromhex(result['original']['finish_roster_hex'])
        self.assertEqual(finished[80:160],original[30*80:31*80])
        changed=bytearray.fromhex(case['finish_roster_hex']);changed[80+0x2f]^=1;case['finish_roster_hex']=changed.hex()
        with self.assertRaises(ValueError):machine.Original(self.image,case).run()
    def test_original_revive_writes_and_C_boundary_order(self):
        case=self.case('revive-0');result=machine.Original(self.image,case).run()
        self.assertEqual([c['label'] for c in result['original']['commit_trace']],['money','flags','hp'])
        self.assertEqual(result['price_at_boundary'],9600)
        trace=result['original']['commit_trace'];self.assertEqual(bytes.fromhex(trace[0]['actor_hex'])[5],0x85)
        self.assertEqual(bytes.fromhex(trace[1]['actor_hex'])[64:66],b'\x0f\0')
        case['commit_trace']=list(reversed(case['commit_trace']))
        with self.assertRaises(ValueError):machine.Original(self.image,case).run()
    def test_money_boundary_does_not_claim_undefined_digits_executed(self):
        result=machine.Original(self.image,self.case('money-sub-1')).run()
        self.assertEqual(result['original']['finish_money'],0xffffffff)
        self.assertIn('before potentially undefined',result['boundary'])
        self.assertNotIn('money digit draw',result['explicit_stub_calls'])
        result=machine.Original(self.image,self.case('money-add-0')).run()
        self.assertEqual(result['boundary'],'complete function return')
        self.assertGreater(result['explicit_stub_calls']['money digit draw'],0)
    def test_undeclared_original_write_is_not_silently_accepted(self):
        emulator=machine.Original(self.image,self.case('hidden-7'))
        with self.assertRaises(RuntimeError):emulator.write_hook(emulator.cpu,0,0x53000,4,9,None)

if __name__=='__main__':unittest.main()
