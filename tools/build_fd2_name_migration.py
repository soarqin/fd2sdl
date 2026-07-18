#!/usr/bin/env python3
"""把历史 function-names.md 转为完整、有账的名称迁移 ledger。

历史地址不统一，脚本不会猜测变换。只有当前机器码已重新确认的 anchor 进入
confirmed registry；其余条目保留 legacy address 和说明，状态为 unresolved。
"""
from __future__ import annotations

import argparse
import json
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DEFAULT_INPUT = ROOT / "docs" / "reverse-engineering" / "function-names.md"
DEFAULT_LEDGER = ROOT / "docs" / "generated" / "fd2-name-migration.json"
DEFAULT_CONFIRMED = ROOT / "docs" / "generated" / "fd2-confirmed-function-names.json"
ADDRESS_RE = re.compile(r"(?:code0\s+)?0x[0-9a-fA-F]+")

CONFIRMED_ANCHORS = {
    "fd2_le_entry": (0x3CCB4, "le-transfer-entry", None, "LE header entry"),
    "title_action_menu": (0x1F894, "confirmed-entry", "chkstk-wrapper",
                          "current bytes and direct xrefs"),
    "animation_play": (0x20421, "confirmed-entry", "chkstk-wrapper",
                       "current bytes and direct xrefs"),
    "music_track_play": (0x25977, "confirmed-entry", "chkstk-wrapper",
                         "current bytes and direct xrefs"),
    "sfx_play": (0x25A96, "confirmed-entry", "chkstk-wrapper",
                 "current bytes and direct xrefs"),
    "new_game_opening_play": (0x3231B, "confirmed-entry", "chkstk-wrapper",
                              "relocation-backed dispatch and current bytes"),
    "dialog_text_scroll_up": (0x16E24, "confirmed-entry", "chkstk-wrapper",
                              "current bytes and text-dialog direct xrefs"),
    "__chkstk": (0x3702F, "watcom-runtime-helper", None,
                 "541 direct push-imm32/call sites"),
}


def parse_rows(path: Path) -> list[dict[str, object]]:
    rows = []
    for line_number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line.startswith("|"):
            continue
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) < 5 or cells[0] in ("地址", "------"):
            continue
        # Markdown 表格正文偶尔在行内代码中含 `|`；首三列固定，最后一列为
        # 证据，所有额外 cell 重新拼回证据文本。
        if len(cells) > 5:
            cells = cells[:4] + [" | ".join(cells[4:])]
        if ADDRESS_RE.search(cells[0]) is None:
            continue
        rows.append({
            "legacy_row": line_number,
            "legacy_address_text": cells[0],
            "legacy_ghidra_name": cells[1],
            "semantic_name": cells[2],
            "purpose": cells[3],
            "legacy_evidence": cells[4],
        })
    return rows


def build(path: Path) -> tuple[dict[str, object], dict[str, object]]:
    rows = parse_rows(path)
    if len(rows) != 210:
        raise ValueError(f"expected 210 historical name rows, got {len(rows)}")
    seen_confirmed: set[str] = set()
    migration_rows = []
    for row in rows:
        item = dict(row)
        anchor = CONFIRMED_ANCHORS.get(str(row["semantic_name"]))
        if anchor is None:
            item.update({
                "disposition": "legacy-unresolved",
                "candidate_va": None,
                "transformation": "none-address-space-not-proven",
            })
        else:
            va, kind, entry_kind, evidence = anchor
            if row["semantic_name"] in seen_confirmed:
                item.update({
                    "disposition": "duplicate-alias-needs-review",
                    "candidate_va": va,
                    "transformation": "semantic-anchor-not-address-arithmetic",
                })
            else:
                seen_confirmed.add(str(row["semantic_name"]))
                item.update({
                    "disposition": "confirmed-entry",
                    "candidate_va": va,
                    "candidate_object": 1,
                    "candidate_offset": va - 0x10000,
                    "entry_kind": entry_kind or kind,
                    "current_evidence": evidence,
                    "transformation": "semantic-anchor-not-address-arithmetic",
                })
        migration_rows.append(item)

    confirmed_rows = []
    for name, (va, kind, entry_kind, evidence) in sorted(
            CONFIRMED_ANCHORS.items(), key=lambda item: item[1][0]):
        confirmed_rows.append({
            "name": name, "va": va, "object": 1, "offset": va - 0x10000,
            "kind": kind, "entry_kind": entry_kind, "evidence": evidence,
        })
    ledger = {
        "schema": "fd2-name-migration-v1",
        "source": "docs/reverse-engineering/function-names.md",
        "row_count": len(migration_rows),
        "confirmed_row_count": sum(
            1 for row in migration_rows if row["disposition"] == "confirmed-entry"),
        "unresolved_row_count": sum(
            1 for row in migration_rows if row["disposition"] == "legacy-unresolved"),
        "duplicate_alias_count": sum(
            1 for row in migration_rows
            if row["disposition"] == "duplicate-alias-needs-review"),
        "rows": migration_rows,
        "policy": "Legacy addresses are never transformed mechanically.",
    }
    confirmed = {
        "schema": "fd2-confirmed-function-names-v1",
        "address_space": "LE relbase linear VA",
        "count": len(confirmed_rows),
        "functions": confirmed_rows,
    }
    return ledger, confirmed


def write_json(path: Path, data: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, ensure_ascii=False, sort_keys=True,
                               separators=(",", ":")) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", type=Path, default=DEFAULT_INPUT)
    parser.add_argument("--ledger", type=Path, default=DEFAULT_LEDGER)
    parser.add_argument("--confirmed", type=Path, default=DEFAULT_CONFIRMED)
    args = parser.parse_args()
    ledger, confirmed = build(args.input.resolve())
    write_json(args.ledger, ledger)
    write_json(args.confirmed, confirmed)
    print(f"historical_rows={ledger['row_count']}")
    print(f"confirmed_rows={ledger['confirmed_row_count']}")
    print(f"unresolved_rows={ledger['unresolved_row_count']}")
    print(f"confirmed_functions={confirmed['count']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
