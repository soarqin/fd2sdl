/* B6物理同步演出：sub_2E2B0 @0x2e2b0、sub_2EBE1 @0x2ebe1、
 * sub_2E9A8 @0x2e9a8、sub_2EB9F @0x2eb9f、sub_2F4D4 @0x2f4d4、
 * sub_2F631 @0x2f631。影像目录由figani解析；业务与RNG不在本文件执行。
 * G2：field_derived_magic_full_execute @0x2cf30（原sub_2CF30）、
 * field_composite_magic_full_execute @0x2d80d（原sub_2D80D）的只读重放。
 * field_item_use_effect_execute @0x20c6f（原sub_20C6F）及
 * field_item_permanent_stat_apply @0x21082（原sub_21082）复用地图helper，
 * Item浮字由明确POPUP_GROUP在重算／压槽前显示，不借Magic专用wrapper。
 * F2：field_map_magic_cast_flash @0x1d6c8（原sub_1D6C8）及
 * field_ai_ray_item_cast_present @0x2dfc8（原sub_2DFC8）；后者的15055
 * caller仍在同演出叶内衔接2189a(80,-4)，实际20c6f费用/效果留给Game。
 * SHORT：field_physical_exchange @0x1548e（原同名）、field_short_strike_present
 * @0x1e98c（原sub_1E98C）、field_short_counter_refresh @0x1e611（原sub_1E611）；
 * field_stationary_recover_present_apply @0x13fd4（原sub_13FD4）。顺序由Game冷图调度，
 * 本服务只消费已提交结果／明确的恢复STEP，不取得业务写权或RNG。
 * 表VA0x52470/0x5266c/0x52684/0x526e3已由只读Domain与relocated字节核对。
 * G3规范命名（完整Domain函数/xref/原资源依据，非ABI推断）：
 * field_promotion_visual_execute @0x2fb2c（原登记 sub_2FB2C）。
 * field_promotion_idle_cycle @0x2fe14（原登记 sub_2FE14）。
 */
#include "battle_animation.h"
#include "field_render_scope.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_attack.h"
#include "field_audio.h"
#include "field_game.h"
#include "field_info.h"
#include "figani.h"
#include "image.h"
#include "magic_fx.h"
#include "portrait.h"
#include "static_data.h"

#include "static_data/battle_animation.inc"

uint8_t fd2_battle_animation_map_solid_color(void) {
    return k_battle_map_solid_color[0];
}

typedef struct {
    fd2_image image;
    uint8_t *opaque; /* RLE写入掩码；literal 0仍不透明，不能用零像素替代SKIP。 */
} battle_image;
typedef struct {
    fd2_figani_part part;
    battle_image drawing;
} battle_part;
typedef struct {
    fd2_figani_clip clip;
    battle_part *parts;
    fd2_pcm_bank bank;
} battle_clip;

struct fd2_battle_animation_exchange {
    fd2_field_physical_present_mode mode;
    size_t actor[2];
    fd2_field_unit units[2];
    battle_clip idle[2], attack[2];
    battle_image bg[2], transition[3], terrain;
    fd2_vga board; /* 同一summary painter的离屏页，非输入owner。 */
    uint8_t work[FD2_FIELD_PAGE_SIZE]; /* 640x200/400stride与SHORT完整App桥共用。 */
    uint8_t map[64000], palette[768], dac[768];
    int counter_ready;
    int sfx_enabled;
    int started;
    int last_attacker;
    uint8_t idle_part, idle_ticks, shake;
    int tint;
    battle_image map_fx[49];
    fd2_pcm_bank map_sound; /* 仅目录自有；sample bytes仍借App FDOTHER。 */
    uint8_t sprite_ids[2], fx_offsets[2];
    int panel_x[2], panel_y[2];
    int map_ready, panels_ready, counter_refreshed, recovery_with_info;
};

static int lmi_image(battle_image *out,const fd2_archive *archive,size_t entry,size_t frame);
static int refresh_map(const fd2_field_game *game,fd2_battle_animation *ctx,fd2_vga *target);
static int map_physical_prepare(const fd2_field_game *game,
    const fd2_field_physical_event *event,fd2_battle_animation *ctx);

/* 同一次真实11eee/11cac投影借App页；离屏VGA只拥有输出像素。 */
static int battle_field_render(const fd2_field_game *game, fd2_battle_animation *ctx,
    fd2_vga *target, int mode) {
    fd2_field_render_scope scope;
    if (fd2_field_render_scope_begin(&scope,target,ctx->vga,0)!=0) return -1;
    if (mode==0) fd2_field_game_render_background(game,target);
    else if (mode==1) fd2_field_game_render_effect_background(game,target);
    else fd2_field_game_render_effect_tiles(game,target);
    fd2_field_render_scope_end(&scope);
    return 0;
}

/* 2e2b0/2cf30正常尾、2d80d在MAP组之前的11cac(1)：非零原参只省water，
 * selection/info仍消费原Game显示状态，不从FULL或skip强造mode1。
 * 无App页仅保既有离屏诊断快照，不签正式逐读取钟。 */
static int battle_field_return(const fd2_field_game *game,fd2_battle_animation *ctx,
        int physical,int clear_vga) {
    if(ctx->visual_owner!=game)return -1;
    if(clear_vga)memset(ctx->vga->framebuffer,0,64000);
    if(ctx->vga->field_page)
        return fd2_field_game_runtime_refresh(ctx->visual_owner,ctx->vga,3);
    uint64_t now=SDL_GetTicksNS();
    if(fd2_field_actor_visual_tick(ctx->vga,now)<0 ||
        fd2_field_game_tick_visual(ctx->visual_owner,now)!=0)return -1;
    if(physical)fd2_field_game_render_battle_return(game,ctx->vga);
    else fd2_field_game_render_effect_background(game,ctx->vga);
    return 0;
}

static void image_close(battle_image *image) {
    fd2_image_free(&image->image);
    free(image->opaque);
    image->opaque = NULL;
}

static int image_open(battle_image *out, const uint8_t *data, size_t size) {
    if (fd2_image_decode_buf(&out->image, data, size) != 0) return -1;
    size_t count = (size_t)out->image.width * (size_t)out->image.height;
    out->opaque = malloc(count);
    if (!out->opaque) { image_close(out); return -1; }
    memset(out->opaque, 255, count);
    if (fd2_image_decode_rle_pixels(out->opaque, out->image.width,
            out->image.height, data + 4, size - 4) != 0) {
        image_close(out); return -1;
    }
    /* 两种初值解同一RLE：只有SKIP在两页不同，包含值0/255的真实写点均相同。 */
    for (size_t i = 0; i < count; i++)
        out->opaque[i] = (uint8_t)(out->opaque[i] == out->image.pixels[i]);
    return 0;
}

static int image_entry(battle_image *out, const fd2_archive *archive, size_t index) {
    const uint8_t *data; size_t size;
    return fd2_archive_get(archive,index,&data,&size) == 0
        ? image_open(out,data,size) : -1;
}

/* 与4e98d的SKIP/普通值/常色33写点相同；裁剪只保护宿主分屏工作页边界。 */
static void image_blit(uint8_t *dst, int width, int height, int stride,
        const battle_image *src, int x, int y, int tint, const uint8_t *lut) {
    for (int sy = 0; sy < src->image.height; sy++) {
        int dy = y + sy;
        if (dy < 0 || dy >= height) continue;
        for (int sx = 0; sx < src->image.width; sx++) {
            int dx = x + sx;
            size_t i = (size_t)sy * (size_t)src->image.width + (size_t)sx;
            if (dx >= 0 && dx < width && src->opaque[i])
                dst[(size_t)dy * (size_t)stride + (size_t)dx] =
                    lut ? lut[src->image.pixels[i]] : tint < 0 ? src->image.pixels[i] :
                    tint <= 255 ? (uint8_t)tint :
                    (uint8_t)(tint+(((tint>>8)+src->image.pixels[i])&7));
        }
    }
}

static void blit(uint8_t *dst, int width, int height, int stride,
        const battle_image *src, int x, int y, int tint) {
    image_blit(dst,width,height,stride,src,x,y,tint,NULL);
}

/* 4ebab的LMI写集落在456页；opaque0由image_open/lmi_image分别保留，
 * 不借VGA回灌。逐行桥保留透明和视窗外margin，越owner部分仅有界省绘制。 */
static int battle_page_image(fd2_field_page *page,const battle_image *image,
    int x,int y,int tint) {
    if (!page || !image || !image->image.pixels || !image->opaque ||
        image->image.width<=0 || image->image.height<=0) return -1;
    int left=x<0 ? -x:0,right=image->image.width;
    if (right>FD2_FIELD_PAGE_STRIDE-x) right=FD2_FIELD_PAGE_STRIDE-x;
    if (left>=right) return 0;
    uint8_t row[FD2_FIELD_PAGE_STRIDE];
    for (int sy=0;sy<image->image.height;sy++) {
        int dy=y+sy;
        if (dy<0 || dy>=FD2_FIELD_PAGE_HEIGHT) continue;
        size_t width=(size_t)(right-left);
        if (fd2_field_page_read(page,x+left,dy,(int)width,1,row,sizeof(row),width)!=0) return -1;
        for (int sx=left;sx<right;sx++) {
            size_t i=(size_t)sy*(size_t)image->image.width+(size_t)sx;
            if (image->opaque[i]) row[sx-left]=tint<0 ? image->image.pixels[i]:(uint8_t)tint;
        }
        if (fd2_field_page_write(page,x+left,dy,(int)width,1,row,sizeof(row),width)!=0) return -1;
    }
    return 0;
}

static void copy_pixels(uint8_t *dst, int dst_stride, const uint8_t *src, int src_stride) {
    for (size_t y=0;y<200;y++) memcpy(dst+y*(size_t)dst_stride,src+y*(size_t)src_stride,320);
}

static void clip_close(battle_clip *clip) {
    if (clip->parts) {
        for (size_t i = 0; i < clip->clip.part_count; i++)
            image_close(&clip->parts[i].drawing);
    }
    free(clip->parts);
    fd2_pcm_bank_close(&clip->bank);
    memset(clip,0,sizeof(*clip));
}

static int clip_open_visual(battle_clip *out,const fd2_archive *archive,size_t index) {
    if (fd2_figani_open_entry(&out->clip,archive,index) != 0 ||
        out->clip.empty || out->clip.split_mode > 1 ||
        (out->clip.split_mode && out->clip.intro_end > out->clip.part_count)) return -1;
    out->parts=calloc(out->clip.part_count,sizeof(*out->parts));
    if (!out->parts) return -1;
    for (size_t i=0;i<out->clip.part_count;i++) {
        battle_part *p=&out->parts[i];
        if (fd2_figani_get_part(&out->clip,i,&p->part) != 0 ||
            p->part.x+p->part.width>320u || p->part.y+p->part.height>200u ||
            image_open(&p->drawing,p->part.image_data,p->part.image_size) != 0) return -1;
    }
    return 0;
}

static int bank_sample_valid(const fd2_pcm_bank *bank,size_t index) {
    const uint8_t *sample;size_t length;
    return fd2_archive_get(&bank->archive,index,&sample,&length)==0 &&
        length && length<=UINT32_MAX;
}

static int clip_open(battle_clip *out, fd2_battle_animation *ctx,
        size_t index, int attack) {
    if (clip_open_visual(out,ctx->figani,index) != 0 || out->clip.bank_selector>6)
        return -1;
    if (attack && out->clip.bank_selector &&
        fd2_pcm_bank_open(&out->bank,ctx->fdother,47u+out->clip.bank_selector) != 0) return -1;
    for (size_t i=0;attack && i<out->clip.part_count;i++) {
        if (out->parts[i].part.cue &&
            (!bank_sample_valid(&out->bank,out->parts[i].part.cue) ||
             !bank_sample_valid(&out->bank,0))) return -1;
    }
    return 0;
}

static void exchange_close(fd2_battle_animation *ctx) {
    fd2_battle_animation_exchange *e=ctx->exchange;
    if (!e) return;
    for (size_t i=0;i<2;i++) {
        clip_close(&e->idle[i]); clip_close(&e->attack[i]); image_close(&e->bg[i]);
    }
    for (size_t i=0;i<3;i++) image_close(&e->transition[i]);
    image_close(&e->terrain);
    for (size_t i=0;i<49;i++) image_close(&e->map_fx[i]);
    fd2_pcm_bank_close(&e->map_sound);
    free(e); ctx->exchange=NULL;
}

int fd2_battle_animation_open(fd2_battle_animation *ctx, fd2_vga *vga,
        fd2_pcm_player *primary, fd2_pcm_player *secondary,
        const fd2_archive *fdother, const fd2_archive *figani,
    struct fd2_field_game *visual_owner) {
    if (!ctx || !vga || !fdother || !figani || !visual_owner || ctx->vga || ctx->exchange || ctx->magic_exchange) return -1;
    ctx->vga=vga;ctx->primary=primary;ctx->secondary=secondary;
    ctx->fdother=fdother;ctx->figani=figani;ctx->visual_owner=visual_owner;
    return 0;
}

/* 只有物理/FULL场景需要BG/TAI；13与未演出的Play不为无关资源失败。 */
static int ensure_backgrounds(fd2_battle_animation *ctx) {
    if (ctx->backgrounds.data && ctx->terrain_images.data) return 0;
    fd2_archive_close(&ctx->backgrounds);fd2_archive_close(&ctx->terrain_images);
    fd2_archive bg={0},terrain={0};
    if (fd2_archive_open(&bg,"original_game/BG.DAT")!=0 ||
        fd2_archive_open(&terrain,"original_game/TAI.DAT")!=0) {
        fd2_archive_close(&bg);fd2_archive_close(&terrain);return -1;
    }
    ctx->backgrounds=bg;ctx->terrain_images=terrain;return 0;
}

void fd2_battle_animation_set_skip(fd2_battle_animation *ctx, int skip) {
    if (ctx) ctx->skip=skip!=0;
}

static int stopped(fd2_battle_animation *ctx) {
    if (fd2_input_take_quit(&ctx->vga->input)) ctx->host_quit=1;
    return ctx->skip || ctx->host_quit;
}

static void present(fd2_battle_animation *ctx, uint32_t ticks) {
    if (stopped(ctx)) return;
    /* 原有限帧只等自身ticks/ms；保宿主事件泵，不履行外来deadline。 */
    fd2_input_begin_frame(&ctx->vga->input);
    fd2_vga_upload_current_frame(ctx->vga);
    if (!stopped(ctx) && ticks) fd2_wait_ticks(ticks);
    (void)stopped(ctx);
}

static int play_cue(fd2_battle_animation *ctx,const fd2_pcm_bank *bank,int enabled,uint8_t index) {
    if (stopped(ctx) || !ctx->primary || !enabled) return 0;
    return fd2_pcm_play_replace(ctx->primary,bank,index,1,FD2_GAME_SFX_DEFAULT_GAIN);
}

static int cue(fd2_battle_animation *ctx, const battle_clip *clip, uint8_t index) {
    return play_cue(ctx,&clip->bank,ctx->exchange->sfx_enabled,index);
}

static void part_draw(uint8_t *dst, int width, int height, int stride,
        const battle_clip *clip, size_t index, int x, int y, int tint) {
    const battle_part *p=&clip->parts[index];
    blit(dst,width,height,stride,&p->drawing,x+p->part.x,y+p->part.y,tint);
}

/* field_battle_unit_summary_draw @0x2facd（原sub_2FACD）：原raw index
 * 与当前stage决定唯一bottom override，不能改为unit_id或从side猜raw。 */
static int battle_summary_draw(fd2_vga *board,const fd2_field_game *game,
        const fd2_field_unit *unit,size_t raw_index) {
    int bottom=!unit->side || (game->stage==k_battle_summary_bottom_override[0] &&
        raw_index==k_battle_summary_bottom_override[1]);
    return fd2_field_unit_summary_draw_at(board,&game->info_assets,&game->font,
        &game->ui_text,unit,bottom?0:171,bottom?154:4);
}

static int summary(fd2_battle_animation_exchange *e,const fd2_field_game *game,
        const fd2_field_unit *unit,size_t raw_index) {
    return battle_summary_draw(&e->board,game,unit,raw_index);
}

static int scene_index(const fd2_field_game *game, const fd2_field_unit *unit,
        size_t *out) {
    if (!game->map.cells || unit->x >= game->map.width || unit->y >= game->map.height)
        return -1;
    const fd2_terrain_attr *attr=fd2_terrain_attr_get(&game->terrain,
        fd2_field_cell_terrain(fd2_field_map_cell(&game->map,unit->x,unit->y)));
    if (!attr) return -1;
    /* 2e2b0/52470：免地形按原stage表；其余读12e38输出+6(attr2)。 */
    if (game->stage>=sizeof(k_battle_default_scene)) return -1;
    *out=fd2_field_attack_unit_ignores_terrain_modifier(unit)
        ? k_battle_default_scene[game->stage]:attr->attr2;
    return 0;
}

static int prepare(const fd2_field_game *game, const fd2_field_physical_event *event,
        fd2_battle_animation *ctx) {
    if (event->presentation_mode!=FD2_FIELD_PHYSICAL_MODE_FULL)
        return map_physical_prepare(game,event,ctx);
    const uint8_t *palette; size_t size;
    if (ctx->exchange || ctx->magic_exchange || event->actor_index >= game->units.count ||
        event->target_index >= game->units.count || event->actor_index == event->target_index ||
        fd2_archive_get(ctx->fdother,0,&palette,&size) != 0 || size<768) return -1;
    if (ensure_backgrounds(ctx)!=0) return -1;
    fd2_battle_animation_exchange *e=calloc(1,sizeof(*e));
    if (!e) return -1;
    ctx->exchange=e;
    e->sfx_enabled=game->option_values[1]!=0;
    e->actor[0]=event->actor_index; e->actor[1]=event->target_index;
    e->units[0]=game->units.items[e->actor[0]];
    e->units[1]=game->units.items[e->actor[1]];
    /* AI11在首动作RNG前验证之后可能腾出的格/目标；只有资源probe允许
     * 借入同身份的只读位置投影，真实演出仍以Game为唯一记录owner。 */
    if (event->resource_only) {
        if (event->actor_before) {
            if (event->actor_before->unit_id!=e->units[0].unit_id) goto fail;
            e->units[0]=*event->actor_before;
        }
        if (event->target_before) {
            if (event->target_before->unit_id!=e->units[1].unit_id) goto fail;
            e->units[1]=*event->target_before;
        }
    }
    size_t scenes[2];
    for (size_t i=0;i<2;i++) {
        if (e->units[i].side>2 || fd2_field_unit_is_hidden(&e->units[i]) ||
            scene_index(game,&e->units[i],&scenes[i]) != 0 ||
            clip_open(&e->idle[i],ctx,3u*e->units[i].unit_id,0) != 0 ||
            summary(e,game,&e->units[i],e->actor[i]) != 0) goto fail;
    }
    if (clip_open(&e->attack[0],ctx,3u*e->units[0].unit_id+1u,1) != 0) goto fail;
    e->counter_ready=fd2_field_attack_counterattack_is_available(&e->units[1],&e->units[0]);
    if (e->counter_ready &&
        clip_open(&e->attack[1],ctx,3u*e->units[1].unit_id+1u,1) != 0) goto fail;
    /* bg[0]为2e2b0交换后的5413B(目标侧)，bg[1]为54137(攻击侧)。 */
    int split=e->attack[0].clip.split_mode;
    size_t left=e->units[0].side ? 1u:0u;
    if (image_entry(&e->bg[0],&ctx->backgrounds,scenes[split ? 1u:left]) != 0 ||
        (split && image_entry(&e->bg[1],&ctx->backgrounds,scenes[0]) != 0)) goto fail;
    if (split || (e->counter_ready && e->attack[1].clip.split_mode)) {
        if (!e->bg[1].image.pixels && image_entry(&e->bg[1],&ctx->backgrounds,scenes[0]) != 0)
            goto fail;
        for (size_t i=0;i<3;i++)
            if (image_entry(&e->transition[i],&ctx->backgrounds,i) != 0) goto fail;
    }
    size_t right=e->units[0].side ? 0u:1u;
    if (image_entry(&e->terrain,&ctx->terrain_images,scenes[right]) != 0) goto fail;
    memset(e->board.framebuffer,0,sizeof(e->board.framebuffer));
    if (summary(e,game,&e->units[0],e->actor[0]) != 0) goto fail;
    blit(e->board.framebuffer,320,200,320,&e->bg[split ? 1:0],0,50,-1);
    if (!split && summary(e,game,&e->units[1],e->actor[1]) != 0) goto fail;
    memcpy(e->map,ctx->vga->framebuffer,64000);
    memcpy(e->palette,ctx->vga->palette,768); memcpy(e->dac,ctx->vga->dac,768);
    e->last_attacker=-1; e->tint=-1;
    if (event->resource_only) exchange_close(ctx);
    return 0;
fail:
    exchange_close(ctx); return -1;
}

static void copy_view(fd2_battle_animation *ctx, int stride, int offset) {
    const uint8_t *src=ctx->exchange->work+offset;
    for (size_t y=0;y<200;y++) memcpy(ctx->vga->framebuffer+y*320,src+y*(size_t)stride,320);
}

static void copy_board(fd2_battle_animation_exchange *e, int stride, int offset) {
    for (size_t y=0;y<200;y++) memcpy(e->work+offset+y*(size_t)stride,e->board.framebuffer+y*320,320);
}

