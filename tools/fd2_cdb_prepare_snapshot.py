"""既有CDB/同App PDB的owned只读prepare owner快照；不扫描roster或改产品。

命令由root提供的WinDbg协议：~*kn→选main线程/帧→dv/??求真实类型布局；
原native05的.writemem创建文件失败，改以owned Popen既有handle单次RPM再由Python写隔离文件。
全部线程在同一次attach内保持停止，finally .detach;q。仅本轮PID，隐藏CLI。
SYMSRV_INDEX_INFOW布局依据本机Windows SDK10.0.26100.0 um/DbgHelp.h。
"""
from __future__ import annotations

import ctypes
from ctypes import wintypes as wt
import hashlib
import os
from pathlib import Path
import queue
import re
import struct
import subprocess
import threading
import time
import uuid


def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def normalize_output_line(line):
    """CDB把无换行prompt与下一输出接在同一行；仅去掉严格行首prompt。

    不移除命令回显：'.echo nonce'仍是命令，不是执行后的'nonce'。
    原始bytes与带prompt文本分别保留，只有解析events使用此视图。
    """
    return re.sub(r'^[ \t]*(?:[0-9]+:[0-9a-fA-F]+(?::(?:x86|x64|arm64|arm64ec))?>|>)[ \t]*','',line,count=1)


def file_index(path,dbghelp):
    class Info(ctypes.Structure):
        _fields_=[('sizeofstruct',wt.DWORD),('file',wt.WCHAR*261),('stripped',wt.BOOL),
            ('timestamp',wt.DWORD),('size',wt.DWORD),('dbgfile',wt.WCHAR*261),('pdbfile',wt.WCHAR*261),
            ('guid',ctypes.c_ubyte*16),('sig',wt.DWORD),('age',wt.DWORD)]
    library=ctypes.WinDLL(str(Path(dbghelp).resolve()),use_last_error=True,winmode=0x1100)
    call=library.SymSrvGetFileIndexInfoW;call.argtypes=[wt.LPCWSTR,ctypes.POINTER(Info),wt.DWORD];call.restype=wt.BOOL
    info=Info();info.sizeofstruct=ctypes.sizeof(info)
    if not call(str(Path(path).resolve()),ctypes.byref(info),0):
        raise OSError('DbgHelp读取本地PE/PDB索引失败，Win32='+str(ctypes.get_last_error()))
    return {'guid':str(uuid.UUID(bytes_le=bytes(info.guid))),'age':int(info.age),'timestamp':int(info.timestamp),
        'pdbfile':str(info.pdbfile),'file':str(info.file),'size':int(info.size)}


def symbols(exe,pdb,dbghelp,*,reader=file_index):
    first=reader(exe,dbghelp);second=reader(pdb,dbghelp)
    if first['guid']!=second['guid'] or first['age']!=second['age'] or \
        first['guid']=='00000000-0000-0000-0000-000000000000' or first['age']<1:
        raise ValueError('CDB App PE CodeView与PDB GUID/age不同，禁止猜栈偏移')
    return {'exe':str(Path(exe).resolve()),'exe_sha256':sha(exe),'pdb':str(Path(pdb).resolve()),
        'pdb_sha256':sha(pdb),'dbghelp':str(Path(dbghelp).resolve()),'dbghelp_sha256':sha(dbghelp),
        'PE_index':first,'PDB_index':second,'GUID_age_match':True}


def main_frame(lines,pid,module='fd2sdl'):
    current=None;found=[]
    for line in lines:
        banner=re.search(r'^\s*[.#]?\s*([0-9]+)\s+Id:\s*([0-9a-f]+)\.([0-9a-f]+)\b',line,re.I)
        if banner:
            if int(banner[2],16)!=pid:raise ValueError('CDB stack线程不属于本轮目标PID')
            current=int(banner[3],16)
        frame=re.search(r'^\s*([0-9a-f]+)\s+[0-9a-f`]+\s+[0-9a-f`]+\s+'+re.escape(module)+r'!main(?:\+[^\s]+)?(?:\s|$)',line,re.I)
        if frame:
            if current is None:raise ValueError('CDB main frame缺少线程owner')
            found.append({'thread_id':current,'frame':int(frame[1],16),'line':line})
    if len(found)!=1:raise ValueError('CDB需要唯一main frame，未按相似roster/固定RSP猜测：'+str(found))
    return found[0]


