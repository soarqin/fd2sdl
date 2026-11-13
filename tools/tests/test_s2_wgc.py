"""S2附加WGC真实来源合并与有界调度；纯合成PNG，不启动窗口或子进程。"""
import copy
import json
from pathlib import Path
import sys
import tempfile
import time
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from PIL import Image
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import fd2_s2_wgc as wgc
import run_s2_ticks_check as ticks


class FreshWGCTests(unittest.TestCase):
    def setUp(self):
        self.temp=tempfile.TemporaryDirectory();self.addCleanup(self.temp.cleanup);self.root=Path(self.temp.name)
        self.capture=SimpleNamespace(artifacts=self.root,driver=self.root/'driver.exe',
            process=SimpleNamespace(pid=71,poll=lambda:None))

    def session(self,name,items):
        directory=self.root/name;directory.mkdir(exist_ok=True);rows=[]
        for index,(pose,stamp) in enumerate(items):
            file=f'frame{index:05d}.png';Image.new('RGBA',(320,200),(pose,0,0,255)).save(directory/file)
            rows.append({'index':index,'file':file,'wgc_timestamp_100ns':stamp,'pid':71,'hwnd':'0x29',
                'client_width':320,'client_height':200,'content_width':320,'content_height':200,'crop_x':0,'crop_y':0})
        ready={'ready':True,'pid':71,'hwnd':'0x29','first_index':0,'wgc_timestamp_100ns':rows[0]['wgc_timestamp_100ns'],
            'client_width':320,'client_height':200}
        (directory/'frames.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in rows))
        (directory/'ready.json').write_text(json.dumps(ready))
        return directory,ready,wgc.live_frames(directory,ready,71)

    def watcher(self,items=((1,1_000_000),)):
        self.session('action-series',items)
        watcher=wgc.FreshPlateauWatcher(self.capture,{},self.match)
        watcher.clock={'qpc':0,'qpc_frequency':10_000_000};watcher.started=time.monotonic();watcher.deadline=watcher.started+12
        return watcher

    @staticmethod
    def match(row):
        with Image.open(row['path']) as image:value=image.getpixel((0,0))[0]
        return None if value==9 else value

    def test_live_ignores_only_incomplete_tail_and_rejects_rewritten_source(self):
        directory,ready,rows=self.session('primary',[(1,1_000_000),(9,1_100_000),(2,1_200_000)])
        path=directory/'frames.jsonl';text=path.read_text();path.write_text(text+'{"unfinished":')
        with patch.object(wgc.Image,'open',side_effect=AssertionError('old PNG should not be decoded again')):
            self.assertEqual(wgc.live_frames(directory,ready,71,rows),rows)
        path.write_text(text.replace('1100000','1150000'))
        with self.assertRaisesRegex(AssertionError,'原始行改变'):wgc.live_frames(directory,ready,71,rows)
        path.write_text(text.replace('"pid": 71','"pid": 72'))
        with self.assertRaisesRegex(AssertionError,'owner'):wgc.live_frames(directory,ready,71)

    def test_merge_keeps_nonmatches_deduplicates_real_sources_and_rejects_conflicts(self):
        _,_,primary=self.session('primary',[(1,1_000_000),(9,1_500_000),(1,3_500_000)])
        _,_,extra=self.session('fresh',[(1,3_500_000),(2,4_000_000)])
        merged=wgc.merge_frames(primary,[{'session':'fresh','frames':extra}])
        self.assertEqual([r['wgc_timestamp_100ns'] for r in merged],[1_000_000,1_500_000,3_500_000,4_000_000])
        self.assertEqual(self.match(merged[1]),None);self.assertEqual(len(merged[2]['raw_sources']),2)
        self.assertEqual(sum(len(r['raw_sources']) for r in merged),len(primary)+len(extra))
        wrong=copy.deepcopy(extra);wrong[0]['crop_x']=1
        with self.assertRaisesRegex(AssertionError,'几何'):wgc.merge_frames(primary,[{'session':'fresh','frames':wrong}])
        _,_,conflict=self.session('conflict',[(2,3_500_000)])
        with self.assertRaisesRegex(AssertionError,'像素冲突'):wgc.merge_frames(primary,[{'session':'bad','frames':conflict}])
        Image.new('RGBA',(320,200),(3,0,0,255)).save(primary[0]['path'])
        with self.assertRaisesRegex(AssertionError,'原件身份'):wgc.merge_frames(primary,[])

    def test_union_still_uses_unchanged_two_frame_200ms_and_nonmatch_gate(self):
        for broken in (False,True):
            name='broken' if broken else 'good'
            items=[(1,1_000_000)]+([(9,1_500_000)] if broken else [])+[(2,5_000_000),(2,7_500_000),
                (3,9_000_000),(3,11_500_000),(4,13_000_000),(4,13_300_000)]
            _,_,primary=self.session(name,items);_,_,extra=self.session(name+'-extra',[(1,3_500_000)])
            merged=wgc.merge_frames(primary,[{'session':'fresh','frames':extra}])
            with patch.object(ticks.fixture,'pose_match',side_effect=lambda frame,poses,index:
                {'matched':True} if frame.getpixel((0,0))[0]==index else None):
                if broken:
                    with self.assertRaises(AssertionError):ticks.continuous_gate(merged,[])
                else:self.assertTrue(ticks.continuous_gate(merged,[])['ok'])

    def test_poll_requests_immediately_once_without_200ms_delay_or_fifth_corner(self):
        watcher=self.watcher();calls=[]
        with patch.object(watcher,'_capture',side_effect=lambda c:calls.append(c['pose'])):
            for qpc in (1_000_001,2_999_999,3_000_000,4_000_000):
                with patch.object(wgc,'qpc_stamp',return_value={'qpc':qpc,'qpc_frequency':10_000_000}):watcher.poll()
                self.assertEqual(calls,[1])
        self.assertEqual(calls,[1])
        self.session('action-series',[(1,1_000_000),(2,5_000_000),(3,9_000_000),(4,13_000_000)])
        with patch.object(watcher,'_capture',side_effect=lambda c:calls.append(c['pose'])),\
             patch.object(wgc,'qpc_stamp',return_value={'qpc':16_000_000,'qpc_frequency':10_000_000}):watcher.poll()
        self.assertEqual(calls,[1,4]);self.assertEqual(set(watcher.corners),{1,2,3,4})
        self.session('action-series',[(1,1_000_000),(2,5_000_000),(3,9_000_000),(4,13_000_000),(5,14_000_000)])
        with patch.object(watcher,'_capture') as capture,patch.object(wgc,'qpc_stamp',return_value=watcher.clock):
            watcher.poll();capture.assert_not_called()
        self.assertTrue(all(c['finished'] for c in watcher.corners.values()))

    def test_primary_independent_plateaus_record_proof_and_skip_all_unneeded_auxiliary_sessions(self):
        items=((1,1_000_000),(1,3_000_000),(2,5_000_000),(2,7_000_000),
            (3,9_000_000),(3,11_000_000),(4,13_000_000),(4,13_300_000))
        watcher=self.watcher(items)
        with patch.object(watcher,'_capture') as capture,patch.object(wgc,'qpc_stamp',return_value=watcher.clock):
            watcher.poll();capture.assert_not_called()
        for pose in (1,2,3,4):
            proof=watcher.corners[pose]
            self.assertTrue(proof['finished']);self.assertTrue(proof['covered_by_primary'])
            self.assertEqual(proof['primary_support']['frame_count'],2)
            self.assertEqual(proof['primary_support']['observed_stable_span_ms'],30 if pose==4 else 200)
            self.assertEqual([r['index'] for r in proof['primary_support']['frames']],[(pose-1)*2,(pose-1)*2+1])
            self.assertTrue(all(Path(r['path']).is_file() and len(r['sha256'])==64 and len(r['raw_row_sha256'])==64
                for r in proof['primary_support']['frames']))
        primary=wgc.live_frames(watcher.directory,watcher.ready,71)
        self.assertEqual(len(watcher.merge(primary)),len(primary));self.assertEqual(watcher.report['sessions'],[])

    def test_primary_coverage_never_joins_none_other_pose_short_or_single_frame(self):
        for items in (((1,1_000_000),(9,2_000_000),(1,3_500_000)),
                      ((1,1_000_000),(0,2_000_000),(1,3_500_000)),
                      ((1,1_000_000),(1,2_999_999)),((1,1_000_000),)):
            with self.subTest(items=items):
                watcher=self.watcher(items)
                with patch.object(watcher,'_capture') as capture,patch.object(wgc,'qpc_stamp',return_value=watcher.clock):
                    watcher.poll();capture.assert_called_once()
                self.assertNotIn('covered_by_primary',watcher.corners[1])
                self.assertNotIn('primary_support',watcher.corners[1])

    def test_single_fourth_primary_frame_requests_auxiliary_but_two_distinct_frames_are_covered(self):
        prefix=((1,1_000_000),(1,3_000_000),(2,5_000_000),(2,7_000_000),(3,9_000_000),(3,11_000_000))
        for tail,covered in ((((4,13_000_000),),False),(((4,13_000_000),(4,13_000_001)),True)):
            with self.subTest(tail=tail):
                watcher=self.watcher(prefix+tail)
                with patch.object(watcher,'_capture') as capture,patch.object(wgc,'qpc_stamp',return_value=watcher.clock):
                    watcher.poll()
                    if covered:capture.assert_not_called()
                    else:self.assertEqual(capture.call_args.args[0]['pose'],4)
                self.assertEqual(watcher.corners[4].get('covered_by_primary',False),covered)
                self.assertEqual(watcher.report['maximum_secondary_sessions'],4)

    def helper_case(self,*,stamp=3_100_000,pose=1,hwnd='0x29',crop=0,reason='stop',finish_error=False,
                    request_qpc=3_000_000,later=(),corner_pose=1):
        watcher=self.watcher(((corner_pose,1_000_000),));name=f'fresh-corner-{corner_pose}';corner={'pose':corner_pose,'first_stamp':1_000_000,'first_row':watcher.seen[0] if watcher.seen else
            wgc.live_frames(watcher.directory,watcher.ready,71)[0]}
        outer=self;instances=[]
        class Helper:
            def __init__(self,driver,args,work,label,record):
                self.args,self.record=args,record;instances.append(self)
            def start(self):
                directory,ready,_=outer.session(name,[(pose,stamp)])
                ready['hwnd']=hwnd;(directory/'ready.json').write_text(json.dumps(ready))
                path=directory/'frames.jsonl';rows=[json.loads(r) for r in path.read_text().splitlines()]
                for row in rows:row['hwnd']=hwnd;row['crop_x']=crop
                path.write_text(''.join(json.dumps(r)+'\n' for r in rows))
                summary={'ok':True,'backend':'WGC-HWND-session','stop_reason':reason,'pid':71,'hwnd':hwnd,
                    'session_started':True,'ready_published':True,'hresult':0,'color_filter':False,'qpc_frequency':10_000_000,
                    'frames':1,'first_wgc_timestamp_100ns':stamp,'last_wgc_timestamp_100ns':stamp}
                (directory/'summary.json').write_text(json.dumps(summary));self.summary=summary
                return SimpleNamespace(poll=lambda:None)
            def finish(self):
                self.stopped=(outer.root/name/'stop').is_file()
                directory=outer.root/name;rows=[json.loads(r) for r in (directory/'frames.jsonl').read_text().splitlines()]
                self.summary.update(frames=len(rows),last_wgc_timestamp_100ns=rows[-1]['wgc_timestamp_100ns'])
                (directory/'summary.json').write_text(json.dumps(self.summary))
                self.record.update(summary=self.summary,cleaned=True,forced_cleanup=False)
                if finish_error:raise RuntimeError('owned cleanup失败')
        clock=[0];watcher.deadline=12
        def wait(interval):
            clock[0]+=0.1
            if later and clock[0]<0.2:outer.session(name,[(pose,stamp),*later])
            return False
        watcher.stop=SimpleNamespace(wait=wait)
        with patch.object(wgc,'OwnedHelper',Helper),patch.object(wgc,'qpc_stamp',return_value={'qpc':request_qpc,'qpc_frequency':10_000_000}),\
             patch.object(wgc.time,'monotonic',side_effect=lambda:clock[0]):
            try:watcher._capture(corner);error=None
            except Exception as caught:error=caught
        self.assertTrue(instances[0].stopped);self.assertEqual(instances[0].args[-2:],[1500,16])
        return watcher,error

    def test_fresh_one_frame_session_has_complete_owned_and_raw_provenance(self):
        watcher,error=self.helper_case();self.assertIsNone(error)
        session=watcher.report['sessions'][0]
        self.assertEqual(len(session['frames']),1);self.assertEqual(session['request_qpc']['qpc'],3_000_000)
        self.assertEqual(set(session['artifacts']),{'ready.json','summary.json','frames.jsonl'})
        self.assertTrue(all(len(v['sha256'])==64 for v in session['artifacts'].values()))

    def test_early_new_frame_waits_in_same_session_until_real_200ms_age(self):
        watcher,error=self.helper_case(stamp=2_000_000,request_qpc=1_100_000,later=((1,3_100_000),))
        self.assertIsNone(error);self.assertEqual(len(watcher.report['sessions']),1)
        session=watcher.report['sessions'][0]
        self.assertEqual(len(session['frames']),2);self.assertEqual(session['accepted_frame']['index'],1)
        self.assertEqual(session['accepted_frame']['source_age_ms'],210)

    def test_fourth_fresh_frame_needs_new_stamp_and_request_bound_without_200ms(self):
        for stamp,requested,success in ((1_100_001,1_100_000,True),(1_000_000,1_000_000,False),
                                       (1_000_001,1_100_000,False)):
            with self.subTest(stamp=stamp,requested=requested),tempfile.TemporaryDirectory() as directory:
                old=self.root;self.root=Path(directory);self.capture.artifacts=self.root
                try:
                    watcher,error=self.helper_case(corner_pose=4,pose=4,stamp=stamp,request_qpc=requested)
                    if success:
                        self.assertIsNone(error);session=watcher.report['sessions'][0]
                        self.assertEqual(session['minimum_observed_span_ms'],0)
                        self.assertGreater(session['accepted_frame']['source_age_ms'],0)
                        self.assertLess(session['accepted_frame']['source_age_ms'],200)
                    else:self.assertIsInstance(error,Exception)
                finally:self.root=old;self.capture.artifacts=old

    def test_early_frames_alone_timeout_and_intervening_nonmatches_remain_raw(self):
        watcher,error=self.helper_case(stamp=2_000_000,request_qpc=1_100_000)
        self.assertIsInstance(error,TimeoutError);self.assertNotIn('accepted_frame',watcher.report['sessions'][0])
        with tempfile.TemporaryDirectory() as directory:
            old=self.root;self.root=Path(directory);self.capture.artifacts=self.root
            try:
                watcher,error=self.helper_case(stamp=2_000_000,request_qpc=1_100_000,later=((9,2_500_000),(1,3_100_000)))
                self.assertIsNone(error)
                self.assertEqual([self.match(r) for r in watcher.report['sessions'][0]['frames']],[1,None,1])
            finally:self.root=old;self.capture.artifacts=old

    def test_old_wrong_pose_owner_geometry_timeout_and_cleanup_never_pass(self):
        for kwargs in ({'stamp':2_999_999},{'pose':2},{'hwnd':'0x30'},{'crop':1},{'reason':'time_limit'},{'finish_error':True}):
            with self.subTest(kwargs=kwargs),tempfile.TemporaryDirectory() as directory:
                old=self.root;self.root=Path(directory);self.capture.artifacts=self.root
                try:watcher,error=self.helper_case(**kwargs);self.assertIsInstance(error,Exception)
                finally:self.root=old;self.capture.artifacts=old

    def test_fresh_merge_rejects_unfinished_observation_and_changed_primary(self):
        watcher=self.watcher();primary=wgc.live_frames(watcher.directory,watcher.ready,71)
        with self.assertRaisesRegex(AssertionError,'未完成'):watcher.merge(primary)
        watcher.corners={i:{'finished':True} for i in (1,2,3,4)};watcher.seen=copy.deepcopy(primary)
        primary[0]['sha256']='changed'
        with self.assertRaisesRegex(AssertionError,'完整索引不同'):watcher.merge(primary)

    def test_request_budget_fifth_corner_and_thread_errors_cannot_become_success(self):
        watcher=self.watcher();corner={'pose':1,'first_stamp':1_000_000,'first_row':{}}
        watcher.deadline=0
        with patch.object(wgc,'OwnedHelper') as helper:
            helper.return_value.finish.return_value=None
            with self.assertRaisesRegex(TimeoutError,'未启动'):watcher._capture(corner)
            helper.return_value.start.assert_not_called()
        watcher=self.watcher()
        with self.assertRaisesRegex(AssertionError,'四角'):watcher._capture({'pose':5})
        watcher.report['sessions']=[{'pose':1}]
        with self.assertRaisesRegex(AssertionError,'四角'):watcher._capture(corner)
        watcher.deadline=0
        with patch.object(watcher,'poll',side_effect=AssertionError('must not poll')) as poll:
            watcher._run();poll.assert_not_called()
        self.assertIsInstance(watcher.error,TimeoutError)
        with self.assertRaises(TimeoutError):watcher.merge([])

    def test_merge_rechecks_raw_rows_after_watcher_exit_and_requires_complete_index(self):
        watcher=self.watcher(((1,1_000_000),(2,5_000_000)))
        primary=wgc.live_frames(watcher.directory,watcher.ready,71)
        watcher.seen=copy.deepcopy(primary);watcher.corners={i:{'finished':True} for i in (1,2,3,4)}
        path=watcher.directory/'frames.jsonl';original=path.read_text()
        self.assertEqual(len(watcher.merge(primary)),2)
        rows=[json.loads(line) for line in original.splitlines()]
        changed=copy.deepcopy(rows);changed[0]['source_unused_field']=1
        changed2=copy.deepcopy(rows);changed2[0]['crop_x']=1
        for text in (original.rstrip('\n'),original.splitlines(keepends=True)[0],
            ''.join(json.dumps(row)+'\n' for row in changed),''.join(json.dumps(row)+'\n' for row in changed2)):
            with self.subTest(raw_index=text):
                path.write_text(text)
                with self.assertRaises(AssertionError):watcher.merge(primary)
        path.write_text(original)
        with self.assertRaisesRegex(AssertionError,'计数'):watcher.merge(primary[:-1])


if __name__=='__main__':unittest.main()
