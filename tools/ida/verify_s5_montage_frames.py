"""31529真实montage调用后缀原码→实际C有限叶逐帧/HP/RNG差分。

原PC产生命令和64000B页面，绝不从生产冷图生成expected。完整memmove/
stackprobe/RLE/DAC/query/SFX执行原码；仅RNG值、BIOS等待和OUT硬件为替身。
不执行整个ending/credits/file/heap，不声明原生窗口或声音/时间等价。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import sys
import zlib

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
from dat_extract import parse_archive
from verify_s5_cinematic import unique_keys,validate_output

IMAGE_SHA='8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1'
EXTENTS={0x31529:0x31bdf,0x2e2b0:0x2e95b,0x2e9a8:0x2eb9f,0x2ebe1:0x2f4d4,
 0x2eb9f:0x2ebe1,0x4e98d:0x4eb48,0x11eb0:0x11eee,0x3771c:0x3776e,
 0x11d40:0x11df2,0x190ac:0x1956b,0x37ae5:0x37af1,0x3702f:0x3703f,
 0x37042:0x3705f,0x1f0dc:0x1f183,0x37932:0x37940,0x1b83d:0x1b8a6,
 0x1b722:0x1b750,0x4e8bc:0x4e8d3,0x25a96:0x25b45,0x4ebe3:0x4ebff}
P=0x708000;HP=P-112;ENTRY=P-36

def digest(raw):return hashlib.sha256(raw).hexdigest()

def validate_source(image,report):
    if digest(image)!=IMAGE_SHA or report.get('image_sha256')!=IMAGE_SHA:
        raise ValueError('strict image identity differs')
    functions=report.get('functions',[]);rows={r['va']:r for r in functions}
    if len(rows)!=len(functions) or set(rows)!=set(EXTENTS):raise ValueError('complete20-owner montage readset required')
    for va,end in EXTENTS.items():
        row=rows[va];covered=set();decoded=set()
        if row.get('end')!=end or not row.get('blocks') or not row.get('instructions'):
            raise ValueError('complete original CFG/instructions missing')
        for chunk in row.get('chunks',[]):
            raw=image[chunk['start']:chunk['end']]
            if raw.hex()!=chunk['raw_hex'] or digest(raw)!=chunk['sha256']:raise ValueError('original chunk differs')
            covered.update(range(chunk['start'],chunk['end']))
        for ins in row['instructions']:
            if image[ins['ea']:ins['ea']+ins['size']].hex()!=ins['raw_hex']:raise ValueError('instruction source differs')
            decoded.update(range(ins['ea'],ins['ea']+ins['size']))
        if covered!=set(range(va,end)) or covered!=decoded:raise ValueError('sparse owner cannot prove stack/frame flow')
    for request in report.get('requested_entries',[]):
        owner=rows.get(request['owner_va'])
        if not owner or request['requested_va'] not in {ins['ea']for ins in owner['instructions']}:
            raise ValueError('requested source is not an original instruction head')
    return rows

def validate_row(actual,expected):
    if set(actual)!={'status','hp','rng','yes','pixels_hex'} or type(actual['status']) is not int or actual['status']!=0:
        raise ValueError('actual C status/protocol differs')
    if actual['hp']!=expected['hp'] or any(type(v) is not int for v in actual['hp']) or \
        type(actual['rng']) is not int or actual['rng']!=expected['rng'] or \
        type(actual['yes']) is not int or actual['yes']!=expected['yes']:
        raise ValueError('actual C HP/RNG/branch differs')
    text=actual['pixels_hex'];wanted=expected.get('pixels',b'')
    if not isinstance(text,str) or len(text)!=len(wanted)*2 or bytes.fromhex(text)!=wanted:
        raise ValueError('actual C complete framebuffer differs')

def execute(image,rows,archives,pair,rng):
    import unicorn
    import unicorn.x86_const as x
    cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    cpu.mem_map(0,0x64000);cpu.mem_write(0,image[:0x64000])
    cpu.mem_map(0xa0000,0x10000);cpu.mem_map(0x700000,0x10000)
    cpu.mem_map(0x910000,0x1000);cpu.mem_write(0x910000,struct.pack('<3Q',0,0x00cf9a000000ffff,0x00cf92000000ffff))
    cpu.reg_write(x.UC_X86_REG_GDTR,(0,0x910000,0xfff,0))
    for reg,selector in ((x.UC_X86_REG_CS,8),(x.UC_X86_REG_DS,16),(x.UC_X86_REG_ES,16),(x.UC_X86_REG_SS,16)):
        cpu.reg_write(reg,selector)
    cpu.mem_write(0x700000,b'\xff'*0x10000)
    first,second,scene=(image[va+pair]for va in (0x526e9,0x526fd,0x52711))
    work,background,idle_a,idle_b,attack,reverse,tai,bg,palette,units=\
        (0x100000,0x140000,0x200000,0x240000,0x280000,0x2c0000,0x300000,0x340000,0x380000,0x3c0000)
    def data(address,raw):
        cpu.mem_map(address,(max(4096,len(raw))+4095)&~4095);cpu.mem_write(address,raw)
    for address,raw in ((work,bytes(128000)),(background,bytes(64000)),
        (idle_a,archives['FIGANI'][3*first]),(idle_b,archives['FIGANI'][3*second]),
        (attack,archives['FIGANI'][3*first+1]),(reverse,archives['FIGANI'][3*second+1]),
        (tai,archives['TAI'][3 if first in (26,54) or second==55 else scene]),
        (bg,archives['BG'][scene]),(palette,archives['FDOTHER'][0]),(units,bytes(160))):data(address,raw)
    def put(address,*values):cpu.mem_write(address,struct.pack('<'+'I'*len(values),*values))
    def get(address):return struct.unpack('<I',cpu.mem_read(address,4))[0]
    put(0x53a45,units);put(0x53a65,palette);put(0x54133,scene)
    put(0x5414b,0xdeadbeef,0xcafebabe);cpu.mem_write(0x53ef1,b'\x01');cpu.mem_write(0x51e62,b'\x01')
    cpu.mem_write(units+6,bytes((2 if first<76 else 0,first)))
    cpu.mem_write(units+86,bytes((2 if second<76 else 0,second)))
    # 纯反击查询四条实际路径；不影响override必做的第二侧，仍执行真helper。
    mode=pair%4
    if mode==0:cpu.mem_write(units+118,b'\x01')
    elif mode in (2,3):
        cpu.mem_write(units+80,b'\x01')
        if mode==3:
            item=next(i for i in range(128)if image[0x602ad+23*i+11]==1)
            cpu.mem_write(units+90,bytes((64,item)))
    out_count=0
    def output(machine,port,size,value,userdata):
        nonlocal out_count
        if port not in (0x3c8,0x3c9) or size!=1:raise ValueError('unknown original port')
        out_count+=1
    cpu.hook_add(unicorn.UC_HOOK_INSN,output,None,1,0,x.UC_X86_INS_OUT)
    put(P,0x900000,bg,0,50,background,320,0xffffffff);cpu.reg_write(x.UC_X86_REG_ESP,P)
    cpu.emu_start(0x4e98d,0x900000,count=2000000)
    cpu.mem_write(0x700000,b'\xff'*0x10000)
    put(P+0x14,0);put(P+0x1c,attack,reverse,idle_b,idle_a,tai);put(P+0x40,0x900000,0,1)
    cpu.reg_write(x.UC_X86_REG_ESP,P);cpu.reg_write(x.UC_X86_REG_EBP,background);cpu.reg_write(x.UC_X86_REG_EDI,work)
    events=[];rng_count=0;half=-1;entries=0;last_write=None;ticks=0;sfx=0
    def hp():return list(struct.unpack('<H',cpu.mem_read(units+64,2))+struct.unpack('<H',cpu.mem_read(units+144,2)))
    def emit(command,pc,*,frame=False,yes=-1,write=None):
        values=hp()
        if write:values[write[0]]=write[1]
        events.append({'command':command,'source_pc':pc,'hp':values,'rng':rng_count,'yes':yes,
            **({'pixels':bytes(cpu.mem_read(0xa0000,64000))}if frame else {})})
    emit(f'o {first} {second} {scene}',0x31aee)
    def at(machine,pc,size,userdata):
        nonlocal rng_count,half,entries,ticks,sfx
        sp=cpu.reg_read(x.UC_X86_REG_ESP)
        if pc in (0x2ea3d,0x2eb19):emit('i '+str(cpu.reg_read(x.UC_X86_REG_ESI)),pc,frame=True)
        elif pc==0x2e6bf:emit('f',pc)
        elif pc==0x2ebe1:
            half=get(sp+4);entries+=1
            if sp!=ENTRY or get(sp-0x4c)!=0xaf8c0:raise ValueError('natural caller HP last-write lost')
            emit('h '+str(half),pc)
        elif pc==0x4ebe3:
            rng_count+=1;emit('r '+str(rng),pc,yes=int(rng%100<3))
            cpu.reg_write(x.UC_X86_REG_EAX,rng);cpu.reg_write(x.UC_X86_REG_EIP,get(sp));cpu.reg_write(x.UC_X86_REG_ESP,sp+4)
        elif pc==0x17aa9:
            ticks+=1;cpu.reg_write(x.UC_X86_REG_EAX,0);cpu.reg_write(x.UC_X86_REG_EIP,get(sp));cpu.reg_write(x.UC_X86_REG_ESP,sp+4)
        elif pc==0x25a96:
            sfx+=1
            if get(0x54133)==0:raise ValueError('montage reached unqualified sound mode')
            emit('c '+str(cpu.reg_read(x.UC_X86_REG_EDI)),pc)
        elif pc==0x2efc8:emit('t',pc)
        elif pc==0x2f0c6:emit('d '+str(cpu.reg_read(x.UC_X86_REG_EDI)),pc,frame=True)
    for address in (0x2ea3d,0x2eb19,0x2e6bf,0x2ebe1,0x4ebe3,0x17aa9,0x25a96,0x2efc8,0x2f0c6):
        cpu.hook_add(unicorn.UC_HOOK_CODE,at,None,address,address)
    def written(machine,access,address,size,value,userdata):
        nonlocal last_write
        pc=cpu.reg_read(x.UC_X86_REG_EIP)
        if address==HP:last_write={'pc':pc,'size':size,'value':value}
        elif address in (units+64,units+144):
            raw=int(address==units+144)
            if pc!=0x2ef9e or size!=2 or value!=63680:raise ValueError('unexpected original HP write')
            emit(f'p {raw} {value}',pc,write=(raw,value))
    cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE,written,None,HP,HP+3)
    cpu.hook_add(unicorn.UC_HOOK_MEM_WRITE,written,None,units+64,units+145)
    def forbidden(machine,pc,size,userdata):raise ValueError('montage reached AIL '+hex(pc))
    for address in rows[0x25a96]['callees']:
        if address!=0x3702f:cpu.hook_add(unicorn.UC_HOOK_CODE,forbidden,None,address,address)
    cpu.emu_start(0x2e6a4,0x2e763,count=25000000)
    if cpu.reg_read(x.UC_X86_REG_EIP)!=0x2e763 or entries!=2 or rng_count!=2 or \
        last_write!={'pc':0x3771c,'size':4,'value':0xaf8c0}:raise ValueError('complete original pair did not close')
    emit('q',0x2e763)
    return events,{'pair':pair,'actors':[first,second],'scene':scene,'rng_value':rng,
        'counter_path':mode,'function_entries':entries,'rng_calls':rng_count,'BIOS_calls':ticks,
        'suppressed_sfx_calls':sfx,'OUT_instructions':out_count,'last_HP_seed_write':last_write}

def main(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','image','readset','probe','output'):parser.add_argument('--'+name,required=True,type=Path)
    args=parser.parse_args(argv);root=args.root.resolve();output=args.output.resolve();original=root/'original_game'
    validate_output(output,(args.image,args.readset,args.probe))
    if output.is_relative_to(original.resolve()) or original.resolve().is_relative_to(output):
        raise ValueError('output cannot contain or modify original resources')
    paths=[args.image,args.readset,args.probe,Path(__file__)]+[original/(name+'.DAT')for name in ('FIGANI','TAI','BG','FDOTHER')]
    for name in ('battle_animation.c','battle_animation.h','scene_cinematic.c','scene_cinematic.h','scene_cinematic_flow.h','tests/scene_cinematic_test.c'):
        paths.append(root/'src'/name)
    inputs={str(path.resolve()):digest(path.read_bytes())for path in paths}
    image=args.image.read_bytes();rows=validate_source(image,json.loads(args.readset.read_text(encoding='utf8'),object_pairs_hook=unique_keys))
    archives={name:parse_archive((original/(name+'.DAT')).read_bytes())[1]for name in ('FIGANI','TAI','BG','FDOTHER')}
    output.mkdir(parents=True);report={'schema':'fd2-s5-montage-actualC-frames-v1','ok':False,'cases':[],
        'input_sha256':inputs,'probe_before_sha256':inputs[str(args.probe.resolve())],
        'limits':['original final2e2b0 call slice, complete intro/both2ebe1/query/SFX, actual original resources',
            'only scripted RNG0/50, BIOSdelay and OUT hardware sinks; no full ending/credits/file/heap/native/time/audio claim',
            'C finite leaves driven by originalPC events; production coldgraph and shipping bindings require separate gates']}
    try:
        for pair in range(20):
            for rng in (0,50):
                events,case=execute(image,rows,archives,pair,rng);work=output/f'pair{pair:02d}-rng{rng}';work.mkdir()
                commands='\n'.join(event['command']for event in events)+'\n';(work/'commands.txt').write_text(commands)
                source_rows=[]
                with (work/'original-frames.zbin').open('wb')as frames:
                    for event in events:
                        row={k:v for k,v in event.items()if k!='pixels'}
                        if 'pixels'in event:
                            packed=zlib.compress(event['pixels']);row.update(frame_offset=frames.tell(),frame_size=len(packed),pixels_sha256=digest(event['pixels']))
                            frames.write(packed)
                        source_rows.append(row)
                (work/'original-events.json').write_text(json.dumps(source_rows,indent=2)+'\n')
                with (work/'actual-C.jsonl').open('wb')as stream:
                    process=subprocess.run([str(args.probe.resolve()),'--montage-vectors'],input=commands.encode(),
                        stdout=stream,stderr=subprocess.PIPE,cwd=root,timeout=60)
                (work/'actual-C.stderr.txt').write_bytes(process.stderr)
                if process.returncode:raise ValueError(f'actual C probe exit {process.returncode}: pair{pair}/rng{rng}')
                with (work/'actual-C.jsonl').open(encoding='ascii')as stream:
                    for index,event in enumerate(events):
                        line=stream.readline()
                        if not line:raise ValueError('actual C omitted original command')
                        try:validate_row(json.loads(line,object_pairs_hook=unique_keys),event)
                        except ValueError as error:raise ValueError(f'pair{pair}/rng{rng} command{index} {event["command"]}: {error}')from error
                    if stream.read():raise ValueError('actual C has unmatched output')
                case.update(ok=True,commands=len(events),full_frames=sum('pixels'in event for event in events),path=str(work))
                report['cases'].append(case)
            print(json.dumps({'pair':pair,'complete_actual_C':True}),flush=True)
        report['ok']=True
    except Exception as error:report['error']={'type':type(error).__name__,'message':str(error)}
    finally:
        report['sources_unchanged']=all(Path(path).is_file()and digest(Path(path).read_bytes())==value for path,value in inputs.items())
        report['probe_after_sha256']=digest(args.probe.read_bytes());report['ok'] &= report['sources_unchanged']
        report['case_count']=len(report['cases']);report['full_frame_count']=sum(case['full_frames']for case in report['cases'])
        (output/'comparison.json').write_text(json.dumps(report,indent=2)+'\n')
    print(json.dumps({'ok':report['ok'],'case_count':report['case_count'],'frames':report['full_frame_count'],'output':str(output)}))
    return int(not report['ok'])

if __name__=='__main__':raise SystemExit(main())
