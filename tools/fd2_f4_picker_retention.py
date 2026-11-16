"""F4：复用正常LOAD/owned/WGC，只取成员画像周期及导航后保留反馈。

全页期望沿279bc/1956b/26996/27d33/26eda/27079/16559及原资源组合；
不设共享RNG/钟轨迹，不以ROI、容差或固定等待秒数代全320x200检查点。
"""
from __future__ import annotations
import copy
import hashlib
import struct

import fd2_g3_prepare_pages as pages
from run_g4_detail_check import detail_series,series_checkpoint
from run_original_comparison import archive_entry
from run_original_end_turn import decode_lmi
from run_original_load import require_source_unchanged


def narrow(prepared):
    steps=prepared['steps'];ends=[i for i,s in enumerate(steps) if s.get('name')=='equipment-command2']
    if len(ends)!=1:raise ValueError('画像反馈缺唯一正常EQT菜单入口')
    prefix=copy.copy(prepared);prefix['steps']=copy.deepcopy(steps[:ends[0]+1])
    if (prefix['steps'][-1].get('key')!='RIGHT' or
            any('save' in s or 'collector' in s or s.get('key')=='HOST_W' for s in prefix['steps'])):
        raise ValueError('画像反馈前缀不允许Save/采音或不明输入')
    names={s['name'] for s in prefix['steps']}
    prefix['source_rois']={k:v for k,v in prepared['source_rois'].items() if k in names}
    prefix['manifest']=copy.deepcopy(prepared['manifest'])
    prefix['manifest'].update(native_hotel_save_after_action=False,
        feedback_only='normal LOAD -> shop EQT members -> portrait cycle -> DOWN -> retained cycle; no detail/Save')
    prefix['runtime_protocol']={'scope':'picker_portrait_retention_only','prefix_steps':copy.deepcopy(prefix['steps']),
        'additional_keys':['ENTER','DOWN'],'save_executed':False,'full_frame_checkpoints':True,
        'phase_gate_by_role':{'original':['P0_navigation_precondition','DOWN_selected2','P0_later_sprite_phase'],
            'sdl':['B','P3','P0','P0_later_sprite_phase','DOWN_selected2','P0_later_sprite_phase']},
        'original_cycle_scope':'existing 237-frame source-history audit is separate; this new run does not re-sign it',
        'no_shared_wall_clock_or_RNG_trajectory_claimed':True}
    prefix['manifest']['native_protocol']=copy.deepcopy(prefix['runtime_protocol'])
    prefix['manifest']['expected_action']={'kind':'portrait_feedback_only','business_write_trace':[],
        'Save_executed':False,'SAV_policy':'complete fixture bytes and mtime unchanged'}
    return prefix


