"""B7真实stage1 entry→本关自然战斗→22f37完整剧情→stage2；无第二套玩法模拟。

只读planner选择动作，每轮用正式UI Save重规划。34778/22f37词元逐页ACK；
既有stage0已完成报告仅证明本次只读entry来源，不当作本关运行通过。
"""
import hashlib
import json
from pathlib import Path

import fd2_stage1_complete_oracle as o
import fd2_natural_oracle as n
import run_original_natural as natural
from fd2_opening_oracle import page_match
from fd2_transition_oracle import notice_resources, picker_image
from run_original_battle_menu import wait_state, wait_title
from run_original_comparison import title_templates
from run_original_end_turn import round_assets
from run_original_load import require_source_unchanged


def reader_story_pages(stories):
    """原11/13/15与12/14/16各是同一单页资源；不从画像猜死亡raw。"""
    result = [page for page in stories if page["fragment"] not in range(11, 17)]
    for fragments, raws, identity in (((11,13,15),(5,7,9),134),((12,14,16),(6,8,10),133)):
        candidates = [[page for page in stories if page["fragment"] == fragment] for fragment in fragments]
        if any(len(pages) != 1 for pages in candidates):
            raise ValueError("stage1村民死亡片段不是各一页，不能套等价单ACK合同")
        pages = [group[0] for group in candidates]; first = pages[0]
        for page, raw in zip(pages, raws):
            if (page["speaker_control"] != -19 or page["speaker_argument"] != raw or
                    page["portrait_id"] != identity or page["arrow_position"] is not None or
                    any(page[key] != first[key] for key in ("token_offset","portrait_position","border_position")) or
                    page["text"].convert("RGB").tobytes() != first["text"].convert("RGB").tobytes() or
                    len(page["portraits"]) != len(first["portraits"]) or
                    any(a.convert("RGB").tobytes() != b.convert("RGB").tobytes() for a,b in zip(page["portraits"],first["portraits"]))):
                raise ValueError("stage1死亡等价族资源/原raw参数改变，不猜页面身份")
        representative = dict(first)
        representative.update(death_fragment_candidates=list(fragments),death_raw_candidates=list(raws),
            death_caller_uniquely_observed=False,representative_fragment_is_resource_key_only=True)
        result.append(representative)
    return result


class Stage1ProgressReader(natural.ProgressReader):
    """仅B7的两组已证等价死亡页；其它多合同歧义及重复ACK保护沿原Reader。"""
    def __init__(self, original, source, assets, report, **kwargs):
        kwargs["stories"] = reader_story_pages(kwargs["stories"])
        super().__init__(original,source,assets,report,**kwargs)

    def message(self, frame):
        detail = super().message(frame)
        page = detail.get("page", {}) if detail else {}
        if "death_fragment_candidates" in page:
            candidates = {"fragment_candidates":page["death_fragment_candidates"],"raw_candidates":page["death_raw_candidates"],
                "death_caller_uniquely_observed":False,"representative_fragment_is_resource_key_only":True}
            detail.update(candidates)
            detail["resource"] = dict(detail["resource"], **candidates)
        return detail

    def acknowledge(self, capture, detail, checkpoint):
        page = detail["page"]
        super().acknowledge(capture,detail,checkpoint)
        if "death_fragment_candidates" in page:
            row = self.report["resource_acknowledgements"][-1]
            row.update(resource_contract_kind="villager_death_equivalent_page",
                fragment_candidates=page["death_fragment_candidates"],raw_candidates=page["death_raw_candidates"],
                death_caller_uniquely_observed=False,representative_fragment_is_resource_key_only=True)
            self.report.setdefault("villager_death_acknowledgements",[]).append(dict(row))


