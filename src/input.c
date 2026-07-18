/* 炎龙骑士团 2 SDL3 重写 - 逐呈现帧键态输入
 *
 * input_check @code0 0x620（VA 0x10620）只检查 BIOS BDA 键盘缓冲。
 * SDL 适配不建立按键 FIFO：每次 present 后从 SDL_GetKeyboardState
 * 计算当前帧 IsPressed／repeat 脉冲，交互层只读取该呈现帧。
 */
#include "input.h"

#include <signal.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
static volatile LONG host_quit_requested;
#else
static volatile sig_atomic_t host_quit_requested;
#endif

static fd2_input *active_input;

int fd2_input_host_quit_requested(void) {
#ifdef _WIN32
    return InterlockedCompareExchange(&host_quit_requested, 0, 0) != 0;
#else
    return host_quit_requested != 0;
#endif
}

static void request_host_quit(void) {
#ifdef _WIN32
    (void)InterlockedExchange(&host_quit_requested, 1);
#else
    host_quit_requested = 1;
#endif
}

#ifndef _WIN32
static void input_signal_handler(int signal_number) {
    (void)signal_number;
    host_quit_requested = 1;
}
#else
static BOOL WINAPI input_console_handler(DWORD control_type) {
    if (control_type == CTRL_C_EVENT || control_type == CTRL_BREAK_EVENT ||
        control_type == CTRL_CLOSE_EVENT || control_type == CTRL_LOGOFF_EVENT ||
        control_type == CTRL_SHUTDOWN_EVENT) {
        (void)InterlockedExchange(&host_quit_requested, 1);
        return TRUE;
    }
    return FALSE;
}
#endif

void fd2_input_install_interrupt_handlers(void) {
#ifdef _WIN32
    (void)InterlockedExchange(&host_quit_requested, 0);
#else
    host_quit_requested = 0;
#endif
#ifndef _WIN32
    (void)signal(SIGINT, input_signal_handler);
    (void)signal(SIGTERM, input_signal_handler);
#else
    (void)SetConsoleCtrlHandler(input_console_handler, TRUE);
#endif
}

static fd2_input_key key_from_scancode(SDL_Scancode scancode) {
    switch (scancode) {
        case SDL_SCANCODE_ESCAPE: return FD2_INPUT_KEY_ESCAPE;
        case SDL_SCANCODE_RETURN: return FD2_INPUT_KEY_ENTER;
        case SDL_SCANCODE_KP_ENTER: return FD2_INPUT_KEY_KEYPAD_CONFIRM;
        case SDL_SCANCODE_SPACE: return FD2_INPUT_KEY_SPACE;
        case SDL_SCANCODE_INSERT:
        case SDL_SCANCODE_KP_0: return FD2_INPUT_KEY_KEYPAD_CONFIRM;
        case SDL_SCANCODE_DELETE:
        case SDL_SCANCODE_KP_DECIMAL: return FD2_INPUT_KEY_CANCEL;
        case SDL_SCANCODE_UP:
        case SDL_SCANCODE_KP_8: return FD2_INPUT_KEY_UP;
        case SDL_SCANCODE_DOWN:
        case SDL_SCANCODE_KP_2: return FD2_INPUT_KEY_DOWN;
        case SDL_SCANCODE_LEFT:
        case SDL_SCANCODE_KP_4: return FD2_INPUT_KEY_LEFT;
        case SDL_SCANCODE_RIGHT:
        case SDL_SCANCODE_KP_6: return FD2_INPUT_KEY_RIGHT;
        case SDL_SCANCODE_F1: return FD2_INPUT_KEY_F1;
        case SDL_SCANCODE_F2: return FD2_INPUT_KEY_F2;
        case SDL_SCANCODE_HOME:
        case SDL_SCANCODE_KP_7: return FD2_INPUT_KEY_HOME;
        case SDL_SCANCODE_PAGEUP:
        case SDL_SCANCODE_KP_9: return FD2_INPUT_KEY_PAGE_UP;
        case SDL_SCANCODE_G: return FD2_INPUT_KEY_G;
        case SDL_SCANCODE_Z: return FD2_INPUT_KEY_Z;
        case SDL_SCANCODE_KP_5: return FD2_INPUT_KEY_KEYPAD_5;
        default: return FD2_INPUT_KEY_OTHER;
    }
}

static int is_modifier_scancode(SDL_Scancode scancode) {
    return scancode == SDL_SCANCODE_LCTRL ||
           scancode == SDL_SCANCODE_RCTRL ||
           scancode == SDL_SCANCODE_LSHIFT ||
           scancode == SDL_SCANCODE_RSHIFT ||
           scancode == SDL_SCANCODE_LALT ||
           scancode == SDL_SCANCODE_RALT ||
           scancode == SDL_SCANCODE_LGUI ||
           scancode == SDL_SCANCODE_RGUI;
}

void fd2_input_init(fd2_input *input) {
    if (!input) return;
    memset(input, 0, sizeof(*input));
    active_input = input;
}

