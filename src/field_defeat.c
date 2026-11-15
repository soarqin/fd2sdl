/* field_defeated_units_finalize @0x1db65（原登记同名），完整1db65..1debe：
 * 本文件只复现有限页写。13组方向/flags1/25次wait/分支/尾11cac均归冷图；
 * 没有Game副本、RNG或第二个持久页owner。FDOTHER5 LMI68..79经4ebab绘制。 */
#include "field_defeat.h"

#include <stdlib.h>
#include <string.h>

#include "field_background.h"
#include "field_game.h"
#include "field_visual.h"
#include "portrait.h"

enum { DEFEAT_ANCHORS=30, DEFEAT_IMAGES=12 };
struct fd2_field_defeat_visual {
    fd2_image images[DEFEAT_IMAGES];
    fd2_field_page *base;
    fd2_field_page *app; /* 仅借原53A49身份，close绝不释放/替换。 */
    int x[DEFEAT_ANCHORS],y[DEFEAT_ANCHORS];
    size_t count,unit_count;
    int camera_x,camera_y,base_ready;
};

static uint32_t defeat_u32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}

void fd2_field_defeat_close(fd2_field_defeat_visual **visual) {
    if(!visual || !*visual)return;
    for(size_t i=0;i<DEFEAT_IMAGES;i++)fd2_image_free(&(*visual)->images[i]);
    fd2_field_page_destroy((*visual)->base);free(*visual);*visual=NULL;
}

static int defeat_valid(const fd2_field_defeat_visual *v,const fd2_field_game *game,const fd2_vga *vga) {
    return v && game && game->ready && !game->failure && vga && vga->field_page==v->app &&
        game->units.count==v->unit_count && game->camera_cell_x==v->camera_x &&
        game->camera_cell_y==v->camera_y && vga->field_visual.actor_idle_phase<=3 &&
        vga->field_visual.cursor_phase<=3 && vga->actor_jitter_phase<=1;
}

static int defeat_tiles(const fd2_field_game *game,fd2_vga *vga,fd2_field_page *target) {
    /* 1db65的11eee仍按terrain→实际背景→range，分别沿原读取点取钟。 */
    if(fd2_field_visual_terrain_step_live(&vga->field_visual)<0 ||
        fd2_field_background_draw_from_live(vga->field_page,target,72,72,
            game->camera_cell_x,game->camera_cell_y,0,0,1)!=0 ||
        fd2_field_visual_range_step_live(&vga->field_visual,-1)<0)return -1;
    return fd2_terrain_render_page_block(target,&game->terrain,&game->map,72,72,13,8,
        game->camera_cell_x,game->camera_cell_y,(int)vga->field_visual.terrain_phase,
        (uint8_t)vga->field_visual.actor_idle_phase,NULL,NULL);
}

static int defeat_actors(const fd2_field_game *game,fd2_vga *vga,fd2_field_page *target) {
    for(size_t raw=0;raw<game->units.count;raw++) {
        if(fd2_field_unit_is_hidden(&game->units.items[raw]))continue;
        if(fd2_field_actor_visual_tick_live(vga)<0 ||
            fd2_field_units_render_page_actor(target,&game->sprites,&game->units,raw,
                game->camera_cell_x,game->camera_cell_y,(uint8_t)vga->field_visual.actor_idle_phase,
                (uint8_t)vga->field_visual.cursor_phase,vga->actor_jitter_phase,0)!=0)return -1;
    }
    return fd2_field_units_render_page_terrain_overlays(target,&game->terrain,&game->map,
        &game->units,game->camera_cell_x,game->camera_cell_y,(int)vga->field_visual.terrain_phase,NULL,NULL);
}