def prepare(original, fixture, scenario, source_report):
    if scenario != "natural-stage1": raise ValueError("本模块只执行natural-stage1")
    original, fixture = Path(original), Path(fixture)
    provenance = o.validate_origin_report(source_report, fixture)
    encoded, source, manifest = o.derive_fixture(original, fixture, scenario)
    data, assets = o.tables(original), round_assets(original)
    stories = o.stage1_story_pages(original, assets, source)
    if {p["fragment"] for p in stories} != {4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}:
        raise ValueError("stage1动态剧情完整read-set不符")
    reader_story_pages(stories)  # 首键/产物前核两组资源确实相同，不在实机时临时放宽。
    # 这里只预检资源，不把尚未发生的自然HP/成长/阵亡填成存档期望。
    hint_roster = bytearray(source[0x8a3:0x12a3])
    index = next(i for i, row in enumerate(data["stages"][1]["templates"]) if row[21] == 4)
    new, _ = o.template_record(data, 1, index, (15, 0)); new[3] = 2
    hint_roster[400:480] = new
    hint = {"stage": 2, "count": 6, "roster": bytes(hint_roster)}
    town = o.stage2_town(original, data, hint)
    notices = {f: notice_resources(original, town["assets"], f) for f in (585, 586, 660)}
    notices[513] = notice_resources(original, town["assets"], 513, portrait=75, origin=(95, 119))
    entry = o.stage2_projection(hint, data)
    entry_pages = o.stage2_pages(original, assets, hint, data)
    if len(entry_pages) != 15: raise ValueError("stage2初始剧情不是15页")
    for selected in range(4): picker_image(town["assets"], source, selected)
    origin = {"fixture": {"path": str(fixture.resolve()), "sha256": hashlib.sha256(encoded).hexdigest(),
        "size": len(encoded), "mtime_ns": fixture.stat().st_mtime_ns},
        "report": {"path": str(Path(source_report).resolve()), "sha256": provenance["sha256"]},
        "previous_native_entry_provenance": provenance, "inherited_runtime_as_B7_pass": False}
    return {"original": original, "scenario": scenario, "source": source, "encoded": encoded,
        "manifest": manifest, "origin": origin, "data": data, "assets": assets,
        "stories": stories, "town": town, "notices": notices, "entry": entry,
        "entry_pages": entry_pages, "titles": title_templates(original / "FDOTHER.DAT"),
        "expected_acknowledgements": {"settlement_acknowledgements": 30,
            "stage2_entry_acknowledgements": 15, "script13_acknowledgements": 9},
        "required_snapshots": ("manual_save", "completed_save")}


