/* 有限Scene绘制叶：sub_24336 @0x24336、sub_2189A @0x2189a、
 * sub_24B4D @0x24b4d、sub_31529 @0x31529、sub_31C49 @0x31c49。
 * 依据S5完整Domain/CFG/strict LE读集；2eb9f/311e5、168b6、4ed7a
 * 分别复用FIGANI、同一详情边框和现字模实现。此文件不持PC、不等待/切曲/
 * 轮询输入，也不执行整段终章或复制Game/SAV/业务RNG owner。 */
#include "scene_cinematic.h"
#include "bios_clock.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "figani.h"
#include "battle_animation.h"
#include "field_background.h"
#include "field_info.h"
#include "field_visual.h"
#include "image.h"
#include "portrait.h"

typedef struct { fd2_image image; uint8_t *opaque; } cinematic_image;
typedef struct { fd2_figani_part part; cinematic_image drawing; } cinematic_part;
typedef struct { fd2_figani_clip clip; cinematic_part *parts; } cinematic_clip;
typedef struct { uint8_t *data; size_t size; } cinematic_page;

struct fd2_scene_cinematic {
    fd2_vga *vga;
    fd2_field_game *game;
    const fd2_archive *fdother;
    cinematic_page pages[FD2_CINEMATIC_PAGE_COUNT];
    cinematic_clip sheets[FD2_CINEMATIC_SHEET_COUNT];
    uint8_t *app_transfer;
    fd2_field_page *circle_base;
    size_t circle_actor;
    int circle_x,circle_y;
    uint8_t saved_palette[768];
    unsigned palette_saved;
    fd2_archive tai;
    fd2_text_entry credits_text;
    cinematic_image credits_terrain;
    cinematic_clip idle, intro;
    fd2_image portrait[2];
    fd2_vga *glyph;
    size_t outer_index, raw_actor;
    uint8_t idle_ticks, idle_part, blink_countdown;
    unsigned credits_assets, actor_ready, idle_ready, blink_ready, card_skip;
    fd2_pcm_bank audio_bank;
    unsigned audio_ready;
    fd2_battle_montage *montage;
};

static void image_close(cinematic_image *image) {
    fd2_image_free(&image->image);free(image->opaque);image->opaque=NULL;
}

static int image_open(cinematic_image *out,const uint8_t *data,size_t size) {
    cinematic_image candidate={0};
    if(fd2_image_decode_buf(&candidate.image,data,size)!=0)return -1;
    size_t count=(size_t)candidate.image.width*(size_t)candidate.image.height;
    candidate.opaque=malloc(count);
    if(!candidate.opaque)goto fail;
    memset(candidate.opaque,255,count);
    if(fd2_image_decode_rle_pixels(candidate.opaque,candidate.image.width,
        candidate.image.height,data+4,size-4)!=0)goto fail;
    for(size_t i=0;i<count;i++)
        candidate.opaque[i]=(uint8_t)(candidate.opaque[i]==candidate.image.pixels[i]);
    *out=candidate;return 0;
fail:
    image_close(&candidate);return -1;
}

static int image_entry(cinematic_image *out,const fd2_archive *archive,size_t entry) {
    const uint8_t *data;size_t size;
    return fd2_archive_get(archive,entry,&data,&size)==0?image_open(out,data,size):-1;
}

static void clip_close(cinematic_clip *clip) {
    if(clip->parts)for(size_t i=0;i<clip->clip.part_count;i++)
        image_close(&clip->parts[i].drawing);
    free(clip->parts);memset(clip,0,sizeof(*clip));
}

static int clip_open(cinematic_clip *out,const fd2_archive *archive,size_t entry) {
    cinematic_clip candidate={0};
    if(fd2_figani_open_entry(&candidate.clip,archive,entry)!=0 || candidate.clip.empty)
        return -1;
    candidate.parts=calloc(candidate.clip.part_count,sizeof(*candidate.parts));
    if(!candidate.parts)return -1;
    for(size_t i=0;i<candidate.clip.part_count;i++) {
        cinematic_part *part=&candidate.parts[i];
        if(fd2_figani_get_part(&candidate.clip,i,&part->part)!=0 ||
            image_open(&part->drawing,part->part.image_data,part->part.image_size)!=0)
            {clip_close(&candidate);return -1;}
    }
    *out=candidate;return 0;
}

static int slot_valid(int slot) {
    return slot>=0 && slot<FD2_CINEMATIC_PAGE_COUNT;
}

