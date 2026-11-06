"""B6 SFX预声明与保守时点摘要；全部为静态/合成反例，不是实际音效通过。"""
import copy
import hashlib
from pathlib import Path
import subprocess
import sys
import unittest
from unittest.mock import patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import fd2_battle_animation_sfx as d
import fd2_sfx_oracle as sfx
from run_original_comparison import archive_entry
ROOT=Path(__file__).resolve().parents[2]
ORIGINAL=(ROOT/'original_game').resolve()


class DeclaredSFXTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.declarations={name:d.declare(ORIGINAL,name) for name in d.CORE_SCENARIOS}
    def observation(self,declaration,times=None):
        cues=declaration['expected_action_cue_subsequence']
        return {'schema':'fd2-sfx-observation-v1','fdother':{'sha256':declaration['source']['fdother_sha256']},
            'references':copy.deepcopy(declaration['references']),
            'queries':[{'name':cue['name'],'expected':cue['expected'],'identity_status':'supported_candidate',
                'onset_status':'observed_wav_position','candidates':[{**cue['expected'],'onset_ms':at}]} for cue,at in zip(cues,times or [100+200*i for i in range(len(cues))])]}
    def test_physical_real_figani_bank_marker_and_single_guaranteed_hit(self):
        decl=self.declarations['physical-full-hit'];clip=decl['metadata']['attack']
        self.assertEqual((clip['entry'],clip['bank_selector'],decl['metadata']['resolved_bank']),(1,5,52))
        cues=decl['expected_action_cue_subsequence'];self.assertEqual(len(cues),1)
        self.assertEqual((cues[0]['part'],cues[0]['effect_marker'],cues[0]['expected']),(6,1,{'bank':52,'sample':3}))
    def test_full0_real_fallback_cost_marker_then_seven_unconditional_phase4_cues(self):
        decl=self.declarations['player-magic0'];clip=decl['metadata']['casting'];cues=decl['expected_action_cue_subsequence']
        self.assertEqual((clip['entry'],clip['intro_end']), (28,11))
        self.assertEqual([(q['expected']['bank'],q['expected']['sample']) for q in cues],[(82,0)]+[(82,1)]*7)
        self.assertEqual(cues[0]['part'],6)
        self.assertEqual([q['main_tick_zero_based'] for q in cues[1:]],[3,5,7,9,11,13,15])
        self.assertTrue(decl['metadata']['cue_independent_of_hit'])
        self.assertGreater(cues[1]['source_duration_ms'],1200)
        self.assertEqual(len(decl['repeated_cues'][0]['names']),7)
    def test_heal_actual_tables_and_map_call_order(self):
        decl=self.declarations['player-heal13'];tables=decl['metadata']['map_effect_tables']
        self.assertEqual([tables[key]['id13_value'] for key in ('start','count','cue')],[57,7,12])
        self.assertEqual([q['expected'] for q in decl['expected_action_cue_subsequence']],
            [{'bank':80,'sample':11},{'bank':80,'sample':12},{'bank':80,'sample':1}])
        for row in tables.values():self.assertEqual(hashlib.sha256(bytes.fromhex(row['raw_hex'])).hexdigest(),row['sha256'])
    def test_all_complete_candidate_bank_bytes_and_identical_sample_aliases(self):
        original=(ORIGINAL/'FDOTHER.DAT').read_bytes();outer=sfx.archive_entries(original)
        for decl in self.declarations.values():
            self.assertEqual(len(decl['references']),24)
            for ref in decl['references']:
                raw=sfx.archive_entries(outer[ref['bank']])[ref['sample']]
                self.assertEqual(hashlib.sha256(raw).hexdigest(),ref['sha256']);self.assertEqual(len(raw),ref['source_frames'])
            groups=[row['aliases'] for row in decl['identical_voice_bytes']]
            self.assertIn([{'bank':80,'sample':0},{'bank':82,'sample':0}],groups)
            self.assertIn([{'bank':80,'sample':3},{'bank':80,'sample':4}],groups)
    def test_no_wave_fitted_windows_prefix_or_thresholds_and_existing_oracle_no_recording(self):
        for decl in self.declarations.values():
            self.assertIsNone(decl['search_windows']);self.assertIsNone(decl['reference_prefix_ms']);self.assertIsNone(decl['identity_thresholds'])
            windows={q['name']:(0,10000) for q in decl['expected_action_cue_subsequence']}
            request=d.request(decl,windows,min_score=.9,identity_margin=.1)
            self.assertTrue(all('reference_prefix_ms' not in row for row in request['queries']))
            with patch.object(subprocess,'Popen') as popen:
                report=sfx.analyze(ORIGINAL/'FDOTHER.DAT',request,None)
                popen.assert_not_called()
            self.assertEqual(report['status'],'pending_no_recording')
            self.assertEqual(d.order_observation(decl,report)['status'],'pending_capture_or_unique_cues')
    def test_request_rejects_missing_repeat_cue_invalid_window_or_threshold(self):
        decl=self.declarations['player-magic0'];windows={q['name']:(0,10000) for q in decl['expected_action_cue_subsequence']}
        missing=dict(windows);missing.pop(next(reversed(missing)))
        with self.assertRaises(ValueError):d.request(decl,missing,min_score=.9,identity_margin=.1)
        for bounds in ((1,1),(-1,3),(0,float('inf')),(0,)):
            invalid=dict(windows);invalid[next(iter(invalid))]=bounds
            with self.subTest(bounds=bounds),self.assertRaises(ValueError):d.request(decl,invalid,min_score=.9,identity_margin=.1)
        for threshold,margin in ((0,.1),(float('nan'),.1),(.9,-1),(.9,float('inf'))):
            with self.assertRaises(ValueError):d.request(decl,windows,min_score=threshold,identity_margin=margin)
    def test_synthetic_unique_heal_order_only_reports_relative_intervals_never_A7(self):
        decl=self.declarations['player-heal13'];report=self.observation(decl,[20,500,900]);result=d.order_observation(decl,report)
        self.assertEqual(result['status'],'observed_finite_recording_order_only');self.assertEqual(result['intervals_ms'],[480,400])
        self.assertFalse(result['ok']);self.assertFalse(result['full_A7_fidelity_pass'])
        self.assertEqual(result['native_absolute_input_latency'],'pending_no_first_packet_QPC')
    def test_equal_voice_bytes_never_infer_bank_and_repeated_peaks_remain_pending(self):
        decl=self.declarations['player-magic0'];report=self.observation(decl)
        for row in report['queries'][1:]:row['onset_status']='ambiguous_repeated_occurrence'
        result=d.order_observation(decl,report)
        self.assertEqual(result['cues'][0]['status'],'pending_identical_bytes_bank_or_sample')
        self.assertTrue(all(row['status']=='pending_identity_or_occurrence' for row in result['cues'][1:]))
        self.assertEqual(result['intervals_ms'],[])
    def test_one_peak_cannot_be_reused_for_duplicate_cues_and_no_auto_sort(self):
        decl=copy.deepcopy(self.declarations['physical-full-hit']);cue=copy.deepcopy(decl['expected_action_cue_subsequence'][0])
        cue['name']='attack-other-occurrence';decl['expected_action_cue_subsequence'].append(cue)
        for times in ([100,100],[200,100]):
            result=d.order_observation(decl,self.observation(decl,times))
            self.assertEqual(result['status'],'pending_reused_peak_or_wrong_order');self.assertEqual(result['intervals_ms'],[])
    def test_rejects_changed_readset_wrong_candidate_and_deleted_or_reordered_cues(self):
        decl=self.declarations['player-heal13']
        for change in ('reference','candidate','missing','order'):
            report=self.observation(decl)
            if change=='reference':report['references'][0]['sha256']='0'*64
            if change=='candidate':report['queries'][0]['candidates'][0]['sample']=0
            if change=='missing':report['queries'].pop()
            if change=='order':report['queries'].reverse()
            with self.subTest(change=change),self.assertRaises(ValueError):d.order_observation(decl,report)
    def test_unknown_scenario_never_generates_a_generic_audio_contract(self):
        for scene in ('ai-full-00','unknown'):
            with self.assertRaises(ValueError):d.declare(ORIGINAL,scene)

if __name__=='__main__':unittest.main()
