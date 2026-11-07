"""A7有限音频回归：沿原16f55/1728c/19df7/25977资源与提交边界。
复用现有后台输入、WGC和音频生命周期；不恢复历史候选或声明音色/设备保真。
ordered WGC沿已实测C3协议，保所有行/首个NO/原15秒边界。
"""
from __future__ import annotations
from collections import Counter
import hashlib
import importlib
import io
import json
import math
import os
import re
from pathlib import Path
import shutil
import sys
import time
import wave
from PIL import Image

SCENARIOS = ('options', 'active-load', 'opening', 'end-turn', 'sfx-reenable')
META = 0x30C3
FIXTURE_META = bytes((1,12,0,5,11,6,14,1,3,4,0,0,0,0,0,1,1,1))
MAX_MS = 30000
INTERVAL_MS = 33
ACK_SECONDS = 15
VOICE_SHA = 'b96497f9c7cef388aad35670abdf2d800f82db3587c5e4fddb9898540d840bed'
VOICE_FRAMES = 2652

def fact(path):
    path = Path(path).resolve(strict=True)
    before = path.stat(); data = path.read_bytes(); after = path.stat()
    if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
        raise AssertionError('input changed while reading: ' + str(path))
    return dict(path=str(path), sha256=hashlib.sha256(data).hexdigest(),
                size=len(data), mtime_ns=after.st_mtime_ns)

def assert_options_only(before, after, values):
    """仅正式Save测量：整plain包含未知尾，只允许两配置字节与checksum改变。"""
    if len(before) != 0x59CB or len(after) != len(before):
        raise AssertionError('Save size')
    if sum(after[:-4]) != int.from_bytes(after[-4:], 'little'):
        raise AssertionError('Save checksum')
    if after[META+16:META+18] != bytes(values):
        raise AssertionError('measured Music/SFX differs')
    expected = bytearray(before)
    expected[META+16:META+18] = bytes(values)
    expected[-4:] = sum(expected[:-4]).to_bytes(4, 'little')
    if after != expected:
        raise AssertionError('non-option Save bytes changed')

def options_match(frame, oracle, values, selected, kind, offsets):
    if kind not in ('original', 'sdl') or selected not in (0, 1):
        raise ValueError('finite options scope')
    commands = (18 + (not values[0]), 20 + (not values[1]),
                22 + values[2], 24 + (not values[3]))
    matches = []
    for index, command in enumerate(commands):
        at = tuple(a+b for a,b in zip(oracle.focus, offsets[index]))
        if kind == 'sdl' and index >= 2:
            # 原disabled全透明，不能当正像素；两种值的normal/bright均须不存在。
            visible = (66,67,69,70) if index == 2 else (72,73,75,76)
            if oracle.choices(frame, visible, at): return None
            matches.append(dict(item=index, disabled_transparent=True, positive_pixels_compared=False))
        else:
            found = oracle.choices(frame, (command*3 + (selected == index),), at)
            if not found: return None
            matches.append(found)
    return dict(kind='options', selected=selected, values=list(values), matches=matches)

def first_selected_yes(oracle, observe_any):
    """同一有界wait：plain继续，首次明确NO即失败；不重起deadline或补键。"""
    observed = False
    def predicate(frame):
        nonlocal observed
        any_buttons = oracle.buttons(frame, None)
        if not any_buttons: return None
        if not observed:
            observe_any(frame, any_buttons); observed = True
        if oracle.buttons(frame, 1):
            raise AssertionError('first explicit selection is NO; later YES cannot wash it out')
        return oracle.buttons(frame, 0)
    return predicate

class OrderedFrames:
    def __init__(self,directory,ready,pid,normalize):
        self.directory=Path(directory);self.ready=ready;self.pid=pid;self.normalize=normalize
        self.offset=0;self.index=0;self.stamp=None

    def unread(self):
        path=self.directory/'frames.jsonl'
        if not path.is_file():return
        if path.stat().st_size<self.offset:raise AssertionError('series JSONL truncated')
        with path.open('rb') as stream:
            stream.seek(self.offset);data=stream.read()
        for part in data.splitlines(keepends=True):
            if not part.endswith(b'\n'):break  # Only the final incomplete row may wait.
            row=json.loads(part)
            if not isinstance(row,dict):raise AssertionError('complete series row is not an object')
            stamp=row.get('wgc_timestamp_100ns');name=f'frame{self.index:05d}.png'
            if (type(row.get('index')) is not int or row['index']!=self.index
                    or row.get('file')!=name or type(row.get('pid')) is not int or row['pid']!=self.pid
                    or row.get('hwnd')!=self.ready['hwnd'] or type(stamp) is not int or stamp<=0
                    or self.stamp is not None and stamp<=self.stamp):
                raise AssertionError('series index/owner/timestamp invalid')
            if self.index==0 and (self.ready.get('first_index')!=0
                    or stamp!=self.ready.get('wgc_timestamp_100ns')):
                raise AssertionError('series first row does not match ready')
            png=self.directory/name;raw=png.read_bytes()
            with Image.open(io.BytesIO(raw)) as image:
                if image.format!='PNG' or image.size!=(row.get('client_width'),row.get('client_height')):
                    raise AssertionError('series PNG/client size invalid')
                frame=self.normalize(image)
            detail={**row,'path':str(png),'sha256':hashlib.sha256(raw).hexdigest()}
            self.offset+=len(part);self.index+=1;self.stamp=stamp
            yield detail,frame

def owner_receipt(record,pid,hwnd):
    rows=[row for row in record['inputs'] if 'key' in row]
    if not rows:raise AssertionError('no owned key receipt to bind series ready')
    delivery=rows[-1].get('delivery',{})
    if (delivery.get('pid')!=pid or delivery.get('hwnd')!=hwnd or delivery.get('ok') is not True
            or delivery.get('error')!=0 or delivery.get('noactivate_verified') is not True
            or delivery.get('foreground_guard_ok') is not True):
        raise AssertionError('series ready and actual key HWND/PID differ')
    return rows[-1]

def ordered_ack(reader,predicate,alive,record,*,clock=time,started=None):
    if started is None:started=clock.monotonic()
    deadline=started+ACK_SECONDS
    record.update(started_monotonic=started,deadline_monotonic=deadline,timeout_seconds=ACK_SECONDS,
                  consumed_frames=[])
    while clock.monotonic()<deadline:
        alive()
        for row,frame in reader.unread():
            alive()
            if clock.monotonic()>=deadline:raise TimeoutError('resource ACK exceeded15s')
            record['consumed_frames'].append(row)
            record['current_frame']=row
            matched=predicate(frame)
            if clock.monotonic()>=deadline:raise TimeoutError('late resource ACK not accepted after15s')
            if matched is not None:
                record.update(completed=True,ACK=row,resource=matched,
                              elapsed_seconds=clock.monotonic()-started)
                return matched
        clock.sleep(.01)
    raise TimeoutError('resource ACK exceeded15s; no extra keys')

