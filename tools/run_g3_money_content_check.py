"""G3已有paired WGC/全SAV的金额滚动离线门；不启动GUI、不改原报告。

26a7a/26b91逐列九切片、26c9b复制6x9；原raw6x99/187d6数字31..40。
只验完整48x9/max3的前值→非末态滚动→后值，不声称所有90ms阶段或时刻等价。
"""
from pathlib import Path
import argparse
import json
import struct
import sys

from PIL import Image

ROOT=Path(__file__).resolve().parents[1];sys.path.insert(0,str(ROOT/'tools'))
import fd2_g3_prepare_oracle as oracle
import fd2_g3_prepare_pages as pages
from fd2_magic_list_oracle import digit
from rebuild_fd2_analysis import parse_le,parse_objects,parse_fixups,build_relbase_image,apply_fixups
from run_original_comparison import archive_entry,normalize
from run_graphics_tests import decode_save
from run_g3_promotion_content_check import maximum,file_sha,sha,write

SCENARIOS={'p1-buy-equip-yes':(1,True),'p2-sell-equipped':(1,False),'p3-revive':(4,True)}


def source_contract(root):
    original=root/'original_game';exe=(original/'FD2.EXE').read_bytes()
    if sha(exe)!=oracle.EXE_SHA256:raise ValueError('金额离线仅接受已证完整原EXE')
    le=parse_le(exe);objects=parse_objects(exe,le)
    image=apply_fixups(build_relbase_image(exe,le,objects),parse_fixups(exe,le,objects))
    # G1完整Domain及G3 naming readset的固定whole chunks，不从本轮截图拟合。
    expected={0x26a7a:(279,'c50045ea229470ca08c86b1f1ff30ece2fec3aafffe04fbf2e0c59bac8bba68a'),
        0x26b91:(266,'0e30371f6947b2d6083f6cefe1fa19cba58e0203f4f339d490b3c68e66b7999e'),
        0x26c9b:(73,'690437f6a28691df44a757ca358f6fdb88987b7a4bce5fa8451ee1396f90482d')}
    functions=[]
    for va,(size,value) in expected.items():
        if len(image[va:va+size])!=size or sha(image[va:va+size])!=value:raise ValueError('金额原完整函数SHA不同')
        functions.append({'cva':hex(va),'size':size,'sha256':value,'legal_relocated_source_equal':True})
    return {'functions':functions,'files':{str(original/name):file_sha(original/name) for name in ('FD2.EXE','FDOTHER.DAT')}}


def templates(root,selector,old,new,subtract):
    if not 0<=old<=0x7fffffff or not 0<=new<=0x7fffffff:raise ValueError('本原生代表不扩到原负号越目录')
    other=(root/'original_game/FDOTHER.DAT').read_bytes();palette=archive_entry(other,0)[:768]
    palette=[(c<<2)|(c>>4) for c in palette];sheet=archive_entry(other,12 if selector==1 else 14)
    strip=pages._image(pages.member(sheet,2),palette,raw=True,size=(6,99))
    panel=pages._image(pages.member(sheet,1),palette,lmi=True,size=(63,15))
    numbers=archive_entry(other,5)
    def static(value):
        canvas=panel.crop((11,3,59,12));canvas.putpalette(palette)
        for column,char in enumerate(f'{value:08d}'[:8]):
            image=digit(numbers,31+int(char));canvas.paste(image,(6*column,1),image.point(lambda c:255 if c else 0,mode='L'))
        return canvas
    before=static(old).convert('RGB');after=static(new).convert('RGB');canvas=static(old)
    digits=[int(c) for c in f'{old:08d}'[:8]];target=[int(c) for c in f'{new:08d}'[:8]];result=[]
    for cycle in range(10):
        moving=[a!=b for a,b in zip(digits,target)]
        if not any(moving):break
        if subtract:digits=[(d-1)%10 if yes else d for d,yes in zip(digits,moving)]
        for phase in range(9):
            for column,yes in enumerate(moving):
                if yes:
                    line=9*digits[column]+(8-phase if subtract else phase+1)
                    if not 0<=line<=90:raise ValueError('原raw6x99切片越界')
                    canvas.paste(strip.crop((0,line,6,line+9)),(6*column,0))
            image=canvas.convert('RGB')
            if 0<phase<8 and maximum(image,before)>3 and maximum(image,after)>3:
                result.append({'image':image,'cycle':cycle,'phase':phase,'digits':list(digits),'moving':moving})
        if not subtract:digits=[(d+1)%10 if yes else d for d,yes in zip(digits,moving)]
    else:raise ValueError('原八位金额循环未在有界10cycle收敛')
    if digits!=target or not result:raise ValueError('没有非末态滚动原资源模板')
    broken=result[0]['image'].copy();p=broken.getpixel((0,0));broken.putpixel((0,0),tuple(255-v for v in p))
    negative={'before_is_not_roll':all(maximum(before,row['image'])>3 for row in result),
        'after_is_not_roll':all(maximum(after,row['image'])>3 for row in result),
        'one_omitted_ROI_pixel_rejected':maximum(broken,result[0]['image'])>3}
    if not all(negative.values()):raise ValueError('金额完整ROI门缺少区分力')
    return before,after,result,negative


