"""S2 ticks专用有界WGC重采：只增加真实会话，不生成帧或改变游戏输入。"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import threading
import time

from PIL import Image
from run_original_battle_animation import OwnedHelper, qpc_stamp, wait_ready
from run_original_comparison import normalize

GEOMETRY=('content_width','content_height','client_width','client_height','crop_x','crop_y')
MINIMUM_SPAN_100NS={1:2_000_000,2:2_000_000,3:2_000_000,4:0}


def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def live_frames(directory,ready,pid,known=()):
    """只读取完整行；逐张保留原件，不能用latest跳过非匹配行。"""
    directory=Path(directory);path=directory/'frames.jsonl'
    if not path.is_file():return []
    rows=[];last=None
    for line in path.read_bytes().splitlines(keepends=True):
        if not line.endswith(b'\n'):break
        row=json.loads(line);index=len(rows);stamp=row.get('wgc_timestamp_100ns')
        if (row.get('index')!=index or row.get('file')!=f'frame{index:05d}.png' or
            row.get('pid')!=pid or row.get('hwnd')!=ready.get('hwnd') or type(stamp) is not int or
            stamp<0 or last is not None and stamp<=last):raise AssertionError('S2 live WGC行的owner/index/时间戳不符')
        raw_hash=hashlib.sha256(line).hexdigest()
        if index<len(known):
            if known[index]['raw_row_sha256']!=raw_hash:raise AssertionError('S2 primary live原始行改变')
            rows.append(known[index]);last=stamp;continue
        png=directory/row['file']
        with Image.open(png) as image:
            if image.format!='PNG' or image.size!=(row.get('client_width'),row.get('client_height')):
                raise AssertionError('S2 live WGC的PNG/client尺寸不符')
            normalize(image)
            pixels=hashlib.sha256(image.convert('RGBA').tobytes()).hexdigest()
        if (row.get('client_width'),row.get('client_height'))!=(ready.get('client_width'),ready.get('client_height')):
            raise AssertionError('S2 live WGC与ready尺寸不符')
        rows.append({**row,'path':str(png),'sha256':digest(png),'pixel_sha256':pixels,'raw_row_sha256':raw_hash});last=stamp
    if rows and (ready.get('first_index')!=0 or rows[0]['wgc_timestamp_100ns']!=ready.get('wgc_timestamp_100ns')):
        raise AssertionError('S2 live WGC与ready起点不符')
    return rows


def merge_frames(primary,sessions):
    """按全局原WGC时间合并；相同时间不能增加帧数，冲突不得择优。"""
    if not primary:raise AssertionError('S2合并没有primary真实帧')
    rows=[];anchor=primary[0]
    for name,frames in [('primary',primary)]+[(s['session'],s['frames']) for s in sessions]:
        for frame in frames:
            row={**frame,'capture_session':name,'source_index':frame['index']}
            if any(row.get(k)!=anchor.get(k) for k in ('pid','hwnd',*GEOMETRY)):
                raise AssertionError('S2合并会话owner/几何不同')
            with Image.open(row['path']) as image:
                pixels=hashlib.sha256(image.convert('RGBA').tobytes()).hexdigest()
            if digest(row['path'])!=row['sha256']:raise AssertionError('S2合并前WGC原件身份改变')
            if row.get('pixel_sha256',pixels)!=pixels:raise AssertionError('S2合并前WGC像素身份改变')
            row['pixel_sha256']=pixels;rows.append(row)
    rows.sort(key=lambda r:(r['wgc_timestamp_100ns'],r['capture_session'],r['source_index']))
    merged=[]
    for row in rows:
        source={k:row[k] for k in ('capture_session','source_index','path','sha256')}
        if merged and row['wgc_timestamp_100ns']==merged[-1]['wgc_timestamp_100ns']:
            old=merged[-1]
            if any(row.get(k)!=old.get(k) for k in ('pid','hwnd',*GEOMETRY,'pixel_sha256')):
                raise AssertionError('S2相同WGC时间戳出现owner/几何/像素冲突')
            old['raw_sources'].append(source);continue
        row['index']=len(merged);row['raw_sources']=[source];merged.append(row)
    return merged


class FreshPlateauWatcher:
    """主会话始终保留；四角各至多一个短会话，同一观察预算内owned清理。"""
    def __init__(self,capture,record,match):
        self.capture,self.record,self.match=capture,record,match
        self.directory=capture.artifacts/'action-series';self.ready=json.loads((self.directory/'ready.json').read_text())
        self.stop=threading.Event();self.error=None;self.seen=[];self.corners={};self.next_corner=1
        self.primary_run_pose=None;self.primary_run=[]
        self.report={'maximum_seconds':12,'maximum_secondary_sessions':4,'sessions':[],
            'source':'real WGC sessions; no input/redraw/interpolation','primary_frames_discarded':False}
        record['fresh_wgc']=self.report

    def __enter__(self):
        self.started=time.monotonic();self.deadline=self.started+12;self.clock=qpc_stamp()
        if type(self.clock['qpc']) is not int or type(self.clock['qpc_frequency']) is not int or self.clock['qpc_frequency']<=0:
            raise AssertionError('S2 fresh WGC缺少实际QPC时钟')
        self.report['watcher_started']=self.clock
        self.thread=threading.Thread(target=self._run,name='s2-fresh-wgc');self.thread.start();return self

    def _run(self):
        try:
            while not self.stop.is_set():
                if time.monotonic()>=self.deadline:raise TimeoutError('S2 fresh WGC的12秒预算耗尽')
                self.poll()
                if all(c.get('finished') for c in self.corners.values()) and len(self.corners)==4:return
                self.stop.wait(0.005)
        except BaseException as error:
            self.error=error;self.report['error']={'type':type(error).__name__,'message':str(error)}

    def poll(self):
        if self.capture.process.poll() is not None:raise AssertionError('S2 fresh WGC目标提前退出')
        frames=live_frames(self.directory,self.ready,self.capture.process.pid,self.seen)
        if len(frames)<len(self.seen) or any(a!=b for a,b in zip(frames,self.seen)):
            raise AssertionError('S2 primary live原件回退或改变')
        for row in frames[len(self.seen):]:
            pose=self.match(row)
            if self.next_corner<=4 and pose==self.next_corner:
                self.corners[pose]={'pose':pose,'first_stamp':row['wgc_timestamp_100ns'],'first_row':row}
                self.next_corner+=1
            if pose is None:
                self.primary_run_pose=None;self.primary_run=[]
            elif pose!=self.primary_run_pose:
                self.primary_run_pose=pose;self.primary_run=[row]
            else:self.primary_run.append(row)
            corner=self.corners.get(pose)
            if corner is not None and not corner.get('finished') and len(self.primary_run)>=2:
                span=self.primary_run[-1]['wgc_timestamp_100ns']-self.primary_run[0]['wgc_timestamp_100ns']
                if span>=MINIMUM_SPAN_100NS[pose]:
                    corner.update(finished=True,covered_by_primary=True,primary_support={
                        'frame_count':len(self.primary_run),'observed_stable_span_ms':span/10000,
                        'frames':[{k:r[k] for k in ('index','path','sha256','raw_row_sha256','wgc_timestamp_100ns')}
                            for r in self.primary_run]})
            self.last_pose=pose
        self.seen=frames
        now=qpc_stamp();frequency=self.clock['qpc_frequency']
        if now['qpc_frequency']!=frequency:raise AssertionError('S2 fresh WGC的QPC频率改变')
        for pose,corner in self.corners.items():
            if corner.get('finished'):continue
            # 不等待或延长已离开的平台；未取得正证仍由原连续门拒绝。
            if self.last_pose!=pose:
                corner.update(finished=True,skip_reason='primary已离开该角；未延长平台');continue
            corner['finished']=True
            self._capture(corner)

    def _capture(self,corner):
        pose=corner['pose'];name=f'fresh-corner-{pose}'
        if pose not in MINIMUM_SPAN_100NS or len(self.report['sessions'])>=4 or any(s['pose']==pose for s in self.report['sessions']):
            raise AssertionError('S2 fresh只允许四角各一次短会话')
        directory=self.capture.artifacts/name;slot={'session':name,'pose':pose,'original_first_stamp':corner['first_stamp'],
            'minimum_observed_span_ms':MINIMUM_SPAN_100NS[pose]/10000}
        self.report['sessions'].append(slot)
        helper=OwnedHelper(self.capture.driver,[self.capture.driver,'capture-series',self.capture.process.pid,directory,1500,16],
            self.capture.artifacts,name+'-helper',slot)
        ready=None;observed=[]
        try:
            requested_at=time.monotonic();remaining=self.deadline-requested_at
            if remaining<=0:raise TimeoutError('S2 fresh WGC的12秒预算耗尽；未启动新会话')
            capture_deadline=min(self.deadline,requested_at+1.5)
            requested=qpc_stamp();slot['request_qpc']=requested
            if requested['qpc_frequency']!=self.clock['qpc_frequency']:raise AssertionError('S2 fresh请求QPC频率改变')
            child=helper.start();ready=wait_ready(directory/'ready.json',child,self.capture.process,timeout=min(1,remaining))
            if time.monotonic()>self.deadline:raise TimeoutError('S2 fresh首帧超过12秒截止；不接受过期结果')
            if any(ready.get(k)!=self.ready.get(k) for k in ('pid','hwnd','client_width','client_height')):
                raise AssertionError('S2 fresh会话ready owner/尺寸改变')
            observed=live_frames(directory,ready,self.capture.process.pid);first=observed[0]
            if first['wgc_timestamp_100ns']*requested['qpc_frequency']<requested['qpc']*10_000_000:
                raise AssertionError('S2 fresh首帧是请求之前的旧时间戳')
            if self.match(first)!=pose:raise AssertionError('S2 fresh启动已错过原角平台')
            while True:
                if time.monotonic()>capture_deadline:raise TimeoutError('S2 fresh等待门所需真实新帧超过1500ms总预算')
                accepted=next((row for row in observed if row['wgc_timestamp_100ns']>corner['first_stamp']
                    and row['wgc_timestamp_100ns']-corner['first_stamp']>=MINIMUM_SPAN_100NS[pose]
                    and row['wgc_timestamp_100ns']*requested['qpc_frequency']>=requested['qpc']*10_000_000
                    and self.match(row)==pose),None)
                if accepted is not None:
                    slot['accepted_frame']={k:accepted[k] for k in ('index','path','sha256','wgc_timestamp_100ns')}
                    slot['accepted_frame']['source_age_ms']=(accepted['wgc_timestamp_100ns']-corner['first_stamp'])/10000
                    break
                if child.poll() is not None or self.capture.process.poll() is not None:
                    raise AssertionError('S2 fresh在取得门所需真实新帧前退出')
                if self.stop.wait(0.005):raise AssertionError('S2 fresh在取得门所需真实新帧前结束观察')
                new=live_frames(directory,ready,self.capture.process.pid,observed)
                if len(new)<len(observed):raise AssertionError('S2 fresh live索引被截断')
                observed=new
        finally:
            if directory.exists():(directory/'stop').write_text('stop\n',encoding='utf-8')
            helper.finish()
            if ready is not None:
                slot.update(ready=ready,frames=live_frames(directory,ready,self.capture.process.pid,observed))
                summary=slot['summary']
                if (not (directory/'frames.jsonl').read_bytes().endswith(b'\n') or
                    len(slot['frames'])<len(observed) or
                    summary!=json.loads((directory/'summary.json').read_text()) or summary.get('ok') is not True or
                    summary.get('backend')!='WGC-HWND-session' or summary.get('stop_reason')!='stop' or
                    summary.get('pid')!=self.ready['pid'] or summary.get('hwnd')!=self.ready['hwnd'] or
                    summary.get('session_started') is not True or summary.get('ready_published') is not True or
                    summary.get('hresult')!=0 or summary.get('color_filter') is not False or
                    summary.get('qpc_frequency')!=self.clock['qpc_frequency'] or len(slot['frames'])!=summary.get('frames') or
                    not slot['frames'] or summary.get('first_wgc_timestamp_100ns')!=slot['frames'][0]['wgc_timestamp_100ns'] or
                    summary.get('last_wgc_timestamp_100ns')!=slot['frames'][-1]['wgc_timestamp_100ns']):
                    raise AssertionError('S2 fresh会话未按显式stop完整采集')
                geometry=corner['first_row']
                if any(row.get(k)!=geometry.get(k) for row in slot['frames'] for k in GEOMETRY):
                    raise AssertionError('S2 fresh会话与primary几何不同')
                slot['artifacts']={name:{'path':str(directory/name),'sha256':digest(directory/name)}
                    for name in ('ready.json','summary.json','frames.jsonl')}

    def __exit__(self,kind,error,traceback):
        self.stop.set();self.thread.join()
        self.report['corners']=[{k:v for k,v in c.items() if k!='first_row'} for c in self.corners.values()]
        self.report['elapsed_seconds']=time.monotonic()-self.started
        if error is None and self.error is not None:raise self.error

    def merge(self,primary):
        if self.error is not None:raise self.error
        if any(not c.get('finished') for c in self.corners.values()) or len(self.corners)!=4:
            raise AssertionError('S2 fresh观察未完成四角；不补造来源')
        if not (self.directory/'frames.jsonl').read_bytes().endswith(b'\n'):
            raise AssertionError('S2最终primary raw index没有完整末行')
        final=live_frames(self.directory,self.ready,self.capture.process.pid,self.seen)
        if len(final)<len(self.seen) or len(final)!=len(primary):raise AssertionError('S2最终primary raw行被截断或计数不符')
        if any(any(row[k]!=published.get(k) for k in row if k not in ('pixel_sha256','raw_row_sha256'))
            for row,published in zip(final,primary)):raise AssertionError('S2最终primary与已发布完整索引不同')
        if any(row.get(k)!=self.ready.get(k) for row in primary for k in ('pid','hwnd')):
            raise AssertionError('S2合并primary owner改变')
        if len(primary)<len(self.seen) or any(any(old[k]!=new[k] for k in ('index','path','sha256','wgc_timestamp_100ns'))
            for old,new in zip(self.seen,primary)):raise AssertionError('S2最终primary与实际live来源不同')
        merged=merge_frames(primary,self.report['sessions']);self.report['merged_frames']=merged
        self.report.update(primary_frame_count=len(primary),secondary_frame_count=sum(len(s['frames']) for s in self.report['sessions']),
            merged_frame_count=len(merged),duplicate_timestamp_count=sum(len(r['raw_sources'])-1 for r in merged))
        return merged