def wait_resource_series(capture,prepared,record,name,predicate,open_key=None):
    """One owned session per bright ACK; ordinary deadline starts at call, Save after open_key."""
    began=time.monotonic()
    battle=prepared['tools']['run_original_battle_animation']
    normalize=prepared['tools']['run_original_comparison'].normalize
    directory=capture.artifacts/(name+'-series')
    label=name+'-series-helper'
    slot={};state=dict(name=name,ready_before_open_key=open_key is not None,interval_ms=INTERVAL_MS,max_ms=MAX_MS,
                       frame_cap=600,completed=False,helper=slot)
    record.setdefault('resource_series',[]).append(state)
    if name=='save-final-default-YES':record['save_confirmation_series']=state
    helper=battle.OwnedHelper(capture.driver,[capture.driver,'capture-series',capture.process.pid,
        directory,MAX_MS,INTERVAL_MS],capture.artifacts,label,slot)
    ready=None;first_error=None;summary=None
    try:
        process=helper.start()
        ready=battle.wait_ready(directory/'ready.json',process,capture.process,timeout=5)
        if (ready.get('ready') is not True or type(ready.get('pid')) is not int
                or ready['pid']!=capture.process.pid or not isinstance(ready.get('hwnd'),str)
                or not ready['hwnd'] or type(ready.get('first_index')) is not int or ready['first_index']!=0
                or type(ready.get('wgc_timestamp_100ns')) is not int
                or ready['wgc_timestamp_100ns']<=0):
            raise AssertionError('series ready owner/boundary invalid')
        def alive():
            if capture.process.poll() is not None or process.poll() is not None:
                raise RuntimeError('target or series helper exited before confirmation ACK')
        alive()
        state['ready']=ready;state['ready_fact']=fact(directory/'ready.json')
        state['ready_bound_key_receipt']=owner_receipt(record,capture.process.pid,ready['hwnd'])
        reader=OrderedFrames(directory,ready,capture.process.pid,normalize)
        if open_key is not None:
            state['pre_open_key_receipt']=state['ready_bound_key_receipt']
            open_key()
            state['open_key_receipt']=owner_receipt(record,capture.process.pid,ready['hwnd'])
            started=None
        else:
            started=began  # Ready/session setup consumes the original post-key15s window.
        result=ordered_ack(reader,predicate,alive,state,started=started)
        record['checkpoints'].append(dict(name=name,pid=capture.process.pid,time=time.monotonic(),
            source_frame=state['ACK'],**result))
        # Ordinary menus may match ready frame0 immediately. Preserve that ACK,
        # but satisfy the existing >=2-frame close contract within its same deadline.
        state['post_ACK_close_frames']=[]
        while reader.index<2:
            alive()
            if time.monotonic()>=state['deadline_monotonic']:
                raise TimeoutError('series minimum2 rows not complete within original15s')
            obtained=False
            for row,frame in reader.unread():
                if time.monotonic()>=state['deadline_monotonic']:
                    raise TimeoutError('late minimum2 close row after original15s')
                state['post_ACK_close_frames'].append(row);obtained=True
                if reader.index>=2:break
            if not obtained:time.sleep(.01)
    except BaseException as error:
        first_error=error;state['error']=dict(type=type(error).__name__,message=str(error))
    finally:
        try:
            if directory.is_dir():
                with (directory/'stop').open('x',encoding='ascii') as stream:stream.write('stop\n')
        except BaseException as error:
            state['stop_error']=dict(type=type(error).__name__,message=str(error))
            if first_error is None:first_error=error
        try:
            summary=helper.finish()
            if ready is not None and summary is not None:
                frames=battle.series_index(directory,summary,ready,capture.process.pid)
                state['closed_frames']=frames;state['summary']=summary
                state['summary_path']=str(capture.artifacts/(label+'.stdout.json'))
                for observed in state.get('consumed_frames',[])+state.get('post_ACK_close_frames',[]):
                    closed=frames[observed['index']]
                    if (closed['sha256']!=observed['sha256'] or
                            closed['wgc_timestamp_100ns']!=observed['wgc_timestamp_100ns']):
                        raise AssertionError('observed series PNG changed before normal stop')
            elif first_error is None:
                raise AssertionError('series never published a complete ready/summary')
        except BaseException as error:
            state['finish_error']=dict(type=type(error).__name__,message=str(error))
            if first_error is None:first_error=error
        finally:
            files=[directory/'ready.json',directory/'frames.jsonl',directory/'stop',capture.artifacts/(label+'.stdout.json'),capture.artifacts/(label+'.stderr.log'),*directory.glob('frame*.png')]
            try:
                state['closed_evidence_facts']={str(p.relative_to(capture.artifacts)):fact(p) for p in files if p.is_file()}
            except BaseException as error:
                state['evidence_hash_error']=dict(type=type(error).__name__,message=str(error))
                if first_error is None:first_error=error
    if first_error is not None:
        state['completed']=False
        raise first_error
    state['strict_stop_owner_frame_guard_passed']=True
    return state

def prepare_short(prepared):
    opening=importlib.import_module('fd2_opening_oracle')
    runner=importlib.import_module('run_original_opening')
    pages=opening.build_pages(prepared['original'],prepared['assets'],stage_plan=((32,3),))
    prefix=[p for p in pages if p['fragment'] in (0,1)]
    first=next(p for p in pages if p['fragment']==2)
    if Counter(p['fragment'] for p in prefix)!={0:7,1:22} or len(prefix)!=29:
        raise AssertionError('opening29 exact wait contract')
    if (first['stage'],first['fragment'],first['token_offset'])!=(32,2,41):
        raise AssertionError('first fragment2 wait changed')
    source=prepared['tools']['run_graphics_tests'].decode_save(prepared['original']/'FD2.SAV')
    counts=runner.title_menu_counts(source)
    banner=prepared['tools']['run_original_end_turn'].round_two_template(prepared['original']/'FDOTHER.DAT',prepared['assets']['palette'])
    return dict(opening=opening,prefix=prefix,first=first,title_counts=counts,banner=banner,
        prefix_description=[{k:p[k] for k in ('stage','fragment','token_offset','reason')} for p in prefix])