/* dac_apply_darkness @0x11d40逐通道减暗度；不复用旧VGA比例近似。 */
static void apply_darkness_vga(fd2_vga *vga, int darkness) {
    for (size_t i=0;i<768;i++) {
        uint8_t v=(uint8_t)(vga->palette[i]>darkness ? vga->palette[i]-darkness:0);
        vga->dac[i]=(uint8_t)((v<<2)|(v>>4));
    }
}
static void apply_darkness(fd2_battle_animation *ctx, int darkness) {
    apply_darkness_vga(ctx->vga,darkness);
}

static void fade(fd2_battle_animation *ctx, int in) {
    for (int i=0;i<(in ? 65:64) && !stopped(ctx);i++) {
        apply_darkness(ctx,in ? 64-i:i);
        present(ctx,0);
        if (!stopped(ctx)) fd2_delay_ms(2);
    }
}

static void intro_scene(fd2_battle_animation *ctx,fd2_vga *board,uint8_t *work,
        int side,int split,const battle_clip *attacker,const battle_clip *target,
        const battle_image *terrain) {
    if (!side && !split) blit(board->framebuffer,320,200,320,terrain,164,157,-1);
    for (int i=8;i>=0 && !stopped(ctx);i--) {
        int base=side ? 0:320;
        copy_pixels(work+base,640,board->framebuffer,320);
        if (side && !split) part_draw(work,640,200,640,target,0,0,0,-1);
        if (side) blit(work,640,200,640,terrain,164+10*i,157,-1);
        part_draw(work,640,200,640,attacker,0,base+(side ? 10*i:-10*i),0,-1);
        if (!side && !split) part_draw(work,640,200,640,target,0,320,0,-1);
        copy_pixels(ctx->vga->framebuffer,320,work+base,640);
        apply_darkness(ctx,6*i);present(ctx,0);
    }
    if (side) blit(board->framebuffer,320,200,320,terrain,164,157,-1);
}

static void intro(fd2_battle_animation *ctx) {
    fd2_battle_animation_exchange *e=ctx->exchange;
    intro_scene(ctx,&e->board,e->work,e->units[0].side!=0,e->attack[0].clip.split_mode,
        &e->idle[0],&e->idle[1],&e->terrain);
}

/* 2f4d4/2f631的640stride双页平移；调用点决定当前/目标侧，不改任何actor。 */
static int scene_pan(fd2_battle_animation *ctx,const fd2_field_game *game,
        fd2_vga *board,uint8_t *work,const battle_image transition[3],
        const battle_image *terrain,const fd2_field_unit *unit,size_t raw_index,
        const battle_clip *idle,const battle_image *bg,int right) {
    for (int n=right ? 1:9; right ? n<10:n>=0; n+=right ? 1:-1) {
        if (stopped(ctx)) return 0;
        blit(work,640,200,640,&transition[n%3],right ? 320:0,50,-1);
        copy_pixels(ctx->vga->framebuffer,320,work+32*n,640);present(ctx,0);
    }
    memset(work,0,128000);memset(board->framebuffer,0,64000);
    blit(board->framebuffer,320,200,320,bg,0,50,-1);
    if (right) blit(board->framebuffer,320,200,320,terrain,164,157,-1);
    if(battle_summary_draw(board,game,unit,raw_index)!=0)return -1;
    copy_pixels(work+(right ? 320:0),640,board->framebuffer,320);
    part_draw(work,640,200,640,idle,0,right ? 320:0,0,-1);
    for (int n=right ? 1:9; right ? n<=10:n>=0; n+=right ? 1:-1) {
        if (stopped(ctx)) return 0;
        int overlay=right ? (n+1)%3:(n+2)%3;
        blit(work,640,200,640,&transition[overlay],right ? 0:320,50,-1);
        copy_pixels(ctx->vga->framebuffer,320,work+32*n,640);present(ctx,0);
    }
    return 0;
}

static int pan(fd2_battle_animation *ctx,const fd2_field_game *game,
        int actor,const battle_clip *idle,const battle_image *bg,int right) {
    fd2_battle_animation_exchange *e=ctx->exchange;
    return scene_pan(ctx,game,&e->board,e->work,e->transition,&e->terrain,
        &e->units[actor],e->actor[actor],idle,bg,right);
}

static void flash(fd2_battle_animation *ctx, uint8_t r, uint8_t g, uint8_t b) {
    if (stopped(ctx)) return;
    ctx->vga->dac[0]=(uint8_t)((r<<2)|(r>>4));
    ctx->vga->dac[1]=(uint8_t)((g<<2)|(g>>4));
    ctx->vga->dac[2]=(uint8_t)((b<<2)|(b>>4)); present(ctx,0);
    if (!stopped(ctx)) fd2_delay_ms(20);
    memset(ctx->vga->dac,0,3);
}

/* 2ebe1的单个non/split绘制tick；同步物理与31529只复用画面，不共享提交。 */
static void full_tick_draw(fd2_battle_animation *ctx,const battle_clip *attack,
        const battle_clip *idle,size_t index,int side) {
    fd2_battle_animation_exchange *e=ctx->exchange;
    const fd2_figani_part *part=&attack->parts[index].part;
    int dx=shake_x[e->shake],dy=attack->clip.split_mode?0:shake_y[e->shake];
    copy_board(e,400,16040);
    int first=side?(part->layer_flags&1):!(part->layer_flags&1);
    if(first)part_draw(e->work,400,320,400,attack,index,40,40,-1);
    part_draw(e->work,400,320,400,idle,e->idle_part,
        40+(side?-dx:dx),40+(side?-dy:dy),e->tint);
    if(!first)part_draw(e->work,400,320,400,attack,index,40,40,-1);
    copy_view(ctx,400,16040);
}
static void full_tick_advance(fd2_battle_animation_exchange *e,const battle_clip *idle) {
    if(++e->idle_ticks==idle->parts[e->idle_part].part.duration) {
        e->idle_ticks=0;
        if(++e->idle_part==idle->clip.part_count)e->idle_part=0;
    }
    if(e->shake)e->shake--;
    e->tint=-1;
}

static int strike(const fd2_field_game *game, const fd2_field_physical_event *event,
        fd2_battle_animation *ctx) {
    fd2_battle_animation_exchange *e=ctx->exchange;
    int actor=event->counterattack ? 1:0, target=1-actor;
    if (!e || !event->actor_before || !event->target_before || !event->result ||
        e->actor[actor]!=event->actor_index || e->actor[target]!=event->target_index ||
        (actor && !e->counter_ready) ||
        event->actor_before->unit_id!=e->units[actor].unit_id ||
        event->target_before->unit_id!=e->units[target].unit_id) return -1;
    e->units[actor]=*event->actor_before; e->units[target]=*event->target_before;
    if (stopped(ctx)) return 0;
    if (!e->started) {
        e->started=1; fade(ctx,0);
        const uint8_t *palette; size_t size;
        if (fd2_archive_get(ctx->fdother,0,&palette,&size) != 0 || size<768) return -1;
        fd2_vga_set_palette(ctx->vga,palette); intro(ctx);
    }
    const battle_clip *attack=&e->attack[actor], *idle=&e->idle[target];
    int side=event->actor_before->side!=0;
    if (e->last_attacker==actor && attack->clip.split_mode==1) {
        memset(e->work,0,sizeof(e->work));
        copy_board(e,640,side ? 0:320);
        part_draw(e->work,640,200,640,idle,0,side ? 0:320,0,-1);
        if (pan(ctx,game,actor,attack,&e->bg[1],side) != 0) return -1;
    }
    if (e->last_attacker!=actor) { e->idle_part=0; e->idle_ticks=0; e->shake=0; e->tint=-1; }
    e->last_attacker=actor;
    size_t begin=0;
    if (attack->clip.split_mode) {
        memset(e->work,0,sizeof(e->work));
        for (;begin<attack->clip.intro_end;begin++) {
            if (stopped(ctx)) return 0;
            const fd2_figani_part *p=&attack->parts[begin].part;
            if (p->cue && cue(ctx,attack,p->cue) != 0) return -1;
            copy_board(e,640,side ? 320:0);
            part_draw(e->work,640,200,640,attack,begin,side ? 320:0,0,-1);
            copy_view(ctx,640,side ? 320:0); present(ctx,p->duration);
        }
        if (pan(ctx,game,target,idle,&e->bg[0],!side) != 0) return -1;
    }
    size_t markers=0,seen=0;
    for (size_t i=0;i<attack->clip.part_count;i++)
        if (attack->parts[i].part.effect_marker) markers++;
    if (!markers) markers=1;
    for (size_t i=begin;i<attack->clip.part_count;i++) {
        if (stopped(ctx)) return 0;
        const fd2_figani_part *p=&attack->parts[i].part;
        if (p->effect_marker) {
            seen++;
            /* 原2ebe1用uncapped raw damage逐标记扣HP；只改本服务的80B影子。 */
            uint64_t loss=(uint64_t)event->result->rolled_damage*seen/markers;
            e->units[target].hp=loss>=event->target_before->hp ? 0:
                (uint16_t)(event->target_before->hp-loss);
            if (summary(e,game,&e->units[target],e->actor[target]) != 0) return -1;
            if (p->cue && cue(ctx,attack,event->result->hit ? p->cue:0) != 0) return -1;
            if (event->result->hit) { e->shake=5; e->tint=33; }
        } else if (p->cue && cue(ctx,attack,p->cue) != 0) return -1;
        for (unsigned tick=0;tick<p->duration;tick++) {
            if (stopped(ctx)) return 0;
            full_tick_draw(ctx,attack,idle,i,side);
            if (p->effect_marker==1 && seen==markers) {
                if (event->pre_hit_status_applied) flash(ctx,1,32,0);
                if (event->result->critical) {
                    flash(ctx,63,63,63);
                    present(ctx,0); /* 原DAC黑写须先上传，不能让白纹理覆盖后40ms。 */
                    if (!stopped(ctx)) fd2_delay_ms(40);
                }
            }
            present(ctx,1);
            full_tick_advance(e,idle);
        }
    }
    return 0;
}

enum {
    SHORT_BANK, RECOVERY_BANK, SHORT_SPRITE_STRIDE, SHORT_PANEL_FIRST,
    SHORT_PANEL_COUNT, SHORT_PANEL_MS, SHORT_FX_MS, SHORT_BAR_MS, SHORT_COUNTER_WIDTH
};

/* sub_4E87D @0x4e87d（原同名）：装备item[0]选择视觉序列，
 * 与决定二击/暴击的item[9]分属不同字段。 */
static int short_weapon_offset(const fd2_field_unit *unit,uint8_t *offset) {
    const uint8_t *raw=(const uint8_t *)unit;
    for (size_t i=0;i<8;i++) {
        size_t at=10u+2u*i;
        if ((raw[at]&0x40u) && raw[at+1]<128u) {
            const uint8_t *item=fd2_field_item_record_get(raw[at+1]);
            if (!item || item[0]>=sizeof(k_battle_short_fx_offsets)) return -1;
            *offset=k_battle_short_fx_offsets[item[0]];
            return 0;
        }
    }
    return -1;
}

static int map_physical_prepare(const fd2_field_game *game,
        const fd2_field_physical_event *event,fd2_battle_animation *ctx) {
    int recovery=event->presentation_mode==FD2_FIELD_PHYSICAL_MODE_RECOVERY;
    if ((!recovery && event->presentation_mode!=FD2_FIELD_PHYSICAL_MODE_SHORT) ||
        ctx->exchange || ctx->magic_exchange || event->actor_index>=game->units.count ||
        event->target_index>=game->units.count ||
        (recovery ? event->actor_index!=event->target_index : event->actor_index==event->target_index) ||
        game->idle_phase>3u || !game->sprites.decoded_frames) return -1;
    fd2_battle_animation_exchange *e=calloc(1,sizeof(*e));
    if (!e) return -1;
    ctx->exchange=e;e->mode=event->presentation_mode;
    e->sfx_enabled=game->option_values[1]!=0;
    e->actor[0]=event->actor_index;e->actor[1]=event->target_index;
    for (size_t i=0;i<(recovery ? 1u:2u);i++) {
        size_t raw=e->actor[i],sprite=fd2_field_unit_cached_sprite_id(&game->units,raw);
        e->units[i]=game->units.items[raw];
        const fd2_field_unit *projected=i ? event->target_before:event->actor_before;
        if (event->resource_only && projected) {
            if (projected->unit_id!=e->units[i].unit_id) goto fail;
            e->units[i]=*projected;
        }
        if (sprite>UINT8_MAX || sprite*12u+11u>=game->sprites.frame_count ||
            fd2_field_unit_is_hidden(&e->units[i]) ||
            game->units.walk_frames[raw]>3u || (!recovery && !e->units[i].hp_max)) goto fail;
        /* SHORT首资源门在face/reset前，actor旧方向不决定随后动作帧。
         * 目标在SHORT_MAP、恢复单位在1da16仍保留旧方向；script24的raw5/零步
         * 按TMP下一cache class取3..5帧，需原cache owner能明确解析。
         * helper只取phase0基址，之后独立加相位，避免其普通raw26规则混入1da16。 */
        if ((recovery || i==1u) && e->units[i].direction>3u) {
            size_t frame=fd2_field_unit_cached_frame_index(&game->units,raw,0);
            if (e->units[i].direction!=5u || e->units[i].frame_phase!=0 ||
                frame==SIZE_MAX || frame+2u>=game->sprites.frame_count) goto fail;
        }
        e->sprite_ids[i]=(uint8_t)sprite;
    }
    /* 13fd4现有caller仅18d8c零步WAIT与13a9f AI：前者19077..19094
     * 不写51AAC，后者处于13565/16f55整轮抑制域。只记录同步caller上下文，
     * 不新建全局51AAC owner；新增caller必须复审该区分和51AAB选项门。 */
    e->recovery_with_info=recovery && game->option_values[3]!=0 &&
        game->active_side==2u && game->selected_unit==(int)event->actor_index &&
        game->interaction==FD2_FIELD_INTERACTION_COMMAND && e->units[0].side==2u;
    size_t bank=k_battle_map_action_parameters[recovery ? RECOVERY_BANK:SHORT_BANK];
    if (fd2_pcm_bank_open(&e->map_sound,ctx->fdother,bank)!=0 ||
        !bank_sample_valid(&e->map_sound,4)) goto fail;
    if (!recovery) {
        e->counter_ready=fd2_field_attack_counterattack_is_available(&e->units[1],&e->units[0]);
        for (size_t i=0;i<2;i++) {
            if (i && !e->counter_ready) continue;
            if (short_weapon_offset(&e->units[i],&e->fx_offsets[i])!=0) goto fail;
            size_t at=e->fx_offsets[i];
            if (at>=sizeof(k_battle_short_fx_sequences)) goto fail;
            size_t count=k_battle_short_fx_sequences[at++];
            if (!count || at+2u*count>sizeof(k_battle_short_fx_sequences)) goto fail;
            for (size_t j=0;j<count;j++,at+=2) {
                size_t frame=k_battle_short_fx_sequences[at];
                uint8_t sample=k_battle_short_fx_sequences[at+1];
                if (frame>=k_battle_map_action_parameters[SHORT_PANEL_FIRST] ||
                    (!e->map_fx[frame].image.pixels &&
                     lmi_image(&e->map_fx[frame],ctx->fdother,6,frame)!=0) ||
                    (sample!=255 && !bank_sample_valid(&e->map_sound,sample))) goto fail;
            }
        }
        size_t first=k_battle_map_action_parameters[SHORT_PANEL_FIRST];
        size_t end=first+k_battle_map_action_parameters[SHORT_PANEL_COUNT];
        if (end>49 || end<=first) goto fail;
        for (size_t i=first;i<end;i++)
            if (lmi_image(&e->map_fx[i],ctx->fdother,6,i)!=0) goto fail;
        /* 借用既有raw 1x5列图：FDOTHER5 frames23/24/25/29/30，
         * 全像素（含0）均写，不套LMI透明或宿主矩形。 */
        const fd2_field_info_assets *a=&game->info_assets;
        if (!a->ready) goto fail;
        for (size_t i=0;i<8;i++) {
            if (i==1 || i==4 || i==5) continue;
            if (!a->bars[i].pixels || a->bars[i].width!=1 || a->bars[i].height!=5) goto fail;
        }
    }
    memcpy(e->map,ctx->vga->framebuffer,64000);
    memcpy(e->palette,ctx->vga->palette,768);memcpy(e->dac,ctx->vga->dac,768);
    if (event->resource_only) exchange_close(ctx);
    return 0;
fail:
    exchange_close(ctx);return -1;
}

/* map_actor_blit_24x24 @0x1da16（原同名）专用投影：raw26/acted不改帧；
 * idle/walk相位3→1，缓存身份和四方向偏移均保留。原动态stride低byte
 * 决定SOLID色：SHORT直VGA320→64，恢复地图buffer456→200。 */
static int map_physical_actor(const fd2_field_game *game,fd2_battle_animation_exchange *e,
        uint8_t *pixels,int actor,int solid,int viewport,fd2_field_page *page,
        unsigned cursor_phase) {
    const fd2_field_unit *u=&e->units[actor];
    size_t raw=e->actor[actor];
    /* 1dac1..1dadf读共享53C07，不能借移动脚本的局部walk_frames。 */
    unsigned phase=u->frame_phase ? cursor_phase:game->idle_phase;
    if (phase>3u) return -1;
    if (phase==3u) phase=1;
    size_t frame;
    if (u->direction<=3u) frame=(size_t)e->sprite_ids[actor]*12u+3u*u->direction+phase;
    else {
        /* 原1dadf..1daf6用cacheClass*12+direction*3，不是unitID+1。
         * raw5/frame0跨到下一TMP class的3号帧；默认位移方向为右，零步不偏移。
         * 不开放未有来源的其它跨cache方向/步态，也不把5按&3重解释。 */
        frame=fd2_field_unit_cached_frame_index(&game->units,raw,0);
        if (u->direction!=5u || u->frame_phase!=0 || frame==SIZE_MAX) return -1;
        frame+=phase;
    }
    if (!game->sprites.decoded_frames || frame>=game->sprites.frame_count) return -1;
    int x=((int)u->x-game->camera_cell_x)*24;
    int y=((int)u->y-game->camera_cell_y)*24-6;
    if (u->x<game->camera_cell_x-1 || u->x>game->camera_cell_x+13 ||
        u->y<game->camera_cell_y-1 || u->y>game->camera_cell_y+9) return 0;
    int step=4*(int)u->frame_phase;
    switch (u->direction) {case 0:y+=step;break;case 1:x-=step;break;
        case 2:y-=step;break;default:x+=step;break;}
    if (page) {
        const uint8_t *rle;size_t length;uint8_t lut[256];
        if (fd2_map_sprite_frame_source(&game->sprites,frame,&rle,&length)!=0) return -1;
        for (unsigned i=0;i<256;i++) lut[i]=solid ? 200:(uint8_t)i;
        /* 1da16不作逻辑列裁边；同一App页内的跨行写仍有定义。 */
        return fd2_field_page_blit_rle_linear(page,(int64_t)(72+y)*456+72+x,
            24,24,rle,length,lut);
    }
    uint8_t *source=game->sprites.decoded_frames+frame*576u;
    /* 已证FDICON所有1680帧无opaque0，decoded0恰为RLE SKIP。 */
    battle_image image={{24,24,source},source};
    int tint=solid ? (viewport ? fd2_battle_animation_map_solid_color():
        (int)(k_battle_map_action_parameters[SHORT_SPRITE_STRIDE]&255u)):-1;
    if (viewport) blit(pixels+1284,312,192,320,&image,x,y,tint);
    else blit(pixels,320,200,320,&image,x+4,y+4,tint);
    return 0;
}

/* field_short_panels_layout @0x1ec2a（原sub_1EC2A）：按朝向/边界选原面板位置。 */
static void short_panel_position(const fd2_field_game *game,const fd2_field_unit *u,
        int *x,int *y) {
    *x=((int)u->x-game->camera_cell_x)*24+4;
    *y=((int)u->y-game->camera_cell_y)*24;
    if (u->direction<2u) {
        *y+=*y>=18 ? -18:5;
        *x+=*x+108>319 ? -86:28;
    } else {
        *y+=*y+37<=199 ? 22:5;
        *x+=*x-86<0 ? 28:-88;
    }
}

/* 1e739→1685c共享线性1×5逐列写；VGA与App只借不同size/stride。
 * 首次越owner即停，保前列及当前列已写行；不按逻辑矩形裁掉有效前缀。 */
static int short_bar_column(const fd2_field_info_assets *assets,uint8_t *dst,
        size_t size,int stride,size_t image,int64_t x,int64_t y) {
    for(int row=0;row<5;row++) {
        int64_t at=(y+row)*stride+x;
        if(at<0 || (uint64_t)at>=size)return -1;
        dst[(size_t)at]=assets->bars[image].pixels[row];
    }
    return 0;
}

/* field_short_hp_bar_draw @0x1e739（原sub_1E739）：空条70列，
 * 正宽度有左/中/右帽，width<70再填到第70列。RAW列含0也写。 */
