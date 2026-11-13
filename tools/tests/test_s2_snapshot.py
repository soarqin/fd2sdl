"""S2 numeric tuple与有限就绪适配；全mock，无进程/TCP/Win32/原资源。"""
import ast
import copy
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import fd2_s2_snapshot as s2


def view(small=False,**extras):
    r={'CS':0x88 if small else 0x208,'DS':0xa0 if small else 0x210,'EIP':0xdbd,'ESP':0x6446,'EBP':2057290,'CR0':0x11,'EFLAGS':2}
    cs={'base':0x174820 if small else 0,'limit':0x5e3f if small else 0xffffffff,'type':0x1a,'dpl':0,'flags':'10000' if small else '10011'}
    ds={'base':0x186330 if small else 0,'limit':0x7cff if small else 0xffffffff,'type':0x12,'dpl':0,'flags':'10000' if small else '10011'}
    return {'r':r,'cs':cs,'ds':ds,**extras}


def descriptor(d):return [f"b:{d['base']:08X} type:{d['type']:02X} l:{d['limit']:08X} dpl : {d['dpl']} {d['flags']}"]


class Script:
    def __init__(self,views,clock=None):self.views=views;self.current=None;self.dumped=False;self.index=0;self.clock=clock
    def request(self,rpc,command):
        if command.startswith('EXEC EV '):
            if self.dumped:
                r=self.current.get('after',self.current['r']);self.after=True
            else:
                self.current=self.views[self.index];self.index+=1;r=self.current['r'];self.after=False
            response=[' '.join(f'{r[k]:X}' for k in s2.base.REGISTERS)]
        elif command.startswith('EXEC SELINFO '):
            selector=command.split()[-1]
            if selector in ('CS','DS'):
                d=self.current.get('dynamic_'+selector.lower(),self.current[selector.lower()])
            elif selector==f'{self.current["r"]["CS"]:X}' and not self.after:d=self.current['cs']
            else:d=self.current.get('after_ds',self.current['ds']) if self.after else self.current['ds']
            response=descriptor(d)
            if self.after:
                self.dumped=False
                if self.current.get('late') and self.clock is not None:self.clock[0]=6
        elif command.startswith('EXEC MEMDUMPBIN '):
            Path(command.split(' ',4)[4]).write_bytes(self.current.get('payload',b'A'*s2.base.RAM_SIZE))
            self.dumped=True;response=['Memory dump binary success.']
        else:raise AssertionError(command)
        rpc.record['requests'].append({'command':command,'response':response,'ok':True})
        return response


