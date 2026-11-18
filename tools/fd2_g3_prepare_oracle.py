"""G3独立准备状态／夹具与22987B容器模型；不导入SDL冷JSON、不启动窗口。

依据26152/2872b/28cbd/28f65/2a43e/2aa00/2ac7d/2af28及2968d。
完整80B从原START/late来源复制；受控修改逐字节登记，不称自然到达。
UI等待RNG、画面／声音及原指令执行不由本模型验证。
"""
from __future__ import annotations

import copy
import hashlib
from pathlib import Path
import struct
import sys

from fd2_transition_oracle import (derive_prepare_fixture,FILE_SIZE,SLOT_BASE,SLOT_STRIDE,
    ROSTER_SIZE,LATE_SAVE_SHA256)
from fd2_g2_battle_oracle import crypt,EXE_SHA256
from fd2_reward_oracle import _stat_scale,GROWTH_SHA256
from fd2_save_checks import decode_save

sys.path.insert(0,str(Path(__file__).resolve().parent/'static_data'))
from verify_unit_tables import source_range

SCENARIOS=('p1-buy-equip-yes','p1-buy-equip-no','p1-buy-insufficient','p1-buy-full',
    'p2-sell-equipped','p2-self-give','p3-revive','p3-revive-insufficient',
    'p4-promote-normal','p4-promote-special90','p5-select17-final-no','p5-select21-final-no',
    'p6-notown22-missing24')


def digest(data):return hashlib.sha256(data).hexdigest()
def word(data,at):return struct.unpack_from('<H',data,at)[0]
def signed(value):return value if value<0x80000000 else value-0x100000000


def valid_container(data):
    if not isinstance(data,bytes) or len(data)!=FILE_SIZE or sum(data[:-4])!=int.from_bytes(data[-4:],'little'):
        raise ValueError('G3需要checksum有效的完整22987B容器')
    return data


def checksum(data):
    if len(data)!=FILE_SIZE:raise ValueError('G3容器长度不同')
    result=bytearray(data);result[-4:]=sum(result[:-4]).to_bytes(4,'little');return bytes(result)


def write_slot(source,state,index):
    valid_container(source)
    if not 0<=index<4 or not 1<=state['stage']<=29 or not 1<=state['count']<=32 or \
        len(state['roster'])!=ROSTER_SIZE or len(state['options'])!=4 or not 0<=state['resource']<=0xffffffff:
        raise ValueError('G3完整手工状态或slot参数不同')
    result=bytearray(source);at=SLOT_BASE+index*SLOT_STRIDE
    result[at:at+ROSTER_SIZE]=state['roster']
    result[at+ROSTER_SIZE:at+ROSTER_SIZE+10]=bytes((state['stage'],state['count']))+struct.pack('<I',state['resource'])+state['options']
    return checksum(result)


def tables(original):
    raw=(Path(original)/'FD2.EXE').read_bytes()
    if digest(raw)!=EXE_SHA256:raise ValueError('G3原EXE身份不同')
    specs={'items':(0x602ad,215*23),'profiles':(0x6188a,29*7),'growth':(0x620a1,68*11),
        'promotion':(0x615fe,36*2),'special':(0x523d5,18),'fees':(0x52397,62),
        'notown':(0x523e7,30),'stage_rows':(0x6238d,26*31)}
    result={key:source_range(raw,va,size) for key,(va,size) in specs.items()}
    if digest(result['growth'])!=GROWTH_SHA256 or digest(result['promotion'])!='980123c50d9cb82dd221ce30dd6981505e52f8b6ff08e71c5a46db65fa8b1870':
        raise ValueError('G3原成长／转职表身份不同')
    return result,{'exe_sha256':EXE_SHA256,'tables':{key:{'cva':hex(va),'size':size,'sha256':digest(result[key])}
        for key,(va,size) in specs.items()}}


