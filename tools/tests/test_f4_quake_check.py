"""Independent source/owner/protocol boundaries; no process, GUI or C execution."""
import argparse
import copy
from contextlib import contextmanager
import importlib.util
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image

ROOT=next(p for p in Path(__file__).resolve().parents if (p/'AGENTS.md').is_file())
sys.path.insert(0,str(ROOT/'tools'))
MODULE=Path(__file__).resolve().parents[1]/'run_f4_quake_check.py'
G1_SOURCE=next(p for p in (ROOT/'build/g1/ida/preparation/scope-callee-closure.json',
    ROOT.parent.parent/'build/g1/ida/preparation/scope-callee-closure.json') if p.is_file())
spec=importlib.util.spec_from_file_location('quake_native_candidate',MODULE)
m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)


class QuakeSourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.p=m.prepare(ROOT,m.SCENARIO,lambda p,expected=None:Path(p).read_bytes())

    def test_exact_donor_transport_and_xy_only(self):
        original=(ROOT/'original_game').resolve()
        start=m.codec.crypt((original/'START.SAV').read_bytes())
        donor=m.codec.crypt((original/'FD2.SAV').read_bytes())[0x8a3:0x8f3]
        expected=bytearray(donor);expected[:2]=b'\x09\x0e'
        self.assertEqual(self.p['source'][m.UNITS:m.UNITS+80],expected)
        self.assertEqual(self.p['source'][0x8a3:m.UNITS],start[0x8a3:m.UNITS])
        actual={i for i,(a,b) in enumerate(zip(start,self.p['source'])) if a!=b}
        allowed=set(range(m.UNITS,m.UNITS+80))|{m.UNITS+880,m.UNITS+881}|set(range(len(start)-4,len(start)))
        self.assertLessEqual(actual,allowed)
        self.assertEqual(m.codec.magic_ids(expected),[3,7,8,12,15,23])

    def test_source_UI_models_are_not_executed_Saves(self):
        for kind,wanted in self.p['expected'].items():
            self.assertEqual(wanted[0x8a3:m.UNITS],self.p['source'][0x8a3:m.UNITS])
            self.assertEqual(struct.unpack_from('<HHHH',wanted,m.UNITS+64),(892,892,261,341))
            self.assertEqual(wanted[m.UNITS+60],38)
            self.assertEqual(wanted[m.META:m.META+3],bytes((1,12,0)))
            self.assertEqual(wanted[m.META+5:m.META+9],bytes((9,14,4,3)))
        a,b=self.p['expected']['miss'],self.p['expected']['hit']
        meaningful={i for i,(x,y) in enumerate(zip(a,b)) if x!=y and i<len(a)-4}
        self.assertEqual(meaningful,{m.UNITS+11*80+5,m.UNITS+11*80+64})

    def test_measurement_contract_has_no_Save_or_runtime_dump(self):
        self.assertEqual(self.p['measurement']['Save_count'],0)
        self.assertTrue(self.p['measurement']['disk_must_remain_original_fixture'])
        self.assertEqual(self.p['postcast_detail'][1]['mp'],261)
        self.assertEqual(self.p['feedback_mode'],'strict-whole')
        self.assertFalse(hasattr(m,'full_save_check'))

    def test_no_saturation_net_HP_or_reward_denominator_shortcut(self):
        damages={9*340//10+340*x//1000 for x in range(100)}
        self.assertEqual((min(damages),max(damages)),(306,339))
        self.assertTrue(all(x>28 for x in damages));self.assertEqual(42//61,0)

    def test_strict_resource_identity_rejected_before_transport(self):
        def get(path,expected=None):
            raw=Path(path).read_bytes()
            if Path(path).name=='FD2.SAV':raw=raw[:-1]+bytes((raw[-1]^1,))
            if expected:m.require(m.digest(raw)==expected,'pin')
            return raw
        with self.assertRaises(ValueError):m.derive((ROOT/'original_game').resolve(),get)

    def test_quake_source_three_origins_and_no_water_palette_dependency(self):
        model=self.p['visual_model']
        self.assertEqual(model['summary']['first_samples'],[[15020,33632],[15360,33792],[15700,34208]])
        # Typed scene has no water indices in ANY of the actual three pages/three idle phases.
        self.assertTrue(all(not(set(v['indices'])&set(range(224,240))) for v in model['snapshots']))
        self.assertEqual(len({v['rgb'][0] for v in model['snapshots'] if v['idle']==0}),3)
        for v in model['snapshots']:self.assertTrue(all(rgb==v['rgb'][0] for rgb in v['rgb']))

    def test_ring_excludes_the_two_real_ordinary_range_equivalences(self):
        model=self.p['visual_model']
        self.assertEqual(len(model['ring']),88)
        keys={(r['phase'],tuple(r['position'])) for r in model['ring']}
        self.assertNotIn((2,(124,124)),keys)
        self.assertNotIn((3,(100,148)),keys)
        self.assertIn((1,(76,28)),keys)  # proper curved edge, not a blanket tile LUT
        field=m.archive_entry((ROOT/'original_game/FDFIELD.DAT').read_bytes(),0)
        meta=m.archive_entry((ROOT/'original_game/FDFIELD.DAT').read_bytes(),1)
        sheet=m.archive_entry((ROOT/'original_game/FDSHAP.DAT').read_bytes(),meta[0]*2)
        lutraw=m.archive_entry((ROOT/'original_game/FDOTHER.DAT').read_bytes(),3)
        # Source independent geometry: each of these whole24x24 windows lies
        # inside the ellipse; therefore it is exactly the ordinary whole-tile LUT.
        for phase,wx,wy,radius in ((2,10,16,62),(3,9,17,78)):
            cell=struct.unpack_from('<I',field,4+4*(wy*24+wx))[0]
            tile,mask=m.packed_frame(sheet,cell&1023);base=tile.tobytes()
            self.assertTrue(all(mask.tobytes()))
            lo,hi=struct.unpack_from('<II',lutraw,6+4*phase);lut=lutraw[lo:hi]
            x,y=(wx-5)*24,(wy-11)*24
            self.assertTrue(all(25*(x+c-108)**2<36*(radius**2-(y+r-90)**2)
                for r in range(24) for c in range(24)))
            whole=m.rgb(bytes(lut[c] for c in base),model['palettes'][0])
            self.assertNotEqual(whole,m.rgb(base,model['palettes'][0]))

    def test_whole_border_is4096_and_single_pixel_not_ignored(self):
        image=Image.new('RGB',(320,200),(1,2,3));other=image.copy();other.putpixel((319,199),(4,5,6))
        self.assertEqual(len(m.border_bytes(image)),4096*3)
        self.assertNotEqual(m.border_bytes(image),m.border_bytes(other))
        forbidden=next(iter(self.p['visual_model']['water_rgb']))
        image.putpixel((0,0),forbidden)
        with self.assertRaises(ValueError):m.qualify_border(image,self.p['visual_model'])

    def test_source_order_not_arbitrary_three_page_set(self):
        self.assertTrue(m.ordered_quake([0,0,1,2,1,0,2,1]))
        self.assertFalse(m.ordered_quake([0,1]))
        self.assertFalse(m.ordered_quake([0,2]*31+[1]))
        with self.assertRaises(ValueError):m.ordered_quake([False,1,2])

    def test_compound_local_Q0_is_source_fixed_and_never_a_whole_page(self):
        model=self.p['visual_model'];windows=model['Q0_local_windows']
        self.assertEqual([tuple(w['position']) for w in windows],[(28,y) for y in (28,52,76,100,124,148)])
        for w in windows:
            x,y=w['position'];x-=4;y-=4
            for v in model['snapshots']:
                crop=Image.frombytes('RGB',(312,192),v['rgb'][0]).crop((x,y,x+24,y+24)).tobytes()
                if v['q']==0:self.assertEqual(crop,w['rgb'])
                else:self.assertNotEqual(crop,w['rgb'])
        before=Image.new('RGB',(320,200),(1,2,3))
        def qpage(q):
            v=next(v for v in model['snapshots'] if v['q']==q and v['idle']==0)
            page=before.copy();page.paste(Image.frombytes('RGB',(312,192),v['rgb'][0]),(4,4));return page
        ring=before.copy();r=model['ring'][0];ring.paste(Image.frombytes('RGB',(24,24),r['rgb']),r['position'])
        partial=before.copy();w=windows[0];partial.paste(Image.frombytes('RGB',w['size'],w['rgb']),w['position'])
        spec=next(m.popup_models(model,'miss'));popup=before.copy()
        popup.paste(Image.frombytes('RGB',spec['size'],spec['rgb']),spec['position'])
        images=[ring,partial,qpage(1),qpage(2),qpage(1),popup]
        rows=[{'index':i,'sha256':str(i)} for i in range(len(images))]
        with patch.object(m,'read_frame',side_effect=lambda row:images[row['index']]):
            self.assertFalse(m.feedback(rows,before,model,'miss')['ok'])
            result=m.feedback(rows,before,model,'miss','compound-feedback')
        self.assertTrue(result['ok']);self.assertFalse(result['full64000_quake_RGB_and4096_border'])
        self.assertEqual([r['index'] for r in result['Q0_local_samples']],[1])
        self.assertEqual([r['index'] for r in result['quake_full_page_samples']],[2,3])
        self.assertFalse(result['complete_compound_qualification_claimed'])
        self.assertTrue(result['external_original_C9_logical_join_required'])
        x,y=w['position'];c=partial.getpixel((x,y));partial.putpixel((x,y),((c[0]+1)%256,c[1],c[2]))
        with patch.object(m,'read_frame',side_effect=lambda row:images[row['index']]):
            self.assertFalse(m.feedback(rows,before,model,'miss','compound-feedback')['ok'])
        with self.assertRaises(ValueError):m.feedback([],before,model,'miss','unknown')

    def test_initial_bank_water_RGB_rejected_without_assuming_a_tick(self):
        raw=m.archive_entry((ROOT/'original_game/FDOTHER.DAT').read_bytes(),0)
        initial={tuple((c<<2)|(c>>4) for c in raw[i*3:i*3+3]) for i in range(224,240)}
        cycles={tuple(p[i*3:i*3+3]) for p in self.p['visual_model']['palettes'] for i in range(224,240)}
        self.assertTrue(initial-cycles)  # reproduces the old gate gap, not an empty check
        self.assertLessEqual(initial,self.p['visual_model']['water_rgb'])
        for color in initial:
            image=Image.new('RGB',(320,200),(1,2,3));image.putpixel((319,199),color)
            with self.assertRaisesRegex(ValueError,'动态water色'):m.qualify_border(image,self.p['visual_model'])

    def test_quake_full_page_one_pixel_failure(self):
        wanted=next(v for v in self.p['visual_model']['snapshots'] if v['q']==1 and v['idle']==0)
        page=Image.new('RGB',(320,200),(1,2,3));page.paste(Image.frombytes('RGB',(312,192),wanted['rgb'][0]),(4,4))
        self.assertIn((1,0,0),m.quake_matches(page,self.p['visual_model']))
        p=page.getpixel((151,91));page.putpixel((151,91),((p[0]+1)%256,p[1],p[2]))
        self.assertEqual(m.quake_matches(page,self.p['visual_model']),[])

    def test_popup_uses_original_LMI_nonzero_mask_and_raw_damage(self):
        specs=list(m.popup_models(self.p['visual_model'],'hit'))
        self.assertEqual([v['value'] for v in specs],list(range(306,340)))
        for v in (specs[0],next(m.popup_models(self.p['visual_model'],'miss'))):
            self.assertEqual(v['position'],(124,73))
            page=Image.new('RGB',(320,200),(1,2,3));x,y=v['position'];w,h=v['size']
            page.paste(Image.frombytes('RGB',(w,h),v['rgb']),(x,y))
            self.assertIsNotNone(m.popup_match(page,[v]))
            i=next(i for i,f in enumerate(v['mask']) if f);at=(x+i%w,y+i//w)
            c=page.getpixel(at);page.putpixel(at,((c[0]+1)%256,c[1],c[2]))
            self.assertIsNone(m.popup_match(page,[v]))

    def test_output_original_and_existing_are_rejected(self):
        with self.assertRaises(ValueError):m.checked_output(ROOT,ROOT/'original_game'/'new')
        with tempfile.TemporaryDirectory() as d:
            p=Path(d);(p/'old').mkdir()
            with self.assertRaises(ValueError):m.checked_output(ROOT,p/'old',p)
            try:(p/'alias').symlink_to((ROOT/'original_game').resolve(),target_is_directory=True)
            except OSError:return
            with self.assertRaises(ValueError):m.checked_output(ROOT,p/'alias'/'new',p)

    def test_missing_feedback_not_retried_or_satisfied_by_empty_rows(self):
        model=self.p['visual_model'];before=Image.new('RGB',(320,200),(1,2,3))
        result=m.feedback([],before,model,'miss')
        self.assertFalse(result['ok']);self.assertEqual(result['quake_full_page_samples'],[])
        self.assertFalse(result['all60_submissions_claimed'])
        self.assertTrue(result['missing_feedback_is_missing_evidence_not_implementation_error'])

    def test_prelude_Q1_is_not_misclassified_as_first_quake(self):
        model=self.p['visual_model'];before=Image.new('RGB',(320,200),(1,2,3))
        def qpage(q):
            v=next(v for v in model['snapshots'] if v['q']==q and v['idle']==0)
            page=before.copy();page.paste(Image.frombytes('RGB',(312,192),v['rgb'][0]),(4,4));return page
        ring=before.copy();r=model['ring'][0];ring.paste(Image.frombytes('RGB',(24,24),r['rgb']),r['position'])
        v=next(m.popup_models(model,'miss'));popup=before.copy()
        popup.paste(Image.frombytes('RGB',v['size'],v['rgb']),v['position'])
        # Source11cac before ring can equal Q1; only the bounded later witness qualifies.
        images=[qpage(1),ring,qpage(0),qpage(1),qpage(2),qpage(1),popup]
        rows=[{'index':i,'sha256':str(i)} for i in range(len(images))]
        with patch.object(m,'read_frame',side_effect=lambda row:images[row['index']]):
            result=m.feedback(rows,before,model,'miss')
        self.assertTrue(result['ok']);self.assertEqual(result['ring_before_quake'][0]['index'],1)
        self.assertEqual([v['index'] for v in result['quake_full_page_samples']],[2,3,4])
        self.assertTrue(result['Q1_is_not_a_unique_PC_or_epoch_tag'])
        # An outer-byte failure inside the required three-page window is still fatal.
        images[4].putpixel((319,199),(255,1,2))
        with patch.object(m,'read_frame',side_effect=lambda row:images[row['index']]):
            self.assertFalse(m.feedback(rows,before,model,'miss')['ok'])

    def test_protocol_both_modes_disable_Save_cancel_twice_and_never_write(self):
        for mode in m.FEEDBACK_MODES:
          for failure in ('none','cancel','clock-gate','feedback','disk'):
            with self.subTest(mode=mode,failure=failure),tempfile.TemporaryDirectory() as directory:
                d=Path(directory);save=d/'FD2.SAV';save.write_bytes(self.p['encoded'])
                guard=[save.read_bytes(),save.stat().st_mtime_ns];calls=[];scope={'after':{'index':0}}
                image=Image.new('RGB',(320,200),(1,2,3));p=dict(self.p,feedback_mode=mode)
                list_frame=image.copy();where,resource=p['magic_lists'][0]
                list_frame.paste(resource.convert('RGB'),where)
                for where,resource in p['list_upper'].values():list_frame.paste(resource.convert('RGB'),where)
                record={'kind':'sdl','checkpoints':[],'inputs':[]}
                class Capture:
                    artifacts=d;process=type('P',(),{'pid':123})()
                    def key(self,key,**options):calls.append(key)
                    def wait(self,*a,**k):pass
                def menu_picture(ids):
                    frame=image.copy();at=m.screen(p['source'],m.SAVE_AT)
                    for offset,index in zip(((0,-20),(-24,2),(24,2),(0,24)),ids):
                        tile=p['assets']['commands'][index]
                        frame.paste(tile.convert('RGB'),(at[0]+offset[0],at[1]+offset[1]),tile.point(lambda c:255 if c else 0,mode='L'))
                    return frame
                def cp(capture,name,predicate,points,**kwargs):
                    calls.append(('CHECK',name))
                    if failure=='cancel' and name=='quake-checked-mode4-cancel':raise RuntimeError('business cancel checkpoint')
                    if name=='quake-checked-mode4-cancel':
                        self.assertIsNone(m.command(list_frame,p['source'],p['command_actor'],0,1,[0,4,6,9]))
                        self.assertIsNotNone(predicate(list_frame))
                    if name=='quake-MP261-detail':
                        frame=image.copy();templates,_=p['postcast_detail']
                        for key in ('unselected','actor_name','current_max_hp','current_max_mp','level','experience',
                            'attack','defense','accuracy','evasion','dexterity','movement','status_25','status_26','status_27','hp_bar','mp_bar'):
                            at,tile=templates[key]
                            tile=tile.copy();tile.putpalette(p['visual_model']['palettes'][0]);frame.paste(tile.convert('RGB'),at)
                        self.assertIsNotNone(predicate(frame))
                        wrong=frame.copy();at,tile=p['list_upper']['current_max_mp'];wrong.paste(tile.convert('RGB'),at)
                        self.assertIsNone(predicate(wrong),'oldMP341 cannot pass MP261 gate')
                    if name=='quake-save-disabled':
                        self.assertIsNotNone(predicate(menu_picture((37,41,42,45))))
                        self.assertIsNone(predicate(menu_picture((37,39,42,45))),'enabled Save is a source counterexample')
                        if failure=='disk':save.write_bytes(save.read_bytes()[:-1]+b'X')
                    return image,{}
                @contextmanager
                def series(capture,record):
                    record['detail_series']=CompletedTapTest.series([CompletedTapTest.row(0,30000,30000)])
                    if failure=='clock-gate':record['detail_series']['summary']['qpc_frequency']=9999999
                    record['detail_series']['checkpoint_PNGs_bound_to_closed_series']=True
                    yield scope
                def series_cp(capture,s,name,predicate,points,**kwargs):
                    if 'return' in name:s['after']={'index':0}
                    return image,{}
                def navigation(capture,p,source,start,end,cp,prefix):
                    if prefix.startswith(('quake-target','quake-back','quake-to-system')):
                        self.assertEqual(source[m.UNITS+5]&128,128)
                        self.assertEqual(struct.unpack_from('<H',source,m.UNITS+68)[0],261)
                    calls.append(('NAV',start,end))
                def observed(*args,**kwargs):return {'ok':failure!='feedback' and args[3]=='hit'}
                with patch.object(m,'wait_state',cp),patch.object(m,'wait_title'),patch.object(m,'navigate',navigation),\
                     patch.object(m,'detail_series',series),patch.object(m,'series_checkpoint',series_cp),\
                     patch.object(m,'qpc_stamp',side_effect=[{'qpc':10000,'qpc_frequency':10000000},
                        {'qpc':20000,'qpc_frequency':10000000}]),patch.object(m,'feedback',side_effect=observed):
                    if failure=='cancel':
                        with self.assertRaisesRegex(RuntimeError,'business cancel'):m.exercise(Capture(),p,save,guard,record)
                    elif failure=='clock-gate':
                        with self.assertRaisesRegex(ValueError,'QPC频率不同'):m.exercise(Capture(),p,save,guard,record)
                        self.assertEqual(calls.count('ENTER'),7)
                    elif failure=='feedback':
                        with self.assertRaisesRegex(ValueError,'不发送Save'):m.exercise(Capture(),p,save,guard,record)
                        self.assertNotIn(('NAV',m.CASTER,m.TARGET),calls)
                    elif failure=='disk':
                        with self.assertRaises(AssertionError):m.exercise(Capture(),p,save,guard,record)
                    else:
                        m.exercise(Capture(),p,save,guard,record)
                        self.assertEqual(save.read_bytes(),self.p['encoded'])
                        self.assertEqual(save.stat().st_mtime_ns,guard[1])
                        keys=[r for r in calls if isinstance(r,str)]
                        self.assertEqual(keys.count('ENTER'),9)
                        self.assertEqual(keys.count('ESC'),5)
                        self.assertEqual(keys[-4:],['ENTER','ENTER','ESC','ESC'])
                        self.assertEqual(keys.count('DOWN'),6)
                        at=keys.index('ESC');self.assertEqual(keys[at+1:at+5],['DOWN','DOWN','DOWN','ENTER'])
                        names=[r[1] for r in calls if isinstance(r,tuple) and r[0]=='CHECK']
                        self.assertEqual(names[-3:],['quake-save-disabled','quake-secondary-cancel-parent','quake-parent-cancel-browse'])
                        self.assertEqual(record['observed_postcast']['outcome'],'hit')
                        self.assertEqual(record['no_write_storage_check']['Save_count'],0)
                        self.assertNotIn('completed_save',record)
                        self.assertNotIn('state_comparison',record)
                        self.assertFalse(record['no_write_storage_check']['persisted_state_measured'])
                        self.assertFalse(record['checked_cancel']['persisted_state_measured'])
                        self.assertFalse(record['checked_cancel']['raw_cancel_write_set_measured'])
                        self.assertNotIn('later_fullSave_checks_inmemory_business_writes',record['checked_cancel'])

    def test_reference_late_error_does_not_replace_business_and_cleanup(self):
        with tempfile.TemporaryDirectory() as directory:
            base=Path(directory);install=base/'install';install.mkdir();output_root=base/'outputs';output_root.mkdir()
            for name in ('dosbox-x.exe','app.exe','driver.exe'):(install/name).write_bytes(name.encode())
            bootstrap=base/'bootstrap.conf';bootstrap.write_text('[cpu]\ncore=normal\n')
            config=base/'actual.conf';config.write_text('[cpu]\ncycles=fixed 20000\n')
            events=[];verify_calls=[]
            @contextmanager
            def prior_running(*unused,**options):
                events.append('started')
                try:yield object()
                finally:events.append('cleaned')
            def fake_build(original,dosbox,configs,overrides,bootstrap):
                return {'schema':'test-only','original':str(original),'dosbox':{'path':str(dosbox)},
                    'configs':[{'path':str(c.resolve()),'sha256':m.fact(c)[1]['sha256'],'size':c.stat().st_size} for c in configs]}
            def verify(path):
                verify_calls.append(str(path))
                if len(verify_calls)==3:raise ValueError('late-config-guard')
            def pair(args):
                work=args.artifacts/'original';work.mkdir(parents=True)
                with m.host.running([str(install/'dosbox-x.exe'),'-conf',str(config)],work,{},work/'game.log',install/'driver.exe',[]):
                    raise RuntimeError('first-business-error')
            a=argparse.Namespace(root=ROOT,artifacts=output_root/'fresh',output_root=output_root,
                bootstrap=bootstrap,g1_source=G1_SOURCE,
                scenario=m.SCENARIO,run=True,sdl=install/'app.exe',driver=install/'driver.exe',dosbox=install/'dosbox-x.exe')
            with patch.object(m.host,'run',side_effect=pair),patch.object(m.host,'running',prior_running),\
                 patch.object(m.original_reference,'build',side_effect=fake_build),patch.object(m.original_reference,'verify',side_effect=verify):
                report=m.run(a)
            self.assertEqual(report['error']['message'],'first-business-error')
            self.assertEqual(report['reference_late_errors'],['late-config-guard'])
            self.assertEqual(events,['started','cleaned'])
            self.assertEqual(len(verify_calls),3)
            self.assertFalse(report['ok'])

    def test_compound_prepare_labels_models_and_never_creates_expected_Save(self):
        with tempfile.TemporaryDirectory() as directory:
            base=Path(directory);bootstrap=base/'bootstrap.conf';bootstrap.write_text('[cpu]\ncore=normal\n')
            a=argparse.Namespace(root=ROOT,artifacts=base/'fresh',output_root=base,
                bootstrap=bootstrap,g1_source=G1_SOURCE,scenario=m.SCENARIO,run=False,
                feedback_mode='compound-feedback',sdl=None,driver=None,dosbox=None)
            report=m.run(a)
            self.assertEqual(report['status'],'ready_not_executed');self.assertFalse(report['ok'])
            self.assertEqual(report['feedback_mode'],'compound-feedback')
            self.assertFalse(report['persisted_state_measured'])
            self.assertFalse(report['complete_compound_qualification_claimed'])
            self.assertTrue(report['external_original_C9_logical_join_required'])
            self.assertEqual([p.name for p in a.artifacts.rglob('*.SAV')],['controlled.SAV'])
            self.assertFalse(json.loads((a.artifacts/'postcast-models.json').read_text())['persisted_state_measured'])
            pair=json.loads((a.artifacts/'paired/comparison.json').read_text())
            self.assertFalse(pair['persisted_state_measured']);self.assertIn('no Save/END',pair['scope'])


class CompletedTapTest(unittest.TestCase):
    @staticmethod
    def row(index=0,timestamp=20001,poll=20000):
        return {'index':index,'pid':123,'hwnd':'0xabc','wgc_timestamp_100ns':timestamp,
            **dict(zip(('poll_started_qpc','poll_finished_qpc','arrival_qpc','copy_finished_qpc','png_finished_qpc'),
                range(poll,poll+5)))}

    @staticmethod
    def series(rows):
        return {'ready':{'pid':123,'hwnd':'0xabc'},'frames':rows,
            'summary':{'ok':True,'pid':123,'hwnd':'0xabc','frames':len(rows),'qpc_frequency':10000000}}

    def dispatch(self):
        return {'dispatch_start':{'qpc':10000,'qpc_frequency':10000000},
            'posted_end':{'qpc':20000,'qpc_frequency':10000000}}

    def test_exact_frame_boundary_and_future_stamp_pre_key_poll(self):
        for stamp,poll,accepted in ((20000,20000,False),(20001,19999,False),
                                   (20001,20000,True),(20001,20001,True)):
            with self.subTest(stamp=stamp,poll=poll):
                rows,audit=m.completed_tap_frames(self.series([self.row(0,stamp,poll)]),self.dispatch(),123,0)
                self.assertEqual(bool(rows),accepted)
                self.assertTrue(audit['SRT_and_poll_both_required'])

    def test_integer_cross_product_not_rounded_milliseconds(self):
        dispatch=self.dispatch()
        for v in dispatch.values():v['qpc_frequency']=3
        dispatch['dispatch_start']['qpc']=1000;dispatch['posted_end']['qpc']=2000
        for stamp,accepted in ((6666666666,False),(6666666667,True)):
            series=self.series([self.row(0,stamp,2000)]);series['summary']['qpc_frequency']=3
            rows,_=m.completed_tap_frames(series,dispatch,123,0)
            self.assertEqual(bool(rows),accepted)

    def test_missing_or_reordered_clock_is_rejected(self):
        changes=(('posted_end','qpc',-1),('posted_end','qpc',False),
                 ('posted_end','qpc_frequency',0),('posted_end','qpc_frequency',True),
                 ('posted_end','qpc_frequency',9999999),('dispatch_start','qpc',20001))
        for which,key,value in changes:
            with self.subTest(which=which,key=key,value=value):
                dispatch=self.dispatch();dispatch[which][key]=value
                with self.assertRaises(ValueError):m.completed_tap_frames(self.series([self.row()]),dispatch,123,0)

    def test_closed_owner_count_frequency_and_return_boundary(self):
        for section,key,value in (('summary','pid',124),('ready','pid',124),('summary','hwnd','0xdef'),
            ('summary','frames',2),('summary','qpc_frequency',10000001),('summary','ok',False)):
            with self.subTest(section=section,key=key):
                series=self.series([self.row()]);series[section][key]=value
                with self.assertRaises(ValueError):m.completed_tap_frames(series,self.dispatch(),123,0)
        for through in (-1,True,1):
            with self.assertRaises(ValueError):m.completed_tap_frames(self.series([self.row()]),self.dispatch(),123,through)

    def test_full_metrics_are_required_even_after_the_return_checkpoint(self):
        base=self.series([self.row(),self.row(1,20020,20010)])
        rows,audit=m.completed_tap_frames(base,self.dispatch(),123,0)
        self.assertEqual([r['index'] for r in rows],[0])
        self.assertEqual(audit['excluded'],[{'index':1,'reasons':['after_return_checkpoint']}])
        for key,value in (('pid',124),('hwnd','0xdef'),('index',2),('wgc_timestamp_100ns',20000),
            ('poll_started_qpc',20003),('poll_finished_qpc',20020),('arrival_qpc',None),
            ('copy_finished_qpc',True),('png_finished_qpc',20012)):
            with self.subTest(key=key,value=value):
                series=copy.deepcopy(base);series['frames'][1][key]=value
                with self.assertRaises(ValueError):m.completed_tap_frames(series,self.dispatch(),123,0)


if __name__=='__main__':unittest.main()
