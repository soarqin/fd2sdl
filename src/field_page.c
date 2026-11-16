/* field_view_render @0x11cac（原sub_11CAC）、field_view_render_tiles
 * @0x11eee（原sub_11EEE）、blit_image @0x4e22a（原sub_4E22A）及
 * shape_blit_palette_lut_24x24 @0x4e016／透明变体@0x4e0a2的工作页。
 * 原53A49分配0x25680字节；32904为正常视窗原点，30168为actor原点。 */
#include "field_page.h"
#include "field_page_private.h"

#include <stdlib.h>
#include <string.h>

fd2_field_page *fd2_field_page_create(void) {
    return calloc(1, sizeof(fd2_field_page));
}

void fd2_field_page_destroy(fd2_field_page *page) {
    if(!page)return;
    free(page->background.source);free(page->background.work);free(page);
}

fd2_field_page *fd2_field_page_clone(const fd2_field_page *page) {
    if(!page)return NULL;
    fd2_field_page *copy=malloc(sizeof(*copy));
    if(!copy)return NULL;
    memcpy(copy,page,sizeof(*copy));
    copy->background.source=copy->background.work=NULL;
    if(page->background.source) {
        copy->background.source=malloc(page->background.source_size);
        if(!copy->background.source)goto fail;
        memcpy(copy->background.source,page->background.source,page->background.source_size);
    }
    if(page->background.work) {
        copy->background.work=malloc(page->background.work_size);
        if(!copy->background.work)goto fail;
        memcpy(copy->background.work,page->background.work,page->background.work_size);
    }
    return copy;
fail:
    fd2_field_page_destroy(copy);return NULL;
}

int fd2_field_page_copy_pixels(fd2_field_page *destination,const fd2_field_page *source) {
    if(!destination || !source)return -1;
    if(destination!=source)memcpy(destination->pixels,source->pixels,FD2_FIELD_PAGE_SIZE);
    return 0;
}

static int rle_visit(fd2_field_page *page, int x, int y, int width, int height,
    const uint8_t *src, size_t src_len, const uint8_t lut[256],int map_skip,
    int linear, int64_t offset) {
    if (!src || width <= 0 || height <= 0 || width > 4096 || height > 4096)
        return -1;
    size_t cursor = 0;
    for (int row = 0; row < height; row++) {
        int col = 0;
        while (col < width) {
            if (cursor >= src_len) return -1;
            uint8_t control = src[cursor++];
            int count = (control & 63) + 1;
            unsigned mode = control & 192u;
            int span = mode == 64u ? count * 2 : count;
            if (span > width - col) return -1;
            if (mode == 128u && (size_t)count > src_len - cursor) return -1;
            if (mode < 128u && cursor >= src_len) return -1;
            if(mode==192u && page && map_skip) {
                int64_t dy=(int64_t)y+row;
                for(int i=0;i<count;i++) {
                    int64_t dx=(int64_t)x+col+i;
                    if(dx>=0 && dx<FD2_FIELD_PAGE_STRIDE && dy>=0 && dy<FD2_FIELD_PAGE_HEIGHT) {
                        uint8_t *pixel=&page->pixels[(size_t)dy*FD2_FIELD_PAGE_STRIDE+(size_t)dx];
                        *pixel=lut[*pixel];
                    }
                }
            }
            if (mode != 192u && page) {
                int64_t dy = (int64_t)y + row;
                for (int i = 0; i < count; i++) {
                    int column = col + (mode == 64u ? i * 2 + 1 : i);
                    size_t target;
                    if (linear) {
                        int64_t delta = (int64_t)row * FD2_FIELD_PAGE_STRIDE + column;
                        /* 先界定真实owner再相加，INT64极值也不溢出；SKIP不访问目标。 */
                        if (offset < -delta || offset > FD2_FIELD_PAGE_SIZE - 1 - delta)
                            return -1;
                        target = (size_t)(offset + delta);
                    } else {
                        int64_t dx = (int64_t)x + column;
                        if (dx < 0 || dx >= FD2_FIELD_PAGE_STRIDE ||
                            dy < 0 || dy >= FD2_FIELD_PAGE_HEIGHT) continue;
                        target = (size_t)dy * FD2_FIELD_PAGE_STRIDE + (size_t)dx;
                    }
                    uint8_t color = src[cursor + (mode == 128u ? (size_t)i : 0)];
                    page->pixels[target] = lut ? lut[color] : color;
                }
            }
            if (mode == 128u) cursor += (size_t)count;
            else if (mode < 128u) cursor++;
            col += span;
        }
    }
    return cursor == src_len ? 0 : -1;
}

int fd2_field_page_validate_rle(int width, int height,
    const uint8_t *src, size_t src_len) {
    return rle_visit(NULL, 0, 0, width, height, src, src_len, NULL,0,0,0);
}

