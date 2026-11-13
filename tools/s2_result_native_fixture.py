"""S2 stage29/20bf5受控SAV与只读原版/PDB结果owner观察；不启动进程。

原START记录／未知尾保留，metadata仅引用真实stage29原资源前缀。
读取沿S0已核双原码锚/fixup/DS及OwnedRPC/CDB，不扩RPC写内存能力。
"""
from __future__ import annotations

import hashlib
from contextlib import contextmanager
from pathlib import Path
import struct

import fd2_s0_native as s0
from audit_g4_render_source import archive_entries, text_fragments
from run_g4_runtime_check import encode
from run_g4_common_event_check import story_speaker
from run_graphics_tests import decode_save
from run_original_end_turn import META, UNITS, STRIDE
from static_data.audit_stage_data import SOURCE_HASHES

SCENARIO = "stage29-story7-post-write"
START_SHA = "2c89019b2a83f5460354597e15e860c075951241beb5c9a3dcb6551bb7ca5310"


def digest(data): return hashlib.sha256(data).hexdigest()


def derive(source, metadata):
    if len(source)!=0x59cb or sum(source[:-4])!=int.from_bytes(source[-4:],"little"):
        raise ValueError("原SAV长度/checksum不同")
    if source[META+2]!=0 or source[META+1]!=12 or len(metadata)!=0x83+70*26 or metadata[:3]!=bytes((29,20,70)):
        raise ValueError("S2夹具要求固定START与真实stage29 metadata源域")
    plain=bytearray(source);plain[:len(metadata)]=metadata
    # 原stage29地图(10,12)的东邻(11,12)有完整干燥range正门；
    # 仅本受控fixture显式移动raw0，不将stage0坐标当新关默认或自然可达。
    plain[UNITS:UNITS+2]=bytes((10,12))
    donor=source[UNITS+11*STRIDE:UNITS+12*STRIDE]
    for raw in range(12,21):plain[UNITS+raw*STRIDE:UNITS+(raw+1)*STRIDE]=donor
    for raw in range(21):
        at=UNITS+raw*STRIDE+5
        plain[at]=plain[at]&~1 if raw in (0,20) else plain[at]|1
    plain[META+1]=21;plain[META+2]=29
    speaker=story_speaker(plain,9)
    if speaker!={"unit_id":9,"raw_actor":1,"source":"active","visible":False}:
        raise ValueError("story7必须解析真实raw1/text9/unit9，不能用未定义说话人")
    plain[-4:]=sum(plain[:-4]).to_bytes(4,"little")
    changes=[{"offset":i,"before":a,"after":b} for i,(a,b) in enumerate(zip(source,plain)) if a!=b]
    allowed=set(range(len(metadata)))|set(range(UNITS,UNITS+21*STRIDE))|{META+1,META+2}|set(range(len(source)-4,len(source)))
    if any(row["offset"] not in allowed for row in changes):raise ValueError("S2夹具越出声明metadata/active/count/stage/checksum写集")
    return bytes(plain),encode(plain),{"scenario":SCENARIO,"controlled_fixture":True,"natural_state_claimed":False,
        "original_result_target":"0x20bf5","original_result_slot":29,"source_flow":"common.result.source_020bf5",
        "production_binding_added":False,"source_plain_sha256":digest(source),"derived_plain_sha256":digest(plain),
        "plaintext_patches":changes,"metadata":{"entry":88,"length":len(metadata),"sha256":digest(metadata)},
        "record_copies":[{"raw":i,"source_raw":11,"source_record_sha256":digest(donor)} for i in range(12,21)],
        "active_count":21,"visible_raw":[0,20],"hidden_raw":list(range(1,20)),"speaker":speaker,
        "controller_position":{"raw":0,"world":[10,12],"reason":"explicit controlled placement; original stage29 dry east-neighbor range resource"},
        "expected_latch":{"fresh_continue":0,"story7_entry":0,"after_story_ack":2},
        "result_codes":"SDL ongoing0/victory1/defeat2; original raw0/2/1",
        "unknown_tail":"metadata after1951, unused active after21, roster/manual slots and all other bytes preserved"}


