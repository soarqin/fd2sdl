#!/usr/bin/env python3
"""START行军配对：原版先行，随后SDL；WGC状态门与真实UI Save测量共同验收。

field_empty_focus_menu_execute @0x16f55、field_ai_move_toward_cell @0x14b78。
固定本次源焦点anchor，比较确定性玩家终点/回合门和未改owner；不比较未对齐RNG伤害。
由总runner的driver run-job约束进程树；所有场景串行、只用WGC HWND捕获。
"""
from collections import deque
from itertools import product
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from fd2_movement_oracle import movement_table
from run_graphics_tests import wait_log
from run_original_comparison import Capture, archive_entry, driver_call, hashes, running, title_templates
from run_original_battle_menu import MENU_OFFSETS, PARENT_IDS, match_indexed, wait_state, wait_title
from run_original_end_turn import (META, STRIDE, UNITS, RoundOracle, browse_anywhere,
    empty_focus_navigation, round_assets, round_two_template, validate_initial)
from run_original_load import require_source_unchanged, wait_measured_save
from run_original_player import cursor_assets, visual_focus


def prepare(original, fixture):
    metadata = archive_entry((original / "FDFIELD.DAT").read_bytes(), 1)
    source, focus = validate_initial(fixture, metadata[0])
    units = [source[UNITS + i * STRIDE:UNITS + (i + 1) * STRIDE] for i in range(source[META + 1])]
    players = [i for i, unit in enumerate(units) if unit[6] == 2 and not unit[5] & 0x85]
    if [units[i][7] for i in players] != [0, 9, 4, 30]:
        raise ValueError("本配对仅覆盖真实START四名eligible玩家身份，不推断其他场景")
    for unit in units:
        if unit[5] not in (0, 1):
            raise ValueError("START前置不支持未知flags")
        if unit[6] == 0 and (unit[0x34] & 15 or any(unit[0x1a:0x1f]) or any(unit[0x44:0x46])):
            raise ValueError("本场景要求START敌方behavior0、MP0、无已知法术")
        if unit[6] == 0 and any(not unit[0x0a + slot * 2] & 0x80 and unit[0x0b + slot * 2] >= 192 for slot in range(8)):
            raise ValueError("本场景不覆盖敌方消耗品分派")
    _, profile_evidence = movement_table(original / "FD2.EXE")
    receipt = {"eligible_players": [{"index": i, "unit_id": units[i][7],
                    "source_xy": list(units[i][:2]), "profile": units[i][0x20],
                    "movement_points": units[i][0x3b]} for i in players],
               "full_player_hp_no_statuses": True, "enemy_behavior0_without_spells_or_consumables": True,
               "movement_table_oracle": profile_evidence}
    return source, focus, empty_focus_navigation(source), receipt


def check_march_phases(text, anchor):
    lines, positions = text.splitlines(), []
    accepted = f"field march: anchor=({anchor[0]},{anchor[1]}) turn=1 phase=2"
    march = [i for i, line in enumerate(lines) if line.startswith("field march:")]
    if len(march) != 1 or lines[march[0]] != accepted:
        raise AssertionError("March must accept exactly once at the source-derived anchor")
    positions.extend(march)
    for prefix in ("field phase: turn=1 phase=1 ", "field phase: turn=1 phase=0 ",
                   "field phase: turn=2 phase=2 "):
        found = [i for i, line in enumerate(lines) if line.startswith(prefix)]
        if len(found) != 1:
            raise AssertionError("March phase checkpoint missing or repeated")
        positions.append(found[0])
    if positions != sorted(positions):
        raise AssertionError("March acceptance/phase checkpoints out of order")
    if any(line.startswith("field phase:") and "turn=3 " in line for line in lines):
        raise AssertionError("one March advanced a second round")
    if any(line.startswith("field end turn:") for line in lines):
        raise AssertionError("March unexpectedly used an END submission")


def visible_actors(frame, sprites):
    return {(column, row) for row in range(1, 7) for column in range(1, 12)
            if any(match_indexed(frame, sprite, (4 + column * 24, 4 + row * 24 - 6))["ok"]
                   for sprite in sprites)}


