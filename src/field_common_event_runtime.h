/* G4有限共用callback续行：348ea/35123/35854/35a0d与352ca/35346/
 * 355f0/35730（原sub_*），完整分支/等待/提交顺序来自common_event冷图。
 * 仅由field_game.c在loot领域之后包含，不复制Game/资源/RNG或持久owner。 */
/* 本域完整来源登记（不是具体stage绑定或完整剧情已开放声明）：
 * field_event_state16_ai7_once @0x348ea（原sub_348EA）；
 * field_event_item89_grant_once @0x35123（原sub_35123）；
 * field_event_side_state_turn_slot2_once @0x35e0e（原sub_35E0E）；
 * field_event_turn_slot2_random_pair @0x3623c（原sub_3623C）；
 * field_event_cell_reward_complete_first_five @0x35854（原sub_35854）；
 * field_event_key208_recruit31 @0x35a0d（原sub_35A0D）；
 * field_event_item211_story11 @0x352ca（原sub_352CA）；
 * field_event_story3_item213_story4 @0x35346（原sub_35346）；
 * sub_355F0 @0x355f0：item101后story3，旧story11归属已撤回；
 * field_event_actor17_exchange_item11_once @0x35730（原sub_35730）；
 * field_event_story5_defeat_tail18 @0x35677（原sub_35677）。 */
#ifndef FD2_FIELD_COMMON_EVENT_RUNTIME_H
#define FD2_FIELD_COMMON_EVENT_RUNTIME_H

#include "field_common_event_flow.h"

/* 10b4e的DWORD参数与模板BYTE零扩展比较；不继承旧stage夹具的
 * duplicate/no-new门。一次构造仍由现有units原语保证半组不交接。 */
static int common_event_append_group_wide(fd2_field_game *game, uint32_t group, int force) {
    if (!game || (force != 0 && force != 1) || game->units.count > FD2_FIELD_MAX_UNITS) return -1;
    size_t templates;
    if (fd2_field_unit_template_count(&game->metadata, &templates) != 0) return -1;
    if (group > UINT8_MAX) return 0;
    int match = 0;
    for (size_t i = 0; i < templates; i++)
        if (game->metadata.unit_templates[i].bytes[0x15] == group) match = 1;
    if (!match) return 0; /* 原10b4e空匹配不读地图/placement，更不拒绝整事件。 */
    fd2_field_units units = game->units;
    if (fd2_field_units_append_group_placed(&units, &game->metadata,
        &game->placements, &game->map, (uint8_t)group, force) != 0) return -1;
    for (size_t i = game->units.count; i < units.count; i++)
        /* 图形源越界由renderer有界省绘制；不能撤销已证10b4e业务新增。 */
        if (fd2_field_unit_combat_stats_recompute(&units.items[i], fd2_field_unit_original_item_stats,
                NULL, fd2_field_unit_original_status_scale, NULL) != 0) return -1;
    game->units = units;
    return 0;
}

static int common_event_leaf(const char *op) {
    static const char *const leaves[] = { "story", "clear_keys", "notice_actor", "message",
        "portrait_frame", "wait_key", "close_box", "map_replace", "raw_animation", "pan",
        "move", "refresh_map", "background_rebuild", "brightness", "delay_ms",
        "arrival_prepare", "arrival_frames", "arrival_close", "palette_add", "palette_subtract", "delay_ticks",
        "focus_cell", "teleport_begin", "call_teleport" };
    for (size_t i = 0; i < sizeof(leaves)/sizeof(leaves[0]); i++)
        if (!strcmp(op, leaves[i])) return 1;
    return 0;
}

/* 将已证固定来源投影为既有领域节点；无通用变量或表达式解释。 */
static int common_event_literal(const fd2_field_game *game,
        const fd2_static_flow_node *source, fd2_static_flow_node *literal) {
    if (!game || !source || !literal) return -1;
    *literal = *source;
    if (!strcmp(source->op, "story_source")) {
        uint32_t value;
        if (fd2_field_event_source_value((fd2_field_event_value_source)source->args[0],
            game->turn_number, game->cell_action_completed, &value) != 0 || value > UINT16_MAX) return -1;
        literal->op = "story"; literal->args[0] = (int32_t)value;
    } else if (!strcmp(source->op,"teleport_begin")) {
        uint32_t actor;
        if (common_event_teleport_actor(source->args[0],game->cell_action_completed,&actor)!=0 ||
                actor>=game->units.count) return -1;
        literal->args[0]=(int32_t)actor;
    }
    return 0;
}

