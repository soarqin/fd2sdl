"""一次应用mix取证控制；无播放器/设备操作，不代表WASAPI或扬声器输出。"""
from __future__ import annotations
import hashlib
import json
import os
from pathlib import Path
import time

BACKEND = 'fd2-audio-pre-os-session'
CLOCK = 'SDL_performance_counter_after_main_thread_successful_Put'
SCHEMA = 'fd2-app-mix-capture-v3'
PREVIOUS_SCHEMA = 'fd2-app-mix-capture-v2'
PCM_QUANTIZER = 's16-unit32768-round-half-away-saturate'
LEGACY_CLOCK = 'SDL_performance_counter_at_real_mix_callback'
LEGACY_SCHEMA = 'fd2-app-mix-capture-v1'

def capture_clock_fields(row):
    """历史callback/Put和当前精确S16载体各保身份，不按缺字段回退。"""
    schema=row.get('schema')
    if schema==LEGACY_SCHEMA:
        if (row.get('clock_source')!=LEGACY_CLOCK or
            any(key in row for key in ('transport_owner','capture_point_version','capture_point',
                                      'first_submission_qpc','last_submission_qpc','submission_quiescent',
                                      'pcm_quantizer','pcm_quantizer_version'))):
            raise AssertionError('v1 callback身份混入新submission字段')
        return ('first_callback_qpc','last_callback_qpc','callback_quiescent')
    if schema in (PREVIOUS_SCHEMA,SCHEMA):
        if (row.get('clock_source')!=CLOCK or row.get('transport_owner')!='main-thread' or
            type(row.get('capture_point_version')) is not int or row['capture_point_version']!=2 or
            row.get('capture_point')!='after_successful_SDL_PutAudioStreamData' or
            any(key in row for key in ('first_callback_qpc','last_callback_qpc','callback_quiescent'))):
            raise AssertionError('v2/v3需要主线程successful Put身份，不能混用callback记录')
        if schema==PREVIOUS_SCHEMA:
            if any(key in row for key in ('pcm_quantizer','pcm_quantizer_version')):
                raise AssertionError('历史v2不能混入v3量化身份')
        elif (row.get('pcm_quantizer')!=PCM_QUANTIZER or
              type(row.get('pcm_quantizer_version')) is not int or row['pcm_quantizer_version']!=1):
            raise AssertionError('v3需要明确32768单位、half-away及饱和量化身份')
        return ('first_submission_qpc','last_submission_qpc','submission_quiescent')
    raise AssertionError('未知app-mix schema')

def first_qpc(row):
    first,_,_=capture_clock_fields(row)
    if type(row.get(first)) is not int or row[first]<=0:
        raise AssertionError('app-mix首个已声明观察点QPC无效')
    return row[first]

def fact(path):
    path=Path(path);s=path.stat()
    return {'path':str(path),'sha256':hashlib.sha256(path.read_bytes()).hexdigest(),
            'size':s.st_size,'mtime_ns':s.st_mtime_ns}

def validate_ready(row,pid,max_ms):
    first,_,_=capture_clock_fields(row)
    if (row.get('ready') is not True or row.get('pid')!=pid or
        row.get('backend')!=BACKEND or
        (row.get('sample_rate'),row.get('channels'),row.get('bits'))!=(48000,2,16) or
        row.get('source_format')!='F32-stereo' or row.get('max_ms')!=max_ms or
        row.get('suppress_device_output') is not True or row.get('device_output_validated') is not False or
        row.get('A8_validated') is not False or type(row.get(first)) is not int or
        row[first]<=0 or type(row.get('qpc_frequency')) is not int or row['qpc_frequency']<=0):
        raise AssertionError('app-mix ready的PID/明确观察点/quiet/格式/证据层级不符')
    return row

def validate_summary(row,ready,pid,max_ms):
    validate_ready(ready,pid,max_ms)
    first,last,quiescent=capture_clock_fields(row)
    frames=row.get('frames');capacity=48000*max_ms//1000
    if (row.get('schema')!=ready['schema'] or row.get('ok') is not True or row.get('pid')!=pid or
        row.get('backend')!=BACKEND or row.get('clock_source')!=ready['clock_source'] or
        row.get('ready_published') is not True or row.get('suppress_device_output') is not True or
        row.get('device_output_validated') is not False or row.get('A8_validated') is not False or
        (row.get('sample_rate'),row.get('channels'),row.get('bits'))!=(48000,2,16) or
        row.get('source_format')!='F32-stereo' or row.get('max_ms')!=max_ms or
        row.get('capacity_frames')!=capacity or type(frames) is not int or not 0<frames<=capacity or
        row.get('data_bytes')!=frames*4 or row.get('stop_reason')!='stop' or
        row.get('error_code')!=0 or row.get('error')!='' or row.get(quiescent) is not True or
        row.get('wave_finalized') is not True or row.get('wav')!='app-mix.wav' or
        type(row.get(first)) is not int or row.get(first)!=first_qpc(ready) or
        type(row.get(last)) is not int or row[last]<row[first] or
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
        if self.ready['schema']!=SCHEMA:
            raise AssertionError('本轮runtime要求v3；v1/v2仅作历史证据读取，不重新签身份')
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
        if self.ready['schema']!=SCHEMA:raise AssertionError('本轮runtime不能封装历史v1/v2身份')
        validate_summary(summary,self.ready,self.pid,self.max_ms)
        return {'ready':self.ready,'summary':summary,'ready_fact':fact(self.directory/'ready.json'),
            'summary_fact':fact(self.directory/'summary.json'),'source_scope':'application_mix_observed_after_successful_Put_before_OS_session',
            'capture_point':summary['capture_point'],'transport_owner':summary['transport_owner'],
            'pcm_quantizer':summary['pcm_quantizer'],'pcm_quantizer_version':summary['pcm_quantizer_version'],
            'device_submission':'same_length_silence','suppress_device_output':True,
            'device_output_validated':False,'A8_validated':False}
