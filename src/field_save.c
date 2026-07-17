/* 炎龙骑士团 2 SDL3 重写 - 正式战场 session 与 FD2.SAV 活动快照
 *
 * 逆向依据：战场写档路径 code0 0x9f7a..0xa136 将当前单位表写入
 * plain+0x12a3、32 字节 cell state 写入 +0x30a3，并更新
 * +0x30c3 起的活动快照 meta；启动恢复入口 code0 0x10..0x44b 原样
 * 读回相同区域。meta[0..8]、资源字段 [10..13] 与配置字节 [14..17] 已有
 * SDL owner；其余 meta 和 unit-area 尾部保持原文件字节。
 */

#include "field_game.h"

#include <string.h>

#include "save.h"

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr_u32_le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

int fd2_field_game_export_battle_snapshot(
        const fd2_field_game *game, fd2_save_battle_snapshot *snapshot) {
    if (!game || !snapshot || !game->ready || game->stage > 0xffu ||
        game->units.count > FD2_SAVE_BATTLE_MAX_UNITS ||
        game->turn_number > 0xffu || game->camera_cell_x < 0 ||
        game->camera_cell_x > 0xff || game->camera_cell_y < 0 ||
        game->camera_cell_y > 0xff || game->cursor_cell_x < 0 ||
        game->cursor_cell_x > 0xff || game->cursor_cell_y < 0 ||
        game->cursor_cell_y > 0xff)
        return -1;

    /* snapshot 必须来自 fd2_save_file_get_battle_snapshot；只覆盖原版
     * writer 明确写回且已有 owner 的单位前缀、cell state、meta[0..17]。
     * 其中 meta[9] 是 count 副本，meta[10..13] 是资源值。 */
    memcpy(snapshot->unit_area, game->units.items,
           game->units.count * FD2_FIELD_UNIT_RECORD_SIZE);
    memcpy(snapshot->cell_state, game->cell_action_completed,
           sizeof(game->cell_action_completed));
    snapshot->meta[FD2_SAVE_BATTLE_META_TURN] =
        (uint8_t)game->turn_number;
    snapshot->meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] =
        (uint8_t)game->units.count;
    snapshot->meta[FD2_SAVE_BATTLE_META_STAGE] = (uint8_t)game->stage;
    snapshot->meta[FD2_SAVE_BATTLE_META_CAMERA_X] =
        (uint8_t)game->camera_cell_x;
    snapshot->meta[FD2_SAVE_BATTLE_META_CAMERA_Y] =
        (uint8_t)game->camera_cell_y;
    snapshot->meta[FD2_SAVE_BATTLE_META_FOCUS_X] =
        (uint8_t)game->cursor_cell_x;
    snapshot->meta[FD2_SAVE_BATTLE_META_FOCUS_Y] =
        (uint8_t)game->cursor_cell_y;
    snapshot->meta[FD2_SAVE_BATTLE_META_FOCUS_REL_X] =
        (uint8_t)(game->cursor_cell_x - game->camera_cell_x);
    snapshot->meta[FD2_SAVE_BATTLE_META_FOCUS_REL_Y] =
        (uint8_t)(game->cursor_cell_y - game->camera_cell_y);
    /* 原版 writer @code0 0xa0aa..0xa0b9：meta[9] 是单位表 count 的
     * 冗余副本，meta[10..13] 是 DS:0x3bf3 资源值。 */
    snapshot->meta[FD2_SAVE_BATTLE_META_UNIT_COUNT_COPY] =
        (uint8_t)game->units.count;
    wr_u32_le(snapshot->meta + FD2_SAVE_BATTLE_META_RESOURCE_VALUE,
              game->resource_value);
    /* 原版 writer @code0 0xa0bc..0xa0dd。 */
    snapshot->meta[FD2_SAVE_BATTLE_META_OPTION_EFFECT] =
        game->option_values[2]; /* DS:0x3af9 */
    snapshot->meta[FD2_SAVE_BATTLE_META_OPTION_CAMERA] =
        game->option_values[3]; /* DS:0x1aab */
    snapshot->meta[FD2_SAVE_BATTLE_META_OPTION_MUSIC] =
        game->option_values[0]; /* DS:0x1e61 */
    snapshot->meta[FD2_SAVE_BATTLE_META_OPTION_SFX] =
        game->option_values[1]; /* DS:0x1e62 */
    return 0;
}

