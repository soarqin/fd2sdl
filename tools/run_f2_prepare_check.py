"""F2准备页单一受控代表：复用G3 p2-self-give的原资源及完整SAV oracle。

原版后SDL串行，owned run-job/WGC/输入/清理由G3持有；只补显示ON及当前来源门。
不签自然到达、完整RNG/严格phase轨迹、全像素或音频等价，不修改共享默认配置。
"""
from __future__ import annotations
import argparse
from contextlib import contextmanager
import json
from pathlib import Path
import sys

from run_f2_input_check import sha, source_facts, validate_output
from run_original_battle_animation import owned_job_required
from run_original_comparison import quiet_game_command
import original_reference
import run_g3_prepare_check as g3

SCENARIO='p2-self-give'


def command_configuration(command):
    """读取实际quiet helper构造出的有序-conf/-set，不推定默认配置。"""
    configs,overrides=[],[]
    for index,argument in enumerate(command):
        if argument in ('-conf','-set'):
            if index+1>=len(command):raise ValueError('DOSBox配置参数缺少值')
            value=command[index+1]
            if argument=='-conf':configs.append(Path(value).resolve(strict=True))
            else:overrides.append(value)
    if not configs:raise ValueError('原版启动命令缺少实际-conf')
    return configs,overrides


def read_frozen(path):
    path=Path(path).resolve(strict=True);stamp=path.stat().st_mtime_ns
    identity,raw=original_reference.read_input(path)
    if path.stat().st_mtime_ns!=stamp:raise ValueError('冻结期间文件修改时间改变：'+str(path))
    return {**identity,'mtime_ns':stamp},raw


def adapt_legacy_g3_rate(command,env,bootstrap):
    """只迁移旧G3的44100声明；目标由冻结参照及bootstrap共同确认。"""
    actual,_,_=quiet_game_command(command,env)
    configs,overrides=command_configuration(actual)
    reference_identity,reference_raw=read_frozen(original_reference.REFERENCE)
    reference=original_reference.parse_json(reference_raw.decode('utf-8'))
    if reference.get('schema')!=original_reference.REFERENCE_SCHEMA:raise ValueError('原参照schema不符')
    source=reference['configuration_source'];target=source['inherited_settings']['mixer.rate']
    bootstrap_identity,bootstrap_raw=read_frozen(bootstrap)
    if any(bootstrap_identity[key]!=source['bootstrap'][key] for key in ('sha256','size')):
        raise ValueError('bootstrap用户配置身份不符，未调整rate')
    bootstrap_settings,_=original_reference.parse_config(bootstrap_raw)
    if target!='48000' or bootstrap_settings.get('mixer.rate')!=target:
        raise ValueError('未登记的参照/bootstrap mixer.rate，未调整rate')
    config_inputs=[read_frozen(path) for path in configs]
    settings,_=original_reference.declared_config([bootstrap_raw]+[raw for _,raw in config_inputs],overrides)
    previous=settings.get('mixer.rate')
    if previous not in ('44100',target):raise ValueError('未知G3 mixer.rate，未调整：'+str(previous))
    adjusted=list(command)
    if previous=='44100':adjusted+=['-set','mixer rate='+target]
    actual,_,_=quiet_game_command(adjusted,env)
    return adjusted,actual,{'setting':'mixer.rate','from':previous,'to':target,'applied':previous!=target,
        'policy':'known legacy G3 44100 to pinned 48000; command-local only',
        'reference':reference_identity,'bootstrap':bootstrap_identity,
        'configs':[identity for identity,_ in config_inputs],
        'original_command':list(command),'actual_command':actual,
        'config_files_modified':False,'global_settings_modified':False}


