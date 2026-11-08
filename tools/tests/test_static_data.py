"""冷定义生成器拒绝坏输入；生产表由独立原版来源另行核对。"""
import importlib.util
import json
from pathlib import Path
import tempfile
import shutil
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("static_generate", ROOT / "tools/static_data/generate.py")
generate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generate)


class StaticDataTest(unittest.TestCase):
    def document(self):
        return {"schema_version": 1, "tables": [{"name": "test_table", "c_type": "uint8_t", "dimensions": [2, 2],
                "values": [[0, 255], [1, 2]], "source": {"evidence": "synthetic test"}}]}

    def test_reject_invalid_schema_type_extent_range_and_identifier(self):
        for mutate in (lambda d: d.update(schema_version=2), lambda d: d.update(unknown=1),
                       lambda d: d["tables"][0].update(c_type="uint8_t; injected"),
                       lambda d: d["tables"][0].update(name="../unsafe"),
                       lambda d: d["tables"][0].update(dimensions=[3, 2]),
                       lambda d: d["tables"][0]["values"][0].__setitem__(1, 256),
                       lambda d: d["tables"][0]["values"][0].__setitem__(1, True),
                       lambda d: d["tables"][0].update(source={})):
            document = self.document()
            mutate(document)
            with self.assertRaises(ValueError):
                generate.validate_document(document)

    def test_duplicate_key_rejected(self):
        with tempfile.TemporaryDirectory() as name:
            path = Path(name) / "bad.json"
            path.write_text('{"schema_version":1,"schema_version":2}')
            with self.assertRaises(ValueError):
                generate.load_json(path)

    def test_real_inputs_reproducible_and_check_detects_stale(self):
        with tempfile.TemporaryDirectory() as name:
            output = Path(name) / "generated"
            count = generate.generate(ROOT, output)
            self.assertGreater(count, 0)
            before = {p.name: p.read_bytes() for p in output.iterdir()}
            self.assertEqual(count, generate.generate(ROOT, output, check=True))
            self.assertEqual(before, {p.name: p.read_bytes() for p in output.iterdir()})
            next(output.glob("*.inc")).write_text("stale")
            with self.assertRaisesRegex(ValueError, "stale"):
                generate.generate(ROOT, output, check=True)

    def test_struct_fields_checked(self):
        document = self.document()
        table = document["tables"][0]
        table.update(c_type="fd2_field_stage_music", dimensions=[2])
        generate.validate_document(document)
        table["values"][0][0] = -1
        with self.assertRaises(ValueError):
            generate.validate_document(document)

    def test_cross_table_references_reject_unknown_ids(self):
        tables = {t["name"]: t["values"] for path in (ROOT / "data/static/tables").glob("*.json")
                  for t in generate.load_json(path)["tables"]}
        generate.validate_references(tables)
        for name, column, value in (("k_growth_records", 10, 20),
                                    ("k_player_base_records", 12, 215),
                                    ("k_learning_records", 1, 36),
                                    ("k_enemy_records", 1, 0)):
            old = tables[name][0][column]
            tables[name][0][column] = value
            with self.assertRaisesRegex(ValueError, "reference"):
                generate.validate_references(tables)
            tables[name][0][column] = old

    def test_manifest_sources_contracts_and_stale_output_rejected(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name) / "root"
            shutil.copytree(ROOT / "data/static", root / "data/static")
            output = Path(name) / "out"
            generate.generate(root, output)
            before = {p.name: p.read_bytes() for p in output.iterdir()}
            manifest_path = root / "data/static/manifest.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            for key, value in (("schema_version", True), ("sources", {}), ("table_contracts", {})):
                changed = dict(manifest, **{key: value})
                manifest_path.write_text(json.dumps(changed), encoding="utf-8")
                with self.assertRaises(ValueError):
                    generate.generate(root, output)
                self.assertEqual(before, {p.name: p.read_bytes() for p in output.iterdir()})
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            table_path = root / "data/static/tables/magic_fx.json"
            doc = json.loads(table_path.read_text(encoding="utf-8"))
            table = doc["tables"][0]
            table["values"] = table["values"][:1]
            table["dimensions"][0] = 1
            table_path.write_text(json.dumps(doc), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "contract"):
                generate.generate(root, output)
            shutil.copyfile(ROOT / "data/static/tables/magic_fx.json", table_path)
            (output / "obsolete.inc").write_text("old table", encoding="utf-8")
            for check in (False, True):
                with self.assertRaisesRegex(ValueError, "obsolete"):
                    generate.generate(root, output, check=check)


if __name__ == "__main__":
    unittest.main()
