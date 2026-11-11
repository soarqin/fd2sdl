"""G5 null正式入口的纯协议/原资源/失败门；不启动任何窗口。"""
from contextlib import contextmanager
import os
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import Mock,patch

from PIL import Image

TOOLS=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(TOOLS))
import run_g5_null_check as runner
from run_original_battle_menu import MENU_OFFSETS,PARENT_IDS

ROOT=TOOLS.parent
VALID_LOG='\n'.join((
    'audio: null backend, 48000 Hz, FDOTHER[31]=13, [77]=4, [78]=1, [80]=16, [95]=1 samples',
    'music request: stage=0 track=19 reason=continue available=1 result=0 current=19 loop=0',
    'field play: stage=0, map=24x24, units=12, camera=(5,11), cursor=(8,15), turn=1, phase=2, handoff=no',
    'field input ready: stage=0 turn=1 phase=2'))


class NativeNullTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.prepared=runner.prepare(ROOT/'original_game')

    def test_formal_Load_consumes_exact_exit_phases_without_changing_null_business(self):
        for y,wrong_turn in ((92,False),(97,False),(104,False),(169,False),(93,False),(92,True)):
            with self.subTest(y=y,wrong_turn=wrong_turn),tempfile.TemporaryDirectory() as directory:
                work=Path(directory);save=work/'FD2.SAV';save.write_bytes(self.prepared['encoded'])
                capture=Mock(artifacts=work);record={'checkpoints':[]};seen=[];stopped=[]
                turn=self.prepared['source'][runner.META]
                template=runner.load_round_template(ROOT/'original_game',self.prepared['assets']['palette'],
                    turn+1 if wrong_turn else turn,exiting=True)
                image=Image.new('RGB',(320,200));image.paste(template.convert('RGB'),(120,y))
                @contextmanager
                def series(*args):
                    try:yield {},[]
                    finally:stopped.append(True)
                def checkpoint(capture,scope,name,predicate,points):
                    seen.append(name)
                    if name!='continued-actual-round':raise TimeoutError('fixture stops after real Round predicate')
                    found=predicate(image)
                    if not found:raise TimeoutError('full source Round rejected')
                    self.assertEqual((found['phase'],found['digit_relative_y'],found['position'],found['turn']),
                        ('exit',6,[120,y],turn))
                    return image,found
                with patch.object(runner,'modal_series',series),patch.object(runner,'series_checkpoint',side_effect=checkpoint), \
                        patch.object(runner,'wait_title'),patch.object(runner,'driver_call') as close:
                    with self.assertRaises(TimeoutError):runner.exercise(capture,self.prepared,save,record)
                self.assertEqual(seen,['continued-actual-round'] if wrong_turn or y==93 else
                    ['continued-actual-round','continued-actual-browse'])
                self.assertEqual([c.args[0] for c in capture.key.call_args_list],['UP','ENTER'])
                self.assertEqual(stopped,[True]);close.assert_not_called()
                self.assertEqual(save.read_bytes(),self.prepared['encoded'])

    def frame(self,kind,focus=None):
        p=self.prepared;focus=focus or p['steps'][-1][1]
        frame=Image.new('RGB',(320,200),(11,17,23))
        if kind=='round':frame.paste(p['round'].convert('RGB'),(120,84));return frame
        frame.paste(p['assets']['cursor'].convert('RGB'),focus)
        if kind=='menu':
            for index,command in enumerate(PARENT_IDS):
                at=tuple(a+b for a,b in zip(focus,MENU_OFFSETS[index]))
                frame.paste(p['assets']['commands'][command*3+(index==0)].convert('RGB'),at)
        return frame

    def test_fixture_is_whole_original_START_and_bounded_empty_navigation(self):
        p=self.prepared
        self.assertEqual(p['encoded'],(ROOT/'original_game/START.SAV').read_bytes())
        self.assertEqual(len(p['encoded']),22987)
        self.assertEqual(p['focus'],(76,100))
        self.assertEqual(p['steps'],[('LEFT',(52,100))])
        self.assertTrue(p['manifest']['whole_original_START_without_patches'])
        self.assertFalse(p['manifest']['debug_field_entry'])

    def test_browse_rejects_partial_Load_dialog_and_real_menu(self):
        p=self.prepared;focus=p['steps'][-1][1]
        frame=self.frame('browse')
        self.assertIsNotNone(runner.browse_state(frame,p,focus))
        for y in (112,125,138,151,164,177):
            partial=frame.copy();partial.paste(p['assets']['dialog_top'].convert('RGB'),(5,y))
            self.assertIsNone(runner.browse_state(partial,p,focus),y)
        self.assertIsNone(runner.browse_state(self.frame('menu'),p,focus))
        self.assertIsNone(runner.browse_state(Image.new('RGB',(320,200)),p,focus))

    def test_log_requires_real_null_PCM_and_continue_not_old_or_dummy_claim(self):
        found=runner.validate_log(VALID_LOG)
        self.assertEqual(found['PCM_bank_sample_counts'],[13,4,1,16,1])
        for wrong in (VALID_LOG.replace('null backend','SDL device'),
                VALID_LOG+'\n'+VALID_LOG.splitlines()[0],
                VALID_LOG.replace('[80]=16','[80]=0'),
                VALID_LOG.replace('reason=continue available=1','reason=entry available=1'),
                VALID_LOG.replace('units=12','units=11'),
                VALID_LOG.replace('handoff=no','handoff=no, once'),
                VALID_LOG+'\nfield action: unit=0 command=wait',
                VALID_LOG+'\nfield session stopped: reason=4'):
            with self.subTest(wrong=wrong),self.assertRaises(AssertionError):runner.validate_log(wrong)

    def test_environment_removes_dummy_and_capture_overrides_without_mixer_change(self):
        inherited={'SDL_VIDEODRIVER':'dummy','SDL_AUDIODRIVER':'dummy','FD2SDL_FIELD_RNG_SEED':'5',
            'FD2SDL_APP_PCM_CAPTURE_DIR':'elsewhere','FD2SDL_APP_PCM_CAPTURE_QUIET':'0',
            'FD2SDL_SAVE_PATH':'user-save','FD2SDL_TEST_AUDIO_QUIET':'0','PATH':'kept'}
        before=inherited.copy();environment,removed=runner.native_environment(inherited,Path('isolated.SAV'))
        self.assertEqual(inherited,before)
        self.assertNotIn('SDL_VIDEODRIVER',environment)
        self.assertNotIn('FD2SDL_FIELD_RNG_SEED',environment)
        self.assertFalse(any(key.startswith('FD2SDL_APP_PCM_CAPTURE_') for key in environment))
        self.assertEqual(environment['SDL_AUDIODRIVER'],runner.INVALID_AUDIO_DRIVER)
        self.assertEqual(environment['FD2SDL_SAVE_PATH'],str(Path('isolated.SAV').resolve()))
        self.assertEqual(environment['FD2SDL_TEST_AUDIO_QUIET'],'1')
        self.assertEqual(environment['PATH'],'kept')
        self.assertEqual(set(removed),set(inherited)-{'PATH'})

    def test_formal_sequence_Round_then_browse_menu_Esc_and_normal_close(self):
        for fail in (None,'round','save'):
            with self.subTest(fail=fail),tempfile.TemporaryDirectory() as directory:
                folder=Path(directory);save=folder/'FD2.SAV';save.write_bytes(self.prepared['encoded'])
                capture=Mock(artifacts=folder);capture.process.pid=81;capture.process.wait.return_value=0
                record={'checkpoints':[]};order=[];states=[]
                @contextmanager
                def series(*args):
                    states.append(('ready',[c.args[0] for c in capture.key.call_args_list]))
                    try:yield {},record['checkpoints']
                    finally:states.append(('stop',[c.args[0] for c in capture.key.call_args_list]))
                def checkpoint(capture,scope,name,predicate,points):
                    order.append(name)
                    if name=='continued-actual-round':
                        image=self.frame('round' if fail!='round' else 'browse')
                    elif name=='continued-actual-browse':image=self.frame('browse',self.prepared['focus'])
                    elif name=='formal-SYSTEM-menu':image=self.frame('menu')
                    else:image=self.frame('browse')
                    found=predicate(image)
                    if not found:raise TimeoutError('expected actual resource: '+name)
                    if fail=='save' and name=='continued-actual-round':save.write_bytes(b'x'+save.read_bytes()[1:])
                    return image,found
                with patch.object(runner,'modal_series',series),patch.object(runner,'series_checkpoint',side_effect=checkpoint), \
                        patch.object(runner,'wait_title'),patch.object(runner,'driver_call') as close:
                    if fail:
                        with self.assertRaises((TimeoutError,AssertionError)):
                            runner.exercise(capture,self.prepared,save,record)
                        self.assertEqual([c.args[0] for c in capture.key.call_args_list],['UP','ENTER'])
                        close.assert_not_called()
                    else:
                        runner.exercise(capture,self.prepared,save,record)
                        self.assertEqual([c.args[0] for c in capture.key.call_args_list],['UP','ENTER','LEFT','ENTER','ESC'])
                        self.assertEqual(order,['continued-actual-round','continued-actual-browse',
                            'empty-focus-0','formal-SYSTEM-menu','cancelled-actual-browse'])
                        close.assert_called_once_with(capture.driver,'close',capture.process)
                        self.assertEqual(save.read_bytes(),self.prepared['encoded'])
                        self.assertEqual(record['host_close']['exit_code'],0)
                    self.assertEqual(states[0],('ready',['UP']))
                    self.assertEqual(states[-1][0],'stop')

    def test_prepare_only_has_no_window_display_or_job_calls(self):
        with tempfile.TemporaryDirectory() as directory:
            args=SimpleNamespace(root=ROOT,artifacts=Path(directory)/'prepared',run=False,sdl=None,driver=None)
            with patch.object(runner,'owned_job_required') as job,patch.object(runner,'running') as windows, \
                    patch.object(runner,'windows_display_requirement') as display:
                report=runner.run(args)
            job.assert_not_called();windows.assert_not_called();display.assert_not_called()
            self.assertEqual(report['status'],'prepared_only');self.assertFalse(report['ok'])
            self.assertEqual((args.artifacts/'fixture.SAV').read_bytes(),self.prepared['encoded'])
            self.assertFalse(report['processes']);self.assertFalse(report['paired_result'])

    def test_owned_cleanup_guard_release_late_guard_order_and_first_error(self):
        for fail in (None,'body','guard','body_and_guard','late_guard'):
            with self.subTest(fail=fail),tempfile.TemporaryDirectory() as directory:
                root=Path(directory)/'root';original=root/'original_game';original.mkdir(parents=True)
                (original/'START.SAV').write_bytes(self.prepared['encoded'])
                (original/'FDOTHER.DAT').write_bytes(b'isolated-resource-protocol-fixture')
                app=root/'fd2sdl.exe';driver=root/'driver.exe';app.write_bytes(b'app');driver.write_bytes(b'driver')
                args=SimpleNamespace(root=root,artifacts=root/'evidence',run=True,sdl=app,driver=driver)
                order=[];active=False;closed=False
                @contextmanager
                def display(record):
                    nonlocal active
                    active=True;order.append('display-acquire')
                    try:yield
                    finally:
                        order.append('display-release');active=False;record['released']=True
                        if fail=='late_guard':(args.artifacts/'runtime/FD2.SAV').write_bytes(b'late-corruption')
                @contextmanager
                def windows(command,cwd,environment,log,driver,receipts):
                    nonlocal closed
                    self.assertTrue(active);order.append('window-enter')
                    receipt={'pid':31,'cleaned':False,'forced_cleanup':False,'exit_code':None};receipts.append(receipt)
                    process=Mock();process.pid=31
                    try:yield process
                    finally:
                        receipt.update(cleaned=True,exit_code=0);closed=True;order.append('owned-cleanup')
                def exercise(capture,p,save,record):
                    (args.artifacts/'game.log').write_text(VALID_LOG,encoding='utf-8')
                    if fail in ('guard','body_and_guard'):save.write_bytes(b'body-corruption')
                    if fail in ('body','body_and_guard'):raise ValueError('primary native-body failure')
                def source_guard(*args):
                    order.append('source-pre-release' if active and closed else
                        'source-after-release' if not active else 'source-before-window')
                    if closed and fail=='body_and_guard':raise OSError('secondary source-guard failure')
                    return True
                with patch.object(runner,'prepare',return_value=self.prepared), \
                        patch.object(runner,'project_save_guards',return_value={}),patch.object(runner,'owned_job_required'), \
                        patch.object(runner,'freeze',return_value=({}, {}, [])), \
                        patch.object(runner,'unchanged',side_effect=source_guard), \
                        patch.object(runner,'windows_display_requirement',display),patch.object(runner,'running',windows), \
                        patch.object(runner,'exercise',side_effect=exercise):
                    report=runner.run(args)
                self.assertEqual(order,['display-acquire','source-before-window','window-enter','owned-cleanup',
                    'source-pre-release','display-release','source-after-release'])
                self.assertTrue(report['guards_before_display_release']['owned_process_cleanup_complete'])
                self.assertEqual(report['guards_before_display_release']['owned_process_count'],1)
                self.assertTrue(report['display_requirement']['released'])
                self.assertEqual(report['ok'],fail is None)
                self.assertEqual(report['status'],'passed' if fail is None else 'failed')
                if fail in ('body','body_and_guard'):
                    self.assertEqual(report['error'],{'type':'ValueError','message':'primary native-body failure'})
                if fail=='body_and_guard':
                    self.assertEqual(report['guards_before_display_release']['errors'][0]['type'],'OSError')
                    self.assertEqual(report['guards_after_display_release']['errors'][0]['type'],'OSError')
                if fail=='late_guard':
                    self.assertTrue(report['guards_before_display_release']['ok'])
                    self.assertFalse(report['guards_after_display_release']['whole_22987B_Save_and_mtime_unchanged'])

    def test_Save_guard_detects_bytes_mtime_creation_and_deletion(self):
        with tempfile.TemporaryDirectory() as directory:
            path=Path(directory)/'FD2.SAV';guard={str(path):None}
            self.assertTrue(runner.saves_unchanged(guard))
            path.write_bytes(b'original');self.assertFalse(runner.saves_unchanged(guard))
            guard={str(path):runner.file_fact(path)};self.assertTrue(runner.saves_unchanged(guard))
            stamp=path.stat();os.utime(path,ns=(stamp.st_atime_ns,stamp.st_mtime_ns+1_000_000_000))
            self.assertEqual(path.read_bytes(),b'original');self.assertFalse(runner.saves_unchanged(guard))
            guard={str(path):runner.file_fact(path)}
            path.write_bytes(b'changed');self.assertFalse(runner.saves_unchanged(guard))
            path.unlink();self.assertFalse(runner.saves_unchanged(guard))


if __name__=='__main__':unittest.main()
