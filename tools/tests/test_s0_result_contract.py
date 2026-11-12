"""S0原码探针的缺源、共享尾、向量和差分失败保护；不把替身当原运行。"""
import copy
import hashlib
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "ida"))
import verify_s0_result_contract as oracle


class SourceGuardsTest(unittest.TestCase):
    def fixture(self):
        image = bytes(0x64000)
        sha = hashlib.sha256(image).hexdigest()
        functions = []
        for va in oracle.TARGETS:
            chunks = [{"start": a, "end": b, "raw_hex": image[a:b].hex(),
                       "sha256": hashlib.sha256(image[a:b]).hexdigest()}
                      for a, b in oracle.CHUNKS[va]]
            functions.append({"va": va, "chunks": chunks, "instructions": [1], "blocks": [1]})
        tail = {"start": 0x22BBE, "end": 0x22BC6, "raw_hex": image[0x22BBE:0x22BC6].hex(),
                "sha256": hashlib.sha256(image[0x22BBE:0x22BC6]).hexdigest()}
        return image, {"schema": "fd2-s0-result-domain-v1", "image_sha256": sha,
                       "functions": functions, "load_return_shared_tail": tail}, sha

    def test_missing_or_changed_source_rejected(self):
        image, readset, sha = self.fixture()
        with patch.object(oracle, "RELOCATED_SHA256", sha):
            self.assertEqual(set(oracle.validate_source(image, readset)), set(oracle.TARGETS))
            for field in ("instructions", "blocks", "chunks"):
                bad = copy.deepcopy(readset); bad["functions"][0][field] = []
                with self.assertRaises(ValueError): oracle.validate_source(image, bad)
            bad = copy.deepcopy(readset); bad["functions"].pop()
            with self.assertRaises(ValueError): oracle.validate_source(image, bad)
            with self.assertRaises(ValueError): oracle.validate_source(image+b"x", readset)

    def test_shared_tail_and_chunk_bytes_required(self):
        image, readset, sha = self.fixture()
        with patch.object(oracle, "RELOCATED_SHA256", sha):
            bad = copy.deepcopy(readset)
            next(f for f in bad["functions"] if f["va"] == 0x205B4)["chunks"].pop()
            with self.assertRaises(ValueError): oracle.validate_source(image, bad)
            bad = copy.deepcopy(readset); bad["load_return_shared_tail"]["end"] -= 1
            with self.assertRaises(ValueError): oracle.validate_source(image, bad)
            bad = copy.deepcopy(readset); bad["functions"][0]["chunks"][0]["sha256"] = "0"*64
            with self.assertRaises(ValueError): oracle.validate_source(image, bad)

    def test_vectors_preserve_hp_flags_and_stage_domain(self):
        rows = list(oracle.vector_inputs())
        self.assertEqual(len(rows), 576)
        self.assertEqual({s for s, _ in rows}, {0, 1})
        self.assertEqual({r[5] for _, r in rows}, {0, 1, 0x80, 0x81})
        self.assertEqual({int.from_bytes(r[0x40:0x42], "little") for _, r in rows}, {0, 1, 65535})
        self.assertTrue(all(len(r) == 880 for _, r in rows))

    def test_differential_mismatch_or_missing_line_fails(self):
        cases = [{"stage": 0, "result_sdl": 1}, {"stage": 1, "result_sdl": 2}]
        self.assertEqual(oracle.validate_c_rows(cases, ["0 1", "1 2"]), 2)
        for rows in (["0 1"], ["0 1", "1 1"], ["0 1", "1 2", "extra"]):
            with self.assertRaises(ValueError): oracle.validate_c_rows(cases, rows)

    def test_c_probe_identity_bound_and_change_rejected(self):
        cases = [{"stage": 0, "count": 1, "records_hex": "00", "result_sdl": 0}]
        with tempfile.TemporaryDirectory() as directory:
            probe = Path(directory) / "probe.exe"
            probe.write_bytes(b"probe")
            reply = SimpleNamespace(returncode=0, stdout="0 0\n", stderr="")
            with patch.object(oracle.subprocess, "run", return_value=reply):
                result = oracle.compare_c(probe, cases)
                self.assertEqual(result["vectors"], 1)
                self.assertEqual(result["sha256"], hashlib.sha256(b"probe").hexdigest())
            def changed(*args, **kwargs):
                probe.write_bytes(b"changed")
                return reply
            with patch.object(oracle.subprocess, "run", side_effect=changed):
                with self.assertRaisesRegex(ValueError, "probe changed"):
                    oracle.compare_c(probe, cases)


if __name__ == "__main__": unittest.main()
