#!/usr/bin/env python3
"""S3 正式 App 输入的有限代表；默认准备，--run 才串行打开原版/SDL。

不使用source API、lookup alias、私有Game或测试App。受控SAV改动与正式
CONTINUE/END/result/prepare/title LOAD/entry可达分账，不称自然整关。
"""
from __future__ import annotations
import argparse
import json
import os
from pathlib import Path
import shutil
import sys

from native import s3_fixture as oracle
from native import s3_defeat
import fd2_natural_oracle as natural
import run_original_natural as protocol
from fd2_display_requirement import windows_display_requirement
from fd2_opening_oracle import page_match
from fd2_reward_oracle import common_tokens
from fd2_transition_oracle import resources, picker_image, picker_match, notice_resources, notice_match, _text
from field_play_input_test import wait_log
from run_g2_battle_check import fingerprint, sha
from run_original_battle_animation import owned_job_required
from run_original_battle_menu import wait_state
from run_original_comparison import Capture, running, title_templates
from run_original_end_turn import META, UNITS, RoundOracle
from run_original_load import require_source_unchanged, wait_measured_save
from run_original_player import screen, sheet_frame
from run_original_stage1_complete import hotel_menu_state
from run_original_transition import town_match
from static_data.stage_source_ledger import validate_output

ROOT=Path(__file__).resolve().parents[1]


def write(path,value):Path(path).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf8')


def product_sources(root):
    sources=set()
    for path in (root/'src').rglob('*'):
        relative=path.relative_to(root/'src')
        if path.is_file() and path.suffix.lower() in ('.c','.h','.cpp') and not any(
            p=='tests' or p.startswith('build') or p in ('Debug','Release','_deps','CMakeFiles') for p in relative.parts):sources.add(path.resolve())
    sources.update(p.resolve() for p in (root/'data/static').rglob('*.json'))
    return sources


def freeze(root,original,binaries,build):
    """只收当前实际构建目录，不暗收src/build-win的其它候选生成物。"""
    files={Path(p).resolve() for p in binaries};sources=product_sources(root);files.update(sources)
    cache=build/'CMakeCache.txt'
    if not cache.is_file() or not (build/'generated/static_data').is_dir():raise ValueError('--build-dir不是本次实际CMake/生成物目录')
    home=[line.split('=',1)[1] for line in cache.read_text(encoding='utf8').splitlines() if line.startswith('CMAKE_HOME_DIRECTORY:INTERNAL=')]
    if len(home)!=1 or Path(home[0]).resolve()!=(root/'src').resolve():raise ValueError('实际构建目录并非本工作树 src，拒绝误用其它候选')
    files.add(cache);files.update(p.resolve() for p in (build/'generated/static_data').glob('*.inc'))
    for module in tuple(sys.modules.values()):
        path=getattr(module,'__file__',None)
        if path and Path(path).resolve().is_relative_to(root/'tools'):files.add(Path(path).resolve())
    return {str(p):sha(p) for p in sorted(files)},fingerprint(original),sorted(map(str,sources))


def unchanged(inputs,original,baseline,root,sources):
    return (sorted(map(str,product_sources(root)))==sources and fingerprint(original)==baseline and
        all(Path(p).is_file() and sha(p)==value for p,value in inputs.items()))


def checkpoint(capture,record,save,guard):
    def cp(name,predicate,**options):
        result=wait_state(capture,name,predicate,record['checkpoints'],**options)
        require_source_unchanged(save,*guard)
        # 原共享reader返回的临时PIL/state不进入最终JSON。
        record['checkpoints'][-1].pop('page',None);record['checkpoints'][-1].pop('state',None)
        return result
    return cp


def continue_stage(capture,p,cp):
    from run_original_battle_menu import wait_title
    wait_title(capture,p['titles'][3][0],timeout=90)
    capture.key('UP');capture.wait(p['titles'][3][2]);capture.key('ENTER')
    cp('s3-formal-continue-stage2',lambda f:natural.focus_match(f,p['original'],p['source'],p['assets']),timeout=90)


