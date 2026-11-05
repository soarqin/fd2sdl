#!/usr/bin/env python3
"""把审计后的语义命名与历史注释写回 IDA 数据库，并修复 Hex-Rays 调用分析失败。

前置：
    python3 tools/ida/ida_fd2_export.py                 # 生成 tools/ida-out/db/FD2.EXE.i64
    python3 tools/ida/fd2_name_audit.py --json tools/ida-out/ida-name-audit.json

## 为什么分两阶段

Hex-Rays 在反编译时会把 **猜测出来的函数原型写回数据库**。如果先做一遍全量
反编译再保存 IDB，`__chkstk` 之类的 helper 会被猜成
`void __fastcall(__int32 eax0, int edx0, int ebx0, int ecx0, …)`，随后这些幽灵
寄存器参数会污染全部 541 个 `__chkstk` 调用者的签名。实测：干净库全量反编译
得到 220 个带幽灵寄存器参数的签名；若把猜测类型存回再跑一遍会涨到 434 个。

因此本脚本：

* `--probe-only`：在 **临时副本** 上做全量反编译，只用来定位失败函数并按调用点证据
  推导原型，把结果写成 JSON。副本用完即弃，不污染权威库。
* 默认（apply 阶段）：在权威库上 **只写** 命名、注释和 probe 阶段确认的原型，
  完全不做反编译，因此不会留下任何猜测类型。

## 原型推导依据

Watcom 会让多个函数共用同一段 `add esp,N; pop …; retn` 尾码，被调用者以 `jmp`
跳进别的函数体内。IDA 的函数只能拥有一段 chunk，Hex-Rays 于是无法推断被调用者
的栈效果，报 `call analysis failed`。修法不是猜测，而是用调用点证据：调用后紧跟
的 `add esp,N` 直接给出该被调用者的栈参数个数（FD2 为 caller-cleans）。只有当
全部调用点给出同一个数值时才采用。

本脚本 **不修改任何机器码字节**，与 Ghidra 流程里 patch `__chkstk` 的做法性质不同。

用法：
    python3 tools/ida/ida_fd2_annotate.py --probe-only   # 生成原型建议 JSON
    python3 tools/ida/ida_fd2_annotate.py           # 写入命名/注释/原型并保存
"""

from __future__ import annotations

import idapro  # noqa: E402  isort:skip

import argparse  # noqa: E402
import collections  # noqa: E402
import json  # noqa: E402
import shutil  # noqa: E402
import sys  # noqa: E402
from pathlib import Path  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tools" / "ida-out"
DEFAULT_IDB = OUT / "db" / "FD2.EXE.i64"
DEFAULT_AUDIT = OUT / "ida-name-audit.json"
DEFAULT_PROBE = OUT / "ida-prototype-probe.json"

# 历史命名里表示 “wrapper / body / runtime 变体” 的后缀。
# IDA 把 Watcom `__chkstk` wrapper 与 body 合成同一个函数，因此这些变体应折叠。
VARIANT_SUFFIXES = ("_entry", "_core", "_runtime", "_body")

# IDA 由 Watcom runtime FLIRT 确认的入口名，保留不覆盖。
RESERVED = {"main", "start"}

# 机器码 + 已有文档同时确认的 anchor 命名。
# 只放证据充分、且历史档案没有登记的项；不写入类型（实测给 `__chkstk`
# 写原型会让 Hex-Rays 反而给出 `@<eax>` 幽灵参数，签名质量变差）。
CONFIRMED_ANCHORS = {
    # docs/reverse-engineering/address-mapping.md 已确认 `__chkstk @VA 0x3702f`；
    # IDA 机器码：`xchg eax,[esp+4]; call probe; mov eax,[esp+4]; retn 4`
    # —— 保留 EAX、自行清 4 字节参数，519 个直接调用点。
    # 注意：**不能**把它命名为 `__chkstk`/`alloca`/`_alloca_probe`。IDA 对这些魔法
    # 名字有内置 alloca 语义，会把 `push N; call` 当成动态栈分配。实测命名为
    # `__chkstk` 后全量反编译出现 500 处伪 `alloca()`，带寄存器幽灵参数的签名从
    # 220 涨到 434；改用中性名 `watcom_stack_check` 可保持 220/2。
    0x3702F: ("watcom_stack_check",
              "Watcom 栈检查 helper（文档中的 `__chkstk` @VA 0x3702f）：机器码为 "
              "`xchg eax,[esp+4]; call probe; mov eax,[esp+4]; retn 4`，保留 EAX 并"
              "自清 4 字节参数。`push N; call watcom_stack_check` 是函数序言，"
              "不是调用参数，也不做动态栈分配。519 个直接调用点。"),
    0x37042: ("watcom_stack_check_probe",
              "`watcom_stack_check` 内层探测：比较请求帧与 `dword_52814` 栈限，"
              "越界时走 SS 校验/终止路径。"),
}

