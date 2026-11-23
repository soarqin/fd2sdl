#!/usr/bin/env python3
"""F1 原参照只读身份门；依据原 res_load、font FDOTHER[4] 和 MDI/DIG INI。

只冻结字节／显式配置，不启动 DOSBox，不把配置文件当运行时加载证明。
"""
from __future__ import annotations

import argparse
import configparser
import hashlib
import json
import os
from pathlib import Path
import re
import sys

from analyze_fd2_audio import archive_entries

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "docs/formats/original-reference.json"
REFERENCE_SCHEMA = "fd2-original-reference-v1"
SCHEMA = "fd2-original-reference-receipt-v2"


def parse_json(text):
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"重复 JSON 键：{key}")
            result[key] = value
        return result
    return json.loads(text, object_pairs_hook=unique)


def load_json(path):
    return parse_json(Path(path).read_text(encoding="utf-8"))


# 旧参照与 2026-11-21 的首关读集相同，唯一差异是第 23 章结算指令的角色编号。
HISTORICAL_EXE_SHA256 = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"


def stage0_executable_sha256(data):
    """确认完整 EXE；只供首关及不读取第 23 章结算指令的数据表对照。"""
    digest = hashlib.sha256(data).hexdigest()
    if digest == HISTORICAL_EXE_SHA256:
        return digest
    operand = 0x4a84b
    if data[operand:operand + 1] == bytes([13]):
        old = hashlib.sha256(data[:operand])
        old.update(bytes([18]))
        old.update(memoryview(data)[operand + 1:])
        if old.hexdigest() == HISTORICAL_EXE_SHA256:
            return digest
    raise ValueError("FD2.EXE 未通过首关完整文件身份校验")


def read_input(path):
    path = Path(path).resolve(strict=True)
    if not path.is_file():
        raise ValueError(f"不是普通文件：{path}")
    before = path.stat()
    raw = path.read_bytes()
    digest = hashlib.sha256(raw).hexdigest()
    after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise ValueError(f"读取期间文件改变：{path}")
    return {"path": str(path), "size": after.st_size, "sha256": digest}, raw


def facts(path):
    return read_input(path)[0]


def parse_config(raw):
    """仅解析声明值；autoexec 单列，不执行命令或展开宿主变量。"""
    if raw.startswith(b"\xef\xbb\xbf"):
        raise ValueError("不支持带 UTF-8 BOM 的 DOSBox 配置")
    lines, autoexec = [], []
    in_autoexec = False
    for line in raw.decode("utf-8").splitlines():
        stripped = line.strip()
        # 固定上游ParseConfigFile会激活这些行；ConfigParser却当普通注释。
        # 尚未建模条件配置，明确拒绝，不能签出不同于实际启动的属性值。
        if stripped.upper().startswith("#DOSBOX-X"):
            raise ValueError("不支持 DOSBOX-X 条件配置行")
        if stripped.startswith("[") and stripped.endswith("]"):
            in_autoexec = stripped.casefold() == "[autoexec]"
        if in_autoexec:
            if stripped and not stripped.startswith(("#", ";", "[")):
                autoexec.append(stripped)
        else:
            # 上游逐行trim后处理，不存在ConfigParser的缩进续值规则。
            # 保留每行独立含义；无'='的非注释行交给严格parser拒绝。
            lines.append(stripped)
    # 上游Section_prop::HandleInputline只以'='分隔；':'不能作赋值。
    parser = configparser.ConfigParser(interpolation=None, strict=True, delimiters=("=",))
    parser.read_string("\n".join(lines))
    if parser.defaults():
        raise ValueError("不接受配置中的 DEFAULT 继承")
    settings = {f"{section.casefold()}.{key}": value.strip()
                for section in parser.sections() for key, value in parser.items(section)}
    return settings, autoexec


def declared_config(configs, overrides):
    settings, autoexec = {}, []
    for raw in configs:
        values, commands = parse_config(raw)
        settings.update(values)
        autoexec.extend(commands)
    for value in overrides:
        match = re.fullmatch(r"([^\s=]+)\s+([^=]+)=(.*)", value)
        if not match:
            raise ValueError(f"-set 格式应为 section key=value：{value}")
        section, key, setting = match.groups()
        settings[f"{section.casefold()}.{key.strip().casefold()}"] = setting.strip()
    return settings, autoexec


def driver_name(raw):
    values = re.findall(r"^\s*DRIVER\s+(\S+)\s*$",
                        raw.decode("ascii"), flags=re.MULTILINE)
    if len(values) != 1 or Path(values[0]).name != values[0] or not re.fullmatch(r"[A-Za-z0-9_.]+", values[0]):
        raise ValueError("INI DRIVER 字段不唯一或不是文件名")
    return values[0].upper()


