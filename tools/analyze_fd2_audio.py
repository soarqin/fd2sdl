#!/usr/bin/env python3
"""转储 FD2 的 XMIDI 曲目与已确认数字样本 bank。

逆向依据：
- music_track_play @0x4ab8b 从 FDMUS handle 加载 track；
- sfx_play @0x4acaa 按嵌套 offset 表提交样本地址与长度；
- DAT_00003eec / DAT_00003b13 分别固定加载 FDOTHER[31]/[80]；
- title_action_menu @code0 0xfd1c 通过 secondary handle 播放 FDOTHER[77] SFX 3。
"""

from __future__ import annotations

import argparse
import json
import struct
import sys
import wave
from pathlib import Path
from typing import Any

MAGIC = b"LLLLLL"
SFX_BANK_ENTRIES = (31, 77, 80)
STAGE_MUSIC_FILE_OFFSET = 0x76E73
STAGE_MUSIC_COUNT = 30


def archive_offsets(data: bytes) -> list[int]:
    if not data.startswith(MAGIC):
        raise ValueError("缺少 LLLLLL 魔数")
    offsets: list[int] = []
    pos = len(MAGIC)
    while pos + 4 <= len(data):
        value = struct.unpack_from("<I", data, pos)[0]
        if value >= len(data) or (offsets and value <= offsets[-1]):
            break
        offsets.append(value)
        pos += 4
    if not offsets:
        raise ValueError("归档 offset 流为空")
    return offsets


def archive_entries(data: bytes) -> list[bytes]:
    offsets = archive_offsets(data)
    return [
        data[start : offsets[index + 1] if index + 1 < len(offsets) else len(data)]
        for index, start in enumerate(offsets)
    ]


def sample_summary(index: int, sample: bytes) -> dict[str, Any]:
    return {
        "index": index,
        "bytes": len(sample),
        "min": min(sample) if sample else None,
        "max": max(sample) if sample else None,
        "mean": round(sum(sample) / len(sample), 3) if sample else None,
    }


def stage_music_tables(fd2_exe: bytes) -> dict[str, Any]:
    start = STAGE_MUSIC_FILE_OFFSET
    end = start + STAGE_MUSIC_COUNT * 2
    if len(fd2_exe) < end:
        raise ValueError("FD2.EXE 不包含完整 stage music tables")
    return {
        "file_offset": start,
        "code0_offset": start - 0x10E00,
        "primary": list(fd2_exe[start:start + STAGE_MUSIC_COUNT]),
        "alternate": list(fd2_exe[start + STAGE_MUSIC_COUNT:end]),
    }


def analyze(fdmus: bytes, fdother: bytes,
            fd2_exe: bytes | None = None) -> dict[str, Any]:
    music_entries = archive_entries(fdmus)
    tracks = []
    sentinels = []
    unknown = []
    for index, entry in enumerate(music_entries):
        if entry == b" \r\n":
            sentinels.append(index)
        elif entry.startswith(b"FORM") and b"XMID" in entry[:64]:
            tracks.append({"index": index, "bytes": len(entry)})
        else:
            unknown.append({"index": index, "bytes": len(entry),
                            "head": entry[:12].hex()})

    other_entries = archive_entries(fdother)
    banks = []
    for entry_index in SFX_BANK_ENTRIES:
        if entry_index >= len(other_entries):
            raise ValueError(f"FDOTHER 缺少 entry {entry_index}")
        samples = archive_entries(other_entries[entry_index])
        banks.append({
            "fdother_entry": entry_index,
            "samples": [sample_summary(i, sample)
                        for i, sample in enumerate(samples)],
        })

    result = {
        "music": {
            "entry_count": len(music_entries),
            "xmid_tracks": tracks,
            "sentinel_entries": sentinels,
            "unknown_entries": unknown,
        },
        "sfx_playback_defaults": {
            "sample_rate": 11025,
            "format": "u8-mono",
            "pan": 64,
            "loop_count": 1,
            "evidence": "ail_init_sample @0x5e735 / core code0 0x566f4",
        },
        "sfx_banks": banks,
    }
    if fd2_exe is not None:
        result["stage_music_tables"] = stage_music_tables(fd2_exe)
    return result


def extract_music_xmi(fdmus: bytes, output_dir: Path) -> int:
    output_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    for index, entry in enumerate(archive_entries(fdmus)):
        if entry.startswith(b"FORM") and b"XMID" in entry[:64]:
            (output_dir / f"track{index:02d}.xmi").write_bytes(entry)
            count += 1
    return count


def extract_sfx_wavs(fdother: bytes, output_dir: Path) -> int:
    other_entries = archive_entries(fdother)
    output_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    for entry_index in SFX_BANK_ENTRIES:
        for sample_index, sample in enumerate(
                archive_entries(other_entries[entry_index])):
            path = output_dir / f"fdother{entry_index:03d}_{sample_index:02d}.wav"
            with wave.open(str(path), "wb") as wav:
                wav.setnchannels(1)
                wav.setsampwidth(1)
                wav.setframerate(11025)
                wav.writeframes(sample)
            count += 1
    return count


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--fdmus", default="original_game/FDMUS.DAT")
    parser.add_argument("--fdother", default="original_game/FDOTHER.DAT")
    parser.add_argument("--fd2-exe", default="original_game/FD2.EXE")
    parser.add_argument("--json", action="store_true")
    parser.add_argument("--extract-sfx-dir", type=Path,
                        help="按 AIL 默认参数导出 FDOTHER[31]/[80] WAV")
    parser.add_argument("--extract-music-dir", type=Path,
                        help="导出 FDMUS 中 15 个有效 XMIDI entry")
    args = parser.parse_args()

    fdmus = Path(args.fdmus).read_bytes()
    fdother = Path(args.fdother).read_bytes()
    result = analyze(fdmus, fdother, Path(args.fd2_exe).read_bytes())
    if args.extract_sfx_dir:
        count = extract_sfx_wavs(fdother, args.extract_sfx_dir)
        print(f"exported {count} WAV files to {args.extract_sfx_dir}",
              file=sys.stderr)
    if args.extract_music_dir:
        count = extract_music_xmi(fdmus, args.extract_music_dir)
        print(f"exported {count} XMI files to {args.extract_music_dir}",
              file=sys.stderr)
    if args.json:
        print(json.dumps(result, ensure_ascii=False, indent=2))
        return 0

    music = result["music"]
    print(f"FDMUS: entries={music['entry_count']} "
          f"xmid={len(music['xmid_tracks'])} "
          f"sentinels={len(music['sentinel_entries'])}")
    print("  XMIDI:", ", ".join(
        f"{item['index']}({item['bytes']}B)" for item in music["xmid_tracks"]))
    stage_tables = result["stage_music_tables"]
    print("stage music primary:", ",".join(map(str, stage_tables["primary"])))
    print("stage music alternate:",
          ",".join(map(str, stage_tables["alternate"])))
    defaults = result["sfx_playback_defaults"]
    for bank in result["sfx_banks"]:
        print(f"FDOTHER[{bank['fdother_entry']}]: "
              f"samples={len(bank['samples'])} "
              f"{defaults['format']}@{defaults['sample_rate']}Hz "
              f"({defaults['evidence']})")
        print("  lengths:", ", ".join(
            str(item["bytes"]) for item in bank["samples"]))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
