"""Windows 正式 SDL 输入回归；沿用 XTest 的 stage 0 序列和结果断言。"""

import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import time
import unittest
from unittest.mock import Mock, call, patch


def check_bitmap(path):
    """验证宿主客户区确实生成了有内容的 24-bit BMP，不作原版视觉等价判断。"""
    data = Path(path).read_bytes()
    if len(data) < 54 or data[:2] != b"BM":
        raise ValueError("missing BMP header")
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height, planes, bits, compression = struct.unpack_from("<iiHHI", data, 18)
    if width < 320 or abs(height) < 200 or planes != 1 or bits != 24 or compression:
        raise ValueError("unexpected BMP dimensions or format")
    stride = (width * 3 + 3) & ~3
    if offset < 54 or len(data) != offset + stride * abs(height):
        raise ValueError("truncated BMP pixels")
    colors = set()
    for row in range(abs(height)):
        start = offset + row * stride
        colors.update(data[p:p + 3] for p in range(start, start + width * 3, 3))
        if len(colors) >= 8:
            return {"width": width, "height": abs(height), "colors_at_least": 8}
    raise ValueError("blank or nearly uniform window capture")


def wait_log(process, path, expected, timeout=30):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        text = path.read_text(encoding="utf-8", errors="replace")
        if expected in text:
            return text
        if process.poll() is not None:
            raise RuntimeError(f"game exited {process.returncode} before {expected!r}\n{text}")
        time.sleep(0.1)
    raise TimeoutError(f"missing {expected!r}; see {path}")


WAIT_EXPECTED = [
    "field cursor: cell=(7,21)",
    "field action: unit=0 command=wait acted=1 remaining=3 phase=2",
    "field action: unit=1 command=wait acted=1 remaining=2 phase=2",
    "field action: unit=2 command=wait acted=1 remaining=1 phase=2",
    "field action: unit=3 command=wait acted=1 remaining=0 phase=1",
    "field acted gate: unit=0, interaction=browse",
    "field phase: turn=1 phase=1 remaining=0",
    "field phase: turn=1 phase=0 remaining=8",
    "field phase: turn=2 phase=2 remaining=4",
]
ATTACK_EXPECTED = [
    "field action: unit=2 command=attack acted=1 remaining=3 phase=2",
    "field attack: target=10 strikes=1 hit=1 critical=0 base=24 rolled=25 damage=25 total=25 hp=3 defeated=0 hidden=0",
    "field acted gate: unit=2, interaction=browse",
]
LETHAL_EXPECTED = [
    ATTACK_EXPECTED[0],
    "field attack: target=10 strikes=2 hit=1 critical=0 base=24 rolled=24 damage=4 total=28 hp=0 defeated=1 hidden=1",
    ATTACK_EXPECTED[-1],
]


def focus_initial_actor(driver, process, log, mode):
    """非幂等Z逐次等真实焦点ACK；漏采时停止，不让Enter误开系统确认框。"""
    if mode not in ("wait", "attack"):
        raise ValueError(f"unsupported input scenario: {mode}")
    for unit in range(1 if mode == "wait" else 3):
        driver("key", "Z")
        prefix = f"field focus: unit={unit},"
        text = wait_log(process, log, prefix, timeout=10)
        focused = [line for line in text.splitlines() if line.startswith("field focus:")][-1]
        if not focused.startswith(prefix) or not focused.endswith(", interaction=browse"):
            raise AssertionError(f"unexpected focus acknowledgement: {focused}; see {log}")
    return f"{mode}-focused"


def run_case(args, name, mode, seed, expected):
    # 每轮使用新目录，失败现场保留，也不读取上一次运行留下的存档。
    work = Path(tempfile.mkdtemp(prefix=f"{name}-", dir=args.artifacts))
    log = work / "game.log"
    env = os.environ.copy()
    env.update(SDL_VIDEODRIVER="windows", SDL_AUDIODRIVER="dummy",
               FD2SDL_SAVE_PATH=str(work / "FD2.SAV"), FD2SDL_FIELD_RNG_SEED=str(seed))
    captures = []

    def driver(command, *extra):
        if process.poll() is not None:
            raise RuntimeError(f"game exited {process.returncode} before {command}")
        subprocess.run([str(args.driver), command, str(process.pid), *map(str, extra)],
                       check=True, timeout=35)

    def capture(name):
        path = work / f"{name}.bmp"
        driver("capture", path)
        captures.append({"path": str(path), **check_bitmap(path)})

    with log.open("wb") as output:
        process = subprocess.Popen([str(args.game), "--field-play", "0"],
                                   cwd=args.root, env=env, stdout=output,
                                   stderr=subprocess.STDOUT)
        try:
            wait_log(process, log, "field input ready: stage=0 turn=1 phase=2")
            driver("focus")
            capture("start")
            focused_mode = focus_initial_actor(driver, process, log, mode)
            driver(focused_mode)
            text = wait_log(process, log, expected[-1])
            for line in expected:
                if line not in text:
                    raise AssertionError(f"missing {line!r}; see {log}")
            capture("end")
            driver("close")
            if process.wait(timeout=10) != 0:
                raise RuntimeError(f"game close exited {process.returncode}; see {log}")
        finally:
            if process.poll() is None:
                process.terminate()
                try:
                    process.wait(timeout=5)
                except subprocess.TimeoutExpired:
                    process.kill()
                    process.wait(timeout=5)
    return {"name": name, "seed": seed, "captures": captures, "log": str(log)}