def exercise(capture, prepared, save, guard, report):
    """调用既有合法输入协议；预测、游戏数据、RNG及真实SAV均不回灌。"""
    original, assets = prepared["original"], prepared["assets"]
    state = prepared["source"]
    report.update(actual_turn_saves=[], resource_acknowledgements=[], native_round_banners=[], natural_actions=[],
        native_audio={"enabled": False, "scope": "B7 no new audio recording in this runner"})
    ordinal = 0
    def checkpoint(name, predicate, **kwargs):
        nonlocal ordinal
        ordinal += 1
        result = wait_state(capture, f"b7-natural-{ordinal:04d}-{name}", predicate, report["checkpoints"], **kwargs)
        require_source_unchanged(save, *guard)
        report["checkpoints"][-1].pop("page", None); report["checkpoints"][-1].pop("state", None)
        return result
    wait_title(capture, prepared["titles"][3][0], timeout=90)
    capture.key("UP"); capture.wait(prepared["titles"][3][2]); capture.key("ENTER")
    checkpoint("continue-own-stage1-entry", lambda frame: n.focus_match(frame, original, state, assets), timeout=90)
    bank, digests = n.cursor_bank(original, state, assets), {}
    for _ in range(prepared["max_turns"]):
        digest = n.semantic_digest(state); digests[digest] = digests.get(digest, 0) + 1
        if digests[digest] >= 4:
            raise natural.NaturalStopped("stage1自然语义状态重复4次；保留停滞，不重开或重抽")
        plan = natural.read_plan(prepared["planner"], prepared["root"], save, stage=1)
        path = capture.artifacts / f"turn-{state[o.META]:03d}-plan.json"
        path.write_text(json.dumps(plan, ensure_ascii=False, indent=2), encoding="utf-8")
        report["natural_actions"].append({"turn": state[o.META], "plan": str(path),
            "sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "planning_not_oracle": True})
        turn = state[o.META]
        reader = Stage1ProgressReader(original, state, assets, report, stories=prepared["stories"],
            victory_fragment=10, cell_contract=lambda orig, world: cell_contract(orig, world),
            nonphase_story_fragments=(6, 7, 8, 9, 10))
        actions = [plan["next_action"]] + plan.get("rest_tail", [])
        completed_heals = {}
        for index, action in enumerate(actions):
            completion = {}
            if action["kind"] == "end_turn":
                state = natural.end_phase(capture, original, state, assets, bank, reader, report, checkpoint)
                phase = True
            else:
                phase = index == len(actions) - 1 and bool(action.get("predicted_phase_advance_after_wait", len(actions) == 1))
                observation = completed_heals.get(action["actor_raw"]) if index else None
                if observation is not None and observation["record_hex"] != n.actor(state, action["actor_raw"]).hex():
                    raise natural.NaturalStopped("stage1治疗目标已不是同一完整80B，未套用显示HP许可")
                state = natural.perform_action(capture, original, state, action, assets, bank, reader,
                    checkpoint, report, last=phase, fresh=index == 0, completion=completion, hp_observation=observation)
            state = natural.wait_progress(capture, original, state, turn, assets, reader, report, checkpoint,
                phase=phase, ready_raw=action.get("actor_raw", 0), ready_target=completion or None)
            if state is None: break
            if not phase and completion:
                before = bytes.fromhex(action.get("actor_record_hex", ""))
                herb = (action["kind"] == "herb" and 0 <= action.get("item_slot", -1) < 8 and
                    len(before) == 80 and before[11 + 2 * action["item_slot"]] == 192)
                if herb or action["kind"] == "magic" and action.get("magic_id") == 13:
                    target = action["target_raw"]
                    completed_heals[target] = {"raw": target, "record_hex": n.actor(state, target).hex(),
                        "actual_completed_heal": True, "kind": "herb192/code5" if herb else "magic13",
                        "caster_raw": action["actor_raw"], "source_turn": turn}
        if state is None: break
        if not phase:
            state = natural.end_phase(capture, original, state, assets, bank, reader, report, checkpoint)
            state = natural.wait_progress(capture, original, state, turn, assets, reader, report, checkpoint, phase=True)
            if state is None: break
        state = natural.save_active(capture, original, state, assets, bank, save, guard, report,
            checkpoint, f"turn-{turn + 1:03d}", stage=1)
    else:
        raise natural.NaturalStopped("stage1自然流程达到本次max-turns上界；不重开、不判通过")
    if not reader.victory_text_complete:
        raise natural.NaturalStopped("stage1未完整确认结算fragment10")
    stories = [row for row in report["resource_acknowledgements"] if row["signature"][0] == "story"]
    report["script13_acknowledgements"] = [row for row in stories if row["signature"][1] == 4]
    report["settlement_acknowledgements"] = [row for row in stories if row["signature"][1] in (6, 7, 8, 9, 10)]
    if (len(report["script13_acknowledgements"]) != 9 or len(report["settlement_acknowledgements"]) != 30):
        raise natural.NaturalStopped("stage1完整script13/结算剧情页不齐，不能进入后继验收")
    report["natural_stage1_victory_dialogue_complete"] = True
    from run_original_stage1_complete import finish_stage2
    finish_stage2(capture, prepared, save, guard, report)


def cell_contract(original, world):
    found = next((row for row in o.stage1_chests(original) if row["world"] == list(world)), None)
    if found is None: raise ValueError("stage1未知特殊格未建输入/提示合同")
    return found
