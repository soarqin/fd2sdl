"""校验 Windows 专用工具已由本轮驱动置于 Job 中。"""

import ctypes
import sys


def owned_job_required():
    if sys.platform != "win32":
        raise RuntimeError("该工具需要 Windows owned Job；未创建产物")
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.GetCurrentProcess.restype = ctypes.c_void_p
    kernel.IsProcessInJob.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_int)]
    present = ctypes.c_int()
    if not kernel.IsProcessInJob(kernel.GetCurrentProcess(), None, ctypes.byref(present)) or not present.value:
        raise RuntimeError("请用本轮 driver run-job 启动；未创建产物或启动窗口")
