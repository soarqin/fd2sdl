/* 炎龙骑士团 2 SDL3 重写 - 按需键态输入抽象
 *
 * 原版键表依据：input_check @code0 0x620（VA 0x10620）比较 BIOS BDA
 * 0x041a/0x041c；具体 UI 读取通过 INT 16h/AH=10h 消费一项按键。
 * SDL 宿主不复制 BIOS FIFO，而是在交互层需要输入时采样当前物理键态。
 * 完整依据和宿主差异见 docs/systems/input.md。
 */
#ifndef FD2_INPUT_H
#define FD2_INPUT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <SDL3/SDL.h>

/* IBM PC/AT BIOS 常见默认 typematic：首次重复约 500 ms，之后约
 * 10.9 次/秒。原版 EXE 依赖 BIOS/键盘控制器，本身不维护 repeat FIFO。 */
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
    /* 每个物理键只记录当前电平、是否已经交付和下一 typematic 时点；
     * 没有待消费事件数组或跨状态按键队列。 */
    bool pressed[SDL_SCANCODE_COUNT];
    bool delivered[SDL_SCANCODE_COUNT];
    bool repeat_armed[SDL_SCANCODE_COUNT];
    uint64_t repeat_due_ms[SDL_SCANCODE_COUNT];
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

/* 泵送并清空 SDL 事件队列。普通键只更新当前 pressed 电平；KEY_DOWN
 * 不入游戏 FIFO。KEY_UP 立即清除 repeat 生命周期。窗口关闭和宿主
 * 中断保持为进程级请求。 */
void fd2_input_poll_host_events(fd2_input *input);

/* 按需读取当前物理键态。一次新按下立即返回；只有 SDL 已报告真实
 * typematic KEY_DOWN 后，持续按住才按 initial/repeat deadline 返回。
 * 普通短按不会仅因 pressed 电平暂未更新而由计时器合成重复。 */
int fd2_input_take_key(fd2_input *input, fd2_input_event *event);

/* 非消费式查看当前是否有「可读取」按键：新按下立即可读；已交付但仍
 * 按住的键只有到达 typematic deadline 才再次可读。它不登记动作。 */
int fd2_input_has_any_key(fd2_input *input);

/* 非消费式查询实际导致动画／片头跳过时调用：返回当前可读取键，同时
 * 启动该键的 typematic 生命周期，但不产生可排队的 UI 事件。 */
int fd2_input_observe_any_key(fd2_input *input);

int fd2_input_take_action(fd2_input *input, fd2_input_context context,
                          fd2_input_action *action);

/* 确定性测试入口：改用注入的当前键态，并在指定毫秒时点查询。 */
void fd2_input_set_test_key_state(fd2_input *input, SDL_Scancode source,
                                  int pressed);
int fd2_input_has_any_key_at(fd2_input *input, uint64_t now_ms);
int fd2_input_observe_any_key_at(fd2_input *input, uint64_t now_ms);
int fd2_input_take_key_at(fd2_input *input, fd2_input_event *event,
                          uint64_t now_ms);

int fd2_input_take_quit(fd2_input *input);
fd2_input_action fd2_input_action_for_key(fd2_input_context context,
                                          fd2_input_key key);

#endif /* FD2_INPUT_H */
