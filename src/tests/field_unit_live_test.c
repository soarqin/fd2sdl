/* sub_127A9 @0x127a9 / sub_127E0 @0x127e0（原登记同名）：
 * 独立有序tick锁定hidden外门、camera/raw26之前的逐actor读钟及绘制前缀。
 * 私有TU保真实unit/visual实现，只替换SDL clock；不启动窗口或原版实验。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "vga.h"
#include "bios_clock.h"
#include "field_page_private.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr,"field_unit_live:%d: %s\n",__LINE__,#e); exit(1); \
} } while (0)

static uint64_t unit_clock_samples[32];
static size_t unit_clock_count,unit_clock_at;
static void (*unit_observe_clock)(size_t);
static Uint64 unit_test_clock(void) {
    CHECK(unit_clock_at<unit_clock_count);
    if (unit_observe_clock) unit_observe_clock(unit_clock_at);
    return unit_clock_samples[unit_clock_at++];
}

/* 私有副本不与core导出重符，也不向生产加入测试hook。 */
#define SDL_GetTicksNS unit_test_clock
#define fd2_field_units_render_terrain_overlays private_units_render_terrain_overlays
#define fd2_field_units_clear private_units_clear
#define fd2_field_units_sprite_cache_append private_units_sprite_cache_append
#define fd2_field_units_sprite_cache_restore private_units_sprite_cache_restore
#define fd2_field_unit_init private_unit_init
#define fd2_field_units_set private_units_set
#define fd2_field_unit_template_count private_unit_template_count
#define fd2_field_unit_deployment_position private_unit_deployment_position
#define fd2_field_units_append_template private_units_append_template
#define fd2_field_units_append_group private_units_append_group
#define fd2_field_units_project_group_identity private_units_project_group_identity
#define fd2_field_units_append_group_placed private_units_append_group_placed
#define fd2_field_unit_is_hidden private_unit_is_hidden
#define fd2_field_unit_set_hidden private_unit_set_hidden
#define fd2_field_unit_has_acted private_unit_has_acted
#define fd2_field_unit_set_acted private_unit_set_acted
#define fd2_field_units_find_visible_text_id private_units_find_visible_text_id
#define fd2_field_units_field_write private_units_field_write
#define fd2_field_units_ai_behavior_write private_units_ai_behavior_write
#define fd2_field_unit_frame_index private_unit_frame_index
#define fd2_field_unit_cached_sprite_id private_unit_cached_sprite_id
#define fd2_field_unit_cached_frame_index private_unit_cached_frame_index
#define fd2_field_unit_render private_unit_render
#define fd2_field_units_render_actor private_units_render_actor
#define fd2_field_units_render_page_actor private_units_render_page_actor
#define fd2_field_map_action_page_actor private_map_action_page_actor
#define fd2_field_fixed_page_actor private_fixed_page_actor
#define fd2_field_flash_page_actor private_flash_page_actor
#define fd2_field_units_render_page private_units_render_page
#define fd2_field_units_render_page_actor_live private_units_render_page_actor_live
#define fd2_field_units_render_page_live private_units_render_page_live
#define fd2_field_units_render_page_terrain_overlays private_units_render_page_terrain_overlays
#define fd2_field_units_render private_units_render
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
#include "../field_unit.c"
#include "../field_visual.c"

static fd2_vga unit_vga,unit_vga_before,unit_vga_compare;
static fd2_field_page unit_page_before;
static fd2_field_page *unit_page;
static fd2_field_units unit_records,unit_records_before;
static uint8_t unit_rle[24*48],unit_rle_before[24*48];
static uint32_t unit_offsets[24],unit_offsets_before[24];
static fd2_map_sprite_bank unit_bank,unit_bank_before;

