/* 炎龙骑士团 2 SDL3 重写 - 原版键盘输入抽象
 *
 * 逆向依据：input_check @0x35834 比较 BIOS BDA 0x041a/0x041c，
 * field_key_read @0x36cbc 以 INT 16h/AH=10h 读取扫描码。
 * 完整键表与上下文依据见 docs/systems/input.md。
 */
#ifndef FD2_INPUT_H
#define FD2_INPUT_H

#include <stddef.h>
#include <stdint.h>

#include <SDL3/SDL.h>

/* BIOS BDA 键盘环形缓冲有 16 个槽位，并保留一个空槽区分满／空。
 * SDL 层采用相同的 15 项有效容量；满时丢弃新到的 key down。 */
#define FD2_INPUT_QUEUE_CAPACITY 15u

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
    fd2_input_event queue[FD2_INPUT_QUEUE_CAPACITY];
    size_t head;
    size_t count;
    uint32_t dropped_keys;
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

/* 唯一读取 SDL 事件队列的入口。KEY_DOWN（包括 repeat）按到达顺序入队；
 * KEY_UP 不影响原版 BIOS make-code 缓冲，不进入队列。 */
void fd2_input_pump(fd2_input *input);

/* 测试和无窗口验证入口；成功入队返回 0，满队列返回 -1。 */
int fd2_input_push_key(fd2_input *input, fd2_input_key key,
                       SDL_Scancode source, int repeat);

/* 对应原版 input_check：只查看是否已有键，不消费队首。 */
int fd2_input_has_any_key(const fd2_input *input);
size_t fd2_input_pending_count(const fd2_input *input);

/* 消费一项原始输入；无项时返回 0。 */
int fd2_input_take_key(fd2_input *input, fd2_input_event *event);

/* 取出一项并按当前 UI 上下文翻译。未知按键也会被消费并返回 NONE，
 * 对应原版 INT 16h 读到未处理扫描码后继续菜单循环。 */
int fd2_input_take_action(fd2_input *input, fd2_input_context context,
                          fd2_input_action *action);

/* SDL_EVENT_QUIT 是宿主窗口请求，不冒充 DOS 按键；读取后清除请求。 */
int fd2_input_take_quit(fd2_input *input);

/* 纯映射接口，供 CTest 直接验证原版上下文键表。 */
fd2_input_action fd2_input_action_for_key(fd2_input_context context,
                                          fd2_input_key key);

#endif /* FD2_INPUT_H */
