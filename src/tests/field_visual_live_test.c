/* 1297d/11eee/127e0/4e31c的live读钟合同；仅替换SDL时间输入。
 * 窗口、原码、设备均不执行。按独立PIT边界逐读给值，观察写锚之前的写集。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "vga.h"
#include "bios_clock.h"
#include "field_page_private.h"

#define CHECK(x) do {if(!(x)){fprintf(stderr,"field_visual_live:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static fd2_vga live_output_owner; /* 独立无设备的输出owner，不改变原数值golden。 */
static uint64_t clock_samples[8];
static size_t clock_count,clock_at;
static void (*observe_read)(size_t);
static Uint64 live_test_clock(void) {
    CHECK(clock_at<clock_count);
    if(observe_read)observe_read(clock_at);
    return clock_samples[clock_at++];
}

/* TU私有副本保完整实现及真实依赖，避免与core导出重符。 */
#define SDL_GetTicksNS live_test_clock
#define fd2_field_auxiliary_marker_color private_auxiliary_marker_color
#define fd2_field_visual_actor_step private_visual_actor_step
#define fd2_field_visual_actor_step_live private_visual_actor_step_live
#define fd2_field_visual_tiles_step private_visual_tiles_step
#define fd2_field_visual_tiles_step_live private_visual_tiles_step_live
#define fd2_field_visual_terrain_step_live private_visual_terrain_step_live
#define fd2_field_visual_range_step_live private_visual_range_step_live
#define fd2_field_visual_terrain_step private_visual_terrain_step
#define fd2_field_visual_range_step private_visual_range_step
#define fd2_field_transition_page_before_actors_from private_transition_page_before
#define fd2_field_transition_page_after_actors_from private_transition_page_after
#define fd2_field_transition_page_circle12 private_transition_page_circle
#define fd2_field_visuals_open private_visuals_open
#define fd2_field_visuals_close private_visuals_close
#define fd2_field_cursor_frame private_cursor_frame
#define fd2_field_range_lut private_range_lut
#define fd2_field_lut_frame private_lut_frame
#define fd2_field_apply_palette_lut private_apply_palette_lut
#define fd2_field_transition_lut_before_actors private_transition_lut_before
#define fd2_field_transition_lut_after_actors private_transition_lut_after
#define fd2_field_transition_lut_before_actors_from private_transition_lut_before_from
#define fd2_field_transition_lut_after_actors_from private_transition_lut_after_from
#define fd2_field_transition_lut_circle12 private_transition_lut_circle
#define fd2_field_actor_visual_tick private_actor_visual_tick
#define fd2_field_actor_visual_tick_live private_actor_visual_tick_live
#define fd2_field_water_palette_apply private_water_palette_apply
#define fd2_field_water_tick private_water_tick
#define fd2_field_water_tick_live private_water_tick_live
#define fd2_field_background_rebuild private_background_rebuild
#define fd2_field_background_scroll private_background_scroll
#define fd2_field_background_draw private_background_draw
#define fd2_field_background_draw_from private_background_draw_from
#define fd2_field_background_draw_live private_background_draw_live
#define fd2_field_background_draw_from_live private_background_draw_from_live
#include "../field_visual.c"
#include "../field_background.c"

/* ceil(tick*65536*1e9/1193182)，本文件最大tick65538，不借生产转换造输入。 */
static uint64_t tick_ns(uint64_t tick) {
    return (tick*65536u*UINT64_C(1000000000)+1193181u)/1193182u;
}
static void set_clock(const uint32_t *ticks,size_t count) {
    CHECK(count<=sizeof(clock_samples)/sizeof(clock_samples[0]));
    fd2_bios_clock_reset(0,0);clock_count=count;clock_at=0;observe_read=NULL;
    for(size_t i=0;i<count;i++)clock_samples[i]=tick_ns(ticks[i]);
}
#define CLOCK(...) do {const uint32_t samples_[]={__VA_ARGS__};set_clock(samples_,sizeof(samples_)/sizeof(samples_[0]));}while(0)
static fd2_field_visual_state visual;
static fd2_vga vga,vga_before;
static fd2_field_page owner,target;
static uint8_t background_source[64000],background_work[64000];

