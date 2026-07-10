#ifndef FD2_FONT_H
#define FD2_FONT_H

#include <stddef.h>
#include <stdint.h>

#include "archive.h"
#include "vga.h"

/* 原版 16×16 字模渲染
 *
 * 逆向依据：FUN_000136cc @0x136cc 使用 FDTXT.DAT 的 u16 token；
 * FUN_0004c4c2 @0x4c4c2 从 FDOTHER.DAT[4] 取 token*0x20 的
 * 16 行位图，每行 16 bit，绘制主色与阴影色。
 */
typedef struct {
    const uint8_t *glyphs; /* 指向 FDOTHER.DAT[4] 内部，不拥有 */
    size_t size;
    size_t glyph_count;
} fd2_font;

int  fd2_font_open_fdother(fd2_font *font, const fd2_archive *fdother, size_t entry_idx);
void fd2_font_draw_glyph(fd2_vga *vga, const fd2_font *font,
                         uint16_t glyph_idx, int x, int y,
                         uint8_t fg, uint8_t shadow, int bg);

#endif /* FD2_FONT_H */
