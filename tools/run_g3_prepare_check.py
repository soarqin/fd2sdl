#!/usr/bin/env python3
"""G3 标题 LOAD→准备服务→酒店真实 Save；默认原版先、owned 清理后 SDL。

页面期望仅来自原 EXE/FDOTHER/FDTXT/FDICON/DATO，状态来自独立 G3 oracle。
--prepare-only 不启动窗口。P5/P6 仅输出夹具及后态取证方案，不冒称原生通过。
"""
from __future__ import annotations

import argparse
import ast
from contextlib import ExitStack
import hashlib
import json
import os
from pathlib import Path
import shutil
from types import SimpleNamespace

import fd2_g3_prepare_oracle as oracle
import fd2_g3_prepare_pages as pages
from fd2_display_requirement import windows_display_requirement
from fd2_transition_oracle import picker_image,picker_match,town_images
from run_g2_battle_check import freeze,unchanged,sha,fingerprint
from run_g4_detail_check import detail_series,series_checkpoint
from run_graphics_tests import decode_save
from run_original_battle_animation import AnimationCollectors,owned_job_required,parse_job,series_index,latest_series_frame,qpc_stamp
from run_original_battle_menu import wait_title,wait_state as resource_wait_state
from run_original_comparison import Capture,running,title_templates,compare
from run_original_end_turn import RoundOracle,round_assets
from run_original_load import require_source_unchanged,wait_measured_save
from run_original_transition import town_match


RUNTIME_SCENARIOS=tuple(name for name in oracle.SCENARIOS if name.startswith(('p1-','p2-','p3-','p4-')))
# 首个原版完整PASS的runner。仅允许本次继承适配层变化；三个执行/期望函数
# AST仍逐字结构锁定，pages/oracle/实际import其余文件继续完整SHA核验。
PRE_INHERIT_RUNNER_SHA256='8b2f75363d4b9645f931f6950fcc4011e23caa5019714a95d8c445e6404078c6'
PRE_INHERIT_EXECUTION_AST='f3cc265e23df7d415cb56a019836892aa7f38583e61d61eb56a57750f3d7197c'
# 09的P3/P4真实wait/菜单后stop已验证；这次仅增加首错诊断wrapper。
PRE_ERROR_GUARD_RUNNER_SHA256='f6b2195621764b72e16aadc2d90f9b658798a51d2991d301d6c4d73978459e06'
PRE_ERROR_GUARD_FINALIZER_AST='cb1b3b745ff70ccff151bcaa112c0877ab8c660031ec5c4f775e77f4a67aab55'


def public(value):
    """报告只含可复核声明，不把 PIL 对象或闭包序列化为伪证据。"""
    if isinstance(value,bytes):return value.hex()
    if isinstance(value,dict):return {k:public(v) for k,v in value.items()}
    if isinstance(value,(tuple,list)):return [public(v) for v in value]
    return value


def wait_state(capture,name,predicate,checkpoints,**options):
    """原资源等待完全委托；只在reraise前留首错，防finally stop覆盖它。"""
    try:
        return resource_wait_state(capture,name,predicate,checkpoints,**options)
    except BaseException as error:
        case=getattr(capture,'g3_case_record',None)
        if case is not None:
            case.setdefault('first_failure',{'checkpoint':name,
                'error':{'type':type(error).__name__,'message':str(error)}})
        raise


def record_failure(report,error):
    current={'type':type(error).__name__,'message':str(error)}
    cases=[case for case in report.get('cases',[]) if case.get('first_failure')]
    if cases:
        case=cases[-1];first=case['first_failure']
        report['first_failure']=first;report['error']=first['error']
        if current!=first['error']:
            detail={'phase':'随后采集stop/上下文清理','error':current}
            case.setdefault('secondary_errors',[]).append(detail)
            report.setdefault('secondary_errors',[]).append(detail)
    else:report['error']=current