static int max_camera_cell(int map_cells, int view_cells) {
    int max = map_cells - view_cells;
    return max > 0 ? max : 0;
}

static int validate_import(const fd2_field_game *game,
                           const fd2_save_battle_snapshot *snapshot) {
    if (!game || !snapshot || !game->ready) return -1;
    size_t count = snapshot->meta[FD2_SAVE_BATTLE_META_UNIT_COUNT];
    size_t stage = snapshot->meta[FD2_SAVE_BATTLE_META_STAGE];
    if (stage != game->stage || stage != 0u || count == 0u ||
        snapshot->meta[FD2_SAVE_BATTLE_META_UNIT_COUNT_COPY] != count ||
        count > FD2_SAVE_BATTLE_MAX_UNITS ||
        count > FD2_FIELD_MAX_UNITS || game->map.width <= 0 ||
        game->map.height <= 0)
        return -1;
    for (size_t i = 0; i < count; i++) {
        fd2_field_unit unit;
        memcpy(&unit,
               snapshot->unit_area + i * FD2_FIELD_UNIT_RECORD_SIZE,
               sizeof(unit));
        if (unit.x >= game->map.width || unit.y >= game->map.height)
            return -1;
    }
    if (snapshot->meta[FD2_SAVE_BATTLE_META_CAMERA_X] >
            max_camera_cell(game->map.width, FD2_FIELD_VIEW_CELLS_X) ||
        snapshot->meta[FD2_SAVE_BATTLE_META_CAMERA_Y] >
            max_camera_cell(game->map.height, FD2_FIELD_VIEW_CELLS_Y) ||
        snapshot->meta[FD2_SAVE_BATTLE_META_FOCUS_X] >
            game->map.width - 1 ||
        snapshot->meta[FD2_SAVE_BATTLE_META_FOCUS_Y] >
            game->map.height - 1 ||
        snapshot->meta[FD2_SAVE_BATTLE_META_FOCUS_X] <
            snapshot->meta[FD2_SAVE_BATTLE_META_CAMERA_X] ||
        snapshot->meta[FD2_SAVE_BATTLE_META_FOCUS_Y] <
            snapshot->meta[FD2_SAVE_BATTLE_META_CAMERA_Y] ||
        snapshot->meta[FD2_SAVE_BATTLE_META_FOCUS_REL_X] !=
            snapshot->meta[FD2_SAVE_BATTLE_META_FOCUS_X] -
            snapshot->meta[FD2_SAVE_BATTLE_META_CAMERA_X] ||
        snapshot->meta[FD2_SAVE_BATTLE_META_FOCUS_REL_Y] !=
            snapshot->meta[FD2_SAVE_BATTLE_META_FOCUS_Y] -
            snapshot->meta[FD2_SAVE_BATTLE_META_CAMERA_Y])
        return -1;
    return 0;
}

