/* 炎龙骑士团 2 SDL3 重写 - 正式战场循环入口
 *
 * 当前阶段完成 session 生命周期、开场状态交接、键盘光标、移动、
 * 四向图形指令菜单、无演出攻击和取消回退。
 */

#include "field_play.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <SDL3/SDL.h>

#include "field_ai.h"
#include "field_attack.h"
#include "field_game.h"
#include "field_rng.h"
#include "font.h"
#include "input.h"
#include "text.h"
#include "scene.h"

/* 菜单机器码每相位执行一次 field buffer 合成，但没有 BIOS tick delay。
 * SDL 以一个约 60 Hz 宿主帧分隔四个 present，避免连续上传被合并；
 * 该值不登记为原版 DOS 时序。 */
#define FD2_FIELD_COMMAND_FRAME_MS 16
#define FD2_FIELD_AUTOMATIC_ACTION_MS 150

static int play_system_confirmation_text(fd2_field_game *game,
                                         fd2_vga *vga,
                                         size_t fragment,
                                         int fast) {
    if (!game || !vga || !game->ui_text.data ||
        !game->font.glyphs || !game->ready)
        return -1;
    const uint8_t *tokens;
    size_t token_count;
    if (fd2_text_entry_get_fragment(&game->ui_text, fragment,
                                    &tokens, &token_count) != 0)
        return -1;
    /* FDTXT confirmation fragments are short single-line messages. Render
     * token glyphs in a neutral lower dialog strip; the actual yes/no choice
     * remains a system-menu state and no action is committed here. */
    for (int row = 0; row < 86; row++)
        memset(vga->framebuffer + (112 + row) * VGA_STRIDE + 5,
               0xcd, 310u);
    int x = 15;
    int y = 119;
    for (size_t i = 0; i < token_count; i++) {
        int16_t token = (int16_t)fd2_text_token_at(tokens, i);
        if (token == -1) break;
        if (token == -2) {
            x = 15;
            y += 16;
            continue;
        }
        if (token < 0) continue;
        fd2_font_draw_glyph(vga, &game->font, (uint16_t)token,
                            x, y, 0xcd, 0x4c, -1);
        x += 16;
    }
    fd2_vga_present(vga);
    if (!fast) fd2_delay_ms(250);
    return 0;
}

static void apply_field_audio_options(const fd2_field_game *game,
                                      fd2_field_audio *field_audio) {
    if (!game || !field_audio || !field_audio->player ||
        !field_audio->player->audio)
        return;
    /* DS:0x1e61 控制 sequence 音量，DS:0x1e62 控制 SFX 播放门。当前
     * 尚无 XMIDI source；仍设置两条 bus，供后续音乐后端直接继承。 */
    (void)fd2_audio_set_bus_gain(
        field_audio->player->audio, FD2_AUDIO_BUS_MUSIC,
        game->option_values[0] ? 1.0f : 0.0f);
    (void)fd2_audio_set_bus_gain(
        field_audio->player->audio, FD2_AUDIO_BUS_SFX,
        game->option_values[1] ? 1.0f : 0.0f);
}

static void detail_phase_sfx(void *userdata, int opening, int phase) {
    fd2_field_audio *audio = userdata;
    if (!audio || fd2_field_detail_sfx_for_phase(opening, phase) < 0) return;
    /* 原版详情路径在上述 phase 调用
     * sfx_play(DAT_00003eec,sample,1)；全局 AIL handle 会重启前一声。 */
    (void)fd2_field_audio_play(audio, opening
        ? FD2_FIELD_SFX_DETAIL_OPEN : FD2_FIELD_SFX_DETAIL_CLOSE);
}

typedef enum {
    FD2_FIELD_ACTION_NONE = 0,
    FD2_FIELD_ACTION_LEFT,
    FD2_FIELD_ACTION_RIGHT,
    FD2_FIELD_ACTION_UP,
    FD2_FIELD_ACTION_DOWN,
    FD2_FIELD_ACTION_CONFIRM,
    FD2_FIELD_ACTION_CANCEL,
    FD2_FIELD_ACTION_DETAIL,
    FD2_FIELD_ACTION_AUXILIARY,
    FD2_FIELD_ACTION_FOCUS_CYCLE,
    FD2_FIELD_ACTION_EXIT
} fd2_field_action;

static fd2_field_action field_action_from_input(fd2_input_action action) {
    switch (action) {
        case FD2_INPUT_ACTION_LEFT: return FD2_FIELD_ACTION_LEFT;
        case FD2_INPUT_ACTION_RIGHT: return FD2_FIELD_ACTION_RIGHT;
        case FD2_INPUT_ACTION_UP: return FD2_FIELD_ACTION_UP;
        case FD2_INPUT_ACTION_DOWN: return FD2_FIELD_ACTION_DOWN;
        case FD2_INPUT_ACTION_CONFIRM: return FD2_FIELD_ACTION_CONFIRM;
        case FD2_INPUT_ACTION_CANCEL: return FD2_FIELD_ACTION_CANCEL;
        case FD2_INPUT_ACTION_FIELD_DETAIL: return FD2_FIELD_ACTION_DETAIL;
        case FD2_INPUT_ACTION_FIELD_AUXILIARY:
            return FD2_FIELD_ACTION_AUXILIARY;
        case FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE:
            return FD2_FIELD_ACTION_FOCUS_CYCLE;
        case FD2_INPUT_ACTION_EXIT: return FD2_FIELD_ACTION_EXIT;
        default: return FD2_FIELD_ACTION_NONE;
    }
}

static int detail_sprite_matches(const fd2_vga *vga,
                                 const fd2_image *image,
                                 int dst_x, int dst_y) {
    if (!vga || !image || !image->pixels) return 0;
    for (int y = 0; y < image->height; y++) {
        for (int x = 0; x < image->width; x++) {
            uint8_t pixel = image->pixels[(size_t)y * image->width + x];
            if (pixel != 0)
                return vga->framebuffer[(dst_y + y) * VGA_STRIDE +
                                        dst_x + x] == pixel;
        }
    }
    return 0;
}

static int validate_field_info(fd2_field_game *game, fd2_vga *vga) {
    if (!game || !vga || !game->info_assets.ready ||
        game->info_assets.panel.width != 69 ||
        game->info_assets.panel.height != 34 || game->units.count == 0)
        return -1;
    int saved_cursor_x = game->cursor_cell_x;
    int saved_cursor_y = game->cursor_cell_y;
    int saved_panel_right = game->info_panel_right;
    game->cursor_cell_x = game->units.items[0].x;
    game->cursor_cell_y = game->units.items[0].y;
    fd2_field_game_render(game, vga);
    /* 左侧底板、unit sprite 与三位 HP 都应覆盖对应位置。 */
    int ok = vga->framebuffer[161 * VGA_STRIDE + 5] != 0 &&
             vga->framebuffer[182 * VGA_STRIDE + 14] != 0;
    const uint8_t *record = (const uint8_t *)&game->units.items[0];
    if (!ok || game->units.items[0].attack != 16 ||
        game->units.items[0].defense != 12 ||
        game->units.items[0].accuracy != 97 ||
        game->units.items[0].evasion != 2 ||
        record[0x0a] != 0x40 || record[0x0b] != 0 ||
        record[0x0c] != 0x40 || record[0x0d] != 132 ||
        record[0x0e] != 0 || record[0x0f] != 192 ||
        fd2_field_game_open_detail(game, 0) != 0)
        ok = 0;
    static const uint8_t other_equipment[][2] = {
        {52, 164}, {20, 128}, {72, 178},
    };
    for (size_t unit_index = 1; ok && unit_index < 4; unit_index++) {
        const uint8_t *other = (const uint8_t *)&game->units.items[unit_index];
        if (other[0x0a] != 0x40 ||
            other[0x0b] != other_equipment[unit_index - 1][0] ||
            other[0x0c] != 0x40 ||
            other[0x0d] != other_equipment[unit_index - 1][1])
            ok = 0;
        for (size_t slot = 2; slot < 8; slot++) {
            if (other[0x0a + slot * 2u] != 0x80u ||
                other[0x0b + slot * 2u] != 0xffu)
                ok = 0;
        }
    }
    if (ok) {
        uint8_t *status_record = (uint8_t *)&game->units.items[0];
        uint8_t saved_status[6];
        memcpy(saved_status, status_record + 0x22, sizeof(saved_status));
        memset(status_record + 0x22, 1, sizeof(saved_status));
        fd2_field_game_render(game, vga);
        ok = game->detail_visible && game->detail_portrait.pixels &&
             vga->framebuffer[7 * VGA_STRIDE + 92] != 0 &&
             vga->framebuffer[94 * VGA_STRIDE + 5] != 0 &&
             detail_sprite_matches(vga,
                                   &game->info_assets.detail_border[1],
                                   5, 7) &&
             detail_sprite_matches(vga,
                                   &game->info_assets.status_icons[0],
                                   194, 68) &&
             detail_sprite_matches(vga,
                                   &game->info_assets.digits[0][0],
                                   267, 41) &&
             detail_sprite_matches(vga,
                                   &game->info_assets.digits[2][0],
                                   117, 67) &&
             detail_sprite_matches(vga,
                                   &game->info_assets.detail_icons[3],
                                   13, 101) &&
             detail_sprite_matches(vga,
                                   &game->info_assets.detail_icons[5],
                                   108, 105);
        memcpy(status_record + 0x22, saved_status, sizeof(saved_status));
    }
    fd2_field_game_close_detail(game);
    game->cursor_cell_x = saved_cursor_x;
    game->cursor_cell_y = saved_cursor_y;
    game->info_panel_right = saved_panel_right;
    return ok ? 0 : -1;
}

