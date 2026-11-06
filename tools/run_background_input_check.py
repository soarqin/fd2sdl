#!/usr/bin/env python3
"""Windows后台HWND投键与WGC校准；必须由本轮owned Windows Job启动。

接口：--root ROOT --game EXE --driver EXE --dosbox EXE --artifacts NEWDIR。
例如：driver.exe run-job 600000 python -B tools/run_background_input_check.py
      --root . --game src/Debug/fd2sdl.exe --driver driver.exe
      --dosbox dosbox-x.exe --artifacts <新的隔离目录>。

原版／SDL串行使用既有标题和菜单资源门；同文件--sentinel子模式仅供内部
创建自有无激活哨兵，复用它做遮挡，最多同时一个哨兵和一个游戏窗口。
成功／失败都清理本轮PID；哨兵零按键及10ms焦点抽样不证明用户前台应用
的键盘收据，也不保证观察到短于10ms的转焦。WAV只证明后台热键识别，
不证明曲目身份或A7保真。C3实机来源保留；永久版另有仅纯测试验证的
HWND／PID所有权校验，不能沿用迁移前的完整AST等价结论。
"""
from __future__ import annotations
import argparse, ctypes, hashlib, json, os, queue, shutil, subprocess, sys, threading, time, wave
from contextlib import contextmanager
from pathlib import Path
from types import SimpleNamespace


def sha(path): return hashlib.sha256(Path(path).read_bytes()).hexdigest()
def evidence(root):
    return {str(p.relative_to(root)):{'sha256':sha(p),'mtime_ns':p.stat().st_mtime_ns}
            for p in sorted(Path(root).rglob('*')) if p.is_file()}
def write(path,data): Path(path).write_text(json.dumps(data,ensure_ascii=False,indent=2)+'\n',encoding='utf-8',newline='\n')

def winapi():
    from ctypes import wintypes as w
    u=ctypes.WinDLL('user32',use_last_error=True)
    u.GetForegroundWindow.restype=w.HWND
    u.GetWindowThreadProcessId.argtypes=[w.HWND,ctypes.POINTER(w.DWORD)]
    u.PostMessageW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM]
    u.IsWindow.argtypes=[w.HWND];u.IsWindow.restype=w.BOOL
    u.IsWindowVisible.argtypes=[w.HWND];u.IsWindowVisible.restype=w.BOOL
    u.GetWindowRect.argtypes=[w.HWND,ctypes.POINTER(w.RECT)]
    u.SetWindowPos.argtypes=[w.HWND,w.HWND,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_int,w.UINT]
    return u,w

def foreground(u,w):
    hwnd=u.GetForegroundWindow();pid=w.DWORD()
    if hwnd:u.GetWindowThreadProcessId(hwnd,ctypes.byref(pid))
    return {'hwnd':int(hwnd or 0),'pid':pid.value}

def assert_background(state,target_pid):
    if state['pid']==target_pid: raise AssertionError('Owned game became foreground; background acceptance rejected')

def require_owned_window(u,w,hwnd,pid):
    """每次操作前核对实际窗口所有者，不能信任子进程输出或复用的句柄。"""
    if not isinstance(hwnd,int) or isinstance(hwnd,bool) or hwnd<=0 or not u.IsWindow(hwnd):
        raise AssertionError('Sentinel closed or invalid HWND; no window operation allowed')
    actual=w.DWORD()
    thread=u.GetWindowThreadProcessId(hwnd,ctypes.byref(actual))
    if not thread or actual.value!=pid:
        raise AssertionError('Sentinel HWND is not owned by PID '+str(pid)+'; actual PID '+str(actual.value))

