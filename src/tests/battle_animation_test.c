/* B6物理服务离屏回归：2e2b0/2ebe1/2e9a8/2f4d4/2f631原调用边界。
 * present/BIOS等待/PCM启动替身不创建窗口、不打开设备；真实目录/RLE/summary照常执行。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SDL_GetTicks battle_test_ticks
#define fd2_pcm_play_replace battle_test_pcm_replace
#define fd2_pcm_stop battle_test_pcm_stop
#include "../battle_animation.c"
#undef fd2_pcm_play_replace
#undef fd2_pcm_stop
#undef SDL_GetTicks

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x); return -1; } } while(0)
static unsigned frames, ticks, delays, cues, primary_stops, secondary_cues;
static uint64_t fake_ms;
Uint64 SDLCALL battle_test_ticks(void) { return fake_ms; }
static unsigned quit_frame,skip_frame;
static fd2_battle_animation *skip_context;
static int cue_failure;
static size_t last_cue;
static fd2_pcm_player *expected_primary;
static fd2_pcm_player *expected_secondary;
static unsigned wrong_stop;
static fd2_battle_animation *watch;
static uint16_t watched_hp[16];
static size_t watched_count;
static uint8_t last_present_dac[3], dark_samples[80];
static unsigned dark_sample_count, bad_white_wait, delay2_calls, return_calls;
static unsigned normal_return_calls, effect_return_calls;
static uint16_t returned_hp;
static fd2_battle_animation *magic_watch;
static uint32_t magic_hashes[64];static size_t magic_hash_count;
static unsigned mask_calls;static int mask_failure;
static int mask_radius[64];
static uint32_t last_present_hash;static unsigned stale_wait200;
static int capture_sequence;static uint64_t frame_digest;
static int capture_first;static unsigned captured_count;static uint8_t first_frame[64000];
static uint32_t hud_hash(const uint8_t *frame) {
    uint32_t h=2166136261u;
    for (int y=154;y<196;y++) for (int x=0;x<149;x++) h=(h^frame[y*320+x])*16777619u;
    return h;
}
static unsigned business_rng_calls;
static uint32_t forbidden_rng(void *unused) { (void)unused;business_rng_calls++;return 1; }
static fd2_field_game game;
static fd2_vga screen;
static fd2_archive fdother, fdtxt, figani_fixture;

void fd2_vga_clear(fd2_vga *vga,uint8_t fill) { memset(vga->framebuffer,fill,64000); }
void fd2_vga_present(fd2_vga *vga) {
    frames++;
    if(skip_context && skip_frame==frames)fd2_battle_animation_set_skip(skip_context,1);
    if (capture_sequence) for (size_t i=0;i<64000;i++)
        frame_digest=(frame_digest^vga->framebuffer[i])*UINT64_C(1099511628211);
    if (capture_first && captured_count++==0) memcpy(first_frame,vga->framebuffer,64000);
    if (magic_watch && magic_hash_count<64) magic_hashes[magic_hash_count++]=hud_hash(vga->framebuffer);
    last_present_hash=hud_hash(vga->framebuffer);
    memcpy(last_present_dac,vga->dac,3);
    if (dark_sample_count<80) dark_samples[dark_sample_count++]=vga->dac[0];
    if (watch && watch->exchange && watched_count<16)
        watched_hp[watched_count++]=watch->exchange->units[1].hp;
    if (quit_frame && frames==quit_frame) vga->input.quit_requested=1;
}
void fd2_wait_ticks(uint32_t value) { ticks+=value;fake_ms+=(uint64_t)value*55u; }
void fd2_delay_ms(uint32_t value) {
    delays+=value;fake_ms+=value;
    if (value==2) delay2_calls++;
    if (value==200 && last_present_hash!=hud_hash(screen.framebuffer)) stale_wait200++;
    if (value==40 && (last_present_dac[0] || last_present_dac[1] || last_present_dac[2]))
        bad_white_wait++;
}
static void map_background(const fd2_field_game *current,fd2_vga *vga) {
    return_calls++;returned_hp=current->units.items[1].hp;
    memset(vga->framebuffer,(uint8_t)(20+returned_hp),64000);
}
void fd2_field_game_render_battle_return(const fd2_field_game *current,fd2_vga *vga) {
    normal_return_calls++;map_background(current,vga);
    vga->framebuffer[319]=99; /* 仅替身的普通cursor可见标记；不是游戏cursor像素期望。 */
}
void fd2_field_game_render_effect_background(const fd2_field_game *current,fd2_vga *vga) {effect_return_calls++;map_background(current,vga);
}
static unsigned visual_calls;
static uint64_t visual_stamps[64];
/* 此处只校接线/退出；真正1297d/11eee实现由field_unit_render真实Game回归验证。 */
int fd2_field_game_tick_visual(fd2_field_game *g,uint64_t now) {
    if(!g || !g->ready || g->failure) return -1;
    if(visual_calls<64)visual_stamps[visual_calls]=now;
    visual_calls++;
    if(!g->last_idle_ms)g->last_idle_ms=now;
    else if(now<g->last_idle_ms || now-g->last_idle_ms>=275) {g->idle_phase=(g->idle_phase+1u)&3u;g->last_idle_ms=now;}
    if(!g->last_terrain_visual_ms)g->last_terrain_visual_ms=now;
    else if(now<g->last_terrain_visual_ms || now-g->last_terrain_visual_ms>=55) {g->terrain_visual_phase^=1u;g->last_terrain_visual_ms=now;}
    return 0;
}
static int unchanged_except_visual(const fd2_field_game *g,const fd2_field_game *before) {
    fd2_field_game *copy=malloc(sizeof(*copy));if(!copy)return 0;*copy=*g;
    copy->idle_phase=before->idle_phase;copy->last_idle_ms=before->last_idle_ms;
    copy->terrain_visual_phase=before->terrain_visual_phase;copy->last_terrain_visual_ms=before->last_terrain_visual_ms;
    int same=!memcmp(copy,before,sizeof(*copy));free(copy);return same;
}
int fd2_input_take_quit(fd2_input *input) { return input->quit_requested; }
int fd2_field_game_render_transition_mask(const fd2_field_game *current,fd2_vga *vga,
        const uint8_t lut[256],int x,int y,int radius) {
    (void)current;(void)x;(void)y;
    if (mask_calls<64) mask_radius[mask_calls]=radius;
    mask_calls++;if (mask_failure) return -1;
    memset(vga->framebuffer,lut[7],64000);return 0;
}
void fd2_vga_set_palette(fd2_vga *vga,const uint8_t pal[768]) {
    memcpy(vga->palette,pal,768);
    for (size_t i=0;i<768;i++) vga->dac[i]=(uint8_t)((pal[i]<<2)|(pal[i]>>4));
}
void fd2_vga_set_brightness(fd2_vga *vga,int darkness) {
    for (size_t i=0;i<768;i++) {
        uint8_t v=(uint8_t)(vga->palette[i]*(64-darkness)/64);
        vga->dac[i]=(uint8_t)((v<<2)|(v>>4));
    }
}
int battle_test_pcm_replace(fd2_pcm_player *player,const fd2_pcm_bank *bank,
        size_t index,int loops,float gain) {
    (void)bank;
    if ((player!=expected_primary && player!=expected_secondary) || loops!=1 || gain!=(101.0f/128.0f)) return -1;
    if (player==expected_secondary) secondary_cues++;
    cues++; last_cue=index;
    return cue_failure ? -1:0;
}
int battle_test_pcm_stop(fd2_pcm_player *player) {
    if (player!=expected_primary) { wrong_stop++; return -1; }
    primary_stops++; return 0;
}
int fd2_field_unit_is_hidden(const fd2_field_unit *u) { return (u->flags&1)!=0; }

