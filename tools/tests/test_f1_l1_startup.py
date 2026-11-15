"""L1有源启动账：合成描述符/输入轨迹；不分发loader或运行DOSBox。"""
import copy
from pathlib import Path
import subprocess
import struct
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock,patch

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_le_preentry_check as runner
import verify_le_startup_writes as verifier

def fixture(shift=0):
    memory=bytearray(0x20000);gdt={'base':0x100,'limit':0x300}
    sels={name:value+shift for name,value in {'ss':0x20,'ds':0x28,'es':0x30,'stack':0x38,'module':0x40,'loader':0x48,'transfer':0x50,'entry':0x58}.items()}
    def desc(selector,base,bits=16,kind=0x12):
        limit=0xffff;low=(limit&65535)|((base&65535)<<16)
        high=(base>>16&255)|((kind|0x80)<<8)|(limit>>16&15)<<16|(base&0xff000000)
        if bits==32:high|=0x400000
        struct.pack_into('<II',memory,gdt['base']+selector,low,high)
    parameter=0xd690;bp=0x800;local=0x1000+bp-0x5c;context=0x100;entry=0x4040
    for name,base,bits,kind in (('ss',0x1000,16,0x12),('ds',0x2000,16,0x12),('es',parameter,16,0x12),
        ('stack',0,32,0x12),('module',0x3000,16,0x12),('loader',0x11000,16,0x1a),('transfer',0x12000,16,0x1a),('entry',0,32,0x1a)):
        desc(sels[name],base,bits,kind)
    frame=bytearray(62)
    struct.pack_into('<HHHH',frame,0,0,sels['stack'],sels['stack'],0x28)
    struct.pack_into('<I',frame,0x18,parameter);struct.pack_into('<I',frame,0x32,entry)
    struct.pack_into('<H',frame,0x36,sels['entry']);struct.pack_into('<I',frame,0x3a,0x200)
    memory[local:local+62]=frame
    struct.pack_into('<HH',memory,0x1000+bp+0xe,context,sels['ds'])
    struct.pack_into('<HH',memory,0x2000+0x299a,0,sels['module'])
    struct.pack_into('<II',memory,0x3000+0x34,0x11121314,0x01020304)
    regs={key:0 for key in ('EAX','EBX','ECX','EDX','ESI','EDI','EBP','ESP','DS','ES','FS','GS','SS','FLG','CR0')}
    regs.update(DS=sels['ds'],ES=sels['es'],SS=sels['ss'],EBP=bp,CR0=1)
    initial={**regs,'CS':sels['loader'],'EIP':0x3966,'EFLAGS':2};initial.pop('FLG')
    rows=[]
    def row(cs,pc,code='90',**updates):
        rows.append({'index':len(rows),'CS':cs,'EIP':pc,'raw_hex':code,'registers':{**regs,**updates}})
    for pc,value in ((0x3966,0x304),(0x396a,0x102),(0x397d,0x1314),(0x3981,0x1112)):
        row(sels['loader'],pc,**{'EAX' if pc in (0x3966,0x397d) else 'EDX':value})
    for count in (31,19,5):
        row(sels['loader'],0xb0af,'f3a5',DS=sels['ss'],ES=sels['ds'],ECX=count,ESI=bp-0x5c+(31-count)*2,EDI=context+(31-count)*2)
    row(sels['loader'],0xb0b1,ECX=0,ESI=bp-0x5c+62,EDI=context+62)
    for pc,offset,value in ((0x3444,0x10,sels['entry']),(0x3448,0x12,0),(0x6da,0x3a,0x200),(0x6e0,0x3c,0)):
        row(sels['loader'],pc,('c706'+struct.pack('<HH',context+offset,value).hex()) if pc in (0x6da,0x6e0) else '90',
            ES=sels['ds'],EBX=context,EAX=sels['entry'])
    for count in (14,10,6,2):
        row(sels['transfer'],0x6b0,'67f366a5',DS=sels['ds'],ES=sels['stack'],ECX=count,ESI=context+4+(14-count)*4,EDI=parameter-58+(14-count)*4)
    row(sels['transfer'],0x6b4,ECX=0,EDI=parameter-2,ESI=context+60)
    row(sels['transfer'],0x6b6,'67a5',DS=sels['ds'],ES=sels['stack'],ECX=0,EDI=parameter-2,ESI=context+60)
    row(sels['transfer'],0x6b8,EDI=parameter)
    row(sels['entry'],entry)
    objects=[SimpleNamespace(index=1,vsize=0x1000),SimpleNamespace(index=2,vsize=0x6000),SimpleNamespace(index=3,vsize=0x1000)]
    bases={1:0x4000,2:0x8000,3:0x10000}
    roles={role:{'selector':sels[role]} for role in ('loader','transfer')}
    return memory,rows,{'start_registers':initial},roles,gdt,objects,bases,entry

