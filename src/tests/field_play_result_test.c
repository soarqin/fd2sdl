/* 仅测试构建替换正式 field_play 依赖，验证错误出口和移动取消，不启动窗口或原版游戏。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "field_game.h"
#include "field_play.h"
#include "field_rng.h"
#include "scene.h"
#include "test_temp.h"

/* 仅测试TU在wrapper中复用正式input消费，不把注入脉冲变成手工action队列。 */
#ifdef fd2_input_take_action
#undef fd2_input_take_action
int fd2_input_take_action(fd2_input *, fd2_input_context, fd2_input_action *);
#endif

#ifdef fd2_field_game_confirm_cursor
#undef fd2_field_game_confirm_cursor
int fd2_field_game_confirm_cursor(fd2_field_game *);
#endif

#undef fd2_field_game_animate_command
int fd2_field_game_animate_command(fd2_field_game *, fd2_vga *, int, uint32_t);
#undef fd2_field_game_close
void fd2_field_game_close(fd2_field_game *);

#undef fd2_field_game_animate_system_menu
int fd2_field_game_animate_system_menu(fd2_field_game *, fd2_vga *, int, uint32_t);
#undef fd2_field_game_execute_storage_action
int fd2_field_game_execute_storage_action(fd2_field_game *, fd2_field_system_action,
                                         const char *, size_t *);
#undef fd2_field_unit_summary_draw
int fd2_field_unit_summary_draw(fd2_vga *, const fd2_field_info_assets *,
    const fd2_font *, const fd2_text_entry *, const fd2_field_unit *, int);

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static int automatic_failure;
static int movement_failure;
static int action_sent;
static int scene_result;
static int failure_called;
static int cancellation_case;
static int cancellation_checked;
static fd2_input_key cancellation_key;
static fd2_field_game *active_game;
static fd2_field_unit unchanged_units[2];
static uint16_t unchanged_rng;
static int session_failure_case;
static int terminal_case;
static int auxiliary_checked;
static int leave_text_presented;
static int leave_state_checked;
static int leave_missing_resource;
static int march_case;
static int march_missing_resource;
static int march_called;
static int march_text_presented;
static int march_state_checked;
static int march_phase_called;
static int march_phase_input_checked;
static int march_phase_clean_parameter;
static int march_phase_outer_clean;
static int ordinary_automatic_clean;
static int controller_case;
static int controller_present_count;
static int controller_automatic_called;
static int controller_input_checked;
static int controller_sync_drained;
static uint64_t controller_repeat_at;
static int detail_resource_failure, detail_no_target;
static int detail_open_calls, detail_open_animation_calls;
static int detail_close_animation_calls, detail_close_calls;
static int item_case, item_frames, item_confirm_calls, item_context_bad;
static int item_menu_checked, item_invalid_checked, item_finished_checked;
static uint16_t item_initial_rng;
static int remainder_case, remainder_scene, remainder_automatic, remainder_checked;
static fd2_field_system_action storage_quit_action;
static int storage_quit_error, storage_calls, storage_closing_frames;
static int summary_case, summary_present, summary_draws, summary_confirms;
static int summary_dispatches, summary_initial_consumed, summary_checked;
static int summary_bad_owner;
static uint16_t summary_rng;

/* 关闭动画依赖替身只采样四帧；正式主循环和input消费保持真实。 */
static int closing_case, closing_frames, closing_openings, closing_confirms;
static int closing_outer_frames, closing_in_animation, closing_final_seen;
static fd2_field_interaction closing_final_interaction;
static uint8_t closing_final_x, closing_final_y;
static fd2_field_unit closing_before_units[3], closing_final_units[3];
static fd2_field_pending_cell_event closing_final_pending;
static uint16_t closing_before_rng, closing_final_rng;
static int closing_is_system(void) { return closing_case >= 6; }
static int closing_mode(void) { return (closing_case - 1) % 5; }

/* SYSTEM结果提示自己present的新帧，不能交给返回后的BROWSE。 */
static int result_frame_case, result_frame_samples, result_frame_outer;
static int result_frame_in_close, result_frame_fired, result_frame_openings;
static int result_frame_confirms, result_frame_browse_actions, result_frame_storage;
static int result_frame_storage_success, result_frame_final_seen;
static uint16_t result_frame_rng, result_frame_final_rng;
static uint8_t result_frame_final_x, result_frame_final_y;
static uint32_t result_frame_final_experience;
static fd2_field_interaction result_frame_final_interaction;
static fd2_field_unit result_frame_saved_units[2], result_frame_expected_units[2], result_frame_final_units[2];
static int result_frame_load(void) { return result_frame_case >= 5; }
static int result_frame_right(void) { return (result_frame_case - 1) & 1; }
static int result_frame_quit_now(void) { return ((result_frame_case - 1) & 2) != 0; }

static int is_leave_case(void) {
    return terminal_case == 3 || terminal_case == 7 || terminal_case == 8;
}

