"""G3 runner/source ROI 纯回归；不启动原版、SDL、WGC或构建。"""
from pathlib import Path
import ast
import copy
import hashlib
import json
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import run_g3_prepare_check as runner
import fd2_g3_prepare_pages as pages
import fd2_g3_prepare_oracle as oracle


class PrepareCheckTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prepared={name:runner.prepare(ROOT,name) for name in oracle.SCENARIOS}
        cls.p=cls.prepared['p1-buy-equip-yes'];cls.assets=cls.p['assets'];cls.t=cls.p['tables']

    def test_all_scenarios_separate_native_route_from_unmeasured_models(self):
        self.assertEqual(len(self.prepared),13)
        for name,p in self.prepared.items():
            with self.subTest(name=name):
                native=name in runner.RUNTIME_SCENARIOS
                self.assertEqual(p['runtime_protocol']['ready'],native)
                names=[s['name'] for s in p['steps'] if 'name' in s]
                self.assertEqual(len(names),len(set(names)))
                self.assertEqual(set(names),set(p['checks']))
                if native:
                    self.assertEqual([s['save'] for s in p['steps'] if 'save' in s],[0])
                    self.assertEqual(names[-1],'town-final')
                    self.assertTrue(all(s.get('name') or s.get('collector') or 'save' in s for s in p['steps']))
                    self.assertFalse(p['runtime_protocol'].get('audio_identity_passed',False))
                else:
                    self.assertEqual(p['steps'],[])
                    self.assertIn('转储',p['runtime_protocol']['reason'])
                self.assertEqual(oracle.crypt(p['encoded']),p['plain'])

    def test_purchase_commits_after_recipient_and_optional_equip_choice(self):
        yes=[s.get('name') or s.get('collector') or 'save' for s in self.p['steps']]
        self.assertLess(yes.index('buy-confirm'),yes.index('buy-recipients'))
        self.assertLess(yes.index('buy-recipients'),yes.index('buy-equip-confirm'))
        self.assertLess(yes.index('buy-equip-confirm'),yes.index('start'))
        no=self.prepared['p1-buy-equip-no']
        choice=next(s for s in no['steps'] if s.get('name')=='buy-equip-no')
        self.assertEqual((choice['key'],choice['selected']),('RIGHT',1))
        for name in ('p1-buy-insufficient','p1-buy-full','p3-revive-insufficient'):
            self.assertFalse(any(s.get('collector') for s in self.prepared[name]['steps']))

    def test_original_typed_tables_and_special_portrait_origins(self):
        # 原52387逐byte，不把其内容当DWORD或shop selector当bank。
        self.assertEqual(self.assets['service_portraits'],bytes((129,128,0,130,131,132)))
        buy=pages.notice(self.assets,502,dynamic=309,value=50)
        church=pages.notice(self.assets,590,selector=4,dynamic=5,value=100)
        self.assertEqual(buy['portrait'],128);self.assertEqual(buy['portrait_frames'][0][1],(123,13))
        self.assertEqual(church['portrait'],131);self.assertEqual(church['portrait_frames'][0][1],(118,4))
        normal=pages.notice(self.assets,490,value=13,xy=(95,138),portrait=36)
        self.assertEqual(normal['portrait_frames'][0][1],(8,115))
        with self.assertRaises(ValueError):pages.notice(self.assets,502,value=50)
        with self.assertRaises(ValueError):pages.notice(self.assets,502,dynamic=309)

    def test_product_complete_page_and_negative_price_selection_source(self):
        assets=self.assets;ids=[128,129,132,165]
        image=pages.items(assets,self.t,ids,0)
        canvas=Image.new('P',(320,200),0);canvas.putpalette(assets['palette']);canvas.paste(image,(5,112));frame=canvas.convert('RGB')
        self.assertTrue(pages.page_match(frame,image,kind='items',selected=0,source=['0x272d0']))
        wrong=pages.items(assets,self.t,ids,1)
        self.assertIsNone(pages.page_match(frame,wrong,kind='items',selected=1,source=['0x272d0']))
        sell=pages.items(assets,self.t,ids,0,selling=True)
        self.assertNotEqual(image.tobytes(),sell.tobytes())
        # 任一原完整ROI像素改动必须拒绝，不能靠“看起来像列表”的低色阈值。
        broken=frame.copy();broken.putpixel((5,112),(255,255,255))
        self.assertIsNone(pages.page_match(broken,image,kind='items',selected=0,source=['0x272d0']))
        self.assertIsNone(pages.page_match(Image.new('RGB',(320,200)),image,kind='items',selected=0,source=['0x272d0']))

    def test_preview_different_from_combat_recompute_and_all_sprites_covered(self):
        state=self.p['before'];unit=oracle.record(state,0);values=pages.preview_values(unit,128,self.t)
        armor=oracle.item(self.t,128);weapon=oracle.item(self.t,0)
        expected=[struct.unpack_from('<h',unit,55)[0]+struct.unpack_from('<h',armor,1)[0]+struct.unpack_from('<h',weapon,1)[0],
            struct.unpack_from('<h',unit,57)[0]+struct.unpack_from('<h',armor,5)[0]+struct.unpack_from('<h',weapon,5)[0],
            struct.unpack_from('<h',unit,62)[0]+struct.unpack_from('<h',armor,3)[0]+struct.unpack_from('<h',weapon,3)[0],
            struct.unpack_from('<h',unit,62)[0]+struct.unpack_from('<h',armor,7)[0]+struct.unpack_from('<h',weapon,7)[0]]
        self.assertEqual(values,expected)
        before=copy.deepcopy(state)
        for kind,kwargs in (('members',{}),('equipment',{'raws':[0,1,2,4],'item_id':128})):
            for phase,image in enumerate(pages.roster(self.assets,self.t,state,0,kind=kind,**kwargs)):
                self.assertEqual(image.size,(310,86))
                self.assertIn(73,image.getdata())
                sprite=pages._sprite(self.assets,state,0,phase//2 if kind=='equipment' else phase)
                self.assertEqual(sprite.size,(24,24))
        self.assertEqual(state,before)

    def test_revival_six_digit_fee_keeps_source_five_character_draw_bound(self):
        from fd2_magic_list_oracle import digit
        state=copy.deepcopy(self.p['before']);unit=bytearray(oracle.record(state,1))
        unit[32]=21;unit[33]=40;state['roster']=state['roster'][:80]+bytes(unit)+state['roster'][160:]
        self.assertEqual(oracle.word(self.t['fees'],42)*40,120000)
        actual=pages.roster(self.assets,self.t,state,0,kind='revive',raws=[1],selector=4)[0]
        expected=pages._base(self.assets,4)
        for index,character in enumerate('12000'):
            image=digit(self.assets['info'],119+int(character))
            expected.paste(image,(228+6*index,125),image.point(lambda v:255 if v else 0,mode='L'))
        # 六位区域同时核末列未绘，不只比较恰巧相同的前五位。
        self.assertEqual(actual.crop((223,13,259,21)).tobytes(),expected.crop((228,125,264,133)).tobytes())

    def test_scroll_arrows_are_full_original_frames_not_masked_out(self):
        variants=pages.roster(self.assets,self.t,self.p['before'],0,kind='equipment',raws=[0,1,2,4],item_id=128)
        self.assertEqual(len(variants),6)
        self.assertNotEqual(variants[0].tobytes(),variants[1].tobytes())
        canvas=Image.new('P',(320,200),0);canvas.putpalette(self.assets['palette'])
        canvas.paste(variants[3],(5,112));frame=canvas.convert('RGB')
        hit=pages.page_match(frame,variants,kind='prepare_equipment',selected=0,source=['0x2825b','0x27079'])
        self.assertEqual((hit['sprite_phase'],hit['arrow_phase'],hit['checked_pixels']),(1,1,26660))
        blank=pages._image(pages.member(self.assets['services'][1]['sheet'],17),self.assets['palette'],lmi=True)
        canvas.paste(blank,(298,181))
        self.assertIsNone(pages.page_match(canvas.convert('RGB'),variants,kind='prepare_equipment',selected=0,source=['0x27079']))
        # 7项出售页同源27079 mode1门是6行；两种arrow保留全部点。
        ids=[128,129,132,165,192,193,194]
        first=pages.items(self.assets,self.t,ids,0,selling=True,arrow_phase=0)
        second=pages.items(self.assets,self.t,ids,0,selling=True,arrow_phase=1)
        self.assertNotEqual(first.tobytes(),second.tobytes())

    def test_promotion_every_ack_declared_from_original_nonzero_growth(self):
        for name in ('p4-promote-normal','p4-promote-special90'):
            p=self.prepared[name];raw=p['manifest']['parameters']['raw'];new=oracle.record(p['after'],raw)[7]
            growth=self.t['growth'][11*new:11*(new+1)]
            expected=[490+i for i in range(5) if growth[2*i+1]]
            expected.append(596)
            steps=[s for s in p['steps'] if s.get('name','').startswith('promotion-growth-')]
            self.assertEqual([s['fragment'] for s in steps],expected)
            self.assertEqual([s['origin'][1] for s in steps],[138,157]+[157]*(len(steps)-3)+[176])
            self.assertTrue(all(s['portrait']==new and s['key']=='ENTER' for s in steps))
            self.assertEqual(p['before']['roster'][80*raw+8],p['after']['roster'][80*raw+8])

    def test_source_guard_prepare_only_no_windows_and_full_manifest(self):
        with tempfile.TemporaryDirectory(prefix='g3-pure-') as tmp:
            output=Path(tmp)/'result'
            args=SimpleNamespace(root=ROOT,artifacts=output,scenario='p1-buy-equip-yes',prepare_only=True,
                sdl=None,driver=None,dosbox=None)
            with patch.object(runner,'owned_job_required',side_effect=AssertionError('不得启动owned Job')),\
                 patch.object(runner,'running',side_effect=AssertionError('不得启动窗口')),\
                 patch.object(runner,'windows_display_requirement',side_effect=AssertionError('不得申请显示')):
                report=runner.run(args)
            self.assertFalse(report['ok']);self.assertEqual(report['status'],'ready_not_executed')
            self.assertTrue(report['final_sources_unchanged']);self.assertFalse(report['audio_identity_passed'])
            leaves={Path(p).name for p in report['inputs']}
            self.assertTrue({'run_g3_prepare_check.py','fd2_g3_prepare_pages.py','fd2_g3_prepare_oracle.py'}<=leaves)
            self.assertIn('FD2.EXE',report['original_before']);self.assertIn('DATO.DAT',report['original_before'])
            self.assertEqual(len((output/'controlled.SAV').read_bytes()),22987)
            self.assertEqual(len((output/'expected-after-slot0.SAV').read_bytes()),22987)
            self.assertEqual(json.loads((output/'comparison.json').read_text(encoding='utf-8'))['scenario'],args.scenario)

    def test_save_snapshot_binding_and_failed_comparison_keep_authorized_guard(self):
        p=self.p
        with tempfile.TemporaryDirectory(prefix='g3-save-pure-') as tmp:
            folder=Path(tmp);save=folder/'FD2.SAV';save.write_bytes(p['encoded']);guard=[save.read_bytes(),save.stat().st_mtime_ns]
            intended=oracle.write_slot(p['plain'],p['after'],0);bad=bytearray(intended);bad[0]^=1;bad=oracle.checksum(bad)
            capture=SimpleNamespace(artifacts=folder,key=lambda key:None);record={}
            def change(*args):save.write_bytes(oracle.crypt(bad));return bad
            with patch.object(runner,'wait_measured_save',side_effect=change):
                with self.assertRaisesRegex(AssertionError,'完整22987B'):runner.save_measurement(capture,p,save,guard,record,0)
            self.assertEqual(guard[0],save.read_bytes());self.assertFalse(record['state_comparison']['ok'])
            self.assertEqual(record['state_comparison']['mismatch_offsets'],[0,22983])
            self.assertEqual((folder/'measured.SAV').read_bytes(),save.read_bytes())

    def test_original_reuse_rejects_incomplete_original_even_if_top_metadata_valid(self):
        with tempfile.TemporaryDirectory(prefix='g3-reuse-pure-') as tmp:
            path=Path(tmp)/'comparison.json'
            args=SimpleNamespace(root=ROOT,scenario='p1-buy-equip-yes')
            for changes in ({'ok':False,'business_sequence_completed':False},
                {'ok':True,'business_sequence_completed':False},
                {'ok':True,'business_sequence_completed':True,'guards':{'cleanup':False}}):
                report={'scenario':args.scenario,'final_sources_unchanged':True,'original_before':{},
                    'display_requirement':{'released':True},'cases':[{'kind':'original',**changes}]}
                path.write_text(json.dumps(report),encoding='utf-8')
                with self.assertRaisesRegex(ValueError,'未完成或失败原版端'):
                    runner.inherit_original(path,self.p,args,{}, {})

    def test_original_reuse_adapter_cannot_change_protected_execution_ast(self):
        source=Path(runner.__file__).read_text(encoding='utf-8');module=ast.parse(source)
        protected=[node for node in module.body if isinstance(node,ast.FunctionDef) and node.name in
            ('prepare','exercise','save_measurement')]
        def digest(nodes):return hashlib.sha256('\n'.join(ast.dump(node,include_attributes=False) for node in nodes).encode()).hexdigest()
        self.assertEqual(len(protected),3)
        # 58只换真实wait的观察来源并增加stop封口；旧继承常量不改成新hash。
        self.assertNotEqual(digest(protected),runner.PRE_INHERIT_EXECUTION_AST)
        case=self
        class ObservationOnly(ast.NodeTransformer):
            waits=0;binds=0;boundary_assigns=0;boundary_blocks=0
            def visit_FunctionDef(self,node):
                if node.name=='exercise':
                    case.assertEqual([arg.arg for arg in node.args.kwonlyargs],['ready_wait_visual_only'])
                    case.assertEqual(ast.dump(node.args.kw_defaults[0]),ast.dump(ast.Constant(value=False)))
                    node.args.kwonlyargs=[];node.args.kw_defaults=[]
                return self.generic_visit(node)
            def visit_With(self,node):
                case.assertEqual(len(node.items),1)
                case.assertEqual(ast.dump(node.items[0]),ast.dump(ast.parse('with ExitStack() as visual_owner:\n pass').body[0].items[0]))
                case.assertEqual(len(node.body),1);case.assertIsInstance(node.body[0],ast.Try)
                return self.visit(node.body[0])
            def visit_If(self,node):
                if 'ready_wait_visual_only' in ast.unparse(node.test):
                    # 62只删除已逐字冻结的视觉入口/close元数据，不允许任意业务分支变化。
                    case.assertIn(hashlib.sha256(ast.dump(node,include_attributes=False).encode()).hexdigest(),{
                        'bd17a26c7ec878a5e0d77dfd72e8a386d6f88373674ee9fd1d4400724d31abfd',
                        '5911bd5ce82f00702782bb2d29bfe0de76ea8b6d96e509a189dbcf085fd01cc4',
                        'c18e8cebf5670c8db4d20ac38370f1aa38012fd5bed4bc315350aed5c7cac746'})
                    return None
                if ast.dump(node.test)==ast.dump(ast.parse("step.get('page')=='notice_real_wait'",mode='eval').body):
                    self.waits+=1
                    case.assertEqual(ast.dump(node.body[0]),ast.dump(ast.parse(
                        "wait_ready_series(capture,visual_scope if ready_wait_visual_only else ready_wait_scope(collectors),step['name'],predicate,record['checkpoints'],timeout=options['timeout'])").body[0]))
                    self.visit(node.body[1])
                    return node.orelse
                if ast.dump(node.test)==ast.dump(ast.parse('boundary is not None',mode='eval').body):
                    allowed=[
                        "boundary['key_call_qpc_bracket']=record['animation_action']['key_call_qpc_bracket']",
                        "boundary['key_call_qpc_bracket']={'before':key_before,'after':qpc_stamp()}",
                        "boundary['input']=json.loads(json.dumps(capture.trace[boundary['trace_index']]))\nboundary['trace_length_after']=len(capture.trace)"]
                    case.assertIn([ast.dump(item) for item in node.body],
                        [[ast.dump(item) for item in ast.parse(text).body] for text in allowed])
                    case.assertEqual(node.orelse,[])
                    self.boundary_blocks+=1;return None
                return self.generic_visit(node)
            def visit_Assign(self,node):
                if len(node.targets)==1 and isinstance(node.targets[0],ast.Name) and node.targets[0].id=='visual_scope':
                    case.assertEqual(ast.dump(node),ast.dump(ast.parse('visual_scope=None').body[0]));return None
                if len(node.targets)==1 and isinstance(node.targets[0],ast.Name) and node.targets[0].id in ('boundary','key_before'):
                    expected={
                        'boundary':"boundary=begin_ready_wait_boundary(capture,visual_scope if ready_wait_visual_only else ready_wait_scope(collectors),step['name'],record) if step.get('name') in ('revive-no-candidates','promotion-no-candidates') else None",
                        'key_before':"key_before=qpc_stamp() if boundary is not None else None"}
                    case.assertEqual(ast.dump(node),ast.dump(ast.parse(expected[node.targets[0].id]).body[0]))
                    self.boundary_assigns+=1;return None
                return self.generic_visit(node)
            def visit_Expr(self,node):
                if isinstance(node.value,ast.Call) and isinstance(node.value.func,ast.Name) and node.value.func.id=='bind_ready_series_points':
                    case.assertEqual(ast.dump(node),ast.dump(ast.parse('bind_ready_series_points(capture,record)').body[0]))
                    self.binds+=1;return None
                return self.generic_visit(node)
        projected=copy.deepcopy(protected);projection=ObservationOnly()
        projected=[projection.visit(node) for node in projected]
        self.assertEqual((projection.waits,projection.binds,projection.boundary_assigns,projection.boundary_blocks),(1,2,2,3))
        self.assertEqual(digest(projected),runner.PRE_INHERIT_EXECUTION_AST)
        changed=copy.deepcopy(projected);changed[0].body.append(ast.Return(value=ast.Constant(value=None)))
        self.assertNotEqual(digest(changed),runner.PRE_INHERIT_EXECUTION_AST)

    def test_original_reuse_final_seal_rejects_changed_or_missing_actual_file(self):
        with tempfile.TemporaryDirectory(prefix='g3-reuse-seal-') as tmp:
            path=Path(tmp)/'source.png';path.write_bytes(b'old-file-identity')
            receipt={'sealed_files':{str(path):runner.sha(path)}}
            self.assertTrue(runner.inherited_unchanged(receipt))
            path.write_bytes(b'changed-file-identity');self.assertFalse(runner.inherited_unchanged(receipt))
            path.unlink();self.assertFalse(runner.inherited_unchanged(receipt))

    def test_revive_wait_ready_requires_real_portrait3_without_extra_key(self):
        p=runner.prepare(ROOT,'p3-revive');old_keys=[s.get('key') for s in p['steps'] if s.get('key')]
        runner.finalize_ready_waits(p,'p3-revive');name='revive-no-candidates-real-wait'
        index=next(i for i,s in enumerate(p['steps']) if s.get('name')==name)
        self.assertIsNone(p['steps'][index]['key']);self.assertEqual(p['steps'][index+1]['name'],'service-returned')
        self.assertEqual(p['steps'][index+2],{'collector':'stop'})
        self.assertNotIn({'collector':'stop'},p['steps'][:index])
        self.assertEqual([s.get('key') for s in p['steps'] if s.get('key')],old_keys)
        expected=pages.notice(p['assets'],588,selector=4)
        canvas=expected['text'].copy();image,xy,_=expected['portrait_frames'][0];canvas.paste(image,xy)
        self.assertIsNone(p['checks'][name][0](canvas.convert('RGB')))
        image,xy,_=expected['portrait_frames'][1];canvas.paste(image,xy)
        self.assertTrue(p['checks'][name][0](canvas.convert('RGB')))
        with self.assertRaises(ValueError):runner.finalize_ready_waits(p,'p3-revive')
        # 既有P1/P2计划结构和按键完全不变。
        for scenario in ('p1-buy-equip-yes','p2-sell-equipped','p2-self-give','p3-revive-insufficient'):
            prepared=runner.prepare(ROOT,scenario);old=list(prepared['steps'])
            runner.finalize_ready_waits(prepared,scenario);self.assertEqual(prepared['steps'],old)

    def test_promotion_stop_is_after_verified586_not591_any_key_wait(self):
        for scenario in ('p4-promote-normal','p4-promote-special90'):
            p=runner.prepare(ROOT,scenario);keys=[s.get('key') for s in p['steps'] if s.get('key')]
            runner.finalize_ready_waits(p,scenario)
            wait=next(i for i,s in enumerate(p['steps']) if s.get('name')=='promotion-no-candidates-real-wait')
            self.assertEqual(p['steps'][wait-1]['fragment'],591)
            self.assertIsNone(p['steps'][wait]['key']);self.assertEqual(p['steps'][wait]['exact_portrait_phase'],3)
            self.assertEqual(p['steps'][wait+1]['name'],'service-returned')
            self.assertEqual(p['steps'][wait+2],{'collector':'stop'})
            self.assertNotIn({'collector':'stop'},p['steps'][:wait])
            self.assertEqual([s.get('key') for s in p['steps'] if s.get('key')],keys)
        malformed=runner.prepare(ROOT,'p3-revive')
        malformed['steps'].remove({'collector':'stop'})
        with self.assertRaisesRegex(ValueError,'stop位置不同'):runner.finalize_ready_waits(malformed,'p3-revive')

    def test_business_timeout_remains_primary_when_real_exercise_finally_stop_fails(self):
        case={'kind':'sdl','ok':False,'checkpoints':[]}
        capture=SimpleNamespace(g3_case_record=case,artifacts=Path('.'))
        prepared={'titles':{3:{0:object()}},'steps':[{'collector':'start'},{'name':'promotion-growth-596','key':None}],
            'checks':{'promotion-growth-596':(object(),{})}}
        class Collector:
            def __init__(self,*args):pass
            def start(self):pass
            def stop(self):raise AssertionError('600cap truncated; not complete')
        with patch.object(runner,'wait_title'),patch.object(runner,'require_source_unchanged'),\
            patch.object(runner,'AnimationCollectors',Collector),\
            patch.object(runner,'resource_wait_state',side_effect=TimeoutError('等待promotion-growth-596超时')):
            with self.assertRaises(AssertionError) as error:
                runner.exercise(capture,prepared,None,[],case,object())
        report={'cases':[case]};runner.record_failure(report,error.exception)
        self.assertEqual(report['error'],{'type':'TimeoutError','message':'等待promotion-growth-596超时'})
        self.assertEqual(report['first_failure']['checkpoint'],'promotion-growth-596')
        self.assertEqual(report['secondary_errors'][0]['error'],
            {'type':'AssertionError','message':'600cap truncated; not complete'})
        self.assertFalse(case['ok']);self.assertNotIn('collection_completed',case)

    def test_wait_wrapper_delegates_success_and_reraises_same_failure_without_retry(self):
        case={};capture=SimpleNamespace(g3_case_record=case);sentinel=object();predicate=object();checks=[]
        with patch.object(runner,'resource_wait_state',return_value=sentinel) as wait:
            self.assertIs(runner.wait_state(capture,'page',predicate,checks,timeout=7),sentinel)
            wait.assert_called_once_with(capture,'page',predicate,checks,timeout=7)
        self.assertEqual(case,{})
        first=TimeoutError('first');later=ValueError('later')
        for failure in (first,later):
            with patch.object(runner,'resource_wait_state',side_effect=failure) as wait:
                with self.assertRaises(type(failure)) as error:runner.wait_state(capture,'page',predicate,checks)
                self.assertIs(error.exception,failure);self.assertEqual(wait.call_count,1)
        self.assertEqual(case['first_failure']['error']['message'],'first')

    def test_nonbusiness_error_does_not_invent_a_timeout(self):
        report={'cases':[]};runner.record_failure(report,ValueError('resource invalid'))
        self.assertEqual(report['error'],{'type':'ValueError','message':'resource invalid'})
        self.assertNotIn('secondary_errors',report)


if __name__=='__main__':unittest.main()