static void reset_observer(void) {
    frames=ticks=delays=cues=primary_stops=quit_frame=wrong_stop=secondary_cues=0; cue_failure=0;skip_frame=0;skip_context=NULL;
    magic_watch=NULL;magic_hash_count=0;mask_calls=0;mask_failure=0;stale_wait200=0;capture_first=0;captured_count=0;
    watch=NULL;watched_count=0;dark_sample_count=bad_white_wait=delay2_calls=return_calls=0;
    normal_return_calls=effect_return_calls=0;
    screen.input.quit_requested=0;
    capture_sequence=0;frame_digest=UINT64_C(14695981039346656037);
}
static void actor(fd2_field_unit *u,uint8_t id,uint8_t side,int x) {
    memset(u,0,sizeof(*u));u->unit_id=id;u->text_id=0;u->side=side;
    u->x=(uint8_t)x;u->y=1;u->hp=u->hp_max=42;u->level=1;
    u->movement_profile=1;u->accuracy=100;u->attack=20;u->defense=12;
    for (size_t i=0;i<8;i++) ((uint8_t *)u)[10+i*2]=128;
    ((uint8_t *)u)[10]=64; ((uint8_t *)u)[11]=0;
}
static fd2_field_physical_event make_event(fd2_field_physical_phase phase) {
    fd2_field_physical_event event={0};event.phase=phase;event.actor_index=0;event.target_index=1;
    return event;
}
static int finish(fd2_battle_animation *ctx,int failure) {
    fd2_field_physical_event event=make_event(FD2_FIELD_PHYSICAL_FINISH);
    event.action_failed=failure;
    return fd2_battle_animation_physical_present(&game,&event,ctx);
}
static int setup(fd2_battle_animation *ctx) {
    memset(ctx,0,sizeof(*ctx)); reset_observer();expected_secondary=NULL;game.option_values[1]=1;
    game.attack_rng=forbidden_rng;game.attack_rng_userdata=&business_rng_calls;
    actor(&game.units.items[0],0,2,1);actor(&game.units.items[1],96,0,2);
    memset(screen.framebuffer,71,64000);memset(screen.palette,13,768);memset(screen.dac,47,768);
    fake_ms=1000;visual_calls=0;game.ready=1;game.failure=FD2_FIELD_FAILURE_NONE;
    game.idle_phase=game.terrain_visual_phase=0;
    game.last_idle_ms=game.last_terrain_visual_ms=1000;
    return fd2_battle_animation_open(ctx,&screen,NULL,NULL,&fdother,&figani_fixture,&game);
}

static int test_mask_and_clipping(void) {
    /* RUN零/LITERAL255必须写，SKIP才透明，stride-2只写奇数列。 */
    const uint8_t raw[]={6,0,1,0,0,0,0xc0,0x80,255,0x40,9,0xc0};
    battle_image image={0};CHECK(image_open(&image,raw,sizeof(raw))==0);
    uint8_t dst[12];memset(dst,77,sizeof(dst));
    blit(dst,6,2,6,&image,0,0,-1);
    CHECK(dst[0]==0 && dst[1]==77 && dst[2]==255 && dst[3]==77 && dst[4]==9 && dst[5]==77);
    memset(dst,77,sizeof(dst));blit(dst,6,2,6,&image,-2,1,33);
    CHECK(dst[6]==33 && dst[7]==77 && dst[8]==33 && dst[9]==77 && dst[10]==77);
    image_close(&image);CHECK(!image.image.pixels && !image.opaque);
    return 0;
}

static int test_prepare_atomic_and_skip(void) {
    fd2_battle_animation ctx; CHECK(setup(&ctx)==0);
    CHECK(!ctx.backgrounds.data && !ctx.terrain_images.data);
    fd2_field_game *before=malloc(sizeof(game));fd2_vga *vbefore=malloc(sizeof(screen));
    CHECK(before && vbefore);*before=game;*vbefore=screen;
    fd2_field_physical_event event=make_event(FD2_FIELD_PHYSICAL_PREPARE);
    game.info_assets.bars[7].height=4;before->info_assets.bars[7].height=4;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==-1);
    CHECK(ctx.failed && !ctx.exchange && !memcmp(&game,before,sizeof(game)) && !memcmp(&screen,vbefore,sizeof(screen)));
    game.info_assets.bars[7].height=5;*before=game;
    size_t count=ctx.backgrounds.count;ctx.backgrounds.count=0;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==-1);
    CHECK(!ctx.exchange && !memcmp(&screen,vbefore,sizeof(screen)) && !memcmp(&game,before,sizeof(game)));
    ctx.backgrounds.count=count;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    CHECK(!frames && !cues && !memcmp(&screen,vbefore,sizeof(screen)) && !memcmp(&game,before,sizeof(game)));
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==-1); /* 不覆盖未结束scope。 */
    fd2_field_attack_result result={0};result.hit=1;result.rolled_damage=9;result.hp_after=33;
    event.phase=FD2_FIELD_PHYSICAL_STRIKE;event.actor_before=&game.units.items[0];
    event.target_before=&game.units.items[1];event.result=&result;
    fd2_battle_animation_set_skip(&ctx,1);
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    CHECK(!frames && !ticks && !delays && !cues && !memcmp(&game,before,sizeof(game)));
    CHECK(finish(&ctx,0)==0 && !ctx.exchange && return_calls==1 && returned_hp==42);
    CHECK(normal_return_calls==1 && !effect_return_calls && screen.framebuffer[319]==99);
    CHECK(screen.framebuffer[0]==62 && screen.framebuffer[63999]==62 && !frames);
    CHECK(!memcmp(screen.palette,vbefore->palette,768) && !memcmp(screen.dac,vbefore->dac,768));
    CHECK(finish(&ctx,0)==-1);
    fd2_battle_animation_close(&ctx);CHECK(!ctx.vga && !ctx.figani);
    free(before);free(vbefore);return 0;
}

static int test_scene_source_contract(void) {
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    fd2_terrain_attr saved=game.terrain.attrs[0];game.terrain.attrs[0].attr2=4;
    size_t scene=99;
    CHECK(scene_index(&game,&game.units.items[0],&scene)==0 && scene==4);
    game.units.items[0].movement_profile=19;
    CHECK(scene_index(&game,&game.units.items[0],&scene)==0 && scene==3);
    game.units.items[0].unit_id=28;
    CHECK(scene_index(&game,&game.units.items[0],&scene)==0 && scene==4);
    game.units.items[0].unit_id=0;game.units.items[0].movement_profile=1;
    game.units.items[0].race=4;
    CHECK(scene_index(&game,&game.units.items[0],&scene)==0 && scene==3);
    actor(&game.units.items[0],0,2,1);
    fd2_field_physical_event event=make_event(FD2_FIELD_PHYSICAL_PREPARE);
    fd2_vga *before=malloc(sizeof(screen));CHECK(before);*before=screen;
    game.stage=1;CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    CHECK(finish(&ctx,1)==0 && !memcmp(&screen,before,sizeof(screen)));
    game.stage=2;CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==-1);
    CHECK(!ctx.exchange && !memcmp(&screen,before,sizeof(screen)));
    game.stage=0;game.terrain.attrs[0].attr2=255;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==-1);
    CHECK(!ctx.exchange && !memcmp(&screen,before,sizeof(screen)));
    game.terrain.attrs[0]=saved;
    game.units.items[0].x=255;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==-1);
    CHECK(!ctx.exchange && !memcmp(&screen,before,sizeof(screen)));
    fd2_battle_animation_close(&ctx);free(before);return 0;
}

