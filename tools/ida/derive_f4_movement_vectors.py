#!/usr/bin/env python3
"""从已取得完整指令推导合成地图向量；不导入/运行待测C，不执行原CPU。

4e390/4e42c/4e4be、4e4f6/4e5cc/4e680/4e703/4e71f/4e751
及145cd/146d1为固定来源。输出完整cells/raw3、屏蔽前reachable与屏蔽后target；
路径向量只使用scratch mode未被frame覆盖的浅参数，不签正常producer或深BSS联合域。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path

SOURCE_SHA = "18617904b37d73b066eea7a78889d6da64fc6dad895b9292ae279df4b8f47517"
IMAGE_SHA = "8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1"
FUNCTIONS = (0x145cd, 0x14625, 0x146a7, 0x146d1, 0x4df4c,
             0x4e390, 0x4e42c, 0x4e4be, 0x4e4f6, 0x4e5cc,
             0x4e680, 0x4e703, 0x4e71f, 0x4e751, 0x18890, 0x14237, 0x14b78)
NEIGHBORS = ((1, 0, 3), (-1, 0, 1), (0, 1, 0), (0, -1, 2))


def sha(data):
    return hashlib.sha256(data).hexdigest()


def signed(value):
    return value - 256 if value & 128 else value


def input_id(path):
    data = path.read_bytes()
    stat = path.stat()
    return dict(path=str(path.resolve()), sha256=sha(data), size=len(data), mtime_ns=stat.st_mtime_ns)


def source_guard(source, image):
    raw = source.read_bytes(); image_raw = image.read_bytes()
    if sha(raw) != SOURCE_SHA or sha(image_raw) != IMAGE_SHA:
        raise ValueError("fixed source/image identity differs")
    fs = {f["va"]: f for f in json.loads(raw)["functions"]}
    counts = {}
    for va in FUNCTIONS:
        f = fs[va]
        ins = {r["va"]: r for r in f["instructions"]}
        if len(ins) != len(f["instructions"]):
            raise ValueError("duplicate instruction head")
        covered = set()
        for chunk in f["chunks"]:
            data = bytes.fromhex(chunk["bytes"])
            start, end = chunk["start"], chunk["end"]
            if len(data) != end - start or sha(data) != chunk["sha256"] or image_raw[start:end] != data:
                raise ValueError(f"strict chunk bytes differ: {va:x}")
            pos = start
            while pos < end:
                if pos in covered:
                    raise ValueError("overlapping source chunks")
                covered.add(pos)
                row = ins[pos]
                size = row["size"]
                if size <= 0 or pos + size > end:
                    raise ValueError("contiguous source instruction bounds differ")
                pos += size
        if covered != set(ins):
            raise ValueError("instruction outside complete chunks")
        counts[hex(va)] = len(ins)
    return counts


def marked(case):
    w, h = case["width"], case["height"]
    cells = list(case["cells"])
    if not case["apply"]:
        cells = [(v & 0x001f03ff) | 0xff000000 for v in cells]
    mover = case["units"][0]
    for side, x, y, flags in case["units"]:
        if flags & 1 or (side == 0) == (mover[0] == 0):
            continue
        cells[y*w+x] |= 0x00400000
        for dx, dy, _ in NEIGHBORS:
            nx, ny = x+dx, y+dy
            if 0 <= nx < w and 0 <= ny < h:
                cells[ny*w+nx] |= 0x00800000
    return cells


def range_expected(case):
    w, h, budget = case["width"], case["height"], case["budget"]
    cells = marked(case)
    _, sx, sy, _ = case["units"][0]
    start = sy*w+sx
    cells[start] = (cells[start] & 0xffffff) | (budget << 24)
    # Literal source DFS; explicit frames only avoid Python recursion limits.
    frames = [[sx, sy, budget, 0]]
    max_depth = 1
    while frames:
        x, y, remaining, n = frames[-1]
        if n == 4:
            frames.pop(); continue
        frames[-1][3] += 1
        dx, dy, _ = NEIGHBORS[n]
        nx, ny = x+dx, y+dy
        if not (0 <= nx < w and 0 <= ny < h):
            continue
        k = ny*w+nx; cell = cells[k]
        cost = case["costs"][case["classes"][cell & 0x3ff]]
        if cost > remaining:
            continue  # 4e4d8 unsigned SUB borrow
        value = remaining - cost
        if signed(value) <= signed(cell >> 24) or cell & 0x400000:
            continue  # 4e4df signed JLE, then40
        if cell & 0x800000:
            value = 0  # 4e4ea..4e4ee, only after comparison
        cells[k] = (cell & 0xffffff) | (value << 24)
        frames.append([nx, ny, value, 0]); max_depth = max(max_depth, len(frames))
    before_friend = [v >> 24 for v in cells]
    for side, x, y, flags in case["units"][1:]:
        if not flags & 1 and (side == 0) == (case["units"][0][0] == 0):
            cells[y*w+x] |= 0xff000000
    distances = [0xffffffff if value == 255 else (budget-value) & 255 for value in before_friend]
    stop = sum((1 << i) for i, value in enumerate(cells) if value >> 24 != 255)
    expand = sum((1 << i) for i, value in enumerate(before_friend)
                 if value != 255 and (i == start or not cells[i] & 0x800000))
    return dict(cells=cells, before_friend_raw3=before_friend, distances=distances,
                can_stop_mask=stop, can_expand_mask=expand, max_frame_depth=max_depth)


def path_expected(case):
    w, h = case["width"], case["height"]
    mode = case["mode"] & 3
    case = dict(case, apply=0)
    cells = marked(case)
    if case["mode"] & 4:
        cells = [(v & 0x001f03ff) | 0xff000000 for v in cells]
    _, sx, sy, _ = case["units"][0]
    cells[sy*w+sx] = (cells[sy*w+sx] & 0xffffff) | (case["budget"] << 24)
    route = []; best = None; witness = None; max_depth = 0
    def visit(x, y, remaining):
        nonlocal best, witness, max_depth
        max_depth = max(max_depth, len(route)+1)
        if max_depth >= 33:
            raise ValueError("path fixture reaches unresolved shared mode-byte alias")
        if mode != 2 and [x, y] == case["target"] and (best is None or len(route) <= len(best)):
            best = list(route)
        for dx, dy, direction in NEIGHBORS:
            nx, ny = x+dx, y+dy
            if not (0 <= nx < w and 0 <= ny < h): continue
            k = ny*w+nx; cell = cells[k]
            cost = case["costs"][case["classes"][cell & 0x3ff]]
            if cost > remaining: continue
            value = remaining-cost
            route.append(direction)
            runs = 1 + sum(a != b for a, b in zip(route, route[1:]))
            score = (runs*4) & 255
            old = cell >> 24
            if signed(value) < signed(old) or (value == old and (mode != 1 or score <= ((cell >> 8) & 252))):
                route.pop(); continue
            # 4e6ca writes turn score before40 gate, even if theedge later rejects.
            cell = (cell & ~0xfc00) | (score << 8)
            cells[k] = cell
            if mode == 2:
                cells[k] = (cell & 0xffffff) | (value << 24)
                if cell & 0x400000: witness = [nx, ny]
            else:
                if cell & 0x400000:
                    route.pop(); continue
                if cell & 0x800000: value = 0
                cells[k] = (cell & 0xffffff) | (value << 24)
            visit(nx, ny, value)
            route.pop()
    visit(sx, sy, case["budget"])
    return dict(found=int(witness is not None if mode == 2 else best is not None),
                directions=best or [] if mode != 2 else [], witness=witness or [-1,-1],
                cells=cells, max_frame_depth=max_depth)


def cases(profiles):
    out = []
    def add(name, w, h, budget, classes, costs, units, apply=0, cells=None, mode=None, target=None):
        row = dict(name=name, width=w, height=h, budget=budget, classes=classes,
                   costs=costs, units=units, apply=apply,
                   cells=cells if cells is not None else [0xff003c00 | (i % len(classes)) for i in range(w*h)])
        if mode is None: row["expected"] = range_expected(row)
        else:
            row.update(mode=mode, target=target); row["expected"] = path_expected(row)
        out.append(row)
    for budget in range(256):
        add(f"byte-{budget}", 3, 1, budget, [0], [1], [[2,1,0,0]])
    for profile, costs in enumerate(profiles):
        for terrain_class in range(20):
            add(f"profile-{profile}-class-{terrain_class}", 3, 1, 5,
                [terrain_class], costs, [[2,1,0,0]])
    for budget in (0,1,5,20,127,128,129,130,147,148,149,254,255):
        add(f"mixed-{budget}", 3, 2, budget, [0,1,2], [1,20,2], [[2,1,1,0]])
    for side in (0,1,2):
        enemy = 2 if side == 0 else 0
        for hidden in (0,1):
            for apply in (0,1):
                add(f"occupied-{side}-{hidden}-{apply}", 4,3,5,[0],[1],
                    [[side,0,1,0],[side,1,1,0],[enemy,3,1,hidden],
                     [side,0,1,0],[enemy,1,1,hidden]],apply)
    add("zero-cost",3,2,0,[0],[0],[[2,1,1,0]])
    add("stale-owner",3,1,2,[0],[1],[[2,1,0,0]],1,[0x7f203c00,0x80203c00,0xff203c00])
    for mode in (0,1,2,5):
        for budget in (0,1,2,3,127,128,129,255):
            add(f"path-{mode}-{budget}",2,2,budget,[0],[1],
                [[2,1,1,0],[0,0,0,0]],mode=mode,target=[0,0])
    for mode in (0,1,2,5):
        for budget in (0,1,2,3,127,128,129,255):
            add(f"path-free-{mode}-{budget}",2,2,budget,[0],[1],
                [[2,1,1,0]],mode=mode,target=[0,0])
    # Independent literal tie with noZOC: first R/L/D/U hit is LEFT,UP.
    add("path-literal-tie",2,2,2,[0],[1],[[2,1,1,0]],mode=0,target=[0,0])
    assert out[-1]["expected"]["directions"] == [1,2]
    assert out[129]["expected"]["before_friend_raw3"] == [255,129,255]
    assert out[255]["expected"]["can_stop_mask"] == 0
    return out


def header(rows):
    ranges = [r for r in rows if "mode" not in r]
    paths = [r for r in rows if "mode" in r]
    def nums(values): return "{" + ",".join(str(x)+"u" for x in values) + "}"
    def units(values): return "{" + ",".join(nums(v) for v in values) + "}"
    lines = ["/* Generated by tools/ida/derive_f4_movement_vectors.py; source-derived, no C oracle. */",
        "#ifndef FD2_FIELD_MOVE_SOURCE_VECTORS_H", "#define FD2_FIELD_MOVE_SOURCE_VECTORS_H",
        "typedef struct {", " const char *name; unsigned width,height,budget,apply,unit_count,class_count,cost_count;",
        " uint8_t classes[4],costs[20],units[5][4]; uint32_t before[16],after[16],distance[16];",
        " unsigned stop,expand;", "} field_range_source_vector;", "static const field_range_source_vector k_source_ranges[] = {"]
    for r in ranges:
        e=r["expected"]
        lines.append(" {\"%s\",%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s}," %
            (r["name"],r["width"],r["height"],r["budget"],r["apply"],len(r["units"]),len(r["classes"]),len(r["costs"]),
             nums(r["classes"]),nums(r["costs"]),units(r["units"]),nums(r["cells"]),nums(e["cells"]),nums(e["distances"]),e["can_stop_mask"],e["can_expand_mask"]))
    lines += ["};", "typedef struct {", " const char *name; unsigned width,height,budget,mode,unit_count;",
        " uint8_t units[5][4]; uint32_t cells[16]; int tx,ty,found,wx,wy; unsigned length; uint8_t path[16];",
        "} field_path_source_vector;", "static const field_path_source_vector k_source_paths[] = {"]
    for r in paths:
        e=r["expected"]
        lines.append(" {\"%s\",%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s}," %
            (r["name"],r["width"],r["height"],r["budget"],r["mode"],len(r["units"]),units(r["units"]),nums(r["cells"]),
             *r["target"],e["found"],*e["witness"],len(e["directions"]),nums(e["directions"] or [0])))
    lines += ["};", "#endif", ""]
    return "\n".join(lines)


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source",type=Path,required=True)
    parser.add_argument("--image",type=Path,required=True)
    parser.add_argument("--movement",type=Path,required=True)
    parser.add_argument("--output",type=Path,required=True)
    args=parser.parse_args()
    inputs=[args.source,args.image,args.movement,Path(__file__)]
    before=[input_id(p) for p in inputs]
    functions=source_guard(args.source,args.image)
    profiles=json.loads(args.movement.read_text(encoding="utf8"))["tables"][0]["values"]
    if len(profiles)!=29 or any(len(p)!=20 or min(p)<1 or max(p)>255 for p in profiles):
        raise ValueError("movement cold domain differs")
    movement_bytes=bytes(value for row in profiles for value in row)
    if args.image.read_bytes()[0x61646:0x6188a] != movement_bytes:
        raise ValueError("movement cold bytes differ from fixed original image@61646")
    rows=cases(profiles)
    args.output.mkdir(parents=True,exist_ok=False)
    (args.output/"vectors.json").write_text(json.dumps(dict(schema="fd2-source-byte-movement-v1",vectors=rows),indent=2)+"\n",encoding="utf8")
    (args.output/"field_move_source_vectors.h").write_text(header(rows),encoding="utf8",newline="\n")
    after=[input_id(p) for p in inputs]
    if before!=after: raise ValueError("source inputs changed during derivation")
    report=dict(ok=True,actual_original_cpu_runs=0,actual_c_runs=0,functions=functions,
                movement_image_va="0x61646",movement_byte_count=580,movement_sha256=sha(movement_bytes),
                range_vectors=sum("mode" not in r for r in rows),path_vectors=sum("mode" in r for r in rows),
                complete_range_cells=sum(r["width"]*r["height"] for r in rows if "mode" not in r),
                max_range_frames=max(r["expected"]["max_frame_depth"] for r in rows if "mode" not in r),
                max_path_frames=max(r["expected"]["max_frame_depth"] for r in rows if "mode" in r),
                before=before,after=after,
                claims=dict(static_instruction_derived=True,normal_producer=False,deep_scratch_alias=False,
                            entire_byte_path_domain=False,native=False))
    (args.output/"receipt.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf8")
    print(json.dumps({k:v for k,v in report.items() if k not in ("before","after","functions")}))

if __name__ == "__main__":
    main()
