#!/usr/bin/env python3
"""重建 FD2.EXE 的可分析镜像与 LE fixup 清单。

关键原则：
- raw-unrelocated 与 loader-relocated 镜像分开输出。旧脚本使用错误 page base
  和 target bias，把 fixup 写到错误指令位置；新流程只按 manifest 中的合法
  relocation source 写入独立 relocated 镜像。
- `__chkstk` 已按当前 object1 机器码确认在 relbase VA `0x3702f` /
  code0 `0x2702f`；保留原始字节，不再通过 patch 改变分析输入。

输出：
  tools/fd2_le_raw.bin              object 按 LE relbase 摆放，未应用 fixup
  tools/fd2_le_code0.bin            object1 从 0 开始的真实 LE page view
  tools/fd2_le_ghidra_chkstk.bin    兼容文件名；当前与 raw 相同，不做未验证 patch
  tools/fd2_le_dual_clean.bin       code-only：object1 放在 0x10000，并镜像低 64K
  tools/fd2_le_fixups.txt           完整 internal offset fixup 清单
  tools/fd2_le_relocated_relbase.bin loader-relocated relbase 镜像
  tools/fd2_le_relocation_manifest.tsv 逐 source relocation manifest
  docs/generated/le-fixups.txt      生成摘要和验证状态
"""

from __future__ import annotations

import argparse
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_EXE = ROOT / "original_game" / "FD2.EXE"
TOOLS = ROOT / "tools"
DOCS = ROOT / "docs"


@dataclass(frozen=True)
class LEHeader:
    le_off: int
    num_pages: int
    page_size: int
    last_page_size: int
    objtab_off: int
    objcnt: int
    objmap_off: int
    fixup_page_table_off: int
    fixup_record_table_off: int
    data_pages_off: int
    entry_object: int
    entry_eip: int
    module_file_off: int


@dataclass(frozen=True)
class ObjectRecord:
    index: int
    vsize: int
    relbase: int
    flags: int
    page_map_index: int
    page_count: int


@dataclass(frozen=True)
class FixupRecord:
    page_index: int
    source_offset: int
    source_addr_relbase: int
    source_addr_code0: int
    source_type: int
    target_flags: int
    target_object: int
    target_offset: int
    target_linear: int
    record_file_off: int = 0


# 本工程只输出形如 `SRC=0x07 FLAGS=0x00 <src:u16> <obj:1..3> <off:u16>` 的简单记录。
# LE fixup 表允许变长记录；这里按记录结构顺序跳过未知类型，但清单仍只作索引，
# 不作为完整 loader 实现。
SIMPLE_FIXUP_SIZE = 7


def u16(buf: bytes, off: int) -> int:
    return struct.unpack_from("<H", buf, off)[0]


def u32(buf: bytes, off: int) -> int:
    return struct.unpack_from("<I", buf, off)[0]


def find_bound_mz_module(buf: bytes, le_off: int) -> int:
    """Find the embedded MZ whose e_lfanew points at the selected LE header."""
    matches = []
    for off in range(le_off + 1):
        if buf[off:off + 2] != b"MZ" or off + 0x40 > len(buf):
            continue
        if off + u32(buf, off + 0x3C) == le_off:
            matches.append(off)
    if len(matches) != 1:
        raise ValueError(f"LE 头必须有唯一 bound MZ owner，实际 {matches}")
    return matches[0]


def parse_le(buf: bytes) -> LEHeader:
    # 外层 DOS stub 的 e_lfanew 不可靠；找到 LE 后再要求唯一 embedded MZ owner。
    candidates = [i for i in range(len(buf) - 2) if buf[i:i + 2] == b"LE"]
    for off in candidates:
        if off + 0x90 <= len(buf) and u16(buf, off + 8) == 2 and u16(buf, off + 0x0A) == 1:
            module_file_off = find_bound_mz_module(buf, off)
            return LEHeader(
                le_off=off,
                num_pages=u32(buf, off + 0x14),
                page_size=u32(buf, off + 0x28),
                last_page_size=u32(buf, off + 0x2C),
                objtab_off=u32(buf, off + 0x40),
                objcnt=u32(buf, off + 0x44),
                objmap_off=u32(buf, off + 0x48),
                fixup_page_table_off=u32(buf, off + 0x68),
                fixup_record_table_off=u32(buf, off + 0x6C),
                data_pages_off=u32(buf, off + 0x80),
                entry_object=u32(buf, off + 0x18),
                entry_eip=u32(buf, off + 0x1C),
                module_file_off=module_file_off,
            )
    raise ValueError("未找到有效 LE 头")


