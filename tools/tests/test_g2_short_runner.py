"""SHORT入口/同一快照/真实阶段ACK的负例；不启动窗口。"""
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'tools'))
import fd2_g2_battle_oracle as common
import run_g2_short_check as runner


def plain(value):
    data=bytearray(common.SAVE_SIZE);data[common.META+1]=1;data[common.UNITS+7]=value
    return common.checksum(data)


class ShortRunnerTest(unittest.TestCase):
    def test_runner_has_no_window_resize_or_foreground_api(self):
        import ast
        source=Path(runner.__file__).read_text(encoding='utf-8')
        tree=ast.parse(source)
        imports={alias.name for node in ast.walk(tree) if isinstance(node,(ast.Import,ast.ImportFrom))
            for alias in node.names}
        self.assertFalse(imports & {'ctypes','threading','require_owned_window'})
        forbidden={'SetWindowPos','SetForegroundWindow','SetActiveWindow','SetFocus','ShowWindow',
            'SetWindowLongPtrW','SetThreadDpiAwarenessContext','SendInput','WinDLL',
            'resize_owned_sdl_client','_resize_owned_sdl_client','_resize_winapi','require_resized_wgc'}
        names={node.id for node in ast.walk(tree) if isinstance(node,ast.Name)}
        names.update(node.attr for node in ast.walk(tree) if isinstance(node,ast.Attribute))
        self.assertFalse(names & forbidden)
        self.assertNotIn('640',source)

    def test_prepare_only_is_ready_not_native_success(self):
        with tempfile.TemporaryDirectory(prefix='fd2-short-prepare-') as directory:
            output=Path(directory)/'new'
            args=SimpleNamespace(root=ROOT,artifacts=output,prepare_only=True)
            with mock.patch.object(runner,'owned_job_required') as job, mock.patch.object(runner,'running') as run:
                report=runner.run(args)
            job.assert_not_called();run.assert_not_called()
            self.assertFalse(report['ok']);self.assertEqual(report['status'],'ready_not_executed')
            self.assertEqual(report['cases'],[]);self.assertEqual(report['animation_capture_interval_ms'],16)
            self.assertEqual(report['sdl_client_default'],[960,600]);self.assertNotIn('sdl_client_requested',report)
            self.assertTrue((output/'controlled.SAV').is_file())
            self.assertIn(str(ROOT/'tools/fd2_g2_short_oracle.py'),report['inputs'])
            self.assertIn(str(ROOT/'tools/run_original_focus.py'),report['inputs'])

    def test_existing_output_is_not_reused(self):
        with tempfile.TemporaryDirectory(prefix='fd2-short-existing-') as directory:
            with self.assertRaises(ValueError):runner.run(SimpleNamespace(root=ROOT,artifacts=Path(directory),prepare_only=True))

    def test_save_binds_checked_plain_to_exact_encoded_snapshot(self):
        for mismatch in (False,True):
            with self.subTest(mismatch=mismatch),tempfile.TemporaryDirectory(prefix='fd2-short-save-') as directory:
                path=Path(directory);save=path/'FD2.SAV';initial=common.crypt(plain(1));save.write_bytes(initial)
                guard=[initial,save.stat().st_mtime_ns];checked=plain(2);actual=common.crypt(plain(3) if mismatch else checked)
                capture=SimpleNamespace(artifacts=path,key=mock.Mock())
                prepared={'assets':{},'source':b'','original':Path('unused')};record={'kind':'sdl'}
                view=SimpleNamespace(**{name:mock.Mock() for name in ('closed_browse','menu','secondary','buttons')})
                def finish(*_):save.write_bytes(actual);return checked
                with mock.patch.object(runner,'RoundOracle',return_value=view),mock.patch.object(runner,'screen',return_value=(28,28)),\
                    mock.patch.object(runner,'wait_measured_save',side_effect=finish),\
                    mock.patch.object(runner.oracle,'check_result',return_value={'ok':True,'hero_hp':31}) as gate:
                    if mismatch:
                        with self.assertRaisesRegex(AssertionError,'同一份encoded'):runner.save_measurement(capture,prepared,save,guard,record,mock.Mock())
                        gate.assert_not_called();self.assertNotIn('completed_save',record)
                    else:
                        self.assertEqual(runner.save_measurement(capture,prepared,save,guard,record,mock.Mock()),31)
                        self.assertEqual(Path(record['completed_save']['path']).read_bytes(),actual)
                        self.assertEqual(gate.call_args.args[2],checked)
                        self.assertEqual(guard,[actual,save.stat().st_mtime_ns])


if __name__=='__main__':unittest.main()