def cpp_integer(lines):
    values=[]
    for line in lines:
        # 使用PDB本来就能求值的指针/sizeof/ptrdiff，不再引入CDB不识别的cast。
        # 只接受完整typed结果；命令回显、诊断地址和负ptrdiff都不是有效布局。
        integer=r'(?:unsigned )?(?:(?:__)?int64|int|long)'
        pointer=r'(?:(?:struct|class) [A-Za-z_][A-Za-z0-9_:!]*|void|(?:unsigned )?char)\s*\*'
        match=re.fullmatch(r'\s*(?:'+integer+'|'+pointer+r')\s+(0x[0-9a-f`]+|0n[0-9]+|[0-9]+)\s*',line,re.I)
        if match:
            value=match[1].lower().replace('`','')
            values.append(int(value[2:],16) if value.startswith('0x') else int(value[2:]) if value.startswith('0n') else int(value))
    if len(values)!=1:raise ValueError('CDB C++表达式没有唯一完整整数结果：'+str(lines))
    return values[0]


def _memory_api():
    api=ctypes.WinDLL('kernel32',use_last_error=True)
    api.GetProcessId.argtypes=[wt.HANDLE];api.GetProcessId.restype=wt.DWORD
    api.GetExitCodeProcess.argtypes=[wt.HANDLE,ctypes.POINTER(wt.DWORD)];api.GetExitCodeProcess.restype=wt.BOOL
    api.QueryFullProcessImageNameW.argtypes=[wt.HANDLE,wt.DWORD,wt.LPWSTR,ctypes.POINTER(wt.DWORD)]
    api.QueryFullProcessImageNameW.restype=wt.BOOL
    api.ReadProcessMemory.argtypes=[wt.HANDLE,wt.LPCVOID,wt.LPVOID,ctypes.c_size_t,ctypes.POINTER(ctypes.c_size_t)]
    api.ReadProcessMemory.restype=wt.BOOL
    return api


def read_owned_memory(target,address,size,exe,*,api=None):
    """沿本轮Popen既有handle只读一个有界区间；不按PID重开、不scan、不接受短读。

    本叶不声称已停线程；take()须在同一次CDB停止态attach内调用它。
    api仅供纯回归注入，不改变实际Win32调用及handle owner验证。
    """
    if type(address) is not int or type(size) is not int or address<=0 or not 0<size<=1024*1024 or \
        address+size>1<<(8*ctypes.sizeof(ctypes.c_void_p)):
        raise ValueError('owned内存读取只接受非零指针及至多1MiB完整区间')
    handle=getattr(target,'_handle',None)
    if not isinstance(handle,int) or isinstance(handle,bool) or handle<=0 or type(target.pid) is not int or target.pid<=0:
        raise ValueError('owned内存读取缺少本轮Popen既有handle/PID，禁止OpenProcess兜底')
    expected=Path(exe).resolve(strict=True);api=api if api is not None else _memory_api()
    def failure(name):raise OSError(name+'失败，Win32='+str(getattr(ctypes,'get_last_error',lambda:0)()))
    def owner():
        if target.poll() is not None or int(api.GetProcessId(handle))!=target.pid:
            raise ValueError('owned内存读取handle的本轮PID或存活改变')
        exit_code=wt.DWORD()
        if not api.GetExitCodeProcess(handle,ctypes.byref(exit_code)):failure('GetExitCodeProcess')
        if exit_code.value!=259:raise ValueError('owned内存读取目标已经退出')
        buffer=ctypes.create_unicode_buffer(32768);length=wt.DWORD(len(buffer))
        if not api.QueryFullProcessImageNameW(handle,0,buffer,ctypes.byref(length)):failure('QueryFullProcessImageNameW')
        if not 0<length.value<len(buffer) or len(buffer.value)!=length.value or \
            os.path.normcase(str(Path(buffer.value).resolve()))!=os.path.normcase(str(expected)):
            raise ValueError('owned内存读取handle实际映像不是本轮App')
        return {'pid':target.pid,'image':buffer.value,'exit_code':exit_code.value}
    before=owner();buffer=ctypes.create_string_buffer(size);read=ctypes.c_size_t()
    if not api.ReadProcessMemory(handle,ctypes.c_void_p(address),buffer,size,ctypes.byref(read)):
        failure('ReadProcessMemory')
    if read.value!=size:raise ValueError('ReadProcessMemory没有完整返回请求字节，禁止短读/补零')
    after=owner();data=buffer.raw
    return data,{'method':'ReadProcessMemory','address':address,'size':size,'sha256':hashlib.sha256(data).hexdigest(),
        'existing_Popen_handle_used':True,'OpenProcess_used':False,'single_ReadProcessMemory':True,
        'all_requested_bytes_read':True,'owner_before':before,'owner_after':after,'target_memory_written':False}


