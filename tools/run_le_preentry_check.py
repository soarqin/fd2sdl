"""F1：真实loader完成重定位/写启动栈前的全对象严格门。

原依据：FD2 bound DOS/4GW、LE entry/objects/fixup；本轮Domain证实loader
16bit四个WORD写3966/396a/397d/3981。selector/base全部从当次GDT/分配取，
不写guest寄存器/数据、不patch原EXE。内存取证独立于显示，不签WGC/音画。
"""
from __future__ import annotations
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import threading
import time

from fd2_dosbox_snapshot import OwnedRPC, parse_selector
from rebuild_fd2_analysis import parse_le, parse_objects, parse_fixups
from fd2_windows_job import owned_job_required
from run_original_comparison import running
import original_reference

ROOT=Path(__file__).resolve().parents[1]
REGS=('CS','DS','ES','SS','EIP','ESP','EAX','EBX','ECX','EDX','CR0','EFLAGS')
TRACE_REGS=REGS+('ESI','EDI','EBP')
FIRST_WRITE=bytes.fromhex('2689470c2689570e')

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def write(path,data):path.write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf8')

def check_sources(guards):
    """每项保留缺失/读取失败；晚守卫不得遮盖原业务错误。"""
    rows=[]
    for path,expected in guards.items():
        row={'path':path,'expected_sha256':expected,'ok':False}
        try:
            row['actual_sha256']=sha(path)
            row['ok']=row['actual_sha256']==expected
        except OSError as error:
            row['error']={'type':type(error).__name__,'message':str(error)}
        rows.append(row)
    return rows

class LoaderRPC(OwnedRPC):
    def __enter__(self):
        result=super().__enter__()
        self.record.update(debugger_break_used=True,console_creation_requested=True,
            all_business_fields_from_one_dump=False,scope='loader-only; not gameplay/time/RNG validation')
        return result
    def request(self,command):
        if getattr(self,'startup_trace',False) and command=='EXEC LOGL 2000':return self._exchange(command)
        if command in ('EXEC RUN','EXEC BPDEL *','EXEC GDT') or re.fullmatch(r'EXEC (BPINT 31 05|BP [0-9A-F]+:[0-9A-F]+)',command):
            return self._exchange(command)
        return super().request(command)
    def regs(self):
        names=TRACE_REGS if getattr(self,'startup_trace',False) else REGS
        lines=self.request('EXEC EV '+' '.join(names))
        rows=[line for line in lines if re.fullmatch(r'[0-9a-fA-F]+(?: [0-9a-fA-F]+){'+str(len(names)-1)+'}',line)]
        if len(rows)!=1:raise ValueError('loader寄存器行不完整')
        return dict(zip(names,(int(v,16) for v in rows[0].split())))

def descriptors(lines):
    text='\n'.join(lines);found=[]
    pattern=r'(?m)^([0-9A-F]{4}): b:([0-9A-F]{8}) type: ([0-9A-F]{2}) parbg\n\s+l:([0-9A-F]{8}) dpl : ([0-3])\s+([01]{5})$'
    for selector,base,kind,limit,dpl,flags in re.findall(pattern,text):
        found.append({'selector':int(selector,16),'base':int(base,16),'type':int(kind,16),
            'limit':int(limit,16),'dpl':int(dpl),'flags':flags})
    if not found:raise ValueError('本轮GDT未解析出descriptor')
    return found

def flat_selectors(table):
    def select(kind):
        rows=[r for r in table if r['base']==0 and r['limit']==0xffffffff and r['type'] in (kind,kind+1)
              and r['flags'][0]=='1' and r['flags'][-2]=='1']
        if len(rows)!=1:raise ValueError('本轮GDT没有唯一32bit flat代码/数据视图')
        return rows[0]['selector']
    return select(0x1a),select(0x12)