def record(state,raw):
    if not 0<=raw<state['count']:raise ValueError('G3原始成员索引越界')
    return state['roster'][raw*80:(raw+1)*80]


def inventory_count(unit):return sum(not unit[10+2*i]&128 for i in range(8))
def selected_id(unit,rank):
    if not 0<=rank<inventory_count(unit):raise ValueError('G3列表rank越界')
    return unit[11+rank*2]
def item(t,item_id):
    if not 0<=item_id<215:raise ValueError('G3所用原物品记录未确认')
    return t['items'][item_id*23:(item_id+1)*23]


def remove(unit,slot):
    if not 0<=slot<8:raise ValueError('G3原槽越界')
    result=bytearray(unit);result[10+2*slot:24]=unit[12+2*slot:26];result[24]=128;return bytes(result)


def append(unit,item_id):
    for i in range(8):
        if unit[10+2*i]&128:
            result=bytearray(unit);result[10+2*i:12+2*i]=bytes((0,item_id));return bytes(result)
    raise ValueError('G3追加之前必须通过满包分支')


def equip(unit,slot):
    result=bytearray(unit);new=unit[11+slot*2]
    for i in range(8):
        if unit[10+i*2]&64 and (new<128)==(unit[11+i*2]<128):result[10+i*2]=0
    result[10+slot*2]=64;return bytes(result)


def recompute(unit,t):
    ap,dp,dx=(struct.unpack_from('<h',unit,at)[0] for at in (0x37,0x39,0x3e))
    hit=ev=dx+(15 if unit[0x24] else 0)
    for slot in range(8):
        if unit[10+slot*2]&64:
            ia,ih,idp,ie=struct.unpack_from('<4h',item(t,unit[11+slot*2]),1)
            ap+=ia;dp+=idp;hit+=ih;ev+=ie
    if unit[0x22]:ap=_stat_scale(ap)
    if unit[0x23]:dp=_stat_scale(dp)
    result=bytearray(unit);struct.pack_into('<4H',result,0x48,*(value&65535 for value in (ap,dp,hit,ev)));return bytes(result)


def _unit_write(state,raw,after,trace,name,cva):
    before=record(state,raw);roster=bytearray(state['roster']);roster[80*raw:80*(raw+1)]=after;state['roster']=bytes(roster)
    trace.append({'step':name,'source_cva':cva,'raw':raw,'changes':[{'offset':i,'before':a,'after':b}
        for i,(a,b) in enumerate(zip(before,after)) if a!=b]})


def _money(state,amount,trace,*,subtract):
    old=state['resource'];state['resource']=(old-amount if subtract else old+amount)&0xffffffff
    trace.append({'step':'money_subtract' if subtract else 'money_add','source_cva':'0x26bcc' if subtract else '0x26ab5',
        'before':old,'after':state['resource'],'amount':amount,'before_rolling_animation':True})


