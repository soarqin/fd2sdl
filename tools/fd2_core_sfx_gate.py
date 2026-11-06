#!/usr/bin/env python3
"""B6三场最低声音证据：完整VOICE SHA身份组＋录音内有序出现。

静态cue来源由fd2_battle_animation_sfx.declare提供；不修改既有oracle语义。
统一60ms前缀在采集前声明：原FX0每2tick replace，不能拿1217ms整样本
要求每次播完。相同完整字节只证明声音等价类，不能推出运行bank指针。
本工具不采集声音，不将最低gate当作完整声音、精确时延或A7保真验收。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
from pathlib import Path
import wave

import numpy as np
from scipy import signal

from fd2_battle_animation_sfx import CORE_SCENARIOS, declare
from fd2_sfx_oracle import _match, read_recording, references

PREFIX_MS = 60
SOURCE_RATE = 11025
PREFIX_FRAMES = SOURCE_RATE * PREFIX_MS // 1000  # 661原始点，报告实际59.955ms。
MIN_SCORE = .8
IDENTITY_MARGIN = .05


def _prepared(fdother: Path, declaration: dict) -> tuple[dict, dict]:
    if (declaration.get("schema") != "fd2-b6-sfx-declaration-v1"
            or declaration.get("scenario") not in CORE_SCENARIOS):
        raise ValueError("仅支持已声明的B6核心三场")
    digest = hashlib.sha256(fdother.read_bytes()).hexdigest()
    if digest != declaration["source"]["fdother_sha256"]:
        raise ValueError("FDOTHER与静态cue声明来源不同")
    refs = references(fdother, declaration["candidates"])
    metadata = [{k: v for k, v in r.items() if k != "values"} for r in refs]
    if metadata != declaration["references"]:
        raise ValueError("独立VOICE读集被删改/重排")
    lookup = {(r["bank"], r["sample"]): r for r in refs}
    groups = {}
    for ref in refs:
        if ref["source_frames"] < PREFIX_FRAMES:
            raise ValueError("完整候选短于预声明60ms前缀，不能默默删掉反候选")
        prefix = ref["values"][:PREFIX_FRAMES]
        ac = float(np.std(prefix))
        if ac <= 1e-6:
            raise ValueError("候选60ms前缀缺少交流分量，不能作为身份模板")
        key = ref["sha256"]
        if key not in groups:
            groups[key] = {"sha256": key, "source_frames": ref["source_frames"],
                           "aliases": [], "prefix_ac_rms": ac,
                           "prefix_sha256": hashlib.sha256(
                               np.rint(prefix * 128 + 128).astype(np.uint8).tobytes()).hexdigest()}
        groups[key]["aliases"].append({"bank": ref["bank"], "sample": ref["sample"]})
    steps = []
    names = set()
    for cue in declaration["expected_action_cue_subsequence"]:
        name, expected = cue["name"], cue["expected"]
        key = (expected["bank"], expected["sample"])
        if name in names or key not in lookup or cue["voice_sha256"] != lookup[key]["sha256"]:
            raise ValueError("cue名称/完整VOICE身份与独立资源不一致")
        names.add(name)
        voice = cue["voice_sha256"]
        if not steps or steps[-1]["voice_sha256"] != voice:
            steps.append({"voice_sha256": voice, "declared_cues": []})
        steps[-1]["declared_cues"].append(name)
    if not steps:
        raise ValueError("没有已声明的核心cue")
    for step in steps:
        step.update(minimum_occurrences_required=1,
                    declared_calls=len(step["declared_cues"]),
                    exact_call_count_validated=False)
    plan = {"schema": "fd2-core-sfx-plan-v1", "scenario": declaration["scenario"],
            "fdother_sha256": digest, "prefix_ms_declared_before_recording": PREFIX_MS,
            "source_prefix_frames": PREFIX_FRAMES,
            "actual_prefix_duration_ms": PREFIX_FRAMES * 1000 / SOURCE_RATE,
            "prefix_source_policy": "all candidates fixed first661 source frames; no capture fitting",
            "min_score_predeclared": MIN_SCORE, "identity_margin_predeclared": IDENTITY_MARGIN,
            "actual_game_threshold_calibration": "pending",
            "voice_groups": list(groups.values()), "minimum_ordered_steps": steps,
            "declared_total_calls": len(names), "bank_runtime_pointer_proven": False}
    return plan, {r["sha256"]: r for r in refs}


def prepare_core_sfx(fdother: Path, declaration: dict) -> dict:
    """无录音即可固定完整别名分组、长度/AC和最低顺序，不选择未来时窗。"""
    return _prepared(fdother, declaration)[0]


def analyze_core_sfx(fdother: Path, declaration: dict, wav_path: Path | None = None) -> dict:
    plan, refs = _prepared(fdother, declaration)
    result = {"schema": "fd2-core-sfx-observation-v1", "ok": False,
              "minimum_status": "pending_no_recording", "minimum_core_sfx_supported": False,
              "plan": plan, "voice_matches": [], "ordered_steps": [],
              "absolute_input_latency": "pending_not_measured",
              "exact_replay_count": "pending_not_measured",
              "full_action_audio_validated": False, "exact_timing_validated": False,
              "full_A7_or_sample_fidelity_pass": False, "actual_bank_pointer_validated": False}
    if wav_path is None:
        return result
    metadata, values = read_recording(wav_path)
    result["wav"] = metadata
    if not len(values):
        result["minimum_status"] = "pending_empty_recording"
        return result
    rate = metadata["sample_rate"]
    divisor = math.gcd(rate, SOURCE_RATE)
    templates = {key: signal.resample_poly(ref["values"][:PREFIX_FRAMES],
                 rate // divisor, SOURCE_RATE // divisor) for key, ref in refs.items()}
    if len(values) < len(next(iter(templates.values()))):
        result["minimum_status"] = "not_supported_recording_shorter_than_fixed_prefix"
        return result
    hits = {}
    for group in plan["voice_groups"]:
        key = group["sha256"]
        hit = _match(values, templates[key])  # 整份录音的真实最大峰，不按预期cue挑窗。
        at = hit["offset_frames"]
        local = values[at:at + len(templates[key])]
        # 各合法cue在不同位置出现不应互相争夺全局first。身份竞争只比较同一峰窗口。
        competitors = [{"voice_sha256": other, "score": _match(local, template)["score"]}
                       for other, template in templates.items() if other != key]
        competitors.sort(key=lambda row: row["score"], reverse=True)
        second = competitors[0]["score"] if competitors else -1.0
        supported = hit["score"] >= MIN_SCORE and hit["score"] - second >= IDENTITY_MARGIN
        hits[key] = {"voice_sha256": key, "aliases": group["aliases"],
                     "identity_supported": supported, "score": hit["score"],
                     "strongest_other_group_at_same_peak": competitors[0] if competitors else None,
                     "onset_frame": at, "onset_ms": at * 1000 / rate,
                     "search_scope": "entire_recording_global_peak",
                     "same_voice_second_peak": {"score": hit["second_occurrence_score"],
                         "offset_frames": hit["second_occurrence_offset_frames"]},
                     "individual_cue_instance_proven": False, "runtime_bank_pointer_proven": False}
        result["voice_matches"].append(hits[key])
    times = []
    for step in plan["minimum_ordered_steps"]:
        hit = hits[step["voice_sha256"]]
        result["ordered_steps"].append({**step, "identity_supported": hit["identity_supported"],
            "observed_global_peak_ms": hit["onset_ms"],
            "repeated_cue_instance_mapping": "pending" if step["declared_calls"] > 1 else "not_claimed"})
        times.append(hit["onset_ms"])
    ordered = all(a < b for a, b in zip(times, times[1:]))
    identities = all(row["identity_supported"] for row in result["ordered_steps"])
    result.update(minimum_status="supported" if identities and ordered else "not_supported_identity_or_order",
                  minimum_core_sfx_supported=identities and ordered,
                  selected_peak_order_observed=ordered,
                  selected_peak_intervals_ms=[b - a for a, b in zip(times, times[1:])],
                  interval_scope="differences between selected global peaks; repeated instances unmapped")
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, required=True)
    parser.add_argument("--scenario", choices=CORE_SCENARIOS, required=True)
    parser.add_argument("--wav", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        original = args.original.resolve(strict=True)
        resource_root = (original / "FDOTHER.DAT").resolve(strict=True).parent
        output = args.output.resolve() if args.output else None
        if output is not None and (output == original or output.is_relative_to(original)
                or output == resource_root or output.is_relative_to(resource_root)
                or any(p.lower() == "original_game" for p in output.parts)):
            raise ValueError("报告不能写入真实原资源目录")
        report = analyze_core_sfx(original / "FDOTHER.DAT", declare(original, args.scenario), args.wav)
        text = json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
        if output is None:
            print(text, end="")
        else:
            with output.open("x", encoding="utf-8") as stream:
                stream.write(text)
        return 0
    except (OSError, ValueError, KeyError, wave.Error) as error:
        print(json.dumps({"minimum_status": "error", "error": str(error)}, ensure_ascii=False))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