static int test_real_scene_and_quit(void) {
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    fd2_vga *before=malloc(sizeof(screen));fd2_field_game *gbefore=malloc(sizeof(game));
    CHECK(before && gbefore);*before=screen;*gbefore=game;
    fd2_pcm_player primary={0},secondary={0};ctx.primary=&primary;ctx.secondary=&secondary;expected_primary=&primary;
    fd2_field_physical_event event=make_event(FD2_FIELD_PHYSICAL_PREPARE);
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    fd2_field_unit a=game.units.items[0],b=game.units.items[1];
    fd2_field_attack_result result={0};result.hit=1;result.critical=1;result.rolled_damage=9;result.hp_after=33;
    event.phase=FD2_FIELD_PHYSICAL_STRIKE;event.actor_before=&a;event.target_before=&b;event.result=&result;
    event.pre_hit_status_applied=1;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    CHECK(frames>70 && ticks>0 && cues>0 && delays>=80 && !bad_white_wait && !memcmp(&game,gbefore,sizeof(game)));
    CHECK(ctx.exchange->units[1].hp==33);
    b.hp=33;result.rolled_damage=99;result.hp_after=0;event.strike_index=1;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    CHECK(ctx.exchange->units[1].hp==0 && !memcmp(&game,gbefore,sizeof(game)));
    game.units.items[1].hp=0; /* 模拟Game已提交的真实最终HP；service不得回放42HP旧页。 */
    CHECK(finish(&ctx,0)==0 && primary_stops==1 && return_calls==1 && returned_hp==0);
    CHECK(normal_return_calls==1 && !effect_return_calls && screen.framebuffer[319]==99);
    CHECK(screen.framebuffer[0]==20 && screen.framebuffer[63999]==20);
    CHECK(!memcmp(screen.palette,before->palette,768) && !memcmp(screen.dac,before->dac,768));
    fd2_battle_animation_close(&ctx);

    CHECK(setup(&ctx)==0);*before=screen;ctx.primary=&primary;ctx.secondary=&secondary;
    event=make_event(FD2_FIELD_PHYSICAL_PREPARE);
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    event.phase=FD2_FIELD_PHYSICAL_STRIKE;event.actor_before=&a;event.target_before=&b;event.result=&result;
    quit_frame=3;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0 && ctx.host_quit && frames==3);
    unsigned atquit=frames;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0 && frames==atquit);
    CHECK(finish(&ctx,0)==0 && !ctx.exchange && frames==atquit && primary_stops==1 && !return_calls);
    CHECK(!memcmp(screen.framebuffer,before->framebuffer,64000) && !memcmp(screen.dac,before->dac,768));
    CHECK(screen.input.quit_requested); /* 持久quit保留给唯一Play owner。 */
    fd2_battle_animation_close(&ctx);free(before);free(gbefore);return 0;
}

/* 1像素影像把marker、layer、shake和未消费原字节的写点分开验证。 */
static int tiny_clip(battle_clip *clip,size_t count,int idle) {
    for (size_t i=0;i<clip->clip.part_count;i++) image_close(&clip->parts[i].drawing);
    free(clip->parts);clip->parts=calloc(count,sizeof(*clip->parts));
    CHECK(clip->parts);clip->clip.part_count=(uint8_t)count;clip->clip.split_mode=0;
    for (size_t i=0;i<count;i++) {
        battle_part *p=&clip->parts[i];
        p->part.x=(uint16_t)(idle ? 114:100);p->part.y=(uint16_t)(idle ? 110:100);
        p->part.duration=1;p->drawing.image.width=p->drawing.image.height=1;
        p->drawing.image.pixels=malloc(1);p->drawing.opaque=malloc(1);
        CHECK(p->drawing.image.pixels && p->drawing.opaque);
        p->drawing.image.pixels[0]=(uint8_t)(idle ? 9:7);p->drawing.opaque[0]=1;
    }
    return 0;
}
static int test_marker_layers_idle_and_miss(void) {
    for (unsigned layer=0;layer<2;layer++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
        fd2_field_physical_event event=make_event(FD2_FIELD_PHYSICAL_PREPARE);
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        CHECK(tiny_clip(&ctx.exchange->attack[0],1,0)==0);
        CHECK(tiny_clip(&ctx.exchange->idle[1],2,1)==0);
        ctx.exchange->started=1;
        ctx.exchange->attack[0].parts[0].part.effect_marker=1;
        ctx.exchange->attack[0].parts[0].part.layer_flags=(uint8_t)layer;
        fd2_field_unit a=game.units.items[0],b=game.units.items[1];
        fd2_field_attack_result result={0};result.hit=1;result.rolled_damage=9;result.hp_after=33;
        event.phase=FD2_FIELD_PHYSICAL_STRIKE;event.actor_before=&a;event.target_before=&b;event.result=&result;
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        /* side非0：layer1先attack，目标在shake(-14,-10)后盖上33；layer0反序。 */
        CHECK(screen.framebuffer[100*320+100]==(layer ? 33:7));
        CHECK(ctx.exchange->idle_part==1 && ctx.exchange->idle_ticks==0 && ctx.exchange->shake==4);
        CHECK(ctx.exchange->units[1].hp==33 && game.units.items[1].hp==42);
        CHECK(finish(&ctx,1)==0);fd2_battle_animation_close(&ctx);
    }
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    fd2_field_physical_event event=make_event(FD2_FIELD_PHYSICAL_PREPARE);
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    fd2_pcm_player primary={0},secondary={0};ctx.primary=&primary;ctx.secondary=&secondary;expected_primary=&primary;
    CHECK(tiny_clip(&ctx.exchange->attack[0],3,0)==0);
    CHECK(tiny_clip(&ctx.exchange->idle[1],2,1)==0);
    ctx.exchange->started=1;ctx.exchange->idle[1].parts[0].part.duration=0;
    for (size_t i=0;i<3;i++) {
        ctx.exchange->attack[0].parts[i].part.effect_marker=(uint8_t)(i!=1);
        ctx.exchange->attack[0].parts[i].part.cue=(uint8_t)(i!=1);
    }
    fd2_field_unit a=game.units.items[0],b=game.units.items[1];
    fd2_field_attack_result result={0};result.hit=1;result.rolled_damage=11;result.hp_after=31;
    event.phase=FD2_FIELD_PHYSICAL_STRIKE;event.actor_before=&a;event.target_before=&b;event.result=&result;
    watch=&ctx;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    CHECK(watched_count==3 && watched_hp[0]==37 && watched_hp[1]==37 && watched_hp[2]==31);
    CHECK(ctx.exchange->idle_part==0 && ctx.exchange->idle_ticks==3 && cues==2 && last_cue==1);
    watch=NULL;
    result.hit=0;result.rolled_damage=0;result.hp_after=42;ctx.exchange->shake=0;
    unsigned previous=cues;event.strike_index=1;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    CHECK(cues==previous+2 && last_cue==0 && ctx.exchange->units[1].hp==42 && ctx.exchange->shake==0);
    CHECK(finish(&ctx,1)==0 && primary_stops==1 && !wrong_stop);
    fd2_battle_animation_close(&ctx);return 0;
}

static int test_sfx_option_gate(void) {
    uint8_t *off_frame=malloc(64000);CHECK(off_frame);
    unsigned off_frames=0,off_ticks=0;
    for (int enabled=0;enabled<2;enabled++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);game.option_values[1]=(uint8_t)enabled;
        fd2_pcm_player primary={0},secondary={0};ctx.primary=&primary;ctx.secondary=&secondary;expected_primary=&primary;
        fd2_field_game *before=malloc(sizeof(game));CHECK(before);*before=game;
        fd2_field_physical_event event=make_event(FD2_FIELD_PHYSICAL_PREPARE);
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        CHECK(ctx.exchange->attack[0].bank.archive.data); /* off仍完整预检bank。 */
        fd2_field_attack_result result={0};result.hit=1;result.rolled_damage=9;result.hp_after=33;
        event.phase=FD2_FIELD_PHYSICAL_STRIKE;event.actor_before=&game.units.items[0];
        event.target_before=&game.units.items[1];event.result=&result;
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        CHECK(!memcmp(&game,before,sizeof(game)));
        if (!enabled) { memcpy(off_frame,screen.framebuffer,64000);off_frames=frames;off_ticks=ticks;CHECK(!cues); }
        else CHECK(cues>0 && !memcmp(off_frame,screen.framebuffer,64000) && frames==off_frames && ticks==off_ticks);
        CHECK(finish(&ctx,0)==0 && primary_stops==(unsigned)enabled && !wrong_stop);
        fd2_battle_animation_close(&ctx);free(before);
    }
    free(off_frame);return 0;
}

