#include <stdio.h>
#include <string.h>

#include "input.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "check failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return -1; \
    } \
} while (0)

static int test_context_mappings(void) {
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_MOVEMENT,
                                   FD2_INPUT_KEY_ESCAPE) == FD2_INPUT_ACTION_CANCEL);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_MOVEMENT,
                                   FD2_INPUT_KEY_CANCEL) == FD2_INPUT_ACTION_CANCEL);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_MOVEMENT,
                                   FD2_INPUT_KEY_Z) == FD2_INPUT_ACTION_NONE);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_MOVEMENT,
                                   FD2_INPUT_KEY_KEYPAD_5) == FD2_INPUT_ACTION_NONE);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_MOVEMENT,
                                   FD2_INPUT_KEY_ENTER) == FD2_INPUT_ACTION_CONFIRM);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_MOVEMENT,
                                   FD2_INPUT_KEY_LEFT) == FD2_INPUT_ACTION_LEFT);
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
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_COMMAND,
                                   FD2_INPUT_KEY_Z) == FD2_INPUT_ACTION_NONE);
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD_MOVEMENT,
                                   FD2_INPUT_KEY_KEYPAD_5) == FD2_INPUT_ACTION_NONE);
    return 0;
}

static int test_movement_cancel_scancodes(void) {
    const SDL_Scancode keys[] = {
        SDL_SCANCODE_ESCAPE, SDL_SCANCODE_DELETE, SDL_SCANCODE_KP_DECIMAL,
    };
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        fd2_input input;
        fd2_input_init(&input);
        fd2_input_set_test_key_state(&input, keys[i], 1);
        fd2_input_begin_test_frame(&input, 100);
        fd2_input_action action;
        CHECK(fd2_input_take_action(&input, FD2_INPUT_CONTEXT_FIELD_MOVEMENT, &action));
        CHECK(action == FD2_INPUT_ACTION_CANCEL);
        CHECK(!fd2_input_take_action(&input, FD2_INPUT_CONTEXT_FIELD_MOVEMENT, &action));
    }
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

static int test_unread_press_survives_present(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&input, 200);
    CHECK(fd2_input_has_any_key(&input));

    /* 10620只peek；没有原clear，下一present不得丢掉已接受的键。 */
    fd2_input_begin_test_frame(&input, 201);
    CHECK(fd2_input_has_any_key(&input));
    fd2_input_event event;
    CHECK(fd2_input_take_key(&input,&event) && event.key==FD2_INPUT_KEY_ENTER);
    CHECK(!fd2_input_has_any_key(&input));
    return 0;
}

static int test_glyph_peek_preserves_key_for_wait(void) {
    fd2_input input;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&input, 300);

    /* 15f84只观察；它与后继16c57之间没有clear时仍由同一缓存供给。 */
    CHECK(fd2_input_has_any_key(&input));
    CHECK(fd2_input_has_any_key(&input));

    /* 字形继续present不消费；wait消费一次，不将同一键重复送下一页。 */
    fd2_input_begin_test_frame(&input, 301);
    fd2_input_event event;
    CHECK(fd2_input_take_key(&input, &event) && event.key==FD2_INPUT_KEY_ENTER);
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

