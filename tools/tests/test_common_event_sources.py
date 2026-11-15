"""G4事件原来源合同：ACK前后顺序、整数身份、未绑定和错误重放负例。"""
import importlib.util
import copy
import json
import shutil
from pathlib import Path
import sys
import unittest
import tempfile

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools/static_data'))
spec=importlib.util.spec_from_file_location('common_event_source',ROOT/'tools/static_data/verify_common_events.py')
verify=importlib.util.module_from_spec(spec);spec.loader.exec_module(verify)

class CommonEventSourcesTest(unittest.TestCase):
    def tables(self):
        return json.loads((ROOT/'data/static/tables/common_event.json').read_text(encoding='utf-8'))
    def flows(self):
        document=json.loads((ROOT/'data/static/flows/common_event.json').read_text(encoding='utf-8'))
        # 原21图语义回归只用原六绑定；S3新增绑定由下列独立receipt合同测试覆盖。
        document['bindings']=dict(verify.LEGACY_BINDINGS)
        return document
    def release_fixture(self):
        import stage_release_scope as release
        # 这是S3原2..19来源合同夹具；不能把S5全部正式绑定交给旧S3证据。
        temporary=tempfile.TemporaryDirectory();self.addCleanup(temporary.cleanup)
        project=Path(temporary.name)/'s3-project'
        shutil.copytree(ROOT/'data/static',project/'data/static')
        shutil.copytree(ROOT/'docs/generated/ida-canonical',project/'docs/generated/ida-canonical')
        path=project/'data/static/flows/common_event.json'
        document=json.loads(path.read_text(encoding='utf-8'))
        document['bindings']={key:value for key,value in document['bindings'].items() if int(key.split('.')[1])<20}
        path.write_text(json.dumps(document),encoding='utf-8')
        path=project/'data/static/flows/stage.json';stage_document=json.loads(path.read_bytes())
        for flow in stage_document['flows']:
            if flow['id'].startswith('stage.capability.') and int(flow['id'].split('.')[-1])>=20:
                flow['nodes'][0]['args'][0]=0
        path.write_text(json.dumps(stage_document),encoding='utf-8')
        sources=release.ledger_tool.Sources(ROOT/'original_game')
        flows,aliases,_=release.audit.load_definitions(project/'data/static')
        stages={}
        for identity,definition in document['bindings'].items():
            if identity in verify.LEGACY_BINDINGS:continue
            parts=identity.split('.');stage=int(parts[1]);callback=parts[2]=='action'
            slot=int(parts[3]) if callback else stage;va=(0x51b91 if callback else 0x51b19)+slot*4
            source=sources.exe(va,4);raw=sources.read(source)
            source.update(raw_hex=raw.hex(),read_sha256=verify.digest(raw))
            row=stages.setdefault(stage,{'stage':stage,'battle_enabled':True,
                'capability':release.audit.capability(stage,flows,aliases),'bindings':[]})
            row['bindings'].append({'runtime_id':identity,'definition_domain':'common_event',
                'definition_id':definition,'source_id':f'dispatch:{"event_callback" if callback else "stage_runtime_result"}:{slot:03d}',
                'original_target':hex(sources.fixups[va][2]),'source':source,
                'definition_sha256':verify.digest(release.disposition.encode(flows[definition]).encode())})
        receipt={'schema':'fd2-stage-release-scope-v1','structural_binding_scope_closed':True,
            'input_sha256':{str(path):verify.digest(path.read_bytes()) for path in
                (project/'data/static/flows/common_event.json',project/'data/static/flows/stage.json',
                 ROOT/'build/s2/source-disposition-final03/disposition.json',
                 ROOT/'build/s3/callback-candidate12/candidates.json',
                 ROOT/'build/s3/callback-binding-source-candidate12/comparison.json')},
            'stages':list(stages.values())}
        return document,receipt,project
    def sources(self):
        return {'k_common_event_nested_descriptors':bytes([0,211,0,0,213,0,0,101,0,0,11,0]),
                'k_common_event_cell_items':bytes([29,43,51,61,71])}
    def flow(self,document,name):
        return next(f for f in document['flows'] if f['id']==name)
    def node(self,document,flow,node):
        return next(n for n in self.flow(document,flow)['nodes'] if n['id']==node)
    def test_current_source_contract(self):
        verify.validate_tables(self.tables(),self.sources())
        result=verify.validate_flow(self.flows())
        self.assertEqual((result['flow_count'],result['source_control_paths_checked'],result['bindings_added']),(21,19,0))
        self.assertEqual(result['existing_bindings_migrated'],6)
    def test_refresh0_calls_water_and_refresh1_skips_from_original_JNZ(self):
        raw=verify.source_range((ROOT/'original_game/FD2.EXE').read_bytes(),0x11cbc,12)
        result=verify.validate_refresh_water_source(raw)
        self.assertTrue(result['raw_argument_0_calls_water'] and result['raw_argument_1_skips_water'])
        inverted=bytearray(raw);inverted[5]=0x74
        with self.assertRaisesRegex(ValueError,'水色原条件'):verify.validate_refresh_water_source(inverted)
        data=self.flows();data['operations']['refresh_map']['args'][0]['name']='water'
        with self.assertRaisesRegex(ValueError,'0推进/1跳过'):verify.validate_flow(data)
    def test_table_source_byte_shape_identity_and_integer_rejected(self):
        mutations=[lambda d:d['tables'][0]['values'][0].__setitem__(0,2),
            lambda d:d['tables'][0]['values'][3].__setitem__(1,0),
            lambda d:d['tables'][1]['values'].__setitem__(4,True),
            lambda d:d['tables'][1].update(dimensions=[4]),
            lambda d:d['tables'][1]['source'].update(canonical_va='0x52742'),
            lambda d:d['tables'][0]['source'].update(file_sha256='0'*64)]
        for mutate in mutations:
            d=self.tables();mutate(d)
            with self.subTest(d=d),self.assertRaises(ValueError):verify.validate_tables(d,self.sources())
    def test_grant_must_follow_wait_ack_but_key_remove_precedes_ack(self):
        d=self.flows()
        self.node(d,'common.cell.grant','item.portrait')['on']['next']='item.grant'
        with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
        d=self.flows()
        self.node(d,'common.key.consume','has.key')['on']['yes']='used.say'
        self.node(d,'common.key.consume','used.wait')['on']['ack']='consume'
        self.node(d,'common.key.consume','consume')['on']['yes']='used.close'
        with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
    def test_child_actor_identity_and_parent_after_child_order(self):
        for index in range(3):
            d=self.flows();self.node(d,f'common.loot.child.{index}','child')['args'][1]=0
            with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
        d=self.flows();self.node(d,'common.loot.physical','child')['args'][1]=-1
        with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
        d=self.flows();self.node(d,'common.loot.child.0','child')['on']['ack']='done'
        with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
    def test_retry_after_late_failure_and_new_stage_binding_rejected(self):
        d=self.flows();self.node(d,'common.inventory.once','say')['on']['error']='caller'
        with self.assertRaisesRegex(ValueError,'冻结'):verify.validate_flow(d)
        d=self.flows();d['bindings']={'stage.002.action.001':'common.cell.grant'}
        with self.assertRaisesRegex(ValueError,'绑定'):verify.validate_flow(d)
    def test_source_specific_map_tail_and_random_order(self):
        d=self.flows();self.node(d,'common.map.replace','cells')['on']['next']='pan.a'
        with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
        d=self.flows();self.node(d,'common.random.pair','schedule')['args'][1]=0
        with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
        d=self.flows();self.node(d,'common.loot.physical','clear')['op']='clear_keys'
        with self.assertRaisesRegex(ValueError,'提交顺序'):verify.validate_flow(d)
    def test_new_or_duplicate_flow_is_not_source_closed(self):
        d=self.flows();d['flows'].append(d['flows'][0])
        with self.assertRaisesRegex(ValueError,'未审'):verify.validate_flow(d)
        d=self.flows();d['flows'].append({'id':'unreviewed.callback','entry':'end','nodes':[{'id':'end','op':'end','args':[],'on':{}}]})
        with self.assertRaisesRegex(ValueError,'未审'):verify.validate_flow(d)

    def test_current_S5_callback51_has_independent_original_shared_tail_guard(self):
        import stage_release_scope as release
        document=json.loads((ROOT/'data/static/flows/common_event.json').read_bytes())
        original=verify.validate_callback51_source(ROOT/'original_game/FD2.EXE',document)
        self.assertEqual(original['target_va'],'0x355f0')
        self.assertFalse(original['original_machine_code_executed'])
        flows={f['id']:f for f in document['flows']}
        identity='dispatch:event_callback:051';runtime='stage.021.action.051'
        source={'disposition':'converted_candidate','definition_id':'common.callback.051',
            'definition_domain':'common_event','target_va':'0x355f0','original_caller':'complete355f0/shared3530d', 'source':{}}
        actual=release.check_binding(runtime,identity,{identity:source},flows,document['bindings'])
        self.assertEqual(actual['definition_sha256'],original['definition_sha256'])
        for mode in ('story11','old_alias','coordinated_old_source_alias','target','reordered'):
            bad=copy.deepcopy(document);binding=copy.deepcopy(source)
            if mode=='story11':
                next(n for n in self.flow(bad,'common.callback.051')['nodes'] if n['op']=='story')['args']=[11]
            elif mode=='old_alias':bad['bindings'][runtime]='common.loot.child.2'
            elif mode=='coordinated_old_source_alias':
                bad['bindings'][runtime]=binding['definition_id']='common.loot.child.2'
            elif mode=='target':binding['target_va']='0x352ca'
            else:self.flow(bad,'common.callback.051')['nodes'].reverse();self.flow(bad,'common.callback.051')['entry']='done'
            # caller元数据/alias/图一起伪造也必须被固定原来源规则拒绝。
            badflows={f['id']:f for f in bad['flows']}
            with self.subTest(mode=mode),self.assertRaisesRegex(ValueError,'binding differs|callback51'):
                release.check_binding(runtime,identity,{identity:binding},badflows,bad['bindings'])
            if mode in ('story11','reordered'):
                with self.assertRaisesRegex(ValueError,'story3'):
                    verify.validate_callback51_source(ROOT/'original_game/FD2.EXE',bad)
        with tempfile.TemporaryDirectory() as directory:
            wrong=Path(directory)/'FD2.EXE';raw=bytearray((ROOT/'original_game/FD2.EXE').read_bytes())
            # object1 VA35632的立即数3改11；独立原身份门先拒绝，不执行该副本。
            raw[0x36014+0x35632-0x10000]=11;wrong.write_bytes(raw)
            with self.assertRaisesRegex(ValueError,'原EXE身份'):
                verify.validate_callback51_source(wrong,document)

    def test_S3_bindings_need_current_source_receipt_without_expanding_G4_semantics(self):
        document,receipt,project=self.release_fixture()
        with self.assertRaisesRegex(ValueError,'release-scope'):verify.validate_flow(document)
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'scope.json';path.write_text(json.dumps(receipt),encoding='utf-8')
            result=verify.validate_flow(document,path,ROOT/'original_game/FD2.EXE',root=project)
            self.assertEqual((result['flow_count'],result['source_control_paths_checked'],result['existing_bindings_migrated']),(21,19,6))
            self.assertEqual(result['additional_shipping_bindings_source_checked'],44)
            self.assertEqual(len(result['additional_shipping_bindings_not_semantically_checked']),44)
            self.assertFalse(result['additional_binding_semantics_proven_by_this_tool'])
            self.assertEqual(result['bindings_added'],0)
            old51=copy.deepcopy(document);old51['bindings']['stage.021.action.051']='common.loot.child.2'
            with self.assertRaisesRegex(ValueError,'callback51.*S5'):
                verify.validate_flow(old51,path,ROOT/'original_game/FD2.EXE',root=project)
            # 同时篡改收据行和alias也不能借两份自报SHA把原slot9接成另一个已知图。
            forged=copy.deepcopy(document);forged_receipt=copy.deepcopy(receipt)
            identity='stage.002.action.009';replacement='common.callback.026'
            forged['bindings'][identity]=replacement
            for stage in forged_receipt['stages']:
                for row in stage['bindings']:
                    if row['runtime_id']==identity:
                        row['definition_id']=replacement
                        import stage_release_scope as release
                        row['definition_sha256']=verify.digest(release.disposition.encode(self.flow(forged,replacement)).encode())
            # 实际临时冷输入和scope输入摘要也一并更新，排除只靠旧file SHA挡住的假负例。
            mirror=Path(directory)/'project'
            shutil.copytree(project/'data/static',mirror/'data/static')
            shutil.copytree(project/'docs/generated/ida-canonical',mirror/'docs/generated/ida-canonical')
            (mirror/'data/static/flows/common_event.json').write_text(json.dumps(forged),encoding='utf-8')
            for name in ('common_event','stage'):
                old=project/f'data/static/flows/{name}.json';new=mirror/f'data/static/flows/{name}.json'
                forged_receipt['input_sha256'].pop(str(old))
                forged_receipt['input_sha256'][str(new)]=verify.digest(new.read_bytes())
            path.write_text(json.dumps(forged_receipt),encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'binding differs'):
                verify.validate_flow(forged,path,ROOT/'original_game/FD2.EXE',root=mirror)
            # 已知callback26也不能移植到原metadata没有该caller的stage2。
            forged=copy.deepcopy(document);forged_receipt=copy.deepcopy(receipt)
            new=copy.deepcopy(next(row for stage in receipt['stages'] for row in stage['bindings']
                if row['runtime_id']=='stage.006.action.026'))
            new['runtime_id']='stage.002.action.026';forged['bindings'][new['runtime_id']]=new['definition_id']
            next(stage for stage in forged_receipt['stages'] if stage['stage']==2)['bindings'].append(new)
            path.write_text(json.dumps(forged_receipt),encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'发布域'):
                verify.validate_flow(forged,path,ROOT/'original_game/FD2.EXE',root=project)
            path.write_text(json.dumps(receipt),encoding='utf-8')
            for mutate in (lambda d:d['bindings'].update({'stage.020.action.082':'common.callback.009'}),
                    lambda d:d['bindings'].__setitem__('stage.002.action.009','common.callback.026'),
                    lambda d:d['bindings'].pop('stage.012.result'),
                    lambda d:self.flow(d,'common.callback.009')['nodes'][0]['args'].append(999)):
                bad=copy.deepcopy(document);mutate(bad)
                with self.subTest(bindings=bad['bindings']),self.assertRaises(ValueError):
                    verify.validate_flow(bad,path,ROOT/'original_game/FD2.EXE',root=project)
            for mutate in (lambda r:r.update(structural_binding_scope_closed=False),
                    lambda r:r['input_sha256'].__setitem__(str(project/'data/static/flows/stage.json'),'0'*64),
                    lambda r:r['stages'][0]['bindings'][0].update(source_id='dispatch:event_callback:082'),
                    lambda r:r['stages'][0]['bindings'][0].update(original_target='0x362e8'),
                    lambda r:r['stages'][0].update(battle_enabled=False),
                    lambda r:r['stages'][0]['bindings'][0].update(definition_sha256='0'*64),
                    lambda r:r['stages'][0]['bindings'].append(r['stages'][0]['bindings'][0])):
                bad=copy.deepcopy(receipt);mutate(bad);path.write_text(json.dumps(bad),encoding='utf-8')
                with self.subTest(receipt=bad),self.assertRaises(ValueError):
                    verify.validate_flow(document,path,ROOT/'original_game/FD2.EXE',root=project)

if __name__=='__main__':unittest.main()
