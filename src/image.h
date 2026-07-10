#ifndef FD2_IMAGE_H
#define FD2_IMAGE_H

#include <stdint.h>
#include "archive.h"

/* 炎龙骑士团 2 图像解码
 * 图像条目结构：
 *   [0:2] width  (u16 LE)
 *   [2:4] height (u16 LE)
 *   [4:]  RLE 压缩像素（8bpp 索引色）
 *
 * RLE 方案源自 FUN_0004c0d5 @0x4c0d5 (blit_image_clipped)，
 * 详见 docs/03-data-formats.md §7。
 */

typedef struct {
    int      width;
    int      height;
    uint8_t *pixels;   /* width*height 字节，8bpp 索引色 */
} fd2_image;

/* 从归档条目解码图像。idx 为 -1 时用 raw（u16/u16/pixels）。
 * 成功返回 0。 */
int fd2_image_decode_entry(fd2_image *img, const fd2_archive *ar, size_t idx);

/* 直接从内存缓冲解码（条目已取出）。 */
int fd2_image_decode_buf(fd2_image *img, const uint8_t *buf, size_t len);

/* 解码无 4 字节宽高头的 RLE 像素流。
 * FDSHAP.DAT 精灵帧共用同一控制字节方案，但宽高来自精灵包头。 */
int fd2_image_decode_rle_pixels(uint8_t *dst, int width, int height,
                                const uint8_t *src, size_t src_len);

void fd2_image_free(fd2_image *img);

#endif /* FD2_IMAGE_H */
