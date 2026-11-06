"""B7 stage2初始活动与准备页存取；不开放END、行动或turn3/action9。

只读来源必须是本次原版完整stage2入场实际Save。复用酒店/四槽/活动存取，
Title LOAD不重播stage1结算；资源与所有未知容器区按本端严格保存。
"""
import json
from pathlib import Path
import struct

import fd2_natural_oracle as n
import fd2_stage1_complete_oracle as o
from fd2_item_list_oracle import inventory_templates
from fd2_transition_oracle import SLOT_BASE, ROSTER_SIZE, picker_image, picker_match, notice_resources
from run_original_battle_menu import wait_state, wait_title
from run_original_comparison import title_templates
from run_original_end_turn import RoundOracle, round_assets
from run_original_item_list import inventory_detail
from run_original_load import require_source_unchanged, wait_measured_save
from run_original_overview import overview_templates, match_overview
from run_original_player import screen


def prepare(original, fixture, scenario, source_report):
    from run_original_stage1_complete import fact, sha
    if scenario != "stage2-storage": raise ValueError("stage2存取场景名称错误")
    original, fixture, source_report = Path(original), Path(fixture), Path(source_report)
    report = json.loads(source_report.read_text(encoding="utf-8"))
    completed = report.get("completed_save", {})
    if (report.get("kind") != "original" or report.get("status") != "passed" or
            report.get("business_sequence_completed") is not True or
            completed.get("sha256") != sha(fixture) or Path(completed.get("snapshot", "")).resolve() != fixture.resolve() or
            not report.get("guards") or not all(value is True for value in report["guards"].values()) or
            not report.get("processes") or any(p.get("exit_code") != 0 or not p.get("cleaned") or p.get("forced_cleanup") for p in report["processes"])):
        raise ValueError("stage2 storage需真实原版passed端的complete Save、正常清理及全晚守卫")
    acknowledgements = report.get("stage2_entry_acknowledgements", [])
    if len(acknowledgements) != 15: raise ValueError("stage2 storage来源缺完整15页实际入场")
    captures = []
    for index, ack in enumerate(acknowledgements):
        name = f"b7-entry-{index:02d}"
        found = [row for row in report["checkpoints"] if row.get("name") == name]
        if (len(found) != 1 or any(found[0].get(key, {}).get("ok") is not True for key in ("text", "border")) or
                (found[0].get("fragment"), found[0].get("token_offset"), found[0].get("portrait_id")) !=
                (ack["fragment"], ack["offset"], ack["portrait"])):
            raise ValueError("stage2 storage来源15页词元/资源检查点不完整")
        image = source_report.parent / (name + ".png")
        if report.get("capture_hashes", {}).get(image.name) != sha(image):
            raise ValueError("stage2 storage来源实际剧情PNG不符报告hash")
        captures.append(fact(image))
    source, data = o.decode_save(fixture), o.tables(original)
    if (o.checksum(source) != source or source[o.META:o.META + 3] != bytes((1, 15, 2)) or source[o.META + 9] != 6):
        raise ValueError("stage2 storage只接受已确认初始turn1/15active/6persistent")
    slot = source[SLOT_BASE:SLOT_BASE + ROSTER_SIZE + 10]
    if slot[ROSTER_SIZE:ROSTER_SIZE + 2] != bytes((2, 6)):
        raise ValueError("stage2 storage来源manual0不是本次六人stage2准备")
    state = {"stage":2,"count":6,"roster":bytes(slot[:ROSTER_SIZE]),
        "resource":struct.unpack_from("<I",slot,ROSTER_SIZE + 2)[0],"options":bytes(slot[-4:])}
    projection = o.stage2_projection(state, data)
    if (list(source[o.META + 3:o.META + 5]) != projection["camera"] or
            list(source[o.META + 5:o.META + 7]) != [6,22]):
        raise ValueError("stage2 storage来源不是完整entry后的已证测量焦点")
    checked = o.check_stage2_active_save(source, source, state, projection, source, data)
    assets, town = round_assets(original), o.stage2_town(original, data, state)
    notices = {f:notice_resources(original,town["assets"],f) for f in (585,586,660,478)}
    notices[513] = notice_resources(original,town["assets"],513,portrait=75,origin=(95,119))
    for selected in range(4): picker_image(town["assets"],source,selected)
    overview_templates(original,source,assets["palette"])
    _, templates, fields = inventory_templates(original,o.actor(source,0),assets)
    fields["require_word_bars"] = True
    return {"original":original,"scenario":scenario,"encoded":fixture.read_bytes(),"source":source,
        "manifest":{"scenario":scenario,"source":fact(fixture),"plaintext_patches":[],"encrypted_patches":[],
            "all_container_bytes_preserved":True,"controlled_initial_activity":True,"stage2_whole_battle":False},
        "origin":{"fixture":fact(fixture),"report":fact(source_report),"stage2_entry_captures":captures,
            "entry_source_measurement":checked,"previous_runtime_not_inherited_as_new_pass":True},
        "data":data,"assets":assets,"town":town,"notices":notices,"entry":projection,
        "entry_pages":o.stage2_pages(original,assets,state,data),"titles":title_templates(original/"FDOTHER.DAT"),
        "expected_preparation":state,"roster_masks":[bytes([1]*80)]*6,"exercise_storage":True,
        "expected_acknowledgements":{"stage2_entry_acknowledgements":15},"required_snapshots":("manual_save","completed_save")}


