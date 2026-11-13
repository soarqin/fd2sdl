"""S2机制代表的来源写集、原故事引用和实际WGC全白门负例。"""
from pathlib import Path
from contextlib import contextmanager, nullcontext
import ast
import sys
import json
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch
from PIL import Image

ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'tools'))
import run_s2_callback_check as check

class CallbackNativeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):cls.p=check.prepare(ROOT,check.SCENARIO)

    def test_source_domain_and_original_records_preserved(self):
        p=self.p;b=check.base;source=check.decode_save(ROOT/'original_game/START.SAV')
        self.assertEqual(p['source'][2],0);self.assertEqual(p['source'][b.META+1],46)
        self.assertEqual(p['source'][b.UNITS+11*80+49:b.UNITS+11*80+52],bytes((2,70,0)))
        self.assertEqual(p['source'][0x8a3:b.UNITS],source[0x8a3:b.UNITS])
        self.assertEqual(p['source'][b.UNITS+46*80:0x30a3],source[b.UNITS+46*80:0x30a3])
        for raw,identity in ((39,68),(40,97)):
            speaker=check.host.story_speaker(p['source'],identity)
            self.assertEqual((speaker['raw_actor'],speaker['visible'],speaker['unit_id']),(raw,False,identity))
        self.assertFalse(p['manifest']['production_binding_added'])

    def test_ai_write_does_not_touch_high_nibble_or_other_prefix(self):
        p=self.p;b=check.base
        expected=bytearray(b.expected_state(p['source'],'loot-story',b.item_table(p['original']/'FD2.EXE')[0])['immediate'])
        for raw in range(41,46):expected[b.UNITS+raw*80+52]&=0xf0
        self.assertEqual(p['model']['immediate'],b.checksum(expected))
        self.assertEqual({page['fragment'] for page in p['stories']},{5,6})

    def test_white_gate_requires_three_separated_complete_frames(self):
        with tempfile.TemporaryDirectory() as directory:
            frames=[]
            for i,color in enumerate((0,255,255,0,255,0,255,0)):
                path=Path(directory)/f'{i}.png';Image.new('RGB',(320,200),(color,)*3).save(path)
                frames.append({'path':str(path),'index':i,'sha256':check.sha(path),'wgc_timestamp_100ns':i+1})
            self.assertTrue(check.flash_gate(frames)['ok'])
            with self.assertRaises(AssertionError):check.flash_gate(frames[:6])
            image=Image.open(frames[6]['path']);image.putpixel((319,199),(0,0,0));image.save(frames[6]['path'])
            with self.assertRaises(AssertionError):check.flash_gate(frames)

    def test_prepare_only_does_not_start_window_or_display(self):
        with tempfile.TemporaryDirectory() as directory,patch.object(check.host,'running') as run,patch.object(check.host,'windows_display_requirement') as display:
            result=check.host.run(SimpleNamespace(root=ROOT,artifacts=Path(directory)/'new',scenario=check.SCENARIO,run=False),
                prepare_case=check.prepare,exercise_case=check.exercise,alias_message=check.ALIAS)
            self.assertEqual(result['status'],'prepared_only');self.assertFalse(result['ok'])
            run.assert_not_called();display.assert_not_called()

    def test_series_keeps_old_default_and_passes_explicit_capture_interval(self):
        import run_g2_battle_check as shared
        with tempfile.TemporaryDirectory() as directory:
            capture=SimpleNamespace(driver=Path('owned-driver'),artifacts=Path(directory),process=SimpleNamespace(pid=7))
            for interval in (None,16):
                with self.subTest(interval=interval):
                    record={};slot={}
                    helper=SimpleNamespace(start=lambda:SimpleNamespace(),finish=lambda:slot.update(summary={'ok':True}))
                    def construct(driver,command,artifacts,name,target):
                        slot['target']=target;slot['command']=command
                        helper.finish=lambda:target.update(summary={'ok':True})
                        return helper
                    with patch.object(shared,'OwnedHelper',side_effect=construct),\
                         patch.object(shared,'wait_ready',return_value={'pid':7,'hwnd':'owned'}),\
                         patch.object(shared,'series_index',return_value=[]):
                        with shared.series(capture,record,**({} if interval is None else {'interval_ms':interval})):pass
                    self.assertEqual(slot['command'][-1],33 if interval is None else interval)
                    self.assertTrue(record['continuous_wgc']['complete_raw_collection'])
            for bad in (True,0,15,68,16.0,'16'):
                with self.subTest(bad=bad),patch.object(shared,'OwnedHelper') as helper:
                    with self.assertRaises(ValueError):
                        with shared.series(capture,{},interval_ms=bad):pass
                    helper.assert_not_called()
        calls=[n for n in ast.walk(ast.parse(Path(check.__file__).read_text(encoding='utf-8')))
            if isinstance(n,ast.Call) and isinstance(n.func,ast.Name) and n.func.id=='series']
        self.assertEqual(len(calls),1)
        self.assertEqual([(k.arg,k.value.value) for k in calls[0].keywords],[('interval_ms',16)])

    def test_cli_runs_real_host_both_bootstraps_without_window(self):
        # 只替换窗口/显示/采集和业务exercise；真实CLI配置及host的两端启动分支
        # 均执行，避免prepare-only遗漏SDL分支读取capture_defeat的接口错误。
        visits=[]
        @contextmanager
        def running(command,cwd,env,log,driver,records):
            log.write_text(check.ALIAS+'\n',encoding='utf-8')
            records.append({'cleaned':True,'forced_cleanup':False,'exit_code':0})
            visits.append({'command':command,'env':dict(env)})
            yield SimpleNamespace(pid=1)
        def exercise(capture,p,save,record,args):
            self.assertIs(args.capture_defeat,False)
            snapshot=save.parent/'snapshot.SAV';snapshot.write_bytes(save.read_bytes())
            record['completed_save']={'snapshot':str(snapshot),'mtime_ns':save.stat().st_mtime_ns}
        def display(record):record['released']=True;return nullcontext()
        def copytree(source,destination):destination.mkdir();return destination
        with tempfile.TemporaryDirectory() as directory:
            root=Path(directory);dummy=root/'owned-test-executable';dummy.write_bytes(b'not launched')
            argv=['run_s2_callback_check.py','--root',str(ROOT),'--artifacts',str(root/'evidence'),
                '--sdl',str(dummy),'--driver',str(dummy),'--dosbox',str(dummy),'--run']
            with patch.object(sys,'argv',argv),patch.object(check,'prepare',return_value=self.p),\
                 patch.object(check,'exercise',side_effect=exercise),patch.object(check.host,'owned_job_required'),\
                 patch.object(check.host,'windows_display_requirement',side_effect=display),\
                 patch.object(check.host,'freeze',return_value=({str(dummy):check.sha(dummy)}, {}, [str(dummy)])),\
                 patch.object(check.host,'unchanged',return_value=True),\
                 patch.object(check.host.shutil,'copytree',side_effect=copytree),\
                 patch.object(check.host,'running',side_effect=running),patch.object(check.host,'Capture'):
                self.assertEqual(check.main(),0)
                receipt=json.loads((root/'evidence/comparison.json').read_text(encoding='utf-8'))
                self.assertEqual(receipt['input_sha256'][str(dummy)],check.sha(dummy))
                self.assertEqual(receipt['product_source_paths'],[str(dummy)])
                self.assertIn(str(root/'evidence/original/dosbox.conf'),receipt['input_sha256'])
        self.assertEqual(len(visits),2)
        self.assertIn('FD2SDL_SAVE_PATH',visits[1]['env'])
        self.assertNotIn('FD2SDL_APP_PCM_CAPTURE_DIR',visits[1]['env'])

if __name__=='__main__':unittest.main()
