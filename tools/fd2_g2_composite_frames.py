"""G2 34原始WGC的有限资源顺序门；不启动GUI、不改原件。

复用B6的RLE写点、FIGANI解析及SHA绑定PNG读取原语；不调用其只支持
0..9/13的整动作分类器。原2d80d/1c4cc/1c2da/1df58决定资源与顺序。
本门只证明列出的实际资源阶段，既不比较整段RGB，也不验证声音或同轨RNG。
"""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys

from PIL import Image

import fd2_battle_animation_frames as frames
import fd2_g2_battle_oracle as oracle
from run_original_battle_animation import series_index
from run_original_player import screen

sys.path.insert(0,str(Path(__file__).resolve().parent/"static_data"))
from verify_unit_tables import source_range

PHASES=("intro157","special67","fx17","flash17","popup17",
        "fx18","flash18","popup18","fx19","flash19","popup19")


def resources(original,source):
    original=Path(original)
    _,confirmed,_=oracle.derive_fixture(original,"composite34")
    if source!=confirmed:
        raise ValueError("34资源顺序门只接受已登记完整夹具")
    used=("FD2.EXE","FIGANI.DAT","FDOTHER.DAT","FDICON.B24","FDFIELD.DAT","FDSHAP.DAT","FDTXT.DAT")
    payload={name:(original/name).read_bytes() for name in used}
    hashes={name:oracle.digest(data) for name,data in payload.items()}
    other=payload["FDOTHER.DAT"];palette=[p<<2 for p in frames.archive_entry(other,0)]
    caster=oracle.actor(source,1);targets=[oracle.actor(source,raw) for raw in (0,2)]
    positions=[tuple(v-6 if axis==1 else v for axis,v in enumerate(screen(source,tuple(t[:2])))) for t in targets]
    rules={phase:[] for phase in PHASES}
    def add(phase,variant,*anchors):
        if all(anchor is not None for anchor in anchors):
            rules[phase].append({"variant":variant,"anchors":anchors})
    parts,header=frames.figani_parts(payload["FIGANI.DAT"],157)
    idle,_=frames.figani_parts(payload["FIGANI.DAT"],156)
    title=frames._title(other,payload["FDTXT.DAT"],caster,palette)
    if header["intro"]!=18 or header["empty"] or header["split"]:
        raise ValueError("34 caster固定slot1的已审intro域不同")
    for part in parts[:header["intro"]]:
        if frames._distinct_part(part,idle,palette):
            add("intro157",part["index"],title,frames._part_anchor(part,palette,
                {"resource":"FIGANI","entry":157,"part":part["index"]}))
    special,special_header=frames.figani_parts(other,67)
    if len(special)!=9:
        raise ValueError("34专用FDOTHER67实际part域不同")
    for part in special[1:]:
        add("special67",part["index"],frames._part_anchor(part,palette,
            {"resource":"FDOTHER","entry":67,"part":part["index"]}))
    # 已登记源的静态干地块；明确验证无actor、前景、零色和水色，不从截图抄底图。
    field=frames.archive_entry(payload["FDFIELD.DAT"],0)
    metadata=frames.archive_entry(payload["FDFIELD.DAT"],1)
    shapes=payload["FDSHAP.DAT"]
    attrs=frames.archive_entry(shapes,metadata[0]*2+1)
    cell=struct.unpack_from("<I",field,4+(12*24+6)*4)[0];tile=cell&1023
    dry=frames.sheet_frame(frames.archive_entry(shapes,metadata[0]*2),tile)
    if cell>>10 or attrs[tile*4] or any(p==0 or 224<=p<240 for p in dry.getdata()) or any(
            tuple(oracle.actor(source,i)[:2]) in ((6,12),(6,13)) and not oracle.actor(source,i)[5]&1
            for i in range(source[oracle.META+1])):
        raise ValueError("34静态地形正锚的源前置不符")
    dry_at=screen(source,(6,12))
    dry_anchor=frames._anchor(dry,Image.new("L",(24,24),255),dry_at,
        {"resource":"FDSHAP","tile":tile,"world":[6,12]},palette)
    def compound(images,locations,identity,*,include_dry=False):
        image=Image.new("P",(320,200));mask=Image.new("L",(320,200))
        if include_dry:
            image.paste(dry,dry_at);mask.paste(255,(*dry_at,dry_at[0]+24,dry_at[1]+24))
        for part,at in zip(images,locations):
            opaque=part.point(lambda p:255 if p else 0,mode="L")
            image.paste(part,at,opaque);mask.paste(255,at,opaque)
        return frames._anchor(image,mask,(0,0),identity,palette)
    lmi=frames.archive_entry(other,6);glyphs=frames.archive_entry(other,5)
    curve=source_range(payload["FD2.EXE"],0x5202c,26)
    tints=source_range(payload["FD2.EXE"],0x51f15,30)
    # 完整Domain4e127反证：传入的arg_C未读；arg_8既作stride也装进AH作常色。
    # 1c2da确传stride456，故实际色200；不能把caller表值当实色或从捕获拟合。
    stride_call=source_range(payload["FD2.EXE"],0x1c350,5)
    callee=source_range(payload["FD2.EXE"],0x4e133,13)
    if stride_call!=bytes.fromhex("68c8010000") or callee!=bytes.fromhex("8b551083ea1833c98b45108ae0"):
        raise ValueError("34 SOLID实际stride低byte颜色原指令不同")
    actual_tint=struct.unpack_from("<I",stride_call,1)[0]&255
    for core in (17,18,19):
        start=source_range(payload["FD2.EXE"],0x51f33+core,1)[0]
        count=source_range(payload["FD2.EXE"],0x51f54+core,1)[0]
        for part in range(count):
            material=frames.sheet_frame(lmi,start+part,lmi=True)
            add("fx"+str(core),part,dry_anchor,compound([material]*2,positions,
                {"resource":"FDOTHER6","frame":start+part,"target_raws":[0,2]}))
        for phase in range(3):
            images=[frames.sheet_frame(payload["FDICON.B24"],t[7]*12+phase).point(
                lambda p: actual_tint if p else 0) for t in targets]
            add("flash"+str(core),phase,dry_anchor,compound(images,positions,
                {"resource":"FDICON+4e127 stride-low-byte","core":core,"color":actual_tint,
                 "unused_caller_table_argument":tints[core],"phase":phase,"target_raws":[0,2]}))
        values=[(15*struct.unpack_from("<H",t,72 if core==17 else 74)[0]+100)//100
                if core!=19 else 15 for t in targets]
        for frame in range(22):
            images=[];locations=[]
            for at,value in zip(positions,values):
                digits=str(value);next_digit=0
                for slot in range(4):
                    if len(digits)>3-slot:
                        images.append(frames.sheet_frame(glyphs,105+int(digits[next_digit]),lmi=True));next_digit+=1
                        locations.append((at[0]+slot*5+2,at[1]+6+curve[frame+slot]-3))
            # 短数字本身可少于24像素；连同独立完整地块组成源正锚，仍检查两目标全部数字写点。
            add("popup"+str(core),frame,compound(images,locations,
                {"resource":"FDOTHER5 green glyphs+5202c","values":values,"curve_frame":frame,"target_raws":[0,2]},include_dry=True))
    if any(not value for value in rules.values()):
        raise ValueError("34缺少有效资源正锚，不能按帧数补阶段")
    return {"rules":rules,"source_sha256":oracle.digest(source),"resource_sha256":hashes,
        "solid_tints":{str(i):actual_tint for i in (17,18,19)},
        "unused_caller_table_arguments":{str(i):tints[i] for i in (17,18,19)},
        "casting_header":header,"special_header":special_header}


