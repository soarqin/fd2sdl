#!/usr/bin/env python3
"""将 DOSBox mixer WAV 与 FDOTHER 原始样本按候选采样率做相关性比较。

示例：
  python3 tools/compare_fd2_sfx_capture.py capture.wav \
      --sample 5 --window 4.1:5.0

需要 NumPy；捕获文件和原版资源不入库。
"""

from __future__ import annotations

import argparse
import sys
import wave
from pathlib import Path

try:
    import numpy as np
except ImportError as error:  # pragma: no cover - analysis-only dependency
    raise SystemExit("需要 NumPy 才能比较 DOSBox 波形") from error

sys.path.insert(0, str(Path(__file__).resolve().parent))
from analyze_fd2_audio import archive_entries  # noqa: E402


def read_capture(path: Path) -> tuple[np.ndarray, int]:
    with wave.open(str(path), "rb") as wav:
        if wav.getsampwidth() != 2:
            raise ValueError("DOSBox capture 必须为 16-bit PCM WAV")
        channels = wav.getnchannels()
        samples = np.frombuffer(wav.readframes(wav.getnframes()), "<i2")
        samples = samples.reshape(-1, channels).astype(np.float64).mean(axis=1)
        return samples, wav.getframerate()


def normalized_best(segment: np.ndarray,
                    reference: np.ndarray) -> tuple[int, float]:
    reference = reference - reference.mean()
    full_size = len(segment) + len(reference) - 1
    fft_size = 1 << (full_size - 1).bit_length()
    full = np.fft.irfft(
        np.fft.rfft(segment, fft_size) *
        np.fft.rfft(reference[::-1], fft_size), fft_size)[:full_size]
    correlation = full[len(reference) - 1:len(segment)]
    sums = np.r_[0.0, np.cumsum(segment)]
    squares = np.r_[0.0, np.cumsum(segment * segment)]
    window_sum = sums[len(reference):] - sums[:-len(reference)]
    window_square = squares[len(reference):] - squares[:-len(reference)]
    energy = np.maximum(
        window_square - window_sum * window_sum / len(reference), 1e-9)
    scores = correlation / np.sqrt(energy * np.dot(reference, reference))
    index = int(np.nanargmax(np.abs(scores)))
    return index, float(scores[index])


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture", type=Path)
    parser.add_argument("--fdother", type=Path,
                        default=Path("original_game/FDOTHER.DAT"))
    parser.add_argument("--bank", type=int, default=31)
    parser.add_argument("--sample", type=int, required=True)
    parser.add_argument("--window", default="0:9999",
                        help="捕获中的搜索秒数范围，例如 4.1:5.0")
    parser.add_argument("--rates", default="8000,11025,16000,22050")
    args = parser.parse_args()

    start, end = (float(value) for value in args.window.split(":"))
    capture, output_rate = read_capture(args.capture)
    segment = capture[int(start * output_rate):int(end * output_rate)]
    outer = archive_entries(args.fdother.read_bytes())
    samples = archive_entries(outer[args.bank])
    raw = np.frombuffer(samples[args.sample], np.uint8).astype(np.float64) - 128.0

    print(f"capture={output_rate}Hz bank={args.bank} sample={args.sample} "
          f"bytes={len(raw)}")
    for source_rate in (int(value) for value in args.rates.split(",")):
        output_frames = round(len(raw) * output_rate / source_rate)
        reference = np.interp(
            np.arange(output_frames) * source_rate / output_rate,
            np.arange(len(raw)), raw)
        if len(reference) > len(segment):
            print(f"{source_rate:5d} Hz: window too short")
            continue
        index, score = normalized_best(segment, reference)
        print(f"{source_rate:5d} Hz: duration={len(reference) / output_rate:.6f}s "
              f"start={start + index / output_rate:.6f}s corr={score:.6f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