# 同一 VA 出现多个历史语义名且语义不同时的显式主名裁决。
# 只在 alias 之间做选择或合并，不引入无证据的新语义。
PRIMARY_OVERRIDES = {
    # 0x1f894 同时被登记为 boot_intro_title(_entry) 和 title_action_menu：
    # 机器码是同一个函数——播放片头动画、绘制标题、再返回菜单选择结果。
    0x1F894: "boot_intro_title_menu",
}


def collect_callsite_evidence(idc, idautils, ida_bytes) -> dict[int, collections.Counter]:
    ev: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    for fea in idautils.Functions():
        for item in idautils.FuncItems(fea):
            if idc.print_insn_mnem(item) != "call":
                continue
            if idc.get_operand_type(item, 0) != idc.o_near:
                continue
            target = idc.get_operand_value(item, 0)
            nxt = item + ida_bytes.get_item_size(item)
            mnem = idc.print_insn_mnem(nxt)
            if mnem == "add" and idc.print_operand(nxt, 0) == "esp":
                ev[target][idc.get_operand_value(nxt, 1) // 4] += 1
            elif mnem in ("retn", "ret", "jmp", "pop", "leave"):
                ev[target]["tail"] += 1  # 尾调用/共享尾码：不提供清栈证据
            else:
                ev[target][0] += 1
    return ev


def unique_stack_args(counter: collections.Counter) -> int | None:
    nums = {k: v for k, v in counter.items() if isinstance(k, int)}
    if len(nums) != 1:
        return None
    return next(iter(nums))


def run_probe(args) -> int:
    work = args.probe.resolve().parent / "probe-db"
    if work.exists():
        raise FileExistsError("Use a new isolated probe output directory; preserve previous DB evidence")
    shutil.copytree(args.idb.parent, work)
    idapro.enable_console_messages(False)
    rc = idapro.open_database(str(work / args.idb.name), run_auto_analysis=False)
    if rc != 0:
        print(f"open_database failed rc={rc}", file=sys.stderr)
        return 1

    import ida_bytes
    import ida_funcs
    import ida_hexrays
    import ida_nalt
    import ida_typeinf
    import idautils
    import idc

    if not ida_hexrays.init_hexrays_plugin():
        print("Hex-Rays 不可用", file=sys.stderr)
        return 2

    evidence = collect_callsite_evidence(idc, idautils, ida_bytes)

    def failures() -> list[tuple[int, int, str]]:
        out = []
        for fea in sorted(idautils.Functions()):
            hf = ida_hexrays.hexrays_failure_t()
            cf = ida_hexrays.decompile_func(ida_funcs.get_func(fea), hf,
                                            ida_hexrays.DECOMP_NO_WAIT)
            if cf is None:
                out.append((fea, hf.errea, hf.desc()))
        return out

    proposals: dict[int, dict] = {}
    rounds = []
    cur = failures()
    initial = len(cur)
    print(f"初始 Hex-Rays 失败函数: {initial}")
    for rnd in range(args.max_rounds):
        if not cur:
            break
        applied = 0
        for fea, errea, desc in cur:
            if "call analysis failed" not in desc:
                continue
            if idc.print_insn_mnem(errea) != "call":
                continue
            if idc.get_operand_type(errea, 0) != idc.o_near:
                continue
            callee = idc.get_operand_value(errea, 0)
            if callee == 0x3702F or callee in proposals or ida_nalt.is_userti(callee):
                continue
            n = unique_stack_args(evidence.get(callee, collections.Counter()))
            if n is None:
                continue
            proto = ("int __cdecl f(void);" if n == 0 else
                     "int __cdecl f(" + ", ".join(["int"] * n) + ");")
            tif = ida_typeinf.tinfo_t()
            guessed = str(tif) if ida_nalt.get_tinfo(tif, callee) else None
            if not idc.SetType(callee, proto):
                continue
            applied += 1
            proposals[callee] = {
                "callee": f"{callee:#x}",
                "callee_name": ida_funcs.get_func_name(callee),
                "stack_args": n,
                "prototype": proto,
                "triggered_by": f"{fea:#x}",
                "err_ea": f"{errea:#x}",
                "hexrays_guessed_prototype": guessed,
                "callsite_evidence": {str(k): v for k, v in evidence[callee].items()},
            }
        ida_hexrays.clear_cached_cfuncs()
        nxt = failures()
        rounds.append({"round": rnd, "prototypes_applied": applied,
                       "failures_before": len(cur), "failures_after": len(nxt)})
        print(f"  round {rnd}: 写入 {applied} 个原型, 失败 {len(cur)} -> {len(nxt)}")
        cur = nxt
        if applied == 0:
            break

    payload = {
        "schema": "fd2-ida-prototype-probe-v1",
        "note": ("在一次性副本上定位 Hex-Rays 失败函数，并按调用点 `add esp,N` "
                 "证据推导被调用者栈参数个数。不修改机器码。"),
        "initial_failures": initial,
        "rounds": rounds,
        "prototypes": list(proposals.values()),
        "remaining_failures": [
            {"func": f"{a:#x}", "name": ida_funcs.get_func_name(a),
             "err_ea": f"{b:#x}", "desc": c} for a, b, c in cur],
    }
    args.probe.parent.mkdir(parents=True, exist_ok=True)
    args.probe.write_text(json.dumps(payload, indent=1, ensure_ascii=False),
                          encoding="utf-8", newline="\n")
    print(f"wrote {args.probe}  (剩余失败 {len(cur)})")
    idapro.close_database(save=False)
    shutil.rmtree(work, ignore_errors=True)
    return 0


def pick_primary(va: int, names: list[str]) -> tuple[str, list[str]]:
    if va in PRIMARY_OVERRIDES:
        p = PRIMARY_OVERRIDES[va]
        return p, [n for n in names if n != p]
    base = [n for n in names if not n.endswith(VARIANT_SUFFIXES)]
    pool = base or names
    primary = sorted(pool, key=lambda n: (len(n), n))[0]
    return primary, [n for n in names if n != primary]


def run_apply(args) -> int:
    audit = json.loads(args.audit.read_text(encoding="utf-8"))
    probe = (json.loads(args.probe.read_text(encoding="utf-8"))
             if args.probe.exists() else {"prototypes": [], "remaining_failures": []})

    idapro.enable_console_messages(False)
    rc = idapro.open_database(str(args.idb), run_auto_analysis=False)
    if rc != 0:
        print(f"open_database failed rc={rc}", file=sys.stderr)
        return 1

    import ida_funcs
    import ida_name
    import ida_nalt
    import idc

    report: dict = {"schema": "fd2-ida-annotate-v1"}

    # ------------------------------------------------------ 1. probe 阶段确认的原型
    applied, rejected = [], []
    for p in probe["prototypes"]:
        ea = int(p["callee"], 16)
        if ea == 0x3702F:
            raise ValueError("Forbidden explicit prototype for watcom_stack_check")
        if idc.SetType(ea, p["prototype"]):
            ida_nalt.set_userti(ea)   # 标记为已确认，后续反编译不再猜测
            applied.append(p)
        else:
            rejected.append(p)
    report["prototypes"] = {
        "method": "callsite `add esp,N` 证据 -> 显式 __cdecl 原型；不修改机器码字节",
        "applied": applied, "rejected": rejected,
        "remaining_hexrays_failures": probe["remaining_failures"],
    }
    print(f"写入 {len(applied)} 个原型（拒绝 {len(rejected)}）")

    # ------------------------------------------------------ 2. 机器码确认的 anchor
    anchors = []
    for ea, (nm, note) in sorted(CONFIRMED_ANCHORS.items()):
        f = ida_funcs.get_func(ea)
        if f is None or f.start_ea != ea:
            continue
        old_name = ida_funcs.get_func_name(ea)
        if ida_name.set_name(ea, nm, ida_name.SN_CHECK | ida_name.SN_FORCE):
            idc.set_func_cmt(ea, f"[{nm}] {note}", 1)
            anchors.append({"va": f"{ea:#x}", "from": old_name, "to": nm})
    report["confirmed_anchors"] = anchors
    print(f"写入 {len(anchors)} 个机器码确认 anchor 命名")

    # ------------------------------------------------------ 3. 语义命名与注释
    by_va: dict[int, list[dict]] = collections.defaultdict(list)
    for row in audit["rows"]:
        if row["resolution"] not in ("exact", "chkstk-body", "exact-conflict"):
            continue
        if not row["semantic_name"] or row["canonical_va"] is None:
            continue
        by_va[row["canonical_va"]].append(row)

    renamed, skipped, comments = [], [], 0
    for va, rows in sorted(by_va.items()):
        f = ida_funcs.get_func(va)
        if f is None or f.start_ea != va:
            skipped.append({"va": f"{va:#x}", "reason": "不是 IDA 函数入口"})
            continue
        names = sorted({r["semantic_name"] for r in rows})
        primary, aliases = pick_primary(va, names)
        old = ida_funcs.get_func_name(va)
        flirt_named = bool(f.flags & ida_funcs.FUNC_LIB) and not old.startswith(
            ("sub_", "nullsub_", "unknown_libname"))
        if old in RESERVED or flirt_named:
            # FLIRT 从真实 Watcom v9/1.5 runtime 签名匹配出的符号名，比历史手工
            # 猜名证据更强（例如历史 `vga_clear` 实为通用 `memset`）。保留 IDA 名，
            # 历史名降级为 alias 并在 corrections 表中记录。
            primary, aliases = old, names
        elif ida_name.set_name(va, primary, ida_name.SN_CHECK | ida_name.SN_FORCE):
            renamed.append({"va": f"{va:#x}", "from": old, "to": primary,
                            "aliases": aliases})
        else:
            skipped.append({"va": f"{va:#x}", "reason": f"set_name 失败: {primary}"})

        lines = []
        if aliases:
            lines.append("别名(历史登记): " + ", ".join(aliases))
        for r in rows:
            legacy = f"历史地址列 `{r['address_cell']}`"
            if r["legacy_body_va"] is not None:
                legacy += (f"（`{r['legacy_body_va']:#x}` 是 __chkstk body，"
                           f"入口为 `{va:#x}`）")
            lines.append(f"[{r['semantic_name']}] {r['purpose']}")
            lines.append(f"  依据: {r['evidence']}")
            lines.append(f"  {legacy}；旧 Ghidra 名 `{r['ghidra_cell']}`；"
                         f"判定 {r['resolution']}")
        idc.set_func_cmt(va, "\n".join(lines), 1)
        comments += 1

    report["naming"] = {
        "source": ("docs/reverse-engineering/function-names-legacy.md "
                   "via tools/ida/fd2_name_audit.py"),
        "primary_overrides": {f"{k:#x}": v for k, v in PRIMARY_OVERRIDES.items()},
        "renamed": renamed, "skipped": skipped, "commented": comments,
        "collisions": [{"va": f"{va:#x}",
                        "names": sorted({r["semantic_name"] for r in rows})}
                       for va, rows in sorted(by_va.items())
                       if len({r["semantic_name"] for r in rows}) > 1],
    }
    print(f"重命名 {len(renamed)} 个函数，写入 {comments} 条注释，跳过 {len(skipped)}")

    if args.report:
        args.report.parent.mkdir(parents=True, exist_ok=True)
        args.report.write_text(json.dumps(report, indent=1, ensure_ascii=False),
                              encoding="utf-8", newline="\n")
        print(f"wrote {args.report}")
    idapro.close_database(save=True)
    return 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--idb", type=Path, default=DEFAULT_IDB)
    ap.add_argument("--audit", type=Path, default=DEFAULT_AUDIT)
    ap.add_argument("--probe", type=Path, default=DEFAULT_PROBE)
    ap.add_argument("--report", type=Path, default=OUT / "ida-annotate-report.json")
    ap.add_argument("--max-rounds", type=int, default=6)
    ap.add_argument("--probe-only", dest="do_probe", action="store_true",
                    help="只做 probe，把原型建议写成 JSON")
    args = ap.parse_args()
    return run_probe(args) if args.do_probe else run_apply(args)


if __name__ == "__main__":
    raise SystemExit(main())
