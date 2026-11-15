"""Windows图形测试临时显示需求；授权主动点亮，主线程finally恢复原要求。

测试宿主设施，不属于FD2原版ABI。基于Microsoft SetThreadExecutionState及
已核的console/session显示通知。授权的独立zero MOVE仅用于重置显示空闲，
不投游戏键、不点击、不改焦点；显式临时电源副本由独立journal恢复。
"""
from contextlib import contextmanager, nullcontext
import ctypes
import os
import sys
import threading
import time
from types import SimpleNamespace
import uuid


ES_CONTINUOUS = 0x80000000
REQUIRED_FLAGS = ES_CONTINUOUS | 0x01 | 0x02
# 仅注销失败的致命错误路径：native可能仍借用callback，进程退出前不能释放。
# 该路径使ON先验失败、不进入测试body；不重试或另建通知服务。
_unreleased_notifications = []


def _error(error):
    return {"type": type(error).__name__, "message": str(error)}


def _display_snapshot():
    # 延迟绑定：模块导入、Linux与fake-API纯测试均不调用WinDLL/显示API。
    from ctypes import wintypes

    class GUID(ctypes.Structure):
        _fields_ = [("a", ctypes.c_uint32), ("b", ctypes.c_uint16),
                    ("c", ctypes.c_uint16), ("d", ctypes.c_ubyte * 8)]

    class SETTING(ctypes.Structure):
        _fields_ = [("guid", GUID), ("length", ctypes.c_uint32), ("data", ctypes.c_ubyte * 4)]

    callback_type = ctypes.WINFUNCTYPE(ctypes.c_uint32, ctypes.c_void_p,
                                      ctypes.c_uint32, ctypes.c_void_p)

    class SUBSCRIBE(ctypes.Structure):
        _fields_ = [("callback", callback_type), ("context", ctypes.c_void_p)]

    power = ctypes.WinDLL("powrprof.dll", use_last_error=True)
    power.PowerSettingRegisterNotification.argtypes = [ctypes.POINTER(GUID), ctypes.c_uint32,
        ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p)]
    power.PowerSettingRegisterNotification.restype = ctypes.c_uint32
    power.PowerSettingUnregisterNotification.argtypes = [ctypes.c_void_p]
    power.PowerSettingUnregisterNotification.restype = ctypes.c_uint32
    kernel = ctypes.WinDLL("kernel32.dll", use_last_error=True)
    kernel.ProcessIdToSessionId.argtypes = [ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)]
    kernel.ProcessIdToSessionId.restype = wintypes.BOOL
    kernel.WTSGetActiveConsoleSessionId.restype = ctypes.c_uint32
    user = ctypes.WinDLL("user32.dll", use_last_error=True)
    user.GetSystemMetrics.argtypes = [ctypes.c_int]
    user.GetSystemMetrics.restype = ctypes.c_int

    def query(text):
        expected = uuid.UUID(text)
        guid = GUID.from_buffer_copy(expected.bytes_le)
        events, ready, lock = [], threading.Event(), threading.Lock()

        def notify(context, kind, pointer):
            row = {"type": int(kind)}
            try:
                if kind != 0x8013 or not pointer:
                    raise ValueError("显示通知type/pointer无效")
                value = ctypes.cast(pointer, ctypes.POINTER(SETTING)).contents
                if bytes(value.guid) != expected.bytes_le or value.length != 4:
                    raise ValueError("显示通知GUID/length无效")
                state = int.from_bytes(bytes(value.data), "little")
                if state not in (0, 1, 2):
                    raise ValueError("未知显示状态")
                row["state"] = state
            except Exception as error:
                row["error"] = _error(error)
            with lock:
                events.append(row)
            ready.set()
            return 0

        callback = callback_type(notify)
        recipient = SUBSCRIBE(callback, None)
        handle = ctypes.c_void_p()
        result = {"guid": text, "timeout_ms": 5000}
        try:
            result["register_code"] = int(power.PowerSettingRegisterNotification(
                ctypes.byref(guid), 2, ctypes.byref(recipient), ctypes.byref(handle)))
            if result["register_code"] == 0:
                ready.wait(5)
        finally:
            try:
                result["unregister_code"] = (int(power.PowerSettingUnregisterNotification(handle))
                                              if handle.value else None)
            finally:
                if handle.value and result.get("unregister_code") != 0:
                    _unreleased_notifications.append((callback, recipient, handle, power))
        # 成功注销后才释放callback；失败引用保留至本进程退出，不冒称已解除。
        with lock:
            result["events"] = list(events)
        valid = [row["state"] for row in result["events"] if "state" in row]
        result["state"] = valid[-1] if valid else None
        result["ok"] = (result["register_code"] == result["unregister_code"] == 0 and bool(valid)
                        and not any("error" in row for row in result["events"]))
        return result

    session = ctypes.c_uint32()
    session_ok = bool(kernel.ProcessIdToSessionId(os.getpid(), ctypes.byref(session)))
    active = int(kernel.WTSGetActiveConsoleSessionId())
    return {"session_query_ok": session_ok, "process_session_id": session.value if session_ok else None,
        "active_console_session_id": active,
        "same_as_active_console": session_ok and active != 0xffffffff and session.value == active,
        "SM_REMOTESESSION": int(user.GetSystemMetrics(0x1000)),
        "console_display": query("6fe69556-704a-47a0-8f24-c28d936fda47"),
        "session_display": query("2b84c20e-ad23-4ddf-93db-05ffbd7efca5")}


