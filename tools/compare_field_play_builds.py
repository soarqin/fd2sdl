"""对比两份 SDL 程序在相同 stage0 战场输入下的可观察结果（Windows）。

用于重构类改动的旧版／新版执行对照：同一 RNG 种子、隔离存档目录、
经 fd2_field_play_input_driver 向本轮 PID 的窗口后台投递按键，记录
stdout 逻辑轨迹（field ...: 行）、FD2.SAV 和每步 WGC 单窗口截图。

判定：逻辑轨迹与存档必须逐字节一致；截图含水面、待机和光标等实时
动画，按像素差异计数报告，并以同一旧版两次运行的差异作为噪声基线。
它不是原版对照，也不代替 CTest。

用法（PowerShell）：
  python tools/compare_field_play_builds.py --root . `
    --old-game build/ab/old-build/Debug/fd2sdl.exe `
    --new-game src/build-win/Debug/fd2sdl.exe `
    --driver src/build-win/Debug/fd2_field_play_input_driver.exe `
    --artifacts build/ab/runs
"""

import argparse
import json
import os
from pathlib import Path
import struct
import subprocess
import sys
import time

READY = "field input ready: stage=0 turn=1 phase=2"

# 场景用空格分隔的按键序列表示；"KEY!" 表示按键后等待新的 input ready 行
# （只在奖励／事件提示后重报），"KEY^" 表示等待回到第 2 回合玩家阶段。
# 浏览时 ESC/Z 为焦点轮换，A 无映射。
# 方向在径向菜单中依次选 UP=0、LEFT=1、RIGHT=2、DOWN=3：
# 指令菜单为攻击／法术／物品／待机；系统菜单为次级页／出击／选项／结束回合；
# 次级页为总览／存档／读档／离开战场；确认框 LEFT=是、RIGHT=否。
SCENARIOS = {
    # 四名玩家逐个待机，敌方回合自动行动后进入第 2 回合。
    "wait-round": "Z wait-focused",
    # unit2 移动后攻击 (3,22)，结算奖励，再回到攻击者触发已行动详情。
    "attack": "Z Z Z ENTER LEFT LEFT LEFT ENTER UP ENTER ENTER! RIGHT RIGHT ENTER ESC",
    # 选人取消；移动后指令菜单取消；无目标攻击后取消。
    "cancel": "Z ENTER ESC ENTER LEFT ENTER ESC ENTER LEFT ENTER UP ENTER ESC ESC",
    # 详情页（F2/HOME）翻页与关闭、辅助视图（F1）开合、敌方单位详情。
    "detail-aux": ("Z F2 ENTER ENTER F1 ESC Z Z HOME ENTER ENTER "
                   "LEFT LEFT LEFT LEFT LEFT ENTER ENTER ENTER PAGEUP ENTER"),
    # 物品子菜单：使用／交给／装备／丢弃列表的打开与取消，最后待机。
    "item": ("Z ENTER ENTER RIGHT ENTER ENTER DOWN UP ESC "
             "LEFT ENTER ESC RIGHT ENTER ENTER ESC DOWN ENTER ESC ESC DOWN ENTER"),
    # 逐个单位尝试法术列表：打开、移动、取消；确认进入目标后取消。
    "magic": ("Z Z ENTER ENTER LEFT ENTER DOWN ESC ENTER ENTER ESC ESC ESC "
              "Z ENTER ENTER LEFT ENTER ENTER ESC ESC ESC ESC"),
    # 系统菜单：方向与关闭、选项开关、总览、存档（否／是）、读档（取消／是）、出击。
    # A 在浏览中无映射，用于收起可能等待按键的结果提示，避免后续按键错位。
    "system": ("UP UP UP ENTER RIGHT LEFT DOWN ESC "
               "ENTER RIGHT ENTER ENTER RIGHT ENTER ESC ESC "
               "ENTER ENTER ENTER A "
               "ENTER ENTER LEFT ENTER RIGHT ENTER A "
               "ENTER ENTER LEFT ENTER ENTER A "
               "ENTER ENTER RIGHT ENTER ESC A "
               "ENTER ENTER RIGHT ENTER ENTER A "
               "ENTER LEFT ENTER ENTER^"),
    # 结束回合确认（先选否再确认），敌方回合后回到玩家。
    "end-turn": "UP UP UP ENTER DOWN ENTER RIGHT ENTER A ENTER DOWN ENTER ENTER^",
}


