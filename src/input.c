/* 炎龙骑士团 2 SDL3 重写 - 按需键态输入抽象
 *
 * input_check @code0 0x620（VA 0x10620）只比较 BIOS BDA 键盘缓冲
 * head/tail；field/UI 的等待 helper 再以 INT 16h/AH=10h 读取一项。
 * SDL 无法直接复刻 BIOS 中断层，因此宿主适配为按需采样当前键态，并
 * 用 BIOS 风格 typematic deadline 防止一次长按跨 UI 状态立即重触发。
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

/* 项目只有一个 VGA 输入服务；delay/present 没有 vga 参数时仍通过最近
 * 初始化的服务同步 KEY_UP，避免短暂 release/repress 被漏掉。 */
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
        case SDL_SCANCODE_ESCAPE:
            return FD2_INPUT_KEY_ESCAPE;
        case SDL_SCANCODE_RETURN:
            return FD2_INPUT_KEY_ENTER;
        case SDL_SCANCODE_KP_ENTER:
            return FD2_INPUT_KEY_KEYPAD_CONFIRM;
        case SDL_SCANCODE_SPACE:
            return FD2_INPUT_KEY_SPACE;
        case SDL_SCANCODE_INSERT:
        case SDL_SCANCODE_KP_0:
            return FD2_INPUT_KEY_KEYPAD_CONFIRM;
        case SDL_SCANCODE_DELETE:
        case SDL_SCANCODE_KP_DECIMAL:
            return FD2_INPUT_KEY_CANCEL;
        case SDL_SCANCODE_UP:
        case SDL_SCANCODE_KP_8:
            return FD2_INPUT_KEY_UP;
        case SDL_SCANCODE_DOWN:
        case SDL_SCANCODE_KP_2:
            return FD2_INPUT_KEY_DOWN;
        case SDL_SCANCODE_LEFT:
        case SDL_SCANCODE_KP_4:
            return FD2_INPUT_KEY_LEFT;
        case SDL_SCANCODE_RIGHT:
        case SDL_SCANCODE_KP_6:
            return FD2_INPUT_KEY_RIGHT;
        case SDL_SCANCODE_F1:
            return FD2_INPUT_KEY_F1;
        case SDL_SCANCODE_F2:
            return FD2_INPUT_KEY_F2;
        case SDL_SCANCODE_HOME:
        case SDL_SCANCODE_KP_7:
            return FD2_INPUT_KEY_HOME;
        case SDL_SCANCODE_PAGEUP:
        case SDL_SCANCODE_KP_9:
            return FD2_INPUT_KEY_PAGE_UP;
        case SDL_SCANCODE_G:
            return FD2_INPUT_KEY_G;
        case SDL_SCANCODE_Z:
            return FD2_INPUT_KEY_Z;
        case SDL_SCANCODE_KP_5:
            return FD2_INPUT_KEY_KEYPAD_5;
        default:
            return FD2_INPUT_KEY_OTHER;
    }
}

void fd2_input_init(fd2_input *input) {
    if (!input) return;
    memset(input, 0, sizeof(*input));
    active_input = input;
}

static int is_controlled_scancode(SDL_Scancode scancode) {
    return scancode > SDL_SCANCODE_UNKNOWN &&
           scancode < SDL_SCANCODE_COUNT;
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

static void set_key_level(fd2_input *input, SDL_Scancode scancode,
                          int pressed) {
    if (!input || !is_controlled_scancode(scancode)) return;
    input->pressed[scancode] = pressed != 0;
    if (!pressed) {
        input->delivered[scancode] = false;
        input->repeat_armed[scancode] = false;
        input->repeat_due_ms[scancode] = 0;
    }
}

static void release_all_keys(fd2_input *input) {
    if (!input) return;
    memset(input->pressed, 0, sizeof(input->pressed));
    memset(input->delivered, 0, sizeof(input->delivered));
    memset(input->repeat_armed, 0, sizeof(input->repeat_armed));
    memset(input->repeat_due_ms, 0, sizeof(input->repeat_due_ms));
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
            /* 失焦期间未必能收到每个 KEY_UP；不能让旧按下电平在恢复后
             * 永久卡住或继续 typematic。 */
            release_all_keys(input);
        } else if (event.type == SDL_EVENT_KEY_DOWN ||
                   event.type == SDL_EVENT_KEY_UP) {
            int pressed = event.type == SDL_EVENT_KEY_DOWN;
            SDL_Scancode scancode = event.key.scancode;
            if (pressed && (event.key.mod & SDL_KMOD_CTRL) != 0 &&
                (scancode == SDL_SCANCODE_C ||
                 scancode == SDL_SCANCODE_PAUSE)) {
                request_host_quit();
                if (input) input->quit_requested = 1;
                continue;
            }
            if (pressed && scancode == SDL_SCANCODE_PAUSE) {
                request_host_quit();
                if (input) input->quit_requested = 1;
                continue;
            }
            /* KEY_DOWN repeat 不排队为游戏输入，只证明宿主 typematic
             * 已经开始。此前已交付的长按键从此才允许按 repeat deadline
             * 再读；普通短按不会由本模块凭时间自行制造重复。 */
            if (!event.key.repeat) {
                set_key_level(input, scancode, pressed);
            } else if (pressed && input && input->pressed[scancode] &&
                       input->delivered[scancode]) {
                input->repeat_armed[scancode] = true;
            }
        }
        /* 其他 window/display/mouse/text 事件在此统一丢弃，不能积压。 */
    }
}