def observe_audio(capture,record,label,seconds=2):
    """已ACK状态上的声明听取窗；仅存活/源guard，不作输入settle或FPS判据。"""
    started=time.monotonic();deadline=started+seconds
    while time.monotonic()<deadline:
        if capture.process.poll() is not None:raise RuntimeError('target exited during declared audio observation')
        time.sleep(min(.02,max(0,deadline-time.monotonic())))
    record['events'].append(dict(name=label,observation_seconds=time.monotonic()-started,
        requested_seconds=seconds,extra_keys=0,frames_or_FPS_claimed=False))

def exercise_opening(capture,kind,prepared,save,record,mix):
    b=prepared['tools']['run_original_battle_menu'];short=prepared['short'];o=short['opening']
    before,stamp=save.read_bytes(),save.stat().st_mtime_ns
    def unchanged():prepared['tools']['run_original_load'].require_source_unchanged(save,before,stamp)
    def cp(label,page):
        result=b.wait_state(capture,label,lambda frame:o.page_match(frame,page,prepared['assets']),record['checkpoints'],timeout=90)
        unchanged();return result
    count=short['title_counts'][kind]
    b.wait_title(capture,prepared['titles'][count][0]);unchanged()
    capture.key('ENTER')  # 默认START；不是CONTINUE，也不导入旧档。
    acknowledgements=[]
    for ordinal,page in enumerate(short['prefix'],1):
        cp(f'opening-{ordinal:02d}-fragment{page["fragment"]}',page)
        if ordinal==29:
            if mix:start_audio(mix,capture.process,record)
            observe_audio(capture,record,'before-opening-fade-track18');unchanged()
        capture.key('ENTER');acknowledgements.append(dict(ordinal=ordinal,fragment=page['fragment'],token_offset=page['token_offset']))
    cp('opening-first-fragment2-track11-window',short['first'])
    observe_audio(capture,record,'first-fragment2-track11');unchanged()
    if mix:finish_audio(mix,capture.process,record)
    record['opening_acknowledgements']=acknowledgements
    record['terminal_wait_acknowledged']=False
    record['source_save_unchanged']=True
    record['business_sequence_completed']=len(acknowledgements)==29


def validate_scenarios(values):
    values = tuple(values)
    if not values or len(values) != len(set(values)) or any(x not in SCENARIOS for x in values):
        raise ValueError('select nonempty, unique registered A7 scenarios')
    return values


def validate_output(root, output, original=None):
    output = Path(output).resolve()
    if (output.exists() or root == output or root in output.parents or
            original is not None and (output==original or original in output.parents) or
            any(x.casefold() == 'original_game' for x in output.parts) or not output.parent.is_dir()):
        raise ValueError('artifacts must be new, outside source/resources, with an existing parent')
    return output


def load_tools(root):
    if Path(__file__).resolve().parent!=(root/'tools').resolve(strict=True):
        raise ValueError('--root must bind the source tree containing this entry/helper')
    sys.path.insert(0, str(root/'tools'))
    names = ('run_original_comparison', 'run_original_battle_menu', 'run_original_end_turn',
             'run_graphics_tests', 'run_original_load', 'run_original_battle_animation',
             'fd2_app_mix_capture', 'fd2_display_requirement', 'fd2_sfx_oracle',
             'run_background_input_check')
    result={name: importlib.import_module(name) for name in names}
    if any(Path(module.__file__).resolve().parent!=(root/'tools').resolve() for module in result.values()):
        raise ValueError('cached helper belongs to a different source tree')
    return result


def source_facts(root):
    paths = []
    for directory in ('tools', 'src'):
        for path in (root/directory).rglob('*'):
            relative = path.relative_to(root)
            if (path.is_file() and not any(x.startswith('build') or x in
                    ('Debug', 'Release', 'RelWithDebInfo', 'MinSizeRel', '__pycache__', 'ida-out')
                    for x in relative.parts[:-1]) and
                    (path.suffix in ('.py', '.c', '.h', '.cpp', '.hpp', '.cmake') or path.name == 'CMakeLists.txt')):
                paths.append(path)
    return {p.relative_to(root).as_posix(): fact(p) for p in sorted(paths)}


def derive_flags(encoded, plain, values):
    if len(plain) != 0x59cb or len(encoded) != len(plain) or sum(plain[:-4]) != int.from_bytes(plain[-4:], 'little'):
        raise ValueError('require a complete checksum-valid START container')
    if plain[META:META+18] != FIXTURE_META:
        raise ValueError('unsupported START geometry, identities or option layout')
    if tuple(values) not in ((1, 1), (0, 0)):
        raise ValueError('only declared 11/00 inputs are supported')
    controlled = bytearray(plain)
    controlled[META+16:META+18] = bytes(values)
    controlled[-4:] = sum(controlled[:-4]).to_bytes(4, 'little')
    transformed = bytes(a ^ b ^ c for a, b, c in zip(encoded, plain, controlled))
    differences = [dict(offset=i, before=a, after=b) for i, (a, b) in enumerate(zip(plain, controlled)) if a != b]
    allowed = {META+16, META+17, *range(len(plain)-4, len(plain))}
    if any(x['offset'] not in allowed for x in differences):
        raise AssertionError('unknown/container bytes changed')
    return transformed, bytes(controlled), differences


def prepare(args):
    args.scenarios = validate_scenarios(args.scenarios)
    args.root = args.root.resolve(strict=True)
    args.artifacts = validate_output(args.root, args.artifacts)
    for key in ('game', 'driver', 'dosbox'):
        value = getattr(args, key).resolve(strict=True)
        if not value.is_file(): raise ValueError(key+' must be a file')
        setattr(args, key, value)
    if args.dosbox.name.casefold() not in ('dosbox-x','dosbox-x.exe'):
        raise ValueError('this entry requires the verified DOSBox-X quiet/HOST_W path')
    tools = load_tools(args.root)
    original = (args.root/'original_game').resolve(strict=True)
    validate_output(args.root,args.artifacts,original)
    original_facts = {p.name: fact(p) for p in original.iterdir() if p.is_file()}
    for required in ('FD2.EXE', 'FD2.SAV', 'START.SAV', 'FDOTHER.DAT', 'FDFIELD.DAT', 'FDTXT.DAT', 'SAMPLE.AD', 'FDMUS.DAT'):
        if required not in original_facts: raise ValueError('missing original resource: '+required)
    plain = tools['run_graphics_tests'].decode_save(original/'START.SAV')
    if plain[0x30c6:0x30ca] != bytes((5, 11, 8, 15)):
        raise ValueError('unsupported original START viewport/focus')
    focus = bytearray(plain)
    focus[0x30c8:0x30cc] = bytes((6, 14, 1, 3))
    focus[-4:] = sum(focus[:-4]).to_bytes(4, 'little')
    if focus[META:META+18] != FIXTURE_META:
        raise ValueError('unsupported START metadata; no fixture guessed')
    assets = tools['run_original_end_turn'].round_assets(original)
    prepared = dict(tools=tools, original=original, original_facts=original_facts,
        source=source_facts(args.root), inputs={key: fact(getattr(args,key)) for key in ('game','driver','dosbox')},
        focus_plain=bytes(focus), assets=assets,
        titles=tools['run_original_comparison'].title_templates(original/'FDOTHER.DAT'))
    prepared['short'] = prepare_short(prepared)
    if 'sfx-reenable' in args.scenarios:
        refs=tools['fd2_sfx_oracle'].references(original/'FDOTHER.DAT',[dict(bank=31,sample=i) for i in range(13)])
        if refs[8]['sha256']!=VOICE_SHA or refs[8]['source_frames']!=VOICE_FRAMES:
            raise ValueError('unsupported sample8 source; no capture or alternate template attempted')
    return prepared