static int validate_move_ranges(const fd2_field_game *game) {
    if (!game || !game->ready || game->units.count < 4) return -1;
    printf("field move ranges:");
    for (size_t i = 0; i < 4; i++) {
        fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
        if (fd2_field_game_compute_move_range(game, i, &range) != 0 ||
            !fd2_field_path_is_destination(&range,
                                            game->units.items[i].x,
                                            game->units.items[i].y)) {
            fd2_field_path_close(&range);
            return -1;
        }
        size_t destination_count = 0;
        size_t node_count = (size_t)range.width * (size_t)range.height;
        for (size_t node = 0; node < node_count; node++) {
            if (range.nodes[node].distance != FD2_FIELD_PATH_UNREACHABLE &&
                range.nodes[node].can_stop)
                destination_count++;
        }
        if (destination_count < 2) {
            fd2_field_path_close(&range);
            return -1;
        }
        printf(" unit%zu=%zu", i, destination_count);
        fd2_field_path_close(&range);
    }
    putchar('\n');
    return 0;
}

static int validate_ai_queries(const fd2_field_game *game) {
    if (!game || !game->ready || game->units.count != 12) return -1;
    fd2_field_units units_before = game->units;
    uint32_t turn_before = game->turn_number;
    uint8_t side_before = game->active_side;
    fd2_field_interaction interaction_before = game->interaction;
    size_t phase_cursor_before = game->phase_unit_cursor;
    int selected_before = game->selected_unit;
    int cursor_x_before = game->cursor_cell_x;
    int cursor_y_before = game->cursor_cell_y;
    for (size_t i = 4; i < 12; i++) {
        const fd2_field_unit *unit = &game->units.items[i];
        if (unit->side != 0 || fd2_field_ai_behavior(unit) != 0 ||
            unit->ai_param_35 != 0 || unit->ai_param_36 != 0)
            return -1;

        fd2_field_ai_target target;
        fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
        fd2_field_ai_destination destination;
        fd2_field_ai_physical_candidate physical;
        int ok = fd2_field_ai_nearest_opponent(
                     &game->units, i, 0, &target) == 0 &&
                 fd2_field_game_compute_move_range(game, i, &range) == 0 &&
                 fd2_field_ai_choose_destination(
                     &range, target.x, target.y, &destination) == 0 &&
                 fd2_field_path_is_destination(
                     &range, destination.x, destination.y) &&
                 destination.manhattan_distance <=
                     target.manhattan_distance;
        int physical_result = ok ? fd2_field_ai_choose_physical_candidate(
            &game->map, &game->terrain, &game->units, i, 0,
            &range, &physical) : -1;
        if (physical_result == 0) {
            ok = physical.unit_index < game->units.count &&
                 game->units.items[physical.unit_index].side != 0 &&
                 fd2_field_path_is_destination(
                     &range, physical.destination_x,
                     physical.destination_y) &&
                 (physical.priority == 0 || physical.priority == 8 ||
                  physical.priority == 0x12) &&
                 fd2_field_ai_select_attack_action(
                     physical.priority, 0, 0, 0) ==
                     (physical.priority >= 6
                         ? FD2_FIELD_AI_ACTION_PHYSICAL
                         : FD2_FIELD_AI_ACTION_NONE);
        }
        fd2_field_path_close(&range);
        if (!ok) return -1;
    }
    return memcmp(&game->units, &units_before, sizeof(units_before)) == 0 &&
           game->turn_number == turn_before && game->active_side == side_before &&
           game->interaction == interaction_before &&
           game->phase_unit_cursor == phase_cursor_before &&
           game->selected_unit == selected_before &&
           game->cursor_cell_x == cursor_x_before &&
           game->cursor_cell_y == cursor_y_before
        ? 0 : -1;
}

static int validate_move_interaction(fd2_field_game *game, fd2_vga *vga) {
    if (!game || !game->ready || !vga || game->units.count == 0)
        return -1;

    fd2_field_unit saved_unit = game->units.items[0];
    uint8_t saved_walk_frame = game->units.walk_frames[0];
    int saved_camera_x = game->camera_cell_x;
    int saved_camera_y = game->camera_cell_y;
    int saved_cursor_x = game->cursor_cell_x;
    int saved_cursor_y = game->cursor_cell_y;
    int result = -1;

    game->selected_unit = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->cursor_cell_x = game->units.items[0].x;
    game->cursor_cell_y = game->units.items[0].y;
    if (fd2_field_game_confirm_cursor(game) != 0 ||
        game->interaction != FD2_FIELD_INTERACTION_UNIT_SELECTED ||
        !game->move_preview_valid)
        goto done;

    int target_x = -1;
    int target_y = -1;
    uint32_t farthest = 0;
    for (int y = 0; y < game->move_range.height; y++) {
        for (int x = 0; x < game->move_range.width; x++) {
            uint32_t distance = fd2_field_path_distance(&game->move_range,
                                                        x, y);
            if (!fd2_field_path_is_destination(&game->move_range, x, y) ||
                (x == game->move_origin_x && y == game->move_origin_y) ||
                distance < farthest)
                continue;
            farthest = distance;
            target_x = x;
            target_y = y;
        }
    }
    if (target_x < 0) goto done;

    game->cursor_cell_x = target_x;
    game->cursor_cell_y = target_y;
    if (fd2_field_game_update_move_preview(game) != 0 ||
        !game->move_preview_valid || game->move_path_length == 0)
        goto done;

    int expected_x = game->move_origin_x;
    int expected_y = game->move_origin_y;
    int expected_camera_x = game->move_origin_camera_x;
    int expected_camera_y = game->move_origin_camera_y;
    int max_camera_x = game->map.width - FD2_FIELD_VIEW_CELLS_X;
    int max_camera_y = game->map.height - FD2_FIELD_VIEW_CELLS_Y;
    if (max_camera_x < 0) max_camera_x = 0;
    if (max_camera_y < 0) max_camera_y = 0;
    for (size_t i = 0; i < game->move_path_length; i++) {
        switch (game->move_path[i]) {
            case 0:
                if (expected_y - expected_camera_y >= 6 &&
                    expected_camera_y < max_camera_y)
                    expected_camera_y++;
                expected_y++;
                break;
            case 1:
                if (expected_x - expected_camera_x < 2 &&
                    expected_camera_x > 0)
                    expected_camera_x--;
                expected_x--;
                break;
            case 2:
                if (expected_y - expected_camera_y < 2 &&
                    expected_camera_y > 0)
                    expected_camera_y--;
                expected_y--;
                break;
            case 3:
                if (expected_x - expected_camera_x >= 11 &&
                    expected_camera_x < max_camera_x)
                    expected_camera_x++;
                expected_x++;
                break;
            default:
                goto done;
        }
    }

    if (expected_x != target_x || expected_y != target_y ||
        fd2_field_game_confirm_cursor(game) != 0 ||
        game->interaction != FD2_FIELD_INTERACTION_MOVING ||
        fd2_field_game_execute_move(game, vga, 0) != 0 ||
        game->interaction != FD2_FIELD_INTERACTION_COMMAND ||
        !game->command_assets.ready ||
        game->command_selected < 0 ||
        !game->command_disabled[FD2_FIELD_COMMAND_MAGIC] ||
        !game->command_disabled[FD2_FIELD_COMMAND_ITEM] ||
        game->command_disabled[FD2_FIELD_COMMAND_WAIT] ||
        game->units.items[0].x != target_x ||
        game->units.items[0].y != target_y ||
        game->camera_cell_x != expected_camera_x ||
        game->camera_cell_y != expected_camera_y ||
        game->units.items[0].frame_phase != 0 ||
        game->camera_pixel_offset_x != 0 ||
        game->camera_pixel_offset_y != 0)
        goto done;

    if (!fd2_field_game_cancel_selection(game) ||
        game->interaction != FD2_FIELD_INTERACTION_UNIT_SELECTED ||
        game->units.items[0].x != game->move_origin_x ||
        game->units.items[0].y != game->move_origin_y ||
        game->units.items[0].direction != game->move_origin_direction ||
        game->camera_cell_x != game->move_origin_camera_x ||
        game->camera_cell_y != game->move_origin_camera_y ||
        !game->move_preview_valid)
        goto done;

    game->cursor_cell_x = target_x;
    game->cursor_cell_y = target_y;
    if (fd2_field_game_update_move_preview(game) != 0 ||
        fd2_field_game_confirm_cursor(game) != 0 ||
        fd2_field_game_execute_move(game, vga, 0) != 0 ||
        fd2_field_game_confirm_cursor(game) != 0 ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE ||
        game->selected_unit != -1 || game->move_range.nodes != NULL)
        goto done;

    /* 独立覆盖四个方向的单格执行和镜头阈值，不依赖上面所选路径恰好
     * 包含哪些方向。 */
    static const struct {
        uint8_t direction;
        uint8_t start_x, start_y;
        uint8_t camera_x, camera_y;
        uint8_t end_x, end_y;
        uint8_t end_camera_x, end_camera_y;
    } cases[] = {
        {0, 5, 6, 0, 0, 5, 7, 0, 1},
        {1, 2, 5, 1, 0, 1, 5, 0, 0},
        {2, 5, 2, 0, 1, 5, 1, 0, 0},
        {3, 11, 5, 0, 0, 12, 5, 1, 0},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        game->units.items[0].x = cases[i].start_x;
        game->units.items[0].y = cases[i].start_y;
        game->units.items[0].frame_phase = 0;
        game->camera_cell_x = cases[i].camera_x;
        game->camera_cell_y = cases[i].camera_y;
        game->selected_unit = 0;
        game->interaction = FD2_FIELD_INTERACTION_MOVING;
        game->move_preview_valid = 1;
        game->move_path_length = 1;
        game->move_path[0] = cases[i].direction;
        if (fd2_field_game_execute_move(game, vga, 0) != 0 ||
            game->interaction != FD2_FIELD_INTERACTION_COMMAND ||
            game->units.items[0].x != cases[i].end_x ||
            game->units.items[0].y != cases[i].end_y ||
            game->units.items[0].direction != cases[i].direction ||
            game->units.items[0].frame_phase != 0 ||
            game->camera_cell_x != cases[i].end_camera_x ||
            game->camera_cell_y != cases[i].end_camera_y ||
            game->camera_pixel_offset_x != 0 ||
            game->camera_pixel_offset_y != 0)
            goto done;
    }
    result = 0;

done:
    fd2_field_path_close(&game->move_range);
    game->units.items[0] = saved_unit;
    game->units.walk_frames[0] = saved_walk_frame;
    game->camera_cell_x = saved_camera_x;
    game->camera_cell_y = saved_camera_y;
    game->camera_pixel_offset_x = 0;
    game->camera_pixel_offset_y = 0;
    game->cursor_cell_x = saved_cursor_x;
    game->cursor_cell_y = saved_cursor_y;
    game->selected_unit = -1;
    game->move_path_length = 0;
    game->move_preview_valid = 0;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    return result;
}

