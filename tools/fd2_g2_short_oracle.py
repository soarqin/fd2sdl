"""G2 SHORT弓距离2：原始记录／有限HP／完整SAV与最低资源、声音门。

18d8c玩家恒FULL；本夹具只经1a30b→13a9f→1548e→1e856/1e98c。
读取原EXE和START，既不引用SDL冷定义，也不从捕获终态拟合公式。
"""
from __future__ import annotations

import hashlib
import io
import itertools
import json
import math
from pathlib import Path
import struct
import sys

from PIL import Image
import fd2_g2_battle_oracle as common
import fd2_battle_animation_frames as frames
from run_original_comparison import archive_entry, normalize
from run_original_player import sheet_frame, screen

sys.path.insert(0,str(Path(__file__).resolve().parent/'static_data'))
from verify_unit_tables import source_range

SCENARIO='ai-short-bow'
META,UNITS,STRIDE,SAVE_SIZE=common.META,common.UNITS,common.STRIDE,common.SAVE_SIZE
ATTACKER,TARGET=11,0
HERO_WORLD,ARCHER_WORLD,SAVE_WORLD=(7,14),(9,14),(6,14)
WORD=common._word


def source_contract(original):
    original=Path(original);exe=(original/'FD2.EXE').read_bytes()
    if common.digest(exe)!=common.EXE_SHA256:raise ValueError('SHORT原EXE身份不同')
    encoded=(original/'START.SAV').read_bytes()
    if common.digest(encoded)!=common.START_SHA256:raise ValueError('SHORT START身份不同')
    source=common.valid_plain(common.crypt(encoded));donor=common.actor(source,2);hero=common.actor(source,0)
    weapon=source_range(exe,0x602ad+20*23,23)
    if donor[7:9]!=bytes((4,4)) or donor[32:34]!=bytes((3,1)) or donor[10:12]!=bytes((64,20)) or \
        list(struct.unpack_from('<8H',donor,64))!=[48,48,0,0,26,6,92,2] or \
        weapon[0]!=3 or weapon[9]!=0 or weapon[11:13]!=bytes((1,2)):
        raise ValueError('SHORT真实unit4/profile3/weapon20来源不同')
    if any(donor[26:31]) or any(donor[34:40]):raise ValueError('SHORT donor不能含magic/status竞争')
    for slot in range(8):
        flag,identity=donor[10+slot*2:12+slot*2]
        if not flag&128 and source_range(exe,0x602ad+identity*23,23)[13]!=0:
            raise ValueError('SHORT donor具有可用Item竞争')
    field=(original/'FDFIELD.DAT').read_bytes();shape=(original/'FDSHAP.DAT').read_bytes()
    if common.digest(field)!=common.FIELD_SHA256 or common.digest(shape)!=common.SHAPE_SHA256:
        raise ValueError('SHORT原地图/地形身份不同')
    grid=archive_entry(field,0);metadata=archive_entry(field,1);attrs=archive_entry(shape,metadata[0]*2+1)
    if struct.unpack_from('<HH',grid)!=(24,24):raise ValueError('SHORT仅stage0原图')
    cells=[]
    for xy in (ARCHER_WORLD,HERO_WORLD,SAVE_WORLD):
        cell=struct.unpack_from('<I',grid,4+(xy[1]*24+xy[0])*4)[0]
        attr=attrs[(cell&1023)*4:(cell&1023)*4+4]
        if len(attr)!=4 or attr[0]&96 or (cell>>16)&31:raise ValueError('SHORT目标/保存格不是无事件普通格')
        cells.append({'world':list(xy),'cell':cell,'attr':list(attr)})
    ap_table=struct.unpack('<6i',source_range(exe,0x51a12,24));dp_table=struct.unpack('<6i',source_range(exe,0x51a2a,24))
    if cells[0]['attr'][1]!=2 or cells[1]['attr'][1]!=0:raise ValueError('SHORT地形class变化')
    # 原signed idiv向零：负百分比不能用Python负数floor。
    product=WORD(donor,72)*ap_table[2]
    attack=WORD(donor,72)+(1 if product>=0 else -1)*(abs(product)//100)
    product=WORD(hero,74)*dp_table[0]
    defense=WORD(hero,74)+(1 if product>=0 else -1)*(abs(product)//100)
    normal=9*(attack-defense)//10;critical=9*(attack-defense//2)//10
    critical_chance=source_range(exe,0x524a8+donor[32]-1,1)[0]
    if (attack,defense,normal,critical,critical_chance)!=(25,12,11,17,3) or normal//9!=1 or critical//9!=1:
        raise ValueError('SHORT有限伤害分支来源不同')
    if WORD(hero,64)!=42 or WORD(hero,66)!=42 or WORD(donor,76)-WORD(hero,78)!=90:
        raise ValueError('SHORT Hero42/命中阈值90前置不同')
    pointers=struct.unpack('<21I',source_range(exe,0x61955,84))
    sequence_va=0x60000+pointers[weapon[0]]
    count=source_range(exe,sequence_va,1)[0];sequence=source_range(exe,sequence_va+1,count*2)
    pairs=[list(sequence[i*2:i*2+2]) for i in range(count)]
    if sequence_va!=0x619b6 or pairs!=[[6,3],[7,3],[8,3],[9,255],[10,255],[11,255]]:
        raise ValueError('SHORT弓kind3不再映射已证FX6..11/cue3')
    if source_range(exe,0x53ec8,4)!=bytes(4):raise ValueError('SHORT fresh进程贡献初值非0')
    return source,{'executable_sha256':common.EXE_SHA256,'donor_raw':2,'donor_record_sha256':common.digest(donor),
        'donor_record_hex':donor.hex(),'weapon_id':20,'weapon_record_va':hex(0x602ad+20*23),
        'weapon_record_hex':weapon.hex(),'weapon_type':3,'range':[1,2],'movement':0,'counterattack':False,
        'terrain_cells':cells,'adjusted_attack':attack,'adjusted_defense':defense,'hit_threshold':90,
        'critical_chance':3,'normal_damage':11,'critical_damage':17,'one_or_two_strikes':True,
        'finite_final_hp':[8,14,20,25,31,42],'fx_sequence':pairs,'fx_pointer_table_va':'0x61955',
        'fx_pointer_table_raw_hex':source_range(exe,0x61955,84).hex(),'fx_sequence_va':hex(sequence_va),
        'sound_bank':64,'legal_minimum_cues':[3,4],
        'source_functions':['0x14237','0x13a9f','0x1548e','0x1e856','0x1ecc7','0x1e98c','0x1eb05','0x1e7f6'],
        'candidate_proof':'14237 MOV0 only origin9,14; sole nonzero-side target within weapon1..2 is Hero7,14; AP-DP>2 gives priority8; no magic bit or Item.use_code; distance2 prevents1f0dc counter',
        'candidate_priority':8,'candidate_world':list(ARCHER_WORLD),'target_world':list(HERO_WORLD),
        'fresh_process_contribution_initial_value':0,'counter_undefined_fourth_argument_not_reached':True}


def derive_fixture(original,scenario=SCENARIO):
    if scenario!=SCENARIO:raise ValueError('未知SHORT夹具')
    source,contract=source_contract(original);data=bytearray(source)
    for raw in range(source[META+1]):
        record=bytearray(common.actor(source,raw));record[3:5]=bytes(2)
        if raw in (1,2,3):record[:2]=bytes((raw,21))
        elif raw>=4:record[5]|=4
        if raw==ATTACKER:
            record=bytearray(common.actor(source,2));record[:2]=bytes(ARCHER_WORLD)
            record[3:7]=bytes((0,0,0,0));record[26:31]=bytes(5)
            record[52]=0;record[59]=0
        data[UNITS+raw*STRIDE:UNITS+(raw+1)*STRIDE]=record
    camera=tuple(source[META+3:META+5]);data[META+5:META+9]=bytes((*HERO_WORLD,HERO_WORLD[0]-camera[0],HERO_WORLD[1]-camera[1]))
    data[META+14]=1;data[META+16:META+18]=bytes((0,1))
    plain=common.checksum(data)
    allowed=set(range(UNITS,UNITS+12*STRIDE))|set(range(META+5,META+9))|{META+14,META+16,META+17}|set(range(SAVE_SIZE-4,SAVE_SIZE))
    patches=[{'offset':i,'before':a,'after':b} for i,(a,b) in enumerate(zip(source,plain)) if a!=b]
    if any(p['offset'] not in allowed for p in patches):raise ValueError('SHORT夹具写出声明域')
    for turn,side in ((1,1),(1,0),(2,2)):
        if any((plain[3+i*3],plain[5+i*3])==(turn,side) for i in range(16)):
            raise ValueError('SHORT夹具包含未隔离回合事件')
    from run_original_focus import native_focus_reference
    for start,end in ((HERO_WORLD,SAVE_WORLD),(SAVE_WORLD,ARCHER_WORLD),(ARCHER_WORLD,HERO_WORLD),(HERO_WORLD,SAVE_WORLD)):
        if tuple(native_focus_reference(camera,start,end)['camera'])!=camera:raise ValueError('SHORT需未建模滚屏')
    encoded=common.crypt(plain)
    return encoded,plain,{'scenario':SCENARIO,'source_sha256':common.START_SHA256,'source_plain_sha256':common.START_PLAIN_SHA256,
        'derived_sha256':common.digest(encoded),'derived_plain_sha256':common.digest(plain),'plaintext_patches':patches,
        'controlled_fixture':True,'natural_action_history':False,'resource_contract':contract,
        'attacker_raw':11,'target_raw':0,'end_world':list(SAVE_WORLD),'save_world':list(SAVE_WORLD),
        'focus_after_end':list(HERO_WORLD),'camera':list(camera),'turns':[1,2],
        'unchanged_domains':['persistent roster','manual slots','unknown bytes','all MP/EX/inventory','all other HP'],
        'unsaved_state_not_measured':['RNG stream','0x53ec8 live contribution','modal state'],
        'source_write_scope':'raw11 copies complete real raw2; xy/flags/side/direction/AI0/MOV0/noMagic isolation explicit, donor AI params/descriptor unknowns preserved; other AI04, players1..3 far; effect1/music0/sfx1; no Hero HP/stat synthesis'}


def paths():
    return [{'damages':list(values),'hp':42-sum(values)} for n in (1,2)
        for values in itertools.product((0,11,17),repeat=n)]


def check_result(original,source,measured,*,engine):
    common.valid_plain(measured)
    _,registered,_=derive_fixture(original)
    if source!=registered or engine not in ('original','sdl'):raise ValueError('SHORT结果须已登记完整前态/端点')
    hp=WORD(common.actor(measured,0),64);allowed=sorted({p['hp'] for p in paths()})
    expected=bytearray(source);expected[META]=2
    camera=source[META+3:META+5];expected[META+5:META+9]=bytes((*SAVE_WORLD,SAVE_WORLD[0]-camera[0],SAVE_WORLD[1]-camera[1]))
    ids=[]
    for raw in range(source[META+1]):
        identity=common.actor(source,raw)[7]
        if identity not in ids:ids.append(identity)
        start=UNITS+raw*STRIDE;expected[start+3:start+5]=bytes(2);expected[start+5]&=127
        if engine=='original':expected[start+2]=ids.index(identity)
    if hp in allowed:struct.pack_into('<H',expected,UNITS+64,hp)
    expected=common.checksum(expected);mismatches=[i for i in range(SAVE_SIZE) if expected[i]!=measured[i]]
    return {'ok':hp in allowed and not mismatches,'hero_hp':hp,'finite_hp':allowed,
        'matching_source_paths':[p for p in paths() if p['hp']==hp],'mismatch_count':len(mismatches),'mismatch_offsets':mismatches[:32],
        'checked_container_bytes':SAVE_SIZE,'turn':2,'counterattack':False,'shared_rng_trajectory_claimed':False}


def visual_resources(original,source):
    original=Path(original);other=(original/'FDOTHER.DAT').read_bytes();palette=[v<<2 for v in archive_entry(other,0)]
    sheet=archive_entry(other,6);at=(80,54);rules={'panel':[],'fx':[],'bar':[],'solid':[]}
    for i in range(39,49):
        image=sheet_frame(sheet,i,lmi=True);mask=image.point(lambda p:255 if p else 0,mode='L')
        anchor=frames._anchor(image,mask,at,{'resource':'FDOTHER6','frame':i},palette)
        if anchor:rules['panel'].append({'frame':i,'anchor':anchor})
    for i in range(6,12):
        image=sheet_frame(sheet,i,lmi=True);mask=image.point(lambda p:255 if p else 0,mode='L')
        anchor=frames._anchor(image,mask,(52,72),{'resource':'FDOTHER6','frame':i},palette)
        if anchor:rules['fx'].append({'frame':i,'anchor':anchor})
    # 1e98c第0FX之前以1da16/stride320将完整sprite写域填64，再覆盖原FX6。
    # 实际present在两次写之后；13个FX6像素覆盖sprite，不能要求未present的纯64页，
    # 也不能删除这13点。正锚保全部原RLE写点，逐点检查64基底+已知FX6覆盖。
    icons=(original/'FDICON.B24').read_bytes();hero=common.actor(source,TARGET)
    if struct.unpack_from('<HHH',icons)!=(24,24,1680) or hero[7]!=0 or hero[3:5]!=bytes(2) or any(hero[37:39]):
        raise ValueError('SHORT实体正锚仅已证Hero0静止、原25/26无状态域')
    ids=[]
    for raw in range(source[META+1]):
        identity=common.actor(source,raw)[7]
        if identity not in ids:ids.append(identity)
    cache_class=ids.index(hero[7])
    if cache_class!=0 or hero[2]!=cache_class:raise ValueError('SHORT Hero0原Load cache映射不同')
    actor_at=screen(source,tuple(hero[:2]));origin=(actor_at[0],actor_at[1]-6)
    fx=sheet_frame(sheet,6,lmi=True);fx_at=(actor_at[0],actor_at[1]-4)
    if fx.size!=(12,10) or fx_at!=(52,72) or origin!=(52,70):raise ValueError('SHORT实体/FX源坐标不同')
    for phase in (0,1,2):
        frame=hero[7]*12+hero[3]*3+phase
        start,end=struct.unpack_from('<II',icons,6+frame*4)
        if not 6+1680*4<=start<end<=len(icons):raise ValueError('SHORT FDICON原frame目录不同')
        _,mask=frames.rle_material(struct.pack('<HH',24,24)+icons[start:end])
        image=Image.new('P',(24,24),64);covered=0
        for y in range(fx.height):
            for x in range(fx.width):
                value=fx.getpixel((x,y))
                if not value:continue
                px=x+fx_at[0]-origin[0];py=y+fx_at[1]-origin[1]
                if not (0<=px<24 and 0<=py<24 and mask.getpixel((px,py))):
                    raise ValueError('SHORT FX6不再完全覆盖已声明Hero写域')
                image.putpixel((px,py),value);covered+=1
        count=sum(bool(p) for p in mask.getdata())
        if covered!=13 or count!=(454 if phase==1 else 449):raise ValueError('SHORT完整RLE实体/FX6覆盖点数不同')
        anchor=frames._anchor(image,mask,origin,{'resource':'FDICON Hero0 opaque SOLID64 then FDOTHER6 frame6',
            'sprite_frame':frame,'fx_frame':6,'write_order':['opaque_sprite_fill64','FX6_nonzero_overlay']},palette)
        if not anchor or anchor['checked_pixels']!=count:raise ValueError('SHORT实体正锚遗漏原RLE写点')
        rules['solid'].append({'frame':phase,'source_cache_class':cache_class,'sprite_origin':list(origin),
            'palette_index':64,'phase3_maps_to':1,'sprite_rle_opaque_pixels':count,
            'source_fx6_overlay_pixels':covered,'solid64_pixels':count-covered,
            'write_order':['1da16 complete RLE opaque fill64 with stride320','1e98c original FX6 nonzero overlay','present'],
            'fx6_origin':list(fx_at),'source_mask':mask,'anchor':anchor})
    bars=archive_entry(other,5);columns={}
    for i in (23,24,25,29,30):
        start,end=struct.unpack_from('<II',bars,6+i*4);raw=bars[start:end]
        if len(raw)!=9 or struct.unpack_from('<HH',raw)!=(1,5):raise ValueError('SHORT原raw HP列不是1x5+5字节')
        columns[i]=Image.frombytes('P',(1,5),raw[4:])
    for width in range(1,71):
        image=Image.new('P',(71,5),0)
        for x in range(71):
            frame=23 if x==0 else 24 if x<width else 25 if x==width else 30 if x==70 else 29
            if width==70 and x==70:frame=25
            image.paste(columns[frame],(x,0))
        anchor=frames._anchor(image,Image.new('L',image.size,255),(87,60),{'resource':'FDOTHER5','hp_bar_width':width},palette)
        if anchor:rules['bar'].append({'width':width,'anchor':anchor})
    if not all(rules.values()):raise ValueError('SHORT最低视觉资源正锚不足')
    return rules


def check_visual(rows,resources,hp):
    if hp not in {p['hp'] for p in paths()}:raise ValueError('SHORT视觉HP不是预声明合法分支')
    minimum=min(69*p['hp']//42+1 for p in paths() if p['hp']==hp)
    # 保证新正锚完整保留原sprite写域；删覆盖点不能成为“通过”的透明豁免。
    for rule in resources['solid']:
        anchor,mask=rule['anchor'],rule['source_mask'];box=mask.getbbox()
        if box is None or anchor['mask'].tobytes()!=mask.crop(box).tobytes() or \
            anchor['mask'].size!=mask.crop(box).size or anchor['checked_pixels']!=rule['sprite_rle_opaque_pixels']:
            raise ValueError('SHORT实体正锚不得删去原RLE写点')
    matches=[]
    for row in rows:
        raw=Path(row['path']).read_bytes()
        if common.digest(raw)!=row['sha256']:raise ValueError('SHORT WGC原件身份变化')
        with Image.open(io.BytesIO(raw)) as image:frame=normalize(image)
        found={}
        for phase,rules in resources.items():
            for rule in rules:
                if phase=='bar' and not minimum<=rule['width']<=70:continue
                if frames._anchor_match(frame,rule['anchor']):
                    found[phase]={k:v for k,v in rule.items() if k not in ('anchor','source_mask')};break
        if found:matches.append({'index':row['index'],'sha256':row['sha256'],'path':row['path'],'phases':found})
    panel=next((r for r in matches if 'panel' in r['phases']),None)
    solid=next((r for r in matches if panel and r['index']>panel['index'] and 'solid' in r['phases']),None)
    unexpected_solid=next((r for r in matches if 'solid' in r['phases']),None) if hp==42 else None
    fx=next((r for r in matches if panel and r['index']>panel['index'] and 'fx' in r['phases'] and
        (hp==42 or solid and r['index']>=solid['index'])),None)
    bar=next((r for r in matches if fx and r['index']>=fx['index'] and 'bar' in r['phases'] and
        (hp==42 or r['phases']['bar']['width']<70)),None)
    return {'ok':all((panel,fx,bar)) and (unexpected_solid is None if hp==42 else solid is not None),
        'panel':panel,'solid64':solid,'solid_required':hp<42,'unexpected_solid_on_miss':unexpected_solid,
        'weapon_fx':fx,'legal_hp_bar':bar,'classified':matches,
        'hit_or_miss_branch':'all_miss' if hp==42 else 'at_least_one_hit','whole_frame_equivalence_claimed':False}


def sound_declaration(original):
    from fd2_sfx_oracle import references
    other=Path(original)/'FDOTHER.DAT';bank=archive_entry(other.read_bytes(),64)
    count=len(common._archives(bank));candidates=[{'bank':64,'sample':i} for i in range(count)]
    refs=references(other,candidates)
    if count!=6 or any(r['source_frames']<661 for r in refs):raise ValueError('SHORT完整bank64不满足统一60ms前缀')
    return {'fdother_sha256':common.digest(other.read_bytes()),'candidates':candidates,
        'references':[{k:v for k,v in r.items() if k!='values'} for r in refs],
        'prefix_frames':661,'prefix_ms':60,'minimum_score':0.8,'identity_margin':0.05,
        'source_branch':'finalHP42 requires miss cue4; other legalHP requires hit cue3; no exact repeats or runtime bank pointer claim'}


def check_sound(original,declaration,wav,hp):
    from scipy import signal
    from fd2_sfx_oracle import references,read_recording,_match
    if declaration!=sound_declaration(original):raise ValueError('SHORT声音声明变更')
    if hp not in {p['hp'] for p in paths()}:raise ValueError('SHORT声音分支HP非法')
    metadata,values=read_recording(Path(wav));refs=references(Path(original)/'FDOTHER.DAT',declaration['candidates'])
    groups={ref['sha256']:ref for ref in refs};expected=next(r for r in refs if r['sample']==(4 if hp==42 else 3))
    divisor=math.gcd(metadata['sample_rate'],11025)
    templates={key:signal.resample_poly(ref['values'][:661],metadata['sample_rate']//divisor,11025//divisor) for key,ref in groups.items()}
    template=templates[expected['sha256']];hit=_match(values,template);local=values[hit['offset_frames']:hit['offset_frames']+len(template)]
    competitors=[{'sha256':key,'score':_match(local,candidate)['score']} for key,candidate in templates.items() if key!=expected['sha256']]
    second=max((r['score'] for r in competitors),default=-1.)
    return {'ok':hit['score']>=0.8 and hit['score']-second>=0.05,'wav':metadata,'expected_sample':expected['sample'],
        'voice_sha256':expected['sha256'],'score':hit['score'],'margin':hit['score']-second,'competitors':competitors,
        'full_bank_candidates':declaration['references'],'prefix_frames':661,'onset_frame':hit['offset_frames'],
        'exact_cue_count_or_latency_or_A7_claimed':False,'runtime_bank_pointer_claimed':False}