static const bool *current_keyboard_state(fd2_input *input) {
    if (!input->use_test_key_state)
        fd2_input_poll_host_events(input);
    return input->pressed;
}

static int input_key_is_readable(const fd2_input *input,
                                  const bool *state,
                                  SDL_Scancode scancode,
                                  uint64_t now_ms) {
    return state[scancode] && is_controlled_scancode(scancode) &&
           !is_modifier_scancode(scancode) &&
           scancode != SDL_SCANCODE_PAUSE &&
           (!input->delivered[scancode] ||
            (input->repeat_armed[scancode] &&
             now_ms >= input->repeat_due_ms[scancode]));
}

static void input_mark_read(fd2_input *input, SDL_Scancode scancode,
                            uint64_t now_ms) {
    int repeat = input->delivered[scancode] != 0;
    input->delivered[scancode] = true;
    /* 第一次读取后仍须等 SDL 报告真实 typematic KEY_DOWN；若只是普通
     * 短按，即使 KEY_UP 尚未被泵到，也不能按计时器凭空连发。 */
    if (!repeat) input->repeat_armed[scancode] = false;
    input->repeat_due_ms[scancode] = now_ms +
        (repeat ? FD2_INPUT_REPEAT_MS : FD2_INPUT_INITIAL_REPEAT_MS);
}

static int input_find_readable(const fd2_input *input, const bool *state,
                               uint64_t now_ms, SDL_Scancode *found) {
    for (int i = 1; i < SDL_SCANCODE_COUNT; i++) {
        SDL_Scancode scancode = (SDL_Scancode)i;
        if (input_key_is_readable(input, state, scancode, now_ms)) {
            if (found) *found = scancode;
            return 1;
        }
    }
    return 0;
}

int fd2_input_take_key_at(fd2_input *input, fd2_input_event *event,
                          uint64_t now_ms) {
    if (!input) return 0;
    const bool *state = current_keyboard_state(input);
    SDL_Scancode scancode;
    if (!input_find_readable(input, state, now_ms, &scancode)) return 0;

    int repeat = input->delivered[scancode] != 0;
    input_mark_read(input, scancode, now_ms);
    if (event) {
        event->key = key_from_scancode(scancode);
        event->source = scancode;
        event->repeat = repeat ? 1u : 0u;
    }
    return 1;
}

int fd2_input_take_key(fd2_input *input, fd2_input_event *event) {
    return fd2_input_take_key_at(input, event, SDL_GetTicks());
}

int fd2_input_has_any_key_at(fd2_input *input, uint64_t now_ms) {
    if (!input) return 0;
    const bool *state = current_keyboard_state(input);
    return input_find_readable(input, state, now_ms, NULL);
}

int fd2_input_observe_any_key_at(fd2_input *input, uint64_t now_ms) {
    if (!input) return 0;
    const bool *state = current_keyboard_state(input);
    SDL_Scancode scancode;
    if (!input_find_readable(input, state, now_ms, &scancode)) return 0;
    /* 状态跳转后的下一 UI 总是获得完整 initial delay；即使本次 skip
     * 由已按住键的 repeat 时点触发，也不能只保护一个 repeat 间隔。 */
    input->delivered[scancode] = true;
    /* 状态跳转本身不能伪造 typematic；必须等后续宿主 repeat KEY_DOWN
     * 再允许同一长按键进入下一 UI。 */
    input->repeat_armed[scancode] = false;
    input->repeat_due_ms[scancode] =
        now_ms + FD2_INPUT_INITIAL_REPEAT_MS;
    return 1;
}

int fd2_input_observe_any_key(fd2_input *input) {
    return fd2_input_observe_any_key_at(input, SDL_GetTicks());
}

int fd2_input_has_any_key(fd2_input *input) {
    return fd2_input_has_any_key_at(input, SDL_GetTicks());
}

void fd2_input_set_test_key_state(fd2_input *input, SDL_Scancode source,
                                  int pressed) {
    if (!input || !is_controlled_scancode(source)) return;
    input->use_test_key_state = 1;
    input->pressed[source] = pressed != 0;
    if (!pressed) {
        input->delivered[source] = 0;
        input->repeat_armed[source] = 0;
        input->repeat_due_ms[source] = 0;
    }
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
    fd2_input_poll_host_events(input);
    if (fd2_input_host_quit_requested()) input->quit_requested = 1;
    return input->quit_requested != 0;
}
