#!/usr/bin/env python3
"""只读审计FDICON的RLE实际写入0，验证运行期缓存的透明假设。

sub_4E1A6 @0x4e1a6 会把实际写入的palette0映为24，而skip不写。
逐token核查原始RLE，不从已解码0猜透明；没有GUI、数据库或文件修改。
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


def audit_fdicon(data: bytes, *, expected_frames=1680) -> dict:
    if len(data) < 6:
        raise ValueError("FDICON头截断")
    width, height, count = struct.unpack_from("<HHH", data)
    if (width, height) != (24, 24) or count == 0 or (expected_frames is not None and count != expected_frames):
        raise ValueError("FDICON尺寸/帧数不符")
    boundary = 6 + count * 4
    if boundary > len(data):
        raise ValueError("FDICON偏移表截断")
    offsets = struct.unpack_from("<" + "I" * count, data, 6)
    if any(offset < boundary or offset >= len(data) or (i and offset <= offsets[i - 1])
           for i, offset in enumerate(offsets)):
        raise ValueError("FDICON帧边界无效")
    stats = {"repeat_zero_pixels": 0, "alternating_zero_pixels": 0,
             "literal_zero_pixels": 0, "written_pixels": 0,
             "explicit_skip_pixels": 0, "alternating_skip_pixels": 0}
    examples, zero_frames = [], []
    for frame, start in enumerate(offsets):
        end = offsets[frame + 1] if frame + 1 < count else len(data)
        source, frame_zeros = start, 0
        for y in range(height):
            x = 0
            while x < width:
                if source >= end:
                    raise ValueError(f"FDICON帧{frame}行{y}控制字节截断")
                control = data[source]
                source += 1
                length, mode = (control & 63) + 1, control & 192
                span = length * 2 if mode == 64 else length
                if x + span > width:
                    raise ValueError(f"FDICON帧{frame}行{y}RLE超行")
                if mode == 192:
                    stats["explicit_skip_pixels"] += span
                else:
                    needed = length if mode == 128 else 1
                    if source + needed > end:
                        raise ValueError(f"FDICON帧{frame}行{y}像素截断")
                    stats["written_pixels"] += length
                    if mode == 64:
                        stats["alternating_skip_pixels"] += length
                    zero_indices = [i for i, value in enumerate(data[source:source + length]) if value == 0] if mode == 128 else list(range(length)) if data[source] == 0 else []
                    label = "literal_zero_pixels" if mode == 128 else "alternating_zero_pixels" if mode == 64 else "repeat_zero_pixels"
                    stats[label] += len(zero_indices)
                    frame_zeros += len(zero_indices)
                    if zero_indices and len(examples) < 8:
                        first = zero_indices[0]
                        examples.append({"frame": frame, "x": x + (first * 2 + 1 if mode == 64 else first),
                                         "y": y, "mode": mode,
                                         "file_offset": source + (first if mode == 128 else 0)})
                    source += needed
                x += span
        if source != end:
            raise ValueError(f"FDICON帧{frame}存在未消费尾部")
        if frame_zeros:
            zero_frames.append(frame)
    total = sum(stats[key] for key in ("repeat_zero_pixels", "alternating_zero_pixels", "literal_zero_pixels"))
    return {"validated_frames": count, "opaque_zero_pixels": total,
            "frames_with_opaque_zero": zero_frames, "counts": stats, "examples": examples}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("resource", type=Path)
    args = parser.parse_args()
    try:
        data = args.resource.read_bytes()
        report = {"resource": str(args.resource.resolve()), "sha256": hashlib.sha256(data).hexdigest(), **audit_fdicon(data)}
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0 if report["opaque_zero_pixels"] == 0 else 1
    except (OSError, ValueError, struct.error) as error:
        print(f"FDICON透明审计未完成：{error}。检查原始资源后重跑。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
