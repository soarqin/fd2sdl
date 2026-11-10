/* 背景原义：sub_10652 @0x10652（原登记同名）重建53AFF/53B03；
 * field_view_render_tiles @0x11eee铺底；sub_4EEE0 @0x4eee0波纹逐行；
 * sub_24D22 @0x24d22滚动。source dispatch与原byte仅编辑field_background.json。
 * 临时canvas首次SKIP未定义字节最小置0，后续page透明历史绝不清0。
 * 仅投影CPU叶终态，不宣称复制/行轮转的内部逐写轨迹与BIOS墙钟等价。 */
#include "field_background.h"
#include "field_page_private.h"
#include "image.h"

#include <stdlib.h>
#include <string.h>

#include "static_data/field_background.inc"

enum { BACKGROUND_NONE, BACKGROUND_WAVE, BACKGROUND_PARALLAX, BACKGROUND_SCROLL };

static uint16_t word_le(const uint8_t *p) {
    return (uint16_t)p[0]|(uint16_t)((uint16_t)p[1]<<8);
}

static void background_close(fd2_field_background_state *state) {
    free(state->source);free(state->work);state->source=NULL;state->work=NULL;
}

static int copy_image(fd2_field_background_state *state,
    const fd2_archive *archive, size_t entry, int y, int height) {
    fd2_image image={0};
    if(fd2_image_decode_entry(&image,archive,entry)!=0)return -1;
    if(image.width!=state->width || image.height!=height || y<0 ||
        height>state->height-y) {fd2_image_free(&image);return -1;}
    memcpy(state->source+(size_t)y*(size_t)state->width,image.pixels,
        (size_t)image.width*(size_t)image.height);
    fd2_image_free(&image);return 0;
}

static int scroll_rows(fd2_field_background_state *state) {
    unsigned amount=state->scroll_speed;
    if(state->kind!=BACKGROUND_SCROLL || !state->source ||
        state->width!=312 || state->height!=192 || amount>192u)return -1;
    if(!amount || amount==192u)return 0;
    /* 三次行反转与原尾段→头部轮转终态一致，无新持久scratch或热owner。 */
    unsigned first[3]={0,0,amount},last[3]={191,amount-1u,191};
    uint8_t row[312];
    for(unsigned part=0;part<3;part++)
        while(first[part]<last[part]) {
            uint8_t *a=state->source+(size_t)first[part]++*312u;
            uint8_t *b=state->source+(size_t)last[part]--*312u;
            memcpy(row,a,312);memcpy(a,b,312);memcpy(b,row,312);
        }
    return 0;
}

int fd2_field_background_rebuild(fd2_field_page *page,
    const fd2_archive *fdother, size_t stage) {
    if(!page || !fdother || stage>=sizeof(k_field_background_config)/
            sizeof(k_field_background_config[0]))return -1;
    const uint16_t *config=k_field_background_config[stage];
    fd2_field_background_state candidate=page->background;
    candidate.source=candidate.work=NULL;
    candidate.source_size=candidate.work_size=0;
    candidate.kind=(uint8_t)config[0];candidate.width=config[2];candidate.height=config[3];
    if(!candidate.initialized) {
        candidate.scroll_speed=k_field_background_scroll_initial[0];
        candidate.initialized=1;
    }
    if(candidate.kind!=BACKGROUND_NONE) {
        candidate.source_size=(size_t)candidate.width*(size_t)candidate.height;
        candidate.source=calloc(1,candidate.source_size);
        if(!candidate.source)goto fail;
        if(candidate.kind==BACKGROUND_WAVE) {
            const uint8_t *raw;size_t length;
            if(fd2_archive_get(fdother,config[1],&raw,&length)!=0 ||
                length!=64004u || word_le(raw)!=320 || word_le(raw+2)!=200)goto fail;
            memcpy(candidate.source,raw+4,64000u);
            candidate.work_size=64000u;candidate.work=calloc(1,candidate.work_size);
            if(!candidate.work)goto fail;
        } else if(candidate.kind==BACKGROUND_PARALLAX) {
            if(copy_image(&candidate,fdother,config[1],0,candidate.height/2)!=0 ||
                copy_image(&candidate,fdother,(size_t)config[1]+1u,
                    candidate.height/2,candidate.height/2)!=0)goto fail;
        } else if(candidate.kind==BACKGROUND_SCROLL) {
            if(copy_image(&candidate,fdother,config[1],0,candidate.height)!=0 ||
                scroll_rows(&candidate)!=0)goto fail;
        } else goto fail;
    }
    background_close(&page->background);page->background=candidate;return 0;
fail:
    background_close(&candidate);return -1;
}

int fd2_field_background_scroll(fd2_field_page *page, uint8_t amount) {
    if(!page)return -1;
    if(amount) {page->background.scroll_speed=amount;page->background.initialized=1;return 0;}
    return scroll_rows(&page->background);
}

int fd2_field_background_draw_from(fd2_field_page *owner,fd2_field_page *page,
    int dest_x, int dest_y, int camera_x, int camera_y,
    int parallax_x, int parallax_y, uint64_t now_ms, int advance) {
    if(!owner || !page || (advance!=0 && advance!=1))return -1;
    fd2_field_background_state *state=&owner->background;
    if(state->kind==BACKGROUND_NONE)return 0;
    if(!state->source || dest_x<0 || dest_y<0 ||
        dest_x>FD2_FIELD_PAGE_STRIDE-312 || dest_y>FD2_FIELD_PAGE_HEIGHT-192)
        return -1;
    uint32_t low=(uint32_t)(now_ms/55u)&0xffffu;
    /* 11eee的539F8与terrain锚同为movsx WORD[46c]后的DWORD位型。 */
    uint32_t tick=low<0x8000u?low:low|0xffff0000u;
    if(state->kind==BACKGROUND_WAVE) {
        if(!state->work || state->phase>=16u)return -1;
        if(advance && tick!=state->last_tick) {
            for(size_t row=0;row<192u;row++)
                memcpy(state->work+row*320u,
                    state->source+row*320u+k_field_background_wave[(row+state->phase)&15u],312u);
            state->last_tick=tick;state->phase=(uint8_t)((state->phase+1u)&15u);
        }
        return fd2_field_page_write(page,dest_x,dest_y,312,192,
            state->work,state->work_size,320);
    }
    if(state->kind==BACKGROUND_SCROLL) {
        if(advance && tick!=state->last_tick) {
            if(scroll_rows(state)!=0)return -1;
            state->last_tick=tick;
        }
        return fd2_field_page_write(page,dest_x,dest_y,312,192,
            state->source,state->source_size,312);
    }
    if(state->kind==BACKGROUND_PARALLAX) {
        int64_t x=(int64_t)camera_x*3+parallax_x/2;
        int64_t y=(int64_t)camera_y*2+parallax_y/3;
        if(x<0 || y<0 || x>state->width-312 || y>state->height-192)return -1;
        size_t offset=(size_t)y*(size_t)state->width+(size_t)x;
        return fd2_field_page_write(page,dest_x,dest_y,312,192,
            state->source+offset,state->source_size-offset,(size_t)state->width);
    }
    return -1;
}

int fd2_field_background_draw(fd2_field_page *page,
    int dest_x,int dest_y,int camera_x,int camera_y,
    int parallax_x,int parallax_y,uint64_t now_ms,int advance) {
    return fd2_field_background_draw_from(page,page,dest_x,dest_y,camera_x,camera_y,
        parallax_x,parallax_y,now_ms,advance);
}