static void actor_before_anchor(size_t read) {
    if(read==1)CHECK(visual.actor_idle_phase==1 && !visual.actor_idle_tick && !visual.cursor_phase);
}
static void actor_before_wrap_anchor(size_t read) {
    if(read==2)CHECK(visual.actor_idle_phase==1 && visual.actor_idle_tick==32767u && !visual.cursor_phase);
}
static void test_actor_reads(void) {
    memset(&visual,0,sizeof(visual));CLOCK(5,6);observe_read=actor_before_anchor;
    CHECK(fd2_field_visual_actor_step_live(&live_output_owner,&visual)==1 && clock_at==2);
    CHECK(visual.actor_idle_phase==1 && visual.actor_idle_tick==6 && visual.cursor_phase==1);
    CLOCK(10,10);CHECK(fd2_field_visual_actor_step_live(&live_output_owner,&visual)==1 && clock_at==2);
    CHECK(visual.actor_idle_phase==1 && visual.actor_idle_tick==6 && visual.cursor_phase==2);
    /* 首>4不成立时第二读只检查<0；不能改成第二读也检查>4。 */
    memset(&visual,0,sizeof(visual));CLOCK(0,5);
    CHECK(fd2_field_visual_actor_step_live(&live_output_owner,&visual)==1 && clock_at==2);
    CHECK(!visual.actor_idle_phase && !visual.actor_idle_tick && visual.cursor_phase==1);
    visual.actor_idle_tick=32767;visual.cursor_phase=0;CLOCK(32767,32768,32769);
    observe_read=actor_before_wrap_anchor;
    CHECK(fd2_field_visual_actor_step_live(&live_output_owner,&visual)==1 && clock_at==3);
    CHECK(visual.actor_idle_tick==0xffff8001u && visual.actor_idle_phase==1 && visual.cursor_phase==1);
    memset(&visual,0,sizeof(visual));CLOCK(500,501);
    CHECK(fd2_field_visual_actor_step_live(&live_output_owner,&visual)==1 && visual.actor_idle_phase==1 && visual.actor_idle_tick==501);
}

static void terrain_before_anchor(size_t read) {
    if(read==1)CHECK(visual.terrain_phase==1 && visual.terrain_tick==9 && visual.range_phase==19);
}
static void range_before_anchor(size_t read) {
    if(read==2)CHECK(visual.range_phase==1 && !visual.range_tick);
}
static void test_tiles_reads(void) {
    memset(&visual,0,sizeof(visual));visual.terrain_tick=9;visual.range_phase=19;
    CLOCK(10,11);observe_read=terrain_before_anchor;
    CHECK(fd2_field_visual_tiles_step_live(&live_output_owner,&visual,7)==1 && clock_at==2);
    CHECK(visual.terrain_tick==11 && visual.terrain_phase==1 && visual.range_phase==7 && !visual.range_tick);
    memset(&visual,0,sizeof(visual));visual.terrain_tick=3;CLOCK(3,3,4);observe_read=range_before_anchor;
    CHECK(fd2_field_visual_tiles_step_live(&live_output_owner,&visual,-1)==1 && clock_at==3);
    CHECK(visual.range_phase==1 && visual.range_tick==4);
    visual.terrain_tick=6;CLOCK(6,6,6);
    CHECK(fd2_field_visual_tiles_step_live(&live_output_owner,&visual,-1)==0 && clock_at==3);
    CHECK(visual.range_phase==1 && visual.range_tick==4);
    visual.terrain_tick=10;visual.range_tick=32767;visual.range_phase=19;
    CLOCK(10,32767,32768,32769);
    CHECK(fd2_field_visual_tiles_step_live(&live_output_owner,&visual,-1)==1 && clock_at==4);
    CHECK(!visual.range_phase && visual.range_tick==0xffff8001u);
    /* 分离叶供真实11eee夹入背景，不读写另一门或actor/cursor owner。 */
    memset(&visual,0,sizeof(visual));visual.terrain_tick=9;visual.range_tick=30;
    CLOCK(10,11);CHECK(fd2_field_visual_terrain_step_live(&live_output_owner,&visual)==1 && clock_at==2);
    CHECK(visual.terrain_tick==11 && !visual.range_phase && visual.range_tick==30 && !visual.cursor_phase);
    CLOCK(33,34);CHECK(fd2_field_visual_range_step_live(&live_output_owner,&visual,-1)==1 && clock_at==2);
    CHECK(visual.range_tick==34 && visual.terrain_tick==11 && visual.terrain_phase==1 && !visual.cursor_phase);
    set_clock(NULL,0);CHECK(fd2_field_visual_range_step_live(&live_output_owner,&visual,7)==1 && !clock_at);
    CHECK(visual.range_phase==7 && visual.range_tick==34);
}

