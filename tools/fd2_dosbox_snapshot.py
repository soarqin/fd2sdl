"""DOSBox-X2026.10.01 本轮owned只读RPC与单RAM快照解码；无BREAK/GUI输入。

官方同tag src/debug/debug_mcp.cpp：REQ/BEGIN/END、127.0.0.1反向连接；
ControlServer_Poll在主emulation线程同步ParseCommand。debug.cpp的EV/SELINFO/
MEMDUMPBIN不调用DEBUG_EnableDebugger/AllocConsole。本模块不开放写内存/寄存器命令。
FD2映射据26152/2b777/2968d、4e809/4e8bc，逐轮重算DS-view与object基址。
"""
from __future__ import annotations

import ctypes
from ctypes import wintypes as wt
import hashlib
import json
from pathlib import Path
import re
import socket
import struct
import time

import fd2_g3_prepare_oracle as oracle
from rebuild_fd2_analysis import parse_le,parse_objects,parse_fixups

RAM_SIZE=16*1024*1024
REGISTERS=('CS','DS','EIP','ESP','EBP','CR0','EFLAGS')
SOURCE_URL='https://github.com/joncampbell123/dosbox-x/blob/dosbox-x-v2026.10.01/src/debug/debug_mcp.cpp'


def sha(data):return hashlib.sha256(data).hexdigest()


def tcp_client_pid(peer_port,server_port):
    """GetExtendedTcpTable只读核反向连接属于本轮DOSBox PID；不接其它连接试命令。"""
    class Row(ctypes.Structure):
        _fields_=[(key,wt.DWORD) for key in ('state','local_address','local_port','remote_address','remote_port','pid')]
    library=ctypes.WinDLL('iphlpapi',use_last_error=True)
    call=library.GetExtendedTcpTable
    call.argtypes=[ctypes.c_void_p,ctypes.POINTER(wt.DWORD),wt.BOOL,wt.ULONG,ctypes.c_int,wt.ULONG]
    call.restype=wt.DWORD
    size=wt.DWORD(0);result=call(None,ctypes.byref(size),False,socket.AF_INET,5,0)
    if result!=122 or size.value<4 or size.value>16*1024*1024:raise OSError('RPC TCP owner表尺寸/错误码不同：'+str(result))
    buffer=ctypes.create_string_buffer(size.value);result=call(buffer,ctypes.byref(size),False,socket.AF_INET,5,0)
    if result:raise OSError('RPC TCP owner表查询失败：'+str(result))
    count=struct.unpack_from('<I',buffer.raw)[0]
    if 4+count*ctypes.sizeof(Row)>size.value:raise ValueError('RPC TCP owner表截断')
    pids=[];loopback=socket.inet_aton('127.0.0.1')
    for i in range(count):
        row=Row.from_buffer_copy(buffer,4+i*ctypes.sizeof(Row))
        if row.state==5 and socket.ntohs(row.local_port&65535)==peer_port and \
            socket.ntohs(row.remote_port&65535)==server_port and \
            struct.pack('<I',row.local_address)==loopback and struct.pack('<I',row.remote_address)==loopback:
            pids.append(int(row.pid))
    if len(pids)!=1:raise ValueError('RPC没有唯一已建立的loopback client owner：'+str(pids))
    return pids[0]