int fd2_field_defeat_prepare(fd2_field_defeat_visual **visual,
    const fd2_field_game *game,const fd2_vga *vga,const fd2_archive *fdother) {
    if(!visual || *visual || !game || !game->ready || game->failure || !vga ||
        !vga->field_page || !fdother || game->units.count>FD2_FIELD_MAX_UNITS ||
        vga->field_visual.actor_idle_phase>3 || vga->field_visual.cursor_phase>3 ||
        vga->actor_jitter_phase>1)return -1;
    fd2_field_defeat_visual *v=calloc(1,sizeof(*v));
    fd2_field_page *scratch=NULL;
    if(!v)return -1;
    v->app=vga->field_page;v->unit_count=game->units.count;
    v->camera_x=game->camera_cell_x;v->camera_y=game->camera_cell_y;
    for(size_t raw=0;raw<game->units.count;raw++) {
        const fd2_field_unit *u=&game->units.items[raw];
        if(u->hp || fd2_field_unit_is_hidden(u) ||
            (int64_t)u->x<(int64_t)v->camera_x-1 || (int64_t)u->x>(int64_t)v->camera_x+13 ||
            (int64_t)u->y<(int64_t)v->camera_y-1 || (int64_t)u->y>(int64_t)v->camera_y+9)continue;
        /* 当前保留30锚；原第31锚与临时页指针重叠的有效域另归F4，不能按伪C长度判U。 */
        if(v->count<DEFEAT_ANCHORS) {
            v->x[v->count]=72+((int)u->x-v->camera_x-1)*24;
            v->y[v->count]=66+((int)u->y-v->camera_y-1)*24;v->count++;
        }
    }
    if(!v->count)goto failure;
    const uint8_t *data;size_t size;
    if(fd2_archive_get(fdother,5,&data,&size)!=0 || size<6 || memcmp(data,"LMI1",4))goto failure;
    size_t count=(size_t)data[4]|((size_t)data[5]<<8),table=6+4*count;
    if(count<80 || table>size)goto failure;
    for(size_t i=0;i<DEFEAT_IMAGES;i++) {
        size_t index=68+i;
        uint32_t a=defeat_u32(data+6+4*index),b=index+1<count?defeat_u32(data+10+4*index):(uint32_t)size;
        if(a<table || a>=b || b>size || fd2_lmi_decode_image(&v->images[i],data+a,b-a)!=0)goto failure;
    }
    v->base=fd2_field_page_create();scratch=fd2_field_page_create();
    if(!v->base || !scratch)goto failure;
    /* PREPARE只在临时页校验原4个旋转方向/地形相位；不提前提交方向。
     * 这是短寿命资源投影，不是第二个业务owner。 */
    fd2_field_units projected=game->units;
    for(unsigned direction=0;direction<4;direction++) {
        for(size_t raw=0;raw<projected.count;raw++)
            if(!projected.items[raw].hp && !fd2_field_unit_is_hidden(&projected.items[raw]))
                projected.items[raw].direction=(uint8_t)direction;
        if(fd2_field_units_render_page(scratch,&game->sprites,&projected,v->camera_x,v->camera_y,
            (uint8_t)vga->field_visual.actor_idle_phase,(uint8_t)vga->field_visual.cursor_phase,
            vga->actor_jitter_phase)!=0)goto failure;
    }
    for(unsigned phase=0;phase<2;phase++)
        if(fd2_field_background_draw_from_live(vga->field_page,scratch,72,72,v->camera_x,v->camera_y,
            0,0,0)!=0 ||
            fd2_terrain_render_page_block(scratch,&game->terrain,&game->map,72,72,13,8,
                v->camera_x,v->camera_y,(int)phase,(uint8_t)vga->field_visual.actor_idle_phase,NULL,NULL)!=0)goto failure;
    fd2_field_page_destroy(scratch);*visual=v;return 0;
failure:
    fd2_field_page_destroy(scratch);fd2_field_defeat_close(&v);return -1;
}

int fd2_field_defeat_rotate(fd2_field_defeat_visual *v,const fd2_field_game *game,fd2_vga *vga) {
    if(!defeat_valid(v,game,vga))return -1;
    if(fd2_input_take_quit(&vga->input))return 1;
    return defeat_tiles(game,vga,vga->field_page);
}

