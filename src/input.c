/* 炎龙骑士团 2 SDL3 重写 - 单owner普通键缓存
 *
 * input_check @code0 0x620（VA 0x10620）只检查 BIOS BDA 键盘缓冲。
 * 16c57读取一个BIOS键，4e381明确清队。宿主DOWN/UP和唯一repeat源
 * 向同一fd2_input有界缓存交付，不因下一present丢未读确认/取消。
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

static int is_state_only_scancode(SDL_Scancode scancode) {
    /* 当前IBM BIOS普通Caps/Num/ScrollLock只改锁状态，不产生INT16待取键。
     * Pause/Ctrl-Break仍走既有宿主退出映射，不把锁键当详情/对白ACK。 */
    return scancode == SDL_SCANCODE_LCTRL ||
           scancode == SDL_SCANCODE_RCTRL ||
           scancode == SDL_SCANCODE_LSHIFT ||
           scancode == SDL_SCANCODE_RSHIFT ||
           scancode == SDL_SCANCODE_LALT ||
           scancode == SDL_SCANCODE_RALT ||
           scancode == SDL_SCANCODE_LGUI ||
           scancode == SDL_SCANCODE_RGUI ||
           scancode == SDL_SCANCODE_CAPSLOCK ||
           scancode == SDL_SCANCODE_NUMLOCKCLEAR ||
           scancode == SDL_SCANCODE_SCROLLLOCK;
}

void fd2_input_init(fd2_input *input) {
    if (!input) return;
    memset(input, 0, sizeof(*input));
    active_input = input;
}

static void clear_key_lifecycle(fd2_input *input) {
    if (!input) return;
    memset(input->previous_down, 0, sizeof(input->previous_down));
    input->queued_head=input->queued_count=0;
    memset(input->next_repeat_ms, 0, sizeof(input->next_repeat_ms));
}

static uint16_t modifiers_from_state(const bool *state) {
    uint16_t modifiers=0;
    if(state[SDL_SCANCODE_LSHIFT] || state[SDL_SCANCODE_RSHIFT])modifiers|=SDL_KMOD_SHIFT;
    if(state[SDL_SCANCODE_LCTRL] || state[SDL_SCANCODE_RCTRL])modifiers|=SDL_KMOD_CTRL;
    if(state[SDL_SCANCODE_LALT] || state[SDL_SCANCODE_RALT])modifiers|=SDL_KMOD_ALT;
    return modifiers;
}

static void queue_key(fd2_input *input,SDL_Scancode source,int repeat,uint16_t modifiers) {
    if(input->queued_count==FD2_INPUT_KEY_QUEUE_CAPACITY)return;
    unsigned tail=(input->queued_head+input->queued_count)%FD2_INPUT_KEY_QUEUE_CAPACITY;
    input->queued_keys[tail]=(fd2_input_event){key_from_scancode(source),source,
        (uint8_t)(repeat!=0),modifiers};
    input->queued_count++;
}

static void sample_key_state(fd2_input *input,const bool *state,uint64_t now_ms) {
    uint16_t modifiers=modifiers_from_state(state);
    for(int i=1;i<SDL_SCANCODE_COUNT;i++) {
        SDL_Scancode source=(SDL_Scancode)i;
        /* Ctrl+C/Pause属于持久宿主退出，物理fallback也不能把它重排为OTHER。 */
        int down=state[i] && !is_state_only_scancode(source) && source!=SDL_SCANCODE_PAUSE &&
            !(source==SDL_SCANCODE_C && (modifiers&SDL_KMOD_CTRL));
        if(!down) {
            input->previous_down[i]=false;input->next_repeat_ms[i]=0;continue;
        }
        if(!input->previous_down[i]) {
            queue_key(input,source,0,modifiers);
            input->previous_down[i]=true;
            input->next_repeat_ms[i]=now_ms+FD2_INPUT_INITIAL_REPEAT_MS;
        } else if(now_ms>=input->next_repeat_ms[i]) {
            queue_key(input,source,1,modifiers);
            input->next_repeat_ms[i]=now_ms+FD2_INPUT_REPEAT_MS;
        }
    }
}