typedef struct {
    const uint32_t *values;
    size_t count;
    size_t cursor;
} m6_rng_sequence;

static uint32_t m6_rng_next(void *userdata) {
    m6_rng_sequence *sequence = userdata;
    if (!sequence || sequence->cursor >= sequence->count) return 0;
    return sequence->values[sequence->cursor++];
}

/* M6 垂直切片：使用 field_target_range_build @0x39a2c 已确认的武器
 * 范围与敌方过滤，使用已捕获的 profile 暴击表和固定 RNG。 */
static int validate_attack_flow(const fd2_field_game *game) {
    if (!game || !game->ready || game->active_side != 2) return -1;

    fd2_field_game replay = *game;
    replay.move_range = (fd2_field_path_result)FD2_FIELD_PATH_RESULT_INITIALIZER;
    replay.move_path = NULL;
    replay.move_path_capacity = 0;
    replay.move_path_length = 0;
    replay.move_preview_valid = 0;
    replay.selected_unit = -1;
    replay.attack_target = -1;
    replay.last_attack_valid = 0;
    replay.interaction = FD2_FIELD_INTERACTION_BROWSE;

    int defender_index = -1;
    for (size_t i = 0; i < replay.units.count; i++) {
        if (replay.units.items[i].side == 0 &&
            !fd2_field_unit_is_hidden(&replay.units.items[i])) {
            defender_index = (int)i;
            break;
        }
    }
    if (defender_index < 0) return -1;

    int attacker_index = -1;
    int target_x = -1;
    int target_y = -1;
    for (size_t i = 0; i < replay.units.count && attacker_index < 0; i++) {
        fd2_field_unit *candidate = &replay.units.items[i];
        if (candidate->side != 2 || fd2_field_unit_is_hidden(candidate))
            continue;
        uint8_t original_x = candidate->x;
        uint8_t original_y = candidate->y;
        for (int origin_y = 0;
             origin_y < replay.map.height && attacker_index < 0;
             origin_y++) {
            for (int origin_x = 0;
                 origin_x < replay.map.width && attacker_index < 0;
                 origin_x++) {
                candidate->x = (uint8_t)origin_x;
                candidate->y = (uint8_t)origin_y;
                fd2_field_path_result attack_range =
                    FD2_FIELD_PATH_RESULT_INITIALIZER;
                if (fd2_field_attack_range_compute(
                        &attack_range, &replay.map,
                        &replay.terrain, candidate) != 0)
                    continue;
                for (int y = 0; y < replay.map.height && target_x < 0; y++) {
                    for (int x = 0; x < replay.map.width; x++) {
                        if (!fd2_field_path_is_destination(&attack_range,
                                                           x, y))
                            continue;
                        attacker_index = (int)i;
                        target_x = x;
                        target_y = y;
                        break;
                    }
                }
                fd2_field_path_close(&attack_range);
            }
        }
        if (attacker_index < 0) {
            candidate->x = original_x;
            candidate->y = original_y;
        }
    }
    if (attacker_index < 0 || target_x < 0) return -1;

    fd2_field_unit *attacker = &replay.units.items[attacker_index];
    fd2_field_unit *defender = &replay.units.items[defender_index];
    /* 集成验证在 session 副本中复用真实地形；目标格若已有剧情重叠 actor，
     * 先隐藏它们，确保 unit_at 稳定返回被测防御者。 */
    for (size_t i = 0; i < replay.units.count; i++) {
        if ((int)i == attacker_index || (int)i == defender_index) continue;
        if ((replay.units.items[i].x == target_x &&
             replay.units.items[i].y == target_y) ||
            (replay.units.items[i].x == attacker->x &&
             replay.units.items[i].y == attacker->y))
            fd2_field_unit_set_hidden(&replay.units.items[i], 1);
    }
    defender->x = (uint8_t)target_x;
    defender->y = (uint8_t)target_y;
    fd2_field_unit_set_hidden(defender, 0);
    fd2_field_unit_set_acted(attacker, 0);
    attacker->attack = 100;
    attacker->accuracy = 100;
    defender->defense = 0;
    defender->evasion = 0;
    defender->hp = 10;
    defender->hp_max = 10;

    const uint32_t rolls[] = {50, 0, 99, 8};
    m6_rng_sequence rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    replay.selected_unit = attacker_index;
    replay.interaction = FD2_FIELD_INTERACTION_COMMAND;
    replay.cursor_cell_x = attacker->x;
    replay.cursor_cell_y = attacker->y;
    if (fd2_field_game_set_attack_hooks(
            &replay, NULL, NULL, NULL, NULL, m6_rng_next, &rng) != 0 ||
        fd2_field_game_begin_attack(&replay) != 0 ||
        replay.interaction != FD2_FIELD_INTERACTION_TARGETING)
        return -1;

    /* 非法目标和取消都不能消耗 RNG、扣 HP 或标记攻击者。 */
    replay.cursor_cell_x = attacker->x;
    replay.cursor_cell_y = attacker->y;
    if (fd2_field_game_resolve_attack(&replay) == 0 || rng.cursor != 0 ||
        defender->hp != 10 || fd2_field_unit_has_acted(attacker) ||
        !fd2_field_game_cancel_attack(&replay) ||
        replay.interaction != FD2_FIELD_INTERACTION_COMMAND)
        return -1;
    replay.cursor_cell_x = defender->x;
    replay.cursor_cell_y = defender->y;
    if (fd2_field_game_confirm_cursor(&replay) != attacker_index ||
        replay.interaction != FD2_FIELD_INTERACTION_TARGETING)
        return -1;
    fd2_field_unit_set_acted(attacker, 1);
    if (fd2_field_game_confirm_cursor(&replay) != -1 ||
        rng.cursor != 0 || defender->hp != 10 ||
        replay.interaction != FD2_FIELD_INTERACTION_TARGETING)
        return -1;
    fd2_field_unit_set_acted(attacker, 0);
    if (fd2_field_game_confirm_cursor(&replay) != attacker_index ||
        !replay.last_attack_valid || !replay.last_attack.hit ||
        !replay.last_attack.defeated || defender->hp != 0 ||
        !fd2_field_unit_has_acted(attacker) ||
        replay.interaction != FD2_FIELD_INTERACTION_BROWSE ||
        replay.selected_unit != -1 || rng.cursor != 4)
        return -1;
    return 0;
}