class HarnessTest(unittest.TestCase):
    def test_focus_acknowledgement_precedes_each_next_key(self):
        for mode, count in (("wait", 1), ("attack", 3)):
            with self.subTest(mode=mode):
                events = Mock()
                events.wait.side_effect = [
                    f"field focus: unit={unit}, cell=(7,20), interaction=browse\n"
                    for unit in range(count)]
                with patch(__name__ + ".wait_log", events.wait):
                    self.assertEqual(focus_initial_actor(events.driver, "process", "log", mode),
                                     f"{mode}-focused")
                expected = []
                for unit in range(count):
                    expected.extend((call.driver("key", "Z"), call.wait(
                        "process", "log", f"field focus: unit={unit},", timeout=10)))
                self.assertEqual(events.mock_calls, expected)

    def test_missing_focus_acknowledgement_stops_without_more_keys(self):
        driver = Mock()
        with patch(__name__ + ".wait_log", side_effect=TimeoutError("missed Z")):
            with self.assertRaisesRegex(TimeoutError, "missed Z"):
                focus_initial_actor(driver, "process", "log", "attack")
        driver.assert_called_once_with("key", "Z")

    def test_later_wrong_focus_cannot_reuse_an_earlier_acknowledgement(self):
        driver = Mock()
        text = ("field focus: unit=0, cell=(7,20), interaction=browse\n"
                "field focus: unit=1, cell=(8,20), interaction=browse\n")
        with patch(__name__ + ".wait_log", return_value=text):
            with self.assertRaisesRegex(AssertionError, "unexpected focus"):
                focus_initial_actor(driver, "process", "log", "attack")
        driver.assert_called_once_with("key", "Z")

    def test_bitmap_validation(self):
        with tempfile.TemporaryDirectory() as temp:
            path = Path(temp) / "capture.bmp"
            width, height = 320, 200
            pixels = bytes(width * height * 3)
            header = struct.pack("<2sIHHI", b"BM", 54 + len(pixels), 0, 0, 54)
            header += struct.pack("<IiiHHIIiiII", 40, width, height, 1, 24, 0,
                                  len(pixels), 0, 0, 0, 0)
            path.write_bytes(header + pixels)
            with self.assertRaises(ValueError):
                check_bitmap(path)
            pixels = bytes(range(240)) * (len(pixels) // 240)
            path.write_bytes(header + pixels)
            self.assertEqual(check_bitmap(path)["width"], width)
            path.write_bytes(header + pixels[:-1])
            with self.assertRaises(ValueError):
                check_bitmap(path)
            path.write_bytes(b"not a bitmap")
            with self.assertRaises(ValueError):
                check_bitmap(path)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--self-test", action="store_true")
    for name in ("game", "driver", "root", "artifacts"):
        parser.add_argument(f"--{name}", type=Path)
    args = parser.parse_args()
    if args.self_test:
        result = unittest.TextTestRunner().run(unittest.defaultTestLoader.loadTestsFromTestCase(HarnessTest))
        return 0 if result.wasSuccessful() else 1
    if not all((args.game, args.driver, args.root, args.artifacts)):
        parser.error("--game, --driver, --root and --artifacts are required")
    if sys.platform != "win32":
        parser.error("this runner requires Windows; Linux uses field_play_input_test.sh")
    for name in ("game", "driver", "root", "artifacts"):
        setattr(args, name, getattr(args, name).resolve())
    args.artifacts.mkdir(parents=True, exist_ok=True)
    summary = {"platform": sys.platform, "status": "failed", "cases": []}
    try:
        for name, mode, seed, expected in (
                ("wait", "wait", 0x7a18, WAIT_EXPECTED),
                ("attack", "attack", 0x7a18, ATTACK_EXPECTED),
                ("lethal", "attack", 496, LETHAL_EXPECTED)):
            summary["cases"].append(run_case(args, name, mode, seed, expected))
            print(f"{name}: passed", flush=True)
        summary["status"] = "passed"
        return 0
    except (OSError, ValueError, RuntimeError, AssertionError, subprocess.SubprocessError) as exc:
        summary["error"] = str(exc)
        print(f"输入回归未通过：{exc}；查看产物目录后重跑。", file=sys.stderr)
        return 1
    finally:
        (args.artifacts / "summary.json").write_text(
            json.dumps(summary, ensure_ascii=False, indent=2), encoding="utf-8")


if __name__ == "__main__":
    sys.exit(main())
