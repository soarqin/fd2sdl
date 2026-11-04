/* 仅测试构建替换正式 field_play 依赖，验证错误出口和移动取消，不启动窗口或原版游戏。 */
#include <stdio.h>
#include <string.h>

#include "field_game.h"
#include "field_play.h"
#include "field_rng.h"
#include "scene.h"

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

int __wrap_fd2_field_game_open(fd2_field_game *game, fd2_vga *vga,
                               const fd2_archive *fdother, size_t stage) {
    (void)vga;
    (void)fdother;
    memset(game, 0, sizeof(*game));
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
    if (terminal_case == 3 || terminal_case == 6) {
        fd2_field_system_menu_init(&game->system_menu);
        game->system_menu.page = FD2_FIELD_SYSTEM_PAGE_CONFIRMATION;
        game->system_menu.confirmation_action = terminal_case == 3
            ? FD2_FIELD_SYSTEM_ACTION_LEAVE_BATTLE
            : FD2_FIELD_SYSTEM_ACTION_END_TURN_CONFIRM;
        game->system_menu.confirmation_return_page = terminal_case == 3
            ? FD2_FIELD_SYSTEM_PAGE_SECONDARY : FD2_FIELD_SYSTEM_PAGE_PARENT;
        game->system_menu.confirmation_return_selected = 3;
        game->system_menu.confirmation_yes = 1;
        game->interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
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

void __wrap_fd2_vga_present(fd2_vga *vga) { (void)vga; }

int __wrap_fd2_input_take_action(fd2_input *input, fd2_input_context context,
                                 fd2_input_action *action) {
    (void)input;
    if ((terminal_case == 3 || terminal_case == 6) && action_sent++ == 0) {
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
    CHECK(fd2_field_play_run(&vga, &fdother, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_TITLE);
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
    puts("field_play_result_test: ok");
    return 0;
}
