#include <stdint.h>
/* B6物理服务离屏回归：2e2b0/2ebe1/2e9a8/2f4d4/2f631原调用边界。
 * present/BIOS等待/PCM启动替身不创建窗口、不打开设备；真实目录/RLE/summary照常执行。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "input.h"
void battle_test_input_pump(fd2_input *input);

#define SDL_GetTicks battle_test_ticks
uint64_t battle_test_ticks_ns(void);
#define SDL_GetTicksNS battle_test_ticks_ns
#define fd2_pcm_play_replace battle_test_pcm_replace
#define fd2_pcm_stop battle_test_pcm_stop
#define fd2_input_begin_frame battle_test_input_pump
#include "../battle_animation.c"
#undef fd2_pcm_play_replace
#undef fd2_pcm_stop
#undef fd2_input_begin_frame
#undef SDL_GetTicks

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x); return -1; } } while(0)
static unsigned frames, ticks, delays, cues, primary_stops, secondary_cues;
static unsigned input_pumps;
void battle_test_input_pump(fd2_input *input) { (void)input;input_pumps++; }
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
static unsigned preamble_capture,preamble_waits,preamble_bad_wait;
static unsigned composite_capture,composite_slot,composite_delay6,composite_bad_delay;
static unsigned composite_quit_at_six,composite_quit_after_return;
static unsigned return_clear_watch,return_fade_delay2,return_bad_clear;
static unsigned return_host_before_refresh,return_host_after_refresh;
static unsigned composite_stop_count,composite_stop_bank_flags[4];
static fd2_battle_animation *composite_owner;
static uint8_t composite_base_palette[768];
static void composite_observe_delay(uint32_t value);
static unsigned ray_capture,ray_wait_count,ray_quit_at_six,ray_white_calls,ray_bad_white;
static uint32_t ray_wait_values[32];static unsigned ray_wait_frames[32];
static uint8_t preamble_colors[8][3];
static unsigned dark_sample_count, bad_white_wait, delay2_calls, return_calls;
static unsigned normal_return_calls, effect_return_calls;
static unsigned background_return_calls;
static int effect_info_marker;
static uint16_t returned_hp;
static fd2_battle_animation *magic_watch;
static uint32_t magic_hashes[64];static size_t magic_hash_count;
static unsigned mask_calls;static int mask_failure;
static int mask_radius[64],mask_x[64],mask_y[64];
static int wrapper_circle_observe;
static uint32_t last_present_hash;static unsigned stale_wait200;
static int capture_sequence;static uint64_t frame_digest;
static int capture_first;static unsigned captured_count;static uint8_t first_frame[64000];
static uint8_t *map_captured;
static size_t map_captured_count;
static int capture_map;
static int quake_observe;static unsigned quake_cue_count,quake_bad_order,quake_frame_base;
static const uint8_t *quake_before;
static const uint8_t *map_cue_banks[64];
static size_t map_cue_indices[64],map_cue_count;
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
/* 转职独立组合probe只观察真实上传，不替换stage或源绘制。 */
static void (*f4_promotion_observe)(fd2_vga *vga);
/* MAP history supplies declared refresh/terrain pages, never a live clock oracle. */
static int (*f4_map_refresh_override)(fd2_field_game *,fd2_vga *);
static int (*f4_map_terrain_override)(fd2_field_game *,fd2_vga *,fd2_field_page *);

void fd2_vga_clear(fd2_vga *vga,uint8_t fill) { memset(vga->framebuffer,fill,64000); }
static void battle_observe_frame(fd2_vga *vga) {
    frames++;
    if(f4_promotion_observe)f4_promotion_observe(vga);
    if(return_clear_watch && return_host_before_refresh && return_fade_delay2==63u)
        vga->input.quit_requested=1;
    if(preamble_capture && frames<=8)memcpy(preamble_colors[frames-1],vga->dac,3);
    if (capture_map && map_captured && map_captured_count<128)
        memcpy(map_captured+64000u*map_captured_count++,vga->framebuffer,64000);
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
int fd2_vga_present(fd2_vga *vga) {
    /* 模拟旧present对foreign owner的消费，不能用无副作用spy掩盖。 */
    vga->frame_deadline_ns=vga->frame_interval_ns=0;battle_observe_frame(vga);
return 0;
}
int fd2_wait_ticks(uint32_t value) {
#ifdef FD2_CLOCK_FAILURE_TEST
    if(clock_fault_wait()<0)return -71;
#endif
    if(preamble_capture){preamble_waits++;if(value!=1u || frames!=preamble_waits)preamble_bad_wait++;}
    if(ray_capture && ray_wait_count<32u) {
        ray_wait_values[ray_wait_count]=value;ray_wait_frames[ray_wait_count++]=frames;
        if(ray_quit_at_six && value==6u)screen.input.quit_requested=1;
    }
    ticks+=value;fake_ms+=(uint64_t)value*55u;
return 0;
}
int fd2_delay_ms(uint32_t value) {
#ifdef FD2_CLOCK_FAILURE_TEST
    if(clock_fault_wait()<0)return -71;
#endif
    composite_observe_delay(value);
    if(return_clear_watch && value==2u)return_fade_delay2++;
    delays+=value;fake_ms+=value;
    if (value==2) delay2_calls++;
    if (value==200 && last_present_hash!=hud_hash(screen.framebuffer)) stale_wait200++;
    if (value==40 && (last_present_dac[0] || last_present_dac[1] || last_present_dac[2]))
        bad_white_wait++;
return 0;
}
int fd2_vga_present_timed(fd2_vga *vga,uint32_t frame_ms) {
    fd2_vga_present(vga);fd2_delay_ms(frame_ms);
return 0;
}
/* upload只观察提交帧/退出，不消费deadline，不代真实设备。 */
void fd2_vga_upload_current_frame(fd2_vga *vga) {
    battle_observe_frame(vga);
#ifdef FD2_CLOCK_FAILURE_TEST
    clock_fault_upload(vga);
#endif
}
int fd2_vga_wait_frame_deadline(fd2_vga *vga) { (void)vga; return 0;
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
    if (effect_info_marker) vga->framebuffer[184u*320u+20u]=233; /* 模型区分mode3信息板。 */
}
void fd2_field_game_render_background(const fd2_field_game *current,fd2_vga *vga) {
    background_return_calls++;map_background(current,vga);
}
void fd2_field_game_render_effect_tiles(const fd2_field_game *current,fd2_vga *vga) {
    map_background(current,vga);
}
void fd2_field_game_render_effect_actors(const fd2_field_game *current,fd2_vga *vga) {
    for (size_t i=0;i<current->units.count;i++) {
        int x=((int)current->units.items[i].x-current->camera_cell_x)*24+4;
        int y=((int)current->units.items[i].y-current->camera_cell_y)*24+4;
        if (x>=4 && x<316 && y>=4 && y<196) vga->framebuffer[y*320+x]=67;
    }
}
/* 只校呈现如何消费三张既有快照；实际地形缩放由field_effect测试覆盖。 */
int fd2_field_game_prepare_earthquake(fd2_field_game *current,fd2_vga *vga,
        fd2_field_effect_frames *result) {
    (void)current;(void)vga;
    if (fd2_field_effect_frames_open(result,FD2_FIELD_EFFECT_EARTHQUAKE,3)!=0) return -1;
    for (size_t i=0;i<3;i++) {
        uint8_t *snapshot=fd2_field_effect_snapshot(result,i);
        if(quake_observe) {
            memcpy(snapshot,quake_before,64000);
            for(size_t y=4;y<196;y++)memset(snapshot+y*320u+4u,20+(int)i,312);
        } else memset(snapshot,20+(int)i,64000);
    }
    return 0;
}
int fd2_field_game_begin_earthquake(fd2_field_game *current,fd2_vga *vga,
        fd2_field_effect_frames *result) {
    return fd2_field_game_prepare_earthquake(current,vga,result);
}
int fd2_field_game_effect_actors(const fd2_field_game *current,fd2_vga *vga) {
    fd2_field_game_render_effect_actors(current,vga);return 0;
}
static unsigned visual_calls,terrain_calls;
static uint64_t visual_stamps[64];
static unsigned runtime_calls,runtime_phase,runtime_frames;
static uint32_t runtime_display_mode;
static int runtime_failure;
static uint8_t runtime_pixel(unsigned phase,int x,int y) {
    return (uint8_t)(40u+((unsigned)x*7u+(unsigned)y*11u+phase*3u)%173u);
}
/* 只签Battle如何调用原phase叶/借App页；逐读取钟、selection/info及
 * 11eee/127a9真实语义由Game/unit_render/live-clock测试独立验证。 */
int fd2_field_game_runtime_refresh(fd2_field_game *current,fd2_vga *vga,int phase) {
    if(current!=&game || vga!=&screen || !vga->field_page ||
        current->visual_owner!=&vga->field_visual || (phase!=1 && phase!=3))return -1;
    runtime_calls++;runtime_phase=(unsigned)phase;runtime_frames=frames;
    runtime_display_mode=current->ordinary_display_mode;
    if(return_clear_watch) {
        if(return_fade_delay2!=64u)return_bad_clear++;
        for(size_t i=0;i<64000u;i++)if(vga->framebuffer[i])return_bad_clear++;
        for(size_t i=0;i<768u;i++)if(vga->dac[i])return_bad_clear++;
    }
    if(runtime_failure) {
        uint8_t prefix=239;
        if(fd2_field_page_write(vga->field_page,72,72,1,1,&prefix,1,1)!=0)return -1;
        if(composite_capture && composite_quit_after_return)vga->input.quit_requested=1;
        return -1;
    }
    uint8_t row[312];
    for(int y=0;y<192;y++) {
        for(int x=0;x<312;x++)row[x]=runtime_pixel((unsigned)phase,x,y);
        if(fd2_field_page_write(vga->field_page,72,72+y,312,1,row,sizeof(row),312)!=0)return -1;
    }
    int result=phase==3 ? fd2_field_page_project(vga->field_page,72,72,
        vga->framebuffer,sizeof(vga->framebuffer)):0;
    if((composite_capture && composite_quit_after_return) ||
        (return_clear_watch && return_host_after_refresh))vga->input.quit_requested=1;
    return result;
}
/* 此处只校接线/退出；真正1297d/11eee实现由field_unit_render真实Game回归验证。 */
int fd2_field_game_tick_terrain_visual(fd2_field_game *g,uint64_t now_ns) {
    uint64_t now=now_ns/UINT64_C(1000000);
    if(!g || !g->ready || g->failure) return -1;
    terrain_calls++;
    if(!g->last_terrain_visual_ms)g->last_terrain_visual_ms=now;
    else if(now<g->last_terrain_visual_ms || now-g->last_terrain_visual_ms>=55) {g->terrain_visual_phase^=1u;g->last_terrain_visual_ms=now;}
    return 0;
}
int fd2_field_game_tick_visual(fd2_field_game *g,uint64_t now_ns) {
    uint64_t now=now_ns/UINT64_C(1000000);
    if(!g || !g->ready || g->failure) return -1;
    if(visual_calls<64)visual_stamps[visual_calls]=now;
    visual_calls++;
    if(!g->last_idle_ms)g->last_idle_ms=now;
    else if(now<g->last_idle_ms || now-g->last_idle_ms>=275) {g->idle_phase=(g->idle_phase+1u)&3u;g->last_idle_ms=now;}
    return fd2_field_game_tick_terrain_visual(g,now_ns);
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
    (void)current;
    if (mask_calls<64) {mask_radius[mask_calls]=radius;mask_x[mask_calls]=x;mask_y[mask_calls]=y;}
    mask_calls++;if (mask_failure) return -1;
    if(wrapper_circle_observe) {
        for(size_t row=4;row<196;row++)memset(vga->framebuffer+row*320u+4u,lut[7],312);
    } else memset(vga->framebuffer,lut[7],64000);
    return 0;
}
/* 本文件只检呈现adapter；完整Game/page/时钟语义由unit_render覆盖。 */
int fd2_field_game_effect_terrain_begin(fd2_field_game *current,fd2_vga *vga,fd2_field_page *page) {
    if(f4_map_terrain_override)return f4_map_terrain_override(current,vga,page);
    (void)current;return fd2_field_page_copy_pixels(page,vga->field_page);
}
int fd2_field_game_effect_mask_frame(const fd2_field_game *current,fd2_vga *vga,
    const fd2_field_page *base,const uint8_t lut[256],int x,int y,int radius,int first,int circle12) {
    (void)first;(void)circle12;
    if (fd2_field_page_copy_pixels(vga->field_page,base)!=0) return -1;
    return fd2_field_game_render_transition_mask(current,vga,lut,x,y,radius);
}
int fd2_field_game_effect_refresh(fd2_field_game *current,fd2_vga *vga) {
    if(f4_map_refresh_override)return f4_map_refresh_override(current,vga);
    if (fd2_field_game_tick_visual(current,(battle_test_ticks())*UINT64_C(1000000))!=0) return -1;
    fd2_field_game_render_effect_background(current,vga);return 0;
}

static int test_effect_page_stamp(void) {
    fd2_field_page *page=fd2_field_page_create();CHECK(page);
    uint8_t pixels[]={0,5,6,7,8,9},opaque[]={1,0,1,0,1,1},row[4];
    battle_image image={{3,2,pixels},opaque};
    uint8_t before[]={41,42,43,44};
    CHECK(fd2_field_page_write(page,71,71,4,1,before,4,4)==0);
    CHECK(battle_page_image(page,&image,71,71,-1)==0);
    CHECK(fd2_field_page_read(page,71,71,4,1,row,4,4)==0);
    CHECK(row[0]==0 && row[1]==42 && row[2]==6 && row[3]==44);
    CHECK(battle_page_image(page,&image,-2,335,200)==0);
    CHECK(fd2_field_page_read(page,0,335,2,1,row,4,2)==0 && row[0]==200 && row[1]==0);
    CHECK(battle_page_image(page,&image,456,0,-1)==0);
    fd2_field_page_destroy(page);return 0;
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
/* 仅观察新caller如何传原pal_partial_set参数；实际DAC加法的独立回归归vga。 */
int fd2_vga_palette_add(fd2_vga *vga,int first,int last,int32_t value) {
    if(!vga || first!=0 || last!=255 || value<0 || value>64)return -1;
    if(ray_capture && (ray_white_calls>=65 || value!=64-(int32_t)ray_white_calls))ray_bad_white++;
    ray_white_calls++;
    return 0;
}
int battle_test_pcm_replace(fd2_pcm_player *player,const fd2_pcm_bank *bank,
        size_t index,int loops,float gain) {
#ifdef FD2_CLOCK_FAILURE_TEST
    if(clock_audio_at)return clock_audio_return(0);
#endif
    if (capture_map && map_cue_count<64) {
        map_cue_banks[map_cue_count]=bank->archive.data;
        map_cue_indices[map_cue_count++]=index;
    }
    if(quake_observe) {
        if(index!=13u || frames!=quake_frame_base+quake_cue_count*6u)quake_bad_order++;
        /* 首cue在原before，之后frame6k前一源恒1；不是本帧0/2。 */
        for(size_t y=0;y<200;y++)for(size_t x=0;x<320;x++) {
            size_t at=y*320u+x;uint8_t expected=quake_before[at];
            if(quake_cue_count && x>=4 && x<316 && y>=4 && y<196)expected=21;
            if(screen.framebuffer[at]!=expected)quake_bad_order++;
        }
        quake_cue_count++;
    }
    if ((player!=expected_primary && player!=expected_secondary) || loops!=1 || gain!=(101.0f/128.0f)) return -1;
    if (player==expected_secondary) secondary_cues++;
    cues++; last_cue=index;
    return cue_failure ? -1:0;
}
int battle_test_pcm_stop(fd2_pcm_player *player) {
#ifdef FD2_CLOCK_FAILURE_TEST
    if(clock_audio_at)return clock_audio_return(0);
#endif
    if(composite_owner && composite_owner->magic_exchange && composite_stop_count<4u) {
        const fd2_battle_magic_exchange *e=composite_owner->magic_exchange;
        composite_stop_bank_flags[composite_stop_count++]=(e->sound.archive.data?1u:0u)|
            (e->map_sound.archive.data?2u:0u);
    }
    if (player!=expected_primary) { wrong_stop++; return -1; }
    primary_stops++; return 0;
}
static void reset_observer(void) {
    return_clear_watch=return_fade_delay2=return_bad_clear=0;
    return_host_before_refresh=return_host_after_refresh=0;
    composite_capture=composite_slot=composite_delay6=composite_bad_delay=0;
    composite_quit_at_six=composite_quit_after_return=composite_stop_count=0;composite_owner=NULL;
    memset(composite_stop_bank_flags,0,sizeof(composite_stop_bank_flags));
    ray_capture=ray_wait_count=ray_quit_at_six=ray_white_calls=ray_bad_white=0;
    preamble_capture=preamble_waits=preamble_bad_wait=0;memset(preamble_colors,0,sizeof(preamble_colors));
    frames=ticks=delays=cues=primary_stops=quit_frame=wrong_stop=secondary_cues=0; cue_failure=0;skip_frame=0;skip_context=NULL;
    magic_watch=NULL;magic_hash_count=0;mask_calls=0;mask_failure=0;stale_wait200=0;capture_first=0;captured_count=0;
    watch=NULL;watched_count=0;dark_sample_count=bad_white_wait=delay2_calls=return_calls=0;
    normal_return_calls=effect_return_calls=0;
    background_return_calls=0;effect_info_marker=0;
    runtime_calls=runtime_phase=runtime_frames=0;runtime_display_mode=0;runtime_failure=0;
    screen.input.quit_requested=0;
    capture_sequence=0;frame_digest=UINT64_C(14695981039346656037);
    capture_map=0;map_captured_count=map_cue_count=0;
    quake_observe=0;quake_cue_count=quake_bad_order=quake_frame_base=0;quake_before=NULL;wrapper_circle_observe=0;
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
    game.active_side=0;game.selected_unit=-1;game.interaction=FD2_FIELD_INTERACTION_BROWSE;
    game.option_values[3]=1;
    game.attack_rng=forbidden_rng;game.attack_rng_userdata=&business_rng_calls;
    actor(&game.units.items[0],0,2,1);actor(&game.units.items[1],96,0,2);
    memset(screen.framebuffer,71,64000);memset(screen.palette,13,768);memset(screen.dac,47,768);
    fake_ms=1000;visual_calls=terrain_calls=0;game.ready=1;game.failure=FD2_FIELD_FAILURE_NONE;
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
    game.stage=2;CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    CHECK(finish(&ctx,1)==0);
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
    CHECK(!memcmp(screen.palette,before->palette,768));
    /* 原1f525的darkness0逐通道写base，而非入场DAC snapshot。 */
    for(size_t i=0;i<768u;i++)CHECK(screen.dac[i]==
        (uint8_t)((before->palette[i]<<2)|(before->palette[i]>>4)));
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
        CHECK(!memcmp(screen.palette,before->palette,768));
        /* 原1f525的darkness0逐通道写base，而非入场DAC snapshot。 */
        for(size_t i=0;i<768u;i++)CHECK(screen.dac[i]==
            (uint8_t)((before->palette[i]<<2)|(before->palette[i]>>4)));
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

/* sub1d6c8完整原212B：四次[DAC0=color;wait1;DAC0=black;wait1]。
 * id13原strict镜像51aba/51ade/51b02为RGB6=63/61/46，独立RGB8=255/247/186；
 * 不从新sequence或生成表反推expected。时钟本体另由bios_wait/live测试覆盖。 */
static int test_player_map_caller_preamble(void) {
    fd2_field_game *before=malloc(sizeof(*before));CHECK(before);
    for(int kind=0;kind<6;kind++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);game.active_side=2;
        ctx.primary=(fd2_pcm_player *)(uintptr_t)1;expected_primary=ctx.primary;
        if(kind==1)game.option_values[1]=0;
        if(kind==2)fd2_battle_animation_set_skip(&ctx,1);
        if(kind==3){skip_context=&ctx;skip_frame=1;}
        if(kind==4)quit_frame=1;
        if(kind==5)cue_failure=1;
        fd2_field_magic_present_event event={0};event.phase=FD2_FIELD_MAGIC_PRESENT_MAP_CALLER_PREAMBLE;
        event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;event.magic_id=event.effect_id=13;
        *before=game;preamble_capture=1;
        int result=fd2_battle_animation_magic_present(&game,&event,&ctx);
        CHECK(result==(kind==4?1:kind==5?-1:0));
        CHECK(!memcmp(before,&game,sizeof(game)) && !ctx.exchange && !ctx.magic_exchange && !business_rng_calls);
        CHECK(!preamble_bad_wait && !delays && !secondary_cues && !wrong_stop);
        if(kind<2) {
            CHECK(frames==8 && ticks==8 && preamble_waits==8 && cues==(kind?0u:1u) && !primary_stops);
            for(size_t i=0;i<8;i++) {
                const uint8_t color[3]={255,247,186},black[3]={0,0,0};
                CHECK(!memcmp(preamble_colors[i],i%2?black:color,3));
            }
            CHECK(!screen.dac[0] && !screen.dac[1] && !screen.dac[2]);
        } else if(kind==2 || kind==5) {
            CHECK(!frames && !ticks && !preamble_waits && cues==(kind==5?1u:0u) && !primary_stops);
            CHECK(screen.dac[0]==47 && screen.dac[1]==47 && screen.dac[2]==47);
        } else {
            const uint8_t color[3]={255,247,186};
            CHECK(frames==1 && !ticks && !preamble_waits && cues==1 && !memcmp(screen.dac,color,3));
            CHECK(!!ctx.host_quit==(kind==4) && primary_stops==(kind==4?1u:0u));
        }
        for(size_t i=0;i<64000;i++)CHECK(screen.framebuffer[i]==71);
        for(size_t i=3;i<768;i++)CHECK(screen.dac[i]==47);
        fd2_battle_animation_close(&ctx);
    }
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);game.active_side=2;
    fd2_field_magic_present_event event={0};event.phase=FD2_FIELD_MAGIC_PRESENT_MAP_CALLER_PREAMBLE;
    event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;event.magic_id=13;
    for(int kind=0;kind<5;kind++) {
        fd2_field_magic_present_event bad=event;
        if(kind==0)bad.magic_id=8;
        if(kind==1)bad.magic_id=24;
        if(kind==2)bad.presentation_mode=FD2_FIELD_MAGIC_MODE_FULL;
        if(kind==3)bad.action_kind=FD2_FIELD_MAGIC_ACTION_ITEM;
        if(kind==4)bad.actor_index=game.units.count;
        CHECK(fd2_battle_animation_magic_present(&game,&bad,&ctx)==-1);
        CHECK(!frames && !ticks && !cues && !ctx.magic_exchange);
    }
    fd2_battle_animation_close(&ctx);
    CHECK(setup(&ctx)==0);game.active_side=2;
    uint8_t target[]={1};event=magic_event(13,target,1);
    event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;event.cast_cost_count=1;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && ctx.magic_exchange);
    event.phase=FD2_FIELD_MAGIC_PRESENT_MAP_CALLER_PREAMBLE;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==-1 && ctx.magic_exchange && !frames && !ticks);
    event.phase=FD2_FIELD_MAGIC_PRESENT_FINISH;event.action_failed=1;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && !ctx.magic_exchange);
    fd2_battle_animation_close(&ctx);free(before);return 0;
}
static int magic_finish(fd2_battle_animation *ctx,fd2_field_magic_present_event *e,int failure) {
    e->phase=FD2_FIELD_MAGIC_PRESENT_FINISH;e->action_failed=failure;
    return fd2_battle_animation_magic_present(&game,e,ctx);
}
/* 独立原FIGANI58字节：unit19 casting共7part，duration=[2,2,10,2,2,2,4]，
 * cue仅part3=5。2dfc8从EDI=count2播part2..6，每part只提交一帧；
 * count7自然零part仍保wait6。固定renderer/clock替身只签路由与源序，
 * 不签自然高射程场景或原设备速率。variant7是明确合成duration0边界。 */
