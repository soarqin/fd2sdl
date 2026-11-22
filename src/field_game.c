/* 炎龙骑士团 2 SDL3 重写 - 正式战场 session
 *
 * 逆向依据：FDFIELD stage metadata/placement、DAT_00003a45 的 0x50
 * 字节单位表、field_view_render_tiles @0x11eee、
 * map_scene_render_actors @0x1cb94、map_actor_blit_24x24 @0x1da16、
 * map_tile_blit_visible @0x12ac6、field_unit_detail_transition_frame
 * @0x18409、field_physical_exchange @0x1548e、
 * field_physical_attack_sequence @0x1e856、
 * field_physical_attack_resolve @0x1ecc7 与
 * field_defeated_units_finalize @0x1db65（entry @0x1db65）、
 * field_player_unit_action @0x18890（原登记同名）、
 * field_controller_input @0x117e7（原登记同名）与sub_13565 @0x13565。
 * 法术列表边界依据field_player_magic_command_execute @0x1cff0（原登记同名）
 * 与field_player_magic_list_select_step @0x1d51d（原登记sub_1D51D）。
 * 物品层依据field_player_item_command_execute @0x1bbdc（原登记同名）、
 * field_unit_inventory_select @0x1b932（原sub_1B932）与
 * field_unit_inventory_select_step @0x1b9de（原sub_1B9DE）。
 * 物品提交依据field_item_use_effect_execute @0x20c6f（原sub_20C6F）、
 * field_hp_heal_group_execute @0x211a4（原sub_211A4）的逐目标HP恢复与
 * field_unit_inventory_slot_remove @0x1b8e7（原sub_1B8E7）。
 * 装备依据field_unit_equipment_menu_execute @0x1bffe（原sub_1BFFE）、
 * field_unit_equipment_slot_set @0x1c142（原sub_1C142）与
 * field_unit_can_equip_item @0x1c1c3（原sub_1C1C3）。
 */

#include "settings.h"
#include "field_game.h"
#include "bios_clock.h"
#include "battle_animation.h"
#include "figani.h"
#include "magic_fx.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_attack.h"
#include "field_move.h"
#include "field_background.h"
#include "field_render_scope.h"
#include "field_move_profile.h"
#include "field_item.h"
#include "field_prepare.h"
#include "field_magic.h"
#include "portrait.h"
#include "scene.h"
#include "save.h"
#include "field_unit_base.h"
#include "field_unit_stats.h"
#include "static_flow.h"
#include "static_data.h"
#include "static_data/field_game_effects.inc"
#include "field_reward_flow.h"
#include "field_common_event_flow.h"

_Static_assert(FD2_FIELD_PROGRESS_GAIN == 1 && FD2_FIELD_PROGRESS_LEVEL == 2 &&
               FD2_FIELD_PROGRESS_GROWTH == 3 && FD2_FIELD_PROGRESS_LEARN == 4 &&
               FD2_FIELD_PROGRESS_CLOSE == 5,
               "reward notice IDs must match the static flow contract");

#define FD2_FIELD_BIOS_TICK_MS 55
#define FD2_FIELD_IDLE_MS 275
#define FD2_FIELD_RANGE_VISUAL_MS 165 /* 3 个约 55 ms 的 BIOS tick */
/* field_command_menu_wait_key @code0 0x7898 每超过 3 个 BIOS tick 切换一次
 * normal/highlight 帧，即约 4×55 ms。 */
#define FD2_FIELD_COMMAND_HIGHLIGHT_MS 220
#define FD2_FIELD_AUTOMATIC_ACTION_MS 150

/* 有限copy/source显式等待前提交：保同input owner，不履行另一动画的deadline。 */
static void game_present_copy(fd2_vga *vga) {
    fd2_input_begin_frame(&vga->input);
    fd2_vga_upload_current_frame(vga);
}


static int recover_stationary_unit(fd2_field_game *game, size_t actor_index);
static int turn_present(fd2_field_game *game, fd2_field_runtime_present_step step, int value);
static int focus_to(fd2_field_game *game, int x, int y);
static int run_turn_entry(fd2_field_game *game, fd2_field_turn_entry entry);
typedef enum {
    PLAYER_ACTION_WAIT,
    PLAYER_ACTION_PHYSICAL,
    PLAYER_ACTION_MAGIC,
    PLAYER_ACTION_ITEM,
    PLAYER_ACTION_WAIT_COMPLETE,
    PLAYER_ACTION_AI_PHYSICAL,
    PLAYER_ACTION_AI_MAGIC,
    PLAYER_ACTION_AI_ITEM
} player_action_kind;
static int finish_unit_action_internal(fd2_field_game *game,
                                       player_action_kind kind,
                                       uint64_t magic_raw_reward);
static int resolve_physical_exchange(fd2_field_game *game,
                                     size_t attacker_index,
                                     size_t target_index,
                                     int finish_player_action);

/* field_ai_unit_execute_core @0x13a9f 的 common tail：动作 helper 只提交
 * 物理／移动结果；此处完成最终lookup、acted和方向reset，outer随后
 * 分派pending。它不经过玩家 command menu。 */
static int finish_ai_action(fd2_field_game *game, size_t actor_index);
static void inventory_slot_remove(fd2_field_unit *unit, size_t slot_index);
static int inventory_first_empty_add(fd2_field_unit *unit, uint8_t item_id);
static int item_give_has_target(const fd2_field_game *game);
static int finish_item_give_cancel(fd2_field_game *game, int restore_focus);
static const uint8_t *item_target_record(const fd2_field_game *game);
static int item_teleport_destination_is_legal(const fd2_field_game *game);
static int teleport_destination_is_legal(const fd2_field_game *game, size_t raw);
static int effect_deaths_are_supported(fd2_field_game *game, size_t recipient,
    const uint8_t *targets, size_t count, uint8_t magic_id, int item_heal, int magic_collector);
static int complete_player_controller_tail(fd2_field_game *game, size_t actor_index);
static int complete_player_controller_after_result(fd2_field_game *game, size_t actor_index);
static int begin_stage_result_call(fd2_field_game *game,size_t actor,unsigned completion);
static int finish_ai_outer_result(fd2_field_game *game,size_t actor);
static int finish_ai_outer_after_result(fd2_field_game *game,size_t actor);
static int skip_ai_outer_result(fd2_field_game *game,size_t actor);
static int refresh_battle_result(fd2_field_game *game,size_t actor);
static void log_stage_result_return(const fd2_field_game *game,const char *caller,size_t actor);
static int finish_and_dispatch_ai_action(fd2_field_game *game, size_t actor_index);
static int dispatch_pending_cell_event(fd2_field_game *game, size_t actor_index);
static int advance_status_progress(fd2_field_game *game);
static int run_turn_entry(fd2_field_game *game, fd2_field_turn_entry entry);
static int request_music(fd2_field_game *game, int track, fd2_field_music_reason reason);
static int preflight_reward(fd2_field_game *game, size_t actor_index, uint32_t gain);
static int begin_reward(fd2_field_game *game, size_t actor_index,
                         fd2_field_progress_resume resume, size_t resume_actor);
static int loot_descriptor_preflight(fd2_field_game *game, size_t recipient,
                                     const uint8_t descriptor[3]);
static int begin_defeated_loot(fd2_field_game *game, size_t recipient,
    size_t action_actor, player_action_kind kind, uint64_t raw_reward,
    int magic_collector);
static int begin_wait_cell(fd2_field_game *game, size_t actor_index);
static int loot_resource_ready(fd2_field_game *game, const fd2_field_loot_notice *notice);
static int inventory_visible_is_known(const fd2_field_unit *unit);
static int common_event_bound_begin(fd2_field_game *game, const fd2_static_flow *flow, size_t actor);
static int common_event_preflight_flow(const fd2_field_game *game, const fd2_static_flow *flow, size_t actor);
static int march_continue(fd2_field_game *game, fd2_vga *vga, uint32_t frame_delay_ms,
    int anchor_x, int anchor_y, size_t first);
static void record_turn_events_from(fd2_field_game *game, size_t first_slot);
static void apply_phase_status_tick(fd2_field_game *game, uint8_t side);
static void set_ai_continuation(fd2_field_game *game, fd2_field_progress_resume resume,
        size_t actor, int common_tail) {
    if (game->loot.notice.kind != FD2_FIELD_LOOT_NONE) {
        game->loot.resume = resume; game->loot.action_actor = actor;
        game->loot.ai_common_tail = (uint8_t)common_tail;
    } else {
        game->progress.resume = resume; game->progress.resume_actor = actor;
        game->progress.ai_common_tail = (uint8_t)common_tail;
    }
}
static int progress_pending(const fd2_field_game *game) {
    return game && (game->runtime_host_quit || game->event_resume.active || game->loot.child.active ||
                    game->loot.source != 0u ||
                    game->progress.notice.kind != FD2_FIELD_PROGRESS_NONE ||
                    game->progress.resume != FD2_FIELD_PROGRESS_RESUME_NONE);
}

/* field_selection_overlay_draw @0x122dc（原登记同名）display6的候选查询
 * 投影；App的真实map/node写点归下方selection_cursor_mark，headless仅保逻辑接口。 */
static void prepare_item_cursor(fd2_field_game *game) {
    if (game->interaction != FD2_FIELD_INTERACTION_ITEM_TARGETING ||
        !game->move_range.nodes) return;
    const uint8_t *record = item_target_record(game);
    if (!record || record[0x12] != 4 || game->cursor_cell_x < 0 || game->cursor_cell_y < 0 ||
        game->cursor_cell_x >= game->move_range.width || game->cursor_cell_y >= game->move_range.height) return;
    fd2_field_path_node *node = &game->move_range.nodes[
        (size_t)game->cursor_cell_y * (size_t)game->move_range.width + (size_t)game->cursor_cell_x];
    node->distance = 0;
    node->can_stop = 1;
}

/* field_selection_overlay_draw @0x122dc（原同名）126f1：只有mode6在
 * terrain之后、actor/FG之前写当前map raw3=0；其轨迹跨后继mode0保留。 */
static int selection_cursor_mark(fd2_field_game *game) {
    if(game->ordinary_display_mode!=6u)return 0;
    if(!game->map.cells || game->cursor_cell_x<0 || game->cursor_cell_y<0 ||
        game->cursor_cell_x>=game->map.width || game->cursor_cell_y>=game->map.height)return -1;
    size_t cell=(size_t)game->cursor_cell_y*(size_t)game->map.width+(size_t)game->cursor_cell_x;
    game->map.cells[cell]&=UINT32_C(0x00ffffff);
    prepare_item_cursor(game); /* 现有玩家候选查询投影；不建另一份轨迹。 */
    return 0;
}

#include "field_defeat_flow.h"

static int append_persistent_player(fd2_field_game *game, uint8_t unit_id) {
    if (game->roster_count >= FD2_SAVE_BATTLE_MAX_ROSTER) return -1;
    fd2_field_unit *record = &game->roster[game->roster_count];
    if (fd2_field_unit_persistent_create(record, unit_id) != 0) return -1;
    game->roster_count++;
    return 0;
}

static int load_initial_units(fd2_field_game *game, const fd2_static_flow *flow,
    const fd2_save_manual_state *initial_roster);

size_t fd2_field_game_group_count(const fd2_field_game *game, uint8_t group) {
    size_t count = 0;
    size_t templates;
    if (!game || fd2_field_unit_template_count(&game->metadata, &templates) != 0) return 0;
    for (size_t i = 0; i < game->units.count; i++) {
        uint16_t source = game->units.source_template_indices[i];
        if (source >= templates) continue;
        if (game->metadata.unit_templates[source].bytes[0x15] == group)
            count++;
    }
    return count;
}

static int validate_initial_units(const fd2_field_game *game) {
    size_t templates;
    if (!game || !game->units.count || game->units.count > FD2_FIELD_MAX_UNITS ||
        !game->roster_count || game->roster_count > FD2_SAVE_BATTLE_MAX_ROSTER ||
        fd2_field_unit_template_count(&game->metadata, &templates) != 0)
        return -1;

    for (size_t i = 0; i < game->units.count; i++) {
        uint16_t source = game->units.source_template_indices[i];
        if (source != FD2_FIELD_UNIT_NO_TEMPLATE &&
            (source >= templates ||
             game->units.items[i].unit_id != game->metadata.unit_templates[source].bytes[1]))
            return -1;
        if (fd2_field_unit_cached_frame_index(&game->units, i, 0) >=
                game->sprites.frame_count ||
            !fd2_field_movement_profile_get(
                game->units.items[i].movement_profile) ||
            game->units.items[i].movement_points == 0 ||
            game->units.items[i].hp == 0 ||
            game->units.items[i].hp_max == 0)
            return -1;
    }
    return 0;
}

static int max_camera_x(const fd2_field_game *game) {
    int value = game ? game->map.width - FD2_FIELD_VIEW_CELLS_X : 0;
    return value > 0 ? value : 0;
}

static int max_camera_y(const fd2_field_game *game) {
    int value = game ? game->map.height - FD2_FIELD_VIEW_CELLS_Y : 0;
    return value > 0 ? value : 0;
}

static int clamp_int(int value, int lo, int hi) {
    if (value < lo) return lo;
    if (value > hi) return hi;
    return value;
}

/* field_secondary_menu_execute @code0 0x9df7：
 * - selector 2（读档）在 FD2.SAV 资源打开失败时使用 disabled frame；
 * - selector 1（存档）在仍有未隐藏 actor 且其 acted bit 0x80 已置位时
 *   使用 disabled frame。原版读取 DS:0x3f22 的四个零 dword 到栈，
 *   再只改这两个动态项。 */
static void init_option_values(fd2_field_game *game) {
    static const uint8_t defaults[FD2_FIELD_SYSTEM_MENU_COUNT] = {
        1, 1, 0, 1
    };
    if (game) memcpy(game->option_values, defaults, sizeof(defaults));
}

static void refresh_secondary_menu_disabled(fd2_field_game *game) {
    if (!game) return;
    /* 总览沿1b41d索引取条件；菜单能力由当前发布绑定决定。 */
    uint8_t disabled[FD2_FIELD_SYSTEM_MENU_COUNT] = {0, 0, 0, 0};
    disabled[0] = !fd2_field_stage_is_supported(game->stage);
    if (!game->save_resource_available)
        disabled[2] = 1;
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (!fd2_field_unit_is_hidden(unit) &&
            fd2_field_unit_has_acted(unit)) {
            disabled[1] = 1;
            break;
        }
    }
    fd2_field_system_menu_set_disabled(&game->system_menu, disabled);
}

static void refresh_system_menu_disabled(fd2_field_game *game) {
    if (game->system_menu.page == FD2_FIELD_SYSTEM_PAGE_SECONDARY) {
        refresh_secondary_menu_disabled(game);
    } else if (game->system_menu.page == FD2_FIELD_SYSTEM_PAGE_PARENT) {
        /* END与行军按当前批次战斗能力判门，entry-only域保持关闭。 */
        const uint8_t disabled[] = {0, !fd2_field_stage_battle_is_supported(game->stage), 0, !fd2_field_stage_battle_is_supported(game->stage)};
        fd2_field_system_menu_set_disabled(&game->system_menu, disabled);
    } else if (game->system_menu.page == FD2_FIELD_SYSTEM_PAGE_OPTIONS) {
        const uint8_t disabled[] = {0, 0, 0, 0};
        fd2_field_system_menu_set_disabled(&game->system_menu, disabled);
    }
}

static int unit_can_act_for_side(const fd2_field_unit *unit, uint8_t side) {
    return unit && unit->side == side && !fd2_field_unit_is_hidden(unit) &&
           unit->hp != 0;
}

static int ai_unit_can_execute(const fd2_field_unit *unit, uint8_t side) {
    return unit_can_act_for_side(unit, side) &&
           (unit->flags & FD2_FIELD_UNIT_FLAG_AI_INELIGIBLE) == 0;
}

size_t fd2_field_game_remaining_units(const fd2_field_game *game,
                                      uint8_t side) {
    size_t count = 0;
    if (!game) return 0;
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (unit_can_act_for_side(unit, side) &&
            !fd2_field_unit_has_acted(unit))
            count++;
    }
    return count;
}

static const fd2_static_flow *stage_flow_get(const char *kind, size_t stage) {
    char id[64];
    int length = snprintf(id, sizeof(id), "stage.%s.%03zu", kind, stage);
    return length > 0 && (size_t)length < sizeof(id)
        ? fd2_static_flow_lookup("stage", id) : NULL;
}

static const fd2_static_flow *stage_result_event_flow(size_t stage) {
    char id[64];int length=snprintf(id,sizeof(id),"stage.%03zu.result",stage);
    return length>0 && (size_t)length<sizeof(id)?fd2_static_flow_lookup("common_event",id):NULL;
}

/* sub_205B4 @0x205b4、field_stage1_battle_result_check @0x206c5（原sub_206C5）：
 * 正式结果按hidden/side及原覆盖顺序查询，不以HP替代死亡整理；
 * default仅保留既有内部fallback身份。纯查询不提交53ECC结果锁存。 */
int fd2_field_game_result_query_flow(const fd2_field_game *game, const char *id,
                                    fd2_field_battle_result *out) {
    if (!game || !game->ready || !id || !out || strncmp(id, "stage.result.", 13)) return -1;
    const fd2_static_flow *flow = fd2_static_flow_lookup("stage", id);
    int pc = flow ? flow->entry : -1;
    for (unsigned budget = 0; budget < FD2_FLOW_STEP_BUDGET; budget++) {
        const fd2_static_flow_node *n = fd2_static_flow_at(flow, pc);
        if (!n || !(n->views & FD2_FLOW_LOGIC)) break;
        if (!strcmp(n->op, "result_return")) {
            if (n->args[0] >= FD2_FIELD_BATTLE_ONGOING && n->args[0] <= FD2_FIELD_BATTLE_DEFEAT) {
                *out = (fd2_field_battle_result)n->args[0];
                return 0;
            }
            break;
        } else if (!strcmp(n->op, "result_keep")) {
            if (game->battle_result < FD2_FIELD_BATTLE_ONGOING || game->battle_result > FD2_FIELD_BATTLE_DEFEAT) break;
            *out = game->battle_result;
            return 0;
        } else if (!strcmp(n->op, "result_invalid")) {
            return -1;
        } else {
            /* 20957类结果条件与事件共用同一const谓词，不发明raw0 caller。
             * stage域只登记明确actor的条件；覆盖优先级由各结果冷图决定。 */
            unsigned port = FD2_FLOW_ERROR;
            if (common_event_query(game, SIZE_MAX, n, &port) != 1) break;
            pc = fd2_static_flow_edge(flow, n, port);
            continue;
        }
    }
    return -1;
}

int fd2_field_game_battle_result_query(const fd2_field_game *game,fd2_field_battle_result *out) {
    if (!game || !game->ready || !out || game->units.count>FD2_FIELD_MAX_UNITS) return -1;
    /* 含story/53ECC写点的来源仅在原player/phase/AI的五个后置CALLsite执行。
     * 普通绘制、AI检查及存取的const查询只读已提交锁存，不能重播剧情。 */
    if (stage_result_event_flow(game->stage)) {
        if (game->battle_result<FD2_FIELD_BATTLE_ONGOING || game->battle_result>FD2_FIELD_BATTLE_DEFEAT)return -1;
        *out=game->battle_result;return 0;
    }
    const fd2_static_flow *flow = stage_flow_get("result", game->stage);
    if (!flow) flow = fd2_static_flow_lookup("stage", "stage.result.default");
    return flow?fd2_field_game_result_query_flow(game,flow->id,out):-1;
}

fd2_field_battle_result fd2_field_game_battle_result(const fd2_field_game *game) {
    if(!game || !game->ready)return FD2_FIELD_BATTLE_ONGOING;
    fd2_field_battle_result result;
    if(fd2_field_game_battle_result_query(game,&result)==0)return result;
    /* 保留旧ABI的已证域接口；正式存取/控制器不得经这里处理坏输入。 */
    fprintf(stderr, "corrupt internal battle result flow (stage=%zu)\n", game->stage);
    abort();
}

/* field_unit_stage_template_append @0x10c50 的53AFA只控制重叠布点；
 * 默认搜索最近空占格，同行平价取后者。并非AI-ineligible flags04。 */
static int append_group_units(const fd2_field_game *game, fd2_field_units *units,
                              uint8_t group, int force) {
    if (!game || !units || !fd2_field_stage_is_supported(game->stage) ||
        fd2_field_game_group_count(game, group)) return -1;
    fd2_field_units shadow = *units;
    if (fd2_field_units_append_group_placed(&shadow, &game->metadata,
            &game->placements, &game->map, group, force) != 0 ||
        shadow.count == units->count) return -1;
    /* 10c50完成基础/装备记录后调用1b750；新group不能保留清零的战斗派生值。
     * 整组在shadow内验证，坏装备或资源不会交接半组actor。 */
    for (size_t i = units->count; i < shadow.count; i++) {
        if (fd2_field_unit_cached_frame_index(&shadow, i, 0) >=
            game->sprites.frame_count ||
            fd2_field_unit_combat_stats_recompute(&shadow.items[i],
                fd2_field_unit_original_item_stats, NULL, fd2_field_unit_original_status_scale, NULL) != 0)
            return -1;
    }
    *units = shadow;
    return 0;
}

/* 事件绑定由冷定义ID决定；未知stage/action不创建默认流程。 */
static const fd2_static_flow *known_event_flow_domain(const fd2_field_game *game, uint8_t action,
        const char *domain) {
    char id[64];
    if (!game) return NULL;
    int written = snprintf(id, sizeof(id), "stage.%03zu.action.%03u", game->stage, (unsigned)action);
    if (written < 0 || (size_t)written >= sizeof(id)) return NULL;
    return fd2_static_flow_lookup(domain, id);
}

static const fd2_static_flow *known_common_event_flow(const fd2_field_game *game, uint8_t action) {
    return known_event_flow_domain(game, action, "common_event");
}

int fd2_field_game_event_resources_validate_actor(const fd2_field_game *game,
        uint8_t action,size_t actor) {
    if(!game || game->units.count>FD2_FIELD_MAX_UNITS || actor>=game->units.count) return -1;
    const fd2_static_flow *common=known_common_event_flow(game,action);
    if(!common)return -1;
    return common_event_preflight_flow(game,common,actor);
}

static int execute_known_turn_event(fd2_field_game *game,uint8_t action,size_t actor) {
    const fd2_static_flow *flow=known_common_event_flow(game,action);
    /* 不再先跑整段LOGIC后重放Scene；唯一父PC在每个实际叶/ACK后前进。
     * 未知绑定继续明确失败，不以旧事务snapshot回滚已提交前缀。 */
    return flow?common_event_bound_begin(game,flow,actor):-1;
}

static void fail_session(fd2_field_game *game, fd2_field_failure failure,
                         const fd2_field_event_notice *notice) {
    if (game->failure != FD2_FIELD_FAILURE_NONE) return;
    game->failure = failure;
    game->failure_notice = *notice;
    game->phase_next_action_ms = 0;
}

static void fail_action(fd2_field_game *game, size_t actor_index) {
    fd2_field_event_notice notice = {
        .turn_number = game->turn_number,
        .phase = game->active_side,
        .unit_index = (uint8_t)actor_index,
        .action = UINT8_MAX,
        .slot = SIZE_MAX,
    };
    fail_session(game, FD2_FIELD_FAILURE_ACTION, &notice);
}

static int refresh_battle_result(fd2_field_game *game,size_t actor) {
    fd2_field_battle_result value;
    if(fd2_field_game_battle_result_query(game,&value)!=0){fail_action(game,actor);return -1;}
    game->battle_result=value;return 0;
}

int fd2_field_game_set_progress_preflight(fd2_field_game *game,
        fd2_field_progress_preflight_fn preflight, void *userdata) {
    if (!game || !game->ready || progress_pending(game))
        return -1;
    game->progress_preflight = preflight;
    game->progress_preflight_userdata = userdata;
    return 0;
}

static int progress_resource_ready(fd2_field_game *game,
                                    const fd2_field_progress_notice *notice) {
    if (!game->progress_preflight || game->progress_preflight(
            game, notice, game->progress_preflight_userdata) == 0) return 1;
    fail_action(game, notice->actor_index); /* 必需UI资源错误不能冒充普通非法目标。 */
    return 0;
}

static fd2_reward_cursor reward_cursor_get(const fd2_field_progress *progress) {
    return (fd2_reward_cursor){progress->total, progress->level,
        progress->growth_cursor, progress->learn_cursor, progress->flow_step};
}

static int advance_reward_progress(fd2_field_game *game) {
    fd2_field_progress *progress = &game->progress;
    if (progress->notice.actor_index >= game->units.count) return -1;
    fd2_reward_cursor cursor = reward_cursor_get(progress);
    fd2_reward_notice notice;
    int result = fd2_reward_advance(fd2_reward_graph(), &game->units.items[progress->notice.actor_index],
        game->experience_value, &cursor, &notice, fd2_field_unit_original_item_stats, NULL,
        fd2_field_unit_original_status_scale, NULL);
    progress->total = cursor.total; progress->level = cursor.level;
    progress->growth_cursor = cursor.growth_cursor; progress->learn_cursor = cursor.learn_cursor;
    progress->flow_step = cursor.step;
    if (result == 0) {
        progress->notice = (fd2_field_progress_notice){(fd2_field_progress_kind)notice.kind,
            progress->notice.actor_index, notice.fragment, notice.value, notice.magic_id, notice.wait_ticks};
        if (notice.set_message) game->message_number = notice.value;
        if (notice.kind==FD2_FIELD_PROGRESS_LEARN)
            game->message_new_fragment=441u+notice.magic_id;
    }
    return result;
}

/* 资源预检消费与真实提交相同的图和单步原语，只操作80B局部记录。 */
static int preflight_reward(fd2_field_game *game, size_t actor_index, uint32_t gain) {
    if (actor_index >= game->units.count) return -1;
    const fd2_field_unit *unit = &game->units.items[actor_index];
    fd2_field_unit shadow;
    int result = fd2_field_unit_reward_preview(unit, gain,
        fd2_field_unit_original_status_scale, NULL, &shadow);
    if (result <= 0) return result;
    const fd2_static_flow *flow = fd2_reward_graph();
    if (!flow) return -1;
    /* progress冷域仍由生成器限制254节点；不扩大原热游标。 */
    fd2_reward_cursor cursor = {0}; cursor.step = (uint8_t)flow->entry;
    shadow = *unit;
    for (unsigned budget = 0; budget < FD2_FLOW_STEP_BUDGET; budget++) {
        fd2_reward_notice value;
        result = fd2_reward_advance(flow, &shadow, gain, &cursor, &value,
            fd2_field_unit_original_item_stats, NULL, fd2_field_unit_original_status_scale, NULL);
        if (result == 1) return 1;
        if (result != 0) return -1;
        fd2_field_progress_notice notice = {(fd2_field_progress_kind)value.kind,
            actor_index, value.fragment, value.value, value.magic_id, value.wait_ticks};
        if (!progress_resource_ready(game, &notice) || fd2_reward_ack(flow, &cursor) != 0) return -1;
    }
    return -1;
}

static int begin_reward(fd2_field_game *game, size_t actor_index,
                         fd2_field_progress_resume resume, size_t resume_actor) {
    if (progress_pending(game) ||
        actor_index >= game->units.count) return -1;
    int checked = preflight_reward(game, actor_index, game->experience_value);
    if (checked <= 0) return checked;
    memset(&game->progress, 0, sizeof(game->progress));
    const fd2_static_flow *flow = fd2_reward_graph();
    if (!flow) return -1;
    game->progress.flow_step = (uint8_t)flow->entry;
    game->progress.notice.actor_index = actor_index;
    game->progress.resume = resume;
    game->progress.resume_actor = resume_actor;
    return advance_reward_progress(game) == 0 ? 1 : -1;
}

int fd2_field_game_progress_peek(const fd2_field_game *game,
                                 fd2_field_progress_notice *notice) {
    if (!game || !game->ready || !notice || game->failure != FD2_FIELD_FAILURE_NONE)
        return -1;
    if (game->progress.notice.kind == FD2_FIELD_PROGRESS_NONE) return 0;
    if (game->progress.notice.kind > FD2_FIELD_PROGRESS_STATUS ||
        game->progress.notice.actor_index >= game->units.count) return -1;
    *notice = game->progress.notice;
    return 1;
}

int fd2_field_game_progress_ack(fd2_field_game *game) {
    fd2_field_progress_notice notice;
    if (!game || game->runtime_host_quit) return -1;
    if (fd2_field_game_progress_peek(game, &notice) != 1) return -1;
    int result = 0;
    if (notice.kind == FD2_FIELD_PROGRESS_STATUS) {
        const fd2_static_flow *flow = fd2_static_flow_lookup("progress", "phase.status");
        const fd2_static_flow_node *node = fd2_static_flow_at(flow, game->progress.flow_step);
        int next = node && !strcmp(node->op, "status_notice")
            ? fd2_static_flow_edge(flow, node, FD2_FLOW_ACK) : -1;
        if (next < 0) result = -1;
        else { game->progress.flow_step = (uint8_t)next; result = advance_status_progress(game); }
    } else {
        fd2_reward_cursor cursor = reward_cursor_get(&game->progress);
        const fd2_static_flow_node *node = fd2_static_flow_at(fd2_reward_graph(), cursor.step);
        if (!node || node->args[0] != (int32_t)notice.kind ||
                fd2_reward_ack(fd2_reward_graph(), &cursor) != 0) result = -1;
        else { game->progress.flow_step = cursor.step; result = advance_reward_progress(game); }
    }
    if (result == 1) {
        fd2_field_progress_resume resume = game->progress.resume;
        size_t actor = game->progress.resume_actor;
        int common_tail = game->progress.ai_common_tail;
        game->experience_value = 0;
        memset(&game->progress, 0, sizeof(game->progress));
        if (resume == FD2_FIELD_PROGRESS_RESUME_PLAYER)
            result = complete_player_controller_tail(game, actor);
        else if (resume == FD2_FIELD_PROGRESS_RESUME_AI_DIRECT ||
                 resume == FD2_FIELD_PROGRESS_RESUME_AI_SCHEDULER) {
            result=0; /* reward_finish的1是已返回，不是跳过outer后置的业务状态。 */
            if (common_tail) result = finish_ai_action(game, actor);
            if (result == 0 && !game->runtime_host_quit) result = dispatch_pending_cell_event(game, actor);
            if (result == 0 && !game->runtime_host_quit && !game->event_resume.active) {
                result=finish_ai_outer_result(game,actor);
            }
        }
    }
    if (result < 0 && game->failure == FD2_FIELD_FAILURE_NONE) fail_action(game, notice.actor_index);
    return result < 0 ? -1 : 0;
}

static int reserve_event_notice(fd2_field_game *game,
                                 const fd2_field_event_notice *notice) {
    if (game->failure != FD2_FIELD_FAILURE_NONE) return -1;
    if (game->event_log_count < FD2_FIELD_EVENT_LOG_CAPACITY) return 0;
    /* notice 还持有必需演出，容量不足必须先于 handler 拒绝。 */
    fail_session(game, FD2_FIELD_FAILURE_EVENT_CAPACITY, notice);
    return -1;
}

static void append_event_notice(fd2_field_game *game,
                                const fd2_field_event_notice *notice) {
    game->event_log[game->event_log_count++] = *notice;
    if (game->event_total_count != UINT32_MAX) game->event_total_count++;
    if (!notice->handled && game->unhandled_event_count != UINT32_MAX)
        game->unhandled_event_count++;
}

static void record_turn_events_from(fd2_field_game *game, size_t first_slot) {
    if (!game || game->failure != FD2_FIELD_FAILURE_NONE) return;
    /* 1a813逐slot重读live metadata；callback可以改尚未扫描的turn BYTE。
     * 等键后从原slot+1继续，不能重扫或预先拍一份匹配列表。 */
    for (size_t slot = first_slot; slot < FD2_FIELD_EVENT_SLOTS; slot++) {
        const fd2_field_turn_event match = game->metadata.turn_events[slot];
        if (match.turn != game->turn_number || match.phase != game->active_side) continue;
        if (known_common_event_flow(game,match.action)) {
            /* 原1a813实际传raw actor0；这与无caller查询的缺省值不同。 */
            if (execute_known_turn_event(game,match.action,0) != 0) { fail_action(game,0); return; }
            if (game->event_resume.active) {
                game->event_resume.completion=FD2_EVENT_COMPLETE_TURN;
                game->event_resume.next_slot=(uint8_t)(slot+1u);
                return;
            }
            continue;
        }
        size_t presentation_unit_count = game->units.count;
        int presentation_camera_x = game->camera_cell_x;
        int presentation_camera_y = game->camera_cell_y;
        int presentation_focus_x = game->cursor_cell_x;
        int presentation_focus_y = game->cursor_cell_y;
        fd2_field_event_notice notice = {
            .kind = FD2_FIELD_EVENT_TURN,
            .turn_number = game->turn_number,
            .phase = game->active_side,
            .action = match.action,
            .unit_index = UINT8_MAX,
            .presentation_unit_count = (uint8_t)presentation_unit_count,
            .presentation_camera_x = (int16_t)presentation_camera_x,
            .presentation_camera_y = (int16_t)presentation_camera_y,
            .presentation_focus_x = (int16_t)presentation_focus_x,
            .presentation_focus_y = (int16_t)presentation_focus_y,
            .slot = slot,
        };
        if (reserve_event_notice(game, &notice) != 0) return;
        /* 未绑定身份仅保来源诊断，绝不落入旧LOGIC事务/Scene重放。 */
        append_event_notice(game, &notice);
        fail_session(game,FD2_FIELD_FAILURE_EVENT_UNSUPPORTED,&notice);
        return;
    }
}

static void record_turn_events(fd2_field_game *game) { record_turn_events_from(game,0); }

/* field_cell_event_lookup @0x13a44（原登记同名）：只有命中才覆盖pending；
 * no-match保留前次命中。lookup不分派handler、不追加notice，不消费RNG。 */
static int collect_pending_cell_event(fd2_field_game *game,
                                       size_t actor_index, uint8_t match_arg) {
    if (!game || game->failure != FD2_FIELD_FAILURE_NONE ||
        actor_index >= game->units.count)
        return -1;
    const fd2_field_unit *actor = &game->units.items[actor_index];
    uint8_t action = UINT8_MAX;
    size_t slot = SIZE_MAX;
    int found = fd2_field_cell_event_find(&game->map, &game->terrain,
        &game->metadata, actor->x, actor->y, match_arg, &action, &slot);
    if (found < 0) {
        fd2_field_event_notice notice = {
            .kind = FD2_FIELD_EVENT_CELL, .turn_number = game->turn_number,
            .phase = game->active_side, .action = UINT8_MAX,
            .unit_index = (uint8_t)actor_index, .x = actor->x, .y = actor->y,
            .slot = SIZE_MAX,
        };
        fail_session(game, FD2_FIELD_FAILURE_EVENT_DATA, &notice);
        return -1;
    }
    if (found) game->pending_cell_event =
        (fd2_field_pending_cell_event){1, action, slot};
    return found;
}

/* side1/side0 outer @0x1d80b/@0x1d8ba 在common tail完成后才分派。
 * notice.slot记录lookup来源，x/y记录执行时actor的最终格，二者不混用。 */
static int dispatch_pending_cell_event(fd2_field_game *game, size_t actor_index) {
    if (!game || game->failure != FD2_FIELD_FAILURE_NONE ||
        actor_index >= game->units.count)
        return -1;
    if(game->runtime_host_quit)return 0;
    if (!game->pending_cell_event.valid) return 0;
    const fd2_field_unit *actor = &game->units.items[actor_index];
    fd2_field_event_notice notice = {
        .kind = FD2_FIELD_EVENT_CELL, .turn_number = game->turn_number,
        .phase = game->active_side, .action = game->pending_cell_event.action,
        .unit_index = (uint8_t)actor_index, .x = actor->x, .y = actor->y,
        .slot = game->pending_cell_event.source_slot,
    };
    notice.presentation_unit_count = (uint8_t)game->units.count;
    notice.presentation_camera_x = (int16_t)game->camera_cell_x;
    notice.presentation_camera_y = (int16_t)game->camera_cell_y;
    notice.presentation_focus_x = (int16_t)game->cursor_cell_x;
    notice.presentation_focus_y = (int16_t)game->cursor_cell_y;
    if (known_common_event_flow(game,notice.action)) {
        if(execute_known_turn_event(game,notice.action,actor_index)!=0)return -1;
        if(game->event_resume.active)game->event_resume.completion=FD2_EVENT_COMPLETE_AI_PENDING;
        return 0;
    }
    if (reserve_event_notice(game, &notice) != 0) return -1;
    append_event_notice(game, &notice);
    fail_session(game, FD2_FIELD_FAILURE_EVENT_UNSUPPORTED, &notice);
    return -1;
}

/* field_phase_status_tick @0x1a866（原登记同名）：提示只借同一B4续态，
 * 每raw/byte的写入、聚焦与重算保持原序；没有新的状态UI RNG。 */
static int run_status_flow(fd2_field_game *game, fd2_field_progress *progress,
        fd2_field_units *units, int preflight) {
    const fd2_static_flow *flow = fd2_static_flow_lookup("progress", "phase.status");
    const uint8_t side = progress->status_side;
    int pc = progress->flow_step;
    for (unsigned budget = 0; budget < FD2_FLOW_STEP_BUDGET; budget++) {
        if (!preflight && game->runtime_host_quit) return 0;
        const fd2_static_flow_node *n = fd2_static_flow_at(flow, pc);
        if (!n || !(n->views & FD2_FLOW_LOGIC)) return -1;
        progress->flow_step = (uint8_t)pc;
        fd2_field_unit *unit = progress->status_cursor < units->count
            ? &units->items[progress->status_cursor] : NULL;
        uint8_t *timer = unit && progress->status_byte && progress->status_byte <= 6u
            ? &unit->attack_status + progress->status_byte - 1u : NULL;
        unsigned port = FD2_FLOW_NEXT;
        const char *op = n->op;
        if (!strcmp(op, "status_poison_reset")) {
            progress->status_pass = 0; progress->status_cursor = progress->status_byte = 0;
        } else if (!strcmp(op, "status_actor_available"))
            port = unit ? FD2_FLOW_YES : FD2_FLOW_NO;
        else if (!strcmp(op, "status_actor_matches")) {
            if (!unit) return -1;
            port = unit->side == side && !fd2_field_unit_is_hidden(unit) ? FD2_FLOW_YES : FD2_FLOW_NO;
        } else if (!strcmp(op, "status_poison_active")) {
            if (!unit) return -1;
            port = unit->detail_status[0] ? FD2_FLOW_YES : FD2_FLOW_NO;
        } else if (!strcmp(op, "status_poison_damage")) {
            if (!unit || n->args[0] <= 0) return -1;
            uint16_t damage = (uint16_t)(unit->hp_max / (uint32_t)n->args[0]);
            if (!preflight) game->message_number = damage;
            unit->hp = unit->hp > damage ? (uint16_t)(unit->hp - damage) : 0;
            progress->notice.value = damage;
        } else if (!strcmp(op, "status_focus")) {
            if (!unit) return -1;
            progress->notice.actor_index = progress->status_cursor;
            if (!preflight && focus_to(game, unit->x, unit->y) != 0) {
                fail_action(game, progress->status_cursor); return -1;
            }
            if(!preflight && game->runtime_host_quit)return 0;
        } else if (!strcmp(op, "status_actor_next")) {
            progress->status_cursor++; progress->status_byte = 0;
        } else if (!strcmp(op, "status_finalize")) {
            if (!preflight) {
                int result=finalize_zero_hp_units(game);
                if(result<0)return -1;
                if(result>0)return 0;
            }
            else for (size_t i = 0; i < units->count; i++) if (!units->items[i].hp) units->items[i].flags = 1;
        } else if (!strcmp(op, "status_result")) {
            if (!preflight) {
                int result=begin_stage_result_call(game,0,FD2_EVENT_COMPLETE_STATUS_RESULT);
                if(result<0)return -1;
                if(result>0) {
                    int next=fd2_static_flow_edge(flow,n,FD2_FLOW_NEXT);
                    if(next<0 || next>UINT8_MAX)return -1;
                    progress->flow_step=(uint8_t)next;
                    /* 旧487/状态提示已由原ACK消费；结果story接管等待后，
                     * 不能再暴露旧notice供第二次ACK。失败前不清未交接owner。 */
                    memset(&progress->notice,0,sizeof(progress->notice));
                    return 0;
                }
                log_stage_result_return(game,"status",0);
            }
        } else if (!strcmp(op, "status_timer_reset")) {
            progress->status_pass = 1; progress->status_cursor = progress->status_byte = 0;
        } else if (!strcmp(op, "status_byte_available"))
            port = progress->status_byte < n->args[0] ? FD2_FLOW_YES : FD2_FLOW_NO;
        else if (!strcmp(op, "status_byte_select")) {
            if (progress->status_byte >= 6u) return -1;
            progress->status_byte++;
        } else if (!strcmp(op, "status_timer_nonzero")) {
            if (!timer) return -1;
            port = *timer ? FD2_FLOW_YES : FD2_FLOW_NO;
        } else if (!strcmp(op, "status_timer_decrement")) {
            if (!timer || !*timer) return -1;
            --*timer;
        } else if (!strcmp(op, "status_timer_expired")) {
            if (!timer) return -1;
            port = *timer == 0 ? FD2_FLOW_YES : FD2_FLOW_NO;
        } else if (!strcmp(op, "status_recompute")) {
            if (!unit || (!preflight && fd2_field_unit_combat_stats_recompute(unit,
                    fd2_field_unit_original_item_stats, NULL, fd2_field_unit_original_status_scale, NULL) != 0)) return -1;
        } else if (!strcmp(op, "status_notice")) {
            if (!unit || (n->args[1] && !timer)) return -1;
            uint32_t value = n->args[1] ? 0 : progress->notice.value;
            uint16_t fragment = (uint16_t)(n->args[0] + (n->args[1] ? progress->status_byte - 1u : 0u));
            progress->notice = (fd2_field_progress_notice){FD2_FIELD_PROGRESS_STATUS,
                progress->status_cursor, fragment, value, 0, (uint8_t)n->args[2]};
            return 0;
        } else if (!strcmp(op, "status_side_is")) port = side == n->args[0] ? FD2_FLOW_YES : FD2_FLOW_NO;
        else if (!strcmp(op, "status_tracks_differ")) {
            const fd2_field_stage_music *music = fd2_field_stage_music_get(game->stage);
            port = music && music->primary != music->alternate ? FD2_FLOW_YES : FD2_FLOW_NO;
        } else if (!strcmp(op, "status_enemy_music")) {
            const fd2_field_stage_music *music = fd2_field_stage_music_get(game->stage);
            if (!music) return -1;
            if(!preflight && request_music(game,music->alternate,FD2_FIELD_MUSIC_ENEMY)<0)return -1;
        } else if (!strcmp(op, "status_clear")) memset(progress, 0, sizeof(*progress));
        else if (!strcmp(op, "status_have_actor")) port = units->count ? FD2_FLOW_YES : FD2_FLOW_NO;
        else if (!strcmp(op, "status_focus_leader")) {
            if (!units->count) return -1;
            if (!preflight) {
                if (run_turn_entry(game, FD2_FIELD_TURN_PLAYER_READY) < 0) {
                    fail_action(game, 0); return -1;
                }
            }
        } else if (!strcmp(op, "status_finish")) { memset(progress, 0, sizeof(*progress)); return 1; }
        else return -1;
        pc = fd2_static_flow_edge(flow, n, port);
        if (pc < 0) return -1;
    }
    return -1;
}

static int advance_status_progress(fd2_field_game *game) {
    int result = run_status_flow(game, &game->progress, &game->units, 0);
    if (result < 0 && game->failure == FD2_FIELD_FAILURE_NONE)
        fail_action(game, game->progress.notice.actor_index);
    /* phase2的status_finish才真正返回原1a30b caller；不可在focus/clear
     * 中恢复51AAC，也不可越过未ACK提示、错误或hostquit。 */
    if(result>=0 && !progress_pending(game) && game->active_side==2u &&
        (game->player_post_waiting_phase || game->turn_post_caller!=FD2_FIELD_TURN_POST_NONE) &&
        fd2_field_game_resume_player_controller(game)<0)return -1;
    return result < 0 ? -1 : 0;
}

int fd2_field_game_progress_resume(fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE) return -1;
    if (game->runtime_host_quit) return 1;
    if(game->event_resume.active)return 1;
    if (game->progress.notice.kind != FD2_FIELD_PROGRESS_NONE) return 1;
    if (game->progress.resume != FD2_FIELD_PROGRESS_RESUME_PHASE) return 0;
    for (size_t i = 0; i < game->event_log_count; i++)
        if (game->event_log[i].kind == FD2_FIELD_EVENT_TURN &&
            game->event_log[i].handled && game->event_log[i].presentation_deferred) return 1;
    if (advance_status_progress(game) < 0) return -1;
    return progress_pending(game) ? 1 : 0;
}

static void apply_phase_status_tick(fd2_field_game *game, uint8_t side) {
    if (!game || game->failure != FD2_FIELD_FAILURE_NONE) return;
    if (progress_pending(game)) { fail_action(game, 0); return; }
    const fd2_static_flow *flow = fd2_static_flow_lookup("progress", "phase.status");
    if (!flow) { fail_action(game, 0); return; }
    /* 同图只模拟可预测写入以枚举资源；focus及派生错误保留其原实际写点。 */
    fd2_field_units units = game->units;
    fd2_field_progress preview = {0}; preview.status_side = side; preview.flow_step = (uint8_t)flow->entry;
    for (unsigned budget = 0; budget < FD2_FLOW_STEP_BUDGET; budget++) {
        int result = run_status_flow(game, &preview, &units, 1);
        if (result == 1) break;
        if (result < 0 || !progress_resource_ready(game, &preview.notice)) { fail_action(game, preview.notice.actor_index); return; }
        const fd2_static_flow_node *node = fd2_static_flow_at(flow, preview.flow_step);
        int next = fd2_static_flow_edge(flow, node, FD2_FLOW_ACK);
        if (next < 0 || budget + 1u == FD2_FLOW_STEP_BUDGET) { fail_action(game, preview.notice.actor_index); return; }
        preview.flow_step = (uint8_t)next;
    }
    memset(&game->progress, 0, sizeof(game->progress));
    game->progress.resume = FD2_FIELD_PROGRESS_RESUME_PHASE;
    game->progress.status_side = side;
    game->progress.flow_step = (uint8_t)flow->entry;
    (void)fd2_field_game_progress_resume(game);
}

static void clear_all_acted(fd2_field_game *game) {
    if (!game) return;
    /* field_all_actors_clear_acted @0x13536：遍历完整 raw actor 表，
     * 无视 side 与 hidden，仅执行 flags &= 0x7f。 */
    for (size_t i = 0; i < game->units.count; i++)
        fd2_field_unit_set_acted(&game->units.items[i], 0);
}

int fd2_field_game_set_music_request(fd2_field_game *game,
        fd2_field_music_request_fn request, void *userdata) {
    if (!game || !game->ready || progress_pending(game)) return -1;
    game->music_request = request; game->music_request_userdata = userdata;
    return 0;
}

static int request_music(fd2_field_game *game, int track, fd2_field_music_reason reason) {
    if (game->music_request && game->music_request(game->stage,track,reason,
            game->music_request_userdata)<0)return -1;
    return fd2_bios_clock_error()<0?-1:0;
}

static void enter_phase(fd2_field_game *game, uint8_t side) {
    if (!game || game->failure != FD2_FIELD_FAILURE_NONE) return;
    game->active_side = side;
    game->phase_unit_cursor = 0;
    game->phase_ai_pass = 0;
    game->phase_next_action_ms = 0;
    /* 原版顺序是 turn-event dispatch → poison/status tick → phase actor
     * loop；事件可能先追加 group 或设置剧情退出标志，因此不能交换。 */
    record_turn_events(game);
    if (game->failure != FD2_FIELD_FAILURE_NONE) return;
    if (game->event_resume.active) return; /* 原callback返回后才开始status。 */
    apply_phase_status_tick(game, side);
}

/* 1a30b的有限业务adapter；分支/等待/phase与呈现顺序在runtime.turncycle。
 * 同步呈现借Play，失败保留已提交owner，不重试或复制整Game。 */
static int turn_recovery_actor(const fd2_field_unit *unit) {
    return unit->side == 2u && !(unit->flags & 0x81u) &&
        !unit->detail_status[0] && !unit->detail_status[1] && unit->hp != unit->hp_max;
}

static int turn_present(fd2_field_game *game, fd2_field_runtime_present_step step, int value) {
    if (game->runtime_host_quit) return 1;
    int result = game->runtime_present ? game->runtime_present(game, step, value,
        game->runtime_present_userdata) : 0;
    if (result > 0) game->runtime_host_quit = 1;
    return result;
}

static int turn_apply(fd2_field_turn_step step, int value, void *userdata) {
    fd2_field_game *game = userdata;
    if (game->runtime_host_quit) return 1;
    const fd2_field_stage_music *music = fd2_field_stage_music_get(game->stage);
    switch (step) {
    case FD2_FIELD_TURN_RECOVERY_WAIT:
        return turn_present(game, FD2_FIELD_RUNTIME_RECOVERY_WAIT, value);
    case FD2_FIELD_TURN_RECOVERY_HIGHLIGHT:
        return turn_present(game, FD2_FIELD_RUNTIME_RECOVERY_HIGHLIGHT, 0);
    case FD2_FIELD_TURN_RECOVERY_CUE:
        for (size_t i=0;i<game->units.count;i++) if (turn_recovery_actor(&game->units.items[i]))
            return turn_present(game, FD2_FIELD_RUNTIME_RECOVERY_CUE, 0);
        return 0;
    case FD2_FIELD_TURN_RECOVERY_COMMIT:
        for (size_t i=0;i<game->units.count;i++) {
            fd2_field_unit *unit=&game->units.items[i];
            if (!turn_recovery_actor(unit)) continue;
            uint32_t hp=(uint32_t)unit->hp+unit->hp_max/5u;
            unit->hp=(uint16_t)(hp>unit->hp_max?unit->hp_max:hp);
            int result=turn_present(game, FD2_FIELD_RUNTIME_RECOVERY_NORMAL_ACTOR, (int)i);
            if (result) return result;
            fd2_field_unit_set_acted(unit,1);
        }
        return 0;
    case FD2_FIELD_TURN_RECOVERY_PROJECT:
        return turn_present(game, FD2_FIELD_RUNTIME_RECOVERY_PROJECT, 0);
    case FD2_FIELD_TURN_REFRESH:
        return turn_present(game, FD2_FIELD_RUNTIME_REFRESH, 0);
    case FD2_FIELD_TURN_STOP_IF_DIFFERENT:
        if (music && music->primary != music->alternate)
            if(request_music(game,-1,FD2_FIELD_MUSIC_PHASE_STOP)<0)return -1;
        return 0;
    case FD2_FIELD_TURN_ENEMY_TITLE:
        return turn_present(game, FD2_FIELD_RUNTIME_ENEMY_TITLE, 0);
    case FD2_FIELD_TURN_PLAYER_TITLE:
        return turn_present(game, FD2_FIELD_RUNTIME_PLAYER_TITLE, 0);
    case FD2_FIELD_TURN_CLEAR_ACTED:
        clear_all_acted(game); return 0;
    case FD2_FIELD_TURN_ENTER_PHASE:
        enter_phase(game,(uint8_t)value);
        return game->runtime_host_quit ? 1 : game->failure == FD2_FIELD_FAILURE_NONE ? 0 : -1;
    case FD2_FIELD_TURN_INCREMENT:
        game->turn_number++; return 0; /* 原DWORD模加，不饱和。 */
    case FD2_FIELD_TURN_DISPLAY_MODE:
        game->ordinary_display_mode=(uint8_t)value; return 0;
    case FD2_FIELD_TURN_PRIMARY_MUSIC:
        if (!music) return -1;
        return request_music(game,music->primary,FD2_FIELD_MUSIC_PLAYER);
    case FD2_FIELD_TURN_ROUND:
        return turn_present(game, FD2_FIELD_RUNTIME_ROUND, 0);
    case FD2_FIELD_TURN_FOCUS_RESET:
        game->focus_cycle_unit=0; return 0;
    case FD2_FIELD_TURN_FOCUS_ZERO:
        if(game->units.count && focus_to(game,game->units.items[0].x,game->units.items[0].y)<0)return -1;
        return game->runtime_host_quit?1:0;
    case FD2_FIELD_TURN_CLEAR_KEYS:
        return turn_present(game, FD2_FIELD_RUNTIME_CLEAR_KEYS, 0);
    case FD2_FIELD_TURN_INFO_VISIBLE:
        return turn_present(game, FD2_FIELD_RUNTIME_INFO_VISIBLE, value);
    case FD2_FIELD_TURN_RESET_DIRECTIONS:
        /* field_all_actor_directions_reset @0x134e4（原登记同名）：只写raw+3。 */
        for(size_t i=0;i<game->units.count;i++)game->units.items[i].direction=0;
        return 0;
    case FD2_FIELD_TURN_DELAY_MS:
        return turn_present(game,FD2_FIELD_RUNTIME_DELAY_MS,value);
    default: return -1;
    }
}

static int run_turn_entry(fd2_field_game *game, fd2_field_turn_entry entry) {
    return fd2_field_turn_run(entry,turn_apply,game);
}

static void advance_phase(fd2_field_game *game) {
    if (!game || game->failure != FD2_FIELD_FAILURE_NONE) return;
    fd2_field_turn_entry entry = game->active_side==2 ? FD2_FIELD_TURN_PLAYER_DONE :
        game->active_side==1 ? FD2_FIELD_TURN_SIDE1_DONE : FD2_FIELD_TURN_SIDE0_DONE;
    if (run_turn_entry(game,entry) < 0) fail_action(game,0);
}

/* field_actor_face_toward @0x1f04a（原sub_1F04A）：比较两个actor记录坐标，
 * 水平差严格大于垂直差时取左右（1/3），平价取上下（2/0）。 */
static uint8_t face_toward(const fd2_field_unit *from, const fd2_field_unit *to) {
    int dx = (int)from->x - (int)to->x, dy = (int)from->y - (int)to->y;
    return abs(dx) > abs(dy) ? (dx > 0 ? 1u : 3u) : (dy > 0 ? 2u : 0u);
}

/* 战场视窗左上角的像素坐标：格坐标×24、去4px黑边、加镜头像素偏移。 */
static int camera_origin_x(const fd2_field_game *game) {
    return game->camera_cell_x * 24 - FD2_FIELD_VIEW_BORDER + game->camera_pixel_offset_x;
}
static int camera_origin_y(const fd2_field_game *game) {
    return game->camera_cell_y * 24 - FD2_FIELD_VIEW_BORDER + game->camera_pixel_offset_y;
}

static void clear_view_border(fd2_vga *vga) {
    if (!vga || vga->field_page) return;
    /* 仅无App页的旧离屏诊断裁边。正式11cac只投影312×192，
     * 不拥有其它4096B VGA旧底；不能把“未写”改成写palette index0。 */
    for (int y = 0; y < VGA_H; y++) {
        uint8_t *row = vga->framebuffer + (size_t)y * VGA_STRIDE;
        if (y < FD2_FIELD_VIEW_BORDER ||
            y >= VGA_H - FD2_FIELD_VIEW_BORDER) {
            memset(row, 0, VGA_W);
        } else {
            memset(row, 0, FD2_FIELD_VIEW_BORDER);
            memset(row + VGA_W - FD2_FIELD_VIEW_BORDER, 0,
                   FD2_FIELD_VIEW_BORDER);
        }
    }
}

static int open_resources_for_flow(fd2_field_game *game,
                        const fd2_archive *fdother,
                        size_t stage, int source_only) {
    if (!game) return -1;
    memset(game, 0, sizeof(*game));
    if (!fdother) return -1;
    game->fdother = fdother;
    game->selected_unit = -1;
    game->command_selected = -1;
    fd2_field_system_menu_init(&game->system_menu);
    init_option_values(game);
    fd2_field_system_menu_set_options(&game->system_menu,
                                      game->option_values);
    refresh_system_menu_disabled(game);
    game->last_system_action = FD2_FIELD_SYSTEM_ACTION_NONE;
    game->manual_slot_picker.open = 0;
    game->save_resource_available = 0;
    game->stage_runtime_entry_offset = 0;
    game->detail_unit = -1;
    game->detail_acknowledged_unit = -1;

    /* 正式资源门按capability；显式source机制域保留30槽上界。 */
    if (source_only ? stage >= 30u : !fd2_field_stage_is_supported(stage)) return -1;

    if (fd2_archive_open(&game->field_archive,
                         fd2_asset_path(FD2_FDFIELD)) != 0 ||
        fd2_archive_open(&game->fdshap_archive,
                         fd2_asset_path(FD2_FDSHAP)) != 0 ||
        fd2_archive_open(&game->dato_archive,
                         fd2_asset_path(FD2_DATO)) != 0 ||
        fd2_archive_open(&game->fdtxt_archive,
                         fd2_asset_path(FD2_FDTXT)) != 0 ||
        fd2_archive_open(&game->figani_archive,
                         fd2_asset_path(FD2_FIGANI)) != 0 ||
        fd2_text_entry_open_entry(&game->ui_text,
                                  &game->fdtxt_archive, 0) != 0 ||
        fd2_font_open_fdother(&game->font, fdother, 4) != 0 ||
        fd2_field_map_open_stage(&game->map, &game->field_archive,
                                 stage) != 0 ||
        fd2_field_metadata_open_stage(&game->metadata,
                                      &game->field_archive, stage) != 0 ||
        fd2_field_placements_open_stage(&game->placements,
                                        &game->field_archive, stage) != 0 ||
        fd2_terrain_tileset_open_stage(&game->terrain,
                                       &game->fdshap_archive,
                                       &game->field_archive, stage) != 0 ||
        fd2_map_sprite_bank_open(&game->sprites,
                                 fd2_asset_path(FD2_FDICON)) != 0 ||
        fd2_field_visuals_open(&game->visuals, fdother) != 0 ||
        fd2_field_info_assets_open(&game->info_assets, fdother) != 0 ||
        fd2_field_command_assets_open(&game->command_assets, fdother) != 0)
        goto fail;

    /* 1088d@1097a/Load10010@10276的4df4c：资源raw3不是运行期marks初态。 */
    if(fd2_field_event_map_reset_cells(&game->map)!=0)goto fail;

    const uint8_t *palette;
    size_t palette_size;
    if (fd2_archive_get(fdother, 0, &palette, &palette_size) != 0 ||
        palette_size < 768)
        goto fail;

    game->stage = stage;
    game->camera_cell_x = 0;
    game->camera_cell_y = 0;
    game->cursor_cell_x = game->cursor_cell_y = 0;
    game->turn_number = 1;
    game->selected_unit = -1;
    game->save_resource_available = 0;
    game->stage_runtime_entry_offset = 0;
    game->detail_unit = -1;
    game->detail_acknowledged_unit = -1;
    game->command_selected = -1;
    game->attack_target = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->battle_result = FD2_FIELD_BATTLE_ONGOING;
    game->ready = 1;
    game->active_side = 2;
    refresh_system_menu_disabled(game);
    return 0;

fail:
    fd2_field_game_close(game);
    return -1;
}

int fd2_field_game_open_resources(fd2_field_game *game,
        const fd2_archive *fdother, size_t stage) {
    return open_resources_for_flow(game, fdother, stage, 0);
}

static int open_initial_game(fd2_field_game *game, fd2_vga *vga,
        const fd2_archive *fdother, size_t stage,
        const fd2_save_manual_state *initial_roster) {
    if (!game) return -1;
    if (!vga || stage != 0u) { memset(game, 0, sizeof(*game)); return -1; }
    if (fd2_field_game_open_resources(game, fdother, stage) != 0) return -1;
    if (load_initial_units(game, fd2_static_flow_lookup("scene", "scene.init.000"), initial_roster) != 0)
        goto fail;
    const uint8_t *palette;
    size_t palette_size;
    if (fd2_archive_get(fdother, 0, &palette, &palette_size) != 0 || palette_size < 768)
        goto fail;
    fd2_vga_set_palette(vga, palette);
    fd2_vga_set_brightness(vga, 0);
    /* 无handoff的field-play使用旧battle视口；自然START随后由同源开场的
     * handoff覆盖camera/cursor/idle，不以这里的调试入口值约束冷图。 */
    game->camera_cell_y = clamp_int(15, 0, max_camera_y(game));
    game->cursor_cell_x = clamp_int(6, 0, game->map.width - 1);
    game->cursor_cell_y = clamp_int(game->camera_cell_y + 4, 0, game->map.height - 1);
    enter_phase(game, 2);
    if (game->failure != FD2_FIELD_FAILURE_NONE) goto fail;
    game->ordinary_display_mode = 1;
    return 0;
fail:
    fd2_field_game_close(game);
    return -1;
}

int fd2_field_game_open(fd2_field_game *game, fd2_vga *vga,
        const fd2_archive *fdother, size_t stage) {
    return open_initial_game(game,vga,fdother,stage,NULL);
}

int fd2_field_game_open_from_roster(fd2_field_game *game, fd2_vga *vga,
        const fd2_archive *fdother, const fd2_save_manual_state *roster) {
    if (!roster || roster->unit_count!=4u) {
        if(game)memset(game,0,sizeof(*game));
        return -1;
    }
    return open_initial_game(game,vga,fdother,0,roster);
}

/* sub_1088D @0x1088d与field_stage1_entry_play @0x32d18（原sub_32D18）：只重建active交互字段，
 * 不调用初始构造器重置已成长的persistent记录。 */
static int prepare_deployment(fd2_field_game *game, const fd2_archive *fdother,
        const fd2_save_manual_state *state, size_t stage, size_t slots, int source_only) {
    if (!game) return -1;
    memset(game, 0, sizeof(*game));
    if (!fdother || !state || (!source_only && !slots) || slots > FD2_FIELD_MAX_UNITS || state->stage_id != stage ||
        !state->unit_count || state->unit_count > FD2_SAVE_MAX_UNITS) return -1;
    if (open_resources_for_flow(game, fdother, stage, source_only) != 0) return -1;
    if (source_only) slots = game->metadata.unknown_01;
    if (!slots || slots > FD2_FIELD_MAX_UNITS) goto fail;
    if (game->metadata.unknown_01 != slots) goto fail;
    memcpy(game->roster, state->unit_area, sizeof(game->roster));
    game->roster_count = state->unit_count;
    game->resource_value = state->gold_or_flags;
    game->stage_runtime_entry_offset = source_only ? 0 : 0x238du + (uint32_t)(stage - 1u) * 31u;
    game->option_values[0] = state->options[2];
    game->option_values[1] = state->options[3];
    game->option_values[2] = state->options[1];
    game->option_values[3] = state->options[0];
    fd2_field_system_menu_set_options(&game->system_menu, game->option_values);
    if (source_only) return 0; /* 1088d的active写集留在明确stage_setup_source叶。 */
    /* 1088d @10ae1..10b20：有限冷gate决定是否消费；跳空时roster和
     * placement都不前进，不能以active序号猜持久前缀的位置。 */
    if (fd2_field_prepare_deployment_project(state,stage,&game->map,&game->metadata,
            &game->placements,&game->units)!=0) goto fail;
    for (size_t i=0;i<game->units.count;i++)
        if (fd2_field_unit_cached_frame_index(&game->units,i,0)>=game->sprites.frame_count) goto fail;
    return 0;
fail:
    fd2_field_game_close(game);
    return -1;
}

/* sub_205DA @0x205da（原登记同名）：仅按明确源图准备1088d的base active。
 * 不预跑pan/group/script或对白后缀；manual输入的旧结果锁存不在SAV中，
 * 此入口只借现roster/资源，active仍为空；真实1088d写集由冷setup叶执行。 */
int fd2_field_game_prepare_stage_source_flow(fd2_field_game *game,
        const fd2_archive *fdother, const fd2_save_manual_state *state,
        const char *source_id) {
    if (!game) return -1;
    memset(game, 0, sizeof(*game));
    if (!state || !source_id || !state->stage_id || state->stage_id >= 30u) return -1;
    char expected[64];
    if (snprintf(expected, sizeof(expected), "stage.source.entry.%03u", (unsigned)state->stage_id) < 0 ||
            strcmp(source_id, expected)) return -1;
    const fd2_static_flow *flow = fd2_static_flow_lookup("stage", source_id);
    if (!flow) return -1;
    const fd2_static_flow_node *first = fd2_static_flow_at(flow, flow->entry);
    const fd2_static_flow *init = fd2_static_flow_lookup("stage", "stage.source.initialize");
    const fd2_static_flow_node *mode = init ? fd2_static_flow_at(init, init->entry) : NULL;
    const fd2_static_flow_node *reset = mode ? fd2_static_flow_at(init,
        fd2_static_flow_edge(init, mode, FD2_FLOW_NEXT)) : NULL;
    const fd2_static_flow_node *setup = reset ? fd2_static_flow_at(init,
        fd2_static_flow_edge(init, reset, FD2_FLOW_NEXT)) : NULL;
    /* 有效域固定为先205da；不能把构造预投影到用户重排的story前缀之前。 */
    if (!first || strcmp(first->op, "call_init") || !mode || !reset || !setup ||
            strcmp(mode->op, "display_mode") || mode->args[0] != 0 ||
            strcmp(reset->op, "entry_result_reset") || strcmp(setup->op, "stage_setup_source")) return -1;
    return prepare_deployment(game, fdother, state, state->stage_id, 0, 1);
}

/* persistent_roster_sync_from_battle @0x11506：固定记录投影算法，不决定剧情分支。 */
static int settlement_roster_sync(const fd2_field_units *units, fd2_save_manual_state *state) {
    for (size_t i = 0; i < units->count; i++) {
        const fd2_field_unit *active = &units->items[i];
        for (size_t j = 0; j < state->unit_count; j++) {
            fd2_field_unit persistent;
            memcpy(&persistent, state->unit_area + j * sizeof(persistent), sizeof(persistent));
            if (active->text_id != persistent.text_id ||
                (!persistent.text_id && fd2_field_unit_is_hidden(active))) continue;
            persistent = *active;
            memset((uint8_t *)&persistent + 0x22, 0, 6);
            persistent.flags &= 1u;
            if (persistent.flags != 1u) persistent.hp = persistent.hp_max;
            persistent.mp = persistent.mp_max;
            if (fd2_field_unit_persistent_stats_recompute(&persistent) != 0) return -1;
            memcpy(state->unit_area + j * sizeof(persistent), &persistent, sizeof(persistent));
        }
    }
    return 0;
}

static int common_event_append_group_wide(fd2_field_game *game, uint32_t group, int force);
static int common_event_append_group_units(fd2_field_units *units,
        const fd2_field_metadata *metadata, const fd2_field_placements *placements,
        const fd2_field_map *map, uint32_t group, int force);

/* sub_1088D @0x1088d（原同名）的内部credits资源域：只换实际resource30，
 * 不借205da重置turn/focus/event/result，也不把坏结局的热stage26改成30。
 * 同一Game继续拥有map/metadata/placements/terrain和原96条active记录。 */
int fd2_field_game_source_credits_setup(fd2_field_game *game) {
    if (!game || !game->ready || (game->stage!=26u && game->stage!=30u) ||
            !game->roster_count || game->roster_count>FD2_SAVE_BATTLE_MAX_ROSTER) return -1;
    fd2_field_map map={0}; fd2_field_metadata metadata={0};
    fd2_field_placements placements={0}; fd2_terrain_tileset terrain={0};
    int result=-1;
    if (fd2_field_map_open_stage(&map,&game->field_archive,30) ||
            fd2_field_metadata_open_stage(&metadata,&game->field_archive,30) ||
            fd2_field_placements_open_stage(&placements,&game->field_archive,30) ||
            fd2_terrain_tileset_open_stage(&terrain,&game->fdshap_archive,&game->field_archive,30) ||
            fd2_field_event_map_reset_cells(&map)) goto done;
    fd2_save_manual_state state={0}; fd2_field_units units;
    state.stage_id=30; state.unit_count=(uint8_t)game->roster_count;
    memcpy(state.unit_area,game->roster,sizeof(game->roster));
    if (fd2_field_prepare_deployment_project(&state,30,&map,&metadata,&placements,&units)) goto done;
    for (size_t i=0;i<units.count;i++)
        if (fd2_field_unit_cached_frame_index(&units,i,0)>=game->sprites.frame_count) goto done;
    /* group0 追加在局部 stage30 资源上完成；失败时 game 保持不变。 */
    if (common_event_append_group_units(&units,&metadata,&placements,&map,0,0)) goto done;
    fd2_field_map_close(&game->map); game->map=map; memset(&map,0,sizeof(map));
    fd2_field_metadata_close(&game->metadata); game->metadata=metadata; memset(&metadata,0,sizeof(metadata));
    fd2_field_placements_close(&game->placements); game->placements=placements; memset(&placements,0,sizeof(placements));
    fd2_terrain_tileset_close(&game->terrain); game->terrain=terrain; memset(&terrain,0,sizeof(terrain));
    game->units=units;
    result=0;
done:
    fd2_field_map_close(&map); fd2_field_metadata_close(&metadata);
    fd2_field_placements_close(&placements); fd2_terrain_tileset_close(&terrain);
    return result;
}

int fd2_field_game_stage_source_is_idle(const fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
            game->units.count > FD2_FIELD_MAX_UNITS || game->roster_count > FD2_SAVE_BATTLE_MAX_ROSTER ||
            game->event_log_count > sizeof(game->event_log) / sizeof(game->event_log[0]) ||
            progress_pending(game) || game->player_post_waiting_phase ||
            game->turn_post_caller != FD2_FIELD_TURN_POST_NONE ||
            game->loot.notice.kind != FD2_FIELD_LOOT_NONE || game->loot.resume != FD2_FIELD_PROGRESS_RESUME_NONE ||
            game->event_resume.child_wait || game->event_resume.completion != FD2_EVENT_COMPLETE_NONE ||
            game->interaction != FD2_FIELD_INTERACTION_BROWSE) return 0;
    for (size_t i = 0; i < game->event_log_count; i++)
        if (game->event_log[i].presentation_deferred) return 0;
    return 1;
}

/* 205da／33499／11506的源机制叶；分支及先后只由明确的stage源冷图控制。
 * 1=本叶已执行，0=交Scene呈现，-1=停在当前前缀；不查结果、不存新续态。 */
int fd2_field_game_stage_source_node(fd2_field_game *game,
        const fd2_static_flow_node *node, unsigned *port) {
    if (!game || !game->ready || !node || !node->op || !port ||
            game->units.count > FD2_FIELD_MAX_UNITS || game->roster_count > FD2_SAVE_BATTLE_MAX_ROSTER)
        return -1;
    const int32_t *a = node->args;
    *port = FD2_FLOW_NEXT;
    if (!strcmp(node->op, "stage_setup_source")) {
        if (!game->stage || game->stage >= 30u || !game->metadata.unknown_01) return -1;
        fd2_save_manual_state state = {0};
        state.stage_id = (uint8_t)game->stage;
        state.unit_count = (uint8_t)game->roster_count;
        memcpy(state.unit_area, game->roster, sizeof(game->roster));
        fd2_field_units units;
        if (fd2_field_prepare_deployment_project(&state, game->stage, &game->map, &game->metadata,
                &game->placements, &units) != 0) return -1;
        for (size_t i = 0; i < units.count; i++)
            if (fd2_field_unit_cached_frame_index(&units, i, 0) >= game->sprites.frame_count) return -1;
        game->units = units;
    } else if (!strcmp(node->op, "entry_result_reset")) {
        game->battle_result = FD2_FIELD_BATTLE_ONGOING;
    } else if (!strcmp(node->op, "entry_view_reset")) {
        game->camera_cell_x = game->camera_cell_y = 0;
        game->camera_pixel_offset_x = game->camera_pixel_offset_y = 0;
        game->cursor_cell_x = game->cursor_cell_y = 0;
    } else if (!strcmp(node->op, "entry_turn_reset")) {
        game->turn_number = 1;
    } else if (!strcmp(node->op, "view_set")) {
        for (size_t i = 0; i < 4; i++) if (a[i] < 0 || a[i] > 255) return -1;
        game->cursor_cell_x = a[0]; game->cursor_cell_y = a[1];
        game->camera_cell_x = a[2]; game->camera_cell_y = a[3];
        game->camera_pixel_offset_x = game->camera_pixel_offset_y = 0;
    } else if (!strcmp(node->op, "clear_acted")) {
        for (size_t i = 0; i < game->units.count; i++) fd2_field_unit_set_acted(&game->units.items[i], 0);
    } else if (!strcmp(node->op, "unit_id_position")) {
        /* 24ec6..24ef0：只改first..count中ID匹配的x/y；不按HP/hidden过滤。 */
        if (a[0] < 0 || a[0] > FD2_FIELD_MAX_UNITS || a[1] < 0 || a[1] > UINT8_MAX ||
                a[2] < 0 || a[2] > UINT8_MAX || a[3] < 0 || a[3] > UINT8_MAX) return -1;
        for (size_t i = (size_t)a[0]; i < game->units.count; i++) if (game->units.items[i].unit_id == (uint8_t)a[1]) {
            game->units.items[i].x = (uint8_t)a[2]; game->units.items[i].y = (uint8_t)a[3];
        }
    } else if (!strcmp(node->op, "branch_any_hidden")) {
        if (a[0] < 0 || a[1] < a[0] || (size_t)a[1] >= game->units.count) return -1;
        int hidden = 0;
        for (int32_t i = a[0]; i <= a[1]; i++) if (fd2_field_unit_is_hidden(&game->units.items[i])) hidden = 1;
        *port = hidden ? FD2_FLOW_YES : FD2_FLOW_NO;
    } else if (!strcmp(node->op, "fail")) {
        return -1;
    } else if (!strcmp(node->op, "event_byte_write") || !strcmp(node->op, "unit_field_write") ||
            !strcmp(node->op,"event_item_consume") || !strcmp(node->op,"unit_hp_zero_tail") ||
            !strcmp(node->op,"map_reset_cells") ||
            !strncmp(node->op, "branch_", 7)) {
        return common_event_domain(game, 0, node, port, 0);
    } else if (!strcmp(node->op,"grant_party_item")) {
        if (a[0]<0 || a[0]>255) return -1;
        /* 同1c220：side2/raw首空槽，hidden/acted不排除，不重算stats。 */
        for (size_t i=0;i<game->units.count;i++)
            if (game->units.items[i].side==2u &&
                    inventory_first_empty_add(&game->units.items[i],(uint8_t)a[0])==1) break;
    } else if (!strcmp(node->op, "display_mode")) {
        if (a[0] < 0 || a[0] > 1) return -1;
        game->ordinary_display_mode = (uint8_t)a[0];
    } else if (!strcmp(node->op, "append_group")) {
        if (a[0] < 0 || a[0] > UINT8_MAX || a[1] < 0 || a[1] > 1 || a[2] != 0)
            return -1; /* 到场prepare/frames必须由其已证冷序列另行表达。 */
        return common_event_append_group_wide(game, (uint32_t)a[0], a[1]) == 0 ? 1 : -1;
    } else if (!strcmp(node->op, "reset_directions")) {
        for (size_t i = 0; i < game->units.count; i++) game->units.items[i].direction = 0;
    } else if (!strcmp(node->op, "roster_append")) {
        if (a[0] < 0 || a[0] > 31 || append_persistent_player(game, (uint8_t)a[0]) != 0) return -1;
    } else if (!strcmp(node->op, "roster_sync")) {
        fd2_save_manual_state state = {0};
        state.unit_count = (uint8_t)game->roster_count;
        memcpy(state.unit_area, game->roster, sizeof(game->roster));
        int result = settlement_roster_sync(&game->units, &state);
        /* 首错之前已经同步的持久记录不回滚。 */
        memcpy(game->roster, state.unit_area, sizeof(game->roster));
        if (result != 0) return -1;
    } else if (!strcmp(node->op, "bgm")) {
        if (a[0]<-1 || a[0]>255 || turn_present(game,FD2_FIELD_RUNTIME_MUSIC_TRACK,a[0])<0) return -1;
    } else if (!strcmp(node->op, "roster_restore")) {
        /* sub_25089：逐persistent flags=0、HP=HPmax、MP=MPmax，无active同步。 */
        for (size_t i=0;i<game->roster_count;i++) {
            game->roster[i].flags=0;
            game->roster[i].hp=game->roster[i].hp_max;
            game->roster[i].mp=game->roster[i].mp_max;
        }
    } else if (!strcmp(node->op, "defeated_finalize")) {
        int result=finalize_zero_hp_units(game);
        if (result<0) return -1;
    } else if (!strcmp(node->op, "next_stage")) {
        if (a[0] < 1 || a[0] > 30) return -1;
        game->stage = (size_t)a[0];
    } else return 0;
    return 1;
}

typedef struct {
    const fd2_field_game *resources;
    fd2_field_game *entry_game;
    fd2_field_units *units;
    const fd2_archive *fdother;
    const fd2_save_manual_state *deployment;
    fd2_save_manual_state *prepared;
    const fd2_save_manual_state *initial_roster; /* 仅START已构造来源，不新增热owner。 */
} stage_flow_context;

/* entry/victory/settlement共用冷节点；Scene只消费相同来源的present视图。 */
static int run_stage_logic(const fd2_static_flow *flow, stage_flow_context *context) {
    if (!flow || !context || !context->resources || !context->units) return -1;
    int pc = flow->entry;
    for (unsigned budget = 0; budget < FD2_FLOW_STEP_BUDGET; budget++) {
        const fd2_static_flow_node *n = fd2_static_flow_at(flow, pc);
        if (!n) return -1;
        unsigned port = FD2_FLOW_NEXT;
        if (n->views & FD2_FLOW_LOGIC) {
            fd2_field_units *units = context->units;
            fd2_field_game *entry = context->entry_game;
            fd2_save_manual_state *state = context->prepared;
            const char *op = n->op;
            if (!strcmp(op, "preflight_before_music") || !strcmp(op, "victory_music")) {
                /* App owner消费线性前缀；这里仅投影单位，不能切曲。 */
            } else if (!strcmp(op, "init_player")) {
                size_t actor = (size_t)n->args[0], tail = (size_t)n->args[2], ordinal = (size_t)n->args[3];
                if (!entry || n->args[0] < 0 || actor != units->count || actor >= FD2_FIELD_MAX_UNITS ||
                    n->args[1] < 0 || n->args[1] > UINT8_MAX || n->args[2] <= 0 ||
                    tail > entry->placements.count || n->args[3] < 0 || ordinal >= tail)
                    return -1;
                uint8_t x, y;
                if (fd2_field_unit_deployment_position(&entry->metadata, &entry->placements,
                        ordinal, &x, &y) != 0 || x >= entry->map.width || y >= entry->map.height)
                    return -1;
                size_t persistent;
                if(context->initial_roster) {
                    if(entry->roster_count!=tail || actor>=entry->roster_count ||
                        entry->roster[actor].unit_id!=(uint8_t)n->args[1] ||
                        entry->roster[actor].text_id!=(uint8_t)n->args[1])return -1;
                    persistent=actor;
                } else {
                    if(append_persistent_player(entry,(uint8_t)n->args[1])!=0)return -1;
                    persistent=entry->roster_count-1u;
                }
                if (fd2_field_units_set(units, actor, (uint8_t)n->args[1], (uint8_t)n->args[1], 2,
                        x, y) != 0) return -1;
                /* 1088d拷persistent后部署；正式START的112a5已由Scene原写点完成，
                 * 独立field入口才在本TU构造。成员和部署引用仍取同一冷图。 */
                fd2_field_unit *active = &units->items[actor];
                *active = entry->roster[persistent];
                active->x = x; active->y = y;
                active->direction = active->frame_phase = 0;
                ((uint8_t *)active)[0x31] = 0xff;
                if (fd2_field_units_sprite_cache_append(units, actor) != 0) return -1;
            } else if (!strcmp(op, "init_templates")) {
                if (!entry || n->args[0] < 0 || n->args[1] < -1 || n->args[2] < 0 ||
                    (size_t)n->args[2] != units->count || n->args[3] < 0 || n->args[3] > 1 ||
                    n->args[4] < 0 || n->args[4] > 1) return -1;
                size_t first = (size_t)n->args[0], templates;
                if (fd2_field_unit_template_count(&entry->metadata, &templates) != 0) return -1;
                size_t end = n->args[1] < 0 ? templates : first + (size_t)n->args[1];
                if (end > templates || end > entry->placements.count || first > end) return -1;
                for (size_t i = first; i < end; i++) {
                    const fd2_field_placement *position = &entry->placements.records[i];
                    if (n->args[3] && !position->x && !position->y) break;
                    size_t actor = units->count;
                    if (fd2_field_units_append_template(units, &entry->metadata, &entry->placements, i) != 0)
                        return -1;
                    if (n->args[4]) fd2_field_unit_set_hidden(&units->items[actor], 1);
                }
            } else if (!strcmp(op, "stage_setup")) {
                if (!entry || !context->deployment || prepare_deployment(entry, context->fdother,
                        context->deployment, (size_t)n->args[0], (size_t)n->args[1], 0) != 0 ||
                        entry->metadata.unknown_01 != n->args[3]) return -1;
            } else if (!strcmp(op, "pan")) {
                if (!entry) return -1;
                entry->camera_cell_x = n->args[0]; entry->camera_cell_y = n->args[1];
            } else if (!strcmp(op, "move")) {
                const uint8_t *script; size_t size;
                if (fd2_field_movement_script_get((uint8_t)n->args[0], &script, &size) != 0 ||
                    fd2_field_movement_script_apply(units, script, size) != 0) return -1;
            } else if (!strcmp(op, "append_group")) {
                if (append_group_units(context->resources, units, (uint8_t)n->args[0], n->args[1]) != 0) return -1;
            } else if (!strcmp(op, "reset_directions")) {
                for (size_t i = 0; i < units->count; i++) units->items[i].direction = 0;
            } else if (!strcmp(op, "focus_actor")) {
                size_t at = (size_t)n->args[0];
                if (!entry || at >= units->count || focus_to(entry, units->items[at].x, units->items[at].y) != 0) return -1;
                if(entry->runtime_host_quit)return 0;
            } else if (!strcmp(op, "assert_unit_count")) {
                if (units->count != (size_t)n->args[0]) return -1;
            } else if (!strcmp(op, "display_mode")) {
                if (!entry) return -1;
                entry->ordinary_display_mode = (uint8_t)n->args[0];
            } else if (!strcmp(op, "assert_roster_capacity")) {
                if (context->resources->roster_count + (size_t)n->args[0] > FD2_SAVE_BATTLE_MAX_ROSTER) return -1;
            } else if (!strcmp(op, "branch_any_hidden")) {
                int hidden = 0;
                if (n->args[0] < 0 || n->args[0] > n->args[1] ||
                        (size_t)n->args[1] >= units->count) return -1;
                for (int32_t i = n->args[0]; i <= n->args[1]; i++)
                    if (fd2_field_unit_is_hidden(&units->items[i])) hidden = 1;
                port = hidden ? FD2_FLOW_YES : FD2_FLOW_NO;
            } else if (!strcmp(op, "grant_party_item")) {
                if (n->args[0] < 0 || n->args[0] > UINT8_MAX) return -1;
                for (size_t i = 0; i < units->count; i++)
                    if (units->items[i].side == 2u && inventory_first_empty_add(&units->items[i], (uint8_t)n->args[0]) == 1) break;
            } else if (!strcmp(op, "next_stage")) {
                if (!state || n->args[0] < 1 || n->args[0] > 30) return -1;
                state->stage_id = (uint8_t)n->args[0];
            } else if (!strcmp(op, "roster_append")) {
                if (!state || state->unit_count >= FD2_SAVE_BATTLE_MAX_ROSTER ||
                        n->args[0] < 0 || n->args[0] > 31) return -1;
                fd2_field_unit added;
                uint8_t *target = state->unit_area + state->unit_count * sizeof(added);
                memcpy(&added, target, sizeof(added));
                if (fd2_field_unit_persistent_create(&added, (uint8_t)n->args[0]) != 0) return -1;
                memcpy(target, &added, sizeof(added)); state->unit_count++;
            } else if (!strcmp(op, "roster_sync")) {
                if (!state || settlement_roster_sync(units, state) != 0) return -1;
            } else if (!strcmp(op, "end")) return 0;
            else return -1;
        }
        pc = fd2_static_flow_edge(flow, n, port);
        if (pc < 0) return -1;
    }
    return -1;
}

/* new_game_opening_play @0x3231b：Scene与Game共用初始成员和模板选择，
 * 仅Scene消费present-only的开场前藏显；Game不重放对白和移动。 */
static int load_initial_units(fd2_field_game *game, const fd2_static_flow *flow,
        const fd2_save_manual_state *initial_roster) {
    if (!game || !flow) return -1;
    fd2_field_units_clear(&game->units);
    game->roster_count = 0;
    if(initial_roster) {
        memcpy(game->roster,initial_roster->unit_area,sizeof(game->roster));
        game->roster_count=initial_roster->unit_count;
    }
    stage_flow_context context = {game, game, &game->units, NULL, NULL, NULL,initial_roster};
    if (run_stage_logic(flow, &context) != 0) return -1;
    return validate_initial_units(game);
}

static int prepare_stage_flow(fd2_field_game *game, const fd2_archive *fdother,
        const fd2_save_manual_state *state, const fd2_static_flow *flow) {
    if (!game) return -1;
    memset(game, 0, sizeof(*game));
    stage_flow_context context = {game, game, &game->units, fdother, state, NULL,NULL};
    if (run_stage_logic(flow, &context) == 0) return 0;
    fd2_field_game_close(game);
    return -1;
}

int fd2_field_game_prepare_stage(fd2_field_game *game,
        const fd2_archive *fdother, const fd2_save_manual_state *state) {
    if (game && state && fd2_field_stage_source_is_bound(state->stage_id, "entry")) {
        if (!fd2_field_stage_is_supported(state->stage_id)) {
            memset(game, 0, sizeof(*game)); return -1;
        }
        char id[64];
        int length=snprintf(id,sizeof(id),"stage.source.entry.%03u",(unsigned)state->stage_id);
        if (length<0 || (size_t)length>=sizeof(id)) { memset(game,0,sizeof(*game)); return -1; }
        int result=fd2_field_game_prepare_stage_source_flow(game,fdother,state,id);
        if (result==0) game->stage_runtime_entry_offset=0x238du+(uint32_t)(state->stage_id-1u)*31u;
        return result;
    }
    return prepare_stage_flow(game, fdother, state,
        state ? stage_flow_get("entry", state->stage_id) : NULL);
}

int fd2_field_game_prepare_stage1(fd2_field_game *game,
        const fd2_archive *fdother, const fd2_save_manual_state *state) {
    return prepare_stage_flow(game, fdother, state, stage_flow_get("entry", 1));
}

/* field_stage2_entry_play @0x32e8c（原sub_32E8C）：script18先于group1的顺序由图保留。 */
int fd2_field_game_prepare_stage2(fd2_field_game *game,
        const fd2_archive *fdother, const fd2_save_manual_state *state) {
    if (!state || state->stage_id!=2u) { if(game)memset(game,0,sizeof(*game));return -1; }
    return fd2_field_game_prepare_stage(game,fdother,state);
}

/* field_stage1_victory_play @0x22f37（原sub_22F37）：只读投影原版提交顺序；
 * field_party_item_append @0x1c220（原sub_1C220）仅按side2/raw顺序找首空槽，
 * 不排除hidden或acted角色，满员背包无提示且不重算装备派生值。 */
int fd2_field_game_settlement_project_units(const fd2_field_game *game,
        fd2_field_units *out) {
    if (!game || !out || !game->ready || !fd2_field_stage_battle_is_supported(game->stage) ||
        game->failure != FD2_FIELD_FAILURE_NONE || progress_pending(game) ||
        game->units.count > FD2_FIELD_MAX_UNITS || game->roster_count > FD2_SAVE_BATTLE_MAX_ROSTER) return -1;
    fd2_field_battle_result result;
    if(fd2_field_game_battle_result_query(game,&result)!=0 || result!=FD2_FIELD_BATTLE_VICTORY)return -1;
    fd2_field_units shadow = game->units;
    stage_flow_context context = {game, NULL, &shadow, NULL, NULL, NULL,NULL};
    if (run_stage_logic(stage_flow_get("victory", game->stage), &context) != 0) return -1;
    *out = shadow;
    return 0;
}

/* persistent_roster_sync_from_battle @0x11506（原sub_11506）：按text_id在原raw顺序同步，保留未匹配及未用尾。 */
int fd2_field_game_settlement_prepare(const fd2_field_game *game,
        fd2_save_manual_state *out) {
    fd2_field_units units;
    if (!out || fd2_field_game_settlement_project_units(game, &units) != 0) return -1;
    fd2_save_manual_state shadow = {0};
    shadow.unit_count = (uint8_t)game->roster_count;
    shadow.gold_or_flags = game->resource_value;
    shadow.options[0] = game->option_values[3]; shadow.options[1] = game->option_values[2];
    shadow.options[2] = game->option_values[0]; shadow.options[3] = game->option_values[1];
    memcpy(shadow.unit_area, game->roster, sizeof(game->roster));
    stage_flow_context context = {game, NULL, &units, NULL, NULL, &shadow,NULL};
    if (run_stage_logic(stage_flow_get("settlement", game->stage), &context) != 0) return -1;
    *out = shadow;
    return 0;
}

int fd2_field_game_settlement_commit(fd2_field_game *game, fd2_save_manual_state *out) {
    fd2_save_manual_state shadow;
    if (!out || fd2_field_game_settlement_prepare(game, &shadow) != 0) return -1;
    memcpy(game->roster, shadow.unit_area, sizeof(game->roster));
    game->roster_count = shadow.unit_count;
    game->stage = shadow.stage_id;
    game->ready = 0; /* 旧stage资源仍由close释放，不允许继续导出/操作。 */
    *out = shadow;
    return 0;
}

/* main @0x25bf4的胜利caller已经按源图完成11506/追加/next_stage。
 * 此处仅导出原持久owner，不再投影active或重放结算；旧资源由App随后close。 */
int fd2_field_game_export_preparation(const fd2_field_game *game, fd2_save_manual_state *out) {
    fd2_field_stage_capability capability;
    if (!out || !fd2_field_game_stage_source_is_idle(game) || !game->stage ||
        game->stage>=30u || game->roster_count>FD2_SAVE_BATTLE_MAX_ROSTER ||
        !fd2_field_prepare_stage_is_valid(game->stage) ||
        fd2_field_stage_capability_get(game->stage,&capability)!=0 ||
        (!capability.preparation_slots && !fd2_field_stage_source_is_bound(game->stage,"entry"))) return -1;
    fd2_save_manual_state shadow={0};
    shadow.unit_count=(uint8_t)game->roster_count;shadow.stage_id=(uint8_t)game->stage;
    shadow.gold_or_flags=game->resource_value;
    shadow.options[0]=game->option_values[3];shadow.options[1]=game->option_values[2];
    shadow.options[2]=game->option_values[0];shadow.options[3]=game->option_values[1];
    memcpy(shadow.unit_area,game->roster,sizeof(game->roster));
    *out=shadow;return 0;
}

int fd2_field_game_apply_handoff(fd2_field_game *game,
                                 const fd2_field_handoff *handoff) {
    if (!game || !game->ready || !handoff || !handoff->valid ||
        handoff->stage != game->stage ||
        handoff->units.count != game->units.count)
        return -1;

    /* 先验证索引与稳定 unit ID，避免部分写入后才发现过场和正式 roster
     * 不一致。stage 0 的玩家、group 1/2 顺序已经由开场流程确认。 */
    for (size_t i = 0; i < game->units.count; i++) {
        if (game->units.items[i].unit_id != handoff->units.items[i].unit_id)
            return -1;
    }

    for (size_t i = 0; i < game->units.count; i++) {
        fd2_field_unit *dst = &game->units.items[i];
        const fd2_field_unit *src = &handoff->units.items[i];
        dst->x = src->x;
        dst->y = src->y;
        dst->direction = src->direction;
        dst->frame_phase = src->frame_phase;
        fd2_field_unit_set_hidden(dst, fd2_field_unit_is_hidden(src));
        game->units.walk_frames[i] = handoff->units.walk_frames[i];
    }

    game->camera_cell_x = clamp_int(handoff->camera_cell_x,
                                    0, max_camera_x(game));
    game->camera_cell_y = clamp_int(handoff->camera_cell_y,
                                    0, max_camera_y(game));
    game->cursor_cell_x = clamp_int(handoff->focus_cell_x,
                                    0, game->map.width - 1);
    game->cursor_cell_y = clamp_int(handoff->focus_cell_y,
                                    0, game->map.height - 1);
    game->idle_phase = handoff->idle_phase;
    game->last_idle_ms = 0;
    game->last_terrain_visual_ms = 0;
    game->last_range_visual_ms = 0;
    game->last_command_highlight_ms = 0;
    game->selected_unit = -1;
    game->command_selected = -1;
    fd2_field_system_menu_init(&game->system_menu);
    fd2_field_system_menu_set_options(&game->system_menu,
                                      game->option_values);
    game->last_system_action = FD2_FIELD_SYSTEM_ACTION_NONE;
    game->manual_slot_picker.open = 0;
    game->save_resource_available = 0;
    game->stage_runtime_entry_offset = fd2_field_stage_prepare_record_get(game->stage)
        ? (uint32_t)(0x238du + (game->stage - 1u) * FD2_FIELD_PREPARE_RECORD_SIZE) : 0u;
    game->attack_target = -1;
    game->last_attack_valid = 0;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->ordinary_display_mode = 1;
    return 0;
}

void fd2_field_game_close(fd2_field_game *game) {
    if (!game) return;
    fd2_field_path_close(&game->move_range);
    free(game->move_path);
    fd2_image_free(&game->detail_portrait);
    game->detail_visible = 0;
    game->detail_page = FD2_FIELD_DETAIL_EQUIPMENT;
    fd2_field_command_assets_close(&game->command_assets);
    fd2_field_info_assets_close(&game->info_assets);
    fd2_field_visuals_close(&game->visuals);
    fd2_map_sprite_bank_close(&game->sprites);
    fd2_terrain_tileset_close(&game->terrain);
    fd2_field_placements_close(&game->placements);
    fd2_field_metadata_close(&game->metadata);
    fd2_field_map_close(&game->map);
    fd2_text_entry_close(&game->ui_text);
    fd2_archive_close(&game->figani_archive);
    fd2_archive_close(&game->fdtxt_archive);
    fd2_archive_close(&game->dato_archive);
    fd2_archive_close(&game->fdshap_archive);
    fd2_archive_close(&game->field_archive);
    memset(game, 0, sizeof(*game));
    game->selected_unit = -1;
    game->command_selected = -1;
    fd2_field_system_menu_init(&game->system_menu);
    game->last_system_action = FD2_FIELD_SYSTEM_ACTION_NONE;
    game->manual_slot_picker.open = 0;
    game->detail_unit = -1;
    game->detail_acknowledged_unit = -1;
}

/* field_view_render_tiles @0x11eee（原同名）首部：仅观察到BIOS变化时翻转一次
 * terrain相位并重取锚点。正式路径沿共享PIT clock；无owner回退仅供headless。
 * 1e611直调11eee，不经1297d；只能写这两个既有Game字段。 */
int fd2_field_game_tick_terrain_visual(fd2_field_game *game, uint64_t now_ns) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE) return -1;
    if (game->visual_owner) {
        if (fd2_field_visual_tiles_step(game->visual_owner, now_ns, -1) < 0) return -1;
        game->visual_now_ns = now_ns;
        game->terrain_visual_phase = (uint8_t)game->visual_owner->terrain_phase;
        game->range_visual_phase = (uint8_t)game->visual_owner->range_phase;
        return 0;
    }
    uint64_t now_ms=now_ns/UINT64_C(1000000);
    if (game->last_terrain_visual_ms == 0) {
        game->last_terrain_visual_ms = now_ms;
    } else {
        if (now_ms < game->last_terrain_visual_ms ||
            now_ms - game->last_terrain_visual_ms >= FD2_FIELD_BIOS_TICK_MS) {
            game->terrain_visual_phase ^= 1u;
            game->last_terrain_visual_ms = now_ms;
        }
    }
    return 0;
}

int fd2_field_game_bind_visual(fd2_field_game *game, fd2_vga *vga) {
    if (!game || !game->ready || !vga || !vga->field_page || !game->fdother ||
        fd2_field_background_rebuild(vga->field_page, game->fdother, game->stage) != 0)
        return -1;
    game->visual_owner = &vga->field_visual;
    game->idle_phase = (uint8_t)vga->field_visual.actor_idle_phase;
    game->terrain_visual_phase = (uint8_t)vga->field_visual.terrain_phase;
    game->range_visual_phase = (uint8_t)vga->field_visual.range_phase;
    game->info_panel_right = vga->info_panel_right;
    return 0;
}

/* field_animation_phase_update @0x1297d（原同名）：超4 BIOS tick或回绕时
 * idle加一并重取last，cursor每调用加一，不补追全部经过周期。
 * 正式App沿现visual_owner，headless保原idle回退；本叶不调用11eee，
 * 不推进raw26/water/AI/输入/RNG、菜单、单位或持久状态。 */
int fd2_field_game_tick_actor_phase_visual(fd2_field_game *game, uint64_t now_ns) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE) return -1;
    if (game->visual_owner) {
        if (fd2_field_visual_actor_step(game->visual_owner, now_ns) < 0) return -1;
        game->idle_phase = (uint8_t)game->visual_owner->actor_idle_phase;
        return 0;
    }
    uint64_t now_ms=now_ns/UINT64_C(1000000);
    if (game->last_idle_ms == 0) {
        game->last_idle_ms = now_ms;
    } else {
        if (now_ms < game->last_idle_ms ||
            now_ms - game->last_idle_ms >= FD2_FIELD_IDLE_MS) {
            game->idle_phase = (uint8_t)((game->idle_phase + 1u) & 3u);
            game->last_idle_ms = now_ms;
        }
    }
    return 0;
}

int fd2_field_game_tick_visual(fd2_field_game *game, uint64_t now_ns) {
    if (fd2_field_game_tick_actor_phase_visual(game, now_ns) != 0) return -1;
    return fd2_field_game_tick_terrain_visual(game, now_ns);
}

/* headless 周期补追：返回自 *last 起完整经过的周期数并推进 *last。
 * *last==0 表示尚未起算；时钟回退时只重取 *last，不补追（否则无符号差回绕）。 */
static uint64_t headless_elapsed_periods(uint64_t now_ms, uint64_t *last,
                                         uint64_t period_ms) {
    if (*last == 0 || now_ms < *last) {
        *last = now_ms;
        return 0;
    }
    uint64_t periods = (now_ms - *last) / period_ms;
    *last += periods * period_ms;
    return periods;
}

void fd2_field_game_tick(fd2_field_game *game, uint64_t now_ns) {
    if (fd2_field_game_tick_visual(game, now_ns) != 0) return;
    uint64_t now_ms=now_ns/UINT64_C(1000000);
    /* 正式range与菜单从同一App owner借读，不走旧headless时钟。 */
    if (game->visual_owner) goto transient_deadlines;
    game->range_visual_phase = (uint8_t)((game->range_visual_phase +
        headless_elapsed_periods(now_ms, &game->last_range_visual_ms,
                                 FD2_FIELD_RANGE_VISUAL_MS)) %
        FD2_FIELD_RANGE_PHASE_COUNT);
    if (game->interaction == FD2_FIELD_INTERACTION_COMMAND ||
        game->interaction == FD2_FIELD_INTERACTION_ITEM_MENU) {
        game->command_highlight_phase ^= (uint8_t)(headless_elapsed_periods(now_ms,
            &game->last_command_highlight_ms, FD2_FIELD_COMMAND_HIGHLIGHT_MS) & 1u);
        game->system_menu.highlight_phase = 0;
        game->system_menu.last_highlight_ms = 0;
    } else if (game->interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU) {
        game->command_highlight_phase = 0;
        game->last_command_highlight_ms = 0;
        game->system_menu.highlight_phase ^= (uint8_t)(headless_elapsed_periods(now_ms,
            &game->system_menu.last_highlight_ms, FD2_FIELD_COMMAND_HIGHLIGHT_MS) & 1u);
    } else {
        game->command_highlight_phase = 0;
        game->last_command_highlight_ms = 0;
        game->system_menu.highlight_phase = 0;
        game->system_menu.last_highlight_ms = 0;
    }
transient_deadlines:
    fd2_field_game_tick_deadlines(game,now_ms);
}

void fd2_field_game_tick_deadlines(fd2_field_game *game,uint64_t now_ms) {
    if(!game || !game->ready || game->failure!=FD2_FIELD_FAILURE_NONE)return;
    if (game->system_result_fragment != 0 &&
        now_ms >= game->system_result_until_ms) {
        game->system_result_fragment = 0;
        game->system_result_until_ms = 0;
    }
    if (game->manual_slot_result_fragment != 0 &&
        now_ms >= game->manual_slot_result_until_ms) {
        game->manual_slot_result_fragment = 0;
        game->manual_slot_result_until_ms = 0;
    }

    /* field_side1_phase_execute @0x1d80b / 正确 dual 0x1d80b 与
     * field_side0_phase_execute @0x1d8ba / 正确 dual 0x1d8ba：自动阶段
     * 按 raw actor index 逐个进入 AI core；完整演出由 play loop 显式
     * visual 调用，tick 保持无窗口逻辑路径。 */
    if (game->active_side != 2 && !progress_pending(game) &&
        game->interaction == FD2_FIELD_INTERACTION_BROWSE) {
        if (game->phase_next_action_ms == 0) {
            game->phase_next_action_ms =
                now_ms + FD2_FIELD_AUTOMATIC_ACTION_MS;
        } else if (now_ms >= game->phase_next_action_ms) {
            /* 只发布 due 标记；field_play 的 VGA owner 同步执行一次动作。 */
            game->phase_next_action_ms = now_ms;
        }
    }
}

static void render_move_selection(const fd2_field_game *game,
                                  fd2_vga *vga,
                                  int camera_x, int camera_y);
static void render_field_cursor(const fd2_field_game *game,
                                fd2_vga *vga,
                                int camera_x, int camera_y, int normal);
static void render_actor_terrain_overlays(const fd2_field_game *game,
                                          fd2_vga *vga,
                                          int camera_x, int camera_y, int draw_ui, uint8_t terrain_phase);
static void render_field_info_at(const fd2_field_game *game, fd2_vga *vga,
    const fd2_field_units *units, int focus_x, int focus_y,
    int panel_right, uint8_t idle_phase);
static void render_field_info(const fd2_field_game *game, fd2_vga *vga,
    int panel_right, uint8_t idle_phase) {
    render_field_info_at(game, vga, &game->units, game->cursor_cell_x,
        game->cursor_cell_y, panel_right, idle_phase);
}
static void render_field_command(const fd2_field_game *game,
                                 fd2_vga *vga,
                                 int camera_x, int camera_y);
static void render_field_command_icons(const fd2_field_game *game,
    fd2_vga *vga,int camera_x,int camera_y);
static void render_field_system_menu(const fd2_field_game *game,
                                     fd2_vga *vga,
                                     int camera_x, int camera_y);
static void render_manual_slot_picker(const fd2_field_game *game,
                                      fd2_vga *vga);
static void render_manual_slot_result(const fd2_field_game *game,
                                      fd2_vga *vga);
static void render_system_result(const fd2_field_game *game,
                                 fd2_vga *vga);

static int render_menu_page_overlay(const fd2_field_game *game,fd2_vga *vga,int live);

static int field_info_panel_side(const fd2_field_game *game) {
    int rel_x = game->cursor_cell_x - game->camera_cell_x;
    int rel_y = game->cursor_cell_y - game->camera_cell_y;
    if (rel_y >= 6 && rel_x <= 2) return 1;
    if (rel_y >= 6 && rel_x >= 10) return 0;
    return game->info_panel_right;
}

/* 正式11eee/127a9共享456页；离屏纯像素测试不拥有App页时仍使用
 * 下方旧VGA投影。这里的scratch只从同一page读写选择层，不读显示页。 */
static int render_work_page(const fd2_field_game *game, fd2_vga *vga,
    int draw_selection, int draw_info, int project,
    uint8_t idle_phase, uint8_t terrain_phase, fd2_field_game *live_owner,
    fd2_field_game *selection_owner) {
    fd2_field_page *page = vga->field_page;
    if (!page) return -1;
    /* 正式11eee按原读取点推进；纯投影不代替这个调用合同。 */
    if(live_owner) {
        if(live_owner!=game || !live_owner->ready || live_owner->failure!=FD2_FIELD_FAILURE_NONE ||
            live_owner->visual_owner!=&vga->field_visual ||
            fd2_field_visual_terrain_step_live(vga,live_owner->visual_owner)<0)return -1;
        terrain_phase=(uint8_t)live_owner->visual_owner->terrain_phase;
        live_owner->terrain_visual_phase=terrain_phase;
    }
    const fd2_field_path_result *range = NULL;
    const uint8_t *lut = NULL;
    int map_x = game->camera_cell_x, map_y = game->camera_cell_y;
    int background_map_x = map_x;
    int dest_x = 72, dest_y = 72, cols = 13, rows = 8;
    int offset_x = game->camera_pixel_offset_x, offset_y = game->camera_pixel_offset_y;
    int moving = game->interaction == FD2_FIELD_INTERACTION_MOVING;
    int parallax_x = 0, parallax_y = 0;
    if (moving && game->selected_unit >= 0 &&
        (size_t)game->selected_unit < game->units.count) {
        const fd2_field_unit *actor = &game->units.items[game->selected_unit];
        unsigned step = actor->frame_phase;
        if (actor->direction == 0) {
            rows = offset_y ? 9 : 8;
            parallax_y = offset_y ? (int)step : 0;
        } else if (actor->direction == 3) {
            cols = offset_x ? 14 : 13;
            parallax_x = offset_x ? (int)step : 0;
        }
        else if (actor->direction == 1) {
            /* 1300d@130cb即使cameraX0也减1；pan6/2抵回正常BG源0。
             * tile另读有限linear源，不能让二维query改写原BG参数。 */
            background_map_x--;
            /* 1300d@130cb无条件借左margin；page block支持其有限linear源。 */
            map_x--; dest_x = 48; cols = 14;
            parallax_x = 6 - (offset_x ? (int)step : 0);
        } else if (actor->direction == 2) {
            rows = 9;
            if (map_y) { map_y--; dest_y = 48; }
            parallax_y = game->camera_cell_y ? 6 - (offset_y ? (int)step : 0) : 0;
        }
    }
    /* 右/下额外margin仍限制到原map末端；左x=-1有独立合法linear源。 */
    if (cols > game->map.width - map_x) cols = game->map.width - map_x;
    if (rows > game->map.height - map_y) rows = game->map.height - map_y;
    /* 12eaa/13315正向53AF5亦随phase平移；左/上margin已由53AED/53AF1补偿。 */
    int background=live_owner ? fd2_field_background_draw_live(vga,page,
        72+offset_x,72+offset_y,
        background_map_x,map_y,parallax_x,parallax_y,1) :
        fd2_field_background_draw(page, 72 + offset_x,
            72 + offset_y, background_map_x,
            map_y, parallax_x, parallax_y, game->visual_now_ns, 0);
    if(background!=0)return -1;
    /* 1210f..1214b在实际背景更新之后，12164才选择range LUT。 */
    if(live_owner) {
        if(fd2_field_visual_range_step_live(vga,live_owner->visual_owner,-1)<0)return -1;
        live_owner->range_visual_phase=(uint8_t)live_owner->visual_owner->range_phase;
    }
    if (draw_selection && game->move_range.nodes &&
        (game->interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED ||
         game->interaction == FD2_FIELD_INTERACTION_TARGETING ||
         game->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING ||
         game->interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING ||
         game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING)) {
        range = &game->move_range;
        lut = fd2_field_range_lut(&game->visuals, game->range_visual_phase);
        if (!lut) return -1;
        if (game->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING) {
            const uint8_t *magic = fd2_field_magic_record_get(game->magic_target_id);
            if (!magic || !magic[3]) { range = NULL; lut = NULL; }
        }
    }
    /* source11eee按map raw3取LUT，不受后继mode0/interaction限制。
     * 普通查询仍借现nodes投影；mode6轨迹只归同一map raw3。 */
    if(!game->map.cells || game->map.width<=0 || game->map.height<=0 ||
        game->map.width>256 || game->map.height>256)return -1;
    /* cameraY0左margin首格是WH：其raw3为0，map内全ff也必须取LUT。 */
    int marked=game->ordinary_display_mode==6u || (map_x==-1 && map_y==0);
    for(size_t i=0;!marked && i<(size_t)game->map.width*(size_t)game->map.height;i++)
        marked=(game->map.cells[i]>>24)!=0xffu;
    if(marked && !lut) {
        lut=fd2_field_range_lut(&game->visuals,game->range_visual_phase);
        if(!lut)return -1;
    }
    if (fd2_terrain_render_page_block(page, &game->terrain, &game->map,
            dest_x, dest_y, cols, rows, map_x, map_y,
            terrain_phase, idle_phase, range, lut) != 0) return -1;
    if(draw_selection && selection_owner &&
        (selection_owner!=game || selection_cursor_mark(selection_owner)!=0))return -1;
    if (draw_selection && !moving) {
        fd2_vga scratch = {0};
        uint8_t *visible = scratch.framebuffer + 4 * VGA_STRIDE + 4;
        size_t size = sizeof(scratch.framebuffer) - (size_t)(visible - scratch.framebuffer);
        if (fd2_field_page_read(page, 72, 72, 312, 192,
                visible, size, VGA_STRIDE) != 0) return -1;
        render_field_cursor(game, &scratch, game->camera_cell_x * 24 - 4,
            game->camera_cell_y * 24 - 4, draw_selection == 3 ? 2 : draw_selection == 2);
        if (fd2_field_page_write(page, 72, 72, 312, 192,
                visible, size, VGA_STRIDE) != 0) return -1;
    }
    int actors=live_owner ? fd2_field_units_render_page_live(vga,page,&game->sprites,&game->units,
        game->camera_cell_x,game->camera_cell_y,idle_phase,(uint8_t)vga->field_visual.cursor_phase) :
        fd2_field_units_render_page(page, &game->sprites, &game->units,
            game->camera_cell_x, game->camera_cell_y, idle_phase,
            (uint8_t)vga->field_visual.cursor_phase, vga->actor_jitter_phase);
    if (actors != 0 ||
        fd2_field_units_render_page_terrain_overlays(page, &game->terrain, &game->map,
            &game->units, game->camera_cell_x, game->camera_cell_y,
            terrain_phase, range, lut ? lut :
                fd2_field_range_lut(&game->visuals,game->range_visual_phase)) != 0) return -1;
    if (draw_info && !moving && fd2_field_game_render_page_info(game, vga, page,
            &game->units, game->camera_cell_x, game->camera_cell_y,
            game->cursor_cell_x, game->cursor_cell_y, idle_phase) != 0) return -1;
    return project ? fd2_field_page_project(page, 72 + offset_x, 72 + offset_y,
        vga->framebuffer, sizeof(vga->framebuffer)) : 0;
}

/* mode0纯背景；1正常owner UI；2物理返回页；3法术演出背景（原51a83=0）。 */
static void render_field(const fd2_field_game *game, fd2_vga *vga,
                         int mode, fd2_field_game *ui_owner, uint8_t idle_phase, uint8_t terrain_phase) {
    int draw_ui = mode != 0;
    if (!game || !game->ready || !vga) return;
    int camera_x = camera_origin_x(game);
    int camera_y = camera_origin_y(game);
    if (vga->field_page) {
        int selection = mode == 1 ? 1 : mode == 2 ? 2 : 0;
        if (render_work_page(game, vga, selection, draw_ui, 1,
                idle_phase, terrain_phase, NULL, ui_owner) != 0) {
            if (ui_owner) fail_action(ui_owner, 0);
            return;
        }
        if (ui_owner) ui_owner->info_panel_right = vga->info_panel_right;
        goto overlays;
    }

    /* field_actor_move_{down,left,up,right}_follow_camera
     * @0x12eaa/0x1300d/0x13185/0x13315 在六个步态相位中每次同步
     * 卷动镜头 4 px；camera_pixel_offset_* 保存尚未提交到格坐标的部分。 */
    fd2_terrain_render_field_base_animated(
        vga, &game->terrain, &game->map, camera_x, camera_y,
        terrain_phase, idle_phase);
    if (mode == 1 || mode == 2) {
        if (mode == 1) render_move_selection(game, vga, camera_x, camera_y);
        render_field_cursor(game, vga, camera_x, camera_y, mode == 2);
    }
    fd2_field_units_render(vga, &game->sprites, &game->units,
                           camera_x, camera_y, idle_phase);
    render_actor_terrain_overlays(game, vga, camera_x, camera_y, mode == 1, terrain_phase);
    clear_view_border(vga);
    if (!draw_ui) return;
    if (game->option_values[3] && !vga->info_panel_suppressed) {
        int panel_right = field_info_panel_side(game);
        if (ui_owner) ui_owner->info_panel_right = (uint8_t)panel_right;
        render_field_info(game, vga, panel_right, idle_phase);
    }
overlays:
    if (!draw_ui || mode >= 2) return;
    int page_menu=vga->field_page &&
        (game->interaction==FD2_FIELD_INTERACTION_COMMAND ||
         game->interaction==FD2_FIELD_INTERACTION_ITEM_MENU ||
         (game->interaction==FD2_FIELD_INTERACTION_SYSTEM_MENU &&
          game->system_menu.page!=FD2_FIELD_SYSTEM_PAGE_CONFIRMATION));
    if(page_menu) {
        if(render_menu_page_overlay(game,vga,0)!=0) {
            if(ui_owner)fail_action(ui_owner,0);
            return;
        }
    } else {
        render_field_command(game, vga, camera_x, camera_y);
        render_field_system_menu(game, vga, camera_x, camera_y);
    }
    render_manual_slot_picker(game, vga);
    render_system_result(game, vga);
    render_manual_slot_result(game, vga);
    if (game->detail_visible && game->detail_unit >= 0 &&
        (size_t)game->detail_unit < game->units.count) {
        if (fd2_field_detail_draw(vga, &game->info_assets, &game->font,
                              &game->ui_text,
                              &game->units.items[game->detail_unit],
                              &game->detail_portrait) != 0) {
            if (ui_owner) fail_action(ui_owner, 0);
            return;
        }
        if (game->detail_page == FD2_FIELD_DETAIL_MAGIC)
            (void)fd2_field_magic_list_draw(vga, &game->info_assets, &game->font,
                &game->ui_text, &game->units.items[game->detail_unit], -1);
    }
}

void fd2_field_game_render(fd2_field_game *game, fd2_vga *vga) {
    if (game) render_field(game, vga, 1, game, game->idle_phase, game->terrain_visual_phase);
}

void fd2_field_game_render_preview(const fd2_field_game *game, fd2_vga *vga) {
    if (game) render_field(game, vga, 1, NULL, game->idle_phase, game->terrain_visual_phase);
}

void fd2_field_game_render_background(const fd2_field_game *game, fd2_vga *vga) {
    if (game) render_field(game, vga, 0, NULL, game->idle_phase, game->terrain_visual_phase);
}

static int runtime_actor_phase(fd2_field_game *game,fd2_vga *vga) {
    if(!game || !game->ready || game->failure!=FD2_FIELD_FAILURE_NONE ||
        !vga || game->visual_owner!=&vga->field_visual ||
        fd2_field_visual_actor_step_live(vga,game->visual_owner)<0)return -1;
    game->idle_phase=(uint8_t)game->visual_owner->actor_idle_phase;
    return 0;
}

int fd2_field_game_runtime_refresh(fd2_field_game *game, fd2_vga *vga,
    int phase_only) {
    if (!game || !game->ready || !vga || !vga->field_page ||
        phase_only < 0 || phase_only > 3) return -1;
    if (phase_only == 1 || phase_only == 2) {
        if (phase_only==2 && runtime_actor_phase(game,vga)!=0)return -1;
        return render_work_page(game, vga, 0, 0, 0,
            game->idle_phase, game->terrain_visual_phase,game,game);
    }
    /* field_view_refresh @0x11cac（原sub_11CAC）：1297d先推进actor/cursor，
     * 原实参0才调用4e31c，然后11eee推进terrain/range，127e0推进raw26。
     * 分开有限叶以保水色失败前缀，不能先组合推进actor+terrain。 */
    if (runtime_actor_phase(game,vga) != 0 ||
        (phase_only == 0 && fd2_field_water_tick_live(vga) < 0)) return -1;
    /* 原11cac直接消费51A83；AI也能给7/9/11，不能从玩家interaction反推。 */
    int selection = game->ordinary_display_mode ? 3:0;
    int result = render_work_page(game, vga, selection, 1, 1,
        game->idle_phase, game->terrain_visual_phase,game,game);
    if(result!=0)return result;
    game->info_panel_right = vga->info_panel_right;
    return result;
}

/* 17898/179d5的同一page叶；preview不推进clock，正式wait仍逐direct读钟。
 * 图标和focus先写同一page再投影，不能借清零border掩盖VGA直绘越出视口。 */
static int render_menu_page_overlay(const fd2_field_game *game,fd2_vga *vga,int live) {
    if(!game || !vga || !vga->field_page || (live!=0 && live!=1))return -1;
    int x=48+(game->cursor_cell_x-game->camera_cell_x)*24;
    int y=48+(game->cursor_cell_y-game->camera_cell_y)*24;
    fd2_vga icons={0};
    if(fd2_field_page_read(vga->field_page,x,y,72,72,icons.framebuffer,
            sizeof(icons.framebuffer),VGA_STRIDE)!=0)return -1;
    render_field_command_icons(game,&icons,game->cursor_cell_x*24-24,
        game->cursor_cell_y*24-24);
    render_field_system_menu(game,&icons,game->cursor_cell_x*24-24,
        game->cursor_cell_y*24-24);
    if(fd2_field_page_write(vga->field_page,x,y,72,72,icons.framebuffer,
            sizeof(icons.framebuffer),VGA_STRIDE)!=0)return -1;
    int focus=fd2_field_game_unit_at(game,game->cursor_cell_x,game->cursor_cell_y);
    if(focus>=0) {
        int actor=live?fd2_field_units_render_page_actor_live(vga,vga->field_page,
            &game->sprites,&game->units,(size_t)focus,game->camera_cell_x,game->camera_cell_y,
            game->idle_phase,(uint8_t)vga->field_visual.cursor_phase,0):
            fd2_field_units_render_page_actor(vga->field_page,&game->sprites,&game->units,
                (size_t)focus,game->camera_cell_x,game->camera_cell_y,game->idle_phase,
                (uint8_t)vga->field_visual.cursor_phase,vga->actor_jitter_phase,0);
        if(actor!=0)return -1;
    }
    if(fd2_field_page_project(vga->field_page,72,72,vga->framebuffer,sizeof(vga->framebuffer))!=0)return -1;
    return 0;
}

/* field_command_menu_wait_key @0x17898：caller已water/highlight；此处
 * 1297d→11eee→127a9→1acf3→179d5，无122dc、水色或RNG附加调用。 */
int fd2_field_game_runtime_menu_refresh(fd2_field_game *game,fd2_vga *vga) {
    if(!game || !vga || !vga->field_page ||
        (game->interaction!=FD2_FIELD_INTERACTION_COMMAND &&
         game->interaction!=FD2_FIELD_INTERACTION_ITEM_MENU &&
         (game->interaction!=FD2_FIELD_INTERACTION_SYSTEM_MENU ||
          game->system_menu.page==FD2_FIELD_SYSTEM_PAGE_CONFIRMATION)))return -1;
    if(runtime_actor_phase(game,vga)!=0 ||
        render_work_page(game,vga,0,1,0,game->idle_phase,game->terrain_visual_phase,game,game)!=0)
        return -1;
    if(render_menu_page_overlay(game,vga,1)!=0)return -1;
    game->info_panel_right=vga->info_panel_right;
    return 0;
}

/* field_unit_selection_summary_wait @0x18b84（原sub_18B84）的已证body。
 * 18b96在入口锁定板侧，18c14写同一工作页，18c2c的信息板在它之后；
 * 不能借11cac先画info/project再只把summary叠在VGA。门/两次water由Play持有。 */
int fd2_field_game_runtime_summary_refresh(fd2_field_game *game,fd2_vga *vga,
        size_t actor,int summary_left) {
    if(!game || !game->ready || game->failure!=FD2_FIELD_FAILURE_NONE ||
        !vga || !vga->field_page || game->units.count>FD2_FIELD_MAX_UNITS ||
        actor>=game->units.count || (summary_left!=0 && summary_left!=1))return -1;
    if(runtime_actor_phase(game,vga)!=0 ||
        render_work_page(game,vga,game->ordinary_display_mode?3:0,0,0,
            game->idle_phase,game->terrain_visual_phase,game,game)!=0)return -1;
    int x=summary_left?9:160;
    if(fd2_field_unit_summary_draw_page(vga->field_page,&game->info_assets,&game->font,&game->ui_text,
            &game->units.items[actor],x+68,77)!=0 ||
        fd2_field_game_render_page_info(game,vga,vga->field_page,&game->units,
            game->camera_cell_x,game->camera_cell_y,game->cursor_cell_x,game->cursor_cell_y,
            game->idle_phase)!=0 ||
        fd2_field_page_project(vga->field_page,72,72,vga->framebuffer,sizeof(vga->framebuffer))!=0)
        return -1;
    game->info_panel_right=vga->info_panel_right;
    return 0;
}

/* sub_1B1E7 @0x1b1e7（原sub_1B1E7）：入口仅11eee/127a9；稳态在
 * 两者前1297d→4e31c。均无选择层、info或宿主deadline推进。 */
int fd2_field_game_runtime_overview_refresh(fd2_field_game *game,fd2_vga *vga,int steady) {
    if(!game || !vga || !vga->field_page || (steady!=0 && steady!=1))return -1;
    if(steady && (runtime_actor_phase(game,vga)!=0 || fd2_field_water_tick_live(vga)<0))return -1;
    if(render_work_page(game,vga,0,0,1,game->idle_phase,game->terrain_visual_phase,game,game)!=0)return -1;
    return 0;
}

static int menu_transition_terminal(const fd2_field_game *game,fd2_vga *vga) {
    if((fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0) || game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
    return game->runtime_host_quit || fd2_input_take_quit(&vga->input)?1:0;
}

/* field_command_menu_open @0x1741c / close @0x176b4（原同名）。
 * 1749d..174e7仅入口1297d/11eee/127a9/info，再调用
 * field_command_menu_background_save @0x175a9（原sub_175A9）保存5184B；
 * 四相位先field_command_menu_background_restore @0x17643（原sub_17643），
 * 再图标/direct127e0/project。关闭没有入口刷新，
 * 177ed末次restore不再project；177f2的FIFO clear归Play成功返回边界。
 * source01完整指令排除了旧每相位全图重绘/16ms门，不推测伪C多参数ABI。 */
static int animate_menu_page(fd2_field_game *game,fd2_vga *vga,int opening,int system) {
    if(!game || !game->ready || !vga || !vga->field_page ||
        game->visual_owner!=&vga->field_visual || game->units.count>FD2_FIELD_MAX_UNITS ||
        (opening!=0 && opening!=1) || !game->command_assets.ready)return -1;
    const uint8_t *disabled=system?game->system_menu.disabled:game->command_disabled;
    static const uint8_t command_ids[2][4]={{0,1,2,3},{8,9,10,11}};
    const uint8_t *ids=system?game->system_menu.command_ids:
        command_ids[game->interaction==FD2_FIELD_INTERACTION_ITEM_MENU];
    if((system && (game->interaction!=FD2_FIELD_INTERACTION_SYSTEM_MENU ||
            game->system_menu.page==FD2_FIELD_SYSTEM_PAGE_CONFIRMATION)) ||
        (!system && game->interaction!=FD2_FIELD_INTERACTION_COMMAND &&
            game->interaction!=FD2_FIELD_INTERACTION_ITEM_MENU))return -1;
    for(size_t i=0;i<4;i++) {
        size_t frame=(size_t)ids[i]*3u+(size_t)disabled[i]*2u;
        if(disabled[i]>1u || frame>=game->command_assets.frame_count ||
            frame>=FD2_FIELD_COMMAND_FRAME_COUNT || !game->command_assets.frames[frame].pixels ||
            game->command_assets.frames[frame].width!=24 ||
            game->command_assets.frames[frame].height!=20)return -1;
    }
    int64_t rel_x=(int64_t)game->cursor_cell_x-game->camera_cell_x;
    int64_t rel_y=(int64_t)game->cursor_cell_y-game->camera_cell_y;
    if(rel_x<0 || rel_x>=13 || rel_y<0 || rel_y>=8 ||
        (!opening && game->command_backdrop_owner!=vga->field_page))return -1;
    int status=menu_transition_terminal(game,vga);if(status)return status;
    int focus=fd2_field_game_unit_at(game,game->cursor_cell_x,game->cursor_cell_y);
    int x=48+(int)rel_x*24,y=48+(int)rel_y*24;
    uint8_t *phase=system?&game->system_menu.animation_phase:&game->command_animation_phase;
    uint8_t *direction=system?&game->system_menu.animation_opening:&game->command_animation_opening;
    if(opening) {
        if(runtime_actor_phase(game,vga)!=0 ||
            render_work_page(game,vga,0,1,0,game->idle_phase,game->terrain_visual_phase,game,game)!=0)return -1;
        status=menu_transition_terminal(game,vga);if(status)return status;
        game->info_panel_right=vga->info_panel_right;
        if(fd2_field_page_read(vga->field_page,x,y,72,72,game->command_backdrop,
                sizeof(game->command_backdrop),72)!=0)return -1;
        game->command_backdrop_owner=vga->field_page;
    }
    *direction=(uint8_t)opening;
    for(uint8_t step=1;step<=4;step++) {
        status=menu_transition_terminal(game,vga);if(status)return status;
        *phase=step;
        if(fd2_field_page_write(vga->field_page,x,y,72,72,game->command_backdrop,
                sizeof(game->command_backdrop),72)!=0)return -1;
        fd2_vga scratch={0};
        if(fd2_field_page_read(vga->field_page,x,y,72,72,scratch.framebuffer,
                sizeof(scratch.framebuffer),VGA_STRIDE)!=0)return -1;
        fd2_field_command_draw_id_animation(&scratch,&game->command_assets,24,24,
            ids,-1,disabled,0,opening,step);
        if(fd2_field_page_write(vga->field_page,x,y,72,72,scratch.framebuffer,
                sizeof(scratch.framebuffer),VGA_STRIDE)!=0 ||
            (focus>=0 && fd2_field_units_render_page_actor_live(vga,vga->field_page,&game->sprites,
                &game->units,(size_t)focus,game->camera_cell_x,game->camera_cell_y,game->idle_phase,
                (uint8_t)vga->field_visual.cursor_phase,0)!=0) ||
            fd2_field_page_project(vga->field_page,72,72,vga->framebuffer,sizeof(vga->framebuffer))!=0)
            return -1;
        game_present_copy(vga);
        status=menu_transition_terminal(game,vga);if(status)return status;
    }
    if(!opening) {
        if(fd2_field_page_write(vga->field_page,x,y,72,72,game->command_backdrop,
                sizeof(game->command_backdrop),72)!=0)return -1;
        game->command_backdrop_owner=NULL;
    }
    *direction=*phase=0;
    return 0;
}

/* sub_2E2B0 @0x2e2b0 正常尾在fade-in前调用sub_11CAC @0x11cac重画当前地图；
 * 不复用旧TARGETING缓存，也不让同步present写panel owner或再次提交动作。 */
void fd2_field_game_render_battle_return(const fd2_field_game *game, fd2_vga *vga) {
    if (!game || !game->ready || !vga) return;
    memset(vga->framebuffer, 0, sizeof(vga->framebuffer));
    render_field(game, vga, 2, NULL, game->idle_phase, game->terrain_visual_phase);
}

/* field_transition_lut_mask @0x22046（原登记FUN_0004725a）：22077在两次
 * LUT scan之间调用127a9重画actor/前景；不能先画完整背景再叠三层mask。 */
static int effect_terrain_draw(const fd2_field_game *game,fd2_vga *vga,
    fd2_field_page *target,fd2_field_game *live_owner) {
    if(!game || !game->ready || !vga || !vga->field_page || !target)return -1;
    if(live_owner) {
        if(live_owner!=game || game->failure!=FD2_FIELD_FAILURE_NONE ||
            game->visual_owner!=&vga->field_visual ||
            fd2_field_visual_terrain_step_live(vga,live_owner->visual_owner)<0)return -1;
        live_owner->terrain_visual_phase=(uint8_t)live_owner->visual_owner->terrain_phase;
    }
    if(fd2_field_background_draw_from_live(vga,vga->field_page,target,72,72,
        game->camera_cell_x,game->camera_cell_y,0,0,live_owner!=NULL)!=0)return -1;
    if(live_owner) {
        if(fd2_field_visual_range_step_live(vga,live_owner->visual_owner,-1)<0)return -1;
        live_owner->range_visual_phase=(uint8_t)live_owner->visual_owner->range_phase;
    }
    return fd2_terrain_render_page_block(target,&game->terrain,&game->map,72,72,13,8,
        game->camera_cell_x,game->camera_cell_y,game->terrain_visual_phase,game->idle_phase,NULL,
        fd2_field_range_lut(&game->visuals,game->range_visual_phase));
}

int fd2_field_game_effect_terrain_page(const fd2_field_game *game,fd2_vga *vga,
    fd2_field_page *target) {
    return effect_terrain_draw(game,vga,target,NULL);
}

int fd2_field_game_effect_terrain_begin(fd2_field_game *game,fd2_vga *vga,
    fd2_field_page *target) {
    return effect_terrain_draw(game,vga,target,game);
}

int fd2_field_game_effect_mask_page(const fd2_field_game *game,fd2_vga *vga,
    const fd2_field_page *base,const uint8_t lut[256],int x,int y,
    int radius,int first_row,int circle12) {
    if(!game || !game->ready || !vga || !vga->field_page || !base || !lut ||
        radius<1 || radius>4096 || first_row<0 || first_row>192 ||
        (circle12!=0 && circle12!=1) || (circle12 && first_row))return -1;
    fd2_field_page *page=vga->field_page;
    if(fd2_field_page_copy_pixels(page,base)!=0)return -1;
    int result=circle12?fd2_field_transition_page_circle12(page,lut,x,y,radius):
        fd2_field_transition_page_before_actors_from(page,lut,x,y,radius,first_row);
    if(result ||
        fd2_field_units_render_page_live(vga,page,&game->sprites,&game->units,
            game->camera_cell_x,game->camera_cell_y,game->idle_phase,
            (uint8_t)vga->field_visual.cursor_phase)!=0 ||
        fd2_field_units_render_page_terrain_overlays(page,&game->terrain,&game->map,
            &game->units,game->camera_cell_x,game->camera_cell_y,
            game->terrain_visual_phase,NULL,
            fd2_field_range_lut(&game->visuals,vga->field_visual.range_phase))!=0)return -1;
    if(!circle12 && fd2_field_transition_page_after_actors_from(page,lut,x,y,radius,first_row)!=0)
        return -1;
    return 0;
}

int fd2_field_game_effect_mask_frame(const fd2_field_game *game,fd2_vga *vga,
    const fd2_field_page *base,const uint8_t lut[256],int x,int y,
    int radius,int first_row,int circle12) {
    if (fd2_field_game_effect_mask_page(game,vga,base,lut,x,y,radius,first_row,circle12)) return -1;
    return fd2_field_page_project(vga->field_page,72,72,vga->framebuffer,sizeof(vga->framebuffer));
}

int fd2_field_game_effect_refresh(fd2_field_game *game,fd2_vga *vga) {
    /* 21548/21eb1尾的11cac(0)复用正式owner路径；24618没有此尾调用。 */
    return fd2_field_game_runtime_refresh(game,vga,0);
}

static int render_transition_mask(const fd2_field_game *game, fd2_vga *vga,
    const uint8_t lut[256], int center_x, int center_y, int radius,
    uint8_t idle_phase, uint8_t terrain_phase) {
    if (!game || !game->ready || !vga || !lut || radius < 1 || radius > 4096)
        return -1;
    if(vga->field_page) {
        fd2_field_page *base=fd2_field_page_create();if(!base)return -1;
        int result=fd2_field_game_effect_terrain_page(game,vga,base);
        if(!result)result=fd2_field_game_effect_mask_frame(game,vga,base,lut,
            center_x,center_y,radius,0,0);
        fd2_field_page_destroy(base);return result;
    }
    int camera_x = camera_origin_x(game);
    int camera_y = camera_origin_y(game);
    fd2_terrain_render_field_base_animated(vga, &game->terrain, &game->map,
        camera_x, camera_y, terrain_phase, idle_phase);
    (void)fd2_field_transition_lut_before_actors(vga, lut, center_x, center_y, radius);
    fd2_field_units_render(vga, &game->sprites, &game->units,
        camera_x, camera_y, idle_phase);
    render_actor_terrain_overlays(game, vga, camera_x, camera_y, 0, terrain_phase);
    (void)fd2_field_transition_lut_after_actors(vga, lut, center_x, center_y, radius);
    clear_view_border(vga);
    return 0;
}

/* field_player_magic_command_execute @0x1cff0及AI15311保持51a83=0。
 * 由唯一Game视觉owner先在原11cac点推进；这里仅画当前相位，不另推时钟。 */
void fd2_field_game_render_effect_background(const fd2_field_game *game, fd2_vga *vga) {
    if (!game || !game->ready || !vga) return;
    if(!vga->field_page) memset(vga->framebuffer, 0, sizeof(vga->framebuffer));
    render_field(game, vga, 3, NULL, game->idle_phase, game->terrain_visual_phase);
}

void fd2_field_game_render_effect_tiles(const fd2_field_game *game, fd2_vga *vga) {
    if (!game || !game->ready || !vga) return;
    if(!vga->field_page) memset(vga->framebuffer, 0, sizeof(vga->framebuffer));
    if (vga->field_page) {
        if (fd2_field_background_draw(vga->field_page,72,72,game->camera_cell_x,
                game->camera_cell_y,0,0,game->visual_now_ns,0)!=0 ||
            fd2_terrain_render_page_block(vga->field_page,&game->terrain,&game->map,
                72,72,13,8,game->camera_cell_x,game->camera_cell_y,
                game->terrain_visual_phase,game->idle_phase,NULL,NULL)!=0) return;
        (void)fd2_field_page_project(vga->field_page,72,72,vga->framebuffer,sizeof(vga->framebuffer));
        return;
    }
    fd2_terrain_render_field_base_animated(vga, &game->terrain, &game->map,
        camera_origin_x(game), camera_origin_y(game),
        game->terrain_visual_phase, game->idle_phase);
    clear_view_border(vga);
}

void fd2_field_game_render_effect_actors(const fd2_field_game *game, fd2_vga *vga) {
    if (!game || !game->ready || !vga) return;
    if(vga->field_page) {(void)fd2_field_game_effect_actors(game,vga);return;}
    int camera_x = camera_origin_x(game);
    int camera_y = camera_origin_y(game);
    fd2_field_units_render(vga, &game->sprites, &game->units, camera_x, camera_y, game->idle_phase);
    render_actor_terrain_overlays(game, vga, camera_x, camera_y, 0, game->terrain_visual_phase);
    clear_view_border(vga);
}

int fd2_field_game_effect_actors(const fd2_field_game *game,fd2_vga *vga) {
    if(!game || !game->ready || !vga || !vga->field_page)return -1;
    if(fd2_field_units_render_page_live(vga,vga->field_page,&game->sprites,&game->units,
        game->camera_cell_x,game->camera_cell_y,game->idle_phase,
        (uint8_t)vga->field_visual.cursor_phase)!=0 ||
        fd2_field_units_render_page_terrain_overlays(vga->field_page,&game->terrain,
            &game->map,&game->units,game->camera_cell_x,game->camera_cell_y,
            game->terrain_visual_phase,NULL,
            fd2_field_range_lut(&game->visuals,vga->field_visual.range_phase))!=0)return -1;
    return fd2_field_page_project(vga->field_page,72,72,vga->framebuffer,sizeof(vga->framebuffer));
}

int fd2_field_game_render_transition_mask(const fd2_field_game *game, fd2_vga *vga,
    const uint8_t lut[256], int center_x, int center_y, int radius) {
    if (!game) return -1;
    return render_transition_mask(game, vga, lut, center_x, center_y, radius,
        game->idle_phase, game->terrain_visual_phase);
}

static void render_field_info_at(const fd2_field_game *game, fd2_vga *vga,
    const fd2_field_units *units, int focus_x, int focus_y,
    int panel_right, uint8_t render_idle_phase) {
    if (!game || !vga || !units) return;
    uint32_t cell = fd2_field_map_cell(&game->map,
                                        focus_x, focus_y);
    uint16_t terrain_id = fd2_field_cell_terrain(cell);
    const fd2_terrain_attr *attr = fd2_terrain_attr_get(
        &game->terrain, terrain_id);
    size_t frame_index;
    const fd2_image *terrain_image = NULL;
    /* field_cell_info_panel_draw @0x1acf3 按 map_cell_info_at 返回的
     * 原始 terrain ID 取基础帧，不套用地图层动画相位。 */
    if (fd2_terrain_base_frame_from_cell(
            &game->terrain, cell, &frame_index) == 0)
        terrain_image = fd2_terrain_frame(&game->terrain, frame_index);

    /* field_cell_info_panel_draw 与物理攻击共读 DS:0x1a12/0x1a2a。
     * class 0..5 完整表已由 DOSBox debugger runtime capture 确认。 */
    int32_t attack = 0;
    int32_t defense = 0;
    if (attr)
        (void)fd2_field_attack_terrain_modifiers(
            attr->movement_cost_class, &attack, &defense);

    const fd2_image *unit_image = NULL;
    fd2_image unit_frame = {0};
    uint16_t hp = 0;
    uint16_t hp_max = 0;
    int unit_index = -1;
    for (size_t i = 0; i < units->count; i++) {
        const fd2_field_unit *unit = &units->items[i];
        if (!fd2_field_unit_is_hidden(unit) && unit->x == focus_x && unit->y == focus_y) {
            unit_index = (int)i; break;
        }
    }
    if (unit_index >= 0) {
        const fd2_field_unit *unit = &units->items[unit_index];
        /* 原版面板固定使用cache映射的sprite身份，不随热unit_id变化、
         * 地图朝向或移动中的walk frame改变。 */
        uint8_t idle_phase = render_idle_phase == 3 ? 1 : render_idle_phase;
        size_t sprite_id = fd2_field_unit_cached_sprite_id(units, (size_t)unit_index);
        size_t sprite_index = sprite_id == SIZE_MAX ? SIZE_MAX : sprite_id * 12u + idle_phase;
        if (unit->unit_id != 121 && !(unit->race == 10 && unit->side == 1) &&
            sprite_index < game->sprites.frame_count &&
            game->sprites.decoded_frames) {
            unit_frame.width = 24;
            unit_frame.height = 24;
            unit_frame.pixels = game->sprites.decoded_frames +
                                sprite_index * 24u * 24u;
            unit_image = &unit_frame;
            hp = unit->hp;
            hp_max = unit->hp_max;
        }
    }
    fd2_field_info_draw(vga, &game->info_assets,
                        panel_right, terrain_image, unit_image,
                        hp, hp_max, attack, defense);
}

int fd2_field_game_render_page_info(const fd2_field_game *game, fd2_vga *vga,
    fd2_field_page *page, const fd2_field_units *units,
    int camera_x, int camera_y, int focus_x, int focus_y, uint8_t idle_phase) {
    if (!game || !vga || !page || !units || units->count > FD2_FIELD_MAX_UNITS ||
        idle_phase > 3u) return -1;
    if (!game->option_values[3] || vga->info_panel_suppressed) return 0;
    int side = vga->info_panel_right;
    int rel_x = focus_x - camera_x, rel_y = focus_y - camera_y;
    if (rel_y > 5 && rel_x < 3) side = 1;
    else if (rel_y > 5 && rel_x > 9) side = 0;
    /* 狭矩形桥只借page自身的透明底；不得借当前VGA/对白像素回灌。
     * a1=53A49+32904，v2=a1+157*456+(1/242)，对应VGA(5/246,161)。 */
    fd2_vga scratch = {0};
    int x = side ? 246 : 5;
    uint8_t *rect = scratch.framebuffer + 161u * VGA_STRIDE + (size_t)x;
    size_t available = sizeof(scratch.framebuffer) - (size_t)(rect - scratch.framebuffer);
    if (fd2_field_page_read(page, x + 68, 229, 69, 34,
            rect, available, VGA_STRIDE) != 0) return -1;
    render_field_info_at(game, &scratch, units, focus_x, focus_y, side, idle_phase);
    if (fd2_field_page_write(page, x + 68, 229, 69, 34,
            rect, available, VGA_STRIDE) != 0) return -1;
    vga->info_panel_right = (uint8_t)side;
    return 0;
}

static void render_field_command_icons(const fd2_field_game *game,
                                 fd2_vga *vga,
                                 int camera_x, int camera_y) {
    if (!game || !vga ||
        (game->interaction != FD2_FIELD_INTERACTION_COMMAND &&
         game->interaction != FD2_FIELD_INTERACTION_ITEM_MENU) ||
        game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count)
        return;
    const fd2_field_unit *unit = &game->units.items[game->selected_unit];
    /* field_command_menu_draw @0x179d5：以单位格左上角为基准，
     * 静止时用 close 初值；开合动画期间保留各自四帧坐标。 */
    /* 18d8c用0..3，1bbdc物品子菜单用8..11；禁用仍取各自能力门。 */
    static const uint8_t command_ids[2][FD2_FIELD_COMMAND_COUNT] = {
        {0, 1, 2, 3}, {8, 9, 10, 11}
    };
    fd2_field_command_draw_id_animation(
        vga, &game->command_assets,
        (int)unit->x * 24 - camera_x,
        (int)unit->y * 24 - camera_y,
        command_ids[game->interaction == FD2_FIELD_INTERACTION_ITEM_MENU],
        game->command_selected, game->command_disabled,
        game->command_highlight_phase, game->command_animation_opening,
        game->command_animation_phase);
}

static void render_field_command(const fd2_field_game *game,fd2_vga *vga,
    int camera_x,int camera_y) {
    if(!game || !vga || (game->interaction!=FD2_FIELD_INTERACTION_COMMAND &&
        game->interaction!=FD2_FIELD_INTERACTION_ITEM_MENU) || game->selected_unit<0 ||
        (size_t)game->selected_unit>=game->units.count)return;
    render_field_command_icons(game,vga,camera_x,camera_y);
    /* field_command_menu_draw @0x179d5 在四个图标后再次调用
     * sub_127E0 @0x127e0（原登记同名）重画当前actor，不重铺遮挡tile。
     * 角色上移6px，覆盖上图标末行；最后按原312×192视区裁去边框。 */
    fd2_field_units_render_actor(vga, &game->sprites, &game->units,
        (size_t)game->selected_unit, camera_x, camera_y, game->idle_phase, 0);
    clear_view_border(vga);
}

static void render_manual_slot_picker(const fd2_field_game *game,
                                      fd2_vga *vga) {
    if (!game || !vga || game->interaction !=
            FD2_FIELD_INTERACTION_MANUAL_SLOT)
        return;
    /* field_manual_slot_draw @code0 0x19ab2：四个 slot 是纵向列表，
     * y=0x77+index*0x13；先绘制 FDTXT 0x225 的「第」，再按空槽
     * 0x202 或有效槽 0x202+stage 绘制状态。没有证据的装饰帧不猜测。 */
    for (int row = 112; row < 198; row++)
        memset(vga->framebuffer + row * VGA_STRIDE + 5, 0xcd, 310u);
    for (size_t i = 0; i < FD2_SAVE_SLOT_COUNT; i++) {
        const int y = 0x77 + (int)i * 0x13;
        const int selected = i == game->manual_slot_picker.selected;
        const uint8_t color = selected ? 0xc9u : 0xcdu;
        const uint8_t *tokens;
        size_t token_count;
        if (fd2_text_entry_get_fragment(&game->ui_text, 0x225,
                                        &tokens, &token_count) == 0) {
            int x = 10;
            for (size_t token_i = 0; token_i < token_count; token_i++) {
                int16_t token = (int16_t)fd2_text_token_at(tokens, token_i);
                if (token == -1) break;
                if (token < 0) continue;
                fd2_font_draw_glyph(vga, &game->font, (uint16_t)token,
                                    x, y, color, 0x4c, -1);
                x += 16;
            }
        }
        /* 原版把 DS:0x3ae1 设为 index+1，再由格式 token 插值显示。
         * SDL 当前文本渲染不处理该控制 token，因此直接画全角数字。 */
        fd2_font_draw_glyph(vga, &game->font, (uint16_t)(i + 1u),
                            26, y, color, 0x4c, -1);
        size_t fragment = 0x202u;
        if (game->manual_slot_picker.occupied[i]) {
            uint8_t stage = game->manual_slot_picker.stage_ids[i];
            fragment += stage;
            if (fragment >= game->ui_text.fragment_count)
                fragment = 0x202u;
        }
        if (fd2_text_entry_get_fragment(&game->ui_text, fragment,
                                        &tokens, &token_count) == 0) {
            int x = 88;
            for (size_t token_i = 0; token_i < token_count; token_i++) {
                int16_t token = (int16_t)fd2_text_token_at(tokens, token_i);
                if (token == -1) break;
                if (token < 0) continue;
                fd2_font_draw_glyph(vga, &game->font, (uint16_t)token,
                                    x, y, color, 0x4c, -1);
                x += 16;
            }
        }
    }
}

static void render_manual_slot_result(const fd2_field_game *game,
                                      fd2_vga *vga) {
    if (!game || !vga || game->manual_slot_result_fragment == 0 ||
        !game->ui_text.data)
        return;
    const fd2_image *dialog = &game->info_assets.detail_bottom;
    if (dialog->pixels)
        fd2_map_sprite_blit(vga, dialog, 5, 112, 0);
    const uint8_t *tokens;
    size_t token_count;
    if (fd2_text_entry_get_fragment(&game->ui_text,
                                    game->manual_slot_result_fragment,
                                    &tokens, &token_count) != 0)
        return;
    int x = 15;
    int y = 119;
    for (size_t i = 0; i < token_count; i++) {
        int16_t token = (int16_t)fd2_text_token_at(tokens, i);
        if (token == -1) break;
        if (token == -2) {
            x = 15;
            y += 16;
        } else if (token >= 0 && x + 16 <= 315 && y + 16 <= 198) {
            fd2_font_draw_glyph(vga, &game->font, (uint16_t)token,
                                x, y, 0xcd, 0x4c, -1);
            x += 16;
        }
    }
}

static int system_notice_text(const fd2_field_game *game, size_t fragment,
        const uint8_t **tokens, size_t *count) {
    if (!game || !game->ready || !game->font.glyphs || !game->font.glyph_count ||
        game->font.glyph_count > game->font.size / 32u ||
        fd2_text_entry_get_fragment(&game->ui_text, fragment, tokens, count) != 0)
        return -1;
    for (size_t i = 0; i < *count; i++) {
        uint16_t token = fd2_text_token_at(*tokens, i);
        if (token == 0xffffu) { *count = i; return 0; }
        if (token != 0xfffeu && (token >= 0xff00u || token >= game->font.glyph_count)) return -1;
    }
    return -1;
}

static void system_notice_text_draw(const fd2_field_game *game, fd2_vga *vga,
        const uint8_t *tokens, size_t count, int y) {
    int x = 99;
    for (size_t i = 0; i < count; i++) {
        uint16_t token = fd2_text_token_at(tokens, i);
        if (token == 0xfffeu) { x = 99; y += 19; continue; }
        fd2_font_draw_glyph(vga, &game->font, token, x, y, 0xcd, 0x4c, 0x4a);
        x += 16;
    }
}

/* field_empty_focus_menu_execute @0x16f55（原FUN_0001469d）、
 * field_secondary_menu_execute @0x19df7（原FUN_0001753f）：
 * notice75，只有March借raw0的unit_id；1956b→15f84→19953，无独立UI真值。 */
int fd2_field_game_render_system_notice(const fd2_field_game *game,
        fd2_vga *vga, size_t fragment) {
    if (!game || !vga) return -1;
    size_t prompt = fragment;
    int result_line = 0;
    switch (fragment) {
        case 410: case 413: case 415: case 417: case 419: break;
        case 411: case 414: case 416: case 418: case 420:
            prompt = fragment - 1u; result_line = 1; break;
        case 412:
            prompt = game->system_menu.confirmation_prompt_fragment;
            if (!(prompt == 410 || prompt == 413 || prompt == 415 || prompt == 417 || prompt == 419)) return -1;
            result_line = 1; break;
        default: return -1;
    }
    uint16_t portrait = 75;
    if (prompt == 417) {
        if (!game->units.count) return -1;
        portrait = game->units.items[0].unit_id;
    }
    const uint8_t *prompt_tokens, *result_tokens = NULL;
    size_t prompt_count, result_count = 0;
    if (system_notice_text(game, prompt, &prompt_tokens, &prompt_count) != 0 ||
        (result_line && system_notice_text(game, fragment, &result_tokens, &result_count) != 0) ||
        fd2_scene_field_notice_page(vga, game->fdother, &game->dato_archive, portrait) != 0)
        return -1;
    system_notice_text_draw(game, vga, prompt_tokens, prompt_count, 127);
    if (result_line) system_notice_text_draw(game, vga, result_tokens, result_count, 146);
    return 0;
}

static void render_system_result(const fd2_field_game *game, fd2_vga *vga) {
    if (game && vga && game->system_result_fragment)
        (void)fd2_field_game_render_system_notice(game, vga, game->system_result_fragment);
}

static void render_field_system_menu(const fd2_field_game *game,
                                     fd2_vga *vga, int camera_x, int camera_y) {
    if (!game || !vga || game->interaction !=
            FD2_FIELD_INTERACTION_SYSTEM_MENU)
        return;
    if (game->system_menu.page == FD2_FIELD_SYSTEM_PAGE_CONFIRMATION) {
        if (fd2_field_game_render_system_notice(game, vga,
                game->system_menu.confirmation_prompt_fragment) != 0) return;
        /* sub_19953 @0x19953：DS:0x1eed={16,17}，YES/NO 最终位置。
         * 复用 FDOTHER[2] 原图，选中项在 normal/highlight 间闪动。 */
        for (uint8_t choice = 0; choice < 2; choice++) {
            const fd2_image *button = fd2_field_command_frame_id(
                &game->command_assets, (uint8_t)(16 + choice),
                game->system_menu.confirmation_yes == (choice == 0), 0,
                game->system_menu.highlight_phase);
            if (button)
                fd2_map_sprite_blit(vga, button, 232 + choice * 32, 168, 0);
        }
        return;
    }
    uint8_t command_ids[FD2_FIELD_SYSTEM_MENU_COUNT];
    memcpy(command_ids, game->system_menu.command_ids, sizeof(command_ids));
    fd2_field_command_draw_id_animation(
        vga, &game->command_assets,
        game->cursor_cell_x * 24 - camera_x,
        game->cursor_cell_y * 24 - camera_y,
        command_ids, game->system_menu.selected,
        game->system_menu.disabled, game->system_menu.highlight_phase,
        game->system_menu.animation_opening,
        game->system_menu.animation_phase);
}

static void render_move_selection(const fd2_field_game *game,
                                  fd2_vga *vga,
                                  int camera_x, int camera_y) {
    if (!game || !vga ||
        (game->interaction != FD2_FIELD_INTERACTION_UNIT_SELECTED &&
         game->interaction != FD2_FIELD_INTERACTION_TARGETING &&
         game->interaction != FD2_FIELD_INTERACTION_MAGIC_TARGETING &&
         game->interaction != FD2_FIELD_INTERACTION_ITEM_TARGETING &&
         game->interaction != FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING) ||
        !game->move_range.nodes)
        return;
    if (game->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING) {
        const uint8_t *magic = fd2_field_magic_record_get(game->magic_target_id);
        if (!magic || !magic[3]) return;
    }

    /* field_view_render_tiles @0x11eee：node offset 0x07 不为 0xff
     * 的可达格不叠加独立贴图，而是用 FDOTHER[3] 的 256 字节 LUT 重绘
     * 整个 FDSHAP 24×24 tile。DS:0x1a97 选择 20 相位动画帧。 */
    const uint8_t *lut = fd2_field_range_lut(
        &game->visuals, game->range_visual_phase);
    if (!lut) return;
    for (int y = 0; y < game->move_range.height; y++) {
        for (int x = 0; x < game->move_range.width; x++) {
            if (!fd2_field_path_is_destination(&game->move_range, x, y))
                continue;
            fd2_field_apply_palette_lut(vga, x * 24 - camera_x,
                                        y * 24 - camera_y, 24, 24, lut);
        }
    }
}

static void render_actor_terrain_overlays(const fd2_field_game *game,
                                          fd2_vga *vga,
                                          int camera_x, int camera_y, int draw_ui, uint8_t terrain_phase) {
    const fd2_field_path_result *range = NULL;
    const uint8_t *lut = NULL;
    if (draw_ui && game->move_range.nodes &&
        (game->interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED ||
         game->interaction == FD2_FIELD_INTERACTION_TARGETING ||
         game->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING ||
         game->interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING ||
         game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING)) {
        range = &game->move_range;
        lut = fd2_field_range_lut(&game->visuals, game->range_visual_phase);
    }
    if (game->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING) {
        const uint8_t *magic = fd2_field_magic_record_get(game->magic_target_id);
        if (!magic || !magic[3]) { range = NULL; lut = NULL; }
    }
    fd2_field_units_render_terrain_overlays(vga, &game->terrain, &game->map,
        &game->units, camera_x, camera_y, terrain_phase, range, lut);
}

static void render_field_cursor(const fd2_field_game *game,
                                fd2_vga *vga,
                                int camera_x, int camera_y, int normal) {
    if (!game || !vga || (!normal && (game->active_side != 2 ||
        game->interaction == FD2_FIELD_INTERACTION_MOVING)))
        return;

    /* field_selection_overlay_draw @0x122dc 在普通模式 1 调用
     * field_selection_sprite_blit @0x126f7，把 FDOTHER[1] frame 0
     * 以透明 SKIP 规则画在焦点格；顺序位于地形/范围之后、单位之前。 */
    /* normal=2仅由正式11cac工作页传入，读取既有raw owner，无平行状态。 */
    unsigned display = normal==2 ? game->ordinary_display_mode:1;
    if (!normal && game->interaction == FD2_FIELD_INTERACTION_MAGIC_TELEPORTING) display = 2;
    if (!normal && game->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING) {
        const uint8_t *record = fd2_field_magic_record_get(game->magic_target_id);
        if (record && record[3]) display = (unsigned)record[4] + 2u;
    }
    if (!normal && game->interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING) {
        const uint8_t *record = item_target_record(game);
        if (record) display = (unsigned)record[0x12] + 2u;
    }
    /* 原122dc switch只有1..6；6写node无sprite，其它值走default。
     * 15311的magic10..12确实产生7/9/11，不是资源非法值。 */
    if(display==0 || display>=6)return;
    /* 122dc/126f7：每个格先做13×8视口门，再画完整24×24；actor后盖。
     * display6由工作页selection_cursor_mark拥有map/node写集，本纯像素叶无sprite。 */
    static const int8_t sprites[][3] = {
        {0,0,1},{0,-3,2},{-3,0,3},{3,0,4},{0,3,5},
        {-1,-2,6},{-2,-1,6},{1,-2,7},{2,-1,7},
        {-1,2,8},{-2,1,8},{1,2,9},{2,1,9},
        {0,-2,10},{-2,0,11},{2,0,12},{0,2,13},
        {-1,-1,15},{1,-1,16},{-1,1,17},{1,1,18}
    };
    static const int8_t radius2[][3] = {
        {0,0,1},{0,-2,2},{-2,0,3},{2,0,4},{0,2,5},
        {-1,-1,6},{1,-1,7},{-1,1,8},{1,1,9},
        {0,-1,10},{-1,0,11},{1,0,12},{0,1,13}
    };
    static const int8_t radius1[][3] = {{0,0,14},{0,-1,2},{-1,0,3},{1,0,4},{0,1,5}};
    const int8_t one[1][3] = {{0,0,display == 1 ? 0 : 1}};
    const int8_t (*parts)[3] = display == 3 ? radius1 : display == 4 ? radius2 : display == 5 ? sprites : one;
    size_t count = display == 3 ? 5 : display == 4 ? 13 : display == 5 ? 21 : 1;
    for (size_t i = 0; i < count; i++) {
        int x = game->cursor_cell_x + parts[i][0], y = game->cursor_cell_y + parts[i][1];
        if (x < game->camera_cell_x || x >= game->camera_cell_x + 13 ||
            y < game->camera_cell_y || y >= game->camera_cell_y + 8) continue;
        const fd2_image *cursor = fd2_field_cursor_frame(&game->visuals, (size_t)parts[i][2]);
        if (cursor) fd2_map_sprite_blit(vga, cursor, x * 24 - camera_x, y * 24 - camera_y, 0);
    }
}

static int move_camera_internal(fd2_field_game *game, int dx, int dy) {
    if (!game || !game->ready) return 0;
    int old_x = game->camera_cell_x;
    int old_y = game->camera_cell_y;
    game->camera_cell_x = clamp_int(old_x + dx, 0, max_camera_x(game));
    game->camera_cell_y = clamp_int(old_y + dy, 0, max_camera_y(game));
    return game->camera_cell_x != old_x || game->camera_cell_y != old_y;
}

static int move_cursor_internal(fd2_field_game *game, int dx, int dy) {
    if (!game || !game->ready) return 0;
    int old_x = game->cursor_cell_x;
    int old_y = game->cursor_cell_y;
    int new_x = clamp_int(old_x + dx, 0, game->map.width - 1);
    int new_y = clamp_int(old_y + dy, 0, game->map.height - 1);
    if (new_x == old_x && new_y == old_y) return 0;

    /* sub_11C59/11B48 @0x11c59/@0x11b48（原登记同名）：移动前相对格
     * 左/上严格<2才卷动；右/下>=11/>=6。地图边缘仍受camera范围限制。 */
    int rel_x = old_x - game->camera_cell_x;
    int rel_y = old_y - game->camera_cell_y;
    if (new_x < old_x && rel_x < 2)
        move_camera_internal(game, -1, 0);
    else if (new_x > old_x && rel_x >= 11)
        move_camera_internal(game, 1, 0);
    if (new_y < old_y && rel_y < 2)
        move_camera_internal(game, 0, -1);
    else if (new_y > old_y && rel_y >= 6)
        move_camera_internal(game, 0, 1);

    game->cursor_cell_x = new_x;
    game->cursor_cell_y = new_y;
    if(!game->visual_owner)prepare_item_cursor(game); /* 无App页的纯逻辑诊断。 */
    game->detail_acknowledged_unit = -1;
    if (game->interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED)
        (void)fd2_field_game_update_move_preview(game);
    return 1;
}

int fd2_field_game_move_camera(fd2_field_game *game, int dx, int dy) {
    return !game || progress_pending(game) ? 0 : move_camera_internal(game, dx, dy);
}
int fd2_field_game_move_cursor(fd2_field_game *game, int dx, int dy) {
    if(!game || progress_pending(game) || game->failure!=FD2_FIELD_FAILURE_NONE || game->runtime_host_quit)return 0;
    int cx=game->camera_cell_x,cy=game->camera_cell_y;
    int moved=move_cursor_internal(game,dx,dy);
    if(moved && (game->ordinary_display_mode || cx!=game->camera_cell_x || cy!=game->camera_cell_y)) {
        /* 原四方向helper自己11cac(0)，不借12cea追加wait/clear。 */
        if(turn_present(game,FD2_FIELD_RUNTIME_CURSOR_STEP,game->ordinary_display_mode)<0)
            fail_action(game,game->selected_unit>=0?(size_t)game->selected_unit:0);
    }
    return moved;
}

/* field_focus_move_to @0x12cea（原登记 FUN_00010432）：入口11cac(0)，
 * X→Y逐格；mode0仅卷镜头时方向helper刷新，mode6刷新但免wait1。
 * 几何/prepare_item_cursor只在Game；HOST沿旧void合同0+既有锁存返回。 */
static int focus_to(fd2_field_game *game, int x, int y) {
    if (!game || !game->ready || x < 0 || y < 0 ||
        x >= game->map.width || y >= game->map.height ||
        game->failure!=FD2_FIELD_FAILURE_NONE)
        return -1;
    if(game->runtime_host_quit)return 0;
    int result=turn_present(game,FD2_FIELD_RUNTIME_FOCUS_BEGIN,game->ordinary_display_mode);
    if(result<0 || game->failure!=FD2_FIELD_FAILURE_NONE)goto failed;
    if(game->runtime_host_quit)return 0;
    for(unsigned axis=0;axis<2u;axis++) {
        while((axis?game->cursor_cell_y:game->cursor_cell_x)!=(axis?y:x)) {
            int cx=game->camera_cell_x,cy=game->camera_cell_y;
            int delta=(axis?game->cursor_cell_y:game->cursor_cell_x)<(axis?y:x)?1:-1;
            if(!move_cursor_internal(game,axis?0:delta,axis?delta:0))goto failed;
            if(game->ordinary_display_mode || cx!=game->camera_cell_x || cy!=game->camera_cell_y) {
                result=turn_present(game,FD2_FIELD_RUNTIME_FOCUS_STEP,game->ordinary_display_mode);
                if(result<0 || game->failure!=FD2_FIELD_FAILURE_NONE)goto failed;
                if(game->runtime_host_quit)return 0;
            }
            result=turn_present(game,FD2_FIELD_RUNTIME_CLEAR_KEYS,0);
            if(result<0 || game->failure!=FD2_FIELD_FAILURE_NONE)goto failed;
            if(game->runtime_host_quit)return 0;
        }
    }
    return 0;
failed:
    fail_action(game,game->selected_unit>=0?(size_t)game->selected_unit:0);return -1;
}

int fd2_field_game_cycle_focus(fd2_field_game *game) {
    if (!game || !game->ready || progress_pending(game) || game->active_side != 2u ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE ||
        game->units.count == 0)
        return -1;

    size_t start = game->focus_cycle_unit;
    if (start >= game->units.count) start = 0;
    for (size_t step = 0; step < game->units.count; step++) {
        size_t index = (start + step) % game->units.count;
        fd2_field_unit *unit = &game->units.items[index];
        if (fd2_field_unit_is_hidden(unit) ||
            (unit->flags & FD2_FIELD_UNIT_FLAG_AI_INELIGIBLE) ||
            fd2_field_unit_has_acted(unit) || unit->side != 2u)
            continue;
        /* field_controller_input @0x117e7（原登记同名）先经12d7b/12cea
         * 完成聚焦，再推进轮询owner；非法actor目标不留下半次聚焦。 */
        if (focus_to(game, unit->x, unit->y) != 0) return -1;
        if(game->runtime_host_quit)return -1;
        game->focus_cycle_unit = (index + 1u) % game->units.count;
        game->detail_acknowledged_unit = -1;
        return (int)index;
    }
    return -1;
}

int fd2_field_game_unit_at(const fd2_field_game *game, int x, int y) {
    if (!game || !game->ready) return -1;
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (!fd2_field_unit_is_hidden(unit) && unit->x == x && unit->y == y)
            return (int)i;
    }
    return -1;
}

/* 18890：immune→19，否则unit_id28→16；14237：immune→19，否则raw20。
 * 14b78另按text_id28覆盖为1，不能把三个caller合成全局profile28修正。 */
static const uint8_t *move_profile(const fd2_field_unit *unit, int player) {
    uint8_t id = unit->movement_profile;
    if (fd2_field_attack_unit_ignores_terrain_modifier(unit)) id = 19u;
    else if (player && unit->unit_id == 28u) id = 16u;
    return fd2_field_movement_profile_get(id);
}

static int player_move_range(fd2_field_game *game, size_t actor_index) {
    const fd2_field_unit *unit = &game->units.items[actor_index];
    const uint8_t *profile = move_profile(unit, 1);
    if (!profile) return -1;
    return fd2_field_move_range_apply(&game->move_range, &game->map,
        &game->terrain, &game->units, actor_index, profile,
        FD2_FIELD_MOVEMENT_COST_CLASS_COUNT, unit->movement_points);
}

/* 关闭目标范围owner并清空法术／物品／攻击共用的候选与焦点游标。 */
static void close_target_range(fd2_field_game *game) {
    fd2_field_path_close(&game->move_range);
    game->magic_target_id = 0;
    memset(game->magic_target_candidates, 0, sizeof(game->magic_target_candidates));
    game->magic_target_count = game->magic_target_cycle = 0;
}

static void clear_move_query(fd2_field_game *game) {
    if (!game) return;
    fd2_field_path_close(&game->move_range);
    game->move_path_length = 0;
    game->move_preview_valid = 0;
    game->camera_pixel_offset_x = 0;
    game->camera_pixel_offset_y = 0;
}

static int begin_move_selection(fd2_field_game *game, int unit_index) {
    if (!game || unit_index < 0 ||
        (size_t)unit_index >= game->units.count)
        return -1;

    size_t node_count = (size_t)game->map.width * (size_t)game->map.height;
    if (node_count == 0) return -1;
    if (game->move_path_capacity < node_count) {
        uint8_t *path = realloc(game->move_path, node_count);
        if (!path) return -1;
        game->move_path = path;
        game->move_path_capacity = node_count;
    }

    fd2_field_unit *unit = &game->units.items[unit_index];
    game->selected_unit = unit_index;
    game->attack_target = -1;
    game->last_attack_valid = 0;
    game->move_origin_x = unit->x;
    game->move_origin_y = unit->y;
    game->move_origin_direction = unit->direction;
    game->move_origin_camera_x = game->camera_cell_x;
    game->move_origin_camera_y = game->camera_cell_y;
    game->cursor_cell_x = unit->x;
    game->cursor_cell_y = unit->y;
    game->interaction = FD2_FIELD_INTERACTION_UNIT_SELECTED;

    if (player_move_range(game, (size_t)unit_index) != 0 ||
        fd2_field_game_update_move_preview(game) != 0) {
        clear_move_query(game);
        game->selected_unit = -1;
        game->interaction = FD2_FIELD_INTERACTION_BROWSE;
        return -1;
    }
    /* field_player_unit_action @0x18890入口，原controller重入也清同一slot。 */
    game->pending_cell_event.valid = 0;
    game->item_transfer_committed = 0;
    game->item_give_recipient = game->item_give_selected = 0;
    return unit_index;
}

int fd2_field_game_begin_player_controller(fd2_field_game *game, size_t actor_index) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        !fd2_field_stage_battle_is_supported(game->stage) ||
        progress_pending(game) ||
        game->active_side != 2 || game->interaction != FD2_FIELD_INTERACTION_BROWSE ||
        actor_index >= game->units.count ||
        fd2_field_unit_is_hidden(&game->units.items[actor_index])) return -1;
    game->experience_value = 0; /* controller117e7指令VA0x118ee，F2不经过。 */
    return 0;
}

int fd2_field_game_update_move_preview(fd2_field_game *game) {
    if (!game || !game->ready || game->selected_unit < 0 ||
        game->interaction != FD2_FIELD_INTERACTION_UNIT_SELECTED ||
        !game->move_range.nodes || !game->move_path)
        return -1;

    game->move_path_length = 0;
    game->move_preview_valid = 0;
    if (!fd2_field_path_is_destination(&game->move_range,
                                        game->cursor_cell_x,
                                        game->cursor_cell_y))
        return 0;

    const fd2_field_unit *unit = &game->units.items[game->selected_unit];
    const uint8_t *profile = move_profile(unit, 1);
    size_t length = 0;
    /* 189f8的4e4f6自行按R/L/D/U找路；range.previous没有路径语义。
     * 光标预览只做同参数纯查询，正式确认另保18993/18a0c两次reset。 */
    int found = profile ? fd2_field_move_path_find(
        &game->map, &game->terrain, &game->units, (size_t)game->selected_unit,
        profile, FD2_FIELD_MOVEMENT_COST_CLASS_COUNT, unit->movement_points,
        FD2_FIELD_MOVE_PATH_NORMAL, game->cursor_cell_x, game->cursor_cell_y,
        game->move_path, game->move_path_capacity, &length, NULL, NULL) : -1;
    if (found < 0) return -1;
    if (!found) return 0;
    game->move_path_length = length;
    game->move_preview_valid = 1;
    return 0;
}

int fd2_field_game_open_system_menu(fd2_field_game *game) {
    if (!game || !game->ready || progress_pending(game) || game->active_side != 2u ||
        game->failure != FD2_FIELD_FAILURE_NONE || game->player_post_waiting_phase ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE ||
        fd2_field_game_unit_at(game, game->cursor_cell_x,
                               game->cursor_cell_y) >= 0)
        return -1;
    fd2_field_system_menu_init(&game->system_menu);
    fd2_field_system_menu_set_options(&game->system_menu,
                                      game->option_values);
    refresh_system_menu_disabled(game);
    game->last_system_action = FD2_FIELD_SYSTEM_ACTION_NONE;
    game->interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
    return 0;
}

int fd2_field_game_select_system_menu_direction(
        fd2_field_game *game, fd2_field_command_direction direction) {
    if (!game || !game->ready ||
        game->interaction != FD2_FIELD_INTERACTION_SYSTEM_MENU)
        return -1;
    return fd2_field_system_menu_select_direction(
        &game->system_menu, direction);
}

fd2_field_system_action fd2_field_game_confirm_system_menu(
        fd2_field_game *game) {
    if (!game || !game->ready ||
        game->interaction != FD2_FIELD_INTERACTION_SYSTEM_MENU)
        return FD2_FIELD_SYSTEM_ACTION_NONE;
    int confirming = game->system_menu.page ==
        FD2_FIELD_SYSTEM_PAGE_CONFIRMATION;
    game->last_system_action = fd2_field_system_menu_confirm(
        &game->system_menu);
    if (game->last_system_action ==
             FD2_FIELD_SYSTEM_ACTION_OPTIONS_TOGGLE_PENDING) {
        /* field_options_menu_execute @0x1728c（原FUN_000149d4）只写所选字节；
         * 菜单的布尔显示投影不能改写其余存档配置。 */
        size_t index = game->system_menu.selected;
        if (index<2u) game->option_values[index]=(uint8_t)(game->option_values[index]==0u);
        else game->option_values[index] ^= 1u;
        fd2_field_system_menu_set_options(&game->system_menu, game->option_values);
    }
    refresh_system_menu_disabled(game);
    /* field_empty_focus_menu_execute @0x16f55（原登记 FUN_0001469d）
     * 与 secondary 的共享返回尾：确认／拒绝后均退出到浏览。 */
    if (confirming || game->last_system_action ==
                          FD2_FIELD_SYSTEM_ACTION_SCREEN_TRANSITION)
        game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    return game->last_system_action;
}

fd2_field_system_action fd2_field_game_cancel_system_menu(
        fd2_field_game *game) {
    if (!game || !game->ready ||
        game->interaction != FD2_FIELD_INTERACTION_SYSTEM_MENU)
        return FD2_FIELD_SYSTEM_ACTION_NONE;
    int confirming = game->system_menu.page ==
        FD2_FIELD_SYSTEM_PAGE_CONFIRMATION;
    game->last_system_action = fd2_field_system_menu_cancel(
        &game->system_menu);
    if (confirming || game->last_system_action == FD2_FIELD_SYSTEM_ACTION_CANCEL)
        game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    else
        refresh_system_menu_disabled(game);
    return game->last_system_action;
}

void fd2_field_game_set_save_resource_available(fd2_field_game *game,
                                                int available) {
    if (!game) return;
    game->save_resource_available = available ? 1 : 0;
    if (game->interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU &&
        game->system_menu.page == FD2_FIELD_SYSTEM_PAGE_SECONDARY)
        refresh_secondary_menu_disabled(game);
}

int fd2_field_game_open_manual_slot_picker(
        fd2_field_game *game, const char *path,
        fd2_field_manual_slot_mode mode, size_t initial_slot) {
    if (!game || !game->ready || !path ||
        game->interaction != FD2_FIELD_INTERACTION_SYSTEM_MENU)
        return -1;
    fd2_save_file save = {0};
    /* 原版 Save 在文件不存在时以 0xff 建立容器；picker 也保留这一
     * 语义，真正提交时才使用独占临时文件写回。 */
    if (fd2_save_file_open(&save, path) != 0 &&
        mode == FD2_FIELD_MANUAL_SLOT_MODE_SAVE)
        (void)fd2_save_file_create_empty(&save);
    fd2_field_manual_slot_picker_init(
        &game->manual_slot_picker, mode, initial_slot,
        save.data ? &save : NULL);
    fd2_save_file_close(&save);
    game->interaction = FD2_FIELD_INTERACTION_MANUAL_SLOT;
    return 0;
}

int fd2_field_game_move_manual_slot_picker(fd2_field_game *game, int delta) {
    if (!game || game->interaction != FD2_FIELD_INTERACTION_MANUAL_SLOT)
        return 0;
    return fd2_field_manual_slot_picker_move(
        &game->manual_slot_picker, delta);
}

int fd2_field_game_confirm_manual_slot_picker(fd2_field_game *game) {
    if (!game || game->interaction != FD2_FIELD_INTERACTION_MANUAL_SLOT)
        return -1;
    int selected = fd2_field_manual_slot_picker_confirm(
        &game->manual_slot_picker);
    if (selected >= 0)
        game->interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
    return selected;
}

int fd2_field_game_cancel_manual_slot_picker(fd2_field_game *game) {
    if (!game || game->interaction != FD2_FIELD_INTERACTION_MANUAL_SLOT)
        return 0;
    fd2_field_manual_slot_picker_cancel(&game->manual_slot_picker);
    game->interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
    return 1;
}

int fd2_field_game_animate_system_menu(fd2_field_game *game, fd2_vga *vga,
                                       int opening,
                                       uint32_t frame_delay_ms) {
    if(vga && vga->field_page)return animate_menu_page(game,vga,opening,1);
    /* 无App页的历史诊断不签正式menu时序/原页恢复。 */
    if (!game || !game->ready || !vga ||
        game->interaction != FD2_FIELD_INTERACTION_SYSTEM_MENU ||
        !game->command_assets.ready)
        return -1;
    game->system_menu.animation_opening = opening ? 1u : 0u;
    for (uint8_t phase = 1; phase <= 4u; phase++) {
        game->system_menu.animation_phase = phase;
        fd2_field_game_render(game, vga);
        if(fd2_vga_present_timed(vga, frame_delay_ms)<0)return -1;
    }
    game->system_menu.animation_opening = 0;
    game->system_menu.animation_phase = 0;
    return 0;
}

int fd2_field_game_confirm_cursor(fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        progress_pending(game) ||
        game->player_post_waiting_phase) return -1;

    if (game->interaction == FD2_FIELD_INTERACTION_BROWSE) {
        if (game->active_side != 2) return -1;
        int unit_index = fd2_field_game_unit_at(game,
                                                game->cursor_cell_x,
                                                game->cursor_cell_y);
        if (unit_index >= 0 && fd2_field_game_begin_player_controller(game, (size_t)unit_index) != 0)
            return -1;
        if (unit_index < 0 ||
            game->units.items[unit_index].side != game->active_side ||
            fd2_field_unit_is_hidden(&game->units.items[unit_index]) ||
            fd2_field_unit_has_acted(&game->units.items[unit_index]) ||
            game->units.items[unit_index].detail_status[1] != 0u ||
            game->units.items[unit_index].unit_id == 0x79u ||
            game->units.items[unit_index].race == 0x0au)
            return -1;
        /* 117e7/18890的controller入口只查flags/side，不因已Load的HP0
         * 可见记录跳过选择及取消后的1197b；攻击/AI各自的HP门不变。 */
        int selected = begin_move_selection(game, unit_index);
        if (selected >= 0) game->ordinary_display_mode = 1; /* 18890 @18a2e。 */
        return selected;
    }

    if (game->interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED) {
        if (!fd2_field_path_is_destination(&game->move_range,
                game->cursor_cell_x, game->cursor_cell_y)) return -1;
        /* selector115b6有效返回先18993清图，再189f8找路；18a0c再清图。
         * 输入无效仍留selector。找路错误保已到达的reset前缀。 */
        if (fd2_field_event_map_reset_cells(&game->map) != 0 ||
            fd2_field_game_update_move_preview(game) != 0 ||
            !game->move_preview_valid) return -1;
        if (fd2_field_event_map_reset_cells(&game->map) != 0) return -1;
        fd2_field_path_close(&game->move_range);
        game->interaction = FD2_FIELD_INTERACTION_MOVING;
        return game->selected_unit;
    }

    if (game->interaction == FD2_FIELD_INTERACTION_COMMAND) {
        int unit_index = game->selected_unit;
        return fd2_field_game_confirm_command(game) == 0
            ? unit_index : -1;
    }

    if (game->interaction == FD2_FIELD_INTERACTION_TARGETING) {
        int unit_index = game->selected_unit;
        if (fd2_field_game_resolve_attack(game) != 0) return -1;
        return unit_index;
    }
    if (game->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING ||
        game->interaction == FD2_FIELD_INTERACTION_MAGIC_TELEPORTING) {
        int unit_index = game->selected_unit;
        return fd2_field_game_resolve_player_magic(game) == 0 ? unit_index : -1;
    }
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_MENU) {
        int unit_index = game->selected_unit;
        int available = game->command_selected == 0
            ? fd2_field_game_item_use_is_available(game)
            : game->command_selected == 1 ? fd2_field_game_item_give_is_available(game)
            : game->command_selected == 2 ? fd2_field_game_item_equip_is_available(game)
            : game->command_selected == 3 && fd2_field_game_item_drop_is_available(game);
        return available &&
            fd2_field_game_confirm_item_menu(game) == 0 ? unit_index : -1;
    }
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING ||
        game->interaction == FD2_FIELD_INTERACTION_ITEM_TELEPORTING) {
        int unit_index = game->selected_unit;
        return fd2_field_game_item_use_is_available(game) &&
            fd2_field_game_resolve_player_item(game) == 0 ? unit_index : -1;
    }
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING) {
        int unit_index = game->selected_unit;
        return fd2_field_game_item_give_is_available(game) &&
            fd2_field_game_resolve_item_give(game) == 0 ? unit_index : -1;
    }
    return -1;
}

int fd2_field_game_set_attack_hooks(
        fd2_field_game *game, fd2_field_attack_target_fn target_allowed,
        void *target_userdata, fd2_field_critical_base_fn critical_base,
        void *critical_base_userdata,
        fd2_field_combat_rng_fn rng, void *rng_userdata) {
    if (!game || !game->ready || !rng) return -1;
    game->attack_target_allowed = target_allowed;
    game->attack_target_userdata = target_userdata;
    game->attack_critical_base = critical_base;
    game->attack_critical_base_userdata = critical_base_userdata;
    game->attack_rng = rng;
    game->attack_rng_userdata = rng_userdata;
    return 0;
}

int fd2_field_game_attack_target_is_legal(const fd2_field_game *game,
                                          size_t target_index) {
    if (!game || !game->ready || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count ||
        target_index >= game->units.count)
        return 0;

    const fd2_field_unit *attacker = &game->units.items[game->selected_unit];
    const fd2_field_unit *defender = &game->units.items[target_index];
    if (attacker == defender || attacker->side != 2 ||
        attacker->side != game->active_side || defender->side != 0 ||
        attacker->hp == 0 || defender->hp == 0 ||
        fd2_field_unit_is_hidden(attacker) ||
        fd2_field_unit_is_hidden(defender) ||
        !unit_can_act_for_side(attacker, game->active_side) ||
        fd2_field_unit_has_acted(attacker))
        return 0;
    uint8_t min_range = 0;
    uint8_t max_range = 0;
    if (fd2_field_attack_weapon_range(attacker, &min_range, &max_range) != 0)
        return 0;
    if (game->attack_target_allowed)
        return game->attack_target_allowed(attacker, defender,
                                           game->attack_target_userdata) != 0;
    return fd2_field_attack_target_is_legal(
        &game->map, &game->terrain, &game->units,
        (size_t)game->selected_unit, target_index);
}

static int fd2_field_game_attack_is_available(const fd2_field_game *game) {
    if (!game || !game->ready || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count ||
        !game->attack_rng)
        return 0;
    const fd2_field_unit *attacker =
        &game->units.items[game->selected_unit];
    uint8_t min_range = 0;
    uint8_t max_range = 0;
    if (!unit_can_act_for_side(attacker, game->active_side) ||
        fd2_field_unit_has_acted(attacker) ||
        fd2_field_attack_weapon_range(attacker, &min_range, &max_range) != 0)
        return 0;
    for (size_t i = 0; i < game->units.count; i++)
        if (fd2_field_game_attack_target_is_legal(game, i)) return 1;
    return 0;
}

int fd2_field_game_refresh_commands(fd2_field_game *game) {
    if (!game || !game->ready || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count ||
        game->interaction != FD2_FIELD_INTERACTION_COMMAND)
        return -1;

    /* field_player_command_execute_core @0x18d8c 的原版顺序固定为
     * attack/magic/item/wait。field_unit_inventory_count @0x1b8a6 控制
     * item 可用性；field_unit_magic_list_build @0x1c269 与 record +0x27
     * 控制 magic 可用性。其执行入口分别为
     * field_player_item_command_execute @0x1bbdc 与
     * field_player_magic_command_execute @0x1cff0。法术只按当前已验证的
     * 基础分支开放；物品按当前已确认的Use效果能力开放。 */
    game->command_disabled[FD2_FIELD_COMMAND_ATTACK] =
        fd2_field_game_attack_is_available(game) ? 0u : 1u;
    game->command_disabled[FD2_FIELD_COMMAND_MAGIC] =
        fd2_field_game_magic_is_available(game) ? 0u : 1u;
    game->command_disabled[FD2_FIELD_COMMAND_ITEM] =
        fd2_field_game_item_is_available(game) ? 0u : 1u;
    game->command_disabled[FD2_FIELD_COMMAND_WAIT] = 0u;
    game->command_selected = fd2_field_command_first_enabled(
        game->command_disabled);
    game->command_highlight_phase = 0;
    game->command_animation_phase = 0;
    game->command_animation_opening = 0;
    game->last_command_highlight_ms = 0;
    return game->command_selected >= 0 ? 0 : -1;
}

int fd2_field_game_select_command_direction(
        fd2_field_game *game, fd2_field_command_direction direction) {
    if (!game || !game->ready ||
        game->interaction != FD2_FIELD_INTERACTION_COMMAND)
        return -1;
    int previous = game->command_selected;
    game->command_selected = fd2_field_command_select_direction(
        previous, direction, game->command_disabled);
    return game->command_selected != previous;
}

int fd2_field_game_confirm_command(fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        progress_pending(game) ||
        game->interaction != FD2_FIELD_INTERACTION_COMMAND ||
        game->command_selected < 0 ||
        game->command_selected >= (int)FD2_FIELD_COMMAND_COUNT ||
        game->command_disabled[game->command_selected])
        return -1;
    switch ((fd2_field_command)game->command_selected) {
        case FD2_FIELD_COMMAND_ATTACK:
            return fd2_field_game_begin_attack(game);
        case FD2_FIELD_COMMAND_WAIT:
            return fd2_field_game_finish_unit_action(game);
        case FD2_FIELD_COMMAND_MAGIC:
            return fd2_field_game_begin_magic_list(game);
        case FD2_FIELD_COMMAND_ITEM:
            return fd2_field_game_item_is_available(game)
                ? fd2_field_game_begin_item_menu(game) : -1;
    }
    return -1;
}

int fd2_field_game_animate_command(fd2_field_game *game, fd2_vga *vga,
                                   int opening, uint32_t frame_delay_ms) {
    if(vga && vga->field_page)return animate_menu_page(game,vga,opening,0);
    /* 无App页的历史诊断仍保旧固定帧调用口。 */
    if (!game || !game->ready || !vga ||
        (game->interaction != FD2_FIELD_INTERACTION_COMMAND &&
         game->interaction != FD2_FIELD_INTERACTION_ITEM_MENU) ||
        !game->command_assets.ready)
        return -1;
    game->command_animation_opening = opening ? 1u : 0u;
    for (uint8_t phase = 1; phase <= 4u; phase++) {
        game->command_animation_phase = phase;
        fd2_field_game_render(game, vga);
        if(fd2_vga_present_timed(vga, frame_delay_ms)<0)return -1;
    }
    game->command_animation_opening = 0;
    game->command_animation_phase = 0;
    return 0;
}

static int magic_list_actor_is_valid(const fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        progress_pending(game) ||
        game->player_post_waiting_phase || game->active_side != 2 ||
        game->units.count > FD2_FIELD_MAX_UNITS || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count)
        return 0;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    /* field_player_unit_action @0x18890：非零路径后，指令VA 0x18a63..
     * 0x18a72仅让unit18/19/34保留MAGIC，其他ID置disabled[1]。 */
    return unit_can_act_for_side(actor, 2) &&
           !fd2_field_unit_has_acted(actor) && !actor->detail_status[2] &&
           (game->move_path_length == 0 || actor->unit_id == 18 ||
            actor->unit_id == 19 || actor->unit_id == 34);
}

static int player_magic_entry_is_supported(const fd2_field_unit *actor, uint8_t id) {
    /* G1正常初始/模板/学习/转职写入闭包：31为空名且无赋值源，36..39无record。
     * caster资源在确认后的PREPARE独立预检，不把unit9夹具充当全体默认。 */
    return actor && id < FD2_FIELD_MAGIC_COUNT && id != 31u;
}

int fd2_field_game_begin_magic_list(fd2_field_game *game) {
    if (!magic_list_actor_is_valid(game) ||
        game->interaction != FD2_FIELD_INTERACTION_COMMAND)
        return -1;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    size_t count = fd2_field_unit_magic_list_build(actor, NULL, 0);
    int selected = 0;
    uint8_t id = 0;
    /* 完整record预检复用纯选择器；12项是现有画面的可见支持门。
     * MP不足不禁止进入列表，只有确认受费用门约束。 */
    if (count == 0 || count > FD2_FIELD_MAGIC_LIST_MAX_COUNT ||
        fd2_field_magic_list_select_step(actor, &selected,
            FD2_FIELD_MAGIC_LIST_KEY_OTHER, &id) != 0)
        return -1;
    game->magic_list_selected = 0;
    game->interaction = FD2_FIELD_INTERACTION_MAGIC_LIST;
    return 0;
}

int fd2_field_game_magic_list_step(fd2_field_game *game,
                                   fd2_field_magic_list_key key,
                                   uint8_t *confirmed_id) {
    if (!magic_list_actor_is_valid(game) ||
        game->interaction != FD2_FIELD_INTERACTION_MAGIC_LIST)
        return -2;
    if (!confirmed_id) return -2;
    uint8_t candidate = 0xff;
    int result = fd2_field_magic_list_select_step(
        &game->units.items[game->selected_unit], &game->magic_list_selected,
        key, &candidate);
    if (result == 1) {
        const fd2_field_unit *actor = &game->units.items[game->selected_unit];
        if (!player_magic_entry_is_supported(actor, candidate)) return 0;
        *confirmed_id = candidate;
    }
    return result;
}

int fd2_field_game_end_magic_list(fd2_field_game *game) {
    if (!magic_list_actor_is_valid(game) ||
        game->interaction != FD2_FIELD_INTERACTION_MAGIC_LIST)
        return -1;
    game->magic_list_selected = 0;
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    game->experience_value = 0; /* field_player_command_execute指令VA0x18dc4。 */
    return fd2_field_game_refresh_commands(game);
}

/* field_player_item_command_execute @0x1bbdc（原登记同名）与
 * field_unit_inventory_select @0x1b932（原sub_1B932）：不套MAGIC的
 * MP/+27/移动后unit例外门，纯库存预检复用1b9de的SDL支持门。 */
static int item_actor_is_valid(const fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        progress_pending(game) ||
        !fd2_field_stage_battle_is_supported(game->stage) || game->player_post_waiting_phase || game->active_side != 2 ||
        game->units.count > FD2_FIELD_MAX_UNITS || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count)
        return 0;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    return unit_can_act_for_side(actor, 2) && !fd2_field_unit_has_acted(actor);
}

static int item_menu_actor_is_valid(const fd2_field_game *game) {
    if (!item_actor_is_valid(game)) return 0;
    int selected = 0;
    return fd2_field_item_list_select_step(&game->units.items[game->selected_unit],
            &selected, FD2_FIELD_ITEM_LIST_KEY_OTHER, 1) == 0;
}

static int item_command_is_supported(const fd2_field_game *game, int command) {
    if (!item_menu_actor_is_valid(game)) return 0;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    if (command == 1 || command == 2 || command == 3) {
        if (command == 2) {
            const uint8_t *raw = (const uint8_t *)actor;
            if (fd2_field_item_equip_compatibility(actor->movement_profile, raw[0x0b]) < 0)
                return 0;
            for (size_t slot = 0; slot < 8u; slot++)
                if ((raw[0x0a + slot * 2u] & 0x40u) &&
                    !fd2_field_item_record_get(raw[0x0b + slot * 2u])) return 0;
            return 1; /* 不兼容确认不执行1b750，先只校验全8槽lookup。 */
        }
        /* Drop无Use handler门，包含隐藏flag40的全8槽必须可重算。 */
        fd2_field_unit shadow = *actor;
        return fd2_field_unit_combat_stats_recompute(&shadow, fd2_field_unit_original_item_stats, NULL,
            fd2_field_unit_original_status_scale, NULL) == 0 && (command != 1 || item_give_has_target(game));
    }
    if (command != 0) return 0;
    const uint8_t *raw = (const uint8_t *)actor;
    size_t count = fd2_field_unit_inventory_count(actor);
    for (size_t i = 0; i < count; i++) {
        uint8_t id = raw[0x0bu + i * 2u];
        const uint8_t *record = fd2_field_item_record_get(id);
        /* 20c6f实际215记录仅code0与5..24；code0确认仍留页。 */
        if (record[0x0d] && (record[0x0d] < 5 || record[0x0d] > 24)) return 0;
    }
    return 1;
}

int fd2_field_game_item_use_is_available(const fd2_field_game *game) {
    return game && game->attack_rng && item_command_is_supported(game, 0);
}

int fd2_field_game_item_drop_is_available(const fd2_field_game *game) {
    return game && game->attack_rng && item_command_is_supported(game, 3);
}

int fd2_field_game_item_equip_is_available(const fd2_field_game *game) {
    return game && game->attack_rng && item_command_is_supported(game, 2);
}

int fd2_field_game_item_give_is_available(const fd2_field_game *game) {
    return game && game->attack_rng && item_command_is_supported(game, 1);
}

int fd2_field_game_item_is_available(const fd2_field_game *game) {
    return fd2_field_game_item_use_is_available(game) || fd2_field_game_item_give_is_available(game) ||
        fd2_field_game_item_equip_is_available(game) ||
        fd2_field_game_item_drop_is_available(game);
}

static int enter_item_menu(fd2_field_game *game) {
    /* 原版非空库存仅按邻接目标禁Give；SDL的Use/Drop能力分别限制，
     * 纯session不执行等待，RNG有效性留正式入口查询。 */
    uint8_t disabled[FD2_FIELD_COMMAND_COUNT] = {
        !item_command_is_supported(game, 0), !item_command_is_supported(game, 1), !item_command_is_supported(game, 2),
        !item_command_is_supported(game, 3)};
    int selected = fd2_field_command_first_enabled(disabled);
    if (selected < 0) return -1;
    memcpy(game->command_disabled, disabled, sizeof(disabled));
    game->command_selected = selected;
    game->command_highlight_phase = 0;
    game->command_animation_phase = 0;
    game->command_animation_opening = 0;
    game->last_command_highlight_ms = 0;
    game->item_list_selected = 0;
    game->interaction = FD2_FIELD_INTERACTION_ITEM_MENU;
    return 0;
}

int fd2_field_game_begin_item_menu(fd2_field_game *game) {
    if (!item_menu_actor_is_valid(game) || game->interaction != FD2_FIELD_INTERACTION_COMMAND)
        return -1;
    return enter_item_menu(game);
}

int fd2_field_game_select_item_menu_direction(
        fd2_field_game *game, fd2_field_command_direction direction) {
    if (!item_menu_actor_is_valid(game) || game->interaction != FD2_FIELD_INTERACTION_ITEM_MENU ||
        direction < FD2_FIELD_COMMAND_DIRECTION_UP || direction > FD2_FIELD_COMMAND_DIRECTION_DOWN)
        return -1;
    int previous = game->command_selected;
    game->command_selected = fd2_field_command_select_direction(previous, direction, game->command_disabled);
    return game->command_selected != previous;
}

int fd2_field_game_confirm_item_menu(fd2_field_game *game) {
    if (!item_menu_actor_is_valid(game) || game->interaction != FD2_FIELD_INTERACTION_ITEM_MENU ||
        game->command_selected < 0 || game->command_selected >= 4 ||
        game->command_disabled[game->command_selected] ||
        !item_command_is_supported(game, game->command_selected))
        return -1;
    game->item_list_selected = 0;
    game->interaction = FD2_FIELD_INTERACTION_ITEM_LIST;
    return 0;
}

int fd2_field_game_item_list_step(fd2_field_game *game, fd2_field_item_list_key key) {
    if (!item_menu_actor_is_valid(game) || game->interaction != FD2_FIELD_INTERACTION_ITEM_LIST ||
        game->command_selected < 0 || game->command_selected >= 4)
        return -2;
    return fd2_field_item_list_select_step(&game->units.items[game->selected_unit],
        &game->item_list_selected, key, game->command_selected == 0);
}

/* field_player_item_command_execute @0x1bbdc（原登记同名）的Drop尾：
 * 0x1bfda取消亦落入field_unit_combat_stats_recompute @0x1b750；确认才先
 * field_unit_inventory_slot_remove @0x1b8e7。shadow确保晚坏装备不改库存。 */
static int finish_item_drop(fd2_field_game *game, int confirmed) {
    if (!item_menu_actor_is_valid(game) || game->interaction != FD2_FIELD_INTERACTION_ITEM_LIST ||
        game->command_selected != 3)
        return -1;
    fd2_field_unit shadow = game->units.items[game->selected_unit];
    int selected = game->item_list_selected;
    if (fd2_field_item_list_select_step(&shadow, &selected,
            FD2_FIELD_ITEM_LIST_KEY_OTHER, 0) != 0)
        return -1;
    if (confirmed) inventory_slot_remove(&shadow, (size_t)selected);
    if (fd2_field_unit_combat_stats_recompute(&shadow, fd2_field_unit_original_item_stats, NULL,
            fd2_field_unit_original_status_scale, NULL) != 0)
        return -1;
    game->units.items[game->selected_unit] = shadow;
    if (fd2_field_unit_inventory_count(&shadow)) {
        return enter_item_menu(game);
    }
    /* 基础actor门允许刚删空的库存；refresh的其余前置均已满足，WAIT恒可用。 */
    game->item_list_selected = 0;
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    return fd2_field_game_refresh_commands(game);
}

int fd2_field_game_resolve_item_drop(fd2_field_game *game) {
    return finish_item_drop(game, 1);
}

int fd2_field_game_equip_item(fd2_field_game *game) {
    if (!item_command_is_supported(game, 2) || game->interaction != FD2_FIELD_INTERACTION_ITEM_LIST ||
        game->command_selected != 2)
        return -1;
    fd2_field_unit shadow = game->units.items[game->selected_unit];
    int selected = game->item_list_selected;
    if (fd2_field_item_list_select_step(&shadow, &selected, FD2_FIELD_ITEM_LIST_KEY_OTHER, 0) != 0)
        return -1;
    uint8_t *raw = (uint8_t *)&shadow;
    uint8_t item_id = raw[0x0b + (size_t)selected * 2u];
    int compatible = fd2_field_item_equip_compatibility(shadow.movement_profile, item_id);
    if (compatible != 1) return compatible;
    /* field_unit_equipment_slot_set @0x1c142（原sub_1C142）：按ID的<128/≥128组清整个flag，
     * 再将selected写40；不是toggle，且不按category分组或跳过hidden。 */
    if (fd2_field_unit_equipment_slot_set(&shadow,(size_t)selected)!=0) return -1;
    /* hidden equipped清0可能唤醒空洞；不能交接本SDL raw导航不支持的结果。 */
    if (fd2_field_item_list_select_step(&shadow, &selected, FD2_FIELD_ITEM_LIST_KEY_OTHER, 0) != 0 ||
        fd2_field_unit_combat_stats_recompute(&shadow, fd2_field_unit_original_item_stats, NULL,
            fd2_field_unit_original_status_scale, NULL) != 0)
        return -1;
    game->units.items[game->selected_unit] = shadow;
    return 1; /* 1bffe重绘并继续原页/原索引，不关闭或推进controller。 */
}

int fd2_field_game_end_item_list(fd2_field_game *game) {
    if (game && game->interaction == FD2_FIELD_INTERACTION_ITEM_LIST && game->command_selected == 1)
        return finish_item_give_cancel(game, 0);
    if (game && game->interaction == FD2_FIELD_INTERACTION_ITEM_LIST &&
        game->command_selected == 3)
        return finish_item_drop(game, 0);
    if (!item_menu_actor_is_valid(game) || game->interaction != FD2_FIELD_INTERACTION_ITEM_LIST ||
        (game->command_selected != 0 && game->command_selected != 2))
        return -1;
    int selected = game->item_list_selected;
    if (fd2_field_item_list_select_step(&game->units.items[game->selected_unit],
            &selected, FD2_FIELD_ITEM_LIST_KEY_OTHER, game->command_selected == 0) != 0)
        return -1;
    /* Use/EQT取消均重跑1bbdc；EQT的1bffe退出不调用1b750重算。 */
    return enter_item_menu(game);
}

int fd2_field_game_end_item_menu(fd2_field_game *game) {
    if (!item_menu_actor_is_valid(game) || game->interaction != FD2_FIELD_INTERACTION_ITEM_MENU)
        return -1;
    game->item_list_selected = 0;
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    return fd2_field_game_refresh_commands(game);
}

static int target_focus_is_valid(const fd2_field_game *game) {
    return game->map.width > 0 && game->map.height > 0 &&
        game->cursor_cell_x >= 0 && game->cursor_cell_x < game->map.width &&
        game->cursor_cell_y >= 0 && game->cursor_cell_y < game->map.height &&
        game->camera_cell_x >= 0 && game->camera_cell_x <= max_camera_x(game) &&
        game->camera_cell_y >= 0 && game->camera_cell_y <= max_camera_y(game);
}

static int target_matches_filter(const fd2_field_unit *unit, uint8_t filter) {
    if (fd2_field_unit_is_hidden(unit)) return 0;
    switch (filter) {
        case 0: return unit->side == 0;
        case 1: return unit->side != 0;
        case 2: return unit->side == 1;
        case 3: return unit->side == 2;
        default: return 0;
    }
}

static int magic_target_matches(const fd2_field_unit *unit, uint8_t magic_id) {
    /* 14818/14742共享filter：ID0为side0，ID13为side!=0；
     * 不加HP/acted/占格门，不使用敌方AI的filter反转。 */
    const uint8_t *record = fd2_field_magic_record_get(magic_id);
    return record && target_matches_filter(unit, record[6]);
}

static int magic_target_context_is_valid(const fd2_field_game *game) {
    return magic_list_actor_is_valid(game) &&
        (game->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING ||
         game->interaction == FD2_FIELD_INTERACTION_MAGIC_TELEPORTING) &&
        player_magic_entry_is_supported(&game->units.items[game->selected_unit], game->magic_target_id) &&
        target_focus_is_valid(game) &&
        game->units.items[game->selected_unit].x < game->map.width &&
        game->units.items[game->selected_unit].y < game->map.height;
}

/* 14818→115b6的范围/候选与focus顺序由物理、法术、物品真实共用；
 * caller先检查各自actor、record、列表索引与焦点，不混用消费门。 */
static int begin_target_selection(fd2_field_game *game, uint8_t cast, uint8_t minimum,
        uint8_t filter, fd2_field_interaction interaction, uint8_t magic_id) {
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    if (fd2_field_target_range_compute(&range, &game->map, &game->terrain,
            actor->x, actor->y, minimum, cast) != 0)
        return -1;
    uint8_t candidates[FD2_FIELD_MAX_UNITS];
    size_t count = 0;
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (fd2_field_unit_is_hidden(unit)) continue;
        if (unit->x >= game->map.width || unit->y >= game->map.height) {
            fd2_field_path_close(&range);
            return -1;
        }
        if (target_matches_filter(unit, filter) &&
            fd2_field_path_is_destination(&range, unit->x, unit->y))
            candidates[count++] = (uint8_t)i;
    }
    unsigned focus_mode=1;
    if(interaction==FD2_FIELD_INTERACTION_MAGIC_TARGETING) {
        const uint8_t *record=fd2_field_magic_record_get(magic_id);
        if(!record){fd2_field_path_close(&range);return -1;}
        focus_mode=record[3] ? record[4]+2u:1u;
    } else if(interaction==FD2_FIELD_INTERACTION_ITEM_TARGETING) {
        const uint8_t *record=item_target_record(game);
        if(!record){fd2_field_path_close(&range);return -1;}
        focus_mode=record[0x12u]+2u;
    }
    /* 数据预检已完成；正式focus的呈现/host仍可能中断，保已提交范围owner。 */
    fd2_field_path_close(&game->move_range);
    game->move_range = range;
    game->magic_target_id = magic_id;
    memset(game->magic_target_candidates, 0, sizeof(game->magic_target_candidates));
    memcpy(game->magic_target_candidates, candidates, count);
    game->magic_target_count = count;
    game->magic_target_cycle = 0;
    game->interaction = interaction;
    game->ordinary_display_mode=(uint8_t)focus_mode;
    const uint8_t *magic = interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING ?
        fd2_field_magic_record_get(magic_id) : NULL;
    if (count && game->units.items[candidates[0]].unit_id != 121 && (!magic || magic[3]))
        return focus_to(game, game->units.items[candidates[0]].x,
                              game->units.items[candidates[0]].y);
    return 0;
}

int fd2_field_game_begin_magic_targeting(fd2_field_game *game, uint8_t magic_id) {
    if (!magic_list_actor_is_valid(game) ||
        game->interaction != FD2_FIELD_INTERACTION_MAGIC_LIST ||
        !target_focus_is_valid(game))
        return -1;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    if (!player_magic_entry_is_supported(actor, magic_id)) return -1;
    uint8_t confirmed = 0xff;
    int selected = game->magic_list_selected;
    if (fd2_field_unit_magic_list_build(actor, NULL, 0) > FD2_FIELD_MAGIC_LIST_MAX_COUNT ||
        game->cursor_cell_x != actor->x || game->cursor_cell_y != actor->y ||
        fd2_field_magic_list_select_step(actor, &selected,
            FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, &confirmed) != 1 || confirmed != magic_id)
        return -1;
    const uint8_t *record = fd2_field_magic_record_get(magic_id);
    int result = begin_target_selection(game, record[3] ? record[3] : record[4],
        magic_id == 23u ? 1u : 0u, record[6],
        FD2_FIELD_INTERACTION_MAGIC_TARGETING, magic_id);
    return result;
}

/* 1cff0→14818/149f8。范围传播与14742确认的Manhattan门独立；
 * 30按选定方向逐格只收首可见side0，空格/其它side不截断。 */
static int collect_player_magic_targets(const fd2_field_game *game, uint8_t id,
        uint8_t *targets, size_t *count) {
    const uint8_t *record = fd2_field_magic_record_get(id);
    if (!record || !targets || !count || game->selected_unit < 0) return -1;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    for (size_t raw = 0; raw < game->units.count; raw++) {
        const fd2_field_unit *target = &game->units.items[raw];
        if (!fd2_field_unit_is_hidden(target) &&
            (target->x >= game->map.width || target->y >= game->map.height)) return -1;
    }
    size_t used = 0;
    if (id == 30u) {
        int dx = game->cursor_cell_x - actor->x, dy = game->cursor_cell_y - actor->y;
        if (dx && dy) return -1;
        /* 149f8同X分支用originY<=centerY，完全同格也按向下扫描。 */
        if (!dx) dy = dy < 0 ? -1 : 1;
        else { dx = dx < 0 ? -1 : 1; dy = 0; }
        for (unsigned step = 1; step <= (unsigned)record[3] - 16u; step++) {
            int x = actor->x + dx * (int)step, y = actor->y + dy * (int)step;
            if (x < 0 || y < 0 || x >= game->map.width || y >= game->map.height) continue;
            for (size_t raw = 0; raw < game->units.count; raw++) {
                const fd2_field_unit *target = &game->units.items[raw];
                if (fd2_field_unit_is_hidden(target) || target->x != x || target->y != y) continue;
                if (!target->side) targets[used++] = (uint8_t)raw;
                break;
            }
        }
    } else {
        int x = record[3] ? game->cursor_cell_x : actor->x;
        int y = record[3] ? game->cursor_cell_y : actor->y;
        fd2_field_path_result area = FD2_FIELD_PATH_RESULT_INITIALIZER;
        if (fd2_field_target_range_compute(&area, &game->map, &game->terrain,
                x, y, 0, record[4]) != 0) return -1;
        for (size_t raw = 0; raw < game->units.count; raw++) {
            const fd2_field_unit *target = &game->units.items[raw];
            if (magic_target_matches(target, id) &&
                fd2_field_path_is_destination(&area, target->x, target->y))
                targets[used++] = (uint8_t)raw;
        }
        fd2_field_path_close(&area);
    }
    *count = used;
    return 0;
}

int fd2_field_game_magic_target_is_legal(const fd2_field_game *game) {
    if (!magic_target_context_is_valid(game)) return -1;
    if (game->interaction == FD2_FIELD_INTERACTION_MAGIC_TELEPORTING) {
        if (!game->magic_target_count || game->magic_target_candidates[0] >= game->units.count) return -1;
        const fd2_field_unit *target = &game->units.items[game->magic_target_candidates[0]];
        const fd2_field_unit *actor = &game->units.items[game->selected_unit];
        fd2_field_path_result cast = FD2_FIELD_PATH_RESULT_INITIALIZER;
        if (!magic_target_matches(target, 23u) || fd2_field_target_range_compute(&cast,
                &game->map, &game->terrain, actor->x, actor->y, 1,
                fd2_field_magic_record_get(23)[3]) != 0) return -1;
        int inside = fd2_field_path_is_destination(&cast, target->x, target->y);
        fd2_field_path_close(&cast);
        return inside ? teleport_destination_is_legal(game, game->magic_target_candidates[0]) : -1;
    }
    if (!game->move_range.nodes ||
        game->move_range.width != game->map.width || game->move_range.height != game->map.height)
        return -1;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    int selected = game->magic_list_selected;
    uint8_t confirmed = 0xff;
    if (game->move_range.start_x != actor->x || game->move_range.start_y != actor->y ||
        fd2_field_magic_list_select_step(actor, &selected,
            FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, &confirmed) != 1 ||
        confirmed != game->magic_target_id)
        return -1;
    if (!fd2_field_path_is_destination(&game->move_range,
            game->cursor_cell_x, game->cursor_cell_y))
        return 0;
    const uint8_t *record = fd2_field_magic_record_get(game->magic_target_id);
    if (!record[3]) {
        uint8_t targets[FD2_FIELD_MAX_UNITS]; size_t count;
        return collect_player_magic_targets(game, game->magic_target_id, targets, &count) != 0 ? -1 : count != 0;
    }
    /* 14742的strictRadius=area+1。目标可能HP0或acted，仍按原filter收集。 */
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (magic_target_matches(unit, game->magic_target_id) &&
            abs((int)unit->x - game->cursor_cell_x) +
            abs((int)unit->y - game->cursor_cell_y) <= record[4])
            return 1;
    }
    return 0;
}

static int cycle_target_selection(fd2_field_game *game) {
    if (game->magic_target_count > FD2_FIELD_MAX_UNITS ||
        (game->magic_target_count && game->magic_target_cycle >= game->magic_target_count))
        return -1;
    if (!game->magic_target_count) return 0;
    size_t next = (game->magic_target_cycle + 1u) % game->magic_target_count;
    size_t raw = game->magic_target_candidates[next];
    if (raw >= game->units.count) return -1;
    const fd2_field_unit *unit = &game->units.items[raw];
    if(unit->unit_id!=121 && (unit->x>=game->map.width || unit->y>=game->map.height))return -1;
    /* 115b6先递增/回绕槽，再聚焦；HOST保本次已选槽，与browse cycle不同。 */
    game->magic_target_cycle = next;
    if (unit->unit_id != 121 && focus_to(game, unit->x, unit->y) != 0)
        return -1;
    /* 11779先递增再取槽；121不移动但仍占槽，不连续跳过。 */
    return 1;
}

int fd2_field_game_cycle_magic_target(fd2_field_game *game) {
    if (!magic_target_context_is_valid(game)) return -1;
    if (!fd2_field_magic_record_get(game->magic_target_id)[3] ||
        game->interaction == FD2_FIELD_INTERACTION_MAGIC_TELEPORTING) return 0;
    return cycle_target_selection(game);
}

int fd2_field_game_cancel_magic_targeting(fd2_field_game *game) {
    if (!magic_target_context_is_valid(game)) return -1;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    game->ordinary_display_mode=0;
    if (focus_to(game, actor->x, actor->y) != 0) return -1;
    if(game->runtime_host_quit)return 0;
    game->ordinary_display_mode=1;
    close_target_range(game);
    game->magic_list_selected = 0;
    game->interaction = FD2_FIELD_INTERACTION_MAGIC_LIST;
    return 0;
}

/* 1bbdc在1bd14之前按raw槽读record；使用player的cast+10/area+12/
 * filter+15，不借AI的+11筛选；code0不得确认，1..4无已知记录。 */
static const uint8_t *item_target_record(const fd2_field_game *game) {
    if (!item_menu_actor_is_valid(game) || game->command_selected != 0) return NULL;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    int selected = game->item_list_selected;
    if (fd2_field_item_list_select_step(actor, &selected,
            FD2_FIELD_ITEM_LIST_KEY_CONFIRM, 1) != 1)
        return NULL;
    const uint8_t *raw = (const uint8_t *)actor;
    const uint8_t *record = fd2_field_item_record_get(raw[0x0bu + (size_t)selected * 2u]);
    return record && record[0x0d] >= 5 && record[0x0d] <= 24 &&
        record[0x12] <= 4 && record[0x15] <= 3 ? record : NULL;
}

static int item_target_context_is_valid(const fd2_field_game *game) {
    return item_target_record(game) &&
        game->interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING &&
        target_focus_is_valid(game) &&
        game->units.items[game->selected_unit].x < game->map.width &&
        game->units.items[game->selected_unit].y < game->map.height;
}

int fd2_field_game_begin_item_targeting(fd2_field_game *game) {
    const uint8_t *record = item_target_record(game);
    if (!record || game->interaction != FD2_FIELD_INTERACTION_ITEM_LIST ||
        !target_focus_is_valid(game))
        return -1;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    if (game->cursor_cell_x != actor->x || game->cursor_cell_y != actor->y)
        return -1;
    int result = begin_target_selection(game, record[0x10], record[0x0d] == 23 ? 1 : 0, record[0x15],
                                  FD2_FIELD_INTERACTION_ITEM_TARGETING, 0);
    if (!result && !game->visual_owner) prepare_item_cursor(game);
    return result;
}

int fd2_field_game_item_target_is_legal(const fd2_field_game *game) {
    if (item_target_record(game) && game->interaction == FD2_FIELD_INTERACTION_ITEM_TELEPORTING)
        return item_teleport_destination_is_legal(game);
    if (!item_target_context_is_valid(game) || !game->move_range.nodes ||
        game->move_range.width != game->map.width || game->move_range.height != game->map.height)
        return -1;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    if (game->move_range.start_x != actor->x || game->move_range.start_y != actor->y)
        return -1;
    if (!fd2_field_path_is_destination(&game->move_range,
            game->cursor_cell_x, game->cursor_cell_y))
        return 0;
    const uint8_t *record = item_target_record(game);
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (target_matches_filter(unit, record[0x15]) &&
            abs((int)unit->x - game->cursor_cell_x) + abs((int)unit->y - game->cursor_cell_y) <= record[0x12])
            return 1;
    }
    return 0;
}

int fd2_field_game_cycle_item_target(fd2_field_game *game) {
    if (!item_target_context_is_valid(game)) return -1;
    return cycle_target_selection(game);
}

int fd2_field_game_cancel_item_targeting(fd2_field_game *game) {
    if (!item_target_context_is_valid(game) &&
        !(item_target_record(game) && game->interaction == FD2_FIELD_INTERACTION_ITEM_TELEPORTING &&
          target_focus_is_valid(game))) return -1;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    if(fd2_field_event_map_reset_cells(&game->map)!=0)return -1;
    game->ordinary_display_mode=0;
    if (focus_to(game, actor->x, actor->y) != 0) return -1;
    if(game->runtime_host_quit)return 0;
    game->ordinary_display_mode=1;
    close_target_range(game);
    /* 1be60先focus，1be85重开1b932的selected0；不返回物品子菜单。 */
    game->item_list_selected = 0;
    game->interaction = FD2_FIELD_INTERACTION_ITEM_LIST;
    return 0;
}

/* 1bbdc→14818(max1,min1,filter3)：排除中心整格，HP0/acted与121仍计入。 */
static int item_give_has_target(const fd2_field_game *game) {
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    if (fd2_field_target_range_compute(&range, &game->map, &game->terrain,
            actor->x, actor->y, 1, 1) != 0) return 0;
    int found = 0;
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (fd2_field_unit_is_hidden(unit)) continue;
        if (unit->x >= game->map.width || unit->y >= game->map.height) {
            found = 0;
            break;
        }
        if (target_matches_filter(unit, 3) &&
            fd2_field_path_is_destination(&range, unit->x, unit->y)) found = 1;
    }
    fd2_field_path_close(&range);
    return found;
}

static int give_source_is_valid(const fd2_field_game *game) {
    if (!item_menu_actor_is_valid(game) || game->command_selected != 1) return 0;
    int selected = game->item_list_selected;
    return fd2_field_item_list_select_step(&game->units.items[game->selected_unit],
        &selected, FD2_FIELD_ITEM_LIST_KEY_OTHER, 0) == 0;
}

static int inventory_prefix_is_valid(const fd2_field_unit *unit) {
    if (!fd2_field_unit_inventory_count(unit)) return 1;
    int selected = 0;
    return fd2_field_item_list_select_step(unit, &selected, FD2_FIELD_ITEM_LIST_KEY_OTHER, 0) == 0;
}

static int return_item_menu_or_command(fd2_field_game *game) {
    game->item_give_recipient = game->item_give_selected = 0;
    if (fd2_field_unit_inventory_count(&game->units.items[game->selected_unit]))
        return enter_item_menu(game);
    game->item_list_selected = 0;
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    return fd2_field_game_refresh_commands(game);
}

int fd2_field_game_begin_item_give(fd2_field_game *game) {
    if (!give_source_is_valid(game) || game->interaction != FD2_FIELD_INTERACTION_ITEM_LIST ||
        !item_command_is_supported(game, 1) || !target_focus_is_valid(game)) return -1;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    if (game->cursor_cell_x != actor->x || game->cursor_cell_y != actor->y) return -1;
    int result = begin_target_selection(game, 1, 1, 3, FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING, 0);
    if (result == 0) game->item_give_recipient = game->item_give_selected = 0;
    return result;
}

static int give_cell_has_player(const fd2_field_game *game, int x, int y) {
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (target_matches_filter(unit, 3) && unit->x == x && unit->y == y) return 1;
    }
    return 0;
}

static int fd2_field_game_item_give_target_is_legal(const fd2_field_game *game) {
    if (!give_source_is_valid(game) || game->interaction != FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING ||
        !target_focus_is_valid(game)) return -1;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    if (fd2_field_target_range_compute(&range, &game->map, &game->terrain,
            actor->x, actor->y, 1, 1) != 0) return -1;
    int legal = fd2_field_path_is_destination(&range, game->cursor_cell_x, game->cursor_cell_y) &&
        give_cell_has_player(game, game->cursor_cell_x, game->cursor_cell_y);
    fd2_field_path_close(&range);
    return legal;
}

int fd2_field_game_cycle_item_give_target(fd2_field_game *game) {
    if (!give_source_is_valid(game) || game->interaction != FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING)
        return -1;
    return cycle_target_selection(game);
}


static int give_recipient_is_valid(const fd2_field_game *game, size_t recipient) {
    if (recipient >= game->units.count || recipient == (size_t)game->selected_unit) return 0;
    const fd2_field_unit *source = &game->units.items[game->selected_unit];
    const fd2_field_unit *target = &game->units.items[recipient];
    int distance = abs((int)source->x - target->x) + abs((int)source->y - target->y);
    /* 12c0d在115b6之后另取第一raw可见actor；叠格时不能重筛side3目标。 */
    return !fd2_field_unit_is_hidden(target) && target->x < game->map.width && target->y < game->map.height &&
        distance == 1 && give_cell_has_player(game, target->x, target->y) &&
        fd2_field_game_unit_at(game, target->x, target->y) == (int)recipient && inventory_prefix_is_valid(target);
}

int fd2_field_game_item_give_list_step(fd2_field_game *game, fd2_field_item_list_key key) {
    if (!give_source_is_valid(game) || game->interaction != FD2_FIELD_INTERACTION_ITEM_GIVE_LIST ||
        game->item_give_recipient < 0 || !give_recipient_is_valid(game, (size_t)game->item_give_recipient) ||
        fd2_field_unit_inventory_count(&game->units.items[game->item_give_recipient]) != 8u) return -2;
    return fd2_field_item_list_select_step(&game->units.items[game->item_give_recipient],
        &game->item_give_selected, key, 0);
}

int fd2_field_game_resolve_item_give(fd2_field_game *game) {
    if (!give_source_is_valid(game) || !target_focus_is_valid(game)) return -1;
    int receiver;
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING) {
        if (fd2_field_game_item_give_target_is_legal(game) != 1) return -1;
        receiver = fd2_field_game_unit_at(game, game->cursor_cell_x, game->cursor_cell_y);
    } else if (game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_LIST) {
        if (fd2_field_game_item_give_list_step(game, FD2_FIELD_ITEM_LIST_KEY_OTHER) != 0) return -1;
        receiver = game->item_give_recipient;
    } else return -1;
    if (receiver < 0 || !give_recipient_is_valid(game, (size_t)receiver)) return -1;
    fd2_field_unit sender = game->units.items[game->selected_unit];
    fd2_field_unit recipient = game->units.items[receiver];
    if (sender.x >= game->map.width || sender.y >= game->map.height ||
        fd2_field_unit_combat_stats_recompute(&sender, fd2_field_unit_original_item_stats, NULL,
            fd2_field_unit_original_status_scale, NULL) != 0) return -1;
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING &&
        fd2_field_unit_inventory_count(&recipient) == 8u) {
        /* 原版先恢复Give前focus，再打开recipient的mode0列表。 */
        if (focus_to(game, sender.x, sender.y) != 0) return -1;
        if(game->runtime_host_quit)return 0;
        close_target_range(game);
        game->item_give_recipient = receiver;
        game->item_give_selected = 0;
        game->interaction = FD2_FIELD_INTERACTION_ITEM_GIVE_LIST;
        return 0;
    }
    const uint8_t *source_raw = (const uint8_t *)&sender;
    uint8_t source_id = source_raw[0x0b + (size_t)game->item_list_selected * 2u];
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_LIST) {
        uint8_t received_id = ((const uint8_t *)&recipient)[0x0b + (size_t)game->item_give_selected * 2u];
        inventory_slot_remove(&recipient, (size_t)game->item_give_selected);
        if (inventory_first_empty_add(&recipient, source_id) != 1) return -1;
        inventory_slot_remove(&sender, (size_t)game->item_list_selected);
        if (inventory_first_empty_add(&sender, received_id) != 1) return -1;
    } else {
        if (inventory_first_empty_add(&recipient, source_id) != 1) return -1;
        inventory_slot_remove(&sender, (size_t)game->item_list_selected);
    }
    /* 1bbdc只重算sender；recipient装备被交换时亦保留其原派生，不擅自修整。 */
    if (!inventory_prefix_is_valid(&sender) || !inventory_prefix_is_valid(&recipient) ||
        fd2_field_unit_combat_stats_recompute(&sender, fd2_field_unit_original_item_stats, NULL,
            fd2_field_unit_original_status_scale, NULL) != 0) return -1;
    if (focus_to(game, sender.x, sender.y) != 0) return -1;
    if(game->runtime_host_quit)return 0;
    game->units.items[game->selected_unit] = sender;
    game->units.items[receiver] = recipient;
    game->item_transfer_committed = 1;
    close_target_range(game);
    return return_item_menu_or_command(game);
}

static int finish_item_give_cancel(fd2_field_game *game, int restore_focus) {
    if (!give_source_is_valid(game)) return -1;
    fd2_field_unit sender = game->units.items[game->selected_unit];
    if (fd2_field_unit_combat_stats_recompute(&sender, fd2_field_unit_original_item_stats, NULL,
            fd2_field_unit_original_status_scale, NULL) != 0) return -1;
    if (restore_focus) {
        if (!target_focus_is_valid(game) || focus_to(game, sender.x, sender.y) != 0) return -1;
        if(game->runtime_host_quit)return 0;
        close_target_range(game);
    }
    game->units.items[game->selected_unit] = sender;
    return return_item_menu_or_command(game);
}

int fd2_field_game_cancel_item_give(fd2_field_game *game) {
    if (!game || (game->interaction != FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING &&
        game->interaction != FD2_FIELD_INTERACTION_ITEM_GIVE_LIST)) return -1;
    return finish_item_give_cancel(game, 1);
}
static int player_basic_magic_list_is_supported(const fd2_field_game *game) {
    if (!magic_list_actor_is_valid(game) || !fd2_field_stage_battle_is_supported(game->stage)) return 0;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    uint8_t ids[40];
    size_t count = fd2_field_unit_magic_list_build(actor, ids, sizeof(ids));
    if (!count || count > FD2_FIELD_MAGIC_LIST_MAX_COUNT) return 0;
    int supported = 0;
    for (size_t i = 0; i < count; i++) {
        if (!fd2_field_magic_record_get(ids[i])) return 0;
        if (player_magic_entry_is_supported(actor, ids[i])) supported = 1;
    }
    return supported; /* 按真实技能记录与各入口有效域判断，不因单项无效禁用整表。 */
}

int fd2_field_game_magic_is_available(const fd2_field_game *game) {
    return game && game->attack_rng && game->interaction == FD2_FIELD_INTERACTION_COMMAND &&
        player_basic_magic_list_is_supported(game);
}

static int player_effect_tail_is_supported(const fd2_field_game *game,
                                            size_t actor_index,
                                            const uint8_t *healed_targets,
                                            size_t healed_count,
                                            int clears_paralysis,
                                            int clears_acted,
                                            int final_x, int final_y) {
    uint8_t action;
    size_t slot;
    int found = fd2_field_cell_event_find(&game->map, &game->terrain,
        &game->metadata, final_x, final_y, 1, &action, &slot);
    /* 最终match1覆盖旧pending；只放开已恢复stage-private handler。 */
    if (found < 0) return 0;
    int have_pending = found || game->pending_cell_event.valid;
    uint8_t pending = found ? action : game->pending_cell_event.action;
    if(have_pending) {
        const fd2_static_flow *flow=known_common_event_flow(game,pending);
        if(!flow || common_event_preflight_flow(game,flow,actor_index)!=0)return 0;
    }
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        int affected = 0;
        for (size_t t = 0; t < healed_count; t++) if (healed_targets[t] == i) affected = 1;
        int healed = affected && !clears_acted;
        if (i != actor_index && unit->side == 2 &&
            (unit->flags & (clears_acted && affected ? 0x01u : 0x81u)) == 0 &&
            (!unit->detail_status[1] || (clears_paralysis && healed))) {
            /* 预测本次finalize后的flags，不改13565本体的无HP门契约。
             * 治疗同格HP0已通过maxHP预检，必先恢复存活；其他HP0将隐藏。 */
            if (healed && !unit->hp_max) continue;
            if (!unit->hp && !(healed && unit->hp_max)) continue;
            return 1;
        }
    }
    /* 最后一名玩家会立即进入side1；按真实raw0投影callback身份/脚本，
     * 不以日志容量或捕获人数代替合法性。执行后的晚错仍保留已提交事实。 */
    fd2_field_turn_event_match matches[FD2_FIELD_EVENT_SLOTS];
    size_t count = fd2_field_turn_events_find(&game->metadata,
        game->turn_number, 1, matches, FD2_FIELD_EVENT_SLOTS);
    if (count > FD2_FIELD_EVENT_SLOTS || game->event_log_count > FD2_FIELD_EVENT_LOG_CAPACITY)
        return 0;
    for(size_t i=0;i<count;i++) {
        const fd2_static_flow *flow=known_common_event_flow(game,matches[i].action);
        if(!flow || common_event_preflight_flow(game,flow,0)!=0)return 0;
    }
    return 1;
}

/* field_magic_battle_animation @0x2ff01（原FUN_00055115）：一cast同步owner。
 * FX自身RNG、effect、HP插值与jitter按原调用点交错；present只重放片段。
 * 每片段最多prefix6或(12+9)*6个FX随机值，512留有明确余量；不存跨回合队列。 */
#define FD2_FULL_MAGIC_FX_RANDOM_CAPACITY 512u
typedef struct {
    fd2_field_game *game;
    fd2_magic_fx state;
    fd2_magic_fx_parameters parameters;
    uint8_t durations[256];
    uint32_t random[FD2_FULL_MAGIC_FX_RANDOM_CAPACITY];
    size_t random_count;
    int random_error;
} fd2_full_magic_cast;

static uint32_t full_magic_fx_random(void *userdata) {
    fd2_full_magic_cast *cast = userdata;
    if (cast->random_count >= FD2_FULL_MAGIC_FX_RANDOM_CAPACITY) {
        cast->random_error = 1;
        return 0;
    }
    uint32_t value = cast->game->attack_rng(cast->game->attack_rng_userdata);
    cast->random[cast->random_count++] = value;
    return value;
}

static int full_magic_fx_step(fd2_full_magic_cast *cast, uint8_t phase, int *result) {
    int ignored;
    if (!result) result = &ignored;
    return fd2_magic_fx_step(&cast->state, phase, full_magic_fx_random, cast,
        NULL, NULL, result) != 0 || cast->random_error ? -1 : 0;
}

static void full_magic_segment_begin(fd2_full_magic_cast *cast) {
    cast->random_count = 0;
    cast->random_error = 0;
}

static int full_magic_prepare(fd2_full_magic_cast *cast, fd2_field_game *game,
    const fd2_field_unit *actor, uint8_t id, uint8_t *cost_count) {
    if (!cast || !game || !actor || !cost_count || !game->fdother) return -1;
    cast->game = game;
    fd2_figani_magic_cast caster;
    fd2_figani_clip effect;
    if (fd2_figani_magic_cast_plan(&caster, &game->figani_archive, actor->unit_id) != 0 ||
        caster.clip.empty || fd2_magic_fx_parameters_get(id, actor->side, &cast->parameters) != 0 ||
        fd2_figani_open_entry(&effect, game->fdother, cast->parameters.effect_entry) != 0 ||
        effect.empty || effect.part_count < cast->parameters.minimum_part_count) return -1;
    for (size_t i = 0; i < effect.part_count; i++) {
        fd2_figani_part part;
        if (fd2_figani_get_part(&effect, i, &part) != 0) return -1;
        cast->durations[i] = part.duration;
    }
    fd2_magic_fx_parts parts = {cast->durations, effect.part_count};
    if (fd2_magic_fx_init(&cast->state, id, actor->side, parts) != 0) return -1;
    *cost_count = caster.cost_marker_count;
    return 0;
}

static int full_magic_present(fd2_full_magic_cast *cast,
    fd2_field_magic_present_event *event) {
    event->fx_random = cast->random;
    event->fx_random_count = cast->random_count;
    return !cast->game->magic_present ? 0 : cast->game->magic_present(
        cast->game, event, cast->game->magic_present_userdata);
}

/* 0/3/6各返回一次for次数；1/2、4/5、7/8各配对，不按while猜循环。 */
static int full_magic_repeated_pair(fd2_full_magic_cast *cast,
    uint8_t start, uint8_t first, uint8_t second) {
    int count;
    if (full_magic_fx_step(cast, start, &count) != 0 || count < 0 || count > 64) return -1;
    for (int i = 0; i < count; i++)
        if (full_magic_fx_step(cast, first, NULL) != 0 ||
            full_magic_fx_step(cast, second, NULL) != 0) return -1;
    return 0;
}

static int execute_full_magic(fd2_field_game *game, size_t actor_index, uint8_t id,
    const uint8_t *targets, size_t count, int clear_contribution) {
    /* FULL9的共享BYTE counter跨target保留；31part以外仅图形读无定义，
     * magic_fx按已证安全扩展略该DRAW，仍保留每target core/cue/计数/RNG。 */
    fd2_full_magic_cast cast = {0};
    fd2_field_unit *actor = &game->units.items[actor_index];
    fd2_field_unit cast_before = *actor;
    fd2_field_magic_present_event event = {0};
    event.actor_index = actor_index; event.magic_id = id;
    event.targets = targets; event.target_count = count; event.actor_before = &cast_before;
    if (full_magic_prepare(&cast, game, actor, id, &event.cast_cost_count) != 0) {
        fail_action(game, actor_index);
        return -1;
    }
    event.phase = FD2_FIELD_MAGIC_PRESENT_PREPARE;
    if (full_magic_present(&cast, &event) != 0) return -1;
    game->ordinary_display_mode = 0;
    const uint8_t *record = fd2_field_magic_record_get(id);
    actor->mp = (uint16_t)(actor->mp - (unsigned)record[5] * event.cast_cost_count);
    if (clear_contribution) game->experience_value = 0;
    full_magic_segment_begin(&cast);
    if (full_magic_repeated_pair(&cast, 0, 1, 2) != 0) goto failed;
    event.phase = FD2_FIELD_MAGIC_PRESENT_CAST;
    if (full_magic_present(&cast, &event) != 0) goto failed;
    for (size_t t = 0; t < count; t++) {
        full_magic_segment_begin(&cast);
        fd2_field_unit before_actor = *actor;
        fd2_field_unit before_target = game->units.items[targets[t]];
        fd2_field_unit *target = &game->units.items[targets[t]];
        fd2_field_magic_basic_result effect;
        int frames;
        if (full_magic_fx_step(&cast, 3, &frames) != 0 || frames < 0 || frames > 64 ||
            fd2_field_magic_apply_damage_result(id, target, game->attack_rng,
                game->attack_rng_userdata, &effect) < 0) goto failed;
        game->experience_value += effect.experience_value; /* 原DWORD ADD，AI不造caster归一。 */
        /* 原306xx保存effect HP后恢复before；FX4/5内部RNG也须看见这个写序。 */
        target->hp = effect.hp_before;
        event.impact_count = 0;
        unsigned hp_step = 1;
        for (int frame = 0; frame < frames; frame++) {
            int impact;
            if (full_magic_fx_step(&cast, 4, NULL) != 0 ||
                full_magic_fx_step(&cast, 5, &impact) != 0) goto failed;
            if (effect.raw_damage && impact == 1) {
                if (hp_step <= cast.parameters.hp_denominator) {
                    int32_t before = (int16_t)effect.hp_before, after = (int16_t)effect.hp_after;
                    target->hp = (uint16_t)(before - (int32_t)hp_step *
                        (before - after) / cast.parameters.hp_denominator);
                    hp_step++;
                }
                if (event.impact_count >= FD2_FIELD_MAGIC_IMPACT_CAPACITY) goto failed;
                event.impact_jitter[event.impact_count++] = (int8_t)(1 -
                    (int)(game->attack_rng(game->attack_rng_userdata) % 3u));
            }
        }
        /* 原31266九tick继续同一FX state；忽略返回值，不写HP/不取jitter。 */
        if (t + 1u < count) for (int frame = 0; frame < 9; frame++)
            if (full_magic_fx_step(&cast, 4, NULL) != 0 ||
                full_magic_fx_step(&cast, 5, NULL) != 0) goto failed;
        event.phase = FD2_FIELD_MAGIC_PRESENT_TARGET; event.target_index = targets[t];
        event.actor_before = &before_actor; event.target_before = &before_target; event.result = &effect;
        event.target_after = target;
        if (full_magic_present(&cast, &event) != 0) goto failed;
    }
    full_magic_segment_begin(&cast);
    if (full_magic_repeated_pair(&cast, 6, 7, 8) != 0) goto failed;
    event.phase = FD2_FIELD_MAGIC_PRESENT_FINISH;
    event.actor_before = event.target_before = event.target_after = NULL; event.result = NULL; event.impact_count = 0;
    if (full_magic_present(&cast, &event) != 0) { fail_action(game, actor_index); return -1; }
    return 0;
failed:
    fail_action(game, actor_index);
    event.phase = FD2_FIELD_MAGIC_PRESENT_FINISH; event.action_failed = 1;
    event.actor_before = event.target_before = event.target_after = NULL; event.result = NULL;
    event.fx_random = NULL; event.fx_random_count = 0; event.impact_count = 0;
    if (game->magic_present) (void)game->magic_present(game, &event, game->magic_present_userdata);
    return -1;
}

/* 原2185f/21a9e→21548→1c7c6的三个C11同步HOST输出点：
 * 普通MAP10..12仅0继续/skip、1宿主停止、
 * -1真实错误。Game唯一锁存；其它族/phase的旧非零错误合同不扩张。 */
static int map_c11_present(fd2_field_game *game,fd2_field_magic_present_event *event) {
    int result=game->magic_present ? game->magic_present(game,event,game->magic_present_userdata):0;
    int c11=event->presentation_mode==FD2_FIELD_MAGIC_MODE_MAP &&
        event->action_kind==FD2_FIELD_MAGIC_ACTION_MAGIC && event->magic_id>=10u && event->magic_id<=12u &&
        event->effect_id==event->magic_id &&
        (event->phase==FD2_FIELD_MAGIC_PRESENT_MAP_WRAPPER_PRELUDE ||
         event->phase==FD2_FIELD_MAGIC_PRESENT_CAST || event->phase==FD2_FIELD_MAGIC_PRESENT_TARGET);
    if(!c11)return result!=0 ? -1:0;
    if(result==1)game->runtime_host_quit=1;
    if(result<0 || result>1 || game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
    return game->runtime_host_quit?1:0;
}

/* 28槽地图wrapper的共享effect提交：真实目标raw顺序，present只读结果。
 * 普通费用一次，17读18；9只用首目标，24表内别名16（玩家24另走2cf30）。 */
static int magic_effect_group(fd2_field_game *game, fd2_field_magic_present_event *event,
        uint8_t effect_id, int32_t power_override) {
    event->effect_id = effect_id;
    if (event->presentation_mode == FD2_FIELD_MAGIC_MODE_COMPOSITE) {
        event->phase = FD2_FIELD_MAGIC_PRESENT_CAST;
        if (game->magic_present && game->magic_present(game, event,
                game->magic_present_userdata) != 0) return -1;
    }
    size_t count = event->effect_target_count ? event->effect_target_count : event->target_count;
    for (size_t i = 0; i < count; i++) {
        size_t raw = event->targets[i];
        fd2_field_unit before = game->units.items[raw];
        fd2_field_magic_basic_result effect;
        if (fd2_field_magic_apply_effect_result(effect_id, (uint8_t)raw, &game->units,
                power_override, game->attack_rng, game->attack_rng_userdata,
                &game->experience_value, &effect) < 0) return -1;
        event->phase = FD2_FIELD_MAGIC_PRESENT_TARGET; event->target_index = raw;
        event->target_before = &before; event->target_after = &game->units.items[raw];
        event->result = &effect;
        int presented=map_c11_present(game,event);
        event->target_before = event->target_after = NULL;
        event->result = NULL;
        if(presented)return presented; /* 当前target已提交，HOST不得进入下一目标。 */
    }
    event->target_before = event->target_after = NULL;
    event->result = NULL;
    return 0;
}

static int magic_action_flow(fd2_field_game *game, fd2_field_magic_present_event *event) {
    if(game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
    if(game->runtime_host_quit)return 0;
    char name[32];
    (void)snprintf(name, sizeof(name), "magic.effect.%03u", event->magic_id);
    const fd2_static_flow *flow = fd2_static_flow_lookup("magic", name);
    if (!flow) return -1;
    int pc = flow->entry;
    for (unsigned budget = 0; budget < FD2_FLOW_STEP_BUDGET; budget++) {
        const fd2_static_flow_node *node = fd2_static_flow_at(flow, pc);
        if (!node) return -1;
        int result = 0;
        if (!strcmp(node->op, "done")) return 0;
        if (!strcmp(node->op, "fail")) return -1;
        if (!strcmp(node->op, "effect_group")) {
            int id = node->args[0] < 0 ? event->magic_id : node->args[0];
            if (id < 0 || id >= (int)FD2_FIELD_MAGIC_COUNT) return -1;
            result = magic_effect_group(game, event, (uint8_t)id, node->args[1]);
            event->group_pass++;
        } else if (!strcmp(node->op, "status_clear_group")) {
            if (node->args[0] < 37 || node->args[0] > 39 || node->args[1] < 1 ||
                node->args[0] + node->args[1] > 40) return -1;
            for (size_t i = 0; i < event->target_count; i++)
                memset((uint8_t *)&game->units.items[event->targets[i]] + node->args[0],
                    0, (size_t)node->args[1]);
        } else if (!strcmp(node->op, "popup_clear")) {
            event->phase = FD2_FIELD_MAGIC_PRESENT_POPUP_CLEAR;
            if (game->magic_present) result = game->magic_present(game, event, game->magic_present_userdata);
        } else if (!strcmp(node->op, "teleport_focus")) {
            if (event->magic_id != 23u || node->args[0] < 0 || node->args[0] > 1 ||
                !event->target_before) return -1;
            result = focus_to(game, node->args[0] ? event->destination_x : event->target_before->x,
                node->args[0] ? event->destination_y : event->target_before->y);
        } else if (!strcmp(node->op, "teleport_cost_reward")) {
            if (event->magic_id != 23u || event->target_index >= game->units.count) return -1;
            fd2_field_unit *caster = &game->units.items[event->actor_index];
            const fd2_field_unit *target = &game->units.items[event->target_index];
            caster->mp = (uint16_t)(caster->mp - fd2_field_magic_record_get(23)[5]);
            game->experience_value += 10u * (target->level +
                (target->movement_profile > 8u && target->movement_profile < 25u ? 30u : 0u));
        } else if (!strcmp(node->op, "teleport_present")) {
            static const fd2_field_magic_present_phase phases[] = {
                FD2_FIELD_MAGIC_PRESENT_TELEPORT_OUT, FD2_FIELD_MAGIC_PRESENT_TELEPORT_OUT_COMMIT,
                FD2_FIELD_MAGIC_PRESENT_TELEPORT_IN, FD2_FIELD_MAGIC_PRESENT_TELEPORT_IN_COMMIT};
            if (event->magic_id != 23u || node->args[0] < 0 || node->args[0] > 3) return -1;
            event->phase = phases[node->args[0]];
            if (game->magic_present) result = game->magic_present(game, event, game->magic_present_userdata);
        } else if (!strcmp(node->op, "teleport_position")) {
            if (event->magic_id != 23u || event->target_index >= game->units.count ||
                node->args[0] < 0 || node->args[0] > 1) return -1;
            fd2_field_unit *target = &game->units.items[event->target_index];
            target->x = node->args[0] ? (uint8_t)event->destination_x : 255;
            target->y = node->args[0] ? (uint8_t)event->destination_y : 255;
        } else if (!strcmp(node->op, "display_mode")) {
            if (node->args[0] < 0 || node->args[0] > 1) return -1;
            game->ordinary_display_mode = (uint8_t)node->args[0];
        } else return -1;
        if(result<0 || game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
        if(game->runtime_host_quit)return 0;
        pc = fd2_static_flow_edge(flow, node, result ? FD2_FLOW_ERROR : FD2_FLOW_NEXT);
        if (pc < 0) return -1;
    }
    return -1;
}

/* field_player_magic_command_execute @0x1cff0（原登记同名）的
 * 1d3f3/1d3ff→1d459共同尾：两个selector都成功后先mode0/refresh，
 * 再1d6c8；handler费用尚未提交。独立呈现叶不建立跨selector owner。 */
static int player_map_caller_preamble(fd2_field_game *game,size_t actor,uint8_t id) {
    if(!game->magic_present)return 0; /* 无呈现诊断不合成额外等待/业务状态。 */
    game->ordinary_display_mode=0;
    int result=turn_present(game,FD2_FIELD_RUNTIME_REFRESH,0);
    if(result<0 || game->failure!=FD2_FIELD_FAILURE_NONE) {
        if(game->failure==FD2_FIELD_FAILURE_NONE)fail_action(game,actor);
        return -1;
    }
    if(result>0 || game->runtime_host_quit){game->runtime_host_quit=1;return 1;}
    fd2_field_magic_present_event event={0};
    event.phase=FD2_FIELD_MAGIC_PRESENT_MAP_CALLER_PREAMBLE;
    event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;event.action_kind=FD2_FIELD_MAGIC_ACTION_MAGIC;
    event.actor_index=actor;event.magic_id=id;event.effect_id=id;
    event.actor_before=&game->units.items[actor];
    result=game->magic_present(game,&event,game->magic_present_userdata);
    if(result>0)game->runtime_host_quit=1;
    if(result<0 || game->failure!=FD2_FIELD_FAILURE_NONE) {
        if(game->failure==FD2_FIELD_FAILURE_NONE)fail_action(game,actor);
        return -1;
    }
    if(result>0 || game->runtime_host_quit){game->runtime_host_quit=1;return 1;}
    return 0;
}

static int execute_map_magic(fd2_field_game *game, size_t actor_index,
    uint8_t id, const uint8_t *targets, size_t count, int clear_contribution) {
    fd2_field_magic_map_profile profile;
    if (fd2_field_magic_map_profile_get(id, &profile) != 0) return -1;
    fd2_field_unit *actor = &game->units.items[actor_index];
    fd2_field_unit cast_before = *actor;
    fd2_field_magic_present_event event = {0};
    event.phase = FD2_FIELD_MAGIC_PRESENT_PREPARE; event.actor_index = actor_index;
    event.magic_id = id; event.cast_cost_count = 1;
    event.presentation_mode = FD2_FIELD_MAGIC_MODE_MAP;
    event.effect_id = profile.effect_id;
    event.targets = targets; event.target_count = count; event.actor_before = &cast_before;
    event.effect_target_count = profile.target_limit && count > profile.target_limit ? profile.target_limit : count;
    if(clear_contribution) {
        int preamble=player_map_caller_preamble(game,actor_index,id);
        if(preamble)return preamble<0?-1:0;
    }
    if (game->magic_present && game->magic_present(game, &event,
            game->magic_present_userdata) != 0) return -1;
    game->ordinary_display_mode = 0;
    if (clear_contribution) game->experience_value = 0;
    /* 2185f/21a9e的2189a先于共享21548@215a2费用写点；
     * presenter只读，Game在radial返回后、quake CAST前恰扣一次。 */
    if(id==11u || id==12u) {
        event.phase=FD2_FIELD_MAGIC_PRESENT_MAP_WRAPPER_PRELUDE;
        int presented=map_c11_present(game,&event);
        if(presented<0)goto failed;
        if(presented>0)goto host_stopped;
    }
    if (profile.cost_before_cast)
        actor->mp = (uint16_t)(actor->mp - fd2_field_magic_record_get(profile.cost_id)[5]);
    event.phase = FD2_FIELD_MAGIC_PRESENT_CAST;
    int presented=map_c11_present(game,&event);
    if(presented<0)goto failed;
    if(presented>0)goto host_stopped;
    if (!profile.cost_before_cast)
        actor->mp = (uint16_t)(actor->mp - fd2_field_magic_record_get(profile.cost_id)[5]);
    presented=magic_effect_group(game,&event,event.effect_id,-1);
    if(presented<0)goto failed;
    if(presented>0)goto host_stopped;
    event.phase = FD2_FIELD_MAGIC_PRESENT_FINISH;
    event.actor_before = event.target_before = NULL; event.result = NULL;
    if (game->magic_present && game->magic_present(game, &event,
            game->magic_present_userdata) != 0) { fail_action(game, actor_index); return -1; }
    return 0;
host_stopped:
    /* HOST保当前MP/HP/RNG前缀；FINISH正常释放exchange，不走业务成功尾。
     * 真实cleanup错误仍优先分类，不能借HOST抹掉第一资源错误。 */
    event.phase=FD2_FIELD_MAGIC_PRESENT_FINISH;
    event.actor_before=event.target_before=event.target_after=NULL;event.result=NULL;
    if(game->magic_present && game->magic_present(game,&event,game->magic_present_userdata)!=0) {
        fail_action(game,actor_index);return -1;
    }
    if(game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
    return 0;
failed:
    fail_action(game, actor_index);
    event.phase = FD2_FIELD_MAGIC_PRESENT_FINISH; event.action_failed = 1;
    event.actor_before = event.target_before = NULL; event.result = NULL;
    if (game->magic_present) (void)game->magic_present(game, &event, game->magic_present_userdata);
    return -1;
}

/* field_derived_magic_full_execute @0x2cf30（原sub_2CF30）／
 * field_composite_magic_full_execute @0x2d80d（原sub_2D80D）：
 * 沿原FIGANI选择策略及intro marker费用，保持Game唯一owner。
 * 复合演出先回地图，再执行冷图；派生伤害逐part写HP，duration0也有marker。 */
static int execute_special_magic(fd2_field_game *game, size_t actor_index,
        uint8_t id, const uint8_t *targets, size_t count) {
    fd2_figani_magic_cast plan;
    if (fd2_figani_magic_action_plan(&plan, &game->figani_archive,
            game->units.items[actor_index].unit_id, id) != 0 || plan.clip.empty) return -1;
    fd2_field_unit *actor = &game->units.items[actor_index], actor_before = *actor;
    fd2_field_magic_present_event event = {0};
    event.magic_id = event.effect_id = id; event.actor_index = actor_index;
    event.presentation_mode = id < 32u ? FD2_FIELD_MAGIC_MODE_DERIVED : FD2_FIELD_MAGIC_MODE_COMPOSITE;
    event.targets = targets; event.target_count = count; event.actor_before = &actor_before;
    event.cast_cost_count = plan.cost_marker_count;
    event.group_pass = id >= 32u ? UINT8_MAX : 0;
    event.phase = FD2_FIELD_MAGIC_PRESENT_PREPARE;
    if (game->magic_present && game->magic_present(game, &event, game->magic_present_userdata) != 0) return -1;
    game->ordinary_display_mode = 0;
    game->experience_value = 0;
    actor->mp = (uint16_t)(actor->mp - (unsigned)fd2_field_magic_record_get(id)[5] * plan.cost_marker_count);
    event.phase = FD2_FIELD_MAGIC_PRESENT_CAST;
    if (game->magic_present && game->magic_present(game, &event, game->magic_present_userdata) != 0) goto failed;
    if (id >= 32u) {
        event.group_pass = 0;
        event.phase = FD2_FIELD_MAGIC_PRESENT_POPUP_CLEAR;
        if (game->magic_present && game->magic_present(game, &event, game->magic_present_userdata) != 0) goto failed;
        if (magic_action_flow(game, &event) != 0) goto failed;
    } else {
        for (size_t i = 0; i < count; i++) {
            fd2_field_unit *target = &game->units.items[targets[i]], before = *target;
            fd2_field_magic_basic_result effect;
            if (fd2_field_magic_apply_derived_result(id, actor, target, game->attack_rng,
                    game->attack_rng_userdata, &game->experience_value, &effect) < 0) goto failed;
            target->hp = before.hp;
            event.target_index = targets[i]; event.target_before = &before;
            event.target_after = target; event.result = &effect; event.marker_count = 0;
            for (size_t part = plan.clip.intro_end; part < plan.clip.part_count; part++) {
                fd2_figani_part definition;
                if (fd2_figani_get_part(&plan.clip, part, &definition) != 0) goto failed;
                if (definition.effect_marker == 1u) {
                    event.marker_count++;
                    int32_t hp = (int16_t)before.hp - effect.signed_damage * event.marker_count / (id == 28u ? 8 : 1);
                    target->hp = (uint16_t)hp;
                }
                event.phase = FD2_FIELD_MAGIC_PRESENT_STEP; event.part_index = (uint16_t)part;
                if (game->magic_present && game->magic_present(game, &event, game->magic_present_userdata) != 0) goto failed;
            }
            event.phase = FD2_FIELD_MAGIC_PRESENT_TARGET;
            if (game->magic_present && game->magic_present(game, &event, game->magic_present_userdata) != 0) goto failed;
        }
    }
    event.phase = FD2_FIELD_MAGIC_PRESENT_FINISH; event.result = NULL;
    event.target_before = event.target_after = NULL; event.actor_before = NULL;
    if (game->magic_present && game->magic_present(game, &event, game->magic_present_userdata) != 0) goto failed;
    return 0;
failed:
    fail_action(game, actor_index);
    event.phase = FD2_FIELD_MAGIC_PRESENT_FINISH; event.action_failed = 1;
    event.target_before = event.target_after = NULL; event.actor_before = NULL; event.result = NULL;
    if (game->magic_present) (void)game->magic_present(game, &event, game->magic_present_userdata);
    return -1;
}

static int magic_targets_preflight(const fd2_field_game *game, size_t actor_index,
        uint8_t id, const uint8_t *targets, size_t count) {
    if (!count || count > FD2_FIELD_MAX_UNITS) return -1;
    for (size_t i = 0; i < count; i++) if (targets[i] >= game->units.count) return -1;
    if (id == 23u) return 0;
    if (id == 24u || (id >= 28u && id <= 31u)) {
        for (size_t i = 0; i < count; i++)
            if (fd2_field_magic_derived_preflight(id, &game->units.items[actor_index],
                    &game->units.items[targets[i]]) != 0) return -1;
        return 0;
    }
    const fd2_static_flow *flow = NULL;
    if (id >= 32u) {
        char name[32]; (void)snprintf(name, sizeof(name), "magic.effect.%03u", id);
        flow = fd2_static_flow_lookup("magic", name);
        if (!flow) return -1;
    }
    if (!flow) {
        for (size_t i = 0; i < count; i++)
            if (fd2_field_magic_effect_reward_preflight(id, &game->units.items[targets[i]]) != 0) return -1;
        return 0;
    }
    int pc = flow->entry;
    for (unsigned budget = 0; budget < FD2_FLOW_STEP_BUDGET; budget++) {
        const fd2_static_flow_node *node = fd2_static_flow_at(flow, pc);
        if (!node) return -1;
        if (!strcmp(node->op, "done")) return 0;
        if (!strcmp(node->op, "effect_group")) {
            int effect = node->args[0] < 0 ? id : node->args[0];
            if (effect < 0 || effect >= (int)FD2_FIELD_MAGIC_COUNT) return -1;
            for (size_t i = 0; i < count; i++)
                if (fd2_field_magic_effect_reward_preflight((uint8_t)effect,
                        &game->units.items[targets[i]]) != 0) return -1;
        } else if (strcmp(node->op, "status_clear_group") && strcmp(node->op, "popup_clear")) return -1;
        pc = fd2_static_flow_edge(flow, node, FD2_FLOW_NEXT);
    }
    return -1;
}

/* 2218a/22253双段传送。坐标255仅存在于两段之间的Game热状态；
 * renderer分别消费原位置/目的地投影，不把255当可见地图坐标。 */
static int execute_teleport_magic(fd2_field_game *game, size_t actor_index, uint8_t target_index) {
    fd2_field_unit *actor = &game->units.items[actor_index], actor_before = *actor;
    fd2_field_unit *target = &game->units.items[target_index], before = *target;
    int x = game->cursor_cell_x, y = game->cursor_cell_y;
    fd2_field_magic_present_event event = {0};
    event.magic_id = event.effect_id = 23; event.presentation_mode = FD2_FIELD_MAGIC_MODE_MAP;
    event.actor_index = actor_index; event.actor_before = &actor_before;
    event.targets = &target_index; event.target_count = 1; event.target_index = target_index;
    event.target_before = &before; event.target_after = target;
    event.destination_x = x; event.destination_y = y; event.cast_cost_count = 1;
    /* 1d1da/1d238两次selector均成功才到1d459；取消目的地不闪光。 */
    int preamble=player_map_caller_preamble(game,actor_index,23);
    if(preamble)return preamble<0?-1:0;
    event.phase = FD2_FIELD_MAGIC_PRESENT_PREPARE;
    if (game->magic_present && game->magic_present(game, &event, game->magic_present_userdata) != 0) return -1;
    if (focus_to(game, actor->x, actor->y) != 0) return -1;
    if(game->runtime_host_quit)return 0;
    game->ordinary_display_mode = 0;
    game->experience_value = 0;
    if (magic_action_flow(game, &event) != 0) goto failed;
    if(game->runtime_host_quit)return 0;
    event.phase = FD2_FIELD_MAGIC_PRESENT_FINISH;
    if (game->magic_present && game->magic_present(game, &event, game->magic_present_userdata) != 0) goto failed;
    return 0;
failed:
    fail_action(game, actor_index); event.phase = FD2_FIELD_MAGIC_PRESENT_FINISH; event.action_failed = 1;
    if (game->magic_present) (void)game->magic_present(game, &event, game->magic_present_userdata);
    return -1;
}

int fd2_field_game_resolve_player_magic(fd2_field_game *game) {
    if (!magic_target_context_is_valid(game) || !game->attack_rng ||
        !player_basic_magic_list_is_supported(game)) return -1;
    size_t actor_index = (size_t)game->selected_unit;
    fd2_field_unit *actor = &game->units.items[actor_index];
    int selected = game->magic_list_selected;
    uint8_t id = 0xff;
    if (fd2_field_magic_list_select_step(actor, &selected,
            FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, &id) != 1 || id != game->magic_target_id)
        return -1;
    if (fd2_field_game_magic_target_is_legal(game) != 1) return -1;
    if (game->interaction != FD2_FIELD_INTERACTION_MAGIC_TELEPORTING) {
        const uint8_t *definition = fd2_field_magic_record_get(id);
        fd2_field_path_result verified = FD2_FIELD_PATH_RESULT_INITIALIZER;
        if (fd2_field_target_range_compute(&verified, &game->map, &game->terrain,
                actor->x, actor->y, id == 23u ? 1u : 0u,
                definition[3] ? definition[3] : definition[4]) != 0) return -1;
        int inside = fd2_field_path_is_destination(&verified, game->cursor_cell_x, game->cursor_cell_y);
        fd2_field_path_close(&verified);
        if (!inside) return -1;
    }
    uint8_t targets[FD2_FIELD_MAX_UNITS];
    size_t count = 0;
    int teleport = game->interaction == FD2_FIELD_INTERACTION_MAGIC_TELEPORTING;
    if (teleport) targets[count++] = game->magic_target_candidates[0];
    else if (collect_player_magic_targets(game, id, targets, &count) != 0) return -1;
    if (!count) return -1;
    if (id == 23u && !teleport) {
        fd2_field_path_close(&game->move_range);
        memcpy(game->magic_target_candidates, targets, count);
        game->magic_target_count = count; game->magic_target_cycle = 0;
        game->interaction = FD2_FIELD_INTERACTION_MAGIC_TELEPORTING;
        return 0;
    }
    fd2_field_magic_map_profile map_profile;
    size_t effect_count = count;
    if (fd2_field_magic_map_profile_get(id, &map_profile) == 0 && map_profile.target_limit &&
        effect_count > map_profile.target_limit) effect_count = map_profile.target_limit;
    if (magic_targets_preflight(game, actor_index, id, targets, effect_count) != 0 ||
        !effect_deaths_are_supported(game, actor_index, targets, effect_count, id, 0, 0)) return -1;
    uint32_t maximum_gain = 99;
    if (id == 0u || id == 13u) {
        int64_t lower = 0, upper = 0;
        for (size_t i = 0; i < count; i++) {
            int32_t low, high;
            if (fd2_field_magic_basic_reward_bounds(id, &game->units.items[targets[i]], &low, &high) != 0) return -1;
            lower += low; upper += high;
        }
        /* 原53ec8为DWORD；区间跨signed断点时不能只把上端回绕成负数。
         * 区间是保守可达界，不消费effect RNG；正/负提示仍各自预检。 */
        int32_t raw_high = (int32_t)(uint32_t)upper;
        if (upper - lower > UINT32_MAX || raw_high < (int32_t)(uint32_t)lower) raw_high = INT32_MAX;
        if (fd2_field_magic_reward_normalize(actor, (uint32_t)raw_high, &maximum_gain) != 0) return -1;
    }
    int healing = (id >= 13u && id <= 16u) || id == 20u || id == 21u || id == 33u;
    uint8_t healed_targets[FD2_FIELD_MAX_UNITS];
    size_t healed_count = 0;
    if (healing) for (size_t i = 0; i < effect_count; i++) {
        const fd2_field_unit *target = &game->units.items[targets[i]];
        /* 20/21无原状态时不调用恢复core；HP0仍会在finalize隐藏。 */
        if ((id != 20u && id != 21u) || target->detail_status[id - 20u])
            healed_targets[healed_count++] = targets[i];
    }
    /* signed贡献的资源分支也先验：-1覆盖旧EX高byte导致的最大负gain升级链，
     * INT32_MIN覆盖最宽负数文本/不升级分支；不消费真实RNG或写live EX。 */
    if ((healing || id == 24u || (id >= 28u && id <= 30u)) &&
        (preflight_reward(game, actor_index, UINT32_MAX) < 0 ||
         preflight_reward(game, actor_index, (uint32_t)INT32_MIN) < 0)) return -1;
    if (
        preflight_reward(game, actor_index, (int32_t)maximum_gain > 99 ? 99u : maximum_gain) < 0 ||
        !player_effect_tail_is_supported(game, actor_index,
            id == 25u ? targets : healed_targets, id == 25u ? effect_count : healed_count,
            id == 21u || id == 33u, id == 25u, actor->x, actor->y)) return -1;
    int effect_result = id < 9u ? execute_full_magic(game, actor_index, id, targets, count, 1) :
        teleport ? execute_teleport_magic(game, actor_index, targets[0]) :
        id == 24u || id >= 28u ? execute_special_magic(game, actor_index, id, targets, count) :
        execute_map_magic(game, actor_index, id, targets, count, 1);
    if (effect_result != 0) return -1;
    if(game->runtime_host_quit)return 0;
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    int loot_result = begin_defeated_loot(game, actor_index, actor_index,
        PLAYER_ACTION_MAGIC, game->experience_value, 0);
    if (loot_result < 0) { fail_action(game, actor_index); return -1; }
    if (loot_result) return 0;
    if (finish_unit_action_internal(game, PLAYER_ACTION_MAGIC, game->experience_value) != 0) {
        fail_action(game, actor_index);
        return -1;
    }
    return 0;

}

/* field_item_use_effect_execute @0x20c6f（原sub_20C6F）：目标收集用14818，确认用14742的Manhattan门，
 * 两者不可合并。未知掉落描述符属于尚未接入的败亡交互，提交前拒绝。 */
static int magic_effect_may_kill(uint8_t id, const fd2_field_unit *unit) {
    if (id <= 12u || id == 32u) {
        const uint8_t *record=fd2_field_magic_record_get(id);uint32_t scale;
        if (id >= 10u && id <= 12u && fd2_field_attack_unit_ignores_terrain_modifier(unit)) return !unit->hp;
        if (fd2_field_magic_damage_profile_scale(unit->movement_profile,&scale)!=0) return -1;
        uint64_t base=(uint64_t)fd2_field_magic_u16(record,0)*scale/10u;
        return unit->hp<=base*9u/10u+base*99u/1000u;
    }
    if ((id>=13u && id<=16u) || id == 33u) return !unit->hp_max;
    if (id == 24u || (id >= 28u && id <= 31u)) return 1;
    if (id==20u || id==21u)
        return unit->detail_status[id-20u] ? !unit->hp_max : !unit->hp;
    if (!unit->hp) return 1;
    if (id == 35u) return unit->hp <= 27u && unit->movement_profile != 25u && unit->movement_profile != 26u;
    if (id==22u || id==26u || id==27u)
        return unit->hp<=9u && unit->movement_profile!=25u && unit->movement_profile!=26u &&
            !unit->detail_status[id==22u?2:id==26u?0:1];
    return 0;
}

static int item_effect_may_kill(const uint8_t *record, const fd2_field_unit *unit) {
    unsigned code=record[0x0d];
    if (code==5u || code==13u) return !unit->hp_max;
    if (code==20u || code==21u || code==24u) {
        uint16_t id=(uint16_t)fd2_field_item_i16(record,0x0e);
        return id>12u?-1:magic_effect_may_kill((uint8_t)id,unit);
    }
    if (code==6u || code==7u || code==14u || code==22u)
        return magic_effect_may_kill(code==6u?20u:code==7u?21u:code==14u?27u:22u,unit);
    return !unit->hp;
}

/* 原1b6b7扫描全表；只阻断可能进入尚未恢复的描述符交互者。 */
static int effect_deaths_are_supported(fd2_field_game *game, size_t recipient,
        const uint8_t *targets,size_t count,uint8_t magic_id,int item_heal, int magic_collector) {
    for(size_t i=0;i<game->units.count;i++) {
        const fd2_field_unit *unit=&game->units.items[i];
        if(fd2_field_unit_is_hidden(unit))continue;
        int target=0;for(size_t j=0;j<count;j++)if(targets[j]==i)target=1;
        int may_die=target?(item_heal?!unit->hp_max:magic_effect_may_kill(magic_id,unit)):!unit->hp;
        if(may_die<0) return 0;
        const uint8_t *d=(const uint8_t*)unit+0x31;
        if(may_die && (!magic_collector || d[0]==3u) && loot_descriptor_preflight(game,recipient,d)!=0)return 0;
    }
    return 1;
}

static int teleport_destination_is_legal(const fd2_field_game *game, size_t raw) {
    if (!target_focus_is_valid(game)) return -1;
    if (raw >= game->units.count) return -1;
    const fd2_field_unit *unit = &game->units.items[raw];
    if (fd2_field_unit_is_hidden(unit) || unit->x >= game->map.width || unit->y >= game->map.height) return -1;
    for (size_t i = 0; i < game->units.count; i++)
        if (i != raw && !fd2_field_unit_is_hidden(&game->units.items[i]) &&
            game->units.items[i].x == game->cursor_cell_x && game->units.items[i].y == game->cursor_cell_y) return 0;
    uint8_t profile_id = unit->unit_id == 28 ? 1 : unit->movement_profile;
    if (fd2_field_attack_unit_ignores_terrain_modifier(unit)) profile_id = 19;
    const uint8_t *profile = fd2_field_movement_profile_get(profile_id);
    uint16_t terrain = fd2_field_cell_terrain(fd2_field_map_cell(&game->map, game->cursor_cell_x, game->cursor_cell_y));
    if (!profile || terrain >= game->terrain.attr_count ||
        game->terrain.attrs[terrain].movement_cost_class >= FD2_FIELD_MOVEMENT_COST_CLASS_COUNT) return -1;
    return profile[game->terrain.attrs[terrain].movement_cost_class] != 20;
}

static int item_teleport_destination_is_legal(const fd2_field_game *game) {
    if (!game->magic_target_count) return -1;
    size_t raw = game->magic_target_candidates[0];
    if (raw >= game->units.count) return -1;
    const uint8_t *record = item_target_record(game);
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    const fd2_field_unit *target = &game->units.items[raw];
    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    if (!record || !target_matches_filter(target, record[0x15]) ||
        fd2_field_target_range_compute(&range, &game->map, &game->terrain,
            actor->x, actor->y, 1, record[0x10]) != 0) return -1;
    int in_cast = fd2_field_path_is_destination(&range, target->x, target->y);
    fd2_field_path_close(&range);
    return in_cast ? teleport_destination_is_legal(game, raw) : -1;
}

/* field_item_permanent_stat_apply @0x21082（原sub_21082）的WORD写集；
 * code19跨EX的临时进位先保留，20c6f在helper/浮字/重算/压槽后恢复EX。 */
static void apply_item_permanent_stat(fd2_field_unit *unit, unsigned code, uint16_t power) {
    uint8_t *raw = (uint8_t *)unit;
    size_t offset = code == 8 ? 0x37 : code == 9 ? 0x39 : code == 10 ? 0x3e :
        code == 17 ? 0x42 : code == 18 ? 0x46 : 0x3b;
    uint16_t value = (uint16_t)(raw[offset] | ((uint16_t)raw[offset + 1] << 8));
    value = (uint16_t)(value + power); raw[offset] = (uint8_t)value;
    raw[offset + 1] = (uint8_t)(value >> 8);
}

/* field_item_use_effect_execute @0x20c6f（原sub_20C6F）：共用地图呈现与
 * 原有有限effect core。FX使用完整raw目标组，永久stat只改首target。
 * 浮字播放/重算/压槽/恢复EX由item.effect冷图决定，失败保留已写事实。 */
static int execute_item_effect(fd2_field_game *game, size_t actor_index, size_t slot,
        const uint8_t *record, const uint8_t *targets, size_t count,
        const fd2_field_unit *recomputed) {
    unsigned code = record[0x0d];
    if (code < 5u || code > 24u || !count || count > FD2_FIELD_MAX_UNITS) return -1;
    const fd2_static_flow *flow = fd2_static_flow_lookup("magic", "item.effect");
    if (!flow) return -1;
    uint16_t power = (uint16_t)fd2_field_item_i16(record, 0x0e);
    int growth = (code >= 8u && code <= 10u) || (code >= 17u && code <= 19u);
    uint8_t id = code == 5u || code == 11u || code == 13u || code == 17u || code == 18u ? 13u :
        code == 6u ? 20u : code == 7u ? 21u : code == 8u || code == 16u ? 17u :
        code == 9u || code == 15u ? 18u : code == 10u || code == 12u || code == 19u ? 19u :
        code == 14u ? 27u : code == 22u ? 22u : code == 23u ? 23u : (uint8_t)power;
    fd2_field_unit *actor = &game->units.items[actor_index], actor_before = *actor;
    fd2_field_magic_present_event event = {0};
    event.phase = FD2_FIELD_MAGIC_PRESENT_PREPARE; event.action_kind = FD2_FIELD_MAGIC_ACTION_ITEM;
    event.presentation_mode = FD2_FIELD_MAGIC_MODE_MAP;
    event.actor_index = actor_index; event.actor_before = &actor_before;
    event.magic_id = event.effect_id = id; event.item_code = (uint8_t)code; event.item_power = power;
    event.targets = targets; event.target_count = count; event.effect_target_count = growth ? 1u : count;
    fd2_field_unit teleport_before = game->units.items[targets[0]];
    if (code == 23u) {
        event.target_index = targets[0]; event.target_before = &teleport_before;
        event.target_after = &game->units.items[targets[0]];
        event.destination_x = game->cursor_cell_x; event.destination_y = game->cursor_cell_y;
        event.cast_cost_count = 1;
    }
    if (game->magic_present && game->magic_present(game, &event, game->magic_present_userdata) != 0) return -1;
    game->ordinary_display_mode = 0;
    uint8_t previous_ex = growth ? game->units.items[targets[0]].experience : 0;
    int pc = flow->entry, completed = 0;
    for (unsigned budget = 0; budget < FD2_FLOW_STEP_BUDGET; budget++) {
        const fd2_static_flow_node *node = fd2_static_flow_at(flow, pc);
        if (!node) goto failed;
        int error = 0;
        unsigned port = FD2_FLOW_NEXT;
        if (!strcmp(node->op, "done")) { completed = 1; break; }
        if (!strcmp(node->op, "fail")) goto failed;
        if (!strcmp(node->op, "item_code_in")) {
            if (node->args[0] < 0 || node->args[0] > 1048575) goto failed;
            port = ((uint32_t)node->args[0] & (1u << (code - 5u))) ? FD2_FLOW_YES : FD2_FLOW_NO;
        } else if (!strcmp(node->op, "item_cast")) {
            event.phase = FD2_FIELD_MAGIC_PRESENT_CAST;
            if (game->magic_present) error = game->magic_present(game, &event, game->magic_present_userdata);
        } else if (!strcmp(node->op, "item_effect_group")) {
            for (size_t i = 0; i < event.effect_target_count; i++) {
                fd2_field_unit *target = &game->units.items[targets[i]], before = *target;
                fd2_field_magic_basic_result result = {0};
                result.hp_before = target->hp;
                int effect;
                if (growth) {
                    apply_item_permanent_stat(target, code, power);
                    result.hit = 1; result.raw_damage = power;
                    effect = 1;
                } else if (code == 5u || code == 11u || code == 13u) {
                    effect = code == 11u && !target->mp_max ? 0 :
                        fd2_field_magic_apply_heal_result(target, power, code == 11u,
                            game->attack_rng, game->attack_rng_userdata, &game->experience_value, &result);
                } else {
                    effect = fd2_field_magic_apply_effect_result(id, targets[i], &game->units, -1,
                        game->attack_rng, game->attack_rng_userdata, &game->experience_value, &result);
                }
                if (effect < 0) { error = -1; break; }
                result.hp_after = target->hp;
                event.phase = FD2_FIELD_MAGIC_PRESENT_TARGET; event.target_index = targets[i];
                event.target_before = &before; event.target_after = target; event.result = &result;
                if (game->magic_present && game->magic_present(game, &event, game->magic_present_userdata) != 0) {
                    error = -1; break;
                }
            }
            event.target_before = event.target_after = NULL; event.result = NULL;
        } else if (!strcmp(node->op, "item_popup_group")) {
            event.phase = FD2_FIELD_MAGIC_PRESENT_POPUP_GROUP;
            if (game->magic_present) error = game->magic_present(game, &event, game->magic_present_userdata);
        } else if (!strcmp(node->op, "item_recompute_caster")) {
            if (!recomputed) error = -1;
            else {
                actor->attack = recomputed->attack; actor->accuracy = recomputed->accuracy;
                actor->defense = recomputed->defense; actor->evasion = recomputed->evasion;
            }
        } else if (!strcmp(node->op, "item_consume_slot")) inventory_slot_remove(actor, slot);
        else if (!strcmp(node->op, "item_restore_ex")) game->units.items[targets[0]].experience = previous_ex;
        else if (!strcmp(node->op, "item_clear_reward")) game->experience_value = 0;
        else if (!strcmp(node->op, "item_teleport")) {
            error = focus_to(game, actor->x, actor->y);
            if (!error) error = magic_action_flow(game, &event);
        } else goto failed;
        if(error<0 || game->failure!=FD2_FIELD_FAILURE_NONE)goto failed;
        if(game->runtime_host_quit)return 0;
        pc = fd2_static_flow_edge(flow, node, error ? FD2_FLOW_ERROR : port);
        if (pc < 0) goto failed;
    }
    if (!completed) goto failed;
    event.phase = FD2_FIELD_MAGIC_PRESENT_FINISH;
    event.actor_before = event.target_before = event.target_after = NULL; event.result = NULL;
    if (game->magic_present && game->magic_present(game, &event, game->magic_present_userdata) != 0) goto failed;
    return 0;
failed:
    fail_action(game, actor_index); event.phase = FD2_FIELD_MAGIC_PRESENT_FINISH; event.action_failed = 1;
    event.actor_before = event.target_before = event.target_after = NULL; event.result = NULL;
    if (game->magic_present) (void)game->magic_present(game, &event, game->magic_present_userdata);
    return -1;
}

int fd2_field_game_resolve_player_item(fd2_field_game *game) {
    const uint8_t *record = item_target_record(game);
    if (!record || !game->attack_rng || !target_focus_is_valid(game) ||
        (game->interaction != FD2_FIELD_INTERACTION_ITEM_TARGETING &&
         game->interaction != FD2_FIELD_INTERACTION_ITEM_TELEPORTING)) return -1;
    size_t actor_index = (size_t)game->selected_unit;
    fd2_field_unit *actor = &game->units.items[actor_index];
    size_t slot_index = (size_t)game->item_list_selected;
    unsigned code = record[0x0d];
    uint16_t power = (uint16_t)fd2_field_item_i16(record, 0x0e);
    uint8_t targets[FD2_FIELD_MAX_UNITS];
    size_t count = 0;
    int teleport = game->interaction == FD2_FIELD_INTERACTION_ITEM_TELEPORTING;
    if (teleport) {
        if (code != 23 || actor->text_id != 24 || actor->mp_max < 20 ||
            item_teleport_destination_is_legal(game) != 1) return -1;
        targets[count++] = game->magic_target_candidates[0];
    } else {
        fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
        if (fd2_field_target_range_compute(&range, &game->map, &game->terrain,
                actor->x, actor->y, code == 23 ? 1 : 0, record[0x10]) != 0) return -1;
        int legal = record[0x12] == 4 || fd2_field_path_is_destination(&range, game->cursor_cell_x, game->cursor_cell_y);
        fd2_field_path_close(&range);
        if (!legal || fd2_field_game_item_target_is_legal(game) != 1) return -1;
        if (fd2_field_target_range_compute(&range, &game->map, &game->terrain,
                game->cursor_cell_x, game->cursor_cell_y, 0, record[0x12]) != 0) return -1;
        for (size_t i = 0; i < game->units.count; i++) {
            const fd2_field_unit *unit = &game->units.items[i];
            if (fd2_field_unit_is_hidden(unit)) continue;
            if (unit->x >= game->map.width || unit->y >= game->map.height) {
                fd2_field_path_close(&range); return -1;
            }
            if (target_matches_filter(unit, record[0x15]) && fd2_field_path_is_destination(&range, unit->x, unit->y))
                targets[count++] = (uint8_t)i;
        }
        fd2_field_path_close(&range);
        if (code == 23) {
            if (actor->text_id != 24 || actor->mp_max < 20 || !count)
                return fd2_field_game_cancel_item_targeting(game);
            if(fd2_field_event_map_reset_cells(&game->map)!=0)return -1;
            /* 1bbdc清旧cast LUT，115b6(mode6)保留首target焦点和普通frame0。
             * 保留同一个候选owner的首raw目标，不新增slot/ID真值缓存。 */
            fd2_field_path_close(&game->move_range);
            memset(game->magic_target_candidates, 0, sizeof(game->magic_target_candidates));
            memcpy(game->magic_target_candidates, targets, count);
            game->magic_target_count = count; game->magic_target_cycle = 0;
            game->interaction = FD2_FIELD_INTERACTION_ITEM_TELEPORTING;
            return 0;
        }
    }
    int heals = code == 5 || code == 13;
    uint8_t healed_targets[FD2_FIELD_MAX_UNITS];
    size_t healed_count = 0;
    for (size_t t = 0; t < count; t++) {
        const fd2_field_unit *unit = &game->units.items[targets[t]];
        if (heals || ((code == 6 || code == 7) && unit->detail_status[code == 6 ? 0 : 1]))
            healed_targets[healed_count++] = targets[t];
    }
    int growth = (code >= 8 && code <= 10) || (code >= 17 && code <= 19);
    if (growth && !count) return -1;
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (fd2_field_unit_is_hidden(unit)) continue;
        if (unit->x >= game->map.width || unit->y >= game->map.height) return -1;
        int target = 0;
        for (size_t t = 0; t < count; t++) if (targets[t] == i) target = 1;
        int may_die = target ? item_effect_may_kill(record, unit) : !unit->hp;
        if (may_die < 0) return -1;
        if (target && !growth && code != 23) {
            if (heals) {
                if (unit->unit_id < 75u && !unit->hp_max) return -1;
            } else if (code == 11) {
                /* dispatcher的maxMP==0分支完全跳过恢复及贡献。 */
            } else {
                uint8_t id = code == 6 ? 20 : code == 7 ? 21 : code == 12 ? 19 : code == 14 ? 27 :
                    code == 15 ? 18 : code == 16 ? 17 : code == 22 ? 22 : (uint8_t)power;
                if (fd2_field_magic_effect_reward_preflight(id, unit) != 0) return -1;
            }
        }
        for (size_t t = 0; t < healed_count; t++)
            if (healed_targets[t] == i) may_die = !unit->hp_max;
        if (may_die && loot_descriptor_preflight(game, actor_index,
                (const uint8_t *)unit + 0x31) != 0) return -1;
    }
    fd2_field_unit recomputed = *actor;
    if (growth) {
        /* 21082只写首target的WORD，19恢复跨WORD的EX；重算的是caster。 */
        if (targets[0] == actor_index) apply_item_permanent_stat(&recomputed, code, power);
        if (fd2_field_unit_combat_stats_recompute(&recomputed, fd2_field_unit_original_item_stats, NULL,
                fd2_field_unit_original_status_scale, NULL) != 0) return -1;
    }
    int final_x = teleport && targets[0] == actor_index ? game->cursor_cell_x : actor->x;
    int final_y = teleport && targets[0] == actor_index ? game->cursor_cell_y : actor->y;
    if (!player_effect_tail_is_supported(game, actor_index, healed_targets,
            healed_count, code == 7, 0, final_x, final_y)) return -1;
    /* 1bbdc在selector/最终收集后4df4c，取消或Use均不把轨迹交给效果页。 */
    if(fd2_field_event_map_reset_cells(&game->map)!=0)return -1;
    if (execute_item_effect(game, actor_index, slot_index, record, targets,
            teleport ? 1u : count, growth ? &recomputed : NULL) != 0) return -1;
    if(game->runtime_host_quit)return 0;
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    int loot_result = begin_defeated_loot(game, actor_index, actor_index, PLAYER_ACTION_ITEM, 0, 0);
    if (loot_result < 0) { fail_action(game, actor_index); return -1; }
    if (loot_result) return 0;
    if (finish_unit_action_internal(game, PLAYER_ACTION_ITEM, 0) != 0) {
        fail_action(game, actor_index); return -1;
    }
    return 0;
}

int fd2_field_game_begin_attack(fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        progress_pending(game) ||
        game->units.count > FD2_FIELD_MAX_UNITS ||
        game->interaction != FD2_FIELD_INTERACTION_COMMAND ||
        !fd2_field_game_attack_is_available(game))
        return -1;

    /* field_player_command_execute @0x18d8c（原field_player_command_execute_core）
     * 以武器min/max与filter0
     * 调用field_target_range_build @0x14818，再由
     * field_cell_selection_execute @0x115b6聚焦首raw候选；121只占槽。 */
    const fd2_field_unit *attacker =
        &game->units.items[game->selected_unit];
    uint8_t minimum = 0, maximum = 0;
    if (!target_focus_is_valid(game) ||
        fd2_field_attack_weapon_range(attacker, &minimum, &maximum) != 0 ||
        begin_target_selection(game, maximum, minimum, 0,
            FD2_FIELD_INTERACTION_TARGETING, 0) != 0)
        return -1;
    if(game->runtime_host_quit)return 0;

    game->attack_target = -1;
    game->last_attack_valid = 0;
    game->last_attack_strikes = 0;
    game->last_counterattack_valid = 0;
    game->last_counterattack_strikes = 0;
    return 0;
}

int fd2_field_game_cycle_attack_target(fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        progress_pending(game) || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count ||
        game->units.count > FD2_FIELD_MAX_UNITS || !target_focus_is_valid(game) ||
        game->interaction != FD2_FIELD_INTERACTION_TARGETING)
        return -1;
    return cycle_target_selection(game);
}

int fd2_field_game_cancel_attack(fd2_field_game *game) {
    if (!game || !game->ready || progress_pending(game) || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count ||
        game->interaction != FD2_FIELD_INTERACTION_TARGETING)
        return 0;
    /* field_player_command_execute @0x18d8c（原field_player_command_execute_core）
     * 在目标选择返回 -1 后，
     * 调用 field_focus_move_to @0x12cea 恢复攻击前焦点，再返回 0 让
     * 上层重开指令菜单。沿用同一 X 后 Y 顺序以同步恢复镜头。 */
    const fd2_field_unit *attacker =
        &game->units.items[game->selected_unit];
    if (focus_to(game, attacker->x, attacker->y) != 0) return 0;
    if(game->runtime_host_quit)return 0;
    game->attack_target = -1;
    close_target_range(game);
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    (void)fd2_field_game_refresh_commands(game);
    return 1;
}

static int terrain_adjusted_stat(const fd2_field_game *game,
                                 const fd2_field_unit *unit,
                                 uint32_t base_stat,
                                 int use_attack_modifier,
                                 uint32_t *adjusted) {
    if (!game || !unit || !adjusted) return -1;
    if (fd2_field_attack_unit_ignores_terrain_modifier(unit)) {
        *adjusted = base_stat;
        return 0;
    }
    uint32_t cell = fd2_field_map_cell(&game->map, unit->x, unit->y);
    const fd2_terrain_attr *attr = fd2_terrain_attr_get(
        &game->terrain, fd2_field_cell_terrain(cell));
    if (!attr) return -1;
    int32_t attack_percent = 0;
    int32_t defense_percent = 0;
    if (fd2_field_attack_terrain_modifiers(attr->movement_cost_class,
                                           &attack_percent,
                                           &defense_percent) != 0)
        return -1;
    return fd2_field_attack_apply_terrain_modifier(
        base_stat, use_attack_modifier ? attack_percent : defense_percent,
        adjusted);
}

static int physical_reward_value(const fd2_field_unit *attacker,
        const fd2_field_unit *defender, uint32_t damage, uint16_t hp_after,
        uint32_t *value) {
    if (attacker->side != 2u || defender->unit_id < 68u) return 0;
    uint8_t coefficient;
    if (fd2_field_unit_enemy_experience_coefficient(defender->unit_id, &coefficient) != 0) return -1;
    uint8_t level = attacker->level;
    if ((attacker->movement_profile > 8u && attacker->movement_profile < 25u) || attacker->text_id == 28u)
        level = (uint8_t)(level + 30u);
    return fd2_field_combat_reward_value(level, defender->level, coefficient,
        defender->hp_max, damage, hp_after == 0u, value) == 0 ? 1 : -1;
}

static int physical_max_damage(const fd2_field_game *game,
        const fd2_field_unit *attacker, const fd2_field_unit *defender, uint32_t *damage) {
    uint32_t attack, defense;
    uint8_t critical = 0, bonus;
    if (terrain_adjusted_stat(game, attacker, attacker->attack, 1, &attack) != 0 ||
        terrain_adjusted_stat(game, defender, defender->defense, 0, &defense) != 0 ||
        fd2_field_attack_weapon_critical_bonus(attacker, &bonus) != 0 ||
        (!game->attack_critical_base && fd2_field_attack_base_critical_chance(attacker, &critical) != 0)) return -1;
    if (game->attack_critical_base || critical || bonus) defense /= 2u;
    uint32_t base = attack > defense ? (uint32_t)((uint64_t)(attack - defense) * 9u / 10u) : 0;
    *damage = attacker->accuracy > defender->evasion ? base + (base / 9u ? base / 9u - 1u : 0) : 0;
    return 0;
}

static int physical_reward_preflight(fd2_field_game *game,
        size_t attacker_index, size_t target_index, int player) {
    const fd2_field_unit *attacker = &game->units.items[attacker_index];
    const fd2_field_unit *defender = &game->units.items[target_index];
    uint32_t attack_max, counter_max = 0;
    int counter = fd2_field_attack_counterattack_is_available(defender, attacker);
    if (physical_max_damage(game, attacker, defender, &attack_max) != 0 ||
        (counter && physical_max_damage(game, defender, attacker, &counter_max) != 0)) return -1;
    uint32_t maximum = game->experience_value;
    const fd2_field_unit *from[2] = {attacker, defender}, *to[2] = {defender, attacker};
    uint32_t damage[2] = {attack_max, counter_max};
    for (size_t i = 0; i < (counter ? 2u : 1u); i++) {
        uint32_t full;
        int writes = physical_reward_value(from[i], to[i], 0, 0, &full);
        if (writes < 0) return -1;
        if (!writes) continue;
        if (full > maximum) maximum = full;
        uint32_t nonlethal = to[i]->hp ? to[i]->hp - 1u : 0;
        if (nonlethal > damage[i]) nonlethal = damage[i];
        uint32_t alive;
        if (physical_reward_value(from[i], to[i], nonlethal, 1, &alive) < 0) return -1;
        if (alive > maximum) maximum = alive;
    }
    fd2_field_attack_sequence_mode mode = !player && game->option_values[2]
        ? FD2_FIELD_ATTACK_SEQUENCE_SHORT : FD2_FIELD_ATTACK_SEQUENCE_FULL;
    uint8_t attack_limit, counter_limit = 0;
    if (fd2_field_attack_sequence_max_count_for_mode(attacker, mode, &attack_limit) != 0 ||
        (counter && fd2_field_attack_sequence_max_count_for_mode(defender, mode, &counter_limit) != 0))
        return -1;
    /* 所有可能进入1b6b7的死亡均先检查，未知掉落不在首RNG之后才报错。 */
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (fd2_field_unit_is_hidden(unit)) continue;
        /* FULL的type3与3%叠加可三击；逐方查询上界，不能漏致死loot，
         * 也不能把无type3武器一律乘3造成错误拒绝。 */
        int may_die = !unit->hp || (i == target_index && unit->hp <= (uint64_t)attack_max * attack_limit) ||
            (counter && i == attacker_index && unit->hp <= (uint64_t)counter_max * counter_limit);
        if (may_die && loot_descriptor_preflight(game, player ? attacker_index : target_index,
                (const uint8_t *)unit + 0x31) != 0) return -1;
    }
    if (player && maximum > 99u) maximum = 99u;
    return preflight_reward(game, player ? attacker_index : target_index, maximum) < 0 ? -1 : 0;
}

static int resolve_attack_sequence(
        fd2_field_game *game,
        fd2_field_unit *attacker,
        fd2_field_unit *defender,
        fd2_field_attack_result *last_result,
        uint8_t *resolved_strikes, int counterattack,
        fd2_field_physical_present_mode presentation_mode) {
    if (!game || !attacker || !defender || !last_result ||
        !resolved_strikes || !game->attack_rng)
        return -1;

    uint32_t adjusted_attack = 0;
    uint32_t adjusted_defense = 0;
    /* 非法 class >5 先于任何 RNG 拒绝，避免错误数据推进共享随机流。 */
    if (terrain_adjusted_stat(game, attacker, attacker->attack, 1,
                              &adjusted_attack) != 0 ||
        terrain_adjusted_stat(game, defender, defender->defense, 0,
                              &adjusted_defense) != 0)
        return -1;

    uint8_t weapon_critical_bonus = 0;
    if (fd2_field_attack_weapon_critical_bonus(
            attacker, &weapon_critical_bonus) != 0)
        return -1;

    uint8_t strike_limit = 1;
    if (fd2_field_attack_sequence_count_for_mode(
            attacker, presentation_mode == FD2_FIELD_PHYSICAL_MODE_SHORT
                ? FD2_FIELD_ATTACK_SEQUENCE_SHORT : FD2_FIELD_ATTACK_SEQUENCE_FULL,
            game->attack_rng,
            game->attack_rng_userdata, &strike_limit) != 0)
        return -1;

    *resolved_strikes = 0;
    for (uint8_t strike = 0; strike < strike_limit; strike++) {
        fd2_field_unit actor_before = *attacker, target_before = *defender;
        uint8_t base_critical_chance = 0;
        int critical_result = game->attack_critical_base
            ? game->attack_critical_base(
                  attacker, game->attack_critical_base_userdata,
                  &base_critical_chance)
            : fd2_field_attack_base_critical_chance(
                  attacker, &base_critical_chance);
        if (critical_result != 0) {
            return -1;
        }
        uint32_t critical_chance =
            (uint32_t)base_critical_chance +
            (uint32_t)weapon_critical_bonus;
        /* 原函数以整数阈值与 rand()%100 比较；超过 255 的阈值与 255 对
         * 0..99 的结果相同。 */
        if (critical_chance > UINT8_MAX) critical_chance = UINT8_MAX;

        int pre_hit_effect_applied = 0;
        if (fd2_field_attack_apply_pre_hit_effect(
                attacker, defender, game->attack_rng,
                game->attack_rng_userdata,
                &pre_hit_effect_applied) != 0) {
            return -1;
        }
        fd2_field_attack_params params = {
            .attack = adjusted_attack,
            .defense = adjusted_defense,
            .accuracy = attacker->accuracy,
            .evasion = defender->evasion,
            .critical_chance = critical_chance,
            .defender_hp = defender->hp,
        };
        if (fd2_field_combat_resolve_attack(
                &params, game->attack_rng, game->attack_rng_userdata,
                last_result) != 0) {
            return -1;
        }
        defender->hp = last_result->hp_after;
        uint32_t reward;
        int writes = physical_reward_value(attacker, defender,
            last_result->rolled_damage, last_result->hp_after, &reward);
        if (writes < 0) return -1;
        if (writes) game->experience_value = reward; /* 每击覆盖，非累加。 */
        (*resolved_strikes)++;
        if (game->physical_present) {
            fd2_field_physical_event event = {0};
            event.phase = FD2_FIELD_PHYSICAL_STRIKE;
            event.presentation_mode = presentation_mode;
            event.actor_index = (size_t)(attacker - game->units.items);
            event.target_index = (size_t)(defender - game->units.items);
            event.counterattack = (uint8_t)(counterattack != 0);
            event.strike_index = strike;
            event.pre_hit_status_applied = (uint8_t)(pre_hit_effect_applied != 0);
            event.actor_before = &actor_before;
            event.target_before = &target_before;
            event.result = last_result;
            if (game->physical_present(game, &event,
                    game->physical_present_userdata) != 0) return -1;
        }
        if (defender->hp == 0) break;
    }
    return 0;
}

static int physical_sequence_preflight(
        const fd2_field_game *game,
        const fd2_field_unit *attacker,
        const fd2_field_unit *defender,
        int require_builtin_critical) {
    if (!game || !attacker || !defender || !game->attack_rng ||
        (require_builtin_critical && game->attack_critical_base))
        return -1;
    uint32_t adjusted_attack = 0;
    uint32_t adjusted_defense = 0;
    uint8_t weapon_critical_bonus = 0;
    uint8_t base_critical_chance = 0;
    /* AI 提交不接受测试用 critical hook：回调可能在已消费 RNG 后失败，
     * 而通用 RNG ABI 没有 checkpoint。生产路径使用已确认的 profile 表；
     * 将所有可失败校验前移，保证开始 physical sequence 后不会返回错误。 */
    if (terrain_adjusted_stat(game, attacker, attacker->attack, 1,
                              &adjusted_attack) != 0 ||
        terrain_adjusted_stat(game, defender, defender->defense, 0,
                              &adjusted_defense) != 0 ||
        adjusted_attack > FD2_FIELD_COMBAT_EFFECTIVE_STAT_MAX ||
        adjusted_defense > FD2_FIELD_COMBAT_EFFECTIVE_STAT_MAX ||
        fd2_field_attack_weapon_critical_bonus(
            attacker, &weapon_critical_bonus) != 0 ||
        (!game->attack_critical_base &&
         fd2_field_attack_base_critical_chance(
             attacker, &base_critical_chance) != 0))
        return -1;
    return 0;
}

static int physical_exchange_preflight(
        const fd2_field_game *game,
        size_t attacker_index,
        size_t target_index,
        int require_builtin_critical) {
    if (!game || attacker_index >= game->units.count ||
        target_index >= game->units.count || attacker_index == target_index)
        return -1;
    const fd2_field_unit *attacker = &game->units.items[attacker_index];
    const fd2_field_unit *defender = &game->units.items[target_index];
    if (physical_sequence_preflight(
            game, attacker, defender, require_builtin_critical) != 0)
        return -1;
    if (fd2_field_attack_counterattack_is_available(defender, attacker) &&
        physical_sequence_preflight(
            game, defender, attacker, require_builtin_critical) != 0)
        return -1;
    return 0;
}

typedef struct {
    fd2_field_game *game;
    size_t actor, target;
    fd2_field_physical_present_mode mode;
    fd2_field_attack_result results[2];
    uint8_t strikes[2];
    int counter, prepared, finished, recovered;
} field_action_flow;

/* 18d8c/1548e的FULL/SHORT外层与13fd4恢复共用既有同步服务；冷图只选
 * 有限领域操作，单击数学/RNG仍由原Game核心唯一提交，不增加持久执行状态。 */
static int action_flow_present(field_action_flow *ctx,
        fd2_field_physical_phase phase, int step, int value) {
    fd2_field_game *game=ctx->game;
    fd2_field_physical_event event={0};
    event.phase=phase;event.presentation_mode=ctx->mode;
    event.actor_index=ctx->actor;event.target_index=ctx->target;
    if (phase!=FD2_FIELD_PHYSICAL_PREPARE) {
        event.actor_before=&game->units.items[ctx->actor];
        event.target_before=&game->units.items[ctx->target];
    }
    event.recovery_step=(fd2_field_recovery_step)step;event.step_value=(uint16_t)value;
    return game->physical_present ? game->physical_present(game,&event,
        game->physical_present_userdata):0;
}

static int run_action_flow(field_action_flow *ctx, const char *id) {
    const fd2_static_flow *flow=fd2_static_flow_lookup("progress",id);
    if (!flow || !ctx || !ctx->game) return -1;
    fd2_field_game *game=ctx->game;
    fd2_field_unit *actor=&game->units.items[ctx->actor];
    fd2_field_unit *target=&game->units.items[ctx->target];
    int pc=flow->entry;
    for (unsigned budget=0;budget<FD2_FLOW_STEP_BUDGET;budget++) {
        const fd2_static_flow_node *node=fd2_static_flow_at(flow,pc);
        if (!node) break;
        unsigned port=FD2_FLOW_NEXT;int value=node->args[0];
        const char *op=node->op;
        if (!strcmp(op,"action_prepare")) {
            if (action_flow_present(ctx,FD2_FIELD_PHYSICAL_PREPARE,0,0)!=0) break;
            ctx->prepared=1;
        } else if (!strcmp(op,"action_display")) game->ordinary_display_mode=(uint8_t)value;
        else if (!strcmp(op,"action_present")) {
            if (!ctx->prepared || (value!=FD2_FIELD_PHYSICAL_FINISH &&
                value!=FD2_FIELD_PHYSICAL_SHORT_BEGIN && value!=FD2_FIELD_PHYSICAL_SHORT_MAP &&
                value!=FD2_FIELD_PHYSICAL_COUNTER_REFRESH)) break;
            if (value==FD2_FIELD_PHYSICAL_FINISH) ctx->finished=1;
            if (action_flow_present(ctx,(fd2_field_physical_phase)value,0,0)!=0) break;
        } else if (!strcmp(op,"action_face")) {
            fd2_field_unit *from=value ? target:actor,*to=value ? actor:target;
            from->direction=face_toward(from,to); /* 1f04a */
        } else if (!strcmp(op,"action_call_full")) {
            /* 固定领域子调用不能在bare FULL自递归，坏冷定义明确失败。 */
            if(strcmp(id,"physical.caller.full"))break;
            if(run_action_flow(ctx,"physical.full")!=0)return -1;
        } else if (!strcmp(op,"action_delay_ms")) {
            if(value!=20)break;
            int wait=turn_present(game,FD2_FIELD_RUNTIME_DELAY_MS,value);
            if(wait<0)break;
            if(wait>0)return 0;
        } else if (!strcmp(op,"action_reset_directions")) {
            for (size_t i=0;i<game->units.count;i++) game->units.items[i].direction=0;
        } else if (!strcmp(op,"action_sequence")) {
            if (value<0 || value>1 || resolve_attack_sequence(game,
                value ? target:actor,value ? actor:target,&ctx->results[value],
                &ctx->strikes[value],value,ctx->mode)!=0) break;
        } else if (!strcmp(op,"action_counter_ready")) {
            int ready=(!value || target->hp!=0) &&
                fd2_field_attack_counterattack_is_available(target,actor);
            if (value) ctx->counter=ready;
            port=ready ? FD2_FLOW_YES:FD2_FLOW_NO;
        } else if (!strcmp(op,"recovery_needed")) {
            port=actor->hp!=actor->hp_max && !actor->detail_status[0] && !actor->detail_status[1]
                ? FD2_FLOW_YES:FD2_FLOW_NO;
        } else if (!strcmp(op,"recovery_focus")) {
            if (focus_to(game,actor->x,actor->y)!=0) break;
        } else if (!strcmp(op,"recovery_step")) {
            int arg=node->args[1];
            if (ctx->mode!=FD2_FIELD_PHYSICAL_MODE_RECOVERY ||
                !((value==FD2_FIELD_RECOVERY_WAIT && arg==1) ||
                  (value==FD2_FIELD_RECOVERY_CUE && arg==4) ||
                  (value==FD2_FIELD_RECOVERY_ACTOR && (arg==0 || arg==2))) ||
                action_flow_present(ctx,FD2_FIELD_PHYSICAL_RECOVERY_STEP,value,arg)!=0) break;
        } else if (!strcmp(op,"recovery_hp_commit")) {
            if (value<=0) break;
            uint32_t hp=(uint32_t)actor->hp+actor->hp_max/(unsigned)value;
            actor->hp=(uint16_t)(hp>actor->hp_max ? actor->hp_max:hp);
            ctx->recovered=1;
        } else if (!strcmp(op,"action_done")) return 0;
        else break;
        if(game->runtime_host_quit)return 0;
        pc=fd2_static_flow_edge(flow,node,port);
        if (pc<0) break;
    }
    if (ctx->prepared) {
        fail_action(game,ctx->actor);
        if (!ctx->finished && game->physical_present) {
            fd2_field_physical_event event={0};
            event.phase=FD2_FIELD_PHYSICAL_FINISH;event.presentation_mode=ctx->mode;
            event.actor_index=ctx->actor;event.target_index=ctx->target;event.action_failed=1;
            (void)game->physical_present(game,&event,game->physical_present_userdata);
        }
    }
    return -1;
}

int fd2_field_game_resolve_event_exchange(fd2_field_game *game,
    size_t actor, size_t target) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        physical_exchange_preflight(game, actor, target, 0) != 0) return -1;
    field_action_flow action = {0};
    action.game = game; action.actor = actor; action.target = target;
    action.mode = FD2_FIELD_PHYSICAL_MODE_FULL;
    return run_action_flow(&action, "physical.full");
}

static int resolve_physical_exchange(fd2_field_game *game,
                                     size_t attacker_index,
                                     size_t target_index,
                                     int finish_player_action) {
    if (!game || !game->ready ||
        physical_exchange_preflight(
            game, attacker_index, target_index, 0) != 0 ||
        physical_reward_preflight(game, attacker_index, target_index, finish_player_action) != 0)
        return -1;
    /* 玩家18d8c始终FULL；只有AI1548e按53af9选择SHORT，算法与呈现同域。 */
    field_action_flow action={0};action.game=game;action.actor=attacker_index;action.target=target_index;
    action.mode=!finish_player_action && game->option_values[2]
        ? FD2_FIELD_PHYSICAL_MODE_SHORT:FD2_FIELD_PHYSICAL_MODE_FULL;
    if (run_action_flow(&action,action.mode==FD2_FIELD_PHYSICAL_MODE_SHORT
            ? "physical.short":"physical.caller.full")!=0) return -1;
    if(game->runtime_host_quit)return 0;

    /* field_physical_attack_resolve @0x1ecc7 本体只写 +0x25/+0x40；外围
     * exchange 返回后立即调用 field_defeated_units_finalize @0x1db65，
     * 统一将所有 HP==0 actor 的 +0x05 写为 1。 */
    game->last_attack = action.results[0];
    game->last_attack_strikes = action.strikes[0];
    game->last_attack_valid = 1;
    game->last_counterattack_strikes = action.strikes[1];
    game->last_counterattack_valid = (uint8_t)action.counter;
    if (action.counter) game->last_counterattack = action.results[1];
    game->attack_target = (int)target_index;
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    int loot_result = begin_defeated_loot(game, finish_player_action ? attacker_index : target_index,
        attacker_index, finish_player_action ? PLAYER_ACTION_PHYSICAL : PLAYER_ACTION_AI_PHYSICAL, 0, 0);
    if (loot_result < 0) { fail_action(game, attacker_index); return -1; }
    if (game->runtime_host_quit) return 0;
    if (loot_result) {
        if (!finish_player_action) {
            game->selected_unit = game->command_selected = -1;
            game->interaction = FD2_FIELD_INTERACTION_BROWSE;
        }
        return 0;
    }
    if (finish_player_action && finish_unit_action_internal(game, PLAYER_ACTION_PHYSICAL, 0) != 0) {
        /* cell/phase 失败发生在伤害与 RNG 提交之后，不能假回滚再重试。 */
        fail_action(game, attacker_index);
        return -1;
    }
    if (!finish_player_action) {
        /* AI core tail owns action-completion; exchange 不得直接 acted/event/
         * phase advance。 */
        game->selected_unit = -1;
        game->command_selected = -1;
        game->interaction = FD2_FIELD_INTERACTION_BROWSE;
        if (begin_reward(game, target_index, FD2_FIELD_PROGRESS_RESUME_NONE, attacker_index) < 0) {
            fail_action(game, attacker_index); return -1;
        }
    }
    return 0;

}

int fd2_field_game_resolve_attack(fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        progress_pending(game) ||
        game->selected_unit < 0 ||
        game->interaction != FD2_FIELD_INTERACTION_TARGETING ||
        !game->attack_rng)
        return -1;

    int target_index = fd2_field_game_unit_at(game, game->cursor_cell_x,
                                               game->cursor_cell_y);
    if (target_index < 0 ||
        !fd2_field_game_attack_target_is_legal(game, (size_t)target_index))
        return -1;
    return resolve_physical_exchange(
        game, (size_t)game->selected_unit, (size_t)target_index, 1);
}

/* 详情仅支持普通字形和结束符；末尾坏字模也须在首帧之前拒绝。 */
static int detail_fragment_ready(const fd2_field_game *game, size_t fragment,
                                  const uint8_t **tokens, size_t *glyphs) {
    const uint8_t *data;
    size_t count;
    if (fd2_text_entry_get_fragment(&game->ui_text, fragment, &data, &count) != 0)
        return 0;
    for (size_t i = 0; i < count; i++) {
        uint16_t token = fd2_text_token_at(data, i);
        if (token == 0xffffu) {
            *tokens = data;
            *glyphs = i;
            return 1;
        }
        if (token >= 0xff00u || token >= game->font.glyph_count) return 0;
    }
    return 0;
}

/* 详情与法术列表共用上页读集；下页由各自调用者完整预检，
 * 不把法术列表已清掉的装备内容变成其必需资源。 */
int fd2_field_game_detail_upper_is_ready(const fd2_field_game *game,
                                    const fd2_field_unit *unit) {
    return game && game->ready && fd2_field_detail_upper_is_ready(
        &game->info_assets, &game->font, &game->ui_text, unit);
}

static int detail_resources_ready(const fd2_field_game *game,
                                   const fd2_field_unit *unit) {
    if (!fd2_field_game_detail_upper_is_ready(game, unit)) return 0;
    const fd2_field_info_assets *assets = &game->info_assets;
    if (!assets->detail_bottom.pixels || assets->detail_bottom.width != 310 ||
        assets->detail_bottom.height != 99) return 0;
    for (size_t i = 0; i < 9; i++) {
        if (!assets->detail_icons[i].pixels ||
            assets->detail_icons[i].width != (i < 5 ? 24 : 21) ||
            assets->detail_icons[i].height != (i < 5 ? 20 : 8)) return 0;
    }
    const uint8_t *record = (const uint8_t *)unit;
    for (size_t slot = 0; slot < 8; slot++) {
        if (record[0x0a + slot * 2u] & 0x80u) continue;
        uint8_t item_id = record[0x0b + slot * 2u];
        const uint8_t *tokens;
        size_t glyphs;
        if (!fd2_field_item_record_get(item_id) ||
            !detail_fragment_ready(game, (size_t)item_id + 0xb5u,
                                     &tokens, &glyphs)) return 0;
    }
    size_t count = fd2_field_unit_magic_list_build(unit, NULL, 0);
    if (count == 0) return 1;
    if (count > FD2_FIELD_MAGIC_LIST_MAX_COUNT) return 0;
    /* 第二页也在装备首帧前完整预检；不把未知位／坏末项当空列表。 */
    fd2_vga *check = malloc(sizeof(*check));
    if (!check) return 0;
    memset(check, 0, sizeof(*check));
    int valid = fd2_field_magic_list_draw(check, assets, &game->font,
        &game->ui_text, unit, -1) == 0;
    free(check);
    return valid;
}

int fd2_field_game_open_detail(fd2_field_game *game, size_t unit_index) {
    if (!game || !game->ready || progress_pending(game) || unit_index >= game->units.count ||
        game->units.count > FD2_FIELD_MAX_UNITS ||
        fd2_field_unit_is_hidden(&game->units.items[unit_index]) ||
        game->units.items[unit_index].hp == 0 ||
        !detail_resources_ready(game, &game->units.items[unit_index]))
        return -1;
    fd2_image portrait = {0};
    if (fd2_portrait_decode_frame(&portrait, &game->dato_archive,
                                  game->units.items[unit_index].unit_id,
                                  0) != 0)
        return -1;
    if (!portrait.pixels || portrait.width != 80 || portrait.height != 80) {
        fd2_image_free(&portrait);
        return -1;
    }
    fd2_image_free(&game->detail_portrait);
    game->detail_portrait = portrait;
    game->detail_unit = (int)unit_index;
    game->detail_visible = 1;
    game->detail_page = FD2_FIELD_DETAIL_EQUIPMENT;
    game->detail_acknowledged_unit = -1;
    return 0;
}

void fd2_field_game_close_detail(fd2_field_game *game) {
    if (!game) return;
    int unit_index = game->detail_unit;
    fd2_image_free(&game->detail_portrait);
    game->detail_unit = -1;
    game->detail_visible = 0;
    game->detail_page = FD2_FIELD_DETAIL_EQUIPMENT;
    game->detail_acknowledged_unit = unit_index;
}

int fd2_field_game_animate_detail(fd2_field_game *game, fd2_vga *vga,
                                  int opening,
                                  fd2_field_detail_phase_fn phase_callback,
                                  void *phase_userdata) {
    if (!vga) return -1;
    if(!game)return -1;
    int terminal=menu_transition_terminal(game,vga);
    if(terminal)return terminal;
    if (!game->detail_visible || game->detail_unit < 0 ||
        (size_t)game->detail_unit >= game->units.count ||
        (game->detail_page != FD2_FIELD_DETAIL_EQUIPMENT &&
         game->detail_page != FD2_FIELD_DETAIL_MAGIC) ||
        !detail_resources_ready(game, &game->units.items[game->detail_unit]) ||
        !game->detail_portrait.pixels || game->detail_portrait.width != 80 ||
        game->detail_portrait.height != 80) return -1;
    uint8_t *background = malloc(VGA_W * VGA_H);
    fd2_vga *detail = malloc(sizeof(*detail));
    if (!background || !detail) {
        free(background);
        free(detail);
        return -1;
    }

    memset(detail, 0, sizeof(*detail));
    fd2_field_render_scope visual_scope;
    if (fd2_field_render_scope_begin(&visual_scope, detail, vga, 1) != 0) {
        free(background); free(detail); return -1;
    }
    game->detail_visible = 0;
    fd2_field_game_render_preview(game, detail);
    fd2_field_render_scope_end(&visual_scope);
    memcpy(background, detail->framebuffer, VGA_W * VGA_H);
    game->detail_visible = 1;
    terminal=menu_transition_terminal(game,vga);
    if(terminal) {free(background);free(detail);return terminal;}
    if (fd2_field_detail_draw(detail, &game->info_assets, &game->font,
        &game->ui_text, &game->units.items[game->detail_unit], &game->detail_portrait) != 0) {
        free(background); free(detail); return -1;
    }
    if (game->detail_page == FD2_FIELD_DETAIL_MAGIC &&
        fd2_field_magic_list_draw(detail, &game->info_assets, &game->font,
            &game->ui_text, &game->units.items[game->detail_unit], -1) != 0) {
        free(background);
        free(detail);
        return -1;
    }

    int result = 0;
    if (opening) {
        for (int phase = 11; phase >= 0; phase--) {
            if (phase_callback)
                phase_callback(phase_userdata, 1, phase);
            if ((result=menu_transition_terminal(game,vga))!=0) break;
            fd2_field_detail_transition_frame(vga->framebuffer, detail->framebuffer,
                                    background, phase);
            game_present_copy(vga); /* 17e0b→18409：本12相位无额外等待。 */
            if ((result=menu_transition_terminal(game,vga))!=0) break;
        }
    } else {
        for (int phase = 0; phase < 12; phase++) {
            if (phase_callback)
                phase_callback(phase_userdata, 0, phase);
            if ((result=menu_transition_terminal(game,vga))!=0) break;
            fd2_field_detail_transition_frame(vga->framebuffer, detail->framebuffer,
                                    background, phase);
            game_present_copy(vga); /* 17aed/1b932/1bffe/1cff0关闭同源。 */
            if ((result=menu_transition_terminal(game,vga))!=0) break;
        }
        if (result == 0) {
            memcpy(vga->framebuffer, background, VGA_W * VGA_H);
            game_present_copy(vga);
            result=menu_transition_terminal(game,vga);
        }
    }
    free(background);
    free(detail);
    return result;
}

/* field_unit_detail_present @0x17aed（原sub_17AED）固定left/top位置，
 * 1839b只卷动底条；phase7是独立memmove，只留下上两块。
 * 与18409整页过渡分开，不能让翻页时上半立绘和属性一起移出。 */
static void detail_bottom_frame(uint8_t *dst, const uint8_t *page,
                                  const uint8_t *background, int phase) {
    memcpy(dst, background, VGA_W * VGA_H);
    for (int row = 7; row < 93; row++) {
        memcpy(dst + row * VGA_STRIDE + 5, page + row * VGA_STRIDE + 5, 86);
        memcpy(dst + row * VGA_STRIDE + 92, page + row * VGA_STRIDE + 92, 223);
    }
    if (phase == 7) return;
    int y = 94 + phase * 16;
    int height = 200 - y;
    if (height > 102) height = 102;
    for (int row = 0; row < height; row++)
        memcpy(dst + (y + row) * VGA_STRIDE + 5,
               page + (94 + row) * VGA_STRIDE + 5, 310);
}

int fd2_field_game_animate_detail_magic_page(
        fd2_field_game *game, fd2_vga *vga,
        fd2_field_detail_phase_fn phase_callback, void *phase_userdata) {
    if (!vga) return -1;
    if(!game)return -1;
    int terminal=menu_transition_terminal(game,vga);
    if(terminal)return terminal;
    if (!game->detail_visible || game->detail_unit < 0 ||
        (size_t)game->detail_unit >= game->units.count ||
        game->detail_page != FD2_FIELD_DETAIL_EQUIPMENT ||
        !detail_resources_ready(game, &game->units.items[game->detail_unit]) ||
        !fd2_field_unit_magic_list_build(&game->units.items[game->detail_unit], NULL, 0) ||
        !game->detail_portrait.pixels || game->detail_portrait.width != 80 ||
        game->detail_portrait.height != 80) return -1;
    uint8_t *background = malloc(VGA_W * VGA_H);
    fd2_vga *page = malloc(sizeof(*page));
    int result = -1;
    if (!background || !page) goto done;
    memset(page, 0, sizeof(*page));
    fd2_field_render_scope visual_scope;
    if (fd2_field_render_scope_begin(&visual_scope, page, vga, 1) != 0) goto done;
    game->detail_visible = 0;
    fd2_field_game_render_preview(game, page);
    fd2_field_render_scope_end(&visual_scope);
    memcpy(background, page->framebuffer, VGA_W * VGA_H);
    game->detail_visible = 1;
    terminal=menu_transition_terminal(game,vga);
    if(terminal) {result=terminal;goto done;}
    if (fd2_field_detail_draw(page, &game->info_assets, &game->font,
        &game->ui_text, &game->units.items[game->detail_unit], &game->detail_portrait) != 0) goto done;
    if (phase_callback) phase_callback(phase_userdata, 0, 0);
    for (int phase = 0; phase <= 6; phase++) {
        if ((result=menu_transition_terminal(game,vga))!=0) goto done;
        detail_bottom_frame(vga->framebuffer, page->framebuffer, background, phase);
        game_present_copy(vga); /* 17aed→1839b：连续底条拷贝，无16ms门。 */
        if ((result=menu_transition_terminal(game,vga))!=0) goto done;
    }
    detail_bottom_frame(vga->framebuffer, page->framebuffer, background, 7);
    game_present_copy(vga); /* 原版独立空底页，没有额外16ms计时门。 */
    if ((result=menu_transition_terminal(game,vga))!=0) goto done;
    if (fd2_field_magic_list_draw(page, &game->info_assets, &game->font,
            &game->ui_text, &game->units.items[game->detail_unit], -1) != 0) {
        result=-1;goto done;
    }
    game->detail_page = FD2_FIELD_DETAIL_MAGIC;
    if (phase_callback) phase_callback(phase_userdata, 1, 11);
    for (int phase = 6; phase >= 0; phase--) {
        if ((result=menu_transition_terminal(game,vga))!=0) goto done;
        detail_bottom_frame(vga->framebuffer, page->framebuffer, background, phase);
        game_present_copy(vga);
        if ((result=menu_transition_terminal(game,vga))!=0) goto done;
    }
    result = 0;
done:
    free(page);
    free(background);
    return result;
}

int fd2_field_game_cancel_selection(fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        progress_pending(game) ||
        game->player_post_waiting_phase) return 0;
    if (game->interaction == FD2_FIELD_INTERACTION_MANUAL_SLOT)
        return fd2_field_game_cancel_manual_slot_picker(game);
    if (game->interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU)
        return fd2_field_game_cancel_system_menu(game) !=
               FD2_FIELD_SYSTEM_ACTION_NONE;
    if (game->interaction == FD2_FIELD_INTERACTION_MAGIC_LIST)
        return fd2_field_game_end_magic_list(game) == 0;
    if (game->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING ||
        game->interaction == FD2_FIELD_INTERACTION_MAGIC_TELEPORTING)
        return fd2_field_game_cancel_magic_targeting(game) == 0;
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING)
        return fd2_field_game_cancel_item_targeting(game) == 0;
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_TELEPORTING)
        return fd2_field_game_cancel_item_targeting(game) == 0;
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING ||
        game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_LIST)
        return fd2_field_game_cancel_item_give(game) == 0;
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_LIST)
        return fd2_field_game_end_item_list(game) == 0;
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_MENU)
        return fd2_field_game_end_item_menu(game) == 0;
    if (game->selected_unit < 0) return 0;
    if (game->interaction == FD2_FIELD_INTERACTION_TARGETING)
        return fd2_field_game_cancel_attack(game);

    fd2_field_unit *unit = &game->units.items[game->selected_unit];
    if (game->interaction == FD2_FIELD_INTERACTION_COMMAND) {
        if (game->item_transfer_committed) {
            /* field_player_unit_action @0x18890（原登记同名），指令VA 0x18adb：
             * 53C53已置1时，移动与零步两路的COMMAND取消
             * 均markacted→lookup1→controller；不回滚坐标或执行WAIT恢复。 */
            size_t actor_index = (size_t)game->selected_unit;
            int result = finish_unit_action_internal(game, PLAYER_ACTION_ITEM, 0);
            if (result < 0 && game->failure == FD2_FIELD_FAILURE_NONE) fail_action(game, actor_index);
            return result == 0;
        }
        /* field_player_command_execute @0x18d8c 返回 -1 后，调用者恢复
         * 确认移动前的唯一坐标记录，再重新显示原移动范围。 */
        game->command_selected = -1;
        game->command_highlight_phase = 0;
        game->last_command_highlight_ms = 0;
        unit->x = (uint8_t)game->move_origin_x;
        unit->y = (uint8_t)game->move_origin_y;
        unit->direction = game->move_origin_direction;
        unit->frame_phase = 0;
        game->units.walk_frames[game->selected_unit] = 1;
        game->camera_cell_x = game->move_origin_camera_x;
        game->camera_cell_y = game->move_origin_camera_y;
        game->cursor_cell_x = game->move_origin_x;
        game->cursor_cell_y = game->move_origin_y;
        game->camera_pixel_offset_x = 0;
        game->camera_pixel_offset_y = 0;
        game->interaction = FD2_FIELD_INTERACTION_UNIT_SELECTED;
        /* 普通COMMAND取消返回0，controller重入18890后清pending。
         * 53C53已提交的Give例外已由上方分支独立处理。 */
        game->pending_cell_event.valid = 0;
        if (player_move_range(game, (size_t)game->selected_unit) != 0 ||
            fd2_field_game_update_move_preview(game) != 0) {
            clear_move_query(game);
            game->selected_unit = -1;
            game->interaction = FD2_FIELD_INTERACTION_BROWSE;
            return 0;
        }
        return 1;
    }

    if (game->interaction != FD2_FIELD_INTERACTION_UNIT_SELECTED)
        return 0;
    /* 18890@18993：selector Esc也清raw1高位/raw2控制位/raw3，随后才聚焦。 */
    if (fd2_field_event_map_reset_cells(&game->map) != 0) return 0;
    unit->direction = game->move_origin_direction;
    unit->frame_phase = 0;
    game->camera_cell_x = game->move_origin_camera_x;
    game->camera_cell_y = game->move_origin_camera_y;
    game->cursor_cell_x = game->move_origin_x;
    game->cursor_cell_y = game->move_origin_y;
    size_t actor_index = (size_t)game->selected_unit;
    clear_move_query(game);
    game->selected_unit = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    /* selection Esc是18890返回1，仍走controller后置；不acted、不lookup1。 */
    return fd2_field_game_complete_player_controller(game, actor_index) == 0;
}

typedef enum {
    FIELD_PATH_PLAYER_COMMAND,
    FIELD_PATH_BROWSE,
    FIELD_PATH_MARCH_BROWSE
} field_path_playback_mode;

static int execute_path_playback(fd2_field_game *game,
                                 fd2_vga *vga,
                                 uint32_t frame_delay_ms,
                                 field_path_playback_mode mode) {
    /* field_actor_path_play @0x13488 按 0下/1左/2上/3右分派四个
     * 单格 helper；每格 helper 写 direction，并用 1..6 相位各走 4 px。
     * 每相位VGA copy后按13460共享旧锚等tick变化；非0诊断参数启用原等待，
     * 不是从当前时刻新等固定毫秒或新鲜1tick。 */
    static const int dx[] = {0, -1, 0, 1};
    static const int dy[] = {1, 0, -1, 0};
    static const uint8_t walk_frames[] = {1, 2, 1, 0, 1, 2};
    if (!game || !game->ready || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count ||
        game->interaction != FD2_FIELD_INTERACTION_MOVING ||
        !game->move_preview_valid || !game->move_path)
        return -1;
    if(game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
    if(game->runtime_host_quit)return 1;

    fd2_field_unit *unit = &game->units.items[game->selected_unit];
    for (size_t step = 0; step < game->move_path_length; step++) {
        uint8_t direction = game->move_path[step];
        if (direction > 3) return -1;
        int camera_dx = 0;
        int camera_dy = 0;
        int rel_x = (int)unit->x - game->camera_cell_x;
        int rel_y = (int)unit->y - game->camera_cell_y;
        if (direction == 0 && rel_y >= 6 &&
            game->camera_cell_y < max_camera_y(game))
            camera_dy = 1;
        else if (direction == 1 && rel_x < 2 &&
                 game->camera_cell_x > 0)
            camera_dx = -1;
        else if (direction == 2 && rel_y < 2 &&
                 game->camera_cell_y > 0)
            camera_dy = -1;
        else if (direction == 3 && rel_x >= 11 &&
                 game->camera_cell_x < max_camera_x(game))
            camera_dx = 1;

        unit->direction = direction;
        for (size_t phase = 0; phase < sizeof(walk_frames); phase++) {
            /* 13185/12eaa/1300d/13315先water，再写步态并推进actor/tiles。 */
            if(vga && fd2_field_water_tick_live(vga)<0)return -1;
            game->units.walk_frames[game->selected_unit] = walk_frames[phase];
            unit->frame_phase = (uint8_t)phase + 1u;
            game->camera_pixel_offset_x =
                camera_dx * (int)(phase + 1u) * 4;
            game->camera_pixel_offset_y =
                camera_dy * (int)(phase + 1u) * 4;
            if (vga) {
                /* 四方向原helper每步态均1297d，再11eee/127a9；全局
                 * cursor相位从App借读，不用每格重播1,2,1,0,1,2代替。 */
                /* march的51A83=0取消选择层，51AAC=0关闭常驻信息面板。
                 * 仅本次私有路径参数控制，不留下session显示策略。 */
                if(vga->field_page) {
                    if(runtime_actor_phase(game,vga)!=0 ||
                        render_work_page(game,vga,mode==FIELD_PATH_MARCH_BROWSE?0:1,
                            mode!=FIELD_PATH_MARCH_BROWSE,1,game->idle_phase,game->terrain_visual_phase,game,game)!=0)
                        return -1;
                } else {
                    /* 无工作页的离屏诊断沿固定快照，不签正式App读钟。 */
                    if(fd2_field_game_tick_visual(game,SDL_GetTicksNS())!=0 ||
                        fd2_field_actor_visual_tick(vga,SDL_GetTicksNS())<0)return -1;
                    if(mode==FIELD_PATH_MARCH_BROWSE)fd2_field_game_render_background(game,vga);
                    else fd2_field_game_render(game,vga);
                }
                if(vga->field_page) {
                    game_present_copy(vga);
                    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return -1;
                }
                else if(fd2_vga_present(vga)<0)return -1; /* 无App诊断，不签正式提交时序。 */
                if(frame_delay_ms) {
                    int waited=fd2_vga_wait_movement_tick(vga);
                    if(waited<0 || game->failure!=FD2_FIELD_FAILURE_NONE)return -1;
                    if(waited>0){game->runtime_host_quit=1;return 1;}
                }
            }
        }

        /* field_actor_path_play @0x13488共享单格尾：坐标提交后仅lookup0；
         * 玩家与AI均不在途中dispatch，未命中保留最后pending。 */
        unit->x = (uint8_t)((int)unit->x + dx[direction]);
        unit->y = (uint8_t)((int)unit->y + dy[direction]);
        game->camera_cell_x += camera_dx;
        game->camera_cell_y += camera_dy;
        game->camera_pixel_offset_x = 0;
        game->camera_pixel_offset_y = 0;
        unit->frame_phase = 0;
        game->units.walk_frames[game->selected_unit] = 1;
        game->cursor_cell_x = unit->x;
        game->cursor_cell_y = unit->y;
        if (collect_pending_cell_event(
                game, (size_t)game->selected_unit, 0) < 0)
            return -1;
    }

    if (mode != FIELD_PATH_PLAYER_COMMAND) {
        game->interaction = FD2_FIELD_INTERACTION_BROWSE;
        return 0;
    }
    /* @0x18890 的完整非零路径之后才reset全表方向，再开COMMAND。 */
    if (game->move_path_length != 0) {
        int reset=run_turn_entry(game,FD2_FIELD_TURN_DIRECTIONS_RESET);
        if(reset<0)return -1;
        if(reset>0)return 0;
    }
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    return fd2_field_game_refresh_commands(game);
}

int fd2_field_game_execute_move(fd2_field_game *game,
                                fd2_vga *vga,
                                uint32_t frame_delay_ms) {
    if (!game || game->failure != FD2_FIELD_FAILURE_NONE || progress_pending(game)) return -1;
    return execute_path_playback(game, vga, frame_delay_ms, FIELD_PATH_PLAYER_COMMAND);
}

/* field_controller_input @0x117e7（原登记同名）在13565返回后读取
 * 当前共享slot；AI outer可以清空或覆盖它，不能保存玩家pending副本。 */
static int finish_player_controller_input(fd2_field_game *game,size_t actor_index) {
    if(game->runtime_host_quit)return 0;
    game->pending_cell_event.valid=0; /* 119a6先清共享slot，再到1187a全清输入。 */
    game->player_post_waiting_phase=0;game->player_post_actor=0;
    int result=turn_present(game,FD2_FIELD_RUNTIME_CLEAR_KEYS,0);
    if(result<0){fail_action(game,actor_index);return -1;}
    return 0; /* 正值由同owner runtime_host_quit传播，不能洗成业务ERROR。 */
}

static int dispatch_player_controller_pending(fd2_field_game *game,
                                               size_t actor_index) {
    if (dispatch_pending_cell_event(game, actor_index) != 0) return -1;
    if(game->event_resume.active) {
        game->event_resume.completion=FD2_EVENT_COMPLETE_PLAYER_PENDING;
        return 0;
    }
    return finish_player_controller_input(game,actor_index);
}

int fd2_field_game_resume_player_controller(fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE)
        return -1;
    if (game->runtime_host_quit || progress_pending(game)) return 1;
    if (game->turn_post_caller != FD2_FIELD_TURN_POST_NONE) {
        if (game->turn_post_caller != FD2_FIELD_TURN_POST_MARCH &&
            game->turn_post_caller != FD2_FIELD_TURN_POST_END) return -1;
        if(refresh_battle_result(game,game->player_post_actor)!=0)return -1;
        if (game->active_side != 2u && game->battle_result == FD2_FIELD_BATTLE_ONGOING) return 1;
        /* 17159只属于March；END原caller仅恢复51AAC，继承51A83。 */
        if (game->turn_post_caller == FD2_FIELD_TURN_POST_MARCH) game->ordinary_display_mode = 1;
        int restored=run_turn_entry(game,FD2_FIELD_TURN_CALLER_RETURN);
        if(restored<0){fail_action(game,0);return -1;}
        if(restored>0)return 1;
        game->turn_post_caller = FD2_FIELD_TURN_POST_NONE;
        return 0;
    }
    if (!game->player_post_waiting_phase) return 0;
    /* native turncycle也可因stage结果在side1/0提前返回controller。 */
    if(refresh_battle_result(game,game->player_post_actor)!=0)return -1;
    if (game->active_side != 2u &&
        game->battle_result == FD2_FIELD_BATTLE_ONGOING)
        return 1;
    game->ordinary_display_mode = 1; /* 135ca：整轮返回，即使AI阶段早胜。 */
    int restored=run_turn_entry(game,FD2_FIELD_TURN_CALLER_RETURN);
    if(restored<0){fail_action(game,game->player_post_actor);return -1;}
    if(restored>0)return 1;
    return dispatch_player_controller_pending(game, game->player_post_actor);
}

int fd2_field_game_complete_player_controller(fd2_field_game *game,
                                               size_t actor_index) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        !fd2_field_stage_battle_is_supported(game->stage) ||
        progress_pending(game) ||
        actor_index >= game->units.count ||
        game->player_post_waiting_phase || game->active_side != 2 ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE) return -1;
    if (game->experience_value <= INT32_MAX && game->experience_value > 99u)
        game->experience_value = 99u;
    int reward = begin_reward(game, actor_index, FD2_FIELD_PROGRESS_RESUME_PLAYER, actor_index);
    if (reward < 0) { fail_action(game, actor_index); return -1; }
    return reward ? 0 : complete_player_controller_tail(game, actor_index);
}

static int complete_player_controller_tail(fd2_field_game *game, size_t actor_index) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        game->player_post_waiting_phase || actor_index >= game->units.count ||
        game->active_side != 2u ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE)
        return -1;
    /* 原117e7在奖励关闭后再进stage结果/整轮控制器。 */
    int result=begin_stage_result_call(game,actor_index,FD2_EVENT_COMPLETE_PLAYER_RESULT);
    if(result<0)return -1;
    if(result>0)return 0;
    return complete_player_controller_after_result(game,actor_index);
}

/* 1197b、1a866、1d80b/1d8ba共用真实分派；PC/actor仍在原owner中。
 * 1=源结果函数等待呈现，0=已经返回；const getter永不执行此入口。 */
static int begin_stage_result_call(fd2_field_game *game,size_t actor,unsigned completion) {
    const fd2_static_flow *flow=stage_result_event_flow(game->stage);
    if(!flow)return refresh_battle_result(game,actor);
    if(common_event_bound_begin(game,flow,actor)!=0)return -1;
    if(game->event_resume.active){game->event_resume.completion=(uint8_t)completion;return 1;}
    return 0;
}

static void log_stage_result_return(const fd2_field_game *game,const char *caller,size_t actor) {
    printf("field result query: stage=%zu result=%d caller=%s actor=%zu\n",
        game->stage,(int)game->battle_result,caller,actor);fflush(stdout);
}

static int finish_ai_outer_after_result(fd2_field_game *game,size_t actor) {
    log_stage_result_return(game,"ai",actor);
    if(game->battle_result!=FD2_FIELD_BATTLE_ONGOING &&
        fd2_field_game_resume_player_controller(game)<0)return -1;
    return 0;
}

static int finish_ai_outer_result(fd2_field_game *game,size_t actor) {
    int result=begin_stage_result_call(game,actor,FD2_EVENT_COMPLETE_AI_RESULT);
    return result<0?-1:result?0:finish_ai_outer_after_result(game,actor);
}

/* 1d80b/1d8ba的gate只跳AI本体，不跳结果表。纯无写图在连续跳过的
 * raw状态不变时沿旧调度合并；含写/等待图每个raw均真正调用一次。 */
static int skip_ai_outer_result(fd2_field_game *game,size_t actor) {
    if(!stage_result_event_flow(game->stage))return 0;
    game->phase_unit_cursor=actor+1u;
    if(finish_ai_outer_result(game,actor)<0){fail_action(game,actor);return -1;}
    return game->event_resume.active || game->runtime_host_quit || game->battle_result!=FD2_FIELD_BATTLE_ONGOING;
}

/* 原1197b返回后才到13565/pending；story ACK续此处，不重新查询结果。 */
static int complete_player_controller_after_result(fd2_field_game *game,size_t actor_index) {
    if (!game || !game->ready || game->failure!=FD2_FIELD_FAILURE_NONE ||
        actor_index>=game->units.count || game->active_side!=2u ||
        game->interaction!=FD2_FIELD_INTERACTION_BROWSE) return -1;
    log_stage_result_return(game,"player",actor_index);
    /* sub_13565 @0x13565（原登记同名）：不含HP与bit04门。 */
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if ((unit->flags & 0x81u) == 0u && unit->side == 2u &&
            unit->detail_status[1] == 0u)
            return dispatch_player_controller_pending(game, actor_index);
    }
    /* 仅异步整轮phase需要续行上下文；零初始化waiting=0不指向actor0。 */
    game->player_post_actor = actor_index;
    game->player_post_waiting_phase = 1;
    game->ordinary_display_mode = 0; /* 13565整轮入口；显式END没有此恢复owner。 */
    advance_phase(game);
    return fd2_field_game_resume_player_controller(game) < 0 ? -1 : 0;
}

/* field_special_cell_execute @0x190ac（原sub_190AC）：先验证全部可能提示和全图12263写集，
 * 首次WAIT恢复/RNG之后才进入CHOICE；No也正常完成WAIT。 */
static int loot_flow_preflight_cell(fd2_field_game *game, size_t actor_index);

static int loot_flow_finish_unit_action(fd2_field_game *game, player_action_kind kind,
        uint64_t raw_reward);
static int finish_unit_action_internal(fd2_field_game *game,
        player_action_kind kind, uint64_t magic_raw_reward) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        progress_pending(game) || game->player_post_waiting_phase || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count ||
        game->interaction != FD2_FIELD_INTERACTION_COMMAND) return -1;
    return loot_flow_finish_unit_action(game, kind, magic_raw_reward);
}

int fd2_field_game_finish_unit_action(fd2_field_game *game) {
    return finish_unit_action_internal(game, PLAYER_ACTION_WAIT, 0);
}


/* field_loot_descriptors_execute @0x1aa1d（原sub_1AA1D） / field_special_cell_execute @0x190ac（原sub_190AC）：业务停点保留在原写入边界。
 * 不把UI等键、全屏剧情或背包选择伪装成同步副作用。 */
enum { LOOT_SOURCE_DEFEAT = 1, LOOT_SOURCE_CELL = 2, LOOT_SOURCE_AI_REMAINDER = 3 };
#include "field_loot_flow.h"
#include "field_common_event_runtime.h"

/* SDL提交前的只读资源检查；与12263实际写回共用，不创建第二张地图owner。 */
static int completed_cell_tiles_preflight(const fd2_field_game *resources,
        const fd2_field_map *map, const uint8_t completed[FD2_SAVE_BATTLE_CELL_STATE_SIZE]) {
    if (!resources || !map || !map->cells || !completed || !resources->terrain.attrs ||
        map->width <= 0 || map->height <= 0 || (size_t)map->width > SIZE_MAX / (size_t)map->height) return -1;
    const fd2_terrain_tileset *terrain = &resources->terrain;
    size_t count = (size_t)map->width * (size_t)map->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t cell = map->cells[i];
        uint16_t id = fd2_field_cell_terrain(cell);
        if (id >= terrain->attr_count) return -1;
        if ((terrain->attrs[id].flags & 0x60u) == 0x20u && completed[(cell >> 16) & 31u]) {
            uint32_t changed = (cell & UINT32_C(0xff000000)) | (uint16_t)((uint16_t)cell + 1u);
            size_t frame;
            if (fd2_terrain_base_frame_from_cell(terrain, changed, &frame) != 0) return -1;
        }
    }
    return 0;
}

/* field_completed_chest_tiles_apply @0x12263（原sub_12263）：仅flags20地形，WORD模加1、byte2清0，byte3保留。 */
int fd2_field_game_apply_completed_cells(const fd2_field_game *resources,
        fd2_field_map *map, const uint8_t completed[FD2_SAVE_BATTLE_CELL_STATE_SIZE]) {
    if (completed_cell_tiles_preflight(resources, map, completed) != 0) return -1;
    const fd2_terrain_tileset *terrain = &resources->terrain;
    size_t count = (size_t)map->width * (size_t)map->height;
    for (size_t i = 0; i < count; i++) {
        uint32_t cell = map->cells[i];
        uint16_t id = fd2_field_cell_terrain(cell);
        if ((terrain->attrs[id].flags & 0x60u) == 0x20u && completed[(cell >> 16) & 31u])
            map->cells[i] = (cell & UINT32_C(0xff000000)) | (uint16_t)((uint16_t)cell + 1u);
    }
    return 0;
}

static int complete_cell_tiles(fd2_field_game *game) {
    return fd2_field_game_apply_completed_cells(game, &game->map, game->cell_action_completed);
}

int fd2_field_game_end_active_phase(fd2_field_game *game) {
    /* 快速逻辑回归专用的 phase skip；正式玩家输入不调用此全 acted 路径。 */
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        progress_pending(game) ||
        game->player_post_waiting_phase || game->selected_unit >= 0 ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE)
        return -1;
    for (size_t i = 0; i < game->units.count; i++) {
        if (unit_can_act_for_side(&game->units.items[i], game->active_side))
            fd2_field_unit_set_acted(&game->units.items[i], 1);
    }
    advance_phase(game);
    return game->failure == FD2_FIELD_FAILURE_NONE ? 0 : -1;
}

int fd2_field_game_end_player_turn(fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        !fd2_field_stage_battle_is_supported(game->stage) ||
        progress_pending(game) ||
        game->active_side != 2u || game->player_post_waiting_phase ||
        game->selected_unit >= 0 ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE)
        return -1;
    /* field_empty_focus_menu_execute @0x16f55（原登记同名）的 selector 3
     * 确认后直接进入 turn_cycle，不先给全部玩家设置 acted。 */
    game->turn_post_caller=FD2_FIELD_TURN_POST_END;
    advance_phase(game);
    return game->failure == FD2_FIELD_FAILURE_NONE ?
        (fd2_field_game_resume_player_controller(game)<0 ? -1:0) : -1;
}

static int finish_ai_action(fd2_field_game *game, size_t actor_index) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        progress_pending(game) ||
        actor_index >= game->units.count)
        return -1;
    fd2_field_unit *actor = &game->units.items[actor_index];
    /* field_physical_exchange 可在反击中让 actor 死亡并先执行 defeated
     * finalize；原版 AI common tail 仍无条件处理该 actor。这里不能因
     * hidden 拒绝，否则会在 RNG 已消费后错误回滚棋盘。 */
    if (actor->side != game->active_side)
        return -1;
    /* field_ai_unit_execute_core @0x13a9f：lookup1命中才覆盖路径pending，
     * 再 OR acted；未知handler尚未dispatch，不能在lookup阶段失败。
     * 随后调用 field_all_actor_directions_reset @0x134e4。record +0x03
     * 的原版字段在静止状态确实统一归零（默认朝下）；这不是独立的
     * selection 缓冲。原helper的20ms及随后11cac(0)来自runtime冷序；
     * headless无callback只执行状态，不以动作帧延迟替代这次独立等待。 */
    (void)collect_pending_cell_event(game, actor_index, 1);
    fd2_field_unit_set_acted(actor, 1);
    if(run_turn_entry(game,FD2_FIELD_TURN_DIRECTIONS_RESET_REFRESH)<0)return -1;
    if(game->runtime_host_quit)return 0;
    game->selected_unit = -1;
    game->command_selected = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    return game->failure == FD2_FIELD_FAILURE_NONE ? 0 : -1;
}

/* 直接commit API保留“完整一名AI动作”的现有契约；正式scheduler用内部
 * 未finish版本，确保tail与outer dispatch各执行一次。 */
static int finish_and_dispatch_ai_action(fd2_field_game *game, size_t actor_index) {
    if (finish_ai_action(game, actor_index) != 0) return -1;
    if(game->runtime_host_quit)return 0;
    return dispatch_pending_cell_event(game, actor_index);
}

static int physical_plan_matches(
        const fd2_field_ai_physical_candidate *a,
        const fd2_field_ai_physical_candidate *b) {
    return a && b && a->unit_index == b->unit_index &&
        a->destination_x == b->destination_x &&
        a->destination_y == b->destination_y &&
        a->priority == b->priority &&
        a->secondary_score == b->secondary_score;
}

static int ai_path_toward_anchor(const fd2_field_game *game,
                                   size_t actor_index,
                                   int anchor_x, int anchor_y,
                                   uint8_t *path, size_t count,
                                   size_t *path_length);

static int commit_ai_physical_internal(
        fd2_field_game *game,
        size_t actor_index,
        uint8_t side_selector,
        const fd2_field_ai_physical_candidate *plan,
        fd2_vga *vga,
        uint32_t frame_delay_ms,
        int finish_action, field_path_playback_mode mode) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        progress_pending(game) ||
        !plan || !game->attack_rng ||
        game->active_side == 2u || actor_index >= game->units.count ||
        plan->unit_index >= game->units.count || game->selected_unit >= 0 ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE)
        return -1;
    fd2_field_unit *actor = &game->units.items[actor_index];
    if (!unit_can_act_for_side(actor, game->active_side) ||
        fd2_field_unit_has_acted(actor) ||
        plan->priority < 6 ||
        (side_selector == 0u
             ? game->units.items[plan->unit_index].side == 0u
             : game->units.items[plan->unit_index].side != 0u))
        return -1;

    /* field_ai_physical_candidate_evaluate @0x14237 的输出只是计划；提交前
     * 以当前 session 状态重算，防止坐标、装备或目标在查询后改变。 */
    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    if (fd2_field_game_compute_move_range(game, actor_index, &range) != 0)
        return -1;
    fd2_field_ai_physical_candidate current;
    int choose_result = fd2_field_ai_choose_physical_candidate(
        &game->map, &game->terrain, &game->units, actor_index,
        side_selector, &range, &current);
    if (choose_result != 0 || !physical_plan_matches(&current, plan) ||
        !fd2_field_path_is_destination(
            &range, plan->destination_x, plan->destination_y)) {
        fd2_field_path_close(&range);
        return -1;
    }

    size_t node_count = (size_t)game->map.width * (size_t)game->map.height;
    if (node_count == 0 || node_count > SIZE_MAX / sizeof(uint8_t)) {
        fd2_field_path_close(&range);
        return -1;
    }
    uint8_t *directions = malloc(node_count);
    if (!directions) {
        fd2_field_path_close(&range);
        return -1;
    }
    /* 154c6走完整14b78：用其text_id/immune策略及终点选择，不能把
     * 14237候选范围的previous树或profile当成正式路径。 */
    size_t path_length = 0;
    int path_found = ai_path_toward_anchor(game, actor_index,
        plan->destination_x, plan->destination_y, directions, node_count, &path_length);
    fd2_field_path_close(&range);
    if (path_found != 1) {
        free(directions);
        return -1;
    }

    /* 任何可能失败的 combat/profile hook 都要在动画前拒绝；外部 RNG
     * 没有 checkpoint，故后续 exchange 只进入预检已通过的确定性路径。 */
    if (physical_exchange_preflight(
            game, actor_index, plan->unit_index, 1) != 0 ||
        physical_reward_preflight(game, actor_index, plan->unit_index, 0) != 0) {
        free(directions);
        return -1;
    }

    /* execute_path_playback 可修改镜头、cursor、walk frame 和 event log；
     * 用完整 session 旁路快照回滚，不能只恢复单位表。资源指针在这条
     * 事务中不转移所有权，directions 在恢复前单独释放。 */
    fd2_field_game session_snapshot = *game;
    /* field_physical_exchange @0x1548e（原登记同名）先12d7b聚焦进攻者，
     * 移动后再聚焦目标；只赋cursor会漏掉12cea的逐格镜头卷动。 */
    if (focus_to(game, actor->x, actor->y) != 0) {
        if(game->failure==FD2_FIELD_FAILURE_NONE)*game = session_snapshot;
        free(directions);
        return -1;
    }
    if(game->runtime_host_quit){free(directions);return 0;}
    if (finish_action) game->pending_cell_event.valid = 0;
    /* 14eaf清图在focus actor之后、首个步态之前；HOST在focus处返回不越过它。 */
    if (fd2_field_event_map_reset_cells(&game->map) != 0) {
        free(directions); return -1;
    }

    /* field_ai_move_toward_cell @0x14b78 最终把重建路径交给
     * field_actor_path_play @0x13488；SDL 复用同一六相位 path player。 */
    game->selected_unit = (int)actor_index;
    game->move_origin_x = actor->x;
    game->move_origin_y = actor->y;
    game->move_origin_direction = actor->direction;
    game->move_origin_camera_x = game->camera_cell_x;
    game->move_origin_camera_y = game->camera_cell_y;
    game->cursor_cell_x = plan->destination_x;
    game->cursor_cell_y = plan->destination_y;
    game->move_path = directions;
    game->move_path_capacity = node_count;
    game->move_path_length = (size_t)path_length;
    game->move_preview_valid = 1;
    game->interaction = FD2_FIELD_INTERACTION_MOVING;
    /* field_ai_move_toward_cell @0x14b78 只播放 actor path；不打开
     * player COMMAND 菜单，也不更新 command 的旁路动画状态。 */
    int commit_result = execute_path_playback(
        game, vga, frame_delay_ms, mode);
    int path_host=commit_result>0 && game->runtime_host_quit &&
        game->failure==FD2_FIELD_FAILURE_NONE;
    if (commit_result == 0) {
        fd2_field_unit *target = &game->units.items[plan->unit_index];
        if (actor->hp == 0 || target->hp == 0 ||
            fd2_field_unit_is_hidden(actor) ||
            fd2_field_unit_is_hidden(target)) {
            commit_result = -1;
        } else {
            /* 154c6的14b78可用另一profile选择实际落点；1548e之后直接
             * 聚焦原已选目标并交换，不再用当前位置重验攻击范围。
             * 首次candidate/stale、HP/hidden、资源预检仍各归原入口。 */
            if (commit_result == 0 && focus_to(game, target->x, target->y) != 0)
                commit_result = -1;
            if(commit_result==0 && game->runtime_host_quit){path_host=1;goto path_cleanup;}
            actor->direction = face_toward(actor, target); /* 1f04a */
            if (commit_result == 0) {
                game->interaction = FD2_FIELD_INTERACTION_COMMAND;
                commit_result = resolve_physical_exchange(
                    game, actor_index, plan->unit_index, 0);
            }
        }
    }
path_cleanup:
    if (commit_result == 0 || path_host) {
        /* directions 为本次事务私有；不能把已释放的路径指针留在 session。 */
        game->move_path = session_snapshot.move_path;
        game->move_path_capacity = session_snapshot.move_path_capacity;
        game->move_path_length = session_snapshot.move_path_length;
        game->move_preview_valid = session_snapshot.move_preview_valid;
        /* execute_path_playback 清理前仍保存 directions；恢复原始指针后
         * 释放一次，避免 close session 时释放同一地址两次。 */
        free(directions);
        /* HOST只清借用路径；保已走格/步态与同App退出锁，不进攻击或tail。 */
        if(game->runtime_host_quit)return game->failure==FD2_FIELD_FAILURE_NONE?0:-1;
        if (progress_pending(game)) {
            if (finish_action) {
                set_ai_continuation(game, FD2_FIELD_PROGRESS_RESUME_AI_DIRECT, actor_index, 1);
            }
            return 0;
        }
        if (!finish_action || finish_and_dispatch_ai_action(game, actor_index) == 0)
            return 0;
        fail_action(game, actor_index);
        return -1;
    } else {
        /* session 仍指向 directions，先恢复后再释放该临时缓冲。 */
        if (game->failure == FD2_FIELD_FAILURE_NONE) {
            *game = session_snapshot;
        } else {
            game->move_path = session_snapshot.move_path;
            game->move_path_capacity = session_snapshot.move_path_capacity;
            game->move_path_length = session_snapshot.move_path_length;
            game->move_preview_valid = session_snapshot.move_preview_valid;
        }
        free(directions);
        return -1;
    }
}

int fd2_field_game_commit_ai_physical(
        fd2_field_game *game, size_t actor_index, uint8_t side_selector,
        const fd2_field_ai_physical_candidate *plan,
        fd2_vga *vga, uint32_t frame_delay_ms) {
    return commit_ai_physical_internal(game, actor_index, side_selector,
                                       plan, vga, frame_delay_ms, 1, FIELD_PATH_BROWSE);
}

static int magic_plan_matches(
        const fd2_field_ai_magic_candidate *a,
        const fd2_field_ai_magic_candidate *b) {
    return a && b && a->magic_id == b->magic_id &&
        a->cast_x == b->cast_x && a->cast_y == b->cast_y &&
        a->score == b->score && a->tie_value == b->tie_value;
}

static int item_plan_matches(
        const fd2_field_ai_item_candidate *a,
        const fd2_field_ai_item_candidate *b) {
    return a && b && a->item_id == b->item_id &&
        a->slot_index == b->slot_index &&
        a->target_x == b->target_x && a->target_y == b->target_y &&
        a->score == b->score;
}

static int commit_ai_magic_internal(
        fd2_field_game *game,
        size_t actor_index,
        uint8_t selector_mode,
        const fd2_field_ai_magic_candidate *plan,
        int finish_action) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        progress_pending(game) ||
        !plan || !game->attack_rng ||
        actor_index >= game->units.count || game->active_side == 2u ||
        game->selected_unit >= 0 ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE)
        return -1;
    fd2_field_unit *actor = &game->units.items[actor_index];
    const uint8_t *record = fd2_field_magic_record_get(plan->magic_id);
    if (!unit_can_act_for_side(actor, game->active_side) ||
        fd2_field_unit_has_acted(actor) || !record || plan->score < 6 ||
        actor->mp < record[5])
        return -1;
    fd2_field_ai_magic_candidate current;
    if (fd2_field_ai_choose_magic_candidate(
            &game->map, &game->terrain, &game->units, actor_index,
            selector_mode, &current) != 0 ||
        !magic_plan_matches(&current, plan))
        return -1;

    uint8_t targets[FD2_FIELD_MAX_UNITS];
    size_t target_count = 0;
    if (fd2_field_ai_collect_magic_targets(
            &game->map, &game->terrain, &game->units,
            plan->magic_id, plan->cast_x, plan->cast_y, selector_mode,
            targets, sizeof(targets), &target_count) != 0 ||
        target_count == 0)
        return -1;
    /* 特殊 handler 在修改状态前显式拒绝；不把部分支持误装成 fallback。 */
    if (plan->magic_id == 23u || plan->magic_id == 24u ||
        plan->magic_id >= 28u)
        return -1;
    /* MAP9只提交首raw；FX仍接完整目标组，不能因后项无业务读取的
     * profile而拒绝这个原有效域。FULL9资源域另由FULL预检负责。 */
    size_t effect_count = target_count;
    int full = plan->magic_id < 10u && !game->option_values[2];
    fd2_field_magic_map_profile profile;
    if (!full && fd2_field_magic_map_profile_get(plan->magic_id, &profile) == 0 &&
        profile.target_limit && effect_count > profile.target_limit) effect_count = profile.target_limit;
    if (plan->magic_id <= 12u) {
        for (size_t i = 0; i < effect_count; i++) {
            uint32_t ignored_scale = 0;
            if (fd2_field_magic_damage_profile_scale(
                    game->units.items[targets[i]].movement_profile,
                    &ignored_scale) != 0)
                return -1;
        }
    }

    for (size_t i = 0; i < effect_count; i++)
        if (fd2_field_magic_effect_reward_preflight(plan->magic_id, &game->units.items[targets[i]]) != 0) return -1;
    if (!effect_deaths_are_supported(game,actor_index,targets,effect_count,plan->magic_id,0,1)) return -1;
    /* field_ai_magic_action_execute @0x15311（原登记同名）：15360先聚焦
     * caster，153c2再12cea到cast中心，均在费用／首效果RNG之前。 */
    if(focus_to(game,actor->x,actor->y)!=0)return -1;
    if(game->runtime_host_quit)return 0;
    game->ordinary_display_mode=(uint8_t)(record[4]+2u);
    if(focus_to(game,plan->cast_x,plan->cast_y)!=0)return -1;
    if(game->runtime_host_quit)return 0;
    game->ordinary_display_mode = 0; /* AI15311不自行恢复1，END继续继承0。 */
    if (finish_action) game->pending_cell_event.valid = 0;
    if (full) {
        if (execute_full_magic(game, actor_index, plan->magic_id, targets, target_count, 0) != 0) {
            if (game->failure == FD2_FIELD_FAILURE_NONE) fail_action(game, actor_index);
            return -1;
        }
    } else {
        if (execute_map_magic(game, actor_index, plan->magic_id, targets, target_count, 0) != 0) {
            if (game->failure == FD2_FIELD_FAILURE_NONE) fail_action(game, actor_index);
            return -1;
        }
    }
    if(plan->magic_id>=10u && plan->magic_id<=12u && game->runtime_host_quit)return 0;
    int loot_result = begin_defeated_loot(game, actor_index, actor_index, PLAYER_ACTION_AI_MAGIC, 0, 1);
    if (loot_result < 0) { fail_action(game, actor_index); return -1; }
    if (loot_result) {
        if (finish_action) { game->loot.resume = FD2_FIELD_PROGRESS_RESUME_AI_DIRECT; game->loot.ai_common_tail = 1; }
        return 0;
    }
    game->experience_value = 0; /* AI15311指令VA0x1546a：wrapper/finalize/loot之后。 */
    if (finish_action && finish_and_dispatch_ai_action(game, actor_index) != 0) {
        fail_action(game, actor_index);
        return -1;
    }
    return 0;
}

int fd2_field_game_commit_ai_magic(
        fd2_field_game *game,
        size_t actor_index,
        uint8_t selector_mode,
        const fd2_field_ai_magic_candidate *plan) {
    return commit_ai_magic_internal(
        game, actor_index, selector_mode, plan, 1);
}

static void inventory_slot_remove(fd2_field_unit *unit, size_t slot_index) {
    (void)fd2_field_unit_inventory_slot_remove(unit,slot_index);
}

/* field_ai_item_action_execute @0x15055（原登记同名）：15124之后的有限
 * common/ray caller顺序由ai.item.ray冷图驱动。几何仍只走focus_to；
 * 2dfc8/2189a只借同步呈现叶，不重放后面的20c6f业务与浮字。
 * 149f8原count经1511c EDI传到2dfc8，故直接借同一targets/count。
 * F2在正式caller发现并修复149f8旧反向扫描；该玩法依赖不冒充F3已完。 */
static int ai_item_caller_present(fd2_field_game *game,size_t actor_index,
        const fd2_field_ai_item_candidate *plan,const uint8_t *item,
        const uint8_t *targets,size_t target_count) {
    const fd2_static_flow *flow=fd2_static_flow_lookup("progress","ai.item.ray");
    if(!flow || !target_count)return -1;
    fd2_field_unit *actor=&game->units.items[actor_index];
    const fd2_field_unit *first=&game->units.items[targets[0]];
    int pc=flow->entry;
    for(unsigned budget=0;budget<FD2_FLOW_STEP_BUDGET;budget++) {
        const fd2_static_flow_node *node=fd2_static_flow_at(flow,pc);
        if(!node)break;
        const char *op=node->op;int value=node->args[0],result=0;
        unsigned port=FD2_FLOW_NEXT;
        if(!strcmp(op,"ray_cells_clear"))result=fd2_field_event_map_reset_cells(&game->map);
        else if(!strcmp(op,"ray_delay_ms"))result=turn_present(game,FD2_FIELD_RUNTIME_DELAY_MS,value);
        else if(!strcmp(op,"ray_area_display"))game->ordinary_display_mode=(uint8_t)(item[0x12u]+2u);
        else if(!strcmp(op,"ray_is_long"))port=item[0x10u]>15u?FD2_FLOW_YES:FD2_FLOW_NO;
        else if(!strcmp(op,"ray_face_first")) {
            actor->direction=face_toward(actor,first); /* 1516e→1f04a */
        } else if(!strcmp(op,"ray_refresh"))result=turn_present(game,FD2_FIELD_RUNTIME_REFRESH,value);
        else if(!strcmp(op,"ray_wait_ticks"))result=turn_present(game,FD2_FIELD_RUNTIME_WAIT_TICKS,value);
        else if(!strcmp(op,"ray_present")) {
            if(game->magic_present) {
                fd2_field_magic_present_event event={0};
                event.phase=FD2_FIELD_MAGIC_PRESENT_AI_RAY_ITEM;
                event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;
                event.action_kind=FD2_FIELD_MAGIC_ACTION_ITEM;
                event.actor_index=actor_index;event.actor_before=actor;
                event.magic_id=item[0x0eu];event.effect_id=event.magic_id;
                event.item_code=item[0x0du];
                event.item_power=(uint16_t)item[0x0eu]|((uint16_t)item[0x0fu]<<8);
                event.targets=targets;event.target_count=target_count;
                result=game->magic_present(game,&event,game->magic_present_userdata);
                if(result>0)game->runtime_host_quit=1;
            }
        } else if(!strcmp(op,"ray_display")) {
            if(value!=0 && value!=6)break;
            game->ordinary_display_mode=(uint8_t)value;
        } else if(!strcmp(op,"ray_range")) {
            if(value<0 || value>8)break;
            game->range_visual_phase=(uint8_t)value;
            if(game->visual_owner)game->visual_owner->range_phase=(uint32_t)value;
        } else if(!strcmp(op,"ray_focus")) {
            int x=plan->target_x,y=plan->target_y;
            if(value==1) {
                int length=item[0x10u]-16;
                x=clamp_int(actor->x+(plan->target_x-actor->x)*length,0,game->map.width-1);
                y=clamp_int(actor->y+(plan->target_y-actor->y)*length,0,game->map.height-1);
            } else if(value==2) {x=first->x;y=first->y;}
            else if(value!=0)break;
            result=focus_to(game,x,y);
        } else if(!strcmp(op,"ray_done"))return 0;
        else break;
        /* 真错误优先；HOST保已完成方向/几何/视觉前缀，后面不做effect/acted。 */
        if(result<0 || game->failure!=FD2_FIELD_FAILURE_NONE)break;
        if(game->runtime_host_quit)return 0;
        pc=fd2_static_flow_edge(flow,node,port);
    }
    if(game->failure==FD2_FIELD_FAILURE_NONE)fail_action(game,actor_index);
    return -1;
}

static int commit_ai_item_internal(
        fd2_field_game *game,
        size_t actor_index,
        uint8_t selector_mode,
        const fd2_field_ai_item_candidate *plan,
        int finish_action) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        progress_pending(game) ||
        !plan || !game->attack_rng ||
        actor_index >= game->units.count || game->active_side == 2u ||
        game->selected_unit >= 0 ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE)
        return -1;
    fd2_field_unit *actor = &game->units.items[actor_index];
    if (!unit_can_act_for_side(actor, game->active_side) ||
        fd2_field_unit_has_acted(actor) || plan->score < 6 ||
        plan->slot_index >= 8u)
        return -1;
    fd2_field_ai_item_candidate current;
    if (fd2_field_ai_choose_item_candidate(
            &game->map, &game->terrain, &game->units, actor_index,
            selector_mode, &current) != 0 ||
        !item_plan_matches(&current, plan))
        return -1;
    const uint8_t *item = fd2_field_item_record_get(plan->item_id);
    uint8_t code = item ? item[0x0du] : 0u;
    uint16_t value = item ? (uint16_t)(item[0x0eu] |
        ((unsigned)item[0x0fu] << 8)) : 0;
    /* 0x20c6f 的物品 dispatcher：AI 评分涉及 code 5/13/20/21/24。
     * code 5/13 先共用 0x211a4，逐目标以 item value 调用恢复 core
     * 0x1c916；只有 code 5 随后在 0x20d24 移除并压紧原 slot。
     * code 20/24 共用 0x20f6d 分支；code 21 调用 0x2111a，三者最后
     * 都逐目标进入 field_magic_damage_profile_apply @正确 dual 0x1c75e。
     * item value 是传给该 core 的 magic ID，code 13/20/21/24 均不消耗。 */
    if (!item || (code != 5u && code != 13u && code != 20u &&
                  code != 21u && code != 24u))
        return -1;

    uint8_t targets[FD2_FIELD_MAX_UNITS];
    size_t target_count = 0;
    if (fd2_field_ai_collect_item_targets(
            &game->map, &game->terrain, &game->units, actor_index,
            plan->item_id, plan->target_x, plan->target_y, selector_mode,
            targets, sizeof(targets), &target_count) != 0 ||
        target_count == 0)
        return -1;
    if (code == 20u || code == 21u || code == 24u) {
        if (value >= FD2_FIELD_MAGIC_COUNT || value > 12u) return -1;
        for (size_t i = 0; i < target_count; i++) {
            uint32_t ignored_scale = 0;
            if (fd2_field_magic_damage_profile_scale(
                    game->units.items[targets[i]].movement_profile,
                    &ignored_scale) != 0)
                return -1;
        }
    }

    for (size_t i = 0; i < target_count; i++) {
        const fd2_field_unit *target = &game->units.items[targets[i]];
        if (code == 5 || code == 13) {
            if (target->unit_id < 75u && !target->hp_max) return -1;
        } else if (fd2_field_magic_effect_reward_preflight((uint8_t)value, target) != 0) return -1;
    }
    if (!effect_deaths_are_supported(game,actor_index,targets,target_count,(uint8_t)value,code==5u||code==13u,0)) return -1;
    /* 15055先focus caster；射线的两次真实尾focus不能提前到2dfc8之前。 */
    if(item[0x12u]>4u || focus_to(game,actor->x,actor->y)!=0)return -1;
    if(game->runtime_host_quit)return 0;
    if(ai_item_caller_present(game,actor_index,plan,item,targets,target_count)!=0)return -1;
    if(game->runtime_host_quit)return 0;
    if (finish_action) game->pending_cell_event.valid = 0;
    if (execute_item_effect(game, actor_index, plan->slot_index, item,
            targets, target_count, NULL) != 0) return -1;
    if(game->runtime_host_quit)return 0;
    /* field_ai_item_action_execute @0x15055（原登记同名）在20c6f返回后
     * 独立调用134e4；不是稍后13a9f的第二次common reset/refresh。 */
    if(run_turn_entry(game,FD2_FIELD_TURN_DIRECTIONS_RESET)<0)return -1;
    if(game->runtime_host_quit)return 0;
    int loot_result = begin_defeated_loot(game, actor_index, actor_index, PLAYER_ACTION_AI_ITEM, 0, 0);
    if (loot_result < 0) { fail_action(game, actor_index); return -1; }
    /* field_loot_descriptors_execute @0x1aa1d（原sub_1AA1D）的非side2 mode0/1
     * 可同步终止队列，只有待续态才延后common tail。 */
    if (loot_result && progress_pending(game)) {
        if (finish_action) { game->loot.resume = FD2_FIELD_PROGRESS_RESUME_AI_DIRECT; game->loot.ai_common_tail = 1; }
        return 0;
    }
    if (finish_action && finish_and_dispatch_ai_action(game, actor_index) != 0) {
        fail_action(game, actor_index);
        return -1;
    }
    return 0;
}

int fd2_field_game_commit_ai_item(
        fd2_field_game *game, size_t actor_index, uint8_t selector_mode,
        const fd2_field_ai_item_candidate *plan) {
    return commit_ai_item_internal(game, actor_index, selector_mode, plan, 1);
}

static int commit_ai_path(fd2_field_game *game,
                          size_t actor_index,
                          uint8_t *path,
                          size_t path_capacity,
                          size_t path_length,
                          fd2_vga *vga,
                          uint32_t frame_delay_ms,
                          field_path_playback_mode mode) {
    if (!game || actor_index >= game->units.count ||
        (path_length != 0 && !path) || path_length > path_capacity)
        return -1;
    if (path_length == 0) return 0;
    fd2_field_game snapshot = *game;
    fd2_field_unit *actor = &game->units.items[actor_index];
    int start_x = actor->x;
    int start_y = actor->y;
    game->selected_unit = (int)actor_index;
    game->move_origin_x = actor->x;
    game->move_origin_y = actor->y;
    game->move_origin_direction = actor->direction;
    game->move_origin_camera_x = game->camera_cell_x;
    game->move_origin_camera_y = game->camera_cell_y;
    game->move_path = path;
    game->move_path_capacity = path_capacity;
    game->move_path_length = path_length;
    game->move_preview_valid = 1;
    game->interaction = FD2_FIELD_INTERACTION_MOVING;
    int result = execute_path_playback(
        game, vga, frame_delay_ms, mode);
    game->move_path = snapshot.move_path;
    game->move_path_capacity = snapshot.move_path_capacity;
    game->move_path_length = snapshot.move_path_length;
    game->move_preview_valid = snapshot.move_preview_valid;
    if(result>0 && game->runtime_host_quit && game->failure==FD2_FIELD_FAILURE_NONE)return 1;
    if (result != 0) {
        if (game->failure == FD2_FIELD_FAILURE_NONE) *game = snapshot;
        return -1;
    }
    /* lookup0只更新pending，不追加notice，不能在此丢弃/分派最后命中。 */
    /* 防御性检查：非空路径必须改变坐标；否则按提交失败回滚。 */
    if (actor->x == start_x && actor->y == start_y) {
        *game = snapshot;
        return -1;
    }
    return 1;
}

/* field_ai_move_toward_cell @0x14b78（原登记同名）：先probe请求anchor，
 * 必要时沿完整long route更新本回合anchor，最后才排友方占格/选终点。 */
static int ai_path_toward_anchor(const fd2_field_game *game,
                                   size_t actor_index,
                                   int anchor_x, int anchor_y,
                                   uint8_t *path, size_t count,
                                   size_t *path_length) {
    if (!game || !game->ready || actor_index >= game->units.count ||
        game->map.width <= 0 || game->map.height <= 0 ||
        anchor_x < 0 || anchor_y < 0 || anchor_x >= game->map.width ||
        anchor_y >= game->map.height)
        return -1;
    const fd2_field_unit *actor = &game->units.items[actor_index];
    /* 145cd/146d1：selector0按side0分组，selector1按side!=0分组。
     * AI outer只调side0/1，March只side2；现query的actor.side分组等价，
     * 不能把此caller边界推广为任意actor/selector组合。 */
    /* @14b9f取raw3b预算（允许0，无HP门）；@14bc7 immune→19，
     * @14bcc..14bd5按text_id==28覆盖为1，区别于18890的unit_id28→16。 */
    uint8_t profile_id = actor->movement_profile;
    if (fd2_field_attack_unit_ignores_terrain_modifier(actor)) profile_id = 19;
    if (actor->text_id == 28u) profile_id = 1;
    const uint8_t *profile = fd2_field_movement_profile_get(profile_id);
    if (!profile || (size_t)game->map.width > SIZE_MAX / (size_t)game->map.height)
        return -1;
    if (!path || !path_length || count <
        (size_t)game->map.width * (size_t)game->map.height) return -1;
    *path_length = 0;
    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    int result = -1;
    size_t length = 0;
    /* @14c3a的normal probe目标是调用者的原anchor，不是已排序合法终点。 */
    int found = fd2_field_move_path_find(
        &game->map, &game->terrain, &game->units, actor_index,
        profile, FD2_FIELD_MOVEMENT_COST_CLASS_COUNT,
        actor->movement_points, FD2_FIELD_MOVE_PATH_NORMAL,
        anchor_x, anchor_y, path, count, &length, NULL, NULL);
    if (found < 0) goto done;
    if (!found) {
        /* @14c7d固定预算0x1c/mode1仍探同一请求anchor；14c53先4df4c
         * 清40/80，直到14ca6才重建145cd，因此长路没有单位阻挡/ZOC。
         * 路径buffer可复用；最后普通路径再次覆盖，不提交该long route。 */
        size_t long_length = 0;
        int long_found = fd2_field_move_path_find(
            &game->map, &game->terrain, &game->units, actor_index,
            profile, FD2_FIELD_MOVEMENT_COST_CLASS_COUNT, 0x1cu,
            (fd2_field_move_path_mode)(FD2_FIELD_MOVE_PATH_EQUAL_ROUTE |
                FD2_FIELD_MOVE_PATH_CLEAR_UNIT_FLAGS), anchor_x, anchor_y,
            path, count, &long_length, NULL, NULL);
        if (long_found < 0) goto done;
        if (long_found) {
            if (fd2_field_move_range_compute(
                    &range, &game->map, &game->terrain, &game->units, actor_index,
                    profile, FD2_FIELD_MOVEMENT_COST_CLASS_COUNT,
                    actor->movement_points) != 0)
                goto done;
            int x = actor->x, y = actor->y;
            int last_x = anchor_x, last_y = anchor_y;
            static const int dx[] = {0, -1, 0, 1};
            static const int dy[] = {1, 0, -1, 0};
            for (size_t i = 0; i < long_length; i++) {
                uint8_t direction = path[i];
                if (direction > 3u) goto done;
                x += dx[direction]; y += dy[direction];
                /* @14d17未命中只继续，不能break；此时原版尚未block友方
                 * destination，检查reachable distance而不是can_stop。 */
                if (fd2_field_path_distance(&range, x, y) != FD2_FIELD_PATH_UNREACHABLE) {
                    last_x = x; last_y = y;
                }
            }
            anchor_x = last_x; anchor_y = last_y;
        }
    }
    /* @14d47后才最终range/friendly block/sort；distance保屏蔽友方前
     * 传播值，can_stop对应146d1屏蔽后的raw3，不影响沿途经过友方格。 */
    if (fd2_field_move_range_compute(
            &range, &game->map, &game->terrain, &game->units, actor_index,
            profile, FD2_FIELD_MOVEMENT_COST_CLASS_COUNT,
            actor->movement_points) != 0)
        goto done;
    fd2_field_ai_destination destination;
    if (fd2_field_ai_choose_destination(&range, anchor_x, anchor_y, &destination) != 0) {
        result = 0;
        goto done;
    }
    found = fd2_field_move_path_find(
        &game->map, &game->terrain, &game->units, actor_index,
        profile, FD2_FIELD_MOVEMENT_COST_CLASS_COUNT,
        actor->movement_points, FD2_FIELD_MOVE_PATH_NORMAL,
        destination.x, destination.y, path, count, &length, NULL, NULL);
    if (found != 1) {
        result = found < 0 ? -1 : 0;
        goto done;
    }
    *path_length = length;
    result = 1;
done:
    fd2_field_path_close(&range);
    return result;
}

static int commit_ai_move_toward_anchor(fd2_field_game *game,
                                         size_t actor_index,
                                         int anchor_x, int anchor_y,
                                         fd2_vga *vga,
                                         uint32_t frame_delay_ms,
                                         field_path_playback_mode mode) {
    if (!game || game->map.width <= 0 || game->map.height <= 0 ||
        (size_t)game->map.width > SIZE_MAX / (size_t)game->map.height) return -1;
    size_t count = (size_t)game->map.width * (size_t)game->map.height;
    uint8_t *path = malloc(count);
    if (!path) return -1;
    size_t length = 0;
    int result = ai_path_toward_anchor(game, actor_index, anchor_x, anchor_y,
                                      path, count, &length);
    if (result > 0) {
        /* 14eaf在正式path playback前清同一地图owner。 */
        if (fd2_field_event_map_reset_cells(&game->map) != 0) result = -1;
        else result = commit_ai_path(game, actor_index, path, count, length,
                                      vga, frame_delay_ms, mode);
    }
    free(path);
    return result;
}

/* field_empty_focus_menu_execute @0x16f55（原登记同名）selector1：
 * 每名玩家只走14b78几何，不经AI decision/common-tail的match1与恢复。
 * 当前actor失败前的路径和之前完成者均已提交，不能退回成普通取消。 */
int fd2_field_game_march(fd2_field_game *game, fd2_vga *vga,
                         uint32_t frame_delay_ms) {
    if (!game || !game->ready || !fd2_field_stage_battle_is_supported(game->stage) || progress_pending(game) ||
        game->failure != FD2_FIELD_FAILURE_NONE || game->active_side != 2u ||
        game->player_post_waiting_phase || game->selected_unit >= 0 ||
        game->detail_visible || game->interaction != FD2_FIELD_INTERACTION_BROWSE ||
        game->cursor_cell_x < 0 || game->cursor_cell_y < 0 ||
        game->cursor_cell_x >= game->map.width ||
        game->cursor_cell_y >= game->map.height)
        return -1;
    if (vga && fd2_input_take_quit(&vga->input)) return 1;
    const int anchor_x = game->cursor_cell_x;
    const int anchor_y = game->cursor_cell_y;
    game->ordinary_display_mode = 0; /* 170ba的整批+turncycle caller。 */
    return march_continue(game,vga,frame_delay_ms,anchor_x,anchor_y,0);
}

static int march_continue(fd2_field_game *game,fd2_vga *vga,uint32_t frame_delay_ms,
        int anchor_x,int anchor_y,size_t first) {
    for (size_t i = first; i < game->units.count; i++) {
        fd2_field_unit *actor = &game->units.items[i];
        if ((actor->flags & 0x85u) != 0u || actor->side != 2u) continue;
        if (vga && fd2_input_take_quit(&vga->input)) return 1;
        /* 原版先focus actor，再清共享slot；后续actor不能把移动后的focus
         * 当成新anchor。焦点移动沿现有X→Y镜头阈值，不改变固定anchor。 */
        if (focus_to(game, actor->x, actor->y) != 0) {
            fail_action(game, i);
            return -1;
        }
        if(game->runtime_host_quit)return 1;
        game->pending_cell_event.valid = 0;
        int moved=commit_ai_move_toward_anchor(
                game, i, anchor_x, anchor_y, vga, frame_delay_ms,
                FIELD_PATH_MARCH_BROWSE);
        if (moved < 0 || game->failure!=FD2_FIELD_FAILURE_NONE) {
            fail_action(game, i);
            return -1;
        }
        if(game->runtime_host_quit)return 1;
        if (vga && fd2_input_take_quit(&vga->input)) return 1;
        /* march只有沿途match0；无path也到此，不追加最终match1。 */
        if (dispatch_pending_cell_event(game, i) != 0) return -1;
        if(game->event_resume.active) {
            game->event_resume.completion=FD2_EVENT_COMPLETE_MARCH;
            game->event_resume.next_slot=(uint8_t)(i+1u);
            game->event_resume.march_anchor_x=(int16_t)anchor_x;
            game->event_resume.march_anchor_y=(int16_t)anchor_y;
            game->event_resume.march_frame_ms=frame_delay_ms;
            game->event_resume.march_vga=vga;
            return 0;
        }
        int reset=run_turn_entry(game,FD2_FIELD_TURN_DIRECTIONS_RESET);
        if(reset<0){fail_action(game,i);return -1;}
        if(reset>0)return 1;
        fd2_field_unit_set_acted(actor, 1);
        game->selected_unit = -1;
        game->command_selected = -1;
        game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    }
    if (vga) {
        fd2_field_game_render_background(game, vga);
        if(vga->field_page) {
            game_present_copy(vga);
            if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return -1;
        }
        else if(fd2_vga_present(vga)<0)return -1; /* 无App诊断。 */
        if (fd2_input_take_quit(&vga->input)) return 1;
    }
    /* 整批结束才统一turncycle；沿既有逐actor呈现完整side1→0→new2。
     * march没有玩家controller的额外pending后置，也没有自动Save。 */
    game->turn_post_caller = FD2_FIELD_TURN_POST_MARCH;
    advance_phase(game);
    return fd2_field_game_resume_player_controller(game) < 0 ? -1 : 0;
}

/* field_ai_hostile_anchor @0x14121（IDA `sub_14121`）：使用固定预算
 * 0x1c、path mode 2 寻找 hostile occupied witness，再交给
 * field_ai_move_toward_cell @0x14b78 移向其本回合合法 anchor。 */
static int commit_ai_hostile_witness(fd2_field_game *game,
                                      size_t actor_index,
                                      fd2_vga *vga,
                                      uint32_t frame_delay_ms, field_path_playback_mode mode) {
    if (!game || actor_index >= game->units.count ||
        game->map.width <= 0 || game->map.height <= 0)
        return -1;
    const fd2_field_unit *actor = &game->units.items[actor_index];
    const uint8_t *profile =
        fd2_field_movement_profile_get(actor->movement_profile);
    if (!profile) return -1;
    size_t count = (size_t)game->map.width * (size_t)game->map.height;
    size_t ignored_length = 0;
    int witness_x = -1;
    int witness_y = -1;
    int found = fd2_field_move_path_find(
        &game->map, &game->terrain, &game->units, actor_index,
        profile, FD2_FIELD_MOVEMENT_COST_CLASS_COUNT, 0x1cu,
        FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS, 0, 0,
        NULL, count, &ignored_length, &witness_x, &witness_y);
    if (found != 1) return found;
    if (witness_x == actor->x && witness_y == actor->y) return 0;
    /* sub_14121 @0x14121：只在实际不同格witness分支暂关cursor，
     * 14b78本体不改显示模式；即使本轮零步，正常返回也恢复1。 */
    game->ordinary_display_mode = 0;
    if (focus_to(game, actor->x, actor->y) != 0) return -1;
    if(game->runtime_host_quit)return 1;
    int result = commit_ai_move_toward_anchor(
        game, actor_index, witness_x, witness_y, vga, frame_delay_ms, mode);
    if(game->runtime_host_quit)return result;
    if (result >= 0) game->ordinary_display_mode = 1;
    return result;
}

static int run_ai_try_attack(fd2_field_game *game, size_t actor_index,
                             fd2_vga *vga, uint32_t frame_delay_ms, field_path_playback_mode mode) {
    fd2_field_unit *actor = &game->units.items[actor_index];
    uint8_t selector = game->active_side == 0u ? 0u : 1u;
    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    fd2_field_ai_physical_candidate physical={0};
    int have_physical = game->attack_rng &&
        fd2_field_game_compute_move_range(
            game, actor_index, &range) == 0 &&
        fd2_field_ai_choose_physical_candidate(
            &game->map, &game->terrain, &game->units, actor_index,
            selector, &range, &physical) == 0;
    fd2_field_path_close(&range);
    /* magic/item 候选与物理候选平价分派；特殊 effect 若缺少完整语义，
     * commit 会在修改/RNG 前显式拒绝，不能静默降级为物理。 */
    fd2_field_ai_magic_candidate magic;
    fd2_field_ai_item_candidate item;
    int have_magic = fd2_field_ai_choose_magic_candidate(
        &game->map, &game->terrain, &game->units, actor_index,
        selector, &magic) == 0;
    int have_item = fd2_field_ai_choose_item_candidate(
        &game->map, &game->terrain, &game->units, actor_index,
        selector, &item) == 0;
    const fd2_field_unit *physical_target = have_physical
        ? &game->units.items[physical.unit_index] : NULL;
    fd2_field_ai_action action = fd2_field_ai_select_attack_action_for_candidates(
        actor, physical_target, have_physical ? &physical : NULL,
        have_magic ? &magic : NULL, have_item ? &item : NULL);
    if (action == FD2_FIELD_AI_ACTION_PHYSICAL)
        return commit_ai_physical_internal(
            game, actor_index, selector, &physical,
            vga, frame_delay_ms, 0, mode) == 0 ? 1 : -1;
    if (action == FD2_FIELD_AI_ACTION_MAGIC)
        return commit_ai_magic_internal(
            game, actor_index, selector, &magic, 0) == 0 ? 1 : -1;
    if (action == FD2_FIELD_AI_ACTION_ITEM)
        return commit_ai_item_internal(
            game, actor_index, selector, &item, 0) == 0 ? 1 : -1;
    if (action == FD2_FIELD_AI_ACTION_HANDLED_NOOP) return 1;
    return 0;
}

static int ai_behavior5_target_find(const fd2_field_game *game,
                                    size_t actor_index,
                                    int *target_x, int *target_y) {
    if (!game || actor_index >= game->units.count || !target_x || !target_y ||
        !game->map.cells || !game->terrain.attrs)
        return -1;
    const fd2_field_unit *actor = &game->units.items[actor_index];
    uint8_t lookup_id = actor->data_3d;
    if (lookup_id >= FD2_FIELD_EVENT_SLOTS) return -2;
    if (game->cell_action_completed[lookup_id] != 0u) return 0;
    for (int y = 0; y < game->map.height; y++) {
        for (int x = 0; x < game->map.width; x++) {
            uint32_t cell = game->map.cells[
                (size_t)y * (size_t)game->map.width + (size_t)x];
            uint16_t terrain_id = (uint16_t)(cell & 0x03ffu);
            if (terrain_id >= game->terrain.attr_count) return -2;
            if (((cell >> 16) & 0x1fu) == lookup_id &&
                (game->terrain.attrs[terrain_id].flags & 0x60u) == 0x20u) {
                *target_x = x;
                *target_y = y;
                return 1;
            }
        }
    }
    return 0;
}

/* field_unit_inventory_item_append @0x1bb8c（原sub_1BB8C）：首hidden槽整flag清0。 */
static int inventory_first_empty_add(fd2_field_unit *unit, uint8_t item_id) {
    int result=fd2_field_unit_inventory_item_append(unit,item_id);
    return result==0 ? -1:result; /* 保留既有Game满包-1合同。 */
}

static int ai_behavior5_complete(fd2_field_game *game,
                                 size_t actor_index,
                                 uint8_t lookup_id) {
    if (!game || actor_index >= game->units.count ||
        lookup_id >= FD2_FIELD_EVENT_SLOTS)
        return -1;
    fd2_field_unit *actor = &game->units.items[actor_index];
    const fd2_field_cell_action *action =
        &game->metadata.cell_actions[lookup_id];
    if (action->mode < 2u) {
        uint8_t *record = (uint8_t *)actor;
        record[0x31] = action->mode;
        record[0x32] = (uint8_t)(action->param & 0xffu);
        record[0x33] = (uint8_t)(action->param >> 8);
        if (action->mode == 0u)
            (void)inventory_first_empty_add(actor, (uint8_t)action->param);
    }
    game->cell_action_completed[lookup_id] = 1u;
    /* field_ai_unit_execute @0x13a9f（原登记field_ai_unit_execute；旧主体登记field_ai_unit_execute_core）：13d1b调用12263，
     * 13d20随后整byte写07；不是仅更改低四位。开箱资源已在移动前验证。 */
    if (complete_cell_tiles(game) != 0) return -1;
    actor->ai_behavior_raw = 7u;
    return 0;
}

static int recover_stationary_unit(fd2_field_game *game, size_t actor_index) {
    if (!game || actor_index >= game->units.count) return -1;
    /* sub_13FD4 @0x13fd4：HP等值才早退，over-max亦会夹回max；无除法HP门。
     * 三wait/cue/actor顺序及HP写点由stationary.recover冷图掌握。 */
    field_action_flow action={0};action.game=game;action.actor=action.target=actor_index;
    action.mode=FD2_FIELD_PHYSICAL_MODE_RECOVERY;
    return run_action_flow(&action,"stationary.recover")!=0 ? -1:action.recovered;
}

/* 只校验本动作后续可能使用的13fd4环境，不播放、不保留scope或改Game。
 * AI11可能先以20清毒并恢复少量HP，再进入13fd4，不能只看初始状态门。 */
static int recovery_resources_preflight(fd2_field_game *game, size_t actor_index) {
    if (!game || actor_index>=game->units.count) return -1;
    const fd2_field_unit *actor=&game->units.items[actor_index];
    int possible=actor->hp!=actor->hp_max && !actor->detail_status[0] && !actor->detail_status[1];
    if (!possible && fd2_field_ai_behavior(actor)==11 && actor->hp!=actor->hp_max &&
        actor->detail_status[0] && !actor->detail_status[1]) {
        fd2_field_ai_magic_candidate magic;
        uint8_t selector=game->active_side==0u ? 0u:1u;
        if (fd2_field_ai_choose_magic_candidate(&game->map,&game->terrain,&game->units,
                actor_index,selector,&magic)==0 && magic.score>=6 && magic.magic_id==20u) {
            uint8_t targets[FD2_FIELD_MAX_UNITS];size_t count=0;
            if (fd2_field_ai_collect_magic_targets(&game->map,&game->terrain,&game->units,
                    magic.magic_id,magic.cast_x,magic.cast_y,selector,targets,sizeof(targets),&count)!=0) return -1;
            for (size_t i=0;i<count;i++) if (targets[i]==actor_index) possible=1;
        }
    }
    if (!possible || !game->physical_present) return 0;
    fd2_field_physical_event event={0};
    event.phase=FD2_FIELD_PHYSICAL_PREPARE;event.presentation_mode=FD2_FIELD_PHYSICAL_MODE_RECOVERY;
    event.actor_index=event.target_index=actor_index;event.resource_only=1;
    return game->physical_present(game,&event,game->physical_present_userdata);
}

/* 13a9f behavior11可在magic/loot后继续物理；magic可能清掉阻挡者，故不能
 * 只预检施法前best candidate。这里只枚举有限资源环境，不选第二行动、不用RNG：
 * 当前可见存活hostile × MOV可容纳单格成本的地图格/原起点 × 武器距离。
 * 同资源身份/target attr2、actor attr2及counter门只借一格；不预测最终路径。
 * 单次暂存至多96*(80B记录+512B环境位+cache/attr2)，退出均释放，无新热owner。 */
static int ai_followup_physical_preflight(fd2_field_game *game, size_t actor_index) {
    if (!game->physical_present) return 0;
    const fd2_field_unit *actor=&game->units.items[actor_index];
    uint8_t min_range,max_range;
    if (fd2_field_attack_weapon_range(actor,&min_range,&max_range)!=0) return 0;
    const uint8_t *profile=fd2_field_movement_profile_get(actor->movement_profile);
    if (!profile || !game->map.cells || !game->terrain.attrs ||
        game->map.width<=0 || game->map.height<=0 || game->units.count>FD2_FIELD_MAX_UNITS) return -1;
    typedef struct {
        fd2_field_unit target; /* 除坐标外全部80B参与，未知字段不擅自忽略。 */
        size_t sprite_id;
        uint8_t target_scene,seen[256][2];
    } resource_group;
    resource_group *groups=calloc(game->units.count,sizeof(*groups));
    if (!groups) return -1;
    size_t group_count=0;int result=-1;
    for (size_t target_index=0;target_index<game->units.count;target_index++) {
        const fd2_field_unit *target=&game->units.items[target_index];
        if (target_index==actor_index || !target->hp || fd2_field_unit_is_hidden(target) ||
            (game->active_side==0u ? target->side==0u:target->side!=0u)) continue;
        if (target->x>=game->map.width || target->y>=game->map.height) goto done;
        const fd2_terrain_attr *target_attr=fd2_terrain_attr_get(&game->terrain,
            fd2_field_cell_terrain(fd2_field_map_cell(&game->map,target->x,target->y)));
        if (!target_attr) goto done;
        fd2_field_unit key=*target;key.x=key.y=0;
        size_t sprite_id=fd2_field_unit_cached_sprite_id(&game->units,target_index),group=0;
        for (;group<group_count;group++)
            if (groups[group].sprite_id==sprite_id && groups[group].target_scene==target_attr->attr2 &&
                !memcmp(&groups[group].target,&key,sizeof(key))) break;
        if (group==group_count) {
            groups[group].target=key;groups[group].sprite_id=sprite_id;
            groups[group].target_scene=target_attr->attr2;group_count++;
        }
        for (int y=0;y<game->map.height && y<=UINT8_MAX;y++)
        for (int x=0;x<game->map.width && x<=UINT8_MAX;x++) {
            int distance=abs(x-(int)target->x)+abs(y-(int)target->y);
            if (distance<min_range || distance>max_range) continue;
            const fd2_terrain_attr *attr=fd2_terrain_attr_get(&game->terrain,
                fd2_field_cell_terrain(fd2_field_map_cell(&game->map,x,y)));
            if (!attr || attr->movement_cost_class>=FD2_FIELD_MOVEMENT_COST_CLASS_COUNT) goto done;
            int origin=x==actor->x && y==actor->y;
            if (!origin && (!actor->movement_points ||
                profile[attr->movement_cost_class]>actor->movement_points)) continue;
            fd2_field_unit projected=*actor;projected.x=(uint8_t)x;projected.y=(uint8_t)y;
            unsigned counter=(unsigned)fd2_field_attack_counterattack_is_available(target,&projected);
            if (groups[group].seen[attr->attr2][counter]) continue;
            groups[group].seen[attr->attr2][counter]=1;
            fd2_field_physical_event event={0};
            event.phase=FD2_FIELD_PHYSICAL_PREPARE;event.resource_only=1;
            event.presentation_mode=game->option_values[2]
                ? FD2_FIELD_PHYSICAL_MODE_SHORT:FD2_FIELD_PHYSICAL_MODE_FULL;
            event.actor_index=actor_index;event.target_index=target_index;
            event.actor_before=&projected;event.target_before=target;
            if (game->physical_present(game,&event,game->physical_present_userdata)!=0) goto done;
        }
    }
    result=0;
done:
    free(groups);return result;
}

static int run_mode0_action(fd2_field_game *game, size_t actor_index,
                            fd2_vga *vga, uint32_t frame_delay_ms, field_path_playback_mode mode) {
    int attack = run_ai_try_attack(
        game, actor_index, vga, frame_delay_ms, mode);
    if (attack != 0) return attack;
    int moved = commit_ai_hostile_witness(
        game, actor_index, vga, frame_delay_ms, mode);
    if (moved != 0) return moved;
    fd2_field_ai_target target;
    if (fd2_field_ai_nearest_opponent(&game->units, actor_index,
                                      game->active_side, &target) == 0 &&
        (target.x != game->units.items[actor_index].x ||
         target.y != game->units.items[actor_index].y)) {
        const fd2_field_unit *actor = &game->units.items[actor_index];
        game->ordinary_display_mode = 0; /* field_ai_move_toward_nearest @0x13e9c。 */
        if (focus_to(game, actor->x, actor->y) != 0) return -1;
        if(game->runtime_host_quit)return 1;
        int result = commit_ai_move_toward_anchor(
            game, actor_index, target.x, target.y,
            vga, frame_delay_ms, mode);
        if(game->runtime_host_quit)return result;
        if (result >= 0) game->ordinary_display_mode = 1;
        return result;
    }
    return 0;
}

static int move_then_recover(fd2_field_game *game, size_t actor_index,
                             int x, int y,
                             fd2_vga *vga, uint32_t frame_delay_ms, field_path_playback_mode mode) {
    const fd2_field_unit *actor = &game->units.items[actor_index];
    if (focus_to(game, actor->x, actor->y) != 0) return -1;
    if(game->runtime_host_quit)return 1;
    int moved = commit_ai_move_toward_anchor(
        game, actor_index, x, y, vga, frame_delay_ms, mode);
    if (moved != 0) return moved;
    return recover_stationary_unit(game, actor_index) < 0 ? -1 : 0;
}

static int run_ai_after_magic(fd2_field_game *game, size_t actor_index,
        fd2_vga *vga, uint32_t frame_delay_ms, field_path_playback_mode mode, int cast) {
    uint8_t selector = game->active_side == 0u ? 0u : 1u;
    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    fd2_field_ai_physical_candidate physical={0};
    int have_physical = game->attack_rng &&
        fd2_field_game_compute_move_range(
            game, actor_index, &range) == 0 &&
        fd2_field_ai_choose_physical_candidate(
            &game->map, &game->terrain, &game->units,
            actor_index, selector, &range, &physical) == 0;
    fd2_field_path_close(&range);
    if (have_physical && physical.priority >= 6) {
        if (commit_ai_physical_internal(
                game, actor_index, selector, &physical,
                vga, frame_delay_ms, 0, mode) == 0)
            return 1;
        /* magic 已消费 RNG；后段失败保留已提交效果并锁住 session，
         * 不能按成功补 common tail 或重放此前施法。 */
        if (cast) fail_action(game, actor_index);
        return -1;
    }
    int moved = commit_ai_hostile_witness(
        game, actor_index, vga, frame_delay_ms, mode);
    if (moved != 0) return moved;
    return recover_stationary_unit(game, actor_index) < 0 ? -1 : 0;
}

static int run_ai_behavior(fd2_field_game *game, size_t actor_index,
                           fd2_vga *vga, uint32_t frame_delay_ms,
                           int *run_common_tail, field_path_playback_mode mode) {
    if (!game || actor_index >= game->units.count || !run_common_tail)
        return -1;
    fd2_field_unit *actor = &game->units.items[actor_index];
    uint8_t behavior = fd2_field_ai_behavior(actor);
    *run_common_tail = 1;
    /* 纯查询确定可能13fd4的行为域；11的future清毒分支在首magic RNG前校验。
     * 1/8无恢复分支，不将无关恢复资源变为其执行支持门。 */
    if ((behavior==0u || (behavior>=2u && behavior<=5u) || behavior==7u ||
         (behavior>=9u && behavior<=11u)) && recovery_resources_preflight(game,actor_index)!=0)
        return -1;
    switch (behavior) {
        case 0:
            return run_mode0_action(
                game, actor_index, vga, frame_delay_ms, mode);
        case 1: {
            int attack = run_ai_try_attack(
                game, actor_index, vga, frame_delay_ms, mode);
            if (attack != 0) return attack;
            return commit_ai_hostile_witness(
                game, actor_index, vga, frame_delay_ms, mode);
        }
        case 2: {
            int attack = run_ai_try_attack(
                game, actor_index, vga, frame_delay_ms, mode);
            if (attack != 0) return attack;
            /* 正确 dual 0x13b5d：再次运行 physical candidate query，
             * 不调用 exchange；helper 固定返回 0，随后进入恢复/common tail。 */
            fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
            if (fd2_field_game_compute_move_range(
                    game, actor_index, &range) == 0) {
                fd2_field_ai_physical_candidate ignored;
                (void)fd2_field_ai_choose_physical_candidate(
                    &game->map, &game->terrain, &game->units,
                    actor_index, game->active_side == 0u ? 0u : 1u,
                    &range, &ignored);
            }
            fd2_field_path_close(&range);
            return recover_stationary_unit(game, actor_index) < 0 ? -1 : 0;
        }
        case 3: {
            int attack = run_ai_try_attack(
                game, actor_index, vga, frame_delay_ms, mode);
            if (attack != 0) return attack;
            int target_index = fd2_field_units_find_visible_text_id(
                &game->units, actor->ai_param_35);
            if (target_index < 0)
                return commit_ai_hostile_witness(
                    game, actor_index, vga, frame_delay_ms, mode);
            const fd2_field_unit *target = &game->units.items[target_index];
            int result = move_then_recover(
                game, actor_index, target->x, target->y,
                vga, frame_delay_ms, mode);
            if(game->runtime_host_quit)return result;
            /* field_ai_unit_execute @0x13a9f behavior3在移动/恢复后再写0。 */
            if (result >= 0) game->ordinary_display_mode = 0;
            return result;
        }
        case 4:
            game->ordinary_display_mode = 0;
            return move_then_recover(
                game, actor_index, actor->ai_param_35, actor->ai_param_36,
                vga, frame_delay_ms, mode);
        case 5: {
            int attack = run_ai_try_attack(
                game, actor_index, vga, frame_delay_ms, mode);
            if (attack != 0) return attack;
            game->ordinary_display_mode = 0;
            /* 原13c39在completed/15df3查询前聚焦；fallback无动作仍保留镜头。 */
            if (focus_to(game, actor->x, actor->y) != 0) return -1;
            if(game->runtime_host_quit)return 1;
            int target_x = -1;
            int target_y = -1;
            int found = ai_behavior5_target_find(
                game, actor_index, &target_x, &target_y);
            /* 0x13c4d/0x13c61 均跳回 0x13b05：完成位已置或
             * 0x15df3 未命中时执行完整 behavior 0 fallback。 */
            if (found == -2) return -1;
            if (found <= 0)
                return run_mode0_action(
                    game, actor_index, vga, frame_delay_ms, mode);
            uint8_t completed[sizeof(game->cell_action_completed)];
            memcpy(completed, game->cell_action_completed, sizeof(completed));
            completed[actor->data_3d] = 1u;
            if (completed_cell_tiles_preflight(game, &game->map, completed) != 0)
                return -1;
            int moved = commit_ai_move_toward_anchor(
                game, actor_index, target_x, target_y,
                vga, frame_delay_ms, mode);
            if (moved < 0) return -1;
            if(game->runtime_host_quit)return moved;
            if (moved == 0 && recover_stationary_unit(game, actor_index) < 0)
                return -1;
            actor = &game->units.items[actor_index];
            if (actor->x == target_x && actor->y == target_y &&
                ai_behavior5_complete(
                    game, actor_index, actor->data_3d) != 0)
                return -1;
            return moved;
        }
        case 7: {
            game->ordinary_display_mode = 0;
            if (focus_to(game, actor->x, actor->y) != 0) return -1;
            if(game->runtime_host_quit)return 1;
            int moved = commit_ai_move_toward_anchor(
                game, actor_index, actor->ai_param_35, actor->ai_param_36,
                vga, frame_delay_ms, mode);
            if (moved < 0) return -1;
            if(game->runtime_host_quit)return moved;
            if (moved == 0 && recover_stationary_unit(game, actor_index) < 0)
                return -1;
            actor = &game->units.items[actor_index];
            if (actor->x == actor->ai_param_35 &&
                actor->y == actor->ai_param_36) {
                /* behavior 7 的正确 dual 0x32975 仅执行
                 * `actor[+0x05] = 1`，没有隐藏的 stage callback。 */
                actor->flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
            }
            return moved;
        }
        case 8:
            /* corrected dual 0x13d97..0x13d9a 直接跳到 AI function epilogue，
             * 不执行 cell lookup、acted 或全表 direction reset。outer side
             * loop 仍执行 stage handler；SDL phase cursor 随后越过该 actor，
             * 不能因其保持 unacted 而在同一 phase 重复调度。 */
            *run_common_tail = 0;
            return 0;
        case 9: {
            int target_index = fd2_field_units_find_visible_text_id(
                &game->units, actor->ai_param_35);
            if (target_index < 0)
                return run_mode0_action(
                    game, actor_index, vga, frame_delay_ms, mode);
            const fd2_field_unit *target = &game->units.items[target_index];
            return move_then_recover(
                game, actor_index, target->x, target->y,
                vga, frame_delay_ms, mode);
        }
        case 10: {
            int attack = run_ai_try_attack(
                game, actor_index, vga, frame_delay_ms, mode);
            if (attack != 0) return attack;
            game->ordinary_display_mode = 0;
            return move_then_recover(
                game, actor_index, actor->ai_param_35, actor->ai_param_36,
                vga, frame_delay_ms, mode);
        }
        case 11: {
            uint8_t selector = game->active_side == 0u ? 0u : 1u;
            fd2_field_ai_magic_candidate magic;
            int cast = 0;
            if (fd2_field_ai_choose_magic_candidate(
                    &game->map, &game->terrain, &game->units,
                    actor_index, selector, &magic) == 0 && magic.score >= 6) {
                if (ai_followup_physical_preflight(game,actor_index)!=0) return -1;
                if (commit_ai_magic_internal(
                        game, actor_index, selector, &magic, 0) != 0)
                    return -1;
                if(magic.magic_id>=10u && magic.magic_id<=12u && game->runtime_host_quit)return 1;
                cast = 1;
            }
            if (game->loot.notice.kind != FD2_FIELD_LOOT_NONE) {
                game->loot.ai_after_magic = 1;
                return 1;
            }
            return run_ai_after_magic(game, actor_index, vga, frame_delay_ms, mode, cast);
        }
        /* behavior 6 无有效分支。 */
        default:
            return -1;
    }
}

/* 调度器选中一名AI actor后的完整一步：推进游标、执行行为、处理延续，
 * 补公共tail、分派待决格事件并结算外层结果。两个pass共用。
 * 内部物理／法术／物品及移动事务都未执行tail；scheduler按原版无条件补一次，
 * 不因效果提交后的acted/死亡跳过（behavior8除外）。原版 outer side loop
 * 此时才调用 DS:0x1b91[cell_code]；未知handler不得按「无事件」继续，保留
 * 已提交动作并显式停机，不能伪造外部RNG回滚。 */
static int run_scheduled_ai_actor(fd2_field_game *game, size_t i, fd2_vga *vga,
        uint32_t frame_delay_ms, field_path_playback_mode mode) {
    game->phase_unit_cursor = i + 1u;
    int run_common_tail = 1;
    int action_result = run_ai_behavior(
        game, i, vga, frame_delay_ms, &run_common_tail, mode);
    if (action_result < 0) {
        fail_action(game, i);
        return -1;
    }
    if(game->runtime_host_quit)return game->failure==FD2_FIELD_FAILURE_NONE?1:-1;
    if (progress_pending(game)) {
        set_ai_continuation(game, FD2_FIELD_PROGRESS_RESUME_AI_SCHEDULER, i, run_common_tail);
        return 1;
    }
    if (run_common_tail && finish_ai_action(game, i) != 0)
        return -1;
    if(game->runtime_host_quit)return 1;
    if (game->failure != FD2_FIELD_FAILURE_NONE ||
        dispatch_pending_cell_event(game, i) != 0)
        return -1;
    if(game->event_resume.active)return 1;
    if(finish_ai_outer_result(game,i)<0)return -1;
    return game->failure == FD2_FIELD_FAILURE_NONE ? 1 : -1;
}

int fd2_field_game_process_automatic_action_visual(
        fd2_field_game *game, fd2_vga *vga, uint32_t frame_delay_ms,
        int clean_background) {
    field_path_playback_mode mode = clean_background
        ? FIELD_PATH_MARCH_BROWSE : FIELD_PATH_BROWSE;
    if (game && game->failure != FD2_FIELD_FAILURE_NONE) return -1;
    if (game && game->event_resume.active) return 0;
    if (game && game->ready && game->loot.source == LOOT_SOURCE_AI_REMAINDER) {
        fd2_field_loot saved = game->loot;
        memset(&game->loot, 0, sizeof(game->loot));
        int result = run_ai_after_magic(game, saved.action_actor, vga, frame_delay_ms, mode, 1);
        if (result < 0) { fail_action(game, saved.action_actor); return -1; }
        if(game->runtime_host_quit)return game->failure==FD2_FIELD_FAILURE_NONE?1:-1;
        if (progress_pending(game)) {
            set_ai_continuation(game, saved.resume, saved.action_actor, saved.ai_common_tail);
            return 1;
        }
        if (loot_flow_resume_ai_tail(game, &saved) != 0) return -1;
        return 1;
    }
    if (!game || !game->ready || progress_pending(game) || game->active_side == 2 ||
        game->selected_unit >= 0 ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE)
        return 0;
    /* side1/side0 outer @1d80b/1d8ba每raw在pending之后调用51b19。
     * 此入口纯检查只读已有latch；新含写/等待绑定不得在这里额外执行，
     * 而在以下每raw真实返回点（含被gate跳过者）使用同一有限续态。 */
    if(refresh_battle_result(game,game->phase_unit_cursor<game->units.count?game->phase_unit_cursor:0)!=0)return -1;
    if (game->battle_result != FD2_FIELD_BATTLE_ONGOING)
        return fd2_field_game_resume_player_controller(game) < 0 ? -1 : 0;
    /* tick 只安排自动阶段时间；有 VGA 的 play owner 负责同步路径演出，
     * 避免 tick 在 render 前静默推进同一 actor。 */
    if (!vga && frame_delay_ms != 0u) return 0;

    /* side1/side0 均按 raw actor index 升序，而不是旧 skeleton 的环形
     * phase cursor。AI 入口复现 flags `&0x05` gate：hidden bit 0、
     * AI-ineligible bit 2 均不进入自动行动。正确 dual 0x1d8ba 的 side0
     * 第一轮只让 magic/item
     * candidate >=6 的 actor 先执行；随后第二轮处理尚未 acted 的单位。
     * 两个pass都完整扫raw表，各actor起点清共享pending。 */
    if (game->active_side == 0u && game->phase_ai_pass == 0u) {
        for (size_t i = game->phase_unit_cursor; i < game->units.count; i++) {
            game->pending_cell_event.valid = 0;
            fd2_field_unit *unit = &game->units.items[i];
            if (!ai_unit_can_execute(unit, 0u) ||
                fd2_field_unit_has_acted(unit) ||
                unit->detail_status[1] != 0u) {
                int result=skip_ai_outer_result(game,i);
                if(result<0)return -1;
                if(result>0)return 1;
                continue;
            }
            fd2_field_ai_magic_candidate magic;
            fd2_field_ai_item_candidate item;
            int magic_ready = fd2_field_ai_choose_magic_candidate(
                &game->map, &game->terrain, &game->units, i, 0,
                &magic) == 0 && magic.score >= 6;
            int item_ready = fd2_field_ai_choose_item_candidate(
                &game->map, &game->terrain, &game->units, i, 0,
                &item) == 0 && item.score >= 6;
            if (!magic_ready && !item_ready) {
                int result=skip_ai_outer_result(game,i);
                if(result<0)return -1;
                if(result>0)return 1;
                continue;
            }
            /* 13a9f没有统一聚焦；各实际动作wrapper按自身原调用点处理。 */
            return run_scheduled_ai_actor(game, i, vga, frame_delay_ms, mode);
        }
        game->phase_ai_pass = 1u;
        game->phase_unit_cursor = 0;
    }

    for (size_t i = game->phase_unit_cursor; i < game->units.count; i++) {
        game->pending_cell_event.valid = 0;
        fd2_field_unit *unit = &game->units.items[i];
        if (!ai_unit_can_execute(unit, game->active_side) ||
            fd2_field_unit_has_acted(unit) || unit->detail_status[1] != 0u) {
            int result=skip_ai_outer_result(game,i);
            if(result<0)return -1;
            if(result>0)return 1;
            continue;
        }
        return run_scheduled_ai_actor(game, i, vga, frame_delay_ms, mode);
    }
    /* 扫到raw尾（含被跳过者）才换phase；不能remaining=0提前跳过清slot。 */
    advance_phase(game);
    return fd2_field_game_resume_player_controller(game) < 0 ? -1 : 1;
}

int fd2_field_game_process_automatic_action(fd2_field_game *game) {
    return fd2_field_game_process_automatic_action_visual(game, NULL, 0, 0);
}

int fd2_field_game_compute_move_range(const fd2_field_game *game,
                                      size_t mover_index,
                                      fd2_field_path_result *result) {
    if (!game || !game->ready || mover_index >= game->units.count || !result)
        return -1;
    const fd2_field_unit *unit = &game->units.items[mover_index];
    /* 只读入口按正式player18890/physical14237调用域选择；14b78在其
     * 私有planner内独立覆盖text_id28，不借此改全局movement profile。 */
    const uint8_t *profile = move_profile(unit, unit->side == 2u);
    /* field_reachable_cells_compute @0x4e390先写起点预算；MOV0仍可原地行动。 */
    if (!profile) return -1;
    return fd2_field_move_range_compute(
        result, &game->map, &game->terrain, &game->units, mover_index,
        profile, FD2_FIELD_MOVEMENT_COST_CLASS_COUNT,
        unit->movement_points);
}

static void actor_flash_paint(fd2_field_game *game, uint8_t *snapshot,
                              size_t unit_index, uint8_t palette_index) {
    if (!game || !snapshot || unit_index >= game->units.count) return;
    /* field_actor_group_flash @0x1c2da（原sub_1C2DA）调用
     * map_sprite_solid_blit_24 @0x4e127（原同名）：arg_C未被读取，
     * 原战场stride456的低byte才是常色；不能取SDL stride320或caller色。 */
    (void)palette_index;
    uint8_t solid_color = fd2_battle_animation_map_solid_color();
    const fd2_field_unit *unit = &game->units.items[unit_index];
    size_t sprite_id = fd2_field_unit_cached_sprite_id(&game->units, unit_index);
    size_t frame_index = sprite_id == SIZE_MAX ? SIZE_MAX : sprite_id * 12u +
        (game->idle_phase == 3u ? 2u : game->idle_phase);
    if (fd2_field_unit_is_hidden(unit) || !game->sprites.decoded_frames ||
        frame_index >= game->sprites.frame_count)
        return;
    int camera_x = camera_origin_x(game);
    int camera_y = camera_origin_y(game);
    int x = (int)unit->x * 24 - camera_x;
    int y = (int)unit->y * 24 - camera_y - 6;
    const uint8_t *sprite = game->sprites.decoded_frames +
                            frame_index * 24u * 24u;
    for (int row = 0; row < 24; row++) {
        int dy = y + row;
        if (dy < 4 || dy >= 196) continue;
        for (int col = 0; col < 24; col++) {
            int dx = x + col;
            if (dx < 4 || dx >= 316 || sprite[row * 24 + col] == 0)
                continue;
            snapshot[(size_t)dy * VGA_STRIDE + (size_t)dx] = solid_color;
        }
    }
}

int fd2_field_game_prepare_actor_group_flash(
        fd2_field_game *game, fd2_vga *vga,
        const uint8_t *unit_indices, size_t unit_count,
        uint8_t palette_index, fd2_field_effect_frames *frames) {
    if (!game || !game->ready || !vga || !frames || frames->storage ||
        !unit_indices || unit_count == 0 || unit_count > game->units.count)
        return -1;
    for (size_t i = 0; i < unit_count; i++)
        if (unit_indices[i] >= game->units.count) return -1;
    if (fd2_field_effect_frames_open(
            frames, FD2_FIELD_EFFECT_ACTOR_GROUP_FLASH, 2) != 0)
        return -1;
    if(vga->field_page) {
        fd2_field_page *solid=fd2_field_page_create();if(!solid)goto failed;
        if(fd2_field_page_copy_pixels(solid,vga->field_page)!=0) {
            fd2_field_page_destroy(solid);goto failed;
        }
        for(size_t i=0;i<unit_count;i++)
            if(fd2_field_flash_page_actor(solid,&game->sprites,&game->units,
                unit_indices[i],game->camera_cell_x,game->camera_cell_y,game->idle_phase)!=0) {
                fd2_field_page_destroy(solid);goto failed;
            }
        if(fd2_field_page_project(vga->field_page,72,72,fd2_field_effect_snapshot(frames,0),64000)!=0 ||
            fd2_field_page_project(solid,72,72,fd2_field_effect_snapshot(frames,1),64000)!=0 ||
            fd2_field_effect_frames_take_page(frames,1,solid)!=0) {
            fd2_field_page_destroy(solid);goto failed;
        }
        return 0;
    }
    fd2_field_game_render_effect_background(game, vga);
    uint8_t *original = fd2_field_effect_snapshot(frames, 0);
    uint8_t *modified = fd2_field_effect_snapshot(frames, 1);
    memcpy(original, vga->framebuffer, VGA_W * VGA_H);
    memcpy(modified, original, VGA_W * VGA_H);
    for (size_t i = 0; i < unit_count; i++)
        actor_flash_paint(game, modified, unit_indices[i], palette_index);
    return 0;
failed:
    fd2_field_effect_frames_close(frames);return -1;
}

static int prepare_earthquake(fd2_field_game *game,fd2_vga *vga,
    fd2_field_effect_frames *frames,int live) {
    if (!game || !game->ready || game->units.count>FD2_FIELD_MAX_UNITS ||
        !vga || !frames || frames->storage)
        return -1;
    if (fd2_field_effect_frames_open(
            frames, FD2_FIELD_EFFECT_EARTHQUAKE, 3) != 0)
        return -1;
    if(!vga->field_page)fd2_field_game_render_effect_background(game, vga);
    uint8_t baseline[VGA_W * VGA_H];
    memcpy(baseline, vga->framebuffer, sizeof(baseline));
    /* field_earthquake_effect @0x21527 → sub_21548 @0x21548：原三表
     * 52096/520a2/520ae复制到P+1c/P+28/P+10。2167e/216a4/216cb
     * 的实际SP分别取step/y/x，不能沿伪C局部标签错移一项。 */
    _Static_assert(sizeof(k_field_quake_parameters)==9u*sizeof(int32_t),
                   "original quake has three x/y/step rows");
    for (size_t i = 0; i < 3; i++) {
        const int32_t *parameters=k_field_quake_parameters[i];
        uint8_t *snapshot = fd2_field_effect_snapshot(frames, i);
        if(vga->field_page) {
            if(live && fd2_input_take_quit(&vga->input)) {
                fd2_field_effect_frames_close(frames);return 1;
            }
            fd2_field_page *page=fd2_field_page_create();if(!page)goto failed;
            if(!i && fd2_field_page_copy_pixels(page,vga->field_page)!=0) {
                fd2_field_page_destroy(page);goto failed;
            }
            fd2_vga scaled={0};
            /* 1f558的cache按原terrain ID采样；不是11eee动画替代帧。 */
            fd2_terrain_render_field_scaled_animated(&scaled,&game->terrain,&game->map,
                game->camera_cell_x*0xc00+FD2_FIELD_VIEW_CELLS_X*0x600+parameters[0],
                game->camera_cell_y*0xc00+FD2_FIELD_VIEW_CELLS_Y*0x600+parameters[1],parameters[2],0,0);
            if(fd2_field_page_scaled_view_write(page,scaled.framebuffer,64000)!=0) {
                fd2_field_page_destroy(page);goto failed;
            }
            for(size_t raw=0;raw<game->units.count;raw++) {
                if(fd2_field_unit_is_hidden(&game->units.items[raw]))continue;
                int actor_result=live ?
                    fd2_field_units_render_page_actor_live(vga,page,&game->sprites,&game->units,
                        raw,game->camera_cell_x,game->camera_cell_y,game->idle_phase,
                        (uint8_t)vga->field_visual.cursor_phase,0) :
                    fd2_field_units_render_page_actor(page,&game->sprites,&game->units,
                        raw,game->camera_cell_x,game->camera_cell_y,game->idle_phase,
                        (uint8_t)vga->field_visual.cursor_phase,vga->actor_jitter_phase,0);
                if(actor_result!=0) {
                    fd2_field_page_destroy(page);goto failed;
                }
            }
            /* 21548@21779只投影312×192到VGA(4,4)；每个快照的4096B
             * 外框均来自进入时VGA，不借frames calloc的零底。 */
            memcpy(snapshot,baseline,sizeof(baseline));
            if(fd2_field_page_project(page,72,72,snapshot,64000)!=0 ||
                (live && !i && fd2_field_page_copy_pixels(vga->field_page,page)!=0) ||
                fd2_field_effect_frames_take_page(frames,i,page)!=0) {
                fd2_field_page_destroy(page);goto failed;
            }
            continue;
        }
        memcpy(snapshot, baseline, sizeof(baseline));
        fd2_vga target = *vga;
        memcpy(target.framebuffer, snapshot, sizeof(target.framebuffer));
        int center_x = game->camera_cell_x * 0xc00 +
                       FD2_FIELD_VIEW_CELLS_X * 0x600 + parameters[0];
        int center_y = game->camera_cell_y * 0xc00 +
                       FD2_FIELD_VIEW_CELLS_Y * 0x600 + parameters[1];
        fd2_terrain_render_field_scaled_animated(
            &target, &game->terrain, &game->map, center_x, center_y, parameters[2],
            game->terrain_visual_phase, game->idle_phase);
        int camera_x = game->camera_cell_x * 24 - FD2_FIELD_VIEW_BORDER;
        int camera_y = game->camera_cell_y * 24 - FD2_FIELD_VIEW_BORDER;
        fd2_field_units_render(&target, &game->sprites, &game->units,
                               camera_x, camera_y, game->idle_phase);
        memcpy(snapshot, target.framebuffer, sizeof(target.framebuffer));
    }
    return 0;
failed:
    fd2_field_effect_frames_close(frames);return -1;
}

int fd2_field_game_prepare_earthquake(fd2_field_game *game,fd2_vga *vga,
    fd2_field_effect_frames *frames) {
    return prepare_earthquake(game,vga,frames,0);
}

int fd2_field_game_begin_earthquake(fd2_field_game *game,fd2_vga *vga,
    fd2_field_effect_frames *frames) {
    return prepare_earthquake(game,vga,frames,1);
}

int fd2_field_game_prepare_stage_transition(
        fd2_field_game *game, fd2_vga *vga,
        int center_screen_x, int center_screen_y,
        int radius, int radius_step, fd2_field_effect_frames *frames) {
    if (!game || !game->ready || !vga || !frames || frames->storage ||
        radius <= 0 || radius > 4096 ||
        radius_step < -4096 || radius_step > 4096 ||
        radius + radius_step * 8 <= 0 || radius + radius_step * 8 > 4096)
        return -1;
    if (fd2_field_effect_frames_open(
            frames, FD2_FIELD_EFFECT_STAGE_TRANSITION, 9) != 0)
        return -1;
    if(vga->field_page) {
        fd2_field_page *base=fd2_field_page_create(),*working=fd2_field_page_create();
        if(!base || !working) {
            fd2_field_page_destroy(base);fd2_field_page_destroy(working);goto failed;
        }
        fd2_vga target={0};target.field_page=working;
        target.field_visual=vga->field_visual;
        target.actor_jitter_tick=vga->actor_jitter_tick;
        target.actor_jitter_phase=vga->actor_jitter_phase;
        int result=fd2_field_game_effect_terrain_page(game,vga,base);
        for(size_t frame=0;!result && frame<9;frame++) {
            const uint8_t *lut=fd2_field_lut_frame(&game->visuals,9-frame);
            result=fd2_field_game_effect_mask_frame(game,&target,base,lut,
                center_screen_x,center_screen_y,radius+(int)frame*radius_step,0,0);
            if(result)break;
            fd2_field_page *page=fd2_field_page_create();
            if(!page) {result=-1;break;}
            if(fd2_field_page_copy_pixels(page,working)!=0 ||
                fd2_field_effect_frames_take_page(frames,frame,page)!=0) {
                fd2_field_page_destroy(page);result=-1;break;
            }
            memcpy(fd2_field_effect_snapshot(frames,frame),target.framebuffer,64000);
        }
        fd2_field_page_destroy(working);fd2_field_page_destroy(base);
        if(result)goto failed;
        return 0;
    }
    for (size_t frame = 0; frame < 9; frame++) {
        const uint8_t *lut = fd2_field_lut_frame(&game->visuals, 9 - frame);
        uint8_t *snapshot = fd2_field_effect_snapshot(frames, frame);
        if (!lut || !snapshot || fd2_field_game_render_transition_mask(game, vga,
                lut, center_screen_x, center_screen_y,
                radius + (int)frame * radius_step) != 0) {
            fd2_field_effect_frames_close(frames);
            return -1;
        }
        memcpy(snapshot, vga->framebuffer, sizeof(vga->framebuffer));
    }
    return 0;
failed:
    fd2_field_effect_frames_close(frames);return -1;
}

typedef struct {
    const fd2_field_game *game;
    fd2_field_page *base;
    int x,y,radius,step;
} field_transition_live;

static int transition_live_frame(fd2_field_effect effect,size_t frame,size_t source,
    fd2_vga *vga,void *userdata) {
    (void)source;field_transition_live *live=userdata;
    if(effect!=FD2_FIELD_EFFECT_STAGE_TRANSITION || frame>=9 || !live)return -1;
    return fd2_field_game_effect_mask_frame(live->game,vga,live->base,
        fd2_field_lut_frame(&live->game->visuals,9-frame),live->x,live->y,
        live->radius+(int)frame*live->step,0,0);
}

int fd2_field_game_play_actor_group_flash(
        fd2_field_game *game, fd2_vga *vga, fd2_field_audio *audio,
        const uint8_t *unit_indices, size_t unit_count,
        uint8_t palette_index, int timed) {
    if(!game || !vga)return -1;
    if(fd2_input_take_quit(&vga->input))return 1;
    fd2_field_effect_frames frames = {0};
    if (fd2_field_game_prepare_actor_group_flash(
            game, vga, unit_indices, unit_count, palette_index, &frames) != 0)
        return -1;
    int result = fd2_field_effect_play(&frames, vga, audio, timed);
    fd2_field_effect_frames_close(&frames);
    return result;
}

int fd2_field_game_play_earthquake(fd2_field_game *game, fd2_vga *vga,
                                   fd2_field_audio *audio, int timed) {
    if(!game || !vga)return -1;
    if(fd2_input_take_quit(&vga->input))return 1;
    fd2_field_effect_frames frames = {0};
    int prepared=fd2_field_game_begin_earthquake(game,vga,&frames);
    if(prepared)return prepared;
    int result = fd2_field_effect_play(&frames, vga, audio, timed);
    fd2_field_effect_frames_close(&frames);
    /* 21548在HP/RNG前先11cac(0)；本视觉入口不执行后续伤害。 */
    if(!result && vga->field_page)result=fd2_field_game_effect_refresh(game,vga);
    return result;
}

int fd2_field_game_play_stage_transition(
        fd2_field_game *game, fd2_vga *vga, fd2_field_audio *audio,
        int center_screen_x, int center_screen_y,
        int radius, int radius_step, int timed) {
    if(!game || !vga)return -1;
    if(fd2_input_take_quit(&vga->input))return 1;
    fd2_field_effect_frames frames = {0};
    if (fd2_field_game_prepare_stage_transition(
            game, vga, center_screen_x, center_screen_y,
            radius, radius_step, &frames) != 0)
        return -1;
    int result;
    if(vga->field_page) {
        fd2_field_page *base=fd2_field_page_create();
        if(!base) {fd2_field_effect_frames_close(&frames);return -1;}
        result=fd2_field_game_effect_terrain_begin(game,vga,base);
        field_transition_live live={game,base,center_screen_x,center_screen_y,radius,radius_step};
        if(!result)result=fd2_field_effect_play_hooked(&frames,vga,audio,timed,
            transition_live_frame,&live);
        fd2_field_page_destroy(base);
    } else result=fd2_field_effect_play(&frames,vga,audio,timed);
    fd2_field_effect_frames_close(&frames);
    if (result != 0 || !timed) return result;
    /* field_stage_transition_effect @0x24618（原登记FUN_0004982c）：
     * 原尾500ms后32次11df2加色0..62/每级4ms，不能当作渐暗fade。 */
    result=fd2_scene_stage_transition_finish(vga);
    return result==FD2_SCENE_RESULT_HOST_QUIT ? 1 : result==FD2_SCENE_RESULT_OK ? 0 : -1;
}
