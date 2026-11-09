"""显式选择的临时 VIDEOIDLE 副本；原计划只读，恢复 journal 在每次修改前落盘。

仅用于已授权 Windows 图形测试；调用者先验证活动 Default 桌面和显示 ON。
Power API 声明及 GUID 按 Windows SDK 10.0.26100.0 powrprof/powersetting/winnt。
复制用系统 powercfg 的指定目标 GUID，避免崩溃后找不到随机分配的副本。
持久化后再次核对活动方案；PowerSetActiveScheme没有CAS，最后核对到设置
之间仍有OS竞态，不能将此保护称为原子操作或保证用户永不同时切换。
"""
from contextlib import contextmanager
import ctypes
import json
import os
from pathlib import Path
import subprocess
import uuid

VIDEO = "7516b95f-f776-4464-8c53-06167f40cc99"
VIDEOIDLE = "3c0bc021-c8a8-4e07-a973-6b14cbcb2b7e"
SCHEMA = "fd2-temporary-video-idle-v1"


def _write(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".next")
    with temporary.open("w", encoding="utf-8") as stream:
        json.dump(value, stream, ensure_ascii=False, indent=2)
        stream.write("\n"); stream.flush(); os.fsync(stream.fileno())
    os.replace(temporary, path)


@contextmanager
def _exclusive(path):
    import msvcrt
    path = Path(path).with_suffix(Path(path).suffix + ".lock")
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("a+b") as stream:
        if stream.tell() == 0:
            stream.write(b"0"); stream.flush()
        stream.seek(0)
        msvcrt.locking(stream.fileno(), msvcrt.LK_NBLCK, 1)
        try:
            yield
        finally:
            stream.seek(0)
            msvcrt.locking(stream.fileno(), msvcrt.LK_UNLCK, 1)