def save_path(leader, occupied):
    """只在实时视区规划，不滚镜头；候选仍必须由真正parent菜单证明为空。"""
    blocked = set(occupied) - {leader}
    queue, paths = deque([leader]), {leader: []}
    while queue:
        x, y = queue.popleft()
        for key, dx, dy in (("LEFT", -1, 0), ("RIGHT", 1, 0), ("UP", 0, -1), ("DOWN", 0, 1)):
            nx, ny = x + dx, y + dy
            scroll = (dx < 0 and x <= 2) or (dx > 0 and x >= 11) or (dy < 0 and y <= 2) or (dy > 0 and y >= 6)
            if scroll or not (1 <= nx <= 11 and 1 <= ny <= 6) or (nx, ny) in blocked or (nx, ny) in paths:
                continue
            paths[nx, ny] = paths[x, y] + [(key, (nx, ny))]
            queue.append((nx, ny))
    candidates = [cell for cell in paths if 2 <= cell[0] <= 10 and 2 <= cell[1] <= 5
                  and cell != leader and cell not in occupied and (cell[0], cell[1] + 1) not in occupied]
    if not candidates:
        raise ValueError("no bounded Save candidate without scrolling or known actor obstruction")
    def score(cell):
        clearance = min(abs(cell[0] - x) + abs(cell[1] - y) for x, y in occupied | {leader})
        return (-clearance, len(paths[cell]), cell[1], cell[0])
    target = min(candidates, key=score)
    if len(paths[target]) > 20:
        raise ValueError("Save navigation exceeds bounded input budget")
    return target, paths[target]


def parent_menu(frame, oracle, selected):
    detail = RoundOracle.menu(oracle, frame, selected)
    if detail:
        detail["kind"] = "strict_parent"
    return detail

class MarchOracle(RoundOracle):
    # 本场景使用全部普通／高亮资源，March禁用图不能作为可用菜单。
    def menu(self, frame, selected=None):
        return parent_menu(frame, self, selected)

    def closed_browse(self, frame):
        detail = super().closed_browse(frame)
        return detail if detail and not self.secondary(frame, None, overview_available=True) else None


def leader_ready(frame, assets, banner):
    if match_indexed(frame, banner, (120, 84))["ok"]:
        return None
    detail = browse_anywhere(frame, assets, leader=True)
    if not detail:
        return None
    oracle = MarchOracle(assets, tuple(detail["focus_screen"]))
    if oracle.menu(frame) or oracle.secondary(frame, None, overview_available=True) or match_indexed(frame, assets["dialog_top"], (5, 112))["ok"]:
        return None
    return detail


