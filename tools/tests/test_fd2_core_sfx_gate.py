"""B6最低声音gate：实际资源静态预检＋明确标为合成的PCM反例；无实录。"""
from __future__ import annotations

import contextlib
import copy
import hashlib
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
import fd2_core_sfx_gate as gate
from analyze_fd2_audio import archive_entries
from fd2_battle_animation_sfx import CORE_SCENARIOS, declare
from fd2_sfx_oracle import references

ORIGINAL = Path(__file__).resolve().parents[2] / "original_game"


def archive(entries):
    offsets, payload = [], bytearray()
    for entry in entries:
        offsets.append(6 + 4 * len(entries) + len(payload))
        payload.extend(entry)
    return b"LLLLLL" + b"".join(struct.pack("<I", x) for x in offsets) + payload


class CoreSfxTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.fdother = ORIGINAL / "FDOTHER.DAT"
        cls.before = hashlib.sha256(cls.fdother.read_bytes()).hexdigest(), cls.fdother.stat().st_mtime_ns
        cls.declarations = {name: declare(ORIGINAL, name) for name in CORE_SCENARIOS}
        cls.refs = references(cls.fdother, cls.declarations[CORE_SCENARIOS[0]]["candidates"])
        cls.voices = {(r["bank"], r["sample"]): r["values"] for r in cls.refs}

    @classmethod
    def tearDownClass(cls):
        assert (hashlib.sha256(cls.fdother.read_bytes()).hexdigest(), cls.fdother.stat().st_mtime_ns) == cls.before

    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="fd2-core-sfx-")
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def pcm(self, events, name="synthetic.wav", rate=44100):
        values = np.zeros(rate * 3)
        for identity, seconds in events:
            source = self.voices[identity][:gate.PREFIX_FRAMES]
            divisor = __import__("math").gcd(rate, 11025)
            part = signal.resample_poly(source, rate // divisor, 11025 // divisor)
            start = round(seconds * rate)
            values[start:start + len(part)] += part * .5
        # 固定、低幅的DC/背景，不据测试结果或未来实际WAV挑参数。
        values += .00001 * np.sin(np.arange(len(values)) * .034) + .0001
        pcm = np.round(values * 32768).astype("<i2")
        path = self.root / name
        with wave.open(str(path), "wb") as output:
            output.setnchannels(2); output.setsampwidth(2); output.setframerate(rate)
            output.writeframes(np.column_stack((pcm, pcm)).astype("<i2").tobytes())
        return path

    def test_actual_resources_fix_60ms_ac_and_complete_sha_aliases_before_capture(self):
        for name in CORE_SCENARIOS:
            plan = gate.prepare_core_sfx(self.fdother, self.declarations[name])
            self.assertEqual(len(plan["voice_groups"]), 22)
            self.assertEqual(plan["source_prefix_frames"], 661)
            self.assertEqual(plan["prefix_ms_declared_before_recording"], 60)
            self.assertGreater(min(group["prefix_ac_rms"] for group in plan["voice_groups"]), .015)
            self.assertGreaterEqual(min(group["source_frames"] for group in plan["voice_groups"]), 1812)
            self.assertFalse(plan["bank_runtime_pointer_proven"])
        plan = gate.prepare_core_sfx(self.fdother, self.declarations["player-magic0"])
        self.assertEqual(plan["declared_total_calls"], 8)
        self.assertEqual([s["declared_calls"] for s in plan["minimum_ordered_steps"]], [1, 7])
        cast = next(g for g in plan["voice_groups"] if {"bank": 82, "sample": 0} in g["aliases"])
        self.assertIn({"bank": 80, "sample": 0}, cast["aliases"])
        pending = gate.analyze_core_sfx(self.fdother, self.declarations["player-magic0"])
        self.assertEqual(pending["minimum_status"], "pending_no_recording")
        self.assertFalse(pending["minimum_core_sfx_supported"])

    def test_full0_same_bytes_cast_and_repeated_fx_minimum_not_seven_call_proof(self):
        # 故意按80/0名称合成cast；其完整字节==82/0，不能因bank别名误拒。
        events = [((80, 0), .2)] + [((82, 1), .7 + i * .11) for i in range(7)]
        result = gate.analyze_core_sfx(self.fdother, self.declarations["player-magic0"], self.pcm(events))
        self.assertTrue(result["minimum_core_sfx_supported"])
        self.assertEqual(result["minimum_status"], "supported")
        self.assertEqual(len(result["ordered_steps"]), 2)
        self.assertEqual(result["ordered_steps"][1]["repeated_cue_instance_mapping"], "pending")
        self.assertFalse(result["ordered_steps"][1]["exact_call_count_validated"])
        self.assertEqual(result["exact_replay_count"], "pending_not_measured")
        self.assertEqual(result["absolute_input_latency"], "pending_not_measured")
        for field in ("ok", "full_action_audio_validated", "exact_timing_validated",
                      "full_A7_or_sample_fidelity_pass", "actual_bank_pointer_validated"):
            self.assertFalse(result[field])
        for voice in result["voice_matches"]:
            self.assertEqual(voice["search_scope"], "entire_recording_global_peak")
            self.assertFalse(voice["runtime_bank_pointer_proven"])

    def test_three_distinct_heal_cues_found_globally_without_competing_at_other_times(self):
        declaration = self.declarations["player-heal13"]
        events = [((80, 11), .2), ((80, 12), .7), ((80, 1), 1.2)]
        for rate in (44100, 48000):
            result = gate.analyze_core_sfx(self.fdother, declaration, self.pcm(events, f"{rate}.wav", rate))
            self.assertTrue(result["minimum_core_sfx_supported"])
            self.assertEqual(len(result["ordered_steps"]), 3)
            for actual, expected in zip(result["ordered_steps"], (200, 700, 1200)):
                self.assertAlmostEqual(actual["observed_global_peak_ms"], expected, delta=1000 / rate)
            self.assertTrue(all(abs(delta - 500) < .03 for delta in result["selected_peak_intervals_ms"]))

    def test_wrong_order_wrong_voice_and_silence_do_not_pass_minimum(self):
        declaration = self.declarations["player-heal13"]
        cases = [([((80, 1), .2), ((80, 12), .7), ((80, 11), 1.2)], "wrong-order"),
                 ([((52, 0), .2), ((80, 12), .7), ((80, 1), 1.2)], "missing-cue"),
                 ([], "silent")]
        for events, name in cases:
            result = gate.analyze_core_sfx(self.fdother, declaration, self.pcm(events, name + ".wav"))
            self.assertFalse(result["minimum_core_sfx_supported"], name)
            self.assertFalse(result["full_A7_or_sample_fidelity_pass"])

    def test_physical_unique_voice_and_different_whole_sha_same_prefix_not_merged(self):
        declaration = self.declarations["physical-full-hit"]
        wav = self.pcm([((52, 3), .2)])
        positive = gate.analyze_core_sfx(self.fdother, declaration, wav)
        self.assertTrue(positive["minimum_core_sfx_supported"])
        # 新的不同完整VOICE故意拥有相同60ms前缀，绝不能当完整SHA alias合并。
        data = archive_entries(self.fdother.read_bytes()); bank = archive_entries(data[52])
        bank[0] = bank[3][:gate.PREFIX_FRAMES] + bank[0][gate.PREFIX_FRAMES:]
        data[52] = archive(bank)
        fdother = self.root / "synthetic-FDOTHER.DAT"; fdother.write_bytes(archive(data))
        altered = copy.deepcopy(declaration)
        altered["source"]["fdother_sha256"] = hashlib.sha256(fdother.read_bytes()).hexdigest()
        altered["references"] = [{k: v for k, v in r.items() if k != "values"}
                                 for r in references(fdother, altered["candidates"])]
        result = gate.analyze_core_sfx(fdother, altered, wav)
        self.assertFalse(result["minimum_core_sfx_supported"])
        required = result["ordered_steps"][0]["voice_sha256"]
        group = next(g for g in result["plan"]["voice_groups"] if g["sha256"] == required)
        self.assertNotIn({"bank": 52, "sample": 0}, group["aliases"])

    def test_declared_resource_hash_length_and_ac_rejections(self):
        declaration = copy.deepcopy(self.declarations["physical-full-hit"])
        declaration["source"]["fdother_sha256"] = "wrong"
        with self.assertRaisesRegex(ValueError, "来源不同"):
            gate.prepare_core_sfx(self.fdother, declaration)
        declaration = copy.deepcopy(self.declarations["physical-full-hit"])
        declaration["expected_action_cue_subsequence"][0]["voice_sha256"] = "wrong"
        with self.assertRaisesRegex(ValueError, "身份"):
            gate.prepare_core_sfx(self.fdother, declaration)
        for zero in (False, True):
            refs = copy.deepcopy(self.refs)
            if zero:
                refs[0]["values"][:] = 0
            else:
                refs[0]["source_frames"] = gate.PREFIX_FRAMES - 1
            declaration = copy.deepcopy(self.declarations["physical-full-hit"])
            declaration["references"] = [{k: v for k, v in r.items() if k != "values"} for r in refs]
            with patch.object(gate, "references", return_value=refs), self.assertRaises(ValueError):
                gate.prepare_core_sfx(self.fdother, declaration)

    def test_cli_resolved_resource_output_guard_and_no_overwrite(self):
        destination = ORIGINAL / "never-create-sfx-gate.json"
        argv = ["fd2_core_sfx_gate.py", "--original", str(ORIGINAL),
                "--scenario", "physical-full-hit", "--output", str(destination)]
        with patch.object(sys, "argv", argv), patch.object(gate, "declare") as declaration, \
             contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(gate.main(), 1)
            declaration.assert_not_called()
        self.assertFalse(destination.exists())
        output = self.root / "report.json"; output.write_text("preserved")
        argv[-1] = str(output)
        with patch.object(sys, "argv", argv), contextlib.redirect_stdout(io.StringIO()):
            self.assertEqual(gate.main(), 1)
        self.assertEqual(output.read_text(), "preserved")
        broken = self.root / "not-pcm.wav"; broken.write_bytes(b"RIFF-not-a-valid-WAVE")
        argv.extend(["--wav", str(broken)])
        captured = io.StringIO()
        with patch.object(sys, "argv", argv), contextlib.redirect_stdout(captured):
            self.assertEqual(gate.main(), 1)
        self.assertEqual(json.loads(captured.getvalue())["minimum_status"], "error")
        self.assertEqual(output.read_text(), "preserved")


if __name__ == "__main__":
    unittest.main()
