#ifndef FD2_FIELD_INFO_H
#define FD2_FIELD_INFO_H

#include <stddef.h>
#include <stdint.h>

#include "archive.h"
#include "font.h"
#include "image.h"
#include "text.h"
#include "field_unit.h"
#include "vga.h"

#define FD2_FIELD_INFO_PANEL_FRAME 130u
#define FD2_FIELD_INFO_PLUS_FRAME 131u
#define FD2_FIELD_INFO_MINUS_FRAME 132u
#define FD2_FIELD_INFO_DIGIT_FRAME 31u

typedef struct {
    fd2_image panel;
    fd2_image signs[2]; /* 0=正号，1=负号 */
    fd2_image digits[3][10]; /* frame 31..40、42..51、119..128 */
    fd2_image digit_overflow[3]; /* frame 41/52/129 */
    fd2_image two_digit_overflow; /* frame 93 */
    fd2_image detail_border[18]; /* frame 1..17：通用 UI 边框小块 */
    fd2_image detail_top;    /* frame 20: 223×86 */
    fd2_image detail_bottom; /* frame 21: 310×99 */
    fd2_image affiliation[2];/* frame 53/54 */
    fd2_image status_icons[3]; /* frame 55..57 */
    fd2_image bars[2];       /* frame 23/26: HP/MP 1×5 色条 */
    fd2_image detail_icons[9]; /* frame 59..67 */
    int ready;
} fd2_field_info_assets;

/* FDOTHER[5] 战场格子/单位信息面板资源。
 * field_cell_info_panel_draw_entry @0x3ff07 / body @0x3ff11 使用 frame 130
 * 作为 69×34 底板，frame 131/132 为正负号，frame 31..40 与 42..51
 * 分别为满 HP 和受伤状态的数字。 */
/* assets 首次调用前必须清零；已 ready 的对象可直接重新打开。 */
int fd2_field_info_assets_open(fd2_field_info_assets *assets,
                               const fd2_archive *fdother);
int fd2_field_info_assets_open_mem(fd2_field_info_assets *assets,
                                   const uint8_t *data, size_t size);
void fd2_field_info_assets_close(fd2_field_info_assets *assets);

/* panel_right=0 时逻辑坐标为 (5,161)，非 0 时为 (246,161)。terrain 和
 * unit 均绘制到左侧 24×24 格；unit 可为空。 */
void fd2_field_info_draw(fd2_vga *vga,
                         const fd2_field_info_assets *assets,
                         int panel_right,
                         const fd2_image *terrain,
                         const fd2_image *unit,
                         uint16_t hp,
                         uint16_t hp_max,
                         int attack_modifier,
                         int defense_modifier);

/* 全屏角色详情页主体：复现 field_unit_detail_draw @0x3d103 与
 * field_unit_detail_stats_draw @0x3d1de。portrait 为 DATO 当前单位首帧。 */
void fd2_field_detail_draw(fd2_vga *vga,
                           const fd2_field_info_assets *assets,
                           const fd2_font *font,
                           const fd2_text_entry *text,
                           const fd2_field_unit *unit,
                           const fd2_image *portrait);

/* field_unit_detail_transition_frame @0x3d61d：phase 11→0 打开，
 * 0→11 关闭；每帧从 background 重建三段开合画面。 */
void fd2_field_detail_transition_frame(uint8_t *dst,
                                       const uint8_t *detail,
                                       const uint8_t *background,
                                       int phase);

/* field_unit_detail_open @0x3d01f：打开 phase 11/5 播放 SFX 5；
 * 调用方关闭循环 phase 0/7 播放 SFX 6。其他 phase 返回 -1。 */
int fd2_field_detail_sfx_for_phase(int opening, int phase);

#endif /* FD2_FIELD_INFO_H */