int __wrap_fd2_field_game_open(fd2_field_game *game, fd2_vga *vga,
                               const fd2_archive *fdother, size_t stage) {
    (void)vga;
    (void)fdother;
    memset(game, 0, sizeof(*game));
    if (result_frame_case) {
        fd2_input_init(&vga->input);
        game->ready = 1; game->stage = stage; game->active_side = 2; game->turn_number = 17;
        game->selected_unit = game->command_selected = game->detail_unit = game->attack_target = -1;
        game->map.width = game->map.height = 8;
        /* 复用纯snapshot codec路径，不假称此无archive夹具验证地图重载。 */
        game->sprites.frame_count = 140u * 12u;
        game->units.count = 2;
        memcpy(game->units.items, result_frame_saved_units, sizeof(result_frame_saved_units));
        game->units.items[0].hp = 30;
        game->cursor_cell_x = game->cursor_cell_y = 3;
        memset(game->metadata.turn_events, 0xff, sizeof(game->metadata.turn_events));
        fd2_field_system_menu_init(&game->system_menu);
        if (result_frame_load()) {
            fd2_field_system_menu_open_secondary(&game->system_menu);
            game->system_menu.selected = 2;
        } else game->system_menu.selected = 3;
        if (fd2_field_system_menu_confirm(&game->system_menu) != FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION)
            return -1;
        if (!result_frame_load())
            (void)fd2_field_system_menu_select_direction(&game->system_menu, FD2_FIELD_COMMAND_DIRECTION_RIGHT);
        game->interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
        const size_t fragment = result_frame_load() ? FD2_FIELD_SYSTEM_TEXT_LOAD_ACCEPTED : FD2_FIELD_SYSTEM_TEXT_CANCELLED;
        static const uint8_t tokens[] = {0,0,0xff,0xff};
        static const uint8_t glyph[32] = {0x80};
        game->ui_text.offsets = calloc(fragment + 1u, sizeof(*game->ui_text.offsets));
        if (!game->ui_text.offsets) return -1;
        game->ui_text.data = tokens; game->ui_text.size = sizeof(tokens);
        game->ui_text.fragment_count = fragment + 1u;
        game->font.glyphs = glyph; game->font.size = sizeof(glyph); game->font.glyph_count = 1;
        memcpy(result_frame_expected_units, result_frame_load() ? result_frame_saved_units : game->units.items,
            sizeof(result_frame_expected_units));
        active_game = game;
        return 0;
    }
    if (closing_case) {
        fd2_input_init(&vga->input);
        game->ready = 1; game->stage = stage; game->active_side = 2;
        game->turn_number = 17; game->selected_unit = closing_is_system() ? -1 : 0;
        game->detail_unit = game->attack_target = -1;
        game->interaction = closing_is_system() ? FD2_FIELD_INTERACTION_SYSTEM_MENU : FD2_FIELD_INTERACTION_COMMAND;
        game->command_selected = closing_mode() < 2 ? FD2_FIELD_COMMAND_ATTACK : FD2_FIELD_COMMAND_WAIT;
        game->map.width = game->map.height = 8;
        game->map.cells = calloc(64, sizeof(*game->map.cells));
        game->terrain.attrs = calloc(1, sizeof(*game->terrain.attrs));
        if (!game->map.cells || !game->terrain.attrs) return -1;
        game->terrain.attr_count = 1; game->units.count = 3;
        for (size_t i = 0; i < 3; i++) {
            fd2_field_unit *u = &game->units.items[i];
            if (fd2_field_unit_init(u, i == 1 ? 68 : 0, i == 1 ? 68 : 0,
                    i == 1 ? 0 : 2, i == 0 ? 1 : i == 1 ? 2 : 6,
                    i == 2 ? 6 : 1) != 0) return -1;
            u->hp = u->hp_max = 50; u->movement_profile = 1; u->movement_points = 4;
            uint8_t *bytes = (uint8_t *)u;
            for (size_t slot = 0; slot < 8; slot++) { bytes[10 + slot*2] = 0x80; bytes[11 + slot*2] = 0xff; }
            bytes[10] = 0x40; bytes[11] = 0; bytes[0x31] = 0xff;
        }
        game->units.items[0].hp = 30;
        game->cursor_cell_x = closing_is_system() ? 3 : 1;
        game->cursor_cell_y = closing_is_system() ? 3 : 1;
        game->resource_value = 12345;
        game->pending_cell_event = (fd2_field_pending_cell_event){0};
        memset(game->metadata.turn_events, 0xff, sizeof(game->metadata.turn_events));
        if (closing_is_system()) fd2_field_system_menu_init(&game->system_menu);
        active_game = game;
        memcpy(closing_before_units, game->units.items, sizeof(closing_before_units));
        return 0;
    }
    if (summary_case) {
        fd2_input_init(&vga->input);
        game->ready = 1; game->stage = stage; game->active_side = 2;
        game->selected_unit = game->command_selected = game->detail_unit = -1;
        game->map.width = game->map.height = 8;
        game->units.count = 2;
        game->units.items[0].x = game->cursor_cell_x = 1;
        game->units.items[0].y = game->cursor_cell_y = 2;
        game->units.items[0].side = 2; game->units.items[0].hp = 42;
        game->units.items[1].x = game->units.items[1].y = 6;
        game->units.items[1].side = 0; game->units.items[1].hp = 28;
        game->pending_cell_event = (fd2_field_pending_cell_event){1,9,4};
        active_game = game;
        memcpy(unchanged_units, game->units.items, sizeof(unchanged_units));
        return 0;
    }
    if (storage_quit_action) {
        fd2_input_init(&vga->input);
        game->ready = 1; game->stage = stage; game->active_side = 2;
        game->selected_unit = game->command_selected = game->detail_unit = -1;
        game->map.width = game->map.height = 24;
        game->sprites.frame_count = 140u * 12u; /* synthetic resource capacity */
        game->units.count = 2;
        game->units.items[0].side = 2; game->units.items[0].hp = 42;
        game->units.items[1].side = 0; game->units.items[1].hp = 28;
        game->resource_value = 12345; game->turn_number = 7;
        fd2_field_system_menu_init(&game->system_menu);
        game->system_menu.page = FD2_FIELD_SYSTEM_PAGE_CONFIRMATION;
        game->system_menu.confirmation_action = storage_quit_action;
        game->system_menu.confirmation_yes = 1;
        game->system_menu.confirmation_return_page = FD2_FIELD_SYSTEM_PAGE_SECONDARY;
        game->interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
        active_game = game;
        return 0;
    }
    if (remainder_case) {
        game->ready=1;game->stage=stage;game->active_side=0;game->turn_number=17;
        game->selected_unit=game->command_selected=game->detail_unit=-1;
        game->interaction=FD2_FIELD_INTERACTION_BROWSE;
        game->units.count=2;
        game->units.items[0].side=2;game->units.items[0].hp=1;
        game->units.items[1].side=0;game->units.items[1].hp=1;
        /* 在注册只读preflight之后，由剧情替身交回真实待续状态。 */
        game->event_log_count=1;game->event_log[0].handled=1;
        game->event_log[0].presentation_deferred=1;
        active_game=game;
        return 0;
    }
    if (item_case) {
        fd2_input_init(&vga->input);
        game->ready = 1; game->stage = stage; game->active_side = 2;
        game->turn_number = 17; game->selected_unit = 0;
        game->detail_unit = -1; game->attack_target = -1;
        game->interaction = FD2_FIELD_INTERACTION_COMMAND;
        game->command_selected = FD2_FIELD_COMMAND_ITEM;
        game->command_assets.ready = item_case != 3;
        game->map.width = game->map.height = 8;
        game->map.cells = calloc(64, sizeof(*game->map.cells));
        game->terrain.attrs = calloc(1, sizeof(*game->terrain.attrs));
        if (!game->map.cells || !game->terrain.attrs) return -1;
        game->terrain.attr_count = 1;
        game->units.count = 3;
        for (size_t i = 0; i < game->units.count; i++) {
            game->units.items[i].hp = game->units.items[i].hp_max = 42;
            game->units.items[i].side = i == 1 ? 0 : 2;
            game->units.items[i].x = game->units.items[i].y = (uint8_t)(1 + i * 2);
            ((uint8_t *)&game->units.items[i])[0x31] = 0xff;
        }
        game->units.items[0].hp = 20;
        game->units.items[0].experience = 99;
        game->units.items[0].direction = 3;
        const uint8_t inventory[] = {0x40,0, 0x40,132, 0,192,
                                    0x80,0, 0x80,0, 0x80,0, 0x80,0, 0x80,201};
        memcpy((uint8_t *)&game->units.items[0] + 0x0a, inventory, sizeof(inventory));
        game->cursor_cell_x = game->cursor_cell_y = 1;
        game->move_origin_x = game->move_origin_y = 0;
        memset(game->metadata.turn_events, 0xff, sizeof(game->metadata.turn_events));
        memset(game->metadata.cell_lookup, 0xff, sizeof(game->metadata.cell_lookup));
        if (item_case == 2) {
            game->interaction = FD2_FIELD_INTERACTION_ITEM_LIST;
            game->item_list_selected = 2;
            game->command_selected = 0; /* Use不是顶层ATTACK。 */
            if (fd2_field_game_begin_item_targeting(game) != 0) return -1;
        } else if (item_case == 4) {
            for (unsigned slot = 0; slot < 8; slot++) {
                ((uint8_t *)&game->units.items[0])[0x0a + slot*2] = 0x80;
                ((uint8_t *)&game->units.items[2])[0x0a + slot*2] = 0x80;
            }
            ((uint8_t *)&game->units.items[0])[0x0a] = 0;
            ((uint8_t *)&game->units.items[0])[0x0b] = 192;
            game->units.items[2].x = 2; game->units.items[2].y = 1;
            game->interaction = FD2_FIELD_INTERACTION_ITEM_LIST;
            game->command_selected = 1;
            if (fd2_field_game_begin_item_give(game) != 0) return -1;
        } else game->pending_cell_event = (fd2_field_pending_cell_event){1,9,4};
        active_game = game;
        memcpy(unchanged_units, game->units.items, sizeof(unchanged_units));
        return 0;
    }
    if (controller_case) {
        fd2_input_init(&vga->input);
        game->ready = 1;
        game->turn_number = 17;
        game->active_side = 2;
        game->selected_unit = 0;
        game->command_selected = FD2_FIELD_COMMAND_WAIT;
        game->attack_target = -1;
        game->detail_unit = -1;
        game->interaction = FD2_FIELD_INTERACTION_COMMAND;
        game->map.width = game->map.height = 8;
        game->map.cells = calloc(64, sizeof(*game->map.cells));
        game->terrain.attrs = calloc(1, sizeof(*game->terrain.attrs));
        if (!game->map.cells || !game->terrain.attrs) return -1;
        game->terrain.attr_count = 1;
        game->units.count = 3;
        for (size_t i = 0; i < game->units.count; i++) {
            game->units.items[i].hp = game->units.items[i].hp_max = 50;
            game->units.items[i].side = i == 1 ? 0 : 2;
            game->units.items[i].x = game->units.items[i].y = (uint8_t)(1 + i * 2);
        }
        game->units.items[0].hp = 30;
        if (controller_case != 1) game->units.items[2].flags = 0x80;
        if (controller_case >= 3) {
            game->units.items[0].detail_status[1] = 1;
            game->interaction = FD2_FIELD_INTERACTION_BROWSE;
            game->selected_unit = -1;
        }
        game->cursor_cell_x = game->cursor_cell_y = 1;
        if (detail_no_target == 1)
            game->cursor_cell_x = game->cursor_cell_y = 2;
        else if (detail_no_target == 2) {
            game->units.items[2].hp = 0;
            game->cursor_cell_x = game->cursor_cell_y = 5;
        }
        memset(game->metadata.turn_events, 0xff, sizeof(game->metadata.turn_events));
        active_game = game;
        return 0;
    }
    game->ready = 1;
    game->stage = stage;
    game->selected_unit = -1;
    game->units.count = 2;
    game->units.items[0].side = 2;
    game->units.items[0].hp = 1;
    game->units.items[1].side = 0;
    game->units.items[1].hp = 1;
    game->active_side = automatic_failure ? 0 : 2;
    game->phase_next_action_ms = 1;
    if (terminal_case == 1 || terminal_case == 5) game->units.items[1].hp = 0;
    if (terminal_case == 2) game->units.items[0].hp = 0;
    if (is_leave_case() || terminal_case == 6 || march_case) {
        fd2_field_system_menu_init(&game->system_menu);
        game->system_menu.page = FD2_FIELD_SYSTEM_PAGE_CONFIRMATION;
        game->system_menu.confirmation_action = is_leave_case()
            ? FD2_FIELD_SYSTEM_ACTION_LEAVE_BATTLE
            : march_case ? FD2_FIELD_SYSTEM_ACTION_MARCH_CONFIRM
                         : FD2_FIELD_SYSTEM_ACTION_END_TURN_CONFIRM;
        game->system_menu.confirmation_return_page = is_leave_case()
            ? FD2_FIELD_SYSTEM_PAGE_SECONDARY : FD2_FIELD_SYSTEM_PAGE_PARENT;
        game->system_menu.confirmation_return_selected = 3;
        game->system_menu.confirmation_yes = 1;
        game->interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
    }
    if (is_leave_case() || march_case) {
        const size_t accepted_fragment = march_case ? FD2_FIELD_SYSTEM_TEXT_MARCH_ACCEPTED
                                                  : FD2_FIELD_SYSTEM_TEXT_LEAVE_ACCEPTED;
        /* 只有 FDTXT[416] 非空；一个测试字形的下左阴影用于识别
         * 真正呈现，不依赖生产日志或额外测试钩子。offsets 归 session。 */
        static const uint8_t text[] = {0, 0, 0xff, 0xff};
        static const uint8_t glyph[32] = {0x80};
        game->ui_text.offsets = calloc(
            accepted_fragment + 1u, sizeof(*game->ui_text.offsets));
        if (!game->ui_text.offsets) return -1;
        game->ui_text.data = text;
        game->ui_text.size = sizeof(text);
        game->ui_text.fragment_count = accepted_fragment + 1u;
        game->font.glyphs = glyph;
        game->font.size = sizeof(glyph);
        game->font.glyph_count = 1;
        if (terminal_case == 7) {
            if (leave_missing_resource == 0) game->ui_text.data = NULL;
            if (leave_missing_resource == 1) game->font.glyphs = NULL;
            if (leave_missing_resource == 2)
                game->ui_text.fragment_count =
                    FD2_FIELD_SYSTEM_TEXT_LEAVE_ACCEPTED;
        }
        if (march_case == 1) {
            if (march_missing_resource == 0) game->ui_text.data = NULL;
            if (march_missing_resource == 1) game->font.glyphs = NULL;
            if (march_missing_resource == 2) game->ui_text.fragment_count = accepted_fragment;
        }
        if (march_case == 6) {
            game->map.width = game->map.height = 8;
            game->map.cells = calloc(64, sizeof(*game->map.cells));
            game->terrain.attrs = calloc(1, sizeof(*game->terrain.attrs));
            if (!game->map.cells || !game->terrain.attrs) return -1;
            game->terrain.attr_count = 1;
            game->cursor_cell_x = 1; game->cursor_cell_y = 6;
        }
        game->turn_number = 17;
        active_game = game;
        memcpy(unchanged_units, game->units.items, sizeof(unchanged_units));
        memset(vga->framebuffer, 0, sizeof(vga->framebuffer));
    }
    if (terminal_case == 6) {
        /* 缺少 ui_text/font 时不得越过 FDTXT[420] 呈现边界。空事件表
         * 保证错误地推进 phase 不会先被其他 handler 失败掩盖。 */
        game->turn_number = 17;
        memset(game->metadata.turn_events, 0xff,
               sizeof(game->metadata.turn_events));
    }
    if (terminal_case == 4) active_game = game;
    if (session_failure_case == 1) {
        game->failure = FD2_FIELD_FAILURE_EVENT_CAPACITY;
        game->units.items[0].hp = 0; /* 同时出现结果门，也必须先报告错误。 */
    }
    if (cancellation_case) {
        active_game = game;
        game->interaction = FD2_FIELD_INTERACTION_UNIT_SELECTED;
        game->selected_unit = 0;
        game->units.items[0].x = 1;
        game->units.items[0].y = 2;
        game->move_origin_x = 1;
        game->move_origin_y = 2;
        game->cursor_cell_x = 2;
        game->cursor_cell_y = 3;
        game->turn_number = 17;
    }
    if (!automatic_failure && !movement_failure && !cancellation_case) {
        game->event_log_count = 1;
        game->event_log[0].handled = session_failure_case != 3;
        game->event_log[0].presentation_deferred = 1;
    }
    if (session_failure_case == 2) game->event_log_count = 0;
    if (terminal_case >= 3 || march_case) game->event_log_count = 0;
    return 0;
}

