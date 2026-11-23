"""Exercise the setup window and the resulting game using owned HWND input and WGC."""
import configparser
import hashlib
import json
import os
from pathlib import Path
import sys
import tempfile
import time

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from run_original_comparison import Capture, driver_call, running


def run(game, driver, artifacts):
    if artifacts.resolve().is_relative_to((ROOT / "original_game").resolve()):
        raise ValueError("artifact path inside original resources")
    artifacts.mkdir(parents=True, exist_ok=True)
    work = Path(tempfile.mkdtemp(prefix="launcher-", dir=artifacts))
    config = work / "启动设置.cfg"
    env = dict(os.environ, SDL_AUDIODRIVER="dummy", FD2SDL_SAVE_PATH=str(work / "FD2.SAV"))
    report = {"processes": [], "checkpoints": [], "exe_sha256": hashlib.sha256(game.read_bytes()).hexdigest()}
    font = {}
    for line in (ROOT / "src/third_party/setup-glyphs.hex").read_text().splitlines():
        cp, data = line.split(":")
        stride = len(data) // 16
        font[chr(int(cp, 16))] = (stride * 4, [int(data[i:i+stride], 16) for i in range(0, len(data), stride)])

    def frame(p, name, size=(800, 540)):
        target = work / (name + ".bmp")
        # WGC starts after the owned window is visible. Retain the first valid checkpoint.
        deadline = time.monotonic() + 8
        while True:
            try:
                driver_call(driver, "capture", p, target)
                break
            except RuntimeError as error:
                if time.monotonic() >= deadline or not any(x in str(error) for x in
                        ("no visible window", "blank or nearly uniform", "geometry changed")):
                    raise
                time.sleep(0.15)
        with Image.open(target) as source:
            image = source.convert("RGB")
        assert image.size == size, (name, image.size)
        assert len(image.getcolors(image.width * image.height)) > 5, name
        report["checkpoints"].append({"frame": target.name, "size": size})
        return image

    def glyphs(image, x, y, label):
        for char in label:
            width, rows = font[char]
            for j, row in enumerate(rows):
                for i in range(width):
                    bright = min(image.getpixel((x+i, y+j))) > 96
                    assert bright == bool(row & (1 << (width-1-i))), (label, char, i, j)
            x += width
        report["checkpoints"][-1].setdefault("labels", []).append(label)

    def launch(name, args, settings, locale="zh_CN"):
        return running([str(game), *args], ROOT, dict(env, SDL_PREFERRED_LOCALES=locale),
                       work / (name + ".log"), driver,
                       report["processes"], settings=settings)

    try:
        with launch("first", [], config) as p:
            controls = Capture(driver, p, work)
            image = frame(p, "paths")
            glyphs(image, 206, 93, "路径")
            glyphs(image, 206, 380, "资源齐全")
            glyphs(image, 465, 27, "原版一致")
            glyphs(image, 700, 26, "繁體中文")
            for page, title in [("video", "画面"), ("sound", "声音"), ("keyboard", "键盘"),
                                ("gamepad", "手柄"), ("pacing", "节奏与规则")]:
                controls.key("PAGEDOWN")
                image = frame(p, page)
                glyphs(image, 206, 93, title)
                if page == "video":
                    controls.key("ENTER")
                    controls.key("LEFT")
                    glyphs(frame(p, "scale"), 464, 135, "2")
                if page == "keyboard":
                    for _ in range(6):
                        controls.key("DOWN", fast=True)
                    controls.key("ENTER")
                    controls.key("A")
                if page == "pacing":
                    glyphs(image, 214, 135, "升级属性成长")
                    glyphs(image, 464, 135, "原版随机")
                    controls.key("RIGHT")
                    glyphs(frame(p, "growth-max"), 464, 135, "最大成长")
                    controls.key("RIGHT")
                    image = frame(p, "growth-max-minus-one")
                    glyphs(image, 464, 135, "最大成长-1")
                    glyphs(image, 465, 27, "非原版设置")
                    controls.key("RIGHT")
                    glyphs(frame(p, "growth-min"), 464, 135, "最小成长")
                    controls.key("LEFT")
                    controls.key("DOWN")
                    controls.key("RIGHT")
            controls.key("TAB")
            controls.key("RIGHT")
            controls.key("ENTER")
            assert p.wait(timeout=5) == 0
        saved = config.read_bytes()
        values = configparser.ConfigParser(interpolation=None, strict=False)
        values.read_string(saved.decode("utf-8"))
        assert values["video"]["window_scale"] == "2"
        assert values["pacing"]["key_repeat_delay_ms"] == "501"
        assert values["rules"]["level_up_growth"] == "max_minus_one"
        assert "A" in values["keyboard"]["Enter"].split(", ")
        with launch("discard", ["--setup"], config, locale="en_US,zh_CN") as p:
            controls = Capture(driver, p, work)
            image = frame(p, "reopened")
            glyphs(image, 206, 93, "路徑")
            glyphs(image, 465, 27, "非原版設定")
            glyphs(image, 700, 26, "简体中文")
            controls.key("PAGEUP")
            image = frame(p, "traditional-growth")
            glyphs(image, 206, 93, "節奏與規則")
            glyphs(image, 214, 135, "升級屬性成長")
            glyphs(image, 464, 135, "最大成長-1")
            controls.key("F2")
            glyphs(frame(p, "switched-growth"), 464, 135, "最大成长-1")
            controls.key("ENTER")
            controls.key("RIGHT")
            controls.key("ESC")
            assert p.wait(timeout=5) == 0
        assert config.read_bytes() == saved
        missing = work / "discarded.cfg"
        with launch("cancel-first", [], missing) as p:
            frame(p, "cancel-first")
            Capture(driver, p, work).key("ESC")
            assert p.wait(timeout=5) == 0
        assert not missing.exists()
        with launch("start", ["--setup", "--map-preview", "0"], config) as p:
            controls = Capture(driver, p, work)
            glyphs(frame(p, "before-start"), 206, 93, "路径")
            controls.key("TAB")
            controls.key("TAB")
            controls.key("ENTER")
            image = frame(p, "game", (640, 400))
            assert len(image.getcolors(640*400)) >= 32
            controls.key("A")
            assert p.wait(timeout=5) == 0
        always = work / "always.cfg"
        always.write_text("[launcher]\nshow_on_start=on\n", encoding="utf-8")
        with launch("show-on-start", [], always) as p:
            glyphs(frame(p, "show-on-start"), 206, 93, "路径")
            Capture(driver, p, work).key("ESC")
            assert p.wait(timeout=5) == 0
        with launch("no-config", [], None) as p:
            frame(p, "default-game", (960, 600))
        assert "config=none parity=original" in (work / "no-config.log").read_text(encoding="utf-8")
        report["passed"] = True
    finally:
        (work / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        print(work)


if __name__ == "__main__":
    run(*(Path(arg).resolve() for arg in sys.argv[1:]))