static int page_get(fd2_scene_cinematic *scope,int slot,uint8_t **data,size_t *size) {
    if(!scope || !data || !size || !slot_valid(slot))return -1;
    if(slot==FD2_CINEMATIC_VGA) {
        *data=scope->vga->framebuffer;*size=64000;return 0;
    }
    if(slot==FD2_CINEMATIC_APP_PREFIX) {
        if(!scope->vga->field_page)return -1;
        if(!scope->app_transfer)scope->app_transfer=malloc(64000);
        if(!scope->app_transfer || fd2_field_page_phase_prefix_read(scope->vga->field_page,
            scope->app_transfer,64000)!=0)return -1;
        *data=scope->app_transfer;*size=64000;return 0;
    }
    if(!scope->pages[slot].data)return -1;
    *data=scope->pages[slot].data;*size=scope->pages[slot].size;return 0;
}

static int page_commit(fd2_scene_cinematic *scope,int slot) {
    return slot==FD2_CINEMATIC_APP_PREFIX?
        fd2_field_page_phase_prefix_write(scope->vga->field_page,scope->app_transfer,64000):0;
}

static int rectangle(size_t size,size_t offset,size_t width,size_t height,size_t stride) {
    return width && height && stride>=width && stride<=640u && offset<=size &&
        height-1u<=(SIZE_MAX-width)/stride &&
        (height-1u)*stride+width<=size-offset;
}

int fd2_scene_cinematic_open(fd2_scene_cinematic **out,fd2_vga *vga,
        fd2_field_game *game,const fd2_archive *fdother) {
    if(!out || *out || !vga || !fdother)return -1;
    fd2_scene_cinematic *scope=calloc(1,sizeof(*scope));
    if(!scope)return -1;
    scope->vga=vga;scope->game=game;scope->fdother=fdother;*out=scope;return 0;
}

void fd2_scene_cinematic_close(fd2_scene_cinematic **owner) {
    if(!owner || !*owner)return;
    fd2_scene_cinematic *scope=*owner;
    fd2_battle_montage_close(&scope->montage);
    (void)fd2_scene_cinematic_credits_actor_close(scope);
    fd2_text_entry_close(&scope->credits_text);image_close(&scope->credits_terrain);
    fd2_archive_close(&scope->tai);
    fd2_pcm_bank_close(&scope->audio_bank);
    for(size_t i=0;i<FD2_CINEMATIC_SHEET_COUNT;i++)clip_close(&scope->sheets[i]);
    for(size_t i=1;i<=FD2_CINEMATIC_ROLL;i++)free(scope->pages[i].data);
    fd2_field_page_destroy(scope->circle_base);free(scope->app_transfer);
    free(scope->glyph);free(scope);*owner=NULL;
}

int fd2_scene_cinematic_montage_open(fd2_scene_cinematic *scope,int first,int second,int scene) {
    if(!scope || !scope->game || (scope->game->stage!=26u && scope->game->stage!=30u) ||
        scope->game->units.count<2u || first<0 || first>127 || second<0 || second>127 ||
        scene<1 || scene>63)return -1;
    return fd2_battle_montage_open(&scope->montage,scope->vga,&scope->game->figani_archive,
        scope->game->units.items,(uint8_t)first,(uint8_t)second,(uint8_t)scene);
}
int fd2_scene_cinematic_montage_close(fd2_scene_cinematic *scope) {
    if(!scope || !scope->montage)return -1;
    fd2_battle_montage_close(&scope->montage);return 0;
}
int fd2_scene_cinematic_montage_intro(fd2_scene_cinematic *scope,unsigned phase) {
    return scope?fd2_battle_montage_intro(scope->montage,phase):-1;
}
int fd2_scene_cinematic_montage_intro_finish(fd2_scene_cinematic *scope) {
    return scope?fd2_battle_montage_intro_finish(scope->montage):-1;
}
int fd2_scene_cinematic_montage_half(fd2_scene_cinematic *scope,unsigned half) {
    return scope?fd2_battle_montage_half(scope->montage,half):-1;
}
int fd2_scene_cinematic_montage_current_half(const fd2_scene_cinematic *scope,unsigned *half) {
    return scope?fd2_battle_montage_current_half(scope->montage,half):-1;
}
int fd2_scene_cinematic_montage_part_count(const fd2_scene_cinematic *scope,size_t *count) {
    return scope?fd2_battle_montage_part_count(scope->montage,count):-1;
}
int fd2_scene_cinematic_montage_part_info(const fd2_scene_cinematic *scope,size_t part,
        uint8_t *marker,uint8_t *ticks,int *last_marker) {
    return scope?fd2_battle_montage_part_info(scope->montage,part,marker,ticks,last_marker):-1;
}
int fd2_scene_cinematic_montage_rng(fd2_scene_cinematic *scope,unsigned threshold,int *yes) {
    unsigned half;
    if(!scope || !yes || threshold>100u || !scope->game || !scope->game->attack_rng ||
        fd2_battle_montage_current_half(scope->montage,&half)!=0)return -1;
    *yes=scope->game->attack_rng(scope->game->attack_rng_userdata)%100u<threshold;return 0;
}
int fd2_scene_cinematic_montage_hit(fd2_scene_cinematic *scope) {
    return scope?fd2_battle_montage_hit(scope->montage):-1;
}
int fd2_scene_cinematic_montage_cue(fd2_scene_cinematic *scope,size_t part) {
    uint8_t marker,ticks;int last;
    /* 真25a96先检查54133==0；两half均非0，bank和AIL handle不可达。 */
    return fd2_scene_cinematic_montage_part_info(scope,part,&marker,&ticks,&last);
}
int fd2_scene_cinematic_montage_frame(fd2_scene_cinematic *scope,size_t part) {
    return scope?fd2_battle_montage_frame(scope->montage,part):-1;
}

