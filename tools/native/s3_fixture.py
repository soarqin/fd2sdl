"""S3 正式输入代表的独立原资源模型；不导入 SDL 冷图或调用游戏 API。

来源：B7 实际 stage2 入场、112a5/1145a/11506、action9 @0x34818、230f2、
stage3 原 metadata/placement/script20。完整原件留本地，未知字节逐 owner 处理。
"""
from __future__ import annotations
import hashlib
from pathlib import Path
import struct
from contextlib import contextmanager

from audit_g4_render_source import archive_entries
from fd2_g3_prepare_oracle import checksum, valid_container, write_slot
from fd2_opening_oracle import build_pages
from fd2_stage1_complete_oracle import apply_script, tables, template_record
from fd2_stage2_storage import prepare as prepare_origin
from fd2_transition_oracle import ROSTER_SIZE, SLOT_BASE, SLOT_STRIDE
from run_g4_runtime_check import encode
from run_graphics_tests import decode_save
from run_original_end_turn import META, STRIDE, UNITS, round_assets
from run_original_focus import native_focus_reference
from run_original_item_list import drop_derived
from dump_fd2_fixup_table import collect_internal_offset_fixups

SCENARIOS=('stage2-turn3-storage','stage2-victory3-entry','stage12-entry-cancel-storage')


def digest(value):return hashlib.sha256(value).hexdigest()


def units(plain):return [bytearray(plain[UNITS+i*STRIDE:UNITS+(i+1)*STRIDE]) for i in range(plain[META+1])]


def derive(source,scenario):
    valid_container(source)
    if scenario not in SCENARIOS[:2] or source[META:META+3]!=bytes((1,15,2)) or source[META+9]!=6:
        raise ValueError('S3 代表只接受已验 B7 turn1/stage2/15 active/6 roster')
    changed=bytearray(source);allowed=set(range(len(source)-4,len(source)))
    for raw,record in enumerate(units(source)):
        at=UNITS+raw*STRIDE+5
        if scenario==SCENARIOS[0] and record[6]!=2:changed[at]|=4
        if scenario==SCENARIOS[1] and record[6]==0:changed[at]|=1
        allowed.add(at)
    if changed[UNITS+6*STRIDE+5]&1:raise ValueError('代表须保留 raw6 visible 的真实 action9/招募分支')
    changed=checksum(changed)
    patches=[{'offset':i,'before':a,'after':b} for i,(a,b) in enumerate(zip(source,changed)) if a!=b]
    if any(p['offset'] not in allowed for p in patches):raise AssertionError('S3 夹具越出 active flags/checksum')
    return changed,{'scenario':scenario,'controlled_fixture':True,'natural_whole_stage_claimed':False,
        'source_plain_sha256':digest(source),'derived_plain_sha256':digest(changed),'plaintext_patches':patches,
        'injection':'nonplayer flags04, two real END' if scenario==SCENARIOS[0] else 'enemy hidden only, real selection cancel result',
        'runtime_alias_or_API_injection':False}


def add_stage(data,original,stage):
    records=archive_entries((Path(original)/'FDFIELD.DAT').read_bytes())
    meta,placement=records[stage*3+1:stage*3+3]
    if len(meta)<131 or (len(meta)-131)%26 or (len(placement)-2)%6:raise ValueError('原关卡资源结构不同')
    data['stages'][stage]={'metadata':meta,'templates':[meta[i:i+26] for i in range(131,len(meta),26)],
        'placement':[list(struct.unpack_from('<HH',placement,i)) for i in range(2,len(placement),6)]}
    return data['stages'][stage]


def script(data,original,identity):
    fixes=collect_internal_offset_fixups(Path(original)/'FD2.EXE')
    obj,_,start=fixes[0x627d8+4*identity];next_obj,_,end=fixes[0x627d8+4*(identity+1)]
    if obj!=3 or next_obj!=3 or not start<end:raise ValueError('原移动脚本 fixup 不是已证连续 object3 域')
    raw=bytes(data['raw'][start:end]);cursor=1;blocks=[]
    for _ in range(raw[0]):
        mode,count=raw[cursor:cursor+2];cursor+=2
        pairs=[tuple(raw[cursor+i*2:cursor+i*2+2]) for i in range(count)];cursor+=2*count
        if any(len(p)!=2 or p[1]>3 for p in pairs):raise ValueError('原移动脚本方向/长度不同')
        blocks.append((mode,pairs))
    if cursor!=len(raw):raise ValueError('原移动脚本完整边界不同')
    return {'blocks':blocks,'va':hex(start),'sha256':digest(raw)}


