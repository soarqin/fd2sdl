/* 炎龙骑士团 2 SDL3 重写 - FDTXT 字模渲染
 * 逆向依据：text_dialog_render_tokens @0x15f84（原登记 FUN_000136cc）与
 * font_glyph_blit_16x16 @0x4ed7a（原登记 FUN_0004c4c2）。
 */

#include "font.h"

#include <string.h>

int fd2_font_open_fdother(fd2_font *font, const fd2_archive *fdother, size_t entry_idx) {
    memset(font, 0, sizeof(*font));
    if (!fdother) return -1;

    const uint8_t *data;
    size_t size;
    if (fd2_archive_get(fdother, entry_idx, &data, &size) != 0) return -1;
    if (size < 0x20 || (size % 0x20) != 0) return -1;

    font->glyphs = data;
    font->size = size;
    font->glyph_count = size / 0x20;
    return 0;
}

static void glyph_put_pixel(fd2_vga *vga, int64_t x, int64_t y, uint8_t color) {
    if (x < 0 || x >= VGA_W || y < 0 || y >= VGA_H) return;
    vga->framebuffer[(size_t)y * VGA_STRIDE + (size_t)x] = color;
}

void fd2_font_draw_glyph(fd2_vga *vga, const fd2_font *font,
                         uint16_t glyph_idx, int x, int y,
                         uint8_t fg, uint8_t shadow, int bg) {
    if (!vga || !font || !font->glyphs || glyph_idx >= font->glyph_count)
        return;

    /* 指令 VA 0x4edc0：原版低 byte 为 0 时不预清。SDL 保留既有 bg<0
     * 透明约定；调用者将原版 background_arg=0 显式映射为 bg=-1。 */
    if (bg >= 0) {
        for (int row = 0; row < 16; row++)
            for (int col = 0; col < 16; col++)
                glyph_put_pixel(vga, (int64_t)x + col, (int64_t)y + row,
                                (uint8_t)bg);
    }
    /* 指令 VA 0x4edeb：字形 10 先完成可选预清，再跳过前景与阴影。 */
    if (glyph_idx == 10u) return;

    const uint8_t *glyph = font->glyphs + (size_t)glyph_idx * 0x20;
    for (int row = 0; row < 16; row++) {
        uint16_t bits = ((uint16_t)glyph[row * 2] << 8) | glyph[row * 2 + 1];
        for (int col = 0; col < 16; col++) {
            if (bits & 0x8000u) {
                int64_t dx = (int64_t)x + col;
                int64_t dy = (int64_t)y + row;
                /* 指令 VA 0x4ee1e..0x4ee23：阴影写下左／正下，0 色也照写。
                 * 三个目标各自裁切，前景出界时阴影仍可能可见。 */
                glyph_put_pixel(vga, dx, dy, fg);
                glyph_put_pixel(vga, dx - 1, dy + 1, shadow);
                glyph_put_pixel(vga, dx, dy + 1, shadow);
            }
            bits <<= 1;
        }
    }
}
