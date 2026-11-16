#ifndef FD2_FONT_H
#define FD2_FONT_H

#include <stddef.h>
#include <stdint.h>

#include "archive.h"
#include "vga.h"

/* 原版 16×16 字模渲染
 *
 * 逆向依据：FUN_000136cc @0x15f84 使用 FDTXT.DAT 的 u16 token；
 * FUN_0004c4c2 @0x4ed7a 从 FDOTHER.DAT[4] 取 token*0x20 的
 * 16 行位图，每行 16 bit，绘制主色与阴影色。
 */
typedef struct {
    const uint8_t *glyphs; /* 指向 FDOTHER.DAT[4] 内部，不拥有 */
    size_t size;
    size_t glyph_count;
} fd2_font;

int  fd2_font_open_fdother(fd2_font *font, const fd2_archive *fdother, size_t entry_idx);
/* bg<0 保留底图；bg>=0 先预填16×16再画字（含0色）。原版背景
 * 参数0表示透明，调用此SDL接口时应传-1。阴影写下左与正下，0色
 * 不是关闭阴影；字形10只执行可选预填。越界目标像素各自裁切。 */
void fd2_font_draw_glyph(fd2_vga *vga, const fd2_font *font,
                         uint16_t glyph_idx, int x, int y,
                         uint8_t fg, uint8_t shadow, int bg);

/* 原4ed7a的有限linear owner入口：offset为相对pixels的字节位置，
 * stride按原LOWWORD；不按320/456逻辑行clip。background为原raw BYTE：
 * 0透明、非0先清16x16；与上方XY的bg=0黑底约定有意分开。
 * 成功0；首次真实源WORD/目的BYTE越owner返回-1，保此前已写前缀。
 * glyph10清底后不读font；其它索引按原DWORD<<5后在font.size内逐行读。
 * 不拥有buffer/font、不present、不输入/RNG/clock。负数SAFE_MINUS另属H，
 * 不能用此入口执行原font owner外的-3字模或伪造其像素。 */
int fd2_font_draw_glyph_linear(uint8_t *pixels,size_t size,const fd2_font *font,
    uint32_t glyph_idx,int64_t offset,uint32_t stride,
    uint8_t fg,uint8_t shadow,uint8_t background);

#endif /* FD2_FONT_H */
