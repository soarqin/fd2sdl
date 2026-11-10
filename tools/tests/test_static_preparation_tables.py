"""准备原字节校验和唯一源投影保护；不依赖未分发的原资源。"""
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/static_data"))
import generate
import verify_preparation_tables as verify


class PreparationTableTest(unittest.TestCase):
    def fixture(self):
        raw = struct.pack("<2H", 506, 100)
        return {"name": "fee", "c_type": "uint16_t", "dimensions": [2], "values": [506, 100],
                "source": {"file": "FD2.EXE", "file_sha256": verify.EXE_SHA256,
                           "object": 2, "canonical_va": "0x52397", "length": 4,
                           "sha256": hashlib.sha256(raw).hexdigest(),
                           "view": "raw-unrelocated", "evidence": "synthetic neighboring word/fee"}}, raw

    def test_word_little_endian_and_neighbor_preserved(self):
        table, raw = self.fixture()
        verify.check_table(table, ("fee", "uint16_t", (2,), 0x52397), raw)
        table["values"][0] = 0
        table["source"]["sha256"] = hashlib.sha256(bytes(2) + raw[2:]).hexdigest()
        with self.assertRaisesRegex(ValueError, "original bytes"):
            verify.check_table(table, ("fee", "uint16_t", (2,), 0x52397), raw)

    def test_wrong_source_shape_type_and_boolean_rejected(self):
        table, raw = self.fixture()
        for mutate in (lambda t: t.update(c_type="uint8_t"), lambda t: t.update(dimensions=[1, 2]),
                       lambda t: t["values"].__setitem__(0, True),
                       lambda t: t["source"].update(canonical_va="0x52399")):
            bad = copy.deepcopy(table)
            mutate(bad)
            with self.assertRaises(ValueError):
                verify.check_table(bad, ("fee", "uint16_t", (2,), 0x52397), raw)
        with self.assertRaisesRegex(ValueError, "identity"):
            verify.check({}, b"wrong executable")

    def tables(self):
        return {t["name"]: t["values"] for p in (ROOT / "data/static/tables").glob("*.json")
                for t in json.loads(p.read_text(encoding="utf-8"))["tables"]}

    def test_projection_keeps_profile0_invalid_and_reuses_neighbor(self):
        tables = self.tables()
        views = {t["name"]: t["values"] for t in generate.preparation_views(tables)}
        self.assertEqual([i for i, v in enumerate(views["k_prepare_revive_profile_valid"]) if v],
                         list(range(1, 7)) + list(range(8, 26)) + [28])
        self.assertEqual(views["k_prepare_service_return_fragments"], [1, 506, 1, 506, 506, 506])
        tables["k_prepare_revive_coefficients"][0] = 1234
        views = {t["name"]: t["values"] for t in generate.preparation_views(tables)}
        self.assertEqual(views["k_prepare_service_return_fragments"][-1], 1234)
        self.assertFalse(views["k_prepare_revive_profile_valid"][0])

    def test_projection_rejects_new_profile_stock_and_fake_departure_asset(self):
        for mutate in (lambda t: t["k_prepare_promotion_records"][0].__setitem__(0, 0),
                       lambda t: t["k_prepare_special_items"].__setitem__(0, 215),
                       lambda t: t["k_stage_prepare"][0].__setitem__(3, 215),
                       lambda t: t["k_stage_prepare"][0].__setitem__(0, 3),
                       lambda t: t["k_prepare_deployment_roster_gate"][0].__setitem__(2, 96),
                       lambda t: t["k_prepare_town_service_entries"].__setitem__(2, 13)):
            tables = self.tables()
            mutate(tables)
            with self.assertRaises(ValueError):
                generate.preparation_views(tables)


if __name__ == "__main__":
    unittest.main()
