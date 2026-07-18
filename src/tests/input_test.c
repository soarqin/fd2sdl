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
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_COMMAND,
                                   FD2_INPUT_KEY_SPACE) ==
          FD2_INPUT_ACTION_CONFIRM);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_COMMAND,
                                   FD2_INPUT_KEY_CANCEL) ==
          FD2_INPUT_ACTION_CANCEL);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_SYSTEM_MENU,
                                   FD2_INPUT_KEY_RIGHT) ==
          FD2_INPUT_ACTION_RIGHT);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_MANUAL_SLOT,
                                   FD2_INPUT_KEY_UP) ==
          FD2_INPUT_ACTION_LEFT);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_TARGETING,
                                   FD2_INPUT_KEY_ENTER) ==
          FD2_INPUT_ACTION_CONFIRM);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_TITLE,
                                   FD2_INPUT_KEY_ESCAPE) ==
          FD2_INPUT_ACTION_NONE);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD,
                                   FD2_INPUT_KEY_CANCEL) ==
          FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD,
                                   FD2_INPUT_KEY_ESCAPE) ==
          FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_COMMAND,
                                   FD2_INPUT_KEY_HOME) ==
          FD2_INPUT_ACTION_NONE);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_SYSTEM_MENU,
                                   FD2_INPUT_KEY_ENTER) ==
          FD2_INPUT_ACTION_CONFIRM);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_SYSTEM_MENU,
                                   FD2_INPUT_KEY_ESCAPE) ==
          FD2_INPUT_ACTION_CANCEL);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_MANUAL_SLOT,
                                   FD2_INPUT_KEY_DOWN) ==
          FD2_INPUT_ACTION_RIGHT);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_MANUAL_SLOT,
                                   FD2_INPUT_KEY_LEFT) ==
          FD2_INPUT_ACTION_NONE);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_TARGETING,
                                   FD2_INPUT_KEY_CANCEL) ==
          FD2_INPUT_ACTION_CANCEL);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_TARGETING,
                                   FD2_INPUT_KEY_F2) ==
          FD2_INPUT_ACTION_NONE);
    return 0;
}

static int test_press_exists_for_one_present_frame(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&input, 100);
    CHECK(fd2_input_has_any_key(&input));

    /* 同一呈现帧只消费一次。 */
    fd2_input_event event;
    CHECK(fd2_input_take_key(&input, &event));
    CHECK(event.key == FD2_INPUT_KEY_ENTER && event.repeat == 0);
    CHECK(!fd2_input_take_key(&input, &event));

    /* 下一次 present 会重建脉冲；按键仍按住但未到 repeat 时不再触发。 */
    fd2_input_begin_test_frame(&input, 101);
    CHECK(!fd2_input_has_any_key(&input));
    return 0;
}

static int test_unread_press_does_not_cross_present(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&input, 200);
    CHECK(fd2_input_has_any_key(&input));

    /* 本帧没有交互读取；下一次 present 仍无权重放旧 press。 */
    fd2_input_begin_test_frame(&input, 201);
    CHECK(!fd2_input_has_any_key(&input));
    return 0;
}

static int test_glyph_check_cannot_advance_later_page(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&input, 300);

    /* 逐字阶段只观察当前呈现帧。 */
    CHECK(fd2_input_has_any_key(&input));
    CHECK(fd2_input_has_any_key(&input));

    /* 字形继续渲染会 present 新帧；旧 press 在页面等待前已消失。 */
    fd2_input_begin_test_frame(&input, 301);
    fd2_input_event event;
    CHECK(!fd2_input_take_key(&input, &event));
    fd2_input_begin_test_frame(&input, 799);
    CHECK(!fd2_input_take_key(&input, &event));
    return 0;
}

static int test_typematic_is_frame_based(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_DOWN, 1);
    fd2_input_begin_test_frame(&input, 1000);
    fd2_input_event event;
    CHECK(fd2_input_take_key(&input, &event));
    CHECK(event.key == FD2_INPUT_KEY_DOWN && event.repeat == 0);

    fd2_input_begin_test_frame(
        &input, 1000 + FD2_INPUT_INITIAL_REPEAT_MS - 1);
    CHECK(!fd2_input_take_key(&input, &event));
    fd2_input_begin_test_frame(
        &input, 1000 + FD2_INPUT_INITIAL_REPEAT_MS);
    CHECK(fd2_input_take_key(&input, &event));
    CHECK(event.repeat == 1);
    fd2_input_begin_test_frame(
        &input, 1000 + FD2_INPUT_INITIAL_REPEAT_MS +
                FD2_INPUT_REPEAT_MS - 1);
    CHECK(!fd2_input_take_key(&input, &event));
    fd2_input_begin_test_frame(
        &input, 1000 + FD2_INPUT_INITIAL_REPEAT_MS +
                FD2_INPUT_REPEAT_MS);
    CHECK(fd2_input_take_key(&input, &event));
    CHECK(event.repeat == 1);
    return 0;
}

static int test_release_repress_is_new_press(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&input, 100);
    fd2_input_event event;
    CHECK(fd2_input_take_key(&input, &event));

    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 0);
    fd2_input_begin_test_frame(&input, 101);
    CHECK(!fd2_input_take_key(&input, &event));
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&input, 102);
    CHECK(fd2_input_take_key(&input, &event));
    CHECK(event.repeat == 0);
    return 0;
}

static int test_observe_consumes_current_frame_only(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_SPACE, 1);
    fd2_input_begin_test_frame(&input, 500);
    CHECK(fd2_input_observe_any_key(&input));
    CHECK(!fd2_input_observe_any_key(&input));

    /* 动画 skip 后的下一 present 不能把同一次 press 交给菜单。 */
    fd2_input_begin_test_frame(&input, 501);
    fd2_input_action action;
    CHECK(!fd2_input_take_action(
        &input, FD2_INPUT_CONTEXT_TITLE, &action));
    return 0;
}

static int test_modifier_not_game_key(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_LSHIFT, 1);
    fd2_input_begin_test_frame(&input, 0);
    CHECK(!fd2_input_has_any_key(&input));
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
        test_press_exists_for_one_present_frame() != 0 ||
        test_unread_press_does_not_cross_present() != 0 ||
        test_glyph_check_cannot_advance_later_page() != 0 ||
        test_typematic_is_frame_based() != 0 ||
        test_release_repress_is_new_press() != 0 ||
        test_observe_consumes_current_frame_only() != 0 ||
        test_modifier_not_game_key() != 0 ||
        test_quit_request() != 0)
        return 1;
    puts("input_test: ok");
    return 0;
}
