"""F2移动代表的宿主边界测试；不打开窗口、调用设备或执行游戏。"""
from contextlib import contextmanager
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import run_f2_motion_check as runner


class F2MotionCheckTest(unittest.TestCase):
    def fixture(self,root):
        original=root/'original_game';original.mkdir()
        (original/'START.SAV').write_bytes(b'fixture')
        for name in ('game.exe','driver.exe','dosbox.exe','bootstrap.conf'):
            (root/name).write_bytes(name.encode())
        return SimpleNamespace(root=root,artifacts=root/'out',game=root/'game.exe',
            driver=root/'driver.exe',dosbox=root/'dosbox.exe',bootstrap=root/'bootstrap.conf')

    def test_display_failure_stops_before_player_and_restores_owner(self):
        @contextmanager
        def off(record,*,require_display_on):
            self.assertIs(require_display_on,True)
            raise RuntimeError('display OFF')
            yield
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory));previous=runner.player.running
            with patch.object(runner,'owned_job_required'),patch.object(runner,'source_facts',return_value={}),\
                    patch.object(runner,'windows_display_requirement',off),patch.object(runner.player,'run') as game:
                result=runner.run(args)
            self.assertFalse(result['ok']);self.assertEqual(result['error']['text'],'display OFF')
            game.assert_not_called();self.assertIs(runner.player.running,previous)

    def test_reference_gate_precedes_spawn(self):
        order=[]
        @contextmanager
        def display(record,*,require_display_on):
            self.assertIs(require_display_on,True)
            yield
        @contextmanager
        def spawn(command,*pos,**kw):
            order.append('spawn');yield object()
        def check(*args):
            order.append('reference');raise ValueError('wrong original identity')
        def player(args):
            config=args.artifacts/'original/dosbox.conf';config.parent.mkdir(parents=True)
            config.write_text('actual config')
            with runner.player.running([str(args.dosbox),'-conf',str(config)]):
                self.fail('reference failure must stop before body')
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory))
            with patch.object(runner,'owned_job_required'),patch.object(runner,'source_facts',return_value={}),\
                    patch.object(runner,'windows_display_requirement',display),\
                    patch.object(runner.player,'running',spawn),patch.object(runner.player,'run',player),\
                    patch.object(runner.original_reference,'build',check):
                result=runner.run(args)
            self.assertEqual(order,['reference']);self.assertFalse(result['ok'])
            self.assertEqual(result['error']['text'],'wrong original identity')

    def test_player_failure_survives_late_guard_error_and_seed_is_restored(self):
        @contextmanager
        def display(record,*,require_display_on):
            self.assertIs(require_display_on,True)
            yield
        with tempfile.TemporaryDirectory() as directory:
            args=self.fixture(Path(directory));previous=runner.player.running
            with patch.dict(os.environ,{'FD2SDL_FIELD_RNG_SEED':'0x1234'}),\
                    patch.object(runner,'owned_job_required'),\
                    patch.object(runner,'source_facts',side_effect=[{},OSError('late guard')]),\
                    patch.object(runner,'windows_display_requirement',display),\
                    patch.object(runner.player,'run',return_value={'ok':False,'error':'action mismatch'}):
                result=runner.run(args)
                self.assertEqual(os.environ['FD2SDL_FIELD_RNG_SEED'],'0x1234')
            self.assertIs(runner.player.running,previous)
            self.assertFalse(result['ok']);self.assertFalse(result['sources_unchanged'])
            self.assertEqual(result['error']['text'],'action mismatch')
            self.assertEqual(result['guard_errors'][0]['text'],'late guard')


if __name__=='__main__':unittest.main()
