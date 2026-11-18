"""Only the new stream protocol/guards; no C, resources, audio, or guest."""
import io
import importlib.util
import os
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch

HERE = Path(__file__).resolve().parent
SPEC = importlib.util.spec_from_file_location("pcm_asset_oracle", HERE / "verify_pcm_assets.py")
oracle = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(oracle)


class Protocol(unittest.TestCase):
    def test_archive_literal(self):
        self.assertEqual(oracle.directory(b"LLLLLL" + struct.pack("<II", 14, 16) + b"abcd"), [(14, 16), (16, 18)])

    def test_resource_pin_before_directory(self):
        with self.assertRaisesRegex(ValueError, "resource pin"):
            oracle.catalog_inputs(b"bad", {"resource_sha256": "0" * 64})

    def test_exact_truncation(self):
        with self.assertRaisesRegex(ValueError, "truncated stdout"):
            oracle.exact(io.BytesIO(b"123"), 4)

    def test_exact_binary_control_bytes(self):
        raw = b"\x00\x0a\x0d\x1a\xff"
        self.assertEqual(oracle.exact(io.BytesIO(raw), len(raw)), raw)

    def test_packet_size(self):
        with self.assertRaisesRegex(ValueError, "packet size"):
            oracle.compare_packet(b"", b"")

    def test_wrong_sequence(self):
        a = bytearray(oracle.RECORD_SIZE)
        b = bytearray(a); b[4] = 1
        with self.assertRaisesRegex(ValueError, "byte=4"):
            oracle.compare_packet(a, b)

    def test_difference_in_preserved_DMA(self):
        a = bytearray(oracle.RECORD_SIZE)
        b = bytearray(a); b[-1] = 5
        with self.assertRaisesRegex(ValueError, "actual=0 expected=5"):
            oracle.compare_packet(a, b)

    def test_original_output(self):
        with tempfile.TemporaryDirectory() as d:
            original = Path(d) / "original"; original.mkdir()
            with self.assertRaisesRegex(ValueError, "outside resolved original/resource"):
                oracle.output_path(original / ".." / "original" / "new", original, original / "FDOTHER.DAT")

    def test_existing_output(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(ValueError):
                oracle.output_path(d, Path(d) / "original", Path(d) / "resource" / "FDOTHER.DAT")

    def test_wrong_declared_directory_still_protects_resource(self):
        with tempfile.TemporaryDirectory() as d:
            original = Path(d) / "actual-original"; original.mkdir()
            resource = original / "FDOTHER.DAT"; resource.write_bytes(b"owned fixture")
            with self.assertRaises(ValueError):
                oracle.output_path(original / "new", Path(d) / "misdeclared", resource)
            self.assertEqual(resource.read_bytes(), b"owned fixture")
            self.assertFalse((original / "new").exists())

    def test_resource_junction_resolves_before_protection(self):
        with tempfile.TemporaryDirectory() as d:
            original = Path(d) / "actual-original"; original.mkdir()
            resource = original / "FDOTHER.DAT"; resource.write_bytes(b"owned fixture")
            alias = Path(d) / "alias"
            if os.name == "nt":
                import _winapi
                _winapi.CreateJunction(str(original), str(alias))
            else:
                alias.symlink_to(original, target_is_directory=True)
            with self.assertRaises(ValueError):
                oracle.output_path(alias / "new", Path(d) / "misdeclared", alias / resource.name)
            self.assertFalse((original / "new").exists())

    def test_tool_root_original_is_always_protected(self):
        with tempfile.TemporaryDirectory() as d:
            root = Path(d) / "project"
            original = root / "original_game"; original.mkdir(parents=True)
            with patch.object(oracle, "__file__", str(root / "tools/testing/verify_pcm_assets.py")):
                with self.assertRaises(ValueError):
                    oracle.output_path(original / "new", Path(d) / "misdeclared",
                                       Path(d) / "elsewhere/FDOTHER.DAT")

    def test_cleanup_failures_do_not_replace_business_error(self):
        from unittest.mock import Mock
        child = Mock()
        child.poll.return_value = None
        child.terminate.side_effect = OSError("terminate denied")
        child.wait.side_effect = [OSError("wait failed"), OSError("final wait failed")]
        child.kill.side_effect = OSError("kill denied")
        report = dict(first_error={"error": "packet business mismatch"}, secondary_errors=[])
        closed, errors = oracle.close_owned(child)
        report.update(closed); report["secondary_errors"] += errors
        self.assertEqual(report["first_error"]["error"], "packet business mismatch")
        self.assertEqual([e["stage"] for e in errors],
                         ["close_terminate", "close_wait", "close_kill", "close_final_wait"])
        self.assertFalse(report["owned_ended"])
        with tempfile.TemporaryDirectory() as d:
            source = Path(d) / "source"; source.write_bytes(b"before")
            fact = oracle.freeze(source)[1]; source.write_bytes(b"after")
            self.assertEqual(len(oracle.check_facts([fact])), 1)
            self.assertIsNone(oracle.write_report(Path(d) / "report.json", report))

    def test_terminate_failure_kept_even_when_child_exits(self):
        from unittest.mock import Mock
        child = Mock()
        child.poll.side_effect = [None, 0]
        child.terminate.side_effect = OSError("terminate race")
        child.wait.return_value = 0
        closed, errors = oracle.close_owned(child)
        self.assertEqual(closed, {"exit_code": 0, "owned_ended": True})
        self.assertEqual([e["stage"] for e in errors], ["close_terminate"])

    def test_already_ended_child_cleanup_does_not_signal(self):
        from unittest.mock import Mock
        child = Mock(); child.poll.return_value = 0; child.wait.return_value = 0
        closed, errors = oracle.close_owned(child)
        self.assertEqual(closed, {"exit_code": 0, "owned_ended": True})
        self.assertEqual(errors, [])
        child.terminate.assert_not_called(); child.kill.assert_not_called()

    def test_late_guard(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "input"; p.write_bytes(b"before")
            fact = oracle.freeze(p)[1]; p.write_bytes(b"after")
            self.assertEqual(len(oracle.check_facts([fact])), 1)

    def test_report_failure_preserves_first_business_error(self):
        report = {"first_error": {"error": "business"}, "secondary_errors": []}
        with patch("pathlib.Path.open", side_effect=OSError("denied")):
            secondary = oracle.write_report("unused", report)
        self.assertEqual(report["first_error"], {"error": "business"})
        self.assertEqual(secondary["stage"], "report")

    def test_closed_formula_literal_and_joint_quantization(self):
        rows = {(1, 0): dict(bytes=2, offset=14, payload=bytes([0, 255]))}
        model = oracle.Model(rows)
        model.event((1, 0, 1, 0, 1, 0)); model.event((1, 1, 1, 0, 1, 0))
        self.assertEqual(model.event((3, 0, 0, 0, 0, 0)), 1)
        self.assertEqual(model.build[:4], [-51712, 51308, 51308, 0])
        self.assertEqual(model.dma[1][:4], bytes([0, 255, 255, 128]))
        self.assertEqual(model.dma[0], bytes([128]) * 2048)
        self.assertEqual((model.p[0]["position"], model.p[0]["status"], model.entered), (2, 2, 2))


    def portable_entry_refusal(self, manifest, source_args=()):
        # Only route/launch ordering is mocked here, not a source qualification.
        import json
        from types import SimpleNamespace
        build = oracle.helper("pcm_asset_build", [])
        with tempfile.TemporaryDirectory() as d:
            root = Path(d); original = root / "original"; original.mkdir()
            resource = original / "FDOTHER.DAT"; resource.write_bytes(b"owned resource")
            catalog = root / "catalog.json"; catalog.write_text(json.dumps({"sources": {}}))
            producer = root / "producer.json"; producer.write_text(json.dumps(manifest))
            args = SimpleNamespace(output=root / "new-output", original_directory=original,
                resource=resource, catalog=catalog, catalog_sha256=oracle.sha(catalog.read_bytes()),
                source=list(source_args), source_mode="portable-bytes", original_exe=original / "FD2.EXE",
                source_profile=root / "profile.json", source_profile_sha256="0"*64,
                project_root=root, build_manifest=producer, exe=root / "never.exe", timeout=1)
            stub = SimpleNamespace(portable=lambda *a: {"mock_source_gate_only": True})
            with patch.object(oracle, "helper", side_effect=lambda name, facts: stub if name == "pcm_asset_sources" else build), \
                 patch.object(oracle, "catalog_inputs", return_value={}), \
                 patch.object(oracle, "cases", return_value=[]), \
                 patch.object(oracle.subprocess, "Popen") as spawn, patch("builtins.print"):
                self.assertEqual(oracle.run(args), 1)
                spawn.assert_not_called()
            self.assertFalse(args.output.exists())

    def test_empty_producer_rejected_before_spawn(self):
        self.portable_entry_refusal({"schema": "fd2.f5.pcm-assets-build-inputs.v2", "files": []})

    def test_wrong_producer_schema_rejected_before_spawn(self):
        self.portable_entry_refusal({"schema": "not-a-producer", "files": []})

    def test_portable_source_never_falls_back_to_legacy_roles(self):
        self.portable_entry_refusal({}, source_args=("strict_image=missing",))


if __name__ == "__main__":
    unittest.main()
