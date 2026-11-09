"""G2 AI11静止恢复：原FDICON/field VOICE正证与完整SAV有限门。

13fd4只画实体SOLID200→normal，没有恢复浮字；25bf4在25c63加载
FDOTHER31到53eec，13fd4在14035选cue4。不读取SDL冷表推期望。
"""
from __future__ import annotations

import hashlib
import io
import math
from pathlib import Path
import struct

from PIL import Image
import numpy as np
from scipy import signal

import fd2_battle_animation_frames as frames
import run_original_battle_animation as battle
from fd2_g2_battle_oracle import checksum,FIELD_SHA256,SHAPE_SHA256
from fd2_sfx_oracle import references, read_recording, _match
from run_g2_item_check import check_cache
from run_original_comparison import normalize
from run_original_load import measuring_focus
from run_original_player import screen
from run_original_player import sheet_frame

SCENARIO = "ai-behavior11-recover"
PREFIX_FRAMES = 661
MIN_SCORE, IDENTITY_MARGIN = .8, .05


def sha(data):
    return hashlib.sha256(data).hexdigest()


def recovery_background(original,source,assets):
    """原地图完整草地156；不从截图取颜色、不忽略信息板遮盖的像素。"""
    field=(original/"FDFIELD.DAT").read_bytes();shape=(original/"FDSHAP.DAT").read_bytes()
    if sha(field)!=FIELD_SHA256 or sha(shape)!=SHAPE_SHA256:
        raise ValueError("恢复背景原地图／shape身份不符")
    if source[battle.META+2]!=0 or tuple(source[battle.META+3:battle.META+5])!=(5,11):
        raise ValueError("恢复背景门仅对应stage0/camera5,11")
    data=battle.archive_entry(field,0);metadata=battle.archive_entry(field,1)
    width,height=struct.unpack_from("<HH",data)
    tiles=battle.archive_entry(shape,metadata[0]*2);attrs=battle.archive_entry(shape,metadata[0]*2+1)
    world=(5,18);position=screen(source,world)
    if (width,height,metadata[0],position)!=(24,24,0,(4,172)):
        raise ValueError("恢复背景坐标／地图资源域不符")
    neighborhood=[]
    for y in range(17,20):
        for x in range(4,7):
            cell=struct.unpack_from("<I",data,4+4*(y*width+x))[0];tile=cell&1023
            attribute=attrs[tile*4:tile*4+4]
            if cell>>10 or attribute!=bytes((0,0,4,0)):
                raise ValueError("恢复背景及相邻格不能含前景／动态／事件标记")
            neighborhood.append({"world":[x,y],"cell":cell,"attributes_hex":attribute.hex()})
    for raw in range(source[battle.META+1]):
        unit=battle.oracle.actor(source,raw)
        if unit[5]&1:continue
        if unit[4] or unit[38]:
            raise ValueError("恢复背景排遮挡的夹具不能带步态／抖动位移")
        x,y=screen(source,tuple(unit[:2]));y-=6
        if x<position[0]+24 and x+24>position[0] and y<position[1]+24 and y+24>position[1]:
            raise ValueError("恢复完整背景格受到原可见actor遮挡")
    cell=struct.unpack_from("<I",data,4+4*(world[1]*width+world[0]))[0]
    image=sheet_frame(tiles,cell&1023)
    if image.size!=(24,24) or 0 in image.getdata() or any(224<=v<240 for v in image.getdata()):
        raise ValueError("恢复背景格必须原不透明且无水色周期像素")
    anchor=frames._anchor(image,Image.new("L",(24,24),255),position,
        {"resource":"FDSHAP.DAT stage0 dry opaque tile","world":list(world),"tile":cell&1023},assets["palette"])
    if anchor is None or anchor["checked_pixels"]!=576:
        raise ValueError("恢复背景门必须完整576像素")
    evidence={"world":list(world),"screen":list(position),"tile":cell&1023,"cell":cell,
        "attributes_hex":attrs[(cell&1023)*4:(cell&1023)*4+4].hex(),"checked_pixels":576,
        "map_entry_sha256":sha(data),"shape_tiles_sha256":sha(tiles),"shape_attributes_sha256":sha(attrs),
        "no_water_palette_or_actor":True,"neighborhood":neighborhood}
    return anchor,evidence


