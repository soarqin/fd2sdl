/* 炎龙骑士团 2 SDL3 重写 - 战场选择框与可达范围视觉资源
 *
 * 逆向依据：field_selection_overlay_draw @0x374f0、
 * field_selection_sprite_blit @0x3790b、field_view_render_tiles @0x3710c
 * 与 shape_blit_palette_lut_24x24 @0x7322a、透明变体 @0x732b6。
 * FDOTHER[1] 提供 24×24
 * 选择框帧，FDOTHER[3] 提供 256 字节 palette translation LUT。
 */

#include "field_visual.h"

#include <stdlib.h>
#include <string.h>

#include "shape.h"

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8);
}

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int load_cursor_frames(fd2_field_visuals *visuals,
                              const fd2_archive *fdother) {
    const uint8_t *data;
    size_t size;
    fd2_shape_sheet sheet = {0};

    if (fd2_archive_get(fdother, 1, &data, &size) != 0 ||
        fd2_shape_sheet_open_mem(&sheet, data, size) != 0)
        return -1;
    if (sheet.width != 24 || sheet.height != 24 ||
        sheet.frame_count != FD2_FIELD_CURSOR_FRAME_COUNT) {
        fd2_shape_sheet_close(&sheet);
        return -1;
    }

    for (size_t i = 0; i < sheet.frame_count; i++) {
        if (fd2_shape_sheet_decode_frame(&visuals->cursor_frames[i],
                                         &sheet, i) != 0) {
            fd2_shape_sheet_close(&sheet);
            return -1;
        }
        visuals->cursor_frame_count++;
    }
    fd2_shape_sheet_close(&sheet);
    return 0;
}

static int load_range_luts(fd2_field_visuals *visuals,
                           const fd2_archive *fdother) {
    const uint8_t *data;
    size_t size;
    if (fd2_archive_get(fdother, 3, &data, &size) != 0 || size < 6 ||
        memcmp(data, "LMI1", 4) != 0)
        return -1;

    uint16_t count = rd_u16_le(data + 4);
    if (count != FD2_FIELD_RANGE_LUT_COUNT ||
        6u + (size_t)count * 4u > size)
        return -1;

    for (size_t i = 0; i < count; i++) {
        uint32_t start = rd_u32_le(data + 6u + i * 4u);
        uint32_t end = i + 1u < count
                     ? rd_u32_le(data + 6u + (i + 1u) * 4u)
                     : (uint32_t)size;
        if (start > size || end > size || end < start || end - start != 256u)
            return -1;
        memcpy(visuals->range_luts[i], data + start, 256u);
        visuals->range_lut_count++;
    }
    return 0;
}

int fd2_field_visuals_open(fd2_field_visuals *visuals,
                           const fd2_archive *fdother) {
    if (!visuals || !fdother) return -1;
    memset(visuals, 0, sizeof(*visuals));
    if (load_cursor_frames(visuals, fdother) != 0 ||
        load_range_luts(visuals, fdother) != 0) {
        fd2_field_visuals_close(visuals);
        return -1;
    }
    return 0;
}

void fd2_field_visuals_close(fd2_field_visuals *visuals) {
    if (!visuals) return;
    for (size_t i = 0; i < FD2_FIELD_CURSOR_FRAME_COUNT; i++)
        fd2_image_free(&visuals->cursor_frames[i]);
    memset(visuals, 0, sizeof(*visuals));
}

const fd2_image *fd2_field_cursor_frame(const fd2_field_visuals *visuals,
                                        size_t frame_index) {
    if (!visuals || frame_index >= visuals->cursor_frame_count)
        return NULL;
    return &visuals->cursor_frames[frame_index];
}

const uint8_t *fd2_field_lut_frame(const fd2_field_visuals *visuals,
                                   size_t frame_index) {
    if (!visuals || frame_index >= visuals->range_lut_count)
        return NULL;
    return visuals->range_luts[frame_index];
}

const uint8_t *fd2_field_range_lut(const fd2_field_visuals *visuals,
                                   size_t phase) {
    /* DS:0x1a97，20 个 phase 对 FDOTHER[3] LUT frame 的选择表。
     * field_view_render_tiles @0x3710c 每 3 个 BIOS tick 前进一步。 */
    static const uint8_t sequence[FD2_FIELD_RANGE_PHASE_COUNT] = {
        8, 1, 0, 0, 0, 4, 2, 5, 1, 0,
        0, 0, 4, 2, 5, 1, 0, 0, 0, 4,
    };
    if (!visuals || visuals->range_lut_count != FD2_FIELD_RANGE_LUT_COUNT)
        return NULL;
    return fd2_field_lut_frame(
        visuals, sequence[phase % FD2_FIELD_RANGE_PHASE_COUNT]);
}

void fd2_field_apply_palette_lut(fd2_vga *vga,
                                 int x, int y, int width, int height,
                                 const uint8_t lut[256]) {
    if (!vga || !lut || width <= 0 || height <= 0) return;
    int64_t right64 = (int64_t)x + width;
    int64_t bottom64 = (int64_t)y + height;
    int left = x < 0 ? 0 : x;
    int top = y < 0 ? 0 : y;
    int right = right64 > VGA_W ? VGA_W : (int)right64;
    int bottom = bottom64 > VGA_H ? VGA_H : (int)bottom64;
    for (int row = top; row < bottom; row++) {
        uint8_t *dst = vga->framebuffer + (size_t)row * VGA_STRIDE;
        for (int col = left; col < right; col++)
            dst[col] = lut[dst[col]];
    }
}