/* ceil(tick*65536*1e9/1193182)，不借生产转换生成边界。 */
static uint64_t unit_tick_ns(uint32_t tick) {
    return ((uint64_t)tick*65536u*UINT64_C(1000000000)+1193181u)/1193182u;
}
static void unit_set_clock(const uint32_t *ticks,size_t count) {
    CHECK(count<=sizeof(unit_clock_samples)/sizeof(unit_clock_samples[0]));
    fd2_bios_clock_reset(0,0);
    unit_clock_at=0;unit_clock_count=count;unit_observe_clock=NULL;
    for (size_t i=0;i<count;i++) unit_clock_samples[i]=unit_tick_ns(ticks[i]);
}
#define CLOCK(...) do { const uint32_t ticks_[]={__VA_ARGS__}; \
    unit_set_clock(ticks_,sizeof(ticks_)/sizeof(ticks_[0])); } while (0)

static void unit_fixture(void) {
    memset(&unit_vga,0,sizeof(unit_vga));
    memset(unit_vga.framebuffer,0x5a,sizeof(unit_vga.framebuffer));
    memset(unit_vga.palette,0x6b,sizeof(unit_vga.palette));
    memset(unit_vga.dac,0x7c,sizeof(unit_vga.dac));
    unit_vga.actor_jitter_tick=0;unit_vga.actor_jitter_phase=0;
    memset(&unit_records,0,sizeof(unit_records));
    memset(&unit_bank,0,sizeof(unit_bank));
    for (size_t f=0;f<24;f++) {
        unit_offsets[f]=(uint32_t)(f*48);
        for (size_t row=0;row<24;row++) {
            unit_rle[f*48+row*2]=23;
            unit_rle[f*48+row*2+1]=(uint8_t)(f+1);
        }
    }
    unit_bank.data=unit_rle;unit_bank.size=sizeof(unit_rle);
    unit_bank.offsets=unit_offsets;unit_bank.frame_count=24;
    memset(unit_page->pixels,0,sizeof(unit_page->pixels));
    unit_set_clock(NULL,0);
}
static void unit_snapshot(void) {
    memcpy(&unit_vga_before,&unit_vga,sizeof(unit_vga));
    memcpy(&unit_records_before,&unit_records,sizeof(unit_records));
    memcpy(&unit_bank_before,&unit_bank,sizeof(unit_bank));
    memcpy(&unit_page_before,unit_page,sizeof(*unit_page));
    memcpy(unit_rle_before,unit_rle,sizeof(unit_rle));
    memcpy(unit_offsets_before,unit_offsets,sizeof(unit_offsets));
}
static uint8_t unit_pixel(int x,int y) {
    uint8_t pixel;
    CHECK(fd2_field_page_read(unit_page,x,y,1,1,&pixel,1,1)==0);
    return pixel;
}
static void unit_check_owner_write_set(void) {
    memcpy(&unit_vga_compare,&unit_vga,sizeof(unit_vga));
    unit_vga_compare.actor_jitter_tick=unit_vga_before.actor_jitter_tick;
    unit_vga_compare.actor_jitter_phase=unit_vga_before.actor_jitter_phase;
    CHECK(!memcmp(&unit_vga_compare,&unit_vga_before,sizeof(unit_vga)));
    CHECK(!memcmp(&unit_records,&unit_records_before,sizeof(unit_records)));
    CHECK(!memcmp(&unit_bank,&unit_bank_before,sizeof(unit_bank)));
    CHECK(!memcmp(unit_rle,unit_rle_before,sizeof(unit_rle)));
    CHECK(!memcmp(unit_offsets,unit_offsets_before,sizeof(unit_offsets)));
}
static void unit_check_no_write(void) {
    unit_check_owner_write_set();
    CHECK(!memcmp(&unit_vga,&unit_vga_before,sizeof(unit_vga)));
    CHECK(!memcmp(unit_page,&unit_page_before,sizeof(*unit_page)));
}

