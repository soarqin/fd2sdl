#include <stdio.h>
#include <string.h>

#include "input.h"
#include "settings.h"
#include "bindings.h"

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

static int test_typematic_integer_millisecond_boundaries(void) {
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
    _Static_assert(FD2_INPUT_KEY_QUEUE_CAPACITY==15u, "FD2_INPUT_KEY_QUEUE_CAPACITY==15u");
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
    /* 原硬件1ms handler在渲染停顿时继续生产；满BIOS队列丢新。
     * 这不同于visual helper每次调用只推进一步。 */
    for(unsigned i=0;i<FD2_INPUT_KEY_QUEUE_CAPACITY;i++)
        CHECK(fd2_input_take_key(&input,&event) && event.repeat);
    CHECK(!fd2_input_take_key(&input,&event));
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
    /* 与Windows Shift修正同类：DOWN有Ctrl，末尾物理快照已无Ctrl。
     * HOST组合不能在fallback或后续repeat中变成普通C。 */
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_C,1);
    CHECK(buffer_push_key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_C,SDL_KMOD_CTRL));
    fd2_input_poll_host_events(&input);fd2_input_begin_test_frame(&input,1000);
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
        CHECK(!fd2_input_has_any_key(&input) && input.previous_down[locks[i]] &&
            input.repeat_key==locks[i]); /* 不入FIFO，但make参与last-key。 */
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

/* 预期取自固定b6abbd5 BIOS AX表与增强键分支，不从待测mapper生成。 */
static int test_bios_ax_and_modified_game_keys(void) {
    const struct {SDL_Scancode key,modifier;uint16_t ax;fd2_input_key game;} cases[]={
        {SDL_SCANCODE_A,SDL_SCANCODE_UNKNOWN,0x1e61,FD2_INPUT_KEY_OTHER},
        {SDL_SCANCODE_A,SDL_SCANCODE_LSHIFT,0x1e41,FD2_INPUT_KEY_OTHER},
        {SDL_SCANCODE_A,SDL_SCANCODE_RCTRL,0x1e01,FD2_INPUT_KEY_OTHER},
        {SDL_SCANCODE_A,SDL_SCANCODE_LALT,0x1e00,FD2_INPUT_KEY_OTHER},
        {SDL_SCANCODE_UP,SDL_SCANCODE_UNKNOWN,0x48e0,FD2_INPUT_KEY_UP},
        {SDL_SCANCODE_UP,SDL_SCANCODE_LSHIFT,0x48e0,FD2_INPUT_KEY_UP},
        {SDL_SCANCODE_UP,SDL_SCANCODE_LCTRL,0x8de0,FD2_INPUT_KEY_OTHER},
        {SDL_SCANCODE_UP,SDL_SCANCODE_RALT,0x9800,FD2_INPUT_KEY_OTHER},
        {SDL_SCANCODE_F1,SDL_SCANCODE_LSHIFT,0x5400,FD2_INPUT_KEY_OTHER},
        {SDL_SCANCODE_F2,SDL_SCANCODE_LALT,0x6900,FD2_INPUT_KEY_OTHER},
        {SDL_SCANCODE_RETURN,SDL_SCANCODE_LCTRL,0x1c0a,FD2_INPUT_KEY_ENTER},
        {SDL_SCANCODE_KP_ENTER,SDL_SCANCODE_LCTRL,0xe00a,FD2_INPUT_KEY_KEYPAD_CONFIRM},
        {SDL_SCANCODE_KP_ENTER,SDL_SCANCODE_LALT,0xa600,FD2_INPUT_KEY_OTHER},
        {SDL_SCANCODE_KP_DIVIDE,SDL_SCANCODE_UNKNOWN,0xe02f,FD2_INPUT_KEY_KEYPAD_CONFIRM},
        {SDL_SCANCODE_KP_DIVIDE,SDL_SCANCODE_LCTRL,0x9500,FD2_INPUT_KEY_OTHER},
        {SDL_SCANCODE_2,SDL_SCANCODE_LCTRL,0x0300,FD2_INPUT_KEY_OTHER},
        {SDL_SCANCODE_TAB,SDL_SCANCODE_LCTRL,0x9400,FD2_INPUT_KEY_OTHER},
        {SDL_SCANCODE_TAB,SDL_SCANCODE_LSHIFT,0x0f00,FD2_INPUT_KEY_OTHER},
        {SDL_SCANCODE_BACKSPACE,SDL_SCANCODE_LALT,0x0e00,FD2_INPUT_KEY_OTHER},
        {SDL_SCANCODE_KP_8,SDL_SCANCODE_LCTRL,0x8d00,FD2_INPUT_KEY_OTHER},
        {SDL_SCANCODE_KP_8,SDL_SCANCODE_LSHIFT,0x4838,FD2_INPUT_KEY_UP},
        {SDL_SCANCODE_KP_DECIMAL,SDL_SCANCODE_LSHIFT,0x532e,FD2_INPUT_KEY_CANCEL},
        {SDL_SCANCODE_NONUSBACKSLASH,SDL_SCANCODE_UNKNOWN,0x565c,FD2_INPUT_KEY_OTHER},
    };
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        fd2_input input;fd2_input_init(&input);
        fd2_input_set_test_key_state(&input,cases[i].modifier,1);
        fd2_input_set_test_key_state(&input,cases[i].key,1);
        fd2_input_begin_test_frame(&input,0);fd2_input_event event;
        CHECK(fd2_input_take_key(&input,&event));
        CHECK(event.bios_code==cases[i].ax && event.key==cases[i].game);
        CHECK(!fd2_input_take_key(&input,&event));
    }
    const struct {SDL_Scancode key,modifier;} absent[]={
        {SDL_SCANCODE_RETURN,SDL_SCANCODE_LALT},
        {SDL_SCANCODE_TAB,SDL_SCANCODE_LALT},
        {SDL_SCANCODE_1,SDL_SCANCODE_LCTRL},
        {SDL_SCANCODE_SEMICOLON,SDL_SCANCODE_LCTRL},
        {SDL_SCANCODE_NONUSBACKSLASH,SDL_SCANCODE_LALT}};
    for(unsigned i=0;i<sizeof(absent)/sizeof(absent[0]);i++) {
        fd2_input input;fd2_input_init(&input);
        fd2_input_set_test_key_state(&input,absent[i].modifier,1);
        fd2_input_set_test_key_state(&input,absent[i].key,1);
        fd2_input_begin_test_frame(&input,0);
        CHECK(!fd2_input_has_any_key(&input));
    }
    return 0;
}

