"""P4现存WGC的离线资源正门；不启动游戏、不改原收据或正式工具。

2fb2c/2fe14原顺序→BG0..2/FIGANI unit*3；旧16tick末帧渐白20步，
新24tick。白化最小门用原clearwork/BG y50/oldpart最小y共同定义的顶带；
完整白化帧另报，不从截图选裁切线。不把采样序列当263present全帧/时刻等价。
"""
from pathlib import Path
import argparse
import hashlib
import json
import struct
import sys

from PIL import Image,ImageChops

ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import fd2_g3_prepare_oracle as oracle
import run_g3_prepare_check as base
from fd2_battle_animation_frames import figani_parts
from run_original_comparison import archive_entry,decode_image,normalize
from rebuild_fd2_analysis import parse_le,parse_objects,parse_fixups,build_relbase_image,apply_fixups


def sha(data):return hashlib.sha256(data).hexdigest()
def file_sha(path):return sha(Path(path).read_bytes())
def write(path,data):path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
def maximum(a,b):return max(value[1] for value in ImageChops.difference(a,b).getextrema())


SCENARIOS={'p4-promote-normal':(4,36,2),'p4-promote-special90':(9,52,1)}


def verify_source(root):
    original=root/'original_game';exe=(original/'FD2.EXE').read_bytes()
    if sha(exe)!=oracle.EXE_SHA256:raise ValueError('原EXE来源不同')
    le=parse_le(exe);objects=parse_objects(exe,le)
    relocated=apply_fixups(build_relbase_image(exe,le,objects),parse_fixups(exe,le,objects))
    # G1完整Domain chunks与strict LE闭证的常量，不要求分发原机器码/ignored数据库。
    expected={0x2fb2c:(744,'a284f1654cab702018e826582f9a5b816aef001e35e474d880e33d09438a8d0b'),
        0x2fe14:(237,'ee33ee0961ce3f432bb11943141abc2359ad89ace2110c71883c11cb8fbc7fb4')}
    functions=[]
    for va,(size,value) in expected.items():
        raw=relocated[va:va+size]
        if len(raw)!=size or sha(raw)!=value:
            raise ValueError('原完整Domain chunk/实际EXE不同：'+hex(va))
        functions.append({'cva':hex(va),'size':len(raw),'sha256':sha(raw),'whole_EXE_legal_relocated_bytes_equal':True})
    return {'functions':functions,'Domain_source_reference':'G1 preparation/evidence.json; common-preparation-semantics; promotion-presentation-notes',
        'files':{str(original/name):file_sha(original/name) for name in ('FD2.EXE','FDOTHER.DAT','FIGANI.DAT','BG.DAT')}}


def templates(root,old_id,new_id):
    original=root/'original_game';figani=(original/'FIGANI.DAT').read_bytes();bg=(original/'BG.DAT').read_bytes()
    palette=archive_entry((original/'FDOTHER.DAT').read_bytes(),0)[:768]
    if len(palette)!=768 or max(palette)>63:raise ValueError('调色板不是原6bit通道')
    backgrounds=[decode_image(archive_entry(bg,i)) for i in range(3)]
    if any(image.size!=(320,100) for image in backgrounds):raise ValueError('BG不是有证320x100')
    def frame(part,background,white=0):
        canvas=Image.new('P',(320,200),0);canvas.paste(backgrounds[background],(0,50))
        canvas.paste(part['image'],part['at'],part['mask'])
        channels=[min(channel+white,63) for channel in palette]
        canvas.putpalette([(channel<<2)|(channel>>4) for channel in channels]);return canvas.convert('RGB')
    result={'old':[],'white':[],'new':[]};facts={};last_old=None;blank_height=50
    for name,identity,ticks in (('old',old_id,16),('new',new_id,24)):
        parts,fact=figani_parts(figani,identity*3);facts[name]={'unit_id':identity,**fact}
        if not parts:raise ValueError('本代表不是empty：不能按空图伪证身份')
        if name=='old':blank_height=min(blank_height,min(part['at'][1] for part in parts))
        part=age=background=0
        for tick in range(ticks):
            background=(background+1)%3;image=frame(parts[part],background)
            result[name].append({'label':f'{name}.tick{tick}.part{part}.bg{background}',
                'image':image,'sample':image.resize((32,20),Image.Resampling.NEAREST)})
            if name=='old':last_old=(parts[part],background)
            age+=1
            if age==parts[part]['duration']:age=0;part=(part+1)%len(parts)
    for step in range(1,20):
        image=frame(*last_old,white=step*3)
        result['white'].append({'label':f'white.step{step}.delta{step*3}',
            'image':image,'sample':image.resize((32,20),Image.Resampling.NEAREST),
            'blank_band':image.crop((0,0,320,blank_height)),'blank_height':blank_height})
    # 负例：完整像素门不允许换身份、省精灵、不白化或删一个白化写点。
    old=result['old'][0]['image'];new=result['new'][0]['image'];white=result['white'][-1]['image']
    blank=Image.new('P',(320,200),0);blank.paste(backgrounds[1],(0,50));blank.putpalette([(x<<2)|(x>>4) for x in palette]);blank=blank.convert('RGB')
    broken=white.copy();broken.putpixel((0,0),(0,0,0))
    negatives={'wrong_identity_rejected':maximum(old,new)>3,
        'missing_sprite_rejected':all(maximum(blank,row['image'])>3 for row in result['old']),
        'unwhitened_old_rejected':all(maximum(old,row['image'])>3 for row in result['white']),
        'one_deleted_white_pixel_rejected':maximum(broken,white)>3}
    if not all(negatives.values()):raise ValueError('独立source门缺少必要区分力')
    return result,facts,negatives