def classify(frame,prepared):
    found={}
    for phase,rules in prepared["rules"].items():
        for rule in rules:
            if all(frames._anchor_match(frame,a) for a in rule["anchors"]):
                found[phase]={"variant":rule["variant"],"anchors":[{k:a[k] for k in
                    ("identity","at","checked_pixels","nonblack_pixels","transparency")} for a in rule["anchors"]]}
                break
    return found


def ordered_witnesses(identified,phases=PHASES):
    chosen=[];previous=-1
    for phase in phases:
        witness=next((row for row in identified if row["row"]["index"]>previous and phase in row["phases"]),None)
        if witness is None:
            return {"ok":False,"witnesses":chosen,"missing_phase":phase,
                "unobserved_after_index":previous}
        previous=witness["row"]["index"]
        chosen.append({"phase":phase,"frame":witness["row"],"resource":witness["phases"][phase]})
    return {"ok":True,"witnesses":chosen}


def verify(original,evidence):
    original,evidence=Path(original).resolve(),Path(evidence).resolve()
    report=json.loads((evidence/"comparison.json").read_text(encoding="utf-8"))
    if (report.get("ok") is not True or report.get("scenario")!="composite34" or
            report.get("final_sources_unchanged") is not True or report.get("display_requirement",{}).get("released") is not True):
        raise ValueError("34原件缺完整业务／来源／显示释放收据")
    encoded,source,_=oracle.derive_fixture(original,"composite34")
    if (evidence/"controlled.SAV").read_bytes()!=encoded:
        raise ValueError("34原件controlled.SAV不等于独立已登记源")
    prepared=resources(original,source)
    for name,digest in prepared["resource_sha256"].items():
        if report.get("original_before",{}).get(name,{}).get("sha256")!=digest:
            raise ValueError("34当前资源不是实际采集时冻结原源："+name)
    cases={case["kind"]:case for case in report["cases"]}
    if len(report["cases"])!=2 or set(cases)!={"original","sdl"}:
        raise ValueError("34需唯一original与sdl两个端点")
    results={}
    for kind,case in cases.items():
        if case.get("ok") is not True or any(case.get("guards",{}).get(key) is not True
                for key in ("sources_unchanged","save_matches_completed","cleanup")):
            raise ValueError("34端点守卫未通过")
        owned=case.get("processes",[])
        if len(owned)!=1 or owned[0].get("cleaned") is not True or owned[0].get("forced_cleanup") is not False or owned[0].get("exit_code")!=0:
            raise ValueError("34游戏没有唯一owned正常退出收据")
        collection=case["continuous_wgc"];helper=case["series_helper"]
        if helper.get("forced_cleanup") or not helper.get("cleaned") or helper.get("exit_code")!=0:
            raise ValueError("34连续采集helper没有正常清理")
        job=helper.get("job",{})
        if job.get("cleaned")!=1 or any(job.get(key)!=0 for key in ("worker_exit","timed_out","residual","forced","result")):
            raise ValueError("34连续采集helper嵌套Job未完整正常收尾")
        bound=series_index(evidence/kind/"action-series",collection["summary"],collection["ready"],case["processes"][0]["pid"])
        if bound!=collection["frames"]:
            raise ValueError("34实际series索引与封存报告不一致")
        loaded=frames._read_frames(bound)
        identified=[{"row":item["row"],"phases":classify(item["image"],prepared)} for item in loaded]
        counts={phase:sum(phase in row["phases"] for row in identified) for phase in PHASES}
        results[kind]={**ordered_witnesses(identified),"identified_counts":counts,
            "all_classified_frames":[row for row in identified if row["phases"]]}
    return {"ok":all(case["ok"] for case in results.values()),"scenario":"composite34",
        "scope":"ordered original-resource witnesses only; no whole-frame pair or full animation/audio/timing equivalence",
        "whole_animation_RGB_equivalence":False,"audio_verified":False,"rng_stream_equivalence":False,
        "required_order":list(PHASES),"source_sha256":prepared["source_sha256"],
        "resource_sha256":prepared["resource_sha256"],"original_solid_tints":prepared["solid_tints"],
        "evidence":str(evidence),"checker_sha256":hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),"cases":results}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--original",type=Path,required=True)
    parser.add_argument("--evidence",type=Path,required=True)
    parser.add_argument("--json",type=Path,required=True)
    args=parser.parse_args()
    destination=args.json.resolve();original=args.original.resolve()
    if destination.exists() or destination==original or destination.is_relative_to(original):
        parser.error("输出必须是原版目录以外的尚不存在文件；保留旧证据")
    result=verify(original,args.evidence)
    destination.parent.mkdir(parents=True,exist_ok=True)
    destination.write_text(json.dumps(result,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    print(json.dumps({"ok":result["ok"],"cases":{k:{"ok":v["ok"],"missing_phase":v.get("missing_phase"),
        "identified_counts":v["identified_counts"]} for k,v in result["cases"].items()}},ensure_ascii=False))
    return 0 if result["ok"] else 1


if __name__=="__main__":
    raise SystemExit(main())
