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


def drive_attack_case(driver, process, log, expected):
    """实际auto-target ACK后提交；奖励后新ready才允许返回已行动的caster。"""
    def confirm(marker):
        offset = log.stat().st_size
        driver("key", "ENTER")
        return wait_log(process, log, marker, after=offset)

    driver("key", "ENTER")  # 初选简表保留下一方向键，不能在此等待收框日志。
    for _ in range(3):
        driver("key", "LEFT")
    text = confirm("field cursor: cell=(5,22)")
    cursor = [line for line in text.splitlines() if line.startswith("field cursor:")][-1]
    if not cursor.startswith("field cursor: cell=(5,22),") or "interaction=3," not in cursor:
        raise AssertionError("movement did not reach COMMAND at (5,22)")
    driver("key", "UP")
    text = confirm("field cursor: cell=(3,22)")
    cursor = [line for line in text.splitlines() if line.startswith("field cursor:")][-1]
    if not cursor.startswith("field cursor: cell=(3,22),") or "interaction=4," not in cursor:
        raise AssertionError("attack did not auto-focus target (3,22)")
    offset = log.stat().st_size
    driver("key", "ENTER")
    text = wait_log(process, log, expected[1], after=offset)
    if expected[0] not in text:
        raise AssertionError("attack result has no matching committed action")
    text = wait_log(process, log, "field input ready: stage=0 turn=1 phase=2", after=offset)
    if ("field progress: kind=1 actor=2 fragment=488" not in text or
            "field progress: kind=5 actor=2 fragment=0" not in text):
        raise AssertionError("post-attack ready has no complete GAIN/CLOSE sequence")
    for _ in range(2):
        driver("key", "RIGHT")
    confirm(expected[-1])


def cleanup_process(process, driver):
    """只清理本轮Popen：所有结果先正常关闭，超时才terminate/kill。"""
    result = {"pid": process.pid, "normal_close_requested": False,
              "terminate_requested": False, "kill_requested": False, "errors": []}
    if process.poll() is None:
        result["normal_close_requested"] = True
        try:
            driver("close")
        except Exception as error:
            result["errors"].append(f"normal close: {error}")
        try:
            process.wait(timeout=10)
        except subprocess.TimeoutExpired:
            pass
        except OSError as error:
            result["errors"].append(f"normal wait: {error}")
    if process.poll() is None:
        result["terminate_requested"] = True
        try:
            process.terminate()
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            pass
        except OSError as error:
            result["errors"].append(f"terminate: {error}")
    if process.poll() is None:
        result["kill_requested"] = True
        try:
            process.kill()
            process.wait(timeout=5)
        except (OSError, subprocess.TimeoutExpired) as error:
            result["errors"].append(f"kill: {error}")
    result["exit_code"] = process.poll()
    result["forced"] = result["terminate_requested"] or result["kill_requested"]
    result["cleaned"] = result["exit_code"] is not None
    result["ok"] = result["cleaned"] and result["exit_code"] == 0 and not result["forced"] and not result["errors"]
    return result


