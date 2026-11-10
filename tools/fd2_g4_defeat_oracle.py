"""1db65败亡的有限原资源身份门；不启动窗口、输入或声音设备。

连续WGC由原runner核PID/HWND/时间戳及显式stop。本模块只核13旋转方向、
随后12张消散LMI和UI31 cue3固定前缀；不声称整页、精确时长/次数或A7等价。
原字节/FDICON/FDOTHER为独立输入，不导入SDL定义、实现或捕获终态。
"""
from __future__ import annotations

import hashlib
import io
import math
from pathlib import Path
import struct
import sys

from PIL import Image, ImageChops

import fd2_g2_battle_oracle as common
from fd2_battle_animation_frames import rle_material
from run_original_comparison import TOLERANCE, archive_entry, normalize
from run_original_player import acted_sprite, sheet_frame

sys.path.insert(0, str(Path(__file__).resolve().parent / 'static_data'))
from verify_unit_tables import source_range

META, UNITS, STRIDE = common.META, common.UNITS, common.STRIDE
FUNCTION_SHA = '434c25c5e2d79e14b2c6b4eb729001cde3b6d38e63a8a2a911a5dc452aff6806'
PREFIX = 661


def _digest(data):
    return hashlib.sha256(data).hexdigest()


def _anchor(image, mask, at, palette):
    # 68帧仅18个原opaque点，不能套通用24点门而丢掉合法首帧。
    # 所有原写点均检查；不挑捕获中碰巧匹配的点。
    if not mask.getbbox() or not 0 <= at[0] <= 320-image.width or not 0 <= at[1] <= 200-image.height:
        raise ValueError('败亡原资源锚为空或越出320x200有效域')
    if any(224 <= color < 240 for color, active in zip(image.getdata(), mask.getdata()) if active):
        raise ValueError('败亡身份门未建模动态水色资源')
    image.putpalette(palette)
    result = {'image': image.convert('RGB'), 'mask': mask, 'at': tuple(at),
              'checked_pixels': sum(value != 0 for value in mask.getdata())}
    result['guard'] = _anchor_guard(result)
    return result


def _anchor_guard(anchor):
    return _digest(anchor['image'].tobytes() + anchor['mask'].tobytes() +
                   struct.pack('<4i', *anchor['at'], *anchor['image'].size))


def _match(image, anchor):
    x, y = anchor['at']; width, height = anchor['image'].size
    difference = ImageChops.difference(image.crop((x, y, x+width, y+height)), anchor['image'])
    difference.paste((0, 0, 0), mask=ImageChops.invert(anchor['mask']))
    return max(high for _, high in difference.getextrema()) <= TOLERANCE


def sound_declaration(original):
    from analyze_fd2_audio import archive_entries
    from fd2_sfx_oracle import references
    other = Path(original) / 'FDOTHER.DAT'
    count = len(archive_entries(archive_entry(other.read_bytes(), 31)))
    if count != 13:
        raise ValueError('败亡UI31 bank不是已证13样本')
    candidates = [{'bank': 31, 'sample': sample} for sample in range(count)]
    refs = references(other, candidates)
    if refs[3]['source_frames'] < PREFIX:
        raise ValueError('败亡cue3短于预声明661帧前缀')
    return {'fdother_sha256': _digest(other.read_bytes()), 'candidates': candidates,
            'references': [{**{k: v for k, v in ref.items() if k != 'values'},
                            'comparison_prefix_frames': min(PREFIX, ref['source_frames']),
                            'constant_source_prefix': float((ref['values'][:PREFIX]-
                                ref['values'][:PREFIX].mean()).var()) <= 1e-12} for ref in refs],
            'expected_sample': 3, 'prefix_frames': PREFIX, 'prefix_ms': 60,
            'short_competitors': '全部原长；bank31样本0/7为160帧、9为390帧，不能伪造661帧',
            'minimum_score': .8, 'identity_margin': .05}