static int common_event_preflight_leaf_view(const fd2_field_game *game,const fd2_static_flow_node *node,
        size_t actor,const fd2_field_event_resource_view *view) {
    fd2_static_flow_node literal = *node;
    if (!strcmp(literal.op, "notice_actor") && literal.args[0] == -1) literal.args[0] = (int32_t)actor;
    return !game->event_preflight || !game->event_preflight(game,&literal,view,game->event_preflight_userdata) ? 0:-1;
}

static int common_event_preflight_leaf(const fd2_field_game *game,const fd2_static_flow_node *node,size_t actor) {
    return common_event_preflight_leaf_view(game,node,actor,NULL);
}

#include "field_common_event_projection.h"

static int common_event_preflight_flow(const fd2_field_game *game, const fd2_static_flow *flow, size_t actor) {
    if (!game || !flow || !fd2_static_flow_at(flow,flow->entry)) return -1;
    /* 单一有界投影同时守核心raw/写点和可选视觉资源门；
     * 即使未注册UI callback，move等已知源仍必须核原raw索引。 */
    return common_event_preflight_identity(game,flow,actor)>0?0:-1;
}

static int common_event_run(fd2_field_game *game);

static int common_event_invalid_deferred(const fd2_field_game *game) {
    for(size_t i=0;i<game->event_log_count;i++)
        if(game->event_log[i].presentation_deferred)return 1;
    return 0;
}

/* 失败保留当前PC/已提交业务字段；不能重启entry假造可重试事务。 */
static int common_event_fail(fd2_field_game *game) {
    if (game) fail_action(game, game->event_resume.actor);
    return -1;
}

