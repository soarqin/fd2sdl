"""S3结果表真实caller的完整Domain/strict只读来源；不保存或命名数据库。"""
import hashlib
import json
from pathlib import Path
from g1_preparation_evidence import function_record,RELOCATED_SHA256

TARGETS=(0x117e7,0x1a30b,0x1a866,0x1d80b,0x1d8ba)


def collect(db,image_path,output):
    output=Path(output)
    if output.exists():raise ValueError('output exists')
    image=Path(image_path).read_bytes()
    if hashlib.sha256(image).hexdigest()!=RELOCATED_SHA256:raise ValueError('strict image identity differs')
    functions=[function_record(db,va) for va in TARGETS]
    for row in functions:
        if not row['chunks'] or not row['blocks'] or not row['instructions']:raise ValueError('incomplete caller')
        for chunk in row['chunks']:
            raw=bytes.fromhex(chunk['raw_hex']);a,b=chunk['start'],chunk['end']
            if raw!=image[a:b] or len(raw)!=b-a or hashlib.sha256(raw).hexdigest()!=chunk['sha256']:
                raise ValueError('caller Domain/strict bytes differ')
    references=[{'from':r.from_ea,'to':r.to_ea,'type':r.type.name} for r in db.xrefs.to_ea(0x51b19)]
    report={'schema':'fd2-s3-result-callers-v1','functions':functions,'table_refs':references,
        'image_sha256':RELOCATED_SHA256,'database_saved':False,'original_runtime_executed':False,
        'source_scope':'complete callers and original CFG/table xrefs; not physical/AI/GUI execution'}
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return {'path':str(output),'functions':len(functions),'bytes':sum(c['end']-c['start'] for f in functions for c in f['chunks']),
        'table_refs':len(references),'sha256':hashlib.sha256(output.read_bytes()).hexdigest()}