int fd2_scene_cinematic_page_alloc(fd2_scene_cinematic *scope,int slot,size_t bytes) {
    if(!scope || slot<1 || slot>FD2_CINEMATIC_ROLL || scope->pages[slot].data ||
        (bytes!=64000 && bytes!=128000 && bytes!=224000))return -1;
    uint8_t *data=calloc(1,bytes);if(!data)return -1;
    scope->pages[slot]=(cinematic_page){data,bytes};return 0;
}

int fd2_scene_cinematic_page_release(fd2_scene_cinematic *scope,int slot) {
    if(!scope || slot<1 || slot>FD2_CINEMATIC_ROLL || !scope->pages[slot].data)return -1;
    free(scope->pages[slot].data);scope->pages[slot]=(cinematic_page){0};return 0;
}

int fd2_scene_cinematic_page_clear(fd2_scene_cinematic *scope,int slot,uint8_t value) {
    uint8_t *data;size_t size;
    if(page_get(scope,slot,&data,&size)!=0)return -1;
    memset(data,value,size);return page_commit(scope,slot);
}

int fd2_scene_cinematic_work_clear(fd2_scene_cinematic *scope,uint8_t value) {
    if(!scope || !scope->vga->field_page)return -1;
    uint8_t row[FD2_FIELD_PAGE_STRIDE];memset(row,value,sizeof(row));
    for(int y=0;y<FD2_FIELD_PAGE_HEIGHT;y++)
        if(fd2_field_page_write(scope->vga->field_page,0,y,FD2_FIELD_PAGE_STRIDE,1,
            row,sizeof(row),FD2_FIELD_PAGE_STRIDE)!=0)return -1;
    return 0;
}

int fd2_scene_cinematic_copy(fd2_scene_cinematic *scope,int source,int destination,
        size_t so,size_t dest,size_t width,size_t height,size_t ss,size_t ds) {
    uint8_t *src,*dst;size_t src_size,dst_size;
    if(!scope || (source==FD2_CINEMATIC_APP_PREFIX && destination==source) ||
        page_get(scope,source,&src,&src_size)!=0 ||
        page_get(scope,destination,&dst,&dst_size)!=0 ||
        !rectangle(src_size,so,width,height,ss) ||
        !rectangle(dst_size,dest,width,height,ds))return -1;
    /* 当前原调用的跨页复制无重叠；同一页仅接受相同矩形，避免造通用滚动器。 */
    if(src==dst)return so==dest && ss==ds?0:-1;
    for(size_t row=0;row<height;row++)memcpy(dst+dest+row*ds,src+so+row*ss,width);
    return page_commit(scope,destination);
}

int fd2_scene_cinematic_pixels(const fd2_scene_cinematic *scope,int slot,
        const uint8_t **pixels,size_t *size) {
    uint8_t *data;size_t bytes;
    if(!pixels || !size || page_get((fd2_scene_cinematic *)scope,slot,&data,&bytes)!=0)return -1;
    *pixels=data;*size=bytes;return 0;
}

int fd2_scene_cinematic_sheet_open(fd2_scene_cinematic *scope,int slot,size_t entry) {
    if(!scope || slot<0 || slot>=FD2_CINEMATIC_SHEET_COUNT)return -1;
    cinematic_clip candidate={0};
    if(clip_open(&candidate,scope->fdother,entry)!=0)return -1;
    clip_close(&scope->sheets[slot]);scope->sheets[slot]=candidate;return 0;
}

int fd2_scene_cinematic_sheet_close(fd2_scene_cinematic *scope,int slot) {
    if(!scope || slot<0 || slot>=FD2_CINEMATIC_SHEET_COUNT || !scope->sheets[slot].parts)return -1;
    clip_close(&scope->sheets[slot]);return 0;
}

