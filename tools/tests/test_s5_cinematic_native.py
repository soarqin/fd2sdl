"""S5两新族只读预检/来源与资源负例；不启动进程，不将合成帧当native。"""
from pathlib import Path
import hashlib
import sys
import tempfile
import unittest
from unittest.mock import Mock,patch
from PIL import Image

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_s5_cinematic_check as runner
from native import s5_cinematic_fixture as fixture
from native import s3_fixture as shared
from run_graphics_tests import decode_save


class CinematicNativeTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.p24=runner.prepare(ROOT,fixture.SCENARIOS[0]);cls.p20=runner.prepare(ROOT,fixture.SCENARIOS[1])

    def test_NoTown_true_selection_and_all_source_records(self):
        p=self.p24;original=decode_save(ROOT/'original_game/FD2.SAV');s=p['manual_state']
        self.assertEqual((s['stage'],s['count']),(24,17));self.assertEqual(p['data']['raw'][0x523e7+24],1)
        self.assertEqual(len(p['records']),62);self.assertEqual(len(p['pages']),25)
        self.assertEqual(s['roster'][17*80:],original[0x8a3+17*80:0x12a3])
        for row in p['manifest']['source_complete_roster_copies']:
            raw=s['roster'][row['target_raw']*80:(row['target_raw']+1)*80]
            self.assertEqual(raw,original[0x8a3+row['source_raw']*80:0x8a3+(row['source_raw']+1)*80])
        self.assertEqual({page['fragment'] for page in p['pages']},{1,2})
        active={u[8]:u[7] for u in p['records'] if not u[5]&1}
        for page in p['pages']:
            if page['speaker_control'] in (-17,-18):
                self.assertIn(page['speaker_argument'],active)
                self.assertEqual(page['portrait_id'],active[page['speaker_argument']])
            else:self.assertEqual(page['portrait_id'],p['records'][page['speaker_argument']][7])

    def test_six_presence_true_and_owned_unknown_slots(self):
        p=self.p20;records=p['records'];plain=decode_save(ROOT/'original_game/FD2.SAV')
        self.assertEqual(len(records),75);self.assertEqual(p['source'][shared.META:shared.META+3],bytes((1,75,20)))
        count=sum(any(not u[10+2*k]&128 and u[11+2*k]==item for k in range(8))
            for u in records[:16] for item in range(209,215))
        self.assertEqual(count,6);self.assertTrue(all(u[5]&1 for u in records if u[6]==0))
        for raw,offset in p['manifest']['constructed_unknown_holes_preserved_from_same_SAV_slot']:
            self.assertEqual(records[raw][offset],plain[shared.UNITS+raw*80+offset])
        self.assertEqual({page['fragment'] for page in p['pages']},{5,7,8,9,10})
        self.assertEqual([a['identity']['part'] for a in p['sheet_anchors']],[1,35,68,69,85,100])
        self.assertFalse(p['manifest']['natural_whole_stage_claimed'])

    def frames(self,directory,images):
        rows=[]
        for i,image in enumerate(images):
            path=Path(directory)/f'{i}.png';image.save(path)
            rows.append({'index':i,'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
                'wgc_timestamp_100ns':100000+i*330000})
        return rows

    def test_shake_needs_both_actual_full_resource_rows(self):
        anchors=self.p24['shake_anchors'];images=[]
        for phase in (0,1,0,1):
            image=Image.new('RGB',(320,200))
            for a in anchors:image.paste(a['images'][phase],tuple(a['position']))
            images.append(image)
        with tempfile.TemporaryDirectory() as d:
            rows=self.frames(d,images)
            self.assertTrue(runner.shake_gate(rows,anchors)['ok'])
            with self.assertRaises(AssertionError):runner.shake_gate(rows[:2],anchors)
            same=self.frames(d,[images[0]]*4)
            with self.assertRaises(AssertionError):runner.shake_gate(same,anchors)

    def test_101_resource_order_missing_and_corrupt_frame_refused(self):
        anchors=self.p20['sheet_anchors'];images=[]
        for a in anchors:
            image=Image.new('RGB',(320,200));image.paste(a['image'],a['at']);images.append(image)
        with tempfile.TemporaryDirectory() as d:
            rows=self.frames(d,images)
            self.assertTrue(runner.sheet_gate(rows,anchors)['ok'])
            with self.assertRaises(AssertionError):runner.sheet_gate(rows[:-1],anchors)
            with self.assertRaises(AssertionError):runner.sheet_gate(list(reversed(rows)),anchors)
            with self.assertRaises(AssertionError):runner.sheet_gate(self.frames(d,[Image.new('RGB',(320,200))]*6),anchors)

    def test_101_part35_opaque_top_cannot_be_replaced_by_previous_pose(self):
        anchors=self.p20['sheet_anchors'];images=[]
        for a in anchors:
            image=Image.new('RGB',(320,200));image.paste(a['image'],a['at']);images.append(image)
        a=anchors[1];x,y=156,30
        self.assertEqual(a['mask'].getpixel((x-a['at'][0],y-a['at'][1])),255)
        self.assertEqual(images[1].getpixel((x,y)),(252,252,252))
        parts,_=runner.animation.figani_parts((ROOT/'original_game/FDOTHER.DAT').read_bytes(),34)
        previous=Image.new('P',(320,200));mask=Image.new('L',(320,200))
        runner.animation._paste(previous,mask,parts[0]);runner.animation._paste(previous,mask,parts[34])
        previous.putpalette(self.p20['assets']['palette'])
        images[1].paste(previous.convert('RGB').crop((0,0,320,35)),(0,0))
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(AssertionError):runner.sheet_gate(self.frames(d,images),anchors)

    def test_victory20_sampling_density_changes_only_original_and_keeps_six_pose_gate(self):
        for kind,interval in (('original',16),('sdl',67)):
            with self.subTest(kind=kind):
                capture=Mock();record={'kind':kind};p=self.p20
                with patch.object(runner.shared,'checkpoint',return_value=lambda *a,**k:None),\
                     patch.object(runner,'wait_title'),patch.object(runner,'story_pages'),\
                     patch.object(runner.natural,'actor_browse_anywhere',return_value=(p['source'],None)),\
                     patch.object(runner.natural,'selection_summary'),patch.object(runner.natural,'range_probe'),\
                     patch.object(runner,'series',side_effect=StopIteration('before capture')) as sampled:
                    with self.assertRaises(StopIteration):runner.victory20(capture,p,None,None,record)
                sampled.assert_called_once_with(capture,record,interval_ms=interval)
                policy=record['sheet_sampling_policy']
                self.assertEqual((policy['max_ms'],policy['max_frames']),(45000,600))
                self.assertEqual(policy['ordered_parts'],[1,35,68,69,85,100])
                self.assertEqual(policy['max_channel_difference'],3)
                prior9=sum(page['fragment']==9 for page in p['pages'])-1
                self.assertEqual([c.args[0] for c in capture.key.call_args_list],
                    ['UP','ENTER','Z','ENTER','ESC']+['ENTER']*prior9)

    def test_unknown_scenario_rejected_before_source_read(self):
        with self.assertRaises(ValueError):fixture.derive(Path('absent-source'),'stage20-guess')


if __name__=='__main__':unittest.main()