/* 原读点127f6和1280d。每个后actor首读时，前actor必须已按当时phase画完。 */
static void unit_observe_batch(size_t read) {
    if (read==1) {
        CHECK(unit_vga.actor_jitter_phase==1 && unit_vga.actor_jitter_tick==0);
        CHECK(unit_pixel(121,114)==0); /* 先xor再重读锚，随后才画actor0。 */
    }
    if (read>=2) CHECK(unit_pixel(120,114)==0 && unit_pixel(121,114)==1);
    if (read>=4) CHECK(unit_pixel(215,114)==0 && unit_pixel(216,114)==1);
    if (read>=6) CHECK(unit_pixel(263,114)==0 && unit_pixel(264,114)==3);
    if (read==7) CHECK(unit_vga.actor_jitter_phase==0 && unit_vga.actor_jitter_tick==12);
    if (read>=10) CHECK(unit_pixel(312,114)==0 && unit_pixel(313,114)==1);
}
static void test_batch_order(void) {
    unit_fixture();unit_records.count=7;
    for (size_t i=0;i<unit_records.count;i++) {
        unit_records.items[i].x=(uint8_t)(2+i*2);
        unit_records.items[i].y=2;
        unit_records.items[i].detail_status[1]=1;
    }
    unit_records.items[1].flags=FD2_FIELD_UNIT_FLAG_HIDDEN;
    unit_records.items[3].detail_status[1]=0; /* 仍读钟，idle2不强制frame0。 */
    unit_records.items[4].x=unit_records.items[4].y=40; /* 相机外仍翻phase。 */
    unit_records.items[4].detail_status[1]=0;
    unit_records.items[5].x=10;unit_records.items[6].x=12;
    unit_snapshot();
    CLOCK(10,10,11,11,12,12,13,13,14,14,14);
    unit_observe_clock=unit_observe_batch;
    CHECK(fd2_field_units_render_page_live(&unit_vga,unit_page,&unit_bank,
        &unit_records,0,0,2,3)==0);
    CHECK(unit_clock_at==11 && unit_vga.actor_jitter_tick==14 &&
        unit_vga.actor_jitter_phase==1);
    CHECK(unit_pixel(168,114)==0); /* hidden没有画或读钟。 */
    CHECK(unit_pixel(360,114)==0 && unit_pixel(361,114)==1); /* 同tick不再翻。 */
    unit_check_owner_write_set();
}

static void test_empty_and_hidden(void) {
    unit_fixture();unit_vga.actor_jitter_tick=23;unit_vga.actor_jitter_phase=1;
    unit_snapshot();
    CHECK(fd2_field_units_render_page_live(&unit_vga,unit_page,&unit_bank,
        &unit_records,0,0,0,0)==0);
    CHECK(!unit_clock_at);unit_check_no_write();
    unit_records.count=3;
    for (size_t i=0;i<3;i++) {
        unit_records.items[i].flags=FD2_FIELD_UNIT_FLAG_HIDDEN;
        unit_records.items[i].unit_id=255;
    }
    unit_snapshot();
    CHECK(fd2_field_units_render_page_live(&unit_vga,unit_page,&unit_bank,
        &unit_records,0,0,0,0)==0);
    CHECK(!unit_clock_at);unit_check_no_write();
}

static void test_direct_read_gates(void) {
    unit_fixture();unit_records.count=1;
    unit_records.items[0].x=unit_records.items[0].y=2;
    unit_records.items[0].flags=FD2_FIELD_UNIT_FLAG_HIDDEN;
    unit_snapshot();CLOCK(9,9);
    CHECK(fd2_field_units_render_page_actor_live(&unit_vga,unit_page,&unit_bank,
        &unit_records,0,0,0,2,3,0)==0);
    CHECK(unit_clock_at==2 && unit_vga.actor_jitter_tick==9 && unit_vga.actor_jitter_phase==1);
    CHECK(!memcmp(unit_page,&unit_page_before,sizeof(*unit_page)));
    unit_check_owner_write_set();
    unit_records.items[0].flags=0;unit_records.items[0].x=unit_records.items[0].y=200;
    unit_snapshot();CLOCK(10,10);
    CHECK(fd2_field_units_render_page_actor_live(&unit_vga,unit_page,&unit_bank,
        &unit_records,0,0,0,2,3,0)==0);
    CHECK(unit_clock_at==2 && unit_vga.actor_jitter_tick==10 && !unit_vga.actor_jitter_phase);
    CHECK(!memcmp(unit_page,&unit_page_before,sizeof(*unit_page)));
    unit_check_owner_write_set();
    unit_records.items[0].x=unit_records.items[0].y=2;
    unit_snapshot();CLOCK(11,11);
    CHECK(fd2_field_units_render_page_actor_live(&unit_vga,unit_page,&unit_bank,
        &unit_records,0,0,0,2,3,-12)==0);
    CHECK(unit_clock_at==2 && unit_vga.actor_jitter_phase==1);
    CHECK(unit_pixel(120,102)==3 && unit_pixel(119,102)==0); /* raw26=0不抖。 */
    unit_check_owner_write_set();
}

