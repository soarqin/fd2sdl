#!/usr/bin/env python3
"""构建 FD2.EXE 的规范 Ghidra 输入 bundle。

该脚本不信任派生 relocated 镜像。它从原始 bound LE 可执行文件提取 object
初始化字节，并把已经由 ``validate_le_fixups.py`` 独立验证的 relocation 清单
写入版本化 JSON。Ghidra 只消费此 bundle，不在 Jython 中维护第三套 LE parser。
"""
from __future__ import annotations

import argparse
import hashlib
import json
import struct
from pathlib import Path

from rebuild_fd2_analysis import (
    build_relbase_image,
    parse_fixups,
    parse_le,
    parse_objects,
)
from validate_le_fixups import validate

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_EXE = ROOT / "original_game" / "FD2.EXE"
DEFAULT_OUT = ROOT / "tools" / "fd2-analysis-bundle"
SCHEMA = "fd2-ghidra-bundle-v1"
EXPECTED_EXE_SHA256 = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"
EXPECTED_GHIDRA_SHA256 = "99d45035bdcc3d6627e7b1232b7b379905a9fad76c772c920602e2b5d8b2dac2"
EXPECTED_OBJECTS = (
    (1, 0x10000, 0x3EF29, 0x2045, 1, 63),
    (2, 0x50000, 0x056B0, 0x2043, 64, 4),
    (3, 0x60000, 0x034D2, 0x2043, 68, 4),
)


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def sha256_file(path: Path) -> str:
    return sha256_bytes(path.read_bytes())


def canonical_json(data: object) -> bytes:
    return (json.dumps(data, ensure_ascii=False, sort_keys=True,
                       separators=(",", ":")) + "\n").encode("utf-8")


def permissions(flags: int) -> str:
    # LE object flags: readable=bit 0, writable=bit 1, executable=bit 2.
    return "".join(("r" if flags & 1 else "-",
                    "w" if flags & 2 else "-",
                    "x" if flags & 4 else "-"))


def object_initialized_size(obj: object, page_size: int) -> int:
    return min(obj.vsize, obj.page_count * page_size)