def prepare(original):
    original = Path(original)
    p = battle.prepare_runtime(original, SCENARIO)
    source, assets = p["source"], p["assets"]
    caster = battle.oracle.actor(source, 11)
    if caster[3] or caster[4] or caster[38] or caster[5]&0x80:
        raise ValueError("恢复像素门要求原方向0/无步态/无麻痹/未行动的精确夹具")
    # 原始指令锚：31→53eec；cue4→同owner。先验EXE身份由source_range完整核验。
    from fd2_item_list_oracle import EXE_SHA256
    from static_data.verify_unit_tables import source_range
    exe = (original/"FD2.EXE").read_bytes()
    if sha(exe) != EXE_SHA256:
        raise ValueError("恢复原EXE身份不符")
    bank_code = source_range(exe, 0x25C63, 30)
    cue_code = source_range(exe, 0x14033, 14)
    if bank_code[:8] != bytes.fromhex("6a1fff35ec3e0000") or cue_code[:10] != bytes.fromhex("6a016a04ff35ec3e0000"):
        raise ValueError("原31/53eec/cue4指令锚改变")
    icons = (original/"FDICON.B24").read_bytes()
    if struct.unpack_from("<HHH", icons) != (24,24,1680):
        raise ValueError("恢复FDICON目录不符")
    at = screen(source, tuple(caster[:2])); at = (at[0],at[1]-6)
    if not 4<=at[0]<=292 or not 4<=at[1]<=172:
        raise ValueError("恢复实体门必须使用完整24x24资源")
    normals, solids = [], []
    for phase in range(3):
        identity = caster[7]*12+phase
        lo = struct.unpack_from("<I",icons,6+identity*4)[0]
        hi = struct.unpack_from("<I",icons,10+identity*4)[0]
        if not 6+1680*4<=lo<hi<=len(icons):
            raise ValueError("恢复FDICON原帧越界")
        image, mask = frames.rle_material(struct.pack("<HH",24,24)+icons[lo:hi])
        normal = frames._anchor(image, mask, at,{"resource":"FDICON.B24","frame":identity},assets["palette"])
        solid = frames._anchor(Image.new("P",(24,24),456&255),mask,at,
            {"resource":"FDICON original opaque mask","frame":identity,"solid":200},assets["palette"])
        if normal is None or solid is None:
            raise ValueError("恢复实体资源缺少足够非黑正证")
        normals.append(normal); solids.append(solid)
    bank = battle.archive_entry((original/"FDOTHER.DAT").read_bytes(),31)
    from analyze_fd2_audio import archive_entries
    candidates = [{"bank":31,"sample":i} for i in range(len(archive_entries(bank)))]
    refs = references(original/"FDOTHER.DAT",candidates)
    if len(refs)!=13 or refs[4]["source_frames"]<PREFIX_FRAMES:
        raise ValueError("恢复field bank31读集/固定cue4前缀不符")
    background,background_evidence=recovery_background(original,source,assets)
    p["recovery_resources"] = {"normal":normals,"solid":solids,"background":background}
    p["recovery_plan"] = {"scenario":SCENARIO,"actor_raw":11,"actor_world":list(caster[:2]),
        "solid_color":200,"phase_order":["normal-before","solid200","normal-after"],
        "normal_frame_ids":[caster[7]*12+i for i in range(3)],
        "opaque_pixels":[a["checked_pixels"] for a in normals],"position":list(at),
        "required_background_on_solid_and_normal_after":background_evidence,
        "source_instruction_spans":[{"va":"0x25c63","length":len(bank_code),"sha256":sha(bank_code)},
            {"va":"0x14033","length":len(cue_code),"sha256":sha(cue_code)}],
        "sfx":{"bank":31,"cue":4,"candidate_count":len(refs),"prefix_frames":PREFIX_FRAMES,
            "prefix_policy":"each source first min(661,length); constant-DC candidates retained as non-identity references",
            "min_score":MIN_SCORE,"identity_margin":IDENTITY_MARGIN,
            "references":[{k:v for k,v in r.items() if k!="values"} for r in refs]},
        "whole_animation_or_audio_fidelity":False,"GUI_executed":False}
    return p