def parse_objects(buf: bytes, le: LEHeader) -> list[ObjectRecord]:
    base = le.le_off + le.objtab_off
    out: list[ObjectRecord] = []
    for i in range(le.objcnt):
        off = base + i * 24
        out.append(ObjectRecord(
            index=i + 1,
            vsize=u32(buf, off),
            relbase=u32(buf, off + 4),
            flags=u32(buf, off + 8),
            page_map_index=u32(buf, off + 12),
            page_count=u32(buf, off + 16),
        ))
    return out


def page_number_from_map_entry(entry: bytes) -> int:
    # LE/LX object page map: 24-bit big-end page number + 8-bit flags.
    return (entry[0] << 16) | (entry[1] << 8) | entry[2]


def extract_page(buf: bytes, le: LEHeader, page_no_1based: int, page_is_last: bool) -> bytes:
    if page_no_1based == 0:
        return b"\x00" * le.page_size
    # LE +0x80 相对当前 executable module 开头。FD2.EXE 是 bound file：
    # 外层 DOS stub 后嵌有 MZ @0x25214，该 MZ 的 e_lfanew=0x28b8 指向
    # LE @0x27acc；因此 page 1 位于 0x25214+0x10e00=0x36014。
    off = le.module_file_off + le.data_pages_off + (page_no_1based - 1) * le.page_size
    size = le.last_page_size if page_is_last and le.last_page_size else le.page_size
    data = buf[off:off + size]
    if len(data) < le.page_size:
        data += b"\x00" * (le.page_size - len(data))
    return data[:le.page_size]


def build_relbase_image(buf: bytes, le: LEHeader, objects: Iterable[ObjectRecord]) -> bytearray:
    size = max(obj.relbase + obj.vsize for obj in objects)
    image = bytearray(size)
    objmap = le.le_off + le.objmap_off
    for obj in objects:
        for page_i in range(obj.page_count):
            map_index = obj.page_map_index - 1 + page_i
            entry = buf[objmap + map_index * 4: objmap + map_index * 4 + 4]
            page_no = page_number_from_map_entry(entry)
            global_page_index = map_index
            page_is_last = global_page_index == le.num_pages - 1
            page = extract_page(buf, le, page_no, page_is_last)
            dst = obj.relbase + page_i * le.page_size
            max_len = max(0, min(le.page_size, obj.relbase + obj.vsize - dst))
            image[dst:dst + max_len] = page[:max_len]
    return image


def build_code0_image(buf: bytes, le: LEHeader,
                      objects: list[ObjectRecord]) -> bytearray:
    # code0 是 object1 的真实 LE page view，不再把 bound module 前的外层 stub
    # 或 object2/3 错当作 object1 尾部。跨 object 数据由 relbase raw 表达。
    relbase = build_relbase_image(buf, le, objects)
    code = objects[0]
    return bytearray(relbase[code.relbase:code.relbase + code.vsize])


def patch_chkstk_for_analysis(image: bytearray) -> None:
    # `__chkstk @VA 0x3702f / code0 0x2702f` 保留原始字节；
    # canonical Ghidra 脚本只将其标为可返回 runtime helper。
    del image


def build_dual_clean_image(code0_image: bytes) -> bytearray:
    # object1 放在 relbase 0x10000，并把前 64 KiB 镜像到低地址以解析
    # near call。该文件是 code-only；DS object2/3 从 relbase raw 查看。
    image = bytearray(len(code0_image) + 0x10000)
    image[0x10000:] = code0_image
    mirror_size = min(0x10000, len(code0_image))
    image[0:mirror_size] = code0_image[0:mirror_size]
    return image


