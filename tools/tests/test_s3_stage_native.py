"""S3 原生协议/独立容器模型纯测；不启动GUI、游戏或driver。"""
import copy
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from native import s3_fixture as f
import run_s3_stage_check as runner


class S3FixtureTests(unittest.TestCase):
    def source(self):
        result=bytearray((i*29+11)&255 for i in range(0x59cb))
        result[f.META:f.META+10]=bytes((1,15,2,3,18,6,22,3,4,6))
        for i in range(15):
            unit=bytearray(80);unit[:2]=bytes((i%10+4,i//10+20));unit[6]=2 if i<6 else 1 if i==6 else 0
            unit[7:9]=bytes((i,i));unit[49]=255;struct.pack_into('<4H',unit,64,100,100,10,10)
            result[f.UNITS+i*80:f.UNITS+(i+1)*80]=unit
        for i,identity in enumerate((0,9,4,30,1,8)):
            result[0x8a3+i*80+7:0x8a3+i*80+9]=bytes((identity,identity))
        return f.checksum(result)

    def data(self):
        raw=bytearray(0x63000);base=bytearray(24)
        base[:3]=bytes((2,3,4));struct.pack_into('<HH',base,3,20,6);base[7]=5
        base[8:12]=bytes((1,2,3,4));base[12:18]=bytes((1,2,255,4,255,5))
        struct.pack_into('<3H',base,18,10,12,14)
        raw[0x61da1+48:0x61da1+72]=base;raw[0x620a1+22:0x620a1+33]=bytes((2,1,3,1,4,1,5,1,6,1,0))
        items=bytearray(215*23);struct.pack_into('<4h',items,24,2,3,4,-5);struct.pack_into('<4h',items,47,7,8,9,10)
        return {'raw':raw,'items':bytes(items)}

    def test_controlled_derivation_has_only_declared_flags_checksum_and_no_source_mutation(self):
        source=self.source();before=bytes(source)
        for scenario,flag in zip(f.SCENARIOS,(4,1)):
            actual,manifest=f.derive(source,scenario)
            self.assertEqual(source,before);self.assertTrue(manifest['controlled_fixture']);self.assertFalse(manifest['runtime_alias_or_API_injection'])
            for row in manifest['plaintext_patches']:
                if row['offset']<len(source)-4:
                    self.assertEqual((row['offset']-f.UNITS)%80,5)
                    self.assertEqual(row['after'],row['before']|flag)
            self.assertFalse(actual[f.UNITS+6*80+5]&1)
        for offset,value in ((f.META,2),(f.META+1,14),(f.META+2,3),(f.META+9,7),(f.UNITS+6*80+5,1)):
            invalid=bytearray(source);invalid[offset]=value
            with self.assertRaises(ValueError):f.derive(f.checksum(invalid),f.SCENARIOS[0])

    def test_112a5_preserves_exact22_bytes_and_uses_source_base_growth_equipment(self):
        before=bytes(range(80));after=f.persistent_create(before,2,self.data())
        untouched=set(range(5))|{23,25}|set(range(40,49))|set(range(50,55))|{61}
        self.assertEqual(len(untouched),22)
        self.assertTrue(all(after[i]==before[i] for i in untouched))
        self.assertEqual(after[5:10],bytes((0,2,2,2,0)));self.assertEqual(after[31:34],bytes((2,3,4)))
        self.assertEqual(struct.unpack_from('<4H',after,64),(35,35,24,24))
        self.assertEqual(struct.unpack_from('<4H',after,72),(27,37,41,35))
        for identity in (-1,32):
            with self.assertRaises(ValueError):f.persistent_create(before,identity,self.data())

    def test_11506_sync_happens_before_append_and_new_slot_preserves_old_unknown_owner(self):
        source=self.source();data=self.data();actual=f.settlement(source,data,'sdl')
        old=source[0x8a3+6*80:0x8a3+7*80]
        self.assertEqual(actual['roster'][6*80:7*80],f.persistent_create(old,2,data))
        self.assertEqual(actual['stage'],3);self.assertEqual(actual['count'],7)
        self.assertEqual(actual['roster'][7*80:],source[0x8a3+7*80:0x12a3])
        self.assertNotEqual(actual['roster'][6*80:7*80],source[f.UNITS+2*80:f.UNITS+3*80])

    def test_loaded_container_rejects_unknown_or_fresh_hole_edits_and_wrong_cache_policy(self):
        before=self.source();view=bytearray(before);view[f.META+5]=7;view=f.checksum(view)
        for kind in ('original','sdl'):
            expected=bytearray(view)
            for i,unit in enumerate(f.loaded_units(before,kind)):expected[f.UNITS+i*80:f.UNITS+(i+1)*80]=unit
            expected=f.checksum(expected)
            self.assertTrue(f.check_loaded_save(before,expected,view,kind)['loaded_unknown_bytes_preserved'])
            for at in (40,f.UNITS+40,0x312b+17,len(before)-8):
                bad=bytearray(expected);bad[at]^=1
                with self.assertRaises(AssertionError):f.check_loaded_save(before,f.checksum(bad),view,kind)

    def test_manual_full_container_protects_other_slots_and_constructor_preserved_bytes(self):
        before=self.source();state=f.settlement(before,self.data(),'sdl')
        for slot in range(4):
            after=f.write_slot(before,state,slot);self.assertEqual(f.check_manual(before,after,state,slot)['checked_bytes'],22987)
            for at in (10,f.UNITS+79,f.SLOT_BASE+slot*f.SLOT_STRIDE+6*80,f.SLOT_BASE+((slot+1)%4)*f.SLOT_STRIDE):
                bad=bytearray(after);bad[at]^=1
                with self.assertRaises(AssertionError):f.check_manual(before,f.checksum(bad),state,slot)

    def test_activity_masks_allow_only_exact_fresh_record_holes_not_other_bytes(self):
        before=self.source();records=f.loaded_units(before,'sdl');mask=[bytearray([1]*80) for _ in records];mask[3][40]=0
        after=bytearray(before);after[f.UNITS+3*80+40]^=1;after=f.checksum(after)
        self.assertTrue(f.check_activity(before,after,before,records,mask,before[:1171],kind='sdl')['fresh_exclusions'])
        bad=bytearray(after);bad[f.UNITS+3*80+41]^=1
        with self.assertRaises(AssertionError):f.check_activity(before,f.checksum(bad),before,records,mask,before[:1171],kind='sdl')

    def test_original_fresh_metadata_heap_tail_is_not_an_SDL_or_Loaded_exclusion(self):
        before=self.source();records=f.loaded_units(before,'sdl');masks=[bytes([1]*80) for _ in records]
        for prefix_size in (1171,1951):
            metadata=before[:prefix_size]
            for kind in ('original','sdl'):
                ordinary=f.check_activity(before,before,before,records,masks,metadata,kind=kind,fresh_metadata=True)
                self.assertEqual(ordinary['metadata_prefix_size'],prefix_size)
                for at in (prefix_size,0x8a2):
                    changed=bytearray(before);changed[at]^=1;changed=f.checksum(changed)
                    if kind=='original':
                        result=f.check_activity(before,changed,before,records,masks,metadata,kind=kind,fresh_metadata=True)
                        self.assertEqual(result['metadata_tail_policy'],'explicit original fresh heap exclusion')
                        self.assertTrue(any(e['owner']=='original fresh metadata heap tail' for e in result['fresh_exclusions']))
                    else:
                        with self.assertRaises(AssertionError):f.check_activity(before,changed,before,records,masks,metadata,kind=kind,fresh_metadata=True)
                    with self.assertRaises(AssertionError):f.check_activity(before,changed,before,records,masks,metadata,kind=kind,fresh_metadata=False)
                    with self.assertRaises(AssertionError):f.check_loaded_save(before,changed,before,kind)
            for invalid in (metadata[:-1],bytes(130),bytes(2212)):
                with self.assertRaises(ValueError):f.check_activity(before,before,before,records,masks,invalid,kind='sdl')

    def test_entry12_reserved_raw6_has_full_memset_and_does_not_consume_roster_or_placement(self):
        metadata=bytearray(131);metadata[1:3]=bytes((15,70))
        placements=[[0,0] for _ in range(70)]+[[2+i%9,10+i//9] for i in range(15)]
        data={'stages':{12:{'metadata':bytes(metadata),'placement':placements}}}
        roster=bytearray((i*17+3)&255 for i in range(2560))
        for i in range(15):roster[i*80+5]=0;roster[i*80+8]=i
        def group(records,masks,*args):
            for _ in range(44):
                unit=bytearray(80);unit[5]=1;records.append(unit);masks.append(bytearray([1]*80))
        for sixth_text in (8,2):
            roster[6*80+8]=sixth_text;state={'count':15,'roster':bytes(roster)}
            with patch.object(f,'append_group',side_effect=group):
                view,records,masks=f.entry12_projection(self.source(),state,data,ROOT/'original_game')
            self.assertEqual(len(records),59);self.assertEqual(view[f.META+9],15)
            self.assertEqual(view[0x8a3:0x12a3],state['roster'])
            if sixth_text==8:
                empty=bytearray(80);empty[5]=1
                self.assertEqual(records[6],empty);self.assertEqual(masks[6],bytes([1]*80))
                self.assertEqual(records[7][7:9],roster[6*80+7:6*80+9])
                self.assertEqual(list(records[7][:2]),placements[76])
                self.assertEqual(records[14][7:9],roster[13*80+7:13*80+9])
                self.assertEqual(records[7][40:49],roster[6*80+40:6*80+49])
            else:
                self.assertEqual(records[6][8],2);self.assertEqual(list(records[7][:2]),placements[77])
                self.assertEqual(records[14][7:9],roster[14*80+7:14*80+9])


class S3RunnerTests(unittest.TestCase):
    def test_activity_Save_has_one_confirmation_then_measures_and_validates_before_publishing(self):
        source=S3FixtureTests().source()
        for reject in (False,True):
            with self.subTest(reject=reject),tempfile.TemporaryDirectory() as temporary:
                work=Path(temporary);save=work/'FD2.SAV';save.write_bytes(f.encode(source));keys=[];events=[]
                capture=SimpleNamespace(artifacts=work,last=None,key=keys.append);p={'original':work,'assets':{}}
                guard=[save.read_bytes(),save.stat().st_mtime_ns];record={}
                def measured(*args):events.append('actual_save_measured');return source
                def validate(before,after,view):
                    events.append('validated')
                    if reject:raise AssertionError('independent source mismatch')
                    return f.check_loaded_save(before,after,view,'sdl')
                with patch.object(runner.natural,'cursor_bank'),patch.object(runner.natural,'empty_tile',return_value={'world':(6,22)}),\
                     patch.object(runner.protocol,'navigate',return_value=source),patch.object(runner,'wait_measured_save',side_effect=measured):
                    if reject:
                        with self.assertRaisesRegex(AssertionError,'independent'):runner.save_activity(capture,p,source,save,guard,record,lambda *a,**k:None,'probe',validate)
                        self.assertNotIn('completed_save',record)
                    else:runner.save_activity(capture,p,source,save,guard,record,lambda *a,**k:None,'probe',validate)
                self.assertEqual(keys,['ENTER','ENTER','LEFT','ENTER','ENTER']);self.assertEqual(events,['actual_save_measured','validated'])
                self.assertTrue((work/'probe.SAV').is_file()) # 失败测量原件也保留。

    def test_active_Load_cancel_precedes_real_Load_and_final_whole_container_measurement(self):
        source=S3FixtureTests().source()
        with tempfile.TemporaryDirectory() as temporary:
            work=Path(temporary);save=work/'FD2.SAV';save.write_bytes(f.encode(source));keys=[];labels=[]
            capture=SimpleNamespace(artifacts=work,last=None,key=keys.append);p={'original':work,'assets':{}}
            record={'kind':'sdl'};guard=[save.read_bytes(),save.stat().st_mtime_ns]
            with patch.object(runner.natural,'cursor_bank'),patch.object(runner.natural,'empty_tile',return_value={'world':(6,22)}),\
                 patch.object(runner.protocol,'navigate',return_value=source),patch.object(runner,'save_activity',return_value=source) as save_call:
                runner.load_activity(capture,p,source,save,guard,record,lambda name,*a,**k:labels.append(name))
            self.assertEqual(keys,['ENTER','ENTER','RIGHT','ENTER','ESC','ENTER','ENTER','RIGHT','ENTER','ENTER'])
            self.assertLess(labels.index('s3-load-cancel-browse'),labels.index('s3-confirm-load-default-yes'))
            self.assertTrue(record['load_cancel']['disk_bytes_and_mtime_unchanged']);self.assertFalse(record['load_cancel']['runtime_full_state_claimed'])
            save_call.assert_called_once();check=save_call.call_args.args[-1]
            self.assertTrue(check(source,source,source)['loaded_unknown_bytes_preserved'])

    def test_stage12_cancel_has_real_input_order_and_unique_post_anchor_before_Save(self):
        source=S3FixtureTests().source();view=bytearray(source);view[f.META+2]=12;view=f.checksum(view)
        state={'stage':12,'count':15,'roster':bytes(2560),'resource':0,'options':bytes(4)}
        for emits in (0,1,2):
            with self.subTest(post_count=emits),tempfile.TemporaryDirectory() as temporary:
                work=Path(temporary);save=work/'FD2.SAV';save.write_bytes(f.encode(source));log=work/'game.log';log.write_text('')
                keys=[];labels=[];anchor='field result query: stage=12 result=0'
                def key(value):
                    keys.append(value)
                    if value=='ESC':log.write_text((anchor+'\n')*emits)
                capture=SimpleNamespace(artifacts=work,process=SimpleNamespace(poll=lambda:None),key=key,wait=lambda value:None)
                p={'original':work,'assets':{},'titles':{3:[None,None,None]},'data':{'stages':{12:{'metadata':source[:1171]}}}}
                record={'kind':'sdl','checkpoints':[]};guard=[save.read_bytes(),save.stat().st_mtime_ns]
                def observed(cap,name,predicate,points,**options):
                    labels.append(name);points.append({'name':name});return None,{}
                with patch.object(runner,'preparation_town',return_value={'assets':{},'towns':[]}),\
                     patch('run_original_battle_menu.wait_title'),patch.object(runner,'wait_state',side_effect=observed),\
                     patch.object(f,'entry12_projection',return_value=(view,[],[])),patch.object(f,'pages',return_value=[]),\
                     patch.object(runner,'notice_resources',return_value={}),patch.object(runner.natural,'selection_summary',return_value={}),\
                     patch.object(runner.natural,'range_probe',return_value=[]),patch.object(runner,'wait_log',side_effect=lambda *a,**k:log.read_text()),\
                     patch.object(runner,'save_activity',return_value=view) as saved,patch.object(runner,'load_activity',return_value=view) as loaded:
                    if emits==1:self.assertEqual(runner.title_load_entry(capture,p,save,guard,record,state),view)
                    else:
                        with self.assertRaisesRegex(AssertionError,'唯一1197b'):runner.title_load_entry(capture,p,save,guard,record,state)
                self.assertEqual(keys,['DOWN','ENTER','ENTER','LEFT','LEFT','ENTER','ENTER','ENTER','ESC'])
                self.assertLess(labels.index('s3-stage12-real-selection'),labels.index('s3-stage12-selection-cancelled'))
                if emits==1:
                    saved.assert_called_once();loaded.assert_called_once();self.assertTrue(record['result_post']['SDL_unique_post_log_verified'])
                    self.assertFalse(record['result_post']['original_runtime_latch_dumped'])
                else:saved.assert_not_called();loaded.assert_not_called();self.assertNotIn('result_post',record)

    def test_actual_build_identity_excludes_other_builds_and_new_source_is_detected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);src=root/'src';src.mkdir();(src/'main.c').write_text('real source')
            original=root/'original_game';original.mkdir();(original/'data').write_bytes(b'original')
            build=src/'build-s3';generated=build/'generated/static_data';generated.mkdir(parents=True)
            (build/'CMakeCache.txt').write_text('CMAKE_HOME_DIRECTORY:INTERNAL='+str(src)+'\n')
            (generated/'stage.inc').write_text('current candidate')
            other=src/'build-win/generated/static_data';other.mkdir(parents=True);(other/'stage.inc').write_text('old candidate')
            binary=root/'fd2sdl.exe';binary.write_bytes(b'actual app')
            inputs,baseline,sources=runner.freeze(root,original,[binary],build)
            self.assertIn(str(generated/'stage.inc'),inputs);self.assertNotIn(str(other/'stage.inc'),inputs)
            self.assertTrue(runner.unchanged(inputs,original,baseline,root,sources))
            (src/'new.c').write_text('new product source');self.assertFalse(runner.unchanged(inputs,original,baseline,root,sources))
            (build/'CMakeCache.txt').write_text('CMAKE_HOME_DIRECTORY:INTERNAL='+str(root/'other')+'\n')
            with self.assertRaisesRegex(ValueError,'本工作树'):runner.freeze(root,original,[binary],build)

    def test_preparation_never_opens_window_or_reports_runtime_pass(self):
        with tempfile.TemporaryDirectory() as temporary:
            root=Path(temporary);original=root/'original_game';original.mkdir();output=root/'build/s3/prepared'
            args=SimpleNamespace(root=root,original=original,artifacts=output,save_fixture=root/'source.SAV',source_report=root/'report.json',scenario=f.SCENARIOS[0],run=False)
            prepared={'manifest':{'controlled_fixture':True},'encoded':b'isolated','source':b''}
            with patch.object(f,'prepare',return_value=prepared),patch.object(f,'preflight',return_value={}),\
                 patch.object(runner,'title_templates'),patch.object(runner,'owned_job_required') as job,\
                 patch.object(runner,'running') as running:
                report=runner.run(args)
            self.assertEqual(report['status'],'prepared');self.assertFalse(report['ok']);self.assertEqual(report['cases'],[])
            job.assert_not_called();running.assert_not_called();self.assertEqual((output/'controlled.SAV').read_bytes(),b'isolated')


class S3RewardObservationTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        from run_original_end_turn import round_assets
        cls.original=ROOT/'original_game';cls.assets=round_assets(cls.original)

    def source(self):
        source=bytearray(S3FixtureTests().source());source[f.META+1]=2
        for raw,(identity,side) in enumerate(((8,2),(51,1))):
            source[f.UNITS+80*raw+6:f.UNITS+80*raw+8]=bytes((side,identity))
        return f.checksum(source)

    def test_explicit_stage_resource_readset_keeps_legacy_gate_and_uses_actual_stage_templates(self):
        source=self.source();before=bytes(source)
        with self.assertRaisesRegex(ValueError,'六种portrait'):
            runner.natural.required_reward_pages(self.original,self.assets,source)
        pages=runner.natural.required_reward_pages(self.original,self.assets,source,resource_stage=2)
        meta=runner.natural.archive_entry((self.original/'FDFIELD.DAT').read_bytes(),7)
        expected={8,51}|{meta[i+1] for i in range(131,len(meta),26) if meta[i] in (1,2) and meta[i+1]<68}
        self.assertEqual({p['portrait_id'] for p in pages},expected)
        self.assertNotIn(3,expected) # 首关未来模板不能混入stage2观察域。
        self.assertEqual(source,before)
        for wrong in (True,-1,0,3,30):
            with self.assertRaisesRegex(ValueError,'当前stage'):
                runner.natural.required_reward_pages(self.original,self.assets,source,resource_stage=wrong)

    def test_added_portraits_keep_full_resource_text_arrow_and_growth_guards(self):
        from fd2_reward_oracle import page_match, reward_tables
        pages=runner.natural.required_reward_pages(self.original,self.assets,self.source(),resource_stage=2)
        for identity in (8,51):
            page=next(p for p in pages if p['portrait_id']==identity and p['fragment']==489)
            frame=page['text'].convert('RGB');frame.paste(self.assets['dialog_top'].convert('RGB'),(5,112))
            frame.paste(page['portraits'][0].convert('RGB'),page['portrait_position'])
            frame.paste(page['arrows'][0].convert('RGB'),page['arrow_position'])
            self.assertIsNotNone(page_match(frame,page,self.assets))
            for point in ((8,115),(87,194),page['arrow_position']):
                bad=frame.copy();bad.putpixel(point,(255,0,255))
                self.assertIsNone(page_match(bad,page,self.assets))
        tables,proof=reward_tables(self.original/'FD2.EXE');changed=dict(tables);growth=bytearray(tables['growth'])
        for index in (1,2,3,4):growth[8*11+2*index+1]=0
        changed['growth']=bytes(growth)
        with patch.object(runner.natural,'reward_tables',return_value=(changed,proof)):
            with self.assertRaisesRegex(ValueError,'成长项不足'):
                runner.natural.required_reward_pages(self.original,self.assets,self.source(),resource_stage=2)


if __name__=='__main__':unittest.main()