static int test_ai_ray_item_single_present(void) {
    for(unsigned variant=0;variant<8u;variant++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
        game.stage=0;game.units.count=8;game.active_side=0;
        actor(&game.units.items[0],19,0,1);
        for(size_t i=1;i<8;i++)actor(&game.units.items[i],96,2,2);
        game.cursor_cell_x=game.units.items[0].x;game.cursor_cell_y=game.units.items[0].y;
        fd2_field_visual_state *owner=game.visual_owner;game.visual_owner=&screen.field_visual;
        fd2_field_page *page=fd2_field_page_create();CHECK(page);screen.field_page=page;
        fd2_pcm_player primary={0};ctx.primary=&primary;expected_primary=&primary;
        uint8_t targets[]={1,2,3,4,5,6,7};
        fd2_field_magic_present_event event={0};
        event.phase=FD2_FIELD_MAGIC_PRESENT_AI_RAY_ITEM;
        event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;event.action_kind=FD2_FIELD_MAGIC_ACTION_ITEM;
        event.actor_index=0;event.magic_id=3;event.item_code=24;event.item_power=3;
        event.targets=targets;event.target_count=variant==1?7u:2u;
        fd2_field_game *before=malloc(sizeof(game));CHECK(before);*before=game;
        uint32_t before_frame_hash=hud_hash(screen.framebuffer);
        ray_capture=1;
        if(variant==2)ctx.skip=1;
        if(variant==3)quit_frame=74;
        if(variant==4)cue_failure=1;
        if(variant==5)ray_quit_at_six=1;
        if(variant==6)runtime_failure=1;
        uint8_t *duration=NULL,saved_duration=0;
        if(variant==7) {
            const uint8_t *clip;size_t size;CHECK(fd2_archive_get(&figani_fixture,58,&clip,&size)==0);
            uint32_t part=battle_u32(clip+8+2*4);CHECK(part+7u<size && clip[part+6]==10);
            duration=(uint8_t *)clip+part+6;saved_duration=*duration;*duration=0;
        }
        int result=fd2_battle_animation_magic_present(&game,&event,&ctx);
        if(duration)*duration=saved_duration;
        CHECK(result==(variant==3 || variant==5?1:variant==4 || variant==6?-1:0));
        CHECK(!ctx.exchange && !ctx.magic_exchange && !business_rng_calls && !wrong_stop &&
            unchanged_except_visual(&game,before));
        if(variant==0 || variant==1 || variant==7) {
            static const uint32_t expected[]={10,2,2,2,4,6};
            CHECK(frames==(variant==1?279u:284u) && ticks==(variant==1?6u:variant==7?16u:26u) &&
                delays==846 && runtime_calls==1 && runtime_phase==3 && mask_calls==10 &&
                ray_white_calls==65 && !ray_bad_white &&
                primary_stops==1 && cues==(variant==1?0u:1u) &&
                ray_wait_count==(variant==1?1u:6u));
            if(variant==1)CHECK(ray_wait_values[0]==6 && ray_wait_frames[0]==73);
            else for(unsigned i=0;i<6u;i++)CHECK(ray_wait_values[i]==(variant==7 && !i?0u:expected[i]) &&
                ray_wait_frames[i]==(i<5u?74u+i:78u));
            for(unsigned i=0;i<10;i++)CHECK(mask_radius[i]==80-4*(int)i &&
            mask_x[i]==((int)game.units.items[0].x-game.camera_cell_x)*24+12 &&
            mask_y[i]==((int)game.units.items[0].y-game.camera_cell_y)*24+18);
        } else if(variant==2)CHECK(!frames && !ticks && !delays && !primary_stops && !ray_wait_count);
        else if(variant==3)CHECK(frames==74 && !ticks && !runtime_calls && !mask_calls && primary_stops==1);
        else if(variant==4)CHECK(frames==74 && ticks==10 && ray_wait_count==1 && !runtime_calls &&
            !mask_calls && ctx.failed && primary_stops==1);
        else if(variant==5)CHECK(frames==78 && ticks==26 && ray_wait_count==6 && !runtime_calls &&
            !mask_calls && primary_stops==1);
        else {
            uint8_t prefix=0;CHECK(frames==142 && ticks==26 && runtime_calls==1 && !mask_calls &&
                ctx.failed && primary_stops==1 && fd2_field_page_read(page,72,72,1,1,&prefix,1,1)==0 && prefix==239);
        }
        if(variant==3 || variant==4)CHECK(hud_hash(screen.framebuffer)!=before_frame_hash);
        free(before);fd2_battle_animation_close(&ctx);
        game.visual_owner=owner;screen.field_page=NULL;fd2_field_page_destroy(page);
    }
    return 0;
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
    CHECK(ctx.magic_exchange->work[flash_at]==62 && ctx.magic_exchange->work[flash_at+1]==200);
    fd2_field_magic_basic_result result={0};result.hit=1;result.hp_before=result.hp_after=42;result.raw_heal=29;
    e.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;e.target_index=1;e.target_before=&game.units.items[1];e.result=&result;
    unsigned prior_frames=frames;CHECK(fd2_battle_animation_magic_present(&game,&e,&ctx)==0 && frames==prior_frames);
    CHECK(ctx.magic_exchange->raw_heal[0]==29); /* 满HP也显示29，不能拿HP差0冒充。 */
    fd2_vga *digits_expected=calloc(1,sizeof(*digits_expected));CHECK(digits_expected);
    memset(digits_expected->framebuffer,62,64000); /* effect_background替身当前HP42的无cursor整页。 */
    int dx=4+((int)game.units.items[1].x-game.camera_cell_x)*24+12;
    int dy=4+((int)game.units.items[1].y-game.camera_cell_y)*24+12;
    fd2_map_sprite_blit(digits_expected,&ctx.magic_exchange->digits[107].image,dx,dy,0);
    fd2_map_sprite_blit(digits_expected,&ctx.magic_exchange->digits[114].image,dx+5,dy,0);
    reset_observer();ctx.primary=&primary;expected_primary=&primary;capture_first=1;
    CHECK(magic_finish(&ctx,&e,0)==0 && frames==22 && delays==544 && primary_stops==1);
    CHECK(!normal_return_calls && effect_return_calls==1 && screen.framebuffer[319]==62);
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
    CHECK(magic_finish(&ctx,&e,0)==0 && frames==3 && primary_stops==1 && !ctx.magic_exchange);
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
    CHECK(magic_finish(&ctx,&e,1)==0 && !ctx.magic_exchange && !return_calls && primary_stops==1);
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
    /* FULL9仅缺失part的绘制由同FX engine省略，不能拒整次多目标动作。 */
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && ctx.magic_exchange);
    CHECK(!memcmp(&game,source,sizeof(game)) && !memcmp(&screen,old,sizeof(screen)) && !frames && !cues);
    CHECK(magic_finish(&ctx,&event,1)==0 && !ctx.magic_exchange);
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

static int test_map_magic_families(void) {
    uint8_t targets[]={1,2};
    for (unsigned side=0;side<=2;side+=2) for (uint8_t id=0;id<28;id++) {
        if (id==23) continue;
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
        game.units.count=3;actor(&game.units.items[0],9,(uint8_t)side,1);
        actor(&game.units.items[2],96,0,2);
        fd2_field_game *before=malloc(sizeof(game));CHECK(before);*before=game;
        fd2_field_magic_present_event event=magic_event(id,targets,id==9 ? 1u:2u);
        event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;
        event.effect_id=id==24 ? 16:id;event.cast_cost_count=1;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        CHECK(!frames && !cues && !ctx.backgrounds.data);
        if(id==11 || id==12) {
            event.phase=FD2_FIELD_MAGIC_PRESENT_MAP_WRAPPER_PRELUDE;
            CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        }
        event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && frames>0);
        fd2_field_magic_basic_result effect={0};effect.hit=1;effect.raw_damage=7;effect.raw_heal=9;
        for (size_t i=0;i<event.target_count;i++) {
            event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;event.target_index=targets[i];
            event.target_before=event.target_after=&game.units.items[targets[i]];event.result=&effect;
            CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        }
        CHECK(magic_finish(&ctx,&event,0)==0 && !ctx.magic_exchange);
        CHECK(unchanged_except_visual(&game,before) && !business_rng_calls);
        fd2_battle_animation_close(&ctx);free(before);game.units.count=2;
    }
    return 0;
}

/* 已声明Game叶只给3张可分辨页；实际Battle wrapper/cue/copy/upload/wait
 * 照常执行，独立0/1/2/1＋非零外框expected不从实际上传反生。 */
static int quake_refresh_fixture(fd2_field_game *current,fd2_vga *vga) {
    if(current!=&game || vga!=&screen)return -1;
    for(size_t y=4;y<196;y++)memset(vga->framebuffer+y*320u+4u,89,312);
    return 0;
}
static int test_quake_wrapper_cue_and_page_prefix(void) {
    for(unsigned outcome=0;outcome<4;outcome++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);game.units.count=2;
        actor(&game.units.items[0],9,2,1);
        fd2_pcm_player primary={0};ctx.primary=&primary;expected_primary=&primary;
        uint8_t targets[]={1};fd2_field_magic_present_event event=magic_event(10,targets,1);
        event.effect_id=10;event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;event.cast_cost_count=1;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        fd2_field_page *page=fd2_field_page_create();CHECK(page);screen.field_page=page;
        fd2_field_visual_state *old_owner=game.visual_owner;game.visual_owner=&screen.field_visual;
        uint8_t *before=malloc(64000),*expected=malloc(64000);fd2_field_game *gb=malloc(sizeof(game));CHECK(before && expected && gb);
        for(size_t i=0;i<64000;i++)before[i]=(uint8_t)(47u+i*11u+i/320u);
        memcpy(screen.framebuffer,before,64000);*gb=game;
        quake_before=before;quake_observe=1;capture_map=1;map_captured_count=0;
        f4_map_refresh_override=quake_refresh_fixture;
        if(outcome==1)cue_failure=1;
        if(outcome==2)quit_frame=7;
        if(outcome==3)screen.input.quit_requested=1;
        unsigned old_pumps=input_pumps; /* 既有observer是跨测试累计，源每upload一pump。 */
        event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==(outcome==1?-1:outcome>=2?1:0));
        capture_map=0;quake_observe=0;f4_map_refresh_override=NULL;
        unsigned uploads=outcome==0?61u:outcome==2?7u:0u;
        CHECK(frames==uploads && map_captured_count==uploads && !quake_bad_order && !ticks && !business_rng_calls);
        CHECK(quake_cue_count==(outcome==0?8u:outcome==1?1u:outcome==2?2u:0u));
        CHECK(delays==(outcome==0?600u:outcome==2?60u:0u) && input_pumps==old_pumps+uploads);
        CHECK(!memcmp(&game,gb,sizeof(game)));
        static const unsigned cycle[4]={0,1,2,1};
        for(unsigned frame=0;frame<uploads;frame++) {
            memcpy(expected,before,64000);
            for(size_t y=4;y<196;y++)memset(expected+y*320u+4u,frame==60u?89:20+(int)cycle[frame%4u],312);
            CHECK(!memcmp(map_captured+frame*64000u,expected,64000));
        }
        if(!uploads)memcpy(expected,before,64000);
        CHECK(!memcmp(screen.framebuffer,expected,64000));
        CHECK(ctx.failed==(outcome==1) && ctx.host_quit==(outcome>=2));
        CHECK(magic_finish(&ctx,&event,1)==0);fd2_battle_animation_close(&ctx);
        game.visual_owner=old_owner;screen.field_page=NULL;fd2_field_page_destroy(page);
        free(before);free(expected);free(gb);quake_before=NULL;
    }
    return 0;
}

/* 2185f/21a9e实参和三段调用边：cursor故意不同于actor。
 * 原叶接口给定LUT/terrain值，实际prelude→CAST wrapper完整执行。 */
static int test_radial_quake_wrapper_source_protocol(void) {
    for(unsigned id=11;id<=12;id++)for(unsigned outcome=0;outcome<3;outcome++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);game.units.count=2;
        actor(&game.units.items[0],9,2,1);game.units.items[0].y=1;
        game.cursor_cell_x=game.cursor_cell_y=3;game.camera_cell_x=game.camera_cell_y=0;
        fd2_pcm_player primary={0};ctx.primary=&primary;expected_primary=&primary;
        uint8_t targets[]={1};fd2_field_magic_present_event event=magic_event((uint8_t)id,targets,1);
        event.effect_id=(uint8_t)id;event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;event.cast_cost_count=1;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        fd2_field_page *page=fd2_field_page_create();CHECK(page);screen.field_page=page;
        fd2_field_visual_state *owner=game.visual_owner;game.visual_owner=&screen.field_visual;
        uint8_t *before=malloc(64000),*ring_before=malloc(64000),*expected=malloc(64000);
        fd2_field_game *gb=malloc(sizeof(game));CHECK(before && ring_before && expected && gb);
        for(size_t i=0;i<64000;i++)before[i]=(uint8_t)(31u+i*17u+i/320u);
        memcpy(screen.framebuffer,before,64000);memcpy(ring_before,before,64000);
        for(size_t y=4;y<196;y++)memset(ring_before+y*320u+4u,89,312);
        *gb=game;f4_map_refresh_override=quake_refresh_fixture;mask_calls=0;
        wrapper_circle_observe=1;capture_map=1;map_captured_count=0;
        if(outcome==1)mask_failure=1;
        if(outcome==2)quit_frame=3;
        event.phase=FD2_FIELD_MAGIC_PRESENT_MAP_WRAPPER_PRELUDE;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==(outcome==1?-1:outcome==2?1:0));
        unsigned ring_uploads=outcome==0?11u:outcome==2?3u:0u;
        CHECK(frames==ring_uploads && cues==1 && mask_calls==(outcome==1?1u:outcome==2?3u:10u));
        CHECK(!ticks && !delays && !business_rng_calls && !memcmp(gb,&game,sizeof(game)));
        for(unsigned i=0;i<mask_calls;i++)CHECK(mask_x[i]==36 && mask_y[i]==42 &&
            mask_radius[i]==(id==11?15+(int)i*10:30+(int)i*16));
        for(unsigned i=0;i<ring_uploads;i++) {
            memcpy(expected,before,64000);
            uint8_t color=i==10?89:fd2_field_lut_frame(&game.visuals,i)[7];
            for(size_t y=4;y<196;y++)memset(expected+y*320u+4u,color,312);
            CHECK(!memcmp(map_captured+i*64000u,expected,64000));
        }
        if(outcome==0) {
            CHECK(!memcmp(screen.framebuffer,ring_before,64000));
            wrapper_circle_observe=0;quake_before=ring_before;quake_observe=1;quake_frame_base=11;
            event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
            CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
            CHECK(frames==72 && map_captured_count==72 && cues==9 && quake_cue_count==8 && !quake_bad_order && delays==600);
            static const unsigned order[4]={0,1,2,1};
            for(unsigned i=0;i<61;i++) {
                memcpy(expected,ring_before,64000);
                for(size_t y=4;y<196;y++)memset(expected+y*320u+4u,i==60?89:20+(int)order[i%4u],312);
                CHECK(!memcmp(map_captured+(11u+i)*64000u,expected,64000));
            }
            CHECK(!memcmp(screen.framebuffer,expected,64000) && !ctx.failed && !ctx.host_quit);
        } else {
            if(!ring_uploads)memcpy(expected,before,64000);
            CHECK(!memcmp(screen.framebuffer,expected,64000));
            CHECK(ctx.failed==(outcome==1) && ctx.host_quit==(outcome==2));
        }
        CHECK(!memcmp(gb,&game,sizeof(game)) && !business_rng_calls && !ticks);
        quake_observe=wrapper_circle_observe=capture_map=0;f4_map_refresh_override=NULL;
        CHECK(magic_finish(&ctx,&event,1)==0);fd2_battle_animation_close(&ctx);
        game.visual_owner=owner;screen.field_page=NULL;fd2_field_page_destroy(page);
        free(before);free(ring_before);free(expected);free(gb);quake_before=NULL;
    }
    return 0;
}

