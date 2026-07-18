/* 炎龙骑士团 2 SDL3 重写 - 逐呈现帧键态输入
 *
 * 原版键表依据：input_check @code0 0x620（VA 0x10620）比较 BIOS BDA
 * 0x041a/0x041c；具体 UI 读取通过 INT 16h/AH=10h 消费一项按键。
 * SDL 宿主不复制 BIOS FIFO：每次画面 present 后采样 SDL 当前键态，
 * 只向交互层暴露该呈现帧的按下／重复脉冲。
 */
#ifndef FD2_INPUT_H
#define FD2_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include <SDL3/SDL.h>

/* PC/AT 常见默认 typematic 的宿主近似值；不是 FD2.EXE 内部常量。 */
#define FD2_INPUT_INITIAL_REPEAT_MS 500u
#define FD2_INPUT_REPEAT_MS 92u

typedef enum {
    FD2_INPUT_KEY_OTHER = 0,
    FD2_INPUT_KEY_ESCAPE,
    FD2_INPUT_KEY_ENTER,
    FD2_INPUT_KEY_SPACE,
    FD2_INPUT_KEY_KEYPAD_CONFIRM,
    FD2_INPUT_KEY_CANCEL,
    FD2_INPUT_KEY_UP,
    FD2_INPUT_KEY_DOWN,
    FD2_INPUT_KEY_LEFT,
    FD2_INPUT_KEY_RIGHT,
    FD2_INPUT_KEY_F1,
    FD2_INPUT_KEY_F2,
    FD2_INPUT_KEY_HOME,
    FD2_INPUT_KEY_PAGE_UP,
    FD2_INPUT_KEY_G,
    FD2_INPUT_KEY_Z,
    FD2_INPUT_KEY_KEYPAD_5
} fd2_input_key;

typedef struct {
    fd2_input_key key;
    SDL_Scancode source;
    uint8_t repeat;
} fd2_input_event;

typedef struct {
    /* previous_down／next_repeat_ms 只用于从当前物理电平计算本帧脉冲；
     * frame_trigger 每次 begin_frame 全量重建，不是跨帧事件队列。 */
    bool previous_down[SDL_SCANCODE_COUNT];
    bool frame_trigger[SDL_SCANCODE_COUNT];
    bool frame_repeat[SDL_SCANCODE_COUNT];
    bool frame_consumed[SDL_SCANCODE_COUNT];
    uint64_t next_repeat_ms[SDL_SCANCODE_COUNT];
    bool test_key_state[SDL_SCANCODE_COUNT];
    int use_test_key_state;
    int quit_requested;
} fd2_input;

typedef enum {
    FD2_INPUT_CONTEXT_TITLE = 0,
    FD2_INPUT_CONTEXT_CHOICE,
    FD2_INPUT_CONTEXT_FIELD,
    FD2_INPUT_CONTEXT_FIELD_COMMAND,
    FD2_INPUT_CONTEXT_FIELD_TARGETING,
    FD2_INPUT_CONTEXT_FIELD_SYSTEM_MENU,
    FD2_INPUT_CONTEXT_FIELD_MANUAL_SLOT,
    FD2_INPUT_CONTEXT_FIELD_AUXILIARY,
    FD2_INPUT_CONTEXT_PREVIEW
} fd2_input_context;

typedef enum {
    FD2_INPUT_ACTION_NONE = 0,
    FD2_INPUT_ACTION_UP,
    FD2_INPUT_ACTION_DOWN,
    FD2_INPUT_ACTION_LEFT,
    FD2_INPUT_ACTION_RIGHT,
    FD2_INPUT_ACTION_CONFIRM,
    FD2_INPUT_ACTION_CANCEL,
    FD2_INPUT_ACTION_EXIT,
    FD2_INPUT_ACTION_FIELD_AUXILIARY,
    FD2_INPUT_ACTION_FIELD_DETAIL,
    FD2_INPUT_ACTION_FIELD_FOCUS_CYCLE,
    FD2_INPUT_ACTION_FIELD_SPECIAL_G
} fd2_input_action;

void fd2_input_init(fd2_input *input);
void fd2_input_install_interrupt_handlers(void);
int fd2_input_host_quit_requested(void);

/* 泵送并清空宿主事件，仅保留持久 quit。普通键不在这里生成游戏输入。 */
void fd2_input_poll_host_events(fd2_input *input);

/* 每次呈现画面时调用一次：采样 SDL_GetKeyboardState，并重建该呈现帧
 * 的 IsPressed／repeat 脉冲。上一帧未读取的脉冲在这里直接消失。 */
void fd2_input_begin_frame(fd2_input *input);

/* 当前呈现帧的非消费式查询与消费接口；均不会主动泵送 SDL 事件。 */
int fd2_input_has_any_key(const fd2_input *input);
int fd2_input_take_key(fd2_input *input, fd2_input_event *event);
int fd2_input_observe_any_key(fd2_input *input);
int fd2_input_take_action(fd2_input *input, fd2_input_context context,
                          fd2_input_action *action);

/* 确定性测试入口：注入物理键态，并以指定时间建立呈现帧。 */
void fd2_input_set_test_key_state(fd2_input *input, SDL_Scancode source,
                                  int pressed);
void fd2_input_begin_test_frame(fd2_input *input, uint64_t now_ms);

int fd2_input_take_quit(fd2_input *input);
fd2_input_action fd2_input_action_for_key(fd2_input_context context,
                                          fd2_input_key key);

#endif /* FD2_INPUT_H */
