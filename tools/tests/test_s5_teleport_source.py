"""S5 teleport差分协议必须核全页、严格前缀、commit写点和scope释放。"""
from pathlib import Path
import copy,hashlib,sys,unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'ida'))
import verify_s5_teleport as oracle

class TeleportProtocol(unittest.TestCase):
    def setUp(self):
        page=bytes(oracle.SIZE);vga=bytes(64000);self.rows=[];self.expected=[]
        tags=[('begin',0)]+[('sprite',i) for i in range(114,125)]+[('icon',124)]+[
            ('contract',i) for i in range(5,-1,-1)]+[('commit',0)]+[('wipe',i) for i in range(18)]+[
            ('expand',i) for i in range(10)]
        committed=False
        for tag,index in tags:
            committed |= tag=='commit';x,y=(11,11) if committed else (12,15)
            row={'tag':tag,'index':index,'x':x,'y':y,'page_hex':page.hex(),'vga_hex':vga.hex()}
            expected={'tag':tag,'index':index,'x':x,'y':y,
                'page_sha256':hashlib.sha256(page).hexdigest(),'vga_sha256':hashlib.sha256(vga).hexdigest()}
            if tag=='icon':row['base_hex']=page.hex();expected['base_sha256']=hashlib.sha256(page).hexdigest()
            self.rows.append(row);self.expected.append(expected)
        self.row={'frames':self.rows,'top':1,'sample':1,'uploads':45,'page_creates':1,'page_destroys':1,'logic_unchanged':True}
        self.source={'args':[11,11,11,11,4,0,1],'counts':{'wipe':18},'sample':1,'frames':self.expected}

    def test_full_ordered_prefixes_and_top_branch(self):
        self.assertEqual(oracle.validate_probe([self.row],[self.source])['ordered_prefixes'],48)

    def test_commit_is_not_early_or_delayed_and_same_size_pixels_are_checked(self):
        for index,key,value in ((0,'x',11),(19,'x',12),(0,'index',1),(0,'page_hex','01'+self.rows[0]['page_hex'][2:])):
            row=copy.deepcopy(self.row);row['frames'][index][key]=value
            with self.subTest(key=key,index=index),self.assertRaises(ValueError):oracle.validate_probe([row],[self.source])

    def test_missing_extra_or_reordered_prefix_cannot_pass(self):
        row=copy.deepcopy(self.row);row['frames'].pop()
        with self.assertRaises(ValueError):oracle.validate_probe([row],[self.source])
        row=copy.deepcopy(self.row);row['frames'][1],row['frames'][2]=row['frames'][2],row['frames'][1]
        with self.assertRaises(ValueError):oracle.validate_probe([row],[self.source])
        source=copy.deepcopy(self.source);source['frames'].pop()
        with self.assertRaises(ValueError):oracle.validate_probe([self.row],[source])

    def test_owner_branch_cleanup_and_icon_base_all_checked(self):
        for key,value in [('page_destroys',0),('page_creates',9),('uploads',51),('top',0),('sample',0),('logic_unchanged',1)]:
            row=copy.deepcopy(self.row);row[key]=value
            with self.assertRaises(ValueError):oracle.validate_probe([row],[self.source])
        row=copy.deepcopy(self.row);row['frames'][12]['base_hex']='01'+row['frames'][12]['base_hex'][2:]
        with self.assertRaises(ValueError):oracle.validate_probe([row],[self.source])

if __name__=='__main__':unittest.main()