def counter_reward(source, measured, original):
    """仅真实START首轮的raw11→raw2交战有限域，不接受任意EX/成长。

    field_physical_exchange @0x1548e给原防御者执行1e292；1ecc7及原effect0的
    2f7b6按最后一击MOV贡献。1e856允许随机双击，故先hit后miss可损HP但EX0。
    原版B4 counter-multi已验owner/成长，physical-overwrite已验覆盖写；这里不
    将本轮native无伤害终档冒称已看到非零488或与SDL共享RNG。
    """
    from fd2_item_list_oracle import EXE_SHA256, item_table
    from rebuild_fd2_analysis import parse_le, parse_objects, build_relbase_image

    if hashlib.sha256(source).hexdigest() != "b50fd076d397ea5afbb31630ebe1dd6aa40b335a776a0f8323730cde0365f975":
        raise AssertionError("March counter reward only supports the exact real START source")
    count = source[META + 1]
    before = [source[UNITS + i * STRIDE:UNITS + (i + 1) * STRIDE] for i in range(count)]
    after = [measured[UNITS + i * STRIDE:UNITS + (i + 1) * STRIDE] for i in range(count)]
    # 此来源只有近战敌兵、无状态/法术/可用道具。交战后不移动，所以终态必须
    # 只有这条邻格边；死亡raw11仍保坐标，不能因hidden而漏掉其counter写集。
    edges = [(enemy, player) for enemy, old in enumerate(before) if old[6] == 0 and not old[5] & 1
             for player, unit in enumerate(before) if unit[6] == 2
             if abs(after[enemy][0] - after[player][0]) + abs(after[enemy][1] - after[player][1]) == 1]
    if edges != [(11, 2)] or after[2][:2] != bytes((6, 15)) or after[11][:2] != bytes((5, 15)):
        raise AssertionError("March counter reward requires the unique raw11/raw2 fixed-position exchange")
    executable = original / "FD2.EXE"
    data = executable.read_bytes()
    if hashlib.sha256(data).hexdigest() != EXE_SHA256:
        raise ValueError("March counter reward requires the verified original executable")
    le = parse_le(data)
    raw = build_relbase_image(data, le, parse_objects(data, le))
    items, _ = item_table(executable)
    if any(items[item * 23 + 9] != 0 for item in (0, 20)):
        raise ValueError("March counter reward weapon effects changed")
    fields = (original / "FDFIELD.DAT").read_bytes()
    map_data, metadata = archive_entry(fields, 0), archive_entry(fields, 1)
    width, height = struct.unpack_from("<HH", map_data)
    if (width, height) != (24, 24):
        raise ValueError("March counter reward requires stage0 map24x24")
    attributes = archive_entry((original / "FDSHAP.DAT").read_bytes(), metadata[0] * 2 + 1)
    modifiers = []
    for actor in (2, 11):
        x, y = after[actor][:2]
        cell = struct.unpack_from("<I", map_data, 4 + (y * width + x) * 4)[0]
        tile = cell & 0x3ff
        flags, cost_class = attributes[tile * 4:tile * 4 + 2]
        if tile != 53 or flags or cost_class != 0 or (cell >> 16) & 31:
            raise AssertionError("March counter reward terrain is outside the confirmed static class0 pair")
        modifiers.append((struct.unpack_from("<i", raw, 0x51a12 + 4 * cost_class)[0],
                          struct.unpack_from("<i", raw, 0x51a2a + 4 * cost_class)[0]))
    if modifiers != [(5, 0), (5, 0)] or raw[0x524a8 + before[2][0x20] - 1] != 3 or raw[0x524a8 + before[11][0x20] - 1] != 0:
        raise ValueError("March counter reward original terrain/critical tables changed")

    def hit_damage(attacker, defender, modifier, critical):
        attack = struct.unpack_from("<H", before[attacker], 0x48)[0]
        defense = struct.unpack_from("<H", before[defender], 0x4a)[0]
        attack += attack * modifier // 100
        values = set()
        for effective_defense in ({defense, defense // 2} if critical else {defense}):
            base = max(0, (attack - effective_defense) * 9 // 10)
            values.update(base + spread for spread in range(max(1, base // 9)))
        return sorted(values)

    counter_hits = hit_damage(2, 11, modifiers[0][0], True)
    incoming_hits = hit_damage(11, 2, modifiers[1][0], False)
    enemy_hp, enemy_max = struct.unpack_from("<HH", before[11], 0x40)
    player_hp = struct.unpack_from("<H", before[2], 0x40)[0]
    coefficient = raw[0x61af9 + (before[11][7] - 68) * 10 + 9]
    gain_base = before[11][0x21] * coefficient // before[2][0x21]
    if (counter_hits, incoming_hits, coefficient, gain_base) != ([20, 21, 22, 23], [17], 21, 42):
        raise AssertionError("March counter reward is outside the confirmed single-exchange arithmetic")
    if before[2][0x3c] + gain_base >= 100:
        raise AssertionError("March counter reward does not support level-up")
    legal = set()
    incoming_hp = set()
    for strikes in (1, 2):
        for sequence in product([0, *counter_hits], repeat=strikes):
            hp, last = enemy_hp, 0
            for damage in sequence:
                hp = max(0, hp - damage)
                last = gain_base if not hp else damage * gain_base // enemy_max
                if not hp:
                    break
            legal.add((enemy_hp - hp, before[2][0x3c] + last))
        for sequence in product([0, *incoming_hits], repeat=strikes):
            incoming_hp.add(max(0, player_hp - sum(sequence)))
    measured_enemy_hp = struct.unpack_from("<H", after[11], 0x40)[0]
    loss, experience = enemy_hp - measured_enemy_hp, after[2][0x3c]
    if (loss, experience) not in legal or struct.unpack_from("<H", after[2], 0x40)[0] not in incoming_hp:
        raise AssertionError("March counter HP/EX pair is not a legal original single/double sequence")
    for index, (old, new) in enumerate(zip(before, after)):
        if index != 2 and old[0x3c] != new[0x3c]:
            raise AssertionError("March counter reward changed the wrong actor EX")
        if index not in (2, 11) and old[0x40:0x42] != new[0x40:0x42]:
            raise AssertionError("March counter reward has another unsupported damage source")
    return {"kind": "source_specific_original_counter_finite_domain", "enemy_raw": 11, "player_raw": 2,
            "enemy_hp_loss": loss, "experience": experience, "legal_loss_experience_pairs": sorted(map(list, legal)),
            "incoming_hp_values": sorted(incoming_hp), "gain_base": gain_base, "level_up_possible": False,
            "last_miss_overwrites_contribution_with_zero": True,
            "tables": {"terrain_attack_va": 0x51a12, "terrain_defense_va": 0x51a2a,
                       "critical_va": 0x524a8, "enemy96_va": 0x61c11,
                       "original_executable_sha256": EXE_SHA256},
            "scope": "exact START raw11/raw2 exchange only; finite native arithmetic, not aligned RNG or an observed native positive-gain page"}


def measurement(source, measured, anchor, leader_cell, save_cell):
    if len(source) != 0x59cb or len(measured) != len(source) or sum(measured[:-4]) != int.from_bytes(measured[-4:], "little"):
        raise AssertionError("measurement checksum/size invalid")
    count, roster = source[META + 1], source[META + 9]
    if measured[META:META + 3] != bytes((2, count, 0)) or measured[META + 9] != roster:
        raise AssertionError("March did not retain source counts and reach exactly round2/stage0")
    expected = bytearray(source)
    expected[META] = 2
    leader_x, leader_y = measured[UNITS:UNITS + 2]
    camera = (leader_x - leader_cell[0], leader_y - leader_cell[1])
    focus = (camera[0] + save_cell[0], camera[1] + save_cell[1])
    if not 0 <= camera[0] <= 11 or not 0 <= camera[1] <= 16 or not 0 <= focus[0] < 24 or not 0 <= focus[1] < 24:
        raise AssertionError("WGC-derived camera/focus outside the supported START map")
    expected[META + 3:META + 9] = bytes((*camera, *focus, *save_cell))
    if measured[META + 3:META + 9] != expected[META + 3:META + 9]:
        raise AssertionError("saved coordinates differ from resource-proven leader and no-scroll Save path")
    reward = counter_reward(source, measured, ROOT / "original_game")
    expected[UNITS + 2 * STRIDE + 0x3c] = reward["experience"]
    players, excluded = [], []
    for index in range(count):
        offset = UNITS + index * STRIDE
        unit = measured[offset:offset + STRIDE]
        hp, maximum = struct.unpack_from("<HH", unit, 0x40)
        if unit[0] >= 24 or unit[1] >= 24 or hp > maximum or unit[3] != 0 or unit[4] != 0:
            raise AssertionError("round2 actor position/HP/direction/frame phase is invalid")
        flags = 1 if hp == 0 else source[offset + 5] & ~0x80
        if unit[5] != flags:
            raise AssertionError("new player round must clear acted and only permit known HP0 hidden finalization")
        for field in (0, 1, 2, 3, 4, 5, 0x40, 0x41):
            expected[offset + field] = unit[field]
        for field, name in ((2, "cache_class"), (5, "death_flags"), (0x40, "hp_low"), (0x41, "hp_high")):
            if source[offset + field] != unit[field]:
                excluded.append({"actor": index, "field": name, "source": source[offset + field], "measured": unit[field]})
        if source[offset + 6] == 2 and not source[offset + 5] & 0x85:
            players.append({"index": index, "unit_id": unit[7], "text_id": unit[8], "x": unit[0], "y": unit[1]})
    differences = [i for i in range(len(source) - 4) if expected[i] != measured[i]]
    if differences:
        raise AssertionError("March changed undeclared source bytes: " + ", ".join(hex(i) for i in differences[:12]))
    if len(players) != 4:
        raise AssertionError("four source-eligible player endpoints were not measured")
    return {"anchor": list(anchor), "turn": 2, "stage": 0, "all_acted_cleared": True,
            "active_count": count, "roster_count": roster, "players": players,
            "observed_camera": list(camera), "measurement_focus": list(focus),
            "preserved_source_regions": ["metadata0..0x8a3", "whole raw roster0xa00", "inactive actor tail",
                  "cell-state32", "resource/options", "manual slots and remaining unknown bytes"],
            "raw_roster_sha256": hashlib.sha256(measured[0x8a3:UNITS]).hexdigest(),
            "source_equal_except_declared_runtime_fields_and_measurement_navigation": True,
            "counter_reward": reward,
            "excluded_active_changes": excluded}


def compare_player_geometry(original, sdl):
    keys = ("anchor", "turn", "stage", "all_acted_cleared", "active_count", "roster_count", "players")
    return all(original[key] == sdl[key] for key in keys)


def close_host(capture, save, observation):
    before, stamp = save.read_bytes(), save.stat().st_mtime_ns
    observation["normal_host_close"] = False
    driver_call(capture.driver, "close", capture.process)
    if capture.process.wait(timeout=10) != 0:
        raise AssertionError("host close after March measurement did not normally exit0")
    require_source_unchanged(save, before, stamp)
    observation["normal_host_close"] = True
    observation["host_close_did_not_write_save"] = True


def exercise(capture, kind, log, assets, sprites, composites, source, focus, steps, titles, banner,
             save, source_bytes, source_mtime, checkpoints, observation):
    wait_title(capture, titles[3][0])
    capture.key("UP")
    capture.wait(titles[3][2])
    capture.key("ENTER")
    oracle = MarchOracle(assets, focus)
    measurement_started = False
    def checkpoint(name, predicate, **kwargs):
        result = wait_state(capture, name, predicate, checkpoints, **kwargs)
        if not measurement_started:
            require_source_unchanged(save, source_bytes, source_mtime)
        return result
    checkpoint("initial-browse", oracle.initial_browse, timeout=30)
    for index, (key, position) in enumerate(steps, 1):
        capture.key(key)
        oracle = MarchOracle(assets, position)
        checkpoint(f"anchor-step-{index}", oracle.closed_browse)
    anchor = (source[META + 3] + (oracle.focus[0] - 4) // 24,
              source[META + 4] + (oracle.focus[1] - 4) // 24)
    observation["fixed_world_anchor"] = list(anchor)
    observation["cancel_full_runtime_fields_measured"] = False
    def open_march(label):
        capture.key("ENTER")
        checkpoint(label + "-parent", lambda image: parent_menu(image, oracle, 0))
        capture.key("LEFT")
        checkpoint(label + "-march-selected", lambda image: parent_menu(image, oracle, 1))
        capture.key("ENTER")
        checkpoint(label + "-default-yes", lambda image: oracle.buttons(image, 0))
    for branch in ("escape", "no"):
        open_march(branch)
        if branch == "no":
            capture.key("RIGHT")
            checkpoint("no-selected", lambda image: oracle.buttons(image, 1))
        capture.key("ESC" if branch == "escape" else "ENTER")
        checkpoint(branch + "-closed-browse", oracle.closed_browse, transition=oracle.cancellation_notice)
        require_source_unchanged(save, source_bytes, source_mtime)
    open_march("affirmative")
    require_source_unchanged(save, source_bytes, source_mtime)
    capture.key("ENTER")
    def round_two(image):
        match = match_indexed(image, banner, (120, 84))
        return {"kind": "round_banner", "turn": 2, **match} if match["ok"] else None
    if kind == "original":
        checkpoint("round-002", round_two, timeout=90)
    else:
        text = wait_log(capture.process, log, "field phase: turn=2 phase=2 ", timeout=90)
        check_march_phases(text, anchor)
        observation["phase_order_proven"] = True
    leader_frame, detail = checkpoint("final-actor0", lambda image: leader_ready(image, assets, banner), timeout=30)
    require_source_unchanged(save, source_bytes, source_mtime)
    leader = tuple((value - 4) // 24 for value in detail["focus_screen"])
    occupied = visible_actors(leader_frame, sprites) | {leader}
    target, path = save_path(leader, occupied)
    observation["save_navigation"] = {"leader_cell_screen": list(leader), "known_occupied": sorted(map(list, occupied)),
                                       "target_cell_screen": list(target), "steps": path,
                                       "occupancy_is_candidate_filter_only": True}
    for index, (key, expected) in enumerate(path, 1):
        capture.key(key)
        def acknowledged(image):
            found = visual_focus(image, assets, composites)
            return found if found and tuple(found["cell_screen"]) == expected else None
        checkpoint(f"save-step-{index}", acknowledged)
    oracle = MarchOracle(assets, (4 + target[0] * 24, 4 + target[1] * 24))
    capture.key("ENTER")
    # 唯一必要空格证明；若不是parent，停止，不猜Esc或继续键序。
    checkpoint("save-empty-proven-by-parent", lambda image: parent_menu(image, oracle, 0))
    capture.key("UP")
    capture.key("ENTER")
    checkpoint("save-secondary-zero", lambda image: oracle.secondary(image, 0, overview_available=True))
    capture.key("LEFT")
    checkpoint("save-selected", lambda image: oracle.secondary(image, 1, overview_available=True))
    capture.key("ENTER")
    checkpoint("save-default-yes", lambda image: oracle.buttons(image, 0))
    require_source_unchanged(save, source_bytes, source_mtime)
    observation["no_automatic_save"] = True
    previous, previous_mtime = save.read_bytes(), save.stat().st_mtime_ns
    measurement_started = True
    capture.key("ENTER")
    measured = wait_measured_save(save, previous, previous_mtime)
    checkpoint("measured-closed-browse", oracle.closed_browse)
    shutil.copy2(save, capture.artifacts / "march-round2.SAV")
    return measurement(source, measured, anchor, leader, target)


def run(args):
    root, output = args.root.resolve(), args.artifacts.resolve()
    original = (root / "original_game").resolve()
    fixture = (args.save_fixture or original / "START.SAV").resolve()
    if output == original or original in output.parents or output == fixture or output in fixture.parents:
        raise ValueError("产物目录不能覆盖original_game或源夹具")
    output.mkdir(parents=True, exist_ok=False)
    before, source_bytes, fixture_stamp = hashes(original), fixture.read_bytes(), fixture.stat().st_mtime_ns
    report = {"ok": False, "scope": "START fixed-anchor March, WGC menu/cancel/round/leader gates, player geometry and whole-container preservation by explicit UI Save",
              "excluded": ["RNG-aligned damage/HP/death/reward or AI endpoints", "full cancellation runtime", "transient animation timing/full-map pixels/audio", "input bursts during automatic phases", "fragment418 and per-phase clean-background transient pixels", "intermediate per-actor pending/acted order", "other stages and unknown handlers"],
              "capture": "Windows.Graphics.Capture HWND only", "sdl_rng_seed": "31256 (reproducible SDL only; no native RNG alignment claim)", "original_before": before,
              "fixture": {"path": str(fixture), "sha256": hashlib.sha256(source_bytes).hexdigest(), "mtime_ns": fixture_stamp},
              "processes": [], "inputs": [], "checkpoints": [], "cases": []}
    try:
        report["binaries"] = {name: {"path": str(path.resolve()), "sha256": hashlib.sha256(path.read_bytes()).hexdigest()}
                               for name, path in (("game", args.game), ("driver", args.driver), ("dosbox", args.dosbox))}
        report["runner_sha256"] = hashlib.sha256(Path(__file__).read_bytes()).hexdigest()
        report["helper_sha256"] = {name: hashlib.sha256((ROOT / "tools" / name).read_bytes()).hexdigest()
            for name in ("run_original_comparison.py", "run_original_battle_menu.py", "run_original_end_turn.py",
                         "run_original_load.py", "run_original_player.py", "fd2_movement_oracle.py", "rebuild_fd2_analysis.py")}
        source, focus, steps, precondition = prepare(original, fixture)
        report["source_precondition"] = precondition
        assets = round_assets(original)
        sprites, composites = cursor_assets(original / "FDICON.B24", source, assets)
        banner = round_two_template(original / "FDOTHER.DAT", assets["palette"])
        titles = title_templates(original / "FDOTHER.DAT")
        for kind in ("original", "sdl"):
            work = output / kind
            work.mkdir()
            environment = os.environ.copy()
            for key in ("SDL_VIDEODRIVER", "SDL_AUDIODRIVER"):
                environment.pop(key, None)
            if kind == "original":
                game_copy = work / "game"
                shutil.copytree(original, game_copy)
                save = game_copy / "FD2.SAV"
                shutil.copy2(fixture, save)
                config = work / "dosbox.conf"
                config.write_text(
                    "[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
                    "[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\n[render]\naspect=false\nscaler=normal2x\n"
                    "[cpu]\ncore=normal\ncycles=fixed 20000\n"
                    f'[autoexec]\nmount c "{game_copy}"\nc:\nFD2.EXE\nexit\n', encoding="utf-8")
                command, cwd = [str(args.dosbox.resolve()), "-conf", str(config), "-noconsole"], work
            else:
                save = work / "FD2.SAV"
                shutil.copy2(fixture, save)
                environment["FD2SDL_SAVE_PATH"] = str(save)
                environment["FD2SDL_FIELD_RNG_SEED"] = "31256"
                command, cwd = [str(args.game.resolve())], root
            stamp = save.stat().st_mtime_ns
            log = work / "game.log"
            case = {"name": kind, "status": "failed", "source_mtime_ns_before_start": stamp, "observation": {}, "checkpoints": []}
            report["cases"].append(case)
            try:
                with running(command, cwd, environment, log, args.driver, report["processes"]) as process:
                    capture = Capture(args.driver, process, work, report["inputs"])
                    case["measurement"] = exercise(capture, kind, log, assets, sprites, composites, source, focus, steps,
                                                  titles, banner, save, source_bytes, stamp,
                                                  case["checkpoints"], case["observation"])
                    saved, saved_stamp = save.read_bytes(), save.stat().st_mtime_ns
                    close_host(capture, save, case["observation"])
                require_source_unchanged(save, saved, saved_stamp)
                if kind == "sdl":
                    check_march_phases(log.read_text(encoding="utf-8", errors="replace"), case["measurement"]["anchor"])
                case["status"] = "passed"
                if kind == "sdl":
                    report["player_geometry_equal"] = compare_player_geometry(report["cases"][0]["measurement"], case["measurement"])
                    if not report["player_geometry_equal"]:
                        raise AssertionError("SDL fixed-anchor player endpoints/round gates differ from this run's original measurement")
            except Exception as error:
                case.update(status="failed", error=str(error))
                raise
            finally:
                report["checkpoints"].extend({"case": kind, **item} for item in case["checkpoints"])
                (output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
        report["ok"] = True
    except Exception as error:
        report["error"] = str(error)
    finally:
        report["original_unchanged"] = hashes(original) == before
        report["fixture_unchanged"] = fixture.read_bytes() == source_bytes and fixture.stat().st_mtime_ns == fixture_stamp
        report["ok"] &= report["original_unchanged"] and report["fixture_unchanged"]
        (output / "comparison.json").write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for option in ("root", "game", "driver", "dosbox", "artifacts"):
        parser.add_argument("--" + option, type=Path, required=True)
    parser.add_argument("--save-fixture", type=Path, help="只读真实START副本，默认original_game/START.SAV")
    args = parser.parse_args()
    if sys.platform != "win32":
        parser.error("March配对要求Windows WGC；未启动GUI")
    try:
        report = run(args)
        print(json.dumps(report, ensure_ascii=False, indent=2))
        return 0 if report["ok"] else 1
    except Exception as error:
        print(f"March配对无法启动：{error}。检查参数后重跑。", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
