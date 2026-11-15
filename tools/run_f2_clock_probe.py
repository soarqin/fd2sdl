"""F2正常START两页只读寄存器观察；不暂停、改写或读取原栈。

来源：2000a/117e7/11aa8、1b1e7、__CMain@4609b（4609d设置EBP）；
复用原F1/概要资源WGC门和OwnedRPC。局部代码两锚/合法LE fixup映射，
RAM只读当前代码及53a45/53beb两个DWORD，不使用整RAM snapshot/roster扫描。
每条MEMDUMPBIN以同命令长度表达式拒绝PE0/VM1/PG1；descriptor身份另账。
观察是具体profile的非原子样本，不签首次比较命中、完整轨迹或全F2一致。
"""
from __future__ import annotations
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import sys
import time

import fd2_dosbox_snapshot as snapshot
from fd2_display_requirement import windows_display_requirement
from rebuild_fd2_analysis import parse_le, parse_objects, parse_fixups, build_relbase_image
from run_f2_input_check import sha, source_facts, validate_output
from run_f2_prepare_check import command_configuration
from run_original_battle_animation import owned_job_required
from run_original_comparison import Capture, hashes, quiet_game_command, running, title_templates
from run_original_battle_menu import wait_state, wait_title
from run_original_end_turn import META, RoundOracle
from run_original_load import require_source_unchanged
import original_reference
import run_original_f1 as auxiliary
import run_original_overview as overview

EXE_SHA = 'bb35004c06fc483e68f869bb7eb14dde9f9b7e585af29501af5ad1004c8861cd'
REGISTERS = ('FS','GS','EBP','ESI','CS','EIP','DS','SS','ESP','CR0','EFLAGS')
EV = 'EXEC EV ' + ' '.join(REGISTERS)
ANCHORS = (0x2615c, 0x2b781)
WINDOW = 96
RAM_LIMIT = 16 * 1024 * 1024
READ_LIMIT = 2048
READ_ATTEMPTS = 8
COMMAND_LIMIT = 512
PROBE_SECONDS = 30
MODE_FACTOR = '(CR0&1)*(1-((EFLAGS>>11)&1))*(1-((CR0>>1F)&1))'
MODE_SOURCE = {'version':'dosbox-x-v2026.10.01',
    'debug_cpp_sha256':'29ed1e6b459127b49cf74b1b26251c67b500d07e5db4a86f9ae956d4b958d1df',
    'contract':'GetHexValue / ParseCommand MEMDUMPBIN / SaveMemoryBin; static source qualification, not a new runtime parser test'}
FIELDS = {'active_pointer':0x3a45, 'active_count':0x3beb}
BODY = {'auxiliary':(0x201cc,0x202e7), 'overview':(0x1b2ee,0x1b389)}


