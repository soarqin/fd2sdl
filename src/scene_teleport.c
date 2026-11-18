/* sub_22253/22470/22547/22656 @canonical同地址（原sub_*）：完整chunk、
 * caller push/read与S5严格读集。临时页借App背景owner，不借法术/费用状态。
 * 22547共享尾返回frame0 LUT；22253先写raw.xy再mask与逐行wipe。 */
#include "scene_teleport.h"
#include "scene.h"
#include "bios_clock.h"
#include "portrait.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

struct fd2_scene_teleport_scope {
    fd2_field_game *game;
    fd2_vga *vga;
    fd2_field_page *owner, *base;
    const fd2_archive *fdother;
    fd2_pcm_bank sound;
    size_t stage, actor;
    int destination_x, destination_y, draw_x, draw_y, center_x, center_y;
};

static uint32_t teleport_u32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}

static int teleport_image(const fd2_archive *archive, unsigned frame, fd2_image *image) {
    const uint8_t *data; size_t size;
    if (!archive || !image || fd2_archive_get(archive,6,&data,&size) || size<6 ||
            memcmp(data,"LMI1",4)) return -1;
    size_t count=(size_t)data[4]|((size_t)data[5]<<8), table=6+4*count;
    if (frame>=count || table>size) return -1;
    size_t a=teleport_u32(data+6+4*frame), b=frame+1<count ? teleport_u32(data+10+4*frame):size;
    return a>=table && a<b && b<=size ? fd2_lmi_decode_image(image,data+a,b-a):-1;
}

static int teleport_owner(const fd2_scene_teleport_scope *scope) {
    if (!scope || !scope->game || !scope->game->ready || !scope->vga || !scope->base ||
            scope->vga->field_page!=scope->owner || scope->game->stage!=scope->stage ||
            scope->game->units.count>FD2_FIELD_MAX_UNITS ||
            scope->actor>=FD2_FIELD_MAX_UNITS) return FD2_SCENE_RESULT_ERROR;
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(scope->vga)<0)return FD2_SCENE_RESULT_ERROR;
    return fd2_input_take_quit(&scope->vga->input) ? FD2_SCENE_RESULT_HOST_QUIT:0;
}

int fd2_scene_teleport_validate(const fd2_archive *fdother) {
    fd2_pcm_bank bank={0};
    if (fd2_pcm_bank_open(&bank,fdother,81)) return -1;
    int result=fd2_pcm_bank_count(&bank)>=2 ? 0:-1;
    fd2_pcm_bank_close(&bank);
    for (unsigned i=114;!result && i<=124;i++) {
        fd2_image image={0}; result=teleport_image(fdother,i,&image); fd2_image_free(&image);
    }
    return result;
}

int fd2_scene_teleport_begin(fd2_scene_teleport_scope **scope,
        fd2_field_game *game,fd2_vga *vga,const fd2_archive *fdother,
        size_t actor,int destination_x,int destination_y,int draw_x,int draw_y) {
    if (!scope || *scope || !game || !game->ready || !vga || !vga->field_page || !fdother ||
            game->units.count>FD2_FIELD_MAX_UNITS || actor>=FD2_FIELD_MAX_UNITS ||
            destination_x<0 || destination_x>255 || destination_y<0 || destination_y>255 ||
            draw_x<0 || draw_x>255 || draw_y<0 || draw_y>255) return -1;
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(vga)<0)return FD2_SCENE_RESULT_ERROR;
    if (fd2_input_take_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    int64_t center_x=((int64_t)game->cursor_cell_x-game->camera_cell_x)*24+12;
    int64_t center_y=((int64_t)game->cursor_cell_y-game->camera_cell_y)*24+15;
    if (center_x<INT_MIN || center_x>INT_MAX || center_y<INT_MIN || center_y>INT_MAX) return -1;
    fd2_scene_teleport_scope *created=calloc(1,sizeof(*created));
    if (!created) return -1;
    created->game=game; created->vga=vga; created->owner=vga->field_page;
    created->stage=game->stage; created->actor=actor; created->fdother=fdother;
    created->destination_x=destination_x; created->destination_y=destination_y;
    created->draw_x=draw_x; created->draw_y=draw_y;
    /* 222e8/22303读的是当前focus-relative，不由draw坐标反算。 */
    created->center_x=(int)center_x; created->center_y=(int)center_y;
    *scope=created;
    if (fd2_pcm_bank_open(&created->sound,fdother,81) ||
            !(created->base=fd2_field_page_create()) ||
            fd2_field_game_effect_terrain_begin(game,vga,created->base)) {
        fd2_scene_teleport_release(scope); return -1;
    }
    return teleport_owner(created);
}

static int teleport_blit(fd2_scene_teleport_scope *scope,fd2_field_page *target,unsigned frame) {
    fd2_image image={0};
    if (teleport_image(scope->fdother,frame,&image)) return -1;
    int64_t x=72+((int64_t)scope->draw_x-scope->game->camera_cell_x)*24;
    int64_t y=73+((int64_t)scope->draw_y-scope->game->camera_cell_y)*24;
    int result=0;
    uint8_t row[FD2_FIELD_PAGE_STRIDE];
    for (int sy=0;sy<image.height && !result;sy++) {
        int64_t dy=y+sy, left=x<0 ? -x:0, right=image.width;
        if (right>FD2_FIELD_PAGE_STRIDE-x) right=FD2_FIELD_PAGE_STRIDE-x;
        if (dy<0 || dy>=FD2_FIELD_PAGE_HEIGHT || left>=right) continue;
        int width=(int)(right-left);
        result=fd2_field_page_read(target,(int)(x+left),(int)dy,width,1,row,sizeof(row),(size_t)width);
        for (int sx=(int)left;!result && sx<right;sx++) {
            uint8_t value=image.pixels[(size_t)sy*(size_t)image.width+(size_t)sx];
            if (value) row[sx-left]=value; /* 4ebab LMI颜色0透明。 */
        }
        if (!result) result=fd2_field_page_write(target,(int)(x+left),(int)dy,width,1,row,sizeof(row),(size_t)width);
    }
    fd2_image_free(&image); return result;
}

