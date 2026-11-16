"""Normal250cc flash: source-state prefix and full-RGB before/solid relation.

No current C/cold graph/screenshot creates the source mask or RGB. A is an
explicit same-run observed before; this is not an absolute field compositor.
"""
from __future__ import annotations
import hashlib,io,json,struct
from pathlib import Path
from PIL import Image
from native import s3_fixture as state
from native import s5_late_fixture as late
from native.s5_cinematic_fixture import speech_view
from fd2_stage1_complete_oracle import apply_script
from fd2_battle_animation_frames import rle_material
from audit_g4_render_source import archive_entries
from run_original_comparison import normalize
from static_data.verify_unit_tables import source_range

RGB_SOURCE_SHA='b439808b4974bb9ceca12b8de39c3f44cd0bef7269f823aff32765ce71ed8a2a'
SOURCE_MANIFEST=Path(__file__).with_name('fixtures')/'f4_flash_source.json'
def digest(raw):return hashlib.sha256(raw).hexdigest()

def source_model(p,rgb_source):
    original=Path(p['original']);exe=(original/'FD2.EXE').read_bytes()
    manifest=json.loads(SOURCE_MANIFEST.read_text(encoding='utf-8'))
    if manifest['schema']!='fd2-f4-flash-source-v1' or digest(exe)!=manifest['exe_sha256']:
        raise ValueError('flash原EXE/来源manifest不合格')
    for f in manifest['functions']:
        for c in f['chunks']:
            if digest(source_range(exe,c['start'],c['end']-c['start']))!=c['raw_unrelocated_sha256']:
                raise ValueError('flash完整源chunk原字节变化')
    rgb_source=Path(rgb_source).resolve();rgb_bytes=rgb_source.read_bytes()
    if digest(rgb_bytes)!=RGB_SOURCE_SHA:raise ValueError('固定DOSBox-X RGB来源身份不符')
    if p['manifest']['scenario']!=late.SCENARIOS[2] or p['source'][state.META+2]!=26:
        raise ValueError('flash只接受已登记26坏臂合法隔离夹具')
    records=state.loaded_units(p['source'],'original');raw=p['data']['raw']
    if len(records)!=63:raise ValueError('flash需真实16部署+47group0的63active')
    trace=[]
    def checkpoint(label,camera,focus):
        trace.append({'step':label,'camera':list(camera),'focus':list(focus),
            'records_sha256':digest(b''.join(records)),'raw1_hex':records[1].hex()})
    #250cc25103..25123 and complete233c6: no specialactor(arg14=0).
    for u in records[:16]:u[5]=0
    for u in records:u[5]&=127
    for i in range(16):
        records[i][0]=raw[0x52306+i];records[i][1]=raw[0x52316+i];records[i][3]=2
    camera=[9,8];focus=[9,8];checkpoint('25141 layout',camera,focus)
    camera,focus=speech_view(original,records,26,(8,),camera,focus)
    checkpoint('25168 text8',camera,focus)
    scripts=[]
    for identity,fragment in ((82,13),(84,14),(82,15)):
        script=late.script(p['data'],original,identity);scripts.append(script)
        apply_script(records,script);checkpoint('script'+str(identity),camera,focus)
        camera,focus=speech_view(original,records,26,(fragment,),camera,focus)
        checkpoint('text'+str(fragment),camera,focus)
    cache=[]
    for u in state.loaded_units(p['source'],'original'):
        if u[7] not in cache:cache.append(u[7])
    if any(u[2]>=len(cache) or cache[u[2]]!=u[7] for u in records):
        raise ValueError('flash前cache身份未沿同Load来源')
    unit=records[1];sprite=cache[unit[2]]
    if list(unit[:2])==[255,255] or unit[5]&1 or unit[4] or unit[0x26]:
        raise ValueError('flash前raw1不能使用已传送/hidden/未资格步态')
    if camera!=[9,8] or focus!=[15,9] or list(unit[:2])!=[15,9] or sprite!=52:
        raise ValueError('flash完整source前缀不再是已核有限位置/身份')
    icon=(original/'FDICON.B24').read_bytes();w,h,count=struct.unpack_from('<HHH',icon)
    if (w,h,count)!=(24,24,1680):raise ValueError('flash FDICON固定原目录不同')
    rules=[];x=4+(unit[0]-camera[0])*24;y=-2+(unit[1]-camera[1])*24
    for phase in range(3):
        frame=sprite*12+phase;a=struct.unpack_from('<I',icon,6+4*frame)[0]
        b=struct.unpack_from('<I',icon,10+4*frame)[0] if frame+1<count else len(icon)
        if not 6+4*count<=a<b<=len(icon):raise ValueError('flash RLE source越目录')
        pixels,mask=rle_material(struct.pack('<HH',24,24)+icon[a:b])
        #4e127 writes every ordinary RLE store inclopaque0; SKIP/stride2 holes preserve.
        addresses=[(y+i//24)*320+x+i%24 for i,v in enumerate(mask.getdata()) if v]
        if any(not (4<=x+i%24<316 and 4<=y+i//24<196)
                for i,v in enumerate(mask.getdata()) if v) or len(addresses)<64:
            raise ValueError('flash固定源mask未完整落在正常viewport')
        rules.append({'effective_idle':phase,'raw_idle_values':[phase] if phase<2 else [2,3],
            'source_frame':frame,'source_sha256':digest(icon[a:b]),'addresses':addresses,
            'mask_sha256':digest(bytes(1 if v else 0 for v in mask.getdata()))})
    palette=archive_entries((original/'FDOTHER.DAT').read_bytes())[0]
    if len(palette)!=768 or max(palette)>63:raise ValueError('flash原palette0不是6bit')
    color=[(v<<2)|(v>>4) for v in palette[600:603]]
    contract={'schema':'fd2-f4-flash-before-relation-v1','controlled_fixture':True,
        'natural_whole_stage_claimed':False,'source_manifest_sha256':digest(SOURCE_MANIFEST.read_bytes()),
        'rgb_source':{'path':str(rgb_source),'sha256':RGB_SOURCE_SHA,'git_blob':'509d42fbead67a56594e49cec55549bc6fcc6d7c'},
        'flash_call':0x253fa,'post_teleport_call':0x2541f,'trace':trace,'scripts':scripts,
        'records_hex':[u.hex() for u in records],'sprite_cache_ids':cache,'camera':camera,'focus':focus,
        'raw_actor':1,'sprite_id':sprite,'position':[x,y],'raw_color6':list(palette[600:603]),'solid_RGB':color,
        'rules':rules,'source_order':'five(A,tick1,B,tick1), thenA; noRNG/clockphase advancement insideflash',
        'absolute_full_field_compositor_qualified':False,
        'scope':'same-run observed completeRGB A; only source raw1 solidmask may differ in B; all other64000 pixels retained'}
    return contract,records

def solid_page(before,addresses,rgb):
    if len(before)!=320*200*3 or len(rgb)!=3 or any(type(v)is not int or not 0<=v<=255 for v in rgb):
        raise ValueError('flash需要完整320x200RGB和固定source RGB')
    if not addresses or len(set(addresses))!=len(addresses) or any(type(a)is not int or not 0<=a<64000 for a in addresses):
        raise ValueError('flash source mask地址非法/重复')
    result=bytearray(before);color=bytes(rgb)
    for a in addresses:result[3*a:3*a+3]=color
    return bytes(result)

def find_relation(samples,model,*,minimum_changes=4):
    """Full-page equality; selected PNGs are validated separately by read_series.

    Source loop has10changes; finiteWGC must witness at leastABABA, not claim
    everyCPU copy/display interval. One fixedA/B pair and maskclass per witness.
    """
    if minimum_changes!=4:raise ValueError('flash最小门固定为ABABA，不支持降级')
    if not samples:raise ValueError('flash未取得有界完整采样')
    by_hash={}
    for i,item in enumerate(samples):
        if len(item['RGB'])!=192000:raise ValueError('flash采样不是完整RGB页')
        by_hash.setdefault(digest(item['RGB']),[]).append(i)
    results=[]
    masks={}
    for rule in model['rules']:
        key=tuple(rule['addresses']);masks.setdefault(key,[]).append(rule['effective_idle'])
    for ai,a in enumerate(samples):
        ah=digest(a['RGB'])
        for addresses,phases in masks.items():
            b=solid_page(a['RGB'],addresses,model['solid_RGB']);bh=digest(b)
            changed=sum(a['RGB'][3*i:3*i+3]!=b[3*i:3*i+3] for i in addresses)
            if changed<64:continue #rejecttrivial A==B/unknownsolidbefore.
            wanted=(ah,bh,ah,bh,ah);chosen=[];after=ai-1
            for expected in wanted:
                n=next((j for j in by_hash.get(expected,[]) if j>after and
                    samples[j]['RGB']==(a['RGB'] if expected==ah else b)),None)
                if n is None:break
                chosen.append(n);after=n
            if len(chosen)==5:
                results.append({'A_RGB_sha256':ah,'B_RGB_sha256':bh,'changed_source_pixels':changed,
                    'effective_idle_classes':phases,'witness_indices':chosen,
                    'witnesses':[{k:v for k,v in samples[j].items() if k!='RGB'} for j in chosen]})
    if not results:raise ValueError('有限WGC未见同一全64000RGB A/B的ABABA，不能降为ROI或旧图')
    result=min(results,key=lambda r:tuple(r['witness_indices']))
    result.update(whole_before_relation_qualified=True,absolute_compositor_qualified=False,
        all_CPU_or_display_frames_qualified=False,exact_BIOS_wall_timing_qualified=False)
    return result

def read_series(rows,pid,hwnd,*,after_index,before_index):
    samples=[];previous=-1;stamp=-1
    for row in rows:
        if row.get('pid')!=pid or row.get('hwnd')!=hwnd or type(row.get('index'))is not int or row['index']<=previous or \
                type(row.get('wgc_timestamp_100ns'))is not int or row['wgc_timestamp_100ns']<=stamp:
            raise ValueError('flash WGC row owner/顺序不合格')
        previous=row['index'];stamp=row['wgc_timestamp_100ns']
        if not after_index<row['index']<before_index:continue
        path=Path(row['path']);raw=path.read_bytes()
        if digest(raw)!=row['sha256']:raise ValueError('flash原WGC PNG身份变化')
        with Image.open(io.BytesIO(raw)) as image:
            frame=normalize(image).convert('RGB')
            if frame.size!=(320,200):raise ValueError('flash归一尺寸不符')
            rgb=frame.tobytes()
        samples.append({'RGB':rgb,'index':row['index'],'path':str(path),'sha256':row['sha256'],
            'pid':pid,'hwnd':hwnd,'wgc_timestamp_100ns':stamp,'RGB_sha256':digest(rgb)})
    return samples
