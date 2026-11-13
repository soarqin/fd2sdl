"""S2只读snapshot薄适配：fresh numeric selector、有限pre-dump就绪等待。

实际dump/descriptor/owner及三whole预算仍复用S0；不读栈、不猜selector，
只将唯一EV的非PM/v86或完整三请求中已观察的16bit PE小DS列为未就绪。
"""
from __future__ import annotations
import json
from pathlib import Path
import time
import fd2_dosbox_snapshot as base
import fd2_s0_native as s0

CAPACITY_ERROR='RPC尚未在已证32bit protected DS容量内；未猜默认selector/base'
MODE_ERROR='S2 CPU当前不在protected非v86视图；仅EV未就绪'


def owner_view_status(registers,cs,ds):
    if not registers['CR0']&1 or registers['EFLAGS']&(1<<17) or not registers['CS'] or not registers['DS']:
        raise ValueError('S2 CPU就绪门遇到未登记real/v86或空selector')
    for descriptor,kind in ((cs,0x1a),(ds,0x12)):
        if descriptor['type']&~1!=kind or descriptor['dpl']!=0 or descriptor['flags'] not in ('10000','10010','10011') or \
                not 0<=descriptor['base']<=0xffffffff or not 0<descriptor['limit']<=0xffffffff:
            raise ValueError('S2 CPU就绪门遇到未知descriptor')
    small_code=cs['flags']=='10000' and cs['limit']<=0xffff and cs['base']+cs['limit']<base.RAM_SIZE and registers['EIP']<=cs['limit']
    # 实际final22：CS80为16bit代码而DS178为32bit数据；dump只借DS视图。
    if (cs['flags'][-2]=='1' or small_code) and ds['flags'][-2]=='1' and ds['limit']>=base.RAM_SIZE-1:
        return 'ready_32bit_protected_DS_capacity'
    # DS不是SS；不从完整ESP/EBP推断16bit栈，也不读取栈。
    if small_code and ds['flags']=='10000' and ds['limit']<=0xffff and ds['base']+ds['limit']<base.RAM_SIZE:
        return 'not_ready_16bit_protected_small_DS'
    raise ValueError('S2 CPU就绪门遇到未登记混合位宽/容量视图')


def not_ready_view(requests,error):
    if type(error) is not ValueError or any(r.get('ok') is not True for r in requests):return None
    if str(error)==MODE_ERROR and len(requests)==1:
        try:
            if requests[0].get('command')!='EXEC EV '+' '.join(base.REGISTERS):return None
            registers=base.parse_registers(requests[0]['response'])
        except (KeyError,TypeError,ValueError):return None
        if not registers['CR0']&1 or registers['EFLAGS']&(1<<17):
            return {'status':'not_ready_outside_protected_non_v86','registers':registers}
        return None
    if str(error)!=CAPACITY_ERROR or len(requests)!=3:return None
    try:
        registers=base.parse_registers(requests[0]['response'])
        expected=['EXEC EV '+' '.join(base.REGISTERS),f'EXEC SELINFO {registers["CS"]:X}',f'EXEC SELINFO {registers["DS"]:X}']
        if [r.get('command') for r in requests]!=expected:return None
        cs=base.parse_selector(requests[1]['response']);ds=base.parse_selector(requests[2]['response'])
        status=owner_view_status(registers,cs,ds)
    except (KeyError,TypeError,ValueError):return None
    if status!='not_ready_16bit_protected_small_DS':return None
    return {'status':status,'registers':registers,'CS_descriptor':cs,'DS_descriptor':ds}


class OwnedRPC(base.OwnedRPC):
    def __init__(self,*args,**kwargs):
        super().__init__(*args,**kwargs);self._deadline=None;self._readiness=None

    def _snapshot_registers_validate(self,registers):
        if not registers['CR0']&1 or registers['EFLAGS']&(1<<17):raise ValueError(MODE_ERROR)

    def _snapshot_view_validate(self,registers,cs,ds):
        if owner_view_status(registers,cs,ds)!='ready_32bit_protected_DS_capacity':raise ValueError(CAPACITY_ERROR)

    def request(self,command):
        if self._deadline is not None:
            remaining=self._deadline-time.monotonic()
            if remaining<=0:raise ValueError('S2 CPU就绪门5秒预算耗尽')
            self.connection.settimeout(min(remaining,self._old_timeout) if self._old_timeout is not None else remaining)
        return super().request(command)

    def snapshot(self,work,label,*,explicit_selectors=False):
        if self._readiness is None or explicit_selectors is not True:
            raise ValueError('S2 snapshot必须由共享numeric就绪窗口调用')
        gate=self._readiness
        while len(gate['samples'])<32:
            if time.monotonic()>=self._deadline:raise ValueError('S2 CPU就绪门5秒预算耗尽')
            self.snapshot_request_start=len(self.record['requests'])
            sample={'sample':len(gate['samples'])+1,'request_start':self.snapshot_request_start}
            gate['samples'].append(sample)
            try:
                result=super().snapshot(work,label,explicit_selectors=True)
                if time.monotonic()>self._deadline:raise ValueError('S2 snapshot超过5秒截止；不接受过期结果')
                sample['classification']='numeric_snapshot_returned';return result
            except ValueError as error:
                classified=not_ready_view(self.record['requests'][self.snapshot_request_start:],error)
                sample.update(error={'type':type(error).__name__,'message':str(error)},classification=classified)
                if classified is None:raise
            finally:
                sample.update(requests=self.record['requests'][self.snapshot_request_start:],elapsed_seconds=time.monotonic()-self._started)
                (self._directory/f'view-sample-{sample["sample"]:02d}.json').write_text(json.dumps(sample,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
        raise ValueError('S2 CPU就绪门32组预算耗尽；不启动新dump')


def original_snapshot(rpc,work,label,record,*,owner_decoder=None,explicit_selectors=False):
    if not isinstance(rpc,OwnedRPC) or explicit_selectors is not True:raise ValueError('S2读取须显式启用numeric selector适配')
    rpc._directory=Path(work);rpc._started=time.monotonic();rpc._deadline=rpc._started+5
    rpc._old_timeout=rpc.connection.gettimeout()
    gate={'maximum_seconds':5,'maximum_samples':32,'samples':[],
        'numeric_selectors_from_each_before_EV':True,'old_ready_tuple_reused':False,
        'stack_or_SS_inferred':False,'S0_whole_attempt_limit':3}
    rpc._readiness=gate;record['CPU_view_readiness']=gate
    try:
        result=s0.original_snapshot(rpc,work,label,record,owner_decoder=owner_decoder,explicit_selectors=True)
        gate['owner_validated']=owner_decoder is not None;return result
    except BaseException as error:
        gate['error']={'type':type(error).__name__,'message':str(error)};raise
    finally:
        rpc._deadline=None;rpc._readiness=None;rpc.connection.settimeout(rpc._old_timeout)
        (Path(work)/'view-readiness.json').write_text(json.dumps(gate,ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