static void jitter_before_anchor(size_t read) {
    if(read==1)CHECK(vga.actor_jitter_phase==1 && vga.actor_jitter_tick==9);
}
static void test_jitter_reads(void) {
    memset(&vga,0,sizeof(vga));vga.actor_jitter_tick=9;CLOCK(10,11);observe_read=jitter_before_anchor;
    CHECK(fd2_field_actor_visual_tick_live(&vga)==1 && clock_at==2);
    CHECK(vga.actor_jitter_tick==11 && vga.actor_jitter_phase==1);
    CLOCK(11);CHECK(fd2_field_actor_visual_tick_live(&vga)==0 && clock_at==1);
    vga.actor_jitter_tick=65535;CLOCK(65536,65537);
    CHECK(fd2_field_actor_visual_tick_live(&vga)==1 && !vga.actor_jitter_phase && vga.actor_jitter_tick==1);
}

static void water_before_anchor(size_t read) {
    if(read!=1)return;
    CHECK(vga.field_water_phase==1 && vga.field_water_last_tick==8);
    /* 原RGB6 cycle1首项13/20/37；整个48分量已写，范围外保持。 */
    CHECK(vga.dac[224u*3u]==52 && vga.dac[224u*3u+1u]==81 && vga.dac[224u*3u+2u]==150);
    for(size_t i=0;i<sizeof(vga.dac);i++)
        CHECK(i>=224u*3u && i<240u*3u ? vga.dac[i]!=0x77 : vga.dac[i]==0x77);
}
static void test_water_reads(void) {
    memset(&vga,0,sizeof(vga));memset(vga.dac,0x77,sizeof(vga.dac));vga.field_water_last_tick=8;
    memset(vga.framebuffer,0x5a,sizeof(vga.framebuffer));memset(vga.palette,0x11,sizeof(vga.palette));
    vga_before=vga;
    CLOCK(10,11);observe_read=water_before_anchor;
    CHECK(fd2_field_water_tick_live(&vga)==1 && clock_at==2 && vga.field_water_last_tick==11);
    vga_before.field_water_phase=1;vga_before.field_water_last_tick=11;
    memcpy(vga_before.dac+224u*3u,vga.dac+224u*3u,48u);
    CHECK(!memcmp(&vga,&vga_before,sizeof(vga))); /* 完整owner仅允许这份写集。 */
    vga_before=vga;CLOCK(12);CHECK(fd2_field_water_tick_live(&vga)==0 && clock_at==1);
    CHECK(!memcmp(&vga,&vga_before,sizeof(vga)));
    vga.field_water_phase=15;CLOCK(18181,18182);
    CHECK(fd2_field_water_tick_live(&vga)==1 && !vga.field_water_phase && vga.field_water_last_tick==18182);
    vga.field_water_last_tick=65535;CLOCK(65536);
    CHECK(fd2_field_water_tick_live(&vga)==0 && clock_at==1);
    CLOCK(65537,65538);CHECK(fd2_field_water_tick_live(&vga)==1 && vga.field_water_last_tick==2 && clock_at==2);
}

