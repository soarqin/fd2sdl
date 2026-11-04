#!/usr/bin/env python3
"""合并 IDA 独立分析与历史命名档案，生成规范函数命名登记表。

输入：
    tools/ida-out/ida-name-audit.json    fd2_name_audit.py 的逐行判定
    tools/ida-out/ida-functions.tsv      从已标注 IDB 导出的函数表
    tools/ida-out/ida-annotate-report.json

输出：
    docs/generated/ida-canonical/fd2-ida-function-registry.json
    docs/reverse-engineering/function-names.md 的表格正文（--markdown）

规范主键统一为 LE relbase linear VA；`OFF` 为 object1 内偏移（`VA - 0x10000`）。
"""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "tools" / "ida-out"
CODE0_BASE = 0x10000

# 历史档案里被 IDA 复核推翻或需要澄清的结论。
# 原始说法保留在 `records[].purpose/evidence` 中，更正以独立字段附加，
# 便于追溯「谁在什么依据下改了什么」。详见 docs/reverse-engineering/ida-crosscheck.md。
CORRECTIONS: dict[int, list[dict]] = {
    0x112A5: [{
        "target": "evidence",
        "was": "持久玩家构造与活动 stage 模板构造尚未区分完整初始字段",
        "now": "本函数在独立 roster 末尾调用 sub_1145A @0x1145a；"
               "从 object3 0x61da1 的 32 条 24-byte base 与 0x620a1 的 "
               "11-byte growth 读取初始等级、4-byte 法术位图、装备及库存。"
               "合计写 record 58 字节，保留 00..04、17、19、28..30、"
               "32..36、3d；3b 为 movement，确实写入。SDL 仅成功时提交记录。",
        "why": "已发布权威伪 C sub_112A5（661..729 行）与 sub_1145A"
               "（734..770 行），由 sub_4E838/sub_4E821 取表；"
               "LE page map 只读提取并逐行复核全部 32 条原表。"
               "派生函数只对 flags &0x40 的装备累计 AP/DP/HIT/EV；"
               "初始状态已清零，可复用现有装备派生。保留中性函数名，"
               "未新建 IDB 或修改权威伪 C。",
    }],
    0x37910: [{
        "target": "semantic_name",
        "was": "vga_clear（清屏 memset 0xa0000）",
        "now": "memset",
        "why": "IDA FLIRT『Watcom v9-*1.5 32bit DOS runtime』直接命名为 memset；"
               "调用点传入任意目标缓冲，不限于 0xa0000 显存。",
    }],
    0x3706E: [{"target": "semantic_name", "was": "mem_alloc", "now": "malloc",
               "why": "Watcom runtime FLIRT 符号"}],
    0x3776E: [{"target": "semantic_name", "was": "mem_free", "now": "free",
               "why": "Watcom runtime FLIRT 符号"}],
    0x37324: [{"target": "semantic_name", "was": "file_open", "now": "fopen",
               "why": "Watcom runtime FLIRT 符号"}],
    0x373CA: [{"target": "semantic_name", "was": "file_read", "now": "fread",
               "why": "Watcom runtime FLIRT 符号"}],
    0x37940: [{"target": "semantic_name", "was": "file_seek", "now": "fseek",
               "why": "Watcom runtime FLIRT 符号"}],
    0x3759C: [{"target": "semantic_name", "was": "file_close", "now": "fclose",
               "why": "Watcom runtime FLIRT 符号"}],
    0x16F55: [{
        "target": "evidence",
        "was": "selector 3 已确认结束回合，但确认框按钮资源与布局尚未登记",
        "now": "selector 3 使用通用 sub_19953 @0x19953：默认选择索引 0，"
               "Left 选 0、Right 选 1，Up/Down 无分支。object2 VA 0x51eed/0x51ef1 "
               "的 dword 为 16/17，对应 FDOTHER[2] 的 YES 与 NO!；普通帧 "
               "48/51、高亮帧 49/52 均为 24×16。四步展开后，320×200 屏幕"
               "按钮左上角分别为 (232,168)/(264,168)。仅选中项叠加 "
               "dword_53C13/2，计数每 2 BIOS tick 在 0..3 循环；非选中项"
               "始终使用普通帧。sub_197E5 @0x197e5 使用同值表 0x51ee5/0x51ee9 "
               "收回按钮。",
        "why": "复用已发布权威伪 C 的 @0x16f55 调用链与 sub_19953 "
               "（6916..7028 行）及 published fd2-functions.tsv 边界；"
               "只读按现有 LE parser 提取原版 object2 表并核对 FDOTHER[2] "
               "raw 像素。屏幕坐标由 buffer 偏移 0x1a599+3±16、view 源 "
               "32904/stride 456 与目标屏幕 (4,4) 换算。保留中性 helper 名，"
               "未新建 IDB、改写伪 C 或发布新的 IDA 逆向基线。",
    }],
    0x19DF7: [{
        "target": "evidence",
        "was": "SDL 活动快照只覆盖单位、cell state 和 meta，未登记开头 metadata owner",
        "now": "原版 Save 先把 dword_53A55 的 2211（0x8a3）字节 metadata "
               "写到 FD2.SAV 明文开头，再把独立的 2560 字节持久 roster 写到 "
               "+0x8a3。活动恢复 sub_10010 @0x10010 原样读回 metadata，并用 "
               "metadata[0]*2 选择 FDSHAP。stage 0 的 FDFIELD[1] 已用长度为 "
               "937（0x83+31*26）字节，首三字节为 00 04 1e。SDL 导出须从"
               "当前 session 序列化头部、turn/cell 表和完整模板字节，不能"
               "沿用旧 stage 的 prefix；未使用 metadata 尾由目标容器保留，"
               "完整 roster（含未用 raw 尾）由 session 恢复并整表写回。"
               "+1/+2 不赋予未经确认的玩法语义。",
        "why": "复用已发布权威伪 C 的 @0x19df7 writer（7143..7145 行）与 "
               "sub_10010（68..90 行）读回／shape 选择；FDFIELD.DAT[1] 原始"
               "长度和首部只读复核。原版 WGC 对照暴露旧 stage 29 prefix "
               "1d 14 46 与活动 stage 0 混用；未新建 IDB 或改写伪 C。",
    }, {
        "target": "evidence",
        "was": "活动 meta[9]（DS:0x3bfb）被当作当前 actor count 的副本",
        "now": "meta[1] 为当前战场 actor count（DS:0x3beb，格式容量 96）；"
               "meta[9] 为独立持久 roster count（DS:0x3bfb，0x8a3 起 "
               "0xa00 字节、32 条 0x50 记录）。二者不要求相等，也不以"
               "当前 side 2 actor 数量或 active count 为 roster 上限。",
        "why": "已发布伪 C：sub_112A5 @0x112a5 在 dword_53bf7 + "
               "80*dword_53bfb 追加记录并递增 roster count；sub_11506 "
               "@0x11506 对 active actor 遍历独立 roster 同步；@0x19df7 "
               "writer 分别写 meta[1]/meta[9]。只读 START.SAV 的两个值为 "
               "12/4，FD2.SAV 为 27/31，反证 count-copy 与 roster<=active "
               "假设。保留中性函数名，未新建 IDB 或修改权威伪 C。",
    }],
    0x1A30B: [{
        "target": "evidence",
        "was": "现有登记只描述玩家结束后的 phase 顺序与状态 tick",
        "now": "首部仅恢复 side 2、flags & 0x81 == 0、record +0x25/+0x26 "
               "均为零且 HP != maxHP 的 actor：HP 增加 maxHP/5 并封顶，"
               "随后 OR acted；整数恢复量为零也标记。满 HP 或被 gate 排除者"
               "不统一标记 acted。空焦点菜单 selector 3 确认后直接调用本函数。"
               "末尾重置 dword_53AE9=0，执行 phase 2 事件与状态后，"
               "field_focus_move_to_actor(0) 逐 X 再 Y 恢复焦点和镜头。",
        "why": "复用已发布权威伪 C fd2-ida-decomp-all.c 的本函数首部"
               "（7282..7314 行）与 field_empty_focus_menu_execute @0x16f55 "
               "selector 3 调用（5250..5267 行），末尾 @0x1a30b 的调用链，"
               "2026-11-04 真实 START 原版肯定 END→round002→Save 运行证据；"
               "未重新建立 IDB 或发布逆向基线。",
    }],
    0x25977: [{
        "target": "evidence",
        "was": "其他值从 FDMUS handle `DS:0x1a79` 加载 entry",
        "now": "object2 `DS:0x1a79`（VA `0x51a79`）是字符串 `\"FDMUS.DAT\"`，"
               "不是 handle；`music_track_play` 把该文件名传给 "
               "`res_load @0x111ba` 取曲目记录。",
        "why": "IDA 字符串识别 `aFdmusDat` + res_load 调用形态",
    }],
    0x1F894: [{
        "target": "semantic_name",
        "was": "boot_intro_title / boot_intro_title_entry / title_action_menu 三条独立登记",
        "now": "boot_intro_title_menu（同一函数）",
        "why": "`0x44aa8 - 0x25214 = 0x1f894`，`0x44ab2 - 0x25214 = 0x1f89e` 即其 "
               "__chkstk body；IDA 只有一个函数 `0x1f894..0x1ff79`。",
    }, {
        "target": "evidence",
        "was": "corrected code0 `0x15db1` 在进入 `title_action_dispatch` 前调用 "
               "`music_track_play(18,0)`",
        "now": "该描述属于 `main @0x25bf4` 体内 `0x25db1`，不是本函数。",
        "why": "IDA 由 Watcom FLIRT 确认 `main @0x25bf4..0x25ebb`，"
               "其主循环为 music_track_play(18,0) -> title_action_dispatch @0x25ebb。",
    }, {
        "target": "evidence",
        "was": "标题为 new-game/exit 两项，有活动快照时在中间插入恢复项",
        "now": "基础标题为 START/LOAD；完整存档与活动 stage 校验通过时"
               "在末尾追加 CONTINUE。默认索引 0，三项标题按 Up 回绕到"
               "索引 2 CONTINUE；LOAD 与 CONTINUE 是不同分派。",
        "why": "复用已发布 boot_intro_title_menu @0x1f894 伪 C 的项数门、"
               "上下键回绕与 FDOTHER[7] 菜单资源；2026-11-04 原版 WGC "
               "Up→CONTINUE→stage 0 运行证据。未新建 IDB 或改写伪 C。",
    }],
    0x25EBB: [{
        "target": "evidence",
        "was": "标题只有新游戏与退出，或把 LOAD 与活动快照恢复合并",
        "now": "标题分派 branch 0 进入新游戏，branch 1 调用手工四槽 "
               "picker；branch 2 调用 sub_10010 @0x10010 恢复活动快照。"
               "LOAD 不等于 CONTINUE。",
        "why": "复用已发布 title_action_dispatch @0x25ebb 的分支与 xref；"
               "结合原版三项标题 WGC 的 Up→CONTINUE→stage 0 检查点，"
               "不修改现有函数名或发布新的 IDA 逆向基线。",
    }],
    0x32975: [{
        "target": "semantic_name",
        "was": "field_actor_hide / field_ai_behavior7_hide_actor 两条独立登记",
        "now": "field_actor_hide（同一函数）",
        "why": "两条历史描述（写 actor[idx*0x50+5]=1）在 IDA 反编译上完全吻合。",
    }],
    0x111BA: [{
        "target": "address",
        "was": "0x463ce",
        "now": "VA 0x111ba / OFF 0x11ba",
        "why": "`FUN_0000e902 + 0x28b8`；机器码 fopen -> fseek(idx*4+6) -> fread，"
               "39 个调用点，`main` 用它加载 FDOTHER/FDTXT。",
    }],
}


