"""F3活动Load正式代表：隔离已有存档，严格核原cache写回及完整测量文件。

复用既有END→取消→Load→Save正常输入与WGC，不修改来源存档或构造新状态。
旧finite-Save helper的任意cache排除不用于此入口；不签自然生产来源或完整RNG。
"""
from __future__ import annotations
import argparse
from contextlib import contextmanager
import hashlib
import json
import os
from pathlib import Path
import sys
from types import SimpleNamespace

from fd2_display_requirement import windows_display_requirement
from run_f2_input_check import sha, source_facts, validate_output
from run_original_battle_animation import owned_job_required
from run_original_comparison import hashes
import original_reference
import run_original_load as load


def fixture_contract(plain):
    classes=load.active_load_cache_classes(plain)
    if len(plain)!=0x59cb or sum(plain[:-4])!=int.from_bytes(plain[-4:],'little'):
        raise ValueError('F3来源容器checksum无效')
    changes=[{'raw':raw,'saved':plain[load.UNITS+raw*load.STRIDE+2],'expected':value}
        for raw,value in enumerate(classes) if plain[load.UNITS+raw*load.STRIDE+2]!=value]
    if not changes:raise ValueError('本代表须已有至少一处旧cache class，不能用无写回差异的来源签新修复')
    return {'stage':plain[load.META+2],'turn':plain[load.META],'active_count':len(classes),
        'expected_classes':list(classes),'required_rewrites':changes,
        'input_bytes_modified':False,'natural_original_save_provenance_claimed':False}