static void test_fixed_and_rejected_paths(void) {
    set_clock(NULL,0);memset(&visual,0,sizeof(visual));
    CHECK(fd2_field_visual_actor_step(&visual,tick_ns(5))==1 && visual.actor_idle_tick==5);
    CHECK(fd2_field_visual_tiles_step(&visual,tick_ns(5),-1)==1 && visual.terrain_tick==5 && visual.range_tick==5);
    memset(&vga,0,sizeof(vga));CHECK(fd2_field_actor_visual_tick(&vga,tick_ns(5))==1);
    CHECK(fd2_field_water_tick(&vga,tick_ns(5))==1 && vga.field_water_last_tick==5);
    CHECK(!clock_at); /* 固定now向量不从真实时钟偷取第二个值。 */
    visual.actor_idle_phase=4;fd2_field_visual_state old=visual;
    CHECK(fd2_field_visual_actor_step_live(&live_output_owner,&visual)==-1 && !memcmp(&old,&visual,sizeof(old)));
    visual.actor_idle_phase=0;visual.range_phase=20;old=visual;
    CHECK(fd2_field_visual_tiles_step_live(&live_output_owner,&visual,-1)==-1 && !memcmp(&old,&visual,sizeof(old)));
    visual.range_phase=0;old=visual;
    CHECK(fd2_field_visual_tiles_step_live(&live_output_owner,&visual,20)==-1 && !memcmp(&old,&visual,sizeof(old)));
    CHECK(fd2_field_visual_actor_step_live(&live_output_owner,NULL)==-1 && fd2_field_visual_tiles_step_live(&live_output_owner,NULL,-1)==-1);
    CHECK(fd2_field_visual_terrain_step_live(&live_output_owner,NULL)==-1 && fd2_field_visual_range_step_live(&live_output_owner,NULL,-1)==-1);
    vga.actor_jitter_phase=2;vga_before=vga;
    CHECK(fd2_field_actor_visual_tick_live(&vga)==-1 && !memcmp(&vga,&vga_before,sizeof(vga)));
    vga.field_water_phase=16;vga_before=vga;
    CHECK(fd2_field_water_tick_live(&vga)==-1 && !memcmp(&vga,&vga_before,sizeof(vga)));
    vga.field_water_phase=0;vga.input.quit_requested=1;vga_before=vga;
    CHECK(fd2_field_water_tick_live(&vga)==0 && !memcmp(&vga,&vga_before,sizeof(vga)));
    CHECK(fd2_field_actor_visual_tick_live(NULL)==-1 && fd2_field_water_tick_live(NULL)==-1 && !clock_at);
}

