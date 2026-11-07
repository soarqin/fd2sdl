/* B6物理同步演出：sub_2E2B0 @0x2e2b0、sub_2EBE1 @0x2ebe1、
 * sub_2E9A8 @0x2e9a8、sub_2EB9F @0x2eb9f、sub_2F4D4 @0x2f4d4、
 * sub_2F631 @0x2f631。影像目录由figani解析；业务与RNG不在本文件执行。
 * 表VA0x52470/0x5266c/0x52684/0x526e3已由只读Domain与relocated字节核对。 */
#include "battle_animation.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_attack.h"
#include "field_game.h"
#include "field_info.h"
#include "figani.h"
#include "image.h"
#include "magic_fx.h"
#include "portrait.h"

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
    size_t actor[2];
    fd2_field_unit units[2];
    battle_clip idle[2], attack[2];
    battle_image bg[2], transition[3], terrain;
    fd2_vga board; /* 同一summary painter的离屏页，非输入owner。 */
    uint8_t work[128000]; /* 原640x200与400stride/40px守卫共用。 */
    uint8_t map[64000], palette[768], dac[768];
    int counter_ready;
    int sfx_enabled;
    int started;
    int last_attacker;
    uint8_t idle_part, idle_ticks, shake;
    int tint;
};

static const int shake_x[6] = {0,4,9,14,18,14};
static const int shake_y[6] = {0,2,4,6,8,10};

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
    fd2_vga_present(ctx->vga);
    if (!stopped(ctx) && ticks) fd2_wait_ticks(ticks);
    (void)stopped(ctx);
}

static int play_cue(fd2_battle_animation *ctx,const fd2_pcm_bank *bank,int enabled,uint8_t index) {
    if (stopped(ctx) || !ctx->primary || !enabled) return 0;
    return fd2_pcm_play_replace(ctx->primary,bank,index,1,1.0f);
}

static int cue(fd2_battle_animation *ctx, const battle_clip *clip, uint8_t index) {
    return play_cue(ctx,&clip->bank,ctx->exchange->sfx_enabled,index);
}

static void part_draw(uint8_t *dst, int width, int height, int stride,
        const battle_clip *clip, size_t index, int x, int y, int tint) {
    const battle_part *p=&clip->parts[index];
    blit(dst,width,height,stride,&p->drawing,x+p->part.x,y+p->part.y,tint);
}

static int summary(fd2_battle_animation_exchange *e, const fd2_field_game *game,
        const fd2_field_unit *unit) {
    return fd2_field_unit_summary_draw_at(&e->board,&game->info_assets,&game->font,
        &game->ui_text,unit,unit->side ? 171:0,unit->side ? 4:154);
}

static int scene_index(const fd2_field_game *game, const fd2_field_unit *unit,
        size_t *out) {
    if (!game->map.cells || unit->x >= game->map.width || unit->y >= game->map.height)
        return -1;
    const fd2_terrain_attr *attr=fd2_terrain_attr_get(&game->terrain,
        fd2_field_cell_terrain(fd2_field_map_cell(&game->map,unit->x,unit->y)));
    if (!attr) return -1;
    /* 2e2b0/52470：仅stage0/1已开放，两行均3；非免疫读12e38输出+6(attr2)。 */
    *out=fd2_field_attack_unit_ignores_terrain_modifier(unit) ? 3u:attr->attr2;
    return 0;
}