def _set_execution_state(flags):
    kernel = ctypes.WinDLL("kernel32.dll", use_last_error=True)
    kernel.SetThreadExecutionState.argtypes = [ctypes.c_uint32]
    kernel.SetThreadExecutionState.restype = ctypes.c_uint32
    ctypes.set_last_error(0)
    result = int(kernel.SetThreadExecutionState(flags))
    return {"return_value": result, "last_error": int(ctypes.get_last_error())}


def _input_desktop():
    from ctypes import wintypes
    user = ctypes.WinDLL("user32.dll", use_last_error=True)
    user.OpenInputDesktop.argtypes = [ctypes.c_uint32, wintypes.BOOL, ctypes.c_uint32]
    user.OpenInputDesktop.restype = ctypes.c_void_p
    user.GetUserObjectInformationW.argtypes = [ctypes.c_void_p, ctypes.c_int,
        ctypes.c_void_p, ctypes.c_uint32, ctypes.POINTER(ctypes.c_uint32)]
    user.GetUserObjectInformationW.restype = wintypes.BOOL
    user.CloseDesktop.argtypes = [ctypes.c_void_p]
    user.CloseDesktop.restype = wintypes.BOOL
    handle = user.OpenInputDesktop(0, False, 1)  # DESKTOP_READOBJECTS，只读查询。
    if not handle:
        return {"ok": False, "open_error": ctypes.get_last_error()}
    result = {"ok": False}
    try:
        name, needed = ctypes.create_unicode_buffer(256), ctypes.c_uint32()
        valid = bool(user.GetUserObjectInformationW(handle, 2, name,
            ctypes.sizeof(name), ctypes.byref(needed)))
        result.update(query_ok=valid, name=name.value if valid else None,
                      query_error=ctypes.get_last_error() if not valid else 0)
    finally:
        result["closed"] = bool(user.CloseDesktop(handle))
    result["ok"] = result.get("query_ok") is True and result["closed"] and result["name"] == "Default"
    return result