class S2SnapshotTests(unittest.TestCase):
    def rpc(self,cls=s2.OwnedRPC):
        rpc=cls({'requests':[]});rpc.timeouts=[]
        rpc.connection=SimpleNamespace(gettimeout=lambda:20,settimeout=rpc.timeouts.append)
        return rpc

    def execute(self,views,*,decoder=None,clock=None):
        clock=[0] if clock is None else clock;rpc=self.rpc();script=Script(views,clock);record={}
        directory=tempfile.TemporaryDirectory();self.addCleanup(directory.cleanup)
        with patch.object(s2.base.OwnedRPC,'request',new=lambda rpc,c:script.request(rpc,c)),patch.object(s2.time,'monotonic',side_effect=lambda:clock[0]):
            try:result=s2.original_snapshot(rpc,Path(directory.name),'active',record,owner_decoder=decoder,explicit_selectors=True)
            except BaseException as error:result=error
        return result,rpc,record,Path(directory.name)

    def test_default_keeps_dynamic_six_requests_and_late_race_rejection(self):
        for racing in (False,True):
            v=view(racing)
            if racing:
                v.update(dynamic_cs=view()['cs'],dynamic_ds=view()['ds'])
            rpc=self.rpc(s2.base.OwnedRPC);script=Script([v])
            with tempfile.TemporaryDirectory() as directory,patch.object(s2.base.OwnedRPC,'request',new=lambda r,c:script.request(r,c)):
                if racing:
                    with self.assertRaisesRegex(ValueError,'descriptor改变'):rpc.snapshot(directory,'active')
                else:rpc.snapshot(directory,'active')
            commands=[r['command'] for r in rpc.record['requests']]
            self.assertEqual(len(commands),6);self.assertEqual(commands[1:3],['EXEC SELINFO CS','EXEC SELINFO DS'])
            self.assertEqual(commands[-1],f'EXEC SELINFO {v["r"]["DS"]:X}')
            self.assertTrue(commands[3].startswith(f'EXEC MEMDUMPBIN {v["r"]["DS"]:X}:0 '))

    def test_numeric_small_view_has_no_dump_then_fresh_tuple_is_used(self):
        small=view(True);small.update(dynamic_cs=view()['cs'],dynamic_ds=view()['ds'])
        wide=view(after=view(True)['r'])
        result,rpc,record,directory=self.execute([small,wide],decoder=lambda data,receipt:len(data))
        self.assertNotIsInstance(result,Exception);self.assertEqual(record['validated_owner'],s2.base.RAM_SIZE)
        commands=[r['command'] for r in rpc.record['requests']]
        self.assertEqual(commands[1:3],['EXEC SELINFO 88','EXEC SELINFO A0'])
        self.assertEqual(commands[4:6],['EXEC SELINFO 208','EXEC SELINFO 210'])
        self.assertEqual(sum('MEMDUMP' in c for c in commands),1)
        self.assertTrue(commands[6].startswith('EXEC MEMDUMPBIN 210:0 '));self.assertEqual(commands[-1],'EXEC SELINFO 210')
        self.assertEqual(len(record['CPU_view_readiness']['samples']),2)
        self.assertTrue((directory/'view-sample-01.json').is_file())
        self.assertEqual(rpc.timeouts[-1],20)

    def test_observed_16bit_code_32bit_DS_is_ready_without_stack_inference(self):
        v=view(True);v['ds']=view()['ds']
        v['r'].update(CS=0x80,DS=0x178,EIP=0x550,ESP=0x1f1622,EBP=0x1f1630,EFLAGS=7)
        self.assertEqual(s2.owner_view_status(v['r'],v['cs'],v['ds']),'ready_32bit_protected_DS_capacity')
        result,rpc,record,_=self.execute([v],decoder=lambda data,receipt:receipt['view_selector'])
        self.assertNotIsInstance(result,Exception);self.assertEqual(record['validated_owner'],0x178)
        self.assertFalse(record['CPU_view_readiness']['stack_or_SS_inferred'])
        self.assertEqual([r['command'] for r in rpc.record['requests']][1:3],['EXEC SELINFO 80','EXEC SELINFO 178'])

    def test_accessed_bit_preserves_observed_type_and_all_other_guards(self):
        small=view(True)
        small['r'].update(CS=0x70,DS=0x20,EIP=0xce8,ESP=0x6446,EBP=0x1f644a,EFLAGS=6)
        small['cs'].update(base=0x18e030,limit=0x57cf,type=0x1b)
        small['ds'].update(base=0x8240,limit=0xffff,type=0x13)
        wide=view();wide['cs']['type']=0x1b;wide['ds']['type']=0x13
        self.assertEqual(s2.owner_view_status(small['r'],small['cs'],small['ds']),'not_ready_16bit_protected_small_DS')
        self.assertEqual(s2.owner_view_status(wide['r'],wide['cs'],wide['ds']),'ready_32bit_protected_DS_capacity')
        result,rpc,record,_=self.execute([small,wide])
        self.assertNotIsInstance(result,Exception)
        first=record['CPU_view_readiness']['samples'][0]
        self.assertEqual(first['classification']['CS_descriptor']['type'],0x1b)
        self.assertEqual(first['classification']['DS_descriptor']['type'],0x13)
        self.assertEqual(len(first['requests']),3)
        self.assertEqual(sum('MEMDUMP' in r['command'] for r in rpc.record['requests']),1)
        changed=copy.deepcopy(wide);changed['after_ds']={**wide['ds'],'type':0x12}
        result,_,record,_=self.execute([changed])
        self.assertIsInstance(result,ValueError)
        self.assertFalse(record['rpc_snapshot_attempts'][0]['retry_scheduled'])
        for slot,kind in (('cs',0x18),('cs',0x19),('cs',0x1c),('cs',0x1d),('ds',0x10),('ds',0x11),('ds',0x16),('ds',0x17)):
            bad=copy.deepcopy(small);bad[slot]['type']=kind
            with self.subTest(slot=slot,kind=kind):
                with self.assertRaisesRegex(ValueError,'未知descriptor'):
                    s2.owner_view_status(bad['r'],bad['cs'],bad['ds'])

    def test_unknown_protected_descriptors_are_not_readiness_retries(self):
        for kind in ('type','flags','mixed','short32'):
            v=view(True)
            if kind=='type':v['ds']['type']=0x10
            if kind=='flags':v['ds']['flags']='00000'
            if kind=='mixed':v['cs']=view()['cs']  # 逆混合32CS/小16DS仍拒绝。
            if kind=='short32':v['cs']['flags']=v['ds']['flags']='10010'
            with self.subTest(kind=kind):
                result,rpc,record,_=self.execute([v]);self.assertIsInstance(result,ValueError)
                self.assertEqual(len(rpc.record['requests']),3);self.assertEqual(len(record['rpc_snapshot_attempts']),1)
                self.assertFalse(record['rpc_snapshot_attempts'][0]['retry_scheduled'])

    def test_exact_single_EV_real_or_v86_waits_without_descriptor_or_dump(self):
        for cr0,flags in ((0x10,2),(0x11,0x20202)):
            v=view();v['r'].update(CS=0xf71,DS=0xf71,EIP=0x2aeb,ESP=0x1e2182,EBP=0x21c2,CR0=cr0,EFLAGS=flags)
            result,rpc,record,_=self.execute([v,view()])
            self.assertNotIsInstance(result,Exception)
            first=record['CPU_view_readiness']['samples'][0]
            self.assertEqual(len(first['requests']),1)
            self.assertEqual(first['classification']['status'],'not_ready_outside_protected_non_v86')
            self.assertFalse(any('SELINFO F71' in row['command'] for row in rpc.record['requests']))
        ev='EXEC EV '+' '.join(s2.base.REGISTERS)
        good={'command':ev,'response':['208 210 dbd 6446 1f6446 11 2'],'ok':True}
        self.assertIsNone(s2.not_ready_view([good],ValueError(s2.MODE_ERROR)))
        real={**good,'response':['f71 f71 2aeb 1e2182 21c2 10 2']}
        for rows,error in (([],ValueError(s2.MODE_ERROR)),([real,real],ValueError(s2.MODE_ERROR)),
            ([{**real,'ok':False}],ValueError(s2.MODE_ERROR)),([{**real,'command':'EXEC EV CS'}],ValueError(s2.MODE_ERROR)),
            ([{**real,'response':['bad']}],ValueError(s2.MODE_ERROR)),([real],ValueError('unrelated'))):
            self.assertIsNone(s2.not_ready_view(rows,error))

    def test_single_EV_not_ready_also_exhausts_shared_32_sample_budget(self):
        v=view();v['r']['CR0']=0x10
        result,rpc,record,_=self.execute([copy.deepcopy(v) for _ in range(32)])
        self.assertIsInstance(result,ValueError);self.assertEqual(len(rpc.record['requests']),32)
        self.assertEqual(len(record['CPU_view_readiness']['samples']),32)
        self.assertEqual(len(record['rpc_snapshot_attempts']),1)
        self.assertFalse(record['rpc_snapshot_attempts'][0]['retry_scheduled'])

    def test_numeric_mode_transition_retries_exact_six_after_readiness_prefix(self):
        v=view(after={**view()['r'],'CR0':0x10})
        result,rpc,record,_=self.execute([view(True),v,view()])
        self.assertNotIsInstance(result,Exception)
        first=record['rpc_snapshot_attempts'][0]
        self.assertTrue(first['numeric_retry_window']['exact_six']);self.assertEqual(first['numeric_retry_window']['request_count'],6)
        self.assertEqual(first['retry_classification']['reason'],'guest_left_protected_mode_after_dump')
        self.assertEqual(len(record['rpc_snapshot_attempts']),2)
        v=view(after={**view()['r'],'CR0':0x10})
        result,_,record,_=self.execute([copy.deepcopy(v) for _ in range(3)])
        self.assertIsInstance(result,ValueError);self.assertEqual(len(record['rpc_snapshot_attempts']),3)
        self.assertFalse(record['rpc_snapshot_attempts'][-1]['retry_scheduled'])

    def test_descriptor_owner_short_and_late_errors_do_not_retry(self):
        cases=[(view(after_ds={**view()['ds'],'base':0x1000}),None),
               (view(payload=b'short'),None),(view(late=True),None),
               (view(),lambda data,receipt:(_ for _ in ()).throw(ValueError('fixup错误')))]
        for v,decoder in cases:
            result,rpc,record,_=self.execute([v],decoder=decoder)
            self.assertIsInstance(result,ValueError);self.assertEqual(len(record['rpc_snapshot_attempts']),1)
            self.assertFalse(record['rpc_snapshot_attempts'][0]['retry_scheduled']);self.assertEqual(rpc.timeouts[-1],20)

    def test_numeric_ring_requires_full_fresh_dump_and_exact_request_window(self):
        ring=(b'\0\1'+bytes(65534))*256;fresh=ring[:-1]+b'\2'
        def decode(data,receipt):
            if data==ring:raise ValueError('原码anchor在本轮snapshot没有唯一match：source')
            return True
        result,_,record,_=self.execute([view(True),view(payload=ring),view(payload=fresh)],decoder=decode)
        self.assertNotIsInstance(result,Exception);self.assertTrue(record['validated_owner'])
        self.assertEqual(record['rpc_snapshot_attempts'][0]['retry_classification']['reason'],'exact_16bit_segment_offset_wrap')
        requests=[]
        self.assertFalse(s2.s0.snapshot_requests_match(requests,explicit_selectors=True))
        first=record['rpc_snapshot_attempts'][0]
        import json
        requests=json.loads(Path(first['requests']['path']).read_text())[3:]
        self.assertTrue(s2.s0.snapshot_requests_match(requests,explicit_selectors=True))
        for index in (1,2,3,5):
            bad=copy.deepcopy(requests);bad[index]['command']='EXEC SELINFO 178'
            self.assertFalse(s2.s0.snapshot_requests_match(bad,explicit_selectors=True))

    def test_readiness_has_one_shared_5second_32sample_budget(self):
        result,rpc,record,_=self.execute([view(True) for _ in range(32)])
        self.assertIsInstance(result,ValueError);self.assertEqual(len(record['CPU_view_readiness']['samples']),32)
        self.assertEqual(len(rpc.record['requests']),96)
        self.assertFalse(any('MEMDUMP' in r['command'] for r in rpc.record['requests']))
        clock=[0];v=view(True)
        class Slow(Script):
            def request(self,rpc,command):
                result=super().request(rpc,command)
                if command.startswith('EXEC SELINFO A0'):clock[0]=5
                return result
        rpc=self.rpc();script=Slow([v],clock);record={}
        with tempfile.TemporaryDirectory() as directory,patch.object(s2.base.OwnedRPC,'request',new=lambda r,c:script.request(r,c)),patch.object(s2.time,'monotonic',side_effect=lambda:clock[0]):
            with self.assertRaisesRegex(ValueError,'耗尽'):s2.original_snapshot(rpc,Path(directory),'active',record,explicit_selectors=True)
        self.assertEqual(len(rpc.record['requests']),3);self.assertEqual(rpc.timeouts[-1],20)

    def test_three_S2_wrappers_explicitly_use_shared_adapter(self):
        for name in ('result','ticks','layout'):
            tree=ast.parse((ROOT/f'tools/run_s2_{name}_check.py').read_text(encoding='utf-8'))
            calls=[n for n in ast.walk(tree) if isinstance(n,ast.Call) and isinstance(n.func,ast.Attribute) and n.func.attr=='original_snapshot']
            self.assertEqual(len(calls),1);self.assertEqual(calls[0].func.value.id,'s2_snapshot')
            self.assertTrue(any(k.arg=='explicit_selectors' and k.value.value is True for k in calls[0].keywords))


if __name__=='__main__':unittest.main()
