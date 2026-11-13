"""S3来源闭包的独立小例：不将生成结果当原版运行oracle。"""
import copy
from pathlib import Path
import sys
import unittest

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"tools/static_data"))
import stage_release_scope as scope


def node(identity,op,args=(),**edges):
    return {"id":identity,"op":op,"args":list(args),"on":edges}


class ReleaseScopeTests(unittest.TestCase):
    def test_capability_active96_and_separate_deployment20_bounds(self):
        flow={"id":"stage.capability.019","nodes":[node("cap","stage_capability",[1,83,0,20])]}
        self.assertEqual(scope.audit.capability(19,{flow["id"]:flow},{}),[1,83,0,20])
        for args in ([1,97,0,20],[1,0,97,20],[1,83,0,21],[1,True,0,0]):
            flow["nodes"][0]["args"]=args
            with self.subTest(args=args),self.assertRaises(ValueError):
                scope.audit.capability(19,{flow["id"]:flow},{})

    def test_stage_ranges_keep_opening_out_of_battle_tables(self):
        self.assertEqual(scope.stages("2:10,11:19,20"),list(range(2,21)))
        for value in ("31", "32", "4:2", "2:30", "2::3", "-1", ""):
            with self.subTest(value=value),self.assertRaises(ValueError):scope.stages(value)

    def test_complete_entry_counts_shared_init_extra_group_and_resource_tail(self):
        templates=[]
        for group in (0,0,1,1,1,0):
            raw=bytearray(26);raw[21]=group;templates.append(raw)
        meta={"templates":templates,"unknown_01":7,"unknown_02":5}
        init={"id":"stage.source.initialize","entry":"setup","nodes":[
            node("setup","stage_setup_source",next="zero"),
            node("zero","append_group",[0,0,0],next="end"),node("end","end")]}
        entry={"id":"source","entry":"init","nodes":[
            node("init","call_init",next="one"),node("one","append_group",[1,0,0],next="end"),node("end","end")]}
        self.assertEqual(scope.entry_counts(entry,meta,{init["id"]:init},{}),{12})
        meta["unknown_01"]=96
        with self.assertRaisesRegex(ValueError,"capacity96"):scope.entry_counts(entry,meta,{init["id"]:init},{})
        with self.assertRaisesRegex(ValueError,"initialization"):scope.entry_counts(entry,meta,{}, {})

    def test_entry_branch_join_and_different_success_counts_stay_distinct(self):
        raw=bytearray(26);raw[21]=1
        meta={"templates":[raw],"unknown_01":2,"unknown_02":1}
        f={"id":"source","entry":"setup","nodes":[node("setup","stage_setup_source",next="branch"),
            node("branch","branch_roster_text",[2],yes="append",no="end"),
            node("append","append_group",[1,0,0],next="end"),node("end","end")]}
        self.assertEqual(scope.entry_counts(f,meta,{},{}),{2,3})
        f["nodes"][1]["on"]["yes"]="end"
        self.assertEqual(scope.entry_counts(f,meta,{},{}),{2})

    def test_callback_word_not_low_byte_and_defaults_not_ignored(self):
        stage={"metadata":{"turn_slots":[{"default_fill":False,"slot":0,"action":9},
                    {"default_fill":True,"slot":1,"action":255}],
            "cell_lookup_slots":[{"slot":0,"action":255},{"slot":1,"action":8}],
            "cell_action_slots":[{"slot":0,"referenced_by_map":True,"callback_u16":0x101}],
            "templates":[]}}
        with self.assertRaisesRegex(ValueError,"WORD"):scope.callback_requirements(stage)
        stage["metadata"]["cell_action_slots"][0]["callback_u16"]=1
        self.assertEqual(scope.callback_requirements(stage),[("turn",0,9),("lookup",1,8),("cell_action",0,1)])
        stage["metadata"]["templates"]=[{"effective_template":False,"index":0,"drop":{"callback_u16":999}}]
        self.assertEqual(len(scope.callback_requirements(stage)),3)

    def test_exact_target_alias_missing_and_quarantine_not_empty_success(self):
        flow={"id":"common.callback.009","entry":"end","nodes":[node("end","end")]}
        source="dispatch:event_callback:009";runtime="stage.002.action.009"
        row={"disposition":"converted_candidate","definition_id":flow["id"],
            "target_va":"0x34531","original_caller":"field_turn_event_check @0x1a813",
            "source":{"canonical_va":"0x51bb5"},"definition_domain":"common_event"}
        result=scope.check_binding(runtime,source,{source:row},{flow["id"]:flow},{runtime:flow["id"]})
        self.assertEqual(result["definition_id"],flow["id"])
        for alias in ({},{runtime:"wrong"}):
            with self.assertRaisesRegex(ValueError,"binding differs"):
                scope.check_binding(runtime,source,{source:row},{flow["id"]:flow},alias)
        row["disposition"]="blocked"
        with self.assertRaisesRegex(ValueError,"quarantined"):
            scope.check_binding(runtime,source,{source:row},{flow["id"]:flow},{runtime:flow["id"]})

    def test_literal_text_reference_bounds_and_move_raw_prefix(self):
        stage={"stage":3,"text_reference":{"fragment_count":2}}
        f={"id":"f","nodes":[node("text","text",[1]),node("move","move",[7,0])]}
        binding=[{"definition_id":"f"}]
        scripts={"7":{"canonical_va":"0x62980","length":5,"sha256":"source",
                       "raw_hex":"0100011700"}}
        refs=scope.resource_references(stage,binding,{"f":f},scripts)
        self.assertEqual(refs[0]["source_id"],"text:004:00001")
        self.assertEqual(refs[1]["required_raw_prefix"],24)
        bad=copy.deepcopy(f);bad["nodes"][0]["args"]=[2]
        with self.assertRaisesRegex(ValueError,"fragment out of range"):
            scope.resource_references(stage,binding,{"f":bad},scripts)
        with self.assertRaisesRegex(ValueError,"script missing"):
            scope.resource_references(stage,binding,{"f":f},{})

    def test_every_victory_success_edge_has_a_next_stage_handoff(self):
        f={"id":"victory","entry":"condition","nodes":[node("condition","branch_roster_text",[2],yes="write",no="end"),
            node("write","next_stage",[3],next="end"),node("end","end")]}
        with self.assertRaisesRegex(ValueError,"lacks original next_stage"):scope.next_stage_targets(f)
        f["nodes"][0]["on"]["no"]="alternate"
        f["nodes"].append(node("alternate","next_stage",[4],next="end"))
        self.assertEqual(scope.next_stage_targets(f),{3,4})

    def test_only_shared_tail41_refinement_with_actual_c_gate_is_accepted(self):
        dispatch=[{"source_id":f"dispatch:event_callback:{i:03d}","target_va":hex(0x30000+i),"definition_id":f"old.{i}"} for i in range(90)]
        candidates={"decisions":[{"source_id":r["source_id"],"target_va":r["target_va"],"definition":r["definition_id"],"disposition":"existing_definition_reference"} for r in dispatch]}
        dispatch[41]["definition_id"]="common.loot.child.1";candidates["decisions"][41].update(definition="common.callback.041",disposition="converted_candidate")
        with self.assertRaisesRegex(ValueError,"independent complete caller"):scope.refine_callbacks(dispatch,candidates,None)
        proof={"schema":"fd2-s3-callback-binding-source-v1","original_CPU_executed":True,"actual_C":{"verified":True},
               "S2_refinement":{"current_definition":"common.callback.041"}}
        rows,changes=scope.refine_callbacks(dispatch,candidates,proof)
        self.assertEqual(rows["dispatch:event_callback:041"]["definition_id"],"common.callback.041")
        self.assertEqual(dispatch[41]["definition_id"],"common.loot.child.1")
        self.assertEqual(len(changes),1)
        candidates["decisions"][8]["definition"]="unreviewed"
        with self.assertRaisesRegex(ValueError,"lacks reviewed"):scope.refine_callbacks(dispatch,candidates,proof)


if __name__=="__main__":unittest.main()