static int short_bar_raw(const fd2_field_game *game,uint8_t *dst,size_t size,
        int stride,int x,int y,int width) {
    const fd2_field_info_assets *a=&game->info_assets;
    if(width<=0) {
        for(int col=0;col<69;col++)if(short_bar_column(a,dst,size,stride,6,(int64_t)x+col,y))return -1;
        return short_bar_column(a,dst,size,stride,7,(int64_t)x+69,y);
    }
    if(short_bar_column(a,dst,size,stride,0,x,y))return -1;
    for(int col=1;col<width;col++)if(short_bar_column(a,dst,size,stride,2,(int64_t)x+col,y))return -1;
    if(short_bar_column(a,dst,size,stride,3,(int64_t)x+width,y))return -1;
    if(width<70) {
        for(int col=width+1;col<=69;col++)if(short_bar_column(a,dst,size,stride,6,(int64_t)x+col,y))return -1;
        return short_bar_column(a,dst,size,stride,7,(int64_t)x+70,y);
    }
    return 0;
}

static int short_bar(const fd2_field_game *game,uint8_t *dst,int x,int y,int width) {
    return short_bar_raw(game,dst,64000,320,x,y,width);
}

/* field_short_hp_bar_scaled @0x1e7f6（原sub_1E7F6）：HP0不画，否则69比例+1。 */
static int short_unit_bar(const fd2_field_game *game,fd2_battle_animation_exchange *e,
        uint8_t *pixels,int panel,const fd2_field_unit *unit) {
    return unit->hp?short_bar(game,pixels,e->panel_x[panel]+7,
        e->panel_y[panel]+6,(int)(69u*(uint32_t)unit->hp/unit->hp_max+1u)):0;
}

static void restore_image_rect(uint8_t *dst,const uint8_t *saved,const battle_image *image,
        int x,int y) {
    int left=x<0 ? 0:x,top=y<0 ? 0:y;
    int right=x+image->image.width,bottom=y+image->image.height;
    if (right>320) right=320;if (bottom>200) bottom=200;
    if (right<=left || bottom<=top) return;
    for (int row=top;row<bottom;row++)
        memcpy(dst+row*320+left,saved+row*320+left,(size_t)(right-left));
}

static void map_present_ms(fd2_battle_animation *ctx,uint32_t delay) {
    present(ctx,0);
    if (!stopped(ctx) && delay) fd2_delay_ms(delay);
    (void)stopped(ctx);
}

static int short_map(const fd2_field_game *game,fd2_battle_animation *ctx) {
    fd2_battle_animation_exchange *e=ctx->exchange;
    if (!e || e->mode!=FD2_FIELD_PHYSICAL_MODE_SHORT || e->map_ready) return -1;
    e->map_ready=1;e->started=1;
    if (stopped(ctx)) return 0;
    if (refresh_map(game,ctx,&e->board)!=0) return -1;
    memcpy(ctx->vga->framebuffer,e->board.framebuffer,64000);present(ctx,0);
    return 0;
}

/* field_short_panels_present @0x1eb05（原sub_1EB05）：原双save-under及恢复顺序。 */
static int short_begin(const fd2_field_game *game,fd2_battle_animation *ctx) {
    fd2_battle_animation_exchange *e=ctx->exchange;
    if (!e || e->mode!=FD2_FIELD_PHYSICAL_MODE_SHORT || !e->map_ready || e->panels_ready) return -1;
    e->panels_ready=1;
    for (int i=0;i<2;i++) {
        e->units[i]=game->units.items[e->actor[i]];
        short_panel_position(game,&e->units[i],&e->panel_x[i],&e->panel_y[i]);
    }
    if (stopped(ctx)) return 0;
    size_t first=k_battle_map_action_parameters[SHORT_PANEL_FIRST];
    size_t count=k_battle_map_action_parameters[SHORT_PANEL_COUNT];
    for (size_t frame=0;frame<count && !stopped(ctx);frame++) {
        const battle_image *image=&e->map_fx[first+frame];
        memcpy(e->work,ctx->vga->framebuffer,64000);
        blit(ctx->vga->framebuffer,320,200,320,image,e->panel_x[1],e->panel_y[1],-1);
        if (e->counter_ready) {
            memcpy(e->work+64000,ctx->vga->framebuffer,64000);
            blit(ctx->vga->framebuffer,320,200,320,image,e->panel_x[0],e->panel_y[0],-1);
        }
        map_present_ms(ctx,k_battle_map_action_parameters[SHORT_PANEL_MS]);
        if (frame+1<count) {
            restore_image_rect(ctx->vga->framebuffer,e->work,image,e->panel_x[1],e->panel_y[1]);
            if (e->counter_ready)
                restore_image_rect(ctx->vga->framebuffer,e->work+64000,image,e->panel_x[0],e->panel_y[0]);
        }
    }
    if (!stopped(ctx)) {
        /* 1548e调用1e7f6直接写VGA，无wait/flip。前条及当前列有效前缀
         * 立即归真实VGA owner，不能留在board直到两条成功才提交。 */
        if(short_unit_bar(game,e,ctx->vga->framebuffer,1,&e->units[1]))return -1;
        if(e->counter_ready && short_unit_bar(game,e,ctx->vga->framebuffer,0,&e->units[0]))return -1;
        memcpy(e->board.framebuffer,ctx->vga->framebuffer,64000);
    }
    return 0;
}

/* 1e739的raw列按线性目的地址写；HP比例超过70时不截在面板82px内。
 * 只在第一次超出实际App owner时停止，保此前已写前缀。 */
static int short_bar_app(const fd2_field_game *game,uint8_t *pixels,int x,int y,int width) {
    return short_bar_raw(game,pixels,FD2_FIELD_PAGE_SIZE,456,x,y,width);
}

/* 1e611的15f0e/1e739/1e7f6目的仍为53A49；复用现work作App桥。
 * 不把VGA外框回灌App，也不改变原逐面板写入／后投影顺序。 */
static int short_counter_panel(const fd2_field_game *game,fd2_battle_animation *ctx,
        const battle_image *image,int panel,int width) {
    fd2_battle_animation_exchange *e=ctx->exchange;
    int x=e->panel_x[panel],y=e->panel_y[panel];
    if(ctx->vga->field_page) {
        image_blit(e->work,456,336,456,image,x+68,y+68,-1,NULL);
        int result=width<0?0:short_bar_app(game,e->work,x+75,y+74,width);
        if(fd2_field_page_write(ctx->vga->field_page,0,0,456,336,e->work,
            sizeof(e->work),456)!=0)return -1;
        return result;
    }
    blit(e->board.framebuffer,320,200,320,image,x,y,-1);
    return width>=0?short_bar(game,e->board.framebuffer,x+7,y+6,width):0;
}

/* 1e7f6的索引来自1e611实际PUSH位移：1e6a8读第三参target，
 * 1e6fc读第二参actor。两条先后写同一App；HP0保底条/背景，
 * 正HP/max0在原IDIV处停，保此前绘制前缀，不预先拒绝整个刷新。 */
static int short_counter_unit_bar(const fd2_field_game *game,fd2_battle_animation *ctx,
        int panel,const fd2_field_unit *unit) {
    if(!unit->hp)return 0;
    if(!unit->hp_max)return -1;
    int width=(int)(69u*(uint32_t)unit->hp/unit->hp_max+1u);
    fd2_battle_animation_exchange *e=ctx->exchange;
    if(ctx->vga->field_page) {
        int result=short_bar_app(game,e->work,e->panel_x[panel]+75,e->panel_y[panel]+74,width);
        if(fd2_field_page_write(ctx->vga->field_page,0,0,456,336,e->work,
            sizeof(e->work),456)!=0)return -1;
        return result;
    }
    return short_bar(game,e->board.framebuffer,e->panel_x[panel]+7,e->panel_y[panel]+6,width);
}

static int short_counter_refresh(const fd2_field_game *game,fd2_battle_animation *ctx) {
    fd2_battle_animation_exchange *e=ctx->exchange;
    if (!e || e->mode!=FD2_FIELD_PHYSICAL_MODE_SHORT || !e->panels_ready ||
        !e->counter_ready || e->counter_refreshed) return -1;
    e->counter_refreshed=1;
    if (stopped(ctx)) return 0;
    /* 1e640→11eee首部翻53A40地形门，1e648→127a9更新App jitter；
     * 没有11cac/1297d/1acf3，不推进idle/walk或水DAC。 */
    if(ctx->vga->field_page) {
        if(fd2_field_game_runtime_refresh(ctx->visual_owner,ctx->vga,1)!=0)return -1;
        if(fd2_field_page_read(ctx->vga->field_page,0,0,456,336,e->work,
            sizeof(e->work),456)!=0)return -1;
    } else {
        /* 无App工作页的旧诊断只用一个固定时间样本。 */
        uint64_t now=SDL_GetTicksNS();
        if (fd2_field_game_tick_terrain_visual(ctx->visual_owner,now)!=0 ||
            fd2_field_actor_visual_tick(ctx->vga,now)<0) return -1;
        e->board.actor_jitter_tick=ctx->vga->actor_jitter_tick;
        e->board.actor_jitter_phase=ctx->vga->actor_jitter_phase;
        memset(e->board.framebuffer,0,64000);
        if (battle_field_render(game,ctx,&e->board,0)!=0) return -1;
    }
    size_t last=k_battle_map_action_parameters[SHORT_PANEL_FIRST]+
        k_battle_map_action_parameters[SHORT_PANEL_COUNT]-1u;
    if(short_counter_panel(game,ctx,&e->map_fx[last],1,
        k_battle_map_action_parameters[SHORT_COUNTER_WIDTH])!=0)return -1;
    /* 1548e @155f3..15600只需三个实参。1e6a8/1e6fc在先PUSH
     * descriptor后分别读ESP+14h/10h，按实际SP为target/actor，
     * 不能把IDA伪参数名arg_C当作缺第四参。首目标55底条后直接覆比例条，
     * 其间不重画背景；第二actor面板随后覆盖，最后才11d2c投影。 */
    if(short_counter_unit_bar(game,ctx,1,&game->units.items[e->actor[1]])!=0)return -1;
    if(short_counter_panel(game,ctx,&e->map_fx[last],0,-1)!=0)return -1;
    if(short_counter_unit_bar(game,ctx,0,&game->units.items[e->actor[0]])!=0)return -1;
    if(ctx->vga->field_page) {
        if(fd2_field_page_project(ctx->vga->field_page,72,72,ctx->vga->framebuffer,
            sizeof(ctx->vga->framebuffer))!=0)return -1;
        memcpy(e->board.framebuffer,ctx->vga->framebuffer,64000);
    } else memcpy(ctx->vga->framebuffer,e->board.framebuffer,64000);
    present(ctx,0);
    return 0;
}

/* field_short_strike_present @0x1e98c（原sub_1E98C）及1e856条动画，结果已由Game提交。 */
static int short_strike(const fd2_field_game *game,const fd2_field_physical_event *event,
        fd2_battle_animation *ctx) {
    fd2_battle_animation_exchange *e=ctx->exchange;
    int actor=event->counterattack ? 1:0,target=1-actor;
    if (!e || !e->panels_ready || !event->actor_before || !event->target_before || !event->result ||
        e->actor[actor]!=event->actor_index || e->actor[target]!=event->target_index ||
        (actor && !e->counter_refreshed) || !event->target_before->hp_max) return -1;
    e->units[actor]=*event->actor_before;e->units[target]=*event->target_before;
    if (stopped(ctx)) return 0;
    size_t at=e->fx_offsets[actor],count=k_battle_short_fx_sequences[at++];
    int x=((int)e->units[target].x-game->camera_cell_x)*24+4;
    int y=((int)e->units[target].y-game->camera_cell_y)*24;
    for (size_t i=0;i<count && !stopped(ctx);i++,at+=2) {
        const battle_image *image=&e->map_fx[k_battle_short_fx_sequences[at]];
        uint8_t sample=k_battle_short_fx_sequences[at+1];
        if (sample!=255 && play_cue(ctx,&e->map_sound,e->sfx_enabled,
                event->result->hit ? sample:4)!=0) return -1;
        if (event->result->hit && i<2 &&
            map_physical_actor(game,e,ctx->vga->framebuffer,target,i==0,0,NULL,
                ctx->vga->field_visual.cursor_phase)!=0) return -1;
        memcpy(e->work,ctx->vga->framebuffer,64000);
        blit(ctx->vga->framebuffer,320,200,320,image,x,y,-1);
        map_present_ms(ctx,k_battle_map_action_parameters[SHORT_FX_MS]);
        restore_image_rect(ctx->vga->framebuffer,e->work,image,x,y);
    }
    int width=(int)(70u*(uint32_t)event->target_before->hp/event->target_before->hp_max);
    int after=(int)(69u*(uint32_t)event->result->hp_after/event->target_before->hp_max+1u);
    while (width>=after && !stopped(ctx)) {
        if(short_bar(game,ctx->vga->framebuffer,e->panel_x[target]+7,e->panel_y[target]+6,width))return -1;
        map_present_ms(ctx,k_battle_map_action_parameters[SHORT_BAR_MS]);width--;
    }
    if (!stopped(ctx)) present(ctx,0);
    return 0;
}

/* field_stationary_recover_present_apply @0x13fd4（原sub_13FD4）的有限冷STEP。 */
static int recovery_step(const fd2_field_game *game,const fd2_field_physical_event *event,
        fd2_battle_animation *ctx) {
    fd2_battle_animation_exchange *e=ctx->exchange;
    if (!e || e->mode!=FD2_FIELD_PHYSICAL_MODE_RECOVERY ||
        event->actor_index!=e->actor[0] || event->target_index!=e->actor[0]) return -1;
    if (event->recovery_step==FD2_FIELD_RECOVERY_WAIT) {
        if (event->step_value!=1) return -1;
    } else if (event->recovery_step==FD2_FIELD_RECOVERY_CUE) {
        if (event->step_value!=4) return -1;
    } else if (event->recovery_step!=FD2_FIELD_RECOVERY_ACTOR ||
        (event->step_value!=0 && event->step_value!=2)) return -1;
    if (stopped(ctx)) return 0;
    if (!e->started) {
        /* 13fd4的focus已由Game完成；一次只读背景投影，不在三wait间
         * 额外更新idle/walk时钟，因原函数仅直接1da16/copy。 */
        e->board.actor_jitter_tick=ctx->vga->actor_jitter_tick;
        e->board.actor_jitter_phase=ctx->vga->actor_jitter_phase;
        /* 12d7b→12cea即使same-target亦先11cac(0)，1acf3受51AAB&&51AAC。
         * 玩家WAIT保当前信息板；AI整轮51AAC=0须pure。这里只选既有只读
         * mode3/0，不推进共享clock，也不扩G4设置或改变Magic/Item背景。 */
        if (battle_field_render(game,ctx,&e->board,e->recovery_with_info?1:0)!=0) return -1;
        memcpy(ctx->vga->framebuffer,e->board.framebuffer,64000);e->started=1;
    }
    if (event->recovery_step==FD2_FIELD_RECOVERY_WAIT) present(ctx,event->step_value);
    else if (event->recovery_step==FD2_FIELD_RECOVERY_CUE)
        return play_cue(ctx,&e->map_sound,e->sfx_enabled,(uint8_t)event->step_value);
    else {
        e->units[0]=game->units.items[e->actor[0]];
        if (map_physical_actor(game,e,e->board.framebuffer,0,event->step_value==2,1,ctx->vga->field_page,
            ctx->vga->field_visual.cursor_phase)!=0) return -1;
        if (ctx->vga->field_page) {
            if (fd2_field_page_project(ctx->vga->field_page,72,72,ctx->vga->framebuffer,64000)!=0) return -1;
        } else for (size_t y=4;y<196;y++)
            memcpy(ctx->vga->framebuffer+y*320u+4,e->board.framebuffer+y*320u+4,312);
        present(ctx,0);
    }
    return 0;
}

static int map_physical_restore(fd2_battle_animation *ctx,const fd2_field_game *game,int normal) {
    fd2_battle_animation_exchange *e=ctx->exchange;
    (void)game;
    (void)stopped(ctx);
    /* 13fd4的最后一次viewportcopy在第三wait之前；HP/mode写后直接返回。
     * SHORT也没有FULL内部的11cac(1)尾：1548e在collect后/descriptor后
     * 的两次11cac(0)由原caller冷尾驱动，不在本服务FINISH提前插入。
     * 两者均未改palette；正常退出不回滚本轮已到达的water DAC。 */
    if (normal && !ctx->host_quit) {
        exchange_close(ctx);return 0;
    }
    memcpy(ctx->vga->framebuffer,e->map,64000);
    memcpy(ctx->vga->palette,e->palette,768);memcpy(ctx->vga->dac,e->dac,768);
    /* 原SHORT/13fd4没有额外primary stop。此bank仅借FDOTHER sample bytes，
     * close只释放目录，App归档仍长于voice；不为释放目录提前截断cue。 */
    exchange_close(ctx);return 0;
}

static int restore(fd2_battle_animation *ctx, const fd2_field_game *game, int animate) {
    fd2_battle_animation_exchange *e=ctx->exchange;
    if (!e) return -1;
    if (e->mode!=FD2_FIELD_PHYSICAL_MODE_FULL) return map_physical_restore(ctx,game,animate);
    int status=0;
    if (animate && e->started && !stopped(ctx)) {
        if (!e->attack[0].clip.split_mode) {
            copy_board(e,640,0);
            blit(e->work,640,200,640,&e->terrain,164,157,-1);
            part_draw(e->work,640,200,640,&e->idle[1],0,0,0,-1);
            part_draw(e->work,640,200,640,&e->idle[0],0,0,0,-1);
            copy_view(ctx,640,0); present(ctx,6);
        } else if (!stopped(ctx)) fd2_wait_ticks(6);
        fade(ctx,0);
    }
    (void)stopped(ctx);
    memcpy(ctx->vga->palette,e->palette,768);
    if(!animate || !e->started || ctx->skip || ctx->host_quit || status<0)
        memcpy(ctx->vga->dac,e->dac,768);
    /* 2e2b0的11cac(1)重画当前地图，不能复显PREPARE前TARGETING/菜单。
     * 2e2b0普通战场的统一尾仍执行11cac；skip只跳演出，normal返回仍推进。
     * 错误/quit仅恢复退出快照，不把它作为正常战斗返回帧。 */
    if (animate && !ctx->host_quit) {
        /* 2e8eb fade-out后2e8fc清VGA，再2e906刷新；montage不经此尾。 */
        if(battle_field_return(game,ctx,1,e->started && !ctx->skip)!=0) {
            status=-1;memcpy(ctx->vga->framebuffer,e->map,64000);
        }
    } else memcpy(ctx->vga->framebuffer,e->map,64000);
    /* 正常2e2b0只停止primary；secondary是独立AIL handle，不作额外停止。 */
    if (ctx->primary && e->sfx_enabled) (void)fd2_pcm_stop(ctx->primary);
    if (animate && status==0 && e->started && !stopped(ctx)) fade(ctx,1);
    if (ctx->host_quit) memcpy(ctx->vga->framebuffer,e->map,64000);
    if(!animate || !e->started || ctx->skip || ctx->host_quit || status<0)
        memcpy(ctx->vga->dac,e->dac,768);
    exchange_close(ctx);
    return status;
}


static int physical_dispatch(const fd2_field_game *game,
        const fd2_field_physical_event *event, void *userdata) {
    fd2_battle_animation *ctx=userdata;
    if (!game || !event || !ctx || !ctx->vga || !ctx->fdother || ctx->visual_owner!=game) return -1;
    if (event->phase!=FD2_FIELD_PHYSICAL_PREPARE && ctx->exchange &&
        event->presentation_mode!=ctx->exchange->mode) return -1;
    switch (event->phase) {
        case FD2_FIELD_PHYSICAL_PREPARE: return prepare(game,event,ctx);
        case FD2_FIELD_PHYSICAL_SHORT_MAP:return short_map(game,ctx);
        case FD2_FIELD_PHYSICAL_SHORT_BEGIN:return short_begin(game,ctx);
        case FD2_FIELD_PHYSICAL_COUNTER_REFRESH:return short_counter_refresh(game,ctx);
        case FD2_FIELD_PHYSICAL_RECOVERY_STEP:return recovery_step(game,event,ctx);
        case FD2_FIELD_PHYSICAL_STRIKE:
            if (!ctx->exchange) return -1;
            return ctx->exchange->mode==FD2_FIELD_PHYSICAL_MODE_SHORT ? short_strike(game,event,ctx):
                ctx->exchange->mode==FD2_FIELD_PHYSICAL_MODE_FULL ? strike(game,event,ctx):-1;
        case FD2_FIELD_PHYSICAL_FINISH:
            if (!ctx->exchange) return -1;
            return restore(ctx,game,!event->action_failed);
        default: return -1;
    }
}

/* FULL0..9的field_magic_battle_animation @0x2ff01 / sub_2B996 @0x2b996；
 * 基本13的sub_21AD9 @0x21ad9 / sub_21B18 @0x21b18。仅同步呈现。 */