def loaded_units(source,kind):
    result=units(source)
    if kind=='original':
        cache=[]
        for record in result:
            if record[7] not in cache:cache.append(record[7])
            record[2]=cache.index(record[7])
    elif kind!='sdl':raise ValueError('需明确本端 Load cache 政策')
    return result


def persistent_create(before,identity,data):
    """已发布完整112a5/1145a：写58B，原目标槽其余22B原样保留。"""
    if len(before)!=80 or not 0<=identity<32:raise ValueError('112a5有限输入域不同')
    raw=data['raw'];base=raw[0x61da1+24*identity:0x61da1+24*(identity+1)]
    growth=raw[0x620a1+11*identity:0x620a1+11*(identity+1)];level=base[2];unit=bytearray(before)
    unit[5:14]=bytes((0,2,identity,identity,0,64,base[12],64,base[13]))
    for i in range(4):unit[14+2*i:16+2*i]=bytes((128 if base[14+i]==255 else 0,base[14+i]))
    unit[22]=unit[24]=128;unit[26:30]=base[8:12];unit[30]=0;unit[31:34]=base[:2]+bytes((level,))
    unit[34:40]=bytes(6);unit[49]=255
    for at,source_at,g in ((55,18,0),(57,20,2),(62,22,4)):
        struct.pack_into('<H',unit,at,(struct.unpack_from('<H',base,source_at)[0]+level*growth[g])&65535)
    unit[59:61]=bytes((base[7],0))
    hp=(struct.unpack_from('<H',base,3)[0]+(level-1)*growth[6])&65535
    mp=(struct.unpack_from('<H',base,5)[0]+(level-1)*growth[8])&65535
    struct.pack_into('<4H',unit,64,hp,hp,mp,mp)
    struct.pack_into('<4H',unit,72,*drop_derived(unit,data['items']))
    return bytes(unit)


def settlement(source,data,kind):
    roster=bytearray(source[0x8a3:0x12a3]);count=source[META+9]
    for active in loaded_units(source,kind):
        for i in range(count):
            if active[8]!=roster[i*80+8] or active[8]==0 and active[5]&1:continue
            record=bytearray(active);record[34:40]=bytes(6);record[5]&=1
            if not record[5]&1:record[64:66]=record[66:68]
            record[68:70]=record[70:72];struct.pack_into('<4H',record,72,*drop_derived(record,data['items']))
            roster[i*80:(i+1)*80]=record
    roster[count*80:(count+1)*80]=persistent_create(roster[count*80:(count+1)*80],2,data)
    return {'stage':3,'count':count+1,'roster':bytes(roster),'resource':struct.unpack_from('<I',source,META+10)[0],
        'options':bytes(source[META+i] for i in (15,14,16,17))}


def pages(original,assets,data,source,stage,fragments):
    mapping={row[1]:row[1] for row in data['stages'][stage]['templates']}
    # 首visible/最后hidden/最后persistent；当前固定域内text ID和身份相同，仍保来源。
    for record in reversed(units(source)):mapping[record[8]]=record[7]
    roster=source[0x8a3:0x12a3]
    for i in range(source[META+9]):mapping.setdefault(roster[i*80+8],roster[i*80+7])
    all_pages=build_pages(Path(original),assets,stage_plan=((stage,max(fragments)+1),),
        text_identities_by_stage={stage:mapping})
    return [page for fragment in fragments for page in all_pages if page['fragment']==fragment]


def check_loaded_save(before,after,expected_view,kind):
    """Loaded 不沿用 fresh holes：本端 preload 的整22987B，仅cache/view有独立写集。"""
    valid_container(before);valid_container(after);expected=bytearray(before)
    expected[META+3:META+9]=expected_view[META+3:META+9]
    for i,record in enumerate(loaded_units(before,kind)):expected[UNITS+i*80:UNITS+(i+1)*80]=record
    if checksum(expected)!=after:raise AssertionError('S3 Loaded 后完整已拥有容器不同；不能使用 fresh 豁免')
    return {'checked_bytes':len(after),'loaded_unknown_bytes_preserved':True,'cache_policy':kind,'fresh_exclusions_used':False}