int __wrap_fd2_field_game_process_automatic_action_visual(
        fd2_field_game *game, fd2_vga *vga, uint32_t frame_delay_ms,
        int clean_background) {
    (void)game;
    (void)vga;
    (void)frame_delay_ms;
    if (remainder_case) {
        remainder_automatic++;
        remainder_checked = game->loot.source == 3u &&
            game->loot.notice.kind == FD2_FIELD_LOOT_NONE &&
            game->phase_next_action_ms == 0 &&
            fd2_field_game_battle_result(game) == FD2_FIELD_BATTLE_DEFEAT &&
            !fd2_field_unit_has_acted(&game->units.items[1]);
        /* 3仅为本TU的实际AI11 remainder fixture，不新增生产枚举。 */
        memset(&game->loot,0,sizeof(game->loot));
        fd2_field_unit_set_acted(&game->units.items[1],1);
        return 1;
    }
    if (march_case == 6) {
        march_phase_clean_parameter = clean_background;
        march_phase_called++;
        game->active_side = 2;
        game->turn_number++;
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
        fd2_input_begin_test_frame(&vga->input, 1000);
        return 1;
    }
    if (controller_case == 2 || controller_case == 3) {
        controller_automatic_called++;
        game->active_side = 2;
        game->turn_number++;
        /* 模拟phase最后一帧产生普通键；真实resume不保存/恢复pending。 */
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
        fd2_input_begin_test_frame(&vga->input, 1000);
        return fd2_field_game_resume_player_controller(game) < 0 ? -1 : 1;
    }
    ordinary_automatic_clean = clean_background;
    failure_called++;
    return -1;
}

int __wrap_fd2_scene_play_field_event(fd2_vga *vga,
        const fd2_archive *fdother, fd2_field_game *game,
        fd2_field_event_notice *notice, fd2_field_audio *audio, int fast) {
    (void)vga;
    (void)fdother;
    (void)game;
    (void)notice;
    (void)audio;
    (void)fast;
    if (remainder_case) {
        remainder_scene++;
        game->units.items[0].hp=0;game->units.items[0].flags=1;
        game->loot.source=3;game->loot.notice.kind=FD2_FIELD_LOOT_NONE;
        game->phase_next_action_ms=0; /* core tick被busy门挡住，不会补timer。 */
        return FD2_SCENE_RESULT_OK;
    }
    failure_called++;
    return scene_result;
}