struct fd2_battle_magic_exchange {
    uint8_t id, targets[FD2_FIELD_MAX_UNITS];
    uint8_t sprite_ids[FD2_FIELD_MAX_UNITS];
    fd2_field_magic_action_kind action_kind;
    uint8_t item_code;
    uint16_t item_power;
    fd2_field_magic_present_mode mode;
    uint8_t effect_id, group_pass, markers;
    uint8_t teleport_phase;
    int teleport_x,teleport_y;
    size_t next_part;
    size_t caster_index, count, effect_count, next;
    fd2_field_unit caster, units[FD2_FIELD_MAX_UNITS];
    battle_clip caster_idle, casting, target_idle[FD2_FIELD_MAX_UNITS], particles;
    battle_image bg, terrain, target_bg, transition[3], map_fx[256], digits[256];
    fd2_pcm_bank sound, map_sound, teleport_sound;
    fd2_field_effect_frames earthquake;
    fd2_field_page *map_base; /* 本次地图effect的临时像素base，非资源/clock owner。 */
    fd2_vga board;
    uint8_t work[172800]; /* 2ff01原2a300：640stride带30行/160px守卫。 */
    uint8_t map[64000], palette[768], dac[768];
    uint32_t raw_heal[FD2_FIELD_MAX_UNITS];
    uint8_t popup_glyphs[FD2_FIELD_MAX_UNITS][4];
    uint8_t popup_x[FD2_FIELD_MAX_UNITS][4];
    fd2_magic_fx fx;
    fd2_magic_fx_parameters fx_parameters;
    uint8_t part_durations[255];
    int sfx_enabled, cast_done, started, popup_done, wrapper_prelude_done;
    int shake_phase, shake_sign, tint_phase;
    uint8_t background_flip;
    uint8_t idle_part, idle_ticks;
};

static uint32_t battle_u32(const uint8_t *p) {
    return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);
}

/* 4ebab/LMI图像以颜色0透明，与4e98d/FIGANI的RLE SKIP分别保留。 */
static int lmi_image(battle_image *out,const fd2_archive *archive,size_t entry,size_t frame) {
    const uint8_t *data;size_t size;
    if (fd2_archive_get(archive,entry,&data,&size)!=0 || size<6 || memcmp(data,"LMI1",4)) return -1;
    size_t count=(size_t)data[4]|((size_t)data[5]<<8), end_table=6+count*4;
    if (frame>=count || end_table>size) return -1;
    uint32_t a=battle_u32(data+6+frame*4),b=frame+1<count ? battle_u32(data+10+frame*4):(uint32_t)size;
    if (a<end_table || a>=b || b>size || fd2_lmi_decode_image(&out->image,data+a,b-a)!=0) return -1;
    size_t pixels=(size_t)out->image.width*(size_t)out->image.height;
    out->opaque=malloc(pixels);
    if (!out->opaque) {image_close(out);return -1;}
    for (size_t i=0;i<pixels;i++) out->opaque[i]=(uint8_t)(out->image.pixels[i]!=0);
    return 0;
}

static int magic_summary(fd2_battle_magic_exchange *e,const fd2_field_game *game,
        const fd2_field_unit *unit,size_t raw_index) {
    return battle_summary_draw(&e->board,game,unit,raw_index);
}

static void magic_close_exchange(fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (!e) return;
    clip_close(&e->caster_idle);clip_close(&e->casting);clip_close(&e->particles);
    for (size_t i=0;i<e->count;i++) clip_close(&e->target_idle[i]);
    for (size_t i=0;i<256;i++) {image_close(&e->map_fx[i]);image_close(&e->digits[i]);}
    for (size_t i=0;i<3;i++) image_close(&e->transition[i]);
    image_close(&e->bg);image_close(&e->terrain);image_close(&e->target_bg);
    fd2_pcm_bank_close(&e->sound);fd2_pcm_bank_close(&e->map_sound);
    fd2_pcm_bank_close(&e->teleport_sound);fd2_field_effect_frames_close(&e->earthquake);
    fd2_field_page_destroy(e->map_base);
    free(e);ctx->magic_exchange=NULL;
}

enum { MAP_ACTORS_NONE, MAP_ACTORS_SOLID, MAP_ACTORS_RAMP, MAP_ACTORS_DUOTONE };

static int magic_map_actor_style(const fd2_battle_magic_exchange *e,uint8_t id) {
    if (e->action_kind==FD2_FIELD_MAGIC_ACTION_ITEM)
        return e->item_code>=5 && e->item_code<=24
            ? k_battle_item_presentations[e->item_code-5][0]:-1;
    if (id<4) return MAP_ACTORS_RAMP;
    if ((id>=4 && id<=8) || id==32) return MAP_ACTORS_DUOTONE;
    if ((id>=13 && id<=22) || (id>=25 && id<=27)) return MAP_ACTORS_SOLID;
    return MAP_ACTORS_NONE;
}

static int magic_map_assets_impl(fd2_battle_magic_exchange *e,const fd2_field_game *game,
        fd2_battle_animation *ctx,uint8_t id) {
    if (id>=33) return -1;
    int style=magic_map_actor_style(e,id);
    if (style<0 || (style==MAP_ACTORS_RAMP && id>=sizeof(k_battle_map_damage_tint))) return -1;
    for (unsigned i=0;id!=23 && i<k_battle_map_fx_count[id];i++) {
        size_t frame=(size_t)k_battle_map_fx_start[id]+i;
        if (frame>=256 || (!e->map_fx[frame].image.pixels &&
            lmi_image(&e->map_fx[frame],ctx->fdother,6,frame)!=0)) return -1;
    }
    if (style==MAP_ACTORS_DUOTONE) for (size_t frame=74;frame<=75;frame++)
        if (!e->map_fx[frame].image.pixels && lmi_image(&e->map_fx[frame],ctx->fdother,6,frame)!=0) return -1;
    int damage=e->action_kind==FD2_FIELD_MAGIC_ACTION_ITEM
        ? k_battle_item_presentations[e->item_code-5][1]==1
        : id<=12 || id==22 || id==26 || id==27 || id==32;
    size_t first=damage ? 94u:105u;
    for (size_t frame=first;id!=23 && id!=25 && frame<first+10u;frame++)
        if (!e->digits[frame].image.pixels && lmi_image(&e->digits[frame],ctx->fdother,5,frame)!=0)
            return -1;
    for (size_t i=0;id!=23 && i<4;i++) {
        size_t frame=k_battle_popup_miss[i];
        if (!e->digits[frame].image.pixels && lmi_image(&e->digits[frame],ctx->fdother,5,frame)!=0)
            return -1;
    }
    if (k_battle_map_fx_cue[id] && !bank_sample_valid(&e->map_sound,k_battle_map_fx_cue[id])) return -1;
    for (size_t i=0;i<sizeof(k_battle_map_extra_cues)/sizeof(k_battle_map_extra_cues[0]);i++)
        if (k_battle_map_extra_cues[i][0]==id && !bank_sample_valid(&e->map_sound,k_battle_map_extra_cues[i][2])) return -1;
    int wrapper=e->action_kind==FD2_FIELD_MAGIC_ACTION_MAGIC && e->mode==FD2_FIELD_MAGIC_MODE_MAP;
    if (style==MAP_ACTORS_SOLID && !bank_sample_valid(&e->map_sound,1)) return -1;
    if (wrapper && id>=10 && id<=12 && !bank_sample_valid(&e->map_sound,13)) return -1;
    if (wrapper && (id==11 || id==12) && !bank_sample_valid(&e->map_sound,2)) return -1;
    if (wrapper && id>=13 && id<=16 && !bank_sample_valid(&e->map_sound,11)) return -1;
    if ((wrapper && id>=11 && id<=16) || id==23) for (size_t i=id>=13 && id<=16 ? 1u:0u;i<=9;i++) {
        const uint8_t *lut=fd2_field_lut_frame(&game->visuals,i);
        if (!lut || fd2_field_game_render_transition_mask(game,&e->board,lut,
            (game->cursor_cell_x-game->camera_cell_x)*24+12,
            (game->cursor_cell_y-game->camera_cell_y)*24+16,1)!=0) return -1;
    }
    for (size_t i=0;i<e->count;i++) for (unsigned phase=0;phase<3;phase++) {
        size_t frame=(size_t)e->sprite_ids[i]*12u+phase;
        if (!game->sprites.decoded_frames || frame>=game->sprites.frame_count) return -1;
    }
    if (wrapper && id>=10 && id<=12 && !e->earthquake.storage &&
        fd2_field_game_prepare_earthquake(ctx->visual_owner,&e->board,&e->earthquake)!=0) return -1;
    if (id==23) {
        for (size_t frame=114;frame<=124;frame++)
            if (!e->map_fx[frame].image.pixels && lmi_image(&e->map_fx[frame],ctx->fdother,6,frame)!=0)
                return -1;
        if (!e->teleport_sound.archive.data &&
            fd2_pcm_bank_open(&e->teleport_sound,ctx->fdother,81)!=0) return -1;
        if (!bank_sample_valid(&e->teleport_sound,0) || !bank_sample_valid(&e->teleport_sound,1)) return -1;
    }
    return 0;
}

static int magic_map_assets(fd2_battle_magic_exchange *e,const fd2_field_game *game,
    fd2_battle_animation *ctx,uint8_t id) {
    fd2_field_render_scope scope;
    if (fd2_field_render_scope_begin(&scope,&e->board,ctx->vga,1)!=0) return -1;
    int result=magic_map_assets_impl(e,game,ctx,id);
    fd2_field_render_scope_end(&scope);
    return result;
}

static int magic_prepare(const fd2_field_game *game,const fd2_field_magic_present_event *event,
        fd2_battle_animation *ctx) {
    fd2_field_magic_present_mode mode=event->presentation_mode;
    if (mode==FD2_FIELD_MAGIC_MODE_FULL && event->magic_id==13 &&
        event->action_kind==FD2_FIELD_MAGIC_ACTION_MAGIC) mode=FD2_FIELD_MAGIC_MODE_MAP;
    int full=mode==FD2_FIELD_MAGIC_MODE_FULL;
    int derived=mode==FD2_FIELD_MAGIC_MODE_DERIVED;
    int composite=mode==FD2_FIELD_MAGIC_MODE_COMPOSITE;
    if (ctx->exchange || ctx->magic_exchange || event->magic_id>=36 ||
        (event->action_kind!=FD2_FIELD_MAGIC_ACTION_MAGIC && event->action_kind!=FD2_FIELD_MAGIC_ACTION_ITEM) ||
        (event->action_kind==FD2_FIELD_MAGIC_ACTION_ITEM &&
            (mode!=FD2_FIELD_MAGIC_MODE_MAP || event->item_code<5 || event->item_code>24)) ||
        (full && event->magic_id>=10) ||
        (derived && event->magic_id!=24 && (event->magic_id<28 || event->magic_id>31)) ||
        (composite && (event->magic_id<32 || event->magic_id>35)) ||
        mode>FD2_FIELD_MAGIC_MODE_COMPOSITE || !event->targets ||
        !event->target_count || event->target_count>FD2_FIELD_MAX_UNITS ||
        event->effect_target_count>event->target_count ||
        ((full || derived || composite) && event->effect_target_count &&
            event->effect_target_count!=event->target_count) ||
        event->actor_index>=game->units.count) return -1;
    const uint8_t *pal;size_t pal_size;
    if (fd2_archive_get(ctx->fdother,0,&pal,&pal_size)!=0 || pal_size<768) return -1;
    fd2_battle_magic_exchange *e=calloc(1,sizeof(*e));if (!e) return -1;
    ctx->magic_exchange=e;e->id=event->magic_id;e->caster_index=event->actor_index;
    e->board.actor_jitter_tick=ctx->vga->actor_jitter_tick;
    e->board.actor_jitter_phase=ctx->vga->actor_jitter_phase;
    e->mode=mode;e->effect_id=event->effect_id;e->group_pass=event->group_pass;
    e->action_kind=event->action_kind;e->item_code=event->item_code;e->item_power=event->item_power;
    if (mode==FD2_FIELD_MAGIC_MODE_MAP && !event->effect_id && e->id) e->effect_id=e->id;
    e->caster=game->units.items[e->caster_index];e->count=event->target_count;
    e->effect_count=event->effect_target_count ? event->effect_target_count:event->target_count;
    e->sfx_enabled=game->option_values[1]!=0;
    size_t caster_scene;
    if (fd2_field_unit_is_hidden(&e->caster) || scene_index(game,&e->caster,&caster_scene)!=0) goto fail;
    for (size_t i=0;i<e->count;i++) {
        size_t raw=event->targets[i];
        if (raw>=game->units.count) goto fail;
        for (size_t j=0;j<i;j++) if (e->targets[j]==raw) goto fail;
        e->targets[i]=(uint8_t)raw;e->units[i]=game->units.items[raw];
        size_t sprite=fd2_field_unit_cached_sprite_id(&game->units,raw);
        if (sprite>UINT8_MAX) goto fail;
        e->sprite_ids[i]=(uint8_t)sprite;
        size_t ignored;
        if (fd2_field_unit_is_hidden(&e->units[i]) || scene_index(game,&e->units[i],&ignored)!=0)
            goto fail;
    }
    if (full || derived || composite) {
        if (ensure_backgrounds(ctx)!=0) goto fail;
        /* 2ff01真实slot2→首WORD0才slot1，不借物理option。Game亦查询同一元数据。 */
        fd2_figani_magic_cast plan;
        if (fd2_figani_magic_action_plan(&plan,ctx->figani,e->caster.unit_id,e->id)!=0 ||
            plan.clip.empty || plan.cost_marker_count!=event->cast_cost_count) goto fail;
        if ((derived ? clip_open(&e->casting,ctx,plan.selected_entry,1)
                : clip_open_visual(&e->casting,ctx->figani,plan.selected_entry))!=0 ||
            e->casting.clip.intro_end>e->casting.clip.part_count ||
            clip_open_visual(&e->caster_idle,ctx->figani,3u*e->caster.unit_id)!=0) goto fail;
        if (full && (fd2_magic_fx_parameters_get(e->id,e->caster.side,&e->fx_parameters)!=0 ||
            clip_open_visual(&e->particles,ctx->fdother,e->fx_parameters.effect_entry)!=0 ||
            fd2_pcm_bank_open(&e->sound,ctx->fdother,e->fx_parameters.sfx_bank)!=0)) goto fail;
        for (size_t i=0;full && i<e->fx_parameters.minimum_sample_count;i++)
            if (!bank_sample_valid(&e->sound,i)) goto fail;
        for (size_t i=0;full && i<e->particles.clip.part_count;i++)
            e->part_durations[i]=e->particles.parts[i].part.duration;
        fd2_magic_fx_parts fx_parts={e->part_durations,e->particles.clip.part_count};
        if (full && fd2_magic_fx_init(&e->fx,e->id,e->caster.side,fx_parts)!=0) goto fail;
        if (!full) e->fx_parameters.return_lut_base=(uint8_t)((e->id==32 || e->id==33) ? 19:15);
        e->shake_phase=3;e->shake_sign=-1;e->tint_phase=8;
        size_t target_scene=k_battle_default_scene[game->stage]; /* 30e25逆序覆盖。 */
        for (size_t i=e->count;i>0;i--) {
            if (!fd2_field_attack_unit_ignores_terrain_modifier(&e->units[i-1]) &&
                scene_index(game,&e->units[i-1],&target_scene)!=0) goto fail;
            if (clip_open_visual(&e->target_idle[i-1],ctx->figani,3u*e->units[i-1].unit_id)!=0 ||
                magic_summary(e,game,&e->units[i-1],e->targets[i-1])!=0) goto fail;
        }
        if (image_entry(&e->bg,&ctx->backgrounds,full ?
                (e->caster.side ? target_scene:caster_scene):
                (e->id==28 ? target_scene:caster_scene))!=0 ||
            image_entry(&e->terrain,&ctx->terrain_images,full && !e->caster.side ? target_scene:caster_scene)!=0)
            goto fail;
        if (derived) {
            if (image_entry(&e->target_bg,&ctx->backgrounds,target_scene)!=0) goto fail;
            for (size_t i=0;i<3;i++) if (image_entry(&e->transition[i],&ctx->backgrounds,i)!=0) goto fail;
            e->next_part=e->casting.clip.intro_end;
            e->shake_phase=0;e->tint_phase=-1;
        }
        if (composite && (clip_open_visual(&e->particles,ctx->fdother,(size_t)e->id+33u)!=0 ||
            fd2_pcm_bank_open(&e->sound,ctx->fdother,k_battle_composite_banks[e->id-32])!=0 ||
            e->particles.clip.part_count<2 || !bank_sample_valid(&e->sound,0) ||
            !bank_sample_valid(&e->sound,1) ||
            ((e->id==32 || e->id==35) && !bank_sample_valid(&e->sound,2)))) goto fail;
        for (size_t i=e->fx_parameters.return_lut_base;
             i<=(size_t)e->fx_parameters.return_lut_base+3u;i++)
            if (!fd2_field_lut_frame(&game->visuals,i)) goto fail;
        memset(e->board.framebuffer,0,64000);
        if (magic_summary(e,game,&e->caster,e->caster_index)!=0 ||
            ((full || (derived && e->id==28)) && magic_summary(e,game,&e->units[0],e->targets[0])!=0)) goto fail;
        blit(e->board.framebuffer,320,200,320,&e->bg,0,50,-1);
    }
    if (!full && !derived) {
        if (fd2_pcm_bank_open(&e->map_sound,ctx->fdother,80)!=0) goto fail;
        if (composite) {
            char name[32];snprintf(name,sizeof(name),"magic.effect.%03u",e->id);
            const fd2_static_flow *flow=fd2_static_flow_lookup("magic",name);
            if (!flow) goto fail;
            size_t groups=0;
            for (size_t i=0;i<flow->count;i++) if (!strcmp(flow->nodes[i].op,"effect_group")) {
                int id=flow->nodes[i].args[0]<0 ? e->id:flow->nodes[i].args[0];
                if (id<0 || id>=33 || magic_map_assets(e,game,ctx,(uint8_t)id)!=0) goto fail;
                groups++;
            }
            if (!groups) goto fail;
        } else if (event->cast_cost_count!=(e->action_kind==FD2_FIELD_MAGIC_ACTION_ITEM && e->id!=23 ? 0:1) ||
            magic_map_assets(e,game,ctx,e->effect_id)!=0) goto fail;
    }
    memcpy(e->map,ctx->vga->framebuffer,64000);memcpy(e->palette,ctx->vga->palette,768);
    memcpy(e->dac,ctx->vga->dac,768);return 0;
fail:
    magic_close_exchange(ctx);return -1;
}

static void magic_idle_advance(fd2_battle_magic_exchange *e,const battle_clip *clip) {
    if(++e->idle_ticks>=clip->parts[e->idle_part].part.duration) {
        e->idle_ticks=0;if(++e->idle_part>=clip->clip.part_count)e->idle_part=0;
    }
}

/* 311e5用>=而物理2ebe1用==；duration0在这里每次前进，不混两种规则。 */
static void magic_idle(fd2_battle_magic_exchange *e,const battle_clip *clip,
        uint8_t *dst,int width,int height,int stride,int x,int y,int tint) {
    part_draw(dst,width,height,stride,clip,e->idle_part,x,y,tint);
    magic_idle_advance(e,clip);
}

static int sprite_lut(fd2_battle_magic_exchange *e,const uint8_t *lut) {
    if (!lut) return -1;
    image_blit(e->board.framebuffer,320,200,320,&e->bg,0,50,-1,lut);
    image_blit(e->board.framebuffer,320,200,320,&e->terrain,164,157,-1,lut);
    return 0;
}

static void dac0(fd2_battle_animation *ctx,uint8_t r,uint8_t g,uint8_t b) {
    ctx->vga->dac[0]=(uint8_t)((r<<2)|(r>>4));
    ctx->vga->dac[1]=(uint8_t)((g<<2)|(g>>4));
    ctx->vga->dac[2]=(uint8_t)((b<<2)|(b>>4));
}

/* field_map_magic_cast_flash @0x1d6c8（原登记 sub_1D6C8）：1cff0在两个selector成功、
 * mode0/11cac之后，且handler费用之前调用。只借App FDOTHER80和VGA，
 * 不建立magic_exchange或复用FULL的30ms闪光。bank目录可局部关闭，
 * voice借用的样本字节仍归整个App的FDOTHER，不能提前停止正常cue。 */
static int magic_map_caller_preamble(const fd2_field_game *game,
        const fd2_field_magic_present_event *event,fd2_battle_animation *ctx) {
    if(!game->ready || game->failure!=FD2_FIELD_FAILURE_NONE ||
        game->units.count>FD2_FIELD_MAX_UNITS || ctx->exchange || ctx->magic_exchange || event->action_failed ||
        event->presentation_mode!=FD2_FIELD_MAGIC_MODE_MAP ||
        event->action_kind!=FD2_FIELD_MAGIC_ACTION_MAGIC || event->magic_id<9u ||
        event->magic_id>27u || event->magic_id==24u || game->active_side!=2u ||
        event->actor_index>=game->units.count)return -1;
    for(size_t i=0;i<sizeof(k_battle_map_cast_preamble)/sizeof(*k_battle_map_cast_preamble);i++)
        if(k_battle_map_cast_preamble[i][0]>1u || k_battle_map_cast_preamble[i][1]!=1u)return -1;
    if(stopped(ctx))return ctx->host_quit?1:0; /* skip不阻断后继费用。 */
    fd2_pcm_bank bank={0};
    if(fd2_pcm_bank_open(&bank,ctx->fdother,80)!=0)return -1;
    if(!bank_sample_valid(&bank,0)) {fd2_pcm_bank_close(&bank);return -1;}
    int enabled=game->option_values[1]!=0;
    int result=play_cue(ctx,&bank,enabled,0);
    fd2_pcm_bank_close(&bank);
    if(result<0)return -1;
    for(size_t i=0;i<sizeof(k_battle_map_cast_preamble)/sizeof(*k_battle_map_cast_preamble) && !stopped(ctx);i++) {
        const uint8_t *color=k_battle_cast_colors[event->magic_id];
        if(k_battle_map_cast_preamble[i][0])dac0(ctx,color[0],color[1],color[2]);
        else dac0(ctx,0,0,0);
        present(ctx,k_battle_map_cast_preamble[i][1]);
    }
    if(ctx->host_quit && ctx->primary && enabled)(void)fd2_pcm_stop(ctx->primary);
    return ctx->host_quit?1:0;
}

