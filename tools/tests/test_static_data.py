"""冷定义生成器拒绝坏输入；生产表由独立原版来源另行核对。"""
import copy
import json
from pathlib import Path
import sys
import tempfile
import shutil
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/static_data"))
import generate
from extract import extract, source_values
from table_schema import project_tables, table_definition

TABLES = ROOT / "data/static/tables"


def read_tables():
    return {table["name"]: table["values"] for path in TABLES.glob("*.json")
            if path.stem != "movement_scripts"
            for table in project_tables(path.stem, generate.load_json(path))}


class StaticDataTest(unittest.TestCase):
    def test_reject_invalid_schema_fields_extent_range_and_identifier(self):
        for mutate in (lambda d: d.update(schema_version=2), lambda d: d.update(unknown=1),
                       lambda d: d["items"].update({"00": d["items"].pop("0")}),
                       lambda d: d["items"]["0"].update(category=256),
                       lambda d: d["items"]["0"].update(attack_bonus=32768),
                       lambda d: d["items"]["0"].update(buy_price=True),
                       lambda d: d["items"]["0"].update(source={}),
                       lambda d: d["items"]["0"].pop("unknown_16"),
                       lambda d: d["equipment_profiles"][0]["allowed_categories"].pop()):
            document = generate.load_json(TABLES / "items.json")
            mutate(document)
            with self.assertRaises(ValueError):
                project_tables("items", document)

    def test_named_mod_fields_pack_signed_words_and_preserve_unknown_bytes(self):
        document = generate.load_json(TABLES / "items.json")
        item = document["items"]["0"]
        item.update(attack_bonus=-2, buy_price=500, use_parameter=0x1234, unknown_16=129)
        document["items"]["0"] = dict(reversed(list(item.items())))
        tables = {table["name"]: table["values"] for table in project_tables("items", document)}
        row = tables["ITEM_RECORDS"][0]
        self.assertEqual(row[1:3], [254, 255])
        self.assertEqual(row[14:16], [0x34, 0x12])
        self.assertEqual(row[19:21], [244, 1])
        self.assertEqual(row[22], 129)
        self.assertEqual(table_definition("items", tables), document)

    def test_map_spell_fields_follow_spell_id_not_object_order(self):
        document = generate.load_json(TABLES / "magic.json")
        document["spells"] = dict(reversed(list(document["spells"].items())))
        document["spells"]["17"]["map"]["cost_record_id"] = 19
        tables = project_tables("magic", document)
        profile = next(t for t in generate.native_records("magic", tables) if t["name"] == "k_magic_map_profiles")
        self.assertEqual(profile["values"][17], [17, 19, 0, 1])
        self.assertEqual(profile["values"][24], [16, 16, 0, 0])
        document["spells"]["28"]["map"] = document["spells"]["27"]["map"]
        with self.assertRaisesRegex(ValueError, "unknown field"):
            project_tables("magic", document)

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
        document = generate.load_json(TABLES / "stage.json")
        music = document["stages"]["0"]["music"]
        music.update(primary=13, alternate=8)
        values = {t["name"]: t["values"] for t in project_tables("stage", document)}
        self.assertEqual(values["k_stage_music"][0], [13, 8])
        music["primary"] = -1
        with self.assertRaises(ValueError):
            project_tables("stage", document)

    def test_short_fx_offsets_follow_named_sequence_lengths(self):
        document = generate.load_json(TABLES / "battle_animation.json")
        first, second = document["short_fx_sequences"][:2]
        first.append(second.pop())
        tables = {row["name"]: row["values"] for row in project_tables("battle_animation", document)}
        category = document["short_fx_sequence_by_category"].index(1)
        self.assertEqual(tables["k_battle_short_fx_offsets"][category], 1 + 2 * len(first))
        self.assertEqual(table_definition("battle_animation", tables), document)
        document["short_fx_sequence_by_category"][category] = 6
        with self.assertRaisesRegex(ValueError, "invalid short FX sequence reference"):
            project_tables("battle_animation", document)

    def test_movement_actor_capacity_checked_before_generation(self):
        slot = {"slot": 95, "direction": 0}
        document = {"schema_version": 3, "scripts": {"42": {"groups": [{"move_cells": 1, "actors": [slot]}]}}}
        self.assertEqual(generate.read_movement_scripts(document), {42: [1, 1, 1, 95, 0]})
        for actor in (96, 255):
            slot["slot"] = actor
            with self.subTest(actor=actor), self.assertRaisesRegex(ValueError, "actor/direction"):
                generate.read_movement_scripts(document)

    def test_movement_ids_and_modes_must_be_unambiguous(self):
        actor = {"slot": 0, "direction": 5}
        redraw = {"groups": [{"wait_frames": 0, "actors": [actor]}]}
        self.assertEqual(generate.read_movement_scripts({"schema_version": 3, "scripts": {"0": redraw}}),
                         {0: [1, 128, 1, 0, 5]})
        for scripts in ({"042": {"groups": []}}, {"256": {"groups": []}}, {"0": [0]},
                        {"0": {"groups": [{"move_cells": 0, "wait_frames": 0, "actors": []}]}},
                        {"0": {"groups": [{"wait_frames": 1, "actors": [actor]}]}},
                        {"0": {"groups": [{"move_cells": True, "actors": []}]}},
                        {"0": {"groups": [{"move_cells": 0, "actors": [actor] * 33}]}}):
            with self.subTest(scripts=scripts), self.assertRaises(ValueError):
                generate.read_movement_scripts({"schema_version": 3, "scripts": scripts})

    def test_resize_movement_script_and_reject_missing_referenced_id(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name) / "root"
            shutil.copytree(ROOT / "data/static", root / "data/static")
            path = root / "data/static/tables/movement_scripts.json"
            document = generate.load_json(path)
            document["scripts"]["0"] = {"groups": [{"move_cells": 1, "actors": [{"slot": 0, "direction": 3}]}]}
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
        tables["MAGIC_MAP_COST_IDS"][17] = 36
        with self.assertRaisesRegex(ValueError, "map magic reference"):
            generate.validate_references(tables)

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
            manifest["table_contracts"]["magic_fx.json"]["k_fx_friendly_clips"]["dimensions"][0] = 1
            manifest_path.write_text(json.dumps(manifest), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "extent"):
                generate.generate(root, output)
            shutil.copyfile(ROOT / "data/static/manifest.json", manifest_path)
            self.assertEqual(before, {p.name: p.read_bytes() for p in output.iterdir()})
            (output / "obsolete.inc").write_text("old table", encoding="utf-8")
            for check in (False, True):
                with self.assertRaisesRegex(ValueError, "obsolete"):
                    generate.generate(root, output, check=check)

    def test_extract_source_music_planes_and_signed_scalars(self):
        with patch("extract.source_range", return_value=b"\x13\x01\x0c\x12"):
            self.assertEqual(source_values(b"", "fd2_field_stage_music", (2,), 0),
                             [[19, 12], [1, 18]])
        with patch("extract.source_range", return_value=b"\xfb\xff\xff\xff"):
            self.assertEqual(source_values(b"", "int32_t", (1,), 0), [-5])

    def test_extract_rejects_modified_exe_and_protected_output(self):
        with tempfile.TemporaryDirectory() as name:
            root = Path(name)
            original = root / "original"
            original.mkdir()
            executable = original / "FD2.EXE"
            executable.write_bytes(b"unknown executable")
            output = root / "export"
            with self.assertRaisesRegex(ValueError, "FD2.EXE"):
                extract(executable, output)
            self.assertFalse(output.exists())
            with self.assertRaises(ValueError):
                extract(executable, original / "export")
            self.assertEqual(list(original.iterdir()), [executable])


if __name__ == "__main__":
    unittest.main()