static int test_exact_dac_intro_and_fade(void) {
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    fd2_field_physical_event event=make_event(FD2_FIELD_PHYSICAL_PREPARE);
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    memset(screen.palette,32,768);
    intro(&ctx);
    CHECK(dark_sample_count==9 && dark_samples[0]==0 && dark_samples[4]==32 && dark_samples[8]==130);
    /* 32-24=8→8bit32；旧比例helper会给20→81。低于暗度的通道夹0。 */
    reset_observer();fade(&ctx,0);
    CHECK(frames==64 && delay2_calls==64 && delays==128);
    CHECK(dark_samples[0]==130 && dark_samples[24]==32 && dark_samples[32]==0);
    reset_observer();fade(&ctx,1);
    CHECK(frames==65 && delay2_calls==65 && delays==130 && dark_samples[64]==130);
    CHECK(finish(&ctx,1)==0 && !return_calls);
    fd2_battle_animation_close(&ctx);return 0;
}

static int test_split_reverse_and_failure(void) {
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    size_t split_id=SIZE_MAX;
    for (size_t id=0;id<136;id++) {
        fd2_figani_clip clip;
        if (!fd2_figani_open_entry(&clip,ctx.figani,id*3+1) && !clip.empty && clip.split_mode) {split_id=id;break;}
    }
    CHECK(split_id!=SIZE_MAX);
    fd2_vga *before=malloc(sizeof(screen));CHECK(before);*before=screen;
    for (int reverse=0;reverse<2;reverse++) {
        reset_observer(); actor(&game.units.items[0],(uint8_t)split_id,reverse ? 0:2,1);
        actor(&game.units.items[1],0,reverse ? 2:0,2);
        fd2_field_physical_event event=make_event(FD2_FIELD_PHYSICAL_PREPARE);
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        CHECK(ctx.exchange->attack[0].clip.split_mode && ctx.exchange->transition[2].image.pixels);
        fd2_field_unit a=game.units.items[0],b=game.units.items[1];
        fd2_field_attack_result result={0};result.hit=1;result.rolled_damage=9;result.hp_after=33;
        event.phase=FD2_FIELD_PHYSICAL_STRIKE;event.actor_before=&a;event.target_before=&b;event.result=&result;
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        b.hp=33;event.strike_index=1;
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0 && frames>100);
        a.hp=33;event.actor_index=1;event.target_index=0;event.counterattack=1;event.strike_index=0;
        event.actor_before=&b;event.target_before=&a;
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        CHECK(finish(&ctx,0)==0 && return_calls==1 && returned_hp==42);
        CHECK(screen.framebuffer[0]==62 && screen.framebuffer[63999]==62);
        CHECK(!memcmp(screen.palette,before->palette,768) && !memcmp(screen.dac,before->dac,768));
    }
    fd2_battle_animation_close(&ctx);
    CHECK(setup(&ctx)==0);*before=screen;
    fd2_pcm_player primary={0};ctx.primary=&primary;expected_primary=&primary;
    fd2_field_physical_event event=make_event(FD2_FIELD_PHYSICAL_PREPARE);
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    fd2_field_unit a=game.units.items[0],b=game.units.items[1];
    fd2_field_attack_result result={0};result.hit=1;result.rolled_damage=9;result.hp_after=33;
    event.phase=FD2_FIELD_PHYSICAL_STRIKE;event.actor_before=&a;event.target_before=&b;event.result=&result;
    cue_failure=1;CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==-1);
    CHECK(ctx.exchange && cues==1);CHECK(finish(&ctx,1)==0 && !ctx.exchange && primary_stops==1 && !return_calls);
    CHECK(!memcmp(&screen,before,sizeof(screen)));
    fd2_battle_animation_close(&ctx);free(before);return 0;
}

static fd2_field_magic_present_event magic_event(uint8_t id,uint8_t *targets,size_t count) {
    fd2_field_magic_present_event e={0};e.magic_id=id;e.actor_index=0;
    e.targets=targets;e.target_count=count;e.phase=FD2_FIELD_MAGIC_PRESENT_PREPARE;
    e.actor_before=&game.units.items[0];
    return e;
}
static int magic_finish(fd2_battle_animation *ctx,fd2_field_magic_present_event *e,int failure) {
    e->phase=FD2_FIELD_MAGIC_PRESENT_FINISH;e->action_failed=failure;
    return fd2_battle_animation_magic_present(&game,e,ctx);
}
static int test_magic_zero_cost_metadata_and_skip(void) {
    const uint8_t ids[]={8,9,12,7},costs[]={0,1,2,4};
    for (size_t i=0;i<4;i++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
        actor(&game.units.items[0],ids[i],2,1);game.units.items[0].mp=1;game.units.items[0].mp_max=30;
        uint8_t targets[]={1};fd2_field_magic_present_event e=magic_event(0,targets,1);e.cast_cost_count=costs[i];
        fd2_figani_magic_cast plan;CHECK(fd2_figani_magic_cast_plan(&plan,ctx.figani,ids[i])==0);
        CHECK(plan.cost_marker_count==costs[i]);
        fd2_field_game *before=malloc(sizeof(game));fd2_vga *old=malloc(sizeof(screen));CHECK(before && old);
        *before=game;*old=screen;
        CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0);
        CHECK(!memcmp(&game,before,sizeof(game)) && !memcmp(&screen,old,sizeof(screen)) && !frames && !cues);
        fd2_field_physical_event physical=make_event(FD2_FIELD_PHYSICAL_PREPARE);
        CHECK(fd2_battle_animation_physical_present(&game,&physical,&ctx)==-1);
        fd2_field_unit caster=game.units.items[0];
        game.units.items[0].mp=(uint16_t)(caster.mp-fd2_field_magic_record_get(0)[5]*costs[i]);
        *before=game;e.phase=FD2_FIELD_MAGIC_PRESENT_CAST;e.actor_before=&caster;
        CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0);
        CHECK(ctx.magic_exchange->caster.mp==game.units.items[0].mp && !memcmp(&game,before,sizeof(game)));
        CHECK(cues==0); /* 无声player仍呈现MP，不调用音频。 */
        CHECK(magic_finish(&ctx,&e,1)==0 && !ctx.magic_exchange);
        fd2_battle_animation_close(&ctx);free(before);free(old);
    }
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    actor(&game.units.items[0],48,2,1);uint8_t targets[]={1};
    fd2_field_magic_present_event e=magic_event(0,targets,1);
    fd2_figani_magic_cast plan;CHECK(fd2_figani_magic_cast_plan(&plan,ctx.figani,48)==0 && plan.clip.empty);
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==-1 && !ctx.magic_exchange);
    actor(&game.units.items[0],9,2,1);game.units.items[0].mp=5;game.units.items[0].mp_max=10;
    fd2_field_unit caster=game.units.items[0];e=magic_event(0,targets,1);e.cast_cost_count=1;
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0);fd2_battle_animation_set_skip(&ctx,1);
    game.units.items[0].mp=(uint16_t)(caster.mp-fd2_field_magic_record_get(0)[5]);
    e.phase=FD2_FIELD_MAGIC_PRESENT_CAST;e.actor_before=&caster;
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0);
    fd2_field_magic_basic_result result={0};result.hit=1;result.hp_before=42;result.hp_after=28;result.raw_damage=14;
    e.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;e.target_index=1;e.target_before=&game.units.items[1];
    e.result=&result;e.impact_count=7;
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0);
    CHECK(magic_finish(&ctx,&e,0)==0 && !frames && !ticks && !delays && !cues && return_calls==1);
    CHECK(!normal_return_calls && effect_return_calls==1 && screen.framebuffer[319]==62);
    fd2_battle_animation_close(&ctx);return 0;
}

