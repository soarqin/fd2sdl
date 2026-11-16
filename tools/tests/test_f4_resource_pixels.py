"""Runner protocol/layout tests only; no C build, game assets or original CPU."""
import importlib.util
import io
import json
from pathlib import Path
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from unittest.mock import patch
import zlib

sys.dont_write_bytecode = True
SOURCE = Path(__file__).resolve().parents[1]/"verify_f4_resource_pixels.py"
SPEC = importlib.util.spec_from_file_location("f4_resource_pixels", SOURCE)
runner = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(runner)


class Process:
    def __init__(self, output=b"", code=0):
        self.stdin, self.stdout = io.BytesIO(), io.BytesIO(output)
        self.code, self.terminated, self.killed = code, False, False

    def poll(self):
        return self.code

    def wait(self, timeout):
        return self.code

    def terminate(self):
        self.terminated = True
        self.code = -1

    def kill(self):
        self.killed = True
        self.code = -1


class CatalogReads:
    """第二次读取若发生则返回另一份内容；不让ABA在测试里静默一致。"""
    def __init__(self, raw, replacement):
        self.raw, self.replacement = raw, replacement
        self.byte_reads = self.text_reads = 0

    def read_bytes(self):
        self.byte_reads += 1
        return self.raw if self.byte_reads == 1 else self.replacement

    def read_text(self, **kwargs):
        self.text_reads += 1
        return self.replacement.decode("utf-8")