def decode(data,layout):
    fields={'stage_id':1,'unit_count':1,'gold_or_flags':4,'options':4,'unit_area':2560}
    if len(data)!=layout['size'] or not 2560<=len(data)<=8192:raise ValueError('CDB单次prepare结构快照长度不同')
    spans=[]
    for key,size in fields.items():
        at=layout['offsets'][key]
        if at<0 or at+size>len(data):raise ValueError('PDB字段偏移出单快照范围：'+key)
        spans.append((at,at+size))
    if any(a<d and c<b for i,(a,b) in enumerate(spans) for c,d in spans[i+1:]):raise ValueError('PDB字段区间重叠')
    offsets=layout['offsets']
    state={'stage':data[offsets['stage_id']],'count':data[offsets['unit_count']],
        'resource':struct.unpack_from('<I',data,offsets['gold_or_flags'])[0],
        'options':data[offsets['options']:offsets['options']+4],
        'roster':data[offsets['unit_area']:offsets['unit_area']+2560]}
    if not 1<=state['stage']<=29 or not 1<=state['count']<=32:raise ValueError('CDB prepare字段不在有证域')
    return state


class CDB:
    def __init__(self,cdb,target,work,record):
        self.cdb=Path(cdb).resolve();self.target=target;self.work=Path(work).resolve();self.record=record
        self.process=None;self.events=queue.Queue();self.sequence=0;self.nonce=uuid.uuid4().hex
        self.output=[];self.raw_output=bytearray();self.worker=None
    def __enter__(self):
        if self.target.poll() is not None:raise ValueError('CDB目标在attach前已退出')
        environment=os.environ.copy()
        for key in ('_NT_SYMBOL_PATH','_NT_ALT_SYMBOL_PATH','_NT_EXECUTABLE_IMAGE_PATH'):environment.pop(key,None)
        command=[str(self.cdb),'-p',str(self.target.pid),'-c','.echo FD2_READY_'+self.nonce]
        self.record.update(command=command,cdb_sha256=sha(self.cdb),target_pid=self.target.pid,
            requests=[],detached=False,closed=False,hidden_cli=True,roster_scan_used=False)
        self.process=subprocess.Popen(command,cwd=self.work,env=environment,stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,stderr=subprocess.STDOUT,creationflags=subprocess.CREATE_NO_WINDOW)
        self.record['cdb_pid']=self.process.pid
        self.worker=threading.Thread(target=self._read_stdout,daemon=True);self.worker.start()
        try:self._until('FD2_READY_'+self.nonce)
        except BaseException:
            self.__exit__(None,None,None);raise
        return self
    def _read_stdout(self):
        try:
            while True:
                line=self.process.stdout.readline()
                if not line:break
                self.raw_output.extend(line)
                text=line.decode('utf-8',errors='backslashreplace').rstrip('\r\n')
                self.output.append(text);self.events.put(normalize_output_line(text))
        finally:self.events.put(None)
    def _until(self,marker,timeout=20):
        deadline=time.monotonic()+timeout;lines=[]
        while time.monotonic()<deadline:
            try:line=self.events.get(timeout=max(.001,deadline-time.monotonic()))
            except queue.Empty:break
            if line is None:raise RuntimeError('CDB在标记前退出')
            if line.strip()==marker:return lines
            lines.append(line)
            if sum(map(len,lines))>2*1024*1024:raise ValueError('CDB响应超过本次范围')
        raise TimeoutError('CDB命令标记超时，随后仍尝试.detach;q')
    def request(self,command):
        if self.target.poll() is not None or self.process.poll() is not None:raise ValueError('CDB/本轮目标已退出')
        if '\r' in command or '\n' in command:raise ValueError('CDB只接收本工具单行命令')
        self.sequence+=1;marker='FD2_'+self.nonce+'_'+str(self.sequence)
        entry={'command':command,'ok':False};self.record['requests'].append(entry)
        # .sympath把分号当多路径分隔符；marker必须单独输入行，不能拼到其参数。
        self.process.stdin.write((command+'\n.echo '+marker+'\n').encode('utf-8'));self.process.stdin.flush()
        lines=self._until(marker);entry.update(response=lines,ok=True);return lines
    def __exit__(self,*exception):
        error=None
        try:
            if self.process and self.process.poll() is None:
                self.process.stdin.write(b'.detach;q\n');self.process.stdin.flush()
                try:self.process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    # 不直接杀仍附加的debugger；重送本次detach并保留明确失败。
                    self.process.stdin.write(b'.detach;q\n');self.process.stdin.flush()
                    self.process.wait(timeout=5)
            if self.worker:self.worker.join(timeout=2)
            self.record['detached']=bool(self.process and self.process.poll()==0 and self.target.poll() is None)
            if not self.record['detached']:error=RuntimeError('CDB未证明正常detach或目标存活')
        finally:
            (self.work/'cdb-transcript.txt').write_text('\n'.join(self.output)+'\n',encoding='utf-8')
            (self.work/'cdb.stdout.bin').write_bytes(self.raw_output)
            self.record['transcript_sha256']=sha(self.work/'cdb-transcript.txt')
            self.record['raw_stdout_sha256']=sha(self.work/'cdb.stdout.bin')
            self.record['closed']=bool(self.process and self.process.poll() is not None)
            if self.process:
                for stream in (self.process.stdin,self.process.stdout):
                    if stream:stream.close()
        if error:raise error


