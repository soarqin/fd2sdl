/* 炎龙骑士团 2 SDL3 重写 - FDTXT 字模渲染
 * 逆向依据：FUN_000136cc @0x136cc 与 FUN_0004c4c2 @0x4c4c2。
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

void fd2_font_draw_glyph(fd2_vga *vga, const fd2_font *font,
                         uint16_t glyph_idx, int x, int y,
                         uint8_t fg, uint8_t shadow, int bg) {
    if (!vga || !font || !font->glyphs || glyph_idx >= font->glyph_count)
        return;

    const uint8_t *glyph = font->glyphs + (size_t)glyph_idx * 0x20;
    for (int row = 0; row < 16; row++) {
        int dy = y + row;
        if (dy < 0 || dy >= VGA_H) continue;

        uint16_t bits = ((uint16_t)glyph[row * 2] << 8) | glyph[row * 2 + 1];
        for (int col = 0; col < 16; col++) {
            int dx = x + col;
            if (dx < 0 || dx >= VGA_W) {
                bits <<= 1;
                continue;
            }
            if (bits & 0x8000u) {
                vga->framebuffer[(size_t)dy * VGA_STRIDE + (size_t)dx] = fg;
                if (shadow) {
                    if (dx + 1 < VGA_W)
                        vga->framebuffer[(size_t)dy * VGA_STRIDE + (size_t)(dx + 1)] = shadow;
                    if (dy + 1 < VGA_H)
                        vga->framebuffer[(size_t)(dy + 1) * VGA_STRIDE + (size_t)dx] = shadow;
                }
            } else if (bg >= 0) {
                vga->framebuffer[(size_t)dy * VGA_STRIDE + (size_t)dx] = (uint8_t)bg;
            }
            bits <<= 1;
        }
    }
}
