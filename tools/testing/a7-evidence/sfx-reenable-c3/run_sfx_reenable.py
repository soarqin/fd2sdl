"""唯一SFX重新启用补证；默认只读dry，执行须root自有Job。"""
import argparse
import json
from pathlib import Path
import sys
sys.dont_write_bytecode=True
import options_load as options
from reenable_case import declaration,verify_pair


def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ('root','manifest','game','driver','dosbox','fixture','artifacts'):
        parser.add_argument('--'+key,type=Path,required=True)
    parser.add_argument('--public-app',type=Path,default=Path('E:/Projects/fd2sdl/src/Debug/fd2sdl.exe'))
    parser.add_argument('--execute',action='store_true')
    args=parser.parse_args(argv)
    prepared=options.prepare(args)
    for key,name in [('entry_worker',__file__),('reenable_case',str(Path(__file__).with_name('reenable_case.py'))),('save_confirmation',str(Path(__file__).with_name('save_confirmation.py'))),('prior_original_audio_module',str(Path(__file__).with_name('prior_original_audio.py')))]:
        prepared['inputs'][key]=options.fact(name)
    if not args.execute:
        print(json.dumps(dict(dry=True,GUI_started=False,device_opened=False,power_API=False,
            source_files=len(prepared['source']),original50=len(prepared['original_facts']),
            declaration=declaration(),fixture_derivation=prepared['fixture_derivation'],
            binary_source_inputs=prepared['inputs'],prior_original_audio=prepared['prior_original_audio']['summary'],strict_display_ON_required=True,artifacts=str(args.artifacts)),ensure_ascii=False))
        return 0
    prepared['tools']['run_original_battle_animation'].owned_job_required()
    display={};report=None;partial={};error=None
    try:
        with prepared['tools']['fd2_display_requirement'].windows_display_requirement(display):
            report=options.execute(args,prepared,_final_report=partial)
    except BaseException as caught:
        error=caught
    if report is None:report=partial.get('value')
    if report is None:
        report=dict(ok=False,collected_only=True,collection_passed=False,entire_A7_passed=False)
    report['display_requirement']=display
    if error:
        report.update(ok=False,collection_passed=False,status='failed')
        report['entry_error']=dict(type=type(error).__name__,message=str(error))
    if 'value' in partial:
        options.finish_guards(args,prepared,report)
        try:
            report['finite_sfx_reenable']=verify_pair(prepared,report)
            report['ok']=report['finite_sfx_reenable']['ok'] is True
            report['collected_only']=not report['ok']
            report['status']='derived-finite-sfx-reenable-passed' if report['ok'] else 'failed-finite-sfx-reenable'
            report['sound_semantics_verified']=report['ok']
            report['sound_semantics_scope']='single declared sample8 reenable; no whole music/Load/A7 fidelity'
            report['audio_declaration']['minimum_sound_gate_passed']=report['ok']
        except BaseException as caught:
            report.update(ok=False,collected_only=True,status='failed-finite-sfx-reenable')
            report['finite_gate_error']=dict(type=type(caught).__name__,message=str(caught))
            if error is None:error=caught
        # Final status is published only after display-context release and all late guards.
        try:
            with (args.artifacts/'comparison.json').open('x',encoding='utf-8') as stream:
                json.dump(report,stream,ensure_ascii=False,indent=2)
        except OSError as caught:
            report.update(ok=False,status='failed-report-write')
            report['report_write_error']=dict(type=type(caught).__name__,message=str(caught))
    print(json.dumps(report,ensure_ascii=False))
    return 0 if report.get('ok') is True else 130 if isinstance(error,KeyboardInterrupt) else 1


if __name__=='__main__':raise SystemExit(main())


