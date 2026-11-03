/* 图像解码依据 blit_image_clipped（旧名 FUN_0004c0d5）@0x4e98d。 */
#include "image.h"

#include <stdlib.h>
#include <string.h>

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/*
 * RLE 解码（源自反编译 FUN_0004c0d5 @0x4e98d）
 *
 * 头部 4 字节: u16 width + u16 height
 * 控制字节 c：
 *   bit7=1,bit6=1 (0xC0-0xFF): SKIP（透明跳过）count=(c&0x3f)+1，不消耗额外数据
 *   bit7=1,bit6=0 (0x80-0xBF): LITERAL count=(c&0x3f)+1，逐字节复制 count 个像素
 *   bit7=0,bit6=1 (0x40-0x7F): RUN stride-2 count=(c&0x3f)+1，下一字节为值，隔字节写
 *   bit7=0,bit6=0 (0x00-0x3F): RUN count=(c&0x3f)+1，下一字节为值，连续写
 *
 * 验证：FDOTHER[0x45-0x49] 片头5帧精确消耗完所有数据，输出恰好 320x147
 */
int fd2_image_decode_rle_pixels(uint8_t *dst, int width, int height,
                                const uint8_t *src, size_t src_len) {
    if (!dst || !src || width <= 0 || height <= 0 ||
        (size_t)width > SIZE_MAX / (size_t)height)
        return -1;
    size_t si = 0;
    for (int row = 0; row < height; row++) {
        int col = 0;
        uint8_t *d = dst + (size_t)row * (size_t)width;
        while (col < width) {
            if (si >= src_len) return -1;
            uint8_t c = src[si++];
            int count = (c & 0x3f) + 1;
            int span = (c & 0xc0) == 0x40 ? count * 2 : count;
            /* 不裁剪损坏指令，否则超行的 RUN 或截断 LITERAL 会假成功。 */
            if (span > width - col) return -1;
            if (c & 0x80) {
                if (!(c & 0x40)) {
                    if ((size_t)count > src_len - si) return -1;
                    memcpy(d + col, src + si, (size_t)count);
                    si += (size_t)count;
                }
            } else {
                if (si >= src_len) return -1;
                uint8_t value = src[si++];
                if (c & 0x40) {
                    for (int k = 0; k < count; k++) d[col + k * 2 + 1] = value;
                } else {
                    memset(d + col, value, (size_t)count);
                }
            }
            col += span;
        }
    }
    return si == src_len ? 0 : -1;
}

int fd2_image_decode_buf(fd2_image *img, const uint8_t *buf, size_t len) {
    if (!img) return -1;
    memset(img, 0, sizeof(*img));
    if (!buf || len < 4) return -1;

    int w = rd_u16_le(buf);
    int h = rd_u16_le(buf + 2);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return -1;

    img->width = w;
    img->height = h;
    img->pixels = malloc((size_t)w * h);
    if (!img->pixels) {
        memset(img, 0, sizeof(*img));
        return -1;
    }
    memset(img->pixels, 0, (size_t)w * h);  /* SKIP 指令依赖清零 */

    if (fd2_image_decode_rle_pixels(img->pixels, w, h, buf + 4, len - 4) != 0) {
        fd2_image_free(img);
        return -1;
    }
    return 0;
}

int fd2_image_decode_entry(fd2_image *img, const fd2_archive *ar, size_t idx) {
    if (!img) return -1;
    memset(img, 0, sizeof(*img));
    const uint8_t *buf;
    size_t len;
    if (fd2_archive_get(ar, idx, &buf, &len) != 0) return -1;
    return fd2_image_decode_buf(img, buf, len);
}

void fd2_image_free(fd2_image *img) {
    free(img->pixels);
    img->pixels = NULL;
    img->width = img->height = 0;
}