static int validate_deferred_presentations(
        const fd2_field_game *game,
        fd2_vga *vga,
        const fd2_archive *fdother,
        size_t *validated_count) {
    if (!game || !vga || !fdother || !validated_count) return -1;
    while (*validated_count < game->event_log_count) {
        size_t index = (*validated_count)++;
        const fd2_field_event_notice *notice = &game->event_log[index];
        if (!notice->handled || !notice->presentation_deferred) continue;
        fd2_field_game replay = *game;
        fd2_field_event_notice replay_notice = *notice;
        if (fd2_scene_play_field_event(vga, fdother, &replay,
                                       &replay_notice, 1) != 0 ||
            replay_notice.presentation_deferred)
            return -1;
    }
    return 0;
}

static int validate_turn_flow(fd2_field_game *game, fd2_vga *vga,
                              const fd2_archive *fdother) {
    if (!game || !game->ready || !vga || !fdother || game->turn_number != 1 ||
        game->active_side != 2 || game->selected_unit != -1 ||
        game->interaction != FD2_FIELD_INTERACTION_BROWSE ||
        game->move_range.nodes != NULL ||
        game->camera_pixel_offset_x != 0 ||
        game->camera_pixel_offset_y != 0)
        return -1;

    fd2_field_units saved_units = game->units;
    int saved_camera_x = game->camera_cell_x;
    int saved_camera_y = game->camera_cell_y;
    int saved_cursor_x = game->cursor_cell_x;
    int saved_cursor_y = game->cursor_cell_y;
    uint32_t saved_turn = game->turn_number;
    uint8_t saved_side = game->active_side;
    size_t saved_phase_cursor = game->phase_unit_cursor;
    uint64_t saved_phase_time = game->phase_next_action_ms;
    fd2_field_event_notice saved_log[FD2_FIELD_EVENT_LOG_CAPACITY];
    memcpy(saved_log, game->event_log, sizeof(saved_log));
    size_t saved_log_count = game->event_log_count;
    uint32_t saved_total = game->event_total_count;
    uint32_t saved_unhandled = game->unhandled_event_count;
    uint32_t saved_dropped = game->dropped_event_count;
    int result = -1;

    /* 原地确认等价于最小待机：仍经过移动确认状态，但路径长度为 0。 */
    game->cursor_cell_x = game->units.items[0].x;
    game->cursor_cell_y = game->units.items[0].y;
    if (fd2_field_game_confirm_cursor(game) != 0 ||
        fd2_field_game_confirm_cursor(game) != 0 ||
        game->interaction != FD2_FIELD_INTERACTION_MOVING ||
        fd2_field_game_execute_move(game, vga, 0) != 0 ||
        fd2_field_game_confirm_cursor(game) != 0 ||
        !fd2_field_unit_has_acted(&game->units.items[0]) ||
        fd2_field_game_remaining_units(game, 2) != 3)
        goto done;

    game->units = saved_units;
    game->cursor_cell_x = saved_cursor_x;
    game->cursor_cell_y = saved_cursor_y;
    game->event_log_count = 0;
    game->event_total_count = 0;
    game->unhandled_event_count = 0;
    game->dropped_event_count = 0;

    size_t guard = 0;
    size_t validated_presentations = 0;
    while (game->turn_number <= 6) {
        if (game->active_side != 2 ||
            fd2_field_game_end_active_phase(game) != 0 ||
            validate_deferred_presentations(
                game, vga, fdother, &validated_presentations) != 0)
            goto done;
        while (game->active_side != 2) {
            /* 该回归验证 stage event／phase 调度，不依赖 AI 战斗结果；
             * 明确结束自动 phase，避免候选 RNG 改写脚本夹具。 */
            if (fd2_field_game_end_active_phase(game) != 0 ||
                ++guard > 128 ||
                validate_deferred_presentations(
                    game, vga, fdother, &validated_presentations) != 0)
                goto done;
        }
    }

    if (game->turn_number != 7 || game->active_side != 2 ||
        fd2_field_game_remaining_units(game, 2) != 6 ||
        game->units.count != 27 ||
        fd2_field_game_group_count(game, 3) != 1 ||
        fd2_field_game_group_count(game, 7) != 1 ||
        fd2_field_game_group_count(game, 4) != 4 ||
        fd2_field_game_group_count(game, 5) != 5 ||
        fd2_field_game_group_count(game, 6) != 4 ||
        game->units.items[12].x != 11 || game->units.items[12].y != 13 ||
        game->units.items[12].direction != 1 ||
        game->units.items[13].x != 11 || game->units.items[13].y != 12 ||
        game->units.items[13].direction != 1 ||
        game->units.items[14].x != 15 || game->units.items[14].y != 21 ||
        game->units.items[18].x != 3 || game->units.items[18].y != 21 ||
        game->units.items[23].x != 18 || game->units.items[23].y != 15 ||
        game->units.items[26].x != 19 || game->units.items[26].y != 14 ||
        game->camera_cell_x != 11 || game->camera_cell_y != 11 ||
        game->event_log_count != 4 || game->event_total_count != 4 ||
        game->unhandled_event_count != 0 || game->dropped_event_count != 0 ||
        validated_presentations != 4)
        goto done;
    static const uint8_t expected_turn[] = {3, 4, 5, 6};
    static const uint8_t expected_side[] = {1, 0, 0, 1};
    static const uint8_t expected_action[] = {0, 1, 2, 3};
    static const uint8_t expected_pre_units[] = {12, 14, 18, 23};
    for (size_t i = 0; i < 4; i++) {
        const fd2_field_event_notice *notice = &game->event_log[i];
        if (notice->kind != FD2_FIELD_EVENT_TURN ||
            notice->turn_number != expected_turn[i] ||
            notice->phase != expected_side[i] ||
            notice->action != expected_action[i] || !notice->handled ||
            !notice->presentation_deferred ||
            notice->presentation_unit_count != expected_pre_units[i])
            goto done;
    }
    result = 0;

done:
    fd2_field_path_close(&game->move_range);
    game->units = saved_units;
    game->camera_cell_x = saved_camera_x;
    game->camera_cell_y = saved_camera_y;
    game->cursor_cell_x = saved_cursor_x;
    game->cursor_cell_y = saved_cursor_y;
    game->turn_number = saved_turn;
    game->active_side = saved_side;
    game->phase_unit_cursor = saved_phase_cursor;
    game->phase_next_action_ms = saved_phase_time;
    memcpy(game->event_log, saved_log, sizeof(saved_log));
    game->event_log_count = saved_log_count;
    game->event_total_count = saved_total;
    game->unhandled_event_count = saved_unhandled;
    game->dropped_event_count = saved_dropped;
    game->selected_unit = -1;
    game->move_path_length = 0;
    game->move_preview_valid = 0;
    game->camera_pixel_offset_x = 0;
    game->camera_pixel_offset_y = 0;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    return result;
}

static int present_field_events(fd2_field_game *game,
                                fd2_vga *vga,
                                const fd2_archive *fdother) {
    if (!game || !vga || !fdother) return -1;
    for (size_t i = 0; i < game->event_log_count; i++) {
        fd2_field_event_notice *notice = &game->event_log[i];
        if (!notice->handled || !notice->presentation_deferred) continue;
        int result = fd2_scene_play_field_event(
            vga, fdother, game, notice, 0);
        if (result == FD2_SCENE_RESULT_HOST_QUIT)
            return FD2_SCENE_RESULT_HOST_QUIT;
        if (result != 0)
            return -1;
    }
    return 0;
}