def save_activity(capture,p,state,save,guard,record,cp,label,validate):
    bank=natural.cursor_bank(p['original'],state,p['assets'])
    tile=natural.empty_tile(p['original'],state,p['assets'],capture.last)
    view=protocol.navigate(capture,p['original'],state,p['assets'],bank,tile['world'],cp,label+'-navigate')
    menu=RoundOracle(p['assets'],screen(view,tile['world']))
    capture.key('ENTER');cp(label+'-system',lambda f:menu.menu(f,0))
    capture.key('ENTER');cp(label+'-secondary',lambda f:menu.secondary(f,0))
    capture.key('LEFT');cp(label+'-save-selected',lambda f:menu.secondary(f,1))
    capture.key('ENTER');cp(label+'-save-yes',lambda f:menu.buttons(f,0))
    before=oracle.decode_save(save);capture.key('ENTER');after=wait_measured_save(save,*guard)
    guard[:]=[save.read_bytes(),save.stat().st_mtime_ns]
    snapshot=capture.artifacts/(label+'.SAV');snapshot.write_bytes(guard[0])
    result=validate(before,after,oracle.checksum(view))
    row={'label':label,'snapshot':str(snapshot),'sha256':sha(snapshot),'mtime_ns':guard[1],'measurement':result}
    record.setdefault('active_saves',[]).append(row);record['completed_save']=row
    cp(label+'-saved-browse',menu.closed_browse)
    return after


def load_activity(capture,p,state,save,guard,record,cp):
    """同一活动档先取消，再真正Load，再Save；各端拥有字节不跨端借值。"""
    bank=natural.cursor_bank(p['original'],state,p['assets']);tile=natural.empty_tile(p['original'],state,p['assets'],capture.last)
    moved=protocol.navigate(capture,p['original'],state,p['assets'],bank,tile['world'],cp,'s3-load-away')
    menu=RoundOracle(p['assets'],screen(moved,tile['world']))
    def open_load(label):
        capture.key('ENTER');cp(label+'-system',lambda f:menu.menu(f,0))
        capture.key('ENTER');cp(label+'-secondary',lambda f:menu.secondary(f,0))
        capture.key('RIGHT');cp(label+'-load-selected',lambda f:menu.secondary(f,2))
        capture.key('ENTER');cp(label+'-default-yes',lambda f:menu.buttons(f,0))
    open_load('s3-cancel-load');capture.key('ESC');cp('s3-load-cancel-browse',menu.closed_browse)
    record['load_cancel']={'disk_bytes_and_mtime_unchanged':True,'runtime_full_state_claimed':False}
    open_load('s3-confirm-load');capture.key('ENTER')
    cp('s3-loaded-original-view',lambda f:natural.focus_match(f,p['original'],state,p['assets']),timeout=90)
    return save_activity(capture,p,state,save,guard,record,cp,'s3-after-active-load',
        lambda before,after,view:oracle.check_loaded_save(state,after,view,record['kind']))


def turn3_storage(capture,p,save,guard,record):
    cp=checkpoint(capture,record,save,guard);continue_stage(capture,p,cp);state=p['source']
    record.update(resource_acknowledgements=[],native_round_banners=[],actual_turn_saves=[])
    for turn in (2,3):
        reader=protocol.ProgressReader(p['original'],state,p['assets'],record,stories=p['story_pages'],victory_fragment=-1,
            reward_resource_stage=2)
        state=protocol.end_phase(capture,p['original'],state,p['assets'],natural.cursor_bank(p['original'],state,p['assets']),reader,record,cp)
        state=protocol.wait_progress(capture,p['original'],state,turn-1,p['assets'],reader,record,cp,phase=True)
        records,masks=oracle.turn_projection(p['source'],p['data'],record['kind'],turn)
        view=oracle.as_view(state,records,2,turn,state[META+3:META+5],state[META+5:META+7])
        state=save_activity(capture,p,view,save,guard,record,cp,'s3-turn'+str(turn),
            lambda before,after,v,records=records,masks=masks:oracle.check_activity(before,after,v,records,masks,
                p['data']['stages'][2]['metadata'],kind=record['kind']))
    expected=[['story',page['fragment'],page['token_offset']] for page in p['story_pages']]
    actual=[row['signature'] for row in record['resource_acknowledgements'] if row['signature'][0]=='story']
    if actual!=expected:raise AssertionError('真实callback9 story4 ACK不完整或重复')
    record['turn3_source_binding']={'turn':3,'phase':2,'action':9,'active_count':27,'full_story4_ACK':True,'API_or_alias_used':False}
    load_activity(capture,p,state,save,guard,record,cp)


