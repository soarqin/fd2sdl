/* 炎龙骑士团 2 SDL3 重写 - 标题菜单反馈状态
 *
 * 逆向依据：title_action_menu @code0 0xf894：
 * - animation_play @code0 0x1043f..0x10552 为 ANI[1] 加载
 *   FDOTHER[78]，并在 frame 0 解码后、首个 delay 前播放 SFX 0；
 * - 0xf8d6..0xf8e6 将 FDOTHER[77] 保存到 local +0x54；
 * - 片头滚动期间以 primary handle 播放 FDOTHER[77] SFX 0；
 * - 标题入口以 secondary handle 播放 SFX 3；
 * - Up／Down 以 primary handle 播放 SFX 2；
 * - 确认以 primary handle 播放 SFX 1；
 * - 确认后选中项按 normal／highlight 交替 4 轮，每帧 80 ms。
 */
#ifndef FD2_TITLE_H
#define FD2_TITLE_H

#include <stddef.h>

typedef enum {
    FD2_TITLE_SFX_FLIGHT = 0,
    FD2_TITLE_SFX_ENTER,
    FD2_TITLE_SFX_MOVE,
    FD2_TITLE_SFX_CONFIRM,
} fd2_title_sfx;

typedef enum {
    FD2_TITLE_SFX_HANDLE_PRIMARY = 0,
    FD2_TITLE_SFX_HANDLE_SECONDARY,
} fd2_title_sfx_handle;

#define FD2_TITLE_SFX_BANK 77
#define FD2_TITLE_LETTER_FLY_SFX_BANK 78
#define FD2_TITLE_LETTER_FLY_SFX_SAMPLE 0
#define FD2_TITLE_FLIGHT_TRIGGER_COUNT 14
#define FD2_TITLE_LIGHTNING_TRIGGER_COUNT FD2_TITLE_FLIGHT_TRIGGER_COUNT
#define FD2_TITLE_LIGHTNING_FLASH_FRAMES 11
#define FD2_TITLE_SCROLL_DELAY_MS 30
#define FD2_TITLE_ANIM_DELAY_SLOW_MS 90
#define FD2_TITLE_ANIM_DELAY_MEDIUM_MS 50
#define FD2_TITLE_ANIM_DELAY_FAST_MS 15

typedef struct {
    unsigned frames_remaining;
} fd2_title_lightning_state;

#define FD2_TITLE_CONFIRM_FLASH_CYCLES 4
#define FD2_TITLE_CONFIRM_FLASH_FRAMES (FD2_TITLE_CONFIRM_FLASH_CYCLES * 2)
#define FD2_TITLE_CONFIRM_FLASH_DELAY_MS 80

int fd2_title_sfx_resolve(fd2_title_sfx cue,
                          fd2_title_sfx_handle *handle,
                          size_t *sample_index);

/* object2 DS:0x204e 的 15 项降序阈值；title_action_menu 每次滚动位置
 * 命中下一项时以 primary handle 重启 SFX 0。末项 1000 不会被
 * scroll_y=0x217..0 命中，因此有效触发共 14 次。 */
int fd2_title_flight_sfx_for_scroll_y(int scroll_y);

/* 14 个滚动音效阈值都会重启一次闪电。每个闪电保持 11 个完整滚动
 * 帧，随后下一帧恢复 FDOTHER[101]；这组调色板差异构成打雷发光效果。 */
int fd2_title_lightning_trigger_for_scroll_y(int scroll_y);
void fd2_title_lightning_state_init(fd2_title_lightning_state *state);
int fd2_title_lightning_state_advance(fd2_title_lightning_state *state,
                                      int scroll_y);

/* frame 0 先显示 normal，frame 1 恢复 highlight；共 8 帧。
 * 对应 code0 0xfef0..0xff30 的两次 draw + 80 ms、循环 4 次。 */
int fd2_title_confirm_highlight_for_frame(int frame);

/* 以 normal/highlight 成对资源重绘完整菜单。返回实际 blit 次数；
 * draw 回调便于主程序渲染和 CTest 精确验证帧选择。 */
typedef void (*fd2_title_draw_item_fn)(void *userdata,
                                       int item_index,
                                       int image_index);
int fd2_title_draw_menu_frame(int menu_count,
                              int selection,
                              int highlight_selected,
                              fd2_title_draw_item_fn draw,
                              void *userdata);

#endif /* FD2_TITLE_H */