/* sub_2EB9F @0x2eb9f→blit_image_clipped @0x4e98d实际按线性地址写。
 * 工作页640列不是裁窗；合法跨行仍属同owner。FULL原172800B，派生原128000B。
 * 首越真实owner后的有名H仍有界省写，不升级为整个ID拒绝/原等值或自然U。 */
static void magic_work_part(fd2_battle_magic_exchange *e,int origin,
        const battle_clip *clip,size_t index,int x,int y,int tint) {
    const battle_part *part=&clip->parts[index];const battle_image *image=&part->drawing;
    size_t owner=e->mode==FD2_FIELD_MAGIC_MODE_FULL?sizeof(e->work):128000u;
    int64_t base=(int64_t)origin+x+part->part.x+640LL*((int64_t)y+part->part.y);
    for(int sy=0;sy<image->image.height;sy++)for(int sx=0;sx<image->image.width;sx++) {
        size_t at=(size_t)sy*(size_t)image->image.width+(size_t)sx;
        if(image->opaque[at]) {
            int64_t dst=base+640LL*sy+sx;
            if(dst>=0 && (uint64_t)dst<owner) {
                uint8_t pixel=image->image.pixels[at];
                e->work[(size_t)dst]=tint<0?pixel:tint<=255?(uint8_t)tint:
                    (uint8_t)(tint+(((tint>>8)+pixel)&7));
            }
        }
    }
}

/* Game给出的FX随机片段只读重放；effect/jitter由事件另传，不再触及业务RNG。 */
typedef struct {
    fd2_battle_animation *ctx;
    const uint32_t *values;
    size_t count, used;
    int origin;
} magic_fx_reader;

static int magic_fx_reader_open(magic_fx_reader *r,fd2_battle_animation *ctx,
        const fd2_field_magic_present_event *event) {
    if (event->fx_random_count && !event->fx_random) return -1;
    for (size_t i=0;i<event->fx_random_count;i++)
        if (event->fx_random[i]>UINT16_MAX) return -1;
    *r=(magic_fx_reader){ctx,event->fx_random,event->fx_random_count,0,19360};
    return 0;
}

static uint32_t magic_fx_recorded_random(void *userdata) {
    magic_fx_reader *r=userdata;
    return r->used<r->count ? r->values[r->used++]:UINT32_MAX;
}

static int magic_fx_emit(const fd2_magic_fx_emission *event,void *userdata) {
    magic_fx_reader *r=userdata;fd2_battle_animation *ctx=r->ctx;
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (event->kind==FD2_MAGIC_FX_PART) {
        if (event->index>=e->particles.clip.part_count) return -1;
        magic_work_part(e,r->origin,&e->particles,event->index,event->x,event->y,-1);
        return 0;
    }
    fd2_pcm_player *player;
    if (event->kind==FD2_MAGIC_FX_PRIMARY_SAMPLE) player=ctx->primary;
    else if (event->kind==FD2_MAGIC_FX_SECONDARY_SAMPLE) player=ctx->secondary;
    else return -1;
    if (!bank_sample_valid(&e->sound,event->index)) return -1;
    if (stopped(ctx) || !e->sfx_enabled || !player) return 0;
    return fd2_pcm_play_replace(player,&e->sound,event->index,1,FD2_GAME_SFX_DEFAULT_GAIN);
}

static int magic_fx_step(magic_fx_reader *r,uint8_t phase,int *native_return) {
    return fd2_magic_fx_step(&r->ctx->magic_exchange->fx,phase,
        magic_fx_recorded_random,r,magic_fx_emit,r,native_return);
}


static void magic_work_idle(fd2_battle_magic_exchange *e,int origin,
        const battle_clip *clip,int x,int y,int tint) {
    magic_work_part(e,origin,clip,e->idle_part,x,y,tint);
    magic_idle_advance(e,clip);
}

static void magic_work_present(fd2_battle_animation *ctx,int origin,uint32_t wait) {
    if (!stopped(ctx))
        copy_pixels(ctx->vga->framebuffer,320,ctx->magic_exchange->work+origin,640);
    present(ctx,wait);
}

static int magic_cast_full(const fd2_field_game *game,const fd2_field_magic_present_event *event,
        fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;magic_fx_reader reader;
    if (!event->actor_before || magic_fx_reader_open(&reader,ctx,event)!=0) return -1;
    e->caster=*event->actor_before;
    if (magic_summary(e,game,&e->caster,e->caster_index)!=0) return -1;
    if (!stopped(ctx)) {
        e->started=1;fade(ctx,0);
        const uint8_t *pal;size_t size;
        if (fd2_archive_get(ctx->fdother,0,&pal,&size)!=0 || size<768) return -1;
        fd2_vga_set_palette(ctx->vga,pal);
        intro_scene(ctx,&e->board,e->work,e->caster.side!=0,0,
            &e->caster_idle,&e->target_idle[0],&e->terrain);
    }
    uint8_t idle=0,elapsed=0;unsigned flashes=0;
    const uint8_t *record=fd2_field_magic_record_get(e->id);
    if (!record) return -1;
    for (size_t i=0;i<e->casting.clip.intro_end;i++) {
        const fd2_figani_part *p=&e->casting.parts[i].part;
        if (p->effect_marker==1) {
            e->caster.mp=(uint16_t)(e->caster.mp-record[5]);
            if (magic_summary(e,game,&e->caster,e->caster_index)!=0 || sprite_lut(e,
                fd2_field_lut_frame(&game->visuals,e->fx_parameters.return_lut_base))!=0)
                return -1;
            flashes=6;
            if (play_cue(ctx,&e->sound,e->sfx_enabled,0)!=0) return -1;
        }
        for (unsigned tick=0;tick<p->duration;tick++) {
            copy_pixels(e->work,320,e->board.framebuffer,320);
            if (e->caster.side) part_draw(e->work,320,200,320,&e->casting,i,0,0,-1);
            part_draw(e->work,320,200,320,&e->target_idle[0],idle,0,0,-1);
            if (!e->caster.side) part_draw(e->work,320,200,320,&e->casting,i,0,0,-1);
            if (!stopped(ctx)) memcpy(ctx->vga->framebuffer,e->work,64000);
            if (flashes) {
                if (!stopped(ctx)) {
                    dac0(ctx,k_battle_cast_colors[e->id][0],k_battle_cast_colors[e->id][1],k_battle_cast_colors[e->id][2]);present(ctx,0);
                    if (!stopped(ctx)) fd2_delay_ms(30);
                    dac0(ctx,0,0,0);
                }
                flashes--;
            }
            present(ctx,1);
            if (++elapsed==e->target_idle[0].parts[idle].part.duration) {
                elapsed=0;if (++idle==e->target_idle[0].clip.part_count) idle=0;
            }
        }
    }
    if (e->id==9) {
        for (int i=0;i<=10;i++) {
            copy_pixels(e->work+320,640,e->board.framebuffer,320);
            if (i!=10) magic_work_part(e,320,&e->caster_idle,0,-10*i,0,-1);
            magic_work_part(e,320,&e->target_idle[0],0,0,0,-1);
            magic_work_present(ctx,320,0);
        }
        if (!stopped(ctx)) fd2_delay_ms(500);
    }
    int duration,native_return;
    if (magic_fx_step(&reader,0,&duration)!=0 || duration<0 || duration>UINT16_MAX) return -1;
    e->idle_part=e->idle_ticks=0;
    for (int frame=0;frame<duration;frame++) {
        copy_pixels(e->work+reader.origin,640,e->board.framebuffer,320);
        if (magic_fx_step(&reader,1,&native_return)!=0) return -1;
        if (e->id!=9) magic_work_part(e,reader.origin,&e->casting,
            e->casting.clip.part_count-1u,0,0,-1);
        magic_work_idle(e,reader.origin,&e->target_idle[0],0,0,-1);
        if (magic_fx_step(&reader,2,&native_return)!=0) return -1;
        magic_work_present(ctx,reader.origin,1);
    }
    return reader.used==reader.count ? 0:-1;
}

/* sub_21B18 @0x21b18 / sub_1C4CC @0x1c4cc：51A83仍为0，地图FX不绘cursor。 */
static int refresh_map(const fd2_field_game *game,fd2_battle_animation *ctx,fd2_vga *target) {
    if (stopped(ctx)) return 0;
    if (ctx->vga->field_page) {
        if (ctx->visual_owner!=game || fd2_field_game_effect_refresh(ctx->visual_owner,ctx->vga)!=0) return -1;
        if (target!=ctx->vga) memcpy(target->framebuffer,ctx->vga->framebuffer,64000);
        return 0;
    }
    uint64_t now=SDL_GetTicksNS();
    if (ctx->visual_owner!=game || !game->ready || game->failure ||
        fd2_field_water_tick(ctx->vga,now)<0 ||
        fd2_field_actor_visual_tick(ctx->vga,now)<0 ||
        fd2_field_game_tick_visual(ctx->visual_owner,now)!=0) return -1;
    target->actor_jitter_tick=ctx->vga->actor_jitter_tick;
    target->actor_jitter_phase=ctx->vga->actor_jitter_phase;
    return battle_field_render(game,ctx,target,1);
}

static void map_actor_paint(fd2_battle_magic_exchange *e,const fd2_field_game *game,
        uint8_t *pixels,size_t index,int tint) {
    const fd2_field_unit *u=&e->units[index];
    size_t phase=game->idle_phase==3 ? 2u:game->idle_phase;
    size_t frame=(size_t)e->sprite_ids[index]*12u+phase;
    uint8_t *source=game->sprites.decoded_frames+frame*576u;
    battle_image sprite={{24,24,source},source};
    blit(pixels+1284,312,192,320,&sprite,
        ((int)u->x-game->camera_cell_x)*24,
        ((int)u->y-game->camera_cell_y)*24-6,tint);
}

/* 1cd17按原RLE选择固定direction0、idle3→2；literal0也参与染色。 */
static int map_actor_page(fd2_battle_magic_exchange *e,const fd2_field_game *game,
    fd2_field_page *page,size_t index,int tint) {
    uint8_t lut[256];
    for (unsigned i=0;i<256;i++) lut[i]=tint<0 ? (uint8_t)i:tint<=255 ? (uint8_t)tint:
        (uint8_t)(tint+(((tint>>8)+i)&7u));
    return fd2_field_fixed_page_actor(page,&game->sprites,&game->units,e->targets[index],
        game->camera_cell_x,game->camera_cell_y,game->idle_phase,lut);
}

static int magic_map_base(fd2_battle_magic_exchange *e,const fd2_vga *vga) {
    fd2_field_page_destroy(e->map_base);e->map_base=NULL;
    if (vga->field_page && !(e->map_base=fd2_field_page_clone(vga->field_page))) return -1;
    return 0;
}

static int magic_map_project(fd2_vga *vga) {
    return fd2_field_page_project(vga->field_page,72,72,vga->framebuffer,sizeof(vga->framebuffer));
}

static int magic_terrain_base(fd2_battle_magic_exchange *e,fd2_battle_animation *ctx) {
    if (magic_map_base(e,ctx->vga)!=0) return -1;
    return e->map_base ? fd2_field_game_effect_terrain_begin(ctx->visual_owner,ctx->vga,e->map_base):0;
}

/* 1cb94：独立terrain页，各raw按顺序画；目标用LMI74/75，其余固定dir0。
 * 与127a9不同，无acted/raw26/移动帧和前景，也不带信息板。
 * 1cac7在1cad5/1cad9与1cafc/1cb00重复同一count/list栈槽；Hex-Rays
 * 跨尾jump推入的EBP/EDI“第二组”是虚参，不是75页的另一目标组。 */
static int magic_duotone_page(const fd2_field_game *game,fd2_battle_animation *ctx,
    fd2_field_page *page,size_t variant) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (fd2_field_game_effect_terrain_begin(ctx->visual_owner,ctx->vga,page)!=0) return -1;
    for (size_t raw=0;raw<game->units.count;raw++) {
        const fd2_field_unit *u=&game->units.items[raw];
        if (fd2_field_unit_is_hidden(u) || (int)u->x<game->camera_cell_x-1 ||
            (int)u->x>game->camera_cell_x+13 || (int)u->y<game->camera_cell_y-1 ||
            (int)u->y>game->camera_cell_y+9) continue;
        size_t target=0;
        while (target<e->count && e->targets[target]!=raw) target++;
        if (target<e->count) {
            if (battle_page_image(page,&e->map_fx[74+variant],
                72+((int)u->x-game->camera_cell_x)*24,
                66+((int)u->y-game->camera_cell_y)*24,-1)!=0) return -1;
        } else if (fd2_field_fixed_page_actor(page,&game->sprites,&game->units,raw,
            game->camera_cell_x,game->camera_cell_y,game->idle_phase,NULL)!=0) return -1;
    }
    return 0;
}

/* 1c4cc地图LMI、1cd17染色、1cac7双色闪烁及1c2da实体闪烁。
 * 原各wrapper只选择有限core；此处既不扣费也不运行effect/RNG。 */
static int magic_map_frames(const fd2_field_game *game,fd2_battle_animation *ctx,uint8_t id) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    int style=magic_map_actor_style(e,id);
    if (id>=33 || style<0 || refresh_map(game,ctx,&e->board)!=0) return -1;
    if (magic_map_base(e,ctx->vga)!=0) return -1;
    for (unsigned frame=0;frame<k_battle_map_fx_count[id] && !stopped(ctx);frame++) {
        memcpy(ctx->vga->framebuffer,e->board.framebuffer,64000);
        if (e->map_base && fd2_field_page_copy_pixels(ctx->vga->field_page,e->map_base)!=0) return -1;
        for (size_t i=0;i<e->count;i++) {
            const battle_image *fx=&e->map_fx[(size_t)k_battle_map_fx_start[id]+frame];
            int x=((int)e->units[i].x-game->camera_cell_x)*24;
            int y=((int)e->units[i].y-game->camera_cell_y)*24-6;
            if (e->map_base) {
                if ((int)e->units[i].x>=game->camera_cell_x-1 && (int)e->units[i].x<=game->camera_cell_x+13 &&
                    (int)e->units[i].y>=game->camera_cell_y-1 && (int)e->units[i].y<=game->camera_cell_y+9 &&
                    battle_page_image(ctx->vga->field_page,fx,72+x,72+y,-1)!=0) return -1;
            } else blit(ctx->vga->framebuffer+1284,312,192,320,fx,x,y,-1);
        }
        if (e->map_base && magic_map_project(ctx->vga)!=0) return -1;
        int sample=!frame && k_battle_map_fx_cue[id] ? k_battle_map_fx_cue[id]:-1;
        if (sample<0) for (size_t i=0;i<sizeof(k_battle_map_extra_cues)/sizeof(k_battle_map_extra_cues[0]);i++)
            if (k_battle_map_extra_cues[i][0]==id && k_battle_map_extra_cues[i][1]==frame)
                sample=k_battle_map_extra_cues[i][2];
        if (sample>=0 && play_cue(ctx,&e->map_sound,e->sfx_enabled,(uint8_t)sample)!=0) return -1;
        present(ctx,1);
    }
    if (stopped(ctx)) return 0;
    if (refresh_map(game,ctx,&e->board)!=0) return -1;
    memcpy(ctx->vga->framebuffer,e->board.framebuffer,64000);present(ctx,0);
    if (magic_map_base(e,ctx->vga)!=0) return -1;
    if (style==MAP_ACTORS_RAMP) {
        for (unsigned frame=0;frame<10 && !stopped(ctx);frame++) {
            memcpy(ctx->vga->framebuffer,e->board.framebuffer,64000);
            if (e->map_base && fd2_field_page_copy_pixels(ctx->vga->field_page,e->map_base)!=0) return -1;
            int phase=7-(int)(frame%8u);
            if (!phase) phase=8; /* 原rotation0仍是8色映射，不是实体常色。 */
            for (size_t i=0;i<e->count;i++) {
                int tint=k_battle_map_damage_tint[id]+(phase<<8);
                if (e->map_base) {
                    if (map_actor_page(e,game,ctx->vga->field_page,i,tint)!=0) return -1;
                } else map_actor_paint(e,game,ctx->vga->framebuffer,i,tint);
            }
            if (e->map_base && magic_map_project(ctx->vga)!=0) return -1;
            present(ctx,1);
        }
        /* 1cd17尾只投影旧snapshot，App仍保最后一帧染色。 */
        if (!stopped(ctx)) {memcpy(ctx->vga->framebuffer,e->board.framebuffer,64000);present(ctx,0);}
    } else if (style==MAP_ACTORS_DUOTONE) {
        if (e->map_base) {
            fd2_field_page *alternate=fd2_field_page_create();
            if (!alternate) return -1;
            int result=magic_duotone_page(game,ctx,ctx->vga->field_page,0);
            if (!result) result=magic_duotone_page(game,ctx,alternate,1);
            for (unsigned frame=0;!result && frame<8 && !stopped(ctx);frame++) {
                result=fd2_field_page_project(frame&1u ? alternate:ctx->vga->field_page,
                    72,72,ctx->vga->framebuffer,64000);
                if (!result) {present(ctx,0);if (!stopped(ctx)) fd2_delay_ms(90);}
            }
            fd2_field_page_destroy(alternate);
            if (result) return -1;
            if (!stopped(ctx)) {
                if (refresh_map(game,ctx,ctx->vga)!=0) return -1;
                present(ctx,0);
            }
            return 0;
        }
        for (unsigned frame=0;frame<8 && !stopped(ctx);frame++) {
            memcpy(ctx->vga->framebuffer,e->board.framebuffer,64000);
            for (size_t i=0;i<e->count;i++) map_actor_paint(e,game,ctx->vga->framebuffer,i,74+(int)(frame&1u));
            present(ctx,0);if (!stopped(ctx)) fd2_delay_ms(90);
        }
    } else if (style==MAP_ACTORS_SOLID) {
        memcpy(e->work,e->board.framebuffer,64000);
        /* field_actor_group_flash @0x1c2da（原sub_1C2DA）虽传51f15[id]，
         * map_sprite_solid_blit_24 @0x4e127（原同名）实际只取原stride456低byte。
         * arg_C未消费，不能借SDL stride或caller色表改变原有效常色。 */
        if (play_cue(ctx,&e->map_sound,e->sfx_enabled,1)!=0) return -1;
        for (size_t i=0;i<e->count;i++) {
            if (e->map_base) {
                if (map_actor_page(e,game,ctx->vga->field_page,i,fd2_battle_animation_map_solid_color())!=0) return -1;
            } else map_actor_paint(e,game,e->work,i,fd2_battle_animation_map_solid_color());
        }
        if (e->map_base && fd2_field_page_project(ctx->vga->field_page,72,72,e->work,64000)!=0) return -1;
        for (unsigned frame=0;frame<11 && !stopped(ctx);frame++) {
            memcpy(ctx->vga->framebuffer,(frame&1u) ? e->work:e->board.framebuffer,64000);
            present(ctx,frame<10 ? 1u:0u);
        }
    }
    return 0;
}

static int map_actor_circle(const fd2_field_game *game,fd2_battle_animation *ctx,
    size_t actor_index,int radius,int step);

/* 21527/2185f/21a9e最终都调用21548：三页缩放、cue13、60帧、11cac。
 * 1ca89费用由Game在本CAST之前提交；本叶没有1c4cc/LMI/style或伤害。 */
static int magic_map_quake(const fd2_field_game *game,fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    /* 21548在真正CAST时生成三页；资源PREPARE的旧视觉快照不能重放成热页。 */
    fd2_field_effect_frames_close(&e->earthquake);
    int prepared=fd2_field_game_begin_earthquake(ctx->visual_owner,ctx->vga,&e->earthquake);
    if (prepared<0) return -1;
    if (prepared>0) {ctx->host_quit=1;return 0;}
    for (size_t frame=0;frame<60 && !stopped(ctx);frame++) {
        int source=fd2_field_effect_source_index(FD2_FIELD_EFFECT_EARTHQUAKE,frame);
        if (source<0 || (size_t)source>=e->earthquake.snapshot_count) return -1;
        /* 21548@217a8先cue13，21779随后投影；cue失败保旧VGA前缀。 */
        if (frame<43 && frame%6u==0 && play_cue(ctx,&e->map_sound,e->sfx_enabled,13)!=0) return -1;
        memcpy(ctx->vga->framebuffer,e->earthquake.snapshots[source],64000);
        if (ctx->vga->field_page && fd2_field_effect_apply_page_frame(&e->earthquake,ctx->vga,frame)!=0) return -1;
        present(ctx,0);if (!stopped(ctx)) fd2_delay_ms(10);
    }
    /* 原21548恢复真实page指针后先11cac(0)，随后才进入HP/RNG写点。 */
    if (!stopped(ctx) && refresh_map(game,ctx,ctx->vga)!=0) return -1;
    if (!stopped(ctx)) present(ctx,0);
    return 0;
}