static void report_field_events(const fd2_field_game *game,
                                size_t *reported_count,
                                uint32_t *reported_dropped) {
    if (!game || !reported_count || !reported_dropped) return;
    while (*reported_count < game->event_log_count) {
        const fd2_field_event_notice *notice =
            &game->event_log[*reported_count];
        const char *status = notice->handled
            ? (notice->presentation_deferred
                   ? "handled, presentation deferred"
                   : "handled")
            : "unhandled, skipped";
        if (notice->kind == FD2_FIELD_EVENT_TURN) {
            printf("field event: turn=%u phase=%u slot=%zu action=%u %s\n",
                   notice->turn_number, notice->phase, notice->slot,
                   notice->action, status);
        } else {
            printf("field event: cell=(%u,%u) unit=%u slot=%zu code=%u %s\n",
                   notice->x, notice->y, notice->unit_index,
                   notice->slot, notice->action, status);
        }
        (*reported_count)++;
    }
    if (*reported_dropped != game->dropped_event_count) {
        uint32_t delta = game->dropped_event_count - *reported_dropped;
        printf("field event: %u additional unhandled event(s) skipped; "
               "details dropped from bounded log\n", delta);
        *reported_dropped = game->dropped_event_count;
    }
    if (game->event_log_count != 0 || game->dropped_event_count != 0)
        fflush(stdout);
}

static int validate_field_effects(fd2_field_game *game, fd2_vga *vga,
                                  fd2_field_audio *field_audio) {
    if (!game || !game->ready || !vga || game->units.count == 0) return -1;
    int result = -1;
    fd2_field_effect_frames frames = {0};
    uint8_t units[] = {0};
    uint8_t invalid_unit[] = {(uint8_t)game->units.count};

    if (fd2_field_game_prepare_actor_group_flash(
            game, vga, invalid_unit, 1, 0xc0, &frames) == 0 ||
        frames.storage != NULL ||
        fd2_field_game_prepare_stage_transition(
            game, vga, 0, 0, 0, 8, &frames) == 0 ||
        frames.storage != NULL)
        goto done;
    if (fd2_field_game_prepare_stage_transition(
            game, vga, INT_MIN, INT_MAX, 10, 8, &frames) != 0)
        goto done;
    fd2_field_effect_frames_close(&frames);

    if (fd2_field_game_prepare_actor_group_flash(
            game, vga, units, 1, 0xc0, &frames) != 0 ||
        !frames.snapshots[0] || !frames.snapshots[1] ||
        memcmp(frames.snapshots[0], frames.snapshots[1], VGA_W * VGA_H) == 0 ||
        fd2_field_effect_play(&frames, vga, NULL, 0) != 0 ||
        memcmp(vga->framebuffer, frames.snapshots[0], VGA_W * VGA_H) != 0)
        goto done;
    fd2_field_effect_frames_close(&frames);
    if (fd2_field_game_play_actor_group_flash(
            game, vga, field_audio, units, 1, 0xc0, 0) != 0)
        goto done;

    if (fd2_field_game_prepare_earthquake(game, vga, &frames) != 0 ||
        memcmp(frames.snapshots[0], frames.snapshots[1], VGA_W * VGA_H) == 0 ||
        memcmp(frames.snapshots[1], frames.snapshots[2], VGA_W * VGA_H) == 0 ||
        fd2_field_effect_play(&frames, vga, NULL, 0) != 0 ||
        /* 第 59 帧是 cycle[3]，应落在 source 1。 */
        memcmp(vga->framebuffer, frames.snapshots[1], VGA_W * VGA_H) != 0)
        goto done;
    fd2_field_effect_frames_close(&frames);
    if (fd2_field_game_play_earthquake(
            game, vga, field_audio, 0) != 0)
        goto done;

    int center_x = (game->cursor_cell_x - game->camera_cell_x) * 24 + 12;
    int center_y = (game->cursor_cell_y - game->camera_cell_y) * 24 + 16;
    if (fd2_field_game_prepare_stage_transition(
            game, vga, center_x, center_y, 10, 8, &frames) != 0 ||
        memcmp(frames.snapshots[0], frames.snapshots[8], VGA_W * VGA_H) == 0 ||
        fd2_field_effect_play(&frames, vga, NULL, 0) != 0 ||
        memcmp(vga->framebuffer, frames.snapshots[8], VGA_W * VGA_H) != 0)
        goto done;
    fd2_field_effect_frames_close(&frames);
    if (fd2_field_game_play_stage_transition(
            game, vga, field_audio, center_x, center_y, 10, 8, 0) != 0)
        goto done;
    result = 0;

done:
    fd2_field_effect_frames_close(&frames);
    fd2_field_game_render(game, vga);
    return result;
}

static int validate_cursor_controls(fd2_field_game *game) {
    if (!game || !game->ready || game->units.count == 0) return -1;

    int saved_camera_x = game->camera_cell_x;
    int saved_camera_y = game->camera_cell_y;
    int saved_cursor_x = game->cursor_cell_x;
    int saved_cursor_y = game->cursor_cell_y;
    int saved_selected = game->selected_unit;
    fd2_field_interaction saved_interaction = game->interaction;
    uint8_t saved_flags = game->units.items[0].flags;
    uint16_t saved_hp = game->units.items[0].hp;
    int result = -1;

    game->camera_cell_x = 5;
    game->camera_cell_y = 5;
    game->cursor_cell_x = 7;
    game->cursor_cell_y = 7;
    if (!fd2_field_game_move_cursor(game, -1, 0) ||
        game->cursor_cell_x != 6 || game->camera_cell_x != 4)
        goto done;

    game->camera_cell_x = 5;
    game->cursor_cell_x = 16;
    if (!fd2_field_game_move_cursor(game, 1, 0) ||
        game->cursor_cell_x != 17 || game->camera_cell_x != 6)
        goto done;

    game->camera_cell_y = 5;
    game->cursor_cell_y = 7;
    if (!fd2_field_game_move_cursor(game, 0, -1) ||
        game->cursor_cell_y != 6 || game->camera_cell_y != 4)
        goto done;

    game->camera_cell_y = 5;
    game->cursor_cell_y = 11;
    if (!fd2_field_game_move_cursor(game, 0, 1) ||
        game->cursor_cell_y != 12 || game->camera_cell_y != 6)
        goto done;

    game->camera_cell_x = 0;
    game->camera_cell_y = 0;
    game->cursor_cell_x = 0;
    game->cursor_cell_y = 0;
    if (fd2_field_game_move_cursor(game, -1, 0) ||
        fd2_field_game_move_cursor(game, 0, -1) ||
        game->cursor_cell_x != 0 || game->cursor_cell_y != 0)
        goto done;

    game->cursor_cell_x = game->units.items[0].x;
    game->cursor_cell_y = game->units.items[0].y;
    game->selected_unit = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    if (fd2_field_game_unit_at(game, game->cursor_cell_x,
                               game->cursor_cell_y) != 0 ||
        fd2_field_game_confirm_cursor(game) != 0 ||
        !fd2_field_game_cancel_selection(game))
        goto done;

    fd2_field_unit_set_hidden(&game->units.items[0], 1);
    if (fd2_field_game_unit_at(game, game->cursor_cell_x,
                               game->cursor_cell_y) != -1)
        goto done;
    game->units.items[0].flags = saved_flags;
    game->units.items[0].hp = 0;
    game->selected_unit = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    if (fd2_field_game_confirm_cursor(game) != -1)
        goto done;
    game->units.items[0].hp = saved_hp;

    game->cursor_cell_x = game->units.items[4].x;
    game->cursor_cell_y = game->units.items[4].y;
    game->selected_unit = -1;
    if (fd2_field_game_confirm_cursor(game) != -1)
        goto done;
    result = 0;

done:
    game->camera_cell_x = saved_camera_x;
    game->camera_cell_y = saved_camera_y;
    game->cursor_cell_x = saved_cursor_x;
    game->cursor_cell_y = saved_cursor_y;
    game->selected_unit = saved_selected;
    game->interaction = saved_interaction;
    game->units.items[0].flags = saved_flags;
    game->units.items[0].hp = saved_hp;
    return result;
}

