"""已确认冷定义的严格校验与 C11 生成；只使用 Python 标准库。

来源见各 JSON 的 source；本工具不读取 FD2.EXE，不推断未知字段。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
SCHEMA_VERSION = 1
INTEGER_TYPES = {
    "uint8_t": (0, 255), "int8_t": (-128, 127),
    "uint16_t": (0, 65535), "int16_t": (-32768, 32767),
    "uint32_t": (0, 4294967295), "int32_t": (-2147483648, 2147483647),
    "int": (-2147483648, 2147483647),
}
STRUCT_TYPES = {"fd2_field_stage_music": ["uint8_t", "uint8_t"]}


def reject_duplicates(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def load_json(path: Path):
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=reject_duplicates,
                      parse_constant=lambda value: (_ for _ in ()).throw(ValueError(value)))


def integer(value, c_type):
    lo, hi = INTEGER_TYPES[c_type]
    if type(value) is not int or not lo <= value <= hi:
        raise ValueError(f"{c_type} value outside [{lo}, {hi}]: {value!r}")


def validate_values(values, dimensions, c_type):
    if dimensions:
        if not isinstance(values, list) or len(values) != dimensions[0]:
            raise ValueError(f"array extent mismatch: expected {dimensions}")
        for value in values:
            validate_values(value, dimensions[1:], c_type)
    elif c_type in STRUCT_TYPES:
        fields = STRUCT_TYPES[c_type]
        if not isinstance(values, list) or len(values) != len(fields):
            raise ValueError(f"{c_type}: wrong field count")
        for value, field in zip(values, fields):
            integer(value, field)
    else:
        integer(values, c_type)


def validate_document(document):
    if not isinstance(document, dict) or set(document) != {"schema_version", "tables"}:
        raise ValueError("table document requires schema_version and tables only")
    if type(document["schema_version"]) is not int or document["schema_version"] != SCHEMA_VERSION:
        raise ValueError("unsupported static schema_version")
    if not isinstance(document["tables"], list) or not document["tables"]:
        raise ValueError("nonempty tables required")
    names = set()
    for table in document["tables"]:
        if not isinstance(table, dict) or set(table) != {"name", "c_type", "dimensions", "values", "source"}:
            raise ValueError("unknown/missing table field")
        name, c_type, dimensions = table["name"], table["c_type"], table["dimensions"]
        if not isinstance(name, str) or not re.fullmatch(r"[a-zA-Z][a-zA-Z0-9_]*", name) or name in names:
            raise ValueError(f"invalid/duplicate C table identifier: {name}")
        names.add(name)
        if c_type not in INTEGER_TYPES and c_type not in STRUCT_TYPES:
            raise ValueError(f"unsupported C type: {c_type}")
        if not isinstance(dimensions, list) or not 1 <= len(dimensions) <= 3 or any(
                type(n) is not int or not 1 <= n <= 65536 for n in dimensions):
            raise ValueError(f"invalid dimensions: {dimensions}")
        source = table["source"]
        if not isinstance(source, dict) or not isinstance(source.get("evidence"), str) or not source["evidence"].strip():
            raise ValueError(f"{name}: source evidence required")
        validate_values(table["values"], dimensions, c_type)
    return document


def initializer(value):
    if isinstance(value, list):
        return "{" + ", ".join(initializer(item) for item in value) + "}"
    # uint32 最大值也应按无符号常量生成，避免 MSVC 的 decimal signed warning。
    return str(value) + ("u" if value > 2147483647 else "")


def validate_references(tables):
    """只校验已确认的跨表引用，不把未解码 byte 猜成引用。"""
    required = {"k_profiles", "k_player_base_records", "k_enemy_records", "ITEM_RECORDS", "k_growth_records", "k_learning_records", "MAGIC_RECORDS"}
    if not required <= tables.keys():
        raise ValueError(f"missing referenced tables: {sorted(required-tables.keys())}")
    def reference(value, count, label, sentinel=None, first=0):
        if value != sentinel and not first <= value < first + count:
            raise ValueError(f"invalid {label} reference: {value}")
    profiles = len(tables["k_profiles"])
    for row in tables["k_player_base_records"] + tables["k_enemy_records"]:
        reference(row[1], profiles, "movement profile", first=1)
    for row in tables["k_player_base_records"]:
        for item in row[12:18]:
            reference(item, len(tables["ITEM_RECORDS"]), "initial item", sentinel=255)
    for row in tables["k_growth_records"]:
        reference(row[10], len(tables["k_learning_records"]), "learning selector", sentinel=255)
    for row in tables["k_learning_records"]:
        for magic in row[1::2]:
            reference(magic, len(tables["MAGIC_RECORDS"]), "learned magic", sentinel=255)


def render(document, input_digest):
    lines = ["/* 由 tools/static_data/generate.py 生成，禁止手改。",
             f" * schema={SCHEMA_VERSION}; source JSON SHA256={input_digest} */"]
    for table in document["tables"]:
        dimensions = "".join(f"[{n}]" for n in table["dimensions"])
        lines.append(f"static const {table['c_type']} {table['name']}{dimensions} = {{")
        lines.extend("    " + initializer(row) + "," for row in table["values"])
        lines.extend(["};", ""])
    return "\n".join(lines).encode("utf-8")


def generate(root: Path, output: Path, check=False):
    if output.resolve().is_relative_to((root / "original_game").resolve()) or output.resolve().is_relative_to((root / "data").resolve()):
        raise ValueError("output must not overwrite source definitions or original_game")
    manifest_path = root / "data/static/manifest.json"
    manifest = load_json(manifest_path)
    if not isinstance(manifest, dict) or set(manifest) != {"schema_version", "table_files", "sources", "table_contracts"} or type(manifest["schema_version"]) is not int or manifest["schema_version"] != SCHEMA_VERSION:
        raise ValueError("invalid static manifest")
    sources = manifest["sources"]
    if not isinstance(sources, dict) or not sources or any(
            not isinstance(value, dict) or not isinstance(value.get("sha256"), str) or
            not re.fullmatch(r"[0-9a-f]{64}", value["sha256"]) for value in sources.values()):
        raise ValueError("manifest source identities required")
    files = manifest["table_files"]
    if not isinstance(files, list) or len(set(files)) != len(files) or not files:
        raise ValueError("table_files must be a nonempty unique list")
    actual = {p.name for p in (root / "data/static/tables").glob("*.json")}
    if actual != set(files):
        raise ValueError(f"table manifest coverage mismatch: missing={set(files)-actual}, extra={actual-set(files)}")
    if not isinstance(manifest["table_contracts"], dict) or set(manifest["table_contracts"]) != set(files):
        raise ValueError("table contracts must cover every input exactly")
    expected_outputs = {Path(name).stem + ".inc" for name in files}
    extras = {path.name for path in output.glob("*.inc")} - expected_outputs
    if extras:
        raise ValueError(f"obsolete generated headers must be removed explicitly: {sorted(extras)}")
    outputs = {}
    receipt = {"schema_version": SCHEMA_VERSION, "manifest_sha256": hashlib.sha256(manifest_path.read_bytes()).hexdigest(), "tables": []}
    names = set()
    tables = {}
    for name in sorted(files):
        if not re.fullmatch(r"[a-z][a-z0-9_]*\.json", name):
            raise ValueError(f"unsafe table file: {name}")
        path = root / "data/static/tables" / name
        raw = path.read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        document = validate_document(load_json(path))
        contracts = manifest["table_contracts"][name]
        if not isinstance(contracts, dict) or set(contracts) != {t["name"] for t in document["tables"]}:
            raise ValueError(f"{name}: ABI table coverage differs from manifest")
        for table in document["tables"]:
            if json.dumps(contracts[table["name"]], sort_keys=True) != json.dumps({"c_type": table["c_type"], "dimensions": table["dimensions"]}, sort_keys=True):
                raise ValueError(f"{name}/{table['name']}: consumer ABI contract mismatch")
            source = table["source"]
            source_key = source.get("file", "")
            source_hash = source.get("file_sha256")
            if source.get("kind") == "existing_confirmed_definition":
                source_key += "@" + source.get("baseline_commit", "")
                source_hash = source.get("baseline_source_sha256")
            if source_key not in sources or source_hash != sources[source_key]["sha256"]:
                raise ValueError(f"{name}/{table['name']}: unknown/mismatched source identity")
            if table["name"] in names:
                raise ValueError(f"duplicate global table name: {table['name']}")
            names.add(table["name"])
            tables[table["name"]] = table["values"]
            receipt["tables"].append({"file": name, "name": table["name"], "dimensions": table["dimensions"], "input_sha256": digest, "source": table["source"]})
        outputs[Path(name).stem + ".inc"] = render(document, digest)
    validate_references(tables)
    outputs["sources.json"] = (json.dumps(receipt, ensure_ascii=False, sort_keys=True, indent=2) + "\n").encode("utf-8")
    # 全部输入验证成功后才更新任何输出，坏数据不能留下半套新定义。
    for name, data in outputs.items():
        path = output / name
        if path.exists() and path.read_bytes() == data:
            continue
        if check:
            raise ValueError(f"generated file stale/missing: {path}")
        output.mkdir(parents=True, exist_ok=True)
        temporary = path.with_suffix(path.suffix + ".tmp")
        temporary.write_bytes(data)
        temporary.replace(path)
    return len(names)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    try:
        count = generate(args.root, args.output, args.check)
    except (ValueError, OSError, TypeError) as error:
        parser.exit(1, f"static data: {error}\n")
    print(f"static data: {count} validated tables")


if __name__ == "__main__":
    main()