def check_manual(before,after,state,slot):
    if write_slot(before,state,slot)!=valid_container(after):raise AssertionError('S3 手工槽全容器/持久58B构造或未知22B不同')
    return {'slot':slot,'stage':state['stage'],'roster_count':state['count'],'checked_bytes':len(after),'unknown_bytes_preserved':True}


def append_group(records,masks,data,stage,group):
    row=data['stages'][stage]
    for index,template in enumerate(row['templates']):
        if template[21]!=group:continue
        point=row['placement'][index]
        if any(not u[5]&1 and list(u[:2])==point for u in records):
            raise ValueError('S3 固定代表 group 落点被占，不猜 nearest-free')
        record,mask=template_record(data,stage,index,point);records.append(record);masks.append(mask)


def turn_projection(source,data,kind,turn):
    if turn not in (2,3):raise ValueError('S3 turn 代表仅执行两次真实 END 到 turn3')
    records=loaded_units(source,kind);masks=[bytearray([1]*80) for _ in records]
    for unit in records:
        if any(unit[34:40]) or unit[64:66]!=unit[66:68] or unit[68:70]!=unit[70:72]:
            raise ValueError('受控 END 代表要求原15人无状态且满HP/MP；不豁免周期副作用')
        unit[5]&=127
    if turn==3:append_group(records,masks,data,2,2)
    return records,masks


def as_view(source,records,stage,turn,camera,focus,roster_count=None):
    result=bytearray(source)
    for i,record in enumerate(records):result[UNITS+i*80:UNITS+(i+1)*80]=record
    result[META:META+9]=bytes((turn,len(records),stage,*camera,*focus,focus[0]-camera[0],focus[1]-camera[1]))
    if roster_count is not None:result[META+9]=roster_count
    return checksum(result)


def check_activity(before,after,view,records,masks,metadata,*,kind,fresh_metadata=False,roster=None):
    valid_container(before);valid_container(after);expected=bytearray(before);exclusions=[]
    if kind not in ('original','sdl'):raise ValueError('活动量测须明确原版/SDL metadata owner')
    if not 131<=len(metadata)<=0x8a3 or (len(metadata)-131)%26:
        raise ValueError('metadata必须是完整原DAT模板区；不能按effective或当前active人数猜导出长度')
    expected[:len(metadata)]=metadata
    if fresh_metadata and kind=='original':
        expected[len(metadata):0x8a3]=after[len(metadata):0x8a3]
        exclusions.append({'owner':'original fresh metadata heap tail','start':len(metadata),'end':0x8a3})
    # SDL export_metadata只覆0x83+retained_templates*26，SAV旧尾仍归before。
    # 两端Loaded后由check_loaded_save整档逐字节保护，不能继承本fresh原版豁免。
    if roster is not None:expected[0x8a3:0x12a3]=roster
    for i,(record,mask) in enumerate(zip(records,masks)):
        wanted=bytearray(record)
        for j,known in enumerate(mask):
            if not known:
                wanted[j]=after[UNITS+i*80+j];exclusions.append({'owner':'fresh active ctor/cache','raw':i,'offset':j})
        expected[UNITS+i*80:UNITS+(i+1)*80]=wanted
    expected[META:META+10]=view[META:META+10]
    if checksum(expected)!=after:raise AssertionError('S3 活动整容器超出原来源/构造/视口写集')
    return {'checked_bytes':len(after),'stage':view[META+2],'turn':view[META],'active_count':len(records),
        'backend':kind,'metadata_prefix_size':len(metadata),
        'metadata_tail_policy':'explicit original fresh heap exclusion' if fresh_metadata and kind=='original' else 'preserve before SAV bytes',
        'fresh_exclusions':exclusions,'unused_active_manual_and_unknown_owners_checked':True}


@contextmanager
def resource_domain(original,stages=(2,3)):
    """只扩本工具原资源观察视图，退出还原共享自然工具的旧stage域。"""
    import fd2_natural_oracle as natural
    fields=archive_entries((Path(original)/'FDFIELD.DAT').read_bytes());old=dict(natural.STAGE_DIMENSIONS)
    try:
        for stage in stages:
            data=fields[stage*3];width,height=struct.unpack_from('<HH',data)
            if len(data)!=4+width*height*4:raise ValueError('S3 观察域原地图长度不符')
            natural.STAGE_DIMENSIONS[stage]=(width,height)
        yield
    finally:natural.STAGE_DIMENSIONS.clear();natural.STAGE_DIMENSIONS.update(old)