static int common_event_finish(fd2_field_game *game) {
    unsigned from_loot = game->event_resume.from_loot;
    unsigned completion=game->event_resume.completion;
    size_t actor=game->event_resume.actor,next=game->event_resume.next_slot;
    int anchor_x=game->event_resume.march_anchor_x,anchor_y=game->event_resume.march_anchor_y;
    uint32_t frame_ms=game->event_resume.march_frame_ms;
    fd2_vga *vga=game->event_resume.march_vga;
    memset(&game->event_resume, 0, sizeof(game->event_resume));
    if(from_loot==1)return loot_flow_resume_event_callback(game);
    if(completion==FD2_EVENT_COMPLETE_TURN) {
        record_turn_events_from(game,next);
        if(game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
        if(!game->event_resume.active)apply_phase_status_tick(game,game->active_side);
    } else if(completion==FD2_EVENT_COMPLETE_PLAYER_PENDING) {
        return finish_player_controller_input(game,actor);
    } else if(completion==FD2_EVENT_COMPLETE_PLAYER_RESULT) {
        return complete_player_controller_after_result(game,actor);
    } else if(completion==FD2_EVENT_COMPLETE_AI_RESULT) {
        return finish_ai_outer_after_result(game,actor);
    } else if(completion==FD2_EVENT_COMPLETE_STATUS_RESULT) {
        log_stage_result_return(game,"status",0);
        return advance_status_progress(game);
    } else if(completion==FD2_EVENT_COMPLETE_LOOT_RESULT) {
        log_stage_result_return(game,"ai-loot",actor);
        return loot_flow_resume_event_result(game);
    } else if(completion==FD2_EVENT_COMPLETE_AI_PENDING) {
        return finish_ai_outer_result(game,actor);
    } else if(completion==FD2_EVENT_COMPLETE_LOOT_PENDING)return loot_flow_resume_event_pending(game);
    else if(completion==FD2_EVENT_COMPLETE_MARCH) {
        if(actor>=game->units.count){fail_action(game,actor);return -1;}
        int reset=run_turn_entry(game,FD2_FIELD_TURN_DIRECTIONS_RESET);
        if(reset<0){fail_action(game,actor);return -1;}
        if(reset>0)return 0;
        fd2_field_unit_set_acted(&game->units.items[actor],1);
        game->selected_unit=game->command_selected=-1;game->interaction=FD2_FIELD_INTERACTION_BROWSE;
        return march_continue(game,vga,frame_ms,anchor_x,anchor_y,next);
    }
    return game->failure==FD2_FIELD_FAILURE_NONE?0:-1;
}

static int common_event_advance(fd2_field_game *game, unsigned port) {
    fd2_field_event_resume *r = &game->event_resume;
    const fd2_static_flow_node *node = fd2_static_flow_at(r->flow, r->pc);
    int next = fd2_static_flow_edge(r->flow, node, port);
    if (next < 0) return common_event_fail(game);
    r->pc = (int16_t)next;
    return 0;
}

static int common_event_child_resume(fd2_field_game *game) {
    int parent_pc; size_t actor;
    if (!game || game->loot.child.active ||
        fd2_field_event_child_finish(&game->event_resume, 1, &parent_pc, &actor) != 0 ||
        common_event_advance(game, FD2_FLOW_ACK) != 0) return common_event_fail(game);
    memset(&game->loot.child, 0, sizeof(game->loot.child));
    return common_event_run(game);
}

static int common_event_run(fd2_field_game *game) {
    if (!game || !game->ready || !game->event_resume.active || game->units.count > FD2_FIELD_MAX_UNITS ||
        game->failure != FD2_FIELD_FAILURE_NONE) return -1;
    fd2_field_event_resume *r = &game->event_resume;
    if(game->runtime_host_quit)return 0;
    if (r->child_wait) return game->loot.child.active ? 0 : common_event_child_resume(game);
    for (unsigned budget = 0; budget < FD2_FLOW_STEP_BUDGET; budget++) {
        if(game->runtime_host_quit)return 0;
        const fd2_static_flow_node *node = fd2_static_flow_at(r->flow, r->pc);
        if (!node || !node->op) return common_event_fail(game);
        if (!strcmp(node->op, "end")) return common_event_finish(game);
        if (!strcmp(node->op, "fail")) return common_event_fail(game);
        fd2_static_flow_node literal;
        if (common_event_literal(game, node, &literal) != 0) return common_event_fail(game);
        if (common_event_leaf(literal.op)) {
            if (common_event_preflight_leaf(game, &literal, r->actor) != 0) return common_event_fail(game);
            return 0;
        }
        unsigned port = FD2_FLOW_NEXT;
        int handled = common_event_domain(game, r->actor, node, &port, 0);
        if (handled < 0) port = FD2_FLOW_ERROR;
        else if (!handled) {
            const int32_t *a = node->args;
            int result = -1;
            if (!strcmp(node->op, "completed_cells")) result = complete_cell_tiles(game);
            else if(!strcmp(node->op,"display_mode")) {
                if(a[0]<0 || a[0]>1)return common_event_fail(game);
                game->ordinary_display_mode=(uint8_t)a[0];result=0;
            } else if(!strcmp(node->op,"directions_reset")) {
                for(size_t i=0;i<game->units.count;i++)game->units.items[i].direction=0;
                result=0;
            } else if(!strcmp(node->op,"ai_anchor")) {
                if(a[0]<0 || a[1]<a[0] || (size_t)a[1]>=game->units.count ||
                    a[2]<0 || a[2]>255 || a[3]<0 || a[3]>255)return common_event_fail(game);
                for(size_t i=(size_t)a[0];i<=(size_t)a[1];i++) {
                    game->units.items[i].ai_param_35=(uint8_t)a[2];
                    game->units.items[i].ai_param_36=(uint8_t)a[3];
                }
                result=0;
            }
            else if (!strcmp(node->op, "roster_append")) result = append_persistent_player(game, (uint8_t)a[0]);
            else if (!strcmp(node->op, "roster_sync")) {
                fd2_save_manual_state persistent = {0};
                if (game->roster_count <= FD2_SAVE_BATTLE_MAX_ROSTER) {
                    persistent.unit_count = (uint8_t)game->roster_count;
                    memcpy(persistent.unit_area, game->roster, sizeof(game->roster));
                    result = settlement_roster_sync(&game->units, &persistent);
                    if (!result) memcpy(game->roster, persistent.unit_area, sizeof(game->roster));
                }
            } else if (!strcmp(node->op, "grant_party_item")) {
                result = fd2_field_item_record_get((uint8_t)a[0]) ? 0 : -1;
                if (!result) for (size_t i = 0; i < game->units.count; i++)
                    if (game->units.items[i].side == 2 &&
                        inventory_first_empty_add(&game->units.items[i], (uint8_t)a[0]) == 1) break;
            } else if (!strcmp(node->op, "append_group"))
                result = common_event_append_group_wide(game, (uint32_t)a[0], a[1]);
            else if (!strcmp(node->op, "append_group_source")) {
                uint32_t value;
                result = fd2_field_event_source_value((fd2_field_event_value_source)a[0],
                    game->turn_number, game->cell_action_completed, &value);
                if (!result) result = common_event_append_group_wide(game, value, a[1]);
            } else if (!strcmp(node->op, "physical_exchange"))
                result = fd2_field_game_resolve_event_exchange(game, (size_t)a[0], (size_t)a[1]);
            else if (!strcmp(node->op, "finalize_defeated")) result=finalize_zero_hp_units(game);
            else if (!strcmp(node->op, "contribution_clear")) { game->experience_value = 0; result = 0; }
            else if (!strcmp(node->op, "loot_child")) {
                size_t recipient; const uint8_t *descriptor;
                if (common_event_actor(game, r->actor, a[1], &recipient) != 0 ||
                    fd2_field_event_child_descriptor((size_t)a[0], &descriptor) != 0 ||
                    fd2_field_event_child_begin(r, r->flow, r->pc, r->actor, game->units.count) != 0 ||
                    loot_flow_begin_event_child(game, recipient, descriptor) != 0) return common_event_fail(game);
                return game->loot.child.active ? 0 : common_event_child_resume(game);
            }
            if (result < 0) port = FD2_FLOW_ERROR;
            else if(game->runtime_host_quit)return 0;
        }
        if (common_event_advance(game, port) != 0) return -1;
    }
    return common_event_fail(game);
}

int fd2_field_game_common_event_begin(fd2_field_game *game, const char *id, size_t actor) {
    if (!game || !game->ready || !id || game->units.count > FD2_FIELD_MAX_UNITS || actor >= game->units.count ||
        progress_pending(game) || game->player_post_waiting_phase ||
        game->failure != FD2_FIELD_FAILURE_NONE) return -1;
    const fd2_static_flow *flow = fd2_static_flow_lookup("common_event", id);
    if (!flow) return -1;
    /* 所有已登记叶均先作只读资源检查，不消费键/RNG；未来身份投影由
     * Play的资源callback负责。执行时再次核当前资源，晚错保留原写序。 */
    return common_event_bound_begin(game,flow,actor);
}

static int common_event_bound_begin(fd2_field_game *game,const fd2_static_flow *flow,size_t actor) {
    if(!game || !flow || !game->ready || game->event_resume.active ||
        game->runtime_host_quit || game->failure!=FD2_FIELD_FAILURE_NONE ||
        game->units.count>FD2_FIELD_MAX_UNITS || actor>=game->units.count ||
        common_event_invalid_deferred(game) || common_event_preflight_flow(game,flow,actor)!=0)return -1;
    game->event_resume=(fd2_field_event_resume){.flow=flow,.pc=flow->entry,.actor=(uint8_t)actor,.active=1};
    return common_event_run(game);
}

int fd2_field_game_common_event_peek(const fd2_field_game *game,
        fd2_static_flow_node *node, size_t *actor) {
    if (!game || !node || !actor || game->failure != FD2_FIELD_FAILURE_NONE) return -1;
    const fd2_field_event_resume *r = &game->event_resume;
    if (!r->active || r->child_wait) return 0;
    const fd2_static_flow_node *source = fd2_static_flow_at(r->flow, r->pc);
    if (!source || common_event_literal(game, source, node) != 0 || !common_event_leaf(node->op)) return -1;
    *actor = r->actor;
    return 1;
}

int fd2_field_game_common_event_reply(fd2_field_game *game, unsigned port) {
    if (!game || !game->event_resume.active || game->event_resume.child_wait ||
        game->runtime_host_quit || game->failure != FD2_FIELD_FAILURE_NONE) return -1;
    const fd2_static_flow_node *node = fd2_static_flow_at(game->event_resume.flow, game->event_resume.pc);
    fd2_static_flow_node literal;
    if (!node || common_event_literal(game, node, &literal) != 0 || !common_event_leaf(literal.op)) return -1;
    unsigned expected = !strcmp(literal.op, "story") || !strcmp(literal.op, "wait_key") ? FD2_FLOW_ACK : FD2_FLOW_NEXT;
    if (port != expected && port != FD2_FLOW_ERROR) return -1;
    if (common_event_advance(game, port) != 0) return -1;
    return common_event_run(game);
}

static int common_event_callback_begin(fd2_field_game *game, uint8_t action, size_t actor) {
    const fd2_static_flow *flow = known_common_event_flow(game,action);
    if (!flow) return 0; /* 尚未绑定的机制夹具不开放新关卡。 */
    if (game->event_resume.active || game->runtime_host_quit || game->failure!=FD2_FIELD_FAILURE_NONE ||
        common_event_invalid_deferred(game) || actor >= game->units.count ||
        common_event_preflight_flow(game, flow, actor) != 0) return -1;
    game->event_resume = (fd2_field_event_resume){.flow=flow,.pc=flow->entry,
        .actor=(uint8_t)actor,.active=1,.from_loot=2};
    if (common_event_run(game) != 0) return -1;
    if (game->event_resume.active) game->event_resume.from_loot = 1;
    return 1;
}

#endif
