"""S2胜利2/233c6受控存档、原资源等待ROI和只读owner；不启动进程。

START只读派生，stage2 metadata与地图引用原资源；显式源Scene不批准关卡绑定。
原版观察继承S0完整RPC与既有S2双原码锚/fixup，stage变更后拒读活动owner。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct

import fd2_s0_native as s0
import s2_result_native_fixture as result_fixture
from audit_g4_render_source import archive_entries, text_fragments
from fd2_loot_oracle import story_pages as resource_story_pages
from run_g4_common_event_check import story_speaker
from run_g4_runtime_check import encode
from run_graphics_tests import decode_save
from run_original_end_turn import META, UNITS, STRIDE, round_assets, decode_lmi
from static_data.audit_stage_data import SOURCE_HASHES
from static_data.stage_source_ledger import validate_output
from static_data.verify_unit_tables import source_range

ROOT = Path(__file__).resolve().parents[1]
FLOW = "stage.source.victory.002"
SPEAKERS = (0, 1, 2, 4, 8)


def digest(raw): return hashlib.sha256(raw).hexdigest()


def derive(source, metadata):
    if len(source) != 0x59cb or sum(source[:-4]) != int.from_bytes(source[-4:], "little"):
        raise ValueError("原SAV长度/checksum不同")
    if source[META + 1:META + 3] != bytes((12, 0)) or len(metadata) != 0x83 + 40 * 26 or metadata[:3] != bytes((2, 6, 40)):
        raise ValueError("布局夹具仅接受固定START与真实stage2 metadata源域")
    plain = bytearray(source)
    plain[:len(metadata)] = metadata
    copies = []
    for raw, donor in ((4, 0), (5, 0), (6, 0), (12, 11), (13, 11), (14, 11)):
        record = source[UNITS + donor * STRIDE:UNITS + (donor + 1) * STRIDE]
        plain[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE] = record
        copies.append({"raw": raw, "source_raw": donor, "source_record_sha256": digest(record)})
    identities = ((4, 2), (5, 1), (6, 8))
    for raw, identity in identities:
        at = UNITS + raw * STRIDE
        plain[at + 7:at + 9] = bytes((identity, identity))
    # 两个可见友方避免结果raw0败亡分支；raw6必须visible才进入layout/text7路径。
    # 原stage2中央水区没有既有干燥range ROI；明确使用真实底部地形(7,22)/(8,23)
    # 两个完整正向范围资源。坐标和view均是本夹具写集，不作自然入场默认。
    plain[UNITS:UNITS + 2] = bytes((8, 22))
    plain[UNITS + 6 * STRIDE:UNITS + 6 * STRIDE + 2] = bytes((9, 22))
    for raw in range(15):
        at = UNITS + raw * STRIDE + 5
        plain[at] = plain[at] & ~1 if raw in (0, 6) else plain[at] | 1
    plain[META + 1:META + 3] = bytes((15, 2))
    plain[META + 3:META + 9] = bytes((2, 18, 8, 22, 6, 4))
    for raw in (0, 6):
        at = UNITS + raw * STRIDE
        if plain[at + 6] != 2 or int.from_bytes(plain[at + 64:at + 66], "little") == 0:
            raise ValueError("layout必须由正HP可见友方构成受控胜利源域")
    speakers = {str(identity): story_speaker(plain, identity) for identity in SPEAKERS}
    if any(plain[UNITS + raw * STRIDE + 6] == 0 and not plain[UNITS + raw * STRIDE + 5] & 1 for raw in range(15)):
        raise ValueError("受控胜利仍存在visible敌方")
    plain[-4:] = sum(plain[:-4]).to_bytes(4, "little")
    changes = [{"offset": i, "before": a, "after": b} for i, (a, b) in enumerate(zip(source, plain)) if a != b]
    allowed = set(range(len(metadata))) | set(range(UNITS, UNITS + 15 * STRIDE)) | set(range(META + 1, META + 9)) | set(range(len(source) - 4, len(source)))
    if any(row["offset"] not in allowed for row in changes):
        raise ValueError("布局派生越出metadata/active/count/stage/checksum声明写集")
    return bytes(plain), encode(plain), {"scenario": "stage2-victory-layout-double-fade", "source_flow": FLOW,
        "controlled_fixture": True, "natural_history_claimed": False, "new_runtime_binding": False,
        "source_plain_sha256": digest(source), "derived_plain_sha256": digest(plain), "plaintext_patches": changes,
        "metadata": {"entry": 7, "length": len(metadata), "sha256": digest(metadata)}, "record_copies": copies,
        "identity_overrides": [{"raw": raw, "unit_id": identity, "text_id": identity} for raw, identity in identities],
        "active_count": 15, "visible_raw": [0, 6], "hidden_raw": [raw for raw in range(15) if raw not in (0, 6)],
        "speakers": speakers, "initial_roster_count": plain[META + 9],
        "controlled_positions": {"raw0": [8, 22], "raw6": [9, 22], "camera": [2, 18], "focus": [8, 22],
                                 "reason": "two actual dry neighboring range ROIs, no natural stage2 history claim"},
        "expected_prefix": {"layout_raw": list(range(7)), "view": [2, 0, 2, 0], "display_mode": 0,
                            "text": 7, "after_text_roster_append_id": 2, "after_text_next_stage": 3},
        "owner_boundary": "only stage2/count15 before final text7 ACK; stage3 original may free/replace active owner",
        "unknown_regions": "metadata unused tail, active after15, roster/manual slots, session unknown tail preserved"}


def story_pages(original, plain, assets):
    """与既有opening ROI相同的16e24滚动规则，只处理真实FDTXT[3]fragment7。"""
    tokens = text_fragments(archive_entries((original / "FDTXT.DAT").read_bytes())[3])[7]
    controls = {token for token in tokens if token < 0}
    if controls != {-18, -17, -3, -2, -1} or tokens[-1] != -1:
        raise ValueError("stage2 story7不在已核文本控制域")
    identities = {tokens[i + 1] for i, token in enumerate(tokens[:-1]) if token in (-17, -18)}
    if identities != set(SPEAKERS): raise ValueError("story7说话人集合不同")
    pages = resource_story_pages(original, plain, assets, 7, stage=2)
    for page in pages:
        if page['known_text_pixels'] < 256: raise ValueError('story7等待ROI已知像素不足')
        page['speaker_source'] = story_speaker(plain, page['speaker_argument'])
    speaker_controls = sum(token in (-17, -18) for token in tokens)
    return pages, {"entry": 3, "fragment": 7, "token_count": len(tokens), "glyph_tokens": sum(t >= 0 for t in tokens) - speaker_controls,
                   "speaker_text_ids": sorted(identities), "wait_count": len(pages), "controls": sorted(controls)}


def prepare(original):
    original = Path(original).resolve(strict=True)
    for name, expected in SOURCE_HASHES.items():
        if digest((original / name).read_bytes()) != expected: raise ValueError("原资源身份不同：" + name)
    start = original / "START.SAV"
    if digest(start.read_bytes()) != result_fixture.START_SHA: raise ValueError("只读START.SAV身份不同")
    source = decode_save(start)
    fields = archive_entries((original / "FDFIELD.DAT").read_bytes())
    plain, encoded, manifest = derive(source, fields[7])
    map_data = fields[6]; dimensions = struct.unpack_from("<HH", map_data)
    if dimensions != (18, 26) or len(map_data) != 4 + 18 * 26 * 4: raise ValueError("stage2地图源域不同")
    assets = round_assets(original); pages, text_source = story_pages(original, plain, assets)
    dato = archive_entries((original / "DATO.DAT").read_bytes()); portrait_sources = []
    for identity in SPEAKERS:
        entry = dato[identity]; frames = []
        for frame in range(4):
            first = struct.unpack_from("<I", entry, frame * 4)[0]
            last = struct.unpack_from("<I", entry, (frame + 1) * 4)[0] if frame < 3 else len(entry)
            if not 16 <= first < last <= len(entry): raise ValueError("story7完整四帧头像目录越界")
            raw = entry[first:last]
            if decode_lmi(raw).size != (80, 80): raise ValueError("story7完整头像不是80x80")
            frames.append({"frame": frame, "entry_offset": first, "length": len(raw), "sha256": digest(raw)})
        portrait_sources.append({"entry": identity, "frames": frames})
    executable = (original / "FD2.EXE").read_bytes()
    arrays = [{"canonical_va": hex(va), "raw_hex": (raw := source_range(executable, va, 7)).hex(), "sha256": digest(raw)}
              for va in (0x520ba, 0x520c1, 0x520c8)]
    layout = [{"raw": raw, "x": bytes.fromhex(arrays[0]["raw_hex"])[raw],
               "y": bytes.fromhex(arrays[1]["raw_hex"])[raw], "direction": bytes.fromhex(arrays[2]["raw_hex"])[raw]}
              for raw in range(7)]
    manifest.update(source_save_sha256=digest(start.read_bytes()), encoded_sha256=digest(encoded),
        resource_sha256={name: digest((original / name).read_bytes()) for name in (*SOURCE_HASHES, "FDOTHER.DAT", "DATO.DAT", "FDICON.B24")},
        map={"entry": 6, "dimensions": list(dimensions), "sha256": digest(map_data)}, text_source=text_source,
        complete_portrait_frames=portrait_sources,
        layout_source={"caller": "0x230f2", "helper": "0x233c6", "arrays": arrays, "expected_prefix": layout},
        waits=[{key: value for key, value in page.items() if key not in ("text", "portraits", "arrows")} |
               {"text_roi_sha256": digest(page["text"].tobytes())} for page in pages])
    return {"original": original, "source": source, "plain": plain, "encoded": encoded,
            "manifest": manifest, "assets": assets, "pages": pages}


def original_owner(memory, original, descriptor):
    # actors=False仅复用代码身份与global读证明；读活动指针前执行本fixture stage/count门。
    state, proof = result_fixture.original_owner(memory, original, descriptor, actors=False)
    if state["stage"] != 2 or state["count"] != 15:
        raise ValueError("原owner已离开stage2/count15，禁止读取释放/替换后的活动表")
    base = proof["object2_DS_offset"]
    pointer = struct.unpack_from("<I", memory, base + 0x3a45)[0]
    if not 0 < pointer <= len(memory) - 15 * STRIDE: raise ValueError("stage2活动owner指针越界")
    records = memory[pointer:pointer + 15 * STRIDE]
    state.update(actors=s0.active_fields(records, 15), records_hex=records.hex())
    proof.update(scope="stage2 controlled layout prefix only; no post-stage3 owner read", active_pointer_DS_offset=pointer)
    return state, proof


def sdl_owner(cdb, target, exe, pdb, work):
    """从命名fixture Game一次owned快照读prefix；拒绝stage3冒充原stage2活动owner。"""
    work = Path(work); record = {"symbols": s0.cdb_api.symbols(exe, pdb, Path(cdb).parent / "dbghelp.dll")}
    names = ("stage", "units.count", "battle_result", "roster_count", "ordinary_display_mode",
             "camera_cell_x", "camera_cell_y", "cursor_cell_x", "cursor_cell_y")
    with s0.cdb_api.CDB(cdb, target, work, record) as debugger:
        debugger.request(".sympath " + str(Path(pdb).resolve().parent))
        debugger.request(".reload /f " + Path(exe).name)
        def get(expression): return s0.cdb_api.cpp_integer(debugger.request("?? " + expression))
        address, size = get("&s2_layout_game"), get("sizeof(s2_layout_game)")
        if not address or not 7680 <= size <= 1024 * 1024: raise ValueError("PDB layout Game地址/大小无效")
        offsets = {name: get("(char *)&s2_layout_game." + name + " - (char *)&s2_layout_game") for name in names + ("units.items",)}
        widths = {name: get("sizeof(s2_layout_game." + name + ")") for name in names}
        data, receipt = s0.cdb_api.read_owned_memory(target, address, size, exe)
        def field(name):
            at, width = offsets[name], widths[name]
            if width not in (1, 2, 4, 8) or not 0 <= at <= len(data) - width: raise ValueError("PDB layout字段布局越界")
            return int.from_bytes(data[at:at + width], "little")
        if field("stage") != 2 or field("units.count") != 15 or field("battle_result") not in (0, 1, 2):
            raise ValueError("SDL layout owner已离开stage2/count15，不作stage3流程观察")
        at = offsets["units.items"]
        if not 0 <= at <= len(data) - 15 * STRIDE: raise ValueError("SDL layout原记录前缀越界")
        records = data[at:at + 15 * STRIDE]
        state = {"stage": 2, "count": 15, "cached_result": field("battle_result"), "roster_count": field("roster_count"),
            "display_mode": field("ordinary_display_mode"), "actors": s0.active_fields(records, 15), "records_hex": records.hex(),
            "camera": [field("camera_cell_x"), field("camera_cell_y")], "focus": [field("cursor_cell_x"), field("cursor_cell_y")]}
        path = work / "sdl-game.bin"
        if path.exists(): raise ValueError("PDB layout快照路径已存在")
        path.write_bytes(data)
        record.update(layout={"address": address, "size": size, "offsets": offsets, "widths": widths}, memory_read=receipt,
            snapshot={"path": str(path), "size": size, "sha256": digest(data)}, named_fixture_owner=True)
    if digest(Path(exe).read_bytes()) != record["symbols"]["exe_sha256"] or digest(Path(pdb).read_bytes()) != record["symbols"]["pdb_sha256"]:
        raise ValueError("读取期间layout fixture/PDB改变")
    return state, record


def validate_layout_prefix(state, manifest):
    if state["stage"] != 2 or state["count"] != 15:
        raise ValueError("layout prefix必须在stage2/count15有效owner观察")
    records = bytes.fromhex(state["records_hex"])
    if len(records) != 15 * STRIDE: raise ValueError("layout prefix原记录长度不同")
    for expected in manifest["layout_source"]["expected_prefix"]:
        unit = records[expected["raw"] * STRIDE:(expected["raw"] + 1) * STRIDE]
        if (unit[0], unit[1], unit[3]) != (expected["x"], expected["y"], expected["direction"]):
            raise ValueError("layout raw0..6坐标/方向不符独立EXE数组")
    if any(records[raw * STRIDE + 5] & 0x80 for raw in range(15)):
        raise ValueError("原13536完整活动表clearacted尚未到达")
    if state["camera"] != [2, 0] or state["focus"] != [2, 0]:
        raise ValueError("layout首次story7等待前镜头/焦点不符")
    return {"raw_prefix": 7, "complete_acted_count": 15, "camera": [2, 0], "focus": [2, 0],
            "scope": "first story7 wait only; later visible speaker may move focus"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    output = validate_output(args.output_dir, root=ROOT, original=args.original.resolve(strict=True))
    p = prepare(args.original)
    output.mkdir(parents=True)
    (output / "controlled.SAV").write_bytes(p["encoded"])
    (output / "fixture.json").write_text(json.dumps(p["manifest"], ensure_ascii=False, indent=2) + "\n", encoding="utf8")
    print(json.dumps({"status": "prepared-only", "source_flow": FLOW, "waits": len(p["pages"]),
                      "gui_started": False, "output": str(output)}, ensure_ascii=False))


if __name__ == "__main__": main()