int fd2_scene_cinematic_sheet_info(const fd2_scene_cinematic *scope,int slot,
        size_t *count,size_t *intro) {
    if(!scope || !count || !intro || slot<0 || slot>=FD2_CINEMATIC_SHEET_COUNT ||
        !scope->sheets[slot].parts)return -1;
    *count=scope->sheets[slot].clip.part_count;*intro=scope->sheets[slot].clip.intro_end;return 0;
}

int fd2_scene_cinematic_sheet_duration(const fd2_scene_cinematic *scope,int slot,
        size_t frame,unsigned *ticks) {
    if(!scope || !ticks || slot<0 || slot>=FD2_CINEMATIC_SHEET_COUNT ||
        !scope->sheets[slot].parts || frame>=scope->sheets[slot].clip.part_count)return -1;
    *ticks=scope->sheets[slot].parts[frame].part.duration;return 0;
}

static int blit(fd2_scene_cinematic *scope,int destination,size_t stride,
        const cinematic_image *image,int64_t x,int64_t y) {
    uint8_t *dst;size_t size;
    if(!image || !image->image.pixels || !image->opaque || stride==0 || stride>640 ||
        page_get(scope,destination,&dst,&size)!=0 || size%stride!=0)return -1;
    size_t rows=destination==FD2_CINEMATIC_ROLL?size/stride:200u;
    if(rows>size/stride)return -1;
    for(int sy=0;sy<image->image.height;sy++) {
        int64_t dy=y+sy;if(dy<0 || (uint64_t)dy>=rows)continue;
        for(int sx=0;sx<image->image.width;sx++) {
            int64_t dx=x+sx;size_t p=(size_t)sy*(size_t)image->image.width+(size_t)sx;
            if(dx>=0 && (uint64_t)dx<stride && image->opaque[p])
                dst[(size_t)dy*stride+(size_t)dx]=image->image.pixels[p];
        }
    }
    return page_commit(scope,destination);
}

int fd2_scene_cinematic_sheet_blit(fd2_scene_cinematic *scope,int slot,size_t frame,
        int destination,int x,int y,size_t stride) {
    if(!scope || slot<0 || slot>=FD2_CINEMATIC_SHEET_COUNT ||
        !scope->sheets[slot].parts || frame>=scope->sheets[slot].clip.part_count)return -1;
    const cinematic_part *part=&scope->sheets[slot].parts[frame];
    return blit(scope,destination,stride,&part->drawing,(int64_t)x+part->part.x,
        (int64_t)y+part->part.y);
}

int fd2_scene_cinematic_image_blit(fd2_scene_cinematic *scope,size_t entry,
        int destination,int x,int y,size_t stride) {
    if(!scope)return -1;
    cinematic_image image={0};
    if(image_entry(&image,scope->fdother,entry)!=0)return -1;
    int result=blit(scope,destination,stride,&image,x,y);image_close(&image);return result;
}

int fd2_scene_cinematic_palette_save(fd2_scene_cinematic *scope) {
    if(!scope || scope->palette_saved)return -1;
    memcpy(scope->saved_palette,scope->vga->palette,768);scope->palette_saved=1;return 0;
}
int fd2_scene_cinematic_palette_validate(const fd2_scene_cinematic *scope,size_t entry) {
    const uint8_t *data;size_t size;
    if(!scope ||
        fd2_archive_get(scope->fdother,entry,&data,&size)!=0 || size!=768)return -1;
    for(size_t i=0;i<size;i++)if(data[i]>63)return -1;
    return 0;
}
int fd2_scene_cinematic_palette_load(fd2_scene_cinematic *scope,size_t entry) {
    const uint8_t *data;size_t size;
    if(!scope || !scope->palette_saved ||
        fd2_scene_cinematic_palette_validate(scope,entry)!=0 ||
        fd2_archive_get(scope->fdother,entry,&data,&size)!=0)return -1;
    memcpy(scope->vga->palette,data,768);return 0;
}
int fd2_scene_cinematic_palette_restore(fd2_scene_cinematic *scope) {
    if(!scope || !scope->palette_saved)return -1;
    memcpy(scope->vga->palette,scope->saved_palette,768);scope->palette_saved=0;return 0;
}

