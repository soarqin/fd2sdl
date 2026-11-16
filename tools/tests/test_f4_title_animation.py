from pathlib import Path
import importlib.util,hashlib,tempfile,unittest,zlib
from types import SimpleNamespace
P=Path(__file__).resolve().parents[1]/'verify_f4_title_animation.py'
s=importlib.util.spec_from_file_location('title_ani_reviewed',P);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)

class SourceContract(unittest.TestCase):
    def test_four_actual_caller_histories_are_bounded_and_not_all_skippable(self):
        rows=[list(m.schedule(i))for i in range(4)]
        self.assertEqual([len(r)for r in rows],[97,97,2,7])
        self.assertEqual(sum(map(len,rows)),203)
        self.assertEqual([r[-1]['source_result']for r in rows],[0,0,1,-2])
        self.assertEqual([r[-1]['clears']for r in rows],[97,97,1,6])
        self.assertEqual([r[-1]['queued']for r in rows],[0,0,1,0])
        self.assertEqual([r[-1]['host']for r in rows],[0,0,0,1])
        self.assertEqual([r[-1]['frame']for r in rows],[95,95,0,5])

    def test_invalid_typed_case_never_constructs_a_start(self):
        for bad in(-1,4,True,'0',0.0):
            with self.assertRaises(ValueError):list(m.schedule(bad))

    def test_unused_colours_still_have_a_full_base_owner_contract(self):
        page=bytes(64000);base=bytes(768);dac=bytes(768)
        expected=page+base+dac+bytes(65536)
        actual=bytearray(expected);actual[64003]=63
        self.assertEqual(m.rgb(expected[:64000],expected[64768:65536]),m.rgb(actual[:64000],actual[64768:65536]))
        delta=m.core.mismatch(expected,actual,64768)
        self.assertEqual((delta['field'],delta['offset']),('full_page',64003))

    def test_rgb_uses_dac_not_raw6_base_or_bgra_order(self):
        dac=bytes([1,2,3,7,11,13])+bytes(762)
        self.assertEqual(m.rgb(bytes([1,0]),dac),bytes([7,11,13,1,2,3]))
        self.assertEqual(m.expanded(bytes([0,16,63])*256),bytes([0,65,255])*256)
        with self.assertRaises(ValueError):m.expanded(bytes([64])*768)

    def test_successful_helper_fades_but_host_prefix_does_not(self):
        o=m.Original.__new__(m.Original);o.palettes=[bytes([17])*768]*100;o.pixels=lambda *args:bytes([1])*64000
        normal=list(m.schedule(0))[-1];host=list(m.schedule(3))[-1]
        n,_=o.expected(normal);h,_=o.expected(host)
        self.assertEqual(len(n),131072)
        self.assertEqual(n[64000:64768],bytes([17])*768)
        self.assertEqual(n[64768:65536],bytes(768))
        self.assertEqual(h[64768:65536],bytes([69])*768)

    def test_retained_page_corruption_is_rejected_by_independent_original_hash(self):
        with tempfile.TemporaryDirectory()as directory:
            p=Path(directory);page=bytes(64000);pal=bytes(768);raw=bytearray(page+pal+bytes(64768));raw[123]=1
            (p/'actual-00-000.zbin').write_bytes(zlib.compress(raw))
            o=m.Original.__new__(m.Original);o.args=SimpleNamespace(retained=p);o.guards={};o.cache={}
            o.hashes={0:{'pixels_sha256':m.sha(page),'temporary_palette_sha256':m.sha(pal)}}
            with self.assertRaisesRegex(ValueError,'original hash-bound'):o.pixels(0,0)

    def test_retained_temp_corruption_is_not_hidden_by_equal_page(self):
        with tempfile.TemporaryDirectory()as directory:
            p=Path(directory);page=bytes(64000);pal=bytes(768);raw=bytearray(page+pal+bytes(64768));raw[64000]=1
            (p/'actual-00-000.zbin').write_bytes(zlib.compress(raw))
            o=m.Original.__new__(m.Original);o.args=SimpleNamespace(retained=p);o.guards={};o.cache={}
            o.hashes={0:{'pixels_sha256':m.sha(page),'temporary_palette_sha256':m.sha(pal)}}
            with self.assertRaisesRegex(ValueError,'original hash-bound'):o.pixels(0,0)

    def test_new_output_cannot_enter_actual_original_directory_via_its_named_parent(self):
        with tempfile.TemporaryDirectory()as directory:
            root=Path(directory);(root/'original_game').mkdir();out=root/'original_game'/'new'
            with self.assertRaisesRegex(ValueError,'original resource'):m.core.checked_output(root,out,root)

if __name__=='__main__':unittest.main()