class LayeredWrites(unittest.TestCase):
    def without(self,pc):
        args=list(fixture());args[1]=[r for r in args[1] if r['EIP']!=pc]
        for index,row in enumerate(args[1]):row['index']=index
        return args
    def make(self):
        args=fixture();writes,source=verifier.predict_writes(*args)
        actual=bytearray(args[0])
        for write in writes:actual[write['linear']:write['linear']+write['width']]=bytes.fromhex(write['bytes'])
        return args,writes,source,actual
    def test_complete_ledger_writes_66_bytes_including_zero_WORD(self):
        args,writes,source,actual=self.make()
        self.assertEqual(sum(w['width'] for w in writes),66);self.assertEqual(writes[-1]['bytes'],'0000')
        self.assertEqual(source['predicted_entry_ESP'],0xd690)
        verifier.compare_objects(actual,actual,args[5],args[6])
    def test_selector_roles_are_not_previous_run_numbers(self):
        for shift in (0,0x80):
            writes,source=verifier.predict_writes(*fixture(shift))
            self.assertEqual(writes[0]['selector'],0x48+shift)
            self.assertEqual(source['saved_entry_CS'],0x58+shift)
    def test_negative_omit_pointer_high_WORD(self):
        args=self.without(0x396a)
        with self.assertRaisesRegex(ValueError,'恰好执行一次'):verifier.predict_writes(*args)
    def test_negative_omit_saved_ESI_override(self):
        args=self.without(0x3444)
        with self.assertRaisesRegex(ValueError,'恰好执行一次'):verifier.predict_writes(*args)
    def test_negative_change_actual_L0_source_EIP(self):
        args=list(fixture());local=0x1000+0x800-0x5c;args[0][local+0x32]^=1
        with self.assertRaisesRegex(ValueError,'保存EIP'):verifier.predict_writes(*args)
    def test_negative_omit_zero_tail_WORD(self):
        args=self.without(0x6b6)
        with self.assertRaisesRegex(ValueError,'恰好执行一次'):verifier.predict_writes(*args)
    def test_negative_outside_ledger_BSS(self):
        args,writes,source,actual=self.make();damaged=bytearray(actual);damaged[args[6][2]+0x5000]^=1
        with self.assertRaisesRegex(ValueError,'完整对象字节不同'):verifier.compare_objects(actual,damaged,args[5],args[6])
    def test_negative_file_byte(self):
        args,writes,source,actual=self.make();damaged=bytearray(actual);damaged[args[6][1]+0x40]^=1
        with self.assertRaisesRegex(ValueError,'完整对象字节不同'):verifier.compare_objects(actual,damaged,args[5],args[6])
    def test_negative_fixup_byte(self):
        args,writes,source,actual=self.make();expected=bytearray(actual)
        struct.pack_into('<I',expected,args[6][1]+0x44,0x12345678);damaged=bytearray(expected);damaged[args[6][1]+0x44]^=1
        with self.assertRaisesRegex(ValueError,'完整对象字节不同'):verifier.compare_objects(expected,damaged,args[5],args[6])
    def test_reverse_copy_or_wrong_execution_order_rejected(self):
        args=list(fixture());copy_row=next(r for r in args[1] if r['EIP']==0xb0af);copy_row['registers']['FLG']|=0x400
        with self.assertRaisesRegex(ValueError,'方向位'):verifier.predict_writes(*args)
        args=list(fixture());next(r for r in args[1] if r['EIP']==0x3444)['index']=0
        with self.assertRaisesRegex(ValueError,'写序'):verifier.predict_writes(*args)
    def test_log_does_not_consume_disassembly_immediate_as_code(self):
        registers=' '.join(f'{k}:00000000' for k in ('EAX','EBX','ECX','EDX','ESI','EDI','EBP','ESP','DS','ES','FS','GS','SS','FLG','CR0'))
        raw=('0 0088:0000B06E enter 0,00  C8 00 00 00  '+registers+'\n')*2
        self.assertEqual(verifier.parse_log(raw.encode())[0]['raw_hex'],'c8000000')
        with self.assertRaises(ValueError):verifier.parse_log(raw.replace(' CR0:00000000','').encode())
    def test_source_second_read_cannot_replace_original_identity(self):
        with tempfile.TemporaryDirectory() as tmp:
            path=Path(tmp)/'source';path.write_bytes(b'A');inputs={};verifier.snapshot(path,inputs);path.write_bytes(b'B')
            with self.assertRaisesRegex(ValueError,'两次读取间改变'):verifier.snapshot(path,inputs)
            with self.assertRaisesRegex(ValueError,'晚守卫'):verifier.late_guard(inputs)
    def test_descriptor_uses_same_L0_raw_GDT_not_stale_text(self):
        args=fixture();memory,gdt=args[0],args[4]
        self.assertEqual(verifier.descriptor(memory,gdt,0x30)['base'],0xd690)
        for selector in (0,0x34,0x308):
            with self.assertRaises(ValueError):verifier.descriptor(memory,gdt,selector)
    def test_first_and_last_PC_registers_flags_and_execution_domain_are_joined(self):
        args=fixture();rows=[copy.deepcopy(args[1][0]),copy.deepcopy(args[1][-1])]
        def state(row):return {**row['registers'],'CS':row['CS'],'EIP':row['EIP'],'EFLAGS':row['registers']['FLG']}
        a={'before':state(rows[0]),'after':state(rows[0])};b={'before':state(rows[-1]),'after':state(rows[-1])}
        trace={'start_registers':a['before'],'stop_registers':b['before'],'trace_instructions':2}
        verifier.check_endpoints(rows,trace,a,b)
        for name in ('EAX','EDI','CR0','FLG'):
            bad=copy.deepcopy(rows);bad[-1]['registers'][name]^=1
            with self.subTest(name=name),self.assertRaisesRegex(ValueError,'起止寄存器'):verifier.check_endpoints(bad,trace,a,b)
        bad=copy.deepcopy(rows);bad[0]['EIP']+=1
        with self.assertRaisesRegex(ValueError,'起止PC'):verifier.check_endpoints(bad,trace,a,b)
    def test_output_always_protects_actual_original_even_without_reference(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);(root/'build').mkdir();original=root/'original_game';original.mkdir()
            with patch.object(verifier,'ROOT',root):
                with self.assertRaises(ValueError):verifier.new_output(original/'forbidden',{})
                if sys.platform=='win32':
                    alias=root/'build'/'alias'
                    subprocess.run(['cmd','/c','mklink','/J',str(alias),str(original)],capture_output=True,check=True)
                    with self.assertRaises(ValueError):verifier.new_output(alias/'forbidden',{})
                self.assertEqual(verifier.new_output(root/'build'/'new',{}),root/'build'/'new')
            self.assertEqual(list(original.iterdir()),[])
    def test_actual_original_is_protected_when_build_itself_is_alias(self):
        if sys.platform!='win32':self.skipTest('Windows junction boundary')
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);original=root/'original_game';original.mkdir()
            subprocess.run(['cmd','/c','mklink','/J',str(root/'build'),str(original)],capture_output=True,check=True)
            with patch.object(verifier,'ROOT',root),self.assertRaisesRegex(ValueError,'原资源'):
                verifier.new_output(root/'build'/'forbidden',{})
            self.assertEqual(list(original.iterdir()),[])
    def test_domain_is_bound_to_capture_complete_windows_and_actual_L0_bytes(self):
        memory=bytes(0x20000);roles={'loader':{'selector':0x48,'base':0},'transfer':{'selector':0x50,'base':0x10000}}
        domain={'schema':'fd2-le-startup-Domain-v1','database_saved':False,'capture_sha256':'capture','windows':[]}
        for role,module in roles.items():
            for start,end,wanted in verifier.WINDOWS[role]:
                domain['windows'].append({'selector':module['selector'],'window':[start,end],'base_from_L0_RAM_GDT':module['base'],
                    'bitness':16,'instructions':[{'cs_offset':at,'size':1,'raw_hex':'00'} for at in range(start,end)]})
        rows=[{'CS':0x48,'EIP':0x3940,'raw_hex':'00'}]
        self.assertEqual(verifier.domain_join(memory,rows,domain,roles,'capture'),1)
        for change in ('capture','missing','raw','bitness','duplicate'):
            bad=copy.deepcopy(domain)
            if change=='capture':bad['capture_sha256']='wrong'
            if change=='missing':bad['windows'].pop()
            if change=='raw':bad['windows'][0]['instructions'][0]['raw_hex']='01'
            if change=='bitness':bad['windows'][0]['bitness']=32
            if change=='duplicate':bad['windows'].append(bad['windows'][0])
            with self.subTest(change=change),self.assertRaises(ValueError):verifier.domain_join(memory,rows,bad,roles,'capture')

