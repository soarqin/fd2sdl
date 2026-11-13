"""S2真stage20特殊格→WORD47／四pan与17aa9等待的受控来源及图像门。

仅声明原190ac入口与SDL显式source机制；WAIT恢复/acted后缀分列。
读原map60/metadata61/shape20，不在stage0构造越界pan或写原资源。
"""
from __future__ import annotations
from contextlib import contextmanager
from pathlib import Path
import struct

import fd2_natural_oracle as natural
from dump_fd2_fixup_table import collect_internal_offset_fixups
from run_g4_runtime_check import encode
from run_graphics_tests import decode_save
from run_original_comparison import archive_entry
from run_original_end_turn import META, UNITS, STRIDE, round_assets
from run_original_player import exact_tile, sheet_frame
from static_data.audit_stage_data import SOURCE_HASHES
from s2_result_native_fixture import START_SHA, digest

SCENARIO="stage20-cell47-bios-ticks"
POSES=((3,15),(0,0),(28,0),(28,32),(0,32))


def derive(source,metadata):
    if len(source)!=0x59cb or sum(source[:-4])!=int.from_bytes(source[-4:],"little") or source[META+1:META+3]!=bytes((12,0)):
        raise ValueError("ticks代表要求固定完整START源域")
    if len(metadata)!=0x8a3 or metadata[:3]!=bytes((20,16,80)):raise ValueError("原stage20完整metadata不同")
    if source[UNITS+0x1a] or struct.unpack_from("<HH",source,UNITS+0x40)!=(42,42):
        raise ValueError("零步WAIT的raw0必须满HP/恢复tag0，不猜恢复前缀")
    plain=bytearray(source);plain[:len(metadata)]=metadata;plain[2]=0
    struct.pack_into("<BH",plain,0x53+2*3,2,47)
    donor=source[UNITS+4*STRIDE:UNITS+5*STRIDE]
    for raw in range(12,18):plain[UNITS+raw*STRIDE:UNITS+(raw+1)*STRIDE]=donor
    for raw in range(18):
        at=UNITS+raw*STRIDE
        if raw in (0,1):plain[at+5]&=~0x85
        elif raw in (16,17):
            plain[at:at+2]=bytes((40,39));plain[at+5]=(plain[at+5]|4)&~1
        else:plain[at+5]|=5
    plain[UNITS:UNITS+2]=bytes((7,19))
    plain[META:META+9]=bytes((1,18,20,3,15,7,19,4,4))
    plain[0x30a3+2]=0;plain[-4:]=sum(plain[:-4]).to_bytes(4,"little")
    changes=[{"offset":i,"before":a,"after":b} for i,(a,b) in enumerate(zip(source,plain)) if a!=b]
    allowed=set(range(0x8a3))|set(range(UNITS,UNITS+18*80))|set(range(META,META+9))|{0x30a3+2}|set(range(len(source)-4,len(source)))
    if any(r["offset"] not in allowed for r in changes):raise ValueError("ticks夹具越出声明metadata/active/meta/checksum写集")
    manifest={"scenario":SCENARIO,"controlled_fixture":True,"natural_state_claimed":False,"production_binding_added":False,
        "source_plain_sha256":digest(source),"derived_plain_sha256":digest(plain),"plaintext_patches":changes,
        "metadata":{"entry":61,"source_length":len(metadata),"source_sha256":digest(metadata),"effective_template_count":0},
        "record_copies":[{"raw":i,"source_raw":4,"source_record_sha256":digest(donor)} for i in range(12,18)],
        "special_cell":{"entry":60,"world":[7,19],"raw_cell":0x200fc,"terrain_flags":32,"action_index":2,
                        "original_descriptor":[1,20000],"controlled_descriptor":[2,47]},
        "original_dispatch":"18d8c zero-step WAIT ->190ac YES ->19511 full WORD47 ->35468",
        "SDL_dispatch":"nonshipping source-only Game common.callback.047; caller UI/restoration skipped",
        "caller_prefix":"raw0 HP42/max42 and recoverytag0: stationary recover has no HP write; original190ac does not mark actor before callback",
        "caller_suffix":"original18d8c marks raw0 acted after190ac returns; SDL explicit source flow does not, compared separately",
        "protected_raw":[16,17],"protected_flags":"visible, AI-ineligible, positiveHP; raw1 remains unacted to prevent automatic END",
        "source_goals":[1,8,8,8,8],"source_pans":[list(p) for p in POSES[1:]],
        "finite_wait_scope":"CPU/C proves signedWORD/goal; native proves ordered feedback/finite completion, not BIOS phase or absolute timing",
        "unknown_regions":"full roster/manual slots/unused active after18 and other meta bytes preserved"}
    return bytes(plain),encode(plain),manifest


