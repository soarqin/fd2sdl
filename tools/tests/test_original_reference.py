"""F1 独立参照身份、配置覆盖和只读输出边界回归。"""
import copy
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import original_reference as reference

# setUp 会清空 os.environ；Windows 子进程缺 SystemRoot 时 CreateProcess 失败，
# 故在导入时保留创建 junction 所需的最小系统环境。
_WINDOWS_CHILD_ENV = {key: os.environ[key] for key in ("SystemRoot", "ComSpec")
                      if key in os.environ}


def digest(data):
    return hashlib.sha256(data).hexdigest()


class OriginalReferenceTest(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.original = self.root / "original"
        self.original.mkdir()
        self.pin = copy.deepcopy(reference.load_json(reference.REFERENCE))
        self.exe = self.root / "emulator" / "dosbox-x.exe"
        self.exe.parent.mkdir()
        self.exe.write_bytes(b"independent pinned emulator")
        self.pin["dosbox"]["sha256"] = digest(self.exe.read_bytes())
        glyphs = bytes(range(32)) * 2
        entries = [b"not-font"] * 4 + [glyphs]
        offset = 6 + len(entries) * 4
        offsets = []
        for entry in entries:
            offsets.append(offset)
            offset += len(entry)
        archive = b"LLLLLL" + struct.pack("<5I", *offsets) + b"".join(entries)
        for name, expected in self.pin["files"].items():
            data = (b"DRIVER SBPRO2.MDI\r\n" if name == "MDI.INI" else
                    b"DRIVER SB16.DIG\r\n" if name == "DIG.INI" else
                    archive if name == "FDOTHER.DAT" else (name + " fixture").encode())
            (self.original / name).write_bytes(data)
            expected.update(size=len(data), sha256=digest(data))
        self.pin["font"].update(size=len(glyphs), sha256=digest(glyphs), glyph_count=2)
        self.bootstrap = self.root / "user.conf"
        bootstrap_sections = {}
        for key, value in {**self.pin["configuration_source"]["inherited_settings"],
                           "cpu.core": "auto"}.items():
            section, option = key.split(".", 1)
            bootstrap_sections.setdefault(section, []).append(f"{option}={value}")
        bootstrap_raw = ("\n".join(f"[{s}]\n" + "\n".join(rows)
                                    for s, rows in bootstrap_sections.items()) +
                         "\n[autoexec]\necho bootstrap-only\n").encode()
        self.bootstrap.write_bytes(bootstrap_raw)
        self.pin["configuration_source"]["bootstrap"].update(
            size=len(bootstrap_raw), sha256=digest(bootstrap_raw))
        self.pin_path = self.root / "reference.json"
        self.pin_path.write_text(json.dumps(self.pin), encoding="utf-8")
        self.addCleanup(mock.patch.stopall)
        mock.patch.object(reference, "REFERENCE", self.pin_path).start()
        mock.patch.dict(os.environ, {}, clear=True).start()
        self.config = self.root / "dosbox.conf"
        sections = {}
        for key, value in self.pin["explicit_settings"].items():
            section, option = key.split(".", 1)
            sections.setdefault(section, []).append(f"{option}={value}")
        self.config.write_text("\n".join(f"[{s}]\n" + "\n".join(rows)
                                       for s, rows in sections.items()) +
                               '\n[autoexec]\nmount c "isolated game"\nc:\nFD2.EXE\nexit\n',
                               encoding="utf-8")

    def build(self):
        return reference.build(self.original, self.exe, [self.config], [], self.bootstrap)

    def test_roundtrip_and_font_identity_do_not_claim_runtime(self):
        receipt = self.build()
        self.assertEqual(receipt["font"]["glyph_count"], 2)
        self.assertEqual(receipt["ini_driver_selection"],
                         {"MDI.INI": "SBPRO2.MDI", "DIG.INI": "SB16.DIG"})
        self.assertFalse(receipt["scope"]["runtime_executed"])
        self.assertFalse(receipt["scope"]["effective_default_values_measured"])
        self.assertTrue(receipt["scope"]["configuration_chain_verified"])
        self.assertNotIn("echo bootstrap-only", receipt["autoexec_commands"])
        self.assertEqual(receipt["derived_effective_settings"]["cpu.core"], "normal")
        self.assertIsNone(receipt["provenance"]["release_label"])
        path = self.root / "receipt.json"
        reference.write_new(path, receipt)
        self.assertEqual(reference.verify(path), receipt)

    def test_changed_missing_resource_and_emulator_fail(self):
        target = self.original / "FDMUS.DAT"
        original = target.read_bytes()
        target.write_bytes(original + b"changed")
        with self.assertRaisesRegex(ValueError, "FDMUS"):
            self.build()
        target.unlink()
        with self.assertRaises(OSError):
            self.build()
        target.write_bytes(original)
        self.exe.write_bytes(b"another executable")
        with self.assertRaisesRegex(ValueError, "DOSBox-X"):
            self.build()

    def test_config_order_cli_override_and_wrong_cycles(self):
        overlay = self.root / "overlay.conf"
        overlay.write_text("[cpu]\ncycles=fixed 18000\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "cpu.cycles"):
            reference.build(self.original, self.exe, [self.config, overlay], [], self.bootstrap)
        receipt = reference.build(self.original, self.exe, [self.config, overlay],
                                  ["cpu cycles=fixed 20000"], self.bootstrap)
        self.assertEqual(receipt["declared_settings"]["cpu.cycles"], "fixed 20000")
        with self.assertRaisesRegex(ValueError, "-set"):
            reference.build(self.original, self.exe, [self.config], ["cycles 20000"], self.bootstrap)

    def test_unfrozen_environment_is_rejected_without_values(self):
        with mock.patch.dict(os.environ, {"DOSBOX_CPU_CYCLES": "private-value"}):
            with self.assertRaisesRegex(ValueError, "DOSBOX_CPU_CYCLES") as error:
                self.build()
            self.assertNotIn("private-value", str(error.exception))

    def test_duplicate_or_missing_config_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "有序"):
            reference.build(self.original, self.exe, [], [], self.bootstrap)
        with self.assertRaisesRegex(ValueError, "不重复"):
            reference.build(self.original, self.exe, [self.config, self.config], [], self.bootstrap)
        self.config.write_text("[cpu]\ncycles=x\ncycles=y\n", encoding="utf-8")
        with self.assertRaises(reference.configparser.Error):
            self.build()

    def test_colon_or_multiline_assignment_cannot_claim_effective_core(self):
        original = self.config.read_bytes()
        # 官方只认逐行'='：这两种Python配置语法都不会在DOSBox设成normal。
        for replacement in (b"core:normal", b"core=\n  normal"):
            with self.subTest(replacement=replacement):
                self.config.write_bytes(original.replace(b"core=normal", replacement))
                with self.assertRaises(reference.configparser.Error):
                    self.build()

    def test_activated_conditional_config_line_is_rejected_not_ignored(self):
        original = self.config.read_bytes()
        for prefix in (b"#DOSBOX-X:", b"#DOSBOX-X-ADV:"):
            with self.subTest(prefix=prefix):
                self.config.write_bytes(original.replace(b"core=normal",
                    b"core=normal\n" + prefix + b"core=auto"))
                with self.assertRaisesRegex(ValueError, "DOSBOX-X.*配置行"):
                    self.build()

    def test_utf8_bom_is_not_silently_removed_from_actual_config(self):
        self.config.write_bytes(b"\xef\xbb\xbf" + self.config.read_bytes())
        with self.assertRaisesRegex(ValueError, "BOM"):
            self.build()

    def test_ini_path_escape_or_unpinned_selection_fails(self):
        for selection in ("../SBPRO2.MDI", "OTHER.MDI"):
            target = self.original / "MDI.INI"
            data = ("DRIVER " + selection).encode()
            target.write_bytes(data)
            pin = copy.deepcopy(self.pin)
            pin["files"]["MDI.INI"].update(size=len(data), sha256=digest(data))
            self.pin_path.write_text(json.dumps(pin), encoding="utf-8")
            with self.assertRaises(ValueError):
                self.build()

    def test_modified_receipt_cannot_omit_resources_or_change_scope(self):
        for change in (lambda r: r["resources"].pop("FD2.EXE"),
                       lambda r: r["scope"].update(runtime_executed=True)):
            receipt = self.build()
            change(receipt)
            path = self.root / "altered.json"
            path.write_text(json.dumps(receipt), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "收据字段"):
                reference.verify(path)

    def test_changed_frozen_config_fails_verify(self):
        path = self.root / "receipt.json"
        reference.write_new(path, self.build())
        self.config.write_text(self.config.read_text() + "\n# changed\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "收据字段"):
            reference.verify(path)

    def test_output_protects_original_emulator_and_existing_receipt(self):
        receipt = self.build()
        for path in (self.original / "receipt.json", self.exe.parent / "receipt.json"):
            with self.assertRaisesRegex(ValueError, "输出不得"):
                reference.write_new(path, receipt)
            self.assertFalse(path.exists())
        path = self.root / "receipt.json"
        reference.write_new(path, receipt)
        before = path.read_bytes()
        with self.assertRaises(FileExistsError):
            reference.write_new(path, receipt)
        self.assertEqual(path.read_bytes(), before)

    def test_json_duplicate_keys_are_not_silently_overwritten(self):
        path = self.root / "bad.json"
        path.write_text('{"schema":1,"schema":2}', encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "重复 JSON"):
            reference.load_json(path)

    def test_unknown_reference_schema_is_not_interpreted_as_current(self):
        self.pin["schema"] = "fd2-original-reference-future"
        self.pin_path.write_text(json.dumps(self.pin), encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "清单 schema"):
            self.build()

    def test_output_through_directory_alias_cannot_enter_original(self):
        link = self.root / "original-alias"
        if os.name == "nt":
            subprocess.run([_WINDOWS_CHILD_ENV.get("ComSpec", "cmd"), "/c", "mklink", "/J",
                            str(link), str(self.original)],
                           check=True, capture_output=True, env=_WINDOWS_CHILD_ENV)
            self.assertEqual(link.lstat().st_reparse_tag, 0xA0000003)
        else:
            link.symlink_to(self.original, target_is_directory=True)
        try:
            self.assertEqual(link.resolve(), self.original.resolve())
            with self.assertRaisesRegex(ValueError, "输出不得"):
                reference.write_new(link / "receipt.json", self.build())
            self.assertFalse((self.original / "receipt.json").exists())
        finally:
            # 只删除本测试创建的链接本身；不递归、不解析为原目录再删除。
            if os.name == "nt":
                os.rmdir(link)
            else:
                link.unlink()

    def test_config_change_after_parse_cannot_hash_different_declared_bytes(self):
        real_parse = reference.parse_config
        target = self.config.read_bytes()

        def changed_after_parse(raw):
            result = real_parse(raw)
            if raw == target:
                self.config.write_bytes(raw.replace(b"fixed 20000", b"fixed 18000"))
            return result

        with mock.patch.object(reference, "parse_config", side_effect=changed_after_parse):
            with self.assertRaisesRegex(ValueError, "冻结期间输入改变"):
                self.build()

    def test_later_config_read_cannot_hide_earlier_resource_change(self):
        real_parse = reference.parse_config

        def change_earlier_input(raw):
            (self.original / "FD2.EXE").write_bytes(b"changed after its first read")
            return real_parse(raw)

        with mock.patch.object(reference, "parse_config", side_effect=change_earlier_input):
            with self.assertRaisesRegex(ValueError, "冻结期间输入改变"):
                self.build()

    def test_missing_or_changed_bootstrap_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "缺少.*bootstrap"):
            reference.build(self.original, self.exe, [self.config], [])
        self.bootstrap.write_bytes(self.bootstrap.read_bytes() + b"\n# changed\n")
        with self.assertRaisesRegex(ValueError, "bootstrap 用户配置身份"):
            self.build()

    def test_bootstrap_change_after_later_input_parse_is_rejected(self):
        real_parse = reference.parse_config
        target = self.config.read_bytes()

        def change_bootstrap(raw):
            result = real_parse(raw)
            if raw == target:
                self.bootstrap.write_bytes(self.bootstrap.read_bytes() + b"\n# changed\n")
            return result

        with mock.patch.object(reference, "parse_config", side_effect=change_bootstrap):
            with self.assertRaisesRegex(ValueError, "冻结期间输入改变"):
                self.build()

    def test_reversed_final_config_order_cannot_hide_overwrite(self):
        overlay = self.root / "late.conf"
        overlay.write_text("[cpu]\ncore=auto\n", encoding="utf-8")
        with self.assertRaisesRegex(ValueError, "cpu.core"):
            reference.build(self.original, self.exe, [self.config, overlay], [], self.bootstrap)
        receipt = reference.build(self.original, self.exe, [overlay, self.config], [], self.bootstrap)
        self.assertEqual(receipt["derived_effective_settings"]["cpu.core"], "normal")


if __name__ == "__main__":
    unittest.main()