def analyze(root,report_path,scenario,out,old_id,new_id,actor):
    raw=report_path.read_bytes();report=json.loads(raw)
    if report.get('scenario')!=scenario or report.get('ok') is not True or report.get('final_sources_unchanged') is not True:
        raise ValueError('需要该scenario已完整通过的真实原生收据')
    for name in ('FD2.EXE','BG.DAT','FIGANI.DAT','FDOTHER.DAT'):
        if file_sha(root/'original_game'/name)!=report.get('original_before',{}).get(name,{}).get('sha256'):
            raise ValueError('当前原资源与原生收据冻结的完整源不同：'+name)
    prepared=base.prepare(root,scenario);base.finalize_ready_waits(prepared,scenario)
    if (report_path.parent/'controlled.SAV').read_bytes()!=prepared['encoded'] or \
        prepared['before']['roster'][actor*80+7]!=old_id or prepared['after']['roster'][actor*80+7]!=new_id:
        raise ValueError('完整人工夹具/原独立模型身份不同')
    expected,facts,negatives=templates(root,old_id,new_id);all_cases=[]
    if sorted(case.get('kind','') for case in report['cases'])!=['original','sdl']:
        raise ValueError('需要唯一original及SDL配对，不能重复原端伪称paired')
    for case in report['cases']:
        if case.get('ok') is not True:raise ValueError('不能提升失败端为呈现通过')
        capture=case['animation_capture'];summary=Path(capture['summary_path'])
        if file_sha(summary)!=capture['summary_sha256'] or capture['capture_backend']!='Windows.Graphics.Capture HWND' or \
            capture['summary'].get('ok') is not True or capture['summary'].get('stop_reason')!='stop':
            raise ValueError('原WGC目录/正常停止收据不同')
        pid=capture['ready']['pid'];hwnd=capture['ready']['hwnd'];hits={key:[] for key in (*expected,'restored_service')};full_white=[]
        sealed={str(summary):file_sha(summary)};rows=capture['frames'];previous=-1
        for row in rows:
            path=Path(row['path'])
            if row['index']<=previous or row['pid']!=pid or row['hwnd']!=hwnd or file_sha(path)!=row['sha256']:
                raise ValueError('实际WGC PNG身份/PID/HWND/序号不同')
            previous=row['index'];sealed[str(path)]=row['sha256']
            with Image.open(path) as raw_image:image=normalize(raw_image)
            sample=image.resize((32,20),Image.Resampling.NEAREST)
            for group,candidates in expected.items():
                for candidate in candidates:
                    if group=='white':
                        actual=image.crop((0,0,320,candidate['blank_height']))
                        difference=maximum(actual,candidate['blank_band']);pixels=320*candidate['blank_height']
                    else:difference=maximum(image,candidate['image']) if maximum(sample,candidate['sample'])<=3 else 255;pixels=64000
                    if difference<=3:
                        hits[group].append({'index':row['index'],'timestamp_ms':row['timestamp_ms'],
                            'path':str(path),'sha256':row['sha256'],'source_frame':candidate['label'],
                            'checked_RGB_pixels':pixels,'max_channel_difference':difference,
                            'source_ROI':'原clearwork且BG和old FIGANI均未覆盖的完整320x'+str(candidate['blank_height']) if group=='white' else '完整320x200'})
                        break
            if any(maximum(sample,row['sample'])<=3 and maximum(image,row['image'])<=3 for row in expected['white']):
                full_white.append(row['index'])
            matched=prepared['checks']['service-returned'][0](image)
            if matched:hits['restored_service'].append({'index':row['index'],'path':str(path),'sha256':row['sha256'],'source_ROI':matched})
        order=[];last=-1
        for group in ('old','white','new','restored_service'):
            found=next((row for row in hits[group] if row['index']>last),None)
            if found:order.append({'stage':group,**found});last=found['index']
            else:break
        ok=len(order)==4;work=out/case['kind'];work.mkdir()
        for group,candidates in expected.items():
            label=next((row['source_frame'] for row in order if row['stage']==group),None)
            if label:next(row['image'] for row in candidates if row['label']==label).save(work/(group+'-independent-source.png'))
        result={'kind':case['kind'],'ok':ok,'pid':pid,'hwnd':hwnd,'checked_WGC_frames':len(rows),
            'source_hit_counts':{key:len(value) for key,value in hits.items()},'ordered_representatives':order,
            'complete_white_frame_matches':full_white,
            'source_FIGANI':facts,'negative_controls':negatives,'sealed_actual_files':sealed,
            'scope':{'old_new_representatives_full_64000_RGB_max3':True,
                'white_source_clear_band_only':True,'complete_white_frame_equivalence_required':False,
                'restored_service_independent_ROI_only':True,'exact_saved_page_restore_bytes_validated':False,
                'every_present_or_delay_or_263_present_equivalence':False,'waveform_or_music_identity_validated':False}}
        write(work/'comparison.json',result);all_cases.append(result)
    return {'scenario':scenario,'ok':len(all_cases)==2 and all(case['ok'] for case in all_cases),
        'source_report':str(report_path),'source_report_sha256':sha(raw),'cases':all_cases}


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--root',type=Path,default=ROOT)
    parser.add_argument('--reports',type=Path,nargs='+',required=True,help='既有P4 paired comparison.json，可仅核一个代表')
    parser.add_argument('--artifacts',type=Path,required=True,help='新的离线输出目录，不覆盖旧native收据')
    args=parser.parse_args(argv);root=args.root.resolve();out=args.artifacts.resolve()
    reports=[path.resolve(strict=True) for path in args.reports]
    scenarios=[json.loads(path.read_text(encoding='utf-8')).get('scenario') for path in reports]
    if any(name not in SCENARIOS for name in scenarios) or len(set(scenarios))!=len(scenarios):
        raise ValueError('仅接受唯一P4普通/特殊90配对报告')
    if out.exists() or out.is_relative_to(root/'original_game'):raise ValueError('需要新的隔离离线产物目录')
    out.mkdir(parents=True)
    tool_files={str(Path(module.__file__).resolve()):file_sha(module.__file__) for module in tuple(sys.modules.values())
        if getattr(module,'__file__',None) and Path(module.__file__).resolve().is_relative_to(ROOT/'tools')}
    tool_files[str(Path(__file__).resolve())]=file_sha(__file__)
    report={'ok':False,'sources':verify_source(root),'tool_files':tool_files,'cases':[],'new_GUI_started':False,'new_capture_started':False,
        'original_reports_modified':False,'expected_from_SDL_state_or_cold_JSON':False}
    for path,scenario in zip(reports,scenarios):
        old,new,actor=SCENARIOS[scenario]
        case_out=out/scenario;case_out.mkdir()
        report['cases'].append(analyze(root,path,scenario,case_out,old,new,actor))
    sealed={**report['sources']['files'],**tool_files}
    for case in report['cases']:
        sealed[case['source_report']]=case['source_report_sha256']
        for endpoint in case['cases']:sealed.update(endpoint['sealed_actual_files'])
    report['final_files_unchanged']=all(Path(path).is_file() and file_sha(path)==value for path,value in sealed.items())
    report['ok']=report['final_files_unchanged'] and all(case['ok'] for case in report['cases']);write(out/'comparison.json',report)
    print(json.dumps({'ok':report['ok'],'cases':[{'scenario':case['scenario'],'ok':case['ok'],
        'counts':[sub['source_hit_counts'] for sub in case['cases']]} for case in report['cases']]},ensure_ascii=False))
    return 0 if report['ok'] else 1


if __name__=='__main__':raise SystemExit(main())