void __wrap_fd2_vga_present(fd2_vga *vga) {
    if (result_frame_case) {
        for (int key = 1; key < SDL_SCANCODE_COUNT; key++)
            fd2_input_set_test_key_state(&vga->input, (SDL_Scancode)key, 0);
        ++result_frame_samples;
        if (!result_frame_in_close) {
            if (++result_frame_outer == 1) {
                result_frame_rng = ((fd2_field_rng *)active_game->attack_rng_userdata)->state;
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
            } else if (!result_frame_fired && active_game->interaction == FD2_FIELD_INTERACTION_BROWSE &&
                       vga->framebuffer[120u * VGA_STRIDE + 14u] == 0x4c) {
                result_frame_fired = 1; /* 真正结果文字present，而非closing帧。 */
                fd2_input_set_test_key_state(&vga->input,
                    result_frame_right() ? SDL_SCANCODE_RIGHT : SDL_SCANCODE_RETURN, 1);
                if (result_frame_quit_now()) vga->input.quit_requested = 1;
            } else vga->input.quit_requested = 1; /* 无quit向量在下一个外帧退出。 */
        }
        fd2_input_begin_test_frame(&vga->input, (uint64_t)result_frame_samples * 100u);
        return;
    }
    if (closing_case) {
        for (int key = 1; key < SDL_SCANCODE_COUNT; key++)
            fd2_input_set_test_key_state(&vga->input, (SDL_Scancode)key, 0);
        if (closing_in_animation) {
            if (++closing_frames == 4) {
                int mode = closing_mode();
                if (mode < 2) fd2_input_set_test_key_state(&vga->input,
                    mode == 0 ? SDL_SCANCODE_RIGHT : SDL_SCANCODE_RETURN, 1);
                if (mode == 2 || mode == 4) vga->input.quit_requested = 1;
            }
        } else if (++closing_outer_frames == 1) {
            closing_before_rng = ((fd2_field_rng *)active_game->attack_rng_userdata)->state;
            fd2_input_set_test_key_state(&vga->input,
                closing_is_system() ? SDL_SCANCODE_ESCAPE : SDL_SCANCODE_RETURN, 1);
        } else vga->input.quit_requested = 1;
        fd2_input_begin_test_frame(&vga->input,
            (uint64_t)(closing_frames + closing_outer_frames) * 100u);
        return;
    }
    if (summary_case) {
        ++summary_present;
        for (int key = 1; key < SDL_SCANCODE_COUNT; key++)
            fd2_input_set_test_key_state(&vga->input, (SDL_Scancode)key, 0);
        if (summary_present == 1) {
            summary_rng = ((fd2_field_rng *)active_game->attack_rng_userdata)->state;
            fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
            if (summary_case == 5)
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
        } else if (summary_present == 3 && summary_case != 5) {
            fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_LSHIFT, 1);
            if (summary_case == 4) vga->input.quit_requested = 1;
        } else if (summary_present == 4 && summary_case != 5) {
            fd2_input_set_test_key_state(&vga->input,
                summary_case == 2 ? SDL_SCANCODE_A : SDL_SCANCODE_RIGHT, 1);
        }
        if (summary_present == (summary_case == 5 ? 2 : 5)) {
            summary_checked = active_game->interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED &&
                active_game->selected_unit == 0 && summary_confirms == 1 &&
                active_game->cursor_cell_x == (summary_case == 2 ? 1 : 2) &&
                active_game->cursor_cell_y == 2 && summary_dispatches == 1 &&
                !memcmp(unchanged_units, active_game->units.items, sizeof(unchanged_units)) &&
                ((fd2_field_rng *)active_game->attack_rng_userdata)->state == summary_rng &&
                active_game->pending_cell_event.valid &&
                active_game->pending_cell_event.action == 9 &&
                active_game->pending_cell_event.source_slot == 4;
            vga->input.quit_requested = 1;
        }
        fd2_input_begin_test_frame(&vga->input, (uint64_t)summary_present * 100u);
        if (summary_present > 8) vga->input.quit_requested = 1;
        return;
    }
    if (storage_quit_action) {
        if (active_game->system_menu.animation_phase) {
            storage_closing_frames++;
            if (storage_closing_frames == 1) {
                vga->input.quit_requested = 1;
                if (storage_quit_error) active_game->failure = FD2_FIELD_FAILURE_EVENT_DATA;
            }
        }
        return;
    }
    if (remainder_case) {
        if (++action_sent > 5) vga->input.quit_requested=1;
        return;
    }
    if (item_case) {
        item_frames++;
        for (int key = 1; key < SDL_SCANCODE_COUNT; key++)
            fd2_input_set_test_key_state(&vga->input, (SDL_Scancode)key, 0);
        if (item_frames == 1)
            item_initial_rng = ((fd2_field_rng *)active_game->attack_rng_userdata)->state;
        if (item_case == 4) {
            if (item_frames == 1) {
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_ESCAPE, 1);
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_SPACE, 1);
            }
            if (item_frames == 2) {
                item_menu_checked = active_game->interaction == FD2_FIELD_INTERACTION_COMMAND &&
                    active_game->command_selected == FD2_FIELD_COMMAND_WAIT &&
                    active_game->item_transfer_committed && !fd2_field_unit_has_acted(&active_game->units.items[0]) &&
                    !fd2_field_unit_inventory_count(&active_game->units.items[0]) &&
                    fd2_field_unit_inventory_count(&active_game->units.items[2]) == 1 &&
                    active_game->cursor_cell_x == 1 && active_game->cursor_cell_y == 1 &&
                    !fd2_input_has_any_key(&vga->input);
            }
            if (item_frames == 3) {
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_ESCAPE, 1);
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
            }
            if (item_frames == 4) {
                item_finished_checked = active_game->interaction == FD2_FIELD_INTERACTION_BROWSE &&
                    active_game->selected_unit == -1 && active_game->item_transfer_committed &&
                    active_game->units.items[0].x == 1 && active_game->units.items[0].y == 1 &&
                    active_game->units.items[0].hp == 20 && active_game->units.items[0].experience == 99 &&
                    active_game->units.items[0].direction == 3 && fd2_field_unit_has_acted(&active_game->units.items[0]) &&
                    ((fd2_field_rng *)active_game->attack_rng_userdata)->state == item_initial_rng &&
                    !fd2_input_has_any_key(&vga->input);
                vga->input.quit_requested = 1;
            }
        } else if (item_case == 2) {
            if (item_frames == 1) fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
            if (item_frames == 2) {
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_SPACE, 1);
            }
            if (item_frames == 3) {
                item_invalid_checked = active_game->interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING &&
                    active_game->cursor_cell_x == 2 && active_game->cursor_cell_y == 1 &&
                    active_game->units.items[0].hp == 20 && !fd2_field_unit_has_acted(&active_game->units.items[0]) &&
                    item_confirm_calls == 2 && ((fd2_field_rng *)active_game->attack_rng_userdata)->state == item_initial_rng;
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_Z, 1);
            }
            if (item_frames == 4) {
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
            }
            if (item_frames == 5) {
                item_finished_checked = active_game->interaction == FD2_FIELD_INTERACTION_BROWSE &&
                    active_game->selected_unit == -1 && active_game->cursor_cell_x == 1 && active_game->cursor_cell_y == 1 &&
                    active_game->units.items[0].hp == 42 && active_game->units.items[0].experience == 99 &&
                    active_game->units.items[0].mp == 0 && active_game->units.items[0].direction == 3 &&
                    fd2_field_unit_has_acted(&active_game->units.items[0]) &&
                    fd2_field_unit_inventory_count(&active_game->units.items[0]) == 2 && item_confirm_calls == 3 &&
                    !fd2_input_has_any_key(&vga->input);
                vga->input.quit_requested = 1;
            }
        } else {
            if (item_frames == 1) {
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_ESCAPE, 1);
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
            }
            if (item_frames == 2)
                item_menu_checked = active_game->interaction == FD2_FIELD_INTERACTION_ITEM_MENU &&
                    active_game->command_selected == 0 && item_confirm_calls == 1 && !fd2_input_has_any_key(&vga->input);
            if (item_frames == 3) {
                for (int key = SDL_SCANCODE_RIGHT; key <= SDL_SCANCODE_UP; key++)
                    fd2_input_set_test_key_state(&vga->input, (SDL_Scancode)key, 1);
            }
            if (item_frames == 4) {
                item_menu_checked = item_menu_checked && active_game->interaction == FD2_FIELD_INTERACTION_ITEM_MENU &&
                    active_game->command_selected == 0 && active_game->cursor_cell_x == 1 && active_game->cursor_cell_y == 1;
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_ESCAPE, 1);
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_SPACE, 1);
            }
            if (item_frames == 5) {
                item_finished_checked = active_game->interaction == FD2_FIELD_INTERACTION_COMMAND &&
                    active_game->command_selected == FD2_FIELD_COMMAND_ITEM && item_confirm_calls == 1 &&
                    active_game->cursor_cell_x == 1 && active_game->cursor_cell_y == 1 &&
                    active_game->pending_cell_event.valid && active_game->pending_cell_event.action == 9 &&
                    !memcmp(unchanged_units, active_game->units.items, sizeof(unchanged_units)) &&
                    !fd2_input_has_any_key(&vga->input);
                vga->input.quit_requested = 1;
            }
        }
        fd2_input_begin_test_frame(&vga->input, (uint64_t)item_frames * 100u);
        if (item_frames > 8) vga->input.quit_requested = 1;
        return;
    }
    if (march_case && active_game && active_game->last_system_action ==
            FD2_FIELD_SYSTEM_ACTION_MARCH_CONFIRM) {
        if (!march_called && vga->framebuffer[120u * VGA_STRIDE + 14u] == 0x4c) {
            march_text_presented++;
            if (march_case == 2) vga->input.quit_requested = 1;
        } else if (march_called && march_case == 3) {
            vga->input.quit_requested = 1;
        } else if (march_called && march_case == 6 && !march_phase_called) {
            march_phase_outer_clean = active_game->info_panel_right == 0;
        } else if (march_called && march_case == 6 && march_phase_called) {
            march_phase_input_checked = !fd2_input_has_any_key(&vga->input) &&
                active_game->active_side == 2 && active_game->turn_number == 18 &&
                active_game->info_panel_right == 1 &&
                vga->input.previous_down[SDL_SCANCODE_RIGHT] &&
                vga->input.next_repeat_ms[SDL_SCANCODE_RIGHT] > 1000;
            vga->input.quit_requested = 1;
        }
    }
    if (controller_case) {
        controller_present_count++;
        if (controller_case >= 3) {
            if (controller_present_count == 1) {
                fd2_input_set_test_key_state(&vga->input,
                    controller_case == 3 ? SDL_SCANCODE_RETURN : SDL_SCANCODE_F2, 1);
                fd2_input_begin_test_frame(&vga->input, 100);
            } else if (controller_present_count == 2) {
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 0);
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_F2, 0);
                fd2_input_begin_test_frame(&vga->input, 101);
                if (detail_no_target) vga->input.quit_requested = 1;
            } else if (controller_present_count == 3) {
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
                if (controller_case == 3)
                    fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
                fd2_input_begin_test_frame(&vga->input, 102);
            } else if (controller_present_count == 4) {
                controller_sync_drained = !fd2_input_has_any_key(&vga->input) &&
                    !active_game->detail_visible && active_game->selected_unit == -1 &&
                    !fd2_field_unit_has_acted(&active_game->units.items[0]) &&
                    active_game->cursor_cell_x == 1 && active_game->cursor_cell_y == 1;
                if (controller_case == 4) {
                    controller_input_checked = controller_sync_drained &&
                        !active_game->player_post_waiting_phase &&
                        active_game->active_side == 2 && active_game->turn_number == 17;
                    vga->input.quit_requested = 1;
                }
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 0);
                fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 0);
                fd2_input_begin_test_frame(&vga->input, 103);
            } else if (controller_automatic_called) {
                controller_input_checked = controller_sync_drained &&
                    !fd2_input_has_any_key(&vga->input) &&
                    !active_game->player_post_waiting_phase &&
                    active_game->active_side == 2 && active_game->turn_number == 18 &&
                    !fd2_field_unit_has_acted(&active_game->units.items[0]);
                vga->input.quit_requested = 1;
            }
            if (controller_present_count > 40) vga->input.quit_requested = 1;
            return;
        }
        if (controller_present_count == 1) {
            fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
            fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
            fd2_input_begin_test_frame(&vga->input, 100);
            controller_repeat_at = vga->input.next_repeat_ms[SDL_SCANCODE_RETURN];
        } else if (!controller_sync_drained) {
            controller_sync_drained = !fd2_input_has_any_key(&vga->input) &&
                vga->input.previous_down[SDL_SCANCODE_RETURN] &&
                vga->input.next_repeat_ms[SDL_SCANCODE_RETURN] == controller_repeat_at &&
                active_game->cursor_cell_x == 1 && active_game->cursor_cell_y == 1;
            fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 0);
            fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 0);
            fd2_input_begin_test_frame(&vga->input, 101);
            if (controller_case == 1) {
                controller_input_checked = controller_sync_drained &&
                    !active_game->player_post_waiting_phase;
                vga->input.quit_requested = 1;
            }
        } else if (controller_automatic_called) {
            controller_input_checked = !fd2_input_has_any_key(&vga->input) &&
                !active_game->player_post_waiting_phase &&
                active_game->active_side == 2 && active_game->turn_number == 18 &&
                active_game->cursor_cell_x == 1 && active_game->cursor_cell_y == 1 &&
                vga->input.previous_down[SDL_SCANCODE_RIGHT] &&
                vga->input.next_repeat_ms[SDL_SCANCODE_RIGHT] > 1000;
            vga->input.quit_requested = 1;
        }
        if (controller_present_count > 40) vga->input.quit_requested = 1;
        return;
    }
    if (is_leave_case() && active_game &&
        active_game->last_system_action ==
            FD2_FIELD_SYSTEM_ACTION_LEAVE_BATTLE &&
        vga->framebuffer[120u * VGA_STRIDE + 14u] == 0x4c) {
        ++leave_text_presented;
        leave_state_checked = active_game->turn_number == 17 &&
            active_game->active_side == 2 &&
            active_game->phase_next_action_ms == 1 &&
            active_game->event_log_count == 0 &&
            active_game->event_total_count == 0 &&
            memcmp(unchanged_units, active_game->units.items,
                   sizeof(unchanged_units)) == 0;
        if (terminal_case == 8) vga->input.quit_requested = 1;
    }
}