/* 115b6物理/法术目标共用Esc取消、Z/小键盘5轮询，不套browse的Esc轮询。 */
static int test_target_scancodes(void) {
    const fd2_input_context contexts[] = {
        FD2_INPUT_CONTEXT_FIELD_TARGETING, FD2_INPUT_CONTEXT_FIELD_MAGIC_TARGETING
    };
    const struct { SDL_Scancode key; fd2_input_action action; } cases[] = {
        {SDL_SCANCODE_ESCAPE, FD2_INPUT_ACTION_CANCEL},
        {SDL_SCANCODE_DELETE, FD2_INPUT_ACTION_CANCEL},
        {SDL_SCANCODE_KP_DECIMAL, FD2_INPUT_ACTION_CANCEL},
        {SDL_SCANCODE_Z, FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE},
        {SDL_SCANCODE_KP_5, FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE},
        {SDL_SCANCODE_RETURN, FD2_INPUT_ACTION_CONFIRM},
        {SDL_SCANCODE_SPACE, FD2_INPUT_ACTION_CONFIRM},
        {SDL_SCANCODE_KP_ENTER, FD2_INPUT_ACTION_CONFIRM},
        {SDL_SCANCODE_UP, FD2_INPUT_ACTION_UP},
        {SDL_SCANCODE_DOWN, FD2_INPUT_ACTION_DOWN},
        {SDL_SCANCODE_LEFT, FD2_INPUT_ACTION_LEFT},
        {SDL_SCANCODE_RIGHT, FD2_INPUT_ACTION_RIGHT},
        {SDL_SCANCODE_F2, FD2_INPUT_ACTION_NONE},
    };
    for (size_t c = 0; c < sizeof(contexts) / sizeof(contexts[0]); c++)
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fd2_input input;
        fd2_input_init(&input);
        fd2_input_set_test_key_state(&input, cases[i].key, 1);
        fd2_input_begin_test_frame(&input, 0);
        fd2_input_action action;
        CHECK(fd2_input_take_action(&input, contexts[c], &action));
        CHECK(action == cases[i].action);
        CHECK(!fd2_input_take_action(&input, contexts[c], &action));
    }
    return 0;
}

static int test_detail_any_ordinary_key(void) {
    const SDL_Scancode keys[] = {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_DELETE,
        SDL_SCANCODE_KP_DECIMAL, SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE,
        SDL_SCANCODE_KP_ENTER, SDL_SCANCODE_UP, SDL_SCANCODE_DOWN,
        SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT, SDL_SCANCODE_A,
        SDL_SCANCODE_Z, SDL_SCANCODE_F1, SDL_SCANCODE_F2};
    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        fd2_input input;
        fd2_input_init(&input);
        fd2_input_set_test_key_state(&input, keys[i], 1);
        fd2_input_begin_test_frame(&input, 0);
        fd2_input_action action;
        CHECK(fd2_input_take_action(&input, FD2_INPUT_CONTEXT_FIELD_DETAIL, &action));
        CHECK(action == FD2_INPUT_ACTION_CONFIRM);
        CHECK(!fd2_input_take_action(&input, FD2_INPUT_CONTEXT_FIELD_DETAIL, &action));
    }
    CHECK(fd2_input_action_for_key(FD2_INPUT_CONTEXT_FIELD, FD2_INPUT_KEY_ESCAPE) ==
        FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE); /* browse原义不变。 */
    return 0;
}

static int test_preparation_function_scan(void) {
    const struct { SDL_Scancode modifier,key;uint8_t expected; } cases[]={
        {SDL_SCANCODE_LSHIFT,SDL_SCANCODE_F1,84},{SDL_SCANCODE_RSHIFT,SDL_SCANCODE_F10,93},
        {SDL_SCANCODE_LCTRL,SDL_SCANCODE_F2,95},{SDL_SCANCODE_RCTRL,SDL_SCANCODE_F10,103},
        {SDL_SCANCODE_LALT,SDL_SCANCODE_F3,106},{SDL_SCANCODE_RALT,SDL_SCANCODE_F9,112},
        {SDL_SCANCODE_LSHIFT,SDL_SCANCODE_F11,135},{SDL_SCANCODE_LALT,SDL_SCANCODE_F12,140},
        {SDL_SCANCODE_UNKNOWN,SDL_SCANCODE_F1,59},{SDL_SCANCODE_LSHIFT,SDL_SCANCODE_A,0}};
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        fd2_input input;fd2_input_init(&input);
        fd2_input_set_test_key_state(&input,cases[i].modifier,1);
        fd2_input_set_test_key_state(&input,cases[i].key,1);
        fd2_input_begin_test_frame(&input,0);fd2_input_event event;
        CHECK(fd2_input_take_key(&input,&event));
        CHECK(event.source==cases[i].key && fd2_input_function_scan(&event)==cases[i].expected);
        CHECK(!fd2_input_take_key(&input,&event)); /* modifier不成为独立游戏输入。 */
    }
    CHECK(fd2_input_function_scan(NULL)==0);
    return 0;
}