def sentinel(path):
    u,w=winapi();k=ctypes.WinDLL('kernel32',use_last_error=True)
    k.GetModuleHandleW.argtypes=[w.LPCWSTR];k.GetModuleHandleW.restype=w.HINSTANCE
    proc_type=ctypes.WINFUNCTYPE(ctypes.c_ssize_t,w.HWND,w.UINT,w.WPARAM,w.LPARAM)
    class WNDCLASS(ctypes.Structure):
        _fields_=[('style',w.UINT),('proc',proc_type),('class_extra',ctypes.c_int),('window_extra',ctypes.c_int),('instance',w.HINSTANCE),('icon',w.HICON),('cursor',w.HANDLE),('background',w.HBRUSH),('menu',w.LPCWSTR),('name',w.LPCWSTR)]
    u.DefWindowProcW.argtypes=[w.HWND,w.UINT,w.WPARAM,w.LPARAM];u.DefWindowProcW.restype=ctypes.c_ssize_t
    u.RegisterClassW.argtypes=[ctypes.POINTER(WNDCLASS)];u.RegisterClassW.restype=w.WORD
    u.CreateWindowExW.argtypes=[w.DWORD,w.LPCWSTR,w.LPCWSTR,w.DWORD,ctypes.c_int,ctypes.c_int,ctypes.c_int,ctypes.c_int,w.HWND,w.HMENU,w.HINSTANCE,w.LPVOID];u.CreateWindowExW.restype=w.HWND
    u.ShowWindow.argtypes=[w.HWND,ctypes.c_int];u.UpdateWindow.argtypes=[w.HWND];u.DestroyWindow.argtypes=[w.HWND]
    u.GetMessageW.argtypes=[ctypes.POINTER(w.MSG),w.HWND,w.UINT,w.UINT];u.GetMessageW.restype=ctypes.c_int
    u.TranslateMessage.argtypes=[ctypes.POINTER(w.MSG)];u.DispatchMessageW.argtypes=[ctypes.POINTER(w.MSG)]
    keys=[];instance=k.GetModuleHandleW(None)
    @proc_type
    def procedure(hwnd,msg,wp,lp):
        if msg in (0x100,0x101,0x104,0x105):
            keys.append({'time':time.monotonic(),'message':msg,'vk':int(wp),'lparam':int(lp)})
            with Path(path).with_suffix('.keys.jsonl').open('a',encoding='utf-8') as f:f.write(json.dumps(keys[-1])+'\n')
            return 0
        if msg==0x10:u.DestroyWindow(hwnd);return 0
        if msg==2:u.PostQuitMessage(0);return 0
        return u.DefWindowProcW(hwnd,msg,wp,lp)
    c=WNDCLASS(0,procedure,0,0,instance,None,None,ctypes.cast(ctypes.c_void_p(6),w.HBRUSH),None,'FD2BackgroundSentinel')
    if not u.RegisterClassW(ctypes.byref(c)):raise ctypes.WinError(ctypes.get_last_error())
    hwnd=u.CreateWindowExW(0x08000080,c.name,'FD2 background input sentinel - no test keys should arrive',0x00cf0000,30,30,360,210,None,None,instance,None)
    if not hwnd:raise ctypes.WinError(ctypes.get_last_error())
    u.ShowWindow(hwnd,4);u.UpdateWindow(hwnd)
    initial={'pid':os.getpid(),'hwnd':int(hwnd),'foreground':foreground(u,w),'keys':[]}
    write(path,initial);print(json.dumps(initial),flush=True)
    msg=w.MSG()
    while True:
        r=u.GetMessageW(ctypes.byref(msg),None,0,0)
        if r==-1:raise ctypes.WinError(ctypes.get_last_error())
        if r==0:break
        u.TranslateMessage(ctypes.byref(msg));u.DispatchMessageW(ctypes.byref(msg))
    write(path,{**initial,'keys':keys,'closed':True});return 0

def fixture_from_start(root,dest):
    from run_graphics_tests import decode_save
    source=root/'original_game/START.SAV';before=decode_save(source);plain=bytearray(before)
    # 只派生焦点四字节，保留真实START镜头(5,11)。
    if plain[0x30c6:0x30ca]!=bytes((5,11,8,15)):raise AssertionError('Unexpected START viewport/focus')
    plain[0x30c8:0x30cc]=bytes((6,14,1,3));plain[-4:]=sum(plain[:-4]).to_bytes(4,'little')
    encoded=bytearray(plain);state=0xa5
    for i in range(len(encoded)):
        state=(state+0x9014)&0xffff;state=((state<<3)|(state>>13))&0xffff;encoded[i]^=state&255
    dest.write_bytes(encoded);assert decode_save(dest)==plain
    diffs=[{'offset':i,'before':a,'after':b} for i,(a,b) in enumerate(zip(before,plain)) if a!=b]
    assert all(d['offset'] in (0x30c8,0x30c9,0x30ca,0x30cb) or d['offset']>=len(plain)-4 for d in diffs)
    return {'source':str(source),'source_sha256':sha(source),'path':str(dest),'sha256':sha(dest),'plain_before_sha256':hashlib.sha256(before).hexdigest(),'plain_after_sha256':hashlib.sha256(plain).hexdigest(),'focus':[6,14],'camera':[5,11],'relative':[1,3],'stored_relative_fields_verified':list(plain[0x30ca:0x30cc])==[1,3],'differences':diffs,'controlled_not_natural':True}

