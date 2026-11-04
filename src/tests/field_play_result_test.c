/* 仅测试构建替换正式 field_play 依赖，验证错误出口和移动取消，不启动窗口或原版游戏。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_game.h"
#include "field_play.h"
#include "field_rng.h"
#include "scene.h"

/* 仅测试TU在wrapper中复用正式input消费，不把注入脉冲变成手工action队列。 */
#ifdef fd2_input_take_action
#undef fd2_input_take_action
int fd2_input_take_action(fd2_input *, fd2_input_context, fd2_input_action *);
#endif

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
static int controller_case;
static int controller_present_count;
static int controller_automatic_called;
static int controller_input_checked;
static int controller_sync_drained;
static uint64_t controller_repeat_at;

static int is_leave_case(void) {
    return terminal_case == 3 || terminal_case == 7 || terminal_case == 8;
}

int __wrap_fd2_field_game_open(fd2_field_game *game, fd2_vga *vga,
                               const fd2_archive *fdother, size_t stage) {
    (void)vga;
    (void)fdother;
    memset(game, 0, sizeof(*game));
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
    if (is_leave_case() || terminal_case == 6) {
        fd2_field_system_menu_init(&game->system_menu);
        game->system_menu.page = FD2_FIELD_SYSTEM_PAGE_CONFIRMATION;
        game->system_menu.confirmation_action = is_leave_case()
            ? FD2_FIELD_SYSTEM_ACTION_LEAVE_BATTLE
            : FD2_FIELD_SYSTEM_ACTION_END_TURN_CONFIRM;
        game->system_menu.confirmation_return_page = is_leave_case()
            ? FD2_FIELD_SYSTEM_PAGE_SECONDARY : FD2_FIELD_SYSTEM_PAGE_PARENT;
        game->system_menu.confirmation_return_selected = 3;
        game->system_menu.confirmation_yes = 1;
        game->interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
    }
    if (is_leave_case()) {
        /* 只有 FDTXT[416] 非空；一个测试字形的下左阴影用于识别
         * 真正呈现，不依赖生产日志或额外测试钩子。offsets 归 session。 */
        static const uint8_t text[] = {0, 0, 0xff, 0xff};
        static const uint8_t glyph[32] = {0x80};
        game->ui_text.offsets = calloc(
            FD2_FIELD_SYSTEM_TEXT_LEAVE_ACCEPTED + 1u,
            sizeof(*game->ui_text.offsets));
        if (!game->ui_text.offsets) return -1;
        game->ui_text.data = text;
        game->ui_text.size = sizeof(text);
        game->ui_text.fragment_count =
            FD2_FIELD_SYSTEM_TEXT_LEAVE_ACCEPTED + 1u;
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
    if (terminal_case >= 3) game->event_log_count = 0;
    return 0;
}

int __wrap_fd2_field_game_process_automatic_action_visual(
        fd2_field_game *game, fd2_vga *vga, uint32_t frame_delay_ms) {
    (void)game;
    (void)vga;
    (void)frame_delay_ms;
    if (controller_case == 2 || controller_case == 3) {
        controller_automatic_called++;
        game->active_side = 2;
        game->turn_number++;
        /* 模拟phase最后一帧产生普通键；真实resume不保存/恢复pending。 */
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
        fd2_input_begin_test_frame(&vga->input, 1000);
        return fd2_field_game_resume_player_controller(game) < 0 ? -1 : 1;
    }
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
    failure_called++;
    return scene_result;
}

void __wrap_fd2_vga_present(fd2_vga *vga) {
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

int __wrap_fd2_input_take_action(fd2_input *input, fd2_input_context context,
                                 fd2_input_action *action) {
    if (controller_case) return fd2_input_take_action(input, context, action);
    (void)input;
    if ((is_leave_case() || terminal_case == 6) && action_sent++ == 0) {
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


int __wrap_fd2_field_game_open_detail(fd2_field_game *game, size_t actor) {
    game->detail_visible = 1;
    game->detail_unit = (int)actor;
    return 0;
}
void __wrap_fd2_field_game_close_detail(fd2_field_game *game) {
    game->detail_visible = 0;
    game->detail_unit = -1;
}
int __wrap_fd2_field_game_animate_detail(fd2_field_game *game, fd2_vga *vga,
        int opening, fd2_field_detail_phase_fn callback, void *userdata) {
    (void)game; (void)vga; (void)opening; (void)callback; (void)userdata;
    return 0;
}

int main(void) {
    fd2_vga vga = {0};
    fd2_archive fdother = {0};
    automatic_failure = 1;
    CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_ERROR);
    CHECK(failure_called == 1);

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
    CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_HOST_QUIT);
    CHECK(auxiliary_checked);
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
    puts("field_play_result_test: ok");
    return 0;
}