static int test_single_last_key_and_release_never_resumes_old_key(void) {
    fd2_input input;fd2_input_init(&input);fd2_input_event event;
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_A,1);
    fd2_input_begin_test_frame(&input,0);
    CHECK(fd2_input_take_key(&input,&event) && event.source==SDL_SCANCODE_A);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_B,1);
    fd2_input_begin_test_frame(&input,200);
    CHECK(fd2_input_take_key(&input,&event) && event.source==SDL_SCANCODE_B);
    CHECK(!input.next_repeat_ms[SDL_SCANCODE_A]);
    fd2_input_begin_test_frame(&input,500);CHECK(!fd2_input_has_any_key(&input));
    fd2_input_begin_test_frame(&input,700);
    CHECK(fd2_input_take_key(&input,&event) && event.repeat && event.source==SDL_SCANCODE_B);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_B,0);
    fd2_input_begin_test_frame(&input,701);
    fd2_input_begin_test_frame(&input,2000);
    CHECK(input.previous_down[SDL_SCANCODE_A] && input.repeat_key==SDL_SCANCODE_UNKNOWN);
    CHECK(!fd2_input_has_any_key(&input));
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_B,1);
    fd2_input_begin_test_frame(&input,2001);
    CHECK(fd2_input_take_key(&input,&event) && !event.repeat);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_A,0);
    fd2_input_begin_test_frame(&input,2002);
    fd2_input_begin_test_frame(&input,2501);
    CHECK(fd2_input_take_key(&input,&event) && event.source==SDL_SCANCODE_B && event.repeat);
    CHECK(!fd2_input_has_any_key(&input));
    return 0;
}