int fd2_field_game_import_battle_snapshot(
        fd2_field_game *game, const fd2_save_battle_snapshot *snapshot) {
    if (validate_import(game, snapshot) != 0) return -1;

    /* 先构造完整旁路副本；验证全部完成后才一次提交到 live session。 */
    fd2_field_units loaded = {0};
    loaded.count = snapshot->meta[FD2_SAVE_BATTLE_META_UNIT_COUNT];
    memcpy(loaded.items, snapshot->unit_area,
           loaded.count * FD2_FIELD_UNIT_RECORD_SIZE);
    for (size_t i = 0; i < loaded.count; i++) {
        loaded.walk_frames[i] = 1;
        loaded.source_template_indices[i] = FD2_FIELD_UNIT_NO_TEMPLATE;
    }

    fd2_field_path_close(&game->move_range);
    game->move_path_length = 0;
    game->move_preview_valid = 0;
    game->camera_pixel_offset_x = 0;
    game->camera_pixel_offset_y = 0;
    game->units = loaded;
    game->stage_runtime_entry_offset = 0x238du +
        (uint32_t)snapshot->meta[FD2_SAVE_BATTLE_META_STAGE] * 0x1fu;
    /* 活动快照保存原版 0x20 字节 DAT_00003ad5。SDL 当前仅确认并拥有
     * 前 16 个 FDFIELD cell-action slot，后 16 字节继续由容器保留。 */
    memcpy(game->cell_action_completed, snapshot->cell_state,
           sizeof(game->cell_action_completed));
    game->turn_number = snapshot->meta[FD2_SAVE_BATTLE_META_TURN];
    /* 原版 loader 随后直接进入 field controller；其交互边界是玩家
     * side 2。meta 中没有独立 active-side 字段。 */
    game->active_side = 2;
    game->phase_unit_cursor = 0;
    game->phase_ai_pass = 0;
    game->camera_cell_x = snapshot->meta[FD2_SAVE_BATTLE_META_CAMERA_X];
    game->camera_cell_y = snapshot->meta[FD2_SAVE_BATTLE_META_CAMERA_Y];
    game->cursor_cell_x = snapshot->meta[FD2_SAVE_BATTLE_META_FOCUS_X];
    game->cursor_cell_y = snapshot->meta[FD2_SAVE_BATTLE_META_FOCUS_Y];
    game->resource_value = rd_u32_le(
        snapshot->meta + FD2_SAVE_BATTLE_META_RESOURCE_VALUE);
    game->option_values[0] =
        snapshot->meta[FD2_SAVE_BATTLE_META_OPTION_MUSIC] ? 1u : 0u;
    game->option_values[1] =
        snapshot->meta[FD2_SAVE_BATTLE_META_OPTION_SFX] ? 1u : 0u;
    game->option_values[2] =
        snapshot->meta[FD2_SAVE_BATTLE_META_OPTION_EFFECT] ? 1u : 0u;
    game->option_values[3] =
        snapshot->meta[FD2_SAVE_BATTLE_META_OPTION_CAMERA] ? 1u : 0u;

    game->selected_unit = -1;
    game->detail_unit = -1;
    game->detail_visible = 0;
    game->detail_acknowledged_unit = -1;
    game->command_selected = -1;
    memset(game->command_disabled, 0, sizeof(game->command_disabled));
    game->command_highlight_phase = 0;
    game->command_animation_phase = 4;
    game->command_animation_opening = 1;
    game->last_command_highlight_ms = 0;
    fd2_field_system_menu_init(&game->system_menu);
    fd2_field_system_menu_set_options(&game->system_menu,
                                      game->option_values);
    game->last_system_action = FD2_FIELD_SYSTEM_ACTION_NONE;
    game->manual_slot_picker.open = 0;
    game->system_result_fragment = 0;
    game->system_result_until_ms = 0;
    game->manual_slot_result_fragment = 0;
    game->manual_slot_result_until_ms = 0;
    game->attack_target = -1;
    game->last_attack_valid = 0;
    game->last_counterattack_valid = 0;
    game->last_attack_strikes = 0;
    game->last_counterattack_strikes = 0;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->event_log_count = 0;
    game->event_total_count = 0;
    game->unhandled_event_count = 0;
    game->dropped_event_count = 0;
    game->phase_next_action_ms = 0;
    game->last_idle_ms = 0;
    game->last_terrain_visual_ms = 0;
    game->last_range_visual_ms = 0;
    game->battle_result = FD2_FIELD_BATTLE_ONGOING;
    if (game->units.count != 0 &&
        (fd2_field_unit_is_hidden(&game->units.items[0]) ||
         game->units.items[0].hp == 0)) {
        game->battle_result = FD2_FIELD_BATTLE_DEFEAT;
    } else {
        int have_hostile = 0;
        for (size_t i = 0; i < game->units.count; i++) {
            const fd2_field_unit *unit = &game->units.items[i];
            if (unit->side == 0u && !fd2_field_unit_is_hidden(unit) &&
                unit->hp != 0) {
                have_hostile = 1;
                break;
            }
        }
        if (!have_hostile)
            game->battle_result = FD2_FIELD_BATTLE_VICTORY;
    }
    return 0;
}

static int snapshot_is_importable(const fd2_field_game *game,
                                  const fd2_save_battle_snapshot *snapshot) {
    return validate_import(game, snapshot) == 0;
}

