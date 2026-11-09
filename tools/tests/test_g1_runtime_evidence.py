"""G1 只读运行机制证据：独立字节 oracle，报告缺项／篡改不能自证成功。"""
import copy
import hashlib
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "ida"))
from g1_runtime_extract import FUNCTIONS, GLOBALS, verify_image


class RuntimeEvidenceTest(unittest.TestCase):
    def setUp(self):
        self.image = bytearray(0x70000)
        self.report = {"functions": [], "globals": []}
        for va in FUNCTIONS:
            self.image[va] = 0x90
            self.report["functions"].append({
                "va": va, "chunks": [{"start": va, "end": va + 1}],
                "instructions": [{"va": va, "bytes": "90"}],
                "sha256_instruction_bytes": hashlib.sha256(b"\x90").hexdigest(),
            })
        for va in GLOBALS:
            self.report["globals"].append({"va": va, "initial_bytes": "00000000"})

    def test_complete_report_matches_independent_image(self):
        result = verify_image(self.report, self.image)
        self.assertEqual(result["instruction_bytes"], len(FUNCTIONS))

    def test_changed_instruction_and_its_new_digest_do_not_change_oracle(self):
        report = copy.deepcopy(self.report)
        function = report["functions"][0]
        function["instructions"][0]["bytes"] = "c3"
        function["sha256_instruction_bytes"] = hashlib.sha256(b"\xc3").hexdigest()
        with self.assertRaisesRegex(ValueError, "bytes mismatch"):
            verify_image(report, self.image)

    def test_missing_function_or_global_rejected(self):
        for key in ("functions", "globals"):
            report = copy.deepcopy(self.report)
            report[key].pop()
            with self.subTest(key=key), self.assertRaisesRegex(ValueError, "set/order"):
                verify_image(report, self.image)

    def test_empty_duplicate_outside_extent_and_bad_digest_rejected(self):
        for mode in ("empty", "duplicate", "outside", "digest", "missing_tail"):
            report = copy.deepcopy(self.report)
            function = report["functions"][0]
            if mode == "empty":
                function["instructions"] = []
            elif mode == "duplicate":
                function["instructions"] *= 2
            elif mode == "outside":
                function["chunks"][0]["end"] -= 1
            elif mode == "missing_tail":
                function["chunks"][0]["end"] += 1
            else:
                function["sha256_instruction_bytes"] = "0" * 64
            with self.subTest(mode=mode), self.assertRaises(ValueError):
                verify_image(report, self.image)

    def test_wrong_initial_global_rejected(self):
        self.report["globals"][0]["initial_bytes"] = "01000000"
        with self.assertRaisesRegex(ValueError, "global initial bytes"):
            verify_image(self.report, self.image)


if __name__ == "__main__":
    unittest.main()