static int test_derived_magic_parts(void) {
    static const uint8_t ids[]={24,28,29,30},casters[]={32,19,34,18};
    uint8_t targets[]={1,2};
    for (size_t kind=0;kind<4;kind++) for (unsigned side=0;side<=2;side+=2) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
        game.units.count=3;actor(&game.units.items[0],casters[kind],(uint8_t)side,1);
        actor(&game.units.items[2],96,0,2);
        fd2_figani_magic_cast plan;
        CHECK(fd2_figani_magic_action_plan(&plan,ctx.figani,casters[kind],ids[kind])==0 && !plan.clip.empty);
        fd2_field_magic_present_event event=magic_event(ids[kind],targets,2);
        event.presentation_mode=FD2_FIELD_MAGIC_MODE_DERIVED;event.cast_cost_count=plan.cost_marker_count;
        fd2_field_unit caster=game.units.items[0];event.actor_before=&caster;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        game.units.items[0].mp=(uint16_t)(caster.mp-plan.cost_marker_count*fd2_field_magic_record_get(ids[kind])[5]);
        event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        for (size_t target=0;target<2;target++) {
            fd2_field_unit before=game.units.items[targets[target]];
            fd2_field_magic_basic_result result={0};result.hit=1;result.signed_damage=20;
            event.target_index=targets[target];event.target_before=&before;
            event.target_after=&game.units.items[targets[target]];event.result=&result;
            unsigned markers=0;
            for (size_t part=plan.clip.intro_end;part<plan.clip.part_count;part++) {
                fd2_figani_part original;CHECK(fd2_figani_get_part(&plan.clip,part,&original)==0);
                if (original.effect_marker==1) markers++;
                game.units.items[targets[target]].hp=(uint16_t)(before.hp-20u*markers/(ids[kind]==28 ? 8u:1u));
                event.phase=FD2_FIELD_MAGIC_PRESENT_STEP;event.part_index=(uint16_t)part;event.marker_count=(uint8_t)markers;
                fd2_field_unit committed=game.units.items[targets[target]];
                CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
                CHECK(!memcmp(&committed,&game.units.items[targets[target]],sizeof(committed)));
            }
            CHECK(markers==plan.effect_marker_count);
            event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;
            CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        }
        CHECK(magic_finish(&ctx,&event,0)==0 && !ctx.magic_exchange && !business_rng_calls);
        fd2_battle_animation_close(&ctx);game.units.count=2;
    }
    /* 原marker在duration循环外；零时长仍接收已提交HP，不补造动画tick。 */
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);actor(&game.units.items[0],19,2,1);
    fd2_field_magic_present_event event=magic_event(28,targets,1);
    event.presentation_mode=FD2_FIELD_MAGIC_MODE_DERIVED;event.cast_cost_count=1;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    size_t part=ctx.magic_exchange->casting.clip.intro_end;
    CHECK(ctx.magic_exchange->casting.parts[part].part.effect_marker==1);
    ctx.magic_exchange->casting.parts[part].part.duration=0;
    fd2_field_unit old=game.units.items[1];game.units.items[1].hp=40;
    event.phase=FD2_FIELD_MAGIC_PRESENT_STEP;event.part_index=(uint16_t)part;event.marker_count=1;
    event.target_index=1;event.target_before=&old;event.target_after=&game.units.items[1];
    unsigned prior_frames=frames;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && frames==prior_frames);
    CHECK(ctx.magic_exchange->units[0].hp==40 && game.units.items[1].hp==40);
    CHECK(magic_finish(&ctx,&event,1)==0);fd2_battle_animation_close(&ctx);
    return 0;
}

/* 原4e127取caller stride456低byte，不读取arg_C；不反读生产表构造期望。 */
static int test_map_solid_source_colors(void) {
    static const uint8_t ids[]={13,17,18,19,22},colors[]={200,200,200,200,200};
    static const uint8_t codes[]={5,8,9,10,22};
    uint8_t target=1;
    for(size_t kind=0;kind<5;kind++) for(int item=0;item<2;item++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
        fd2_field_magic_present_event event=magic_event(ids[kind],&target,1);
        event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;event.effect_id=ids[kind];
        event.cast_cost_count=item?0:1;
        if(item) {event.action_kind=FD2_FIELD_MAGIC_ACTION_ITEM;event.item_code=codes[kind];}
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        fd2_battle_magic_exchange *e=ctx.magic_exchange;
        size_t phase=game.idle_phase==3?2u:game.idle_phase;
        const uint8_t *pixels=game.sprites.decoded_frames+((size_t)e->sprite_ids[0]*12+phase)*576;
        const fd2_field_unit *u=&e->units[0];
        int x=((int)u->x-game.camera_cell_x)*24+4;
        int y=((int)u->y-game.camera_cell_y)*24-6+4;
        size_t checked=0;
        for(int row=0;row<24;row++) for(int col=0;col<24;col++) if(pixels[row*24+col]) {
            CHECK(e->work[(size_t)(y+row)*320+(size_t)(x+col)]==colors[kind]);checked++;
        }
        CHECK(checked>100 && !business_rng_calls);
        CHECK(magic_finish(&ctx,&event,1)==0);fd2_battle_animation_close(&ctx);
    }
    return 0;
}

static int test_composite_magic_passes(void) {
    uint8_t targets[]={1,2};
    for (uint8_t id=32;id<=35;id++) for (int skip=0;skip<=1;skip++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
        game.units.count=3;actor(&game.units.items[0],52,2,1);actor(&game.units.items[2],96,0,2);
        fd2_figani_magic_cast plan;CHECK(fd2_figani_magic_action_plan(&plan,ctx.figani,52,id)==0);
        fd2_field_magic_present_event event=magic_event(id,targets,2);
        event.presentation_mode=FD2_FIELD_MAGIC_MODE_COMPOSITE;event.cast_cost_count=plan.cost_marker_count;
        event.group_pass=UINT8_MAX;event.effect_id=id;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        if (skip) fd2_battle_animation_set_skip(&ctx,1);
        event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        unsigned intro_delays=delay2_calls;
        char name[32];snprintf(name,sizeof(name),"magic.effect.%03u",id);
        const fd2_static_flow *flow=fd2_static_flow_lookup("magic",name);CHECK(flow);
        unsigned groups=0;event.group_pass=0;
        for (size_t node=0;node<flow->count;node++) {
            if (!strcmp(flow->nodes[node].op,"effect_group")) {
                event.effect_id=(uint8_t)(flow->nodes[node].args[0]<0 ? id:flow->nodes[node].args[0]);
                event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
                CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
                fd2_field_magic_basic_result result={0};result.hit=1;result.raw_damage=7;result.raw_heal=9;
                for (size_t target=0;target<2;target++) {
                    event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;event.target_index=targets[target];
                    event.target_before=event.target_after=&game.units.items[targets[target]];event.result=&result;
                    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
                }
                groups++;event.group_pass++;
            } else if (!strcmp(flow->nodes[node].op,"popup_clear")) {
                event.phase=FD2_FIELD_MAGIC_PRESENT_POPUP_CLEAR;
                CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
            }
        }
        CHECK(groups==(id>=34 ? 3u:1u));
        CHECK(delay2_calls-intro_delays==(skip ? 0u:groups*22u));
        CHECK(magic_finish(&ctx,&event,0)==0 && !ctx.magic_exchange && !business_rng_calls);
        CHECK(delay2_calls-intro_delays==(skip ? 0u:groups*22u));
        if (skip) CHECK(!frames && !cues);
        fd2_battle_animation_close(&ctx);game.units.count=2;
    }
    return 0;
}

static int test_teleport_write_boundaries(void) {
    uint8_t targets[]={1};
    for (int item=0;item<=1;item++) for (int skip=0;skip<=1;skip++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);actor(&game.units.items[0],52,2,1);
        fd2_field_unit before=game.units.items[1];
        fd2_field_magic_present_event event=magic_event(23,targets,1);
        event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;event.effect_id=23;event.cast_cost_count=1;
        event.action_kind=item ? FD2_FIELD_MAGIC_ACTION_ITEM:FD2_FIELD_MAGIC_ACTION_MAGIC;
        event.item_code=item ? 23u:0u;
        event.target_index=1;event.target_before=&before;event.target_after=&game.units.items[1];
        event.destination_x=3;event.destination_y=2;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        if (skip) fd2_battle_animation_set_skip(&ctx,1);
        event.phase=FD2_FIELD_MAGIC_PRESENT_TELEPORT_OUT;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        CHECK(game.units.items[1].x==before.x && game.units.items[1].y==before.y);
        game.units.items[1].x=game.units.items[1].y=255;
        event.phase=FD2_FIELD_MAGIC_PRESENT_TELEPORT_OUT_COMMIT;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        event.phase=FD2_FIELD_MAGIC_PRESENT_TELEPORT_IN;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        CHECK(game.units.items[1].x==255 && game.units.items[1].y==255);
        game.units.items[1].x=3;game.units.items[1].y=2;
        event.phase=FD2_FIELD_MAGIC_PRESENT_TELEPORT_IN_COMMIT;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        CHECK(magic_finish(&ctx,&event,0)==0 && !ctx.magic_exchange);
        CHECK(game.units.items[1].x==3 && game.units.items[1].y==2 && !business_rng_calls);
        if (skip) CHECK(!frames && !ticks && !delays);
        else CHECK(frames>=102);
        fd2_battle_animation_close(&ctx);
    }
    return 0;
}

/* 独立20c6f caller向量，不能按Magic wrapper重新推导Item的完整演出。 */
static int test_item_map_presentations(void) {
    static const uint8_t effect_ids[]={13,20,21,17,18,19,13,19,13,27,18,17,13,13,19,4,0,22,23,11};
    uint8_t targets[]={1,2};fd2_pcm_player primary={0};
    for (uint8_t code=5;code<=24;code++) {
        if (code==23) continue;
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
        game.units.count=3;actor(&game.units.items[0],9,2,1);actor(&game.units.items[2],96,2,2);
        ctx.primary=&primary;expected_primary=&primary;
        uint8_t id=effect_ids[code-5];
        int permanent=(code>=8 && code<=10) || (code>=17 && code<=19);
        fd2_field_magic_present_event event=magic_event(id,targets,2);
        event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;event.action_kind=FD2_FIELD_MAGIC_ACTION_ITEM;
        event.item_code=code;event.item_power=permanent ? 0u:40u;
        event.effect_id=id;event.effect_target_count=permanent ? 1u:0u;
        mask_failure=1; /* Item直接helper没有21eb1/2189a wrapper，不能碰mask。 */
        fd2_field_units before=game.units;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && !mask_calls);
        event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && !mask_calls);
        /* 1cd17十张染色后还有一次旧snapshot投影，App页并不随之恢复。 */
        unsigned style_frames=code==21 ? 8u:11u;
        CHECK(frames==(unsigned)k_battle_map_fx_count[id]+1u+style_frames);
        if (code==5 || code==13) CHECK(cues==2); /* 12+1，无Magic13的前置11。 */
        if (code==24) CHECK(cues==0); /* core11直接LMI/ramp，不变成2189a。 */
        fd2_field_magic_basic_result result={0};result.hit=1;
        result.raw_damage=permanent ? 0u:7u;result.raw_heal=9;
        size_t count=permanent ? 1u:2u;
        for (size_t i=0;i<count;i++) {
            event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;event.target_index=targets[i];
            event.target_before=event.target_after=&game.units.items[targets[i]];event.result=&result;
            CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && !delay2_calls);
        }
        if (permanent) CHECK(ctx.magic_exchange->popup_glyphs[0][3]==94); /* 原red0，非MISS/green。 */
        event.phase=FD2_FIELD_MAGIC_PRESENT_POPUP_GROUP;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && delay2_calls==22);
        CHECK(ctx.magic_exchange->popup_done && ctx.magic_exchange->next==count);
        CHECK(magic_finish(&ctx,&event,0)==0 && delay2_calls==22 && primary_stops==1 && !ctx.magic_exchange);
        CHECK(!memcmp(&before,&game.units,sizeof(before)) && !business_rng_calls);
        mask_failure=0;fd2_battle_animation_close(&ctx);game.units.count=2;
    }
    return 0;
}

/* 真LMI/FDICON与App借页接线：PREPARE不得写App；1c2da最后显示旧页，
 * 实际App却保solid，不能以最后VGA覆盖工作页。Game绘制本体另有独立回归。 */
static int test_map_app_page_lifetime(void) {
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    fd2_field_page *page=fd2_field_page_create();CHECK(page);screen.field_page=page;
    uint8_t *pixels=malloc(FD2_FIELD_PAGE_SIZE),*before=malloc(FD2_FIELD_PAGE_SIZE);CHECK(pixels&&before);
    memset(pixels,17,FD2_FIELD_PAGE_SIZE);
    CHECK(fd2_field_page_write(page,0,0,456,336,pixels,FD2_FIELD_PAGE_SIZE,456)==0);
    uint8_t targets[]={1};fd2_field_magic_present_event event=magic_event(13,targets,1);
    event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;event.effect_id=13;
    event.action_kind=FD2_FIELD_MAGIC_ACTION_ITEM;event.item_code=5;event.cast_cost_count=0;
    fd2_field_units units=game.units;
    CHECK(fd2_field_page_read(page,0,0,456,336,before,FD2_FIELD_PAGE_SIZE,456)==0);
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    CHECK(fd2_field_page_read(page,0,0,456,336,pixels,FD2_FIELD_PAGE_SIZE,456)==0);
    CHECK(!memcmp(before,pixels,FD2_FIELD_PAGE_SIZE) && screen.field_page==page && !frames);
    event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    size_t sprite=ctx.magic_exchange->sprite_ids[0]*12u+(game.idle_phase==3?2u:game.idle_phase);
    uint8_t region[576];const uint8_t *raw=game.sprites.decoded_frames+sprite*576u;
    CHECK(fd2_field_page_read(page,72+((int)game.units.items[1].x-game.camera_cell_x)*24,
        66+((int)game.units.items[1].y-game.camera_cell_y)*24,24,24,region,sizeof(region),24)==0);
    unsigned solid=0;
    for(size_t i=0;i<576;i++) if(raw[i]) {CHECK(region[i]==200);solid++;}
    CHECK(solid && !memcmp(ctx.vga->framebuffer,ctx.magic_exchange->board.framebuffer,64000));
    CHECK(!memcmp(&units,&game.units,sizeof(units)) && !business_rng_calls);
    CHECK(fd2_field_page_read(page,0,0,1,1,region,sizeof(region),1)==0 && region[0]==17);
    CHECK(fd2_field_page_read(page,0,0,456,336,before,FD2_FIELD_PAGE_SIZE,456)==0);
    fd2_field_magic_basic_result result={0};result.hit=1;result.raw_heal=9;
    event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;event.target_index=1;
    event.target_before=event.target_after=&game.units.items[1];event.result=&result;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    event.phase=FD2_FIELD_MAGIC_PRESENT_POPUP_GROUP;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    CHECK(fd2_field_page_read(page,0,0,456,336,pixels,FD2_FIELD_PAGE_SIZE,456)==0);
    CHECK(memcmp(before,pixels,FD2_FIELD_PAGE_SIZE)!=0);
    CHECK(fd2_field_page_project(page,72,72,pixels,FD2_FIELD_PAGE_SIZE)==0);
    for(size_t y=4;y<196;y++) CHECK(!memcmp(pixels+y*320+4,screen.framebuffer+y*320+4,312));
    CHECK(!memcmp(&units,&game.units,sizeof(units)) && !business_rng_calls);
    CHECK(fd2_field_page_read(page,0,0,456,336,before,FD2_FIELD_PAGE_SIZE,456)==0);
    memcpy(pixels,screen.framebuffer,64000);unsigned clocks=visual_calls;
    CHECK(magic_finish(&ctx,&event,0)==0);
    CHECK(!ctx.magic_exchange && visual_calls==clocks && !memcmp(pixels,screen.framebuffer,64000));
    CHECK(fd2_field_page_read(page,0,0,456,336,pixels,FD2_FIELD_PAGE_SIZE,456)==0);
    CHECK(!memcmp(before,pixels,FD2_FIELD_PAGE_SIZE));
    fd2_battle_animation_close(&ctx);screen.field_page=NULL;fd2_field_page_destroy(page);
    free(pixels);free(before);return 0;
}

static int test_item_popup_and_effect_count_contract(void) {
    uint8_t targets[]={1,2};fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    game.units.count=3;actor(&game.units.items[0],9,2,1);actor(&game.units.items[2],96,0,2);
    fd2_field_magic_present_event event=magic_event(17,targets,2);
    event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;event.action_kind=FD2_FIELD_MAGIC_ACTION_ITEM;
    event.item_code=8;event.item_power=20;event.effect_id=17;event.effect_target_count=1;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    fd2_field_magic_basic_result result={0};result.hit=1;result.raw_damage=20;
    event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;event.target_index=1;event.result=&result;
    event.target_before=event.target_after=&game.units.items[1];
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    event.target_index=2;event.target_before=event.target_after=&game.units.items[2];
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==-1);
    CHECK(magic_finish(&ctx,&event,1)==0);fd2_battle_animation_close(&ctx);
    CHECK(setup(&ctx)==0);actor(&game.units.items[0],9,2,1);
    event=magic_event(13,targets,2);event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;
    event.action_kind=FD2_FIELD_MAGIC_ACTION_ITEM;event.item_code=11;event.effect_id=13;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    fd2_battle_animation_set_skip(&ctx,1);event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    memset(&result,0,sizeof(result)); /* MPmax0分支的MISS，不产生第二次RNG。 */
    for (size_t i=0;i<2;i++) {
        event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;event.target_index=targets[i];
        event.target_before=event.target_after=&game.units.items[targets[i]];event.result=&result;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    }
    CHECK(ctx.magic_exchange->popup_glyphs[0][0]==k_battle_popup_miss[0]);
    event.phase=FD2_FIELD_MAGIC_PRESENT_POPUP_GROUP;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && !frames);
    CHECK(magic_finish(&ctx,&event,0)==0 && !frames && !business_rng_calls);
    fd2_battle_animation_close(&ctx);
    CHECK(setup(&ctx)==0);event=magic_event(9,targets,2);
    event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;event.effect_id=9;
    event.effect_target_count=1;event.cast_cost_count=1;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    CHECK(ctx.magic_exchange->count==2 && ctx.magic_exchange->effect_count==1);
    event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    result.hit=1;result.raw_damage=10;event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;
    event.target_index=1;event.target_before=event.target_after=&game.units.items[1];event.result=&result;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    CHECK(magic_finish(&ctx,&event,0)==0 && !ctx.magic_exchange);
    fd2_battle_animation_close(&ctx);game.units.count=2;
    return 0;
}