static int test_modifier_make_arbitrates_but_is_not_an_ack(void) {
    fd2_input input;fd2_input_init(&input);fd2_input_event event;
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_DOWN,1);
    fd2_input_begin_test_frame(&input,0);CHECK(fd2_input_take_key(&input,&event));
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LSHIFT,1);
    fd2_input_begin_test_frame(&input,100);
    fd2_input_begin_test_frame(&input,700);
    CHECK(input.repeat_key==SDL_SCANCODE_LSHIFT && !fd2_input_has_any_key(&input));
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LSHIFT,0);
    fd2_input_begin_test_frame(&input,701);
    fd2_input_begin_test_frame(&input,3000);
    CHECK(input.previous_down[SDL_SCANCODE_DOWN] && !fd2_input_has_any_key(&input));
    return 0;
}

static int test_repeat_runs_while_consumer_waits_and_keeps_tick_phase(void) {
    fd2_input input;fd2_input_init(&input);fd2_input_event event;
    _Static_assert(FD2_INPUT_INITIAL_REPEAT_MS==500 && FD2_INPUT_REPEAT_MS==33, "FD2_INPUT_INITIAL_REPEAT_MS==500 && FD2_INPUT_REPEAT_MS==33");
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_DOWN,1);
    fd2_input_begin_test_frame(&input,100);CHECK(fd2_input_take_key(&input,&event));
    fd2_input_begin_test_frame(&input,1600);
    CHECK(input.queued_count==15u && input.next_repeat_ms[SDL_SCANCODE_DOWN]==1623u);
    for(unsigned i=0;i<15u;i++)CHECK(fd2_input_take_key(&input,&event) && event.repeat);
    fd2_input_begin_test_frame(&input,1622);CHECK(!fd2_input_has_any_key(&input));
    fd2_input_begin_test_frame(&input,1623);
    CHECK(fd2_input_take_key(&input,&event) && event.repeat);
    fd2_input_begin_test_frame(&input,UINT64_MAX);
    CHECK(input.queued_count==15u); /* 仍为有界FIFO，未逐毫秒循环。 */
    return 0;
}

static int test_initial_repeat_deadline_overflow_keeps_first_make(void) {
    const uint64_t remaining[]={0,1,499,500,501};
    for(size_t i=0;i<sizeof(remaining)/sizeof(*remaining);i++) {
        fd2_input input;fd2_input_init(&input);fd2_input_event event;
        uint64_t start=UINT64_MAX-remaining[i];
        fd2_input_set_test_key_state(&input,SDL_SCANCODE_A,1);
        fd2_input_begin_test_frame(&input,start);
        CHECK(fd2_input_take_key(&input,&event) && !event.repeat && event.bios_code==0x1e61);
        CHECK(input.previous_down[SDL_SCANCODE_A]);
        fd2_input_begin_test_frame(&input,start);
        CHECK(!fd2_input_has_any_key(&input));
        fd2_input_begin_test_frame(&input,UINT64_MAX);
        if(remaining[i]>=500u)
            CHECK(fd2_input_take_key(&input,&event) && event.repeat && event.bios_code==0x1e61);
        CHECK(!fd2_input_has_any_key(&input) && input.repeat_key==SDL_SCANCODE_UNKNOWN);
        CHECK(!input.next_repeat_ms[SDL_SCANCODE_A] && input.previous_down[SDL_SCANCODE_A]);
        fd2_input_begin_test_frame(&input,UINT64_MAX);
        CHECK(!fd2_input_has_any_key(&input));
    }
    return 0;
}

static int test_lock_break_and_keypad_num_shift_xor(void) {
    fd2_input input;fd2_input_init(&input);fd2_input_event event;
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_CAPSLOCK,1);
    fd2_input_begin_test_frame(&input,0);
    CHECK(!(input.lock_modifiers&SDL_KMOD_CAPS));
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_CAPSLOCK,0);
    fd2_input_begin_test_frame(&input,1);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_A,1);
    fd2_input_begin_test_frame(&input,2);
    CHECK(fd2_input_take_key(&input,&event) && event.bios_code==0x1e41);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_A,0);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LSHIFT,1);
    fd2_input_begin_test_frame(&input,3);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_A,1);
    fd2_input_begin_test_frame(&input,4);
    CHECK(fd2_input_take_key(&input,&event) && event.bios_code==0x1e61);
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_NUMLOCKCLEAR,1);
    fd2_input_begin_test_frame(&input,0);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_NUMLOCKCLEAR,0);
    fd2_input_begin_test_frame(&input,1);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_KP_8,1);
    fd2_input_begin_test_frame(&input,2);
    CHECK(fd2_input_take_key(&input,&event) && event.bios_code==0x4838);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_KP_8,0);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LSHIFT,1);
    fd2_input_begin_test_frame(&input,3);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_KP_8,1);
    fd2_input_begin_test_frame(&input,4);
    CHECK(fd2_input_take_key(&input,&event) && event.bios_code==0x4800);
    return 0;
}

