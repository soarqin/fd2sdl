#include <stdio.h>

#include "input.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "check failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return -1; \
    } \
} while (0)

static int test_context_mappings(void) {
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_TITLE,
                                   FD2_INPUT_KEY_UP) == FD2_INPUT_ACTION_UP);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_TITLE,
                                   FD2_INPUT_KEY_KEYPAD_CONFIRM) ==
          FD2_INPUT_ACTION_CONFIRM);
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

    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_COMMAND,
                                   FD2_INPUT_KEY_UP) == FD2_INPUT_ACTION_UP);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_COMMAND,
                                   FD2_INPUT_KEY_LEFT) == FD2_INPUT_ACTION_LEFT);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_COMMAND,
                                   FD2_INPUT_KEY_SPACE) ==
          FD2_INPUT_ACTION_CONFIRM);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_COMMAND,
                                   FD2_INPUT_KEY_CANCEL) ==
          FD2_INPUT_ACTION_CANCEL);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_COMMAND,
                                   FD2_INPUT_KEY_HOME) ==
          FD2_INPUT_ACTION_NONE);

    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_SYSTEM_MENU,
                                   FD2_INPUT_KEY_RIGHT) ==
          FD2_INPUT_ACTION_RIGHT);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_SYSTEM_MENU,
                                   FD2_INPUT_KEY_ENTER) ==
          FD2_INPUT_ACTION_CONFIRM);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_SYSTEM_MENU,
                                   FD2_INPUT_KEY_ESCAPE) ==
          FD2_INPUT_ACTION_CANCEL);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_MANUAL_SLOT,
                                   FD2_INPUT_KEY_UP) ==
          FD2_INPUT_ACTION_LEFT);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_MANUAL_SLOT,
                                   FD2_INPUT_KEY_DOWN) ==
          FD2_INPUT_ACTION_RIGHT);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_MANUAL_SLOT,
                                   FD2_INPUT_KEY_LEFT) ==
          FD2_INPUT_ACTION_NONE);

    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_TARGETING,
                                   FD2_INPUT_KEY_RIGHT) ==
          FD2_INPUT_ACTION_RIGHT);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_TARGETING,
                                   FD2_INPUT_KEY_ENTER) ==
          FD2_INPUT_ACTION_CONFIRM);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_TARGETING,
                                   FD2_INPUT_KEY_CANCEL) ==
          FD2_INPUT_ACTION_CANCEL);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_TARGETING,
                                   FD2_INPUT_KEY_F2) ==
          FD2_INPUT_ACTION_NONE);
    return 0;
}

static int test_no_preinput_queue(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 1);
    CHECK(fd2_input_has_any_key(&input));

    /* 交互层读取前已经松开：没有 KEY_DOWN 队列可在之后重放。 */
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 0);
    CHECK(!fd2_input_has_any_key(&input));
    fd2_input_event event;
    CHECK(!fd2_input_take_key_at(&input, &event, 100));
    return 0;
}

static void arm_test_repeat(fd2_input *input, SDL_Scancode scancode) {
    input->repeat_armed[scancode] = true;
}

static int test_non_consuming_check_then_take(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 1);

    /* 对话逐字 input_check 只观察当前键态，不登记一次 UI 动作。 */
    CHECK(fd2_input_has_any_key(&input));
    CHECK(fd2_input_has_any_key(&input));

    fd2_input_event event;
    CHECK(fd2_input_take_key_at(&input, &event, 100));
    CHECK(event.key == FD2_INPUT_KEY_ENTER && event.repeat == 0);
    CHECK(!fd2_input_has_any_key_at(
        &input, 100 + FD2_INPUT_INITIAL_REPEAT_MS - 1));
    arm_test_repeat(&input, SDL_SCANCODE_RETURN);
    CHECK(fd2_input_has_any_key_at(
        &input, 100 + FD2_INPUT_INITIAL_REPEAT_MS));
    return 0;
}

static int test_typematic_deadlines(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_DOWN, 1);

    fd2_input_event event;
    CHECK(fd2_input_take_key_at(&input, &event, 1000));
    CHECK(event.key == FD2_INPUT_KEY_DOWN && event.repeat == 0);
    CHECK(!fd2_input_take_key_at(&input, &event,
                                 1000 + FD2_INPUT_INITIAL_REPEAT_MS - 1));
    /* 只有宿主开始投递 typematic KEY_DOWN 后，持续按住才可重复。 */
    arm_test_repeat(&input, SDL_SCANCODE_DOWN);
    CHECK(fd2_input_take_key_at(&input, &event,
                                1000 + FD2_INPUT_INITIAL_REPEAT_MS));
    CHECK(event.key == FD2_INPUT_KEY_DOWN && event.repeat == 1);
    CHECK(!fd2_input_take_key_at(&input, &event,
                                 1000 + FD2_INPUT_INITIAL_REPEAT_MS +
                                 FD2_INPUT_REPEAT_MS - 1));
    CHECK(fd2_input_take_key_at(&input, &event,
                                1000 + FD2_INPUT_INITIAL_REPEAT_MS +
                                FD2_INPUT_REPEAT_MS));
    CHECK(event.repeat == 1);

    /* 松开后再次按下是新的第一次按下，不继承旧 deadline。 */
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_DOWN, 0);
    CHECK(!fd2_input_take_key_at(&input, &event, 2000));
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_DOWN, 1);
    CHECK(fd2_input_take_key_at(&input, &event, 2001));
    CHECK(event.repeat == 0);
    return 0;
}