static int test_new_magic_failure_quit_and_cache(void) {
    uint8_t targets[]={1};fd2_pcm_player primary={0};
    for (unsigned kind=0;kind<2;kind++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
        actor(&game.units.items[0],52,2,1);ctx.primary=&primary;expected_primary=&primary;
        fd2_field_magic_present_event event=magic_event((uint8_t)(kind ? 34:26),targets,1);
        event.presentation_mode=kind ? FD2_FIELD_MAGIC_MODE_COMPOSITE:FD2_FIELD_MAGIC_MODE_MAP;
        event.effect_id=event.magic_id;event.cast_cost_count=1;event.group_pass=kind ? UINT8_MAX:0;
        fd2_field_unit caster=game.units.items[0],target=game.units.items[1];
        fd2_vga *before=malloc(sizeof(screen));CHECK(before);*before=screen;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        cue_failure=1;event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==-1 && ctx.failed);
        CHECK(!memcmp(&caster,&game.units.items[0],sizeof(caster)) &&
              !memcmp(&target,&game.units.items[1],sizeof(target)));
        cue_failure=0;CHECK(magic_finish(&ctx,&event,1)==0 && !ctx.magic_exchange);
        CHECK(!memcmp(screen.framebuffer,before->framebuffer,64000) && !memcmp(screen.dac,before->dac,768));
        fd2_battle_animation_close(&ctx);free(before);
    }
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);actor(&game.units.items[0],52,2,1);
    fd2_field_magic_present_event event=magic_event(34,targets,1);
    event.presentation_mode=FD2_FIELD_MAGIC_MODE_COMPOSITE;event.effect_id=34;
    event.cast_cost_count=1;event.group_pass=UINT8_MAX;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    quit_frame=3;event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0 && ctx.host_quit && frames==3);
    event.group_pass=0;event.effect_id=17;event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    fd2_field_magic_basic_result result={0};result.hit=1;result.raw_heal=4;
    event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;event.target_index=1;event.result=&result;
    event.target_before=event.target_after=&game.units.items[1];
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    CHECK(magic_finish(&ctx,&event,0)==0 && frames==3 && !ctx.magic_exchange);
    fd2_battle_animation_close(&ctx);

    CHECK(setup(&ctx)==0);actor(&game.units.items[0],9,2,1);
    uint8_t *original=game.sprites.decoded_frames;
    size_t bytes=game.sprites.frame_count*576u;
    uint8_t *shadow=malloc(bytes);CHECK(shadow);memcpy(shadow,original,bytes);
    memset(shadow+(96u*12u+2u)*576u,0,576);shadow[(96u*12u+2u)*576u+2]=1;
    memset(shadow+(126u*12u+2u)*576u,0,576);shadow[(126u*12u+2u)*576u+1]=1;
    game.sprites.decoded_frames=shadow;game.units.items[1].unit_id=126;
    game.units.sprite_cache_exact=1;game.units.sprite_cache_count=1;
    game.units.sprite_cache_classes[1]=0;game.units.sprite_cache_ids[0]=96;
    event=magic_event(13,targets,1);event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;
    event.effect_id=13;event.cast_cost_count=1;
    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
    CHECK(ctx.magic_exchange->sprite_ids[0]==96 && game.units.items[1].unit_id==126);
    game.idle_phase=2;memset(ctx.magic_exchange->work,7,64000);
    map_actor_paint(ctx.magic_exchange,&game,ctx.magic_exchange->work,0,192);
    size_t origin=(size_t)(4+24-6)*320u+4u+2u*24u;
    CHECK(ctx.magic_exchange->work[origin+2]==192 && ctx.magic_exchange->work[origin+1]==7);
    CHECK(magic_finish(&ctx,&event,1)==0);fd2_battle_animation_close(&ctx);
    game.units.sprite_cache_exact=0;game.units.sprite_cache_count=0;
    game.sprites.decoded_frames=original;free(shadow);
    CHECK(!business_rng_calls);return 0;
}

static fd2_field_physical_event map_physical_event(fd2_field_physical_present_mode mode,
        fd2_field_physical_phase phase) {
    fd2_field_physical_event event=make_event(phase);event.presentation_mode=mode;
    if (mode==FD2_FIELD_PHYSICAL_MODE_RECOVERY) event.target_index=event.actor_index;
    return event;
}

static int map_physical_finish(fd2_battle_animation *ctx,fd2_field_physical_present_mode mode,int failed) {
    fd2_field_physical_event event=map_physical_event(mode,FD2_FIELD_PHYSICAL_FINISH);
    event.action_failed=failed;return fd2_battle_animation_physical_present(&game,&event,ctx);
}

/* 独立逐像素投影，不调用生产blit/actor/bar helper；输入是真实资源解码结果。 */
static void expected_image(uint8_t dst[64000],const battle_image *image,int x,int y) {
    for (int row=0;row<image->image.height;row++) for (int col=0;col<image->image.width;col++) {
        int px=x+col,py=y+row;size_t at=(size_t)row*(size_t)image->image.width+(size_t)col;
        if (px>=0 && px<320 && py>=0 && py<200 && image->opaque[at])
            dst[py*320+px]=image->image.pixels[at];
    }
}

static void expected_actor(uint8_t dst[64000],const fd2_field_unit *unit,size_t sprite,
        unsigned phase,int solid,int viewport) {
    if (phase==3) phase=1;
    int dx=((int)unit->x-game.camera_cell_x)*24+4;
    int dy=((int)unit->y-game.camera_cell_y)*24-2;
    if (unit->x<game.camera_cell_x-1 || unit->x>game.camera_cell_x+13 ||
        unit->y<game.camera_cell_y-1 || unit->y>game.camera_cell_y+9) return;
    int movement=(int)unit->frame_phase*4;
    if (unit->direction==0) dy+=movement;
    else if (unit->direction==1) dx-=movement;
    else if (unit->direction==2) dy-=movement;
    else dx+=movement;
    const uint8_t *source=game.sprites.decoded_frames+(sprite*12u+unit->direction*3u+phase)*576u;
    for (int row=0;row<24;row++) for (int col=0;col<24;col++) {
        int x=dx+col,y=dy+row;uint8_t value=source[row*24+col];
        if (x>=(viewport ? 4:0) && x<(viewport ? 316:320) &&
            y>=(viewport ? 4:0) && y<(viewport ? 196:200) && value)
            dst[y*320+x]=solid>=0 ? (uint8_t)solid:value;
    }
}

static void expected_bar(uint8_t dst[64000],int x,int y,int width) {
    int last=width<=0 ? 69:width<70 ? 70:width;
    for (int col=0;col<=last && x+col<320;col++) {
        size_t frame;
        if (width<=0) frame=col==69 ? 7u:6u;
        else if (!col) frame=0;
        else if (col<width) frame=2;
        else if (col==width) frame=3;
        else frame=col==70 ? 7u:6u;
        for (int row=0;row<5;row++) if (x+col>=0 && y+row>=0 && y+row<200)
            dst[(y+row)*320+x+col]=game.info_assets.bars[frame].pixels[row];
    }
}

static int test_map_physical_preflight_and_actor_projection(void) {
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    fd2_field_game *before=malloc(sizeof(game));fd2_vga *old=malloc(sizeof(screen));
    uint8_t *expected=malloc(64000);CHECK(before && old && expected);
    for (int which=0;which<2;which++) {
        fd2_field_physical_present_mode mode=which ? FD2_FIELD_PHYSICAL_MODE_RECOVERY:FD2_FIELD_PHYSICAL_MODE_SHORT;
        fd2_field_physical_event e=map_physical_event(mode,FD2_FIELD_PHYSICAL_PREPARE);
        e.resource_only=1;*before=game;*old=screen;
        CHECK(fd2_battle_animation_physical_present(&game,&e,&ctx)==0 && !ctx.exchange);
        CHECK(!memcmp(before,&game,sizeof(game)) && !memcmp(old,&screen,sizeof(screen)) && !frames && !cues);
        CHECK(!ctx.backgrounds.data && !ctx.terrain_images.data);
        fd2_archive limited=fdother;limited.count=which ? 31:64;ctx.fdother=&limited;
        game.option_values[1]=0;*before=game;*old=screen;
        CHECK(fd2_battle_animation_physical_present(&game,&e,&ctx)==-1 && !ctx.exchange);
        CHECK(!memcmp(before,&game,sizeof(game)) && !memcmp(old,&screen,sizeof(screen)));
        ctx.fdother=&fdother;game.option_values[1]=1;ctx.failed=0;
    }
    fd2_field_physical_event event=map_physical_event(FD2_FIELD_PHYSICAL_MODE_RECOVERY,FD2_FIELD_PHYSICAL_PREPARE);
    /* raw identity写后缓存仍为0；专用1da16忽略acted/raw26，并测试phase3→1。 */
    game.units.items[0].unit_id=126;game.units.sprite_cache_exact=1;
    game.units.sprite_cache_count=1;game.units.sprite_cache_classes[0]=0;game.units.sprite_cache_ids[0]=0;
    game.units.items[0].detail_status[1]=3;game.units.items[0].flags=0x80;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    CHECK(ctx.exchange->sprite_ids[0]==0);
    game.camera_cell_x=2;game.camera_cell_y=2;
    for (int pos=0;pos<4;pos++) for (uint8_t dir=0;dir<4;dir++) for (int walking=0;walking<2;walking++) {
        fd2_field_unit u=game.units.items[0];u.direction=dir;u.frame_phase=(uint8_t)(walking ? 2:0);
        u.x=(uint8_t)(pos==0 ? 1:pos==1 ? 15:6);u.y=(uint8_t)(pos==2 ? 1:pos==3 ? 11:5);
        ctx.exchange->units[0]=u;game.idle_phase=3;game.units.walk_frames[0]=3;
        for (int viewport=0;viewport<2;viewport++) for (int solid=0;solid<2;solid++) {
            memset(screen.framebuffer,109,64000);memset(expected,109,64000);
            CHECK(map_physical_actor(&game,ctx.exchange,screen.framebuffer,0,solid,viewport,NULL,3)==0);
            expected_actor(expected,&u,0,3,solid ? viewport ? 200:64:-1,viewport);
            CHECK(!memcmp(screen.framebuffer,expected,64000));
        }
    }
    game.camera_cell_x=game.camera_cell_y=0;game.idle_phase=0;game.units.walk_frames[0]=1;
    CHECK(map_physical_finish(&ctx,FD2_FIELD_PHYSICAL_MODE_RECOVERY,1)==0 && !ctx.exchange);
    fd2_battle_animation_close(&ctx);game.units.sprite_cache_exact=0;game.units.sprite_cache_count=0;
    free(before);free(old);free(expected);return 0;
}

static int test_short_panels_counter_and_layout(void) {
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    game.units.items[0].x=5;game.units.items[1].x=6;
    game.units.items[0].y=game.units.items[1].y=4;
    game.units.items[0].direction=3;game.units.items[1].direction=0;
    fd2_field_physical_event event=map_physical_event(FD2_FIELD_PHYSICAL_MODE_SHORT,FD2_FIELD_PHYSICAL_PREPARE);
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0 && ctx.exchange->counter_ready);
    event.phase=FD2_FIELD_PHYSICAL_SHORT_MAP;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    game.units.items[1].direction=1;event.phase=FD2_FIELD_PHYSICAL_SHORT_BEGIN;
    capture_map=1;map_captured_count=0;
    unsigned begin_frames=frames,begin_delays=delays;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0 && map_captured_count==10);
    CHECK(frames==begin_frames+10u && delays==begin_delays+250u);
    CHECK(ctx.exchange->panel_x[1]==176 && ctx.exchange->panel_y[1]==78);
    CHECK(ctx.exchange->panel_x[0]==36 && ctx.exchange->panel_y[0]==118);
    uint8_t *expected=malloc(64000);CHECK(expected);
    for (size_t i=0;i<10;i++) {
        memset(expected,62,64000);
        expected_image(expected,&ctx.exchange->map_fx[39+i],176,78);
        expected_image(expected,&ctx.exchange->map_fx[39+i],36,118);
        CHECK(!memcmp(expected,map_captured+i*64000u,64000));
    }
    expected_bar(expected,183,84,70);expected_bar(expected,43,124,70);
    /* 原1e7f6只改CPU/VGA页，不能多出第11个零时长present。
     * 两条HP已完整写入，随后首STRIKE会与实体/首FX一起上传。 */
    CHECK(!memcmp(expected,screen.framebuffer,64000));
    CHECK(memcmp(screen.framebuffer,map_captured+9u*64000u,64000)!=0);
    game.units.items[1].hp=21;event.phase=FD2_FIELD_PHYSICAL_COUNTER_REFRESH;
    effect_info_marker=1;fake_ms+=1000; /* 1e611仅11eee门，不借1297d或信息板。 */
    fd2_field_game *counter_before=malloc(sizeof(game));CHECK(counter_before);*counter_before=game;
    unsigned clock_calls=visual_calls,terrain_clock_calls=terrain_calls;
    unsigned backgrounds=background_return_calls,effects=effect_return_calls;
    fd2_vga *vga_before=malloc(sizeof(screen));CHECK(vga_before);*vga_before=screen;
    uint16_t water_tick=screen.field_water_last_tick;uint8_t water_phase=screen.field_water_phase;
    uint16_t jitter_tick=(uint16_t)(fake_ms*1193182u/UINT64_C(65536000));
    uint8_t jitter_phase=(uint8_t)(screen.actor_jitter_phase^(screen.actor_jitter_tick!=jitter_tick));
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0 && map_captured_count==11);
    CHECK(game.terrain_visual_phase==(uint8_t)(counter_before->terrain_visual_phase^1u));
    CHECK(game.last_terrain_visual_ms==fake_ms && terrain_calls==terrain_clock_calls+1u);
    /* 精确允许11eee两个字段；idle/walk锚/单位/业务/RNG及其它Game字节全保留。 */
    counter_before->terrain_visual_phase=game.terrain_visual_phase;
    counter_before->last_terrain_visual_ms=game.last_terrain_visual_ms;
    CHECK(!memcmp(counter_before,&game,sizeof(game)) && visual_calls==clock_calls);
    CHECK(background_return_calls==backgrounds+1u && effect_return_calls==effects);
    CHECK(screen.field_water_last_tick==water_tick && screen.field_water_phase==water_phase);
    CHECK(!memcmp(screen.palette,vga_before->palette,sizeof(screen.palette)) &&
        !memcmp(screen.dac,vga_before->dac,sizeof(screen.dac)));
    CHECK(screen.actor_jitter_tick==jitter_tick && screen.actor_jitter_phase==jitter_phase);
    free(counter_before);free(vga_before);
    memset(expected,41,64000);
    expected_image(expected,&ctx.exchange->map_fx[48],176,78);
    expected_bar(expected,183,84,55);expected_bar(expected,183,84,35); /* target第三参：底条后覆HP。 */
    expected_image(expected,&ctx.exchange->map_fx[48],36,118);
    expected_bar(expected,43,124,70); /* actor第二参，42/42。 */
    CHECK(!memcmp(expected,map_captured+10u*64000u,64000));
    CHECK(map_physical_finish(&ctx,FD2_FIELD_PHYSICAL_MODE_SHORT,1)==0);
    fd2_battle_animation_close(&ctx);free(expected);
    /* 1ec2a的上下/左右边界逐分支，不靠普通菜单布局替代。 */
    const uint8_t position[][3]={{0,0,0},{12,0,1},{0,7,2},{12,7,3}};
    const int wanted[][2]={{32,5},{206,5},{32,173},{204,173}};
    for (size_t i=0;i<4;i++) {
        fd2_field_unit u={0};u.x=position[i][0];u.y=position[i][1];u.direction=position[i][2];
        int x,y;short_panel_position(&game,&u,&x,&y);CHECK(x==wanted[i][0] && y==wanted[i][1]);
    }
    return 0;
}

static int check_runtime_page(const fd2_field_page *page,unsigned phase,int failure,int touched) {
    uint8_t *pixels=malloc(FD2_FIELD_PAGE_SIZE);CHECK(pixels);
    CHECK(fd2_field_page_read(page,0,0,456,336,pixels,FD2_FIELD_PAGE_SIZE,456)==0);
    int same=1;
    for(int y=0;y<336 && same;y++)for(int x=0;x<456;x++) {
        uint8_t expected=17;
        if(touched && failure && x==72 && y==72)expected=239;
        else if(touched && !failure && x>=72 && x<384 && y>=72 && y<264)
            expected=runtime_pixel(phase,x-72,y-72);
        if(pixels[(size_t)y*456u+(size_t)x]!=expected){same=0;break;}
    }
    free(pixels);CHECK(same);return 0;
}

/* 完整2d80d的2dd2b..2ddb1：清VGA、11cac(1)、41次RGB渐变/6ms，
 * 此后才跑MAP组；2def4→1d4f6→1a80a只stop/free，不再写App/VGA/DAC。
 * RGB6直接来自原strict image5265c/52660/52664；本例只签caller组合合同，
 * runtime_pixel是明确的Game叶替身，不把它当真实地形/单位像素oracle。 */
static const uint8_t composite_source_rgb[4][3]={
    {63,63,63},{51,57,63},{53,0,0},{53,58,9}
};
static void composite_observe_delay(uint32_t value) {
    if(!composite_capture || value!=6u)return;
    unsigned factor=composite_delay6++;
    if(factor>40u || runtime_calls!=1u || runtime_phase!=3u || effect_return_calls)
        composite_bad_delay++;
    for(size_t i=0;i<768u;i++) {
        int color=composite_source_rgb[composite_slot][i%3u];
        int level=i<765u?color+(int)factor*((int)composite_base_palette[i]-color)/40:
            composite_base_palette[i];
        uint8_t expected=(uint8_t)((level<<2)|(level>>4));
        if(screen.dac[i]!=expected)composite_bad_delay++;
    }
    for(int y=0;y<200;y++)for(int x=0;x<320;x++) {
        uint8_t expected=x>=4 && x<316 && y>=4 && y<196?runtime_pixel(3,x-4,y-4):0;
        if(screen.framebuffer[(size_t)y*320u+(size_t)x]!=expected)composite_bad_delay++;
    }
    if(composite_quit_at_six && factor==0u)screen.input.quit_requested=1;
}

