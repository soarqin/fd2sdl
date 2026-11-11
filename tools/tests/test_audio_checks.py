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
        record=dict(business_sequence_completed=True,audio_collection_passed=True)
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
        self.assertIn('rate=44100',config)

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
            native=checks.NativeWave(capture,folder,SimpleNamespace(wav_info=lambda p:
                {'frames':10,'sample_rate':44100,'path':str(p)}))
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
        argv=['--game','game','--driver','driver','--dosbox','dosbox','--artifacts','new-output']
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


if __name__=='__main__':unittest.main()