fd2_field_play_result fd2_field_play_run(fd2_vga *vga,
                       const fd2_archive *fdother,
                       size_t stage,
                       int once,
                       const fd2_field_handoff *handoff,
                       fd2_field_audio *field_audio,
                       const char *save_path,
                       int load_active) {
    fd2_field_game game;
    if (!vga || !fdother) return FD2_FIELD_PLAY_RETURN_ERROR;
    if (fd2_field_game_open(&game, vga, fdother, stage) != 0) {
        fprintf(stderr, "cannot load stage %zu field game\n", stage);
        return FD2_FIELD_PLAY_RETURN_ERROR;
    }
    /* M6 已接入原版武器 record +0x0b/+0x0c 范围与 side==0 目标过滤。
     * loader 初值已捕获；原版每次进入战场控制循环前还会按计时器低
     * 8 位推进该流，活动快照未保存 RNG，因此读档不承诺续接同一序列。
     * profile 暴击基础值与武器 effect type 4 加值均使用已确认原版表。 */
    fd2_field_rng m6_rng;
    fd2_field_rng_seed(&m6_rng, 0x7a18);
    if (fd2_field_game_set_attack_hooks(
            &game, NULL, NULL, NULL, NULL,
            fd2_field_rng_next, &m6_rng) != 0) {
        fd2_field_game_close(&game);
        return FD2_FIELD_PLAY_RETURN_ERROR;
    }
    if (handoff && fd2_field_game_apply_handoff(&game, handoff) != 0) {
        fprintf(stderr, "cannot apply stage %zu opening handoff\n", stage);
        fd2_field_game_close(&game);
        return FD2_FIELD_PLAY_RETURN_ERROR;
    }
    if (load_active && fd2_field_game_load_active(&game, save_path) != 0) {
        fprintf(stderr, "cannot import active field save: %s\n",
                save_path ? save_path : "(null)");
        fd2_field_game_close(&game);
        return FD2_FIELD_PLAY_RETURN_ERROR;
    }
    fd2_field_game_set_save_resource_available(
        &game, fd2_field_game_save_resource_available(&game, save_path));
    apply_field_audio_options(&game, field_audio);
    if (once) {
#define VALIDATE_STEP(name, expression) do { \
        if ((expression) != 0) { \
            fprintf(stderr, "field validation failed: %s\n", (name)); \
            fd2_field_game_close(&game); \
            return FD2_FIELD_PLAY_RETURN_ERROR; \
        } \
    } while (0)
        VALIDATE_STEP("cursor", validate_cursor_controls(&game));
        VALIDATE_STEP("info", validate_field_info(&game, vga));
        VALIDATE_STEP("move-range", validate_move_ranges(&game));
        VALIDATE_STEP("ai-query", validate_ai_queries(&game));
        VALIDATE_STEP("move-interaction", validate_move_interaction(&game, vga));
        VALIDATE_STEP("attack", validate_attack_flow(&game));
        VALIDATE_STEP("turn", validate_turn_flow(&game, vga, fdother));
        VALIDATE_STEP("effect", validate_field_effects(
            &game, vga, field_audio));
#undef VALIDATE_STEP
    }

    printf("field play: stage=%zu, map=%dx%d, units=%zu, camera=(%d,%d), "
           "cursor=(%d,%d), turn=%u, phase=%u, handoff=%s%s\n",
           game.stage, game.map.width, game.map.height, game.units.count,
           game.camera_cell_x, game.camera_cell_y,
           game.cursor_cell_x, game.cursor_cell_y,
           game.turn_number, game.active_side,
           handoff ? "yes" : "no",
           once ? ", cursor-check=ok, info-check=ok, detail-check=ok, move-check=ok, "
                  "ai-query-check=ok, move-exec-check=ok, command-check=ok, attack-check=ok, turn-check=ok, event-check=ok, effect-check=ok, fast"
                : "");

    size_t reported_event_count = 0;
    uint32_t reported_dropped_events = 0;
    uint32_t reported_turn = game.turn_number;
    uint8_t reported_side = game.active_side;
    int running = 1;
    fd2_field_play_result play_result = FD2_FIELD_PLAY_RETURN_COMPLETE;
    while (running) {
        fd2_field_game_tick(&game, SDL_GetTicks());
        game.battle_result = fd2_field_game_battle_result(&game);
        if (game.battle_result != FD2_FIELD_BATTLE_ONGOING) {
            printf("field battle: %s, stage=%zu\n",
                   game.battle_result == FD2_FIELD_BATTLE_VICTORY
                       ? "victory" : "defeat",
                   game.stage);
            fflush(stdout);
            running = 0;
        }
        if (game.turn_number != reported_turn ||
            game.active_side != reported_side) {
            printf("field phase: turn=%u phase=%u remaining=%zu\n",
                   game.turn_number, game.active_side,
                   fd2_field_game_remaining_units(&game, game.active_side));
            fflush(stdout);
            reported_turn = game.turn_number;
            reported_side = game.active_side;
        }
        if (!once) {
            int event_result = present_field_events(&game, vga, fdother);
            if (event_result == FD2_SCENE_RESULT_HOST_QUIT) {
                play_result = FD2_FIELD_PLAY_RETURN_HOST_QUIT;
                running = 0;
            } else if (event_result != 0) {
                fprintf(stderr, "field event presentation failed\n");
                running = 0;
            }
        }
        report_field_events(&game, &reported_event_count,
                            &reported_dropped_events);
        fd2_field_game_render(&game, vga);
        fd2_vga_present(vga);
        if (once || !running) break;
        if (game.active_side != 2 &&
            game.interaction == FD2_FIELD_INTERACTION_BROWSE &&
            game.phase_next_action_ms != 0 &&
            SDL_GetTicks() >= game.phase_next_action_ms) {
            int ai_result = fd2_field_game_process_automatic_action_visual(
                &game, vga, 55);
            if (game.active_side != 2)
                game.phase_next_action_ms =
                    SDL_GetTicks() + FD2_FIELD_AUTOMATIC_ACTION_MS;
            if (ai_result < 0) {
                fprintf(stderr, "field automatic action unsupported or failed\n");
                running = 0;
            }
            continue;
        }


        if (fd2_input_take_quit(&vga->input)) {
            play_result = FD2_FIELD_PLAY_RETURN_HOST_QUIT;
            running = 0;
        }
        fd2_input_action input_action;
        while (running &&
               fd2_input_take_action(
                   &vga->input,
                   game.interaction == FD2_FIELD_INTERACTION_COMMAND
                       ? FD2_INPUT_CONTEXT_FIELD_COMMAND
                       : game.interaction == FD2_FIELD_INTERACTION_TARGETING
                           ? FD2_INPUT_CONTEXT_FIELD_TARGETING
                           : game.interaction ==
                                 FD2_FIELD_INTERACTION_SYSTEM_MENU
                               ? FD2_INPUT_CONTEXT_FIELD_SYSTEM_MENU
                               : game.interaction ==
                                     FD2_FIELD_INTERACTION_MANUAL_SLOT
                                   ? FD2_INPUT_CONTEXT_FIELD_MANUAL_SLOT
                                   : game.interaction ==
                                         FD2_FIELD_INTERACTION_AUXILIARY
                                       ? FD2_INPUT_CONTEXT_FIELD_AUXILIARY
                                       : FD2_INPUT_CONTEXT_FIELD,
                   &input_action)) {
            fd2_field_action action = field_action_from_input(input_action);
            if (game.detail_visible) {
                if (action == FD2_FIELD_ACTION_CONFIRM ||
                    action == FD2_FIELD_ACTION_CANCEL) {
                    (void)fd2_field_game_animate_detail(
                        &game, vga, 0, detail_phase_sfx, field_audio);
                    fd2_field_game_close_detail(&game);
                }
                continue;
            }
            if (game.active_side != 2 && action != FD2_FIELD_ACTION_EXIT)
                continue;
            switch (action) {
                case FD2_FIELD_ACTION_EXIT:
                    play_result = FD2_FIELD_PLAY_RETURN_HOST_QUIT;
                    running = 0;
                    break;
                case FD2_FIELD_ACTION_CANCEL:
                    if (game.interaction ==
                            FD2_FIELD_INTERACTION_MANUAL_SLOT) {
                        (void)fd2_field_game_cancel_manual_slot_picker(&game);
                    } else if (game.interaction ==
                            FD2_FIELD_INTERACTION_SYSTEM_MENU) {
                        fd2_field_system_page page = game.system_menu.page;
                        /* Secondary/options cancel first closes the current
                         * radial page, then parent page opens again. Parent
                         * cancel closes the menu before returning to browse. */
                        if (game.option_values[1])
                            (void)fd2_field_audio_play(
                                field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                        (void)fd2_field_game_animate_system_menu(
                            &game, vga, 0, FD2_FIELD_COMMAND_FRAME_MS);
                        (void)fd2_field_game_cancel_system_menu(&game);
                        if (page != FD2_FIELD_SYSTEM_PAGE_PARENT &&
                            game.interaction ==
                                FD2_FIELD_INTERACTION_SYSTEM_MENU) {
                            if (game.option_values[1])
                                (void)fd2_field_audio_play(
                                    field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                            (void)fd2_field_game_animate_system_menu(
                                &game, vga, 1, FD2_FIELD_COMMAND_FRAME_MS);
                        }
                    } else if (game.interaction ==
                                   FD2_FIELD_INTERACTION_AUXILIARY) {
                        game.interaction = FD2_FIELD_INTERACTION_BROWSE;
                    } else if (game.interaction ==
                                   FD2_FIELD_INTERACTION_UNIT_SELECTED ||
                               game.interaction == FD2_FIELD_INTERACTION_MOVING) {
                        (void)fd2_field_game_cancel_selection(&game);
                    } else if (game.interaction ==
                                   FD2_FIELD_INTERACTION_COMMAND) {
                        (void)fd2_field_audio_play(
                            field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                        (void)fd2_field_game_animate_command(
                            &game, vga, 0, FD2_FIELD_COMMAND_FRAME_MS);
                        (void)fd2_field_game_cancel_selection(&game);
                    } else if (game.interaction ==
                                   FD2_FIELD_INTERACTION_TARGETING &&
                               fd2_field_game_cancel_attack(&game)) {
                        /* field_player_command_execute_core @0x3dfaa：
                         * target selector 返回 -1 后重新进入菜单循环。 */
                        (void)fd2_field_audio_play(
                            field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                        (void)fd2_field_game_animate_command(
                            &game, vga, 1, FD2_FIELD_COMMAND_FRAME_MS);
                    }
                    break;
                case FD2_FIELD_ACTION_DETAIL: {
                    int unit_at = fd2_field_game_unit_at(
                        &game, game.cursor_cell_x, game.cursor_cell_y);
                    if (game.interaction == FD2_FIELD_INTERACTION_BROWSE &&
                        unit_at >= 0 &&
                        game.units.items[unit_at].unit_id != 0x79 &&
                        game.units.items[unit_at].race != 0x0a &&
                        fd2_field_game_open_detail(&game, (size_t)unit_at) == 0)
                        (void)fd2_field_game_animate_detail(
                            &game, vga, 1, detail_phase_sfx, field_audio);
                    break;
                }
                case FD2_FIELD_ACTION_AUXILIARY:
                    /* field_controller_input @code0 0x17e7 的 F1/Page Up
                     * 分支进入独立 tactical-map dispatcher @code0 0x1000a。
                     * 该页自身以键盘读取结束；SDL 先保留独立状态，不能
                     * 把它误并入 system/command menu。 */
                    if (game.interaction == FD2_FIELD_INTERACTION_BROWSE)
                        game.interaction = FD2_FIELD_INTERACTION_AUXILIARY;
                    break;
                case FD2_FIELD_ACTION_FOCUS_CYCLE:
                    if (game.interaction == FD2_FIELD_INTERACTION_BROWSE)
                        (void)fd2_field_game_cycle_focus(&game);
                    break;
                case FD2_FIELD_ACTION_CONFIRM: {
                    int unit_at = fd2_field_game_unit_at(
                        &game, game.cursor_cell_x, game.cursor_cell_y);
                    if (game.interaction == FD2_FIELD_INTERACTION_BROWSE &&
                        unit_at >= 0) {
                        const fd2_field_unit *focused =
                            &game.units.items[unit_at];
                        const uint8_t *record = (const uint8_t *)focused;
                        /* code0 0x1884：只有可行动的 side 2 普通单位进入
                         * 移动／操作路径；敌方、已行动单位和特殊记录先走
                         * 原版的单位详情 helper。 */
                        if (focused->unit_id != 0x79 &&
                            focused->race != 0x0a &&
                            (focused->side != 2u ||
                             fd2_field_unit_has_acted(focused) ||
                             record[0x26] != 0)) {
                            if (fd2_field_game_open_detail(
                                    &game, (size_t)unit_at) == 0)
                                (void)fd2_field_game_animate_detail(
                                    &game, vga, 1,
                                    detail_phase_sfx, field_audio);
                            break;
                        }
                    }
                    game.detail_acknowledged_unit = -1;
                    fd2_field_interaction previous_interaction =
                        game.interaction;
                    if (previous_interaction ==
                            FD2_FIELD_INTERACTION_MANUAL_SLOT) {
                        int slot = fd2_field_game_confirm_manual_slot_picker(
                            &game);
                        if (slot >= 0) {
                            int success;
                            size_t fragment = 0;
                            if (game.manual_slot_picker.mode ==
                                    FD2_FIELD_MANUAL_SLOT_MODE_SAVE) {
                                success = fd2_field_game_save_manual_slot(
                                    &game, save_path, (size_t)slot) == 0;
                                fragment = success
                                    ? FD2_FIELD_MANUAL_TEXT_SAVE_SUCCESS
                                    : FD2_FIELD_SYSTEM_TEXT_SAVE_FAILURE;
                                fd2_field_game_set_save_resource_available(
                                    &game,
                                    fd2_field_game_save_resource_available(
                                        &game, save_path));
                            } else {
                                success = fd2_field_game_load_manual_slot(
                                    &game, save_path, (size_t)slot) == 0;
                                fragment = success
                                    ? FD2_FIELD_MANUAL_TEXT_LOAD_SUCCESS
                                    : FD2_FIELD_SYSTEM_TEXT_LOAD_FAILURE;
                                if (success)
                                    apply_field_audio_options(&game,
                                                              field_audio);
                            }
                            game.manual_slot_result_fragment =
                                (uint16_t)fragment;
                            game.manual_slot_result_until_ms =
                                fragment != 0 ? SDL_GetTicks() + 1000u : 0;
                            if (fragment != 0)
                                (void)play_system_confirmation_text(
                                    &game, vga, fragment, once);
                            if (!success)
                                fprintf(stderr,
                                        "field manual slot %s failed: %d\n",
                                        game.manual_slot_picker.mode ==
                                                FD2_FIELD_MANUAL_SLOT_MODE_SAVE
                                            ? "save" : "load", slot);
                        }
                        break;
                    }
                    if (previous_interaction ==
                            FD2_FIELD_INTERACTION_SYSTEM_MENU) {
                        /* open/input/close primitive 的原版边界：确认后先按
                         * 当前 command IDs 收起，再进入下一 dispatcher。
                         * confirmation page 只展示 FDTXT 片段。 */
                        if (game.option_values[1])
                            (void)fd2_field_audio_play(
                                field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                        (void)fd2_field_game_animate_system_menu(
                            &game, vga, 0, FD2_FIELD_COMMAND_FRAME_MS);
                        fd2_field_system_action system_action =
                            fd2_field_game_confirm_system_menu(&game);
                        if (system_action ==
                                FD2_FIELD_SYSTEM_ACTION_OPTIONS_TOGGLE_PENDING)
                            apply_field_audio_options(&game, field_audio);
                        if (system_action ==
                                FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION) {
                            size_t fragment = 0;
                            if (fd2_field_system_menu_get_confirmation_fragment(
                                    &game.system_menu,
                                    FD2_FIELD_SYSTEM_CONFIRM_PROMPT,
                                    &fragment) == 0)
                                (void)play_system_confirmation_text(
                                    &game, vga, fragment, once);
                        } else if (system_action ==
                                       FD2_FIELD_SYSTEM_ACTION_SAVE ||
                                   system_action ==
                                       FD2_FIELD_SYSTEM_ACTION_LOAD) {
                            /* secondary dispatcher @code0 0x9df7 操作活动
                             * snapshot；四槽 hand_save/hand_load 属于独立
                             * dispatcher @code0 0x19300，不能在此替换。 */
                            size_t fragment = 0;
                            int success =
                                fd2_field_game_execute_storage_action(
                                    &game, system_action, save_path,
                                    &fragment) == 1;
                            if (success && system_action ==
                                               FD2_FIELD_SYSTEM_ACTION_LOAD)
                                apply_field_audio_options(&game, field_audio);
                            if (fragment != 0) {
                                game.system_result_fragment =
                                    (uint16_t)fragment;
                                game.system_result_until_ms =
                                    SDL_GetTicks() + 1000u;
                                (void)play_system_confirmation_text(
                                    &game, vga, fragment, once);
                            }
                            if (!success)
                                fprintf(stderr,
                                        "field %s failed: %s\n",
                                        system_action ==
                                                FD2_FIELD_SYSTEM_ACTION_SAVE
                                            ? "save" : "load",
                                        save_path ? save_path : "(null)");
                            if (game.interaction ==
                                    FD2_FIELD_INTERACTION_SYSTEM_MENU) {
                                if (game.option_values[1])
                                    (void)fd2_field_audio_play(
                                        field_audio,
                                        FD2_FIELD_SFX_COMMAND_MENU);
                                (void)fd2_field_game_animate_system_menu(
                                    &game, vga, 1,
                                    FD2_FIELD_COMMAND_FRAME_MS);
                            }
                        } else if (system_action ==
                                       FD2_FIELD_SYSTEM_ACTION_LEAVE_BATTLE) {
                            play_result = FD2_FIELD_PLAY_RETURN_TITLE;
                            running = 0;
                        } else {
                            if (game.option_values[1])
                                (void)fd2_field_audio_play(
                                    field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                            (void)fd2_field_game_animate_system_menu(
                                &game, vga, 1, FD2_FIELD_COMMAND_FRAME_MS);
                        }
                        /* march/end-turn 尚未接正式后端；Save／Load 与
                         * leave 已分别提交到文件编排和返回标题分流。 */
                        if (system_action !=
                                FD2_FIELD_SYSTEM_ACTION_OPEN_SECONDARY &&
                            system_action !=
                                FD2_FIELD_SYSTEM_ACTION_OPEN_OPTIONS &&
                            system_action !=
                                FD2_FIELD_SYSTEM_ACTION_OPTIONS_TOGGLE_PENDING &&
                            system_action != FD2_FIELD_SYSTEM_ACTION_SAVE &&
                            system_action != FD2_FIELD_SYSTEM_ACTION_LOAD &&
                            system_action !=
                                FD2_FIELD_SYSTEM_ACTION_LEAVE_BATTLE)
                            fprintf(stderr,
                                    "field system action pending: %d\n",
                                    (int)system_action);
                        break;
                    }
                    if (previous_interaction ==
                            FD2_FIELD_INTERACTION_AUXILIARY) {
                        game.interaction = FD2_FIELD_INTERACTION_BROWSE;
                        break;
                    }
                    if (previous_interaction ==
                            FD2_FIELD_INTERACTION_COMMAND) {
                        (void)fd2_field_audio_play(
                            field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                        (void)fd2_field_game_animate_command(
                            &game, vga, 0, FD2_FIELD_COMMAND_FRAME_MS);
                    }
                    int unit_index;
                    if (previous_interaction ==
                            FD2_FIELD_INTERACTION_BROWSE && unit_at < 0) {
                        unit_index = fd2_field_game_open_system_menu(&game);
                    } else {
                        unit_index = fd2_field_game_confirm_cursor(&game);
                    }
                    if (game.interaction ==
                            FD2_FIELD_INTERACTION_SYSTEM_MENU &&
                        previous_interaction == FD2_FIELD_INTERACTION_BROWSE) {
                        if (game.option_values[1])
                            (void)fd2_field_audio_play(
                                field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                        (void)fd2_field_game_animate_system_menu(
                            &game, vga, 1, FD2_FIELD_COMMAND_FRAME_MS);
                    }
                    if (game.interaction == FD2_FIELD_INTERACTION_MOVING) {
                        if (fd2_field_game_execute_move(&game, vga, 55) != 0) {
                            fprintf(stderr, "field move execution failed\n");
                            running = 0;
                            break;
                        }
                        (void)fd2_field_audio_play(
                            field_audio, FD2_FIELD_SFX_COMMAND_MENU);
                        (void)fd2_field_game_animate_command(
                            &game, vga, 1, FD2_FIELD_COMMAND_FRAME_MS);
                    }
                    if (game.last_attack_valid) {
                        printf("field attack: target=%d hit=%u critical=%u damage=%u hp=%u\n",
                               game.attack_target, game.last_attack.hit,
                               game.last_attack.critical, game.last_attack.damage,
                               game.last_attack.hp_after);
                        game.last_attack_valid = 0;
                    }
                    uint16_t terrain_id = fd2_field_cell_terrain(
                        fd2_field_map_cell(&game.map,
                                           game.cursor_cell_x,
                                           game.cursor_cell_y));
                    printf("field cursor: cell=(%d,%d), terrain=%u, "
                           "unit=%d, selected=%d, interaction=%d, "
                           "camera=(%d,%d), camera-offset=(%d,%d)\n",
                           game.cursor_cell_x, game.cursor_cell_y,
                           (unsigned)terrain_id, unit_at, unit_index,
                           (int)game.interaction,
                           game.camera_cell_x, game.camera_cell_y,
                           game.camera_pixel_offset_x,
                           game.camera_pixel_offset_y);
                    fflush(stdout);
                    break;
                }
                case FD2_FIELD_ACTION_LEFT:
                    if (game.interaction ==
                            FD2_FIELD_INTERACTION_MANUAL_SLOT)
                        (void)fd2_field_game_move_manual_slot_picker(&game, -1);
                    else if (game.interaction ==
                            FD2_FIELD_INTERACTION_SYSTEM_MENU)
                        (void)fd2_field_game_select_system_menu_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_LEFT);
                    else if (game.interaction == FD2_FIELD_INTERACTION_COMMAND)
                        (void)fd2_field_game_select_command_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_LEFT);
                    else
                        fd2_field_game_move_cursor(&game, -1, 0);
                    break;
                case FD2_FIELD_ACTION_RIGHT:
                    if (game.interaction ==
                            FD2_FIELD_INTERACTION_MANUAL_SLOT)
                        (void)fd2_field_game_move_manual_slot_picker(&game, 1);
                    else if (game.interaction ==
                            FD2_FIELD_INTERACTION_SYSTEM_MENU)
                        (void)fd2_field_game_select_system_menu_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_RIGHT);
                    else if (game.interaction == FD2_FIELD_INTERACTION_COMMAND)
                        (void)fd2_field_game_select_command_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_RIGHT);
                    else
                        fd2_field_game_move_cursor(&game, 1, 0);
                    break;
                case FD2_FIELD_ACTION_UP:
                    if (game.interaction ==
                            FD2_FIELD_INTERACTION_MANUAL_SLOT)
                        (void)fd2_field_game_move_manual_slot_picker(&game, -1);
                    else if (game.interaction ==
                            FD2_FIELD_INTERACTION_SYSTEM_MENU)
                        (void)fd2_field_game_select_system_menu_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_UP);
                    else if (game.interaction == FD2_FIELD_INTERACTION_COMMAND)
                        (void)fd2_field_game_select_command_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_UP);
                    else
                        fd2_field_game_move_cursor(&game, 0, -1);
                    break;
                case FD2_FIELD_ACTION_DOWN:
                    if (game.interaction ==
                            FD2_FIELD_INTERACTION_MANUAL_SLOT)
                        (void)fd2_field_game_move_manual_slot_picker(&game, 1);
                    else if (game.interaction ==
                            FD2_FIELD_INTERACTION_SYSTEM_MENU)
                        (void)fd2_field_game_select_system_menu_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_DOWN);
                    else if (game.interaction == FD2_FIELD_INTERACTION_COMMAND)
                        (void)fd2_field_game_select_command_direction(
                            &game, FD2_FIELD_COMMAND_DIRECTION_DOWN);
                    else
                        fd2_field_game_move_cursor(&game, 0, 1);
                    break;
                case FD2_FIELD_ACTION_NONE:
                    break;
            }
        }
        fd2_delay_ms(16);
    }

    fd2_field_game_close(&game);
    return play_result;
}

