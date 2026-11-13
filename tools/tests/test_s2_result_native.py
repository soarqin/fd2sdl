"""S2受控结果SAV的写集、未知尾与合法说话人；只读纯测试，不启动窗口。"""
import copy
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import struct

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools"))
import s2_result_native_fixture as fixture
from run_s2_result_check import output_guard


class ResultNativeFixtureTest(unittest.TestCase):
    def source(self):
        data=bytearray([0x57]*0x59cb)
        data[fixture.META+1]=12;data[fixture.META+2]=0;data[fixture.META+9]=4
        for raw in range(12):
            at=fixture.UNITS+raw*80;data[at+5]=0;data[at+7]=96;data[at+8]=96
        at=fixture.UNITS+80;data[at+7:at+9]=bytes((9,9))
        data[-4:]=sum(data[:-4]).to_bytes(4,"little")
        metadata=bytes((29,20,70))+bytes(0x83+70*26-3)
        return bytes(data),metadata

    def test_exact_write_set_retains_unknown_tail_roster_and_slots(self):
        source,metadata=self.source();plain,encrypted,manifest=fixture.derive(source,metadata)
        self.assertEqual(len(plain),len(source));self.assertEqual(len(encrypted),len(source))
        self.assertEqual(plain[:len(metadata)],metadata)
        self.assertEqual(plain[len(metadata):fixture.UNITS],source[len(metadata):fixture.UNITS])
        self.assertEqual(plain[fixture.UNITS+21*80:fixture.META],source[fixture.UNITS+21*80:fixture.META])
        self.assertEqual(plain[fixture.META+3:-4],source[fixture.META+3:-4])
        self.assertEqual(plain[fixture.META+1:fixture.META+3],bytes((21,29)))
        self.assertEqual([raw for raw in range(21) if not plain[fixture.UNITS+raw*80+5]&1],[0,20])
        donor=source[fixture.UNITS+11*80:fixture.UNITS+12*80]
        for raw in range(12,21):
            expected=bytearray(donor);expected[5]=expected[5]&~1 if raw==20 else expected[5]|1
            self.assertEqual(plain[fixture.UNITS+raw*80:fixture.UNITS+(raw+1)*80],expected)
        self.assertEqual(manifest["speaker"],{"unit_id":9,"raw_actor":1,"source":"active","visible":False})
        self.assertEqual(manifest["expected_latch"],{"fresh_continue":0,"story7_entry":0,"after_story_ack":2})
        self.assertEqual(sum(plain[:-4]),int.from_bytes(plain[-4:],"little"))
        self.assertFalse(manifest["production_binding_added"]);self.assertFalse(manifest["natural_state_claimed"])

    def test_missing_speaker_wrong_source_and_bad_checksum_are_rejected(self):
        source,metadata=self.source()
        for change in (lambda x:x.__setitem__(fixture.META+1,13),
                       lambda x:x.__setitem__(fixture.UNITS+80+8,8)):
            bad=bytearray(source);change(bad);bad[-4:]=sum(bad[:-4]).to_bytes(4,"little")
            with self.assertRaises(ValueError):fixture.derive(bytes(bad),metadata)
        with self.assertRaises(ValueError):fixture.derive(source[:-1],metadata)
        with self.assertRaises(ValueError):fixture.derive(source,b'\0'+metadata[1:])
        bad=bytearray(source);bad[0]^=1
        with self.assertRaises(ValueError):fixture.derive(bytes(bad),metadata)

    def test_output_is_new_build_s2_outside_readonly_original(self):
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);original=root/"original_game";original.mkdir()
            output_guard(root,original,root/"build/s2/new")
            for target in (root/"elsewhere",original/"build/s2/new",root/"build/s0/new",original):
                with self.subTest(target=target),self.assertRaises(ValueError):output_guard(root,original,target)
            existing=root/"build/s2/existing";existing.mkdir(parents=True)
            with self.assertRaises(ValueError):output_guard(root,original,existing)

    def test_resource_adapter_restores_all_dimension_state_on_failure(self):
        import fd2_natural_oracle as natural
        source,metadata=self.source();plain,_,_=fixture.derive(source,metadata)
        original_dimensions=dict(natural.STAGE_DIMENSIONS)
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);raw=b"synthetic-readonly-container";(root/"FDFIELD.DAT").write_bytes(raw)
            map_data=struct.pack("<HH",45,40)+bytes(45*40*4)
            with patch.object(fixture,"SOURCE_HASHES",{"FDFIELD.DAT":fixture.digest(raw)}),\
                 patch.object(fixture,"archive_entries",return_value=[b""]*87+[map_data]):
                with self.assertRaisesRegex(RuntimeError,"stop"):
                    with fixture.resource_matcher_context(root,plain) as evidence:
                        self.assertEqual(natural.STAGE_DIMENSIONS[29],(45,40))
                        self.assertEqual(evidence["length"],7204)
                        raise RuntimeError("stop")
            self.assertEqual(natural.STAGE_DIMENSIONS,original_dimensions)
            with self.assertRaises(ValueError):
                with fixture.resource_matcher_context(root,source):pass
        self.assertEqual(natural.STAGE_DIMENSIONS,original_dimensions)

    def test_sdl_log_interface_runs_both_real_wait_log_calls_without_window(self):
        from types import SimpleNamespace
        import run_s2_result_check as runner
        source,metadata=self.source();plain,_,_=fixture.derive(source,metadata)
        with tempfile.TemporaryDirectory() as directory:
            work=Path(directory);save=work/'controlled.SAV';save.write_bytes(b'isolated-save')
            (work/'game.log').write_text('S2 result source ready:\nS2 result source done: result=2\n')
            capture=SimpleNamespace(process=SimpleNamespace(poll=lambda:None),key=lambda key:None)
            case={'kind':'sdl','checkpoints':[]}
            prepared={'plain':plain,'original':work,'assets':{},'pages':[]}
            with patch.object(runner,'wait_state'),patch.object(runner,'observe'):
                runner._exercise(capture,prepared,None,None,work,case,save)
            self.assertTrue(case['controlled_save_unchanged'])


if __name__=="__main__":unittest.main()
