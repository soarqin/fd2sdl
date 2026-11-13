"""S2独立来源、计数、原码向量和真实C差分协议的失败保护。"""
import copy
import hashlib
import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/ida"))
import s2_result_source as source
import verify_s2_results as oracle


class ResultSourceTests(unittest.TestCase):
    def fixture(self):
        image = bytes(0x64000)
        sha = hashlib.sha256(image).hexdigest()
        rows = [{"va": va, "chunks": [{"start": a, "end": b,
                 "raw_hex": image[a:b].hex(), "sha256": hashlib.sha256(image[a:b]).hexdigest()}
                 for a, b in source.CHUNKS[va]], "instructions": [1], "blocks": [1],
                 "callers": [], "callees": []} for va in source.TARGETS + source.HELPERS]
        return image, {"schema": "fd2-s2-result-domain-v1", "image_sha256": sha, "functions": rows}, sha

    def test_full_function_shared_tail_and_identity_are_required(self):
        image, report, sha = self.fixture()
        with patch.object(source, "RELOCATED_SHA256", sha):
            self.assertEqual(len(source.validate_source(image, report)), 20)
            for field in ("chunks", "instructions", "blocks"):
                bad = copy.deepcopy(report); bad["functions"][0][field] = []
                with self.assertRaises(ValueError): source.validate_source(image, bad)
            bad = copy.deepcopy(report); bad["functions"][0]["chunks"].pop()
            with self.assertRaises(ValueError): source.validate_source(image, bad)
            bad = copy.deepcopy(report); bad["functions"][0]["chunks"][0]["sha256"] = "0"*64
            with self.assertRaises(ValueError): source.validate_source(image, bad)
            bad = copy.deepcopy(report); bad["functions"].pop()
            with self.assertRaises(ValueError): source.validate_source(image, bad)
            with self.assertRaises(ValueError): source.validate_source(image+b"x", report)

    def test_dispatch_slots_are_not_targets_or_new_runtime_bindings(self):
        rows = source.closure_mapping()
        self.assertEqual(len(rows), 30)
        self.assertEqual(len({r["target_va"] for r in rows}), 18)
        self.assertEqual(len({r["definition_id"] for r in rows}), 18)
        self.assertEqual(len({r["semantic_family"] for r in rows}), 9)
        self.assertEqual(sum(r["target_va"] == "0x205b4" for r in rows), 11)
        self.assertEqual(sum(r["target_va"] == "0x20a87" for r in rows), 3)
        self.assertEqual(sum(r["ordered_text"] for r in rows), 5)
        self.assertEqual(sum(r["preserves_latch"] for r in rows), 4)
        self.assertTrue(all(not r["new_runtime_binding"] for r in rows))
        self.assertEqual(len({r["source_id"] for r in rows}), 30)
        self.assertEqual(sum(b-a for spans in source.CHUNKS.values() for a, b in spans), 1663)

    def test_vectors_cover_widths_latch_and_all_source_targets(self):
        rows = list(oracle.vector_inputs())
        self.assertEqual(len(rows), 832)
        self.assertEqual({r["target"] for r in rows}, set(source.TARGETS))
        self.assertEqual(sum(r["stop"] != 0 for r in rows), 6)
        self.assertEqual({r["initial"] for r in rows}, {0, 1, 2})
        self.assertTrue({0x80000000, 5, 6, 7, 0x7fffffff} <= {r["turn"] for r in rows})
        self.assertEqual({int.from_bytes(r["records"][0x40:0x42], "little") for r in rows}, {0, 1, 100, 65535})
        self.assertTrue(all(len(r["records"])//80 >= source.MINIMUM[r["target"]] for r in rows))
        self.assertTrue(all(len(r["state"]) == 32 for r in rows))

    def test_c_trace_checks_result_story_latch_owner_and_stop(self):
        expected = {"target": "0x20bf5", "result": 1, "stopped_at_story": True,
                    "stories": [{"fragment": 7, "latch": 1, "args": []}]}
        row = {"status": 0, "result": 1, "stopped_at_story": True, "owner_unchanged": True,
               "stories": [{"fragment": 7, "latch": 1}]}
        oracle.validate_probe(expected, json.dumps(row))
        for key, value in (("status", -1), ("result", 2), ("stopped_at_story", False),
                           ("owner_unchanged", False), ("stories", [{"fragment": 7, "latch": 2}])):
            bad = dict(row); bad[key] = value
            with self.subTest(key=key), self.assertRaises(ValueError): oracle.validate_probe(expected, json.dumps(bad))
        with self.assertRaises(ValueError): oracle.validate_probe(expected, "not-json")

    def test_c_probe_is_identity_bound_and_missing_rows_rejected(self):
        case = next(oracle.vector_inputs())
        expected = {"target": "0x205b4", "result": 0, "stories": [], "stopped_at_story": False}
        row = json.dumps({"status": 0, "result": 0, "stories": [],
                          "stopped_at_story": False, "owner_unchanged": True})+"\n"
        with tempfile.TemporaryDirectory() as directory:
            probe = Path(directory) / "probe.exe"; probe.write_bytes(b"probe")
            reply = SimpleNamespace(stdout=row)
            with patch.object(oracle.subprocess, "run", return_value=reply):
                identity = oracle.compare_c(probe, [case], [expected])
                self.assertEqual(identity["before_sha256"], hashlib.sha256(b"probe").hexdigest())
                self.assertEqual(identity["before_sha256"], identity["after_sha256"])
            with patch.object(oracle.subprocess, "run", return_value=SimpleNamespace(stdout="")):
                with self.assertRaises(ValueError): oracle.compare_c(probe, [case], [expected])
            def changed(*args, **kwargs):
                probe.write_bytes(b"changed"); return reply
            with patch.object(oracle.subprocess, "run", side_effect=changed):
                with self.assertRaisesRegex(ValueError, "changed"): oracle.compare_c(probe, [case], [expected])

    def test_candidates_exist_once_and_shipping_aliases_require_exact_source_identity(self):
        docs = {name: json.loads((ROOT / f"data/static/flows/{name}.json").read_text(encoding="utf-8"))
                for name in ("stage", "common_event")}
        for va in source.TARGETS:
            domain, identity = source.source_flow(va)
            self.assertEqual(sum(f["id"] == identity for f in docs[domain]["flows"]), 1)
        for stage in range(2, 30):
            self.assertFalse(any(f["id"] == f"stage.result.{stage:03d}" for f in docs["stage"]["flows"]))
            domain,identity=source.source_flow(source.SLOTS[stage])
            pure=docs['stage'].get('bindings',{}).get(f'stage.result.{stage:03d}')
            effect=docs['common_event'].get('bindings',{}).get(f'stage.{stage:03d}.result')
            if pure is not None:self.assertTrue(2<=stage<=20 and domain=='stage' and pure==identity)
            if effect is not None:self.assertTrue(2<=stage<=19 and domain=='common_event' and effect==identity)
        for name in docs:
            for flow in docs[name]["flows"]:
                if ".result.source_" not in flow["id"]: continue
                self.assertEqual(flow["entry"], "domain")
                domain = next(n for n in flow["nodes"] if n["id"] == "domain")
                self.assertIn(domain["op"], {"result_count_at_least", "branch_hidden_count"})
                self.assertFalse(any(n["args"][1] for n in flow["nodes"] if n["op"] in
                                     {"result_leader_dead", "result_side_present"}))


if __name__ == "__main__": unittest.main()