def resources(original, source, target_raw=11):
    """source须是确认目标时的明文SAV，含实际camera；common39传p.target_view。"""
    original = Path(original); source = common.valid_plain(bytes(source))
    exe = (original / 'FD2.EXE').read_bytes()
    if _digest(exe) != common.EXE_SHA256 or _digest(source_range(exe, 0x1db65, 857)) != FUNCTION_SHA:
        raise ValueError('败亡完整原函数或EXE身份不同')
    if type(target_raw) is not int or not 0 <= target_raw < source[META+1]:
        raise ValueError('败亡目标raw不在active范围')
    unit = bytes(source[UNITS+target_raw*STRIDE:UNITS+(target_raw+1)*STRIDE])
    camera = tuple(source[META+3:META+5]); world = tuple(unit[:2])
    if unit[5] & 1 or unit[3] > 3 or unit[4] or unit[0x26] or unit[7] >= 140:
        raise ValueError('败亡资源门只覆盖非hidden静止、无抖动且FDICON有定义的目标')
    rel = tuple(a-b for a, b in zip(world, camera))
    sprite_at = (4+rel[0]*24, -2+rel[1]*24)
    fx_at = (4+(rel[0]-1)*24, -2+(rel[1]-1)*24)
    field = (original / 'FDFIELD.DAT').read_bytes(); shapes = (original / 'FDSHAP.DAT').read_bytes()
    if _digest(field) != common.FIELD_SHA256 or _digest(shapes) != common.SHAPE_SHA256:
        raise ValueError('败亡地图/地形来源身份不同')
    stage = source[META+2]; grid = archive_entry(field, stage*2)
    width, height = struct.unpack_from('<HH', grid)
    metadata = archive_entry(field, stage*2+1); attrs = archive_entry(shapes, metadata[0]*2+1)
    cells = []
    for x, y in (world, (world[0], world[1]-1)):
        if not 0 <= x < width or not 0 <= y < height:
            raise ValueError('败亡目标遮挡审计格越界')
        cell = struct.unpack_from('<I', grid, 4+(y*width+x)*4)[0]
        attr = attrs[(cell & 1023)*4:(cell & 1023)*4+4]
        if len(attr) != 4 or attr[0] & 128:
            raise ValueError('败亡目标含前景遮挡；该最小资源身份门没有删去被遮点')
        cells.append({'world': [x, y], 'cell': cell, 'attr': list(attr)})
    other = (original / 'FDOTHER.DAT').read_bytes(); icon = (original / 'FDICON.B24').read_bytes()
    palette6 = archive_entry(other, 0)
    if len(palette6) != 768 or max(palette6) > 63 or struct.unpack_from('<HHH', icon) != (24, 24, 1680):
        raise ValueError('败亡palette/FDICON目录与已证原格式不同')
    palette = [value*4 for value in palette6]; rotation = []
    for phase in range(3):
        for direction in range(4):
            frame = unit[7]*12+direction*3+phase
            start = struct.unpack_from('<I', icon, 6+frame*4)[0]
            end = struct.unpack_from('<I', icon, 10+frame*4)[0] if frame < 1679 else len(icon)
            if not 6+1680*4 <= start < end <= len(icon):
                raise ValueError('败亡FDICON资源帧边界错误')
            image, mask = rle_material(struct.pack('<HH', 24, 24)+icon[start:end])
            if unit[5] & 128: image = acted_sprite(image)
            rotation.append({'phase': phase, 'direction': direction, 'frame': frame,
                             'anchor': _anchor(image, mask, sprite_at, palette)})
    sheet = archive_entry(other, 5); dissolve = []
    for frame in range(68, 80):
        image = sheet_frame(sheet, frame, lmi=True)
        mask = image.point(lambda value: 255 if value else 0, mode='L')
        dissolve.append({'frame': frame, 'anchor': _anchor(image, mask, fx_at, palette)})
    contract = {'function_va': '0x1db65', 'raw_unrelocated_function_sha256': FUNCTION_SHA,
                'source_plain_sha256': _digest(source), 'target_raw': target_raw, 'unit_id': unit[7],
                'target_record_hex': unit.hex(), 'camera': list(camera), 'world': list(world),
                'sprite_at': list(sprite_at), 'dissolve_at': list(fx_at), 'foreground_cells': cells,
                'sources': {name: _digest(data) for name, data in (
                    ('FD2.EXE', exe), ('FDOTHER.DAT', other), ('FDICON.B24', icon),
                    ('FDFIELD.DAT', field), ('FDSHAP.DAT', shapes))},
                'directions': [j % 4 for j in range(13)], 'dissolve_frames': list(range(68, 80)),
                'raw_idle_to_effective_sprite_phase': [0, 1, 2, 1],
                'raw_idle_dword_measured': False,
                'transparency': 'FDICON原RLE SKIP；LMI仅原color0，不删其它写点',
                'whole_frame_or_exact_timing_claimed': False}
    return {'contract': contract, 'rotation': rotation, 'dissolve': dissolve,
            'sound': sound_declaration(original)}