int fd2_field_defeat_actor(fd2_field_defeat_visual *v,const fd2_field_game *game,fd2_vga *vga,size_t raw) {
    if(!defeat_valid(v,game,vga) || raw>=game->units.count)return -1;
    if(fd2_input_take_quit(&vga->input))return 1;
    if(fd2_field_unit_is_hidden(&game->units.items[raw]))return 0;
    if(fd2_field_actor_visual_tick_live(vga)<0)return -1;
    return fd2_field_units_render_page_actor(vga->field_page,&game->sprites,&game->units,raw,
        game->camera_cell_x,game->camera_cell_y,(uint8_t)vga->field_visual.actor_idle_phase,
        (uint8_t)vga->field_visual.cursor_phase,vga->actor_jitter_phase,0);
}

int fd2_field_defeat_project(fd2_field_defeat_visual *v,const fd2_field_game *game,fd2_vga *vga) {
    if(!defeat_valid(v,game,vga))return -1;
    if(fd2_input_take_quit(&vga->input))return 1;
    if(fd2_field_units_render_page_terrain_overlays(vga->field_page,&game->terrain,&game->map,
        &game->units,game->camera_cell_x,game->camera_cell_y,(int)vga->field_visual.terrain_phase,NULL,NULL)!=0)return -1;
    return fd2_field_page_project(vga->field_page,72,72,vga->framebuffer,sizeof(vga->framebuffer));
}

int fd2_field_defeat_base(fd2_field_defeat_visual *v,const fd2_field_game *game,fd2_vga *vga) {
    if(!defeat_valid(v,game,vga) || v->base_ready)return -1;
    if(fd2_input_take_quit(&vga->input))return 1;
    for(size_t raw=0;raw<game->units.count;raw++)
        if(!game->units.items[raw].hp && game->units.items[raw].flags!=1)return -1;
    if(defeat_tiles(game,vga,v->base)!=0 || defeat_actors(game,vga,v->base)!=0)return -1;
    v->base_ready=1;return 0;
}

/* 原4ebab：颜色0跳过，LMI68..79最大72x71且原锚落在456x336内。
 * 有界裁剪仅保护宿主owner，不能拿VGA像素造透明底。 */
static int defeat_image(fd2_field_page *page,const fd2_image *image,int x,int y) {
    int left=x<0?-x:0,right=image->width;
    if(right>FD2_FIELD_PAGE_STRIDE-x)right=FD2_FIELD_PAGE_STRIDE-x;
    if(left>=right)return 0;
    uint8_t row[FD2_FIELD_PAGE_STRIDE];
    for(int sy=0;sy<image->height;sy++) {
        int dy=y+sy;if(dy<0 || dy>=FD2_FIELD_PAGE_HEIGHT)continue;
        size_t width=(size_t)(right-left);
        if(fd2_field_page_read(page,x+left,dy,(int)width,1,row,sizeof(row),width)!=0)return -1;
        for(int sx=left;sx<right;sx++) {
            uint8_t color=image->pixels[(size_t)sy*(size_t)image->width+(size_t)sx];
            if(color)row[sx-left]=color;
        }
        if(fd2_field_page_write(page,x+left,dy,(int)width,1,row,sizeof(row),width)!=0)return -1;
    }
    return 0;
}

int fd2_field_defeat_frame(fd2_field_defeat_visual *v,const fd2_field_game *game,fd2_vga *vga,unsigned frame) {
    if(!defeat_valid(v,game,vga) || !v->base_ready || frame<68 || frame>79)return -1;
    if(fd2_input_take_quit(&vga->input))return 1;
    if(frame>=74 && fd2_field_page_copy_pixels(vga->field_page,v->base)!=0)return -1;
    for(size_t i=0;i<v->count;i++)
        if(defeat_image(vga->field_page,&v->images[frame-68],v->x[i],v->y[i])!=0)return -1;
    return fd2_field_page_project(vga->field_page,72,72,vga->framebuffer,sizeof(vga->framebuffer));
}