MARKERS = {"!": "field input ready:", "^": "field phase: turn=2 phase=2"}


def scenario_steps(text):
    """返回 [(key, 等待标记或 None)]。"""
    steps = []
    for token in text.split():
        marker = MARKERS.get(token[-1])
        steps.append((token[:-1] if marker else token, marker))
    return steps


def bmp_pixels(path):
    data = Path(path).read_bytes()
    offset = struct.unpack_from("<I", data, 10)[0]
    width, height = struct.unpack_from("<ii", data, 18)
    return width, abs(height), data[offset:]


def pixel_diff(a, b):
    wa, ha, pa = bmp_pixels(a)
    wb, hb, pb = bmp_pixels(b)
    if (wa, ha) != (wb, hb) or len(pa) != len(pb):
        return None
    return sum(1 for i in range(0, len(pa), 3) if pa[i:i + 3] != pb[i:i + 3])


def settle(log, process, quiet=2.0, minimum=0.6, timeout=90):
    """等待日志在 quiet 秒内不再增长；进程退出也视为静止。"""
    start = time.monotonic()
    last_size, last_change = -1, start
    while time.monotonic() - start < timeout:
        size = log.stat().st_size
        now = time.monotonic()
        if size != last_size:
            last_size, last_change = size, now
        elif now - start >= minimum and now - last_change >= quiet:
            return
        if process.poll() is not None:
            return
        time.sleep(0.1)


def wait_marker(log, process, marker, after, timeout=60):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if marker in log.read_bytes()[after:].decode("utf-8", "replace"):
            return
        if process.poll() is not None:
            raise RuntimeError(f"game exited before {marker!r}")
        time.sleep(0.1)
    raise TimeoutError(f"missing {marker!r}")


def run(args, game, label, name, steps):
    work = Path(args.artifacts) / name / label
    work.mkdir(parents=True, exist_ok=True)
    for stale in work.iterdir():
        stale.unlink()
    log = work / "game.log"
    env = os.environ.copy()
    env.update(SDL_VIDEODRIVER="windows", SDL_AUDIODRIVER="dummy",
               FD2SDL_SAVE_PATH=str(work / "FD2.SAV"), FD2SDL_FIELD_RNG_SEED=str(args.seed),
               SDL_WINDOW_ACTIVATE_WHEN_SHOWN="0", SDL_WINDOW_ACTIVATE_WHEN_RAISED="0",
               SDL_VIDEO_MINIMIZE_ON_FOCUS_LOSS="0", SDL_WINDOWS_RAW_KEYBOARD="0")
    result = {"label": label, "captures": [], "exit_code": None, "forced": False,
              "error": None}
    with log.open("wb") as output:
        process = subprocess.Popen([str(game), "--field-play", "0"], cwd=args.root,
                                   env=env, stdout=output, stderr=subprocess.STDOUT)

        def driver(*argv):
            completed = subprocess.run([str(args.driver), argv[0], str(process.pid), *argv[1:]],
                                       capture_output=True, text=True, timeout=60)
            if completed.returncode != 0:
                raise RuntimeError(f"driver {argv} failed: {completed.stderr}")

        try:
            wait_marker(log, process, READY, 0)
            settle(log, process)
            for index, (key, marker) in enumerate(steps):
                offset = log.stat().st_size
                driver(key) if key.startswith("wait") else driver("key", key)
                if marker:
                    wait_marker(log, process, marker, offset)
                settle(log, process)
                if process.poll() is not None:
                    break
                shot = work / f"{index:02d}-{key}.bmp"
                driver("capture", str(shot))
                result["captures"].append(shot.name)
        except Exception as error:  # 记录后继续清理，由比较阶段判为失败
            result["error"] = f"{type(error).__name__}: {error}"
        finally:
            # 只清理本轮 PID：先正常关闭，超时才强制结束。
            if process.poll() is None:
                try:
                    driver("close")
                except Exception:
                    pass
                try:
                    process.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    result["forced"] = True
                    process.kill()
                    process.wait(timeout=5)
            result["exit_code"] = process.returncode
    save = work / "FD2.SAV"
    result["save"] = save.read_bytes().hex() if save.exists() else None
    result["trace"] = log.read_text("utf-8", "replace").splitlines()
    return work, result