def check_visual(rows, prepared):
    """rows消费series_index核验后的原PNG；保持原顺序，不裁图、不补造丢帧。"""
    if [(r['phase'], r['direction']) for r in prepared['rotation']] != [(p, d) for p in range(3) for d in range(4)] or \
            [r['frame'] for r in prepared['dissolve']] != list(range(68, 80)):
        raise ValueError('败亡正锚集合不完整')
    for rule in prepared['rotation'] + prepared['dissolve']:
        anchor = rule['anchor']
        if _anchor_guard(anchor) != anchor['guard']:
            raise ValueError('败亡资源身份门不得修改原opaque写集/原锚')
    classified = []; last = -1
    for row in rows:
        if type(row['index']) is not int or row['index'] <= last:
            raise ValueError('败亡WGC帧index不是原严格顺序')
        last = row['index']; raw = Path(row['path']).read_bytes()
        if _digest(raw) != row['sha256']:
            raise ValueError('败亡WGC原PNG身份变化')
        with Image.open(io.BytesIO(raw)) as image: image = normalize(image)
        rotations = [(r['phase'], r['direction']) for r in prepared['rotation'] if _match(image, r['anchor'])]
        effects = [r['frame'] for r in prepared['dissolve'] if _match(image, r['anchor'])]
        if rotations or effects:
            classified.append({k: row[k] for k in ('index', 'path', 'sha256')} |
                              {'rotation': rotations, 'dissolve': effects})
    # 每个有效sprite帧类独立消费；原idle DWORD=3和1均映射同类1。
    # 图像不能区分这两个原值。重复采样不充作下一方向。
    attempts = []
    for phase in range(3):
        spin = []; effect = []
        for row in classified:
            if len(spin) < 13:
                directions = [d for p, d in row['rotation'] if p == phase]
                if directions == [len(spin) % 4]: spin.append(row)
            elif len(effect) < 12 and row['index'] > spin[-1]['index'] and row['dissolve'] == [68+len(effect)]:
                effect.append(row)
        attempts.append({'phase': phase, 'rotation': spin, 'dissolve': effect})
    best = max(attempts, key=lambda r: len(r['rotation'])+len(r['dissolve']))
    ok = len(best['rotation']) == 13 and len(best['dissolve']) == 12
    return {'ok': ok, 'contract': prepared['contract'], 'effective_sprite_phase': best['phase'] if ok else None,
            'raw_idle_dword_measured': False,
            'rotation_representatives': best['rotation'], 'dissolve_representatives': best['dissolve'],
            'missing_rotation_steps': list(range(len(best['rotation']), 13)),
            'missing_dissolve_frames': list(range(68+len(best['dissolve']), 80)),
            'classified': classified, 'scope': '13方向/12LMI有序原资源代表，非全页、精确时序或相同页历史证明'}


def check_sound(original, wav):
    """UI31全13候选；cue3固定661帧前缀，其它短样本按全部原长比较。"""
    from scipy import signal
    from fd2_sfx_oracle import references, read_recording, _match as match_sound
    declaration = sound_declaration(original)
    metadata, values = read_recording(Path(wav))
    refs = references(Path(original) / 'FDOTHER.DAT', declaration['candidates'])
    divisor = math.gcd(metadata['sample_rate'], 11025)
    templates = {ref['sha256']: signal.resample_poly(ref['values'][:PREFIX],
                 metadata['sample_rate']//divisor, 11025//divisor) for ref in refs}
    expected = refs[3]; template = templates[expected['sha256']]
    hit = match_sound(values, template)
    local = values[hit['offset_frames']:hit['offset_frames']+len(template)]
    competitors = [{'sha256': key, 'prefix_frames': min(PREFIX, next(
                      ref['source_frames'] for ref in refs if ref['sha256'] == key)),
                    # bank31 cue1前661点为原静音；它不是非零cue3的身份反候选，
                    # 明确记NCC0而不删除候选或按录音临时平移原模板。
                    'constant_source_prefix': float(candidate.var()) <= 1e-12,
                    'score': 0. if float(candidate.var()) <= 1e-12 else match_sound(local, candidate)['score']}
                   for key, candidate in templates.items() if key != expected['sha256']]
    second = max((row['score'] for row in competitors), default=-1.)
    return {'ok': hit['score'] >= .8 and hit['score']-second >= .05,
            'wav': metadata, 'expected_bank': 31, 'expected_sample': 3,
            'voice_sha256': expected['sha256'], 'score': hit['score'], 'margin': hit['score']-second,
            'onset_frame': hit['offset_frames'], 'prefix_frames': PREFIX,
            'full_bank_candidates': declaration['references'], 'competitors': competitors,
            'exact_cue_count_latency_A7_or_runtime_bank_pointer_claimed': False}