static int test_function_metadata_belongs_to_queued_event(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input input;fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_F1,1);
    SDL_Event event={0};event.type=SDL_EVENT_KEY_DOWN;event.key.scancode=SDL_SCANCODE_F1;
    event.key.mod=SDL_KMOD_SHIFT;CHECK(SDL_PushEvent(&event));
    /* SDL Windows修正后的键态已经无Shift；DOWN本身仍携原修饰位。 */
    fd2_input_poll_host_events(&input);
    uint64_t first_deadline=input.next_repeat_ms[SDL_SCANCODE_F1];
    fd2_input_begin_test_frame(&input,first_deadline-1u);
    fd2_input_event key;CHECK(fd2_input_take_key(&input,&key) && fd2_input_function_scan(&key)==84);
    fd2_input_begin_test_frame(&input,first_deadline);
    CHECK(fd2_input_take_key(&input,&key) && key.repeat && fd2_input_function_scan(&key)==59);

    fd2_input_init(&input);fd2_input_set_test_key_state(&input,SDL_SCANCODE_F1,1);
    CHECK(SDL_PushEvent(&event));fd2_input_poll_host_events(&input);
    first_deadline=input.next_repeat_ms[SDL_SCANCODE_F1];
    fd2_input_begin_test_frame(&input,first_deadline-2u);
    fd2_input_begin_test_frame(&input,first_deadline-1u);
    CHECK(fd2_input_take_key(&input,&key) && fd2_input_function_scan(&key)==84);
    fd2_input_begin_test_frame(&input,first_deadline);
    CHECK(fd2_input_take_key(&input,&key) && fd2_input_function_scan(&key)==59);

    fd2_input_init(&input);fd2_input_set_test_key_state(&input,SDL_SCANCODE_F1,1);
    CHECK(SDL_PushEvent(&event));event.type=SDL_EVENT_KEY_UP;CHECK(SDL_PushEvent(&event));
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_F1,0);
    fd2_input_poll_host_events(&input);fd2_input_begin_test_frame(&input,0);
    CHECK(fd2_input_take_key(&input,&key) && fd2_input_function_scan(&key)==84);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_F1,1);fd2_input_begin_test_frame(&input,1);
    CHECK(fd2_input_take_key(&input,&key) && fd2_input_function_scan(&key)==59);

    fd2_input_init(&input);fd2_input_set_test_key_state(&input,SDL_SCANCODE_F1,1);
    event.type=SDL_EVENT_KEY_DOWN;CHECK(SDL_PushEvent(&event));
    event.type=SDL_EVENT_WINDOW_FOCUS_LOST;CHECK(SDL_PushEvent(&event));
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_F1,0);
    fd2_input_poll_host_events(&input);fd2_input_begin_test_frame(&input,0);
    CHECK(!fd2_input_take_key(&input,&key));
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_F1,1);fd2_input_begin_test_frame(&input,1);
    CHECK(fd2_input_take_key(&input,&key) && fd2_input_function_scan(&key)==59);
    SDL_Quit();return 0;
}