def visual_gate(rows, resources):
    stages = ["normal-before","solid200","normal-after"]
    found=[]; phase=0
    for row in rows:
        encoded=Path(row["path"]).read_bytes()
        if sha(encoded)!=row["sha256"]:
            raise AssertionError("恢复WGC原PNG身份改变")
        with Image.open(io.BytesIO(encoded)) as image:
            frame=normalize(image)
        family = resources["solid"] if phase==1 else resources["normal"]
        matches=[a for a in family if frames._anchor_match(frame,a)]
        background_ok=phase==0 or frames._anchor_match(frame,resources["background"])
        if matches and background_ok:
            found.append({"stage":stages[phase],"index":row["index"],"sha256":row["sha256"],
                "wgc_timestamp_100ns":row["wgc_timestamp_100ns"],"resource":matches[0]["identity"],
                "checked_pixels":matches[0]["checked_pixels"],
                "background_checked_pixels":0 if phase==0 else resources["background"]["checked_pixels"]})
            phase+=1
            if phase==3:break
    return {"ok":phase==3,"ordered_stages":found,"full_dynamic_RGB_equivalence":False}


def audio_gate(original, wav, expected_sha):
    wav=Path(wav)
    if sha(wav.read_bytes())!=expected_sha:
        raise AssertionError("恢复WAV与已封包采集身份不符")
    original=Path(original)
    from analyze_fd2_audio import archive_entries
    bank=archive_entries((original/"FDOTHER.DAT").read_bytes())[31]
    refs=references(original/"FDOTHER.DAT",[{"bank":31,"sample":i} for i in range(len(archive_entries(bank)))])
    meta,values=read_recording(wav)
    if meta["sha256"]!=expected_sha:
        raise AssertionError("恢复WAV分析期间身份改变")
    rate=meta["sample_rate"]; expected=refs[4]
    templates={}
    constant=[]
    for ref in refs:
        prefix=ref["values"][:PREFIX_FRAMES]
        if np.std(prefix)<=1e-6:
            constant.append({"sample":ref["sample"],"sha256":ref["sha256"],"reason":"constant DC is not AC identity"})
        else:
            divisor=math.gcd(rate,11025)
            templates[ref["sha256"]]=signal.resample_poly(prefix,rate//divisor,11025//divisor)
    template=templates[expected["sha256"]]
    hit=_match(values,template);lo=hit["offset_frames"];local=values[lo:lo+len(template)]
    competitors=[{"sha256":key,"score":_match(local,t)["score"]} for key,t in templates.items() if key!=expected["sha256"]]
    competitors.sort(key=lambda x:x["score"],reverse=True)
    second=competitors[0]["score"] if competitors else -1
    return {"ok":hit["score"]>=MIN_SCORE and hit["score"]-second>=IDENTITY_MARGIN,
        "expected_bank":31,"expected_cue":4,"VOICE_sha256":expected["sha256"],
        "score":hit["score"],"strongest_other_at_same_peak":competitors[0] if competitors else None,
        "onset_ms":lo*1000/rate,"search_scope":"entire closed recording; source prefix fixed before capture",
        "candidate_count":len(refs),"constant_references_retained":constant,
        "runtime_bank_pointer_or_exact_timing_proven":False,"full_audio_fidelity":False}


def save_gate(prepared, measured, engine):
    if engine not in ("original","sdl"):
        raise ValueError("恢复存档端未知")
    expected, detail=battle.oracle.project_final(prepared,measured)
    steps=[("LEFT",screen(expected,(6,14)))]
    expected=bytearray(expected)
    expected[battle.META+5:battle.META+9]=measuring_focus(expected,steps)
    cache=check_cache(prepared["source"],measured,engine)
    if not cache["ok"]:
        raise AssertionError("恢复Load cache不符合独立构造序列")
    for row in cache["rows"]:
        expected[battle.UNITS+row["raw"]*battle.STRIDE+2]=row["expected"]
    expected=checksum(expected)
    if expected!=measured:
        raise AssertionError("恢复完整22987B存档越出有限业务/原cache/显式Save导航写集")
    return {"ok":True,"bytes":len(measured),"cache":cache,"finite_effects":detail,
        "caster_HP":60,"caster_MP":8,"turn":2,"all_unknown_bytes_preserved":True}
