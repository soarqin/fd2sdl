#!/usr/bin/env python3
"""转储 FD2 的 XMIDI 曲目与已确认数字样本 bank。

逆向依据：
- music_track_play @0x25977 从 FDMUS.DAT 文件名加载 track；
- sfx_play @0x4acaa 按嵌套 offset 表提交样本地址与长度；
- DAT_00003eec / DAT_00003b13 分别固定加载 FDOTHER[31]/[80]；
- title_action_menu @code0 0xf8d6 加载 FDOTHER[77]，SFX 0/1/2/3 分别用于
  片头滚动／确认／移动／action 入场；primary/secondary 只区分 AIL handle；
- animation_play @code0 0x1043f 为 ANI[1] 加载 FDOTHER[78]，首帧前播放 SFX 0。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import struct
import sys
import wave
from pathlib import Path
from typing import Any

from rebuild_fd2_analysis import (build_relbase_image, parse_le, parse_objects,
                                  page_number_from_map_entry)

MAGIC = b"LLLLLL"
SFX_BANK_ENTRIES = (31, 77, 78, 80)
STAGE_MUSIC_PRIMARY_VA = 0x51E63
STAGE_MUSIC_ALTERNATE_VA = 0x51E81
FD2_EXE_SHA256 = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"
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
    # 仅支持已核版本；先校验完整输入，避免畸形对象触发loader大分配。
    # 51e5f是51e5d DWORD fixup的中间字节，旧0x76e73不是音乐表起点。
    if hashlib.sha256(fd2_exe).hexdigest() != FD2_EXE_SHA256:
        raise ValueError("stage music tables只支持已核FD2.EXE版本")
    le = parse_le(fd2_exe)
    objects = parse_objects(fd2_exe, le)
    image = build_relbase_image(fd2_exe, le, objects)
    owner = next(obj for obj in objects
                 if obj.relbase <= STAGE_MUSIC_PRIMARY_VA < obj.relbase + obj.vsize)
    relative = STAGE_MUSIC_PRIMARY_VA - owner.relbase
    map_index = owner.page_map_index - 1 + relative // le.page_size
    entry = le.le_off + le.objmap_off + map_index * 4
    page_number = page_number_from_map_entry(fd2_exe[entry:entry + 4])
    file_offset = (le.module_file_off + le.data_pages_off +
                   (page_number - 1) * le.page_size + relative % le.page_size)
    raw = bytes(image[STAGE_MUSIC_PRIMARY_VA:
                      STAGE_MUSIC_ALTERNATE_VA + STAGE_MUSIC_COUNT])
    return {
        "file_offset": file_offset,
        "primary_canonical_va": STAGE_MUSIC_PRIMARY_VA,
        "alternate_canonical_va": STAGE_MUSIC_ALTERNATE_VA,
        "primary": list(raw[:STAGE_MUSIC_COUNT]),
        "alternate": list(raw[STAGE_MUSIC_COUNT:]),
        "sha256": hashlib.sha256(raw).hexdigest(),
        "executable_sha256": FD2_EXE_SHA256,
        "source": "parse_le/parse_objects/build_relbase_image raw-unrelocated",
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
                        help="按 AIL 默认参数导出 FDOTHER[31]/[77]/[78]/[80] WAV")
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
