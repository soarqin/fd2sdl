"""合成relocated RAM验证音频快照门；无进程、RPC、窗口或音频设备。"""
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import fd2_g3_audio_observation as audio
if __package__:
    from . import test_dosbox_snapshot as fixture
else:
    import test_dosbox_snapshot as fixture


class G3AudioTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        fixture.SnapshotTest.setUpClass()
        cls.fixture=fixture.SnapshotTest
        _,cls.tracks,_=audio.sources(ROOT/'original_game')

    def prepared(self,track=14,loop=0):
        memory=bytearray(self.fixture.memory);db=self.fixture.bases[2]
        resource=0x1b0000;sequence=0x1a0000;raw=self.tracks[track];evnt=raw.index(b'EVNT')
        memory[resource:resource+len(raw)]=raw
        for at,value in ((db+0x3ee0,resource),(db+0x3ed0,sequence),(sequence+4,4),
            (sequence+0x10,resource+evnt),(sequence+0x14,resource+evnt+8),
            (sequence+0x28,loop),(sequence+0x34,23),(sequence+0x38,127)):
            struct.pack_into('<I',memory,at,value)
        memory[db+0x1a11]=track;memory[db+0x3ef0]=1;memory[db+0x1e61]=1
        return memory,resource,sequence

    def observe(self,memory,track=14,loop=0):
        return audio.observe_music(bytes(memory),ROOT/'original_game',{'base':0x1500},track,loop)

    def test_real_source_fields_tracks_and_thirteen_voice_directory(self):
        _,_,source=audio.sources(ROOT/'original_game')
        self.assertEqual(len(source['ui_voices']),13)
        self.assertEqual([x['track'] for x in source['tracks']],list(audio.TRACKS))
        self.assertEqual([(x['track'],x['loop']) for x in source['g3_music_call_sites']],[(17,1),(11,1),(16,1),(11,0)])
        for track,loop in ((14,0),(17,1),(16,1),(11,1)):
            with self.subTest(track=track,loop=loop):
                memory,_,_=self.prepared(track,loop);result=self.observe(memory,track,loop)
                self.assertTrue(result['ok']);self.assertTrue(all(result['checks'].values()))
                self.assertFalse(result['scope']['acoustic_identity_claimed'])
                self.assertFalse(result['scope']['completed_loop_count_claimed'])

    def test_cached_track_alone_and_matching_track_elsewhere_are_insufficient(self):
        memory,resource,_=self.prepared();memory[resource+100]^=1
        # 正确曲目另在RAM中，不扫描或用该副本替代真实53EE0 owner。
        memory[0x1d0000:0x1d0000+len(self.tracks[14])]=self.tracks[14]
        result=self.observe(memory)
        self.assertFalse(result['ok']);self.assertTrue(result['checks']['requested_track_matches'])
        self.assertFalse(result['checks']['full_loaded_original_entry_matches'])
        memory,_,_=self.prepared();memory[self.fixture.bases[2]+0x1a11]=13
        self.assertFalse(self.observe(memory)['ok'])

    def test_loop_completed_status_cursor_pointer_and_muted_do_not_pass_playing_gate(self):
        for offset,value in ((4,2),(0x28,0),(0x10,0x1b0000),(0x14,0x1b0000),
                             (0x38,0),(0x34,128)):
            with self.subTest(offset=offset):
                memory,_,sequence=self.prepared(17,1);struct.pack_into('<I',memory,sequence+offset,value)
                self.assertFalse(self.observe(memory,17,1)['ok'])
        memory,_,_=self.prepared();memory[self.fixture.bases[2]+0x1e61]=0
        self.assertFalse(self.observe(memory)['ok'])

    def test_null_owner_changed_runtime_leaf_and_invalid_contract_rejected(self):
        memory,_,_=self.prepared();struct.pack_into('<I',memory,self.fixture.bases[2]+0x3ed0,0)
        with self.assertRaises(ValueError):self.observe(memory)
        memory,_,_=self.prepared();memory[self.fixture.bases[1]+0x44960-0x10000]^=1
        with self.assertRaisesRegex(ValueError,'原指令'):self.observe(memory)
        memory,_,_=self.prepared();memory[self.fixture.bases[1]+0x2a670-0x10000]^=1
        with self.assertRaisesRegex(ValueError,'原码合法重定位'):self.observe(memory)
        memory,_,_=self.prepared()
        for track,loop in ((12,0),(14,2),(True,0),(14,False)):
            with self.assertRaises(ValueError):self.observe(memory,track,loop)

    def test_ram_receipt_is_bound_but_offline_does_not_claim_tcp_owner_revalidation(self):
        memory,_,_=self.prepared();descriptor={'base':0x1500,'limit':0xffffffff,'type':18,'dpl':3,'flags':'11111'}
        receipt={'sha256':audio.sha(memory),'size':len(memory),'single_main_emulation_thread_command':True,
            'break_or_extra_window':False,'DS_descriptor_before':descriptor,'DS_descriptor_after':descriptor,
            'registers_before':{'CR0':1,'EFLAGS':0x202},'registers_after':{'CR0':1,'EFLAGS':0x202}}
        with tempfile.TemporaryDirectory(prefix='fd2-g3-audio-') as temp:
            ram=Path(temp)/'ram.bin';meta=Path(temp)/'receipt.json';ram.write_bytes(memory)
            meta.write_text(json.dumps(receipt),encoding='utf-8')
            result=audio.observe_snapshot(ROOT/'original_game',ram,meta,14,0)
            self.assertTrue(result['ok']);self.assertFalse(result['scope']['actual_RPC_owner_revalidated_offline'])
            receipt['sha256']='0'*64;meta.write_text(json.dumps(receipt),encoding='utf-8')
            with self.assertRaisesRegex(ValueError,'收据'):audio.observe_snapshot(ROOT/'original_game',ram,meta,14,0)


if __name__=='__main__':
    unittest.main()
