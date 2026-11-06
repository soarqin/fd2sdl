#!/usr/bin/env python3
"""只读复核 B7 首段结算的 576 像素光标格；不运行游戏或生成截图。

依据 field_stage1_victory_play @0x22f37、field_focus_move_to_actor
@0x12d7b 和 field_focus_move_to_cell @0x12cea。仅支持已证 raw6 场景，
分别比较两端三个合法 idle 的有／无光标族，不要求两端同相。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

from PIL import Image

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
from run_graphics_tests import decode_save
from run_original_comparison import normalize, archive_entry, decode_image
from run_original_end_turn import round_assets
from run_original_player import sheet_frame
from run_original_focus import native_focus_reference


def require(condition, message):
    if not condition:
        raise ValueError(message)


def fact(path):
    path = Path(path).resolve()
    data = path.read_bytes()
    stat = path.stat()
    return {"path": str(path), "sha256": hashlib.sha256(data).hexdigest(),
            "size": len(data), "mtime_ns": stat.st_mtime_ns}


def unchanged(before):
    return all(fact(row["path"]) == row for row in before)


def output_path(root, output):
    output = Path(output).resolve()
    require(not output.is_relative_to((Path(root) / "original_game").resolve()),
            "输出不得位于实际 original_game 资源目录")
    if output.exists():
        raise FileExistsError("拒绝覆盖既有派生收据")
    return output


def validate_geometry(plain, report):
    count = plain[0x30c4]
    require(plain[0x30c5] == 1 and 7 <= count <= 64, "需要 stage1 完整活动表")
    units = [plain[0x12a3 + i * 80:0x12a3 + (i + 1) * 80] for i in range(count)]
    unit = units[6]
    require(len(unit) == 80 and list(unit[:2]) == [15, 13] and
            unit[7:9] == bytes((133, 133)) and not unit[5] & 0x81 and
            unit[3:5] == bytes(2), "raw6 身份、位置、方向或存活前置不符")
    require(not any(not u[5] & 1 and list(u[:2]) == [15, 14] for u in units),
            "光标格下邻存在 actor，不能使用本模板")
    native = next(c for c in report["cases"] if c["kind"] == "original")
    hero = next(p for p in native["checkpoints"] if p["name"] == "b7-hero-focus")
    focus = native_focus_reference(hero["camera"], hero["world"], list(unit[:2]), (27, 21))
    sdl_focus = native_focus_reference(list(plain[0x30c6:0x30c8]),
        list(plain[0x30c8:0x30ca]), list(unit[:2]), (27, 21))
    require(focus["camera"] == [9, 8] and focus["screen"] == [148, 124] and
            sdl_focus["camera"] == focus["camera"] and sdl_focus["screen"] == focus["screen"],
            "原版 focus ledger 或活动快照镜头不符")
    for role in ("original", "sdl"):
        case = next(c for c in report["cases"] if c["kind"] == role)
        page = next(p for p in case["checkpoints"] if p["name"] == "b7-settlement-00")
        require(page["fragment"] in (6, 7) and page["speaker_control"] == -19 and
                page["speaker_argument"] == 6 and page["portrait_id"] == 133,
                "结算首段 speaker／portrait 合同不符")
    return unit, focus, sdl_focus


def resource_templates(original):
    assets = round_assets(original)
    field = archive_entry((original / "FDFIELD.DAT").read_bytes(), 3)
    require(struct.unpack_from("<HH", field) == (27, 21), "stage1 地图尺寸不符")
    shape = (original / "FDSHAP.DAT").read_bytes()
    tiles, attrs = archive_entry(shape, 2), archive_entry(shape, 3)
    tile = struct.unpack_from("<I", field, 4 + 4 * (13 * 27 + 15))[0] & 1023
    require(tile == 73 and attrs[4 * tile] == 0, "光标格地形／前景条件不符")
    terrain = sheet_frame(tiles, tile)
    terrain.putpalette(assets["palette"])
    icon = (original / "FDICON.B24").read_bytes()
    templates = []
    for cursor in (False, True):
        for phase in range(3):
            index = 133 * 12 + phase
            first, last = struct.unpack_from("<II", icon, 6 + 4 * index)
            sprite = decode_image(struct.pack("<HH", 24, 24) + icon[first:last])
            sprite.putpalette(assets["palette"])
            template = terrain.copy()
            if cursor:
                template.paste(assets["cursor"], (0, 0), assets["cursor"].point(
                    lambda px: 255 if px else 0, mode="L"))
            template.paste(sprite, (0, -6), sprite.point(lambda px: 255 if px else 0, mode="L"))
            templates.append((cursor, phase, template.convert("RGB")))
    return templates, tile, list(attrs[tile * 4:tile * 4 + 4])


def compare_roi(actual, templates):
    require(actual.size == (24, 24), "必须比较完整 576 像素光标格")
    checks = []
    for cursor, phase, template in templates:
        delta = [max(abs(a - b) for a, b in zip(x, y))
                 for x, y in zip(actual.convert("RGB").getdata(), template.getdata())]
        checks.append({"cursor": cursor, "phase": phase, "checked_pixels": 576,
            "bad_pixels_over3": sum(v > 3 for v in delta), "max_delta": max(delta)})
    families = sorted({row["cursor"] for row in checks if not row["bad_pixels_over3"]})
    return {"matches": checks, "accepted_cursor_families": families,
            "unique_no_cursor_family": families == [False]}


def verify(root, artifacts):
    original = Path(root).resolve() / "original_game"
    base = Path(artifacts).resolve()
    manifest_path, report_path = base / "fixture-manifest.json", base / "comparison.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    require(manifest["scenario"] in ("victory-all-villagers", "victory-villager-loss"),
            "仅支持已证首6／7两分支")
    source = Path(manifest["source_path"])
    if not source.is_absolute():
        source = base / source
    source = source.resolve()
    pngs = [base / role / "b7-settlement-00.png" for role in ("original", "sdl")]
    # 记录实际读取的原资源及本地 Python 来源，端末核 bytes/size/mtime；不写原件。
    code = {Path(module.__file__).resolve() for module in tuple(sys.modules.values())
            if getattr(module, "__file__", None) and
            Path(module.__file__).resolve().is_relative_to(TOOLS)}
    code.add(Path(__file__).resolve())
    paths = [source, manifest_path, report_path, *pngs,
        *(original / name for name in ("FDFIELD.DAT", "FDSHAP.DAT", "FDICON.B24", "FDOTHER.DAT", "FDTXT.DAT")),
        *sorted(code)]
    before = [fact(path) for path in paths]
    require(before[0]["sha256"] == manifest["source_sha256"], "fixture source SHA 与 manifest 不符")
    plain = bytearray(decode_save(source))
    for patch in manifest["plaintext_patches"]:
        require(plain[patch["offset"]] == patch["before"], "manifest patch before 不符")
        plain[patch["offset"]] = patch["after"]
    require(hashlib.sha256(plain).hexdigest() == manifest["derived_plain_sha256"], "派生明文 SHA 不符")
    report = json.loads(report_path.read_text(encoding="utf-8"))
    unit, focus, sdl_focus = validate_geometry(plain, report)
    templates, tile, attrs = resource_templates(original)
    at = focus["screen"]
    observed = []
    for role, png in zip(("original", "sdl"), pngs):
        with Image.open(png) as image:
            actual = normalize(image.convert("RGB")).crop((at[0], at[1], at[0] + 24, at[1] + 24))
        observed.append({"role": role, "png": fact(png), **compare_roi(actual, templates)})
    return {"schema": "fd2-b7-existing-ending-frame-review-v1", "source": before,
        "source_raw6": unit.hex(), "world": [15, 13], "camera": focus["camera"],
        "roi": [*at, 24, 24], "source_tile": tile, "attrs": attrs, "lower_actor_absent": True,
        "focus_ledger": {"original": focus, "sdl_from_actual_snapshot": sdl_focus},
        "observed": observed, "passed": all(row["unique_no_cursor_family"] for row in observed),
        "guards": unchanged(before),
        "scope": "Existing first6/7 dialog frame only; not a new GUI run or continuous focus/PAN witness. "
                 "576 pixels cover the cursor cell, not a full actor sprite. Independent endpoint idle phases."}


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=TOOLS.parent)
    parser.add_argument("--artifacts", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    output = output_path(args.root, args.output)  # 任何分析／mkdir前保护实际资源目录。
    result = verify(args.root, args.artifacts)
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
    print(json.dumps({"passed": result["passed"], "guards": result["guards"],
        "output": str(output)}, ensure_ascii=False))
    return 0 if result["passed"] and result["guards"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
