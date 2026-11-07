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


if __name__=='__main__':unittest.main()