static int test_composite_map_return_boundary(void) {
    /* 原2ddd8/2de31/2de4b/2de69/2de87/2dec8/2deea组序；
     * 不从生产cold图反造组参数。全组业务/随机另由Game回归覆盖。 */
    static const uint8_t effects[4][3]={{32,0,0},{13,0,0},{17,18,19},{26,22,27}};
    uint8_t targets[]={1};
    for(unsigned sample=0;sample<9u;sample++) {
        unsigned outcome=sample<4u?0u:sample-3u;
        uint8_t id=(uint8_t)(sample<4u?32u+sample:34u);
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);game.units.count=2;
        actor(&game.units.items[0],52,2,1);game.ordinary_display_mode=0;
        game.units.items[0].mp=game.units.items[0].mp_max=200;
        fd2_field_page *page=fd2_field_page_create();CHECK(page);screen.field_page=page;
        fd2_field_visual_state *owner=game.visual_owner;game.visual_owner=&screen.field_visual;
        uint8_t *expected=malloc(FD2_FIELD_PAGE_SIZE),*actual=malloc(FD2_FIELD_PAGE_SIZE);
        uint8_t *vga_before=malloc(64000);fd2_field_game *before=malloc(sizeof(game));
        CHECK(expected && actual && vga_before && before);
        memset(expected,17,FD2_FIELD_PAGE_SIZE);
        CHECK(fd2_field_page_write(page,0,0,456,336,expected,FD2_FIELD_PAGE_SIZE,456)==0);
        const uint8_t *palette;size_t palette_size;
        CHECK(fd2_archive_get(&fdother,0,&palette,&palette_size)==0 && palette_size>=768u);
        memcpy(screen.palette,palette,768);memcpy(composite_base_palette,palette,768);
        memset(screen.dac,47,768);memcpy(vga_before,screen.framebuffer,64000);
        fd2_pcm_player primary={0};ctx.primary=&primary;expected_primary=&primary;
        fd2_figani_magic_cast plan;CHECK(fd2_figani_magic_action_plan(&plan,ctx.figani,52,id)==0);
        fd2_field_magic_present_event event=magic_event(id,targets,1);
        event.presentation_mode=FD2_FIELD_MAGIC_MODE_COMPOSITE;event.cast_cost_count=plan.cost_marker_count;
        event.effect_id=id;event.group_pass=UINT8_MAX;
        CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
        composite_owner=&ctx;
        *before=game;
        uint64_t old_deadline=screen.frame_deadline_ns,old_interval=screen.frame_interval_ns;
        screen.frame_deadline_ns=UINT64_C(123456789);screen.frame_interval_ns=UINT64_C(16666667);
        uint16_t water_tick=screen.field_water_last_tick;
        uint8_t water_phase=screen.field_water_phase;
        composite_capture=1;composite_slot=id-32u;
        composite_quit_at_six=outcome==4u;
        composite_quit_after_return=outcome==3u || outcome==5u;
        runtime_failure=outcome==2u || outcome==5u;
        if(outcome==1u)fd2_battle_animation_set_skip(&ctx,1);
        event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
        int status=fd2_battle_animation_magic_present(&game,&event,&ctx);
        CHECK(status==((outcome==2u || outcome==5u)?-1:0));
        CHECK(ctx.failed==(outcome==2u || outcome==5u));
        CHECK(!composite_bad_delay && !effect_return_calls && !visual_calls && !terrain_calls &&
            !business_rng_calls && !memcmp(before,&game,sizeof(game)));
        CHECK(screen.field_water_last_tick==water_tick && screen.field_water_phase==water_phase &&
            screen.frame_deadline_ns==UINT64_C(123456789) &&
            screen.frame_interval_ns==UINT64_C(16666667));
        CHECK(runtime_calls==(outcome==1u?0u:1u) && composite_delay6==
            (outcome==0u?41u:outcome==4u?1u:0u));
        if(outcome!=1u) {
            CHECK(check_runtime_page(page,3,runtime_failure,1)==0);
            for(int y=0;y<200;y++)for(int x=0;x<320;x++) {
                uint8_t pixel=!runtime_failure && x>=4 && x<316 && y>=4 && y<196?
                    runtime_pixel(3,x-4,y-4):0;
                CHECK(screen.framebuffer[(size_t)y*320u+(size_t)x]==pixel);
            }
        }
        if(outcome==0u) {
            CHECK(primary_stops==1u && composite_stop_count==1u && composite_stop_bank_flags[0]==3u &&
                !ctx.magic_exchange->sound.archive.data &&
                ctx.magic_exchange->map_sound.archive.data && !ctx.host_quit);
            /* 使用真实FIGANI/地图FX/浮字后，将这些输出作为FINISH的输入；
             * 独立expected是原尾无像素/调色板writer的恒等合同。 */
            composite_capture=0;
            for(unsigned group=0;group<(id<34u?1u:3u);group++) {
                event.group_pass=(uint8_t)group;event.effect_id=effects[id-32u][group];
                if(group) {
                    event.phase=FD2_FIELD_MAGIC_PRESENT_POPUP_CLEAR;
                    CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
                }
                event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
                CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
                fd2_field_magic_basic_result result={0};result.hit=1;result.raw_damage=7;result.raw_heal=9;
                event.phase=FD2_FIELD_MAGIC_PRESENT_TARGET;event.target_index=1;
                event.target_before=event.target_after=&game.units.items[1];event.result=&result;
                CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
            }
            CHECK(fd2_field_page_read(page,0,0,456,336,expected,FD2_FIELD_PAGE_SIZE,456)==0);
            memcpy(vga_before,screen.framebuffer,64000);
            uint8_t last_palette[768],last_dac[768];
            memcpy(last_palette,screen.palette,768);memcpy(last_dac,screen.dac,768);
            unsigned old_frames=frames,old_ticks=ticks,old_delays=delays,old_pumps=input_pumps;
            unsigned old_runtime=runtime_calls,old_visual=visual_calls,old_terrain=terrain_calls;
            CHECK(magic_finish(&ctx,&event,0)==0 && !ctx.magic_exchange && primary_stops==2u);
            CHECK(composite_stop_count==2u && composite_stop_bank_flags[1]==2u);
            CHECK(fd2_field_page_read(page,0,0,456,336,actual,FD2_FIELD_PAGE_SIZE,456)==0 &&
                !memcmp(actual,expected,FD2_FIELD_PAGE_SIZE) && !memcmp(screen.framebuffer,vga_before,64000) &&
                !memcmp(screen.palette,last_palette,768) && !memcmp(screen.dac,last_dac,768));
            CHECK(frames==old_frames && ticks==old_ticks && delays==old_delays && input_pumps==old_pumps &&
                runtime_calls==old_runtime && visual_calls==old_visual && terrain_calls==old_terrain);
        } else {
            /* 合法fast单独归还地图；错误/HOST延续已有清理合同，不把它们
             * 当原正常尾写集。真实error与同轮HOST并存时保负返回及failed。 */
            composite_capture=0;unsigned old_runtime=runtime_calls,old_frames=frames;
            CHECK(magic_finish(&ctx,&event,(outcome==2u || outcome==5u)?1:0)==0 && !ctx.magic_exchange);
            CHECK(runtime_calls==old_runtime+(outcome==1u?1u:0u) && frames==old_frames);
            if(outcome==1u)CHECK(check_runtime_page(page,3,0,1)==0);
            else {
                CHECK(!memcmp(screen.framebuffer,vga_before,64000));
                CHECK(check_runtime_page(page,3,runtime_failure,1)==0);
            }
            for(size_t i=0;i<768u;i++)CHECK(screen.dac[i]==47);
            CHECK(!memcmp(screen.palette,palette,768));
        }
        CHECK(!wrong_stop && !business_rng_calls && unchanged_except_visual(&game,before));
        fd2_battle_animation_close(&ctx);composite_owner=NULL;game.visual_owner=owner;screen.field_page=NULL;
        screen.frame_deadline_ns=old_deadline;screen.frame_interval_ns=old_interval;
        fd2_field_page_destroy(page);free(expected);free(actual);free(vga_before);free(before);
    }
    return 0;
}

/* 原2e2b0在54133==0的普通FULL尾（split0/1共用）、2cf30和2ff01
 * 都在fade-out后清VGA64000B，再11cac(1)。31529 montage另有scope，
 * 不调用这个正常return；此处用真实资源PREPARE出的已started尾状态隔离该叶。
 * 这是源派生尾合同/错误前缀回归，不把合成started称完整自然攻击轨迹。 */
static int test_full_return_clear_boundary(void) {
    for(unsigned kind=0;kind<4u;kind++)for(unsigned outcome=0;outcome<5u;outcome++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);game.units.count=2;
        uint8_t id=kind==3u?28u:0u;
        actor(&game.units.items[0],kind==1u?8u:kind==3u?19u:0u,2,1);
        game.units.items[0].mp=game.units.items[0].mp_max=500;
        game.ordinary_display_mode=0;
        fd2_field_page *page=fd2_field_page_create();CHECK(page);screen.field_page=page;
        fd2_field_visual_state *owner=game.visual_owner;game.visual_owner=&screen.field_visual;
        uint8_t *seed=malloc(FD2_FIELD_PAGE_SIZE),*old_vga=malloc(64000);
        fd2_field_game *before=malloc(sizeof(game));CHECK(seed && old_vga && before);
        memset(seed,17,FD2_FIELD_PAGE_SIZE);
        CHECK(fd2_field_page_write(page,0,0,456,336,seed,FD2_FIELD_PAGE_SIZE,456)==0);
        /* 原4e31c只改DAC224..239；base仍是同份只读FDOTHER0。
         * phase5的RGB6来自raw原palette+1，不调用被测water/fade生成expected。 */
        const uint8_t *palette;size_t palette_size;
        CHECK(fd2_archive_get(&fdother,0,&palette,&palette_size)==0 && palette_size>=768u);
        uint8_t base_dac[768],old_dac[768];
        memcpy(screen.palette,palette,768);
        for(size_t i=0;i<768u;i++)base_dac[i]=(uint8_t)((palette[i]<<2)|(palette[i]>>4));
        memcpy(old_dac,base_dac,768);
        for(unsigned i=0;i<16u;i++)for(unsigned channel=0;channel<3u;channel++) {
            uint8_t value=(uint8_t)(palette[(224u+((5u+i)&15u))*3u+channel]+1u);
            old_dac[(224u+i)*3u+channel]=(uint8_t)((value<<2)|(value>>4));
        }
        CHECK(memcmp(base_dac,old_dac,768)!=0);memcpy(screen.dac,old_dac,768);
        uint8_t targets[]={1};fd2_field_magic_present_event event=magic_event(id,targets,1);
        if(kind<2u) {
            fd2_field_physical_event physical=make_event(FD2_FIELD_PHYSICAL_PREPARE);
            CHECK(fd2_battle_animation_physical_present(&game,&physical,&ctx)==0);
            CHECK(ctx.exchange->attack[0].clip.split_mode==(kind==1u));
            ctx.exchange->started=1;
        } else {
            fd2_figani_magic_cast plan;
            CHECK(fd2_figani_magic_action_plan(&plan,ctx.figani,game.units.items[0].unit_id,id)==0);
            event.cast_cost_count=plan.cost_marker_count;
            if(kind==3u)event.presentation_mode=FD2_FIELD_MAGIC_MODE_DERIVED;
            CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
            ctx.magic_exchange->started=1;
        }
        *before=game;memcpy(old_vga,screen.framebuffer,64000);
        /* 不假定原上一个图像边框为0：源clear应覆盖任意有限合法before。 */
        memset(screen.framebuffer,179,64000);
        if(outcome==4u)fd2_battle_animation_set_skip(&ctx,1);
        return_clear_watch=outcome!=4u;return_host_before_refresh=outcome==1u;
        return_host_after_refresh=outcome==3u;runtime_failure=outcome==2u;
        int status=kind<2u?finish(&ctx,0):magic_finish(&ctx,&event,0);
        CHECK(status==(outcome==2u?-1:0) && !ctx.exchange && !ctx.magic_exchange);
        CHECK(!return_bad_clear && runtime_calls==(outcome==1u?0u:1u) &&
            return_fade_delay2==(outcome==0u?129u:outcome==1u?63u:outcome==4u?0u:64u));
        CHECK(check_runtime_page(page,3,outcome==2u,outcome!=1u)==0);
        if(outcome==0u || outcome==4u) {
            for(int y=0;y<200;y++)for(int x=0;x<320;x++) {
                uint8_t expected=x>=4 && x<316 && y>=4 && y<196?runtime_pixel(3,x-4,y-4):(outcome==4u?179u:0u);
                CHECK(screen.framebuffer[(size_t)y*320u+(size_t)x]==expected);
            }
        } else {
            CHECK(!memcmp(screen.framebuffer,old_vga,64000));
            if(outcome==2u)CHECK(ctx.failed && frames==runtime_frames);
            else CHECK(ctx.host_quit && !ctx.failed);
        }
        CHECK(!memcmp(screen.palette,palette,768) &&
            !memcmp(screen.dac,outcome==0u?base_dac:old_dac,768));
        CHECK(!visual_calls && !terrain_calls && !return_calls && !business_rng_calls &&
            !memcmp(before,&game,sizeof(game)));
        return_clear_watch=0;
        fd2_battle_animation_close(&ctx);game.visual_owner=owner;screen.field_page=NULL;
        fd2_field_page_destroy(page);free(seed);free(old_vga);free(before);
    }
    return 0;
}

/* 独立原1685c RAW 1×5列，不从生产assets/renderer回读expected。 */
static int short_source_columns(uint8_t raw[8][5]) {
    const uint8_t *data;size_t size;
    CHECK(fd2_archive_get(&fdother,5,&data,&size)==0 && size>=6 && !memcmp(data,"LMI1",4));
    unsigned count=(unsigned)data[4]|((unsigned)data[5]<<8);
    static const unsigned frame[8]={23,0,24,25,0,0,29,30};
    CHECK(count>30 && 6u+count*4u<=size);
    memset(raw,0,8u*5u);
    for(unsigned i=0;i<8;i++) {
        if(i==1 || i==4 || i==5)continue;
        size_t at=battle_u32(data+6u+frame[i]*4u),end=battle_u32(data+10u+frame[i]*4u);
        CHECK(at>=6u+count*4u && end==at+9u && end<=size &&
            data[at]==1 && data[at+1]==0 && data[at+2]==5 && data[at+3]==0);
        memcpy(raw[i],data+at+4u,5);
    }
    return 0;
}
/* 1e739闭式列分类，独立于产品分支/指针，按col后row逐点保首错前缀。 */
static int short_source_bar(uint8_t *dst,uint8_t raw[8][5],int x,int y,int width) {
    int last=width<=0?69:width<70?70:width;
    for(int col=0;col<=last;col++) {
        unsigned image=width<=0?(col==69?7u:6u):
            !col?0u:col<width?2u:col==width?3u:col==70?7u:6u;
        for(int row=0;row<5;row++) {
            int64_t at=(int64_t)(y+row)*320+x+col;
            if(at<0 || at>=64000)return -1;
            dst[(size_t)at]=raw[image][row];
        }
    }
    return 0;
}
static int test_short_vga_bar_source_owner(void) {
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);uint8_t raw[8][5];CHECK(short_source_columns(raw)==0);
    unsigned before_pumps=input_pumps;
    uint8_t *actual=malloc(64002),*expected=malloc(64002);CHECK(actual && expected);
    static const int widths[]={-1,0,1,35,70,104};
    for(size_t i=0;i<sizeof(widths)/sizeof(widths[0]);i++) {
        memset(actual,173,64002);memcpy(expected,actual,64002);
        CHECK(short_source_bar(expected+1,raw,231,60,widths[i])==0);
        CHECK(short_bar(&game,actual+1,231,60,widths[i])==0 && !memcmp(actual,expected,64002));
    }
    /* 固定五帧RAW没有0；这个独立合成列仅检1685c全字节copy语义，不冒原资源。 */
    uint8_t zero[5]={0},*saved=game.info_assets.bars[0].pixels;uint8_t saved_raw[5];
    memcpy(saved_raw,raw[0],5);memset(raw[0],0,5);game.info_assets.bars[0].pixels=zero;
    memset(actual,173,64002);memcpy(expected,actual,64002);
    CHECK(short_source_bar(expected+1,raw,231,60,1)==0);
    int zero_status=short_bar(&game,actual+1,231,60,1);
    game.info_assets.bars[0].pixels=saved;memcpy(raw[0],saved_raw,5);
    CHECK(zero_status==0 && !memcmp(actual,expected,64002));
    /* 第一个owner错误在首列第二行，已写首行必须保留；两侧哨兵都保。 */
    memset(actual,173,64002);memcpy(expected,actual,64002);
    CHECK(short_source_bar(expected+1,raw,318,199,35)==-1);
    CHECK(short_bar(&game,actual+1,318,199,35)==-1 && !memcmp(actual,expected,64002));
    CHECK(!frames && !ticks && !delays && input_pumps==before_pumps && !business_rng_calls);
    free(actual);free(expected);fd2_battle_animation_close(&ctx);return 0;
}
/* 真实STRIKE协议，只给已提交miss/core结果：FX后恢复已知VGA，然后105/104两条。
 * HP63/max42是合法typed参数，不声称自然角色已到此stats。owner坏点同理。 */
static int test_short_vga_bar_adapter(void) {
    for(unsigned outcome=0;outcome<3;outcome++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);uint8_t raw[8][5];CHECK(short_source_columns(raw)==0);
        fd2_field_map saved_map=game.map;uint32_t cells[13*8]={0};
        game.map.width=13;game.map.height=8;game.map.cells=cells;game.units.count=2;
        game.units.items[0].x=7;game.units.items[1].x=8;
        game.units.items[0].y=game.units.items[1].y=3;
        game.units.items[0].direction=3;game.units.items[1].direction=0;
        game.units.items[1].hp=63;
        fd2_field_physical_event event=map_physical_event(FD2_FIELD_PHYSICAL_MODE_SHORT,FD2_FIELD_PHYSICAL_PREPARE);
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        event.phase=FD2_FIELD_PHYSICAL_SHORT_MAP;CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        event.phase=FD2_FIELD_PHYSICAL_SHORT_BEGIN;CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        CHECK(ctx.exchange->panel_x[1]==224 && ctx.exchange->panel_y[1]==54 && ctx.exchange->fx_offsets[0]==0);
        uint8_t *expected=malloc(64000);fd2_field_game *gb=malloc(sizeof(game));fd2_vga *vb=malloc(sizeof(screen));
        CHECK(expected && gb && vb);for(size_t i=0;i<64000;i++)expected[i]=(uint8_t)(i*17u+i/320u+83u);
        memcpy(screen.framebuffer,expected,64000);*gb=game;*vb=screen;
        fd2_field_unit actor_before=game.units.items[0],target_before=game.units.items[1];
        fd2_field_attack_result result={0};result.hp_after=63;event.phase=FD2_FIELD_PHYSICAL_STRIKE;
        event.actor_before=&actor_before;event.target_before=&target_before;event.result=&result;
        unsigned old_frames=frames,old_delays=delays,old_pumps=input_pumps;
        if(outcome==1)screen.input.quit_requested=1;
        if(outcome==2){ctx.exchange->panel_x[1]=311;ctx.exchange->panel_y[1]=193;}
        capture_map=1;map_captured_count=0;
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==(outcome==2?-1:0));
        capture_map=0;
        if(!outcome) {
            CHECK(short_source_bar(expected,raw,231,60,105)==0);
            CHECK(map_captured_count==9 && !memcmp(expected,map_captured+6u*64000u,64000));
            CHECK(short_source_bar(expected,raw,231,60,104)==0 &&
                !memcmp(expected,map_captured+7u*64000u,64000) && !memcmp(expected,map_captured+8u*64000u,64000));
            CHECK(frames==old_frames+9 && delays==old_delays+496 && input_pumps==old_pumps+9);
        } else if(outcome==1) {
            CHECK(ctx.host_quit && !ctx.failed && frames==old_frames && delays==old_delays && input_pumps==old_pumps);
        } else {
            CHECK(short_source_bar(expected,raw,318,199,105)==-1);
            CHECK(ctx.failed && !ctx.host_quit && frames==old_frames+6 && delays==old_delays+480 && input_pumps==old_pumps+6);
        }
        CHECK(!memcmp(expected,screen.framebuffer,64000) && !memcmp(gb,&game,sizeof(game)) && !business_rng_calls);
        CHECK(!memcmp(screen.palette,vb->palette,768) && !memcmp(screen.dac,vb->dac,768));
        CHECK(map_physical_finish(&ctx,FD2_FIELD_PHYSICAL_MODE_SHORT,outcome!=0)==0);
        fd2_battle_animation_close(&ctx);game.map=saved_map;free(expected);free(gb);free(vb);
    }
    return 0;
}