class Audit:
    def __init__(self,sentinel_pid,sentinel_hwnd,path):
        self.u,self.w=winapi();self.pid=0;self.sentinel_pid=sentinel_pid;self.sentinel_hwnd=sentinel_hwnd
        self.events=[];self.violations=[];self.stop=threading.Event();self.path=path
        self.thread=threading.Thread(target=self.monitor,daemon=True);self.thread.start()
    def point(self,kind,**extra):
        row={'time':time.monotonic(),'kind':kind,'target_pid':self.pid,**foreground(self.u,self.w),**extra}
        self.events.append(row)
        if self.pid and row['pid']==self.pid:self.violations.append(row)
        return row
    def check(self,kind):
        row=self.point(kind)
        if self.violations:raise AssertionError('Game activated itself or input/capture activated game; see foreground trace')
        assert_background(row,self.pid)
        require_owned_window(self.u,self.w,self.sentinel_hwnd,self.sentinel_pid)
        return row
    def monitor(self):
        last=None
        while not self.stop.wait(.01):
            state=foreground(self.u,self.w)
            value=(state['hwnd'],state['pid'],self.pid)
            if value!=last:self.point('foreground_change');last=value
    def finish(self):
        self.stop.set();self.thread.join(2);write(self.path,{'events':self.events,'violations':self.violations,'poll_interval_seconds':.01,'scope':'sampled changes plus every Capture.key/frame before and after; not a claim to observe sub-10ms transients'})


def check_output(root,out):
    original=(root/'original_game').resolve()
    if out==original or original in out.parents:raise ValueError('Artifacts cannot write into original_game')

