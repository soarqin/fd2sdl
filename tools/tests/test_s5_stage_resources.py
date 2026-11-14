"""S5资源读链反例；合成格式只测边界，真实原DAT另跑只读门。"""
from pathlib import Path
import struct
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'static_data'))
import s5_stage_resources as resources
import s5_stage_transitions as conversion


def sheet():
    part=b'\0'*9+struct.pack('<HH',2,1)+bytes([0x81,0,7])
    return bytes([2,1,1,0,0,0,0,0])+struct.pack('<II',16,32)+part+part


def archive_rows(count,raw=b'x'):
    return [(100+i*10,raw) for i in range(count)]


class S5ResourceTests(unittest.TestCase):
    def reference_reader(self):
        reader=resources.ResourceReferences.__new__(resources.ResourceReferences)
        reader.data={'FDOTHER.DAT':b'known fixture','FDTXT.DAT':b'known text','FDFIELD.DAT':b'field','FDSHAP.DAT':b'shape'}
        reader.entries={'FDOTHER.DAT':archive_rows(59,sheet()),'FDTXT.DAT':archive_rows(32,b'\x02\x00\xff\xff'),
            'FDFIELD.DAT':archive_rows(93),'FDSHAP.DAT':archive_rows(2)}
        reader.entries['FDTXT.DAT'][31]=(800,b'\x04\x00\x06\x00\xff\xff\xff\xff')
        reader.entries['FDFIELD.DAT'][91]=(1000,bytes(131))
        reader.sheets={}
        return reader

    def test_sheet_byte_count_and_literal_zero_remain_opaque_write(self):
        result=resources.sheet_directory(sheet())
        self.assertEqual(result['part_count'],2)
        self.assertEqual(result['frames'][0]['opaque_writes'],2)
        self.assertTrue(result['frames'][0]['RLE_verified'])

    def test_invalid_part_directory_truncated_RLE_and_cross_row_rejected(self):
        source=sheet()
        for bad in (source[:10],source[:12]+struct.pack('<I',16)+source[16:],
                    source[:-1],source[:29]+bytes([0x82,0,7])+source[32:]):
            with self.subTest(raw=bad),self.assertRaises(ValueError):resources.sheet_directory(bad)

    def test_frame_requires_live_unique_slot_and_in_range(self):
        programs=[
            [conversion.action('cin_sheet_blit',0,0,0,0,0,320)],
            [conversion.action('cin_sheet_close',0)],
            [conversion.action('cin_sheet_open',0,34),conversion.action('cin_sheet_open',0,34)],
            [conversion.action('cin_sheet_open',0,34),conversion.action('cin_sheet_blit',0,2,0,0,0,320)]]
        for program in programs:
            reader=self.reference_reader();flow=conversion.compile_program('stage.source.fixture',program)
            with self.subTest(program=program),self.assertRaises(ValueError):
                reader.references(20,{'definition_id':flow['id']},{flow['id']:flow},{})

    def test_resource30_is_explicit_rebind_not_hot_stage_next_write(self):
        reader=self.reference_reader()
        program=[conversion.action('text',0),conversion.action('next_stage',30),
                 conversion.action('credits_setup'),conversion.action('text',1)]
        f=conversion.compile_program('stage.source.fixture',program)
        refs=reader.references(29,{'definition_id':f['id']},{f['id']:f},{})
        text=[r for r in refs if r['kind']=='text_fragment']
        self.assertEqual([r['source_id'] for r in text],['text:030:00000','text:031:00001'])
        constructors=[r for r in refs if r['kind']=='terminal_constructor_resource']
        self.assertEqual([r['source']['entry'] for r in constructors],[90,91,92])
        self.assertTrue(all(r['hot_stage_not_assigned'] and not r['ordinary_battle_capability'] for r in constructors))
        wrong=conversion.compile_program('bad',[program[1],program[3]])
        with self.assertRaisesRegex(ValueError,'after resource rebinding'):
            reader.references(29,{'definition_id':'bad'},{'bad':wrong},{})

    def test_original_extra_archive_identity_cannot_be_dynamic_baseline(self):
        with patch.object(Path,'read_bytes',return_value=b'not original'):
            with self.assertRaisesRegex(ValueError,'original resource identity'):
                resources.ResourceReferences(Path('original'),{})

    def test_montage_literal_assets_nonsplit_markers_duration_and_unknown_pair_reject(self):
        raw=bytearray(sheet());raw[1]=0
        for offset in (16,32):raw[offset+4]=1;raw[offset+6]=2
        def reader():
            r=self.reference_reader()
            for name,count in (('FIGANI.DAT',8),('TAI.DAT',8),('BG.DAT',8)):
                r.data[name]=b'bounded original family fixture';r.entries[name]=archive_rows(count,bytes(raw))
            return r
        f=conversion.compile_program('stage.cinematic.ending',[conversion.action('cin_montage_open',0,1,4)])
        refs=reader().references(26,{'definition_id':f['id']},{f['id']:f},{})
        self.assertEqual(sum(r['kind']=='montage_attack' for r in refs),2)
        self.assertEqual([r['source']['entry'] for r in refs if r['kind']=='montage_terrain_overlay'],[4])
        for kind in ('split','marker','ticks'):
            bad=reader();attack=bytearray(raw)
            if kind=='split':attack[1]=1
            if kind=='marker':attack[20]=attack[36]=0
            if kind=='ticks':attack[22]=11
            bad.entries['FIGANI.DAT'][1]=(100,bytes(attack))
            with self.subTest(kind=kind),self.assertRaisesRegex(ValueError,'source montage'):
                bad.references(26,{'definition_id':f['id']},{f['id']:f},{})
        with patch.object(Path,'read_bytes',return_value=b'not original caller proof'):
            with self.assertRaisesRegex(ValueError,'original caller proof identity'):
                conversion.natural_montage_source(Path('wrong.json'),Path('wrong-readset.json'))


if __name__=='__main__':unittest.main()