/* 1e739的地址是线性owner，不是逻辑行裁剪；超owner保第一有效列前缀。 */
static int test_short_counter_raw_bar_owner(void) {
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
    uint8_t *actual=malloc(FD2_FIELD_PAGE_SIZE+2u),*expected=malloc(FD2_FIELD_PAGE_SIZE+2u);
    CHECK(actual && expected);memset(actual,37,FD2_FIELD_PAGE_SIZE+2u);
    memcpy(expected,actual,FD2_FIELD_PAGE_SIZE+2u);
    for(int col=0;col<=70;col++)for(int row=0;row<5;row++) {
        size_t source=col==0?0u:col==70?3u:2u;
        expected[1u+(size_t)(10+row)*456u+450u+(size_t)col]=game.info_assets.bars[source].pixels[row];
    }
    CHECK(short_bar_app(&game,actual+1,450,10,70)==0);
    CHECK(!memcmp(actual,expected,FD2_FIELD_PAGE_SIZE+2u));
    memset(actual,37,FD2_FIELD_PAGE_SIZE+2u);memcpy(expected,actual,FD2_FIELD_PAGE_SIZE+2u);
    expected[1u+335u*456u+454u]=game.info_assets.bars[0].pixels[0];
    CHECK(short_bar_app(&game,actual+1,454,335,55)==-1);
    CHECK(!memcmp(actual,expected,FD2_FIELD_PAGE_SIZE+2u));
    fd2_battle_animation_close(&ctx);free(actual);free(expected);return 0;
}

/* 1e739闭式列序按App真实owner；原23/24/25/29/30列来自独立档案RAW，
 * 不由产品绘制生成expected。先col后row，保首越界之前的每一个byte。 */
static int short_source_app_bar(uint8_t *dst,uint8_t raw[8][5],int x,int y,int width) {
    int last=width<=0?69:width<70?70:width;
    for(int col=0;col<=last;col++) {
        unsigned image=width<=0?(col==69?7u:6u):
            !col?0u:col<width?2u:col==width?3u:col==70?7u:6u;
        for(int row=0;row<5;row++) {
            int64_t at=(int64_t)(y+row)*456+x+col;
            if(at<0 || at>=FD2_FIELD_PAGE_SIZE)return -1;
            dst[(size_t)at]=raw[image][row];
        }
    }
    return 0;
}

static void short_source_app_panel(uint8_t *dst,const battle_image *image,int x,int y) {
    for(int row=0;row<image->image.height;row++)for(int col=0;col<image->image.width;col++) {
        size_t from=(size_t)row*(size_t)image->image.width+(size_t)col;
        int px=x+col,py=y+row;
        if(px>=0 && px<456 && py>=0 && py<336 && image->opaque[from])
            dst[(size_t)py*456u+(size_t)px]=image->image.pixels[from];
    }
}

/* 1e611完整正常写序；-1表示HP0不画，-2表示正HP/max0原IDIV点。
 * 显式宽度常量独立取自1e81b/1e81f MOVZX、69乘法、IDIV和INC。 */
static int short_source_counter_page(uint8_t *dst,const battle_image *panel,
        uint8_t raw[8][5],const int x[2],const int y[2],int actor_width,int target_width) {
    short_source_app_panel(dst,panel,x[1]+68,y[1]+68);
    if(short_source_app_bar(dst,raw,x[1]+75,y[1]+74,55)!=0)return -1;
    if(target_width==-2)return -1;
    if(target_width>=0 && short_source_app_bar(dst,raw,x[1]+75,y[1]+74,target_width)!=0)return -1;
    short_source_app_panel(dst,panel,x[0]+68,y[0]+68);
    if(actor_width==-2)return -1;
    return actor_width<0?0:short_source_app_bar(dst,raw,x[0]+75,y[0]+74,actor_width);
}

/* 真正SHORT阶段经公开adapter到达1e611；只替Game源phase叶，不替
 * App/面板/条及312×192投影。Typed HP高值/重叠descriptor不冒自然producer。 */
static int test_app_page_short_counter_adapter(void) {
    static const struct {
        uint16_t actor_hp,actor_max,target_hp,target_max;
        int actor_width,target_width,outcome; /* 0完成/1首错/2HOST/3runtime叶错。 */
    } cases[]={
        {42,42,21,42,70,35,0}, {21,42,0,0,35,-1,0}, {0,0,63,42,-1,104,0},
        {32768,65535,65535,65535,35,70,0}, {65535,65535,32768,65535,70,35,0},
        {1,0,21,42,-2,35,1}, {21,42,1,0,35,-2,1},
        {42,42,65535,1,70,4521916,1}, {42,42,1,0,70,-2,2}, {42,42,21,42,70,35,3}
    };
    for(size_t test=0;test<sizeof(cases)/sizeof(cases[0]);test++)for(int edge=0;edge<3;edge++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);game.units.count=2;
        game.units.items[0].x=(uint8_t)(edge==1?13:5);
        game.units.items[1].x=(uint8_t)(edge==1?14:6);
        game.units.items[0].y=game.units.items[1].y=4;
        game.units.items[0].direction=3;game.units.items[1].direction=0;
        fd2_field_physical_event event=map_physical_event(FD2_FIELD_PHYSICAL_MODE_SHORT,FD2_FIELD_PHYSICAL_PREPARE);
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        event.phase=FD2_FIELD_PHYSICAL_SHORT_MAP;
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        event.phase=FD2_FIELD_PHYSICAL_SHORT_BEGIN;
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        /* 第三组是显式有限descriptor重叠写序，不声称1ec2a自然产生该组合。 */
        if(edge==2) {
            ctx.exchange->panel_x[0]=ctx.exchange->panel_x[1]+8;
            ctx.exchange->panel_y[0]=ctx.exchange->panel_y[1];
        }
        fd2_field_page *page=fd2_field_page_create();CHECK(page);screen.field_page=page;
        fd2_field_visual_state *owner=game.visual_owner;game.visual_owner=&screen.field_visual;
        uint8_t *expected=malloc(FD2_FIELD_PAGE_SIZE),*actual=malloc(FD2_FIELD_PAGE_SIZE),*board_before=malloc(64000);
        fd2_field_game *before=malloc(sizeof(game));fd2_vga *vbefore=malloc(sizeof(screen));uint8_t raw[8][5];
        CHECK(expected && actual && board_before && before && vbefore && short_source_columns(raw)==0);
        memset(expected,17,FD2_FIELD_PAGE_SIZE);
        CHECK(fd2_field_page_write(page,0,0,456,336,expected,FD2_FIELD_PAGE_SIZE,456)==0);
        for(size_t i=0;i<64000u;i++)screen.framebuffer[i]=(uint8_t)(43u+i%31u);
        game.units.items[0].hp=cases[test].actor_hp;game.units.items[0].hp_max=cases[test].actor_max;
        game.units.items[1].hp=cases[test].target_hp;game.units.items[1].hp_max=cases[test].target_max;
        if(cases[test].outcome==2)screen.input.quit_requested=1;
        *before=game;*vbefore=screen;memcpy(board_before,ctx.exchange->board.framebuffer,64000);
        unsigned old_frames=frames,old_visual=visual_calls,old_terrain=terrain_calls,old_pumps=input_pumps;
        unsigned old_returns=return_calls,old_ticks=ticks,old_delays=delays,old_cues=cues;
        runtime_failure=cases[test].outcome==3;event.phase=FD2_FIELD_PHYSICAL_COUNTER_REFRESH;
        int failed=cases[test].outcome==1 || cases[test].outcome==3;
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==(failed?-1:0));
        CHECK(ctx.exchange->counter_refreshed && ctx.failed==failed && ctx.host_quit==(cases[test].outcome==2));
        CHECK(runtime_calls==(cases[test].outcome==2?0u:1u));
        if(runtime_calls)CHECK(runtime_phase==1 && runtime_display_mode==before->ordinary_display_mode);
        CHECK(visual_calls==old_visual && terrain_calls==old_terrain && return_calls==old_returns);
        CHECK(!memcmp(&game,before,sizeof(game)) && !business_rng_calls && ticks==old_ticks && delays==old_delays && cues==old_cues);
        CHECK(!memcmp(screen.palette,vbefore->palette,768) && !memcmp(screen.dac,vbefore->dac,768) &&
            !memcmp(&screen.field_visual,&vbefore->field_visual,sizeof(screen.field_visual)) &&
            screen.actor_jitter_tick==vbefore->actor_jitter_tick && screen.actor_jitter_phase==vbefore->actor_jitter_phase &&
            screen.field_water_last_tick==vbefore->field_water_last_tick && screen.field_water_phase==vbefore->field_water_phase);
        if(cases[test].outcome==3)expected[72u*456u+72u]=239; /* 已证runtime叶首错写前缀。 */
        else if(cases[test].outcome!=2) {
            for(int y=0;y<192;y++)for(int x=0;x<312;x++)expected[(y+72)*456+x+72]=runtime_pixel(1,x,y);
            CHECK(short_source_counter_page(expected,&ctx.exchange->map_fx[48],raw,
                ctx.exchange->panel_x,ctx.exchange->panel_y,cases[test].actor_width,cases[test].target_width)==(failed?-1:0));
        }
        CHECK(fd2_field_page_read(page,0,0,456,336,actual,FD2_FIELD_PAGE_SIZE,456)==0 &&
            !memcmp(expected,actual,FD2_FIELD_PAGE_SIZE));
        if(cases[test].outcome==0) {
            memcpy(board_before,vbefore->framebuffer,64000);
            for(int y=0;y<192;y++)memcpy(board_before+(size_t)(y+4)*320u+4u,
                expected+(size_t)(y+72)*456u+72u,312u);
            CHECK(frames==old_frames+1u && input_pumps==old_pumps+1u);
            CHECK(!memcmp(board_before,screen.framebuffer,64000) && !memcmp(board_before,ctx.exchange->board.framebuffer,64000));
        } else CHECK(frames==old_frames && input_pumps==old_pumps && !memcmp(board_before,ctx.exchange->board.framebuffer,64000) &&
            !memcmp(screen.framebuffer,vbefore->framebuffer,64000));
        CHECK(map_physical_finish(&ctx,FD2_FIELD_PHYSICAL_MODE_SHORT,1)==0);
        fd2_battle_animation_close(&ctx);game.visual_owner=owner;screen.field_page=NULL;
        fd2_field_page_destroy(page);free(expected);free(actual);free(board_before);free(before);free(vbefore);
    }
    return 0;
}

/* FULL物理/基本法术、派生与MAP skip共同归还真实Game owner；
 * 这里签FINISH adapter，不将没有重放整套cast的控制向量称原版演出证据。 */
static int test_app_page_normal_return_adapter(void) {
    const uint8_t ids[]={0,0,28,13},casters[]={0,0,19,9};
    const uint8_t modes[]={0,1,7,0};
    const size_t cast_entries[]={0,1,59,0};
    for(unsigned kind=0;kind<4;kind++)for(unsigned outcome=0;outcome<4;outcome++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);game.units.count=2;
        actor(&game.units.items[0],casters[kind],2,1);game.ordinary_display_mode=modes[kind];
        if(kind==3) {
            game.units.items[0].mp=game.units.items[0].mp_max=30;
            actor(&game.units.items[1],0,2,2);game.units.items[1].hp=21;
            game.cursor_cell_x=2;game.cursor_cell_y=1;
        }
        fd2_field_page *page=fd2_field_page_create();CHECK(page);screen.field_page=page;
        fd2_field_visual_state *owner=game.visual_owner;game.visual_owner=&screen.field_visual;
        uint8_t *seed=malloc(FD2_FIELD_PAGE_SIZE);fd2_field_game *before=malloc(sizeof(game));
        CHECK(seed && before);memset(seed,17,FD2_FIELD_PAGE_SIZE);
        CHECK(fd2_field_page_write(page,0,0,456,336,seed,FD2_FIELD_PAGE_SIZE,456)==0);free(seed);
        uint8_t targets[]={1};fd2_field_magic_present_event magic=magic_event(ids[kind],targets,1);
        if(kind==3) {
            /* execute_map_magic的正式事件：MAP固定一次费用，独立于FIGANI
             * intro marker；CAST/FINISH时51A83=0。本例只签返回adapter。 */
            magic.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;
            magic.effect_id=13;magic.cast_cost_count=1;
        }
        if(kind>=1 && kind<=2) {
            fd2_figani_magic_cast plan;
            CHECK(fd2_figani_magic_action_plan(&plan,ctx.figani,casters[kind],ids[kind])==0);
            /* 固定FIGANI真实目录：entry1/59的intro各有一个费用marker；
             * 基本FULL也须提供元数据，不能把零初始化的event当合法无费用cast。 */
            CHECK(!plan.clip.empty && plan.selected_entry==cast_entries[kind] && plan.cost_marker_count==1);
            magic.cast_cost_count=plan.cost_marker_count;
            if(kind==2)magic.presentation_mode=FD2_FIELD_MAGIC_MODE_DERIVED;
        }
        fd2_field_physical_event physical=make_event(FD2_FIELD_PHYSICAL_PREPARE);
        int prepared=kind?fd2_battle_animation_magic_present(&game,&magic,&ctx):
            fd2_battle_animation_physical_present(&game,&physical,&ctx);
        if(prepared!=0)fprintf(stderr,"App page return fixture PREPARE kind=%u outcome=%u cost=%u failed\n",
            kind,outcome,(unsigned)magic.cast_cost_count);
        CHECK(prepared==0);
        if(outcome==1 || (kind==3 && outcome==2))fd2_battle_animation_set_skip(&ctx,1);
        /* 真STRIKE使FULL失败例已经started，错误后不能再走成功fade-in。 */
        if(kind==0 && outcome==2) {
            fd2_field_attack_result hit={0};hit.hit=1;hit.rolled_damage=9;hit.hp_after=33;
            physical.phase=FD2_FIELD_PHYSICAL_STRIKE;physical.actor_before=&game.units.items[0];
            physical.target_before=&game.units.items[1];physical.result=&hit;
            CHECK(fd2_battle_animation_physical_present(&game,&physical,&ctx)==0);
        }
        *before=game;unsigned clocks=visual_calls,terrains=terrain_calls,returns=return_calls;
        uint16_t jitter_tick=screen.actor_jitter_tick,water_tick=screen.field_water_last_tick;
        uint8_t jitter_phase=screen.actor_jitter_phase,water_phase=screen.field_water_phase;
        fd2_field_visual_state visual=screen.field_visual;
        runtime_failure=outcome==2;if(outcome==3)screen.input.quit_requested=1;
        CHECK((kind?magic_finish(&ctx,&magic,0):finish(&ctx,0))==(outcome==2?-1:0));
        unsigned called=outcome!=3 && (kind!=3 || outcome==1 || outcome==2);
        CHECK(runtime_calls==called && (!called || (runtime_phase==3 && runtime_display_mode==modes[kind])));
        CHECK(visual_calls==clocks && terrain_calls==terrains && return_calls==returns);
        CHECK(!memcmp(&game,before,sizeof(game)) && !business_rng_calls &&
            !memcmp(&screen.field_visual,&visual,sizeof(visual)) &&
            screen.actor_jitter_tick==jitter_tick && screen.actor_jitter_phase==jitter_phase &&
            screen.field_water_last_tick==water_tick && screen.field_water_phase==water_phase);
        CHECK(check_runtime_page(page,3,outcome==2,(int)called)==0);
        if(outcome==2)CHECK(frames==runtime_frames && !ctx.exchange && !ctx.magic_exchange);
        if(called && outcome!=2)for(int y=0;y<192;y++)for(int x=0;x<312;x++)
            CHECK(screen.framebuffer[(y+4)*320+x+4]==runtime_pixel(3,x,y));
        fd2_battle_animation_close(&ctx);game.visual_owner=owner;screen.field_page=NULL;
        fd2_field_page_destroy(page);free(before);
    }
    return 0;
}

static void raw5_cache_fixture(void) {
    game.units.items[0].unit_id=126;game.units.items[0].direction=5;
    game.units.items[0].frame_phase=0;game.units.items[0].hp=17;
    game.units.sprite_cache_exact=1;game.units.sprite_cache_count=2;
    game.units.sprite_cache_classes[0]=game.units.sprite_cache_classes[1]=0;
    game.units.sprite_cache_ids[0]=0;game.units.sprite_cache_ids[1]=96;
}

static int test_raw5_recovery_and_short_preflight(void) {
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);raw5_cache_fixture();
    fd2_field_physical_event event=map_physical_event(FD2_FIELD_PHYSICAL_MODE_RECOVERY,FD2_FIELD_PHYSICAL_PREPARE);
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    uint8_t *expected=malloc(64000);CHECK(expected);
    for (uint8_t phase=0;phase<4;phase++) {
        game.idle_phase=phase;memset(screen.framebuffer,109,64000);memset(expected,109,64000);
        CHECK(map_physical_actor(&game,ctx.exchange,screen.framebuffer,0,1,1,NULL,0)==0);
        /* class0→class1真实ID96，而非当前hot126或baseID0+1；raw5索引15
         * 恰为下一class的3号帧。只是夹具等效frame表达，不把5在生产中&3。 */
        fd2_field_unit projected=game.units.items[0];projected.direction=1;
        expected_actor(expected,&projected,96,phase,200,1);
        CHECK(!memcmp(screen.framebuffer,expected,64000));
    }
    game.idle_phase=3;event.phase=FD2_FIELD_PHYSICAL_RECOVERY_STEP;
    event.recovery_step=FD2_FIELD_RECOVERY_WAIT;event.step_value=1;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    event.recovery_step=FD2_FIELD_RECOVERY_ACTOR;event.step_value=2;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    memset(expected,62,64000);fd2_field_unit projected=game.units.items[0];projected.direction=1;
    expected_actor(expected,&projected,96,3,200,1);CHECK(!memcmp(screen.framebuffer,expected,64000));
    event.step_value=0;CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
    expected_actor(expected,&projected,96,3,-1,1);CHECK(!memcmp(screen.framebuffer,expected,64000));
    CHECK(map_physical_finish(&ctx,FD2_FIELD_PHYSICAL_MODE_RECOVERY,1)==0);
    fd2_battle_animation_close(&ctx);
    size_t total_frames=game.sprites.frame_count;
    fd2_field_game *before=malloc(sizeof(game));fd2_vga *old=malloc(sizeof(screen));CHECK(before && old);
    for (int failure=0;failure<5;failure++) {
        CHECK(setup(&ctx)==0);raw5_cache_fixture();
        if (failure==0)game.units.sprite_cache_exact=0;
        else if (failure==1)game.units.sprite_cache_count=1;
        else if (failure==2)game.units.sprite_cache_ids[1]=255;
        else if (failure==3)game.sprites.frame_count=96u*12u+5u;
        else game.units.items[0].frame_phase=1; /* 未有来源的raw5非零步不猜。 */
        *before=game;*old=screen;
        event=map_physical_event(FD2_FIELD_PHYSICAL_MODE_RECOVERY,FD2_FIELD_PHYSICAL_PREPARE);
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==-1 && !ctx.exchange);
        CHECK(!memcmp(before,&game,sizeof(game)) && !memcmp(old,&screen,sizeof(screen)));
        fd2_battle_animation_close(&ctx);game.sprites.frame_count=total_frames;
    }
    CHECK(setup(&ctx)==0);raw5_cache_fixture();
    event=map_physical_event(FD2_FIELD_PHYSICAL_MODE_SHORT,FD2_FIELD_PHYSICAL_PREPARE);event.resource_only=1;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0 && !ctx.exchange);
    game.units.items[1].direction=5;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0 && !ctx.exchange);
    game.units.sprite_cache_count=1;*before=game;*old=screen;
    CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==-1 && !ctx.exchange);
    CHECK(!memcmp(before,&game,sizeof(game)) && !memcmp(old,&screen,sizeof(screen)));
    fd2_battle_animation_close(&ctx);game.units.sprite_cache_exact=0;game.units.sprite_cache_count=0;
    free(expected);free(before);free(old);return 0;
}

