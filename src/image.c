#include "image.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/*
 * RLE 解码（源自反编译 FUN_0004c0d5 @0x4c0d5）
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
static int decode_rle(const uint8_t *src, size_t src_len,
                       uint8_t *dst, int width, int height) {
    /* 逐行解码，对应 FUN_0004c0d5 @0x4c0d5 的双层循环:
     *   外层遍历 height 行，每行用宽度计数器从 width 减到 0 */
    size_t si = 0;
    for (int row = 0; row < height; row++) {
        int col = 0;  /* 当前行已处理宽度 */
        uint8_t *d = dst + (size_t)row * width;
        while (col < width && si < src_len) {
            uint8_t c = src[si++];
            size_t count;
            uint8_t val;
            if (c & 0x80) {
                if (c & 0x40) {
                    /* SKIP（透明跳过，dst 已清零） */
                    count = (size_t)(c & 0x3f) + 1;
                    col += count;
                } else {
                    /* LITERAL */
                    count = (size_t)(c & 0x3f) + 1;
                    for (size_t k = 0; k < count && col < width; k++) {
                        if (si >= src_len) return -1;
                        d[col++] = src[si++];
                    }
                }
            } else {
                if (c & 0x40) {
                    /* RUN stride-2（隔字节写，占 2*count 宽度） */
                    count = (size_t)(c & 0x3f) + 1;
                    if (si >= src_len) return -1;
                    val = src[si++];
                    for (size_t k = 0; k < count && col + 1 < width; k++) {
                        d[col + 1] = val;
                        col += 2;
                    }
                } else {
                    /* RUN */
                    count = (size_t)(c & 0x3f) + 1;
                    if (si >= src_len) return -1;
                    val = src[si++];
                    for (size_t k = 0; k < count && col < width; k++) {
                        d[col++] = val;
                    }
                }
            }
        }
        if (col != width) return -1;  /* 行宽度不匹配 */
    }
    if (si != src_len) return -1;  /* 数据未消耗完 */
    return 0;
}

int fd2_image_decode_buf(fd2_image *img, const uint8_t *buf, size_t len) {
    memset(img, 0, sizeof(*img));
    if (len < 4) return -1;

    int w = rd_u16_le(buf);
    int h = rd_u16_le(buf + 2);
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return -1;

    img->width = w;
    img->height = h;
    img->pixels = malloc((size_t)w * h);
    if (!img->pixels) return -1;
    memset(img->pixels, 0, (size_t)w * h);  /* SKIP 指令依赖清零 */

    if (decode_rle(buf + 4, len - 4, img->pixels, w, h) != 0) {
        free(img->pixels);
        img->pixels = NULL;
        return -1;
    }
    return 0;
}

int fd2_image_decode_entry(fd2_image *img, const fd2_archive *ar, size_t idx) {
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