class OwnedRPC:
    def __init__(self,record,*,owner_query=tcp_client_pid):
        self.record=record;self.owner_query=owner_query;self.listener=None;self.connection=None
        self.process=None;self.stream=None;self.sequence=0;self.peer=None
    def __enter__(self):
        self.listener=socket.socket(socket.AF_INET,socket.SOCK_STREAM)
        self.listener.bind(('127.0.0.1',0));self.listener.listen(1);self.listener.settimeout(12)
        self.port=self.listener.getsockname()[1]
        self.record.update(host='127.0.0.1',port=self.port,requests=[],source=SOURCE_URL,
            debugger_break_used=False,frontend_keys_used=False,register_or_memory_writes_used=False,
            all_business_fields_from_one_dump=True,console_creation_requested=False,closed=False)
        return self
    def bind(self,process):
        self.process=process
        connection,peer=self.listener.accept();self.connection=connection;self.peer=peer
        connection.settimeout(20);self.stream=connection.makefile('rb')
        if peer[0]!='127.0.0.1' or self.owner_query(peer[1],self.port)!=process.pid or process.poll() is not None:
            raise ValueError('RPC client不是本轮存活owned DOSBox')
        self.record.update(pid=process.pid,peer=list(peer),tcp_owner_verified=True)
        if self.request('PING')!=['PONG']:raise ValueError('DOSBox RPC PING协议不同')
    def _owner(self):
        if self.process is None or self.process.poll() is not None or self.peer is None or \
            self.owner_query(self.peer[1],self.port)!=self.process.pid:
            raise ValueError('RPC本轮PID/连接归属或存活改变')
    def request(self,command):
        self._owner()
        if command!='PING' and not command.startswith(('EXEC EV ','EXEC SELINFO ','EXEC MEMDUMPBIN ')):
            raise ValueError('RPC只允许本工具EV/SELINFO/MEMDUMPBIN；禁止BREAK/写操作')
        if '\n' in command or '\r' in command or len(command)>2048:raise ValueError('RPC单行命令无效')
        self.sequence+=1;identity=str(self.sequence);entry={'id':identity,'command':command,'started':time.monotonic(),'ok':False}
        self.record['requests'].append(entry)
        self.connection.sendall(('REQ '+identity+' '+command+'\n').encode('ascii'))
        def line():
            raw=self.stream.readline(1024*1024+1)
            if not raw or len(raw)>1024*1024 or not raw.endswith(b'\n'):raise ValueError('RPC响应缺行尾/超长/提前关闭')
            return raw.decode('utf-8').rstrip('\r\n')
        begin=line()
        if begin not in ('BEGIN '+identity+' OK','BEGIN '+identity+' ERR'):raise ValueError('RPC响应id/BEGIN不同：'+begin)
        lines=[]
        for _ in range(16385):
            value=line()
            if value=='END '+identity:break
            if value.startswith(('BEGIN ','END ')):raise ValueError('RPC嵌套或结束id不符')
            lines.append(value)
        else:raise ValueError('RPC响应超过官方capture行数上限')
        entry.update(response=lines,ended=time.monotonic(),ok=begin.endswith(' OK'))
        self._owner()
        if not entry['ok']:raise ValueError('DOSBox RPC拒绝命令：'+str(lines))
        return lines
    def _snapshot_view_validate(self,registers,cs,ds):
        pass  # S2薄适配可收窄numeric前态；旧默认路径不调用。
    def _snapshot_registers_validate(self,registers):
        pass  # S2可仅据EV等待PM；默认路径不添加前置采样或改变命令。
    def snapshot(self,work,label,*,explicit_selectors=False):
        work=Path(work).resolve();path=work/(label+'-ram.bin')
        if not re.fullmatch(r'[A-Za-z0-9_-]{1,60}',label) or path.exists() or not str(path).isascii():
            raise ValueError('RPC snapshot需要新ASCII隔离路径与合法label')
        def registers():return parse_registers(self.request('EXEC EV '+' '.join(REGISTERS)))
        before=registers()
        if explicit_selectors:self._snapshot_registers_validate(before)
        cs=parse_selector(self.request('EXEC SELINFO '+(f'{before["CS"]:X}' if explicit_selectors else 'CS')))
        ds=parse_selector(self.request('EXEC SELINFO '+(f'{before["DS"]:X}' if explicit_selectors else 'DS')))
        if explicit_selectors:self._snapshot_view_validate(before,cs,ds)
        if not before['CR0']&1 or before['EFLAGS']&(1<<17) or not before['DS'] or ds['limit']<RAM_SIZE-1 or \
            len(ds['flags'])!=5 or ds['flags'][0]!='1' or ds['flags'][-2]!='1':
            raise ValueError('RPC尚未在已证32bit protected DS容量内；未猜默认selector/base')
        lines=self.request(f'EXEC MEMDUMPBIN {before["DS"]:X}:0 {RAM_SIZE:X} {path}')
        if not any('Memory dump binary success.' in line for line in lines):raise ValueError('RPC未报告真实binary dump成功')
        after=registers();ds_after=parse_selector(self.request('EXEC SELINFO '+f'{before["DS"]:X}'))
        if not after['CR0']&1 or after['EFLAGS']&(1<<17) or \
            any(ds_after[k]!=ds[k] for k in ('base','limit','type','dpl','flags')):
            raise ValueError('RPC前后protected descriptor改变；不拼接不同视图')
        data=path.read_bytes()
        if len(data)!=RAM_SIZE:raise ValueError('MEMDUMPBIN实际长度不同，未把全零填补当可读')
        result={'path':str(path),'sha256':sha(data),'size':len(data),'registers_before':before,'registers_after':after,
            'CS_descriptor_before':cs,'DS_descriptor_before':ds,'DS_descriptor_after':ds_after,
            'view_selector':before['DS'],'view_linear_base':ds['base'],
            'single_main_emulation_thread_command':True,'break_or_extra_window':False,
            'registers_are_separate_observations_not_atomic_guest_stack_trace':True,
            'UI_RNG_snapshot_not_compared':True}
        if explicit_selectors:result['descriptor_selectors_bound_to_before_EV']=True
        self.record.setdefault('snapshots',[]).append(result);return data,result
    def __exit__(self,*exception):
        if self.stream:self.stream.close()
        if self.connection:self.connection.close()
        if self.listener:self.listener.close()
        self.record['closed']=True