def prepare(original):
    original=Path(original).resolve(strict=True)
    for name,sha in SOURCE_HASHES.items():
        if digest((original/name).read_bytes())!=sha:raise ValueError("原资源身份不同："+name)
    path=original/"START.SAV"
    if digest(path.read_bytes())!=START_SHA:raise ValueError("只读START.SAV身份不同")
    source=decode_save(path)
    metadata=archive_entries((original/"FDFIELD.DAT").read_bytes())[88]
    plain,encoded,manifest=derive(source,metadata)
    tokens=text_fragments(archive_entries((original/"FDTXT.DAT").read_bytes())[30])[7]
    if tokens[:2]!=(-18,9) or tokens[-1]!=-1 or any(t<0 and t not in (-18,-2,-1) for t in tokens):
        raise ValueError("真实stage29 story7控制域不同")
    return {"original":original,"plain":plain,"encoded":encoded,"manifest":manifest,"source":source}


@contextmanager
def resource_matcher_context(original,plain):
    """只给本stage29/21raw的纯资源matcher借实际维度；绝不改planner门。"""
    import fd2_natural_oracle as natural
    if len(plain)!=0x59cb or plain[META+1:META+3]!=bytes((21,29)):
        raise ValueError("资源adapter仅限本stage29/21raw受控fixture")
    field=(Path(original)/"FDFIELD.DAT").read_bytes()
    if digest(field)!=SOURCE_HASHES["FDFIELD.DAT"]:raise ValueError("FDFIELD原资源身份不同")
    data=archive_entries(field)[87];dimensions=struct.unpack_from("<HH",data)
    if dimensions!=(45,40) or len(data)!=4+45*40*4:raise ValueError("原stage29地图完整45x40源域不同")
    before=dict(natural.STAGE_DIMENSIONS)
    try:
        natural.STAGE_DIMENSIONS[29]=dimensions
        yield {"stage":29,"active_count":21,"entry":87,"width":45,"height":40,"length":len(data),
               "sha256":digest(data),"scope":"temporary pure resource matcher only; natural.validate_plan still rejects stage29"}
    finally:
        natural.STAGE_DIMENSIONS.clear();natural.STAGE_DIMENSIONS.update(before)


def original_owner(memory, original, descriptor, *, actors=True,stage=29,count=21):
    """复用S0地址证明规则；仅限29/21或20/18显式来源夹具观测域。"""
    if (stage,count) not in ((29,21),(20,18)):raise ValueError("S2 reader仅接受29/21或20/18显式机制域")
    executable=(Path(original)/"FD2.EXE").read_bytes()
    if digest(executable)!=SOURCE_HASHES["FD2.EXE"] or len(memory)!=s0.rpc_api.RAM_SIZE:
        raise ValueError("原EXE身份或RPC完整长度不同")
    le=s0.parse_le(executable);fixups=s0.parse_fixups(executable,le,s0.parse_objects(executable,le))
    anchors=s0.rpc_api._anchors(executable,fixups)
    bases=[s0.rpc_api.locate(memory,a)-(a["cva"]-0x10000) for a in anchors]
    if len(set(bases))!=1 or bases[0]<0:raise ValueError("两原码锚未定位同一object1")
    code=bases[0];proofs=[]
    for f in fixups:
        if f.target_object==2 and any(a<=f.source_addr_relbase<b for a,b in
                ((0x26152,0x265ec),(0x2b777,0x2b843),(0x2968d,0x2986f))):
            value=struct.unpack_from("<I",memory,code+f.source_addr_relbase-0x10000)[0]
            proofs.append({"source_cva":hex(f.source_addr_relbase),"target_offset":f.target_offset,
                           "base_DS_offset":value-f.target_offset})
    if len(proofs)<2 or len({r["base_DS_offset"] for r in proofs})!=1:raise ValueError("本轮object2 fixup基址不同")
    base=proofs[0]["base_DS_offset"]
    def get(at):
        if not 0<=base+at<=len(memory)-4:raise ValueError("原global越界")
        return struct.unpack_from("<I",memory,base+at)[0]
    raw=get(0x3ecc)
    if raw not in (0,1,2):raise ValueError("原结果码无效")
    state={"stage":get(0x3c03),"count":get(0x3beb),"cached_result":{0:0,1:2,2:1}[raw],"original_result_raw":raw}
    state.update(camera=[get(0x3aa9),get(0x3aad)],focus=[get(0x3ab1),get(0x3ab5)])
    if actors:
        pointer=get(0x3a45)
        if state["stage"]!=stage or state["count"]!=count or not 0<pointer<=len(memory)-count*STRIDE:raise ValueError("原owner不在显式S2机制域")
        records=memory[pointer:pointer+count*STRIDE]
        state["actors"]=s0.active_fields(records,count);state["records_hex"]=records.hex()
    return state,{"object1_DS_offset":code,"object2_DS_offset":base,"object2_linear_base":descriptor["base"]+base,
                  "fixup_proofs":proofs,"actor_content_scan_used":False,"scope":"explicit S2 source owner 29/21 or 20/18; post-story may inspect latch only"}


