"""S5冷序/拒绝与端口合同；不冒称独立原码或实际C oracle。"""
import copy
from pathlib import Path
import sys
import unittest

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'static_data'))
import s5_stage_transitions as source


def walk(flow, choose=lambda n:False, limit=30000, flows=None):
    nodes={n['id']:n for n in flow['nodes']};pc=flow['entry'];observed=[]
    for _ in range(limit):
        n=nodes[pc];observed.append(n)
        if n['op']=='call_cinematic_card_first' and flows:
            observed.extend(walk(flows['stage.cinematic.card_first'],choose,limit,flows))
        if not n['on'] or set(n['on'])=={'error'}: return observed
        port=('yes' if choose(n) else 'no') if 'yes' in n['on'] else ('ack' if 'ack' in n['on'] else 'next')
        pc=n['on'][port]
    raise ValueError('test trace exceeded bound')


class S5ColdConversionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.image=bytes(0x60000)
        cls.programs=source.caller_programs(cls.image)
        cls.helpers={f['id']:f for f in source.cinematic_helpers(cls.image)}

    def test_all_13_sources_distinct_and_no_runtime_bindings_generated_by_programs(self):
        self.assertEqual(len(self.programs),13)
        self.assertEqual(sum(i.startswith('stage.source.entry.') for i in self.programs),5)
        self.assertEqual(sum(i.startswith('stage.source.victory.') for i in self.programs),7)
        self.assertIn('common.callback.082',self.programs)

    def test_HP_predicates_use_actor_HP_not_HP_MAX_and_flags_write_separate_enum(self):
        for identity,count in [('stage.source.entry.022',16),('stage.source.entry.027',20)]:
            branches=[n for n in self.programs[identity] if isinstance(n,dict)]
            self.assertEqual([n['args'] for n in branches],[[i,4,1,0] for i in range(count)])
            self.assertTrue(all(n['yes'][0][1][2]==4 for n in branches))

    def test_victory22_turn15_not_roster_count_and_v26_flash_only_raw1(self):
        graph=source.compile_program('stage.source.victory.022',self.programs['stage.source.victory.022'])
        turns=[n for n in graph['nodes'] if n['op']=='branch_turn']
        self.assertEqual([n['args'] for n in turns],[[2,15]])
        self.assertFalse(any(n['op']=='branch_roster_count' for n in graph['nodes']))
        graph=source.compile_program('stage.source.victory.026',self.programs['stage.source.victory.026'])
        self.assertEqual([n['args'] for n in graph['nodes'] if n['op']=='flash_actor'],[[1]])

    def test_shared_teleport_exact_frame_and_wait_domain(self):
        flow=source.teleport_helper()
        for top,delay in [(True,240),(False,300)]:
            nodes=walk(flow,lambda n:top)
            self.assertEqual([n['args'][0] for n in nodes if n['op']=='teleport_sprite'],list(range(114,125)))
            self.assertEqual(sum(n['args'][0] for n in nodes if n['op']=='delay_ticks'),23)
            self.assertEqual(sum(n['args'][0] for n in nodes if n['op']=='delay'),delay)
            self.assertEqual(sum(n['op']=='teleport_commit' for n in nodes),1)

    def test_callback82_no_state_increment_and_fourth_branch_uses_existing_source_contracts(self):
        flow=source.compile_program('common.callback.082',self.programs['common.callback.082'])
        nodes=walk(flow,lambda n:False)
        self.assertEqual(sum(n['op']=='call_teleport' for n in nodes),3)
        self.assertEqual([n['args'] for n in nodes if n['op']=='append_group_source'],[[5,0]])
        self.assertEqual([n['args'] for n in nodes if n['op']=='unit_ai_write_source'],[[6,0]])
        self.assertFalse(any(n['op']=='event_byte_write' for n in flow['nodes']))
        self.assertEqual(sum(n['op']=='call_teleport' for n in walk(flow,lambda n:True)),1)

    def test_credits_part_NO_stops_before_missing_part_instead_of_falling_through(self):
        for count in (0,1,254,255):
            flow=self.helpers['stage.cinematic.card_normal']
            nodes=walk(flow,lambda n:n['op']=='branch_cin_credits_part' and n['args'][0]<count,flows=self.helpers)
            self.assertEqual([n['args'][0] for n in nodes if n['op']=='cin_credits_part'],list(range(count)))
            self.assertEqual(sum(n['op']=='cin_credits_card_prepare' for n in nodes),1)

    def test_card_input_does_not_break_inner_frames_or_reset_blink(self):
        for frames,name in [(220,'normal'),(440,'last')]:
            flow=self.helpers['stage.cinematic.card_'+name]
            nodes=walk(flow,lambda n:n['op']=='branch_cin_input',flows=self.helpers)
            self.assertEqual([n['args'][0] for n in nodes if n['op']=='cin_credits_card'],list(range(frames)))
            self.assertEqual(sum(n['op']=='cin_credits_skip_set' and n['args']==[1] for n in nodes),frames)
            self.assertEqual(sum(n['op']=='cin_credits_skip_set' and n['args']==[0] for n in nodes),1)
            self.assertFalse(any(n['op']=='cin_credits_blink_reset' for n in nodes))
            intro=nodes[:next(i for i,n in enumerate(nodes) if n['op']=='cin_credits_intro_finish')]
            self.assertFalse(any(n['op'] in ('delay','delay_ticks','cin_present') for n in intro))

    def test_outer_count_descends_and_last_card_skip_repeats_last_only(self):
        flow=self.helpers['stage.cinematic.credits']
        for count in (0,1,2,32):
            nodes=walk(flow,lambda n:n['op']=='branch_cin_roster_count' and count>n['args'][0])
            actors=[n['args'][0] for n in nodes if n['op']=='cin_credits_actor']
            self.assertEqual(actors,list(range(count-1,-1,-1)))
            self.assertEqual(sum(n['op']=='cin_credits_blink_reset' for n in nodes),1)
        by_id={n['id']:n for n in flow['nodes']}
        queries=[n for n in flow['nodes'] if n['op']=='branch_cin_credits_skip']
        self.assertTrue(all(n['on']['yes']=='last_card' for n in queries))
        self.assertEqual(by_id['last_card']['op'],'cin_credits_epoch')

    def test_complete_pair_is_cold_two_RNG_with_explicit_ID_side_writes(self):
        ending=self.helpers['stage.cinematic.ending']
        self.assertFalse(any(n['op']=='cin_pair' for n in ending['nodes']))
        path=walk(ending)
        self.assertEqual(path[-1]['op'],'end')
        self.assertEqual(sum(n['op']=='cin_montage_open' for n in path),20)
        self.assertEqual(sum(n['op']=='branch_cin_montage_rng' for n in path),40)
        self.assertEqual(sum(n['op']=='call_cinematic_montage_round' for n in path),40)
        both=walk(ending,lambda n:n['op']=='branch_cin_montage_rng')
        self.assertEqual(sum(n['op']=='branch_cin_montage_rng' for n in both),40)
        self.assertEqual(sum(n['op']=='call_cinematic_montage_round' for n in both),60)
        pair=source.montage_pair(51,103,4)
        self.assertEqual(pair[:4],[source.action('unit_field_write',0,0,5,0,2),
            source.action('unit_field_write',0,0,6,0,51),source.action('unit_field_write',1,1,5,0,0),
            source.action('unit_field_write',1,1,6,0,103)])
        intro=pair[pair.index(source.action('call_fade_out'))+1:pair.index(source.action('cin_montage_intro_finish'))]
        self.assertEqual([n[1][0] for n in intro if n[0]=='cin_montage_intro'],list(range(8,-1,-1)))
        self.assertFalse(any(n[0] in ('cin_present','delay','delay_ticks') for n in intro))

    def test_montage_duration0_still_writes_HP_and_half1_returns_after_part_not_frame(self):
        flow=self.helpers['stage.cinematic.montage_round']
        def choice(half):
            return lambda n: {'branch_cin_montage_part':lambda: n['args'][0]<2,
                'branch_cin_montage_marker':lambda:n['args'][0]==0,
                'branch_cin_montage_last_marker':lambda:n['args'][0]==0,
                'branch_cin_montage_half':lambda:n['args']==[half],
                'branch_cin_montage_tick':lambda:n['args'][0]==1 and n['args'][1]<2}[n['op']]()
        first=walk(flow,choice(0))
        self.assertEqual([n['args'] for n in first if n['op']=='unit_field_write'],[[1,1,18,0,63680]])
        self.assertEqual([n['args'] for n in first if n['op']=='cin_montage_frame'],[[1],[1]])
        self.assertEqual([n['args'] for n in first if n['op']=='cin_montage_cue'],[[0],[1]])
        second=walk(flow,choice(1))
        self.assertEqual([n['args'] for n in second if n['op']=='unit_field_write'],[[0,0,18,0,63680]])
        self.assertEqual([n['args'] for n in second if n['op']=='cin_montage_cue'],[[0]])
        self.assertFalse(any(n['op'] in ('cin_montage_frame','delay_ticks') for n in second))
        for nodes in (first,second):
            ops=[n['op'] for n in nodes]
            self.assertLess(ops.index('unit_field_write'),ops.index('cin_montage_hit'))
            self.assertLess(ops.index('cin_montage_hit'),ops.index('cin_montage_cue'))

    def test_terminal_wait_never_next_even_with_complete_ending_source(self):
        for identity in ('stage.source.victory.026','stage.source.victory.029'):
            flow=source.compile_program(identity,self.programs[identity])
            self.assertTrue(all(n['on']=={} for n in flow['nodes'] if n['op']=='terminal_wait'))

    def test_victory29_resource30_setup_after_stage_write_and_before_new_text(self):
        nodes=walk(source.compile_program('stage.source.victory.029',self.programs['stage.source.victory.029']))
        stage=next(i for i,n in enumerate(nodes) if n['op']=='next_stage')
        setup=next(i for i,n in enumerate(nodes) if n['op']=='credits_setup')
        text0=next(i for i,n in enumerate(nodes) if n['op']=='text' and n['args']==[0])
        self.assertLess(stage,setup);self.assertLess(setup,text0)

    def test_victory28_all_three_original_pulse_calls_have_caller_delay300(self):
        # 完整2548c：25682/25694/256a6各call361b0；返回后各push12c/call3790a。
        nodes=walk(source.compile_program('stage.source.victory.028',self.programs['stage.source.victory.028']))
        waits=[i for i,n in enumerate(nodes) if n['op']=='delay' and n['args']==[300]]
        self.assertEqual(len(waits),3)
        for i in waits:
            self.assertEqual((nodes[i-1]['op'],nodes[i-1]['args']),('delay',[8]))
            self.assertEqual((nodes[i-2]['op'],nodes[i-2]['args']),('palette_add',[0,255,0]))
        self.assertEqual((nodes[waits[-1]+1]['op'],nodes[waits[-1]+1]['args']),('text',[15]))

    def test_manifest_preserves_old_ports_and_adds_only_fail_error_edges(self):
        graph=source.compile_program('stage.source.fixture',[source.action('delay',10)])
        document={'stage':{'operations':{'delay':{'args':[]},'end':{'args':[]},'fail':{'args':[]}}},
            'common_event':{'operations':{}},
            'manifest':{'flow_contracts':{'stage.json':{'ports':{'delay':[['next']], 'end':[[]], 'fail':[[]]}}}}}
        patch=source.operation_patch({'stage':[graph]},document,{'operations':{}})['stage']
        self.assertEqual(patch['manifest_patch']['ports']['delay'],[['error','next'],['next']])
        broken=copy.deepcopy(graph)
        next(n for n in broken['nodes'] if n['op']=='delay')['on']['error']='done'
        with self.assertRaisesRegex(ValueError,'only terminate at fail'):
            source.operation_patch({'stage':[broken]},document,{'operations':{}})

    def test_all_fixed_graphs_have_exact_nonoverlapping_ids_and_fail_edges(self):
        for flow in self.helpers.values():
            by_id={n['id']:n for n in flow['nodes']}
            self.assertEqual(len(by_id),len(flow['nodes']))
            self.assertLess(len(by_id),32767)
            for n in flow['nodes']:
                self.assertTrue(set(n['on'].values())<=by_id.keys())
                if 'error' in n['on']: self.assertEqual(by_id[n['on']['error']]['op'],'fail')


if __name__=='__main__': unittest.main()