int fd2_field_page_blit_rle(fd2_field_page *page, int x, int y,
    int width, int height, const uint8_t *src, size_t src_len,
    const uint8_t lut[256]) {
    if (!page || fd2_field_page_validate_rle(width, height, src, src_len) != 0)
        return -1;
    return rle_visit(page, x, y, width, height, src, src_len, lut,0,0,0);
}

/* sub_127E0 @0x127e0通过4e22a/4e1a6按456线性地址画actor；
 * logical x跨行仍属同App，首个真实opaque写越owner时保留已达前缀。 */
int fd2_field_page_blit_rle_linear(fd2_field_page *page, int64_t offset,
    int width, int height, const uint8_t *src, size_t src_len,
    const uint8_t lut[256]) {
    if (!page || fd2_field_page_validate_rle(width,height,src,src_len)!=0)
        return -1;
    return rle_visit(page,0,0,width,height,src,src_len,lut,0,1,offset);
}

int fd2_field_page_blit_base_lut_24(fd2_field_page *page,int x,int y,
    const uint8_t *src,size_t src_len,const uint8_t lut[256]) {
    if(!page || !lut || fd2_field_page_validate_rle(24,24,src,src_len)!=0)return -1;
    return rle_visit(page,x,y,24,24,src,src_len,lut,1,0,0);
}

int fd2_field_page_choice_snapshot_write(fd2_field_page *page,
    const uint8_t *pixels,size_t size) {
    if(!page || !pixels || size<64000u)return -1;
    return fd2_field_page_write(page,68,68,320,200,pixels,size,320);
}

int fd2_field_page_choice_bottom_write(fd2_field_page *page,
    const uint8_t *pixels,size_t size) {
    if(!page || !pixels || size<64000u)return -1;
    return fd2_field_page_write(page,73,180,310,86,pixels+35845u,size-35845u,320);
}

int fd2_field_page_scaled_view_write(fd2_field_page *page,
    const uint8_t *pixels,size_t size) {
    if(!page || !pixels || size<64000u)return -1;
    return fd2_field_page_write(page,72,72,312,192,pixels+1284u,size-1284u,320);
}

static int rectangle_valid(int x, int y, int width, int height,
    size_t size, size_t stride) {
    if (x < 0 || y < 0 || width <= 0 || height <= 0 ||
        width > FD2_FIELD_PAGE_STRIDE - x ||
        height > FD2_FIELD_PAGE_HEIGHT - y || stride < (size_t)width)
        return 0;
    return (size_t)(height - 1) <= (SIZE_MAX - (size_t)width) / stride &&
        stride * (size_t)(height - 1) + (size_t)width <= size;
}

static int buffer_separate(const fd2_field_page *page,
    const void *pixels, size_t size) {
    uintptr_t a = (uintptr_t)page, b = (uintptr_t)pixels;
    if (size > UINTPTR_MAX - b) return 0;
    return b >= a + sizeof(*page) || b + size <= a;
}

int fd2_field_page_read(const fd2_field_page *page, int x, int y,
    int width, int height, uint8_t *pixels, size_t size, size_t stride) {
    if (!page || !pixels || !rectangle_valid(x,y,width,height,size,stride) ||
        !buffer_separate(page, pixels, size)) return -1;
    for (int row = 0; row < height; row++)
        memcpy(pixels + (size_t)row * stride,
            page->pixels + (size_t)(y + row) * FD2_FIELD_PAGE_STRIDE + (size_t)x,
            (size_t)width);
    return 0;
}

int fd2_field_page_write(fd2_field_page *page, int x, int y,
    int width, int height, const uint8_t *pixels, size_t size, size_t stride) {
    if (!page || !pixels || !rectangle_valid(x,y,width,height,size,stride) ||
        !buffer_separate(page, pixels, size)) return -1;
    for (int row = 0; row < height; row++)
        memcpy(page->pixels + (size_t)(y + row) * FD2_FIELD_PAGE_STRIDE + (size_t)x,
            pixels + (size_t)row * stride, (size_t)width);
    return 0;
}

int fd2_field_page_project(const fd2_field_page *page,
    int source_x, int source_y, uint8_t *pixels, size_t size) {
    if (size < 320u * 200u) return -1;
    return fd2_field_page_read(page, source_x, source_y,
        FD2_FIELD_PAGE_VIEW_W, FD2_FIELD_PAGE_VIEW_H,
        pixels ? pixels + 4u * 320u + 4u : NULL,
        size - (4u * 320u + 4u), 320u);
}

int fd2_field_page_phase_prefix_read(const fd2_field_page *page,
        uint8_t *pixels, size_t size) {
    if(!page || !pixels || size!=64000u || !buffer_separate(page,pixels,size))
        return -1;
    memcpy(pixels,page->pixels,size);return 0;
}

int fd2_field_page_phase_prefix_write(fd2_field_page *page,
        const uint8_t *pixels, size_t size) {
    if(!page || !pixels || size!=64000u || !buffer_separate(page,pixels,size))
        return -1;
    memcpy(page->pixels,pixels,size);return 0;
}