static void test_error_prefixes(void) {
    unit_fixture();unit_records.count=2;
    unit_records.items[0].x=unit_records.items[0].y=2;
    unit_records.items[1].x=4;unit_records.items[1].y=2;
    unit_records.items[1].unit_id=1;
    unit_offsets[12]=(uint32_t)sizeof(unit_rle)+1u;
    unit_snapshot();
    CHECK(fd2_field_units_render_page_live(&unit_vga,unit_page,&unit_bank,
        &unit_records,0,0,0,0)==-1);
    CHECK(!unit_clock_at);unit_check_no_write(); /* 后项坏资源不留前项clock/像素。 */
    CLOCK(21,22);
    CHECK(fd2_field_units_render_page_actor_live(&unit_vga,unit_page,&unit_bank,
        &unit_records,1,0,0,0,0,0)==-1);
    CHECK(unit_clock_at==2 && unit_vga.actor_jitter_tick==22 && unit_vga.actor_jitter_phase==1);
    CHECK(!memcmp(unit_page,&unit_page_before,sizeof(*unit_page)));
    unit_check_owner_write_set(); /* direct资源错发生在原clock前缀之后。 */
}

static void test_structure_errors(void) {
    unit_fixture();unit_records.count=1;unit_snapshot();
    CHECK(fd2_field_units_render_page_live(NULL,unit_page,&unit_bank,&unit_records,0,0,0,0)==-1);
    CHECK(fd2_field_units_render_page_live(&unit_vga,NULL,&unit_bank,&unit_records,0,0,0,0)==-1);
    CHECK(fd2_field_units_render_page_live(&unit_vga,unit_page,NULL,&unit_records,0,0,0,0)==-1);
    CHECK(fd2_field_units_render_page_live(&unit_vga,unit_page,&unit_bank,NULL,0,0,0,0)==-1);
    CHECK(fd2_field_units_render_page_actor_live(&unit_vga,unit_page,&unit_bank,
        &unit_records,1,0,0,0,0,0)==-1);
    CHECK(fd2_field_units_render_page_actor_live(&unit_vga,unit_page,&unit_bank,
        &unit_records,0,0,0,4,0,0)==-1);
    CHECK(fd2_field_units_render_page_live(&unit_vga,unit_page,&unit_bank,&unit_records,0,0,0,4)==-1);
    CHECK(!unit_clock_at);unit_check_no_write();
    unit_records.count=FD2_FIELD_MAX_UNITS+1u;unit_snapshot();
    CHECK(fd2_field_units_render_page_live(&unit_vga,unit_page,&unit_bank,&unit_records,0,0,0,0)==-1);
    CHECK(!unit_clock_at);unit_check_no_write();
    unit_records.count=1;unit_vga.actor_jitter_phase=2;unit_snapshot();
    CHECK(fd2_field_units_render_page_live(&unit_vga,unit_page,&unit_bank,&unit_records,0,0,0,0)==-1);
    CHECK(fd2_field_units_render_page_actor_live(&unit_vga,unit_page,&unit_bank,
        &unit_records,0,0,0,0,0,0)==-1);
    CHECK(!unit_clock_at);unit_check_no_write();
}

int main(void) {
    unit_page=fd2_field_page_create();CHECK(unit_page);
    test_batch_order();test_empty_and_hidden();test_direct_read_gates();
    test_error_prefixes();test_structure_errors();
    fd2_field_page_destroy(unit_page);
    puts("field_unit_live: passed");
    return 0;
}