int fd2_field_game_save_resource_available(const fd2_field_game *game,
                                           const char *path) {
    if (!game || !path || !game->ready) return 0;
    fd2_save_file save = {0};
    fd2_save_battle_snapshot snapshot;
    int available = fd2_save_file_open(&save, path) == 0 &&
                    fd2_save_file_get_battle_snapshot(&save, &snapshot) == 0 &&
                    snapshot_is_importable(game, &snapshot);
    fd2_save_file_close(&save);
    return available;
}

int fd2_field_game_save_active(fd2_field_game *game, const char *path) {
    if (!game || !path || !game->ready) return -1;
    fd2_save_file save = {0};
    fd2_save_battle_snapshot snapshot;
    if (fd2_save_file_open(&save, path) != 0) {
        if (fd2_save_file_create_empty(&save) != 0) return -1;
    }
    if (fd2_save_file_get_battle_snapshot(&save, &snapshot) != 0 ||
        fd2_field_game_export_battle_snapshot(game, &snapshot) != 0 ||
        fd2_save_file_update_battle_snapshot(&save, &snapshot) != 0 ||
        fd2_save_file_write(&save, path) != 0) {
        fd2_save_file_close(&save);
        return -1;
    }
    fd2_save_file_close(&save);
    game->save_resource_available = 1;
    return 0;
}

int fd2_field_game_load_active(fd2_field_game *game, const char *path) {
    if (!game || !path || !game->ready) return -1;
    fd2_save_file save = {0};
    fd2_save_battle_snapshot snapshot;
    if (fd2_save_file_open(&save, path) != 0 ||
        fd2_save_file_get_battle_snapshot(&save, &snapshot) != 0) {
        fd2_save_file_close(&save);
        return -1;
    }
    fd2_save_file_close(&save);
    /* import 在任何 live 写入前完成全部语义验证。 */
    return fd2_field_game_import_battle_snapshot(game, &snapshot);
}

int fd2_field_game_save_manual_slot(fd2_field_game *game, const char *path,
                                    size_t slot_index) {
    if (!game || !path || !game->ready || slot_index >= FD2_SAVE_SLOT_COUNT ||
        game->stage > 0xfeu || game->units.count > FD2_SAVE_MAX_UNITS)
        return -1;
    fd2_save_file save = {0};
    if (fd2_save_file_open(&save, path) != 0 &&
        fd2_save_file_create_empty(&save) != 0)
        return -1;
    fd2_save_manual_slot slot;
    if (fd2_save_file_get_manual_slot_raw(&save, slot_index, &slot) != 0) {
        fd2_save_file_close(&save);
        return -1;
    }
    fd2_save_manual_state state = {0};
    state.stage_id = (uint8_t)game->stage;
    state.unit_count = (uint8_t)game->units.count;
    state.gold_or_flags = game->resource_value;
    /* manual slot meta +6..+9 的原版顺序是 camera/effect/music/SFX。 */
    state.options[0] = game->option_values[3];
    state.options[1] = game->option_values[2];
    state.options[2] = game->option_values[0];
    state.options[3] = game->option_values[1];
    memcpy(state.unit_area, game->units.items,
           game->units.count * FD2_FIELD_UNIT_RECORD_SIZE);
    /* 原版 hand_save 无条件复制完整 0xa00 runtime table；SDL items 数组
     * 也是 64×0x50，手工槽只取其前 32 条。 */
    if (game->units.count < FD2_SAVE_MAX_UNITS)
        memcpy(state.unit_area +
                   game->units.count * FD2_FIELD_UNIT_RECORD_SIZE,
               game->units.items + game->units.count,
               (FD2_SAVE_MAX_UNITS - game->units.count) *
                   FD2_FIELD_UNIT_RECORD_SIZE);
    int result = fd2_save_manual_slot_encode(&slot, &state) == 0 &&
                 fd2_save_file_update_manual_slot(
                     &save, slot_index, &slot) == 0 &&
                 fd2_save_file_write(&save, path) == 0 ? 0 : -1;
    fd2_save_file_close(&save);
    return result;
}