def wait_confirmation(capture, prepared, record, oracle, key, label):
    def first(frame, detail):
        state = record['resource_series'][-1]
        record['checkpoints'].append(dict(name=label+'-first-any', pid=capture.process.pid,
            time=time.monotonic(), source_frame=state['current_frame'], **detail))
    return wait_resource_series(capture, prepared, record, label,
        first_selected_yes(oracle, first), open_key=lambda: key('ENTER'))


class NativeWave:
    """显式HOST_W首尾；只声明已投递请求和闭合WAV，不冒充首PCM/QPC ready。"""
    def __init__(self, capture, directory, battle):
        self.capture, self.directory, self.battle = capture, directory, battle
        self.active = False;self.started=False
    def start(self, process):
        if self.active or list(self.directory.iterdir()): raise AssertionError('native recording must be a new window')
        self.capture.key('HOST_W'); self.active = True;self.started=True;self.started_monotonic=time.monotonic()
        return dict(backend='DOSBox-native-HOST_W', pid=process.pid,
            request_delivery=self.capture.trace[-1], first_PCM_clock_observed=False)
    def assert_running(self, process):
        if process.poll() is not None or not self.active: raise RuntimeError('native target/recording no longer active')
        if time.monotonic()-self.started_monotonic>=120:raise TimeoutError('declared native audio window exceeded120s')
    def request_stop(self):
        if self.active and self.capture.process.poll() is None:
            self.capture.key('HOST_W'); self.active = False
    def finish(self, process):
        if not self.started or process.poll() is not None:raise RuntimeError('native recording never started or target exited')
        self.request_stop()
        deadline = time.monotonic()+5; prior = None
        while time.monotonic() < deadline:
            paths = list(self.directory.glob('*.wav'))
            if len(paths) == 1:
                try:
                    current = fact(paths[0])
                    if current == prior:
                        info = self.battle.wav_info(paths[0])
                        if info['frames'] <= 0: raise AssertionError('empty native WAV')
                        if info['frames']>120*info['sample_rate']:raise AssertionError('native audio exceeds declared120s cap')
                        return dict(backend='DOSBox-native-HOST_W', pid=process.pid,
                            closed_wave=info, stop_delivery=self.capture.trace[-1],
                            first_PCM_clock_observed=False, device_output_validated=False)
                    prior = current
                except (OSError, ValueError, EOFError, wave.Error):
                    pass
            if process.poll() is not None: raise RuntimeError('native target exited before WAV finalized')
            time.sleep(.05)
        raise TimeoutError('exactly one closed native WAV was not produced after HOST_W stop')


def checkpoint(capture, prepared, record, name, predicate, *, stable=False, **kwargs):
    if stable:
        return prepared['tools']['run_original_battle_menu'].wait_state(
            capture, name, predicate, record['checkpoints'], **kwargs)
    if kwargs: raise ValueError('bright ACK uses the original fixed15s window')
    state = wait_resource_series(capture, prepared, record, name, predicate)
    return None, state['resource']


def stop_audio(audio, record):
    pending=sys.exc_info()[0] is not None
    try:audio.request_stop()
    except BaseException as error:
        record.setdefault('capture_stop_error',dict(type=type(error).__name__,message=str(error)))
        if not pending:raise


def start_audio(audio, process, record):
    record['audio_start_attempted']=True
    record['audio_ready']=audio.start(process)


def finish_audio(audio, process, record):
    record['audio_finish_attempted']=True
    record['audio_capture']=audio.finish(process)


def close_audio_before_host(audio, process, record):
    """失败也先等待封包；已失败的finish不再尝试，不以清理错误覆盖原业务首错。"""
    pending=sys.exc_info()[0] is not None
    try:
        if record.get('audio_start_attempted') and not record.get('audio_finish_attempted'):
            finish_audio(audio,process,record)
        else:stop_audio(audio,record)
    except BaseException as error:
        record.setdefault('capture_close_error',dict(type=type(error).__name__,message=str(error)))
        if not pending:raise


