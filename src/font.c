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

/* font_glyph_blit_16x16 @0x4ed7a（原FUN_0004c4c2）：唯一bit/清底/写序。
 * XY policy保现宿主逐点裁切；linear policy只检查真实owner，不按逻辑行clip。
 * target仅本次调用的借用参数，不拥有像素、字模或跨帧状态。 */
typedef struct {
    fd2_vga *xy;
    uint8_t *pixels;
    size_t size;
    int64_t offset;
    uint16_t stride; /* 4ed99保存AX，4edb9零扩展BP。 */
    int x,y;
} glyph_target;

static int glyph_put_pixel(const glyph_target *target,int col,int row,uint8_t color) {
    if(target->xy) {
        int64_t x=(int64_t)target->x+col,y=(int64_t)target->y+row;
        if(x>=0 && x<VGA_W && y>=0 && y<VGA_H)
            target->xy->framebuffer[(size_t)y*VGA_STRIDE+(size_t)x]=color;
        return 0;
    }
    int64_t delta=(int64_t)row*target->stride+col;
    if((delta>0 && target->offset>INT64_MAX-delta) ||
       (delta<0 && target->offset<INT64_MIN-delta))return -1;
    int64_t at=target->offset+delta;
    if(at<0 || (uint64_t)at>=(uint64_t)target->size)return -1;
    target->pixels[(size_t)at]=color;return 0;
}

static int glyph_draw(const glyph_target *target,const fd2_font *font,
        uint32_t glyph_idx,uint8_t fg,uint8_t shadow,int clear,uint8_t bg) {
    if(clear)for(int row=0;row<16;row++)for(int col=0;col<16;col++)
        if(glyph_put_pixel(target,col,row,bg)!=0)return -1;
    /* 4edeb：只有原DWORD等于10才跳过；发生在可选清底之后。 */
    if(glyph_idx==10u)return 0;
    uint32_t glyph_offset=glyph_idx<<5; /* 4edf0：原DWORD低32位SHL。 */
    for(int row=0;row<16;row++) {
        uint64_t source=(uint64_t)glyph_offset+2u*(unsigned)row;
        /* linear源owner也在第一次真实WORD读时判断，保已写清底／前行前缀。
         * XY wrapper仍沿原公开glyph_count/必需font前置，保持旧裁切合同。 */
        if(!target->xy && (!font || !font->glyphs || source>font->size ||
                (uint64_t)font->size-source<2u))return -1;
        const uint8_t *word=font->glyphs+(size_t)source;
        uint16_t bits=(uint16_t)(((uint16_t)word[0]<<8)|word[1]);
        for(int col=0;col<16;col++) {
            if(bits&0x8000u) {
                /* 4ee1e/20/23：fg、下左shadow、正下shadow；0色也照写。 */
                if(glyph_put_pixel(target,col,row,fg)!=0 ||
                   glyph_put_pixel(target,col-1,row+1,shadow)!=0 ||
                   glyph_put_pixel(target,col,row+1,shadow)!=0)return -1;
            }
            bits<<=1;
        }
    }
    return 0;
}

void fd2_font_draw_glyph(fd2_vga *vga,const fd2_font *font,
        uint16_t glyph_idx,int x,int y,uint8_t fg,uint8_t shadow,int bg) {
    if(!vga || !font || !font->glyphs || glyph_idx>=font->glyph_count)return;
    glyph_target target={.xy=vga,.x=x,.y=y};
    (void)glyph_draw(&target,font,glyph_idx,fg,shadow,bg>=0,(uint8_t)bg);
}

int fd2_font_draw_glyph_linear(uint8_t *pixels,size_t size,const fd2_font *font,
        uint32_t glyph_idx,int64_t offset,uint32_t stride,
        uint8_t fg,uint8_t shadow,uint8_t background) {
    if(!pixels || !size)return -1;
    glyph_target target={.pixels=pixels,.size=size,.offset=offset,.stride=(uint16_t)stride};
    return glyph_draw(&target,font,glyph_idx,fg,shadow,background!=0u,background);
}