static int closing_animation(fd2_vga *vga, int opening) {
    if (opening) { closing_openings++; return 0; }
    closing_in_animation = 1;
    for (int phase = 0; phase < 4; phase++) __wrap_fd2_vga_present(vga);
    closing_in_animation = 0;
    return closing_mode() >= 3 ? -1 : 0;
}
int __wrap_fd2_field_game_animate_command(fd2_field_game *game,
        fd2_vga *vga, int opening, uint32_t delay) {
    if (closing_case) return closing_animation(vga, opening);
    /* 旧controller夹具只测后置输入，未提供菜单资源；关闭错误由上面的专门向量覆盖。 */
    if (controller_case) return 0;
    return fd2_field_game_animate_command(game, vga, opening, delay);
}
void __wrap_fd2_field_game_close(fd2_field_game *game) {
    if (result_frame_case) {
        ++result_frame_final_seen;
        result_frame_final_interaction = game->interaction;
        result_frame_final_x = game->cursor_cell_x; result_frame_final_y = game->cursor_cell_y;
        result_frame_final_experience = game->experience_value;
        result_frame_final_rng = ((fd2_field_rng *)game->attack_rng_userdata)->state;
        memcpy(result_frame_final_units, game->units.items, sizeof(result_frame_final_units));
    }
    if (closing_case) {
        closing_final_seen++;
        closing_final_interaction = game->interaction;
        closing_final_x = game->cursor_cell_x; closing_final_y = game->cursor_cell_y;
        closing_final_pending = game->pending_cell_event;
        memcpy(closing_final_units, game->units.items, sizeof(closing_final_units));
        closing_final_rng = ((fd2_field_rng *)game->attack_rng_userdata)->state;
    }
    fd2_field_game_close(game);
}

int __wrap_fd2_field_game_animate_system_menu(fd2_field_game *game,
        fd2_vga *vga, int opening, uint32_t frame_delay_ms) {
    if (result_frame_case) {
        if (opening) { ++result_frame_openings; return 0; }
        result_frame_in_close = 1;
        for (unsigned phase = 0; phase < 4; phase++) __wrap_fd2_vga_present(vga);
        result_frame_in_close = 0;
        return 0;
    }
    if (closing_case) return closing_animation(vga, opening);
    /* 旧业务提示夹具不提供菜单资源，显式成功替身不再依赖生产忽略-1。 */
    if (is_leave_case() || terminal_case == 6 || march_case) return 0;
    if (!storage_quit_action)
        return fd2_field_game_animate_system_menu(game, vga, opening, frame_delay_ms);
    if (opening) return -1;
    /* 只替该测试的present：第一个closing帧收到quit，动画仍完整返回。 */
    for (uint8_t phase = 1; phase <= 4; phase++) {
        game->system_menu.animation_phase = phase;
        __wrap_fd2_vga_present(vga);
    }
    game->system_menu.animation_phase = 0;
    return 0;
}

int __wrap_fd2_field_game_execute_storage_action(fd2_field_game *game,
        fd2_field_system_action action, const char *path, size_t *fragment) {
    if (result_frame_case) {
        ++result_frame_storage;
        result_frame_storage_success = fd2_field_game_execute_storage_action(game, action, path, fragment);
        return result_frame_storage_success;
    }
    if (storage_quit_action) storage_calls++;
    return fd2_field_game_execute_storage_action(game, action, path, fragment);
}

