"""F1两尺寸域及非shipping三槽资源alias纯测；不启动窗口。"""
from contextlib import contextmanager
import ast
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import Mock,patch

from PIL import Image

TOOLS=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(TOOLS))
sys.path.insert(0,str(Path(__file__).resolve().parent))
import fd2_g5_f1_oracle as oracle
import run_g5_f1_check as runner
from run_original_f1 import scaled_base,marker_frames,FullFrameOracle
import test_stage2_storage as storage_test

ROOT=TOOLS.parent


class G5F1Test(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prepared=oracle.prepare(ROOT/'original_game','tall-alias7',ROOT/'original_game/START.SAV')

    def test_formal_Load_consumes_exact_exit_phases_before_F1_without_extra_input(self):
        for y,wrong_turn in ((92,False),(97,False),(104,False),(169,False),(93,False),(92,True)):
            with self.subTest(y=y,wrong_turn=wrong_turn),tempfile.TemporaryDirectory() as directory:
                work=Path(directory);save=work/'FD2.SAV';save.write_bytes(self.prepared['encoded'])
                capture=Mock(artifacts=work);record={'checkpoints':[]};seen=[];stopped=[]
                turn=self.prepared['source'][oracle.META]
                actual=runner.load_round_template(ROOT/'original_game',self.prepared['assets']['palette'],
                    turn+1 if wrong_turn else turn,exiting=True)
                image=Image.new('RGB',(320,200));image.paste(actual.convert('RGB'),(120,y))
                @contextmanager
                def series(*args):
                    try:yield {},[]
                    finally:stopped.append(True)
                def point(capture,scope,name,predicate,points,**kwargs):
                    seen.append(name)
                    if name!='continued-actual-round':raise TimeoutError('fixture stops after real Round predicate')
                    found=predicate(image)
                    if not found:raise TimeoutError('full source Round rejected')
                    self.assertEqual(found['phase'],'exit')
                    self.assertEqual(found['digit_relative_y'],6)
                    self.assertEqual(found['position'],[120,y])
                    self.assertEqual(found['turn'],turn)
                    return image,found
                with patch.object(runner,'modal_series',series),patch.object(runner,'series_checkpoint',side_effect=point), \
                        patch.object(runner,'wait_title'),patch.object(runner,'driver_call') as close:
                    with self.assertRaises(TimeoutError):runner.exercise(capture,ROOT/'original_game',self.prepared,save,record)
                self.assertEqual(seen,['continued-actual-round'] if wrong_turn or y==93 else
                    ['continued-actual-round','continued-actual-browse'])
                self.assertEqual([c.args[0] for c in capture.key.call_args_list],['UP','ENTER'])
                self.assertEqual(stopped,[True]);close.assert_not_called()
                self.assertEqual(save.read_bytes(),self.prepared['encoded'])

    def test_alias_exact_three_entries_and_other_payloads_unchanged(self):
        prepared=self.prepared;manifest=prepared['manifest'];alias=manifest['view_alias']
        self.assertEqual(manifest['map'],[29,45]);self.assertEqual(manifest['shape_frames'],384)
        self.assertEqual(manifest['scale'],3);self.assertEqual(manifest['markers']['marker_size'],[2,2])
        self.assertEqual(len(alias['slots']),3)
        for name,mapping in (('FDFIELD.DAT',{0:21}),('FDSHAP.DAT',{0:14,1:15})):
            before=oracle.dat_entries((ROOT/'original_game'/name).read_bytes())
            actual=oracle.dat_entries(prepared['aliases'][name])
            self.assertEqual(len(actual),len(before))
            for index,row in enumerate(actual):self.assertEqual(row,before[mapping.get(index,index)])
        self.assertFalse(alias['EXE_metadata_SAV_units_RNG_modified'])
        self.assertFalse(alias['shipping_capability_changed'])
        self.assertEqual(prepared['encoded'],(ROOT/'original_game/START.SAV').read_bytes())

    def test_alias_directory_mutation_missing_EOF_and_empty_entries_rejected(self):
        good=oracle.repack_dat([b'one',b'two'])
        self.assertEqual(oracle.dat_entries(good),[b'one',b'two'])
        with self.assertRaises(ValueError):oracle.dat_entries(good[:-1])
        with self.assertRaises(ValueError):oracle.repack_dat([b'one',b''])

    def test_height_boundary_changes_true_sampling_and_marker_size(self):
        palette=[channel for index in range(256) for channel in (index,index,index)]
        tile=bytes((row*24+col)%256 for row in range(24) for col in range(24))
        p=bytearray(self.prepared['source']);p[oracle.META+1]=1
        p[oracle.UNITS:oracle.UNITS+2]=bytes((4,5));p[oracle.UNITS+5]=0;p[oracle.UNITS+6]=2
        for height,scale,step in ((40,4,768),(41,3,1024)):
            width=29;image=scaled_base(width,height,[0]*(width*height),{0:tile},palette)
            # 完全独立整帧采样，以Python floor/divmod核负坐标及24px余数。
            want=bytearray(64000)
            for y in range(192):
                ty,iy=divmod(height*1536-96*step+y*step,3072)
                for x in range(312):
                    tx,ix=divmod(width*1536-156*step+x*step,3072)
                    if 0<=tx<width and 0<=ty<height:want[(y+4)*320+x+4]=tile[(iy//128)*24+ix//128]
            self.assertEqual(image.tobytes(),bytes(want))
            frames,markers=marker_frames(image,p,(4,5),(32,80,72),width,height)
            self.assertEqual(markers['marker_size'],[scale-1]*2)
            sx=4*scale+160-width*scale//2;sy=5*scale+100-height*scale//2
            expected=image.copy();expected.paste(26,(sx,sy,sx+scale-1,sy+scale-1))
            self.assertEqual(frames[2].tobytes(),expected.tobytes())
        with self.assertRaises(ValueError):scaled_base(65,40,[0]*2600,{0:tile},palette)

    def test_tall_whole_frame_oracle_rejects_wrong_shape_marker_and_black(self):
        frames=self.prepared['frames'];gate=FullFrameOracle(frames)
        for phase in (0,4,7):self.assertEqual(gate.match(frames[phase])['pulse_phase'],phase)
        for position in ((0,0),tuple(self.prepared['manifest']['markers']['focus_screen']),(160,100)):
            wrong=frames[4].convert('RGB');color=wrong.getpixel(position)
            wrong.putpixel(position,tuple(255-channel for channel in color))
            self.assertIsNone(gate.match(wrong))

    def test_current_working_directory_resolves_actual_alias_objects(self):
        with tempfile.TemporaryDirectory() as directory:
            work=Path(directory)
            cwd,resources,receipt=runner.setup_resources(ROOT/'original_game',work,self.prepared['aliases'])
            self.assertEqual((cwd/'original_game').resolve(),resources.resolve())
            self.assertEqual(receipt['actual_process_cwd'],str(cwd.resolve()))
            source=self.prepared['manifest']['source_objects']
            self.assertEqual(receipt['actual_map0_sha256'],source['map_sha256'])
            self.assertEqual(receipt['actual_shape0_sheet_sha256'],source['shape_sheet_sha256'])
            self.assertEqual(receipt['actual_shape0_attr_sha256'],source['shape_attr_sha256'])
            self.assertEqual((resources/'FD2.EXE').read_bytes(),(ROOT/'original_game/FD2.EXE').read_bytes())

    def test_return_tile_zero_is_not_transparency(self):
        template=Image.new('P',(24,24),0);template.putpalette([v for i in range(256) for v in (i,i,i)])
        frame=Image.new('RGB',(320,200),(0,0,0))
        self.assertEqual(runner.exact_rect(frame,template,(28,28))['checked_pixels'],576)
        frame.putpixel((28,28),(255,255,255))
        self.assertFalse(runner.exact_rect(frame,template,(28,28))['ok'])

    def test_origin_report_does_not_accept_model_or_unowned_save(self):
        with tempfile.TemporaryDirectory() as directory:
            folder=Path(directory);save=folder/'fixture.SAV';save.write_bytes(self.prepared['encoded'])
            report=folder/'comparison.json';report.write_text(json.dumps({'kind':'sdl','status':'passed'}))
            with self.assertRaisesRegex(ValueError,'原版'):oracle.validate_origin(save,report,1)
            report.write_text(json.dumps({'kind':'original','status':'passed','original_unchanged':True,
                'fixture_unchanged':True,'processes':[{'exit_code':0,'cleaned':True,'forced_cleanup':False}],
                'completed_save':{'snapshot':str(save),'sha256':'0'*64}}))
            with self.assertRaisesRegex(ValueError,'绑定'):oracle.validate_origin(save,report,1)

    def test_native_sequence_requires_round_then_full_page_and_no_recovery_keys(self):
        @contextmanager
        def series(owner,record):
            scopes.append(('ready',owner.artifacts.name,[c.args[0] for c in capture.key.call_args_list]))
            try:yield {}
            finally:scopes.append(('stop',owner.artifacts.name,[c.args[0] for c in capture.key.call_args_list]))
        for fail in (False,True):
            with self.subTest(fail=fail),tempfile.TemporaryDirectory() as directory:
                folder=Path(directory);save=folder/'FD2.SAV';save.write_bytes(self.prepared['encoded'])
                capture=Mock(artifacts=folder);capture.process.pid=17
                capture.process.wait.return_value=0
                record={'checkpoints':[]};order=[];scopes=[]
                def point(capture,scope,name,predicate,records,**kwargs):
                    order.append(name)
                    if name=='F1-full-first' and fail:raise TimeoutError('missing full64000 source page')
                    if '-full-' in name:
                        phase=0 if name.endswith('first') else 1
                        image=self.prepared['frames'][phase].copy()
                        image.putpalette(self.prepared['assets']['f1_palette_cycles'][14]['palette'])
                        found=predicate(image)
                        self.assertIsNotNone(found)
                        return image,found
                    return Image.new('RGB',(320,200)),{}
                with patch.object(runner,'detail_series',series),patch.object(runner,'series_checkpoint',side_effect=point), \
                        patch.object(runner,'wait_title'),patch.object(runner,'driver_call') as close:
                    if fail:
                        with self.assertRaisesRegex(TimeoutError,'64000'):
                            runner.exercise(capture,ROOT/'original_game',self.prepared,save,record)
                        self.assertEqual([c.args[0] for c in capture.key.call_args_list],['UP','ENTER','F1'])
                        close.assert_not_called()
                        self.assertEqual(scopes[-1],('stop','F1',['UP','ENTER','F1']))
                    else:
                        runner.exercise(capture,ROOT/'original_game',self.prepared,save,record)
                        self.assertEqual([c.args[0] for c in capture.key.call_args_list],['UP','ENTER','F1','A','PAGEUP','ESC'])
                        self.assertEqual(order[:2],['continued-actual-round','continued-actual-browse'])
                        self.assertEqual(order[-1],'PageUp-read-one-return-page')
                        self.assertTrue(record['whole_22987B_Save_and_mtime_unchanged'])
                        self.assertEqual(scopes,[
                            ('ready','load',['UP']),('stop','load',['UP','ENTER']),
                            ('ready','F1',['UP','ENTER']),('stop','F1',['UP','ENTER','F1','A']),
                            ('ready','PageUp',['UP','ENTER','F1','A']),
                            ('stop','PageUp',['UP','ENTER','F1','A','PAGEUP','ESC'])])
                        close.assert_called_once()
                self.assertEqual(save.read_bytes(),self.prepared['encoded'])

    def b7_model_source(self,folder):
        """严格来源结构的纯模型；positive不是本次原版运行或真实WGC证明。"""
        model=storage_test.Stage2StorageTest()
        model.data=oracle.stage2_storage.o.tables(ROOT/'original_game')
        save,report,plain,state,record=model.model_source(folder)
        record['model_only']=False  # 用合成结构测试admission；下方反例显式拒model标签。
        record['guards']={key:True for key in ('source_fixture_and_report_unchanged',
            'original_bytes_size_mtime_unchanged','tools_and_binaries_unchanged','save_bytes_mtime_expected',
            'owned_cleanup','business_complete','snapshot_files_unchanged','isolated_static_resources_unchanged')}
        record['processes'][0]['pid']=42
        for point in record['checkpoints']:point['pid']=42
        manual=Path(folder)/'manual0.SAV';manual.write_bytes(save.read_bytes())
        record['manual_save']={'slot':0,'snapshot':str(manual),'sha256':oracle.sha(manual.read_bytes())}
        report.write_text(json.dumps(record),encoding='utf-8')
        return save,report,record

    def test_stage2_B7_preflight_reuses_complete_projection_and_seals_all_origin_files(self):
        with tempfile.TemporaryDirectory() as folder:
            save,report,record=self.b7_model_source(folder)
            source=oracle.validate_origin(save,report,2,original=ROOT/'original_game')
            self.assertEqual(source['schema'],'fd2-f1-b7-stage2-origin-v1')
            self.assertEqual(source['stage2_entry_ACKs_checked_against_original_pages'],15)
            self.assertEqual(len(source['source_files']),18)
            self.assertEqual(source['actual_encoded_Save_sha256'],oracle.sha(save.read_bytes()))
            self.assertTrue(source['entry_source_measurement']['full_roster_slots_unused_active_meta_tails_checked'])
            self.assertTrue(oracle.stage2_sources_unchanged(source))
            self.assertEqual(source['strict_B7_guards'],list(record['guards']))
            prepared=oracle.prepare(ROOT/'original_game','stage2',save,report)
            self.assertEqual(prepared['encoded'],save.read_bytes())
            self.assertEqual(prepared['manifest']['runtime_stage'],2)
            self.assertIsNone(prepared['manifest']['view_alias'])
            self.assertEqual(prepared['aliases'],{})
            self.assertFalse(prepared['manifest']['source_report'].get('stage2_battle_claimed',False))

    def test_stage2_missing_each_B7_guard_model_SDL_or_unowned_source_never_falls_back(self):
        with tempfile.TemporaryDirectory() as folder:
            save,report,record=self.b7_model_source(folder)
            for bad in [*record['guards'],'model','sdl','business','pid0','pid_negative','pid_text',
                    'two_processes','exit','cleaned','forced','top_flags_only']:
                changed=copy.deepcopy(record)
                if bad in changed['guards']:changed['guards'].pop(bad)
                elif bad=='model':changed['model_only']=True
                elif bad=='sdl':changed['kind']='sdl'
                elif bad=='business':changed['business_sequence_completed']=False
                elif bad.startswith('pid'):changed['processes'][0]['pid']={'pid0':0,'pid_negative':-1,'pid_text':'42'}[bad]
                elif bad=='two_processes':changed['processes'].append(copy.deepcopy(changed['processes'][0]))
                elif bad=='exit':changed['processes'][0]['exit_code']=1
                elif bad=='cleaned':changed['processes'][0]['cleaned']=False
                elif bad=='forced':changed['processes'][0]['forced_cleanup']=True
                else:changed['guards']={};changed.update(original_unchanged=True,fixture_unchanged=True)
                report.write_text(json.dumps(changed),encoding='utf-8')
                with self.subTest(bad=bad),patch.object(oracle.stage2_storage,'prepare') as storage:
                    with self.assertRaises(ValueError):oracle.validate_origin(save,report,2,original=ROOT/'original_game')
                    storage.assert_not_called()

    def test_stage2_actual_Save_ACK_token_owner_PNG_and_manual0_are_required(self):
        for bad in ('save_sha','other_snapshot','checksum','ack','token','self_consistent_wrong_token',
                'self_consistent_order','text','border','point_pid','png','manual_sha','manual_slot','manual_missing'):
            with self.subTest(bad=bad),tempfile.TemporaryDirectory() as folder:
                save,report,record=self.b7_model_source(folder);point=record['checkpoints'][0]
                if bad=='save_sha':record['completed_save']['sha256']='0'*64
                elif bad=='other_snapshot':
                    record['unrelated']={'snapshot':str(save),'sha256':oracle.sha(save.read_bytes())}
                    record['completed_save']['snapshot']=str(Path(folder)/'unowned.SAV')
                elif bad=='checksum':
                    data=bytearray(save.read_bytes());data[0]^=1;save.write_bytes(data)
                    record['completed_save']['sha256']=oracle.sha(data)
                elif bad=='ack':record['stage2_entry_acknowledgements'].pop()
                elif bad=='token':point['token_offset']+=1
                elif bad=='self_consistent_wrong_token':
                    point['token_offset']+=1;record['stage2_entry_acknowledgements'][0]['offset']+=1
                elif bad=='self_consistent_order':
                    a,b=record['stage2_entry_acknowledgements'][:2]
                    record['stage2_entry_acknowledgements'][:2]=[b,a]
                    for current,ack in zip(record['checkpoints'][:2],[b,a]):
                        current.update(fragment=ack['fragment'],token_offset=ack['offset'],portrait_id=ack['portrait'])
                elif bad in ('text','border'):point[bad]['ok']=False
                elif bad=='point_pid':point['pid']=43
                elif bad=='png':(Path(folder)/'b7-entry-00.png').write_bytes(b'changed after source')
                elif bad=='manual_sha':record['manual_save']['sha256']='0'*64
                elif bad=='manual_slot':
                    path=Path(record['manual_save']['snapshot']);encoded=path.read_bytes();old=oracle.decode_save(path)
                    plain=bytearray(old);plain[oracle.stage2_storage.SLOT_BASE]^=1
                    plain=oracle.stage2_storage.o.checksum(plain)
                    path.write_bytes(bytes(a^b^c for a,b,c in zip(encoded,old,plain)))
                    record['manual_save']['sha256']=oracle.sha(path.read_bytes())
                else:Path(record['manual_save']['snapshot']).unlink()
                report.write_text(json.dumps(record),encoding='utf-8')
                with self.assertRaises((ValueError,AssertionError,FileNotFoundError)):
                    oracle.validate_origin(save,report,2,original=ROOT/'original_game')

    def test_stage2_every_source_file_is_late_guarded_for_bytes_and_mtime(self):
        with tempfile.TemporaryDirectory() as folder:
            save,report,record=self.b7_model_source(folder)
            origin=oracle.validate_origin(save,report,2,original=ROOT/'original_game')
            self.assertTrue(oracle.stage2_sources_unchanged(origin))
            for fact in origin['source_files']:
                path=Path(fact['path']);raw=path.read_bytes();stat=path.stat()
                path.write_bytes(raw+b'changed')
                self.assertFalse(oracle.stage2_sources_unchanged(origin))
                path.write_bytes(raw)
                import os
                os.utime(path,ns=(stat.st_atime_ns,stat.st_mtime_ns))
                self.assertTrue(oracle.stage2_sources_unchanged(origin))
            self.assertTrue(oracle.stage2_sources_unchanged(None))
            self.assertTrue(oracle.stage2_sources_unchanged({'path':'old stage1'}))

    def test_stage2_source_files_enter_the_existing_freeze_without_starting_any_window(self):
        with tempfile.TemporaryDirectory() as folder:
            folder=Path(folder);save,report,record=self.b7_model_source(folder)
            origin=oracle.validate_origin(save,report,2,original=ROOT/'original_game')
            binary=folder/'fake.exe';binary.write_bytes(b'not executed')
            prepared={'manifest':{'source_report':origin},'encoded':save.read_bytes(),
                'aliases':{},'frames':self.prepared['frames'][:1]}
            args=type('Args',(),dict(root=ROOT,artifacts=folder/'output',save_fixture=save,source_report=report,
                scenario='stage2',run=True,kind='pair',sdl=binary,driver=binary,dosbox=binary))()
            captured=[]
            def freeze(root,original,binaries):
                captured.extend(Path(p).resolve() for p in binaries)
                return {str(p.resolve()):runner.sha(p) for p in binaries},{},[]
            @contextmanager
            def no_display(record):
                try:raise RuntimeError('pure stops before display/owned game')
                finally:record['released']=True
                yield
            with patch.object(runner.model,'prepare',return_value=prepared),patch.object(runner,'owned_job_required'),\
                    patch.object(runner,'freeze',side_effect=freeze),patch.object(runner,'unchanged',return_value=True),\
                    patch.object(runner,'windows_display_requirement',no_display),patch.object(runner,'running') as game:
                actual=runner.run(args)
            self.assertFalse(actual['ok']);game.assert_not_called()
            for fact in origin['source_files']:
                self.assertIn(Path(fact['path']).resolve(),captured)
                self.assertEqual(actual['frozen_inputs'][fact['path']],fact['sha256'])

    def test_legacy_stage1_tall_oracles_and_all_input_drawing_execution_AST_stay_identical(self):
        digest=lambda node:hashlib.sha256(ast.dump(node,include_attributes=False).encode()).hexdigest()
        functions={node.name:node for node in ast.parse(Path(oracle.__file__).read_text(encoding='utf-8')).body
            if isinstance(node,ast.FunctionDef)}
        self.assertEqual(digest(functions['view_alias']),'b2fc741b603a1c3bebb1286e9eda409ab81c8e77eff1b557386ad27c15eac82c')
        self.assertEqual(digest(functions['return_tiles']),'1dfc4404ba35221b0af00ddbbdf476ef167017361cfe589e0a65a1d91fc448b4')
        legacy=copy.deepcopy(functions['validate_origin'])
        self.assertEqual([arg.arg for arg in legacy.args.kwonlyargs],['original'])
        self.assertIsNone(legacy.args.kw_defaults[0].value)
        self.assertEqual(ast.dump(legacy.body[0]),ast.dump(ast.parse(
            'if stage==2:return validate_stage2_origin(original,fixture,report_path)').body[0]))
        legacy.body.pop(0);legacy.args.kwonlyargs=[];legacy.args.kw_defaults=[]
        self.assertEqual(digest(legacy),'6f9979c8707c6a951edd8327c1af2dd917c376db52adbe490ed473c42aa2f48c')
        test=self
        class SourceOnly(ast.NodeTransformer):
            count=0
            def visit_IfExp(self,node):
                if ast.unparse(node.test)=='resource_stage == 2':
                    test.assertEqual(ast.dump(node),ast.dump(ast.parse(
                        'validate_origin(fixture,report_path,resource_stage,original=original) if resource_stage==2 else validate_origin(fixture,report_path,resource_stage)',mode='eval').body))
                    self.count+=1;return node.orelse
                return self.generic_visit(node)
        projection=SourceOnly();prepared=projection.visit(copy.deepcopy(functions['prepare']))
        self.assertEqual(projection.count,1)
        self.assertEqual(digest(prepared),'b4e458c8a2776e81d4c0b098e0148459288e7e9133bfa178bd92e014117f719a')
        runtime={node.name:node for node in ast.parse(Path(runner.__file__).read_text(encoding='utf-8')).body
            if isinstance(node,ast.FunctionDef)}
        expected={'exact_rect':'5c55e71e5f11606501041a91acb685cb4b6014426d292ff8cae7056fd56cf822',
            'browse_state':'7baa99af79e133e6cdaa4f996d63c542de799858587e7560bdecd0bfdcd523c2',
            'setup_resources':'26c8522c577e3105f0dda0c698d9294a8506a559cdaf85475559ac94fc6e1735',
            'modal_series':'5600abff3236b26a78e5167b5eba2bed614675f096430d34a9d51c820ee8eb66',
            'exercise':'7429b2908b432b58dbdbef361f886b0336ac33816f0898f081a709bb2a298996'}
        for name,sha in expected.items():self.assertEqual(digest(runtime[name]),sha,name)


if __name__=='__main__':unittest.main()
