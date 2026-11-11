"""精确AI item56 miss的原资源／纯协议负例，不启动WGC、窗口或设备。"""
import copy
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch
import wave
from contextlib import contextmanager
from types import SimpleNamespace

from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import fd2_ai_item_miss_oracle as oracle
import run_original_battle_animation as capture
from run_original_end_turn import META


class ItemMissTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prepared=capture.prepare_runtime(ROOT/'original_game','ai-item-score20')
        cls.material=oracle.resources(ROOT/'original_game',cls.prepared)

    def series(self,folder,fault=None):
        directory=folder/'animation-series';directory.mkdir()
        images=[]
        background=Image.new('RGB',(320,200),(13,17,29))
        images.append(background.copy())
        for identity in oracle.MAP_FRAMES:
            image=background.copy();part=self.material['fx'][identity]
            image.paste(part.convert('RGB'),(52,70),part.point(lambda i:255 if i else 0,mode='L'))
            images.append(image)
        for phase in (9,20):
            image=background.copy();part=self.material['words'][phase]
            image.paste(part.convert('RGB'),(52,66),part.point(lambda i:255 if i else 0,mode='L'))
            images.append(image)
        if fault=='missing_fx':images.pop(2)  # 缺最强51帧，不能只剩MISS就通过。
        elif fault=='wrong_target':
            images=[image.transform(image.size,Image.Transform.AFFINE,(1,0,-24,0,1,0)) for image in images]
        elif fault=='wrong_trajectory':images[-1]=images[-2].copy()
        elif fault=='missing_letter':images[-2].paste((0,0,0),(54,74,61,81))
        rows=[]
        for index,image in enumerate(images):
            path=directory/f'frame{index:05d}.png';image.save(path)
            rows.append({'index':index,'file':path.name,'pid':42,'hwnd':'0x123',
                'wgc_timestamp_100ns':100000+index*330000,'client_width':320,'client_height':200})
        (directory/'frames.jsonl').write_text(''.join(json.dumps(row)+'\n' for row in rows),encoding='utf-8')
        ready={'ready':True,'pid':42,'hwnd':'0x123','first_index':0,'wgc_timestamp_100ns':100000}
        (directory/'ready.json').write_text(json.dumps(ready),encoding='utf-8')
        summary={'ok':True,'backend':'WGC-HWND-session','pid':42,'hwnd':'0x123','ready_published':True,
            'session_started':True,'stop_reason':'stop','hresult':0,'color_filter':False,'frames':len(rows),
            'first_wgc_timestamp_100ns':100000,'last_wgc_timestamp_100ns':rows[-1]['wgc_timestamp_100ns']}
        summary_path=directory/'summary.json';summary_path.write_text(json.dumps(summary),encoding='utf-8')
        closed=capture.series_index(directory,summary,ready,42)
        return ready,summary,summary_path,closed

    def case(self,folder,fault=None):
        ready,summary,summary_path,rows=self.series(folder,fault)
        stderr=folder/'helper.log';stderr.write_text('fd2 job: root=1 worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1 result=0\n')
        plain=bytearray(self.prepared['base_final']);plain[META+5:META+9]=bytes((6,14,1,3))
        plain[-4:]=sum(plain[:-4]).to_bytes(4,'little')
        encoded=bytes(a^b^c for a,b,c in zip(self.prepared['encoded'],self.prepared['source'],plain))
        saved=folder/'measured.SAV';saved.write_bytes(encoded)
        live=folder/'FD2.SAV';live.write_bytes(encoded)
        os.utime(live,ns=(saved.stat().st_atime_ns,saved.stat().st_mtime_ns))
        wav_path=folder/'mix.wav'
        with wave.open(str(wav_path),'wb') as stream:
            stream.setnchannels(2);stream.setsampwidth(2);stream.setframerate(48000);stream.writeframes(b'\x01\0\x02\0'*80)
        app_ready={'pid':42,'ready':True}
        app_summary={'pid':42,'ok':True,'callback_quiescent':True,'wave_finalized':True,'stop_reason':'stop'}
        facts={}
        for label,value in (('ready',app_ready),('summary',app_summary)):
            path=folder/('mix-'+label+'.json');path.write_text(json.dumps(value),encoding='utf-8')
            facts[label+'_fact']={'path':str(path),'sha256':oracle.sha(path)}
        delivery={'pid':42,'hwnd':'0x123','ok':True}
        case={'kind':'sdl','business_sequence_completed':True,'collection_completed':True,
            'processes':[{'pid':42,'cleaned':True,'forced_cleanup':False,'exit_code':0}],
            'series_helper':{'cleaned':True,'forced_cleanup':False,'exit_code':0,'stderr':str(stderr),
                'job':capture.parse_job(stderr.read_text()),'summary':summary},
            'animation_capture':{'summary_path':str(summary_path),'summary_sha256':oracle.sha(summary_path),
                'summary':summary,'ready':ready,'frames':rows},
            'animation_action':{'key':'ENTER','confirmations':1,'delivery':delivery,
                'key_call_qpc_bracket':{'before':{'qpc':100001}}},'inputs':[{'delivery':delivery}],
            'completed_save':{'snapshot':str(saved),'sha256':oracle.sha(saved),'mtime_ns':live.stat().st_mtime_ns},
            'ai_item_action_positive_evidence':{'passed':False},
            'audio_capture':{'wav':{'path':str(wav_path),'sha256':oracle.sha(wav_path),
                'sample_rate':48000,'frames':80,'data_bytes':320}},
            'app_mix_capture':{'ready':app_ready,'summary':app_summary,**facts}}
        return case

    def test_real_source_only_and_exclusive_fixture_rejects_competing_actions(self):
        self.assertEqual(self.material['glyphs'],[116,117,118,118])
        self.assertEqual(oracle.MAP_FRAMES,tuple(range(50,57)))
        p=self.prepared
        for raw,offset,value in ((11,26,1),(11,10,64),(11,59,1),(11,72,1),(4,5,0),(0,0,8),(0,64,49)):
            source=bytearray(p['source']);source[oracle.UNITS+raw*80+offset]=value
            with self.subTest(raw=raw,offset=offset),self.assertRaises((AssertionError,ValueError)):
                oracle._exclusive(source,p['items'][56*23:57*23],p['magic'],p['contract']['ai_plan'])

    def test_complete_source_sequence_passes_but_missing_or_wrong_visual_is_inconclusive(self):
        for fault in (None,'missing_fx','wrong_target','wrong_trajectory','missing_letter'):
            with self.subTest(fault=fault),tempfile.TemporaryDirectory() as directory:
                folder=Path(directory);_,_,_,rows=self.series(folder,fault)
                if fault:
                    with self.assertRaises(oracle.MissingActionWitness):oracle.check_visual(rows,self.material,100001)
                else:
                    found=oracle.check_visual(rows,self.material,100001)
                    self.assertEqual([x['frame'] for x in found['MAP0_source_frames']],list(range(50,57)))
                    self.assertEqual([x['source']['index'] for x in found['MISS_complete_word_poses']],[8,9])
                    self.assertTrue(found['source_first49_not_required_or_claimed'])
                    self.assertFalse(found['all22_trajectory_frames_claimed'])

    def test_complete_closed_case_keeps_HP_false_and_has_no_external_effects(self):
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory);case=self.case(folder);before=copy.deepcopy(case)
            with patch.object(oracle,'resources',return_value=self.material):
                result=oracle.observe_case(ROOT/'original_game',self.prepared,case,folder)
            self.assertTrue(result['passed'] and result['full22987B_native_finite_Save_verified'])
            self.assertFalse(result['HP_positive_passed'] or result['outer_Job_or_final_producer_guards_claimed'])
            self.assertEqual(case,before)

    def test_closed_owner_SHA_Save_and_normal_cleanup_errors_are_fatal(self):
        for fault in ('running','forced','pid','hwnd','png','order','save','audio','guard'):
            with self.subTest(fault=fault),tempfile.TemporaryDirectory() as directory:
                folder=Path(directory);case=self.case(folder)
                if fault=='running':case['processes'][0]['cleaned']=False
                elif fault=='forced':case['series_helper']['forced_cleanup']=True
                elif fault=='pid':case['animation_action']['delivery']['pid']=43
                elif fault=='hwnd':case['animation_action']['delivery']['hwnd']='0x456'
                elif fault=='png':Path(case['animation_capture']['frames'][3]['path']).write_bytes(b'changed')
                elif fault=='order':case['animation_capture']['frames'][3]['wgc_timestamp_100ns']=0
                elif fault=='save':Path(case['completed_save']['snapshot']).write_bytes(b'incomplete')
                elif fault=='audio':case['app_mix_capture']['summary']['wave_finalized']=False
                else:case['guards']={'sources_unchanged':False}
                with patch.object(oracle,'resources',return_value=self.material),self.assertRaises((AssertionError,ValueError,OSError)):
                    oracle.observe_case(ROOT/'original_game',self.prepared,case,folder)

    def test_collector_runs_miss_observer_after_cleanup_and_missing_proof_still_rejects(self):
        for hit,proof in ((False,True),(False,False),(True,False)):
            with self.subTest(hit=hit,proof=proof),tempfile.TemporaryDirectory() as directory:
                order=[];work=Path(directory)/'new'
                args=SimpleNamespace(root=ROOT,artifacts=work,scenario='ai-item-score20',sdl=None,
                    driver=Path(capture.__file__),dosbox=Path(capture.__file__),audio_recorder=None,
                    verify_core=False,sdl_audio_source='app-mix')
                @contextmanager
                def running(command,cwd,env,log,driver,receipts):
                    receipt={'pid':42,'cleaned':False,'forced_cleanup':False,'exit_code':None}
                    receipts.append(receipt);order.append('start')
                    try:yield SimpleNamespace(pid=42)
                    finally:receipt.update(cleaned=True,exit_code=0);order.append('cleanup')
                def exercise(capture_object,prepared,save,guard,record,options):
                    order.append('measure_HP_true' if hit else 'measure_HP_false')
                    record.update(business_sequence_completed=True,collection_completed=True,
                        ai_item_action_positive_evidence={'passed':hit})
                def observe(original,prepared,record,folder):
                    self.assertTrue(record['processes'][0]['cleaned'])
                    self.assertEqual(order[-1],'cleanup');order.append('observe')
                    if not proof:raise oracle.MissingActionWitness('no complete source evidence')
                    return {'passed':True,'kind':'exclusive_item56_executed_legal_miss'}
                with patch.object(capture,'owned_job_required'),patch.object(capture,'running',running), \
                        patch.object(capture.shutil,'copytree',side_effect=lambda source,destination:destination.mkdir()), \
                        patch.object(capture,'Capture'),patch.object(capture,'exercise',exercise), \
                        patch.object(oracle,'observe_case',side_effect=observe) as observed:
                    report=capture.run(args)
                self.assertEqual(report['collection_passed'],hit or proof)
                self.assertFalse(report['ok'])  # 该runner仍是collection，不冒充全FX/SFX通过。
                if hit:
                    observed.assert_not_called();self.assertEqual(order,['start','measure_HP_true','cleanup'])
                else:
                    self.assertEqual(order,['start','measure_HP_false','cleanup','observe'])
                    self.assertFalse(report['cases'][0]['ai_item_action_positive_evidence']['passed'])
                    self.assertEqual(report['cases'][0]['action_execution_witness']['passed'],proof)
                    if not proof:self.assertEqual(report['status'],'inconclusive')


if __name__=='__main__':unittest.main()
