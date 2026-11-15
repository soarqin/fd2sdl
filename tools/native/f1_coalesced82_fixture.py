"""原362c5/362e8、10b4e/10c50的受控合并调度；不读生产冷图。

只借S5 C22原版实际27人Save。两次81调度/四条defeated/AI隔离均为显式
夹具输入，未跑自然击杀；原SAV/DAT只读，原loaded未知字节继续归本端。
"""
from pathlib import Path
import json
import struct

from native import s3_fixture as state
from native.s5_cinematic_fixture import speech_view
from fd2_stage1_complete_oracle import tables
from audit_g4_render_source import archive_entries
from fd2_g3_prepare_oracle import valid_container
from fd2_g2_battle_oracle import crypt
from run_original_end_turn import META,UNITS
from run_original_focus import native_focus_reference
from run_g4_runtime_check import encode
from dump_fd2_fixup_table import collect_internal_offset_fixups

SCENARIO='f1-coalesced82-count27-29-capacity-xy'
SOURCE=Path('F:/fd2sdl-evidence/s5-20261114/native-entry29-C22-01/original')
SOURCE_SAVE_SHA='8851b007ecf5f4449b9a78052e9445081d2dd77215a8f39326c1762909531aa7'
SOURCE_REPORT_SHA='66e79e7732663fd04699e28065dc0abf05bc0938f339b4cb8f2c1d698f2200ce'
STATE=0x30a3


def origin(save,report):
    save=Path(save).resolve(strict=True);report=Path(report).resolve(strict=True)
    raw=save.read_bytes();report_raw=report.read_bytes()
    if state.digest(raw)!=SOURCE_SAVE_SHA or state.digest(report_raw)!=SOURCE_REPORT_SHA:
        raise ValueError('需S5 C22同源原版27人实际Save及原报告，不能用SDL或近似源')
    data=json.loads(report_raw)
    completed=data['completed_save']
    if (data.get('kind')!='original' or data.get('ok')is not True or data.get('sources_unchanged')is not True or
        Path(completed['snapshot']).resolve()!=save or completed['sha256']!=SOURCE_SAVE_SHA or
        completed['measurement']['checked_bytes']!=22987 or completed['measurement']['active_count']!=27 or
        any(not p['cleaned']or p['forced_cleanup']or p['exit_code']!=0 for p in data['processes'])):
        raise ValueError('S5原版来源owner/整容器/正常清理不符')
    evidence={str(save):SOURCE_SAVE_SHA,str(report):SOURCE_REPORT_SHA}
    # 七个原caller的实际PNG正证仍保各自SHA；不把它们计作本次82运行。
    for batch in data['teleport_batches']:
        for row in batch['gate']['matched']:
            path=Path(row['path']).resolve(strict=True)
            if state.digest(path.read_bytes())!=row['sha256']:raise ValueError('C22原传送PNG身份改变')
            evidence[str(path)]=row['sha256']
    if len(evidence)!=9 or len(data['resource_acknowledgements'])!=29:
        raise ValueError('C22来源七caller/29页ACK不完整')
    return valid_container(crypt(raw)),evidence


