"""两offline probe的真实CLI文件边界；只写自有临时目录，不初始化设备/GUI。"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import os
import shutil
import subprocess
import tempfile


def fact(path):
    data = path.read_bytes()
    return hashlib.sha256(data).hexdigest(), len(data), path.stat().st_mtime_ns


def check(trace_probe, wav_probe, original):
    sources = {name: fact(original / name) for name in ("FDMUS.DAT", "SAMPLE.AD")}
    calls = 0
    with tempfile.TemporaryDirectory(prefix="fd2-audio-output-") as temporary:
        root = Path(temporary).resolve()
        resources = root / "original_game"
        resources.mkdir()
        for name in sources:
            shutil.copyfile(original / name, resources / name)
        output = root / "输出-中文"
        output.mkdir()
        sub = output / "sub"
        sub.mkdir()
        environment = os.environ.copy()
        for key in ("FD2SDL_APP_PCM_CAPTURE_DIR", "FD2SDL_APP_PCM_CAPTURE_MAX_MS",
                    "FD2SDL_APP_PCM_CAPTURE_QUIET", "FD2SDL_TEST_AUDIO_QUIET"):
            environment.pop(key, None)

        def run(kind, paths, success=False):
            nonlocal calls
            arguments = ([str(trace_probe), "--original", str(resources), "--frames", "1",
                          "--trace", str(paths[0]), "--metadata", str(paths[1])]
                         if kind == "trace" else [str(wav_probe), "18", "1", str(paths[0])])
            result = subprocess.run(arguments, cwd=root, env=environment, capture_output=True,
                                    text=True, encoding="utf-8", errors="replace", timeout=15)
            calls += 1
            if (result.returncode == 0) != success:
                raise AssertionError(f"{kind}: unexpected rc={result.returncode}\n{result.stdout}{result.stderr}")

        # 真正渲染一帧/一秒，确认UTF-8 argv、native exclusive写入和metadata未失效。
        trace, meta = output / "寄存器.jsonl", output / "元数据.json"
        run("trace", (trace, meta), True)
        if not trace.stat().st_size or json.loads(meta.read_text(encoding="utf-8"))["frames"] != 1:
            raise AssertionError("trace outputs are incomplete")
        wave = output / "原始音乐.wav"
        run("wave", (wave,), True)
        if wave.stat().st_size != 44 + 44100 * 4 or wave.read_bytes()[:4] != b"RIFF":
            raise AssertionError("wave output is incomplete")

        existing = output / "existing"
        existing.write_bytes(b"do not overwrite")
        before = fact(existing)
        first = output / "first.jsonl"
        run("trace", (first, existing))
        if first.exists() or fact(existing) != before:
            raise AssertionError("both outputs must be checked before the first write")
        run("wave", (existing,))
        if fact(existing) != before:
            raise AssertionError("existing output changed")

        same = output / "same.json"
        run("trace", (same, sub / ".." / same.name))
        if same.exists():
            raise AssertionError("two aliases of one new output were accepted")
        for kind in ("trace", "wave"):
            blocked = resources / (kind + "-blocked")
            run(kind, (blocked, output / "blocked-meta") if kind == "trace" else (blocked,))
            if blocked.exists():
                raise AssertionError("resource directory was written")

        alias = root / "resource-alias"
        if os.name == "nt":
            junction_env = environment.copy()
            junction_env.update(FD2_TEST_JUNCTION_LINK=str(alias),
                                FD2_TEST_JUNCTION_TARGET=str(resources))
            made = subprocess.run([
                "powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
                "New-Item -ItemType Junction -Path $env:FD2_TEST_JUNCTION_LINK "
                "-Target $env:FD2_TEST_JUNCTION_TARGET -ErrorAction Stop | Out-Null"],
                env=junction_env, capture_output=True, creationflags=subprocess.CREATE_NO_WINDOW)
            if made.returncode:
                raise AssertionError(f"owned test junction could not be created: {made.stdout!r} {made.stderr!r}")
        else:
            alias.symlink_to(resources, target_is_directory=True)
        try:
            for kind in ("trace", "wave"):
                blocked = alias / (kind + "-alias-blocked")
                run(kind, (blocked, output / "alias-meta") if kind == "trace" else (blocked,))
                if blocked.exists():
                    raise AssertionError("resource junction/symlink was written")
        finally:
            if os.name == "nt":
                alias.rmdir()  # 仅移除本测试创建的junction，不遍历目标。
            else:
                alias.unlink()
        if {p.name for p in resources.iterdir()} != set(sources):
            raise AssertionError("unexpected file in the owned resource copy")
        if any(fact(original / name) != value for name, value in sources.items()):
            raise AssertionError("read-only source resources changed")
    print(f"audio probe output paths: {calls} real offline CLI cases; no device/GUI")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--trace-probe", required=True, type=Path)
    parser.add_argument("--wav-probe", required=True, type=Path)
    parser.add_argument("--original", required=True, type=Path)
    args = parser.parse_args()
    check(args.trace_probe.resolve(), args.wav_probe.resolve(), args.original.resolve())


if __name__ == "__main__":
    main()
