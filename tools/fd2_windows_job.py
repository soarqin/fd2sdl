"""Windows 图形测试的进程归属和日志等待。"""

import ctypes
import sys
import time


def owned_job_required():
    if sys.platform != "win32":
        raise RuntimeError("该工具需要 Windows owned Job；未创建产物")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.GetCurrentProcess.restype = ctypes.c_void_p
    kernel.IsProcessInJob.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
    present = ctypes.c_int()
    if not kernel.IsProcessInJob(kernel.GetCurrentProcess(), None, ctypes.byref(present)) or not present.value:
        raise RuntimeError("请用本轮 driver run-job 启动；未创建产物或启动窗口")


def wait_log(process, path, expected, timeout=30, *, after=0):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        text = path.read_bytes()[after:].decode("utf-8", errors="replace")
        if expected in text:
            return text
        if process.poll() is not None:
            raise RuntimeError(f"game exited {process.returncode} before {expected!r}\n{text}")
        time.sleep(0.1)
    raise TimeoutError(f"missing {expected!r}; see {path}")