def analyze(root,path,out):
    raw=path.read_bytes();report=json.loads(raw);scenario=report.get('scenario')
    if scenario not in SCENARIOS or report.get('ok') is not True or report.get('final_sources_unchanged') is not True:
        raise ValueError('需要已通过的P1购买/P2出售/P3复活配对原生收据')
    if sorted(c['kind'] for c in report['cases'])!=['original','sdl']:raise ValueError('需要唯一原版/SDL配对')
    for name in ('FD2.EXE','FDOTHER.DAT'):
        if file_sha(root/'original_game'/name)!=report.get('original_before',{}).get(name,{}).get('sha256'):
            raise ValueError('当前原资源与该原生收据不同')
    selector,subtract=SCENARIOS[scenario]
    encoded,plain,before,after,manifest=oracle.derive_fixture(root/'original_game',scenario)
    fixture=path.parent/'controlled.SAV'
    if fixture.read_bytes()!=encoded:raise ValueError('金额夹具不是完整独立原容器')
    first,last,rolling,negative=templates(root,selector,before['resource'],after['resource'],subtract)
    all_cases=[]
    for case in report['cases']:
        if case.get('ok') is not True:raise ValueError('不能签失败端金额内容')
        capture=case['animation_capture'];summary=Path(capture['summary_path']);save=Path(case['completed_save']['path'])
        if file_sha(save)!=case['completed_save']['sha256'] or file_sha(summary)!=capture['summary_sha256']:
            raise ValueError('真实SAV或WGC收据SHA不同')
        if capture.get('capture_backend')!='Windows.Graphics.Capture HWND' or \
            capture.get('summary',{}).get('ok') is not True or capture['summary'].get('stop_reason')!='stop':
            raise ValueError('金额帧不是正常结束的owned WGC采集')
        state=oracle.check_save(plain,decode_save(save),after,case['completed_save']['slot'])
        if not state['ok']:raise ValueError('真实22987B保存后态不符')
        pid=capture['ready']['pid'];hwnd=capture['ready']['hwnd'];hits={'before':[],'rolling':[],'after':[]}
        if len(case.get('processes',[]))!=1 or case['processes'][0].get('pid')!=pid or \
            case['processes'][0].get('cleaned') is not True or case['processes'][0].get('forced_cleanup') is not False:
            raise ValueError('金额WGC不属于本次正常清理的唯一owned进程')
        sealed={str(path):sha(raw),str(fixture):file_sha(fixture),str(summary):file_sha(summary),str(save):file_sha(save)}
        previous=-1
        for row in capture['frames']:
            file=Path(row['path'])
            if row['index']<=previous or row['pid']!=pid or row['hwnd']!=hwnd or file_sha(file)!=row['sha256']:raise ValueError('WGC原PNG/PID/HWND/序号不同')
            previous=row['index']
            sealed[str(file)]=row['sha256']
            with Image.open(file) as frame:roi=normalize(frame).crop((16,98,64,107))
            hit={'index':row['index'],'path':str(file),'sha256':row['sha256'],'timestamp_ms':row['timestamp_ms']}
            if maximum(roi,first)<=3:hits['before'].append(hit)
            if maximum(roi,last)<=3:hits['after'].append(hit)
            for source in rolling:
                difference=maximum(roi,source['image'])
                if difference<=3:
                    hits['rolling'].append({**hit,'cycle':source['cycle'],'phase':source['phase'],
                        'digits':source['digits'],'moving':source['moving'],'max_channel_difference':difference});break
        order=[];index=-1
        for group in ('before','rolling','after'):
            hit=next((row for row in hits[group] if row['index']>index),None)
            if not hit:break
            order.append({'stage':group,**hit});index=hit['index']
        result={'kind':case['kind'],'ok':len(order)==3,'wallet_before':before['resource'],'wallet_after':after['resource'],
            'subtract':subtract,'checked_ROI':{'x':16,'y':98,'width':48,'height':9,'all_pixels':432,'max_RGB_difference':3},
            'counts':{group:len(rows) for group,rows in hits.items()},'ordered_representatives':order,
            'negative_controls':negative,'whole_Save':state,'sealed_files':sealed,
            'scope':{'nonterminal_phase1_to7_only':True,'all_cycles_or_timing_equivalent':False,'new_GUI_started':False}}
        work=out/case['kind'];work.mkdir();first.save(work/'before-source.png');last.save(work/'after-source.png')
        if len(order)>1:
            match=order[1];next(row['image'] for row in rolling if row['cycle']==match['cycle'] and row['phase']==match['phase']).save(work/'rolling-source.png')
        write(work/'comparison.json',result);all_cases.append(result)
    return {'scenario':scenario,'ok':all(case['ok'] for case in all_cases),'cases':all_cases}


