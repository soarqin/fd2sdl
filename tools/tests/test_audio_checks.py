"""A7永久入口的纯合同；不启动游戏、窗口、设备或WinDLL。"""
from __future__ import annotations
import contextlib
import copy
import io
import json
from pathlib import Path
import subprocess
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

from PIL import Image

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import fd2_audio_checks as checks
import fd2_app_mix_capture as app_mix

class LeaveAudioTest(unittest.TestCase):
    def test_real_messages_require_all_seven_glyphs_at_formal_origins(self):
        import run_original_comparison as common
        import run_original_battle_menu as menu
        import fd2_reward_oracle as reward
        import fd2_magic_list_oracle as magic
        root=Path(__file__).resolve().parents[2]
        assets=menu.resource_templates(root/'original_game/FDOTHER.DAT',root/'original_game/FDTXT.DAT')
        tools={'run_original_comparison':common,'run_original_battle_menu':menu,
            'fd2_reward_oracle':reward,'fd2_magic_list_oracle':magic}
        expected=checks.prepare_leave_messages(root/'original_game',assets,tools)
        prepared=dict(leave_messages=expected,tools=tools)
        for fragment,y in ((415,127),(416,146)):
            with self.subTest(fragment=fragment):
                item=expected[fragment]
                self.assertEqual(item['position'],(98,y));self.assertEqual(item['glyphs'],7)
                frame=Image.new('P',(320,200),0);frame.putpalette(assets['palette'])
                frame.paste(item['image'],item['position']);frame=frame.convert('RGB')
                hit=checks.leave_message_match(frame,prepared,fragment)
                self.assertEqual(hit['text_origin'],[99,y]);self.assertTrue(hit['text_match']['ok'])
                frame.paste(tuple(assets['palette'][:3]),(99+6*16,y,99+7*16,y+17))
                self.assertIsNone(checks.leave_message_match(frame,prepared,fragment))
        tools['fd2_reward_oracle']=SimpleNamespace(common_tokens=lambda *args:[1]*6+[-3,-1])
        with self.assertRaises(ValueError):checks.prepare_leave_messages(root/'original_game',assets,tools)

    def test_leave_is_explicit_registered_scope_not_a_return_to_title(self):
        import run_audio_checks as cli
        args=cli.parser().parse_args(['--game','game','--driver','driver','--dosbox','dosbox',
            '--bootstrap','bootstrap','--artifacts','new','--scenarios','leave'])
        self.assertEqual(args.scenarios,['leave']);self.assertFalse(args.execute)
        declaration=checks.declaration(args.scenarios)
        self.assertIn('normalexit0',declaration['episode_scope']['leave'])
        self.assertIn('not full200ms/4s',declaration['leave_audio_scope'])

    def test_leave_requires_one_final_stop_after_valid_field_music(self):
        def row(track,reason,current=None):
            return 'music request: stage=0 track=%d reason=%s available=1 result=0 current=%d loop=0'%(track,reason,track if current is None else current)
        good='\n'.join([row(18,'title'),row(-1,'continue-stop'),row(19,'continue'),row(-1,'leave')])
        self.assertEqual(checks.music_requests(good,'leave')['required_ordered_subsequence'],[19,-1])
        bad=[good.rsplit('\n',1)[0],good+'\n'+row(-1,'leave'),good+'\n'+row(18,'title'),
            good.replace('reason=leave','reason=shutdown'),good.replace('result=0 current=-1 loop=0','result=-1 current=-1 loop=0'),
            good.replace('reason=leave available=1 result=0 current=-1','reason=leave available=1 result=0 current=19')]
        for log in bad:
            with self.subTest(log=log),self.assertRaises(AssertionError):checks.music_requests(log,'leave')

    def test_normal_leave_qualification_needs_pixels_exit_save_and_transport(self):
        process=dict(exit_code=0,cleaned=True,forced_cleanup=False)
        record=dict(kind='sdl',business_sequence_completed=True,audio_collection_passed=True,
            leave_416_observed=True,leave_capture_stop_requested_after_full416=True,
            normal_leave_exit=dict(exit_code=0,no_game_key_after_yes=True,source_save_unchanged=True,returned_to_title=False),
            transport_observation={'ok':True})
        self.assertTrue(checks.qualify_case(record,process,'leave')['ok'])
        for field in ['leave_416_observed','leave_capture_stop_requested_after_full416']:
            bad=copy.deepcopy(record);bad[field]=False
            with self.subTest(field=field):self.assertFalse(checks.qualify_case(bad,process,'leave')['ok'])
        for field,value in [('exit_code',1),('no_game_key_after_yes',False),('source_save_unchanged',False),('returned_to_title',True)]:
            bad=copy.deepcopy(record);bad['normal_leave_exit'][field]=value
            with self.subTest(field=field):self.assertFalse(checks.qualify_case(bad,process,'leave')['ok'])
        record['transport_observation']['ok']=False
        self.assertFalse(checks.qualify_case(record,process,'leave')['ok'])
        record['kind']='original'
        self.assertTrue(checks.qualify_case(record,process,'leave')['ok'])

    def leave_flow(self,kind,*,miss=False,exit_error=False,stop_error=False):
        events=[];record=dict(inputs=[],events=[],checkpoints=[],resource_series=[])
        with tempfile.TemporaryDirectory() as directory:
            save=Path(directory)/'FD2.SAV';save.write_bytes(b'complete-isolated-source')
            process=mock.Mock(pid=42);process.poll.return_value=None
            def wait(timeout):
                events.append('exit-wait');self.assertEqual(timeout,5)
                if exit_error:raise TimeoutError('normal exit not observed')
                process.poll.return_value=0;return 0
            process.wait.side_effect=wait
            def key(name,**kwargs):
                events.append(('key',name,kwargs));record['inputs'].append({'key':name})
            capture=SimpleNamespace(key=key,wait=mock.Mock(),process=process)
            audio=mock.Mock();audio.assert_running=mock.Mock()
            def stop():
                events.append('capture-stop')
                if stop_error:raise OSError('stop publish failed')
            audio.request_stop.side_effect=stop
            audio.finish.side_effect=lambda p:events.append('capture-finalized') or {'closed':True}
            menu=SimpleNamespace(wait_title=mock.Mock())
            oracle=SimpleNamespace(menu=mock.Mock(),secondary=mock.Mock(),closed_browse=mock.Mock())
            tools={'run_original_battle_menu':menu,
                'run_original_end_turn':SimpleNamespace(RoundOracle=lambda *args:oracle),
                'run_original_load':SimpleNamespace(require_source_unchanged=mock.Mock()),
                'run_original_battle_animation':SimpleNamespace(qpc_stamp=lambda:dict(qpc=100,qpc_frequency=1000))}
            prepared=dict(tools=tools,assets={},titles={3:['title','unused','continue']})
            def confirmation(c,p,r,o,delivery,label,**kwargs):
                self.assertEqual(label,'leave-default-YES');self.assertIn('message',kwargs)
                events.append('prompt415-and-first-YES');delivery('ENTER')
            def accepted(c,p,r,name,predicate,*,open_key,on_match):
                events.append('WGC-ready');open_key()
                if miss:raise TimeoutError('416 not observed')
                events.append('full416');on_match();events.append('WGC-close')
            error=None
            with (mock.patch.object(checks,'checkpoint'),mock.patch.object(checks,'observe_audio'),
                    mock.patch.object(checks,'wait_confirmation',side_effect=confirmation),
                    mock.patch.object(checks,'wait_resource_series',side_effect=accepted)):
                try:checks.exercise_leave(capture,kind,prepared,save,record,audio)
                except (TimeoutError,OSError) as caught:error=caught
            return record,events,audio,error

    def test_real_input_shape_and_stop_before_WGC_close_then_normal_exit(self):
        record,events,audio,error=self.leave_flow('sdl')
        self.assertIsNone(error);self.assertTrue(record['business_sequence_completed'])
        keys=[event for event in events if isinstance(event,tuple)]
        self.assertEqual([event[1] for event in keys],['UP','ENTER','ENTER','UP','ENTER','DOWN','ENTER','ENTER'])
        self.assertEqual(keys[-1][2],{'fast':True})
        self.assertLess(events.index('WGC-ready'),events.index(keys[-1]))
        self.assertLess(events.index('full416'),events.index('capture-stop'))
        self.assertLess(events.index('capture-stop'),events.index('WGC-close'))
        self.assertLess(events.index('capture-finalized'),events.index('exit-wait'))
        self.assertEqual(audio.request_stop.call_count,1);self.assertEqual(audio.finish.call_count,1)
        self.assertFalse(record['normal_leave_exit']['returned_to_title'])

    def test_original_does_not_toggle_HOST_W_at_416_or_force_capture_stop(self):
        record,events,audio,error=self.leave_flow('original')
        self.assertIsNone(error);self.assertTrue(record['audio_finalize_on_normal_close'])
        self.assertTrue(record['business_sequence_completed']);audio.request_stop.assert_not_called()
        audio.finish.assert_not_called();self.assertNotIn('HOST_W',[e[1] for e in events if isinstance(e,tuple)])

    def test_missed_416_stop_error_and_exit_timeout_preserve_failure_without_more_keys(self):
        for kwargs in [dict(miss=True),dict(stop_error=True),dict(exit_error=True)]:
            with self.subTest(kwargs=kwargs):
                record,events,audio,error=self.leave_flow('sdl',**kwargs)
                self.assertIsNotNone(error);self.assertNotIn('business_sequence_completed',record)
                self.assertEqual(len([e for e in events if isinstance(e,tuple)]),8)
                if kwargs.get('miss'):
                    audio.request_stop.assert_not_called();audio.finish.assert_not_called()
                    self.assertNotIn('leave_416_observed',record)
                if kwargs.get('stop_error'):audio.finish.assert_not_called()

    def test_actual_ordered_WGC_seam_stops_capture_before_helper_close_even_on_stop_error(self):
        import run_original_battle_animation as battle
        for fail_stop in (False,True):
            with self.subTest(fail_stop=fail_stop),tempfile.TemporaryDirectory() as directory:
                folder=Path(directory);events=[];ready=dict(ready=True,pid=42,hwnd='0x123',
                    first_index=0,wgc_timestamp_100ns=100)
                summary=dict(ok=True,backend='WGC-HWND-session',pid=42,hwnd='0x123',
                    ready_published=True,session_started=True,stop_reason='stop',hresult=0,
                    color_filter=False,frames=2,first_wgc_timestamp_100ns=100,last_wgc_timestamp_100ns=101)
                record=dict(inputs=[dict(key='ENTER',delivery=dict(pid=42,hwnd='0x123',ok=True,
                    error=0,noactivate_verified=True,foreground_guard_ok=True))],checkpoints=[],resource_series=[])
                capture=SimpleNamespace(driver=Path('driver'),artifacts=folder,
                    process=SimpleNamespace(pid=42,poll=lambda:None))
                class Helper:
                    def __init__(self,driver,args,work,label,slot):self.folder=Path(args[3])
                    def start(self):
                        self.folder.mkdir();rows=[]
                        for i in range(2):
                            Image.new('RGB',(320,200),(i,2,3)).save(self.folder/f'frame{i:05d}.png')
                            rows.append(dict(index=i,file=f'frame{i:05d}.png',pid=42,hwnd='0x123',
                                wgc_timestamp_100ns=100+i,client_width=320,client_height=200))
                        (self.folder/'frames.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in rows),encoding='utf-8')
                        (self.folder/'ready.json').write_text(json.dumps(ready),encoding='utf-8')
                        return SimpleNamespace(poll=lambda:None)
                    def finish(self):events.append('helper-finish');return summary
                tools={'run_original_battle_animation':SimpleNamespace(OwnedHelper=Helper,
                    wait_ready=lambda *args,**kwargs:ready,series_index=battle.series_index),
                    'run_original_comparison':SimpleNamespace(normalize=lambda frame:frame.convert('RGB'))}
                def stop():
                    events.append('capture-stop')
                    if fail_stop:raise OSError('exclusive stop error')
                call=lambda:checks.wait_resource_series(capture,dict(tools=tools),record,'leave416',
                    lambda frame:dict(fragment=416) if frame.getpixel((0,0))[0]==1 else None,
                    open_key=lambda:events.append('YES'),on_match=stop)
                if fail_stop:
                    with self.assertRaisesRegex(OSError,'exclusive stop error'):call()
                    self.assertFalse(record['resource_series'][0]['completed'])
                else:self.assertTrue(call()['strict_stop_owner_frame_guard_passed'])
                self.assertEqual(events,['YES','capture-stop','helper-finish'])
                self.assertTrue((folder/'leave416-series'/'stop').is_file())

class TransportStatsTest(unittest.TestCase):
    GOOD='audio transport v1: first_error=0 queue_samples=3 queue_peak_bytes=384 successful_puts=3 backlog_drop_chunks=0'
    def test_exact_owner_receipt_is_not_device_or_OS_peak(self):
        row=checks.audio_transport_observation('unrelated\n'+self.GOOD+'\n')
        self.assertTrue(row['ok']);self.assertEqual(row['successful_puts'],3)
        self.assertFalse(row['actual_OS_queue_peak_proven']);self.assertFalse(row['device_output_validated'])
        self.assertIn('sampled_',row['peak_kind'])
    def test_missing_duplicate_malformed_and_overflow_rejected(self):
        for log in ('',self.GOOD+'\n'+self.GOOD,self.GOOD.replace('v1:','v1: extra'),
                    self.GOOD.replace('queue_samples=3','queue_samples=-1'),
                    self.GOOD.replace('queue_samples=3','queue_samples=18446744073709551616'),
                    self.GOOD.replace('queue_peak_bytes=384','queue_peak_bytes=2147483648'),
                    self.GOOD.replace('queue_samples=3','queue_samples=2')):
            with self.subTest(log=log),self.assertRaises(AssertionError):checks.audio_transport_observation(log)
    def test_zero_drop_and_first_error_are_independent_required_gates(self):
        for log in (self.GOOD.replace('first_error=0','first_error=3'),
                    self.GOOD.replace('successful_puts=3 backlog_drop_chunks=0','successful_puts=2 backlog_drop_chunks=1'),
                    self.GOOD.replace('queue_samples=3','queue_samples=0').replace('queue_peak_bytes=384','queue_peak_bytes=0').replace('successful_puts=3','successful_puts=0')):
            with self.subTest(log=log):self.assertFalse(checks.audio_transport_observation(log)['ok'])
    def test_sdl_sfx_requires_stats_original_and_other_cases_do_not(self):
        process=dict(exit_code=0,cleaned=True,forced_cleanup=False)
        row=dict(kind='sdl',business_sequence_completed=True,audio_collection_passed=True,finite_sfx_verification={'ok':True})
        self.assertFalse(checks.qualify_case(row,process,'sfx-reenable')['ok'])
        row['transport_observation']=checks.audio_transport_observation(self.GOOD)
        self.assertTrue(checks.qualify_case(row,process,'sfx-reenable')['ok'])
        row['transport_observation']['ok']=False
        self.assertFalse(checks.qualify_case(row,process,'sfx-reenable')['ok'])
        self.assertTrue(checks.qualify_case(row,process,'options')['ok'])
        row['kind']='original';self.assertTrue(checks.qualify_case(row,process,'sfx-reenable')['ok'])


class CaptureClockVersionTest(unittest.TestCase):
    def test_reenable_window_uses_exact_declared_capture_origin(self):
        for schema in (app_mix.LEGACY_SCHEMA,app_mix.PREVIOUS_SCHEMA,app_mix.SCHEMA):
            ready=dict(schema=schema,qpc_frequency=10000000)
            if schema==app_mix.LEGACY_SCHEMA:
                ready.update(clock_source=app_mix.LEGACY_CLOCK,first_callback_qpc=100000)
            else:
                ready.update(clock_source=app_mix.CLOCK,transport_owner='main-thread',
                    capture_point_version=2,capture_point='after_successful_SDL_PutAudioStreamData',
                    first_submission_qpc=100000)
            if schema==app_mix.SCHEMA:
                ready.update(pcm_quantizer=app_mix.PCM_QUANTIZER,pcm_quantizer_version=1)
            calls=[]
            refs=[{} for _ in range(13)]
            refs[8]=dict(sha256=checks.VOICE_SHA,source_frames=2652)
            def analyze(resource,query,path):
                calls.append(query)
                return dict(queries=[dict(identity_status='supported_candidate')])
            oracle=SimpleNamespace(references=lambda *args:refs,analyze=analyze)
            prepared=dict(original=Path('not-read'),tools={'fd2_sfx_oracle':oracle,'fd2_app_mix_capture':app_mix})
            record=dict(kind='sdl',audio_ready=ready,audio_wave=dict(path='not-read.wav',frames=48000,sample_rate=48000),
                events=[dict(name='off-observation-before',clock=dict(qpc=150000,qpc_frequency=10000000)),
                        dict(name='off-observation-after',clock=dict(qpc=200000,qpc_frequency=10000000))])
            result=checks.verify_reenable(prepared,record)
            self.assertEqual((calls[1]['queries'][0]['start_ms'],calls[1]['queries'][0]['end_ms']),(5,10))
            self.assertEqual(result['off_observation']['capture_schema'],schema)
            self.assertFalse(result['exact_timing_verified'])
            for bad_frequency in (None,True,1):
                record['events'][0]['clock']['qpc_frequency']=bad_frequency
                with self.subTest(schema=schema,bad_frequency=bad_frequency),self.assertRaises(AssertionError):
                    checks.verify_reenable(prepared,record)
            record['events'][0]['clock']['qpc_frequency']=10000000
            record['events'][0]['clock']['qpc']=200001
            with self.assertRaises(AssertionError):checks.verify_reenable(prepared,record)
    def test_mixed_callback_and_submission_identity_cannot_supply_clock_origin(self):
        for row in [dict(schema=app_mix.SCHEMA,clock_source=app_mix.LEGACY_CLOCK,first_callback_qpc=100),
                    dict(schema=app_mix.LEGACY_SCHEMA,clock_source=app_mix.CLOCK,first_submission_qpc=100)]:
            with self.assertRaises(AssertionError):app_mix.first_qpc(row)
import run_audio_checks as cli


class Clock:
    def __init__(self):self.value=0
    def monotonic(self):return self.value
    def sleep(self,seconds):self.value+=seconds


class Reader:
    def __init__(self,frames):self.frames=frames
    def unread(self):
        rows,self.frames=self.frames,[]
        yield from rows


def buttons():
    return SimpleNamespace(buttons=lambda frame,selected:
        {'matched':frame} if frame!='absent' and (selected is None or
            frame==('YES' if selected==0 else 'NO')) else None)


class AudioChecksTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from run_original_battle_menu import MENU_OFFSETS,resource_templates,Oracle
        root=Path(__file__).resolve().parents[2]
        cls.option_assets=resource_templates(root/'original_game/FDOTHER.DAT',root/'original_game/FDTXT.DAT')
        cls.option_oracle=Oracle(cls.option_assets,(28,76));cls.option_offsets=MENU_OFFSETS

    def option_frame(self,values,selected,*,missing=None,override=None):
        canvas=Image.new('P',(320,200),0);canvas.putpalette(self.option_assets['palette'])
        commands=(18+(not values[0]),20+(not values[1]),22+values[2],24+(not values[3]))
        for index,command in enumerate(commands):
            if index==missing:continue
            resource=override[1] if override and override[0]==index else command*3+(selected==index)
            image=self.option_assets['commands'][resource]
            at=tuple(a+b for a,b in zip(self.option_oracle.focus,self.option_offsets[index]))
            canvas.paste(image,at,image.point(lambda value:255 if value else 0,mode='L'))
        return canvas.convert('RGB')

    def test_SDL_options_validate_all_four_original_positive_resource_rows(self):
        for bits in range(16):
            values=[(bits>>i)&1 for i in range(4)]
            for selected in (0,1):
                frame=self.option_frame(values,selected)
                with self.subTest(values=values,selected=selected):
                    original=checks.options_match(frame,self.option_oracle,values,selected,'original',self.option_offsets)
                    sdl=checks.options_match(frame,self.option_oracle,values,selected,'sdl',self.option_offsets)
                    self.assertEqual(sdl,original);self.assertIsNotNone(sdl)
                    self.assertEqual(len(sdl['matches']),4)
                    self.assertTrue(all(row['checked_pixels']>0 and row['ok'] for row in sdl['matches']))
                    self.assertFalse(any('disabled_transparent' in row for row in sdl['matches']))

    def test_SDL_options_lower_row_missing_disabled_or_wrong_highlight_is_rejected(self):
        values=[1,1,0,1];selected=0
        for index in (2,3):
            command=(22,24)[index-2]
            disabled=command*3+2
            self.assertFalse(any(self.option_assets['commands'][disabled].getdata()))
            for reason,frame in (
                    ('missing',self.option_frame(values,selected,missing=index)),
                    ('disabled_transparent',self.option_frame(values,selected,override=(index,disabled))),
                    ('wrong_highlight',self.option_frame(values,selected,override=(index,command*3+1)))):
                for kind in ('sdl','original'):
                    with self.subTest(index=index,reason=reason,kind=kind):
                        self.assertIsNone(checks.options_match(frame,self.option_oracle,values,selected,kind,self.option_offsets))

    def test_SDL_options_wrong_value_or_selected_row_is_rejected(self):
        values=[1,1,0,1];frame=self.option_frame(values,0)
        for kind in ('sdl','original'):
            for index in range(4):
                wrong=list(values);wrong[index]^=1
                with self.subTest(kind=kind,index=index):
                    self.assertIsNone(checks.options_match(frame,self.option_oracle,wrong,0,kind,self.option_offsets))
            self.assertIsNone(checks.options_match(frame,self.option_oracle,values,1,kind,self.option_offsets))
        for kind,selected in (('unknown',0),('sdl',2)):
            with self.assertRaises(ValueError):checks.options_match(frame,self.option_oracle,values,selected,kind,self.option_offsets)

    def fixture(self):
        plain=bytearray((i*37+11)&255 for i in range(0x59cb))
        plain[checks.META:checks.META+18]=checks.FIXTURE_META
        plain[-4:]=sum(plain[:-4]).to_bytes(4,'little')
        mask=bytes((i*19+5)&255 for i in range(len(plain)))
        return bytes(a^b for a,b in zip(plain,mask)),bytes(plain),mask

    def test_only_registered_unique_scenarios(self):
        self.assertEqual(checks.validate_scenarios(checks.SCENARIOS),checks.SCENARIOS)
        for values in ([],['options','options'],['old-private-scene']):
            with self.subTest(values=values),self.assertRaises(ValueError):checks.validate_scenarios(values)

    def test_flags_keep_full_unknown_container_and_cipher(self):
        encoded,plain,mask=self.fixture()
        output,controlled,changes=checks.derive_flags(encoded,plain,(0,0))
        self.assertEqual(bytes(a^b for a,b in zip(output,mask)),controlled)
        allowed={checks.META+16,checks.META+17,*range(len(plain)-4,len(plain))}
        self.assertTrue(changes)
        self.assertTrue(all(x['offset'] in allowed for x in changes))
        self.assertEqual(bytes(v for i,v in enumerate(controlled) if i not in allowed),
            bytes(v for i,v in enumerate(plain) if i not in allowed))
        self.assertEqual(sum(controlled[:-4]),int.from_bytes(controlled[-4:],'little'))

    def test_flags_reject_bad_source_or_unproven_values(self):
        encoded,plain,_=self.fixture()
        for broken in (plain[:-1],plain[:-1]+bytes((plain[-1]^1,))):
            with self.subTest(size=len(broken)),self.assertRaises(ValueError):
                checks.derive_flags(encoded,broken,(0,0))
        altered=bytearray(plain);altered[checks.META+3]=6
        altered[-4:]=sum(altered[:-4]).to_bytes(4,'little')
        with self.assertRaises(ValueError):checks.derive_flags(encoded,bytes(altered),(0,0))
        with self.assertRaises(ValueError):checks.derive_flags(encoded,plain,(0,1))

    def test_output_boundary_precedes_any_write(self):
        with tempfile.TemporaryDirectory() as folder:
            parent=Path(folder);root=parent/'source';root.mkdir()
            original=root/'original_game';original.mkdir();existing=parent/'old';existing.mkdir()
            for target in (root/'evidence',original/'new',existing,parent/'missing'/'new'):
                with self.subTest(target=target),self.assertRaises(ValueError):checks.validate_output(root,target)
                self.assertFalse((target/'comparison.json').exists())
            permitted=parent/'new';self.assertEqual(checks.validate_output(root,permitted),permitted.resolve())
            self.assertFalse(permitted.exists())
            resources=parent/'actual-resource-target';resources.mkdir()
            with self.assertRaises(ValueError):checks.validate_output(root,resources/'new',resources)

    def test_confirmation_plain_then_yes_and_first_no_rejected(self):
        seen=[];predicate=checks.first_selected_yes(buttons(),lambda f,d:seen.append(f))
        self.assertIsNone(predicate('plain'));self.assertTrue(predicate('YES'));self.assertEqual(seen,['plain'])
        predicate=checks.first_selected_yes(buttons(),lambda f,d:None)
        self.assertIsNone(predicate('plain'))
        with self.assertRaisesRegex(AssertionError,'first explicit selection is NO'):predicate('NO')

    def test_ordered_batch_cannot_skip_no_to_later_yes(self):
        reader=Reader([({'index':0},'plain'),({'index':1},'NO'),({'index':2},'YES')]);record={}
        with self.assertRaisesRegex(AssertionError,'first explicit selection is NO'):
            checks.ordered_ack(reader,checks.first_selected_yes(buttons(),lambda f,d:None),lambda:None,record,clock=Clock())
        self.assertEqual([r['index'] for r in record['consumed_frames']],[0,1])
        self.assertNotIn('ACK',record)

    def test_deadline_not_restarted_by_plain_or_late_yes(self):
        clock=Clock();record={}
        def predicate(frame):clock.value=15.001;return {'selected':0}
        with self.assertRaisesRegex(TimeoutError,'late resource ACK'):
            checks.ordered_ack(Reader([({'index':0},'YES')]),predicate,lambda:None,record,clock=clock)
        self.assertNotIn('ACK',record)
        clock=Clock()
        with self.assertRaises(TimeoutError):
            checks.ordered_ack(Reader([]),lambda f:None,lambda:None,{},clock=clock)
        self.assertGreaterEqual(clock.value,15)

    def rows(self,folder):
        rows=[]
        for index in range(2):
            Image.new('RGB',(320,200),(index,2,3)).save(folder/f'frame{index:05d}.png')
            rows.append(dict(index=index,file=f'frame{index:05d}.png',pid=42,hwnd='0x123',
                wgc_timestamp_100ns=100+index,client_width=320,client_height=200))
        return rows

    def reader(self,folder):
        return checks.OrderedFrames(folder,dict(first_index=0,hwnd='0x123',wgc_timestamp_100ns=100),42,lambda f:f.convert('RGB'))

    def test_partial_jsonl_waits_without_dropping_complete_rows(self):
        for newline in (b'\n',b'\r\n'):
            with self.subTest(newline=newline),tempfile.TemporaryDirectory() as directory:
                folder=Path(directory);rows=self.rows(folder);path=folder/'frames.jsonl'
                pending=json.dumps(rows[1]).encode()
                path.write_bytes(json.dumps(rows[0]).encode()+newline+pending[:20])
                reader=self.reader(folder);self.assertEqual([r['index'] for r,f in reader.unread()],[0])
                # 真实 producer 只追加；不得重写已消费字节并改变其换行长度。
                with path.open('ab') as stream:stream.write(pending[20:]+newline)
                observed=list(reader.unread());self.assertEqual([r['index'] for r,f in observed],[1])
                self.assertEqual(observed[0][0]['sha256'],checks.fact(folder/'frame00001.png')['sha256'])

    def test_complete_bad_json_owner_index_and_timestamp_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory);rows=self.rows(folder);path=folder/'frames.jsonl'
            for field,value in (('pid',43),('hwnd','0x456'),('index',2),('wgc_timestamp_100ns',100),('client_width',640)):
                broken=copy.deepcopy(rows);broken[1][field]=value
                path.write_text(''.join(json.dumps(r)+'\n' for r in broken),encoding='utf-8')
                with self.subTest(field=field),self.assertRaises(AssertionError):list(self.reader(folder).unread())
            path.write_text('{bad}\n',encoding='utf-8')
            with self.assertRaises(ValueError):list(self.reader(folder).unread())

    def test_ready_key_owner_must_match_actual_delivery(self):
        receipt=dict(pid=42,hwnd='0x123',ok=True,error=0,noactivate_verified=True,foreground_guard_ok=True)
        record={'inputs':[dict(key='ENTER',delivery=receipt)]}
        self.assertEqual(checks.owner_receipt(record,42,'0x123'),record['inputs'][0])
        for key,value in (('pid',7),('hwnd','0x456'),('ok',False),('foreground_guard_ok',False)):
            wrong=copy.deepcopy(record);wrong['inputs'][0]['delivery'][key]=value
            with self.subTest(key=key),self.assertRaises(AssertionError):checks.owner_receipt(wrong,42,'0x123')

    def test_functional_gate_does_not_turn_collection_into_SFX_identity(self):
        process=dict(exit_code=0,cleaned=True,forced_cleanup=False)
        record=dict(kind='original',business_sequence_completed=True,audio_collection_passed=True)
        self.assertTrue(checks.qualify_case(record,process,'options')['ok'])
        self.assertFalse(checks.qualify_case(record,process,'sfx-reenable')['ok'])
        record['finite_sfx_verification']={'ok':True}
        gate=checks.qualify_case(record,process,'sfx-reenable');self.assertTrue(gate['ok'])
        self.assertFalse(gate['A7_fidelity_validated']);self.assertFalse(gate['device_output_validated'])
        for key,value in (('forced_cleanup',True),('cleaned',False),('exit_code',1)):
            wrong=dict(process,**{key:value})
            with self.subTest(key=key):self.assertFalse(checks.qualify_case(record,wrong,'sfx-reenable')['ok'])

    def good_report(self):
        return dict(cases=[dict(scenario='options',kind=kind,status='passed',qualification={'ok':True}) for kind in ('original','sdl')],
            processes=[{},{}],guards={'source':True,'capture':True},display_requirement={'status':'released'})

    def test_final_qualification_requires_both_roles_guards_and_release(self):
        report=self.good_report();self.assertTrue(checks.qualify_report(report,['options']))
        variants=[]
        value=copy.deepcopy(report);value['cases'].pop();variants.append(value)
        value=copy.deepcopy(report);value['guards']['source']=False;variants.append(value)
        value=copy.deepcopy(report);value['display_requirement']['status']='failed';variants.append(value)
        value=copy.deepcopy(report);value['cases'][0]['qualification']['ok']=False;variants.append(value)
        for value in variants:self.assertFalse(checks.qualify_report(value,['options']))

    def test_music_log_checks_success_order_not_tone(self):
        log='\n'.join(f'music request: stage=0 track={track} reason=player available=1 result=0 current={track} loop=0'
            for track in (19,-1,12,-1,19))
        result=checks.music_requests(log,'end-turn')
        self.assertEqual(result['required_ordered_subsequence'],[19,12,19])
        self.assertFalse(result['acoustic_track_identity_proven'])
        with self.assertRaises(AssertionError):checks.music_requests(log.replace('result=0','result=-1',1),'end-turn')
        with self.assertRaises(AssertionError):checks.music_requests(log.replace('track=12','track=19'),'end-turn')

    def test_native_command_has_no_implicit_DX_capture(self):
        config=checks.native_config_text(Path('owned-game'),Path('owned-captures'))
        self.assertIn('\nFD2.EXE\nexit\n',config);self.assertNotIn('DX-CAPTURE',config)
        self.assertNotIn('/O',config);self.assertNotIn('/A',config)
        self.assertIn('rate=48000',config)

    def test_error_path_finishes_recording_before_host_close(self):
        events=[]
        audio=SimpleNamespace(finish=lambda process:events.append('finalized') or {'closed':True},
            request_stop=lambda:events.append('stop'))
        record={'audio_start_attempted':True}
        with self.assertRaisesRegex(ValueError,'resource failure'):
            try:raise ValueError('resource failure')
            finally:
                checks.close_audio_before_host(audio,object(),record)
                events.append('host-close')
        self.assertEqual(events,['finalized','host-close'])
        self.assertTrue(record['audio_capture']['closed'])

    def test_opening_starts_at_title_and_never_toggles_native_at_ack(self):
        for kind in ('original','sdl'):
            with self.subTest(kind=kind),tempfile.TemporaryDirectory() as directory:
                save=Path(directory)/'FD2.SAV';save.write_bytes(b'unchanged')
                capture=SimpleNamespace(key=mock.Mock(),wait=mock.Mock(),process=object())
                menu=SimpleNamespace(wait_title=mock.Mock(),wait_state=mock.Mock())
                pages=[dict(fragment=1,token_offset=i) for i in range(29)]
                prepared=dict(tools={'run_original_battle_menu':menu,
                    'run_original_load':SimpleNamespace(require_source_unchanged=mock.Mock())},
                    assets={},titles={3:['title']},short=dict(title_counts={kind:3},
                    opening=SimpleNamespace(page_match=mock.Mock()),prefix=pages,
                    first=dict(fragment=2,token_offset=41)))
                record={'checkpoints':[]};audio=object()
                def start(*args):self.assertEqual(capture.key.call_count,0)
                with (mock.patch.object(checks,'start_audio',side_effect=start) as started,
                      mock.patch.object(checks,'finish_audio') as finished,
                      mock.patch.object(checks,'observe_audio')):
                    checks.exercise_opening(capture,kind,prepared,save,record,audio)
                started.assert_called_once()
                self.assertEqual([c.args[0] for c in capture.key.call_args_list],['ENTER']*30)
                self.assertEqual(len(record['opening_acknowledgements']),29)
                self.assertFalse(record['terminal_wait_acknowledged'])
                self.assertEqual(record.get('audio_finalize_on_normal_close',False),kind=='original')
                self.assertEqual(finished.call_count,int(kind=='sdl'))

    def test_opening_native_cleanup_sends_no_ack_and_seals_only_normal_exit(self):
        audio=mock.Mock()
        checks.close_audio_before_host(audio,object(),{'audio_finalize_on_normal_close':True})
        audio.finish.assert_not_called();audio.request_stop.assert_not_called()
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory);(folder/'closed.wav').write_bytes(b'closed')
            capture=SimpleNamespace(key=mock.Mock(),trace=[],process=None)
            process=SimpleNamespace(pid=42,poll=mock.Mock(return_value=0))
            capture.process=process
            native=checks.NativeWave(capture,folder,SimpleNamespace(wav_info=lambda p,expected_rate:
                {'frames':10,'sample_rate':expected_rate,'path':str(p)}))
            native.started=True;native.active=True
            with mock.patch.object(checks.time,'sleep'):
                result=native.finish_after_exit(process)
            self.assertFalse(result['stop_delivery']['game_key_sent'])
            capture.key.assert_not_called()
            process.poll.return_value=1
            with self.assertRaises(RuntimeError):native.finish_after_exit(process)

    def test_finish_failure_keeps_first_error_and_is_not_retried(self):
        audio=mock.Mock();audio.finish.side_effect=RuntimeError('finalize failure')
        record={'audio_start_attempted':True}
        with self.assertRaisesRegex(ValueError,'original error'):
            try:raise ValueError('original error')
            finally:checks.close_audio_before_host(audio,object(),record)
        self.assertEqual(record['capture_close_error']['message'],'finalize failure')
        checks.close_audio_before_host(audio,object(),record)
        self.assertEqual(audio.finish.call_count,1)
        self.assertEqual(audio.request_stop.call_count,1)

    def test_cli_default_dry_never_calls_execution_or_display(self):
        prepared=dict(original_facts={},inputs={},source={},tools={})
        argv=['--game','game','--driver','driver','--dosbox','dosbox','--bootstrap','bootstrap','--artifacts','new-output']
        with (mock.patch.object(checks,'prepare',return_value=prepared),
                mock.patch.object(checks,'execute',side_effect=AssertionError('execute forbidden')) as execute,
                mock.patch.object(subprocess,'Popen',side_effect=AssertionError('Popen forbidden')),
                contextlib.redirect_stdout(io.StringIO()) as out):
            self.assertEqual(cli.main(argv),0)
        self.assertFalse(execute.called)
        result=json.loads(out.getvalue());self.assertEqual(result['status'],'ready_not_executed')
        self.assertFalse(result['ok']);self.assertFalse(result['GUI_or_device_or_display_API_called'])

    def test_report_publish_is_exclusive(self):
        with tempfile.TemporaryDirectory() as directory:
            target=Path(directory)/'comparison.json';cli.publish(target,{'ok':False})
            original=target.read_bytes()
            with self.assertRaises(FileExistsError):cli.publish(target,{'ok':True})
            self.assertEqual(target.read_bytes(),original)
            self.assertFalse((Path(directory)/'comparison.json.tmp').exists())