def allocation_bases(samples,objects,page_size):
    bases={}
    for obj in objects:
        size=(obj.vsize+page_size-1)//page_size*page_size
        rows=[r for r in samples if r['requested_size']==size]
        if len(rows)!=1:raise ValueError('LE object与当次成功DPMI分配没有唯一大小对应')
        bases[obj.index]=rows[0]['linear']
        if not 0<=bases[obj.index]<=16*1024*1024-obj.vsize:raise ValueError('DPMI对象超出本轮完整RAM读域')
    spans=sorted((bases[o.index],bases[o.index]+o.vsize) for o in objects)
    if any(a[1]>b[0] for a,b in zip(spans,spans[1:])):raise ValueError('DPMI对象分配互相重叠')
    return bases

def output_guard(path,original,dosbox):
    output=Path(path).resolve()
    for protected in (Path(original).resolve(),Path(dosbox).resolve().parent):
        if output==protected or output.is_relative_to(protected):raise ValueError('输出会写入原资源/工具安装目录')
    if output.exists() or not str(output).isascii() or any(c in str(output) for c in '\r\n'):
        raise ValueError('输出须为新的ASCII隔离目录')
    return output

class ForegroundAudit:
    def __init__(self,record):self.record=record;self.stop=threading.Event()
    def sample(self):
        user=ctypes.WinDLL('user32',use_last_error=True);user.GetForegroundWindow.restype=ctypes.c_void_p
        hwnd=user.GetForegroundWindow();pid=ctypes.c_uint32()
        user.GetWindowThreadProcessId(ctypes.c_void_p(hwnd),ctypes.byref(pid))
        return {'hwnd':int(hwnd or 0),'pid':int(pid.value)}
    def __enter__(self):
        self.before=self.sample();self.record.update(before=self.before,changes=[],interval_ms=10,
            sample_count=0,errors=[])
        def observe():
            last=self.before
            while not self.stop.wait(.01):
                try:value=self.sample()
                except BaseException as error:
                    self.record['errors'].append({'type':type(error).__name__,'message':str(error)})
                    self.stop.set();return
                self.record['sample_count']+=1
                if value!=last:self.record['changes'].append({'time':time.monotonic(),**value});last=value
        self.thread=threading.Thread(target=observe,daemon=True);self.thread.start();return self
    def __exit__(self,error_type,*error):
        self.stop.set();self.thread.join(timeout=1)
        self.record['thread_joined']=not self.thread.is_alive()
        try:self.record['after']=self.sample()
        except BaseException as failure:
            self.record['errors'].append({'type':type(failure).__name__,'message':str(failure)})
        self.record['sampling_complete']=self.record['thread_joined'] and not self.record['errors']
        # 已有业务错误继续传播；附带采样失败也不得遮盖它。
        if not self.record['sampling_complete'] and error_type is None:
            raise RuntimeError('前台采样不完整；不能签核owned前台观察')

def dump(rpc,directory,label,view):
    before=rpc.regs();desc=parse_selector(rpc.request(f'EXEC SELINFO {view:X}'))
    if not before['CR0']&1 or before['EFLAGS']&(1<<17):raise ValueError('dump要求protected非v86执行域')
    if desc['base']!=0 or desc['limit']!=0xffffffff or desc['flags'][-2]!='1':raise ValueError('dump不是新核32bit flat视图')
    path=directory/(label+'.bin')
    result=rpc.request(f'EXEC MEMDUMPBIN {view:X}:0 1000000 {path}')
    after=rpc.regs();end=parse_selector(rpc.request(f'EXEC SELINFO {view:X}'))
    if not after['CR0']&1 or after['EFLAGS']&(1<<17):raise ValueError('dump后已离开protected非v86执行域')
    if before!=after or desc!=end or path.stat().st_size!=16*1024*1024 or not any('Memory dump binary success.' in s for s in result):
        raise ValueError('暂停时dump视图/寄存器/长度改变')
    return {'path':str(path),'sha256':sha(path),'size':path.stat().st_size,'before':before,'after':after,'view':desc,'selector':view}