class ResourcePixelRunnerTests(unittest.TestCase):
    def test_external_evidence_root_still_rejects_escape_existing_and_original(self):
        with tempfile.TemporaryDirectory() as directory:
            base = Path(directory).resolve()
            root, evidence = base/'project', base/'evidence'
            (root/'original_game').mkdir(parents=True)
            evidence.mkdir()
            new = evidence/'new-run'
            self.assertEqual(runner.checked_output(root, new, evidence), new)
            for path in (evidence, base/'escaped', root/'original_game/new-run'):
                with self.assertRaises(ValueError):
                    runner.checked_output(root, path, base if path.is_relative_to(root) else evidence)
            new.mkdir()
            with self.assertRaises(ValueError):
                runner.checked_output(root, new, evidence)

    @staticmethod
    def synthetic_catalog():
        # 维持真实14711行分母，但仅用人工单字节span；不读游戏或生成像素。
        rows = []
        for family, count in runner.COUNTS.items():
            for index in range(count):
                rows.append({"key": f"{family}:{index}", "family": family,
                    "file": "synthetic", "start": 0, "end": 1,
                    "source_sha256": runner.sha(b"S"),
                    "raw_x": 0, "raw_y": 0, "width": 1, "height": 1})
        return ("\n".join(json.dumps(row) for row in rows)+"\n").encode("utf-8")

    def test_catalog_hash_and_parse_use_one_frozen_read(self):
        raw = self.synthetic_catalog()
        path = CatalogReads(raw, b"changed after pinned read\n")
        with patch.object(runner, "CATALOG_SHA", runner.sha(raw)):
            rows = runner.read_catalog(path, {"synthetic": b"S"})
        self.assertEqual(len(rows), 14711)
        self.assertEqual((path.byte_reads, path.text_reads), (1, 0))

    def test_catalog_wrong_pin_rejects_before_decode_or_parse(self):
        path = CatalogReads(b"\xffnot UTF-8 or JSON", b"changed\n")
        with patch.object(runner, "CATALOG_SHA", "0"*64):
            with self.assertRaisesRegex(ValueError, "catalog identity"):
                runner.read_catalog(path, {})
        self.assertEqual((path.byte_reads, path.text_reads), (1, 0))

    def test_output_rejects_original_resolved_under_build_even_when_new(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory).resolve()
            physical = root/"build/f4/original-physical"
            physical.mkdir(parents=True)
            original_resolve = Path.resolve

            def resolve(path, *args, **kwargs):
                # resolve层模拟Windows junction；不请求建link权限或动真原件。
                if path.is_relative_to(root/"original_game"):
                    return physical/path.relative_to(root/"original_game")
                return original_resolve(path, *args, **kwargs)

            with patch.object(Path, "resolve", resolve):
                for requested in (root/"original_game", root/"original_game/new-child", physical,
                                  physical/"new-child", physical/"nested/new-child"):
                    with self.subTest(path=requested), self.assertRaisesRegex(ValueError, "original resource"):
                        runner.checked_output(root, requested)
                normal = root/"build/f4/separate-new-output"
                self.assertEqual(runner.checked_output(root, normal), normal)
                with patch.object(sys, "argv", ["runner", "--root", str(root), "--plan-only",
                                                 "--out", str(physical/"new-child")]):
                    with self.assertRaisesRegex(ValueError, "original resource"):
                        runner.main()
            self.assertFalse((physical/"new-child").exists())
            self.assertFalse((physical/"nested").exists())
            self.assertFalse(normal.exists())

    def test_bounded_compressed_layout(self):
        raw = b"\x00\xff"*17
        self.assertEqual(runner.bounded_unpack(zlib.compress(raw), len(raw)), raw)
        for packed, length in ((zlib.compress(raw)+b"extra", len(raw)),
                               (zlib.compress(raw), len(raw)-1),
                               (zlib.compress(raw)[:-1], len(raw))):
            with self.assertRaises(ValueError):
                runner.bounded_unpack(packed, length)

    def test_full_page_palette_and_both_masks_are_compared(self):
        raw = bytes(2*4+1536)
        for offset, field in ((0, "full_page"), (4, "temporary_palette"),
                              (772, "page_write_mask"), (776, "palette_write_mask")):
            changed = bytearray(raw)
            changed[offset] = 1
            result = runner.mismatch(raw, bytes(changed), 4)
            self.assertEqual((result["field"], result["offset"]), (field, 0))
        self.assertIsNone(runner.mismatch(raw, raw, 4))
        self.assertEqual(runner.mismatch(raw, raw[:-1], 4)["field"], "payload_size")

    def test_actual_mask_domain_and_reported_count(self):
        raw = bytes(4)+bytes(768)+bytes([1, 0, 1, 0])+bytes(768)
        header = {"page_bytes": 4, "pixel_addresses_written": 2, "palette_addresses_written": 0}
        runner.validate_actual(header, raw)
        header["pixel_addresses_written"] = 1
        with self.assertRaises(ValueError):
            runner.validate_actual(header, raw)
        header["pixel_addresses_written"] = 3
        with self.assertRaises(ValueError):
            runner.validate_actual(header, raw[:772]+bytes([2, 0, 1, 0])+bytes(768))

    def test_source_joined_position_preserves_opaque_zero_and_skip(self):
        # 人工协议例：只有第0/3点被原writer写，包含显式0；不是新游戏oracle。
        raw = bytes([0, 91, 92, 255])+bytes([165])*768+bytes([1, 0, 0, 1])+bytes(768)
        positioned = runner.positioned_expected(raw, {"width": 2, "height": 2, "raw_x": 7, "raw_y": 3})
        page, palette, mask, pmask = runner.split_payload(positioned, 64000)
        self.assertEqual((page[967], page[1288]), (0, 255))
        self.assertEqual((mask[967], mask[1288], sum(mask)), (1, 1, 2))
        self.assertEqual(page[968], (968*17+3+83) & 255)
        self.assertEqual((palette, pmask), (bytes([165])*768, bytes(768)))

    def test_source_guard_checks_each_original_instruction(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)/"source.json"
            source = {"image_sha256": runner.IMAGE_SHA, "functions": [{"va": 1,
                "chunks": [{"start": 1, "end": 3, "raw_hex": "1122", "sha256": runner.sha(b"\x11\x22")}],
                "instructions": [{"ea": 1, "size": 1, "raw_hex": "11"},
                                 {"ea": 2, "size": 1, "raw_hex": "22"}]}]}
            path.write_text(json.dumps(source), encoding="utf-8")
            expected = runner.sha(path.read_bytes())
            self.assertEqual(runner.source_guard(b"\x00\x11\x22", path, expected), {1})
            with self.assertRaises(ValueError):
                runner.source_guard(b"\x00\x11\x23", path, expected)

    def test_payload_second_read_must_still_match_pinned_compressed_bytes(self):
        with tempfile.TemporaryDirectory() as directory:
            report_path = Path(directory)/"report.json"
            raw = bytes(1544)
            packed = zlib.compress(raw)
            payload = Path(directory)/"case-00000.zbin"
            payload.write_bytes(packed)
            spec = {"payload": payload.name, "payload_sha256": runner.sha(packed), "page_bytes": 4}
            with patch.object(Path, "read_bytes", side_effect=[packed, zlib.compress(b"different")]) as reads:
                with self.assertRaisesRegex(ValueError, "original payload identity"):
                    runner.expected_payload(report_path, spec, {})
            self.assertEqual(reads.call_count, 2)

    def test_late_source_and_binary_guard_keep_primary_error(self):
        with tempfile.TemporaryDirectory() as directory:
            out = Path(directory)
            source, binary = out/"field.h", out/"probe.exe"
            source.write_bytes(b"source-A")
            binary.write_bytes(b"binary-A")
            guards = {str(path): runner.fact(path) for path in (source, binary)}
            source.write_bytes(b"source-B")
            binary.write_bytes(b"binary-B")
            report = {"ok": False, "error": "PRIMARY-business"}
            runner.finish_report(out, report, guards)
            saved = json.loads((out/"report.json").read_bytes())
            self.assertEqual(saved["error"], "PRIMARY-business")
            self.assertFalse(saved["guards_unchanged"])
            self.assertEqual(saved["secondary_errors"][0]["phase"], "late_guard")
            self.assertIn(str(source), saved["secondary_errors"][0]["error"])
            self.assertIn(str(binary), saved["secondary_errors"][0]["error"])

    def run_main_fixture(self, root, *, source_error=False, deny_diagnostics=False):
        # 只测main收据/异常顺序：人工image/resource、受控source适配器，不启动C。
        image = root/"synthetic-image.bin"
        image.write_bytes(b"I")
        original = root/"original_game"
        original.mkdir()
        (original/"ANI.DAT").write_bytes(b"R")
        owners = {0x4ebff, 0x4ec31, 0x4e22a, 0x4e1a6, 0x4ed7a,
                  0x2eb9f, 0x4e98d, 0x36fd3, 0x36ff4}
        case = {"request": ("scene", "glyph", 48, 0, 0),
                "row": {"key": "synthetic", "source_sha256": runner.sha(b"R")},
                "qualification": "synthetic main protocol only", "expected_source": "synthetic",
                "expected_packed": zlib.compress(bytes(1544), 1), "expected_size": 1544,
                "stride": 2, "height": 2}
        out = root/"build/f4/new-output"
        def fact(path):
            return {"path": str(path), "sha256": runner.EXE_SHA if path.name == "FD2.EXE" else "a"*64,
                    "size": 1, "mtime_ns": 1}
        original_write = Path.write_text
        def write(path, *args, **kwargs):
            if deny_diagnostics and path.name in ("failure.txt", "report.json"):
                raise OSError("secondary diagnostic denied")
            return original_write(path, *args, **kwargs)
        stream = io.StringIO()
        with patch.object(sys, "argv", ["runner", "--root", str(root), "--plan-only", "--image", str(image), "--out", str(out)]), \
             patch.object(runner, "fact", fact), patch.object(runner, "IMAGE_SHA", runner.sha(b"I")), \
             patch.object(runner, "RESOURCES", {"ANI.DAT": runner.sha(b"R")}), \
             patch.object(runner, "source_guard", side_effect=ValueError("PRIMARY-business") if source_error else None,
                          return_value=owners), \
             patch.object(runner, "read_catalog", return_value={}), \
             patch.object(runner, "prepare_cases", return_value=[case]), \
             patch.object(runner, "Probe") as probe, patch.object(Path, "write_text", write), redirect_stdout(stream):
            status = runner.main()
            probe.assert_not_called()
        return status, json.loads(stream.getvalue()), out

    def test_main_plan_receipt_and_no_process(self):
        with tempfile.TemporaryDirectory() as directory:
            status, summary, out = self.run_main_fixture(Path(directory).resolve())
            report = json.loads((out/"report.json").read_bytes())
            self.assertEqual(status, 0)
            self.assertTrue(summary["ok"] and report["guards_unchanged"])
            self.assertEqual(report["actual_C_cases_executed"], 0)
            self.assertEqual(report["C_loaded_input_bindings"], {})

    def test_main_failure_and_report_write_errors_do_not_replace_primary(self):
        with tempfile.TemporaryDirectory() as directory:
            status, summary, out = self.run_main_fixture(Path(directory).resolve(), source_error=True, deny_diagnostics=True)
            self.assertEqual(status, 1)
            self.assertIn("ValueError: PRIMARY-business", summary["error"])
            self.assertEqual([item["phase"] for item in summary["secondary_errors"]], ["write_failure", "write_report"])
            self.assertFalse((out/"failure.txt").exists() or (out/"report.json").exists())

    def test_C_input_buffer_receipt_mismatch_rejects_before_any_request(self):
        actual = b"B"
        header = {"schema": "fd2-f4-input-v1", "input_name": "synthetic", "payload_bytes": 1}
        process = Process(json.dumps(header).encode()+b"\n"+actual)
        with tempfile.TemporaryDirectory() as directory, patch.object(runner.subprocess, "Popen", return_value=process):
            probe = runner.Probe(Path("unused.exe"), Path(directory), Path(directory), "fake", 1, {"synthetic": b"A"})
            with self.assertRaisesRegex(ValueError, "C loaded input differs"):
                probe.request({"request": ("scene", "figani", 0, 0, 0), "stride": 2, "height": 2})
            self.assertEqual(process.stdin.getvalue(), b"")
            probe.close(True)

    def test_C_input_buffer_receipt_binds_same_bytes_before_request(self):
        resource = b"\x00\x0a\x0d\xff"
        resource_header = {"schema": "fd2-f4-input-v1", "input_name": "synthetic", "payload_bytes": len(resource)}
        raw = bytes(1544)
        header = {"schema": "fd2-f4-reader-v1", "status": 0, "payload_bytes": len(raw),
                  "op": "figani", "a": 0, "b": 0, "variant": 0, "stride": 2, "height": 2,
                  "page_bytes": 4, "owner_guard": True}
        output = json.dumps(resource_header).encode()+b"\n"+resource+json.dumps(header).encode()+b"\n"+raw
        process = Process(output)
        with tempfile.TemporaryDirectory() as directory, patch.object(runner.subprocess, "Popen", return_value=process):
            probe = runner.Probe(Path("unused.exe"), Path(directory), Path(directory), "fake", 1, {"synthetic": resource})
            self.assertEqual(probe.request({"request": ("scene", "figani", 0, 0, 0), "stride": 2, "height": 2})[1], raw)
            self.assertEqual(probe.bound_inputs["synthetic"]["sha256"], runner.sha(resource))
            self.assertTrue(probe.ready)
            probe.close(False)

    def test_protocol_success_reads_raw_binary_without_text_conversion(self):
        raw = bytes([0, 10, 13, 255])+bytes(768)+bytes([1])*4+bytes(768)
        header = {"schema": "fd2-f4-reader-v1", "status": 0, "payload_bytes": len(raw),
                  "op": "figani", "a": 0, "b": 0, "variant": 0, "stride": 2, "height": 2,
                  "page_bytes": 4, "owner_guard": True}
        process = Process(json.dumps(header).encode()+b"\n"+raw)
        with tempfile.TemporaryDirectory() as directory, patch.object(runner.subprocess, "Popen", return_value=process):
            probe = runner.Probe(Path("unused.exe"), Path(directory), Path(directory), "fake", 1, {})
            actual_header, actual = probe.request({"request": ("scene", "figani", 0, 0, 0), "stride": 2, "height": 2})
            self.assertEqual((actual_header, actual), (header, raw))
            self.assertEqual(process.stdin.getvalue(), b"figani 0 0 0\n")
            probe.close(False)

    def test_protocol_rejects_short_error_extra_and_oversized_output(self):
        outputs = [b'{"schema":"fd2-f4-reader-v1","status":1,"payload_bytes":0}\n',
                   b'{"schema":"fd2-f4-reader-v1","status":0,"payload_bytes":1}\n',
                   b'{"schema":"fd2-f4-reader-v1","status":0,"payload_bytes":9999999}\n',
                   b"x"*2049]
        for index, output in enumerate(outputs):
            with self.subTest(index=index), tempfile.TemporaryDirectory() as directory:
                process = Process(output)
                with patch.object(runner.subprocess, "Popen", return_value=process):
                    probe = runner.Probe(Path("unused.exe"), Path(directory), Path(directory), "fake", 1, {})
                    with self.assertRaises((ValueError, json.JSONDecodeError)):
                        probe.request({"request": ("scene", "figani", 0, 0, 0), "stride": 2, "height": 2})
                    probe.close(True)

    def test_extra_packet_and_nonzero_exit_are_not_success(self):
        output = b'{"schema":"fd2-f4-reader-v1","status":0,"payload_bytes":1}\nX'
        for suffix, code in ((output, 0), (b"", 1)):
            with self.subTest(code=code), tempfile.TemporaryDirectory() as directory:
                process = Process(output+suffix, code)
                with patch.object(runner.subprocess, "Popen", return_value=process):
                    probe = runner.Probe(Path("unused.exe"), Path(directory), Path(directory), "fake", 1, {})
                    probe.thread.join(1)
                    self.assertEqual(probe.replies.get_nowait()[0], "inputs_bound")
                    probe.replies.get_nowait()
                    with self.assertRaises(ValueError):
                        probe.close(False)


if __name__ == "__main__":
    unittest.main()
