#!/usr/bin/env python3
"""执行完整原1e5c0，核水色/取键/signed WORD两次读钟及尾clear顺序。

七个完整Domain函数字节先和strict relocated镜像比对；Watcom栈检查、水色、
输入和clear是显式替身。原BIOS WORD读仍由真实指令执行，九类向量可与真实
私有Play C consumer的JSON对比。不模拟PCM/绘图/实际BIOS时间，不属WGC验收。
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

IMAGE_SHA='8c40cac1a4eb947007740c90a4bae8acdb49b3922a58f565eda0454bf4d238d1'
FUNCTIONS={
    0x1a866:(0x1aa1d,'de9b153fcda9e8979cd6e09657da17dee40af3271fc97282a6d0010fdd064773'),
    0x1e292:(0x1e529,'6cb16c742a551e6ded5c7076a01ba2ccb8955fee9912257fccb92793d77d3614'),
    0x1e529:(0x1e5c0,'b313e2e8c306f9fd924c478206426a6404457aa4aec03502afea220c52e15e25'),
    0x1e5c0:(0x1e611,'e9edc6715a04c11222290f6108decd5f54409f1adc7f209963bd0a599ced8e4d'),
    0x17aa9:(0x17aed,'4c699109f96bc0134c6f7039bc12325289b5f1b0dea02c60c46cd35d61091e71'),
    0x10620:(0x10652,'50eb04762fc213124909b4ee901660b8d21bd84c5c370724ab5c19cf550c8986'),
    0x3790a:(0x3790f,'1826539e8c4fd904e9dfdef145ea37ee54154c5203299c30a017a101a3fbe715')}
CASES=(
    ('deadline10',10,100,((100,100,0),(104,104,0),(110,110,0))),
    ('deadline11',11,100,((100,100,0),(110,110,0),(111,111,0))),
    ('queued_key',10,100,((100,100,1),)),
    ('late_key',10,100,((100,100,0),(101,101,1))),
    ('zero_do_once',0,100,((100,100,0),)),
    ('signed_half_wrap',10,0x7fff,((0x8000,0x8000,0),)),
    ('ffff_to_zero',10,0xffff,((0,0,0),(8,8,0),(9,9,0))),
    ('negative_start',10,0x8000,((0x8000,0x8000,0),(0x8009,0x8009,0),(0x800a,0x800a,0))),
    ('second_read_backwards',10,100,((101,99,0),)))


def validate_source(image,readset):
    if hashlib.sha256(image).hexdigest()!=IMAGE_SHA:raise ValueError('strict镜像身份不同')
    checked=[]
    for start,(end,sha) in FUNCTIONS.items():
        rows=[f for f in readset['functions'] if f['va']==start]
        if len(rows)!=1 or rows[0]['end']!=end:raise ValueError('完整Domain函数边界缺失:'+hex(start))
        row=rows[0]
        if row['chunks']!=[{'start':start,'end':end}]:raise ValueError('完整Domain chunk不同:'+hex(start))
        cursor=start;raw=bytearray()
        for insn in row['instructions']:
            part=bytes.fromhex(insn['bytes'])
            if insn['va']!=cursor or not part:raise ValueError('Domain指令不连续:'+hex(start))
            raw.extend(part);cursor+=len(part)
        if cursor!=end or raw!=image[start:end] or hashlib.sha256(raw).hexdigest()!=sha:
            raise ValueError('Domain完整原字节不同:'+hex(start))
        checked.append({'va':hex(start),'end':hex(end),'sha256':sha})
    # 1a866两个STATUS text后clear属于caller；1e292 CLOSE没有这项clear。
    calls={0x1a928:0x4e381,0x1a92f:0x1e5c0,0x1a9fb:0x4e381,
        0x1aa02:0x1e5c0,0x1e506:0x1e5c0,0x1e5d9:0x4e31c,
        0x1e5de:0x10620,0x1e608:0x4e381}
    for at,target in calls.items():
        if image[at]!=0xe8 or at+5+struct.unpack_from('<i',image,at+1)[0]!=target:
            raise ValueError('原caller/water/clear锚不同:'+hex(at))
    if image[0x1e504:0x1e506]!=bytes.fromhex('6a0b'):raise ValueError('CLOSE11tick参数不同')
    return checked


def signed_word(value):
    return value if value<0x8000 else value-0x10000


def model(case):
    name,ticks,start,steps=case;events=[];clock=0
    for index,(first,second,key) in enumerate(steps):
        events.extend(['water','input','clock_first']);clock+=1
        timed=signed_word(first)-signed_word(start)>=ticks
        if not timed:
            events.append('clock_second');clock+=1
            timed=signed_word(second)<signed_word(start)
        if key or timed:
            if index!=len(steps)-1:raise ValueError('向量结束后还有未消费步骤')
            events.append('clear')
            return {'name':name,'water':index+1,'input':index+1,'clock_reads':clock,'tail_clear':1},events
    raise ValueError('向量没有退出条件')


def execute(image,case):
    import unicorn
    from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_ESP,UC_X86_REG_EAX
    cpu=unicorn.Uc(unicorn.UC_ARCH_X86,unicorn.UC_MODE_32)
    cpu.mem_map(0,0x1000);cpu.mem_map(0x10000,0x54000)
    cpu.mem_write(0x10000,image[0x10000:0x64000]);cpu.mem_map(0x200000,0x10000)
    stop,stack=0x400000,0x208000
    name,ticks,start,steps=case
    cpu.mem_write(stack,struct.pack('<II',stop,ticks));cpu.reg_write(UC_X86_REG_ESP,stack)
    events=[];current=-1;start_reads=0
    def read(at):return struct.unpack('<I',cpu.mem_read(at,4))[0]
    def ret(result=0,extra=0):
        sp=cpu.reg_read(UC_X86_REG_ESP)
        cpu.reg_write(UC_X86_REG_EAX,result);cpu.reg_write(UC_X86_REG_EIP,read(sp))
        cpu.reg_write(UC_X86_REG_ESP,sp+4+extra)
    def hook(cpu,address,size,userdata):
        nonlocal current
        if address==stop:cpu.emu_stop();return
        if address==0x3702f:ret(extra=4);return
        if address==0x4e31c:
            current+=1
            if current>=len(steps):raise ValueError('原函数超过向量域')
            events.append('water');ret();return
        if address==0x10620:events.append('input');ret(steps[current][2]);return
        if address==0x4e381:events.append('clear');ret()
    def bios_read(cpu,access,address,size,value,userdata):
        nonlocal start_reads
        pc=cpu.reg_read(UC_X86_REG_EIP)
        if pc==0x1e5d6:tick=start;start_reads+=1
        elif pc==0x1e5ea:tick=steps[current][0];events.append('clock_first')
        elif pc==0x1e5f8:tick=steps[current][1];events.append('clock_second')
        else:raise ValueError('未登记BIOS读取点:'+hex(pc))
        cpu.mem_write(0x46c,struct.pack('<H',tick))
    cpu.hook_add(unicorn.UC_HOOK_CODE,hook)
    cpu.hook_add(unicorn.UC_HOOK_MEM_READ,bios_read,begin=0x46c,end=0x46d)
    cpu.emu_start(0x1e5c0,stop,count=10000)
    expected,expected_events=model(case)
    if cpu.reg_read(UC_X86_REG_EIP)!=stop or start_reads!=1 or events!=expected_events:
        raise ValueError('完整原1e5c0轨迹不同:'+str({'name':name,'actual':events,'expected':expected_events}))
    return dict(expected,events=events)


def compare_c(cases,document):
    expected=[{k:v for k,v in row.items() if k!='events'} for row in cases]
    if document.get('schema')!='fd2.g4.notice-wait-c.v1' or document.get('cases')!=expected:
        raise ValueError('C/完整原码九类向量不同')
    return {'ok':True,'cases':len(cases),'scope':'实际私有Play helper；时钟/输入/water/present明确替身，非原生窗口'}


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('image','readset','output'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--c-probe',type=Path)
    args=parser.parse_args();image=args.image.read_bytes()
    functions=validate_source(image,json.loads(args.readset.read_text(encoding='utf-8')))
    cases=[execute(image,case) for case in CASES]
    result={'ok':True,'schema':'fd2.g4.notice-wait-original.v1','image_sha256':IMAGE_SHA,
        'readset_sha256':hashlib.sha256(args.readset.read_bytes()).hexdigest(),
        'functions':functions,'cases':cases,'c_comparison':None,
        'scope':'完整原1e5c0 CPU；water/input/clear/stackcheck替身；七函数只核字节及caller，非其完整业务执行或WGC'}
    if args.c_probe:
        process=subprocess.run([str(args.c_probe.resolve()),'--notice-wait-json'],check=True,capture_output=True,text=True)
        result['c_comparison']=compare_c(cases,json.loads(process.stdout))
        result['c_probe_sha256']=hashlib.sha256(args.c_probe.read_bytes()).hexdigest()
    if args.output.exists():raise ValueError('输出需新路径，不覆盖旧证据')
    args.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    print(json.dumps({'ok':True,'cases':len(cases),'c_comparison':result['c_comparison'],'output':str(args.output)},ensure_ascii=False))


if __name__=='__main__':main()