def validate(exe,snapshot,bases,out):
    command=[sys.executable,str(ROOT/'tools/verify_le_runtime_dump.py'),snapshot['path'],'--exe',str(exe)]
    for i in (1,2,3):command.extend([f'--object{i}-base',hex(bases[i])])
    result=subprocess.run(command,cwd=ROOT,capture_output=True,timeout=30)
    out.write_bytes(result.stdout+result.stderr)
    return {'command':command,'exit_code':result.returncode,'log':str(out),'passed':result.returncode==0}

def capture_startup_trace(rpc,report,out,appcode,entry,view):
    """同一owner内只执行一次有界LOGL；未知停止不RUN，不用旧21B决定通过。"""
    if not report['loaded_validation']['passed']:raise ValueError('启动trace要求当前L0严格通过')
    start=rpc.regs()
    if start!=report['loaded_snapshot']['before']:raise ValueError('trace起点已离开当前L0')
    trace={'schema':'fd2-le-startup-trace-v1','complete':False,'instruction_limit':8192,
        'command':'EXEC LOGL 2000','start_registers':start,
        'GDT':rpc.request('EXEC GDT'),
        'start_descriptors':{name:rpc.request(f'EXEC SELINFO {start[name]:X}') for name in ('CS','DS','ES','SS')},
        'command_source':'DOSBox-X debug.cpp Git blob 152ad8bf4fb792c1439917a7673de9d74e77f419; LOGL hex count; preinstruction logging'}
    report['startup_trace']=trace
    rpc.request('EXEC BPDEL *');rpc.request(f'EXEC BP {appcode:X}:{entry:X}')
    rpc.request('EXEC LOGL 2000')
    deadline=time.monotonic()+10;stable=None
    while time.monotonic()<deadline:
        point=rpc.regs()
        if (point['CS'],point['EIP'])==(appcode,entry):
            if point==stable:break
            stable=point
        else:
            stable=None
            if 'DEBUG: cpu log LOGCPU.TXT created' in (out/'native.log').read_text(encoding='utf8',errors='replace'):
                raise ValueError('8192条trace先耗尽；未知停止不继续RUN')
        time.sleep(.05)
    else:raise ValueError('有界trace未在真实LE entry稳定命中；不继续RUN')
    snapshot=dump(rpc,out,'application-entry',view)
    if snapshot['before']!=point:raise ValueError('trace命中后快照前寄存器改变')
    raw=(out/'LOGCPU.TXT').read_bytes();lines=raw.decode('ascii').splitlines()
    if not 1<len(lines)<=8192:raise ValueError('LOGL为空或超过硬上限')
    match=re.match(r'^\S+\s+([0-9A-F]{4}):([0-9A-F]{8})\s',lines[-1])
    if not match or (int(match[1],16),int(match[2],16))!=(point['CS'],point['EIP']):
        raise ValueError('trace末条不是暂停的LE entry前指令')
    values={k:int(v,16) for k,v in re.findall(r'\b(EAX|EBX|ECX|EDX|ESI|EDI|EBP|ESP|DS|ES|SS):([0-9A-F]+)',lines[-1])}
    if set(values)!={'EAX','EBX','ECX','EDX','ESI','EDI','EBP','ESP','DS','ES','SS'} or any(point[k]!=v for k,v in values.items()):
        raise ValueError('trace末条寄存器与实际暂停点不符')
    path=out/'entry-cpu.txt'
    with path.open('xb') as stream:stream.write(raw)
    trace.update(path=str(path),sha256=sha(path),size=len(raw),trace_instructions=len(lines),
        last_line_unexecuted=True,stop_registers=point,complete=True)
    report['entry_snapshot']=snapshot