int fd2_scene_cinematic_audio_open(fd2_scene_cinematic *scope,size_t entry) {
    if(!scope || scope->audio_ready || entry!=88)return -1;
    if(fd2_pcm_bank_open(&scope->audio_bank,scope->fdother,entry)!=0)return -1;
    scope->audio_ready=1;return 0;
}
int fd2_scene_cinematic_audio_play(fd2_scene_cinematic *scope,fd2_field_audio *audio,int sample) {
    if(!scope || !scope->audio_ready || sample!=1)return -1;
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(scope->vga)<0)return -1;
    int result=audio?fd2_field_audio_play_borrowed_bank(audio,&scope->audio_bank,sample,1):0;
    return fd2_bios_clock_error()<0 || fd2_vga_output_error(scope->vga)<0?-1:result;
}
int fd2_scene_cinematic_audio_close(fd2_scene_cinematic *scope,fd2_field_audio *audio) {
    if(!scope || !scope->audio_ready)return -1;
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(scope->vga)<0)return -1;
    if(audio && fd2_field_audio_play_borrowed_bank(audio,&scope->audio_bank,-1,1)!=0)return -1;
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(scope->vga)<0)return -1;
    fd2_pcm_bank_close(&scope->audio_bank);scope->audio_ready=0;return 0;
}

int fd2_scene_cinematic_circle_prepare(fd2_scene_cinematic *scope,size_t actor) {
    if(!scope || !scope->game || scope->circle_base || actor>=scope->game->units.count)return -1;
    fd2_field_page *base=fd2_field_page_create();if(!base)return -1;
    if(fd2_field_game_effect_terrain_begin(scope->game,scope->vga,base)!=0)
        {fd2_field_page_destroy(base);return -1;}
    const fd2_field_unit *unit=&scope->game->units.items[actor];
    scope->circle_actor=actor;
    scope->circle_x=((int)unit->x-scope->game->camera_cell_x)*24+12;
    scope->circle_y=((int)unit->y-scope->game->camera_cell_y)*24+18;
    scope->circle_base=base;return 0;
}
int fd2_scene_cinematic_circle_frame(fd2_scene_cinematic *scope,size_t actor,
        int radius,size_t frame) {
    if(!scope || !scope->game || !scope->circle_base || actor>=scope->game->units.count ||
        actor!=scope->circle_actor || frame>9)return -1;
    const uint8_t *lut=fd2_field_lut_frame(&scope->game->visuals,frame);
    if(!lut)return -1;
    return fd2_field_game_effect_mask_frame(scope->game,scope->vga,scope->circle_base,
        lut,scope->circle_x,scope->circle_y,radius,0,1);
}
int fd2_scene_cinematic_circle_close(fd2_scene_cinematic *scope) {
    if(!scope || !scope->circle_base)return -1;
    fd2_field_page_destroy(scope->circle_base);scope->circle_base=NULL;return 0;
}

int fd2_scene_cinematic_shake_prepare(fd2_scene_cinematic *scope) {
    if(!scope || !scope->game || !scope->vga->field_page)return -1;
    fd2_field_game *game=scope->game;
    /* 24b4d的独立11eee先terrain，再实际背景，再range；不推进1297d。 */
    if(fd2_field_visual_terrain_step_live(scope->vga,&scope->vga->field_visual)<0)return -1;
    game->terrain_visual_phase=(uint8_t)scope->vga->field_visual.terrain_phase;
    if(fd2_field_background_draw_live(scope->vga,scope->vga->field_page,72,72,game->camera_cell_x,
            game->camera_cell_y,0,0,1)!=0 ||
        fd2_field_visual_range_step_live(scope->vga,&scope->vga->field_visual,-1)<0)return -1;
    game->range_visual_phase=(uint8_t)scope->vga->field_visual.range_phase;
    if(fd2_terrain_render_page_block(scope->vga->field_page,&game->terrain,&game->map,
            72,72,13,9,game->camera_cell_x,game->camera_cell_y,
            game->terrain_visual_phase,game->idle_phase,NULL,NULL)!=0)return -1;
    return fd2_field_game_runtime_refresh(game,scope->vga,0);
}
int fd2_scene_cinematic_shake_frame(fd2_scene_cinematic *scope,unsigned row) {
    if(!scope || !scope->vga->field_page || row>1)return -1;
    uint8_t line[312];
    /* 原raw8088=page(72,72)，(i&1)*456只选择额外的一行。 */
    for(int y=0;y<192;y++) {
        if(fd2_field_page_read(scope->vga->field_page,72,72+(int)row+y,312,1,
            line,sizeof(line),312)!=0)return -1;
        memcpy(scope->vga->framebuffer+(size_t)(y+4)*320+4,line,312);
    }
    return 0;
}

/* credits字模仍走fd2_font_draw_glyph；局部18×17桥把已有目的像素借到
 * 同一VGA字模器，保留透明底和下左shadow，不复制字模算法。 */
