"""应用mix控制/collector模型；无游戏、设备、进程启动或真实采集。"""
import copy,json,os,struct,sys,tempfile,unittest,wave
from pathlib import Path
from types import SimpleNamespace
from unittest.mock import Mock,patch
sys.path.insert(0,str(Path(__file__).resolve().parents[1]))
import fd2_app_mix_capture as a
import run_original_battle_animation as r

def ready(pid=42):
 return dict(schema=a.SCHEMA,ready=True,pid=pid,backend=a.BACKEND,clock_source=a.CLOCK,sample_rate=48000,channels=2,bits=16,source_format='F32-stereo',max_ms=45000,first_callback_qpc=100,qpc_frequency=10000000,suppress_device_output=True,device_output_validated=False,A8_validated=False)
def summary(pid=42):
 d=ready(pid);d.update(ok=True,ready_published=True,frames=4,data_bytes=16,capacity_frames=2160000,stop_reason='stop',error_code=0,error='',callback_quiescent=True,wave_finalized=True,wav='app-mix.wav',last_callback_qpc=200);return d

def make_wave(path,rate=48000):
 with wave.open(str(path),'wb') as f:f.setparams((2,2,rate,0,'NONE','none'));f.writeframes(struct.pack('<8h',100,-100,100,-100,100,-100,100,-100))