static int test_alt_keypad_byte_token_and_both_alt_release(void) {
    fd2_input input;fd2_input_init(&input);fd2_input_event event;
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LALT,1);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_RALT,1);
    fd2_input_begin_test_frame(&input,0);
    const SDL_Scancode digits[]={SDL_SCANCODE_KP_7,SDL_SCANCODE_KP_8};
    for(unsigned i=0;i<2u;i++) {
        fd2_input_set_test_key_state(&input,digits[i],1);fd2_input_begin_test_frame(&input,1u+2u*i);
        fd2_input_set_test_key_state(&input,digits[i],0);fd2_input_begin_test_frame(&input,2u+2u*i);
    }
    CHECK(input.alt_token==78 && !fd2_input_has_any_key(&input));
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_RALT,0);fd2_input_begin_test_frame(&input,5);
    CHECK(!fd2_input_has_any_key(&input));
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LALT,0);fd2_input_begin_test_frame(&input,6);
    CHECK(fd2_input_take_key(&input,&event) && event.bios_code==78 && !event.repeat);
    CHECK(!fd2_input_has_any_key(&input) && !input.alt_token);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LALT,1);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_KP_7,1);
    fd2_input_begin_test_frame(&input,7);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_KP_7,0);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_KP_DECIMAL,1);
    fd2_input_begin_test_frame(&input,8);
    CHECK(input.alt_token==70 && !fd2_input_has_any_key(&input));
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_KP_DECIMAL,0);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LALT,0);
    fd2_input_begin_test_frame(&input,9);
    CHECK(fd2_input_take_key(&input,&event) && event.bios_code==70);
    /* 长gap的Alt数字token仍须等价于全部硬件repeat，而不是只保第一项。 */
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LALT,1);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_KP_7,1);
    fd2_input_begin_test_frame(&input,10);
    fd2_input_begin_test_frame(&input,1000000);
    unsigned expected=0;
    for(unsigned i=0;i<8u;i++)expected=(10u*expected+7u)&255u;
    CHECK(input.alt_token==expected && !fd2_input_has_any_key(&input));
    return 0;
}

static int test_title_uses_original_ax_not_physical_confirm(void) {
    const struct {SDL_Scancode key,modifier;fd2_input_action action;} cases[]={
        {SDL_SCANCODE_RETURN,SDL_SCANCODE_LCTRL,FD2_INPUT_ACTION_NONE},
        {SDL_SCANCODE_M,SDL_SCANCODE_LCTRL,FD2_INPUT_ACTION_CONFIRM}, /* AL=0d */
        {SDL_SCANCODE_KP_0,SDL_SCANCODE_UNKNOWN,FD2_INPUT_ACTION_CONFIRM}, /* AH52 */
        {SDL_SCANCODE_KP_0,SDL_SCANCODE_LSHIFT,FD2_INPUT_ACTION_CONFIRM},
        {SDL_SCANCODE_KP_DIVIDE,SDL_SCANCODE_UNKNOWN,FD2_INPUT_ACTION_CONFIRM}, /* AHe0 */
        {SDL_SCANCODE_UP,SDL_SCANCODE_LCTRL,FD2_INPUT_ACTION_NONE},
        {SDL_SCANCODE_SPACE,SDL_SCANCODE_LALT,FD2_INPUT_ACTION_CONFIRM}};
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        fd2_input input;fd2_input_init(&input);fd2_input_action action;
        fd2_input_set_test_key_state(&input,cases[i].modifier,1);
        fd2_input_set_test_key_state(&input,cases[i].key,1);
        fd2_input_begin_test_frame(&input,0);
        CHECK(fd2_input_take_action(&input,FD2_INPUT_CONTEXT_TITLE,&action));
        CHECK(action==cases[i].action && !fd2_input_has_any_key(&input));
    }
    return 0;
}

