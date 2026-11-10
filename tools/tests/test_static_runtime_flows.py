"""Round原双caller立即数/调用投影与冷顺序负例；无GUI/IDA执行。"""
from pathlib import Path
import copy
import json
import sys
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools/static_data'))
import verify_runtime_flows as verify


class RuntimeFlowTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe=(ROOT/'original_game/FD2.EXE').read_bytes()
        cls.doc=json.loads((ROOT/'data/static/flows/runtime.json').read_text(encoding='utf-8'))

    def test_two_source_callers_and_42_nodes(self):
        result=verify.verify(self.exe,self.doc)
        self.assertTrue(result['ok']);self.assertEqual(result['source_equal_nodes'],42)
        self.assertEqual(result['phase_source_equal_nodes'],132)
        self.assertFalse(result['original_machine_execution_claimed']);self.assertFalse(result['new_GUI_started'])
        self.assertEqual(result['defeat_source_equal_nodes'],86)
        self.assertEqual(result['detail_source_equal_nodes'],8)
        p=verify.program(self.exe,'load');self.assertEqual(p,verify.program(self.exe,'new_turn'))
        self.assertEqual([args[0] for op,args in p if op=='round_frame'],list(range(83,92)))
        self.assertEqual([args for op,args in p if op=='round_number'],[[171,91],[171,91]])
        self.assertEqual([args for op,args in p if op=='round_drop'],
            [[91,120,92,171,98],[91,120,97,171,103],[91,120,104,171,110],[91,120,169,171,175]])

    def test_defeat_original_rotations_raw_order_and_two_frame_halves(self):
        p=verify.defeat_program(self.exe,True)
        self.assertEqual([a[0] for op,a in p if op=='defeat_rotate_actors'],[i%4 for i in range(13)])
        self.assertEqual([a[0] for op,a in p if op=='defeat_frame'],list(range(68,80)))
        self.assertEqual(sum(op=='defeat_wait' for op,a in p),25)
        self.assertEqual(verify.defeat_program(self.exe,False),
            [('defeat_has_visible',[]),('defeat_mark_hidden',[]),('defeat_finish',[])])
        self.assertEqual(p[-3:],[('defeat_close',[]),('turn_refresh',[]),('defeat_finish',[])])
        for identity,label,change in (
            ('runtime.defeat','rotate.0.page',lambda n:n.update(op='defeat_rotate_actors',args=[0])),
            ('runtime.defeat','rotate.1.direction',lambda n:n.update(args=[0])),
            ('runtime.defeat','hidden',lambda n:n['on'].update(next='close')),
            ('runtime.defeat','dissolve.74',lambda n:n.update(args=[73])),
            ('runtime.defeat','dissolve.79.wait',lambda n:n.update(args=[0])),
            ('runtime.defeat','close',lambda n:n['on'].update(cancel='done')),
            ('runtime.detail','wait',lambda n:n['on'].update(next='close')),
            ('runtime.detail','wait_magic',lambda n:n['on'].update(cancel='done'))):
            with self.subTest(identity=identity,label=label):
                doc=copy.deepcopy(self.doc);flow=next(f for f in doc['flows'] if f['id']==identity)
                change(next(n for n in flow['nodes'] if n['id']==label))
                with self.assertRaises(ValueError):verify.verify(self.exe,doc)

    def test_detail_waits_come_from_both_original_call_paths(self):
        for magic in (False,True):
            p=verify.detail_program(self.exe,magic)
            self.assertEqual(sum(op=='detail_wait' for op,a in p),2 if magic else 1)
            self.assertEqual(p[-2:],[('detail_close',[]),('detail_done',[])])

    def test_defeat_and_detail_reject_truncated_or_changed_complete_source(self):
        original=verify.source_range
        for function,va in ((verify.defeat_program,0x1db65),(verify.detail_program,0x17aed)):
            function.cache_clear()
            def changed(exe,at,size):
                raw=original(exe,at,size)
                return raw[:-1]+bytes((raw[-1]^1,)) if at==va else raw
            try:
                with patch.object(verify,'source_range',side_effect=changed):
                    with self.assertRaisesRegex(ValueError,'完整函数'):function(self.exe,True)
            finally:function.cache_clear()

    def test_wrong_frame_wait_drop_coordinate_or_extra_ACK_rejected(self):
        for index in (0,1,23,30,31,40):
            with self.subTest(index=index):
                bad=copy.deepcopy(self.doc);node=bad['flows'][0]['nodes'][index]
                if node['args']:node['args'][0]+=1
                else:node['op']='round_wait_ms';node['args']=[1]
                with self.assertRaises(ValueError):verify.verify(self.exe,bad)
        bad=copy.deepcopy(self.doc);bad['flows'][0]['nodes'][0]['on']['ack']=bad['flows'][0]['nodes'][1]['id']
        with self.assertRaisesRegex(ValueError,'ACK'):verify.verify(self.exe,bad)

    def test_cycle_missing_terminal_and_unreachable_node_rejected(self):
        for mode in ('cycle','missing','extra'):
            with self.subTest(mode=mode):
                bad=copy.deepcopy(self.doc);flow=bad['flows'][0]
                if mode=='cycle':flow['nodes'][-1]['on']={'next':flow['entry']}
                elif mode=='missing':flow['nodes'].pop()
                else:flow['nodes'].append({'id':'unused','op':'round_finish','args':[],'on':{},'views':['present']})
                with self.assertRaises(ValueError):verify.verify(self.exe,bad)

    def test_original_hash_and_call_target_are_independent_of_JSON(self):
        bad=bytearray(self.exe);bad[0]^=1
        with self.assertRaisesRegex(ValueError,'EXE'):verify.verify(bytes(bad),self.doc)
        original=verify.source_range
        def corrupt(exe,va,size):
            raw=original(exe,va,size)
            return raw[:-1]+bytes([raw[-1]^1]) if va==0x104d6 else raw
        verify.program.cache_clear()
        try:
            with patch.object(verify,'source_range',side_effect=corrupt):
                with self.assertRaisesRegex(ValueError,'call'):verify.verify(self.exe,self.doc)
        finally:verify.program.cache_clear()

    def test_complete_phase_shared_source_geometry_and_waits(self):
        for enemy in (False,True):
            p=verify.phase_program(self.exe,enemy)
            self.assertEqual(p[0],('phase_parameters',[82 if enemy else 80,20 if enemy else 150]))
            self.assertEqual(len(p),131)
            self.assertEqual(sum(op=='round_wait_ticks' for op,args in p),45)
            self.assertEqual([a[0] for op,a in p if op=='phase_mosaic'],list(range(1,17))+list(range(17,0,-1)))
            self.assertEqual([a[0] for op,a in p if op=='phase_slide'],[100,75,50,25,0,1,0,0,25,50,75,100])
            self.assertEqual([op for op,a in p].count('phase_capture_prefix'),1)

    def test_phase_hold_palette_prefix_and_frame_mutations_rejected(self):
        for op in ('phase_parameters','phase_darkness','phase_capture_prefix','phase_slide'):
            with self.subTest(op=op):
                bad=copy.deepcopy(self.doc);node=next(n for n in bad['flows'][1]['nodes'] if n['op']==op)
                if op=='phase_parameters':node['args'][1]+=1
                elif op=='phase_capture_prefix':node['op']='round_map_background'
                else:node['args'][0]+=1
                with self.assertRaisesRegex(ValueError,'phase'):verify.verify(self.exe,bad)

    def test_turncycle_original_group_order_and_caller_boundaries(self):
        p=verify.turn_program(self.exe)
        self.assertEqual([args[0] for op,args in p['player_done'] if op=='turn_recovery_wait'],[1,0,0])
        self.assertEqual([op for op,args in p['side0_done']],
            ['turn_increment','turn_stop_if_different','turn_player_title','turn_display_mode',
             'turn_clear_acted','turn_refresh','turn_primary_music','turn_round','turn_focus_reset',
             'turn_enter_phase','turn_done'])
        for entry in p:
            bad=copy.deepcopy(self.doc);f=next(f for f in bad['flows'] if f['id']=='turncycle')
            n=next(n for n in f['nodes'] if n['id']==entry);n['op']='turn_focus_zero';n['args']=[]
            with self.assertRaisesRegex(ValueError,'turncycle'):verify.verify(self.exe,bad)

    def test_choice_wait_before_tick_cancel_and_close_are_source_order(self):
        nodes=verify.choice_program(self.exe)
        self.assertEqual([a[0] for _,op,a,_ in nodes if op=='choice_draw'],[4,8,12,16,12,8,4,0])
        for ident,port in [('wait','yes'),('read','ack'),('read','cancel'),
                ('field.begin','yes'),('field.copy','no'),('field.close','yes')]:
            bad=copy.deepcopy(self.doc);f=next(f for f in bad['flows'] if f['id']=='choice')
            n=next(n for n in f['nodes'] if n['id']==ident);n['on'][port]='tick'
            with self.assertRaisesRegex(ValueError,'choice'):verify.verify(self.exe,bad)


if __name__=='__main__':unittest.main()
