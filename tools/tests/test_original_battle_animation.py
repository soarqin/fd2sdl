"""B6真实采集前置/有限SAV/owned helper纯回归；不启动任何游戏或helper。"""
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import wave
from contextlib import contextmanager
from types import SimpleNamespace
from unittest.mock import Mock, patch
from PIL import Image

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import run_original_battle_animation as r
import fd2_battle_animation_oracle as o
from run_original_load import measuring_focus
ROOT=Path(__file__).resolve().parents[2]
ORIGINAL=(ROOT/'original_game').resolve()


class BattleFixtureTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.all={name:o.prepare(ORIGINAL,name) for name in o.SCENARIOS}
    def test_exact_19_source_manifest_roundtrip_no_source_writes(self):
        before=r.fingerprint(ORIGINAL)
        for name,p in self.all.items():
            with self.subTest(name=name):
                original=o.decode_save(ORIGINAL/'START.SAV')
                actual=[{'offset':i,'before':a,'after':b} for i,(a,b) in enumerate(zip(original,p['source'])) if a!=b]
                self.assertEqual(actual,p['manifest']['plaintext_patches'])
                self.assertEqual(p['source'][:o.UNITS],original[:o.UNITS])
                self.assertEqual(p['source'][-4:],sum(p['source'][:-4]).to_bytes(4,'little'))
                self.assertEqual(p['manifest']['options']['effect'],0)
                self.assertEqual(p['manifest']['options']['music'],0)
                self.assertEqual(p['manifest']['options']['sfx'],1)
        self.assertEqual(len(self.all),19)
        self.assertEqual(r.fingerprint(ORIGINAL),before)
    def test_ai_actual_single_spell_mov0_priority_and_raw_targets(self):
        for name,p in self.all.items():
            if not name.startswith('ai-full-'):continue
            caster=o.actor(p['source'],11);identity=p['manifest']['magic_id']
            self.assertEqual((caster[6],caster[7],caster[0x3b],o._word(caster,0x48)),(0,9,0,0))
            self.assertEqual(caster[0x34],0);self.assertEqual(caster[0x0a:0x1a],bytes((0x80,0))*8)
            self.assertEqual(p['manifest']['controlled_AI_dispatch']['behavior_after'],0)
            learned=[8*i+bit for i in range(5) for bit in range(8) if caster[0x1a+i]&(1<<bit)]
            self.assertEqual(learned,[identity]);self.assertEqual(p['caster_metadata']['cost_count'],1)
            self.assertEqual(p['contract']['ai_plan']['targets'],[0,2] if name.endswith('multi') else [0])
            self.assertGreaterEqual(p['contract']['ai_plan']['score'],6)
        self.assertEqual(self.all['ai-full-08-multi']['contract']['ai_plan']['center'],[7,14])
    def test_ai_player_behavior_default_and_competing_inventory_are_rejected(self):
        p=self.all['ai-full-00'];source=p['source'];base=o.decode_save(ORIGINAL/'START.SAV')
        self.assertEqual(o.actor(base,1)[0x34],0xce)
        for kind in ('copied-player-behavior','occupied-inventory'):
            data=bytearray(source);at=o.UNITS+11*o.STRIDE
            if kind=='copied-player-behavior':data[at+0x34]=0xce
            else:data[at+0x0a:at+0x0c]=bytes((0x40,52))
            with self.subTest(kind=kind),self.assertRaises(ValueError):o.ai_plan(o._checksum(data),p['magic'])
        projected,_=o.project_final(p,p['base_final'])
        self.assertEqual(o._word(o.actor(source,11),0x44),250)
        self.assertEqual(o._word(o.actor(projected,11),0x44),248) # magic0 cost2仍严格，不白名单MP。

    def test_unsupported_id9_multi_unknown_rejected_before_creation(self):
        for name in ('ai-full-09-multi','ai-full-10','unknown'):
            with self.assertRaises(ValueError):o.derive_fixture(ORIGINAL,name)
    def measurement(self,p,hps=None):
        data=bytearray(p['base_final'])
        for raw,hp in (hps or {}).items():o._store(data,o.UNITS+raw*80+0x40,hp)
        # 先用独立有限域产生其余原期望，不从measured未知区反填。
        fixed,_=o.project_final(p,o._checksum(data));data=bytearray(fixed)
        steps=[('LEFT',r.screen(fixed,(6,14)))]
        data[o.META+5:o.META+9]=measuring_focus(fixed,steps)
        return o._checksum(data),steps
    def test_fixed_physical_and_heal_full_container_and_raw_writes(self):
        for name in ('physical-full-hit','player-heal13'):
            p=self.all[name];measured,steps=self.measurement(p)
            _,detail=o.check_measurement(p,measured,steps)
            self.assertTrue(detail['source_equal_except_declared_cache_and_measuring_focus'])
        physical=self.all['physical-full-hit'];heal=self.all['player-heal13']
        self.assertEqual(o.actor(physical['base_final'],0)[0x3c],99)
        self.assertTrue(o.actor(physical['base_final'],11)[5]&1)
        self.assertEqual(o._word(o.actor(heal['base_final'],0),0x40),42)
        self.assertEqual(o._word(o.actor(heal['base_final'],1),0x44),5)
    def test_player0_each_hp_loss_couples_exact_ex_and_mp(self):
        p=self.all['player-magic0']
        for hp in (999,950,951,952,953,954):
            data,steps=self.measurement(p,{11:hp});_,detail=o.check_measurement(p,data,steps)
            self.assertEqual(o._word(o.actor(data,1),0x44),6)
            self.assertEqual(o.actor(data,1)[0x3c],(999-hp)*42//999)
            wrong=bytearray(data);wrong[o.UNITS+80+0x3c]+=1
            with self.assertRaises(AssertionError):o.check_measurement(p,o._checksum(wrong),steps)
        with self.assertRaises(AssertionError):self.measurement(p,{11:949})
    def test_ai_id8_later_target_keeps_hp_but_cost_once(self):
        p=self.all['ai-full-08-multi'];target=o.actor(p['source'],0)
        scale=struct.unpack_from('<i',p['effects']['damage_profiles'],4*(target[0x20]-1))[0]
        minimum=(struct.unpack_from('<h',p['magic'])[0]*scale//10)*9//10
        data,steps=self.measurement(p,{0:999-minimum,2:999})
        o.check_measurement(p,data,steps)
        self.assertEqual(o._word(o.actor(data,11),0x44),250-p['magic'][5])
        wrong=bytearray(data);o._store(wrong,o.UNITS+2*80+0x40,998)
        with self.assertRaises(AssertionError):o.check_measurement(p,o._checksum(wrong),steps)
    def test_unknown_roster_slots_status_level_flags_and_extra_cost_not_whitelisted(self):
        for name in ('physical-full-hit','player-magic0','ai-full-03-multi'):
            p=self.all[name];hps={raw:999 for raw in p['manifest']['targets']} if name!='physical-full-hit' else None
            data,steps=self.measurement(p,hps)
            for at in (10,0x8a4,o.UNITS+0x28,o.UNITS+0x21,o.UNITS+5,o.UNITS+0x22,o.UNITS+11*80+0x44):
                wrong=bytearray(data);wrong[at]^=1
                with self.subTest(name=name,at=at),self.assertRaises(AssertionError):o.check_measurement(p,o._checksum(wrong),steps)
    def test_bad_measurement_length_or_checksum_rejected_without_struct_error(self):
        p=self.all['player-magic0']
        for data in (b'',p['base_final'][:-1],bytes(len(p['source']))):
            with self.assertRaises(AssertionError):o.project_final(p,data)

    def test_item56_equality_uses_actual_AI_fields_unique_target_and_no_cost(self):
        p=self.all['ai-item-score20'];caster=o.actor(p['source'],11);hero=o.actor(p['source'],0)
        record=p['items'][56*23:57*23]
        self.assertEqual(record.hex(),'0678005a000000000000000101140000020000e0150000')
        self.assertEqual((record[13],o._word(record,14),record[16],record[17],record[18]),(20,0,2,0,0))
        self.assertEqual(record[19],224) # 不把player+19当原AI+13。
        self.assertEqual((o._word(p['magic'],0),p['magic'][2]),(50,90))
        self.assertEqual((hero[32],o._word(hero,64),o._word(hero,66)),(1,50,50))
        self.assertEqual(caster[26:31],bytes(5));self.assertEqual(caster[10:26],bytes((0,56))+bytes((128,0))*7)
        self.assertEqual(p['contract']['ai_plan']['score'],18);self.assertEqual(p['contract']['ai_plan']['targets'],[0])
        self.assertEqual(p['manifest']['action'],'item');self.assertEqual(p['manifest']['item_id'],56)
        self.assertIsNone(p['caster_metadata']);self.assertFalse(p['contract']['ai_plan']['guaranteed_hit'])
        for hp in (1,2,3,4,5,50):
            data,steps=self.measurement(p,{0:hp});o.check_measurement(p,data,steps)
            self.assertEqual(o._word(o.actor(data,11),68),250)
            self.assertEqual(o.actor(data,11)[10:26],caster[10:26])
            evidence=o.item_action_coverage(p,data,steps)
            self.assertEqual(evidence['passed'],hp<50)
            self.assertEqual(evidence['status'],'actual_nonlethal_HP_loss' if hp<50 else 'inconclusive_legal_miss_or_no_action')
        for hp in (0,6,49,51):
            with self.subTest(hp=hp),self.assertRaises(AssertionError):self.measurement(p,{0:hp})

    def test_item_equality_preflight_rejects_competing_actions_and_wrong_boundary(self):
        p=self.all['ai-item-score20'];record=p['items'][56*23:57*23]
        for raw,offset,value in ((11,0x34,14),(11,0x1a,1),(11,0x0d,56),(11,0x3b,1),(11,0x48,1),(0,0x40,49),(0,0x20,2)):
            data=bytearray(p['source']);data[o.UNITS+raw*80+offset]=value
            with self.subTest(raw=raw,offset=offset),self.assertRaises(ValueError):o.ai_item_plan(o._checksum(data),record,p['magic'])
        wrong=bytearray(record);wrong[13]=0
        with self.assertRaises(ValueError):o.ai_item_plan(p['source'],bytes(wrong),p['magic'])
        for at in (10,0x8a4,o.UNITS+0x28,o.UNITS+0x21,o.UNITS+5,o.UNITS+0x22,o.UNITS+11*80+0x44,o.UNITS+11*80+0x0a):
            data,steps=self.measurement(p,{0:5});wrong=bytearray(data);wrong[at]^=1
            with self.subTest(at=at),self.assertRaises(AssertionError):o.check_measurement(p,o._checksum(wrong),steps)

    def test_item_action_gate_stops_after_measured_save_when_only_legal_miss(self):
        p=dict(self.all['ai-item-score20'],assets={})
        for hp in (5,50):
            data,steps=self.measurement(p,{0:hp})
            with tempfile.TemporaryDirectory() as temp:
                save=Path(temp)/'FD2.SAV';save.write_bytes(data);guard=[b'old',0];record={'b6_final_save':{}}
                with patch.object(r,'measure_inventory_save'),patch.object(r,'decode_save',return_value=data):
                    if hp==50:
                        with self.assertRaises(r.ActionCoverageInconclusive):r.measure(SimpleNamespace(),p,save,guard,record,Mock())
                    else:r.measure(SimpleNamespace(),p,save,guard,record,Mock())
                self.assertTrue(record['business_sequence_completed'])
                self.assertEqual(guard,[data,save.stat().st_mtime_ns])
                self.assertEqual(record['ai_item_action_positive_evidence']['passed'],hp<50)

    def test_item_miss_inconclusive_does_not_mask_source_or_cleanup_failure(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);original=folder/'original';original.mkdir();(original/'source').write_bytes(b'readonly')
            fixture=folder/'controlled.SAV';fixture.write_bytes(b'fixture');save=folder/'FD2.SAV';save.write_bytes(b'measured')
            before=r.fingerprint(original);stamp=fixture.stat().st_mtime_ns;guard=[save.read_bytes(),save.stat().st_mtime_ns]
            for failure in ('none','source','cleanup'):
                record={'collection_passed':False,'business_sequence_completed':True,'collection_completed':True,
                    'failure_category':'inconclusive_action_coverage','error':'legal miss: no positive action evidence',
                    'processes':[{'cleaned':True,'forced_cleanup':False,'exit_code':0}]}
                if failure=='source':(original/'source').write_bytes(b'drift')
                if failure=='cleanup':record['processes'][0]['forced_cleanup']=True
                r.endpoint_guard(record,original,before,fixture,b'fixture',stamp,save,guard,{}, {})
                self.assertFalse(record['collection_passed'])
                self.assertEqual(record['status'],'inconclusive' if failure=='none' else 'failed')
                self.assertEqual(record['error'],'legal miss: no positive action evidence')


class CaptureContractTest(unittest.TestCase):
    def test_unique_nested_owned_job_receipt_and_no_forced_success(self):
        good='fd2 job: root=4 worker_exit=0 timed_out=0 residual=0 forced=0 cleaned=1 result=0'
        self.assertEqual(r.parse_job(good)['root'],4)
        for bad in ('',good+'\n'+good,good.replace('forced=0','forced=1'),good.replace('residual=0','residual=1'),good.replace('cleaned=1','cleaned=0')):
            with self.assertRaises(AssertionError):r.parse_job(bad)
    def test_ready_failure_never_sends_action_confirmation_and_preserves_first_error(self):
        with tempfile.TemporaryDirectory() as temp:
            capture=SimpleNamespace(artifacts=Path(temp),process=SimpleNamespace(pid=42,poll=lambda:None),key=Mock())
            c=r.AnimationCollectors(capture,SimpleNamespace(driver=Path('driver')),{'kind':'original'})
            with patch.object(c,'start',side_effect=RuntimeError('first-ready')),patch.object(c,'stop',side_effect=RuntimeError('late-stop')):
                with self.assertRaisesRegex(RuntimeError,'first-ready'):
                    with c.active():c.confirm()
            capture.key.assert_not_called();self.assertEqual(c.record['collector_cleanup_error'],'late-stop')
    def test_audio_ready_identity_precedes_unique_confirm_same_hwnd(self):
        with tempfile.TemporaryDirectory() as temp:
            events=[];record={'kind':'sdl'}
            capture=SimpleNamespace(artifacts=Path(temp),process=SimpleNamespace(pid=42,poll=lambda:None),trace=[])
            def key(name):events.append(name);capture.trace.append({'delivery':{'hwnd':'0x11'}})
            capture.key=key
            c=r.AnimationCollectors(capture,SimpleNamespace(driver=Path('driver'),audio_recorder=Path('audio')),record)
            ready=[{'ready':True,'pid':42,'hwnd':'0x11'},{'ready':True,'target_pid':42,'sample_rate':44100,'channels':2,'bits':16}]
            with patch.object(r.OwnedHelper,'start',return_value=Mock()),patch.object(r,'wait_ready',side_effect=lambda *a:events.append('ready') or ready.pop(0)):
                c.start();c.confirm()
            self.assertEqual(events,['ready','ready','ENTER']);self.assertEqual(record['animation_action']['confirmations'],1)
            c.ready['hwnd']='0x22'
            with self.assertRaises(AssertionError):c.confirm()
    def test_helper_launch_failure_closes_only_owned_streams(self):
        with tempfile.TemporaryDirectory() as temp:
            h=r.OwnedHelper(Path('driver'),['helper'],Path(temp),'x',{})
            with patch.object(r.subprocess,'Popen',side_effect=OSError('launch')):
                with self.assertRaises(OSError):h.start()
            self.assertTrue(all(s.closed for s in h.streams))
    def series(self,folder):
        rows=[]
        for i in range(2):
            path=folder/f'frame{i:05d}.png';Image.new('RGB',(640,400),(i,2,3)).save(path)
            rows.append({'index':i,'file':path.name,'pid':42,'hwnd':'0x11','wgc_timestamp_100ns':100+i*10000,'client_width':640,'client_height':400})
        (folder/'frames.jsonl').write_text(''.join(json.dumps(row)+'\n' for row in rows),encoding='utf-8')
        summary={'ok':True,'backend':'WGC-HWND-session','pid':42,'hwnd':'0x11','ready_published':True,'session_started':True,'stop_reason':'stop','hresult':0,'color_filter':False,'frames':2,'first_wgc_timestamp_100ns':100,'last_wgc_timestamp_100ns':10100}
        ready={'first_index':0,'wgc_timestamp_100ns':100,'hwnd':'0x11'}
        return rows,summary,ready
    def test_series_preserves_raw_pixels_dimensions_sha_and_monotonic_times(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);_,summary,ready=self.series(folder)
            result=r.series_index(folder,summary,ready,42)
            self.assertEqual([x['timestamp_ms'] for x in result],[0,1]);self.assertEqual(result[0]['width'],640)
            self.assertEqual(result[0]['sha256'],r.sha(folder/'frame00000.png'))
    def test_series_rejects_truncation_owner_duplicate_timestamp_and_path_escape(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp)
            for change in ('stop','owner','time','path'):
                rows,summary,ready=self.series(folder)
                if change=='stop':summary['stop_reason']='frame_limit'
                if change=='owner':rows[1]['pid']=99
                if change=='time':rows[1]['wgc_timestamp_100ns']=100
                if change=='path':rows[1]['file']='../other.png'
                (folder/'frames.jsonl').write_text(''.join(json.dumps(row)+'\n' for row in rows),encoding='utf-8')
                with self.subTest(change=change),self.assertRaises(AssertionError):r.series_index(folder,summary,ready,42)
    def test_wav_requires_real_closed_nonzero_pcm_and_does_not_claim_sfx(self):
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'test.wav'
            for nonzero in (False,True):
                with wave.open(str(path),'wb') as w:
                    w.setparams((2,2,44100,0,'NONE','none'));w.writeframes(struct.pack('<hhhh',0,int(nonzero),0,0))
                if nonzero:self.assertFalse(r.wav_info(path)['SFX_identity_validated'])
                else:
                    with self.assertRaises(AssertionError):r.wav_info(path)
    def test_source_late_guard_sticky_rejects_changed_mtime_and_blocks_acceptance(self):
        with tempfile.TemporaryDirectory() as temp:
            original=Path(temp)/'original';original.mkdir();(original/'a').write_bytes(b'x')
            fixture=Path(temp)/'fixture';fixture.write_bytes(b'y');save=Path(temp)/'save';save.write_bytes(b'z')
            record={'collection_passed':True,'collection_completed':True,'business_sequence_completed':True,'processes':[{'cleaned':True,'forced_cleanup':False,'exit_code':0}]}
            before=r.fingerprint(original);guard=[b'z',save.stat().st_mtime_ns]
            r.endpoint_guard(record,original,before,fixture,b'y',fixture.stat().st_mtime_ns,save,guard,{}, {})
            self.assertTrue(record['collection_passed'])
            (original/'a').write_bytes(b'X')
            r.endpoint_guard(record,original,before,fixture,b'y',fixture.stat().st_mtime_ns,save,guard,{}, {})
            self.assertFalse(record['collection_passed'])
            (original/'a').write_bytes(b'x')
            r.endpoint_guard(record,original,before,fixture,b'y',fixture.stat().st_mtime_ns,save,guard,{}, {})
            self.assertFalse(record['collection_passed'])
    def test_preflight_bad_paths_unsupported_and_missing_audio_never_mkdir_or_spawn(self):
        with tempfile.TemporaryDirectory() as temp:
            file=Path(temp)/'fake';file.write_bytes(b'not executed')
            cases=[(ORIGINAL/'new','physical-full-hit',None),
                   (Path(temp)/'out','ai-full-09-multi',None),
                   (Path(temp)/'out','physical-full-hit',file)]
            for output,scenario,sdl in cases:
                args=SimpleNamespace(root=ROOT,artifacts=output,scenario=scenario,driver=file,dosbox=file,sdl=sdl,audio_recorder=None)
                with self.subTest(scenario=scenario,sdl=sdl),patch.object(r.Path,'mkdir') as mkdir,patch.object(r.subprocess,'Popen') as popen:
                    with self.assertRaises(ValueError):r.run(args)
                    mkdir.assert_not_called();popen.assert_not_called()


    def test_body_failure_normal_cleanup_blocks_sdl_and_keeps_original_first_error(self):
        prepared=o.prepare(ORIGINAL,'physical-full-hit')
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);binary=folder/'not-executed.exe';binary.write_bytes(b'not executable')
            args=SimpleNamespace(root=ROOT,artifacts=folder/'run',scenario='physical-full-hit',driver=binary,dosbox=binary,sdl=binary,audio_recorder=binary)
            started=[]
            @contextmanager
            def running(command,cwd,env,log,driver,processes):
                started.append(command)
                processes.append({'pid':42,'cleaned':True,'forced_cleanup':False,'exit_code':0})
                yield SimpleNamespace(pid=42)
            with patch.object(r,'owned_job_required'),patch.object(r,'prepare_runtime',return_value=prepared), \
                patch.object(r,'running',running),patch.object(r,'exercise',side_effect=TimeoutError('first-native-resource')), \
                patch.object(r.subprocess,'Popen') as popen:
                result=r.run(args)
            self.assertEqual(len(started),1);popen.assert_not_called()
            self.assertEqual(result['error'],'first-native-resource')
            self.assertEqual(result['cases'][0]['error'],'first-native-resource')
            self.assertFalse(result['ok']);self.assertFalse(result['collection_passed'])
            self.assertEqual([case['kind'] for case in result['cases']],['original'])
            self.assertTrue(result['cases'][0]['guards']['owned_game_cleanup'])

    def test_working_copy_resource_drift_rejected_cache_only_change_reported(self):
        with tempfile.TemporaryDirectory() as temp:
            original=Path(temp)/'original';original.mkdir();(original/'res').write_bytes(b'original')
            game=Path(temp)/'game';game.mkdir();(game/'res').write_bytes(b'copied')
            save=game/'FD2.SAV';save.write_bytes(b'save');fixture=Path(temp)/'controlled';fixture.write_bytes(b'fixture')
            before=r.fingerprint(original);guard=[b'save',save.stat().st_mtime_ns]
            def record():return {'collection_passed':True,'collection_completed':True,'business_sequence_completed':True,
                'processes':[{'cleaned':True,'forced_cleanup':False,'exit_code':0}],'isolated_copy_initial':r.fingerprint(game)}
            first=record();(game/'FD2.TMP').write_bytes(b'working cache')
            r.endpoint_guard(first,original,before,fixture,b'fixture',fixture.stat().st_mtime_ns,save,guard,{}, {})
            self.assertTrue(first['collection_passed'])
            second=record();(game/'res').write_bytes(b'drift')
            r.endpoint_guard(second,original,before,fixture,b'fixture',fixture.stat().st_mtime_ns,save,guard,{}, {})
            self.assertFalse(second['collection_passed']);self.assertFalse(second['guards']['isolated_static_resources_unchanged'])


class ActionProtocolTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.prepared={name:r.prepare_runtime(ORIGINAL,name) for name in o.SCENARIOS[:3]}
    def exercise(self,scenario,fail=None):
        prepared=self.prepared[scenario];events=[]
        with tempfile.TemporaryDirectory() as temp:
            capture=SimpleNamespace(artifacts=Path(temp),key=lambda name:events.append(('key',name)),wait=Mock(),process=SimpleNamespace(pid=42))
            record={'kind':'original','checkpoints':[]}
            def wait(capture,name,predicate,records,**options):
                events.append(('gate',name))
                if name==fail:raise TimeoutError(name)
                return None,{'model':True}
            @contextmanager
            def active(instance):
                events.append(('collector','ready'))
                try:yield instance
                finally:events.append(('collector','stopped'))
            def confirm(instance):events.append(('action','unique-confirm'))
            with patch.object(r,'wait_state',side_effect=wait),patch.object(r,'wait_title'),patch.object(r,'require_source_unchanged'), \
                patch.object(r.AnimationCollectors,'active',active),patch.object(r.AnimationCollectors,'confirm',confirm), \
                patch.object(r.AnimationCollectors,'wait_heal_idle0',side_effect=lambda *a:events.append(('collector','heal-idle-edge'))), \
                patch.object(r,'measure',side_effect=lambda *a:events.append(('save','measured'))):
                try:r.exercise(capture,prepared,Path(temp)/'save',[b'',0],record,SimpleNamespace())
                except TimeoutError:
                    if fail is None:raise
        return events
    def test_three_core_unique_action_collector_stop_before_navigation_save(self):
        for name in o.SCENARIOS[:3]:
            with self.subTest(name=name):
                events=self.exercise(name)
                self.assertEqual(events.count(('action','unique-confirm')),1)
                self.assertLess(events.index(('gate','b6-target')),events.index(('collector','ready')))
                self.assertLess(events.index(('collector','ready')),events.index(('action','unique-confirm')))
                if name=='player-heal13':
                    self.assertLess(events.index(('collector','ready')),events.index(('collector','heal-idle-edge')))
                    self.assertLess(events.index(('collector','heal-idle-edge')),events.index(('action','unique-confirm')))
                else:self.assertNotIn(('collector','heal-idle-edge'),events)
                self.assertLess(events.index(('collector','stopped')),events.index(('gate','b6-end-parent')))
                self.assertLess(events.index(('gate','b6-turn2-ready')),events.index(('save','measured')))
    def test_missing_target_or_initial_yes_stops_without_blind_extra_key(self):
        for gate in ('b6-target','b6-end-first-complete-buttons'):
            events=self.exercise('physical-full-hit',gate)
            self.assertEqual(events[-1],('gate',gate))
            if gate=='b6-target':self.assertNotIn(('action','unique-confirm'),events)

class VerifyCoreTest(unittest.TestCase):
    def completed(self,folder,scenario='physical-full-hit'):
        cases=[]
        for kind in ('original','sdl'):
            directory=folder/kind;directory.mkdir()
            path=directory/'closed.wav'
            with wave.open(str(path),'wb') as stream:
                stream.setparams((2,2,44100,0,'NONE','none'));stream.writeframes(struct.pack('<hhhh',0,1,0,0))
            cases.append({'kind':kind,'collection_passed':True,'business_sequence_completed':True,'collection_completed':True,
                'guards':{'late_original':True,'late_source':True,'cleanup':True},
                'processes':[{'cleaned':True,'forced_cleanup':False,'exit_code':0}],
                'audio_capture':{'wav':r.wav_info(path)}})
        return {'scenario':scenario,'collection_passed':True,'final_guards':{'source':True,'fixture':True},
                'independent_frame_pairing':{'ok':True},'cases':cases}
    def analyze(self,fdother,declaration,path):
        return {'minimum_core_sfx_supported':True,'wav':{'path':str(path),'sha256':r.sha(path)}}
    def test_each_core_requires_complete_collection_frames_and_both_sounds(self):
        for scenario in o.SCENARIOS[:3]:
            with self.subTest(scenario=scenario),tempfile.TemporaryDirectory() as temp:
                folder=Path(temp);report=self.completed(folder,scenario)
                prepared={'original':ORIGINAL,'core_sfx_declaration':r.sfx_declaration.declare(ORIGINAL,scenario)}
                with patch.object(r.core_sfx,'analyze_core_sfx',side_effect=self.analyze) as analyze:
                    result=r.verify_core(report,prepared,folder)
                self.assertTrue(result['ok']);self.assertEqual(analyze.call_count,2)
                self.assertTrue(result['collection_guards_cleanup_passed']);self.assertTrue(result['frame_passed'])
                self.assertEqual(result['sfx_passed'],{'original':True,'sdl':True})
                self.assertFalse(result['seven_FX0_instances_validated']);self.assertFalse(result['A7_fidelity_validated'])
    def test_missing_frames_or_either_wrong_sound_cannot_be_a_pass(self):
        for which in ('frames','original','sdl'):
            with self.subTest(which=which),tempfile.TemporaryDirectory() as temp:
                folder=Path(temp);report=self.completed(folder)
                if which=='frames':report['independent_frame_pairing']={'ok':False}
                def analyze(fdother,declaration,path):
                    result=self.analyze(fdother,declaration,path)
                    if Path(path).parent.name==which:result['minimum_core_sfx_supported']=False
                    return result
                with patch.object(r.core_sfx,'analyze_core_sfx',side_effect=analyze):
                    result=r.verify_core(report,{'original':ORIGINAL,'core_sfx_declaration':{}},folder)
                self.assertFalse(result['ok'])
    def test_single_endpoint_late_guard_or_unclean_process_blocks_analysis(self):
        for which in ('single','late','process','business'):
            with self.subTest(which=which),tempfile.TemporaryDirectory() as temp:
                folder=Path(temp);report=self.completed(folder)
                if which=='single':report['cases'].pop()
                if which=='late':report['final_guards']['source']=False
                if which=='process':report['cases'][1]['processes'][0]['forced_cleanup']=True
                if which=='business':report['cases'][0]['business_sequence_completed']=False
                with patch.object(r.core_sfx,'analyze_core_sfx') as analyze:
                    result=r.verify_core(report,{},folder)
                self.assertFalse(result['ok']);self.assertFalse(result['collection_guards_cleanup_passed']);analyze.assert_not_called()
    def test_changed_wav_wrong_endpoint_or_audio_sha_is_rejected(self):
        for which in ('changed','wrong-endpoint','oracle-sha'):
            with self.subTest(which=which),tempfile.TemporaryDirectory() as temp:
                folder=Path(temp);report=self.completed(folder)
                if which=='changed':Path(report['cases'][0]['audio_capture']['wav']['path']).write_bytes(b'changed')
                if which=='wrong-endpoint':report['cases'][1]['audio_capture']=report['cases'][0]['audio_capture']
                def analyze(fdother,declaration,path):
                    result=self.analyze(fdother,declaration,path)
                    if which=='oracle-sha':result['wav']['sha256']='0'*64
                    return result
                with patch.object(r.core_sfx,'analyze_core_sfx',side_effect=analyze):
                    result=r.verify_core(report,{'original':ORIGINAL,'core_sfx_declaration':{}},folder)
                self.assertFalse(result['ok']);self.assertIn('errors',result)
    def test_analysis_exception_and_late_wav_rewrite_keep_gate_false(self):
        for which in ('exception','late-rewrite'):
            with self.subTest(which=which),tempfile.TemporaryDirectory() as temp:
                folder=Path(temp);report=self.completed(folder)
                def analyze(fdother,declaration,path):
                    if which=='exception':raise RuntimeError('analysis failure')
                    if Path(path).parent.name=='sdl':Path(report['cases'][0]['audio_capture']['wav']['path']).write_bytes(b'changed after first analysis')
                    return self.analyze(fdother,declaration,path)
                with patch.object(r.core_sfx,'analyze_core_sfx',side_effect=analyze):
                    result=r.verify_core(report,{'original':ORIGINAL,'core_sfx_declaration':{}},folder)
                self.assertFalse(result['ok']);self.assertIn('errors',result)
                if which=='late-rewrite':self.assertFalse(result['wav_sources_unchanged'])

    def test_flag_without_sdl_or_with_ai_rejected_before_any_output_or_process(self):
        for scenario,sdl in (('physical-full-hit',None),('ai-full-00',Path('fake.exe'))):
            args=SimpleNamespace(root=ROOT,artifacts=ROOT/'not-created-core',scenario=scenario,sdl=sdl,verify_core=True)
            with self.subTest(scenario=scenario),patch.object(r.Path,'mkdir') as mkdir,patch.object(r.subprocess,'Popen') as popen,patch.object(r,'owned_job_required') as job:
                with self.assertRaises(ValueError):r.run(args)
                mkdir.assert_not_called();popen.assert_not_called();job.assert_not_called()
    def test_core_modules_are_in_runtime_tool_manifest(self):
        paths=r.dependencies()
        for name in ('fd2_core_sfx_gate.py','fd2_battle_animation_sfx.py','fd2_sfx_oracle.py'):
            self.assertIn(str((ROOT/'tools'/name).resolve()),paths)
    def test_cli_default_collection_and_strict_failure_have_different_exit_contracts(self):
        import io
        common=['runner','--root','root','--driver','driver','--dosbox','dosbox','--artifacts','new','--scenario','physical-full-hit']
        for flag,ok,expected in ((False,False,0),(True,False,1),(True,True,0)):
            report={'status':'passed' if ok else 'collected','collection_passed':True,'ok':ok,'collected_only':not flag}
            with self.subTest(flag=flag,ok=ok),patch.object(sys,'argv',common+(['--verify-core'] if flag else [])), \
                patch.object(r,'run',return_value=report),patch.object(sys,'stdout',io.StringIO()):
                self.assertEqual(r.main(),expected)
    def test_strict_analysis_happens_only_after_both_game_contexts_close(self):
        prepared=o.prepare(ORIGINAL,'physical-full-hit');prepared['frame_resources']={};events=[]
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);binary=folder/'fake.exe';binary.write_bytes(b'not executed')
            args=SimpleNamespace(root=ROOT,artifacts=folder/'run',scenario='physical-full-hit',driver=binary,dosbox=binary,sdl=binary,audio_recorder=binary,verify_core=True)
            @contextmanager
            def running(command,cwd,env,log,driver,processes):
                row={'cleaned':False,'forced_cleanup':False,'exit_code':None};processes.append(row)
                role='original' if '-conf' in command else 'sdl';events.append('open-'+role)
                try:yield SimpleNamespace(pid=42)
                finally:row.update(cleaned=True,exit_code=0);events.append('close-'+role)
            def exercise(capture,p,save,guard,record,args):
                path=capture.artifacts/'closed.wav'
                with wave.open(str(path),'wb') as stream:
                    stream.setparams((2,2,44100,0,'NONE','none'));stream.writeframes(struct.pack('<hhhh',0,1,0,0))
                record.update(business_sequence_completed=True,collection_completed=True,
                    animation_capture={'frames':[]},audio_capture={'wav':r.wav_info(path)})
            def analyze(fdother,declaration,path):
                self.assertEqual(events[:4],['open-original','close-original','open-sdl','close-sdl'])
                events.append('analyze-'+Path(path).parent.name)
                return self.analyze(fdother,declaration,path)
            with patch.object(r,'owned_job_required'),patch.object(r,'prepare_runtime',return_value=prepared), \
                patch.object(r,'running',running),patch.object(r,'exercise',side_effect=exercise), \
                patch.object(r.frame_oracle,'compare_sequences',return_value={'ok':True}), \
                patch.object(r.core_sfx,'analyze_core_sfx',side_effect=analyze),patch.object(r.subprocess,'Popen') as popen:
                result=r.run(args)
            popen.assert_not_called();self.assertTrue(result['ok'],json.dumps(result,ensure_ascii=False));self.assertFalse(result['collected_only'])
            self.assertEqual(events[-2:],['analyze-original','analyze-sdl'])


class HealIdleSeriesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.anchors=r.prepare_runtime(ORIGINAL,'player-heal13')['heal_idle_anchors']
    def write(self,folder,index,phase,*,stamp=None,pid=42,hwnd='0x11',trailing=False):
        frame=Image.new('RGB',(320,200))
        if phase is not None:
            anchor=self.anchors[phase];frame.paste(anchor['image'],anchor['at'])
        path=folder/f'frame{index:05d}.png';frame.resize((640,400),Image.Resampling.NEAREST).save(path)
        row={'index':index,'file':path.name,'pid':pid,'hwnd':hwnd,'wgc_timestamp_100ns':100+10000*index if stamp is None else stamp,'client_width':640,'client_height':400}
        with (folder/'frames.jsonl').open('ab') as stream:
            stream.write(json.dumps(row).encode()+ (b'' if trailing else b'\n'))
        return row
    def run_idle(self,phases,*,initial=0,target=True,timeout=4,wrong=None):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp)/'animation-series';folder.mkdir()
            self.write(folder,0,initial)
            capture=SimpleNamespace(artifacts=Path(temp),key=Mock(),frame=Mock(),process=SimpleNamespace(pid=42,poll=lambda:None))
            record={};collector=r.AnimationCollectors(capture,SimpleNamespace(),record)
            collector.started=True;collector.ready={'pid':42,'hwnd':'0x11','first_index':0,'wgc_timestamp_100ns':100}
            collector.helpers=[SimpleNamespace(process=SimpleNamespace(poll=lambda:None))]
            clock=[0];next_index=[1];pending=list(phases)
            def sleep(_):
                clock[0]+=1
                if pending:
                    phase=pending.pop(0);idx=next_index[0];next_index[0]+=1
                    options=wrong or {};self.write(folder,idx,phase,**options)
            predicate=Mock(return_value={'kind':'target-resource'} if target else None)
            error=None;result=None
            with patch.object(r.time,'monotonic',side_effect=lambda:clock[0]),patch.object(r.time,'sleep',side_effect=sleep):
                try:result=collector.wait_heal_idle0(self.anchors,predicate,timeout=timeout)
                except (TimeoutError,AssertionError) as exception:error=exception
            capture.key.assert_not_called();capture.frame.assert_not_called()
            return result,error,record,predicate
    def test_source_anchor_is_original403_pixels_and_transparency_crop_is_retained(self):
        zero=self.anchors[0]
        self.assertEqual(zero['identity'],{'resource':'FDICON identity','raw':1,'unit_id':9,'frame':0})
        self.assertEqual(zero['checked_pixels'],403);self.assertEqual(zero['at'],(125,94))
        for phase in (0,1,2):
            image=Image.new('RGB',(320,200));image.paste(self.anchors[phase]['image'],self.anchors[phase]['at'])
            self.assertEqual([p for p,a in self.anchors.items() if r.frame_oracle._anchor_match(image,a)],[phase])
    def test_only_new_nonzero_then_zero_same_series_ack_can_succeed(self):
        for nonzero in (1,2):
            result,error,record,predicate=self.run_idle([nonzero,0])
            self.assertIsNone(error);self.assertTrue(result['completed'])
            self.assertEqual(result['nonzero']['index'],1);self.assertEqual(result['zero']['index'],2)
            self.assertLess(result['nonzero']['wgc_timestamp_100ns'],result['zero']['wgc_timestamp_100ns'])
            self.assertEqual(result['zero']['checked_pixels'],403);predicate.assert_called_once()
            self.assertFalse(result['game_clock_phase_lock_or_pixel_alignment_guaranteed'])
    def test_initial_zero_and_unknown_or_bare_frames_do_not_arm_edge(self):
        for phases in ([0,0,0],[None,0,None]):
            result,error,record,predicate=self.run_idle(phases)
            self.assertIsNone(result);self.assertIsInstance(error,TimeoutError);self.assertFalse(record['heal13_idle0_ack']['completed'])
            self.assertNotIn('nonzero',record['heal13_idle0_ack']);predicate.assert_not_called()
    def test_nonzero_without_new_zero_or_missing_target_never_confirms(self):
        for phases,target in (([1,2,1],True),([1,0,0],False)):
            result,error,record,predicate=self.run_idle(phases,target=target)
            self.assertIsNone(result);self.assertIsInstance(error,TimeoutError)
            self.assertIn('nonzero',record['heal13_idle0_ack']);self.assertFalse(record['heal13_idle0_ack']['completed'])
    def test_bad_owner_or_nonmonotonic_new_wgc_timestamp_rejected(self):
        for wrong in ({'pid':99},{'hwnd':'0x22'},{'stamp':100}):
            result,error,record,predicate=self.run_idle([1,0],wrong=wrong)
            self.assertIsNone(result);self.assertIsInstance(error,AssertionError);predicate.assert_not_called()
    def test_old_finished_edge_is_baseline_and_cannot_be_replayed(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp)/'animation-series';folder.mkdir();self.write(folder,0,1);self.write(folder,1,0)
            capture=SimpleNamespace(artifacts=Path(temp),key=Mock(),process=SimpleNamespace(pid=42,poll=lambda:None))
            c=r.AnimationCollectors(capture,SimpleNamespace(),{});c.started=True;c.ready={'pid':42,'hwnd':'0x11','first_index':0,'wgc_timestamp_100ns':100}
            c.helpers=[];clock=[0]
            with patch.object(r.time,'monotonic',side_effect=lambda:clock[0]),patch.object(r.time,'sleep',side_effect=lambda _:clock.__setitem__(0,clock[0]+1)):
                with self.assertRaises(TimeoutError):c.wait_heal_idle0(self.anchors,Mock(),timeout=2)
            self.assertEqual(c.record['heal13_idle0_ack']['baseline']['index'],1)
            self.assertNotIn('nonzero',c.record['heal13_idle0_ack']);capture.key.assert_not_called()
    def test_stop_rebinds_ack_witness_sha_to_the_final_same_series(self):
        for changed in (False,True):
            with self.subTest(changed=changed),tempfile.TemporaryDirectory() as temp:
                folder=Path(temp)/'animation-series';folder.mkdir()
                rows=[self.write(folder,i,phase) for i,phase in enumerate((0,1,0))]
                capture=SimpleNamespace(artifacts=Path(temp),key=Mock(),process=SimpleNamespace(pid=42,poll=lambda:None))
                summary={'ok':True,'backend':'WGC-HWND-session','pid':42,'hwnd':'0x11','ready_published':True,'session_started':True,
                    'stop_reason':'stop','hresult':0,'color_filter':False,'frames':3,'first_wgc_timestamp_100ns':100,'last_wgc_timestamp_100ns':20100}
                ready={'pid':42,'hwnd':'0x11','first_index':0,'wgc_timestamp_100ns':100}
                ack={'completed':True,'nonzero':{**rows[1],'sha256':r.sha(folder/rows[1]['file'])},'zero':{**rows[2],'sha256':r.sha(folder/rows[2]['file'])}}
                record={'kind':'original','series_helper':{'summary':summary},'animation_capture':{},'heal13_idle0_ack':ack,
                    'native_audio':{'enabled':True,'started':True,'stopped':True}}
                (folder/'summary.json').write_text(json.dumps(summary),encoding='utf-8')
                audio=Path(temp)/'captures';audio.mkdir()
                with wave.open(str(audio/'closed.wav'),'wb') as stream:
                    stream.setparams((2,2,44100,0,'NONE','none'));stream.writeframes(struct.pack('<hhhh',0,1,0,0))
                c=r.AnimationCollectors(capture,SimpleNamespace(),record);c.ready=ready
                helper=SimpleNamespace(finish=Mock());c.helpers=[helper]
                if changed:Image.new('RGB',(640,400),(1,1,1)).save(folder/rows[2]['file'])
                if changed:
                    with self.assertRaises(AssertionError):c.stop()
                    self.assertNotIn('collection_completed',record)
                else:c.stop();self.assertTrue(record['collection_completed'])
                helper.finish.assert_called_once();capture.key.assert_not_called();self.assertTrue((folder/'stop').is_file())

    def test_partial_trailing_json_is_not_a_published_frame_and_timeout_is_bounded(self):
        with tempfile.TemporaryDirectory() as temp:
            folder=Path(temp);first=self.write(folder,0,0);self.write(folder,1,1,trailing=True)
            ready={'hwnd':'0x11','first_index':0,'wgc_timestamp_100ns':100}
            row,_=r.latest_series_frame(folder,ready,42)
            self.assertEqual(row['index'],0);self.assertIsNone(r.latest_series_frame(folder,ready,42,row))
            capture=SimpleNamespace(artifacts=folder,process=SimpleNamespace(pid=42))
            c=r.AnimationCollectors(capture,SimpleNamespace(),{})
            with self.assertRaises(ValueError):c.wait_heal_idle0(self.anchors,Mock(),timeout=16)


if __name__=='__main__':unittest.main()