int fd2_scene_teleport_sprite(fd2_scene_teleport_scope *scope,unsigned frame) {
    int result=teleport_owner(scope); if (result) return result;
    if (frame<114 || frame>124 || fd2_field_page_copy_pixels(scope->owner,scope->base) ||
            teleport_blit(scope,scope->owner,frame) ||
            fd2_field_game_effect_actors(scope->game,scope->vga)) return -1;
    fd2_vga_upload_current_frame(scope->vga); return teleport_owner(scope);
}

int fd2_scene_teleport_icon(fd2_scene_teleport_scope *scope) {
    int result=teleport_owner(scope); return result ? result:teleport_blit(scope,scope->base,124);
}

int fd2_scene_teleport_sound(fd2_scene_teleport_scope *scope,fd2_field_audio *audio) {
    int result=teleport_owner(scope); if (result || !audio || !audio->player) return result;
    int sample=scope->destination_x==scope->draw_x && scope->destination_y==scope->draw_y;
    result=fd2_field_audio_play_borrowed_bank(audio,&scope->sound,sample,1);
    if(fd2_bios_clock_error()<0 || fd2_vga_output_error(scope->vga)<0)return FD2_SCENE_RESULT_ERROR;
    return result;
}

int fd2_scene_teleport_contract(fd2_scene_teleport_scope *scope,unsigned frame,unsigned multiplier) {
    int result=teleport_owner(scope); if (result) return result;
    if (frame>5 || multiplier>5) return -1;
    const uint8_t *lut=fd2_field_lut_frame(&scope->game->visuals,frame);
    if (!lut || fd2_field_game_effect_mask_frame(scope->game,scope->vga,scope->base,lut,
            scope->center_x,scope->center_y,11,(scope->center_y/5)*(int)multiplier,0)) return -1;
    fd2_vga_upload_current_frame(scope->vga); return teleport_owner(scope);
}

int fd2_scene_teleport_commit(fd2_scene_teleport_scope *scope) {
    int result=teleport_owner(scope); if (result) return result;
    scope->game->units.items[scope->actor].x=(uint8_t)scope->destination_x;
    scope->game->units.items[scope->actor].y=(uint8_t)scope->destination_y;
    const uint8_t *lut=fd2_field_lut_frame(&scope->game->visuals,0);
    return lut ? fd2_field_game_effect_mask_page(scope->game,scope->vga,scope->base,lut,
        scope->center_x,scope->center_y,11,0,0):-1;
}

int fd2_scene_teleport_wipe(fd2_scene_teleport_scope *scope,unsigned row) {
    int result=teleport_owner(scope); if (result) return result;
    int64_t dx=(int64_t)scope->draw_x-scope->game->camera_cell_x;
    int64_t dy=(int64_t)scope->draw_y-scope->game->camera_cell_y;
    if (row>=24) return -1;
    if (!dy && row>=18) return 0;
    int64_t source_x=72+dx*24, source_y=72+dy*24-(dy ? 6:0)+(int)row;
    int64_t x=dx*24+4, y=dy*24+(dy ? -2:4)+(int)row;
    /* 原越界读取不模拟；合法可见caller保持24-byte完整原写。 */
    uint8_t pixels[24];
    if (source_x<0 || source_x>FD2_FIELD_PAGE_STRIDE-24 || source_y<0 ||
        source_y>=FD2_FIELD_PAGE_HEIGHT || fd2_field_page_read(scope->owner,
            (int)source_x,(int)source_y,24,1,pixels,sizeof(pixels),24)) return -1;
    if (y>=0 && y<200) for (int col=0;col<24;col++)
        if (x+col>=0 && x+col<320) scope->vga->framebuffer[(size_t)y*320+(size_t)(x+col)]=pixels[col];
    fd2_vga_upload_current_frame(scope->vga); return teleport_owner(scope);
}

int fd2_scene_teleport_top(fd2_scene_teleport_scope *scope) {
    int result=teleport_owner(scope);
    return result ? result:scope->draw_y==scope->game->camera_cell_y;
}

int fd2_scene_teleport_expand(fd2_scene_teleport_scope *scope,unsigned frame) {
    int result=teleport_owner(scope); if (result) return result;
    if (frame>9) return -1;
    const uint8_t *lut=fd2_field_lut_frame(&scope->game->visuals,frame);
    if (!lut || fd2_field_game_effect_mask_frame(scope->game,scope->vga,scope->base,lut,
            scope->center_x,scope->center_y,11,0,0)) return -1;
    fd2_vga_upload_current_frame(scope->vga); return teleport_owner(scope);
}

void fd2_scene_teleport_release(fd2_scene_teleport_scope **scope) {
    if (!scope || !*scope) return;
    fd2_pcm_bank_close(&(*scope)->sound);
    fd2_field_page_destroy((*scope)->base); free(*scope); *scope=NULL;
}