int __wrap_fd2_input_take_action(fd2_input *input, fd2_input_context context,
                                 fd2_input_action *action) {
    if (result_frame_case) {
        int result = fd2_input_take_action(input, context, action);
        if (result && active_game->interaction == FD2_FIELD_INTERACTION_BROWSE)
            ++result_frame_browse_actions;
        return result;
    }
    if (closing_case) return fd2_input_take_action(input, context, action);
    if (summary_case) {
        int result = fd2_input_take_action(input, context, action);
        if (result && context == FD2_INPUT_CONTEXT_FIELD_MOVEMENT) {
            summary_dispatches++;
            if (*action != (summary_case == 2 ? FD2_INPUT_ACTION_NONE : FD2_INPUT_ACTION_RIGHT))
                summary_bad_owner = 1;
        }
        return result;
    }
    if (controller_case || item_case) {
        if (item_case && active_game &&
            ((active_game->interaction == FD2_FIELD_INTERACTION_ITEM_MENU && context != FD2_INPUT_CONTEXT_FIELD_COMMAND) ||
             ((active_game->interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING ||
               active_game->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING ||
               active_game->interaction == FD2_FIELD_INTERACTION_ITEM_TELEPORTING) && context != FD2_INPUT_CONTEXT_FIELD_MAGIC_TARGETING)))
            item_context_bad = 1;
        return fd2_input_take_action(input, context, action);
    }
    (void)input;
    if (storage_quit_action && action_sent++ == 0) {
        *action = FD2_INPUT_ACTION_CONFIRM;
        return 1;
    }
    if ((is_leave_case() || terminal_case == 6 || march_case) && action_sent++ == 0) {
        *action = FD2_INPUT_ACTION_CONFIRM;
        return 1;
    }
    if (terminal_case == 4) {
        if (action_sent++ == 0) {
            *action = FD2_INPUT_ACTION_FIELD_AUXILIARY;
            return 1;
        }
        auxiliary_checked = active_game->interaction == FD2_FIELD_INTERACTION_BROWSE;
        *action = FD2_INPUT_ACTION_EXIT;
        return 1;
    }
    if (session_failure_case == 2 && action_sent == 0) {
        action_sent = 1;
        *action = FD2_INPUT_ACTION_CONFIRM;
        return 1;
    }
    if (session_failure_case == 4) {
        if (action_sent++ == 0) return 0;
        *action = FD2_INPUT_ACTION_EXIT;
        return 1;
    }
    if (cancellation_case) {
        if (action_sent == 0) {
            memcpy(unchanged_units, active_game->units.items, sizeof(unchanged_units));
            unchanged_rng = ((fd2_field_rng *)active_game->attack_rng_userdata)->state;
            action_sent = 1;
            *action = fd2_input_action_for_key(context, cancellation_key);
            return 1;
        }
        if (action_sent == 1) {
            cancellation_checked = context == FD2_INPUT_CONTEXT_FIELD &&
                active_game->interaction == FD2_FIELD_INTERACTION_BROWSE &&
                active_game->selected_unit == -1 && active_game->turn_number == 17 &&
                active_game->active_side == 2 && active_game->phase_next_action_ms == 1 &&
                active_game->cursor_cell_x == 1 && active_game->cursor_cell_y == 2 &&
                active_game->event_log_count == 0 && active_game->event_total_count == 0 &&
                memcmp(unchanged_units, active_game->units.items, sizeof(unchanged_units)) == 0 &&
                ((fd2_field_rng *)active_game->attack_rng_userdata)->state == unchanged_rng;
            action_sent = 2;
            *action = FD2_INPUT_ACTION_EXIT;
            return 1;
        }
        return 0;
    }
    if (!movement_failure || action_sent) return 0;
    action_sent = 1;
    *action = FD2_INPUT_ACTION_CONFIRM;
    return 1;
}

int __wrap_fd2_field_game_confirm_cursor(fd2_field_game *game) {
    if (result_frame_case) {
        ++result_frame_confirms;
        return fd2_field_game_confirm_cursor(game); /* 暴露真实controller清XP，不伪造结果。 */
    }
    if (closing_case) {
        closing_confirms++;
        /* 意外第二次target确认只记录分派，不运行图形战斗依赖。 */
        if (game->interaction == FD2_FIELD_INTERACTION_TARGETING) return -1;
        return fd2_field_game_confirm_cursor(game);
    }
    if (summary_case) {
        summary_confirms++;
        if (game->interaction != FD2_FIELD_INTERACTION_BROWSE) summary_bad_owner = 1;
        game->interaction = FD2_FIELD_INTERACTION_UNIT_SELECTED;
        game->selected_unit = 0;
        return 0;
    }
    if (item_case) {
        item_confirm_calls++;
        return fd2_field_game_confirm_cursor(game);
    }
    if (controller_case) return fd2_field_game_confirm_command(game);
    if (session_failure_case == 2) {
        game->failure = FD2_FIELD_FAILURE_EVENT_UNSUPPORTED;
        return -1;
    }
    game->interaction = FD2_FIELD_INTERACTION_MOVING;
    return 0;
}

int __wrap_fd2_field_game_execute_move(fd2_field_game *game, fd2_vga *vga,
                                      uint32_t frame_delay_ms) {
    (void)game;
    (void)vga;
    (void)frame_delay_ms;
    failure_called++;
    return -1;
}

int __wrap_fd2_field_unit_summary_draw(fd2_vga *vga,
        const fd2_field_info_assets *assets, const fd2_font *font,
        const fd2_text_entry *text, const fd2_field_unit *unit, int relative_x) {
    if (!summary_case)
        return fd2_field_unit_summary_draw(vga, assets, font, text, unit, relative_x);
    ++summary_draws;
    if (active_game->interaction != FD2_FIELD_INTERACTION_UNIT_SELECTED ||
        active_game->selected_unit != 0 || unit != &active_game->units.items[0] ||
        assets != &active_game->info_assets || font != &active_game->font ||
        text != &active_game->ui_text || relative_x != 1 || fd2_input_has_any_key(&vga->input))
        summary_bad_owner = 1;
    if (summary_draws == 1) summary_initial_consumed = vga->input.frame_consumed[SDL_SCANCODE_RETURN];
    return summary_case == 3 ? -1 : 0; /* 实际renderer另由field_info资源回归覆盖。 */
}


int __wrap_fd2_field_game_open_detail(fd2_field_game *game, size_t actor) {
    detail_open_calls++;
    if (detail_resource_failure == 1) return -1;
    game->detail_visible = 1;
    game->detail_unit = (int)actor;
    return 0;
}
void __wrap_fd2_field_game_close_detail(fd2_field_game *game) {
    detail_close_calls++;
    game->detail_visible = 0;
    game->detail_unit = -1;
}
int __wrap_fd2_field_game_animate_detail(fd2_field_game *game, fd2_vga *vga,
        int opening, fd2_field_detail_phase_fn callback, void *userdata) {
    (void)game; (void)vga; (void)callback; (void)userdata;
    if (opening) detail_open_animation_calls++;
    else detail_close_animation_calls++;
    if ((opening && detail_resource_failure == 2) ||
        (!opening && detail_resource_failure == 3)) return -1;
    return 0;
}

int __wrap_fd2_field_game_march(fd2_field_game *game, fd2_vga *vga,
                               uint32_t frame_delay_ms) {
    (void)vga; (void)frame_delay_ms;
    march_called++;
    march_state_checked = march_text_presented == 1 &&
        game->turn_number == 17 && game->active_side == 2 &&
        game->interaction == FD2_FIELD_INTERACTION_BROWSE &&
        !memcmp(unchanged_units, game->units.items, sizeof(unchanged_units));
    if (march_case == 4) {
        game->failure = FD2_FIELD_FAILURE_ACTION;
        return -1;
    }
    if (march_case == 5) return 1;
    game->active_side = 1;
    if (march_case == 6) game->info_panel_right = 0;
    return 0;
}

static int storage_close_quit_test(fd2_vga *vga, fd2_archive *fdother) {
    char path[FD2_TEST_PATH_MAX];
    fd2_save_file source = {0}, after = {0};
    CHECK(fd2_test_temp_path(path) == 0);
    fd2_save_battle_snapshot snapshot = {0};
    fd2_field_unit units[2] = {0};
    units[0].side = 2; units[0].hp = 30;
    units[1].side = 0; units[1].hp = 20;
    memcpy(snapshot.unit_area, units, sizeof(units));
    snapshot.meta[FD2_SAVE_BATTLE_META_TURN] = 9;
    snapshot.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] = 2;
    CHECK(fd2_save_file_create_empty(&source) == 0);
    CHECK(fd2_save_file_update_battle_snapshot(&source, &snapshot) == 0);
    const fd2_field_system_action actions[] = {
        FD2_FIELD_SYSTEM_ACTION_SAVE, FD2_FIELD_SYSTEM_ACTION_LOAD
    };
    for (size_t i = 0; i < sizeof(actions) / sizeof(actions[0]); i++) {
        for (storage_quit_error = 0; storage_quit_error <= 1; storage_quit_error++) {
            CHECK(fd2_save_file_write(&source, path) == 0);
            struct stat before_stat, after_stat;
            CHECK(stat(path, &before_stat) == 0);
            fd2_field_play_context context;
            fd2_field_play_context_init(&context, NULL);
            context.experience_value = 77; context.message_number = 998;
            fd2_field_play_context before = context;
            storage_quit_action = actions[i];
            storage_calls = storage_closing_frames = action_sent = 0;
            fd2_field_play_result result = fd2_field_play_run_context(vga, fdother,
                0, 0, NULL, NULL, path, 0, &context);
            int stable = fd2_save_file_open(&after, path) == 0 &&
                after.size == source.size && !memcmp(after.data, source.data, source.size) &&
                stat(path, &after_stat) == 0 && before_stat.st_mtime == after_stat.st_mtime &&
                before_stat.st_size == after_stat.st_size;
            fd2_save_file_close(&after);
            int okay = result == (storage_quit_error ? FD2_FIELD_PLAY_RETURN_ERROR :
                    FD2_FIELD_PLAY_RETURN_HOST_QUIT) && storage_calls == 0 &&
                storage_closing_frames == 4 && action_sent == 1 && stable &&
                !memcmp(&context, &before, sizeof(context));
            if (!okay) {
                fprintf(stderr, "storage close quit: action=%d error=%d result=%d calls=%d stable=%d\n",
                    (int)actions[i], storage_quit_error, (int)result, storage_calls, stable);
                fd2_save_file_close(&source); remove(path); return 1;
            }
        }
    }
    storage_quit_action = FD2_FIELD_SYSTEM_ACTION_NONE;
    storage_quit_error = 0;
    fd2_save_file_close(&source);
    CHECK(remove(path) == 0);
    return 0;
}

