"""G4详情native工具的独立资源/整档/来源负例；本文件不启动窗口。"""
from pathlib import Path
import json
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from PIL import Image

TOOLS=Path(__file__).resolve().parents[1];sys.path.insert(0,str(TOOLS))
import run_g4_detail_check as check
ROOT=TOOLS.parent


class DetailCheckTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original=ROOT/'original_game'
        cls.prepared={name:check.prepare(cls.original,name) for name in check.SCENARIOS}

    def frame(self,page='equipment',phase=0):
        prepared=self.prepared['f2-magic'];frame=Image.new('RGB',(320,200))
        for name in ('unselected','actor_name','current_max_mp'):
            at,image=prepared[page][name];frame.paste(image.convert('RGB'),at)
        frame.paste(prepared['portraits'][phase==3].convert('RGB'),(8,10))
        return frame

    def test_complete_original_functions_and_both_literal_zero_waits(self):
        source=check.source_contract(self.original)
        self.assertEqual(source['complete_functions']['0x17aed']['size'],642)
        self.assertEqual(source['complete_functions']['0x16c57']['size'],461)
        self.assertEqual([c['call_cva'] for c in source['call_anchors'] if c['target_cva']=='0x16c57'],['0x17b06','0x17ce6'])
        self.assertFalse(source['native_PC_hits_claimed'])
        real=check.source_range
        def tampered(exe,at,size):
            raw=real(exe,at,size)
            return bytes((raw[0]^1,))+raw[1:] if at==0x17aed else raw
        with patch.object(check,'source_range',side_effect=tampered),self.assertRaises(ValueError):
            check.source_contract(self.original)

    def test_frame3_gate_preserves_all136_original_portraits(self):
        domain=check.portrait_domain(self.original)
        self.assertEqual((domain['portraits'],domain['checked_images'],domain['all_dimensions']),(136,272,[80,80]))
        self.assertEqual({r['portrait'] for r in domain['images']},set(range(136)))
        self.assertFalse(domain['runtime_execution_claimed'])

    def test_fixture_only_focus_and_acted_byte_and_roundtrip(self):
        before=check.decode_save(self.original/'START.SAV')
        for scenario,p in self.prepared.items():
            allowed=set(range(check.META+5,check.META+9))
            if scenario.startswith('enter'):allowed.add(check.UNITS+check.STRIDE+5)
            changed={i for i,(a,b) in enumerate(zip(before[:-4],p['plain'][:-4])) if a!=b}
            self.assertTrue(changed and changed<=allowed)
            self.assertEqual(changed,{int(r['offset'],16) for r in p['manifest']['controlled_changes_before_checksum']})
            self.assertEqual(p['plain'][:check.UNITS],before[:check.UNITS])
            self.assertEqual(p['plain'][check.UNITS+check.STRIDE+5]&0x80,0 if scenario=='f2-magic' else 0x80)
            self.assertFalse(p['manifest']['native_controller_PC_count_claimed'])
            with tempfile.TemporaryDirectory() as directory:
                path=Path(directory)/'controlled.SAV';path.write_bytes(p['encoded'])
                self.assertEqual(check.decode_save(path),p['plain'])

    def test_complete_portrait_and_page_not_merely_nonzero_edges(self):
        p=self.prepared['f2-magic']
        for page in ('equipment','magic'):
            for phase in (0,3):
                frame=self.frame(page,phase);result=check.detail_state(frame,p,page,phase)
                self.assertEqual(result['portrait']['checked_pixels'],6400)
                self.assertEqual(result['rectangles']['unselected']['checked_pixels'],30690)
                self.assertTrue(result['full_portrait_including_index0'] and result['no_arrow_full_bottom_checked'])
                self.assertFalse(result['runtime_RNG_count_measured'])
                self.assertIsNone(check.detail_state(frame,p,page,3 if phase==0 else 0))
                self.assertIsNone(check.detail_state(frame,p,'magic' if page=='equipment' else 'equipment',phase))
                for point in ((8,10),(87,89),(191,181),(314,192),(5,94)):
                    bad=frame.copy();rgb=bad.getpixel(point);bad.putpixel(point,tuple(255-c for c in rgb))
                    self.assertIsNone(check.detail_state(bad,p,page,phase),point)

    def test_portrait_wrong_origin_and_wrong_page_are_rejected(self):
        p=self.prepared['f2-magic'];frame=self.frame()
        frame.paste((0,0,0),(8,10,88,90));frame.paste(p['portraits'][0].convert('RGB'),(9,10))
        self.assertIsNone(check.detail_state(frame,p,'equipment',0))
        with self.assertRaises(ValueError):check.detail_state(frame,p,'other',0)
        with self.assertRaises(ValueError):check.detail_state(frame,p,'magic',1)

    def test_full_Save_measurement_cannot_hide_gameplay_or_unknown_bytes(self):
        p=self.prepared['f2-magic'];source=p['plain'];got=bytearray(source)
        focus=(100,100);got[check.META+5:check.META+9]=bytes((9,15,4,4))
        got[-4:]=sum(got[:-4]).to_bytes(4,'little')
        check.check_load_measurement(source,got,[('left-empty',focus)])
        for at in (check.UNITS+check.STRIDE+5,check.UNITS+check.STRIDE+0x44,0x8a3,0x30a3,0x59c6):
            bad=bytearray(got);bad[at]^=1;bad[-4:]=sum(bad[:-4]).to_bytes(4,'little')
            with self.assertRaises(AssertionError):check.check_load_measurement(source,bad,[('left-empty',focus)])

    def test_prepare_never_requests_display_process_or_original_write(self):
        with self.assertRaises(ValueError):check.run(SimpleNamespace(root=ROOT,artifacts=self.original,run=False))
        with tempfile.TemporaryDirectory() as directory,patch.object(check,'running') as launch,patch.object(check,'windows_display_requirement') as display:
            output=Path(directory)/'prepared';result=check.run(SimpleNamespace(root=ROOT,artifacts=output,run=False))
            self.assertFalse(result['ok']);self.assertEqual(result['status'],'prepared_only')
            self.assertEqual(len(list(output.glob('*.SAV'))),2)
            launch.assert_not_called();display.assert_not_called()
            duplicate=Path(directory)/'duplicate'
            with self.assertRaises(ValueError):check.run(SimpleNamespace(root=ROOT,artifacts=duplicate,run=False,scenario=['f2-magic']*2))
            self.assertFalse(duplicate.exists())

    def publish_series(self,directory):
        directory.mkdir();rows=[]
        for index,phase in enumerate((0,3,0,0)):
            path=directory/f'frame{index:05d}.png';self.frame('equipment',phase).save(path)
            rows.append({'index':index,'file':path.name,'pid':42,'hwnd':'0x123',
                'wgc_timestamp_100ns':100000+index*330000,'client_width':320,'client_height':200})
        (directory/'frames.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in rows),encoding='utf-8')
        ready={'ready':True,'pid':42,'hwnd':'0x123','first_index':0,'wgc_timestamp_100ns':100000}
        (directory/'ready.json').write_text(json.dumps(ready),encoding='utf-8')
        return ready,rows

    def test_series_consumes_short_blink_even_if_latest_frame_already_open(self):
        with tempfile.TemporaryDirectory() as folder:
            work=Path(folder);directory=work/'detail-series';ready,rows=self.publish_series(directory)
            process=SimpleNamespace(pid=42,poll=lambda:None)
            capture=SimpleNamespace(process=process,artifacts=work,last=None)
            scope={'directory':directory,'ready':ready,'process':process,'after':None};points=[]
            for label,phase in (('open',0),('blink',3),('restored',0)):
                check.series_checkpoint(capture,scope,label,
                    lambda f,n=phase:check.detail_state(f,self.prepared['f2-magic'],'equipment',n),points,timeout=1)
            self.assertEqual([p['series_frame']['index'] for p in points],[0,1,2])
            self.assertTrue(all(p['no_additional_WGC_session'] for p in points))
            for point in points:
                self.assertEqual(check.sha(point['series_frame']['path']),point['series_frame']['sha256'])
                self.assertEqual(check.sha(point['normalized_PNG']['path']),point['normalized_PNG']['sha256'])

    def test_series_wrong_owner_or_reversed_clock_rejected_before_check(self):
        for field,value in (('pid',43),('hwnd','0x456'),('wgc_timestamp_100ns',90000)):
            with tempfile.TemporaryDirectory() as folder:
                work=Path(folder);directory=work/'detail-series';ready,rows=self.publish_series(directory)
                rows[1][field]=value
                (directory/'frames.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in rows),encoding='utf-8')
                process=SimpleNamespace(pid=42,poll=lambda:None)
                capture=SimpleNamespace(process=process,artifacts=work,last=None)
                scope={'directory':directory,'ready':ready,'process':process,'after':None}
                with self.assertRaises(AssertionError):check.series_checkpoint(capture,scope,'bad',lambda f:True,[],timeout=1)
                self.assertFalse((work/'bad.png').exists())

    def test_series_body_failure_still_stops_and_closes_owned_helper(self):
        with tempfile.TemporaryDirectory() as folder:
            work=Path(folder);process=SimpleNamespace(pid=42,poll=lambda:None)
            capture=SimpleNamespace(process=process,artifacts=work,last=None,driver=work/'driver.exe')
            test=self;finished=[]
            class Helper:
                def __init__(self,*args):pass
                def start(self):
                    self.ready,self.rows=test.publish_series(work/'detail-series');return process
                def finish(self):
                    finished.append((work/'detail-series/stop').is_file())
                    return {'ok':True,'backend':'WGC-HWND-session','pid':42,'hwnd':'0x123',
                        'ready_published':True,'session_started':True,'stop_reason':'stop','hresult':0,
                        'color_filter':False,'frames':4,'first_wgc_timestamp_100ns':self.rows[0]['wgc_timestamp_100ns'],
                        'last_wgc_timestamp_100ns':self.rows[-1]['wgc_timestamp_100ns']}
            record={'checkpoints':[]}
            with patch.object(check,'OwnedHelper',Helper),self.assertRaisesRegex(RuntimeError,'explicit fixture failure'):
                with check.detail_series(capture,record) as scope:
                    check.series_checkpoint(capture,scope,'open',lambda f:check.detail_state(
                        f,self.prepared['f2-magic'],'equipment',0),record['checkpoints'],timeout=1)
                    raise RuntimeError('explicit fixture failure')
            self.assertEqual(finished,[True])
            self.assertTrue(record['detail_series']['checkpoint_PNGs_bound_to_closed_series'])

    def series_failure(self, body_error=None, cleanup='time_limit', ready_error=None):
        with tempfile.TemporaryDirectory() as folder:
            work=Path(folder);process=SimpleNamespace(pid=42,poll=lambda:None)
            capture=SimpleNamespace(process=process,artifacts=work,last=None,driver=work/'driver.exe')
            test=self;finished=[];ready=None
            class Helper:
                def __init__(self,driver,arguments,output,label,record):
                    test.assertEqual(arguments,[driver,'capture-series',42,work/'detail-series',45000,33])
                    self.record=record
                def start(self):
                    nonlocal ready
                    ready,self.rows=test.publish_series(work/'detail-series');return process
                def finish(self):
                    finished.append((work/'detail-series/stop').is_file())
                    self.record['cleaned']=True
                    if cleanup=='finish':raise RuntimeError('owned helper close failed')
                    summary={'ok':True,'backend':'WGC-HWND-session','pid':42,'hwnd':'0x123',
                        'ready_published':True,'session_started':True,'stop_reason':cleanup,'hresult':0,
                        'color_filter':False,'frames':4,'first_wgc_timestamp_100ns':self.rows[0]['wgc_timestamp_100ns'],
                        'last_wgc_timestamp_100ns':self.rows[-1]['wgc_timestamp_100ns']}
                    self.record['summary']=summary
                    return summary
            def get_ready(*args):
                if ready_error is not None:raise ready_error
                return ready
            record={'checkpoints':[]};raised=None
            with patch.object(check,'OwnedHelper',Helper),patch.object(check,'wait_ready',get_ready):
                try:
                    with check.detail_series(capture,record):
                        if body_error is not None:raise body_error
                except BaseException as error:raised=error
            return record,raised,finished

    def test_series_preserves_first_body_error_or_cancel_despite_cleanup_failure(self):
        for error in (TimeoutError('required complete Round missing'),KeyboardInterrupt('cancel fixture')):
            for cleanup in ('finish','time_limit'):
                with self.subTest(error=type(error).__name__,cleanup=cleanup):
                    record,raised,finished=self.series_failure(error,cleanup)
                    self.assertIs(raised,error)
                    evidence=record['detail_series']
                    self.assertEqual(evidence['error'],str(error))
                    self.assertIn('cleanup_error',evidence)
                    self.assertTrue(evidence['helper']['cleaned'])
                    self.assertEqual(finished,[True])
                    self.assertNotIn('checkpoint_PNGs_bound_to_closed_series',evidence)

    def test_series_clean_body_still_fails_on_time_limit_or_owned_cleanup_error(self):
        for cleanup in ('finish','time_limit'):
            with self.subTest(cleanup=cleanup):
                record,raised,finished=self.series_failure(cleanup=cleanup)
                self.assertIsInstance(raised,(RuntimeError,AssertionError))
                evidence=record['detail_series']
                self.assertEqual(evidence['cleanup_error'],str(raised))
                self.assertNotIn('error',evidence)
                self.assertEqual(finished,[True])
                self.assertNotIn('checkpoint_PNGs_bound_to_closed_series',evidence)

    def test_series_ready_error_is_preserved_and_helper_is_closed(self):
        first=TimeoutError('ready fixture failed before confirmation')
        record,raised,finished=self.series_failure(cleanup='finish',ready_error=first)
        self.assertIs(raised,first)
        self.assertEqual(finished,[True])
        self.assertEqual(record['detail_series']['error'],str(first))
        self.assertEqual(record['detail_series']['cleanup_error'],'owned helper close failed')

    def test_single_endpoint_preparation_never_claims_paired_native(self):
        with tempfile.TemporaryDirectory() as directory:
            result=check.run(SimpleNamespace(root=ROOT,artifacts=Path(directory)/'sdl',run=False,kind='sdl'))
            self.assertEqual(result['requested_endpoints'],['sdl'])
            self.assertFalse(result['paired_result'] or result['ok'])


if __name__=='__main__':unittest.main()