static void setup_background(unsigned kind) {
    memset(&owner,0,sizeof(owner));memset(&target,0,sizeof(target));
    memset(target.pixels,0xa5,sizeof(target.pixels));
    for(size_t row=0;row<200;row++)for(size_t x=0;x<320;x++)
        background_source[row*320+x]=(uint8_t)(row+x);
    memset(background_work,0xcc,sizeof(background_work));
    owner.background.source=background_source;owner.background.source_size=sizeof(background_source);
    owner.background.work=background_work;owner.background.work_size=sizeof(background_work);
    owner.background.kind=(uint8_t)kind;owner.background.width=kind==BACKGROUND_WAVE?320:312;
    owner.background.height=kind==BACKGROUND_WAVE?200:192;
    owner.background.scroll_speed=1;owner.background.last_tick=8;
}
static void wave_before_anchor(size_t read) {
    if(read!=1)return;
    static const unsigned wave[16]={2,3,3,4,4,4,3,3,2,1,1,0,0,0,1,1};
    CHECK(!owner.background.phase && owner.background.last_tick==8);
    for(size_t y=0;y<192;y++)for(size_t x=0;x<312;x++)
        CHECK(background_work[y*320+x]==background_source[y*320+x+wave[y&15u]]);
    CHECK(target.pixels[72u*456u+72u]==0xa5); /* 还没投影目的页。 */
}
static void scroll_before_anchor(size_t read) {
    if(read!=1)return;
    CHECK(owner.background.last_tick==8);
    for(size_t row=0;row<192;row++)for(size_t x=0;x<312;x++)
        CHECK(background_source[row*312+x]==(uint8_t)(row?row-1u:191u));
}
static void test_background_reads(void) {
    setup_background(BACKGROUND_WAVE);CLOCK(10,11);observe_read=wave_before_anchor;
    CHECK(fd2_field_background_draw_from_live(&live_output_owner,&owner,&target,72,72,0,0,0,0,1)==0 && clock_at==2);
    CHECK(owner.background.last_tick==11 && owner.background.phase==1 && !target.background.kind);
    CHECK(target.pixels[72u*456u+72u]==background_source[2]);
    CLOCK(11);CHECK(fd2_field_background_draw_from_live(&live_output_owner,&owner,&target,72,72,0,0,0,0,1)==0 && clock_at==1);
    CHECK(owner.background.last_tick==11 && owner.background.phase==1);
    setup_background(BACKGROUND_WAVE);owner.background.last_tick=32767;
    CLOCK(32768,32769);
    CHECK(fd2_field_background_draw_from_live(&live_output_owner,&owner,&target,72,72,0,0,0,0,1)==0 && clock_at==2);
    CHECK(owner.background.last_tick==0xffff8001u && owner.background.phase==1);
    setup_background(BACKGROUND_WAVE);CLOCK(18181,18182);
    CHECK(fd2_field_background_draw_live(&live_output_owner,&owner,72,72,0,0,0,0,1)==0 && owner.background.phase==1 && clock_at==2);
    setup_background(BACKGROUND_SCROLL);
    for(size_t row=0;row<192;row++)memset(background_source+row*312,(int)row,312);
    CLOCK(10,11);observe_read=scroll_before_anchor;
    CHECK(fd2_field_background_draw_from_live(&live_output_owner,&owner,&target,72,72,0,0,0,0,1)==0 && clock_at==2);
    CHECK(owner.background.last_tick==11 && target.pixels[72u*456u+72u]==191);
    CLOCK(11);CHECK(fd2_field_background_draw_live(&live_output_owner,&owner,72,72,0,0,0,0,1)==0 && clock_at==1);
    CHECK(owner.background.last_tick==11 && background_source[0]==191);
}
static void test_background_no_read_and_failure(void) {
    setup_background(BACKGROUND_WAVE);set_clock(NULL,0);
    CHECK(fd2_field_background_draw_from_live(&live_output_owner,&owner,&target,72,72,0,0,0,0,0)==0 && !clock_at);
    CHECK(fd2_field_background_draw(&owner,72,72,0,0,0,0,tick_ns(10),1)==0 && !clock_at);
    CHECK(owner.background.last_tick==10 && owner.background.phase==1);
    owner.background.work=NULL;fd2_field_background_state old=owner.background;
    CHECK(fd2_field_background_draw_live(&live_output_owner,&owner,72,72,0,0,0,0,1)==-1 && !memcmp(&old,&owner.background,sizeof(old)));
    CHECK(fd2_field_background_draw_from_live(&live_output_owner,NULL,&target,72,72,0,0,0,0,1)==-1);
    setup_background(BACKGROUND_NONE);CHECK(fd2_field_background_draw_live(&live_output_owner,&owner,72,72,0,0,0,0,1)==0 && !clock_at);
    setup_background(BACKGROUND_PARALLAX);CHECK(fd2_field_background_draw_live(&live_output_owner,&owner,72,72,0,0,0,0,1)==0 && !clock_at);
    CHECK(fd2_field_background_draw_live(&live_output_owner,&owner,-1,72,0,0,0,0,1)==-1 && !clock_at);
    setup_background(BACKGROUND_SCROLL);owner.background.scroll_speed=193;old=owner.background;CLOCK(10);
    CHECK(fd2_field_background_draw_live(&live_output_owner,&owner,72,72,0,0,0,0,1)==-1 && clock_at==1);
    CHECK(!memcmp(&old,&owner.background,sizeof(old)));
}

int main(void) {
    test_actor_reads();test_tiles_reads();test_jitter_reads();test_water_reads();
    test_fixed_and_rejected_paths();test_background_reads();test_background_no_read_and_failure();
    puts("field_visual_live: exact read order, short circuit, post-write anchors, wraps, fixed APIs and failures passed; no GUI/original");
    return 0;
}