static int prepare(const fd2_field_game *game, const fd2_field_physical_event *event,
        fd2_battle_animation *ctx) {
    const uint8_t *palette; size_t size;
    if (ctx->exchange || ctx->magic_exchange || game->stage > 1 || event->actor_index >= game->units.count ||
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
    size_t scenes[2];
    for (size_t i=0;i<2;i++) {
        if (e->units[i].side>2 || fd2_field_unit_is_hidden(&e->units[i]) ||
            scene_index(game,&e->units[i],&scenes[i]) != 0 ||
            clip_open(&e->idle[i],ctx,3u*e->units[i].unit_id,0) != 0 ||
            summary(e,game,&e->units[i]) != 0) goto fail;
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
    if (summary(e,game,&e->units[0]) != 0) goto fail;
    blit(e->board.framebuffer,320,200,320,&e->bg[split ? 1:0],0,50,-1);
    if (!split && summary(e,game,&e->units[1]) != 0) goto fail;
    memcpy(e->map,ctx->vga->framebuffer,64000);
    memcpy(e->palette,ctx->vga->palette,768); memcpy(e->dac,ctx->vga->dac,768);
    e->last_attacker=-1; e->tint=-1;
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
static void apply_darkness(fd2_battle_animation *ctx, int darkness) {
    for (size_t i=0;i<768;i++) {
        uint8_t v=(uint8_t)(ctx->vga->palette[i]>darkness ? ctx->vga->palette[i]-darkness:0);
        ctx->vga->dac[i]=(uint8_t)((v<<2)|(v>>4));
    }
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
static int pan(fd2_battle_animation *ctx, const fd2_field_game *game,
        int actor, const battle_clip *idle, const battle_image *bg, int right) {
    fd2_battle_animation_exchange *e=ctx->exchange;
    for (int n=right ? 1:9; right ? n<10:n>=0; n+=right ? 1:-1) {
        if (stopped(ctx)) return 0;
        blit(e->work,640,200,640,&e->transition[n%3],right ? 320:0,50,-1);
        copy_view(ctx,640,32*n); present(ctx,0);
    }
    memset(e->work,0,sizeof(e->work)); memset(e->board.framebuffer,0,64000);
    blit(e->board.framebuffer,320,200,320,bg,0,50,-1);
    if (right) blit(e->board.framebuffer,320,200,320,&e->terrain,164,157,-1);
    if (summary(e,game,&e->units[actor]) != 0) return -1;
    copy_board(e,640,right ? 320:0);
    part_draw(e->work,640,200,640,idle,0,right ? 320:0,0,-1);
    for (int n=right ? 1:9; right ? n<=10:n>=0; n+=right ? 1:-1) {
        if (stopped(ctx)) return 0;
        int overlay=right ? (n+1)%3:(n+2)%3;
        blit(e->work,640,200,640,&e->transition[overlay],right ? 0:320,50,-1);
        copy_view(ctx,640,32*n); present(ctx,0);
    }
    return 0;
}

static void flash(fd2_battle_animation *ctx, uint8_t r, uint8_t g, uint8_t b) {
    if (stopped(ctx)) return;
    ctx->vga->dac[0]=(uint8_t)((r<<2)|(r>>4));
    ctx->vga->dac[1]=(uint8_t)((g<<2)|(g>>4));
    ctx->vga->dac[2]=(uint8_t)((b<<2)|(b>>4)); present(ctx,0);
    if (!stopped(ctx)) fd2_delay_ms(20);
    memset(ctx->vga->dac,0,3);
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
            if (summary(e,game,&e->units[target]) != 0) return -1;
            if (p->cue && cue(ctx,attack,event->result->hit ? p->cue:0) != 0) return -1;
            if (event->result->hit) { e->shake=5; e->tint=33; }
        } else if (p->cue && cue(ctx,attack,p->cue) != 0) return -1;
        for (unsigned tick=0;tick<p->duration;tick++) {
            if (stopped(ctx)) return 0;
            int dx=shake_x[e->shake],dy=attack->clip.split_mode ? 0:shake_y[e->shake];
            copy_board(e,400,16040);
            int first=side ? (p->layer_flags&1):!(p->layer_flags&1);
            if (first) part_draw(e->work,400,320,400,attack,i,40,40,-1);
            part_draw(e->work,400,320,400,idle,e->idle_part,
                40+(side ? -dx:dx),40+(side ? -dy:dy),e->tint);
            if (!first) part_draw(e->work,400,320,400,attack,i,40,40,-1);
            copy_view(ctx,400,16040);
            if (p->effect_marker==1 && seen==markers) {
                if (event->pre_hit_status_applied) flash(ctx,1,32,0);
                if (event->result->critical) {
                    flash(ctx,63,63,63);
                    present(ctx,0); /* 原DAC黑写须先上传，不能让白纹理覆盖后40ms。 */
                    if (!stopped(ctx)) fd2_delay_ms(40);
                }
            }
            present(ctx,1);
            if (++e->idle_ticks==idle->parts[e->idle_part].part.duration) {
                e->idle_ticks=0;
                if (++e->idle_part==idle->clip.part_count) e->idle_part=0;
            }
            if (e->shake) e->shake--;
            e->tint=-1;
        }
    }
    return 0;
}

static int restore(fd2_battle_animation *ctx, const fd2_field_game *game, int animate) {
    fd2_battle_animation_exchange *e=ctx->exchange;
    if (!e) return -1;
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
    memcpy(ctx->vga->dac,e->dac,768);
    /* 2e2b0的11cac(1)重画当前地图，不能复显PREPARE前TARGETING/菜单。
     * 2e2b0普通战场的统一尾仍执行11cac；skip只跳演出，normal返回仍推进。
     * 错误/quit仅恢复退出快照，不把它作为正常战斗返回帧。 */
    if (animate && !ctx->host_quit) {
        if (fd2_field_game_tick_visual(ctx->visual_owner,SDL_GetTicks())!=0)
            status=-1;
        if (status==0) fd2_field_game_render_battle_return(game,ctx->vga);
        else memcpy(ctx->vga->framebuffer,e->map,64000);
    } else memcpy(ctx->vga->framebuffer,e->map,64000);
    /* 正常2e2b0只停止primary；secondary是独立AIL handle，不作额外停止。 */
    if (ctx->primary && e->sfx_enabled) (void)fd2_pcm_stop(ctx->primary);
    if (animate && e->started && !stopped(ctx)) fade(ctx,1);
    if (ctx->host_quit) memcpy(ctx->vga->framebuffer,e->map,64000);
    memcpy(ctx->vga->dac,e->dac,768);
    exchange_close(ctx);
    return status;
}


static int physical_dispatch(const fd2_field_game *game,
        const fd2_field_physical_event *event, void *userdata) {
    fd2_battle_animation *ctx=userdata;
    if (!game || !event || !ctx || !ctx->vga || !ctx->fdother || ctx->visual_owner!=game) return -1;
    switch (event->phase) {
        case FD2_FIELD_PHYSICAL_PREPARE: return prepare(game,event,ctx);
        case FD2_FIELD_PHYSICAL_STRIKE: return strike(game,event,ctx);
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
    size_t caster_index, count, next;
    fd2_field_unit caster, units[FD2_FIELD_MAX_UNITS];
    battle_clip caster_idle, casting, target_idle[FD2_FIELD_MAX_UNITS], particles;
    battle_image bg, terrain, map_fx[7], digits[10];
    fd2_pcm_bank sound;
    fd2_vga board;
    uint8_t work[172800]; /* 2ff01原2a300：640stride带30行/160px守卫。 */
    uint8_t map[64000], palette[768], dac[768];
    uint32_t raw_heal[FD2_FIELD_MAX_UNITS];
    fd2_magic_fx fx;
    fd2_magic_fx_parameters fx_parameters;
    uint8_t part_durations[255];
    int sfx_enabled, cast_done, started;
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
        const fd2_field_unit *unit) {
    return fd2_field_unit_summary_draw_at(&e->board,&game->info_assets,&game->font,
        &game->ui_text,unit,unit->side ? 171:0,unit->side ? 4:154);
}

static void magic_close_exchange(fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (!e) return;
    clip_close(&e->caster_idle);clip_close(&e->casting);clip_close(&e->particles);
    for (size_t i=0;i<e->count;i++) clip_close(&e->target_idle[i]);
    for (size_t i=0;i<7;i++) image_close(&e->map_fx[i]);
    for (size_t i=0;i<10;i++) image_close(&e->digits[i]);
    image_close(&e->bg);image_close(&e->terrain);fd2_pcm_bank_close(&e->sound);
    free(e);ctx->magic_exchange=NULL;
}

static int magic_prepare(const fd2_field_game *game,const fd2_field_magic_present_event *event,
        fd2_battle_animation *ctx) {
    if (ctx->exchange || ctx->magic_exchange || game->stage>1 ||
        (event->magic_id>=10 && event->magic_id!=13) || !event->targets ||
        !event->target_count || event->target_count>FD2_FIELD_MAX_UNITS ||
        (event->magic_id==9 && event->target_count!=1) ||
        event->actor_index>=game->units.count) return -1;
    const uint8_t *pal;size_t pal_size;
    if (fd2_archive_get(ctx->fdother,0,&pal,&pal_size)!=0 || pal_size<768) return -1;
    fd2_battle_magic_exchange *e=calloc(1,sizeof(*e));if (!e) return -1;
    ctx->magic_exchange=e;e->id=event->magic_id;e->caster_index=event->actor_index;
    e->caster=game->units.items[e->caster_index];e->count=event->target_count;
    e->sfx_enabled=game->option_values[1]!=0;
    size_t caster_scene;
    if (fd2_field_unit_is_hidden(&e->caster) || scene_index(game,&e->caster,&caster_scene)!=0) goto fail;
    for (size_t i=0;i<e->count;i++) {
        size_t raw=event->targets[i];
        if (raw>=game->units.count) goto fail;
        for (size_t j=0;j<i;j++) if (e->targets[j]==raw) goto fail;
        e->targets[i]=(uint8_t)raw;e->units[i]=game->units.items[raw];
        size_t ignored;
        if (fd2_field_unit_is_hidden(&e->units[i]) || scene_index(game,&e->units[i],&ignored)!=0)
            goto fail;
    }
    if (e->id<10) {
        if (ensure_backgrounds(ctx)!=0) goto fail;
        /* 2ff01真实slot2→首WORD0才slot1，不借物理option。Game亦查询同一元数据。 */
        fd2_figani_magic_cast plan;
        if (fd2_figani_magic_cast_plan(&plan,ctx->figani,e->caster.unit_id)!=0 ||
            plan.clip.empty || plan.cost_marker_count!=event->cast_cost_count) goto fail;
        if (clip_open_visual(&e->casting,ctx->figani,plan.selected_entry)!=0 ||
            e->casting.clip.intro_end>e->casting.clip.part_count ||
            clip_open_visual(&e->caster_idle,ctx->figani,3u*e->caster.unit_id)!=0 ||
            fd2_magic_fx_parameters_get(e->id,e->caster.side,&e->fx_parameters)!=0 ||
            clip_open_visual(&e->particles,ctx->fdother,e->fx_parameters.effect_entry)!=0 ||
            fd2_pcm_bank_open(&e->sound,ctx->fdother,e->fx_parameters.sfx_bank)!=0) goto fail;
        for (size_t i=0;i<e->fx_parameters.minimum_sample_count;i++)
            if (!bank_sample_valid(&e->sound,i)) goto fail;
        for (size_t i=0;i<e->particles.clip.part_count;i++)
            e->part_durations[i]=e->particles.parts[i].part.duration;
        fd2_magic_fx_parts fx_parts={e->part_durations,e->particles.clip.part_count};
        if (fd2_magic_fx_init(&e->fx,e->id,e->caster.side,fx_parts)!=0) goto fail;
        e->shake_phase=3;e->shake_sign=-1;e->tint_phase=8;
        size_t target_scene=3; /* 30e25逆序，最低raw的非免疫场景最终保留。 */
        for (size_t i=e->count;i>0;i--) {
            if (!fd2_field_attack_unit_ignores_terrain_modifier(&e->units[i-1]) &&
                scene_index(game,&e->units[i-1],&target_scene)!=0) goto fail;
            if (clip_open_visual(&e->target_idle[i-1],ctx->figani,3u*e->units[i-1].unit_id)!=0 ||
                magic_summary(e,game,&e->units[i-1])!=0) goto fail;
        }
        if (image_entry(&e->bg,&ctx->backgrounds,e->caster.side ? target_scene:caster_scene)!=0 ||
            image_entry(&e->terrain,&ctx->terrain_images,e->caster.side ? caster_scene:target_scene)!=0)
            goto fail;
        for (size_t i=e->fx_parameters.return_lut_base;
             i<=(size_t)e->fx_parameters.return_lut_base+3u;i++)
            if (!fd2_field_lut_frame(&game->visuals,i)) goto fail;
        memset(e->board.framebuffer,0,64000);
        if (magic_summary(e,game,&e->caster)!=0 || magic_summary(e,game,&e->units[0])!=0) goto fail;
        blit(e->board.framebuffer,320,200,320,&e->bg,0,50,-1);
    } else {
        if (event->cast_cost_count!=1 || fd2_pcm_bank_open(&e->sound,ctx->fdother,80)!=0 ||
            !bank_sample_valid(&e->sound,11) || !bank_sample_valid(&e->sound,12) ||
            !bank_sample_valid(&e->sound,1)) goto fail;
        for (size_t i=0;i<7;i++) if (lmi_image(&e->map_fx[i],ctx->fdother,6,57+i)!=0) goto fail;
        for (size_t i=0;i<10;i++) if (lmi_image(&e->digits[i],ctx->fdother,5,105+i)!=0) goto fail;
        for (size_t i=1;i<=9;i++) {
            const uint8_t *lut=fd2_field_lut_frame(&game->visuals,i);
            if (!lut || fd2_field_game_render_transition_mask(game,&e->board,lut,
                (game->cursor_cell_x-game->camera_cell_x)*24+12,
                (game->cursor_cell_y-game->camera_cell_y)*24+16,1)!=0) goto fail;
        }
        /* 1c2da忽略direction/walk，phase3取2；使用现ID→FDICON资源owner。 */
        for (size_t i=0;i<e->count;i++) {
            if (!game->sprites.decoded_frames) goto fail;
            for (unsigned phase=0;phase<4;phase++) {
                size_t frame=(size_t)e->units[i].unit_id*12u+(phase==3 ? 2u:phase);
                if (frame>=game->sprites.frame_count) goto fail;
            }
        }
    }
    memcpy(e->map,ctx->vga->framebuffer,64000);memcpy(e->palette,ctx->vga->palette,768);
    memcpy(e->dac,ctx->vga->dac,768);return 0;
fail:
    magic_close_exchange(ctx);return -1;
}

/* 311e5用>=而物理2ebe1用==；duration0在这里每次前进，不混两种规则。 */
static void magic_idle(fd2_battle_magic_exchange *e,const battle_clip *clip,
        uint8_t *dst,int width,int height,int stride,int x,int y,int tint) {
    part_draw(dst,width,height,stride,clip,e->idle_part,x,y,tint);
    if (++e->idle_ticks>=clip->parts[e->idle_part].part.duration) {
        e->idle_ticks=0;if (++e->idle_part>=clip->clip.part_count) e->idle_part=0;
    }
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
        /* 原172800B工作页含守卫；以整页裁剪，不能把origin后的内存误当270行。 */
        part_draw(e->work,640,270,640,&e->particles,event->index,
            r->origin%640+event->x,r->origin/640+event->y,-1);
        return 0;
    }
    fd2_pcm_player *player;
    if (event->kind==FD2_MAGIC_FX_PRIMARY_SAMPLE) player=ctx->primary;
    else if (event->kind==FD2_MAGIC_FX_SECONDARY_SAMPLE) player=ctx->secondary;
    else return -1;
    if (!bank_sample_valid(&e->sound,event->index)) return -1;
    if (stopped(ctx) || !e->sfx_enabled || !player) return 0;
    return fd2_pcm_play_replace(player,&e->sound,event->index,1,1.0f);
}

static int magic_fx_step(magic_fx_reader *r,uint8_t phase,int *native_return) {
    return fd2_magic_fx_step(&r->ctx->magic_exchange->fx,phase,
        magic_fx_recorded_random,r,magic_fx_emit,r,native_return);
}

static void magic_work_part(fd2_battle_magic_exchange *e,int origin,
        const battle_clip *clip,size_t index,int x,int y,int tint) {
    part_draw(e->work,640,270,640,clip,index,origin%640+x,origin/640+y,tint);
}

static void magic_work_idle(fd2_battle_magic_exchange *e,int origin,
        const battle_clip *clip,int x,int y,int tint) {
    magic_idle(e,clip,e->work,640,270,640,origin%640+x,origin/640+y,tint);
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
    if (magic_summary(e,game,&e->caster)!=0) return -1;
    if (!stopped(ctx)) {
        e->started=1;fade(ctx,0);
        const uint8_t *pal;size_t size;
        if (fd2_archive_get(ctx->fdother,0,&pal,&size)!=0 || size<768) return -1;
        fd2_vga_set_palette(ctx->vga,pal);
        intro_scene(ctx,&e->board,e->work,e->caster.side!=0,0,
            &e->caster_idle,&e->target_idle[0],&e->terrain);
    }
    uint8_t idle=0,elapsed=0;unsigned flashes=0;
    static const uint8_t color[10][3]={
        {63,0,0},{63,0,0},{63,0,0},{63,0,0},{43,50,60},
        {43,50,60},{43,50,60},{43,50,60},{63,63,63},{35,16,8}};
    const uint8_t *record=fd2_field_magic_record_get(e->id);
    if (!record) return -1;
    for (size_t i=0;i<e->casting.clip.intro_end;i++) {
        const fd2_figani_part *p=&e->casting.parts[i].part;
        if (p->effect_marker==1) {
            e->caster.mp=(uint16_t)(e->caster.mp-record[5]);
            if (magic_summary(e,game,&e->caster)!=0 || sprite_lut(e,
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
                    dac0(ctx,color[e->id][0],color[e->id][1],color[e->id][2]);present(ctx,0);
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
    uint64_t now=SDL_GetTicks();
    if (stopped(ctx)) return 0;
    if (ctx->visual_owner!=game || !game->ready || game->failure ||
        fd2_field_water_tick(ctx->vga,now)<0 ||
        fd2_field_game_tick_visual(ctx->visual_owner,now)!=0) return -1;
    fd2_field_game_render_effect_background(game,target);
    return 0;
}

static int magic_cast_heal(const fd2_field_game *game,fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (play_cue(ctx,&e->sound,e->sfx_enabled,11)!=0) return -1;
    int x=(game->cursor_cell_x-game->camera_cell_x)*24+12;
    int y=(game->cursor_cell_y-game->camera_cell_y)*24+16;
    for (int i=9;i>0;i--) {
        if (stopped(ctx)) return 0;
        if (fd2_field_game_render_transition_mask(game,ctx->vga,
            fd2_field_lut_frame(&game->visuals,(size_t)i),x,y,1+(9-i)*2)!=0) return -1;
        present(ctx,0);if (!stopped(ctx)) fd2_delay_ms(5);
    }
    if (!stopped(ctx)) fd2_delay_ms(200);
    for (int i=3;i<10;i++) {
        if (stopped(ctx)) return 0;
        if (fd2_field_game_render_transition_mask(game,ctx->vga,
            fd2_field_lut_frame(&game->visuals,(size_t)i),x,y,17)!=0) return -1;
        present(ctx,0);if (!stopped(ctx)) fd2_delay_ms(5);
    }
    if (stopped(ctx)) return 0;
    if (refresh_map(game,ctx,ctx->vga)!=0) return -1;
    present(ctx,0);if (!stopped(ctx)) fd2_delay_ms(200);
    if (refresh_map(game,ctx,&e->board)!=0) return -1;
    for (size_t frame=0;frame<7;frame++) {
        if (stopped(ctx)) return 0;
        memcpy(ctx->vga->framebuffer,e->board.framebuffer,64000);
        for (size_t i=0;i<e->count;i++) {
            /* 4ebab的0透明与312x192视区，复用同一写点/裁剪循环。 */
            blit(ctx->vga->framebuffer+1284,312,192,320,&e->map_fx[frame],
                ((int)e->units[i].x-game->camera_cell_x)*24,
                ((int)e->units[i].y-game->camera_cell_y)*24-6,-1);
        }
        if (!frame && play_cue(ctx,&e->sound,e->sfx_enabled,12)!=0) return -1;
        present(ctx,1);
    }
    if (stopped(ctx)) return 0;
    if (refresh_map(game,ctx,&e->board)!=0) return -1;
    memcpy(ctx->vga->framebuffer,e->board.framebuffer,64000);present(ctx,0);
    uint8_t idle=game->idle_phase;
    memcpy(e->work,e->board.framebuffer,64000);
    for (size_t i=0;i<e->count;i++) {
        size_t frame=(size_t)e->units[i].unit_id*12u+(idle==3 ? 2u:idle);
        uint8_t *pixels=game->sprites.decoded_frames+frame*576u;
        battle_image solid={{24,24,pixels},pixels};
        blit(e->work+1284,312,192,320,&solid,
            ((int)e->units[i].x-game->camera_cell_x)*24,
            ((int)e->units[i].y-game->camera_cell_y)*24-6,192);
    }
    if (play_cue(ctx,&e->sound,e->sfx_enabled,1)!=0) return -1;
    for (size_t i=0;i<11 && !stopped(ctx);i++) {
        memcpy(ctx->vga->framebuffer,(i&1) ? e->work:e->board.framebuffer,64000);
        present(ctx,i<10 ? 1u:0u);
    }
    return 0;
}

static int magic_cast(const fd2_field_game *game,const fd2_field_magic_present_event *event,
        fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (!e || e->cast_done || event->actor_index!=e->caster_index || event->magic_id!=e->id)
        return -1;
    e->cast_done=1;
    return e->id<10 ? magic_cast_full(game,event,ctx):magic_cast_heal(game,ctx);
}

static int magic_target_full(const fd2_field_game *game,const fd2_field_magic_present_event *event,
        fd2_battle_animation *ctx,size_t index) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;magic_fx_reader reader;
    if (magic_fx_reader_open(&reader,ctx,event)!=0) return -1;
    int duration,native_return;
    if (magic_fx_step(&reader,3,&duration)!=0 || duration<0 || duration>UINT16_MAX) return -1;
    e->idle_part=e->idle_ticks=0;
    size_t impact=0;unsigned hp_step=1;
    static const int dx[4]={6,4,2,0},dy[4]={-3,-2,-1,0};
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
                e->shake_sign*dx[e->shake_phase],dy[e->shake_phase],
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
                if (magic_summary(e,game,&e->units[index])!=0) return -1;
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
        if (magic_summary(e,game,&e->units[index+1])!=0) return -1;
    }
    return reader.used==reader.count ? 0:-1;
}

static int magic_target(const fd2_field_game *game,const fd2_field_magic_present_event *event,
        fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    if (!e || !e->cast_done || e->next>=e->count || !event->target_before || !event->result ||
        event->actor_index!=e->caster_index || event->magic_id!=e->id ||
        event->target_index!=e->targets[e->next] ||
        event->impact_count>FD2_FIELD_MAGIC_IMPACT_CAPACITY ||
        ((!event->result->raw_damage || e->id==13) && event->impact_count)) return -1;
    for (size_t i=0;i<event->impact_count;i++)
        if (event->impact_jitter[i]<-1 || event->impact_jitter[i]>1) return -1;
    size_t index=e->next++;e->units[index]=*event->target_before;e->raw_heal[index]=event->result->raw_heal;
    return e->id<10 ? magic_target_full(game,event,ctx,index):0;
}

static int floating_heals(const fd2_field_game *game,fd2_battle_animation *ctx) {
    fd2_battle_magic_exchange *e=ctx->magic_exchange;
    static const uint8_t curve[25]={15,15,15,15,7,3,1,0,0,1,3,7,15,15,11,9,8,8,9,11,15,15,15,15,15};
    if (refresh_map(game,ctx,&e->board)!=0) return -1;
    int any=0;
    for (size_t i=0;i<e->count;i++) if (e->units[i].x>=game->camera_cell_x &&
        e->units[i].x<game->camera_cell_x+13 && e->units[i].y>=game->camera_cell_y-1 &&
        e->units[i].y<=game->camera_cell_y+8) any=1;
    if (!any) return 0;
    for (size_t frame=0;frame<22 && !stopped(ctx);frame++) {
        memcpy(ctx->vga->framebuffer,e->board.framebuffer,64000);
        for (size_t i=0;i<e->count;i++) {
            const fd2_field_unit *u=&e->units[i];
            if (u->x<game->camera_cell_x || u->x>=game->camera_cell_x+13 ||
                u->y<game->camera_cell_y-1 || u->y>game->camera_cell_y+8) continue;
            char digits[16];snprintf(digits,sizeof(digits),"%u",(unsigned)e->raw_heal[i]);
            size_t length=strlen(digits),next=0;
            for (size_t slot=0;slot<4;slot++) if (length>3u-slot) {
                const battle_image *digit=&e->digits[(size_t)(digits[next++]-'0')];
                blit(ctx->vga->framebuffer+1284,312,192,320,digit,
                    ((int)u->x-game->camera_cell_x)*24+2+(int)slot*5,
                    ((int)u->y-game->camera_cell_y)*24+(int)curve[frame+slot]-3,-1);
            }
        }
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
    int status=0;
    (void)stopped(ctx);
    if (normal && e->id<10 && e->cast_done)
        status=magic_finish_full(ctx,event);
    if (normal && status==0 && !ctx->host_quit && !ctx->skip) {
        if (e->id<10 && e->cast_done) {
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
        } else if (e->id==13 && e->cast_done && floating_heals(game,ctx)!=0) status=-1;
    }
    if (e->id<10 && ctx->primary && e->sfx_enabled) (void)fd2_pcm_stop(ctx->primary);
    if (normal && status==0 && e->id<10 && e->started && !stopped(ctx)) {fd2_wait_ticks(8);fade(ctx,0);}
    if (e->id<10 || !normal || ctx->host_quit || status<0) {
        memcpy(ctx->vga->palette,e->palette,768);memcpy(ctx->vga->dac,e->dac,768);
    }
    /* 21b18→21190共享11cac尾；skip正常返回不等于action_failed/host_quit。 */
    if (normal && !ctx->host_quit && status==0) {
        if (fd2_field_game_tick_visual(ctx->visual_owner,SDL_GetTicks())!=0) status=-1;
        if (status==0) fd2_field_game_render_effect_background(game,ctx->vga);
        else memcpy(ctx->vga->framebuffer,e->map,64000);
    } else memcpy(ctx->vga->framebuffer,e->map,64000);
    if (normal && status==0 && e->id<10 && e->started && !stopped(ctx)) fade(ctx,1);
    if (ctx->host_quit) memcpy(ctx->vga->framebuffer,e->map,64000);
    if (e->id<10 || !normal || ctx->host_quit || status<0) memcpy(ctx->vga->dac,e->dac,768);
    magic_close_exchange(ctx);return status;
}

static int magic_dispatch(const fd2_field_game *game,
        const fd2_field_magic_present_event *event,void *userdata) {
    fd2_battle_animation *ctx=userdata;
    if (!game || !event || !ctx || !ctx->vga || !ctx->fdother || ctx->visual_owner!=game) return -1;
    switch (event->phase) {
        case FD2_FIELD_MAGIC_PRESENT_PREPARE:return magic_prepare(game,event,ctx);
        case FD2_FIELD_MAGIC_PRESENT_CAST:return magic_cast(game,event,ctx);
        case FD2_FIELD_MAGIC_PRESENT_TARGET:return magic_target(game,event,ctx);
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
    return result;
}

void fd2_battle_animation_close(fd2_battle_animation *ctx) {
    if (!ctx) return;
    if (ctx->exchange && ctx->vga) restore(ctx,NULL,0);
    if (ctx->magic_exchange && ctx->vga) (void)magic_restore(ctx,NULL,0,NULL);
    fd2_archive_close(&ctx->backgrounds);
    fd2_archive_close(&ctx->terrain_images);memset(ctx,0,sizeof(*ctx));
}