static int test_short_press_never_synthesizes_repeat(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 1);
    fd2_input_event event;
    CHECK(fd2_input_take_key_at(&input, &event, 100));
    /* 没有宿主 typematic repeat，即使测试键态仍为 down，等待多久也
     * 不能自行生成第二次 UI 输入。 */
    CHECK(!fd2_input_take_key_at(&input, &event, 10000));
    return 0;
}

static int test_state_transition_does_not_retrigger_held_key(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 1);

    fd2_input_action action;
    fd2_input_event event;
    CHECK(fd2_input_observe_any_key_at(&input, 500));
    CHECK(!fd2_input_take_key_at(&input, &event, 501));
    CHECK(!fd2_input_has_any_key_at(&input, 999));
    arm_test_repeat(&input, SDL_SCANCODE_RETURN);
    CHECK(fd2_input_take_key_at(&input, &event, 1000));
    action = fd2_input_action_for_key(FD2_INPUT_CONTEXT_TITLE, event.key);
    CHECK(action == FD2_INPUT_ACTION_CONFIRM);
    CHECK(event.repeat == 1);
    return 0;
}

static int test_repeat_observe_restarts_initial_delay(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 1);
    fd2_input_event event;
    CHECK(fd2_input_take_key_at(&input, &event, 100));
    arm_test_repeat(&input, SDL_SCANCODE_RETURN);
    CHECK(fd2_input_observe_any_key_at(
        &input, 100 + FD2_INPUT_INITIAL_REPEAT_MS));
    CHECK(!fd2_input_take_key_at(
        &input, &event,
        100 + FD2_INPUT_INITIAL_REPEAT_MS + FD2_INPUT_REPEAT_MS));
    arm_test_repeat(&input, SDL_SCANCODE_RETURN);
    CHECK(fd2_input_take_key_at(
        &input, &event,
        100 + FD2_INPUT_INITIAL_REPEAT_MS * 2));
    CHECK(event.repeat == 1);
    return 0;
}

static int test_release_repress_between_reads(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 1);
    fd2_input_event event;
    CHECK(fd2_input_take_key_at(&input, &event, 100));

    /* 释放和重新按下发生在两次交互读取之间，事件泵仍须观察到 KEY_UP
     * 并让重新按下成为新的 initial press。 */
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 0);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 1);
    CHECK(fd2_input_take_key_at(&input, &event, 101));
    CHECK(event.repeat == 0);
    return 0;
}

static int test_sdl_event_pump_no_preinput(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input input;
    fd2_input_init(&input);
    SDL_Event event = {0};

    /* UI 读取前完成 down/up：事件泵只留下最终电平，不留下预输入。 */
    event.type = SDL_EVENT_KEY_DOWN;
    event.key.scancode = SDL_SCANCODE_RETURN;
    CHECK(SDL_PushEvent(&event));
    event.type = SDL_EVENT_KEY_UP;
    CHECK(SDL_PushEvent(&event));
    fd2_input_poll_host_events(&input);
    fd2_input_event key;
    CHECK(!fd2_input_take_key_at(&input, &key, 100));

    /* 普通短按读取一次后，即使测试时间远超 500 ms，也不能在没有
     * SDL typematic repeat 的情况下自行连发。 */
    event.type = SDL_EVENT_KEY_DOWN;
    CHECK(SDL_PushEvent(&event));
    fd2_input_poll_host_events(&input);
    CHECK(fd2_input_take_key_at(&input, &key, 200));
    CHECK(!fd2_input_take_key_at(&input, &key, 5000));

    /* 收到真实 repeat 后仍服从第一次读取建立的 initial deadline。 */
    event.key.repeat = true;
    CHECK(SDL_PushEvent(&event));
    fd2_input_poll_host_events(&input);
    CHECK(!fd2_input_take_key_at(
        &input, &key, 200 + FD2_INPUT_INITIAL_REPEAT_MS - 1));
    CHECK(fd2_input_take_key_at(
        &input, &key, 200 + FD2_INPUT_INITIAL_REPEAT_MS));
    CHECK(key.repeat == 1);

    /* 两次 UI 读取之间发生 release/repress，按事件顺序恢复为新按下。 */
    event.key.repeat = false;
    event.type = SDL_EVENT_KEY_UP;
    CHECK(SDL_PushEvent(&event));
    event.type = SDL_EVENT_KEY_DOWN;
    CHECK(SDL_PushEvent(&event));
    fd2_input_poll_host_events(&input);
    CHECK(fd2_input_take_key_at(&input, &key, 701));
    CHECK(key.repeat == 0);
    SDL_Quit();
    return 0;
}

static int test_modifier_not_game_key(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_LSHIFT, 1);
    CHECK(!fd2_input_has_any_key_at(&input, 0));
    fd2_input_event event;
    CHECK(!fd2_input_take_key_at(&input, &event, 0));
    return 0;
}

static int test_quit_request(void) {
    fd2_input input;
    fd2_input_init(&input);
    input.quit_requested = 1;
    CHECK(fd2_input_take_quit(&input));
    CHECK(fd2_input_take_quit(&input));
    return 0;
}

int main(void) {
    if (test_context_mappings() != 0 ||
        test_no_preinput_queue() != 0 ||
        test_non_consuming_check_then_take() != 0 ||
        test_typematic_deadlines() != 0 ||
        test_short_press_never_synthesizes_repeat() != 0 ||
        test_state_transition_does_not_retrigger_held_key() != 0 ||
        test_repeat_observe_restarts_initial_delay() != 0 ||
        test_release_repress_between_reads() != 0 ||
        test_sdl_event_pump_no_preinput() != 0 ||
        test_modifier_not_game_key() != 0 ||
        test_quit_request() != 0)
        return 1;
    puts("input_test: ok");
    return 0;
}
