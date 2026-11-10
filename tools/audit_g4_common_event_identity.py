"""只读核G4 common.key.consume唯一新增身份后的原story引用，不运行关卡。"""
import argparse
import hashlib
import json
from pathlib import Path

from audit_g4_render_source import archive_entries, text_fragments

SOURCES={
    'FDFIELD.DAT':'81310211c6e96ebd89e79648650696691c740229fa4095ece37e7226bc27b74d',
    'FDTXT.DAT':'b47e39ee727482528da061a8705f5cdee8a9cf759447aeda298afe17d9dd8bcc',
}

def audit(original):
    raw={name:(original/name).read_bytes() for name in SOURCES}
    if any(hashlib.sha256(raw[name]).hexdigest()!=sha for name,sha in SOURCES.items()):
        raise ValueError('原DAT身份不同')
    metadata=archive_entries(raw['FDFIELD.DAT'])[76]
    if len(metadata)!=1951 or metadata[2]!=70:
        raise ValueError('原stage25 metadata布局不同')
    templates=[metadata[131+i*26:131+(i+1)*26] for i in range(metadata[2])]
    appended=[{'template':i,'unit_id':t[1],'group':t[0x15]} for i,t in enumerate(templates) if t[0x15]==1]
    if appended!=[{'template':68,'unit_id':31,'group':1}]:
        raise ValueError('原group1新增身份不同')
    fragment=text_fragments(archive_entries(raw['FDTXT.DAT'])[26])[4]
    controls=[];i=0
    while i<len(fragment):
        token=fragment[i];i+=1
        if token in (-17,-18,-19,-20):
            if i>=len(fragment):raise ValueError('portrait控制缺参数')
            controls.append({'control':token,'argument':fragment[i]});i+=1
        elif token<0 and token not in (-1,-2,-3):
            raise ValueError('引用分析外的动态/未知控制')
    if any(c['control'] in (-19,-20) for c in controls) or {c['argument'] for c in controls}!={0,4,9,31}:
        raise ValueError('原story4身份引用不同')
    if any((original/name).read_bytes()!=value for name,value in raw.items()):
        raise ValueError('核验期间原DAT改变')
    return {'ok':True,'schema':'fd2-g4-common-key-story-identity-v1','source_sha256':SOURCES,
        'source_callback':'35a0d / event_callback061 / original stage025 only',
        'metadata_entry':76,'story_entry':26,'story_fragment':4,
        'group_append_identities':appended,'portrait_controls':controls,
        'persistent_append':{'unit_id':31,'text_id':31},
        'conclusion':'only future text31 resolves to same unit31; no future raw actor portrait reference',
        'scope':'current15 fixtures: only common.key.consume appends identity before a later story; no arbitrary S binding claim',
        'native_runtime':False,'portrait_pixels_decoded':False,'source_files_unchanged':True}

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--original',type=Path,required=True)
    parser.add_argument('--json',type=Path,required=True)
    args=parser.parse_args();result=audit(args.original)
    args.json.parent.mkdir(parents=True,exist_ok=True)
    args.json.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps(result,ensure_ascii=False))

if __name__=='__main__':main()