def sdl_owner(cdb,target,exe,pdb,work,*,stage=29,count=21):
    if (stage,count) not in ((29,21),(20,18)):raise ValueError("S2 reader仅接受29/21或20/18显式机制域")
    work=Path(work);record={"symbols":s0.cdb_api.symbols(exe,pdb,Path(cdb).parent/"dbghelp.dll")}
    with s0.cdb_api.CDB(cdb,target,work,record) as debugger:
        debugger.request(".sympath "+str(Path(pdb).resolve().parent));debugger.request(".reload /f "+Path(exe).name)
        def get(expression):return s0.cdb_api.cpp_integer(debugger.request("?? "+expression))
        address,size=get("&s2_result_game"),get("sizeof(s2_result_game)")
        if not address or not 7680<=size<=1024*1024:raise ValueError("PDB fixture Game地址/大小无效")
        offsets={name:get("(char *)&s2_result_game."+name+" - (char *)&s2_result_game")
                 for name in ("stage","units.count","units.items","battle_result","event_resume.active",
                              "camera_cell_x","camera_cell_y","cursor_cell_x","cursor_cell_y")}
        widths={name:get("sizeof(s2_result_game."+name+")") for name in ("stage","units.count","battle_result","event_resume.active",
                              "camera_cell_x","camera_cell_y","cursor_cell_x","cursor_cell_y")}
        data,receipt=s0.cdb_api.read_owned_memory(target,address,size,exe)
        def field(name):
            at,width=offsets[name],widths[name]
            if width not in (1,2,4,8) or not 0<=at<=len(data)-width:raise ValueError("PDB字段布局越界")
            return int.from_bytes(data[at:at+width],"little")
        if field("stage")!=stage or field("units.count")!=count or field("battle_result") not in (0,1,2):raise ValueError("SDL owner不在本fixture域")
        at=offsets["units.items"]
        if not 0<=at<=len(data)-count*STRIDE:raise ValueError("SDL原记录前缀越界")
        records=data[at:at+count*STRIDE]
        state={"stage":stage,"count":count,"cached_result":field("battle_result"),"event_active":field("event_resume.active"),
               "actors":s0.active_fields(records,count),"records_hex":records.hex(),
               "camera":[field("camera_cell_x"),field("camera_cell_y")],"focus":[field("cursor_cell_x"),field("cursor_cell_y")]}
        path=work/"sdl-game.bin"
        if path.exists():raise ValueError("PDB快照路径已存在")
        path.write_bytes(data);record.update(layout={"address":address,"size":size,"offsets":offsets,"widths":widths},
            memory_read=receipt,snapshot={"path":str(path),"size":size,"sha256":digest(data)},named_fixture_owner=True)
    if digest(Path(exe).read_bytes())!=record["symbols"]["exe_sha256"] or digest(Path(pdb).read_bytes())!=record["symbols"]["pdb_sha256"]:
        raise ValueError("读取期间fixture/PDB改变")
    return state,record