def _reset_display_idle():
    """仅发送一次相对零位移MOVE；记录真实指针及LastInputInfo，不做补发。"""
    from ctypes import wintypes
    class MOUSEINPUT(ctypes.Structure):
        _fields_ = [("dx", wintypes.LONG), ("dy", wintypes.LONG),
            ("data", wintypes.DWORD), ("flags", wintypes.DWORD),
            ("time", wintypes.DWORD), ("extra", ctypes.c_size_t)]
    class KEYBDINPUT(ctypes.Structure):
        _fields_ = [("vk", wintypes.WORD), ("scan", wintypes.WORD),
            ("flags", wintypes.DWORD), ("time", wintypes.DWORD), ("extra", ctypes.c_size_t)]
    class HARDWAREINPUT(ctypes.Structure):
        _fields_ = [("msg", wintypes.DWORD), ("low", wintypes.WORD), ("high", wintypes.WORD)]
    class PAYLOAD(ctypes.Union):
        _fields_ = [("mouse", MOUSEINPUT), ("keyboard", KEYBDINPUT), ("hardware", HARDWAREINPUT)]
    class INPUT(ctypes.Structure):
        _fields_ = [("type", wintypes.DWORD), ("payload", PAYLOAD)]
    class LASTINPUTINFO(ctypes.Structure):
        _fields_ = [("cbSize", wintypes.UINT), ("dwTime", wintypes.DWORD)]
    user = ctypes.WinDLL("user32.dll", use_last_error=True)
    user.GetCursorPos.argtypes = [ctypes.POINTER(wintypes.POINT)]
    user.GetCursorPos.restype = wintypes.BOOL
    user.GetLastInputInfo.argtypes = [ctypes.POINTER(LASTINPUTINFO)]
    user.GetLastInputInfo.restype = wintypes.BOOL
    user.SendInput.argtypes = [wintypes.UINT, ctypes.POINTER(INPUT), ctypes.c_int]
    user.SendInput.restype = wintypes.UINT
    def sample():
        point, last = wintypes.POINT(), LASTINPUTINFO()
        last.cbSize = ctypes.sizeof(last)
        cursor_ok = bool(user.GetCursorPos(ctypes.byref(point)))
        idle_ok = bool(user.GetLastInputInfo(ctypes.byref(last)))
        return {"ok": cursor_ok and idle_ok, "cursor_ok": cursor_ok, "idle_ok": idle_ok,
            "point": [int(point.x), int(point.y)] if cursor_ok else None,
            "last_input_tick": int(last.dwTime) if idle_ok else None}
    result = {"kind": "display-idle-reset-only", "before": sample(),
              "dx": 0, "dy": 0, "flags": 1, "count": 1, "inserted": 0}
    if not result["before"]["ok"]:
        result["ok"] = False
        return result
    event = INPUT()
    event.payload.mouse.flags = 1  # MOUSEEVENTF_MOVE，无键、按钮或绝对坐标。
    result["inserted"] = int(user.SendInput(1, ctypes.byref(event), ctypes.sizeof(event)))
    result["after"] = sample()
    result["pointer_unchanged"] = result["before"]["point"] == result["after"]["point"]
    result["idle_timestamp_changed"] = result["before"]["last_input_tick"] != result["after"]["last_input_tick"]
    result["ok"] = result["inserted"] == 1 and result["after"]["ok"] and result["pointer_unchanged"]
    return result


def _windows_calls():
    return SimpleNamespace(snapshot=_display_snapshot, set_state=_set_execution_state,
        desktop=_input_desktop, reset_idle=_reset_display_idle,
        wait=time.sleep, thread_id=threading.get_native_id)


def _on(snapshot):
    return (snapshot.get("session_query_ok") is True and
        snapshot.get("same_as_active_console") is True and snapshot.get("SM_REMOTESESSION") == 0 and
        all(snapshot.get(key, {}).get("ok") is True and snapshot[key].get("state") == 1
            for key in ("console_display", "session_display")))