/* 2185f/21a9e先cue2、2189a(actor,15/30,10/16)，之后才21548内1ca89。
 * PREPARE后的窄同步相位只读Game；费用/RNG仍由原Game owner提交。 */
static int magic_map_wrapper_prelude(const fd2_field_game *game,
        const fd2_field_magic_present_event *event,fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if(!e || e->mode!=FD2_FIELD_MAGIC_MODE_MAP || e->action_kind!=FD2_FIELD_MAGIC_ACTION_MAGIC ||
        (e->effect_id!=11 && e->effect_id!=12) || event->actor_index!=e->caster_index ||
        event->effect_id!=e->effect_id || event->presentation_mode!=FD2_FIELD_MAGIC_MODE_MAP ||
        e->id!=e->effect_id ||
        e->cast_done || e->wrapper_prelude_done)return -1;
    e->wrapper_prelude_done=1;
    if(stopped(ctx))return 0;
    if(play_cue(ctx,&e->map_sound,e->sfx_enabled,2)!=0)return -1;
    return map_actor_circle(game,ctx,e->caster_index,e->effect_id==11?15:30,e->effect_id==11?10:16);
}

static int magic_map_cast(const fd2_field_game *game,const fd2_field_magic_present_event *event,
        fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    uint8_t id=event->effect_id;
    if (!id && e->id) id=e->id;
    if (id>=33) return -1;
    e->effect_id=id;e->group_pass=event->group_pass;e->next=0;e->popup_done=0;
    for (size_t i=0;i<e->count;i++) e->units[i]=game->units.items[e->targets[i]];
    if (id==23) return 0; /* 2218a的两次22253在明确坐标提交边界分别重放。 */
    int wrapper=e->action_kind==FD2_FIELD_MAGIC_ACTION_MAGIC && e->mode==FD2_FIELD_MAGIC_MODE_MAP;
    if(wrapper && id>=10 && id<=12) {
        if(id!=e->id || (id!=10 && !e->wrapper_prelude_done))return -1;
        return magic_map_quake(game,ctx);
    }
    return magic_map_frames(game,ctx,id);
}

/* field_ai_ray_item_cast_present @0x2dfc8（原登记 sub_2DFC8），唯一caller
 * field_ai_item_action_execute @0x15055。资源只在这个同步叶内借用；原53a49/
 * FDSHAP原先释放后重建，App也重新malloc153216；C继续借既有唯一page。
 * 有当前producer的像素按原写序等值；fresh首未定义读后的保旧底仅属H，
 * 不把此保留页称原同期live App，也不仿造第二热cache。
 * EDI不是从0初始化：150f1→1511c给149f8目标数，1f04a/11cac(1)/17aa9
 * 保EDI，2e9a8也push/pop EDI；2e1a7先比较这个起点再绘casting part。
 * 演出不建立magic_exchange，HOST/错误保已到达VGA前缀，业务仍归20c6f。 */
typedef struct {
    battle_clip idle, casting;
    battle_image background, terrain;
    fd2_vga board;
    uint8_t work[128000];
} battle_ai_ray_scope;

static void ai_ray_scope_close(battle_ai_ray_scope **scope) {
    battle_ai_ray_scope *s=*scope;
    if(!s)return;
    clip_close(&s->idle);clip_close(&s->casting);
    image_close(&s->background);image_close(&s->terrain);
    free(s);*scope=NULL;
}

/* 15055→2189a(actor,80,-4)：无Magic11/12的cue2；先terrain临时页，再逐帧
 * circle12→127a9(含foreground)→project，尾部才11cac(0)。 */
static int map_actor_circle(const fd2_field_game *game,fd2_battle_animation *ctx,
        size_t actor_index,int radius,int step) {
    if(actor_index>=game->units.count)return -1;
    const fd2_field_unit *actor=&game->units.items[actor_index];
    fd2_field_page *base=NULL;
    uint8_t *diagnostic_base=NULL;
    if(ctx->vga->field_page) {
        base=fd2_field_page_clone(ctx->vga->field_page);
        if(!base)return -1;
        if(fd2_field_game_effect_terrain_begin(ctx->visual_owner,ctx->vga,base)!=0) {
            fd2_field_page_destroy(base);return -1;
        }
    }
    else {
        /* 无App的离屏诊断仍从一次terrain快照重画，不累计十张LUT。 */
        fd2_vga terrain=*ctx->vga;
        if(battle_field_render(game,ctx,&terrain,2)!=0)return -1;
        diagnostic_base=malloc(64000);if(!diagnostic_base)return -1;
        memcpy(diagnostic_base,terrain.framebuffer,64000);
    }
    int status=0;
    /* 218d3/218e6写局部x/y；2194d/21951实际SP只读这两项，
     * y=24*(传入actor.y-camera.y)+18，不借cursor或24618的+16。 */
    int x=((int)actor->x-game->camera_cell_x)*24+12;
    int y=((int)actor->y-game->camera_cell_y)*24+18;
    for(unsigned frame=0;frame<10u && !stopped(ctx);frame++) {
        if(diagnostic_base)for(size_t row=4;row<196;row++)
            memcpy(ctx->vga->framebuffer+row*320u+4u,diagnostic_base+row*320u+4u,312);
        const uint8_t *lut=fd2_field_lut_frame(&game->visuals,frame);
        if(!lut || (base ? fd2_field_game_effect_mask_frame(game,ctx->vga,base,lut,
                x,y,radius,0,1):fd2_field_transition_lut_circle12(ctx->vga,lut,x,y,radius))!=0) {
            status=-1;break;
        }
        if(diagnostic_base) {
            if(fd2_field_actor_visual_tick(ctx->vga,SDL_GetTicksNS())<0){status=-1;break;}
            fd2_field_game_render_effect_actors(game,ctx->vga);
        }
        present(ctx,0);radius+=step;
    }
    free(diagnostic_base);fd2_field_page_destroy(base);
    if(!status && !stopped(ctx)) {
        status=refresh_map(game,ctx,ctx->vga);
        if(!status)present(ctx,0);
    }
    return status;
}

static int magic_ai_ray_item(const fd2_field_game *game,
        const fd2_field_magic_present_event *event,fd2_battle_animation *ctx) {
    if(!game->ready || game->failure!=FD2_FIELD_FAILURE_NONE ||
        game->units.count>FD2_FIELD_MAX_UNITS || ctx->exchange || ctx->magic_exchange ||
        !ctx->figani || event->action_failed || game->active_side>1u ||
        event->presentation_mode!=FD2_FIELD_MAGIC_MODE_MAP ||
        event->action_kind!=FD2_FIELD_MAGIC_ACTION_ITEM || event->item_code!=24u ||
        event->item_power!=3u || event->magic_id!=3u ||
        event->actor_index>=game->units.count || !event->targets ||
        !event->target_count || event->target_count>game->units.count)return -1;
    for(size_t i=0;i<event->target_count;i++)if(event->targets[i]>=game->units.count)return -1;
    for(unsigned i=0;i<10u;i++)if(!fd2_field_lut_frame(&game->visuals,i))return -1;
    for(size_t i=0;i<768u;i++)if(ctx->vga->palette[i]>63u)return -1;
    if(stopped(ctx))return ctx->host_quit?1:0;
    const fd2_field_unit *actor=&game->units.items[event->actor_index];
    if(actor->side!=game->active_side || !game->map.cells ||
        actor->x>=game->map.width || actor->y>=game->map.height)return -1;
    const fd2_terrain_attr *attr=fd2_terrain_attr_get(&game->terrain,
        fd2_field_cell_terrain(fd2_field_map_cell(&game->map,actor->x,actor->y)));
    if(!attr || ensure_backgrounds(ctx)!=0)return -1;
    battle_ai_ray_scope *scope=calloc(1,sizeof(*scope));
    fd2_pcm_bank bank={0};int status=-1,stopped_voice=0,started=0;
    int enabled=game->option_values[1]!=0;
    if(!scope)return -1;
    if(clip_open(&scope->idle,ctx,(size_t)actor->unit_id*3u,0)!=0 ||
        clip_open(&scope->casting,ctx,(size_t)actor->unit_id*3u+1u,1)!=0 ||
        image_entry(&scope->background,&ctx->backgrounds,attr->attr2)!=0 ||
        image_entry(&scope->terrain,&ctx->terrain_images,3)!=0 ||
        battle_summary_draw(&scope->board,game,actor,event->actor_index)!=0)goto done;
    blit(scope->board.framebuffer,320,200,320,&scope->background,0,50,-1);
    bank=scope->casting.bank;memset(&scope->casting.bank,0,sizeof(scope->casting.bank));
    status=0;started=1;
    for(size_t i=0;i<sizeof(k_battle_ai_ray_item_present)/sizeof(*k_battle_ai_ray_item_present) && !stopped(ctx);i++) {
        const int16_t *op=k_battle_ai_ray_item_present[i];
        switch(op[0]) {
        case 1:fade(ctx,op[1]);break;
        case 2:
            if(!scope || op[1]!=1){status=-1;break;}
            intro_scene(ctx,&scope->board,scope->work,actor->side!=0,op[1],
                &scope->idle,&scope->idle,&scope->terrain);break;
        case 3:
            if(!scope){status=-1;break;}
            for(size_t part=event->target_count;part<scope->casting.clip.part_count && !stopped(ctx);part++) {
                const fd2_figani_part *p=&scope->casting.parts[part].part;
                if(p->cue && play_cue(ctx,&bank,enabled,p->cue)!=0){status=-1;break;}
                copy_pixels(scope->work,320,scope->board.framebuffer,320);
                part_draw(scope->work,320,200,320,&scope->casting,part,0,0,-1);
                memcpy(ctx->vga->framebuffer,scope->work,64000);present(ctx,0);
                /* 2e19b每part一次，不把duration展开成重复帧；0也调用reader。 */
                if(!stopped(ctx))fd2_wait_ticks(p->duration);
            }
            break;
        case 4:ai_ray_scope_close(&scope);break;
        case 5:if(scope || op[1]!=6)status=-1;else fd2_wait_ticks((uint32_t)op[1]);break;
        case 6:memset(ctx->vga->framebuffer,0,64000);break;
        case 7:
            if(op[1]!=3 || battle_field_return(game,ctx,0,0)!=0)status=-1;
            else present(ctx,0);
            break;
        case 8:
            if(ctx->primary && enabled && fd2_pcm_stop(ctx->primary)!=0)status=-1;
            stopped_voice=1;break;
        case 9:status=map_actor_circle(game,ctx,event->actor_index,op[1],op[2]);break;
        case 10:fd2_delay_ms((uint32_t)op[1]);break;
        case 11:
            for(int level=op[1];level>=0 && !stopped(ctx);level--) {
                if(fd2_vga_palette_add(ctx->vga,0,255,level)!=0){status=-1;break;}
                present(ctx,0);if(!stopped(ctx))fd2_delay_ms((uint32_t)op[2]);
            }
            break;
        default:status=-1;break;
        }
        if(status<0)break;
    }
done:
    ai_ray_scope_close(&scope);
    if(started && !stopped_voice && ctx->primary && enabled && (status<0 || ctx->host_quit || ctx->skip) &&
        fd2_pcm_stop(ctx->primary)<0)status=-1;
    fd2_pcm_bank_close(&bank);
    return status<0?-1:ctx->host_quit?1:0;
}

/* 21ad9/21b99/2211c/22153→21eb1：四种恢复caller各自半径和步长。 */
static int magic_heal_prelude(const fd2_field_game *game,fd2_battle_animation *ctx,uint8_t id) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (id<13 || id>16 || play_cue(ctx,&e->map_sound,e->sfx_enabled,11)!=0) return -1;
    int x=(game->cursor_cell_x-game->camera_cell_x)*24+12;
    int y=(game->cursor_cell_y-game->camera_cell_y)*24+16;
    int start=k_battle_heal_masks[id-13][0],step=k_battle_heal_masks[id-13][1];
    if (magic_terrain_base(e,ctx)!=0) return -1;
    for (int i=9;i>0 && !stopped(ctx);i--) {
        const uint8_t *lut=fd2_field_lut_frame(&game->visuals,(size_t)i);
        int radius=start+(9-i)*step;
        if (e->map_base ? fd2_field_game_effect_mask_frame(game,ctx->vga,e->map_base,lut,x,y,radius,0,0)!=0:
            fd2_field_game_render_transition_mask(game,ctx->vga,lut,x,y,radius)!=0) return -1;
        present(ctx,0);if (!stopped(ctx)) fd2_delay_ms(5);
    }
    if (!stopped(ctx)) fd2_delay_ms(200);
    for (int i=3;i<10 && !stopped(ctx);i++) {
        const uint8_t *lut=fd2_field_lut_frame(&game->visuals,(size_t)i);
        if (e->map_base ? fd2_field_game_effect_mask_frame(game,ctx->vga,e->map_base,lut,x,y,start+8*step,0,0)!=0:
            fd2_field_game_render_transition_mask(game,ctx->vga,lut,x,y,start+8*step)!=0) return -1;
        present(ctx,0);if (!stopped(ctx)) fd2_delay_ms(5);
    }
    if (!stopped(ctx) && refresh_map(game,ctx,ctx->vga)!=0) return -1;
    present(ctx,0);if (!stopped(ctx)) fd2_delay_ms(200);
    return 0;
}

/* 30e9d共用intro；业务费用已经由Game按相同part标记提交，此处只重放板。 */
static int magic_special_intro(const fd2_field_game *game,
        const fd2_field_magic_present_event *event,fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (!event->actor_before) return -1;
    e->caster=*event->actor_before;
    if (magic_summary(e,game,&e->caster,e->caster_index)!=0) return -1;
    if (!stopped(ctx)) {
        const uint8_t *pal;size_t size;
        e->started=1;fade(ctx,0);
        if (fd2_archive_get(ctx->fdother,0,&pal,&size)!=0 || size<768) return -1;
        fd2_vga_set_palette(ctx->vga,pal);
        intro_scene(ctx,&e->board,e->work,e->caster.side!=0,e->id!=28,
            &e->caster_idle,&e->target_idle[0],&e->terrain);
    }
    const uint8_t *record=fd2_field_magic_record_get(e->id);
    if (!record) return -1;
    unsigned flashes=0;uint8_t idle=0,elapsed=0;
    for (size_t i=0;i<e->casting.clip.intro_end;i++) {
        const fd2_figani_part *p=&e->casting.parts[i].part;
        if (e->mode==FD2_FIELD_MAGIC_MODE_DERIVED && e->id!=31 && p->cue &&
            play_cue(ctx,&e->casting.bank,e->sfx_enabled,p->cue)!=0) return -1;
        if (p->effect_marker==1) {
            e->caster.mp=(uint16_t)(e->caster.mp-record[5]);
            if (magic_summary(e,game,&e->caster,e->caster_index)!=0) return -1;
            if (e->mode==FD2_FIELD_MAGIC_MODE_COMPOSITE) {
                if (sprite_lut(e,fd2_field_lut_frame(&game->visuals,e->fx_parameters.return_lut_base))!=0 ||
                    play_cue(ctx,&e->sound,e->sfx_enabled,0)!=0) return -1;
                flashes=6;
            }
        }
        for (unsigned frame=0;frame<p->duration;frame++) {
            copy_pixels(e->work,320,e->board.framebuffer,320);
            if (e->caster.side) part_draw(e->work,320,200,320,&e->casting,i,0,0,-1);
            if (e->id==28) part_draw(e->work,320,200,320,&e->target_idle[0],idle,0,0,-1);
            if (!e->caster.side) part_draw(e->work,320,200,320,&e->casting,i,0,0,-1);
            if (!stopped(ctx)) memcpy(ctx->vga->framebuffer,e->work,64000);
            if (flashes) {
                if (!stopped(ctx)) {
                    dac0(ctx,k_battle_cast_colors[e->id][0],k_battle_cast_colors[e->id][1],k_battle_cast_colors[e->id][2]);
                    present(ctx,0);if (!stopped(ctx)) fd2_delay_ms(30);dac0(ctx,0,0,0);
                }
                flashes--;
            }
            present(ctx,1);
            if (++elapsed==e->target_idle[0].parts[idle].part.duration) {
                elapsed=0;if (++idle==e->target_idle[0].clip.part_count) idle=0;
            }
        }
    }
    return 0;
}

/* 2df01只写palette index0..254；factor0是目标纯色，40回原base palette。 */
static void composite_palette(fd2_battle_animation *ctx,unsigned factor) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (stopped(ctx)) return;
    unsigned slot=e->id-32u;
    uint8_t color[]={k_battle_composite_red[slot],k_battle_composite_green[slot],k_battle_composite_blue[slot]};
    for (size_t i=0;i<255;i++) for (size_t channel=0;channel<3;channel++) {
        int value=color[channel]+(int)factor*((int)ctx->vga->palette[3*i+channel]-color[channel])/40;
        ctx->vga->dac[3*i+channel]=(uint8_t)((value<<2)|(value>>4));
    }
}

/* field_composite_magic_full_execute @0x2d80d（原sub_2D80D）：专用FIGANI/
 * FDOTHER演出后返回地图；其后完整目标组编排由Game消费唯一冷图。 */
static int magic_composite_cast(const fd2_field_game *game,
        const fd2_field_magic_present_event *event,fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (magic_special_intro(game,event,ctx)!=0) return -1;
    if (!stopped(ctx)) fd2_wait_ticks(6);
    for (int i=0;i<8 && !stopped(ctx);i++) {
        copy_pixels(e->work,640,e->board.framebuffer,320);
        part_draw(e->work,640,200,640,&e->caster_idle,0,20*i,0,-1);
        magic_work_present(ctx,0,1);
    }
    for (int i=8;i>=0 && !stopped(ctx);i--) {
        copy_pixels(e->work,640,e->board.framebuffer,320);
        part_draw(e->work,640,200,640,&e->particles,0,30*i,0,-1);
        magic_work_present(ctx,0,1);
    }
    if ((e->id==33 || e->id==34) && !stopped(ctx))
        part_draw(e->board.framebuffer,320,200,320,&e->particles,0,0,0,-1);
    for (size_t i=1;i<e->particles.clip.part_count && !stopped(ctx);i++) {
        copy_pixels(e->work,320,e->board.framebuffer,320);
        part_draw(e->work,320,200,320,&e->particles,i,0,0,-1);
        memcpy(ctx->vga->framebuffer,e->work,64000);present(ctx,0);
        if ((e->id==34 && i==2) || (e->id==33 && i==6)) {
            if (play_cue(ctx,&e->sound,e->sfx_enabled,1)!=0) return -1;
        } else if ((e->id==32 || e->id==35) && i==1 && ctx->secondary && e->sfx_enabled) {
            if (fd2_pcm_play_replace(ctx->secondary,&e->sound,2,1,FD2_GAME_SFX_DEFAULT_GAIN)!=0) return -1;
        }
        if (!stopped(ctx)) fd2_wait_ticks(2);
    }
    if (e->id==32 || e->id==35) for (unsigned i=0;i<=10 && !stopped(ctx);i++) {
        if (!(i&1u) && play_cue(ctx,&e->sound,e->sfx_enabled,1)!=0) return -1;
        copy_pixels(e->work,320,e->board.framebuffer,320);
        part_draw(e->work,320,200,320,&e->particles,(size_t)e->particles.clip.part_count-2u+(i&1u),0,0,-1);
        memcpy(ctx->vga->framebuffer,e->work,64000);present(ctx,2);
        composite_palette(ctx,40u-4u*i);
        present(ctx,0); /* 原DAC写立即可见，SDL须上传同一像素的新palette。 */
    }
    if (!stopped(ctx)) {
        composite_palette(ctx,0);
        /* 2dd3b..2dd51：清VGA后11cac(1)，不读water；紧接factor0
         * 的首轮上传承担同一页/DAC提交，不在41个源wait前另pump/upload。 */
        if (battle_field_return(game,ctx,0,1)!=0) return -1;
        for (unsigned i=0;i<=40 && !stopped(ctx);i++) {
            composite_palette(ctx,i);present(ctx,0);if (!stopped(ctx)) fd2_delay_ms(6);
        }
    }
    if (!stopped(ctx)) {
        if (ctx->primary && e->sfx_enabled) (void)fd2_pcm_stop(ctx->primary);
        /* 2dd9e stop后2ddac释放cast bank；MAP组只借另一个map_sound。 */
        fd2_pcm_bank_close(&e->sound);
    }
    return 0;
}

/* field_derived_magic_full_execute @0x2cf30（原sub_2CF30）：每part的HP写点
 * 已由Game提交；此处按同一marker/duration投影状态板、抖动和音效。 */
