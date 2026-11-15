"""F2标题正常长按代表：一次DOWN/UP，实际目标重复，完整WGC菜单状态门。

仅检查标题输入消费，既有palette映射容差仍显式继承，不签F4像素或全FIFO轨迹。
必须由owned run-job启动；只在ON显示下串行开窗，原资源/存档及bootstrap只读。
"""
from __future__ import annotations
import argparse, hashlib, json, os, shutil, subprocess, sys
from pathlib import Path
from run_original_comparison import Capture, compare, hashes, running, title_templates
from run_original_battle_menu import wait_title
from run_original_battle_animation import owned_job_required
from fd2_display_requirement import windows_display_requirement
from run_original_opening import title_menu_counts
from run_graphics_tests import decode_save
import original_reference

def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def source_facts(root):
    names=subprocess.check_output(['git','ls-files','--cached','--others','--exclude-standard',
        'src','data/static','tools'],cwd=root,text=True,encoding='utf-8').splitlines()
    return {name:{'sha256':sha(root/name),'mtime_ns':(root/name).stat().st_mtime_ns}
        for name in sorted(set(names)) if (root/name).is_file()}
def validate_output(original,out):
    if out==original or original in out.parents:raise ValueError('产物不能写入原资源实际目录')
    if out.exists():raise ValueError('产物目录必须不存在')