static int test_all_function_keys_and_modifier_priority(void) {
    const SDL_Scancode modifiers[]={SDL_SCANCODE_UNKNOWN,SDL_SCANCODE_LSHIFT,
        SDL_SCANCODE_LCTRL,SDL_SCANCODE_LALT};
    const unsigned first10[]={59,84,94,104},last2[]={133,135,137,139};
    for(unsigned key=0;key<24u;key++)for(unsigned mod=0;mod<4u;mod++) {
        fd2_input input;fd2_input_init(&input);fd2_input_event event;
        SDL_Scancode source=(SDL_Scancode)(key<12u?SDL_SCANCODE_F1+key:SDL_SCANCODE_F13+key-12u);
        fd2_input_set_test_key_state(&input,modifiers[mod],1);
        fd2_input_set_test_key_state(&input,source,1);fd2_input_begin_test_frame(&input,0);
        unsigned index=key%12u,effective=key>=12u && mod<2u?1u:mod;
        unsigned scan=index<10u?first10[effective]+index:last2[effective]+index-10u;
        CHECK(fd2_input_take_key(&input,&event));
        CHECK(event.bios_code==(uint16_t)(scan<<8) && fd2_input_function_scan(&event)==scan);
        CHECK(!fd2_input_has_any_key(&input));
    }
    fd2_input input;fd2_input_init(&input);fd2_input_event event;
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LALT,1);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_RCTRL,1);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_RSHIFT,1);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_F1,1);
    fd2_input_begin_test_frame(&input,0);
    CHECK(fd2_input_take_key(&input,&event) && event.bios_code==0x6800);
    return 0;
}

static int test_hardware_extra_keys_and_fake_shift(void) {
    fd2_input input;fd2_input_init(&input);fd2_input_event event;
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LGUI,1);
    fd2_input_begin_test_frame(&input,0);
    CHECK(fd2_input_take_key(&input,&event) && event.bios_code==0xd200);
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_F13,1);
    fd2_input_begin_test_frame(&input,0);
    CHECK(fd2_input_take_key(&input,&event) && event.bios_code==0x5400);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_A,1);
    fd2_input_begin_test_frame(&input,1);
    CHECK(fd2_input_take_key(&input,&event) && event.bios_code==0x1e41);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_F13,0);
    fd2_input_begin_test_frame(&input,2);
    fd2_input_begin_test_frame(&input,501);
    CHECK(fd2_input_take_key(&input,&event) && event.bios_code==0x1e61 && event.repeat);
    /* AddKey1的F13 break也发送LShift break，即使真实LShift尚未释放。
     * 旧物理快照不构成新的make，不能把BIOS Shift位重新打开。 */
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LSHIFT,1);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_F13,1);
    fd2_input_begin_test_frame(&input,0);CHECK(fd2_input_take_key(&input,&event));
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_F13,0);
    fd2_input_begin_test_frame(&input,1);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_A,1);
    fd2_input_begin_test_frame(&input,2);
    CHECK(input.previous_down[SDL_SCANCODE_LSHIFT]);
    CHECK(fd2_input_take_key(&input,&event) && event.bios_code==0x1e61);
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LSHIFT,1);
    fd2_input_begin_test_frame(&input,0);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_F13,1);
    fd2_input_begin_test_frame(&input,1);CHECK(fd2_input_take_key(&input,&event));
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LSHIFT,0);
    fd2_input_begin_test_frame(&input,2);
    CHECK(!(input.held_modifiers&SDL_KMOD_LSHIFT));
    fd2_input_begin_test_frame(&input,501);
    CHECK(fd2_input_take_key(&input,&event) && event.repeat && event.bios_code==0x5400);
    CHECK(input.held_modifiers&SDL_KMOD_LSHIFT); /* repeat确实重新发fake make。 */
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_A,1);
    fd2_input_begin_test_frame(&input,502);
    CHECK(fd2_input_take_key(&input,&event) && event.bios_code==0x1e41);
    return 0;
}

/* 固定b6abbd5 AddKey1 PrintScreen case：make/break均清repeat。
 * US437 hook不映射2a，IRQ1 2a/aa写/清LShift；预期直接取原字节路径。 */