def write(path, value):
    Path(path).write_text(json.dumps(value,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')


def parse_registers(lines):
    header = "EV of '" + ' '.join(REGISTERS) + "' is:"
    rows = [line.strip() for line in lines if re.fullmatch(r'[0-9a-fA-F]{1,8}(?:\s+[0-9a-fA-F]{1,8}){10}',line.strip())]
    if lines.count(header)!=1 or len(rows)!=1:
        raise ValueError('寄存器EV必须有唯一、同顺序11字段响应')
    result = dict(zip(REGISTERS,(int(value,16) for value in rows[0].split())))
    if any(result[key]>0xffff for key in ('FS','GS','CS','DS','SS')):
        raise ValueError('selector响应超16bit')
    return result


def protected(registers):
    return bool(registers['CR0']&1) and not registers['EFLAGS']&(1<<17) and all(registers[k] for k in ('CS','DS','SS'))


def descriptor_valid(value, *, code):
    return (len(value['flags'])==5 and value['flags'][0]=='1' and value['flags'][-2]=='1'
            and bool(value['type']&8)==code and bool(value['type']&0x10))


class ContextNotClient(ValueError):
    """真实采到外部／临时上下文；不能代签App flat-client画像。"""


class CodeModel:
    def __init__(self,image,fixups,objects):
        self.image=image;self.objects={obj.index:obj for obj in objects}
        self.start=self.objects[1].relbase;self.end=self.start+self.objects[1].vsize
        self.fixups=[];self.mask=bytearray(b'\1'*len(image))
        for f in fixups:
            if f.source_type!=7:raise ValueError('此只读映射只支持已证32bit LE fixup')
            if self.start<=f.source_addr_relbase<self.end:
                self.fixups.append(f);self.mask[f.source_addr_relbase:f.source_addr_relbase+4]=b'\0'*4

    @classmethod
    def original(cls,original):
        executable=(original/'FD2.EXE').read_bytes()
        if hashlib.sha256(executable).hexdigest()!=EXE_SHA:raise ValueError('原EXE不是固定参照')
        le=parse_le(executable);objects=parse_objects(executable,le)
        return cls(build_relbase_image(executable,le,objects),parse_fixups(executable,le,objects),objects)

    def matches(self,cva,actual,minimum=48):
        if not self.start<=cva or cva+len(actual)>self.end:return False
        flags=self.mask[cva:cva+len(actual)]
        return sum(flags)>=minimum and all(not flag or a==b for a,b,flag in zip(actual,self.image[cva:],flags))

    def locate_window(self,actual):
        if len(actual)!=WINDOW:raise ValueError('当前PC代码窗口长度不同')
        candidates=set()
        for index in range(WINDOW-7):
            seed=actual[index:index+8]
            if len(set(seed))<3:continue
            at=self.start;hits=0
            while True:
                at=self.image.find(seed,at,self.end)
                if at<0:break
                cva=at-index;hits+=1
                if hits>4096:raise ValueError('代码seed不够独立，未猜映射')
                if self.matches(cva,actual):candidates.add(cva)
                at+=1
        if len(candidates)!=1:raise ValueError('当前PC窗口没有唯一原码匹配：'+str(sorted(candidates)))
        return next(iter(candidates))

    def data_base(self,windows):
        proofs=[]
        for cva,actual in windows:
            if not self.matches(cva,actual):raise ValueError('两独立原码锚的非fixup字节不同')
            for f in self.fixups:
                offset=f.source_addr_relbase-cva
                if f.target_object==2 and 0<=offset<=len(actual)-4:
                    value=struct.unpack_from('<I',actual,offset)[0]
                    proofs.append({'source_cva':f.source_addr_relbase,'target_offset':f.target_offset,
                        'actual':value,'object2_DS_offset':value-f.target_offset})
        if len({p['source_cva'] for p in proofs})<2 or len({p['target_offset'] for p in proofs})<2 or len({p['object2_DS_offset'] for p in proofs})!=1:
            raise ValueError('object2缺少独立一致的合法fixup')
        base=proofs[0]['object2_DS_offset']
        if base<0:raise ValueError('object2逻辑基址为负')
        return base,proofs

    def validate_relocations(self,cva,actual,bases):
        for f in self.fixups:
            offset=f.source_addr_relbase-cva
            if 0<=offset<=len(actual)-4 and f.target_object in bases:
                if struct.unpack_from('<I',actual,offset)[0]!=bases[f.target_object]+f.target_offset:
                    raise ValueError('代码窗口完整fixup值与本次映射不同')


class Probe:
    """OwnedRPC只读窄接口；无通用dump、snapshot或任意EV表达式接口。"""
    def __init__(self,rpc,work,record):
        self.rpc=rpc;self.work=Path(work);self.record=record;self.descriptors={};self.bytes=0
        self.read_upper_bound=0;self.commands=0;self.deadline=time.monotonic()+PROBE_SECONDS
        self.record.update(reads=[],read_attempts=[],register_samples=[],requested_stack_read=False,whole_RAM_dump=False,
            separate_commands_are_not_atomic=True,mode_gate_source=MODE_SOURCE,
            same_command_mode_predicate='PE=1, VM=0, PG=0; descriptor/base/limit/RAM qualification remains separate',
            checked_read_failure_can_zero_fill=True,zero_file_does_not_prove_mode_or_successful_host_write=True,
            actual_guest_addresses_are_not_an_atomic_descriptor_snapshot=True,
            qualified_guest_bytes=0,guest_read_bytes_upper_bound=0,guest_read_bytes_limit=READ_LIMIT,
            command_count=0,command_limit=COMMAND_LIMIT,probe_seconds_limit=PROBE_SECONDS)

    def _exchange(self,command):
        if self.commands>=COMMAND_LIMIT or time.monotonic()>=self.deadline:
            raise ValueError('本页只读命令或时间预算耗尽；未追加读取')
        self.commands+=1;self.record['command_count']=self.commands
        rows=self.rpc.request(command)
        if time.monotonic()>=self.deadline:raise ValueError('本页只读时间预算耗尽；RPC自身超时仍另有限界')
        return rows

    def request(self,command):
        if command!=EV and not re.fullmatch(r'EXEC SELINFO [0-9A-F]{1,4}',command):
            raise ValueError('仅允许固定11寄存器EV及numeric SELINFO')
        return self._exchange(command)

    def registers(self):
        rows=self.request(EV);value=parse_registers(rows)
        self.record['register_samples'].append({'registers':value,'request_index':len(self.rpc.record['requests'])-1})
        return value

    def descriptor(self,selector):
        if type(selector) is not int or not 1<=selector<=0xffff:raise ValueError('numeric selector无效')
        rows=self.request(f'EXEC SELINFO {selector:X}')
        try:value=snapshot.parse_selector(rows)
        except ValueError as error:
            self.record.setdefault('invalid_selector_observations',[]).append({'selector':selector,'response':rows,'reason':str(error)})
            raise ContextNotClient('当前numeric SELINFO不能资格化；保留响应，不认定游戏差异') from error
        self.descriptors[selector]=value;return value

    def _read(self,selector,offset,size,label,*,code):
        value=self.descriptors.get(selector)
        if value is None or not descriptor_valid(value,code=code) or type(offset) is not int or offset<0 or offset+size-1>value['limit']:
            raise ValueError('只读窗口的selector/32bit类型/边界未资格化')
        if size not in (4,WINDOW) or code!=(size==WINDOW):
            raise ValueError('只读窗口长度不符')
        if not 0<=value['base']+offset<=RAM_LIMIT-size:raise ValueError('只读窗口超固定16MiB物理域')
        for _ in range(READ_ATTEMPTS):
            # 源不检查fwrite；零长仍可能是host写失败。按每次N保守限实际guest读取上界。
            if max(self.bytes,self.read_upper_bound)+size>READ_LIMIT:
                raise ValueError('本页2048B guest读取上界预算耗尽；未追加读取')
            path=self.work/f'read-{len(self.record["read_attempts"]):03d}-{label}.bin'
            if path.exists() or not str(path).isascii() or re.search(r'\s',str(path)):
                raise ValueError('只读转储需要新ASCII无空白隔离路径')
            command=f'EXEC MEMDUMPBIN {selector:X}:{offset:X} {size:X}*{MODE_FACTOR} {path}'
            self.read_upper_bound+=size;self.record['guest_read_bytes_upper_bound']=self.read_upper_bound
            rows=self._exchange(command)
            if not any('Memory dump binary success.' in row for row in rows):raise ValueError('有界dump未返回成功')
            data=path.read_bytes()
            attempt={'selector':selector,'offset':offset,'linear_from_separate_descriptor':value['base']+offset,
                'requested_size':size,'returned_size':len(data),'path':str(path),'sha256':sha(path),
                'role':label,'code':code,'command':command,'qualified_guest_bytes':0,
                'guest_read_bytes_upper_bound':size,'qualification':'unqualified_zero_length' if not data else 'pending'}
            self.record['read_attempts'].append(attempt)
            if not data:
                attempt['guest_read_bytes_if_mode_gate_refused']=0
                continue
            if len(data)!=size:
                attempt['qualification']='unexpected_length'
                raise ValueError('有界dump实际长度不同；未pad或fallback')
            self.bytes+=size;self.record['qualified_guest_bytes']=self.bytes
            attempt.update(qualification='mode_gate_allowed; descriptor/RAM and value oracle separate',qualified_guest_bytes=size)
            self.record['reads'].append({'selector':selector,'offset':offset,'linear':value['base']+offset,
                'size':size,'path':str(path),'sha256':sha(path),'role':label,'code':code,
                'same_command_mode_guard':True,'descriptor_identity_is_separate':True,
                'checked_read_success_not_returned_by_debugger':True})
            return data
        raise ContextNotClient('有限模式门尝试结束但未取得非零长合格读取；未猜数据')

    def pc_code(self,registers):
        return self._read(registers['CS'],registers['EIP'],WINDOW,'current-pc',code=True)

    def anchor(self,mapping,cva):
        if cva not in ANCHORS:raise ValueError('未登记代码锚')
        return self._read(mapping['CS'],mapping['code_linear']+cva-0x10000-self.descriptors[mapping['CS']]['base'],WINDOW,f'anchor-{cva:x}',code=True)

    def field(self,mapping,name):
        if name not in FIELDS:raise ValueError('未登记data字段；不读取原栈或单位内容')
        return struct.unpack('<I',self._read(mapping['DS'],mapping['data_DS']+FIELDS[name],4,name,code=False))[0]


def descriptors(probe,registers):
    if not protected(registers):raise ContextNotClient('寄存器样本不在普通protected context')
    values={key:probe.descriptor(registers[key]) for key in ('CS','DS','SS')}
    if not all(descriptor_valid(values[k],code=k=='CS') for k in values):raise ContextNotClient('当前CS/DS/SS不是已证32bit descriptor')
    if len({value['base'] for value in values.values()})!=1:
        raise ContextNotClient('当前CS/DS/SS不是同base的Watcom flat-client；不假定base为0')
    return values


def bind_image(probe,model,attempts=16):
    failures=[];probe.record['mapping_failures']=failures
    for _ in range(attempts):
        registers=probe.registers()
        if not protected(registers):failures.append('non-protected sample');time.sleep(.013);continue
        try:views=descriptors(probe,registers)
        except ContextNotClient as error:failures.append(str(error));time.sleep(.013);continue
        raw=probe.pc_code(registers)
        try:cva=model.locate_window(raw)
        except ValueError as error:failures.append(str(error));time.sleep(.013);continue
        code_linear=views['CS']['base']+registers['EIP']-(cva-0x10000)
        if not 0<=code_linear<=RAM_LIMIT-model.objects[1].vsize:raise ValueError('新object1物理范围不同')
        mapping={'CS':registers['CS'],'DS':registers['DS'],'SS':registers['SS'],'code_linear':code_linear,
            'seed_cva':cva,'seed_sha256':hashlib.sha256(raw).hexdigest(),'seed_registers':registers}
        windows=[(va,probe.anchor(mapping,va)) for va in ANCHORS]
        data,proofs=model.data_base(windows);mapping.update(data_DS=data,fixups=proofs,
            data_linear=views['DS']['base']+data,descriptor_before=views,failed_seed_observations=failures,
            object2_linear_range=[views['DS']['base']+data,views['DS']['base']+data+model.objects[2].vsize])
        bases={1:code_linear-views['DS']['base'],2:data}
        for va,block in windows:model.validate_relocations(va,block,bases)
        after=probe.registers();fresh=descriptors(probe,after)
        if any(after[k]!=registers[k] for k in ('CS','DS','SS')) or fresh!=views:raise ValueError('映射前后selector/descriptor改变')
        return mapping
    raise ValueError('有限PC窗口尝试内未取得唯一原码映射：'+str(failures))


def pool(probe,mapping,expected_count):
    pointer=probe.field(mapping,'active_pointer');count=probe.field(mapping,'active_count')
    ds=probe.descriptors[mapping['DS']];linear=ds['base']+pointer;end=linear+count*80
    if count!=expected_count or not pointer or pointer%4 or pointer+count*80-1>ds['limit'] or not 0<=linear<end<=RAM_LIMIT:
        raise ValueError('当前START active-prefix指针/人数/范围未资格化')
    return {'pointer_DS_offset':pointer,'count':count,'linear_start':linear,'linear_end':end,
        'active_prefix_only':True,'pool_or_stack_content_read':False,'object2_linear_range':mapping['object2_linear_range']}


def classify(registers,mapping,prefix):
    pc=mapping['descriptor_before']['CS']['base']+registers['EIP']-mapping['code_linear']+0x10000
    ds=mapping['descriptor_before']['DS']['base'];ss=mapping['descriptor_before']['SS']['base']
    inside=lambda address:prefix['linear_start']<=address<prefix['linear_end']
    ebp_ss=ss+registers['EBP'];object2=prefix['object2_linear_range']
    return {'canonical_pc':pc,'EBP_as_SS_linear':ebp_ss,'EBP_as_DS_linear':ds+registers['EBP'],
        'ESI_as_DS_linear':ds+registers['ESI'],'EBP_in_object2':object2[0]<=ebp_ss<object2[1],
        'EBP_as_DS_in_active_prefix':inside(ds+registers['EBP']),'ESI_in_active_prefix':inside(ds+registers['ESI']),
        'EBP_is_sign_extended_word':registers['EBP']==((registers['EBP']&0xffff) if not registers['EBP']&0x8000 else (registers['EBP']|0xffff0000)),
        'FS_GS_packed_value_conditional_only':registers['FS']|(registers['GS']<<16)}


def observe(capture,probe,model,kind,predicate,checkpoints,expected_count,*,samples=256,timeout=12):
    before_inputs=len(capture.trace)
    wait_state(capture,kind+'-before-observe',predicate,checkpoints)
    mapping=bind_image(probe,model);prefix=pool(probe,mapping,expected_count)
    result={'kind':kind,'mapping':mapping,'active_prefix':prefix,'samples':[],
        'first_comparison_hit_claimed':False,'atomic_stack_or_RNG_trace_claimed':False}
    probe.record['observation']=result;deadline=time.monotonic()+timeout;lo,hi=BODY[kind]
    for index in range(samples):
        if time.monotonic()>=deadline:break
        registers=probe.registers()
        if not protected(registers) or any(registers[k]!=mapping[k] for k in ('CS','DS','SS')):
            result['samples'].append({'registers':registers,'eligible':False,'reason':'context_changed'});continue
        detail=classify(registers,mapping,prefix);eligible=lo<=detail['canonical_pc']<hi
        result['samples'].append({'registers':registers,**detail,'eligible':eligible,
            'callee_input_check_sample':0x10620<=detail['canonical_pc']<0x10652})
        if sum(row.get('eligible',False) for row in result['samples'])>=3:break
        time.sleep((.005,.011,.017,.029)[index%4])
    wait_state(capture,kind+'-after-observe',predicate,checkpoints)
    fresh=descriptors(probe,probe.registers())
    if fresh!=mapping['descriptor_before'] or pool(probe,mapping,expected_count)!=prefix:
        raise ValueError('观察前后descriptor/active-prefix改变')
    for va in ANCHORS:
        actual=probe.anchor(mapping,va)
        if not model.matches(va,actual):raise ValueError('观察后独立代码锚改变')
        model.validate_relocations(va,actual,{1:mapping['code_linear']-fresh['DS']['base'],2:mapping['data_DS']})
    if len(capture.trace)!=before_inputs:raise ValueError('只读观察区间出现额外业务键')
    result['eligible_samples']=sum(row.get('eligible',False) for row in result['samples'])
    if not result['eligible_samples']:raise ValueError('有限观察结束但未采到目标UI本体PC；不签callee或首比较')
    result['ok']=True;return result


def qualify_reference(original,dosbox,config,bootstrap,port,command,environment):
    actual,_,_=quiet_game_command(command,environment);configs,overrides=command_configuration(actual)
    if configs!=[config.resolve()]:raise ValueError('观察启动仅允许本轮单一显式conf')
    reference=original_reference.build(original,dosbox,configs,overrides,bootstrap)
    declared,_=original_reference.parse_config(config.read_bytes())
    extras={'dosbox.mcp_server':str(port),'dosbox.quit warning':'false'}
    allowed=set(original_reference.load_json(original_reference.REFERENCE)['explicit_settings'])|set(extras)
    if set(declared)-allowed or any(declared.get(key)!=value for key,value in extras.items()):
        raise ValueError('本轮只允许精确RPC端口/owned关闭提示的额外配置')
    return reference,actual,{'settings':extras,'scope':'owned loopback read-only RPC and normal close only',
        'global_reference_rules_modified':False,'observer_scheduling_effect_possible':True}


def exercise(capture,probe_factory,model,prepared,titles,save,guard,record,samples,timeout):
    source,focus,assets,_,_,frames,_,plan=prepared
    points=record['checkpoints'];checkpoint=lambda name,predicate,**kw:wait_state(capture,name,predicate,points,**kw)
    wait_title(capture,titles[3][0]);capture.key('UP');capture.wait(titles[3][2]);capture.key('ENTER')
    checkpoint('initial-browse',RoundOracle(assets,focus).initial_browse,timeout=30)
    for index,(key,position) in enumerate(plan['safe_navigation']):
        capture.key(key);checkpoint('empty-step-'+str(index),RoundOracle(assets,tuple(position)).closed_browse)
    oracle=RoundOracle(assets,tuple(plan['safe_screen']));checkpoint('safe-empty-browse',oracle.closed_browse)
    target=auxiliary.FullFrameOracle(frames,assets['f1_palette_cycles']);capture.key('F1')
    observe(capture,probe_factory('auxiliary'),model,'auxiliary',target.match,points,source[META+1],samples=samples,timeout=timeout)
    require_source_unchanged(save,*guard);capture.key('ESC');checkpoint('auxiliary-closed',oracle.closed_browse)
    capture.key('ENTER');checkpoint('overview-parent',lambda image:oracle.menu(image,0))
    capture.key('UP');capture.key('ENTER');checkpoint('overview-secondary',lambda image:oracle.secondary(image,0,overview_available=True))
    templates=overview.overview_templates(record['original'],source,assets['palette']);capture.key('ENTER')
    observe(capture,probe_factory('overview'),model,'overview',lambda image:overview.match_overview(image,templates),points,source[META+1],samples=samples,timeout=timeout)
    require_source_unchanged(save,*guard);capture.key('ESC');checkpoint('overview-closed',oracle.closed_browse)


def run(args):
    owned_job_required();root=args.root.resolve();original=(root/'original_game').resolve();out=args.artifacts.resolve();validate_output(original,out)
    if not str(out).isascii() or re.search(r'\s',str(out)):raise ValueError('观察产物需要ASCII无空白路径')
    if not 1<=args.samples<=256 or not 0<args.timeout<=15:raise ValueError('观察采样必须有限：1..256项，0..15秒')
    originals=hashes(original);mtimes={str(p):p.stat().st_mtime_ns for p in original.rglob('*') if p.is_file()};sources=source_facts(root)
    guarded=[args.driver.resolve(),args.dosbox.resolve(),args.bootstrap.resolve(),original_reference.REFERENCE.resolve()]
    guards={str(p):{'sha256':sha(p),'mtime_ns':p.stat().st_mtime_ns} for p in guarded}
    check=lambda:hashes(original)==originals and {str(p):p.stat().st_mtime_ns for p in original.rglob('*') if p.is_file()}==mtimes and source_facts(root)==sources and all(sha(Path(p))==v['sha256'] and Path(p).stat().st_mtime_ns==v['mtime_ns'] for p,v in guards.items())
    prepared=auxiliary.prepare(original,original/'START.SAV');model=CodeModel.original(original);titles=title_templates(original/'FDOTHER.DAT')
    out.mkdir(parents=True,exist_ok=False);record={'schema':'fd2.f2.normal-clock-observation.v2','ok':False,'original':original,
        'processes':[],'inputs':[],'checkpoints':[],'rpc':{},'observations':{},'display_requirement':{},'source_before':sources,
        'original_before':originals,'guards':guards,'unaltered_START_copy':True,'natural_arrival_from_NewGame_claimed':False,
        'whole_F2_or_timing_equivalence_claimed':False,'requested_stack_read':False,'whole_RAM_dump':False,
        'mode_gate_source':MODE_SOURCE,'same_command_mode_guard':True,
        'actual_guest_address_scope':'code96 or two global DWORDs through separately qualified numeric descriptors; no EBP/ESP dereference requested; not an atomic descriptor/data snapshot',
        'checked_read_failure_can_zero_fill':True,
        'capture_backend':'Windows.Graphics.Capture HWND',
        'visual_scope':{'auxiliary':'full64000 source resource gate with inherited legal water cycles and RGB tolerance',
            'overview':'four complete source-resource rectangles; background/transition pixels excluded'},
        'register_and_WGC_observations_are_not_atomic':True,'observed_values_not_forced_to_hypothesis':True,
        'failed_qualification_is_not_proof_of_gameplay_difference':True}
    game=out/'game';save=game/'FD2.SAV';guard=None
    try:
        shutil.copytree(original,game);shutil.copy2(original/'START.SAV',save);guard=(save.read_bytes(),save.stat().st_mtime_ns)
        with windows_display_requirement(record['display_requirement'],require_display_on=True):
            try:
                with snapshot.OwnedRPC(record['rpc']) as rpc:
                    record['rpc'].update(all_business_fields_from_one_dump=False,register_observations_not_atomic=True)
                    config=out/'dosbox.conf';config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                        '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\nmcp_server='+str(rpc.port)+'\n'
                        '[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n'
                        '[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
                    guards[str(config)]={'sha256':sha(config),'mtime_ns':config.stat().st_mtime_ns}
                    env=os.environ.copy()
                    for name in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED'):env.pop(name,None)
                    command=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole']
                    reference,actual,extras=qualify_reference(original,args.dosbox,config,args.bootstrap,rpc.port,command,env)
                    record.update(reference=reference,additional_probe_settings=extras,expected_actual_command=actual)
                    for name,fact in reference['resources'].items():
                        copied=game/name
                        if sha(copied)!=fact['sha256'] or copied.stat().st_size!=fact['size']:
                            raise ValueError('实际挂载原资源副本身份不同：'+name)
                        guards[str(copied)]={'sha256':sha(copied),'mtime_ns':copied.stat().st_mtime_ns}
                    if not check():raise ValueError('首窗口前来源改变')
                    with running(command,out,env,out/'game.log',args.driver.resolve(),record['processes']) as process:
                        if record['processes'][-1]['command']!=actual:raise ValueError('实际argv与参照资格不同')
                        rpc.bind(process);capture=Capture(args.driver.resolve(),process,out,record['inputs'])
                        def make_probe(kind):
                            work=out/kind;work.mkdir();item={};record['observations'][kind]=item;return Probe(rpc,work,item)
                        try:exercise(capture,make_probe,model,prepared,titles,save,guard,record,args.samples,args.timeout)
                        except BaseException as error:
                            record['first_failure']={'checkpoint':record['checkpoints'][-1]['name'] if record['checkpoints'] else None,
                                'type':type(error).__name__,'text':str(error)}
                            raise
            finally:
                try:record['sources_after_owned_cleanup_unchanged']=check()
                except Exception as error:
                    record['sources_after_owned_cleanup_unchanged']=False
                    record['cleanup_guard_error']={'type':type(error).__name__,'text':str(error)}
    except BaseException as error:record['error']={'type':type(error).__name__,'text':str(error)}
    finally:
        record['original']=str(original)
        try:record['sources_unchanged']=check()
        except Exception as error:
            record['sources_unchanged']=False;record['final_guard_error']={'type':type(error).__name__,'text':str(error)}
        record['Save_bytes_and_mtime_unchanged']=bool(guard and save.is_file() and save.read_bytes()==guard[0] and save.stat().st_mtime_ns==guard[1])
        record['normal_owned_cleanup']=len(record['processes'])==1 and all(p.get('cleaned') and not p.get('forced_cleanup') and p.get('exit_code')==0 for p in record['processes'])
        record['owned_RPC_verified']=record['rpc'].get('tcp_owner_verified') is True and bool(record['processes']) and record['rpc'].get('pid')==record['processes'][0]['pid']
        if not record.get('error') and not all(record['observations'].get(k,{}).get('observation',{}).get('ok') for k in BODY):
            record['error']={'type':'ObservationIncomplete','text':'两页均须取得有限、合格的本体PC观察；未观察不签通过'}
        record['ok']=not record.get('error') and record['sources_unchanged'] and record.get('sources_after_owned_cleanup_unchanged') is True and record['Save_bytes_and_mtime_unchanged'] and record['normal_owned_cleanup'] and record['owned_RPC_verified'] and record['rpc'].get('closed') is True and record['display_requirement'].get('require_display_on') is True and record['display_requirement'].get('released') is True and all(record['observations'].get(k,{}).get('observation',{}).get('ok') for k in BODY)
        write(out/'report.json',record)
    return record


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for key in ('root','driver','dosbox','bootstrap','artifacts'):parser.add_argument('--'+key,type=Path,required=True)
    parser.add_argument('--samples',type=int,default=256);parser.add_argument('--timeout',type=float,default=12)
    args=parser.parse_args()
    if sys.platform!='win32':parser.error('此正常UI观察要求Windows WGC；未启动窗口')
    try:report=run(args)
    except (OSError,ValueError,RuntimeError) as error:parser.exit(1,'正常UI观察未能启动：'+str(error)+'。请核对来源或条件。\n')
    print(json.dumps({'ok':report['ok'],'report':str(args.artifacts/'report.json'),'error':report.get('error')},ensure_ascii=False));return 0 if report['ok'] else 1


if __name__=='__main__':raise SystemExit(main())