void fd2_input_poll_host_events(fd2_input *input) {
    if (!input) input = active_input;
    int focus_lost=0;
    SDL_PumpEvents();
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_EVENT_QUIT ||
            event.type == SDL_EVENT_TERMINATING) {
            request_host_quit();
            if (input) input->quit_requested = 1;
        } else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
            clear_key_lifecycle(input);
            focus_lost=1;
        } else if (event.type == SDL_EVENT_KEY_DOWN) {
            if ((event.key.mod & SDL_KMOD_CTRL) != 0 &&
                (event.key.scancode == SDL_SCANCODE_C ||
                 event.key.scancode == SDL_SCANCODE_PAUSE)) {
                request_host_quit();
                if (input) input->quit_requested = 1;
            } else if (event.key.scancode == SDL_SCANCODE_PAUSE) {
                request_host_quit();
                if (input) input->quit_requested = 1;
            } else if(input && !event.key.repeat && event.key.scancode>SDL_SCANCODE_UNKNOWN &&
                    event.key.scancode<SDL_SCANCODE_COUNT && !is_state_only_scancode(event.key.scancode) &&
                    !input->previous_down[event.key.scancode]) {
                /* 先按SDL事件到达序接受首按；Windows全局Shift修正不改变
                 * 已接受event的修饰位。UP也不能删除这条BIOS式待取记录。 */
                queue_key(input,event.key.scancode,0,(uint16_t)(event.key.mod &
                    (SDL_KMOD_SHIFT|SDL_KMOD_CTRL|SDL_KMOD_ALT)));
                input->previous_down[event.key.scancode]=true;
                uint64_t now=input->use_test_key_state?input->test_now_ms:SDL_GetTicks();
                input->next_repeat_ms[event.key.scancode]=now+FD2_INPUT_INITIAL_REPEAT_MS;
            }
        } else if(event.type==SDL_EVENT_KEY_UP && input &&
                event.key.scancode>SDL_SCANCODE_UNKNOWN && event.key.scancode<SDL_SCANCODE_COUNT) {
            input->previous_down[event.key.scancode]=false;
            input->next_repeat_ms[event.key.scancode]=0;
        }
    }
    /* 当前Windows单窗SDL先ResetKeyboard再发LOST；其它/显式注入路径的
     * 本轮快照仍可能DOWN。只禁止本轮fallback复活，后续真实DOWN仍按上方入队。 */
    if(input && !focus_lost)sample_key_state(input,input->use_test_key_state?input->test_key_state:
        SDL_GetKeyboardState(NULL),input->use_test_key_state?input->test_now_ms:SDL_GetTicks());
}

void fd2_input_begin_frame(fd2_input *input) {
    if (!input) return;
    fd2_input_poll_host_events(input);
}

void fd2_input_begin_test_frame(fd2_input *input, uint64_t now_ms) {
    if (!input) return;
    input->use_test_key_state = 1;
    input->test_now_ms=now_ms;
    sample_key_state(input, input->test_key_state, now_ms);
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
    return input && input->queued_count!=0;
}

int fd2_input_take_key(fd2_input *input, fd2_input_event *event) {
    if(!input || !input->queued_count)return 0;
    fd2_input_event value=input->queued_keys[input->queued_head];
    input->queued_head=(uint8_t)((input->queued_head+1u)%FD2_INPUT_KEY_QUEUE_CAPACITY);
    input->queued_count--;
    if(event)*event=value;
    return 1;
}

void fd2_input_clear_keys(fd2_input *input) {
    if(!input)return;
    fd2_input_poll_host_events(input);
    input->queued_head=input->queued_count=0;
}

uint8_t fd2_input_function_scan(const fd2_input_event *event) {
    if(!event || event->source<SDL_SCANCODE_F1 || event->source>SDL_SCANCODE_F12)return 0;
    unsigned index=(unsigned)(event->source-SDL_SCANCODE_F1);
    if(index<10u) {
        unsigned base=event->modifiers&SDL_KMOD_ALT ? 104u:
            event->modifiers&SDL_KMOD_CTRL ? 94u:event->modifiers&SDL_KMOD_SHIFT ? 84u:59u;
        return (uint8_t)(base+index);
    }
    unsigned base=event->modifiers&SDL_KMOD_ALT ? 139u:
        event->modifiers&SDL_KMOD_CTRL ? 137u:event->modifiers&SDL_KMOD_SHIFT ? 135u:133u;
    return (uint8_t)(base+index-10u);
}

fd2_input_action fd2_input_action_for_key(fd2_input_context context,
                                          fd2_input_key key) {
    switch (context) {
        case FD2_INPUT_CONTEXT_FIELD_DETAIL:
            /* 17aed的16c57(0)接受任意普通键，不走browse Esc轮询。 */
            return FD2_INPUT_ACTION_CONFIRM;
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
        case FD2_INPUT_CONTEXT_FIELD_MAGIC_TARGETING:
        case FD2_INPUT_CONTEXT_FIELD_MOVEMENT:
            if ((context == FD2_INPUT_CONTEXT_FIELD_MAGIC_TARGETING ||
                 context == FD2_INPUT_CONTEXT_FIELD_TARGETING) &&
                (key == FD2_INPUT_KEY_Z || key == FD2_INPUT_KEY_KEYPAD_5))
                return FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE;
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