static int test_short_six_weapon_families_and_miss(void) {
    static const uint8_t offsets[]={0,13,26,39,56,73};
    uint8_t *expected=malloc(64000),*saved=malloc(64000);CHECK(expected && saved);
    for (size_t family=0;family<6;family++) for (int variant=0;variant<3;variant++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
        uint8_t weapon=255;
        for (unsigned i=0;i<128;i++) {
            const uint8_t *item=fd2_field_item_record_get((uint8_t)i);
            if (item && item[0]<21 && k_battle_short_fx_offsets[item[0]]==offsets[family]) {weapon=(uint8_t)i;break;}
        }
        CHECK(weapon!=255);((uint8_t *)&game.units.items[0])[11]=weapon;
        game.units.items[1].x=6;game.units.items[1].y=3;game.units.items[1].detail_status[1]=4;
        fd2_field_physical_event event=map_physical_event(FD2_FIELD_PHYSICAL_MODE_SHORT,FD2_FIELD_PHYSICAL_PREPARE);
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0 && !ctx.exchange->counter_ready);
        event.phase=FD2_FIELD_PHYSICAL_SHORT_MAP;CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        event.phase=FD2_FIELD_PHYSICAL_SHORT_BEGIN;CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        CHECK(frames==11 && delays==250); /* MAP1 + BEGIN10；初条只写FB。 */
        fd2_field_unit a=game.units.items[0],b=game.units.items[1];
        fd2_field_attack_result result={0};result.hit=(uint8_t)(variant!=0);
        result.hp_after=(uint16_t)(variant==1 ? 21:42);result.rolled_damage=variant==1 ? 21u:0u;
        game.units.items[1].hp=result.hp_after;
        event.phase=FD2_FIELD_PHYSICAL_STRIKE;event.actor_before=&a;event.target_before=&b;event.result=&result;
        fd2_field_game *before=malloc(sizeof(game));CHECK(before);*before=game;
        reset_observer();capture_map=1;expected_primary=(fd2_pcm_player *)(uintptr_t)1;ctx.primary=expected_primary;
        memcpy(expected,screen.framebuffer,64000);
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        CHECK(!memcmp(before,&game,sizeof(game)));free(before);
        size_t at=offsets[family],count=k_battle_short_fx_sequences[at++],cue_count=0;
        const uint8_t *bank;size_t bank_size;CHECK(fd2_archive_get(&fdother,64,&bank,&bank_size)==0);
        for (size_t i=0;i<count;i++,at+=2) {
            if (result.hit && i<2) expected_actor(expected,&b,96,0,i==0 ? 64:-1,0);
            memcpy(saved,expected,64000);
            const battle_image *image=&ctx.exchange->map_fx[k_battle_short_fx_sequences[at]];
            expected_image(expected,image,148,72);
            CHECK(!memcmp(expected,map_captured+i*64000u,64000));
            /* 15e71恢复整个FX矩形，包括透明处；仅clip宿主VGA边界。 */
            for (int y=72;y<72+image->image.height && y<200;y++)
                for (int x=148;x<148+image->image.width && x<320;x++) expected[y*320+x]=saved[y*320+x];
            if (k_battle_short_fx_sequences[at+1]!=255) {
                CHECK(map_cue_banks[cue_count]==bank);
                CHECK(map_cue_indices[cue_count++]==(result.hit ? k_battle_short_fx_sequences[at+1]:4u));
            }
        }
        CHECK(cue_count==map_cue_count);
        int after=variant==1 ? 35:70;
        size_t index=count;
        for (int width=70;width>=after;width--) {
            expected_bar(expected,ctx.exchange->panel_x[1]+7,ctx.exchange->panel_y[1]+6,width);
            CHECK(!memcmp(expected,map_captured+index++*64000u,64000));
        }
        CHECK(!memcmp(expected,map_captured+index++*64000u,64000) && index==map_captured_count);
        CHECK(delays==count*80u+(size_t)(71-after)*8u && !ticks && !business_rng_calls);
        CHECK(map_physical_finish(&ctx,FD2_FIELD_PHYSICAL_MODE_SHORT,1)==0 && !ctx.exchange);
        CHECK(!primary_stops);fd2_battle_animation_close(&ctx);
    }
    free(expected);free(saved);return 0;
}

static int test_recovery_steps_zero_max_skip_and_quit(void) {
    uint8_t *expected=malloc(64000);fd2_field_game *before=malloc(sizeof(game));
    fd2_vga *old=malloc(sizeof(screen));CHECK(expected && before && old);
    static const fd2_field_recovery_step operations[]={FD2_FIELD_RECOVERY_WAIT,FD2_FIELD_RECOVERY_CUE,
        FD2_FIELD_RECOVERY_ACTOR,FD2_FIELD_RECOVERY_WAIT,FD2_FIELD_RECOVERY_ACTOR,FD2_FIELD_RECOVERY_WAIT};
    static const uint16_t values[]={1,4,2,1,0,1};
    for (int variant=0;variant<6;variant++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);game.idle_phase=3;
        effect_info_marker=1;
        screen.actor_jitter_phase=1;screen.actor_jitter_tick=23;
        game.units.items[0].hp=17;if (variant==1) game.units.items[0].hp_max=0;
        fd2_field_unit caster=game.units.items[0];
        fd2_field_physical_event event=map_physical_event(FD2_FIELD_PHYSICAL_MODE_RECOVERY,FD2_FIELD_PHYSICAL_PREPARE);
        *before=game;*old=screen;
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        CHECK(!memcmp(before,&game,sizeof(game)) && !memcmp(old,&screen,sizeof(screen)));
        expected_primary=(fd2_pcm_player *)(uintptr_t)1;ctx.primary=expected_primary;
        capture_map=1;if (variant==2)ctx.skip=1;if (variant==3)quit_frame=2;if (variant==4)cue_failure=1;
        event.phase=FD2_FIELD_PHYSICAL_RECOVERY_STEP;
        int failed=0;
        for (size_t i=0;i<6;i++) {
            event.recovery_step=operations[i];event.step_value=values[i];
            int status=fd2_battle_animation_physical_present(&game,&event,&ctx);
            CHECK(!memcmp(before,&game,sizeof(game))); /* 每wait/cue/actor都在HP写前。 */
            if (status<0) {CHECK(variant==4 && i==1);failed=1;break;}
            if (variant<2) CHECK(ctx.exchange->board.actor_jitter_phase==1 && ctx.exchange->board.actor_jitter_tick==23);
        }
        if (variant<2) {
            CHECK(frames==5 && ticks==3 && !delays && cues==1 && map_cue_count==1);
            CHECK(background_return_calls==1 && effect_return_calls==0);
            const uint8_t *bank;size_t size;CHECK(fd2_archive_get(&fdother,31,&bank,&size)==0);
            CHECK(map_cue_banks[0]==bank && map_cue_indices[0]==4);
            memset(expected,62,64000);CHECK(!memcmp(expected,map_captured,64000));
            expected_actor(expected,&caster,0,3,200,1);
            CHECK(!memcmp(expected,map_captured+64000,64000) && !memcmp(expected,map_captured+128000,64000));
            expected_actor(expected,&caster,0,3,-1,1);
            CHECK(!memcmp(expected,map_captured+192000,64000) && !memcmp(expected,map_captured+256000,64000));
        } else if (variant==2) CHECK(!frames && !ticks && !cues);
        else if (variant==3) CHECK(frames==2 && ticks==1 && cues==1 && ctx.host_quit);
        if (!failed) {
            game.units.items[0].hp=variant==1 ? 0:25;
            if (variant==5) screen.input.quit_requested=1; /* 最后STEP后、FINISH前到达quit。 */
            *before=game;fd2_vga finish_before=screen;
            CHECK(map_physical_finish(&ctx,FD2_FIELD_PHYSICAL_MODE_RECOVERY,0)==0);
            CHECK(!memcmp(before,&game,sizeof(game)) && !ctx.exchange);
            if (variant!=3 && variant!=5) CHECK(!memcmp(&finish_before,&screen,sizeof(screen))); /* FINISH只free。 */
            if (variant==5) CHECK(ctx.host_quit);
        } else CHECK(map_physical_finish(&ctx,FD2_FIELD_PHYSICAL_MODE_RECOVERY,1)==0);
        if (variant>=3) CHECK(!memcmp(old->framebuffer,screen.framebuffer,64000));
        CHECK(!primary_stops && !business_rng_calls);fd2_battle_animation_close(&ctx);
    }
    free(expected);free(before);free(old);return 0;
}

static int test_recovery_caller_info_context(void) {
    /* 原13fd4仅18d8c玩家WAIT和13a9f自动actor两个caller域。mode3的替身
     * marker只区分共享painter选择；真实info像素仍由field_unit_render覆盖。 */
    static const struct { uint8_t phase,side,option;int selected;fd2_field_interaction interaction;int info; } cases[]={
        {2,2,0,0,FD2_FIELD_INTERACTION_COMMAND,0},
        {2,2,1,0,FD2_FIELD_INTERACTION_COMMAND,1},
        {2,2,128,0,FD2_FIELD_INTERACTION_COMMAND,1},
        {0,0,1,-1,FD2_FIELD_INTERACTION_BROWSE,0},
        {1,1,1,-1,FD2_FIELD_INTERACTION_BROWSE,0},
        {2,2,1,1,FD2_FIELD_INTERACTION_COMMAND,0},
        {2,2,1,0,FD2_FIELD_INTERACTION_BROWSE,0},
        {2,0,1,0,FD2_FIELD_INTERACTION_COMMAND,0}
    };
    static const fd2_field_recovery_step steps[]={FD2_FIELD_RECOVERY_WAIT,FD2_FIELD_RECOVERY_CUE,
        FD2_FIELD_RECOVERY_ACTOR,FD2_FIELD_RECOVERY_WAIT,FD2_FIELD_RECOVERY_ACTOR,FD2_FIELD_RECOVERY_WAIT};
    static const uint16_t args[]={1,4,2,1,0,1};
    fd2_field_game *before=malloc(sizeof(game));uint8_t *expected=malloc(64000);CHECK(before && expected);
    for (size_t c=0;c<sizeof(cases)/sizeof(cases[0]);c++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
        game.active_side=cases[c].phase;game.units.items[0].side=cases[c].side;
        game.option_values[3]=cases[c].option;game.selected_unit=cases[c].selected;
        game.interaction=cases[c].interaction;game.units.items[0].hp=17;
        effect_info_marker=1;
        fd2_field_physical_event event=map_physical_event(FD2_FIELD_PHYSICAL_MODE_RECOVERY,FD2_FIELD_PHYSICAL_PREPARE);
        *before=game;
        CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
        CHECK(ctx.exchange->recovery_with_info==cases[c].info && !frames && !return_calls);
        CHECK(!memcmp(before,&game,sizeof(game)));
        capture_map=1;event.phase=FD2_FIELD_PHYSICAL_RECOVERY_STEP;
        for (size_t i=0;i<sizeof(steps)/sizeof(steps[0]);i++) {
            event.recovery_step=steps[i];event.step_value=args[i];
            CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0);
            CHECK(!memcmp(before,&game,sizeof(game)));
        }
        CHECK(frames==5 && ticks==3 && !delays && !visual_calls);
        CHECK(effect_return_calls==(unsigned)cases[c].info && background_return_calls==(unsigned)!cases[c].info);
        memset(expected,62,64000);if(cases[c].info)expected[184u*320u+20u]=233;
        CHECK(!memcmp(expected,map_captured,64000));
        expected_actor(expected,&game.units.items[0],0,0,200,1);
        CHECK(!memcmp(expected,map_captured+64000u,64000) && !memcmp(expected,map_captured+128000u,64000));
        expected_actor(expected,&game.units.items[0],0,0,-1,1);
        CHECK(!memcmp(expected,map_captured+192000u,64000) && !memcmp(expected,map_captured+256000u,64000));
        fd2_vga last=screen;
        CHECK(map_physical_finish(&ctx,FD2_FIELD_PHYSICAL_MODE_RECOVERY,0)==0 && !ctx.exchange);
        CHECK(!memcmp(&last,&screen,sizeof(screen)) && !memcmp(before,&game,sizeof(game)));
        CHECK(!business_rng_calls);fd2_battle_animation_close(&ctx);
    }
    free(before);free(expected);return 0;
}

static int test_prepare_promotion_scope_and_sequence(void) {
    reset_observer();
    fd2_archive bg={0};CHECK(fd2_archive_open(&bg,"original_game/BG.DAT")==0);
    fd2_vga *saved=malloc(sizeof(screen));CHECK(saved);*saved=screen;
    fd2_field_game *before=malloc(sizeof(game));CHECK(before);*before=game;
    fd2_prepare_promotion *scope=NULL;
    fd2_archive short_figani=figani_fixture;short_figani.count=156;
    CHECK(fd2_prepare_promotion_open(&scope,&screen,&bg,&short_figani,9,52)<0 && !scope);
    CHECK(!memcmp(saved,&screen,sizeof(screen)) && !frames);
    fd2_archive short_bg=bg;short_bg.count=2;
    CHECK(fd2_prepare_promotion_open(&scope,&screen,&short_bg,&figani_fixture,9,52)<0 && !scope);
    CHECK(!memcmp(saved,&screen,sizeof(screen)) && !frames);
    CHECK(fd2_prepare_promotion_open(&scope,&screen,&bg,&figani_fixture,9,52)==0 && scope);
    CHECK(!memcmp(saved,&screen,sizeof(screen)) && !frames);
    CHECK(fd2_prepare_promotion_step(scope,FD2_PREPARE_PROMOTION_OLD_ENTER,34)<0);
    CHECK(fd2_prepare_promotion_step(scope,FD2_PREPARE_PROMOTION_RESTORE,1)<0);
    CHECK(!memcmp(saved,&screen,sizeof(screen)) && !frames);
    static const struct { fd2_prepare_promotion_phase phase;unsigned value; } sequence[]={
        {FD2_PREPARE_PROMOTION_BEGIN,1},{FD2_PREPARE_PROMOTION_OLD_ENTER,9},
        {FD2_PREPARE_PROMOTION_OLD_IDLE,16},{FD2_PREPARE_PROMOTION_WHITE,20},
        {FD2_PREPARE_PROMOTION_NEW_FRAME,200},{FD2_PREPARE_PROMOTION_NEW_IDLE,24},
        {FD2_PREPARE_PROMOTION_RESTORE,0}};
    for (size_t i=0;i<sizeof(sequence)/sizeof(sequence[0]);i++)
        CHECK(fd2_prepare_promotion_step(scope,sequence[i].phase,sequence[i].value)==0);
    CHECK(frames==263 && ticks==41 && delays==586);
    CHECK(!memcmp(saved->framebuffer,screen.framebuffer,64000));
    CHECK(!memcmp(saved->palette,screen.palette,768));
    for (size_t i=0;i<768;i++) CHECK(screen.dac[i]==(uint8_t)((screen.palette[i]<<2)|(screen.palette[i]>>4)));
    CHECK(!memcmp(before,&game,sizeof(game)) && !business_rng_calls && !cues && !primary_stops);
    fd2_prepare_promotion_close(scope,0);scope=NULL;
    *saved=screen;reset_observer();
    CHECK(fd2_prepare_promotion_open(&scope,&screen,&bg,&figani_fixture,9,52)==0);
    quit_frame=2;
    CHECK(fd2_prepare_promotion_step(scope,FD2_PREPARE_PROMOTION_BEGIN,1)==-2 && frames==2);
    CHECK(fd2_prepare_promotion_step(scope,FD2_PREPARE_PROMOTION_OLD_IDLE,16)==-2);
    fd2_prepare_promotion_close(scope,1);
    CHECK(!memcmp(saved->framebuffer,screen.framebuffer,64000) && !memcmp(saved->palette,screen.palette,768) &&
        !memcmp(saved->dac,screen.dac,768) && !memcmp(before,&game,sizeof(game)));
    fd2_archive_close(&bg);free(saved);free(before);return 0;
}

static int test_prepare_promotion_duration_and_safe_parameters(void) {
    reset_observer();fd2_archive bg={0};CHECK(fd2_archive_open(&bg,"original_game/BG.DAT")==0);
    fd2_prepare_promotion *scope=NULL;
    CHECK(fd2_prepare_promotion_open(&scope,&screen,&bg,&figani_fixture,9,52)==0);
    uint8_t *expected=calloc(64000,1);CHECK(expected);
    scope->old_idle.parts[0].part.duration=0;
    CHECK(fd2_prepare_promotion_step(scope,FD2_PREPARE_PROMOTION_OLD_IDLE,2)==0);
    expected_image(expected,&scope->background[2],0,50);
    expected_image(expected,&scope->old_idle.parts[0].drawing,
        scope->old_idle.parts[0].part.x,scope->old_idle.parts[0].part.y);
    CHECK(!memcmp(expected,screen.framebuffer,64000) && frames==2 && ticks==2);
    for (size_t i=0;i<scope->old_idle.clip.part_count;i++) scope->old_idle.parts[i].part.duration=1;
    unsigned count=(unsigned)scope->old_idle.clip.part_count+1u;
    CHECK(fd2_prepare_promotion_step(scope,FD2_PREPARE_PROMOTION_OLD_IDLE,count)==0);
    memset(expected,0,64000);
    expected_image(expected,&scope->background[count%3u],0,50);
    expected_image(expected,&scope->old_idle.parts[0].drawing,
        scope->old_idle.parts[0].part.x,scope->old_idle.parts[0].part.y);
    CHECK(!memcmp(expected,screen.framebuffer,64000));
    /* 安全有界参数不锁死默认9/16/20/200/24，也不把Esc当跳过。 */
    CHECK(fd2_prepare_promotion_step(scope,FD2_PREPARE_PROMOTION_NEW_FRAME,0)==0);
    CHECK(fd2_prepare_promotion_step(scope,FD2_PREPARE_PROMOTION_WHITE,0)==0);
    CHECK(fd2_prepare_promotion_step(scope,FD2_PREPARE_PROMOTION_NEW_IDLE,0)==0);
    CHECK(!cues && !business_rng_calls);
    fd2_prepare_promotion_close(scope,1);fd2_archive_close(&bg);free(expected);return 0;
}

static int test_prepare_promotion_empty_idle_only_omits_sprite(void) {
    fd2_archive bg={0};CHECK(fd2_archive_open(&bg,"original_game/BG.DAT")==0);
    uint8_t *expected=calloc(64000,1);CHECK(expected);
    /* 16→48真实合法路径；old48仅检验资源helper对同一哨兵的对称安全边界。 */
    for (int variant=0;variant<2;variant++) {
        reset_observer();fd2_prepare_promotion *scope=NULL;
        CHECK(fd2_prepare_promotion_open(&scope,&screen,&bg,&figani_fixture,
            variant ? 48:16,variant ? 52:48)==0);
        const battle_clip *empty=variant ? &scope->old_idle:&scope->new_idle;
        CHECK(empty->clip.empty && !empty->parts);
        CHECK(fd2_prepare_promotion_step(scope,variant ? FD2_PREPARE_PROMOTION_OLD_IDLE:
            FD2_PREPARE_PROMOTION_NEW_IDLE,24)==0 && frames==24 && ticks==24);
        memset(expected,0,64000);expected_image(expected,&scope->background[0],0,50);
        CHECK(!memcmp(expected,screen.framebuffer,64000) && !cues && !business_rng_calls);
        CHECK(fd2_prepare_promotion_step(scope,variant ? FD2_PREPARE_PROMOTION_OLD_ENTER:
            FD2_PREPARE_PROMOTION_NEW_FRAME,variant ? 9:200)==0);
        CHECK(!memcmp(expected,screen.framebuffer,64000));
        fd2_prepare_promotion_close(scope,1);
    }
    /* 三字节哨兵不可扩成四字节“空资源”以吞掉坏格式。 */
    fd2_prepare_promotion *scope=NULL;uint32_t next=figani_fixture.offsets[145];
    figani_fixture.offsets[145]=figani_fixture.offsets[144]+4u;
    CHECK(fd2_prepare_promotion_open(&scope,&screen,&bg,&figani_fixture,16,48)<0 && !scope);
    figani_fixture.offsets[145]=next;
    fd2_archive_close(&bg);free(expected);return 0;
}

