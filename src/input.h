/* 炎龙骑士团 2 SDL3 重写 - 统一帧输入抽象
 *
 * 原版键表依据：input_check @0x35834 与 field_key_read @0x36cbc。
 * SDL 端只保留当前宿主帧到达的 KEY_DOWN；未消费事件不会跨帧或跨 UI
 * 状态保留。完整键表与帧输入约束见 docs/systems/input.md。
 */
#ifndef FD2_INPUT_H
#define FD2_INPUT_H

#include <stddef.h>
#include <stdint.h>

#include <SDL3/SDL.h>

/* 单个宿主帧内允许的 KEY_DOWN 上限。事件只在本帧有效；下一次
 * fd2_input_begin_frame() 会无条件丢弃未消费项。 */
#define FD2_INPUT_FRAME_EVENT_CAPACITY 32u

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
    fd2_input_event frame_events[FD2_INPUT_FRAME_EVENT_CAPACITY];
    size_t frame_event_count;
    size_t frame_event_cursor;
    uint32_t dropped_frame_keys;
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

/* 安装宿主中断桥：POSIX SIGINT/SIGTERM 与 Windows Ctrl+C/Ctrl+Break
 * 都转成统一、持续有效的 quit 请求，不混入普通帧按键。 */
void fd2_input_install_interrupt_handlers(void);

/* 查询进程级宿主中断状态。该状态由信号／控制台回调置位，在 main 完成
 * 统一清理前保持有效；不能由某个场景消费后让其他等待循环失去退出请求。 */
int fd2_input_host_quit_requested(void);

/* 仅泵送并窥视宿主退出事件，不收集普通按键。共享 deadline／delay
 * 使用该入口缩短窗口关闭与控制台中断的退出延迟。 */
void fd2_input_poll_host_events(void);

/* 开始一个输入帧，也是唯一读取 SDL 事件队列的入口。调用时先清除上一帧
 * 所有未消费按键，再收集当前已到达的 KEY_DOWN。方向键 repeat 保留；
 * 确认／取消键 repeat 丢弃；KEY_UP 不进入帧事件。 */
void fd2_input_begin_frame(fd2_input *input);

/* 测试和无窗口验证入口；向当前帧注入事件。帧容量满时返回 -1。 */
int fd2_input_push_key(fd2_input *input, fd2_input_key key,
                       SDL_Scancode source, int repeat);

/* 查看／消费当前帧事件；均不会访问上一帧。 */
int fd2_input_has_any_key(const fd2_input *input);
size_t fd2_input_pending_count(const fd2_input *input);

/* 消费当前帧的一项原始输入；无项时返回 0。 */
int fd2_input_take_key(fd2_input *input, fd2_input_event *event);

/* 取出当前帧一项并按 UI 上下文翻译。未知按键也会被消费并返回 NONE。 */
int fd2_input_take_action(fd2_input *input, fd2_input_context context,
                          fd2_input_action *action);

/* SDL_EVENT_QUIT／SDL_EVENT_TERMINATING／宿主中断是退出请求，不冒充 DOS
 * 按键。请求在整个进程退出前保持有效；take 名称仅为兼容既有调用点。 */
int fd2_input_take_quit(fd2_input *input);

/* 纯映射接口，供 CTest 直接验证原版上下文键表。 */
fd2_input_action fd2_input_action_for_key(fd2_input_context context,
                                          fd2_input_key key);

#endif /* FD2_INPUT_H */
