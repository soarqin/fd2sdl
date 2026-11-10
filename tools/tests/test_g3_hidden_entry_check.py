"""隐藏代表的原资源正负门及无写合同；纯合成画面/输入，无GUI。"""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

from PIL import Image

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_g3_hidden_entry_check as hidden


class HiddenEntryTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.prepared=hidden.prepare(ROOT)

    def frame(self,view):
        frame=Image.new('RGB',(320,200),(0,0,0));frame.paste(view.convert('RGB'),(4,4));return frame

    def menu(self):
        assets=self.prepared['assets'];service=assets['services'][5]
        frame=service['background'].copy();notice=hidden.pages.notice(assets,440,selector=5)
        words=notice['text'];frame.paste(words,(0,0),words.point(lambda x:255 if x else 0,mode='L'))
        portrait,xy,_=notice['portrait_frames'][0];frame.paste(portrait,xy)
        frame.paste(74,(201,169,305,189))
        for i in range(4):
            icon=service['commands'][2*i+(i==0)]
            frame.paste(icon,(201+26*i,169),icon.point(lambda value:255 if value else 0,mode='L'))
        return frame.convert('RGB'),notice

    def test_raw_stage_row_and_six_stock_prices_without_cold_json(self):
        manifest=self.prepared['manifest']
        self.assertEqual(manifest['stage_row_first3'],[0,0,84])
        self.assertEqual(manifest['stock'],[1,22,53,192,193,132])
        self.assertEqual(manifest['hidden_marker'],[14,14])
        self.assertEqual(manifest['hidden_bank'],63)
        self.assertEqual([x['key'] for x in manifest['input_chords_source_mapping_only'] if x['stage1_selector0_matches']],['SHIFT_F1'])
        self.assertEqual(sum(x['native_requested'] for x in manifest['input_chords_source_mapping_only']),1)
        row=bytes.fromhex(manifest['stage_row_hex'])
        self.assertFalse(hidden.matches_hidden_chord(row,1,'SHIFT_F1'))
        self.assertFalse(hidden.matches_hidden_chord(row,0,'F1'))

    def test_caption500_full_source_view_passes_and_regular_caption_cannot_pass(self):
        assets=self.prepared['assets'];state=self.prepared['state']
        views=self.prepared['source_views']['hidden-caption500']
        for view in views:self.assertIsNotNone(hidden.town_match(self.frame(view),views,5))
        regular=hidden.town_images(ROOT/'original_game',assets,state,0)[0]
        self.assertIsNone(hidden.town_match(self.frame(regular),views,5))
        wrong=views[0].copy();wrong.paste(regular.crop((244,162,306,188)),(244,162))
        self.assertIsNone(hidden.town_match(self.frame(wrong),views,5))

    def test_background_all_four_raw_icons_and_portrait_greeting_are_required(self):
        assets=self.prepared['assets'];frame,notice=self.menu()
        self.assertIsNotNone(hidden.hidden_menu_match(frame,assets,notice))
        bank=assets['services'][5]
        zero=None
        for icon in range(4):
            image=bank['commands'][2*icon+(icon==0)]
            for y in range(image.height):
                for x in range(image.width):
                    if image.getpixel((x,y))==0:zero=(201+26*icon+x,169+y);break
                if zero:break
            if zero:break
        self.assertIsNotNone(zero)
        wrong=frame.copy();wrong.putpixel(zero,(0,0,0))
        self.assertIsNotNone(hidden.pages.menu_match(wrong,assets,5,0,notice))
        self.assertIsNone(hidden.hidden_menu_match(wrong,assets,notice))  # 原透明角底74不能被黑0覆盖。
        wrong=frame.copy();wrong.putpixel((220,8),(255,0,255))
        self.assertIsNone(hidden.hidden_menu_match(wrong,assets,notice))
        wrong=frame.copy();wrong.putpixel((124,11),(255,0,255))
        self.assertIsNone(hidden.hidden_menu_match(wrong,assets,notice))

    def test_complete_product_page_rejects_one_price_pixel(self):
        page=self.prepared['source_views']['hidden-buy-six-products'][0]
        frame=Image.new('RGB',(320,200));frame.paste(page.convert('RGB'),(5,112))
        predicate,_=self.prepared['checks']['hidden-buy-six-products']
        self.assertIsNotNone(predicate(frame))
        frame.putpixel((114,131),(255,0,255));self.assertIsNone(predicate(frame))

    def test_browser_keys_and_whole_encoded_save_mtime_guard(self):
        class Capture:
            def __init__(self,save=None):self.keys=[];self.save=save
            def key(self,key):
                self.keys.append(key)
                if self.save is not None and key=='SHIFT_F1':
                    data=bytearray(self.save.read_bytes());data[100]^=1;self.save.write_bytes(data)
        with tempfile.TemporaryDirectory(prefix='g3-hidden-pure-') as tmp:
            save=Path(tmp)/'FD2.SAV';save.write_bytes(self.prepared['encoded']);guard=[save.read_bytes(),save.stat().st_mtime_ns]
            capture=Capture();record={'checkpoints':[]}
            with patch.object(hidden.service,'wait_title'),patch.object(hidden.service,'wait_state'):
                hidden.exercise(capture,self.prepared,save,guard,record)
                self.assertTrue(record['browser_sequence_completed'])
                self.assertEqual(capture.keys,['DOWN','ENTER','ENTER','SHIFT_F1','ENTER','ENTER','ESC','ESC','LEFT'])
                self.assertFalse(any('save' in step for step in self.prepared['steps']))
                with self.assertRaises(AssertionError):hidden.exercise(Capture(save),self.prepared,save,guard,{'checkpoints':[]})


if __name__=='__main__':unittest.main()