/* G5先在原G4实现运行这四项真红；复用真实poll/物理测试入口，不注入新API。 */
static int test_buffer_unread_crosses_frames(void) {
    fd2_input input;fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_RETURN,1);
    fd2_input_begin_test_frame(&input,100);
    CHECK(fd2_input_has_any_key(&input));
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_RETURN,0);
    fd2_input_begin_test_frame(&input,116);
    fd2_input_begin_test_frame(&input,132);
    CHECK(fd2_input_has_any_key(&input));
    fd2_input_event event;
    CHECK(fd2_input_take_key(&input,&event) && event.key==FD2_INPUT_KEY_ENTER && !event.repeat);
    CHECK(!fd2_input_take_key(&input,&event));
    return 0;
}

static int buffer_push_key(Uint32 type,SDL_Scancode source,SDL_Keymod modifiers) {
    SDL_Event event={0};event.type=type;event.key.scancode=source;event.key.mod=modifiers;
    return SDL_PushEvent(&event);
}

static int test_buffer_preserves_arrival_order(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input input;fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_RIGHT,1);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_RETURN,1);
    CHECK(buffer_push_key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_RIGHT,SDL_KMOD_NONE));
    CHECK(buffer_push_key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_RETURN,SDL_KMOD_NONE));
    fd2_input_poll_host_events(&input);
    fd2_input_begin_test_frame(&input,SDL_GetTicks());
    fd2_input_event event;
    CHECK(fd2_input_take_key(&input,&event) && event.key==FD2_INPUT_KEY_RIGHT);
    CHECK(fd2_input_take_key(&input,&event) && event.key==FD2_INPUT_KEY_ENTER);
    CHECK(!fd2_input_take_key(&input,&event));
    SDL_Quit();return 0;
}

static int test_buffer_short_press_between_presents(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input input;fd2_input_init(&input);
    fd2_input_begin_test_frame(&input,SDL_GetTicks());
    CHECK(buffer_push_key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_RETURN,SDL_KMOD_NONE));
    CHECK(buffer_push_key(SDL_EVENT_KEY_UP,SDL_SCANCODE_RETURN,SDL_KMOD_NONE));
    fd2_input_poll_host_events(&input);
    fd2_input_begin_test_frame(&input,SDL_GetTicks());
    fd2_input_event event;
    CHECK(fd2_input_take_key(&input,&event) && event.key==FD2_INPUT_KEY_ENTER && !event.repeat);
    CHECK(!fd2_input_take_key(&input,&event));
    SDL_Quit();return 0;
}

static int test_buffer_one_read_keeps_later_key(void) {
    fd2_input input;fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_A,1);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_RETURN,1);
    fd2_input_begin_test_frame(&input,100);
    fd2_input_event event;
    CHECK(fd2_input_take_key(&input,&event) && event.source==SDL_SCANCODE_A);
    CHECK(fd2_input_has_any_key(&input) && fd2_input_has_any_key(&input));
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_A,0);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_RETURN,0);
    fd2_input_begin_test_frame(&input,116);
    CHECK(fd2_input_take_key(&input,&event) && event.key==FD2_INPUT_KEY_ENTER);
    CHECK(!fd2_input_take_key(&input,&event));
    return 0;
}

static int buffer_regression(const char *name) {
    if(!strcmp(name,"cross-frame"))return test_buffer_unread_crosses_frames();
    if(!strcmp(name,"arrival-order"))return test_buffer_preserves_arrival_order();
    if(!strcmp(name,"between-presents"))return test_buffer_short_press_between_presents();
    if(!strcmp(name,"one-read"))return test_buffer_one_read_keeps_later_key();
    return -1;
}

