#!/usr/bin/env python3
"""G4非shipping common callback39窗口机制夹具，默认只准备，不开放关卡。

原版用真实START派生的败亡descriptor(2,39,0)，实际1aa1d→352ca；SDL用
私有Game lookup alias，main/Play/资源/输入/热owner全部真实。原stage0 story11
的text_id1由显式active记录提供；本夹具不宣称原资源自然绑定callback39。
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import shutil
import struct
import fd2_g4_defeat_oracle as defeat_oracle

from PIL import Image
from audit_g4_render_source import archive_entries,text_fragments
from fd2_display_requirement import windows_display_requirement
from fd2_loot_oracle import inventory_append,message_pages,message_match
from fd2_magic_list_oracle import glyphs
from fd2_opening_oracle import portraits,page_match
from run_g2_battle_check import freeze,unchanged,sha
from run_g4_runtime_check import encode
from run_graphics_tests import decode_save
from run_original_battle_animation import AnimationCollectors,owned_job_required
from run_original_battle_menu import wait_state,wait_title
from run_original_comparison import Capture,archive_entry,running
from run_original_load import require_source_unchanged
from run_original_item_list import measure_inventory_save
from static_data.verify_common_events import verify as verify_common_source
import run_original_loot as base

SCENARIOS=('child39-empty','child39-full-cancel')
ALIAS_MESSAGE='G4 common-event mechanism alias: stage.000.action.039 -> common.loot.child.0'


def write(path,value):
    path.write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


def story_speaker(source,text_id):
    """原12c60：首visible active、最后hidden active、最后persistent。"""
    active,roster=source[base.META+1],source[base.META+9]
    if active>96 or roster>32:raise ValueError('story说话人source count越界')
    matched=None
    for raw in range(active):
        unit=source[base.UNITS+raw*base.STRIDE:base.UNITS+(raw+1)*base.STRIDE]
        if unit[8]==text_id:
            matched={'unit_id':unit[7],'raw_actor':raw,'source':'active','visible':not unit[5]&1}
            if matched['visible']:return matched
    if matched is not None:return matched
    for raw in range(roster):
        unit=source[0x8a3+raw*base.STRIDE:0x8a3+(raw+1)*base.STRIDE]
        if unit[8]==text_id:matched={'unit_id':unit[7],'raw_actor':None,'roster_index':raw,'source':'persistent','visible':False}
    if matched is None:raise ValueError('story text_id无active/persistent记录，原15f84会读取未定义低址')
    return matched


def story11(original,assets,source):
    tokens=text_fragments(archive_entries((original/'FDTXT.DAT').read_bytes())[1])[11]
    if len(tokens)!=10 or tokens[:2]!=(-17,1) or tokens[-1]!=-1 or any(t<0 for t in tokens[2:-1]):
        raise ValueError('原stage0 story11不再是已核text_id1七字片段')
    speaker=story_speaker(source,tokens[1])
    font=archive_entry((original/'FDOTHER.DAT').read_bytes(),4)
    canvas=Image.new('P',(320,200));canvas.putpalette(assets['palette'])
    for index,token in enumerate(tokens[2:-1]):
        x=15+16*index;canvas.paste(74,(x,9,x+16,25));glyphs(canvas,font,[token],x,9,205)
    return {'stage':0,'fragment':11,'token_offset':9,'reason':'fragment end',
        'speaker_control':-17,'speaker_argument':1,'speaker_source':speaker,
        'portrait_id':speaker['unit_id'],'portrait_position':(232,5),
        'portraits':portraits(original,speaker['unit_id'],assets['palette'],False),'border_position':(5,2),
        'text':canvas,'known_text_pixels':sum(value!=0 for value in canvas.getdata()),'arrow_position':None}


def prepare(root,scenario,*,capture_defeat=False):
    if scenario not in SCENARIOS:raise ValueError('未知common机制代表')
    original=(root/'original_game').resolve()
    # 独立原字节/表和冷图核验，不把它当作新原生执行。
    source_contract=verify_common_source(original/'FD2.EXE',root)
    p=base.prepare(original,'loot-item');source=bytearray(p['source'])
    full=scenario.endswith('full-cancel')
    # 原START没有text_id1；显式提供合法说话人，不能用原空指针+7的DATO0读取作oracle。
    source[base.UNITS+8]=1
    # 声音身份代表明确隔离游戏BGM；不更改宿主/混音器或SFX开关。
    if capture_defeat:source[base.META+16]=0
    if full:source[base.UNITS+10:base.UNITS+26]=bytes((0x40,0,0x40,132,0,192,0,1,0,2,0,3,0,4,0,5))
    source[base.UNITS+11*base.STRIDE+0x31:base.UNITS+11*base.STRIDE+0x34]=bytes((2,39,0))
    source=base.checksum(source)
    initial=decode_save(original/'START.SAV')
    changes=[{'offset':hex(i),'before':a,'fixture':b} for i,(a,b) in enumerate(zip(initial[:-4],source[:-4])) if a!=b]
    if any(not (base.UNITS<=int(row['offset'],16)<base.UNITS+12*80 or
        capture_defeat and int(row['offset'],16)==base.META+16) for row in changes):
        raise ValueError('common39派生越出声明active前缀')
    items,table=base.item_table(original/'FD2.EXE')
    # 原物理/动作尾沿已校准有限模型，callback39只多一次child211或保持满包。
    model=base.expected_state(source,'loot-story',items)
    immediate=bytearray(model['immediate'])
    if not full:immediate[base.UNITS:base.UNITS+80]=inventory_append(immediate[base.UNITS:base.UNITS+80],211)
    immediate=base.checksum(immediate)
    from run_original_item_list import phase_end_model
    model.update(immediate=immediate,final=phase_end_model(immediate,items))
    focused,reference=base.focus_view(source,p['initial'],p['hero'])
    target_view,_=base.focus_view(focused,p['hero'],(8,14))
    expected=base.positioned(immediate,(8,14),target_view[base.META+3:base.META+5])
    story=story11(original,p['assets'],immediate)
    if story['speaker_source']['visible']:
        raw=story['speaker_source']['raw_actor'];at=base.UNITS+raw*base.STRIDE
        expected,_=base.focus_view(expected,(8,14),tuple(immediate[at:at+2]))
    actor=source[base.UNITS:base.UNITS+80]
    _,source_templates,source_fields=base.inventory_templates(original,actor,p['assets'])
    _,final_templates,final_fields=base.inventory_templates(original,expected[base.UNITS:base.UNITS+80],p['assets'])
    final_fields['require_word_bars']=True
    steps=[{'fragment':432,'open':True,'key':True,'item':211}]
    if full:steps.append({'fragment':433,'open':True,'choice':True,'item':211})
    p.update(source=source,encoded=encode(source),table=table,model=model,focused=focused,
        focus_reference=reference,view=base.actor_view(original,focused,p['assets'],0),
        actor=base.actor_focus_resources(original,focused,p['assets'],0,p['assets']['cursor']),
        target_view=target_view,target=base.actor_focus_resources(original,target_view,p['assets'],11,p['assets']['cursor']),
        immediate=expected,pages=message_pages(original,p['assets'],steps),story=story,
        source_templates=source_templates,source_fields=source_fields,final_templates=final_templates,final_fields=final_fields,
        mechanism_scenario=scenario,full=full,source_contract=source_contract)
    p['manifest']={'scenario':scenario,'source':str(original/'START.SAV'),'source_sha256':sha(original/'START.SAV'),
        'controlled_changes_before_checksum':changes,'original_dispatch':'mode2 WORD39 -> 352ca, child descriptor52742=(0,211,0)',
        'SDL_test_alias':ALIAS_MESSAGE,'production_binding_added':False,'natural_stage0_callback_claimed':False,
        'caller':0,'full_cancel':'433选择框Esc，434固定200ms页由源推断，不要求瞬时采到' if full else None,
        'story_source':{'entry':1,'fragment':11,'tokens':10,'text_id':1,'portrait':story['portrait_id'],
            'resolved_speaker':story['speaker_source'],'dynamic_actor_reference':False,
            'explicit_active_text_id_precondition':True},
        'roster_contract':'本callback/END/活动Save不调用11506同步；完整persistent表保持source原字节',
        'death_audio_fixture':{'enabled':capture_defeat,'game_music':source[base.META+16],
            'game_SFX':source[base.META+17],'host_mixer_changed':False}}
    return p


def exercise(capture,p,save,record,args):
    saved=save.read_bytes();stamp=save.stat().st_mtime_ns;assets=p['assets'];source=p['source']
    def checkpoint(name,predicate,source_guard=True,**kwargs):
        result=wait_state(capture,name,predicate,record['checkpoints'],**kwargs)
        if source_guard:require_source_unchanged(save,saved,stamp)
        return result
    wait_title(capture,p['titles'][3][0]);capture.key('UP');capture.wait(p['titles'][3][2]);capture.key('ENTER')
    checkpoint('common-continued',base.RoundOracle(assets,base.screen(source,p['initial'])).initial_browse,timeout=45)
    capture.key('Z');checkpoint('common-actor0',lambda f:base.focus_match(f,p,p['focused']))
    capture.key('ENTER');checkpoint('common-selection',lambda f:base.selection_match(f,p,p['focused']))
    capture.key('ENTER');checkpoint('common-command',lambda f:base.command(f,p,0))
    capture.key('ENTER');checkpoint('common-target',lambda f:base.selection_match(f,p,p['target_view'],11))
    collectors=AnimationCollectors(capture,args,record) if args.capture_defeat else None
    state=p['immediate']
    try:
        if collectors:
            collectors.start();collectors.confirm();record['physical_confirmations']=1
        else:
            capture.key('ENTER');record['physical_confirmations']=1
        checkpoint('common-child211-wait0',lambda f:message_match(f,p['pages'][0],assets),timeout=90)
        capture.key('ENTER')
        if p['full']:
            checkpoint('common-full-child-choice',lambda f:message_match(f,p['pages'][1],assets),timeout=30)
            capture.key('ESC');record['child_cancel_confirmations']=1
        checkpoint('common-parent-story11',lambda f:page_match(f,p['story'],assets),timeout=60)
        capture.key('ENTER');record['parent_story_ACKs']=1
        checkpoint('common-callback-complete',base.quiet_focus(p,state,record,'common-callback-complete'),timeout=90)
    finally:
        # HOST_W不得插入原版任意键ACK边界；成功后只在已核静态浏览处停止。
        if collectors:collectors.stop()
    if collectors:
        record['defeat_visual_gate']=defeat_oracle.check_visual(record['animation_capture']['frames'],p['defeat'])
        wav=record['audio_capture']['wav']
        if sha(wav['path'])!=wav['sha256']:raise ValueError('败亡WAV与已封口采集身份不同')
        record['defeat_sound_gate']=defeat_oracle.check_sound(p['original'],wav['path'])
        if not record['defeat_visual_gate']['ok'] or not record['defeat_sound_gate']['ok']:
            raise AssertionError('败亡旋转/消散原资源或UI31 cue3最低身份未通过')
    state=base.navigate(capture,p,state,tuple(state[base.META+5:base.META+7]),p['hero'],checkpoint,'common-return-hero')
    capture.key('F2');checkpoint('common-inventory-once',lambda f:base.inventory_detail(f,p['final_templates'],p['final_fields']))
    capture.key('ESC')
    terrain=base.detail_close_terrain(p['original'],state,assets)
    checkpoint('common-details-closed',lambda f:dict(found,terrain=tile) if
        (found:=base.focus_match(f,p,state)) and (tile:=base.exact_tile(f,terrain['image'],terrain['position'])) else None)
    anchor=(6,14)
    state=base.navigate(capture,p,state,p['hero'],anchor,checkpoint,'common-end-anchor')
    oracle=base.RoundOracle(assets,base.screen(state,anchor))
    capture.key('ENTER');checkpoint('common-end-parent',lambda f:oracle.menu(f,0))
    capture.key('DOWN');checkpoint('common-end-selected',lambda f:oracle.menu(f,3))
    capture.key('ENTER');checkpoint('common-end-yes',lambda f:oracle.buttons(f,0))
    capture.key('ENTER');record['END_confirmations']=1
    final,_=base.focus_view(base.positioned(p['model']['final'],anchor,state[base.META+3:base.META+5]),anchor,p['hero'])
    checkpoint('common-round2',base.quiet_focus(p,final,record,'common-round2'),timeout=120)
    final=base.navigate(capture,p,final,p['hero'],anchor,checkpoint,'common-save-anchor')
    step=base.save_empty_step(p,final);base.require_save_enabled(final)
    measure_inventory_save(capture,assets,{'expected_source':final,'steps':[(step['key'],tuple(step['position']))]},
        save,saved,stamp,record,checkpoint,prefix='common',measurement_key='whole_save')
    record['final_save_sha256']=sha(save)
    record['one_child_inventory_write']=not p['full']
    record['child_cancel_preserved_full_inventory']=p['full']


def run(args):
    root=args.root.resolve();original=(root/'original_game').resolve();output=args.artifacts.resolve()
    if output.exists() or output==original or original in output.parents:raise ValueError('产物需原资源外新目录')
    p=prepare(root,args.scenario,capture_defeat=getattr(args,'capture_defeat',False))
    if getattr(args,'capture_defeat',False):p['defeat']=defeat_oracle.resources(original,p['target_view'],target_raw=11)
    output.mkdir(parents=True)
    (output/'controlled.SAV').write_bytes(p['encoded']);write(output/'fixture.json',p['manifest'])
    if decode_save(output/'controlled.SAV')!=p['source']:raise ValueError('机制夹具SAV往返不同')
    report={'ok':False,'status':'prepared_only','scenario':args.scenario,'fixture':p['manifest'],'cases':[],
        'scope':'真实输入/原回调与非shipping SDL alias消费者；child211一次提交、满包Cancel、story11、整SAV',
        'excluded':['生产stage自然绑定','S战斗','全15图原生','共享RNG整轨','完整音画/绝对时序','其它平台'],
        'source_contract':p['source_contract'],'display_requirement':{}}
    if 'defeat' in p:
        report['defeat_declaration']={'visual':p['defeat']['contract'],'sound':p['defeat']['sound']}
        write(output/'defeat-declaration.json',report['defeat_declaration'])
    if not args.run:write(output/'comparison.json',report);return report
    owned_job_required()
    binaries=[args.sdl.resolve(),args.driver.resolve(),args.dosbox.resolve(),root/'src/tests/g4_common_event_game_fixture.c',root/'src/CMakeLists.txt']
    if (original.parent/'FD2.SAV').is_file():binaries.append(original.parent/'FD2.SAV')
    inputs,before,sources=freeze(root,original,binaries)
    for name in ('controlled.SAV','fixture.json'):inputs[str(output/name)]=sha(output/name)
    try:
        with windows_display_requirement(report['display_requirement']):
            for kind in ('original','sdl'):
                work=output/kind;work.mkdir();env=os.environ.copy()
                for key in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED',
                    'FD2SDL_APP_PCM_CAPTURE_DIR','FD2SDL_APP_PCM_CAPTURE_MAX_MS',
                    'FD2SDL_APP_PCM_CAPTURE_QUIET','FD2SDL_TEST_AUDIO_QUIET'):env.pop(key,None)
                env['FD2SDL_TEST_AUDIO_QUIET']='1'
                if kind=='original':
                    game=work/'game';shutil.copytree(original,game);save=game/'FD2.SAV';config=work/'dosbox.conf'
                    captures=work/'captures';captures.mkdir()
                    config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                        '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n'+f'captures={captures.as_posix()}\nshow recorded filename=false\n'
                        '[render]\naspect=false\nscaler=normal2x\n[mixer]\nrate=44100\n'
                        '[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
                    inputs[str(config)]=sha(config)
                    mutable={save.resolve(),(game/'FD2.TMP').resolve()}
                    for file in game.rglob('*'):
                        if file.is_file() and file.resolve() not in mutable:inputs[str(file)]=sha(file)
                    command,cwd=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'],work
                else:
                    save=work/'FD2.SAV';env['FD2SDL_SAVE_PATH']=str(save);command,cwd=[str(args.sdl.resolve())],root
                    if args.capture_defeat:
                        app_mix=work/'app-mix';app_mix.mkdir()
                        env.update(FD2SDL_APP_PCM_CAPTURE_DIR=str(app_mix),
                            FD2SDL_APP_PCM_CAPTURE_MAX_MS='45000',FD2SDL_APP_PCM_CAPTURE_QUIET='1')
                save.write_bytes(p['encoded']);initial_stamp=save.stat().st_mtime_ns
                record={'kind':kind,'ok':False,'inputs':[],'checkpoints':[],'processes':[],
                    'owned_mutable_paths':[str(save)]+([str(game/'FD2.TMP')] if kind=='original' else [])}
                report['cases'].append(record)
                try:
                    if not unchanged(inputs,original,before,root,sources):raise ValueError('首窗口前来源变化')
                    with running(command,cwd,env,work/'game.log',args.driver.resolve(),record['processes']) as process:
                        capture=Capture(args.driver.resolve(),process,work,record['inputs'])
                        exercise(capture,p,save,record,args)
                    if kind=='sdl' and ALIAS_MESSAGE not in (work/'game.log').read_text(encoding='utf-8',errors='replace'):
                        raise ValueError('SDL未报告本机制alias；不可用shipping程序代替')
                    record['ok']=True
                finally:
                    completed=record.get('completed_save')
                    expected=Path(completed['snapshot']).read_bytes() if completed else p['encoded']
                    require_source_unchanged(save,expected,completed['mtime_ns'] if completed else initial_stamp)
                    record['sources_unchanged']=unchanged(inputs,original,before,root,sources)
                    record['ok'] &= record['sources_unchanged'] and len(record['processes'])==1 and all(
                        row['cleaned'] and not row['forced_cleanup'] and row['exit_code']==0 for row in record['processes'])
                    write(work/'comparison.json',record)
                if not record['ok']:raise AssertionError('common机制端点未通过：'+kind)
        actual=[decode_save(Path(case['completed_save']['snapshot'])) for case in report['cases']]
        report['paired_save']=base.compare_saves(*actual)
        report['ok']=len(actual)==2 and all(row['ok'] for row in report['cases']);report['status']='passed'
    except BaseException as error:
        report['status']='failed';report['error']={'type':type(error).__name__,'message':str(error)}
    finally:
        report['sources_unchanged']=unchanged(inputs,original,before,root,sources)
        report['ok'] &= report['sources_unchanged'] and report['display_requirement'].get('released') is True
        if not report['ok']:report['status']='failed'
        write(output/'comparison.json',report)
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('sdl','driver','dosbox'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--scenario',choices=SCENARIOS,required=True);parser.add_argument('--run',action='store_true')
    parser.add_argument('--capture-defeat',action='store_true',help='持续WGC及原生WAV/App mix验证败亡原资源和cue3有限身份；不是完整音画等价')
    args=parser.parse_args()
    args.sdl_audio_source='app-mix';args.animation_capture_interval_ms=16
    if args.run and any(getattr(args,n) is None or not getattr(args,n).is_file() for n in ('sdl','driver','dosbox')):
        parser.error('--run需已构建机制exe及driver/dosbox')
    try:report=run(args)
    except Exception as error:parser.exit(1,'G4事件机制夹具未能启动：'+str(error)+'。请核对来源和参数。\n')
    print(json.dumps({'ok':report['ok'],'status':report['status']},ensure_ascii=False))
    return 0 if not args.run or report['ok'] else 1


if __name__=='__main__':raise SystemExit(main())
