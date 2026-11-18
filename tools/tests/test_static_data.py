"""冷定义生成器拒绝坏输入；生产表由独立原版来源另行核对。"""
import importlib.util
import copy
import json
from pathlib import Path
import tempfile
import shutil
import unittest

ROOT = Path(__file__).resolve().parents[2]
spec = importlib.util.spec_from_file_location("static_generate", ROOT / "tools/static_data/generate.py")
generate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generate)


def read_tables():
    return {table["name"]: table["values"] for path in (ROOT / "data/static/tables").glob("*.json")
            for table in generate.load_json(path).get("tables", [])}


class StaticDataTest(unittest.TestCase):
    def document(self):
        return {"schema_version": 2, "tables": [{"name": "test_table", "c_type": "uint8_t", "dimensions": [2, 2],
                "values": [[0, 255], [1, 2]]}]}

    def test_reject_invalid_schema_type_extent_range_and_identifier(self):
        for mutate in (lambda d: d.update(schema_version=1), lambda d: d.update(unknown=1),
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
            self.assertTrue(all(name.endswith(".inc") for name in before))
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

    def test_movement_actor_capacity_checked_before_generation(self):
        document = {"schema_version": 2, "scripts": {"42": [1, 1, 1, 95, 0]}}
        self.assertEqual(generate.read_movement_scripts(document), {42: [1, 1, 1, 95, 0]})
        for actor in (96, 255):
            document["scripts"]["42"][3] = actor
            with self.subTest(actor=actor), self.assertRaisesRegex(ValueError, "actor/direction"):
                generate.read_movement_scripts(document)

    def test_movement_ids_and_streams_must_be_unambiguous(self):
        for scripts in ({"042": [0]}, {"256": [0]}, {"0": [True]}, {"0": []},
                        {"0": [1, 128]}, {"0": [1, 128, 1, 0, 4]}, {"0": [0, 0]}):
            with self.subTest(scripts=scripts), self.assertRaises(ValueError):
                generate.read_movement_scripts({"schema_version": 2, "scripts": scripts})

    def test_resize_movement_script_and_reject_missing_referenced_id(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name) / "root"
            shutil.copytree(ROOT / "data/static", root / "data/static")
            path = root / "data/static/tables/movement_scripts.json"
            document = generate.load_json(path)
            document["scripts"]["0"] = [1, 1, 1, 0, 3]
            path.write_text(json.dumps(document), encoding="utf-8")
            output = Path(name) / "generated"
            generate.generate(root, output)
            before = {p.name: p.read_bytes() for p in output.iterdir()}
            self.assertIn(b"k_movement_script_00[5]", before["movement_scripts.inc"])
            self.assertIn(b"{0, k_movement_script_00, sizeof(k_movement_script_00)}",
                          before["movement_scripts.inc"])
            generate.generate(root, output, check=True)

            del document["scripts"]["0"]
            path.write_text(json.dumps(document), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "unknown movement script reference"):
                generate.generate(root, output)
            self.assertEqual(before, {p.name: p.read_bytes() for p in output.iterdir()})

    def test_cross_table_references_reject_unknown_ids(self):
        tables = read_tables()
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

    def test_constructor_alias_projection_has_only_reviewed_source_windows(self):
        tables = read_tables()
        self.assertEqual([len(row) for row in generate.constructor_alias_records(tables)],[24,24])
        for row,column,value in ((0,0,40),(0,1,2),(0,2,750),(1,2,800)):
            changed=copy.deepcopy(tables);changed["k_player_base_alias_views"][row][column]=value
            with self.subTest(row=row,column=column), self.assertRaisesRegex(ValueError,"alias reference"):
                generate.constructor_alias_records(changed)
        changed=copy.deepcopy(tables);changed["k_stage_prepare"]=[]
        with self.assertRaisesRegex(ValueError,"source extent"):
            generate.constructor_alias_records(changed)

    def test_manifest_contracts_and_stale_output_rejected(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name) / "root"
            shutil.copytree(ROOT / "data/static", root / "data/static")
            output = Path(name) / "out"
            generate.generate(root, output)
            before = {p.name: p.read_bytes() for p in output.iterdir()}
            manifest_path = root / "data/static/manifest.json"
            manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
            for key, value in (("schema_version", True), ("table_contracts", {}), ("flow_contracts", {})):
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