def run(args):
    owned_job_required()
    root=args.root.resolve();original=(root/'original_game').resolve();out=args.artifacts.resolve()
    validate_output(original,out)
    original_before=g3.fingerprint(original);sources=source_facts(root)
    paths=(args.game.resolve(),args.driver.resolve(),args.dosbox.resolve(),
        args.bootstrap.resolve(),Path(original_reference.REFERENCE).resolve())
    guards={str(p):{'sha256':sha(p),'mtime_ns':p.stat().st_mtime_ns} for p in paths}
    out.mkdir(parents=True,exist_ok=False)
    report={'schema':'fd2.f2.prepare-native.v1','ok':False,'scenario':SCENARIO,
        'source_before':sources,'original_before':original_before,'guards':guards,
        'display_requirement':{},'references':[],'rate_adaptations':[],'launch_attempts':[],'launched_roles':[],
        'controlled_fixture':True,'natural_arrival_claimed':False,
        'scope':'G3 title LOAD, service self-give, town return and hotel full-container Save',
        'limitations':['G3 original-resource page gates and complete 22987-byte SAV oracle retained',
            'Controlled fixture; no natural arrival or full RNG/FIFO/strict phase trajectory qualification',
            'Legacy palette/resource tolerances retained; no full-pixel or audio equivalence']}
    previous_running,previous_display=g3.running,g3.windows_display_requirement

    def checks():
        return {'original_unchanged':g3.fingerprint(original)==original_before,
            'sources_unchanged':source_facts(root)==sources,
            'guards_unchanged':all(sha(p)==v['sha256'] and Path(p).stat().st_mtime_ns==v['mtime_ns']
                for p,v in guards.items())}

    @contextmanager
    def display_on(record):
        report['display_requirement']=record
        with previous_display(record,require_display_on=True):yield record

    @contextmanager
    def qualified_running(command,cwd,env,log,driver,receipts,**options):
        expected=('original','sdl')[len(report['launched_roles'])] if len(report['launched_roles'])<2 else None
        executable=Path(command[0]).resolve()
        role='original' if executable==args.dosbox.resolve() else 'sdl' if executable==args.game.resolve() else None
        if role is None or role!=expected:raise ValueError('准备代表必须原版后SDL各一次串行启动')
        if not all(checks().values()):raise ValueError('准备代表冻结来源在窗口前改变')
        # running内部也调用这一已冻结的helper；先构造真实策略argv作来源资格门，
        # 再在进入body前核对实际Popen收据，不能只在报告中声称加了-set。
        if role=='original':
            command,actual_command,adaptation=adapt_legacy_g3_rate(command,env,args.bootstrap)
            report['rate_adaptations'].append(adaptation)
        else:actual_command,_,_=quiet_game_command(command,env)
        report['launch_attempts'].append({'role':role,'command':actual_command})
        if role=='original':
            configs,overrides=command_configuration(actual_command)
            for identity in adaptation['configs']:
                guards[identity['path']]={key:identity[key] for key in ('sha256','mtime_ns')}
            reference=original_reference.build(original,args.dosbox,configs,overrides,args.bootstrap)
            if reference['configs']!=[{key:identity[key] for key in ('path','size','sha256')}
                    for identity in adaptation['configs']]:
                raise ValueError('G3配置在rate核对后改变，未启动')
            report['references'].append(reference)
            if not all(checks().values()):raise ValueError('参照资格核对后、启动前来源改变')
        count=len(receipts)
        with previous_running(command,cwd,env,log,driver,receipts,**options) as process:
            if len(receipts)!=count+1 or receipts[-1].get('command')!=actual_command:
                raise ValueError('实际启动命令与参照资格命令不同')
            report['launched_roles'].append(role)
            yield process

    try:
        g3.running=qualified_running;g3.windows_display_requirement=display_on
        result=g3.run(argparse.Namespace(root=root,artifacts=out/'g3',sdl=args.game,
            driver=args.driver,dosbox=args.dosbox,scenario=SCENARIO,original_evidence=None,prepare_only=False))
        report['comparison']=str(out/'g3/comparison.json');report['legacy_ok']=result['ok']
        report['ok']=result['ok'] and report['launched_roles']==['original','sdl'] and len(report['references'])==1
        if not result['ok']:
            report['error']={'type':'G3PrepareComparisonFailed','text':result.get('error','see G3 comparison')}
    except BaseException as error:
        report['error']={'type':type(error).__name__,'text':str(error)}
    finally:
        g3.running=previous_running;g3.windows_display_requirement=previous_display
        for name,check in (
            ('original_unchanged',lambda:g3.fingerprint(original)==original_before),
            ('sources_unchanged',lambda:source_facts(root)==sources),
            ('guards_unchanged',lambda:all(sha(p)==v['sha256'] and Path(p).stat().st_mtime_ns==v['mtime_ns']
                for p,v in guards.items()))):
            try:report[name]=check()
            except Exception as error:
                report[name]=False
                report.setdefault('guard_errors',[]).append({'guard':name,'type':type(error).__name__,'text':str(error)})
        report['ok'] &= all(report.get(name) is True for name in ('original_unchanged','sources_unchanged','guards_unchanged'))
        report['ok'] &= report['display_requirement'].get('require_display_on') is True and report['display_requirement'].get('released') is True
        (out/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    return report


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','game','driver','dosbox','bootstrap','artifacts'):
        parser.add_argument('--'+name,type=Path,required=True)
    args=parser.parse_args(argv)
    if sys.platform!='win32':parser.error('此代表要求Windows WGC；未启动GUI')
    try:result=run(args)
    except (OSError,ValueError,RuntimeError) as error:
        parser.exit(1,'F2准备代表未能启动：'+str(error)+'。请核对来源或运行条件。\n')
    print(json.dumps({k:result.get(k) for k in ('ok','error','original_unchanged','guards_unchanged')},ensure_ascii=False))
    return 0 if result['ok'] else 1


if __name__=='__main__':sys.exit(main())
