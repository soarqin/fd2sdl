#ifndef FD2_FIELD_VISUAL_H
#define FD2_FIELD_VISUAL_H

#include <stddef.h>
#include <stdint.h>

#include "archive.h"
#include "image.h"
#include "vga.h"

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

/* 只应用已确认水色到DAC224..239，不改变base palette或phase；非法输入原子拒绝。
 * tick由已确认战场等待caller显式调用；render/present不隐式推进。
 * now_ms以55ms宿主近似转换BIOS低16位，差>=2才推进一次，不追赶。 */
int fd2_field_water_palette_apply(fd2_vga *vga, unsigned phase);
int fd2_field_water_tick(fd2_vga *vga, uint64_t now_ms);

#endif /* FD2_FIELD_VISUAL_H */
