"""G3音乐量测的前缀/owner绑定/不写Save/候选隔离；纯mock，不开GUI或attach。"""
from pathlib import Path
from contextlib import contextmanager
import copy
import json
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock,patch
import wave

from PIL import Image

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_g3_audio_runtime_check as runner
import run_g3_prepare_check as base
import run_g4_detail_check as detail


class AudioRuntimeCheckTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prepared={name:runner.prepare(ROOT,name) for name in runner.SCENARIOS}

    def test_prefix_is_original_finalized_path_through_true_wait_without_recording_or_ack(self):
        for scenario,p in self.prepared.items():
            with self.subTest(scenario=scenario):
                original=base.prepare(ROOT,scenario);base.finalize_ready_waits(original,scenario)
                final=next(i for i,step in enumerate(original['steps']) if step.get('name')==p['measurement_checkpoint'])
                wanted=[step for step in original['steps'][:final+1] if 'collector' not in step]
                self.assertEqual(p['steps'][:-1],wanted[:-1])
                self.assertEqual({k:v for k,v in p['steps'][-1].items() if k!='reason'},
                    {k:v for k,v in wanted[-1].items() if k!='reason'})
                self.assertIn('不启录音',p['steps'][-1]['reason'])
                self.assertIsNone(p['steps'][-1]['key']);self.assertEqual(p['steps'][-1]['exact_portrait_phase'],3)
                self.assertEqual(set(p['checks']),{step['name'] for step in wanted})
                self.assertFalse(any('save' in step or 'collector' in step or step.get('key')=='HOST_W' for step in wanted))
                self.assertNotIn('service-returned',[step['name'] for step in wanted])
                self.assertFalse(p['runtime_protocol']['manual_post_Save_claimed'])
                self.assertEqual((len(p['encoded']),len(p['plain'])),(22987,22987))
        self.assertEqual(self.prepared['p3-revive']['expected_music'],{'track':11,'loop':1,'status':4,'intermediate_track':17})
        self.assertEqual(self.prepared['p4-promote-normal']['expected_music'],{'track':11,'loop':0,'status':4,'intermediate_track':16})
        with self.assertRaises(ValueError):runner.prepare(ROOT,'p3-revive-insufficient')

    def test_prepare_only_freezes_real_sources_and_never_requests_window_or_observer(self):
        with tempfile.TemporaryDirectory(prefix='g3-music-prepare-') as tmp:
            args=SimpleNamespace(root=ROOT,artifacts=Path(tmp)/'artifacts',scenario='p3-revive',
                driver=None,dosbox=None,sdl=None,cdb=None,pdb=None,wav_evidence=None,prepare_only=True,original_only=False)
            with patch.object(runner,'owned_job_required') as job,patch.object(runner,'running') as launch,\
                patch.object(runner.snapshot,'OwnedRPC') as rpc,patch.object(runner.cdb_music,'take') as cdb:
                report=runner.run(args)
            self.assertTrue(report['final_sources_unchanged']);self.assertFalse(report['ok'])
            self.assertEqual(report['status'],'ready_not_executed');self.assertEqual(report['cases'],[])
            self.assertFalse(report['new_recording_started']);self.assertFalse(report['manual_post_Save_claimed'])
            self.assertEqual((args.artifacts/'controlled.SAV').read_bytes(),self.prepared['p3-revive']['encoded'])
            for operation in (job,launch,rpc,cdb):operation.assert_not_called()
            for module in ('run_g3_audio_runtime_check.py','fd2_g3_audio_observation.py','fd2_cdb_bgm_snapshot.py',
                'fd2_dosbox_snapshot.py','fd2_cdb_prepare_snapshot.py'):
                self.assertIn(str((ROOT/'tools'/module).resolve()),report['inputs'])

    def test_measurement_requires_last_independent_png_and_live_same_pid_last_input(self):
        with tempfile.TemporaryDirectory(prefix='g3-music-binding-') as tmp:
            work=Path(tmp);image=work/'true-wait.png';Image.new('RGB',(4,4),'red').save(image)
            process=Mock(pid=123);process.poll.return_value=None
            capture=SimpleNamespace(process=process,artifacts=work,trace=[{'delivery':{'pid':123,'hwnd':456}}])
            prepared={'measurement_checkpoint':'true-wait','checks':{'true-wait':(lambda frame: {} if frame.getpixel((0,0))==(255,0,0) else None,{})}}
            case={'checkpoints':[{'name':'true-wait','pid':123}]}
            binding=runner.checkpoint_binding(capture,prepared,case)
            self.assertEqual((binding['pid'],binding['last_input_hwnd']),(123,456))
            self.assertTrue(binding['source_ROI_rechecked']);self.assertFalse(binding['no_business_keys_during_observation'])
            bad=copy.deepcopy(case);bad['checkpoints'][-1]['pid']=999
            with self.assertRaises(ValueError):runner.checkpoint_binding(capture,prepared,bad)
            process.poll.return_value=0
            with self.assertRaises(ValueError):runner.checkpoint_binding(capture,prepared,case)
            process.poll.return_value=None;Image.new('RGB',(4,4),'blue').save(image)
            with self.assertRaises(ValueError):runner.checkpoint_binding(capture,prepared,case)

    def exercise_fixture(self,work,kind='original'):
        process=Mock(pid=123);process.poll.return_value=None
        capture=SimpleNamespace(process=process,artifacts=work,trace=[])
        case={'kind':kind,'inputs':[],'checkpoints':[],'rpc':{'pid':123,'tcp_owner_verified':True},'cdb_music':{}}
        save=work/'FD2.SAV';save.write_bytes(b'whole original unknown bytes')
        guard=[save.read_bytes(),save.stat().st_mtime_ns]
        args=SimpleNamespace(driver=Path('driver.exe'),cdb=Path('cdb.exe'),sdl=Path('fd2sdl.exe'),pdb=Path('fd2sdl.pdb'))
        p={'original':ROOT/'original_game','expected_music':{'track':11,'loop':1,'intermediate_track':17}}
        source={'g3_music_call_sites':[{'track':17,'loop':1,'runtime_PC_hit_claimed':False}]}
        observation={'ok':True,'sources':source,'scope':{}}
        def prefix(*a,**kw):case['business_sequence_completed']=True
        binding={'pid':123,'input_count':0}
        return capture,p,save,guard,case,args,observation,prefix,binding

    def test_original_is_single_live_rpc_snapshot_and_prefix_is_not_full_save(self):
        with tempfile.TemporaryDirectory(prefix='g3-music-original-') as tmp:
            work=Path(tmp);capture,p,save,guard,case,args,observation,prefix,binding=self.exercise_fixture(work)
            rpc=Mock();rpc.snapshot.return_value=(b'original RAM',{'DS_descriptor_before':{'base':0}})
            with patch.object(base,'exercise',side_effect=prefix),patch.object(runner,'seal_visual_artifacts'),\
                patch.object(runner,'visual_artifacts_unchanged',return_value=True),patch.object(runner,'checkpoint_binding',return_value=binding),\
                patch.object(runner.original_music,'observe_music',return_value=observation) as observe:
                runner.exercise(capture,p,save,guard,case,rpc,work,args)
            rpc.snapshot.assert_called_once_with(work,'music-wait')
            observe.assert_called_once_with(b'original RAM',p['original'],{'base':0},11,1)
            self.assertTrue(case['source_prefix_completed']);self.assertNotIn('business_sequence_completed',case)
            self.assertFalse(case['business_full_Save_completed']);self.assertEqual(save.read_bytes(),guard[0])
            self.assertTrue(case['measurement']['no_business_keys_during_observation'])
            self.assertTrue(case['music_observation']['scope']['actual_RPC_owner_revalidated_in_this_run'])
            self.assertFalse(case['intermediate_music']['intermediate_owner_observed'])
            self.assertFalse(case['intermediate_music']['runtime_PC_hit_claimed'])

    def test_wrong_owner_and_an_extra_business_key_cannot_be_signed_by_good_music(self):
        for wrong_owner in (False,True):
            with self.subTest(wrong_owner=wrong_owner),tempfile.TemporaryDirectory(prefix='g3-music-guard-') as tmp:
                work=Path(tmp);capture,p,save,guard,case,args,observation,prefix,binding=self.exercise_fixture(work)
                rpc=Mock()
                def sample(*args):
                    if wrong_owner:case['rpc']['pid']=999
                    else:capture.trace.append({'key':'ENTER'})
                    return b'RAM',{'DS_descriptor_before':{}}
                rpc.snapshot.side_effect=sample
                with patch.object(base,'exercise',side_effect=prefix),patch.object(runner,'seal_visual_artifacts'),\
                patch.object(runner,'visual_artifacts_unchanged',return_value=True),patch.object(runner,'checkpoint_binding',return_value=binding),\
                    patch.object(runner.original_music,'observe_music',return_value=observation):
                    with self.assertRaises(ValueError):runner.exercise(capture,p,save,guard,case,rpc,work,args)
                self.assertNotIn('measurement',case)

    def test_sdl_uses_real_music_take_and_does_not_create_a_prepare_dump(self):
        with tempfile.TemporaryDirectory(prefix='g3-music-sdl-') as tmp:
            work=Path(tmp);capture,p,save,guard,case,args,observation,prefix,binding=self.exercise_fixture(work,'sdl')
            def take(cdb,target,exe,pdb,output,record,original,track,loop):
                self.assertFalse(output.exists());record['target_pid']=target.pid
                return observation,record
            with patch.object(base,'exercise',side_effect=prefix),patch.object(runner,'seal_visual_artifacts'),\
                patch.object(runner,'visual_artifacts_unchanged',return_value=True),patch.object(runner,'checkpoint_binding',return_value=binding),\
                patch.object(runner.cdb_music,'take',side_effect=take) as observer,\
                patch.object(runner.snapshot,'resolve') as resolve:
                runner.exercise(capture,p,save,guard,case,None,work,args)
            observer.assert_called_once_with(args.cdb,capture.process,args.sdl,args.pdb,work/'music-observer',
                case['cdb_music'],p['original'],11,1)
            resolve.assert_not_called();self.assertNotIn('runtime_state_comparison',case)

    def test_bad_music_keeps_observation_and_never_promotes_static_intermediate_inference(self):
        with tempfile.TemporaryDirectory(prefix='g3-music-mismatch-') as tmp:
            work=Path(tmp);capture,p,save,guard,case,args,observation,prefix,binding=self.exercise_fixture(work)
            observation['ok']=False;observation['checks']={'loop_matches':False}
            rpc=Mock();rpc.snapshot.return_value=(b'RAM',{'DS_descriptor_before':{}})
            with patch.object(base,'exercise',side_effect=prefix),patch.object(runner,'seal_visual_artifacts'),\
                patch.object(runner,'visual_artifacts_unchanged',return_value=True),patch.object(runner,'checkpoint_binding',return_value=binding),\
                patch.object(runner.original_music,'observe_music',return_value=observation):
                with self.assertRaises(AssertionError):runner.exercise(capture,p,save,guard,case,rpc,work,args)
            self.assertEqual(json.loads((work/'music-observation.json').read_text(encoding='utf-8'))['checks'],{'loop_matches':False})
            self.assertNotIn('intermediate_music',case)

    def test_old_wav_is_a_sealed_candidate_never_a_reused_runtime_or_track_gate(self):
        p=self.prepared['p3-revive']
        with tempfile.TemporaryDirectory(prefix='g3-music-old-wav-') as tmp:
            folder=Path(tmp);original=folder/'original';original.mkdir();wav=original/'old.wav'
            with wave.open(str(wav),'wb') as stream:
                stream.setparams((2,2,44100,0,'NONE','not compressed'));stream.writeframes(bytes(40))
            case={'kind':'original','ok':True,'collection_completed':True,'guards':{'cleanup':True,'sources_unchanged':True},
                'native_audio':{'files':[{'path':str(wav),'sha256':runner.sha(wav),'frames':10,'sample_rate':44100}]}}
            base.write_json(original/'comparison.json',case);(folder/'controlled.SAV').write_bytes(p['encoded'])
            report={'scenario':p['manifest']['scenario'],'final_sources_unchanged':True,'original_before':{},
                'display_requirement':{'released':True},'cases':[case],'ok':False}
            path=folder/'comparison.json';base.write_json(path,report)
            candidates=runner.wav_candidates(path,p,{})
            self.assertEqual(len(candidates['recording_candidates']),1)
            self.assertFalse(candidates['runtime_gate_reused']);self.assertFalse(candidates['recording_candidates'][0]['track_or_sample_identification_claimed'])
            wav.write_bytes(wav.read_bytes()+b'changed')
            with self.assertRaises(ValueError):runner.wav_candidates(path,p,{})

    def test_run_keeps_observer_failure_primary_and_never_signs_completed_prefix_as_success(self):
        with tempfile.TemporaryDirectory(prefix='g3-music-failure-report-') as tmp:
            folder=Path(tmp);driver=folder/'driver.exe';dosbox=folder/'dosbox.exe'
            driver.write_bytes(b'fake owned driver');dosbox.write_bytes(b'fake owned executable')
            args=SimpleNamespace(root=ROOT,artifacts=folder/'run',scenario='p3-revive',driver=driver,dosbox=dosbox,
                sdl=None,cdb=None,pdb=None,wav_evidence=None,prepare_only=False,original_only=True)
            @contextmanager
            def display(record):
                try:yield
                finally:record['released']=True
            @contextmanager
            def running(command,cwd,env,log,driver,receipts):
                process=Mock(pid=123);process.poll.return_value=None
                try:yield process
                finally:receipts.append({'pid':123,'cleaned':True,'forced_cleanup':False,'exit_code':0})
            @contextmanager
            def rpc(record):
                try:yield SimpleNamespace(port=12345,bind=Mock())
                finally:record['closed']=True;raise RuntimeError('secondary close failed')
            def exercise(capture,prepared,save,guard,case,*args):
                case['source_prefix_completed']=True;case['business_full_Save_completed']=False
                raise ValueError('primary observed owner mismatch')
            with patch.object(runner,'owned_job_required'),patch.object(runner,'windows_display_requirement',display),\
                patch.object(runner,'running',running),patch.object(runner.snapshot,'OwnedRPC',rpc),\
                patch.object(runner,'Capture'),patch.object(runner,'exercise',side_effect=exercise):
                report=runner.run(args)
            self.assertFalse(report['ok']);self.assertFalse(report['original_case_passed'])
            self.assertEqual(report['status'],'failed');self.assertEqual(report['error']['message'],'primary observed owner mismatch')
            self.assertEqual(report['secondary_errors'][0]['error']['message'],'secondary close failed')
            case=report['cases'][0];self.assertTrue(case['source_prefix_completed']);self.assertFalse(case['ok'])
            self.assertFalse(case['business_full_Save_completed']);self.assertTrue(case['guards']['whole_Save_and_mtime_unchanged'])
            self.assertEqual(case['initial_Save']['sha256'],case['final_Save']['sha256'])
            self.assertEqual(case['initial_Save']['mtime_ns'],case['final_Save']['mtime_ns'])

    def visual_prefix(self,work,scenario='p3-revive',kind='original',bad=None):
        """真实base.exercise与detail_series；只替换owned进程/投键/普通页面等待。"""
        prepared=copy.deepcopy(self.prepared[scenario]);point=prepared['measurement_checkpoint']
        prepared['checks'][point][1]['timeout']=.025
        opens=point.removesuffix('-real-wait');steps=prepared['steps'];events=[];keys=[];clock={'now':0}
        process=SimpleNamespace(pid=42,poll=lambda:None)
        capture=SimpleNamespace(process=process,artifacts=work,driver=Path('driver.exe'),trace=[],last=None)
        case={'kind':kind,'checkpoints':[],'rpc':{'pid':42,'tcp_owner_verified':True},'cdb_music':{}}
        capture.g3_case_record=case;directory=work/'detail-series';rows=[];helper_process=SimpleNamespace(poll=lambda:None)
        save=work/'FD2.SAV';save.write_bytes(prepared['encoded']);guard=[save.read_bytes(),save.stat().st_mtime_ns]
        fragment=588 if scenario=='p3-revive' else 591
        expected=base.pages.notice(prepared['assets'],fragment,selector=4)
        def publish(count):
            selected=copy.deepcopy(rows[:count])
            if bad=='row_owner' and count>1:selected[1]['hwnd']='0x456'
            (directory/'frames.jsonl').write_text(''.join(json.dumps(row)+'\n' for row in selected),encoding='utf-8')
        class Helper:
            def __init__(self,driver,arguments,artifacts,label,record):
                self.record=record
                self.args=arguments
                self.stdout=work/(label+'.stdout.json');self.stderr=work/(label+'.stderr.log')
            def start(self):
                events.append('ready');directory.mkdir()
                for index,phase in enumerate((0,0 if bad in ('phase0','timeout_stop') else 3,0)):
                    canvas=expected['text'].copy();image,xy,_=next(row for row in expected['portrait_frames'] if row[2]==phase)
                    canvas.paste(image,xy);canvas=canvas.convert('RGB')
                    if bad=='wrong_text' and index==1:
                        at=next((x,y) for y in range(119,198) for x in range(320) if expected['text'].getpixel((x,y)))
                        rgb=canvas.getpixel(at);canvas.putpixel(at,tuple(255-c for c in rgb))
                    path=directory/f'frame{index:05d}.png';canvas.save(path)
                    rows.append({'index':index,'file':path.name,'pid':42,'hwnd':'0x123',
                        'wgc_timestamp_100ns':100000+330000*index,'client_width':320,'client_height':200})
                ready={'ready':True,'pid':42,'hwnd':'0x456' if bad=='ready_owner' else '0x123',
                    'first_index':0,'wgc_timestamp_100ns':100000}
                (directory/'ready.json').write_text(json.dumps(ready),encoding='utf-8')
                publish(0 if bad=='no_baseline' else 1)
                self.record.update(stdout=str(self.stdout),stderr=str(self.stderr),cleaned=False,forced_cleanup=False)
                self.stdout.write_text('{}',encoding='utf-8');self.stderr.write_text('',encoding='utf-8')
                test.assertEqual(self.args[1:],["capture-series",42,directory,45000,33])
                return helper_process
            def finish(self):
                events.append('stop');test.assertTrue((directory/'stop').is_file())
                if bad=='extra_key_after_gate':key('SPACE')
                if bad in ('stop','timeout_stop'):raise RuntimeError('visual normal stop failed')
                if bad=='raw_changed':(directory/'frame00001.png').write_bytes((directory/'frame00000.png').read_bytes())
                if bad=='baseline_changed':(directory/'frame00000.png').write_bytes((directory/'frame00001.png').read_bytes())
                count=len((directory/'frames.jsonl').read_text(encoding='utf-8').splitlines())
                summary={'ok':True,'backend':'WGC-HWND-session','pid':42,'hwnd':'0x123','ready_published':True,
                    'session_started':True,'stop_reason':'frame_limit' if bad=='cap' else 'stop','hresult':0,
                    'color_filter':False,'frames':count,'first_wgc_timestamp_100ns':rows[0]['wgc_timestamp_100ns'],
                    'last_wgc_timestamp_100ns':rows[count-1]['wgc_timestamp_100ns'] if count else 0}
                (directory/'summary.json').write_text(json.dumps(summary),encoding='utf-8')
                self.stdout.write_text(json.dumps(summary),encoding='utf-8')
                self.stderr.write_text('fd2 job: root=17 worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1 result=0',encoding='utf-8')
                self.record.update(summary=summary,job=runner.parse_job(self.stderr.read_text(encoding='utf-8')),
                    cleaned=True,forced_cleanup=False,exit_code=0)
                return summary
        opening_index=next(i for i,step in enumerate(steps) if step['name']==opens)
        expected_keys=[step['key'] for step in steps if step.get('key')]
        opening_key_index=len([step for step in steps[:opening_index+1] if step.get('key')])
        def key(value):
            keys.append(value);events.append('key:'+value)
            capture.trace.append({'pid':42,'key':value,'time':clock['now']/1000,
                'delivery':{'pid':42,'hwnd':'0x123','backend':'PostMessageW HWND','kind':'key','ok':True,'error':0,
                    'keys':1,'sent':1,'release':1,'key_down_sent':True,'key_up_sent':True}})
            if len(keys)==opening_key_index:publish(3)  # 闭眼在原ENTER返回前发布，末帧又为0。
        capture.key=key
        def stamp():clock['now']+=1;return {'qpc':clock['now'],'qpc_frequency':1000,'host_monotonic':clock['now']/1000}
        def ordinary(capture,name,predicate,points,**options):
            events.append('page:'+name)
            if name==opens and bad=='extra_key':key('SPACE')
        test=self;rpc=Mock()
        observation={'ok':True,'sources':{'g3_music_call_sites':[{'track':prepared['expected_music']['intermediate_track']}]},'scope':{}}
        def sample(*args):
            events.append('sample');test.assertEqual(events[-2],'stop')
            test.assertTrue(case['ready_wait_visual_only_completed']);test.assertNotIn('collection_completed',case)
            if bad=='mutate_after_seal':(directory/'frames.jsonl').write_bytes(b'changed after closed seal')
            return b'RAM',{'DS_descriptor_before':{}}
        rpc.snapshot.side_effect=sample
        def cdb(*args):sample();case['cdb_music']['target_pid']=42;return observation,{}
        args=SimpleNamespace(driver=capture.driver,cdb=Path('cdb.exe'),sdl=Path('fd2sdl.exe'),pdb=Path('fd2sdl.pdb'))
        with patch.object(base,'wait_title'),patch.object(base,'resource_wait_state',side_effect=ordinary),\
                patch.object(base,'qpc_stamp',side_effect=stamp),patch.object(detail,'OwnedHelper',Helper),\
                patch.object(base,'AnimationCollectors',side_effect=AssertionError('audio不得创建PCM collector')),\
                patch.object(runner.original_music,'observe_music',return_value=observation),\
                patch.object(runner.cdb_music,'take',side_effect=cdb) as cdb_take:
            failure=None
            try:runner.exercise(capture,prepared,save,guard,case,rpc,work,args)
            except BaseException as error:failure=error
        return case,capture,events,keys,expected_keys,rpc,cdb_take,failure

    def test_real_audio_prefix_ready_prekey_early_blink_closed_stop_then_one_owner_sample(self):
        for scenario in runner.SCENARIOS:
            for kind in ('original','sdl'):
                with self.subTest(scenario=scenario,kind=kind),tempfile.TemporaryDirectory() as tmp:
                    case,capture,events,keys,wanted,rpc,cdb,error=self.visual_prefix(Path(tmp),scenario,kind)
                    self.assertIsNone(error);self.assertEqual(keys,wanted);self.assertNotIn('HOST_W',keys)
                    self.assertEqual(events[-2:],['stop','sample']);self.assertEqual(events.count('ready'),1)
                    self.assertEqual(events.count('sample'),1);self.assertTrue(case['ready_wait_visual_only_completed'])
                    self.assertNotIn('collection_completed',case);self.assertNotIn('audio_capture',case)
                    self.assertFalse(case['business_full_Save_completed']);self.assertTrue(case['ready_wait_closed_series_bound'])
                    self.assertEqual(case['checkpoints'][-1]['series_frame']['index'],1)
                    self.assertEqual(case['checkpoints'][-1]['portrait_match'][0]['checked_pixels'],6400)
                    self.assertLess(events.index('ready'),events.index('page:'+case['ready_wait_boundary']['opens_step']))
                    self.assertTrue(runner.visual_artifacts_unchanged(case))
                    if kind=='original':rpc.snapshot.assert_called_once();cdb.assert_not_called()
                    else:rpc.snapshot.assert_not_called();cdb.assert_called_once()

    def test_real_audio_bad_visual_scope_never_samples_or_adds_ack(self):
        for bad in ('phase0','wrong_text','ready_owner','row_owner','raw_changed','baseline_changed',
                'no_baseline','extra_key','extra_key_after_gate','cap','stop','timeout_stop'):
            with self.subTest(bad=bad),tempfile.TemporaryDirectory() as tmp:
                case,capture,events,keys,wanted,rpc,cdb,error=self.visual_prefix(Path(tmp),bad=bad)
                self.assertIsNotNone(error);rpc.snapshot.assert_not_called();cdb.assert_not_called()
                self.assertNotIn('sample',events);self.assertNotIn('HOST_W',keys)
                self.assertFalse(case.get('business_full_Save_completed',False))
                self.assertNotIn('measurement',case)
                self.assertEqual(keys,wanted[:-1] if bad in ('no_baseline','ready_owner') else
                    wanted+['SPACE'] if bad in ('extra_key','extra_key_after_gate') else wanted)
                if bad=='timeout_stop':
                    report={'cases':[case]};base.record_failure(report,error)
                    self.assertEqual(report['error']['type'],'TimeoutError')
                    self.assertEqual(case['detail_series']['cleanup_error'],'visual normal stop failed')

    def test_all_visual_raw_metadata_and_helper_files_are_guarded_against_late_change(self):
        for leaf in ('frame00000.png','frames.jsonl','ready.json','summary.json',
                'detail-series-helper.stdout.json','detail-series-helper.stderr.log'):
            with self.subTest(leaf=leaf),tempfile.TemporaryDirectory() as tmp:
                case,capture,events,keys,wanted,rpc,cdb,error=self.visual_prefix(Path(tmp))
                self.assertIsNone(error)
                path=next(Path(name) for name in case['visual_artifacts']['sealed_files'] if Path(name).name==leaf)
                path.write_bytes(path.read_bytes()+b'tampered')
                self.assertFalse(runner.visual_artifacts_unchanged(case))
        with tempfile.TemporaryDirectory() as tmp:
            case,capture,events,keys,wanted,rpc,cdb,error=self.visual_prefix(Path(tmp),bad='mutate_after_seal')
            self.assertIsInstance(error,ValueError);self.assertNotIn('measurement',case)


if __name__=='__main__':unittest.main()
