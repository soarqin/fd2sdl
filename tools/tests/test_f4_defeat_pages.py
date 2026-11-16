"""Pure contracts for the bounded oracle; no original/C execution or DB."""
import importlib.util
from pathlib import Path
import struct
import unittest
from unittest.mock import patch
import argparse
from contextlib import ExitStack, redirect_stderr, redirect_stdout
import io
import json
import os
import sys
import tempfile

PATH = Path(__file__).resolve().parents[1] / "verify_f4_defeat_pages.py"
SPEC = importlib.util.spec_from_file_location("f4_defeat_pages", PATH)
M = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(M)


class F4DefeatPagesContracts(unittest.TestCase):
    def test_original_literal_zero_c0_and_run(self):
        w, h, pixels = M.decode_lmi(struct.pack("<HH", 2, 2) + bytes([0, 0xc0, 0xc2, 7]))
        self.assertEqual((w, h, bytes(pixels)), (2, 2, bytes([0, 0xc0, 7, 7])))
        with self.assertRaises(ValueError):
            M.decode_lmi(struct.pack("<HH", 1, 1) + bytes([0xc2, 7]))
        with self.assertRaises(ValueError):
            M.decode_lmi(struct.pack("<HH", 1, 1) + bytes([0xc1]))

    def test_base_update_feedback_is_next_restore_frame(self):
        # Explicit original 4ebab inputs: one pixel in each of twelve images.
        images = [(1, 1, bytes([i + 1])) for i in range(12)]
        states = list(M.expected_states(images, 31, 0))
        self.assertEqual((states[0][0][0], states[0][1][0]), (5, 1))
        self.assertEqual((states[5][0][0], states[5][1][0]), (5, 6))
        self.assertEqual((states[6][0][0], states[6][1][0]), (6, 7))
        self.assertEqual((states[11][0][0], states[11][1][0]), (11, 12))

    def test_later_normal_app_anchors_are_not_capped_at31(self):
        images = [(1, 1, b"\x7b")] * 12
        app, base, _ = next(M.expected_states(images, 35, 1))
        for raw in range(31, 35):
            self.assertEqual(app[(90 + 24 * (raw // 7)) * 456 + 96 + 24 * (raw % 7)], 123)
        self.assertEqual(base[0], 123)

    def test_opaque_zero_is_not_a_write(self):
        page = bytearray([19]) * M.PAGE
        M.blit(page, (2, 1, b"\x00\x29"), 0, 0)
        self.assertEqual(page[:3], bytes([19, 41, 19]))
        with self.assertRaises(ValueError):
            M.blit(page, (2, 1, b"\x01\x01"), 455, 0)

    def test_header_rejects_truncation_and_unqualified_count(self):
        with self.assertRaises(ValueError):
            M.parameters(b"F4DFX001")
        actual = M.HEADER.pack(b"F4DFX001", 36, 0, M.PAGE, 12) + bytes(12 * (M.PAGE * 2 + M.VGA))
        with self.assertRaises(ValueError):
            M.parameters(actual)


class F4VerifyMainLoopContracts(unittest.TestCase):
    """Synthetic envelopes only; no original files, images, C binary or CPU.

    Expected full artifacts are written from fixed one-pixel source facts,
    without calling expected_states/blit or the changed C implementation.
    """
    def setUp(self):
        self.stack = ExitStack()
        self.addCleanup(self.stack.close)
        temporary = self.stack.enter_context(tempfile.TemporaryDirectory(prefix="fd2-f4-verify-"))
        self.root = Path(temporary)
        original = self.root / "synthetic-original"
        original.mkdir()
        image = bytearray(0x4ec68)
        def function(va, byte):
            image[va] = byte
            raw = bytes([byte])
            return {"va": va, "instructions": [{"synthetic_identity_only": True}],
                    "chunks": [{"start": va, "end": va + 1,
                                "bytes": raw.hex(), "sha256": M.digest(raw)}]}
        control = json.dumps({"functions": [function(0x1db65, 11)]}).encode()
        pixel = json.dumps({"functions": [function(0x4ebab, 22), function(0x4ec66, 33)]}).encode()
        # Six-entry DAT; entry5 has80 tiny, strictly increasing LMI payloads.
        payloads = [struct.pack("<HHB", 1, 1, i - 67 if i >= 68 else 17) for i in range(80)]
        offsets = [326 + 5 * i for i in range(80)]
        lmi = b"LMI1" + struct.pack("<H80I", 80, *offsets) + b"".join(payloads)
        blocks = [b"x"] * 5 + [lmi]
        dat_offsets, at = [], 6 + 7 * 4
        for block in blocks:
            dat_offsets.append(at)
            at += len(block)
        resource = b"LLLLLL" + struct.pack("<7I", *dat_offsets, at) + b"".join(blocks)
        exe = b"synthetic validation envelope, never executable"
        self.args = argparse.Namespace(
            image=self.root / "image.fixture", control_source=self.root / "control.json",
            pixel_source=self.root / "pixel.json", original=original,
            actual=self.root / "actual.bin", producer=self.root / "producer.fixture",
            producer_source=[self.root / "producer.c", self.root / "writer.c"])
        files = [(self.args.image, bytes(image), "IMAGE_SHA"),
                 (self.args.control_source, control, "CONTROL_SHA"),
                 (self.args.pixel_source, pixel, "PIXEL_SHA"),
                 (original / "FD2.EXE", exe, "EXE_SHA"),
                 (original / "FDOTHER.DAT", resource, "OTHER_SHA")]
        for path, raw, constant in files:
            path.write_bytes(raw)
            self.stack.enter_context(patch.object(M, constant, M.digest(raw)))
        self.args.producer.write_bytes(b"synthetic producer identity; not run")
        for path in self.args.producer_source:
            path.write_text("synthetic producer source identity", encoding="utf-8")
        self.self_source, self.archive_source = self.root / "verifier.fixture", self.root / "archive.fixture"
        self.self_source.write_bytes(b"synthetic self identity")
        self.archive_source.write_bytes(b"synthetic dependency identity")
        self.stack.enter_context(patch.object(M, "__file__", str(self.self_source)))
        self.stack.enter_context(patch.object(M, "ARCHIVE_SOURCE", self.archive_source))
        # Independent closed form for D1/seed0, images68..79=(1x1,color1..12).
        initial_app = bytes((i * 13 + 5) & 255 for i in range(456 * 336))
        base = bytes((i * 17 + 91) & 255 for i in range(456 * 336))
        parts = [struct.pack("<8s4I", b"F4DFX001", 1, 0, 456 * 336, 12)]
        for frame in range(12):
            app = bytearray(initial_app if frame < 6 else base)
            app[90 * 456 + 96] = frame + 1
            vga = bytearray([109]) * 64000
            for row in range(192):
                vga[(4 + row) * 320 + 4:(4 + row) * 320 + 316] = app[(72 + row) * 456 + 72:(72 + row) * 456 + 384]
            parts.extend((bytes(app), base, bytes(vga)))
        self.valid = b"".join(parts)
        self.args.actual.write_bytes(self.valid)

    def failure(self, pattern):
        with self.assertRaisesRegex((ValueError, OSError), pattern) as error:
            M.verify(self.args)
        receipt = error.exception.receipt
        self.assertIs(receipt["equal"], False)
        return receipt

    def trigger(self, operation):
        original = M.expected_states
        def states(*args):
            for index, state in enumerate(original(*args)):
                if index == 0:
                    operation()
                yield state
        return patch.object(M, "expected_states", states)

    def test_real_verify_positive_with_full_before_after_receipt(self):
        report = M.verify(self.args)
        self.assertIs(report["equal"], True)
        self.assertEqual(len(report["frames"]), 36)
        identities = report["identity"]
        self.assertEqual(identities["before"], identities["after"])
        roles = {f["role"] for f in identities["before"]}
        self.assertTrue({"verifier_script", "archive_entries_dependency", "producer_binary", "producer_source", "actual_pages"} <= roles)

    def test_all36_owner_blocks_check_edges_and_reject_one_bad_byte(self):
        sizes = (456 * 336, 456 * 336, 64000)
        names = ("app", "base", "vga")
        stride = sum(sizes)
        for frame in range(12):
            for owner, name in enumerate(names):
                offset = 0 if frame % 2 == 0 else sizes[owner] - 1
                at = 24 + frame * stride + sum(sizes[:owner]) + offset
                raw = bytearray(self.valid)
                raw[at] ^= 1
                self.args.actual.write_bytes(raw)
                with self.subTest(frame=frame, owner=name, offset=offset):
                    self.failure(f"frame{68 + frame} {name} first mismatch {offset}:")
        self.args.actual.write_bytes(self.valid)

    def test_real_verify_rejects_swapped_frames_and_truncation(self):
        stride = 456 * 336 * 2 + 64000
        self.args.actual.write_bytes(self.valid[:24] + self.valid[24 + stride:24 + stride * 2] +
                                     self.valid[24:24 + stride] + self.valid[24 + stride * 2:])
        self.failure("frame68 app first mismatch")
        self.args.actual.write_bytes(self.valid[:-1])
        self.failure("actual exact length mismatch")

    def test_late_source_producer_dependency_self_and_actual_changes_reject(self):
        for path in (self.args.control_source, self.args.producer, self.args.producer_source[0],
                     self.archive_source, self.self_source, self.args.actual):
            before = path.read_bytes()
            with self.subTest(path=path.name), self.trigger(lambda p=path: p.write_bytes(p.read_bytes() + b"x")):
                receipt = self.failure("late guard changed")
                self.assertTrue(receipt["identity"]["late_errors"])
            path.write_bytes(before)

    def test_mtime_only_binary_change_is_not_ignored(self):
        before = self.args.producer.stat()
        with self.trigger(lambda: os.utime(self.args.producer, ns=(before.st_atime_ns, before.st_mtime_ns + 10000000))):
            receipt = self.failure("late guard changed")
        self.assertTrue(any(r["role"] == "producer_binary" for r in receipt["identity"]["late_errors"]))

    def test_primary_pixel_error_survives_secondary_guard_read_failure(self):
        raw = bytearray(self.valid)
        raw[24] ^= 1
        self.args.actual.write_bytes(raw)
        with self.trigger(self.args.image.unlink):
            receipt = self.failure("frame68 app first mismatch 0:")
        self.assertTrue(receipt["primary_error"].startswith("frame68 app first mismatch 0:"))
        self.assertTrue(receipt["identity"]["late_errors"])
        self.assertTrue(any(r.get("available") is False for r in receipt["identity"]["after"]))

    def test_cli_failure_emits_false_receipt_not_a_pass(self):
        raw = bytearray(self.valid)
        raw[-1] ^= 1
        self.args.actual.write_bytes(raw)
        argv = ["verify", "--actual", str(self.args.actual), "--original", str(self.args.original),
                "--image", str(self.args.image), "--control-source", str(self.args.control_source),
                "--pixel-source", str(self.args.pixel_source), "--producer", str(self.args.producer)]
        for path in self.args.producer_source:
            argv.extend(("--producer-source", str(path)))
        stdout, stderr = io.StringIO(), io.StringIO()
        with patch.object(sys, "argv", argv), redirect_stdout(stdout), redirect_stderr(stderr):
            self.assertEqual(M.main(), 1)
        self.assertIs(json.loads(stdout.getvalue())["equal"], False)
        self.assertIn("frame79 vga first mismatch", stderr.getvalue())


class F4HighDSourceContracts(unittest.TestCase):
    def test_all_source_grid_anchors_stay_inside_page(self):
        for raw in range(96):
            x,y=(0,0)if raw==30 else(24+24*(raw%15),18+24*(raw//15))
            self.assertTrue(0<=x and x+72<=456 and 0<=y and y+71<=336)
    def test_highD_consumes_last_target_and_only30_aliases_base(self):
        images=[(1,1,b"\x7b")]*12
        for count in (36,96):
            app,base,_=next(M.expected_prefix_states(images,count,0))
            for raw in (31,count-1):self.assertEqual(app[(18+24*(raw//15))*456+24+24*(raw%15)],123)
            self.assertEqual(base[0],123)
    def test_highD_alias_restore_uses_previous_base(self):
        states=list(M.expected_prefix_states([(1,1,bytes([i+1]))for i in range(12)],96,1))
        self.assertEqual((states[5][0][0],states[5][1][0]),(34,6))
        self.assertEqual((states[6][0][0],states[6][1][0]),(6,7))
    def test_prefix_flag_enters_real_verifier_loop(self):
        fixture=F4VerifyMainLoopContracts()
        fixture.setUp()
        try:
            fixture.args.prefix_wide=True;count=36
            first=bytes((i*13+5)&255 for i in range(M.PAGE));base=bytes((i*17+91)&255 for i in range(M.PAGE))
            chunks=[M.HEADER.pack(b"F4DFX002",count,0,M.PAGE,12)]
            for frame in range(12):
                app=bytearray(first if frame<6 else base)
                for raw in range(count):
                    if raw!=30:app[(18+24*(raw//15))*456+24+24*(raw%15)]=frame+1
                base=bytearray(base);base[0]=frame+1;base=bytes(base)
                vga=bytearray([109])*M.VGA
                for y in range(192):vga[(y+4)*320+4:(y+4)*320+316]=app[(y+72)*456+72:(y+72)*456+384]
                chunks.extend((bytes(app),base,bytes(vga)))
            fixture.args.actual.write_bytes(b"".join(chunks))
            report=M.verify(fixture.args)
            self.assertTrue(report['equal']);self.assertEqual(report['schema'],'fd2-f4-defeat-two-page-prefix-v3')
            self.assertEqual(report['count'],36)
        finally:fixture.doCleanups()

    def test_header_versions_and_count_domains_are_separate(self):
        tail=bytes(12*(M.PAGE*2+M.VGA))
        valid=M.HEADER.pack(b"F4DFX002",96,1,M.PAGE,12)+tail
        self.assertEqual(M.parameters_prefix(valid),(96,1))
        with self.assertRaises(ValueError):M.parameters(valid)
        for count in(0,35,37,97):
            with self.assertRaises(ValueError):M.parameters_prefix(M.HEADER.pack(b"F4DFX002",count,1,M.PAGE,12)+tail)


if __name__ == "__main__":
    unittest.main()
