#!/usr/bin/env python3
"""S5后批两条正式App链；默认只准备，--run才由owned Job串行原版/SDL。

26受控近胜→真实controller→victory→27 NoTown/24618/活动Save；29近胜→
hot30/resource30→credits/20pair montage→原终端静止页。来源模型不读冷图。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import time

from PIL import Image
from native import s5_late_fixture as fixture
from native import s3_fixture as state
import run_s5_stage_check as host
import run_s3_stage_check as shared
import fd2_natural_oracle as natural
import fd2_g3_prepare_pages as prepare_pages
import fd2_g3_deployment_pages as deployment
import fd2_battle_animation_frames as animation
from audit_g4_render_source import archive_entries, text_fragments
from fd2_opening_oracle import build_pages, page_match, portraits, raw_ui
from fd2_transition_oracle import _text
from run_original_battle_menu import wait_title, match_indexed
from run_original_comparison import archive_entry, decode_image, normalize, compare
from run_original_end_turn import RoundOracle, META
from run_original_player import exact_tile
from field_play_input_test import wait_log
from run_g2_battle_check import series
from static_data.s5_output import validate_output


def pages_for(p,records,stage,fragments,roster):
    mapping={}
    for unit in records:
        if not unit[5]&1:mapping.setdefault(unit[8],unit[7])
    for i in range(31):mapping.setdefault(roster[i*80+8],roster[i*80+7])
    return build_pages(p['original'],p['assets'],stage_plan=((stage,max(fragments)+1),),
        identities_by_stage={stage:[u[7]for u in records]},text_identities_by_stage={stage:mapping},
        fragment_indices_by_stage={stage:fragments})


def notice_pages(p,portrait,fragment,*,text_entry=31):
    """31bdf有限通知：原84,119/19px、-3真实等待、16e24滚动；不冒普通story。"""
    tokens=text_fragments(archive_entries((p['original']/'FDTXT.DAT').read_bytes())[text_entry])[fragment]
    canvas=Image.new('P',(320,200));canvas.putpalette(p['assets']['palette'])
    font=p['prepare_assets']['font'];other=(p['original']/'FDOTHER.DAT').read_bytes();sheet=archive_entry(other,5)
    arrows=[raw_ui(sheet,i)for i in(18,19)];erase=raw_ui(sheet,13)
    if [image.size for image in arrows]!=[(14,10),(14,10)]or erase.size!=(16,16):
        raise ValueError('31bdf等待UI不是原14x10箭头/16x16清除页')
    for image in (*arrows,erase):image.putpalette(p['assets']['palette'])
    # 16c57沿bottom origin36887定位；与31bdf专属文字起点84无关。
    arrow_at=(191,181);erase_at=(191,176)
    pages=[];x,y,row=84,119,0
    def append(index,continuation):
        expected=canvas.copy()
        # 31bdf先绘制84,119文字，后绘制8,115完整80x80肖像；重叠区由下方全肖像门检查。
        expected.paste(0,(8,115,88,195))
        if continuation:expected.paste(0,(*arrow_at,arrow_at[0]+14,arrow_at[1]+10))
        pages.append({'stage':text_entry-1,'fragment':fragment,'token_offset':index,'reason':'31bdf continuation'if continuation else'31bdf wait0',
            'speaker_control':'fixed31bdf','speaker_argument':portrait,'portrait_id':portrait,
            'portrait_position':(8,115),'portraits':portraits(p['original'],portrait,p['assets']['palette'],True),
            'border_position':(5,112),'text':expected,'known_text_pixels':sum(v!=0 for v in expected.getdata()),
            'arrow_position':arrow_at if continuation else None,'arrows':arrows})
    for index,token in enumerate(tokens):
        if token in(-2,-3):
            if row==3:
                for delta in(3,3,3,3,3,4):
                    # 16e24固定bottom基点(95,119)，并非当前字形起点(84,119)。
                    canvas.paste(canvas.crop((94,119+delta,302,191+delta)),(94,119))
                    canvas.paste(74,(95,191,303,192))
                row-=1
            row+=1;x,y=84,119+row*19
            if token==-3:append(index,True);canvas.paste(erase,erase_at)
        elif token==-1:
            if index!=len(tokens)-1:raise ValueError('通知结束后有未知token')
            append(index,False)
        elif 0<=token<1824:
            if not 1<=x<=304 or not 0<=y<=182:raise ValueError('通知字形超出屏幕')
            canvas.paste(74,(x,y,x+16,y+16))
            if token!=10:
                glyph=font[token*32:(token+1)*32]
                if len(glyph)!=32:raise ValueError('通知字模截断')
                for gy in range(16):
                    bits=int.from_bytes(glyph[gy*2:gy*2+2],'big')
                    for gx in range(16):
                        if bits&(0x8000>>gx):
                            canvas.putpixel((x+gx,y+gy),205);canvas.putpixel((x+gx-1,y+gy+1),76);canvas.putpixel((x+gx,y+gy+1),76)
            x+=16
        else:raise ValueError('31bdf出现本代表未知动态token')
    if not pages or any(page['known_text_pixels']<256 for page in pages):raise ValueError('通知缺足够真实字形写点')
    return pages


def card_anchor(p,records,raw):
    unit=records[raw];canvas=Image.new('P',(320,200));canvas.putpalette(p['assets']['palette'])
    entry31=archive_entry((p['original']/'FDTXT.DAT').read_bytes(),31)
    common=p['prepare_assets']['text'];font=p['prepare_assets']['font']
    from fd2_reward_oracle import common_tokens
    for data,fragment,xy in((entry31,10,(105,18)),(common,unit[8]+1,(155,18)),
                            (entry31,11,(105,38)),(common,unit[32]+150,(155,38))):
        _text(canvas,font,common_tokens(data,fragment),*xy,205)
    entry=archive_entry((p['original']/'DATO.DAT').read_bytes(),unit[7]);images=[]
    from run_original_end_turn import decode_lmi
    for phase in(0,3):
        start=struct.unpack_from('<I',entry,phase*4)[0];end=struct.unpack_from('<I',entry,(phase+1)*4)[0]if phase<3 else len(entry)
        image=decode_lmi(entry[start:end]);image.putpalette(p['assets']['palette'])
        if image.size!=(80,80):raise ValueError('credits肖像不是完整80x80')
        images.append(image)
    background=decode_image(archive_entry((p['original']/'FDOTHER.DAT').read_bytes(),56))
    background.putpalette(p['assets']['palette'])
    return {'raw':raw,'unit_id':unit[7],'text_id':unit[8],'text':canvas,'portraits':images,
        'background_top':background.crop((0,0,320,4))}


def card_match(frame,expected):
    text=match_indexed(frame,expected['text'],(0,0))
    portrait=next(({'phase':phase*3,**found}for phase,image in enumerate(expected['portraits'])
        if(found:=exact_tile(frame,image,(8,10)))),None)
    background=exact_tile(frame,expected['background_top'],(0,0))
    return {'raw':expected['raw'],'unit_id':expected['unit_id'],'text_id':expected['text_id'],
        'text':text,'portrait':portrait,'background':background,
        'scope':'full80x80 portrait, original56 top4rows and exact four name/class labels, not full animated card'}if text['ok']and portrait and background else None


def ending_assets(p,records):
    raw=(p['original']/'FDOTHER.DAT').read_bytes();result={}
    for entry in(59,60):
        image=decode_image(archive_entry(raw,entry));image.putpalette(p['assets']['palette'])
        if image.size!=(320,200):raise ValueError('终章静态资源不是完整320x200')
        result[entry]=image.convert('RGB')
    parts,identity=animation.figani_parts(raw,58)
    if len(parts)!=20:raise ValueError('montage末字样源目录不是20')
    caption_palette=archive_entry(raw,57)
    if len(caption_palette)!=768 or max(caption_palette)>63:raise ValueError('montage字样57不是完整6bit palette')
    result['pair_anchors']=[]
    for index in(0,19):
        image=Image.new('P',(320,200));mask=Image.new('L',(320,200));animation._paste(image,mask,parts[index])
        anchor=animation._anchor(image,mask,(0,0),{'FDOTHER':58,'part':index,'palette_FDOTHER':57},[v<<2 for v in caption_palette])
        if not anchor or anchor['checked_pixels']<256:raise ValueError('montage字样缺真实RLE写点')
        result['pair_anchors'].append(anchor)
    result['first_card']=card_anchor(p,records,30);result['last_card']=card_anchor(p,records,1)
    result['identity']=identity;return result


def prepare(root,scenario):
    p=fixture.derive(Path(root)/'original_game',scenario);p['assets']=host.round_assets(p['original'])
    p['titles']=host.title_templates(p['original']/'FDOTHER.DAT');p['prepare_assets']=prepare_pages.load(p['original'])
    stage=p['source'][META+2]
    with state.resource_domain(p['original'],(26,27,29,30)):
        natural.selection_summary(p['original'],p['source'],p['assets'],0)
        if not natural.range_probe(p['original'],p['source'],p['assets'],0):raise ValueError('近胜raw0无合法选择范围')
        records=fixture.victory_records(p,'sdl');fragments=(8,13,14,15,16)if scenario==fixture.SCENARIOS[2]else(8,9,10,11,12)if stage==26 else(9,10)
        p['victory_pages']=pages_for(p,records,stage,fragments,p['manual_state']['roster'])
        if scenario==fixture.SCENARIOS[0]:
            p['preparation_expected']={kind:fixture.preparation27(p,kind)for kind in('original','sdl')}
            p['entry_expected']={kind:fixture.entry27(p,p['preparation_expected'][kind])for kind in('original','sdl')}
            view,records,_,selected=p['entry_expected']['sdl'];p['entry_pages']=pages_for(p,records,27,(0,),selected['roster'])
            if p['data']['raw'][0x523e7+27]!=1:raise ValueError('原27不是NoTown')
            deployment.selection(p['prepare_assets'],p['preparation_expected']['sdl'],[],0,19)
        else:
            records,_,prepared=fixture.credits30(p,'sdl')
            p['credits_pages']=pages_for(p,records,30,(0,1),prepared['roster'])if stage==29 else[]
            notices=((37,2),(21,3),(26,4),(105,5),(32,6),(45,7),(32,8),(36,9))if stage==29 else((4,17),(21,18),(24,19),(26,20),(32,21),(36,22),(32,23))
            p['notice_pages']=[page for portrait,fragment in notices
                for page in notice_pages(p,portrait,fragment,text_entry=31 if stage==29 else 27)]
            p['ending_assets']=ending_assets(p,records)
    p['manifest']['preflight']={'stage':stage,'actual_active_count':p['source'][META+1],
        'victory_wait_pages':len(p['victory_pages']),'source_only_API_used':False,'native_execution_claimed':False,
        'source':'complete250cc/33c9d or25757/31529/31c49 + original DAT/EXE/SAV'}
    return p


def snapshot_preflight(args,p):
    if not args.snapshot_probe:
        if args.run:raise ValueError('--run需--snapshot-probe真实同SHA导入预检')
        return
    exe=args.snapshot_probe.resolve(strict=True);out=validate_output(args.artifacts.with_name(args.artifacts.name+'-import'),original=p['original'])
    out.mkdir(parents=True);snapshot=out/'controlled.SAV';snapshot.write_bytes(p['encoded'])
    inputs=[exe,snapshot.resolve()]+[p['original']/name for name in
        ('FD2.EXE','FD2.SAV','FDOTHER.DAT','FDFIELD.DAT','FDSHAP.DAT','FDTXT.DAT','FDICON.B24','DATO.DAT')]
    before={str(path.resolve()):state.digest(path.read_bytes())for path in inputs}
    result=subprocess.run([str(exe),'--s5-snapshot',str(snapshot.resolve())],cwd=args.root,text=True,capture_output=True,timeout=90)
    stage=p['source'][META+2];wanted=f'S5 snapshot fixture: stage={stage} active={p["source"][META+1]} roster=31 complete_container_preserved=true RNG=0'
    unchanged=all(state.digest(Path(path).read_bytes())==digest for path,digest in before.items())
    passed=result.returncode==0 and result.stdout.strip()==wanted and not result.stderr.strip()and unchanged
    proof={'schema':'fd2-s5-late-snapshot-import-v1','passed':passed,'actual_C_executed':True,
        'stage':stage,'active_count':p['source'][META+1],'roster_count':31,'encoded_snapshot_sha256':state.digest(p['encoded']),
        'inputs_sha256':before,'inputs_unchanged':unchanged,'expected_stdout':wanted,
        'exit_code':result.returncode,'stdout':result.stdout,'stderr':result.stderr}
    shared.write(out/'comparison.json',proof)
    if not passed:raise ValueError('真实snapshot consumer拒绝夹具或来源变化：'+result.stdout+result.stderr)
    p['snapshot_import_preflight']=proof
    p['manifest']['snapshot_import_preflight']=proof


def acknowledge(capture,p,record,cp,pages,label):
    for index,page in enumerate(pages):
        cp(f'{label}-{index}',lambda f,page=page:page_match(f,page,p['assets']),timeout=120)
        capture.key('ENTER');record.setdefault('resource_acknowledgements',[]).append(
            {k:page[k]for k in('stage','fragment','token_offset','portrait_id')})


def continue_result(capture,p,record,cp):
    stage=p['source'][META+2];proof=p.get('snapshot_import_preflight')
    if not proof or proof.get('encoded_snapshot_sha256')!=state.digest(p['encoded'])or proof.get('passed')is not True:
        raise ValueError('首次CONTINUE缺同SHA真实导入资格')
    for path,digest in proof['inputs_sha256'].items():
        if state.digest(Path(path).read_bytes())!=digest:raise ValueError('实际开窗前snapshot资格失效')
    record['snapshot_import_preflight']=proof
    wait_title(capture,p['titles'][3][0],timeout=90);capture.key('UP');capture.wait(p['titles'][3][2]);capture.key('ENTER')
    cp(f's5-{stage}-formal-CONTINUE',lambda f:natural.focus_match(f,p['original'],p['source'],p['assets']),timeout=90)
    capture.key('Z');cp(f's5-{stage}-focus0',lambda f:{'focus':found[1]}if
       (found:=natural.actor_browse_anywhere(f,p['original'],p['source'],p['assets'],0))else None)
    view,_=natural.actor_browse_anywhere(capture.last,p['original'],p['source'],p['assets'],0)
    summary=natural.selection_summary(p['original'],view,p['assets'],0);ranges=natural.range_probe(p['original'],view,p['assets'],0)
    capture.key('ENTER');cp(f's5-{stage}-selection',lambda f:natural.focus_match(f,p['original'],view,p['assets'],summary=summary)
        if natural.range_match(f,ranges)else None)
    capture.key('ESC')
    acknowledge(capture,p,record,cp,p['victory_pages'],f's5-{stage}-victory-story')
    if record['kind']=='sdl':
        query=f'field result query: stage={stage} result=1 caller=player actor=0'
        lines=wait_log(capture.process,capture.artifacts/'game.log',query,timeout=30).splitlines()
        if [line for line in lines if line.startswith('field result query:')]!=[query]:raise AssertionError('缺唯一真实player结果查询或发生额外查询')
    record['controller_input']={'stage':stage,'actor':0,'selection_cancel':True,'result':1,
        'SDL_unique_post_log':record['kind']=='sdl','original_runtime_PC_or_latch_dumped':False}


def victory27(capture,p,save,guard,record):
    cp=shared.checkpoint(capture,record,save,guard);continue_result(capture,p,record,cp)
    st=p['preparation_expected'][record['kind']];assets=p['prepare_assets'];buttons=RoundOracle(p['assets'],(0,0))
    notice=prepare_pages.notice(assets,410,portrait=75,xy=(100,119))
    def choice(frame,selected):
        a=prepare_pages.notice_match(frame,notice);b=buttons.buttons(frame,selected)
        return dict(a,buttons=b)if a and b else None
    cp('s5-27-NoTown410',lambda f:choice(f,0),timeout=120)
    capture.key('RIGHT');cp('s5-27-NoTown-no',lambda f:choice(f,1));capture.key('ENTER')
    chosen=[]
    for raw in range(1,20):
        expected=deployment.selection(assets,st,chosen,raw-1,19)
        cp(f's5-27-choose-{raw}',lambda f,e=expected:deployment.match(f,e));chosen.append(raw);capture.key('ENTER')
    confirm=prepare_pages.notice(assets,658,portrait=75,xy=(95,119))
    cp('s5-27-confirm658',lambda f:dict(a,buttons=b)if(a:=prepare_pages.notice_match(f,confirm))and(b:=buttons.buttons(f,0))else None)
    capture.key('ENTER');acknowledge(capture,p,record,cp,p['entry_pages'],'s5-27-entry-story0')
    view,records,masks,selected=p['entry_expected'][record['kind']]
    cp('s5-27-browse',lambda f:natural.focus_match(f,p['original'],view,p['assets']),timeout=120)
    shared.save_activity(capture,p,view,save,guard,record,cp,'s5-27-after-NoTown',
        lambda before,after,v:state.check_activity(before,after,v,records,masks,p['data']['stages'][27]['metadata'],
            kind=record['kind'],fresh_metadata=True,roster=selected['roster']),load_available=True)
    record['source_family']={'formal_victory26_to27':True,'NoTown_actual_selected':chosen,'active_count':34,
        'handoff_roster_sha256':state.digest(st['roster']),'source_entry27_24618_and_groups6_7_reached':True,
        'physically_all_transition_frames_or_audio_claimed':False,'full_activity_container_checked':True}


def ending29(capture,p,save,guard,record):
    cp=shared.checkpoint(capture,record,save,guard);continue_result(capture,p,record,cp)
    acknowledge(capture,p,record,cp,p['credits_pages'],'s5-30-resource31-story')
    acknowledge(capture,p,record,cp,p['notice_pages'],'s5-ending-31bdf-notice')
    assets=p['ending_assets']
    cp('s5-credits-first-card30',lambda f:card_match(f,assets['first_card']),timeout=120)
    capture.key('ENTER') # 原31c49只将j设1，不截断本卡220帧；下一张才为raw1末卡。
    cp('s5-credits-last-card1',lambda f:card_match(f,assets['last_card']),timeout=120)
    cp('s5-ending-logo60',lambda f:compare(assets[60],normalize(f))if compare(assets[60],normalize(f))['ok']else None,
        timeout=120,allow_low_color_resource=True)
    for anchor in assets['pair_anchors']:
        cp('s5-montage-overlay-'+str(anchor['identity']['part']),lambda f,a=anchor:
            {'identity':a['identity'],'checked_pixels':a['checked_pixels']}if animation._anchor_match(normalize(f),a)else None,timeout=450)
    final=lambda f:compare(assets[59],normalize(f))if compare(assets[59],normalize(f))['ok']else None
    cp('s5-ending-final59',final,timeout=90,allow_low_color_resource=True)
    before=len(record['inputs']);time.sleep(.25)
    cp('s5-ending-final59-held',final,timeout=10,allow_low_color_resource=True)
    if capture.process.poll()is not None or any('key'in row or'delivery'in row for row in record['inputs'][before:]):
        raise AssertionError('终端前进程已退出或有补键')
    record['source_family']={'formal_victory29_to_resource30':p['source'][META+2]==29,'source_hot_stage':30 if p['source'][META+2]==29 else 26,'resource_id':30,'text_entry':31,
        'original_runtime_stage_or_PC_dumped':False,'source31c49_skip_key_count':1,'skip_does_not_end_current_card':True,
        'montage_ordered_overlays':[0,19],'final_whole_resource':59,'final_two_observations_without_input':True,
        'terminal_loop_basis':'complete25757/25975 self-jump after31529 return; final resource held and process alive before owned close',
        'all31cards_or20pair_physical_frames_or_audio_claimed':False,'no_manual_or_activity_Save_at_stage30':True}

    if p['source'][META+2]==26:
        record['source_family']['formal_victory26_bad_to_resource30']=True
        record['source_family']['terminal_loop_basis']='complete250cc/25462 self-jump after31529 return; final resource held and process alive before owned close'



def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in('root','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--scenario',choices=fixture.SCENARIOS,required=True)
    parser.add_argument('--sdl-only',action='store_true',
        help='只新增SDL证据；既有原版必须另作来源资格审查，不记为本次双端通过')
    for name in('sdl','driver','dosbox','build-dir','snapshot-probe'):parser.add_argument('--'+name,type=Path)
    parser.add_argument('--run',action='store_true');args=parser.parse_args(argv)
    if args.run and(not args.build_dir or any(getattr(args,k)is None or not getattr(args,k).is_file()for k in('sdl','driver','dosbox','snapshot_probe'))):
        parser.error('--run需要正式App/driver/DOSBox/build-dir及同SHA实际snapshot probe')
    try:
        p=prepare(args.root,args.scenario);snapshot_preflight(args,p)
        exercise=victory27 if args.scenario==fixture.SCENARIOS[0]else ending29
        is_bad=args.scenario==fixture.SCENARIOS[2]
        report=host.run_prepared(args,p,exercise,(26,27)if exercise is victory27 else(26,)if is_bad else(29,),args.scenario,
            ['natural whole-stage combat','all physical frames/RNG/audio equality','other late bindings','Linux/macOS'],
            kinds=('sdl',)if is_bad or args.sdl_only else('original','sdl'))
    except Exception as error:parser.exit(1,'S5后批工具准备失败：'+str(error)+'。检查来源和新输出目录。\n')
    print(json.dumps({k:report[k]for k in('ok','status','scenario')},ensure_ascii=False));return int(args.run and not report['ok'])


if __name__=='__main__':raise SystemExit(main())