def parse_registers(lines):
    rows=[line.strip() for line in lines if re.fullmatch(r'[0-9a-fA-F]{1,8}(?: [0-9a-fA-F]{1,8}){6}',line.strip())]
    if len(rows)!=1:raise ValueError('RPC EV需要唯一七寄存器数值行')
    return dict(zip(REGISTERS,(int(value,16) for value in rows[0].split())))


def parse_selector(lines):
    text='\n'.join(lines);bases=re.findall(r'\bb:([0-9a-fA-F]{8})\b',text);limits=re.findall(r'\bl:([0-9a-fA-F]{8})\b',text)
    types=re.findall(r'\btype:([0-9a-fA-F]{2})\b',text);flags=re.findall(r'\bdpl\s*:\s*([0-3])\s+([0-9a-fA-F]{3,6})\b',text)
    if len(bases)!=1 or len(limits)!=1 or len(types)!=1 or len(flags)!=1:raise ValueError('RPC SELINFO没有唯一descriptor base/limit/type/flags')
    return {'base':int(bases[0],16),'limit':int(limits[0],16),'type':int(types[0],16),
        'dpl':int(flags[0][0]),'flags':flags[0][1],'raw':lines}


def _anchors(executable,fixups):
    result=[]
    for va in (0x26152+10,0x2b777+10):
        data=oracle.source_range(executable,va,96);mask=bytearray([1]*len(data))
        for f in fixups:
            for at in range(f.source_addr_relbase,f.source_addr_relbase+4):
                if va<=at<va+len(data):mask[at-va]=0
        spans=[];start=0
        for i in range(len(mask)+1):
            if i==len(mask) or not mask[i]:
                if i>start:spans.append((start,i-start))
                start=i+1
        start,length=max(spans,key=lambda pair:pair[1])
        if length<8 or sum(mask)<32:raise ValueError('FD2 anchor没有足够不可重定位已知码字节')
        result.append({'cva':va,'data':data,'mask':bytes(mask),'needle_start':start,'needle':data[start:start+length]})
    return result


def locate(memory,anchor):
    found=[];at=0
    while True:
        at=memory.find(anchor['needle'],at)
        if at<0:break
        base=at-anchor['needle_start']
        if base>=0 and base+len(anchor['data'])<=len(memory) and all(not flag or memory[base+i]==value
            for i,(value,flag) in enumerate(zip(anchor['data'],anchor['mask']))):found.append(base)
        at+=1
    if len(found)!=1:raise ValueError('原码anchor在本轮snapshot没有唯一match：'+hex(anchor['cva'])+' '+str(found))
    return found[0]