def inject(plain,data):
    """正常有效字段的受控前缀，明确不等于自然双杀已跑。"""
    if (plain[META:META+3]!=bytes((1,27,29))or plain[META+9]!=31 or plain[STATE+16]or
        plain[3:6]!=bytes((255,82,0))):raise ValueError('原27人/turn1/state0/首槽ff5200域改变')
    result=bytearray(plain);row=data['stages'][29]
    if plain[:len(row['metadata'])]!=row['metadata']:raise ValueError('C22已拥有metadata不等原资源')
    if any(plain[UNITS+i*80+5]&1 for i in(0,1,20)):
        raise ValueError('原20bf5须raw0/1/20均可见，不能提前进入结果剧情')
    for actor in(23,24):
        unit=plain[UNITS+80*actor:UNITS+80*(actor+1)]
        if unit[49:52]!=bytes((2,81,0)):raise ValueError('两名officer不是原81 defeated callback')
    for actor in(25,26):
        if plain[UNITS+80*actor+49]!=255:raise ValueError('两guard不可无callback抑制')
    for actor,unit in enumerate(state.units(plain)):
        if any(unit[34:40])or unit[64:66]!=unit[66:68]or unit[68:70]!=unit[70:72]:
            if not(unit[5]&1):raise ValueError('受控END要求可见角色满HP/MP无计时状态')
        if unit[6]!=2:result[UNITS+actor*80+5]|=4
    for actor in(23,24,25,26):
        result[UNITS+actor*80+5]|=1;result[UNITS+actor*80+64:UNITS+actor*80+66]=bytes(2)
    # 两次原81：state16 += 1；metadata[3] = BYTE(turn+1)，不追加group。
    for _ in range(2):result[STATE+16]=(result[STATE+16]+1)&255;result[3]=(result[META]+1)&255
    indices=[i for i,t in enumerate(row['templates'])if t[21]==2]
    if indices!=[9,10]or any(row['templates'][i][17]!=0 for i in indices):raise ValueError('原group2两个模板域改变')
    for index in indices:result[131+26*index+17]=8  # t+17→unit+0x34；behavior8无移动/acted/common tail。
    result=state.checksum(result)
    allowed={3,STATE+16,*range(len(plain)-4,len(plain))}
    allowed.update(UNITS+i*80+5 for i,u in enumerate(state.units(plain))if u[6]!=2)
    for i in(23,24,25,26):allowed.update((UNITS+i*80+64,UNITS+i*80+65))
    allowed.update(131+26*i+17 for i in indices)
    patches=[{'offset':i,'before':a,'after':b}for i,(a,b)in enumerate(zip(plain,result))if a!=b]
    if any(r['offset']not in allowed for r in patches):raise AssertionError('夹具越过声明写集')
    return result,patches


def derive(original,save=SOURCE/'s5-29-entry-active.SAV',report=SOURCE/'comparison.json'):
    original=Path(original).resolve(strict=True);plain,evidence=origin(save,report)
    data=tables(original);state.add_stage(data,original,29)
    if collect_internal_offset_fixups(original/'FD2.EXE')[0x51b19+29*4][2]!=0x20bf5:
        raise ValueError('原stage29结果分派不再是完整20bf5')
    changed,patches=inject(plain,data)
    row=data['stages'][29];size=len(row['metadata']);row['metadata']=bytes(changed[:size])
    row['templates']=[row['metadata'][i:i+26]for i in range(131,size,26)]
    manifest={'scenario':SCENARIO,'controlled_fixture':True,'natural_whole_stage_claimed':False,
        'natural_kills_executed':False,'runtime_alias_or_API_injection':False,
        'source_save':str(Path(save).resolve()),'source_report':str(Path(report).resolve()),
        'source_save_sha256':SOURCE_SAVE_SHA,'source_report_sha256':SOURCE_REPORT_SHA,
        'source_plain_sha256':state.digest(plain),'derived_plain_sha256':state.digest(changed),
        'source_guards':evidence,'plaintext_patches':patches,
        'controlled_prefix':{'defeated_with81':[23,24],'defeated_without_callback':[25,26],
            'AI_ineligible_flags04':'existing side0 only','isolated_group2_AI':{'templates':[9,10],'template_offset':17,'unit_offset':52,'before':0,'after':8},
            'state16':2,'turn':1,'turn_slot0':[2,82,0],'active_count':27,
            'source81':'0x362c5..0x362e8; state16 BYTE increment then first turn slot BYTE=turn+1',
            'defeated_inputs':'HP0/hidden only; no attack/reward/RNG/natural-kill equivalence claim'},
        'new_runtime_domain':{'callback82_state':2,'append_group':2,'count_after':29,
            'teleport_raw':[22,29,30],'inactive_xy_only':[29,30],
            'formal_binding':'stage.029.action.082 via turn2/phase0; original1a813',
            'SAV_inactive_tail':'10010/Save count-prefix owner; inactive29/30 xy stays in runtime heap, not copied into SAV'},
        'result_prerequisite':{'source_target':'0x20bf5..0x20c6f via original51b19[29] fixup',
            'raw0_1_20_visible':True,'outcome':'preserves fresh process ongoing latch; no HP test',
            'runtime_latch_dumped':False,'formal_phase':'CONTINUE turn1/phase2; first END scans turn1 phase0 then Round2; second END scans turn2 phase0 callback82 then Round3',
            'source_order':'1a552 push0;1a554 call1a813;1a5b9 inc53BEF; later phase2 dispatch'}}
    if any(state.digest(Path(path).read_bytes())!=value for path,value in evidence.items()):
        raise ValueError('C22来源在准备期间改变')
    dimensions=struct.unpack_from('<HH',archive_entries((original/'FDFIELD.DAT').read_bytes())[87])
    return {'original':original,'source':changed,'encoded':encode(changed),'data':data,'manifest':manifest,
        'dimensions':dimensions,'source_evidence_paths':list(map(Path,evidence)),'manual_state':{'stage':29}}


