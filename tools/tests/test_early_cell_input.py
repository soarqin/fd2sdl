"""早键输送合同；原版/SDL业务与WGC需独立实际运行。"""
from pathlib import Path
import sys
from types import SimpleNamespace
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from run_original_loot import early_cell_ack, run

class EarlyCellInputTest(unittest.TestCase):
    def capture(self, delta=180, overlap=False):
        rows=[]
        def key(name, *, fast=False):
            self.assertEqual(name,'ENTER');self.assertTrue(fast)
            down=1000 if not rows else 1000+delta
            rows.append({'key':name,'delivery':{'down_tick_ms':down,
                'up_tick_ms':down+(200 if overlap else 150)}})
        return SimpleNamespace(key=key,trace=rows)
    def test_exactly_two_independent_keys_with_no_late_ack_or_image_claim(self):
        capture=self.capture();report={};early_cell_ack(capture,report)
        self.assertEqual(len(capture.trace),2)
        self.assertEqual(report['early_ack']['host_down_delta_ms'],180)
        self.assertFalse(report['early_ack']['late_message_ack_sent'])
        self.assertFalse(report['early_ack']['late_422_wait_image_claimed'])
    def test_late_overlapping_or_backwards_delivery_never_claims_success(self):
        for delta,overlap in ((300,False),(350,False),(0,False),(-1,False),(180,True)):
            with self.subTest(delta=delta,overlap=overlap):
                capture=self.capture(delta,overlap);report={}
                with self.assertRaises(AssertionError):early_cell_ack(capture,report)
                self.assertEqual(len(capture.trace),2)
                self.assertNotIn('early_ack',report)
    def test_other_scenario_rejected_before_paths_or_launch(self):
        with self.assertRaises(ValueError):run(SimpleNamespace(early_ack=True,scenario='chest-swap'))

if __name__=='__main__':unittest.main()