static int text_validate(const fd2_scene_cinematic *scope,const fd2_text_entry *text,
        size_t fragment) {
    const uint8_t *tokens;size_t count;
    if(!scope->game || !scope->game->font.glyphs ||
        fd2_text_entry_get_fragment(text,fragment,&tokens,&count)!=0 || !count)return -1;
    for(size_t i=0;i<count;i++) {
        int16_t token=(int16_t)fd2_text_token_at(tokens,i);
        if(token==-1)return i+1u==count?0:-1;
        if(token!=-2 && (token<0 || (size_t)token>=scope->game->font.glyph_count))return -1;
    }
    return -1;
}
static int text_draw(fd2_scene_cinematic *scope,int destination,size_t offset,
        const fd2_text_entry *text,size_t fragment,unsigned pitch) {
    const uint8_t *tokens;size_t count;uint8_t *dst;size_t bytes;
    if(text_validate(scope,text,fragment)!=0 ||
        page_get(scope,destination,&dst,&bytes)!=0 || offset>=bytes || bytes%320u ||
        fd2_text_entry_get_fragment(text,fragment,&tokens,&count)!=0)return -1;
    if(!scope->glyph)scope->glyph=calloc(1,sizeof(*scope->glyph));
    if(!scope->glyph)return -1;
    int64_t origin=(int64_t)(offset%320u),x=origin,y=(int64_t)(offset/320u);
    for(size_t i=0;i<count;i++) {
        int16_t token=(int16_t)fd2_text_token_at(tokens,i);
        if(token==-1)break;
        if(token==-2) {x=origin;y+=pitch;continue;}
        for(int dy=0;dy<17;dy++)for(int dx=0;dx<18;dx++) {
            int64_t px=x-1+dx,py=y+dy;
            scope->glyph->framebuffer[(size_t)dy*320u+(size_t)dx]=
                px>=0 && px<320 && py>=0 && (uint64_t)py<bytes/320u?
                dst[(size_t)py*320u+(size_t)px]:0;
        }
        fd2_font_draw_glyph(scope->glyph,&scope->game->font,(uint16_t)token,1,0,205,76,-1);
        for(int dy=0;dy<17;dy++)for(int dx=0;dx<18;dx++) {
            int64_t px=x-1+dx,py=y+dy;
            if(px>=0 && px<320 && py>=0 && (uint64_t)py<bytes/320u)
                dst[(size_t)py*320u+(size_t)px]=scope->glyph->framebuffer[(size_t)dy*320u+(size_t)dx];
        }
        x+=16;
    }
    return page_commit(scope,destination);
}

int fd2_scene_cinematic_credits_assets_open(fd2_scene_cinematic *scope) {
    if(!scope || !scope->game || scope->credits_assets)return -1;
    /* 31c49先滚FDTXT31时文本已由roll打开；失败只回收本次新开的owner。 */
    int opened_text=!scope->credits_text.data;
    if(fd2_archive_open(&scope->tai,"original_game/TAI.DAT")!=0 ||
        image_entry(&scope->credits_terrain,&scope->tai,3)!=0 ||
        (opened_text &&
            fd2_text_entry_open_entry(&scope->credits_text,&scope->game->fdtxt_archive,31)!=0))
        goto fail;
    scope->credits_assets=1;return 0;
fail:
    if(opened_text)fd2_text_entry_close(&scope->credits_text);
    image_close(&scope->credits_terrain);
    fd2_archive_close(&scope->tai);return -1;
}
int fd2_scene_cinematic_credits_assets_close(fd2_scene_cinematic *scope) {
    if(!scope || !scope->credits_assets || scope->actor_ready)return -1;
    fd2_text_entry_close(&scope->credits_text);image_close(&scope->credits_terrain);
    fd2_archive_close(&scope->tai);scope->credits_assets=0;return 0;
}
int fd2_scene_cinematic_credits_actor_close(fd2_scene_cinematic *scope) {
    if(!scope)return -1;
    clip_close(&scope->idle);clip_close(&scope->intro);
    fd2_image_free(&scope->portrait[0]);fd2_image_free(&scope->portrait[1]);
    scope->actor_ready=scope->idle_ready=0;return 0;
}
int fd2_scene_cinematic_credits_actor(fd2_scene_cinematic *scope,size_t outer) {
    if(!scope || !scope->game || !scope->credits_assets || scope->actor_ready ||
        outer>=scope->game->roster_count || outer>=scope->game->units.count)return -1;
    size_t raw=outer==0?1:outer==1?0:outer;
    if(raw>=scope->game->units.count)return -1;
    const fd2_field_unit *unit=&scope->game->units.items[raw];
    if(text_validate(scope,&scope->credits_text,10)!=0 ||
        text_validate(scope,&scope->credits_text,11)!=0 ||
        text_validate(scope,&scope->credits_text,(uint8_t)(unit->text_id+12u))!=0 ||
        (!outer && text_validate(scope,&scope->credits_text,45)!=0) ||
        text_validate(scope,&scope->game->ui_text,(size_t)unit->text_id+1u)!=0 ||
        text_validate(scope,&scope->game->ui_text,(size_t)unit->movement_profile+150u)!=0)
        return -1;
    if(clip_open(&scope->idle,&scope->game->figani_archive,3u*unit->unit_id)!=0 ||
        clip_open(&scope->intro,&scope->game->figani_archive,3u*unit->unit_id+1u)!=0 ||
        scope->intro.clip.intro_end>scope->intro.clip.part_count ||
        fd2_portrait_decode_frame(&scope->portrait[0],&scope->game->dato_archive,unit->unit_id,0)!=0 ||
        fd2_portrait_decode_frame(&scope->portrait[1],&scope->game->dato_archive,unit->unit_id,3)!=0)
        goto fail;
    scope->outer_index=outer;scope->raw_actor=raw;scope->actor_ready=1;return 0;
fail:
    (void)fd2_scene_cinematic_credits_actor_close(scope);return -1;
}