static int test_magic_zero_impacts_layers_and_targets(void) {
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    actor(&game.units.items[0],9,2,1);game.units.items[0].mp=5;game.units.items[0].mp_max=10;
    uint8_t targets[]={1};fd2_field_magic_present_event e=magic_event(0,targets,1);e.cast_cost_count=1;
    fd2_pcm_player primary={0};ctx.primary=&primary;expected_primary=&primary;
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0);
    fd2_field_unit caster=game.units.items[0];game.units.items[0].mp=(uint16_t)(caster.mp-fd2_field_magic_record_get(0)[5]);
    e.phase=FD2_FIELD_MAGIC_PRESENT_CAST;e.actor_before=&caster;
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0 && cues==1 && last_cue==0);
    CHECK(tiny_clip(&ctx.magic_exchange->casting,1,0)==0);
    CHECK(tiny_clip(&ctx.magic_exchange->target_idle[0],2,0)==0);
    ctx.magic_exchange->target_idle[0].parts[0].part.duration=0;
    magic_idle(ctx.magic_exchange,&ctx.magic_exchange->target_idle[0],ctx.magic_exchange->work,320,200,320,0,0,-1);
    CHECK(ctx.magic_exchange->idle_part==1 && ctx.magic_exchange->idle_ticks==0);
    CHECK(tiny_clip(&ctx.magic_exchange->target_idle[0],1,0)==0);
    CHECK(tiny_clip(&ctx.magic_exchange->particles,16,0)==0);
    fd2_field_unit target=game.units.items[1];
    fd2_field_magic_basic_result result={0};result.hit=1;result.hp_before=42;result.hp_after=28;result.raw_damage=14;
    uint32_t expected[8];fd2_vga *page=malloc(sizeof(*page));CHECK(page);
    for (size_t i=0;i<=7;i++) {
        *page=ctx.magic_exchange->board;fd2_field_unit u=target;u.hp=(uint16_t)(42-i*2);
        CHECK(fd2_field_unit_summary_draw_at(page,&game.info_assets,&game.font,&game.ui_text,&u,0,154)==0);
        expected[i]=hud_hash(page->framebuffer);
    }
    reset_observer();magic_watch=&ctx;
    game.units.items[1].hp=28;e.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;e.target_index=1;
    e.target_before=&target;e.result=&result;e.impact_count=7;
    for (size_t i=0;i<7;i++) e.impact_jitter[i]=(int8_t)((int)(i%3)-1);
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0 && frames==28 && cues==7 && last_cue==1);
    CHECK(magic_hash_count==28);
    for (size_t tick=0;tick<28;tick++) {
        size_t visible=tick<=8 ? 0:1+(tick-9)/2;if (visible>7) visible=7;
        CHECK(magic_hashes[tick]==expected[visible]);
    }
    CHECK(ctx.magic_exchange->units[0].hp==28);magic_watch=NULL;
    CHECK(magic_finish(&ctx,&e,0)==0 && primary_stops==1 && !ctx.magic_exchange);
    fd2_battle_animation_close(&ctx);free(page);

    CHECK(setup(&ctx)==0);actor(&game.units.items[0],9,2,1);game.units.items[0].mp=5;
    game.units.count=3;actor(&game.units.items[2],96,0,2);uint8_t pair[]={1,2};
    e=magic_event(0,pair,2);e.cast_cost_count=1;
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0);
    caster=game.units.items[0];game.units.items[0].mp=(uint16_t)(caster.mp-fd2_field_magic_record_get(0)[5]);
    e.phase=FD2_FIELD_MAGIC_PRESENT_CAST;e.actor_before=&caster;
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0);reset_observer();
    result.hit=0;result.hp_after=42;result.raw_damage=0;e.result=&result;e.impact_count=0;
    for (size_t i=0;i<2;i++) {
        e.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;e.target_index=pair[i];e.target_before=&game.units.items[pair[i]];
        CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0);
    }
    CHECK(frames==65 && ctx.magic_exchange->next==2); /* 28+9目标平移+28，miss无jitter。 */
    CHECK(magic_finish(&ctx,&e,0)==0);fd2_battle_animation_close(&ctx);game.units.count=2;
    return 0;
}

static int test_magic_heal_raw_numbers_and_quit(void) {
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    game.cursor_cell_x=2;game.cursor_cell_y=1;
    uint8_t targets[]={1};fd2_field_magic_present_event e=magic_event(13,targets,1);e.cast_cost_count=1;
    fd2_pcm_player primary={0};ctx.primary=&primary;expected_primary=&primary;
    fd2_field_game *before=malloc(sizeof(game));fd2_vga *old=malloc(sizeof(screen));CHECK(before && old);
    uint8_t *real_frames=game.sprites.decoded_frames;
    size_t sprite_bytes=game.sprites.frame_count*576u;
    uint8_t *shadow_frames=malloc(sprite_bytes);CHECK(shadow_frames);
    memcpy(shadow_frames,real_frames,sprite_bytes);game.sprites.decoded_frames=shadow_frames;
    memset(shadow_frames+96u*12u*576u,0,576);shadow_frames[96u*12u*576u]=1;
    memset(shadow_frames+(96u*12u+2u)*576u,0,576);shadow_frames[(96u*12u+2u)*576u+1u]=1;
    game.idle_phase=0;
    *before=game;*old=screen;
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0 && mask_calls==9);
    CHECK(!ctx.backgrounds.data && !ctx.terrain_images.data);
    CHECK(!memcmp(&game,before,sizeof(game)) && !memcmp(&screen,old,sizeof(screen)));
    e.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0 && mask_calls==25 && cues==3 && last_cue==1 && !stale_wait200);
    CHECK(!normal_return_calls && effect_return_calls==3 && screen.framebuffer[319]==62);
    CHECK(visual_calls==3 && visual_stamps[1]-visual_stamps[0]==200);
    CHECK(game.idle_phase==2); /* 280ms首调用+1；200ms次调用不增，7FX后再+1。 */
    for (size_t i=0;i<9;i++) CHECK(mask_radius[9+i]==1+(int)i*2);
    for (size_t i=0;i<7;i++) CHECK(mask_radius[18+i]==17);
    size_t flash_at=(size_t)(4+24-6)*320u+4u+48u;
    CHECK(ctx.magic_exchange->work[flash_at]==62 && ctx.magic_exchange->work[flash_at+1]==192);
    fd2_field_magic_basic_result result={0};result.hit=1;result.hp_before=result.hp_after=42;result.raw_heal=29;
    e.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;e.target_index=1;e.target_before=&game.units.items[1];e.result=&result;
    unsigned prior_frames=frames;CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0 && frames==prior_frames);
    CHECK(ctx.magic_exchange->raw_heal[0]==29); /* 满HP也显示29，不能拿HP差0冒充。 */
    fd2_vga *digits_expected=calloc(1,sizeof(*digits_expected));CHECK(digits_expected);
    memset(digits_expected->framebuffer,62,64000); /* effect_background替身当前HP42的无cursor整页。 */
    int dx=4+((int)game.units.items[1].x-game.camera_cell_x)*24+12;
    int dy=4+((int)game.units.items[1].y-game.camera_cell_y)*24+12;
    fd2_map_sprite_blit(digits_expected,&ctx.magic_exchange->digits[2].image,dx,dy,0);
    fd2_map_sprite_blit(digits_expected,&ctx.magic_exchange->digits[9].image,dx+5,dy,0);
    reset_observer();ctx.primary=&primary;expected_primary=&primary;capture_first=1;
    CHECK(magic_finish(&ctx,&e,0)==0 && frames==22 && delays==544 && !primary_stops);
    CHECK(!normal_return_calls && effect_return_calls==2 && screen.framebuffer[319]==62);
    CHECK(!memcmp(first_frame,digits_expected->framebuffer,64000));free(digits_expected);
    fd2_vga *water=calloc(1,sizeof(*water));CHECK(water);
    CHECK(fd2_field_water_palette_apply(water,screen.field_water_phase)==0);
    CHECK(!memcmp(screen.dac+224*3,water->dac+224*3,16*3));free(water);
    CHECK(unchanged_except_visual(&game,before) && !ctx.magic_exchange);
    fd2_battle_animation_close(&ctx);
    game.sprites.decoded_frames=real_frames;free(shadow_frames);

    CHECK(setup(&ctx)==0);e=magic_event(13,targets,1);e.cast_cost_count=1;ctx.primary=&primary;expected_primary=&primary;
    *old=screen;CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0);
    quit_frame=3;e.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0 && ctx.host_quit && frames==3);
    e.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;e.target_index=1;e.target_before=&game.units.items[1];e.result=&result;
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0 && frames==3);
    CHECK(magic_finish(&ctx,&e,0)==0 && frames==3 && !primary_stops && !ctx.magic_exchange);
    CHECK(!normal_return_calls && !effect_return_calls && visual_calls==0);
    CHECK(!memcmp(screen.framebuffer,old->framebuffer,64000) && !memcmp(screen.dac,old->dac,768));
    fd2_battle_animation_close(&ctx);free(before);free(old);return 0;
}

