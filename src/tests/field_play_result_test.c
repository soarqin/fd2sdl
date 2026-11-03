/* 用链接器替身触发正式 field_play 循环的错误出口，不启动窗口或原版游戏。 */
#include <stdio.h>
#include <string.h>

#include "field_game.h"
#include "field_play.h"
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
    if (!automatic_failure && !movement_failure) {
        game->event_log_count = 1;
        game->event_log[0].handled = 1;
        game->event_log[0].presentation_deferred = 1;
    }
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
    (void)context;
    if (!movement_failure || action_sent) return 0;
    action_sent = 1;
    *action = FD2_INPUT_ACTION_CONFIRM;
    return 1;
}

int __wrap_fd2_field_game_confirm_cursor(fd2_field_game *game) {
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
    puts("field_play_result_test: ok");
    return 0;
}
