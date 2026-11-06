#!/usr/bin/env python3
"""B5受控掉落／特殊格：原版先行、可选SDL配对，只用真实输入和HWND WGC。

1aa1d/190ac/12263的确定性业务由合法UI Save测量。无--sdl仅取证，ok仍false；
未比较完整演出、嘴型／共享RNG时序、声音或自然整关。宿主run-job约束本轮进程树。
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import sys
import time

from PIL import Image

from fd2_loot_oracle import (SCENARIOS, checksum, derive_fixture, expected_state,
    message_pages, message_match, story_pages)
from fd2_item_list_oracle import item_table, inventory_templates
from fd2_movement_oracle import movement_profile
from fd2_opening_oracle import page_match as story_match
from run_graphics_tests import decode_save
from run_original_comparison import Capture, archive_entry, hashes, running, title_templates
from run_original_battle_menu import MENU_OFFSETS, match_indexed, wait_state, wait_title
from run_original_end_turn import META, UNITS, STRIDE, RoundOracle, round_assets
from run_original_focus import native_focus_reference
from run_original_item_list import (inventory_detail, measure_inventory_save,
    paired_checkpoint_records, post_use_navigation_resources, post_use_navigation_state,
    rgb_difference, use_range_probe)
from run_original_load import require_source_unchanged, check_load_measurement
from run_original_magic_list import actor_focus_resources, detail_close_terrain
from run_original_player import exact_tile, screen, sheet_frame, PlayerOracle
from run_original_result import selection_probe
from run_original_reward import actor_view, endpoint_guard


def positioned(source, focus, camera=None):
    result = bytearray(source)
    if camera is not None: result[META + 3:META + 5] = bytes(camera)
    result[META + 5:META + 9] = bytes((*focus, focus[0] - result[META + 3], focus[1] - result[META + 4]))
    return checksum(result)


def focus_view(source, start, target):
    reference = native_focus_reference(tuple(source[META + 3:META + 5]), start, target)
    return positioned(source, target, reference["camera"]), reference


def one_step_probe(original, source, assets, raw=0):
    """一步可达格；财宝/WAIT边缘也验证完整24×24，无水色或边界裁切mask。"""
    other = (original / "FDOTHER.DAT").read_bytes()
    data = archive_entry((original / "FDFIELD.DAT").read_bytes(), 0)
    shape = (original / "FDSHAP.DAT").read_bytes()
    tiles, attrs = archive_entry(shape, source[0] * 2), archive_entry(shape, source[0] * 2 + 1)
    actor = source[UNITS + raw * STRIDE:UNITS + (raw + 1) * STRIDE]
    profile, _ = movement_profile(original / "FD2.EXE", actor[0x20])
    hero = tuple(actor[:2])
    visible = [source[UNITS + i * STRIDE:UNITS + (i + 1) * STRIDE] for i in range(source[META + 1]) if not source[UNITS + i * STRIDE + 5] & 1]
    for dx, dy in ((0, -1), (-1, 0), (1, 0), (0, 1)):
        world = (hero[0] + dx, hero[1] + dy)
        if not all(0 <= v < 24 for v in world): continue
        at = screen(source, world)
        if not 0 <= at[0] <= 296 or not 0 <= at[1] <= 176: continue
        if any(tuple(actor[:2]) in (world, (world[0], world[1] + 1)) for actor in visible): continue
        if any(actor[6] == 0 and min(abs(actor[0] - hero[0]) + abs(actor[1] - hero[1]), abs(actor[0] - world[0]) + abs(actor[1] - world[1])) <= 1 for actor in visible): continue
        cell = struct.unpack_from("<I", data, 4 + (world[1] * 24 + world[0]) * 4)[0]
        tile = cell & 1023
        if cell >> 10 or tile * 4 + 4 > len(attrs) or attrs[tile * 4] or not 0 < profile[attrs[tile * 4 + 1]] <= actor[0x3b]: continue
        base = sheet_frame(tiles, tile); base.putpalette(assets["palette"])
        if 0 in base.getdata() or any(224 <= p < 240 for p in base.getdata()): continue
        sheet = archive_entry(other, 3)
        if sheet[:4] != b"LMI1": raise ValueError("边缘selection LUT目录非法")
        count, ranges = struct.unpack_from("<H", sheet, 4)[0], []
        for phase in range(count):
            begin = struct.unpack_from("<I", sheet, 6 + phase * 4)[0]
            end = struct.unpack_from("<I", sheet, 10 + phase * 4)[0] if phase + 1 < count else len(sheet)
            lut = sheet[begin:end]
            if len(lut) != 256: raise ValueError("边缘selection LUT帧不完整")
            image = Image.frombytes("P", (24, 24), bytes(lut[p] for p in base.getdata())); image.putpalette(assets["palette"])
            if image.tobytes() != base.tobytes(): ranges.append((phase, image))
        if ranges: return {"world": world, "position": screen(source, world), "actor_position": screen(source, hero), "base": base, "ranges": ranges}
    raise ValueError("本actor没有可独立完整资源确认的一步selection格")


def prepare(original, scenario):
    encoded, source, manifest = derive_fixture(original, scenario)
    assets = round_assets(original)
    items, table = item_table(original / "FD2.EXE")
    model = expected_state(source, scenario, items)
    waiter = model["final"][UNITS + STRIDE:UNITS + 2 * STRIDE]
    hp, maximum = struct.unpack_from("<HH", waiter, 0x40)
    if (waiter[7] != 9 or waiter[6] != 2 or waiter[5] & 0x85 or
            any(waiter[0x22:0x28]) or not hp or hp != maximum or
            waiter[0x1a:0x1f] != b"\x01\0\0\0\0"):
        raise ValueError("B5 Load前WAIT必须使用健康、未行动、无状态的真实raw1 unit9")
    initial = tuple(source[META + 5:META + 7])
    hero = tuple(source[UNITS:UNITS + 2])
    focused, reference = focus_view(source, initial, hero)
    view = actor_view(original, focused, assets, 0)
    actor = actor_focus_resources(original, focused, assets, 0, assets["cursor"])
    physical = scenario.startswith("loot-")
    if physical:
        movement = use_range_probe(original, focused, assets, 1, 0, hero)
        movement["actor_position"] = screen(focused, hero)
        target_view, _ = focus_view(focused, hero, (8, 14))
        target = actor_focus_resources(original, target_view, assets, 11, assets["cursor"])
        expected = positioned(model["immediate"], (8, 14), target_view[META + 3:META + 5])
    else:
        movement = selection_probe(original, focused, 0, assets) if hero[0] != 0 else one_step_probe(original, focused, assets)
        movement["actor_position"] = screen(focused, hero)
        target_view, target, expected = None, None, positioned(model["immediate"], hero, focused[META + 3:META + 5])
    instructions = []
    if scenario.startswith("chest-") or scenario == "treasure-money":
        instructions += [{"fragment": 428 if scenario == "treasure-money" else 421, "open": True, "choice": True}]
        if scenario != "chest-no":
            instructions += [{"fragment": 430 if scenario == "treasure-money" else 422,
                "y": 146, "key": True, "item": 192, "value": 5000}]
        if scenario in ("chest-swap", "chest-list-cancel", "chest-full-no"):
            instructions += [{"fragment": 423, "open": True, "choice": True}]
        if scenario == "chest-swap":
            instructions += [{"fragment": 425, "open": True, "key": True, "item": 192, "old": 0}]
    elif scenario in ("loot-item", "loot-money"):
        instructions += [{"fragment": 432 if scenario == "loot-item" else 435,
            "open": True, "key": True, "item": 192, "value": 1000}]
    pages = message_pages(original, assets, instructions)
    source_actor = source[UNITS:UNITS + STRIDE]
    _, source_templates, source_fields = inventory_templates(original, source_actor, assets)
    _, final_templates, final_fields = inventory_templates(original, expected[UNITS:UNITS + STRIDE], assets)
    final_fields["require_word_bars"] = True
    stories = story_pages(original, source, assets, 7 if scenario == "loot-callback" else 8) if scenario in ("loot-callback", "loot-story") else []
    for page in stories:
        matches = [raw for raw in range(source[META + 1]) if source[UNITS + raw * STRIDE + 8] == page["speaker_argument"] and not expected[UNITS + raw * STRIDE + 5] & 1]
        if matches:
            world = tuple(expected[UNITS + matches[0] * STRIDE:UNITS + matches[0] * STRIDE + 2])
            expected, _ = focus_view(expected, tuple(expected[META + 5:META + 7]), world)
    return {"original": original, "scenario": scenario, "physical": physical,
        "encoded": encoded, "source": source, "manifest": manifest, "assets": assets,
        "model": model, "table": table, "initial": initial, "hero": hero,
        "focused": focused, "focus_reference": reference, "view": view, "actor": actor,
        "movement": movement, "target_view": target_view, "target": target,
        "immediate": expected, "pages": pages, "stories": stories,
        "source_templates": source_templates, "source_fields": source_fields,
        "final_templates": final_templates, "final_fields": final_fields,
        "titles": title_templates(original / "FDOTHER.DAT")}


def command(frame, prepared, selected):
    """边缘财宝x0不猜左图越界写法；其余三完整资源仍唯一证明COMMAND。"""
    at, assets = screen(prepared["focused"], prepared["hero"]), prepared["view"]
    matches, excluded = [], []
    for item in range(4):
        position = tuple(a + b for a, b in zip(at, MENU_OFFSETS[item]))
        if position[0] < 0 or position[1] < 0 or position[0] + 24 > 320 or position[1] + 20 > 200:
            excluded.append(item); continue
        states = (1,) if item == selected else (0, 2)
        found = None
        for state in states:
            index = item * 3 + state
            images = assets["upper_commands"][index] if item == 0 else [assets["commands"][index]]
            for image in images:
                hit = match_indexed(frame, image, position)
                if hit["ok"]:
                    found = {"frame": index, "position": list(position), "size": list(image.size), **hit}; break
            if found: break
        if not found: return None
        matches.append(found)
    if len(matches) < 3: return None
    return {"kind": "loot_command", "selected": selected, "matches": matches,
            "out_of_view_icons_not_compared": excluded, "full_command_pixels_claimed": not excluded,
            "unselected_capability_gates_compared": False}


def focus_match(frame, prepared, state, raw=0):
    assets = prepared["assets"]
    world = tuple(state[UNITS + raw * STRIDE:UNITS + raw * STRIDE + 2])
    at = screen(state, world)
    actor = actor_focus_resources(prepared["original"], state, assets, raw, assets["cursor"])
    found = next((dict(phase=i, **hit) for i, image in enumerate(actor["templates"])
                  if (hit := match_indexed(frame, image, (at[0], at[1] - 6)))["ok"]), None)
    if found is None or match_indexed(frame, assets["dialog_top"], (5, 112))["ok"]: return None
    # 右/下按钮在x0也全可见；排除普通父/COMMAND/secondary菜单。
    for offset, ids in (((24, 2), (6, 7, 8, 18, 19, 42, 43)), ((0, 24), (9, 10, 11, 12, 13, 45, 46))):
        pos = (at[0] + offset[0], at[1] + offset[1])
        for index in ids:
            if 0 <= pos[0] <= 296 and 0 <= pos[1] <= 180 and match_indexed(frame, assets["commands"][index], pos)["ok"]:
                return None
    return {"kind": "loot_closed_actor", "raw_actor": raw, "focus_screen": list(at),
            "world": list(world), "camera": list(state[META + 3:META + 5]), "actor": found}


def browse_match(frame, prepared, state):
    world = tuple(state[META + 5:META + 7])
    raw = next((i for i in range(state[META + 1]) if not state[UNITS + i * STRIDE + 5] & 1 and tuple(state[UNITS + i * STRIDE:UNITS + i * STRIDE + 2]) == world), None)
    if raw is not None: return focus_match(frame, prepared, state, raw)
    resources = post_use_navigation_resources(prepared["original"], state, prepared["assets"], world)
    return post_use_navigation_state(frame, state, prepared["assets"], resources)


def selection_match(frame, prepared, state, raw=0):
    found = focus_match(frame, prepared, state, raw)
    probe = prepared["movement"]
    translated = next((dict(phase=p, **hit) for p, image in probe["ranges"]
        if (hit := exact_tile(frame, image, probe["position"]))), None)
    return dict(found, range=translated) if found and translated else None


def quiet_focus(prepared, state, report, name):
    receipt = {"host_quiet_seconds": 2.0, "native_timing_claimed": False, "samples": 0,
               "resets": 0, "first": None, "retries": 0}
    report[name + "_quiet"] = receipt
    def predicate(frame):
        retries = sum(row.get("kind") == "wgc_transient" and row["name"].startswith(name + "-wgc-retry-") for row in report["checkpoints"])
        found = browse_match(frame, prepared, state)
        if found is None or retries != receipt["retries"]:
            receipt.update(first=None, samples=0, resets=receipt["resets"] + 1, retries=retries)
            if found is None: return None
        now = time.monotonic()
        if receipt["first"] is None: receipt["first"] = now
        receipt["samples"] += 1
        return dict(found, resource_quiet=dict(receipt)) if now - receipt["first"] >= 2 and receipt["samples"] >= 2 else None
    return predicate


def navigate(capture, prepared, state, start, end, checkpoint, prefix):
    reference = native_focus_reference(tuple(state[META + 3:META + 5]), start, end)
    previous = start
    for i, step in enumerate(reference["steps"], 1):
        world = tuple(step["world"])
        key = "RIGHT" if world[0] > previous[0] else "LEFT" if world[0] < previous[0] else "DOWN" if world[1] > previous[1] else "UP"
        state = positioned(state, world, step["camera"])
        resources = post_use_navigation_resources(prepared["original"], state, prepared["assets"], world)
        capture.key(key)
        checkpoint(f"{prefix}-{i}", lambda frame, s=state, r=resources: post_use_navigation_state(frame, s, prepared["assets"], r))
        previous = world
    return state


def save_empty_step(prepared, state):
    """从实际量测前状态选一邻格；全静态干燥tile、无actor遮挡且不卷屏。"""
    if len(state) != 0x59cb:
        raise ValueError("Save空格前置需要完整stage0活动快照")
    count = state[META + 1]
    if not 0 < count <= 64 or state[META + 2] != 0:
        raise ValueError("Save空格前置需要完整stage0活动快照")
    anchor, camera = tuple(state[META + 5:META + 7]), tuple(state[META + 3:META + 5])
    occupied = {tuple(state[UNITS + raw * STRIDE:UNITS + raw * STRIDE + 2])
                for raw in range(count) if not state[UNITS + raw * STRIDE + 5] & 1}
    if anchor in occupied:
        raise ValueError("Save量测起点被活动actor占据")
    field = archive_entry((prepared["original"] / "FDFIELD.DAT").read_bytes(), 0)
    shape = (prepared["original"] / "FDSHAP.DAT").read_bytes()
    tiles, attrs = archive_entry(shape, state[0] * 2), archive_entry(shape, state[0] * 2 + 1)
    rejected = []
    for key, dx, dy in (("RIGHT", 1, 0), ("LEFT", -1, 0), ("UP", 0, -1), ("DOWN", 0, 1)):
        world = (anchor[0] + dx, anchor[1] + dy)
        reason = None
        tile = None
        at = screen(state, world)
        if not all(0 <= value < 24 for value in world): reason = "outside_map"
        elif world in occupied or (world[0], world[1] + 1) in occupied: reason = "actor_or_lower_actor_overlap"
        elif not 28 <= at[0] <= 268 or not 24 <= at[1] <= 152: reason = "menu_or_tile_outside_safe_view"
        else:
            after, _ = focus_view(state, anchor, world)
            if tuple(after[META + 3:META + 5]) != camera: reason = "camera_would_scroll"
            else:
                cell = struct.unpack_from("<I", field, 4 + (world[1] * 24 + world[0]) * 4)[0]
                tile = cell & 1023
                if cell >> 10 or tile * 4 + 4 > len(attrs) or attrs[tile * 4]: reason = "nonstatic_or_special_cell"
                else:
                    image = sheet_frame(tiles, tile)
                    if image.size != (24, 24) or any(color == 0 or 224 <= color < 240 for color in image.getdata()):
                        reason = "transparent_or_water_tile"
        if reason is None:
            return {"key": key, "world": list(world), "position": list(at), "anchor": list(anchor),
                    "camera": list(camera), "tile": tile, "checked_terrain_pixels": 576,
                    "static_dry_unoccupied_same_camera": True, "rejected_candidates": rejected}
        rejected.append({"key": key, "world": list(world), "reason": reason})
    raise ValueError("Save量测无合法静态干燥空格：" + json.dumps(rejected, ensure_ascii=False))


def exercise(capture, prepared, save, guard, report):
    source, assets = prepared["source"], prepared["assets"]
    def checkpoint(name, predicate, *, source_guard=True, **kwargs):
        result = wait_state(capture, name, predicate, report["checkpoints"], **kwargs)
        if source_guard: require_source_unchanged(save, *guard)
        return result
    wait_title(capture, prepared["titles"][3][0])
    capture.key("UP"); capture.wait(prepared["titles"][3][2]); capture.key("ENTER")
    checkpoint("loot-continued", RoundOracle(assets, screen(source, prepared["initial"])).initial_browse, timeout=45)
    capture.key("Z"); checkpoint("loot-focus-hero", lambda frame: focus_match(frame, prepared, prepared["focused"]))
    capture.key("ENTER")
    checkpoint("loot-selection", lambda frame: selection_match(frame, prepared, prepared["focused"]))
    capture.key("ENTER")
    checkpoint("loot-command", lambda frame: command(frame, prepared, 0 if prepared["physical"] else 2))
    if prepared["physical"]:
        capture.key("ENTER")
        checkpoint("loot-physical-target", lambda frame: selection_match(frame, prepared, prepared["target_view"], 11))
        capture.key("ENTER")
        report["physical_confirmations"] = 1
    else:
        capture.key("DOWN"); checkpoint("loot-wait-selected", lambda frame: command(frame, prepared, 3))
        capture.key("ENTER"); report["wait_confirmations"] = 1
    scenario = prepared["scenario"]
    for ordinal, page in enumerate(prepared["pages"]):
        name = f"loot-message-{ordinal:02d}-{page['fragment']}"
        checkpoint(name, lambda frame, p=page: message_match(frame, p, assets), timeout=90)
        if page.get("choice"):
            no = scenario == "chest-no" or scenario == "chest-full-no" and page["fragment"] == 423
            if no:
                capture.key("RIGHT"); checkpoint(name + "-no", lambda frame, p=page: message_match(frame, p, assets, 1))
            capture.key("ENTER")
            if page["fragment"] == 423 and not no:
                at, image = prepared["source_templates"]["selected_0"]
                checkpoint("loot-replace-inventory0", lambda frame: {"kind": "loot_inventory", "selected": 0, "position": list(at), "size": list(image.size), **hit} if (hit := exact_tile(frame, image, at)) else None)
                capture.key("ESC" if scenario == "chest-list-cancel" else "ENTER")
        elif page.get("key"):
            capture.key("ENTER")
    for ordinal, page in enumerate(prepared["stories"]):
        checkpoint(f"loot-story-{ordinal:02d}-{page['fragment']}", lambda frame, p=page: story_match(frame, p, assets), timeout=90)
        capture.key("ENTER")
    state = prepared["immediate"]
    checkpoint("loot-action-complete", quiet_focus(prepared, state, report, "loot-action-complete"), timeout=120)
    state = navigate(capture, prepared, state, tuple(state[META + 5:META + 7]), prepared["hero"], checkpoint, "loot-return-hero")
    terrain = detail_close_terrain(prepared["original"], state, assets)
    capture.key("F2")
    checkpoint("loot-inventory-after-action", lambda frame: inventory_detail(frame, prepared["final_templates"], prepared["final_fields"]))
    capture.key("ESC")
    def closed(frame):
        found = focus_match(frame, prepared, state)
        restored = exact_tile(frame, terrain["image"], terrain["position"])
        return dict(found, full_detail_close_terrain={**{k: v for k, v in terrain.items() if k != "image"}, **restored}) if found and restored else None
    checkpoint("loot-detail-closed", closed)
    # 避开actor与特殊格，逐键资源ACK；最终Save本身只由现有共享测量helper发出。
    start = tuple(state[META + 5:META + 7])
    anchor = (2, 10) if scenario == "treasure-money" else (6, 14)
    state = navigate(capture, prepared, state, start, anchor, checkpoint, "loot-to-end")
    oracle = RoundOracle(assets, screen(state, anchor))
    capture.key("ENTER"); checkpoint("loot-end-parent", lambda frame: oracle.menu(frame, 0))
    capture.key("DOWN"); checkpoint("loot-end-selected", lambda frame: oracle.menu(frame, 3))
    capture.key("ENTER"); checkpoint("loot-end-default-yes", lambda frame: oracle.buttons(frame, 0))
    capture.key("ENTER")
    final, _ = focus_view(positioned(prepared["model"]["final"], anchor, state[META + 3:META + 5]), anchor, prepared["hero"])
    checkpoint("loot-round2-complete", quiet_focus(prepared, final, report, "loot-round2-complete"), timeout=120)
    final = navigate(capture, prepared, final, prepared["hero"], anchor, checkpoint, "loot-to-save-a")
    save_step = save_empty_step(prepared, final)
    report["save_empty_navigation"] = save_step
    destination = tuple(save_step["world"])
    require_save_enabled(final)
    measure_inventory_save(capture, assets, {"expected_source": final, "steps": [(save_step["key"], tuple(save_step["position"]))]},
        save, *guard, report, checkpoint, prefix="loot-save-a", measurement_key="loot_measurement_a")
    guard[:] = [save.read_bytes(), save.stat().st_mtime_ns]
    saved = decode_save(save)
    waiter_world = tuple(saved[UNITS + STRIDE:UNITS + STRIDE + 2])
    waiting = navigate(capture, prepared, saved, destination, waiter_world, checkpoint, "loot-to-wait-raw1")
    probe = one_step_probe(prepared["original"], waiting, assets, 1)
    wait_view = actor_view(prepared["original"], waiting, assets, 1)
    command_oracle = PlayerOracle(waiting, wait_view, [], {})
    capture.key("ENTER")
    def wait_selection(frame):
        found = focus_match(frame, prepared, waiting, 1)
        translated = next((dict(phase=p, **hit) for p, image in probe["ranges"] if (hit := exact_tile(frame, image, probe["position"]))), None)
        return dict(found, range=translated) if found and translated else None
    checkpoint("loot-wait-raw1-selection", wait_selection)
    capture.key("ENTER")
    checkpoint("loot-wait-raw1-command", lambda frame: command_oracle.command(frame, screen(waiting, waiter_world), 1))
    capture.key("DOWN")
    checkpoint("loot-wait-raw1-selected", lambda frame: command_oracle.command(frame, screen(waiting, waiter_world), 3))
    capture.key("ENTER")
    waiting = bytearray(waiting); waiting[UNITS + STRIDE + 5] |= 0x80; waiting = checksum(waiting)
    checkpoint("loot-wait-raw1-acted", quiet_focus(prepared, waiting, report, "loot-wait-raw1-acted"), timeout=90)
    waiting = navigate(capture, prepared, waiting, waiter_world, anchor, checkpoint, "loot-to-load")
    oracle = RoundOracle(assets, screen(waiting, anchor))
    capture.key("ENTER"); checkpoint("loot-load-parent", lambda frame: oracle.menu(frame, 0))
    capture.key("ENTER"); checkpoint("loot-load-secondary", lambda frame: oracle.secondary(frame, 0, save_available=False))
    capture.key("RIGHT"); checkpoint("loot-load-selected", lambda frame: oracle.secondary(frame, 2, save_available=False))
    capture.key("ENTER"); checkpoint("loot-load-default-yes", lambda frame: oracle.buttons(frame, 0))
    capture.key("ENTER")
    checkpoint("loot-loaded-a", lambda frame: browse_match(frame, prepared, saved), timeout=45)
    restored = navigate(capture, prepared, saved, destination, anchor, checkpoint, "loot-loaded-to-save")
    if save_empty_step(prepared, restored) != save_step:
        raise AssertionError("Load后Save空格前置与第一次量测不一致")
    require_save_enabled(restored)
    measure_inventory_save(capture, assets, {"expected_source": restored, "steps": [(save_step["key"], tuple(save_step["position"]))]},
        save, *guard, report, checkpoint, prefix="loot-save-b", measurement_key="loot_measurement_b")
    guard[:] = [save.read_bytes(), save.stat().st_mtime_ns]
    measured = decode_save(save)
    restoration = compare_saves(saved, measured)
    report["save_end_load"] = {"business_then_end_turn2_save_a_raw1_wait_load_a_save_b": True,
        "real_acted_change_before_load": True, "save_disabled_resource_checked_after_wait": True,
        "load_did_not_write_bytes_or_mtime": True, "a_and_b": restoration,
        "complete_remeasured_container": True, "whole_terrain_render_equivalence_claimed": False}
    report["deterministic_state"] = {key: prepared["model"][key] for key in ("inventory_hex", "completed", "resource", "stat_recompute_claimed")}
    report["automatic_200ms_pages_not_guaranteed_sampled"] = True


def require_save_enabled(state):
    count = state[META + 1]
    if not 0 < count <= 64 or any(not state[UNITS + raw * STRIDE + 5] & 1 and state[UNITS + raw * STRIDE + 5] & 0x80 for raw in range(count)):
        raise AssertionError("19df7：仍有visible acted单位，禁止把Save当normal资源或发保存输入")


def compare_saves(original, sdl):
    """沿既有Load量测合同，只排active前缀每80B+2，不能排坐标/方向或roster。"""
    if len(original) != 0x59cb or len(sdl) != 0x59cb:
        raise AssertionError("B5实际SAV容器长度错误")
    count = original[META + 1]
    if not 0 < count <= 64 or sdl[META + 1] != count:
        raise AssertionError("B5 cache比较active_count越界或两端不同")
    check_load_measurement(original, sdl, [])
    changes = [{"offset": UNITS + raw * STRIDE + 2, "raw_actor": raw,
                "original": original[UNITS + raw * STRIDE + 2], "sdl": sdl[UNITS + raw * STRIDE + 2]}
               for raw in range(count) if original[UNITS + raw * STRIDE + 2] != sdl[UNITS + raw * STRIDE + 2]]
    return {"raw_equal": original == sdl, "mask_equal": True, "cache_class_changes": changes,
        "excluded_byte_per_active_record": 2, "active_count": count,
        "all_other_unknown_roster_metadata_manual_bytes_compared": True}


def compare_endpoints(cases, output, pages):
    if [case["kind"] for case in cases] != ["original", "sdl"] or any(not case["probe_cases_passed"] for case in cases):
        raise AssertionError("B5配对需要两端真实执行且清理／最终守卫通过")
    records = [paired_checkpoint_records(case, output / case["kind"])[0] for case in cases]
    if records[0].keys() != records[1].keys(): raise AssertionError("B5业务检查点集合不一致")
    message_templates = {page["fragment"]: page["text"] for page in pages}
    comparisons = []
    for name, row in records[0].items():
        if row["kind"] != records[1][name]["kind"]:
            raise AssertionError("B5共同检查点类型不同")
        if row["kind"] in ("loot_message", "loot_choice") and row["fragment"] != records[1][name]["fragment"]:
            raise AssertionError("B5共同检查点消息不同")
        frames = [Image.open(output / case["kind"] / (name + ".png")).convert("RGB") for case in cases]
        if row["kind"] in ("loot_message", "loot_choice"):
            # 两端独立完整资源门；只比较实际相同文字mask，不比较水色／嘴型相位。
            template = message_templates[row["fragment"]]
            diff = rgb_difference(*frames, (0, 0), (320, 200), template)
            if not diff["within_tolerance"]: raise AssertionError("B5文字跨端实际RGB不一致")
            comparisons.append({"name": name, **diff})
            if row.get("requires_key"):
                diff = rgb_difference(*frames, (191, 181), (14, 10))
                if not diff["within_tolerance"]: raise AssertionError("B5外部wait0无箭头区域跨端实际RGB不一致")
                comparisons.append({"name": name + ":wait0-no-arrow", **diff})
        elif row["kind"] in ("loot_inventory", "inventory_detail"):
            rectangles = row["rectangles"] if "rectangles" in row else [{"name": "inventory", "position": row["position"], "size": row["size"]}]
            for rectangle in rectangles:
                diff = rgb_difference(*frames, tuple(rectangle["position"]), tuple(rectangle["size"]))
                if not diff["within_tolerance"]: raise AssertionError("B5完整库存／数值RGB跨端不同")
                comparisons.append({"name": name + ":" + rectangle["name"], **diff})
    snapshots = [Path(case["completed_save"]["snapshot"]) for case in cases]
    values = compare_saves(*(decode_save(path) for path in snapshots))
    return {"same_business_checkpoints": len(records[0]), "static_rgb": comparisons,
            "whole_save_bytes_equal": values["raw_equal"], "save_comparison": values,
            "save_sha256": {case["kind"]: hashlib.sha256(path.read_bytes()).hexdigest() for case, path in zip(cases, snapshots)},
            "full_frame_water_palette_and_portrait_rng_aligned": False}


def identity_unchanged(rows):
    return all(Path(row["path"]).is_file() and hashlib.sha256(Path(row["path"]).read_bytes()).hexdigest() == row["sha256"] for row in rows.values())


def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = (root / "original_game").resolve()
    if output == original or original in output.parents: raise ValueError("B5产物禁止写入原版证据目录")
    if args.sdl is not None and not args.sdl.is_file(): raise ValueError("SDL程序不存在；未创建产物")
    prepared = prepare(original, args.scenario)
    before, fixture = hashes(original), original / "START.SAV"
    data, stamp = fixture.read_bytes(), fixture.stat().st_mtime_ns
    if hashlib.sha256(data).hexdigest() != prepared["manifest"]["source_sha256"]:
        raise AssertionError("B5原START在资源准备期间变化；未创建产物或窗口")
    output.mkdir(parents=True, exist_ok=False)
    (output / "controlled.SAV").write_bytes(prepared["encoded"])
    if decode_save(output / "controlled.SAV") != prepared["source"]: raise AssertionError("B5 XOR/checksum往返失败")
    (output / "fixture-manifest.json").write_text(json.dumps(prepared["manifest"], ensure_ascii=False, indent=2), encoding="utf-8")
    report = {"ok": False, "probe_only": args.sdl is None, "probe_cases_passed": False, "status": "failed",
        "scenario": args.scenario, "capture_backend": "Windows.Graphics.Capture HWND", "cases": [],
        "scope": "controlled stage0 loot/special-cell resource ACK; END/turn2/SaveA/raw1WAIT/LoadA/SaveB; original first then SDL",
        "excluded": "natural whole battle, complete FIGANI/audio/shared RNG timing, automatic 200ms text sampling, offscreen command clipping at treasure x0",
        "derived_fixture": prepared["manifest"], "original_before": before,
        "tool_sources": {name: {"path": str(Path(sys.modules[name].__file__).resolve()),
            "sha256": hashlib.sha256(Path(sys.modules[name].__file__).read_bytes()).hexdigest()}
            for name in ("fd2_loot_oracle", "fd2_item_list_oracle", "fd2_reward_oracle", "fd2_opening_oracle", "fd2_magic_list_oracle", "fd2_movement_oracle",
                "run_original_comparison", "run_original_battle_menu", "run_original_end_turn", "run_original_focus", "run_original_item_list", "run_original_load", "run_original_magic_list", "run_original_player", "run_original_result", "run_original_reward", "run_graphics_tests", "rebuild_fd2_analysis")},
        "binaries": {key: {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
            for key, path in (("driver", args.driver), ("dosbox", args.dosbox)) + ((("sdl", args.sdl),) if args.sdl is not None else ())}}
    report["tool_sources"]["run_original_loot"] = {"path": str(Path(__file__).resolve()), "sha256": hashlib.sha256(Path(__file__).read_bytes()).hexdigest()}
    try:
        for kind in (("original",) if args.sdl is None else ("original", "sdl")):
            work = output / kind; work.mkdir()
            env = os.environ.copy()
            for key in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER"): env.pop(key, None)
            if kind == "original":
                game_copy = work / "game"; shutil.copytree(original, game_copy)
                save = game_copy / "FD2.SAV"
                config = work / "dosbox.conf"
                config.write_text('[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n[autoexec]\nmount c "' + str(game_copy) + '"\nc:\nFD2.EXE\nexit\n', encoding="utf-8")
                argv, cwd = [str(args.dosbox.resolve()), "-conf", str(config), "-noconsole"], work
            else:
                save = work / "FD2.SAV"; env["FD2SDL_SAVE_PATH"] = str(save)
                argv, cwd = [str(args.sdl.resolve())], root
            save.write_bytes(prepared["encoded"])
            guard = [prepared["encoded"], save.stat().st_mtime_ns]
            record = {"kind": kind, "status": "failed", "probe_cases_passed": False, "ok": False,
                "checkpoints": [], "inputs": [], "processes": [], "initial_save_mtime_ns": guard[1]}
            report["cases"].append(record)
            try:
                with running(argv, cwd, env, work / "game.log", args.driver.resolve(), record["processes"]) as process:
                    capture = Capture(args.driver.resolve(), process, work, record["inputs"])
                    exercise(capture, prepared, save, guard, record)
                    require_source_unchanged(save, *guard)
                    if not record.get("completed_save") or not record.get("save_end_load", {}).get("complete_remeasured_container"):
                        raise AssertionError("B5端点未完成实际turn2/SaveA/WAIT/LoadA/SaveB量测")
                record.update(status="passed", probe_cases_passed=True)
            except Exception as error:
                record.setdefault("error", str(error))
            finally:
                endpoint_guard(record, original, before, fixture, data, stamp, save, guard)
                record["tool_sources_unchanged"] = identity_unchanged(report["tool_sources"])
                record["binaries_unchanged"] = identity_unchanged(report["binaries"])
                if not record["tool_sources_unchanged"] or not record["binaries_unchanged"]:
                    record.update(status="failed", probe_cases_passed=False)
                    record.setdefault("error", "B5工具或二进制在端点执行期间变化；未启动下一端")
                record["capture_hashes"] = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in work.iterdir() if p.is_file() and p.suffix.lower() in (".png", ".bmp", ".sav", ".jsonl")}
                (work / "comparison.json").write_text(json.dumps(record, ensure_ascii=False, indent=2), encoding="utf-8")
            if not record["probe_cases_passed"]: raise AssertionError(record["error"])
        if args.sdl is not None:
            report["paired"] = compare_endpoints(report["cases"], output, prepared["pages"])
        report.update(status="passed", probe_cases_passed=True)
    except Exception as error:
        report.setdefault("error", str(error))
    finally:
        report["original_unchanged"] = hashes(original) == before
        report["fixture_unchanged"] = fixture.read_bytes() == data and fixture.stat().st_mtime_ns == stamp
        report["tool_sources_unchanged"] = identity_unchanged(report["tool_sources"])
        report["binaries_unchanged"] = identity_unchanged(report["binaries"])
        for case in report["cases"]:
            save = output / case["kind"] / ("game/FD2.SAV" if case["kind"] == "original" else "FD2.SAV")
            completed = case.get("completed_save")
            snapshot = Path(completed["snapshot"]) if completed else None
            expected = snapshot.read_bytes() if snapshot and snapshot.is_file() else prepared["encoded"]
            expected_stamp = completed["mtime_ns"] if completed else case["initial_save_mtime_ns"]
            endpoint_guard(case, original, before, fixture, data, stamp, save, [expected, expected_stamp])
            (output / case["kind"] / "comparison.json").write_text(json.dumps(case, ensure_ascii=False, indent=2), encoding="utf-8")
        report["probe_cases_passed"] = report["probe_cases_passed"] and report["original_unchanged"] and report["fixture_unchanged"] and report["tool_sources_unchanged"] and report["binaries_unchanged"] and all(case["probe_cases_passed"] for case in report["cases"])
        report["ok"] = args.sdl is not None and len(report["cases"]) == 2 and report["probe_cases_passed"]
        if not report["probe_cases_passed"]: report["status"] = "failed"
        (output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("root", "driver", "dosbox", "artifacts"): parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--sdl", type=Path)
    parser.add_argument("--scenario", choices=SCENARIOS, required=True)
    args = parser.parse_args()
    if sys.platform != "win32": parser.error("B5实机对照仅支持当前Windows WGC")
    try:
        report = run(args)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0 if report["probe_cases_passed"] else 1
    except Exception as error:
        print(f"B5掉落对照无法完成：{error}。请保留现场并使用新的产物目录。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