class NativePower:
    def __init__(self, operations):
        from ctypes import wintypes
        self.operations = operations
        self.power = ctypes.WinDLL("powrprof.dll", use_last_error=True)
        self.kernel = ctypes.WinDLL("kernel32.dll", use_last_error=True)
        self.kernel.LocalFree.argtypes = [ctypes.c_void_p]
        self.kernel.LocalFree.restype = ctypes.c_void_p
        self.kernel.GetSystemPowerStatus.argtypes = [ctypes.c_void_p]
        self.kernel.GetSystemPowerStatus.restype = wintypes.BOOL
        self.kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
        self.kernel.OpenProcess.restype = wintypes.HANDLE
        self.kernel.GetProcessTimes.argtypes = [wintypes.HANDLE] + [ctypes.c_void_p] * 4
        self.kernel.GetProcessTimes.restype = wintypes.BOOL
        self.kernel.GetExitCodeProcess.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD)]
        self.kernel.GetExitCodeProcess.restype = wintypes.BOOL
        self.kernel.CloseHandle.argtypes = [wintypes.HANDLE]
        self.kernel.CloseHandle.restype = wintypes.BOOL
        for name, count in (("PowerGetActiveScheme", 2), ("PowerSetActiveScheme", 2),
            ("PowerDeleteScheme", 2), ("PowerReadACValueIndex", 5),
            ("PowerReadDCValueIndex", 5)):
            method = getattr(self.power, name)
            method.argtypes = [ctypes.c_void_p] * count
            method.restype = wintypes.DWORD
        for source in ("AC", "DC"):
            method = getattr(self.power, f"PowerWrite{source}ValueIndex")
            method.argtypes = [ctypes.c_void_p] * 4 + [wintypes.DWORD]
            method.restype = wintypes.DWORD

    @staticmethod
    def guid(value):
        return (ctypes.c_ubyte * 16).from_buffer_copy(uuid.UUID(value).bytes_le)

    def call(self, name, *args, missing_ok=False):
        code = int(getattr(self.power, name)(*args))
        self.operations.append({"api": name, "code": code})
        if code and not (missing_ok and code in (2, 1168)):
            raise OSError(code, name)
        return code

    def active(self):
        pointer = ctypes.c_void_p()
        self.call("PowerGetActiveScheme", None, ctypes.byref(pointer))
        if not pointer.value:
            raise RuntimeError("PowerGetActiveScheme未返回GUID")
        try:
            return str(uuid.UUID(bytes_le=ctypes.string_at(pointer, 16)))
        finally:
            if self.kernel.LocalFree(pointer):
                raise OSError("PowerGetActiveScheme GUID释放失败")

    def settings(self, scheme):
        result = {}
        for source in ("AC", "DC"):
            value = ctypes.c_uint32()
            self.call(f"PowerRead{source}ValueIndex", None, self.guid(scheme),
                      self.guid(VIDEO), self.guid(VIDEOIDLE), ctypes.byref(value))
            result[source] = value.value
        return result

    def source(self):
        # SYSTEM_POWER_STATUS共12字节；第一字节ACLineStatus 0=DC，1=AC，255未知。
        data = (ctypes.c_ubyte * 12)()
        if not self.kernel.GetSystemPowerStatus(data) or data[0] not in (0, 1):
            raise RuntimeError("当前供电方式未知，不修改临时电源设置")
        return "AC" if data[0] == 1 else "DC"

    def identity(self, pid):
        from ctypes import wintypes
        handle = self.kernel.OpenProcess(0x1000, False, pid)  # QUERY_LIMITED_INFORMATION
        if not handle:
            error = ctypes.get_last_error()
            if error == 87:
                return None
            raise OSError(error, "不能确认恢复记录owner是否已退出")
        try:
            times = [(ctypes.c_uint32 * 2)() for _ in range(4)]
            if not self.kernel.GetProcessTimes(handle, *(ctypes.byref(t) for t in times)):
                raise OSError(ctypes.get_last_error(), "GetProcessTimes")
            code = wintypes.DWORD()
            if not self.kernel.GetExitCodeProcess(handle, ctypes.byref(code)):
                raise OSError(ctypes.get_last_error(), "GetExitCodeProcess")
            if code.value != 259:
                return None
            return {"pid": int(pid), "created": int(times[0][0]) | (int(times[0][1]) << 32)}
        finally:
            if not self.kernel.CloseHandle(handle):
                raise OSError(ctypes.get_last_error(), "CloseHandle")

    def duplicate(self, original, temporary):
        command = [str(Path(os.environ["SystemRoot"]) / "System32" / "powercfg.exe"),
                   "/duplicatescheme", original, temporary]
        completed = subprocess.run(command, shell=False, capture_output=True, timeout=15,
                                   creationflags=subprocess.CREATE_NO_WINDOW)
        self.operations.append({"command": command, "returncode": completed.returncode,
            "stdout_hex": completed.stdout.hex(), "stderr_hex": completed.stderr.hex()})
        if completed.returncode:
            raise RuntimeError(f"powercfg复制临时方案失败：{completed.returncode}")

    def write_idle(self, scheme, source, value):
        if source not in ("AC", "DC"):
            raise ValueError("无效供电类型")
        self.call(f"PowerWrite{source}ValueIndex", None, self.guid(scheme),
                  self.guid(VIDEO), self.guid(VIDEOIDLE), value)

    def activate(self, scheme):
        self.call("PowerSetActiveScheme", None, self.guid(scheme))

    def delete(self, scheme):
        self.call("PowerDeleteScheme", None, self.guid(scheme), missing_ok=True)


