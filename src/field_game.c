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
 * 草药提交依据sub_20C6F @0x20c6f（原登记同名，仅code5分支）、
 * field_hp_heal_group_execute @0x211a4（原sub_211A4）的逐目标HP恢复与
 * field_unit_inventory_slot_remove @0x1b8e7（原sub_1B8E7）。
 */

#include "field_game.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "field_attack.h"
#include "field_move.h"
#include "field_move_profile.h"
#include "field_item.h"
#include "field_magic.h"
#include "portrait.h"
#include "save.h"
#include "field_unit_base.h"
#include "field_unit_stats.h"

#define FD2_FIELD_BIOS_TICK_MS 55
#define FD2_FIELD_IDLE_MS 275
#define FD2_FIELD_RANGE_VISUAL_MS 165 /* 3 个约 55 ms 的 BIOS tick */
/* field_command_menu_wait_key @code0 0x7898 每超过 3 个 BIOS tick 切换一次
 * normal/highlight 帧，即约 4×55 ms。 */
#define FD2_FIELD_COMMAND_HIGHLIGHT_MS 220
#define FD2_FIELD_AUTOMATIC_ACTION_MS 150

static int recover_stationary_unit(fd2_field_game *game, size_t actor_index);
static int focus_to(fd2_field_game *game, int x, int y);
typedef enum {
    PLAYER_ACTION_WAIT,
    PLAYER_ACTION_PHYSICAL,
    PLAYER_ACTION_MAGIC,
    PLAYER_ACTION_ITEM
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

/* field_defeated_units_finalize @0x1db65（原登记同名）：有屏内可见死者
 * 才旋转全表可见死者，最终direction0；之后全部HP0 flags精确1。
 * 原始51a87/51a8b为13/8。只省略帧，不省略可观察记录终态。 */
static void finalize_zero_hp_units(fd2_field_game *game) {
    if (!game) return;
    fd2_field_units *units = &game->units;
    int rotate = 0;
    for (size_t i = 0; i < units->count; i++) {
        const fd2_field_unit *unit = &units->items[i];
        if (unit->hp == 0 && !fd2_field_unit_is_hidden(unit) &&
            unit->x >= (int64_t)game->camera_cell_x - 1 &&
            unit->x <= (int64_t)game->camera_cell_x + FD2_FIELD_VIEW_CELLS_X &&
            unit->y >= (int64_t)game->camera_cell_y - 1 &&
            unit->y <= (int64_t)game->camera_cell_y + FD2_FIELD_VIEW_CELLS_Y + 1) {
            rotate = 1;
            break;
        }
    }
    for (size_t i = 0; i < units->count; i++) {
        fd2_field_unit *unit = &units->items[i];
        if (unit->hp != 0) continue;
        if (rotate && !fd2_field_unit_is_hidden(unit)) unit->direction = 0;
        unit->flags = 1;
    }
}

static int known_item_stat_lookup(void *userdata, uint8_t item_id,
                                  fd2_field_item_stat_effect *effect) {
    (void)userdata;
    if (!effect) return -1;
    const uint8_t *record = fd2_field_item_record_get(item_id);
    if (!record) return -1;
    effect->attack = fd2_field_item_i16(record, 1);
    effect->accuracy = fd2_field_item_i16(record, 3);
    effect->defense = fd2_field_item_i16(record, 5);
    effect->evasion = fd2_field_item_i16(record, 7);
    return 0;
}

static int known_status_stat_scale(void *userdata,
                                   fd2_field_unit_scaled_stat stat,
                                   int32_t value,
                                   int32_t *scaled_value) {
    (void)userdata;
    (void)stat;
    if (!scaled_value) return -1;
    /* corrected dual 0x1b7ef/0x1b80a 经 fixup 指向 object2
     * DS:0x018d 的 double 1.15；x87 frndint/fistp 使用 nearest-even。 */
    int64_t numerator = (int64_t)value * 115;
    int64_t quotient = numerator / 100;
    int64_t remainder = numerator % 100;
    int64_t magnitude = remainder < 0 ? -remainder : remainder;
    if (magnitude > 50 ||
        (magnitude == 50 && (quotient & 1) != 0))
        quotient += numerator < 0 ? -1 : 1;
    if (quotient < INT32_MIN || quotient > INT32_MAX) return -1;
    *scaled_value = (int32_t)quotient;
    return 0;
}

static int append_persistent_player(fd2_field_game *game, uint8_t unit_id) {
    if (game->roster_count >= FD2_SAVE_BATTLE_MAX_ROSTER) return -1;
    fd2_field_unit *record = &game->roster[game->roster_count];
    if (fd2_field_unit_persistent_create(record, unit_id) != 0) return -1;
    game->roster_count++;
    return 0;
}

static int load_stage0_units(fd2_field_game *game) {
    /* new_game_opening_play @0x3231b 在进入 stage 0 前把四名玩家
     * 0/9/4/30 放在 placement 末四个玩家槽；随后
     * field_actor_group_arrival_effect @0x32999 依次加入 group 1/2；
     * 组内 actor 由 field_unit_stage_template_append @0x10c50 的模板重排
     * 与基础表等级公式构造。 */
    static const uint8_t player_ids[] = {0, 9, 4, 30};
    fd2_field_units *units = &game->units;
    const fd2_field_metadata *meta = &game->metadata;
    const fd2_field_placements *placements = &game->placements;
    if (placements->count < 4) return -1;

    fd2_field_units_clear(units);
    game->roster_count = 0;
    size_t player_base = placements->count - 4;
    for (size_t i = 0; i < sizeof(player_ids); i++) {
        const fd2_field_placement *placement =
            &placements->records[player_base + i];
        if (append_persistent_player(game, player_ids[i]) != 0 ||
            fd2_field_units_set(units, units->count,
                                player_ids[i], player_ids[i], 2,
                                placement->x, placement->y) != 0) {
            fd2_field_units_clear(units);
            return -1;
        }
        /* sub_1088D @0x1088d 从持久记录实例化 active，再写部署坐标、
         * cache 与交互字段；不把 active 状态反向作为 roster 初始化。 */
        fd2_field_unit *active = &units->items[i];
        *active = game->roster[i];
        active->x = (uint8_t)placement->x;
        active->y = (uint8_t)placement->y;
        active->direction = active->frame_phase = 0;
        ((uint8_t *)active)[0x31] = 0xff;
    }
    if (fd2_field_units_append_group(units, meta, placements, 1) != 0 ||
        fd2_field_units_append_group(units, meta, placements, 2) != 0 ||
        units->count != 12) {
        fd2_field_units_clear(units);
        return -1;
    }
    return 0;
}

size_t fd2_field_game_group_count(const fd2_field_game *game, uint8_t group) {
    size_t count = 0;
    if (!game) return 0;
    for (size_t i = 0; i < game->units.count; i++) {
        uint16_t source = game->units.source_template_indices[i];
        if (source >= game->metadata.unit_template_count) continue;
        if (game->metadata.unit_templates[source].bytes[0x15] == group)
            count++;
    }
    return count;
}

static int validate_stage0_units(const fd2_field_game *game) {
    static const uint8_t player_ids[] = {0, 9, 4, 30};
    static const uint8_t equipment[][4] = {
        {0, 132, 192, 0xff}, {52, 164, 0xff, 0xff},
        {20, 128, 0xff, 0xff}, {72, 178, 0xff, 0xff},
    };
    static const uint16_t stats[][4] = {
        {16, 12, 97, 2}, {11, 7, 86, 1},
        {26, 6, 92, 2}, {22, 14, 92, 2},
    };
    if (!game || game->units.count != 12 ||
        fd2_field_game_group_count(game, 1) != 4 ||
        fd2_field_game_group_count(game, 2) != 4)
        return -1;

    for (size_t i = 0; i < game->units.count; i++) {
        if (i < sizeof(player_ids)) {
            const fd2_field_unit *player = &game->units.items[i];
            const uint8_t *record = (const uint8_t *)player;
            if (player->unit_id != player_ids[i] ||
                game->units.source_template_indices[i] !=
                    FD2_FIELD_UNIT_NO_TEMPLATE ||
                record[0x0b] != equipment[i][0] ||
                record[0x0d] != equipment[i][1] ||
                record[0x0f] != equipment[i][2] ||
                record[0x11] != equipment[i][3] ||
                player->attack != stats[i][0] ||
                player->defense != stats[i][1] ||
                player->accuracy != stats[i][2] ||
                player->evasion != stats[i][3])
                return -1;
        } else if (game->units.source_template_indices[i] !=
                   (uint16_t)(i - 4)) {
            return -1;
        }
        if (fd2_field_unit_frame_index(&game->units.items[i],
                                       game->units.walk_frames[i], 0) >=
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
    /* 当前总览仅恢复正式 stage 0；其他关卡不借用其条件文本。 */
    uint8_t disabled[FD2_FIELD_SYSTEM_MENU_COUNT] = {0, 0, 0, 0};
    disabled[0] = game->stage != 0;
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
        const uint8_t disabled[] = {0, game->stage != 0u, 0, 0};
        fd2_field_system_menu_set_disabled(&game->system_menu, disabled);
    } else if (game->system_menu.page == FD2_FIELD_SYSTEM_PAGE_OPTIONS) {
        /* 后两项只确认过存档位，尚未实现其可见效果。 */
        const uint8_t disabled[] = {0, 0, 1, 1};
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

fd2_field_battle_result fd2_field_game_battle_result(
        const fd2_field_game *game) {
    if (!game || !game->ready) return FD2_FIELD_BATTLE_ONGOING;
    int have_player = 0;
    int have_hostile = 0;
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (fd2_field_unit_is_hidden(unit) || unit->hp == 0) continue;
        if (unit->side == 2u) have_player = 1;
        if (unit->side == 0u) have_hostile = 1;
    }
    if (game->stage == 0u) {
        /* sub_205B4 @0x205b4（原登记中性名），DS:0x1b19[0]：先把
         * 0x3ecc 设为 2；只要仍有可见 side 0 actor 就清回 0；随后
         * actor 0（新游戏的主角）隐藏时覆盖为 1。这里直接暴露其
         * 可观察结果，避免把未加载的 future group 当作胜负门。
         * 原版只查hidden；HP0门是SDL内部防护，受控原版对照只改hidden位。 */
        const fd2_field_unit *leader = game->units.count != 0
            ? &game->units.items[0] : NULL;
        if (!leader || fd2_field_unit_is_hidden(leader) || leader->hp == 0)
            return FD2_FIELD_BATTLE_DEFEAT;
        if (!have_hostile) return FD2_FIELD_BATTLE_VICTORY;
        return FD2_FIELD_BATTLE_ONGOING;
    }
    if (!have_player) return FD2_FIELD_BATTLE_DEFEAT;
    if (!have_hostile) return FD2_FIELD_BATTLE_VICTORY;
    return FD2_FIELD_BATTLE_ONGOING;
}

static int append_stage0_event_group(fd2_field_game *game, uint8_t group) {
    if (!game || game->stage != 0 ||
        fd2_field_game_group_count(game, group) != 0)
        return -1;

    size_t first = game->units.count;
    if (fd2_field_units_append_group(&game->units, &game->metadata,
                                     &game->placements, group) != 0 ||
        game->units.count == first)
        return -1;

    for (size_t i = first; i < game->units.count; i++) {
        fd2_field_unit *unit = &game->units.items[i];
        if (fd2_field_unit_frame_index(unit, game->units.walk_frames[i], 0) >=
            game->sprites.frame_count)
            return -1;
    }
    return 0;
}

static int apply_stage0_event_script(fd2_field_game *game,
                                     uint8_t script_id) {
    const uint8_t *script;
    size_t script_size;
    if (!game || fd2_field_stage0_movement_script_get(
            script_id, &script, &script_size) != 0)
        return -1;
    return fd2_field_movement_script_apply(&game->units, script,
                                            script_size);
}

static int execute_known_turn_event(fd2_field_game *game, uint8_t action) {
    if (!game || game->stage != 0 || action > 3) return -1;

    fd2_field_units saved_units = game->units;
    fd2_field_unit saved_roster[FD2_SAVE_BATTLE_MAX_ROSTER];
    memcpy(saved_roster, game->roster, sizeof(saved_roster));
    size_t saved_roster_count = game->roster_count;
    int saved_camera_x = game->camera_cell_x;
    int saved_camera_y = game->camera_cell_y;
    int result = -1;

    /* field_stage0_31_turn_action0..3 @0x34531/0x3460b/0x34673/
     * 0x346cd。session 先事务化提交 group、镜头和移动脚本；scene 演出层
     * 再从 notice 的提交前快照重放镜头、移动、LMI1 与对话。 */
    switch (action) {
        case 0:
            if (append_persistent_player(game, 1) != 0 ||
                append_stage0_event_group(game, 3) != 0) break;
            game->camera_cell_x = clamp_int(5, 0, max_camera_x(game));
            game->camera_cell_y = clamp_int(8, 0, max_camera_y(game));
            if (apply_stage0_event_script(game, 7) != 0 ||
                append_stage0_event_group(game, 7) != 0 ||
                apply_stage0_event_script(game, 8) != 0)
                break;
            result = 0;
            break;
        case 1:
            game->camera_cell_x = clamp_int(11, 0, max_camera_x(game));
            game->camera_cell_y = clamp_int(16, 0, max_camera_y(game));
            if (append_stage0_event_group(game, 4) == 0 &&
                apply_stage0_event_script(game, 3) == 0)
                result = 0;
            break;
        case 2:
            game->camera_cell_x = clamp_int(0, 0, max_camera_x(game));
            game->camera_cell_y = clamp_int(16, 0, max_camera_y(game));
            if (append_stage0_event_group(game, 5) == 0 &&
                apply_stage0_event_script(game, 4) == 0)
                result = 0;
            break;
        case 3:
            game->camera_cell_x = clamp_int(11, 0, max_camera_x(game));
            game->camera_cell_y = clamp_int(11, 0, max_camera_y(game));
            if (append_stage0_event_group(game, 6) == 0 &&
                apply_stage0_event_script(game, 6) == 0)
                result = 0;
            break;
    }

    if (result != 0) {
        game->units = saved_units;
        memcpy(game->roster, saved_roster, sizeof(saved_roster));
        game->roster_count = saved_roster_count;
        game->camera_cell_x = saved_camera_x;
        game->camera_cell_y = saved_camera_y;
    }
    return result;
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

static void record_turn_events(fd2_field_game *game) {
    if (!game || game->failure != FD2_FIELD_FAILURE_NONE) return;
    fd2_field_turn_event_match matches[FD2_FIELD_EVENT_SLOTS];
    size_t count = fd2_field_turn_events_find(
        &game->metadata, game->turn_number, game->active_side,
        matches, FD2_FIELD_EVENT_SLOTS);
    if (count > FD2_FIELD_EVENT_SLOTS) count = FD2_FIELD_EVENT_SLOTS;
    for (size_t i = 0; i < count; i++) {
        size_t presentation_unit_count = game->units.count;
        int presentation_camera_x = game->camera_cell_x;
        int presentation_camera_y = game->camera_cell_y;
        int presentation_focus_x = game->cursor_cell_x;
        int presentation_focus_y = game->cursor_cell_y;
        fd2_field_event_notice notice = {
            .kind = FD2_FIELD_EVENT_TURN,
            .turn_number = game->turn_number,
            .phase = game->active_side,
            .action = matches[i].action,
            .unit_index = UINT8_MAX,
            .presentation_unit_count = (uint8_t)presentation_unit_count,
            .presentation_camera_x = (int16_t)presentation_camera_x,
            .presentation_camera_y = (int16_t)presentation_camera_y,
            .presentation_focus_x = (int16_t)presentation_focus_x,
            .presentation_focus_y = (int16_t)presentation_focus_y,
            .slot = matches[i].slot,
        };
        if (reserve_event_notice(game, &notice) != 0) return;
        int handled = execute_known_turn_event(game, matches[i].action) == 0;
        notice.handled = (uint8_t)handled;
        notice.presentation_deferred = (uint8_t)handled;
        /* field_turn_event_check @0x1a813 的 DS:0x1b91 已确认为 cdecl
         * handler(actor_index) 表；回合事件传 0。未知 action 或事务失败
         * 仍保留 unhandled 记录，不能执行部分脚本。 */
        append_event_notice(game, &notice);
        if (!handled) {
            fail_session(game,
                game->stage != 0 || matches[i].action > 3
                    ? FD2_FIELD_FAILURE_EVENT_UNSUPPORTED
                    : FD2_FIELD_FAILURE_EVENT_EXECUTION, &notice);
            return;
        }
    }
}

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
    if (!game->pending_cell_event.valid) return 0;
    const fd2_field_unit *actor = &game->units.items[actor_index];
    fd2_field_event_notice notice = {
        .kind = FD2_FIELD_EVENT_CELL, .turn_number = game->turn_number,
        .phase = game->active_side, .action = game->pending_cell_event.action,
        .unit_index = (uint8_t)actor_index, .x = actor->x, .y = actor->y,
        .slot = game->pending_cell_event.source_slot,
    };
    if (reserve_event_notice(game, &notice) != 0) return -1;
    append_event_notice(game, &notice);
    /* 当前尚无已确认stage-private cell handler；保留已提交动作和pending，
     * 冻结后不重试。成功handler的写集与slot生命周期须随其证据恢复。 */
    fail_session(game, FD2_FIELD_FAILURE_EVENT_UNSUPPORTED, &notice);
    return -1;
}

static void apply_phase_status_tick(fd2_field_game *game, uint8_t side) {
    if (!game) return;
    /* field_phase_status_tick @0x1a866（原登记同名）：先按 raw 顺序给
     * 本阵营可见中毒 actor 扣 maxHP/10、聚焦，再统一清理死亡；HP0与
     * damage0仍聚焦。随后逐个递减+0x22..+0x27，每次归零即聚焦、重算。
     * 当前只恢复这些字段写序，不追加RNG；原版提示／等待及其共享RNG
     * 尚未恢复，不能据此称整个原版状态流程不耗RNG。 */
    for (size_t i = 0; i < game->units.count; i++) {
        fd2_field_unit *unit = &game->units.items[i];
        if (unit->side != side || fd2_field_unit_is_hidden(unit)) continue;
        if (unit->detail_status[0] != 0u) {
            uint16_t damage = (uint16_t)(unit->hp_max / 10u);
            unit->hp = unit->hp > damage
                ? (uint16_t)(unit->hp - damage) : 0u;
            if (focus_to(game, unit->x, unit->y) != 0) {
                fail_action(game, i);
                return;
            }
        }
    }
    finalize_zero_hp_units(game);
    for (size_t i = 0; i < game->units.count; i++) {
        fd2_field_unit *unit = &game->units.items[i];
        if (unit->side != side || fd2_field_unit_is_hidden(unit)) continue;
        /* 指令VA 0x1a99f先递减当前byte，0x1a9b0聚焦；原版提示后
         * 0x1aa10立即重算，才处理下一byte。失败保留已提交的计时／焦点。 */
        uint8_t *status = &unit->attack_status;
        for (size_t offset = 0; offset < 6u; offset++) {
            if (status[offset] == 0u) continue;
            status[offset]--;
            if (status[offset] != 0u) continue;
            if (focus_to(game, unit->x, unit->y) != 0 ||
                fd2_field_unit_combat_stats_recompute(
                    unit, known_item_stat_lookup, NULL,
                    known_status_stat_scale, NULL) != 0) {
                fail_action(game, i);
                return;
            }
        }
    }
    game->battle_result = fd2_field_game_battle_result(game);
}

static void clear_all_acted(fd2_field_game *game) {
    if (!game) return;
    /* field_all_actors_clear_acted @0x13536：遍历完整 raw actor 表，
     * 无视 side 与 hidden，仅执行 flags &= 0x7f。 */
    for (size_t i = 0; i < game->units.count; i++)
        fd2_field_unit_set_acted(&game->units.items[i], 0);
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
    apply_phase_status_tick(game, side);
}

static void recover_unacted_players(fd2_field_game *game) {
    /* field_turn_cycle_run @0x1a30b（原登记 FUN_00017a53）：进入 side 1
     * 前只恢复 side 2、flags &0x81==0、+0x25/+0x26 为零且 HP!=maxHP
     * 的 actor。恢复量为 maxHP/5；即使整数结果为零也 OR acted。 */
    for (size_t i = 0; i < game->units.count; i++) {
        fd2_field_unit *unit = &game->units.items[i];
        if (unit->side != 2u || (unit->flags & 0x81u) != 0u ||
            unit->detail_status[0] != 0u || unit->detail_status[1] != 0u ||
            unit->hp == unit->hp_max)
            continue;
        uint32_t hp = (uint32_t)unit->hp + unit->hp_max / 5u;
        unit->hp = (uint16_t)(hp > unit->hp_max ? unit->hp_max : hp);
        fd2_field_unit_set_acted(unit, 1);
    }
}

static void advance_phase(fd2_field_game *game) {
    if (!game || game->failure != FD2_FIELD_FAILURE_NONE) return;
    /* field_turn_cycle_run @0x1a30b 的阶段顺序：玩家 side 2 完成后直接
     * 进入 side 1；side 1 结束后全表清 acted 再进入 side 0；side 0
     * 结束后回合数加一、再次全表清 acted，最后进入 side 2。
     * raw尾扫描与动作提交已分步，clear可在side1 outer真正返回后、
     * phase0 turn-event/status之前执行，不再延迟到side0首个actor。 */
    if (game->active_side == 2) {
        recover_unacted_players(game);
        enter_phase(game, 1);
    } else if (game->active_side == 1) {
        clear_all_acted(game);
        enter_phase(game, 0);
    } else {
        if (game->turn_number != UINT32_MAX) game->turn_number++;
        clear_all_acted(game);
        game->focus_cycle_unit = 0;
        enter_phase(game, 2);
        /* field_turn_cycle_run @0x1a30b（原登记 FUN_00017a53）末尾：
         * 重置 Z 轮询起点，phase 2 的事件／状态完成后聚焦 raw actor 0。
         * field_focus_move_to_actor @0x12d7b 沿 X 再 Y 逐格移动，复用
         * field_focus_move_to @0x12cea 的镜头阈值；失败不继续改焦点。 */
        if (game->failure == FD2_FIELD_FAILURE_NONE && game->units.count) {
            const fd2_field_unit *leader = &game->units.items[0];
            if (focus_to(game, leader->x, leader->y) != 0)
                fail_action(game, 0);
        }
    }
}

static void clear_view_border(fd2_vga *vga) {
    if (!vga) return;
    /* field_view_render_tiles @0x11eee 只写 VGA (4,4) 起的
     * 312×192 内区，外围保持 palette index 0。 */
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

int fd2_field_game_open(fd2_field_game *game,
                        fd2_vga *vga,
                        const fd2_archive *fdother,
                        size_t stage) {
    if (!game || !vga || !fdother) return -1;
    memset(game, 0, sizeof(*game));
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

    /* 当前只确认 stage 0 的玩家槽和初始 group；其他 stage 不能仅凭
     * placement 猜测当前回合应激活的单位。 */
    if (stage != 0) return -1;

    if (fd2_archive_open(&game->field_archive,
                         "original_game/FDFIELD.DAT") != 0 ||
        fd2_archive_open(&game->fdshap_archive,
                         "original_game/FDSHAP.DAT") != 0 ||
        fd2_archive_open(&game->dato_archive,
                         "original_game/DATO.DAT") != 0 ||
        fd2_archive_open(&game->fdtxt_archive,
                         "original_game/FDTXT.DAT") != 0 ||
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
                                 "original_game/FDICON.B24") != 0 ||
        fd2_field_visuals_open(&game->visuals, fdother) != 0 ||
        fd2_field_info_assets_open(&game->info_assets, fdother) != 0 ||
        fd2_field_command_assets_open(&game->command_assets, fdother) != 0)
        goto fail;

    if (load_stage0_units(game) != 0 ||
        validate_stage0_units(game) != 0)
        goto fail;

    const uint8_t *palette;
    size_t palette_size;
    if (fd2_archive_get(fdother, 0, &palette, &palette_size) != 0 ||
        palette_size < 768)
        goto fail;
    fd2_vga_set_palette(vga, palette);
    fd2_vga_set_brightness(vga, 0);

    game->stage = stage;
    game->camera_cell_x = 0;
    game->camera_cell_y = clamp_int(15, 0, max_camera_y(game));
    game->cursor_cell_x = clamp_int(game->camera_cell_x + 6,
                                    0, game->map.width - 1);
    game->cursor_cell_y = clamp_int(game->camera_cell_y + 4,
                                    0, game->map.height - 1);
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
    enter_phase(game, 2);
    if (game->failure != FD2_FIELD_FAILURE_NONE) goto fail;
    return 0;

fail:
    fd2_field_game_close(game);
    return -1;
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
    game->stage_runtime_entry_offset = 0;
    game->attack_target = -1;
    game->last_attack_valid = 0;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
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

void fd2_field_game_tick(fd2_field_game *game, uint64_t now_ms) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE) return;
    if (game->last_idle_ms == 0) {
        game->last_idle_ms = now_ms;
    } else {
        while (now_ms - game->last_idle_ms >= FD2_FIELD_IDLE_MS) {
            game->idle_phase = (uint8_t)((game->idle_phase + 1u) & 3u);
            game->last_idle_ms += FD2_FIELD_IDLE_MS;
        }
    }
    if (game->last_terrain_visual_ms == 0) {
        game->last_terrain_visual_ms = now_ms;
    } else {
        while (now_ms - game->last_terrain_visual_ms >=
               FD2_FIELD_BIOS_TICK_MS) {
            game->terrain_visual_phase ^= 1u;
            game->last_terrain_visual_ms += FD2_FIELD_BIOS_TICK_MS;
        }
    }
    if (game->last_range_visual_ms == 0) {
        game->last_range_visual_ms = now_ms;
    } else {
        while (now_ms - game->last_range_visual_ms >=
               FD2_FIELD_RANGE_VISUAL_MS) {
            game->range_visual_phase = (uint8_t)(
                (game->range_visual_phase + 1u) %
                FD2_FIELD_RANGE_PHASE_COUNT);
            game->last_range_visual_ms += FD2_FIELD_RANGE_VISUAL_MS;
        }
    }
    if (game->interaction == FD2_FIELD_INTERACTION_COMMAND ||
        game->interaction == FD2_FIELD_INTERACTION_ITEM_MENU) {
        if (game->last_command_highlight_ms == 0) {
            game->last_command_highlight_ms = now_ms;
        } else {
            while (now_ms - game->last_command_highlight_ms >=
                   FD2_FIELD_COMMAND_HIGHLIGHT_MS) {
                game->command_highlight_phase ^= 1u;
                game->last_command_highlight_ms +=
                    FD2_FIELD_COMMAND_HIGHLIGHT_MS;
            }
        }
        game->system_menu.highlight_phase = 0;
        game->system_menu.last_highlight_ms = 0;
    } else if (game->interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU) {
        game->command_highlight_phase = 0;
        game->last_command_highlight_ms = 0;
        if (game->system_menu.last_highlight_ms == 0) {
            game->system_menu.last_highlight_ms = now_ms;
        } else {
            while (now_ms - game->system_menu.last_highlight_ms >=
                   FD2_FIELD_COMMAND_HIGHLIGHT_MS) {
                game->system_menu.highlight_phase ^= 1u;
                game->system_menu.last_highlight_ms +=
                    FD2_FIELD_COMMAND_HIGHLIGHT_MS;
            }
        }
    } else {
        game->command_highlight_phase = 0;
        game->last_command_highlight_ms = 0;
        game->system_menu.highlight_phase = 0;
        game->system_menu.last_highlight_ms = 0;
    }
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
    if (game->active_side != 2 &&
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
                                int camera_x, int camera_y);
static void render_move_selection_overlay(const fd2_field_game *game,
                                          fd2_vga *vga,
                                          int camera_x, int camera_y);
static void render_field_info(fd2_field_game *game, fd2_vga *vga);
static void render_field_command(const fd2_field_game *game,
                                 fd2_vga *vga,
                                 int camera_x, int camera_y);
static void render_field_system_menu(const fd2_field_game *game,
                                     fd2_vga *vga,
                                     int camera_x, int camera_y);
static void render_manual_slot_picker(const fd2_field_game *game,
                                      fd2_vga *vga);
static void render_manual_slot_result(const fd2_field_game *game,
                                      fd2_vga *vga);
static void render_system_result(const fd2_field_game *game,
                                 fd2_vga *vga);

static void render_field(fd2_field_game *game, fd2_vga *vga, int draw_ui) {
    if (!game || !game->ready || !vga) return;

    /* field_actor_move_{down,left,up,right}_follow_camera
     * @0x12eaa/0x1300d/0x13185/0x13315 在六个步态相位中每次同步
     * 卷动镜头 4 px；camera_pixel_offset_* 保存尚未提交到格坐标的部分。 */
    int camera_x = game->camera_cell_x * 24 - FD2_FIELD_VIEW_BORDER +
                   game->camera_pixel_offset_x;
    int camera_y = game->camera_cell_y * 24 - FD2_FIELD_VIEW_BORDER +
                   game->camera_pixel_offset_y;
    fd2_terrain_render_field_base_animated(
        vga, &game->terrain, &game->map, camera_x, camera_y,
        game->terrain_visual_phase, game->idle_phase);
    if (draw_ui) {
        render_move_selection(game, vga, camera_x, camera_y);
        render_field_cursor(game, vga, camera_x, camera_y);
    }
    fd2_field_units_render(vga, &game->sprites, &game->units,
                           camera_x, camera_y, game->idle_phase);
    fd2_terrain_render_field_overlay(
        vga, &game->terrain, &game->map, camera_x, camera_y,
        game->terrain_visual_phase);
    if (draw_ui) render_move_selection_overlay(game, vga, camera_x, camera_y);
    clear_view_border(vga);
    if (!draw_ui) return;
    render_field_info(game, vga);
    render_field_command(game, vga, camera_x, camera_y);
    render_field_system_menu(game, vga, camera_x, camera_y);
    render_manual_slot_picker(game, vga);
    render_system_result(game, vga);
    render_manual_slot_result(game, vga);
    if (game->detail_visible && game->detail_unit >= 0 &&
        (size_t)game->detail_unit < game->units.count) {
        fd2_field_detail_draw(vga, &game->info_assets, &game->font,
                              &game->ui_text,
                              &game->units.items[game->detail_unit],
                              &game->detail_portrait);
        if (game->detail_page == FD2_FIELD_DETAIL_MAGIC)
            (void)fd2_field_magic_list_draw(vga, &game->info_assets, &game->font,
                &game->ui_text, &game->units.items[game->detail_unit], -1);
    }
}

void fd2_field_game_render(fd2_field_game *game, fd2_vga *vga) {
    render_field(game, vga, 1);
}

void fd2_field_game_render_background(fd2_field_game *game, fd2_vga *vga) {
    render_field(game, vga, 0);
}

static void render_field_info(fd2_field_game *game, fd2_vga *vga) {
    if (!game || !vga) return;
    int rel_x = game->cursor_cell_x - game->camera_cell_x;
    int rel_y = game->cursor_cell_y - game->camera_cell_y;
    /* field_cell_info_panel_draw @0x1acf3：光标进入视窗左下角时把面板
     * 移到右侧；进入右下角时移回左侧，其他位置沿用上次一侧。 */
    if (rel_y >= 6 && rel_x <= 2)
        game->info_panel_right = 1;
    else if (rel_y >= 6 && rel_x >= 10)
        game->info_panel_right = 0;

    uint32_t cell = fd2_field_map_cell(&game->map,
                                        game->cursor_cell_x,
                                        game->cursor_cell_y);
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
    int unit_index = fd2_field_game_unit_at(
        game, game->cursor_cell_x, game->cursor_cell_y);
    if (unit_index >= 0) {
        const fd2_field_unit *unit = &game->units.items[unit_index];
        /* 原版面板固定使用 cache_class*12 + idle phase，不随地图朝向
         * 或移动中的 walk frame 改变。SDL 直接 FDICON 视图以 unit ID
         * 替代运行期 cache class。 */
        uint8_t idle_phase = game->idle_phase == 3 ? 1 : game->idle_phase;
        size_t sprite_index = (size_t)unit->unit_id * 12u + idle_phase;
        if (sprite_index < game->sprites.frame_count &&
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
                        game->info_panel_right, terrain_image, unit_image,
                        hp, hp_max, attack, defense);
}

static void render_field_command(const fd2_field_game *game,
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
    /* field_command_menu_draw @0x179d5 在四个图标后再次调用
     * sub_127E0 @0x127e0（原登记同名）重画当前actor，不重铺遮挡tile。
     * 角色上移6px，覆盖上图标末行；最后按原312×192视区裁去边框。 */
    fd2_field_unit_render(vga, &game->sprites, unit,
                          game->units.walk_frames[game->selected_unit],
                          camera_x, camera_y, game->idle_phase);
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
        const int color = selected ? 0xc9 : 0xcd;
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

static void render_system_result(const fd2_field_game *game,
                                 fd2_vga *vga) {
    if (!game || !vga || game->system_result_fragment == 0) return;
    const fd2_image *dialog = &game->info_assets.detail_bottom;
    if (dialog->pixels)
        fd2_map_sprite_blit(vga, dialog, 5, 112, 0);
    const uint8_t *tokens;
    size_t token_count;
    if (fd2_text_entry_get_fragment(&game->ui_text,
                                    game->system_result_fragment,
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

static void render_field_system_menu(const fd2_field_game *game,
                                     fd2_vga *vga, int camera_x, int camera_y) {
    if (!game || !vga || game->interaction !=
            FD2_FIELD_INTERACTION_SYSTEM_MENU)
        return;
    if (game->system_menu.page == FD2_FIELD_SYSTEM_PAGE_CONFIRMATION) {
        const fd2_image *dialog = &game->info_assets.detail_bottom;
        if (dialog->pixels)
            fd2_map_sprite_blit(vga, dialog, 5, 112, 0);
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
        size_t fragment;
        if (fd2_field_system_menu_get_confirmation_fragment(
                &game->system_menu, FD2_FIELD_SYSTEM_CONFIRM_PROMPT,
                &fragment) != 0)
            return;
        const uint8_t *tokens;
        size_t token_count;
        if (fd2_text_entry_get_fragment(&game->ui_text, fragment,
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
         game->interaction != FD2_FIELD_INTERACTION_ITEM_TARGETING) ||
        !game->move_range.nodes)
        return;

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

static void render_move_selection_overlay(const fd2_field_game *game,
                                          fd2_vga *vga,
                                          int camera_x, int camera_y) {
    if (!game || !vga ||
        (game->interaction != FD2_FIELD_INTERACTION_UNIT_SELECTED &&
         game->interaction != FD2_FIELD_INTERACTION_TARGETING &&
         game->interaction != FD2_FIELD_INTERACTION_MAGIC_TARGETING &&
         game->interaction != FD2_FIELD_INTERACTION_ITEM_TARGETING) ||
        !game->move_range.nodes)
        return;
    const uint8_t *lut = fd2_field_range_lut(
        &game->visuals, game->range_visual_phase);
    if (!lut) return;
    for (int y = 0; y < game->move_range.height; y++) {
        for (int x = 0; x < game->move_range.width; x++) {
            if (!fd2_field_path_is_destination(&game->move_range, x, y))
                continue;
            uint32_t cell = fd2_field_map_cell(&game->map, x, y);
            size_t frame_index;
            int overlay = fd2_terrain_overlay_frame_from_cell(
                &game->terrain, cell, game->terrain_visual_phase,
                &frame_index);
            if (overlay <= 0) continue;
            const fd2_image *image = fd2_terrain_frame(
                &game->terrain, frame_index);
            fd2_tileset_blit_lut(vga, image, x * 24 - camera_x,
                                 y * 24 - camera_y, 0, lut);
        }
    }
}

static void render_field_cursor(const fd2_field_game *game,
                                fd2_vga *vga,
                                int camera_x, int camera_y) {
    if (!game || !vga || game->active_side != 2 ||
        game->interaction == FD2_FIELD_INTERACTION_MOVING)
        return;

    /* field_selection_overlay_draw @0x122dc 在普通模式 1 调用
     * field_selection_sprite_blit @0x126f7，把 FDOTHER[1] frame 0
     * 以透明 SKIP 规则画在焦点格；顺序位于地形/范围之后、单位之前。 */
    /* 1cff0/1bbdc将area0选择模式设为2，122dc改画frame1；仍先于actor。 */
    size_t frame = (game->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING ||
                    game->interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING) ? 1u : 0u;
    const fd2_image *cursor = fd2_field_cursor_frame(&game->visuals, frame);
    if (!cursor) return;
    fd2_map_sprite_blit(vga, cursor,
                        game->cursor_cell_x * 24 - camera_x,
                        game->cursor_cell_y * 24 - camera_y, 0);
}

int fd2_field_game_move_camera(fd2_field_game *game, int dx, int dy) {
    if (!game || !game->ready) return 0;
    int old_x = game->camera_cell_x;
    int old_y = game->camera_cell_y;
    game->camera_cell_x = clamp_int(old_x + dx, 0, max_camera_x(game));
    game->camera_cell_y = clamp_int(old_y + dy, 0, max_camera_y(game));
    return game->camera_cell_x != old_x || game->camera_cell_y != old_y;
}

int fd2_field_game_move_cursor(fd2_field_game *game, int dx, int dy) {
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
        fd2_field_game_move_camera(game, -1, 0);
    else if (new_x > old_x && rel_x >= 11)
        fd2_field_game_move_camera(game, 1, 0);
    if (new_y < old_y && rel_y < 2)
        fd2_field_game_move_camera(game, 0, -1);
    else if (new_y > old_y && rel_y >= 6)
        fd2_field_game_move_camera(game, 0, 1);

    game->cursor_cell_x = new_x;
    game->cursor_cell_y = new_y;
    game->detail_acknowledged_unit = -1;
    if (game->interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED)
        (void)fd2_field_game_update_move_preview(game);
    return 1;
}

/* field_focus_move_to @0x12cea（原登记 FUN_00010432）：先X后Y，逐格
 * 复用browse方向移动；不居中，不在同目标时额外卷动。预检两轴后才写入。 */
static int focus_to(fd2_field_game *game, int x, int y) {
    if (!game || !game->ready || x < 0 || y < 0 ||
        x >= game->map.width || y >= game->map.height)
        return -1;
    while (game->cursor_cell_x != x) {
        if (!fd2_field_game_move_cursor(
                game, game->cursor_cell_x < x ? 1 : -1, 0))
            return -1;
    }
    while (game->cursor_cell_y != y) {
        if (!fd2_field_game_move_cursor(
                game, 0, game->cursor_cell_y < y ? 1 : -1))
            return -1;
    }
    return 0;
}

int fd2_field_game_cycle_focus(fd2_field_game *game) {
    if (!game || !game->ready || game->active_side != 2u ||
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

    if (fd2_field_game_compute_move_range(game, (size_t)unit_index,
                                          &game->move_range) != 0 ||
        fd2_field_game_update_move_preview(game) != 0) {
        clear_move_query(game);
        game->selected_unit = -1;
        game->interaction = FD2_FIELD_INTERACTION_BROWSE;
        return -1;
    }
    /* field_player_unit_action @0x18890入口，原controller重入也清同一slot。 */
    game->pending_cell_event.valid = 0;
    return unit_index;
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

    int length = fd2_field_path_build(&game->move_range,
                                      game->cursor_cell_x,
                                      game->cursor_cell_y,
                                      game->move_path,
                                      game->move_path_capacity);
    if (length < 0) return -1;
    game->move_path_length = (size_t)length;
    game->move_preview_valid = 1;
    return 0;
}

int fd2_field_game_open_system_menu(fd2_field_game *game) {
    if (!game || !game->ready || game->active_side != 2u ||
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
             FD2_FIELD_SYSTEM_ACTION_OPTIONS_TOGGLE_PENDING)
        memcpy(game->option_values, game->system_menu.option_values,
               sizeof(game->option_values));
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
    if (!game || !game->ready || !vga ||
        game->interaction != FD2_FIELD_INTERACTION_SYSTEM_MENU ||
        !game->command_assets.ready)
        return -1;
    game->system_menu.animation_opening = opening ? 1u : 0u;
    for (uint8_t phase = 1; phase <= 4u; phase++) {
        game->system_menu.animation_phase = phase;
        fd2_field_game_render(game, vga);
        fd2_vga_present_timed(vga, frame_delay_ms);
    }
    game->system_menu.animation_opening = 0;
    game->system_menu.animation_phase = 0;
    return 0;
}

int fd2_field_game_confirm_cursor(fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        game->player_post_waiting_phase) return -1;

    if (game->interaction == FD2_FIELD_INTERACTION_BROWSE) {
        if (game->active_side != 2) return -1;
        int unit_index = fd2_field_game_unit_at(game,
                                                game->cursor_cell_x,
                                                game->cursor_cell_y);
        if (unit_index < 0 ||
            !unit_can_act_for_side(&game->units.items[unit_index],
                                   game->active_side) ||
            fd2_field_unit_has_acted(&game->units.items[unit_index]) ||
            game->units.items[unit_index].detail_status[1] != 0u ||
            game->units.items[unit_index].unit_id == 0x79u ||
            game->units.items[unit_index].race == 0x0au)
            return -1;
        return begin_move_selection(game, unit_index);
    }

    if (game->interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED) {
        if (fd2_field_game_update_move_preview(game) != 0 ||
            !game->move_preview_valid)
            return -1;
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
    if (game->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING) {
        int unit_index = game->selected_unit;
        return fd2_field_game_resolve_player_magic(game) == 0 ? unit_index : -1;
    }
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_MENU) {
        int unit_index = game->selected_unit;
        return fd2_field_game_item_is_available(game) &&
            fd2_field_game_confirm_item_menu(game) == 0 ? unit_index : -1;
    }
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING) {
        int unit_index = game->selected_unit;
        return fd2_field_game_item_is_available(game) &&
            fd2_field_game_resolve_player_item(game) == 0 ? unit_index : -1;
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

int fd2_field_game_attack_is_available(const fd2_field_game *game) {
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
    if (!game || !game->ready || !vga ||
        (game->interaction != FD2_FIELD_INTERACTION_COMMAND &&
         game->interaction != FD2_FIELD_INTERACTION_ITEM_MENU) ||
        !game->command_assets.ready)
        return -1;
    game->command_animation_opening = opening ? 1u : 0u;
    for (uint8_t phase = 1; phase <= 4u; phase++) {
        game->command_animation_phase = phase;
        fd2_field_game_render(game, vga);
        fd2_vga_present_timed(vga, frame_delay_ms);
    }
    game->command_animation_opening = 0;
    game->command_animation_phase = 0;
    return 0;
}

static int magic_list_actor_is_valid(const fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
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
    return fd2_field_magic_list_select_step(
        &game->units.items[game->selected_unit], &game->magic_list_selected,
        key, confirmed_id);
}

int fd2_field_game_end_magic_list(fd2_field_game *game) {
    if (!magic_list_actor_is_valid(game) ||
        game->interaction != FD2_FIELD_INTERACTION_MAGIC_LIST)
        return -1;
    game->magic_list_selected = 0;
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    return fd2_field_game_refresh_commands(game);
}

/* field_player_item_command_execute @0x1bbdc（原登记同名）与
 * field_unit_inventory_select @0x1b932（原sub_1B932）：不套MAGIC的
 * MP/+27/移动后unit例外门，纯库存预检复用1b9de的SDL支持门。 */
static int item_menu_actor_is_valid(const fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        game->stage != 0 || game->player_post_waiting_phase || game->active_side != 2 ||
        game->units.count > FD2_FIELD_MAX_UNITS || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count)
        return 0;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    int selected = 0;
    return unit_can_act_for_side(actor, 2) && !fd2_field_unit_has_acted(actor) &&
        fd2_field_item_list_select_step(actor, &selected, FD2_FIELD_ITEM_LIST_KEY_OTHER, 1) == 0;
}

int fd2_field_game_item_is_available(const fd2_field_game *game) {
    if (!item_menu_actor_is_valid(game) || !game->attack_rng) return 0;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    const uint8_t *raw = (const uint8_t *)actor;
    size_t count = fd2_field_unit_inventory_count(actor);
    for (size_t i = 0; i < count; i++) {
        uint8_t id = raw[0x0bu + i * 2u];
        const uint8_t *record = fd2_field_item_record_get(id);
        /* code0确认留列表；其余Use只开放192，不让未知handler进入正式UI。 */
        if (record[0x0d] != 0 && id != 192) return 0;
    }
    return 1;
}

static void enter_item_menu(fd2_field_game *game) {
    /* 原版[Use,Give,Equip,Drop]按场景计算；这里是SDL尚未开放的能力门。 */
    game->command_disabled[0] = 0;
    for (size_t i = 1; i < FD2_FIELD_COMMAND_COUNT; i++) game->command_disabled[i] = 1;
    game->command_selected = 0;
    game->command_highlight_phase = 0;
    game->command_animation_phase = 0;
    game->command_animation_opening = 0;
    game->last_command_highlight_ms = 0;
    game->item_list_selected = 0;
    game->interaction = FD2_FIELD_INTERACTION_ITEM_MENU;
}

int fd2_field_game_begin_item_menu(fd2_field_game *game) {
    if (!item_menu_actor_is_valid(game) || game->interaction != FD2_FIELD_INTERACTION_COMMAND)
        return -1;
    enter_item_menu(game);
    return 0;
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
        game->command_selected != 0 || game->command_disabled[0])
        return -1;
    game->item_list_selected = 0;
    game->interaction = FD2_FIELD_INTERACTION_ITEM_LIST;
    return 0;
}

int fd2_field_game_item_list_step(fd2_field_game *game, fd2_field_item_list_key key) {
    if (!item_menu_actor_is_valid(game) || game->interaction != FD2_FIELD_INTERACTION_ITEM_LIST)
        return -2;
    return fd2_field_item_list_select_step(&game->units.items[game->selected_unit],
                                          &game->item_list_selected, key, 1);
}

int fd2_field_game_end_item_list(fd2_field_game *game) {
    if (!item_menu_actor_is_valid(game) || game->interaction != FD2_FIELD_INTERACTION_ITEM_LIST)
        return -1;
    int selected = game->item_list_selected;
    if (fd2_field_item_list_select_step(&game->units.items[game->selected_unit],
            &selected, FD2_FIELD_ITEM_LIST_KEY_OTHER, 1) != 0)
        return -1;
    /* 18d8c @0x19060在Use取消后重跑整个1bbdc，不返回COMMAND。 */
    enter_item_menu(game);
    return 0;
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
    return !fd2_field_unit_is_hidden(unit) &&
        (filter == 0 ? unit->side == 0 : unit->side != 0);
}

static int magic_target_matches(const fd2_field_unit *unit, uint8_t magic_id) {
    /* 14818/14742共享filter：ID0为side0，ID13为side!=0；
     * 不加HP/acted/占格门，不使用敌方AI的filter反转。 */
    return target_matches_filter(unit, magic_id == 0 ? 0 : 1);
}

static int magic_target_context_is_valid(const fd2_field_game *game) {
    return magic_list_actor_is_valid(game) &&
        game->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING &&
        (game->magic_target_id == 0 || game->magic_target_id == 13) &&
        target_focus_is_valid(game) &&
        game->units.items[game->selected_unit].x < game->map.width &&
        game->units.items[game->selected_unit].y < game->map.height;
}

/* 14818→115b6的范围/候选与focus顺序由法术、物品真实共用；
 * caller先检查各自actor、record、列表索引与焦点，不混用消费门。 */
static int begin_target_selection(fd2_field_game *game, uint8_t cast,
        uint8_t filter, fd2_field_interaction interaction, uint8_t magic_id) {
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    if (fd2_field_target_range_compute(&range, &game->map, &game->terrain,
            actor->x, actor->y, 0, cast) != 0)
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
    /* 所有范围/坐标预检已完成，focus_to不再有可失败的数据路径。 */
    fd2_field_path_close(&game->move_range);
    game->move_range = range;
    game->magic_target_id = magic_id;
    memset(game->magic_target_candidates, 0, sizeof(game->magic_target_candidates));
    memcpy(game->magic_target_candidates, candidates, count);
    game->magic_target_count = count;
    game->magic_target_cycle = 0;
    game->interaction = interaction;
    if (count && game->units.items[candidates[0]].unit_id != 121)
        (void)focus_to(game, game->units.items[candidates[0]].x,
                             game->units.items[candidates[0]].y);
    return 0;
}

int fd2_field_game_begin_magic_targeting(fd2_field_game *game, uint8_t magic_id) {
    if (!magic_list_actor_is_valid(game) ||
        game->interaction != FD2_FIELD_INTERACTION_MAGIC_LIST ||
        (magic_id != 0 && magic_id != 13) || !target_focus_is_valid(game))
        return -1;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    uint8_t confirmed = 0xff;
    int selected = game->magic_list_selected;
    if (fd2_field_unit_magic_list_build(actor, NULL, 0) > FD2_FIELD_MAGIC_LIST_MAX_COUNT ||
        game->cursor_cell_x != actor->x || game->cursor_cell_y != actor->y ||
        fd2_field_magic_list_select_step(actor, &selected,
            FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, &confirmed) != 1 || confirmed != magic_id)
        return -1;
    const uint8_t *record = fd2_field_magic_record_get(magic_id);
    return begin_target_selection(game, record[3], magic_id == 0 ? 0 : 1,
                                  FD2_FIELD_INTERACTION_MAGIC_TARGETING, magic_id);
}

int fd2_field_game_magic_target_is_legal(const fd2_field_game *game) {
    if (!magic_target_context_is_valid(game) || !game->move_range.nodes ||
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
    /* 14742的strictRadius=area+1；目前area0只需同格，不把terrain
     * 传播当成多格曼哈顿area。目标可能HP0或acted，仍按原filter收集。 */
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (magic_target_matches(unit, game->magic_target_id) &&
            unit->x == game->cursor_cell_x && unit->y == game->cursor_cell_y)
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
    if (unit->unit_id != 121 && focus_to(game, unit->x, unit->y) != 0)
        return -1;
    /* 11779先递增再取槽；121不移动但仍占槽，不连续跳过。 */
    game->magic_target_cycle = next;
    return 1;
}

int fd2_field_game_cycle_magic_target(fd2_field_game *game) {
    if (!magic_target_context_is_valid(game)) return -1;
    return cycle_target_selection(game);
}

int fd2_field_game_cancel_magic_targeting(fd2_field_game *game) {
    if (!magic_target_context_is_valid(game)) return -1;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    if (focus_to(game, actor->x, actor->y) != 0) return -1;
    fd2_field_path_close(&game->move_range);
    game->magic_target_id = 0;
    memset(game->magic_target_candidates, 0, sizeof(game->magic_target_candidates));
    game->magic_target_count = 0;
    game->magic_target_cycle = 0;
    game->magic_list_selected = 0;
    game->interaction = FD2_FIELD_INTERACTION_MAGIC_LIST;
    return 0;
}

/* 1bbdc在1bd14之前按raw槽读record；这里只开放已核草药192，
 * 使用player的cast+10/area+12/filter+15，不借AI的+11筛选。 */
static const uint8_t *item_target_record(const fd2_field_game *game) {
    if (!item_menu_actor_is_valid(game)) return NULL;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    int selected = game->item_list_selected;
    if (fd2_field_item_list_select_step(actor, &selected,
            FD2_FIELD_ITEM_LIST_KEY_CONFIRM, 1) != 1)
        return NULL;
    const uint8_t *raw = (const uint8_t *)actor;
    if (raw[0x0bu + (size_t)selected * 2u] != 192) return NULL;
    const uint8_t *record = fd2_field_item_record_get(192);
    return record && record[0x0d] == 5 && record[0x10] == 1 &&
        record[0x12] == 0 && record[0x15] == 1 ? record : NULL;
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
    return begin_target_selection(game, record[0x10], record[0x15],
                                  FD2_FIELD_INTERACTION_ITEM_TARGETING, 0);
}

int fd2_field_game_item_target_is_legal(const fd2_field_game *game) {
    if (!item_target_context_is_valid(game) || !game->move_range.nodes ||
        game->move_range.width != game->map.width || game->move_range.height != game->map.height)
        return -1;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    if (game->move_range.start_x != actor->x || game->move_range.start_y != actor->y)
        return -1;
    if (!fd2_field_path_is_destination(&game->move_range,
            game->cursor_cell_x, game->cursor_cell_y))
        return 0;
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (target_matches_filter(unit, 1) &&
            unit->x == game->cursor_cell_x && unit->y == game->cursor_cell_y)
            return 1;
    }
    return 0;
}

int fd2_field_game_cycle_item_target(fd2_field_game *game) {
    if (!item_target_context_is_valid(game)) return -1;
    return cycle_target_selection(game);
}

int fd2_field_game_cancel_item_targeting(fd2_field_game *game) {
    if (!item_target_context_is_valid(game)) return -1;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    if (focus_to(game, actor->x, actor->y) != 0) return -1;
    fd2_field_path_close(&game->move_range);
    game->magic_target_id = 0;
    memset(game->magic_target_candidates, 0, sizeof(game->magic_target_candidates));
    game->magic_target_count = 0;
    game->magic_target_cycle = 0;
    /* 1be60先focus，1be85重开1b932的selected0；不返回物品子菜单。 */
    game->item_list_selected = 0;
    game->interaction = FD2_FIELD_INTERACTION_ITEM_LIST;
    return 0;
}

static int player_basic_magic_list_is_supported(const fd2_field_game *game) {
    if (!magic_list_actor_is_valid(game) || game->stage != 0) return 0;
    const fd2_field_unit *actor = &game->units.items[game->selected_unit];
    uint8_t ids[40];
    size_t count = fd2_field_unit_magic_list_build(actor, ids, sizeof(ids));
    if (!count || count > FD2_FIELD_MAGIC_LIST_MAX_COUNT) return 0;
    for (size_t i = 0; i < count; i++) {
        /* ID0玩家强制2ff01；目前仅unit9的FIGANI29空→28
         * 有已核单个caster费用标记。13的21b18在目标循环前扣一次。 */
        if (ids[i] != 13 && !(ids[i] == 0 && actor->unit_id == 9)) return 0;
    }
    return 1;
}

int fd2_field_game_magic_is_available(const fd2_field_game *game) {
    return game && game->attack_rng && game->interaction == FD2_FIELD_INTERACTION_COMMAND &&
        player_basic_magic_list_is_supported(game);
}

static int player_effect_tail_is_supported(const fd2_field_game *game,
                                            size_t actor_index,
                                            int heals_focused_cell) {
    const fd2_field_unit *actor = &game->units.items[actor_index];
    uint8_t action;
    size_t slot;
    int found = fd2_field_cell_event_find(&game->map, &game->terrain,
        &game->metadata, actor->x, actor->y, 1, &action, &slot);
    /* 最终match1覆盖旧pending；当前尚无已支持cell handler，不能在effect
     * 后才发现必经未知handler。特殊地形不是这些command的WAIT cell-action。 */
    if (found < 0 || found || game->pending_cell_event.valid) return 0;
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (i != actor_index && unit->side == 2 &&
            (unit->flags & 0x81u) == 0 && !unit->detail_status[1]) {
            /* 预测本次finalize后的flags，不改13565本体的无HP门契约。
             * 治疗同格HP0已通过maxHP预检，必先恢复存活；其他HP0将隐藏。 */
            if (!unit->hp && !(heals_focused_cell &&
                    unit->x == game->cursor_cell_x && unit->y == game->cursor_cell_y))
                continue;
            return 1;
        }
    }
    /* 最后一名玩家会立即进入side1；未知回合action和notice容量先拒绝。
     * 已知handler本身的执行失败仍由已提交后的controller保留事实并冻结。 */
    fd2_field_turn_event_match matches[FD2_FIELD_EVENT_SLOTS];
    size_t count = fd2_field_turn_events_find(&game->metadata,
        game->turn_number, 1, matches, FD2_FIELD_EVENT_SLOTS);
    if (count > FD2_FIELD_EVENT_SLOTS || game->event_log_count > FD2_FIELD_EVENT_LOG_CAPACITY ||
        count > FD2_FIELD_EVENT_LOG_CAPACITY - game->event_log_count)
        return 0;
    for (size_t i = 0; i < count; i++) if (matches[i].action > 3) return 0;
    return 1;
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
    const uint8_t *record = fd2_field_magic_record_get(id);
    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    if (fd2_field_target_range_compute(&range, &game->map, &game->terrain,
            actor->x, actor->y, 0, record[3]) != 0) return -1;
    int legal_cell = fd2_field_path_is_destination(&range,
        game->cursor_cell_x, game->cursor_cell_y);
    fd2_field_path_close(&range);
    if (!legal_cell) return -1;
    uint8_t targets[FD2_FIELD_MAX_UNITS];
    size_t count = 0;
    uint64_t maximum_reward = 0;
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (fd2_field_unit_is_hidden(unit)) continue;
        if (unit->x >= game->map.width || unit->y >= game->map.height) return -1;
        int target = magic_target_matches(unit, id) &&
            unit->x == game->cursor_cell_x && unit->y == game->cursor_cell_y;
        /* 1b6b7在死亡清理前扫描完整表。基础流程不接+31..33奖励描述符；
         * 非目标原有HP0同样不能把未知掉落/脚本静默丢掉。 */
        if ((target || unit->hp == 0) && ((const uint8_t *)unit)[0x31] != 0xffu)
            return -1;
        if (!target) continue;
        uint32_t maximum;
        if (fd2_field_magic_basic_reward_max(id, unit, &maximum) != 0) return -1;
        maximum_reward += maximum;
        targets[count++] = (uint8_t)i;
    }
    uint8_t maximum_gain;
    if (!count || fd2_field_magic_basic_reward_gain(actor, maximum_reward, &maximum_gain) != 0 ||
        !player_effect_tail_is_supported(game, actor_index, id == 13)) return -1;

    /* 1ca89的u16费用先于首目标effect/RNG；所有数据/整次奖励预检已完成。
     * SDL这里只执行已确认逻辑，不模拟2ff01附加抖动RNG或音画时序。 */
    actor->mp = (uint16_t)(actor->mp - record[5]);
    uint64_t actual_reward = 0;
    for (size_t i = 0; i < count; i++) {
        fd2_field_magic_basic_result effect;
        if (fd2_field_magic_apply_player_basic_effect(id,
                &game->units.items[targets[i]], game->attack_rng,
                game->attack_rng_userdata, &effect) < 0) {
            fail_action(game, actor_index);
            return -1;
        }
        actual_reward += effect.experience_value;
    }
    finalize_zero_hp_units(game);
    game->battle_result = fd2_field_game_battle_result(game);
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    if (finish_unit_action_internal(game, PLAYER_ACTION_MAGIC, actual_reward) != 0) {
        fail_action(game, actor_index);
        return -1;
    }
    return 0;
}

int fd2_field_game_resolve_player_item(fd2_field_game *game) {
    if (!item_target_context_is_valid(game) || !game->attack_rng) return -1;
    size_t actor_index = (size_t)game->selected_unit;
    fd2_field_unit *actor = &game->units.items[actor_index];
    const uint8_t *record = item_target_record(game);
    size_t slot_index = (size_t)game->item_list_selected;
    /* 提交重建当前geometry；目标期旧mask不是可直接信任的提交计划。 */
    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    if (fd2_field_target_range_compute(&range, &game->map, &game->terrain,
            actor->x, actor->y, 0, record[0x10]) != 0) return -1;
    int legal_cell = fd2_field_path_is_destination(&range,
        game->cursor_cell_x, game->cursor_cell_y);
    fd2_field_path_close(&range);
    if (!legal_cell) return -1;
    uint8_t targets[FD2_FIELD_MAX_UNITS];
    size_t count = 0;
    for (size_t i = 0; i < game->units.count; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (fd2_field_unit_is_hidden(unit)) continue;
        if (unit->x >= game->map.width || unit->y >= game->map.height) return -1;
        int target = target_matches_filter(unit, record[0x15]) &&
            unit->x == game->cursor_cell_x && unit->y == game->cursor_cell_y;
        if (target) {
            if (!unit->hp_max || unit->hp > unit->hp_max) return -1;
            targets[count++] = (uint8_t)i;
        } else if (!unit->hp && ((const uint8_t *)unit)[0x31] != 0xffu) {
            /* 1b6b7扫描全表；同格HP0会被本草药救活，其他死者的未知
             * 奖励描述符不能在提交后被静默忽略。 */
            return -1;
        }
    }
    if (!count || !player_effect_tail_is_supported(game, actor_index, 1)) return -1;
    uint16_t power = (uint16_t)record[0x0e] | ((uint16_t)record[0x0f] << 8);
    /* field_hp_heal_group_execute @0x211a4（原sub_211A4）98B逐目标调用
     * field_hp_heal_apply @0x1c916（原sub_1C916）；每target恰一次RNG。
     * sub_20C6F @0x20c6f仅此code5分支随后移除原槽；其清零的是全局
     * 53EC8，不是unit.EX，不能借MAGIC奖励或WAIT恢复尾链。 */
    for (size_t i = 0; i < count; i++) {
        if (fd2_field_hp_heal_apply(&game->units.items[targets[i]], power,
                game->attack_rng, game->attack_rng_userdata) < 0) {
            fail_action(game, actor_index);
            return -1;
        }
    }
    inventory_slot_remove(actor, slot_index);
    finalize_zero_hp_units(game);
    game->battle_result = fd2_field_game_battle_result(game);
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    if (finish_unit_action_internal(game, PLAYER_ACTION_ITEM, 0) != 0) {
        fail_action(game, actor_index);
        return -1;
    }
    return 0;
}

int fd2_field_game_begin_attack(fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        game->interaction != FD2_FIELD_INTERACTION_COMMAND ||
        !fd2_field_game_attack_is_available(game))
        return -1;

    /* field_player_command_execute @0x18d8c 先以武器 min/max range 调用
     * field_target_range_build @0x14818，再把该范围交给
     * field_cell_selection_execute。目标选择期间必须显示这张范围表；
     * 旧实现只在内部做合法性判断，确认非法格时画面没有任何反馈。 */
    fd2_field_path_result attack_range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    const fd2_field_unit *attacker =
        &game->units.items[game->selected_unit];
    if (fd2_field_attack_range_compute(
            &attack_range, &game->map, &game->terrain, attacker) != 0)
        return -1;
    fd2_field_path_close(&game->move_range);
    game->move_range = attack_range;

    game->attack_target = -1;
    game->last_attack_valid = 0;
    game->last_attack_strikes = 0;
    game->last_counterattack_valid = 0;
    game->last_counterattack_strikes = 0;
    game->interaction = FD2_FIELD_INTERACTION_TARGETING;
    return 0;
}

int fd2_field_game_cancel_attack(fd2_field_game *game) {
    if (!game || !game->ready || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count ||
        game->interaction != FD2_FIELD_INTERACTION_TARGETING)
        return 0;
    /* field_player_command_execute_core @0x18d8c 在目标选择返回 -1 后，
     * 调用 field_focus_move_to @0x12cea 恢复攻击前焦点，再返回 0 让
     * 上层重开指令菜单。沿用同一 X 后 Y 顺序以同步恢复镜头。 */
    const fd2_field_unit *attacker =
        &game->units.items[game->selected_unit];
    if (focus_to(game, attacker->x, attacker->y) != 0) return 0;
    game->attack_target = -1;
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

static int resolve_attack_sequence(
        fd2_field_game *game,
        fd2_field_unit *attacker,
        fd2_field_unit *defender,
        fd2_field_attack_result *last_result,
        uint8_t *resolved_strikes) {
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
    if (fd2_field_attack_sequence_count(
            attacker, game->attack_rng,
            game->attack_rng_userdata, &strike_limit) != 0)
        return -1;

    *resolved_strikes = 0;
    for (uint8_t strike = 0; strike < strike_limit; strike++) {
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
        (*resolved_strikes)++;
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

static int resolve_physical_exchange(fd2_field_game *game,
                                     size_t attacker_index,
                                     size_t target_index,
                                     int finish_player_action) {
    if (!game || !game->ready ||
        physical_exchange_preflight(
            game, attacker_index, target_index, 0) != 0)
        return -1;
    fd2_field_unit *attacker = &game->units.items[attacker_index];
    fd2_field_unit *defender = &game->units.items[target_index];
    fd2_field_attack_result attack_result;
    fd2_field_attack_result counterattack_result;
    uint8_t attack_strikes = 0;
    uint8_t counterattack_strikes = 0;

    /* field_physical_exchange @0x1548e 先执行进攻序列；目标存活且
     * field_counterattack_is_available @0x1f0dc 返回 1 时交换双方参数，
     * 再执行反击序列。反击不占用反击者所属阵营的行动标志。 */
    if (resolve_attack_sequence(game, attacker, defender, &attack_result,
                                &attack_strikes) != 0) {
        fail_action(game, attacker_index);
        return -1;
    }
    int counterattack_valid =
        defender->hp != 0 &&
        fd2_field_attack_counterattack_is_available(defender, attacker);
    if (counterattack_valid &&
        resolve_attack_sequence(game, defender, attacker,
                                &counterattack_result,
                                &counterattack_strikes) != 0) {
        fail_action(game, attacker_index);
        return -1;
    }

    /* field_physical_attack_resolve @0x1ecc7 本体只写 +0x25/+0x40；外围
     * exchange 返回后立即调用 field_defeated_units_finalize @0x1db65，
     * 统一将所有 HP==0 actor 的 +0x05 写为 1。 */
    finalize_zero_hp_units(game);
    game->battle_result = fd2_field_game_battle_result(game);
    game->attack_target = (int)target_index;
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
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
    }
    game->last_attack = attack_result;
    game->last_attack_strikes = attack_strikes;
    game->last_attack_valid = 1;
    game->last_counterattack_strikes = counterattack_strikes;
    game->last_counterattack_valid = (uint8_t)counterattack_valid;
    if (counterattack_valid)
        game->last_counterattack = counterattack_result;
    return 0;
}

int fd2_field_game_resolve_attack(fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
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
    if (!game || !game->ready || !unit) return 0;
    const fd2_field_info_assets *assets = &game->info_assets;
#define MAGIC_IMAGE(image, w, h) \
    ((image).pixels && (image).width == (w) && (image).height == (h))
    if (!assets->ready || !game->font.glyphs ||
        game->font.glyph_count == 0 ||
        game->font.glyph_count > game->font.size / 32u ||
        !MAGIC_IMAGE(assets->detail_top, 223, 86) ||
        !MAGIC_IMAGE(assets->affiliation[unit->side == 0], 29, 24))
        return 0;
    for (size_t i = 1; i < 18; i++) {
        int w = i <= 4 || i == 10 || i == 11 || i >= 14 ? 3 : 16;
        int h = i <= 9 || i == 12 ? 3 : 16;
        if (!MAGIC_IMAGE(assets->detail_border[i], w, h)) return 0;
    }
    for (size_t i = 0; i < 2; i++)
        if (!MAGIC_IMAGE(assets->bars[i], 1, 5)) return 0;
    for (size_t i = 0; i < 3; i++) {
        if (unit->detail_status[i] &&
            !MAGIC_IMAGE(assets->status_icons[i], 24, 20)) return 0;
        if (!MAGIC_IMAGE(assets->digit_overflow[i], i == 2 ? 17 : 18, 8)) return 0;
        for (size_t digit = 0; digit < 10; digit++)
            if (!MAGIC_IMAGE(assets->digits[i][digit], digit == 1 ? 5 : 6, 8))
                return 0;
    }
    if (!MAGIC_IMAGE(assets->two_digit_overflow, 12, 8)) return 0;
#undef MAGIC_IMAGE
    const size_t fragments[] = {(size_t)unit->text_id + 1u,
        (size_t)unit->race + 0x8cu, (size_t)unit->movement_profile + 0x96u};
    for (size_t i = 0; i < 3; i++) {
        const uint8_t *tokens;
        size_t count;
        if (!detail_fragment_ready(game, fragments[i], &tokens, &count)) return 0;
    }
    return 1;
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
    if (!game || !game->ready || unit_index >= game->units.count ||
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
    if (fd2_input_take_quit(&vga->input)) return 1;
    if (!game || !game->detail_visible || game->detail_unit < 0 ||
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
    game->detail_visible = 0;
    fd2_field_game_render(game, detail);
    memcpy(background, detail->framebuffer, VGA_W * VGA_H);
    game->detail_visible = 1;
    fd2_field_detail_draw(detail, &game->info_assets, &game->font,
        &game->ui_text, &game->units.items[game->detail_unit], &game->detail_portrait);
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
            if (fd2_input_take_quit(&vga->input)) { result = 1; break; }
            fd2_field_detail_transition_frame(vga->framebuffer, detail->framebuffer,
                                    background, phase);
            fd2_vga_present_timed(vga, 16);
            if (fd2_input_take_quit(&vga->input)) { result = 1; break; }
        }
    } else {
        for (int phase = 0; phase < 12; phase++) {
            if (phase_callback)
                phase_callback(phase_userdata, 0, phase);
            if (fd2_input_take_quit(&vga->input)) { result = 1; break; }
            fd2_field_detail_transition_frame(vga->framebuffer, detail->framebuffer,
                                    background, phase);
            fd2_vga_present_timed(vga, 16);
            if (fd2_input_take_quit(&vga->input)) { result = 1; break; }
        }
        if (result == 0) {
            memcpy(vga->framebuffer, background, VGA_W * VGA_H);
            fd2_vga_present(vga);
            if (fd2_input_take_quit(&vga->input)) result = 1;
        }
    }
    fd2_vga_wait_frame_deadline(vga);
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
    if (fd2_input_take_quit(&vga->input)) return 1;
    if (!game || !game->detail_visible || game->detail_unit < 0 ||
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
    game->detail_visible = 0;
    fd2_field_game_render(game, page);
    memcpy(background, page->framebuffer, VGA_W * VGA_H);
    game->detail_visible = 1;
    fd2_field_detail_draw(page, &game->info_assets, &game->font,
        &game->ui_text, &game->units.items[game->detail_unit], &game->detail_portrait);
    if (phase_callback) phase_callback(phase_userdata, 0, 0);
    for (int phase = 0; phase <= 6; phase++) {
        if (fd2_input_take_quit(&vga->input)) { result = 1; goto done; }
        detail_bottom_frame(vga->framebuffer, page->framebuffer, background, phase);
        fd2_vga_present_timed(vga, 16);
        if (fd2_input_take_quit(&vga->input)) { result = 1; goto done; }
    }
    detail_bottom_frame(vga->framebuffer, page->framebuffer, background, 7);
    fd2_vga_present(vga); /* 原版独立空底页，没有额外16ms计时门。 */
    if (fd2_input_take_quit(&vga->input)) { result = 1; goto done; }
    if (fd2_field_magic_list_draw(page, &game->info_assets, &game->font,
            &game->ui_text, &game->units.items[game->detail_unit], -1) != 0)
        goto done;
    game->detail_page = FD2_FIELD_DETAIL_MAGIC;
    if (phase_callback) phase_callback(phase_userdata, 1, 11);
    for (int phase = 6; phase >= 0; phase--) {
        if (fd2_input_take_quit(&vga->input)) { result = 1; goto done; }
        detail_bottom_frame(vga->framebuffer, page->framebuffer, background, phase);
        fd2_vga_present_timed(vga, 16);
        if (fd2_input_take_quit(&vga->input)) { result = 1; goto done; }
    }
    result = 0;
done:
    fd2_vga_wait_frame_deadline(vga);
    free(page);
    free(background);
    return result;
}

int fd2_field_game_cancel_selection(fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        game->player_post_waiting_phase) return 0;
    if (game->interaction == FD2_FIELD_INTERACTION_MANUAL_SLOT)
        return fd2_field_game_cancel_manual_slot_picker(game);
    if (game->interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU)
        return fd2_field_game_cancel_system_menu(game) !=
               FD2_FIELD_SYSTEM_ACTION_NONE;
    if (game->interaction == FD2_FIELD_INTERACTION_MAGIC_LIST)
        return fd2_field_game_end_magic_list(game) == 0;
    if (game->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING)
        return fd2_field_game_cancel_magic_targeting(game) == 0;
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING)
        return fd2_field_game_cancel_item_targeting(game) == 0;
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_LIST)
        return fd2_field_game_end_item_list(game) == 0;
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_MENU)
        return fd2_field_game_end_item_menu(game) == 0;
    if (game->selected_unit < 0) return 0;
    if (game->interaction == FD2_FIELD_INTERACTION_TARGETING)
        return fd2_field_game_cancel_attack(game);

    fd2_field_unit *unit = &game->units.items[game->selected_unit];
    if (game->interaction == FD2_FIELD_INTERACTION_COMMAND) {
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
         * item transfer的53C53已提交例外尚未开放，不能套入本分支。 */
        game->pending_cell_event.valid = 0;
        if (fd2_field_game_compute_move_range(
                game, (size_t)game->selected_unit,
                &game->move_range) != 0 ||
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
     * 原版每相位等待一次 BIOS tick，调用方传 55 ms 即为实机节奏。 */
    static const int dx[] = {0, -1, 0, 1};
    static const int dy[] = {1, 0, -1, 0};
    static const uint8_t walk_frames[] = {1, 2, 1, 0, 1, 2};
    if (!game || !game->ready || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count ||
        game->interaction != FD2_FIELD_INTERACTION_MOVING ||
        !game->move_preview_valid || !game->move_path)
        return -1;

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
            game->units.walk_frames[game->selected_unit] = walk_frames[phase];
            unit->frame_phase = (uint8_t)phase + 1u;
            game->camera_pixel_offset_x =
                camera_dx * (int)(phase + 1u) * 4;
            game->camera_pixel_offset_y =
                camera_dy * (int)(phase + 1u) * 4;
            if (vga) {
                /* 四方向follow_camera每个步态显式更新水DAC，纯render不推进。 */
                (void)fd2_field_water_tick(vga, SDL_GetTicks());
                /* march的51A83=0取消选择层，51AAC=0关闭常驻信息面板。
                 * 仅本次私有路径参数控制，不留下session显示策略。 */
                if (mode == FIELD_PATH_MARCH_BROWSE)
                    fd2_field_game_render_background(game, vga);
                else
                    fd2_field_game_render(game, vga);
                fd2_vga_present_timed(vga, frame_delay_ms);
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
    if (game->move_path_length != 0)
        for (size_t i = 0; i < game->units.count; i++)
            game->units.items[i].direction = 0;
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    return fd2_field_game_refresh_commands(game);
}

int fd2_field_game_execute_move(fd2_field_game *game,
                                fd2_vga *vga,
                                uint32_t frame_delay_ms) {
    if (!game || game->failure != FD2_FIELD_FAILURE_NONE) return -1;
    return execute_path_playback(game, vga, frame_delay_ms, FIELD_PATH_PLAYER_COMMAND);
}

/* field_controller_input @0x117e7（原登记同名）在13565返回后读取
 * 当前共享slot；AI outer可以清空或覆盖它，不能保存玩家pending副本。 */
static int dispatch_player_controller_pending(fd2_field_game *game,
                                               size_t actor_index) {
    if (dispatch_pending_cell_event(game, actor_index) != 0) return -1;
    game->pending_cell_event.valid = 0; /* controller指令VA 0x119a6 */
    game->player_post_waiting_phase = 0;
    game->player_post_actor = 0;
    return 0;
}

int fd2_field_game_resume_player_controller(fd2_field_game *game) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE)
        return -1;
    if (!game->player_post_waiting_phase) return 0;
    /* native turncycle也可因stage结果在side1/0提前返回controller。 */
    game->battle_result = fd2_field_game_battle_result(game);
    if (game->active_side != 2u &&
        game->battle_result == FD2_FIELD_BATTLE_ONGOING)
        return 1;
    return dispatch_player_controller_pending(game, game->player_post_actor);
}

int fd2_field_game_complete_player_controller(fd2_field_game *game,
                                               size_t actor_index) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        game->player_post_waiting_phase || actor_index >= game->units.count ||
        game->active_side != 2u ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE)
        return -1;
    /* 当前只恢复stage0 handler的全表结果门。经验演出仍属B4边界。 */
    game->battle_result = fd2_field_game_battle_result(game);
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
    advance_phase(game);
    return fd2_field_game_resume_player_controller(game) < 0 ? -1 : 0;
}

/* sub_190AC @0x190ac（原登记同名）的特殊地形不是普通pending事件。
 * shortcut: 宝物/资源确认及库存提交未恢复，WAIT在此支持门显式冻结；
 * 开放其独立handler后替换此门。移动/RNG事实仍保留，不假装wait成功。 */
static int preflight_player_wait_cell(fd2_field_game *game, size_t actor_index) {
    const fd2_field_unit *unit = &game->units.items[actor_index];
    uint32_t cell = fd2_field_map_cell(&game->map, unit->x, unit->y);
    const fd2_terrain_attr *attr = fd2_terrain_attr_get(
        &game->terrain, fd2_field_cell_terrain(cell));
    if (!game->map.cells || unit->x >= game->map.width ||
        unit->y >= game->map.height || !attr) {
        fd2_field_event_notice notice = {
            .kind = FD2_FIELD_EVENT_CELL, .turn_number = game->turn_number,
            .phase = game->active_side, .action = UINT8_MAX,
            .unit_index = (uint8_t)actor_index, .x = unit->x, .y = unit->y,
            .slot = SIZE_MAX,
        };
        fail_session(game, FD2_FIELD_FAILURE_EVENT_DATA, &notice);
        return -1;
    }
    if ((attr->flags & 0x60u) != 0u) {
        fd2_field_event_notice notice = {
            .kind = FD2_FIELD_EVENT_CELL, .turn_number = game->turn_number,
            .phase = game->active_side, .action = UINT8_MAX,
            .unit_index = (uint8_t)actor_index, .x = unit->x, .y = unit->y,
            .slot = (cell >> 16) & 0x1fu,
        };
        fail_session(game, FD2_FIELD_FAILURE_EVENT_UNSUPPORTED, &notice);
        return -1;
    }
    return 0;
}

static int finish_unit_action_internal(fd2_field_game *game,
                                       player_action_kind kind,
                                       uint64_t magic_raw_reward) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
        game->player_post_waiting_phase || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count ||
        game->interaction != FD2_FIELD_INTERACTION_COMMAND)
        return -1;

    size_t unit_index = (size_t)game->selected_unit;
    fd2_field_unit *unit = &game->units.items[unit_index];
    int side_is_actionable = unit_can_act_for_side(unit, game->active_side) ||
        (kind == PLAYER_ACTION_PHYSICAL && unit->side == game->active_side && unit->hp == 0);
    if (!side_is_actionable || fd2_field_unit_has_acted(unit)) return -1;
    /* 只有WAIT执行原地恢复；其他command各保留自己的方向/奖励边界。 */
    if (kind == PLAYER_ACTION_WAIT) {
        if (preflight_player_wait_cell(game, unit_index) != 0) return -1;
        /* sub_13FD4 @0x13fd4：只有原地WAIT恢复，非零路径即使绕回也不恢复。 */
        if (game->move_path_length == 0u &&
            recover_stationary_unit(game, unit_index) < 0)
            return -1;
    }
    if (kind == PLAYER_ACTION_PHYSICAL)
        for (size_t i = 0; i < game->units.count; i++)
            game->units.items[i].direction = 0;
    /* 普通成功command先acted，18890才最终lookup1，再交回controller。 */
    fd2_field_unit_set_acted(unit, 1);
    uint8_t gain = 0;
    if (kind == PLAYER_ACTION_MAGIC &&
        fd2_field_magic_basic_reward_gain(unit, magic_raw_reward, &gain) != 0) {
        fail_action(game, unit_index);
        return -1;
    }
    if (collect_pending_cell_event(game, unit_index, 1) < 0) return -1;
    /* 18d8c先归一，18890 lookup1后controller117e7才调用1e292写EX。
     * 非升级gain已整体预检；提示/portrait/11tick仍待呈现层对照。 */
    if (kind == PLAYER_ACTION_MAGIC) {
        unit->experience = (uint8_t)(unit->experience + gain);
        game->magic_list_selected = 0;
    }
    if (kind == PLAYER_ACTION_ITEM) game->item_list_selected = 0;
    if (kind == PLAYER_ACTION_MAGIC || kind == PLAYER_ACTION_ITEM) {
        game->magic_target_id = 0;
        memset(game->magic_target_candidates, 0, sizeof(game->magic_target_candidates));
        game->magic_target_count = 0;
        game->magic_target_cycle = 0;
    }
    clear_move_query(game);
    game->selected_unit = -1;
    game->command_selected = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    return fd2_field_game_complete_player_controller(game, unit_index);
}

int fd2_field_game_finish_unit_action(fd2_field_game *game) {
    return finish_unit_action_internal(game, PLAYER_ACTION_WAIT, 0);
}

int fd2_field_game_end_active_phase(fd2_field_game *game) {
    /* 快速逻辑回归专用的 phase skip；正式玩家输入不调用此全 acted 路径。 */
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
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
        game->active_side != 2u || game->player_post_waiting_phase ||
        game->selected_unit >= 0 ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE)
        return -1;
    /* field_empty_focus_menu_execute @0x16f55（原登记同名）的 selector 3
     * 确认后直接进入 turn_cycle，不先给全部玩家设置 acted。 */
    advance_phase(game);
    return game->failure == FD2_FIELD_FAILURE_NONE ? 0 : -1;
}

static int finish_ai_action(fd2_field_game *game, size_t actor_index) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
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
     * selection 缓冲。该 helper 还固定延迟 20 ms，SDL 的无视觉提交
     * 只复现状态写入，视觉版已有动作自身的帧延迟。 */
    (void)collect_pending_cell_event(game, actor_index, 1);
    fd2_field_unit_set_acted(actor, 1);
    for (size_t i = 0; i < game->units.count; i++)
        game->units.items[i].direction = 0;
    game->selected_unit = -1;
    game->command_selected = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    return game->failure == FD2_FIELD_FAILURE_NONE ? 0 : -1;
}