static int test_printscreen_break_cancels_current_repeat_and_tracks_fake_shift(void) {
    fd2_input input;fd2_input_init(&input);fd2_input_event event;
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_PRINTSCREEN,1);
    fd2_input_begin_test_frame(&input,0);
    CHECK(!fd2_input_has_any_key(&input) && input.repeat_key==SDL_SCANCODE_UNKNOWN &&
        (input.held_modifiers&SDL_KMOD_LSHIFT) && !input.previous_down[SDL_SCANCODE_LSHIFT]);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_B,1);
    fd2_input_begin_test_frame(&input,1);
    CHECK(fd2_input_take_key(&input,&event) && event.bios_code==0x3042 && !event.repeat &&
        input.repeat_key==SDL_SCANCODE_B);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_PRINTSCREEN,0);
    fd2_input_begin_test_frame(&input,2);
    CHECK(!(input.held_modifiers&SDL_KMOD_LSHIFT) && input.repeat_key==SDL_SCANCODE_UNKNOWN &&
        !input.next_repeat_ms[SDL_SCANCODE_B] && input.previous_down[SDL_SCANCODE_B]);
    fd2_input_begin_test_frame(&input,501);CHECK(!fd2_input_has_any_key(&input));

    const SDL_Scancode modifiers[]={SDL_SCANCODE_LSHIFT,SDL_SCANCODE_LCTRL,SDL_SCANCODE_LALT};
    for(size_t i=0;i<sizeof(modifiers)/sizeof(modifiers[0]);i++) {
        fd2_input_init(&input);fd2_input_set_test_key_state(&input,modifiers[i],1);
        fd2_input_begin_test_frame(&input,0);uint16_t before=input.held_modifiers;
        fd2_input_set_test_key_state(&input,SDL_SCANCODE_PRINTSCREEN,1);
        fd2_input_begin_test_frame(&input,1);
        CHECK(input.held_modifiers==before && !fd2_input_has_any_key(&input));
        fd2_input_set_test_key_state(&input,SDL_SCANCODE_PRINTSCREEN,0);
        fd2_input_begin_test_frame(&input,2);
        CHECK(input.held_modifiers==before && input.repeat_key==SDL_SCANCODE_UNKNOWN &&
            !input.next_repeat_ms[modifiers[i]] && !fd2_input_has_any_key(&input));
    }
    /* 选make/break字节各看当时的物理mod；不能按“PrtSc曾fake”统一配对清除。
     * plain make→Ctrl DOWN→PrtSc UP不发aa，Ctrl UP后fake Shift仍保留。 */
    fd2_input_init(&input);fd2_input_set_test_key_state(&input,SDL_SCANCODE_PRINTSCREEN,1);
    fd2_input_begin_test_frame(&input,0);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LCTRL,1);fd2_input_begin_test_frame(&input,1);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_PRINTSCREEN,0);fd2_input_begin_test_frame(&input,2);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_LCTRL,0);fd2_input_begin_test_frame(&input,3);
    CHECK((input.held_modifiers&SDL_KMOD_LSHIFT) && !input.previous_down[SDL_SCANCODE_LSHIFT]);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_F1,1);fd2_input_begin_test_frame(&input,4);
    CHECK(fd2_input_take_key(&input,&event) && event.bios_code==0x5400 && event.key==FD2_INPUT_KEY_OTHER);
    return 0;
}

static int test_printscreen_polled_make_key_break_preserves_fifo_not_repeat(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));fd2_input input;fd2_input_init(&input);fd2_input_event event;
    int host_before=fd2_input_host_quit_requested();
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_B,1);
    CHECK(buffer_push_key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_PRINTSCREEN,SDL_KMOD_NONE));
    CHECK(buffer_push_key(SDL_EVENT_KEY_DOWN,SDL_SCANCODE_B,SDL_KMOD_NONE));
    CHECK(buffer_push_key(SDL_EVENT_KEY_UP,SDL_SCANCODE_PRINTSCREEN,SDL_KMOD_NONE));
    fd2_input_poll_host_events(&input);
    CHECK(input.queued_count==1u && fd2_input_take_key(&input,&event) && event.bios_code==0x3042 &&
        input.repeat_key==SDL_SCANCODE_UNKNOWN && !input.next_repeat_ms[SDL_SCANCODE_B] &&
        !(input.held_modifiers&SDL_KMOD_LSHIFT) && !input.quit_requested &&
        fd2_input_host_quit_requested()==host_before);
    fd2_input_begin_test_frame(&input,500);CHECK(!fd2_input_has_any_key(&input));
    SDL_Quit();return 0;
}

