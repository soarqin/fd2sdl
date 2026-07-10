#!/usr/bin/env python3
"""Scan relocation-backed stage dispatch code for recognizable prologue calls.

用途：辅助追踪 `call *0x1d71(,%eax,4)` 这类按关卡索引分发的过场代码。
脚本不执行代码，只读取 LE fixup 记录和 `tools/fd2_le_code0.bin`，在每个
dispatch 入口附近列出 FDTXT 片段渲染、角色移动/等待等常见调用。
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from dump_fd2_fixup_table import collect_internal_offset_fixups

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_CODE = ROOT / "tools" / "fd2_le_code0.bin"
DEFAULT_EXE = ROOT / "original_game" / "FD2.EXE"

# 默认使用 code0：object1 从 0 开始，stage dispatch fixup 的 target_offset
# 可直接作为扫描地址。dual_clean 中 0x0-0xffff 额外镜像 object1 前 64K；
# 同一 helper 可能以低地址、code0 高地址或 relbase 线性地址出现。
# 这里只列与 stage script 追踪直接相关的调用。
CALL_NAMES = {
    0x036CC: "text_dialog_render_tokens(alias 0x136cc)",
    0x136CC: "text_dialog_render_tokens",
    0x00D25: "field_camera_pan_to (FUN_00010d25)",
    0x10D25: "field_camera_pan_to (FUN_00010d25)",
    0x00DB2: "field_movement_script_play (FUN_00010db2)",
    0x10DB2: "field_movement_script_play (FUN_00010db2)",
    0x0E296: "music_or_scene_call? (FUN_0000e296)",
    0x10B0E: "field_cutscene_setup_units_camera",
    0x20B0E: "field_cutscene_setup_units_camera",
    0x21FDC: "field_actor_is_hidden (FUN_00031fdc)",
    0x31FDC: "field_actor_is_hidden (FUN_00031fdc)",
    0x21C3A: "field_actor_range_status_set (FUN_00031c3a)",
    0x31C3A: "field_actor_range_status_set (FUN_00031c3a)",
    0x200BD: "field_actor_hide (FUN_000300bd)",
    0x300BD: "field_actor_hide (FUN_000300bd)",
    0x232C0: "field_camera_music_flash? (FUN_000332c0)",
    0x332C0: "field_camera_music_flash? (FUN_000332c0)",
}


def s32(b: bytes) -> int:
    return struct.unpack("<i", b)[0]


def u32_at(buf: bytes, off: int) -> int:
    return struct.unpack_from("<I", buf, off)[0]


def call_target(buf: bytes, pos: int) -> int | None:
    if pos + 5 > len(buf) or buf[pos] != 0xE8:
        return None
    return (pos + 5 + s32(buf[pos + 1:pos + 5])) & 0xFFFFFFFF


def target_aliases(t: int) -> list[int]:
    aliases = [t]
    if t >= 0x80000000:
        aliases.append(t & 0xFFFF)
    if 0x10000 <= t < 0x50000:
        aliases.append(t - 0x10000)
    return aliases


def fmt_arg(arg: tuple[str, int | str]) -> str:
    kind, val = arg
    if kind == "imm":
        return f"{val:#x}"
    if kind == "mem":
        return f"[{val:#x}]"
    if kind == "reg":
        return f"{val}"
    if kind == "lea_sp":
        return f"&stack[{int(val):+#x}]"
    return f"{kind}:{val:#x}"


def parse_pushes_before(buf: bytes, pos: int, max_args: int = 12) -> list[tuple[str, int | str]]:
    out: list[tuple[str, int | str]] = []
    p = pos
    while len(out) < max_args and p > 0:
        if p >= 2 and buf[p - 2] == 0x6A:
            out.append(("imm", buf[p - 1]))
            p -= 2
            continue
        if p >= 5 and buf[p - 5] == 0x68:
            out.append(("imm", u32_at(buf, p - 4)))
            p -= 5
            continue
        if p >= 6 and buf[p - 6] == 0xFF and buf[p - 5] == 0x35:
            out.append(("mem", u32_at(buf, p - 4)))
            p -= 6
            continue
        if p >= 1 and 0x50 <= buf[p - 1] <= 0x57:
            reg = buf[p - 1] - 0x50
            # Common Watcom pattern for stack-local byte arrays:
            #   lea reg, [esp + disp8]
            #   push reg
            if (p >= 5 and buf[p - 5] == 0x8D and buf[p - 3] == 0x24 and
                    ((buf[p - 4] >> 3) & 7) == reg):
                out.append(("lea_sp", buf[p - 2]))
                p -= 5
                continue
            regs = ["eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi"]
            out.append(("reg", regs[reg]))
            p -= 1
            continue
        break
    return out


def text_fragment_from_pushes(args: list[tuple[str, int | str]]) -> int | None:
    # cdecl 最近 call 的 push 是第 1 参数：push [DAT_00003a79]；上一项是 fragment。
    if len(args) < 2:
        return None
    if args[0] == ("mem", 0x3A79) and args[1][0] == "imm":
        return args[1][1]
    return None


def stage_targets(exe: Path, ds: int, count: int, code: Path) -> list[int | None]:
    fixups = collect_internal_offset_fixups(exe)
    base = 0x50000 + ds
    use_code0_offsets = "code0" in code.name
    out: list[int | None] = []
    for i in range(count):
        hit = fixups.get(base + i * 4)
        if not hit:
            out.append(None)
            continue
        target_object, target_offset, target_linear = hit
        if target_object == 1 and use_code0_offsets:
            out.append(target_offset)
        else:
            out.append(target_linear)
    return out


def scan_entry(buf: bytes, entry: int, radius_before: int, radius_after: int,
               stop_at_flow_end: bool = False) -> list[str]:
    start = max(0, entry - radius_before)
    end = min(len(buf), entry + radius_after)
    if stop_at_flow_end and radius_before == 0:
        p = entry
        while p < end:
            op = buf[p]
            if op in (0xC3, 0xCB, 0xC2, 0xCA):
                end = min(end, p + 1)
                break
            if op in (0xE9, 0xEA, 0xEB):
                end = min(end, p + 5)
                break
            p += 1
    lines: list[str] = []
    for pos in range(start, end - 4):
        if buf[pos] != 0xE8:
            continue
        t = call_target(buf, pos)
        if t is None:
            continue
        name = None
        for alias in target_aliases(t):
            name = CALL_NAMES.get(alias)
            if name:
                break
        if not name:
            continue
        args = parse_pushes_before(buf, pos)
        frag = text_fragment_from_pushes(args)
        if frag is not None:
            lines.append(f"  {pos:#07x}: FDTXT fragment {frag} via {name}")
        else:
            arg_text = ", ".join(fmt_arg(a) for a in args)
            lines.append(f"  {pos:#07x}: call {name} args=[{arg_text}]")
    return lines


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", type=Path, default=DEFAULT_EXE)
    ap.add_argument("--code", type=Path, default=DEFAULT_CODE)
    ap.add_argument("--ds", type=lambda s: int(s, 0), default=0x1D71)
    ap.add_argument("--count", type=lambda s: int(s, 0), default=30)
    ap.add_argument("--stage", type=lambda s: int(s, 0), default=None,
                    help="只扫描指定 table index；默认扫描所有 count 项")
    ap.add_argument("--before", type=lambda s: int(s, 0), default=0x40)
    ap.add_argument("--after", type=lambda s: int(s, 0), default=0x260)
    ap.add_argument("--linear", action="store_true",
                    help="从入口线性扫描到第一个 ret/jmp；需配合 --before 0 使用，减少跨函数误报")
    args = ap.parse_args()

    buf = args.code.read_bytes()
    targets = stage_targets(args.exe, args.ds, args.count, args.code)
    indices = [args.stage] if args.stage is not None else list(range(args.count))

    print(f"stage dispatch DS:{args.ds:#06x}, count={args.count}, code={args.code}")
    for i in indices:
        if i < 0 or i >= len(targets):
            continue
        entry = targets[i]
        if entry is None:
            print(f"stage[{i:02d}]: <no fixup>")
            continue
        print(f"stage[{i:02d}]: entry={entry:#07x}, scan={max(0, entry-args.before):#07x}..{min(len(buf), entry+args.after):#07x}")
        lines = scan_entry(buf, entry, args.before, args.after, args.linear)
        if lines:
            print("\n".join(lines))
        else:
            print("  <no recognized calls in window>")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
