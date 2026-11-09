"""G1 战斗读集的来源与缺项拒绝回归；合成字节不作原版语义证据。"""
import hashlib
import importlib.util
from pathlib import Path
import sys
from types import ModuleType, SimpleNamespace
import unittest
from unittest.mock import patch


SPEC = importlib.util.spec_from_file_location(
    "g1_battle_readset", Path(__file__).resolve().parents[1] / "ida/g1_battle_readset.py")
evidence = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(evidence)


class BattleReadsetTest(unittest.TestCase):
    def setUp(self):
        self.original = b"synthetic identity"
        self.executable = SimpleNamespace(read_bytes=lambda: self.original)
        self.source = b"\x34\x00\x00\x00"
        self.actual = b"\x34\x00\x01\x00"
        fixup_module = ModuleType("dump_fd2_fixup_table")
        fixup_module.collect_internal_offset_fixups = lambda _: {0x51D01: (1, 0x34, 0x10034)}
        source_module = ModuleType("verify_unit_tables")
        source_module.EXE_SHA256 = hashlib.sha256(self.original).hexdigest()
        source_module.source_range = lambda _data, _va, _size: self.source
        self.settings = [patch.dict(sys.modules, {
            "dump_fd2_fixup_table": fixup_module, "verify_unit_tables": source_module}),
            patch.object(evidence, "TABLES", (("test_table", 0x51D01, 4),)),
            patch.object(evidence, "FUNCTIONS", ())]
        for setting in self.settings:
            setting.start()
            self.addCleanup(setting.stop)
        self.db = SimpleNamespace(bytes=SimpleNamespace(
            get_bytes_at=lambda *_args, **_kwargs: self.actual,
            is_value_initialized_at=lambda _va: True))

    def collect(self):
        return evidence.collect(self.db, Path(__file__).resolve().parents[2], self.executable)

    def test_raw_and_relocated_are_distinct_and_independently_checked(self):
        report = self.collect()
        row = report["tables"]["test_table"]
        self.assertEqual(row["raw_hex"], self.source.hex())
        self.assertEqual(row["domain_hex"], self.actual.hex())
        self.assertTrue(row["independent_source_match"])

    def test_wrong_executable_identity_fails_before_domain_access(self):
        self.original += b"changed"
        self.db.bytes.get_bytes_at = lambda *_args, **_kwargs: self.fail("Domain must remain untouched")
        with self.assertRaisesRegex(ValueError, "identity"):
            self.collect()

    def test_changed_domain_byte_and_truncated_read_fail(self):
        for actual in (b"\x35\x00\x01\x00", self.actual[:-1]):
            self.actual = actual
            with self.assertRaisesRegex(ValueError, "bytes differ"):
                self.collect()

    def test_uninitialized_byte_fails(self):
        self.db.bytes.is_value_initialized_at = lambda va: va != 0x51D03
        with self.assertRaisesRegex(ValueError, "uninitialized"):
            self.collect()

    def test_missing_function_and_nonentry_are_rejected(self):
        with patch.object(evidence, "FUNCTIONS", (0x10010,)):
            for function in (None, SimpleNamespace(start_ea=0x10000)):
                self.db.functions = SimpleNamespace(get_at=lambda _va: function)
                with self.assertRaisesRegex(ValueError, "canonical function entry"):
                    self.collect()

    def test_code_mismatch_fails_before_disassembly_claim(self):
        with patch.object(evidence, "FUNCTIONS", (0x10010,)):
            function = SimpleNamespace(start_ea=0x10010)
            self.db.functions = SimpleNamespace(get_at=lambda _va: function,
                get_chunks=lambda _: [SimpleNamespace(start_ea=0x10010, end_ea=0x10014)])
            with self.assertRaisesRegex(ValueError, "Domain code differ"):
                self.collect()

    def test_empty_instruction_or_graph_coverage_is_not_success(self):
        with patch.object(evidence, "FUNCTIONS", (0x10010,)):
            self.db.bytes.get_bytes_at = lambda va, **_: self.source if va == 0x10010 else self.actual
            self.db.functions = SimpleNamespace(get_at=lambda _: SimpleNamespace(start_ea=0x10010),
                get_chunks=lambda _: [SimpleNamespace(start_ea=0x10010, end_ea=0x10014)],
                get_instructions=lambda _: [], get_flowchart=lambda _: [])
            with self.assertRaisesRegex(ValueError, "incomplete function evidence"):
                self.collect()


if __name__ == "__main__":
    unittest.main()
