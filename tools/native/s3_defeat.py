"""S3 stage12正式CONTINUE/活动Load→1197b故事等待→GAME OVER→标题。

依据完整10010/117e7/19df7/20765/22e5c/main25bf4；不调用Game或source快入口。
仅在前一owned进程真实Save后，向本轮SAV副本写受控hidden输入；不改原资料。
"""
from pathlib import Path

import fd2_natural_oracle as natural
from fd2_opening_oracle import page_match
from run_graphics_tests import decode_save
from run_original_battle_menu import save_checkpoint, wait_title
from run_original_load import require_source_unchanged
from run_original_result import result_branch, result_templates
from field_play_input_test import wait_log
from . import s3_fixture as oracle

QUERY='field result query: stage=12 result=2 caller=player actor=0'
EXIT='field battle: defeat, stage=12'
RAW_VILLAGERS=tuple(range(15,27))


def derive(source):
    """20765：turn1不读raw59；仅raw15..26 hidden及checksum允许变化。"""
    oracle.valid_container(source)
    if source[oracle.META]!=1 or source[oracle.META+2]!=12 or source[oracle.META+1]!=59:
        raise ValueError('败北代表须为本次stage12 entry后的turn1/59人真实Save')
    records=oracle.units(source)
    if records[0][6]!=2 or records[0][5]&0x85 or any(records[i][5]&1 for i in RAW_VILLAGERS):
        raise ValueError('受控败北须保留可选择raw0，且十二村民源均可见')
    if not any(u[6]==0 and not u[5]&1 for u in records):raise ValueError('普通205be须仍有可见敌军')
    result=bytearray(source)
    for raw in RAW_VILLAGERS:result[oracle.UNITS+80*raw+5]|=1
    result=oracle.checksum(result)
    allowed={oracle.UNITS+80*i+5 for i in RAW_VILLAGERS}|set(range(len(source)-4,len(source)))
    patches=[{'offset':i,'before':a,'after':b} for i,(a,b) in enumerate(zip(source,result)) if a!=b]
    if any(row['offset'] not in allowed for row in patches):raise AssertionError('败北派生越出hidden/checksum')
    return result,{'controlled_fixture':True,'natural_whole_stage_claimed':False,'runtime_API_or_alias_used':False,
        'source_plain_sha256':oracle.digest(source),'derived_plain_sha256':oracle.digest(result),
        'raw_hidden':list(RAW_VILLAGERS),'plaintext_patches':patches,'HP_roster_metadata_manual_slots_preserved':True,
        'source':'20765 all raw15..26 hidden: latch1 before story10; turn1 skips raw59'}


def preflight(p):
    view,_,_=oracle.entry12_projection(p['source'],p['manual_state'],p['data'],p['original'])
    hidden,_=derive(view);pages=oracle.pages(p['original'],p['assets'],p['data'],hidden,12,(10,))
    if not pages:raise ValueError('stage12败北缺完整story10等待页')
    result_templates(p['original'],p['assets'])
    return {'stage':12,'turn':1,'active_count':59,'story_fragment':10,'ACK_count':len(pages),
        'portrait_ids':sorted({page['portrait_id'] for page in pages}),
        'formal_paths':['title CONTINUE','activity Load','actor selection ESC','story10 ACK','GAME OVER','title'],
        'expected_exit_source':'main25dde..25e04 calls22e5c, clears53ECC, i=1 then outer title loop',
        'native_execution_claimed':False}


def _text(capture):
    return (capture.artifacts/'game.log').read_text(encoding='utf8',errors='replace')


def _not_returned(capture,kind):
    if kind=='sdl' and any(line.startswith(('field result query:','field battle:')) for line in _text(capture).splitlines()):
        raise AssertionError('结果故事返回前已记录query完成或Play败北离场')


def _require_returned(text):
    lines=text.splitlines()
    queries=[line for line in lines if line.startswith('field result query:')]
    exits=[line for line in lines if line.startswith('field battle:')]
    if queries!=[QUERY] or exits!=[EXIT]:
        raise AssertionError('story10完成后缺唯一player/raw0结果返回及实际Play败北整行，或出现其它调用')


