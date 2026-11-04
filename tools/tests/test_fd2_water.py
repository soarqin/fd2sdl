import importlib.util
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
SCRIPT = HERE / "audit_fd2_water.py"
if not SCRIPT.is_file(): SCRIPT = HERE.parent / "audit_fd2_water.py"
spec = importlib.util.spec_from_file_location("water_audit", SCRIPT)
audit = importlib.util.module_from_spec(spec)
spec.loader.exec_module(audit)


class WaterLEAuditTest(unittest.TestCase):
    def test_strict_le_owner_file_page_and_state_lifecycle(self):
        period, evidence = audit.water_table(audit.ROOT / "original_game/FD2.EXE")
        self.assertEqual(len(period), 48)
        self.assertEqual(evidence["read_size"], 93)
        self.assertEqual(evidence["pages"], [{"object_page": 0, "page_number": 68, "page_flags": 0, "file_offset": 0x79017}])
        self.assertEqual(evidence["initial_state_hex"], "000000")
        self.assertEqual(evidence["state_fixup_sources"]["0x60000"], ["0x4e326", "0x4e37a"])
        source = HERE / "field_visual.c"
        if not source.is_file(): source = audit.ROOT / "src/field_visual.c"
        self.assertTrue(audit.audit_c_table(source, period)["all_16_windows_compared"])

    def test_unknown_exe_never_reaches_le_parser_or_allocation(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "FD2.EXE"
            source.write_bytes(b"MZLE unknown")
            with patch.object(audit, "parse_le") as parse, patch.object(audit, "build_relbase_image") as build:
                with self.assertRaisesRegex(ValueError, "不是已确认版本"):
                    audit.water_table(source)
            parse.assert_not_called()
            build.assert_not_called()

    def test_c_table_mutation_or_missing_rows_is_rejected(self):
        period, _ = audit.water_table(audit.ROOT / "original_game/FD2.EXE")
        source = HERE / "field_visual.c"
        if not source.is_file(): source = audit.ROOT / "src/field_visual.c"
        text = source.read_text(encoding="utf-8")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "field_visual.c"
            for changed in (text.replace("{14,21,38}", "{13,21,38}", 1),
                            text.replace("field_water_rgb6[16][3]", "field_water_rgb6[15][3]")):
                path.write_text(changed, encoding="utf-8")
                with self.assertRaises(ValueError): audit.audit_c_table(path, period)

    def test_original_static_palette_is_not_any_legal_cycle(self):
        from run_original_comparison import archive_entry
        period, _ = audit.water_table(audit.ROOT / "original_game/FD2.EXE")
        initial = archive_entry((audit.ROOT / "original_game/FDOTHER.DAT").read_bytes(), 0)[224 * 3:240 * 3]
        self.assertEqual(bytes(value + 1 for value in initial), period)
        self.assertTrue(all(initial != (period + period)[phase * 3:phase * 3 + 48] for phase in range(16)))


if __name__ == "__main__": unittest.main()
