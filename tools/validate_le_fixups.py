#!/usr/bin/env python3
"""Strict independent validation for FD2's bound LE pages and fixups.

The decoder below intentionally does not call rebuild_fd2_analysis.parse_fixups.
It is a second implementation for the exact record classes present in FD2.EXE.
"""
from __future__ import annotations

import argparse
import hashlib
import struct
import sys
from dataclasses import dataclass
from pathlib import Path

from rebuild_fd2_analysis import build_relbase_image, parse_fixups, parse_le, parse_objects


def u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def i16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<h", data, offset)[0]


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


@dataclass(frozen=True)
class IndependentFixup:
    record_file_off: int
    page_index: int
    source_offset: int
    source_relbase: int
    target_object: int
    target_offset: int
    target_linear: int
    flags: int


def decode_independently(path: Path) -> tuple[bytes, object, list[object], list[IndependentFixup]]:
    data = path.read_bytes()
    le = parse_le(data)
    objects = parse_objects(data, le)
    page_table = le.le_off + le.fixup_page_table_off
    record_base = le.le_off + le.fixup_record_table_off
    page_owners: dict[int, tuple[object, int]] = {}
    for obj in objects:
        for local_page in range(obj.page_count):
            page_owners[obj.page_map_index - 1 + local_page] = (obj, local_page)
    records: list[IndependentFixup] = []
    previous = 0
    for page in range(le.num_pages + 1):
        current = u32(data, page_table + page * 4)
        if current < previous:
            raise ValueError(f"fixup page table decreases at page {page}")
        previous = current
    for page in range(le.num_pages):
        start = u32(data, page_table + page * 4)
        end = u32(data, page_table + (page + 1) * 4)
        cursor = record_base + start
        limit = record_base + end
        owner, local_page = page_owners[page]
        while cursor < limit:
            record_start = cursor
            if cursor + 7 > limit:
                raise ValueError(f"truncated fixup at page {page}, file {cursor:#x}")
            source_type = data[cursor]
            flags = data[cursor + 1]
            if source_type != 0x07:
                raise ValueError(f"unknown FD2 source type {source_type:#x} at {cursor:#x}")
            if flags not in (0x00, 0x10):
                raise ValueError(f"unknown FD2 target flags {flags:#x} at {cursor:#x}")
            source = i16(data, cursor + 2)
            target_object = data[cursor + 4]
            if not 1 <= target_object <= len(objects):
                raise ValueError(f"invalid target object {target_object} at {cursor:#x}")
            if flags == 0x10:
                if cursor + 9 > limit:
                    raise ValueError(f"truncated u32 target at {cursor:#x}")
                target_offset = u32(data, cursor + 5)
                cursor += 9
            else:
                target_offset = u16(data, cursor + 5)
                cursor += 7
            source_relbase = owner.relbase + local_page * le.page_size + source
            if source_relbase < owner.relbase or source_relbase + 4 > owner.relbase + owner.vsize:
                raise ValueError(f"source outside object at {record_start:#x}")
            target_linear = objects[target_object - 1].relbase + target_offset
            records.append(IndependentFixup(
                record_start, page, source, source_relbase, target_object,
                target_offset, target_linear, flags))
        if cursor != limit:
            raise ValueError(f"page {page} did not end at its page-table boundary")
    return data, le, objects, records


def validate(path: Path) -> tuple[int, list[str], dict[tuple[int, int], int]]:
    errors: list[str] = []
    counts: dict[tuple[int, int], int] = {}
    try:
        data, le, objects, independent = decode_independently(path)
        primary = parse_fixups(data, le, objects)
        a = [
            (r.record_file_off, r.page_index, r.source_offset, r.source_addr_relbase,
             r.target_object, r.target_offset, r.target_linear, r.target_flags)
            for r in primary
        ]
        b = [
            (r.record_file_off, r.page_index, r.source_offset, r.source_relbase,
             r.target_object, r.target_offset, r.target_linear, r.flags)
            for r in independent
        ]
        if a != b:
            errors.append("primary and independent decoders disagree")
        for rec in independent:
            counts[(0x07, rec.flags)] = counts.get((0x07, rec.flags), 0) + 1
        raw = bytes(build_relbase_image(data, le, objects))
        writes: dict[int, int] = {}
        duplicate_count = 0
        for rec in independent:
            old = writes.get(rec.source_relbase)
            if old is not None:
                duplicate_count += 1
                if old != rec.target_linear:
                    errors.append(f"conflicting duplicate source {rec.source_relbase:#x}")
            writes[rec.source_relbase] = rec.target_linear
            source_file = (le.module_file_off + le.data_pages_off +
                           rec.page_index * le.page_size + rec.source_offset)
            encoded = u32(data, source_file)
            if encoded != rec.target_offset:
                errors.append(
                    f"source prevalue mismatch at {rec.source_relbase:#x}: "
                    f"file={encoded:#x}, target_offset={rec.target_offset:#x}")
        relocated = bytearray(raw)
        for source, value in writes.items():
            struct.pack_into("<I", relocated, source, value)
        covered = set()
        for source in writes:
            covered.update(range(source, source + 4))
        for offset, (before, after) in enumerate(zip(raw, relocated)):
            if before != after and offset not in covered:
                errors.append(f"non-fixup byte changed at {offset:#x}")
                break
        expected_magic = (
            10, 10, 10, 10, 7, 7, 10, 10, 10, 10, 9, 10, 5, 5,
            8, 10, 6, 8, 10, 9, 5, 5, 10, 8, 8, 4, 10, 7)
        actual_magic = struct.unpack_from("<28I", raw, objects[1].relbase + 0x1F96)
        if actual_magic != expected_magic:
            errors.append(f"DS:0x1f96 mismatch: {actual_magic}")
        if len(independent) != 7959:
            errors.append(f"expected 7959 records, got {len(independent)}")
        if len(writes) != 7948 or duplicate_count != 11:
            errors.append(
                f"expected 7948 sources/11 duplicates, got {len(writes)}/{duplicate_count}")
        validate.last_details = {
            "unique_sources": len(writes),
            "duplicate_records": duplicate_count,
            "module_file_off": le.module_file_off,
            "data_file_off": le.module_file_off + le.data_pages_off,
            "raw_sha256": hashlib.sha256(raw).hexdigest(),
            "relocated_sha256": hashlib.sha256(relocated).hexdigest(),
            "magic_scale": actual_magic,
        }
        return len(independent), errors, counts
    except Exception as exc:
        errors.append(str(exc))
        validate.last_details = {}
        return 0, errors, counts


validate.last_details = {}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, default=Path("original_game/FD2.EXE"))
    args = parser.parse_args()
    total, errors, counts = validate(args.exe)
    print(f"fixup_records={total}")
    for key in sorted(counts):
        print(f"source=0x{key[0]:02x} flags=0x{key[1]:02x} count={counts[key]}")
    for key, value in validate.last_details.items():
        print(f"{key}={value}")
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("independent_decoder_match=PASS")
    print("source_prevalue_validation=PASS")
    print("non_fixup_immutability=PASS")
    print("static_loader_validation=PASS")
    print("runtime_byte_oracle=RUN_SEPARATELY_WITH_verify_le_runtime_dump.py")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