def full_models(prepared,selected):
    if selected not in (0,2):raise ValueError('反馈只接受首屏成员0或2')
    assets=prepared['assets'];state=prepared['before']
    if state['stage']!=1 or state['count']!=5:raise ValueError('反馈只接受既有五人stage1合法夹具')
    sheet=assets['services'][1]['sheet']
    raw_palette=archive_entry(assets['other'],0)
    if len(raw_palette)!=768 or max(raw_palette)>63:raise ValueError('反馈palette源不符')
    palette=[(v<<2)|(v>>4) for v in raw_palette]
    base=assets['services'][1]['background'].copy()
    base.paste(pages._image(pages.member(sheet,1),palette,lmi=True,size=(63,15)),(5,95))
    value=state['resource'];value=value if value<=0x7fffffff else value-0x100000000
    if not 0<=value<=99999999:raise ValueError('反馈资金fixture不在独立八位非负已证域')
    pages.number(base,assets['info'],value,16,99,31,8)
    entry=archive_entry(assets['portraits'],128);portraits={}
    for phase in (0,3):
        start=struct.unpack_from('<I',entry,phase*4)[0]
        end=struct.unpack_from('<I',entry,phase*4+4)[0] if phase<3 else len(entry)
        image=decode_lmi(entry[start:end])
        if image.size!=(80,80):raise ValueError('反馈service画像源几何不同')
        portraits[phase]=image
    # 27d33只铺一次底；4e29c mode64 inc-EDI/stosb只写odd位置，stride孔
    # 继承当前VGA。首次phase可任意0/1/2/3(3映1)，稳态0/2须保前phase1。
    # 26eda的首P3最早第9个eligible轮，至少经过完整sprite循环，所以P3/P0
    # 只能使用stable族；不能把fresh底九张静态图当作完整时间历史。
    fresh={}
    for sprite,lower in enumerate(pages.roster(assets,prepared['tables'],state,selected)):
        if sprite>2:raise ValueError('反馈首屏箭头应抑制，必须恰三sprite源相位')
        frame=base.copy();frame.paste(lower,(5,112));frame.putpalette(palette);fresh[sprite]=frame
    if set(fresh)!={0,1,2}:raise ValueError('反馈缺原sprite三相位')
    def apply_sprite(before,phase):
        frame=before.copy()
        for raw in range(state['count']):
            sprite=pages._sprite(assets,state,raw,phase)
            if sprite is None:raise ValueError('反馈五人fixture不允许未初始化sprite目录')
            sprite.putpalette(palette)
            # _sprite按原token把SKIP变73、mode64孔留0，并拒任何opaque0；
            # 因此这个mask来自源写点，而不是C输出或某端PNG。
            mask=sprite.point(lambda color:255 if color else 0,mode='L')
            frame.paste(sprite,(14+132*(raw%2),117+26*(raw//2)),mask)
        return frame
    stable={0:apply_sprite(fresh[1],0),1:fresh[1],2:apply_sprite(fresh[1],2)}
    result=[];by_hash={}
    for sprite in (0,1,2):
        for history,lower,uppers in [('fresh',fresh[sprite],('B',)),('stable',stable[sprite],('B','P0','P3'))]:
            for upper in uppers:
                image=lower.copy()
                if upper!='B':image.paste(portraits[int(upper[1:])],(123,13))
                image.putpalette(palette);image=image.convert('RGB');digest=hashlib.sha256(image.tobytes()).hexdigest()
                if digest in by_hash:
                    previous=by_hash[digest]
                    if (previous['upper'],previous['sprite_phase'])!=(upper,sprite):
                        raise ValueError('反馈不同源phase整RGB相同，不能唯一识别')
                    previous['history']='fresh_or_stable';continue
                model={'upper':upper,'sprite_phase':sprite,'selected':selected,'history':history,'image':image,
                    'RGB_sha256':digest,'rejection_only':selected==2 and upper=='B'}
                result.append(model);by_hash[digest]=model
    return result


def classify(frame,models):
    if frame.size!=(320,200):raise ValueError('反馈需要完整320x200归一WGC页')
    raw=frame.convert('RGB').tobytes()
    hits=[{k:v for k,v in m.items() if k!='image'} for m in models if raw==m['image'].tobytes()]
    if len(hits)>1:raise ValueError('反馈资源phase家族不唯一')
    return hits[0] if hits else None


def phase_predicate(models,upper,*,different_sprite=None,forbid_background=False):
    def match(frame):
        found=classify(frame,models)
        if found is None:return None
        if forbid_background and found['upper']=='B':
            raise AssertionError('画像已到P0后被擦回初始背景B')
        if found['upper']!=upper or different_sprite is not None and found['sprite_phase']==different_sprite:return None
        return {'kind':'picker_retention_full_page','checked_pixels':64000,'size':[320,200],
            'exact_RGB':True,'tolerance':0,'original_resource_family':True,**found}
    return match


def exercise(capture,prepared,save,guard,record,args):
    import run_g3_prepare_check as base
    # 沿已验正常title/slot/town/shop导航，不重复原30页、EQT detail或Save尾。
    base.exercise(capture,prepared,save,guard,record,args)
    record['normal_prefix_completed']=record.pop('business_sequence_completed',False)
    models0=full_models(prepared,0);models2=full_models(prepared,2)
    record['picker_model_families']=[{k:v for k,v in m.items() if k!='image'} for m in models0+models2]
    trace_start=len(capture.trace)
    with detail_series(capture,record) as scope:
        require_source_unchanged(save,*guard)
        def key(name):
            try:
                before=len(capture.trace);capture.key(name)
                event=capture.trace[-1];delivery=event.get('delivery',{})
                if (len(capture.trace)!=before+1 or event.get('key')!=name or
                        delivery.get('pid')!=capture.process.pid or
                        delivery.get('hwnd')!=scope['ready']['hwnd']):
                    raise AssertionError('画像反馈按键与连续WGC不是同一本轮owned窗口')
            except BaseException as error:
                record.setdefault('first_failure',{'checkpoint':'picker-key-'+name,
                    'error':{'type':type(error).__name__,'message':str(error)}})
                raise
        key('ENTER')
        def point(name,predicate):
            try:
                result=series_checkpoint(capture,scope,name,predicate,record['checkpoints'],timeout=35)
                require_source_unchanged(save,*guard)
                return result[1]
            except BaseException as error:
                record.setdefault('first_failure',{'checkpoint':name,
                    'error':{'type':type(error).__name__,'message':str(error)}})
                raise
        if record.get('kind')=='original':
            # 原237旧序列已含完整B/P3/P0/后继eligible来源；不为补次数重跑。
            # 此处仅等到实际P0作为新DOWN的正常前态，不能继承尚未投递的导航。
            point('picker-original-P0-navigation-precondition',phase_predicate(models0,'P0'))
            record['portrait_cycle_scope']='not re-signed; new original run is only P0 precondition + DOWN feedback'
        elif record.get('kind')=='sdl':
            point('picker-initial-B',phase_predicate(models0,'B'))
            point('picker-blink-P3',phase_predicate(models0,'P3'))
            opened=point('picker-open-P0',phase_predicate(models0,'P0',forbid_background=True))
            point('picker-retained-after-eligible',phase_predicate(models0,'P0',
                different_sprite=opened['sprite_phase'],forbid_background=True))
            record['portrait_cycle_scope']='this SDL run observed B/P3/P0/later eligible full-page resource history'
        else:raise ValueError('画像反馈需要明确original或sdl角色')
        key('DOWN')
        moved=point('picker-selected2-P0',phase_predicate(models2,'P0',forbid_background=True))
        point('picker-selected2-after-eligible',phase_predicate(models2,'P0',
            different_sprite=moved['sprite_phase'],forbid_background=True))
    keys=[e.get('key') for e in capture.trace[trace_start:] if e.get('key')]
    if keys!=['ENTER','DOWN']:raise AssertionError('画像反馈区间出现未声明额外输入')
    if not record.get('detail_series',{}).get('checkpoint_PNGs_bound_to_closed_series'):
        raise AssertionError('画像反馈检查点未绑定正常关闭的同次WGC原件')
    require_source_unchanged(save,*guard)
    record.update(picker_retention_completed=True,save_executed=False,
        feedback_scope='full320x200 source masked-write history; role-specific cycle/navigation checkpoints; no absolute timing/shared RNG proof')