def exercise_options_load(capture, kind, prepared, save, record, audio, episode):
    tools=prepared['tools']; b=tools['run_original_battle_menu']; e=tools['run_original_end_turn']
    oracle=e.RoundOracle(prepared['assets'], (28,76)); values=[1,1,0,1]
    guard=[save.read_bytes(), save.stat().st_mtime_ns]; active=False
    def key(name):
        if active: audio.assert_running(capture.process)
        capture.key(name)
    def cp(name, predicate, **kwargs):
        if active: audio.assert_running(capture.process)
        result=checkpoint(capture,prepared,record,name,predicate,stable=name.endswith('browse'),**kwargs)
        tools['run_original_load'].require_source_unchanged(save,*guard)
        return result
    def secondary(label,index,direction):
        key('ENTER');cp(label+'-parent',lambda f:oracle.menu(f,0))
        key('UP');cp(label+'-top',lambda f:oracle.menu(f,0))
        key('ENTER');cp(label+'-secondary',lambda f:oracle.secondary(f,0))
        key(direction);cp(label+'-selected',lambda f:oracle.secondary(f,index))
        wait_confirmation(capture,prepared,record,oracle,key,label+'-default-YES')
        tools['run_original_load'].require_source_unchanged(save,*guard)
    def save_now(label):
        secondary('save-'+label,1,'LEFT');key('ENTER')
        measured=tools['run_original_load'].wait_measured_save(save,*guard)
        guard[:]=[save.read_bytes(),save.stat().st_mtime_ns]
        cp('save-'+label+'-browse',oracle.closed_browse)
        path=capture.artifacts/(label+'.SAV');shutil.copy2(save,path)
        record['saves'].append(dict(name=label,snapshot=fact(path),live_mtime_ns=guard[1]))
        return measured
    def open_options(label):
        key('ENTER');cp(label+'-parent',lambda f:oracle.menu(f,0))
        key('RIGHT');cp(label+'-right',lambda f:oracle.menu(f,2))
        key('ENTER');cp(label+'-options',lambda f:options_match(f,oracle,values,0,kind,b.MENU_OFFSETS))
    def close_options(label):
        key('ESC');cp(label+'-closed-parent',lambda f:oracle.menu(f,0))
        key('ESC');cp(label+'-browse',oracle.closed_browse)
    def toggle(label,index):
        open_options(label)
        if index==1:
            key('LEFT');cp(label+'-sfx-selected',lambda f:options_match(f,oracle,values,1,kind,b.MENU_OFFSETS))
        record['events'].append(dict(name=label+'-before',clock=tools['run_original_battle_animation'].qpc_stamp(),values=list(values)))
        key('ENTER');values[index]=int(not values[index])
        cp(label+'-changed',lambda f:options_match(f,oracle,values,index,kind,b.MENU_OFFSETS))
        record['events'].append(dict(name=label+'-after',clock=tools['run_original_battle_animation'].qpc_stamp(),values=list(values)))
        close_options(label)
    b.wait_title(capture,prepared['titles'][3][0]);key('UP');capture.wait(prepared['titles'][3][2]);key('ENTER')
    cp('continued-primary19-browse',oracle.closed_browse,timeout=45)
    baseline=save_now('baseline')
    record['initial_load_measurement']=tools['run_original_load'].check_load_measurement(prepared['plain'],baseline,[])
    start_audio(audio,capture.process,record);active=True
    try:
        if episode=='options':
            for name,index in (('music-off',0),('music-on',0),('sfx-off',1),('sfx-on',1)):
                toggle(name,index)
                if name=='sfx-on':finish_audio(audio,capture.process,record);active=False
                assert_options_only(baseline,save_now(name),values[:2])
        else:
            toggle('before-load-music-off',0);toggle('before-load-sfx-off',1)
            for branch in ('escape','no'):
                secondary('load-'+branch,2,'RIGHT')
                if branch=='no':key('RIGHT');cp('load-no-highlight',lambda f:oracle.buttons(f,1))
                key('ESC' if branch=='escape' else 'ENTER')
                cp('load-'+branch+'-browse',oracle.closed_browse,transition=oracle.cancellation_notice)
                open_options('load-'+branch+'-flags');close_options('load-'+branch+'-flags')
                record['cancellations'].append(dict(branch=branch,live_options=[0,0],disk_unchanged=True))
            secondary('load-yes',2,'RIGHT');key('ENTER')
            cp('loaded-A-browse',oracle.closed_browse,timeout=45)
            values[:2]=[1,1];open_options('loaded-A-flags');close_options('loaded-A-flags')
            finish_audio(audio,capture.process,record);active=False
            final=save_now('loaded-A-final');assert_options_only(baseline,final,(1,1))
            if final!=baseline:raise AssertionError('Load did not restore complete baseline')
        record['business_sequence_completed']=True
    finally:
        if active:stop_audio(audio,record)


def exercise_end(capture,kind,prepared,save,record,audio):
    tools=prepared['tools'];b=tools['run_original_battle_menu'];e=tools['run_original_end_turn']
    guard=[save.read_bytes(),save.stat().st_mtime_ns];oracle=e.RoundOracle(prepared['assets'],(28,76))
    def key(name):capture.key(name)
    def cp(name,predicate,stable=False,**kwargs):
        return checkpoint(capture,prepared,record,name,predicate,stable=stable,**kwargs)
    b.wait_title(capture,prepared['titles'][3][0]);key('UP');capture.wait(prepared['titles'][3][2]);key('ENTER')
    cp('initial-browse',oracle.initial_browse,True,timeout=45)
    start_audio(audio,capture.process,record)
    try:
        observe_audio(capture,record,'primary19-before-END',3)
        key('ENTER');cp('parent',lambda f:oracle.menu(f,0))
        key('DOWN');cp('end-selected',lambda f:oracle.menu(f,3))
        wait_confirmation(capture,prepared,record,oracle,key,'end-default-YES')
        key('ENTER')  # 唯一肯定END；后续只观察，不重复提交。
        if kind=='original':
            def banner(frame):
                found=e.match_indexed(frame,prepared['short']['banner'],(120,84))
                return dict(kind='round_banner',turn=2,**found) if found['ok'] else None
            cp('round-002',banner,True,timeout=90)
        else:
            from field_play_input_test import wait_log
            text=wait_log(capture.process,capture.artifacts/'game.log','field phase: turn=2 phase=2 ',timeout=90)
            e.check_round_phase_log(text)
        cp('new-player-browse',lambda f:e.browse_anywhere(f,prepared['assets'],leader=True),True,timeout=30)
        observe_audio(capture,record,'returned-primary19',3)
        finish_audio(audio,capture.process,record)
    finally:stop_audio(audio,record)
    key('LEFT');_,detail=cp('save-cell',lambda f:e.browse_anywhere(f,prepared['assets']),True)
    oracle=e.RoundOracle(prepared['assets'],tuple(detail['focus_screen']))
    key('ENTER');cp('save-parent',lambda f:oracle.menu(f,0))
    key('UP');cp('save-top',lambda f:oracle.menu(f,0))
    key('ENTER');cp('save-secondary',lambda f:oracle.secondary(f,0,overview_available=True))
    key('LEFT');cp('save-enabled',lambda f:oracle.secondary(f,1,overview_available=True))
    wait_confirmation(capture,prepared,record,oracle,key,'round-save-default-YES')
    key('ENTER');measured=tools['run_original_load'].wait_measured_save(save,*guard)
    cp('saved-browse',oracle.closed_browse,True)
    metadata=tools['run_original_comparison'].archive_entry((prepared['original']/'FDFIELD.DAT').read_bytes(),1)
    record['measurement']=e.check_saved_round(prepared['plain'],measured,len(metadata))
    path=capture.artifacts/'round002.SAV';shutil.copy2(save,path)
    record['saves'].append(dict(name='round002',snapshot=fact(path),live_mtime_ns=save.stat().st_mtime_ns))
    record['business_sequence_completed']=True


