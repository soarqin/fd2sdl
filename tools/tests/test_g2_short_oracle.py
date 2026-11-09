"""SHORT真实来源、完整SAV、资源有序门、完整bank反候选；纯测不启动窗口。"""
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import wave

from PIL import Image

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import fd2_g2_short_oracle as oracle
import fd2_g2_battle_oracle as common


class ShortOracleTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.original=ROOT/'original_game'
        cls.encoded,cls.source,cls.manifest=oracle.derive_fixture(cls.original)
        cls.resources=oracle.visual_resources(cls.original,cls.source)
        cls.sound=oracle.sound_declaration(cls.original)

    def result(self,hp,engine='sdl'):
        data=bytearray(self.source);data[oracle.META]=2
        camera=self.source[oracle.META+3:oracle.META+5]
        data[oracle.META+5:oracle.META+9]=bytes((6,14,6-camera[0],14-camera[1]))
        ids=[]
        for raw in range(data[oracle.META+1]):
            start=oracle.UNITS+80*raw;data[start+3:start+5]=bytes(2);data[start+5]&=127
            if data[start+7] not in ids:ids.append(data[start+7])
            if engine=='original':data[start+2]=ids.index(data[start+7])
        struct.pack_into('<H',data,oracle.UNITS+64,hp)
        return common.checksum(data)

    def test_real_donor_weapon_pointer_and_nonlethal_domain(self):
        contract=self.manifest['resource_contract']
        self.assertEqual(contract['fx_sequence_va'],'0x619b6')
        self.assertEqual(contract['fx_sequence'],[[6,3],[7,3],[8,3],[9,255],[10,255],[11,255]])
        self.assertEqual(contract['finite_final_hp'],[8,14,20,25,31,42])
        self.assertEqual((contract['adjusted_attack'],contract['adjusted_defense']),(25,12))
        self.assertEqual(common.actor(self.source,11)[7:9],bytes((4,4)))
        self.assertEqual(common.actor(self.source,11)[52],0)
        self.assertEqual(common.actor(self.source,11)[53:55],bytes.fromhex(contract['donor_record_hex'])[53:55])
        self.assertEqual(common.actor(self.source,11)[59],0)
        self.assertEqual(common.actor(self.source,0)[64:68],struct.pack('<HH',42,42))
        self.assertEqual(common.crypt(self.encoded),self.source)

    def test_each_predeclared_final_hp_and_whole_container(self):
        for engine in ('original','sdl'):
            for hp in (8,14,20,25,31,42):
                result=oracle.check_result(self.original,self.source,self.result(hp,engine),engine=engine)
                self.assertTrue(result['ok'],result)
                self.assertEqual(result['checked_container_bytes'],22987)

    def test_unknown_hp_and_unrelated_writes_are_rejected(self):
        self.assertFalse(oracle.check_result(self.original,self.source,self.result(19),engine='sdl')['ok'])
        for at in (oracle.UNITS+68,oracle.UNITS+60,oracle.UNITS+11*80+11,0x900,oracle.META,oracle.META+5):
            with self.subTest(at=at):
                data=bytearray(self.result(31));data[at]^=1;data=common.checksum(data)
                self.assertFalse(oracle.check_result(self.original,self.source,data,engine='sdl')['ok'])

    def test_never_accept_a_different_fixture_or_endpoint(self):
        bad=bytearray(self.source);bad[oracle.UNITS+72]^=1
        with self.assertRaises(ValueError):oracle.check_result(self.original,common.checksum(bad),self.result(31),engine='sdl')
        with self.assertRaises(ValueError):oracle.check_result(self.original,self.source,self.result(31),engine='other')

    def write_frames(self,directory,phases):
        rows=[]
        for i,(phase,variant) in enumerate(phases):
            rule=next(r for r in self.resources[phase] if r.get('frame',r.get('width'))==variant)
            a=rule['anchor'];image=Image.new('RGB',(320,200),(31,37,41));image.paste(a['image'],a['at'],a['mask'])
            path=Path(directory)/f'frame{i:05}.png';image.save(path)
            rows.append({'index':i,'path':str(path),'sha256':common.digest(path.read_bytes())})
        return rows

    def test_visual_requires_ordered_panel_fx_and_lower_bar_for_hit(self):
        with tempfile.TemporaryDirectory(prefix='fd2-short-frames-') as directory:
            rows=self.write_frames(directory,[('panel',42),('solid',0),('fx',7),('bar',51)])
            self.assertTrue(oracle.check_visual(rows,self.resources,31)['ok'])
            self.assertFalse(oracle.check_visual(rows[:3],self.resources,31)['ok'])
            wrong=self.write_frames(directory,[('panel',42),('fx',7),('solid',0),('bar',51)])
            self.assertFalse(oracle.check_visual(wrong,self.resources,31)['ok'])
            initial=self.write_frames(directory,[('panel',42),('fx',7),('bar',70)])
            self.assertFalse(oracle.check_visual(initial,self.resources,31)['ok'])
            self.assertTrue(oracle.check_visual(initial,self.resources,42)['ok'])
            no_solid=self.write_frames(directory,[('panel',42),('fx',7),('bar',51)])
            self.assertFalse(oracle.check_visual(no_solid,self.resources,31)['ok'])
            false_miss=self.write_frames(directory,[('panel',42),('solid',0),('fx',7),('bar',70)])
            self.assertFalse(oracle.check_visual(false_miss,self.resources,42)['ok'])

    def test_solid_checks_all_original_opaque_points_including_fx_overlay(self):
        rules=self.resources['solid']
        self.assertEqual([r['sprite_rle_opaque_pixels'] for r in rules],[449,454,449])
        self.assertEqual([r['source_fx6_overlay_pixels'] for r in rules],[13,13,13])
        self.assertEqual([r['solid64_pixels'] for r in rules],[436,441,436])
        for rule in rules:
            self.assertEqual(rule['anchor']['checked_pixels'],rule['sprite_rle_opaque_pixels'])
        # 选三个真实pose共同的非FX写点，防止错误像素借其它pose透明区绕过。
        point=next((x,y) for y in range(7) for x in range(24)
            if all(r['source_mask'].getpixel((x,y)) for r in rules))
        origin=rules[0]['sprite_origin'];base_at=(origin[0]+point[0],origin[1]+point[1])
        # FX6首非零点原坐标(9,5)，相对actor原点(9,7)，这13点从不豁免。
        overlay_at=(rules[0]['fx6_origin'][0]+9,rules[0]['fx6_origin'][1]+5)
        palette=[v<<2 for v in oracle.archive_entry((self.original/'FDOTHER.DAT').read_bytes(),0)]
        rgb64=tuple(palette[64*3:64*3+3])
        for reason,at,value in (('missing64',base_at,(0,255,0)),
                ('wrongFX',overlay_at,(0,255,0)),('deletedFXpoint',overlay_at,rgb64)):
            with self.subTest(reason=reason),tempfile.TemporaryDirectory(prefix='fd2-short-solid-') as directory:
                rows=self.write_frames(directory,[('panel',42),('solid',0),('fx',7),('bar',51)])
                path=Path(rows[1]['path'])
                with Image.open(path) as image:changed=image.convert('RGB')
                changed.putpixel(at,value);changed.save(path);rows[1]['sha256']=common.digest(path.read_bytes())
                self.assertFalse(oracle.check_visual(rows,self.resources,31)['ok'])

    def test_solid_anchor_mask_cannot_drop_original_rle_overlay_points(self):
        resources=dict(self.resources);resources['solid']=[dict(r) for r in self.resources['solid']]
        first=resources['solid'][0];first['anchor']=dict(first['anchor'])
        first['anchor']['mask']=first['anchor']['mask'].copy()
        box=first['source_mask'].getbbox();first['anchor']['mask'].putpixel((9-box[0],7-box[1]),0)
        with self.assertRaisesRegex(ValueError,'不得删去原RLE写点'):
            oracle.check_visual([],resources,31)

    def test_visual_binds_original_png_identity(self):
        with tempfile.TemporaryDirectory(prefix='fd2-short-hash-') as directory:
            rows=self.write_frames(directory,[('panel',42),('fx',7),('bar',51)])
            Path(rows[0]['path']).write_bytes(b'wrong')
            with self.assertRaises(ValueError):oracle.check_visual(rows,self.resources,31)

    def recording(self,path,sample):
        import numpy as np
        from scipy import signal
        from fd2_sfx_oracle import references
        refs=references(self.original/'FDOTHER.DAT',self.sound['candidates'])
        ref=next(r for r in refs if r['sample']==sample)
        data=signal.resample_poly(ref['values'],4,1)
        data=np.r_[np.zeros(4410),data,np.zeros(4410)]
        stereo=np.repeat(np.rint(data*12000).astype('<i2')[:,None],2,axis=1)
        with wave.open(str(path),'wb') as out:
            out.setnchannels(2);out.setsampwidth(2);out.setframerate(44100);out.writeframes(stereo.tobytes())

    def test_sound_all_six_counter_candidates_and_fixed_prefix(self):
        self.assertEqual(len(self.sound['references']),6)
        self.assertEqual(self.sound['prefix_frames'],661)
        with tempfile.TemporaryDirectory(prefix='fd2-short-sfx-') as directory:
            path=Path(directory)/'recording.wav'
            for hp,cue in ((31,3),(42,4)):
                self.recording(path,cue)
                result=oracle.check_sound(self.original,self.sound,path,hp)
                self.assertTrue(result['ok'],result)
                self.assertEqual(result['expected_sample'],cue)
            self.recording(path,0)
            self.assertFalse(oracle.check_sound(self.original,self.sound,path,31)['ok'])
            changed=dict(self.sound,prefix_frames=500)
            with self.assertRaises(ValueError):oracle.check_sound(self.original,changed,path,31)


if __name__=='__main__':unittest.main()
