/* 炎龙骑士团 2 SDL3 重写 - 正式战场 session
 *
 * 逆向依据：FDFIELD stage metadata/placement、DAT_00003a45 的 0x50
 * 字节单位表、field_view_render_tiles @0x3710c、
 * map_scene_render_actors @0x41db2、map_actor_blit_24x24 @0x42c34、
 * map_tile_blit_visible @0x37cda 与
 * field_unit_detail_transition_frame @0x3d61d。
 */

#include "field_game.h"

#include <stdlib.h>
#include <string.h>

#include "field_move.h"
#include "field_move_profile.h"
#include "field_item.h"
#include "portrait.h"
#include "field_unit_base.h"
#include "field_unit_stats.h"

#define FD2_FIELD_BIOS_TICK_MS 55
#define FD2_FIELD_IDLE_MS 275
#define FD2_FIELD_RANGE_VISUAL_MS 165 /* 3 个约 55 ms 的 BIOS tick */
#define FD2_FIELD_AUTOMATIC_ACTION_MS 150

static int stage0_known_item_lookup(void *userdata, uint8_t item_id,
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

static int apply_stage0_player_equipment(fd2_field_unit *unit) {
    if (!unit) return -1;
    fd2_field_unit updated = *unit;
    uint8_t *record = (uint8_t *)&updated;
    /* DOSBox 第一关运行时 0x50 字节记录确认。未使用槽统一规范为
     * flag 0x80 / item 0xff；原版被忽略的 item byte 含未初始化残值。 */
    memset(record + 0x0a, 0x80, 0x10);
    for (size_t slot = 0; slot < 8; slot++)
        record[0x0b + slot * 2u] = 0xff;
    switch (unit->unit_id) {
        case 0:
            record[0x0a] = 0x40; record[0x0b] = 0;   /* 短剑 */
            record[0x0c] = 0x40; record[0x0d] = 132; /* 皮甲 */
            record[0x0e] = 0x00; record[0x0f] = 192; /* 药草 */
            break;
        case 9:
            record[0x0a] = 0x40; record[0x0b] = 52;  /* 长棍 */
            record[0x0c] = 0x40; record[0x0d] = 164; /* 长袍 */
            break;
        case 4:
            record[0x0a] = 0x40; record[0x0b] = 20;  /* 刺矛 */
            record[0x0c] = 0x40; record[0x0d] = 128; /* 布衣 */
            break;
        case 30:
            record[0x0a] = 0x40; record[0x0b] = 72;  /* 威力手臂 */
            record[0x0c] = 0x40; record[0x0d] = 178; /* 战斗装甲 */
            break;
        default:
            return -1;
    }
    if (fd2_field_unit_combat_stats_recompute(
            &updated, stage0_known_item_lookup, NULL, NULL, NULL) != 0)
        return -1;
    *unit = updated;
    return 0;
}

static int load_stage0_units(fd2_field_units *units,
                             const fd2_field_metadata *meta,
                             const fd2_field_placements *placements) {
    /* new_game_opening_play @0x5752f 在进入 stage 0 前把四名玩家
     * 0/9/4/30 放在 placement 末四个玩家槽；随后
     * field_actor_group_arrival_effect @0x57bad 依次加入 group 1/2；
     * 组内 actor 由 field_unit_stage_template_append @0x35e6e 的模板重排
     * 与基础表等级公式构造。 */
    static const uint8_t player_ids[] = {0, 9, 4, 30};
    if (!units || !meta || !placements || placements->count < 4)
        return -1;

    fd2_field_units_clear(units);
    size_t player_base = placements->count - 4;
    for (size_t i = 0; i < sizeof(player_ids); i++) {
        const fd2_field_placement *placement =
            &placements->records[player_base + i];
        if (fd2_field_units_set(units, units->count,
                                player_ids[i], player_ids[i], 2,
                                placement->x, placement->y) != 0 ||
            fd2_field_unit_base_apply(
                &units->items[units->count - 1], 1) != 0 ||
            apply_stage0_player_equipment(
                &units->items[units->count - 1]) != 0) {
            fd2_field_units_clear(units);
            return -1;
        }
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

static int unit_can_act_for_side(const fd2_field_unit *unit, uint8_t side) {
    return unit && unit->side == side && !fd2_field_unit_is_hidden(unit) &&
           unit->hp != 0;
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
    int saved_camera_x = game->camera_cell_x;
    int saved_camera_y = game->camera_cell_y;
    int result = -1;

    /* field_stage0_31_turn_action0..3 @0x59745/0x5981f/0x59887/
     * 0x598e1。session 先事务化提交 group、镜头和移动脚本；scene 演出层
     * 再从 notice 的提交前快照重放镜头、移动、LMI1 与对话。 */
    switch (action) {
        case 0:
            if (append_stage0_event_group(game, 3) != 0) break;
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
        game->camera_cell_x = saved_camera_x;
        game->camera_cell_y = saved_camera_y;
    }
    return result;
}

static void append_event_notice(fd2_field_game *game,
                                const fd2_field_event_notice *notice) {
    if (!game || !notice) return;
    if (game->event_log_count < FD2_FIELD_EVENT_LOG_CAPACITY) {
        game->event_log[game->event_log_count++] = *notice;
    } else if (game->dropped_event_count != UINT32_MAX) {
        game->dropped_event_count++;
    }
    if (game->event_total_count != UINT32_MAX) game->event_total_count++;
    if (!notice->handled && game->unhandled_event_count != UINT32_MAX)
        game->unhandled_event_count++;
}

static void record_turn_events(fd2_field_game *game) {
    if (!game) return;
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
        int handled = execute_known_turn_event(game, matches[i].action) == 0;
        fd2_field_event_notice notice = {
            .kind = FD2_FIELD_EVENT_TURN,
            .turn_number = game->turn_number,
            .phase = game->active_side,
            .action = matches[i].action,
            .unit_index = UINT8_MAX,
            .handled = (uint8_t)handled,
            .presentation_deferred = (uint8_t)handled,
            .presentation_unit_count = (uint8_t)presentation_unit_count,
            .presentation_camera_x = (int16_t)presentation_camera_x,
            .presentation_camera_y = (int16_t)presentation_camera_y,
            .presentation_focus_x = (int16_t)presentation_focus_x,
            .presentation_focus_y = (int16_t)presentation_focus_y,
            .slot = matches[i].slot,
        };
        /* field_turn_event_check @0x3fa27 的 DS:0x1b91 已确认为 cdecl
         * handler(actor_index) 表；回合事件传 0。未知 action 或事务失败
         * 仍保留 unhandled 记录，不能执行部分脚本。 */
        append_event_notice(game, &notice);
    }
}

static void record_cell_event(fd2_field_game *game,
                              size_t unit_index,
                              uint8_t match_arg) {
    if (!game || unit_index >= game->units.count) return;
    const fd2_field_unit *unit = &game->units.items[unit_index];
    uint8_t event_code;
    size_t slot;
    int found = fd2_field_cell_event_find(
        &game->map, &game->terrain, &game->metadata,
        unit->x, unit->y, match_arg, &event_code, &slot);
    if (found != 1) return;
    fd2_field_event_notice notice = {
        .kind = FD2_FIELD_EVENT_CELL,
        .turn_number = game->turn_number,
        .phase = game->active_side,
        .action = event_code,
        .unit_index = (uint8_t)unit_index,
        .x = unit->x,
        .y = unit->y,
        .handled = 0,
        .slot = slot,
    };
    append_event_notice(game, &notice);
}

static void enter_phase(fd2_field_game *game, uint8_t side) {
    if (!game) return;
    game->active_side = side;
    game->phase_unit_cursor = 0;
    game->phase_next_action_ms = 0;
    for (size_t i = 0; i < game->units.count; i++) {
        if (game->units.items[i].side == side)
            fd2_field_unit_set_acted(&game->units.items[i], 0);
    }
    record_turn_events(game);
}

static void advance_phase(fd2_field_game *game) {
    if (!game) return;
    /* field_turn_cycle_run @0x3f51f 的阶段顺序：玩家 side 2 完成后处理
     * side 1，再处理敌方 side 0；随后回合数加一并重新进入 side 2。 */
    if (game->active_side == 2) {
        enter_phase(game, 1);
    } else if (game->active_side == 1) {
        enter_phase(game, 0);
    } else {
        if (game->turn_number != UINT32_MAX) game->turn_number++;
        enter_phase(game, 2);
    }
}

static void clear_view_border(fd2_vga *vga) {
    if (!vga) return;
    /* field_view_render_tiles @0x3710c 只写 VGA (4,4) 起的
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
        fd2_field_info_assets_open(&game->info_assets, fdother) != 0)
        goto fail;

    if (load_stage0_units(&game->units, &game->metadata,
                          &game->placements) != 0 ||
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
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->ready = 1;
    enter_phase(game, 2);
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
    game->selected_unit = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    return 0;
}

void fd2_field_game_close(fd2_field_game *game) {
    if (!game) return;
    fd2_field_path_close(&game->move_range);
    free(game->move_path);
    fd2_image_free(&game->detail_portrait);
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
    game->detail_unit = -1;
    game->detail_acknowledged_unit = -1;
}

void fd2_field_game_tick(fd2_field_game *game, uint64_t now_ms) {
    if (!game || !game->ready) return;
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

    /* 敌方 AI 尚未接入。M5 骨架按 actor 顺序执行确定性待机，使
     * side 1/0 阶段、事件检查和回合切换可独立验证。 */
    if (game->active_side != 2 &&
        game->interaction == FD2_FIELD_INTERACTION_BROWSE) {
        if (game->phase_next_action_ms == 0) {
            game->phase_next_action_ms =
                now_ms + FD2_FIELD_AUTOMATIC_ACTION_MS;
        } else if (now_ms >= game->phase_next_action_ms) {
            (void)fd2_field_game_process_automatic_action(game);
            if (game->active_side != 2)
                game->phase_next_action_ms =
                    now_ms + FD2_FIELD_AUTOMATIC_ACTION_MS;
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

void fd2_field_game_render(fd2_field_game *game, fd2_vga *vga) {
    if (!game || !game->ready || !vga) return;

    /* field_actor_move_{down,left,up,right}_follow_camera
     * @0x380be/0x38221/0x38399/0x38529 在六个步态相位中每次同步
     * 卷动镜头 4 px；camera_pixel_offset_* 保存尚未提交到格坐标的部分。 */
    int camera_x = game->camera_cell_x * 24 - FD2_FIELD_VIEW_BORDER +
                   game->camera_pixel_offset_x;
    int camera_y = game->camera_cell_y * 24 - FD2_FIELD_VIEW_BORDER +
                   game->camera_pixel_offset_y;
    fd2_terrain_render_field_base_animated(
        vga, &game->terrain, &game->map, camera_x, camera_y,
        game->terrain_visual_phase, game->idle_phase);
    render_move_selection(game, vga, camera_x, camera_y);
    render_field_cursor(game, vga, camera_x, camera_y);
    fd2_field_units_render(vga, &game->sprites, &game->units,
                           camera_x, camera_y, game->idle_phase);
    fd2_terrain_render_field_overlay(
        vga, &game->terrain, &game->map, camera_x, camera_y,
        game->terrain_visual_phase);
    render_move_selection_overlay(game, vga, camera_x, camera_y);
    clear_view_border(vga);
    render_field_info(game, vga);
    if (game->detail_visible && game->detail_unit >= 0 &&
        (size_t)game->detail_unit < game->units.count) {
        fd2_field_detail_draw(vga, &game->info_assets, &game->font,
                              &game->ui_text,
                              &game->units.items[game->detail_unit],
                              &game->detail_portrait);
    }
}

static void render_field_info(fd2_field_game *game, fd2_vga *vga) {
    if (!game || !vga) return;
    int rel_x = game->cursor_cell_x - game->camera_cell_x;
    int rel_y = game->cursor_cell_y - game->camera_cell_y;
    /* field_cell_info_panel_draw @0x3ff11：光标进入视窗左下角时把面板
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
    /* field_cell_info_panel_draw @0x3ff11 按 map_cell_info_at 返回的
     * 原始 terrain ID 取基础帧，不套用地图层动画相位。 */
    if (fd2_terrain_base_frame_from_cell(
            &game->terrain, cell, &frame_index) == 0)
        terrain_image = fd2_terrain_frame(&game->terrain, frame_index);

    /* stage 0 实机逐格截图确认 movement cost class 0/1/2 的面板值。
     * 未登记类别不推广样本值。 */
    static const int attack_modifier[] = {5, -5, 5};
    static const int defense_modifier[] = {0, 10, 0};
    int attack = 0;
    int defense = 0;
    if (attr && attr->movement_cost_class < 3) {
        attack = attack_modifier[attr->movement_cost_class];
        defense = defense_modifier[attr->movement_cost_class];
    }

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

static void render_move_selection(const fd2_field_game *game,
                                  fd2_vga *vga,
                                  int camera_x, int camera_y) {
    if (!game || !vga ||
        game->interaction != FD2_FIELD_INTERACTION_UNIT_SELECTED ||
        !game->move_range.nodes)
        return;

    /* field_view_render_tiles @0x3710c：node offset 0x07 不为 0xff
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
        game->interaction != FD2_FIELD_INTERACTION_UNIT_SELECTED ||
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

    /* field_selection_overlay_draw @0x374f0 在普通模式 1 调用
     * field_selection_sprite_blit @0x3790b，把 FDOTHER[1] frame 0
     * 以透明 SKIP 规则画在焦点格；顺序位于地形/范围之后、单位之前。 */
    const fd2_image *cursor = fd2_field_cursor_frame(&game->visuals, 0);
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

    /* field_focus_move_to @0x37efe：焦点向视窗外侧移动前若已达到
     * 左/右 2/11、上/下 2/6 阈值，则光标与镜头同步移动一格。 */
    int rel_x = old_x - game->camera_cell_x;
    int rel_y = old_y - game->camera_cell_y;
    if (new_x < old_x && rel_x <= 2)
        fd2_field_game_move_camera(game, -1, 0);
    else if (new_x > old_x && rel_x >= 11)
        fd2_field_game_move_camera(game, 1, 0);
    if (new_y < old_y && rel_y <= 2)
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

int fd2_field_game_confirm_cursor(fd2_field_game *game) {
    if (!game || !game->ready) return -1;

    if (game->interaction == FD2_FIELD_INTERACTION_BROWSE) {
        if (game->active_side != 2) return -1;
        int unit_index = fd2_field_game_unit_at(game,
                                                game->cursor_cell_x,
                                                game->cursor_cell_y);
        if (unit_index < 0 ||
            !unit_can_act_for_side(&game->units.items[unit_index],
                                   game->active_side) ||
            fd2_field_unit_has_acted(&game->units.items[unit_index]))
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
        if (fd2_field_game_finish_unit_action(game) != 0) return -1;
        return unit_index;
    }
    return -1;
}

int fd2_field_game_open_detail(fd2_field_game *game, size_t unit_index) {
    if (!game || !game->ready || unit_index >= game->units.count ||
        fd2_field_unit_is_hidden(&game->units.items[unit_index]) ||
        game->units.items[unit_index].hp == 0)
        return -1;
    fd2_image portrait = {0};
    if (fd2_portrait_decode_frame(&portrait, &game->dato_archive,
                                  game->units.items[unit_index].unit_id,
                                  0) != 0)
        return -1;
    fd2_image_free(&game->detail_portrait);
    game->detail_portrait = portrait;
    game->detail_unit = (int)unit_index;
    game->detail_visible = 1;
    game->detail_acknowledged_unit = -1;
    return 0;
}

void fd2_field_game_close_detail(fd2_field_game *game) {
    if (!game) return;
    int unit_index = game->detail_unit;
    fd2_image_free(&game->detail_portrait);
    game->detail_unit = -1;
    game->detail_visible = 0;
    game->detail_acknowledged_unit = unit_index;
}

int fd2_field_game_animate_detail(fd2_field_game *game, fd2_vga *vga,
                                  int opening,
                                  fd2_field_detail_phase_fn phase_callback,
                                  void *phase_userdata) {
    if (!game || !vga || !game->detail_visible) return -1;
    uint8_t *background = malloc(VGA_W * VGA_H);
    uint8_t *detail = malloc(VGA_W * VGA_H);
    if (!background || !detail) {
        free(background);
        free(detail);
        return -1;
    }

    game->detail_visible = 0;
    fd2_field_game_render(game, vga);
    memcpy(background, vga->framebuffer, VGA_W * VGA_H);
    game->detail_visible = 1;
    fd2_field_game_render(game, vga);
    memcpy(detail, vga->framebuffer, VGA_W * VGA_H);

    if (opening) {
        for (int phase = 11; phase >= 0; phase--) {
            if (phase_callback)
                phase_callback(phase_userdata, 1, phase);
            fd2_field_detail_transition_frame(vga->framebuffer, detail,
                                    background, phase);
            fd2_vga_present_timed(vga, 16);
        }
    } else {
        for (int phase = 0; phase < 12; phase++) {
            if (phase_callback)
                phase_callback(phase_userdata, 0, phase);
            fd2_field_detail_transition_frame(vga->framebuffer, detail,
                                    background, phase);
            fd2_vga_present_timed(vga, 16);
        }
        memcpy(vga->framebuffer, background, VGA_W * VGA_H);
    }
    free(background);
    free(detail);
    return 0;
}

int fd2_field_game_cancel_selection(fd2_field_game *game) {
    if (!game || !game->ready || game->selected_unit < 0) return 0;

    fd2_field_unit *unit = &game->units.items[game->selected_unit];
    if (game->interaction == FD2_FIELD_INTERACTION_COMMAND) {
        /* 正式指令菜单尚未接入；此状态中的取消复现原版移动后回退：
         * 恢复确认移动前的唯一坐标记录，再重新显示原范围。 */
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
    clear_move_query(game);
    game->selected_unit = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    return 1;
}

int fd2_field_game_execute_move(fd2_field_game *game,
                                fd2_vga *vga,
                                uint32_t frame_delay_ms) {
    /* field_actor_path_play @0x3869c 按 0下/1左/2上/3右分派四个
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
                fd2_field_game_render(game, vga);
                fd2_vga_present_timed(vga, frame_delay_ms);
            }
        }

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
        record_cell_event(game, (size_t)game->selected_unit, 0);
    }

    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    return 0;
}

int fd2_field_game_finish_unit_action(fd2_field_game *game) {
    if (!game || !game->ready || game->selected_unit < 0 ||
        (size_t)game->selected_unit >= game->units.count ||
        game->interaction != FD2_FIELD_INTERACTION_COMMAND)
        return -1;

    size_t unit_index = (size_t)game->selected_unit;
    if (!unit_can_act_for_side(&game->units.items[unit_index],
                               game->active_side) ||
        fd2_field_unit_has_acted(&game->units.items[unit_index]))
        return -1;
    record_cell_event(game, unit_index, 1);
    fd2_field_unit_set_acted(&game->units.items[unit_index], 1);
    clear_move_query(game);
    game->selected_unit = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    if (fd2_field_game_remaining_units(game, game->active_side) == 0)
        advance_phase(game);
    return 0;
}

int fd2_field_game_end_active_phase(fd2_field_game *game) {
    if (!game || !game->ready || game->selected_unit >= 0 ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE)
        return -1;
    for (size_t i = 0; i < game->units.count; i++) {
        if (unit_can_act_for_side(&game->units.items[i], game->active_side))
            fd2_field_unit_set_acted(&game->units.items[i], 1);
    }
    advance_phase(game);
    return 0;
}

int fd2_field_game_process_automatic_action(fd2_field_game *game) {
    if (!game || !game->ready || game->active_side == 2 ||
        game->selected_unit >= 0 ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE)
        return 0;

    for (size_t scanned = 0; scanned < game->units.count; scanned++) {
        size_t i = (game->phase_unit_cursor + scanned) % game->units.count;
        fd2_field_unit *unit = &game->units.items[i];
        if (!unit_can_act_for_side(unit, game->active_side) ||
            fd2_field_unit_has_acted(unit))
            continue;
        game->phase_unit_cursor = (i + 1) % game->units.count;
        game->cursor_cell_x = unit->x;
        game->cursor_cell_y = unit->y;
        record_cell_event(game, i, 1);
        fd2_field_unit_set_acted(unit, 1);
        if (fd2_field_game_remaining_units(game, game->active_side) == 0)
            advance_phase(game);
        return 1;
    }

    /* side 1 在多数早期关卡没有活动单位，也必须经过其事件检查后
     * 安全进入 side 0。 */
    advance_phase(game);
    return 1;
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
    /* field_earthquake_effect @0x4673b 从 DS:0x2096..0x20b6 复制的
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
    /* field_transition_lut_mask @0x4725a：整圆一次、下半圆再一次，再对上半部
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