/* 直接commit API保留“完整一名AI动作”的现有契约；正式scheduler用内部
 * 未finish版本，确保tail与outer dispatch各执行一次。 */
static int finish_and_dispatch_ai_action(fd2_field_game *game, size_t actor_index) {
    if (finish_ai_action(game, actor_index) != 0) return -1;
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

static int commit_ai_physical_internal(
        fd2_field_game *game,
        size_t actor_index,
        uint8_t side_selector,
        const fd2_field_ai_physical_candidate *plan,
        fd2_vga *vga,
        uint32_t frame_delay_ms,
        int finish_action, field_path_playback_mode mode) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
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
    int path_length = fd2_field_path_build(
        &range, plan->destination_x, plan->destination_y,
        directions, node_count);
    fd2_field_path_close(&range);
    if (path_length < 0) {
        free(directions);
        return -1;
    }

    /* 任何可能失败的 combat/profile hook 都要在动画前拒绝；外部 RNG
     * 没有 checkpoint，故后续 exchange 只进入预检已通过的确定性路径。 */
    if (physical_exchange_preflight(
            game, actor_index, plan->unit_index, 1) != 0) {
        free(directions);
        return -1;
    }

    /* execute_path_playback 可修改镜头、cursor、walk frame 和 event log；
     * 用完整 session 旁路快照回滚，不能只恢复单位表。资源指针在这条
     * 事务中不转移所有权，directions 在恢复前单独释放。 */
    fd2_field_game session_snapshot = *game;
    if (finish_action) game->pending_cell_event.valid = 0;

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
    if (commit_result == 0) {
        fd2_field_unit *target = &game->units.items[plan->unit_index];
        if (actor->hp == 0 || target->hp == 0 ||
            fd2_field_unit_is_hidden(actor) ||
            fd2_field_unit_is_hidden(target)) {
            commit_result = -1;
        } else {
            fd2_field_target_filter filter = side_selector == 0u
                ? FD2_FIELD_TARGET_SIDE_NONZERO
                : FD2_FIELD_TARGET_SIDE_ZERO;
            if (!fd2_field_attack_target_is_legal_for_filter(
                    &game->map, &game->terrain, &game->units,
                    actor_index, plan->unit_index, filter)) {
                commit_result = -1;
            }
            game->cursor_cell_x = target->x;
            game->cursor_cell_y = target->y;
            actor->direction = 0;
            /* field_actor_face_toward @0x1f04a（IDA `sub_1F04A`）：以
             * `abs(dx)` 与 `abs(dy)` 比较两个 actor 记录坐标，水平差
             * 严格大于垂直差时取左右；平价取上下。 */
            int dx = (int)actor->x - (int)target->x;
            int dy = (int)actor->y - (int)target->y;
            int abs_x = dx < 0 ? -dx : dx;
            int abs_y = dy < 0 ? -dy : dy;
            actor->direction = abs_x > abs_y
                ? (actor->x > target->x ? 1u : 3u)
                : (actor->y > target->y ? 2u : 0u);
            if (commit_result == 0) {
                game->interaction = FD2_FIELD_INTERACTION_COMMAND;
                commit_result = resolve_physical_exchange(
                    game, actor_index, plan->unit_index, 0);
            }
        }
    }
    if (commit_result == 0) {
        /* directions 为本次事务私有；不能把已释放的路径指针留在 session。 */
        game->move_path = session_snapshot.move_path;
        game->move_path_capacity = session_snapshot.move_path_capacity;
        game->move_path_length = session_snapshot.move_path_length;
        game->move_preview_valid = session_snapshot.move_preview_valid;
        /* execute_path_playback 清理前仍保存 directions；恢复原始指针后
         * 释放一次，避免 close session 时释放同一地址两次。 */
        free(directions);
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
    /* 所有目标 profile 必须在 RNG 前预检；effect 核心开始后不得因
     * 后续目标的数据错误造成无法回滚的 RNG 流。 */
    if (plan->magic_id <= 12u) {
        for (size_t i = 0; i < target_count; i++) {
            uint32_t ignored_scale = 0;
            if (fd2_field_magic_damage_profile_scale(
                    game->units.items[targets[i]].movement_profile,
                    &ignored_scale) != 0)
                return -1;
        }
    }

    if (finish_action) game->pending_cell_event.valid = 0;
    /* sub_1CA89 @0x1ca89（原登记 sub_1CA89）：map-effect wrapper 在
     * 逐目标循环前单次写 actor+0x44 -= record+5；miss/免疫也不退款。
     * ID17 的 sub_226EA @0x226ea 明确传 cost ID18；两者现值均为5。
     * 此处只恢复无演出分支；FIGANI按caster part+4==1扣费，不假称等价。 */
    const uint8_t *cost_record = fd2_field_magic_record_get(
        plan->magic_id == 17u ? 18u : plan->magic_id);
    actor->mp = (uint16_t)(actor->mp - cost_record[5]);
    for (size_t i = 0; i < target_count; i++) {
        int result = fd2_field_magic_apply_known_effect(
            plan->magic_id, targets[i], &game->units,
            game->attack_rng, game->attack_rng_userdata);
        if (result < 0) {
            fail_action(game, actor_index);
            return -1;
        }
    }
    finalize_zero_hp_units(game);
    game->battle_result = fd2_field_game_battle_result(game);
    game->cursor_cell_x = plan->cast_x;
    game->cursor_cell_y = plan->cast_y;
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
    uint8_t *record = (uint8_t *)unit;
    if (!unit || slot_index >= 8u) return;
    for (size_t slot = slot_index; slot + 1u < 8u; slot++) {
        record[0x0au + slot * 2u] = record[0x0cu + slot * 2u];
        record[0x0bu + slot * 2u] = record[0x0du + slot * 2u];
    }
    /* field_unit_inventory_slot_remove @0x1b8e7（原sub_1B8E7）以memmove
     * 左移 (7-slot)*2 字节，最后只写 flag byte +0x18=0x80；+0x19 的
     * item ID 保留为不可见 stale 值，不能擅自规范化为 0xff。 */
    record[0x18u] = 0x80u;
}

static int commit_ai_item_internal(
        fd2_field_game *game,
        size_t actor_index,
        uint8_t selector_mode,
        const fd2_field_ai_item_candidate *plan,
        int finish_action) {
    if (!game || !game->ready || game->failure != FD2_FIELD_FAILURE_NONE ||
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
    uint16_t value = item ? (uint16_t)item[0x0eu] |
        ((uint16_t)item[0x0fu] << 8) : 0u;
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

    if (finish_action) game->pending_cell_event.valid = 0;
    for (size_t i = 0; i < target_count; i++) {
        int result;
        if (code == 5u || code == 13u) {
            result = fd2_field_hp_heal_apply(&game->units.items[targets[i]], value,
                game->attack_rng, game->attack_rng_userdata);
        } else if (code == 20u || code == 21u || code == 24u) {
            /* item value 是 magic ID；复用同一命中、profile scale 与两次
             * RNG 顺序，不能误当作 raw damage 或 MP 恢复值。 */
            result = fd2_field_magic_apply_known_effect(
                (uint8_t)value, targets[i], &game->units,
                game->attack_rng, game->attack_rng_userdata);
        } else {
            result = -1;
        }
        if (result < 0) {
            fail_action(game, actor_index);
            return -1;
        }
    }
    if (code == 5u) inventory_slot_remove(actor, plan->slot_index);
    finalize_zero_hp_units(game);
    game->battle_result = fd2_field_game_battle_result(game);
    game->cursor_cell_x = plan->target_x;
    game->cursor_cell_y = plan->target_y;
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
static int commit_ai_move_toward_anchor(fd2_field_game *game,
                                         size_t actor_index,
                                         int anchor_x, int anchor_y,
                                         fd2_vga *vga,
                                         uint32_t frame_delay_ms,
                                         field_path_playback_mode mode) {
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
    size_t count = (size_t)game->map.width * (size_t)game->map.height;
    uint8_t *path = malloc(count);
    if (!path) return -1;
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
        /* @14c7d固定预算0x1c/mode1仍探同一请求anchor。路径buffer可复用；
         * 最后普通路径会再次覆盖，不保留或提交该long route。 */
        size_t long_length = 0;
        int long_found = fd2_field_move_path_find(
            &game->map, &game->terrain, &game->units, actor_index,
            profile, FD2_FIELD_MOVEMENT_COST_CLASS_COUNT, 0x1cu,
            FD2_FIELD_MOVE_PATH_EQUAL_ROUTE, anchor_x, anchor_y,
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
    /* @14d47后才最终range/friendly block/sort；Dijkstra的can_stop已表达
     * 146d1友方终点门，仍允许路径经过同阵营actor。 */
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
    if (found != 1 || length == 0) {
        result = found < 0 ? -1 : 0;
        goto done;
    }
    result = commit_ai_path(
        game, actor_index, path, count, length, vga, frame_delay_ms, mode);
done:
    fd2_field_path_close(&range);
    free(path);
    return result;
}

/* field_empty_focus_menu_execute @0x16f55（原登记同名）selector1：
 * 每名玩家只走14b78几何，不经AI decision/common-tail的match1与恢复。
 * 当前actor失败前的路径和之前完成者均已提交，不能退回成普通取消。 */
int fd2_field_game_march(fd2_field_game *game, fd2_vga *vga,
                         uint32_t frame_delay_ms) {
    if (!game || !game->ready || game->stage != 0u ||
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
    for (size_t i = 0; i < game->units.count; i++) {
        fd2_field_unit *actor = &game->units.items[i];
        if ((actor->flags & 0x85u) != 0u || actor->side != 2u) continue;
        if (vga && fd2_input_take_quit(&vga->input)) return 1;
        /* 原版先focus actor，再清共享slot；后续actor不能把移动后的focus
         * 当成新anchor。焦点移动沿现有X→Y镜头阈值，不改变固定anchor。 */
        if (focus_to(game, actor->x, actor->y) != 0) {
            fail_action(game, i);
            return -1;
        }
        game->pending_cell_event.valid = 0;
        if (commit_ai_move_toward_anchor(
                game, i, anchor_x, anchor_y, vga, frame_delay_ms,
                FIELD_PATH_MARCH_BROWSE) < 0) {
            fail_action(game, i);
            return -1;
        }
        if (vga && fd2_input_take_quit(&vga->input)) return 1;
        /* march只有沿途match0；无path也到此，不追加最终match1。 */
        if (dispatch_pending_cell_event(game, i) != 0) return -1;
        for (size_t j = 0; j < game->units.count; j++)
            game->units.items[j].direction = 0;
        fd2_field_unit_set_acted(actor, 1);
        game->selected_unit = -1;
        game->command_selected = -1;
        game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    }
    if (vga) {
        fd2_field_game_render_background(game, vga);
        fd2_vga_present(vga);
        if (fd2_input_take_quit(&vga->input)) return 1;
    }
    /* 整批结束才统一turncycle；沿既有逐actor呈现完整side1→0→new2。
     * march没有玩家controller的额外pending后置，也没有自动Save。 */
    advance_phase(game);
    return game->failure == FD2_FIELD_FAILURE_NONE ? 0 : -1;
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
    return commit_ai_move_toward_anchor(
        game, actor_index, witness_x, witness_y, vga, frame_delay_ms, mode);
}

static int run_ai_try_attack(fd2_field_game *game, size_t actor_index,
                             fd2_vga *vga, uint32_t frame_delay_ms, field_path_playback_mode mode) {
    fd2_field_unit *actor = &game->units.items[actor_index];
    uint8_t selector = game->active_side == 0u ? 0u : 1u;
    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    fd2_field_ai_physical_candidate physical;
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

static int inventory_first_empty_add(fd2_field_unit *unit, uint8_t item_id) {
    if (!unit) return -1;
    uint8_t *record = (uint8_t *)unit;
    for (size_t slot = 0; slot < 8u; slot++) {
        uint8_t *flags = &record[0x0au + slot * 2u];
        if ((*flags & 0x80u) == 0u) continue;
        *flags = 0u;
        record[0x0bu + slot * 2u] = item_id;
        return 1;
    }
    return -1;
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
    actor->ai_behavior_raw =
        (uint8_t)((actor->ai_behavior_raw & 0xf0u) | 7u);
    return 0;
}

static int recover_stationary_unit(fd2_field_game *game, size_t actor_index) {
    if (!game || actor_index >= game->units.count) return -1;
    fd2_field_unit *actor = &game->units.items[actor_index];
    /* 0x13fd4：未满 HP 且 +0x25/+0x26 均为 0 时恢复 maxHP/5。 */
    if (actor->hp >= actor->hp_max || actor->detail_status[0] != 0u ||
        actor->detail_status[1] != 0u)
        return 0;
    uint32_t hp = actor->hp + actor->hp_max / 5u;
    actor->hp = (uint16_t)(hp > actor->hp_max ? actor->hp_max : hp);
    return 1;
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
                                      game->active_side, &target) == 0)
        return commit_ai_move_toward_anchor(
            game, actor_index, target.x, target.y,
            vga, frame_delay_ms, mode);
    return 0;
}

static int move_then_recover(fd2_field_game *game, size_t actor_index,
                             int x, int y,
                             fd2_vga *vga, uint32_t frame_delay_ms, field_path_playback_mode mode) {
    int moved = commit_ai_move_toward_anchor(
        game, actor_index, x, y, vga, frame_delay_ms, mode);
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
            return move_then_recover(
                game, actor_index, target->x, target->y,
                vga, frame_delay_ms, mode);
        }
        case 4:
            return move_then_recover(
                game, actor_index, actor->ai_param_35, actor->ai_param_36,
                vga, frame_delay_ms, mode);
        case 5: {
            int attack = run_ai_try_attack(
                game, actor_index, vga, frame_delay_ms, mode);
            if (attack != 0) return attack;
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
            int moved = commit_ai_move_toward_anchor(
                game, actor_index, target_x, target_y,
                vga, frame_delay_ms, mode);
            if (moved < 0) return -1;
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
            int moved = commit_ai_move_toward_anchor(
                game, actor_index, actor->ai_param_35, actor->ai_param_36,
                vga, frame_delay_ms, mode);
            if (moved < 0) return -1;
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
                if (commit_ai_magic_internal(
                        game, actor_index, selector, &magic, 0) != 0)
                    return -1;
                cast = 1;
            }
            fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
            fd2_field_ai_physical_candidate physical;
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
            if (cast) return 1;
            int moved = commit_ai_hostile_witness(
                game, actor_index, vga, frame_delay_ms, mode);
            if (moved != 0) return moved;
            return recover_stationary_unit(game, actor_index) < 0 ? -1 : 0;
        }
        /* behavior 6 无有效分支。 */
        default:
            return -1;
    }
}

int fd2_field_game_process_automatic_action_visual(
        fd2_field_game *game, fd2_vga *vga, uint32_t frame_delay_ms,
        int clean_background) {
    field_path_playback_mode mode = clean_background
        ? FIELD_PATH_MARCH_BROWSE : FIELD_PATH_BROWSE;
    if (game && game->failure != FD2_FIELD_FAILURE_NONE) return -1;
    if (!game || !game->ready || game->active_side == 2 ||
        game->selected_unit >= 0 ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE)
        return 0;
    /* side 1/0 outer loop 原本在每个 raw actor（包括阵营不符、hidden、
     * acted、状态阻断或 AI gate 拒绝者）后无条件调用 DS:0x1b19[stage]。
     * 当前正式 session 只开放 stage 0；其 handler 只依赖整张 actor 表，
     * 因而可在分步 scheduler 入口及每次已提交动作后等价刷新。其他
     * stage 的私有 handler 未恢复，不能用通用全灭判定冒充其脚本。 */
    game->battle_result = fd2_field_game_battle_result(game);
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
                unit->detail_status[1] != 0u)
                continue;
            fd2_field_ai_magic_candidate magic;
            fd2_field_ai_item_candidate item;
            int magic_ready = fd2_field_ai_choose_magic_candidate(
                &game->map, &game->terrain, &game->units, i, 0,
                &magic) == 0 && magic.score >= 6;
            int item_ready = fd2_field_ai_choose_item_candidate(
                &game->map, &game->terrain, &game->units, i, 0,
                &item) == 0 && item.score >= 6;
            if (!magic_ready && !item_ready) continue;
            game->phase_unit_cursor = i + 1u;
            game->cursor_cell_x = unit->x;
            game->cursor_cell_y = unit->y;
            int run_common_tail = 1;
            int action_result = run_ai_behavior(
                game, i, vga, frame_delay_ms, &run_common_tail, mode);
            if (action_result < 0) {
                fail_action(game, i);
                return -1;
            }
            if (run_common_tail && finish_ai_action(game, i) != 0)
                return -1;
            /* side phase owner 在 AI 返回后才按 DS:0x1b91[event_code]
             * 分派 cell handler。SDL 尚未恢复这些 stage-private handler；
             * 动作与 acted 已提交后必须停止，不能继续下一个 actor，亦
             * 不能伪造外部 RNG 回滚。 */
            if (game->failure != FD2_FIELD_FAILURE_NONE ||
                dispatch_pending_cell_event(game, i) != 0)
                return -1;
            /* 等价于本 raw actor 的 cell handler 返回后调用 stage 0
             * DS:0x1b19 handler；必须先于下一 actor 和 phase 推进。 */
            game->battle_result = fd2_field_game_battle_result(game);
            if (game->battle_result != FD2_FIELD_BATTLE_ONGOING)
                return fd2_field_game_resume_player_controller(game) < 0 ? -1 : 1;
            return game->failure == FD2_FIELD_FAILURE_NONE ? 1 : -1;
        }
        game->phase_ai_pass = 1u;
        game->phase_unit_cursor = 0;
    }

    for (size_t i = game->phase_unit_cursor; i < game->units.count; i++) {
        game->pending_cell_event.valid = 0;
        fd2_field_unit *unit = &game->units.items[i];
        if (!ai_unit_can_execute(unit, game->active_side) ||
            fd2_field_unit_has_acted(unit) || unit->detail_status[1] != 0u)
            continue;
        game->phase_unit_cursor = i + 1u;
        game->cursor_cell_x = unit->x;
        game->cursor_cell_y = unit->y;
        int run_common_tail = 1;
        int action_result = run_ai_behavior(
            game, i, vga, frame_delay_ms, &run_common_tail, mode);
        if (action_result < 0) {
            fail_action(game, i);
            return -1;
        }
        /* 内部物理／法术／物品及移动事务都未执行tail；scheduler按原版
         * 无条件补一次，不因效果提交后的acted/死亡跳过。behavior8除外。 */
        if (run_common_tail && finish_ai_action(game, i) != 0)
            return -1;
        /* 原版 outer side loop 此时才调用 DS:0x1b91[cell_code]。未知
         * handler 不得按「无事件」继续；保留已提交动作并显式停机。 */
        if (game->failure != FD2_FIELD_FAILURE_NONE ||
            dispatch_pending_cell_event(game, i) != 0)
            return -1;
        /* 原版 stage handler 对本 raw actor 无条件执行；stage 0 的
         * 全表结果门在动作提交后于此刷新。 */
        game->battle_result = fd2_field_game_battle_result(game);
        if (game->battle_result != FD2_FIELD_BATTLE_ONGOING)
            return fd2_field_game_resume_player_controller(game) < 0 ? -1 : 1;
        return game->failure == FD2_FIELD_FAILURE_NONE ? 1 : -1;
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
    const uint8_t *profile =
        fd2_field_movement_profile_get(unit->movement_profile);
    if (!profile || unit->movement_points == 0) return -1;
    return fd2_field_move_range_compute(
        result, &game->map, &game->terrain, &game->units, mover_index,
        profile, FD2_FIELD_MOVEMENT_COST_CLASS_COUNT,
        unit->movement_points);
}