class ActiveLoadObserverTest(unittest.TestCase):
    def run_observer(self,samples,*,stop_reason='stop',finish_error=None,helper_exit=None,
                     tamper=False,wrong_owner=False,extra_key=False,integration=False):
        import run_g4_detail_check as detail
        import run_original_reward as reward
        clock=Clock();events=[];commands=[];key_calls=[];record=dict(inputs=[],checkpoints=[],saves=[],events=[],cancellations=[])
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory);save=folder/'FD2.SAV';save.write_bytes(b'owned unchanged')
            guard=[save.read_bytes(),save.stat().st_mtime_ns]
            ready=dict(ready=True,pid=42,hwnd='0x123',first_index=0,wgc_timestamp_100ns=100)
            process=SimpleNamespace(pid=42,poll=lambda:None)
            helper_process=SimpleNamespace(poll=lambda:helper_exit)
            capture=SimpleNamespace(process=process,driver=Path('owned-driver'),artifacts=folder,last=None)
            def key(name):
                events.append('key:'+name);key_calls.append(name)
                record['inputs'].append(dict(key=name,time=clock.monotonic(),delivery=dict(pid=42,hwnd='0x123',ok=True,
                    error=0,noactivate_verified=True,foreground_guard_ok=True)))
                if extra_key:record['inputs'].append(dict(record['inputs'][-1]))
            capture.key=key;capture.wait=mock.Mock()
            key('ENTER');key_calls.clear();events.clear()
            codes={index:(when,kind) for index,(when,kind) in enumerate(samples)}
            rows=[]
            class Helper:
                def __init__(self,driver,args,work,label,slot):
                    self.series=Path(args[3]);commands.append(args);self.slot=slot
                def start(self):
                    self.series.mkdir()
                    for index in codes:
                        name=f'frame{index:05d}.png'
                        Image.new('RGB',(320,200),(index,2,3)).save(self.series/name)
                        rows.append(dict(index=index,file=name,pid=42,hwnd='0x123',wgc_timestamp_100ns=100+index,
                            client_width=320,client_height=200))
                    (self.series/'frames.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in rows),encoding='utf-8')
                    (self.series/'ready.json').write_text(json.dumps(ready),encoding='utf-8')
                    return helper_process
                def finish(self):
                    events.append('stop');self.slot['cleaned']=True
                    if finish_error:raise finish_error
                    if tamper:
                        point=record['active_load_observation']['checkpoints'][-1]['series_frame']
                        Image.new('RGB',(320,200),(254,2,3)).save(point['path'])
                    return dict(ok=True,backend='WGC-HWND-session',pid=42,hwnd='0x123',ready_published=True,
                        session_started=True,stop_reason=stop_reason,hresult=0,color_filter=False,frames=len(rows),
                        first_wgc_timestamp_100ns=100,last_wgc_timestamp_100ns=rows[-1]['wgc_timestamp_100ns'])
            def wait_ready(*args,**kwargs):
                events.append('ready');return dict(ready,pid=7) if wrong_owner else ready
            def resource(frame):
                when,kind=codes[frame.getpixel((0,0))[0]];clock.value=when
                return kind
            def round_state(frame,*args):
                kind=resource(frame)
                return dict(kind='complete_round',phase=kind) if kind.startswith('round') else None
            def browse(frame):
                kind=resource(frame)
                return dict(kind='complete_browse',dialog_all_phase_edges_absent=True) if kind=='browse' else None
            def weak_browse(frame):
                # 原单cursor谓词会接受关闭phase/峰，quiet自身也只查其局部资源。
                kind=resource(frame)
                return dict(kind='weak_browse') if kind in ('browse','dialog177','round-overlay') else None
            require=mock.Mock()
            tools={'run_g4_detail_check':detail,'run_original_reward':reward,
                'run_original_load':SimpleNamespace(require_source_unchanged=require,wait_measured_save=mock.Mock()),
                'run_original_battle_menu':SimpleNamespace(wait_title=mock.Mock(),MENU_OFFSETS=()),
                'run_original_end_turn':SimpleNamespace(RoundOracle=lambda *a:SimpleNamespace(
                    menu=lambda *a:True,secondary=lambda *a:True,closed_browse=lambda *a:True,
                    cancellation_notice=lambda *a:True,buttons=lambda *a:True)),
                'run_original_battle_animation':SimpleNamespace(qpc_stamp=lambda:{'fixture':True})}
            prepared=dict(tools=tools,assets={'palette':[]},original=folder,titles={3:['title','other','continue']})
            saved=bytearray(0x59cb);saved[checks.META]=1
            saved[checks.META+16:checks.META+18]=bytes((1,1));saved[-4:]=sum(saved[:-4]).to_bytes(4,'little')
            saved=bytes(saved);prepared['plain']=saved;tools['run_original_load'].wait_measured_save.return_value=saved
            tools['run_original_load'].check_load_measurement=mock.Mock()
            error=None
            with (mock.patch.object(detail,'OwnedHelper',Helper),mock.patch.object(detail,'wait_ready',side_effect=wait_ready),
                  mock.patch.object(reward,'load_round_template',return_value='source_template'),
                  mock.patch.object(reward,'load_round_state',side_effect=round_state),
                  mock.patch.object(reward,'loaded_browse_state',return_value=browse),
                  mock.patch.object(reward,'resource_browse',return_value=weak_browse),
                  mock.patch.object(checks.time,'monotonic',clock.monotonic),mock.patch.object(checks.time,'sleep',clock.sleep)):
                try:
                    if integration:
                        def confirmation(c,p,r,o,k,label):k('ENTER')
                        with (mock.patch.object(checks,'checkpoint',return_value=(None,{})),
                              mock.patch.object(checks,'wait_confirmation',side_effect=confirmation),
                              mock.patch.object(checks,'start_audio'),mock.patch.object(checks,'finish_audio'),
                              mock.patch.object(checks,'stop_audio')):
                            checks.exercise_options_load(capture,'sdl',prepared,save,record,
                                SimpleNamespace(assert_running=mock.Mock()),'active-load')
                    else:checks.observe_audio_active_load(capture,prepared,saved,save,guard,record,key)
                except BaseException as caught:error=caught
            return dict(record=record,error=error,events=events,keys=key_calls,commands=commands,
                guard_calls=require.call_count,save_unchanged=save.read_bytes()==guard[0])

    def test_load_ready_round_then_full_browse_quiet_precedes_next_input(self):
        result=self.run_observer([(0,'dialog177'),(.2,'round-peak'),(.4,'browse'),(2,'dialog177'),
            (2.5,'browse'),(4.6,'browse')])
        self.assertIsNone(result['error']);self.assertEqual(result['keys'],['ENTER'])
        self.assertEqual(result['events'],['ready','key:ENTER','stop'])
        self.assertEqual(result['commands'][0][-2:],[45000,33])
        state=result['record']['active_load_observation'];self.assertTrue(state['completed'])
        self.assertEqual([p['name'] for p in state['checkpoints']],['loaded-A-round','loaded-A-browse'])
        self.assertEqual(state['checkpoints'][0]['series_frame']['index'],1)
        self.assertEqual(state['checkpoints'][1]['series_frame']['index'],5)
        self.assertTrue(state['detail_series']['checkpoint_PNGs_bound_to_closed_series'])
        self.assertEqual(state['loaded-A-browse_quiet']['first'],2.5)
        self.assertEqual(state['loaded-A-browse_quiet']['samples'],2)
        self.assertGreater(state['loaded-A-browse_quiet']['resets'],0)
        self.assertEqual(result['guard_calls'],2);self.assertTrue(result['save_unchanged'])
        self.assertEqual(state['timeout_seconds'],45)
        self.assertFalse(state['native_PC_or_tick_timing_claimed'])

    def test_each_round_overlay_resets_existing_quiet_interval(self):
        for phase in ('round-peak','round-exit2','round-exit3','round-exit4','round-exit9'):
            result=self.run_observer([(0,'round-peak'),(.1,'browse'),(2,phase),(2.1,'browse'),(4.2,'browse')])
            with self.subTest(phase=phase):
                self.assertIsNone(result['error'])
                state=result['record']['active_load_observation']
                self.assertEqual(state['loaded-A-browse_quiet']['first'],2.1)
                self.assertEqual(state['checkpoints'][-1]['series_frame']['index'],4)

    def test_missing_round_missing_browse_or_unquiet_never_sends_extra_input(self):
        for samples in ([(0,'browse'),(.1,'browse')],[(0,'round-peak'),(.1,'dialog177')],
                        [(0,'round-peak'),(.1,'browse'),(1,'browse')]):
            result=self.run_observer(samples)
            with self.subTest(samples=samples):
                self.assertIsInstance(result['error'],TimeoutError)
                self.assertEqual(result['keys'],['ENTER']);self.assertEqual(result['events'][-1],'stop')
                self.assertFalse(result['record']['active_load_observation']['completed'])

    def test_whole_load_deadline_does_not_restart_after_round(self):
        result=self.run_observer([(0,'dialog177'),(44,'round-peak'),(44.1,'browse'),(46.2,'browse')])
        self.assertIsInstance(result['error'],TimeoutError)
        self.assertEqual(result['keys'],['ENTER']);self.assertFalse(result['record']['active_load_observation']['completed'])

    def test_stop_cap_or_closed_PNG_substitution_rejects_ready_without_next_key(self):
        samples=[(0,'round-peak'),(.1,'browse'),(2.2,'browse')]
        for options in ({'stop_reason':'frame_limit'},{'stop_reason':'time_limit'},
                        {'finish_error':RuntimeError('owned stop failed')},{'tamper':True}):
            result=self.run_observer(samples,**options)
            with self.subTest(options=options):
                self.assertIsInstance(result['error'],(RuntimeError,AssertionError))
                self.assertFalse(result['record']['active_load_observation']['completed'])
                self.assertEqual(result['keys'],['ENTER'])
                self.assertEqual(result['events'][-1],'stop')

    def test_body_timeout_keeps_first_error_when_stop_also_fails(self):
        result=self.run_observer([(0,'dialog177'),(.1,'dialog177')],finish_error=RuntimeError('later stop failed'))
        self.assertIsInstance(result['error'],TimeoutError)
        state=result['record']['active_load_observation']['detail_series']
        self.assertIn('later stop failed',state['cleanup_error'])
        self.assertIn('超时',state['error']);self.assertEqual(result['keys'],['ENTER'])

    def test_wrong_ready_owner_and_premature_helper_exit_stop_without_retry(self):
        result=self.run_observer([(0,'dialog177'),(.1,'dialog177')],wrong_owner=True)
        self.assertIsInstance(result['error'],ValueError);self.assertEqual(result['keys'],[])
        result=self.run_observer([(0,'dialog177'),(.1,'dialog177')],helper_exit=0)
        self.assertIsInstance(result['error'],RuntimeError);self.assertEqual(result['keys'],['ENTER'])
        result=self.run_observer([(0,'round-peak'),(.1,'browse'),(2.2,'browse')],extra_key=True)
        self.assertIsInstance(result['error'],AssertionError)
        self.assertFalse(result['record']['active_load_observation']['completed'])

    def test_real_exercise_success_keeps_existing_keys_Save_and_audio_sequence(self):
        result=self.run_observer([(0,'dialog177'),(.2,'round-exit2'),(.3,'browse'),(2.4,'browse')],integration=True)
        self.assertIsNone(result['error']);self.assertTrue(result['record']['business_sequence_completed'])
        index=result['events'].index('ready')
        self.assertEqual(result['events'][index:index+3],['ready','key:ENTER','stop'])
        self.assertEqual(result['events'][index+3],'key:ENTER')  # 原open_options在闭帧之后。
        expected=['UP','ENTER','ENTER','UP','ENTER','LEFT','ENTER','ENTER',
            'ENTER','RIGHT','ENTER','ENTER','ESC','ESC',
            'ENTER','RIGHT','ENTER','LEFT','ENTER','ESC','ESC']
        for branch in ('escape','no'):
            expected+=['ENTER','UP','ENTER','RIGHT','ENTER']
            if branch=='no':expected+=['RIGHT']
            expected+=['ESC' if branch=='escape' else 'ENTER','ENTER','RIGHT','ENTER','ESC','ESC']
        expected+=['ENTER','UP','ENTER','RIGHT','ENTER','ENTER',
            'ENTER','RIGHT','ENTER','ESC','ESC','ENTER','UP','ENTER','LEFT','ENTER','ENTER']
        self.assertEqual(result['keys'],expected)
        self.assertNotIn('HOST_W',result['keys']);self.assertEqual(len(result['commands']),1)
        self.assertEqual([row['name'] for row in result['record']['saves']],['baseline','loaded-A-final'])
        self.assertTrue(result['save_unchanged'])




class CurrentProfileReferenceTest(unittest.TestCase):
    def test_current_profile_and_strict_bootstrap_argument(self):
        args=cli.parser().parse_args(['--game','g','--driver','d','--dosbox','x',
            '--bootstrap','fixed.conf','--artifacts','new','--scenarios','sfx-reenable'])
        self.assertEqual(args.bootstrap,Path('fixed.conf'));self.assertFalse(args.execute)
        text=checks.native_config_text(Path('owned'),Path('captures'))
        self.assertIn('rate=48000',text);self.assertNotIn('44100',text)
        self.assertIn('FD2.EXE\nexit\n',text)

    def test_closed_native_wave_requires_current_rate(self):
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory);(folder/'one.wav').write_bytes(b'fixture')
            process=SimpleNamespace(pid=42,poll=lambda:0)
            native=checks.NativeWave(SimpleNamespace(process=process,trace=[],key=mock.Mock()),folder,
                SimpleNamespace(wav_info=mock.Mock(side_effect=AssertionError('not 48000'))))
            native.started=True
            with mock.patch.object(checks.time,'sleep'):
                with self.assertRaisesRegex(AssertionError,'not 48000'):native.finish_after_exit(process)
            native.battle.wav_info.assert_called_once_with(folder/'one.wav',expected_rate=48000)

    def test_reference_before_and_owned_argv(self):
        import original_reference
        import run_original_comparison as common
        with tempfile.TemporaryDirectory() as directory:
            work=Path(directory);config=work/'dosbox.conf';config.write_text('frozen')
            args=SimpleNamespace(dosbox=Path('dosbox-x.exe'),bootstrap=Path('fixed.conf'))
            record=dict(scenario='sfx-reenable');command=['dosbox-x.exe','-conf',str(config),'-noconsole']
            value={'configs':[checks.fact(config)]}
            def publish(path,receipt):path.write_text('{}')
            prepared=dict(original=Path('not-read'),inputs={},tools={'original_reference':original_reference,
                'run_original_comparison':common})
            with (mock.patch.object(original_reference,'build',return_value=value) as build,
                  mock.patch.object(original_reference,'write_new',side_effect=publish),
                  mock.patch.object(original_reference,'verify',return_value=value) as verify):
                checks.qualify_native_reference(args,prepared,record,config,command,{})
                self.assertTrue(record['reference_checks']['before'])
                self.assertEqual(record['expected_actual_command'][-2:],['-set','mixer nosound=true'])
                build.assert_called_once_with(Path('not-read'),Path('dosbox-x.exe'),[config],
                    ['mixer nosound=true'],Path('fixed.conf'))
                checks.finish_native_reference(prepared,record,business_failed=False)
                self.assertTrue(record['reference_checks']['after']);self.assertEqual(verify.call_count,2)

    def test_configuration_race_cannot_relabel_build_input(self):
        import original_reference
        import run_original_comparison as common
        with tempfile.TemporaryDirectory() as directory:
            work=Path(directory);config=work/'dosbox.conf';config.write_text('first')
            value={'configs':[checks.fact(config)]};config.write_text('other')
            args=SimpleNamespace(dosbox=Path('dosbox-x.exe'),bootstrap=Path('fixed.conf'))
            prepared=dict(original=Path('not-read'),inputs={},tools={'original_reference':original_reference,
                'run_original_comparison':common})
            with (mock.patch.object(original_reference,'build',return_value=value),
                  mock.patch.object(original_reference,'write_new') as publish):
                with self.assertRaisesRegex(AssertionError,'冻结后改变'):
                    checks.qualify_native_reference(args,prepared,{},config,
                        ['dosbox-x.exe','-conf',str(config),'-noconsole'],{})
                publish.assert_not_called()

    def test_late_reference_failure_preserves_primary_business(self):
        reference=SimpleNamespace(verify=mock.Mock(side_effect=ValueError('late reference')))
        prepared={'tools':{'original_reference':reference}}
        record=dict(reference_manifest='reference.json',reference_checks=dict(before=True,after=False))
        with self.assertRaisesRegex(RuntimeError,'first resource ACK'):
            try:raise RuntimeError('first resource ACK')
            finally:checks.finish_native_reference(prepared,record,business_failed=True)
        self.assertFalse(record['reference_checks']['after'])
        self.assertEqual(record['reference_after_error']['message'],'late reference')
        with self.assertRaisesRegex(ValueError,'late reference'):
            checks.finish_native_reference(prepared,record,business_failed=False)

    def test_reenable_save_needs_original_class_not_any_measured_class(self):
        import run_original_load as load
        plain=bytearray(0x59cb);plain[checks.META+1]=3
        for raw,identity in enumerate((7,2,7)):
            plain[load.UNITS+raw*load.STRIDE+7]=identity
            plain[load.UNITS+raw*load.STRIDE+2]=12
        plain[-4:]=sum(plain[:-4]).to_bytes(4,'little')
        measured=bytearray(plain);measured[checks.META+17]=1
        for raw,expected in enumerate((0,1,0)):measured[load.UNITS+raw*load.STRIDE+2]=expected
        measured[-4:]=sum(measured[:-4]).to_bytes(4,'little')
        prepared={'plain':plain,'tools':{'run_original_load':load}}
        good=checks.check_reenable_save(prepared,measured)
        self.assertTrue(good['cache_classes_original_10010_verified'])
        measured[load.UNITS+2]=3;measured[-4:]=sum(measured[:-4]).to_bytes(4,'little')
        with self.assertRaisesRegex(AssertionError,'cache class differs'):
            checks.check_reenable_save(prepared,measured)
        measured[load.UNITS+2]=0;measured[123]=1
        measured[-4:]=sum(measured[:-4]).to_bytes(4,'little')
        with self.assertRaisesRegex(AssertionError,'outside the declared'):
            checks.check_reenable_save(prepared,measured)

if __name__=='__main__':unittest.main()
