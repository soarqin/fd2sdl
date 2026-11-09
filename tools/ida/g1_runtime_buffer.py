"""G1 stage32透明点已定义来源下界；只读原资源，不重建最终截图或捕获色。

依据3231b首pan(3,34)、135dd逐格刷新、11cac工作页+32904和11eee/4e22a。
camera(3,15)仅是既有13点截图的资源配准夹具，不作通用场景默认值。
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from run_original_comparison import archive_entry
from fd2_battle_animation_frames import rle_material

HASHES = {
    "FDFIELD.DAT": "81310211c6e96ebd89e79648650696691c740229fa4095ece37e7226bc27b74d",
    "FDSHAP.DAT": "901b70ea82d5d977192759fad510921ffe16a0ab6af6ab7c32757de03e30aa3c",
}


def audit(original):
    source = {}
    for name, expected in HASHES.items():
        source[name] = (Path(original) / name).read_bytes()
        if hashlib.sha256(source[name]).hexdigest() != expected:
            raise ValueError(f"{name} identity mismatch")
    field, shapes = source["FDFIELD.DAT"], source["FDSHAP.DAT"]
    grid, metadata = archive_entry(field, 96), archive_entry(field, 97)
    width, height = struct.unpack_from("<HH", grid)
    sheet = archive_entry(shapes, metadata[0] * 2)
    attributes = archive_entry(shapes, metadata[0] * 2 + 1)
    count = struct.unpack_from("<H", sheet, 4)[0]
    cache = {}

    def tile(identity):
        if identity not in cache:
            if not 0 <= identity < count:
                raise ValueError("tile outside sheet")
            start = struct.unpack_from("<I", sheet, 6 + identity * 4)[0]
            end = struct.unpack_from("<I", sheet, 10 + identity * 4)[0] if identity + 1 < count else len(sheet)
            cache[identity] = rle_material(struct.pack("<HH", 24, 24) + sheet[start:end])
        return cache[identity]

    def cell(x, y):
        if not 0 <= x < width or not 0 <= y < height:
            raise ValueError("map coordinate outside source")
        return struct.unpack_from("<H", grid, 4 + 4 * (y * width + x))[0] & 1023

    points = []
    for row in range(8):
        for column in range(13):
            final = cell(3 + column, 15 + row)
            _, mask = tile(final)
            for index, written in enumerate(mask.tobytes()):
                if written:
                    continue
                x, y = column * 24 + index % 24, row * 24 + index // 24
                previous = cell(3 + column, 16 + row)
                image, prior_mask = tile(previous)
                # flags08/10/04会改变动画帧；本证据不擅自选一个phase。
                if attributes[previous * 4] & 0x1c or attributes[final * 4] & 0x1c:
                    raise ValueError("animated target requires a separate phase contract")
                known = prior_mask.getpixel((index % 24, index // 24)) == 255
                points.append({"screen": [x + 4, y + 4], "work_page_offset": 32904 + y * 456 + x,
                    "final_tile": final, "earlier_camera": [3, 16], "earlier_tile": previous,
                    "earlier_world": [3 + column, 16 + row], "local": [index % 24, index // 24],
                    "earlier_opaque": known, "earlier_resource_byte": image.getpixel((index % 24, index // 24))})
    complete = len(points) == 13 and all(point["earlier_opaque"] for point in points)
    return {"schema": "fd2-g1-runtime-buffer-provenance-v1", "source_sha256": HASHES,
        "stage": 32, "final_camera_resource_fixture": [3, 15], "first_pan": [3, 34],
        "earlier_camera_is_on_first_pan_path": 0 <= 16 <= 34,
        "points": points, "all_13_have_prior_defined_write": complete,
        "conclusion": "shared-work-page-preservation" if complete else "unresolved",
        "exact_final_colors_reconstructed": False,
        "kind": "resource-and-static-call-contract-not-runtime-dump"}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()
    result = audit(args.original)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key: result[key] for key in ("conclusion", "all_13_have_prior_defined_write", "kind")}))
    raise SystemExit(0 if result["all_13_have_prior_defined_write"] else 1)
