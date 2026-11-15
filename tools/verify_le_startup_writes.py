"""固定FD2/DOS4GW的L1分层字节门：L0全对象＋有源启动写账，零mask。

来源：Domain六窗及真实LOGL；地址均为当次runtime selector:offset，非canonical。
不解释任意x86，不把L1字节作expected，不改变verify_le_runtime_dump的零BSS门。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct

from original_reference import parse_json, read_input
from rebuild_fd2_analysis import build_relbase_image, parse_le, parse_objects, parse_fixups

ROOT=Path(__file__).resolve().parents[1]
EXE_SHA256='bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd'
# 只发布窗口身份，不发布原loader代码；指令仍由当次L0与Domain独立核对。
WINDOWS={
    'loader':((0x3940,0x3a6c,'f91758fc6620f0ee1aecf04ede1814512d3332c956d71d890e7e8acd5cde22a6'),
              (0x343a,0x3452,'e996a8d795bf5610a8aeb1dc5d6c2fb85d7e3e2e802a076da891aaa8756a17da'),
              (0xb06e,0xb0be,'b830fe90abf0a25ce0ab13ec19bc850a12765d55dc917b841d83677d625453c0'),
              (0x6b7,0x6e6,'533cfcf554788a95f0db4fc9e5366962640dc038a33c2263c1482e9160663d2c')),
    'transfer':((0x4ce,0x4e1,'ab147e6094a0b0711b7c744f2f6ee242c5340eb277b5c42a7806f8dbef6f4d70'),
                (0x691,0x6d7,'d79c3073a10e30aa53365e842d8fc8a3411a77b6f56fc22aa508ca5650b1df45'))}

def require(condition,message):
    if not condition:raise ValueError(message)

def digest(raw):return hashlib.sha256(raw).hexdigest()

def snapshot(path,inputs):
    identity,raw=read_input(path);key=identity['path']
    require(key not in inputs or inputs[key]==identity,'同输入在两次读取间改变：'+key)
    inputs[key]=identity;return raw

def late_guard(inputs):
    for path,identity in inputs.items():
        actual,_=read_input(path);require(actual==identity,'晚守卫输入改变：'+path)

def new_output(path,report):
    path=Path(path).resolve()
    require(path.is_relative_to((ROOT/'build').resolve()),'输出只准工作树build内新隔离目录')
    reference=report.get('reference',{})
    for base in ((ROOT/'original_game').resolve(),reference.get('original'),str(Path(reference['dosbox']['path']).parent) if 'dosbox' in reference else None):
        if base:require(not path.is_relative_to(Path(base).resolve()),'输出指向原资源或工具安装目录')
    path.mkdir(parents=True,exist_ok=False);return path

def parse_log(raw):
    rows=[];names={'EAX','EBX','ECX','EDX','ESI','EDI','EBP','ESP','DS','ES','FS','GS','SS','FLG','CR0'}
    for line in raw.decode('ascii').splitlines():
        require(' EAX:' in line,'LOGL缺寄存器行')
        prefix,registers=line.split(' EAX:',1)
        match=re.match(r'^\S+\s+([0-9A-F]{4}):([0-9A-F]{8})\s+',prefix)
        # 原字节只以单空格相连；不得把前方反汇编立即数00并入ENTER原字节。
        code=re.search(r'(?<![0-9A-F])([0-9A-F]{2}(?: [0-9A-F]{2})*)[ ]*$',prefix)
        pairs=re.findall(r'\b(EAX|EBX|ECX|EDX|ESI|EDI|EBP|ESP|DS|ES|FS|GS|SS|FLG|CR0):([0-9A-F]+)','EAX:'+registers)
        require(match is not None and code is not None and len(pairs)==len(names) and {k for k,_ in pairs}==names,'LOGL格式/完整寄存器不符')
        rows.append({'index':len(rows),'CS':int(match[1],16),'EIP':int(match[2],16),
            'raw_hex':bytes.fromhex(code[1]).hex(),'registers':{k:int(v,16) for k,v in pairs}})
    require(1<len(rows)<=8192,'trace为空或超过8192条硬限');return rows

def check_endpoints(rows,trace,a,b):
    require(a['before']==a['after']==trace['start_registers'] and b['before']==b['after']==trace['stop_registers'],'快照/trace起止寄存器不一致')
    require(len(rows)==trace['trace_instructions'],'trace行数声明不符')
    for row,regs in ((rows[0],a['before']),(rows[-1],b['before'])):
        require((row['CS'],row['EIP'])==(regs['CS'],regs['EIP']),'trace起止PC不符')
        require(all(row['registers'][name]==regs[name] for name in ('EAX','EBX','ECX','EDX','ESI','EDI','EBP','ESP','DS','ES','SS','CR0'))
            and row['registers']['FLG']==regs['EFLAGS'],'trace起止寄存器不符')
        require(regs['CR0']&1 and not regs['EFLAGS']&(1<<17),'trace起止必须是protected非v86域')

def descriptor(memory,gdt,selector):
    require(selector!=0 and not selector&4,'只支持当次非空GDT selector')
    offset=selector&~7;require(offset+7<=gdt['limit'],'selector超过当次GDT limit')
    address=gdt['base']+offset;require(address+8<=len(memory),'GDT超过L0快照')
    low,high=struct.unpack_from('<II',memory,address)
    require(high&0x8000,'当次descriptor非present')
    limit=(low&0xffff)|((high>>16&15)<<16)
    if high&0x800000:limit=limit*4096+4095
    return {'selector':selector,'base':((low>>16)&65535)|((high&255)<<16)|(high&0xff000000),
        'limit':limit,'type':high>>8&31,'bits':32 if high&0x400000 else 16}

def gdt_view(lines):
    matches=re.findall(r'GDT Base:([0-9A-F]+) Limit:([0-9A-F]+)','\n'.join(lines))
    require(len(matches)==1,'trace起点GDT header不唯一')
    return dict(zip(('base','limit'),(int(v,16) for v in matches[0])))

def modules(memory,rows,trace):
    gdt=gdt_view(trace['GDT']);loader=trace['start_registers']['CS']
    transfers={r['CS'] for r in rows[:-1] if r['EIP']==0x6b0 and r['raw_hex']=='67f366a5'}
    require(len(transfers)==1,'trace没有唯一已证transfer段')
    result={role:descriptor(memory,gdt,selector) for role,selector in (('loader',loader),('transfer',transfers.pop()))}
    require(result['loader']['selector']!=result['transfer']['selector'],'loader/transfer角色冲突')
    for role,module in result.items():
        require(module['bits']==16 and module['type']&0x18==0x18,'窗口不是当次16bit代码段')
        require(module['base']+module['limit']+1<=len(memory),'loader段越过快照')
        for start,end,wanted in WINDOWS[role]:
            require(end<=module['limit']+1 and digest(memory[module['base']+start:module['base']+end])==wanted,'固定loader窗口身份改变：'+role+':'+hex(start))
    return result,gdt

def domain_join(memory,rows,domain,role_modules,capture_sha):
    require(domain.get('schema')=='fd2-le-startup-Domain-v1' and domain.get('database_saved') is False
        and domain.get('capture_sha256')==capture_sha,'Domain身份/保存边界不符')
    expected={(m['selector'],start,end) for role,m in role_modules.items() for start,end,_ in WINDOWS[role]}
    seen=set();instructions={}
    for window in domain['windows']:
        key=(window['selector'],*window['window']);require(key in expected and key not in seen,'Domain重复/未知窗口')
        seen.add(key);module=next(m for m in role_modules.values() if m['selector']==key[0])
        require(window['base_from_L0_RAM_GDT']==module['base'] and window['bitness']==16,'Domain坐标视图不符')
        at=key[1]
        for insn in window['instructions']:
            raw=bytes.fromhex(insn['raw_hex']);require(insn['cs_offset']==at and insn['size']==len(raw) and 0<len(raw)<=15,'Domain指令边界不连续')
            require(at+len(raw)<=key[2] and memory[module['base']+at:module['base']+at+len(raw)]==raw,'Domain原字节与L0不符')
            instructions[key[0],at]=raw;at+=len(raw)
        require(at==key[2],'Domain窗口尾残缺')
    require(seen==expected,'Domain缺完整六窗')
    checked=0
    for row in rows:
        key=row['CS'],row['EIP']
        if key in instructions:
            require(bytes.fromhex(row['raw_hex'])==instructions[key],'trace/Domain指令原字节不符');checked+=1
    require(checked>0,'trace没有Domain连接');return checked

def predict_writes(memory,rows,trace,role_modules,gdt,objects,bases,entry):
    """只重放已证四WORD及62→58字节链；不从actual/L1读取源。"""
    initial=trace['start_registers'];loader=role_modules['loader']['selector'];transfer=role_modules['transfer']['selector']
    def one(cs,pc):
        matches=[r for r in rows[:-1] if (r['CS'],r['EIP'])==(cs,pc)]
        require(len(matches)==1,'启动写点未恰好执行一次：'+hex(pc));return matches[0]
    def chunk(address,size):
        require(0<=address<=len(memory)-size,'启动源越过L0 RAM');return memory[address:address+size]
    def word(address):return struct.unpack('<H',chunk(address,2))[0]
    def dword(address):return struct.unpack('<I',chunk(address,4))[0]
    ss=descriptor(memory,gdt,initial['SS']);ds=descriptor(memory,gdt,initial['DS']);es=descriptor(memory,gdt,initial['ES'])
    bp=initial['EBP']&65535;local=ss['base']+bp-0x5c;context_offset=word(ss['base']+bp+0xe)
    require(0<=bp-0x5c and bp+0x12<=min(ss['limit']+1,65536) and 0x299e<=ds['limit']+1,'16bit栈/全局源越过合法读域')
    context_selector=word(ss['base']+bp+0x10)
    require(context_selector==initial['DS'],'62B目的selector与当前DS不符')
    require(context_offset+62<=min(ds['limit']+1,65536) and (initial['EBX']&65535)+20<=min(es['limit']+1,65536),'16bit目的/参数发生越界或回绕')
    frame=bytearray(chunk(local,62));parameter=es['base']+(initial['EBX']&65535)
    saved_ss=word(local+2);saved_sp=dword(local+0x18);stack=descriptor(memory,gdt,saved_ss)
    require(stack['base']==0 and stack['bits']==32 and stack['type'] in (0x12,0x13) and saved_sp==parameter and 58<=parameter<=stack['limit']+1,'启动栈不是当次已证flat视图')
    require(dword(local+0x32)==entry,'L0保存EIP不是该次LE entry')
    copy62=[r for r in rows[:-1] if (r['CS'],r['EIP'])==(loader,0xb0af)
        and r['registers']['DS']==initial['SS'] and r['registers']['ES']==context_selector
        and context_offset<=r['registers']['EDI']<context_offset+62]
    require(copy62 and copy62[0]['registers']['ESI']==bp-0x5c and copy62[0]['registers']['EDI']==context_offset
        and copy62[0]['registers']['ECX']==31,'copy62初态不符')
    require(all(not r['registers']['FLG']&0x400 for r in copy62),'copy62方向位不符')
    for left,right in zip(copy62,copy62[1:]):
        advance=left['registers']['ECX']-right['registers']['ECX']
        require(advance>0 and right['registers']['ESI']==left['registers']['ESI']+2*advance
            and right['registers']['EDI']==left['registers']['EDI']+2*advance,'copy62分块调用/写序不连续')
    copied=rows[copy62[-1]['index']+1]['registers']
    require((copied['ESI'],copied['EDI'],copied['ECX'])==(bp-0x5c+62,context_offset+62,0),'copy62未完整结束')
    context_writes=[];saved_cs=word(local+0x36);code=descriptor(memory,gdt,saved_cs)
    require(code['base']==0 and code['bits']==32 and code['type'] in (0x1a,0x1b) and entry<=code['limit'],'保存CS不是当次32bit flat代码视图')
    for pc,offset,value in ((0x3444,0x10,saved_cs),(0x3448,0x12,0),(0x6da,0x3a,0x200),(0x6e0,0x3c,0)):
        event=one(loader,pc);register='ES' if pc in (0x3444,0x3448) else 'DS'
        require(event['registers'][register]==context_selector,'context写selector不符')
        if pc in (0x3444,0x3448):require(event['registers']['EBX']&65535==context_offset,'context BX不符')
        else:require(struct.unpack('<H',bytes.fromhex(event['raw_hex'])[2:4])[0]==context_offset+offset,'context绝对写地址不符')
        frame[offset:offset+2]=struct.pack('<H',value)
        context_writes.append({'pc':pc,'trace_index':event['index'],'offset':offset,'bytes':struct.pack('<H',value).hex()})
    require(one(loader,0x3444)['registers']['EAX']&65535==saved_cs,'保存ESI写源不符')
    final=[r for r in rows[:-1] if (r['CS'],r['EIP'])==(transfer,0x6b0) and r['registers']['ES']==saved_ss]
    destination=parameter-58
    require(final and final[0]['registers']['ECX']==14,'copy58初始14 DWORD不符')
    for event in final:
        reg=event['registers'];done=14-reg['ECX']
        require(0<=done<14 and reg['DS']==context_selector and not reg['FLG']&0x400
            and reg['ESI']==context_offset+4+done*4 and reg['EDI']==destination+done*4,'copy58分块源/目的不符')
    require(all(a['registers']['ECX']>b['registers']['ECX'] for a,b in zip(final,final[1:])),'copy58计数/顺序不符')
    tails=[r for r in rows[:-1] if (r['CS'],r['EIP'])==(transfer,0x6b6) and r['registers']['ES']==saved_ss]
    require(len(tails)==1,'启动栈末WORD未恰好执行一次');tail=tails[0];reg=tail['registers']
    require(reg['ES']==saved_ss and reg['DS']==context_selector and not reg['FLG']&0x400
        and reg['ESI']==context_offset+60 and reg['EDI']==destination+56 and reg['ECX']==0,'末WORD实际源/目的/执行量不符')
    require(rows[tail['index']+1]['registers']['EDI']==parameter,'末WORD未实际前进2字节')
    order=[copy62[-1]['index'],*[w['trace_index'] for w in context_writes],final[0]['index'],tail['index']]
    require(all(a<b for a,b in zip(order,order[1:])),'62→context→58写序不符')
    pointer_offset=word(ds['base']+0x299a);pointer_selector=word(ds['base']+0x299c)
    pointer_descriptor=descriptor(memory,gdt,pointer_selector)
    require(pointer_offset+0x3c<=pointer_descriptor['limit']+1,'模块状态源越过selector limit')
    module=pointer_descriptor['base']+pointer_offset
    pointer1=dword(module+0x38);pointer2=dword(module+0x34);writes=[]
    def put(cs,pc,address,raw,events,source):
        owners=[o for o in objects if bases[o.index]<=address<address+len(raw)<=bases[o.index]+o.vsize]
        require(len(owners)==1,'启动写越过LE对象合法域')
        writes.append({'selector':cs,'offset':pc,'linear':address,'object':owners[0].index,
            'object_offset':address-bases[owners[0].index],'bytes':raw.hex(),'width':len(raw),
            'trace_indices':[r['index'] for r in events],'source':source})
    for pc,offset,value in ((0x3966,12,pointer1&65535),(0x396a,14,pointer1>>16),(0x397d,16,pointer2&65535),(0x3981,18,pointer2>>16)):
        event=one(loader,pc);reg=event['registers'];name='EAX' if pc in (0x3966,0x397d) else 'EDX'
        require(reg[name]&65535==value and reg['ES']==initial['ES'] and reg['EBX']&65535==initial['EBX']&65535,'直接WORD写源/目的不符')
        put(loader,pc,parameter+offset,struct.pack('<H',value),[event],'L0 DS:299a module +34/+38')
    direct_order=[w['trace_indices'][0] for w in writes]+[copy62[0]['index']]
    require(all(a<b for a,b in zip(direct_order,direct_order[1:])),'直接参数WORD与context copy顺序不符')
    put(transfer,0x6b0,destination,bytes(frame[4:60]),final,'L0 local62 -> context four WORD ->14 DWORD')
    put(transfer,0x6b6,destination+56,bytes(frame[60:62]),[tail],'同源末WORD；包含零写')
    return writes,{'local62_linear':local,'local62_sha256':digest(chunk(local,62)),
        'context_offset':context_offset,'context_selector':context_selector,'context_WORD_writes':context_writes,
        'predicted_entry_ESP':parameter,'saved_entry_CS':saved_cs}

def compare_objects(expected,actual,objects,bases):
    for obj in objects:
        base=bases[obj.index];require(0<=base<=len(actual)-obj.vsize,'对象超出RAM')
        left=expected[base:base+obj.vsize];right=actual[base:base+obj.vsize]
        require(len(left)==len(right)==obj.vsize,'对象expected不完整')
        if left!=right:
            offset=next(i for i,(a,b) in enumerate(zip(left,right)) if a!=b)
            raise ValueError(f'完整对象字节不同 obj{obj.index}:{offset:#x} expected={left[offset]:02x} actual={right[offset]:02x}')

def verify_capture(report_path,exe_path,domain_path,output):
    inputs={};report_raw=snapshot(report_path,inputs);report=parse_json(report_raw.decode('utf8'))
    out=new_output(output,report)
    result={'schema':'fd2-le-startup-layered-v1','ok':False,'masked_bytes':0,'excluded_bytes':0,'whole_F1_complete':False}
    try:
        for path in (Path(__file__),ROOT/'tools/original_reference.py',ROOT/'tools/analyze_fd2_audio.py',ROOT/'tools/rebuild_fd2_analysis.py'):
            snapshot(path,inputs)
        require(report.get('schema')=='fd2-strict-loaded-image-runtime-v2' and report.get('ok') is True and report.get('sources_unchanged') is True,'原生收据未合格')
        require(report['foreground'].get('sampling_complete') is True and report['owned_DOSBox_foreground_not_observed'] is True,'owned前台证据不完整')
        require(report['processes'] and all(p['cleaned'] and not p['forced_cleanup'] and p['exit_code']==0 for p in report['processes']),'原生owner未正常退出')
        trace=report['startup_trace'];require(trace['schema']=='fd2-le-startup-trace-v1' and trace['complete'] is True
            and trace['instruction_limit']==8192 and trace['command']=='EXEC LOGL 2000' and trace['last_line_unexecuted'] is True,'缺少显式新trace资格')
        a,b=report['loaded_snapshot'],report['entry_snapshot'];before=snapshot(a['path'],inputs);actual=snapshot(b['path'],inputs)
        require(len(before)==len(actual)==16*1024*1024 and digest(before)==a['sha256'] and digest(actual)==b['sha256'],'L0/L1 RAM身份不符')
        raw=snapshot(trace['path'],inputs);require(len(raw)==trace['size'] and digest(raw)==trace['sha256'],'trace摘要不符')
        rows=parse_log(raw);check_endpoints(rows,trace,a,b)
        exe=snapshot(exe_path,inputs);require(digest(exe)==EXE_SHA256,'不支持其它原EXE/loader参照')
        le=parse_le(exe);objects=parse_objects(exe,le);fixups=parse_fixups(exe,le,objects);bases={int(k):v for k,v in report['object_bases'].items()}
        require(set(bases)=={o.index for o in objects},'object基址分母不闭合')
        spans=sorted((bases[o.index],bases[o.index]+o.vsize) for o in objects)
        require(all(a[1]<=b[0] for a,b in zip(spans,spans[1:])),'LE对象运行域重叠')
        raw_image=build_relbase_image(exe,le,objects);static=bytearray(before)
        for obj in objects:
            require(0<=bases[obj.index]<=len(before)-obj.vsize,'L0对象超出RAM')
            static[bases[obj.index]:bases[obj.index]+obj.vsize]=raw_image[obj.relbase:obj.relbase+obj.vsize]
        source_addresses=[]
        for fixup in fixups:
            owners=[o for o in objects if o.relbase<=fixup.source_addr_relbase<fixup.source_addr_relbase+4<=o.relbase+o.vsize]
            require(len(owners)==1,'fixup source对象不唯一');obj=owners[0]
            address=bases[obj.index]+fixup.source_addr_relbase-obj.relbase;source_addresses.append(address)
            struct.pack_into('<I',static,address,bases[fixup.target_object]+fixup.target_offset)
        compare_objects(static,before,objects,bases)
        roles,gdt=modules(before,rows,trace);domain=parse_json(snapshot(domain_path,inputs).decode('utf8'))
        joined=domain_join(before,rows,domain,roles,digest(report_raw))
        entry=bases[le.entry_object]+le.entry_eip
        require(b['before']['EIP']==entry,'L1不是真实LE entry')
        writes,source=predict_writes(before,rows,trace,roles,gdt,objects,bases,entry);expected=bytearray(before)
        for write in writes:expected[write['linear']:write['linear']+write['width']]=bytes.fromhex(write['bytes'])
        compare_objects(expected,actual,objects,bases)
        require(b['before']['ESP']==source['predicted_entry_ESP'] and b['before']['CS']==source['saved_entry_CS'],'实际入口CS/ESP与源不符')
        changes=[]
        for obj in objects:
            for offset in range(obj.vsize):
                address=bases[obj.index]+offset
                if before[address]!=actual[address]:
                    owners=[w for w in writes if w['linear']<=address<w['linear']+w['width']]
                    require(len(owners)==1,'L1差异未恰好归属一个有源writer')
                    changes.append({'object':obj.index,'offset':offset,'before':before[address],'actual':actual[address],
                        'writer_selector':owners[0]['selector'],'writer_offset':owners[0]['offset']})
        late_guard(inputs)
        result.update(ok=True,L0_static_validation_passed=True,L1_static_zero_model_passed=not changes,
            L1_L0_plus_source_proven_loader_writes_passed=True,checked_full_object_virtual_bytes=sum(o.vsize for o in objects),
            fixup_records_checked=len(source_addresses),unique_fixup_sources=len(set(source_addresses)),trace_instructions=len(rows),
            Domain_matched_trace_rows=joined,object_write_ledger=writes,changed_bytes=changes,source=source,
            write_bytes_including_zeros=sum(w['width'] for w in writes),sources_unchanged=True,
            capture_identity=inputs[str(Path(report_path).resolve())],inherited_evidence=report.get('evidence_migration'),
            limits=['固定Windows参照的loader启动；不签全RAM/玩法/RNG/音画/时序','旧零BSS失败不回签；本项仅新收据的L1有源分层门'])
    except Exception as error:
        result['error']={'type':type(error).__name__,'message':str(error)}
        try:late_guard(inputs)
        except Exception as failure:result['late_guard_error']={'type':type(failure).__name__,'message':str(failure)}
    result['inputs']=inputs
    with (out/'report.json').open('x',encoding='utf8') as stream:json.dump(result,stream,ensure_ascii=False,indent=2);stream.write('\n')
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for flag in ('capture','exe','domain','artifacts'):parser.add_argument('--'+flag,type=Path,required=True)
    args=parser.parse_args();result=verify_capture(args.capture,args.exe,args.domain,args.artifacts)
    print(json.dumps({k:result[k] for k in ('ok','masked_bytes','excluded_bytes')},ensure_ascii=False));return 0 if result['ok'] else 1

if __name__=='__main__':raise SystemExit(main())
