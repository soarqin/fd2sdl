"""Windows图形测试临时显示需求；仅已ON时取得，主线程finally恢复原要求。

测试宿主设施，不属于FD2原版ABI。基于Microsoft SetThreadExecutionState及
已核的console/session显示通知；不改变powerplan、输入、焦点或截图判据。
DISPLAY_REQUIRED可force ON，故先验必须明确ON；查询与申请并非原子操作。
"""
from contextlib import contextmanager
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


def _windows_calls():
    return SimpleNamespace(snapshot=_display_snapshot, set_state=_set_execution_state,
                           thread_id=threading.get_native_id)


@contextmanager
def windows_display_requirement(record):
    """一次入口一次申请；异常仍先完成body的owned清理，再同线程撤销。

    record由入口最终写入报告。硬终止不保证Python finally，不伪造released。
    """
    record.update(platform=sys.platform, requested=False, acquired=False, released=False,
        status="not_applicable", powerplan_changed=False, separate_wake_API_called=False,
        acquire_can_force_display_on=True, conditional_acquire_not_atomic=True)
    if sys.platform != "win32":
        yield record
        return
    api = _windows_calls()
    owner = api.thread_id()
    record.update(status="checking", pid=os.getpid(), owner_thread_id=owner)
    try:
        before = api.snapshot()
        record["before"] = before
        on = (before.get("session_query_ok") is True and before.get("same_as_active_console") is True
              and before.get("SM_REMOTESESSION") == 0 and
              all(before.get(key, {}).get("ok") is True and before[key].get("state") == 1
                  for key in ("console_display", "session_display")))
        if not on:
            record["status"] = "blocked_not_on"
            raise RuntimeError("console/session显示非ON或未知；未申请显示需求、未启动窗口")
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
        if record["status"] != "blocked_not_on":
            record["status"] = "failed"
        record["acquire_error"] = _error(error)
        raise
    body_error = None
    try:
        yield record
    except BaseException as error:
        body_error = error
        record["body_error"] = _error(error)
        raise
    finally:
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