def _restore(path, state, api):
    original, temporary = str(uuid.UUID(state["original"])), str(uuid.UUID(state["temporary"]))
    if original == temporary:
        raise ValueError("恢复记录原方案与临时方案相同")
    active = api.active()
    state["restore_before_active"] = active
    if active == temporary:
        state["phase"] = "restoring"
        _write(path, state)
        state["restore_pre_set_active"] = api.active()
        if state["restore_pre_set_active"] != temporary:
            state["user_plan_conflict"] = True
        else:
            api.activate(original)
            if api.active() != original:
                raise RuntimeError("原活动电源方案未恢复；保留journal")
    elif active != original:
        state["user_plan_conflict"] = True  # 保留用户选择，只删本轮非活动副本。
    api.delete(temporary)
    state["final_active"] = api.active()
    state["original_after"] = api.settings(original)
    state["original_values_unchanged"] = state["original_after"] == state["original_values"]
    state["phase"] = "conflict" if (state.get("user_plan_conflict") or
        state["final_active"] != original or not state["original_values_unchanged"]) else "restored"
    _write(path, state)
    if state["phase"] != "restored":
        raise RuntimeError("用户电源方案或原值已变化；已保留用户选择并清理临时副本，需核对journal")


def _recover(path, api):
    path = Path(path)
    if not path.exists():
        return None
    state = json.loads(path.read_text(encoding="utf-8"))
    if state.get("schema") != SCHEMA:
        raise ValueError("未知电源恢复journal，禁止据此修改方案")
    if state["phase"] == "restored":
        return state
    if state["phase"] == "conflict":
        raise RuntimeError("上轮电源恢复存在用户冲突；先核对journal")
    if api.identity(state["owner"]["pid"]) == state["owner"]:
        raise RuntimeError("临时电源方案owner仍存活，禁止接管")
    _restore(path, state, api)
    return state


def recover_video_idle(record, journal, *, api=None):
    """新一轮窗口前调用；只恢复明确属于本journal且owner已退出的副本。"""
    api = api or NativePower(record.setdefault("operations", []))
    with _exclusive(journal):
        record["previous_recovery"] = _recover(journal, api)


@contextmanager
def temporary_video_idle(record, journal, *, api=None):
    """显式选择，已ON后调用；成功／失败／取消均先恢复再返回调用者。"""
    api = api or NativePower(record.setdefault("operations", []))
    with _exclusive(journal):
        record["previous_recovery"] = _recover(journal, api)
        original = api.active()
        state = {"schema": SCHEMA, "phase": "copy_pending", "owner": api.identity(os.getpid()),
            "original": original, "temporary": str(uuid.uuid4()), "original_values": api.settings(original),
            "source": api.source()}
        if state["owner"] is None:
            raise RuntimeError("无法确认当前电源方案owner")
        record["journal"] = str(Path(journal).resolve())
        record["state"] = state
        _write(journal, state)  # 原状态、已知目标GUID先于任何复制／写入／激活。
        primary = None
        try:
            api.duplicate(original, state["temporary"])
            state["phase"] = "write_pending"; _write(journal, state)
            api.write_idle(state["temporary"], state["source"], 0)
            expected = dict(state["original_values"]); expected[state["source"]] = 0
            state["temporary_values"] = api.settings(state["temporary"])
            if state["temporary_values"] != expected or api.active() != original:
                raise RuntimeError("临时设置不符或用户已换方案；未激活副本")
            state["phase"] = "activate_pending"; _write(journal, state)
            state["activate_pre_set_active"] = api.active()
            if state["activate_pre_set_active"] != original:
                state["user_plan_conflict"] = True
                raise RuntimeError("持久化后用户已换方案；未激活临时副本")
            api.activate(state["temporary"])
            if api.active() != state["temporary"]:
                raise RuntimeError("临时电源方案未成为活动方案")
            record["changed"] = True
            state["phase"] = "held"; _write(journal, state)
            yield record
        except BaseException as error:
            primary = error
            record["error"] = {"type": type(error).__name__, "message": str(error)}
            raise
        finally:
            try:
                _restore(journal, state, api)
                record["restored"] = True
            except BaseException as error:
                record["restore_error"] = {"type": type(error).__name__, "message": str(error)}
                if primary is None:
                    raise
