"""190ac完整原源、callback表边界与冷等待拒绝；不执行窗口或Unicorn。"""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
from rebuild_fd2_analysis import parse_le,parse_objects,build_relbase_image,parse_fixups,apply_fixups
spec=importlib.util.spec_from_file_location('special_cell',ROOT/'tools/ida/verify_fd2_special_cell.py')
source=importlib.util.module_from_spec(spec);spec.loader.exec_module(source)


class SpecialCellSourceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        exe=ROOT/'original_game/FD2.EXE';data=exe.read_bytes();le=parse_le(data)
        objects=parse_objects(data,le)
        cls.image=bytes(apply_fixups(build_relbase_image(data,le,objects),parse_fixups(data,le,objects)))
        # 只构实际原字节的小型读集替身；真实Domain取证来自CLI的G1文件。
        cls.readset={'functions':[{'va':0x190ac,'end':0x1956b,'chunks':[
            {'start':0x190ac,'end':0x1956b,'raw_hex':cls.image[0x190ac:0x1956b].hex()}]}]}
        cls.cold=json.loads((ROOT/'data/static/flows/loot.json').read_text(encoding='utf-8'))

    def test_actual_source_and_ninety_callback_boundary(self):
        self.assertEqual(source.validate_source(self.image,self.readset)['va'],0x190ac)
        table=source.callback_table(self.image)
        self.assertEqual((table['count'],table['end'],table['slot0_target']),(90,'0x51cf9','0x34531'))
        self.assertTrue(source.verify_cold(self.cold)['original_200ms_before_close'])

    def test_wrong_image_missing_chunk_boundary_and_bytes_rejected(self):
        bad=bytearray(self.image);bad[0x190ac]^=1
        with self.assertRaisesRegex(ValueError,'镜像'):source.validate_source(bad,self.readset)
        for change in (lambda f:f.update(end=0x1956a),lambda f:f.update(chunks=[]),
            lambda f:f['chunks'][0].update(raw_hex='00')):
            readset=copy.deepcopy(self.readset);change(readset['functions'][0])
            with self.assertRaises(ValueError):source.validate_source(self.image,readset)
        bad=bytearray(self.image);bad[0x51cf9]=0
        with self.assertRaisesRegex(ValueError,'非指针'):source.callback_table(bad)

    def test_new_stage_gate_skipped_cue_or_close_before200_rejected(self):
        for choice in ('stage','cue','order','word'):
            cold=copy.deepcopy(self.cold);flow=cold['flows'][0];nodes={n['id']:n for n in flow['nodes']}
            if choice=='stage':flow['nodes'].append({'id':'preflight.cell.stage'})
            elif choice=='cue':nodes['cell.ask_chest']['on']['yes']='cell.yes.delay'
            elif choice=='order':nodes['cell.money_mode']['on']['no']='cell.callback.close'
            else:nodes['cell.callback']['op']='descriptor_next'
            with self.subTest(choice=choice),self.assertRaises(ValueError):source.verify_cold(cold)

    def test_stock_mode2_is_binding_fact_not_noop_or_dynamic_proof(self):
        audit=source.audit_bindings(ROOT/'original_game')
        self.assertEqual([(r['stage'],r['index'],r['mode'],r['callback_word'])
            for r in audit['nonordinary_metadata']],[(0,12,2,0),(1,12,2,0),(31,12,2,0)])
        self.assertTrue(all(not r['initial_positions'] and not r['dynamic_reachability_claimed']
            for r in audit['nonordinary_metadata']))
        for stage,index,mode,value,position in ((2,0,0,45,[14,25]),(6,4,1,5000,[17,10])):
            row=next(r for r in audit['initial_special_cells'] if r['stage']==stage and r['index']==index)
            self.assertEqual((row['mode'],row['value']),(mode,value));self.assertIn(position,row['initial_positions'])


if __name__=='__main__':unittest.main()
