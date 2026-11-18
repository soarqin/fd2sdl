#!/usr/bin/env python3
"""无窗口验收 Windows run-job：参数、退出码、超时与父进程退出后的子孙清理。"""
import argparse
import ctypes
from ctypes import wintypes
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile


WORKER = '''import os
from pathlib import Path
import subprocess
import sys
import time
mode, log = sys.argv[1], Path(sys.argv[2])
with log.open("a") as stream:
    stream.write(str(os.getpid()) + "\\n")
if mode != "leaf":
    subprocess.Popen([sys.executable, __file__, "leaf", str(log)],
                     creationflags=subprocess.CREATE_NO_WINDOW)
    deadline = time.monotonic() + 5
    while len(log.read_text().splitlines()) < 2:
        if time.monotonic() >= deadline:
            raise RuntimeError("descendant did not start")
        time.sleep(0.01)
    if mode == "orphan":
        sys.exit(0)
time.sleep(60)
'''


def owned_survivors(log, terminate=False):
    """只检查本轮worker写下的PID；自检失败的兜底也不按进程名清理。"""
    if not log.exists():
        return []
    kernel = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [wintypes.DWORD, wintypes.BOOL, wintypes.DWORD]
    kernel.OpenProcess.restype = wintypes.HANDLE
    kernel.WaitForSingleObject.argtypes = [wintypes.HANDLE, wintypes.DWORD]
    kernel.TerminateProcess.argtypes = [wintypes.HANDLE, wintypes.UINT]
    kernel.CloseHandle.argtypes = [wintypes.HANDLE]
    alive = []
    for pid in map(int, log.read_text().splitlines()):
        handle = kernel.OpenProcess(0x100001, False, pid)  # SYNCHRONIZE | TERMINATE
        if not handle:
            if ctypes.get_last_error() != 87:  # ERROR_INVALID_PARAMETER: PID已结束。
                raise ctypes.WinError(ctypes.get_last_error())
            continue
        try:
            waited = kernel.WaitForSingleObject(handle, 0)
            if waited == 258:  # WAIT_TIMEOUT: 仍在运行。
                alive.append(pid)
                if terminate:
                    if not kernel.TerminateProcess(handle, 137):
                        raise ctypes.WinError(ctypes.get_last_error())
                    if kernel.WaitForSingleObject(handle, 5000) != 0:
                        raise RuntimeError(f"owned self-test PID {pid} not reaped")
            elif waited != 0:
                raise ctypes.WinError(ctypes.get_last_error())
        finally:
            kernel.CloseHandle(handle)
    return alive


def check(driver, artifacts):
    report = {"ok": False, "scope": "headless Windows process-tree guard; no game/window acceptance", "cases": []}
    artifacts.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="job-", dir=artifacts))
    try:
        worker = work / "worker.py"
        worker.write_text(WORKER, encoding="utf-8")

        def run(name, command, expected, timeout=5000, markers=()):
            result = subprocess.run([str(driver), "run-job", str(timeout), *command],
                                    capture_output=True, text=True, timeout=timeout / 1000 + 15)
            (work / f"{name}.log").write_text(result.stdout + result.stderr, encoding="utf-8")
            receipt = {"name": name, "exit_code": result.returncode,
                       "expected_exit": expected, "status": "failed",
                       "receipt": result.stderr.strip()}
            report["cases"].append(receipt)
            if result.returncode != expected or "cleaned=1" not in result.stderr:
                raise AssertionError(f"{name}: exit={result.returncode}; {result.stdout}{result.stderr}")
            for marker in markers:
                if marker not in result.stderr:
                    raise AssertionError(f"{name}: missing {marker!r}: {result.stderr}")
            receipt["status"] = "passed"
            return result.stderr

        arguments = ["", "with spaces", 'with"quote', "trailing\\", 'slash\\"quote']
        code = f"import sys; sys.exit(0 if sys.argv[1:] == {arguments!r} else 9)"
        run("argument-roundtrip", [sys.executable, "-c", code, *arguments], 0,
            markers=("forced=0", "residual=0", "timed_out=0"))
        run("preserved-worker-error", [sys.executable, "-c", "raise SystemExit(7)"], 7,
            markers=("forced=0",))
        for mode, expected, marker in (("timeout", 124, "timed_out=1"), ("orphan", 125, "residual=1")):
            log = work / f"{mode}-pids.txt"
            try:
                diagnostic = run(mode + "-descendants", [sys.executable, str(worker), mode, str(log)],
                                 expected, timeout=1500, markers=(marker, "forced=1"))
                if not log.exists() or len(log.read_text().splitlines()) != 2:
                    raise AssertionError(f"{mode}: worker and grandchild were not both observed")
                worker_pids = set(map(int, log.read_text().splitlines()))
                expected_members = worker_pids.copy()
                if mode == "orphan":
                    expected_members.remove(int(log.read_text().splitlines()[0]))
                rows = re.findall(r"forced_member=(\d+) image=(.+) image_error=(\d+)", diagnostic)
                forced_members = {int(pid) for pid, _, _ in rows}
                # Windows 可为每个隐藏 console 附带本轮 conhost；仍由整 Job 清空门核验。
                if forced_members & worker_pids != expected_members or any(int(err) for _, _, err in rows):
                    raise AssertionError(f"{mode}: actual forced members not identified: {diagnostic}")
                for pid, image, _ in rows:
                    if int(pid) in expected_members and not Path(image).samefile(sys.executable):
                        raise AssertionError(f"{mode}: wrong image for owned worker {pid}: {image}")
                alive = owned_survivors(log)
                if alive:
                    raise AssertionError(f"{mode}: job leaked owned PIDs {alive}")
            finally:
                owned_survivors(log, terminate=True)
        report["ok"] = True
        return report
    except Exception as error:
        report["error"] = str(error)
        raise
    finally:
        (work / "summary.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        print(work, flush=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--driver", type=Path, required=True)
    parser.add_argument("--artifacts", type=Path, required=True)
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("此自检要求 Windows Job API；未启动任何测试进程")
    try:
        print(json.dumps(check(args.driver.resolve(), args.artifacts.resolve()), ensure_ascii=False, indent=2))
        return 0
    except Exception as error:
        print(f"Windows Job 无窗口自检未通过：{error}。修复后使用同一命令重跑。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
