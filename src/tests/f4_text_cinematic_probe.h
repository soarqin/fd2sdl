/* 私有TU编译同一scene_cinematic.c；55个现公开符号局部重命名，避免core重符。
 * 原型逐一取现scene_cinematic.h，不保存猜测ABI/类型；实际text_draw无副本。 */
#ifndef FD2_F4_TEXT_CINEMATIC_PROBE_H
#define FD2_F4_TEXT_CINEMATIC_PROBE_H
#define fd2_scene_cinematic_open f4_text_private_open
#define fd2_scene_cinematic_close f4_text_private_close
#define fd2_scene_cinematic_page_alloc f4_text_private_page_alloc
#define fd2_scene_cinematic_page_release f4_text_private_page_release
#define fd2_scene_cinematic_page_clear f4_text_private_page_clear
#define fd2_scene_cinematic_work_clear f4_text_private_work_clear
#define fd2_scene_cinematic_copy f4_text_private_copy
#define fd2_scene_cinematic_pixels f4_text_private_pixels
#define fd2_scene_cinematic_sheet_open f4_text_private_sheet_open
#define fd2_scene_cinematic_sheet_close f4_text_private_sheet_close
#define fd2_scene_cinematic_sheet_info f4_text_private_sheet_info
#define fd2_scene_cinematic_sheet_duration f4_text_private_sheet_duration
#define fd2_scene_cinematic_sheet_blit f4_text_private_sheet_blit
#define fd2_scene_cinematic_image_blit f4_text_private_image_blit
#define fd2_scene_cinematic_palette_save f4_text_private_palette_save
#define fd2_scene_cinematic_palette_validate f4_text_private_palette_validate
#define fd2_scene_cinematic_palette_load f4_text_private_palette_load
#define fd2_scene_cinematic_palette_restore f4_text_private_palette_restore
#define fd2_scene_cinematic_audio_open f4_text_private_audio_open
#define fd2_scene_cinematic_audio_play f4_text_private_audio_play
#define fd2_scene_cinematic_audio_close f4_text_private_audio_close
#define fd2_scene_cinematic_circle_prepare f4_text_private_circle_prepare
#define fd2_scene_cinematic_circle_frame f4_text_private_circle_frame
#define fd2_scene_cinematic_circle_close f4_text_private_circle_close
#define fd2_scene_cinematic_shake_prepare f4_text_private_shake_prepare
#define fd2_scene_cinematic_shake_frame f4_text_private_shake_frame
#define fd2_scene_cinematic_credits_actor f4_text_private_credits_actor
#define fd2_scene_cinematic_credits_assets_open f4_text_private_credits_assets_open
#define fd2_scene_cinematic_credits_assets_close f4_text_private_credits_assets_close
#define fd2_scene_cinematic_credits_intro f4_text_private_credits_intro
#define fd2_scene_cinematic_credits_intro_finish f4_text_private_credits_intro_finish
#define fd2_scene_cinematic_credits_idle_reset f4_text_private_credits_idle_reset
#define fd2_scene_cinematic_credits_idle f4_text_private_credits_idle
#define fd2_scene_cinematic_credits_part f4_text_private_credits_part
#define fd2_scene_cinematic_credits_part_info f4_text_private_credits_part_info
#define fd2_scene_cinematic_credits_part_count f4_text_private_credits_part_count
#define fd2_scene_cinematic_credits_card_prepare f4_text_private_credits_card_prepare
#define fd2_scene_cinematic_credits_blink_reset f4_text_private_credits_blink_reset
#define fd2_scene_cinematic_credits_card f4_text_private_credits_card
#define fd2_scene_cinematic_credits_actor_close f4_text_private_credits_actor_close
#define fd2_scene_cinematic_credits_skip_set f4_text_private_credits_skip_set
#define fd2_scene_cinematic_credits_skip_get f4_text_private_credits_skip_get
#define fd2_scene_cinematic_credits_roll_text f4_text_private_credits_roll_text
#define fd2_scene_cinematic_montage_open f4_text_private_montage_open
#define fd2_scene_cinematic_montage_close f4_text_private_montage_close
#define fd2_scene_cinematic_montage_intro f4_text_private_montage_intro
#define fd2_scene_cinematic_montage_intro_finish f4_text_private_montage_intro_finish
#define fd2_scene_cinematic_montage_half f4_text_private_montage_half
#define fd2_scene_cinematic_montage_current_half f4_text_private_montage_current_half
#define fd2_scene_cinematic_montage_part_count f4_text_private_montage_part_count
#define fd2_scene_cinematic_montage_part_info f4_text_private_montage_part_info
#define fd2_scene_cinematic_montage_rng f4_text_private_montage_rng
#define fd2_scene_cinematic_montage_hit f4_text_private_montage_hit
#define fd2_scene_cinematic_montage_cue f4_text_private_montage_cue
#define fd2_scene_cinematic_montage_frame f4_text_private_montage_frame

