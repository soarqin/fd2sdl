"""S5来源叠加/NoTown/terminal拒绝门，不将结构门当实际运行。"""
import copy
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'static_data'))
import stage_release_scope as scope
import s2_source_disposition as disposition
import s5_stage_transitions as conversion
import verify_stage_release_traces as traces


def flow(identity,op='end'):
    return {'id':identity,'entry':'one','nodes':[{'id':'one','op':op,'args':[],'on':{}}]}


class S5ReleaseGatesTests(unittest.TestCase):
    def setUp(self):
        self.no_town=bytearray(30)
        for n in (22,23,24,27,28,29):self.no_town[n]=1
        self.notown={'source':{'canonical_va':'0x523e7','length':30,
            'sha256':scope.audit.digest(self.no_town)},'values':list(self.no_town)}
        self.town={'values':[list(bytes([i])*31) for i in range(1,27)]}

    def read_source(self,exe,address,length):
        if address==0x523e7 and length==30:return bytes(self.no_town)
        return bytes([(address-0x6238d)//31+1])*31

    def test_NoTown_before_town_table_for_22_24_and_27_29(self):
        for target in (22,23,24,27,28,29):
            with patch.object(scope.audit,'source_range',side_effect=self.read_source) as read:
                result=scope.preparation_handoff(target,b'original',self.town,self.notown)
            self.assertEqual(result['kind'],'NoTown');self.assertFalse(result['town_record_read'])
            read.assert_called_once_with(b'original',0x523e7,30)

    def test_real_town26_still_checks_exact31B_and_stale_NoTown_rejected(self):
        with patch.object(scope.audit,'source_range',side_effect=self.read_source):
            self.assertEqual(scope.preparation_handoff(26,b'original',self.town,self.notown)['kind'],'town')
            for mutate in (lambda d:d['values'].__setitem__(27,0),
                           lambda d:d['values'].__setitem__(27,2),
                           lambda d:d['source'].update(sha256='0'*64)):
                bad=copy.deepcopy(self.notown);mutate(bad)
                with self.subTest(bad=bad),self.assertRaisesRegex(ValueError,'NoTown'):
                    scope.preparation_handoff(27,b'original',self.town,bad)
            bad=copy.deepcopy(self.town);bad['values'][25][0]=0
            with self.assertRaisesRegex(ValueError,'preparation source'):
                scope.preparation_handoff(26,b'original',bad,self.notown)
            for target in (0,30,True):
                with self.assertRaisesRegex(ValueError,'outside ordinary'):
                    scope.preparation_handoff(target,b'original',self.town,self.notown)

    def test_terminal_stage30_not_released_as_normal_handoff(self):
        f=flow('v29');f['entry']='set';f['nodes']=[
            {'id':'set','op':'next_stage','args':[30],'on':{'next':'hold','error':'fail'}},
            {'id':'hold','op':'terminal_wait','args':[],'on':{}},
            {'id':'fail','op':'fail','args':[],'on':{}}]
        self.assertEqual(scope.next_stage_targets(f),set())
        f['entry']='choice';f['nodes'] += [
            {'id':'choice','op':'branch_item_occurrences','args':[],'on':{'yes':'normal','no':'set'}},
            {'id':'normal','op':'next_stage','args':[27],'on':{'next':'end'}},
            {'id':'end','op':'end','args':[],'on':{}}]
        self.assertEqual(scope.next_stage_targets(f),{27})

    def test_20_29_reuse_needs_exact_caller_and82_never_uses_old_ACK_probe(self):
        row={'slot':55,'definition':'common.loot.physical','caller_sources':['stage:25:drop:00000'],'blocker':None}
        binding={'bindings':{'stage.025.action.055':row['definition']}}
        self.assertEqual(list(traces.bound_callbacks(binding,[row]))[0][1],25)
        for identity in ('stage.024.action.055','stage.030.action.055','stage.029.action.082'):
            with self.subTest(identity=identity),self.assertRaises(ValueError):
                list(traces.bound_callbacks({'bindings':{identity:row['definition']}},[row]))

    def extension_fixture(self,partial=False):
        identity='dispatch:event_callback:082'
        rows=[{'source_id':f'old:{i}','target_va':hex(i),'disposition':'converted_candidate'} for i in range(207)]
        rows.append({'source_id':identity,'target_va':'0x362e8','disposition':'blocked',
                     'blocker':{'family':'old'},'source':{'raw_hex':'not overwritten'}})
        f=flow('common.callback.082')
        decisions=[{'source_id':identity,'target_va':'0x362e8','definition_id':f['id'],
            'definition_domain':'common_event','definition_sha256':'snapshot',
            'disposition':'partially_converted_candidate' if partial else 'converted_candidate'}]
        expected={('common_event',f['id']):f}
        return rows,decisions,expected

    def test_extension_preserves_old_source_and_partial_stays_blocked(self):
        for partial in (False,True):
            rows,decisions,expected=self.extension_fixture(partial)
            with patch.object(conversion,'validate_candidate',return_value=(decisions,expected,{})):
                updated,changes,_=disposition.apply_source_extension(rows,{},expected)
            row=updated['dispatch:event_callback:082']
            self.assertEqual(row['source'],{'raw_hex':'not overwritten'})
            self.assertEqual(row['S2_frozen_disposition'],'blocked')
            self.assertEqual(row['disposition'],'blocked' if partial else 'converted_candidate')
            self.assertFalse(row['S5_source_candidate']['actual_C_compared'])
            self.assertFalse(changes[0]['new_runtime_binding'])
            self.assertEqual(rows[-1]['disposition'],'blocked')

    def test_unknown_target_duplicate_source_and_missing_installed_helper_refuse(self):
        rows,decisions,expected=self.extension_fixture()
        for kind in ('target','already_closed','missing_definition','missing_helper'):
            source=copy.deepcopy(rows);catalog=copy.deepcopy(expected)
            if kind=='target':source[-1]['target_va']='0x11111'
            if kind=='already_closed':source[-1]['disposition']='converted_candidate'
            if kind=='missing_definition':catalog={}
            if kind=='missing_helper':
                expected['common_event','common.callback.082']['nodes'][0].update(op='call_teleport')
                catalog=copy.deepcopy(expected)
                expected['stage','stage.source.teleport']=flow('stage.source.teleport')
            with patch.object(conversion,'validate_candidate',return_value=(decisions,expected,{})):
                with self.subTest(kind=kind),self.assertRaises(ValueError):
                    disposition.apply_source_extension(source,{},catalog)


if __name__=='__main__':unittest.main()