def run(args):
    root=args.root.resolve();out=args.artifacts.resolve();check_output(root,out);out.mkdir(parents=True,exist_ok=False)
    sys.path[:0]=[str(root/'tools'),str(root/'src/tests')]
    import run_original_comparison as title
    import run_original_battle_menu as battle
    before=evidence(root/'original_game');source_before={name:evidence(root/name) for name in ('tools','src/tests')};binary={k:{'path':str(v.resolve()),'sha256':sha(v)} for k,v in (('game',args.game),('driver',args.driver),('dosbox',args.dosbox))}
    report={'ok':False,'scope':'background HWND directed keys, exact existing title5/menu-cancel WGC predicates; inactive owned sentinel zero keys, no claim about user foreground application key receipts; not natural-stage0 or A7 audio fidelity','processes':[],'original_before':before,'binaries':binary,'scenarios':[],'native_audio':None}
    sent=None;audit=None;sentinel_hwnd=None;sent_state=out/'sentinel.json';audio_started=False
    raw_capture=title.Capture;raw_running=title.running
    try:
        command=[sys.executable,'-B',str(Path(__file__).resolve()),'--sentinel',str(sent_state)]
        sent=subprocess.Popen(command,stdout=subprocess.PIPE,stderr=(out/'sentinel.stderr.log').open('wb'),text=True,encoding='utf-8',creationflags=subprocess.CREATE_NO_WINDOW)
        q=queue.Queue();threading.Thread(target=lambda:q.put(sent.stdout.readline()),daemon=True).start()
        row=json.loads(q.get(timeout=10));report['sentinel_started']=row
        if row['pid']!=sent.pid or not row['hwnd']:raise AssertionError('Owned inactive sentinel identity missing; no game started')
        sentinel_hwnd=row['hwnd'];u,w=winapi()
        require_owned_window(u,w,sentinel_hwnd,sent.pid)
        report['sentinel_scope']='Owned inactive sentinel key receipt only; user foreground application input is not inspected'
        audit=Audit(sent.pid,row['hwnd'],out/'foreground.json')
        templates=title.title_templates(root/'original_game/FDOTHER.DAT')
        class BackgroundCapture(raw_capture):
            def key(self,name):
                audit.check('key_before:'+name)
                try:result=super().key(name)
                except BaseException:
                    audit.point('key_after_error:'+name);raise
                audit.check('key_after:'+name);return result
            def frame(self):
                nonlocal audio_started
                audit.check('capture_before')
                frame=super().frame();audit.check('capture_after')
                # 完整标题资源通过后，才发送原版专用录音组合键。
                if not audio_started and self.process.args[0]==str(args.dosbox.resolve()) and any(title.compare(frames[0],frame)['ok'] for frames in templates.values()):
                    audio_started=True
                    self.key('HOST_W');start=time.monotonic();time.sleep(.55);self.key('HOST_W')
                    report['native_audio']={'start_monotonic':start,'stop_monotonic':time.monotonic(),'keys':['HOST_W','HOST_W'],'scope':'native recording hotkey recognition only; no soundtrack identity/fidelity claim'}
                return frame
        @contextmanager
        def background_running(command,cwd,env,log,driver,receipts,**kwargs):
            if str(command[0])==str(args.dosbox.resolve()):
                conf=Path(command[command.index('-conf')+1]);captures=Path(cwd)/'native-audio';captures.mkdir()
                text=conf.read_text(encoding='utf-8').replace('[dosbox]\n','[dosbox]\ncaptures='+captures.as_posix()+'\nshow recorded filename=false\n').replace('[render]\n','[mixer]\nrate=44100\n[render]\n')
                conf.write_text(text,encoding='utf-8',newline='\n')
            audit.point('launch_before',command=command)
            with raw_running(command,cwd,env,log,driver,receipts,**kwargs) as process:
                audit.pid=process.pid;audit.check('launch_returned')
                try:yield process
                finally:audit.point('endpoint_body_finished')
            audit.check('endpoint_cleaned');audit.pid=0
            if evidence(root/'original_game')!=before:raise AssertionError('Original bytes/mtime changed after endpoint; next endpoint blocked')
            if any(evidence(root/name)!=rows for name,rows in source_before.items()):raise AssertionError('Frozen source changed after endpoint; next endpoint blocked')
            if any(sha(row['path'])!=row['sha256'] for row in binary.values()):raise AssertionError('Binary changed after endpoint; next endpoint blocked')
        def occlusion(capture,output):
            u,w=audit.u,audit.w;target=int(capture.trace[-1]['delivery']['hwnd'],16)
            rect=w.RECT();old=w.RECT()
            require_owned_window(u,w,audit.sentinel_hwnd,sent.pid)
            if not u.GetWindowRect(target,ctypes.byref(rect)) or not u.GetWindowRect(audit.sentinel_hwnd,ctypes.byref(old)):raise ctypes.WinError(ctypes.get_last_error())
            baseline=capture.frame().copy()
            # 复用唯一自有哨兵作遮挡，不创建第三个窗口。
            require_owned_window(u,w,audit.sentinel_hwnd,sent.pid)
            if not u.SetWindowPos(audit.sentinel_hwnd,w.HWND(-1),rect.left+60,rect.top+60,240,180,0x10):raise ctypes.WinError(ctypes.get_last_error())
            try:
                time.sleep(.25);actual=capture.frame().copy();actual.save(output/'sdl-occluded.png');found=title.compare(baseline,actual)
                if not found['ok']:raise AssertionError('HWND WGC changed under owned sentinel occlusion')
                return {**found,'overlay_pid':sent.pid,'overlay_hwnd':audit.sentinel_hwnd,'reused_sentinel':True,'overlay_size':[240,180]}
            finally:
                already_failed=sys.exc_info()[0] is not None
                try:
                    require_owned_window(u,w,audit.sentinel_hwnd,sent.pid)
                    if not u.SetWindowPos(audit.sentinel_hwnd,w.HWND(-2),old.left,old.top,old.right-old.left,old.bottom-old.top,0x10):raise ctypes.WinError(ctypes.get_last_error())
                except BaseException as e:
                    report.setdefault('sentinel_window_errors',[]).append(str(e))
                    if not already_failed:raise
        title.Capture=battle.Capture=BackgroundCapture;title.running=battle.running=background_running;title.verify_occlusion=occlusion
        fixture=out/'focus-6-14.SAV';report['fixture']=fixture_from_start(root,fixture)
        battle.validate_fixture(fixture,title.archive_entry((root/'original_game/FDFIELD.DAT').read_bytes(),1)[0])
        for name,module in (('title',title),('battle-menu',battle)):
            case=SimpleNamespace(root=root,game=args.game,driver=args.driver,dosbox=args.dosbox,artifacts=out/name,save_fixture=fixture)
            result=module.run(case);report['scenarios'].append({'name':name,'ok':result['ok'],'report':str(out/name/'comparison.json'),'sha256':sha(out/name/'comparison.json')})
            report['processes']+=result['processes']
            if not result['ok']:raise AssertionError(name+': '+result.get('error','failed strict resource guard'))
            if evidence(root/'original_game')!=before:raise AssertionError('Original bytes/mtime changed between scenarios')
            if any(sha(row['path'])!=row['sha256'] for row in binary.values()):raise AssertionError('Binary changed between scenarios')
        waves=list((out/'title/native-audio').glob('*.wav'))+list((out/'title/native-audio').glob('*.WAV'))
        waves=list(dict.fromkeys(waves))
        if len(waves)!=1 or report['native_audio'] is None:raise AssertionError('Expected one real native WAV from background HOST_W pair')
        with wave.open(str(waves[0]),'rb') as f:details={'channels':f.getnchannels(),'sample_rate':f.getframerate(),'sample_width':f.getsampwidth(),'frames':f.getnframes()}
        if details['sample_rate']!=44100 or details['sample_width']!=2 or details['frames']<=0:raise AssertionError('Native WAV header/payload invalid')
        report['native_audio'].update(path=str(waves[0]),sha256=sha(waves[0]),size=waves[0].stat().st_size,**details)
        report['ok']=True
    except BaseException as e:report['error']=str(e)
    finally:
        if audit:audit.finish();report['foreground_violations']=audit.violations
        if sent is not None:
            if sent.poll() is None:
                u,w=winapi()
                try:
                    require_owned_window(u,w,sentinel_hwnd,sent.pid)
                    if not u.PostMessageW(sentinel_hwnd,0x10,0,0):raise ctypes.WinError(ctypes.get_last_error())
                except BaseException as e:
                    report['ok']=False;report.setdefault('error',str(e))
                    report.setdefault('sentinel_window_errors',[]).append(str(e))
                try:sent.wait(timeout=5)
                except subprocess.TimeoutExpired:sent.terminate();sent.wait(timeout=5);report['sentinel_forced_cleanup']=True
            report['sentinel_exit_code']=sent.returncode
            if sent_state.exists():report['sentinel']=json.loads(sent_state.read_text(encoding='utf-8'))
        report['original_after']=evidence(root/'original_game');report['original_unchanged']=report['original_after']==before
        report['binary_unchanged']=all(sha(row['path'])==row['sha256'] for row in binary.values())
        report['source_unchanged']=all(evidence(root/name)==rows for name,rows in source_before.items())
        report['ok']=report['ok'] and report['original_unchanged'] and report['binary_unchanged'] and report['source_unchanged'] and not report.get('foreground_violations') and not report.get('sentinel_window_errors') and not report.get('sentinel_forced_cleanup') and report.get('sentinel_exit_code')==0 and report.get('sentinel',{}).get('closed') is True and report['sentinel'].get('pid')==getattr(sent,'pid',None) and report['sentinel'].get('hwnd')==sentinel_hwnd and report['sentinel']['keys']==[]
        if not report['ok'] and 'error' not in report:report['error']='Final sentinel/foreground/immutable guard failed'
        write(out/'comparison.json',report)
    return report

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--sentinel',type=Path)
    for name in ('root','game','driver','dosbox','artifacts'):p.add_argument('--'+name,type=Path)
    a=p.parse_args()
    if a.sentinel:return sentinel(a.sentinel)
    if sys.platform!='win32' or any(getattr(a,k) is None for k in ('root','game','driver','dosbox','artifacts')):p.error('Windows and root/game/driver/dosbox/artifacts are required')
    result=run(a);print(json.dumps({'ok':result['ok'],'report':str(a.artifacts/'comparison.json'),'error':result.get('error')},ensure_ascii=False));return 0 if result['ok'] else 1
if __name__=='__main__':sys.exit(main())