def projection(p,kind):
    records=state.loaded_units(p['source'],kind);masks=[bytearray([1]*80)for _ in records]
    phase_flags(records)  # phase1 status清理先于目标phase0 callback82。
    camera=list(p['source'][META+3:META+5]);focus=list(p['source'][META+5:META+7])
    def pan(x,y):
        nonlocal camera,focus
        focus=[focus[0]+x-camera[0],focus[1]+y-camera[1]];camera=[x,y]
    pan(16,1);camera,focus=speech_view(p['original'],records,29,(4,),camera,focus)
    story_records=[bytearray(u)for u in records];pan(16,14);calls=[]
    def transfer(actor,x,y):
        nonlocal camera,focus
        camera=native_focus_reference(camera,focus,[x,y],p['dimensions'])['camera'];focus=[x,y]
        calls.append({'source_pc':'0x362e8 state2','actor':actor,'destination':[x,y],'draw':[x,y],
            'camera':list(camera),'focus':list(focus),'before_records':[bytearray(u)for u in records]})
        if actor<len(records):records[actor][:2]=bytes((x,y))
    transfer(22,22,18)
    state.append_group(records,masks,p['data'],29,2)
    if len(records)!=29:raise ValueError('原group2追加不是27→29')
    records[22][52]=(records[22][52]&0xf0)|0
    transfer(29,21,18);transfer(30,23,18)
    phase_flags(records)
    point=list(records[0][:2]);camera=native_focus_reference(camera,focus,point,p['dimensions'])['camera'];focus=point
    view=state.as_view(p['source'],records,29,3,camera,focus)
    return {'records':records,'masks':masks,'view':view,'teleports':calls,'story_records':story_records}


def check_activity(p,before,after,view,kind):
    expected=p['expected'][kind]
    if view[META:META+3]!=bytes((3,29,29))or after[STATE+16]!=2:raise AssertionError('82人数/turn/state16不符')
    result=state.check_activity(before,after,view,expected['records'],expected['masks'],
        p['data']['stages'][29]['metadata'],kind=kind)
    result.update(loaded_owned_records_checked=27,new_constructor_records=2,
        inactive29_95_SAV_tail_preserved=True,inactive_runtime_xy_dumped=False)
    return result


def first_end(p,kind):
    records=state.loaded_units(p['source'],kind)
    phase_flags(records)
    point=list(records[0][:2]);camera=native_focus_reference(list(p['source'][META+3:META+5]),
        list(p['source'][META+5:META+7]),point,p['dimensions'])['camera']
    return {'view':state.as_view(p['source'],records,29,2,camera,point),'records':records,
        'masks':[bytearray([1]*80)for _ in records]}


def phase_flags(records):
    # 原1a866@1a941无条件调用完整1db65；两条演出/无演出尾均将所有HP0 flags精确赋1。
    # 本夹具HP0全部已hidden，无旋转方向写；1a30b的全表acted清除仍独立执行。
    for unit in records:
        unit[5]&=127
        if unit[64:66]==bytes(2):unit[5]=1


def check_first_end(p,before,after,view,kind):
    e=first_end(p,kind)
    if after[META:META+3]!=bytes((2,27,29))or after[STATE+16]!=2 or after[3:6]!=bytes((2,82,0)):
        raise AssertionError('第一END提前分派82或改变原81调度')
    return state.check_activity(before,after,view,e['records'],e['masks'],p['data']['stages'][29]['metadata'],kind=kind)
