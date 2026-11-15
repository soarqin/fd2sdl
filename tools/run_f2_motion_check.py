"""F2普通移动／取消的正式输入代表；复用原START和既有player oracle。

原版后SDL串行运行。附加严格显示ON、当前来源及配置资格，不扩大旧像素门。
必须从owned run-job启动；不构造新状态，不改原存档、系统混音器或电源设置。
"""
from __future__ import annotations
import argparse
from contextlib import contextmanager
import json
import os
from pathlib import Path
import sys

from fd2_display_requirement import windows_display_requirement
from run_f2_input_check import sha, source_facts, validate_output
from run_original_battle_animation import owned_job_required
from run_original_comparison import hashes
import original_reference
import run_original_player as player


def run(args):
    owned_job_required()
    root=args.root.resolve();original=(root/'original_game').resolve();out=args.artifacts.resolve()
    validate_output(original,out)
    original_before=hashes(original)
    original_mtimes={str(p.relative_to(original)):p.stat().st_mtime_ns
                     for p in original.rglob('*') if p.is_file()}
    sources=source_facts(root)
    paths=(args.game.resolve(),args.driver.resolve(),args.dosbox.resolve(),args.bootstrap.resolve())
    guards={str(p):{'sha256':sha(p),'mtime_ns':p.stat().st_mtime_ns} for p in paths}
    out.mkdir(parents=True,exist_ok=False)
    report={'schema':'fd2.f2.motion-native.v1','ok':False,'source_before':sources,'guards':guards,
        'display_requirement':{},'references':[],
        'scope':'unaltered original START: ordinary actor0 move, command cancel and return; real input and WGC',
        'limitations':['Legacy player resource/palette tolerance retained; no full-pixel equivalence',
            'No intermediate FIFO/RNG trace, exact clock/read time or full runtime-state equivalence',
            'No QK, deaths, AI item79 mode6, all script branches or natural reachability expansion']}
    previous_running=player.running
    previous_seed=os.environ.pop('FD2SDL_FIELD_RNG_SEED',None)

    @contextmanager
    def qualified_running(command,*pos,**kw):
        # 原runner已写实际-conf，尚未启动窗口；此处只给测试入口附加来源门。
        if Path(command[0]).resolve()==args.dosbox.resolve():
            config=Path(command[command.index('-conf')+1]).resolve()
            report['references'].append(original_reference.build(original,args.dosbox,
                [config],['mixer nosound=true'],args.bootstrap))
        with previous_running(command,*pos,**kw) as process:
            yield process

    try:
        player.running=qualified_running
        with windows_display_requirement(report['display_requirement'],require_display_on=True):
            result=player.run(argparse.Namespace(root=root,artifacts=out/'player',
                game=args.game,driver=args.driver,dosbox=args.dosbox,
                save_fixture=original/'START.SAV',scenario='move-cancel'))
            report['comparison']=str(out/'player/comparison.json')
            report['legacy_ok']=result['ok']
            report['ok']=result['ok'] and len(report['references'])==1
            if not result['ok']:report['error']={'type':'PlayerComparisonFailed','text':result.get('error','see comparison')}
    except BaseException as error:
        report['error']={'type':type(error).__name__,'text':str(error)}
    finally:
        player.running=previous_running
        if previous_seed is not None:os.environ['FD2SDL_FIELD_RNG_SEED']=previous_seed
        checks={
            'original_unchanged':lambda:hashes(original)==original_before,
            'original_mtimes_unchanged':lambda:{str(p.relative_to(original)):p.stat().st_mtime_ns
                for p in original.rglob('*') if p.is_file()}==original_mtimes,
            'sources_unchanged':lambda:source_facts(root)==sources,
            'guards_unchanged':lambda:all(sha(p)==v['sha256'] and Path(p).stat().st_mtime_ns==v['mtime_ns']
                for p,v in guards.items())}
        for name,check in checks.items():
            try:report[name]=check()
            except Exception as error:
                report[name]=False
                report.setdefault('guard_errors',[]).append({'guard':name,'type':type(error).__name__,'text':str(error)})
        report['ok']=report['ok'] and all(report[name] for name in checks)
        (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','game','driver','dosbox','bootstrap','artifacts'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    if sys.platform!='win32':parser.error('此代表要求Windows WGC；未启动GUI')
    try:result=run(args)
    except (OSError,ValueError,RuntimeError) as error:
        parser.exit(1,'F2移动代表未能启动：'+str(error)+'。请核对来源或运行条件。\n')
    print(json.dumps({k:result.get(k) for k in ('ok','error','original_unchanged','guards_unchanged')},ensure_ascii=False))
    return 0 if result['ok'] else 1


if __name__=='__main__':sys.exit(main())