def entry3_projection(source,state,data,original):
    row=data['stages'][3];records=[];masks=[];camera=[0,0];focus=[0,0]
    fields=archive_entries((Path(original)/'FDFIELD.DAT').read_bytes());dimensions=struct.unpack_from('<HH',fields[9])
    def pan(x,y):
        nonlocal camera,focus
        focus=[focus[i]+(x,y)[i]-camera[i] for i in range(2)];camera=[x,y]
    def dialogue(fragment):
        nonlocal camera,focus
        from audit_g4_render_source import text_fragments
        tokens=text_fragments(archive_entries((Path(original)/'FDTXT.DAT').read_bytes())[4])[fragment]
        for index,token in enumerate(tokens[:-1]):
            if token not in (-17,-18):continue
            identity=tokens[index+1];raw=next((i for i,u in enumerate(records) if u[8]==identity and not u[5]&1),None)
            if raw is not None:
                point=list(records[raw][:2]);camera=native_focus_reference(camera,focus,point,dimensions)['camera'];focus=point
    if row['metadata'][1:3]!=bytes((7,40)) or state['count']!=7:raise ValueError('entry3固定七人部署来源不同')
    for i in range(7):
        record=bytearray(state['roster'][i*80:(i+1)*80]);record[:2]=bytes(row['placement'][40+i]);record[3:5]=bytes(2)
        record[6]=2;record[49]=255;record[34:40]=bytes(6)
        mask=bytearray([1]*80);mask[2]=0;records.append(record);masks.append(mask)
    append_group(records,masks,data,3,0);pan(4,11);apply_script(records,script(data,original,20));dialogue(0)
    append_group(records,masks,data,3,1);pan(4,0);dialogue(1)
    for record in records:record[3]=0
    point=list(records[0][:2]);camera=native_focus_reference(camera,focus,point,dimensions)['camera'];focus=point
    if len(records)!=24:raise ValueError('entry3真实来源不是七部署加十七group1')
    view=as_view(source,records,3,1,camera,focus,7)
    return view,records,masks


def prepare(original,fixture,source_report,scenario):
    origin=prepare_origin(original,fixture,'stage2-storage',source_report)
    data=tables(original);add_stage(data,original,3)
    if scenario==SCENARIOS[2]:
        plain,state,manifest=stage12_fixture(origin['source'],original);add_stage(data,original,12)
    else:plain,manifest=derive(origin['source'],scenario)
    if scenario==SCENARIOS[0]:
        plain,relocations=unblock_group2(plain,data,original)
        manifest['controlled_player_relocations']=relocations
        manifest['plaintext_patches']=[{'offset':i,'before':a,'after':b} for i,(a,b) in enumerate(zip(origin['source'],plain)) if a!=b]
        manifest['derived_plain_sha256']=digest(plain)
    assets=round_assets(original)
    result={'original':Path(original),'source':plain,'encoded':encode(plain),'manifest':manifest,'origin':origin['origin'],
        'data':data,'assets':assets,'source_path':Path(fixture),'source_report':Path(source_report)}
    if scenario==SCENARIOS[2]:result['manual_state']=state;result['story_pages']=[]
    else:result['story_pages']=pages(original,assets,data,plain,2,(4,) if scenario==SCENARIOS[0] else (7,))
    manifest.update(source_save_sha256=digest(Path(fixture).read_bytes()),source_report_sha256=digest(Path(source_report).read_bytes()),
        origin=origin['origin'],old_runtime_not_inherited_as_new_pass=True,
        stage2_turn_binding={'turn':3,'phase':2,'action':9,'source_id':'stage:02:turn:00000','caller':'0x1a813','target':'0x34818'},
        metadata_owner_contract={'prefix':'0x83 + retained DAT templates*26; not effective/active count',
            'original_fresh_tail':'only source DAT end..0x8a3 explicitly excluded as fresh heap bytes',
            'SDL_fresh_tail':'before SAV bytes strictly preserved',
            'Loaded_both_backends':'complete owned before container; no fresh tail/constructor exclusion'},
        source_family='raw6 visible→group2/story4' if scenario==SCENARIOS[0] else
            'stage12 manual LOAD→205da group0→text0→focusraw0→1197b' if scenario==SCENARIOS[2] else '11506→raw6 visible→layout/story7→112a5(2)→stage3')
    return result