int fd2_scene_cinematic_open(fd2_scene_cinematic **out,fd2_vga *vga,
    fd2_field_game *game,const fd2_archive *fdother);
void fd2_scene_cinematic_close(fd2_scene_cinematic **scope);
int fd2_scene_cinematic_page_alloc(fd2_scene_cinematic *scope,int slot,size_t bytes);
int fd2_scene_cinematic_page_release(fd2_scene_cinematic *scope,int slot);
int fd2_scene_cinematic_page_clear(fd2_scene_cinematic *scope,int slot,uint8_t value);
int fd2_scene_cinematic_work_clear(fd2_scene_cinematic *scope,uint8_t value);
int fd2_scene_cinematic_copy(fd2_scene_cinematic *scope,int source,int destination,
    size_t source_offset,size_t destination_offset,size_t width,size_t height,
    size_t source_stride,size_t destination_stride);
int fd2_scene_cinematic_pixels(const fd2_scene_cinematic *scope,int slot,
    const uint8_t **pixels,size_t *size);
int fd2_scene_cinematic_sheet_open(fd2_scene_cinematic *scope,int slot,size_t entry);
int fd2_scene_cinematic_sheet_close(fd2_scene_cinematic *scope,int slot);
int fd2_scene_cinematic_sheet_info(const fd2_scene_cinematic *scope,int slot,
    size_t *count,size_t *intro_count);
int fd2_scene_cinematic_sheet_duration(const fd2_scene_cinematic *scope,int slot,
    size_t frame,unsigned *ticks);
int fd2_scene_cinematic_sheet_blit(fd2_scene_cinematic *scope,int slot,size_t frame,
    int destination,int x,int y,size_t stride);
int fd2_scene_cinematic_image_blit(fd2_scene_cinematic *scope,size_t entry,
    int destination,int x,int y,size_t stride);