static void actor_flash_paint(fd2_field_game *game, uint8_t *snapshot,
                              size_t unit_index, uint8_t palette_index) {
    if (!game || !snapshot || unit_index >= game->units.count) return;
    const fd2_field_unit *unit = &game->units.items[unit_index];
    size_t frame_index = fd2_field_unit_frame_index(
        unit, game->units.walk_frames[unit_index], game->idle_phase);
    if (fd2_field_unit_is_hidden(unit) || !game->sprites.decoded_frames ||
        frame_index >= game->sprites.frame_count)
        return;
    int camera_x = game->camera_cell_x * 24 - FD2_FIELD_VIEW_BORDER +
                   game->camera_pixel_offset_x;
    int camera_y = game->camera_cell_y * 24 - FD2_FIELD_VIEW_BORDER +
                   game->camera_pixel_offset_y;
    int x = (int)unit->x * 24 - camera_x;
    int y = (int)unit->y * 24 - camera_y - 6;
    int pixel_step = (int)unit->frame_phase * 4;
    switch (unit->direction & 3u) {
        case 0: y += pixel_step; break;
        case 1: x -= pixel_step; break;
        case 2: y -= pixel_step; break;
        case 3: x += pixel_step; break;
    }
    const uint8_t *sprite = game->sprites.decoded_frames +
                            frame_index * 24u * 24u;
    for (int row = 0; row < 24; row++) {
        int dy = y + row;
        if (dy < 4 || dy >= 196) continue;
        for (int col = 0; col < 24; col++) {
            int dx = x + col;
            if (dx < 4 || dx >= 316 || sprite[row * 24 + col] == 0)
                continue;
            snapshot[(size_t)dy * VGA_STRIDE + (size_t)dx] = palette_index;
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
    fd2_field_game_render(game, vga);
    uint8_t *original = fd2_field_effect_snapshot(frames, 0);
    uint8_t *modified = fd2_field_effect_snapshot(frames, 1);
    memcpy(original, vga->framebuffer, VGA_W * VGA_H);
    memcpy(modified, original, VGA_W * VGA_H);
    for (size_t i = 0; i < unit_count; i++)
        actor_flash_paint(game, modified, unit_indices[i], palette_index);
    return 0;
}

int fd2_field_game_prepare_earthquake(fd2_field_game *game, fd2_vga *vga,
                                      fd2_field_effect_frames *frames) {
    if (!game || !game->ready || !vga || !frames || frames->storage)
        return -1;
    if (fd2_field_effect_frames_open(
            frames, FD2_FIELD_EFFECT_EARTHQUAKE, 3) != 0)
        return -1;
    fd2_field_game_render(game, vga);
    uint8_t baseline[VGA_W * VGA_H];
    memcpy(baseline, vga->framebuffer, sizeof(baseline));
    /* field_earthquake_effect @0x21527 从 DS:0x2096..0x20b6 复制的
     * 三组参数：x offset 72/128/0，y offset -128/128/0，
     * fixed-point step 128/131/128。 */
    static const int x_offset[3] = {72, 128, 0};
    static const int y_offset[3] = {-128, 128, 0};
    static const int step[3] = {128, 131, 128};
    for (size_t i = 0; i < 3; i++) {
        uint8_t *snapshot = fd2_field_effect_snapshot(frames, i);
        memcpy(snapshot, baseline, sizeof(baseline));
        fd2_vga target = *vga;
        memcpy(target.framebuffer, snapshot, sizeof(target.framebuffer));
        int center_x = game->camera_cell_x * 0xc00 +
                       FD2_FIELD_VIEW_CELLS_X * 0x600 + x_offset[i];
        int center_y = game->camera_cell_y * 0xc00 +
                       FD2_FIELD_VIEW_CELLS_Y * 0x600 + y_offset[i];
        fd2_terrain_render_field_scaled_animated(
            &target, &game->terrain, &game->map, center_x, center_y, step[i],
            game->terrain_visual_phase, game->idle_phase);
        int camera_x = game->camera_cell_x * 24 - FD2_FIELD_VIEW_BORDER;
        int camera_y = game->camera_cell_y * 24 - FD2_FIELD_VIEW_BORDER;
        fd2_field_units_render(&target, &game->sprites, &game->units,
                               camera_x, camera_y, game->idle_phase);
        memcpy(snapshot, target.framebuffer, sizeof(target.framebuffer));
    }
    return 0;
}

static unsigned nearest_isqrt(unsigned value) {
    unsigned root = 0;
    while ((root + 1u) <= value / (root + 1u)) root++;
    unsigned lower = value - root * root;
    unsigned upper = (root + 1u) * (root + 1u) - value;
    return upper < lower ? root + 1u : root;
}

static void apply_lut_span(uint8_t *snapshot, const uint8_t lut[256],
                           int64_t y, int64_t left, int64_t width) {
    if (!snapshot || !lut || y < 0 || y >= 192 || width <= 0) return;
    int64_t right = left + width;
    if (left < 0) left = 0;
    if (right > 312) right = 312;
    if (right <= left) return;
    uint8_t *dst = snapshot + (size_t)(y + 4) * VGA_STRIDE +
                   (size_t)(left + 4);
    for (int64_t x = left; x < right; x++, dst++) *dst = lut[*dst];
}

static void apply_lut_circle(uint8_t *snapshot, const uint8_t lut[256],
                             int center_x, int center_y, int radius,
                             int min_y, int max_y) {
    int start = min_y < 0 ? 0 : min_y;
    int end = max_y > 192 ? 192 : max_y;
    for (int y = start; y < end; y++) {
        int64_t dy = (int64_t)center_y - y;
        if (dy <= -radius || dy >= radius) continue;
        unsigned square = (unsigned)((int64_t)radius * radius - dy * dy);
        int half = (int)nearest_isqrt(square);
        apply_lut_span(snapshot, lut, y,
                       (int64_t)center_x - half, half * 2);
    }
}

static void apply_transition_lut(uint8_t *snapshot,
                                 const uint8_t lut[256],
                                 int center_x, int center_y, int radius) {
    /* field_transition_lut_mask @0x22046：整圆一次、下半圆再一次，再对上半部
     * 的 radius/sqrt(2) 中央矩形应用一次同一 LUT。 */
    apply_lut_circle(snapshot, lut, center_x, center_y, radius, 0, 192);
    apply_lut_circle(snapshot, lut, center_x, center_y, radius,
                     center_y, 192);
    unsigned half_square = (unsigned)(radius * radius) / 2u;
    int half = (int)nearest_isqrt(half_square);
    int upper_end = center_y < 192 ? center_y : 192;
    for (int y = 0; y < upper_end; y++)
        apply_lut_span(snapshot, lut, y,
                       (int64_t)center_x - half, half * 2);
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
    fd2_field_game_render(game, vga);
    uint8_t baseline[VGA_W * VGA_H];
    memcpy(baseline, vga->framebuffer, sizeof(baseline));
    for (size_t frame = 0; frame < 9; frame++) {
        const uint8_t *lut = fd2_field_lut_frame(&game->visuals, 9 - frame);
        uint8_t *snapshot = fd2_field_effect_snapshot(frames, frame);
        if (!lut || !snapshot) {
            fd2_field_effect_frames_close(frames);
            return -1;
        }
        memcpy(snapshot, baseline, sizeof(baseline));
        apply_transition_lut(snapshot, lut,
                             center_screen_x, center_screen_y,
                             radius + (int)frame * radius_step);
    }
    return 0;
}

int fd2_field_game_play_actor_group_flash(
        fd2_field_game *game, fd2_vga *vga, fd2_field_audio *audio,
        const uint8_t *unit_indices, size_t unit_count,
        uint8_t palette_index, int timed) {
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
    fd2_field_effect_frames frames = {0};
    if (fd2_field_game_prepare_earthquake(game, vga, &frames) != 0)
        return -1;
    int result = fd2_field_effect_play(&frames, vga, audio, timed);
    fd2_field_effect_frames_close(&frames);
    return result;
}

int fd2_field_game_play_stage_transition(
        fd2_field_game *game, fd2_vga *vga, fd2_field_audio *audio,
        int center_screen_x, int center_screen_y,
        int radius, int radius_step, int timed) {
    fd2_field_effect_frames frames = {0};
    if (fd2_field_game_prepare_stage_transition(
            game, vga, center_screen_x, center_screen_y,
            radius, radius_step, &frames) != 0)
        return -1;
    int result = fd2_field_effect_play(&frames, vga, audio, timed);
    fd2_field_effect_frames_close(&frames);
    if (result != 0 || !timed) return result;
    fd2_delay_ms(500);
    for (int brightness = 0; brightness < 0x40; brightness += 2) {
        fd2_vga_set_brightness(vga, brightness);
        fd2_vga_present_timed(vga, 4);
    }
    fd2_vga_wait_frame_deadline(vga);
    return 0;
}
