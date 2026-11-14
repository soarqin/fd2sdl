"""完整24618原码↔C私有探针协议：错像素/漏帧/owner、重复key和输出保护。"""
from pathlib import Path
import copy,hashlib,importlib.util,json,tempfile,unittest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('s5_transition',ROOT/'tools/ida/verify_s5_transition.py')
oracle=importlib.util.module_from_spec(spec);spec.loader.exec_module(oracle)

class TransitionProtocol(unittest.TestCase):
    def setUp(self):
        page=bytes(oracle.SIZE);vga=bytes(64000)
        self.row={'frames':[{'page_hex':page.hex(),'vga_hex':vga.hex()} for _ in range(9)],
            'terrain_calls':1,'uploads':9,'cues':1,'page_creates':1,'page_destroys':1,'logic_unchanged':True}
        self.original={'audio_enabled':1,'frames':[{
            'page_sha256':hashlib.sha256(page).hexdigest(),
            'vga_sha256':hashlib.sha256(vga).hexdigest()} for _ in range(9)]}

    def test_exact_full_page_and_VGA_protocol(self):
        self.assertEqual(oracle.validate_probe([self.row],[self.original])['page_frames'],9)
        silent=copy.deepcopy(self.row);silent['cues']=0
        original={**self.original,'audio_enabled':0}
        self.assertEqual(oracle.validate_probe([silent],[original])['vectors'],1)

    def test_no_count_owner_or_boolean_coercion(self):
        for key,value in [('terrain_calls',2),('uploads',8),('cues',0),('page_creates',9),
                ('page_destroys',0),('logic_unchanged',False),('logic_unchanged',1),('uploads',True)]:
            row=copy.deepcopy(self.row);row[key]=value
            with self.subTest(key=key,value=value),self.assertRaises(ValueError):
                oracle.validate_probe([row],[self.original])
        with self.assertRaises(ValueError):oracle.validate_probe([], [self.original])

    def test_missing_frame_extra_field_or_same_size_pixel_corruption(self):
        rows=[]
        row=copy.deepcopy(self.row);row['frames'].pop();rows.append(row)
        row=copy.deepcopy(self.row);row['frames'][0]['page_hex']='01'+row['frames'][0]['page_hex'][2:];rows.append(row)
        row=copy.deepcopy(self.row);row['frames'][8]['vga_hex']=row['frames'][8]['vga_hex'][:-2];rows.append(row)
        row=copy.deepcopy(self.row);row['frames'][0]['unchecked']='no';rows.append(row)
        row=copy.deepcopy(self.row);row['unchecked']='no';rows.append(row)
        for row in rows:
            with self.assertRaises(ValueError):oracle.validate_probe([row],[self.original])

    def test_duplicate_JSON_key_is_rejected(self):
        with self.assertRaises(ValueError):json.loads('{"uploads":8,"uploads":9}',object_pairs_hook=oracle.strict_object)

    def test_output_cannot_replace_input_existing_or_original_subtree(self):
        with tempfile.TemporaryDirectory() as temp:
            root=Path(temp);original=root/'source';original.mkdir()
            source=root/'domain.json';source.write_text('{}')
            output=root/'new.json';oracle.output_guard(output,original,[source])
            for rejected in (source,original/'new.json',oracle.ROOT/'src/new-s5-evidence.json'):
                with self.assertRaises(ValueError):oracle.output_guard(rejected,original,[source])
            output.write_text('{}')
            with self.assertRaises(ValueError):oracle.output_guard(output,original,[source])

if __name__=='__main__':unittest.main()