def preparation_town(p,state):
    assets=resources(p['original']);raw=p['data']['raw'];stage=state['stage'];row=raw[0x6238d+(stage-1)*31:0x6238d+stage*31]
    if stage not in (3,12) or row[0]!=(0 if stage==3 else 1):raise ValueError('准备页不是stage3/type0或stage12/type1的固定来源域')
    city_type=row[0];other=(p['original']/'FDOTHER.DAT').read_bytes()
    from run_original_comparison import archive_entry,decode_image
    assets['background']=decode_image(archive_entry(other,raw[0x52405+city_type]));assets['background'].putpalette(assets['palette'])
    offset=city_type*6;assets['town_xy']=list(zip(raw[0x52363+offset:0x52368+offset],raw[0x52375+offset:0x5237a+offset]))
    icons=(p['original']/'FDICON.B24').read_bytes();towns=[]
    for selected in range(5):
        views=[]
        for phase in (0,1,2):
            view=assets['background'].crop((0,0,312,192));view.paste(assets['panel'],(244,162))
            _text(view,assets['font'],common_tokens(assets['text'],495+selected),252,168,205,background=74)
            marker=sheet_frame(icons,state['roster'][7]*12+phase)
            view.paste(marker,assets['town_xy'][selected],marker.point(lambda v:255 if v else 0,mode='L'));views.append(view)
        towns.append(views)
    return {'assets':assets,'towns':towns,'source_stage':stage,'source_prepare_type':city_type}


def manual_slots(capture,p,save,guard,record,state):
    cp=checkpoint(capture,record,save,guard);town=preparation_town(p,state);assets=town['assets']
    cp('s3-stage3-prepare',lambda f:town_match(f,town,0),timeout=90)
    def hotel(selected,fragment):
        notice=notice_resources(p['original'],assets,fragment)
        return lambda f:dict(found,notice=text) if (found:=hotel_menu_state(f,assets,selected)) and (text:=notice_match(f,notice,p['assets'])) else None
    capture.key('ENTER');cp('s3-stage3-hotel',hotel(0,585))
    capture.key('RIGHT');cp('s3-stage3-save-command',hotel(1,585));capture.key('ENTER')
    before=oracle.decode_save(save);record['manual_saves']=[]
    for slot in range(4):
        if slot:capture.key('DOWN')
        cp('s3-picker-'+str(slot),lambda f,s=slot,b=before:picker_match(f,picker_image(assets,b,s)),allow_low_color_resource=True)
        capture.key('ENTER');after=wait_measured_save(save,*guard);guard[:]=[save.read_bytes(),save.stat().st_mtime_ns]
        target=capture.artifacts/('s3-stage3-manual'+str(slot)+'.SAV');target.write_bytes(guard[0])
        check=oracle.check_manual(before,after,state,slot);record['manual_saves'].append({'slot':slot,'snapshot':str(target),'sha256':sha(target),'measurement':check})
        cp('s3-manual-saved-'+str(slot),lambda f:notice_match(f,notice_resources(p['original'],assets,660),p['assets']))
        capture.key('ENTER');before=after
    record['completed_save']={'snapshot':str(target),'sha256':sha(target),'mtime_ns':guard[1]}