def build(original, dosbox, configs, overrides, bootstrap_config=None):
    inputs = {}

    def snapshot(path):
        identity, raw = read_input(path)
        if identity["path"] in inputs:
            raise ValueError("输入路径重复：" + identity["path"])
        inputs[identity["path"]] = identity
        return identity, raw

    reference_identity, reference_raw = snapshot(REFERENCE)
    reference = parse_json(reference_raw.decode("utf-8"))
    if not isinstance(reference, dict) or reference.get("schema") != REFERENCE_SCHEMA:
        raise ValueError("原参照清单 schema 不符")
    if bootstrap_config is None:
        raise ValueError("缺少版本化 bootstrap 用户配置身份")
    original = Path(original).resolve(strict=True)
    if not original.is_dir():
        raise ValueError("原资源目录不存在")
    # DOSBox 的配置环境覆盖晚于 -conf/-set；拒绝而非静默继承未知值。
    environment = sorted(key for key in os.environ if key.upper().startswith("DOSBOX_"))
    if environment:
        raise ValueError("存在未冻结的 DOSBox 环境覆盖：" + ", ".join(environment))
    resources, resource_bytes = {}, {}
    for name, expected in reference["files"].items():
        actual, raw = snapshot(original / name)
        if any(actual[key] != expected[key] for key in ("sha256", "size")):
            raise ValueError(f"原参照身份不符：{name}")
        resources[name] = actual
        resource_bytes[name] = raw
    drivers = {name: driver_name(resource_bytes[name]) for name in ("MDI.INI", "DIG.INI")}
    if any(value not in resources for value in drivers.values()):
        raise ValueError("INI 选择了未冻结的驱动")
    font = reference["font"]
    glyphs = archive_entries(resource_bytes[font["container"]])[font["index"]]
    if (len(glyphs) != font["size"] or hashlib.sha256(glyphs).hexdigest() != font["sha256"]
            or len(glyphs) != font["glyph_stride"] * font["glyph_count"]):
        raise ValueError("FDOTHER 字库子资源身份不符")
    executable, _ = snapshot(dosbox)
    if (Path(dosbox).name.casefold() != reference["dosbox"]["filename"]
            or executable["sha256"] != reference["dosbox"]["sha256"]):
        raise ValueError("DOSBox-X 二进制身份不符")
    if not configs or len({Path(p).resolve() for p in configs}) != len(configs):
        raise ValueError("须提供非空、不重复的有序 -conf 文件")
    bootstrap_identity, bootstrap_raw = snapshot(bootstrap_config)
    configuration_source = reference["configuration_source"]
    if any(bootstrap_identity[key] != configuration_source["bootstrap"][key]
           for key in ("sha256", "size")):
        raise ValueError("bootstrap 用户配置身份不符")
    bootstrap_settings, _ = parse_config(bootstrap_raw)
    config_inputs = [snapshot(path) for path in configs]
    settings, _ = declared_config([bootstrap_raw] + [raw for _, raw in config_inputs], overrides)
    # 官方启动预检清空 Section_line；bootstrap 的 autoexec 不进入最终命令。
    _, autoexec = declared_config([raw for _, raw in config_inputs], [])
    for key, expected in reference["explicit_settings"].items():
        if settings.get(key) != expected:
            raise ValueError(f"显式参照配置不符：{key} 应为 {expected}")
    inherited = configuration_source["inherited_settings"]
    for key, expected in inherited.items():
        if bootstrap_settings.get(key) != expected or settings.get(key) != expected:
            raise ValueError(f"继承参照配置不符：{key} 应为 {expected}")
    # 所有解释均来自冻结字节；后续输入读取期间改变早读文件也必须失败。
    for path, identity in inputs.items():
        if facts(path) != identity:
            raise ValueError("冻结期间输入改变：" + path)
    return {"schema": SCHEMA, "reference": reference_identity,
            "identity": reference["identity"], "original": str(original),
            "resources": resources, "font": font, "ini_driver_selection": drivers,
            "dosbox": executable, "configs": [identity for identity, _ in config_inputs],
            "bootstrap_config": bootstrap_identity, "configuration_source": configuration_source,
            "set_overrides": list(overrides), "declared_settings": settings,
            "derived_effective_settings": {key: settings[key] for key in
                                           {**inherited, **reference["explicit_settings"]}},
            "autoexec_commands": autoexec, "provenance": reference["provenance"],
            "scope": {"runtime_executed": False, "loaded_driver_pointer_verified": False,
                      "effective_default_values_measured": False,
                      "configuration_chain_verified": True,
                      "configuration_chain_evidence": "pinned upstream source, bootstrap/explicit-file identities and declared CLI policy; not a dynamic file-read trace",
                      "unspecified_settings": "unlisted effective values not measured; auto/default tokens are retained, not resolved to measured hardware state",
                      "input_save_or_tmp_frozen": False}}


def verify(manifest):
    recorded = load_json(manifest)
    if recorded.get("schema") != SCHEMA:
        raise ValueError("参照收据 schema 不符")
    actual = build(recorded["original"], recorded["dosbox"]["path"],
                   [row["path"] for row in recorded["configs"]], recorded["set_overrides"],
                   recorded["bootstrap_config"]["path"])
    if actual != recorded:
        raise ValueError("冻结参照或收据字段已改变")
    return actual


def write_new(path, receipt):
    path = Path(path).resolve()
    protected = (Path(receipt["original"]), Path(receipt["dosbox"]["path"]).parent)
    if any(path == base or base in path.parents for base in protected):
        raise ValueError("输出不得位于原资源或 DOSBox 安装目录")
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("x", encoding="utf-8", newline="\n") as output:
        json.dump(receipt, output, ensure_ascii=False, indent=2)
        output.write("\n")


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="operation", required=True)
    create = sub.add_parser("create")
    create.add_argument("--original", type=Path, required=True)
    create.add_argument("--dosbox", type=Path, required=True)
    create.add_argument("--config", action="append", type=Path, required=True)
    create.add_argument("--bootstrap-config", type=Path, required=True)
    create.add_argument("--set", dest="overrides", action="append", default=[])
    create.add_argument("--output", type=Path, required=True)
    check = sub.add_parser("verify")
    check.add_argument("manifest", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.operation == "create":
            receipt = build(args.original, args.dosbox, args.config, args.overrides,
                            args.bootstrap_config)
            write_new(args.output, receipt)
        else:
            receipt = verify(args.manifest)
        print(json.dumps({"ok": True, "identity": receipt["identity"],
                          "resources": len(receipt["resources"]),
                          "runtime_executed": False}, ensure_ascii=False))
        return 0
    except (OSError, ValueError, KeyError, IndexError, configparser.Error) as error:
        print(f"参照校验失败：{error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
