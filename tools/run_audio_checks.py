#!/usr/bin/env python3
"""A7功能回归：真实原版→SDL，资源/Save/音频封包与有限SFX身份；不测BGM音色。"""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import sys

import fd2_audio_checks as checks


def parser():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--root',type=Path,default=Path(__file__).resolve().parents[1])
    for name in ('game','driver','dosbox','artifacts'):
        p.add_argument('--'+name,type=Path,required=True)
    p.add_argument('--scenarios',nargs='+',choices=checks.SCENARIOS,default=list(checks.SCENARIOS))
    p.add_argument('--execute',action='store_true',help='显式启动；省略时只做资源/路径前置，不调用显示/设备API或创建产物')
    return p


def publish(path,report):
    text=json.dumps(report,ensure_ascii=False,indent=2,allow_nan=False)+'\n'
    temporary=path.with_name(path.name+'.tmp')
    created=False
    try:
        with temporary.open('x',encoding='utf-8',newline='\n') as stream:
            created=True;stream.write(text);stream.flush();os.fsync(stream.fileno())
        os.link(temporary,path)  # 同目录原子发布且不覆盖旧报告。
    finally:
        if created:temporary.unlink()


def main(argv=None):
    args=parser().parse_args(argv)
    report=dict(schema='fd2-a7-functional-audio-checks-v1',ok=False,status='failed',
        scope=checks.declaration(args.scenarios),cases=[],processes=[],guards={},
        functional_checks_passed=False,BGM_timbre_waveform_absolute_timing='not_in_scope',
        device_output_validated=False,A7_fidelity_validated=False)
    prepared=None;interrupted=None
    try:
        prepared=checks.prepare(args)
        if not args.execute:
            print(json.dumps(dict(schema=report['schema'],status='ready_not_executed',ok=False,
                dry_run=True,scope=report['scope'],scenarios=list(args.scenarios),
                resources=prepared['original_facts'],binaries=prepared['inputs'],
                source_file_count=len(prepared['source']),artifacts=str(args.artifacts),
                GUI_or_device_or_display_API_called=False),ensure_ascii=False,indent=2))
            return 0
        if sys.platform!='win32':raise RuntimeError('实际入口只支持已验证的Windows HWND WGC/后台输入路径')
        with prepared['tools']['fd2_display_requirement'].windows_display_requirement(
                report.setdefault('display_requirement',{})):
            checks.execute(args,prepared,report)
    except BaseException as error:
        report.setdefault('error',dict(type=type(error).__name__,message=str(error)))
        if isinstance(error,(KeyboardInterrupt,SystemExit)):interrupted=error
    finally:
        if args.execute and prepared is not None:
            checks.finalize_guards(args,prepared,report)
            report['ok']=checks.qualify_report(report,args.scenarios)
            report['functional_checks_passed']=report['ok']
            report['status']='passed' if report['ok'] else 'failed'
            # 最终发布在全部owned清理、来源检查及display同线程release之后。
            if report.get('artifacts_created'):
                try:publish(args.artifacts/'comparison.json',report)
                except BaseException as error:
                    report['ok']=False;report['status']='failed';report['functional_checks_passed']=False
                    report['publication_error']=dict(type=type(error).__name__,message=str(error))
                    if isinstance(error,(KeyboardInterrupt,SystemExit)):interrupted=error
    print(json.dumps(dict(ok=report['ok'],status=report['status'],scope=report['scope'],
        report=str(args.artifacts/'comparison.json') if report.get('artifacts_created') else None,
        error=report.get('error'),publication_error=report.get('publication_error')),ensure_ascii=False))
    if interrupted is not None:raise interrupted
    return 0 if report['ok'] else 1


if __name__=='__main__':
    raise SystemExit(main())