def prepare(root,scenario):
    original=Path(root)/'original_game'
    encoded,plain,before,after,manifest=oracle.derive_fixture(original,scenario)
    assets=pages.load(original);t,_=oracle.tables(original)
    result={'original':original,'encoded':encoded,'plain':plain,'before':before,'after':after,
        'manifest':manifest,'assets':assets,'tables':t,'titles':title_templates(original/'FDOTHER.DAT'),
        'field_assets':round_assets(original),'checks':{},'steps':[],'source_rois':{}}
    tracks=[10,11,13,17] if scenario=='p3-revive' else [10,11,13,16] if scenario.startswith('p4-') else [10,11,13] if scenario.startswith('p3-') else [10,13,14]
    music=(original/'FDMUS.DAT').read_bytes()
    result['music_declaration']={'file':'FDMUS.DAT','sha256':oracle.digest(music),
        'tracks':[{ 'id':i,'sha256':oracle.digest(pages.archive_entry(music,i)),
            'size':len(pages.archive_entry(music,i))} for i in tracks],
        'source_cva':['0x25977','0x26152','0x2670e',
            '0x2a43e' if scenario.startswith('p3-') else '0x2aa00' if scenario.startswith('p4-') else '0x279bc'],
        'runtime_track_identity_validated':False,'PCM_equivalence_validated':False,
        'original_options_music_sfx':list(before['options'][2:4])}
    if scenario not in RUNTIME_SCENARIOS:
        result['runtime_protocol']={'ready':False,'reason':'本轮尚无经核实的selection后原版运行时转储入口',
            'proposal':['标题LOAD slot0，按原选择图完成确认/最终NO或必须成员拒绝',
                '不调用选择之前的410 Save冒充后态；在2b439返回/NoTown回圈的已证安全点暂停',
                'DOSBox debugger重新取得本轮selector/object base/delta，转储53bf7指向完整0xa00及count/资金/options',
                'manifest记录运行时地址、原owner、长度、输入/ROI与dump SHA；对照独立完整状态模型',
                'SDL须同一语义安全点只读转储，未知0xa00尾逐字节保护；尚未实现此取证不签native通过'],
            'hotel_save_is_not_post_state_evidence':not manifest['native_hotel_save_after_action']}
        return result
    result['towns']=[town_images(original,assets,before,i) for i in range(5)]
    checks=result['checks'];steps=result['steps'];buttons=RoundOracle(result['field_assets'],(0,0))
    def add(name,check,*,key=None,timeout=30,low=False,**declaration):
        checks[name]=(check,{'timeout':timeout,'allow_low_color_resource':low})
        steps.append({'name':name,'key':key,**declaration})
    def message(name,identity,*,key,selector=1,dynamic=None,value=None,xy=(12,119),portrait=None,timeout=30):
        expected=pages.notice(assets,identity,selector=selector,dynamic=dynamic,value=value,xy=xy,portrait=portrait)
        result['source_rois'][name]=[(expected['text'],(0,0),'text_nonzero_mask')]
        add(name,lambda f,e=expected:pages.notice_match(f,e),key=key,timeout=timeout,
            page='notice',fragment=identity,dynamic=dynamic,value=value,origin=list(xy),portrait=expected['portrait'])
    def menu(name,selector,selected,fragment,*,key=None):
        expected=pages.notice(assets,fragment,selector=selector)
        add(name,lambda f,e=expected:pages.menu_match(f,assets,selector,selected,e),key=key,
            page='service_menu',selector=selector,selected=selected,fragment=fragment)
    def confirm(name,fragment,*,key='ENTER',selector=1,dynamic=None,value=None,selected=0):
        expected=pages.notice(assets,fragment,selector=selector,dynamic=dynamic,value=value)
        result['source_rois'][name]=[(expected['text'],(0,0),'text_nonzero_mask')]
        def check(frame):
            words=pages.notice_match(frame,expected);choice=buttons.buttons(frame,selected)
            return {'kind':'prepare_confirmation','notice':words,'buttons':choice,'selected':selected} if words and choice else None
        add(name,check,key=key,page='confirmation',fragment=fragment,selected=selected,dynamic=dynamic,value=value)
    def item_page(name,state,ids,selected,*,key,first=0,selling=False):
        expected=[pages.items(assets,t,ids,selected,first=first,selling=selling,arrow_phase=phase)
            for phase in ((0,1) if first or len(ids)>6 else (0,))]
        result['source_rois'][name]=[(image,(5,112),'complete_page_arrow'+str(i)) for i,image in enumerate(expected)]
        add(name,lambda f:pages.page_match(f,expected,kind='prepare_item_page',selected=selected,source=['0x272d0','0x275e6','0x27079']),
            key=key,page='items',ids=ids,selected=selected,first=first,selling=selling,checked_pixels=26660)
    def members(name,state,selected,*,key,kind='members',raws=None,item_id=None,target_id=None,selector=1):
        expected=pages.roster(assets,t,state,selected,kind=kind,raws=raws,item_id=item_id,target_id=target_id,selector=selector)
        result['source_rois'][name]=[(image,(5,112),'complete_page_variant'+str(i)) for i,image in enumerate(expected)]
        add(name,lambda f:pages.page_match(f,expected,kind='prepare_'+kind,selected=selected,
            source=['0x2810b' if kind=='members' else '0x2825b' if kind=='equipment' else '0x2a0c2' if kind=='revive' else '0x2a694','0x27079']),
            key=key,page=kind,raws=raws or list(range(state['count'])),selected=selected,item_id=item_id,target_id=target_id,
            checked_pixels=26660)
    def town(name,selected,*,key):
        add(name,lambda f:town_match(f,result,selected),key=key,page='town',selected=selected)
    # 仅本场景最短标题LOAD，不重复B5四槽/所有城镇的历史导航。
    add('title-load-selected',lambda f:{'kind':'title_load','selected':1} if compare(result['titles'][3][1],f)['ok'] else None,key='DOWN',page='title')
    initial_picker=picker_image(assets,plain,0)
    add('title-slot0',lambda f:picker_match(f,initial_picker),key='ENTER',low=True,page='picker',slot=0)
    town('town-initial',0,key='ENTER')
    church=scenario.startswith(('p3-','p4-'));selector=4 if church else 1
    for selected in range(1,selector+1):town('town-service-'+str(selected),selected,key='LEFT')
    menu('service-greeting',selector,0,585 if church else 501,key='ENTER')
    params=manifest['parameters']
    if scenario.startswith('p1-'):
        # 26a0d: stage1 weapon array row+3..14, ff不进入商品数。
        row=t['stage_rows'][(before['stage']-1)*31:before['stage']*31]
        stock=[identity for identity in row[3:15] if identity!=255]
        if stock[0]!=params['item']:raise ValueError('G3购买夹具并非原货单首项')
        item_id=params['item'];price=oracle.word(oracle.item(t,item_id),19)
        item_page('buy-products',before,stock,0,key='ENTER')
        confirm('buy-confirm',502,dynamic=item_id+181,value=price)
        if scenario.endswith('insufficient'):
            message('buy-insufficient',504,key='ENTER',xy=(12,157))
            item_page('buy-products-after-rejection',after,stock,0,key='ENTER')
        else:
            kind=oracle.item(t,item_id)[0]
            candidates=[i for i in range(before['count']) if kind in t['profiles'][oracle.record(before,i)[32]*7:oracle.record(before,i)[32]*7+6]]
            if not candidates or candidates[0]!=params['raw']:raise ValueError('G3首收件人前置不成立')
            members('buy-recipients',before,0,key='ENTER',kind='equipment',raws=candidates,item_id=item_id)
            if scenario.endswith('full'):
                message('buy-full',506,key='ENTER',dynamic=oracle.record(before,params['raw'])[7]+1)
                item_page('buy-products-after-rejection',after,stock,0,key='ENTER')
            else:
                confirm('buy-equip-confirm',507)
                if not params['equip']:confirm('buy-equip-no',507,key='RIGHT',selected=1)
                steps.append({'collector':'start','source_cva':'0x28b41'})
                item_page('buy-products-after-commit',after,stock,0,key='ENTER')
                steps.append({'collector':'stop'})
        menu('service-returned',1,0,503,key='ESC')
    elif scenario=='p2-sell-equipped':
        menu('sell-selected',1,1,501,key='RIGHT')
        members('sell-members',before,0,key='ENTER')
        unit=oracle.record(before,params['raw']);ids=[unit[11+2*i] for i in range(8) if not unit[10+2*i]&128]
        item_page('sell-items',before,ids,0,key='ENTER',selling=True)
        item_page('sell-item1',before,ids,1,key='RIGHT',selling=True)
        identity=oracle.selected_id(unit,params['rank']);confirm('sell-confirm',508,dynamic=identity+181,value=3*oracle.word(oracle.item(t,identity),19)//4)
        steps.append({'collector':'start','source_cva':'0x28b41'})
        members('sell-members-after-commit',after,0,key='ENTER');steps.append({'collector':'stop'})
        menu('service-returned',1,1,503,key='ESC')
    elif scenario=='p2-self-give':
        for selected in range(1,4):menu('give-selected-'+str(selected),1,selected,501,key='RIGHT')
        message('give-sender-prompt',512,key='ENTER');members('give-senders',before,0,key='ENTER')
        members('give-sender2',before,2,key='DOWN')
        unit=oracle.record(before,2);ids=[unit[11+2*i] for i in range(8) if not unit[10+2*i]&128]
        item_page('give-items',before,ids,0,key='ENTER',selling=True)
        message('give-recipient-prompt',510,key='ENTER');members('give-recipients',before,0,key='ENTER')
        members('give-recipient2',before,2,key='DOWN')
        message('give-next-sender-prompt',512,key='ENTER');members('give-members-after-commit',after,0,key='ENTER')
        menu('service-returned',1,3,503,key='ESC')
    elif scenario.startswith('p3-'):
        for selected in range(1,3):menu('revive-selected-'+str(selected),4,selected,585,key='RIGHT')
        message('revive-prompt',589,key='ENTER',selector=4)
        raw=params['raw'];unit=oracle.record(before,raw);fee=oracle.word(t['fees'],2*unit[32])*unit[33]
        members('revive-candidates',before,0,key='ENTER',kind='revive',raws=[raw],selector=4)
        confirm('revive-confirm',590,selector=4,dynamic=unit[8]+1,value=fee)
        if scenario.endswith('insufficient'):
            message('revive-insufficient',504,key='ENTER',selector=4,xy=(12,157))
            message('revive-retry-prompt',589,key='ENTER',selector=4)
            members('revive-retry-candidates',before,0,key='ENTER',kind='revive',raws=[raw],selector=4)
            menu('service-returned',4,2,586,key='ESC')
        else:
            steps.append({'collector':'start','source_cva':'0x2a43e','music_tracks':[17,11]})
            message('revive-no-candidates',588,key='ENTER',selector=4,timeout=90)
            steps.append({'collector':'stop'});menu('service-returned',4,2,586,key='ENTER')
    else:
        for selected in range(1,4):menu('promote-selected-'+str(selected),4,selected,585,key='RIGHT')
        message('promote-prompt',592,key='ENTER',selector=4)
        raw=params['raw'];unit=oracle.record(before,raw);target=oracle.record(after,raw)[7]
        members('promote-candidates',before,0,key='ENTER',kind='promote',raws=[raw],target_id=target,selector=4)
        confirm('promote-confirm',594,selector=4,dynamic=unit[7]+1)
        steps.append({'collector':'start','source_cva':'0x2aa00','music_tracks':[16,11]})
        growth=t['growth'][target*11:(target+1)*11];line=1;first=True
        for stat,fragment_id in enumerate(range(490,495)):
            gain=growth[2*stat+1]
            if not gain:continue
            if line==3:line=2
            message('promotion-growth-'+str(fragment_id),fragment_id,key='ENTER',selector=4,
                value=gain,xy=(95,119+19*line),portrait=target,timeout=90 if first else 30)
            first=False;line+=1
        movement=t['promotion'][2*(target-32)+1]
        if movement:
            message('promotion-growth-596',596,key='ENTER',selector=4,value=movement,
                xy=(95,119+19*line),portrait=target)
        message('promotion-no-candidates',591,key='ENTER',selector=4)
        steps.append({'collector':'stop'});menu('service-returned',4,3,586,key='ENTER')
    town('town-after-service',selector,key='ESC')
    town('town-hotel',0,key='LEFT' if church else 'RIGHT')
    menu('hotel-greeting',0,0,585,key='ENTER');menu('hotel-save-selected',0,1,585,key='RIGHT')
    add('hotel-slot0',lambda f:picker_match(f,initial_picker),key='ENTER',low=True,page='picker',slot=0)
    steps.append({'save':0,'key':'ENTER'})
    message('hotel-save-notice660',660,key=None,selector=0)
    saved_picker=picker_image(assets,oracle.write_slot(plain,after,0),0)
    add('hotel-slot0-after-save',lambda f:picker_match(f,saved_picker),key='ENTER',low=True,page='picker',slot=0)
    menu('hotel-after-save',0,1,586,key='ESC');town('town-final',0,key='ESC')
    result['runtime_protocol']={'ready':True,'hotel_slot':0,'original_first':True,'state_bytes':oracle.FILE_SIZE,
        'pages':'原资源完整310x86商品/成员/预览；command四icon+底图ROI+消息字形/portrait；确认另核YES/NO',
        'audio_scope':'真实PCM录音；P3/P4原track17/16→11已声明但音乐身份尚无独立运行判据',
        'not_claimed':['全帧/所有原生动画帧','UI等待RNG轨迹一致','完整音乐/PCM等价','自然到达该受控夹具']}
    return result


def save_measurement(capture,prepared,save,guard,record,slot):
    require_source_unchanged(save,*guard);capture.key('ENTER')
    measured=wait_measured_save(save,*guard);encoded=save.read_bytes();stamp=save.stat().st_mtime_ns
    require_source_unchanged(save,encoded,stamp)
    snapshot=capture.artifacts/'measured.SAV';snapshot.write_bytes(encoded)
    if decode_save(snapshot)!=measured:raise AssertionError('G3明文与同一encoded快照身份不同')
    guard[:]=[encoded,stamp]  # 正式Save已发生，比较失败也不误报为未授权文件写入。
    record['completed_save']={'path':str(snapshot),'sha256':sha(snapshot),'mtime_ns':stamp,'slot':slot}
    result=oracle.check_save(prepared['plain'],measured,prepared['after'],slot)
    record['state_comparison']=result
    if not result['ok']:raise AssertionError('G3完整22987B后态不符：'+str(result))


def finalize_ready_waits(prepared,scenario):
    """原08证明HOST_W stop消费588任意键wait，后续ENTER又重新进复活。

    164e8逐字口型phase3→1；2a43e在588文本后显式画0，再16c57(0)。
    2aa00的591同理。保持录音，观察真实wait后发唯一ENTER，586菜单/四icon
    成立后才stop。该处26e38仅处理方向/确认/取消，F11/W不是任意键ACK。
    不改变P1/P2/旧prepare/exercise/SAV定义，保留各自历史证据来源。
    """
    if scenario=='p3-revive':name='revive-no-candidates-real-wait';fragment=588;source='0x2a43e'
    elif scenario.startswith('p4-'):name='promotion-no-candidates-real-wait';fragment=591;source='0x2aa00'
    else:return
    if name in prepared['checks']:raise ValueError('教会真实wait门不能重复插入')
    expected=pages.notice(prepared['assets'],fragment,selector=4)
    expected['portrait_frames']=[row for row in expected['portrait_frames'] if row[2]==3]
    prepared['checks'][name]=(lambda frame:pages.notice_match(frame,expected),
        {'timeout':30,'allow_low_color_resource':False})
    prepared['source_rois'][name]=[(expected['text'],(0,0),'text_nonzero_mask')]
    index=next(i for i,step in enumerate(prepared['steps']) if step.get('name')=='service-returned')
    if prepared['steps'][index-1].get('collector')!='stop':raise ValueError('教会原stop位置不同；不得猜测重排')
    stop=prepared['steps'].pop(index-1);index-=1
    prepared['steps'].insert(index,{'name':name,'key':None,'page':'notice_real_wait',
        'fragment':fragment,'exact_portrait_phase':3,'source_cva':[source,'0x164e8','0x16c57'],
        'additional_ACKs':0,'reason':'录音仍在运行时等真实wait blink，再发送既有唯一ENTER'})
    prepared['steps'].insert(index+2,stop)


def ready_wait_scope(collectors):
    """只借用既有collector的WGC字段；音频入口直接使用detail_series原scope。"""
    if (collectors is None or not collectors.started or not collectors.ready or
            not collectors.helpers or collectors.helpers[0].process is None):return None
    return {'directory':collectors.series,'ready':collectors.ready,'process':collectors.helpers[0].process,'after':None}


def begin_ready_wait_boundary(capture,scope,name,record):
    """只在原有588/591打开键之前取已发布行；不把gate入口latest当起点。"""
    try:
        if name not in ('revive-no-candidates','promotion-no-candidates') or 'ready_wait_boundary' in record:
            raise AssertionError('教会真实wait边界步骤错误或重复')
        if (not scope or not scope.get('ready') or not scope.get('process') or
                scope['ready'].get('pid')!=capture.process.pid or not scope['ready'].get('hwnd')):
            raise AssertionError('教会前键边界需要本轮已ready的WGC scope')
        if capture.process.poll() is not None or scope['process'].poll() is not None:
            raise RuntimeError('教会前键边界的目标或WGC helper已退出；未发键')
        sample=latest_series_frame(scope['directory'],scope['ready'],capture.process.pid)
        if sample is None:raise AssertionError('教会前键边界没有已发布原帧；未发键')
        stamp=qpc_stamp()
        if (type(stamp.get('qpc')) is not int or type(stamp.get('qpc_frequency')) is not int or
                stamp['qpc']<=0 or stamp['qpc_frequency']<=0):
            raise AssertionError('教会前键边界缺有效Windows QPC；未发键')
        if capture.process.poll() is not None or scope['process'].poll() is not None:
            raise RuntimeError('教会读取前键原帧期间目标或helper退出；未发键')
        boundary={'opens_step':name,'wait_name':name+'-real-wait','baseline':sample[0],
            'trace_index':len(capture.trace),'read_completed_qpc':stamp}
        record['ready_wait_boundary']=boundary
        return boundary
    except BaseException as error:
        record.setdefault('first_failure',{'checkpoint':name+'-key-boundary',
            'error':{'type':type(error).__name__,'message':str(error)}})
        raise


def validate_ready_wait_boundary(capture,record,name,ready):
    """只校验本次固定打开键及其前键原帧；不声明guest实际收键时刻。"""
    boundary=record.get('ready_wait_boundary',{});baseline=boundary.get('baseline',{})
    index=boundary.get('trace_index');bracket=boundary.get('key_call_qpc_bracket',{})
    stamps=[boundary.get('read_completed_qpc',{}),bracket.get('before',{}),bracket.get('after',{})]
    if (boundary.get('wait_name')!=name or
            boundary.get('opens_step') not in ('revive-no-candidates','promotion-no-candidates') or
            boundary['opens_step']+'-real-wait'!=name or
            type(index) is not int or not 0<=index<len(capture.trace) or
            baseline.get('pid')!=capture.process.pid or baseline.get('hwnd')!=ready.get('hwnd') or
            type(baseline.get('index')) is not int or type(baseline.get('wgc_timestamp_100ns')) is not int):
        raise AssertionError('教会真实wait缺少同次输入的前键边界')
    event=capture.trace[index];delivery=event.get('delivery',{})
    if (event!=boundary.get('input') or event.get('key')!='ENTER' or event.get('pid')!=capture.process.pid or
            boundary.get('trace_length_after')!=index+1 or
            delivery.get('pid')!=capture.process.pid or delivery.get('hwnd')!=ready.get('hwnd') or
            delivery.get('backend')!='PostMessageW HWND' or delivery.get('kind')!='key' or
            delivery.get('ok') is not True or delivery.get('error')!=0 or
            any(delivery.get(key)!=1 for key in ('keys','sent','release','key_down_sent','key_up_sent'))):
        raise AssertionError('教会真实wait的唯一ENTER投递或trace归属不同')
    if (any(type(stamp.get('qpc')) is not int or type(stamp.get('qpc_frequency')) is not int or
            stamp['qpc_frequency']<=0 for stamp in stamps) or
            len({stamp['qpc_frequency'] for stamp in stamps})!=1 or
            not stamps[0]['qpc']<stamps[1]['qpc']<=stamps[2]['qpc']):
        raise AssertionError('教会真实wait边界不是实际发键调用之前的同钟QPC')
    times=[stamp.get('host_monotonic') for stamp in stamps]
    if (any(type(value) not in (int,float) for value in times+[event.get('time')]) or
            not times[0]<=times[1]<=event['time']<=times[2]):
        raise AssertionError('教会真实wait的trace输入不在本次实际发键调用区间')
    return boundary


def wait_ready_series(capture,scope,name,predicate,points,*,timeout=30):
    """16c57 的短 blink 复用已 armed 的同一 WGC，逐条读 backlog，不另截图。"""
    try:
        if not scope or not scope.get('ready') or not scope.get('process'):
            raise AssertionError('教会真实wait需要本次已ready的WGC scope')
        ready=scope['ready'];process=scope['process']
        if ready.get('pid')!=capture.process.pid or not ready.get('hwnd'):
            raise AssertionError('教会真实wait的collector不是本轮owned窗口')
        if capture.process.poll() is not None or process.poll() is not None:
            raise RuntimeError('教会真实wait前目标或WGC helper已经退出')
        record=capture.g3_case_record
        boundary=validate_ready_wait_boundary(capture,record,name,ready)
        if any(row.get('key') for row in capture.trace[boundary['trace_index']+1:]):
            raise AssertionError('教会真实wait之前出现额外输入；未发ACK')
        scope=dict(scope,after=boundary['baseline'])
        result=series_checkpoint(capture,scope,name,predicate,points,timeout=timeout)
        if capture.process.poll() is not None or process.poll() is not None:
            raise RuntimeError('教会真实wait后目标或WGC helper提前退出；未发ACK')
        return result
    except BaseException as error:
        case=getattr(capture,'g3_case_record',None)
        if case is not None:
            case.setdefault('first_failure',{'checkpoint':name,
                'error':{'type':type(error).__name__,'message':str(error)}})
        raise


def bind_ready_series_points(capture,record,*,visual_only=False):
    """stop后按既有完整series_index封口；成功观察不等于整个服务已完成。"""
    points=[point for point in record['checkpoints'] if point.get('name') in
        ('revive-no-candidates-real-wait','promotion-no-candidates-real-wait')]
    if not points:return
    if visual_only:
        animation=record.get('detail_series',{})
        if (animation.get('checkpoint_PNGs_bound_to_closed_series') is not True or
                animation.get('summary',{}).get('stop_reason')!='stop' or animation.get('cleanup_error')):
            raise AssertionError('教会真实wait的仅视觉WGC没有完整normal stop')
    else:
        if record.get('collection_completed') is not True:
            raise AssertionError('教会真实wait的持续采集没有完整stop')
        animation=record['animation_capture']
    ready=animation['ready']
    frames=animation['frames'];by_index={frame['index']:frame for frame in frames}
    if len(by_index)!=len(frames):raise AssertionError('教会真实wait的closed series index重复')
    for point in points:
        boundary=validate_ready_wait_boundary(capture,record,point['name'],ready)
        if visual_only and len(capture.trace)!=boundary['trace_length_after']:
            raise AssertionError('音乐仅视觉WGC关闭期间出现额外业务键；不得取样')
        source=point.get('series_frame',{});actual=by_index.get(source.get('index'),{})
        if not source or any(source.get(key)!=actual.get(key) for key in
                ('index','pid','hwnd','path','sha256','wgc_timestamp_100ns')) or \
                source.get('pid')!=capture.process.pid or source.get('hwnd')!=ready.get('hwnd'):
            raise AssertionError('教会真实wait检查点不属于最终同一closed series')
        baseline=boundary['baseline'];closed_baseline=by_index.get(baseline['index'],{})
        if (any(baseline.get(key)!=closed_baseline.get(key) for key in
                ('index','pid','hwnd','path','sha256','wgc_timestamp_100ns')) or
                sha(Path(baseline['path']))!=baseline['sha256'] or
                source['index']<=baseline['index'] or source['wgc_timestamp_100ns']<=baseline['wgc_timestamp_100ns']):
            raise AssertionError('教会真实wait的前键原帧与closed series不同或成功帧未在其后')
        normalized=point.get('normalized_PNG',{})
        if sha(Path(source['path']))!=source['sha256'] or not normalized.get('path') or \
                sha(Path(normalized['path']))!=normalized.get('sha256'):
            raise AssertionError('教会真实wait的原PNG或归一检查图在采集期间改变')
    record['ready_wait_closed_series_bound']=True
    if visual_only:record['ready_wait_visual_only_completed']=True


def exercise(capture,prepared,save,guard,record,args,*,ready_wait_visual_only=False):
    if ready_wait_visual_only:
        opens=[i for i,step in enumerate(prepared['steps']) if step.get('name') in
            ('revive-no-candidates','promotion-no-candidates')]
        if (len(opens)!=1 or opens[0]!=len(prepared['steps'])-2 or
                prepared['steps'][opens[0]].get('key')!='ENTER' or
                prepared['steps'][-1].get('name')!=prepared['steps'][opens[0]]['name']+'-real-wait' or
                prepared['steps'][-1].get('page')!='notice_real_wait' or prepared['steps'][-1].get('key') is not None or
                any('collector' in step or 'save' in step or step.get('key')=='HOST_W' for step in prepared['steps'])):
            raise ValueError('音频仅视觉观察只接受588/591原ENTER后止于真实wait的无录音/ACK/Save前缀')
    wait_title(capture,prepared['titles'][3][0],timeout=90);require_source_unchanged(save,*guard)
    collectors=None;first_collected_key=False
    visual_scope=None
    with ExitStack() as visual_owner:
        try:
            for step in prepared['steps']:
                if step.get('collector')=='start':
                    collectors=AnimationCollectors(capture,args,record);collectors.start();first_collected_key=True
                    if record.get('native_audio'):
                        record['native_audio']['scope']='G3 service original PCM recording; music1/sfx1; no full music identity/equivalence claim'
                    continue
                if step.get('collector')=='stop':
                    active=collectors;collectors=None;active.stop();bind_ready_series_points(capture,record);continue
                if 'save' in step:
                    save_measurement(capture,prepared,save,guard,record,step['save']);continue
                if step['key']:
                    if ready_wait_visual_only and step.get('name') in ('revive-no-candidates','promotion-no-candidates'):
                        visual_scope=visual_owner.enter_context(detail_series(capture,record))
                        record['detail_series']['scope']='588/591 original ENTER to real wait; visual only; no PCM/HOST_W'
                    boundary=begin_ready_wait_boundary(capture,visual_scope if ready_wait_visual_only else ready_wait_scope(collectors),step['name'],record) \
                        if step.get('name') in ('revive-no-candidates','promotion-no-candidates') else None
                    if collectors and first_collected_key:
                        if step['key']!='ENTER':raise AssertionError('G3采集首业务键必须是已声明ENTER')
                        collectors.confirm();first_collected_key=False
                        if boundary is not None:
                            boundary['key_call_qpc_bracket']=record['animation_action']['key_call_qpc_bracket']
                    else:
                        key_before=qpc_stamp() if boundary is not None else None
                        capture.key(step['key'])
                        if boundary is not None:
                            boundary['key_call_qpc_bracket']={'before':key_before,'after':qpc_stamp()}
                        if collectors and capture.trace[-1].get('delivery',{}).get('hwnd')!=collectors.ready.get('hwnd'):
                            raise AssertionError('G3业务ACK与连续WGC不是同一本轮owned HWND')
                    if boundary is not None:
                        boundary['input']=json.loads(json.dumps(capture.trace[boundary['trace_index']]))
                        boundary['trace_length_after']=len(capture.trace)
                predicate,options=prepared['checks'][step['name']]
                if step.get('page')=='notice_real_wait':
                    wait_ready_series(capture,visual_scope if ready_wait_visual_only else ready_wait_scope(collectors),
                        step['name'],predicate,record['checkpoints'],timeout=options['timeout'])
                    if ready_wait_visual_only:
                        visual_owner.close()
                        bind_ready_series_points(capture,record,visual_only=True)
                else:
                    wait_state(capture,step['name'],predicate,record['checkpoints'],**options)
                require_source_unchanged(save,*guard)
            record['business_sequence_completed']=True
        finally:
            if collectors:
                collectors.stop()
                bind_ready_series_points(capture,record)


def write_json(path,value):path.write_text(json.dumps(public(value),ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


def inherit_original(path,prepared,args,inputs,original_before):
    """只继承已真实完成的原版端；全部页重判，SAV/来源/输入/采集文件再封口。"""
    path=Path(path).resolve();folder=path.parent;root=args.root.resolve();sealed={}
    def seal(file,expected=None):
        file=Path(file).resolve()
        if not file.is_file():raise ValueError('继承原版缺少真实文件：'+str(file))
        value=sha(file)
        if expected is not None and value!=expected:raise ValueError('继承原版文件SHA不同：'+str(file))
        sealed[str(file)]=value;return file
    report=json.loads(seal(path).read_text(encoding='utf-8'))
    if report.get('scenario')!=args.scenario or report.get('final_sources_unchanged') is not True or \
        report.get('original_before')!=original_before or report.get('display_requirement',{}).get('released') is not True:
        raise ValueError('继承原版场景/原资源/最终来源或显示恢复门不同')
    originals=[case for case in report.get('cases',[]) if case.get('kind')=='original']
    if len(originals)!=1:raise ValueError('继承报告需要唯一原版端')
    case=originals[0]
    if case.get('ok') is not True or case.get('business_sequence_completed') is not True or \
        case.get('guards')!={'sources_unchanged':True,'save_matches_completed':True,'cleanup':True}:
        raise ValueError('不能继承未完成或失败原版端')
    case_path=folder/'original/comparison.json'
    if json.loads(seal(case_path).read_text(encoding='utf-8'))!=case:raise ValueError('原版端文件与聚合收据不同')
    old_inputs=report.get('inputs',{});own=str(Path(__file__).resolve())
    def tool_map(mapping):
        return {p:v for p,v in mapping.items() if Path(p).resolve().is_relative_to(root/'tools') and
            not Path(p).resolve().is_relative_to(root/'tools/tests')}
    old_tools,current_tools=tool_map(old_inputs),tool_map(inputs)
    if old_tools.keys()!=current_tools.keys():raise ValueError('继承原版实际import闭包不同')
    for file,value in old_tools.items():
        if file==own and value in (PRE_INHERIT_RUNNER_SHA256,PRE_ERROR_GUARD_RUNNER_SHA256):
            module=ast.parse(Path(own).read_text(encoding='utf-8'))
            protected=[ast.dump(node,include_attributes=False) for node in module.body
                if isinstance(node,ast.FunctionDef) and node.name in ('prepare','exercise','save_measurement')]
            if hashlib.sha256('\n'.join(protected).encode()).hexdigest()!=PRE_INHERIT_EXECUTION_AST:
                raise ValueError('继承适配之外的原版执行/期望函数已改变')
            if value==PRE_ERROR_GUARD_RUNNER_SHA256:
                finalizer=next(node for node in module.body if isinstance(node,ast.FunctionDef) and node.name=='finalize_ready_waits')
                if hashlib.sha256(ast.dump(finalizer,include_attributes=False).encode()).hexdigest()!=PRE_ERROR_GUARD_FINALIZER_AST:
                    raise ValueError('09原版已证的教会真实wait/stop顺序已改变')
        elif value!=current_tools[file]:raise ValueError('继承原版工具/资源oracle已变化：'+file)
    for binary in (args.driver,args.dosbox):
        key=str(binary.resolve())
        if old_inputs.get(key)!=inputs.get(key) or key not in inputs:raise ValueError('继承原版driver/DOSBox身份不同')
    for name in ('controlled.SAV','expected-after-slot0.SAV'):
        file=folder/name
        if old_inputs.get(str(file.resolve())) is None:raise ValueError('原版夹具缺少冻结身份')
        seal(file,old_inputs[str(file.resolve())])
    if (folder/'controlled.SAV').read_bytes()!=prepared['encoded']:raise ValueError('继承原版夹具与当前受控来源不同')
    if json.loads(seal(folder/'fixture-manifest.json').read_text(encoding='utf-8'))!=public(prepared['manifest']):
        raise ValueError('继承原版donor/人工patch/来源manifest不同')
    processes=case.get('processes',[])
    if len(processes)!=1 or processes[0].get('cleaned') is not True or processes[0].get('forced_cleanup') is not False or processes[0].get('exit_code')!=0:
        raise ValueError('继承原版没有正常owned清理')
    process=processes[0];pid=process['pid'];work=folder/'original'
    if Path(process['command'][0]).resolve()!=args.dosbox.resolve():raise ValueError('原版执行目标不是当前指定DOSBox')
    for name in ('FD2.EXE','FDOTHER.DAT','FDTXT.DAT','FDICON.B24','DATO.DAT','FDMUS.DAT'):
        seal(work/'game'/name,original_before[name]['sha256'])
    completed=case.get('completed_save',{});snapshot=seal(completed.get('path',''),completed.get('sha256'))
    if snapshot!=work/'measured.SAV' or completed.get('slot')!=0:raise ValueError('原版量测快照不属于原报告端点')
    measured=decode_save(snapshot);state=oracle.check_save(prepared['plain'],measured,prepared['after'],0)
    if not state['ok'] or case.get('state_comparison')!=state:raise ValueError('继承原版全22987B/未知尾后态不符')
    game_save=seal(work/'game/FD2.SAV',completed['sha256'])
    if game_save.stat().st_mtime_ns!=completed.get('mtime_ns'):raise ValueError('原版末尾真实Save时间身份不同')
    expected_names=[step['name'] for step in prepared['steps'] if 'name' in step]
    business=[row for row in case.get('checkpoints',[]) if row.get('name') in prepared['checks']]
    if [row['name'] for row in business]!=expected_names:raise ValueError('原版业务页面集合/顺序不同')
    for row in business:
        file=seal(work/(row['name']+'.png'))
        from PIL import Image
        with Image.open(file) as image:hit=prepared['checks'][row['name']][0](image.convert('RGB'))
        if hit is None or row.get('pid')!=pid:raise ValueError('原版历史PNG未过当前同源资源门：'+row['name'])
    keys=[row for row in case.get('inputs',[]) if 'key' in row]
    raw_keys=[json.loads(line) for line in seal(work/'input-trace.jsonl').read_text(encoding='utf-8').splitlines()]
    deliveries=[json.loads(line) for line in seal(work/'input-delivery.jsonl').read_text(encoding='utf-8').splitlines()]
    if len(keys)!=len(raw_keys) or len(keys)!=len(deliveries):raise ValueError('原版输入原始记录数量不同')
    for row,raw,delivery in zip(keys,raw_keys,deliveries):
        receipt=row.get('delivery',{});count=2 if row['key']=='HOST_W' else 1
        if {k:v for k,v in row.items() if k!='delivery'}!=raw or delivery.get('delivery')!=receipt or \
            delivery.get('accepted') is not True or delivery.get('transport_ok') is not True or \
            receipt.get('backend')!='PostMessageW HWND' or receipt.get('pid')!=pid or \
            receipt.get('ok') is not True or receipt.get('error')!=0 or receipt.get('sent')!=count or \
            receipt.get('release')!=count or receipt.get('keys')!=count or receipt.get('key_down_sent')!=1 or \
            receipt.get('key_up_sent')!=1 or receipt.get('kind')!=('wave' if count==2 else 'key') or \
            not receipt.get('hwnd') or receipt.get('foreground_before')==receipt['hwnd'] or receipt.get('foreground_after')==receipt['hwnd'] or \
            receipt.get('noactivate_verified') is not True or receipt.get('foreground_guard_ok') is not True or \
            delivery.get('receipts')!=[receipt]:
            raise ValueError('原版输入投递/owner/释放真实收据不同')
    expected_keys=[('HOST_W' if 'collector' in step else step.get('key')) for step in prepared['steps']]
    expected_keys=[key for key in expected_keys if key];observed=[row['key'] for row in keys]
    while observed and observed[0]=='ESC':observed=observed[1:]
    if observed!=expected_keys:raise ValueError('原版输入业务顺序不同')
    for frame in case.get('animation_capture',{}).get('frames',[]):seal(frame['path'],frame['sha256'])
    if any(step.get('collector') for step in prepared['steps']):
        if case.get('collection_completed') is not True:raise ValueError('原版采集没有完整stop')
        audio=case.get('audio_capture',{}).get('wav',{});seal(audio.get('path',''),audio.get('sha256'))
        helper=case.get('series_helper',{})
        if helper.get('cleaned') is not True or helper.get('forced_cleanup') is not False or helper.get('exit_code')!=0:
            raise ValueError('原版连续WGC helper未正常清理')
        seal(case['animation_capture']['summary_path'],case['animation_capture']['summary_sha256'])
        for key in ('stdout','stderr'):seal(helper[key])
        if parse_job(Path(helper['stderr']).read_text(encoding='utf-8'))!=helper.get('job') or \
            json.loads(Path(helper['stdout']).read_text(encoding='utf-8'))!=helper.get('summary'):
            raise ValueError('原版WGC helper原始stdout/正常Job清理收据不同')
        animation=case['animation_capture']
        if series_index(work/'animation-series',helper['summary'],animation['ready'],pid)!=animation['frames']:
            raise ValueError('原版WGC实际PNG/owner/帧序/时间戳和最终采集收据不同')
        seal(work/'animation-series/frames.jsonl');seal(work/'animation-series/ready.json')
    # 引用旧来源但不启动旧PID；生产源/SDL允许变化，分别保留新旧身份。
    receipt={'source_report':str(path),'source_report_sha256':sealed[str(path)],'sealed_files':sealed,
        'original_case_reused':True,'new_original_process_started':False,'original_pass_is_not_paired_pass':True,
        'old_inputs':old_inputs,'current_inputs':dict(inputs),'old_original':report['original_before'],
        'old_production_source_files':report['source_files'],'current_source_compatible_oracle':True,
        'protected_execution_ast':PRE_INHERIT_EXECUTION_AST,'full_plaintext_rechecked_bytes':oracle.FILE_SIZE,
        'business_pngs_rechecked':len(business),'audio_identity_promoted':False,
        'error_guard_only_migration':old_tools.get(own)==PRE_ERROR_GUARD_RUNNER_SHA256}
    return json.loads(json.dumps(case)),receipt


def inherited_unchanged(receipt):
    return all(Path(file).is_file() and sha(file)==value for file,value in receipt['sealed_files'].items())


def run(args):
    root=args.root.resolve();original=(root/'original_game').resolve();output=args.artifacts.resolve()
    if output==original or output.is_relative_to(original) or output.exists():raise ValueError('G3需要新的隔离产物目录')
    binaries=[path.resolve() for path in (args.sdl,args.driver,args.dosbox) if path is not None]
    before=fingerprint(original);prepared=prepare(root,args.scenario)
    finalize_ready_waits(prepared,args.scenario)
    if fingerprint(original)!=before:raise AssertionError('G3只读准备期间来源变化')
    inputs,frozen_original,sources=freeze(root,original,binaries)
    if frozen_original!=before:raise AssertionError('G3准备与冻结之间来源变化')
    output.mkdir(parents=True);fixture=output/'controlled.SAV';fixture.write_bytes(prepared['encoded'])
    if decode_save(fixture)!=prepared['plain']:raise AssertionError('G3夹具codec往返不同')
    expected=output/'expected-after-slot0.SAV';expected.write_bytes(oracle.crypt(oracle.write_slot(prepared['plain'],prepared['after'],0)))
    write_json(output/'fixture-manifest.json',prepared['manifest']);write_json(output/'expected-state.json',prepared['after'])
    write_json(output/'music-declaration.json',prepared['music_declaration'])
    roi_directory=output/'source-rois';roi_directory.mkdir();roi_declarations=[]
    for name,variants in prepared['source_rois'].items():
        for image,position,role in variants:
            path=roi_directory/(name+'-'+role+'.png');image.save(path)
            roi_declarations.append({'checkpoint':name,'path':str(path),'sha256':sha(path),
                'position':list(position),'size':list(image.size),'role':role,
                'derived_from_original_resources':True,'observed_native_frame':False})
    write_json(output/'source-rois.json',roi_declarations)
    inputs.update({str(fixture):sha(fixture),str(expected):sha(expected)})
    report={'ok':False,'status':'ready_not_executed','scenario':args.scenario,'cases':[],
        'inputs':inputs,'original_before':before,'source_files':sources,'display_requirement':{},
        'runtime_protocol':prepared['runtime_protocol'],'steps':prepared['steps'],
        'expected_after_is_model_not_execution':True,'animation_capture_interval_ms':16,'sdl_default_client':[960,600],
        'audio_identity_passed':False,'whole_frame_equivalence_claimed':False,'shared_rng_aligned':False}
    inherited=None
    if getattr(args,'original_evidence',None):
        original_case,inherited=inherit_original(args.original_evidence,prepared,args,inputs,before)
        report['cases'].append(original_case);report['inherited_original']=inherited
    if args.prepare_only:
        report['final_sources_unchanged']=unchanged(inputs,original,before,root,sources)
        if inherited:
            report['inherited_original_final_files_unchanged']=inherited_unchanged(inherited)
            report['final_sources_unchanged'] &= report['inherited_original_final_files_unchanged']
        if not report['final_sources_unchanged']:raise AssertionError('G3 prepare-only冻结来源变化')
        write_json(output/'comparison.json',report);return report
    if not prepared['runtime_protocol']['ready']:raise ValueError('P5/P6尚无post-state运行时转储门，只允许--prepare-only')
    owned_job_required()
    try:
        with windows_display_requirement(report['display_requirement']):
            for kind in (('sdl',) if inherited else ('original','sdl')):
                if not unchanged(inputs,original,before,root,sources):raise AssertionError('G3冻结来源变化')
                if inherited and not inherited_unchanged(inherited):raise AssertionError('G3继承原版真实文件在新窗口前变化')
                work=output/kind;work.mkdir();env=os.environ.copy()
                for key in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED','FD2SDL_APP_PCM_CAPTURE_DIR',
                    'FD2SDL_APP_PCM_CAPTURE_MAX_MS','FD2SDL_APP_PCM_CAPTURE_QUIET','FD2SDL_TEST_AUDIO_QUIET'):env.pop(key,None)
                if kind=='original':
                    game=work/'game';shutil.copytree(original,game);save=game/'FD2.SAV';(work/'captures').mkdir()
                    config=work/'dosbox.conf'
                    config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                        '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n'+f'captures={(work/"captures").as_posix()}\nshow recorded filename=false\n'
                        '[render]\naspect=false\nscaler=normal2x\n[mixer]\nrate=44100\n[cpu]\ncore=normal\ncycles=fixed 20000\n'
                        '[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
                    command,cwd=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'],work
                else:
                    save=work/'FD2.SAV';mix=work/'app-mix';mix.mkdir()
                    env.update(FD2SDL_SAVE_PATH=str(save),FD2SDL_APP_PCM_CAPTURE_DIR=str(mix),
                        FD2SDL_APP_PCM_CAPTURE_MAX_MS='45000',FD2SDL_APP_PCM_CAPTURE_QUIET='1')
                    command,cwd=[str(args.sdl.resolve())],root
                save.write_bytes(prepared['encoded']);guard=[save.read_bytes(),save.stat().st_mtime_ns]
                case={'kind':kind,'ok':False,'processes':[],'inputs':[],'checkpoints':[]};report['cases'].append(case)
                try:
                    with running(command,cwd,env,work/'game.log',args.driver.resolve(),case['processes']) as process:
                        capture=Capture(args.driver.resolve(),process,work,case['inputs'])
                        capture.g3_case_record=case
                        collector_args=SimpleNamespace(driver=args.driver,sdl_audio_source='app-mix',animation_capture_interval_ms=16)
                        exercise(capture,prepared,save,guard,case,collector_args)
                    case['ok']=True
                finally:
                    case['guards']={'sources_unchanged':unchanged(inputs,original,before,root,sources),
                        'save_matches_completed':save.read_bytes()==guard[0] and save.stat().st_mtime_ns==guard[1],
                        'cleanup':len(case['processes'])==1 and all(p['cleaned'] and not p['forced_cleanup'] and p['exit_code']==0 for p in case['processes'])}
                    case['ok'] &= all(case['guards'].values());write_json(work/'comparison.json',case)
                if not case['ok']:raise AssertionError('G3端点守卫失败')
        report['ok']=len(report['cases'])==2 and all(case['ok'] for case in report['cases'])
        report['status']='business_pages_save_passed_audio_identity_pending' if report['ok'] else 'failed'
    except BaseException as error:
        record_failure(report,error)
        for case in report['cases']:
            if case.get('first_failure'):write_json(output/case['kind']/'comparison.json',case)
    finally:
        report['final_sources_unchanged']=unchanged(inputs,original,before,root,sources)
        if inherited:
            report['inherited_original_final_files_unchanged']=inherited_unchanged(inherited)
            report['final_sources_unchanged'] &= report['inherited_original_final_files_unchanged']
        report['ok'] &= report['final_sources_unchanged'] and report['display_requirement'].get('released') is True
        if not report['ok']:report['status']='failed'
        write_json(output/'comparison.json',report)
    return report


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('driver','dosbox','sdl'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--scenario',choices=oracle.SCENARIOS,required=True)
    parser.add_argument('--original-evidence',type=Path,
        help='已完整PASS原版端的同场景聚合comparison.json；只复测SDL，严格重核原资源/工具/PNG/SAV/输入')
    parser.add_argument('--prepare-only',action='store_true');args=parser.parse_args(argv)
    if not args.prepare_only and any(getattr(args,k) is None or not getattr(args,k).is_file() for k in ('sdl','driver','dosbox')):
        parser.error('G3实际运行需存在的--sdl/--driver/--dosbox')
    if args.original_evidence and any(getattr(args,k) is None or not getattr(args,k).is_file() for k in ('driver','dosbox')):
        parser.error('继承原版校验需存在的--driver/--dosbox')
    if not args.prepare_only and args.scenario not in RUNTIME_SCENARIOS:
        parser.error('本入口的P5/P6仅准备；实际运行请用tools/run_g3_deployment_check.py')
    report=run(args);print(json.dumps({k:report[k] for k in ('ok','status','scenario')},ensure_ascii=False))
    return 0 if report['ok'] or args.prepare_only else 1


if __name__=='__main__':raise SystemExit(main())