def exercise_reenable(capture, kind, prepared, save, record, audio):
    """16f55/1728c：Music保持0，SFX0→1；录音首尾不包含最终Save。"""
    tools=prepared['tools'];b=tools['run_original_battle_menu']
    oracle=tools['run_original_end_turn'].RoundOracle(prepared['assets'],(28,76))
    values=[0,0,0,1];guard=[save.read_bytes(),save.stat().st_mtime_ns];active=False
    def event(name):
        record['events'].append(dict(name=name,clock=tools['run_original_battle_animation'].qpc_stamp(),values=list(values)))
    def key(name):
        if active:audio.assert_running(capture.process)
        capture.key(name)
    def cp(name,predicate,*,stable=False,**kwargs):
        if active:audio.assert_running(capture.process)
        result=checkpoint(capture,prepared,record,name,predicate,stable=stable,**kwargs)
        tools['run_original_load'].require_source_unchanged(save,*guard)
        return result
    def parent(label):
        key('ENTER');cp(label,lambda f:oracle.menu(f,0))
    def open_options(label):
        parent(label+'-parent');key('RIGHT');cp(label+'-right',lambda f:oracle.menu(f,2))
        key('ENTER');cp(label+'-options',lambda f:options_match(f,oracle,values,0,kind,b.MENU_OFFSETS))
    def close_options(label):
        key('ESC');cp(label+'-parent',lambda f:oracle.menu(f,0))
        key('ESC');cp(label+'-browse',oracle.closed_browse,stable=True)
    b.wait_title(capture,prepared['titles'][3][0]);key('UP');capture.wait(prepared['titles'][3][2]);key('ENTER')
    cp('continued-00-browse',oracle.closed_browse,stable=True,timeout=45)
    open_options('initial-00');event('initial-00-resource-proven');close_options('initial-00-close')
    observe_audio(capture,record,'declared-Title-fade-clearance',2)
    start_audio(audio,capture.process,record);active=True
    try:
        event('off-observation-before');parent('off-open-parent');key('ESC')
        cp('off-close-browse',oracle.closed_browse,stable=True)
        open_options('reenable');key('LEFT')
        cp('reenable-sfx-selected',lambda f:options_match(f,oracle,values,1,kind,b.MENU_OFFSETS))
        event('off-observation-after');event('sfx-on-before');key('ENTER');values[1]=1
        cp('reenable-01',lambda f:options_match(f,oracle,values,1,kind,b.MENU_OFFSETS))
        event('sfx-on-after');event('positive-ESC-before');key('ESC')
        cp('positive-parent',lambda f:oracle.menu(f,0));event('positive-parent-resource-proven')
        observe_audio(capture,record,'complete-declared-VOICE-tail',math.ceil(VOICE_FRAMES*1000/11025)/1000)
        finish_audio(audio,capture.process,record);active=False
    finally:
        if active:stop_audio(audio,record)
    key('ESC');cp('post-capture-browse',oracle.closed_browse,stable=True)
    parent('save-final-parent');key('UP');cp('save-final-top',lambda f:oracle.menu(f,0))
    key('ENTER');cp('save-final-secondary',lambda f:oracle.secondary(f,0))
    key('LEFT');cp('save-final-selected',lambda f:oracle.secondary(f,1))
    wait_confirmation(capture,prepared,record,oracle,key,'save-final-default-YES');key('ENTER')
    measured=tools['run_original_load'].wait_measured_save(save,*guard)
    guard[:]=[save.read_bytes(),save.stat().st_mtime_ns]
    cp('save-final-browse',oracle.closed_browse,stable=True)
    expected=bytearray(prepared['plain']);expected[META+17]=1
    expected[-4:]=sum(expected[:-4]).to_bytes(4,'little')
    record['measurement']=tools['run_original_load'].check_load_measurement(bytes(expected),measured,[])
    if measured[META+16:META+18]!=b'\0\1':raise AssertionError('final Save must be Music0/SFX1')
    snapshot=capture.artifacts/'reenabled-01.SAV';shutil.copy2(save,snapshot)
    record['saves'].append(dict(name='reenabled-01',snapshot=fact(snapshot),live_mtime_ns=guard[1]))
    record.update(final_options=[0,1],music_changed_during_episode=False,business_sequence_completed=True)


def verify_reenable(prepared, record):
    oracle=prepared['tools']['fd2_sfx_oracle'];wave_info=record['audio_wave']
    candidates=[dict(bank=31,sample=i) for i in range(13)]
    refs=oracle.references(prepared['original']/'FDOTHER.DAT',candidates)
    if refs[8]['sha256']!=VOICE_SHA or refs[8]['source_frames']!=VOICE_FRAMES:
        raise AssertionError('full2652 independent sample8 reference changed')
    request=dict(schema='fd2-sfx-query-v1',candidates=candidates,min_score=.8,identity_margin=.05,
        queries=[dict(name='dedicated-reenabled-cue',start_ms=0,
            end_ms=wave_info['frames']*1000/wave_info['sample_rate'],expected=dict(bank=31,sample=8))])
    observation=oracle.analyze(prepared['original']/'FDOTHER.DAT',request,Path(wave_info['path']))
    negative=dict(status='pending_native_first_PCM_clock',acceptance_gate=False,absolute_absence_proven=False)
    if record['kind']=='sdl':
        ready=record['audio_ready'];events={e['name']:e['clock'] for e in record['events'] if 'clock' in e}
        lo=(events['off-observation-before']['qpc']-ready['first_callback_qpc'])*1000/ready['qpc_frequency']
        hi=(events['off-observation-after']['qpc']-ready['first_callback_qpc'])*1000/ready['qpc_frequency']
        off=dict(request,queries=[dict(name='bounded-OFF-observation',start_ms=lo,end_ms=hi,expected=dict(bank=31,sample=8))])
        negative.update(status='observed_same_callback_clock_window',
            observation=oracle.analyze(prepared['original']/'FDOTHER.DAT',off,Path(wave_info['path'])))
    return dict(ok=observation['queries'][0]['identity_status']=='supported_candidate',
        request=request,observation=observation,off_observation=negative,
        repeated_cue_count_or_unique_ESC_instance_proven=False,default_amplitude_verified=False,
        exact_timing_verified=False,device_output_validated=False,A7_fidelity_validated=False)


def native_config_text(game,captures):
    # DX-CAPTURE全false仍隐式video；显式HOST_W只录本次声明窗。
    return ('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n'
        '[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\nshow recorded filename=false\ncaptures='+str(captures)+'\n'
        '[render]\naspect=false\nscaler=normal2x\n[mixer]\nrate=44100\n'
        '[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "'+str(game)+'"\nc:\nFD2.EXE\nexit\n')


