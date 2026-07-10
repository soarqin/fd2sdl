/* 炎龙骑士团 2 SDL3 重写 - DATO 角色立绘解码
 * 逆向依据：FUN_000136cc @0x136cc、FUN_0004c347 @0x4c347、
 * FUN_0004c379 @0x4c379。
 */

#include "portrait.h"

#include <stdlib.h>
#include <string.h>

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int fd2_lmi_decode_image(fd2_image *img, const uint8_t *buf, size_t len) {
    memset(img, 0, sizeof(*img));
    if (!buf || len < 4) return -1;

    int w = rd_u16_le(buf);
    int h = rd_u16_le(buf + 2);
    if (w <= 0 || h <= 0 || w > 640 || h > 480) return -1;

    size_t total = (size_t)w * (size_t)h;
    uint8_t *pixels = malloc(total);
    if (!pixels) return -1;

    size_t si = 4;
    size_t di = 0;
    while (di < total && si < len) {
        uint8_t c = buf[si++];
        if (c >= 0xc1u) {
            if (si >= len) { free(pixels); return -1; }
            uint8_t value = buf[si++];
            size_t count = (size_t)c - 0xc0u;
            if (di + count > total) { free(pixels); return -1; }
            memset(pixels + di, value, count);
            di += count;
        } else {
            pixels[di++] = c;
        }
    }

    if (di != total || si != len) {
        free(pixels);
        return -1;
    }

    img->width = w;
    img->height = h;
    img->pixels = pixels;
    return 0;
}

int fd2_portrait_decode_frame(fd2_image *img,
                              const fd2_archive *dato,
                              size_t portrait_idx,
                              size_t frame_idx) {
    const uint8_t *entry;
    size_t entry_size;
    if (!dato || fd2_archive_get(dato, portrait_idx, &entry, &entry_size) != 0)
        return -1;
    if (entry_size < 8) return -1;

    uint32_t first = rd_u32_le(entry);
    if (first == 0 || (first % 4) != 0 || first > entry_size) return -1;
    size_t frame_count = first / 4;
    if (frame_idx >= frame_count) return -1;

    uint32_t start = rd_u32_le(entry + frame_idx * 4);
    uint32_t end = (frame_idx + 1 < frame_count)
                 ? rd_u32_le(entry + (frame_idx + 1) * 4)
                 : (uint32_t)entry_size;
    if (start < first || start >= entry_size || end > entry_size || end <= start)
        return -1;

    return fd2_lmi_decode_image(img, entry + start, (size_t)(end - start));
}