@contextmanager
def resource_context(original,plain):
    if plain[META+1:META+3]!=bytes((18,20)):raise ValueError("ticks资源context只限stage20/18raw")
    field=(Path(original)/"FDFIELD.DAT").read_bytes()
    if digest(field)!=SOURCE_HASHES["FDFIELD.DAT"]:raise ValueError("原FDFIELD身份不同")
    data=archive_entry(field,60)
    if struct.unpack_from("<HH",data)!=(41,40) or len(data)!=4+41*40*4:raise ValueError("原stage20完整41x40地图不同")
    before=dict(natural.STAGE_DIMENSIONS)
    try:
        natural.STAGE_DIMENSIONS[20]=(41,40)
        yield {"entry":60,"width":41,"height":40,"length":len(data),"sha256":digest(data),"pure_resource_context_only":True}
    finally:natural.STAGE_DIMENSIONS.clear();natural.STAGE_DIMENSIONS.update(before)


def pose_resources(original,plain,assets):
    field=(original/"FDFIELD.DAT").read_bytes();shape=(original/"FDSHAP.DAT").read_bytes()
    data=archive_entry(field,60);metadata=archive_entry(field,61)
    tiles,attrs=archive_entry(shape,metadata[0]*2),archive_entry(shape,metadata[0]*2+1)
    candidates=[]
    for ry in range(2,6):
        for rx in range(2,11):
            images=[];ids=[]
            for cx,cy in POSES:
                x,y=cx+rx,cy+ry
                if any(not plain[UNITS+i*80+5]&1 and plain[UNITS+i*80]==x and abs(plain[UNITS+i*80+1]-y)<=1 for i in range(18)):break
                cell=struct.unpack_from("<I",data,4+(y*41+x)*4)[0];tile=cell&1023
                if attrs[4*tile]:break
                image=sheet_frame(tiles,tile)
                if 0 in image.getdata() or any(224<=p<240 for p in image.getdata()):break
                image.putpalette(assets["palette"]);images.append(image.convert("RGB"));ids.append(tile)
            else:candidates.append({"relative":[rx,ry],"position":[4+rx*24,4+ry*24],"tiles":ids,"images":images})
    if len(candidates)<3:raise ValueError("四pan没有足够完整无动画/水色/actor的原资源锚")
    pairs={(a,b) for a in range(5) for b in range(a+1,5)};coverage={pair:0 for pair in pairs};chosen=[]
    for row in candidates:
        row["distinguishes"]={pair for pair in pairs if sum(max(abs(x-y) for x,y in zip(p,q))>3
                for p,q in zip(row["images"][pair[0]].getdata(),row["images"][pair[1]].getdata()))>=24}
    while min(coverage.values())<2:
        remaining=[r for r in candidates if r not in chosen]
        if not remaining:raise ValueError("纯资源锚不足以用双见证区分全部camera")
        row=max(remaining,key=lambda r:sum(coverage[p]<2 for p in r["distinguishes"]))
        if not any(coverage[p]<2 for p in row["distinguishes"]):raise ValueError("camera资源并非可区分")
        chosen.append(row)
        for p in row["distinguishes"]:coverage[p]+=1
    return [{"camera":list(camera),"anchors":[{"position":r["position"],"tile":r["tiles"][i],"image":r["images"][i]} for r in chosen]}
            for i,camera in enumerate(POSES)]


def pose_match(frame,resources,index):
    pose=resources[index];matches=[]
    for anchor in pose["anchors"]:
        found=exact_tile(frame,anchor["image"],anchor["position"])
        if found is None:return None
        matches.append({"tile":anchor["tile"],"position":anchor["position"],**found})
    return {"kind":"source_camera_resource_pose","camera":pose["camera"],"index":index,"full_tile_anchors":matches}


def prepare(original):
    original=Path(original).resolve(strict=True)
    for name,sha in SOURCE_HASHES.items():
        if digest((original/name).read_bytes())!=sha:raise ValueError("原资源身份不同："+name)
    if digest((original/"START.SAV").read_bytes())!=START_SHA:raise ValueError("原START身份不同")
    field=(original/"FDFIELD.DAT").read_bytes();metadata=archive_entry(field,61);data=archive_entry(field,60)
    attrs=archive_entry((original/"FDSHAP.DAT").read_bytes(),metadata[0]*2+1)
    cell=struct.unpack_from("<I",data,4+(19*41+7)*4)[0]
    if cell!=0x200fc or attrs[(cell&1023)*4]!=32 or metadata[0x53+6:0x53+9]!=bytes((1,32,78)):
        raise ValueError("stage20真特殊格/0-based action2来源不同")
    pointer=collect_internal_offset_fixups(original/"FD2.EXE").get(0x51b91+47*4)
    if pointer is None or pointer[2]!=0x35468:raise ValueError("原callback47代码fixup不同")
    source=decode_save(original/"START.SAV");plain,encoded,manifest=derive(source,metadata);assets=round_assets(original)
    with resource_context(original,plain) as dimensions:
        resources=pose_resources(original,plain,assets)
        summary=natural.selection_summary(original,plain,assets,0)
        probes=natural.range_probe(original,plain,assets,0)
    manifest.update(map_source=dimensions,pose_anchor_count=len(resources[0]["anchors"]),headless_range_worlds=[r["world"] for r in probes])
    return {"original":original,"source":source,"plain":plain,"encoded":encoded,"manifest":manifest,
            "assets":assets,"poses":resources,"summary":summary,"range_probes":probes}