def exercise(capture,p,state,save,guard,record,cp,*,load_activity):
    """第二owned进程，真实CONTINUE健康Save后，另用活动Load读受控输入。"""
    save=Path(save)
    if save.name!='FD2.SAV' or not save.resolve().is_relative_to(capture.artifacts.parent.resolve()):
        raise ValueError('败北受控写入必须是本轮owned工作目录FD2.SAV')
    require_source_unchanged(save,*guard)
    if decode_save(save)!=state:raise ValueError('CONTINUE必须使用上个owned进程实际完整Save')
    wait_title(capture,p['titles'][3][0],timeout=90)
    capture.key('UP');capture.wait(p['titles'][3][2]);capture.key('ENTER')
    cp('s3-stage12-formal-continue',lambda f:natural.focus_match(f,p['original'],state,p['assets']),timeout=90)
    hidden,manifest=derive(state)
    require_source_unchanged(save,*guard)
    source=capture.artifacts/'s3-stage12-actual-before-hidden.SAV';source.write_bytes(guard[0])
    encoded=oracle.encode(hidden);save.write_bytes(encoded);guard[:]=[encoded,save.stat().st_mtime_ns]
    derived=capture.artifacts/'s3-stage12-hidden-villagers.SAV';derived.write_bytes(encoded)
    manifest.update(source_snapshot=str(source),source_encrypted_sha256=oracle.digest(source.read_bytes()),
        derived_snapshot=str(derived),derived_encrypted_sha256=oracle.digest(encoded),
        write_boundary='after observed formal CONTINUE, before real SYSTEM activity Load')
    record['defeat_fixture']=manifest
    load_record={'kind':record['kind']}
    loaded=load_activity(capture,p,hidden,save,guard,load_record,cp)
    record['defeat_active_load']=load_record
    _not_returned(capture,record['kind'])
    capture.key('Z')
    cp('s3-stage12-defeat-focus-raw0',lambda f:{'state':found[0],**found[1]}
        if (found:=natural.actor_browse_anywhere(f,p['original'],loaded,p['assets'],0)) else None,timeout=90)
    found=natural.actor_browse_anywhere(capture.last,p['original'],loaded,p['assets'],0)
    if found is None:raise AssertionError('受控Load后raw0真实focus丢失')
    view,_=found;summary=natural.selection_summary(p['original'],view,p['assets'],0)
    probes=natural.range_probe(p['original'],view,p['assets'],0)
    if not probes:raise ValueError('受控败北缺真实可选范围资源')
    capture.key('ENTER')
    cp('s3-stage12-defeat-real-selection',lambda f:natural.focus_match(f,p['original'],view,p['assets'],summary=summary)
        if natural.range_match(f,probes) else None)
    _not_returned(capture,record['kind']);capture.key('ESC')
    pages=oracle.pages(p['original'],p['assets'],p['data'],view,12,(10,))
    acknowledgements=[]
    for ordinal,page in enumerate(pages):
        cp('s3-stage12-defeat-story10-'+str(ordinal),lambda f,page=page:page_match(f,page,p['assets']),timeout=90)
        _not_returned(capture,record['kind'])
        acknowledgements.append({'fragment':page['fragment'],'token_offset':page['token_offset'],'portrait':page['portrait_id']})
        capture.key('ENTER')
    after_story_trace=len(record['inputs']);templates=result_templates(p['original'],p['assets'])
    def complete(frame):
        found=result_branch(frame,'defeat',p['assets'],templates)
        return found if found and found['frame']==1 else None
    _,last=cp('s3-stage12-defeat-game-over',complete,timeout=60,allow_low_color_resource=True)
    capture.wait(p['titles'][3][0],blank_transition=True,timeout=90)
    require_source_unchanged(save,*guard)
    # Capture.wait的blank_transition也追加原采集错误；它们不是按键。
    post_story_inputs=[row for row in record['inputs'][after_story_trace:] if 'key' in row or 'delivery' in row]
    if post_story_inputs:raise AssertionError('story10后不得补键伪造原自动出口')
    save_checkpoint(capture,'s3-stage12-defeat-returned-title',capture.last,
        {'kind':'full_title_resource','after_full_GAME_OVER':True,'post_story_input_count':0},record['checkpoints'])
    if record['kind']=='sdl':
        _require_returned(wait_log(capture.process,capture.artifacts/'game.log',EXIT,timeout=30))
    record['stage12_effect_result']={'real_formal_CONTINUE':True,'real_activity_Load':True,
        'trigger':'real actor Enter selection→ESC→1197b','story10_acknowledgements':acknowledgements,
        'SDL_completion_log_verified':record['kind']=='sdl','original_runtime_latch_dumped':False,
        'full_GAME_OVER':last,'returned_title_before_cleanup':True,'post_story_inputs':post_story_inputs,
        'source_contract':'20765 writes latch1 before story10; only after1197b returns main25bf4 calls22e5c then title'}
    return loaded