@contextmanager
def windows_display_requirement(record, *, power_journal=None, require_display_on=False):
    """一次入口一次申请；异常仍先完成body的owned清理，再同线程撤销。

    record由入口最终写入报告。硬终止不保证Python finally，不伪造released。
    """
    record.update(platform=sys.platform, requested=False, acquired=False, released=False,
        status="not_applicable", powerplan_changed=False, separate_wake_API_called=False,
        acquire_can_force_display_on=True, conditional_acquire_not_atomic=True,
        require_display_on=require_display_on)
    if require_display_on and power_journal is not None:
        raise ValueError('ON先验模式不允许电源方案或恢复journal')
    if sys.platform != "win32":
        yield record
        return
    api = _windows_calls()
    owner = api.thread_id()
    record.update(status="checking", pid=os.getpid(), owner_thread_id=owner)
    previous = None
    try:
        if power_journal is not None:
            from fd2_display_power import recover_video_idle
            recover_video_idle(record.setdefault("power", {}), power_journal)
        before = api.snapshot()
        record["before"] = before
        interactive = (before.get("session_query_ok") is True and
            before.get("same_as_active_console") is True and before.get("SM_REMOTESESSION") == 0)
        notifications = all(before.get(key, {}).get("ok") is True
                            for key in ("console_display", "session_display"))
        if not interactive or not notifications:
            record["status"] = "blocked_environment"
            raise RuntimeError("非活动console或显示通知无效；未启动窗口")
        if require_display_on and not _on(before):
            record['status']='blocked_environment'
            raise RuntimeError('显示未确认ON；ON先验模式未申请需求或唤醒，未启动窗口')
        record["input_desktop"] = api.desktop()
        if record["input_desktop"].get("ok") is not True:
            record["status"] = "blocked_environment"
            raise RuntimeError("活动输入桌面不可用或未解锁；未启动窗口")
        record["wake_requested"] = not _on(before)
        record["requested"] = True
        record["acquire"] = {"flags": REQUIRED_FLAGS, "thread_id": owner,
                             "time_ns": time.monotonic_ns()}
        acquired = api.set_state(REQUIRED_FLAGS)
        record["acquire"].update(acquired)
        if not acquired["return_value"]:
            raise RuntimeError("SetThreadExecutionState申请失败")
        previous = acquired["return_value"]
        record.update(acquired=True, status="held", previous_state=previous)
    except BaseException as error:
        if record["status"] != "blocked_environment":
            record["status"] = "failed"
        record["acquire_error"] = _error(error)
        raise
    body_error = None
    try:
        if record["wake_requested"]:
            # 有界的一次申请后核验；不循环唤醒，不向游戏或前台投键。
            record["after_wake"] = api.snapshot()
            if not _on(record["after_wake"]):
                # 先复核桌面再独立重置显示空闲；绝不作为游戏输入回退。
                record["desktop_before_idle_reset"] = api.desktop()
                if record["desktop_before_idle_reset"].get("ok") is not True:
                    raise RuntimeError("活动输入桌面不可用；未发送显示唤醒输入")
                record["separate_wake_API_called"] = True
                record["idle_reset"] = api.reset_idle()
                if record["idle_reset"].get("ok") is not True:
                    raise RuntimeError("零位移显示唤醒失败或指针变化；未补发，未启动窗口")
                record["idle_reset_wait_seconds"] = 1.0
                api.wait(1.0)
                record["after_idle_reset"] = api.snapshot()
                if not _on(record["after_idle_reset"]):
                    raise RuntimeError("一次零位移唤醒后console/session仍非ON；已停止并恢复显示需求")
            record["desktop_after_wake"] = api.desktop()
            if record["desktop_after_wake"].get("ok") is not True:
                raise RuntimeError("主动点亮后活动输入桌面不可用；未启动窗口")
        power_scope = nullcontext()
        if power_journal is not None:
            from fd2_display_power import temporary_video_idle
            record["before_power"] = api.snapshot()
            record["desktop_before_power"] = api.desktop()
            if not _on(record["before_power"]) or record["desktop_before_power"].get("ok") is not True:
                raise RuntimeError("临时电源副本前显示或Default桌面未确认；未修改方案")
            power_scope = temporary_video_idle(record["power"], power_journal)
        with power_scope:
            yield record
    except BaseException as error:
        body_error = error
        record["body_error"] = _error(error)
        raise
    finally:
        record["powerplan_changed"] = record.get("power", {}).get("changed", False)
        release_error = None
        try:
            current = api.thread_id()
            record["release"] = {"flags": previous | ES_CONTINUOUS, "thread_id": current,
                                 "time_ns": time.monotonic_ns()}
            if current != owner:
                raise RuntimeError("显示需求必须在取得它的同一线程撤销")
            released = api.set_state(previous | ES_CONTINUOUS)
            record["release"].update(released)
            if not released["return_value"]:
                raise RuntimeError("SetThreadExecutionState撤销失败")
            record["released"] = True
        except BaseException as error:
            release_error = error
            record["release_error"] = _error(error)
        record["status"] = "released" if body_error is None and release_error is None else "failed"
        if release_error is not None and body_error is None:
            raise release_error