class TraceCapture(unittest.TestCase):
    def trace_fixture(self,out,corrupt=False):
        start={name:0 for name in runner.TRACE_REGS};start.update(CS=0x48,EIP=0x3966,DS=0x28,ES=0x30,SS=0x20)
        point={**start,'CS':0x58,'EIP':0x4040}
        def line(registers):
            names=('EAX','EBX','ECX','EDX','ESI','EDI','EBP','ESP','DS','ES','SS')
            values=' '.join(f'{name}:{registers[name]:08X}' for name in names)
            return f'0 {registers["CS"]:04X}:{registers["EIP"]:08X} nop  90  '+values+'\n'
        raw=line(start)+line(point)
        if corrupt:raw=raw.rsplit('ESI:00000000',1)[0]+'ESI:00000001'+raw.rsplit('ESI:00000000',1)[1]
        (out/'LOGCPU.TXT').write_text(raw,encoding='ascii')
        rpc=Mock();rpc.regs.side_effect=[start,point,point];rpc.request.return_value=['fixture']
        report={'loaded_validation':{'passed':True},'loaded_snapshot':{'before':start}}
        return rpc,report,point
    def test_successful_trace_requires_stable_real_entry_and_complete_last_registers(self):
        with tempfile.TemporaryDirectory() as tmp:
            out=Path(tmp);rpc,report,point=self.trace_fixture(out)
            with patch.object(runner,'dump',return_value={'before':point}) as dump,patch.object(runner.time,'sleep'):
                runner.capture_startup_trace(rpc,report,out,0x58,0x4040,0x38)
            self.assertTrue(report['startup_trace']['complete']);self.assertEqual(report['startup_trace']['trace_instructions'],2)
            self.assertEqual((out/'entry-cpu.txt').read_bytes(),(out/'LOGCPU.TXT').read_bytes())
            self.assertEqual([c.args[0] for c in rpc.request.call_args_list].count('EXEC LOGL 2000'),1)
            self.assertNotIn('EXEC RUN',[c.args[0] for c in rpc.request.call_args_list]);dump.assert_called_once()
    def test_last_register_mismatch_fails_without_qualifying_trace(self):
        with tempfile.TemporaryDirectory() as tmp:
            out=Path(tmp);rpc,report,point=self.trace_fixture(out,corrupt=True)
            with patch.object(runner,'dump',return_value={'before':point}),patch.object(runner.time,'sleep'):
                with self.assertRaisesRegex(ValueError,'寄存器与实际暂停点不符'):
                    runner.capture_startup_trace(rpc,report,out,0x58,0x4040,0x38)
            self.assertFalse(report['startup_trace']['complete']);self.assertFalse((out/'entry-cpu.txt').exists())
    def test_optional_LOGL_is_exact_and_default_remains_closed(self):
        rpc=runner.LoaderRPC.__new__(runner.LoaderRPC);rpc._owner=Mock();rpc._exchange=Mock(return_value=['ok'])
        with self.assertRaises(ValueError):rpc.request('EXEC LOGL 2000')
        rpc.startup_trace=True;self.assertEqual(rpc.request('EXEC LOGL 2000'),['ok'])
        for command in ('EXEC LOGL 2001','EXEC LOGL 2000\nEXEC RUN','EXEC SM 1:1 00'):
            with self.assertRaises(ValueError):rpc.request(command)
    def test_default_and_trace_register_sets_have_different_explicit_width(self):
        rpc=runner.LoaderRPC.__new__(runner.LoaderRPC)
        for enabled,names in ((False,runner.REGS),(True,runner.TRACE_REGS)):
            rpc.startup_trace=enabled;rpc.request=Mock(return_value=[' '.join('0' for _ in names)])
            self.assertEqual(set(rpc.regs()),set(names));self.assertEqual(len(names),15 if enabled else 12)
    def test_trace_exhaustion_fails_without_blind_RUN_or_dump(self):
        with tempfile.TemporaryDirectory() as tmp:
            out=Path(tmp);(out/'native.log').write_text('DEBUG: cpu log LOGCPU.TXT created\n')
            rpc=Mock();rpc.regs.return_value={'CS':0x88,'EIP':0x3966,'DS':0xa0,'ES':0x1c8,'SS':0xb8}
            report={'loaded_validation':{'passed':True},'loaded_snapshot':{'before':rpc.regs.return_value}}
            with patch.object(runner,'dump') as dump:
                with self.assertRaisesRegex(ValueError,'先耗尽'):runner.capture_startup_trace(rpc,report,out,0x170,0x12345,0x178)
            dump.assert_not_called();self.assertNotIn('EXEC RUN',[c.args[0] for c in rpc.request.call_args_list])
            self.assertFalse(report['startup_trace']['complete'])

if __name__=='__main__':unittest.main()
