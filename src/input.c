/* 炎龙骑士团 2 SDL3 重写 - 原版键盘输入抽象
 *
 * 原版 input_check @0x35834 只检查 BIOS BDA 键盘环形缓冲；具体读取
 * 由 INT 16h/AH=10h 完成。SDL 版在此保留同样的「检查不消费、读取
 * 才出队」边界。详见 docs/systems/input.md。
 */
#include "input.h"

#include <signal.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#endif

static volatile sig_atomic_t interrupt_requested;

#ifndef _WIN32
static void input_signal_handler(int signal_number) {
    (void)signal_number;
    interrupt_requested = 1;
}
#else
static BOOL WINAPI input_console_handler(DWORD control_type) {
    if (control_type == CTRL_C_EVENT || control_type == CTRL_BREAK_EVENT ||
        control_type == CTRL_CLOSE_EVENT || control_type == CTRL_LOGOFF_EVENT ||
        control_type == CTRL_SHUTDOWN_EVENT) {
        interrupt_requested = 1;
        return TRUE;
    }
    return FALSE;
}
#endif

void fd2_input_install_interrupt_handlers(void) {
    interrupt_requested = 0;
#ifndef _WIN32
    /* 不依赖 SDL 默认 signal hint；显式把 Ctrl+C／终止请求交给主循环，
     * 既保留统一资源清理，也避免 scene 把中断当成普通跳过键。 */
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
    if (input->count == FD2_INPUT_QUEUE_CAPACITY) {
        input->dropped_keys++;
        return -1;
    }
    size_t tail = (input->head + input->count) % FD2_INPUT_QUEUE_CAPACITY;
    input->queue[tail].key = key;
    input->queue[tail].source = source;
    input->queue[tail].repeat = repeat ? 1u : 0u;
    input->count++;
    return 0;
}

void fd2_input_pump(fd2_input *input) {
    if (!input) return;

    if (interrupt_requested) {
        interrupt_requested = 0;
        input->quit_requested = 1;
    }

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT ||
            event.type == SDL_EVENT_TERMINATING) {
            input->quit_requested = 1;
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            fd2_input_key key = key_from_scancode(event.key.scancode);
            /* 原版 BIOS 会保存 typematic，但确认／取消键在实际 UI 中
             * 以一次按键完成一次操作；SDL 的 repeat 若跨越状态切换，
             * 会把同一次 Enter 继续投递给下一层菜单。方向键仍保留
             * repeat，用于光标连续移动。 */
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
    return input && input->count != 0;
}

size_t fd2_input_pending_count(const fd2_input *input) {
    return input ? input->count : 0;
}

int fd2_input_take_key(fd2_input *input, fd2_input_event *event) {
    if (!input || input->count == 0) return 0;
    if (event) *event = input->queue[input->head];
    input->head = (input->head + 1u) % FD2_INPUT_QUEUE_CAPACITY;
    input->count--;
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
            /* field controller @code0 0x17e7 将 Esc、0x53 规范化取消码、
             * Z 与数字小键盘 5 送入同一遍历 actor／更新焦点分支。尚未
             * 经 DOSBox 确认前，不把这一组误作 SDL 版的取消选择。 */
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
            /* field_command_menu_input @code0 0x77fc 使用 BIOS 扫描码：
             * 48/50/4b/4d 选四方向，39/1c 确认，01 取消；其读取 helper
             * 另将 keypad 0x52 规范化为确认、0x53 规范化为取消。 */
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

        case FD2_INPUT_CONTEXT_FIELD_SYSTEM_MENU:
            /* field_empty_focus_menu_execute @code0 0x6f55 与各子页复用
             * field_command_menu_input 的四方向、确认和取消键。 */
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
            /* field_manual_slot_picker @code0 0x19bcb 只使用扫描码
             * 0x48/0x50 改变纵向 slot；在原版 BIOS 命名中对应 Up/Down。 */
            if (key == FD2_INPUT_KEY_UP) return FD2_INPUT_ACTION_LEFT;
            if (key == FD2_INPUT_KEY_DOWN) return FD2_INPUT_ACTION_RIGHT;
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM)
                return FD2_INPUT_ACTION_CONFIRM;
            if (key == FD2_INPUT_KEY_ESCAPE || key == FD2_INPUT_KEY_CANCEL)
                return FD2_INPUT_ACTION_CANCEL;
            return FD2_INPUT_ACTION_NONE;

        case FD2_INPUT_CONTEXT_FIELD_AUXILIARY:
            /* tactical-map dispatcher @code0 0x1000a 的页面由任意键
             * 返回 browse；SDL 由 field_play 单独消费确认／取消。 */
            if (key == FD2_INPUT_KEY_ENTER || key == FD2_INPUT_KEY_SPACE ||
                key == FD2_INPUT_KEY_KEYPAD_CONFIRM ||
                key == FD2_INPUT_KEY_ESCAPE || key == FD2_INPUT_KEY_CANCEL)
                return FD2_INPUT_ACTION_EXIT;
            return FD2_INPUT_ACTION_NONE;

        case FD2_INPUT_CONTEXT_FIELD_TARGETING:
            /* field_cell_selection_execute @0x367ca 的目标选择循环：
             * 四方向移动，39/1c 确认，规范化返回值 1 取消。 */
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
    if (!input || !input->quit_requested) return 0;
    input->quit_requested = 0;
    return 1;
}
