#!/usr/bin/env python3
"""核对正常技能来源与原 FIGANI caster 选槽，不开放关卡或伪造 empty part。

来源：112a5/10c50 初始位图、1e292 学习、2ae0e/2aa00 转职保留；
1cff0/15311 分派、15b77 AI 评分及 30e9d/2cf30/2d80d 选槽。
复用 G1 common-event-semantics §6.1 已完成的位图写入 owner 闭包。
只读严格身份原资源；不读取 SDL 冷表作为期望，不推断任意外部 SAV。
"""
from __future__ import annotations

import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import struct

from audit_stage_data import archive_entries, parse_metadata
from verify_unit_tables import source_range

ROOT = Path(__file__).resolve().parents[2]
SOURCE_HASHES = {
    "FD2.EXE": "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd",
    "FDFIELD.DAT": "81310211c6e96ebd89e79648650696691c740229fa4095ece37e7226bc27b74d",
    "FIGANI.DAT": "c0cef326c5f8bc61252f03394140dbeed2280ddba0da99bfacdee1bf568846f8",
}
EMPTY = b"\0\0\x0a"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def bits(data):
    return [i for i in range(8 * len(data)) if data[i // 8] >> (i % 8) & 1]


def full_kind(magic_id, *, player):
    """AI 15311 的 FULL9 与玩家 MAP9 分开；未确认记录不能静默当 MAP。"""
    if not 0 <= magic_id < 36 or magic_id == 31:
        raise ValueError("magic source has no normal original record")
    if magic_id < (9 if player else 10):
        return "ordinary"
    if player and (magic_id == 24 or 28 <= magic_id <= 30):
        return "derived"
    if player and magic_id >= 32:
        return "composite"
    return None


def clip_info(entries, index):
    if not 0 <= index < len(entries):
        raise ValueError("FIGANI selected entry outside original directory")
    offset, data = entries[index]
    result = {"entry": index, "offset": offset, "length": len(data),
              "sha256": sha(data), "empty": data == EMPTY}
    if result["empty"]:
        return result
    if len(data) < 8:
        raise ValueError("FIGANI non-empty directory invalid")
    # 2ebe1分别读取BYTE数量*a3与split a3[1]，不是WORD count。
    count, split, intro = data[0], data[1], data[2]
    if not count or 8 + 4 * count > len(data):
        raise ValueError("FIGANI non-empty directory invalid")
    if split not in (0, 1):
        raise ValueError("FIGANI split mode outside confirmed source domain")
    starts = struct.unpack_from(f"<{count}I", data, 8)
    if starts[0] != 8 + 4 * count or intro > count:
        raise ValueError("FIGANI casting intro/directory invalid")
    markers = [0, 0]
    for i, start in enumerate(starts):
        end = starts[i + 1] if i + 1 < count else len(data)
        if not starts[0] <= start < end <= len(data) or end - start < 13:
            raise ValueError("FIGANI casting part boundary invalid")
        if data[start + 4] == 1:
            markers[i >= intro] += 1
    return dict(result, part_count=count, split_mode=split, intro_end=intro,
                cost_markers=markers[0], effect_markers=markers[1])


def casting_plan(entries, unit_id, magic_id, kind):
    # kind is supplied by the controller dispatcher; it must agree with the wrapper.
    expected = "ordinary" if magic_id < 10 else (
        "derived" if magic_id == 24 or 28 <= magic_id <= 30 else
        "composite" if 32 <= magic_id <= 35 else None)
    if kind != expected or kind is None or not 0 <= unit_id < 136:
        raise ValueError("caster dispatch/identity does not select that FULL wrapper")
    selected = unit_id * 3 + (1 if kind == "composite" else 2)
    attempts = [clip_info(entries, selected)]
    if kind == "ordinary" and attempts[0]["empty"]:
        attempts.append(clip_info(entries, selected - 1))
    return {"unit_id": unit_id, "magic_id": magic_id, "kind": kind,
            "attempted_entries": [p["entry"] for p in attempts],
            "clip": attempts[-1]}


def promotion_targets(old_id, special_items):
    if not 0 <= old_id < 18 or old_id == 7:
        return []
    result = [(old_id + 32, "ordinary", None)]
    if special_items[old_id] != 255:
        result.append((old_id + 50, "special", special_items[old_id]))
    if old_id == 9:
        result.append((52, "special-priority", 90))
    return result


def check_pairs(entries, pairs):
    """保留每个失败源；不能把有技能的 empty caster 当成无需支持。"""
    plans = [dict(casting_plan(entries, p["unit_id"], p["magic_id"], p["kind"]),
                  sources=p["sources"]) for p in pairs]
    return plans, [p for p in plans if p["clip"]["empty"]]


def verify(executable, field_data, figani_data):
    for name, data in zip(SOURCE_HASHES, (executable, field_data, figani_data)):
        if sha(data) != SOURCE_HASHES[name]:
            raise ValueError(f"{name} source identity mismatch")
    field, figani = archive_entries(field_data), archive_entries(figani_data)
    if len(field) != 99 or len(figani) != 408:
        raise ValueError("original complete archive count mismatch")
    table_specs = (("base", 0x61da1, 32 * 24), ("growth", 0x620a1, 68 * 11),
                   ("learning", 0x626b3, 20 * 12), ("promotion_items", 0x523d5, 18))
    tables = {name: source_range(executable, va, size) for name, va, size in table_specs}
    base, growth, learning, special = (tables[name] for name, _, _ in table_specs)

    def learned(identity):
        selector = growth[identity * 11 + 10]
        if selector == 255:
            return []
        if selector >= 20:
            raise ValueError("unknown original learning selector")
        row = learning[selector * 12:selector * 12 + 12]
        return [row[i + 1] for i in range(0, 12, 2) if row[i] != 255]

    sources = defaultdict(lambda: defaultdict(list))
    for identity in range(32):
        sources[identity]  # 无技能身份也须参与合法转职。
        row = base[identity * 24:identity * 24 + 24]
        for magic in bits(row[8:12]):
            sources[identity][magic].append({"kind": "base", "va": hex(0x61da1 + identity * 24),
                                              "row_sha256": sha(row)})
    templates, tails = [], 0
    for stage in range(33):
        offset, raw = field[stage * 3 + 1]
        metadata = parse_metadata(raw)
        count = metadata["unknown_02"]
        if count > len(metadata["templates"]):
            raise ValueError("effective template count exceeds original prefix")
        tails += len(metadata["templates"]) - count
        for i, row in enumerate(metadata["templates"][:count]):
            identity, magic_ids = row[1], bits(row[13:17])
            if identity >= 136 or row[0] > 2:
                raise ValueError("new template identity/controller requires review")
            source = {"kind": "template", "stage": stage, "template_index": i,
                      "entry": stage * 3 + 1, "offset": offset + 0x83 + 26 * i,
                      "row_sha256": sha(row), "side": row[0], "level": row[4]}
            templates.append(dict(source, unit_id=identity, magic_ids=magic_ids))
            # 全阵营的 <68 模板都纳入玩家候选，宽于实际招募来源。
            if identity < 68:
                sources[identity]
                for magic in magic_ids:
                    sources[identity][magic].append(source)
    for identity in list(sources):
        for magic in learned(identity):
            sources[identity][magic].append({"kind": "learning", "identity": identity})
    promotions = []
    for old in range(18):
        for new, mode, item in promotion_targets(old, special):
            promotions.append({"from": old, "to": new, "mode": mode, "item": item,
                               "minimum_level": 20})
            sources[new]
            for magic in list(sources[old]):
                sources[new][magic].append({"kind": "promotion-preserved", "from": old,
                                           "mode": mode, "item": item})
            for magic in learned(new):
                sources[new][magic].append({"kind": "learning", "identity": new})

    player_pairs, template_pairs, excluded = [], [], []
    for identity, skills in sorted(sources.items()):
        for magic, origins in sorted(skills.items()):
            kind = full_kind(magic, player=True)
            if kind:
                player_pairs.append({"unit_id": identity, "magic_id": magic,
                                     "kind": kind, "sources": origins})
    for row in templates:
        for magic in row["magic_ids"]:
            kind = full_kind(magic, player=row["side"] == 2)
            if kind:
                template_pairs.append({"unit_id": row["unit_id"], "magic_id": magic,
                                       "kind": kind, "sources": [row]})
            elif row["side"] != 2 and (magic == 24 or magic >= 28):
                # 原15b77 default score0；15311要求score>=6且>=10不走FULL。
                excluded.append(dict(row, magic_id=magic, reason="AI default score0, no FULL dispatch"))
    players, player_errors = check_pairs(figani, player_pairs)
    initial, template_errors = check_pairs(figani, template_pairs)
    if len(templates) != 1887 or tails != 12:
        raise ValueError("effective template source extent changed")
    empty = [i for i in range(136) if figani[i * 3 + 1][1] == EMPTY and figani[i * 3 + 2][1] == EMPTY]
    return {"ok": not player_errors and not template_errors,
            "scope": "normal original base/template/learning/legal-promotion skills versus FULL caster slots",
            "source_sha256": SOURCE_HASHES,
            "source_tables": [{"name": name, "va": hex(va), "length": size,
                               "sha256": sha(tables[name])} for name, va, size in table_specs],
            "counts": {"field_groups": 33, "effective_templates": len(templates), "excluded_raw_tails": tails,
                       "figani_entries": len(figani), "base_identities": 32, "growth_identities": 68,
                       "learning_rows": 20, "legal_promotion_paths": len(promotions),
                       "player_source_identities": len(sources),
                       "player_potential_skill_pairs": sum(len(s) for s in sources.values()),
                       "player_full_pairs": len(players), "template_full_instances": len(initial),
                       "template_full_unique_pairs": len({(p['unit_id'], p['magic_id']) for p in initial}),
                       "AI_nonfull_special_sources": len(excluded),
                       "selected_empty": len(player_errors) + len(template_errors)},
            "ordinary_final_empty_all136": empty,
            "empty_identity_templates": [t for t in templates if t["unit_id"] in empty],
            "player_identity_skill_union": {str(i): sorted(s) for i, s in sorted(sources.items())},
            "legal_promotion_paths": promotions, "player_full_pairs": players,
            "template_full_pairs": initial, "AI_nonfull_special_sources": excluded,
            "violations": player_errors + template_errors,
            "prior_writer_closure": {"document": "docs/reverse-engineering/common-event-semantics.md#61-正常来源的法术位图写入域",
                                     "callee_count": 376,
                                     "readset_sha256": "67ecf2532928a0359c63e5770eeace5d5feecf56098d2fe4d72b2dc1b5fc2c75"},
            "boundary": ["全学习项、全阵营<68模板及合法转职保留取保守并集，宽于具体路线。",
                         "玩家9是MAP；AI9可FULL。MAP不读取caster FIGANI。",
                         "stage13 side0/unit103/30为原AI零分来源，不把固定311 empty当成功FULL。",
                         "仅核empty、目录/part边界、intro及marker；不代替图像解码、声音、完整呈现或原版实机验收。",
                         "复用G1事件/复制owner闭包，不含任意外部SAV、注入位图或未来新增来源。"],
            "new_gameplay_enabled": False, "original_runtime_executed": False}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, default=ROOT / "original_game")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    report = verify(*((args.original / name).read_bytes() for name in SOURCE_HASHES))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"ok": report["ok"], "counts": report["counts"]}, ensure_ascii=False))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