def compare(name, base_dir, base, other_dir, other):
    report = {"trace_equal": base["trace"] == other["trace"],
              "save_equal": base["save"] == other["save"],
              "captures_equal_count": base["captures"] == other["captures"],
              "pixel_diffs": {}}
    if not report["trace_equal"]:
        for i, (a, b) in enumerate(zip(base["trace"], other["trace"])):
            if a != b:
                report["first_trace_diff"] = {"line": i, "base": a, "other": b}
                break
        else:
            report["first_trace_diff"] = {"line": min(len(base["trace"]), len(other["trace"])),
                                          "base_len": len(base["trace"]),
                                          "other_len": len(other["trace"])}
    for shot in base["captures"]:
        if shot in other["captures"]:
            report["pixel_diffs"][shot] = pixel_diff(base_dir / shot, other_dir / shot)
    return report


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True)
    parser.add_argument("--old-game", required=True)
    parser.add_argument("--new-game", required=True)
    parser.add_argument("--driver", required=True)
    parser.add_argument("--artifacts", required=True)
    parser.add_argument("--seed", default="0x7a18")
    parser.add_argument("--scenario", action="append", choices=sorted(SCENARIOS))
    parser.add_argument("--explore", action="store_true",
                        help="只运行旧版一次并打印逻辑轨迹，用于编写场景")
    args = parser.parse_args()
    for key in ("root", "old_game", "new_game", "driver", "artifacts"):
        setattr(args, key, Path(getattr(args, key)).resolve())
    names = args.scenario or list(SCENARIOS)
    summary, ok = {}, True
    if args.explore:
        for name in names:
            work, result = run(args, args.old_game, "explore", name, scenario_steps(SCENARIOS[name]))
            print(f"== {name}: {work} exit={result['exit_code']} captures={len(result['captures'])}")
            for line in result["trace"]:
                if line.startswith("field ") and not line.startswith("field result query"):
                    print("  " + line)
        return 0
    for name in names:
        old1_dir, old1 = run(args, args.old_game, "old1", name, scenario_steps(SCENARIOS[name]))
        old2_dir, old2 = run(args, args.old_game, "old2", name, scenario_steps(SCENARIOS[name]))
        new_dir, new = run(args, args.new_game, "new", name, scenario_steps(SCENARIOS[name]))
        noise = compare(name, old1_dir, old1, old2_dir, old2)
        delta = compare(name, old1_dir, old1, new_dir, new)
        clean_exit = all(r["exit_code"] == 0 and not r["forced"] and not r["error"]
                         for r in (old1, old2, new))
        passed = delta["trace_equal"] and delta["save_equal"] and clean_exit
        ok = ok and passed
        summary[name] = {"passed": passed, "clean_exit": clean_exit,
                         "errors": [r["error"] for r in (old1, old2, new) if r["error"]],
                         "trace_lines": len(old1["trace"]), "old_vs_old": noise,
                         "old_vs_new": delta}
        print(json.dumps({name: {"passed": passed, "trace_lines": len(old1["trace"]),
                                 "old_repeatable": noise["trace_equal"] and noise["save_equal"],
                                 "trace_equal": delta["trace_equal"],
                                 "save_equal": delta["save_equal"],
                                 "clean_exit": clean_exit}}, ensure_ascii=False), flush=True)
    out = Path(args.artifacts) / "summary.json"
    out.write_text(json.dumps(summary, ensure_ascii=False, indent=1), encoding="utf-8")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