def victory_prepare(capture,p,save,guard,record):
    cp=checkpoint(capture,record,save,guard);continue_stage(capture,p,cp)
    capture.key('Z')
    cp('s3-victory-focus-raw0',lambda f:{'state':found[0],**found[1]}
        if (found:=natural.actor_browse_anywhere(f,p['original'],p['source'],p['assets'],0)) else None,timeout=90)
    view,proof=natural.actor_browse_anywhere(capture.last,p['original'],p['source'],p['assets'],0)
    summary=natural.selection_summary(p['original'],view,p['assets'],0);ranges=natural.range_probe(p['original'],view,p['assets'],0)
    capture.key('ENTER');cp('s3-victory-selection',lambda f:natural.focus_match(f,p['original'],view,p['assets'],summary=summary)
        if natural.range_match(f,ranges) else None)
    capture.key('ESC');record['trigger']='real selection cancel→controller result→victory2; controlled enemy-hidden SAV only'
    record['settlement_acknowledgements']=[]
    for i,page in enumerate(p['story_pages']):
        cp('s3-victory-story7-'+str(i),lambda f,page=page:page_match(f,page,p['assets']),timeout=90)
        record['settlement_acknowledgements'].append({'fragment':page['fragment'],'token_offset':page['token_offset'],'portrait':page['portrait_id']})
        capture.key('ENTER')
    state=oracle.settlement(p['source'],p['data'],record['kind']);manual_slots(capture,p,save,guard,record,state)
    return state


def title_load_entry(capture,p,save,guard,record,state):
    from run_original_battle_menu import wait_title
    cp=checkpoint(capture,record,save,guard);town=preparation_town(p,state);assets=town['assets'];stage=state['stage']
    wait_title(capture,p['titles'][3][0],timeout=90);capture.key('DOWN');capture.wait(p['titles'][3][1]);capture.key('ENTER')
    before=oracle.decode_save(save);cp('s3-title-load-picker0',lambda f:picker_match(f,picker_image(assets,before,0)),allow_low_color_resource=True)
    capture.key('ENTER');cp(f's3-title-load-stage{stage}-prepare',lambda f:town_match(f,town,0),timeout=90)
    for selected in (1,2):capture.key('LEFT');cp('s3-deploy-'+str(selected),lambda f,s=selected:town_match(f,town,s))
    button=RoundOracle(p['assets'],(0,0));notice=notice_resources(p['original'],assets,513,portrait=75,origin=(95,119))
    capture.key('ENTER');cp(f's3-entry{stage}-confirm',lambda f:dict(a,b=b) if (a:=button.buttons(f,0)) and (b:=notice_match(f,notice,p['assets'])) else None)
    capture.key('ENTER')
    projection=oracle.entry3_projection if stage==3 else oracle.entry12_projection
    view,records,masks=projection(before,state,p['data'],p['original'])
    story=oracle.pages(p['original'],p['assets'],p['data'],view,stage,(0,1) if stage==3 else (0,));record['entry_acknowledgements']=[]
    for i,page in enumerate(story):
        cp(f's3-entry{stage}-story-'+str(i),lambda f,page=page:page_match(f,page,p['assets']),timeout=90)
        record['entry_acknowledgements'].append({'fragment':page['fragment'],'token_offset':page['token_offset'],'portrait':page['portrait_id']});capture.key('ENTER')
    cp(f's3-entry{stage}-full-browse',lambda f:natural.focus_match(f,p['original'],view,p['assets']),timeout=90)
    if stage==12:
        summary=natural.selection_summary(p['original'],view,p['assets'],0);ranges=natural.range_probe(p['original'],view,p['assets'],0)
        anchor='field result query: stage=12 result=0';log=capture.artifacts/'game.log'
        previous=log.read_text(encoding='utf8',errors='replace').count(anchor) if record['kind']=='sdl' else None
        capture.key('ENTER');cp('s3-stage12-real-selection',lambda f:natural.focus_match(f,p['original'],view,p['assets'],summary=summary)
            if natural.range_match(f,ranges) else None)
        capture.key('ESC');cp('s3-stage12-selection-cancelled',lambda f:natural.focus_match(f,p['original'],view,p['assets'])
            if natural.range_match(f,ranges) is None else None)
        if record['kind']=='sdl':
            text=wait_log(capture.process,log,anchor,timeout=30)
            if text.count(anchor)!=previous+1:raise AssertionError('stage12取消未取得本次唯一1197b后实际query日志')
        record['result_post']={'stage':12,'result':0,'actual_selection_and_cancel':True,'acted_or_WAIT_injected':False,
            'original_evidence':'complete 117e7/18890→1197b source + real selection/cancel + subsequent full UI Save',
            'SDL_unique_post_log_verified':record['kind']=='sdl','original_runtime_latch_dumped':False}
    saved=save_activity(capture,p,view,save,guard,record,cp,f's3-entry{stage}-active',lambda before,after,v:oracle.check_activity(
        before,after,v,records,masks,p['data']['stages'][stage]['metadata'],kind=record['kind'],fresh_metadata=True,roster=state['roster']))
    return load_activity(capture,p,saved,save,guard,record,cp)


