"""G5库存闭包工具的来源/完整范围拒绝；合成fixture不冒称原语义。"""
import copy
import importlib.util
from pathlib import Path
import sys
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools/ida"))
spec = importlib.util.spec_from_file_location("inventory_closure", ROOT / "tools/ida/verify_fd2_inventory_closure.py")
module = importlib.util.module_from_spec(spec); spec.loader.exec_module(module)


class InventoryClosureTest(unittest.TestCase):
    def evidence(self):
        image = bytes(range(96))
        functions = []
        for i, va in enumerate(module.WRITERS):
            a, b = 4*i, 4*i+4; raw = image[a:b]
            functions.append({"va": va, "chunks": [{"start": a, "end": b, "raw_hex": raw.hex(),
                "sha256": module.hashlib.sha256(raw).hexdigest()}], "instructions": [{"ea": a}],
                "blocks": [{"start": a}], "callers": []})
        evidence = {"schema": "fd2-g5-inventory-domain-v1", "image_sha256": module.hashlib.sha256(image).hexdigest(),
            "owner_refs": [], "count_writes": [], "functions": functions,
            "writer_callers": [{"va": v, "callers": []} for v in module.WRITERS]}
        guards = {"IMAGE_SHA": evidence["image_sha256"], "SCOPE_SHA": module.digest_json([f["va"] for f in functions]),
            "XREF_SHA": module.digest_json([]), "CALLER_SHA": module.digest_json(evidence["writer_callers"]),
            "COUNTS_SHA": module.digest_json([]),
            "CHUNKS_SHA": module.digest_json([[f["va"], [[c["start"],c["end"],c["sha256"]] for c in f["chunks"]]] for f in functions])}
        return image, evidence, guards

    def test_guarded_synthetic_complete_scope(self):
        image, evidence, guards = self.evidence()
        with patch.multiple(module, **guards):
            self.assertEqual(module.verify_evidence(evidence, image), 20)

    def test_source_identity_rejected(self):
        image, evidence, guards = self.evidence()
        with patch.multiple(module, **guards):
            with self.assertRaisesRegex(ValueError, "image identity"):
                module.verify_evidence(evidence, image+b"x")
            evidence["image_sha256"] = "bad"
            with self.assertRaisesRegex(ValueError, "evidence identity"):
                module.verify_evidence(evidence, image)

    def test_scope_xref_and_caller_omissions_rejected(self):
        image, evidence, guards = self.evidence()
        for key in ("functions", "owner_refs", "writer_callers", "count_writes"):
            bad = copy.deepcopy(evidence)
            if key in ("owner_refs", "count_writes"): bad[key] = [{"va": 0x53a45, "refs": []}]
            else: bad[key].pop()
            with patch.multiple(module, **guards):
                with self.assertRaisesRegex(ValueError, "scope changed"):
                    module.verify_evidence(bad, image)

    def test_partial_chunk_or_raw_edit_rejected(self):
        image, evidence, guards = self.evidence()
        bad = copy.deepcopy(evidence); bad["functions"][0]["chunks"][0]["end"] -= 1
        with patch.multiple(module, **guards):
            with self.assertRaisesRegex(ValueError, "chunk scope"):
                module.verify_evidence(bad, image)
            bad = copy.deepcopy(evidence); bad["functions"][0]["chunks"][0]["raw_hex"] = "00000000"
            with self.assertRaisesRegex(ValueError, "function bytes"):
                module.verify_evidence(bad, image)

    def test_missing_instructions_cannot_be_claimed_complete(self):
        image, evidence, guards = self.evidence(); evidence["functions"][0]["instructions"] = []
        with patch.multiple(module, **guards):
            with self.assertRaisesRegex(ValueError, "incomplete function"):
                module.verify_evidence(evidence, image)

    def test_dense_type_property_includes_external_c0_counterexample(self):
        record = bytearray(80)
        for i in range(1,8): record[10+2*i] = 0x80
        self.assertTrue(module.dense_record(record))
        for at, value in ((10,0x80), (14,0), (24,0xc0)):
            bad = bytearray(record); bad[at] = value
            if at == 10: bad[12] = 0
            self.assertFalse(module.dense_record(bad))
        self.assertFalse(module.dense_record(record[:-1]))

    def test_unknown_start_identity_is_not_an_induction_base(self):
        with self.assertRaisesRegex(ValueError, "START source identity"):
            module.start_records(b"not an original save")


if __name__ == "__main__": unittest.main()
