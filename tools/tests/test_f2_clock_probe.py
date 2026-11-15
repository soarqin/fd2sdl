"""正常UI观察的纯合同回归；无GUI/TCP/Win32、原CPU或真实原资源依赖。"""
from contextlib import contextmanager
import json
import os
from pathlib import Path
import random
import struct
import sys
import tempfile
from types import SimpleNamespace as NS
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import run_f2_clock_probe as runner


def registers(cva=0x201e1):
    return dict(FS=0,GS=0x20,EBP=4,ESI=0x310000,CS=0x170,DS=0x178,SS=0x178,
        EIP=0x200000+cva-0x10000,ESP=0x255000,CR0=1,EFLAGS=0x202)


def descriptor(base=0,code=False):
    return {'base':base,'limit':0xffffffff,'type':0x1a if code else 0x12,'dpl':0,'flags':'10011'}


class ClockProbeTest(unittest.TestCase):
    def test_exact_EV_parser_and_legacy_parser_is_unchanged(self):
        value=registers();row=' '.join(f'{value[k]:x}' for k in runner.REGISTERS)
        header="EV of '"+' '.join(runner.REGISTERS)+"' is:"
        self.assertEqual(runner.parse_registers([header,row]),value)
        for lines in ([row],[header,row,row],[header,' '.join(row.split()[:-1])],['EV of wrong order is:',row]):
            with self.assertRaises(ValueError):runner.parse_registers(lines)
        with self.assertRaises(ValueError):runner.parse_registers([header,'10000 '+row.split(' ',1)[1]])
        self.assertEqual(runner.snapshot.REGISTERS,('CS','DS','EIP','ESP','EBP','CR0','EFLAGS'))

    def test_narrow_request_rejects_control_write_stack_or_whole_RAM_commands(self):
        rpc=NS(request=lambda command:self.fail('拒绝前不应发送命令'))
        probe=runner.Probe(rpc,Path('unused'),{})
        for command in ('BREAK','EXEC STEP','EXEC RUN','EXEC BP 170:0','EXEC SM DS:0 0',
                'EXEC SR EBP 0','EXEC EV EBP=0','EXEC EV FS GS','EXEC SELINFO CS',
                'EXEC MEMDUMPBIN 178:0 1000000 whole.bin',runner.EV+'\nEXEC STEP'):
            with self.assertRaises(ValueError):probe.request(command)
        with self.assertRaises(ValueError):probe.field({'DS':0x178,'data_DS':0x250000},'stack')

    def test_bounded_reads_use_only_code_window_and_two_DWORD_fields(self):
        with tempfile.TemporaryDirectory() as folder:
            events=[]
            def request(command):
                events.append(command);parts=command.split();length,factor=parts[3].split('*',1)
                self.assertEqual(factor,'(CR0&1)*(1-((EFLAGS>>11)&1))*(1-((CR0>>1F)&1))');size=int(length,16)
                Path(parts[4]).write_bytes(bytes(range(size)));return ['Memory dump binary success.']
            probe=runner.Probe(NS(request=request),Path(folder),{})
            probe.descriptors={0x170:descriptor(code=True),0x178:descriptor()}
            probe.pc_code(registers());mapping={'DS':0x178,'data_DS':0x250000}
            probe.field(mapping,'active_pointer');probe.field(mapping,'active_count')
            self.assertEqual([row['size'] for row in probe.record['reads']],[96,4,4])
            self.assertEqual([row['offset'] for row in probe.record['reads']][1:],[0x253a45,0x253beb])
            self.assertFalse(probe.record['requested_stack_read']);self.assertFalse(probe.record['whole_RAM_dump'])
            self.assertNotIn('raw_stack_read',probe.record)
            with self.assertRaises(ValueError):probe._read(0x178,0,runner.RAM_LIMIT,'all',code=False)
            probe.bytes=2048
            with self.assertRaises(ValueError):probe.pc_code(registers())
            self.assertEqual(len(events),3)

    def test_same_command_mode_expression_and_version_are_explicit(self):
        self.assertEqual(runner.MODE_FACTOR,'(CR0&1)*(1-((EFLAGS>>11)&1))*(1-((CR0>>1F)&1))')
        self.assertEqual(runner.MODE_SOURCE['version'],'dosbox-x-v2026.10.01')
        self.assertEqual(runner.MODE_SOURCE['debug_cpp_sha256'],'29ed1e6b459127b49cf74b1b26251c67b500d07e5db4a86f9ae956d4b958d1df')
        # 这是独立期望数学表，非实际DOSBox parser运行或模式采样。
        for n in (4,0x60):
            for pe in (0,1):
                for vm in (0,1):
                    for pg in (0,1):
                        cr0=0x10|pe|(pg<<31);eflags=0x246|(vm<<17)
                        self.assertEqual(n*(cr0&1)*(1-((eflags>>0x11)&1))*(1-((cr0>>0x1f)&1)),n if pe and not vm and not pg else 0)

    def test_zero_length_retries_new_files_without_turning_zero_into_data(self):
        for code,size in ((True,96),(False,4)):
            with self.subTest(code=code),tempfile.TemporaryDirectory() as folder:
                commands=[];lengths=iter((0,0,size))
                def request(command):
                    commands.append(command);path=Path(command.split()[4]);length=next(lengths)
                    path.write_bytes(bytes(range(length)));return ['Memory dump binary success.']
                probe=runner.Probe(NS(request=request),Path(folder),{})
                probe.descriptors={0x170:descriptor(code=True),0x178:descriptor()}
                value=probe.pc_code(registers()) if code else probe.field({'DS':0x178,'data_DS':0x250000},'active_count')
                self.assertEqual(value,bytes(range(96)) if code else 0x03020100)
                self.assertEqual(len(probe.record['reads']),1)
                self.assertEqual([a['returned_size'] for a in probe.record['read_attempts']],[0,0,size])
                self.assertEqual([a['qualified_guest_bytes'] for a in probe.record['read_attempts']],[0,0,size])
                self.assertEqual(probe.bytes,size);self.assertEqual(probe.read_upper_bound,size*3)
                self.assertEqual(len({c.split()[4] for c in commands}),3)
                self.assertTrue(all(a['guest_read_bytes_if_mode_gate_refused']==0 for a in probe.record['read_attempts'][:2]))
                self.assertTrue(probe.record['actual_guest_addresses_are_not_an_atomic_descriptor_snapshot'])

    def test_empty_reads_exhaust_finitely_and_unexpected_lengths_never_pad(self):
        for actual in (0,1,3,5,95,97):
            with self.subTest(actual=actual),tempfile.TemporaryDirectory() as folder:
                calls=[]
                def request(command):
                    calls.append(command);Path(command.split()[4]).write_bytes(b'x'*actual)
                    return ['Memory dump binary success.']
                probe=runner.Probe(NS(request=request),Path(folder),{});probe.descriptors={0x170:descriptor(code=True)}
                with self.assertRaises(runner.ContextNotClient if not actual else ValueError):probe.pc_code(registers())
                self.assertEqual(len(calls),runner.READ_ATTEMPTS if not actual else 1)
                self.assertFalse(probe.record['reads']);self.assertEqual(probe.bytes,0)
                self.assertLessEqual(probe.read_upper_bound,2048)

    def test_byte_command_and_time_limits_bound_even_zero_length_attempts(self):
        for budget in ('bytes','commands','time'):
            with self.subTest(budget=budget),tempfile.TemporaryDirectory() as folder:
                calls=[]
                def request(command):
                    calls.append(command);Path(command.split()[4]).write_bytes(b'')
                    return ['Memory dump binary success.']
                probe=runner.Probe(NS(request=request),Path(folder),{});probe.descriptors={0x170:descriptor(code=True)}
                if budget=='bytes':probe.read_upper_bound=2048-96
                elif budget=='commands':probe.commands=runner.COMMAND_LIMIT
                else:probe.deadline=runner.time.monotonic()-1
                with self.assertRaisesRegex(ValueError,'预算'):probe.pc_code(registers())
                self.assertEqual(len(calls),1 if budget=='bytes' else 0)
                self.assertLessEqual(probe.read_upper_bound,2048)

    def test_real_four_byte_zero_does_not_bypass_active_count_oracle(self):
        with tempfile.TemporaryDirectory() as folder:
            def request(command):
                parts=command.split();data=struct.pack('<I',0x300000 if ':253A45' in parts[2] else 0)
                Path(parts[4]).write_bytes(data);return ['Memory dump binary success.']
            probe=runner.Probe(NS(request=request),Path(folder),{});probe.descriptors={0x178:descriptor()}
            with self.assertRaisesRegex(ValueError,'人数'):runner.pool(probe,{'DS':0x178,'data_DS':0x250000},12)
            self.assertEqual([a['returned_size'] for a in probe.record['read_attempts']],[4,4])
            self.assertEqual(probe.bytes,8);self.assertEqual(len(probe.record['reads']),2)

    def test_invalid_numeric_selector_response_is_recorded_as_unqualified_context(self):
        probe=runner.Probe(NS(request=lambda command:['Selector not valid']),Path('unused'),{})
        with self.assertRaises(runner.ContextNotClient):probe.descriptor(0x170)
        self.assertEqual(probe.record['invalid_selector_observations'][0]['selector'],0x170)
        self.assertFalse(probe.record['reads'])

    def model(self,*,duplicate=False):
        image=bytearray(0x50000);rng=random.Random(31)
        seed=rng.randbytes(96);image[0x14000:0x14060]=seed
        if duplicate:image[0x15000:0x15060]=seed
        fixups=[]
        for cva,target in ((runner.ANCHORS[0],0x3a45),(runner.ANCHORS[1],0x3beb)):
            image[cva:cva+96]=rng.randbytes(96);struct.pack_into('<I',image,cva+12,0)
            fixups.append(NS(source_type=7,source_addr_relbase=cva+12,target_object=2,target_offset=target))
        objects=[NS(index=1,relbase=0x10000,vsize=0x3ef29),NS(index=2,relbase=0x50000,vsize=0x56b0)]
        return runner.CodeModel(bytes(image),fixups,objects),seed

    def test_PC_seed_is_unique_and_relocation_changes_require_legal_fixups(self):
        model,seed=self.model();self.assertEqual(model.locate_window(seed),0x14000)
        copy,_=self.model(duplicate=True)
        with self.assertRaisesRegex(ValueError,'唯一'):copy.locate_window(seed)
        with self.assertRaises(ValueError):model.locate_window(b'\0'*96)
        blocks=[]
        for cva,f in zip(runner.ANCHORS,model.fixups):
            data=bytearray(model.image[cva:cva+96]);struct.pack_into('<I',data,12,0x250000+f.target_offset)
            self.assertTrue(model.matches(cva,data));blocks.append((cva,bytes(data)))
        base,proof= model.data_base(blocks);self.assertEqual(base,0x250000);self.assertEqual(len(proof),2)
        for cva,data in blocks:model.validate_relocations(cva,data,{2:base})
        corrupt=bytearray(blocks[1][1]);struct.pack_into('<I',corrupt,12,0x250001+0x3beb)
        with self.assertRaisesRegex(ValueError,'独立一致'):model.data_base([blocks[0],(blocks[1][0],corrupt)])
        corrupt=bytearray(blocks[0][1]);corrupt[0]^=1
        with self.assertRaises(ValueError):model.data_base([(blocks[0][0],corrupt),blocks[1]])

    def test_fresh_mapping_uses_CS_and_DS_bases_and_never_an_old_absolute_base(self):
        model,seed=self.model();regs=registers();regs['EIP']=0x180000+0x14000-0x10000
        views={'CS':descriptor(0x80000,True),'DS':descriptor(0x80000),'SS':descriptor(0x80000)}
        blocks={}
        for cva,f in zip(runner.ANCHORS,model.fixups):
            raw=bytearray(model.image[cva:cva+96]);struct.pack_into('<I',raw,12,0x250000+f.target_offset);blocks[cva]=raw
        class Probe:
            record={}
            def registers(self):return regs.copy()
            def pc_code(self,r):return seed
            def anchor(self,m,cva):return bytes(blocks[cva])
        with patch.object(runner,'descriptors',return_value=views):mapping=runner.bind_image(Probe(),model,attempts=1)
        self.assertEqual(mapping['code_linear'],0x200000);self.assertEqual(mapping['data_linear'],0x2d0000)
        self.assertEqual(mapping['data_DS'],0x250000)

    def test_client_context_checks_current_descriptors_and_does_not_assume_zero(self):
        regs=registers();values={0x170:descriptor(0x80000,True),0x178:descriptor(0x80000)}
        probe=NS(descriptor=lambda selector:values[selector])
        self.assertEqual(runner.descriptors(probe,regs)['DS']['base'],0x80000)
        values[0x178]=descriptor(0x10000)
        with self.assertRaises(runner.ContextNotClient):runner.descriptors(probe,regs)

    def test_pointer_classification_uses_whole_object2_and_DS_pool_without_claiming_stack_bounds(self):
        mapping={'code_linear':0x200000,'descriptor_before':{'CS':descriptor(code=True),
            'DS':descriptor(0x10000),'SS':descriptor(0x20000)}}
        prefix={'linear_start':0x310000,'linear_end':0x3103c0,'object2_linear_range':[0x270000,0x2756b0]}
        regs=registers(0x1b2fe);regs.update(EBP=0x255000,ESI=0x300000)
        detail=runner.classify(regs,mapping,prefix)
        self.assertTrue(detail['EBP_in_object2']);self.assertTrue(detail['ESI_in_active_prefix'])
        self.assertFalse(detail['EBP_as_DS_in_active_prefix']);self.assertFalse(detail['EBP_is_sign_extended_word'])
        regs['EBP']=0x250010
        self.assertTrue(runner.classify(regs,mapping,prefix)['EBP_in_object2'])  # file-backed区同属object2，不能把BSS起点称栈界。
        self.assertNotIn('EBP_in_declared_stack',detail)

    def test_no_body_PC_sample_is_finite_failure_even_with_legal_callee_samples(self):
        regs=registers(0x10630);views={'CS':descriptor(code=True),'DS':descriptor(),'SS':descriptor()}
        mapping={'CS':regs['CS'],'DS':regs['DS'],'SS':regs['SS'],'code_linear':0x200000,'data_DS':0x250000,'descriptor_before':views}
        prefix={'linear_start':0x300000,'linear_end':0x3003c0,'object2_linear_range':[0x250000,0x2556b0]}
        probe=NS(record={},registers=lambda:regs.copy(),anchor=lambda m,c:b'x'*96)
        model=NS(matches=lambda c,a:True,validate_relocations=lambda *x:None)
        capture=NS(trace=[])
        with patch.object(runner,'wait_state'),patch.object(runner,'bind_image',return_value=mapping),\
             patch.object(runner,'pool',return_value=prefix),patch.object(runner,'descriptors',return_value=views),\
             patch.object(runner.time,'sleep'):
            with self.assertRaisesRegex(ValueError,'本体PC'):runner.observe(capture,probe,model,'overview',None,[],12,samples=3,timeout=1)
        self.assertEqual(len(probe.record['observation']['samples']),3)
        self.assertTrue(all(row['callee_input_check_sample'] for row in probe.record['observation']['samples']))

    def test_reference_checks_actual_conf_and_explicit_only_MCP_cleanup_additions(self):
        with tempfile.TemporaryDirectory() as folder:
            root=Path(folder);conf=root/'dosbox.conf';conf.write_text('[dosbox]\nmcp_server=12345\nquit warning=false\n')
            command=[str(root/'dosbox-x.exe'),'-conf',str(conf)]
            seen=[]
            def build(*args):seen.append(args);return {'scope':{'configuration_chain_verified':True}}
            with patch.object(runner.original_reference,'build',build),\
                 patch.object(runner.original_reference,'load_json',return_value={'explicit_settings':{}}):
                _,actual,extra=runner.qualify_reference(root,root/'dosbox-x.exe',conf,root/'bootstrap',12345,command,{})
                self.assertEqual(actual[-2:],['-set','mixer nosound=true']);self.assertFalse(extra['global_reference_rules_modified'])
                self.assertEqual(seen[0][2],[conf.resolve()])
                conf.write_text('[dosbox]\nmcp_server=12346\nquit warning=false\n')
                with self.assertRaises(ValueError):runner.qualify_reference(root,root/'dosbox-x.exe',conf,root/'bootstrap',12345,command,{})
                conf.write_text('[dosbox]\nmcp_server=12345\nquit warning=false\nunknown=1\n')
                with self.assertRaises(ValueError):runner.qualify_reference(root,root/'dosbox-x.exe',conf,root/'bootstrap',12345,command,{})

    @contextmanager
    def harness(self,root,*,reject=False,fail_observation=False,mtime_change=False):
        original=root/'original_game';original.mkdir();(original/'START.SAV').write_bytes(b'unmodified start');(original/'FD2.EXE').write_bytes(b'original')
        for name in ('driver.exe','dosbox-x.exe','bootstrap.conf','reference.json'):(root/name).write_bytes(name.encode())
        args=NS(root=root,driver=root/'driver.exe',dosbox=root/'dosbox-x.exe',bootstrap=root/'bootstrap.conf',artifacts=root/'out',samples=3,timeout=1)
        order=[]
        @contextmanager
        def display(record,*,require_display_on):
            self.assertTrue(require_display_on);order.append('display-on');record['require_display_on']=True
            try:yield
            finally:order.append('display-release');record['released']=True
        class RPC:
            def __init__(self,record):self.record=record;self.port=12345
            def __enter__(self):order.append('rpc-enter');return self
            def bind(self,process):order.append('bind');self.record.update(pid=process.pid,tcp_owner_verified=True)
            def __exit__(self,*error):order.append('rpc-close');self.record['closed']=True
        @contextmanager
        def spawn(command,cwd,env,log,driver,receipts):
            order.append('spawn');actual,_,_=runner.quiet_game_command(command,env)
            row={'command':actual,'pid':99};receipts.append(row)
            try:yield NS(pid=99)
            finally:order.append('clean');row.update(cleaned=True,forced_cleanup=False,exit_code=0)
        def reference(original,dosbox,config,bootstrap,port,command,env):
            order.append('reference');self.assertIn('mcp_server=12345',config.read_text())
            if reject:raise ValueError('reference rejected before window')
            if mtime_change:os.utime(config,ns=(config.stat().st_atime_ns,config.stat().st_mtime_ns+1000000))
            actual,_,_=runner.quiet_game_command(command,env)
            return {'resources':{'FD2.EXE':{'sha256':runner.sha(original/'FD2.EXE'),'size':8}}},actual,{}
        def exercise(*items):
            order.append('observe')
            if fail_observation=='zero':
                work=args.artifacts/'auxiliary';work.mkdir();record=items[7];item={};record['observations']['auxiliary']=item
                def request(command):
                    Path(command.split()[4]).write_bytes(b'');return ['Memory dump binary success.']
                probe=runner.Probe(NS(request=request),work,item);probe.descriptors={0x178:descriptor()}
                probe.field({'DS':0x178,'data_DS':0x250000},'active_count')
            if fail_observation:raise ValueError('no qualified sample')
            record=items[7]
            record['observations']={kind:{'observation':{'ok':True}} for kind in runner.BODY}
        with patch.object(runner,'owned_job_required'),patch.object(runner,'source_facts',return_value={}),\
             patch.object(runner.auxiliary,'prepare',return_value=object()),patch.object(runner.CodeModel,'original',return_value=object()),\
             patch.object(runner,'title_templates',return_value={}),patch.object(runner,'windows_display_requirement',display),\
             patch.object(runner.snapshot,'OwnedRPC',RPC),patch.object(runner,'running',spawn),patch.object(runner,'Capture'),\
             patch.object(runner,'qualify_reference',reference),patch.object(runner,'exercise',exercise),\
             patch.object(runner.original_reference,'REFERENCE',root/'reference.json'):
            yield args,order

    def test_single_owned_process_reference_before_spawn_and_cleanup_before_display_release(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.harness(Path(folder)) as (args,order):report=runner.run(args)
            self.assertTrue(report['ok'],report.get('error'));self.assertEqual(order,['display-on','rpc-enter','reference','spawn','bind','observe','clean','rpc-close','display-release'])
            self.assertTrue(report['Save_bytes_and_mtime_unchanged']);self.assertFalse(report['requested_stack_read'])
            self.assertNotIn('raw_stack_read',report);self.assertEqual(report['schema'],'fd2.f2.normal-clock-observation.v2')
            self.assertTrue(report['same_command_mode_guard']);self.assertEqual(report['mode_gate_source'],runner.MODE_SOURCE)
            self.assertTrue((args.artifacts/'report.json').is_file())

    def test_zero_length_exhaustion_keeps_failure_and_owned_cleanup_order(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.harness(Path(folder),fail_observation='zero') as (args,order):report=runner.run(args)
            self.assertFalse(report['ok']);self.assertEqual(report['error']['type'],'ContextNotClient')
            self.assertEqual(order[-3:],['clean','rpc-close','display-release'])
            self.assertTrue(report['normal_owned_cleanup']);self.assertTrue(report['Save_bytes_and_mtime_unchanged'])
            attempts=report['observations']['auxiliary']['read_attempts']
            self.assertEqual(len(attempts),runner.READ_ATTEMPTS);self.assertTrue(all(a['returned_size']==0 for a in attempts))

    def test_reference_rejection_never_spawns_and_missing_observation_still_cleans(self):
        for reject in (True,False):
            with tempfile.TemporaryDirectory() as folder:
                with self.harness(Path(folder),reject=reject,fail_observation=not reject) as (args,order):report=runner.run(args)
                self.assertFalse(report['ok']);self.assertEqual(order[-2:],['rpc-close','display-release'])
                self.assertEqual('spawn' in order,not reject)
                if not reject:self.assertLess(order.index('clean'),order.index('rpc-close'))

    def test_same_bytes_with_changed_config_mtime_stops_before_window(self):
        with tempfile.TemporaryDirectory() as folder:
            with self.harness(Path(folder),mtime_change=True) as (args,order):report=runner.run(args)
            self.assertFalse(report['ok']);self.assertNotIn('spawn',order)
            self.assertFalse(report['sources_unchanged']);self.assertEqual(order[-2:],['rpc-close','display-release'])


if __name__=='__main__':unittest.main()