def decode_fixup_record_end(buf: bytes, p: int, limit: int) -> int | None:
    """Return end offset of one LE/LX fixup record, or None if truncated/unknown.

    Layout used here: SRC, FLAGS, SRCOFF/CNT, target data, optional additive,
    optional source list. This is enough to step through FD2.EXE records without
    byte-scanning false positives.
    """
    start = p
    if p + 2 > limit:
        return None
    src = buf[p]
    flags = buf[p + 1]
    p += 2

    source_list = bool(src & 0x20)
    source_type = src & 0x0f
    if source_list:
        if p + 1 > limit:
            return None
        source_count = buf[p]
        p += 1
    else:
        source_count = 1
        if p + 2 > limit:
            return None
        p += 2

    target_type = flags & 0x03
    obj_ordinal_size = 2 if (flags & 0x40) else 1
    target_offset_size = 4 if (flags & 0x10) else 2

    def need(n: int) -> bool:
        return p + n <= limit

    if target_type == 0:  # internal reference: object ordinal + optional target offset
        if not need(obj_ordinal_size):
            return None
        p += obj_ordinal_size
        # Selector fixups name an object only; offset/pointer fixups also carry a target offset.
        if source_type != 2:
            if not need(target_offset_size):
                return None
            p += target_offset_size
    elif target_type == 1:  # import by ordinal
        if not need(obj_ordinal_size):
            return None
        p += obj_ordinal_size
        ord_size = 1 if (flags & 0x80) else target_offset_size
        if not need(ord_size):
            return None
        p += ord_size
    elif target_type == 2:  # import by name
        if not need(obj_ordinal_size + target_offset_size):
            return None
        p += obj_ordinal_size + target_offset_size
    elif target_type == 3:  # internal entry table reference
        if not need(obj_ordinal_size):
            return None
        p += obj_ordinal_size
    else:
        return None

    if flags & 0x04:  # additive fixup
        add_size = 4 if (flags & 0x20) else 2
        if not need(add_size):
            return None
        p += add_size

    if source_list:
        if not need(source_count * 2):
            return None
        p += source_count * 2

    return p if p > start else None


def parse_fixups(buf: bytes, le: LEHeader, objects: list[ObjectRecord]) -> list[FixupRecord]:
    page_table = le.le_off + le.fixup_page_table_off
    rec_base = le.le_off + le.fixup_record_table_off
    # 计算 page index -> object/code0 基址。
    page_to_obj: dict[int, tuple[ObjectRecord, int]] = {}
    for obj in objects:
        for page_i in range(obj.page_count):
            page_to_obj[obj.page_map_index - 1 + page_i] = (obj, page_i)

    out: list[FixupRecord] = []
    for page_index in range(le.num_pages):
        start = u32(buf, page_table + page_index * 4)
        end = u32(buf, page_table + (page_index + 1) * 4)
        p = rec_base + start
        limit = rec_base + end
        while p < limit:
            rec_end = decode_fixup_record_end(buf, p, limit)
            if rec_end is None:
                break
            rec = buf[p:rec_end]
            # FD2 的全部记录都是 source type 0x07（32-bit offset）与
            # internal target，flags 只出现 0x00/0x10。任何其他组合失败。
            if rec[0] != 0x07 or rec[1] not in (0x00, 0x10):
                raise ValueError(f"unsupported FD2 fixup record: {rec.hex()}")
            expected_size = 9 if rec[1] & 0x10 else 7
            if len(rec) != expected_size or not (1 <= rec[4] <= len(objects)):
                raise ValueError(f"invalid FD2 fixup record: {rec.hex()}")
            source_type = rec[0]
            source_offset = struct.unpack_from("<h", rec, 2)[0]
            target_flags = rec[1]
            target_object = rec[4]
            target_offset = (u32(rec, 5) if target_flags & 0x10 else u16(rec, 5))
            obj, obj_page_i = page_to_obj[page_index]
            source_relbase = obj.relbase + obj_page_i * le.page_size + source_offset
            source_code0 = (obj_page_i * le.page_size + source_offset) if obj.index == 1 else source_relbase
            if source_relbase < obj.relbase or source_relbase + 4 > obj.relbase + obj.vsize:
                raise ValueError(f"fixup source outside object: page={page_index} src={source_offset}")
            target_linear = objects[target_object - 1].relbase + target_offset
            out.append(FixupRecord(
                page_index=page_index,
                source_offset=source_offset,
                source_addr_relbase=source_relbase,
                source_addr_code0=source_code0,
                source_type=source_type,
                target_flags=target_flags,
                target_object=target_object,
                target_offset=target_offset,
                target_linear=target_linear,
                record_file_off=p,
            ))
            p = rec_end
    return out


# Backward-compatible import for helper scripts.
parse_simple_fixups = parse_fixups


def apply_fixups(raw: bytes, fixups: list[FixupRecord]) -> bytearray:
    image = bytearray(raw)
    writes: dict[int, int] = {}
    for rec in fixups:
        old = writes.get(rec.source_addr_relbase)
        if old is not None and old != rec.target_linear:
            raise ValueError(f"conflicting fixups at {rec.source_addr_relbase:#x}")
        writes[rec.source_addr_relbase] = rec.target_linear
    for source, value in sorted(writes.items()):
        struct.pack_into("<I", image, source, value)
    return image


