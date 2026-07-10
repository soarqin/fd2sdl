#include "shape.h"

#include <stdlib.h>
#include <string.h>

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int fd2_shape_sheet_open_mem(fd2_shape_sheet *sheet,
                              const uint8_t *data, size_t size) {
    memset(sheet, 0, sizeof(*sheet));
    if (!data || size < 10) return -1;
    if (size > UINT32_MAX) return -1; /* 原格式偏移表为 u32 */

    int width = rd_u16_le(data);
    int height = rd_u16_le(data + 2);
    uint16_t frame_count_u16 = rd_u16_le(data + 4);
    size_t frame_count = frame_count_u16;

    if (width <= 0 || height <= 0 || width > 4096 || height > 4096)
        return -1;
    if (frame_count == 0) return -1;
    if (frame_count > (SIZE_MAX - 6) / 4) return -1;

    size_t table_end = 6 + frame_count * 4;
    if (table_end > size) return -1;

    uint32_t *offsets = malloc(frame_count * sizeof(*offsets));
    if (!offsets) return -1;

    for (size_t i = 0; i < frame_count; i++) {
        uint32_t off = rd_u32_le(data + 6 + i * 4);
        if (off < table_end || off >= size) {
            free(offsets);
            return -1;
        }
        if (i > 0 && off <= offsets[i - 1]) {
            free(offsets);
            return -1;
        }
        offsets[i] = off;
    }

    sheet->width = width;
    sheet->height = height;
    sheet->frame_count = frame_count;
    sheet->data = data;
    sheet->size = size;
    sheet->offsets = offsets;
    return 0;
}

int fd2_shape_sheet_open_entry(fd2_shape_sheet *sheet,
                                const fd2_archive *ar, size_t idx) {
    const uint8_t *data;
    size_t size;
    if (fd2_archive_get(ar, idx, &data, &size) != 0) return -1;
    return fd2_shape_sheet_open_mem(sheet, data, size);
}

int fd2_shape_sheet_get_frame(const fd2_shape_sheet *sheet, size_t frame_idx,
                               const uint8_t **out_ptr, size_t *out_len) {
    if (!sheet || !sheet->data || !sheet->offsets) return -1;
    if (frame_idx >= sheet->frame_count) return -1;

    uint32_t start = sheet->offsets[frame_idx];
    uint32_t end = (frame_idx + 1 < sheet->frame_count)
                 ? sheet->offsets[frame_idx + 1]
                 : (uint32_t)sheet->size;
    if (start >= sheet->size || end > sheet->size || end <= start) return -1;

    if (out_ptr) *out_ptr = sheet->data + start;
    if (out_len) *out_len = (size_t)(end - start);
    return 0;
}

int fd2_shape_sheet_decode_frame(fd2_image *img,
                                  const fd2_shape_sheet *sheet,
                                  size_t frame_idx) {
    memset(img, 0, sizeof(*img));

    const uint8_t *src;
    size_t src_len;
    if (fd2_shape_sheet_get_frame(sheet, frame_idx, &src, &src_len) != 0)
        return -1;

    if (sheet->width <= 0 || sheet->height <= 0) return -1;
    size_t pixel_count = (size_t)sheet->width * (size_t)sheet->height;
    if (sheet->width != 0 && pixel_count / (size_t)sheet->width != (size_t)sheet->height)
        return -1;

    img->width = sheet->width;
    img->height = sheet->height;
    img->pixels = malloc(pixel_count);
    if (!img->pixels) {
        memset(img, 0, sizeof(*img));
        return -1;
    }
    memset(img->pixels, 0, pixel_count); /* SKIP 指令保留透明色 0 */

    if (fd2_image_decode_rle_pixels(img->pixels, img->width, img->height,
                                    src, src_len) != 0) {
        fd2_image_free(img);
        return -1;
    }
    return 0;
}

void fd2_shape_sheet_close(fd2_shape_sheet *sheet) {
    free(sheet->offsets);
    memset(sheet, 0, sizeof(*sheet));
}