def resolve(memory,original,descriptor):
    """不搜索roster：两原码锚→合法fixup目标→53bf7指针→完整0xa00。"""
    executable=(Path(original)/'FD2.EXE').read_bytes()
    if sha(executable)!=oracle.EXE_SHA256 or len(memory)!=RAM_SIZE:raise ValueError('本轮EXE或完整RAM身份不同')
    le=parse_le(executable);objects=parse_objects(executable,le);fixups=parse_fixups(executable,le,objects)
    anchors=_anchors(executable,fixups);matches=[locate(memory,a) for a in anchors]
    bases=[at-(a['cva']-0x10000) for at,a in zip(matches,anchors)]
    if len(set(bases))!=1 or bases[0]<0:raise ValueError('两个非相邻原码锚推导不同object1 DS-view基址')
    code=bases[0]
    def u32(at):
        if at<0 or at+4>len(memory):raise ValueError('snapshot DWORD越界')
        return struct.unpack_from('<I',memory,at)[0]
    proofs=[]
    def object_base(target,allowed):
        rows=[]
        for f in fixups:
            if 0x10000<=f.source_addr_relbase<0x4ef29 and f.target_object==target and any(start<=f.source_addr_relbase<end for start,end in allowed):
                value=u32(code+f.source_addr_relbase-0x10000)
                rows.append({'source_cva':f.source_addr_relbase,'target_offset':f.target_offset,
                    'encoded_DS_offset':value,'base_DS_offset':value-f.target_offset})
        if len(rows)<2 or len({row['base_DS_offset'] for row in rows})!=1:raise ValueError('原合法fixup没有独立一致data object基址')
        proofs.extend(rows);return rows[0]['base_DS_offset']
    data=object_base(2,((0x26152,0x265ec),(0x2b777,0x2b843),(0x2968d,0x2986f)))
    data3=object_base(3,((0x4e809,0x4e821),(0x4e8bc,0x4e8d7)))
    t,meta=oracle.tables(Path(original))
    tables={'items':(data3,0x2ad),'profiles':(data3,0x188a),'growth':(data3,0x20a1),
        'promotion':(data3,0x15fe),'special':(data,0x23d5),'fees':(data,0x2397),
        'notown':(data,0x23e7),'stage_rows':(data3,0x238d)}
    for key,(base,offset) in tables.items():
        if base<0 or memory[base+offset:base+offset+len(t[key])]!=t[key]:raise ValueError('本轮data映射与完整原表不同：'+key)
    pointer=u32(data+0x3bf7);count=u32(data+0x3bfb);stage=u32(data+0x3c03);money=u32(data+0x3bf3)
    if not 1<=count<=32 or not 1<=stage<=29 or not pointer or pointer+2560>len(memory):
        raise ValueError('本轮canonical owner/人数/stage/堆指针不在有证域')
    options=bytes(memory[data+off] for off in (0x1aab,0x3af9,0x1e61,0x1e62))
    state={'stage':stage,'count':count,'resource':money,'options':options,'roster':memory[pointer:pointer+2560]}
    receipt={'code_anchors':[{'cva':hex(a['cva']),'source_sha256':sha(a['data']),'mask_sha256':sha(a['mask']),
        'snapshot_DS_offset':at,'known_bytes':sum(a['mask'])} for a,at in zip(anchors,matches)],
        'object_DS_offsets':{'1':code,'2':data,'3':data3},
        'object_linear_bases':{str(i):descriptor['base']+base for i,base in ((1,code),(2,data),(3,data3))},
        'relocation_proofs':proofs,'full_original_tables':meta['tables'],'canonical_owner':'0x53bf7',
        'roster_pointer_DS_offset':pointer,'roster_pointer_linear':descriptor['base']+pointer,
        'roster_bytes':2560,'roster_sha256':sha(state['roster']),'fields_source_cva':'0x2968d',
        'roster_content_scan_used':False,'single_snapshot_fields':True,'all_dump_zero_fill_is_not_auto_pass':True}
    return state,receipt


def check_state(actual,expected):
    differences={key:{'actual':actual[key].hex() if isinstance(actual[key],bytes) else actual[key],
        'expected':expected[key].hex() if isinstance(expected[key],bytes) else expected[key]}
        for key in ('stage','count','resource','options') if actual[key]!=expected[key]}
    bad=[i for i,(a,b) in enumerate(zip(actual['roster'],expected['roster'])) if a!=b]
    return {'ok':not differences and not bad,'scalar_differences':differences,'roster_mismatch_count':len(bad),
        'roster_mismatch_offsets':bad[:32],'full_roster_bytes':2560,'unknown_tail_checked':True,
        'native_manual_Save_claimed':False}
