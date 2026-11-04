"""F2关闭必须越过18409全部12相位；只重放资源，没有窗口或游戏进程。"""
import unittest
from unittest.mock import patch

from PIL import ImageChops

import test_original_magic_list as existing
from fd2_opening_oracle import portraits,raw_ui

probe = existing.probe


class DetailCloseTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        existing.HealTest.setUpClass()
        cls.fixture = existing.HealTest()
        cls.source,cls.assets,cls.templates = cls.fixture.source,cls.fixture.assets,cls.fixture.templates
        cls.original = probe.ROOT/"original_game"

    def closed(self):
        frame = self.fixture.closed_caster_frame()
        tile = self.assets["detail_close_terrain"]
        frame.paste(tile["image"].convert("RGB"),tile["position"])
        return frame

    def detail(self,portrait):
        frame = self.fixture.magic_detail_frame()
        sheet = probe.archive_entry((self.original/"FDOTHER.DAT").read_bytes(),5)
        def blit(index,x,y):
            image = raw_ui(sheet,index)
            image.putpalette(self.assets["palette"])
            frame.paste(image.convert("RGB"),(x,y))
        # 168b6(5,7,5,5)实际left框，后17eef在(8,10)复制80×80立绘。
        for index,x,y in ((1,5,7),(2,88,7),(3,5,90),(4,88,90),
                          (5,8,7),(6,72,7),(7,8,90),(8,72,90),
                          (14,5,10),(15,88,10),(16,5,74),(17,88,74)):
            blit(index,x,y)
        for x in (24,40,56):
            blit(9,x,7); blit(12,x,90)
        for y in (26,42):
            blit(10,5,y); blit(11,88,y)
        for row in range(5):
            for col in range(5):
                blit(13,8+col*16,10+row*16)
        frame.paste(portrait.convert("RGB"),(8,10))
        return frame

    @staticmethod
    def closing(page,background,phase):
        # 独立按182ad/18312/1839b的矩形式构造，不调用待测关闭门或SDL动画。
        frame = background.copy()
        left = 101-phase*16 if phase>=6 else 5
        if left<0:
            frame.paste(page.crop((5-left,7,91,93)),(0,7))
        else:
            frame.paste(page.crop((5,7,91,93)),(left,7))
        if phase<9:
            top = 55-phase*16 if phase>2 else 7
            if top<0:
                frame.paste(page.crop((92,7-top,315,93)),(92,0))
            else:
                frame.paste(page.crop((92,7,315,93)),(92,top))
        if phase<6:
            y = 94+phase*16
            frame.paste(page.crop((5,94,315,94+min(102,200-y))),(5,y))
        return frame

    def match(self,frame):
        return probe.caster_closed_browse(frame,self.source,self.assets,self.templates)

    def test_old_phase2_false_positive_and_all_twelve_closing_frames_rejected(self):
        background = self.closed()
        identity = self.source[probe.UNITS+probe.STRIDE+7]
        for portrait in portraits(self.original,identity,self.assets["palette"],False):
            page = self.detail(portrait)
            for phase in range(12):
                with self.subTest(portrait=portrait.tobytes()[:4],phase=phase):
                    frame = self.closing(page,background,phase)
                    self.assertIsNotNone(ImageChops.difference(frame,background).getbbox())
                    if phase==2:
                        # 旧门：cursor/acted完整露出、原位两框/下板消失，仍有y126..199面板。
                        found = probe.visual_focus(frame,self.assets,self.assets["heal_result"]["composites"])
                        x,y = self.assets["heal_result"]["caster_screen"]
                        self.assertEqual(found["focus_screen"],[x,y])
                        self.assertTrue(any(probe.match_indexed(frame,sprite,(x,y-6))["ok"]
                                            for sprite in self.assets["heal_result"]["acted"]))
                        self.assertIsNone(probe.PlayerOracle(self.source,self.assets,[],{}).detail(frame))
                        at,image = self.templates["unselected"]
                        self.assertIsNone(probe.exact_tile(frame,image,at))
                        self.assertIsNone(ImageChops.difference(frame.crop((5,126,315,200)),
                                                               page.crop((5,94,315,168))).getbbox())
                    self.assertIsNone(self.match(frame))
        restored = self.match(background)
        self.assertEqual(restored["restored_terrain"]["checked_pixels"],576)
        self.assertEqual(restored["restored_terrain"]["roi"],[4,28,24,24])

    def test_full_terrain_one_bad_pixel_or_acted_missing_cannot_close(self):
        frame = self.closed()
        self.assertIsNotNone(self.match(frame))
        for point in ((4,28),(10,51),(27,28),(27,51)):
            bad = frame.copy(); bad.putpixel(point,(255,0,0))
            self.assertIsNone(self.match(bad))
        self.assertIsNone(self.match(self.fixture.closed_caster_frame(False)))

    def test_preflight_requires_static_dry_opaque_unoccupied_tiles(self):
        tile = self.assets["detail_close_terrain"]
        self.assertEqual((tile["world"],tile["tile"],tile["flags"],tile["overlays"]),([5,12],46,0,0))
        self.assertFalse(tile["actor_overlap"] or tile["water_palette_pixels"])
        source = bytearray(self.source)
        for raw,y in ((0,12),(1,13)):
            source[probe.UNITS+raw*probe.STRIDE:probe.UNITS+raw*probe.STRIDE+2] = bytes((5,y))
        with self.assertRaises(ValueError):
            probe.detail_close_terrain(self.original,source,self.assets)
        real_frame = probe.sheet_frame
        for index in (0,224):
            def unsafe(*args,**kwargs):
                image = real_frame(*args,**kwargs)
                image.putpixel((23,23),index)
                return image
            with self.subTest(index=index),patch.object(probe,"sheet_frame",side_effect=unsafe),self.assertRaises(ValueError):
                probe.detail_close_terrain(self.original,self.source,self.assets)
        field = (self.original/"FDFIELD.DAT").read_bytes()
        shape = (self.original/"FDSHAP.DAT").read_bytes()
        metadata = probe.archive_entry(field,1)
        real_entry = probe.archive_entry
        for dynamic in (True,False):
            def unsafe_entry(data,index):
                entry = bytearray(real_entry(data,index))
                if dynamic and data==shape and index==metadata[0]*2+1:
                    for tile_id in (46,146): entry[tile_id*4] = 8
                elif not dynamic and data==field and index==0:
                    width = int.from_bytes(entry[:2],"little")
                    for y in (12,13): entry[4+(y*width+5)*4+1] |= 0x10
                return bytes(entry)
            with self.subTest(dynamic=dynamic),patch.object(probe,"archive_entry",side_effect=unsafe_entry),self.assertRaises(ValueError):
                probe.detail_close_terrain(self.original,self.source,self.assets)


if __name__=="__main__":
    unittest.main()