static int result_frame_barrier_test(fd2_vga *vga, fd2_archive *fdother) {
    char path[FD2_TEST_PATH_MAX]; fd2_save_file source = {0}, after = {0};
    CHECK(fd2_test_temp_path(path) == 0 && fd2_save_file_create_empty(&source) == 0);
    fd2_save_battle_snapshot snapshot = {0};
    CHECK(fd2_field_unit_init(&result_frame_saved_units[0], 0,0,2,1,1) == 0);
    CHECK(fd2_field_unit_init(&result_frame_saved_units[1], 68,68,0,6,6) == 0);
    for (size_t i = 0; i < 2; i++) {
        result_frame_saved_units[i].flags = 0;
        result_frame_saved_units[i].hp = result_frame_saved_units[i].hp_max = i ? 28 : 42;
        result_frame_saved_units[i].movement_profile = 1;
        result_frame_saved_units[i].movement_points = 4;
        ((uint8_t *)&result_frame_saved_units[i])[0x31] = 0xff;
    }
    memcpy(snapshot.unit_area, result_frame_saved_units, sizeof(result_frame_saved_units));
    snapshot.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] = 2;
    snapshot.meta[FD2_SAVE_BATTLE_META_TURN] = 9;
    snapshot.meta[FD2_SAVE_BATTLE_META_FOCUS_X] = snapshot.meta[FD2_SAVE_BATTLE_META_FOCUS_Y] = 1;
    snapshot.meta[FD2_SAVE_BATTLE_META_FOCUS_REL_X] = snapshot.meta[FD2_SAVE_BATTLE_META_FOCUS_REL_Y] = 1;
    CHECK(fd2_save_file_update_battle_snapshot(&source, &snapshot) == 0);
    int failures = 0;
    for (result_frame_case = 1; result_frame_case <= 8; result_frame_case++) {
        CHECK(fd2_save_file_write(&source, path) == 0);
        struct stat before_stat, after_stat; CHECK(stat(path, &before_stat) == 0);
        result_frame_samples = result_frame_outer = result_frame_in_close = result_frame_fired = 0;
        result_frame_openings = result_frame_confirms = result_frame_browse_actions = 0;
        result_frame_storage = result_frame_storage_success = result_frame_final_seen = 0;
        fd2_field_play_context context; fd2_field_play_context_init(&context, NULL);
        context.experience_value = 77; context.message_number = 998;
        fd2_field_play_result result = fd2_field_play_run_context(vga, fdother, 0,0,NULL,NULL,path,0,&context);
        int stable = fd2_save_file_open(&after, path) == 0 && after.size == source.size &&
            !memcmp(after.data, source.data, source.size) && stat(path, &after_stat) == 0 &&
            before_stat.st_mtime == after_stat.st_mtime && before_stat.st_size == after_stat.st_size;
        fd2_save_file_close(&after);
        unsigned coordinate = result_frame_load() ? 1u : 3u;
        int okay = result == FD2_FIELD_PLAY_RETURN_HOST_QUIT && result_frame_fired == 1 &&
            result_frame_final_seen == 1 && result_frame_openings == 0 && result_frame_confirms == 0 &&
            result_frame_browse_actions == 0 && result_frame_final_interaction == FD2_FIELD_INTERACTION_BROWSE &&
            result_frame_final_x == coordinate && result_frame_final_y == coordinate &&
            result_frame_storage == result_frame_load() && result_frame_storage_success == result_frame_load() &&
            context.experience_value == 77 && context.message_number == 998 && result_frame_final_experience == 77 &&
            result_frame_rng == result_frame_final_rng && stable &&
            !memcmp(result_frame_expected_units, result_frame_final_units, sizeof(result_frame_final_units));
        printf("result-frame barrier case=%d load=%d right=%d quit=%d result=%d fired=%d browse_actions=%d confirms=%d openings=%d cursor=%u,%u XP=%u storage=%d/%d stable=%d pass=%d\n",
            result_frame_case, result_frame_load(), result_frame_right(), result_frame_quit_now(), (int)result,
            result_frame_fired, result_frame_browse_actions, result_frame_confirms, result_frame_openings,
            result_frame_final_x,result_frame_final_y,(unsigned)context.experience_value,
            result_frame_storage,result_frame_storage_success,stable,okay);
        if (!okay) ++failures;
    }
    result_frame_case = 0; fd2_save_file_close(&source); remove(path); fd2_input_init(&vga->input);
    CHECK(failures == 0); return 0;
}

static int closing_frame_barrier_test(fd2_vga *vga, fd2_archive *fdother) {
    char path[FD2_TEST_PATH_MAX]; fd2_save_file source={0}, after={0};
    CHECK(fd2_test_temp_path(path)==0 && fd2_save_file_create_empty(&source)==0);
    int failures=0;
    for (closing_case=1; closing_case<=10; closing_case++) {
        CHECK(fd2_save_file_write(&source,path)==0);
        struct stat before_stat, after_stat; CHECK(stat(path,&before_stat)==0);
        fd2_field_play_context context;fd2_field_play_context_init(&context,NULL);
        context.experience_value=77;context.message_number=998;
        fd2_field_play_context before=context;
        closing_frames=closing_openings=closing_confirms=closing_outer_frames=0;
        closing_in_animation=closing_final_seen=0;vga->input.quit_requested=0;
        fd2_field_play_result result=fd2_field_play_run_context(vga,fdother,0,0,NULL,NULL,path,0,&context);
        int mode=closing_mode();
        fd2_field_play_result expected=mode>=3?FD2_FIELD_PLAY_RETURN_ERROR:FD2_FIELD_PLAY_RETURN_HOST_QUIT;
        fd2_field_interaction interaction=closing_is_system()
            ? (mode<2?FD2_FIELD_INTERACTION_BROWSE:FD2_FIELD_INTERACTION_SYSTEM_MENU)
            : (mode<2?FD2_FIELD_INTERACTION_TARGETING:FD2_FIELD_INTERACTION_COMMAND);
        int stable=fd2_save_file_open(&after,path)==0 && after.size==source.size &&
            !memcmp(after.data,source.data,source.size) && stat(path,&after_stat)==0 &&
            before_stat.st_mtime==after_stat.st_mtime && before_stat.st_size==after_stat.st_size;
        fd2_save_file_close(&after);
        int okay=result==expected && closing_final_seen==1 && closing_frames==4 &&
            closing_openings==0 && closing_confirms==(closing_is_system()||mode>=2?0:1) &&
            closing_final_interaction==interaction &&
            closing_final_x==(closing_is_system()?3:mode<2?2:1) && closing_final_y==(closing_is_system()?3:1) &&
            !memcmp(closing_before_units,closing_final_units,sizeof(closing_before_units)) &&
            !closing_final_pending.valid && closing_final_rng==closing_before_rng && stable &&
            !memcmp(&context,&before,sizeof(context));
        printf("closing barrier case=%d result=%d expected=%d frames=%d confirms=%d openings=%d interaction=%d expected_interaction=%d cursor=%u,%u stable=%d units_equal=%d rng_equal=%d pass=%d\n",
            closing_case,(int)result,(int)expected,closing_frames,closing_confirms,closing_openings,
            (int)closing_final_interaction,(int)interaction,closing_final_x,closing_final_y,stable,
            !memcmp(closing_before_units,closing_final_units,sizeof(closing_before_units)),closing_final_rng==closing_before_rng,okay);
        if(!okay)failures++;
    }
    closing_case=0;fd2_save_file_close(&source);remove(path);
    fd2_input_init(&vga->input);
    CHECK(failures==0);return 0;
}