def write_checksums(directory: Path) -> None:
    import hashlib

    sums = [f"{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}"
            for path in sorted(directory.iterdir())
            if path.name != "SHA256SUMS" and path.is_file()]
    (directory / "SHA256SUMS").write_text("\n".join(sums) + "\n",
                                        encoding="utf-8", newline="\n")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--audit", type=Path, default=OUT / "ida-name-audit.json")
    ap.add_argument("--functions", type=Path, default=OUT / "ida-functions.tsv")
    ap.add_argument("--annotate", type=Path, default=OUT / "ida-annotate-report.json")
    ap.add_argument("--json", type=Path,
                    default=ROOT / "docs" / "generated" / "ida-canonical"
                    / "fd2-ida-function-registry.json")
    ap.add_argument("--markdown", type=Path)
    ap.add_argument("--publish", type=Path, default=None,
                    help="把小体积证据产物复制到该目录（默认 docs/generated/ida-canonical）")
    ap.add_argument("--no-publish", action="store_true")
    ap.add_argument("--checksums-only", action="store_true",
                    help="仅刷新发布目录的 SHA256SUMS，不读取 IDA 中间导出或改写证据")
    args = ap.parse_args()
    if args.publish is None and not args.no_publish:
        args.publish = args.json.parent
    if args.checksums_only:
        if args.no_publish:
            ap.error("--checksums-only cannot be combined with --no-publish")
        write_checksums(args.publish)
        return 0

    audit = json.loads(args.audit.read_text(encoding="utf-8"))
    annotate = json.loads(args.annotate.read_text(encoding="utf-8"))

    funcs: dict[int, dict] = {}
    with args.functions.open(encoding="utf-8") as f:
        for row in csv.DictReader(f, delimiter="\t"):
            funcs[int(row["start_ea"], 16)] = row

    renamed = {int(r["va"], 16): r for r in annotate["naming"]["renamed"]}

    entries: dict[int, dict] = {}
    for row in audit["rows"]:
        va = row["canonical_va"]
        if va is None or row["resolution"] == "unresolved":
            continue
        e = entries.setdefault(va, {
            "va": va, "off": va - CODE0_BASE, "ida_name": "",
            "size": None, "legacy": [], "records": [],
        })
        e["records"].append({
            "semantic_name": row["semantic_name"],
            "purpose": row["purpose"],
            "evidence": row["evidence"],
            "legacy_address_cell": row["address_cell"],
            "legacy_ghidra_cell": row["ghidra_cell"],
            "resolution": row["resolution"],
            "legacy_body_va": row["legacy_body_va"],
            "source_line": row["line_no"],
        })
        e["legacy"].append(row["address_cell"])

    # 新确认的函数可能没有历史登记；仍须以已发布 IDA 入口校验并纳入登记。
    for va in CORRECTIONS:
        if va not in funcs:
            raise ValueError(f"correction VA is not an IDA function entry: {va:#x}")
        entries.setdefault(va, {
            "va": va, "off": va - CODE0_BASE, "ida_name": "",
            "size": None, "legacy": [], "records": [],
        })

    for va, e in entries.items():
        if va in CORRECTIONS:
            e["corrections"] = CORRECTIONS[va]
        fn = funcs.get(va)
        if fn:
            e["ida_name"] = fn["name"]
            e["size"] = int(fn["size"])
            e["end_va"] = int(fn["end_ea"], 16)
            e["is_lib"] = fn["is_lib"] == "1"
            e["prototype"] = fn["prototype"]
        names = sorted({r["semantic_name"] for r in e["records"]})
        e["historical_names"] = names
        e["aliases"] = [n for n in names if n != e["ida_name"]]
        e["merged_from_multiple_rows"] = len(e["records"]) > 1
        e["chkstk_body_rows"] = [r["source_line"] for r in e["records"]
                                 if r["resolution"] == "chkstk-body"]

    payload = {
        "schema": "fd2-ida-function-registry-v1",
        "oracle": "IDA Pro 9.4 idalib（loaders/lx.so + metapc + Watcom FLIRT + Hex-Rays）",
        "address_key": "LE relbase linear VA；OFF = VA - 0x10000（object1 内偏移）",
        "object1_range": audit["object1_range"],
        "legacy_rules": {
            "legacy_dual_minus": f"{audit['legacy_dual_shift']:#x}",
            "legacy_ghidra_plus": "0x28b8",
            "chkstk_body_offset": "0xa",
        },
        "source_rows": len(audit["rows"]),
        "canonical_functions": len(entries),
        "confirmed_anchors": annotate.get("confirmed_anchors", []),
        "entries": [entries[k] for k in sorted(entries)],
    }
    args.json.parent.mkdir(parents=True, exist_ok=True)
    args.json.write_text(json.dumps(payload, indent=1, ensure_ascii=False),
                         encoding="utf-8", newline="\n")
    print(f"历史登记 {len(audit['rows'])} 行 -> {len(entries)} 个规范函数")
    print(f"其中由多行合并而来: {sum(1 for e in entries.values() if e['merged_from_multiple_rows'])}")
    print(f"含 __chkstk body 行: {sum(1 for e in entries.values() if e['chkstk_body_rows'])}")
    print(f"wrote {args.json}")

    if args.publish:
        import shutil
        args.publish.mkdir(parents=True, exist_ok=True)
        published = []
        for name in ("ida-program.json", "ida-verify.json", "ida-name-audit.json",
                     "ida-annotate-report.json", "ida-prototype-probe.json",
                     "ida-functions.tsv", "ida-names.tsv", "ida-strings.tsv"):
            src = OUT / name
            if not src.exists():
                continue
            dst = args.publish / ("fd2-" + name.removeprefix("ida-")
                                  if name.startswith("ida-") else name)
            shutil.copy(src, dst)
            published.append(dst.name)
        write_checksums(args.publish)
        print(f"published {len(published)} 个证据产物到 {args.publish}")

    if args.markdown:
        conflicts = [e for e in entries.values() if e["merged_from_multiple_rows"]]
        corr_rows = [
            "| VA | 规范函数名 | 项 | 历史说法 | 复核后 | 依据 |",
            "|----|-----------|----|---------|--------|------|",
        ]
        for va in sorted(entries):
            for c in entries[va].get("corrections", []):
                corr_rows.append(
                    f"| `{va:#x}` | `{entries[va]['ida_name']}` | {c['target']} | "
                    f"{c['was']} | {c['now']} | {c['why']} |")

        body = [
            "| VA | OFF | 规范函数名 | 大小 | 历史语义名 | 用途 | 确认依据 | 历史地址列 |",
            "|----|-----|-----------|------|-----------|------|---------|-----------|",
        ]
        for va in sorted(entries):
            e = entries[va]
            for r in e["records"]:
                legacy = f"`{r['legacy_address_cell']}`"
                if r["resolution"] == "chkstk-body":
                    legacy += f"（`{r['legacy_body_va']:#x}` 为 `__chkstk` body）"
                if r["legacy_ghidra_cell"].startswith("FUN_"):
                    legacy += f" / `{r['legacy_ghidra_cell']}`"
                body.append(
                    f"| `{va:#x}` | `{e['off']:#x}` | `{e['ida_name']}` | "
                    f"{e['size']} | `{r['semantic_name']}` | {r['purpose']} | "
                    f"{r['evidence']} | {legacy} |")

        header = f"""# 函数命名登记表（canonical VA 主键）

> 本表由 `python3 tools/ida/build_fd2_name_registry.py --markdown \\
> docs/reverse-engineering/function-names.md` 生成，请勿手工改写地址列。
> 新增或修订语义命名时，先改 `docs/generated/ida-canonical/fd2-ida-function-registry.json`
> 的上游来源（历史档案在 Git 历史中，判定账本为 `tools/ida-out/ida-name-audit.json`），
> 再重新生成本文件。

## 地址主键

唯一主键是 **LE relbase linear VA**；`OFF` 是 object1 内偏移，等于 `VA - 0x10000`。
object1 有效范围为 `VA 0x10000..0x4ef29`。函数入口、边界与大小由 IDA Pro 9.4
（`loaders/lx.so` + `metapc` + Watcom FLIRT）独立分析给出，复核过程见
[`ida-crosscheck.md`](ida-crosscheck.md)。

历史档案的地址列混用两套已废弃坐标系，换算规则如下（均已在本表中完成归约）：

| 旧坐标 | 换算到 canonical VA | 说明 |
|--------|--------------------|------|
| 旧 dual 镜像地址 | `VA = legacy - 0x25214` | 旧流程把 `LE+0x80 = 0x10e00` 当成文件偏移，真实 page 1 在 `0x25214 + 0x10e00 = 0x36014` |
| 旧 Ghidra 窗口地址（`FUN_xxxxxxxx`） | `VA = FUN_addr + 0x28b8` | `0x28b8` 是 embedded MZ 的 `e_lfanew` |
| 已修正的 `code0` | `VA = code0 + 0x10000` | object1 内偏移加 relbase |

Watcom 栈检查序言 `push imm32; call watcom_stack_check` 固定 10 字节，因此历史档案
里记在函数入口 `+0xa` 的「body」地址已全部归一化到入口。IDA 把 wrapper 与 body
视为同一个函数，`*_entry` / `*_core` / `*_runtime` 后缀属旧工具产物，不是原程序结构。

## 统计

| 项 | 值 |
|----|----|
| 历史登记行数 | {len(audit['rows'])} |
| 归约后的规范函数数 | {len(entries)} |
| 由多行历史记录合并而来 | {len(conflicts)} |
| 含 `__chkstk` body 登记行 | {sum(1 for e in entries.values() if e['chkstk_body_rows'])} |
| 机器可读版本 | `docs/generated/ida-canonical/fd2-ida-function-registry.json` |

同一 VA 出现多个历史语义名时，本表保留全部历史描述，规范函数名列给出裁决结果。

## 已推翻或澄清的历史结论

原始说法保留在下方登记表的「用途 / 确认依据」列，本节只记录复核结果。
完整依据见 [`ida-crosscheck.md`](ida-crosscheck.md) 第八节。

{chr(10).join(corr_rows)}

## 登记表

"""
        args.markdown.parent.mkdir(parents=True, exist_ok=True)
        args.markdown.write_text(header + "\n".join(body) + "\n",
                                 encoding="utf-8", newline="\n")
        print(f"wrote {args.markdown}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
