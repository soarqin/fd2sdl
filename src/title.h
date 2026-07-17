/* 炎龙骑士团 2 SDL3 重写 - 标题菜单反馈状态
 *
 * 逆向依据：title_action_menu @code0 0xf894：
 * - 标题入口播放 FDOTHER[77] SFX 3；
 * - Up／Down 播放 FDOTHER[31] SFX 2；
 * - 确认播放 FDOTHER[31] SFX 1；
 * - 确认后选中项按 normal／highlight 交替 4 轮，每帧 80 ms。
 */
#ifndef FD2_TITLE_H
#define FD2_TITLE_H

#include <stddef.h>

typedef enum {
    FD2_TITLE_SFX_ENTER = 0,
    FD2_TITLE_SFX_MOVE,
    FD2_TITLE_SFX_CONFIRM,
} fd2_title_sfx;

typedef enum {
    FD2_TITLE_SFX_BANK_UI = 31,
    FD2_TITLE_SFX_BANK_ENTRY = 77,
} fd2_title_sfx_bank;

#define FD2_TITLE_CONFIRM_FLASH_CYCLES 4
#define FD2_TITLE_CONFIRM_FLASH_FRAMES (FD2_TITLE_CONFIRM_FLASH_CYCLES * 2)
#define FD2_TITLE_CONFIRM_FLASH_DELAY_MS 80

int fd2_title_sfx_resolve(fd2_title_sfx cue,
                          fd2_title_sfx_bank *bank,
                          size_t *sample_index);

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
