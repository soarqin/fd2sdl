"""教会16c57观察纯回归：复用已armed series，不启动窗口或改变业务输入。"""
from pathlib import Path
import json
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import run_g3_prepare_check as runner
import fd2_g3_prepare_pages as pages


class ReadySeriesTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.assets=pages.load(ROOT/'original_game')

    def fixture(self,folder,fragment=591,phases=(0,3,0),bad=None):
        work=Path(folder);directory=work/'animation-series';directory.mkdir()
        expected=pages.notice(self.assets,fragment,selector=4)
        exact=dict(expected,portrait_frames=[p for p in expected['portrait_frames'] if p[2]==3])
        rows=[]
        for index,phase in enumerate(phases):
            canvas=expected['text'].copy()
            image,xy,_=next(row for row in expected['portrait_frames'] if row[2]==phase)
            canvas.paste(image,xy);canvas=canvas.convert('RGB')
            if bad and phase==3:
                if bad=='text':
                    at=next((x,y) for y in range(119,198) for x in range(320)
                        if expected['text'].getpixel((x,y)))
                else:at=xy
                rgb=canvas.getpixel(at);canvas.putpixel(at,tuple(255-c for c in rgb))
            path=directory/f'frame{index:05d}.png';canvas.save(path)
            rows.append({'index':index,'file':path.name,'pid':42,'hwnd':'0x123',
                'wgc_timestamp_100ns':100000+330000*index,'client_width':320,'client_height':200})
        (directory/'frames.jsonl').write_text(''.join(json.dumps(row)+'\n' for row in rows),encoding='utf-8')
        ready={'ready':True,'pid':42,'hwnd':'0x123','first_index':0,'wgc_timestamp_100ns':100000}
        process=SimpleNamespace(pid=42,poll=lambda:None);helper=SimpleNamespace(process=SimpleNamespace(poll=lambda:None))
        record={'kind':'original','checkpoints':[]};keys=[]
        capture=SimpleNamespace(process=process,artifacts=work,last=None,g3_case_record=record,key=keys.append,trace=[])
        collector=SimpleNamespace(started=True,ready=ready,series=directory,helpers=[helper])
        event={'pid':42,'key':'ENTER','time':.0025,'delivery':self.delivery()}
        capture.trace.append(event)
        baseline={**rows[0],'path':str(directory/'frame00000.png'),'sha256':runner.sha(directory/'frame00000.png')}
        name=('revive' if fragment==588 else 'promotion')+'-no-candidates'
        record['ready_wait_boundary']={'opens_step':name,'wait_name':name+'-real-wait',
            'baseline':baseline,'trace_index':0,'trace_length_after':1,'input':json.loads(json.dumps(event)),
            'read_completed_qpc':self.stamp(1),'key_call_qpc_bracket':{'before':self.stamp(2),'after':self.stamp(3)}}
        return capture,collector,record,rows,lambda frame:pages.notice_match(frame,exact),keys

    @staticmethod
    def stamp(value):return {'qpc':value,'qpc_frequency':1000,'host_monotonic':value/1000}

    @staticmethod
    def delivery():return {'pid':42,'hwnd':'0x123','backend':'PostMessageW HWND','kind':'key',
        'ok':True,'error':0,'keys':1,'sent':1,'release':1,'key_down_sent':True,'key_up_sent':True}

    def close(self,capture,collector,record,rows):
        summary={'ok':True,'backend':'WGC-HWND-session','pid':42,'hwnd':'0x123',
            'ready_published':True,'session_started':True,'stop_reason':'stop','hresult':0,
            'color_filter':False,'frames':len(rows),'first_wgc_timestamp_100ns':rows[0]['wgc_timestamp_100ns'],
            'last_wgc_timestamp_100ns':rows[-1]['wgc_timestamp_100ns']}
        record['animation_capture']={'ready':collector.ready,'summary':summary,
            'frames':runner.series_index(collector.series,summary,collector.ready,42)}
        record['collection_completed']=True

    def test_short_blink_in_backlog_is_not_lost_when_latest_is_normal(self):
        for fragment in (588,591):
            with self.subTest(fragment=fragment),tempfile.TemporaryDirectory() as folder:
                capture,collector,record,rows,predicate,keys=self.fixture(folder,fragment)
                name=('revive' if fragment==588 else 'promotion')+'-no-candidates-real-wait'
                runner.wait_ready_series(capture,runner.ready_wait_scope(collector),name,predicate,record['checkpoints'],timeout=1)
                point=record['checkpoints'][0]
                self.assertEqual(point['series_frame']['index'],1)
                self.assertEqual(point['portrait_match'][0]['phase'],3)
                self.assertEqual(point['portrait_match'][0]['checked_pixels'],6400)
                self.assertTrue(point['no_additional_WGC_session']);self.assertEqual(keys,[])
                self.close(capture,collector,record,rows);runner.bind_ready_series_points(capture,record)
                self.assertTrue(record['ready_wait_closed_series_bound'])

    def test_phase0_only_wrong_text_or_one_portrait_pixel_do_not_pass(self):
        for bad in ('only0','text','portrait'):
            with self.subTest(bad=bad),tempfile.TemporaryDirectory() as folder:
                capture,collector,record,rows,predicate,keys=self.fixture(folder,
                    phases=(0,0,0) if bad=='only0' else (0,3,0),bad=None if bad=='only0' else bad)
                with self.assertRaises(TimeoutError):
                    runner.wait_ready_series(capture,runner.ready_wait_scope(collector),'promotion-no-candidates-real-wait',
                        predicate,record['checkpoints'],timeout=.03)
                self.assertEqual(record['first_failure']['checkpoint'],'promotion-no-candidates-real-wait')
                self.assertEqual(record['checkpoints'],[]);self.assertEqual(keys,[])

    def test_missing_unready_wrong_owner_or_exited_collector_cannot_reuse_backlog(self):
        for bad in ('missing','unready','pid','hwnd','helper_exit','target_exit'):
            with self.subTest(bad=bad),tempfile.TemporaryDirectory() as folder:
                capture,collector,record,rows,predicate,keys=self.fixture(folder)
                if bad=='missing':collector=None
                elif bad=='unready':collector.started=False
                elif bad=='pid':collector.ready['pid']=43
                elif bad=='hwnd':collector.ready['hwnd']=''
                elif bad=='helper_exit':collector.helpers[0].process.poll=lambda:0
                else:capture.process.poll=lambda:0
                with patch.object(runner,'series_checkpoint') as read:
                    with self.assertRaises((AssertionError,RuntimeError)):
                        runner.wait_ready_series(capture,runner.ready_wait_scope(collector),'promotion-no-candidates-real-wait',predicate,[],timeout=1)
                    read.assert_not_called()
                self.assertEqual(keys,[])

    def test_changed_owner_order_or_missing_png_is_rejected(self):
        for field,value in (('pid',43),('hwnd','0x456'),('index',2),('wgc_timestamp_100ns',90000),('missing',None)):
            with self.subTest(field=field),tempfile.TemporaryDirectory() as folder:
                capture,collector,record,rows,predicate,keys=self.fixture(folder)
                if field=='missing':(collector.series/'frame00001.png').unlink()
                else:
                    rows[1][field]=value
                    (collector.series/'frames.jsonl').write_text(''.join(json.dumps(row)+'\n' for row in rows),encoding='utf-8')
                with self.assertRaises((AssertionError,FileNotFoundError)):
                    runner.wait_ready_series(capture,runner.ready_wait_scope(collector),'promotion-no-candidates-real-wait',predicate,[],timeout=1)
                self.assertEqual(keys,[])

    def test_helper_ending_after_a_match_still_refuses_ack(self):
        with tempfile.TemporaryDirectory() as folder:
            capture,collector,record,rows,predicate,keys=self.fixture(folder)
            # 模拟检查点已发布后退出，保持帧处理过程中任意次数的活性检查。
            collector.helpers[0].process.poll=lambda:0 if record['checkpoints'] else None
            with self.assertRaisesRegex(RuntimeError,'提前退出'):
                runner.wait_ready_series(capture,runner.ready_wait_scope(collector),'promotion-no-candidates-real-wait',predicate,record['checkpoints'],timeout=1)
            self.assertEqual(len(record['checkpoints']),1)
            self.assertEqual(keys,[]);self.assertIn('first_failure',record)

    def test_final_closed_series_cannot_lose_or_change_successful_witness(self):
        for bad in ('not_closed','missing_point','wrong_sha','wrong_owner','wrong_stamp','raw_changed','normalized_changed',
                'boundary_sha','boundary_row','not_after','trace_replaced'):
            with self.subTest(bad=bad),tempfile.TemporaryDirectory() as folder:
                capture,collector,record,rows,predicate,keys=self.fixture(folder)
                runner.wait_ready_series(capture,runner.ready_wait_scope(collector),'promotion-no-candidates-real-wait',predicate,record['checkpoints'],timeout=1)
                self.close(capture,collector,record,rows);point=record['checkpoints'][0]
                if bad=='not_closed':record['collection_completed']=False
                elif bad=='missing_point':point.pop('series_frame')
                elif bad=='wrong_sha':point['series_frame']['sha256']='0'*64
                elif bad=='wrong_owner':point['series_frame']['hwnd']='0x456'
                elif bad=='wrong_stamp':point['series_frame']['wgc_timestamp_100ns']+=1
                elif bad=='raw_changed':Path(point['series_frame']['path']).write_bytes(b'changed')
                elif bad=='normalized_changed':Path(point['normalized_PNG']['path']).write_bytes(b'changed')
                elif bad=='boundary_sha':record['ready_wait_boundary']['baseline']['sha256']='0'*64
                elif bad=='boundary_row':record['ready_wait_boundary']['baseline']['index']=99
                elif bad=='not_after':record['ready_wait_boundary']['baseline']=dict(point['series_frame'])
                else:capture.trace[0]['delivery']['hwnd']='0x456'
                with self.assertRaises(AssertionError):runner.bind_ready_series_points(capture,record)
                self.assertNotIn('ready_wait_closed_series_bound',record)

    def test_missing_or_wrong_prekey_input_QPC_and_owner_are_rejected(self):
        for bad in ('missing','trace_index','input','key','owner','read_after_key','frequency','missing_qpc',
                'baseline_owner','old_key','extra_delivery'):
            with self.subTest(bad=bad),tempfile.TemporaryDirectory() as folder:
                capture,collector,record,rows,predicate,keys=self.fixture(folder)
                boundary=record['ready_wait_boundary']
                if bad=='missing':record.pop('ready_wait_boundary')
                elif bad=='trace_index':boundary['trace_index']=1
                elif bad=='input':boundary['input']['delivery']['sent']=2
                elif bad=='key':capture.trace[0]['key']='SPACE';boundary['input']=json.loads(json.dumps(capture.trace[0]))
                elif bad=='owner':capture.trace[0]['delivery']['pid']=43;boundary['input']=json.loads(json.dumps(capture.trace[0]))
                elif bad=='read_after_key':boundary['read_completed_qpc']['qpc']=2
                elif bad=='frequency':boundary['read_completed_qpc']['qpc_frequency']=1001
                elif bad=='missing_qpc':boundary['read_completed_qpc']['qpc']=None
                elif bad=='baseline_owner':boundary['baseline']['hwnd']='0x456'
                elif bad=='old_key':
                    capture.trace[0]['time']=.0005;boundary['input']=json.loads(json.dumps(capture.trace[0]))
                else:boundary['trace_length_after']=2
                with self.assertRaises(AssertionError):
                    runner.wait_ready_series(capture,runner.ready_wait_scope(collector),'promotion-no-candidates-real-wait',predicate,[],timeout=1)
                self.assertEqual(keys,[])

    def test_preboundary_blink_cannot_be_reused_but_earliest_postboundary_can(self):
        for phases in ((3,0,0),(3,0,3)):
            with self.subTest(phases=phases),tempfile.TemporaryDirectory() as folder:
                capture,collector,record,rows,predicate,keys=self.fixture(folder,phases=phases)
                if phases[-1]==0:
                    with self.assertRaises(TimeoutError):
                        runner.wait_ready_series(capture,runner.ready_wait_scope(collector),'promotion-no-candidates-real-wait',predicate,[],timeout=.03)
                else:
                    runner.wait_ready_series(capture,runner.ready_wait_scope(collector),'promotion-no-candidates-real-wait',predicate,record['checkpoints'],timeout=1)
                    self.assertEqual(record['checkpoints'][0]['series_frame']['index'],2)
                self.assertEqual(keys,[])

    def test_boundary_mark_is_before_any_input_and_missing_row_or_QPC_stops(self):
        for bad in (None,'missing_row','missing_qpc','exited'):
            with self.subTest(bad=bad),tempfile.TemporaryDirectory() as folder:
                capture,collector,record,rows,predicate,keys=self.fixture(folder)
                record.pop('ready_wait_boundary');capture.trace=[]
                if bad=='missing_row':(collector.series/'frames.jsonl').write_bytes(b'')
                elif bad=='exited':collector.helpers[0].process.poll=lambda:0
                stamp=self.stamp(10) if bad!='missing_qpc' else {'qpc':None,'qpc_frequency':None}
                with patch.object(runner,'qpc_stamp',return_value=stamp):
                    if bad:
                        with self.assertRaises((AssertionError,RuntimeError)):
                            runner.begin_ready_wait_boundary(capture,runner.ready_wait_scope(collector),'promotion-no-candidates',record)
                        self.assertIn('first_failure',record)
                    else:
                        boundary=runner.begin_ready_wait_boundary(capture,runner.ready_wait_scope(collector),'promotion-no-candidates',record)
                        self.assertEqual(boundary['baseline']['index'],2)
                        self.assertEqual(boundary['trace_index'],0)
                        self.assertEqual(boundary['read_completed_qpc'],stamp)
                self.assertEqual(keys,[]);self.assertEqual(capture.trace,[])

    def run_exercise(self,folder,*,timeout=False,stop_error=False,first=False):
        capture,collector,record,rows,predicate,keys=self.fixture(folder,fragment=588 if first else 591,
            phases=(0,0,0) if timeout else (0,3,0))
        events=[];test=self;record.pop('ready_wait_boundary');capture.trace=[];clock={'current':0}
        (collector.series/'frames.jsonl').write_text(json.dumps(rows[0])+'\n',encoding='utf-8')
        def key(key):
            keys.append(key);events.append('key:'+key)
            capture.trace.append({'pid':42,'key':key,'time':clock['current']/1000,'delivery':test.delivery()})
            if len(keys)==(1 if first else 2):
                # 最早blink在打开键返回前已发布，gate仍须消费，不能重取latest作baseline。
                (collector.series/'frames.jsonl').write_text(''.join(json.dumps(row)+'\n' for row in rows),encoding='utf-8')
        capture.key=key
        class Collector:
            def __init__(self,*args):self.__dict__.update(collector.__dict__)
            def start(self):events.append('start')
            def confirm(self):
                events.append('confirm');before=runner.qpc_stamp();capture.key('ENTER')
                record['animation_action']={'key_call_qpc_bracket':{'before':before,'after':runner.qpc_stamp()}}
            def stop(self):
                events.append('stop')
                if stop_error:raise RuntimeError('explicit cleanup failure')
                test.close(capture,self,record,rows)
        opens=('revive' if first else 'promotion')+'-no-candidates';wait=opens+'-real-wait'
        prepared={'titles':{3:{0:object()}},'steps':[{'collector':'start'}]+
            ([] if first else [{'name':'confirmed','key':'ENTER'}])+[
            {'name':opens,'key':'ENTER'},
            {'name':wait,'key':None,'page':'notice_real_wait'},
            {'name':'service-returned','key':'ENTER'},{'collector':'stop'},{'save':0}],
            'checks':{'confirmed':(object(),{}),opens:(object(),{}),
                wait:(predicate,{'timeout':.03 if timeout else 1}),
                'service-returned':(object(),{})}}
        def ordinary(capture,name,*args,**kwargs):events.append(name)
        ticks=iter(range(10,100))
        def stamp():clock['current']=next(ticks);return test.stamp(clock['current'])
        with patch.object(runner,'wait_title'),patch.object(runner,'require_source_unchanged'),\
                patch.object(runner,'AnimationCollectors',Collector),\
                patch.object(runner,'resource_wait_state',side_effect=ordinary),\
                patch.object(runner,'qpc_stamp',side_effect=stamp),\
                patch.object(runner,'save_measurement',side_effect=lambda *args:events.append('save')):
            failure=None
            try:runner.exercise(capture,prepared,None,[],record,object())
            except BaseException as error:failure=error
        return record,keys,events,failure

    def test_real_exercise_only_observer_changes_and_menu_precedes_stop_then_save(self):
        with tempfile.TemporaryDirectory() as folder:
            record,keys,events,failure=self.run_exercise(folder)
            self.assertIsNone(failure);self.assertEqual(keys,['ENTER','ENTER','ENTER'])
            self.assertEqual(events,['start','confirm','key:ENTER','confirmed','key:ENTER','promotion-no-candidates',
                'key:ENTER','service-returned','stop','save'])
            self.assertTrue(record['ready_wait_closed_series_bound']);self.assertTrue(record['business_sequence_completed'])

    def test_real_exercise_timeout_never_sends_service_ack_and_keeps_first_error(self):
        for stop_error in (False,True):
            with self.subTest(stop_error=stop_error),tempfile.TemporaryDirectory() as folder:
                record,keys,events,failure=self.run_exercise(folder,timeout=True,stop_error=stop_error)
                self.assertIsNotNone(failure);self.assertEqual(keys,['ENTER','ENTER'])
                self.assertEqual(events,['start','confirm','key:ENTER','confirmed','key:ENTER','promotion-no-candidates','stop'])
                report={'cases':[record]};runner.record_failure(report,failure)
                self.assertEqual(report['error']['type'],'TimeoutError')
                self.assertEqual(report['first_failure']['checkpoint'],'promotion-no-candidates-real-wait')
                self.assertNotIn('business_sequence_completed',record)
                if stop_error:self.assertEqual(len(report['secondary_errors']),1)

    def test_first_collected_revive_key_uses_existing_confirm_QPC_and_early_blink(self):
        with tempfile.TemporaryDirectory() as folder:
            record,keys,events,failure=self.run_exercise(folder,first=True)
            self.assertIsNone(failure);self.assertEqual(keys,['ENTER','ENTER'])
            boundary=record['ready_wait_boundary']
            self.assertEqual(boundary['opens_step'],'revive-no-candidates')
            self.assertEqual(boundary['trace_index'],0)
            self.assertLess(boundary['read_completed_qpc']['qpc'],boundary['key_call_qpc_bracket']['before']['qpc'])
            self.assertEqual(record['checkpoints'][0]['series_frame']['index'],1)
            self.assertTrue(record['ready_wait_closed_series_bound'])


if __name__=='__main__':unittest.main()
