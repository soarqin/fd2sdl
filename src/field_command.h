/* 炎龙骑士团 2 SDL3 重写 - 战场图形指令菜单
 *
 * 逆向依据：field_command_menu_open @code0 0x741c、
 * field_command_menu_input @code0 0x77fc、field_command_menu_close
 * @code0 0x76b4
 * 与 field_player_command_execute @0x3dfa0。
 */
#ifndef FD2_FIELD_COMMAND_H
#define FD2_FIELD_COMMAND_H

#include <stddef.h>
#include <stdint.h>

#include "archive.h"
#include "image.h"
#include "vga.h"

#define FD2_FIELD_COMMAND_COUNT 4u
#define FD2_FIELD_COMMAND_FRAME_COUNT 78u
#define FD2_FIELD_COMMAND_ICON_WIDTH 24
#define FD2_FIELD_COMMAND_ICON_HEIGHT 20

typedef enum {
    FD2_FIELD_COMMAND_ATTACK = 0,
    FD2_FIELD_COMMAND_MAGIC = 1,
    FD2_FIELD_COMMAND_ITEM = 2,
    FD2_FIELD_COMMAND_WAIT = 3
} fd2_field_command;

typedef enum {
    FD2_FIELD_COMMAND_DIRECTION_UP = 0,
    FD2_FIELD_COMMAND_DIRECTION_LEFT = 1,
    FD2_FIELD_COMMAND_DIRECTION_RIGHT = 2,
    FD2_FIELD_COMMAND_DIRECTION_DOWN = 3
} fd2_field_command_direction;

typedef struct {
    fd2_image frames[FD2_FIELD_COMMAND_FRAME_COUNT];
    size_t frame_count;
    int ready;
} fd2_field_command_assets;

/* FDOTHER.DAT[2] 是 78 项 offset 表；前 12 帧按
 * attack/magic/item/wait × normal/highlight/disabled 排列，后续帧
 * 供 field-level 系统菜单使用。每帧为 u16 width + u16 height
 * + 未压缩索引色像素；frame 48/49/51/52 为 24×16，其余为 24×20。 */
int fd2_field_command_assets_open(fd2_field_command_assets *assets,
                                  const fd2_archive *fdother);
void fd2_field_command_assets_close(fd2_field_command_assets *assets);
const fd2_image *fd2_field_command_frame(
    const fd2_field_command_assets *assets,
    fd2_field_command command, int selected, int disabled,
    uint8_t highlight_phase);
const fd2_image *fd2_field_command_frame_id(
    const fd2_field_command_assets *assets, uint8_t command_id,
    int selected, int disabled, uint8_t highlight_phase);

/* 原版菜单不是线性列表：四个方向直接选择上／左／右／下的
 * attack/magic/item/wait；禁用项不改变当前选择。 */
int fd2_field_command_first_enabled(const uint8_t disabled[FD2_FIELD_COMMAND_COUNT]);
int fd2_field_command_select_direction(
    int current, fd2_field_command_direction direction,
    const uint8_t disabled[FD2_FIELD_COMMAND_COUNT]);

/* focus_x/focus_y 是当前单位 24×24 地图格的屏幕左上角。
 * 图标最终位置严格为上 (0,-18)、左 (-24,2)、右 (24,2)、下 (0,22)。 */
void fd2_field_command_draw_animation(
    fd2_vga *vga, const fd2_field_command_assets *assets,
    int focus_x, int focus_y, int selected,
    const uint8_t disabled[FD2_FIELD_COMMAND_COUNT],
    uint8_t highlight_phase, int opening, uint8_t animation_phase);
void fd2_field_command_draw(
    fd2_vga *vga, const fd2_field_command_assets *assets,
    int focus_x, int focus_y, int selected,
    const uint8_t disabled[FD2_FIELD_COMMAND_COUNT],
    uint8_t highlight_phase);

/* 通用 field-level 四向绘制 primitive。系统菜单传入原版 command ID，
 * 不复用单位行动 enum。 */
void fd2_field_command_draw_id_animation(
    fd2_vga *vga, const fd2_field_command_assets *assets,
    int focus_x, int focus_y, const uint8_t command_ids[FD2_FIELD_COMMAND_COUNT],
    int selected, const uint8_t disabled[FD2_FIELD_COMMAND_COUNT],
    uint8_t highlight_phase, int opening, uint8_t animation_phase);

#endif /* FD2_FIELD_COMMAND_H */
