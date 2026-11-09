"""准备服务读集的来源／遗漏拒绝回归，不把合成值当原版语义证据。"""
import copy
import hashlib
import importlib.util
from pathlib import Path
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location(
    "g1_preparation_evidence", Path(__file__).resolve().parents[1] / "ida/g1_preparation_evidence.py")
evidence = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(evidence)


class PreparationEvidenceTest(unittest.TestCase):
    def setUp(self):
        self.image = bytes(0x64000)
        self.report = {"functions": [{"va": 0x2670E,
            "chunks": [{"start": 0x2670E, "end": 0x2670F,
                        "raw_hex": "00", "sha256": hashlib.sha256(b"\0").hexdigest()}],
            "instructions": [{"ea": 0x2670E}], "blocks": [{"start": 0x2670E}]}], "tables": []}
        self.patches = [patch.object(evidence, "TARGETS", (0x2670E,)),
                        patch.object(evidence, "RELOCATED_SHA256", hashlib.sha256(self.image).hexdigest())]
        for setting in self.patches:
            setting.start()
            self.addCleanup(setting.stop)

    def test_reference_and_source_identity(self):
        self.assertTrue(evidence.verify_bytes(self.report, self.image)["strict_byte_match"])
        with self.assertRaisesRegex(ValueError, "identity"):
            evidence.verify_bytes(self.report, self.image[:-1])

    def test_omitted_function_and_missing_graph_fail(self):
        for mutate in (lambda r: r["functions"].clear(),
                       lambda r: r["functions"][0].update(blocks=[])):
            report = copy.deepcopy(self.report)
            mutate(report)
            with self.assertRaises(ValueError):
                evidence.verify_bytes(report, self.image)

    def test_bad_bytes_and_range_fail(self):
        for mutate in (lambda c: c.update(raw_hex="01"),
                       lambda c: c.update(end=c["start"] + 2),
                       lambda c: c.update(start=0x4EF29, end=0x4EF2A)):
            report = copy.deepcopy(self.report)
            mutate(report["functions"][0]["chunks"][0])
            with self.assertRaises(ValueError):
                evidence.verify_bytes(report, self.image)

    def test_table_length_and_value_fail(self):
        for raw in ("0000", "01"):
            report = copy.deepcopy(self.report)
            report["tables"] = [{"va": 0x52397, "size": 1, "raw_hex": raw}]
            with self.assertRaisesRegex(ValueError, "table"):
                evidence.verify_bytes(report, self.image)


if __name__ == "__main__":
    unittest.main()
