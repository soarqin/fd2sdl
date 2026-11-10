"""音乐CDB helper合成owner/PDB表达式mock；无attach、GUI或真实设备。"""
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import Mock,patch

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import fd2_cdb_bgm_snapshot as music


class CDBMusicTest(unittest.TestCase):
    def setup_owner(self,track=11,loop=1):
        widths=music.fields(8);at=0;offsets={}
        for name,size in widths.items():offsets[name]=at;at+=size
        layout={'address':0x100000,'size':2048,'pointer_size':8,'offsets':offsets,'sizes':widths,
            'anchors':{'driver':1024,'voices[0]':1200,'voices[1]':1600},'from_PDB_symbol_expressions':True}
        data=bytearray(2048);fdmus=(ROOT/'original_game/FDMUS.DAT').read_bytes()
        records=music.archive_entries(fdmus);raw=records[track];evnt=raw.index(b'EVNT')
        values={'current_track':track,'music_enabled':1,'fdmus.data':0x200000,'fdmus.size':len(fdmus),
            'fdmus.count':len(records),'driver.active_sequence':0x100000+1200,'voices[0].magic':0x46444247,
            'voices[0].track':track,'voices[0].loop_count':loop,'voices[0].driver':0x100000+1024,
            'voices[0].started':1,'voices[0].sequence.evnt':0x200000+music.archive_offsets(fdmus)[track]+evnt+8,
            'voices[0].sequence.size':struct.unpack_from('>I',raw,evnt+4)[0],'voices[0].sequence.cursor':10,
            'voices[0].sequence.loop_remaining':loop,'voices[0].sequence.status':4,
            'voices[0].sequence_volume':127,'voices[0].target_volume':127}
        for key,value in values.items():self.put(data,layout,key,value)
        return data,fdmus,layout

    def put(self,data,layout,key,value):
        at=layout['offsets'][key];size=layout['sizes'][key]
        data[at:at+size]=int(value).to_bytes(size,'little',signed=value<0)

    def decode(self,data,fdmus,layout,track=11,loop=1):
        return music.decode(bytes(data),fdmus,layout,ROOT/'original_game',track,loop)

    def test_playing_loop_and_real_evnt_resource_but_no_acoustic_claim(self):
        for track,loop in ((11,1),(11,0),(16,1),(18,0)):
            data,fdmus,layout=self.setup_owner(track,loop);result=self.decode(data,fdmus,layout,track,loop)
            self.assertTrue(result['ok']);self.assertTrue(all(result['checks'].values()))
            self.assertFalse(result['scope']['acoustic_identity_validated'])
            self.assertFalse(result['scope']['all_target_threads_stopped_during_reads'])

    def test_current_track_alone_cannot_pass_actual_voice_or_loop_gate(self):
        for key,value in (('track',17),('magic',0),('driver',0x300000),('started',0),
            ('sequence.evnt',0x200000),('sequence.size',1),('sequence.cursor',0x100000),
            ('sequence.status',2),('loop_count',0),('sequence.loop_remaining',0),
            ('target_volume',0),('sequence_volume',128)):
            with self.subTest(key=key):
                data,fdmus,layout=self.setup_owner();self.put(data,layout,'voices[0].'+key,value)
                result=self.decode(data,fdmus,layout)
                self.assertFalse(result['ok']);self.assertTrue(result['checks']['current_track_matches'])

    def test_wrong_active_owner_resource_and_overlapping_pdb_layout_rejected(self):
        data,fdmus,layout=self.setup_owner();self.put(data,layout,'driver.active_sequence',0x300000)
        with self.assertRaisesRegex(ValueError,'active owner'):self.decode(data,fdmus,layout)
        data,fdmus,layout=self.setup_owner();bad=bytearray(fdmus);bad[-1]^=1
        with self.assertRaisesRegex(ValueError,'已证原资源'):self.decode(data,bytes(bad),layout)
        layout['offsets']['current_track']=layout['offsets']['music_enabled']
        with self.assertRaisesRegex(ValueError,'重叠'):self.decode(data,fdmus,layout)

    def test_pdb_layout_queries_use_exact_module_type_and_no_target_function_call(self):
        data,fdmus,layout=self.setup_owner();values=[layout['address'],layout['size'],8]
        for name in music.fields(8):values.extend([layout['sizes'][name],layout['offsets'][name]])
        values.extend(layout['anchors'].values())
        debugger=Mock();debugger.request.side_effect=[[f'unsigned __int64 0x{value:x}'] for value in values]
        actual=music.probe_layout(debugger,'fd2sdl')
        self.assertEqual(actual['offsets'],layout['offsets']);self.assertEqual(actual['anchors'],layout['anchors'])
        self.assertIn('fd2sdl!fd2_bgm_player',actual['owner_expression'])
        commands=[call.args[0] for call in debugger.request.call_args_list]
        self.assertTrue(all(command.startswith('?? ') for command in commands))
        self.assertFalse(any('(unsigned __int64)' in command for command in commands))
        self.assertFalse(any(token in '\n'.join(commands) for token in ('.call','fd2_bgm_current_track(','.writemem','g;')))
        with self.assertRaises(ValueError):music.probe_layout(debugger,'bad;g')

    def test_dump_uses_shared_single_owned_read_and_python_exclusive_file(self):
        with tempfile.TemporaryDirectory(prefix='g3-bgm-read-pure-') as tmp:
            target=Mock(pid=123);exe=Path(tmp)/'app.exe';output=Path(tmp)/'owner.bin';data=b'abcdef'
            receipt={'address':0x100000,'size':len(data),'sha256':music.sha(data),'single_ReadProcessMemory':True}
            with patch.object(music.cdb_protocol,'read_owned_memory',return_value=(data,receipt)) as read:
                actual,fact=music._dump(target,exe,output,0x100000,len(data))
                read.assert_called_once_with(target,0x100000,len(data),exe)
                self.assertEqual(actual,data);self.assertEqual(output.read_bytes(),data)
                self.assertEqual(fact['owned_read'],receipt)
                with self.assertRaises(ValueError):music._dump(target,exe,output,0x100000,len(data))
                self.assertEqual(read.call_count,1)
            with patch.object(music.cdb_protocol,'read_owned_memory',return_value=(data[:-1],receipt)):
                with self.assertRaisesRegex(ValueError,'长度/SHA'):music._dump(target,exe,Path(tmp)/'bad.bin',0x100000,len(data))
            self.assertFalse((Path(tmp)/'bad.bin').exists())


if __name__=='__main__':unittest.main()