int main(void) {
    fd2_vga vga = {0};
    fd2_archive fdother = {0};
    CHECK(result_frame_barrier_test(&vga, &fdother) == 0);
    CHECK(closing_frame_barrier_test(&vga, &fdother) == 0);
    automatic_failure = 1;
    CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_ERROR);
    CHECK(failure_called == 1 && ordinary_automatic_clean == 0);

    automatic_failure = 0;
    failure_called = 0;
    scene_result = -1;
    CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_ERROR);
    CHECK(failure_called == 1);

    failure_called = 0;
    scene_result = FD2_SCENE_RESULT_HOST_QUIT;
    CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_HOST_QUIT);
    CHECK(failure_called == 1);
    failure_called = 0;
    movement_failure = 1;
    CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_ERROR);
    CHECK(failure_called == 1);
    movement_failure = 0;
    cancellation_case = 1;
    const fd2_input_key cancel_keys[] = {FD2_INPUT_KEY_ESCAPE, FD2_INPUT_KEY_CANCEL};
    for (size_t i = 0; i < sizeof(cancel_keys) / sizeof(cancel_keys[0]); i++) {
        action_sent = 0;
        cancellation_checked = 0;
        cancellation_key = cancel_keys[i];
        CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
              FD2_FIELD_PLAY_RETURN_HOST_QUIT);
        CHECK(cancellation_checked);
    }
    cancellation_case = 0;
    failure_called = 0;
    for (session_failure_case = 1; session_failure_case <= 3;
         session_failure_case++) {
        action_sent = 0;
        CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
              FD2_FIELD_PLAY_RETURN_ERROR);
        CHECK(failure_called == 0);
        if (session_failure_case == 2) CHECK(action_sent == 1);
    }
    /* 成功呈现由owner确认一次；替身不清deferred也不能下一帧重放。 */
    session_failure_case = 4;
    action_sent = 0;
    scene_result = 0;
    CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_HOST_QUIT);
    CHECK(failure_called == 1);
    session_failure_case = 0;
    terminal_case = 1;
    CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_VICTORY);
    terminal_case = 2;
    CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_DEFEAT);
    terminal_case = 3;
    action_sent = 0;
    failure_called = 0;
    leave_text_presented = 0;
    leave_state_checked = 0;
    CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_LEAVE_GAME);
    CHECK(action_sent == 1 && failure_called == 0);
    CHECK(leave_text_presented == 1 && leave_state_checked);
    terminal_case = 4;
    action_sent = 0;
    auxiliary_checked = 0;
    /* 该替身没有地图/terrain资源，F1须明确失败且不继续取下一按键。
     * 有效页面开合与输入隔离由field_play_auxiliary独立覆盖。 */
    CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_ERROR);
    CHECK(action_sent == 1 && !auxiliary_checked);
    terminal_case = 5;
    vga.input.quit_requested = 1;
    CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_HOST_QUIT);
    vga.input.quit_requested = 0;
    terminal_case = 6;
    action_sent = 0;
    failure_called = 0;
    CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_ERROR);
    CHECK(action_sent == 1 && failure_called == 0);
    /* 缺文本、字体或 accepted fragment 都不能冒充成功离场。 */
    terminal_case = 7;
    for (leave_missing_resource = 0; leave_missing_resource < 3;
         ++leave_missing_resource) {
        action_sent = 0;
        failure_called = 0;
        leave_text_presented = 0;
        CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
              FD2_FIELD_PLAY_RETURN_ERROR);
        CHECK(action_sent == 1 && failure_called == 0 &&
              leave_text_presented == 0);
    }
    /* accepted 提示呈现时收到宿主关闭，来源仍必须是 HOST_QUIT。 */
    terminal_case = 8;
    action_sent = 0;
    failure_called = 0;
    leave_text_presented = 0;
    leave_state_checked = 0;
    CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_HOST_QUIT);
    CHECK(action_sent == 1 && failure_called == 0);
    CHECK(leave_text_presented == 1 && leave_state_checked);
    vga.input.quit_requested = 0;
    terminal_case = 0;
    for (controller_case = 1; controller_case <= 4; controller_case++) {
        controller_present_count = controller_automatic_called = 0;
        controller_input_checked = controller_sync_drained = 0;
        CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
              FD2_FIELD_PLAY_RETURN_HOST_QUIT);
        CHECK(controller_input_checked && controller_sync_drained);
        CHECK(controller_automatic_called == ((controller_case == 2 || controller_case == 3) ? 1 : 0));
    }
    controller_case = 0;
    /* 两个详情入口都必须传播资源/开合失败；后续输入正常，不能靠别的错误通过。 */
    for (controller_case = 3; controller_case <= 4; controller_case++) {
        for (detail_resource_failure = 1; detail_resource_failure <= 3;
             detail_resource_failure++) {
            controller_present_count = controller_automatic_called = 0;
            detail_open_calls = detail_open_animation_calls = 0;
            detail_close_animation_calls = detail_close_calls = 0;
            vga.input.quit_requested = 0;
            CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
                  FD2_FIELD_PLAY_RETURN_ERROR);
            CHECK(detail_open_calls == 1 && detail_close_calls == 0);
            CHECK(detail_open_animation_calls == (detail_resource_failure >= 2));
            CHECK(detail_close_animation_calls == (detail_resource_failure == 3));
            CHECK(controller_automatic_called == 0);
        }
    }
    controller_case = 4;
    detail_resource_failure = 1;
    for (detail_no_target = 1; detail_no_target <= 2; detail_no_target++) {
        controller_present_count = controller_automatic_called = 0;
        detail_open_calls = detail_open_animation_calls = 0;
        vga.input.quit_requested = 0;
        CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
              FD2_FIELD_PLAY_RETURN_HOST_QUIT);
        CHECK(detail_open_calls == 0 && detail_open_animation_calls == 0);
    }
    controller_case = detail_resource_failure = detail_no_target = 0;
    for (march_case = 1; march_case <= 6; march_case++) {
        for (march_missing_resource = 0; march_missing_resource <
                (march_case == 1 ? 3 : 1); march_missing_resource++) {
            action_sent = march_called = march_text_presented = march_state_checked = 0;
            march_phase_called = march_phase_input_checked = 0;
            march_phase_clean_parameter = march_phase_outer_clean = 0;
            vga.input.quit_requested = 0;
            fd2_field_play_result expected = march_case == 1 || march_case == 4
                ? FD2_FIELD_PLAY_RETURN_ERROR : FD2_FIELD_PLAY_RETURN_HOST_QUIT;
            CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) == expected);
            CHECK(march_called == (march_case >= 3 ? 1 : 0));
            CHECK(march_text_presented == (march_case == 1 ? 0 : 1));
            if (march_case >= 3) CHECK(march_state_checked);
            if (march_case == 6) CHECK(march_phase_called == 1 && march_phase_input_checked &&
                march_phase_clean_parameter == 1 && march_phase_outer_clean);
        }
    }
    march_case = 0;
    for (item_case = 1; item_case <= 4; item_case++) {
        item_frames = item_confirm_calls = item_context_bad = 0;
        item_menu_checked = item_invalid_checked = item_finished_checked = 0;
        vga.input.quit_requested = 0;
        CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
            (item_case == 3 ? FD2_FIELD_PLAY_RETURN_ERROR : FD2_FIELD_PLAY_RETURN_HOST_QUIT));
        CHECK(!item_context_bad);
        if (item_case == 1) CHECK(item_menu_checked && item_finished_checked && item_frames == 5);
        if (item_case == 2) CHECK(item_invalid_checked && item_finished_checked && item_frames == 5);
        if (item_case == 3) CHECK(item_confirm_calls == 0 && item_frames == 1);
        if (item_case == 4) CHECK(item_menu_checked && item_finished_checked && item_frames == 4 && item_confirm_calls == 1);
    }
    item_case = 0;
    remainder_case=1;remainder_scene=remainder_automatic=remainder_checked=action_sent=0;
    vga.input.quit_requested=0;
    CHECK(fd2_field_play_run(&vga,&fdother,0,0,NULL,NULL,NULL,0)==FD2_FIELD_PLAY_RETURN_DEFEAT);
    CHECK(remainder_scene==1 && remainder_automatic==1 && remainder_checked);
    CHECK(action_sent==2); /* 先render待续态，再render清source后的终态；没有普通键。 */
    remainder_case=0;
    CHECK(storage_close_quit_test(&vga, &fdother) == 0);
    /* 真实BROWSE确认→首键等待；present替身仅生成真实input逐帧脉冲。 */
    for (summary_case = 1; summary_case <= 5; summary_case++) {
        summary_present = summary_draws = summary_confirms = summary_dispatches = 0;
        summary_initial_consumed = summary_checked = summary_bad_owner = 0;
        vga.input.quit_requested = 0;
        CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
              (summary_case == 3 ? FD2_FIELD_PLAY_RETURN_ERROR : FD2_FIELD_PLAY_RETURN_HOST_QUIT));
        CHECK(!summary_bad_owner && summary_confirms == 1);
        if (summary_case == 3) CHECK(summary_draws == 1 && summary_present == 1 && summary_dispatches == 0);
        else if (summary_case == 4) CHECK(summary_draws == 2 && summary_present == 3 && summary_dispatches == 0);
        else CHECK(summary_checked && summary_draws == (summary_case == 5 ? 0 : 3));
        if (summary_case != 5) CHECK(summary_initial_consumed);
    }
    summary_case = 0;
    puts("field_play_result_test: ok");
    return 0;
}
