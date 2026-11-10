#!/usr/bin/env python3
"""G3最小隐藏店代表：ShiftF1→500→bank63→六商品→取消，不交易/Save。

只从原EXE/资源生成期望，默认prepare-only。公共pages/town/Capture和产品不改；
原版与SDL串行owned/WGC/quiet，完整22987B及mtime全程不写。其余29chord
只保来源/纯输入映射声明，不冒称实机覆盖或开放关卡。
"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shutil

import fd2_g3_prepare_oracle as oracle
import fd2_g3_prepare_pages as pages
from fd2_transition_oracle import derive_prepare_fixture,picker_image,picker_match,town_images,_text
from fd2_reward_oracle import common_tokens
from run_original_player import sheet_frame,exact_tile
from run_original_comparison import Capture,running,title_templates,compare,key_chord_contract
from run_original_load import require_source_unchanged
from run_original_battle_animation import owned_job_required
from fd2_display_requirement import windows_display_requirement
from run_g2_battle_check import freeze,unchanged,fingerprint,sha
from run_g3_cross_save_check import final_source_guard
import run_g3_prepare_check as service


def matches_hidden_chord(row,selected,key):
    contract=key_chord_contract(key)
    return bool(contract and row[1]==selected and row[2]==contract['bios_scan_expected'])


def hidden_town_images(assets,state,x,y):
    """265ec原312x192内页；500与selector5 marker坐标独立，不改公共五项模板。"""
    result=[]
    for phase in (0,1,2):
        view=assets['background'].crop((0,0,312,192));view.putpalette(assets['palette'])
        view.paste(assets['panel'],(244,162))
        _text(view,assets['font'],common_tokens(assets['text'],500),252,168,205,background=74)
        marker=sheet_frame(assets['icons'],state['roster'][7]*12+phase);marker.putpalette(assets['palette'])
        view.paste(marker,(x,y),marker.point(lambda value:255 if value else 0,mode='L'))
        result.append(view)
    return result


def town_match(frame,variants,selector):
    for phase,image in enumerate(variants):
        found=exact_tile(frame,image,(4,4))
        if found:return {'kind':'hidden_town_caption' if selector==5 else 'town',
            'selected':selector,'fragment':495+selector,'phase':phase,'position':[4,4],
            'size':[312,192],'source_cva':['0x26152','0x265ec'],**found}
    return None


def hidden_menu_match(frame,assets,notice):
    found=pages.menu_match(frame,assets,5,0,notice)
    if not found:return None
    bank=assets['services'][5]
    # 26ce4先清74；4ed34→4ed4f的lodsb/or al,al/jz明确跳0。
    # 完整比较原74底+透明icon合成，不能把资源四角0要求为屏幕黑。
    icons=[]
    for i in range(4):
        source=bank['commands'][2*i+(i==0)];composite=source.copy()
        composite.paste(74,(0,0,source.width,source.height))
        composite.paste(source,(0,0),source.point(lambda value:255 if value else 0,mode='L'))
        icons.append(exact_tile(frame,composite,(201+26*i,169)))
    background=exact_tile(frame,bank['background'].crop((220,8,316,88)),(220,8))
    if not background or not all(icons):return None
    return {**found,'kind':'hidden_shop_menu','complete_raw_icons':icons,
        'bank63_background_roi':[220,8,96,80],'background':background}


def prepare(root):
    original=Path(root)/'original_game';encoded,plain,state,fixture=derive_prepare_fixture(original)
    if state['stage']!=1 or state['count']!=5:raise ValueError('隐藏代表需已证stage1五人原夹具')
    assets=pages.load(original);tables,table_source=oracle.tables(original);exe=(original/'FD2.EXE').read_bytes()
    row=oracle.source_range(exe,0x6238d,31);xs=oracle.source_range(exe,0x52363,18);ys=oracle.source_range(exe,0x52375,18)
    if row[:3]!=bytes((0,0,84)) or not matches_hidden_chord(row,0,'SHIFT_F1'):
        raise ValueError('原stage1 type/selector/scan不再是0/0/84')
    stock=[]
    for identity in row[23:31]:
        if identity==255:break
        stock.append(identity)
    if len(stock)!=6:raise ValueError('原stage1隐藏库存不再是六项')
    sheet=pages.archive_entry(assets['other'],63)
    hidden={'bank':63,'sheet':sheet,'background':pages._image(pages.member(sheet,0),assets['palette'],size=(320,200)),
        'page':pages._image(pages.member(sheet,16),assets['palette'],lmi=True,size=(310,86)),
        'commands':[pages._image(pages.member(sheet,i),assets['palette'],raw=True,size=(24,20)) for i in range(3,11)],
        'price':pages._image(pages.member(sheet,15),assets['palette'],raw=True,size=(6,8))}
    if any(224<=pixel<=239 for pixel in hidden['background'].crop((220,8,316,88)).getdata()):
        raise ValueError('隐藏底图ROI含水DAC索引，不能以静态RGB门假装完整')
    assets['services'][5]=hidden  # 本次局部assets，不改公共pages或冷表。
    titles=title_templates(original/'FDOTHER.DAT');regular=town_images(original,assets,state,0)
    secret=hidden_town_images(assets,state,xs[5],ys[5]);notice=pages.notice(assets,440,selector=5)
    products=pages.items(assets,tables,stock,0,selector=5)
    checks={};steps=[]
    def add(name,predicate,key=None,low=False,**details):
        checks[name]=(predicate,{'timeout':30,'allow_low_color_resource':low});steps.append({'name':name,'key':key,**details})
    add('title-load-selected',lambda f:{'kind':'title_load','selected':1} if compare(titles[3][1],f)['ok'] else None,'DOWN')
    picker=picker_image(assets,plain,0);add('title-slot0',lambda f:picker_match(f,picker),'ENTER',True,slot=0)
    add('town0-before-chord',lambda f:town_match(f,regular,0),'ENTER',selected=0)
    add('hidden-caption500',lambda f:town_match(f,secret,5),'SHIFT_F1',selected=5,fragment=500,
        expected_chord=key_chord_contract('SHIFT_F1'))
    add('hidden-greeting440',lambda f:hidden_menu_match(f,assets,notice),'ENTER',bank=63,selector=5,fragment=440)
    add('hidden-buy-six-products',lambda f:pages.page_match(f,products,kind='hidden_six_products',selected=0,
        source=['0x26a0d','0x272d0','0x27079']),'ENTER',stock=stock,prices=[oracle.word(oracle.item(tables,i),19) for i in stock])
    add('hidden-menu-after-cancel',lambda f:hidden_menu_match(f,assets,notice),'ESC',bank=63,selector=5)
    add('town5-after-cancel',lambda f:town_match(f,secret,5),'ESC',selected=5,fragment=500)
    add('town0-after-left',lambda f:town_match(f,regular,0),'LEFT',selected=0)
    chords=[{'key':f'{modifier}_F{n}',**key_chord_contract(f'{modifier}_F{n}'),
        'stage1_selector0_matches':matches_hidden_chord(row,0,f'{modifier}_F{n}'),
        'native_requested':modifier=='SHIFT' and n==1} for modifier in ('SHIFT','CTRL','ALT') for n in range(1,11)]
    manifest={'scope':'one ShiftF1/hidden63 browser representative; no transaction/save/battle',
        'fixture_source':fixture,'source_tables':table_source,'stage_row_cva':'0x6238d','stage_row_hex':row.hex(),
        'stage_row_first3':list(row[:3]),'town_xy_cva':['0x52363','0x52375'],'hidden_marker':[xs[5]+4,ys[5]+4],
        'hidden_bank':63,'bank_sha256':oracle.digest(sheet),'stock':stock,
        'prices':[oracle.word(oracle.item(tables,i),19) for i in stock],'caption_fragment':500,'greeting_fragment':440,
        'input_chords_source_mapping_only':chords,'only_one_native_chord_requested':True,
        'source_files_are_read_only':True,'full_SAV_bytes_and_mtime_must_remain_unchanged':True}
    return {'original':original,'encoded':encoded,'plain':plain,'state':state,'assets':assets,'titles':titles,
        'checks':checks,'steps':steps,'manifest':manifest,'source_views':{'hidden-caption500':secret,'hidden-buy-six-products':[products]}}


def exercise(capture,prepared,save,guard,record):
    service.wait_title(capture,prepared['titles'][3][0],timeout=90);require_source_unchanged(save,*guard)
    for step in prepared['steps']:
        if step['key']:capture.key(step['key'])
        predicate,options=prepared['checks'][step['name']]
        service.wait_state(capture,step['name'],predicate,record['checkpoints'],**options)
        require_source_unchanged(save,*guard)
    record['browser_sequence_completed']=True


def run(args):
    root=args.root.resolve();original=(root/'original_game').resolve(strict=True);output=args.artifacts.resolve()
    if output.exists() or output==original or output.is_relative_to(original):raise ValueError('隐藏代表需新隔离目录')
    before=fingerprint(original);prepared=prepare(root)
    if fingerprint(original)!=before:raise ValueError('隐藏prepare期间原资源改变')
    binaries=[p.resolve() for p in (args.sdl,args.driver,args.dosbox) if p is not None]
    inputs,source_original,sources=freeze(root,original,binaries);output.mkdir(parents=True)
    fixture=output/'controlled.SAV';fixture.write_bytes(prepared['encoded']);inputs[str(fixture)]=sha(fixture)
    service.write_json(output/'fixture-manifest.json',prepared['manifest'])
    for name,images in prepared['source_views'].items():
        for n,image in enumerate(images):image.save(output/(name+'-source-'+str(n)+'.png'))
    report={'ok':False,'status':'ready_not_executed','cases':[],'inputs':inputs,'original_before':source_original,
        'source_files':sources,'display_requirement':{},'steps':prepared['steps'],'scope':prepared['manifest'],
        'expected_source_pages_are_not_execution':True,'audio_identity_claimed':False}
    try:
        if not args.execute:return report
        owned_job_required()
        with windows_display_requirement(report['display_requirement']),final_source_guard(report,inputs,original,source_original,root,sources):
            for kind in ('original','sdl'):
                if not unchanged(inputs,original,source_original,root,sources):raise ValueError('隐藏窗口前来源变化')
                work=output/kind;work.mkdir();env=os.environ.copy()
                for key in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED','FD2SDL_APP_PCM_CAPTURE_DIR',
                    'FD2SDL_APP_PCM_CAPTURE_MAX_MS','FD2SDL_APP_PCM_CAPTURE_QUIET','FD2SDL_TEST_AUDIO_QUIET'):env.pop(key,None)
                if kind=='original':
                    game=work/'game';shutil.copytree(original,game);save=game/'FD2.SAV';config=work/'dosbox.conf'
                    config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                        '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n'
                        '[mixer]\nrate=44100\n[cpu]\ncore=normal\ncycles=fixed 20000\n'
                        '[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
                    command,cwd=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'],work
                else:
                    save=work/'FD2.SAV';env['FD2SDL_SAVE_PATH']=str(save);command,cwd=[str(args.sdl.resolve())],root
                save.write_bytes(prepared['encoded']);guard=[save.read_bytes(),save.stat().st_mtime_ns]
                case={'kind':kind,'ok':False,'processes':[],'inputs':[],'checkpoints':[]};report['cases'].append(case)
                try:
                    with running(command,cwd,env,work/'game.log',args.driver.resolve(),case['processes']) as process:
                        capture=Capture(args.driver.resolve(),process,work,case['inputs']);capture.g3_case_record=case
                        exercise(capture,prepared,save,guard,case)
                    case['ok']=True
                finally:
                    case['guards']={'sources_unchanged':unchanged(inputs,original,source_original,root,sources),
                        'full_SAV_and_mtime_unchanged':save.read_bytes()==guard[0] and save.stat().st_mtime_ns==guard[1],
                        'cleanup':len(case['processes'])==1 and all(p['cleaned'] and not p['forced_cleanup'] and p['exit_code']==0 for p in case['processes'])}
                    case['SAV_observation']={'checked_bytes':oracle.FILE_SIZE,'sha256':sha(save),'mtime_ns':save.stat().st_mtime_ns,'write_requested':False}
                    case['ok'] &= all(case['guards'].values());service.write_json(work/'comparison.json',case)
                if not case['ok']:raise ValueError('隐藏代表SAV/退出守卫失败')
        report['ok']=len(report['cases'])==2 and all(c['ok'] for c in report['cases'])
        report['status']='one_hidden_entry_browser_paired_passed' if report['ok'] else 'failed'
    except BaseException as error:
        service.record_failure(report,error);report['status']='failed'
    finally:
        report['final_sources_unchanged']=unchanged(inputs,original,source_original,root,sources)
        if not report['final_sources_unchanged']:report['status']='failed'
        if args.execute:
            report['ok'] &= (report['final_sources_unchanged'] and report.get('sources_before_display_release') is True and
                report['display_requirement'].get('released') is True)
            if not report['ok']:report['status']='failed'
        service.write_json(output/'comparison.json',report)
    return report


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    for name in ('sdl','driver','dosbox'):parser.add_argument('--'+name,type=Path)
    mode=parser.add_mutually_exclusive_group();mode.add_argument('--execute',action='store_true');mode.add_argument('--prepare-only',action='store_true')
    args=parser.parse_args(argv)
    if args.execute and any(getattr(args,key) is None or not getattr(args,key).is_file() for key in ('sdl','driver','dosbox')):
        parser.error('--execute需现有App/driver/DOSBox；默认只准备')
    try:result=run(args)
    except (OSError,ValueError,KeyError,AssertionError) as error:
        print(json.dumps({'status':'failed','error':str(error)},ensure_ascii=False));return 1
    print(json.dumps({k:result[k] for k in ('ok','status')},ensure_ascii=False))
    return 0 if result['ok'] or (not args.execute and result['status']=='ready_not_executed') else 1


if __name__=='__main__':raise SystemExit(main())
