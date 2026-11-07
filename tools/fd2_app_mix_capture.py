"""一次应用mix取证控制；无播放器/设备操作，不代表WASAPI或扬声器输出。"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import time

BACKEND = 'fd2-audio-pre-os-session'
CLOCK = 'SDL_performance_counter_at_real_mix_callback'
SCHEMA = 'fd2-app-mix-capture-v1'

def fact(path):
    path=Path(path);s=path.stat()
    return {'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'size':s.st_size,'mtime_ns':s.st_mtime_ns}

def validate_ready(row,pid,max_ms):
    if (row.get('schema')!=SCHEMA or row.get('ready') is not True or row.get('pid')!=pid or
        row.get('backend')!=BACKEND or row.get('clock_source')!=CLOCK or
        (row.get('sample_rate'),row.get('channels'),row.get('bits'))!=(48000,2,16) or
        row.get('source_format')!='F32-stereo' or row.get('max_ms')!=max_ms or
        row.get('suppress_device_output') is not True or row.get('device_output_validated') is not False or
        row.get('A8_validated') is not False or type(row.get('first_callback_qpc')) is not int or
        row['first_callback_qpc']<=0 or type(row.get('qpc_frequency')) is not int or row['qpc_frequency']<=0):
        raise AssertionError('app-mix ready的PID/真实callback/quiet/格式/证据层级不符')
    return row

def validate_summary(row,ready,pid,max_ms):
    validate_ready(ready,pid,max_ms)
    frames=row.get('frames');capacity=48000*max_ms//1000
    if (row.get('schema')!=SCHEMA or row.get('ok') is not True or row.get('pid')!=pid or
        row.get('backend')!=BACKEND or row.get('clock_source')!=CLOCK or
        row.get('ready_published') is not True or row.get('suppress_device_output') is not True or
        row.get('device_output_validated') is not False or row.get('A8_validated') is not False or
        (row.get('sample_rate'),row.get('channels'),row.get('bits'))!=(48000,2,16) or
        row.get('source_format')!='F32-stereo' or row.get('max_ms')!=max_ms or
        row.get('capacity_frames')!=capacity or type(frames) is not int or not 0<frames<=capacity or
        row.get('data_bytes')!=frames*4 or row.get('stop_reason')!='stop' or
        row.get('error_code')!=0 or row.get('error')!='' or row.get('callback_quiescent') is not True or
        row.get('wave_finalized') is not True or row.get('wav')!='app-mix.wav' or
        row.get('first_callback_qpc')!=ready['first_callback_qpc'] or
        type(row.get('last_callback_qpc')) is not int or row['last_callback_qpc']<row['first_callback_qpc'] or
        row.get('qpc_frequency')!=ready['qpc_frequency']):
        raise AssertionError('app-mix未显式stop/静止/完整封包，或scope/owner/格式错误')
    return row

def publish_start(path,pid):
    path=Path(path);tmp=path.with_name(path.name+'.request-tmp')
    created=False
    try:
        with tmp.open('x',encoding='ascii',newline='\n') as stream:
            created=True
            stream.write(str(pid)+'\n');stream.flush();os.fsync(stream.fileno())
        os.link(tmp,path)  # 原子且不覆盖；同目录的本地文件，不复用旧请求。
    finally:
        if created and tmp.exists():tmp.unlink()

class AppMixCapture:
    def __init__(self,directory,pid,max_ms):
        self.directory=Path(directory);self.pid=pid;self.max_ms=max_ms;self.ready=None
        if type(pid) is not int or pid<=0 or type(max_ms) is not int or not 0<max_ms<=120000:
            raise ValueError('app-mix需要本轮PID和有界时限')
        self.wav=self.directory/'app-mix.wav';self.stop_path=self.directory/'stop'
    def _wait(self,name,process):
        deadline=time.monotonic()+5;path=self.directory/name
        while not path.is_file():
            if process.poll() is not None:raise RuntimeError('app-mix封包前本轮目标退出')
            summary=self.directory/'summary.json'
            if name=='ready.json' and summary.is_file():
                raise AssertionError('app-mix未ready: '+summary.read_text(encoding='utf-8'))
            if time.monotonic()>=deadline:raise TimeoutError('app-mix等待'+name+'超时')
            time.sleep(.01)
        return json.loads(path.read_text(encoding='utf-8'))
    def start(self,process):
        if process.poll() is not None:raise RuntimeError('app-mix目标已退出，未发start')
        publish_start(self.directory/'start',self.pid)
        self.ready=validate_ready(self._wait('ready.json',process),self.pid,self.max_ms)
        return self.ready
    def assert_running(self,process):
        if process.poll() is not None:raise RuntimeError('app-mix本轮目标已退出')
        if (self.directory/'summary.json').exists():raise RuntimeError('app-mix在动作完成前提前停止')
    def request_stop(self):
        if self.directory.is_dir() and not self.stop_path.exists():
            with self.stop_path.open('x',encoding='ascii') as stream:stream.write('stop\n')
    def finish(self,process):
        self.request_stop()
        summary=self._wait('summary.json',process)
        if self.ready is None:raise AssertionError('app-mix没有本轮已验证ready')
        validate_summary(summary,self.ready,self.pid,self.max_ms)
        return {'ready':self.ready,'summary':summary,'ready_fact':fact(self.directory/'ready.json'),
            'summary_fact':fact(self.directory/'summary.json'),'source_scope':'application_mix_before_explicit_test_suppression_and_OS_session',
            'device_submission':'same_length_silence','suppress_device_output':True,
            'device_output_validated':False,'A8_validated':False}
