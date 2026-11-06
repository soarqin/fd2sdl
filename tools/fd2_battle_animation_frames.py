"""B6瞬态WGC序列的独立资源分类与完整帧配对，不启动游戏或操作窗口。

依据2ebe1/2ff01/30e9d/2facd/21b18；只用原版目录、实际80B来源和既有
RLE/LMI/字体primitive。资源SKIP掩码仅识别阶段；跨端接受始终比较全部
320×200 RGB/max3，不修改颜色、不排除随机区域、不把黑色淡出当演出覆盖。
找不到同态帧是待证，不表示动作、全部时序、RNG或音频已一致。
"""
import hashlib
import io
import math
from pathlib import Path
import struct

from PIL import Image, ImageChops, ImageFilter

from fd2_item_list_oracle import number
from fd2_magic_list_oracle import fragment, glyphs
from rebuild_fd2_analysis import build_relbase_image, parse_le, parse_objects
from run_original_comparison import (SIZE, TOLERANCE, archive_entry, compare,
                                     decode_image, normalize, require_content)
from run_original_end_turn import META, STRIDE, UNITS
from run_original_player import sheet_frame

PHASES = ("physical-attack", "physical-impact", "casting", "full-fx", "map-heal")
EXE_SHA256 = "bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd"


def rle_material(data):
    """像素复用既有decoder；只把原payload改为255再解一次以得到真实写点。"""
    image = decode_image(data)
    mask_data = bytearray(data)
    at = 4
    for _ in range(image.height):
        x = 0
        while x < image.width:
            control = data[at]; at += 1
            mode, count = control & 192, (control & 63) + 1
            needed = count if mode == 128 else 0 if mode == 192 else 1
            mask_data[at:at + needed] = b"\xff" * needed
            at += needed
            x += count * 2 if mode == 64 else count
    if at != len(data):
        raise ValueError("RLE写点审计未消费完整资源")
    decoded_mask = decode_image(bytes(mask_data))
    return image, Image.frombytes("L", image.size, bytes(decoded_mask.getdata()))


def figani_parts(data, entry):
    raw = archive_entry(data, entry)
    if raw == b"\0\0\x0a":
        return [], {"entry": entry, "empty": True}
    if len(raw) < 8 or not raw[0] or 8 + raw[0] * 4 > len(raw):
        raise ValueError("FIGANI目录无效")
    starts = struct.unpack_from("<" + "I" * raw[0], raw, 8)
    if starts[0] != 8 + raw[0] * 4:
        raise ValueError("FIGANI首part不紧接目录")
    parts = []
    for index, start in enumerate(starts):
        end = starts[index + 1] if index + 1 < len(starts) else len(raw)
        if not starts[0] <= start < end <= len(raw) or end - start < 13:
            raise ValueError("FIGANI part边界无效")
        image, mask = rle_material(raw[start + 9:end])
        x, y = struct.unpack_from("<HH", raw, start)
        if x + image.width > 320 or y + image.height > 200:
            raise ValueError("当前资源阶段门不支持越出320x200的FIGANI part")
        parts.append({"image": image, "mask": mask, "at": (x, y), "index": index,
                      "marker": raw[start + 4], "duration": raw[start + 6], "layer": raw[start + 7]})
    return parts, {"entry": entry, "empty": False, "split": raw[1], "intro": raw[2],
                   "sha256": hashlib.sha256(raw).hexdigest()}


def _paste(canvas, mask, part, offset=(0, 0), tint=None):
    image = part["image"]
    if tint is not None:
        image = Image.new("P", image.size, tint)
    at = tuple(a + b for a, b in zip(part["at"], offset))
    canvas.paste(image, at, part["mask"])
    mask.paste(255, at, part["mask"])