static int test_clear_preserves_held_repeat_and_quit(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input input;fd2_input_init(&input);input.quit_requested=1;
    uint64_t now=SDL_GetTicks();
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_RETURN,1);
    fd2_input_begin_test_frame(&input,now);
    uint64_t deadline=input.next_repeat_ms[SDL_SCANCODE_RETURN];
    CHECK(fd2_input_has_any_key(&input));
    fd2_input_clear_keys(&input);
    CHECK(!fd2_input_has_any_key(&input) && input.previous_down[SDL_SCANCODE_RETURN]);
    CHECK(input.next_repeat_ms[SDL_SCANCODE_RETURN]==deadline && fd2_input_take_quit(&input));
    fd2_input_begin_test_frame(&input,deadline-1u);
    CHECK(!fd2_input_has_any_key(&input));
    fd2_input_begin_test_frame(&input,deadline);
    fd2_input_event event;
    CHECK(fd2_input_take_key(&input,&event) && event.repeat && event.key==FD2_INPUT_KEY_ENTER);
    CHECK(!fd2_input_take_key(&input,&event) && fd2_input_take_quit(&input));
    fd2_input_clear_keys(NULL);SDL_Quit();return 0;
}

static int test_clear_cuts_off_already_posted_down_without_recreating_press(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input input;fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_RETURN,1);
    CHECK(buffer_push_key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_RETURN,SDL_KMOD_NONE));
    CHECK(!fd2_input_has_any_key(&input));
    fd2_input_clear_keys(&input);
    uint64_t deadline=input.next_repeat_ms[SDL_SCANCODE_RETURN];
    CHECK(!fd2_input_has_any_key(&input) && input.previous_down[SDL_SCANCODE_RETURN]);
    fd2_input_begin_test_frame(&input,deadline-1u);
    CHECK(!fd2_input_has_any_key(&input));
    fd2_input_begin_test_frame(&input,deadline);
    fd2_input_event event;
    CHECK(fd2_input_take_key(&input,&event) && event.repeat);
    CHECK(!fd2_input_take_key(&input,&event));
    SDL_Quit();return 0;
}

static int test_buffer_wrap_and_full_drop_new_keep_old_order(void) {
    fd2_input input;fd2_input_init(&input);
    uint64_t now=0;
    CHECK(FD2_INPUT_KEY_QUEUE_CAPACITY==15u);
    for(unsigned i=0;i<FD2_INPUT_KEY_QUEUE_CAPACITY;i++) {
        SDL_Scancode source=(SDL_Scancode)(SDL_SCANCODE_A+i);
        fd2_input_set_test_key_state(&input,source,1);fd2_input_begin_test_frame(&input,now++);
        fd2_input_set_test_key_state(&input,source,0);fd2_input_begin_test_frame(&input,now++);
    }
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_RETURN,1);
    fd2_input_begin_test_frame(&input,now++); /* 满队新确认不得覆盖旧项。 */
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_RETURN,0);
    fd2_input_begin_test_frame(&input,now++);
    CHECK(input.queued_count==15u);
    fd2_input_event event;
    for(unsigned i=0;i<7u;i++) {
        CHECK(fd2_input_take_key(&input,&event) && event.source==(SDL_Scancode)(SDL_SCANCODE_A+i));
    }
    for(unsigned i=15u;i<22u;i++) {
        SDL_Scancode source=(SDL_Scancode)(SDL_SCANCODE_A+i);
        fd2_input_set_test_key_state(&input,source,1);fd2_input_begin_test_frame(&input,now++);
        fd2_input_set_test_key_state(&input,source,0);fd2_input_begin_test_frame(&input,now++);
    }
    CHECK(input.queued_count==15u);
    for(unsigned i=7u;i<22u;i++) {
        CHECK(fd2_input_take_key(&input,&event) && event.source==(SDL_Scancode)(SDL_SCANCODE_A+i) && !event.repeat);
    }
    CHECK(!fd2_input_take_key(&input,&event));
    return 0;
}