def run_case(args, name, mode, seed, expected):
    # 每轮使用新目录，失败现场保留，也不读取上一次运行留下的存档。
    work = Path(tempfile.mkdtemp(prefix=f"{name}-", dir=args.artifacts))
    log = work / "game.log"
    env = os.environ.copy()
    env.update(SDL_VIDEODRIVER="windows", SDL_AUDIODRIVER="dummy",
               FD2SDL_SAVE_PATH=str(work / "FD2.SAV"), FD2SDL_FIELD_RNG_SEED=str(seed),
               SDL_WINDOW_ACTIVATE_WHEN_SHOWN="0", SDL_WINDOW_ACTIVATE_WHEN_RAISED="0",
               SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS="0", SDL_WINDOWS_RAW_KEYBOARD="0")
    captures = []
    report = {"name": name, "seed": seed, "status": "failed", "captures": captures, "log": str(log)}

    def driver(command, *extra):
        if process.poll() is not None:
            raise RuntimeError(f"game exited {process.returncode} before {command}")
        argv = [str(args.driver), command, str(process.pid), *map(str, extra)]
        record = {"argv": argv, "pid": process.pid}
        try:
            completed = subprocess.run(argv, capture_output=True, text=True,
                                       encoding="utf-8", errors="backslashreplace", timeout=35)
            record.update(returncode=completed.returncode, stdout=completed.stdout, stderr=completed.stderr)
        except (OSError, subprocess.TimeoutExpired) as error:
            record["error"] = str(error)
            raise
        finally:
            with (work / "input-receipts.jsonl").open("a", encoding="utf-8") as stream:
                stream.write(json.dumps(record, ensure_ascii=False) + "\n")
        if completed.stderr:
            print(completed.stderr, file=sys.stderr, end="", flush=True)
        completed.check_returncode()

    def capture(name):
        path = work / f"{name}.bmp"
        driver("capture", path)
        captures.append({"path": str(path), **check_bitmap(path)})

    process = None
    with log.open("wb") as output:
        try:
            process = subprocess.Popen([str(args.game), "--field-play", "0"],
                                       cwd=args.root, env=env, stdout=output,
                                       stderr=subprocess.STDOUT)
            wait_log(process, log, "field input ready: stage=0 turn=1 phase=2")
            capture("start")
            focused_mode = focus_initial_actor(driver, process, log, mode)
            if mode == "attack":
                drive_attack_case(driver, process, log, expected)
            else:
                driver(focused_mode)
            text = wait_log(process, log, expected[-1])
            for line in expected:
                if line not in text:
                    raise AssertionError(f"missing {line!r}; see {log}")
            capture("end")
            report["status"] = "passed"
        except Exception as error:
            report.setdefault("error", str(error))
        finally:
            if process is not None:
                report["cleanup"] = cleanup_process(process, driver)
                if not report["cleanup"]["ok"]:
                    report["status"] = "failed"
                    report.setdefault("error", "owned process did not close normally; see cleanup receipt")
            (work / "case.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


class HarnessTest(unittest.TestCase):
    def test_attack_auto_target_and_ready_precede_acted_check(self):
        driver, log = Mock(), Mock()
        log.stat.return_value.st_size = 123
        rows = ["field cursor: cell=(5,22), interaction=3,\n",
                "field cursor: cell=(3,22), interaction=4,\n",
                "\n".join(ATTACK_EXPECTED[:2]),
                "field progress: kind=1 actor=2 fragment=488\n"
                "field progress: kind=5 actor=2 fragment=0\nfield input ready: stage=0 turn=1 phase=2\n",
                ATTACK_EXPECTED[-1]]
        with patch(__name__ + ".wait_log", side_effect=rows) as wait:
            drive_attack_case(driver, "process", log, ATTACK_EXPECTED)
        self.assertEqual(driver.call_args_list, [call("key", key) for key in
            ("ENTER", "LEFT", "LEFT", "LEFT", "ENTER", "UP", "ENTER", "ENTER", "RIGHT", "RIGHT", "ENTER")])
        self.assertEqual(wait.call_args_list[3], call("process", log,
            "field input ready: stage=0 turn=1 phase=2", after=123))

    def test_missing_auto_target_or_post_reward_ready_stops_keys(self):
        command = "field cursor: cell=(5,22), interaction=3,\n"
        target = "field cursor: cell=(3,22), interaction=4,\n"
        for rows, key_count in (([command, TimeoutError("target missing")], 7),
                ([command, target, "\n".join(ATTACK_EXPECTED[:2]), TimeoutError("ready missing")], 8)):
            with self.subTest(key_count=key_count):
                driver, log = Mock(), Mock()
                log.stat.return_value.st_size = 123
                with patch(__name__ + ".wait_log", side_effect=rows):
                    with self.assertRaises(TimeoutError):
                        drive_attack_case(driver, "process", log, ATTACK_EXPECTED)
                self.assertEqual(driver.call_count, key_count)
                self.assertNotIn(call("key", "RIGHT"), driver.call_args_list)

    def test_startup_ready_and_close_started_do_not_prove_completion(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "game.log"
            initial = b"field input ready: stage=0 turn=1 phase=2\n"
            path.write_bytes(initial + b"field progress: kind=5 actor=2 fragment=0\n")
            process = Mock(); process.poll.return_value = None
            with patch(__name__ + ".time.monotonic", side_effect=(0, 0, 2)), patch(__name__ + ".time.sleep"):
                with self.assertRaises(TimeoutError):
                    wait_log(process, path, initial.decode().strip(), timeout=1, after=len(initial))
            with path.open("ab") as stream:
                stream.write(initial)
            self.assertIn("kind=5", wait_log(process, path, initial.decode().strip(), after=len(initial)))

    def test_cleanup_normally_closes_owned_process_without_force(self):
        process, driver = Mock(), Mock()
        process.pid = 123; process.returncode = None
        process.poll.side_effect = lambda: process.returncode
        def exited(timeout):
            self.assertEqual(timeout, 10)
            driver.assert_called_once_with("close")
            process.returncode = 0
        process.wait.side_effect = exited
        result = cleanup_process(process, driver)
        self.assertTrue(result["ok"] and result["cleaned"] and result["normal_close_requested"])
        self.assertFalse(result["forced"])
        process.terminate.assert_not_called(); process.kill.assert_not_called()

    def test_cleanup_timeout_escalates_only_after_normal_close(self):
        process, driver = Mock(), Mock()
        process.pid = 123; process.returncode = None
        process.poll.side_effect = lambda: process.returncode
        process.wait.side_effect = [subprocess.TimeoutExpired("game", 10),
                                   subprocess.TimeoutExpired("game", 5), -9]
        def killed():
            driver.assert_called_once_with("close")
            process.terminate.assert_called_once_with()
            process.returncode = -9
        process.kill.side_effect = killed
        result = cleanup_process(process, driver)
        self.assertTrue(result["forced"] and result["cleaned"] and result["kill_requested"])
        self.assertFalse(result["ok"])

    def test_cleanup_close_error_still_waits_and_keeps_its_error(self):
        process, driver = Mock(), Mock(side_effect=RuntimeError("close failed"))
        process.pid = 123; process.returncode = None
        process.poll.side_effect = lambda: process.returncode
        def exited(timeout):
            self.assertEqual(timeout, 10)
            process.returncode = 0
        process.wait.side_effect = exited
        result = cleanup_process(process, driver)
        self.assertTrue(result["cleaned"] and result["normal_close_requested"])
        self.assertFalse(result["ok"] or result["forced"])
        self.assertEqual(result["errors"], ["normal close: close failed"])
        process.terminate.assert_not_called(); process.kill.assert_not_called()

    def test_failed_case_keeps_first_error_and_cleanup_receipt(self):
        with tempfile.TemporaryDirectory() as directory:
            process = Mock(); process.pid = 123; process.poll.return_value = None
            cleanup = {"ok": False, "cleaned": True, "forced": True, "exit_code": -9}
            args = argparse.Namespace(artifacts=Path(directory), game=Path("game"),
                driver=Path("driver"), root=Path(directory))
            with patch(__name__ + ".subprocess.Popen", return_value=process), \
                    patch(__name__ + ".wait_log", side_effect=TimeoutError("first readiness failure")), \
                    patch(__name__ + ".cleanup_process", return_value=cleanup) as close:
                result = run_case(args, "attack", "attack", 0x7a18, ATTACK_EXPECTED)
            self.assertEqual(result["error"], "first readiness failure")
            self.assertEqual(result["cleanup"], cleanup)
            close.assert_called_once()
            receipt = next(Path(directory).glob("attack-*/case.json"))
            self.assertEqual(json.loads(receipt.read_text(encoding="utf-8"))["error"], result["error"])

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
            case = run_case(args, name, mode, seed, expected)
            summary["cases"].append(case)
            if case["status"] != "passed":
                raise RuntimeError(case["error"])
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