static int test_mapped_keyboard(void) {
    fd2_settings_defaults();
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_binding enter = fd2_binding_parse("Enter");
    CHECK(enter.key == SDL_SCANCODE_RETURN && fd2_keyboard_bind(SDL_SCANCODE_J, enter));
    CHECK(!fd2_keyboard_bind(SDL_SCANCODE_LSHIFT, enter));
    CHECK(fd2_keyboard_bind(SDL_SCANCODE_K, fd2_binding_parse("Ctrl+F2")));
    fd2_input input;
    fd2_input_event e;
    fd2_input_init(&input);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_J, 1);
    fd2_input_begin_test_frame(&input, 0);
    CHECK(fd2_input_take_key(&input, &e) && e.bios_code == 0x1c0d && !e.repeat);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&input, 200);
    CHECK(!fd2_input_has_any_key(&input));
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_J, 0);
    fd2_input_begin_test_frame(&input, 500);
    CHECK(fd2_input_take_key(&input, &e) && e.repeat && e.source == SDL_SCANCODE_RETURN);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_RETURN, 0);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_K, 1);
    fd2_input_begin_test_frame(&input, 510);
    CHECK(fd2_input_take_key(&input, &e) && e.bios_code == 0x5f00);
    fd2_input_begin_test_frame(&input, 1010);
    CHECK(fd2_input_take_key(&input, &e) && e.repeat && e.bios_code == 0x5f00);
    SDL_Event lost = {0}; lost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    CHECK(SDL_PushEvent(&lost));
    fd2_input_poll_host_events(&input);
    CHECK(!input.chord_mods && !input.sources_down[SDL_SCANCODE_F2] && !input.repeat_key);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_K, 0);
    fd2_input_set_test_key_state(&input, SDL_SCANCODE_A, 1);
    fd2_input_begin_test_frame(&input, 1100);
    CHECK(fd2_input_take_key(&input, &e) && e.bios_code == 0x1e61);
    fd2_settings_defaults();
    SDL_Quit();
    return 0;
}

int main(int argc,char **argv) {
    if(argc==3 && !strcmp(argv[1],"--buffer-case"))return buffer_regression(argv[2])?1:0;
    if(argc!=1)return 1;
    if (test_mapped_keyboard() != 0 || test_context_mappings() != 0 ||
        test_movement_cancel_scancodes() != 0 ||
        test_target_scancodes() != 0 ||
        test_detail_any_ordinary_key() != 0 ||
        test_press_exists_for_one_present_frame() != 0 ||
        test_unread_press_survives_present() != 0 ||
        test_glyph_peek_preserves_key_for_wait() != 0 ||
        test_typematic_integer_millisecond_boundaries() != 0 ||
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
        test_polled_lock_keys_preserve_ordinary_queue_without_ack() != 0 ||
        test_bios_ax_and_modified_game_keys() != 0 ||
        test_single_last_key_and_release_never_resumes_old_key() != 0 ||
        test_modifier_make_arbitrates_but_is_not_an_ack() != 0 ||
        test_repeat_runs_while_consumer_waits_and_keeps_tick_phase() != 0 ||
        test_initial_repeat_deadline_overflow_keeps_first_make() != 0 ||
        test_lock_break_and_keypad_num_shift_xor() != 0 ||
        test_alt_keypad_byte_token_and_both_alt_release() != 0 ||
        test_title_uses_original_ax_not_physical_confirm() != 0 ||
        test_all_function_keys_and_modifier_priority() != 0 ||
        test_hardware_extra_keys_and_fake_shift() != 0 ||
        test_printscreen_break_cancels_current_repeat_and_tracks_fake_shift() != 0 ||
        test_printscreen_polled_make_key_break_preserves_fifo_not_repeat() != 0)
        return 1;
    puts("input_test: ok");
    return 0;
}
