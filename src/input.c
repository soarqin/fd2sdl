/* 炎龙骑士团 2 SDL3 重写 - 统一帧输入抽象
 *
 * 原版键值和上下文映射来自 input_check @0x35834 与
 * field_key_read @0x36cbc；SDL 宿主层不复刻 BIOS FIFO。每次
 * fd2_input_begin_frame() 都丢弃上一帧未消费事件，只暴露本帧到达的
 * KEY_DOWN，避免一次 Enter 跨对话框或跨 UI 状态被再次消费。
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
    if (input) memset(input, 0, sizeof(*input));
}

int fd2_input_push_key(fd2_input *input, fd2_input_key key,
                       SDL_Scancode source, int repeat) {
    if (!input) return -1;
    if (input->frame_event_count == FD2_INPUT_FRAME_EVENT_CAPACITY) {
        input->dropped_frame_keys++;
        return -1;
    }
    fd2_input_event *event =
        &input->frame_events[input->frame_event_count++];
    event->key = key;
    event->source = source;
    event->repeat = repeat ? 1u : 0u;
    return 0;
}

void fd2_input_poll_host_events(void) {
    SDL_PumpEvents();
    if (SDL_HasEvent(SDL_EVENT_QUIT) ||
        SDL_HasEvent(SDL_EVENT_TERMINATING))
        request_host_quit();
}

void fd2_input_begin_frame(fd2_input *input) {
    if (!input) return;

    /* 帧边界先清空，再读取当前 SDL 队列；任何未消费按键都不能进入下一帧。 */
    input->frame_event_count = 0;
    input->frame_event_cursor = 0;
    if (fd2_input_host_quit_requested())
        input->quit_requested = 1;

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT ||
            event.type == SDL_EVENT_TERMINATING) {
            request_host_quit();
            input->quit_requested = 1;
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            if ((event.key.mod & SDL_KMOD_CTRL) != 0 &&
                (event.key.scancode == SDL_SCANCODE_C ||
                 event.key.scancode == SDL_SCANCODE_PAUSE)) {
                request_host_quit();
                input->quit_requested = 1;
                continue;
            }
            if (event.key.scancode == SDL_SCANCODE_PAUSE) {
                request_host_quit();
                input->quit_requested = 1;
                continue;
            }
            fd2_input_key key = key_from_scancode(event.key.scancode);
            /* 同一次确认／取消长按不能跨 UI 状态持续触发；方向 repeat
             * 保留在其实际到达的宿主帧内，用于连续移动。 */
            if (event.key.repeat &&
                (key == FD2_INPUT_KEY_ENTER ||
                 key == FD2_INPUT_KEY_KEYPAD_CONFIRM ||
                 key == FD2_INPUT_KEY_SPACE ||
                 key == FD2_INPUT_KEY_ESCAPE ||
                 key == FD2_INPUT_KEY_CANCEL))
                continue;
            (void)fd2_input_push_key(input, key,
                                     event.key.scancode, event.key.repeat);
        }
    }
}

int fd2_input_has_any_key(const fd2_input *input) {
    return input && input->frame_event_cursor < input->frame_event_count;
}

size_t fd2_input_pending_count(const fd2_input *input) {
    if (!input || input->frame_event_cursor >= input->frame_event_count)
        return 0;
    return input->frame_event_count - input->frame_event_cursor;
}

int fd2_input_take_key(fd2_input *input, fd2_input_event *event) {
    if (!fd2_input_has_any_key(input)) return 0;
    if (event) *event = input->frame_events[input->frame_event_cursor];
    input->frame_event_cursor++;
    return 1;
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