def run(args):
    owned_job_required()
    original=args.original.resolve();out=output_guard(args.artifacts,original,args.dosbox);out.mkdir(parents=True)
    report={'schema':'fd2-strict-loaded-image-runtime-v2','ok':False,'processes':[],'rpc':{},'foreground':{},
            'visual_validation':False,'display_or_power_settings_changed':False,'guest_data_or_register_writes':False,
            'whole_F1_complete':False,'samples':[],'source_guards':{}}
    guards=report['source_guards']
    try:
        for p in original.rglob('*'):
            if p.is_file():guards[str(p.resolve())]=sha(p)
        guards[str(args.bootstrap_config.resolve())]=sha(args.bootstrap_config)
        for p in (args.dosbox,args.driver,Path(__file__),ROOT/'tools/fd2_dosbox_snapshot.py',ROOT/'tools/verify_le_runtime_dump.py',
                  ROOT/'tools/rebuild_fd2_analysis.py',ROOT/'tools/run_original_battle_animation.py',
                  ROOT/'tools/analyze_fd2_audio.py',
                  ROOT/'tools/run_original_comparison.py',ROOT/'tools/original_reference.py',ROOT/'docs/formats/original-reference.json'):
            guards[str(p.resolve())]=sha(p)
        executable=(original/'FD2.EXE').read_bytes();le=parse_le(executable);objects=parse_objects(executable,le)
        game=out/'game';shutil.copytree(original,game)
        for path in game.rglob('*'):
            if path.is_file() and path.name not in ('FD2.SAV','FD2.TMP'):guards[str(path)]=sha(path)
        with LoaderRPC(report['rpc']) as rpc:
            rpc.startup_trace=getattr(args,'startup_trace',False)
            config=out/'dosbox.conf'
            config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
              '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n'+f'mcp_server={rpc.port}\n'
              '[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n'
              '[log]\nlogfile='+str(out/'native.log')+'\n[debug]\ndebuggerrun=debugger\n'
              '[autoexec]\nmount c "'+str(game)+'"\nc:\nDEBUGBOX FD2.EXE\nexit\n',encoding='utf8')
            report['reference']=original_reference.build(original,args.dosbox,[config],['mixer nosound=true'],args.bootstrap_config)
            guards[str(config)]=sha(config)
            write(out/'reference.json',report['reference'])
            with ForegroundAudit(report['foreground']),running([str(args.dosbox.resolve()),'-conf',str(config)],out,os.environ.copy(),
                    out/'launcher.log',args.driver.resolve(),report['processes'],hidden_console=True) as process:
                rpc.bind(process);time.sleep(1);report['MZ_entry']=rpc.regs()
                rpc.request('EXEC BPINT 31 05');rpc.request('EXEC RUN')
                for index in range(7):
                    time.sleep(.15);before=rpc.regs()
                    if before['EAX']!=0x501:raise ValueError('本轮DPMI分配序列改变')
                    code=out/f'allocation-{index}.bin'
                    rpc.request(f'EXEC MEMDUMPBIN {before["CS"]:X}:{before["EIP"]:X} 2 {code}')
                    if code.read_bytes()!=b'\xcd\x31':raise ValueError('停止点不是已核INT31')
                    rpc.request(f'EXEC BP {before["CS"]:X}:{before["EIP"]+2:X}')
                    rpc.request('EXEC RUN');time.sleep(.1);after=rpc.regs()
                    if after['CS']!=before['CS'] or after['EIP']!=before['EIP']+2 or after['EFLAGS']&1:raise ValueError('DPMI返回错误/停止点不同')
                    report['samples'].append({'requested_size':(before['EBX']<<16)|before['ECX'],
                        'linear':(after['EBX']<<16)|after['ECX'],'before':before,'after':after})
                    rpc.request('EXEC BPDEL *')
                    if index!=6:rpc.request('EXEC BPINT 31 05');rpc.request('EXEC RUN')
                bases=allocation_bases(report['samples'],objects,le.page_size);report['object_bases']=bases
                report['GDT']=rpc.request('EXEC GDT');table=descriptors(report['GDT']);appcode,view=flat_selectors(table)
                candidates=[]
                for row in table:
                    if row['type'] not in (0x1a,0x1b) or row['flags'][-2]!='0' or row['flags'][0]!='1' or row['limit']<0x396e:continue
                    path=out/f'loader-{row["selector"]:04x}.bin'
                    rpc.request(f'EXEC MEMDUMPBIN {row["selector"]:X}:3966 8 {path}')
                    if path.read_bytes()==FIRST_WRITE:candidates.append(row)
                if len(candidates)!=1:raise ValueError('当次GDT没有唯一16bit loader首写点')
                loader=candidates[0];report['loader']=loader
                rpc.request(f'EXEC BP {loader["selector"]:X}:3966');rpc.request('EXEC RUN');time.sleep(.15)
                point=rpc.regs()
                if point['CS']!=loader['selector'] or point['EIP']!=0x3966:raise ValueError('未停首写前')
                bound=parse_selector(rpc.request(f'EXEC SELINFO {loader["selector"]:X}'))
                if any(bound[key]!=loader[key] for key in ('base','limit','dpl','flags')) or bound['type']&~1!=loader['type']&~1:
                    raise ValueError('loader停止点descriptor已改变')
                code=out/'loader-first-write-before.bin'
                rpc.request(f'EXEC MEMDUMPBIN {loader["selector"]:X}:3966 8 {code}')
                if code.read_bytes()!=FIRST_WRITE:raise ValueError('loader停止点原指令已改变')
                report['loader_at_stop']=bound
                report['loaded_snapshot']=dump(rpc,out,'loaded-before-stack',view)
                report['loaded_validation']=validate(original/'FD2.EXE',report['loaded_snapshot'],bases,out/'loaded-validation.log')
                entry=bases[le.entry_object]+le.entry_eip
                if rpc.startup_trace:
                    capture_startup_trace(rpc,report,out,appcode,entry,view)
                else:
                    rpc.request('EXEC BPDEL *')
                    rpc.request(f'EXEC BP {appcode:X}:{entry:X}');rpc.request('EXEC RUN');time.sleep(.15)
                    point=rpc.regs()
                    if point['CS']!=appcode or point['EIP']!=entry:raise ValueError('原LE entry停止点不同')
                    report['entry_snapshot']=dump(rpc,out,'application-entry',view)
                report['entry_static_validation']=validate(original/'FD2.EXE',report['entry_snapshot'],bases,out/'entry-validation.log')
                data=Path(report['entry_snapshot']['path']).read_bytes();bss=[]
                for obj in objects:
                    start=min(obj.vsize,obj.page_count*le.page_size)
                    for offset in range(start,obj.vsize):
                        value=data[bases[obj.index]+offset]
                        if value:bss.append({'object':obj.index,'offset':offset,'value':value})
                report['entry_nonzero_BSS']=bss
                rpc.request('EXEC BPDEL *');rpc.request('EXEC RUN')
        owned={r['pid'] for r in report['processes']}
        observations=[report['foreground']['before'],*report['foreground']['changes'],report['foreground']['after']]
        report['owned_DOSBox_foreground_not_observed']=all(c['pid'] not in owned for c in observations)
        report['foreground']['scope']='before/after plus 10ms samples; no sub-sample guarantee; console-host PID qualification separate'
        report['ok']=report['loaded_validation']['passed'] and report['owned_DOSBox_foreground_not_observed'] and all(r['cleaned'] and not r['forced_cleanup'] and r['exit_code']==0 for r in report['processes'])
    except BaseException as error:report['error']={'type':type(error).__name__,'message':str(error)}
    finally:
        report['source_guard_check']=check_sources(guards)
        report['sources_unchanged']=all(row['ok'] for row in report['source_guard_check'])
        report['ok'] &= report['sources_unchanged']
        write(out/'report.json',report)
    return report

def main():
    p=argparse.ArgumentParser(description=__doc__)
    for flag in ('original','dosbox','driver','bootstrap-config','artifacts'):p.add_argument('--'+flag,type=Path,required=True)
    p.add_argument('--startup-trace',action='store_true',help='L0到真实entry只做一次8192条有界LOGL；不改变静态零BSS判据')
    report=run(p.parse_args());print(json.dumps({k:report[k] for k in ('ok','whole_F1_complete','sources_unchanged')},ensure_ascii=False))
    return 0 if report['ok'] else 1

if __name__=='__main__':sys.exit(main())
