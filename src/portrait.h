#ifndef FD2_PORTRAIT_H
#define FD2_PORTRAIT_H

#include <stddef.h>

#include "archive.h"
#include "image.h"

/* DATO.DAT 角色立绘包
 *
 * 逆向依据：FUN_000136cc @0x3b198 遇到 FDTXT 控制码 -0x11..-0x14
 * 时按角色/单位编号从 &DAT_00001a70 加载条目；FUN_0004c347 / FUN_0004c379
 * 使用一种简单 RLE 展开立绘帧。DATO 条目开头是 u32 帧偏移表，
 * first_offset / 4 即帧数；每帧有 u16 width/u16 height 头。
 */
/* DATO 与 LMI1 动画帧共用的 c>=0xc1 run-length 编码。 */
int fd2_lmi_decode_image(fd2_image *img, const uint8_t *buf, size_t len);

int fd2_portrait_decode_frame(fd2_image *img,
                              const fd2_archive *dato,
                              size_t portrait_idx,
                              size_t frame_idx);

#endif /* FD2_PORTRAIT_H */