def finalize_guards(args,prepared,report):
    checks={
        'current_source_set_SHA_size_mtime':lambda:source_facts(args.root)==prepared['source'],
        'original_resource_set_SHA_size_mtime':lambda:{p.name:fact(p) for p in prepared['original'].iterdir() if p.is_file()}==prepared['original_facts'],
        'binary_and_derived_inputs':lambda:all(fact(row['path'])==row for row in prepared['inputs'].values()),
        'closed_evidence_files':lambda:all(case.get('closed_evidence_sealed') is True and
            all(fact(row['path'])==row for row in case.get('closed_capture_facts',{}).values())
            for case in report.get('cases',[])),
    }
    for name,check in checks.items():
        try:
            passed=check();report.setdefault('guards',{})[name]=report.get('guards',{}).get(name,True) and passed
            if not passed:raise AssertionError('SHA/size/mtime or file-set changed')
        except BaseException as error:
            report.setdefault('guards',{})[name]=False
            report.setdefault('guard_errors',{}).setdefault(name,dict(type=type(error).__name__,message=str(error)))
    return bool(report.get('guards')) and all(v is True for v in report['guards'].values())


def music_requests(log, scenario):
    """实际SDL请求返回值，不把请求日志或WAV存在当波形/音色正确。"""
    rows=[]
    for line in log.splitlines():
        if not line.startswith('music request: '):continue
        match=re.fullmatch(r'music request: stage=(\d+) track=(-?\d+) reason=(\S+) available=(\d+) result=(-?\d+) current=(-?\d+) loop=(\d+)',line)
        if not match:raise AssertionError('malformed SDL music request receipt')
        values=match.groups();row=dict(zip(('stage','track','reason','available','result','current','loop'),
            [int(v) if i!=2 else v for i,v in enumerate(values)]))
        if row['available']!=1 or row['result']!=0 or row['loop']!=0:
            raise AssertionError('SDL music backend missing or request failed')
        rows.append(row)
    required={'options':(19,), 'active-load':(19,), 'end-turn':(19,12,19), 'sfx-reenable':(19,), 'opening':()}[scenario]
    position=0
    for row in rows:
        if position<len(required) and row['track']==required[position]:position+=1
    if position!=len(required):raise AssertionError('expected ordered SDL phase requests missing')
    if scenario=='active-load' and not any(r['reason']=='active-load-stop' and r['track']==-1 for r in rows):
        raise AssertionError('successful active Load stop/restart request absent')
    return dict(actual_requests=rows,required_ordered_subsequence=list(required),
        opening_18_to_11_source='scene resource ACKs and native-equivalent Scene call path; no separate request log' if scenario=='opening' else None,
        acoustic_track_identity_proven=False,waveform_or_timbre_in_scope=False)


def qualify_case(record,process_receipt,scenario):
    """只关本入口声明的功能/采集门；SFX场另加预声明身份门。"""
    normal=(process_receipt.get('cleaned') is True and process_receipt.get('forced_cleanup') is False
        and type(process_receipt.get('exit_code')) is int and process_receipt['exit_code']==0)
    functional=(record.get('business_sequence_completed') is True and normal and
        not record.get('capture_stop_error') and not record.get('capture_close_error') and not record.get('error'))
    collection=record.get('audio_collection_passed') is True
    finite=(scenario!='sfx-reenable' or record.get('finite_sfx_verification',{}).get('ok') is True)
    return dict(ok=functional and collection and finite,
        functional_business_passed=functional,audio_collection_passed=collection,
        finite_SFX_required=scenario=='sfx-reenable',finite_SFX_passed=finite if scenario=='sfx-reenable' else None,
        BGM_timbre_waveform_absolute_timing='not_in_scope',device_output_validated=False,A7_fidelity_validated=False)


