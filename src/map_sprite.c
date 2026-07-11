/* 炎龙骑士团 2 SDL3 重写 - FDICON.B24 / FD2.TMP 地图角色帧
 * 逆向依据：fdicon_cache_append_unit @0xe761 从 FDICON.B24 建立缓存、
 * fd2tmp_map_sprite_load @0x2c0a3 读取 FD2.TMP，以及
 * map_actor_blit_24x24 @0xb168 绘制 24×24 帧。
 */

#include "map_sprite.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int measure_rle_24(const uint8_t *src, size_t src_len, size_t *used) {
    size_t si = 0;
    for (int row = 0; row < 24; row++) {
        int col = 0;
        while (col < 24 && si < src_len) {
            uint8_t c = src[si++];
            size_t count = (size_t)(c & 0x3f) + 1;
            if (c & 0x80) {
                if (c & 0x40) {
                    col += (int)count;
                } else {
                    if (si + count > src_len) return -1;
                    si += count;
                    col += (int)count;
                }
            } else {
                if (si >= src_len) return -1;
                si++;
                col += (c & 0x40) ? (int)count * 2 : (int)count;
            }
        }
        if (col != 24) return -1;
    }
    *used = si;
    return 0;
}

int fd2_map_sprite_bank_open(fd2_map_sprite_bank *bank, const char *path) {
    memset(bank, 0, sizeof(*bank));
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 8) { fclose(f); return -1; }

    uint8_t *data = malloc((size_t)sz);
    if (!data) { fclose(f); return -1; }
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) {
        free(data);
        fclose(f);
        return -1;
    }
    fclose(f);

    size_t count = 0;
    size_t table_offset = 0;
    uint32_t first;

    /* FDICON.B24 是全部 140 个单位的权威 24×24 sprite 源：
     * u16 width, u16 height, u16 frame_count, u32 offsets[]。
     * FD2.EXE 会按 actor id 从这里复制 12 帧到可变的 FD2.TMP 缓存；
     * 完整开场跨三个 stage，不能依赖最后一次运行遗留的 TMP 内容。 */
    if ((size_t)sz >= 10 && rd_u16_le(data) == 24 && rd_u16_le(data + 2) == 24) {
        count = rd_u16_le(data + 4);
        table_offset = 6;
        if (count == 0 || table_offset + count * 4u > (size_t)sz) {
            free(data);
            return -1;
        }
        first = rd_u32_le(data + table_offset);
    } else {
        first = rd_u32_le(data);
        if (first == 0 || first >= (uint32_t)sz) {
            free(data);
            return -1;
        }
        /* FD2.TMP 是运行期缓存。其表项可能按 class 重排，逐项以 RLE
         * 校验，保留该兼容路径供独立调试使用。 */
        for (size_t pos = 0; pos + 4 <= (size_t)first; pos += 4) {
            uint32_t off = rd_u32_le(data + pos);
            if (off == 0 || off < first || off >= (uint32_t)sz) break;
            size_t used = 0;
            if (measure_rle_24(data + off, (size_t)sz - off, &used) != 0) break;
            count++;
        }
    }
    if (first == 0 || first >= (uint32_t)sz || count == 0) {
        free(data);
        return -1;
    }

    uint32_t *offsets = malloc(count * sizeof(*offsets));
    if (!offsets) { free(data); return -1; }
    for (size_t i = 0; i < count; i++) {
        uint32_t off = rd_u32_le(data + table_offset + i * 4u);
        if (off < first || off >= (uint32_t)sz) {
            free(offsets);
            free(data);
            return -1;
        }
        offsets[i] = off;
    }

    /* 140 个单位共 1680 帧，完整解码约占 945 KiB。启动时一次解码可
     * 避免镜头卷动时每个可见 actor 每帧反复 malloc + RLE 展开。 */
    size_t decoded_size = count * 24u * 24u;
    /* RLE SKIP 只推进目标位置，不写透明像素，缓存必须先清零。 */
    uint8_t *decoded_frames = calloc(1, decoded_size);
    if (!decoded_frames) {
        free(offsets);
        free(data);
        return -1;
    }
    for (size_t i = 0; i < count; i++) {
        uint32_t start = offsets[i];
        size_t used = 0;
        if (measure_rle_24(data + start, (size_t)sz - start, &used) != 0 ||
            fd2_image_decode_rle_pixels(decoded_frames + i * 24u * 24u,
                                        24, 24, data + start, used) != 0) {
            free(decoded_frames);
            free(offsets);
            free(data);
            return -1;
        }
    }

    bank->data = data;
    bank->size = (size_t)sz;
    bank->offsets = offsets;
    bank->decoded_frames = decoded_frames;
    bank->frame_count = count;
    return 0;
}

int fd2_map_sprite_decode_frame(fd2_image *img,
                                 const fd2_map_sprite_bank *bank,
                                 size_t frame_idx) {
    memset(img, 0, sizeof(*img));
    if (!bank || !bank->data || !bank->offsets || frame_idx >= bank->frame_count)
        return -1;

    uint32_t start = bank->offsets[frame_idx];
    size_t used = 0;
    if (measure_rle_24(bank->data + start, bank->size - start, &used) != 0)
        return -1;
    uint32_t end = start + (uint32_t)used;
    if (end <= start || end > bank->size) return -1;

    img->width = 24;
    img->height = 24;
    img->pixels = calloc(24u * 24u, 1);
    if (!img->pixels) return -1;

    if (bank->decoded_frames) {
        memcpy(img->pixels,
               bank->decoded_frames + frame_idx * 24u * 24u,
               24u * 24u);
    } else if (fd2_image_decode_rle_pixels(img->pixels, 24, 24,
                                           bank->data + start,
                                           (size_t)(end - start)) != 0) {
        fd2_image_free(img);
        return -1;
    }
    return 0;
}

void fd2_map_sprite_bank_close(fd2_map_sprite_bank *bank) {
    if (!bank) return;
    free(bank->data);
    free(bank->offsets);
    free(bank->decoded_frames);
    memset(bank, 0, sizeof(*bank));
}

void fd2_map_sprite_blit_frame(fd2_vga *vga,
                               const fd2_map_sprite_bank *bank,
                               size_t frame_idx,
                               int x, int y, int transparent_index) {
    if (!bank || !bank->decoded_frames || frame_idx >= bank->frame_count)
        return;
    fd2_image frame = {
        .width = 24,
        .height = 24,
        .pixels = bank->decoded_frames + frame_idx * 24u * 24u,
    };
    fd2_map_sprite_blit(vga, &frame, x, y, transparent_index);
}

void fd2_map_sprite_blit(fd2_vga *vga, const fd2_image *img,
                         int x, int y, int transparent_index) {
    if (!vga || !img || !img->pixels) return;
    for (int row = 0; row < img->height; row++) {
        int dy = y + row;
        if (dy < 0 || dy >= VGA_H) continue;
        for (int col = 0; col < img->width; col++) {
            int dx = x + col;
            if (dx < 0 || dx >= VGA_W) continue;
            uint8_t px = img->pixels[(size_t)row * (size_t)img->width + (size_t)col];
            if (transparent_index >= 0 && px == (uint8_t)transparent_index) continue;
            vga->framebuffer[(size_t)dy * VGA_STRIDE + (size_t)dx] = px;
        }
    }
}
