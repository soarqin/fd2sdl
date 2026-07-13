#include <stdio.h>

#include "input.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "check failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return -1; \
    } \
} while (0)

static int test_peek_and_fifo(void) {
    fd2_input input;
    fd2_input_init(&input);
    CHECK(fd2_input_push_key(&input, FD2_INPUT_KEY_OTHER,
                             SDL_SCANCODE_A, 0) == 0);
    CHECK(fd2_input_push_key(&input, FD2_INPUT_KEY_UP,
                             SDL_SCANCODE_UP, 1) == 0);
    /* 对应 input_check：查看不会消费 ANI 跳过键。 */
    CHECK(fd2_input_has_any_key(&input));
    CHECK(fd2_input_pending_count(&input) == 2);

    fd2_input_event event;
    CHECK(fd2_input_take_key(&input, &event));
    CHECK(event.key == FD2_INPUT_KEY_OTHER && event.repeat == 0);
    CHECK(fd2_input_take_key(&input, &event));
    CHECK(event.key == FD2_INPUT_KEY_UP && event.repeat == 1);
    CHECK(!fd2_input_has_any_key(&input));
    return 0;
}

static int test_context_mappings(void) {
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_TITLE,
                                   FD2_INPUT_KEY_UP) == FD2_INPUT_ACTION_UP);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_TITLE,
                                   FD2_INPUT_KEY_KEYPAD_CONFIRM) ==
          FD2_INPUT_ACTION_CONFIRM);
    /* 原版标题没有 H/P/R/Esc 快捷键。 */
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_TITLE,
                                   FD2_INPUT_KEY_G) == FD2_INPUT_ACTION_NONE);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_TITLE,
                                   FD2_INPUT_KEY_ESCAPE) == FD2_INPUT_ACTION_NONE);

    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_CHOICE,
                                   FD2_INPUT_KEY_CANCEL) ==
          FD2_INPUT_ACTION_CANCEL);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_CHOICE,
                                   FD2_INPUT_KEY_RIGHT) ==
          FD2_INPUT_ACTION_RIGHT);

    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD,
                                   FD2_INPUT_KEY_HOME) ==
          FD2_INPUT_ACTION_FIELD_DETAIL);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD,
                                   FD2_INPUT_KEY_PAGE_UP) ==
          FD2_INPUT_ACTION_FIELD_AUXILIARY);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD,
                                   FD2_INPUT_KEY_Z) ==
          FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD,
                                   FD2_INPUT_KEY_CANCEL) ==
          FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD,
                                   FD2_INPUT_KEY_ESCAPE) ==
          FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE);
    return 0;
}

static int test_consumption_and_capacity(void) {
    fd2_input input;
    fd2_input_init(&input);
    CHECK(fd2_input_push_key(&input, FD2_INPUT_KEY_G,
                             SDL_SCANCODE_G, 0) == 0);
    fd2_input_action action = FD2_INPUT_ACTION_CONFIRM;
    /* 未处理键也会像 INT 16h 一样被读走。 */
    CHECK(fd2_input_take_action(&input, FD2_INPUT_CONTEXT_TITLE, &action));
    CHECK(action == FD2_INPUT_ACTION_NONE);
    CHECK(fd2_input_pending_count(&input) == 0);

    for (size_t i = 0; i < FD2_INPUT_QUEUE_CAPACITY; i++)
        CHECK(fd2_input_push_key(&input, FD2_INPUT_KEY_OTHER,
                                 SDL_SCANCODE_UNKNOWN, 0) == 0);
    CHECK(fd2_input_push_key(&input, FD2_INPUT_KEY_OTHER,
                             SDL_SCANCODE_UNKNOWN, 0) == -1);
    CHECK(input.dropped_keys == 1);
    CHECK(fd2_input_pending_count(&input) == FD2_INPUT_QUEUE_CAPACITY);
    return 0;
}

static int test_terminal_action_preserves_following_key(void) {
    fd2_input input;
    fd2_input_init(&input);
    CHECK(fd2_input_push_key(&input, FD2_INPUT_KEY_ENTER,
                             SDL_SCANCODE_RETURN, 0) == 0);
    CHECK(fd2_input_push_key(&input, FD2_INPUT_KEY_DOWN,
                             SDL_SCANCODE_DOWN, 0) == 0);
    fd2_input_action action;
    CHECK(fd2_input_take_action(&input, FD2_INPUT_CONTEXT_TITLE, &action));
    CHECK(action == FD2_INPUT_ACTION_CONFIRM);
    /* 标题／预览循环在终止动作后停止读取，下一 UI 仍可取得 Down。 */
    CHECK(fd2_input_pending_count(&input) == 1);
    CHECK(fd2_input_take_action(&input, FD2_INPUT_CONTEXT_TITLE, &action));
    CHECK(action == FD2_INPUT_ACTION_DOWN);
    return 0;
}

static int test_sdl_event_pump(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input input;
    fd2_input_init(&input);

    SDL_Event event = {0};
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_UP;
    CHECK(SDL_PushEvent(&event));

    event.type = SDL_EVENT_KEY_UP;
    event.key.scancode = SDL_SCANCODE_DOWN;
    CHECK(SDL_PushEvent(&event));

    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_KP_0;
    event.key.repeat = 1;
    CHECK(SDL_PushEvent(&event));

    event.type = SDL_EVENT_QUIT;
    CHECK(SDL_PushEvent(&event));

    fd2_input_pump(&input);
    CHECK(fd2_input_pending_count(&input) == 2);
    fd2_input_event key;
    CHECK(fd2_input_take_key(&input, &key));
    CHECK(key.key == FD2_INPUT_KEY_UP && key.repeat == 0);
    CHECK(fd2_input_take_key(&input, &key));
    CHECK(key.key == FD2_INPUT_KEY_KEYPAD_CONFIRM && key.repeat == 1);
    CHECK(fd2_input_take_quit(&input));
    SDL_Quit();
    return 0;
}

static int test_quit_request(void) {
    fd2_input input;
    fd2_input_init(&input);
    input.quit_requested = 1;
    CHECK(fd2_input_take_quit(&input));
    CHECK(!fd2_input_take_quit(&input));
    return 0;
}

int main(void) {
    if (test_peek_and_fifo() != 0 ||
        test_context_mappings() != 0 ||
        test_consumption_and_capacity() != 0 ||
        test_terminal_action_preserves_following_key() != 0 ||
        test_sdl_event_pump() != 0 ||
        test_quit_request() != 0)
        return 1;
    puts("input_test: ok");
    return 0;
}
