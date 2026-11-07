"""窄流程所有bright ACK共用既有persistent WGC；Save保首个明确NO。"""
import hashlib
import io
import json
from pathlib import Path
import time
from PIL import Image
import options_load as o

MAX_MS=30000
INTERVAL_MS=33
ACK_SECONDS=15


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
        state['ready']=ready;state['ready_fact']=o.fact(directory/'ready.json')
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
                state['closed_evidence_facts']={str(p.relative_to(capture.artifacts)):o.fact(p) for p in files if p.is_file()}
            except BaseException as error:
                state['evidence_hash_error']=dict(type=type(error).__name__,message=str(error))
                if first_error is None:first_error=error
    if first_error is not None:
        state['completed']=False
        raise first_error
    state['strict_stop_owner_frame_guard_passed']=True
    return state


def wait_save_yes(capture,prepared,record,oracle,key):
    """Only confirmation adds first-explicit-NO semantics and a ready-before-open key."""
    def first(frame,detail):
        state=record['save_confirmation_series']
        record['checkpoints'].append(dict(name='save-final-first-any',pid=capture.process.pid,
            time=time.monotonic(),source_frame=state['current_frame'],**detail))
    return wait_resource_series(capture,prepared,record,'save-final-default-YES',
        o.first_selected_yes(oracle,first),open_key=lambda:key('ENTER'))