def _anchor(image, mask, at, identity, palette):
    box = mask.getbbox()
    if box is None:
        return None
    image, mask = image.crop(box), mask.crop(box)
    image.putpalette(palette)
    rgb = image.convert("RGB")
    pixels = [(i, value) for i, (value, active) in enumerate(zip(rgb.getdata(), mask.getdata())) if active]
    positive = sum(max(value) > 32 for _, value in pixels)
    if positive < 24:
        return None
    return {"image": rgb, "mask": mask, "pixels": pixels, "at": (at[0] + box[0], at[1] + box[1]),
            "identity": identity, "checked_pixels": len(pixels), "nonblack_pixels": positive,
            "transparency": "original RLE SKIP / explicit LMI zero only"}


def _part_anchor(part, palette, identity, offset=(0, 0)):
    image, mask = Image.new("P", SIZE), Image.new("L", SIZE)
    _paste(image, mask, part, offset)
    return _anchor(image, mask, (0, 0), identity, palette)


def _distinct_part(part, other_parts, palette):
    """只取至少24个实际重叠opaque像素能否定每个idle/固定尾姿态的part。"""
    a = _part_anchor(part, palette, "candidate")
    if a is None:
        return False
    for other in other_parts:
        canvas, mask = Image.new("P", SIZE), Image.new("L", SIZE)
        _paste(canvas, mask, other)
        canvas.putpalette(palette); rgb = canvas.convert("RGB")
        count = 0
        for (i, expected) in a["pixels"]:
            at = (a["at"][0] + i % a["image"].width, a["at"][1] + i // a["image"].width)
            if mask.getpixel(at) and max(abs(x - y) for x, y in zip(expected, rgb.getpixel(at))) > 2 * TOLERANCE:
                count += 1
        if count < 24:
            return False
    return True


def _title(other, text, unit, palette):
    sheet, font = archive_entry(other, 5), archive_entry(other, 4)
    image = sheet_frame(sheet, 22, lmi=True)
    if image.size != (149, 42) or 0 in image.getdata():
        raise ValueError("2facd需要原149x42不透明summary底板")
    glyphs(image, font, fragment(archive_entry(text, 0), unit[8] + 1), 5, 4, 205)
    number(image, sheet, unit[0x21], 132, 4, 31, 2)
    image = image.crop((0, 0, 149, 21))  # 不含HP/MP数值或条；不猜effect结果。
    return _anchor(image, Image.new("L", image.size, 255), (171, 4) if unit[6] else (0, 154),
                   {"resource": "FDOTHER[5]:22+FDTXT name+LV", "unit_id": unit[7], "text_id": unit[8]}, palette)


def _background(original, source, caster, targets, all_parts, palette, full):
    stage = source[META + 2]
    if stage not in (0, 1):
        raise ValueError("背景分类只支持已确认stage0/1")
    field = (original / "FDFIELD.DAT").read_bytes()
    map_data, metadata = archive_entry(field, stage * 2), archive_entry(field, stage * 2 + 1)
    width, height = struct.unpack_from("<HH", map_data)
    if len(map_data) != 4 + width * height * 4 or metadata[0] != source[0]:
        raise ValueError("来源地图/shape与原资源不符")
    shapes = (original / "FDSHAP.DAT").read_bytes()
    attrs = archive_entry(shapes, metadata[0] * 2 + 1)
    exe = (original / "FD2.EXE").read_bytes()
    if hashlib.sha256(exe).hexdigest() != EXE_SHA256:
        raise ValueError("背景表需要已确认原EXE")
    le = parse_le(exe); raw = build_relbase_image(exe, le, parse_objects(exe, le))
    def scene(unit):
        if unit[0] >= width or unit[1] >= height:
            raise ValueError("actor坐标越界")
        immune = unit[7] != 28 and (unit[0x20] == 19 or unit[0x1f] in (4, 5))
        tile = struct.unpack_from("<I", map_data, 4 + (unit[1] * width + unit[0]) * 4)[0] & 1023
        if tile * 4 + 4 > len(attrs):
            raise ValueError("背景地形属性越界")
        return raw[0x52470 + stage] if immune and raw[0x52470 + stage] else attrs[tile * 4 + 2]
    target_scene = raw[0x52470 + stage]
    for unit in reversed(targets):
        if unit[7] == 28 or not (unit[0x20] == 19 or unit[0x1f] in (4, 5)):
            target_scene = scene(unit)
    bg_index = target_scene if caster[6] else scene(caster)
    if not full:
        bg_index = scene(targets[0]) if caster[6] else scene(caster)
    bg, bg_mask = rle_material(archive_entry((original / "BG.DAT").read_bytes(), bg_index))
    occupied = Image.new("L", SIZE)
    for part in all_parts:
        occupied.paste(255, part["at"], part["mask"])
    occupied = occupied.filter(ImageFilter.MaxFilter(37))  # 已知最大18px shake的保守排除。
    occupied.paste(255, (0, 154, 149, 196)); occupied.paste(255, (164, 157, 320, 200))
    base, mask = Image.new("P", SIZE), Image.new("L", SIZE)
    _paste(base, mask, {"image": bg, "mask": bg_mask, "at": (0, 50)})
    for y in range(50, 142, 8):
        for x in range(0, 297, 8):
            box = (x, y, x + 24, y + 12)
            if occupied.crop(box).getbbox() or mask.crop(box).getextrema() != (255, 255):
                continue
            anchor = _anchor(base.crop(box), Image.new("L", (24, 12), 255), (x, y),
                             {"resource": "BG.DAT", "entry": bg_index, "box": box}, palette)
            if anchor and len(set(anchor["image"].getdata())) >= 3:
                anchor["indexed"] = base.crop(box)
                return anchor
    raise ValueError("找不到不被actor/面板遮挡的独立BG资源块，不能略过背景正锚")


def prepare_resources(original, source, caster_raw, target_raws, *, kind, magic_id=None, camera=None):
    """source是runner已验证的实际plain SAV；不接受SDL record/engine输出代替原资源。"""
    original = Path(original)
    if len(source) != 0x59cb or sum(source[:-4]) != int.from_bytes(source[-4:], "little"):
        raise ValueError("需要完整checksum有效的plain SAV来源")
    if kind not in ("physical", "magic") or kind == "magic" and magic_id not in (*range(10), 13):
        raise ValueError("未实现该动作的资源阶段分类")
    count = source[META + 1]
    if not 1 <= count <= 64:
        raise ValueError("source活动单位数超出已实现Game范围")
    def unit(raw):
        if type(raw) is not int or not 0 <= raw < count:
            raise ValueError("actor raw超出来源")
        value = bytes(source[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE])
        if value[5] & 1 or value[6] > 2:
            raise ValueError("阶段资源需要来源中可见的合法actor")
        return value
    if not target_raws or len(set(target_raws)) != len(target_raws):
        raise ValueError("需要不重复的实际target集合")
    caster, targets = unit(caster_raw), [unit(raw) for raw in target_raws]
    if magic_id == 9 and (caster[6] or len(targets) != 1):
        raise ValueError("ID9仅已有side0/single资源证据")
    other, figani = (original / "FDOTHER.DAT").read_bytes(), (original / "FIGANI.DAT").read_bytes()
    palette6 = archive_entry(other, 0)
    if len(palette6) != 768 or max(palette6) > 63:
        raise ValueError("原调色板无效")
    palette = [x << 2 for x in palette6]
    rules = {phase: [] for phase in PHASES}
    def add(phase, variant, anchors):
        if all(anchor is not None for anchor in anchors):
            rules[phase].append({"variant": variant, "anchors": anchors})
    if kind == "magic" and magic_id == 13:
        _map_heal_rules(original, source, caster_raw, target_raws, camera, other, palette, add)
    else:
        idle, _ = figani_parts(figani, caster[7] * 3)
        clip, header = figani_parts(figani, caster[7] * 3 + (1 if kind == "physical" else 2))
        if kind == "magic" and header["empty"]:
            clip, header = figani_parts(figani, caster[7] * 3 + 1)
        if not idle or not clip or header["split"] or header["intro"] > len(clip):
            raise ValueError("当前阶段资源门只实现非empty/non-split caller；不假称split已验")
        target_idle, _ = figani_parts(figani, targets[0][7] * 3)
        title = _title(other, (original / "FDTXT.DAT").read_bytes(), caster, palette)
        bg = _background(original, source, caster, targets,
                         idle + target_idle if kind == "physical" else idle + clip + target_idle,
                         palette, kind == "magic")
        backgrounds = [bg]
        if kind == "magic" and any(p["marker"] == 1 for p in clip[:header["intro"]]):
            index = 19 if magic_id == 8 else 15 if magic_id > 3 else 11
            sheet = archive_entry(other, 3)
            lut_count = struct.unpack_from("<H", sheet, 4)[0]
            if sheet[:4] != b"LMI1" or not index + 1 < lut_count:
                raise ValueError("施法背景LUT目录无效")
            start, end = struct.unpack_from("<II", sheet, 6 + 4 * index)
            if not 6 + lut_count * 4 <= start < end <= len(sheet) or end - start != 256:
                raise ValueError("施法背景LUT不完整")
            backgrounds.append(_anchor(bg["indexed"].point(list(sheet[start:end])),
                Image.new("L", bg["image"].size, 255), bg["at"], dict(bg["identity"], lut=index), palette))
        if kind == "physical":
            for part in clip:
                # 不把其它时刻的宽幅武器姿态也并入遮挡区；每个姿态有自己的固定原资源块。
                background = _background(original, source, caster, targets, [part] + target_idle, palette, False)
                if not part["marker"] and _distinct_part(part, idle, palette):
                    add("physical-attack", part["index"], [title, background, _part_anchor(part, palette,
                        {"resource": "FIGANI", "entry": header["entry"], "part": part["index"]})])
            for part, tick, shake, tint in _physical_hit_ticks(clip):
                background = _background(original, source, caster, targets, [part] + target_idle, palette, False)
                dx, dy = PHYSICAL_SHAKE_X[shake], PHYSICAL_SHAKE_Y[shake]
                offset = (-dx, -dy) if caster[6] else (dx, dy)
                for target_part in target_idle:
                    image, mask = Image.new("P", SIZE), Image.new("L", SIZE)
                    first = bool(part["layer"] & 1) if caster[6] else not bool(part["layer"] & 1)
                    if first: _paste(image, mask, part)
                    _paste(image, mask, target_part, offset, tint)
                    if not first: _paste(image, mask, part)
                    add("physical-impact", (part["index"], target_part["index"], shake, tint), [title, background,
                        _anchor(image, mask, (0, 0), {"resource": "FIGANI native hit-shake",
                            "part_tick": tick, "shake": shake, "target_tint": tint,
                            "split_mode": header["split"]}, palette)])
        else:
            for part in clip[:header["intro"]]:
                if _distinct_part(part, idle + [clip[-1]], palette):
                    for background in backgrounds:
                        add("casting", part["index"], [title, background, _part_anchor(part, palette,
                            {"resource": "FIGANI caster intro", "entry": header["entry"], "part": part["index"]})])
            if magic_id == 0:
                fx, _ = figani_parts(other, 18 if caster[6] else 20)
                x = (40, 70, 120, 80, 50, 100, 70); y = (0, -10, -20, 0, -15, -5, 0)
                last = _part_anchor(clip[-1], palette, {"resource": "FIGANI caster last", "entry": header["entry"]})
                for i in range(7):
                    for part in fx:
                        add("full-fx", (i, part["index"]), [title, backgrounds[-1], last, _part_anchor(part, palette,
                            {"resource": "FDOTHER Magic0", "entry": 18 if caster[6] else 20, "part": part["index"]},
                            (x[i] + (0 if caster[6] else 148), y[i]))])
    dependencies = ("FD2.EXE", "FDOTHER.DAT", "FIGANI.DAT", "FDTXT.DAT", "BG.DAT", "FDFIELD.DAT", "FDSHAP.DAT", "FDICON.B24")
    return {"rules": rules, "source_sha256": hashlib.sha256(source).hexdigest(),
            "resource_sha256": {name: hashlib.sha256((original / name).read_bytes()).hexdigest() for name in dependencies},
            "scope": "resource phase identity only; actual pair acceptance is full64000 RGB", "kind": kind, "magic_id": magic_id}


# 2EBE1 @0x2ebe1：5266c/52684；split时仅竖移强制0，当前分类仍拒split。
PHYSICAL_SHAKE_X = (0, 4, 9, 14, 18, 14)
PHYSICAL_SHAKE_Y = (0, 2, 4, 6, 8, 10)


def _physical_hit_ticks(parts):
    """仅枚举命中marker触发的可达非零shake；duration0不伪造一个tick。"""
    shake, tint = 0, None
    for part in parts:
        if part["marker"]:
            shake, tint = 5, 33
        for tick in range(part["duration"]):
            if shake:
                yield part, tick, shake, tint
                shake -= 1
            tint = None


def _map_heal_rules(original, source, caster_raw, targets, camera, other, palette, add):
    camera = tuple(source[META + 3:META + 5]) if camera is None else tuple(camera)
    if len(camera) != 2 or any(type(x) is not int or x < 0 for x in camera):
        raise ValueError("地图阶段需明确合法camera")
    stage = source[META + 2]
    if stage not in (0, 1): raise ValueError("地图阶段只支持stage0/1")
    field = (original / "FDFIELD.DAT").read_bytes()
    data, meta = archive_entry(field, stage * 2), archive_entry(field, stage * 2 + 1)
    width, height = struct.unpack_from("<HH", data)
    shapes = (original / "FDSHAP.DAT").read_bytes()
    tiles, attrs = archive_entry(shapes, meta[0] * 2), archive_entry(shapes, meta[0] * 2 + 1)
    units = [bytes(source[UNITS + i * STRIDE:UNITS + (i + 1) * STRIDE]) for i in range(source[META + 1])]
    occupied = {tuple(u[:2]) for u in units if not u[5] & 1}
    terrain = None
    for ry in range(1, 7):
        for rx in range(1, 12):
            x, y = camera[0] + rx, camera[1] + ry
            if x >= width or y >= height or (x, y) in occupied or (x, y + 1) in occupied: continue
            cell = struct.unpack_from("<I", data, 4 + (y * width + x) * 4)[0]; tile = cell & 1023
            if cell >> 10 or tile * 4 + 4 > len(attrs) or attrs[tile * 4]: continue
            image = sheet_frame(tiles, tile)
            if 0 in image.getdata() or any(224 <= c < 240 for c in image.getdata()): continue
            terrain = _anchor(image, Image.new("L", image.size, 255), (4 + rx * 24, 4 + ry * 24),
                              {"resource": "FDSHAP dry source tile", "world": [x, y], "tile": tile}, palette)
            if terrain: break
        if terrain: break
    icons = (original / "FDICON.B24").read_bytes()
    witnesses = []
    for raw in [caster_raw] + list(range(len(units))):
        u = units[raw]; x, y = 4 + (u[0] - camera[0]) * 24, 4 + (u[1] - camera[1]) * 24 - 6
        if raw in targets or u[5] & 1 or not 4 <= x <= 292 or not 4 <= y <= 172: continue
        for phase in range(12):
            image = sheet_frame(icons, u[7] * 12 + phase)
            witnesses.append(_anchor(image, image.point(lambda p: 255 if p else 0, mode="L"), (x, y),
                {"resource": "FDICON identity", "raw": raw, "unit_id": u[7], "frame": phase}, palette))
        break
    if terrain is None or not witnesses: raise ValueError("地图FX缺独立地形/身份正锚")
    sheet = archive_entry(other, 6)
    for raw in targets:
        u = units[raw]; at = (4 + (u[0] - camera[0]) * 24, 4 + (u[1] - camera[1]) * 24 - 6)
        for frame in range(57, 64):
            image = sheet_frame(sheet, frame, lmi=True)
            if at[0] < 4 or at[1] < 4 or at[0] + image.width > 316 or at[1] + image.height > 196: continue
            fx = _anchor(image, image.point(lambda p: 255 if p else 0, mode="L"), at,
                         {"resource": "FDOTHER[6]", "frame": frame, "target_raw": raw}, palette)
            for identity in witnesses: add("map-heal", (raw, frame, identity["identity"]["frame"]), [terrain, identity, fx])


def _anchor_match(frame, anchor):
    x, y = anchor["at"]
    difference = ImageChops.difference(frame.crop((x, y, x + anchor["image"].width,
                                                   y + anchor["image"].height)), anchor["image"])
    # 仅原资源SKIP不参与阶段身份；literal0仍由mask=255完整检查。
    difference.paste((0, 0, 0), mask=ImageChops.invert(anchor["mask"]))
    return max(high for _, high in difference.getextrema()) <= TOLERANCE


def classify_frame(frame, resources):
    if frame.size != SIZE: raise ValueError("阶段分类需规范化320x200")
    try: require_content(frame)
    except ValueError: return {}
    result, checked = {}, {}
    def match(anchor):
        key = id(anchor)
        if key not in checked: checked[key] = _anchor_match(frame, anchor)
        return checked[key]
    for phase in PHASES:
        for rule in resources["rules"][phase]:
            if all(match(anchor) for anchor in rule["anchors"]):
                result[phase] = {"variant": rule["variant"], "anchors": [{k: a[k] for k in
                    ("identity", "at", "checked_pixels", "nonblack_pixels", "transparency")} for a in rule["anchors"]]}
                break
    if "physical-impact" in result: result.pop("physical-attack", None)
    return result


def _read_frames(rows):
    output = []; previous = -1
    for index, row in enumerate(rows):
        stamp = row["timestamp_ms"]
        if type(stamp) not in (int, float) or not math.isfinite(stamp) or stamp < 0 or stamp < previous:
            raise ValueError("序列timestamp无效/逆序")
        previous = stamp; path = Path(row["path"])
        if not path.is_absolute() or any(item["row"]["path"] == str(path.resolve()) for item in output):
            raise ValueError("序列需唯一的绝对PNG路径")
        path = path.resolve(); raw = path.read_bytes()
        digest = hashlib.sha256(raw).hexdigest()
        if row.get("sha256") != digest: raise ValueError("捕获PNG SHA256缺失或漂移")
        with Image.open(io.BytesIO(raw)) as image:
            if image.format != "PNG" or any(row.get(k, v) != v for k, v in zip(("width", "height"), image.size)):
                raise ValueError("捕获格式/尺寸元数据错误")
            frame = normalize(image)
        output.append({"image": frame, "row": {"index": index, "path": str(path), "sha256": digest,
                        "timestamp_ms": stamp}, "pixel_sha256": hashlib.sha256(frame.tobytes()).hexdigest(),
                        "grid": tuple(frame.getpixel((x, y)) for y in range(0, 200, 20) for x in range(0, 320, 20))})
    return output


def compare_sequences(original_rows, sdl_rows, resources, *, required_phases, minimum_pairs=1):
    """时间仅存证，不按固定延迟对齐；同阶段找实际完整帧，未匹配不冒pass。"""
    if not required_phases or len(set(required_phases)) != len(required_phases) or any(p not in PHASES for p in required_phases):
        raise ValueError("需明确不重复的已知阶段集合")
    if type(minimum_pairs) is not int or not 1 <= minimum_pairs <= 16:
        raise ValueError("每阶段要求1..16个不同资源姿态/画面的实际pair")
    series = {"original": _read_frames(original_rows), "sdl": _read_frames(sdl_rows)}
    if {item["row"]["path"] for item in series["original"]} & {item["row"]["path"] for item in series["sdl"]}:
        raise ValueError("两端不能复用同一个PNG文件冒充配对")
    for frames in series.values():
        for item in frames: item["phases"] = classify_frame(item["image"], resources)
    paired, missing = {}, []
    last_original = last_sdl = -1
    for phase in required_phases:
        left = [x for x in series["original"] if phase in x["phases"]]
        right = [x for x in series["sdl"] if phase in x["phases"]]
        pairs, used_original, used_sdl, used_poses, used_sdl_poses = [], set(), set(), set(), set()
        original_images, sdl_images = [], []
        for a in left:
            if a["row"]["index"] <= last_original or a["pixel_sha256"] in used_original: continue
            if any(max(high for _, high in ImageChops.difference(a["image"], previous).getextrema()) <= TOLERANCE for previous in original_images): continue
            pose = a["phases"][phase]["variant"]
            if phase == "map-heal": pose = pose[:2]  # 不把旁边actor的idle变化当成另一张治疗FX。
            if pose in used_poses: continue
            for b in right:
                if b["row"]["index"] <= last_sdl or b["pixel_sha256"] in used_sdl: continue
                if any(max(high for _, high in ImageChops.difference(b["image"], previous).getextrema()) <= TOLERANCE for previous in sdl_images): continue
                sdl_pose = b["phases"][phase]["variant"]
                if phase == "map-heal": sdl_pose = sdl_pose[:2]
                if sdl_pose in used_sdl_poses: continue
                # 固定采样点和PIL extrema只拒候选；接受仍调用完整64000像素比较。
                if any(max(abs(x - y) for x, y in zip(pa, pb)) > TOLERANCE for pa, pb in zip(a["grid"], b["grid"])): continue
                if max(high for _, high in ImageChops.difference(a["image"], b["image"]).getextrema()) > TOLERANCE: continue
                comparison = compare(a["image"], b["image"])
                if comparison["ok"]:
                    pairs.append({"original": a["row"], "sdl": b["row"], "resource_phase": a["phases"][phase],
                        "sdl_resource_phase": b["phases"][phase], "checked_pixels": 64000, "whole_frame": True, **comparison}
                    )
                    last_original, last_sdl = a["row"]["index"], b["row"]["index"]
                    used_original.add(a["pixel_sha256"]); used_sdl.add(b["pixel_sha256"])
                    used_poses.add(pose); used_sdl_poses.add(sdl_pose)
                    original_images.append(a["image"]); sdl_images.append(b["image"])
                    break
            if len(pairs) == minimum_pairs: break
        if len(pairs) == minimum_pairs: paired[phase] = {"pairs": pairs, "ordered": True}
        else: missing.append({"phase": phase, "original_identified": len(left), "sdl_identified": len(right),
            "matched_pairs": pairs, "required_pairs": minimum_pairs,
            "reason": "insufficient ordered/distinct full64000 pairs; conservative search remains pending"})
    return {"ok": not missing, "paired_phases": paired, "pending_phases": missing,
            "scope": "only listed actual same-phase full64000 pairs; no whole-animation/RNG/audio/timing equivalence",
            "source_sha256": resources["source_sha256"], "resource_sha256": resources["resource_sha256"],
            "normalization": "existing integer nearest scale only; raw PNG retained; no RGB remap or random masks",
            "whole_animation_equivalence": False, "requested_phase_order": list(required_phases),
            "minimum_pairs_per_phase": minimum_pairs,
            "frame_counts": {role: len(frames) for role, frames in series.items()}}
