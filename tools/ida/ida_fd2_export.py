#!/usr/bin/env python3
"""用 idalib（IDA Pro 9.4）独立重建 FD2.EXE 分析基线并导出机器可读结果。

本脚本的目的不是取代 `tools/rebuild_fd2_analysis.py`，而是提供 **第三个完全独立的
LE loader 实现**（IDA 自带 `loaders/lx.so`）用于交叉印证：

- LE object → segment 布局（relbase、长度、类别）；
- LE transfer entry；
- relocation（fixup）source 集合、target линear 地址与写入宽度；
- loader 应用 relocation 之后的镜像字节；
- 函数边界、Watcom runtime FLIRT 命名与调用约定。

用法：
    python3 tools/ida/ida_fd2_export.py [--exe original_game/FD2.EXE]
                                        [--out tools/ida-out]
                                        [--no-analysis]

输出（默认写入被 Git 忽略的 `tools/ida-out/`）：
    ida-program.json     程序、segment、entry、fixup 统计与 hash
    ida-fixups.tsv       全部 fixup 逐条记录
    ida-functions.tsv    全部函数边界与命名
    ida-names.tsv         全部命名地址
    ida-image-<seg>.bin  IDA 加载后（已应用 relocation）的 segment 字节
    ida-strings.tsv       IDA 识别的字符串
"""

from __future__ import annotations

# idapro 必须是第一个 import（idalib 要求）。
import idapro  # noqa: E402  isort:skip

import argparse  # noqa: E402
import hashlib  # noqa: E402
import json  # noqa: E402
import os  # noqa: E402
import shutil  # noqa: E402
import struct  # noqa: E402
import sys  # noqa: E402
from pathlib import Path  # noqa: E402

import ida_auto  # noqa: E402
import ida_bytes  # noqa: E402
import ida_entry  # noqa: E402
import ida_fixup  # noqa: E402
import ida_frame  # noqa: E402
import ida_funcs  # noqa: E402
import ida_ida  # noqa: E402
import ida_idaapi  # noqa: E402
import ida_loader  # noqa: E402
import ida_nalt  # noqa: E402
import ida_name  # noqa: E402
import ida_segment  # noqa: E402
import ida_typeinf  # noqa: E402
import idautils  # noqa: E402
import idc  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]

FIXUP_TYPE_NAMES = {
    0: "FIXUP_OFF8",
    1: "FIXUP_OFF16",
    2: "FIXUP_SEG16",
    3: "FIXUP_PTR16",
    4: "FIXUP_OFF32",
    5: "FIXUP_PTR32",
    6: "FIXUP_HI8",
    7: "FIXUP_HI16",
    8: "FIXUP_LOW8",
    9: "FIXUP_LOW16",
    0x0C: "FIXUP_OFF64",
}

SEG_TYPE_NAMES = {
    0: "SEG_NORM", 1: "SEG_XTRN", 2: "SEG_CODE", 3: "SEG_DATA",
    4: "SEG_IMP", 6: "SEG_GRP", 7: "SEG_NULL", 9: "SEG_BSS",
    10: "SEG_ABSSYM", 11: "SEG_COMM", 12: "SEG_IMEM",
}


