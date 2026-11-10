"""G4共享phase标题/菜单时钟：只读Domain完整指令/xref及strict LE字节核验。

由技能run.py注入db；不保存DB、不猜ABI，不改权威生成物。
"""
from pathlib import Path
import hashlib
import json
import sys

FUNCTIONS=(0x1741c,0x176b4,0x17898,0x197e5,0x19953,0x1a30b,0x1a7bd,0x1a7f1,
    0x1f1cc,0x1f30a,0x1f42d,0x4eb59,0x4ed34,0x4ed4f)
GLOBALS=(0x53c13,0x53c17)


def extract(db,root,output):
    root=Path(root);output=Path(output);output.mkdir(parents=True,exist_ok=True)
    sys.path.insert(0,str(root/'tools/ida'));sys.path.insert(0,str(root/'tools'))
    from g1_runtime_extract import function_evidence,global_evidence
    from rebuild_fd2_analysis import parse_le,parse_objects,build_relbase_image,parse_fixups,apply_fixups
    exe=(root/'original_game/FD2.EXE').read_bytes()
    if hashlib.sha256(exe).hexdigest()!='bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd':raise ValueError('G4原EXE身份不同')
    report={'schema':'fd2-g4-runtime-domain-v1','address_view':'canonical relbase VA','database_saved':False,
        'functions':[function_evidence(db,va) for va in FUNCTIONS],
        'globals':[global_evidence(db,va) for va in GLOBALS]}
    le=parse_le(exe);objects=parse_objects(exe,le)
    relocated=apply_fixups(build_relbase_image(exe,le,objects),parse_fixups(exe,le,objects))
    count=0
    for function in report['functions']:
        for i in function['instructions']:
            actual=bytes.fromhex(i['bytes']);expected=bytes(relocated[i['va']:i['va']+len(actual)])
            if actual!=expected:raise ValueError('G4 Domain/strict LE指令不等：'+hex(i['va']))
            count+=len(actual)
    for global_ in report['globals']:
        va=global_['va'];expected=bytes(relocated[va:va+4])
        if bytes.fromhex(global_['initial_bytes'])!=expected:raise ValueError('G4静态global原值不同：'+hex(va))
    for name,value in [('domain-evidence.json',report),('validation.json',{'ok':True,
        'functions_checked':len(FUNCTIONS),'globals_checked':len(GLOBALS),'instruction_bytes_checked':count,
        'static_original_bytes_only':True,'original_machine_execution_claimed':False,'database_saved':False})]:
        (output/name).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return {'ok':True,'functions':len(FUNCTIONS),'globals':len(GLOBALS),'instruction_bytes':count}