int fd2_field_effect_play_run(fd2_vga *vga,
                              const fd2_archive *fdother,
                              size_t stage,
                              fd2_field_audio *field_audio) {
    if (!vga || !fdother) return -1;
    fd2_field_game game;
    if (fd2_field_game_open(&game, vga, fdother, stage) != 0)
        return -1;
    int result = -1;
    uint8_t unit_indices[] = {0};
    int center_screen_x =
        (game.cursor_cell_x - game.camera_cell_x) * 24 + 12;
    int center_screen_y =
        (game.cursor_cell_y - game.camera_cell_y) * 24 + 16;

    fd2_field_game_render(&game, vga);
    fd2_vga_present(vga);
    if (fd2_field_game_play_actor_group_flash(
            &game, vga, field_audio, unit_indices, 1, 0xc0, 1) != 0)
        goto done;
    if (fd2_field_game_play_earthquake(
            &game, vga, field_audio, 1) != 0)
        goto done;
    if (fd2_field_game_play_stage_transition(
            &game, vga, field_audio, center_screen_x, center_screen_y,
            10, 8, 1) != 0)
        goto done;
    printf("field effects: stage=%zu, actor-flash=ok, earthquake=ok, "
           "stage-transition=ok, timed\n", stage);
    fflush(stdout);
    result = 0;

done:
    fd2_field_game_close(&game);
    return result;
}
