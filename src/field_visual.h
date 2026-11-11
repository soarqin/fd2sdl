#ifndef FD2_FIELD_VISUAL_H
#define FD2_FIELD_VISUAL_H

#include <stddef.h>
#include <stdint.h>

#include "archive.h"
#include "image.h"
#include "vga.h"
#include "field_visual_state.h"

#define FD2_FIELD_CURSOR_FRAME_COUNT 20
#define FD2_FIELD_RANGE_LUT_COUNT 23
#define FD2_FIELD_RANGE_PHASE_COUNT 20

typedef struct {
    fd2_image cursor_frames[FD2_FIELD_CURSOR_FRAME_COUNT];
    uint8_t range_luts[FD2_FIELD_RANGE_LUT_COUNT][256];
    size_t cursor_frame_count;
    size_t range_lut_count;
} fd2_field_visuals;

/* FDOTHER[1]：24×24、20 帧的战场选择框 shape sheet。
 * FDOTHER[3]：LMI1 容器，23 帧均为 256 字节 palette translation LUT。 */
int fd2_field_visuals_open(fd2_field_visuals *visuals,
                           const fd2_archive *fdother);
void fd2_field_visuals_close(fd2_field_visuals *visuals);

const fd2_image *fd2_field_cursor_frame(const fd2_field_visuals *visuals,
                                        size_t frame_index);
const uint8_t *fd2_field_range_lut(const fd2_field_visuals *visuals,
                                   size_t phase);
/* 直接按 FDOTHER[3] LMI1 frame 取 LUT；转场使用 frame 9→1，不能套用
 * DS:0x1a97 的移动范围 phase 映射。 */
const uint8_t *fd2_field_lut_frame(const fd2_field_visuals *visuals,
                                   size_t frame_index);

/* 把现有 framebuffer 矩形中的 palette index 逐像素映射。 */
void fd2_field_apply_palette_lut(fd2_vga *vga,
                                 int x, int y, int width, int height,
                                 const uint8_t lut[256]);

/* field_transition_lut_mask @0x22046（原登记FUN_0004725a）：视区(4,4),312×192。
 * 调用者须在两pass之间重画actor/actor前景；只改framebuffer。半宽按原CHP
 * 向零截断，圆横比1.6；radius须1..4096，非法输入无写入。 */
int fd2_field_transition_lut_before_actors(fd2_vga *vga,
    const uint8_t lut[256], int center_x, int center_y, int radius);
int fd2_field_transition_lut_after_actors(fd2_vga *vga,
    const uint8_t lut[256], int center_x, int center_y, int radius);
/* 22253/22547/22656的radius固定11，first_row独立变化；旧接口等价row0。 */
int fd2_field_transition_lut_before_actors_from(fd2_vga *vga,
    const uint8_t lut[256], int center_x, int center_y, int radius, int first_row);
int fd2_field_transition_lut_after_actors_from(fd2_vga *vga,
    const uint8_t lut[256], int center_x, int center_y, int radius, int first_row);
/* 2189a→219ad固定scale12的单圆；不包含22046的actor重画或矩形。 */
int fd2_field_transition_lut_circle12(fd2_vga *vga,
    const uint8_t lut[256], int center_x, int center_y, int radius);
/* 同一219ad/22046几何直接应用实际456页内区。scratch只借同page像素，
 * caller在before/after之间按源调用page actors/foreground；不重铺terrain。
 * 不投影、不推进clock，不把当前VGA回灌page。 */
int fd2_field_transition_page_before_actors_from(fd2_field_page *page,
    const uint8_t lut[256],int center_x,int center_y,int radius,int first_row);
int fd2_field_transition_page_after_actors_from(fd2_field_page *page,
    const uint8_t lut[256],int center_x,int center_y,int radius,int first_row);
int fd2_field_transition_page_circle12(fd2_field_page *page,
    const uint8_t lut[256],int center_x,int center_y,int radius);
/* 127e0的53a04/53a08独立共享抖动门，与1297d的idle相位不同。
 * 原低WORD tick变化才翻0/1一次；App视觉owner，不入Game/SAV/RNG。 */
int fd2_field_actor_visual_tick(fd2_vga *vga, uint64_t now_ms);

/* 只应用已确认水色到DAC224..239，不改变base palette或phase；非法输入原子拒绝。
 * tick由已确认战场等待caller显式调用；render/present不隐式推进。
 * now_ms以55ms宿主近似转换BIOS低16位，差>=2才推进一次，不追赶。 */
int fd2_field_water_palette_apply(fd2_vga *vga, unsigned phase);
int fd2_field_water_tick(fd2_vga *vga, uint64_t now_ms);

/* field_auxiliary_marker_draw @0x1f6ef（原sub_1F6EF）：object2 5208a的3 DWORD阵营色，
 * 加本轮0..7闪烁相位；非法域不写输出，不访问Game/App/RNG。 */
int fd2_field_auxiliary_marker_color(unsigned side,unsigned phase,uint8_t *color);

#endif /* FD2_FIELD_VISUAL_H */
