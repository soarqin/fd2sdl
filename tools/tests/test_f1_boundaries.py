"""F1 来源门回归：完整主跨度不能掩盖共享尾、重叠或越界指令。"""
import copy
import hashlib
import struct
import unittest
from unittest.mock import patch

from tools.ida import verify_f1_boundaries as boundary
from tools.ida.verify_f1_boundaries import verify_instruction_coverage


class FullChunkCoverageTests(unittest.TestCase):
    def fixture(self):
        return {"chunks": [{"start": 100, "end": 103}, {"start": 200, "end": 202}],
                "instructions": [{"va": 100, "size": 2}, {"va": 102, "size": 1},
                                 {"va": 200, "size": 2}]}

    def test_primary_and_shared_tail_are_both_required(self):
        source = self.fixture()
        verify_instruction_coverage(source)
        source["instructions"].pop()
        with self.assertRaisesRegex(ValueError, "shared-tail"):
            verify_instruction_coverage(source)

    def test_duplicate_overlap_and_outside_do_not_count_as_coverage(self):
        source = self.fixture()
        variants = []
        duplicate = copy.deepcopy(source)
        duplicate["instructions"].append({"va": 100, "size": 2})
        variants.append(duplicate)
        crossing = copy.deepcopy(source)
        crossing["instructions"][-1]["size"] = 3
        variants.append(crossing)
        outside = copy.deepcopy(source)
        outside["instructions"].append({"va": 500, "size": 1})
        variants.append(outside)
        overlap = copy.deepcopy(source)
        overlap["chunks"].append({"start": 100, "end": 103})
        variants.append(overlap)
        for variant in variants:
            with self.subTest(variant=variant):
                with self.assertRaises(ValueError):
                    verify_instruction_coverage(variant)


class SourceAndExclusionTests(unittest.TestCase):
    def test_chunk_bytes_and_digest_are_both_independent(self):
        image=b"abcde"
        function={"chunks":[{"start":1,"end":4,"bytes":b"bcd".hex(),
                              "sha256":hashlib.sha256(b"bcd").hexdigest()}]}
        boundary.verify_function_bytes(function,image)
        for field,value in (("bytes",b"bad".hex()),("sha256","0"*64),("end",6)):
            changed=copy.deepcopy(function);changed["chunks"][0][field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):
                boundary.verify_function_bytes(changed,image)

    def tables(self):
        return [{"name":name,"va":va,"count":count,"targets":[1]*count}
                for name,(va,count) in boundary.DISPATCH_SCOPE.items()]

    def test_dispatch_unknown_duplicate_count_and_missing_are_rejected(self):
        tables=self.tables();boundary.verify_dispatch_scope(tables)
        changes=[]
        for field,value in (("count",31),("va",0x51b90),("name","callback255"),("targets",[])):
            changed=copy.deepcopy(tables);changed[0][field]=value;changes.append(changed)
        changes.extend((tables[:-1],tables+[tables[0]]))
        for changed in changes:
            with self.subTest(changed=changed[0]),self.assertRaises(ValueError):
                boundary.verify_dispatch_scope(changed)

    def test_dispatch_targets_are_read_from_original_not_report_expected(self):
        image=struct.pack("<3I",100,200,300)
        table={"va":0,"count":3,"targets":[100,200,300],"sha256":hashlib.sha256(image).hexdigest()}
        boundary.verify_dispatch_bytes(table,image)
        for field,value in (("targets",[100,201,300]),("sha256","0"*64),("va",1)):
            changed=copy.deepcopy(table);changed[field]=value
            with self.subTest(field=field),self.assertRaises(ValueError):
                boundary.verify_dispatch_bytes(changed,image)

    def fixture(self):
        owners=[{"owner":0x53a51,"xrefs":[{"from":100,"function":10,"write":True}]},
                {"owner":0x53a55,"xrefs":[{"from":200,"function":20,"write":False}]}]
        tables=self.tables();next(t for t in tables if t["name"]=="victory")["targets"][22]=0x24754
        return {"owner_refs":owners,
                "functions":[{"va":v} for v in boundary.MAP_PARAMETER_WRITERS]+
                            [{"va":0x24754,"callers":[0x25bf4]}],
                "tables":tables,"opening_callgraph":[{"va":0x3231b,"unresolved_calls":[]}]}

    def test_exclusion_refuses_changed_map_metadata_writers_and_new_consumers(self):
        source=self.fixture()
        expected={r["owner"]:boundary.digest_json(r) for r in source["owner_refs"]}
        with patch.dict(boundary.REVIEWED_OWNER_SHA,expected,clear=True):
            boundary.verify_unused_scope(source)
            variants=[]
            for owner in range(2):
                changed=copy.deepcopy(source)
                changed["owner_refs"][owner]["xrefs"][0]["write"]=not changed["owner_refs"][owner]["xrefs"][0]["write"]
                variants.append(changed)
            changed=copy.deepcopy(source);changed["functions"].pop(0);variants.append(changed)
            changed=copy.deepcopy(source)
            changed["opening_callgraph"].append({"va":0x1b6b7,"unresolved_calls":[]});variants.append(changed)
            changed=copy.deepcopy(source)
            changed["opening_callgraph"][0]["unresolved_calls"]=[{"va":123}];variants.append(changed)
            changed=copy.deepcopy(source)
            next(t for t in changed["tables"] if t["name"]=="callback")["targets"][0]=0x24754
            variants.append(changed)
            for index,changed in enumerate(variants):
                with self.subTest(index=index),self.assertRaises(ValueError):
                    boundary.verify_unused_scope(changed)


if __name__ == "__main__":
    unittest.main()
