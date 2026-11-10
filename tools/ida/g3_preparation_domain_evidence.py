#!/usr/bin/env python3
"""G3准备局部原码执行的只读Domain函数读集；不重复准备静态表。

由ida-domain-scripting/run.py提供db，collect(db, output)；不保存数据库。
原字节／反汇编保持ignored，执行器另读strict relocated并逐chunk交叉。
"""
from pathlib import Path
import json
import runpy

TARGETS=(0x2B777,0x2B843,0x2A07A,0x2AE0E,0x2AEDB,0x34894,
    0x1B722,0x1B8A6,0x1B8E7,0x1BB8C,0x1C142,0x26A7A,0x26B91,
    0x2968D,0x2A43E,0x2872B,0x28CBD,0x28F65)
NAMING_TARGETS=(0x26A0D,0x26A7A,0x26B91,0x26C9B,0x26CE4,0x26E38,
    0x26EDA,0x27079,0x272D0,0x275E6,0x27738,0x27816,0x278E7,
    0x27D33,0x27F4A,0x2810B,0x2825B,0x28632,0x28B41,0x28EFE,
    0x2A07A,0x2A0C2,0x2A29D,0x2A694,0x2A857,0x2AEDB,
    0x2B439,0x2B4FB,0x2B67F,0x2B749,0x2B843,0x2FB2C,0x2FE14)

def _collect(db,output,targets,schema):
    function_record=runpy.run_path(str(Path(__file__).with_name('g1_preparation_evidence.py')))['function_record']
    functions=[function_record(db,ea) for ea in targets]
    report={'schema':schema,'targets':list(targets),
        'scope':'read-only Domain full functions; not execution, ABI proof, tables or database publication',
        'functions':functions}
    output=Path(output);output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return {'functions':len(functions),'chunks':sum(len(f['chunks']) for f in functions),'output':str(output)}

def collect(db,output):
    return _collect(db,output,TARGETS,'fd2-g3-preparation-domain-readset-v1')

def collect_names(db,output):
    return _collect(db,output,NAMING_TARGETS,'fd2-g3-preparation-naming-readset-v1')