def write_relocation_manifest(path: Path, raw: bytes, fixups: list[FixupRecord]) -> None:
    seen: set[int] = set()
    with path.open("w", encoding="utf-8") as f:
        f.write("record_file_off\tpage\tsource_relbase\ttarget_object\ttarget_offset\ttarget_linear\traw_u32\tduplicate\n")
        for rec in fixups:
            raw_u32 = u32(raw, rec.source_addr_relbase)
            duplicate = int(rec.source_addr_relbase in seen)
            seen.add(rec.source_addr_relbase)
            f.write(
                f"0x{rec.record_file_off:x}\t{rec.page_index}\t0x{rec.source_addr_relbase:x}\t"
                f"{rec.target_object}\t0x{rec.target_offset:x}\t0x{rec.target_linear:x}\t"
                f"0x{raw_u32:x}\t{duplicate}\n"
            )


def write_fixup_report(path: Path, fixups: list[FixupRecord]) -> None:
    with path.open("w", encoding="utf-8") as f:
        f.write("# FD2.EXE LE internal offset fixup 完整清单\n")
        f.write("# source offset 按 signed i16 解释；flags 0x00/0x10 分别携带 u16/u32 target offset。\n")
        f.write("# columns: page src_relbase src_code0 src_off type target_obj target_off target_linear\n")
        for r in fixups:
            f.write(
                f"page={r.page_index:02d} "
                f"src_relbase=0x{r.source_addr_relbase:08x} "
                f"src_code0=0x{r.source_addr_code0:08x} "
                f"src_off=0x{r.source_offset:04x} "
                f"type=0x{r.source_type:02x} "
                f"target_obj={r.target_object} "
                f"target_off=0x{r.target_offset:04x} "
                f"target_linear=0x{r.target_linear:08x}\n"
            )


def write_fixup_note(path: Path, count: int, unique_count: int) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        "# FD2.EXE LE fixup 生成摘要\n\n"
        "完整记录由 `python3 tools/rebuild_fd2_analysis.py` 生成到 "
        "`tools/fd2_le_fixups.txt`，逐 source manifest 位于 "
        "`tools/fd2_le_relocation_manifest.tsv`。\n\n"
        f"记录数：{count}；唯一 source 数：{unique_count}。\n\n"
        "FD2 的记录全部为 32-bit internal offset source type `0x07`；"
        "target flags 仅为 `0x00` 或 `0x10`。raw-unrelocated 与 "
        "loader-relocated 镜像分开输出；不得把 relocated 镜像冒充原始代码。\n",
        encoding="utf-8",
    )


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    ap.add_argument("--tools-dir", type=Path, default=TOOLS)
    ap.add_argument("--docs-dir", type=Path, default=DOCS)
    args = ap.parse_args()

    buf = args.exe.read_bytes()
    le = parse_le(buf)
    objects = parse_objects(buf, le)
    relbase = build_relbase_image(buf, le, objects)
    code0 = build_code0_image(buf, le, objects)
    ghidra = bytearray(relbase)
    patch_chkstk_for_analysis(ghidra)
    dual = build_dual_clean_image(code0)
    patch_chkstk_for_analysis(dual)

    args.tools_dir.mkdir(parents=True, exist_ok=True)
    args.docs_dir.mkdir(parents=True, exist_ok=True)
    (args.tools_dir / "fd2_le_raw.bin").write_bytes(relbase)
    (args.tools_dir / "fd2_le_code0.bin").write_bytes(code0)
    (args.tools_dir / "fd2_le_ghidra_chkstk.bin").write_bytes(ghidra)
    (args.tools_dir / "fd2_le_dual_clean.bin").write_bytes(dual)
    fixups = parse_fixups(buf, le, objects)
    relocated = apply_fixups(relbase, fixups)
    write_fixup_report(args.tools_dir / "fd2_le_fixups.txt", fixups)
    write_relocation_manifest(
        args.tools_dir / "fd2_le_relocation_manifest.tsv", relbase, fixups)
    (args.tools_dir / "fd2_le_relocated_relbase.bin").write_bytes(relocated)
    write_fixup_note(
        args.docs_dir / "generated" / "le-fixups.txt", len(fixups),
        len({r.source_addr_relbase for r in fixups}))

    entry_obj = objects[le.entry_object - 1]
    print(f"bound MZ @ 0x{le.module_file_off:x}, LE @ 0x{le.le_off:x}, "
          f"data pages @ 0x{le.module_file_off + le.data_pages_off:x}, "
          f"pages={le.num_pages}, page_size=0x{le.page_size:x}")
    for obj in objects:
        print(f"obj{obj.index}: relbase=0x{obj.relbase:x} vsize=0x{obj.vsize:x} pages={obj.page_count}")
    print(f"entry: object{le.entry_object}:offset 0x{le.entry_eip:x} (relbase linear 0x{entry_obj.relbase + le.entry_eip:x})")
    print("wrote raw/code0/dual, relocated relbase, fixup report and relocation manifest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