static int test_magic_resource_error_and_tint(void) {
    battle_image image={0};const uint8_t raw[]={1,0,1,0,0,10};
    CHECK(image_open(&image,raw,sizeof(raw))==0);uint8_t pixel=99;
    blit(&pixel,1,1,1,&image,0,0,32+(8<<8));CHECK(pixel==34);
    blit(&pixel,1,1,1,&image,0,0,32+(7<<8));CHECK(pixel==33);image_close(&image);
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    uint8_t targets[]={1};fd2_field_magic_present_event e=magic_event(13,targets,1);e.cast_cost_count=1;
    fd2_field_game *before=malloc(sizeof(game));fd2_vga *old=malloc(sizeof(screen));CHECK(before && old);
    *before=game;*old=screen;mask_failure=1;
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==-1 && ctx.failed && !ctx.magic_exchange && !frames && !cues);
    CHECK(!memcmp(&game,before,sizeof(game)) && !memcmp(&screen,old,sizeof(screen)));
    fd2_battle_animation_close(&ctx);CHECK(setup(&ctx)==0 && !ctx.failed);
    e=magic_event(13,targets,1);e.cast_cost_count=1;*old=screen;
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0);
    fd2_pcm_player primary={0};ctx.primary=&primary;expected_primary=&primary;cue_failure=1;
    e.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
    CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==-1 && ctx.failed && !frames && cues==1);
    CHECK(magic_finish(&ctx,&e,1)==0 && !ctx.magic_exchange && !return_calls && !primary_stops);
    CHECK(!memcmp(screen.framebuffer,old->framebuffer,64000));
    fd2_battle_animation_close(&ctx);free(before);free(old);return 0;
}

/* 以同一纯引擎模拟Game录制端；记录不含effect或jitter，源记录绝不改写。 */
typedef struct {
    fd2_magic_fx fx;
    uint32_t values[2048], seed;
    size_t count;
} fx_recording;
static uint32_t record_fx_random(void *userdata) {
    fx_recording *r=userdata;
    r->seed=r->seed*1103515245u+12345u;
    uint32_t value=(r->seed>>16)&65535u;
    if (r->count>=sizeof(r->values)/sizeof(*r->values)) return UINT32_MAX;
    r->values[r->count++]=value;return value;
}
static int record_fx_step(fx_recording *r,uint8_t phase,int *value) {
    return fd2_magic_fx_step(&r->fx,phase,record_fx_random,r,NULL,NULL,value);
}
static int record_fx_segment(fx_recording *r,fd2_field_magic_present_event *event,
        int has_next,int damaging,unsigned *frame_count) {
    r->count=0;*frame_count=0;int count,value;
    if (event->phase==FD2_FIELD_MAGIC_PRESENT_CAST) {
        CHECK(record_fx_step(r,0,&count)==0);
        for (int i=0;i<count;i++) {
            CHECK(record_fx_step(r,1,&value)==0 && record_fx_step(r,2,&value)==0);
            (*frame_count)++;
        }
    } else if (event->phase==FD2_FIELD_MAGIC_PRESENT_TARGET) {
        event->impact_count=0;CHECK(record_fx_step(r,3,&count)==0);
        for (int i=0;i<count;i++) {
            CHECK(record_fx_step(r,4,&value)==0 && record_fx_step(r,5,&value)==0);
            if (damaging && value==1) {
                CHECK(event->impact_count<FD2_FIELD_MAGIC_IMPACT_CAPACITY);
                event->impact_jitter[event->impact_count]=(int8_t)((int)(event->impact_count%3)-1);
                event->impact_count++;
            }
            (*frame_count)++;
        }
        if (has_next) for (size_t i=0;i<9;i++) {
            CHECK(record_fx_step(r,4,&value)==0 && record_fx_step(r,5,&value)==0);
            (*frame_count)++;
        }
    } else {
        CHECK(event->phase==FD2_FIELD_MAGIC_PRESENT_FINISH && record_fx_step(r,6,&count)==0);
        for (int i=0;i<count;i++) {
            CHECK(record_fx_step(r,7,&value)==0 && record_fx_step(r,8,&value)==0);
            (*frame_count)++;
        }
    }
    event->fx_random=r->values;event->fx_random_count=r->count;return 0;
}

static int test_full_magic_all_handlers_and_persistent_state(void) {
    static const uint8_t impacts[10][3]={
        {7,7,7},{8,8,8},{6,6,6},{14,20,20},{6,6,6},
        {9,11,11},{11,12,12},{7,8,8},{16,0,0},{27,0,0}};
    unsigned secondary_total=0;
    for (uint8_t id=0;id<10;id++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
        actor(&game.units.items[0],9,id==9 ? 0:2,1);
        game.units.items[0].mp=game.units.items[0].mp_max=500;
        game.units.count=4;actor(&game.units.items[2],96,0,2);actor(&game.units.items[3],96,0,2);
        size_t target_count=id==9 ? 1u:3u;
        uint8_t targets[]={1,2,3};fd2_field_magic_present_event event=magic_event(id,targets,target_count);
        event.cast_cost_count=1;
        fd2_pcm_player primary={0},secondary={0};ctx.primary=&primary;ctx.secondary=&secondary;
        expected_primary=&primary;expected_secondary=&secondary;
        fd2_field_game *source=malloc(sizeof(game));CHECK(source);*source=game;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        CHECK(!memcmp(&game,source,sizeof(game)) && !frames && !cues);
        CHECK(ctx.magic_exchange->fx_parameters.effect_entry==(id==9 ? 44:
            (uint16_t[]){18,19,26,39,22,24,32,37,28}[id]));
        fx_recording recording={0};recording.seed=123;
        fd2_magic_fx_parts parts={ctx.magic_exchange->part_durations,
            ctx.magic_exchange->particles.clip.part_count};
        CHECK(fd2_magic_fx_init(&recording.fx,id,game.units.items[0].side,parts)==0);
        fd2_field_unit caster=game.units.items[0];event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
        event.actor_before=&caster;unsigned expected_frames;
        CHECK(record_fx_segment(&recording,&event,0,0,&expected_frames)==0);
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        CHECK(!memcmp(&ctx.magic_exchange->fx,&recording.fx,sizeof(recording.fx)));
        CHECK(ctx.magic_exchange->caster.mp==(uint16_t)(500-fd2_field_magic_record_get(id)[5]));
        fd2_field_magic_basic_result result={0};result.hit=1;result.hp_before=42;
        result.hp_after=28;result.raw_damage=14;
        for (size_t i=0;i<target_count;i++) {
            fd2_field_unit target=game.units.items[targets[i]];
            event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;event.target_index=targets[i];
            event.target_before=&target;event.result=&result;
            CHECK(record_fx_segment(&recording,&event,i+1<target_count,1,&expected_frames)==0);
            CHECK(event.impact_count==impacts[id][i]);
            unsigned before_frames=frames;
            CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
            CHECK(frames-before_frames==expected_frames);
            CHECK(ctx.magic_exchange->units[i].hp==(event.impact_count ? 28:42));
            CHECK(!memcmp(&ctx.magic_exchange->fx,&recording.fx,sizeof(recording.fx)));
            CHECK(!memcmp(&game,source,sizeof(game)));
        }
        event.phase=FD2_FIELD_MAGIC_PRESENT_FINISH;
        CHECK(record_fx_segment(&recording,&event,0,0,&expected_frames)==0);
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        CHECK(!ctx.magic_exchange && primary_stops==1 && !wrong_stop);
        CHECK(!normal_return_calls && effect_return_calls==1 && screen.framebuffer[319]==62);
        secondary_total+=secondary_cues;
        CHECK(unchanged_except_visual(&game,source) && !business_rng_calls);
        fd2_battle_animation_close(&ctx);free(source);
    }
    CHECK(secondary_total>0);game.units.count=2;return 0;
}

