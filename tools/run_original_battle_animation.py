#!/usr/bin/env python3
"""B6受控真实输入/SAV及持续WGC/进程音频采集，原版先行、可选SDL。

默认仅collection/probe且ok=false；--verify-core仅接受核心三场双端。
严格入口在全部清理/晚守卫后核最低完整帧与独立VOICE身份/顺序，不声称A7。
15311/2ff01/30e9d/1548e的原语义见同目录oracle；不改原目录/EXE/运行内存。
"""
import argparse
import array
import ctypes
import hashlib
import io
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import time
import wave
from contextlib import contextmanager
from fd2_display_requirement import windows_display_requirement

from PIL import Image

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
import fd2_battle_animation_oracle as oracle
import run_original_reward as reward
import fd2_battle_animation_frames as frame_oracle
import fd2_sfx_oracle as sfx_oracle
import fd2_battle_animation_sfx as sfx_declaration
import fd2_core_sfx_gate as core_sfx
from fd2_app_mix_capture import AppMixCapture
from fd2_item_list_oracle import inventory_templates
from fd2_reward_oracle import magic_page
from run_graphics_tests import decode_save
from run_original_comparison import Capture, archive_entry, normalize, running, title_templates
from run_original_battle_menu import wait_state, wait_title
from run_original_end_turn import META, STRIDE, UNITS, RoundOracle, round_assets, round_two_template, check_round_phase_log
from run_original_load import focused_actor, require_source_unchanged
from run_original_player import PlayerOracle, exact_tile, screen, sheet_frame
from run_original_result import selection_probe, selection_state
from run_original_magic_list import actor_focus_resources
from run_original_item_list import use_range_probe, measure_inventory_save
from run_original_transition import audio_toggle, collect_native_audio

CAPTURE_MAX_MS=45000
CAPTURE_INTERVAL_MS=33
JOB_PATTERN=re.compile(r"fd2 job: root=(\d+) worker_exit=(\d+) timed_out=(\d+) residual=(\d+) forced=(\d+) cleaned=(\d+) result=(\d+)")


class ActionCoverageInconclusive(AssertionError):
    """合法miss与无动作没有可见差别，不伪装为评分通过或产品错误。"""