static int magic_derived_step(const fd2_field_game *game,
        const fd2_field_magic_present_event *event,fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (!e || e->mode!=FD2_FIELD_MAGIC_MODE_DERIVED || !e->cast_done || e->next>=e->count ||
        event->target_index!=e->targets[e->next] || !event->target_before || !event->target_after ||
        event->part_index!=e->next_part || e->next_part>=e->casting.clip.part_count) return -1;
    const fd2_figani_part *p=&e->casting.parts[e->next_part].part;
    if (e->next_part==e->casting.clip.intro_end) {
        e->markers=0;e->idle_part=e->idle_ticks=0;
        e->units[e->next]=*event->target_before;
        memset(e->work,0,128000);
        copy_pixels(e->work+320,640,ctx->vga->framebuffer,320);
        if (e->id!=28 && scene_pan(ctx,game,&e->board,e->work,e->transition,&e->terrain,
            &e->units[e->next],e->targets[e->next],&e->target_idle[e->next],&e->target_bg,e->units[e->next].side!=0)!=0) return -1;
    }
    if (p->cue && play_cue(ctx,&e->casting.bank,e->sfx_enabled,p->cue)!=0) return -1;
    if (p->effect_marker==1) {
        e->markers++;e->shake_phase=5;e->tint_phase=33;
        e->units[e->next]=*event->target_after;
        if (magic_summary(e,game,&e->units[e->next],e->targets[e->next])!=0) return -1;
    }
    if (event->marker_count!=e->markers) return -1;
    for (unsigned frame=0;frame<p->duration;frame++) {
        copy_pixels(e->work+320,640,e->board.framebuffer,320);
        magic_work_idle(e,320,&e->target_idle[e->next],-shake_x[e->shake_phase],0,e->tint_phase);
        magic_work_part(e,320,&e->casting,e->next_part,0,0,-1);
        magic_work_present(ctx,320,1);
        if (e->shake_phase) e->shake_phase--;
        e->tint_phase=-1;
    }
    e->next_part++;
    return 0;
}

static void teleport_view_copy(fd2_vga *vga,const uint8_t *source) {
    for (size_t y=4;y<196;y++) memcpy(vga->framebuffer+y*320u+4u,source+y*320u+4u,312);
}

/* 22253/22547/22656：radius是固定11，变化的是first_row，不能把两者
 * 混为普通恢复术的扩圆。frame124目录偏移为6+4*124=502。 */
static int teleport_mask(const fd2_field_game *game,fd2_battle_animation *ctx,
        size_t lut_frame,int first_row) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    const uint8_t *lut=fd2_field_lut_frame(&game->visuals,lut_frame);
    int x=(e->teleport_x-game->camera_cell_x)*24+12;
    int y=(e->teleport_y-game->camera_cell_y)*24+15;
    if (e->map_base) return fd2_field_game_effect_mask_frame(game,ctx->vga,e->map_base,lut,x,y,11,first_row,0);
    teleport_view_copy(ctx->vga,e->board.framebuffer);
    if (fd2_field_transition_lut_before_actors_from(ctx->vga,lut,x,y,11,first_row)!=0 ||
        fd2_field_actor_visual_tick(ctx->vga,SDL_GetTicksNS())<0) return -1;
    fd2_field_game_render_effect_actors(game,ctx->vga);
    return fd2_field_transition_lut_after_actors_from(ctx->vga,lut,x,y,11,first_row);
}

static int magic_teleport(const fd2_field_game *game,
        const fd2_field_magic_present_event *event,fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (!e || e->mode!=FD2_FIELD_MAGIC_MODE_MAP || e->id!=23 || e->count!=1 ||
        event->target_index!=e->targets[0] || !event->target_before || !event->target_after)
        return -1;
    unsigned phase=event->phase==FD2_FIELD_MAGIC_PRESENT_TELEPORT_OUT ? 0u:
        event->phase==FD2_FIELD_MAGIC_PRESENT_TELEPORT_OUT_COMMIT ? 1u:
        event->phase==FD2_FIELD_MAGIC_PRESENT_TELEPORT_IN ? 2u:3u;
    if (e->teleport_phase!=phase) return -1;
    e->teleport_phase++;e->cast_done=1;
    if (stopped(ctx)) {
        if (phase==3) e->next=e->count;
        return 0;
    }
    if (!(phase&1u)) {
        e->teleport_x=phase==0 ? event->target_before->x:event->destination_x;
        e->teleport_y=phase==0 ? event->target_before->y:event->destination_y;
        if (ctx->vga->field_page) {
            if (magic_terrain_base(e,ctx)!=0 || fd2_field_page_project(e->map_base,72,72,
                    e->board.framebuffer,sizeof(e->board.framebuffer))!=0) return -1;
        } else if (battle_field_render(game,ctx,&e->board,2)!=0) return -1;
        int x=(e->teleport_x-game->camera_cell_x)*24;
        int y=(e->teleport_y-game->camera_cell_y)*24+1;
        for (size_t frame=114;frame<=124 && !stopped(ctx);frame++) {
            if (e->map_base) {
                if (fd2_field_page_copy_pixels(ctx->vga->field_page,e->map_base)!=0 ||
                    battle_page_image(ctx->vga->field_page,&e->map_fx[frame],72+x,72+y,-1)!=0) return -1;
            } else {
                teleport_view_copy(ctx->vga,e->board.framebuffer);
                blit(ctx->vga->framebuffer+1284,312,192,320,&e->map_fx[frame],x,y,-1);
            }
            if (ctx->vga->field_page) {
                if (fd2_field_game_effect_actors(game,ctx->vga)!=0) return -1;
            } else {
                if(fd2_field_actor_visual_tick(ctx->vga,SDL_GetTicksNS())<0)return -1;
                fd2_field_game_render_effect_actors(game,ctx->vga);
            }
            present(ctx,1);
        }
        if (stopped(ctx)) return 0;
        if (e->map_base) {
            if (battle_page_image(e->map_base,&e->map_fx[124],72+x,72+y,-1)!=0) return -1;
        } else blit(e->board.framebuffer+1284,312,192,320,&e->map_fx[124],x,y,-1);
        if (play_cue(ctx,&e->teleport_sound,e->sfx_enabled,(uint8_t)(phase==2))!=0) return -1;
        int center_y=(e->teleport_y-game->camera_cell_y)*24+15;
        for (int i=5;i>=0 && !stopped(ctx);i--) {
            if (teleport_mask(game,ctx,(size_t)i,(center_y/5)*i)!=0) return -1;
            present(ctx,0);if (!stopped(ctx)) fd2_delay_ms(10);
        }
        if (!stopped(ctx)) fd2_wait_ticks(2);
        return 0;
    }
    memcpy(e->work+64000,ctx->vga->framebuffer,64000);
    if (teleport_mask(game,ctx,0,0)!=0) return -1;
    memcpy(e->work,ctx->vga->framebuffer,64000);
    memcpy(ctx->vga->framebuffer,e->work+64000,64000);
    int x=(e->teleport_x-game->camera_cell_x)*24+4;
    int dy=e->teleport_y-game->camera_cell_y;
    int y=dy*24+4-(dy ? 6:0),height=dy ? 24:18;
    for (int row=0;row<height && !stopped(ctx);row++) {
        int yy=y+row;
        if (yy>=4 && yy<196) for (int col=0;col<24;col++) {
            int xx=x+col;
            if (xx>=4 && xx<316) ctx->vga->framebuffer[yy*320+xx]=e->work[yy*320+xx];
        }
        present(ctx,0);if (!stopped(ctx)) fd2_delay_ms(10);
    }
    for (size_t frame=0;frame<10 && !stopped(ctx);frame++) {
        if (teleport_mask(game,ctx,frame,0)!=0) return -1;
        present(ctx,1);
    }
    if (phase==3) e->next=e->count;
    return 0;
}

static int magic_cast(const fd2_field_game *game,const fd2_field_magic_present_event *event,
        fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (!e || event->actor_index!=e->caster_index || event->magic_id!=e->id)
        return -1;
    if (e->mode==FD2_FIELD_MAGIC_MODE_COMPOSITE && event->group_pass!=UINT8_MAX) {
        if (!e->cast_done) return -1;
        memset(e->popup_glyphs,0,sizeof(e->popup_glyphs));
        return magic_map_cast(game,event,ctx);
    }
    if (e->cast_done) return -1;
    e->cast_done=1;
    if (e->mode==FD2_FIELD_MAGIC_MODE_FULL) return magic_cast_full(game,event,ctx);
    if (e->mode==FD2_FIELD_MAGIC_MODE_DERIVED) return magic_special_intro(game,event,ctx);
    if (e->mode==FD2_FIELD_MAGIC_MODE_COMPOSITE) return magic_composite_cast(game,event,ctx);
    if (e->action_kind==FD2_FIELD_MAGIC_ACTION_MAGIC && e->effect_id>=13 && e->effect_id<=16 &&
        magic_heal_prelude(game,ctx,e->effect_id)!=0)
        return -1;
    return magic_map_cast(game,event,ctx);
}

static int magic_target_full(const fd2_field_game *game,const fd2_field_magic_present_event *event,
        fd2_battle_animation *ctx,size_t index) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;magic_fx_reader reader;
    if (magic_fx_reader_open(&reader,ctx,event)!=0) return -1;
    int duration,native_return;
    if (magic_fx_step(&reader,3,&duration)!=0 || duration<0 || duration>UINT16_MAX) return -1;
    e->idle_part=e->idle_ticks=0;
    size_t impact=0;unsigned hp_step=1;
    /* 2ff01取damage helper的raw返回；命中但raw0仍走无染色/无jitter分支。 */
    int damaging=event->result->raw_damage!=0;
    for (int frame=0;frame<duration;frame++) {
        if (e->id==3 || e->id==7 || e->id==9) e->background_flip^=1u;
        copy_pixels(e->work+reader.origin-640*e->background_flip,640,e->board.framebuffer,320);
        if (magic_fx_step(&reader,4,&native_return)!=0) return -1;
        if (e->id!=9) magic_work_part(e,reader.origin,&e->casting,
            e->casting.clip.part_count-1u,0,0,-1);
        if (damaging) {
            magic_work_idle(e,reader.origin,&e->target_idle[index],
                e->shake_sign*k_battle_magic_shake_x[e->shake_phase],k_battle_magic_shake_y[e->shake_phase],
                e->fx_parameters.hit_frame_base+(e->tint_phase<<8));
            if (--e->tint_phase==1) e->tint_phase=8;
            if (e->shake_phase!=3) e->shake_phase++;
        } else magic_work_idle(e,reader.origin,&e->target_idle[index],0,0,-1);
        if (magic_fx_step(&reader,5,&native_return)!=0) return -1;
        if (damaging && native_return==1) {
            if (impact>=event->impact_count) return -1;
            /* HP达到表526bc分母后保持；超出的原impact仍消费一项jitter。 */
            if (hp_step<=e->fx_parameters.hp_denominator) {
                int before=(int)(int16_t)event->target_before->hp;
                int after=(int)(int16_t)event->result->hp_after;
                e->units[index].hp=(uint16_t)(before-(int)hp_step*(before-after)/
                    e->fx_parameters.hp_denominator);
                if (magic_summary(e,game,&e->units[index],e->targets[index])!=0) return -1;
                hp_step++;
            }
            e->shake_sign=event->impact_jitter[impact++];e->shake_phase=0;
        }
        /* 当前页已画旧HP，刚更新的board从下一帧可见。 */
        magic_work_present(ctx,reader.origin,1);
    }
    if (impact!=event->impact_count) return -1;
    if (index+1<e->count) {
        int direction=e->caster.side ? -1:1;uint8_t flip=0;
        reader.origin=18880;
        for (int pass=0;pass<2;pass++)
            for (int j=pass ? 4:1;pass ? j>=0:j<5;j+=pass ? -1:1) {
                if (e->id==3 || e->id==7) flip^=1u;
                copy_pixels(e->work+reader.origin-640*flip,640,e->board.framebuffer,320);
                if (magic_fx_step(&reader,4,&native_return)!=0) return -1;
                /* 31266本身没有ID9省略caster的分支，不沿用主循环条件。 */
                magic_work_part(e,reader.origin,&e->casting,e->casting.clip.part_count-1u,0,0,-1);
                magic_work_part(e,reader.origin,&e->target_idle[index+(size_t)pass],0,direction*35*j,0,-1);
                if (magic_fx_step(&reader,5,&native_return)!=0) return -1;
                magic_work_present(ctx,reader.origin,1);
            }
        if (magic_summary(e,game,&e->units[index+1],e->targets[index+1])!=0) return -1;
    }
    return reader.used==reader.count ? 0:-1;
}

static int floating_values(const fd2_field_game *game,fd2_battle_animation *ctx);

/* field_popup_number_enqueue @0x1e0db（原sub_1E0DB）与
 * field_popup_miss_enqueue @0x1e1dc（原sub_1E1DC）：入队caller原量；
 * 正常typed producer非负，不把HP净差或贡献当原浮字数。原signed接口
 * 的任意负数及其线性字形owner资格另分账，不在此扩容或重抽RNG。 */
static void magic_popup_queue(fd2_battle_magic_exchange *e,size_t index,
        const fd2_field_magic_present_event *event) {
    uint8_t id=e->effect_id;
    int item=e->action_kind==FD2_FIELD_MAGIC_ACTION_ITEM;
    int kind=item ? k_battle_item_presentations[e->item_code-5][1]:
        id==25 || id==23 ? 0:(id<=12 || id==22 || id==26 || id==27 || id==32) ? 1:2;
    int zero_number=item ? k_battle_item_presentations[e->item_code-5][2]:
        !(id<=12 || id==32);
    int damage=kind==1;
    int miss=!event->result->hit || (damage && !event->result->raw_damage && !zero_number);
    if (miss) {
        for (size_t i=0;i<4;i++) {
            e->popup_glyphs[index][i]=k_battle_popup_miss[i];
            e->popup_x[index][i]=(uint8_t)(5*i+(i==1 ? 3:2));
        }
    } else if (kind) {
        char digits[16];
        uint32_t value=damage ? event->result->raw_damage:event->result->raw_heal;
        snprintf(digits,sizeof(digits),"%u",(unsigned)value);
        size_t length=strlen(digits),next=0;
        for (size_t i=0;i<4;i++) {
            e->popup_x[index][i]=(uint8_t)(5*i+2);
            if (length>3u-i) e->popup_glyphs[index][i]=(uint8_t)(
                (damage ? 94:105)+digits[next++]-'0');
        }
    }
}

static int magic_target(const fd2_field_game *game,const fd2_field_magic_present_event *event,
        fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (!e || !e->cast_done || e->next>=e->effect_count || !event->target_before || !event->result ||
        event->actor_index!=e->caster_index || event->magic_id!=e->id ||
        event->target_index!=e->targets[e->next] ||
        event->impact_count>FD2_FIELD_MAGIC_IMPACT_CAPACITY ||
        ((e->mode!=FD2_FIELD_MAGIC_MODE_FULL || !event->result->raw_damage) && event->impact_count)) return -1;
    for (size_t i=0;i<event->impact_count;i++)
        if (event->impact_jitter[i]<-1 || event->impact_jitter[i]>1) return -1;
    size_t index=e->next++;e->units[index]=*event->target_before;e->raw_heal[index]=event->result->raw_heal;
    if (e->mode==FD2_FIELD_MAGIC_MODE_FULL) return magic_target_full(game,event,ctx,index);
    if (e->mode==FD2_FIELD_MAGIC_MODE_DERIVED) {
        if (e->next_part!=e->casting.clip.part_count) return -1;
        e->next_part=e->casting.clip.intro_end;
        return 0;
    }
    magic_popup_queue(e,index,event);
    return e->mode==FD2_FIELD_MAGIC_MODE_COMPOSITE && e->next==e->effect_count ? floating_values(game,ctx):0;
}

/* sub_1DF58 @0x1df58消费上述两种入队glyph，不再抽spread或提交core。 */
static int floating_values(const fd2_field_game *game,fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (refresh_map(game,ctx,&e->board)!=0) return -1;
    int any=0;
    for (size_t i=0;i<e->count;i++) if (e->units[i].x>=game->camera_cell_x &&
        e->units[i].x<game->camera_cell_x+13 && e->units[i].y>=game->camera_cell_y-1 &&
        e->units[i].y<=game->camera_cell_y+8)
        for (size_t slot=0;slot<4;slot++) if (e->popup_glyphs[i][slot]) any=1;
    if (!any) return 0;
    if (magic_map_base(e,ctx->vga)!=0) return -1;
    for (size_t frame=0;frame<22 && !stopped(ctx);frame++) {
        memcpy(ctx->vga->framebuffer,e->board.framebuffer,64000);
        if (e->map_base && fd2_field_page_copy_pixels(ctx->vga->field_page,e->map_base)!=0) return -1;
        for (size_t i=0;i<e->count;i++) {
            const fd2_field_unit *u=&e->units[i];
            if (u->x<game->camera_cell_x || u->x>=game->camera_cell_x+13 ||
                u->y<game->camera_cell_y-1 || u->y>game->camera_cell_y+8) continue;
            for (size_t slot=0;slot<4;slot++) if (e->popup_glyphs[i][slot]) {
                const battle_image *digit=&e->digits[e->popup_glyphs[i][slot]];
                int x=((int)u->x-game->camera_cell_x)*24+e->popup_x[i][slot];
                int y=((int)u->y-game->camera_cell_y)*24+(int)k_battle_heal_curve[frame+slot]-3;
                if (e->map_base) {
                    if (battle_page_image(ctx->vga->field_page,digit,72+x,72+y,-1)!=0) return -1;
                } else blit(ctx->vga->framebuffer+1284,312,192,320,digit,x,y,-1);
            }
        }
        if (e->map_base && magic_map_project(ctx->vga)!=0) return -1;
        present(ctx,0);if (!stopped(ctx)) fd2_delay_ms(2);
    }
    if (!stopped(ctx)) fd2_delay_ms(500);
    return 0;
}

static int magic_finish_full(fd2_battle_animation *ctx,
        const fd2_field_magic_present_event *event) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;magic_fx_reader reader;
    if (!event || e->next!=e->count || magic_fx_reader_open(&reader,ctx,event)!=0) return -1;
    int duration,native_return;
    if (magic_fx_step(&reader,6,&duration)!=0 || duration<0 || duration>UINT16_MAX) return -1;
    for (int frame=0;frame<duration;frame++) {
        copy_pixels(e->work+reader.origin,640,e->board.framebuffer,320);
        if (magic_fx_step(&reader,7,&native_return)!=0) return -1;
        if (e->id!=9) magic_work_part(e,reader.origin,&e->casting,
            e->casting.clip.part_count-1u,0,0,-1);
        magic_work_idle(e,reader.origin,&e->target_idle[e->count-1],0,0,-1);
        if (magic_fx_step(&reader,8,&native_return)!=0) return -1;
        magic_work_present(ctx,reader.origin,1);
    }
    if (reader.used!=reader.count) return -1;
    if (e->id==9) for (int i=7;i>=0;i--) {
        copy_pixels(e->work+320,640,e->board.framebuffer,320);
        magic_work_part(e,320,&e->caster_idle,0,-10*i,0,-1);
        magic_work_part(e,320,&e->target_idle[0],0,0,0,-1);
        magic_work_present(ctx,320,0);
    }
    return 0;
}

static int magic_restore(fd2_battle_animation *ctx,const fd2_field_game *game,int normal,
        const fd2_field_magic_present_event *event) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;if (!e) return -1;
    int full=e->mode==FD2_FIELD_MAGIC_MODE_FULL;
    int derived=e->mode==FD2_FIELD_MAGIC_MODE_DERIVED;
    int cinematic=full || derived;
    int status=0;
    (void)stopped(ctx);
    int skipped_composite=e->mode==FD2_FIELD_MAGIC_MODE_COMPOSITE && ctx->skip;
    if (normal && e->mode==FD2_FIELD_MAGIC_MODE_MAP && e->cast_done &&
        ((e->next!=e->effect_count && !stopped(ctx)) || (e->action_kind==FD2_FIELD_MAGIC_ACTION_ITEM &&
            e->effect_id!=23 && !e->popup_done))) status=-1;
    if (normal && full && e->cast_done)
        status=magic_finish_full(ctx,event);
    if (normal && status==0 && !ctx->host_quit && !ctx->skip) {
        if (full && e->cast_done) {
            size_t first=(size_t)e->fx_parameters.return_lut_base+1u;
            for (size_t i=first;i<first+3u && !stopped(ctx);i++) {
                copy_pixels(e->work,320,e->board.framebuffer,320);
                if (sprite_lut(e,fd2_field_lut_frame(&game->visuals,i))!=0) {status=-1;break;}
                part_draw(e->work,320,200,320,&e->casting,0,0,0,-1);
                magic_idle(e,&e->target_idle[e->count-1],e->work,320,200,320,0,0,-1);
                memcpy(ctx->vga->framebuffer,e->work,64000);present(ctx,1);
            }
            if (status==0 && !stopped(ctx)) {
                copy_pixels(e->work,320,e->board.framebuffer,320);
                blit(e->board.framebuffer,320,200,320,&e->bg,0,50,-1);
                blit(e->board.framebuffer,320,200,320,&e->terrain,164,157,-1);
                part_draw(e->work,320,200,320,&e->casting,0,0,0,-1);
                magic_idle(e,&e->target_idle[e->count-1],e->work,320,200,320,0,0,-1);
                memcpy(ctx->vga->framebuffer,e->work,64000);present(ctx,0);
            }
        }
    }
    if (normal && status==0 && e->mode==FD2_FIELD_MAGIC_MODE_MAP && e->cast_done &&
        e->effect_id!=23 && !e->popup_done && !stopped(ctx) && floating_values(game,ctx)!=0) status=-1;
    /* MAP/Item的1d4cb→1d4f6同样拥有primary尾停，不让bank释放后仍悬挂本轮声源。 */
    if (ctx->primary && e->sfx_enabled) (void)fd2_pcm_stop(ctx->primary);
    if (normal && status==0 && (full || derived) && e->started && !stopped(ctx)) {
        fd2_wait_ticks(derived ? 6u:8u);fade(ctx,0);
    }
    if (cinematic || skipped_composite || !normal || ctx->host_quit || status<0) {
        memcpy(ctx->vga->palette,e->palette,768);
        /* 正常FULL/派生在黑DAC下刷新；不能先还回入场水色。 */
        if(!normal || !cinematic || !e->started || ctx->skip || ctx->host_quit || status<0)
            memcpy(ctx->vga->dac,e->dac,768);
    }
    /* 21190的11cac在浮字前（floating_values首部）；1df58/22253末尾
     * 保最后App页。复合2d80d的11cac(1)在MAP组前，2def4只经
     * 1d4f6 stop/free，不能在最后浮字后再刷新或恢复演出前DAC。
     * FULL/派生正常尾及显式skip仍按既有合同归还当前地图。 */
    if (normal && !ctx->host_quit && status==0) {
        if (cinematic || ctx->skip) {
            /* 2d7da/30df2 fade-out后，2d7eb/30e03清VGA再刷新。 */
            if(battle_field_return(game,ctx,0,cinematic && e->started && !ctx->skip)!=0) {
                status=-1;memcpy(ctx->vga->framebuffer,e->map,64000);
            }
        }
    } else memcpy(ctx->vga->framebuffer,e->map,64000);
    if (normal && status==0 && (full || derived) && e->started && !stopped(ctx)) fade(ctx,1);
    if (ctx->host_quit) memcpy(ctx->vga->framebuffer,e->map,64000);
    /* 1f525最后darkness0已写满base DAC；只有skip/错误/HOST回快照。 */
    if ((cinematic && (!e->started || ctx->skip)) || skipped_composite ||
        !normal || ctx->host_quit || status<0) memcpy(ctx->vga->dac,e->dac,768);
    magic_close_exchange(ctx);return status;
}

