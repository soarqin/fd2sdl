#!/usr/bin/env python3
"""B5自然START首关：真实117页→每轮实际Save只读规划→结算/酒店→下一关。

只用本端观察和正式输入；原版先正常清理再SDL。planner是意图，不是原版oracle。
未锁RNG，不重开赌结果；败北、停滞、任何未知资源门均保留prefix并正常清理。
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import time

from PIL import Image

from fd2_natural_oracle import (actor, actor_browse_anywhere, action_completion_resources, action_completion_match, command_match, cursor_bank,
    cursor_match, empty_tile, focus_match, navigation, range_match, range_probe, selection_summary,
    required_reward_pages, round_banner, semantic_digest, stage0_pages, validate_plan, view,
    visible_resource, visible_acted_resources)
from fd2_opening_oracle import build_pages, geometry, page_match as story_match
from fd2_reward_oracle import magic_page, page_match as reward_match
from fd2_loot_oracle import message_pages, message_match
from fd2_item_list_oracle import inventory_templates
from fd2_transition_oracle import (resources as prepare_assets, picker_image, picker_match,
    hotel_menu_match, notice_resources, notice_match, entry_pages, entry_geometry,
    entry_browse, check_entry_measurement, entry_initialized_records)
from run_graphics_tests import decode_save
from run_original_battle_menu import match_indexed, save_checkpoint, wait_state, wait_title
from run_original_comparison import Capture, archive_entry, hashes, running, title_templates
from run_original_end_turn import META, STRIDE, UNITS, RoundOracle, round_assets
from run_original_load import require_source_unchanged, wait_measured_save
from run_original_opening import exercise as opening_exercise, load_menu_availability, title_menu_counts
from run_original_player import exact_tile, screen, sheet_frame, upper_command_composite
from run_original_result import result_branch, result_templates
from run_original_reward import endpoint_guard
from run_original_transition import audio_toggle, collect_native_audio


class NaturalStopped(RuntimeError):
    pass


def read_plan(planner, root, save, *, stage=0):
    before, stamp = save.read_bytes(), save.stat().st_mtime_ns
    result = subprocess.run([str(planner), "--save", str(save.resolve())], cwd=root,
        text=True, encoding="utf-8", errors="strict", stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=30)
    require_source_unchanged(save, before, stamp)
    if result.returncode: raise NaturalStopped("只读planner失败：" + result.stderr.strip())
    plan = json.loads(result.stdout)
    validate_plan(plan, decode_save(save), stage=stage)
    return plan


def sparse_match(frame, page, matcher, assets):
    # 先用实际新行的前32px作短路，再进入完整原资源判据；不降低像素门。
    if "_needle" not in page:
        box = page["text"].point(lambda p: 255 if p else 0, mode="L").getbbox()
        if box is None: raise ValueError("自然提示资源为空")
        left, top, right, bottom = box
        page["_needle"]=(page["text"].crop((left,top,min(right,left+33),min(bottom,top+17))),(left,top))
    needle,(left,top)=page["_needle"]
    if not match_indexed(frame, needle, (left, top))["ok"]: return None
    return matcher(frame, page, assets)


class ProgressReader:
    """本首关有限的资源事件；只有已确认等键页才发送一键。"""
    def __init__(self, original, source, assets, report, *, stories=None, victory_fragment=9, cell_contract=None,
            nonphase_story_fragments=(8, 9)):
        self.original, self.source, self.assets, self.report = original, source, assets, report
        self.stories = stage0_pages(original, assets) if stories is None else stories
        self.victory_fragment = victory_fragment
        self.cell_contract = cell_contract
        self.nonphase_story_fragments = nonphase_story_fragments
        self.rewards = required_reward_pages(original, assets, source)
        self.fragments = {fragment: [p for p in self.stories if p["fragment"] == fragment]
                          for fragment in sorted({p["fragment"] for p in self.stories})}
        self.story = None
        self.reward = None
        self.last_ack = None
        self.ack_disappeared = True
        self.loot = []
        self.victory_text_complete = False
        self.defeat = result_templates(original, assets)
        metadata = archive_entry((original / "FDFIELD.DAT").read_bytes(), 3 * source[META + 2] + 1)
        portrait_ids={actor(source,i)[7] for i in range(source[META+1]) if actor(source,i)[6]==2 and not actor(source,i)[5]&1}
        portrait_ids.update(metadata[i+1] for i in range(0x83,len(metadata),26) if metadata[i] in (1,2))
        descriptors = {bytes(actor(source, i)[0x31:0x34]) for i in range(source[META + 1])}
        descriptors.update(metadata[i + 0x16:i + 0x19] for i in range(0x83, len(metadata), 26))
        from fd2_opening_oracle import portraits
        for descriptor in descriptors:
            mode, parameter = descriptor[0], int.from_bytes(descriptor[1:], "little")
            if mode not in (0, 1): continue
            step = {"fragment":432 if mode == 0 else 435,"open":True,"key":True,
                    "arrow":False,"item":parameter,"value":parameter}
            for identity in sorted(portrait_ids):
                page = message_pages(original, assets, [step])[0]
                page["portraits"] = portraits(original, identity, assets["palette"], True)
                page["portrait_id"] = identity; self.loot.append(page)
        # 满槽时不替换：完整匹配433后选No，避免自行选择/消耗未知物品。
        for identity in sorted(portrait_ids):
            page = message_pages(original, assets, [{"fragment":433,"open":True,"choice":True}])[0]
            page["portraits"] = portraits(original,identity,assets["palette"],True)
            page["portrait_id"] = identity; self.loot.append(page)

    def special(self, action):
        descriptor = action.get("wait_cell_action")
        if not descriptor: return
        index, mode, parameter = descriptor["index"], descriptor["mode"], descriptor["parameter"]
        raw = action["actor_raw"]
        metadata = self.source
        cell_mode, cell_param = struct.unpack_from("<BH", metadata, 0x53 + index * 3)
        if (mode,parameter) != (cell_mode,cell_param): raise ValueError("planner特殊格参数不等于真实Save metadata")
        from fd2_loot_oracle import cell_contract
        contract = (cell_contract if self.cell_contract is None else self.cell_contract)(self.original, tuple(action["destination"]))
        chest = bool(contract["terrain_flags"] & 0x20)
        question = 421 if chest else 428
        result = (422 if chest else 429) if mode == 0 else (426 if chest else 430)
        steps = [{"fragment":question,"open":True,"choice":True},
            {"fragment":result,"y":146,"key":True,"arrow":False,"item":parameter,"value":parameter}]
        if mode == 0: steps.append({"fragment":423,"open":True,"choice":True})
        if mode not in (0,1) or mode == 1 and not parameter:
            raise ValueError("自然特殊格未建模此callback或既存全局数字分支")
        from fd2_opening_oracle import portraits
        for page in message_pages(self.original,self.assets,steps):
            page["portrait_id"]=actor(self.source,raw)[7]
            page["portraits"]=portraits(self.original,page["portrait_id"],self.assets["palette"],True)
            self.loot.append(page)

    def message(self, frame):
        failed = result_branch(frame, "defeat", self.assets, self.defeat)
        if failed:return {"kind":"natural_actual_defeat","resource":failed}
        candidates = []
        stories = [] if self.reward else ([self.story[0][self.story[1]]] if self.story else
            [pages[0] for pages in self.fragments.values() if pages])
        for page in stories:
            found = sparse_match(frame,page,story_match,self.assets)
            if found: candidates.append((("story",page["fragment"],page["token_offset"]),page,found))
        if not self.story:
            rewards = [self.reward[0][self.reward[1]]] if self.reward else [p for p in self.rewards if p["fragment"]==489]
            for page in rewards:
                found = sparse_match(frame,page,reward_match,self.assets)
                if found: candidates.append((("reward",page["portrait_id"],page["fragment"]),page,found))
            for page in ([] if self.reward else self.loot):
                found = sparse_match(frame,page,message_match,self.assets)
                if found: candidates.append((("loot",page["portrait_id"],page["fragment"],page.get("item"),page.get("value")),page,found))
        # 相同真实片段可能被不同掉落描述符引用，按完整资源身份去重。
        unique={signature:(page,found) for signature,page,found in candidates}
        if len(unique)>1: raise NaturalStopped("自然画面同时命中多种提示合同；未发送猜测ACK")
        if not unique:
            self.ack_disappeared=True
            return None
        signature,(page,found)=next(iter(unique.items()))
        if signature==self.last_ack and not self.ack_disappeared:return None
        return {"kind":"natural_required_message","signature":list(signature),"page":page,"resource":found}

    def acknowledge(self, capture, detail, checkpoint):
        page, signature = detail["page"], tuple(detail["signature"])
        if signature[0]=="reward":
            if page["fragment"]==489:
                if self.reward:raise NaturalStopped("本级成长尚未完成，不确认新的LEVEL")
            elif (not self.reward or
                    (page["portrait_id"],page["fragment"]) !=
                    (self.reward[0][self.reward[1]]["portrait_id"],self.reward[0][self.reward[1]]["fragment"])):
                raise NaturalStopped("成长等待不在已确认LEVEL的原字段顺序内，未发键")
        if page.get("choice") and page["fragment"] in (423,433):
            capture.key("RIGHT")
            checkpoint("choice-no", lambda frame: message_match(frame,page,self.assets,1))
        capture.key("ENTER")
        self.last_ack, self.ack_disappeared = signature, False
        self.report["resource_acknowledgements"].append({"signature":list(signature),"key":"ENTER",
            "choice_no":bool(page.get("choice") and page["fragment"] in (423,433))})
        if signature[0] == "story":
            if self.story is None: self.story = (self.fragments[page["fragment"]],0)
            pages,index=self.story
            self.story=(pages,index+1) if index+1<len(pages) else None
            if self.story is None and page["fragment"]==self.victory_fragment:self.victory_text_complete=True
        elif signature[0]=="reward":
            if page["fragment"]==489:
                # 1e292先489，再按1e529非零字段顺序等待；累计旧行不是新通知。
                pages=[p for p in self.rewards if p["portrait_id"]==page["portrait_id"] and 490<=p["fragment"]<=494]
                self.reward=(pages,0)
            else:
                pages,index=self.reward
                self.reward=(pages,index+1) if index+1<len(pages) else None


def navigate(capture, original, source, assets, bank, destination, checkpoint, prefix):
    for i,(key,state) in enumerate(navigation(source,destination)):
        capture.key(key)
        checkpoint(f"{prefix}-{i}",lambda frame,s=state:cursor_match(frame,assets,bank,screen(s,s[META+5:META+7])))
        source=state
    return source


def save_active(capture, original, state, assets, bank, save, guard, report, checkpoint, prefix, *, stage=0):
    if stage not in (0, 1) or state[META + 2] != stage:
        raise NaturalStopped("自然活动保存只开放此实际stage0/1；stage2不得借用END路径")
    tile=empty_tile(original,state,assets,capture.last)
    state=navigate(capture,original,state,assets,bank,tile["world"],checkpoint,prefix+"-to-empty")
    at=screen(state,tile["world"])
    checkpoint(prefix+"-proven-empty",lambda frame:{"kind":"natural_empty_menu_cell",**hit}
        if (hit:=exact_tile(frame,tile["focused"],at)) else None)
    menu=RoundOracle(assets,at)
    capture.key("ENTER");checkpoint(prefix+"-parent",lambda frame:menu.menu(frame,0))
    capture.key("ENTER");checkpoint(prefix+"-secondary",lambda frame:menu.secondary(frame,0))
    capture.key("LEFT");checkpoint(prefix+"-save-selected",lambda frame:menu.secondary(frame,1))
    capture.key("ENTER");checkpoint(prefix+"-save-yes",lambda frame:menu.buttons(frame,0))
    before=decode_save(save)
    capture.key("ENTER");measured=wait_measured_save(save,*guard)
    guard[:]=[save.read_bytes(),save.stat().st_mtime_ns]
    checkpoint(prefix+"-saved-browse",menu.closed_browse)
    if (measured[META+2]!=stage or not (11 if stage == 1 else 1)<=measured[META+1]<=96 or
            not 1<=measured[META+9]<=32 or sum(measured[:-4])!=int.from_bytes(measured[-4:],"little")):
        raise NaturalStopped("实际活动Save不是checksum有效且私有handler安全的此stage；不再规划")
    if measured[0x312b:-4]!=before[0x312b:-4]:raise AssertionError("自然活动Save改写手工槽未知区域")
    if list(measured[META+3:META+9])!=list(state[META+3:META+9]):raise AssertionError("实际Save的camera/focus与逐键资源导航不同")
    if any(actor(measured,i)[5]&0x80 for i in range(measured[META+1]) if not actor(measured,i)[5]&1):
        raise NaturalStopped("实际Save仍有visible acted，不能冒称新玩家回合")
    snapshot=capture.artifacts/(prefix+".SAV");snapshot.write_bytes(guard[0])
    report["actual_turn_saves"].append({"turn":measured[META],"snapshot":str(snapshot),
        "sha256":hashlib.sha256(guard[0]).hexdigest(),"manual_slots_preserved":True,"planning_source":"actual UI Save only"})
    return measured


def target_candidates(source, action):
    raw=action["actor_raw"];caster=actor(source,raw);query=action["target_query"]
    maximum=query.get("physical_range_max",query.get("cast"));minimum=query.get("physical_range_min",query.get("minimum",0))
    if maximum is None or maximum>=16:raise ValueError("自然首关仅普通Manhattan cast，不猜十字特殊范围")
    filter_id=0 if action["kind"]=="attack" else query["filter"]
    if filter_id not in (0,1):raise ValueError("自然仅使用已证敌/友过滤")
    result=[]
    for i in range(source[META+1]):
        unit=actor(source,i)
        distance=abs(unit[0]-caster[0])+abs(unit[1]-caster[1])
        if not unit[5]&1 and minimum<=distance<=maximum and (unit[6]==0 if filter_id==0 else unit[6]!=0):result.append(i)
    return result


def perform_action(capture, original, state, action, assets, bank, reader, checkpoint, report, *, last=False, fresh=True, completion=None, hp_observation=None):
    raw=action["actor_raw"];world=tuple(actor(state,raw)[:2])
    # 所有本次动作range query先在纯导航参考上准备；资源/几何不受支持时不得先发导航键。
    steps=navigation(state,world);selection=steps[-1][1] if steps else state
    command_id={"attack":0,"magic":1,"herb":2,"wait":3}[action["kind"]]
    destination_steps=navigation(selection,action["destination"])
    destination_view=destination_steps[-1][1] if destination_steps else selection
    from run_original_battle_menu import MENU_OFFSETS
    destination_screen=screen(destination_view,action["destination"])
    required=[(command_id,3*command_id+1)]
    if action["kind"]=="herb":required.append((0,25))
    for index,frame_id in required:
        image=assets["commands"][frame_id]
        where=tuple(a+b for a,b in zip(destination_screen,MENU_OFFSETS[index]))
        visible=visible_resource(image,where)
        if visible is None or visible[2]!=(0,0,image.width,image.height):
            raise NaturalStopped("本动作所需亮图标在原viewport不可完整观察，当前自然协议未覆盖；未发送导航或选择键")
    if action["kind"]=="wait":
        visible_acted_resources(original,assets,actor(state,raw),(destination_screen[0],destination_screen[1]-6))
    stationary=tuple(action["destination"])==world and action.get("reachable_path_directions")==[]
    probes=range_probe(original,selection,assets,raw,allow_empty=stationary)
    summary=selection_summary(original,selection,assets,raw,hp_observation=hp_observation)
    if action["kind"]!="wait":
        steps=navigation(selection,action["destination"]);projected=bytearray(steps[-1][1] if steps else selection)
        projected[UNITS+raw*STRIDE:UNITS+raw*STRIDE+2]=bytes(action["destination"])
        candidates=target_candidates(projected,action)
        if not candidates:raise NaturalStopped("自然目标查询无候选；未发送动作导航键")
        first=candidates[0]
        if actor(projected,first)[7]==121:raise NaturalStopped("自然首候选unit121仅占index不聚焦；未发送动作导航键")
        steps=navigation(projected,tuple(actor(projected,first)[:2]));first_target=steps[-1][1] if steps else projected
        query=action["target_query"];cursor=assets["cursor"] if query["cursor_resource_frame"]==0 else sheet_frame(archive_entry((original/"FDOTHER.DAT").read_bytes(),1),1)
        cursor.putpalette(assets["palette"])
        radius=query.get("physical_range_max",query.get("cast"));minimum=query.get("physical_range_min",query.get("minimum",0))
        third_actors=tuple(i for i in range(projected[META+1]) if i not in (raw,first,action["target_raw"])) if action["kind"]=="attack" else ()
        first_probes=range_probe(original,first_target,assets,raw,radius=radius,minimum=minimum,physical_third_actors=third_actors)
        steps=navigation(first_target,tuple(actor(projected,action["target_raw"])[:2]));final_target=steps[-1][1] if steps else first_target
        final_probes=range_probe(original,final_target,assets,raw,radius=radius,minimum=minimum,physical_third_actors=third_actors)
        target_bank=cursor_bank(original,first_target,assets,cursor)
        if completion is not None:
            completion.update(action_completion_resources(original,final_target,assets,raw,bank,final_probes))
    state=navigate(capture,original,state,assets,bank,world,checkpoint,"to-actor")
    capture.key("ENTER")
    if probes:
        checkpoint("move-selection",lambda frame:{"kind":"natural_move_selection","cursor":focus,"range":r}
            if (focus:=focus_match(frame,original,state,assets,summary=summary)) and (r:=range_match(frame,probes)) else None)
    else:
        # 原地且query path为空的动作不要求额外空邻格。此门只证请求后的角色焦点，
        # 不宣称selection/range已证；下一次ENTER后必须完整COMMAND及本动作亮帧才可提交。
        checkpoint("stationary-selection-request",lambda frame:{"kind":"natural_stationary_selection_request",
            "cursor":focus,"range_compared":False,"movement_range_compared":False,"selection_state_proven":False,
            "selection_confirmation":"following complete COMMAND resource gate"}
            if (focus:=focus_match(frame,original,state,assets,summary=summary)) else None)
    state=navigate(capture,original,state,assets,bank,action["destination"],checkpoint,"selection-cursor")
    capture.key("ENTER")
    state=bytearray(state);state[UNITS+raw*STRIDE:UNITS+raw*STRIDE+2]=bytes(action["destination"])
    if fresh:
        checkpoint("command",lambda frame:command_match(frame,original,state,assets,raw,action["command_first_enabled"],action["command_disabled"]))
    else:
        # 同轮主动作可能杀死邻敌，REST不能把轮初query的Attack可用性当实时事实。
        # 179d5选中项在53C13=0时仍是普通帧；REST只确认实际可见图标且WAIT可用。
        # 不推断默认selected；177fc的DOWN绝对选择3，随后仍严格ACK亮帧10才确认。
        def rest_command(frame):
            result=command_match(frame,original,state,assets,raw,None)
            if result is None or result["matches"][3]["frame"] not in (9,10):return None
            return dict(result,kind="natural_rest_actual_command",default_selection_compared=False,
                source_query_not_used_for_post_effect_capability=True)
        checkpoint("rest-command",rest_command)
    key=("UP","LEFT","RIGHT","DOWN")[command_id]
    capture.key(key)
    checkpoint("command-selected",lambda frame:command_match(frame,original,state,assets,raw,command_id))
    if action["kind"]=="wait":
        reader.special(action)
        if last:audio_toggle(capture,report)
        capture.key("ENTER")
    else:
        capture.key("ENTER")
        if action["kind"]=="magic":
            record=actor(state,raw);ids=[8*b+i for b in range(5) for i in range(8) if record[0x1a+b]&(1<<i)]
            selected=ids.index(action["magic_id"])
            def spell(index):
                at,image=magic_page(original,record,assets,index)[0]
                return lambda frame:{"kind":"natural_magic_list","selected":index,"magic_ids":ids,**hit} if (hit:=exact_tile(frame,image,at)) else None
            checkpoint("magic-list0",spell(0))
            for index in range(1,selected+1):capture.key("DOWN");checkpoint(f"magic-list{index}",spell(index))
            capture.key("ENTER")
        elif action["kind"]=="herb":
            at=screen(state,actor(state,raw)[:2]);selected=0
            sprites=(original/"FDICON.B24").read_bytes();top={24:[],25:[]}
            for phase in range(12):
                sprite=sheet_frame(sprites,actor(state,raw)[7]*12+phase);sprite.putpalette(assets["palette"])
                for which in top:top[which].append(upper_command_composite(assets["commands"][which],sprite))
            # native/SDL目前已完整Use/Give/EQT/Drop，正式菜单0默认Use。
            def item_menu(frame,bright=False):
                from run_original_battle_menu import MENU_OFFSETS
                matches=[]
                for index in range(4):
                    where=tuple(a+b for a,b in zip(at,MENU_OFFSETS[index]))
                    images=[image for which in ((25,) if bright else (24,25)) for image in top[which]] if index==0 else [assets["commands"][24+3*index+code] for code in (0,2)]
                    hit=next((found for image in images if (found:=match_indexed(frame,image,where))["ok"]),None)
                    if hit is None:return None
                    matches.append(hit)
                return {"kind":"natural_item_menu","selected":0 if bright else None,
                    "default_selection_compared":False,"explicit_use_selected":bright,"matches":matches}
            # 1bbdc/173e7共享闪烁允许初始Use普通24；不把未观测default当亮25。
            # 1741c的UP明确选择Use，再完整ACK亮25，之后才进入列表。
            checkpoint("item-menu0",item_menu);capture.key("UP")
            checkpoint("item-menu-use-selected",lambda frame:item_menu(frame,True));capture.key("ENTER")
            _,templates,_=inventory_templates(original,actor(state,raw),assets)
            def inventory(index):
                at,image=templates["selected_"+str(index)]
                return lambda frame:{"kind":"natural_use_list","selected":index,**hit} if (hit:=exact_tile(frame,image,at)) else None
            checkpoint("item-list0",inventory(0))
            for index in range(1,action["item_slot"]+1):capture.key("DOWN");checkpoint(f"item-list{index}",inventory(index))
            capture.key("ENTER")
        state=first_target;probes=first_probes
        def target(frame):
            focus=focus_match(frame,original,state,assets,cursor=cursor);r=range_match(frame,probes)
            return {"kind":"natural_action_target","cursor":focus,"range":r} if focus and r else None
        checkpoint("target-first-raw",target)
        state=navigate(capture,original,state,assets,target_bank,tuple(actor(state,action["target_raw"])[:2]),checkpoint,"target-cursor")
        probes=final_probes
        checkpoint("target-final",target)
        if last:audio_toggle(capture,report)
        capture.key("ENTER")
    state=bytearray(state);state[UNITS+raw*STRIDE+5]|=0x80
    return state


def wait_progress(capture, original, state, source_turn, assets, reader, report, checkpoint, *, phase=False, ready_raw=None, ready_target=None):
    seen_banner=False;quiet={"first":None,"samples":0,"wgc_retries":0,"committed":None};counter=0
    banner=round_banner(original,assets,source_turn+1) if phase and source_turn<255 else None
    while counter<256:
        counter+=1
        def predicate(frame):
            nonlocal seen_banner
            retries=sum(row.get("kind")=="wgc_transient" for row in report.get("checkpoints",[]))
            if retries!=quiet["wgc_retries"]:quiet.update(first=None,samples=0,wgc_retries=retries)
            if phase and report["kind"]=="original" and not seen_banner:
                found=match_indexed(frame,banner,(120,84))
                if found["ok"]:
                    seen_banner=True
                    save_checkpoint(capture,f"natural-round-{source_turn+1:03d}-native-banner",frame,
                        {"kind":"native_round_banner","turn":source_turn+1,"resource":found},report["native_round_banners"])
            message=reader.message(frame)
            if message:
                quiet.update(first=None,samples=0)
                return message
            if reader.victory_text_complete:
                return {"kind":"natural_victory_dialogue_complete"}
            if getattr(reader,"story",None) is not None or getattr(reader,"reward",None) is not None:
                quiet.update(first=None,samples=0,committed=None)
                return None
            if phase and report["kind"]=="original" and not seen_banner:
                return None
            if phase and report["kind"]=="sdl":
                log=(capture.artifacts/"game.log").read_text(encoding="utf-8",errors="replace")
                if f"field phase: turn={source_turn+1} phase=2 " not in log:return None
            raw=0 if phase else ready_raw
            if not phase and ready_target:
                detail=action_completion_match(frame,state,assets,ready_target)
                found=(state,detail) if detail else None
            else:
                found=actor_browse_anywhere(frame,original,state,assets,raw,acted=False if phase else True)
            if found:
                current,detail=found;now=time.monotonic()
                committed=bool(detail.get("caster_acted_sprite")) if not phase and ready_target else True
                if not committed and detail.get("caster_unacted_sprite"):
                    # FIGANI刚回地图、acted尚未提交时仍可见原sprite，不能靠等待秒数补成提交。
                    quiet.update(first=None,samples=0,committed=None)
                    return None
                if quiet["committed"] is not committed:
                    quiet.update(first=None,samples=0,committed=committed)
                if quiet["first"] is None:quiet["first"]=now
                quiet["samples"]+=1
                if now-quiet["first"]>=2 and quiet["samples"]>=2:
                    if not committed:
                        raise NaturalStopped("确认target已连续闭合但未取得caster可见acted资源；当前自然完成门不覆盖其死亡/遮挡/不可见，不推断HP或hidden")
                    return {"kind":"natural_resource_quiet","state":current,"resource":detail,
                        "host_stability_seconds":2,"not_native_timing_equivalence":True}
            else:quiet.update(first=None,samples=0,committed=None)
            return None
        _,detail=checkpoint(f"settle-{counter}",predicate,timeout=180)
        if detail["kind"]=="natural_actual_defeat":
            raise NaturalStopped("实际自然战斗败北；完整资源检查点已保存，不重开或重抽RNG")
        if detail["kind"]=="natural_required_message":
            if not phase and detail["signature"][0]=="story" and detail["signature"][1] not in getattr(reader,"nonphase_story_fragments",(8,9)):
                raise NaturalStopped("非phase特殊剧情焦点合同未覆盖；未发送猜测ACK，未放宽完成格")
            reader.acknowledge(capture,detail,checkpoint)
        elif detail["kind"]=="natural_victory_dialogue_complete":return None
        else:
            if phase:audio_toggle(capture,report)
            return detail["state"]
    raise NaturalStopped("自然同一步提示超过有限上界，未继续发键")


def end_phase(capture, original, state, assets, bank, reader, report, checkpoint):
    tile=empty_tile(original,state,assets,capture.last)
    state=navigate(capture,original,state,assets,bank,tile["world"],checkpoint,"end-empty")
    oracle=RoundOracle(assets,screen(state,tile["world"]))
    checkpoint("end-proven-empty",lambda frame:{"kind":"natural_empty_menu_cell",**hit} if (hit:=exact_tile(frame,tile["focused"],oracle.focus)) else None)
    capture.key("ENTER");checkpoint("end-parent",lambda frame:oracle.menu(frame,0))
    capture.key("DOWN");checkpoint("end-selected",lambda frame:oracle.menu(frame,3))
    capture.key("ENTER");checkpoint("end-yes",lambda frame:oracle.buttons(frame,0))
    audio_toggle(capture,report);capture.key("ENTER")
    return state


def exercise(capture, original, planner, prepared, save, guard, report, max_turns):
    assets=prepared["assets"]
    # 此用例不验片头动画；沿既有startup-only ESC节拍跳过，精确标题出现即停。
    # 新游戏后的117页剧情仍逐资源ACK，受控CONTINUE不重播。
    wait_title(capture,prepared["titles"][report["title_menu_count"]][0],timeout=90)
    opening_exercise(capture,assets,prepared["opening"],prepared["titles"],save,*guard,prepared["geometry"],report)
    guard[:]=[save.read_bytes(),save.stat().st_mtime_ns]
    state=decode_save(save);digests={}
    report.update(actual_turn_saves=[],resource_acknowledgements=[],native_round_banners=[],natural_actions=[])
    bank=cursor_bank(original,state,assets)
    ordinal=0
    def checkpoint(name,predicate,**kwargs):
        nonlocal ordinal
        ordinal+=1
        frame,detail=wait_state(capture,f"natural-{ordinal:04d}-{name}",predicate,report["checkpoints"],**kwargs)
        require_source_unchanged(save,*guard)
        # 只写可JSON序列化的资源事实，PIL页与本地显示view留调用栈。
        saved=report["checkpoints"][-1]
        saved.pop("page",None);saved.pop("state",None)
        return frame,detail
    for round_index in range(max_turns):
        semantic=semantic_digest(state);digests[semantic]=digests.get(semantic,0)+1
        if digests[semantic]>=4:raise NaturalStopped("自然语义状态重复4次，实际停滞；未重开/重抽RNG")
        plan=read_plan(planner,prepared["root"],save)
        plan_path=capture.artifacts/f"turn-{state[META]:03d}-plan.json";plan_path.write_text(json.dumps(plan,ensure_ascii=False,indent=2),encoding="utf-8")
        report["natural_actions"].append({"turn":state[META],"plan":str(plan_path),"sha256":hashlib.sha256(plan_path.read_bytes()).hexdigest(),"planning_not_oracle":True})
        turn=state[META];reader=ProgressReader(original,state,assets,report)
        actions=[plan["next_action"]]+plan.get("rest_tail",[]);completed_heals={}
        for index,action in enumerate(actions):
            completion={}
            if action["kind"]=="end_turn":state=end_phase(capture,original,state,assets,bank,reader,report,checkpoint);phase=True
            else:
                phase=index==len(actions)-1 and bool(action.get("predicted_phase_advance_after_wait",len(actions)==1))
                observation=completed_heals.get(action["actor_raw"]) if index else None
                if observation is not None and observation["record_hex"]!=actor(state,action["actor_raw"]).hex():
                    raise NaturalStopped("本轮治疗目标80B身份已改变；未使用动态HP观察许可")
                state=perform_action(capture,original,state,action,assets,bank,reader,checkpoint,report,last=phase,fresh=index==0,completion=completion,hp_observation=observation)
            state=wait_progress(capture,original,state,turn,assets,reader,report,checkpoint,phase=phase,
                ready_raw=action.get("actor_raw",0),ready_target=completion or None)
            if state is None:
                report["natural_stage0_victory_dialogue_complete"]=True
                return
            # 只有正式确认后，wait_progress取得可见caster acted提交锚并正常返回（完整性另列），
            # 才给同轮该目标后续REST的简表许可；不是单凭计划或预测治疗数值。
            if not phase and completion:
                source_actor=bytes.fromhex(action.get("actor_record_hex",""))
                herb192=(action["kind"]=="herb" and 0<=action.get("item_slot",-1)<8 and
                    len(source_actor)==STRIDE and source_actor[0x0b+2*action["item_slot"]]==192)
                if herb192 or action["kind"]=="magic" and action.get("magic_id")==13:
                    target=action["target_raw"]
                    completed_heals[target]={"raw":target,"record_hex":actor(state,target).hex(),
                        "actual_completed_heal":True,"kind":"herb192/code5" if herb192 else "magic13",
                        "caster_raw":action["actor_raw"],"source_turn":turn}
        if not phase:
            state=end_phase(capture,original,state,assets,bank,reader,report,checkpoint)
            state=wait_progress(capture,original,state,turn,assets,reader,report,checkpoint,phase=True)
            if state is None:report["natural_stage0_victory_dialogue_complete"]=True;return
        state=save_active(capture,original,state,assets,bank,save,guard,report,checkpoint,f"turn-{turn+1:03d}")
    raise NaturalStopped("自然首关达到本次max-turns边界；保留实际结果且不重开")


def after_victory(capture,original,prepared,save,guard,report):
    assets=prepared["assets"];ui=prepare_assets(original)
    from run_original_transition import town_match
    roster_hint={"roster":decode_save(save)[0x8a3:0x12a3],"count":5,"stage":1}
    from fd2_transition_oracle import town_images,SLOT_BASE,ROSTER_SIZE,SLOT_STRIDE
    town={"assets":ui,"towns":[town_images(original,ui,roster_hint,i) for i in range(5)]}
    notices={i:notice_resources(original,ui,i) for i in (585,586,660)}
    notices[513]=notice_resources(original,ui,513,portrait=75,origin=(95,119))
    def cp(name,predicate,**kwargs):
        result=wait_state(capture,"natural-transition-"+name,predicate,report["checkpoints"],**kwargs)
        require_source_unchanged(save,*guard);return result
    cp("town0",lambda frame:town_match(frame,town,0),timeout=90)
    report["natural_settlement_reached_town"]=True
    def hotel(selected,fragment):
        def match(frame):
            menu=hotel_menu_match(frame,ui,selected);notice=notice_match(frame,notices[fragment],assets)
            return dict(menu,notice=notice) if menu and notice else None
        return match
    capture.key("ENTER");cp("hotel",hotel(0,585))
    capture.key("RIGHT");cp("hotel-save1",hotel(1,585))
    capture.key("ENTER")
    plain=decode_save(save)
    # roster/资源属于真实自然过程，保存前不捏造尚未观测的终战HP/EX或malloc尾。
    # 初次picker只要求已存在的slot0完整资源，写入后独立读出真实persistent。
    cp("manual-picker0",lambda frame:picker_match(frame,picker_image(ui,plain,0)),allow_low_color_resource=True)
    capture.key("ENTER");measured=wait_measured_save(save,*guard)
    allowed=set(range(SLOT_BASE,SLOT_BASE+ROSTER_SIZE+10))|set(range(len(plain)-4,len(plain)))
    if any(a!=b and i not in allowed for i,(a,b) in enumerate(zip(plain,measured))):
        raise AssertionError("自然酒店手工Save改写slot0+metadata/checksum之外的字节")
    slot=measured[SLOT_BASE:SLOT_BASE+SLOT_STRIDE]
    if slot[ROSTER_SIZE]!=1 or slot[ROSTER_SIZE+1]!=5:
        raise NaturalStopped("自然结算未形成已确认stage1五人准备；保留实际SAV")
    state={"stage":1,"count":5,"roster":bytes(slot[:ROSTER_SIZE]),
        "resource":struct.unpack_from("<I",slot,ROSTER_SIZE+2)[0],"options":bytes(slot[ROSTER_SIZE+6:ROSTER_SIZE+10])}
    identities=[(state["roster"][80*i+7],state["roster"][80*i+8]) for i in range(5)]
    if identities[:4]!=[(0,0),(9,9),(4,4),(30,30)] or identities[4][1]!=1:
        raise AssertionError("自然持久角色顺序不是实际START四人+剧情角色1")
    guard[:]=[save.read_bytes(),save.stat().st_mtime_ns];plain=measured
    snapshot=capture.artifacts/"natural-settlement-manual0.SAV";snapshot.write_bytes(guard[0])
    report["settlement_manual_save"]={"snapshot":str(snapshot),"sha256":hashlib.sha256(guard[0]).hexdigest(),
        "stage":1,"count":5,"identities":identities,"resource":state["resource"],
        "whole_write_scope_checked":True,"roster_source":"actual native/SDL natural settlement Save, not ctor fixture",
        "unknown_roster_bytes_not_required_equal_between_endpoints":True}
    cp("save660",lambda frame:notice_match(frame,notices[660],assets));capture.key("ENTER")
    cp("saved-picker0",lambda frame:picker_match(frame,picker_image(ui,plain,0)),allow_low_color_resource=True)
    capture.key("ESC");cp("save-cancelled",hotel(1,586))
    capture.key("ESC");cp("hotel-to-town",lambda frame:town_match(frame,town,0))
    for i in (1,2):capture.key("LEFT");cp(f"town-select{i}",lambda frame,j=i:town_match(frame,town,j))
    buttons=RoundOracle(assets,(0,0))
    def deploy(frame):
        notice=notice_match(frame,notices[513],assets);choice=buttons.buttons(frame,0)
        return dict(choice,notice=notice) if choice and notice else None
    capture.key("ENTER");cp("deploy-yes",deploy)
    pages=entry_pages(original,assets,state);geometry=entry_geometry(original,state)
    initialized=entry_initialized_records(original,state,geometry)
    capture.key("ENTER")
    report["natural_entry_acknowledgements"]=[]
    for i,page in enumerate(pages):
        cp(f"entry-{i:02d}",lambda frame,p=page:story_match(frame,p,assets),timeout=90)
        report["natural_entry_acknowledgements"].append({"fragment":page["fragment"],"offset":page["token_offset"],"portrait":page["portrait_id"]})
        capture.key("ENTER")
    _,first=cp("entry-first-browse",lambda frame:entry_browse(frame,assets,geometry),timeout=90)
    x,y=first["focus_screen"]
    if x<52 or not 28<=y<=148:raise NaturalStopped("自然下一关没有已确认LEFT测量焦点，未补键")
    menu=RoundOracle(assets,(x-24,y));capture.key("LEFT");cp("entry-left",menu.closed_browse)
    capture.key("ENTER");cp("entry-parent",lambda frame:menu.menu(frame,0))
    capture.key("ENTER");cp("entry-secondary",lambda frame:menu.secondary(frame,0))
    capture.key("LEFT");cp("entry-save",lambda frame:menu.secondary(frame,1))
    capture.key("ENTER");cp("entry-save-yes",lambda frame:menu.buttons(frame,0))
    capture.key("ENTER");measured=wait_measured_save(save,*guard)
    report["entry_measurement"]=check_entry_measurement(measured,plain,state,geometry,first,
        archive_entry((original/"FDFIELD.DAT").read_bytes(),4),initialized)
    guard[:]=[save.read_bytes(),save.stat().st_mtime_ns]
    snapshot=capture.artifacts/"natural-stage1-entry.SAV";snapshot.write_bytes(guard[0])
    report["completed_save"]={"snapshot":str(snapshot),"sha256":hashlib.sha256(guard[0]).hexdigest(),"mtime_ns":guard[1]}
    cp("entry-saved-browse",menu.closed_browse)


def compare_entry_measurements(first, second):
    """11506保留实际阵亡，1088d/32d18仍给同一raw确定的入场身份/几何。"""
    for endpoint in (first,second):
        if (endpoint.get("complete_persistent_roster_equal") is not True or
                endpoint.get("manual_slots_and_unused_active_meta_tails_preserved") is not True or
                len(endpoint.get("initialized_actor_bytes_checked",[]))!=21):
            raise AssertionError("自然entry尚未通过本端完整roster/初始化/保存守卫")
        if (endpoint.get("stage"),endpoint.get("turn"),endpoint.get("active_count"))!=(1,1,21):
            raise AssertionError("自然下一关不是已验证stage1初态")
    for key in ("stage","turn","active_count","camera","first_focus","measuring_focus"):
        if first[key]!=second[key]:raise AssertionError("自然下一关几何不同："+key)
    left,right=first["actors"],second["actors"]
    if len(left)!=21 or len(right)!=21:raise AssertionError("自然entry演员列表不完整")
    outcomes=[]
    for raw,(a,b) in enumerate(zip(left,right)):
        if a.get("raw_actor")!=raw or b.get("raw_actor")!=raw:raise AssertionError("自然entry raw顺序不同")
        if raw==0 and (a.get("hidden") is not False or b.get("hidden") is not False):
            raise AssertionError("自然胜利交接主角必须存活")
        fixed_a,fixed_b=dict(a),dict(b)
        if 1<=raw<5:
            outcomes.append({"raw_actor":raw,"unit_id":a["unit_id"],"original_hidden":a["hidden"],
                "sdl_hidden":b["hidden"],"equal":a["hidden"]==b["hidden"],
                "source":"each endpoint actual persistent roster, already checked against full entry record"})
            fixed_a.pop("hidden");fixed_b.pop("hidden")
        if fixed_a!=fixed_b:raise AssertionError(f"自然下一关actor{raw}固定身份/几何不同")
    return {"entry_geometry_equal":True,"actual_player_death_outcomes":outcomes,
        "random_player_hidden_excluded_from_fixed_geometry":[1,2,3,4],"NPC_hidden_compared":True,
        "whole_save_equal_claimed":False,"random_battle_outcomes_allowed_to_differ":True,"shared_rng_aligned":False}


def run(args):
    root,output=args.root.resolve(),args.artifacts.resolve();original=(root/"original_game").resolve()
    if output==original or original in output.parents:raise ValueError("自然证据不能写入original_game")
    if not args.planner.is_file() or args.sdl is not None and not args.sdl.is_file():
        raise ValueError("planner/SDL文件不存在，未创建产物或窗口")
    if not 1<=args.max_turns<=100:raise ValueError("自然max-turns需1..100，不允许无限重开/等待")
    assets=round_assets(original);source_path=original/"FD2.SAV";source=decode_save(source_path);source_bytes=source_path.read_bytes()
    prepared={"root":root,"assets":assets,"opening":build_pages(original,assets),"geometry":geometry(original),
        "titles":title_templates(original/"FDOTHER.DAT")}
    counts=title_menu_counts(source);availability=load_menu_availability(source)
    before=hashes(original);stamps={name:(original/name).stat().st_mtime_ns for name in before}
    planner_sha=hashlib.sha256(args.planner.read_bytes()).hexdigest()
    output.mkdir(parents=True,exist_ok=False)
    report={"ok":False,"probe_only":args.sdl is None,"probe_cases_passed":False,"status":"failed",
        "scenario":"natural-stage0","capture_backend":"Windows.Graphics.Capture HWND",
        "scope":"real START117 pages -> complete first battle -> full victoryfragment9 -> hotel -> next stage entry actual Save",
        "strategy":"one queried primary plus same-turn WAIT rest; Hero may legally reposition for safety, other REST remains stationary; actual per-round UI Save; no seed patch/reroll/restart",
        "excluded":"RNG stream/timing/full map or full audio equivalence; intermediate HP estimates are never an oracle",
        "original_before":before,"cases":[],"planner":{"path":str(args.planner.resolve()),"sha256":planner_sha,"planning_not_original_oracle":True}}
    report["binaries"]={name:{"path":str(path.resolve()),"sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
        for name,path in (("driver",args.driver),("dosbox",args.dosbox))+((("sdl",args.sdl),) if args.sdl else ())}
    project=Path(__file__).resolve().parents[1];report["tool_sources"]={}
    for name,module in tuple(sys.modules.items()):
        file=getattr(module,"__file__",None)
        if not file:continue
        path=Path(file).resolve()
        if path.suffix==".py" and (path.is_relative_to(project/"tools") or path.is_relative_to(project/"src/tests")):
            report["tool_sources"][name]={"path":str(path),"sha256":hashlib.sha256(path.read_bytes()).hexdigest()}
    guards={}
    try:
        for role in (("original",) if args.sdl is None else ("original","sdl")):
            work=output/role;work.mkdir();env=os.environ.copy()
            for name in ("SDL_VIDEODRIVER","SDL_AUDIODRIVER","FD2SDL_FIELD_RNG_SEED"):env.pop(name,None)
            if role=="original":
                game=work/"game";shutil.copytree(original,game);save=game/"FD2.SAV"
                audio=bool(args.record_native_audio);captures=work/"captures";captures.mkdir()
                config=work/"dosbox.conf";config.write_text(
                    "[sdl]\nfullscreen=false\noutput=surface\nshowmenu=false\nautolock=false\nwaitonerror=false\n"
                    f"[dosbox]\nmachine=svga_s3\nmemsize=16\nquit warning=false\ncaptures={captures.as_posix()}\nshow recorded filename=false\n"
                    "[mixer]\nrate=44100\n[render]\naspect=false\nscaler=normal2x\n[cpu]\ncore=normal\ncycles=fixed 20000\n"
                    f'[autoexec]\nmount c "{game}"\nc:\nFD2.EXE\nexit\n',encoding="utf-8")
                command,cwd=[str(args.dosbox.resolve()),"-conf",str(config),"-noconsole"],work
            else:
                save=work/"FD2.SAV";save.write_bytes(source_bytes);env["FD2SDL_SAVE_PATH"]=str(save);audio=False
                command,cwd=[str(args.sdl.resolve())],root
            guard=[save.read_bytes(),save.stat().st_mtime_ns];guards[role]=(save,guard)
            case={"kind":role,"status":"failed","probe_cases_passed":False,"checkpoints":[],"inputs":[],"processes":[],
                "title_menu_count":counts[role],"load_available":availability[role],
                "native_audio":{"enabled":audio,"started":False,"stopped":False,"scope":"first real phase-cycle recording only; expected primary19/alternate12/primary19, not A7 equivalence"}}
            report["cases"].append(case)
            try:
                with running(command,cwd,env,work/"game.log",args.driver.resolve(),case["processes"]) as process:
                    capture=Capture(args.driver.resolve(),process,work,case["inputs"],startup_skip=True)
                    try:
                        exercise(capture,original,args.planner.resolve(),prepared,save,guard,case,args.max_turns)
                        if not case.get("natural_stage0_victory_dialogue_complete"):raise NaturalStopped("自然首关未完成")
                        after_victory(capture,original,prepared,save,guard,case)
                        require_source_unchanged(save,*guard);case["business_sequence_completed"]=True
                    finally:
                        failed=sys.exc_info()[0] is not None
                        try:audio_toggle(capture,case,cleanup=True)
                        except Exception as error:
                            case["audio_cleanup_error"]=str(error)
                            if not failed:raise
                case.update(status="passed",probe_cases_passed=True)
            except Exception as error:case["error"]=str(error)
            finally:
                try:collect_native_audio(case,work)
                except Exception as error:case.setdefault("error",str(error));case["probe_cases_passed"]=False
                endpoint_guard(case,original,before,source_path,source_bytes,stamps["FD2.SAV"],save,guard)
                case["original_all_mtimes_unchanged"]=all((original/name).stat().st_mtime_ns==stamp for name,stamp in stamps.items())
                case["planner_unchanged"]=hashlib.sha256(args.planner.read_bytes()).hexdigest()==planner_sha
                case["binary_tools_unchanged"]=all(hashlib.sha256(Path(row["path"]).read_bytes()).hexdigest()==row["sha256"]
                    for row in list(report["binaries"].values())+list(report["tool_sources"].values()))
                if not all(case[key] for key in ("original_all_mtimes_unchanged","planner_unchanged","binary_tools_unchanged")):
                    case.update(status="failed",probe_cases_passed=False)
                    case.setdefault("error","本端退出时原版mtime/planner/冻结二进制工具守卫失败，不再启动下一端")
                case["capture_hashes"]={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in work.iterdir()
                    if p.is_file() and p.suffix.lower() in (".bmp",".png",".sav",".json")}
                (work/"comparison.json").write_text(json.dumps(case,ensure_ascii=False,indent=2),encoding="utf-8")
            if not case["probe_cases_passed"]:raise NaturalStopped(case["error"])
        if args.sdl is not None:
            first,second=report["cases"]
            report["paired"]={"both_actual_natural_victory_to_stage1":True,
                **compare_entry_measurements(first["entry_measurement"],second["entry_measurement"])}
        report.update(status="passed",probe_cases_passed=True)
    except Exception as error:report.setdefault("error",str(error))
    finally:
        checks={"original_unchanged":hashes(original)==before,
            "original_all_mtimes_unchanged":all((original/name).stat().st_mtime_ns==stamp for name,stamp in stamps.items()),
            "planner_unchanged":hashlib.sha256(args.planner.read_bytes()).hexdigest()==planner_sha,
            "binary_tools_unchanged":all(hashlib.sha256(Path(row["path"]).read_bytes()).hexdigest()==row["sha256"]
                for row in list(report["binaries"].values())+list(report["tool_sources"].values()))}
        report.update(checks)
        for case in report["cases"]:
            save,guard=guards[case["kind"]]
            endpoint_guard(case,original,before,source_path,source_bytes,stamps["FD2.SAV"],save,guard)
            (output/case["kind"]/"comparison.json").write_text(json.dumps(case,ensure_ascii=False,indent=2),encoding="utf-8")
        report["probe_cases_passed"] &= all(checks.values()) and all(c["probe_cases_passed"] for c in report["cases"])
        report["ok"]=args.sdl is not None and len(report["cases"])==2 and report["probe_cases_passed"]
        if not report["probe_cases_passed"]:report["status"]="failed"
        (output/"comparison.json").write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding="utf-8")
    return report


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ("root","driver","dosbox","planner","artifacts"):parser.add_argument("--"+name,type=Path,required=True)
    parser.add_argument("--sdl",type=Path);parser.add_argument("--max-turns",type=int,default=50)
    parser.add_argument("--record-native-audio",action="store_true")
    parser.add_argument("--scenario",choices=("natural-stage0",),default="natural-stage0")
    args=parser.parse_args()
    if sys.platform!="win32":parser.error("自然首关实机要求Windows HWND WGC")
    try:
        report=run(args);print(json.dumps(report,ensure_ascii=False,indent=2));return 0 if report["probe_cases_passed"] else 1
    except Exception as error:
        print(f"自然首关未执行完成：{error}。保留本轮现场，不自动重开或重抽RNG。",file=sys.stderr);return 1


if __name__=="__main__":raise SystemExit(main())
