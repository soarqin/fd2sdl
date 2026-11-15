"""L0真实loader两段/六窗的只读IDA Domain导出；不发布原loader字节。

在ida-domain环境内调用export(capture, artifacts)。按项目惯例，经技能run.py的
新临时wrapper调用；数据库仅在新artifacts内建立，save_on_close=False。
输出含原始代码与raw_hex，全部保留于ignored build，不提交或分发。
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import sys

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
from original_reference import parse_json
from verify_le_startup_writes import WINDOWS, digest, late_guard, modules, new_output, parse_log, require, snapshot

def export(capture,artifacts):
    # 延迟导入，纯输入/路径回归无需安装或启动IDA。
    from ida_domain import Database
    from ida_domain.database import IdaCommandOptions
    from ida_domain.segments import AddressingMode
    inputs={};raw=snapshot(capture,inputs);report=parse_json(raw.decode('utf8'));out=new_output(artifacts,report)
    require(report.get('ok') is True and report.get('sources_unchanged') is True,'原生收据未合格')
    for path in (Path(__file__),ROOT/'tools/verify_le_startup_writes.py',ROOT/'tools/original_reference.py',ROOT/'tools/analyze_fd2_audio.py',ROOT/'tools/rebuild_fd2_analysis.py'):
        snapshot(path,inputs)
    trace=report['startup_trace'];require(trace['schema']=='fd2-le-startup-trace-v1' and trace['complete'] is True,'没有合格新trace')
    memory=snapshot(report['loaded_snapshot']['path'],inputs)
    require(digest(memory)==report['loaded_snapshot']['sha256'],'L0摘要不符')
    log=snapshot(trace['path'],inputs);require(digest(log)==trace['sha256'],'LOGL摘要不符')
    rows=parse_log(log);role_modules,gdt=modules(memory,rows,trace);windows=[];module_inputs=[]
    for role,module in role_modules.items():
        code=memory[module['base']:module['base']+module['limit']+1];path=out/(role+'-runtime-code.bin')
        with path.open('xb') as stream:stream.write(code)
        snapshot(path,inputs)
        options=IdaCommandOptions(auto_analysis=False,new_database=True,processor='metapc',file_type='binary',
            output_database=str(path.with_suffix('.i64')),plugin_options='lumina:host=0.0.0.0 -Osecondary_lumina:host=0.0.0.0')
        with Database.open(str(path),options,save_on_close=False) as db:
            for segment in db.segments:
                require(db.segments.set_addressing_mode(segment,AddressingMode.BIT16),'Domain无法固定16bit段')
            for start,end,wanted in WINDOWS[role]:
                instructions=[];at=start
                while at<end:
                    insn=db.instructions.get_at(at)
                    require(insn is not None and insn.size>0 and at+insn.size<=end,'Domain完整窗口边界失败：'+hex(at))
                    db.instructions.create(at);insn_raw=bytes(db.bytes.get_bytes_at(at,size=insn.size))
                    require(insn_raw==code[at:at+insn.size],'Domain与原snapshot字节不同')
                    instructions.append({'cs_offset':at,'size':insn.size,'raw_hex':insn_raw.hex(),'asm':db.instructions.get_disassembly(insn)})
                    at+=insn.size
                windows.append({'role':role,'selector':module['selector'],'base_from_L0_RAM_GDT':module['base'],
                    'bitness':16,'window':[start,end],'sha256':wanted,'instructions':instructions})
        module_inputs.append({'role':role,**module,'path':str(path),'size':len(code),'sha256':digest(code)})
    late_guard(inputs)
    result={'schema':'fd2-le-startup-Domain-v1','capture_sha256':digest(raw),'database_saved':False,
        'coordinate_view':'runtime protected loader selector:offset; not FD2 canonical VA',
        'input_modules':module_inputs,'GDT':gdt,'windows':windows,'inputs':inputs,'sources_unchanged':True}
    with (out/'domain.json').open('x',encoding='utf8') as stream:json.dump(result,stream,ensure_ascii=False,indent=2);stream.write('\n')
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--capture',type=Path,required=True);parser.add_argument('--artifacts',type=Path,required=True)
    args=parser.parse_args();report=export(args.capture,args.artifacts)
    print(json.dumps({'windows':len(report['windows']),'database_saved':report['database_saved']}))

if __name__=='__main__':main()