def repeat_count(held_ms,delay=500,rate=33):
    return 1+(0 if held_ms<delay else 1+(held_ms-delay)//rate)
def hold_prediction(actual_ms,requested_ms):
    if type(actual_ms) is not int:
        raise ValueError('长按实际时长偏离声明，保留收据不签计数')
    calls,distance=_hold_prediction_ns(actual_ms*1000000,requested_ms)
    return calls,distance//1000000

def _hold_prediction_ns(actual_ns,requested_ms):
    if abs(actual_ns-requested_ms*1000000)>4000000:
        raise ValueError('长按实际时长偏离声明，保留收据不签计数')
    remainder=(actual_ns-500000000)%33000000
    distance=min(remainder,33000000-remainder)
    if actual_ns<500000000 or distance<12000000:
        raise ValueError('长按落入repeat阈值附近，保留结果但不签此代表')
    return 2+(actual_ns-500000000)//33000000,distance

def qpc_prediction(receipt,requested_ms):
    fields=('down_qpc','up_qpc','qpc_frequency')
    if receipt.get('qpc_valid') is not True or receipt.get('key_up_sent') is not True or \
            receipt.get('release')!=1 or any(type(receipt.get(k)) is not int for k in fields):
        raise ValueError('长按缺少成功UP与有效QPC收据，未用粗时钟回退')
    down,up,freq=(receipt[k] for k in fields)
    if down<=0 or up<down or freq<=0:
        raise ValueError('长按QPC读数/频率无效')
    elapsed_ns=(up-down)*1000000000//freq
    calls,distance=_hold_prediction_ns(elapsed_ns,requested_ms)
    return elapsed_ns,calls,distance

def run(args):
    owned_job_required()
    root=args.root.resolve();original=(root/'original_game').resolve();out=args.artifacts.resolve()
    validate_output(original,out)
    if any(k.startswith('DOSBOX_') for k in os.environ):raise ValueError('不允许未登记DOSBOX环境覆盖')
    original_before=hashes(original)
    original_mtimes={str(p.relative_to(original)):p.stat().st_mtime_ns for p in original.rglob('*') if p.is_file()}
    sources=source_facts(root)
    paths=(args.game.resolve(),args.driver.resolve(),args.dosbox.resolve(),args.bootstrap.resolve())
    guards={str(p):{'sha256':sha(p),'mtime_ns':p.stat().st_mtime_ns} for p in paths}
    out.mkdir(parents=True,exist_ok=False)
    report={'schema':'fd2.f2.input-native.v1','ok':False,'cases':[], 'guards':guards,
        'source_before':sources,
        'display_requirement':{},'source_scope':'normal title, one held-key make per case; 500/33 predicted by independent pinned DOSBox reset source',
        'limitations':['No runtime FIFO/RNG trace; hardware rate cannot be measured from WGC alone',
            'PostMessage hold duration is not measured target processing duration; only final modulo menu behavior is observed',
            'Title resource oracle retains legacy RGB palette tolerance3; no A14 full-pixel qualification',
            'No gameplay action, QK qualification, physical keyboard or all-locale equivalence']}
    try:
        game=out/'original-game';shutil.copytree(original,game)
        # 仅标题输入代表，选已知合法的原START容器作CONTINUE存在性前提。
        # 只复制到隔离FD2.SAV；不修改原件或构造HP/人数/随机状态。
        shutil.copy2(original/'START.SAV',game/'FD2.SAV')
        report['save_fixture']={'path':str(original/'START.SAV'),'sha256':sha(original/'START.SAV')}
        config=out/'dosbox.conf'
        config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
            '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n'
            '[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
        report['reference']=original_reference.build(original,args.dosbox,[config],['mixer nosound=true'],args.bootstrap)
        templates=title_templates(original/'FDOTHER.DAT')
        menu_counts=title_menu_counts(decode_save(game/'FD2.SAV'))
        sdl_save=out/'SDL-FD2.SAV';shutil.copy2(game/'FD2.SAV',sdl_save)
        with windows_display_requirement(report['display_requirement'],require_display_on=True):
            for role in ('original','sdl'):
                work=out/role;work.mkdir();case={'role':role,'ok':False,'inputs':[],'processes':[],'checkpoints':[]}
                report['cases'].append(case)
                env=os.environ.copy()
                for key in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED'):env.pop(key,None)
                if role=='original':command=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'];cwd=out
                else:command=[str(args.game.resolve())];cwd=root;env['FD2SDL_SAVE_PATH']=str(sdl_save)
                with running(command,cwd,env,work/'game.log',args.driver.resolve(),case['processes']) as process:
                    capture=Capture(args.driver.resolve(),process,work,case['inputs'])
                    count=menu_counts[role]
                    frame=wait_title(capture,templates[count][0]);case['menu_count']=count
                    frame.save(work/'title-default.png');selected=0
                    for key,ms,direction in [('DOWN',1176,1),('UP',780,-1)]:
                        capture.key(key,hold_ms=ms)
                        receipt=case['inputs'][-1]['delivery']
                        # 边界余量要覆盖投递误差，不让跨repeat边界的用例冒充精确计数。
                        elapsed_ns,calls,margin_ns=qpc_prediction(receipt,ms)
                        selected=(selected+direction*calls)%count
                        expected=templates[count][selected]
                        frame=capture.wait(expected,timeout=3)
                        name=key.lower()+'-'+str(ms);frame.save(work/(name+'.png'))
                        case['checkpoints'].append({'name':name,'qpc_delivery_held_ns':elapsed_ns,
                            'coarse_tick_held_ms':receipt['up_tick_ms']-receipt['down_tick_ms'],
                            'predicted_make_count':calls,'repeat_threshold_distance_ns':margin_ns,
                            'repeat_threshold_distance_ms':margin_ns/1000000,'exact_produced_count_claimed':False,
                            'expected_selection':selected,'legacy_resource_gate':True,'frame_sha256':sha(work/(name+'.png'))})
                case['ok']=True
        first,second=report['cases']
        report['pair_same_menu_count']=first['menu_count']==second['menu_count']
        report['pair_same_checkpoints']=[(c['name'],c['expected_selection']) for c in first['checkpoints']]==[(c['name'],c['expected_selection']) for c in second['checkpoints']]
        report['ok']=all(c['ok'] for c in report['cases']) and report['pair_same_menu_count'] and report['pair_same_checkpoints']
    except BaseException as e:report['error']={'type':type(e).__name__,'text':str(e)}
    finally:
        checks={
            'original_unchanged':lambda:hashes(original)==original_before,
            'original_mtimes_unchanged':lambda:{str(p.relative_to(original)):p.stat().st_mtime_ns for p in original.rglob('*') if p.is_file()}==original_mtimes,
            'sources_unchanged':lambda:source_facts(root)==sources,
            'guards_unchanged':lambda:all(sha(p)==v['sha256'] and Path(p).stat().st_mtime_ns==v['mtime_ns'] for p,v in guards.items())}
        for name,check in checks.items():
            try:report[name]=check()
            except Exception as e:
                report[name]=False
                report.setdefault('guard_errors',[]).append({'guard':name,'type':type(e).__name__,'text':str(e)})
        report['ok']=report['ok'] and all(report[name] for name in checks)
        (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return report

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('root','game','driver','dosbox','bootstrap','artifacts'):p.add_argument('--'+name,type=Path,required=True)
    a=p.parse_args();r=run(a)
    print(json.dumps({k:r.get(k) for k in ('ok','error','original_unchanged','guards_unchanged')},ensure_ascii=False))
    return 0 if r['ok'] else 1
if __name__=='__main__':sys.exit(main())