/* 新回归只签2facd的定位/owner组合。18c6d字体、RAW列和signed stats
 * 复用已独立资格painter；参考坐标为原literal，不从新冷表或helper取expected。 */
static int test_battle_summary_raw_predicate(void) {
    fd2_field_game *saved=malloc(sizeof(game));fd2_vga *reference=malloc(sizeof(screen));
    CHECK(saved && reference);*saved=game;
    for(unsigned stage=23;stage<=25;stage++)for(size_t raw=16;raw<=18;raw++)for(uint8_t side=0;side<=2;side++) {
        fd2_battle_animation ctx;CHECK(setup(&ctx)==0);game.stage=stage;game.units.count=19;
        fd2_field_unit unit=game.units.items[0];unit.side=side;unit.unit_id=0;
        fd2_vga *actual=calloc(1,sizeof(*actual));CHECK(actual);
        for(size_t i=0;i<64000;i++)actual->framebuffer[i]=(uint8_t)(i*13u+i/320u+31u);
        *reference=*actual;int bottom=!side || (stage==24 && raw==17);
        CHECK(fd2_field_unit_summary_draw_at(reference,&game.info_assets,&game.font,&game.ui_text,
            &unit,bottom?0:171,bottom?154:4)==0);
        CHECK(battle_summary_draw(actual,&game,&unit,raw)==0 && !memcmp(actual,reference,sizeof(*actual)));
        CHECK(!frames && !ticks && !delays && !business_rng_calls);
        free(actual);fd2_battle_animation_close(&ctx);
    }
    /* 原定址后的owner首错，不得改成stage24特殊case整页回滚。 */
    fd2_battle_animation ctx;CHECK(setup(&ctx)==0);game.stage=24;game.units.count=19;
    fd2_field_unit unit=game.units.items[0];unit.side=2;unit.hp=32767;unit.hp_max=1;
    fd2_vga *actual=calloc(1,sizeof(*actual));CHECK(actual);memset(actual->framebuffer,173,64000);*reference=*actual;
    CHECK(fd2_field_unit_summary_draw_at(reference,&game.info_assets,&game.font,&game.ui_text,&unit,0,154)==-1);
    CHECK(battle_summary_draw(actual,&game,&unit,17)==-1 && !memcmp(actual,reference,sizeof(*actual)));
    CHECK(!frames && !ticks && !delays && !business_rng_calls);free(actual);fd2_battle_animation_close(&ctx);
    /* 真正公开PREPARE：两种raw17角色，unit_id刻意与17不同。
     * expected保持2e52b→BG→2e5a6和301ca→301de→BG的不同源序。 */
    for(unsigned magic=0;magic<2;magic++)for(unsigned target_override=0;target_override<2;target_override++) {
        CHECK(setup(&ctx)==0);game.stage=24;game.units.count=19;
        fd2_field_unit caster=game.units.items[0],target=game.units.items[1];caster.unit_id=magic?9:0;
        caster.side=target_override?0:2;target.side=target_override?1:0;target.hp=21;
        size_t actor_raw=target_override?0u:17u,target_raw=target_override?17u:0u;
        game.active_side=caster.side;game.units.items[actor_raw]=caster;game.units.items[target_raw]=target;
        fd2_field_game *before=malloc(sizeof(game));fd2_vga *vbefore=malloc(sizeof(screen));CHECK(before && vbefore);
        *before=game;*vbefore=screen;memset(reference,0,sizeof(*reference));
        int actor_bottom=!caster.side || actor_raw==17u,target_bottom=!target.side || target_raw==17u;
        if(!magic) {
            fd2_field_physical_event event=make_event(FD2_FIELD_PHYSICAL_PREPARE);event.actor_index=actor_raw;event.target_index=target_raw;
            CHECK(fd2_battle_animation_physical_present(&game,&event,&ctx)==0 && !ctx.exchange->attack[0].clip.split_mode);
            CHECK(fd2_field_unit_summary_draw_at(reference,&game.info_assets,&game.font,&game.ui_text,&caster,
                actor_bottom?0:171,actor_bottom?154:4)==0);
            expected_image(reference->framebuffer,&ctx.exchange->bg[0],0,50);
            CHECK(fd2_field_unit_summary_draw_at(reference,&game.info_assets,&game.font,&game.ui_text,&target,
                target_bottom?0:171,target_bottom?154:4)==0);
            CHECK(!memcmp(reference->framebuffer,ctx.exchange->board.framebuffer,64000));
            CHECK(finish(&ctx,1)==0);
        } else {
            uint8_t targets[1]={(uint8_t)target_raw};fd2_field_magic_present_event event=magic_event(0,targets,1);
            event.actor_index=actor_raw;event.actor_before=&game.units.items[actor_raw];fd2_figani_magic_cast plan;
            CHECK(fd2_figani_magic_action_plan(&plan,ctx.figani,caster.unit_id,0)==0);event.cast_cost_count=plan.cost_marker_count;
            CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
            CHECK(fd2_field_unit_summary_draw_at(reference,&game.info_assets,&game.font,&game.ui_text,&caster,
                actor_bottom?0:171,actor_bottom?154:4)==0);
            CHECK(fd2_field_unit_summary_draw_at(reference,&game.info_assets,&game.font,&game.ui_text,&target,
                target_bottom?0:171,target_bottom?154:4)==0);
            expected_image(reference->framebuffer,&ctx.magic_exchange->bg,0,50);
            CHECK(!memcmp(reference->framebuffer,ctx.magic_exchange->board.framebuffer,64000));
            CHECK(magic_finish(&ctx,&event,1)==0);
        }
        CHECK(!memcmp(before,&game,sizeof(game)) && !memcmp(vbefore->framebuffer,screen.framebuffer,64000) &&
            !memcmp(vbefore->palette,screen.palette,768) && !memcmp(vbefore->dac,screen.dac,768) &&
            !frames && !ticks && !delays && !business_rng_calls);
        fd2_battle_animation_close(&ctx);free(before);free(vbefore);
    }
    game=*saved;free(saved);free(reference);return 0;
}

#define FD2_F4_REMAINDER_BATTLE
#include "f4_remainder_probe.h"
#undef FD2_F4_REMAINDER_BATTLE

static int test_finite_frame_foreign_deadline(void) {
    reset_observer();input_pumps=0;
    uint64_t saved_ms=fake_ms;
    screen.frame_deadline_ns=UINT64_C(999999999999);screen.frame_interval_ns=16000000;
    fd2_battle_animation context={0};context.vga=&screen;
    present(&context,2);
    CHECK(frames==1 && ticks==2 && input_pumps==1 &&
          screen.frame_deadline_ns==UINT64_C(999999999999) && screen.frame_interval_ns==16000000);
    context.skip=1;present(&context,3);
    CHECK(frames==1 && ticks==2 && input_pumps==1);
    fd2_prepare_promotion *promotion=calloc(1,sizeof(*promotion));CHECK(promotion);
    promotion->vga=&screen;
    CHECK(promotion_present(promotion,1)==0 && frames==2 && ticks==3 && input_pumps==2);
    CHECK(screen.frame_deadline_ns==UINT64_C(999999999999) && screen.frame_interval_ns==16000000);
    screen.input.quit_requested=1;
    CHECK(promotion_present(promotion,1)==-2 && frames==2 && ticks==3 && input_pumps==2);
    free(promotion);screen.frame_deadline_ns=screen.frame_interval_ns=0;
    reset_observer();fake_ms=saved_ms;return 0;
}

#include "f4_physical_composition_probe.h"
#include "f4_magic_linear_work_probe.h"
#include "f4_promotion_composition_probe.h"
#include "f4_map_item_history_probe.h"
#include "f4_battle_history_probe.h"
#include "f4_composite_history_probe.h"

/* Battle自己的1da16 adapter，独立原指令mask每行col1/2/4。
 * 非匀整App前态、RAW0、solid200和真实owner首错；不借待测RLE造expected。 */
static int test_map_physical_linear_app_adapter(void) {
    fd2_field_game *g=calloc(1,sizeof(*g));
    fd2_battle_animation_exchange *e=calloc(1,sizeof(*e));
    fd2_field_page *page=fd2_field_page_create();
    uint8_t *before=malloc(FD2_FIELD_PAGE_SIZE),*expected=malloc(FD2_FIELD_PAGE_SIZE),
        *actual=malloc(FD2_FIELD_PAGE_SIZE),*decoded=calloc(12u*24u*24u,1);
    CHECK(g && e && page && before && expected && actual && decoded);
    uint8_t stream[24*7];uint32_t offsets[12]={0};
    static const uint8_t row[7]={0xc0,0x81,0,7,0x40,9,0xd2};
    static const unsigned phases[6]={0,18,19,68,96,255};
    static const int steps[4]={1824,-4,-1824,4},columns[3]={1,2,4};
    static const uint8_t colors[3]={0,7,9};
    for(size_t y=0;y<24;y++)memcpy(stream+y*7,row,7);
    for(size_t i=0;i<FD2_FIELD_PAGE_SIZE;i++)before[i]=(uint8_t)(i*17u+i/456u);
    g->units.count=1;g->sprites=(fd2_map_sprite_bank){.data=stream,.size=sizeof(stream),
        .offsets=offsets,.frame_count=12,.decoded_frames=decoded};
    e->actor[0]=0;e->sprite_ids[0]=0;
    unsigned old_frames=frames,old_ticks=ticks,old_delays=delays,old_pumps=input_pumps;
    for(unsigned direction=0;direction<4;direction++)for(unsigned p=0;p<6;p++)for(unsigned solid=0;solid<2;solid++) {
        e->units[0]=(fd2_field_unit){.direction=(uint8_t)direction,.frame_phase=(uint8_t)phases[p],.flags=0x81};
        e->units[0].detail_status[1]=1;g->units.items[0]=e->units[0];
        int64_t start=30168+(int64_t)phases[p]*steps[direction];
        memcpy(expected,before,FD2_FIELD_PAGE_SIZE);int want=0;
        for(int y=0;y<24 && !want;y++)for(int k=0;k<3;k++) {
            int64_t address=start+(int64_t)y*456+columns[k];
            if(address<0 || address>=FD2_FIELD_PAGE_SIZE){want=-1;break;}
            expected[(size_t)address]=solid?200:colors[k];
        }
        CHECK(fd2_field_page_write(page,0,0,456,336,before,FD2_FIELD_PAGE_SIZE,456)==0);
        CHECK(map_physical_actor(g,e,NULL,0,(int)solid,0,page,0)==want);
        CHECK(fd2_field_page_read(page,0,0,456,336,actual,FD2_FIELD_PAGE_SIZE,456)==0 &&
            !memcmp(actual,expected,FD2_FIELD_PAGE_SIZE));
    }
    /* 三个不同原RUN色，使局部walk2与global0/1/2/3明确可区分；
     * 3映1，raw4=0则取idle2。没有C decoder或renderer生成expected。 */
    uint8_t colored[3*48];static const uint8_t frame_colors[3]={0,37,84};
    for(unsigned f=0;f<3;f++)for(unsigned y=0;y<24;y++) {
        colored[f*48+y*2]=23;colored[f*48+y*2+1]=frame_colors[f];
    }
    for(unsigned f=0;f<12;f++)offsets[f]=(f%3u)*48u;
    g->sprites.data=colored;g->sprites.size=sizeof(colored);
    g->units.walk_frames[0]=2;g->idle_phase=2;
    e->units[0].direction=3;
    for(unsigned request=0;request<5;request++) {
        unsigned cursor=request<4?request:0;
        e->units[0].frame_phase=(uint8_t)(request<4?1:0);
        unsigned selected=request==4?2:cursor==3?1:cursor;
        int offset=30168+(request<4?4:0);
        memcpy(expected,before,FD2_FIELD_PAGE_SIZE);
        for(int y=0;y<24;y++)memset(expected+(size_t)(offset+y*456),frame_colors[selected],24);
        CHECK(fd2_field_page_write(page,0,0,456,336,before,FD2_FIELD_PAGE_SIZE,456)==0);
        CHECK(map_physical_actor(g,e,NULL,0,0,0,page,cursor)==0);
        CHECK(fd2_field_page_read(page,0,0,456,336,actual,FD2_FIELD_PAGE_SIZE,456)==0 &&
            !memcmp(actual,expected,FD2_FIELD_PAGE_SIZE));
    }
    CHECK(frames==old_frames && ticks==old_ticks && delays==old_delays &&
        input_pumps==old_pumps && !business_rng_calls);
    fd2_field_page_destroy(page);free(g);free(e);free(before);free(expected);free(actual);free(decoded);return 0;
}

/* C11公共返回合同：真实pending quit仅在该族phase翻译+1；skip仍0。
 * 复用同一真实exchange与正常source CAST前缀，不伪造cue/资源状态。 */
static int test_c11_public_host_and_skip_boundaries(void) {
    static const fd2_field_magic_present_phase phases[]={
        FD2_FIELD_MAGIC_PRESENT_MAP_WRAPPER_PRELUDE,
        FD2_FIELD_MAGIC_PRESENT_CAST,FD2_FIELD_MAGIC_PRESENT_TARGET};
    uint8_t targets[]={1,2};
    for(uint8_t id=10;id<=12;id++)for(unsigned phase=0;phase<3;phase++) {
        if(id==10 && phase==0)continue;
        for(unsigned skip=0;skip<2;skip++) {
            fd2_battle_animation ctx;CHECK(setup(&ctx)==0);
            game.units.count=3;actor(&game.units.items[0],9,2,1);
            actor(&game.units.items[2],96,0,2);
            fd2_field_magic_present_event event=magic_event(id,targets,2);
            event.presentation_mode=FD2_FIELD_MAGIC_MODE_MAP;event.effect_id=id;
            event.cast_cost_count=1;
            CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
            if(phase>0 && id!=10) {
                event.phase=FD2_FIELD_MAGIC_PRESENT_MAP_WRAPPER_PRELUDE;
                CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
            }
            if(phase==2) {
                event.phase=FD2_FIELD_MAGIC_PRESENT_CAST;
                CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==0);
            }
            fd2_field_game *prefix=malloc(sizeof(game));CHECK(prefix);memcpy(prefix,&game,sizeof(game));
            unsigned old_frames=frames,old_cues=cues,old_pumps=input_pumps;
            if(skip)fd2_battle_animation_set_skip(&ctx,1);else screen.input.quit_requested=1;
            fd2_field_magic_basic_result result={0};result.hit=1;result.raw_damage=7;
            result.hp_before=42;result.hp_after=35;
            event.phase=phases[phase];
            if(phase==2) {
                event.target_index=1;event.target_before=&game.units.items[1];
                event.target_after=&game.units.items[1];event.result=&result;
            }
            CHECK(fd2_battle_animation_magic_present(&game,&event,&ctx)==(skip?0:1));
            CHECK(!!ctx.host_quit==!skip && !ctx.failed && !business_rng_calls &&
                frames==old_frames && cues==old_cues && input_pumps==old_pumps &&
                !memcmp(prefix,&game,sizeof(game)));
            CHECK(magic_finish(&ctx,&event,1)==0 && !ctx.magic_exchange && !ctx.failed);
            fd2_battle_animation_close(&ctx);free(prefix);
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    if(argc>1 && !strcmp(argv[1],"--f4-composite-history"))return argc==3?f4_composite_history_cli(argv[2]):1;
    if(argc>1 && !strcmp(argv[1],"--f4-battle-history"))return argc==3?f4_battle_history_cli(argv[2]):1;
    if(argc>1 && !strcmp(argv[1],"--f4-map-history"))return argc==3?f4_map_history_cli(argv[2]):1;
    if(argc>1 && !strcmp(argv[1],"--f4-promotion-composition"))return argc==3?f4_promotion_cli(argv[2]):1;
    if(argc>1 && !strcmp(argv[1],"--f4-magic-work"))return argc==3?f4_mwork_cli(argv[2]):1;
    if(argc>1 && !strcmp(argv[1],"--f4-physical-composition"))return argc==3?f4_phys_cli(argv[2]):1;
    if(argc>1 && !strcmp(argv[1],"--f4-readers"))return argc==3?f4_reader_cli(argv[2]):1;
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
    CHECK(test_finite_frame_foreign_deadline()==0);
    CHECK(test_battle_summary_raw_predicate()==0);
    CHECK(test_mask_and_clipping()==0);
    CHECK(test_effect_page_stamp()==0);
    CHECK(test_prepare_atomic_and_skip()==0);
    CHECK(test_scene_source_contract()==0);
    CHECK(test_real_scene_and_quit()==0);
    CHECK(test_split_reverse_and_failure()==0);
    CHECK(test_marker_layers_idle_and_miss()==0);
    CHECK(test_sfx_option_gate()==0);
    CHECK(test_exact_dac_intro_and_fade()==0);
    CHECK(test_player_map_caller_preamble()==0);
    CHECK(test_ai_ray_item_single_present()==0);
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
    CHECK(test_map_magic_families()==0);
    CHECK(test_map_solid_source_colors()==0);
    CHECK(test_derived_magic_parts()==0);
    CHECK(test_composite_magic_passes()==0);
    CHECK(test_composite_map_return_boundary()==0);
    CHECK(test_full_return_clear_boundary()==0);
    CHECK(test_teleport_write_boundaries()==0);
    CHECK(test_new_magic_failure_quit_and_cache()==0);
    CHECK(test_item_map_presentations()==0);
    CHECK(test_map_app_page_lifetime()==0);
    CHECK(test_item_popup_and_effect_count_contract()==0);
    map_captured=malloc(128u*64000u);CHECK(map_captured);
    CHECK(test_c11_public_host_and_skip_boundaries()==0);
    CHECK(test_quake_wrapper_cue_and_page_prefix()==0);
    CHECK(test_radial_quake_wrapper_source_protocol()==0);
    CHECK(test_map_physical_linear_app_adapter()==0);
    CHECK(test_map_physical_preflight_and_actor_projection()==0);
    CHECK(test_short_panels_counter_and_layout()==0);
    CHECK(test_short_vga_bar_source_owner()==0);
    CHECK(test_short_vga_bar_adapter()==0);
    CHECK(test_short_counter_raw_bar_owner()==0);
    CHECK(test_app_page_short_counter_adapter()==0);
    CHECK(test_app_page_normal_return_adapter()==0);
    CHECK(test_raw5_recovery_and_short_preflight()==0);
    CHECK(test_short_six_weapon_families_and_miss()==0);
    CHECK(test_recovery_steps_zero_max_skip_and_quit()==0);
    CHECK(test_recovery_caller_info_context()==0);
    CHECK(test_prepare_promotion_scope_and_sequence()==0);
    CHECK(test_prepare_promotion_duration_and_safe_parameters()==0);
    CHECK(test_prepare_promotion_empty_idle_only_omits_sprite()==0);
    free(map_captured);map_captured=NULL;
    fd2_field_visuals_close(&game.visuals);fd2_map_sprite_bank_close(&game.sprites);
    fd2_text_entry_close(&game.ui_text);fd2_field_info_assets_close(&game.info_assets);
    CHECK(!business_rng_calls);
    CHECK(figani_fixture.data && figani_fixture.count==408); /* 服务未关闭借用的Game资源。 */
    fd2_archive_close(&figani_fixture);fd2_archive_close(&fdtxt);fd2_archive_close(&fdother);
    puts("battle_animation: headless groups PASS; no device/window/RNG/effect");return 0;
}

/* 正式视觉接口使用NS；本测试仍沿原毫秒调度器。 */
uint64_t battle_test_ticks_ns(void) { return battle_test_ticks()*UINT64_C(1000000); }

#include "vga_owner_wait_test_bridge.h"
