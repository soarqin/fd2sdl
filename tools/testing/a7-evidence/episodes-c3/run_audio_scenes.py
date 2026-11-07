"""Root-only三项短音频场景；默认dry，无设备／窗口／显示API。"""
import argparse
import copy
from contextlib import contextmanager
import json
from pathlib import Path
import sys

sys.dont_write_bytecode=True
import options_load as options
from short_scenes import prepare_short
from native_reference import prepare_native

SCENARIOS=('sdl-options-only','sdl-load-only','opening18-to11','one-END19-12-19')


def display_state(snapshot):
    if not (snapshot.get('session_query_ok') is True and snapshot.get('same_as_active_console') is True
            and snapshot.get('SM_REMOTESESSION')==0 and all(snapshot.get(k,{}).get('ok') is True
            for k in ('console_display','session_display'))):
        raise RuntimeError('display query/session unknown or abnormal; no request/target')
    states=[snapshot[k].get('state') for k in ('console_display','session_display')]
    if states==[1,1]:return 'on'
    if states==[0,0]:return 'off'
    raise RuntimeError('mixed/dim/unknown display; no request/target')


@contextmanager
def display_scope(module,record):
    # 仅本A7离散资源/声音取证例外；不改Suite/B6连续帧的ON约束。
    record.update(scope='discrete_WGC_and_closed_audio',continuous_FPS_claimed=False,
        requested=False,acquired=False,released=False,separate_wake_API_called=False)
    before=module._display_snapshot();record['initial_snapshot']=before
    mode=display_state(before);record['admission_mode']=mode
    if mode=='off':
        record.update(status='known_off_no_display_request',SetThreadExecutionState_calls=0)
        try:yield record
        except BaseException as error:
            record['status']='failed';record['body_error']={'type':type(error).__name__,'message':str(error)}
            raise
    else:
        # 如果现有ctx申请/复核失败直接失败，不能再fallback成OFF。
        with module.windows_display_requirement(record):yield record


def run_cases(args,prepared,record):
    display=prepared['tools']['fd2_display_requirement']
    for name in SCENARIOS:
        row={'scenario':name,'status':'pending'};record['scenes'].append(row)
        row['display_before']=display._display_snapshot();row['display_before_state']=display_state(row['display_before'])
        case_args=copy.copy(args);case_args.artifacts=args.artifacts/name
        holder={}
        try:
            result=options.execute(case_args,prepared,_final_report=holder,scenario=name)
            row['result']=result
            if not result['collection_passed']:raise RuntimeError('case failed; remaining cases not executed')
            row['display_after']=display._display_snapshot();row['display_after_state']=display_state(row['display_after'])
            row['status']='collection-passed-sound-pending'
        except BaseException:
            if 'result' not in row and holder.get('value') is not None:row['result']=holder['value']
            row['status']='failed'
            raise
    record['collection_passed']=True


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ('root','manifest','game','driver','dosbox','fixture','artifacts'):parser.add_argument('--'+key,type=Path,required=True)
    parser.add_argument('--public-app',type=Path,default=Path('E:/Projects/fd2sdl/src/Debug/fd2sdl.exe'))
    parser.add_argument('--execute',action='store_true')
    args=parser.parse_args(argv);prepared=options.prepare(args);prepared['short']=prepare_short(prepared)
    prepared['native_reference']=prepare_native(prepared)
    prepared['inputs'].update(prepared['native_reference']['facts'])
    prepared['inputs']['three_case_worker']=options.fact(__file__)
    prepared['inputs']['short_scenes']=options.fact(Path(__file__).with_name('short_scenes.py'))
    prepared['inputs']['native_reference_module']=options.fact(Path(__file__).with_name('native_reference.py'))
    if not args.execute:
        print(json.dumps(dict(dry=True,scenarios=SCENARIOS,GUI_started=False,device_opened=False,power_API=False,
            source_files=len(prepared['source']),original50=len(prepared['original_facts']),
            opening29=prepared['short']['prefix_description'],terminal_fragment2_acknowledged=False,
            binary_source_inputs=prepared['inputs'],opening_title_counts=prepared['short']['title_counts'])))
        return 0
    report=dict(schema='fd2-a7-three-short-scenes-v1',ok=False,collected_only=True,
        collection_passed=False,entire_A7_passed=False,scenes=[]);display={};error=None;owned=False
    try:
        prepared['tools']['run_original_battle_animation'].owned_job_required()
        with display_scope(prepared['tools']['fd2_display_requirement'],display):
            args.artifacts.mkdir(parents=True,exist_ok=False);owned=True
            run_cases(args,prepared,report)
    except BaseException as caught:
        error=caught;report['collection_passed']=False
        report['error']={'type':type(caught).__name__,'message':str(caught)}
    report['display_requirement']=display
    report['status']='collection-passed-sound-pending' if report['collection_passed'] else 'failed'
    if owned:
        try:
            with (args.artifacts/'comparison.json').open('x',encoding='utf-8') as stream:json.dump(report,stream,ensure_ascii=False,indent=2)
        except OSError as caught:
            report['collection_passed']=False;report['status']='failed'
            report['report_write_error']={'type':type(caught).__name__,'message':str(caught)}
    print(json.dumps(report,ensure_ascii=False))
    return 0 if report['collection_passed'] else 130 if isinstance(error,KeyboardInterrupt) else 1


if __name__=='__main__':raise SystemExit(main())
