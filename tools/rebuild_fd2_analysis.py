#!/usr/bin/env python3
"""重建 FD2.EXE 的可分析镜像与 LE fixup 清单。

关键原则：
- 不把 LE fixup 直接写回代码页。FD2 的 near call 使用代码段 offset，
  而数据/资源句柄经由 DS offset 访问；上一版脚本把 fixup 当作普通线性
  patch 写入，导致 0x44ab2 启动函数中的指令被覆盖。
- `__chkstk` 已迁移到 dual `0x5c243` / code0 `0x4c243`；保留原始 wrapper，
  不再通过字节 patch 改变分析输入。

输出：
  tools/fd2_le_raw.bin              object 按 LE relbase 摆放，未应用 fixup
  tools/fd2_le_code0.bin            bound CS 从 0 开始，包含 object1 后的运行库尾部
  tools/fd2_le_ghidra_chkstk.bin    兼容文件名；当前与 raw 相同，不做未验证 patch
  tools/fd2_le_dual_clean.bin       code-only：完整 CS 放在 0x10000，并镜像低 64K
  tools/fd2_le_fixups.txt          fixup 记录清单（只作索引，不是 patch 脚本）
  docs/le-fixups.txt                简短说明，避免把巨大记录误当作 patch 输入
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


# 本工程只输出形如 `SRC=0x07 FLAGS=0x00 <src:u16> <obj:1..3> <off:u16>` 的简单记录。
# LE fixup 表允许变长记录；这里按记录结构顺序跳过未知类型，但清单仍只作索引，
# 不作为完整 loader 实现。
SIMPLE_FIXUP_SIZE = 7


def u16(buf: bytes, off: int) -> int:
    return struct.unpack_from("<H", buf, off)[0]


def u32(buf: bytes, off: int) -> int:
    return struct.unpack_from("<I", buf, off)[0]


def parse_le(buf: bytes) -> LEHeader:
    # DOS4GW stub 的 e_lfanew 不可靠，直接搜索真正的 LE 头。
    candidates = [i for i in range(len(buf) - 2) if buf[i:i + 2] == b"LE"]
    for off in candidates:
        if off + 0x90 <= len(buf) and u16(buf, off + 8) == 2 and u16(buf, off + 0x0A) == 1:
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
    # LE header +0x80 是相对文件开头的 data-page file offset，不是
    # 相对 LE header。FD2 是 bound LE，页面区实际位于 LE header 之前；
    # 再加 le_off 会把每个 object page 错读到后方的其他内容。
    off = le.data_pages_off + (page_no_1based - 1) * le.page_size
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
    # FD2 是 bound LE：object1 声明 vsize=0x3ef29，但其后的剧情 handler、
    # Watcom 运行库和辅助代码仍位于同一 bound payload。code0 统一定义为
    # `file_offset - data_pages_off`，覆盖 payload 余下全部字节。它是只读
    # CS/代码分析视图；DS object2/3 继续由 fd2_le_raw.bin 单独表达。
    del objects
    return bytearray(buf[le.data_pages_off:])


def patch_chkstk_for_analysis(image: bytearray) -> None:
    # `__chkstk @dual 0x5c243 / code0 0x4c243` 保留原始 wrapper；
    # Ghidra 脚本只将其标为普通可返回函数，不修改输入字节。
    del image


def build_dual_clean_image(code0_image: bytes) -> bytearray:
    # 兼容旧 Ghidra relbase 视图：完整 bound CS 窗口放在 0x10000，
    # 并把前 64 KiB 镜像到低地址以解析 near call。该文件是 code-only；
    # DS object2/3 必须从 fd2_le_raw.bin 单独查看。
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


def parse_simple_fixups(buf: bytes, le: LEHeader, objects: list[ObjectRecord]) -> list[FixupRecord]:
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
            # 简单 internal offset 记录。
            if len(rec) == SIMPLE_FIXUP_SIZE and rec[0] == 0x07 and rec[1] == 0x00 and 1 <= rec[4] <= len(objects):
                source_type = rec[0]
                source_offset = u16(rec, 2)
                target_flags = rec[1]
                target_object = rec[4]
                target_offset = u16(rec, 5)
                obj, obj_page_i = page_to_obj.get(page_index, (objects[0], page_index))
                source_relbase = obj.relbase + obj_page_i * le.page_size + source_offset
                source_code0 = (obj_page_i * le.page_size + source_offset) if obj.index == 1 else source_relbase
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
                ))
            p = rec_end
    return out


def write_fixup_report(path: Path, fixups: list[FixupRecord]) -> None:
    with path.open("w", encoding="utf-8") as f:
        f.write("# FD2.EXE LE fixup 简单记录清单（只作索引，不可直接 patch 代码页）\n")
        f.write("# 仅输出形如 07 00 <src:u16> <obj:1..3> <off:u16> 的记录；变长记录未展开。\n")
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


def write_fixup_note(path: Path, count: int) -> None:
    path.write_text(
        "# FD2.EXE LE fixup 说明\n\n"
        "完整记录由 `python3 tools/rebuild_fd2_analysis.py` 生成到 "
        "`tools/fd2_le_fixups.txt`。\n\n"
        f"当前可解析简单记录数：{count}。\n\n"
        "这些记录只用于定位 DS/global 引用和保留 relocation 证据，"
        "不得直接写回代码镜像。上一版错误地直接 patch 代码页，"
        "导致 `boot_intro_title` 主体 @0x44ab2 被破坏。\n",
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
    fixups = parse_simple_fixups(buf, le, objects)
    write_fixup_report(args.tools_dir / "fd2_le_fixups.txt", fixups)
    write_fixup_note(args.docs_dir / "le-fixups.txt", len(fixups))

    entry_obj = objects[le.entry_object - 1]
    print(f"LE @ 0x{le.le_off:x}, pages={le.num_pages}, page_size=0x{le.page_size:x}")
    for obj in objects:
        print(f"obj{obj.index}: relbase=0x{obj.relbase:x} vsize=0x{obj.vsize:x} pages={obj.page_count}")
    print(f"entry: object{le.entry_object}:offset 0x{le.entry_eip:x} (relbase linear 0x{entry_obj.relbase + le.entry_eip:x})")
    print("wrote tools/fd2_le_raw.bin, tools/fd2_le_code0.bin, tools/fd2_le_ghidra_chkstk.bin, tools/fd2_le_dual_clean.bin")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