def build_bundle(exe: Path, out: Path) -> dict[str, object]:
    exe_bytes = exe.read_bytes()
    exe_hash = sha256_bytes(exe_bytes)
    if exe_hash != EXPECTED_EXE_SHA256:
        raise ValueError(
            f"FD2.EXE SHA-256 不匹配：{exe_hash}，期望 {EXPECTED_EXE_SHA256}")

    total, errors, counts = validate(exe)
    if errors:
        raise ValueError("independent fixup validation failed: " + "; ".join(errors))
    if total != 7959 or validate.last_details.get("unique_sources") != 7948:
        raise ValueError("unexpected validated fixup totals")

    le = parse_le(exe_bytes)
    objects = parse_objects(exe_bytes, le)
    actual_objects = tuple(
        (o.index, o.relbase, o.vsize, o.flags, o.page_map_index, o.page_count)
        for o in objects)
    if actual_objects != EXPECTED_OBJECTS:
        raise ValueError(f"LE object table changed: {actual_objects!r}")
    if (le.module_file_off, le.le_off,
            le.module_file_off + le.data_pages_off) != (0x25214, 0x27ACC, 0x36014):
        raise ValueError("bound MZ/LE/data-page offsets changed")
    if (le.entry_object, le.entry_eip) != (1, 0x2CCB4):
        raise ValueError("LE entry changed")

    raw = bytes(build_relbase_image(exe_bytes, le, objects))
    # build_relbase_image 的稀疏前缀用于 relbase hash；每个 file-backed
    # object payload 必须再与原始 EXE page 字节独立逐字节核对。
    objmap = le.le_off + le.objmap_off
    for obj in objects:
        for page_i in range(obj.page_count):
            map_index = obj.page_map_index - 1 + page_i
            entry = exe_bytes[objmap + map_index * 4:objmap + map_index * 4 + 4]
            page_no = (entry[0] << 16) | (entry[1] << 8) | entry[2]
            source = le.module_file_off + le.data_pages_off + (page_no - 1) * le.page_size
            remaining = obj.vsize - page_i * le.page_size
            count = min(le.page_size, max(0, remaining))
            expected = exe_bytes[source:source + count]
            # object2 page 4 之后的 vsize 是 BSS；只核对 file-backed 4 页。
            if len(expected) < count:
                expected += b"\x00" * (count - len(expected))
            actual_start = obj.relbase + page_i * le.page_size
            actual = raw[actual_start:actual_start + count]
            if actual != expected:
                raise ValueError(
                    f"object{obj.index} page {page_i} differs from FD2.EXE source bytes")
    fixups = parse_fixups(exe_bytes, le, objects)
    unique_sources: dict[int, int] = {}
    duplicate_count = 0
    relocation_rows: list[dict[str, object]] = []
    for record_index, rec in enumerate(fixups):
        raw_u32 = struct.unpack_from("<I", raw, rec.source_addr_relbase)[0]
        if raw_u32 != rec.target_offset:
            raise ValueError(
                f"relocation prevalue mismatch at {rec.source_addr_relbase:#x}")
        old_target = unique_sources.get(rec.source_addr_relbase)
        duplicate = old_target is not None
        if duplicate:
            duplicate_count += 1
            if old_target != rec.target_linear:
                raise ValueError(
                    f"conflicting relocation source {rec.source_addr_relbase:#x}")
        unique_sources[rec.source_addr_relbase] = rec.target_linear
        source_object = next(
            o for o in objects
            if o.relbase <= rec.source_addr_relbase < o.relbase + o.vsize)
        relocation_rows.append({
            "record_index": record_index,
            "record_file_offset": rec.record_file_off,
            "page_index": rec.page_index,
            "source_object": source_object.index,
            "source_offset": rec.source_addr_relbase - source_object.relbase,
            "source_va": rec.source_addr_relbase,
            "source_type": rec.source_type,
            "target_flags": rec.target_flags,
            "target_object": rec.target_object,
            "target_offset": rec.target_offset,
            "target_va": rec.target_linear,
            "raw_u32": raw_u32,
            "duplicate": duplicate,
        })
    if duplicate_count != 11:
        raise ValueError(f"unexpected duplicate relocation count {duplicate_count}")

    out.mkdir(parents=True, exist_ok=True)
    object_rows: list[dict[str, object]] = []
    for obj in objects:
        initialized_size = object_initialized_size(obj, le.page_size)
        payload = raw[obj.relbase:obj.relbase + initialized_size]
        filename = f"object{obj.index}.init.bin"
        (out / filename).write_bytes(payload)
        first_page_file_offset = (le.module_file_off + le.data_pages_off +
                                  (obj.page_map_index - 1) * le.page_size)
        object_rows.append({
            "object": obj.index,
            "relbase": obj.relbase,
            "vsize": obj.vsize,
            "flags": obj.flags,
            "permissions": permissions(obj.flags),
            "page_map_index": obj.page_map_index,
            "page_count": obj.page_count,
            "initialized_size": initialized_size,
            "bss_size": obj.vsize - initialized_size,
            "init_file": filename,
            "init_sha256": sha256_bytes(payload),
            "first_page_file_offset": first_page_file_offset,
        })

    manifest: dict[str, object] = {
        "schema": SCHEMA,
        "evidence": {
            "exe_path": "original_game/FD2.EXE",
            "exe_size": len(exe_bytes),
            "exe_sha256": exe_hash,
            "ghidra_version": "11.3.2",
            "ghidra_release_zip_sha256": EXPECTED_GHIDRA_SHA256,
            "runtime_validation": "historical-pass-see-docs/tools/dosbox-debug.md",
        },
        "le": {
            "bound_mz_file_offset": le.module_file_off,
            "le_file_offset": le.le_off,
            "data_pages_file_offset": le.module_file_off + le.data_pages_off,
            "page_size": le.page_size,
            "page_count": le.num_pages,
            "last_page_size": le.last_page_size,
            "entry_object": le.entry_object,
            "entry_offset": le.entry_eip,
            "entry_va": objects[le.entry_object - 1].relbase + le.entry_eip,
        },
        "objects": object_rows,
        "relocation_summary": {
            "record_count": len(relocation_rows),
            "unique_source_count": len(unique_sources),
            "duplicate_record_count": duplicate_count,
            "source_type_0x07_flags_0x00_count": counts.get((0x07, 0x00), 0),
            "source_type_0x07_flags_0x10_count": counts.get((0x07, 0x10), 0),
            "raw_relbase_sha256": sha256_bytes(raw),
            "relocated_relbase_sha256": validate.last_details["relocated_sha256"],
        },
        "relocations": relocation_rows,
    }
    manifest_bytes = canonical_json(manifest)
    (out / "manifest.json").write_bytes(manifest_bytes)
    bundle_files = {
        row["init_file"]: row["init_sha256"] for row in object_rows
    }
    bundle_files["manifest.json"] = sha256_bytes(manifest_bytes)
    checksums = "".join(
        f"{digest}  {name}\n" for name, digest in sorted(bundle_files.items()))
    (out / "SHA256SUMS").write_text(checksums, encoding="ascii")
    return manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    parser.add_argument("--out", type=Path, default=DEFAULT_OUT)
    args = parser.parse_args()
    manifest = build_bundle(args.exe.resolve(), args.out.resolve())
    summary = manifest["relocation_summary"]
    print(f"schema={manifest['schema']}")
    print(f"objects={len(manifest['objects'])}")
    print(f"relocation_records={summary['record_count']}")
    print(f"unique_sources={summary['unique_source_count']}")
    print(f"duplicates={summary['duplicate_record_count']}")
    print(f"wrote={args.out.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
