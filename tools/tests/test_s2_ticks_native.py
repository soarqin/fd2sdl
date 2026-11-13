"""S2真stage20 WORD47夹具的写集、只读域、连续帧判据与无窗口接口。"""
from contextlib import nullcontext
import ast
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock,patch

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools"))
import s2_ticks_native_fixture as fixture
import run_s2_ticks_check as runner
import s2_result_native_fixture as reader


class TicksNativeFixtureTest(unittest.TestCase):
    def source(self):
        data=bytearray([0x57]*0x59cb)
        data[fixture.META+1:fixture.META+3]=bytes((12,0))
        for raw in range(12):
            at=fixture.UNITS+raw*80;data[at+5]=0
        data[fixture.UNITS+0x1a]=0
        struct.pack_into("<HH",data,fixture.UNITS+0x40,42,42)
        data[-4:]=sum(data[:-4]).to_bytes(4,"little")
        metadata=bytes((20,16,80))+bytes(0x8a3-3)
        return bytes(data),metadata

    def test_declared_source_cell_and_write_set_preserve_other_bytes(self):
        source,metadata=self.source();plain,encoded,manifest=fixture.derive(source,metadata)
        self.assertEqual(len(encoded),len(source))
        effective=bytearray(metadata);effective[2]=0;struct.pack_into("<BH",effective,0x59,2,47)
        self.assertEqual(plain[:len(metadata)],effective)
        self.assertEqual(plain[len(metadata):fixture.UNITS],source[len(metadata):fixture.UNITS])
        self.assertEqual(plain[fixture.UNITS+18*80:0x30a5],source[fixture.UNITS+18*80:0x30a5])
        self.assertEqual(plain[0x30a6:fixture.META],source[0x30a6:fixture.META])
        self.assertEqual(plain[fixture.META+9:-4],source[fixture.META+9:-4])
        self.assertEqual(plain[fixture.META:fixture.META+9],bytes((1,18,20,3,15,7,19,4,4)))
        self.assertEqual(plain[0x30a5],0)
        self.assertEqual(plain[fixture.UNITS:fixture.UNITS+2],bytes((7,19)))
        self.assertEqual([raw for raw in range(18) if not plain[fixture.UNITS+raw*80+5]&1],[0,1,16,17])
        self.assertEqual(plain[fixture.UNITS+80+5]&0x85,0)
        for raw in (16,17):
            at=fixture.UNITS+raw*80
            self.assertEqual(plain[at:at+2],bytes((40,39)))
            self.assertEqual(plain[at+5],4)
        self.assertEqual(sum(plain[:-4]),int.from_bytes(plain[-4:],"little"))
        self.assertEqual(manifest["source_goals"],[1,8,8,8,8])
        self.assertFalse(manifest["natural_state_claimed"])
        self.assertFalse(manifest["production_binding_added"])

    def test_invalid_count_checksum_metadata_and_wait_recovery_domain_fail(self):
        source,metadata=self.source()
        for at,value in ((fixture.META+1,13),(fixture.META+2,20),(fixture.UNITS+0x1a,1),(fixture.UNITS+0x40,41)):
            bad=bytearray(source);bad[at]=value;bad[-4:]=sum(bad[:-4]).to_bytes(4,"little")
            with self.subTest(offset=at),self.assertRaises(ValueError):fixture.derive(bytes(bad),metadata)
        bad=bytearray(source);bad[0]^=1
        with self.assertRaises(ValueError):fixture.derive(bytes(bad),metadata)
        for bad in (metadata[:-1],bytes((20,16,79))+metadata[3:]):
            with self.assertRaises(ValueError):fixture.derive(source,bad)

    def test_resource_adapter_restores_all_dimensions_even_after_exception(self):
        source,metadata=self.source();plain,_,_=fixture.derive(source,metadata)
        before=dict(fixture.natural.STAGE_DIMENSIONS)
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);raw=b"read-only-synthetic-archive";(root/"FDFIELD.DAT").write_bytes(raw)
            data=struct.pack("<HH",41,40)+bytes(41*40*4)
            with patch.object(fixture,"SOURCE_HASHES",{"FDFIELD.DAT":fixture.digest(raw)}),\
                 patch.object(fixture,"archive_entry",return_value=data):
                with self.assertRaisesRegex(RuntimeError,"stop"):
                    with fixture.resource_context(root,plain) as evidence:
                        self.assertEqual(fixture.natural.STAGE_DIMENSIONS[20],(41,40))
                        self.assertEqual(evidence["length"],6564)
                        raise RuntimeError("stop")
            self.assertEqual(fixture.natural.STAGE_DIMENSIONS,before)
            with self.assertRaises(ValueError):
                with fixture.resource_context(root,source):pass
        self.assertEqual(fixture.natural.STAGE_DIMENSIONS,before)

    def test_reader_rejects_every_unlisted_stage_count_before_access(self):
        for stage,count in ((0,12),(20,17),(20,19),(29,20),(30,21)):
            with self.subTest(stage=stage,count=count):
                with self.assertRaises(ValueError):reader.original_owner(None,None,None,stage=stage,count=count)
                with self.assertRaises(ValueError):reader.sdl_owner(None,None,None,None,None,stage=stage,count=count)

    def gate(self,sequence):
        frames=[{"path":pose,"index":i,"sha256":str(i),"wgc_timestamp_100ns":ms*10000} for i,(pose,ms) in enumerate(sequence)]
        with patch.object(runner.Image,"open",side_effect=lambda pose:pose),\
             patch.object(runner,"normalize",side_effect=lambda pose:pose),\
             patch.object(fixture,"pose_match",side_effect=lambda frame,poses,index:{"matched":True} if frame==index else None):
            return runner.continuous_gate(frames,[])

    def test_continuous_gate_requires_ordered_real_plateaus_and_does_not_join_gaps(self):
        good=[(0,0),(1,100),(1,350),(2,500),(2,750),(3,900),(3,1150),(4,1300),(4,1330)]
        report=self.gate(good)
        self.assertEqual([r["camera"] for r in report["ordered_corners"]],[[0,0],[28,0],[28,32],[0,32]])
        self.assertFalse(report["first1tick_exact_native_phase_claimed"])
        for bad in (
            [(2,100),(2,350),(1,500),(1,750),(3,900),(3,1150),(4,1300),(4,1330)],
            [(1,100),(1,250)]+good[3:],
            [(1,100),(None,150),(1,350)]+good[3:],
            good[:-1]):
            with self.subTest(sequence=bad),self.assertRaises(AssertionError):self.gate(bad)
        calls=[n for n in ast.walk(ast.parse(Path(runner.__file__).read_text(encoding='utf-8')))
            if isinstance(n,ast.Call) and isinstance(n.func,ast.Name) and n.func.id=='series']
        self.assertEqual(len(calls),1)
        self.assertEqual([(k.arg,k.value.value) for k in calls[0].keywords],[('interval_ms',16)])

    def test_owner_only_excludes_cache_and_original_wait_acted_suffix(self):
        source,metadata=self.source();plain,_,_=fixture.derive(source,metadata)
        records=bytearray(plain[fixture.UNITS:fixture.UNITS+18*80]);records[2]^=3
        state={"stage":20,"count":18,"cached_result":0,"event_active":0,"camera":[3,15],"focus":[7,19],"records_hex":records.hex()}
        self.assertTrue(runner.check_owner(state,plain,original=True,final=False)["ok"])
        state.update(camera=[0,32],focus=[4,36]);records[5]|=128;state["records_hex"]=records.hex()
        self.assertTrue(runner.check_owner(state,plain,original=True,final=True)["raw0_acted"])
        with self.assertRaises(AssertionError):runner.check_owner(state,plain,original=False,final=True)
        records[5]&=~128;state["records_hex"]=records.hex();state["event_active"]=1
        with self.assertRaises(AssertionError):runner.check_owner(state,plain,original=False,final=True)
        state["event_active"]=0;records[5]|=128
        records[79]^=1;state["records_hex"]=records.hex()
        with self.assertRaises(AssertionError):runner.check_owner(state,plain,original=True,final=True)

    def test_initial_browse_requires_full_focus_and_camera_then_exact_single_Z(self):
        source,metadata=self.source();plain,_,manifest=fixture.derive(source,metadata)
        p={'plain':plain,'original':self,'assets':{'cursor':object()},'manifest':manifest,'poses':[]}
        # 合成START的side并非真实源；只在本测试明确设为已证raw0玩家侧。
        plain=bytearray(plain);plain[fixture.UNITS+6]=2;p['plain']=bytes(plain)
        with patch.object(fixture,'resource_context',return_value=nullcontext()),\
             patch.object(runner,'actor_templates',return_value={'raw_actor':0}) as actor,\
             patch.object(runner.natural,'actor_foreground_resources') as foreground:
            runner.prepare_browse_contract(p)
        actor.assert_called_once_with(self,p['plain'],p['assets'],0);foreground.assert_called_once()
        self.assertEqual(manifest['browse_input_contract']['planned_Z_count'],1)
        self.assertFalse(manifest['browse_input_contract']['extra_retry_keys'])
        for offset,value in ((fixture.UNITS+5,128),(fixture.UNITS+6,0),(fixture.UNITS,8),(fixture.META+3,4)):
            invalid=bytearray(p['plain']);invalid[offset]=value
            with self.subTest(offset=offset),self.assertRaises(ValueError):
                runner.prepare_browse_contract({**p,'plain':bytes(invalid)})
        focus={'focus_screen':[100,100]};pose={'camera':[3,15]}
        for raw,after_Z in ((focus,False),(focus,True),(None,False),({'focus_screen':[76,100]},True)):
            with patch.object(runner.natural,'focus_match',return_value=raw),\
                 patch.object(runner,'focused_actor',return_value=raw),patch.object(fixture,'pose_match',return_value=pose):
                self.assertEqual(runner.browse_ready(None,p,after_Z=after_Z) is not None,raw==focus)
        with patch.object(runner.natural,'focus_match',return_value=focus),patch.object(fixture,'pose_match',return_value=None):
            self.assertIsNone(runner.browse_ready(None,p))
        with tempfile.TemporaryDirectory() as directory:
            work=Path(directory);save=work/'controlled.SAV';save.write_bytes(b'isolated')
            keys=[];labels=[];capture=SimpleNamespace(process=SimpleNamespace(poll=lambda:None),
                key=keys.append,wait=lambda *a,**k:None)
            case={'kind':'original','checkpoints':[],'continuous_wgc':{'frames':[]}}
            prepared={**p,'titles':{3:[None,None,None]},'summary':{},'range_probes':[],'choice':{}}
            merged=[{'source':'actual-merged-timeline'}];watcher=SimpleNamespace(merge=Mock(return_value=merged))
            with patch.object(fixture,'resource_context',return_value=nullcontext()),patch.object(runner,'wait_title'),\
                 patch.object(runner,'wait_state',side_effect=lambda capture,label,*a,**k:labels.append(label)),\
                 patch.object(runner,'observe'),patch.object(runner,'series',return_value=nullcontext()),\
                 patch.object(runner,'FreshPlateauWatcher',return_value=nullcontext(watcher)) as fresh,\
                 patch.object(runner,'continuous_gate',return_value={'ok':True}) as gate:
                runner.exercise(capture,prepared,None,None,work,case,save)
            fresh.assert_called_once();watcher.merge.assert_called_once_with(case['continuous_wgc']['frames'])
            gate.assert_called_once_with(merged,prepared['poses'])
            self.assertEqual(keys,['UP','ENTER','Z','ENTER','ENTER','DOWN','ENTER','ENTER'])
            self.assertLess(labels.index('ticks-z-focus-raw0'),labels.index('ticks-zero-step-selection'))
    def test_real_wait_log_interface_and_serial_capture_call_without_window(self):
        source,metadata=self.source();plain,_,_=fixture.derive(source,metadata)
        with tempfile.TemporaryDirectory() as directory:
            work=Path(directory);save=work/"controlled.SAV";save.write_bytes(b"isolated-save")
            (work/"game.log").write_text("S2 result source ready:\nS2 result source done: result=0\n")
            capture=SimpleNamespace(process=SimpleNamespace(poll=lambda:None),key=lambda key:None)
            case={"kind":"sdl","checkpoints":[],"continuous_wgc":{"frames":[]}}
            prepared={"plain":plain,"original":work,"assets":{},"poses":[]}
            watcher=SimpleNamespace(merge=lambda frames:frames)
            with patch.object(fixture,"resource_context",return_value=nullcontext()),\
                 patch.object(runner,"wait_state"),patch.object(runner,"observe"),\
                 patch.object(runner,"series",return_value=nullcontext()),\
                 patch.object(runner,"FreshPlateauWatcher",return_value=nullcontext(watcher)) as fresh,\
                 patch.object(runner,"continuous_gate",return_value={"ok":True}) as gate:
                runner.exercise(capture,prepared,None,None,work,case,save)
            fresh.assert_not_called();self.assertIs(gate.call_args.args[0],case['continuous_wgc']['frames'])
            self.assertEqual(case['backend_observation']['observation_mode'],'primary16_only')
            self.assertTrue(case["controlled_save_unchanged"])

    def test_SDL_missing_primary_frames_still_fail_the_real_shared_gate_without_fresh_fallback(self):
        with tempfile.TemporaryDirectory() as directory:
            work=Path(directory);save=work/'controlled.SAV';save.write_bytes(b'isolated-save')
            (work/'game.log').write_text('S2 result source ready:\nS2 result source done: result=0\n')
            capture=SimpleNamespace(process=SimpleNamespace(poll=lambda:None),key=lambda key:None)
            case={'kind':'sdl','checkpoints':[],'continuous_wgc':{'frames':[]}}
            prepared={'plain':b'','original':work,'assets':{},'poses':[]}
            with patch.object(fixture,'resource_context',return_value=nullcontext()),patch.object(runner,'wait_state'),\
                 patch.object(runner,'observe'),patch.object(runner,'series',return_value=nullcontext()),\
                 patch.object(runner,'FreshPlateauWatcher') as fresh:
                with self.assertRaisesRegex(AssertionError,'缺少有序'):runner.exercise(capture,prepared,None,None,work,case,save)
            fresh.assert_not_called();self.assertNotIn('continuous_gate',case)



if __name__=="__main__":unittest.main()
