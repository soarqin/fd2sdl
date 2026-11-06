"""合成PCM只验证有限SFX分析器；不替代原版/SDL实际录音校准。"""
from __future__ import annotations

import contextlib
import copy
import io
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import wave
from unittest.mock import patch

import numpy as np
from scipy import signal

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from fd2_sfx_oracle import analyze, compare_observations, main, read_recording, references


def archive(entries):
    offsets, payload = [], bytearray()
    for entry in entries:
        offsets.append(6 + len(entries) * 4 + len(payload))
        payload += entry
    return b"LLLLLL" + b"".join(struct.pack("<I", x) for x in offsets) + payload


class SfxOracleTest(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="fd2-sfx-oracle-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        rng = np.random.default_rng(120_504)
        self.voices = [rng.integers(10, 245, 1024, dtype=np.uint8).tobytes() for _ in range(2)]
        self.fdother = self.root / "FDOTHER.DAT"
        self.fdother.write_bytes(archive([archive(self.voices)]))
        self.before = self.fdother.read_bytes(), self.fdother.stat().st_mtime_ns
        self.request = {"schema": "fd2-sfx-query-v1",
                        "candidates": [{"bank": 0, "sample": n} for n in range(2)],
                        "queries": [{"name": "impact", "start_ms": 0, "end_ms": 1200,
                                     "expected": {"bank": 0, "sample": 0}}]}

    def tearDown(self):
        self.assertEqual((self.fdother.read_bytes(), self.fdother.stat().st_mtime_ns), self.before)

    def recording(self, rate=44100, voice=0, repeats=(.2,), name="actual.wav"):
        source = (np.frombuffer(self.voices[voice], np.uint8).astype(float) - 128) / 128
        template = signal.resample_poly(source, rate // 25, 11025 // 25)
        t = np.arange(int(1.2 * rate)) / rate
        values = .002 * np.sin(2 * np.pi * 137 * t) + .01
        for at in repeats:
            start = round(at * rate)
            values[start:start + len(template)] += template * .25
        pcm = np.round(values * 32768).astype("<i2")
        path = self.root / name
        with wave.open(str(path), "wb") as output:
            output.setnchannels(2)
            output.setsampwidth(2)
            output.setframerate(rate)
            output.writeframes(np.column_stack([pcm, pcm]).astype("<i2").tobytes())
        return path

    def test_pending_without_capture_has_independent_raw_reference_hashes(self):
        report = analyze(self.fdother, self.request)
        self.assertEqual(report["status"], "pending_no_recording")
        self.assertEqual(len(report["references"]), 2)
        self.assertNotEqual(report["references"][0]["sha256"], report["references"][1]["sha256"])
        self.assertFalse(report["full_A7_or_sample_fidelity_pass"])

    def test_reference_identity_with_mixed_tone_dc_and_actual_two_capture_rates(self):
        for rate in (44100, 48000):
            with self.subTest(rate=rate):
                report = analyze(self.fdother, self.request, self.recording(rate, name=f"{rate}.wav"))
                query = report["queries"][0]
                self.assertEqual(query["identity_status"], "supported_candidate")
                self.assertEqual(query["onset_status"], "observed_wav_position")
                self.assertGreater(query["candidates"][0]["score"], .99)
                self.assertAlmostEqual(query["candidates"][0]["onset_ms"], 200, delta=1000 / rate)
                self.assertFalse(report["thresholds_calibrated_against_actual_game"])
                self.assertEqual(query["clock"]["status"], "pending_no_clock_anchor")

    def test_wrong_sample_and_ambiguous_identity_are_not_supported(self):
        report = analyze(self.fdother, self.request, self.recording(voice=1))
        self.assertEqual(report["queries"][0]["identity_status"], "wrong_candidate")
        self.assertEqual(report["queries"][0]["clock"]["status"], "pending_identity")
        # 两个不同索引拥有相同波形时不能随意选择first。
        other = self.root / "duplicate.DAT"
        other.write_bytes(archive([archive([self.voices[0], self.voices[0]])]))
        report = analyze(other, self.request, self.recording(name="duplicate.wav"))
        self.assertEqual(report["queries"][0]["identity_status"], "ambiguous_or_weak")

    def test_repeated_cue_has_no_unique_onset_and_clock_bracket_is_not_exact_keydown(self):
        clock = {"first_packet_qpc_100ns": 1_000_000_000, "qpc_frequency": 10_000_000,
                 "key_call_qpc_bracket": [1_001_000_000, 1_001_200_000],
                 "discontinuities": 0, "timestamp_errors": 0}
        self.request["clock"] = clock
        path = self.recording()
        query = analyze(self.fdother, self.request, path)["queries"][0]
        timing = query["clock"]
        self.assertEqual(timing["status"], "observed_relative_to_send_call_bracket")
        self.assertFalse(timing["target_receipt_or_exact_keydown_claimed"])
        self.assertAlmostEqual(timing["since_key_call_ms_interval"][0], 80, delta=.03)
        self.assertAlmostEqual(timing["since_key_call_ms_interval"][1], 100, delta=.03)
        repeated = analyze(self.fdother, self.request, self.recording(repeats=(.2, .7), name="twice.wav"))["queries"][0]
        self.assertEqual(repeated["onset_status"], "ambiguous_repeated_occurrence")
        self.assertEqual(repeated["clock"]["status"], "pending_ambiguous_occurrence")
        for flag in ("discontinuities", "timestamp_errors"):
            self.request["clock"] = {**clock, flag: 1}
            self.assertEqual(analyze(self.fdother, self.request, path)["queries"][0]["clock"]["status"],
                             "pending_discontinuous_or_invalid_packet_clock")

    def test_silence_empty_truncation_and_search_bounds(self):
        path = self.recording()
        raw = path.read_bytes()
        # 静音只说明未提供候选身份；不产生零向量/NaN的正证。
        path.write_bytes(raw[:44] + bytes(len(raw) - 44))
        query = analyze(self.fdother, self.request, path)["queries"][0]
        self.assertEqual(query["identity_status"], "ambiguous_or_weak")
        self.assertEqual(query["candidates"][0]["score"], 0)
        path.write_bytes(raw[:-4])
        with self.assertRaisesRegex(ValueError, "不完整"):
            read_recording(path)
        empty = self.root / "empty.wav"
        with wave.open(str(empty), "wb") as output:
            output.setnchannels(2); output.setsampwidth(2); output.setframerate(44100)
        self.assertEqual(analyze(self.fdother, self.request, empty)["status"], "pending_empty_recording")
        good = self.recording(name="valid.wav")
        for end in (1300, 1):
            req = copy.deepcopy(self.request); req["queries"][0]["end_ms"] = end
            with self.assertRaises(ValueError):
                analyze(self.fdother, req, good)

    def test_declared_prefix_resource_validation_and_cli_no_overwrite(self):
        path = self.recording()
        self.request["queries"][0]["reference_prefix_ms"] = 50
        query = analyze(self.fdother, self.request, path)["queries"][0]
        self.assertEqual(query["identity_status"], "supported_candidate")
        self.assertEqual(query["candidates"][0]["reference_frames_compared"], 551)
        for candidates in ([{"bank": 999, "sample": 0}], [{"bank": 0, "sample": 999}],
                           [{"bank": True, "sample": 0}], [{"bank": 0, "sample": 0}] * 2):
            with self.assertRaises(ValueError):
                references(self.fdother, candidates)
        request = self.root / "request.json"; request.write_text(json.dumps(self.request))
        output = self.root / "report.json"; output.write_text("preserved")
        argv = ["fd2_sfx_oracle.py", "--fdother", str(self.fdother), "--request", str(request),
                "--wav", str(path), "--output", str(output)]
        with patch.object(sys, "argv", argv), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(main(), 1)
        self.assertEqual(output.read_text(), "preserved")
        forbidden = self.root / "original_game"; forbidden.mkdir()
        argv[-1] = str(forbidden / "never.json")
        with patch.object(sys, "argv", argv), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(main(), 1)
        self.assertFalse((forbidden / "never.json").exists())

    def test_pair_observations_require_same_reference_and_actual_clock_for_timing(self):
        original = analyze(self.fdother, self.request)
        sdl = copy.deepcopy(original)
        self.assertEqual(compare_observations(original, sdl, 25)["status"], "pending_capture_or_identity")
        self.request["clock"] = {"first_packet_qpc_100ns": 1_000_000_000,
            "qpc_frequency": 10_000_000, "key_call_qpc_bracket": [1_001_000_000, 1_001_200_000]}
        original = analyze(self.fdother, self.request, self.recording())
        sdl = analyze(self.fdother, self.request, self.recording(repeats=(.21,), name="sdl.wav"))
        paired = compare_observations(original, sdl, 35)
        self.assertTrue(paired["queries"][0]["identity_agrees"])
        self.assertTrue(paired["queries"][0]["within_declared_timing_tolerance"])
        self.assertAlmostEqual(paired["queries"][0]["sdl_minus_original_ms_interval"][0], -10, delta=.03)
        self.assertAlmostEqual(paired["queries"][0]["sdl_minus_original_ms_interval"][1], 30, delta=.03)
        self.assertFalse(paired["full_A7_or_sample_fidelity_pass"])
        for key in ("references", "fdother"):
            bad = copy.deepcopy(sdl)
            if key == "references":
                bad[key][0]["sha256"] = "modified"
            else:
                bad[key]["sha256"] = "modified"
            with self.assertRaisesRegex(ValueError, "参考来源"):
                compare_observations(original, bad, 35)

    def test_cli_resolved_resource_parent_rejects_junction_alias_before_analysis(self):
        request = self.root / "request.json"; request.write_text(json.dumps(self.request))
        alias = self.root / "junction-alias"
        forbidden = self.root / "evidence-report.json"
        original_resolve = Path.resolve

        def resolved(path, *args, **kwargs):
            # 模型等价original_game junction指向真实证据目录；解析结果不含别名。
            if path == alias / "FDOTHER.DAT":
                return self.fdother
            if path == alias / "evidence-report.json":
                return forbidden
            return original_resolve(path, *args, **kwargs)

        for destination in (forbidden, alias / "evidence-report.json", self.root):
            argv = ["fd2_sfx_oracle.py", "--fdother", str(alias / "FDOTHER.DAT"),
                    "--request", str(request), "--output", str(destination)]
            with patch.object(Path, "resolve", autospec=True, side_effect=resolved), \
                 patch("fd2_sfx_oracle.analyze") as analysis, patch.object(sys, "argv", argv), \
                 contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(main(), 1)
                analysis.assert_not_called()
            self.assertFalse(forbidden.exists())
        # 同一资源来源仍可在独立目录写新报告。
        with tempfile.TemporaryDirectory(prefix="fd2-sfx-report-") as safe:
            output = Path(safe) / "report.json"
            argv = ["fd2_sfx_oracle.py", "--fdother", str(self.fdother),
                    "--request", str(request), "--output", str(output)]
            with patch.object(sys, "argv", argv), contextlib.redirect_stdout(io.StringIO()):
                self.assertEqual(main(), 0)
            self.assertEqual(json.loads(output.read_text(encoding="utf-8"))["status"], "pending_no_recording")


if __name__ == "__main__":
    unittest.main()