static int magic_dispatch(const fd2_field_game *game,
        const fd2_field_magic_present_event *event,void *userdata) {
    fd2_battle_animation *ctx=userdata;
    if (!game || !event || !ctx || !ctx->vga || !ctx->fdother || ctx->visual_owner!=game) return -1;
    if(event->phase==FD2_FIELD_MAGIC_PRESENT_MAP_CALLER_PREAMBLE)
        return magic_map_caller_preamble(game,event,ctx);
    if(event->phase==FD2_FIELD_MAGIC_PRESENT_AI_RAY_ITEM)
        return magic_ai_ray_item(game,event,ctx);
    if (ctx->magic_exchange && (event->actor_index!=ctx->magic_exchange->caster_index ||
        event->magic_id!=ctx->magic_exchange->id || event->action_kind!=ctx->magic_exchange->action_kind ||
        (event->action_kind==FD2_FIELD_MAGIC_ACTION_ITEM && event->item_code!=ctx->magic_exchange->item_code))) return -1;
    switch (event->phase) {
        case FD2_FIELD_MAGIC_PRESENT_PREPARE:return magic_prepare(game,event,ctx);
        case FD2_FIELD_MAGIC_PRESENT_MAP_WRAPPER_PRELUDE:return magic_map_wrapper_prelude(game,event,ctx);
        case FD2_FIELD_MAGIC_PRESENT_CAST:return magic_cast(game,event,ctx);
        case FD2_FIELD_MAGIC_PRESENT_TARGET:return magic_target(game,event,ctx);
        case FD2_FIELD_MAGIC_PRESENT_STEP:return magic_derived_step(game,event,ctx);
        case FD2_FIELD_MAGIC_PRESENT_TELEPORT_OUT:
        case FD2_FIELD_MAGIC_PRESENT_TELEPORT_OUT_COMMIT:
        case FD2_FIELD_MAGIC_PRESENT_TELEPORT_IN:
        case FD2_FIELD_MAGIC_PRESENT_TELEPORT_IN_COMMIT:
            return magic_teleport(game,event,ctx);
        case FD2_FIELD_MAGIC_PRESENT_POPUP_CLEAR:
            if (!ctx->magic_exchange || ctx->magic_exchange->mode!=FD2_FIELD_MAGIC_MODE_COMPOSITE) return -1;
            memset(ctx->magic_exchange->popup_glyphs,0,sizeof(ctx->magic_exchange->popup_glyphs));
            return 0;
        case FD2_FIELD_MAGIC_PRESENT_POPUP_GROUP: {
            fd2_battle_magic_exchange *e=ctx->magic_exchange;
            if (!e || e->action_kind!=FD2_FIELD_MAGIC_ACTION_ITEM || !e->cast_done ||
                e->effect_id==23 || e->next!=e->effect_count || e->popup_done) return -1;
            if (floating_values(game,ctx)!=0) return -1;
            e->popup_done=1;
            return 0;
        }
        case FD2_FIELD_MAGIC_PRESENT_FINISH:
            if (!ctx->magic_exchange) return -1;
            return magic_restore(ctx,game,!event->action_failed,event);
        default:return -1;
    }
}

int fd2_battle_animation_physical_present(const fd2_field_game *game,
        const fd2_field_physical_event *event,void *userdata) {
    int result=physical_dispatch(game,event,userdata);
    if (result<0 && userdata) ((fd2_battle_animation *)userdata)->failed=1;
    return result;
}

int fd2_battle_animation_magic_present(const fd2_field_game *game,
        const fd2_field_magic_present_event *event,void *userdata) {
    int result=magic_dispatch(game,event,userdata);
    if (result<0 && userdata) ((fd2_battle_animation *)userdata)->failed=1;
    /* C11普通Magic10..12的同步HOST seam；skip仍0，真实ERROR先返回。
     * FINISH不翻译HOST：其职责仅正常关闭现exchange，不重复业务或上传。 */
    if(result==0 && userdata && event &&
        event->presentation_mode==FD2_FIELD_MAGIC_MODE_MAP &&
        event->action_kind==FD2_FIELD_MAGIC_ACTION_MAGIC && event->magic_id>=10u && event->magic_id<=12u &&
        event->effect_id==event->magic_id &&
        (event->phase==FD2_FIELD_MAGIC_PRESENT_MAP_WRAPPER_PRELUDE ||
         event->phase==FD2_FIELD_MAGIC_PRESENT_CAST || event->phase==FD2_FIELD_MAGIC_PRESENT_TARGET)) {
        fd2_battle_animation *ctx=userdata;
        /* MAP TARGET只入浮字队列，无present/stopped；此处仅收HOST，不pump/读普通FIFO。 */
        (void)stopped(ctx);
        if(ctx->host_quit)return 1;
    }
    return result;
}

void fd2_battle_animation_close(fd2_battle_animation *ctx) {
    if (!ctx) return;
    if (ctx->exchange && ctx->vga) restore(ctx,NULL,0);
    if (ctx->magic_exchange && ctx->vga) (void)magic_restore(ctx,NULL,0,NULL);
    fd2_archive_close(&ctx->backgrounds);
    fd2_archive_close(&ctx->terrain_images);memset(ctx,0,sizeof(*ctx));
}

/* sub_2FB2C @0x2fb2c / sub_2FE14 @0x2fe14（原登记同名）：
 * 冷图调度有限视觉阶段，BG/FIGANI只借App，整个scope不读写单位或取RNG。 */
struct fd2_prepare_promotion {
    fd2_vga *vga;
    battle_image background[3];
    battle_clip old_idle,new_idle;
    uint8_t saved[64000],palette[768],dac[768],work[128000];
    int status;
};

static int promotion_quit(fd2_prepare_promotion *scope) {
    if (fd2_input_take_quit(&scope->vga->input)) scope->status=-2;
    return scope->status;
}

static int promotion_present(fd2_prepare_promotion *scope,unsigned wait) {
    if (promotion_quit(scope)) return scope->status;
    fd2_input_begin_frame(&scope->vga->input);
    fd2_vga_upload_current_frame(scope->vga);
    if (!promotion_quit(scope) && wait) fd2_wait_ticks(wait);
    return promotion_quit(scope);
}

static int promotion_fade(fd2_prepare_promotion *scope,int in) {
    for (int i=0;i<(in ? 65:64);i++) {
        if (promotion_quit(scope)) return scope->status;
        apply_darkness_vga(scope->vga,in ? 64-i:i);
        if (promotion_present(scope,0)) return scope->status;
        fd2_delay_ms(2);
    }
    return promotion_quit(scope);
}

static void promotion_frame(fd2_prepare_promotion *scope,const battle_clip *clip,
        size_t part,unsigned background,unsigned x) {
    memset(scope->work,0,sizeof(scope->work));
    blit(scope->work,640,200,640,&scope->background[background],0,50,-1);
    /* 16→48的FIGANI144为3B empty哨兵。完整owner写集已定首2ebb9：
     * payload8..19B内目录未初始化或读越owner；此后H省sprite，不伪造原帧。
     * 不能仅凭3B判OOB，也不能把后继BG投影签成原有定义前缀。 */
    if (!clip->clip.empty) part_draw(scope->work,640,200,640,clip,part,(int)x,0,-1);
    copy_pixels(scope->vga->framebuffer,320,scope->work,640);
}

static int promotion_idle_open(battle_clip *out,const fd2_archive *figani,size_t entry) {
    if (fd2_figani_open_entry(&out->clip,figani,entry)!=0) return -1;
    return out->clip.empty ? 0:clip_open_visual(out,figani,entry);
}

int fd2_prepare_promotion_open(fd2_prepare_promotion **out,fd2_vga *vga,
        const fd2_archive *backgrounds,const fd2_archive *figani,
        uint8_t old_unit_id,uint8_t new_unit_id) {
    if (!out || *out || !vga || !backgrounds || !figani) return -1;
    fd2_prepare_promotion *scope=calloc(1,sizeof(*scope));
    if (!scope) return -1;
    scope->vga=vga;
    memcpy(scope->saved,vga->framebuffer,sizeof(scope->saved));
    memcpy(scope->palette,vga->palette,sizeof(scope->palette));
    memcpy(scope->dac,vga->dac,sizeof(scope->dac));
    if (promotion_idle_open(&scope->old_idle,figani,(size_t)old_unit_id*3u)!=0 ||
        promotion_idle_open(&scope->new_idle,figani,(size_t)new_unit_id*3u)!=0) goto fail;
    for (size_t i=0;i<3;i++)
        if (image_entry(&scope->background[i],backgrounds,i)!=0 ||
            scope->background[i].image.width>640 || scope->background[i].image.height>200) goto fail;
    *out=scope;return 0;
fail:
    fd2_prepare_promotion_close(scope,0);return -1;
}

int fd2_prepare_promotion_step(fd2_prepare_promotion *scope,
        fd2_prepare_promotion_phase phase,unsigned value) {
    if (!scope || !scope->vga) return -1;
    if (scope->status) return scope->status;
    /* 参数先全验，坏冷节点不改视觉；阶段顺序只属于caller冷图。 */
    if ((phase==FD2_PREPARE_PROMOTION_BEGIN && value>255u) ||
        (phase==FD2_PREPARE_PROMOTION_OLD_ENTER && (!value || value>33u)) ||
        ((phase==FD2_PREPARE_PROMOTION_OLD_IDLE || phase==FD2_PREPARE_PROMOTION_NEW_IDLE) && value>4096u) ||
        (phase==FD2_PREPARE_PROMOTION_WHITE && value>256u) ||
        (phase==FD2_PREPARE_PROMOTION_NEW_FRAME && value>320u) ||
        (phase==FD2_PREPARE_PROMOTION_RESTORE && value) ||
        (unsigned)phase>FD2_PREPARE_PROMOTION_RESTORE) return -1;
    if (promotion_quit(scope)) return scope->status;
    if (phase==FD2_PREPARE_PROMOTION_BEGIN) {
        if (promotion_fade(scope,0)) return scope->status;
        for (unsigned i=0;i<value;i++) {
            if (promotion_quit(scope)) return scope->status;
            fd2_wait_ticks(1);
        }
    } else if (phase==FD2_PREPARE_PROMOTION_OLD_ENTER) {
        unsigned background=0;
        for (unsigned i=value;i-->0;) {
            if (promotion_quit(scope)) return scope->status;
            background=(background+1u)%3u;
            promotion_frame(scope,&scope->old_idle,0,background,i*10u);
            apply_darkness_vga(scope->vga,(int)(6u*i));
            if (promotion_present(scope,0)) return scope->status;
        }
    } else if (phase==FD2_PREPARE_PROMOTION_OLD_IDLE || phase==FD2_PREPARE_PROMOTION_NEW_IDLE) {
        const battle_clip *clip=phase==FD2_PREPARE_PROMOTION_OLD_IDLE ? &scope->old_idle:&scope->new_idle;
        unsigned background=0,part=0,age=0;
        for (unsigned i=0;i<value;i++) {
            if (promotion_quit(scope)) return scope->status;
            background=(background+1u)%3u;
            promotion_frame(scope,clip,part,background,0);
            /* 2fe14 duration0不补1，也不按BYTE回绕age；源有界tick中保持原part。 */
            if (!clip->clip.empty && ++age==clip->parts[part].part.duration) {
                age=0;if (++part==clip->clip.part_count) part=0;
            }
            if (promotion_present(scope,1)) return scope->status;
        }
    } else if (phase==FD2_PREPARE_PROMOTION_WHITE) {
        for (unsigned step=0;step<value;step++) {
            if (promotion_quit(scope)) return scope->status;
            for (size_t i=0;i<768;i++) {
                unsigned channel=scope->vga->palette[i]+step*3u;
                if (channel>63u) channel=63u;
                scope->vga->dac[i]=(uint8_t)((channel<<2)|(channel>>4));
            }
            if (promotion_present(scope,0)) return scope->status;
            fd2_delay_ms(10);
        }
    } else if (phase==FD2_PREPARE_PROMOTION_NEW_FRAME) {
        promotion_frame(scope,&scope->new_idle,0,0,value);
        apply_darkness_vga(scope->vga,0);
        return promotion_present(scope,0);
    } else if (phase==FD2_PREPARE_PROMOTION_RESTORE) {
        if (promotion_fade(scope,0)) return scope->status;
        memcpy(scope->vga->framebuffer,scope->saved,sizeof(scope->saved));
        return promotion_fade(scope,1);
    }
    return promotion_quit(scope);
}

void fd2_prepare_promotion_close(fd2_prepare_promotion *scope,int restore_visual) {
    if (!scope) return;
    if (restore_visual && scope->vga) {
        memcpy(scope->vga->framebuffer,scope->saved,sizeof(scope->saved));
        memcpy(scope->vga->palette,scope->palette,sizeof(scope->palette));
        memcpy(scope->vga->dac,scope->dac,sizeof(scope->dac));
    }
    clip_close(&scope->old_idle);clip_close(&scope->new_idle);
    for (size_t i=0;i<3;i++) image_close(&scope->background[i]);
    free(scope);
}

/* 31529实际20组全部non-split；这里只借同一RLE/工作页单帧核心。
 * 原2e9a8最后memmove保存ESI，使2ebe1 var4C为0xAF8C0；其HP写和
 * 3%重复仍是冷图业务，不在本renderer模拟栈或构造physical事件。 */
struct fd2_battle_montage {
    fd2_battle_animation render;
    unsigned intro_zero, intro_finished;
};

void fd2_battle_montage_close(fd2_battle_montage **owner) {
    if(!owner || !*owner)return;
    fd2_battle_montage *scope=*owner;
    exchange_close(&scope->render);
    fd2_archive_close(&scope->render.backgrounds);
    fd2_archive_close(&scope->render.terrain_images);
    free(scope);*owner=NULL;
}

int fd2_battle_montage_open(fd2_battle_montage **out,fd2_vga *vga,
        const fd2_archive *figani,const fd2_field_unit actors[2],
        uint8_t first,uint8_t second,uint8_t scene) {
    if(!out || *out || !vga || !figani || !actors || first>127u || second>127u ||
        !scene || scene>63u || actors[0].unit_id!=first || actors[1].unit_id!=second ||
        actors[0].side!=(first<76u?2u:0u) || actors[1].side!=(second<76u?2u:0u))return -1;
    fd2_battle_montage *scope=calloc(1,sizeof(*scope));if(!scope)return -1;
    fd2_battle_animation *ctx=&scope->render;ctx->vga=vga;ctx->figani=figani;
    ctx->exchange=calloc(1,sizeof(*ctx->exchange));
    if(!ctx->exchange || ensure_backgrounds(ctx)!=0)goto fail;
    fd2_battle_animation_exchange *e=ctx->exchange;
    e->units[0]=actors[0];e->units[1]=actors[1];e->last_attacker=-1;e->tint=-1;
    uint8_t ids[2]={first,second};
    for(size_t half=0;half<2;half++) {
        if(clip_open_visual(&e->idle[half],figani,(size_t)ids[half]*3u)!=0 ||
            clip_open_visual(&e->attack[half],figani,(size_t)ids[half]*3u+1u)!=0 ||
            e->attack[half].clip.split_mode || e->attack[half].clip.part_count>22u)goto fail;
        unsigned markers=0;
        for(size_t part=0;part<e->attack[half].clip.part_count;part++) {
            const fd2_figani_part *p=&e->attack[half].parts[part].part;
            if(p->duration>10u)goto fail;
            markers+=p->effect_marker!=0;
        }
        if(!markers)goto fail;
    }
    size_t terrain=first==26u || first==54u || second==55u?3u:scene;
    if(image_entry(&e->bg[0],&ctx->backgrounds,scene)!=0 ||
        image_entry(&e->terrain,&ctx->terrain_images,terrain)!=0)goto fail;
    blit(e->board.framebuffer,320,200,320,&e->bg[0],0,50,-1);
    *out=scope;return 0;
fail:
    fd2_battle_montage_close(&scope);return -1;
}

int fd2_battle_montage_intro(fd2_battle_montage *scope,unsigned phase) {
    if(!scope || phase>8u || scope->intro_finished)return -1;
    fd2_battle_animation *ctx=&scope->render;fd2_battle_animation_exchange *e=ctx->exchange;
    int side=e->units[0].side!=0,base=side?0:320;
    if(!side && phase==8u)blit(e->board.framebuffer,320,200,320,&e->terrain,164,157,-1);
    copy_pixels(e->work+base,640,e->board.framebuffer,320);
    if(side)part_draw(e->work,640,200,640,&e->idle[1],0,0,0,-1);
    if(side)blit(e->work,640,200,640,&e->terrain,164+10*(int)phase,157,-1);
    part_draw(e->work,640,200,640,&e->idle[0],0,base+(side?10:-10)*(int)phase,0,-1);
    if(!side)part_draw(e->work,640,200,640,&e->idle[1],0,320,0,-1);
    copy_pixels(ctx->vga->framebuffer,320,e->work+base,640);
    if(!phase)scope->intro_zero=1;
    return 0;
}
int fd2_battle_montage_intro_finish(fd2_battle_montage *scope) {
    if(!scope || !scope->intro_zero || scope->intro_finished)return -1;
    fd2_battle_animation_exchange *e=scope->render.exchange;
    if(e->units[0].side)blit(e->board.framebuffer,320,200,320,&e->terrain,164,157,-1);
    scope->intro_finished=1;return 0;
}
int fd2_battle_montage_half(fd2_battle_montage *scope,unsigned half) {
    if(!scope || !scope->intro_finished || half>1u)return -1;
    fd2_battle_animation_exchange *e=scope->render.exchange;
    e->last_attacker=(int)half;e->idle_part=e->idle_ticks=e->shake=0;e->tint=-1;return 0;
}
int fd2_battle_montage_current_half(const fd2_battle_montage *scope,unsigned *half) {
    if(!scope || !half || scope->render.exchange->last_attacker<0)return -1;
    *half=(unsigned)scope->render.exchange->last_attacker;return 0;
}
int fd2_battle_montage_part_count(const fd2_battle_montage *scope,size_t *count) {
    unsigned half;if(!count || fd2_battle_montage_current_half(scope,&half)!=0)return -1;
    *count=scope->render.exchange->attack[half].clip.part_count;return 0;
}
int fd2_battle_montage_part_info(const fd2_battle_montage *scope,size_t index,
        uint8_t *marker,uint8_t *ticks,int *last_marker) {
    unsigned half;if(!marker || !ticks || !last_marker ||
        fd2_battle_montage_current_half(scope,&half)!=0)return -1;
    const battle_clip *clip=&scope->render.exchange->attack[half];
    if(index>=clip->clip.part_count)return -1;
    size_t last=clip->clip.part_count;
    while(last && !clip->parts[last-1u].part.effect_marker)last--;
    *marker=clip->parts[index].part.effect_marker;*ticks=clip->parts[index].part.duration;
    *last_marker=last && index==last-1u;return 0;
}
int fd2_battle_montage_hit(fd2_battle_montage *scope) {
    unsigned half;if(fd2_battle_montage_current_half(scope,&half)!=0)return -1;
    scope->render.exchange->shake=5;scope->render.exchange->tint=33;return 0;
}
int fd2_battle_montage_frame(fd2_battle_montage *scope,size_t part) {
    unsigned half;uint8_t marker,ticks;int last;
    if(fd2_battle_montage_current_half(scope,&half)!=0 ||
        fd2_battle_montage_part_info(scope,part,&marker,&ticks,&last)!=0)return -1;
    fd2_battle_animation_exchange *e=scope->render.exchange;
    full_tick_draw(&scope->render,&e->attack[half],&e->idle[1u-half],part,e->units[half].side!=0);
    full_tick_advance(e,&e->idle[1u-half]);return 0;
}