static int test_full_magic_scope_tape_and_no_damage(void) {
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    actor(&game.units.items[0],9,2,1);uint8_t targets[]={1};
    fd2_field_magic_present_event event=magic_event(9,targets,1);event.cast_cost_count=1;
    fd2_field_game *source=malloc(sizeof(game));fd2_vga *old=malloc(sizeof(screen));CHECK(source && old);
    *source=game;*old=screen;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==-1 && ctx.failed);
    CHECK(!ctx.magic_exchange && !memcmp(&game,source,sizeof(game)) && !memcmp(&screen,old,sizeof(screen)));
    fd2_battle_animation_close(&ctx);
    CHECK(setup(&ctx)==0);actor(&game.units.items[0],9,0,1);
    game.units.count=3;actor(&game.units.items[2],96,2,2);
    uint8_t id9_multi[]={1,2};event=magic_event(9,id9_multi,2);event.cast_cost_count=1;
    *source=game;*old=screen;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==-1 && !ctx.magic_exchange);
    CHECK(!memcmp(&game,source,sizeof(game)) && !memcmp(&screen,old,sizeof(screen)) && !frames && !cues);
    fd2_battle_animation_close(&ctx);game.units.count=2;
    /* 另一side图包仍逐个准备，不让共用引擎悄悄只覆盖side!=0资源。 */
    for (uint8_t id=0;id<9;id++) {
        CHECK(setup(&ctx)==0);actor(&game.units.items[0],9,0,1);
        event=magic_event(id,targets,1);event.cast_cost_count=1;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        CHECK(ctx.magic_exchange->fx_parameters.effect_entry==
            ((uint16_t[]){20,21,27,43,23,25,33,38,30}[id]));
        CHECK(magic_finish(&ctx,&event,1)==0);fd2_battle_animation_close(&ctx);
    }
    CHECK(setup(&ctx)==0);actor(&game.units.items[0],9,2,1);
    event=magic_event(4,targets,1);event.cast_cost_count=1;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;event.fx_random=NULL;event.fx_random_count=1;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==-1 && !frames && ctx.failed);
    CHECK(magic_finish(&ctx,&event,1)==0);fd2_battle_animation_close(&ctx);
    CHECK(setup(&ctx)==0);actor(&game.units.items[0],9,2,1);game.option_values[1]=0;
    fd2_archive no_sound=fdother;no_sound.count=82;ctx.fdother=&no_sound;
    event=magic_event(0,targets,1);event.cast_cost_count=1;*source=game;*old=screen;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==-1 && ctx.failed && !ctx.magic_exchange);
    CHECK(!memcmp(&game,source,sizeof(game)) && !memcmp(&screen,old,sizeof(screen)) && !frames && !cues);
    CHECK(fdother.count>82);fd2_battle_animation_close(&ctx);
    CHECK(setup(&ctx)==0);actor(&game.units.items[0],9,2,1);
    event=magic_event(4,targets,1);event.cast_cost_count=1;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    fx_recording recording={0};recording.seed=7;
    fd2_magic_fx_parts parts={ctx.magic_exchange->part_durations,ctx.magic_exchange->particles.clip.part_count};
    CHECK(fd2_magic_fx_init(&recording.fx,4,2,parts)==0);
    event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;unsigned expected_frames;
    CHECK(record_fx_segment(&recording,&event,0,0,&expected_frames)==0 && event.fx_random_count==6);
    recording.values[recording.count++]=1;event.fx_random_count=recording.count;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==-1 && ctx.failed); /* 多余值也不能静默吞掉。 */
    CHECK(magic_finish(&ctx,&event,1)==0);fd2_battle_animation_close(&ctx);
    /* accuracy_hit但raw_damage0：FX照推，HP/染色/jitter都不采用hit标志。 */
    CHECK(setup(&ctx)==0);actor(&game.units.items[0],9,2,1);
    event=magic_event(6,targets,1);event.cast_cost_count=1;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    memset(&recording,0,sizeof(recording));recording.seed=11;
    parts=(fd2_magic_fx_parts){ctx.magic_exchange->part_durations,ctx.magic_exchange->particles.clip.part_count};
    CHECK(fd2_magic_fx_init(&recording.fx,6,2,parts)==0);
    fd2_battle_animation_set_skip(&ctx,1);event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
    CHECK(record_fx_segment(&recording,&event,0,0,&expected_frames)==0);
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    fd2_field_magic_basic_result result={0};result.hit=1;result.hp_before=42;result.hp_after=0;
    event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;event.target_index=1;
    event.target_before=&game.units.items[1];event.result=&result;
    CHECK(record_fx_segment(&recording,&event,0,0,&expected_frames)==0 && !event.impact_count);
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && ctx.magic_exchange->units[0].hp==42);
    CHECK(!memcmp(&ctx.magic_exchange->fx,&recording.fx,sizeof(recording.fx)));
    event.phase=FD2_FIELD_MAGIC_PRESENT_FINISH;
    CHECK(record_fx_segment(&recording,&event,0,0,&expected_frames)==0);
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && !frames && !cues && !ticks);
    CHECK(!business_rng_calls);fd2_battle_animation_close(&ctx);free(source);free(old);return 0;
}

static int test_full_magic_audio_gate_and_quit_replay(void) {
    uint64_t hashes[2];unsigned frame_counts[2],wait_counts[2],delay_counts[2];
    fd2_pcm_player primary={0},secondary={0};
    fd2_field_game *source=malloc(sizeof(game));fd2_vga *old=malloc(sizeof(screen));CHECK(source && old);
    for (int off=0;off<2;off++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
        actor(&game.units.items[0],9,2,1);game.option_values[1]=(uint8_t)!off;
        ctx.primary=&primary;ctx.secondary=&secondary;expected_primary=&primary;expected_secondary=&secondary;
        uint8_t targets[]={1};fd2_field_magic_present_event event=magic_event(3,targets,1);event.cast_cost_count=1;
        *source=game;capture_sequence=1;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        fx_recording recording={0};recording.seed=17;
        fd2_magic_fx_parts parts={ctx.magic_exchange->part_durations,ctx.magic_exchange->particles.clip.part_count};
        CHECK(fd2_magic_fx_init(&recording.fx,3,2,parts)==0);unsigned count;
        event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
        CHECK(record_fx_segment(&recording,&event,0,0,&count)==0);
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        fd2_field_magic_basic_result result={0};result.hit=1;result.hp_before=42;result.hp_after=28;result.raw_damage=14;
        event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;event.target_index=1;
        event.target_before=&game.units.items[1];event.result=&result;
        CHECK(record_fx_segment(&recording,&event,0,1,&count)==0);
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        CHECK(!memcmp(&ctx.magic_exchange->fx,&recording.fx,sizeof(recording.fx)));
        event.phase=FD2_FIELD_MAGIC_PRESENT_FINISH;
        CHECK(record_fx_segment(&recording,&event,0,0,&count)==0);
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        hashes[off]=frame_digest;frame_counts[off]=frames;wait_counts[off]=ticks;delay_counts[off]=delays;
        CHECK(off ? (!cues && !secondary_cues && !primary_stops):
            (cues>0 && secondary_cues>0 && primary_stops==1));
        CHECK(!wrong_stop && unchanged_except_visual(&game,source) && !business_rng_calls);
        fd2_battle_animation_close(&ctx);
    }
    CHECK(hashes[0]==hashes[1] && frame_counts[0]==frame_counts[1] &&
        wait_counts[0]==wait_counts[1] && delay_counts[0]==delay_counts[1]);
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);actor(&game.units.items[0],9,2,1);
    ctx.primary=&primary;ctx.secondary=&secondary;expected_primary=&primary;expected_secondary=&secondary;
    uint8_t targets[]={1};fd2_field_magic_present_event event=magic_event(4,targets,1);event.cast_cost_count=1;
    *source=game;*old=screen;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    fx_recording recording={0};recording.seed=99;unsigned count;
    fd2_magic_fx_parts parts={ctx.magic_exchange->part_durations,ctx.magic_exchange->particles.clip.part_count};
    CHECK(fd2_magic_fx_init(&recording.fx,4,2,parts)==0);
    event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
    CHECK(record_fx_segment(&recording,&event,0,0,&count)==0);
    quit_frame=5;CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && frames==5 && ctx.host_quit);
    fd2_field_magic_basic_result result={0};result.hit=1;result.hp_before=42;result.hp_after=28;result.raw_damage=14;
    event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;event.target_index=1;
    event.target_before=&game.units.items[1];event.result=&result;
    CHECK(record_fx_segment(&recording,&event,0,1,&count)==0);
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && frames==5);
    CHECK(!memcmp(&ctx.magic_exchange->fx,&recording.fx,sizeof(recording.fx)));
    event.phase=FD2_FIELD_MAGIC_PRESENT_FINISH;
    CHECK(record_fx_segment(&recording,&event,0,0,&count)==0);
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && !ctx.magic_exchange && frames==5);
    CHECK(!cues && !return_calls && !wrong_stop && !business_rng_calls);
    CHECK(!memcmp(&game,source,sizeof(game)) && !memcmp(screen.framebuffer,old->framebuffer,64000) &&
        !memcmp(screen.palette,old->palette,768) && !memcmp(screen.dac,old->dac,768));
    fd2_battle_animation_close(&ctx);free(source);free(old);return 0;
}

