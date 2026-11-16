"""F4 actual-only CLI的文件边界负例；无游戏、设备、GUI或原资源读写。"""
from __future__ import annotations

import argparse
import hashlib
import os
from pathlib import Path
import subprocess
import tempfile


def fact(path):
    raw = path.read_bytes()
    return hashlib.sha256(raw).hexdigest(), len(raw), path.stat().st_mtime_ns


def check(probe):
    calls = 0
    with tempfile.TemporaryDirectory(prefix="fd2-f4-output-") as temporary:
        root = Path(temporary).resolve()
        resources, output = root / "original_game", root / "output"
        resources.mkdir()
        output.mkdir()
        sentinel = resources / "sentinel.bin"
        sentinel.write_bytes(b"owned test sentinel, not FD2 game data")
        protected = fact(sentinel)

        def rejected(path):
            nonlocal calls
            result = subprocess.run([str(probe), "--f4-defeat-pages", "31", "0", str(path)],
                                    cwd=root, capture_output=True, text=True,
                                    encoding="utf-8", errors="replace", timeout=5)
            calls += 1
            # Empty original_game has no gameplay assets: rc2 proves the
            # output gate ran before the resource-reading CHECK/rc1 path.
            if result.returncode != 2:
                raise AssertionError(f"output rejection rc={result.returncode}: {result.stdout}{result.stderr}")

        existing = output / "existing.bin"
        existing.write_bytes(b"must not overwrite")
        before = fact(existing)
        rejected(existing)
        if fact(existing) != before:
            raise AssertionError("existing output changed")

        blocked = resources / "blocked.bin"
        rejected(blocked)
        if blocked.exists():
            raise AssertionError("resource directory was written")

        missing = root / "absent-parent" / "new.bin"
        rejected(missing)
        if missing.parent.exists():
            raise AssertionError("probe created an output directory")

        alias = root / "resource-alias"
        if os.name == "nt":
            environment = os.environ.copy()
            environment.update(FD2_TEST_JUNCTION_LINK=str(alias),
                               FD2_TEST_JUNCTION_TARGET=str(resources))
            made = subprocess.run([
                "powershell.exe", "-NoProfile", "-NonInteractive", "-Command",
                "New-Item -ItemType Junction -Path $env:FD2_TEST_JUNCTION_LINK "
                "-Target $env:FD2_TEST_JUNCTION_TARGET -ErrorAction Stop | Out-Null"],
                env=environment, capture_output=True, timeout=10,
                creationflags=subprocess.CREATE_NO_WINDOW)
            if made.returncode:
                raise AssertionError("owned junction setup failed")
        else:
            alias.symlink_to(resources, target_is_directory=True)
        try:
            blocked = alias / "alias-blocked.bin"
            rejected(blocked)
            if blocked.exists():
                raise AssertionError("resource junction/symlink was written")
        finally:
            # Only remove this owned link, never recursively traverse target.
            if os.name == "nt":
                alias.rmdir()
            else:
                alias.unlink()

        if os.name == "nt":
            rejected(output / "con.bin")
        if fact(sentinel) != protected or {p.name for p in resources.iterdir()} != {sentinel.name}:
            raise AssertionError("owned read-only sentinel changed")
    print(f"F4 probe output paths: {calls} real CLI rejections; no game/assets/device/GUI")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    args = parser.parse_args()
    check(args.probe.resolve())


if __name__ == "__main__":
    main()
