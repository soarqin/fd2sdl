"""S3正式败北协议纯测；不创建进程，不将mock当原生证据。"""
from pathlib import Path
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from native import s3_defeat as d
from native import s3_fixture as o

REAL_QUERY='field result query: stage=12 result=2 caller=player actor=0'
REAL_EXIT='field battle: defeat, stage=12'


def source():
    data=bytearray(0x59cb);data[o.META:o.META+10]=bytes((1,59,12,0,0,3,3,3,3,15))
    for raw in range(59):
        at=o.UNITS+raw*80;data[at:at+80]=bytes(80);data[at+6]=2 if raw==0 else 0
        data[at+7]=raw%32;struct.pack_into('<4H',data,at+64,100,100,20,20)
    return o.checksum(data)


class DefeatProtocolTests(unittest.TestCase):
    def test_derived_input_keeps_every_byte_except_twelve_hidden_flags_and_checksum(self):
        before=source();hidden,manifest=d.derive(before)
        self.assertEqual(manifest['raw_hidden'],list(range(15,27)))
        self.assertFalse(manifest['runtime_API_or_alias_used'])
        self.assertEqual(sum(row['offset']<len(before)-4 for row in manifest['plaintext_patches']),12)
        for i,(a,b) in enumerate(zip(before,hidden)):
            if i<len(before)-4:self.assertEqual(b,a|1 if i in {o.UNITS+80*j+5 for j in range(15,27)} else a)
        self.assertEqual(before,source())
        for at,value in ((o.META,6),(o.META+1,60),(o.META+2,2),(o.UNITS+5,128),(o.UNITS+15*80+5,1)):
            bad=bytearray(before);bad[at]=value
            with self.assertRaises(ValueError):d.derive(o.checksum(bad))
        bad=bytearray(before)
        for raw in range(59):bad[o.UNITS+80*raw+6]=2
        with self.assertRaisesRegex(ValueError,'可见敌军'):d.derive(o.checksum(bad))

    def test_protocol_uses_real_continue_then_declared_Load_and_controller_before_all_ACKs(self):
        for premature,tail_kind in ((False,None),(False,'capture_error'),(False,'ENTER'),(False,'ESC'),(False,'delivery'),(True,None)):
            with self.subTest(premature=premature,tail_kind=tail_kind),tempfile.TemporaryDirectory() as directory:
                work=Path(directory);artifacts=work/'phase1';artifacts.mkdir();save=work/'FD2.SAV'
                plain=source();save.write_bytes(o.encode(plain));guard=[save.read_bytes(),save.stat().st_mtime_ns]
                record={'kind':'sdl','inputs':[],'checkpoints':[]};events=[];keys=[];acks=[]
                log=artifacts/'game.log';log.write_text('')
                def key(value):
                    keys.append(value);record['inputs'].append({'key':value})
                    if acks and value=='ENTER':
                        ordinal=acks.pop()
                        if ordinal==1:log.write_text(REAL_QUERY+'\n'+REAL_EXIT+'\n')
                tail=[]
                if tail_kind=='capture_error':tail=[{'time':136923.312,'pid':40364,'capture_error':'Windows.Graphics.Capture failed: blank or nearly uniform captured client area','frame':'transition-blank-136923312000000.bmp'}]
                elif tail_kind in ('ENTER','ESC'):tail=[{'key':tail_kind,'delivery':{'kind':'key','sent':1}}]
                elif tail_kind=='delivery':tail=[{'delivery':{'kind':'key','sent':1}}]
                def observed_wait(*args,**kwargs):
                    events.append('wait')
                    if kwargs.get('blank_transition'):record['inputs'].extend(tail)
                capture=SimpleNamespace(artifacts=artifacts,process=object(),last=object(),key=key,wait=observed_wait)
                p={'original':work,'assets':{},'data':{},'titles':{3:['title0','title1','continue']}}
                pages=[{'fragment':10,'token_offset':i,'portrait_id':50} for i in range(2)]
                def cp(name,predicate,**options):
                    events.append(name)
                    if name.startswith('s3-stage12-defeat-story10-'):
                        acks.append(int(name.rsplit('-',1)[1]))
                        if premature:log.write_text(REAL_QUERY+'\n'+REAL_EXIT+'\n')
                    return None,{'frame':1}
                def load(cap,prepared,hidden,path,owner,subrecord,checkpoint):
                    events.append('actual_activity_Load')
                    self.assertEqual(o.decode_save(path),hidden);self.assertEqual(owner[0],path.read_bytes())
                    self.assertTrue(all(hidden[o.UNITS+i*80+5]&1 for i in range(15,27)))
                    subrecord['loaded_full_container_checked']=True
                    return hidden
                with patch.object(d,'wait_title'),patch.object(d.oracle,'pages',return_value=pages),\
                     patch.object(d,'save_checkpoint') as saved_title,patch.object(d,'result_templates',return_value={}),\
                     patch.object(d.natural,'actor_browse_anywhere',return_value=(d.derive(plain)[0],{})),\
                     patch.object(d.natural,'selection_summary',return_value={}),patch.object(d.natural,'range_probe',return_value=[{}]),\
                     patch.object(d,'wait_log',side_effect=lambda *a,**k:log.read_text()):
                    if premature:
                        with self.assertRaisesRegex(AssertionError,'故事返回前'):d.exercise(capture,p,plain,save,guard,record,cp,load_activity=load)
                    elif tail_kind in ('ENTER','ESC','delivery'):
                        with self.assertRaisesRegex(AssertionError,'story10后不得补键'):d.exercise(capture,p,plain,save,guard,record,cp,load_activity=load)
                    else:d.exercise(capture,p,plain,save,guard,record,cp,load_activity=load)
                self.assertEqual(keys[:3],['UP','ENTER','Z'])
                self.assertLess(events.index('s3-stage12-formal-continue'),events.index('actual_activity_Load'))
                self.assertLess(events.index('actual_activity_Load'),events.index('s3-stage12-defeat-real-selection'))
                self.assertTrue(record['defeat_active_load']['loaded_full_container_checked'])
                if premature:
                    self.assertNotIn('stage12_effect_result',record);self.assertNotIn('s3-stage12-defeat-game-over',events)
                elif tail_kind in ('ENTER','ESC','delivery'):
                    self.assertNotIn('stage12_effect_result',record);saved_title.assert_not_called()
                else:
                    self.assertEqual(keys,['UP','ENTER','Z','ENTER','ESC','ENTER','ENTER'])
                    self.assertEqual(record['stage12_effect_result']['post_story_inputs'],[])
                    self.assertEqual(len(record['stage12_effect_result']['story10_acknowledgements']),2)
                    self.assertTrue(record['stage12_effect_result']['returned_title_before_cleanup'])
                    saved_title.assert_called_once()
                    if tail_kind=='capture_error':self.assertIs(record['inputs'][-1],tail[0])

    def test_owned_save_boundary_rejects_before_any_UI_or_write(self):
        with tempfile.TemporaryDirectory() as directory:
            work=Path(directory);artifacts=work/'owned/phase1';artifacts.mkdir(parents=True)
            save=work/'FD2.SAV';save.write_bytes(o.encode(source()));before=save.read_bytes()
            capture=SimpleNamespace(artifacts=artifacts)
            with patch.object(d,'wait_title') as wait:
                with self.assertRaisesRegex(ValueError,'owned工作目录'):
                    d.exercise(capture,{},source(),save,[before,save.stat().st_mtime_ns],{},None,load_activity=None)
                wait.assert_not_called()
            self.assertEqual(save.read_bytes(),before)

    def test_real_logger_suffix_and_exact_completion_reject_wrong_or_duplicate_fields(self):
        self.assertEqual(d.QUERY,REAL_QUERY);self.assertEqual(d.EXIT,REAL_EXIT)
        good=REAL_QUERY+'\n'+REAL_EXIT+'\n';d._require_returned(good)
        for wrong in ('',REAL_QUERY+'\n',REAL_EXIT+'\n',
                good.replace(' caller=player actor=0',''),good.replace('caller=player','caller=ai'),
                good.replace('actor=0','actor=1'),good.replace('result=2','result=1'),
                REAL_QUERY+'\n'+good,good+REAL_EXIT+'\n',
                good+'field result query: stage=0 result=0 caller=player actor=0\n'):
            with self.subTest(log=wrong),self.assertRaises(AssertionError):d._require_returned(wrong)

    def test_real_early_query_or_exit_is_rejected_individually_before_story_ACK(self):
        with tempfile.TemporaryDirectory() as directory:
            work=Path(directory);log=work/'game.log';capture=SimpleNamespace(artifacts=work)
            log.write_text('ordinary field ready\n');d._not_returned(capture,'sdl')
            for text in (REAL_QUERY,REAL_EXIT,REAL_QUERY.replace('caller=player','caller=ai'),
                    'field result query: stage=12 result=2'):
                log.write_text(text+'\n')
                with self.subTest(log=text),self.assertRaisesRegex(AssertionError,'故事返回前'):d._not_returned(capture,'sdl')


if __name__=='__main__':unittest.main()