static void clear_key_lifecycle(fd2_input *input) {
    if (!input) return;
    memset(input->previous_down, 0, sizeof(input->previous_down));
    memset(input->frame_trigger, 0, sizeof(input->frame_trigger));
    memset(input->frame_repeat, 0, sizeof(input->frame_repeat));
    memset(input->frame_consumed, 0, sizeof(input->frame_consumed));
    memset(input->next_repeat_ms, 0, sizeof(input->next_repeat_ms));
}

void fd2_input_poll_host_events(fd2_input *input) {
    if (!input) input = active_input;
    SDL_PumpEvents();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT ||
            event.type == SDL_EVENT_TERMINATING) {
            request_host_quit();
            if (input) input->quit_requested = 1;
        } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            clear_key_lifecycle(input);
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            if ((event.key.mod & SDL_KMOD_CTRL) != 0 &&
                (event.key.scancode == SDL_SCANCODE_C ||
                 event.key.scancode == SDL_SCANCODE_PAUSE)) {
                request_host_quit();
                if (input) input->quit_requested = 1;
            } else if (event.key.scancode == SDL_SCANCODE_PAUSE) {
                request_host_quit();
                if (input) input->quit_requested = 1;
            }
        }
        /* 普通 key event 仅用于 SDL 内部键态更新；不转存为游戏事件。 */
    }
}

static void begin_frame_from_state(fd2_input *input, const bool *state,
                                   uint64_t now_ms) {
    memset(input->frame_trigger, 0, sizeof(input->frame_trigger));
    memset(input->frame_repeat, 0, sizeof(input->frame_repeat));
    memset(input->frame_consumed, 0, sizeof(input->frame_consumed));

    for (int i = 1; i < SDL_SCANCODE_COUNT; i++) {
        SDL_Scancode scancode = (SDL_Scancode)i;
        bool down = state[i] && !is_modifier_scancode(scancode) &&
                    scancode != SDL_SCANCODE_PAUSE;
        if (!down) {
            input->previous_down[i] = false;
            input->next_repeat_ms[i] = 0;
            continue;
        }
        if (!input->previous_down[i]) {
            input->frame_trigger[i] = true;
            input->previous_down[i] = true;
            input->next_repeat_ms[i] =
                now_ms + FD2_INPUT_INITIAL_REPEAT_MS;
        } else if (now_ms >= input->next_repeat_ms[i]) {
            input->frame_trigger[i] = true;
            input->frame_repeat[i] = true;
            input->next_repeat_ms[i] = now_ms + FD2_INPUT_REPEAT_MS;
        }
    }
}

void fd2_input_begin_frame(fd2_input *input) {
    if (!input) return;
    fd2_input_poll_host_events(input);
    const bool *state = input->use_test_key_state
        ? input->test_key_state
        : SDL_GetKeyboardState(NULL);
    begin_frame_from_state(input, state, SDL_GetTicks());
}

void fd2_input_begin_test_frame(fd2_input *input, uint64_t now_ms) {
    if (!input) return;
    input->use_test_key_state = 1;
    begin_frame_from_state(input, input->test_key_state, now_ms);
}

void fd2_input_set_test_key_state(fd2_input *input, SDL_Scancode source,
                                  int pressed) {
    if (!input || source <= SDL_SCANCODE_UNKNOWN ||
        source >= SDL_SCANCODE_COUNT)
        return;
    input->use_test_key_state = 1;
    input->test_key_state[source] = pressed != 0;
}

int fd2_input_has_any_key(const fd2_input *input) {
    if (!input) return 0;
    for (int i = 1; i < SDL_SCANCODE_COUNT; i++)
        if (input->frame_trigger[i] && !input->frame_consumed[i]) return 1;
    return 0;
}

int fd2_input_take_key(fd2_input *input, fd2_input_event *event) {
    if (!input) return 0;
    for (int i = 1; i < SDL_SCANCODE_COUNT; i++) {
        if (!input->frame_trigger[i] || input->frame_consumed[i]) continue;
        input->frame_consumed[i] = true;
        if (event) {
            event->source = (SDL_Scancode)i;
            event->key = key_from_scancode((SDL_Scancode)i);
            event->repeat = input->frame_repeat[i] ? 1u : 0u;
        }
        return 1;
    }
    return 0;
}

int fd2_input_observe_any_key(fd2_input *input) {
    fd2_input_event ignored;
    return fd2_input_take_key(input, &ignored);
}

