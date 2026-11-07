"""确认框最低过渡门：所有原帧查亮色占位，同操作配完整面板；不证明逐帧/时序相同。"""
from __future__ import annotations

import hashlib
from pathlib import Path

import numpy as np
from PIL import Image

from run_original_comparison import normalize, TOLERANCE

PANEL = (5, 112, 315, 198)
OPERATIONS = ("escape-open", "escape-cancel", "no-open", "no-cancel")
FLASH_FRACTION = 0.8


def _require(ok, message):
    if not ok:
        raise ValueError(message)


def _series(row, assets):
    from run_original_battle_menu import Oracle
    oracle = Oracle(assets, (0, 0))
    color = np.asarray(list(assets["palette6"][0xcd * 3:0xcd * 3 + 3]), dtype=np.int16) << 2
    _require(color.shape == (3,), "确认框缺原版六位palette")
    identity = (row["role"], row["operation"])
    ready, summary = row.get("ready", {}), row.get("summary", {})
    _require(row.get("status") == "collected" and row.get("cleaned") is True and
             row.get("forced_cleanup") is False and row.get("exit_code") == 0,
             "确认框采集/正常清理未完成")
    job = row.get("job", {})
    _require(job.get("cleaned") == 1 and all(job.get(k) == 0 for k in
             ("worker_exit", "timed_out", "residual", "forced", "result")), "确认框helper Job失败")
    _require(ready.get("ready") is True and ready.get("pid") == row.get("pid") and
             ready.get("hwnd") and summary.get("ok") is True and
             summary.get("pid") == row["pid"] and summary.get("hwnd") == ready["hwnd"] and
             summary.get("backend") == "WGC-HWND-session" and summary.get("session_started") is True and
             summary.get("ready_published") is True and summary.get("stop_reason") == "stop" and summary.get("hresult") == 0 and
             summary.get("color_filter") is False, "确认框WGC身份/显式stop失败")
    frames = row.get("frames", [])
    _require(2 <= len(frames) <= 600 and len(frames) == summary.get("frames"), "确认框series为空/缺帧")
    checks, pictures, candidates = [], [], []
    previous = None
    for index, frame in enumerate(frames):
        stamp = frame.get("wgc_timestamp_100ns")
        _require(frame.get("index") == index and type(stamp) is int and
                 (previous is None or stamp > previous) and frame.get("pid") == row["pid"] and
                 frame.get("hwnd") == ready["hwnd"], "确认框帧序/owner不符")
        path = Path(frame["path"])
        _require(hashlib.sha256(path.read_bytes()).hexdigest() == frame.get("sha256"), "确认框PNG源SHA变化")
        with Image.open(path) as raw:
            _require(raw.format == "PNG" and raw.size == (frame["client_width"], frame["client_height"]),
                     "确认框原PNG/client尺寸不符")
            image = normalize(raw)
        panel = np.asarray(image.crop(PANEL), dtype=np.int16)
        matching = np.max(np.abs(panel - color), axis=2) <= TOLERANCE
        count = int(np.count_nonzero(matching))
        fraction = count / (310 * 86)
        confirmation = oracle.buttons(image)
        if confirmation:
            candidates.append(index)
        checks.append({"index": index, "path": str(path), "sha256": frame["sha256"],
                       "cd_pixels": count, "cd_fraction": fraction,
                       "known_flash": fraction > FLASH_FRACTION,
                       "complete_yes_no_resource": confirmation})
        pictures.append(panel)
        previous = stamp
    _require(ready.get("first_index") == 0 and ready.get("wgc_timestamp_100ns") == frames[0]["wgc_timestamp_100ns"] and
             summary.get("first_wgc_timestamp_100ns") == frames[0]["wgc_timestamp_100ns"] and
             summary.get("last_wgc_timestamp_100ns") == previous, "确认框series首末边界不符")
    return ({"role": identity[0], "operation": identity[1], "frames": checks,
             "known_flash_absent": not any(check["known_flash"] for check in checks),
             "complete_panel_candidates": candidates}, pictures, candidates)


def check_series(row, assets):
    """采集关闭后立即检查全帧；首端已知坏帧不等到SDL再报错。"""
    return _series(row, assets)[0]


def verify(records, assets, source_hashes, *, operations=OPERATIONS):
    """原始PNG/SHA逐项校验；全部帧都查签名，只有完整YES/NO页能参与整板配对。

    operations仅供复核旧的两操作诊断；永久runner使用默认四操作八series。
    """
    _require(bool(source_hashes), "确认框资源源SHA清单为空")
    for name, expected in source_hashes.items():
        _require(hashlib.sha256(Path(name).read_bytes()).hexdigest() == expected,
                 "确认框资源源SHA变化")
    expected_keys = {(role, operation) for role in ("original", "sdl") for operation in operations}
    keys = [(row.get("role"), row.get("operation")) for row in records]
    _require(len(keys) == len(set(keys)) and set(keys) == expected_keys, "确认框series缺失/重复")
    series, panels, eligible = [], {}, {}
    for row in records:
        detail, pictures, candidates = _series(row, assets)
        identity = (row["role"], row["operation"])
        series.append(detail)
        panels[identity], eligible[identity] = pictures, candidates
    pairs = []
    for operation in operations:
        found, closest = None, None
        for original in eligible[("original", operation)]:
            for sdl in eligible[("sdl", operation)]:
                delta = np.max(np.abs(panels[("original", operation)][original] -
                                      panels[("sdl", operation)][sdl]), axis=2)
                bad, maximum = int(np.count_nonzero(delta > TOLERANCE)), int(delta.max())
                candidate = {"original_index": original, "sdl_index": sdl,
                             "checked_pixels": 26660, "bad_pixels": bad, "max_channel_delta": maximum}
                if closest is None or bad < closest["bad_pixels"]:
                    closest = candidate
                if bad == 0:
                    found = candidate
                    break
            if found:
                break
        pairs.append({"operation": operation, "ok": found is not None, "pair": found, "closest": closest})
    flashes_absent = all(row["known_flash_absent"] for row in series)
    return {"ok": flashes_absent and all(row["ok"] for row in pairs),
            "scope": "all collected frames known-CD-flash rejection; same-operation complete-YES/NO full-panel minimum pair",
            "panel_roi": list(PANEL), "channel_tolerance": TOLERANCE,
            "flash_fraction_exclusive_limit": FLASH_FRACTION,
            "all_raw_frames_checked": True, "known_flash_absent": flashes_absent,
            "series": series, "paired_operations": pairs, "source_hashes": source_hashes,
            "all_transition_frames_equal": False, "transition_timing_equal": False}
