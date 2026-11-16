"""仅纯资源/协议替身；不开设备、窗口，不运行游戏。"""
from contextlib import contextmanager
from pathlib import Path
import importlib.util
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from PIL import Image

ROOT=next(p for p in Path(__file__).resolve().parents if (p/'AGENTS.md').is_file())
sys.path.insert(0,str(ROOT/'tools'));sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import fd2_f4_picker_retention as retention
_wrapper_path=Path(__file__).resolve().parents[1]/'run_f2_prepare_check.py'
_wrapper_spec=importlib.util.spec_from_file_location('picker_retention_wrapper_under_test',_wrapper_path)
wrapper=importlib.util.module_from_spec(_wrapper_spec);_wrapper_spec.loader.exec_module(wrapper)
import run_g3_prepare_check as base


def archive(blobs):
    offset=6+4*len(blobs);entries=[]
    for blob in blobs:entries.append(offset);offset+=len(blob)
    return b'LLLLLL'+struct.pack('<'+'I'*len(entries),*entries)+b''.join(blobs)


def lmi(width,height,value):return struct.pack('<HH',width,height)+bytes([value])*width*height


class PickerRetentionTest(unittest.TestCase):
    def prepared(self):
        frames=[lmi(80,80,v) for v in [5,6,8,7]];offset=16;offsets=[]
        for frame in frames:offsets.append(offset);offset+=len(frame)
        portraits=struct.pack('<4I',*offsets)+b''.join(frames)
        palette=bytes(i//3%64 for i in range(768))
        assets={'other':archive([palette]),'portraits':archive([b'x']*128+[portraits]),'info':b'unused-number-source',
            'services':{1:{'sheet':archive([b'x',lmi(63,15,3)]),'background':Image.new('P',(320,200),2)}}}
        return {'assets':assets,'tables':{},'before':{'stage':1,'count':5,'resource':0}}

    def models(self,selected):
        prepared=self.prepared()
        def number(canvas,sheet,value,x,y,bank,width):
            self.assertEqual((value,x,y,bank,width),(0,16,99,31,8));canvas.putpixel((x,y),9)
        def sprite(assets,state,raw,phase):
            image=Image.new('P',(24,24),13+phase)
            # phase1写254；其它相位在同点为stride孔，必须继承254。
            image.putpixel((0,18),254 if phase==1 else 0)
            return image
        def lower(assets,tables,state,focus):
            result=[]
            for phase in (0,1,2):
                image=Image.new('P',(310,86),10)
                for raw in range(5):
                    shape=sprite(assets,state,raw,phase)
                    image.paste(shape,(9+132*(raw%2),5+26*(raw//2)),shape.point(lambda p:255 if p else 0,mode='L'))
                image.putpixel((35,9),20+focus)  # selected文字窗，独立于sprite写点。
                result.append(image)
            return result
        with patch.object(retention.pages,'roster',side_effect=lower),\
                patch.object(retention.pages,'_sprite',side_effect=sprite),\
                patch.object(retention.pages,'number',side_effect=number):
            return retention.full_models(prepared,selected)

    def test_full_page_from_resources_and_one_pixel_outside_ROI_rejected(self):
        models=self.models(0)
        self.assertEqual(len(models),11)
        expected={(m['upper'],m['sprite_phase']):m for m in models}
        for upper,index in [('B',2),('P0',5),('P3',7)]:
            image=expected[upper,1]['image']
            expand=lambda value:((value<<2)|(value>>4),)*3
            self.assertEqual(image.getpixel((175,90)),expand(index))
            self.assertEqual(image.getpixel((5,95)),expand(3))
            self.assertEqual(image.getpixel((16,99)),expand(9))
            self.assertEqual(image.getpixel((5,112)),expand(10))
            self.assertEqual(image.getpixel((319,199)),expand(2))
            self.assertEqual(retention.classify(image,models)['upper'],upper)
        bad=expected['P0',1]['image'].copy();bad.putpixel((319,199),(9,8,8))
        self.assertIsNone(retention.classify(bad,models))

    def test_background_after_P0_and_same_sprite_do_not_pass(self):
        models=self.models(0);by={(m['upper'],m['sprite_phase']):m['image'] for m in models}
        gate=retention.phase_predicate(models,'P0',different_sprite=1,forbid_background=True)
        self.assertIsNone(gate(by['P0',1]));self.assertIsNone(gate(by['P3',2]))
        self.assertEqual(gate(by['P0',2])['checked_pixels'],64000)
        with self.assertRaisesRegex(AssertionError,'擦回'):gate(by['B',2])

    def test_stride_hole_preserves_source_writer_and_fresh_P0_cannot_pass(self):
        models=self.models(0)
        phase0=[m for m in models if m['upper']=='B' and m['sprite_phase']==0]
        self.assertEqual({m['history'] for m in phase0},{'fresh','stable'})
        by_history={m['history']:m['image'] for m in phase0}
        expand=lambda index:(((index%64)<<2)|((index%64)>>4),)*3
        self.assertEqual(by_history['fresh'].getpixel((14,135)),expand(10))
        self.assertEqual(by_history['stable'].getpixel((14,135)),expand(254))
        opened=next(m['image'] for m in models if m['upper']=='P0' and m['sprite_phase']==0)
        self.assertEqual(opened.getpixel((14,135)),expand(254))
        bad=by_history['fresh'].copy();bad.paste(opened.crop((123,13,203,93)),(123,13))
        self.assertIsNone(retention.classify(bad,models))
        self.assertIsNone(retention.phase_predicate(models,'P0')(bad))

    def test_original_only_collects_new_navigation_and_does_not_resign_cycle(self):
        record,keys,error=self.exercise(kind='original');self.assertIsNone(error)
        self.assertEqual([k['key'] for k in keys],['ENTER','DOWN'])
        self.assertEqual([p['name'] for p in record['checkpoints']],
            ['picker-original-P0-navigation-precondition','picker-selected2-P0','picker-selected2-after-eligible'])
        self.assertIn('not re-signed',record['portrait_cycle_scope'])
        self.assertTrue(record['picker_retention_completed']);self.assertFalse(record['save_executed'])
        self.assertTrue(all(p['exact_RGB'] and p['checked_pixels']==64000 for p in record['checkpoints']))

    def test_narrow_stops_before_members_detail_or_Save(self):
        prepared={'steps':[{'name':'title-slot','key':'ENTER'},{'name':'equipment-command2','key':'RIGHT'},
            {'name':'equipment-members','key':'ENTER'},{'save':0}],
            'source_rois':{'title-slot':[],'equipment-members':[]},'manifest':{}}
        result=retention.narrow(prepared)
        self.assertEqual([s['name'] for s in result['steps']],['title-slot','equipment-command2'])
        self.assertFalse(result['manifest']['native_hotel_save_after_action'])
        self.assertEqual(result['runtime_protocol']['additional_keys'],['ENTER','DOWN'])
        prepared['steps'].insert(1,{'name':'unexpected-save','save':0})
        with self.assertRaises(ValueError):retention.narrow(prepared)

    def test_invalid_feedback_flag_fails_before_paths_or_launch(self):
        with patch.object(wrapper,'owned_job_required'):
            with self.assertRaisesRegex(ValueError,'eqt-empty'):
                wrapper.run(SimpleNamespace(inventory_scenario=None,picker_retention_only=True))

    def exercise(self,*,fail_point=None,cleanup_error=False,wrong_hwnd=False,kind='sdl'):
        models={selected:self.models(selected) for selected in [0,2]}
        by={(m['selected'],m['upper'],m['sprite_phase']):m['image'] for group in models.values() for m in group}
        keys=[];record={'kind':kind,'checkpoints':[],'processes':[]}
        capture=SimpleNamespace(trace=keys,process=SimpleNamespace(pid=123))
        capture.key=lambda name:keys.append({'key':name,'delivery':{'pid':123,'hwnd':'0xother' if wrong_hwnd else '0xabc'}})
        @contextmanager
        def series(capture,record):
            try:yield {'ready':{'pid':123,'hwnd':'0xabc'}}
            finally:
                record['detail_series']={'checkpoint_PNGs_bound_to_closed_series':not cleanup_error}
                if cleanup_error:raise RuntimeError('late series close')
        stages=iter([(0,'P0',1),(2,'P0',0),(2,'P0',1)] if kind=='original' else
            [(0,'B',0),(0,'P3',1),(0,'P0',1),(0,'P0',2),(2,'P0',0),(2,'P0',1)])
        def point(capture,scope,name,predicate,points,timeout):
            if name==fail_point:raise TimeoutError('main phase timeout')
            image=by[next(stages)];detail=predicate(image);self.assertIsNotNone(detail)
            points.append({'name':name,**detail});return image,detail
        def prefix(capture,prepared,save,guard,record,args):record['business_sequence_completed']=True
        with patch.object(base,'exercise',side_effect=prefix),\
                patch.object(retention,'full_models',side_effect=lambda _,s:models[s]),\
                patch.object(retention,'detail_series',side_effect=series),\
                patch.object(retention,'series_checkpoint',side_effect=point),\
                patch.object(retention,'require_source_unchanged'):
            try:retention.exercise(capture,{},None,[],record,None)
            except BaseException as error:return record,keys,error
        return record,keys,None

    def test_protocol_is_two_keys_and_full_page_stage_sequence_no_Save(self):
        record,keys,error=self.exercise();self.assertIsNone(error)
        self.assertEqual([k['key'] for k in keys],['ENTER','DOWN'])
        self.assertTrue(record['picker_retention_completed']);self.assertFalse(record['save_executed'])
        self.assertNotIn('completed_save',record);self.assertEqual(len(record['checkpoints']),6)
        self.assertTrue(all(p['checked_pixels']==64000 and p['tolerance']==0 for p in record['checkpoints']))

    def test_main_phase_failure_survives_series_cleanup_diagnostic(self):
        record,_,error=self.exercise(fail_point='picker-open-P0',cleanup_error=True)
        self.assertIsInstance(error,RuntimeError)
        report={'cases':[record]};base.record_failure(report,error)
        self.assertEqual(report['error'],{'type':'TimeoutError','message':'main phase timeout'})
        self.assertEqual(report['first_failure']['checkpoint'],'picker-open-P0')
        self.assertIn('late series close',str(report['secondary_errors']))
        self.assertNotIn('picker_retention_completed',record)

    def test_wrong_input_HWND_fails_before_phase_gate(self):
        record,keys,error=self.exercise(wrong_hwnd=True)
        self.assertIsInstance(error,AssertionError)
        self.assertEqual([k['key'] for k in keys],['ENTER'])
        self.assertEqual(record['first_failure']['checkpoint'],'picker-key-ENTER')
        self.assertFalse(record['checkpoints']);self.assertNotIn('picker_retention_completed',record)

    def test_outer_report_write_failure_keeps_business_error(self):
        import run_g3_inventory_check as lane
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);original=root/'original_game';original.mkdir()
            (original/'START.SAV').write_bytes(b'read-only fixture')
            for name in ['game.exe','driver.exe','dosbox.exe','bootstrap.conf','reference.json']:
                (root/name).write_bytes(name.encode())
            args=SimpleNamespace(root=root,artifacts=root/'out',game=root/'game.exe',driver=root/'driver.exe',
                dosbox=root/'dosbox.exe',bootstrap=root/'bootstrap.conf',inventory_scenario='eqt-empty',
                picker_retention_only=True,wake_display=False)
            real_write=Path.write_text
            def write(path,*a,**k):
                if path==args.artifacts/'report.json':raise OSError('disk full')
                return real_write(path,*a,**k)
            with patch.object(wrapper,'owned_job_required'),patch.object(wrapper,'source_facts',return_value={}),\
                    patch.object(wrapper.original_reference,'REFERENCE',root/'reference.json'),\
                    patch.object(lane,'run',side_effect=TimeoutError('main phase timeout')),\
                    patch.object(Path,'write_text',write):
                with self.assertRaisesRegex(RuntimeError,'main phase timeout.*报告写入失败.*disk full'):
                    wrapper.run(args)


if __name__=='__main__':unittest.main()
