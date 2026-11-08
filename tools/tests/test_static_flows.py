"""有限事件图的结构、入口、参数、容量和错误出口契约。"""
import copy
import importlib.util
import json
from pathlib import Path
import shutil
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("flow_generate", ROOT / "tools/static_data/generate.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


class StaticFlowTest(unittest.TestCase):
    def document(self):
        return {"schema_version": 1, "domain": "test", "source": {"evidence": "synthetic"},
                "operations": {"test": {"args": [{"name": "value", "min": 0, "max": 5}]}, "end": {"args": []}},
                "flows": [{"id": "test.branch", "entry": "choose", "nodes": [
                    {"id": "choose", "op": "test", "args": [1], "on": {"yes": "accepted", "no": "rejected"}},
                    {"id": "accepted", "op": "end", "args": [], "on": {}},
                    {"id": "rejected", "op": "end", "args": [], "on": {}}]}]}

    def test_invalid_nodes_parameters_edges_views_and_capacity(self):
        for mutate in (
                lambda d: d.update(schema_version=True),
                lambda d: d["flows"][0]["nodes"][0].update(op="unknown"),
                lambda d: d["flows"][0]["nodes"][0].update(args=[6]),
                lambda d: d["flows"][0]["nodes"][0].update(args=[True]),
                lambda d: d["flows"][0]["nodes"][0].update(views=["unknown"]),
                lambda d: d["flows"][0]["nodes"][0]["on"].update(yes="missing"),
                lambda d: d["flows"][0]["nodes"][0]["on"].update(typo="accepted"),
                lambda d: d["flows"][0]["nodes"][1].update(id="choose"),
                lambda d: d["flows"][0].update(entry="absent"),
                lambda d: d["flows"][0]["nodes"].extend([copy.deepcopy(d["flows"][0]["nodes"][0])] * 254)):
            with self.subTest(mutate=mutate):
                data = self.document()
                mutate(data)
                with self.assertRaises(ValueError):
                    generator.validate_flows(data)

    def test_disconnected_and_closed_cycles_rejected(self):
        data = self.document()
        data["flows"][0]["nodes"][0]["on"]["yes"] = "choose"
        with self.assertRaisesRegex(ValueError, "unreachable"):
            generator.validate_flows(data)
        data["flows"][0]["nodes"] = data["flows"][0]["nodes"][:1]
        data["flows"][0]["nodes"][0]["on"] = {"next": "choose"}
        with self.assertRaisesRegex(ValueError, "exit"):
            generator.validate_flows(data)

    def test_edges_drive_generated_code_and_external_entries_are_explicit(self):
        data = self.document()
        before = generator.render_flows(generator.validate_flows(data), "0"*64)
        node = data["flows"][0]["nodes"][0]
        node["on"]["yes"], node["on"]["no"] = node["on"]["no"], node["on"]["yes"]
        after = generator.render_flows(generator.validate_flows(data), "0"*64)
        self.assertNotEqual(before, after)
        node["on"] = {"yes": "accepted"}
        data["flows"][0]["entries"] = {"cancelled_from_host": "rejected"}
        generator.validate_flows(data)

    def test_real_current_flows_are_valid(self):
        files = list((ROOT / "data/static/flows").glob("*.json"))
        self.assertTrue(files)
        for path in files:
            with self.subTest(path=path.name):
                generator.validate_flows(generator.load_json(path))

    def test_bindings_share_nodes_and_reject_chains_cycles_missing_and_shadowed_ids(self):
        data = self.document()
        data["bindings"] = {"stage.001.action.042": "test.branch", "stage.002.action.042": "test.branch"}
        generator.validate_flows(data)
        index = generator.flow_index(data)
        self.assertIs(index["stage.001.action.042"], index["test.branch"])
        self.assertIs(index["stage.002.action.042"]["nodes"], index["test.branch"]["nodes"])
        rendered = generator.render_flows(data, "0"*64).decode("utf-8")
        self.assertEqual(rendered.count("static const fd2_static_flow_node "), 1)
        for identity in ("test.branch", "stage.001.action.042", "stage.002.action.042"):
            self.assertIn('{' + json.dumps(identity) + ', k_test_nodes_0, 3, 0}', rendered)
        for bindings in ([], {"": "test.branch"}, {"test.branch": "test.branch"},
                         {"alias": "missing"}, {"alias": 1}, {"alias": "alias"},
                         {"alias": "second", "second": "test.branch"},
                         {"alias": "second", "second": "alias"}):
            with self.subTest(bindings=bindings), self.assertRaises(ValueError):
                generator.validate_flows(dict(data, bindings=bindings))

    def test_real_scene_new_binding_receipt_and_global_collision(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name) / "root"
            shutil.copytree(ROOT / "data/static", root / "data/static")
            path = root / "data/static/flows/scene.json"
            data = generator.load_json(path)
            data["bindings"]["stage.001.action.042"] = "event.action.006"
            path.write_text(json.dumps(data), encoding="utf-8")
            output = Path(name) / "generated"
            generator.generate(root, output)
            receipt = json.loads((output / "sources.json").read_text(encoding="utf-8"))
            scene = next(row for row in receipt["flows"] if row["file"] == "scene.json")
            self.assertEqual(scene["bindings"], data["bindings"])
            self.assertEqual(scene["runtime_flow_count"], len(data["flows"]) + len(data["bindings"]))
            self.assertEqual(scene["node_count"], sum(len(flow["nodes"]) for flow in data["flows"]))
            before = {p.name: p.read_bytes() for p in output.iterdir()}
            generator.generate(root, output, check=True)
            data["bindings"]["stage.entry.001"] = "event.action.006"
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate global"):
                generator.generate(root, output)
            self.assertEqual(before, {p.name: p.read_bytes() for p in output.iterdir()})

    def test_consumer_contract_rejects_missing_runtime_flow_bad_ports_and_references(self):
        manifest = generator.load_json(ROOT / "data/static/manifest.json")
        docs = {p.name: generator.load_json(p) for p in (ROOT / "data/static/flows").glob("*.json")}
        all_flows = {identity: flow for d in docs.values() for identity, flow in generator.flow_index(d).items()}
        scripts = {t["source"]["script_id"] for t in generator.load_json(
            ROOT / "data/static/tables/movement_scripts.json")["tables"]}
        for name, original in docs.items():
            contract = manifest["flow_contracts"][name]
            generator.validate_flow_contract(original, contract, scripts, all_flows)
            bad = copy.deepcopy(original)
            required = contract["required_ids"][0]
            if required in bad.get("bindings", {}):
                del bad["bindings"][required]
            else:
                bad["flows"] = [f for f in bad["flows"] if f["id"] != required]
            with self.assertRaisesRegex(ValueError, "required runtime"):
                generator.validate_flow_contract(bad, contract, scripts, all_flows)
            bad = copy.deepcopy(original)
            bad["flows"][0]["nodes"][0]["on"] = {"error": bad["flows"][0]["entry"]}
            with self.assertRaisesRegex(ValueError, "ports"):
                generator.validate_flow_contract(bad, contract, scripts, all_flows)
            bad = copy.deepcopy(original)
            bad["operations"][next(iter(bad["operations"]))]["args"].append({"name": "extra", "min": 0, "max": 1})
            with self.assertRaisesRegex(ValueError, "ABI"):
                generator.validate_flow_contract(bad, contract, scripts, all_flows)
        with self.assertRaisesRegex(ValueError, "movement script"):
            generator.validate_flow_contract(docs["scene.json"], manifest["flow_contracts"]["scene.json"], set(), all_flows)
        with self.assertRaisesRegex(ValueError, "opening stage"):
            generator.validate_flow_contract(docs["scene.json"], manifest["flow_contracts"]["scene.json"], scripts, {})


if __name__ == "__main__":
    unittest.main()