int fd2_scene_cinematic_palette_save(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_palette_validate(const fd2_scene_cinematic *scope,size_t entry);
int fd2_scene_cinematic_palette_load(fd2_scene_cinematic *scope,size_t entry);
int fd2_scene_cinematic_palette_restore(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_audio_open(fd2_scene_cinematic *scope,size_t entry);
int fd2_scene_cinematic_audio_play(fd2_scene_cinematic *scope,fd2_field_audio *audio,int sample);
int fd2_scene_cinematic_audio_close(fd2_scene_cinematic *scope,fd2_field_audio *audio);
int fd2_scene_cinematic_circle_prepare(fd2_scene_cinematic *scope,size_t actor);
int fd2_scene_cinematic_circle_frame(fd2_scene_cinematic *scope,size_t actor,
    int radius,size_t lut_frame);
int fd2_scene_cinematic_circle_close(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_shake_prepare(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_shake_frame(fd2_scene_cinematic *scope,unsigned row);
int fd2_scene_cinematic_credits_actor(fd2_scene_cinematic *scope,size_t outer_index);
int fd2_scene_cinematic_credits_assets_open(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_credits_assets_close(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_credits_intro(fd2_scene_cinematic *scope,unsigned phase);
int fd2_scene_cinematic_credits_intro_finish(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_credits_idle_reset(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_credits_idle(fd2_scene_cinematic *scope,int destination);
int fd2_scene_cinematic_credits_part(fd2_scene_cinematic *scope,size_t part);
int fd2_scene_cinematic_credits_part_info(const fd2_scene_cinematic *scope,
    size_t *count,size_t part,unsigned *ticks);
int fd2_scene_cinematic_credits_part_count(const fd2_scene_cinematic *scope,
    size_t *count);
int fd2_scene_cinematic_credits_card_prepare(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_credits_blink_reset(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_credits_card(fd2_scene_cinematic *scope,unsigned frame);
int fd2_scene_cinematic_credits_actor_close(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_credits_skip_set(fd2_scene_cinematic *scope,int skip);
int fd2_scene_cinematic_credits_skip_get(const fd2_scene_cinematic *scope,int *skip);
int fd2_scene_cinematic_credits_roll_text(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_montage_open(fd2_scene_cinematic *scope,int first,int second,int scene);
int fd2_scene_cinematic_montage_close(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_montage_intro(fd2_scene_cinematic *scope,unsigned phase);
int fd2_scene_cinematic_montage_intro_finish(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_montage_half(fd2_scene_cinematic *scope,unsigned half);
int fd2_scene_cinematic_montage_current_half(const fd2_scene_cinematic *scope,unsigned *half);
int fd2_scene_cinematic_montage_part_count(const fd2_scene_cinematic *scope,size_t *count);
int fd2_scene_cinematic_montage_part_info(const fd2_scene_cinematic *scope,size_t part,
    uint8_t *marker,uint8_t *ticks,int *last_marker);
int fd2_scene_cinematic_montage_rng(fd2_scene_cinematic *scope,unsigned threshold,int *yes);
int fd2_scene_cinematic_montage_hit(fd2_scene_cinematic *scope);
int fd2_scene_cinematic_montage_cue(fd2_scene_cinematic *scope,size_t part);
int fd2_scene_cinematic_montage_frame(fd2_scene_cinematic *scope,size_t part);
#include "../scene_cinematic.c"

/* 此段位于同一实际scene_cinematic.c私有TU之后；text_draw不是副本算法。 */
#define FD2_F4_PROTOCOL_ONLY
#include "f4_reader_probe.h"
#undef FD2_F4_PROTOCOL_ONLY
#include "f4_text_probe_common.h"
static int f4_text_cinematic_case(fd2_scene_cinematic *scope,const char *op,unsigned fragment,unsigned kind,unsigned variant) {
    if(strcmp(op,"credit") || variant || kind>4u)return -1;
    size_t size=kind==4u?224000u:128000u,offset=0;unsigned pitch=0;
    int destination=kind==4u?FD2_CINEMATIC_ROLL:FD2_CINEMATIC_WORK;
    const fd2_text_entry *text=&scope->credits_text;
    switch(kind) {
    case 0:if(fragment!=10u && fragment!=11u)return -1;offset=fragment==10u?5865u:12265u;break;
    case 1:if(fragment<1u || fragment>136u)return -1;text=&scope->game->ui_text;offset=5915u;break;
    case 2:if(fragment<150u || fragment>178u)return -1;text=&scope->game->ui_text;offset=12315u;break;
    case 3:offset=32008u;pitch=20;break;
    case 4:if(fragment!=44u)return -1;offset=76848u;pitch=25;break;
    }
    if(scope->pages[destination].data)return -1;
    uint8_t *pixels[2]={malloc(size),malloc(size)},*mask=malloc(size),palette[768],pmask[768]={0};
    fd2_field_game *before=malloc(sizeof(*before));fd2_vga *vga_before=malloc(sizeof(*vga_before));
    unsigned written=0;int result=-1;
    if(!pixels[0] || !pixels[1] || !mask || !before || !vga_before)goto done;
    *before=*scope->game;*vga_before=*scope->vga;memset(palette,165,768);
    if(fd2_scene_cinematic_page_alloc(scope,destination,size))goto done;
    for(unsigned pass=0;pass<2u;pass++) {
        f4_seed(scope->pages[destination].data,size,320,0,(int)pass);
        if(text_draw(scope,destination,offset,text,fragment,pitch) ||
           memcmp(before,scope->game,sizeof(*before)) || memcmp(vga_before,scope->vga,sizeof(*vga_before)))goto done;
        memcpy(pixels[pass],scope->pages[destination].data,size);
    }
    if(f4_mask(mask,pixels[0],pixels[1],size,320,0,&written))goto done;
    result=f4_emit(op,fragment,kind,variant,320,(unsigned)(size/320u),pixels[0],palette,mask,pmask,written,0);
done:
    if(scope->pages[destination].data)(void)fd2_scene_cinematic_page_release(scope,destination);
    free(pixels[0]);free(pixels[1]);free(mask);free(before);free(vga_before);return result;
}
static int f4_text_run(const char *root) {
    fd2_archive other={0};fd2_field_game *game=calloc(1,sizeof(*game));fd2_vga *vga=calloc(1,sizeof(*vga));
    fd2_scene_cinematic *scope=NULL;char path[1024],line[128],op[16];unsigned v[3];int result=1;
    if(!game || !vga)goto done;
    memset(vga->palette,165,768);
    if(f4_path(path,root,"FDOTHER.DAT") || fd2_archive_open(&other,path) ||
       f4_path(path,root,"FDTXT.DAT") || fd2_archive_open(&game->fdtxt_archive,path) ||
       fd2_font_open_fdother(&game->font,&other,4) ||
       fd2_text_entry_open_entry(&game->ui_text,&game->fdtxt_archive,0) ||
       fd2_scene_cinematic_open(&scope,vga,game,&other) ||
       fd2_text_entry_open_entry(&scope->credits_text,&game->fdtxt_archive,31) ||
       f4_input_receipt("FDOTHER.DAT",other.data,other.size) ||
       f4_input_receipt("FDTXT.DAT",game->fdtxt_archive.data,game->fdtxt_archive.size))goto done;
    while(fgets(line,sizeof(line),stdin)) {
        if(!strchr(line,'\n') || f4_parse(line,op,v) || f4_text_cinematic_case(scope,op,v[0],v[1],v[2]))goto done;
    }
    result=ferror(stdin)?1:0;
done:
    fd2_scene_cinematic_close(&scope);if(game){fd2_text_entry_close(&game->ui_text);fd2_archive_close(&game->fdtxt_archive);free(game);}
    fd2_archive_close(&other);free(vga);if(result)fputs("F4 credits text reader rejected input/output\n",stderr);return result;
}

#undef fd2_scene_cinematic_open
#undef fd2_scene_cinematic_close
#undef fd2_scene_cinematic_page_alloc
#undef fd2_scene_cinematic_page_release
#undef fd2_scene_cinematic_page_clear
#undef fd2_scene_cinematic_work_clear
#undef fd2_scene_cinematic_copy
#undef fd2_scene_cinematic_pixels
#undef fd2_scene_cinematic_sheet_open
#undef fd2_scene_cinematic_sheet_close
#undef fd2_scene_cinematic_sheet_info
#undef fd2_scene_cinematic_sheet_duration
#undef fd2_scene_cinematic_sheet_blit
#undef fd2_scene_cinematic_image_blit
#undef fd2_scene_cinematic_palette_save
#undef fd2_scene_cinematic_palette_validate
#undef fd2_scene_cinematic_palette_load
#undef fd2_scene_cinematic_palette_restore
#undef fd2_scene_cinematic_audio_open
#undef fd2_scene_cinematic_audio_play
#undef fd2_scene_cinematic_audio_close
#undef fd2_scene_cinematic_circle_prepare
#undef fd2_scene_cinematic_circle_frame
#undef fd2_scene_cinematic_circle_close
#undef fd2_scene_cinematic_shake_prepare
#undef fd2_scene_cinematic_shake_frame
#undef fd2_scene_cinematic_credits_actor
#undef fd2_scene_cinematic_credits_assets_open
#undef fd2_scene_cinematic_credits_assets_close
#undef fd2_scene_cinematic_credits_intro
#undef fd2_scene_cinematic_credits_intro_finish
#undef fd2_scene_cinematic_credits_idle_reset
#undef fd2_scene_cinematic_credits_idle
#undef fd2_scene_cinematic_credits_part
#undef fd2_scene_cinematic_credits_part_info
#undef fd2_scene_cinematic_credits_part_count
#undef fd2_scene_cinematic_credits_card_prepare
#undef fd2_scene_cinematic_credits_blink_reset
#undef fd2_scene_cinematic_credits_card
#undef fd2_scene_cinematic_credits_actor_close
#undef fd2_scene_cinematic_credits_skip_set
#undef fd2_scene_cinematic_credits_skip_get
#undef fd2_scene_cinematic_credits_roll_text
#undef fd2_scene_cinematic_montage_open
#undef fd2_scene_cinematic_montage_close
#undef fd2_scene_cinematic_montage_intro
#undef fd2_scene_cinematic_montage_intro_finish
#undef fd2_scene_cinematic_montage_half
#undef fd2_scene_cinematic_montage_current_half
#undef fd2_scene_cinematic_montage_part_count
#undef fd2_scene_cinematic_montage_part_info
#undef fd2_scene_cinematic_montage_rng
#undef fd2_scene_cinematic_montage_hit
#undef fd2_scene_cinematic_montage_cue
#undef fd2_scene_cinematic_montage_frame
#endif
