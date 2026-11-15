"""准备代表入口守卫；全部宿主/legacy runner为替身，不开窗口或执行游戏。"""
from contextlib import contextmanager
import json
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import run_f2_prepare_check as runner


class F2PrepareCheckTest(unittest.TestCase):
    def fixture(self,root):
        original=root/'original_game';original.mkdir()
        (original/'START.SAV').write_bytes(b'read-only fixture')
        for name in ('fd2sdl.exe','driver.exe','dosbox-x.exe','bootstrap.conf','reference.json'):
            (root/name).write_bytes(name.encode())
        bootstrap=root/'bootstrap.conf';bootstrap.write_bytes(b'[mixer]\nrate=48000\n')
        (root/'reference.json').write_text(json.dumps({'schema':runner.original_reference.REFERENCE_SCHEMA,
            'configuration_source':{'bootstrap':{'sha256':runner.sha(bootstrap),'size':bootstrap.stat().st_size},
                'inherited_settings':{'mixer.rate':'48000'}}}),encoding='utf-8')
        return SimpleNamespace(root=root,artifacts=root/'out',game=root/'fd2sdl.exe',
            driver=root/'driver.exe',dosbox=root/'dosbox-x.exe',bootstrap=root/'bootstrap.conf')

    @contextmanager
    def harness(self,args,*,reference_error=False,display_error=False,wrong_receipt=False,after_original=None,rate='48000'):
        order=[];active=[]
        config_text='[mixer]\nrate='+rate+'\n'

        @contextmanager
        def display(record,*,require_display_on):
            self.assertIs(require_display_on,True);record['require_display_on']=True
            if display_error:raise RuntimeError('display OFF')
            order.append('display-on')
            try:yield
            finally:record['released']=True;order.append('display-released')

        @contextmanager
        def spawn(command,cwd,env,log,driver,receipts,**options):
            self.assertFalse(active,'两端不可同时运行')
            role='original' if Path(command[0])==args.dosbox else 'sdl'
            actual,_,_=runner.quiet_game_command(command,env)
            receipts.append({'command':actual+(['unexpected'] if wrong_receipt else [])})
            order.append('spawn-'+role);active.append(role)
            try:yield object()
            finally:active.pop();order.append('clean-'+role)

        def reference(original,dosbox,configs,overrides,bootstrap):
            self.assertEqual(configs[0].read_text(),config_text)
            self.assertEqual(overrides,(['mixer rate=48000'] if rate=='44100' else [])+['mixer nosound=true'])
            self.assertEqual((original,dosbox,bootstrap),(args.root/'original_game',args.dosbox,args.bootstrap))
            order.append('reference')
            if reference_error:raise ValueError('reference rejected')
            return {'test_reference':True,'configs':[runner.original_reference.read_input(p)[0] for p in configs]}

        def legacy(options):
            self.assertEqual(options.scenario,'p2-self-give')
            self.assertIsNone(options.original_evidence);self.assertFalse(options.prepare_only)
            config=options.artifacts/'original/dosbox.conf'
            with runner.g3.windows_display_requirement({}):
                config.parent.mkdir(parents=True);config.write_text(config_text)
                for role in ('original','sdl'):
                    command=[str(args.dosbox),'-conf',str(config),'-noconsole'] if role=='original' else [str(args.game)]
                    with runner.g3.running(command,args.root,{},args.root/'unused.log',args.driver,[]):
                        order.append('body-'+role)
                    if role=='original' and after_original:after_original()
            return {'ok':True}

        with patch.object(runner,'owned_job_required'),patch.object(runner,'source_facts',return_value={}),\
                patch.object(runner.original_reference,'REFERENCE',args.root/'reference.json'),\
                patch.object(runner.original_reference,'build',reference),\
                patch.object(runner.g3,'running',spawn),patch.object(runner.g3,'windows_display_requirement',display),\
                patch.object(runner.g3,'run',legacy):
            yield order,spawn,display

    def test_single_controlled_pair_uses_actual_conf_and_restores_borrowed_functions(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            with self.harness(args) as (order,spawn,display):
                result=runner.run(args)
                self.assertIs(runner.g3.running,spawn);self.assertIs(runner.g3.windows_display_requirement,display)
            self.assertTrue(result['ok']);self.assertTrue(result['controlled_fixture'])
            self.assertFalse(result['natural_arrival_claimed'])
            self.assertEqual(result['launched_roles'],['original','sdl'])
            self.assertEqual(order,['display-on','reference','spawn-original','body-original','clean-original',
                'spawn-sdl','body-sdl','clean-sdl','display-released'])
            self.assertEqual(result['launch_attempts'][0]['command'][-2:],['-set','mixer nosound=true'])
            self.assertFalse(result['rate_adaptations'][0]['applied'])
            self.assertNotIn('mixer rate=48000',result['launch_attempts'][0]['command'])
            self.assertIn(str(args.bootstrap),result['guards'])
            self.assertIn('strict phase',' '.join(result['limitations']))
            self.assertTrue((args.artifacts/'report.json').is_file())

    def test_reference_rejection_after_conf_stops_before_spawn(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            with self.harness(args,reference_error=True,rate='44100') as (order,spawn,display):
                result=runner.run(args)
                self.assertIs(runner.g3.running,spawn);self.assertIs(runner.g3.windows_display_requirement,display)
            self.assertFalse(result['ok']);self.assertEqual(result['error']['text'],'reference rejected')
            self.assertEqual(order,['display-on','reference','display-released'])
            self.assertEqual((args.artifacts/'g3/original/dosbox.conf').read_text(),'[mixer]\nrate=44100\n')

    def test_known_legacy_rate_is_adjusted_before_reference_and_real_command(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory));before=runner.g3.fingerprint(args.root/'original_game')
            bootstrap=args.bootstrap.read_bytes();environment=dict(os.environ)
            with self.harness(args,rate='44100') as (order,_,_):result=runner.run(args)
            self.assertTrue(result['ok']);self.assertTrue(result['guards_unchanged'])
            adaptation=result['rate_adaptations'][0]
            self.assertTrue(adaptation['applied']);self.assertEqual((adaptation['from'],adaptation['to']),('44100','48000'))
            self.assertEqual(adaptation['actual_command'][-4:],['-set','mixer rate=48000','-set','mixer nosound=true'])
            self.assertEqual(adaptation['actual_command'],result['launch_attempts'][0]['command'])
            self.assertLess(order.index('reference'),order.index('spawn-original'))
            self.assertEqual((args.artifacts/'g3/original/dosbox.conf').read_text(),'[mixer]\nrate=44100\n')
            self.assertEqual(args.bootstrap.read_bytes(),bootstrap);self.assertEqual(dict(os.environ),environment)
            self.assertEqual(runner.g3.fingerprint(args.root/'original_game'),before)
            self.assertFalse(adaptation['config_files_modified']);self.assertFalse(adaptation['global_settings_modified'])

    def test_unknown_rate_is_rejected_without_reference_spawn_or_settings_changes(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory));bootstrap=args.bootstrap.read_bytes();environment=dict(os.environ)
            with self.harness(args,rate='22050') as (order,_,_):result=runner.run(args)
            self.assertFalse(result['ok']);self.assertIn('未知G3 mixer.rate',result['error']['text'])
            self.assertNotIn('reference',order);self.assertNotIn('spawn-original',order)
            self.assertEqual(args.bootstrap.read_bytes(),bootstrap);self.assertEqual(dict(os.environ),environment)
            self.assertEqual((args.artifacts/'g3/original/dosbox.conf').read_text(),'[mixer]\nrate=22050\n')

    def test_new_reference_target_does_not_expand_the_known_migration(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory));args.bootstrap.write_bytes(b'[mixer]\nrate=96000\n')
            reference=json.loads((args.root/'reference.json').read_text())
            source=reference['configuration_source'];source['bootstrap']={
                'sha256':runner.sha(args.bootstrap),'size':args.bootstrap.stat().st_size}
            source['inherited_settings']['mixer.rate']='96000'
            (args.root/'reference.json').write_text(json.dumps(reference),encoding='utf-8')
            with self.harness(args,rate='44100') as (order,_,_):result=runner.run(args)
            self.assertFalse(result['ok']);self.assertIn('未登记的参照/bootstrap mixer.rate',result['error']['text'])
            self.assertNotIn('reference',order);self.assertNotIn('spawn-original',order)
            self.assertEqual(args.bootstrap.read_bytes(),b'[mixer]\nrate=96000\n')

    def test_display_off_stops_before_configuration_or_spawn(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            with self.harness(args,display_error=True) as (order,spawn,display):
                result=runner.run(args)
                self.assertIs(runner.g3.running,spawn);self.assertIs(runner.g3.windows_display_requirement,display)
            self.assertFalse(result['ok']);self.assertEqual(result['error']['text'],'display OFF')
            self.assertEqual(order,[]);self.assertFalse((args.artifacts/'g3').exists())

    def test_bootstrap_mtime_change_blocks_second_endpoint(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            def change():
                stamp=args.bootstrap.stat()
                os.utime(args.bootstrap,ns=(stamp.st_atime_ns,stamp.st_mtime_ns+1000000))
            with self.harness(args,after_original=change) as (order,_,_):result=runner.run(args)
            self.assertFalse(result['ok']);self.assertFalse(result['guards_unchanged'])
            self.assertIn('clean-original',order);self.assertNotIn('spawn-sdl',order)
            self.assertIn('窗口前改变',result['error']['text'])

    def test_actual_command_mismatch_is_rejected_and_owned_scope_is_cleaned(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            with self.harness(args,wrong_receipt=True) as (order,_,_):result=runner.run(args)
            self.assertFalse(result['ok']);self.assertIn('实际启动命令',result['error']['text'])
            self.assertIn('clean-original',order);self.assertNotIn('body-original',order)
            self.assertNotIn('spawn-sdl',order)

    def test_configuration_receipt_cannot_be_refrozen_after_qualification(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            with self.harness(args) as (order,_,_):
                previous=runner.original_reference.build
                def changed(*pos):
                    receipt=previous(*pos);Path(receipt['configs'][0]['path']).write_text('changed after qualification')
                    return receipt
                with patch.object(runner.original_reference,'build',changed):result=runner.run(args)
            self.assertFalse(result['ok']);self.assertFalse(result['guards_unchanged'])
            self.assertIn('启动前来源改变',result['error']['text']);self.assertNotIn('spawn-original',order)

    def test_first_reference_error_survives_late_source_guard_error(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            with self.harness(args,reference_error=True),\
                    patch.object(runner,'source_facts',side_effect=[{}, {}, OSError('late source guard')]):
                result=runner.run(args)
            self.assertFalse(result['ok']);self.assertFalse(result['sources_unchanged'])
            self.assertEqual(result['error']['text'],'reference rejected')
            self.assertEqual(result['guard_errors'][0]['text'],'late source guard')

    def test_protected_output_is_rejected_before_reading_or_creating(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory));args.artifacts=args.root/'original_game/new-output'
            with patch.object(runner,'owned_job_required'),patch.object(runner.g3,'fingerprint') as read,\
                    patch.object(runner.g3,'run') as execute,self.assertRaisesRegex(ValueError,'原资源'):
                runner.run(args)
            read.assert_not_called();execute.assert_not_called();self.assertFalse(args.artifacts.exists())

    def test_command_requires_existing_actual_conf_and_complete_values(self):
        for command in (['dosbox-x.exe'],['dosbox-x.exe','-conf'],['dosbox-x.exe','-set']):
            with self.subTest(command=command),self.assertRaises(ValueError):runner.command_configuration(command)


if __name__=='__main__':unittest.main()