def main():
    parser=argparse.ArgumentParser(description=__doc__);parser.add_argument('--root',type=Path,default=ROOT)
    parser.add_argument('--reports',type=Path,nargs='+',required=True);parser.add_argument('--artifacts',type=Path,required=True)
    args=parser.parse_args();root=args.root.resolve();out=args.artifacts.resolve()
    if out.exists() or out.is_relative_to(root/'original_game'):raise ValueError('需要新的隔离离线输出目录')
    out.mkdir(parents=True);source=source_contract(root)
    tools={str(Path(module.__file__).resolve()):file_sha(module.__file__) for module in tuple(sys.modules.values())
        if getattr(module,'__file__',None) and Path(module.__file__).resolve().is_relative_to(ROOT/'tools')}
    tools[str(Path(__file__).resolve())]=file_sha(__file__)
    report={'ok':False,'source':source,'tool_files':tools,'cases':[],'new_GUI_started':False,'original_reports_modified':False}
    for path in args.reports:
        path=path.resolve(strict=True);scenario=json.loads(path.read_text(encoding='utf-8'))['scenario']
        work=out/scenario;work.mkdir();report['cases'].append(analyze(root,path,work))
    sealed={**source['files'],**tools}
    for case in report['cases']:
        for endpoint in case['cases']:sealed.update(endpoint['sealed_files'])
    report['final_files_unchanged']=all(Path(path).is_file() and file_sha(path)==value for path,value in sealed.items())
    report['ok']=report['final_files_unchanged'] and all(case['ok'] for case in report['cases']);write(out/'comparison.json',report)
    print(json.dumps({'ok':report['ok'],'cases':[{'scenario':case['scenario'],'ok':case['ok'],
        'counts':[sub['counts'] for sub in case['cases']]} for case in report['cases']]},ensure_ascii=False))
    return 0 if report['ok'] else 1


if __name__=='__main__':raise SystemExit(main())
