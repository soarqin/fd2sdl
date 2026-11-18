#!/usr/bin/env python3
"""从固定身份的 FD2.EXE 导出已登记冷表，保留其余已审查的定义。"""
from __future__ import annotations

import argparse
import hashlib
import json
from math import prod
from pathlib import Path
import struct
import tempfile

from table_schema import ROOT, TABLE_SCHEMA_VERSION, decode_json, project_tables, table_definition
from verify_unit_tables import EXE_SHA256, TABLES as UNIT_TABLES, source_range
from verify_stage_tables import TABLES as STAGE_TABLES
from verify_preparation_tables import SPECS as PREPARATION_TABLES
from verify_runtime_tables import SPECS as BATTLE_TABLES, source_short_fx_offsets
from verify_magic_contracts import MAP_FIELDS, source_map_profiles
from verify_movement_scripts import script_definition, source_scripts
from dump_fd2_fixup_table import collect_internal_offset_fixups

# 可直接映射的原字节使用独立校验器登记的读集。其余地址来自经审查的来源记录。
SOURCE_TABLES = {
    **UNIT_TABLES,
    **{stem: tuple(spec[:4] for spec in specs) for stem, specs in STAGE_TABLES.items()},
    "preparation": PREPARATION_TABLES,
    "battle_animation": BATTLE_TABLES + (("shake_x", "uint8_t", (6,), 0x52656),),
    "common_event": (
        ("k_common_event_nested_descriptors", "uint8_t", (4, 3), 0x52742),
        ("k_common_event_cell_items", "uint8_t", (5,), 0x5274E),
    ),
    "field_background": (
        ("k_field_background_wave", "uint8_t", (16,), 0x627C8),
        ("k_field_background_scroll_initial", "uint8_t", (1,), 0x51A10),
    ),
    "runtime": (
        ("k_field_range_sequence", "uint8_t", (20,), 0x51A97),
        ("k_field_auxiliary_side_colors", "uint32_t", (3,), 0x5208A),
    ),
}
FORMATS = {"uint8_t": "B", "uint16_t": "H", "uint32_t": "I", "int32_t": "i"}


def source_values(executable, c_type, dimensions, va):
    if c_type == "fd2_field_stage_music":
        count, = dimensions
        data = source_range(executable, va, 2 * count)
        return [list(row) for row in zip(data[:count], data[count:])]
    fmt = f"<{prod(dimensions)}{FORMATS[c_type]}"
    numbers = list(struct.unpack(fmt, source_range(executable, va, struct.calcsize(fmt))))
    if len(dimensions) == 1:
        return numbers
    rows, columns = dimensions
    return [numbers[i * columns:(i + 1) * columns] for i in range(rows)]


def source_projections(executable, data):
    tables = {stem: {symbol: source_values(data, c_type, extent, va)
                     for symbol, c_type, extent, va in specs}
              for stem, specs in SOURCE_TABLES.items()}
    profiles = source_map_profiles(executable)
    for symbol, field in zip(("MAGIC_MAP_EFFECT_IDS", "MAGIC_MAP_COST_IDS", "MAGIC_MAP_TARGET_LIMITS",
                              "MAGIC_MAP_COST_BEFORE_CAST"), MAP_FIELDS):
        tables["magic"][symbol] = [profile[field] for profile in profiles]
    battle = tables["battle_animation"]
    colors = source_range(data, 0x51AAD, 36 * 3)
    battle["k_battle_cast_colors"] = [list(row) for row in zip(colors[:36], colors[36:72], colors[72:])]
    battle["k_battle_short_fx_offsets"] = source_short_fx_offsets(data, collect_internal_offset_fixups(executable))
    quake = struct.unpack("<9i", source_range(data, 0x52096, 36))
    tables["field_game_effects"] = {
        "k_field_quake_parameters": [list(row) for row in zip(quake[:3], quake[3:6], quake[6:])]
    }
    return tables


def extract(executable: Path, output: Path, tables_dir: Path = ROOT / "data/static/tables"):
    executable = executable.resolve()
    output = output.resolve()
    if any(output.is_relative_to(path.resolve()) for path in
           (executable.parent, ROOT / "original_game", ROOT / "data", tables_dir)):
        raise ValueError("输出目录必须位于原资源及源定义目录之外")
    if output.exists():
        raise FileExistsError(f"输出目录已存在：{output}")
    data = executable.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXE_SHA256:
        raise ValueError("FD2.EXE 身份不符")
    manifest = ROOT / "data/static/manifest.json"
    inputs = {manifest: manifest.read_bytes()}
    table_contracts = decode_json(inputs[manifest])["table_contracts"]
    inputs.update({tables_dir / name: (tables_dir / name).read_bytes() for name in table_contracts})
    sources = source_projections(executable, data)
    documents, retained = {}, {}
    for filename, contracts in table_contracts.items():
        stem = Path(filename).stem
        original = sources.get(stem, {})
        current = project_tables(stem, decode_json(inputs[tables_dir / filename]), contracts)
        # 只有来源可复跑的字节／投影从 EXE 替换。人工解码的规则沿用传入版本。
        values = {row["name"]: row["values"] for row in current}
        retained[stem] = sorted(values.keys() - original.keys())
        values.update(original)
        documents[filename] = table_definition(stem, values, contracts)
    scripts = source_scripts(executable)
    documents["movement_scripts.json"] = {"schema_version": TABLE_SCHEMA_VERSION, "scripts": {
        str(identity): script_definition(script.data) for identity, script in scripts.items()}}
    if executable.read_bytes() != data:
        raise ValueError("FD2.EXE 在提取过程中变化")
    if any(path.read_bytes() != raw for path, raw in inputs.items()):
        raise ValueError("源定义在提取过程中变化")

    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent) as work:
        staged = Path(work) / "tables"
        staged.mkdir()
        for filename, document in documents.items():
            (staged / filename).write_bytes((json.dumps(document, ensure_ascii=False, indent=2) + "\n").encode("utf-8"))
        if output.exists():
            raise FileExistsError(f"输出目录已存在：{output}")
        staged.rename(output)
    return {"executable_sha256": EXE_SHA256, "table_files": len(documents), "movement_scripts": len(scripts),
            "from_exe": {stem: sorted(values) for stem, values in sources.items()},
            "retained": {stem: names for stem, names in retained.items() if names}}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--tables-dir", type=Path, default=ROOT / "data/static/tables")
    args = parser.parse_args()
    try:
        result = extract(args.executable, args.output, args.tables_dir)
    except (OSError, ValueError, TypeError, KeyError, struct.error) as error:
        parser.exit(1, f"冷定义导出失败：{error}\n")
    print(json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