def run(args):
    root=args.root.resolve();original=(args.original or root/'original_game').resolve(strict=True)
    if original!=(root/'original_game').resolve(strict=True):raise ValueError('正式App从root/original_game读资源；--original必须指向同一真实资源目录')
    output=validate_output(args.artifacts,root=root,original=original)
    p=oracle.prepare(original,args.save_fixture,args.source_report,args.scenario);p['titles']=title_templates(original/'FDOTHER.DAT')
    p['manifest']['source_model_preflight']=oracle.preflight(p)
    if args.scenario==oracle.SCENARIOS[2]:p['manifest']['source_model_preflight']['formal_defeat']=s3_defeat.preflight(p)
    output.mkdir(parents=True);(output/'controlled.SAV').write_bytes(p['encoded']);write(output/'fixture.json',p['manifest'])
    report={'schema':'fd2-s3-formal-native-v1','scenario':args.scenario,'ok':False,'status':'prepared','cases':[],
        'fixture':p['manifest'],'formal_public_App_required':True,'natural_whole_stage_claimed':False,'display_requirement':{},
        'scope':'formal declared controlled source→UI bindings; independent resource/container oracle',
        'excluded':['whole-stage natural combat','shared RNG/whole runtime equality','Linux/macOS','unselected bindings']}
    if not args.run:write(output/'comparison.json',report);return report
    if sys.platform!='win32':raise ValueError('S3 原生代表仅在 Windows WGC 执行；其它平台只能准备，未启动窗口')
    owned_job_required()
    if args.sdl.name.lower() not in ('fd2sdl.exe','fd2sdl'):raise ValueError('S3必须运行正式fd2sdl App，不能传S2机制fixture')
    binaries=[args.sdl.resolve(),args.sdl.with_suffix('.pdb').resolve(),args.driver.resolve(),args.dosbox.resolve(),args.save_fixture.resolve(),args.source_report.resolve()]
    user_save=original.parent/'FD2.SAV'
    if user_save.is_file():binaries.append(user_save)
    binaries.extend(Path(row['path']) for row in p['origin']['stage2_entry_captures'])
    inputs,baseline,sources=freeze(root,original,binaries,args.build_dir.resolve());report.update(input_sha256=inputs,product_source_paths=sources,
        actual_build_directory=str(args.build_dir.resolve()),generated_input_policy='only this build directory; no implicit build-win')
    try:
        with windows_display_requirement(report['display_requirement']),oracle.resource_domain(original,(2,3,12)):
            for kind in ('original','sdl'):
                work=output/kind;work.mkdir();case={'kind':kind,'ok':False,'processes':[],'inputs':[],'checkpoints':[]};report['cases'].append(case)
                env=os.environ.copy()
                for key in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED'):env.pop(key,None)
                if kind=='original':
                    game=work/'game';shutil.copytree(original,game);save=game/'FD2.SAV';config=work/'dosbox.conf'
                    config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                        '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n'
                        '[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf8')
                    inputs[str(config)]=sha(config)
                    for file in game.rglob('*'):
                        if file.is_file() and file.resolve() not in (save.resolve(),(game/'FD2.TMP').resolve()):inputs[str(file.resolve())]=sha(file)
                    command,cwd=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'],work
                else:
                    save=work/'FD2.SAV';env['FD2SDL_SAVE_PATH']=str(save);command,cwd=[str(args.sdl.resolve())],root
                save.write_bytes(p['encoded']);guard=[save.read_bytes(),save.stat().st_mtime_ns]
                try:
                    phases=(0,1) if args.scenario in oracle.SCENARIOS[1:] else (0,);state=p.get('manual_state')
                    for phase in phases:
                        phase_work=work/('phase'+str(phase));phase_work.mkdir()
                        if not unchanged(inputs,original,baseline,root,sources):raise ValueError('S3 首窗口前来源改变')
                        with running(command,cwd,env,phase_work/'game.log',args.driver.resolve(),case['processes']) as process:
                            capture=Capture(args.driver.resolve(),process,phase_work,case['inputs'])
                            if args.scenario==oracle.SCENARIOS[2]:
                                if phase:state=s3_defeat.exercise(capture,p,state,save,guard,case,
                                    checkpoint(capture,case,save,guard),load_activity=load_activity)
                                else:state=title_load_entry(capture,p,save,guard,case,state)
                            elif phase:title_load_entry(capture,p,save,guard,case,state)
                            elif args.scenario==oracle.SCENARIOS[0]:turn3_storage(capture,p,save,guard,case)
                            else:state=victory_prepare(capture,p,save,guard,case)
                        require_source_unchanged(save,*guard)
                    case['ok']=len(case['processes'])==len(phases) and all(row['cleaned'] and not row['forced_cleanup'] and row['exit_code']==0 for row in case['processes'])
                finally:
                    case['owned_save_unchanged_since_last_measured_write']=(save.read_bytes()==guard[0] and save.stat().st_mtime_ns==guard[1])
                    case['sources_unchanged']=unchanged(inputs,original,baseline,root,sources)
                    case['ok']&=case['sources_unchanged'] and case['owned_save_unchanged_since_last_measured_write'];write(work/'comparison.json',case)
                if not case['ok']:raise AssertionError('S3正式输入端未通过：'+kind)
        report.update(ok=True,status='passed',original_runtime_executed=True,SDL_runtime_executed=True)
    except BaseException as error:report.update(status='failed',error={'type':type(error).__name__,'message':str(error)})
    finally:
        report['sources_unchanged']=unchanged(inputs,original,baseline,root,sources)
        report['ok']&=report['sources_unchanged'] and report['display_requirement'].get('released') is True
        if not report['ok']:report['status']='failed'
        write(output/'comparison.json',report)
    return report


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts','save-fixture','source-report'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('original','sdl','driver','dosbox','build-dir'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--scenario',choices=oracle.SCENARIOS,required=True);parser.add_argument('--run',action='store_true');args=parser.parse_args(argv)
    if args.run and (not args.build_dir or any(getattr(args,key) is None or not getattr(args,key).is_file() for key in ('sdl','driver','dosbox'))):
        parser.error('--run需真实正式App/driver/DOSBox与其实际--build-dir，不接受S2机制App')
    try:report=run(args)
    except Exception as error:parser.exit(1,'S3正式输入工具未能准备：'+str(error)+'。检查来源与新输出目录。\n')
    print(json.dumps({key:report[key] for key in ('ok','status','scenario')},ensure_ascii=False));return int(args.run and not report['ok'])


if __name__=='__main__':raise SystemExit(main())