static int test_visual_owner_and_early_exit(void) {
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    fd2_field_game *before=malloc(sizeof(game));CHECK(before);*before=game;
    uint8_t targets[]={1};fd2_field_magic_present_event event=magic_event(13,targets,1);event.cast_cost_count=1;
    ctx.visual_owner=before; /* 不得把回调const指针强转，或推进另一个Game的锚。 */
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==-1);
    CHECK(!ctx.magic_exchange && !visual_calls && !memcmp(&game,before,sizeof(game)) && !frames);
    ctx.visual_owner=&game;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && !visual_calls);
    mask_failure=1;event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==-1 && !visual_calls);
    CHECK(magic_finish(&ctx,&event,1)==0 && !ctx.magic_exchange && !visual_calls);
    CHECK(!memcmp(&game,before,sizeof(game)));fd2_battle_animation_close(&ctx);
    CHECK(!ctx.visual_owner);
    CHECK(setup(&ctx)==0);*before=game;event=magic_event(13,targets,1);event.cast_cost_count=1;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    screen.input.quit_requested=1;event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && ctx.host_quit && !visual_calls);
    CHECK(magic_finish(&ctx,&event,0)==0 && !visual_calls && !frames && !ctx.magic_exchange);
    CHECK(!memcmp(&game,before,sizeof(game)));fd2_battle_animation_close(&ctx);free(before);return 0;
}


/* skip只停演出帧；已证11cac正常返回仍须推进同一视觉owner。 */
static int test_skip_return_visual_clock(int magic) {
    for(int late=1;late>=0;late--) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
        fd2_field_game *before=malloc(sizeof(game));CHECK(before);*before=game;
        uint8_t targets[]={1};fd2_field_magic_present_event m=magic_event(13,targets,1);m.cast_cost_count=1;
        fd2_field_physical_event p=make_event(FD2_FIELD_PHYSICAL_PREPARE);
        fd2_field_attack_result result={0};result.hit=1;result.rolled_damage=9;result.hp_after=33;
        CHECK((magic ? fd2_battle_animation_magic_present(&game,&m,&ctx):
            fd2_battle_animation_physical_present(&game,&p,&ctx))==0);
        if(late) {skip_context=&ctx;skip_frame=2;} else fd2_battle_animation_set_skip(&ctx,1);
        if(magic) {
            m.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
            CHECK(fd2_battle_animation_magic_present(&game,&m,&ctx)==0);
        } else {
            p.phase=FD2_FIELD_PHYSICAL_STRIKE;p.actor_before=&game.units.items[0];
            p.target_before=&game.units.items[1];p.result=&result;
            CHECK(fd2_battle_animation_physical_present(&game,&p,&ctx)==0);
        }
        CHECK(ctx.skip && !ctx.host_quit && !visual_calls);
        CHECK(frames==(late?2u:0u));
        if(late) fake_ms=5000; /* 宿主已经过阈值，但只在normal11cac点提交一次。 */
        unsigned old_frames=frames,old_ticks=ticks,old_delays=delays,old_cues=cues;
        CHECK((magic ? magic_finish(&ctx,&m,0):finish(&ctx,0))==0);
        int correct=late ? game.idle_phase==1 && game.last_idle_ms==5000 &&
            game.terrain_visual_phase==1 && game.last_terrain_visual_ms==5000 && visual_calls==1:
            !memcmp(&game,before,sizeof(game));
        int business_same=unchanged_except_visual(&game,before);
        int no_more_frames=frames==old_frames && ticks==old_ticks && delays==old_delays && cues==old_cues;
        int drawn=return_calls==1 && (magic?effect_return_calls==1:normal_return_calls==1);
        fd2_battle_animation_close(&ctx);free(before);skip_context=NULL;
        CHECK(correct && business_same && no_more_frames && drawn && !business_rng_calls);
    }
    return 0;
}

int main(void) {
    CHECK(fd2_archive_open(&fdother,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_archive_open(&fdtxt,"original_game/FDTXT.DAT")==0);
    CHECK(fd2_archive_open(&figani_fixture,"original_game/FIGANI.DAT")==0);
    CHECK(fd2_field_info_assets_open(&game.info_assets,&fdother)==0);
    CHECK(fd2_font_open_fdother(&game.font,&fdother,4)==0);
    CHECK(fd2_text_entry_open_entry(&game.ui_text,&fdtxt,0)==0);
    CHECK(fd2_field_visuals_open(&game.visuals,&fdother)==0);
    CHECK(fd2_map_sprite_bank_open(&game.sprites,"original_game/FDICON.B24")==0);
    uint32_t cells[16]={0};fd2_terrain_attr attrs[1]={{0,0,3,0}};
    game.map.width=4;game.map.height=4;game.map.cells=cells;
    game.terrain.attrs=attrs;game.terrain.attr_count=1;game.units.count=2;
    CHECK(test_mask_and_clipping()==0);
    CHECK(test_prepare_atomic_and_skip()==0);
    CHECK(test_scene_source_contract()==0);
    CHECK(test_real_scene_and_quit()==0);
    CHECK(test_split_reverse_and_failure()==0);
    CHECK(test_marker_layers_idle_and_miss()==0);
    CHECK(test_sfx_option_gate()==0);
    CHECK(test_exact_dac_intro_and_fade()==0);
    CHECK(test_magic_zero_cost_metadata_and_skip()==0);
    CHECK(test_magic_zero_impacts_layers_and_targets()==0);
    CHECK(test_magic_heal_raw_numbers_and_quit()==0);
    int physical_skip=test_skip_return_visual_clock(0);
    int magic_skip=test_skip_return_visual_clock(1);
    CHECK(physical_skip==0 && magic_skip==0);
    CHECK(test_visual_owner_and_early_exit()==0);
    CHECK(test_magic_resource_error_and_tint()==0);
    CHECK(test_full_magic_all_handlers_and_persistent_state()==0);
    CHECK(test_full_magic_scope_tape_and_no_damage()==0);
    CHECK(test_full_magic_audio_gate_and_quit_replay()==0);
    fd2_field_visuals_close(&game.visuals);fd2_map_sprite_bank_close(&game.sprites);
    fd2_text_entry_close(&game.ui_text);fd2_field_info_assets_close(&game.info_assets);
    CHECK(!business_rng_calls);
    CHECK(figani_fixture.data && figani_fixture.count==408); /* 服务未关闭借用的Game资源。 */
    fd2_archive_close(&figani_fixture);fd2_archive_close(&fdtxt);fd2_archive_close(&fdother);
    puts("battle_animation: 17 headless groups PASS; no device/window/RNG/effect");return 0;
}
