#!/usr/bin/env python3
"""B6有限SFX身份/时间取证，不判定完整A7音质或整段声音等价。

独立参考取自FDOTHER嵌套VOICE原字节，沿用analyze_fd2_audio的归档解析。
11025Hz/u8/mono依据ail_init_sample @0x39521（原登记同名）及
sfx_play @0x25a96（原登记同名）；5e735/4acaa仅为历史旧坐标。
只使用现有numpy/scipy的resample_poly/correlate，不采集或控制任何设备。
固定前缀可供会被replace截断的cue；长度须先声明，不能按录音坏点裁切。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import wave
from pathlib import Path

import numpy as np
from scipy import signal

from analyze_fd2_audio import archive_entries


def _hash(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_recording(path: Path) -> tuple[dict, np.ndarray]:
    """只接受helper明确支持的已封包PCM16 mono/stereo，最多120秒。"""
    path = path.resolve(strict=True)
    before = path.stat()
    raw = path.read_bytes()
    with wave.open(str(path), "rb") as wav:
        channels, rate, width, frames = (wav.getnchannels(), wav.getframerate(),
                                       wav.getsampwidth(), wav.getnframes())
        if (wav.getcomptype() != "NONE" or channels not in (1, 2) or width != 2
                or rate not in (44100, 48000) or frames > rate * 120):
            raise ValueError("只支持≤120秒 PCM16 mono/stereo 44100/48000 WAV")
        pcm = wav.readframes(frames)
    after = path.stat()
    if (len(pcm) != frames * channels * width or
            (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns)):
        raise ValueError("WAV不完整或读取时仍在写入")
    samples = np.frombuffer(pcm, dtype="<i2").reshape((-1, channels))
    mono = samples.astype(np.float64).mean(axis=1) / 32768.0
    return {"path": str(path), "sha256": _hash(raw), "sample_rate": rate,
            "channels": channels, "bits": 16, "frames": frames,
            "duration_ms": frames * 1000.0 / rate,
            "nonzero_samples": int(np.count_nonzero(samples)),
            "peak_pcm": int(np.abs(samples.astype(np.int32)).max()) if frames else 0}, mono


def references(fdother: Path, candidates: list[dict]) -> list[dict]:
    """返回独立原始VOICE引用；信号数组仅内存分析，不写入报告或原资源。"""
    data = fdother.read_bytes()
    entries = archive_entries(data)
    seen = set()
    result = []
    for item in candidates:
        bank, sample = item["bank"], item["sample"]
        if type(bank) is not int or type(sample) is not int or bank < 0 or sample < 0:
            raise ValueError("bank/sample必须是非负整数")
        if (bank, sample) in seen:
            raise ValueError("重复VOICE候选")
        seen.add((bank, sample))
        if bank >= len(entries):
            raise ValueError("VOICE bank越界")
        parts = archive_entries(entries[bank])
        if sample >= len(parts) or len(parts[sample]) < 16:
            raise ValueError("VOICE sample越界或不足16点")
        raw = parts[sample]
        result.append({"bank": bank, "sample": sample, "sha256": _hash(raw),
                       "source_rate": 11025, "source_frames": len(raw),
                       "values": (np.frombuffer(raw, dtype=np.uint8).astype(np.float64) - 128) / 128})
    if not result:
        raise ValueError("VOICE候选为空")
    return result


def _match(values: np.ndarray, template: np.ndarray) -> dict:
    template = template - template.mean()
    norm = float(np.dot(template, template))
    if norm <= 1e-12 or len(values) < len(template):
        raise ValueError("VOICE无交流分量或搜索窗口短于固定模板")
    # NCC分母使用每个完整模板窗的方差，静音/DC不成为身份正证。
    sums = np.concatenate(([0.0], np.cumsum(values)))
    squares = np.concatenate(([0.0], np.cumsum(values * values)))
    n = len(template)
    variance = squares[n:] - squares[:-n] - (sums[n:] - sums[:-n]) ** 2 / n
    numerator = signal.correlate(values, template, mode="valid", method="fft")
    score = np.zeros(len(numerator))
    np.divide(numerator, np.sqrt(np.maximum(variance, 0) * norm), out=score,
              where=variance > 1e-12)
    score = np.clip(score, -1, 1)
    at = int(np.argmax(score))
    # 同一个VOICE出现多次不可冒称唯一时刻；只比较相隔至少半模板的峰。
    peaks, _ = signal.find_peaks(score, distance=max(1, n // 2))
    peaks = sorted(set(map(int, peaks)) | {0, len(score) - 1, at},
                   key=lambda p: float(score[p]), reverse=True)
    other = next((p for p in peaks if abs(p - at) >= max(1, n // 2)), None)
    return {"score": float(score[at]), "offset_frames": at,
            "second_occurrence_score": float(score[other]) if other is not None else None,
            "second_occurrence_offset_frames": other}


def _time_anchor(onset_ms: float, clock: dict | None) -> dict:
    if not clock:
        return {"status": "pending_no_clock_anchor"}
    if clock.get("timestamp_errors", 0) or clock.get("discontinuities", 0):
        return {"status": "pending_discontinuous_or_invalid_packet_clock"}
    first, frequency, bracket = (clock.get("first_packet_qpc_100ns"),
                                 clock.get("qpc_frequency"), clock.get("key_call_qpc_bracket"))
    if (type(first) is not int or first <= 0 or type(frequency) is not int or frequency <= 0
            or not isinstance(bracket, list) or len(bracket) != 2
            or any(type(x) is not int or x < 0 for x in bracket) or bracket[1] < bracket[0]):
        raise ValueError("PCM首packet/QPC发送调用区间格式错误")
    absolute = first / 10000.0 + onset_ms
    return {"status": "observed_relative_to_send_call_bracket",
            "since_key_call_ms_interval": [absolute - bracket[1] * 1000 / frequency,
                                             absolute - bracket[0] * 1000 / frequency],
            "target_receipt_or_exact_keydown_claimed": False}


def analyze(fdother: Path, request: dict, wav_path: Path | None = None) -> dict:
    if request.get("schema") != "fd2-sfx-query-v1":
        raise ValueError("未知SFX查询schema")
    if (not isinstance(request.get("candidates"), list) or not 1 <= len(request["candidates"]) <= 32
            or not isinstance(request.get("queries"), list) or not 1 <= len(request["queries"]) <= 128):
        raise ValueError("SFX查询须1..32独立候选及1..128显式时窗")
    refs = references(fdother, request["candidates"])
    report = {"schema": "fd2-sfx-observation-v1", "status": "pending_no_recording",
              "fdother": {"path": str(fdother.resolve()), "sha256": _hash(fdother.read_bytes())},
              "references": [{k: v for k, v in ref.items() if k != "values"} for ref in refs],
              "identity_method": "signed normalized full-window correlation / scipy resample_poly",
              "thresholds_calibrated_against_actual_game": False,
              "full_A7_or_sample_fidelity_pass": False, "queries": []}
    if wav_path is None:
        return report
    metadata, values = read_recording(wav_path)
    report.update(status="analyzed_observation_only", wav=metadata)
    if not len(values):
        report["status"] = "pending_empty_recording"
        return report
    rate = metadata["sample_rate"]
    threshold, margin = request.get("min_score", .8), request.get("identity_margin", .05)
    if (type(threshold) not in (int, float) or not 0 < threshold <= 1 or
            type(margin) not in (int, float) or not 0 <= margin <= 1):
        raise ValueError("身份阈值无效")
    names = set()
    for query in request["queries"]:
        name, start, end = query["name"], query["start_ms"], query["end_ms"]
        if not isinstance(name, str) or not name or name in names:
            raise ValueError("SFX查询名为空或重复")
        names.add(name)
        if (type(start) not in (int, float) or type(end) not in (int, float)
                or not math.isfinite(start) or not math.isfinite(end)
                or not 0 <= start < end <= metadata["duration_ms"] + .001):
            raise ValueError("SFX搜索时窗越界")
        expected = query["expected"]
        expected_key = (expected["bank"], expected["sample"])
        if (any(type(x) is not int for x in expected_key)
                or expected_key not in {(r["bank"], r["sample"]) for r in refs}):
            raise ValueError("预期VOICE不在独立候选读集")
        prefix_ms = query.get("reference_prefix_ms")
        if prefix_ms is not None and (type(prefix_ms) not in (int, float) or not 30 <= prefix_ms <= 2000):
            raise ValueError("固定VOICE前缀须30..2000ms；不允许按坏像素/波形动态裁切")
        lo, hi = int(start * rate / 1000), min(len(values), int(end * rate / 1000))
        ranked = []
        for ref in refs:
            original = ref["values"]
            count = len(original) if prefix_ms is None else int(prefix_ms * 11025 / 1000)
            if count > len(original):
                raise ValueError("候选短于预先声明的统一VOICE前缀")
            divisor = math.gcd(rate, 11025)
            template = signal.resample_poly(original[:count], rate // divisor, 11025 // divisor)
            hit = _match(values[lo:hi], template)
            hit.update(bank=ref["bank"], sample=ref["sample"], reference_frames_compared=count,
                       reference_prefix_ms=prefix_ms,
                       onset_ms=(lo + hit["offset_frames"]) * 1000 / rate)
            ranked.append(hit)
        ranked.sort(key=lambda r: r["score"], reverse=True)
        best = ranked[0]
        unique = (best["score"] >= threshold and
                  (len(ranked) == 1 or best["score"] - ranked[1]["score"] >= margin))
        repeated = (best["second_occurrence_score"] is not None and
                    best["score"] - best["second_occurrence_score"] < margin)
        supported = unique and (best["bank"], best["sample"]) == expected_key
        report["queries"].append({"name": name, "search_window_ms": [start, end],
            "expected": expected, "candidates": ranked,
            "identity_status": "supported_candidate" if supported
                               else "wrong_candidate" if unique else "ambiguous_or_weak",
            "onset_status": "pending_identity" if not supported else
                            "ambiguous_repeated_occurrence" if repeated else "observed_wav_position",
            "clock": {"status": "pending_identity"} if not supported else
                     {"status": "pending_ambiguous_occurrence"} if repeated else
                     _time_anchor(best["onset_ms"], request.get("clock"))})
    return report


def compare_observations(original: dict, sdl: dict, timing_tolerance_ms: float) -> dict:
    """只比较同一独立VOICE读集/显式cue；两个录音来源由runner另绑定。"""
    if (not math.isfinite(timing_tolerance_ms) or timing_tolerance_ms < 0
            or any(r.get("schema") != "fd2-sfx-observation-v1" for r in (original, sdl))):
        raise ValueError("无效SFX报告/时序容差")
    if (original["fdother"]["sha256"] != sdl["fdother"]["sha256"]
            or original["references"] != sdl["references"]):
        raise ValueError("两端独立VOICE参考来源不一致")
    a, b = original["queries"], sdl["queries"]
    result = {"status": "pending_capture_or_identity", "full_A7_or_sample_fidelity_pass": False,
              "timing_tolerance_ms": timing_tolerance_ms, "queries": []}
    if not a or not b:
        return result
    if [(q["name"], q["expected"]) for q in a] != [(q["name"], q["expected"]) for q in b]:
        raise ValueError("两端显式SFX cue顺序/身份合同不同")
    result["status"] = "compared_observations_only"
    for left, right in zip(a, b):
        item = {"name": left["name"], "identity_agrees": all(
            q["identity_status"] == "supported_candidate" for q in (left, right)),
            "timing_status": "pending_clock_or_unique_onset"}
        if item["identity_agrees"]:
            if (left["candidates"][0]["reference_frames_compared"] !=
                    right["candidates"][0]["reference_frames_compared"]):
                raise ValueError("两端VOICE比较前缀不一致")
            if all(q["clock"]["status"] == "observed_relative_to_send_call_bracket" for q in (left, right)):
                lo, hi = left["clock"]["since_key_call_ms_interval"], right["clock"]["since_key_call_ms_interval"]
                delta = [hi[0] - lo[1], hi[1] - lo[0]]
                item.update(timing_status="observed_send_call_relative_interval",
                            sdl_minus_original_ms_interval=delta,
                            within_declared_timing_tolerance=all(abs(x) <= timing_tolerance_ms for x in delta))
        result["queries"].append(item)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fdother", type=Path, required=True)
    parser.add_argument("--request", type=Path, required=True)
    parser.add_argument("--wav", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    try:
        target = args.output.resolve() if args.output else None
        if target is not None:
            # original_game可为junction；边界取真实FDOTHER所属目录，不能只看别名。
            resource_root = args.fdother.resolve(strict=True).parent
            if (target == resource_root or target.is_relative_to(resource_root)
                    or any(p.lower() == "original_game" for p in target.parts)):
                raise ValueError("报告不能写入原资源目录")
        report = analyze(args.fdother, json.loads(args.request.read_text(encoding="utf-8-sig")), args.wav)
        text = json.dumps(report, ensure_ascii=False, indent=2, allow_nan=False) + "\n"
        if target is not None:
            with target.open("x", encoding="utf-8") as stream:
                stream.write(text)
        else:
            print(text, end="")
        return 0
    except (OSError, ValueError, KeyError, wave.Error) as error:
        print(json.dumps({"status": "error", "error": str(error)}, ensure_ascii=False))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