def run(args):
    owned_job_required()
    root=args.root.resolve();original=(root/'original_game').resolve();out=args.artifacts.resolve()
    validate_output(original,out)
    fixture=args.save_fixture.resolve(strict=True)
    # contract和实际输入共用一次bytes快照；不能先解A、后登记B的身份。
    fixture_stamp=fixture.stat().st_mtime_ns
    fixture_bytes=fixture.read_bytes()
    fixture_guard={'sha256':hashlib.sha256(fixture_bytes).hexdigest(),'mtime_ns':fixture_stamp}

    def require_fixture():
        if fixture.read_bytes()!=fixture_bytes or fixture.stat().st_mtime_ns!=fixture_stamp:
            raise ValueError('来源存档在冻结后改变；未使用新版本代签同一输入')

    require_fixture()
    contract=fixture_contract(load.decode_save(SimpleNamespace(read_bytes=lambda:fixture_bytes)))
    require_fixture()
    sources=source_facts(root);original_before=hashes(original)
    original_mtimes={str(p.relative_to(original)):p.stat().st_mtime_ns for p in original.rglob('*') if p.is_file()}
    paths=(args.game.resolve(),args.driver.resolve(),args.dosbox.resolve(),args.bootstrap.resolve())
    guards={str(p):{'sha256':sha(p),'mtime_ns':p.stat().st_mtime_ns} for p in paths}
    guards[str(fixture)]=fixture_guard
    require_fixture()
    out.mkdir(parents=True,exist_ok=False)
    frozen_fixture=out/'source-fixture.SAV'
    with frozen_fixture.open('xb') as output:output.write(fixture_bytes)
    guards[str(frozen_fixture)]={'sha256':fixture_guard['sha256'],'mtime_ns':frozen_fixture.stat().st_mtime_ns}
    report={'schema':'fd2.f3.active-load-native.v1','ok':False,'source_before':sources,'guards':guards,
        'fixture_contract':contract,'display_requirement':{},'references':[],
        'fixture_snapshot':str(frozen_fixture),'fixture_copies':[],'reference_checks':[],
        'scope':'existing checksum-valid source copied without edits; actual END/Load/Save; original10010 cache classes and full measured file',
        'limitations':['Input producer and natural acquisition not proven; original acceptance observed only if run succeeds',
            'No pre-Load damage/AI/RNG equality or full input/timing/audio/pixel equivalence',
            'Same-path loaded opaque preservation; cross-file and fresh-tail sources remain separate'],
        'SDL_debug_seed_removed':False}
    previous_running=load.running;previous_seed=os.environ.pop('FD2SDL_FIELD_RNG_SEED',None)

    @contextmanager
    def qualified_running(command,*pos,**kw):
        reference_check=None
        require_fixture()
        if frozen_fixture.read_bytes()!=fixture_bytes:
            raise ValueError('隔离来源快照已改变；未启动游戏')
        if Path(command[0]).resolve()==args.dosbox.resolve():
            config=Path(command[command.index('-conf')+1]).resolve()
            reference=original_reference.build(original,args.dosbox,
                [config],['mixer nosound=true'],args.bootstrap)
            report['references'].append(reference)
            manifest=out/f'reference-{len(report["references"])}.json'
            with manifest.open('x',encoding='utf-8') as output:
                json.dump(reference,output,ensure_ascii=False,indent=2)
                output.write('\n')
            guards[str(manifest)]={'sha256':sha(manifest),'mtime_ns':manifest.stat().st_mtime_ns}
            # SHA取build所审的字节，不以后读的新配置覆盖旧身份。
            for row in reference['configs']:
                path=Path(row['path']).resolve()
                guards[str(path)]={'sha256':row['sha256'],'mtime_ns':path.stat().st_mtime_ns}
            reference_check={'manifest':str(manifest),'before':False,'after':False}
            report['reference_checks'].append(reference_check)
            original_reference.verify(manifest)
            reference_check['before']=True
            role='original';copy=out/'load/original/game/FD2.SAV'
        elif Path(command[0]).resolve()==args.game.resolve():
            # 旧B1.3b自身声明了调试seed；本新代表采用正式App默认启动流。
            if len(pos)<2:raise ValueError('Load runner缺显式环境，未启动SDL')
            args_list=list(pos);environment=dict(args_list[1]);environment.pop('FD2SDL_FIELD_RNG_SEED',None)
            args_list[1]=environment;pos=tuple(args_list);report['SDL_debug_seed_removed']=True
            role='sdl';copy=out/'load/sdl/FD2.SAV'
            if Path(environment.get('FD2SDL_SAVE_PATH','')).resolve()!=copy.resolve():
                raise ValueError('SDL实际存档路径不属于本轮隔离副本')
        else:raise ValueError('Load runner启动了未登记程序')
        if copy.read_bytes()!=fixture_bytes:
            raise ValueError('实际隔离存档与冻结来源快照不一致；未启动游戏')
        report['fixture_copies'].append({'role':role,'path':str(copy),
            'sha256':fixture_guard['sha256'],'equal_to_snapshot':True})
        body_failed=False
        try:
            with previous_running(command,*pos,**kw) as process:yield process
        except BaseException:
            body_failed=True
            raise
        finally:
            # 先让owned running清理，再核本轮配置。不得遮盖先发生的业务错。
            if reference_check is not None:
                try:
                    original_reference.verify(reference_check['manifest'])
                    reference_check['after']=True
                except Exception as error:
                    reference_check['error']={'type':type(error).__name__,'text':str(error)}
                    report.setdefault('guard_errors',[]).append({'guard':'reference_after','text':str(error)})
                    if not body_failed:raise

    try:
        load.running=qualified_running
        with windows_display_requirement(report['display_requirement'],require_display_on=True):
            result=load.run(argparse.Namespace(root=root,artifacts=out/'load',game=args.game,
                driver=args.driver,dosbox=args.dosbox,save_fixture=frozen_fixture,verify_focus_cycle=False))
            report['comparison']=str(out/'load/comparison.json');report['legacy_ok']=result['ok']
            report['ok']=result['ok'] and len(report['references'])==1 and report['SDL_debug_seed_removed']
            if not result['ok']:report['error']={'type':'ActiveLoadComparisonFailed','text':result.get('error','see comparison')}
    except BaseException as error:report['error']={'type':type(error).__name__,'text':str(error)}
    finally:
        load.running=previous_running
        if previous_seed is not None:os.environ['FD2SDL_FIELD_RNG_SEED']=previous_seed
        for name,check in (
            ('original_unchanged',lambda:hashes(original)==original_before),
            ('original_mtimes_unchanged',lambda:{str(p.relative_to(original)):p.stat().st_mtime_ns for p in original.rglob('*') if p.is_file()}==original_mtimes),
            ('sources_unchanged',lambda:source_facts(root)==sources),
            ('guards_unchanged',lambda:all(sha(Path(p))==v['sha256'] and Path(p).stat().st_mtime_ns==v['mtime_ns'] for p,v in guards.items()))):
            try:report[name]=check()
            except Exception as error:
                report[name]=False;report.setdefault('guard_errors',[]).append({'guard':name,'text':str(error)})
        report['ok'] &= all(report.get(k) is True for k in ('original_unchanged','original_mtimes_unchanged','sources_unchanged','guards_unchanged'))
        report['references_unchanged']=len(report['reference_checks'])==1 and all(
            row['before'] and row['after'] for row in report['reference_checks'])
        report['fixture_copies_verified']=[row['role'] for row in report['fixture_copies']]==['original','sdl']
        report['ok'] &= report['references_unchanged'] and report['fixture_copies_verified']
        report['ok'] &= report['display_requirement'].get('require_display_on') is True and report['display_requirement'].get('released') is True
        try:
            (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        except OSError as error:
            first=report.get('error',{}).get('text','F3 Load报告写入失败')
            raise RuntimeError(f'{first}; report_write_error={error}') from error
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','game','driver','dosbox','bootstrap','save-fixture','artifacts'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args()
    if sys.platform!='win32':parser.error('此代表要求Windows WGC；未启动GUI')
    try:result=run(args)
    except (OSError,ValueError,RuntimeError,AssertionError) as error:
        parser.exit(1,'F3活动Load代表未能启动：'+str(error)+'。请核对来源或运行条件。\n')
    print(json.dumps({k:result.get(k) for k in ('ok','error','original_unchanged','guards_unchanged')},ensure_ascii=False))
    return 0 if result['ok'] else 1


if __name__=='__main__':sys.exit(main())