def execute_case(args,prepared,scenario,report):
    tools=prepared['tools'];common=tools['run_original_comparison'];battle=tools['run_original_battle_animation']
    source=prepared['controlled00'] if scenario=='sfx-reenable' else prepared['focus_fixture']
    if scenario=='opening':source=prepared['original']/'FD2.SAV'
    case_root=args.artifacts/scenario;case_root.mkdir()
    for kind in ('original','sdl'):
        work=case_root/kind;work.mkdir()
        record=dict(scenario=scenario,kind=kind,status='failed',checkpoints=[],inputs=[],saves=[],events=[],cancellations=[],
            source_save=fact(source),audio_recorded_this_job=True,closed_capture_facts={})
        report['cases'].append(record)
        removed=[k for k in os.environ if k.upper() in ('SDL_VIDEODRIVER','SDL_AUDIODRIVER','FD2SDL_FIELD_RNG_SEED',
            'FD2SDL_SAVE_PATH','FD2SDL_TEST_AUDIO_QUIET','FD2SDL_TEST_WGC_MIN_UPDATE_MS') or
            k.upper().startswith(('FD2SDL_APP_PCM_CAPTURE_','DOSBOX_'))]
        env={k:v for k,v in os.environ.items() if k not in removed}
        record['child_environment_removed_names']=sorted(removed)
        if kind=='original':
            game=work/'game';shutil.copytree(prepared['original'],game,copy_function=shutil.copy2)
            save=game/'FD2.SAV';shutil.copy2(source,save);captures=work/'captures';captures.mkdir()
            config=work/'dosbox.conf';config.write_text(native_config_text(game,captures),encoding='utf-8')
            command,cwd=[str(args.dosbox),'-conf',str(config),'-noconsole'],work
            record['isolated_game_before']={p.name:fact(p) for p in game.iterdir() if p.is_file()}
        else:
            save=work/'FD2.SAV';shutil.copy2(source,save);folder=work/'app-mix';folder.mkdir()
            env.update(FD2SDL_SAVE_PATH=str(save),FD2SDL_APP_PCM_CAPTURE_DIR=str(folder),
                FD2SDL_APP_PCM_CAPTURE_MAX_MS='120000',FD2SDL_APP_PCM_CAPTURE_QUIET='1',FD2SDL_TEST_AUDIO_QUIET='1')
            command,cwd=[str(args.game)],args.root
        endpoint=dict(prepared,plain=tools['run_graphics_tests'].decode_save(source))
        try:
            with common.running(command,cwd,env,work/'game.log',args.driver,report['processes']) as process:
                record['pid']=process.pid;capture=common.Capture(args.driver,process,work,record['inputs'])
                audio=(NativeWave(capture,captures,battle) if kind=='original' else
                    tools['fd2_app_mix_capture'].AppMixCapture(folder,process.pid,120000))
                try:
                    if scenario in ('options','active-load'):
                        exercise_options_load(capture,kind,endpoint,save,record,audio,scenario)
                    elif scenario=='opening':exercise_opening(capture,kind,endpoint,save,record,audio)
                    elif scenario=='end-turn':exercise_end(capture,kind,endpoint,save,record,audio)
                    else:exercise_reenable(capture,kind,endpoint,save,record,audio)
                    record['completed_live_save_fact']=fact(save)
                finally:
                    close_audio_before_host(audio,process,record)
            if fact(save)!=record['completed_live_save_fact']:raise AssertionError('normal close changed verified live Save')
            if kind=='original':
                if report['processes'][-1].get('native_quiet_requested') is not True:
                    raise AssertionError('owned DOSBox quiet request was not applied')
                after={p.name:fact(p) for p in game.iterdir() if p.is_file()};before=record['isolated_game_before']
                mutable={'FD2.SAV','FD2.TMP'}
                record['isolated_game_after']=after
                if (not all(after.get(k)==v for k,v in before.items() if k.upper() not in mutable) or
                        not all(k in before or k.upper() in mutable for k in after)):
                    raise AssertionError('isolated original resource changed outside live Save/cache')
                if [p for p in captures.iterdir() if p.suffix.lower()!='.wav'] or len(list(captures.glob('*.wav')))!=1:
                    raise AssertionError('native window must contain exactly one explicit HOST_W WAV')
                record['audio_wave']=record['audio_capture']['closed_wave']
                if fact(record['audio_wave']['path'])['sha256']!=record['audio_wave']['sha256']:
                    raise AssertionError('native finalized WAV changed at normal close')
                record['native_DRO_required']=False
            else:
                info=battle.wav_info(folder/'app-mix.wav',expected_rate=48000)
                summary=record['audio_capture']['summary']
                if info['frames']!=summary['frames'] or info['data_bytes']!=summary['data_bytes']:
                    raise AssertionError('app-mix WAV/summary differ')
                record['audio_wave']=info
                record['music_request_observation']=music_requests((work/'game.log').read_text(encoding='utf-8'),scenario)
            record['audio_collection_passed']=True
            if scenario=='sfx-reenable':record['finite_sfx_verification']=verify_reenable(prepared,record)
            record['qualification']=qualify_case(record,report['processes'][-1],scenario)
            if record['qualification']['ok'] is not True:raise AssertionError('declared functional/collection/SFX gate failed')
            record['status']='passed'
        except BaseException as error:
            record.setdefault('error',dict(type=type(error).__name__,message=str(error)))
            raise
        finally:
            # live Save的mtime属于live；snapshot copy2拥有自己的fact，绝不交叉比较两者。
            pending=sys.exc_info()[0] is not None
            try:
                record['closed_capture_facts']={p.relative_to(work).as_posix():fact(p) for p in work.rglob('*')
                    if p.is_file() and not ('game' in p.relative_to(work).parts)}
                record['closed_evidence_sealed']=True
            except BaseException as error:
                record['closed_evidence_sealed']=False
                record.setdefault('evidence_seal_error',dict(type=type(error).__name__,message=str(error)))
                if not pending:raise
            finalize_guards(args,prepared,report)
        if not all(v is True for v in report['guards'].values()):
            raise AssertionError('endpoint late guard failed; next endpoint not launched')


def execute(args,prepared,report):
    prepared['tools']['run_original_battle_animation'].owned_job_required()
    args.artifacts.mkdir(exist_ok=False)
    report['artifacts_created']=True
    fixture=args.artifacts/'focus-6-14.SAV'
    manifest=prepared['tools']['run_background_input_check'].fixture_from_start(args.root,fixture)
    encoded=fixture.read_bytes();plain=prepared['tools']['run_graphics_tests'].decode_save(fixture)
    if plain!=prepared['focus_plain']:raise AssertionError('public fixture_from_start differs from preflight')
    transformed,controlled,changes=derive_flags(encoded,plain,(0,0))
    controlled_path=args.artifacts/'controlled-00.SAV';controlled_path.write_bytes(transformed)
    if prepared['tools']['run_graphics_tests'].decode_save(controlled_path)!=controlled:raise AssertionError('00 codec roundtrip')
    prepared.update(focus_fixture=fixture,controlled00=controlled_path)
    prepared['inputs'].update(focus_fixture=fact(fixture),controlled00=fact(controlled_path))
    report.update(fixture_from_start=manifest,controlled00_changes=changes,inputs=prepared['inputs'])
    for scenario in args.scenarios:execute_case(args,prepared,scenario,report)


def qualify_report(report,scenarios):
    expected=[(scenario,kind) for scenario in scenarios for kind in ('original','sdl')]
    actual=[(c.get('scenario'),c.get('kind')) for c in report.get('cases',[])]
    return (not report.get('error') and actual==expected and
        len(report.get('processes',[]))==len(expected) and
        all(c.get('status')=='passed' and c.get('qualification',{}).get('ok') is True for c in report['cases']) and
        bool(report.get('guards')) and all(v is True for v in report['guards'].values()) and
        report.get('display_requirement',{}).get('status') in ('released','not_applicable'))


def declaration(scenarios):
    return dict(scenarios=list(scenarios),roles=['original','sdl'],fresh_audio_on_each_endpoint=True,
        fixture='public fixture_from_start plus explicit checksum/00 flag derivation; no RNG override',
        accepted_scope='A7 functional resource/Save/request results, actual closed audio collection, plus declared finite SFX identity',
        episode_scope={'options':'11→01→11→10→11; explicit Save after each toggle',
            'active-load':'baseline11; live00; ESC/NO preserve00; YES restores complete baseline11',
            'opening':'START29 resource ACKs; first stage32fragment2/track11 wait held',
            'end-turn':'one END; round002/player Save; phase requests19→12→19',
            'sfx-reenable':'Music0 throughout; SFX00→01; full sample8/13 candidates/.8/.05'},
        bright_ACK=dict(interval_ms=INTERVAL_MS,max_ms=MAX_MS,timeout_seconds=ACK_SECONDS,frame_cap=600,
            all_complete_rows_in_order=True,first_explicit_NO_fails=True),
        audio=dict(original='explicit HOST_W closed44100PCM16 stereo',
            sdl='actual pre-OS application mix48000PCM16 stereo; device submission suppressed',max_ms=120000),
        BGM_timbre_waveform_absolute_timing='not_in_scope',negative_absolute_silence_proven=False,
        exact_cue_instance_or_latency_proven=False,device_output_validated=False,A7_fidelity_validated=False)
