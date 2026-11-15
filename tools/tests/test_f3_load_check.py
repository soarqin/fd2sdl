"""F3工具的只读来源、显示及启动边界；不启动任何窗口／游戏。"""
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
import run_f3_load_check as runner


def container(stale=True):
    plain=bytearray(0x59cb);plain[runner.load.META+1]=2
    plain[runner.load.UNITS+7]=7;plain[runner.load.UNITS+80+7]=3
    plain[runner.load.UNITS+80+2]=0 if stale else 1
    plain[-4:]=sum(plain[:-4]).to_bytes(4,'little')
    return bytes(plain)


class F3LoadCheckTest(unittest.TestCase):
    def fixture(self,root):
        original=root/'original_game';original.mkdir();(original/'START.SAV').write_bytes(b'original')
        for name in ('game.exe','driver.exe','dosbox.exe','bootstrap.conf','input.SAV'):
            (root/name).write_bytes(name.encode())
        return SimpleNamespace(root=root,artifacts=root/'out',game=root/'game.exe',
            driver=root/'driver.exe',dosbox=root/'dosbox.exe',bootstrap=root/'bootstrap.conf',save_fixture=root/'input.SAV')

    @contextmanager
    def display(self,record,*,require_display_on):
        self.assertIs(require_display_on,True);record['require_display_on']=True
        try:yield
        finally:record['released']=True

    def reference(self,original,dosbox,configs,overrides,bootstrap):
        return {'configs':[{'path':str(p.resolve()),'sha256':runner.sha(p)} for p in configs]}

    def verify_reference(self,path):
        receipt=json.loads(Path(path).read_text(encoding='utf-8'))
        if any(runner.sha(row['path'])!=row['sha256'] for row in receipt['configs']):
            raise ValueError('reference config changed')
        return receipt

    def original_copy(self,args,*,contents=None):
        config=args.artifacts/'original/dosbox.conf'
        config.parent.mkdir(parents=True);config.write_text('config')
        save=config.parent/'game/FD2.SAV';save.parent.mkdir()
        save.write_bytes(args.save_fixture.read_bytes() if contents is None else contents)
        return config

    def test_fixture_uses_identity_byte7_and_requires_actual_rewrite(self):
        report=runner.fixture_contract(container())
        self.assertEqual(report['expected_classes'],[0,1])
        self.assertEqual(report['required_rewrites'],[{'raw':1,'saved':0,'expected':1}])
        self.assertFalse(report['input_bytes_modified'])
        with self.assertRaisesRegex(ValueError,'至少一处'):runner.fixture_contract(container(False))
        bad=bytearray(container());bad[-1]^=1
        with self.assertRaisesRegex(ValueError,'checksum'):runner.fixture_contract(bytes(bad))

    def test_unowned_job_stops_before_fixture_or_output(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            with patch.object(runner,'owned_job_required',side_effect=RuntimeError('not owned')),\
                    patch.object(runner.load,'decode_save') as decode:
                with self.assertRaisesRegex(RuntimeError,'not owned'):runner.run(args)
            decode.assert_not_called();self.assertFalse(args.artifacts.exists())

    def test_display_off_stops_before_game_and_restores_hooks(self):
        @contextmanager
        def off(record,*,require_display_on):
            self.assertIs(require_display_on,True);raise RuntimeError('display OFF');yield
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory));previous=runner.load.running
            with patch.object(runner,'owned_job_required'),patch.object(runner,'source_facts',return_value={}),\
                    patch.object(runner.load,'decode_save',return_value=container()),\
                    patch.object(runner,'windows_display_requirement',off),patch.object(runner.load,'run') as game:
                result=runner.run(args)
            self.assertFalse(result['ok']);self.assertEqual(result['error']['text'],'display OFF')
            game.assert_not_called();self.assertIs(runner.load.running,previous)

    def test_reference_rejection_precedes_original_spawn(self):
        order=[]
        @contextmanager
        def spawn(*args,**kw):order.append('spawn');yield object()
        def reject(*args):order.append('reference');raise ValueError('wrong identity')
        def run(args):
            config=args.artifacts/'original/dosbox.conf';config.parent.mkdir(parents=True);config.write_text('config')
            with runner.load.running([str(args.dosbox),'-conf',str(config)]):self.fail('unqualified spawn')
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            with patch.object(runner,'owned_job_required'),patch.object(runner,'source_facts',return_value={}),\
                    patch.object(runner.load,'decode_save',return_value=container()),\
                    patch.object(runner,'windows_display_requirement',self.display),\
                    patch.object(runner.load,'running',spawn),patch.object(runner.load,'run',run),\
                    patch.object(runner.original_reference,'build',reject):result=runner.run(args)
            self.assertEqual(order,['reference']);self.assertFalse(result['ok'])

    def test_only_copied_environment_loses_debug_seed_and_fixture_is_unedited(self):
        environments=[];source=b'input.SAV'
        @contextmanager
        def spawn(command,*pos,**kw):
            if len(pos)>1:environments.append(dict(pos[1]))
            yield object()
        def run(args):
            self.assertEqual(args.save_fixture.read_bytes(),source)
            self.assertFalse(args.verify_focus_cycle)
            config=self.original_copy(args)
            with runner.load.running([str(args.dosbox),'-conf',str(config)]):pass
            copy=args.artifacts/'sdl/FD2.SAV';copy.parent.mkdir();copy.write_bytes(source)
            environment={'FD2SDL_FIELD_RNG_SEED':'31256','KEEP':'same','FD2SDL_SAVE_PATH':str(copy)}
            with runner.load.running([str(args.game)],args.root,environment):pass
            self.assertEqual(environment['FD2SDL_FIELD_RNG_SEED'],'31256')
            return {'ok':True}
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory));before=args.save_fixture.read_bytes();stamp=args.save_fixture.stat().st_mtime_ns
            with patch.dict(os.environ,{'FD2SDL_FIELD_RNG_SEED':'0x1234'}),\
                    patch.object(runner,'owned_job_required'),patch.object(runner,'source_facts',return_value={}),\
                    patch.object(runner.load,'decode_save',return_value=container()),\
                    patch.object(runner,'windows_display_requirement',self.display),\
                    patch.object(runner.load,'running',spawn),patch.object(runner.load,'run',run),\
                    patch.object(runner.original_reference,'build',side_effect=self.reference),\
                    patch.object(runner.original_reference,'verify',side_effect=self.verify_reference):
                result=runner.run(args);self.assertEqual(os.environ['FD2SDL_FIELD_RNG_SEED'],'0x1234')
            self.assertTrue(result['ok']);self.assertEqual(environments,[{'KEEP':'same','FD2SDL_SAVE_PATH':str(args.artifacts/'load/sdl/FD2.SAV')}])
            self.assertTrue(result['references_unchanged']);self.assertTrue(result['fixture_copies_verified'])
            self.assertEqual(args.save_fixture.read_bytes(),before);self.assertEqual(args.save_fixture.stat().st_mtime_ns,stamp)

    def test_decode_uses_the_frozen_bytes_even_if_input_changes(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory));before=args.save_fixture.read_bytes()
            def decode(snapshot):
                args.save_fixture.write_bytes(b'changed while decoding')
                self.assertEqual(snapshot.read_bytes(),before)
                return container()
            with patch.object(runner,'owned_job_required'),\
                    patch.object(runner.load,'decode_save',side_effect=decode),\
                    patch.object(runner,'windows_display_requirement',self.display),\
                    patch.object(runner.load,'run') as game:
                with self.assertRaisesRegex(ValueError,'来源存档在冻结后改变'):runner.run(args)
            game.assert_not_called();self.assertFalse(args.artifacts.exists())

    def test_fixture_changed_after_contract_cannot_replace_guard_identity(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            def sources(root):
                args.save_fixture.write_bytes(b'changed after contract')
                return {}
            with patch.object(runner,'owned_job_required'),\
                    patch.object(runner.load,'decode_save',return_value=container()),\
                    patch.object(runner,'source_facts',side_effect=sources),\
                    patch.object(runner,'windows_display_requirement',self.display),\
                    patch.object(runner.load,'run') as game:
                with self.assertRaisesRegex(ValueError,'来源存档在冻结后改变'):runner.run(args)
            game.assert_not_called();self.assertFalse(args.artifacts.exists())

    def test_changed_actual_copy_stops_before_process_spawn(self):
        def run(args):
            config=self.original_copy(args,contents=b'wrong copy')
            with runner.load.running([str(args.dosbox),'-conf',str(config)]):self.fail('spawned wrong fixture')
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            with patch.object(runner,'owned_job_required'),patch.object(runner,'source_facts',return_value={}),\
                    patch.object(runner.load,'decode_save',return_value=container()),\
                    patch.object(runner,'windows_display_requirement',self.display),\
                    patch.object(runner.original_reference,'build',side_effect=self.reference),\
                    patch.object(runner.original_reference,'verify',side_effect=self.verify_reference),\
                    patch.object(runner.load,'running') as spawn,patch.object(runner.load,'run',run):result=runner.run(args)
            spawn.assert_not_called();self.assertFalse(result['ok'])
            self.assertIn('实际隔离存档与冻结来源快照不一致',result['error']['text'])

    def test_config_change_between_build_and_spawn_is_rejected(self):
        def build(*args):
            receipt=self.reference(*args)
            args[2][0].write_text('changed after reference build')
            return receipt
        def run(args):
            config=self.original_copy(args)
            with runner.load.running([str(args.dosbox),'-conf',str(config)]):self.fail('spawned changed config')
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            with patch.object(runner,'owned_job_required'),patch.object(runner,'source_facts',return_value={}),\
                    patch.object(runner.load,'decode_save',return_value=container()),\
                    patch.object(runner,'windows_display_requirement',self.display),\
                    patch.object(runner.original_reference,'build',side_effect=build),\
                    patch.object(runner.original_reference,'verify',side_effect=self.verify_reference),\
                    patch.object(runner.load,'running') as spawn,patch.object(runner.load,'run',run):result=runner.run(args)
            spawn.assert_not_called();self.assertFalse(result['ok'])
            self.assertEqual(result['error']['text'],'reference config changed')

    def test_wrong_sdl_copy_is_rejected_after_original_cleanup(self):
        spawned=[]
        @contextmanager
        def spawn(command,*pos,**kw):
            spawned.append(Path(command[0]).name)
            yield object()
        def run(args):
            config=self.original_copy(args)
            with runner.load.running([str(args.dosbox),'-conf',str(config)]):pass
            copy=args.artifacts/'sdl/FD2.SAV';copy.parent.mkdir();copy.write_bytes(b'wrong SDL copy')
            with runner.load.running([str(args.game)],args.root,{'FD2SDL_SAVE_PATH':str(copy)}):
                self.fail('SDL with wrong copy was started')
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            with patch.object(runner,'owned_job_required'),patch.object(runner,'source_facts',return_value={}),\
                    patch.object(runner.load,'decode_save',return_value=container()),\
                    patch.object(runner,'windows_display_requirement',self.display),\
                    patch.object(runner.original_reference,'build',side_effect=self.reference),\
                    patch.object(runner.original_reference,'verify',side_effect=self.verify_reference),\
                    patch.object(runner.load,'running',spawn),patch.object(runner.load,'run',run):result=runner.run(args)
            self.assertFalse(result['ok']);self.assertEqual(spawned,[args.dosbox.name])
            self.assertTrue(result['references_unchanged'])
            self.assertIn('实际隔离存档与冻结来源快照不一致',result['error']['text'])

    def test_reference_is_verified_after_owned_cleanup_without_hiding_body_failure(self):
        for body_failure in (False,True):
            order=[]
            @contextmanager
            def spawn(command,*pos,**kw):
                config=Path(command[command.index('-conf')+1])
                config.write_text('changed during run')
                try:yield object()
                finally:order.append('cleaned')
            def verify(path):
                order.append('verify')
                return self.verify_reference(path)
            def run(args):
                config=self.original_copy(args)
                with runner.load.running([str(args.dosbox),'-conf',str(config)]):
                    if body_failure:raise AssertionError('first business failure')
                return {'ok':True}
            with self.subTest(body_failure=body_failure),tempfile.TemporaryDirectory() as directory:
                args=self.fixture(Path(directory))
                with patch.object(runner,'owned_job_required'),patch.object(runner,'source_facts',return_value={}),\
                        patch.object(runner.load,'decode_save',return_value=container()),\
                        patch.object(runner,'windows_display_requirement',self.display),\
                        patch.object(runner.original_reference,'build',side_effect=self.reference),\
                        patch.object(runner.original_reference,'verify',side_effect=verify),\
                        patch.object(runner.load,'running',spawn),patch.object(runner.load,'run',run):result=runner.run(args)
                self.assertFalse(result['ok']);self.assertFalse(result['references_unchanged'])
                self.assertEqual(order,['verify','cleaned','verify'])
                expected='first business failure' if body_failure else 'reference config changed'
                self.assertEqual(result['error']['text'],expected)
                self.assertTrue(any(row['guard']=='reference_after' for row in result['guard_errors']))

    def test_report_write_failure_preserves_first_business_error(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            with patch.object(runner,'owned_job_required'),patch.object(runner,'source_facts',return_value={}),\
                    patch.object(runner.load,'decode_save',return_value=container()),\
                    patch.object(runner,'windows_display_requirement',self.display),\
                    patch.object(runner.load,'run',return_value={'ok':False,'error':'business mismatch'}),\
                    patch.object(Path,'write_text',side_effect=OSError('report disk failure')):
                with self.assertRaisesRegex(RuntimeError,'business mismatch; report_write_error=report disk failure'):
                    runner.run(args)

    def test_first_business_failure_survives_late_guard_failure(self):
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory));previous=runner.load.running
            with patch.object(runner,'owned_job_required'),\
                    patch.object(runner,'source_facts',side_effect=[{},OSError('late guard')]),\
                    patch.object(runner.load,'decode_save',return_value=container()),\
                    patch.object(runner,'windows_display_requirement',self.display),\
                    patch.object(runner.load,'run',return_value={'ok':False,'error':'business mismatch'}):result=runner.run(args)
            self.assertFalse(result['ok']);self.assertEqual(result['error']['text'],'business mismatch')
            self.assertFalse(result['sources_unchanged']);self.assertEqual(result['guard_errors'][0]['text'],'late guard')
            self.assertIs(runner.load.running,previous)


if __name__=='__main__':unittest.main()