def unblock_group2(source,data,original):
    """仅本代表移开占据增援源落点的玩家；nearest-free沿G1门，不猜其输出。"""
    from fd2_movement_oracle import movement_profile
    fields=archive_entries((Path(original)/'FDFIELD.DAT').read_bytes());shapes=archive_entries((Path(original)/'FDSHAP.DAT').read_bytes())
    map_data=fields[6];width,height=struct.unpack_from('<HH',map_data);attrs=shapes[2*fields[7][0]+1]
    row=data['stages'][2];reserved={tuple(row['placement'][i]) for i,t in enumerate(row['templates']) if t[21]==2}
    records=units(source);occupied={tuple(u[:2]) for u in records if not u[5]&1};result=bytearray(source);changes=[]
    for raw,record in enumerate(records):
        if record[5]&1 or tuple(record[:2]) not in reserved:continue
        if record[6]!=2:raise ValueError('增援占位不是本代表允许重排的玩家')
        profile,_=movement_profile(Path(original)/'FD2.EXE',record[32]);candidates=[]
        for y in range(height):
            for x in range(width):
                cell=struct.unpack_from('<I',map_data,4+4*(y*width+x))[0];tile=cell&1023
                if (x,y) in occupied or (x,y) in reserved or cell>>10 or 4*tile+4>len(attrs):continue
                flags,cost=attrs[4*tile:4*tile+2]
                if flags or cost>=len(profile) or not 0<profile[cost]<255:continue
                candidates.append((abs(x-record[0])+abs(y-record[1]),y,x,tile,cost,profile[cost]))
        if not candidates:raise ValueError('原地图没有受控玩家合法空位，未启动窗口')
        _,y,x,tile,cost,movement_cost=min(candidates);occupied.remove(tuple(record[:2]));occupied.add((x,y))
        result[UNITS+raw*80:UNITS+raw*80+2]=bytes((x,y))
        changes.append({'raw':raw,'before':list(record[:2]),'after':[x,y],'map_index':y*width+x,
            'terrain':tile,'terrain_cost_class':cost,'profile_cost':movement_cost,'map_entry':6,'reason':'avoid reserved group2 landing; no gameplay path claim'})
    return checksum(result),changes


def stage12_fixture(source,original):
    from fd2_transition_oracle import LATE_SAVE_SHA256
    path=Path(original)/'FD2.SAV';encoded=path.read_bytes()
    if digest(encoded)!=LATE_SAVE_SHA256:raise ValueError('stage12只接受既有固定late SAV真实31条roster来源')
    late=decode_save(path);pool=[late[0x8a3+i*80:0x8a3+(i+1)*80] for i in range(late[META+9])]
    required={0,4,13,14,17};selected=[i for i,u in enumerate(pool) if u[8] in required and not u[5]&1]
    for i,u in enumerate(pool):
        if len(selected)>=15:break
        if i not in selected and not u[5]&1:selected.append(i)
    selected.sort()
    if len(selected)!=15 or not required<={pool[i][8] for i in selected}:raise ValueError('late来源没有15条可部署且覆盖文本的真实持久记录')
    roster=bytearray(late[0x8a3:0x12a3])
    for i,source_raw in enumerate(selected):roster[i*80:(i+1)*80]=pool[source_raw]
    state={'stage':12,'count':15,'roster':bytes(roster),'resource':struct.unpack_from('<I',source,META+10)[0],
        'options':bytes(source[META+i] for i in (15,14,16,17))}
    plain=write_slot(source,state,0)
    return plain,state,{'scenario':SCENARIOS[2],'controlled_fixture':True,'natural_whole_stage_claimed':False,
        'source_plain_sha256':digest(source),'derived_plain_sha256':digest(plain),
        'plaintext_patches':[{'offset':i,'before':a,'after':b} for i,(a,b) in enumerate(zip(source,plain)) if a!=b],
        'late_roster_source':{'path':str(path),'sha256':digest(encoded),'records':[{'raw':i,'unit_id':pool[i][7],
            'text_id':pool[i][8],'sha256':digest(pool[i])} for i in selected]},
        'injection':'manual slot0 stage12 +15 source-complete late roster records; active source unchanged',
        'runtime_alias_or_API_injection':False,'post_input':'real Enter selection→Esc→result ongoing→Save; no WAIT/acted bypass'}