int fd2_field_game_load_manual_slot(fd2_field_game *game, const char *path,
                                    size_t slot_index) {
    if (!game || !path || !game->ready || slot_index >= FD2_SAVE_SLOT_COUNT)
        return -1;
    fd2_save_file save = {0};
    fd2_save_manual_slot slot;
    fd2_save_manual_state state;
    if (fd2_save_file_open(&save, path) != 0 ||
        fd2_save_file_get_manual_slot(&save, slot_index, &slot) != 0 ||
        fd2_save_manual_slot_decode(&slot, &state) != 0) {
        fd2_save_file_close(&save);
        return -1;
    }
    fd2_save_file_close(&save);
    if (state.stage_id != game->stage || state.stage_id != 0u ||
        state.unit_count == 0u || state.unit_count > FD2_FIELD_MAX_UNITS)
        return -1;
    for (size_t i = 0; i < state.unit_count; i++) {
        fd2_field_unit unit;
        memcpy(&unit, state.unit_area + i * FD2_FIELD_UNIT_RECORD_SIZE,
               sizeof(unit));
        if (unit.x >= game->map.width || unit.y >= game->map.height)
            return -1;
    }

    fd2_field_units loaded = {0};
    loaded.count = state.unit_count;
    /* code0 0x19974..0x19985 无条件恢复完整 0xa00 runtime table；
     * 即使 unit_count 较小，非活动尾部也不能在 SDL owner 中被清零。 */
    memcpy(loaded.items, state.unit_area, FD2_SAVE_UNIT_TABLE_SIZE);
    for (size_t i = 0; i < loaded.count; i++) {
        loaded.walk_frames[i] = 1;
        loaded.source_template_indices[i] = FD2_FIELD_UNIT_NO_TEMPLATE;
    }
    fd2_field_path_close(&game->move_range);
    game->move_path_length = 0;
    game->move_preview_valid = 0;
    game->units = loaded;
    game->stage_runtime_entry_offset = 0x238du +
        (uint32_t)state.stage_id * 0x1fu;
    game->resource_value = state.gold_or_flags;
    game->option_values[3] = state.options[0] ? 1u : 0u;
    game->option_values[2] = state.options[1] ? 1u : 0u;
    game->option_values[0] = state.options[2] ? 1u : 0u;
    game->option_values[1] = state.options[3] ? 1u : 0u;
    game->selected_unit = -1;
    game->detail_unit = -1;
    game->detail_visible = 0;
    game->detail_acknowledged_unit = -1;
    game->command_selected = -1;
    game->attack_target = -1;
    game->camera_pixel_offset_x = 0;
    game->camera_pixel_offset_y = 0;
    game->active_side = 2;
    game->phase_unit_cursor = 0;
    game->phase_ai_pass = 0;
    game->phase_next_action_ms = 0;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->manual_slot_picker.open = 0;
    game->system_result_fragment = 0;
    game->system_result_until_ms = 0;
    game->manual_slot_result_fragment = 0;
    game->manual_slot_result_until_ms = 0;
    fd2_field_system_menu_init(&game->system_menu);
    fd2_field_system_menu_set_options(&game->system_menu,
                                      game->option_values);
    game->event_log_count = 0;
    game->event_total_count = 0;
    game->unhandled_event_count = 0;
    game->dropped_event_count = 0;
    game->battle_result = FD2_FIELD_BATTLE_ONGOING;
    return 0;
}

int fd2_field_game_execute_storage_action(
        fd2_field_game *game, fd2_field_system_action action,
        const char *path, size_t *result_fragment) {
    if (result_fragment) *result_fragment = 0;
    if (!game || !path || !game->ready ||
        (action != FD2_FIELD_SYSTEM_ACTION_SAVE &&
         action != FD2_FIELD_SYSTEM_ACTION_LOAD))
        return -1;

    size_t fragment = 0;
    if (action == FD2_FIELD_SYSTEM_ACTION_SAVE) {
        int success = fd2_field_game_save_active(game, path) == 0;
        (void)fd2_field_system_menu_get_last_result_fragment(
            &game->system_menu, success, &fragment);
        fd2_field_game_set_save_resource_available(
            game, fd2_field_game_save_resource_available(game, path));
        if (result_fragment) *result_fragment = fragment;
        return success;
    }

    /* 成功导入会重置 system_menu，因此必须先保存 0x19e 结果片段。 */
    (void)fd2_field_system_menu_get_last_result_fragment(
        &game->system_menu, 1, &fragment);
    int success = fd2_field_game_load_active(game, path) == 0;
    fd2_field_game_set_save_resource_available(game, success);
    if (result_fragment) *result_fragment = success ? fragment : 0;
    return success;
}