def exercise(capture, prepared, save, guard, record):
    from run_original_stage1_complete import finish_stage2
    def cp(name,predicate,**options):
        result = wait_state(capture,name,predicate,record["checkpoints"],**options)
        require_source_unchanged(save,*guard); return result
    wait_title(capture,prepared["titles"][3][0],timeout=90)
    capture.key("DOWN"); capture.wait(prepared["titles"][3][1]); capture.key("ENTER")
    cp("b7-stage2-title-picker0",lambda f:picker_match(f,picker_image(prepared["town"]["assets"],prepared["source"],0)),allow_low_color_resource=True)
    capture.key("ENTER")
    finish_stage2(capture,prepared,save,guard,record)


def details(capture, prepared, view, checkpoint):
    _, templates, fields = inventory_templates(prepared["original"],o.actor(view,0),prepared["assets"])
    fields["require_word_bars"] = True
    capture.key("F2"); checkpoint("b7-stage2-hero-detail",lambda f:inventory_detail(f,templates,fields))
    capture.key("ESC"); checkpoint("b7-stage2-detail-closed",lambda f:n.focus_match(f,prepared["original"],view,prepared["assets"]))


def overview(capture, prepared, view, menu, record, checkpoint):
    from run_original_stage1_complete import stage2_parent
    assets = prepared["assets"]; templates = overview_templates(prepared["original"],view,assets["palette"])
    capture.key("ENTER"); checkpoint("b7-stage2-overview-parent",lambda f:stage2_parent(f,menu,record["kind"]))
    capture.key("ENTER"); checkpoint("b7-stage2-overview-secondary",lambda f:menu.secondary(f,0))
    capture.key("ENTER"); _, proof = checkpoint("b7-stage2-overview",lambda f:match_overview(f,templates))
    record["stage2_overview"] = proof
    capture.key("ESC"); checkpoint("b7-stage2-overview-closed",lambda f:n.focus_match(f,prepared["original"],view,assets))


def active_load(capture, prepared, save, guard, state, projection, saved, record, checkpoint):
    from run_original_stage1_complete import stage2_parent, sha
    original, assets = prepared["original"], prepared["assets"]
    record["active_saves"] = [record["completed_save"]]
    alternate = n.navigation(saved,(6,21))
    for index,(key,view) in enumerate(alternate):
        capture.key(key); checkpoint(f"b7-stage2-load-away-{index}",lambda f,s=view:n.focus_match(f,original,s,assets))
    moved = alternate[-1][1] if alternate else saved
    menu = RoundOracle(assets,screen(moved,(6,21)))
    capture.key("ENTER"); checkpoint("b7-stage2-load-parent",lambda f:stage2_parent(f,menu,record["kind"]))
    capture.key("ENTER"); checkpoint("b7-stage2-load-secondary",lambda f:menu.secondary(f,0))
    capture.key("RIGHT"); checkpoint("b7-stage2-load-selected",lambda f:menu.secondary(f,2))
    capture.key("ENTER"); checkpoint("b7-stage2-load-yes",lambda f:menu.buttons(f,0))
    capture.key("ENTER"); checkpoint("b7-stage2-load-restored-focus",lambda f:n.focus_match(f,original,saved,assets),timeout=90)
    record["active_load"] = {"moved_focus":[6,21],"restored_focus":[6,22],"disk_bytes_mtime_unchanged":True,
        "post_load_full_save_checked":True,"scope":"actual focus restore and next full container; allocation lifetime is a separate C test"}
    menu = RoundOracle(assets,screen(saved,(6,22)))
    capture.key("ENTER"); checkpoint("b7-stage2-resave-parent",lambda f:stage2_parent(f,menu,record["kind"]))
    capture.key("ENTER"); checkpoint("b7-stage2-resave-secondary",lambda f:menu.secondary(f,0))
    capture.key("LEFT"); checkpoint("b7-stage2-resave-selected",lambda f:menu.secondary(f,1))
    capture.key("ENTER"); checkpoint("b7-stage2-resave-yes",lambda f:menu.buttons(f,0))
    capture.key("ENTER"); measured = wait_measured_save(save,*guard)
    check = o.check_stage2_active_save(saved,measured,state,projection,saved,prepared["data"])
    guard[:] = [save.read_bytes(),save.stat().st_mtime_ns]
    path = capture.artifacts/"b7-stage2-reloaded.SAV";path.write_bytes(guard[0])
    record["completed_save"] = {"snapshot":str(path),"sha256":sha(path),"mtime_ns":guard[1],"measurement":check}
    record["active_saves"].append(record["completed_save"])
    checkpoint("b7-stage2-resaved-browse",lambda f:n.focus_match(f,original,saved,assets))