static int test_host_repeat_is_not_a_second_repeat_source(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input input;fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_DOWN,1);
    CHECK(buffer_push_key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_DOWN,SDL_KMOD_NONE));
    SDL_Event repeated={0};repeated.type=SDL_EVENT_KEY_DOWN;
    repeated.key.scancode=SDL_SCANCODE_DOWN;repeated.key.repeat=true;
    CHECK(SDL_PushEvent(&repeated));CHECK(SDL_PushEvent(&repeated));
    fd2_input_poll_host_events(&input);
    uint64_t deadline=input.next_repeat_ms[SDL_SCANCODE_DOWN];
    fd2_input_event event;
    CHECK(fd2_input_take_key(&input,&event) && !event.repeat);
    CHECK(!fd2_input_take_key(&input,&event));
    fd2_input_begin_test_frame(&input,deadline);
    CHECK(fd2_input_take_key(&input,&event) && event.repeat);
    fd2_input_begin_test_frame(&input,deadline);
    CHECK(!fd2_input_take_key(&input,&event));
    fd2_input_begin_test_frame(&input,deadline+10000u);
    CHECK(fd2_input_take_key(&input,&event) && event.repeat);
    CHECK(!fd2_input_take_key(&input,&event)); /* 无while追赶重放。 */
    SDL_Quit();return 0;
}

static int test_peek_take_never_pump_unreceived_host_events(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input input;fd2_input_init(&input);
    fd2_input_event event;
    CHECK(buffer_push_key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_RETURN,SDL_KMOD_NONE));
    CHECK(!fd2_input_has_any_key(&input) && !fd2_input_take_key(&input,&event));
    fd2_input_poll_host_events(&input);
    CHECK(fd2_input_has_any_key(&input) && fd2_input_has_any_key(&input));
    CHECK(fd2_input_take_key(&input,&event) && event.key==FD2_INPUT_KEY_ENTER);
    CHECK(!fd2_input_take_key(&input,&event));
    SDL_Quit();return 0;
}

static int test_focus_lost_clears_queue_and_physical_lifecycle_not_quit(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input input;fd2_input_init(&input);input.quit_requested=1;
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_DOWN,1);
    fd2_input_begin_test_frame(&input,SDL_GetTicks());
    CHECK(fd2_input_has_any_key(&input));
    SDL_Event lost={0};lost.type=SDL_EVENT_WINDOW_FOCUS_LOST;CHECK(SDL_PushEvent(&lost));
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_DOWN,0);
    fd2_input_poll_host_events(&input);
    CHECK(!fd2_input_has_any_key(&input) && !input.previous_down[SDL_SCANCODE_DOWN] &&
        !input.next_repeat_ms[SDL_SCANCODE_DOWN] && fd2_input_take_quit(&input));
    SDL_Quit();return 0;
}

static int test_focus_lost_does_not_requeue_stale_down_in_same_poll(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input input;fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_DOWN,1);
    fd2_input_begin_test_frame(&input,100);
    SDL_Event lost={0};lost.type=SDL_EVENT_WINDOW_FOCUS_LOST;CHECK(SDL_PushEvent(&lost));
    /* 此处故意仍保持DOWN；不依赖fake调用者先清物理状态才让测试通过。 */
    fd2_input_poll_host_events(&input);
    CHECK(!fd2_input_has_any_key(&input) && !input.previous_down[SDL_SCANCODE_DOWN] &&
        !input.next_repeat_ms[SDL_SCANCODE_DOWN]);
    /* 当前原生Windows SDL在LOST前ResetKeyboard；下一轮按实际UP域继续。 */
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_DOWN,0);
    fd2_input_poll_host_events(&input);
    CHECK(!fd2_input_has_any_key(&input));
    SDL_Quit();return 0;
}

static int test_explicit_new_down_after_focus_lost_is_retained(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input input;fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_DOWN,1);
    fd2_input_begin_test_frame(&input,100);
    SDL_Event lost={0};lost.type=SDL_EVENT_WINDOW_FOCUS_LOST;CHECK(SDL_PushEvent(&lost));
    CHECK(buffer_push_key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_RIGHT,SDL_KMOD_NONE));
    fd2_input_poll_host_events(&input);
    fd2_input_event event;
    CHECK(fd2_input_take_key(&input,&event) && event.key==FD2_INPUT_KEY_RIGHT && !event.repeat);
    CHECK(!fd2_input_take_key(&input,&event) && !input.previous_down[SDL_SCANCODE_DOWN]);
    CHECK(input.previous_down[SDL_SCANCODE_RIGHT]);
    SDL_Quit();return 0;
}

