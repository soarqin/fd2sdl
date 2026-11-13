"""方向复位caller完整原来源与冷次序的真负例；不启动构建、IDA或窗口。"""
import copy
import importlib.util
import json
from pathlib import Path
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("direction_source", ROOT / "tools/ida/verify_fd2_direction_callers.py")
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)


class DirectionCallersTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = (ROOT / "original_game/FD2.EXE").read_bytes()
        cls.readsets = [json.loads((ROOT / name).read_bytes()) for name in verify.READSETS]
        cls.original = verify.verify_source(cls.exe, cls.readsets)
        cls.cold = {name: json.loads((ROOT / "data/static/flows" / (name + ".json")).read_bytes())
                    for name in ("runtime", "progress", "loot", "stage", "scene", "common_event")}

    def document(self):
        return copy.deepcopy(self.cold)

    def node(self, documents, domain, identity, label):
        return next(n for n in verify.flow(documents[domain], identity)["nodes"] if n["id"] == label)

    def assert_cold_rejected(self, documents):
        with self.assertRaises(ValueError):
            verify.compare_flows(documents, self.original)

    def test_actual_source_closed_callers_and_current_cold(self):
        result = verify.verify(ROOT, self.exe)
        self.assertEqual(result["original"]["parents"], 20)
        self.assertEqual(result["original"]["static_reset_calls"], 23)
        self.assertEqual(result["original"]["function"]["delay_ms"], 20)
        self.assertEqual(result["cold_comparison"]["common_bindings"], 6)
        stage2 = next(row for row in result["original"]["complete_functions"] if row["parent"] == "0x32e8c")
        self.assertEqual(stage2["reset_calls"], ["0x3313b"])
        self.assertFalse(result["CPU_execution_claimed"])
        self.assertFalse(result["native_execution_claimed"])
        self.assertFalse(result["new_GUI_started"])

    def test_wrong_exe_identity_rejected(self):
        with self.assertRaisesRegex(ValueError, "原EXE身份"):
            verify.verify_source(b"wrong original", self.readsets)

    def test_full_source_chunk_not_only_call_opcode_is_guarded(self):
        source_range = verify.source_range
        for address in (0x134e4, 0x13a9f, 0x15055):
            with self.subTest(address=hex(address)):
                def changed(exe, start, size):
                    raw = bytearray(source_range(exe, start, size))
                    raw[address - start] ^= 1
                    return bytes(raw)
                with patch.object(verify, "source_range", side_effect=changed):
                    with self.assertRaisesRegex(ValueError, "完整"):
                        verify.verify_source(self.exe, self.readsets)

    def test_parent_xref_closure_cannot_omit_later_private_caller(self):
        documents = copy.deepcopy(self.readsets)
        for d in documents:
            for f in d["functions"]:
                if f["va"] == 0x134e4:
                    f["callers"].remove(0x350c8)
        with self.assertRaisesRegex(ValueError, "xref闭集"):
            verify.verify_source(self.exe, documents)

    def test_shared_tail_instruction_head_is_required(self):
        documents = copy.deepcopy(self.readsets)
        for d in documents:
            for tail in d.get("shared_tail_instructions", []):
                tail["instructions"] = [i for i in tail["instructions"] if i["ea"] != 0x3313b]
        with self.assertRaisesRegex(ValueError, "静态callsite"):
            verify.verify_source(self.exe, documents)

    def test_runtime_reset_only_and_refresh_entries_cannot_be_swapped(self):
        for label in ("directions_reset", "directions_reset_refresh"):
            documents = self.document()
            f = verify.flow(documents["runtime"], "turncycle")
            f["entries"][label] = "directions_reset_refresh" if label == "directions_reset" else "directions_reset"
            self.assert_cold_rejected(documents)
        documents = self.document()
        self.node(documents, "runtime", "turncycle", "directions_refresh_wait")["args"] = [19]
        self.assert_cold_rejected(documents)

    def test_bare_full_cannot_hide_extra_reset_or_delay(self):
        for operation, args in (("action_reset_directions", []), ("action_delay_ms", [20])):
            documents = self.document()
            f = verify.flow(documents["progress"], "physical.full")
            f["nodes"].append({"id": "unwanted", "op": operation, "args": args, "on": {}})
            self.assert_cold_rejected(documents)

    def test_full_wrapper_cannot_skip_or_repeat_wait(self):
        for mode in ("skip", "repeat", "wrong"):
            documents = self.document()
            if mode == "skip":
                self.node(documents, "progress", "physical.caller.full", "reset")["on"]["next"] = "done"
            elif mode == "repeat":
                self.node(documents, "progress", "physical.caller.full", "wait")["on"]["next"] = "reset"
            else:
                self.node(documents, "progress", "physical.caller.full", "wait")["args"] = [0]
            self.assert_cold_rejected(documents)

    def test_short_wait_cannot_move_past_attack_or_counter_branch(self):
        for before in (True, False):
            documents = self.document()
            self.node(documents, "progress", "physical.short", "reset.before" if before else "reset.after")["on"]["next"] = "attack" if before else "finish"
            self.assert_cold_rejected(documents)
        documents = self.document()
        self.node(documents, "progress", "physical.short", "begin")["on"]["next"] = "attack"
        self.assert_cold_rejected(documents)

    def test_stage_both_reset_sites_and_shared_stage2_tail_keep_wait(self):
        for identity, label in (("stage.entry.001", "directions_a.wait"),
                                ("stage.entry.001", "directions_b.wait")):
            documents = self.document()
            self.node(documents, "stage", identity, label)["args"] = [0]
            self.assert_cold_rejected(documents)
        documents=self.document();entry=verify.flow(documents['stage'],'stage.entry.002')
        reset=next(n for n in entry['nodes'] if n['op']=='reset_directions')
        self.node(documents,'stage','stage.entry.002',reset['on']['next'])['args']=[0]
        self.assert_cold_rejected(documents)

    def test_stage2_alias_keeps_unique_source_without_chain_missing_or_shadow(self):
        document=self.document()['stage']
        self.assertEqual(verify.flow(document,'stage.entry.002')['id'],'stage.source.entry.002')
        for change in ('chain','missing','shadow','wrong_source'):
            documents=self.document();d=documents['stage']
            if change=='chain':d['bindings']['stage.source.entry.002']='stage.source.entry.003'
            elif change=='missing':d['bindings']['stage.entry.002']='missing'
            elif change=='shadow':
                old=copy.deepcopy(verify.flow(d,'stage.entry.002'));old['id']='stage.entry.002';d['flows'].append(old)
            else:d['bindings']['stage.entry.002']='stage.source.entry.003'
            with self.subTest(change=change):self.assert_cold_rejected(documents)

    def test_common_four_reset_handlers_and_two_no_reset_handlers(self):
        for action in (0, 1, 2, 3):
            documents = self.document()
            f = verify.flow(documents["common_event"], f"common.legacy.action.{action:03d}")
            n = next(n for n in f["nodes"] if n["op"] == "directions_reset")
            following = next(n2 for n2 in f["nodes"] if n2["id"] == n["on"]["next"])
            following["args"] = [1]
            self.assert_cold_rejected(documents)
        for action in (4, 6):
            documents = self.document()
            f = verify.flow(documents["common_event"], f"common.legacy.action.{action:03d}")
            f["nodes"].append({"id": "unwanted", "op": "directions_reset", "args": [], "on": {}})
            self.assert_cold_rejected(documents)

    def test_action_finish_cannot_repeat_outer_physical_reset(self):
        documents = self.document()
        self.node(documents, "loot", "action.finish", "action.acted")["op"] = "action_reset_directions"
        self.assert_cold_rejected(documents)


if __name__ == "__main__":
    unittest.main()
