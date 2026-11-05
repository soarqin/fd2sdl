#!/usr/bin/env python3
"""用已保存的 IDA 数据库导出 Hex-Rays 伪 C 与反汇编，用于与 Ghidra 结果交叉核对。

先运行 `tools/ida/ida_fd2_export.py` 生成 `tools/ida-out/db/FD2.EXE.i64`。

用法：
    # 指定若干 VA
    python3 tools/ida/ida_fd2_decompile.py 0x20421 0x25977 0x3231b
    # 导出全部函数（可能较慢）
    python3 tools/ida/ida_fd2_decompile.py --all --out tools/ida-out/ida-decomp-all.c
    # 同时输出反汇编
    python3 tools/ida/ida_fd2_decompile.py 0x1f894 --asm
"""

from __future__ import annotations

import idapro  # noqa: E402  isort:skip

import argparse  # noqa: E402
import json  # noqa: E402
import sys  # noqa: E402
from pathlib import Path  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_IDB = ROOT / "tools" / "ida-out" / "db" / "FD2.EXE.i64"


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("vas", nargs="*", help="函数 VA（十六进制或十进制）")
    ap.add_argument("--idb", type=Path, default=DEFAULT_IDB)
    ap.add_argument("--all", action="store_true", help="导出全部函数")
    ap.add_argument("--out", type=Path, default=None)
    ap.add_argument("--asm", action="store_true", help="同时输出反汇编")
    ap.add_argument("--xrefs", action="store_true", help="输出调用者/被调用者")
    args = ap.parse_args()

    idapro.enable_console_messages(False)
    rc = idapro.open_database(str(args.idb), run_auto_analysis=False)
    if rc != 0:
        print(f"open_database failed rc={rc}", file=sys.stderr)
        return 1

    import ida_funcs
    import ida_hexrays
    import ida_kernwin
    import ida_nalt
    import ida_typeinf
    import ida_lines
    import ida_bytes
    import ida_ua
    import ida_xref
    import idautils
    import idc

    if not ida_hexrays.init_hexrays_plugin():
        print("Hex-Rays 不可用", file=sys.stderr)
        return 2

    targets: list[int] = []
    if args.all:
        targets = sorted(idautils.Functions())
    for v in args.vas:
        targets.append(int(v, 0))

    chunks: list[str] = []
    for ea in targets:
        f = ida_funcs.get_func(ea)
        if f is None:
            chunks.append(f"/* {ea:#x}: 无函数 */\n")
            continue
        name = ida_funcs.get_func_name(f.start_ea)
        flags = []
        if f.flags & ida_funcs.FUNC_LIB:
            flags.append("FLIRT-lib")
        if f.flags & ida_funcs.FUNC_THUNK:
            flags.append("thunk")
        if f.flags & ida_funcs.FUNC_NORET:
            flags.append("noreturn")
        tif = ida_typeinf.tinfo_t()
        proto = str(tif) if ida_nalt.get_tinfo(tif, f.start_ea) else ""
        header = (f"/* ---------------------------------------------------------------\n"
                  f" * IDA {ida_kernwin.get_kernel_version()} Hex-Rays: {name}\n"
                  f" * VA={f.start_ea:#x}..{f.end_ea:#x} OBJ=1 OFF={f.start_ea - 0x10000:#x}"
                  f" size={f.end_ea - f.start_ea:#x}"
                  + (f" [{', '.join(flags)}]" if flags else "") + "\n")
        if proto:
            header += f" * type: {proto}\n"
        cmt = ida_funcs.get_func_cmt(f.start_ea, 1) or ""
        if args.xrefs:
            callers = sorted({
                ida_funcs.get_func(x.frm).start_ea
                for x in idautils.XrefsTo(f.start_ea, 0)
                if ida_funcs.get_func(x.frm)
            })
            callees = set()
            for item in idautils.FuncItems(f.start_ea):
                for x in idautils.XrefsFrom(item, 0):
                    if x.type in (ida_xref.fl_CN, ida_xref.fl_CF):
                        callees.add(x.to)
            header += (f" * callers: {', '.join(hex(c) for c in callers) or '-'}\n"
                       f" * callees: {', '.join(hex(c) for c in sorted(callees)) or '-'}\n")
        try:
            hf = ida_hexrays.hexrays_failure_t()
            cf = ida_hexrays.decompile_func(f, hf, ida_hexrays.DECOMP_NO_WAIT)
            body = str(cf) if cf is not None else None
            fail = None if cf is not None else f"{hf.desc()} @ {hf.errea:#x}"
        except Exception as exc:
            body, fail = None, f"exception: {exc}"
        # Hex-Rays 会把 IDA 函数注释输出在函数体前面。只有在它没有输出时
        # （例如反编译失败），才由 banner 补上历史命名档案注释，避免重复。
        if cmt and (body is None or cmt.splitlines()[0] not in body):
            for cl in cmt.splitlines():
                header += f" * {cl}\n"
        header += " * --------------------------------------------------------------- */\n"
        chunks.append(header)
        if body is None:
            chunks.append(f"/* Hex-Rays 无法反编译（{fail}）；"
                          f"该函数为手写汇编，请用 --asm 查看反汇编。 */\n")
        else:
            chunks.append(body + "\n")
        if args.asm:
            lines = ["/* --- disassembly --- */"]
            for item in idautils.FuncItems(f.start_ea):
                dis = ida_lines.generate_disasm_line(item, ida_lines.GENDSM_REMOVE_TAGS)
                raw = (ida_bytes.get_bytes(item, ida_bytes.get_item_size(item)) or b"").hex()
                lines.append(f" * {item:#08x}  {raw:<20} {dis}")
            chunks.append("/*\n" + "\n".join(lines) + "\n */\n")
        chunks.append("\n")

    import ida_ida, ida_loader
    banner = (
        f"/* FD2.EXE 反编译输出（IDA Pro {ida_kernwin.get_kernel_version()} / Hex-Rays，idalib headless）\n"
        " *\n"
        " * 生成方式：\n"
        " *   python3 tools/ida/ida_fd2_export.py\n"
        " *   python3 tools/ida/fd2_name_audit.py --json tools/ida-out/ida-name-audit.json\n"
        " *   python3 tools/ida/ida_fd2_annotate.py --probe-only\n"
        " *   python3 tools/ida/ida_fd2_annotate.py\n"
        " *   python3 tools/ida/ida_fd2_decompile.py --all --xrefs --out <this file>\n"
        " *\n"
        f" * loader: {ida_loader.get_file_type_name()}；compiler: Watcom C++（IDA 自动识别）\n"
        " * 地址主键：LE relbase linear VA。object1 = VA 0x10000..0x4ef29，OFF = VA - 0x10000。\n"
        " *\n"
        " * 与旧 `docs/generated/ghidra-decomp-all.c` 的区别：\n"
        " *   - 输入是未经任何字节 patch 的原始 LE object；`__chkstk` 保留原样。\n"
        " *   - Watcom `push N; call __chkstk; <body>` 由 IDA 归为同一个函数，\n"
        " *     不再出现 wrapper/body 双函数与 `*_entry`/`*_core` 伪命名。\n"
        " *   - 函数名与 `[名字] 用途 / 依据 / 历史地址` 注释来自\n"
        " *     docs/reverse-engineering/function-names.md，经 IDA 独立复核后写入。\n"
        " *\n"
        " * 已知限制：伪 C 不等于原始源码；参数、返回类型和局部变量仍需按需复核。\n"
        " *   Watcom runtime / Miles AIL 区（VA >= 0x37000）多为寄存器约定或手写汇编，\n"
        " *   Hex-Rays 会给出寄存器形参，属预期现象。\n"
        " */\n\n")
    text = banner + "".join(chunks)
    if args.out:
        args.out.parent.mkdir(parents=True, exist_ok=True)
        args.out.write_text(text, encoding="utf-8", newline="\n")
        print(f"wrote {args.out} ({len(text)} bytes, {len(targets)} functions)")
    else:
        sys.stdout.write(text)
    idapro.close_database(save=False)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