static int test_two_short_taps_and_unknown_action_each_consume_one(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input input;fd2_input_init(&input);
    for(unsigned i=0;i<2u;i++) {
        CHECK(buffer_push_key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_RETURN,SDL_KMOD_NONE));
        CHECK(buffer_push_key(SDL_EVENT_KEY_UP,SDL_SCANCODE_RETURN,SDL_KMOD_NONE));
    }
    fd2_input_poll_host_events(&input);
    fd2_input_action action;
    CHECK(fd2_input_take_action(&input,FD2_INPUT_CONTEXT_CHOICE,&action) && action==FD2_INPUT_ACTION_CONFIRM);
    CHECK(fd2_input_take_action(&input,FD2_INPUT_CONTEXT_FIELD_DETAIL,&action) && action==FD2_INPUT_ACTION_CONFIRM);
    CHECK(!fd2_input_take_action(&input,FD2_INPUT_CONTEXT_FIELD,&action));
    fd2_input_init(&input);
    CHECK(buffer_push_key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_A,SDL_KMOD_NONE));
    CHECK(buffer_push_key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_RETURN,SDL_KMOD_NONE));
    fd2_input_poll_host_events(&input);
    CHECK(fd2_input_take_action(&input,FD2_INPUT_CONTEXT_TITLE,&action) && action==FD2_INPUT_ACTION_NONE);
    CHECK(fd2_input_take_action(&input,FD2_INPUT_CONTEXT_CHOICE,&action) && action==FD2_INPUT_ACTION_CONFIRM);
    CHECK(!fd2_input_take_action(&input,FD2_INPUT_CONTEXT_CHOICE,&action));
    SDL_Quit();return 0;
}

static int test_ctrl_c_quit_never_becomes_physical_fallback_other(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input input;fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_A,1);
    fd2_input_begin_test_frame(&input,0);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_A,0);
    fd2_input_begin_test_frame(&input,1);
    CHECK(input.queued_count==1u);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_C,1);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LCTRL,1);
    CHECK(buffer_push_key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_C,SDL_KMOD_CTRL));
    fd2_input_poll_host_events(&input);
    CHECK(fd2_input_take_quit(&input) && input.queued_count==1u);
    fd2_input_event event;
    CHECK(fd2_input_take_key(&input,&event) && event.source==SDL_SCANCODE_A);
    CHECK(!fd2_input_take_key(&input,&event)); /* 不靠清掉已有普通键掩盖多入队。 */
    fd2_input_begin_test_frame(&input,1000);
    CHECK(!fd2_input_has_any_key(&input) && fd2_input_take_quit(&input));
    SDL_Quit();return 0;
}

static int test_lock_state_keys_never_ack_or_repeat(void) {
    const SDL_Scancode locks[]={SDL_SCANCODE_CAPSLOCK,
        SDL_SCANCODE_NUMLOCKCLEAR,SDL_SCANCODE_SCROLLLOCK};
    for(unsigned i=0;i<sizeof(locks)/sizeof(locks[0]);i++) {
        fd2_input input;fd2_input_init(&input);
        fd2_input_set_test_key_state(&input,locks[i],1);
        fd2_input_begin_test_frame(&input,100);
        CHECK(!fd2_input_has_any_key(&input) && !input.previous_down[locks[i]] &&
            !input.next_repeat_ms[locks[i]]);
        fd2_input_begin_test_frame(&input,700);
        fd2_input_action action;
        CHECK(!fd2_input_take_action(&input,FD2_INPUT_CONTEXT_FIELD_DETAIL,&action));
        CHECK(!fd2_input_take_action(&input,FD2_INPUT_CONTEXT_CHOICE,&action));
        fd2_input_set_test_key_state(&input,locks[i],0);
        fd2_input_begin_test_frame(&input,800);
        CHECK(!fd2_input_has_any_key(&input));
    }
    return 0;
}