def sha256(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def collect_segments() -> list[dict]:
    out = []
    for i in range(ida_segment.get_segm_qty()):
        s = ida_segment.getnseg(i)
        out.append({
            "index": i,
            "name": ida_segment.get_segm_name(s),
            "start_ea": s.start_ea,
            "end_ea": s.end_ea,
            "size": s.size(),
            "bitness_code": s.bitness,
            "bits": {0: 16, 1: 32, 2: 64}.get(s.bitness),
            "sel": s.sel,
            "type_code": s.type,
            "type": SEG_TYPE_NAMES.get(s.type, str(s.type)),
            "perm": s.perm,
            "align": s.align,
            "comb": s.comb,
            "class": ida_segment.get_segm_class(s) or "",
        })
    return out


def collect_fixups() -> list[dict]:
    out = []
    ea = ida_fixup.get_first_fixup_ea()
    while ea != ida_idaapi.BADADDR and ea is not None:
        fd = ida_fixup.fixup_data_t()
        if ida_fixup.get_fixup(fd, ea):
            ftype = fd.get_type()
            size = fd.calc_size()
            stored = ida_bytes.get_bytes(ea, size) or b""
            out.append({
                "source_ea": ea,
                "type_code": ftype,
                "type": FIXUP_TYPE_NAMES.get(ftype, str(ftype)),
                "size": size,
                "sel": fd.sel,
                "off": fd.off,
                "displacement": fd.displacement,
                "flags": fd.get_flags(),
                "has_base": bool(fd.has_base()),
                "base": fd.get_base() if fd.has_base() else 0,
                "is_extdef": bool(fd.is_extdef()),
                "is_unused": bool(fd.is_unused()),
                "was_created": bool(fd.was_created()),
                "value": fd.get_value(ea),
                "stored_bytes": stored.hex(),
                "desc": fd.get_desc(ea),
            })
        nxt = ida_fixup.get_next_fixup_ea(ea)
        if nxt == ea:
            break
        ea = nxt
    return out


def func_cc_name(code: int) -> str:
    names = {
        0: "CM_CC_INVALID", 0x10: "CM_CC_UNKNOWN", 0x20: "CM_CC_VOIDARG",
        0x30: "CM_CC_CDECL", 0x40: "CM_CC_ELLIPSIS", 0x50: "CM_CC_STDCALL",
        0x60: "CM_CC_PASCAL", 0x70: "CM_CC_FASTCALL", 0x80: "CM_CC_THISCALL",
        0x90: "CM_CC_SWIFT", 0xA0: "CM_CC_SPOILED", 0xB0: "CM_CC_GOLANG",
        0xD0: "CM_CC_SPECIALE", 0xE0: "CM_CC_SPECIALP", 0xF0: "CM_CC_SPECIAL",
    }
    return names.get(code & 0xF0, hex(code))


def collect_functions() -> list[dict]:
    out = []
    for ea in idautils.Functions():
        f = ida_funcs.get_func(ea)
        if f is None:
            continue
        name = ida_funcs.get_func_name(ea)
        chunks = []
        it = ida_funcs.func_tail_iterator_t(f)
        ok = it.main()
        while ok:
            ch = it.chunk()
            chunks.append([ch.start_ea, ch.end_ea])
            ok = it.next()
        tif = ida_typeinf.tinfo_t()
        proto = ""
        cc = ""
        if ida_nalt.get_tinfo(tif, ea):
            proto = str(tif)
            fi = ida_typeinf.func_type_data_t()
            if tif.get_func_details(fi):
                cc = func_cc_name(fi.get_cc())
        out.append({
            "start_ea": f.start_ea,
            "end_ea": f.end_ea,
            "size": f.end_ea - f.start_ea,
            "name": name,
            "is_default_name": bool(name.startswith(("sub_", "nullsub_", "unknown_libname"))),
            "flags": f.flags,
            "is_lib": bool(f.flags & ida_funcs.FUNC_LIB),
            "is_thunk": bool(f.flags & ida_funcs.FUNC_THUNK),
            "is_noret": bool(f.flags & ida_funcs.FUNC_NORET),
            "is_far": bool(f.flags & ida_funcs.FUNC_FAR),
            "frame_size": ida_frame.get_frame_size(f),
            "frsize": f.frsize,
            "frregs": f.frregs,
            "argsize": f.argsize,
            "chunk_count": len(chunks),
            "chunks": chunks,
            "prototype": proto,
            "cc": cc,
            "first_bytes": (ida_bytes.get_bytes(f.start_ea, min(16, f.end_ea - f.start_ea)) or b"").hex(),
        })
    out.sort(key=lambda r: r["start_ea"])
    return out


def collect_names() -> list[dict]:
    out = []
    for ea, name in idautils.Names():
        out.append({
            "ea": ea,
            "name": name,
            "is_func": ida_funcs.get_func(ea) is not None and ida_funcs.get_func(ea).start_ea == ea,
            "is_public": bool(ida_name.is_public_name(ea)),
        })
    out.sort(key=lambda r: r["ea"])
    return out


def collect_entries() -> list[dict]:
    out = []
    for i in range(ida_entry.get_entry_qty()):
        ordv = ida_entry.get_entry_ordinal(i)
        out.append({
            "ordinal": ordv,
            "ea": ida_entry.get_entry(ordv),
            "name": ida_entry.get_entry_name(ordv) or "",
        })
    return out


def collect_strings() -> list[dict]:
    out = []
    try:
        import ida_strlist
        ida_strlist.build_strlist()
        n = ida_strlist.get_strlist_qty()
        for i in range(n):
            si = ida_strlist.string_info_t()
            if not ida_strlist.get_strlist_item(si, i):
                continue
            raw = ida_bytes.get_strlit_contents(si.ea, si.length, si.type)
            out.append({
                "ea": si.ea,
                "length": si.length,
                "type": si.type,
                "text": (raw or b"").decode("latin-1", "replace"),
            })
    except Exception as exc:  # pragma: no cover
        print(f"warning: string list failed: {exc}", file=sys.stderr)
    return out


def write_tsv(path: Path, rows: list[dict], columns: list[str]) -> None:
    with path.open("w", encoding="utf-8") as f:
        f.write("\t".join(columns) + "\n")
        for r in rows:
            vals = []
            for c in columns:
                v = r.get(c, "")
                if isinstance(v, bool):
                    v = int(v)
                elif isinstance(v, int) and c not in ("index", "length", "type",
                                                      "size", "chunk_count",
                                                      "ordinal", "bits",
                                                      "frame_size", "frsize",
                                                      "frregs", "argsize"):
                    v = f"0x{v:x}"
                elif isinstance(v, list):
                    v = ";".join(f"0x{a:x}-0x{b:x}" for a, b in v)
                vals.append(str(v).replace("\t", "\\t").replace("\n", "\\n"))
            f.write("\t".join(vals) + "\n")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--exe", type=Path, default=ROOT / "original_game" / "FD2.EXE")
    ap.add_argument("--out", type=Path, default=ROOT / "tools" / "ida-out")
    ap.add_argument("--no-analysis", action="store_true",
                    help="只做 loader 层导出，不跑自动分析（用于纯 relocation 校验）")
    ap.add_argument("--idb", type=Path, default=None,
                    help="从已存在的 IDB 导出（例如已由 ida_fd2_annotate.py 标注过）")
    args = ap.parse_args()

    exe = args.exe.resolve()
    out = args.out.resolve()
    out.mkdir(parents=True, exist_ok=True)

    idapro.enable_console_messages(True)
    if args.idb is not None:
        rc = idapro.open_database(str(args.idb.resolve()), run_auto_analysis=False)
        if rc != 0:
            print(f"open_database failed rc={rc}", file=sys.stderr)
            return 1
    else:
        work = out / ("db-noauto" if args.no_analysis else "db")
        if work.exists():
            shutil.rmtree(work)
        work.mkdir(parents=True)
        target = work / exe.name
        shutil.copy(exe, target)
        rc = idapro.open_database(str(target), run_auto_analysis=not args.no_analysis)
        if rc != 0:
            print(f"open_database failed rc={rc}", file=sys.stderr)
            return 1
        if not args.no_analysis:
            ida_auto.auto_wait()

    segments = collect_segments()
    fixups = collect_fixups()
    entries = collect_entries()
    functions = [] if args.no_analysis else collect_functions()
    names = [] if args.no_analysis else collect_names()
    strings = [] if args.no_analysis else collect_strings()

    seg_dumps = {}
    for s in segments:
        data = ida_bytes.get_bytes(s["start_ea"], s["size"])
        if data is None:
            data = b"".join(
                bytes([ida_bytes.get_byte(a)]) if ida_bytes.is_loaded(a) else b"\x00"
                for a in range(s["start_ea"], s["end_ea"]))
        fname = f"ida-image-{s['name']}-{s['start_ea']:#x}.bin"
        (out / fname).write_bytes(data)
        seg_dumps[s["name"]] = {
            "file": fname,
            "sha256": sha256(data),
            "size": len(data),
            "start_ea": s["start_ea"],
        }
        s["sha256"] = sha256(data)

    program = {
        "schema": "fd2-ida-export-v1",
        "ida_version": idc.__EA64__ if False else ida_pro_version(),
        "input_file": str(exe),
        "input_sha256": sha256(exe.read_bytes()),
        "file_type_name": ida_loader.get_file_type_name(),
        "filetype_code": ida_ida.inf_get_filetype(),
        "procname": ida_ida.inf_get_procname(),
        "app_bitness": ida_ida.inf_get_app_bitness(),
        "min_ea": ida_ida.inf_get_min_ea(),
        "max_ea": ida_ida.inf_get_max_ea(),
        "start_ea": ida_ida.inf_get_start_ea(),
        "start_ip": ida_ida.inf_get_start_ip(),
        "start_cs": ida_ida.inf_get_start_cs(),
        "start_sp": ida_ida.inf_get_start_sp(),
        "start_ss": ida_ida.inf_get_start_ss(),
        "compiler_id": ida_ida.inf_get_cc_id(),
        "compiler_name": ida_typeinf.get_compiler_name(ida_ida.inf_get_cc_id()),
        "auto_analysis": not args.no_analysis,
        "segments": segments,
        "segment_dumps": seg_dumps,
        "entries": entries,
        "fixup_count": len(fixups),
        "fixup_type_histogram": histogram(f"{r['type']}" for r in fixups),
        "fixup_flag_histogram": histogram(f"0x{r['flags']:x}" for r in fixups),
        "fixup_size_histogram": histogram(str(r["size"]) for r in fixups),
        "function_count": len(functions),
        "name_count": len(names),
        "string_count": len(strings),
    }

    (out / "ida-program.json").write_text(
        json.dumps(program, indent=2, ensure_ascii=False), encoding="utf-8")
    write_tsv(out / "ida-fixups.tsv", fixups,
              ["source_ea", "type", "size", "sel", "off", "displacement",
               "flags", "base", "value", "stored_bytes", "desc"])
    if functions:
        write_tsv(out / "ida-functions.tsv", functions,
                  ["start_ea", "end_ea", "size", "name", "is_default_name",
                   "is_lib", "is_thunk", "is_noret", "chunk_count", "chunks",
                   "frame_size", "frsize", "frregs", "argsize", "cc",
                   "prototype", "first_bytes"])
    if names:
        write_tsv(out / "ida-names.tsv", names, ["ea", "name", "is_func", "is_public"])
    if strings:
        write_tsv(out / "ida-strings.tsv", strings, ["ea", "length", "type", "text"])

    print(f"segments={len(segments)} fixups={len(fixups)} "
          f"functions={len(functions)} names={len(names)} strings={len(strings)}")
    for s in segments:
        print(f"  {s['name']:>8} {s['start_ea']:#010x}..{s['end_ea']:#010x} "
              f"size={s['size']:#x} bits={s['bits']} type={s['type']} sha256={s['sha256'][:16]}")
    idapro.close_database(save=args.idb is None)
    print(f"wrote export to {out}")
    return 0


def histogram(items) -> dict:
    h: dict[str, int] = {}
    for i in items:
        h[i] = h.get(i, 0) + 1
    return dict(sorted(h.items()))


def ida_pro_version() -> str:
    try:
        import ida_kernwin
        return ida_kernwin.get_kernel_version()
    except Exception:
        return "unknown"


if __name__ == "__main__":
    raise SystemExit(main())