static int clip_draw(fd2_scene_cinematic *scope,const cinematic_clip *clip,size_t frame,
        int destination,int x,int y,size_t stride) {
    if(!clip->parts || frame>=clip->clip.part_count)return -1;
    const cinematic_part *part=&clip->parts[frame];
    return blit(scope,destination,stride,&part->drawing,(int64_t)x+part->part.x,
        (int64_t)y+part->part.y);
}
int fd2_scene_cinematic_credits_intro(fd2_scene_cinematic *scope,unsigned phase) {
    if(!scope || !scope->actor_ready || phase>8)return -1;
    int side=scope->game->units.items[scope->outer_index].side!=0;
    size_t offset=side?0:320;
    if(fd2_scene_cinematic_copy(scope,FD2_CINEMATIC_BASE,FD2_CINEMATIC_WORK,
        0,offset,320,200,320,640)!=0)return -1;
    if(side && blit(scope,FD2_CINEMATIC_WORK,640,&scope->credits_terrain,
        164+(int)phase*10,157)!=0)return -1;
    if(clip_draw(scope,&scope->idle,0,FD2_CINEMATIC_WORK,
        side?(int)phase*10:320-(int)phase*10,0,640)!=0)return -1;
    return fd2_scene_cinematic_copy(scope,FD2_CINEMATIC_WORK,FD2_CINEMATIC_VGA,
        offset,0,320,200,640,320);
}
int fd2_scene_cinematic_credits_intro_finish(fd2_scene_cinematic *scope) {
    if(!scope || !scope->actor_ready)return -1;
    return scope->game->units.items[scope->outer_index].side!=0?
        blit(scope,FD2_CINEMATIC_BASE,320,&scope->credits_terrain,164,157):0;
}
int fd2_scene_cinematic_credits_idle_reset(fd2_scene_cinematic *scope) {
    if(!scope || !scope->actor_ready)return -1;
    scope->idle_ticks=scope->idle_part=0;scope->idle_ready=1;return 0;
}
int fd2_scene_cinematic_credits_idle(fd2_scene_cinematic *scope,int destination) {
    if(!scope || !scope->actor_ready || !scope->idle_ready ||
        scope->idle_part>=scope->idle.clip.part_count)return -1;
    unsigned part=scope->idle_part;
    if(clip_draw(scope,&scope->idle,part,destination,0,0,320)!=0)return -1;
    scope->idle_ticks++;
    if(scope->idle_ticks>=scope->idle.parts[part].part.duration) {
        scope->idle_ticks=0;scope->idle_part++;
        if(scope->idle_part>=scope->idle.clip.part_count)scope->idle_part=0;
    }
    return 0;
}
int fd2_scene_cinematic_credits_part(fd2_scene_cinematic *scope,size_t part) {
    if(!scope || !scope->actor_ready || part>=scope->intro.clip.intro_end)return -1;
    return clip_draw(scope,&scope->intro,part,FD2_CINEMATIC_WORK,0,0,320);
}
int fd2_scene_cinematic_credits_part_info(const fd2_scene_cinematic *scope,
        size_t *count,size_t part,unsigned *ticks) {
    if(!scope || !scope->actor_ready || !count || !ticks ||
        part>=scope->intro.clip.intro_end)return -1;
    *count=scope->intro.clip.intro_end;*ticks=scope->intro.parts[part].part.duration;return 0;
}
int fd2_scene_cinematic_credits_part_count(const fd2_scene_cinematic *scope,size_t *count) {
    if(!scope || !scope->actor_ready || !count)return -1;
    *count=scope->intro.clip.intro_end;return 0;
}
int fd2_scene_cinematic_credits_card_prepare(fd2_scene_cinematic *scope) {
    if(!scope || !scope->actor_ready || !scope->pages[FD2_CINEMATIC_BASE].data ||
        scope->pages[FD2_CINEMATIC_BASE].size<64000 ||
        !scope->pages[FD2_CINEMATIC_CARD].data || scope->pages[FD2_CINEMATIC_CARD].size<64000)
        return -1;
    if(!scope->glyph)scope->glyph=calloc(1,sizeof(*scope->glyph));
    if(!scope->glyph)return -1;
    memcpy(scope->glyph->framebuffer,scope->pages[FD2_CINEMATIC_BASE].data,64000);
    if(fd2_field_detail_portrait_border_draw(scope->glyph,&scope->game->info_assets)!=0)return -1;
    memcpy(scope->pages[FD2_CINEMATIC_CARD].data,scope->glyph->framebuffer,64000);return 0;
}
int fd2_scene_cinematic_credits_blink_reset(fd2_scene_cinematic *scope) {
    if(!scope)return -1;
    scope->blink_countdown=0;scope->blink_ready=1;return 0;
}