fd2_input_action fd2_input_action_for_key(fd2_input_context context,
                                          fd2_input_key key) {
    switch (context) {
        case FD2_INPUT_CONTEXT_TITLE:
            if (key == FD2_INPUT_KEY_UP) return FD2_INPUT_ACTION_UP;
            if (key == FD2_INPUT_KEY_DOWN) return FD2_INPUT_ACTION_DOWN;
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM)
                return FD2_INPUT_ACTION_CONFIRM;
            return FD2_INPUT_ACTION_NONE;

        case FD2_INPUT_CONTEXT_CHOICE:
            if (key == FD2_INPUT_KEY_LEFT) return FD2_INPUT_ACTION_LEFT;
            if (key == FD2_INPUT_KEY_RIGHT) return FD2_INPUT_ACTION_RIGHT;
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM)
                return FD2_INPUT_ACTION_CONFIRM;
            if (key == FD2_INPUT_KEY_ESCAPE || key == FD2_INPUT_KEY_CANCEL)
                return FD2_INPUT_ACTION_CANCEL;
            return FD2_INPUT_ACTION_NONE;

        case FD2_INPUT_CONTEXT_FIELD:
            if (key == FD2_INPUT_KEY_UP) return FD2_INPUT_ACTION_UP;
            if (key == FD2_INPUT_KEY_DOWN) return FD2_INPUT_ACTION_DOWN;
            if (key == FD2_INPUT_KEY_LEFT) return FD2_INPUT_ACTION_LEFT;
            if (key == FD2_INPUT_KEY_RIGHT) return FD2_INPUT_ACTION_RIGHT;
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM)
                return FD2_INPUT_ACTION_CONFIRM;
            if (key == FD2_INPUT_KEY_ESCAPE || key == FD2_INPUT_KEY_CANCEL ||
                key == FD2_INPUT_KEY_Z || key == FD2_INPUT_KEY_KEYPAD_5)
                return FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE;
            if (key == FD2_INPUT_KEY_F1 || key == FD2_INPUT_KEY_PAGE_UP)
                return FD2_INPUT_ACTION_FIELD_AUXILIARY;
            if (key == FD2_INPUT_KEY_F2 || key == FD2_INPUT_KEY_HOME)
                return FD2_INPUT_ACTION_FIELD_DETAIL;
            if (key == FD2_INPUT_KEY_G)
                return FD2_INPUT_ACTION_FIELD_SPECIAL_G;
            return FD2_INPUT_ACTION_NONE;

        case FD2_INPUT_CONTEXT_FIELD_COMMAND:
        case FD2_INPUT_CONTEXT_FIELD_SYSTEM_MENU:
        case FD2_INPUT_CONTEXT_FIELD_TARGETING:
            if (key == FD2_INPUT_KEY_UP) return FD2_INPUT_ACTION_UP;
            if (key == FD2_INPUT_KEY_DOWN) return FD2_INPUT_ACTION_DOWN;
            if (key == FD2_INPUT_KEY_LEFT) return FD2_INPUT_ACTION_LEFT;
            if (key == FD2_INPUT_KEY_RIGHT) return FD2_INPUT_ACTION_RIGHT;
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM)
                return FD2_INPUT_ACTION_CONFIRM;
            if (key == FD2_INPUT_KEY_ESCAPE || key == FD2_INPUT_KEY_CANCEL)
                return FD2_INPUT_ACTION_CANCEL;
            return FD2_INPUT_ACTION_NONE;

        case FD2_INPUT_CONTEXT_FIELD_MANUAL_SLOT:
            if (key == FD2_INPUT_KEY_UP) return FD2_INPUT_ACTION_LEFT;
            if (key == FD2_INPUT_KEY_DOWN) return FD2_INPUT_ACTION_RIGHT;
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM)
                return FD2_INPUT_ACTION_CONFIRM;
            if (key == FD2_INPUT_KEY_ESCAPE || key == FD2_INPUT_KEY_CANCEL)
                return FD2_INPUT_ACTION_CANCEL;
            return FD2_INPUT_ACTION_NONE;

        case FD2_INPUT_CONTEXT_FIELD_AUXILIARY:
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM ||
                key == FD2_INPUT_KEY_ESCAPE || key == FD2_INPUT_KEY_CANCEL)
                return FD2_INPUT_ACTION_EXIT;
            return FD2_INPUT_ACTION_NONE;

        case FD2_INPUT_CONTEXT_PREVIEW:
            if (key == FD2_INPUT_KEY_UP) return FD2_INPUT_ACTION_UP;
            if (key == FD2_INPUT_KEY_DOWN) return FD2_INPUT_ACTION_DOWN;
            if (key == FD2_INPUT_KEY_LEFT) return FD2_INPUT_ACTION_LEFT;
            if (key == FD2_INPUT_KEY_RIGHT) return FD2_INPUT_ACTION_RIGHT;
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM ||
                key == FD2_INPUT_KEY_ESCAPE || key == FD2_INPUT_KEY_CANCEL)
                return FD2_INPUT_ACTION_EXIT;
            return FD2_INPUT_ACTION_NONE;
    }
    return FD2_INPUT_ACTION_NONE;
}

int fd2_input_take_action(fd2_input *input, fd2_input_context context,
                          fd2_input_action *action) {
    fd2_input_event event;
    if (!fd2_input_take_key(input, &event)) return 0;
    if (action) *action = fd2_input_action_for_key(context, event.key);
    return 1;
}

int fd2_input_take_quit(fd2_input *input) {
    if (!input) return fd2_input_host_quit_requested();
    if (fd2_input_host_quit_requested()) input->quit_requested = 1;
    return input->quit_requested != 0;
}
