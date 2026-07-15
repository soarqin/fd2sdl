#!/usr/bin/env python3
"""Verify a DOSBox pre-entry dump against FD2's static bound-LE loader model."""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

from rebuild_fd2_analysis import build_relbase_image, parse_fixups, parse_le, parse_objects


def parse_base(text: str) -> int:
    return int(text, 0)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("dump", type=Path)
    parser.add_argument("--exe", type=Path, default=Path("original_game/FD2.EXE"))
    parser.add_argument("--object1-base", type=parse_base, required=True)
    parser.add_argument("--object2-base", type=parse_base, required=True)
    parser.add_argument("--object3-base", type=parse_base, required=True)
    args = parser.parse_args()

    runtime = args.dump.read_bytes()
    data = args.exe.read_bytes()
    le = parse_le(data)
    objects = parse_objects(data, le)
    raw = bytes(build_relbase_image(data, le, objects))
    fixups = parse_fixups(data, le, objects)
    bases = {
        1: args.object1_base,
        2: args.object2_base,
        3: args.object3_base,
    }
    errors: list[str] = []
    covered: dict[int, set[int]] = {obj.index: set() for obj in objects}
    for record in fixups:
        source_obj = next(
            obj for obj in objects
            if obj.relbase <= record.source_addr_relbase < obj.relbase + obj.vsize)
        source_offset = record.source_addr_relbase - source_obj.relbase
        covered[source_obj.index].update(range(source_offset, source_offset + 4))
        runtime_source = bases[source_obj.index] + source_offset
        if runtime_source + 4 > len(runtime):
            errors.append(f"source outside dump: {runtime_source:#x}")
            continue
        actual = struct.unpack_from("<I", runtime, runtime_source)[0]
        expected = bases[record.target_object] + record.target_offset
        if actual != expected:
            errors.append(
                f"fixup mismatch source obj{source_obj.index}:{source_offset:#x}: "
                f"actual={actual:#x} expected={expected:#x}")
    for obj in objects:
        file_backed = min(obj.page_count * le.page_size, obj.vsize)
        runtime_start = bases[obj.index]
        if runtime_start + obj.vsize > len(runtime):
            errors.append(f"object{obj.index} outside dump")
            continue
        expected = raw[obj.relbase:obj.relbase + obj.vsize]
        actual = runtime[runtime_start:runtime_start + obj.vsize]
        for offset in range(file_backed):
            if offset in covered[obj.index]:
                continue
            if actual[offset] != expected[offset]:
                errors.append(
                    f"non-fixup file byte mismatch obj{obj.index}:{offset:#x}: "
                    f"actual={actual[offset]:#x} expected={expected[offset]:#x}")
                break
        # Loader-owned stack/BSS can be written before EIP transfer. Report it,
        # but file-backed bytes and every relocation remain the correctness oracle.
        bss_nonzero = sum(actual[i] != 0 for i in range(file_backed, obj.vsize))
        print(
            f"object{obj.index}: base={runtime_start:#x} vsize={obj.vsize:#x} "
            f"file_backed={file_backed:#x} pre_entry_bss_nonzero={bss_nonzero}")
    if errors:
        for error in errors[:50]:
            print(f"ERROR: {error}")
        print(f"runtime_dump_validation=FAIL errors={len(errors)}")
        return 1
    print(f"fixup_sources_verified={len(fixups)}")
    print("file_backed_non_fixup_bytes=PASS")
    print("runtime_dump_validation=PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