def sha(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def fingerprint(directory):
    return {str(p.relative_to(directory)):{"sha256":sha(p),"size":p.stat().st_size,"mtime_ns":p.stat().st_mtime_ns}
        for p in sorted(Path(directory).rglob("*")) if p.is_file()}

def owned_job_required():
    if sys.platform!="win32":raise RuntimeError("B6真实采集需要Windows owned Job；未创建产物")
    kernel=ctypes.WinDLL("kernel32",use_last_error=True)
    kernel.GetCurrentProcess.restype=ctypes.c_void_p
    kernel.IsProcessInJob.argtypes=[ctypes.c_void_p,ctypes.c_void_p,ctypes.POINTER(ctypes.c_int)]
    present=ctypes.c_int()
    if not kernel.IsProcessInJob(kernel.GetCurrentProcess(),None,ctypes.byref(present)) or not present.value:
        raise RuntimeError("请用本轮driver run-job启动；未创建产物或启动窗口")

def qpc_stamp():
    if sys.platform!="win32":return {"host_monotonic":time.monotonic(),"qpc":None,"qpc_frequency":None}
    kernel=ctypes.WinDLL("kernel32",use_last_error=True)
    value,frequency=ctypes.c_longlong(),ctypes.c_longlong()
    if not kernel.QueryPerformanceCounter(ctypes.byref(value)) or not kernel.QueryPerformanceFrequency(ctypes.byref(frequency)):
        raise OSError("QueryPerformanceCounter失败")
    return {"qpc":value.value,"qpc_frequency":frequency.value,"host_monotonic":time.monotonic()}

def read_json(path):return json.loads(Path(path).read_text(encoding="utf-8"))
def wait_ready(path, process, target, timeout=10):
    deadline=time.monotonic()+timeout
    while time.monotonic()<deadline:
        if target.poll() is not None:raise RuntimeError("目标在采集ready前退出；未发送业务确认")
        try:
            if Path(path).is_file():
                ready=read_json(path)
                if ready.get("ready") is not True:raise ValueError("采集ready没有ready=true")
                return ready
        except json.JSONDecodeError:pass  # 只等尚未完成的原子ready，不吞其它合同错误。
        if process.poll() is not None:raise RuntimeError("采集helper在ready前退出；未发送业务确认")
        time.sleep(0.02)
    raise TimeoutError("采集helper ready超时；未发送业务确认")

def parse_job(text):
    found=JOB_PATTERN.findall(text)
    if len(found)!=1:raise AssertionError("采集helper没有唯一嵌套Job收据")
    keys=("root","worker_exit","timed_out","residual","forced","cleaned","result")
    row=dict(zip(keys,map(int,found[0])))
    if any(row[k] for k in ("worker_exit","timed_out","residual","forced","result")) or row["cleaned"]!=1:
        raise AssertionError("采集helper嵌套Job未正常清理："+str(row))
    return row

class OwnedHelper:
    def __init__(self,driver,arguments,work,label,record):
        self.driver,self.arguments,self.work,self.label,self.record=Path(driver),list(arguments),Path(work),label,record
        self.process=None;self.streams=[]
    def start(self):
        stdout=self.work/(self.label+".stdout.json");stderr=self.work/(self.label+".stderr.log")
        command=[str(self.driver),"run-job",str(CAPTURE_MAX_MS+15000),*map(str,self.arguments)]
        try:
            self.streams.append(stdout.open("wb"));self.streams.append(stderr.open("wb"))
            self.process=subprocess.Popen(command,stdout=self.streams[0],stderr=self.streams[1],
                creationflags=subprocess.CREATE_NO_WINDOW if sys.platform=="win32" else 0)
        except BaseException:
            for stream in self.streams:stream.close()
            raise
        self.record.update(command=command,wrapper_pid=self.process.pid,stdout=str(stdout),stderr=str(stderr),
                           forced_cleanup=False,cleaned=False)
        return self.process
    def finish(self):
        if self.process is None:return
        error=None
        try:
            self.process.wait(timeout=8)
        except subprocess.TimeoutExpired:
            self.record["forced_cleanup"]=True
            self.process.terminate() # 仅本轮Job wrapper；KillOnClose回收其helper子树。
            try:self.process.wait(timeout=3)
            except subprocess.TimeoutExpired:self.process.kill();self.process.wait(timeout=3)
            error=RuntimeError("采集helper正常stop后未退出，已回收本轮owned子树")
        finally:
            for stream in self.streams:stream.close()
            self.record.update(exit_code=self.process.poll(),cleaned=self.process.poll() is not None)
        if error:raise error
        if self.record["exit_code"]!=0:raise AssertionError("采集helper退出非0")
        self.record["job"]=parse_job(Path(self.record["stderr"]).read_text(encoding="utf-8",errors="replace"))
        self.record["summary"]=read_json(self.record["stdout"])
        return self.record["summary"]

def wav_info(path, expected_rate=44100):
    path=Path(path)
    with wave.open(str(path),"rb") as stream:
        channels,bits,rate,frames=stream.getnchannels(),stream.getsampwidth()*8,stream.getframerate(),stream.getnframes()
        if stream.getcomptype()!="NONE" or (channels,bits,rate)!=(2,16,expected_rate) or frames<=0:
            raise AssertionError(f"本轮音频不是已关闭的{expected_rate}/stereo/PCM16 WAV")
        payload=stream.readframes(frames)
    if len(payload)!=frames*4:raise AssertionError("WAV数据长度不等于已关闭header")
    samples=array.array("h",payload)
    if sys.byteorder!="little":samples.byteswap()
    nonzero=sum(sample!=0 for sample in samples);peak=max(map(abs,samples),default=0)
    if not nonzero:raise AssertionError("实际WAV全零，不能证明音效已采集")
    return {"path":str(path),"sha256":sha(path),"sample_rate":rate,"channels":channels,"bits":bits,
        "frames":frames,"data_bytes":len(payload),"nonzero_samples":nonzero,"peak":peak,
        "SFX_identity_validated":False,"A7_fidelity_validated":False}

def series_index(directory, summary, ready, pid):
    directory=Path(directory)
    if (summary.get("ok") is not True or summary.get("backend")!="WGC-HWND-session" or
        summary.get("pid")!=pid or summary.get("hwnd")!=ready.get("hwnd") or
        summary.get("ready_published") is not True or summary.get("session_started") is not True or
        summary.get("stop_reason")!="stop" or summary.get("hresult")!=0 or summary.get("color_filter") is not False):
        raise AssertionError("持续WGC未按显式stop完整结束；不能用时限/600cap截断当完整采集")
    rows=[json.loads(line) for line in (directory/"frames.jsonl").read_text(encoding="utf-8").splitlines() if line]
    if not 2<=len(rows)<=600 or len(rows)!=summary.get("frames"):raise AssertionError("持续WGC帧计数不符")
    frames=[];last=None
    for index,row in enumerate(rows):
        name=f"frame{index:05d}.png";stamp=row.get("wgc_timestamp_100ns")
        if (row.get("index")!=index or row.get("file")!=name or row.get("pid")!=pid or
            row.get("hwnd")!=ready.get("hwnd") or type(stamp) is not int or (last is not None and stamp<=last)):
            raise AssertionError("持续WGC index/文件/owner/时间戳合同错误")
        path=directory/name
        with Image.open(path) as image:
            if image.size!=(row["client_width"],row["client_height"]):raise AssertionError("持续WGC PNG/client尺寸不符")
            normalize(image) # 只验整倍320x200；不改写PNG/alpha/RGB。
        frames.append({**row,"path":str(path),"sha256":sha(path),
            "timestamp_ms":(stamp-rows[0]["wgc_timestamp_100ns"])/10000,
            "timestamp_origin":"first saved WGC frame","width":row["client_width"],"height":row["client_height"]})
        last=stamp
    if (ready.get("first_index")!=0 or ready.get("wgc_timestamp_100ns")!=rows[0]["wgc_timestamp_100ns"] or
        summary.get("first_wgc_timestamp_100ns")!=rows[0]["wgc_timestamp_100ns"] or summary.get("last_wgc_timestamp_100ns")!=last):
        raise AssertionError("持续WGC ready/summary边界不符")
    return frames


def latest_series_frame(directory,ready,pid,after=None):
    """只读同一持续WGC已发布的最新完整行/PNG，不另开截图会话。"""
    path=Path(directory)/'frames.jsonl'
    if not path.is_file():return None
    complete=[line for line in path.read_bytes().splitlines(keepends=True) if line.endswith(b'\n')]
    if not complete:return None
    previous=None;last=None
    for index,line in enumerate(complete):
        row=json.loads(line);stamp=row.get('wgc_timestamp_100ns')
        if (type(row.get('index')) is not int or row['index']!=index or row.get('file')!=f'frame{index:05d}.png' or
            row.get('pid')!=pid or row.get('hwnd')!=ready.get('hwnd') or type(stamp) is not int or
            previous is not None and stamp<=previous):raise AssertionError('活动series行的owner/index/时间戳不符')
        previous=stamp;last=row
    first=json.loads(complete[0])
    if ready.get('first_index')!=0 or first['wgc_timestamp_100ns']!=ready.get('wgc_timestamp_100ns'):
        raise AssertionError('活动series与同一ready起点不符')
    if after is not None:
        if last['index']<after['index'] or last['wgc_timestamp_100ns']<after['wgc_timestamp_100ns']:
            raise AssertionError('活动series回退，不能复用旧帧作新边沿')
        if last['index']==after['index']:return None
        if last['wgc_timestamp_100ns']<=after['wgc_timestamp_100ns']:raise AssertionError('新index没有新WGC时间戳')
    png=Path(directory)/last['file'];raw=png.read_bytes()
    with Image.open(io.BytesIO(raw)) as image:
        if image.format!='PNG' or image.size!=(last.get('client_width'),last.get('client_height')):
            raise AssertionError('活动series PNG格式/实际尺寸与行不符')
        frame=normalize(image)
    return {**last,'path':str(png),'sha256':hashlib.sha256(raw).hexdigest()},frame


class AnimationCollectors:
    def __init__(self,capture,args,record):
        self.capture,self.args,self.record=capture,args,record
        self.helpers=[];self.started=False;self.ready=None
        self.series=capture.artifacts/"animation-series"
        self.audio_ready=capture.artifacts/"audio-ready.json";self.audio_stop=capture.artifacts/"audio-stop"
        self.wav=capture.artifacts/"process-audio.wav"
        self.app_mix = (AppMixCapture(capture.artifacts/'app-mix',capture.process.pid,CAPTURE_MAX_MS)
            if record.get('kind')=='sdl' and getattr(args,'sdl_audio_source','process-loopback')=='app-mix' else None)
        if self.app_mix:
            self.audio_ready=self.app_mix.directory/'ready.json';self.audio_stop=self.app_mix.stop_path;self.wav=self.app_mix.wav
    def start(self):
        capture,record=self.capture,self.record
        if capture.process.poll() is not None:raise RuntimeError("目标已退出，未启动采集")
        slot={};record["series_helper"]=slot
        helper=OwnedHelper(self.args.driver,[self.args.driver,"capture-series",capture.process.pid,
            self.series,CAPTURE_MAX_MS,CAPTURE_INTERVAL_MS],capture.artifacts,"series-helper",slot)
        self.helpers.append(helper);process=helper.start()
        self.ready=wait_ready(self.series/"ready.json",process,capture.process)
        if self.ready.get("pid")!=capture.process.pid or not self.ready.get("hwnd"):raise AssertionError("持续WGC ready owner错误")
        record["animation_capture"]={"ready":self.ready,"capture_backend":"Windows.Graphics.Capture HWND",
            "full_frame_equivalence_validated":False,"raw_PNG_preserved":True}
        if record["kind"]=="original":
            record["native_audio"]={"enabled":True,"started":False,"stopped":False,"scope":"B6 SFX recording; source music0/sfx1, prior fade-tail silence not independently proven"}
            audio_toggle(capture,record)
        elif self.app_mix:
            record["audio_ready"]=self.app_mix.start(capture.process)
        else:
            slot={};record["audio_helper"]=slot
            helper=OwnedHelper(self.args.driver,[self.args.audio_recorder,capture.process.pid,self.wav,
                CAPTURE_MAX_MS,self.audio_ready,self.audio_stop],capture.artifacts,"audio-helper",slot)
            self.helpers.append(helper);process=helper.start()
            ready=wait_ready(self.audio_ready,process,capture.process)
            if ready.get("target_pid")!=capture.process.pid or (ready.get("sample_rate"),ready.get("channels"),ready.get("bits"))!=(44100,2,16):
                raise AssertionError("进程音频ready identity/格式错误")
            record["audio_ready"]=ready
        self.started=True
    def wait_heal_idle0(self,anchors,target_match,timeout=15):
        if type(timeout) not in (int,float) or not 0<timeout<=15:raise ValueError('heal idle ACK时限须0..15秒')
        if not self.started or self.ready is None:raise AssertionError('heal idle ACK必须在WGC/音频ready之后')
        if self.app_mix:self.app_mix.assert_running(self.capture.process)
        started=time.monotonic();deadline=started+timeout
        ack={'completed':False,'timeout_seconds':timeout,'fresh_frames':0,'unknown_frames':0,
            'source_sprite_origin':[124,94],'idle0_resource_frame':108,'checked_pixels':anchors[0]['checked_pixels'],
            'anchor_at':list(anchors[0]['at']),'new_capture_session':False,'extra_keys':[],
            'game_clock_phase_lock_or_pixel_alignment_guaranteed':False}
        self.record['heal13_idle0_ack']=ack
        baseline=latest_series_frame(self.series,self.ready,self.capture.process.pid)
        last=baseline[0] if baseline else {'index':self.ready['first_index'],'wgc_timestamp_100ns':self.ready['wgc_timestamp_100ns']}
        ack['baseline']={key:last[key] for key in ('index','wgc_timestamp_100ns')}
        while time.monotonic()<deadline:
            if self.app_mix:self.app_mix.assert_running(self.capture.process)
            if self.capture.process.poll() is not None or any(h.process is not None and h.process.poll() is not None for h in self.helpers):
                raise RuntimeError('heal idle ACK期间目标或采集helper提前退出；未确认效果')
            latest=latest_series_frame(self.series,self.ready,self.capture.process.pid,last)
            if latest is not None:
                last,frame=latest;ack['fresh_frames']+=1
                matches=[phase for phase,anchor in anchors.items() if frame_oracle._anchor_match(frame,anchor)]
                ack['last_frame']=last;ack['last_anchor_matches']=matches
                if len(matches)==1:
                    phase=matches[0]
                    if phase in (1,2):ack['nonzero']={**last,'idle_phase':phase,'checked_pixels':anchors[phase]['checked_pixels']}
                    elif phase==0 and 'nonzero' in ack:
                        target=target_match(frame)
                        if target is not None:
                            ack.update(completed=True,zero={**last,'idle_phase':0,'checked_pixels':anchors[0]['checked_pixels']},
                                target_state=target,elapsed_seconds=time.monotonic()-started)
                            return ack
                else:ack['unknown_frames']+=1
            time.sleep(.01)
        raise TimeoutError('heal idle ACK未在同一15秒内观察到新非0→0且target资源保持；未确认效果')

    def confirm(self):
        if self.app_mix:self.app_mix.assert_running(self.capture.process)
        before=qpc_stamp();self.capture.key("ENTER");after=qpc_stamp()
        receipt=self.capture.trace[-1].get("delivery")
        if not receipt or receipt.get("hwnd")!=self.ready.get("hwnd"):
            raise AssertionError("确认键与持续采集不是同一owned HWND")
        self.record["animation_action"]={"key":"ENTER","confirmations":1,"delivery":receipt,
            "key_call_qpc_bracket":{"before":before,"after":after},
            "actual_received_key_QPC_observed":False}
    def stop(self):
        errors=[]
        if self.record.get("native_audio",{}).get("started") and not self.record["native_audio"].get("stopped"):
            try:
                if self.capture.process.poll() is not None:raise RuntimeError("目标退出早于原生WAV stop")
                audio_toggle(self.capture,self.record,cleanup=True)
            except Exception as e:errors.append(str(e))
        for path in (self.series/"stop",self.audio_stop):
            try:
                if path.parent.exists() and not path.exists():path.write_text("stop\n",encoding="utf-8")
            except OSError as error:errors.append(str(error))
        for helper in self.helpers:
            try:helper.finish()
            except Exception as e:errors.append(str(e))
        if not errors:
            try:
                summary=self.record["series_helper"]["summary"]
                if read_json(self.series/"summary.json")!=summary:raise AssertionError("WGC文件/stdout总结不一致")
                self.record["animation_capture"].update(summary=summary,
                    summary_path=str(self.series/"summary.json"),summary_sha256=sha(self.series/"summary.json"),
                    frames=series_index(self.series,summary,self.ready,self.capture.process.pid))
                ack=self.record.get('heal13_idle0_ack',{})
                if ack.get('completed'):
                    frames={row['index']:row for row in self.record['animation_capture']['frames']}
                    for name in ('nonzero','zero'):
                        witness=ack[name];actual=frames.get(witness['index'],{})
                        if any(actual.get(key)!=witness.get(key) for key in ('sha256','wgc_timestamp_100ns','pid','hwnd')):
                            raise AssertionError('heal idle ACK帧与最终同一series来源不符')
                if self.record["kind"]=="original":
                    deadline=time.monotonic()+5
                    while True:
                        try:
                            collect_native_audio(self.record,self.capture.artifacts)
                            files=self.record["native_audio"].get("files",[])
                            if len(files)!=1:raise AssertionError("原生WAV不是本轮唯一一份")
                            info=wav_info(files[0]["path"]);break
                        except (OSError,EOFError,wave.Error,AssertionError):
                            if time.monotonic()>=deadline:raise
                            time.sleep(.05)
                    self.record["audio_capture"]={"role":"native DOSBox HOST_W","wav":info,"only_collection":True}
                elif self.app_mix:
                    receipt=self.app_mix.finish(self.capture.process)
                    info=wav_info(self.wav,expected_rate=48000)
                    summary=receipt['summary']
                    if info['frames']!=summary['frames'] or info['data_bytes']!=summary['data_bytes']:
                        raise AssertionError('app-mix封包/WAV计数不符')
                    self.record['app_mix_capture']=receipt
                    self.record['audio_capture']={'role':'application final mix before test suppression and OS session',
                        'source_scope':'app_mix','wav':info,'app_mix_summary':summary,'only_collection':True,
                        'suppress_device_output':True,'device_output_validated':False,'A8_validated':False}
                else:
                    audio=self.record["audio_helper"]["summary"]
                    if (audio.get("ok") is not True or audio.get("target_pid")!=self.capture.process.pid or
                        audio.get("stop_reason")!="stop_file" or audio.get("stream_started") is not True or
                        any(int(str(audio.get(k,"-1")),0) for k in ("hresult","stop_hresult","finalize_hresult"))):
                        raise AssertionError("进程音频未按显式stop完整关闭WAV")
                    info=wav_info(self.wav)
                    if audio.get("frames")!=info["frames"] or audio.get("data_bytes")!=info["data_bytes"]:
                        raise AssertionError("音频helper/WAV完整数据计数不一致")
                    self.record["audio_capture"]={"role":"Windows target-process tree","wav":info,"recorder_summary":audio,"only_collection":True}
                self.record["collection_completed"]=True
            except Exception as e:errors.append(str(e))
        if errors:
            self.record.setdefault("collection_errors",[]).extend(errors)
            raise AssertionError("采集未完整完成："+"; ".join(errors))
    @contextmanager
    def active(self):
        body_error=None
        try:
            self.start();yield self
        except BaseException as error:
            body_error=error;raise
        finally:
            try:self.stop()
            except Exception as error:
                self.record.setdefault("collector_cleanup_error",str(error))
                if body_error is None:raise

def prepare_runtime(original, scenario):
    prepared=oracle.prepare(original,scenario)
    assets=round_assets(original)
    prepared.update(assets=assets,titles=title_templates(original/'FDOTHER.DAT'),
        initial_focus=tuple(prepared['source'][META+5:META+7]),end_world=(6,14),
        banner=round_two_template(original/'FDOTHER.DAT',assets['palette']))
    if prepared['b4']:
        for key in ('source_actors','movement','target','target_probe','magic_lists','list_upper','pages'):
            if key in prepared['b4']:prepared[key]=prepared['b4'][key]
    elif scenario=='player-magic0':
        source=prepared['source'];caster=oracle.actor(source,1)
        prepared['source_actors']=[actor_focus_resources(original,source,assets,i,assets['cursor']) for i in (0,1)]
        prepared['movement']=selection_probe(original,source,1,assets)
        prepared['magic_lists']=[magic_page(original,caster,assets,0)[0]]
        _,detail,_=inventory_templates(original,caster,assets)
        prepared['list_upper']={key:detail[key] for key in ('actor_name','current_max_mp')}
        cursor=sheet_frame(archive_entry((original/'FDOTHER.DAT').read_bytes(),1),1)
        cursor.putpalette(assets['palette'])
        prepared['target']=actor_focus_resources(original,source,assets,11,cursor)
        range_source=bytearray(source)
        range_source[UNITS:UNITS+STRIDE]=caster
        range_source[UNITS+STRIDE:UNITS+2*STRIDE]=source[UNITS:UNITS+STRIDE]
        prepared['target_probe']=use_range_probe(original,range_source,assets,prepared['magic'][3],0,(8,14))
    # 固定导航与目标必须在进程创建前完成资源构造。
    for start in (prepared['initial_focus'],(7,14),(8,14)):
        reward.focus_steps(prepared['source'],start,(6,14))
    if scenario not in oracle.BUSINESS_SCENARIOS:
        prepared['frame_resources']=frame_oracle.prepare_resources(original,prepared['source'],
            prepared['manifest']['caster_raw'],prepared['manifest']['targets'],
            kind='physical' if scenario=='physical-full-hit' else 'magic',magic_id=prepared['manifest']['magic_id'])
    if scenario=='ai-behavior5-chest':prepared['ai5_opened_cell']=ai5_opened_cell_resources(original,prepared)
    if scenario=='player-heal13':
        anchors={}
        for rule in prepared['frame_resources']['rules']['map-heal']:
            item=rule['anchors'][1];identity=item['identity'];phase=identity.get('frame')
            if identity.get('resource')=='FDICON identity' and identity.get('raw')==1 and identity.get('unit_id')==9 and phase in (0,1,2):
                anchors.setdefault(phase,item)
        caster=oracle.actor(prepared['source'],1);origin=screen(prepared['source'],tuple(caster[:2]))
        if set(anchors)!={0,1,2} or (origin[0],origin[1]-6)!=(124,94) or anchors[0]['checked_pixels']!=403:
            raise ValueError('heal idle ACK不满足实际unit9/frame108资源前置')
        prepared['heal_idle_anchors']=anchors
    return prepared


def ai5_opened_cell_resources(original, prepared):
    """原248→249，完整格叠全部raw actor（含下格Hero头）；无动态FG的限定资源。"""
    from run_original_player import cursor_assets
    original=Path(original);source=prepared['base_final'];assets=prepared['assets']
    tiles=archive_entry((original/'FDSHAP.DAT').read_bytes(),0)
    attrs=archive_entry((original/'FDSHAP.DAT').read_bytes(),1)
    at=screen(source,(7,13));focus=screen(source,tuple(source[META+5:META+7]))
    if not (4<=at[0]<=292 and 4<=at[1]<=172) or focus!=(at[0],at[1]+24):
        raise ValueError('AI5完整箱格与Hero光标须为已预检相邻内区，不能裁viewport')
    for tile in (248,249):
        begin=struct.unpack_from('<I',tiles,6+tile*4)[0];end=struct.unpack_from('<I',tiles,6+(tile+1)*4)[0]
        _,mask=frame_oracle.rle_material(struct.pack('<HH',24,24)+tiles[begin:end])
        if sum(value!=0 for value in mask.getdata())!=576:raise ValueError('AI5两地形帧须完整576原RLE写像素')
    frames=[];raws=[]
    for raw in range(source[META+1]):
        actor=oracle.actor(source,raw)
        if actor[5]&1:continue
        x,y=screen(source,tuple(actor[:2]));offset=(x-at[0],y-6-at[1])
        if not (-24<offset[0]<24 and -24<offset[1]<24):continue
        if actor[4] or actor[3]!=0:raise ValueError('AI5观测只接受原common-tail清方向后的静止actor')
        isolated=bytearray(source);isolated[META+1]=1;isolated[UNITS:UNITS+STRIDE]=actor
        sprites,_=cursor_assets(original/'FDICON.B24',isolated,assets)
        frames.append((raw,offset,sprites));raws.append(raw)
    if raws!=[0,11] or attrs[248*4:249*4]!=bytes((32,0,4,0)) or attrs[249*4:250*4]!=bytes((0,0,4,0)):
        raise ValueError('AI5当前完整格须raw0下格头→raw11本格原顺序，且箱248/249没有额外FG')
    images={};rgb={}
    for tile in (248,249):
        images[tile]=[];rgb[tile]=[]
        for phase in range(3):
            image=sheet_frame(tiles,tile);image.putpalette(assets['palette'])
            for raw,offset,sprites in frames:
                sprite=sprites[phase];mask=sprite.point(lambda value:255 if value else 0,mode='L')
                image.paste(sprite,offset,mask)
            images[tile].append(image);rgb[tile].append(image.convert('RGB'))
    differences=[]
    for opened in rgb[249]:
        for closed in rgb[248]:
            differences.append(sum(any(abs(a-b)>3 for a,b in zip(x,y)) for x,y in zip(opened.getdata(),closed.getdata())))
    phases=[phase for phase in range(3) if min(differences[phase*3:phase*3+3])>=24]
    if not phases:raise ValueError('AI5完整格没有与所有closed相位至少24正差的可观测idle')
    return {'position':at,'templates':[images[249][phase] for phase in phases],'shared_idle_phases':phases,
        'closed_templates':images[248],'raw_actor_order':raws,'world':[7,13],'closed_tile':248,'opened_tile':249,
        'checked_pixels':576,'opened_closed_positive_matrix':differences,
        'minimum_opened_closed_positive_pixels':min(differences[phase*3+i] for phase in phases for i in range(3)),
        'lower_Hero_head_included':True,'region_cropped_or_masked':False}


def ai5_opened_cell_state(frame, prepared):
    resources=prepared['ai5_opened_cell'];at=resources['position']
    if any(exact_tile(frame,image,at) for image in resources['closed_templates']):return None
    for phase,image in zip(resources['shared_idle_phases'],resources['templates']):
        hit=exact_tile(frame,image,at)
        if hit:return {key:value for key,value in resources.items() if key not in ('templates','closed_templates')}|dict(hit,ok=True,idle_phase=phase)
    return None


def target_state(frame, prepared):
    found=focused_actor(frame,prepared['assets'],prepared['target'])
    if not found:return None
    probe=prepared['target_probe']
    for phase,image in probe['ranges']:
        match=exact_tile(frame,image,probe['position'])
        if match:return {'kind':'b6-target','actor':found,'range':match,'range_phase':phase,
            'cursor_frame':0 if prepared['scenario']=='physical-full-hit' else 1}
    return None


def wait_phase(capture, prepared, checkpoint, record):
    if record['kind']=='original':
        from run_original_player import match_indexed
        def banner(frame):
            found=match_indexed(frame,prepared['banner'],(120,84))
            return dict(found,kind='native-round002') if found['ok'] else None
        _,record['native_round_banner']=wait_state(capture,'b6-native-round002',banner,[],timeout=120)
    else:
        from field_play_input_test import wait_log
        text=wait_log(capture.process,capture.artifacts/'game.log','field phase: turn=2 phase=2 ',timeout=120)
        check_round_phase_log(text)
        record['sdl_phase_log']={'ordered_single_end':True,'banner_image_compared':False}
    checkpoint('b6-turn2-ready',reward.quiet_browse(prepared,prepared['base_final'],(7,14),record,'b6-turn2-ready'),timeout=120)


def open_end(capture, prepared, checkpoint):
    view=RoundOracle(prepared['assets'],screen(prepared['source'],(6,14)))
    capture.key('ENTER');checkpoint('b6-end-parent',lambda frame:view.menu(frame,0))
    capture.key('DOWN');checkpoint('b6-end-selected',lambda frame:view.menu(frame,3))
    capture.key('ENTER');checkpoint('b6-end-first-complete-buttons',lambda frame:view.buttons(frame,0))


def measure(capture, prepared, save, guard, record, checkpoint):
    if prepared['scenario']=='ai-behavior5-chest' and record.get('opened_cell_live_visual',{}).get('ok') is not True:
        raise AssertionError('AI5必须在Save前实见完整opened249格；不能只靠Load重建')
    final=prepared['base_final'];finite={}
    def candidates():
        expected,detail=oracle.project_final(prepared,decode_save(save))
        finite.update(detail)
        yield expected
    plan={'steps':[('LEFT',screen(final,(6,14)))],'expected_sources':candidates()}
    measure_inventory_save(capture,prepared['assets'],plan,save,*guard,record,checkpoint,
        prefix='b6-final',measurement_key='b6_final_save')
    record['b6_final_save'].pop('finite_native_outcome_indices',None)
    record['b6_final_save'].update(finite)
    guard[:]=[save.read_bytes(),save.stat().st_mtime_ns]
    record['business_sequence_completed']=True
    if prepared['scenario']=='ai-behavior11-recover':
        record['ai11_action_business_evidence']=oracle.ai11_action_coverage(prepared,decode_save(save),plan['steps'])
    if prepared['scenario']=='ai-behavior5-chest':
        record['ai5_action_business_evidence']=oracle.ai5_action_coverage(prepared,decode_save(save),plan['steps'])
        record['ai5_action_business_evidence']['live_opened_cell_checked_before_Save']=True
    if prepared['scenario']=='ai-item-score20':
        record['ai_item_action_positive_evidence']=oracle.item_action_coverage(prepared,decode_save(save),plan['steps'])
        if not record['ai_item_action_positive_evidence']['passed']:
            raise ActionCoverageInconclusive('AI item合法miss或无动作：完整SAV合法但缺少HP下降正证；保留本轮、不重抽')


def exercise(capture, prepared, save, guard, record, args):
    source,assets,scenario=prepared['source'],prepared['assets'],prepared['scenario']
    def checkpoint(name,predicate,*,source_guard=True,**options):
        result=wait_state(capture,name,predicate,record['checkpoints'],**options)
        if source_guard:require_source_unchanged(save,*guard)
        return result
    wait_title(capture,prepared['titles'][3][0],timeout=90)
    capture.key('UP');capture.wait(prepared['titles'][3][2]);capture.key('ENTER')
    checkpoint('b6-continued',RoundOracle(assets,screen(source,prepared['initial_focus'])).initial_browse,timeout=45)
    if scenario.startswith('ai-'):
        reward.navigate(capture,prepared,source,prepared['initial_focus'],(6,14),checkpoint,'b6-to-end')
        open_end(capture,prepared,checkpoint)
        if scenario in oracle.BUSINESS_SCENARIOS:
            record['collection_scope']='real WGC business checkpoints and full SAV; no continuous FULL/WAV collectors requested'
            capture.key('ENTER')
            wait_phase(capture,prepared,checkpoint,record)
            if scenario=='ai-behavior5-chest':
                _,record['opened_cell_live_visual']=checkpoint('major-ai5-live-opened-cell',
                    lambda frame:ai5_opened_cell_state(frame,prepared))
            record['collection_completed']=True
        else:
            with AnimationCollectors(capture,args,record).active() as collectors:
                collectors.confirm()
                wait_phase(capture,prepared,checkpoint,record)
    else:
        raw=prepared['manifest']['caster_raw']
        for actor in prepared['source_actors'][:raw+1]:
            capture.key('Z');checkpoint('b6-z-'+str(actor['raw_actor']),lambda frame,a=actor:focused_actor(frame,assets,a))
        capture.key('ENTER')
        checkpoint('b6-move-selection',lambda frame:selection_state(frame,assets,prepared['source_actors'][raw],prepared['movement']))
        capture.key('ENTER');view=reward.actor_view(prepared['original'],source,assets,raw)
        selected,frames=(0,[1,5,6,9]) if raw==0 else (1,[2,4,6,9])
        checkpoint('b6-zero-move-command',lambda frame:reward.command(frame,source,view,raw,selected,frames))
        capture.key('ENTER')
        if raw==1:
            checkpoint('b6-magic-list0',lambda frame:reward.list_state(frame,prepared,0))
            if scenario=='player-heal13':
                capture.key('UP');checkpoint('b6-magic13-selected',lambda frame:reward.list_state(frame,prepared,1))
            capture.key('ENTER')
        checkpoint('b6-target',lambda frame:target_state(frame,prepared))
        with AnimationCollectors(capture,args,record).active() as collectors:
            if scenario=='player-heal13':
                collectors.wait_heal_idle0(prepared['heal_idle_anchors'],lambda frame:target_state(frame,prepared))
                require_source_unchanged(save,*guard)
            collectors.confirm()
            if prepared['b4']:
                reward.observe_pages(capture,prepared,checkpoint,record)
                immediate=prepared['b4']['model']['immediate']
            else:
                immediate=bytearray(source);at=UNITS+raw*STRIDE
                immediate[at+5]|=0x80
                oracle._store(immediate,at+0x44,oracle._word(immediate,at+0x44)-prepared['magic'][5])
            world=(7,14) if scenario=='player-heal13' else (8,14)
            checkpoint('b6-action-ready',reward.quiet_browse(prepared,immediate,world,record,'b6-action-ready'),timeout=120)
        reward.navigate(capture,prepared,immediate,world,(6,14),checkpoint,'b6-after-action-to-end')
        open_end(capture,prepared,checkpoint);capture.key('ENTER')
        wait_phase(capture,prepared,checkpoint,record)
    measure(capture,prepared,save,guard,record,checkpoint)


def dependencies():
    rows={}
    for module in list(sys.modules.values()):
        name=getattr(module,'__file__',None)
        if not name:continue
        path=Path(name).resolve()
        if path.suffix=='.py' and path.is_file() and any(folder.resolve() in path.parents for folder in (ROOT/'tools',ROOT/'src/tests')):
            rows[str(path)]={'sha256':sha(path),'size':path.stat().st_size}
    return rows


def dependency_guard(rows):
    return all(Path(path).is_file() and sha(path)==row['sha256'] and Path(path).stat().st_size==row['size'] for path,row in rows.items())


def endpoint_guard(record,original,before,fixture,fixture_data,fixture_stamp,save,guard,tools,binaries):
    try:
        checks={'original_bytes_size_mtime_unchanged':fingerprint(original)==before,
            'fixture_bytes_mtime_unchanged':fixture.read_bytes()==fixture_data and fixture.stat().st_mtime_ns==fixture_stamp,
            'tool_sources_unchanged':dependency_guard(tools),'binaries_unchanged':dependency_guard(binaries),
            'copy_save_bytes_mtime_expected':save.is_file() and save.read_bytes()==guard[0] and save.stat().st_mtime_ns==guard[1],
            'business_and_collectors_completed':record.get('business_sequence_completed') is True and record.get('collection_completed') is True,
            'owned_game_cleanup':len(record['processes'])==1 and all(p['cleaned'] and not p['forced_cleanup'] and p['exit_code']==0 for p in record['processes'])}
        if record.get('isolated_copy_initial') is not None:
            now=fingerprint(save.parent);initial=record['isolated_copy_initial'];mutable={'FD2.SAV','FD2.TMP'}
            checks['isolated_static_resources_unchanged']=all(now.get(name)==row for name,row in initial.items() if name not in mutable) and not (set(now)-set(initial)-mutable)
            record['isolated_working_cache']={'FD2.TMP_before':initial.get('FD2.TMP'),'FD2.TMP_after':now.get('FD2.TMP'),'excluded_only_work_copy_cache':True}
        app_mix=record.get('app_mix_capture')
        if app_mix:
            checks['app_mix_artifacts_unchanged']=all(
                Path(app_mix[key]['path']).is_file() and sha(app_mix[key]['path'])==app_mix[key]['sha256'] and
                Path(app_mix[key]['path']).stat().st_size==app_mix[key]['size'] and
                Path(app_mix[key]['path']).stat().st_mtime_ns==app_mix[key]['mtime_ns']
                for key in ('ready_fact','summary_fact'))
        completed=record.get('completed_save')
        if completed:checks['completed_snapshot_unchanged']=Path(completed['snapshot']).is_file() and sha(completed['snapshot'])==completed['sha256']
    except (OSError,ValueError) as error:checks={'guard_read_error':False};record.setdefault('error',str(error))
    old=record.setdefault('guards',{})
    for key,value in checks.items():old[key]=old.get(key,True) and value
    record['collection_passed']=bool(record.get('collection_passed')) and all(old.values())
    if not record['collection_passed']:
        record['status']='inconclusive' if record.get('failure_category')=='inconclusive_action_coverage' and all(old.values()) else 'failed'
        record.setdefault('error','B6业务/采集/晚守卫/owned清理未完成')



def verify_core(report, prepared, output):
    """所有进程已正常退出后才读取实际封包WAV；最低门不提升为整段音画保真。"""
    cases=report.get('cases',[])
    endpoints_ready=([case.get('kind') for case in cases]==['original','sdl'] and all(
        case.get('collection_passed') is True and case.get('business_sequence_completed') is True and
        case.get('collection_completed') is True and case.get('guards') and
        all(value is True for value in case['guards'].values()) and len(case.get('processes',[]))==1 and
        all(row.get('cleaned') is True and row.get('forced_cleanup') is False and row.get('exit_code')==0 for row in case['processes'])
        for case in cases))
    ready=(report.get('collection_passed') is True and endpoints_ready and bool(report.get('final_guards')) and
        all(value is True for value in report['final_guards'].values()))
    result={'ok':False,'collection_guards_cleanup_passed':bool(ready),
        'frame_passed':report.get('independent_frame_pairing',{}).get('ok') is True,
        'sfx_passed':{'original':False,'sdl':False},
        'scope':'minimum core full64000 resource-phase pairs plus independent VOICE identity/recording order',
        'whole_animation_equivalence':False,'full_action_audio_validated':False,
        'seven_FX0_instances_validated':False,'exact_timing_validated':False,'A7_fidelity_validated':False}
    if not ready:
        result['error']='业务/来源守卫或两端正常清理未完成；没有执行声音验收'
        return result
    report['core_sfx_observations']={};seen=set();checked=[]
    for case in cases:
        kind=case['kind']
        try:
            audio=case.get('audio_capture',{}).get('wav',{})
            path=Path(audio['path']).resolve(strict=True)
            path.relative_to((Path(output)/kind).resolve(strict=True))
            if path in seen or sha(path)!=audio.get('sha256'):
                raise AssertionError('实际WAV重复使用或已离开本端封包SHA')
            seen.add(path);checked.append((kind,path,audio['sha256']))
            observed=core_sfx.analyze_core_sfx(prepared['original']/'FDOTHER.DAT',prepared['core_sfx_declaration'],path)
            if observed.get('wav',{}).get('sha256')!=audio['sha256']:
                raise AssertionError('声音oracle读取的完整WAV与采集收据不符')
            report['core_sfx_observations'][kind]=observed
            result['sfx_passed'][kind]=observed.get('minimum_core_sfx_supported') is True
            target=Path(output)/kind/'core-sfx.json'
            target.write_text(json.dumps(observed,ensure_ascii=False,indent=2),encoding='utf-8')
        except Exception as error:
            result.setdefault('errors',{})[kind]=type(error).__name__+': '+str(error)
    result['wav_sources_unchanged']=len(checked)==2
    for kind,path,expected_sha in checked:
        try:
            if sha(path)!=expected_sha:raise AssertionError('实际WAV在声音比较期间变化')
        except (OSError,AssertionError) as error:
            result['wav_sources_unchanged']=False;result['sfx_passed'][kind]=False
            result.setdefault('errors',{}).setdefault(kind,str(error))
    result['ok']=result['frame_passed'] and result['wav_sources_unchanged'] and all(result['sfx_passed'].values())
    if not result['ok']:result.setdefault('error','最低完整帧或两端有限VOICE身份/顺序门未通过')
    return result


def run(args, *, _final_report=None):
    root,output=args.root.resolve(),args.artifacts.resolve();original=(root/'original_game').resolve()
    if output==original or original in output.parents:raise ValueError('产物不得位于原版只读目录或其解析后的子目录')
    if args.scenario not in oracle.SCENARIOS:raise ValueError('未知或原版不支持的B6场景')
    verify_requested=bool(getattr(args,'verify_core',False))
    if verify_requested and (args.sdl is None or args.scenario not in sfx_declaration.CORE_SCENARIOS):
        raise ValueError('--verify-core仅支持有SDL的核心三场；未创建产物或启动窗口')
    audio_source=getattr(args,'sdl_audio_source','process-loopback')
    if audio_source not in ('process-loopback','app-mix'):raise ValueError('未知SDL声音来源')
    requested_audio_source=audio_source
    if args.sdl is None or args.scenario in oracle.BUSINESS_SCENARIOS:
        audio_source='not-requested'
    binaries={}
    for name in ('driver','dosbox','sdl','audio_recorder'):
        path=getattr(args,name,None)
        if path is None:
            if name in ('driver','dosbox') or name=='audio_recorder' and audio_source=='process-loopback' and args.sdl is not None and args.scenario not in oracle.BUSINESS_SCENARIOS:raise ValueError('缺少实际采集程序：'+name)
            continue
        path=path.resolve()
        if not path.is_file():raise ValueError('实际采集程序不存在：'+name)
        binaries[str(path)]={'sha256':sha(path),'size':path.stat().st_size}
    owned_job_required()
    before=fingerprint(original);prepared=prepare_runtime(original,args.scenario)
    if verify_requested:
        prepared['core_sfx_declaration']=sfx_declaration.declare(original,args.scenario)
        prepared['core_sfx_plan']=core_sfx.prepare_core_sfx(original/'FDOTHER.DAT',prepared['core_sfx_declaration'])
    tools=dependencies()
    if fingerprint(original)!=before:raise AssertionError('纯前置期间原版来源变化；未启动窗口')
    output.mkdir(parents=True,exist_ok=False)
    fixture=output/'controlled.SAV';fixture.write_bytes(prepared['encoded']);fixture_stamp=fixture.stat().st_mtime_ns
    if decode_save(fixture)!=prepared['source']:raise AssertionError('夹具完整codec/checksum往返失败')
    (output/'fixture-manifest.json').write_text(json.dumps(prepared['manifest'],ensure_ascii=False,indent=2),encoding='utf-8')
    report={'ok':False,'collection_passed':False,'collected_only':not verify_requested,'status':'failed',
        'verify_core_requested':verify_requested,
        'paired_collected':False,'probe_only':args.sdl is None,'scenario':args.scenario,
        'scope':'strict native finite complete SAV + real input + transient raw WGC frames and closed WAV collection',
        'sdl_audio_source':audio_source,'requested_sdl_audio_source':requested_audio_source,
        'application_mix_is_not_device_output':audio_source=='app-mix',
        'pending':['independent complete animation frame equivalence','SFX identity/onset','A7 fidelity'],
        'shared_rng_aligned':False,'contract':prepared['contract'],'caster_metadata':prepared['caster_metadata'],
        'fixture_manifest':prepared['manifest'],'tool_sources':tools,'binaries':binaries,'original_before':before,'cases':[]}
    if args.scenario in oracle.BUSINESS_SCENARIOS:
        report.update(business_regression_passed=False,
            scope='paired AI business regression via real END/WGC checkpoints/full SAV; not FX/SFX verification',
            requested_collection='business checkpoints only; continuous WGC/WAV not requested')
    if verify_requested:report['core_sfx_predeclared_plan']=prepared['core_sfx_plan']
    if _final_report is not None:_final_report['value']=report
    try:
        for kind in (('original',) if args.sdl is None else ('original','sdl')):
            work=output/kind;work.mkdir();env=os.environ.copy()
            for key in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED',
                'FD2SDL_APP_PCM_CAPTURE_DIR','FD2SDL_APP_PCM_CAPTURE_MAX_MS','FD2SDL_APP_PCM_CAPTURE_QUIET',
                'FD2SDL_TEST_AUDIO_QUIET'):env.pop(key,None)
            if kind=='original':
                game=work/'game';shutil.copytree(original,game);save=game/'FD2.SAV';(work/'captures').mkdir()
                config=work/'dosbox.conf'
                config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
                    '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n'
                    f'captures={(work/"captures").as_posix()}\nshow recorded filename=false\n'
                    '[render]\naspect=false\nscaler=normal2x\n[mixer]\nrate=44100\n'
                    f'[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "{game}"\nc:\nFD2.EXE\nexit\n',encoding='utf-8')
                command,cwd=[str(args.dosbox.resolve()),'-conf',str(config),'-noconsole'],work
            else:
                save=work/'FD2.SAV';env['FD2SDL_SAVE_PATH']=str(save);command,cwd=[str(args.sdl.resolve())],root
                if audio_source=='app-mix' and args.scenario not in oracle.BUSINESS_SCENARIOS:
                    app_mix_dir=work/'app-mix';app_mix_dir.mkdir()
                    env.update(FD2SDL_APP_PCM_CAPTURE_DIR=str(app_mix_dir),
                        FD2SDL_APP_PCM_CAPTURE_MAX_MS=str(CAPTURE_MAX_MS),FD2SDL_APP_PCM_CAPTURE_QUIET='1')
                elif audio_source=='process-loopback' and args.scenario not in oracle.BUSINESS_SCENARIOS:
                    # 显式旧采集后端保留实际设备提交；系统mixer仍由用户控制。
                    env['FD2SDL_TEST_AUDIO_QUIET']='0'
            save.write_bytes(prepared['encoded']);guard=[prepared['encoded'],save.stat().st_mtime_ns]
            record={'kind':kind,'status':'failed','ok':False,'collected_only':True,'collection_passed':False,
                'checkpoints':[],'inputs':[],'processes':[],'initial_save_mtime_ns':guard[1]}
            if kind=='original':record['isolated_copy_initial']=fingerprint(game)
            report['cases'].append(record)
            try:
                with running(command,cwd,env,work/'game.log',args.driver.resolve(),record['processes']) as process:
                    capture=Capture(args.driver.resolve(),process,work,record['inputs'])
                    exercise(capture,prepared,save,guard,record,args)
                    require_source_unchanged(save,*guard)
                record.update(collection_passed=True,status='collected')
            except Exception as error:
                record.setdefault('error',str(error))
                if isinstance(error,ActionCoverageInconclusive):record['failure_category']='inconclusive_action_coverage'
            finally:
                endpoint_guard(record,original,before,fixture,prepared['encoded'],fixture_stamp,save,guard,tools,binaries)
                record['capture_hashes']={str(p.relative_to(work)):sha(p) for p in work.rglob('*') if p.is_file() and p.suffix.lower() in ('.png','.bmp','.sav','.wav')}
                (work/'comparison.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
            if not record['collection_passed']:
                if record.get('status')=='inconclusive':raise ActionCoverageInconclusive(record['error'])
                raise AssertionError(record['error'])
        if args.sdl is not None and args.scenario in oracle.BUSINESS_SCENARIOS:
            report['independent_frame_pairing']={'ok':False,'scope':'AI business checkpoints and full SAV regression only',
                'pending':['independent map FX/SFX pairing'],'whole_animation_equivalence':False}
        elif args.sdl is not None and args.scenario=='ai-item-score20':
            report['independent_frame_pairing']={'ok':False,'scope':'AI ITEM地图路径，不按FULL casting识别器冒称配对',
                'pending':['independent item map FX/SFX pairing'],'whole_animation_equivalence':False}
        elif args.sdl is not None:
            phases=['physical-attack','physical-impact'] if args.scenario=='physical-full-hit' else (['map-heal'] if args.scenario=='player-heal13' else ['casting'])
            report['independent_frame_pairing']=frame_oracle.compare_sequences(
                report['cases'][0]['animation_capture']['frames'],report['cases'][1]['animation_capture']['frames'],
                prepared['frame_resources'],required_phases=phases,minimum_pairs=1 if args.scenario=='physical-full-hit' else 2)
        report.update(collection_passed=True,status='collected',paired_collected=args.sdl is not None)
    except Exception as error:
        report.setdefault('error',str(error))
        if isinstance(error,ActionCoverageInconclusive):report['failure_category']='inconclusive_action_coverage'
    finally:
        for record in report['cases']:
            work=output/record['kind'];save=work/'game'/'FD2.SAV' if record['kind']=='original' else work/'FD2.SAV'
            completed=record.get('completed_save')
            data=prepared['encoded']
            if completed:
                try:data=Path(completed['snapshot']).read_bytes()
                except OSError as error:record.setdefault('error',str(error));record['collection_passed']=False
            stamp=completed['mtime_ns'] if completed else record['initial_save_mtime_ns']
            endpoint_guard(record,original,before,fixture,prepared['encoded'],fixture_stamp,save,[data,stamp],tools,binaries)
            (work/'comparison.json').write_text(json.dumps(record,ensure_ascii=False,indent=2),encoding='utf-8')
        report['final_guards']={'original_unchanged':fingerprint(original)==before,'tools_unchanged':dependency_guard(tools),
            'binaries_unchanged':dependency_guard(binaries),'fixture_unchanged':fixture.read_bytes()==prepared['encoded'] and fixture.stat().st_mtime_ns==fixture_stamp}
        report['collection_passed']=report['collection_passed'] and all(report['final_guards'].values()) and all(c['collection_passed'] for c in report['cases'])
        report['paired_collected']=bool(report['collection_passed'] and args.sdl is not None and len(report['cases'])==2)
        if not report['collection_passed']:
            report['status']='inconclusive' if report.get('failure_category')=='inconclusive_action_coverage' and all(report['final_guards'].values()) and all(all(c['guards'].values()) for c in report['cases']) else 'failed'
            report.setdefault('error','最终来源或清理守卫失败')
        if args.scenario in oracle.BUSINESS_SCENARIOS:
            evidence_key='ai11_action_business_evidence' if args.scenario=='ai-behavior11-recover' else 'ai5_action_business_evidence'
            report['business_regression_passed']=bool(report['paired_collected'] and report['collection_passed'] and
                all(c.get(evidence_key,{}).get('passed') is True and
                    (args.scenario!='ai-behavior5-chest' or c.get('opened_cell_live_visual',{}).get('ok') is True and
                     c.get(evidence_key,{}).get('live_opened_cell_checked_before_Save') is True) for c in report['cases']))
        if verify_requested:
            report['core_verification']=verify_core(report,prepared,output)
            report['ok']=report['core_verification']['ok']
            report['status']='passed' if report['ok'] else 'failed'
            report['pending']=['whole animation/every FX frame','complete audio/repeated cue counts','exact timing and A7 fidelity']
            if not report['ok']:report.setdefault('error',report['core_verification'].get('error','B6最低门未通过'))
        if _final_report is None:
            (output/'comparison.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('root','driver','dosbox','artifacts'):parser.add_argument('--'+name,type=Path,required=True)
    parser.add_argument('--sdl',type=Path);parser.add_argument('--audio-recorder',type=Path)
    parser.add_argument('--sdl-audio-source',choices=('process-loopback','app-mix'),default='app-mix',
        help='默认app-mix安静采应用混音，不代表设备输出；显式process-loopback保留设备提交')
    parser.add_argument('--scenario',choices=oracle.SCENARIOS,required=True)
    parser.add_argument('--verify-core',action='store_true',help='核心三场双端在清理后强制最低完整帧及有限SFX门')
    args=parser.parse_args();display={};report=None;error=None;exit_code=0;final_report={}
    # 只改本次新建的输出；OFF/未知与既有artifact失败不覆盖历史报告。
    output_existed=args.artifacts.exists();run_started=False
    try:
        with windows_display_requirement(display):
            run_started=True;report=run(args,_final_report=final_report)
    except BaseException as caught:
        error=caught
        exit_code=(130 if isinstance(caught,KeyboardInterrupt) else
                   caught.code if isinstance(caught,SystemExit) and isinstance(caught.code,int) and caught.code else 1)
    comparison=args.artifacts/'comparison.json'
    owns_output=run_started and not output_existed and args.artifacts.is_dir()
    if report is None:report=final_report.get('value')
    if report is None and owns_output and comparison.is_file():
        try:report=read_json(comparison)
        except (OSError,ValueError) as caught:display['report_read_error']={'type':type(caught).__name__,'message':str(caught)}
    if report is None:
        report={'status':'failed','collection_passed':False,'ok':False,'collected_only':not args.verify_core}
    if error is not None:
        report.update(status='failed',ok=False,collection_passed=False,paired_collected=False,
                      business_regression_passed=False)
        report.setdefault('error',str(error));report['entry_error']={'type':type(error).__name__,'message':str(error)}
    report['display_requirement']=display
    if owns_output:
        try:
            pending=comparison.with_name('comparison-display-pending.json')
            pending.write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
            pending.replace(comparison)
        except (OSError,ValueError) as caught:
            report.update(status='failed',ok=False,collection_passed=False,business_regression_passed=False)
            display['report_write_error']={'type':type(caught).__name__,'message':str(caught)};exit_code=1
    print(json.dumps({'status':report['status'],'collection_passed':report['collection_passed'],'ok':report['ok'],
        'collected_only':report['collected_only'],'display_requirement':display},ensure_ascii=False))
    passed=(report.get('business_regression_passed',False) if args.scenario in oracle.BUSINESS_SCENARIOS else
            report['ok'] if args.verify_core else report['collection_passed'])
    return exit_code or (0 if passed and report['status']!='failed' else 1)

if __name__=='__main__':raise SystemExit(main())