class AppMixTest(unittest.TestCase):
 def test_ready_and_summary_reject_wrong_owner_suppression_device_scope_or_failure(self):
  self.assertIs(a.validate_ready(ready(),42,45000).get('ready'),True)
  self.assertIs(a.validate_summary(summary(),ready(),42,45000)['ok'],True)
  for field,value in [('pid',43),('suppress_device_output',False),('device_output_validated',True),('A8_validated',True),('clock_source','WASAPI_packet'),('sample_rate',44100)]:
   d=ready();d[field]=value
   with self.subTest(field=field),self.assertRaises(AssertionError):a.validate_ready(d,42,45000)
  for field,value in [('ok',False),('pid',43),('frames',0),('data_bytes',15),('callback_quiescent',False),('wave_finalized',False),('stop_reason','capacity'),('error_code',7),('first_callback_qpc',101),('last_callback_qpc',99),('wav','../old.wav')]:
   d=summary();d[field]=value
   with self.subTest(field=field),self.assertRaises(AssertionError):a.validate_summary(d,ready(),42,45000)
 def test_start_publication_is_pid_exact_and_never_overwrites_existing_files(self):
  with tempfile.TemporaryDirectory() as t:
   p=Path(t)/'start';a.publish_start(p,42);self.assertEqual(p.read_bytes(),b'42\n');self.assertFalse((Path(t)/'start.request-tmp').exists())
   with self.assertRaises(FileExistsError):a.publish_start(p,99)
   self.assertEqual(p.read_bytes(),b'42\n')
   tmp=Path(t)/'other.request-tmp';tmp.write_bytes(b'KEEP')
   with self.assertRaises(FileExistsError):a.publish_start(Path(t)/'other',42)
   self.assertEqual(tmp.read_bytes(),b'KEEP')
 def test_start_and_finish_validate_file_facts_and_honest_source_scope(self):
  with tempfile.TemporaryDirectory() as t:
   d=Path(t);c=a.AppMixCapture(d,42,45000);proc=SimpleNamespace(poll=lambda:None)
   with patch.object(c,'_wait',return_value=ready()):self.assertEqual(c.start(proc),ready())
   (d/'ready.json').write_text(json.dumps(ready()));(d/'summary.json').write_text(json.dumps(summary()))
   with patch.object(c,'_wait',return_value=summary()):receipt=c.finish(proc)
   self.assertEqual(receipt['device_submission'],'same_length_silence');self.assertFalse(receipt['device_output_validated']);self.assertFalse(receipt['A8_validated'])
   self.assertEqual(receipt['summary_fact'],a.fact(d/'summary.json'));self.assertTrue((d/'stop').is_file())
 def test_target_exit_before_start_or_ready_fails_without_delivery(self):
  with tempfile.TemporaryDirectory() as t:
   c=a.AppMixCapture(Path(t),42,45000);proc=SimpleNamespace(poll=lambda:0)
   with self.assertRaises(RuntimeError):c.start(proc)
   self.assertFalse((Path(t)/'start').exists())
 def test_wav_rate_is_explicit_not_relabelled_as_old_loopback(self):
  with tempfile.TemporaryDirectory() as t:
   p=Path(t)/'app.wav';make_wave(p)
   with self.assertRaises(AssertionError):r.wav_info(p)
   self.assertEqual(r.wav_info(p,expected_rate=48000)['sample_rate'],48000)
   q=Path(t)/'loopback.wav';make_wave(q,44100);self.assertEqual(r.wav_info(q)['sample_rate'],44100)
 def test_preflight_keeps_native_and_business_without_recording_and_appmix_without_external_recorder(self):
  with tempfile.TemporaryDirectory() as t:
   d=Path(t);f=d/'bin';f.write_bytes(b'not executed')
   args=SimpleNamespace(root=d,artifacts=d/'out',scenario='physical-full-hit',sdl=None,driver=f,dosbox=f,audio_recorder=None,sdl_audio_source='app-mix')
   for sdl,scenario in ((None,'physical-full-hit'),(f,'physical-full-hit'),(f,'ai-behavior11-recover')):
    args.sdl,args.scenario=sdl,scenario
    with patch.object(r.Path,'mkdir') as mkdir,patch.object(r.subprocess,'Popen') as popen, \
         patch.object(r,'owned_job_required',side_effect=RuntimeError('job reached')) as job:
     with self.assertRaisesRegex(RuntimeError,'job reached'):r.run(args)
     job.assert_called_once();mkdir.assert_not_called();popen.assert_not_called()
   args.sdl,args.scenario=f,'physical-full-hit'
   args.sdl_audio_source='process-loopback'
   with patch.object(r,'owned_job_required') as job:
    with self.assertRaisesRegex(ValueError,'audio_recorder'):r.run(args)
    job.assert_not_called()
 def test_collector_appmix_ready_precedes_single_confirm_without_audio_helper(self):
  with tempfile.TemporaryDirectory() as t:
   events=[];proc=SimpleNamespace(pid=42,poll=lambda:None);trace=[]
   def key(name):events.append(name);trace.append({'delivery':{'hwnd':'0x1'}})
   capture=SimpleNamespace(process=proc,artifacts=Path(t),key=key,trace=trace)
   c=r.AnimationCollectors(capture,SimpleNamespace(sdl_audio_source='app-mix',driver=Path('driver')),{'kind':'sdl'})
   with patch.object(r.OwnedHelper,'start',return_value=proc) as spawn,patch.object(r,'wait_ready',side_effect=lambda *x:events.append('WGC') or {'pid':42,'hwnd':'0x1'}),patch.object(c.app_mix,'start',side_effect=lambda p:events.append('APP-MIX') or ready()):
    c.start();c.confirm()
   self.assertEqual(events,['WGC','APP-MIX','ENTER']);self.assertEqual(spawn.call_count,1);self.assertEqual(c.record['audio_ready'],ready())
 def test_collector_appmix_finish_uses_48k_and_keeps_pre_os_scope(self):
  with tempfile.TemporaryDirectory() as t:
   d=Path(t);(d/'app-mix').mkdir();(d/'animation-series').mkdir();proc=SimpleNamespace(pid=42,poll=lambda:None)
   record={'kind':'sdl','series_helper':{'summary':{'ok':True}},'animation_capture':{}};capture=SimpleNamespace(process=proc,artifacts=d)
   c=r.AnimationCollectors(capture,SimpleNamespace(sdl_audio_source='app-mix'),record)
   c.ready={'pid':42,'hwnd':'0x1'};c.helpers=[];(d/'animation-series/summary.json').write_text('{"ok":true}')
   make_wave(d/'app-mix/app-mix.wav');receipt={'summary':summary(),'device_output_validated':False}
   with patch.object(c.app_mix,'finish',return_value=receipt),patch.object(r,'series_index',return_value=[]):c.stop()
   self.assertEqual(record['audio_capture']['source_scope'],'app_mix');self.assertEqual(record['audio_capture']['wav']['sample_rate'],48000)
   self.assertNotIn('recorder_summary',record['audio_capture']);self.assertNotIn('first_packet_qpc_100ns',record['audio_capture']['app_mix_summary']);self.assertFalse(record['audio_capture']['A8_validated'])
 def test_summary_late_change_remains_sticky_failure(self):
  with tempfile.TemporaryDirectory() as t:
   d=Path(t);original=d/'original';original.mkdir();(original/'asset').write_bytes(b'asset');fixture=d/'fixture';fixture.write_bytes(b'save');save=d/'save';save.write_bytes(b'save')
   x=d/'ready.json';y=d/'summary.json';x.write_text('{}');y.write_text('{}')
   record={'kind':'sdl','collection_passed':True,'business_sequence_completed':True,'collection_completed':True,'processes':[{'cleaned':True,'forced_cleanup':False,'exit_code':0}],
       'app_mix_capture':{'ready_fact':a.fact(x),'summary_fact':a.fact(y)}}
   args=(record,original,r.fingerprint(original),fixture,b'save',fixture.stat().st_mtime_ns,save,[b'save',save.stat().st_mtime_ns],{},{} )
   r.endpoint_guard(*args);self.assertTrue(record['guards']['app_mix_artifacts_unchanged'])
   y.write_text('{"changed":true}');r.endpoint_guard(*args);self.assertFalse(record['guards']['app_mix_artifacts_unchanged']);self.assertFalse(record['collection_passed'])

if __name__=='__main__':unittest.main()