def model_action(before,parameters,t):
    state=copy.deepcopy(before);trace=[];kind=parameters['kind'];outcome='completed'
    if kind=='buy':
        raw=parameters['raw'];identity=parameters['item'];entry=item(t,identity);price=word(entry,19)
        if entry[0]<32 and entry[0] not in t['profiles'][record(state,raw)[32]*7:record(state,raw)[32]*7+6]:
            raise ValueError('G3夹具不是原职业许可候选')
        if signed(state['resource'])<price:outcome='insufficient'
        elif inventory_count(record(state,raw))==8:outcome='full'
        else:
            _unit_write(state,raw,append(record(state,raw),identity),trace,'buy_append','0x28a78')
            if parameters.get('equip'):
                slot=inventory_count(record(state,raw))-1
                _unit_write(state,raw,equip(record(state,raw),slot),trace,'equipment_flags','0x28b0b')
                _unit_write(state,raw,recompute(record(state,raw),t),trace,'recompute_sender','0x28b14')
            _money(state,price,trace,subtract=True)
    elif kind=='sell':
        raw=parameters['raw'];slot=parameters['rank'];identity=selected_id(record(state,raw),slot)
        _money(state,3*word(item(t,identity),19)//4,trace,subtract=False)
        _unit_write(state,raw,remove(record(state,raw),slot),trace,'sell_remove','0x28ee1')
        _unit_write(state,raw,recompute(record(state,raw),t),trace,'recompute_sender','0x28eea')
    elif kind=='give':
        sender,recipient,slot=(parameters[k] for k in ('sender','recipient','rank'))
        if inventory_count(record(state,recipient))==8:outcome='full'
        else:
            identity=selected_id(record(state,sender),slot);item(t,identity)
            _unit_write(state,sender,remove(record(state,sender),slot),trace,'give_remove','0x291f5')
            _unit_write(state,recipient,append(record(state,recipient),identity),trace,'give_append','0x29204')
            _unit_write(state,sender,recompute(record(state,sender),t),trace,'recompute_sender','0x2920d')
    elif kind=='revive':
        raw=parameters['raw'];unit=record(state,raw);profile=unit[32]
        if not unit[5]&1:raise ValueError('G3复活夹具不是原hidden候选')
        if profile not in {*range(1,7),*range(8,26),28}:raise ValueError('G3复活profile未在来源域确认')
        fee=word(t['fees'],profile*2)*unit[33]
        if signed(state['resource'])<fee:outcome='insufficient'
        else:
            _money(state,fee,trace,subtract=True)
            changed=bytearray(unit);changed[5]=0
            _unit_write(state,raw,bytes(changed),trace,'revive_flags','0x2a43e')
            changed[64:66]=changed[66:68]
            _unit_write(state,raw,bytes(changed),trace,'revive_hp','0x2a43e')
    elif kind=='promote':
        raw=parameters['raw'];unit=record(state,raw);old=unit[7]
        if old>=18 or old==7 or unit[33]<20:raise ValueError('G3转职前置不成立')
        find=lambda wanted:next((i for i in range(inventory_count(unit)) if selected_id(unit,i)==wanted),None)
        special=find(t['special'][old]);priority=find(90) if old==9 else None
        target=52 if priority is not None else old+50 if special is not None else old+32
        slot=priority if priority is not None else special if target>=50 else None
        if slot is not None:_unit_write(state,raw,remove(unit,slot),trace,'promotion_consume','0x2aa00')
        changed=bytearray(record(state,raw));row=t['promotion'][(target-32)*2:(target-31)*2]
        changed[32]=row[0];changed[7]=target
        _unit_write(state,raw,bytes(changed),trace,'promotion_identity','0x2aa00')
        growth=t['growth'][target*11:(target+1)*11]
        for i,offset in enumerate((0x37,0x39,0x3e,0x42,0x46)):
            if growth[i*2+1]:
                struct.pack_into('<H',changed,offset,(word(changed,offset)+growth[i*2+1])&65535)
                _unit_write(state,raw,bytes(changed),trace,'growth_after_ack','0x1e529')
        if row[1]:
            changed[59]=(changed[59]+row[1])&255
            _unit_write(state,raw,bytes(changed),trace,'movement_after_ack','0x2ac7d')
        _unit_write(state,raw,recompute(record(state,raw),t),trace,'recompute_sender','0x2ac7d')
        changed=bytearray(record(state,raw));changed[33]=1;changed[60]=0;changed[64:66]=changed[66:68];changed[68:70]=changed[70:72]
        _unit_write(state,raw,bytes(changed),trace,'promotion_reset','0x2ac7d')
    elif kind=='deployment':
        selected=set(parameters['selected']);needed=19 if state['stage']>26 else 15
        if len(selected)!=needed or not selected<=set(range(1,state['count'])):raise ValueError('G3选阵位图不是已选满原同伴数')
        units=[record(state,i) for i in range(state['count'])]
        order=[0]+[i for i in range(1,state['count']) if i in selected]+[i for i in range(1,state['count']) if i not in selected]
        state['roster']=b''.join(units[i] for i in order)+state['roster'][80*state['count']:]
        trace.append({'step':'stable_partition','source_cva':'0x2b777','raw_order':order,'full_roster_tail_preserved':True})
        stage=state['stage'];required={18:[16],20:[21],21:[24],22:[24],25:[9,29]}.get(stage,[9] if stage in (17,19) or stage>25 else [])
        if stage==16:raise ValueError('G3 stage16还需33499(18)动态谓词，不能无源猜定')
        prefix=[record(state,i)[8] for i in range(1,needed+1)]
        missing=[identity for identity in required if identity not in prefix]
        if missing:outcome='required_member_missing';trace.append({'step':'required_member_reject','source_cva':'0x2b439','missing':missing})
        else:
            fronts=[9] if stage in (17,19) or stage>25 else {20:[21],21:[24],22:[24],25:[9,29]}.get(stage,[])
            for identity in fronts:
                units=[record(state,i) for i in range(state['count'])];index=max(i for i in range(1,state['count']) if units[i][8]==identity)
                order=[0,index]+[i for i in range(1,state['count']) if i!=index]
                state['roster']=b''.join(units[i] for i in order)+state['roster'][80*state['count']:]
                trace.append({'step':'mandatory_front','source_cva':'0x2b843','text_id':identity,'raw_order':order})
            outcome='accepted' if parameters.get('confirm') else 'final_no_sorted_retained'
    else:raise ValueError('G3尚未建立该服务模型')
    return state,{'outcome':outcome,'business_write_trace':trace,'ui_rng_or_timing_claimed':False,'original_runtime_executed':False}


def derive_fixture(original,scenario):
    original=Path(original)
    if scenario not in SCENARIOS:raise ValueError('G3未知场景')
    encoded,base,state,base_manifest=derive_prepare_fixture(original);state=copy.deepcopy(state);t,source_meta=tables(original)
    if scenario.startswith('p1-'):
        identity=t['stage_rows'][3];parameters={'kind':'buy','raw':0,'item':identity,'equip':scenario.endswith('-yes')}
        if identity!=128 or word(item(t,identity),19)!=50:raise ValueError('G3 stage1真实货单首项变化')
        state['resource']=49 if scenario.endswith('insufficient') else 50
        if scenario.endswith('full'):
            roster=bytearray(state['roster']);roster[:80]=append(record(state,0),192);state['roster']=bytes(roster)
    elif scenario=='p2-sell-equipped':parameters={'kind':'sell','raw':0,'rank':1}
    elif scenario=='p2-self-give':parameters={'kind':'give','sender':2,'recipient':2,'rank':0}
    elif scenario.startswith('p3-'):
        parameters={'kind':'revive','raw':2};roster=bytearray(state['roster']);roster[2*80+5]=1;state['roster']=bytes(roster)
        state['resource']=99 if scenario.endswith('insufficient') else 100
    elif scenario.startswith('p4-'):
        raw=1 if scenario.endswith('special90') else 2;parameters={'kind':'promote','raw':raw};unit=bytearray(record(state,raw));unit[33]=20
        if raw==1:unit=bytearray(append(bytes(unit),90))
        roster=bytearray(state['roster']);roster[raw*80:(raw+1)*80]=unit;state['roster']=bytes(roster)
    else:
        late=decode_save(original/'FD2.SAV')
        if digest((original/'FD2.SAV').read_bytes())!=LATE_SAVE_SHA256:raise ValueError('G3 late完整记录来源变化')
        state['roster']=late[0x8a3:0x12a3];state['count']=21 if 'select21' in scenario else 17
        state['stage']=27 if 'select21' in scenario else 22 if scenario.startswith('p6-') else 1
        selected=[1,*range(3,21)] if state['count']==21 else list(range(2,17))
        if scenario.startswith('p6-'):selected=[1,*range(3,17)]
        parameters={'kind':'deployment','selected':selected,'confirm':False}
    fixture=write_slot(base,state,0);expected,action=model_action(state,parameters,t)
    original_plain=decode_save(original/'START.SAV')
    def reason(offset):
        if offset>=FILE_SIZE-4:return '完整22987B additive checksum；由全部前字节重算'
        relative=offset-SLOT_BASE
        if relative>=ROSTER_SIZE:
            field=relative-ROSTER_SIZE
            if field==0:return '受控stage；源26152/523e7 NoTown或stage1原town行'
            if field==1:return '受控完整记录前缀人数17/21或原五人来源；不截断0xa00尾'
            if 2<=field<6:return '原售价/费用的精确相等或差1资金前置；CMP/JGE@28946/2a5b4'
            return '原START准备options完整复制，顺序camera/effect/music/SFX'
        raw,within=divmod(relative,80)
        if scenario.startswith('p3-') and (raw,within)==(2,5):return '2a07a原hidden候选；只置flags1，原非零HP/MP/状态保留'
        if scenario.startswith('p4-') and raw==parameters['raw']:
            if within==33:return '2ae0e原level>=20候选门；受控设等值20，其它80B保留'
            if 10<=within<26:return '1bb8c首hidden追加原物品90；2ae0e unit9优先52路径'
        if scenario=='p1-buy-full' and raw==0 and within in (24,25):return '1bb8c首hidden追加stage1真实货单192，使原count==8容量门成立'
        if parameters['kind']=='deployment':return '完整late0xa00 roster逐字复制，含未用记录/尾；不造角色构造值'
        return '原START raw0..3或late raw5完整80B复制；详见base_fixture supplement span'
    patches=[{'offset':i,'before':a,'after':b,'reason':reason(i)}
        for i,(a,b) in enumerate(zip(original_plain,fixture)) if a!=b]
    allowed=set(range(SLOT_BASE,SLOT_BASE+ROSTER_SIZE+10))|set(range(FILE_SIZE-4,FILE_SIZE))
    if any(p['offset'] not in allowed for p in patches):raise ValueError('G3夹具写出原slot0协议域')
    manifest={'scenario':scenario,'controlled_fixture':True,'natural_arrival_claimed':False,
        'base_fixture':base_manifest,'source':source_meta,'record_sources':'START roster0..3 + late raw5; deployment uses complete late0xa00 roster',
        'parameters':parameters,'plaintext_patches':patches,
        'encrypted_patches':[{'offset':i,'before':a,'after':b,'reason':'已证save_xor_crypt对上述完整明文修改编码'}
            for i,(a,b) in enumerate(zip((original/'START.SAV').read_bytes(),crypt(fixture))) if a!=b],
        'fixture_plain_sha256':digest(fixture),'fixture_sha256':digest(crypt(fixture)),
        'expected_state_roster_sha256':digest(expected['roster']),'expected_action':action,
        'notown':bool(t['notown'][state['stage']]),'native_hotel_save_after_action':state['stage']==1,
        'native_protocol':'title LOAD slot0 → service → hotel four-slot Save' if state['stage']==1 else
            'source/state fixture only: original410 optional Save precedes selection; post-sort hotel measurement not yet available',
        'unknown_slot_tails_and_active_protected':True,'shared_rng_trajectory_claimed':False}
    return crypt(fixture),fixture,state,expected,manifest


def check_save(before,measured,state,slot):
    expected=write_slot(before,state,slot);valid_container(measured)
    mismatches=[i for i,(a,b) in enumerate(zip(expected,measured)) if a!=b]
    return {'ok':not mismatches,'checked_container_bytes':FILE_SIZE,'mismatch_count':len(mismatches),
        'mismatch_offsets':mismatches[:32],'slot':slot,'unknown_tails_protected':True,'native_execution_claimed':False}