def entry12_projection(source,state,data,original):
    row=data['stages'][12];metadata=row['metadata'];quota,effective=metadata[1:3]
    if quota!=15 or effective!=70 or state['count']!=15:raise ValueError('entry12固定15部署/70模板来源不同')
    records=[];masks=[]
    roster=0
    for active in range(quota):
        # 1088d：stage<13的raw6保留给text2；空槽既不消费roster也不推进placement。
        # 本行是独立原指令投影，不引用SDL的gate表；memset80后只写flags=1。
        if active==6 and state['roster'][roster*80+8]!=2:
            record=bytearray(80);record[5]=1
            records.append(record);masks.append(bytearray([1]*80));continue
        record=bytearray(state['roster'][roster*80:(roster+1)*80]);record[:2]=bytes(row['placement'][effective+roster]);record[3:5]=bytes(2)
        record[6]=2;record[49]=255;record[34:40]=bytes(6)
        mask=bytearray([1]*80);mask[2]=0;records.append(record);masks.append(mask)
        roster+=1
    append_group(records,masks,data,12,0)
    from audit_g4_render_source import text_fragments
    fields=archive_entries((Path(original)/'FDFIELD.DAT').read_bytes());dimensions=struct.unpack_from('<HH',fields[36])
    tokens=text_fragments(archive_entries((Path(original)/'FDTXT.DAT').read_bytes())[13])[0];camera=[0,0];focus=[0,0]
    for index,token in enumerate(tokens[:-1]):
        if token not in (-17,-18):continue
        raw=next((i for i,u in enumerate(records) if u[8]==tokens[index+1] and not u[5]&1),None)
        if raw is not None:
            point=list(records[raw][:2]);camera=native_focus_reference(camera,focus,point,dimensions)['camera'];focus=point
    point=list(records[0][:2]);camera=native_focus_reference(camera,focus,point,dimensions)['camera'];focus=point
    if len(records)!=59:raise ValueError('entry12不是15部署+44group0的59人有效域')
    view=bytearray(as_view(source,records,12,1,camera,focus,15))
    # 未部署的说话人须从本次手动Load的持久owner取身份，不能借旧B7六人区。
    view[0x8a3:0x12a3]=state['roster']
    return checksum(view),records,masks


def preflight(p):
    """全部固定源模型/真实范围资源先核，再创建产物或原生窗口。"""
    import fd2_natural_oracle as natural
    scenario=p['manifest']['scenario'];report={'native_execution_claimed':False,'runtime_binding_claimed':False,'models':[]}
    with resource_domain(p['original'],(2,3,12)):
        if scenario==SCENARIOS[0]:
            # 与实际END路径相同reader，六人首关之外的资源错误须在开窗前拒绝。
            from run_original_natural import ProgressReader
            reader=ProgressReader(p['original'],p['source'],p['assets'],{'resource_acknowledgements':[]},
                stories=p['story_pages'],victory_fragment=-1,reward_resource_stage=2)
            report['reward_observation']={'stage':2,'portrait_ids':sorted({page['portrait_id'] for page in reader.rewards}),
                'pages':len(reader.rewards),'source':'current active + actual FDFIELD[7] templates; strict growth + complete DATO frames',
                'actual_reward_runtime_claimed':False}
        for kind in ('original','sdl'):
            if scenario==SCENARIOS[0]:
                for turn in (2,3):
                    records,_=turn_projection(p['source'],p['data'],kind,turn)
                    report['models'].append({'backend':kind,'stage':2,'turn':turn,'active_count':len(records)})
            else:
                state=p['manual_state'] if scenario==SCENARIOS[2] else settlement(p['source'],p['data'],kind)
                projector=entry12_projection if state['stage']==12 else entry3_projection
                view,records,_=projector(p['source'],state,p['data'],p['original'])
                story=pages(p['original'],p['assets'],p['data'],view,state['stage'],(0,) if state['stage']==12 else (0,1))
                report['models'].append({'backend':kind,'stage':state['stage'],'active_count':len(records),
                    'roster_count':state['count'],'entry_ACK_count':len(story),'camera':list(view[META+3:META+5]),'focus':list(view[META+5:META+7])})
                selection=view if state['stage']==12 else p['source']
                natural.selection_summary(p['original'],selection,p['assets'],0)
                if not natural.range_probe(p['original'],selection,p['assets'],0):raise ValueError('真实选择缺可处理范围资源，未启动窗口')
    return report
