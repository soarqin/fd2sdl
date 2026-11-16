"""Flash source-relation gates only; no C, guest execution or WGC is run."""
import hashlib,json,sys,tempfile,unittest
from pathlib import Path
from PIL import Image
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import fd2_f4_flash as flash

class FlashRelationTest(unittest.TestCase):
    def setUp(self):
        self.addresses=list(range(1000,1120))
        self.model={'rules':[{'addresses':self.addresses,'effective_idle':0}], 'solid_RGB':[203,227,255]}
        self.a=bytes([12,34,56])*64000
        self.b=flash.solid_page(self.a,self.addresses,self.model['solid_RGB'])
    def samples(self,values):
        return [{'RGB':v,'index':i,'pid':123,'hwnd':456,'path':str(i),
            'wgc_timestamp_100ns':100+i,'sha256':str(i)}for i,v in enumerate(values)]
    def test_full_ABABA(self):
        result=flash.find_relation(self.samples([self.a,self.a,self.b,self.b,self.a,self.b,self.a]),self.model)
        self.assertEqual(result['witness_indices'],[0,2,4,5,6])
        self.assertTrue(result['whole_before_relation_qualified'])
        self.assertFalse(result['absolute_compositor_qualified'])
        self.assertFalse(result['exact_BIOS_wall_timing_qualified'])
    def test_changed_outside_mask_rejected(self):
        bad=bytearray(self.b);bad[0]=255
        with self.assertRaises(ValueError):flash.find_relation(self.samples([self.a,bytes(bad),self.a,bytes(bad),self.a]),self.model)
    def test_allRGB_channels_required(self):
        bad=bytearray(self.b);bad[self.addresses[0]*3+2]^=1
        with self.assertRaises(ValueError):flash.find_relation(self.samples([self.a,bytes(bad),self.a,bytes(bad),self.a]),self.model)
    def test_only_ABA_is_insufficient(self):
        with self.assertRaises(ValueError):flash.find_relation(self.samples([self.a,self.b,self.a]),self.model)
    def test_no_downgrade(self):
        with self.assertRaises(ValueError):flash.find_relation([],self.model,minimum_changes=2)
    def test_solid_before_not_vacuously_true(self):
        with self.assertRaises(ValueError):flash.find_relation(self.samples([self.b]*6),self.model)
    def test_cropped_page_rejected(self):
        with self.assertRaises(ValueError):flash.find_relation(self.samples([self.a[:-3]]),self.model)
    def test_source_opaque_zero_writes(self):
        #Independent source mask includes an ordinary store of color0, no nonzero-value gate.
        black=bytes(192000);result=flash.solid_page(black,[2],[203,227,255])
        self.assertEqual(result[6:9],bytes([203,227,255]))
        self.assertEqual(result[:6]+result[9:],black[:6]+black[9:])
    def test_addresses_boundary_and_duplicate(self):
        for addresses in ([],[2,2],[-1],[64000],[True]):
            with self.subTest(addresses=addresses),self.assertRaises(ValueError):flash.solid_page(self.a,addresses,[1,2,3])
    def test_phase_mask_equivalence_keeps_classes(self):
        self.model['rules'].append({'addresses':self.addresses,'effective_idle':2})
        result=flash.find_relation(self.samples([self.a,self.b,self.a,self.b,self.a]),self.model)
        self.assertEqual(result['effective_idle_classes'],[0,2])
    def rows(self,directory):
        result=[]
        for i in range(3):
            path=directory/(str(i)+'.png');Image.new('RGB',(320,200),(i,2,3)).save(path)
            result.append({'pid':123,'hwnd':456,'index':i,'wgc_timestamp_100ns':100+i,
                'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
        return result
    def test_read_closed_series_owner_and_exact_interval(self):
        with tempfile.TemporaryDirectory()as d:
            rows=self.rows(Path(d));samples=flash.read_series(rows,123,456,after_index=0,before_index=2)
            self.assertEqual([s['index']for s in samples],[1]);self.assertEqual(len(samples[0]['RGB']),192000)
    def test_read_closed_series_wrong_owner_or_order_rejected(self):
        with tempfile.TemporaryDirectory()as d:
            rows=self.rows(Path(d))
            for field,value in [('pid',999),('hwnd',999),('index',0),('wgc_timestamp_100ns',100)]:
                changed=[dict(r)for r in rows];changed[1][field]=value
                with self.subTest(field=field),self.assertRaises(ValueError):
                    flash.read_series(changed,123,456,after_index=0,before_index=2)
    def test_png_bytes_late_identity_rejected(self):
        with tempfile.TemporaryDirectory()as d:
            rows=self.rows(Path(d));Path(rows[1]['path']).write_bytes(b'changed')
            with self.assertRaises(ValueError):flash.read_series(rows,123,456,after_index=0,before_index=2)

if __name__=='__main__':unittest.main()