static int test_polled_lock_keys_preserve_ordinary_queue_without_ack(void) {
    const SDL_Scancode locks[]={SDL_SCANCODE_CAPSLOCK,
        SDL_SCANCODE_NUMLOCKCLEAR,SDL_SCANCODE_SCROLLLOCK};
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input input;fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_A,1);
    fd2_input_begin_test_frame(&input,0);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_A,0);
    fd2_input_begin_test_frame(&input,1);
    for(unsigned i=0;i<sizeof(locks)/sizeof(locks[0]);i++) {
        /* DOWN/UP事件与仍DOWN的物理快照分别覆盖两个producer。 */
        fd2_input_set_test_key_state(&input,locks[i],1);
        CHECK(buffer_push_key(SDL_EVENT_KEY_DOWN,locks[i],SDL_KMOD_NONE));
        CHECK(buffer_push_key(SDL_EVENT_KEY_UP,locks[i],SDL_KMOD_NONE));
    }
    fd2_input_poll_host_events(&input);
    CHECK(input.queued_count==1u);
    fd2_input_event event;
    CHECK(fd2_input_take_key(&input,&event) && event.source==SDL_SCANCODE_A);
    CHECK(!fd2_input_take_key(&input,&event));
    fd2_input_begin_test_frame(&input,1000);
    CHECK(!fd2_input_has_any_key(&input));
    SDL_Quit();return 0;
}

int main(int argc,char **argv) {
    if(argc==3 && !strcmp(argv[1],"--buffer-case"))return buffer_regression(argv[2])?1:0;
    if(argc!=1)return 1;
    if (test_context_mappings() != 0 ||
        test_movement_cancel_scancodes() != 0 ||
        test_target_scancodes() != 0 ||
        test_detail_any_ordinary_key() != 0 ||
        test_press_exists_for_one_present_frame() != 0 ||
        test_unread_press_survives_present() != 0 ||
        test_glyph_peek_preserves_key_for_wait() != 0 ||
        test_typematic_is_frame_based() != 0 ||
        test_release_repress_is_new_press() != 0 ||
        test_modifier_not_game_key() != 0 ||
        test_preparation_function_scan() != 0 ||
        test_function_metadata_belongs_to_queued_event() != 0 ||
        test_quit_request() != 0 ||
        test_buffer_unread_crosses_frames() != 0 ||
        test_buffer_preserves_arrival_order() != 0 ||
        test_buffer_short_press_between_presents() != 0 ||
        test_buffer_one_read_keeps_later_key() != 0 ||
        test_clear_preserves_held_repeat_and_quit() != 0 ||
        test_clear_cuts_off_already_posted_down_without_recreating_press() != 0 ||
        test_buffer_wrap_and_full_drop_new_keep_old_order() != 0 ||
        test_host_repeat_is_not_a_second_repeat_source() != 0 ||
        test_peek_take_never_pump_unreceived_host_events() != 0 ||
        test_focus_lost_clears_queue_and_physical_lifecycle_not_quit() != 0 ||
        test_focus_lost_does_not_requeue_stale_down_in_same_poll() != 0 ||
        test_explicit_new_down_after_focus_lost_is_retained() != 0 ||
        test_two_short_taps_and_unknown_action_each_consume_one() != 0 ||
        test_ctrl_c_quit_never_becomes_physical_fallback_other() != 0 ||
        test_lock_state_keys_never_ack_or_repeat() != 0 ||
        test_polled_lock_keys_preserve_ordinary_queue_without_ack() != 0)
        return 1;
    puts("input_test: ok");
    return 0;
}