def take(cdb,target,exe,pdb,work,record):
    work=Path(work).resolve();dbghelp=Path(cdb).resolve().parent/'dbghelp.dll'
    record['symbols']=symbols(exe,pdb,dbghelp)
    with CDB(cdb,target,work,record) as debugger:
        debugger.request('.sympath '+str(Path(pdb).resolve().parent))
        debugger.request('.reload /f '+Path(exe).name)
        trace=debugger.request('~*kn 0n128');frame=main_frame(trace,target.pid,Path(exe).stem);record['main_frame']=frame
        debugger.request(f'~~[{frame["thread_id"]:x}]s')
        debugger.request(f'.frame /r {frame["frame"]:x}')
        local=debugger.request('dv /t /v app_context')
        if not any('app_context' in line and 'fd2_field_play_context' in line for line in local):
            raise ValueError('CDB未按本轮main frame解析app_context正式类型')
        def value(expression):return cpp_integer(debugger.request('?? '+expression))
        address=value('&app_context.preparation');size=value('sizeof(app_context.preparation)')
        if not address or not 2560<=size<=8192:raise ValueError('PDB preparation地址/sizeof无效')
        offsets={field:value('(char *)&app_context.preparation.'+field+' - (char *)&app_context.preparation')
            for field in ('stage_id','unit_count','gold_or_flags','options','unit_area')}
        layout={'address':address,'size':size,'offsets':offsets,'from_PDB_symbol_expressions':True}
        path=work/'sdl-prepare-owner.bin'
        if path.exists() or any(ch in str(path) for ch in ('"','\r','\n')):raise ValueError('CDB需要新的隔离输出')
        data,memory_read=read_owned_memory(target,address,size,exe)
        path.write_bytes(data);state=decode(data,layout)
        record.update(layout=layout,snapshot={'path':str(path),'size':len(data),'sha256':hashlib.sha256(data).hexdigest()},
            memory_read=memory_read,all_fields_from_one_ReadProcessMemory=True,
            all_reads_in_one_stopped_attach=True,no_new_product_dump_hook=True)
    if sha(exe)!=record['symbols']['exe_sha256'] or sha(pdb)!=record['symbols']['pdb_sha256']:
        raise ValueError('CDB读取期间App/PDB身份变化')
    return state,record