int fd2_scene_cinematic_credits_card(fd2_scene_cinematic *scope,unsigned frame) {
    if(!scope || !scope->actor_ready || !scope->idle_ready || !scope->blink_ready ||
        frame>=(scope->outer_index?220u:440u) || !scope->game->attack_rng)return -1;
    uint8_t *card;size_t size;
    if(page_get(scope,FD2_CINEMATIC_CARD,&card,&size)!=0 || size<64000 ||
        !scope->pages[FD2_CINEMATIC_WORK].data ||
        scope->pages[FD2_CINEMATIC_WORK].size<64000)return -1;
    /* 31c49先恢复卡片底页，再推进局部眨眼countdown；不另seed/借第二RNG。 */
    memcpy(scope->pages[FD2_CINEMATIC_WORK].data,card,64000);
    if(scope->blink_countdown)scope->blink_countdown--;
    else scope->blink_countdown=(uint8_t)((scope->game->attack_rng(
        scope->game->attack_rng_userdata)&31u)+40u);
    const fd2_image *portrait=&scope->portrait[scope->blink_countdown<2u];
    uint8_t *work=scope->pages[FD2_CINEMATIC_WORK].data;
    for(int y=0;y<portrait->height;y++)for(int x=0;x<portrait->width;x++) {
        size_t at=(size_t)y*(size_t)portrait->width+(size_t)x;
        /* 31c49→4ebff的LMI没有SKIP/透明色，literal0也覆写底页。 */
        if(x+8<320 && y+10<200)
            work[(size_t)(y+10)*320u+(size_t)(x+8)]=portrait->pixels[at];
    }
    if(fd2_scene_cinematic_credits_idle(scope,FD2_CINEMATIC_WORK)!=0)return -1;
    const fd2_field_unit *unit=&scope->game->units.items[scope->raw_actor];
    return text_draw(scope,FD2_CINEMATIC_WORK,5865,&scope->credits_text,10,0)!=0 ||
        text_draw(scope,FD2_CINEMATIC_WORK,5915,&scope->game->ui_text,
            (size_t)unit->text_id+1u,0)!=0 ||
        text_draw(scope,FD2_CINEMATIC_WORK,12265,&scope->credits_text,11,0)!=0 ||
        text_draw(scope,FD2_CINEMATIC_WORK,12315,&scope->game->ui_text,
            (size_t)unit->movement_profile+150u,0)!=0 ||
        text_draw(scope,FD2_CINEMATIC_WORK,32008,&scope->credits_text,
            frame>=220u?45u:(uint8_t)(unit->text_id+12u),20)!=0?-1:0;
}

int fd2_scene_cinematic_credits_roll_text(fd2_scene_cinematic *scope) {
    if(!scope || !scope->game || !scope->pages[FD2_CINEMATIC_ROLL].data ||
        scope->pages[FD2_CINEMATIC_ROLL].size!=224000)return -1;
    /* 31c49先滚FDTXT31/44，之后才加载TAI3；不把后置资源owner前移。 */
    if(!scope->credits_text.data && fd2_text_entry_open_entry(&scope->credits_text,
        &scope->game->fdtxt_archive,31)!=0)return -1;
    return text_draw(scope,FD2_CINEMATIC_ROLL,76848,&scope->credits_text,44,25);
}

int fd2_scene_cinematic_credits_skip_set(fd2_scene_cinematic *scope,int skip) {
    if(!scope || (skip!=0 && skip!=1))return -1;
    scope->card_skip=(unsigned)skip;return 0;
}
int fd2_scene_cinematic_credits_skip_get(const fd2_scene_cinematic *scope,int *skip) {
    if(!scope || !skip)return -1;
    *skip=(int)scope->card_skip;return 0;
}
