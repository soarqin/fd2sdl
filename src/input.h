/* 炎龙骑士团 2 SDL3 重写 - 单owner普通键缓存
 *
 * 原版键表依据：input_check @code0 0x620（VA 0x10620）比较 BIOS BDA
 * 0x041a/0x041c；具体 UI 读取通过 INT 16h/AH=10h 消费一项按键。
 * input_check不消费；16c57读一项；4e381明确清队。宿主事件、物理
 * held/repeat及有限有序缓存均归本结构，不随present或Game Load重建。
 */
#ifndef FD2_INPUT_H
#define FD2_INPUT_H

#include <stdbool.h>
#include <stdint.h>

#include <SDL3/SDL.h>
#include "bindings.h"

/* 固定DOSBox-X b6abbd5 keyboard.cpp::KEYBOARD_Reset：整数1ms服务tick。
 * 不是BIOS 18.2Hz tick，也不是FD2内部常量；初始化/AL00均为500/33。 */
#define FD2_INPUT_INITIAL_REPEAT_MS 500u
#define FD2_INPUT_REPEAT_MS 33u
/* DOSBox-X dosbox-x-v2026.10.01(b6abbd5) bios_keyboard.cpp:370..456：
 * 普通IBM/non-DOSV的BDA 1e..3e有16个word槽，留一空槽，有效15/满队丢新。
 * 本项目svga_s3证据；不是FD2常量或DOS/V额外缓存合同。quit不占槽。 */
#define FD2_INPUT_KEY_QUEUE_CAPACITY 15u

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
    uint16_t modifiers; /* 首按用该键DOWN的修饰位，repeat用本帧物理位；不生成modifier键。 */
    uint16_t bios_code; /* IBM INT16/AH10的AX；含AL，非游戏普通键同样保留。 */
} fd2_input_event;

typedef struct {
    /* 物理生命周期用于首按去重与唯一repeat源；普通待取键只有下方ring。 */
    bool physical_down[SDL_SCANCODE_COUNT];
    uint16_t sources_down[SDL_SCANCODE_COUNT], chord_sources[3];
    SDL_Keymod chord_mods;
    SDL_Gamepad *gamepad;
    SDL_JoystickID pad_id;
    bool pad_down[FD2_PAD_SOURCES], pad_unfocused;
    int stick_x, stick_y, stick_source;
    bool previous_down[SDL_SCANCODE_COUNT]; /* 虚拟键生命周期，供 BIOS 及唯一重复源使用。 */
    uint64_t next_repeat_ms[SDL_SCANCODE_COUNT]; /* 仅repeat_key项非0；保既有观察接口。 */
    SDL_Scancode repeat_key; /* 原硬件只有一个last-key；release不恢复旧held。 */
    uint64_t repeat_sample_ms;
    uint16_t held_modifiers, lock_modifiers; /* BIOS flags；F13/PrtSc fake Shift可与previous_down物理态不同。 */
    uint8_t alt_token; /* 原BIOS Alt+数字小键盘BYTE累积，末Alt释放才入队。 */
    bool test_key_state[SDL_SCANCODE_COUNT];
    int use_test_key_state;
    uint64_t test_now_ms; /* 测试pump/clear共用最近一次确定性时钟，不混真实uptime。 */
    int quit_requested;
    fd2_input_event queued_keys[FD2_INPUT_KEY_QUEUE_CAPACITY];
    uint8_t queued_head,queued_count;
} fd2_input;

typedef enum {
    FD2_INPUT_CONTEXT_TITLE = 0,
    FD2_INPUT_CONTEXT_CHOICE,
    FD2_INPUT_CONTEXT_FIELD,
    FD2_INPUT_CONTEXT_FIELD_MOVEMENT,
    FD2_INPUT_CONTEXT_FIELD_COMMAND,
    FD2_INPUT_CONTEXT_FIELD_TARGETING,
    FD2_INPUT_CONTEXT_FIELD_SYSTEM_MENU,
    FD2_INPUT_CONTEXT_FIELD_MANUAL_SLOT,
    FD2_INPUT_CONTEXT_FIELD_AUXILIARY,
    FD2_INPUT_CONTEXT_PREVIEW,
    FD2_INPUT_CONTEXT_FIELD_MAGIC_TARGETING,
    FD2_INPUT_CONTEXT_FIELD_DETAIL
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
void fd2_input_connect_gamepad(fd2_input *input);
void fd2_input_close(fd2_input *input);
void fd2_input_install_interrupt_handlers(void);
int fd2_input_host_quit_requested(void);

/* 唯一宿主事件泵：普通DOWN入同owner缓存；UP不删除已接受的键。
 * modifier也参与原last-key仲裁但不入普通FIFO；忽略SDL自带repeat。 */
void fd2_input_poll_host_events(fd2_input *input);

/* 每次呈现调用一次物理采样；不清普通键缓存。 */
void fd2_input_begin_frame(fd2_input *input);

/* 非消费式peek／按到达顺序取一项；均不会主动pump或清剩余队列。 */
int fd2_input_has_any_key(const fd2_input *input);
int fd2_input_take_key(fd2_input *input, fd2_input_event *event);
int fd2_input_take_action(fd2_input *input, fd2_input_context context,
                          fd2_input_action *action);
/* 4e381的明确清键：同owner pump一次后清普通缓存，保held/repeat和quit。 */
void fd2_input_clear_keys(fd2_input *input);

/* 确定性测试入口：注入物理键态，并以指定时间建立呈现帧。 */
void fd2_input_set_test_key_state(fd2_input *input, SDL_Scancode source,
                                  int pressed);
void fd2_input_begin_test_frame(fd2_input *input, uint64_t now_ms);

int fd2_input_take_quit(fd2_input *input);
/* 准备31B记录比较INT16 AH功能键scan；仅映射F1..F12及Shift/Ctrl/Alt。
 * 首按mod归该入队event，repeat使用其生成时的物理mod。 */
uint8_t fd2_input_function_scan(const fd2_input_event *event);
fd2_input_action fd2_input_action_for_key(fd2_input_context context,
                                          fd2_input_key key);

#endif /* FD2_INPUT_H */
