#!/usr/bin/env python3
"""比较 DOSBox OPL capture 与 libADLMIDI 离线渲染的曲目指纹。

render_dir 应包含 render_fd2_opl_prototype.sh all 生成的 trackNN.xmi.wav。
直接波形相关适合相同／相近 emulator；短时谱分数辅助比较不同 OPL 实现。
结果只能生成候选，仍需调用链或运行时资源指针独立确认。
"""

from __future__ import annotations

import argparse
import wave
from pathlib import Path

try:
    import numpy as np
except ImportError as error:  # pragma: no cover - analysis-only dependency
    raise SystemExit("需要 NumPy 才能比较 OPL capture") from error


def read_mono(path: Path) -> tuple[np.ndarray, int]:
    with wave.open(str(path), "rb") as wav:
        if wav.getsampwidth() != 2:
            raise ValueError(f"{path}: 仅支持 16-bit PCM WAV")
        channels = wav.getnchannels()
        values = np.frombuffer(wav.readframes(wav.getnframes()), "<i2")
        return values.reshape(-1, channels).astype(np.float64).mean(axis=1), \
            wav.getframerate()


def normalized_best(long: np.ndarray,
                    short: np.ndarray) -> tuple[int, float] | None:
    if len(long) < len(short):
        return None
    short = short - short.mean()
    long = long - long.mean()
    full_size = len(long) + len(short) - 1
    fft_size = 1 << (full_size - 1).bit_length()
    full = np.fft.irfft(
        np.fft.rfft(long, fft_size) *
        np.fft.rfft(short[::-1], fft_size), fft_size)[:full_size]
    correlation = full[len(short) - 1:len(long)]
    sums = np.r_[0.0, np.cumsum(long)]
    squares = np.r_[0.0, np.cumsum(long * long)]
    window_sum = sums[len(short):] - sums[:-len(short)]
    window_square = squares[len(short):] - squares[:-len(short)]
    energy = np.maximum(
        window_square - window_sum * window_sum / len(short), 1e-9)
    scores = correlation / np.sqrt(energy * np.dot(short, short))
    index = int(np.nanargmax(np.abs(scores)))
    return index, float(scores[index])


def spectrum_features(values: np.ndarray, rate: int) -> np.ndarray:
    target_rate = 11025
    if rate % target_rate != 0:
        raise ValueError("短时谱比较要求采样率为 11025 Hz 的整数倍")
    values = values[::rate // target_rate]
    window_size, hop = 2048, 512
    window = np.hanning(window_size)
    result = []
    for offset in range(0, len(values) - window_size + 1, hop):
        spectrum = np.log1p(np.abs(np.fft.rfft(
            values[offset:offset + window_size] * window)))
        bands = np.array([
            spectrum[1 + band * 16:1 + (band + 1) * 16].mean()
            for band in range(64)
        ])
        bands -= bands.mean()
        norm = np.linalg.norm(bands)
        result.append(bands / (norm if norm else 1.0))
    return np.asarray(result)


def spectrum_best(long: np.ndarray, short: np.ndarray,
                  rate: int) -> tuple[float, float] | None:
    long_features = spectrum_features(long, rate)
    short_features = spectrum_features(short, rate)
    if len(long_features) < len(short_features) or not len(short_features):
        return None
    scores = np.array([
        (long_features[index:index + len(short_features)] *
         short_features).sum() / len(short_features)
        for index in range(len(long_features) - len(short_features) + 1)
    ])
    index = int(scores.argmax())
    return index * 512 / 11025.0, float(scores[index])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("render_dir", type=Path)
    parser.add_argument("--trim", type=float, default=0.3,
                        help="从 capture 首尾裁掉的秒数，默认 0.3")
    args = parser.parse_args()

    capture, rate = read_mono(args.capture)
    trim_frames = round(args.trim * rate)
    if trim_frames:
        capture = capture[trim_frames:-trim_frames]
    rows = []
    for path in sorted(args.render_dir.glob("track*.xmi.wav")):
        rendered, rendered_rate = read_mono(path)
        if rendered_rate != rate:
            raise ValueError(f"{path}: 采样率 {rendered_rate} 与 capture 不同")
        wave_result = normalized_best(rendered, capture)
        spectrum_result = spectrum_best(rendered, capture, rate)
        if not wave_result or not spectrum_result:
            continue
        wave_index, wave_score = wave_result
        spectrum_time, spectrum_score = spectrum_result
        rows.append((wave_score, path.name, wave_index / rate,
                     spectrum_score, spectrum_time))

    for wave_score, name, wave_time, spectrum_score, spectrum_time in \
            sorted(rows, reverse=True):
        print(f"{name}: wave={wave_score:.6f}@{wave_time:.3f}s "
              f"spectrum={spectrum_score:.6f}@{spectrum_time:.3f}s")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
