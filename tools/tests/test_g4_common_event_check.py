"""G4 native机制夹具的来源、整档写集、页面负例；不启动窗口。"""
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

from PIL import Image

TOOLS=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(TOOLS))
import run_g4_common_event_check as check
ROOT=TOOLS.parent


class CommonEventNativeCheckTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.empty=check.prepare(ROOT,'child39-empty')
        cls.full=check.prepare(ROOT,'child39-full-cancel')

    def test_descriptor_source_and_no_production_binding(self):
        import json
        flow=json.loads((ROOT/'data/static/flows/common_event.json').read_text())
        self.assertFalse(flow.get('bindings'))
        for p in (self.empty,self.full):
            self.assertEqual(p['source'][check.base.UNITS+11*80+49:check.base.UNITS+11*80+52],bytes((2,39,0)))
            self.assertFalse(p['manifest']['natural_stage0_callback_claimed'])
            self.assertTrue(p['source_contract']['ok'])
            self.assertFalse(p['source_contract']['native_WGC_acceptance'])

    def test_empty_child_only_appends211_once(self):
        p=self.empty;start=check.base.UNITS
        before=p['source'][start:start+80];after=p['model']['immediate'][start:start+80]
        slots=[after[11+i*2] for i in range(8) if not after[10+i*2]&128]
        self.assertEqual(slots.count(211),1)
        inventory=check.inventory_append(before,211)[10:26]
        self.assertEqual(after[10:26],inventory)
        self.assertEqual(after[0x48:0x50],before[0x48:0x50])

    def test_death_audio_fixture_only_disables_declared_game_music(self):
        p=check.prepare(ROOT,'child39-empty',capture_defeat=True)
        self.assertEqual([i for i,(a,b) in enumerate(zip(self.empty['source'][:-4],p['source'][:-4])) if a!=b],
            [check.base.META+16])
        self.assertEqual(p['source'][check.base.META+16:check.base.META+18],bytes((0,1)))
        self.assertEqual(p['model']['final'][check.base.META+16],0)
        self.assertEqual(p['manifest']['death_audio_fixture'],
            {'enabled':True,'game_music':0,'game_SFX':1,'host_mixer_changed':False})

    def test_full_cancel_preserves_all_inventory_and_only_source_raw_changes(self):
        for p in (self.empty,self.full):
            self.assertTrue(all(check.base.UNITS<=int(row['offset'],16)<check.base.UNITS+12*80
                for row in p['manifest']['controlled_changes_before_checksum']))
            self.assertEqual(len(p['source']),0x59cb)
        start=check.base.UNITS
        self.assertEqual(self.full['source'][start+10:start+26],self.full['model']['immediate'][start+10:start+26])
        self.assertEqual([page['fragment'] for page in self.full['pages']],[432,433])

    def test_story11_is_real_simple_source_not_late_stage_or_dynamic_actor(self):
        p=self.empty;page=p['story']
        self.assertEqual((page['stage'],page['fragment'],page['portrait_id']),(0,11,0))
        self.assertEqual(page['speaker_argument'],1)
        self.assertEqual(page['speaker_source'],{'unit_id':0,'raw_actor':0,'source':'active','visible':True})
        self.assertEqual(p['manifest']['story_source']['tokens'],10)
        self.assertGreater(page['known_text_pixels'],100)
        self.assertIsNone(page['arrow_position'])

    def test_story_text_id_requires_real_record_and_uses_original_lookup_order(self):
        original=check.decode_save(ROOT/'original_game/START.SAV')
        with self.assertRaisesRegex(ValueError,'未定义低址'):
            check.story11(ROOT/'original_game',self.empty['assets'],original)
        state=bytearray(self.empty['source']);base=check.base
        # 第二个可见匹配不能抢首visible；全部hidden时取最后active，不借roster。
        state[base.UNITS+80+8]=1
        self.assertEqual(check.story_speaker(state,1)['unit_id'],0)
        state[base.UNITS+5]|=1
        self.assertEqual(check.story_speaker(state,1)['unit_id'],9)
        state[base.UNITS+80+5]|=1
        self.assertEqual(check.story_speaker(state,1)['raw_actor'],1)
        # active无匹配才读persistent，且保最后匹配。
        state[base.UNITS+8]=0;state[base.UNITS+80+8]=9
        state[0x8a3+7]=4;state[0x8a3+8]=1
        state[0x8a3+80+7]=9;state[0x8a3+80+8]=1
        self.assertEqual(check.story_speaker(state,1),
            {'unit_id':9,'raw_actor':None,'roster_index':1,'source':'persistent','visible':False})

    def test_story_focus_and_whole_roster_are_derived_from_valid_active_source(self):
        p=self.empty;base=check.base
        self.assertEqual(p['source'][base.UNITS+8],1)
        self.assertEqual(tuple(p['immediate'][base.META+5:base.META+7]),p['hero'])
        for at in ('immediate','final'):
            self.assertEqual(p['model'][at][0x8a3:base.UNITS],p['source'][0x8a3:base.UNITS])

    def test_story_requires_text_portrait_and_complete_source_border(self):
        p=self.empty;page=p['story'];assets=p['assets']
        image=Image.new('RGB',(320,200))
        image.paste(assets['dialog_top'].convert('RGB'),page['border_position'])
        image.paste(page['portraits'][0].convert('RGB'),page['portrait_position'])
        image.paste(page['text'].convert('RGB'),(0,0),page['text'].point(lambda n:255 if n else 0,mode='L'))
        self.assertIsNotNone(check.page_match(image,page,assets))
        wrong=image.copy()
        wrong.paste(check.portraits(ROOT/'original_game',1,assets['palette'],False)[0].convert('RGB'),
            page['portrait_position'])
        self.assertIsNone(check.page_match(wrong,page,assets))
        for point in ((15,9),(232,5),(5,2)):
            broken=image.copy();pixel=broken.getpixel(point)
            broken.putpixel(point,tuple(255-v for v in pixel))
            self.assertIsNone(check.page_match(broken,page,assets))

    def test_whole_save_gate_rejects_extra_child_write_and_parent_state_change(self):
        expected=self.empty['model']['final']
        self.assertTrue(check.base.compare_saves(expected,expected)['mask_equal'])
        for offset in (check.base.UNITS+15,0x30a3,0x8a3,check.base.META+10):
            broken=bytearray(expected);broken[offset]^=1;broken=check.base.checksum(broken)
            with self.assertRaises(AssertionError):check.base.compare_saves(expected,broken)

    def test_prepare_only_never_requests_window_or_display(self):
        with tempfile.TemporaryDirectory() as directory,patch.object(check,'running') as launch,patch.object(check,'windows_display_requirement') as display:
            output=Path(directory)/'prepared'
            result=check.run(SimpleNamespace(root=ROOT,artifacts=output,scenario='child39-full-cancel',run=False))
            self.assertEqual(result['status'],'prepared_only');self.assertFalse(result['ok'])
            self.assertEqual(check.decode_save(output/'controlled.SAV'),self.full['source'])
            launch.assert_not_called();display.assert_not_called()

    def test_original_output_rejected_before_write(self):
        with self.assertRaises(ValueError):
            check.run(SimpleNamespace(root=ROOT,artifacts=ROOT/'original_game',scenario='child39-empty',run=False))


if __name__=='__main__':unittest.main()
