#include <stdint.h>
/* 26152/29300/29bcb准备页：真实资源纯绘制及替身present的逐帧输入。
 * 不开窗口，不替代DOSBox/WGC验收；文件只在宿主临时路径写隔离容器。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_temp.h"
#include "field_prepare_play.h"
#include "field_prepare_view.h"
#include "field_visual.h"
#include "static_data.h"
#include "scene.h"

Uint64 SDLCALL prepare_test_ticks(void);
int prepare_test_present(fd2_vga *vga);
static void prepare_test_upload(fd2_vga *vga);
static int prepare_test_delay(uint32_t ms);
int prepare_test_present_timed(fd2_vga *vga, uint32_t delay);
int prepare_test_deadline(fd2_vga *vga);
static int prepare_test_wait_ticks(uint32_t ticks);
static void *prepare_test_calloc(size_t count, size_t size);
static int prepare_test_pcm_replace(fd2_pcm_player *player, const fd2_pcm_bank *bank,
    size_t sample, int loop_count, float gain);
static int prepare_test_bgm_option(fd2_bgm_player *, int, int);
static int prepare_test_bgm_play(fd2_bgm_player *, size_t, int);
static int prepare_test_bgm_stop(fd2_bgm_player *);
static int prepare_test_state_save(const char *path, size_t slot,
    const fd2_save_manual_state *state);
static int prepare_test_magic_draw(fd2_vga *vga, const fd2_field_info_assets *assets,
    const fd2_font *font, const fd2_text_entry *text,
    const fd2_field_unit *unit, int selected);
static int prepare_test_water_live(fd2_vga *vga);
static void prepare_test_clear_keys(fd2_input *input);
static void prepare_test_brightness(fd2_vga *vga,int value);
static int prepare_test_palette_fade(fd2_vga *vga,int fade_in,int fast);
static const fd2_static_flow *prepare_test_flow_lookup(const char *domain,const char *identity);
static uint32_t prepare_test_rng(void *userdata);
static void prepare_test_blit(fd2_vga *,const fd2_image *,int,int,int);
static int prepare_test_members(fd2_vga *,const fd2_field_prepare_assets *,
    const fd2_save_manual_state *,size_t,int,unsigned);

#define SDL_GetTicks prepare_test_ticks
uint64_t prepare_test_ticks_ns(void);
#define SDL_GetTicksNS prepare_test_ticks_ns
/* 与field_visual_live_test相同：保留完整视觉SUT，只替换clock输入。
 * core也链接这些API，故私有命名；consumer仅接私有actor live入口。 */
void prepare_live_visuals_close(fd2_field_visuals *visuals);
int prepare_live_transition_lut_before_from(fd2_vga *,const uint8_t [256],int,int,int,int);
int prepare_live_transition_lut_after_from(fd2_vga *,const uint8_t [256],int,int,int,int);
int prepare_live_transition_lut_circle(fd2_vga *,const uint8_t [256],int,int,int);
#define fd2_field_auxiliary_marker_color prepare_live_auxiliary_marker_color
#define fd2_field_visual_actor_step prepare_live_visual_actor_step
#define fd2_field_visual_actor_step_live prepare_live_visual_actor_step_live
#define fd2_field_visual_tiles_step prepare_live_visual_tiles_step
#define fd2_field_visual_tiles_step_live prepare_live_visual_tiles_step_live
#define fd2_field_visual_terrain_step prepare_live_visual_terrain_step
#define fd2_field_visual_terrain_step_live prepare_live_visual_terrain_step_live
#define fd2_field_visual_range_step prepare_live_visual_range_step
#define fd2_field_visual_range_step_live prepare_live_visual_range_step_live
#define fd2_field_transition_page_before_actors_from prepare_live_transition_page_before
#define fd2_field_transition_page_after_actors_from prepare_live_transition_page_after
#define fd2_field_transition_page_circle12 prepare_live_transition_page_circle
#define fd2_field_visuals_open prepare_live_visuals_open
#define fd2_field_visuals_close prepare_live_visuals_close
#define fd2_field_cursor_frame prepare_live_cursor_frame
#define fd2_field_lut_frame prepare_live_lut_frame
#define fd2_field_range_lut prepare_live_range_lut
#define fd2_field_apply_palette_lut prepare_live_apply_palette_lut
#define fd2_field_transition_lut_before_actors prepare_live_transition_lut_before
#define fd2_field_transition_lut_after_actors prepare_live_transition_lut_after
#define fd2_field_transition_lut_before_actors_from prepare_live_transition_lut_before_from
#define fd2_field_transition_lut_after_actors_from prepare_live_transition_lut_after_from
#define fd2_field_transition_lut_circle12 prepare_live_transition_lut_circle
#define fd2_field_actor_visual_tick prepare_live_actor_visual_tick
#define fd2_field_actor_visual_tick_live prepare_live_actor_visual_tick_live
#define fd2_field_water_palette_apply prepare_live_water_palette_apply
#define fd2_field_water_tick prepare_live_water_tick
#define fd2_field_water_tick_live prepare_live_water_tick_live
#include "../src/field_visual.c"
#undef fd2_field_auxiliary_marker_color
#undef fd2_field_visual_actor_step
#undef fd2_field_visual_tiles_step
#undef fd2_field_visual_tiles_step_live
#undef fd2_field_visual_terrain_step
#undef fd2_field_visual_terrain_step_live
#undef fd2_field_visual_range_step
#undef fd2_field_visual_range_step_live
#undef fd2_field_transition_page_before_actors_from
#undef fd2_field_transition_page_after_actors_from
#undef fd2_field_transition_page_circle12
#undef fd2_field_visuals_open
#undef fd2_field_visuals_close
#undef fd2_field_cursor_frame
#undef fd2_field_lut_frame
#undef fd2_field_range_lut
#undef fd2_field_apply_palette_lut
#undef fd2_field_transition_lut_before_actors
#undef fd2_field_transition_lut_after_actors
#undef fd2_field_transition_lut_before_actors_from
#undef fd2_field_transition_lut_after_actors_from
#undef fd2_field_transition_lut_circle12
#undef fd2_field_actor_visual_tick
#undef fd2_field_actor_visual_tick_live
#undef fd2_field_water_palette_apply
#undef fd2_field_water_tick
#undef fd2_field_water_tick_live
#define fd2_vga_present prepare_test_present
#define fd2_vga_upload_current_frame prepare_test_upload
#define fd2_delay_ms prepare_test_delay
#define fd2_vga_delay_ms(vga,value) ((void)(vga), prepare_test_delay(value))
#define fd2_vga_present_timed prepare_test_present_timed
#define fd2_vga_wait_frame_deadline prepare_test_deadline
#define fd2_wait_ticks prepare_test_wait_ticks
#define fd2_vga_wait_ticks(vga,value) ((void)(vga), prepare_test_wait_ticks(value))
#define calloc prepare_test_calloc
#define fd2_pcm_play_replace prepare_test_pcm_replace
#define fd2_bgm_set_music_enabled prepare_test_bgm_option
#define fd2_bgm_play prepare_test_bgm_play
#define fd2_bgm_stop prepare_test_bgm_stop
#define fd2_save_manual_state_save prepare_test_state_save
#define fd2_field_magic_list_draw prepare_test_magic_draw
#define fd2_field_water_tick_live prepare_test_water_live
#define fd2_input_clear_keys prepare_test_clear_keys
#define fd2_vga_set_brightness prepare_test_brightness
#define fd2_scene_palette_fade prepare_test_palette_fade
#define fd2_static_flow_lookup prepare_test_flow_lookup
#define fd2_field_rng_next prepare_test_rng
#define fd2_map_sprite_blit prepare_test_blit
#define fd2_field_prepare_draw_members prepare_test_members
#include "../src/field_prepare_play.c"
#undef fd2_field_prepare_draw_members
#undef fd2_field_visual_actor_step_live
#undef fd2_field_rng_next
#undef fd2_map_sprite_blit
#undef SDL_GetTicks
#undef fd2_vga_present
#undef fd2_vga_upload_current_frame
#undef fd2_delay_ms
#undef fd2_vga_delay_ms
#undef fd2_vga_present_timed
#undef fd2_vga_wait_frame_deadline
#undef fd2_wait_ticks
#undef fd2_vga_wait_ticks
#undef calloc
#undef fd2_pcm_play_replace
#undef fd2_bgm_set_music_enabled
#undef fd2_bgm_play
#undef fd2_bgm_stop
#undef fd2_save_manual_state_save
#undef fd2_field_magic_list_draw
#undef fd2_field_water_tick_live
#undef fd2_input_clear_keys
#undef fd2_vga_set_brightness
#undef fd2_scene_palette_fade
#undef fd2_static_flow_lookup

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

static int option_sync_count, option_sync_enabled, option_sync_fade;
static int prepare_test_bgm_option(fd2_bgm_player *player, int enabled, int fade) {
    (void)player; option_sync_count++; option_sync_enabled=enabled; option_sync_fade=fade;
#ifdef FD2_CLOCK_FAILURE_TEST
    if(clock_audio_at)return clock_audio_return(0);
#endif
    return 0;
}
static uint64_t clock_ms;
static unsigned source_upload_active,source_uploads,source_upload_cost_ms;
static unsigned host_ticket_probe,host_timed_calls,host_deadline_calls,host_water_fail_call;
static uint64_t host_wait_ms;
static unsigned clock_reads,clock_advance_on_read,clock_read_advance_ms;
static const uint64_t *clock_script_ms;
static size_t clock_script_count;
static void (*clock_read_observer)(unsigned read);
static int blink_tracking,blink_bad;
static unsigned blink_rng_calls,blink_roster_calls;
static unsigned fail_members_draw; /* 0关闭，否则在第N次present后成员叶失败。 */
static unsigned frame_number,quit_frame;
static fd2_vga *blink_vga;
static const fd2_vga *blink_expected;
static prepare_wait *blink_state;
static prepare_choice_scope *choice_blink_scope;
static const fd2_vga *choice_blink_expected;
static unsigned choice_blink_rng_calls;
static int choice_blink_bad;
static uint32_t prepare_test_rng(void *userdata) {
    if(blink_tracking) {
        blink_rng_calls++;
        if(blink_roster_calls!=1u || !blink_expected ||
            memcmp(blink_vga->framebuffer,blink_expected->framebuffer,VGA_W*VGA_H) ||
            blink_state->last!=0 || blink_state->portrait!=3)blink_bad=1;
        clock_ms+=50; /* 在draw后再跨tick，锚必须重新读取。 */
    }
    if(choice_blink_scope) {
        choice_blink_rng_calls++;
        if(!choice_blink_scope->blinking || !choice_blink_expected ||
            memcmp(choice_blink_scope->page->framebuffer,choice_blink_expected->framebuffer,VGA_W*VGA_H))
            choice_blink_bad=1;
    }
    return fd2_field_rng_next(userdata);
}
static int prepare_test_members(fd2_vga *vga,const fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state,size_t first,int selected,unsigned phase) {
    if(fail_members_draw && frame_number>=fail_members_draw)return -1;
    int result=fd2_field_prepare_draw_members(vga,a,state,first,selected,phase);
    /* 观察真实成员widget消费；不再拿已移除的steady底板blit模拟draw成本。 */
    if(!result && blink_tracking && vga==blink_vga){blink_roster_calls++;clock_ms+=70;}
    return result;
}
static void prepare_test_blit(fd2_vga *vga,const fd2_image *image,int x,int y,int transparent) {
    fd2_map_sprite_blit(vga,image,x,y,transparent);
}
static int service_exact_time,service_capture_first;
static unsigned service_delay_count;
static uint32_t service_delays[6000];
static unsigned service_tick_count;
static uint32_t service_ticks[64];
static uint8_t service_first_frame[VGA_W*VGA_H];
static const fd2_field_prepare_assets *confirm_probe_assets;
static unsigned confirm_probe_hits;
static const fd2_field_prepare_assets *service_route_assets;
static const fd2_vga *service_route_products;
static unsigned service_route_menu_hits,service_route_product_hits;
static int probe_image(const fd2_vga *vga,const fd2_image *image,int x,int y) {
    if(!image->pixels)return 0;
    size_t opaque=0;
    for(int row=0;row<image->height;row++)for(int col=0;col<image->width;col++) {
        uint8_t pixel=image->pixels[(size_t)row*(size_t)image->width+(size_t)col];
        if(pixel){opaque++;if(vga->framebuffer[(y+row)*VGA_STRIDE+x+col]!=pixel)return 0;}
    }
    return opaque!=0;
}
static struct { unsigned frame; SDL_Scancode key; } pulses[20];
static size_t pulse_count;
static struct { unsigned frame; uint16_t code; } raw_pulses[8];
static size_t raw_pulse_count;
static int music_tracks[20];
static size_t music_stages[20], music_count;
static int quit_on_music_track;
static unsigned fail_frame_allocation_at, sound_calls, fail_sound_call;
static size_t sound_samples[32];
static int sound_loops[32];
static float sound_last_gain;
static unsigned magic_draw_calls, fail_magic_draw_call, state_save_calls;
static int probe_title, title_background_ok, title_palette_ok;
static unsigned title_fade_calls;
static unsigned brightness_count;
static int brightness_values[32];
static int title_fade_result,title_fade_in,title_fade_fast;
static unsigned water_calls;
static unsigned water_advance_ms;
static int water_failure;
static unsigned clear_calls, quit_on_clear, frames_at_clear[8];
static unsigned town_script_origin,town_bootstrap_pending,town_fade_count;
static int town_fade_values[65],town_bad_pixels,town_pending_at_clear,town_first_black;
static unsigned town_bootstrap_frames,town_inject_early,town_quit_frame;
static const uint8_t *town_expected_pixels;
static uint8_t expected_title_palette[VGA_PALETTE_SIZE];
static int slot_upper_probe,slot_upper_changed;
static uint8_t slot_upper_before[112u*VGA_W];
static int choice_timing_probe;
static unsigned choice_plain,choice_timed;

static void *prepare_test_calloc(size_t count, size_t size) {
    if (count == 1 && size == sizeof(fd2_vga) && fail_frame_allocation_at &&
        frame_number >= fail_frame_allocation_at) return NULL;
    return calloc(count, size);
}
static int prepare_test_pcm_replace(fd2_pcm_player *player, const fd2_pcm_bank *bank,
        size_t sample, int loop_count, float gain) {
    (void)player; (void)bank;
#ifdef FD2_CLOCK_FAILURE_TEST
    if(clock_audio_at)return clock_audio_return(0);
#endif
    if(sound_calls<32u){sound_samples[sound_calls]=sample;sound_loops[sound_calls]=loop_count;}
    sound_last_gain = gain;
    return ++sound_calls == fail_sound_call ? -1 : 0;
}
static int prepare_test_state_save(const char *path, size_t slot,
        const fd2_save_manual_state *state) {
    state_save_calls++;
    return fd2_save_manual_state_save(path, slot, state);
}
static int prepare_test_magic_draw(fd2_vga *vga, const fd2_field_info_assets *assets,
        const fd2_font *font, const fd2_text_entry *text,
        const fd2_field_unit *unit, int selected) {
    if (++magic_draw_calls == fail_magic_draw_call) return -1;
    return fd2_field_magic_list_draw(vga, assets, font, text, unit, selected);
}
static int prepare_test_water_live(fd2_vga *vga) {
    water_calls++;
    if(water_failure || (host_ticket_probe && water_calls==host_water_fail_call))return -1;
    int result=fd2_field_water_tick(vga,prepare_test_ticks_ns());
    clock_ms+=water_advance_ms;
    return result;
}
static void prepare_test_clear_keys(fd2_input *input) {
    if(town_bootstrap_pending)town_pending_at_clear=fd2_input_has_any_key(input);
    fd2_input_clear_keys(input);
    if(town_bootstrap_pending){town_script_origin=frame_number;town_bootstrap_pending=0;}
    if(clear_calls<8u)frames_at_clear[clear_calls]=frame_number;
    if(++clear_calls==quit_on_clear)input->quit_requested=1;
}
static const fd2_static_flow *prepare_test_flow_lookup(const char *domain,const char *identity) {
    const fd2_static_flow *flow=fd2_static_flow_lookup(domain,identity);
    if(flow && !strcmp(domain,"preparation") && !strcmp(identity,"town.bootstrap"))town_bootstrap_pending=1;
    return flow;
}
static void prepare_test_brightness(fd2_vga *vga,int value) {
    fd2_vga_set_brightness(vga,value);
    if(brightness_count<32u)brightness_values[brightness_count]=value;
    brightness_count++;
    if(town_bootstrap_pending){
        if(town_fade_count<65u)town_fade_values[town_fade_count]=value;
        town_fade_count++;
    }
}

Uint64 SDLCALL prepare_test_ticks(void) { return clock_ms; }
int prepare_test_present(fd2_vga *vga) {
    if(service_capture_first){memcpy(service_first_frame,vga->framebuffer,sizeof(service_first_frame));service_capture_first=0;}
    if(choice_timing_probe) {
        choice_plain++;frame_number++;
        if(!source_upload_active && vga->frame_deadline_ns>clock_ms*UINT64_C(1000000))
            clock_ms=(vga->frame_deadline_ns+999999u)/UINT64_C(1000000);
        if(!source_upload_active)vga->frame_deadline_ns=vga->frame_interval_ns=0;
        if(quit_frame && frame_number==quit_frame)vga->input.quit_requested=1;
        return 0;
    }
    clock_ms += source_upload_active ? source_upload_cost_ms:16u; frame_number++;
    unsigned script_frame=frame_number-town_script_origin;
    if(slot_upper_probe && memcmp(vga->framebuffer,slot_upper_before,sizeof(slot_upper_before)))slot_upper_changed=1;
    memset(vga->input.test_key_state, 0, sizeof(vga->input.test_key_state));
    for (size_t i = 0; !town_bootstrap_pending && i < pulse_count; i++) if (pulses[i].frame == script_frame)
        fd2_input_set_test_key_state(&vga->input, pulses[i].key, 1);
    if(town_bootstrap_pending){
        town_bootstrap_frames++;
        if(town_expected_pixels && memcmp(vga->framebuffer,town_expected_pixels,VGA_W*VGA_H))town_bad_pixels=1;
        if(town_bootstrap_frames==1u){town_first_black=1;for(size_t i=0;i<sizeof(vga->dac);i++)if(vga->dac[i])town_first_black=0;}
        if(town_inject_early && town_bootstrap_frames==1u)fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_SPACE,1);
        if(town_quit_frame && town_bootstrap_frames==town_quit_frame)vga->input.quit_requested=1;
    }
    /* 以原YES/NO两完整raw资源到达最终位置为响应门；不猜文字耗时或延长
     * 固定帧脉冲。只用于已明确进入出阵confirm的测试，命中后单次投递。 */
    if(confirm_probe_assets && probe_image(vga,&confirm_probe_assets->commands.frames[49],232,168) &&
        probe_image(vga,&confirm_probe_assets->commands.frames[51],264,168)) {
        fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,1);
        confirm_probe_hits++;confirm_probe_assets=NULL;
    }
    if(service_route_assets && !service_route_menu_hits) {
        int menu=1;
        for(size_t i=0;i<4u;i++)if(!probe_image(vga,&service_route_assets->service.icons[2u*i+(i==0u)],201+26*(int)i,169))menu=0;
        if(menu){fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,1);service_route_menu_hits++;}
    } else if(service_route_assets && service_route_products && !service_route_product_hits) {
        int page=1;
        for(int row=112;row<198;row++)if(memcmp(vga->framebuffer+row*VGA_STRIDE+5,
            service_route_products->framebuffer+row*VGA_STRIDE+5,310))page=0;
        if(page){service_route_product_hits++;vga->input.quit_requested=1;}
    }
    fd2_input_begin_test_frame(&vga->input, clock_ms);
    /* caller消费边界独立投递原BIOS AX；键盘映射由input测试另验。 */
    for(size_t i=0;!town_bootstrap_pending && i<raw_pulse_count;i++)
        if(raw_pulses[i].frame==script_frame && vga->input.queued_count<FD2_INPUT_KEY_QUEUE_CAPACITY) {
            unsigned tail=(vga->input.queued_head+vga->input.queued_count)%FD2_INPUT_KEY_QUEUE_CAPACITY;
            vga->input.queued_keys[tail]=(fd2_input_event){.key=FD2_INPUT_KEY_OTHER,.bios_code=raw_pulses[i].code};
            vga->input.queued_count++;
        }
    if (probe_title && script_frame == 70) {
        title_background_ok = 1;
        for (size_t i = 0; i < 112u * 320u; i++) if (vga->framebuffer[i] != 0) title_background_ok = 0;
        title_palette_ok = memcmp(vga->palette, expected_title_palette, VGA_PALETTE_SIZE) == 0;
        for (size_t i = 0; i < VGA_PALETTE_SIZE; i++)
            if (vga->dac[i] != (uint8_t)((expected_title_palette[i] << 2) | (expected_title_palette[i] >> 4))) title_palette_ok = 0;
    }
    if ((!town_bootstrap_pending && quit_frame && script_frame == quit_frame) ||
            script_frame > (service_exact_time ? 6000u:150u))
        vga->input.quit_requested = 1;
return 0;
}
static void prepare_test_upload(fd2_vga *vga) {
    source_uploads++;source_upload_active=1;prepare_test_present(vga);source_upload_active=0;
#ifdef FD2_CLOCK_FAILURE_TEST
    clock_fault_upload(vga);
#endif
}
static int prepare_test_delay(uint32_t ms) {
#ifdef FD2_CLOCK_FAILURE_TEST
    if(clock_fault_wait()<0)return -71;
#endif
    if(service_delay_count<6000u)service_delays[service_delay_count++]=ms;
    clock_ms+=ms;
return 0;
}
int prepare_test_present_timed(fd2_vga *vga, uint32_t delay) {
#ifdef FD2_CLOCK_FAILURE_TEST
    if(clock_fault_wait()<0)return -71;
#endif
    if(host_ticket_probe) {
        /* vga.c的公开timed合同：先旧target，再独立7ms上传成本；不替helper藏foreign。 */
        uint64_t interval=(uint64_t)delay*UINT64_C(1000000);
        uint64_t target=vga->frame_deadline_ns,now=clock_ms*UINT64_C(1000000);
        int continuing=target && vga->frame_interval_ns==interval && now<=target+interval;
        if(target>now) {
            uint64_t next_ms=(target+UINT64_C(999999))/UINT64_C(1000000);
            host_wait_ms+=next_ms-clock_ms;clock_ms=next_ms;
        }
        prepare_test_upload(vga);host_timed_calls++;
        vga->frame_deadline_ns=continuing ? target+interval:clock_ms*UINT64_C(1000000)+interval;
        vga->frame_interval_ns=interval;
        return 0;
    }
    if(choice_timing_probe) {
        if(!delay){prepare_test_present(vga);return 0;}
        choice_timed++;frame_number++;clock_ms+=delay;
        vga->frame_deadline_ns=(clock_ms+delay)*UINT64_C(1000000);
        vga->frame_interval_ns=(uint64_t)delay*UINT64_C(1000000);
        if(quit_frame && frame_number==quit_frame)vga->input.quit_requested=1;
        return 0;
    }
    uint64_t before=clock_ms;
    if(service_delay_count<6000u)service_delays[service_delay_count++]=delay;
    prepare_test_present(vga);if(service_exact_time)clock_ms=before+delay;
return 0;
}
int prepare_test_deadline(fd2_vga *vga) {
    if(host_ticket_probe) {
        host_deadline_calls++;
        uint64_t target_ms=(vga->frame_deadline_ns+UINT64_C(999999))/UINT64_C(1000000);
        if(target_ms>clock_ms){host_wait_ms+=target_ms-clock_ms;clock_ms=target_ms;}
        vga->frame_deadline_ns=vga->frame_interval_ns=0;return 0;
    }
    if(choice_timing_probe) {
        if(vga->frame_deadline_ns>clock_ms*UINT64_C(1000000))
            clock_ms=(vga->frame_deadline_ns+999999u)/UINT64_C(1000000);
        vga->frame_deadline_ns=vga->frame_interval_ns=0;
    }
return 0;
}
static int prepare_test_wait_ticks(uint32_t ticks) {
#ifdef FD2_CLOCK_FAILURE_TEST
    if(clock_fault_wait()<0)return -71;
#endif
    if(service_tick_count<64u)service_ticks[service_tick_count++]=ticks;
    uint64_t deadline=fd2_bios_clock_deadline_ns(clock_ms*UINT64_C(1000000),ticks);
    clock_ms=(deadline+UINT64_C(999999))/UINT64_C(1000000);
return 0;
}

int fd2_field_play_request_music(fd2_vga *output,fd2_field_play_context *context,
        size_t stage, int track, const char *reason) {
    (void)output; (void)reason;
    if (!context || !context->initialized || music_count >= 20) return -1;
    music_stages[music_count] = stage;
    music_tracks[music_count++] = track;
    if(track==quit_on_music_track)quit_frame=frame_number-town_script_origin+1u;
    return 0;
}

/* cold music叶直接借bgm；测试以context地址作不解引用音频后端的借用标记。 */
static int prepare_test_bgm_play(fd2_bgm_player *player,size_t track,int loop_count) {
    (void)loop_count;
#ifdef FD2_CLOCK_FAILURE_TEST
    if(clock_audio_at)return clock_audio_return(0);
#endif
    fd2_field_play_context *context=(fd2_field_play_context *)player;
    return fd2_field_play_request_music(NULL,context,context->preparation.stage_id,(int)track,"cold-test");
}
static int prepare_test_bgm_stop(fd2_bgm_player *player) {
#ifdef FD2_CLOCK_FAILURE_TEST
    if(clock_audio_at)return clock_audio_return(0);
#endif
    fd2_field_play_context *context=(fd2_field_play_context *)player;
    return fd2_field_play_request_music(NULL,context,context->preparation.stage_id,-1,"cold-test");
}

static void controls(fd2_vga *vga) {
    fd2_input_init(&vga->input); clock_ms = 0; frame_number = quit_frame = 0;
    fd2_bios_clock_reset(0,0); /* 本测试新scope的共享时钟与合成clock_ms一同复位。 */
    source_uploads=source_upload_active=source_upload_cost_ms=0;
    host_ticket_probe=host_timed_calls=host_deadline_calls=host_water_fail_call=0;host_wait_ms=0;
    choice_timing_probe=0;choice_plain=choice_timed=0;
    /* 各用例默认新App；跨页/重入轨迹自行保留这两个owner。 */
    vga->prepare_highlight_phase=vga->prepare_sprite_phase=0;
    clock_reads=clock_advance_on_read=clock_read_advance_ms=0;
    clock_script_ms=NULL;clock_script_count=0;clock_read_observer=NULL;
    water_calls = water_advance_ms = 0;water_failure=0;
    blink_tracking=blink_bad=0;blink_rng_calls=blink_roster_calls=0;
    blink_vga=NULL;blink_expected=NULL;blink_state=NULL;fail_members_draw=0;
    choice_blink_scope=NULL;choice_blink_expected=NULL;choice_blink_rng_calls=0;choice_blink_bad=0;
    clear_calls=quit_on_clear=0;memset(frames_at_clear,0,sizeof(frames_at_clear));
    town_script_origin=town_bootstrap_pending=town_fade_count=town_bootstrap_frames=town_inject_early=town_quit_frame=0;
    town_expected_pixels=NULL;town_bad_pixels=town_pending_at_clear=town_first_black=0;
    memset(town_fade_values,0,sizeof(town_fade_values));
    pulse_count = music_count = 0; memset(pulses, 0, sizeof(pulses));
    quit_on_music_track=-2;
    raw_pulse_count=0;memset(raw_pulses,0,sizeof(raw_pulses));
    memset(music_stages, 0, sizeof(music_stages));
    fail_frame_allocation_at = sound_calls = fail_sound_call = 0;
    memset(sound_samples,0,sizeof(sound_samples));
    memset(sound_loops,0,sizeof(sound_loops));
    magic_draw_calls = fail_magic_draw_call = state_save_calls = 0;
    probe_title = title_background_ok = title_palette_ok = 0;
    title_fade_calls=0;title_fade_result=FD2_SCENE_RESULT_OK;title_fade_in=title_fade_fast=-1;
    brightness_count=0;memset(brightness_values,0,sizeof(brightness_values));
    service_exact_time=service_capture_first=0;service_delay_count=0;
    service_tick_count=0;memset(service_ticks,0,sizeof(service_ticks));
    confirm_probe_assets=NULL;confirm_probe_hits=0;
    service_route_assets=NULL;service_route_products=NULL;service_route_menu_hits=service_route_product_hits=0;
    slot_upper_probe=slot_upper_changed=0;
}

/* caller I/O替身：保留原65/64次呈现的输入cutoff；精确源图由Scene测试执行。 */
static int prepare_test_palette_fade(fd2_vga *vga,int fade_in,int fast) {
    title_fade_calls++;title_fade_in=fade_in;title_fade_fast=fast;
    if((fade_in!=0 && fade_in!=1) || fast)return FD2_SCENE_RESULT_ERROR;
    if(title_fade_result!=FD2_SCENE_RESULT_OK)return title_fade_result;
    for(int step=0;step<(fade_in?65:64);step++) {
        int dark=fade_in?64-step:step;
        if(fd2_input_take_quit(&vga->input))return FD2_SCENE_RESULT_HOST_QUIT;
        if(fd2_vga_palette_subtract(vga,0,255,dark)!=0)return FD2_SCENE_RESULT_ERROR;
        if(town_bootstrap_pending) {
            if(town_fade_count<65u)town_fade_values[town_fade_count]=dark;
            town_fade_count++;
        }
        prepare_test_present_timed(vga,2);
        if(fd2_input_take_quit(&vga->input))return FD2_SCENE_RESULT_HOST_QUIT;
    }
    return FD2_SCENE_RESULT_OK;
}
static void pulse(unsigned frame, SDL_Scancode key) {
    pulses[pulse_count].frame = frame; pulses[pulse_count++].key = key;
}
static int initial_state(fd2_save_manual_state *state) {
    fd2_save_file file = {0}; fd2_save_battle_snapshot snapshot;
    if (fd2_save_file_open(&file, "original_game/START.SAV") != 0) return -1;
    int result = fd2_save_file_get_battle_snapshot(&file, &snapshot);
    fd2_save_file_close(&file);
    if (result != 0) return -1;
    memset(state, 0, sizeof(*state));
    state->stage_id = 1; state->unit_count = snapshot.meta[9];
    memcpy(state->unit_area, snapshot.roster, sizeof(state->unit_area));
    memcpy(state->options, (uint8_t[]){1, 1, 1, 1}, 4);
    state->gold_or_flags = 12345;
    return 0;
}

static int renderer_test(fd2_archive *fdother, fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state, fd2_vga *vga) {
    uint8_t before[VGA_W * VGA_H];
    memset(vga->framebuffer, 0x63, sizeof(vga->framebuffer));
    memcpy(before, vga->framebuffer, sizeof(before));
    CHECK(fd2_field_prepare_draw_town(vga, a, state, 6, 0) == -1);
    CHECK(memcmp(before, vga->framebuffer, sizeof(before)) == 0);
    static const int marker_x[5] = {33, 45, 63, 158, 186};
    static const int marker_y[5] = {50, 113, 167, 143, 69};
    fd2_vga *expected = calloc(1, sizeof(*expected)); CHECK(expected);
    for (int selected = 0; selected < 5; selected++) for (unsigned phase = 0; phase < 4; phase++) {
        memcpy(expected->framebuffer,before,sizeof(before)); /* 源投影保4px外沿。 */
        for (size_t row = 0; row < 192; row++) memcpy(expected->framebuffer + (row + 4) * 320 + 4,
            a->town.pixels + row * 320, 312);
        fd2_map_sprite_blit(expected, &a->town_panel, 248, 166, -1);
        CHECK(text_draw(expected, a, 495u + (size_t)selected, 256, 172, 205, 74, 0) == 0);
        fd2_map_sprite_blit_frame(expected, &a->sprites, (size_t)state->unit_area[7] * 12u +
            (phase == 3 ? 1 : phase), marker_x[selected], marker_y[selected], 0);
        CHECK(fd2_field_prepare_draw_town(vga, a, state, selected, phase) == 0);
        CHECK(memcmp(vga->framebuffer, expected->framebuffer, sizeof(before)) == 0);
    }
    CHECK(fd2_field_prepare_draw_hotel(vga, a, fdother, 2, 1, 0, 585) == 0);
    for (int y = 0; y < 80; y++) CHECK(memcmp(vga->framebuffer + (y + 5) * 320 + 107,
        a->hotel_portraits[0].pixels + y * 80, 80) == 0);
    CHECK(vga->framebuffer[169 * 320 + 253 + 2] == a->hotel_icons[5].pixels[2]);
    memcpy(before, vga->framebuffer, sizeof(before));
    fd2_save_file save = {0}; CHECK(fd2_save_file_create_empty(&save) == 0);
    CHECK(fd2_field_prepare_draw_slots(vga, a, &save, 0) == 0);
    CHECK(vga->framebuffer[112 * 320 + 5] == a->slots.pixels[0]);
    memcpy(before, vga->framebuffer, sizeof(before));
    uint16_t offset = a->text.offsets[549]; a->text.offsets[549] = (uint16_t)(a->text.size + 1u);
    CHECK(fd2_field_prepare_draw_slots(vga, a, &save, 1) == -1);
    CHECK(memcmp(before, vga->framebuffer, sizeof(before)) == 0);
    a->text.offsets[549] = offset;
    fd2_save_file_close(&save); free(expected);
    return 0;
}

static int selector_test(void) {
    int selected = 0; size_t first = 0;
    CHECK(roster_step(FD2_INPUT_KEY_LEFT, 32, &selected, &first) == 0 && selected == 0);
    for (int i = 1; i < 32; i++) {
        CHECK(roster_step(FD2_INPUT_KEY_RIGHT, 32, &selected, &first) == 0 && selected == i);
        CHECK(first <= (size_t)selected && (size_t)selected - first < 6u && (first & 1u) == 0);
    }
    CHECK(roster_step(FD2_INPUT_KEY_RIGHT, 32, &selected, &first) == 0 && selected == 31);
    for (int i = 30; i >= 0; i--) CHECK(roster_step(FD2_INPUT_KEY_LEFT, 32, &selected, &first) == 0 && selected == i);
    CHECK(first == 0);
    CHECK(roster_step(FD2_INPUT_KEY_DOWN, 7, &selected, &first) == 0 && selected == 2);
    CHECK(roster_step(FD2_INPUT_KEY_UP, 7, &selected, &first) == 0 && selected == 0);
    CHECK(roster_step(FD2_INPUT_KEY_DOWN, 0, &selected, &first) == -1 && selected == 0 && first == 0);
    return 0;
}

static int title_cancel_and_load(fd2_archive *fdother,
        const fd2_save_manual_state *state, fd2_vga *vga) {
    /* 26080/26088读原byte；25efa不写这两个App配置，不能bool化。 */
    fd2_save_manual_state raw_state=*state;raw_state.options[0]=0xfe;raw_state.options[1]=0x7b;state=&raw_state;
    char path[FD2_TEST_PATH_MAX]; CHECK(fd2_test_temp_path(path) == 0); CHECK(remove(path) == 0);
    CHECK(fd2_save_manual_state_save(path, 0, state) == 0);
    fd2_field_play_context context = {0}; context.initialized = 1;
    context.experience_value = 631; context.message_number = 99;
    context.camera_option=0x93;context.effect_option=0x8e;
    context.bgm = (fd2_bgm_player *)&context; option_sync_count=0;
    fd2_field_rng_seed(&context.rng, 0x1234);
    fd2_save_manual_state before = context.preparation;
    const uint8_t *palette; size_t palette_size;
    CHECK(fd2_archive_get(fdother, 0, &palette, &palette_size) == 0 && palette_size >= VGA_PALETTE_SIZE);
    memcpy(expected_title_palette, palette, VGA_PALETTE_SIZE);
    memset(vga->framebuffer, 0xa3, sizeof(vga->framebuffer));
    memset(vga->palette, 23, sizeof(vga->palette)); memset(vga->dac, 92, sizeof(vga->dac));
    controls(vga); probe_title = 1; pulse(70, SDL_SCANCODE_ESCAPE);
    CHECK(fd2_field_prepare_load_title(vga, fdother, &context, path, NULL) == FD2_FIELD_PREPARE_TITLE);
    CHECK(!context.has_preparation && memcmp(&before, &context.preparation, sizeof(before)) == 0);
    CHECK(context.camera_option==0x93 && context.effect_option==0x8e);
    CHECK(context.rng.state == 0x1234 && context.experience_value == 631);
    CHECK(title_background_ok && title_palette_ok);
    CHECK(option_sync_count==0 && music_count==0);
    CHECK(title_fade_calls==1u && title_fade_in==0 && title_fade_fast==0);
    for (size_t i = 0; i < sizeof(vga->framebuffer); i++) CHECK(vga->framebuffer[i] == 0);
    controls(vga); pulse(70, SDL_SCANCODE_RETURN);
    CHECK(fd2_field_prepare_load_title(vga, fdother, &context, path, NULL) == FD2_FIELD_PREPARE_DEPART);
    CHECK(context.has_preparation && context.preparation.stage_id == state->stage_id && context.preparation.unit_count == state->unit_count);
    CHECK(option_sync_count==1 && option_sync_enabled==(state->options[2]!=0) && option_sync_fade==0 && music_count==0);
    CHECK(context.music_enabled==(state->options[2]!=0) && context.sfx_enabled==(state->options[3]!=0));
    CHECK(context.camera_option==0xfe && context.effect_option==0x7b);
    CHECK(memcmp(context.preparation.unit_area, state->unit_area, sizeof(state->unit_area)) == 0);
    CHECK(context.experience_value == 631 && context.rng.state == 0x1234);
    controls(vga);option_sync_count=0;pulse(70,SDL_SCANCODE_RETURN);
    CHECK(fd2_field_prepare_load_title(vga,fdother,&context,path,NULL)==FD2_FIELD_PREPARE_DEPART &&
        option_sync_count==1 && context.camera_option==0xfe && context.effect_option==0x7b &&
        !memcmp(context.preparation.unit_area,state->unit_area,sizeof(state->unit_area)));
    controls(vga); quit_frame = 3;
    before = context.preparation;
    CHECK(fd2_field_prepare_load_title(vga, fdother, &context, path, NULL) == FD2_FIELD_PREPARE_HOST_QUIT);
    CHECK(memcmp(&before, &context.preparation, sizeof(before)) == 0);
    CHECK(context.camera_option==0xfe && context.effect_option==0x7b);
    CHECK(title_fade_calls==1u && frame_number==3u);
    for(int host=0;host<2;host++) {
        controls(vga);title_fade_result=host?FD2_SCENE_RESULT_HOST_QUIT:FD2_SCENE_RESULT_ERROR;
        fd2_field_play_context prefix=context;
        CHECK(fd2_field_prepare_load_title(vga,fdother,&context,path,NULL)==
            (host?FD2_FIELD_PREPARE_HOST_QUIT:FD2_FIELD_PREPARE_ERROR));
        CHECK(title_fade_calls==1u && !frame_number && !state_save_calls && !music_count &&
            !memcmp(&prefix,&context,sizeof(context)));
    }
    controls(vga);
    fd2_vga unchanged = *vga;
    fd2_field_play_context unchanged_context = context;
    uint32_t saved_offset = fdother->offsets[1];
    fdother->offsets[1] = fdother->offsets[0] + VGA_PALETTE_SIZE - 1u;
    CHECK(fd2_field_prepare_load_title(vga, fdother, &context, path, NULL) == FD2_FIELD_PREPARE_ERROR);
    CHECK(memcmp(vga, &unchanged, sizeof(*vga)) == 0 && frame_number == 0);
    fdother->offsets[1] = saved_offset;
    uint8_t *writable_palette = fdother->data + fdother->offsets[0];
    uint8_t saved_color = writable_palette[0]; writable_palette[0] = 64;
    CHECK(fd2_field_prepare_load_title(vga, fdother, &context, path, NULL) == FD2_FIELD_PREPARE_ERROR);
    CHECK(memcmp(vga, &unchanged, sizeof(*vga)) == 0 && frame_number == 0);
    writable_palette[0] = saved_color;
    CHECK(memcmp(&unchanged_context, &context, sizeof(context)) == 0);
    char invalid_path[FD2_TEST_PATH_MAX]; CHECK(fd2_test_temp_path(invalid_path) == 0);
    CHECK(fd2_field_prepare_load_title(vga, fdother, &context, invalid_path, NULL) == FD2_FIELD_PREPARE_ERROR);
    CHECK(memcmp(vga, &unchanged, sizeof(*vga)) == 0 && frame_number == 0);
    CHECK(memcmp(&unchanged_context, &context, sizeof(context)) == 0);
    CHECK(remove(invalid_path) == 0);
    saved_offset = fdother->offsets[14]; fdother->offsets[14] = fdother->offsets[13];
    CHECK(fd2_field_prepare_load_title(vga, fdother, &context, path, NULL) == FD2_FIELD_PREPARE_ERROR);
    CHECK(memcmp(vga, &unchanged, sizeof(*vga)) == 0 && frame_number == 0);
    CHECK(memcmp(&unchanged_context, &context, sizeof(context)) == 0);
    fdother->offsets[14] = saved_offset;
    CHECK(remove(path) == 0);
    return 0;
}

static int hotel_save_reopen(fd2_archive *fdother, fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state, fd2_vga *vga) {
    char path[FD2_TEST_PATH_MAX]; CHECK(fd2_test_temp_path(path) == 0); CHECK(remove(path) == 0);
    fd2_field_play_context context = {0}; context.initialized = context.has_preparation = 1;
    context.preparation = *state; context.music_enabled=state->options[2];context.sfx_enabled=state->options[3]; context.experience_value = 99;
    context.camera_option=state->options[0];context.effect_option=state->options[1];
    fd2_field_rng_seed(&context.rng, 17);
    prepare_ui ui = {vga, fdother, a, &context, NULL, path, -1, 0, -1, .service = {0}};
    controls(vga); pulse(6, SDL_SCANCODE_RETURN); pulse(13, SDL_SCANCODE_ESCAPE); pulse(19, SDL_SCANCODE_ESCAPE);
    CHECK(slots_run(&ui, 1, 0) == 0);
    fd2_save_manual_state loaded;
    CHECK(fd2_save_manual_state_load(path, 0, &loaded) == 0);
    CHECK(memcmp(loaded.unit_area, state->unit_area, sizeof(state->unit_area)) == 0);
    CHECK(context.experience_value == 99 && memcmp(&context.preparation, state, sizeof(*state)) == 0);
    CHECK(remove(path) == 0);
    return 0;
}

static int town_roundtrip(fd2_archive *fdother, const fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state, fd2_vga *vga) {
    fd2_field_play_context context = {0}; context.initialized = context.has_preparation = 1;
    context.preparation = *state; context.music_enabled=state->options[2];context.sfx_enabled=state->options[3]; context.experience_value = 42; context.message_number = 7;
    context.camera_option=state->options[0];context.effect_option=state->options[1];
    controls(vga);
    pulse(1, SDL_SCANCODE_RETURN); /* town0 ->hotel */
    pulse(6, SDL_SCANCODE_ESCAPE); /* hotel ->town */
    pulse(12, SDL_SCANCODE_LEFT); pulse(14, SDL_SCANCODE_LEFT);
    pulse(15, SDL_SCANCODE_RETURN);confirm_probe_assets=a;
    CHECK(fd2_field_prepare_run(vga, fdother, &context, "unused.SAV", NULL) == FD2_FIELD_PREPARE_DEPART);
    CHECK(confirm_probe_hits==1u);
    CHECK(music_count == 5 && music_tracks[0] == 10 && music_tracks[1] == -1 &&
          music_tracks[2] == 13 && music_tracks[3] == 10 && music_tracks[4] == -1);
    CHECK(memcmp(&context.preparation, state, sizeof(*state)) == 0 &&
          context.experience_value == 42 && context.message_number == 7);
    controls(vga); quit_frame = 1;
    CHECK(fd2_field_prepare_run(vga, fdother, &context, "unused.SAV", NULL) == FD2_FIELD_PREPARE_HOST_QUIT);
    CHECK(memcmp(&context.preparation, state, sizeof(*state)) == 0);
    return 0;
}

static int read_container(const char *path, uint8_t bytes[FD2_SAVE_FILE_SIZE]) {
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    int okay = fread(bytes, 1, FD2_SAVE_FILE_SIZE, file) == FD2_SAVE_FILE_SIZE && fgetc(file) == EOF;
    return fclose(file) == 0 && okay ? 0 : -1;
}

static int notown_save_direct_picker(fd2_archive *fdother,fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    char path[FD2_TEST_PATH_MAX];CHECK(fd2_test_temp_path(path)==0 && remove(path)==0);
    CHECK(fd2_save_manual_state_save(path,0,state)==0);
    uint8_t before[FD2_SAVE_FILE_SIZE],after[FD2_SAVE_FILE_SIZE];
    CHECK(read_container(path,before)==0);fd2_save_xor_crypt(before,sizeof(before));
    fd2_field_play_context context={0};context.initialized=context.has_preparation=1;
    context.preparation=*state;context.preparation.stage_id=27;context.experience_value=321;
    context.message_number=123;fd2_field_rng_seed(&context.rng,0x4321u);fd2_field_rng rng=context.rng;
    prepare_ui ui={vga,fdother,a,&context,NULL,path,-1,0,-1, .service = {0}};
    /* 原NoTown不读660；把该片段损坏，仍必须一次Save后直接回同槽picker。
     * 6为首次完整页，DOWN后7确认slot1；19为直接重开的完整页，仅Esc退出。 */
    uint16_t old=a->text.offsets[660];a->text.offsets[660]=(uint16_t)(a->text.size+1u);
    controls(vga);memset(vga->framebuffer,0x63,sizeof(vga->framebuffer));
    memcpy(slot_upper_before,vga->framebuffer,sizeof(slot_upper_before));slot_upper_probe=1;
    pulse(6,SDL_SCANCODE_DOWN);pulse(7,SDL_SCANCODE_RETURN);pulse(19,SDL_SCANCODE_ESCAPE);
    fd2_static_flow_node node={.op="manual_save",.args={0}};unsigned port=99;
    CHECK(prepare_service_pick(&ui,&node,&port)==1 && port==FD2_FLOW_NEXT);
    a->text.offsets[660]=old;
    CHECK(state_save_calls==1u && frame_number==25u && !slot_upper_changed && !music_count);
    CHECK(context.rng.state==rng.state && context.experience_value==321 && context.message_number==4);
    fd2_save_manual_state loaded;CHECK(fd2_save_manual_state_load(path,1,&loaded)==0 &&
        !memcmp(&loaded,&context.preparation,sizeof(loaded)));
    CHECK(read_container(path,after)==0);fd2_save_xor_crypt(after,sizeof(after));
    for(size_t i=0;i<sizeof(before)-4u;i++)
        if(i<FD2_SAVE_SLOT_BASE+FD2_SAVE_SLOT_SIZE ||
            i>=FD2_SAVE_SLOT_BASE+FD2_SAVE_SLOT_SIZE+FD2_SAVE_UNIT_TABLE_SIZE+10u)CHECK(before[i]==after[i]);
    controls(vga);node.args[0]=2;
    CHECK(prepare_service_pick(&ui,&node,&port)==-1 && port==FD2_FLOW_ERROR && !frame_number && !state_save_calls && context.rng.state==rng.state);
    CHECK(slots_run(&ui,0,PREPARE_SLOT_NOTOWN_SAVE)==-1 && !frame_number && !state_save_calls);
    CHECK(remove(path)==0);return 0;
}

static int save_commits_before_first_close(fd2_archive *fdother,fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    /* 2968d写点在首个26996收框之前；hostquit与cue失败都保留已写槽，
     * 不重试writer、不回滚owner。对NoTown及酒店两个合法Save调用者均检查。 */
    for(int caller=0;caller<2;caller++)for(int fault=0;fault<2;fault++) {
        char path[FD2_TEST_PATH_MAX];CHECK(fd2_test_temp_path(path)==0 && remove(path)==0);
        fd2_save_manual_state older=*state;older.gold_or_flags++;
        CHECK(fd2_save_manual_state_save(path,0,&older)==0);
        uint8_t before[FD2_SAVE_FILE_SIZE],after[FD2_SAVE_FILE_SIZE];
        CHECK(read_container(path,before)==0);fd2_save_xor_crypt(before,sizeof(before));
        fd2_field_play_context context={0};context.initialized=context.has_preparation=1;
        context.preparation=*state;context.experience_value=321;
        fd2_pcm_player player={0};fd2_pcm_bank bank={0};
        fd2_field_audio audio={0};audio.player=&player;audio.ui_bank=&bank;
        prepare_ui ui={vga,fdother,a,&context,&audio,path,-1,0,-1, .service = {0}};
        controls(vga);pulse(6,SDL_SCANCODE_RETURN);
        if(fault==0)quit_frame=7;else fail_sound_call=2;
        CHECK(slots_run(&ui,1,caller?PREPARE_SLOT_HOTEL:PREPARE_SLOT_NOTOWN_SAVE)==(fault==0?1:-1));
        CHECK(state_save_calls==1u && frame_number==(fault==0?7u:6u));
        CHECK(!memcmp(&context.preparation,state,sizeof(*state)) && context.has_preparation && context.experience_value==321);
        fd2_save_manual_state loaded;CHECK(fd2_save_manual_state_load(path,0,&loaded)==0 && !memcmp(&loaded,state,sizeof(loaded)));
        CHECK(read_container(path,after)==0);fd2_save_xor_crypt(after,sizeof(after));
        for(size_t i=0;i<sizeof(before)-4u;i++)if(i<FD2_SAVE_SLOT_BASE ||
            i>=FD2_SAVE_SLOT_BASE+FD2_SAVE_UNIT_TABLE_SIZE+10u)CHECK(before[i]==after[i]);
        CHECK(remove(path)==0);
    }
    /* 酒店需要660，但资源失败必须在提交前拒绝，不能先写再发现缺字。 */
    char path[FD2_TEST_PATH_MAX];CHECK(fd2_test_temp_path(path)==0 && remove(path)==0);
    CHECK(fd2_save_manual_state_save(path,0,state)==0);
    uint8_t before[FD2_SAVE_FILE_SIZE],after[FD2_SAVE_FILE_SIZE];CHECK(read_container(path,before)==0);
    fd2_field_play_context context={0};context.initialized=context.has_preparation=1;context.preparation=*state;
    prepare_ui ui={vga,fdother,a,&context,NULL,path,-1,0,-1, .service = {0}};
    uint16_t old=a->text.offsets[660];a->text.offsets[660]=(uint16_t)(a->text.size+1u);
    controls(vga);pulse(6,SDL_SCANCODE_RETURN);CHECK(slots_run(&ui,1,PREPARE_SLOT_HOTEL)==-1 && state_save_calls==0u);
    a->text.offsets[660]=old;CHECK(read_container(path,after)==0 && !memcmp(before,after,sizeof(before)));
    CHECK(remove(path)==0);return 0;
}

static int failure_and_cancel(fd2_archive *fdother, fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state, fd2_vga *vga) {
    char path[FD2_TEST_PATH_MAX]; CHECK(fd2_test_temp_path(path) == 0); CHECK(remove(path) == 0);
    fd2_field_play_context context = {0}; context.initialized = context.has_preparation = 1;
    context.preparation = *state; context.music_enabled=state->options[2];context.sfx_enabled=state->options[3]; context.experience_value = 0xfeed1234u;
    context.camera_option=0xfe;context.effect_option=0x7b;
    prepare_ui ui = {vga, fdother, a, &context, NULL, path, -1, 0, -1, .service = {0}};
    controls(vga); pulse(6, SDL_SCANCODE_ESCAPE);
    CHECK(slots_run(&ui, 1, 0) == 0);
    FILE *probe = fopen(path, "rb"); CHECK(probe == NULL); /* save取消不新建磁盘。 */
    CHECK(memcmp(&context.preparation, state, sizeof(*state)) == 0);
    CHECK(fd2_save_manual_state_save(path, 0, state) == 0);
    uint8_t before[FD2_SAVE_FILE_SIZE], after[FD2_SAVE_FILE_SIZE];
    CHECK(read_container(path, before) == 0);
    uint16_t old = a->text.offsets[660]; a->text.offsets[660] = (uint16_t)(a->text.size + 1u);
    controls(vga); pulse(6, SDL_SCANCODE_RETURN);
    CHECK(slots_run(&ui, 1, 0) == -1);
    a->text.offsets[660] = old;
    CHECK(read_container(path, after) == 0 && memcmp(before, after, sizeof(before)) == 0);
    fd2_save_manual_state replacement = *state; replacement.gold_or_flags = 777;
    CHECK(fd2_save_manual_state_save(path, 0, &replacement) == 0);
    old = a->text.offsets[478]; a->text.offsets[478] = (uint16_t)(a->text.size + 1u);
    controls(vga); pulse(6, SDL_SCANCODE_RETURN);
    CHECK(slots_run(&ui, 0, 0) == -1);
    a->text.offsets[478] = old;
    CHECK(memcmp(&context.preparation, state, sizeof(*state)) == 0 && context.experience_value == 0xfeed1234u);
    replacement.stage_id = 0; /* codec可以保存，准备Load明确拒未支持阶段。 */
    CHECK(fd2_save_manual_state_save(path, 0, &replacement) == 0);
    controls(vga); pulse(6, SDL_SCANCODE_RETURN); pulse(13, SDL_SCANCODE_ESCAPE); pulse(19, SDL_SCANCODE_ESCAPE);
    CHECK(slots_run(&ui, 0, 0) == 0);
    CHECK(memcmp(&context.preparation, state, sizeof(*state)) == 0);
    controls(vga); pulse(6, SDL_SCANCODE_RETURN);
    CHECK(slots_run(&ui, 0, 1) == -1);
    CHECK(memcmp(&context.preparation, state, sizeof(*state)) == 0);
    FILE *file = fopen(path, "wb"); CHECK(file != NULL && fputs("bad", file) >= 0 && fclose(file) == 0);
    controls(vga);
    CHECK(slots_run(&ui, 1, 0) == -1 && frame_number == 0);
    CHECK(memcmp(&context.preparation, state, sizeof(*state)) == 0);
    CHECK(context.camera_option==0xfe && context.effect_option==0x7b);
    CHECK(remove(path) == 0);
    (void)fdother;
    return 0;
}

static int depart_capacity_and_leave(fd2_archive *fdother, fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state, fd2_vga *vga) {
    fd2_field_play_context context = {0}; context.initialized = context.has_preparation = 1;
    context.preparation = *state; context.music_enabled=state->options[2];context.sfx_enabled=state->options[3];
    context.camera_option=state->options[0];context.effect_option=state->options[1];
    for (size_t i = state->unit_count; i < 17; i++) memcpy(context.preparation.unit_area + i * 80u, state->unit_area, 80);
    context.preparation.unit_count = 17;
    fd2_save_manual_state before = context.preparation;
    controls(vga); pulse(1, SDL_SCANCODE_LEFT); pulse(3, SDL_SCANCODE_LEFT);
    pulse(4, SDL_SCANCODE_RETURN); pulse(10, SDL_SCANCODE_RETURN); quit_frame = 16;
    CHECK(fd2_field_prepare_run(vga, fdother, &context, "unused.SAV", NULL) == FD2_FIELD_PREPARE_HOST_QUIT);
    CHECK(memcmp(&context.preparation, &before, sizeof(before)) == 0 && context.preparation.unit_count == 17);
    CHECK(music_count == 2 && music_tracks[0] == 10 && music_tracks[1] == -1);
    /* >16现在进入真实选阵而非人数拒绝；直接验证12帧打开/关闭保持完整17人。
     * 正式run上面的宿主退出仍不重排，不能把它当作选满提交的覆盖。 */
    prepare_ui selection_ui={vga,fdother,a,&context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    selection_ui.service.quota=16;selection_ui.service.cursor=0;
    CHECK(fd2_field_prepare_assets_bind_roster(a,&context.preparation)==0);
    controls(vga);
    CHECK(prepare_service_selection_open(&selection_ui)==0 && selection_ui.service.page_kind==3 && frame_number==12u);
    CHECK(!memcmp(&context.preparation,&before,sizeof(before)) && context.preparation.unit_count==17);
    CHECK(prepare_service_selection_close(&selection_ui)==0 && frame_number==25u &&
          !memcmp(&context.preparation,&before,sizeof(before)));
    prepare_service_cleanup(&selection_ui,1);
    context.preparation = *state; context.music_enabled=state->options[2];context.sfx_enabled=state->options[3];
    context.camera_option=state->options[0];context.effect_option=state->options[1];
    CHECK(fd2_field_prepare_assets_bind_roster(a,&context.preparation)==0);
    prepare_ui ui = {vga, fdother, a, &context, NULL, "unused.SAV", -1, 0, -1, .service = {0}};
    controls(vga); pulse(5, SDL_SCANCODE_RIGHT); pulse(7, SDL_SCANCODE_RIGHT); pulse(9, SDL_SCANCODE_RIGHT);
    pulse(11, SDL_SCANCODE_RETURN); pulse(22, SDL_SCANCODE_RETURN);
    CHECK(hotel_run(&ui) == 3); /* yes=真实离开，不是回标题或回城镇。 */
    CHECK(memcmp(&context.preparation, state, sizeof(*state)) == 0);
    return 0;
}

static int member_pages(fd2_archive *fdother, fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state, fd2_vga *vga) {
    fd2_field_play_context context = {0}; context.initialized = context.has_preparation = 1;
    context.preparation = *state; context.music_enabled=state->options[2];context.sfx_enabled=state->options[3]; context.experience_value = 321;
    context.camera_option=state->options[0];context.effect_option=state->options[1];
    prepare_ui ui = {vga, fdother, a, &context, NULL, "unused.SAV", -1, 0, -1, .service = {0}};
    fd2_pcm_player player={0};fd2_pcm_bank bank={0};fd2_field_audio audio={0};
    audio.player=&player;audio.ui_bank=&bank;ui.audio=&audio;
    uint8_t before[VGA_W * VGA_H];
    memset(vga->framebuffer, 0x63, sizeof(vga->framebuffer)); memcpy(before, vga->framebuffer, sizeof(before));
    controls(vga); pulse(13, SDL_SCANCODE_ESCAPE); /* 17ee8在第12帧后清队。 */
    CHECK(member_detail(&ui, 0) == 0);
    CHECK(sound_calls==4u && sound_samples[0]==5u && sound_samples[1]==5u &&
        sound_samples[2]==6u && sound_samples[3]==6u);
    CHECK(memcmp(before, vga->framebuffer, sizeof(before)) == 0);
    controls(vga); pulse(13, SDL_SCANCODE_ESCAPE); pulse(27, SDL_SCANCODE_ESCAPE);
    CHECK(member_detail(&ui, 1) == 0); /* unit9必须经过未选中MAGIC页再关闭。 */
    CHECK(memcmp(before, vga->framebuffer, sizeof(before)) == 0);
    CHECK(memcmp(&context.preparation, state, sizeof(*state)) == 0 && context.experience_value == 321);
    context.preparation.unit_area[0x0a] = 0;
    context.preparation.unit_area[0x0b] = 255;
    fd2_save_manual_state invalid = context.preparation;
    controls(vga);
    CHECK(member_detail(&ui, 0) == -1 && frame_number == 0);
    CHECK(memcmp(before, vga->framebuffer, sizeof(before)) == 0 &&
          memcmp(&context.preparation, &invalid, sizeof(invalid)) == 0);
    return 0;
}

static int late_failures(fd2_archive *fdother, fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state, fd2_vga *vga) {
    char path[FD2_TEST_PATH_MAX]; CHECK(fd2_test_temp_path(path) == 0); CHECK(remove(path) == 0);
    fd2_save_manual_state older = *state; older.gold_or_flags++;
    CHECK(fd2_save_manual_state_save(path, 0, &older) == 0);
    uint8_t before[FD2_SAVE_FILE_SIZE], after[FD2_SAVE_FILE_SIZE];
    CHECK(read_container(path, before) == 0);
    fd2_field_play_context context = {0}; context.initialized = context.has_preparation = 1;
    context.preparation = *state; context.music_enabled=state->options[2];context.sfx_enabled=state->options[3]; context.experience_value = 321;
    context.camera_option=state->options[0];context.effect_option=state->options[1];
    fd2_pcm_player player = {0}; fd2_pcm_bank bank = {0};
    fd2_field_audio audio = {0}; audio.player = &player; audio.ui_bank = &bank;
    prepare_ui ui = {vga, fdother, a, &context, &audio, path, -1, 0, -1, .service = {0}};
    /* 首次读键已经成功后，关页／翻页失败仍须返回ERROR，不能沿用0。
     * 等待RNG是合法UI副作用；这里锁准备表、贡献owner与零storage提交。 */
    for (unsigned fault = 0; fault < 5; fault++) {
        controls(vga); pulse(13, SDL_SCANCODE_ESCAPE); pulse(27, SDL_SCANCODE_ESCAPE);
        if (fault < 4) {
            const unsigned late_cues[4]={3u,3u,4u,5u};
            fail_sound_call=late_cues[fault]; /* 新两次open cue后，关全页/翻页才失败。 */
        }
        else fail_magic_draw_call = 2;
        CHECK(member_detail(&ui, fault == 0 ? 0u : 1u) == -1);
        /* 关闭cue在phase0而非phase1：它失败时尚未present首个关闭帧。 */
        const unsigned error_frames[5]={13u,13u,21u,28u,21u};
        if(frame_number!=error_frames[fault] || vga->input.quit_requested || clear_calls!=1u || frames_at_clear[0]!=12u)
            fprintf(stderr,"detail late fault=%u frames=%u expected=%u quit=%u clear=%u at=%u sounds=%u magic=%u\n",
                fault,frame_number,error_frames[fault],vga->input.quit_requested,clear_calls,frames_at_clear[0],sound_calls,magic_draw_calls);
        CHECK(frame_number==error_frames[fault]);
        CHECK(!vga->input.quit_requested);
        CHECK(clear_calls==1u && frames_at_clear[0]==12u);
        CHECK(fault<4 ? sound_calls==fail_sound_call:magic_draw_calls==2u);
        CHECK(state_save_calls == 0 && music_count == 0);
        CHECK(context.has_preparation && context.experience_value == 321 &&
              memcmp(&context.preparation, state, sizeof(*state)) == 0);
        CHECK(read_container(path, after) == 0 && memcmp(before, after, sizeof(before)) == 0);
    }
    ui.audio = NULL;
    controls(vga); pulse(6, SDL_SCANCODE_RETURN); pulse(13, SDL_SCANCODE_ESCAPE);
    fail_frame_allocation_at = 13; /* Save与660确认成功，重开picker才分配失败。 */
    CHECK(slots_run(&ui, 1, 0) == -1 && frame_number == 13);
    CHECK(state_save_calls == 1);
    fd2_save_manual_state loaded;
    CHECK(fd2_save_manual_state_load(path, 0, &loaded) == 0 &&
          memcmp(&loaded, state, sizeof(loaded)) == 0); /* 已成功Save不回滚。 */
    CHECK(context.has_preparation && context.experience_value == 321 &&
          memcmp(&context.preparation, state, sizeof(*state)) == 0);
    CHECK(read_container(path, before) == 0);
    controls(vga); pulse(6, SDL_SCANCODE_RETURN); pulse(25, SDL_SCANCODE_ESCAPE);
    /* 详情24帧打开末清队，25读ACK；frame38重开名单的首2810b拒绝。
     * widget不再私有calloc整页；不能用失效的new_frame分配hook代caller错误。 */
    fail_members_draw = 38;
    CHECK(roster_run(&ui) == -1 && frame_number == 38 && !vga->input.quit_requested);
    CHECK(clear_calls==1u && frames_at_clear[0]==24u);
    CHECK(state_save_calls == 0 && context.has_preparation && context.experience_value == 321 &&
          memcmp(&context.preparation, state, sizeof(*state)) == 0);
    CHECK(read_container(path, after) == 0 && memcmp(before, after, sizeof(before)) == 0);
    CHECK(remove(path) == 0);
    return 0;
}

static int stage2_preparation_routes(fd2_archive *fdother, fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *stage1, fd2_vga *vga) {
    fd2_save_manual_state state = *stage1;
    state.stage_id = 2;
    fd2_field_unit added = {0};
    CHECK(fd2_field_unit_persistent_create(&added, 8) == 0);
    CHECK(state.unit_count == 4); /* 本UI fixture来源START完整roster，不冒自然结算六人记录。 */
    memcpy(state.unit_area + state.unit_count * 80u, &added, sizeof(added)); state.unit_count++;
    CHECK(state.unit_count == 5);
    CHECK(fd2_field_game_validate_manual_state(&state) == 0);
    CHECK(title_cancel_and_load(fdother, &state, vga) == 0);

    fd2_field_play_context context = {0}; context.initialized = context.has_preparation = 1;
    context.preparation = state; context.music_enabled=state.options[2];context.sfx_enabled=state.options[3]; context.experience_value = 0xfeed1234u;
    context.camera_option=state.options[0];context.effect_option=state.options[1];
    context.message_number = 123456; fd2_field_rng_seed(&context.rng, 0x4321);
    fd2_field_play_context before = context;
    controls(vga); quit_frame = 1;
    CHECK(fd2_field_prepare_run(vga,fdother,&context,"unused.SAV",NULL) == FD2_FIELD_PREPARE_HOST_QUIT);
    CHECK(!memcmp(&context,&before,sizeof(context)));
    CHECK(music_count == 1 && music_tracks[0] == 10 && music_stages[0] == 2);

    controls(vga); pulse(1,SDL_SCANCODE_LEFT); pulse(3,SDL_SCANCODE_LEFT);
    pulse(4,SDL_SCANCODE_RETURN);confirm_probe_assets=assets;
    CHECK(fd2_field_prepare_run(vga,fdother,&context,"unused.SAV",NULL) == FD2_FIELD_PREPARE_DEPART);
    CHECK(confirm_probe_hits==1u);
    fd2_field_play_context expected_depart = before;
    (void)fd2_field_rng_next(&expected_depart.rng); /* choice新等待入口的唯一一取；关闭段不再取。 */
    CHECK(expected_depart.rng.state == 0x99aeu);
    printf("stage2 depart RNG: before=%04x after=%04x expected=%04x\n",
        before.rng.state, context.rng.state, expected_depart.rng.state);
    CHECK(!memcmp(&context,&expected_depart,sizeof(context))); /* 仅已证等待RNG变化，无owner交接。 */
    CHECK(music_count == 2 && music_tracks[0] == 10 && music_tracks[1] == -1 &&
          music_stages[0] == 2 && music_stages[1] == 2);

    controls(vga); context.preparation.stage_id = 30; before = context;
    fd2_vga unchanged = *vga;
    CHECK(fd2_field_prepare_run(vga,fdother,&context,"unused.SAV",NULL) == FD2_FIELD_PREPARE_ERROR);
    CHECK(!frame_number && !music_count && !memcmp(&context,&before,sizeof(context)) &&
          !memcmp(vga,&unchanged,sizeof(*vga)));
    context.preparation = state; context.music_enabled=state.options[2];context.sfx_enabled=state.options[3];
    context.camera_option=state.options[0];context.effect_option=state.options[1];
    context.preparation.unit_area[7] = 255; before = context;
    controls(vga); unchanged = *vga;
    CHECK(fd2_field_prepare_run(vga,fdother,&context,"unused.SAV",NULL) == FD2_FIELD_PREPARE_ERROR);
    CHECK(!frame_number && !music_count && !memcmp(&context,&before,sizeof(context)) &&
          !memcmp(vga,&unchanged,sizeof(*vga)));

    CHECK(town_type(1) == 0 && town_type(2) == 2 && town_type(3) >= 0 && town_type(30) == -1);
    CHECK(prepare_town(assets,fdother,2) == 0);
    fd2_image source62 = {0}; CHECK(fd2_image_decode_entry(&source62,fdother,62) == 0);
    CHECK(image_size(&source62,320,200) && !memcmp(source62.pixels,assets->town_stage2.pixels,64000));
    static const int x[5] = {59,10,59,130,242}, y[5] = {26,144,163,150,31};
    for (int selected=0;selected<5;selected++) {
        fd2_vga expected={0}, actual={0};
        for (size_t row=0;row<192;row++) memcpy(expected.framebuffer+(row+4)*320+4,source62.pixels+row*320,312);
        fd2_map_sprite_blit(&expected,&assets->town_panel,248,166,-1);
        CHECK(text_draw(&expected,assets,(size_t)selected+495,256,172,205,74,0) == 0);
        fd2_map_sprite_blit_frame(&expected,&assets->sprites,(size_t)state.unit_area[7]*12+1,x[selected]+4,y[selected]+4,0);
        CHECK(fd2_field_prepare_draw_town(&actual,assets,&state,selected,1) == 0);
        CHECK(!memcmp(actual.framebuffer,expected.framebuffer,sizeof(actual.framebuffer)));
    }
    fd2_image_free(&source62);
    /* 缺无关62不影响stage1；stage2背景失败必须发生在Load准备owner交接前。 */
    fd2_image_free(&assets->town_stage2);
    uint32_t saved_offset=fdother->offsets[63]; fdother->offsets[63]=fdother->offsets[62];
    CHECK(prepare_town(assets,fdother,1) == 0 && prepare_town(assets,fdother,2) == -1 && !assets->town_stage2.pixels);
    char path[FD2_TEST_PATH_MAX]; CHECK(fd2_test_temp_path(path) == 0 && remove(path) == 0);
    CHECK(fd2_save_manual_state_save(path,0,&state) == 0);
    context.preparation=*stage1; context.has_preparation=1;
    context.music_enabled=stage1->options[2];context.sfx_enabled=stage1->options[3];
    context.camera_option=stage1->options[0];context.effect_option=stage1->options[1];before=context;
    prepare_ui ui={vga,fdother,assets,&context,NULL,path,-1,0,-1, .service = {0}};
    controls(vga); pulse(6,SDL_SCANCODE_RETURN);
    CHECK(slots_run(&ui,0,1) == -1);
    CHECK(context.has_preparation && !memcmp(&context.preparation,&before.preparation,sizeof(before.preparation)) &&
          context.experience_value==before.experience_value && context.rng.state==before.rng.state);
    fdother->offsets[63]=saved_offset;
    CHECK(remove(path) == 0 && prepare_town(assets,fdother,2) == 0);
    return 0;
}

static int hotel_load_keeps_sequence(fd2_archive *fdother, fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state, fd2_vga *vga) {
    char path[FD2_TEST_PATH_MAX]; CHECK(fd2_test_temp_path(path)==0 && remove(path)==0);
    fd2_save_manual_state loaded=*state; loaded.options[2]=!state->options[2]; loaded.options[3]=!state->options[3];
    loaded.options[0]=0xfe;loaded.options[1]=0x7b;
    CHECK(fd2_save_manual_state_save(path,0,&loaded)==0);
    uint8_t before[FD2_SAVE_FILE_SIZE],after[FD2_SAVE_FILE_SIZE]; CHECK(read_container(path,before)==0);
    fd2_field_play_context context={0};context.initialized=context.has_preparation=1;
    context.preparation=*state;context.music_enabled=state->options[2];context.sfx_enabled=state->options[3];context.bgm=(fd2_bgm_player *)&context;context.experience_value=99;
    context.camera_option=state->options[0];context.effect_option=state->options[1];
    prepare_ui ui={vga,fdother,assets,&context,NULL,path,-1,0,-1, .service = {0}};
    controls(vga);option_sync_count=0;
    CHECK(music(&ui,13,"hotel")==0);
    pulse(6,SDL_SCANCODE_RETURN);pulse(13,SDL_SCANCODE_RETURN);
    CHECK(slots_run(&ui,0,0)==2);
    CHECK(option_sync_count==2 && option_sync_enabled==(loaded.options[2]!=0) && option_sync_fade==0);
    CHECK(context.music_enabled==(loaded.options[2]!=0) && context.sfx_enabled==(loaded.options[3]!=0));
    CHECK(context.camera_option==0xfe && context.effect_option==0x7b);
    CHECK(music_count==1 && music_tracks[0]==13); /* Load不是退出酒店，不得发-1/10/13。 */
    CHECK(!memcmp(&context.preparation,&loaded,sizeof(loaded)) && context.experience_value==99);
    CHECK(read_container(path,after)==0 && !memcmp(before,after,sizeof(before)));
    context.preparation.options[2]=2;context.preparation.options[3]=3;
    context.preparation.options[0]=0x81;context.preparation.options[1]=0x72;
    CHECK(sync_context_options(&ui)==0 && context.music_enabled==2 && context.sfx_enabled==3 &&
        context.camera_option==0x81 && context.effect_option==0x72);
    CHECK(option_sync_enabled==1 && option_sync_fade==0 && music_count==1);
    CHECK(remove(path)==0);return 0;
}

static int hotel_notown_and_cache_boundary(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    char path[FD2_TEST_PATH_MAX];CHECK(fd2_test_temp_path(path)==0 && remove(path)==0);
    fd2_field_play_context context={0};context.initialized=context.has_preparation=1;
    context.preparation=*state;context.experience_value=0x12345678u;fd2_field_rng_seed(&context.rng,0x4321);
    prepare_ui ui={vga,fdother,assets,&context,NULL,path,-1,0,-1, .service = {0}};
    CHECK(fd2_field_prepare_assets_bind_roster(assets,state)==0);
    uint8_t cache_before[FD2_SAVE_MAX_UNITS];memcpy(cache_before,assets->sprite_cache_ids,sizeof(cache_before));
    size_t count_before=assets->sprite_cache_count;int ready_before=assets->sprite_cache_ready;
    fd2_save_manual_state notown=*state;notown.stage_id=27;
    CHECK(fd2_field_game_validate_manual_state(&notown)==0 && prepare_is_notown(27));
    CHECK(fd2_save_manual_state_save(path,0,&notown)==0);
    controls(vga);pulse(6,SDL_SCANCODE_RETURN);pulse(13,SDL_SCANCODE_ESCAPE);pulse(19,SDL_SCANCODE_ESCAPE);
    CHECK(slots_run(&ui,0,0)==0 && !memcmp(&context.preparation,state,sizeof(*state)));
    CHECK(assets->sprite_cache_count==count_before && assets->sprite_cache_ready==ready_before &&
        !memcmp(cache_before,assets->sprite_cache_ids,sizeof(cache_before)) && context.experience_value==0x12345678u);
    controls(vga);pulse(6,SDL_SCANCODE_RETURN);uint16_t rng_before=context.rng.state;
    CHECK(slots_run(&ui,0,1)==2 && !memcmp(&context.preparation,&notown,sizeof(notown)) &&
        context.rng.state==rng_before && context.experience_value==0x12345678u);
    /* 同一容器换真实ID顺序，Load成功时先bind再交接；未知80B字节不重造。 */
    fd2_save_manual_state swapped=*state;uint8_t temp[80];
    memcpy(temp,swapped.unit_area,80);memcpy(swapped.unit_area,swapped.unit_area+80,80);memcpy(swapped.unit_area+80,temp,80);
    CHECK(fd2_save_manual_state_save(path,0,&swapped)==0);
    controls(vga);pulse(6,SDL_SCANCODE_RETURN);pulse(13,SDL_SCANCODE_RETURN);
    CHECK(slots_run(&ui,0,0)==2 && !memcmp(&context.preparation,&swapped,sizeof(swapped)));
    CHECK(assets->sprite_cache_ready && assets->sprite_cache_count>=2u &&
        assets->sprite_cache_ids[0]==state->unit_area[80u+7u] && assets->sprite_cache_ids[1]==state->unit_area[7]);
    CHECK(context.experience_value==0x12345678u && fd2_field_prepare_assets_bind_roster(assets,state)==0);
    CHECK(remove(path)==0);return 0;
}

/* 默认幅度来自原40240两输出表，不跟随生产macro计算期望。 */
static int default_sound_gain(void) {
    fd2_pcm_player player = {0}; fd2_pcm_bank bank = {0};
    fd2_field_audio audio = {0}; fd2_field_play_context context = {0};
    audio.player = &player; audio.ui_bank = &bank;
    prepare_ui ui = {0, .service = {0}}; ui.audio = &audio; ui.context = &context;
    sound_calls = fail_sound_call = 0; sound_last_gain = 0;
    CHECK(sound(&ui, 5) == 0);
    CHECK(sound_calls == 1 && sound_last_gain == 101.0f / 128.0f);
    context.has_preparation = 1; context.preparation.options[3] = 0;
    CHECK(sound(&ui, 6) == 0 && sound_calls == 1);
    context.preparation.options[3] = 1;
    CHECK(sound(&ui, 6) == 0 && sound_calls == 2 &&
          sound_last_gain == 101.0f / 128.0f);
    sound_calls = 0;
    return 0;
}

/* 原服务页用两种真实容器；显式原目录索引独立读取，覆盖资源切换与晚坏资源。
 * 这里只验证资源/CPU页面/owner，不声称正式服务输入或原版WGC通过。 */
static int service_resource_pages(fd2_archive *fdother, fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state, fd2_vga *vga) {
    static const int selectors[] = {1, 3, 4, 5};
    static const int resources[] = {12, 29, 14, 63};
    static const int portraits[] = {128, 130, 131, 132};
    static const unsigned origins[] = {4283, 3939, 1398, 3644};
    fd2_save_manual_state before = *state;
    static const struct { int32_t value; size_t count; const char *digits; } numbers[] = {
        {-1,5,"00000"}, {123456,5,"12345"}, {INT32_MAX,8,"21474836"}, {99,2,"99"}
    };
    for (size_t bank = 0; bank < 3; bank++) {
        for (size_t n = 0; n < sizeof(numbers)/sizeof(numbers[0]); n++) {
            fd2_vga actual = {0}, expected = {0};
            for (size_t i = 0; i < numbers[n].count; i++) fd2_map_sprite_blit(&expected,
                &a->info.digits[bank][numbers[n].digits[i]-'0'], 10+(int)i*6,20,0);
            CHECK(fd2_field_info_number_draw(&actual,&a->info,10,20,numbers[n].value,numbers[n].count,bank) == 0 &&
                  !memcmp(actual.framebuffer,expected.framebuffer,64000));
        }
        fd2_vga actual = {0}, expected = {0};
        fd2_map_sprite_blit(&expected,&a->info.digit_overflow[bank],10,20,0);
        CHECK(fd2_field_info_number_draw(&actual,&a->info,10,20,1000,3,bank) == 0 &&
              !memcmp(actual.framebuffer,expected.framebuffer,64000));
    }
    fd2_vga invalid = *vga;
    CHECK(fd2_field_info_number_draw(vga,&a->info,318,20,10,5,0) == -1 &&
          !memcmp(vga,&invalid,sizeof(*vga)));
    for (size_t k = 0; k < 4; k++) {
        const uint8_t *raw; size_t size;
        CHECK(fd2_archive_get(fdother, (size_t)resources[k], &raw, &size) == 0 && size > 10);
        size_t start = read32(raw + 6), end = read32(raw + 10);
        fd2_image original = {0}, portrait = {0};
        CHECK(start < end && end <= size && fd2_image_decode_buf(&original, raw + start, end - start) == 0);
        CHECK(fd2_portrait_decode_frame(&portrait, &a->dato, (size_t)portraits[k], 0) == 0);
        CHECK(fd2_field_prepare_assets_open_service(a, fdother, selectors[k]) == 0);
        CHECK(a->service.ready && a->service.selector == selectors[k]);
        CHECK(!memcmp(original.pixels, a->service.background.pixels, 64000) &&
              !memcmp(portrait.pixels, a->service.portraits[0].pixels, 6400));
        fd2_vga actual = {0}, expected = {0};
        fd2_map_sprite_blit(&expected, &original, 0, 0, 0);
        fd2_map_sprite_blit(&expected, &a->service.money_panel, 5, 95, -1);
        char number[11];
        unsigned value = state->gold_or_flags <= INT32_MAX ? state->gold_or_flags : 0u;
        (void)snprintf(number, sizeof(number), "%08u", value);
        for (size_t d = 0; d < 8; d++) fd2_map_sprite_blit(&expected,
            &a->info.digits[0][number[d] - '0'], 16 + (int)d * 6, 99, 0);
        fd2_map_sprite_blit(&expected, &portrait, (int)(origins[k] % 320u), (int)(origins[k] / 320u), -1);
        CHECK(fd2_scene_field_message_box(&expected, fdother) == 0);
        CHECK(text_draw(&expected, a, 440, 12, 119, 205, 74, 0) == 0);
        CHECK(fd2_field_prepare_draw_service(&actual, a, fdother, state, -1, 0, 0, 440) == 0);
        CHECK(!memcmp(actual.framebuffer, expected.framebuffer, 64000) && !memcmp(state, &before, sizeof(before)));
        for (int selected = 0; selected < 4; selected++) {
            CHECK(fd2_field_prepare_draw_service(&actual, a, fdother, state, selected, 1, 3, 440) == 0);
            CHECK(!memcmp(state, &before, sizeof(before)));
        }
        fd2_image_free(&original); fd2_image_free(&portrait);
    }
    fd2_field_prepare_service_assets saved = a->service;
    CHECK(fd2_field_prepare_assets_open_service(a, fdother, 2) == -1 &&
          !memcmp(&saved, &a->service, sizeof(saved)));
    uint32_t offset = fdother->offsets[13];
    fdother->offsets[13] = fdother->offsets[12] + 15u;
    CHECK(fd2_field_prepare_assets_open_service(a, fdother, 1) == -1 &&
          !memcmp(&saved, &a->service, sizeof(saved)));
    fdother->offsets[13] = offset;
    fd2_vga unchanged = *vga;
    CHECK(fd2_field_prepare_draw_service(vga, a, fdother, state, 4, 0, 0, 440) == -1 &&
          !memcmp(vga, &unchanged, sizeof(*vga)));
    fd2_save_manual_state negative = *state; negative.gold_or_flags = UINT32_MAX;
    fd2_save_manual_state zero = *state; zero.gold_or_flags = 0;
    fd2_vga negative_page = {0}, zero_page = {0};
    CHECK(fd2_field_prepare_draw_service(&negative_page, a, fdother, &negative, -1, 0, 0, 440) == 0 &&
          fd2_field_prepare_draw_service(&zero_page, a, fdother, &zero, -1, 0, 0, 440) == 0 &&
          !memcmp(negative_page.framebuffer, zero_page.framebuffer, 64000));
    return 0;
}

static int service_repaints;
static int service_test_redraw(prepare_ui *ui) {
    service_repaints++;ui->vga->framebuffer[0]=0x69;return 0;
}
static int service_scroll_and_inventory_prices(fd2_archive *fdother,
        fd2_field_prepare_assets *assets,const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context)),*before=malloc(sizeof(*before));
    fd2_vga *expected=malloc(sizeof(*expected));uint8_t *source=malloc(VGA_W*VGA_H);
    CHECK(context&&before&&expected&&source);
    context->initialized=context->has_preparation=1;context->preparation=*state;
    fd2_field_rng_seed(&context->rng,0x5678u);
    prepare_ui ui={0, .service = {0}};ui.vga=vga;ui.fdother=fdother;ui.assets=assets;ui.context=context;
    static const unsigned origins[]={1832u,36887u,4283u,3208u,0u};
    for(size_t n=0;n<sizeof(origins)/sizeof(origins[0]);n++) {
        controls(vga);service_exact_time=1;ui.service.notice_origin=origins[n];
        for(size_t i=0;i<VGA_W*VGA_H;i++)vga->framebuffer[i]=(uint8_t)(i*17u+i/320u);
        *expected=*vga;*before=*context;prepare_service old_service=ui.service;
        if(n<2u) {
            int x=n==0u ? 14:94,y=n==0u ? 9:119;
            /* 独立每轮快照，覆盖原五次3px/一次4px及仅一行底清的完整写集。 */
            for(int step=0;step<6;step++) {
                memcpy(source,expected->framebuffer,VGA_W*VGA_H);
                for(int row=0;row<72;row++)memcpy(expected->framebuffer+(y+row)*320+x,
                    source+(y+row+(step<5 ? 3:4))*320+x,208);
                memset(expected->framebuffer+(y+72)*320+x+1,74,208);
            }
        }
        /* 就绪键与quit不得在原无IO叶中被消费。 */
        fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,1);
        fd2_input_begin_test_frame(&vga->input,clock_ms);expected->input=vga->input;
        quit_frame=1;
        CHECK(prepare_service_scroll(&ui)==0 && !memcmp(vga,expected,sizeof(*vga)) &&
            !memcmp(context,before,sizeof(*before)) && !memcmp(&ui.service,&old_service,sizeof(old_service)) &&
            !frame_number && !service_delay_count && !clock_ms && !sound_calls);
    }
    CHECK(fd2_field_prepare_assets_open_service(assets,fdother,1)==0);
    const uint8_t ids[]={20,0};const uint8_t *first=fd2_field_item_record_get(ids[0]);
    const uint8_t *second=fd2_field_item_record_get(ids[1]);CHECK(first&&second);
    CHECK(read16(first+19)==80u && read16(second+19)==50u);
    for(int kind=0;kind<2;kind++) {
        controls(vga);memset(vga->framebuffer,0x61,VGA_W*VGA_H);
        fd2_map_sprite_blit(vga,&assets->service.roster_page,5,112,-1);*expected=*vga;
        ui.service=(prepare_service){0};ui.service.picker_mode=PREPARE_PICK_ITEM;
        ui.service.item_kind=kind;ui.service.item_count=2;ui.service.raw_slot=0;
        memcpy(ui.service.visible_items,ids,sizeof(ids));prepare_service old_service=ui.service;*before=*context;
        /* 从原价页独立覆盖两数：80*3/4=60，50*3/4=37；两kind不改变物品写集。 */
        CHECK(fd2_field_prepare_draw_items(expected,assets,ids,2,0,0,0)==0);
        /* 数字0色透明，先还原原底页的两个完整30x8格，不能叠在80/50上。 */
        for(int row=0;row<8;row++)for(int column=0;column<2;column++) {
            int x=114+148*column;
            memcpy(expected->framebuffer+(131+row)*320+x,
                assets->service.roster_page.pixels+(19+row)*310+x-5,30);
        }
        CHECK(fd2_field_info_number_draw(expected,&assets->info,114,131,60,5,2)==0 &&
            fd2_field_info_number_draw(expected,&assets->info,262,131,37,5,2)==0);
        fd2_map_sprite_blit(expected,&assets->service.scroll_arrows[4],298,120,-1);
        fd2_map_sprite_blit(expected,&assets->service.scroll_arrows[4],298,181,-1);
        CHECK(prepare_service_picker_draw(&ui)==0 && !memcmp(vga,expected,sizeof(*vga)) &&
            !memcmp(context,before,sizeof(*before)) && !memcmp(&ui.service,&old_service,sizeof(old_service)) &&
            !frame_number && !service_delay_count && !sound_calls);
    }
    free(source);free(expected);free(before);free(context);return 0;
}
static int services_buy_reaches_products(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context)),*expected=malloc(sizeof(*expected));
    fd2_vga *products=calloc(1,sizeof(*products));CHECK(context&&expected&&products);
    context->initialized=context->has_preparation=1;context->preparation=*state;
    context->music_enabled=state->options[2];context->sfx_enabled=state->options[3];
    context->camera_option=state->options[0];context->effect_option=state->options[1];
    context->experience_value=0x12345678u;fd2_field_rng_seed(&context->rng,0x4321);
    CHECK(fd2_field_prepare_assets_open_service(assets,fdother,1)==0 &&
        fd2_field_prepare_assets_bind_roster(assets,state)==0);
    const uint8_t *row=fd2_field_stage_prepare_record_get(state->stage_id);CHECK(row);
    uint8_t stock[12];size_t count=0;
    while(count<12u && row[3u+count]!=255u){stock[count]=row[3u+count];count++;}
    fd2_map_sprite_blit(products,&assets->service.roster_page,5,112,-1);
    CHECK(count && fd2_field_prepare_draw_items(products,assets,stock,count,0,0,0)==0);
    fd2_map_sprite_blit(products,&assets->service.scroll_arrows[4],298,120,-1);
    fd2_map_sprite_blit(products,&assets->service.scroll_arrows[count>6u ? 3u:4u],298,181,-1);
    CHECK(fd2_field_prepare_draw_town(vga,assets,state,1,0)==0);
    prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    controls(vga);service_exact_time=1;service_route_assets=assets;service_route_products=products;
    *expected=*context;(void)fd2_field_rng_next(&expected->rng); /* 26eda菜单入口，仅一次；商品等待尚未开始。 */
    int result=prepare_service_execute(&ui,"services",NULL,1);
    printf("service route checkpoint: result=%d menu=%u products=%u frames=%u stock=%zu\n",
        result,service_route_menu_hits,service_route_product_hits,frame_number,ui.service.stock_count);
    CHECK(result==-2 && service_route_menu_hits==1u && service_route_product_hits==1u);
    CHECK(ui.service.host_quit && !ui.service.irreversible && ui.service.stock_count==count &&
        !memcmp(ui.service.stock,stock,count) && !memcmp(context,expected,sizeof(*expected)));
    CHECK(!ui.service.page && !ui.service.page_background && !ui.service.town_background &&
        !ui.service.active_portraits && !ui.service.promotion_visual);
    service_route_assets=NULL;service_route_products=NULL;
    free(products);free(expected);free(context);return 0;
}

static int service_growth_message_layouts(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context)),*before=malloc(sizeof(*before));
    fd2_vga *expected=malloc(sizeof(*expected));uint8_t *source=malloc(VGA_W*VGA_H);
    CHECK(context&&before&&expected&&source);
    context->initialized=context->has_preparation=1;context->preparation=*state;context->message_number=1;
    fd2_field_rng_seed(&context->rng,0x5678u);
    CHECK(fd2_field_prepare_assets_open_service(assets,fdother,1)==0);
    prepare_ui ui={0, .service = {0}};ui.vga=vga;ui.fdother=fdother;ui.assets=assets;ui.context=context;
    for(int kind=0;kind<2;kind++) {
        controls(vga);service_exact_time=1;ui.service=(prepare_service){0};
        ui.service.selector=1;ui.service.notice_origin=36887;ui.service.growth_line=3;
        ui.context->message_new_fragment=ui.context->message_old_fragment=0;
        ui.service.active_portraits=assets->service.portraits;
        for(size_t i=0;i<VGA_W*VGA_H;i++)vga->framebuffer[i]=(uint8_t)(i*11u+i/320u);
        *expected=*vga;*before=*context;
        if(kind) {
            for(int step=0;step<6;step++) {
                memcpy(source,expected->framebuffer,VGA_W*VGA_H);
                for(int row=0;row<72;row++)memcpy(expected->framebuffer+(119+row)*320+94,
                    source+(119+row+(step<5 ? 3:4))*320+94,208);
                memset(expected->framebuffer+191*320+95,74,208);
            }
        }
        /* 同片段分别传两个布局，防consumer按596偷偷分支：5在176，3先卷到157。 */
        CHECK(text_draw(expected,assets,596,95,kind ? 157:176,205,74,1)==0);
        fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,1);
        fd2_input_begin_test_frame(&vga->input,clock_ms);
        fd2_static_flow_node node={.op="show_message",.args={0,596,kind ? 3:5}};unsigned port=99;
        CHECK(prepare_service_ui(&ui,&node,&port)==1 && port==FD2_FLOW_NEXT &&
            !memcmp(vga->framebuffer,expected->framebuffer,VGA_W*VGA_H) &&
            ui.service.growth_line==(kind ? 2u:3u) && ui.service.input_epoch==0u &&
            frame_number==1u && !service_delay_count && !sound_calls && !memcmp(context,before,sizeof(*before)));
        fd2_input_event queued;
        CHECK(prepare_service_take_key(&ui,&queued) && confirm_key(queued.key) && ui.service.input_epoch==1u);
    }

    size_t setting_count;
    const fd2_setting *settings=fd2_settings_table(&setting_count);
    const fd2_setting *text_speed=NULL,*animation_speed=NULL;
    for(size_t i=0;i<setting_count;i++) {
        if(!strcmp(settings[i].key,"text_speed"))text_speed=&settings[i];
        if(!strcmp(settings[i].key,"animation_speed"))animation_speed=&settings[i];
    }
    CHECK(text_speed && animation_speed && fd2_setting_set(animation_speed,"200"));
    const fd2_text_parameters parameters={1,0,0};
    fd2_text_expanded expanded;
    CHECK(fd2_text_expand(&assets->text,assets->font.glyph_count,&parameters,596,&expanded)==0 &&
          expanded.count>0 && expanded.count<=SDL_arraysize(service_ticks));
    const char *const speeds[]={"normal","fast","instant"};
    for(size_t mode=0;mode<SDL_arraysize(speeds);mode++) {
        controls(vga);
        ui.service=(prepare_service){0};ui.service.selector=1;
        vga->dialog_visual=(fd2_dialog_visual_state){0};
        memset(vga->framebuffer,0x61,sizeof(vga->framebuffer));
        *expected=*vga;*before=*context;
        CHECK(fd2_setting_set(text_speed,speeds[mode]));
        CHECK(text_draw(expected,assets,596,95,119,205,74,1)==0);
        fd2_static_flow_node node={.op="show_message",.args={0,596,5}};unsigned port=99;
        CHECK(prepare_service_ui(&ui,&node,&port)==1 && port==FD2_FLOW_NEXT &&
            !memcmp(vga->framebuffer,expected->framebuffer,sizeof(vga->framebuffer)) &&
            !memcmp(context,before,sizeof(*before)) && !ui.service.input_epoch);
        CHECK(service_tick_count==(mode==0 ? expanded.count:0) &&
              service_delay_count==(mode==0 ? 0:expanded.count));
        for(size_t i=0;i<service_tick_count;i++)CHECK(service_ticks[i]==1);
        for(size_t i=0;i<service_delay_count;i++)CHECK(service_delays[i]==(mode==1 ? 15u:0u));
    }
    fd2_settings_defaults();
    free(source);free(expected);free(before);free(context);return 0;
}

/* G4：69op正式服务的confirm叶必须借runtime.choice，而不是local phase。
 * 真资源/真冷图/真App页；只替宿主采帧，早键归同一input owner。 */
static int service_confirm_shared_owner(fd2_archive *fdother,fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context context={0};context.initialized=context.has_preparation=1;
    context.preparation=*state;
    CHECK(fd2_field_prepare_assets_open_service(a,fdother,1)==0);
    for(unsigned kind=0;kind<3;kind++) {
        prepare_ui ui={0, .service = {0}};ui.vga=vga;ui.fdother=fdother;ui.assets=a;ui.context=&context;
        ui.service.active_portraits=a->service.portraits;ui.service.notice_origin=36887;
        controls(vga);service_exact_time=1;memset(vga->framebuffer,0x61,64000);
        vga->menu_highlight_phase=3;vga->menu_highlight_tick=0x1234;
        fd2_field_rng_seed(&context.rng,0x4321);fd2_field_rng expected=context.rng;
        (void)fd2_field_rng_next(&expected);
        fd2_save_manual_state before=context.preparation;
        if(!kind) {
            fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,1);
            fd2_input_begin_test_frame(&vga->input,0);ui.service.input_epoch=7;
        } else if(kind==1) {pulse(4,SDL_SCANCODE_RIGHT);pulse(5,SDL_SCANCODE_RETURN);}
        else pulse(4,SDL_SCANCODE_ESCAPE);
        unsigned port=FD2_FLOW_ERROR;
        CHECK(prepare_service_confirm(&ui,&port)==0);
        CHECK(port==(unsigned)(!kind?FD2_FLOW_YES:kind==1?FD2_FLOW_NO:FD2_FLOW_CANCEL));
        CHECK(!fd2_input_has_any_key(&vga->input) && ui.service.input_epoch==(!kind?8u:kind==1?2u:1u));
        CHECK(context.rng.state==expected.state && !memcmp(&context.preparation,&before,sizeof(before)));
        /* finite四次不再耗64ms：kind1首空轮0<1234触门，fresh锚仍0；
         * 第5次是唯一H16ms采样，不能把旧合成开框时间当原BIOS写值。 */
        CHECK(!vga->menu_highlight_phase && vga->menu_highlight_tick==(kind==1?0u:0x1234u));
        CHECK(clock_ms==(kind==1?16u:0u));
        uint8_t corner=0;CHECK(fd2_field_page_read(vga->field_page,68,68,1,1,&corner,1,1)==0 && corner==0x61);
        CHECK(!ui.service.page && !ui.service.page_background && !ui.service.irreversible);
    }
    return 0;
}

static int service_present_leaves(fd2_archive *fdother,fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context)),*before=malloc(sizeof(*before));
    fd2_vga *expected=calloc(1,sizeof(*expected));CHECK(context&&before&&expected);
    context->initialized=context->has_preparation=1;context->preparation=*state;
    prepare_ui ui={0, .service = {0}};ui.vga=vga;ui.assets=a;ui.fdother=fdother;ui.context=context;
    ui.menu_selected=ui.roster_selected=-1;ui.context->message_new_fragment=ui.context->message_old_fragment=0;
    static const int selectors[4]={1,3,4,5};
    for(size_t k=0;k<4;k++) {
        CHECK(fd2_field_prepare_assets_open_service(a,fdother,selectors[k])==0);
        controls(vga);service_exact_time=1;memset(vga->framebuffer,0x61,64000);
        ui.service.previous_money=12345678;context->preparation.gold_or_flags=12345679;
        CHECK(fd2_field_info_number_draw(vga,&a->info,16,99,12345678,8,0)==0);
        memcpy(expected->framebuffer,vga->framebuffer,64000);service_capture_first=1;*before=*context;
        CHECK(prepare_service_money_animate(&ui,0)==0 && service_delay_count==9u);
        for(size_t i=0;i<9u;i++)CHECK(service_delays[i]==10u);
        for(size_t row=0;row<9u;row++)memcpy(expected->framebuffer+(98u+row)*320u+58u,
            a->service.money_strip.pixels+6u*(73u+row),6);
        CHECK(!memcmp(service_first_frame,expected->framebuffer,64000) && !memcmp(context,before,sizeof(*before)));
        for(size_t row=0;row<9u;row++)memcpy(expected->framebuffer+(98u+row)*320u+58u,
            a->service.money_strip.pixels+6u*(81u+row),6);
        CHECK(fd2_field_info_number_draw(expected,&a->info,16,99,12345679,8,0)==0);
        CHECK(!memcmp(vga->framebuffer,expected->framebuffer,64000));
        controls(vga);service_exact_time=1;ui.service.previous_money=12345679;context->preparation.gold_or_flags=12345678;*before=*context;
        CHECK(prepare_service_money_animate(&ui,1)==0 && service_delay_count==9u && !memcmp(context,before,sizeof(*before)));
    }
    controls(vga);service_exact_time=1;ui.service.previous_money=INT32_MAX;
    context->preparation.gold_or_flags=UINT32_C(0x80000000);*before=*context;
    CHECK(prepare_service_money_animate(&ui,0)==0 && service_delay_count<=81u && !memcmp(context,before,sizeof(*before)));
    controls(vga);service_exact_time=1;ui.service.previous_money=UINT32_MAX;
    context->preparation.gold_or_flags=UINT32_MAX-1u;*before=*context;
    CHECK(prepare_service_money_animate(&ui,1)==0 && !service_delay_count && !memcmp(context,before,sizeof(*before)));
    controls(vga);service_exact_time=1;ui.service.previous_money=1;context->preparation.gold_or_flags=2;*before=*context;
    uint8_t *saved=a->service.money_strip.pixels;a->service.money_strip.pixels=NULL;
    memcpy(expected->framebuffer,vga->framebuffer,64000);
    CHECK(prepare_service_money_animate(&ui,0)==-1 && !memcmp(expected->framebuffer,vga->framebuffer,64000));
    a->service.money_strip.pixels=saved;
    quit_frame=1;CHECK(prepare_service_money_animate(&ui,0)==1 && ui.service.host_quit && !memcmp(context,before,sizeof(*before)));
    prepare_service_cleanup(&ui,1);ui.service=(prepare_service){0};

    /* 等键原入口RNG及准备无map箭头x143；一键快显同时完成文本内部-3 ACK。 */
    controls(vga);service_exact_time=1;service_capture_first=1;memset(vga->framebuffer,0x61,64000);
    fd2_field_rng_seed(&context->rng,0x7a18);fd2_field_rng expected_rng=context->rng;
    (void)fd2_field_rng_next(&expected_rng);pulse(1,SDL_SCANCODE_RETURN);
    fd2_input_event event;CHECK(prepare_service_wait_key(&ui,&event,1)==0 && confirm_key(event.key));
    CHECK(context->rng.state==expected_rng.state && ui.service.input_epoch==1u);
    const uint8_t *raw;size_t length;CHECK(fd2_archive_get(fdother,5,&raw,&length)==0);
    size_t offset=read32(raw+6u+18u*4u);int w=read16(raw+offset),h=read16(raw+offset+2u);
    for(int row=0;row<h;row++)CHECK(!memcmp(service_first_frame+(181+row)*320+143,raw+offset+4u+(size_t)row*(size_t)w,(size_t)w));
    controls(vga);service_exact_time=1;fd2_field_rng_seed(&context->rng,0x7a18);expected_rng=context->rng;
    (void)fd2_field_rng_next(&expected_rng);vga->prepare_sprite_phase=0;service_repaints=0;pulse(10,SDL_SCANCODE_DOWN);
    CHECK(prepare_service_read_key(&ui,&event,2,service_test_redraw)==0 && event.key==FD2_INPUT_KEY_DOWN);
    CHECK(service_repaints==2 && vga->prepare_highlight_phase==3u && vga->prepare_sprite_phase==1u && context->rng.state==expected_rng.state);
    controls(vga);service_exact_time=1;ui.service=(prepare_service){0};ui.service.notice_origin=36887;
    ui.context->message_new_fragment=ui.context->message_old_fragment=0;context->message_number=7;
    fd2_field_rng_seed(&context->rng,0x7a18);expected_rng=context->rng;(void)fd2_field_rng_next(&expected_rng);
    pulse(1,SDL_SCANCODE_SPACE);*before=*context;
    CHECK(prepare_service_text(&ui,490,95,138)==0 && ui.service.input_epoch==1u && !fd2_input_has_any_key(&vga->input));
    CHECK(context->rng.state==expected_rng.state && frame_number<6u);
    before->rng=context->rng;CHECK(!memcmp(context,before,sizeof(*before)));
    controls(vga);service_exact_time=1;ui.service=(prepare_service){0};ui.service.notice_origin=36887;
    ui.context->message_new_fragment=ui.context->message_old_fragment=0;
    fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,1);fd2_input_begin_test_frame(&vga->input,clock_ms);
    fd2_field_rng_seed(&context->rng,0x7a18);expected_rng=context->rng;(void)fd2_field_rng_next(&expected_rng);
    CHECK(prepare_service_text(&ui,490,95,138)==0 && ui.service.input_epoch==1u &&
        !fd2_input_has_any_key(&vga->input) && context->rng.state==expected_rng.state && frame_number==1u);

    controls(vga);service_exact_time=1;ui.service=(prepare_service){0};ui.service.selector=1;
    CHECK(fd2_field_prepare_assets_open_service(a,fdother,1)==0);
    memset(vga->framebuffer,0x61,64000);*before=*context;
    CHECK(prepare_service_notice_open(&ui,0)==0 && ui.service.page && !memcmp(context,before,sizeof(*before)));
    const fd2_image *portrait=&a->service.portraits[0];unsigned origin=4283;
    for(int row=0;row<80;row++)for(int col=0;col<80;col++)
        CHECK(ui.service.page->framebuffer[(origin/320u+(unsigned)row)*320u+origin%320u-(unsigned)col]==portrait->pixels[row*80+col]);
    CHECK(prepare_service_portrait(&ui,vga,0,0)==0);
    for(int row=0;row<80;row++)CHECK(!memcmp(vga->framebuffer+(13+row)*320+123,portrait->pixels+row*80,80));
    CHECK(prepare_service_modal_close(&ui)==0 && !ui.service.page &&
        ui.service.active_portraits==a->service.portraits && ui.service.notice_origin==4283u);
    CHECK(!memcmp(context,before,sizeof(*before)));
    memcpy(expected->framebuffer,vga->framebuffer,64000);
    uint32_t dato_end=a->dato.offsets[76];a->dato.offsets[76]=a->dato.offsets[75]+4u;
    CHECK(prepare_service_notice_open(&ui,2)==-1 && !ui.service.page &&
        ui.service.active_portraits==a->service.portraits && ui.service.notice_origin==4283u &&
        !memcmp(expected->framebuffer,vga->framebuffer,64000));
    a->dato.offsets[76]=dato_end;
    CHECK(prepare_service_notice_open(&ui,2)==0 && ui.service.active_portraits==ui.service.notice_portraits);
    CHECK(prepare_service_modal_close(&ui)==0 && ui.service.active_portraits==ui.service.notice_portraits);
    CHECK(prepare_service_notice_open(&ui,2)==0 && ui.service.active_portraits==ui.service.notice_portraits);
    CHECK(prepare_service_modal_close(&ui)==0 && !memcmp(context,before,sizeof(*before)));
    ui.service.promotion.old_unit_id=0;ui.service.promotion.target_unit_id=32;
    memcpy(expected->framebuffer,vga->framebuffer,64000);*before=*context;
    CHECK(prepare_service_promotion_prepare(&ui)==0 && ui.service.promotion_visual &&
        !memcmp(expected->framebuffer,vga->framebuffer,64000) && !memcmp(context,before,sizeof(*before)));
    prepare_service_cleanup(&ui,1);prepare_service_cleanup(&ui,1);
    CHECK(!ui.service.promotion_visual && !ui.service.promotion_backgrounds.data && !ui.service.promotion_figani.data);
    fd2_static_flow_node unknown={.op="not_a_service_operation"};unsigned port=99;
    CHECK(prepare_service_ui(&ui,&unknown,&port)==0);
    controls(vga);fd2_pcm_bank bank={0};fd2_pcm_player player={0};fd2_field_audio audio={0};
    CHECK(fd2_pcm_bank_open(&bank,fdother,31)==0 && fd2_pcm_bank_count(&bank)==13u);
    audio.player=&player;audio.ui_bank=&bank;ui.audio=&audio;context->preparation.options[3]=1;*before=*context;
    fd2_static_flow_node cue={.op="sfx",.args={1,3}};
    CHECK(prepare_service_ui(&ui,&cue,&port)==1 && port==FD2_FLOW_NEXT && sound_calls==1u &&
        sound_samples[0]==1u && sound_loops[0]==3 && sound_last_gain==101.0f/128.0f && !memcmp(context,before,sizeof(*before)));
    cue.args[0]=13;CHECK(prepare_service_ui(&ui,&cue,&port)==-1 && port==FD2_FLOW_ERROR && sound_calls==1u);
    cue.args[0]=1;cue.args[1]=4;CHECK(prepare_service_ui(&ui,&cue,&port)==-1 && sound_calls==1u);
    fd2_pcm_bank_close(&bank);ui.audio=NULL;
    free(expected);free(before);free(context);return 0;
}

static void transparent_expected(uint8_t *frame,const fd2_image *image,int x,int y) {
    for(int row=0;row<image->height;row++)for(int col=0;col<image->width;col++) {
        uint8_t pixel=image->pixels[(size_t)row*(size_t)image->width+(size_t)col];
        if(pixel)frame[(y+row)*VGA_STRIDE+x+col]=pixel;
    }
}
static int service_raw_icon_transparency(fd2_archive *fdother,fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context));uint8_t *expected=malloc(VGA_W*VGA_H);
    CHECK(context&&expected);context->initialized=context->has_preparation=1;context->preparation=*state;
    CHECK(fd2_field_prepare_assets_open_service(a,fdother,1)==0);
    prepare_ui ui={vga,fdother,a,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    controls(vga);memset(vga->framebuffer,0x61,VGA_W*VGA_H);memcpy(expected,vga->framebuffer,VGA_W*VGA_H);
    ui.service.picker_mode=PREPARE_PICK_MENU;vga->prepare_highlight_phase=2;
    size_t zero=0,opaque=0;
    for(size_t i=0;i<4u;i++) {
        const fd2_image *icon=&a->service.icons[2u*i+(i==0u)];
        for(int pixel=0;pixel<icon->width*icon->height;pixel++)if(icon->pixels[pixel])opaque++;else zero++;
        transparent_expected(expected,icon,201+26*(int)i,169);
    }
    CHECK(zero && opaque && prepare_service_picker_draw(&ui)==0 && !memcmp(vga->framebuffer,expected,VGA_W*VGA_H));
    controls(vga);service_capture_first=1;memset(vga->framebuffer,0x61,VGA_W*VGA_H);memcpy(expected,vga->framebuffer,VGA_W*VGA_H);
    transparent_expected(expected,&a->commands.frames[48],244,168); /* 原19953展开用normal，不借共享phase。 */
    transparent_expected(expected,&a->commands.frames[51],252,168);
    pulse(4,SDL_SCANCODE_RETURN);unsigned port=99;
    CHECK(prepare_service_confirm(&ui,&port)==0 && port==FD2_FLOW_YES &&
        !memcmp(service_first_frame,expected,VGA_W*VGA_H));
    free(expected);free(context);return 0;
}

/* 原27079/2810b只写bottom列表/箭头；菜单只写raw透明图标。
 * expected从完整before叠既有各独立view叶，不调用picker_draw生成；
 * full-page保留性质独立检查source窗口外每一字节。不是新增原像素实机oracle。 */
static int picker_retained_expected(prepare_ui *ui,fd2_vga *expected,int arrows) {
    prepare_service *s=&ui->service;
    const fd2_save_manual_state *state=&ui->context->preparation;
    size_t first=s->first,count=0,visible=6;int result=-1;
    switch(s->picker_mode) {
        case PREPARE_PICK_PRODUCT:
            first=(size_t)s->product_first;count=s->stock_count;
            result=fd2_field_prepare_draw_items(expected,ui->assets,s->stock,count,first,s->product_index,0);break;
        case PREPARE_PICK_ITEM:
            count=s->item_count;
            result=fd2_field_prepare_draw_items(expected,ui->assets,s->visible_items,count,first,s->raw_slot,1);break;
        case PREPARE_PICK_MEMBER:
            count=state->unit_count;
            result=fd2_field_prepare_draw_members(expected,ui->assets,state,first,s->cursor,ui->vga->prepare_sprite_phase);break;
        case PREPARE_PICK_BUY_MEMBER:
            count=s->candidate_count;visible=3;
            result=fd2_field_prepare_draw_equipment_recipients(expected,ui->assets,state,s->candidates,
                count,first,s->cursor,s->product_id,ui->vga->prepare_sprite_phase);break;
        case PREPARE_PICK_REVIVE:
            count=s->candidate_count;visible=3;
            result=fd2_field_prepare_draw_revive_candidates(expected,ui->assets,state,s->candidates,
                count,first,s->cursor,ui->vga->prepare_sprite_phase);break;
        case PREPARE_PICK_PROMOTION:
            count=s->candidate_count;visible=3;
            result=fd2_field_prepare_draw_promotion_candidates(expected,ui->assets,state,s->candidates,
                count,first,s->cursor,ui->vga->prepare_sprite_phase);break;
        case PREPARE_PICK_MENU:
            for(size_t i=0;i<4u;i++)transparent_expected(expected->framebuffer,
                &ui->assets->service.icons[2u*i+(i==(size_t)s->selected_command ? ui->vga->prepare_highlight_phase/2u:0u)],
                201+26*(int)i,169);
            return 0;
        default:return -1;
    }
    if(result || !arrows)return result;
    fd2_map_sprite_blit(expected,&ui->assets->service.scroll_arrows[first ? ui->vga->prepare_highlight_phase/2u:4u],298,120,-1);
    fd2_map_sprite_blit(expected,&ui->assets->service.scroll_arrows[first+visible<count ? 2u+ui->vga->prepare_highlight_phase/2u:4u],298,181,-1);
    return 0;
}
static void picker_retained_history(fd2_vga *vga,const fd2_image *portrait,int history) {
    if(history==2) {
        for(size_t i=0;i<112u*VGA_W;i++)vga->framebuffer[i]=(uint8_t)(i*29u+i/VGA_W+17u);
    } else {
        /* 原16559特殊origin4283走正向；phase0及3都可成为合法current VGA历史。 */
        for(int row=0;row<80;row++)memcpy(vga->framebuffer+(13+row)*320+123,portrait->pixels+80*row,80);
    }
}
static int picker_retained_pages(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context)),*before_context=malloc(sizeof(*before_context));
    fd2_vga *expected=malloc(sizeof(*expected));uint8_t *before=malloc(VGA_W*VGA_H);
    CHECK(context&&before_context&&expected&&before);
    context->initialized=context->has_preparation=1;context->preparation=*state;
    context->preparation.unit_count=7;
    for(size_t i=0;i<7u;i++) {
        memcpy(context->preparation.unit_area+80u*i,state->unit_area,80);
        context->preparation.unit_area[80u*i+7u]=(uint8_t)i;
        context->preparation.unit_area[80u*i+8u]=(uint8_t)i;
        context->preparation.unit_area[80u*i+33u]=(uint8_t)(i<4u ? 20:1);
        for(size_t slot=0;slot<8u;slot++)context->preparation.unit_area[80u*i+10u+2u*slot]=0x80;
    }
    CHECK(fd2_field_prepare_assets_bind_roster(assets,&context->preparation)==0 &&
        fd2_field_prepare_assets_open_service(assets,fdother,1)==0);
    const int modes[]={PREPARE_PICK_PRODUCT,PREPARE_PICK_ITEM,PREPARE_PICK_MEMBER,
        PREPARE_PICK_BUY_MEMBER,PREPARE_PICK_REVIVE,PREPARE_PICK_PROMOTION,PREPARE_PICK_MENU};
    for(size_t mode=0;mode<sizeof(modes)/sizeof(modes[0]);mode++)for(int history=0;history<3;history++) {
        controls(vga);memset(vga->framebuffer,0x39,VGA_W*VGA_H);
        for(size_t i=0;i<7u;i++) {
            fd2_field_unit unit;memcpy(&unit,context->preparation.unit_area+80u*i,80);
            /* 原2a07a按flags bit0收复活候选，价格叶也要求该bit；HP0本身不足。
             * 仅前四条成为候选；其它模式不继承上一轮的hidden标志。 */
            int dead=modes[mode]==PREPARE_PICK_REVIVE && i<4u;
            unit.flags=(uint8_t)(dead ? 1:0);unit.hp=(uint16_t)(dead ? 0:100);unit.hp_max=100;
            memcpy(context->preparation.unit_area+80u*i,&unit,80);
        }
        prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
        prepare_service *s=&ui.service;s->picker_mode=modes[mode];
        s->stock_count=s->item_count=8;s->candidate_count=4;s->product_id=0;
        for(size_t i=0;i<8u;i++)s->stock[i]=s->visible_items[i]=(uint8_t)(i&1u ? 20:0);
        for(size_t i=0;i<4u;i++)s->candidates[i]=(uint8_t)i;
        if(modes[mode]==PREPARE_PICK_REVIVE || modes[mode]==PREPARE_PICK_PROMOTION) {
            uint8_t source_candidates[FD2_SAVE_MAX_UNITS];size_t source_count=0;
            int source_result=modes[mode]==PREPARE_PICK_REVIVE ?
                fd2_field_prepare_revive_candidates(&context->preparation,source_candidates,FD2_SAVE_MAX_UNITS,&source_count):
                fd2_field_prepare_promotion_candidates(&context->preparation,source_candidates,FD2_SAVE_MAX_UNITS,&source_count);
            CHECK(source_result==0 && source_count==s->candidate_count &&
                !memcmp(source_candidates,s->candidates,source_count));
        }
        s->page=new_frame(vga);s->page_background=malloc(VGA_W*VGA_H);CHECK(s->page&&s->page_background);
        memcpy(s->page_background,vga->framebuffer,VGA_W*VGA_H);
        picker_retained_history(vga,&assets->service.portraits[history==1 ? 3:0],history);
        memcpy(before,vga->framebuffer,VGA_W*VGA_H);*expected=*vga;*before_context=*context;
        int expected_result=picker_retained_expected(&ui,expected,1);
        if(expected_result)fprintf(stderr,"picker expected rejected: mode=%d history=%d count=%zu flags0=%u profile0=%u\n",
            modes[mode],history,s->candidate_count,(unsigned)context->preparation.unit_area[5],
            (unsigned)context->preparation.unit_area[32]);
        CHECK(expected_result==0);
        CHECK(prepare_service_picker_draw(&ui)==0 && !memcmp(vga,expected,sizeof(*vga)) &&
            !memcmp(context,before_context,sizeof(*context)) && !frame_number && !clock_reads && !sound_calls);
        for(size_t i=0;i<VGA_W*VGA_H;i++) {
            size_t x=i%VGA_W,y=i/VGA_W;
            int owned=modes[mode]==PREPARE_PICK_MENU ? x>=201u && x<305u && y>=169u && y<189u:
                x>=5u && x<315u && y>=112u && y<198u;
            if(!owned)CHECK(vga->framebuffer[i]==before[i]);
        }
        /* C结构参数预检失败也不能先恢复整张旧snapshot。 */
        if(modes[mode]==PREPARE_PICK_MEMBER) {
            *expected=*vga;uint8_t count=context->preparation.unit_count;context->preparation.unit_count=0;
            CHECK(prepare_service_picker_draw(&ui)==-1 && !memcmp(vga,expected,sizeof(*vga)));
            context->preparation.unit_count=count;
        }
        free(s->page);free(s->page_background);s->page=NULL;s->page_background=NULL;
    }
    /* 导航/跨页由原27d33本地写名单；上方画像不属于该写集。 */
    for(int down=0;down<2;down++) {
        controls(vga);service_exact_time=1;
        prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
        ui.service.picker_mode=PREPARE_PICK_MEMBER;ui.service.cursor=down ? 4:2;ui.service.first=down ? 0u:2u;
        memset(vga->framebuffer,0x39,VGA_W*VGA_H);picker_retained_history(vga,&assets->service.portraits[3],1);
        CHECK(prepare_service_picker_draw(&ui)==0);memcpy(before,vga->framebuffer,VGA_W*VGA_H);
        size_t first=ui.service.first;int selected=ui.service.cursor;
        CHECK(prepare_service_picker_key(&ui,&selected,7,&first,2,down ? FD2_INPUT_KEY_DOWN:FD2_INPUT_KEY_UP,0)==0 &&
            selected==(down ? 6:0) && first==(down ? 2u:0u));
        ui.service.cursor=selected;ui.service.first=first;*expected=*vga;*before_context=*context;
        /* 27816/278e7先搬当前VGA，再2810b写新名单；不得用新底板抹stride孔。
         * 独立终态方程：目标行取原行+/-26，空出的26行填73；外部逐字节保留。 */
        for(int row=0;row<80;row++)for(int col=0;col<284;col++) {
            int from=down ? row+26:row-26;
            expected->framebuffer[(115+row)*320+10+col]=from>=0 && from<80 ?
                before[(115+from)*320+10+col]:73;
        }
        CHECK(picker_retained_expected(&ui,expected,0)==0);
        CHECK(prepare_service_scroll_rows(&ui,before,expected->framebuffer,down)==0 &&
            !memcmp(vga->framebuffer,expected->framebuffer,VGA_W*VGA_H) &&
            !memcmp(context,before_context,sizeof(*context)) && frame_number==3u);
    }
    /* HOST在首个6px搬移已写后返回；不吞前缀，也不继续余下三步/终态copy。 */
    controls(vga);service_exact_time=1;quit_frame=1;
    prepare_ui stopped={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    for(size_t i=0;i<VGA_W*VGA_H;i++)vga->framebuffer[i]=(uint8_t)(i*7u+i/VGA_W);
    picker_retained_history(vga,&assets->service.portraits[0],0);
    memcpy(before,vga->framebuffer,VGA_W*VGA_H);*expected=*vga;*before_context=*context;
    for(int row=0;row<74;row++)memcpy(expected->framebuffer+(115+row)*320+10,
        before+(121+row)*320+10,284);
    for(int row=74;row<80;row++)memset(expected->framebuffer+(115+row)*320+10,73,284);
    CHECK(prepare_service_scroll_rows(&stopped,before,before,1)==1 && stopped.service.host_quit &&
        frame_number==1u && !memcmp(vga->framebuffer,expected->framebuffer,VGA_W*VGA_H) &&
        !memcmp(context,before_context,sizeof(*context)));
    free(before);free(expected);free(before_context);free(context);return 0;
}
static int picker_retained_modal_boundary(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context));
    uint8_t *before=malloc(VGA_W*VGA_H);CHECK(context&&before);
    context->initialized=context->has_preparation=1;context->preparation=*state;
    CHECK(fd2_field_prepare_assets_bind_roster(assets,state)==0 &&
        fd2_field_prepare_assets_open_service(assets,fdother,1)==0);
    for(int host=0;host<2;host++) {
        controls(vga);service_exact_time=1;fd2_bios_clock_reset(0,0);
        memset(vga->framebuffer,0x39,VGA_W*VGA_H);
        picker_retained_history(vga,&assets->service.portraits[3],1);memcpy(before,vga->framebuffer,VGA_W*VGA_H);
        prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};ui.service.picker_mode=PREPARE_PICK_MEMBER;
        fd2_field_rng_seed(&context->rng,1);fd2_field_rng expected_rng=context->rng;
        (void)fd2_field_rng_next(&expected_rng);
        if(host)quit_frame=7;else pulse(7,SDL_SCANCODE_ESCAPE);
        unsigned port=99;CHECK(prepare_service_pick_list(&ui,&port)==(host ? -1:1) &&
            port==(unsigned)(host ? FD2_FLOW_ERROR:FD2_FLOW_CANCEL) && ui.service.host_quit==host &&
            ui.service.page && ui.service.page_background &&
            !memcmp(ui.service.page_background,before,VGA_W*VGA_H) &&
            !memcmp(vga->framebuffer,before,112u*VGA_W) && context->rng.state==expected_rng.state &&
            !memcmp(&context->preparation,state,sizeof(*state)) && !clear_calls && !sound_calls);
        if(!host)CHECK(prepare_service_modal_close(&ui)==0 && !ui.service.page && !ui.service.page_background &&
            !memcmp(vga->framebuffer,before,VGA_W*VGA_H));
        prepare_service_cleanup(&ui,1);CHECK(!ui.service.page && !ui.service.page_background &&
            !memcmp(vga->framebuffer,before,VGA_W*VGA_H));
    }
    free(before);free(context);return 0;
}

/* 相同原窗口的四个运输步骤必须等于26px终态；before整页非均匀，
 * expected由坐标方程取源，不调用被测运输器，且逐字节保护箭头/上部。 */
static int picker_shift_write_set(fd2_vga *vga) {
    uint8_t *before=malloc(VGA_W*VGA_H),*expected=malloc(VGA_W*VGA_H);CHECK(before&&expected);
    for(size_t i=0;i<VGA_W*VGA_H;i++)before[i]=(uint8_t)(i*17u+i/VGA_W*13u);
    static const int deltas[4]={6,6,6,8};
    for(int down=0;down<2;down++) {
        memcpy(vga->framebuffer,before,VGA_W*VGA_H);memcpy(expected,before,VGA_W*VGA_H);
        for(int step=0;step<4;step++)CHECK(prepare_picker_shift_rows(vga->framebuffer,down,deltas[step])==0);
        for(int row=0;row<80;row++)for(int col=0;col<284;col++) {
            int from=down ? row+26:row-26;
            expected[(115+row)*320+10+col]=from>=0 && from<80 ? before[(115+from)*320+10+col]:73;
        }
        CHECK(!memcmp(vga->framebuffer,expected,VGA_W*VGA_H));
        memcpy(vga->framebuffer,before,VGA_W*VGA_H);
        CHECK(prepare_picker_shift_rows(vga->framebuffer,down,26)==0 && !memcmp(vga->framebuffer,expected,VGA_W*VGA_H));
        CHECK(prepare_picker_shift_rows(vga->framebuffer,down,0)==-1 && !memcmp(vga->framebuffer,expected,VGA_W*VGA_H));
    }
    free(before);free(expected);return 0;
}

/* adapter oracle：原27079三个mode各调不同叶；view的原token整页oracle另在
 * field_prepare_view_test。此处只用其已验证公开叶组合，独立核dispatch/写窗。
 * mode1不得重画name/price；mode2不得重画name/preview；mode0只选中icon。 */
static int picker_steady_dispatch(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context));fd2_vga *expected=malloc(sizeof(*expected));
    CHECK(context&&expected && state->unit_count>=4u);context->initialized=context->has_preparation=1;context->preparation=*state;
    CHECK(fd2_field_prepare_assets_bind_roster(assets,state)==0 &&
        fd2_field_prepare_assets_open_service(assets,fdother,1)==0);
    const int modes[]={PREPARE_PICK_PRODUCT,PREPARE_PICK_ITEM,PREPARE_PICK_BUY_MEMBER,
        PREPARE_PICK_REVIVE,PREPARE_PICK_PROMOTION,PREPARE_PICK_MEMBER,PREPARE_PICK_MENU};
    for(size_t mode=0;mode<sizeof(modes)/sizeof(modes[0]);mode++)for(unsigned phase=0;phase<4u;phase++) {
        controls(vga);for(size_t i=0;i<VGA_W*VGA_H;i++)vga->framebuffer[i]=(uint8_t)(i*31u+i/VGA_W+mode);
        vga->prepare_highlight_phase=phase;vga->prepare_sprite_phase=phase;*expected=*vga;
        prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
        prepare_service *s=&ui.service;s->picker_mode=modes[mode];s->stock_count=s->item_count=8;
        s->candidate_count=4;s->selected_command=2;
        for(size_t i=0;i<4u;i++)s->candidates[i]=(uint8_t)i;
        size_t count=s->picker_mode==PREPARE_PICK_MEMBER ? state->unit_count:
            s->picker_mode==PREPARE_PICK_PRODUCT || s->picker_mode==PREPARE_PICK_ITEM ? 8u:4u;
        size_t visible=s->picker_mode>=PREPARE_PICK_BUY_MEMBER && s->picker_mode<=PREPARE_PICK_PROMOTION ? 3u:6u;
        if(s->picker_mode==PREPARE_PICK_MENU) {
            transparent_expected(expected->framebuffer,&assets->service.icons[4u+phase/2u],253,169);
        } else {
            const fd2_image *up=&assets->service.scroll_arrows[4];
            const fd2_image *down=&assets->service.scroll_arrows[count>visible ? 2u+phase/2u:4u];
            for(int row=0;row<9;row++) {
                memcpy(expected->framebuffer+(120+row)*320+298,up->pixels+row*8,8);
                memcpy(expected->framebuffer+(181+row)*320+298,down->pixels+row*8,8);
            }
            if(s->picker_mode==PREPARE_PICK_MEMBER)
                CHECK(fd2_field_prepare_draw_members(expected,assets,state,0,0,phase)==0);
            else if(visible==3u)
                CHECK(fd2_field_prepare_draw_candidate_sprites(expected,assets,state,s->candidates,4,0,phase)==0);
        }
        fd2_save_manual_state saved=context->preparation;uint16_t rng=context->rng.state;
        CHECK(prepare_service_picker_tick_draw(&ui)==0 && !memcmp(vga,expected,sizeof(*vga)) &&
            !memcmp(&context->preparation,&saved,sizeof(saved)) && context->rng.state==rng &&
            !frame_number && !clock_reads && !clear_calls && !sound_calls);
    }
    free(expected);free(context);return 0;
}

/* 29300->29620->27d33->26eda(3)真实酒店名单；预期选择表独立于arrow helper。
 * 合成5个不同opaque8x9图只测binding/写序，实际原帧另由reader oracle核。 */
static int hotel_arrows_and_initial_wait(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context));fd2_vga *expected=malloc(sizeof(*expected));
    CHECK(context&&expected);context->initialized=context->has_preparation=1;context->preparation=*state;
    fd2_image saved[5];uint8_t pixels[5][72];memcpy(saved,assets->hotel_scroll_arrows,sizeof(saved));
    for(size_t i=0;i<5u;i++) {
        CHECK(saved[i].pixels && saved[i].width==8 && saved[i].height==9);
        memset(pixels[i],40+(int)i,sizeof(pixels[i]));pixels[i][0]=0;
        assets->hotel_scroll_arrows[i]=(fd2_image){.width=8,.height=9,.pixels=pixels[i]};
    }
    static const struct {size_t count,first;unsigned up[4],down[4];} vectors[]={
        {5,0,{4,4,4,4},{4,4,4,4}},{7,0,{4,4,4,4},{2,2,3,3}},
        {7,2,{0,0,1,1},{4,4,4,4}},{9,2,{0,0,1,1},{2,2,3,3}}};
    for(size_t test=0;test<4u;test++)for(unsigned phase=0;phase<4u;phase++) {
        controls(vga);memset(vga->framebuffer,0x61,VGA_W*VGA_H);*expected=*vga;
        for(int row=0;row<9;row++) {
            memcpy(expected->framebuffer+(120+row)*320+298,pixels[vectors[test].up[phase]]+row*8,8);
            memcpy(expected->framebuffer+(181+row)*320+298,pixels[vectors[test].down[phase]]+row*8,8);
        }
        uint16_t rng=context->rng.state;
        CHECK(prepare_picker_arrows(vga,assets->hotel_scroll_arrows,vectors[test].count,vectors[test].first,6,phase)==0 &&
            !memcmp(vga,expected,sizeof(*vga)) && context->rng.state==rng && !clock_reads && !frame_number && !clear_calls);
    }
    /* 原先上再下；坏下图保已写上图，不提前恢复/清页。opaque0也必须写。 */
    controls(vga);memset(vga->framebuffer,0x61,VGA_W*VGA_H);*expected=*vga;
    for(int row=0;row<9;row++)memcpy(expected->framebuffer+(120+row)*320+298,pixels[0]+row*8,8);
    assets->hotel_scroll_arrows[2].width=7;
    CHECK(prepare_picker_arrows(vga,assets->hotel_scroll_arrows,9,2,6,0)==-1 && !memcmp(vga,expected,sizeof(*vga)));
    memcpy(assets->hotel_scroll_arrows,saved,sizeof(saved));
    CHECK(fd2_field_prepare_assets_bind_roster(assets,state)==0);
    controls(vga);fd2_bios_clock_reset(0,0);memset(vga->framebuffer,0x61,VGA_W*VGA_H);
    fd2_map_sprite_blit(vga,&assets->slots,5,112,-1);*expected=*vga;
    CHECK(fd2_field_prepare_draw_roster(expected,assets,state,0,0,0)==0);
    for(int row=0;row<9;row++) {
        memcpy(expected->framebuffer+(120+row)*320+298,saved[4].pixels+row*8,8);
        memcpy(expected->framebuffer+(181+row)*320+298,saved[4].pixels+row*8,8);
    }
    prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,0, .service = {0}};
    fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_ESCAPE,1);
    fd2_input_begin_test_frame(&vga->input,0);expected->input=vga->input;
    fd2_field_rng_seed(&context->rng,1);fd2_field_rng rng=context->rng;(void)fd2_field_rng_next(&rng);
    prepare_wait wait={0};fd2_input_event event;
    CHECK(read_key_state(&ui,&event,2,1,&wait)==0 && cancel_key(event.key) && wait.initialized &&
        context->rng.state==rng.state && vga->prepare_highlight_phase==2u &&
        !memcmp(vga->framebuffer,expected->framebuffer,VGA_W*VGA_H) && !frame_number && !water_calls && !clear_calls);
    /* HOST在整个入口之前；空队伍也保既有无候选边界，不进箭头读取。 */
    controls(vga);*expected=*vga;vga->input.quit_requested=1;expected->input=vga->input;
    uint16_t old_rng=context->rng.state;wait=(prepare_wait){0};
    CHECK(read_key_state(&ui,&event,2,1,&wait)==1 && context->rng.state==old_rng && !wait.initialized &&
        !memcmp(vga->framebuffer,expected->framebuffer,VGA_W*VGA_H));
    controls(vga);context->preparation.unit_count=0;*expected=*vga;
    CHECK(roster_run(&ui)==0 && !memcmp(vga,expected,sizeof(*vga)) && context->rng.state==old_rng);
    free(expected);free(context);return 0;
}

static void queue_pair(fd2_vga *vga) {
    fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RIGHT,1);
    fd2_input_begin_test_frame(&vga->input,clock_ms);
    fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RIGHT,0);
    fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,1);
    fd2_input_begin_test_frame(&vga->input,++clock_ms);
    fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,0);
    fd2_input_begin_test_frame(&vga->input,++clock_ms);
}

/* 26eda/16c57/29bcb各读一项，只有源clear叶才清余队；普通文字peek
 * 不提前计input_epoch，不改变同一SAV/资金/库存owner。 */
/* 原26eda eligible：27079→portrait0→RNG→freshclock，再检查键。
 * 此expected只用于确认这组实际绘制已提交，不自证像素正确或sprite相位原义。 */
static int hotel_wait_write_order(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context));
    fd2_vga *expected=malloc(sizeof(*expected));CHECK(context && expected);
    context->initialized=context->has_preparation=1;context->preparation=*state;
    prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,0, .service = {0}};
    controls(vga);fd2_bios_clock_reset(0,0);
    memset(vga->framebuffer,0x61,sizeof(vga->framebuffer));
    fd2_map_sprite_blit(vga,&assets->slots,5,112,-1);*expected=*vga;
    /* 26eda的highlight2→3仅让独立53F72从0→1，不以highlight3画sprite。 */
    CHECK(fd2_field_prepare_draw_roster(expected,assets,state,0,0,1)==0);
    fd2_map_sprite_blit(expected,&assets->hotel_scroll_arrows[4],298,120,-1);
    fd2_map_sprite_blit(expected,&assets->hotel_scroll_arrows[4],298,181,-1);
    fd2_map_sprite_blit(expected,&assets->hotel_portraits[0],107,5,-1);
    fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,1);
    fd2_input_begin_test_frame(&vga->input,0);
    fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,0);
    fd2_input_begin_test_frame(&vga->input,1);
    clock_ms=110;fd2_field_rng_seed(&context->rng,0x5a31);
    vga->prepare_highlight_phase=2;
    prepare_wait waiting={.last=0,.countdown=2,.portrait=3,.initialized=1};
    blink_tracking=1;blink_vga=vga;blink_expected=expected;blink_state=&waiting;
    fd2_input_event event={0};
    CHECK(read_key_state(&ui,&event,2,1,&waiting)==0 && event.key==FD2_INPUT_KEY_ENTER);
    CHECK(!blink_bad && blink_rng_calls==1 && blink_roster_calls==1 && waiting.last==4 &&
        waiting.portrait==0 && vga->prepare_highlight_phase==3u && vga->prepare_sprite_phase==1u && !frame_number && !water_calls);
    controls(vga);clock_ms=110;frame_number=1;fail_members_draw=1;
    fd2_field_rng_seed(&context->rng,0x5a31);
    vga->prepare_highlight_phase=2;
    waiting=(prepare_wait){.last=0,.countdown=2,.portrait=3,.initialized=1};
    blink_tracking=1;blink_vga=vga;blink_expected=expected;blink_state=&waiting;
    CHECK(read_key_state(&ui,&event,2,1,&waiting)==-1 && !blink_rng_calls && !blink_roster_calls &&
        context->rng.state==0x5a31 && waiting.last==0 && waiting.portrait==3 &&
        vga->prepare_highlight_phase==3u && vga->prepare_sprite_phase==1u);
    /* 16c57反过来先读键；同样的临界状态有早键时不得推进或抽样。 */
    controls(vga);clock_ms=110;fd2_field_rng_seed(&context->rng,0x5a31);
    fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,1);
    fd2_input_begin_test_frame(&vga->input,clock_ms);
    waiting=(prepare_wait){.last=0,.countdown=2,.portrait=3,.initialized=1};
    blink_tracking=1;blink_vga=vga;blink_expected=expected;blink_state=&waiting;
    CHECK(read_key_state(&ui,&event,1,1,&waiting)==0 && !blink_rng_calls && !blink_roster_calls &&
        context->rng.state==0x5a31 && waiting.last==0 && waiting.portrait==3 && !water_calls);
    controls(vga);free(expected);free(context);return 0;
}

/* 原19953先4e31c再读高亮clock；16c57先读键，空等才4e31c。
 * 120ms跨过两个固定PIT tick，用water叶内推进检查下一读者实际fresh读取，
 * 不由待验consumer生成expected；像素及live内部多次读钟由visual族另验。 */
static int prepare_water_live_order(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context));CHECK(context);
    context->initialized=context->has_preparation=1;context->preparation=*state;
    prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    prepare_choice_scope scope={0};scope.ui=&ui;
    controls(vga);fd2_bios_clock_reset(0,0);water_advance_ms=120;
    vga->menu_highlight_phase=vga->menu_highlight_tick=0;
    CHECK(prepare_choice_step(FD2_FIELD_CHOICE_TICK,0,&scope)==1 && water_calls==1u &&
        vga->menu_highlight_phase==1u && vga->menu_highlight_tick==2u && !frame_number);

    /* source09 19b1e比较/19b49存锚是两次读：比较后跨tick，真实锚须用新值。 */
    controls(vga);fd2_bios_clock_reset(0,0);clock_ms=110;
    clock_advance_on_read=2;clock_read_advance_ms=70;
    vga->menu_highlight_phase=vga->menu_highlight_tick=0;
    CHECK(prepare_choice_step(FD2_FIELD_CHOICE_TICK,0,&scope)==1 && clock_reads==3u &&
        vga->menu_highlight_phase==1u && vga->menu_highlight_tick==3u && !frame_number);

    controls(vga);water_failure=1;fd2_field_rng_seed(&context->rng,0x5a31);
    vga->menu_highlight_phase=vga->menu_highlight_tick=0;
    CHECK(prepare_choice_step(FD2_FIELD_CHOICE_TICK,0,&scope)==-1 && water_calls==1u &&
        !vga->menu_highlight_phase && !vga->menu_highlight_tick && !frame_number &&
        context->rng.state==0x5a31);

    controls(vga);fd2_bios_clock_reset(0,0);water_advance_ms=120;quit_frame=1;
    fd2_field_rng_seed(&context->rng,0x5a31);fd2_field_rng expected=context->rng;
    (void)fd2_field_rng_next(&expected);
    prepare_wait waiting={.last=0,.countdown=2,.portrait=3,.initialized=1};
    fd2_input_event event={0};
    CHECK(read_key_state(&ui,&event,1,0,&waiting)==1 && water_calls==1u &&
        waiting.last==2 && waiting.portrait==0 && !vga->prepare_highlight_phase && !vga->prepare_sprite_phase && frame_number==1u &&
        context->rng.state==expected.state);

    controls(vga);water_failure=1;fd2_field_rng_seed(&context->rng,0x5a31);
    waiting=(prepare_wait){.last=0,.countdown=2,.portrait=3,.initialized=1};
    CHECK(read_key_state(&ui,&event,1,0,&waiting)==-1 && water_calls==1u &&
        waiting.last==0 && waiting.portrait==3 && !vga->prepare_highlight_phase && !vga->prepare_sprite_phase && !frame_number &&
        context->rng.state==0x5a31);

    controls(vga);water_failure=1;fd2_field_rng_seed(&context->rng,0x5a31);expected=context->rng;
    (void)fd2_field_rng_next(&expected);
    CHECK(prepare_service_wait_event(&ui,&event,0)==-1 && water_calls==1u && !frame_number &&
        context->rng.state==expected.state);

    controls(vga);water_failure=1;queue_pair(vga);
    fd2_field_rng_seed(&context->rng,0x5a31);expected=context->rng;(void)fd2_field_rng_next(&expected);
    CHECK(prepare_service_wait_event(&ui,&event,0)==0 && event.key==FD2_INPUT_KEY_RIGHT &&
        !water_calls && !frame_number && context->rng.state==expected.state &&
        fd2_input_has_any_key(&vga->input));
    CHECK(!state_save_calls && !memcmp(&context->preparation,state,sizeof(*state)));
    controls(vga);free(context);return 0;
}

static unsigned owner_redraws,owner_fail_redraw,owner_host_redraw,owner_tick_after_entry;
static uint32_t owner_trace[2][2];
static int prepare_owner_redraw(prepare_ui *ui) {
    unsigned index=owner_redraws++;
    if(index<2u) {
        owner_trace[index][0]=ui->vga->prepare_highlight_phase;
        owner_trace[index][1]=ui->vga->prepare_sprite_phase;
    }
    if(owner_redraws==owner_fail_redraw)return -1;
    if(owner_redraws==owner_host_redraw)ui->vga->input.quit_requested=1;
    if(owner_redraws==1u && owner_tick_after_entry)clock_ms=110;
    if(owner_redraws==2u) {
        fd2_input_set_test_key_state(&ui->vga->input,SDL_SCANCODE_RETURN,1);
        fd2_input_begin_test_frame(&ui->vga->input,clock_ms);
    }
    return 0;
}

/* 原26eda只重置53F52=2；27079对每个mode先按奇phase推53F72，
 * 2810b/2825b/2a0c2/2a694只读53F72，菜单/商品虽不画sprite也推进它。
 * 显式原轨迹：(2,2)→(3,3)，重入→(2,3)，酒店tick→(3,0)。 */
static int prepare_phase_owner_contract(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context context={0};context.initialized=context.has_preparation=1;
    context.preparation=*state;fd2_field_rng_seed(&context.rng,0x5a31);
    prepare_ui ui={vga,fdother,assets,&context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    controls(vga);fd2_bios_clock_reset(0,0);
    vga->prepare_highlight_phase=1;vga->prepare_sprite_phase=2;
    owner_redraws=owner_fail_redraw=owner_host_redraw=0;owner_tick_after_entry=1;
    fd2_input_event event={0};fd2_field_rng expected=context.rng;(void)fd2_field_rng_next(&expected);
    CHECK(prepare_service_read_key(&ui,&event,0,prepare_owner_redraw)==0 && event.key==FD2_INPUT_KEY_ENTER);
    CHECK(owner_redraws==2u && owner_trace[0][0]==2u && owner_trace[0][1]==2u &&
        owner_trace[1][0]==3u && owner_trace[1][1]==3u && context.rng.state==expected.state);

    fd2_input_init(&vga->input);queue_pair(vga);owner_redraws=0;owner_tick_after_entry=0;
    (void)fd2_field_rng_next(&expected);
    CHECK(prepare_service_read_key(&ui,&event,1,prepare_owner_redraw)==0 && event.key==FD2_INPUT_KEY_RIGHT &&
        owner_redraws==1u && vga->prepare_highlight_phase==2u && vga->prepare_sprite_phase==3u &&
        context.rng.state==expected.state && fd2_input_has_any_key(&vga->input));
    clock_ms=110;fd2_input_init(&vga->input);queue_pair(vga); /* 排队递增后不再回拨时间。 */
    prepare_wait waiting={.last=0,.countdown=5,.portrait=0,.initialized=1};
    CHECK(read_key_state(&ui,&event,2,0,&waiting)==0 && event.key==FD2_INPUT_KEY_RIGHT &&
        vga->prepare_highlight_phase==3u && vga->prepare_sprite_phase==0u &&
        context.rng.state==expected.state && waiting.countdown==4);

    /* 16c57的空等确实越过tick并更新blink倒计时，但不能推进这两个owner。 */
    fd2_input_init(&vga->input);frame_number=0;quit_frame=1;
    waiting=(prepare_wait){.last=0,.countdown=5,.initialized=1};
    CHECK(read_key_state(&ui,&event,1,0,&waiting)==1 && waiting.countdown==4 &&
        vga->prepare_highlight_phase==3u && !vga->prepare_sprite_phase && context.rng.state==expected.state);

    /* entry错误保原%50+8与53F52=2；tick重画错误保已提交的两相位。 */
    fd2_input_init(&vga->input);clock_ms=0;fd2_bios_clock_reset(0,0);quit_frame=0;
    owner_redraws=0;owner_fail_redraw=1;owner_tick_after_entry=0;
    (void)fd2_field_rng_next(&expected);
    CHECK(prepare_service_read_key(&ui,&event,2,prepare_owner_redraw)==-1 && owner_redraws==1u &&
        vga->prepare_highlight_phase==2u && !vga->prepare_sprite_phase && context.rng.state==expected.state);
    owner_redraws=0;owner_fail_redraw=2;owner_tick_after_entry=1;
    (void)fd2_field_rng_next(&expected);
    CHECK(prepare_service_read_key(&ui,&event,3,prepare_owner_redraw)==-1 && owner_redraws==2u &&
        vga->prepare_highlight_phase==3u && vga->prepare_sprite_phase==1u && context.rng.state==expected.state);
    fd2_input_init(&vga->input);clock_ms=0;fd2_bios_clock_reset(0,0);
    owner_redraws=owner_fail_redraw=0;owner_host_redraw=1;owner_tick_after_entry=0;
    (void)fd2_field_rng_next(&expected);
    CHECK(prepare_service_read_key(&ui,&event,0,prepare_owner_redraw)==1 && owner_redraws==1u &&
        vga->prepare_highlight_phase==2u && vga->prepare_sprite_phase==1u && context.rng.state==expected.state);
    waiting=(prepare_wait){0};
    CHECK(read_key_state(&ui,&event,2,0,&waiting)==1 && !waiting.initialized &&
        vga->prepare_highlight_phase==2u && vga->prepare_sprite_phase==1u && context.rng.state==expected.state);
    fd2_input_init(&vga->input);ui.service.host_quit=0;
    fd2_static_flow_node reset={.op="selection_reset"};unsigned port=FD2_FLOW_ERROR;
    CHECK(prepare_service_domain(&ui,&reset,&port)==1 && port==FD2_FLOW_NEXT &&
        vga->prepare_highlight_phase==2u && vga->prepare_sprite_phase==1u);
    CHECK(!state_save_calls && !memcmp(&context.preparation,state,sizeof(*state)));
    controls(vga);return 0;
}

/* 原19953/19be0：仅闭眼/开眼事件绘图，开眼图已写scope.page才取rng%30+10。
 * 按原画像坐标单独组成expected；不调用choice_step生成画面或顺序预期。 */
/* 19953/197e5开关各四相位；只STEADY是宿主pump。普通/服务仍借
 * 原input owner与service input_epoch；finite不额外读BIOS/RNG。 */
static int prepare_choice_finite_owner(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context));fd2_vga *base=calloc(1,sizeof(*base));
    CHECK(context && base);context->initialized=context->has_preparation=1;context->preparation=*state;
    const int open[4]={4,8,12,16},close[4]={12,8,4,0};
    for(int service=0;service<2;service++) {
        controls(vga);choice_timing_probe=1;vga->frame_deadline_ns=vga->frame_interval_ns=0;
        prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
        unsigned port=99;prepare_choice_scope scope={0};scope.ui=&ui;scope.page=base;scope.service=service;scope.port=&port;
        memset(base->framebuffer,0x69,sizeof(base->framebuffer));
        CHECK(fd2_field_page_choice_snapshot_write(vga->field_page,base->framebuffer,sizeof(base->framebuffer))==0);
        CHECK(prepare_choice_step(FD2_FIELD_CHOICE_BEGIN,0,&scope)==0);
        uint16_t after_begin=context->rng.state;
        for(unsigned i=0;i<4;i++)CHECK(prepare_choice_step(FD2_FIELD_CHOICE_DRAW,open[i],&scope)==0);
        CHECK(choice_plain==4 && !choice_timed && !clock_ms && !clock_reads && !water_calls && context->rng.state==after_begin);
        CHECK(prepare_choice_step(FD2_FIELD_CHOICE_STEADY,16,&scope)==0 && choice_timed==1);
        vga->input.queued_keys[0]=(fd2_input_event){.key=FD2_INPUT_KEY_ENTER};vga->input.queued_head=0;vga->input.queued_count=1;
        CHECK(prepare_choice_step(FD2_FIELD_CHOICE_READ_KEY,0,&scope)==1 && !vga->frame_deadline_ns &&
            !vga->frame_interval_ns && !scope.host_deadline && port==FD2_FLOW_YES && ui.service.input_epoch==(unsigned)service);
        uint64_t after_steady=clock_ms;
        for(unsigned i=0;i<4;i++)CHECK(prepare_choice_step(FD2_FIELD_CHOICE_DRAW,close[i],&scope)==0);
        CHECK(prepare_choice_step(FD2_FIELD_CHOICE_FINISH,0,&scope)==0 && choice_plain==8 && choice_timed==1 &&
            clock_ms==after_steady && !clock_reads && !water_calls && context->rng.state==after_begin);
        scope.host_deadline=UINT64_C(100000000);vga->frame_deadline_ns=UINT64_C(150000000);vga->frame_interval_ns=UINT64_C(10000000);
        prepare_choice_release(&scope);
        CHECK(vga->frame_deadline_ns==UINT64_C(150000000) && vga->frame_interval_ns==UINT64_C(10000000));
        CHECK(prepare_choice_step(FD2_FIELD_CHOICE_DRAW,4,&scope)==0 && clock_ms==after_steady &&
            vga->frame_deadline_ns==UINT64_C(150000000) && vga->frame_interval_ns==UINT64_C(10000000));
        /* 纯copy不等待外来deadline，也不拥有/取消它。 */
        /* HOST从STEADY提交返回；outer cleanup不关框、不保H ticket，也不清foreign。 */
        controls(vga);choice_timing_probe=1;quit_frame=5;ui.service.host_quit=0;port=99;
        CHECK(prepare_service_confirm(&ui,&port)==1 && choice_plain==4 && choice_timed==1 &&
            vga->frame_deadline_ns==UINT64_C(150000000) &&
            vga->frame_interval_ns==UINT64_C(10000000) && port==99);
    }
    choice_timing_probe=0;free(base);free(context);return 0;
}

static int prepare_choice_blink_order(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context));
    fd2_vga *page=malloc(sizeof(*page)),*expected=malloc(sizeof(*expected));
    CHECK(context && page && expected);
    context->initialized=context->has_preparation=1;context->preparation=*state;
    prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    prepare_choice_scope scope={0};scope.ui=&ui;scope.page=page;
    for(int kind=0;kind<3;kind++) {
        controls(vga);*page=*vga;memset(page->framebuffer,0x72,sizeof(page->framebuffer));
        memset(vga->framebuffer,0x61,sizeof(vga->framebuffer));
        scope.depart=kind==1;scope.service=kind==2;scope.blinking=0;scope.counter=2;
        ui.service.active_portraits=assets->hotel_portraits;ui.service.notice_origin=36887u;
        fd2_field_rng_seed(&context->rng,0x5a31);*expected=*page;
        choice_blink_scope=&scope;choice_blink_expected=expected;
        CHECK(prepare_choice_step(FD2_FIELD_CHOICE_BLINK,0,&scope)==0 && scope.counter==1u &&
            !scope.blinking && !choice_blink_rng_calls &&
            !memcmp(page->framebuffer,expected->framebuffer,sizeof(page->framebuffer)));
        scope.counter=0;
        if(kind==0)fd2_map_sprite_blit(expected,&assets->hotel_portraits[3],107,5,-1);
        else for(int y=0;y<80;y++)for(int x=0;x<80;x++) {
            if(kind==1)expected->framebuffer[(115+y)*VGA_STRIDE+8+x]=assets->depart_portraits[3].pixels[y*80+79-x];
            else expected->framebuffer[(115+y)*VGA_STRIDE+87-x]=assets->hotel_portraits[3].pixels[y*80+x];
        }
        CHECK(prepare_choice_step(FD2_FIELD_CHOICE_BLINK,0,&scope)==0 && scope.blinking==1 &&
            !choice_blink_rng_calls && !memcmp(page->framebuffer,expected->framebuffer,sizeof(page->framebuffer)));
        if(kind==0)fd2_map_sprite_blit(expected,&assets->hotel_portraits[0],107,5,-1);
        else for(int y=0;y<80;y++)for(int x=0;x<80;x++) {
            if(kind==1)expected->framebuffer[(115+y)*VGA_STRIDE+8+x]=assets->depart_portraits[0].pixels[y*80+79-x];
            else expected->framebuffer[(115+y)*VGA_STRIDE+87-x]=assets->hotel_portraits[0].pixels[y*80+x];
        }
        fd2_field_rng expected_rng=context->rng;
        uint32_t countdown=fd2_field_rng_next(&expected_rng)%30u+10u;
        CHECK(prepare_choice_step(FD2_FIELD_CHOICE_BLINK,0,&scope)==0 && !scope.blinking &&
            scope.counter==countdown && context->rng.state==expected_rng.state &&
            choice_blink_rng_calls==1u && !choice_blink_bad);
        for(size_t i=0;i<sizeof(vga->framebuffer);i++)CHECK(vga->framebuffer[i]==0x61);
    }
    controls(vga);scope.service=1;scope.blinking=1;scope.counter=9;
    fd2_image invalid_portraits[4]={{0}};ui.service.active_portraits=invalid_portraits;
    *expected=*page;choice_blink_scope=&scope;choice_blink_expected=expected;
    fd2_field_rng_seed(&context->rng,0x5a31);
    CHECK(prepare_choice_step(FD2_FIELD_CHOICE_BLINK,0,&scope)==-1 && scope.blinking==1 &&
        scope.counter==9u && !choice_blink_rng_calls && context->rng.state==0x5a31 &&
        !memcmp(page->framebuffer,expected->framebuffer,sizeof(page->framebuffer)));
    vga->input.quit_requested=1;
    CHECK(prepare_choice_step(FD2_FIELD_CHOICE_BLINK,0,&scope)==3 && scope.blinking==1 &&
        scope.counter==9u && !choice_blink_rng_calls && context->rng.state==0x5a31 &&
        !memcmp(page->framebuffer,expected->framebuffer,sizeof(page->framebuffer)));
    CHECK(!state_save_calls && !memcmp(&context->preparation,state,sizeof(*state)));
    controls(vga);free(expected);free(page);free(context);return 0;
}

static int prepare_buffered_input_contract(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context));
    CHECK(context);context->initialized=context->has_preparation=1;context->preparation=*state;
    CHECK(fd2_field_prepare_assets_open_service(assets,fdother,1)==0);
    prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    controls(vga);queue_pair(vga);fd2_field_rng_seed(&context->rng,0x5a31);
    fd2_field_rng expected=context->rng;(void)fd2_field_rng_next(&expected);(void)fd2_field_rng_next(&expected);
    fd2_input_event event;
    CHECK(read_key(&ui,&event,1,0)==0 && event.key==FD2_INPUT_KEY_RIGHT);
    CHECK(!frame_number && !water_calls && fd2_input_has_any_key(&vga->input));
    CHECK(read_key(&ui,&event,1,0)==0 && event.key==FD2_INPUT_KEY_ENTER);
    CHECK(!frame_number && !water_calls && !fd2_input_has_any_key(&vga->input));
    CHECK(context->rng.state==expected.state && !memcmp(&context->preparation,state,sizeof(*state)));
    for(int mode=0;mode<3;mode++) {
        controls(vga);clock_ms=110;vga->field_water_phase=3;vga->field_water_last_tick=0;
        fd2_field_rng_seed(&context->rng,0x5a31);expected=context->rng;
        if(mode)(void)fd2_field_rng_next(&expected);
        pulse(1,SDL_SCANCODE_RETURN);
        CHECK(read_key(&ui,&event,mode,0)==0 && event.key==FD2_INPUT_KEY_ENTER);
        CHECK(frame_number==1u && water_calls==(mode==1?1u:0u));
        CHECK(vga->field_water_phase==(mode==1?4u:3u) && context->rng.state==expected.state);
    }
    controls(vga);ui.service=(prepare_service){0};ui.service.notice_origin=36887;
    context->message_new_fragment=context->message_old_fragment=0;context->message_number=7;
    fd2_field_rng_seed(&context->rng,0x5a31);expected=context->rng;
    (void)fd2_field_rng_next(&expected);queue_pair(vga);
    CHECK(prepare_service_text(&ui,490,95,138)==0 && ui.service.input_epoch==1u && !water_calls);
    CHECK(context->rng.state==expected.state && fd2_input_has_any_key(&vga->input));
    unsigned frames_before=frame_number;(void)fd2_field_rng_next(&expected);
    CHECK(prepare_service_wait_key(&ui,&event,0)==0 && event.key==FD2_INPUT_KEY_ENTER);
    CHECK(ui.service.input_epoch==2u && frame_number==frames_before && !water_calls &&
          context->rng.state==expected.state && !fd2_input_has_any_key(&vga->input));
    CHECK(!memcmp(&context->preparation,state,sizeof(*state)));
    controls(vga);queue_pair(vga);
    fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_DOWN,1);
    fd2_input_begin_test_frame(&vga->input,++clock_ms);
    uint64_t repeat=vga->input.next_repeat_ms[SDL_SCANCODE_DOWN];
    vga->input.quit_requested=1;CHECK(prepare_service_clear_keys(&ui)==1);
    CHECK(!fd2_input_has_any_key(&vga->input) && vga->input.quit_requested &&
          vga->input.previous_down[SDL_SCANCODE_DOWN] && vga->input.next_repeat_ms[SDL_SCANCODE_DOWN]==repeat);
    for(int mode=0;mode<3;mode++) {
        controls(vga);queue_pair(vga);vga->input.quit_requested=1;
        prepare_wait waiting={0};fd2_input_event untouched={0};fd2_input_event event_before=untouched;
        fd2_field_rng_seed(&context->rng,0x5a31);
        CHECK(read_key_state(&ui,&untouched,mode,0,&waiting)==1 && !waiting.initialized);
        CHECK(context->rng.state==0x5a31 && !frame_number && !water_calls &&
              !memcmp(&untouched,&event_before,sizeof(untouched)) && fd2_input_has_any_key(&vga->input));
    }
    free(context);return 0;
}

/* 有限copy与源后等待不得借用H deadline；read_key只撤自己的H16 owner。
 * 源1974c无delay，27816有独立10ms；上传代价7ms由测试单独注入。 */
static int prepare_finite_copy_clock_contract(fd2_archive *fdother,
        fd2_field_prepare_assets *assets,const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context));CHECK(context);
    context->initialized=context->has_preparation=1;context->preparation=*state;
    prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    controls(vga);source_upload_cost_ms=7;
    vga->frame_deadline_ns=UINT64_C(999999999);vga->frame_interval_ns=16000000;
    queue_pair(vga);
    uint64_t initial=clock_ms;
    CHECK(present_copy(&ui)==0 && source_uploads==1 && clock_ms==initial+7 && !service_delay_count);
    CHECK(prepare_service_present(&ui,10)==0 && source_uploads==2 && clock_ms==initial+24 &&
        service_delay_count==1 && service_delays[0]==10);
    CHECK(prepare_service_present(&ui,0)==0 && source_uploads==3 && clock_ms==initial+31 &&
        service_delay_count==1 && vga->frame_deadline_ns==UINT64_C(999999999) &&
        vga->frame_interval_ns==16000000 && !water_calls && fd2_input_has_any_key(&vga->input));
    controls(vga);choice_timing_probe=1;quit_frame=1;
    prepare_wait waiting={0};fd2_input_event event={0};
    CHECK(read_key_state(&ui,&event,0,0,&waiting)==1 && choice_timed==1 &&
        vga->frame_deadline_ns==UINT64_C(999999999) && vga->frame_interval_ns==16000000 &&
        !source_uploads && !water_calls);
    free(context);return 0;
}

/* H空等只履行自身：独立timed契约向量为0+7=7/next23，再wait23+7=30/next39。
 * 这不把H16ms称原等待；原26eda/16c57/19953有限尾没有foreign持帧读取。 */
static int prepare_host_ticket_contract(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context));CHECK(context);
    context->initialized=context->has_preparation=1;context->preparation=*state;
    prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    const uint64_t foreign=UINT64_C(999999999),interval=UINT64_C(42000000);
    uint64_t entry_deadline=vga->frame_deadline_ns,entry_interval=vga->frame_interval_ns;
    controls(vga);host_ticket_probe=1;source_upload_cost_ms=7;
    vga->frame_deadline_ns=foreign;vga->frame_interval_ns=interval;
    fd2_field_rng_seed(&context->rng,0x5a31);
    vga->input.queued_keys[0]=(fd2_input_event){.key=FD2_INPUT_KEY_RIGHT};
    vga->input.queued_keys[1]=(fd2_input_event){.key=FD2_INPUT_KEY_ENTER};vga->input.queued_count=2;
    uint64_t own=0;
    CHECK(prepare_host_present(&ui,&own)==0 && clock_ms==7u && own==UINT64_C(23000000));
    CHECK(prepare_host_present(&ui,&own)==0 && clock_ms==30u && own==UINT64_C(39000000));
    CHECK(host_timed_calls==2u && host_wait_ms==16u && !host_deadline_calls &&
        vga->frame_deadline_ns==foreign && vga->frame_interval_ns==interval &&
        context->rng.state==0x5a31 && vga->input.queued_count==2u && !water_calls);
    prepare_host_release(&ui,&own);
    CHECK(!own && clock_ms==30u && vga->frame_deadline_ns==foreign && vga->frame_interval_ns==interval);
    /* 相同数值也不代表同owner；旧release会误清这份外来16ms字段。 */
    own=UINT64_C(39000000);vga->frame_deadline_ns=own;vga->frame_interval_ns=UINT64_C(16000000);
    prepare_host_release(&ui,&own);
    CHECK(!own && vga->frame_deadline_ns==UINT64_C(39000000) && vga->frame_interval_ns==UINT64_C(16000000));

    for(int outcome=0;outcome<4;outcome++) {
        controls(vga);host_ticket_probe=1;source_upload_cost_ms=7;
        vga->frame_deadline_ns=foreign;vga->frame_interval_ns=interval;
        fd2_field_rng_seed(&context->rng,0x5a31);
        prepare_wait waiting={.initialized=1,.countdown=30};fd2_input_event event={0};
        if(outcome==0) { /* early key：无提交、无H等待，FIFO第二项保留。 */
            vga->input.queued_keys[0]=(fd2_input_event){.key=FD2_INPUT_KEY_RIGHT};
            vga->input.queued_keys[1]=(fd2_input_event){.key=FD2_INPUT_KEY_ENTER};vga->input.queued_count=2;
        }
        else if(outcome==1)pulse(2,SDL_SCANCODE_RETURN);
        else if(outcome==2){pulse(2,SDL_SCANCODE_RETURN);quit_frame=2;}
        else host_water_fail_call=2; /* 首次空轮已提交，第二water拒绝。 */
        int got=read_key_state(&ui,&event,outcome==3,0,&waiting);
        CHECK(got==(outcome==2?1:outcome==3?-1:0));
        CHECK(vga->frame_deadline_ns==foreign && vga->frame_interval_ns==interval &&
            context->rng.state==0x5a31 && !host_deadline_calls);
        CHECK(clock_ms==(outcome==0?0u:outcome==3?7u:30u) &&
            host_timed_calls==(outcome==0?0u:outcome==3?1u:2u) &&
            host_wait_ms==(outcome==1 || outcome==2?16u:0u));
        CHECK(water_calls==(outcome==3?2u:0u));
        CHECK(vga->input.queued_count==((outcome==0 || outcome==2)?1u:0u));
        CHECK(outcome>=2 || event.key==(outcome==0?FD2_INPUT_KEY_RIGHT:FD2_INPUT_KEY_ENTER));
    }
    /* 四个真实scope尾：有限开页HOST、资源错误、原fade返回HOST/ERROR，均不等foreign。 */
    char path[FD2_TEST_PATH_MAX];CHECK(fd2_test_temp_path(path)==0 && remove(path)==0);
    CHECK(fd2_save_manual_state_save(path,0,state)==0);ui.save_path=path;
    CHECK(fd2_field_prepare_assets_bind_roster(assets,state)==0);
    for(int scope=0;scope<7;scope++) {
        controls(vga);host_ticket_probe=1;source_upload_cost_ms=7;quit_frame=1;
        vga->frame_deadline_ns=foreign;vga->frame_interval_ns=interval;
        context->preparation=*state;fd2_field_rng_seed(&context->rng,0x5a31);
        int got;
        if(scope==0)got=slots_run(&ui,0,PREPARE_SLOT_TITLE);
        else if(scope==1)got=member_detail(&ui,0);
        else if(scope==2)got=roster_run(&ui);
        else if(scope==3) {
            context->preparation.unit_area[0x0a]=0;context->preparation.unit_area[0x0b]=255;
            got=member_detail(&ui,0);
        } else if(scope<6) {
            title_fade_result=scope==4?FD2_SCENE_RESULT_HOST_QUIT:FD2_SCENE_RESULT_ERROR;
            got=(int)fd2_field_prepare_run(vga,fdother,context,path,NULL);
        } else {
            quit_frame=0;pulse(13,SDL_SCANCODE_ESCAPE);got=member_detail(&ui,0);
        }
        fd2_field_rng expected_rng;fd2_field_rng_seed(&expected_rng,0x5a31);
        if(scope==6)(void)fd2_field_rng_next(&expected_rng); /* 16c57本次等待入口唯一一取。 */
        CHECK(got==(scope<3?1:scope==3?-1:scope==4?FD2_FIELD_PREPARE_HOST_QUIT:scope==5?FD2_FIELD_PREPARE_ERROR:0));
        CHECK(!host_deadline_calls && !host_timed_calls && !host_wait_ms &&
            vga->frame_deadline_ns==foreign && vga->frame_interval_ns==interval &&
            clock_ms==(scope<3?7u:scope==6?198u:0u) && context->rng.state==expected_rng.state && !state_save_calls);
        if(scope==6)CHECK(frame_number==26u && service_delay_count==1u && service_delays[0]==16u &&
            !fd2_input_has_any_key(&vga->input)); /* 26次7ms上传+1次H16，无尾持帧。 */
    }
    CHECK(remove(path)==0);free(context);controls(vga);
    vga->frame_deadline_ns=entry_deadline;vga->frame_interval_ns=entry_interval;return 0;
}

static int member_detail_open_clear_and_flip_preserves_queue(fd2_archive *fdother,
        fd2_field_prepare_assets *assets,const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context));CHECK(context);
    context->initialized=context->has_preparation=1;context->preparation=*state;
    prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    controls(vga);service_exact_time=1;fd2_field_rng_seed(&context->rng,0x5a31);
    fd2_field_rng expected=context->rng;(void)fd2_field_rng_next(&expected);(void)fd2_field_rng_next(&expected);
    pulse(3,SDL_SCANCODE_SPACE); /* 17e0b开动画中的旧键在末尾明确clear。 */
    pulse(13,SDL_SCANCODE_RIGHT);pulse(14,SDL_SCANCODE_RETURN); /* 第二键早到flip，不得吞。 */
    CHECK(member_detail(&ui,1)==0);
    CHECK(frame_number==41u && ui.service.input_epoch==2u && context->rng.state==expected.state);
    CHECK(!fd2_input_has_any_key(&vga->input) && !memcmp(&context->preparation,state,sizeof(*state)));
    free(context);return 0;
}

static int prepare_clear_host_quit_boundaries(fd2_archive *fdother,
        fd2_field_prepare_assets *assets,const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context));CHECK(context);
    context->initialized=context->has_preparation=1;context->preparation=*state;
    CHECK(fd2_field_prepare_assets_open_service(assets,fdother,1)==0);
    prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    controls(vga);quit_on_clear=1;fd2_field_rng_seed(&context->rng,0x5a31);
    fd2_static_flow_node node={.op="clear_keys"};unsigned port=FD2_FLOW_NEXT;
    CHECK(prepare_service_ui(&ui,&node,&port)==-1 && port==FD2_FLOW_ERROR && ui.service.host_quit);
    CHECK(!frame_number && clear_calls==1u && context->rng.state==0x5a31);

    controls(vga);quit_on_clear=1;ui.service=(prepare_service){0};ui.service.selector=1;
    ui.service.active_portraits=assets->service.portraits;ui.service.notice_origin=4283;
    node.op="service_animate";port=FD2_FLOW_NEXT;
    CHECK(prepare_service_ui(&ui,&node,&port)==-1 && port==FD2_FLOW_ERROR && ui.service.host_quit);
    CHECK(clear_calls==1u && frame_number==frames_at_clear[0] && context->rng.state==0x5a31);

    controls(vga);quit_on_clear=1;ui.service=(prepare_service){0};
    CHECK(member_detail(&ui,1)==1 && frame_number==12u && clear_calls==1u);
    CHECK(!ui.service.input_epoch && context->rng.state==0x5a31);

    controls(vga);quit_on_clear=1;
    CHECK(fd2_field_prepare_run(vga,fdother,context,"unused.SAV",NULL)==FD2_FIELD_PREPARE_HOST_QUIT);
    CHECK(clear_calls==1u && frame_number==frames_at_clear[0] && !state_save_calls && context->rng.state==0x5a31);
    CHECK(!memcmp(&context->preparation,state,sizeof(*state)));
    free(context);return 0;
}

static int prepare_service_fade_returns(fd2_archive *fdother,
        fd2_field_prepare_assets *assets,const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context context={0};context.initialized=context.has_preparation=1;
    context.preparation=*state;fd2_field_rng_seed(&context.rng,0x5a31);
    CHECK(fd2_field_prepare_assets_open_service(assets,fdother,1)==0);
    prepare_ui ui={vga,fdother,assets,&context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    for(int in=0;in<2;in++)for(int outcome=0;outcome<3;outcome++) {
        controls(vga);ui.service=(prepare_service){0};
        title_fade_result=outcome==1?FD2_SCENE_RESULT_HOST_QUIT:
            outcome==2?FD2_SCENE_RESULT_ERROR:FD2_SCENE_RESULT_OK;
        fd2_field_play_context before=context;
        CHECK(prepare_service_fade(&ui,in)==(outcome==1?1:outcome==2?-1:0));
        CHECK(title_fade_calls==1u && title_fade_in==in && !title_fade_fast &&
            ui.service.host_quit==(outcome==1) && !memcmp(&before,&context,sizeof(context)));
        CHECK(frame_number==(outcome?0u:in?65u:64u));
    }
    for(int outcome=1;outcome<3;outcome++) {
        controls(vga);ui.service=(prepare_service){0};
        title_fade_result=outcome==1?FD2_SCENE_RESULT_HOST_QUIT:FD2_SCENE_RESULT_ERROR;
        fd2_static_flow_node node={.op="service_enter"};unsigned port=FD2_FLOW_NEXT;
        fd2_field_play_context before=context;
        CHECK(prepare_service_ui(&ui,&node,&port)==-1 && port==FD2_FLOW_ERROR);
        CHECK(title_fade_calls==1u && !frame_number && !service_delay_count &&
            ui.service.host_quit==(outcome==1) && !memcmp(&before,&context,sizeof(context)));
    }
    controls(vga);ui.service=(prepare_service){.host_quit=1};
    CHECK(prepare_service_fade(&ui,1)==1 && !title_fade_calls && !frame_number);
    return 0;
}

/* 不用“给旧pulse加65”代替来源门：直接观察真实Town整页、原1f525
 * 65次64..0/2ms、末DAC全亮、随后唯一clear；早到键在该cutoff才丢。 */
static int town_bootstrap_source_contract(fd2_archive *fdother,
        fd2_field_prepare_assets *assets,const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context));
    fd2_vga *expected=calloc(1,sizeof(*expected));CHECK(context && expected);
    context->initialized=context->has_preparation=1;context->preparation=*state;
    fd2_field_rng_seed(&context->rng,0x5a31);
    CHECK(prepare_town(assets,fdother,state->stage_id)==0);
    CHECK(fd2_field_prepare_draw_town(expected,assets,state,0,0)==0);
    /* 原11eb0只投影312x192；本case进入Town前的VGA外沿是0x61。 */
    for(unsigned y=0;y<200u;y++)for(unsigned x=0;x<320u;x++)
        if(x<4u || x>=316u || y<4u || y>=196u)expected->framebuffer[y*320u+x]=0x61;
    const uint8_t *palette;size_t size;CHECK(fd2_archive_get(fdother,0,&palette,&size)==0 && size>=768u);
    prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    int type=town_type(state->stage_id);
    fd2_image *town=type==0 ? &assets->town:type==1 ? &assets->town_type1:&assets->town_stage2;
    for(int kind=0;kind<3;kind++) {
        controls(vga);service_exact_time=1;town_expected_pixels=expected->framebuffer;
        town_inject_early=1;town_quit_frame=kind==1 ? 3u:0u;
        fd2_vga_set_palette(vga,palette);memset(vga->framebuffer,0x61,VGA_W*VGA_H);queue_pair(vga);
        int saved_width=town->width;if(kind==2)town->width=319;
        int result=prepare_service_execute(&ui,"town.bootstrap",NULL,0);
        town->width=saved_width;
        CHECK(context->rng.state==0x5a31 && !memcmp(&context->preparation,state,sizeof(*state)));
        if(kind==0) {
            CHECK(result==0 && clear_calls==1u && frames_at_clear[0]==65u && town_script_origin==65u);
            CHECK(town_bootstrap_frames==65u && town_fade_count==65u && town_first_black && !town_bad_pixels);
            for(unsigned i=0;i<65u;i++)CHECK(town_fade_values[i]==64-(int)i && service_delays[i]==2u);
            CHECK(service_delay_count==65u && town_pending_at_clear && !fd2_input_has_any_key(&vga->input));
            for(size_t i=0;i<768u;i++)CHECK(vga->dac[i]==(uint8_t)((palette[i]<<2)|(palette[i]>>4)));
            CHECK(!memcmp(vga->framebuffer,expected->framebuffer,VGA_W*VGA_H));
        } else if(kind==1) {
            CHECK(result==-2 && frame_number==3u && !clear_calls && town_fade_count==3u && !town_bad_pixels);
        } else {
            CHECK(result==-1 && !frame_number && !clear_calls && !town_fade_count && fd2_input_has_any_key(&vga->input));
            for(size_t i=0;i<VGA_W*VGA_H;i++)CHECK(vga->framebuffer[i]==0x61);
        }
    }
    free(expected);free(context);return 0;
}

/* 26152/265ec读取同一53F52，不另起城镇phase0；四tick只推53F52。
 * cold runner重入与正式prepare_run均保App53F72；不是用stage0捕获值。
 * 固定PIT1193182/65536下，bootstrap130ms后再等20帧16ms只跨一次四tick门。 */
static int prepare_town_phase_lifecycle(fd2_archive *fdother,
        fd2_field_prepare_assets *assets,const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context context={0};context.initialized=context.has_preparation=1;
    context.preparation=*state;fd2_field_rng_seed(&context.rng,0x5a31);
    fd2_vga *expected=malloc(sizeof(*expected));CHECK(expected);
    CHECK(prepare_town(assets,fdother,state->stage_id)==0);
    prepare_ui ui={vga,fdother,assets,&context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    controls(vga);fd2_bios_clock_reset(0,0);service_exact_time=1;
    vga->prepare_highlight_phase=3;vga->prepare_sprite_phase=2;
    *expected=*vga;
    CHECK(fd2_field_prepare_draw_town(expected,assets,state,0,3)==0);
    for(unsigned repeat=0;repeat<2u;repeat++) {
        CHECK(prepare_service_execute(&ui,"town.bootstrap",NULL,0)==0 &&
            vga->prepare_highlight_phase==3u && vga->prepare_sprite_phase==2u &&
            !memcmp(vga->framebuffer,expected->framebuffer,sizeof(vga->framebuffer)));
    }
    for(unsigned repeat=0;repeat<2u;repeat++) {
        uint32_t highlight=vga->prepare_highlight_phase,sprite=vga->prepare_sprite_phase;
        controls(vga);fd2_bios_clock_reset(0,0);service_exact_time=1;
        vga->prepare_highlight_phase=highlight;vga->prepare_sprite_phase=sprite;
        town_expected_pixels=expected->framebuffer;quit_frame=repeat?20u:1u;
        CHECK(fd2_field_prepare_run(vga,fdother,&context,"unused.SAV",NULL)==FD2_FIELD_PREPARE_HOST_QUIT);
        CHECK(!town_bad_pixels && vga->prepare_highlight_phase==(repeat?0u:3u) &&
            vga->prepare_sprite_phase==2u && frame_number==(repeat?85u:66u));
        CHECK(context.rng.state==0x5a31 && !state_save_calls &&
            !memcmp(&context.preparation,state,sizeof(*state)));
    }
    controls(vga);free(expected);return 0;
}

/* 26152 NoTown 原分支：清屏后先11d40(0,255,0)，才开启75肖像/410。
 * 250cc好结局留下白DAC；入口不得依赖上个场景恰好为normal。
 * 实际冷入口执行到首个notice开框的HOST，检查已到写点及未到业务写点。 */
static int notown_entry_restores_dac(fd2_archive *fdother,
        fd2_field_prepare_assets *assets,const fd2_save_manual_state *state,fd2_vga *vga) {
    const uint8_t *palette;size_t size;
    CHECK(fd2_archive_get(fdother,0,&palette,&size)==0 && size>=VGA_PALETTE_SIZE);
    for(int incoming=0;incoming<3;incoming++) {
        fd2_field_play_context context={0};context.initialized=context.has_preparation=1;
        context.preparation=*state;context.preparation.stage_id=27;
        fd2_field_rng_seed(&context.rng,0x4321u);
        fd2_field_play_context before=context;
        prepare_ui ui={vga,fdother,assets,&context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
        controls(vga);fd2_vga_set_palette(vga,palette);
        if(incoming<2)memset(vga->dac,incoming?255:0,sizeof(vga->dac));
        memset(vga->framebuffer,0x61,VGA_W*VGA_H);quit_frame=3;
        CHECK(prepare_service_execute(&ui,"deployment","notown.mode",2)==-2);
        CHECK(brightness_count==1u && brightness_values[0]==0);
        for(size_t i=0;i<VGA_PALETTE_SIZE;i++)
            CHECK(vga->dac[i]==(uint8_t)((palette[i]<<2)|(palette[i]>>4)));
        CHECK(!memcmp(vga->palette,palette,VGA_PALETTE_SIZE));
        CHECK(!memcmp(&context,&before,sizeof(context)) && !state_save_calls && !music_count);
    }
    return 0;
}

/* 26152/2b67f的原caller消费raw AL20；隐藏店比较原AH，不限物理F键。 */
static int raw_bios_consumer_boundaries(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context context={0};context.initialized=context.has_preparation=1;
    context.preparation=*state;
    prepare_ui ui={vga,fdother,assets,&context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    fd2_static_flow_node pick={.op="pick_companion"};
    for(unsigned i=0;i<2u;i++) {
        controls(vga);quit_frame=1;ui.service=(prepare_service){0};unsigned port=99;
        vga->input.queued_keys[0]=(fd2_input_event){.key=FD2_INPUT_KEY_OTHER,.bios_code=(uint16_t)(0x0020u+i)};
        vga->input.queued_count=1;
        int result=prepare_service_pick(&ui,&pick,&port);
        if(!i)CHECK(result==1 && port==FD2_FLOW_SLOT && !frame_number && ui.service.input_epoch==1u);
        else CHECK(result==-1 && ui.service.host_quit && frame_number==1u);
        CHECK(!fd2_input_has_any_key(&vga->input) && !memcmp(&context.preparation,state,sizeof(*state)));
    }
    for(unsigned i=0;i<2u;i++) {
        controls(vga);quit_frame=2;context.preparation=*state;
        raw_pulse_count=1;raw_pulses[0].frame=1;raw_pulses[0].code=(uint16_t)(0x0020u+i);
        CHECK(fd2_field_prepare_run(vga,fdother,&context,"unused.SAV",NULL)==FD2_FIELD_PREPARE_HOST_QUIT);
        CHECK(music_count==(!i?3u:1u) && music_tracks[0]==10);
        if(!i)CHECK(music_tracks[1]==-1 && music_tracks[2]==13); /* 原town停曲，再进酒店13。 */
        CHECK(!state_save_calls && !memcmp(&context.preparation,state,sizeof(*state)));
    }
    controls(vga);context.preparation=*state;context.preparation.stage_id=13;
    context.bgm=(fd2_bgm_player *)&context;
    const uint8_t *row=fd2_field_stage_prepare_record_get(13);
    CHECK(row && row[1]==2u && row[2]==0x56u && !prepare_is_notown(13));
    pulse(1,SDL_SCANCODE_LEFT);pulse(3,SDL_SCANCODE_LEFT);
    raw_pulse_count=1;raw_pulses[0].frame=5;raw_pulses[0].code=0x565c;
    /* 2670e先缩放十步才请求店曲；以真实14请求为HOST门，不在动画首帧退出。 */
    pulse(7,SDL_SCANCODE_RETURN);quit_on_music_track=14;
    CHECK(fd2_field_prepare_run(vga,fdother,&context,"unused.SAV",NULL)==FD2_FIELD_PREPARE_HOST_QUIT);
    if(music_count!=3u || music_tracks[0]!=10 || music_tracks[1]!=-1 || music_tracks[2]!=14) {
        fprintf(stderr,"hidden town: music_count=%zu frames=%u script=%u clock=%llu saves=%u stage=%u tracks=",
            music_count,frame_number,frame_number-town_script_origin,(unsigned long long)clock_ms,
            state_save_calls,(unsigned)context.preparation.stage_id);
        for(size_t i=0;i<music_count;i++)fprintf(stderr,"%s%d",i?",":"",music_tracks[i]);
        fprintf(stderr,"\n");
    }
    CHECK(music_count==3u && music_tracks[0]==10 && music_tracks[1]==-1 && music_tracks[2]==14);
    CHECK(!state_save_calls);
    CHECK(context.preparation.stage_id==13u);
    return 0;
}

/* 独立源28b41：1/3/4/5四种服务的BIOS调用数；4ms不得混作tick。 */
static int service_animation_tick_contract(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    const int selectors[]={1,3,4,5};const unsigned counts[]={5,2,12,7};
    fd2_field_play_context context={0};context.initialized=context.has_preparation=1;context.preparation=*state;
    prepare_ui ui={vga,fdother,assets,&context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    for(size_t n=0;n<4u;n++) {
        controls(vga);service_exact_time=1;fd2_bios_clock_reset(0,0);
        CHECK(fd2_field_prepare_assets_open_service(assets,fdother,selectors[n])==0);
        ui.service=(prepare_service){0};ui.service.selector=selectors[n];
        ui.service.active_portraits=assets->service.portraits;ui.service.notice_origin=4283;
        CHECK(prepare_service_animate(&ui)==0 && service_tick_count==counts[n]);
        for(unsigned i=0;i<counts[n];i++) {
            unsigned expected=selectors[n]==3 ? (i?8u:1u):
                selectors[n]==4 && i>=10u ? (i==10u?10u:5u):2u;
            CHECK(service_ticks[i]==expected);
        }
        CHECK(service_delay_count==(selectors[n]==4?64u:0u));
        for(unsigned i=0;i<service_delay_count;i++)CHECK(service_delays[i]==4u);
        CHECK(!memcmp(&context.preparation,state,sizeof(*state)) && !state_save_calls);
    }
    return 0;
}

static fd2_vga *deployment_clock_vga;
static int deployment_clock_bad;
static void deployment_clock_order(unsigned read) {
    fd2_field_visual_state *visual=&deployment_clock_vga->field_visual;
    /* 2b67f的fresh53F46已经写6，才到1297d首读；该首读与随后锚读分开。 */
    if(read==3u && (deployment_clock_vga->prepare_pick_tick!=6u ||
            visual->actor_idle_phase!=1u || visual->actor_idle_tick || visual->cursor_phase!=1u))
        deployment_clock_bad=1;
    /* 1297d先提交53C0B=2，重读7写53C0F后才轮到cursor。 */
    if(read==4u && (deployment_clock_vga->prepare_pick_tick!=6u ||
            visual->actor_idle_phase!=2u || visual->actor_idle_tick || visual->cursor_phase!=1u))
        deployment_clock_bad=1;
}

static int deployment_visual_owner_boundary(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context context={0};context.initialized=context.has_preparation=1;context.preparation=*state;
    prepare_ui ui={vga,fdother,assets,&context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
    controls(vga);fd2_bios_clock_reset(0,0);
    fd2_field_rng_seed(&context.rng,0x5a31);
    CHECK(fd2_field_prepare_assets_bind_roster(assets,state)==0 && prepare_service_selection_resources(&ui)==0);
    ui.service.picker_mode=PREPARE_PICK_DEPLOYMENT;ui.service.quota=16;
    vga->field_visual=(fd2_field_visual_state){1,0,1,0,0,0,0};
    uint8_t *pixels=malloc(sizeof(vga->framebuffer));CHECK(pixels);
    memcpy(pixels,vga->framebuffer,sizeof(vga->framebuffer));
    int width=ui.service.selection_label.width;ui.service.selection_label.width=1;
    /* H绘图失败位于已执行1297d之后；保cursor写，不提交pure draw的整页。 */
    CHECK(prepare_service_picker_draw(&ui)==-1 && vga->field_visual.cursor_phase==2u &&
        vga->field_visual.actor_idle_phase==1u && !vga->field_visual.actor_idle_tick && clock_reads==2u &&
        !memcmp(vga->framebuffer,pixels,sizeof(vga->framebuffer)));
    ui.service.selection_label.width=width;
    CHECK(prepare_service_picker_draw(&ui)==0 && vga->field_visual.cursor_phase==3u &&
        vga->field_visual.actor_idle_phase==1u); /* 同tick每次1297d也推进cursor。 */

    /* 原比较5→fresh53F46=6→1297d比较6→fresh53C0F=7；原两层门四读。
     * 275/330/385ms分别位于独立PIT oracle的tick5/6/7，不由consumer造预期。 */
    controls(vga);fd2_bios_clock_reset(0,0);vga->prepare_pick_tick=0;
    vga->field_visual=(fd2_field_visual_state){1,0,1,0,0,0,0};
    const uint64_t ticks[]={275,330,330,385};clock_script_ms=ticks;clock_script_count=4;
    deployment_clock_vga=vga;deployment_clock_bad=0;clock_read_observer=deployment_clock_order;
    ui.service.selection_label.width=1;memcpy(pixels,vga->framebuffer,sizeof(vga->framebuffer));
    fd2_static_flow_node pick={.op="pick_companion"};unsigned port=FD2_FLOW_NEXT;
    CHECK(prepare_service_pick(&ui,&pick,&port)==-1 && clock_reads==4u && !deployment_clock_bad &&
        vga->prepare_pick_tick==6u && vga->field_visual.actor_idle_phase==2u &&
        vga->field_visual.actor_idle_tick==7u && vga->field_visual.cursor_phase==2u &&
        !frame_number && !ui.service.input_epoch && !memcmp(vga->framebuffer,pixels,sizeof(vga->framebuffer)));
    ui.service.selection_label.width=width;

    /* 有早键时2b67f先读键，不触碰53F46/1297d，保余队列。 */
    controls(vga);vga->prepare_pick_tick=9;vga->field_visual=(fd2_field_visual_state){2,7,2,0,0,0,0};
    vga->input.queued_keys[0]=(fd2_input_event){.key=FD2_INPUT_KEY_ENTER,.bios_code=0x1c0d};
    vga->input.queued_keys[1]=(fd2_input_event){.key=FD2_INPUT_KEY_RIGHT,.bios_code=0x4d00};
    vga->input.queued_count=2;
    CHECK(prepare_service_pick(&ui,&pick,&port)==1 && port==FD2_FLOW_SLOT && !clock_reads &&
        vga->prepare_pick_tick==9u && vga->field_visual.cursor_phase==2u && vga->field_visual.actor_idle_tick==7u &&
        fd2_input_has_any_key(&vga->input));
    vga->input.quit_requested=1;fd2_field_visual_state before=vga->field_visual;
    CHECK(prepare_service_pick(&ui,&pick,&port)==-1 && ui.service.host_quit && !clock_reads &&
        vga->prepare_pick_tick==9u && !memcmp(&before,&vga->field_visual,sizeof(before)));
    CHECK(context.rng.state==0x5a31 && !state_save_calls && !memcmp(&context.preparation,state,sizeof(*state)));
    controls(vga);prepare_service_cleanup(&ui,1);free(pixels);return 0;
}

/* 原28efe->1bffe->1b9de：空EQT仍进16c57等待，确认后count0才关闭。
 * 导航期望是原DWORD selector写集，不从pick结果或冷生成器反算。 */
static int equipment_empty_wait_contract(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context));
    fd2_save_manual_state *before=malloc(sizeof(*before));CHECK(context && before);
    context->initialized=context->has_preparation=1;context->preparation=*state;
    for(size_t slot=0;slot<8;slot++)context->preparation.unit_area[0x0au+2u*slot]=0x80u;
    *before=context->preparation;
    CHECK(prepare_equipment_cursor_add(INT32_MAX,1)==INT32_MIN &&
        prepare_equipment_cursor_add(INT32_MIN,-1)==INT32_MAX);
    const fd2_static_flow *flow=fd2_static_flow_lookup("preparation","services");CHECK(flow);
    const fd2_static_flow_node *pick=fd2_static_flow_at(flow,fd2_static_flow_label(flow,"equip.item"));
    const fd2_static_flow_node *empty=pick ? fd2_static_flow_at(flow,fd2_static_flow_edge(flow,pick,FD2_FLOW_SLOT)):NULL;
    CHECK(empty && !strcmp(empty->op,"inventory_empty"));
    const fd2_static_flow_node *close=fd2_static_flow_at(flow,fd2_static_flow_edge(flow,empty,FD2_FLOW_YES));
    CHECK(close && !strcmp(close->op,"equipment_close"));
    const unsigned downs[]={0u,0u,1u,15u};
    const SDL_Scancode final_keys[]={SDL_SCANCODE_RETURN,SDL_SCANCODE_ESCAPE,SDL_SCANCODE_RETURN,SDL_SCANCODE_RETURN};
    for(size_t test=0;test<4;test++) {
        controls(vga);service_exact_time=1;
        prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};
        ui.service.member=0;
        fd2_static_flow_node open={.op="equipment_open"};unsigned port=FD2_FLOW_NEXT;
        CHECK(prepare_service_pick(&ui,&open,&port)==1 && ui.service.page && ui.service.equipment_selected==0);
        unsigned opened_at=frame_number;
        for(unsigned i=0;i<downs[test];i++)pulse(opened_at+2u+2u*i,SDL_SCANCODE_DOWN);
        pulse(opened_at+2u+2u*downs[test],final_keys[test]);
        fd2_field_rng_seed(&context->rng,0x7a18);fd2_field_rng expected=context->rng;
        for(unsigned i=0;i<=downs[test];i++)(void)fd2_field_rng_next(&expected);
        CHECK(prepare_service_pick(&ui,pick,&port)==1 && frame_number>opened_at);
        CHECK(port==(unsigned)(test==1u?FD2_FLOW_CANCEL:FD2_FLOW_SLOT));
        CHECK(ui.service.page && ui.service.equipment_selected==(int)downs[test]);
        CHECK(context->rng.state==expected.state && !memcmp(&context->preparation,before,sizeof(*before)));
        CHECK(prepare_service_domain(&ui,empty,&port)==1 && port==FD2_FLOW_YES);
        fd2_static_flow_node shut={.op="equipment_close"};
        CHECK(prepare_service_pick(&ui,&shut,&port)==1 && !ui.service.page && !ui.service.page_background);
        CHECK(!memcmp(&context->preparation,before,sizeof(*before)) && !state_save_calls);
        prepare_service_cleanup(&ui,1);
    }
    controls(vga);service_exact_time=1;
    prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1, .service = {0}};ui.service.member=0;
    fd2_static_flow_node open={.op="equipment_open"};unsigned port=FD2_FLOW_NEXT;
    CHECK(prepare_service_pick(&ui,&open,&port)==1);quit_frame=frame_number+2u;
    CHECK(prepare_service_pick(&ui,pick,&port)==-1 && ui.service.host_quit && ui.service.page);
    CHECK(!memcmp(&context->preparation,before,sizeof(*before)) && !state_save_calls);
    prepare_service_cleanup(&ui,1);CHECK(!ui.service.page && !ui.service.page_background);
    free(before);free(context);return 0;
}

/* 265ec/2921a：literal source固定写义；不以另一C renderer生成expected。 */
static void prepare_app_rect(uint8_t *p,int x,int y,int w,int h,uint8_t color) {
    for(int row=0;row<h;row++)memset(p+(y+row)*456+x,color,(size_t)w);
}

static int prepare_app_town_and_zoom_source(fd2_archive *other,
        fd2_field_prepare_assets *real_assets,const fd2_save_manual_state *real_state,fd2_vga *vga) {
    const size_t P=FD2_FIELD_PAGE_SIZE;
    uint8_t *before=malloc(P),*want=malloc(P),*actual=malloc(P),*source=malloc(64000);
    uint8_t *town=malloc(64000),*sprites=calloc(12u,576u),panel[62*26],glyph[32]={0};
    uint8_t text[501u*4u];
    fd2_vga *old=malloc(sizeof(*old)),*saved=malloc(sizeof(*saved));
    fd2_field_prepare_assets *a=calloc(1,sizeof(*a));
    fd2_field_play_context *ctx=calloc(1,sizeof(*ctx));
    CHECK(before && want && actual && source && town && sprites && old && saved && a && ctx);
    memcpy(old,vga,sizeof(*old));
    CHECK(fd2_field_page_read(vga->field_page,0,0,456,336,before,P,456)==0);
    uint8_t *original_page=malloc(P);CHECK(original_page);memcpy(original_page,before,P);
    for(size_t i=0;i<501u;i++) {
        unsigned offset=1002u+(unsigned)i*2u;text[2*i]=(uint8_t)offset;text[2*i+1]=(uint8_t)(offset>>8);
        text[offset]=text[offset+1]=0xff;
    }
    CHECK(fd2_text_entry_open_mem(&a->text,text,sizeof(text))==0);
    a->font.glyphs=glyph;a->font.size=sizeof(glyph);a->font.glyph_count=1;a->ready=1;
    a->town=a->town_type1=a->town_stage2=(fd2_image){320,200,town};
    a->town_panel=(fd2_image){62,26,panel};memset(panel,84,sizeof(panel));
    a->sprites.decoded_frames=sprites;a->sprites.frame_count=12;
    memset(sprites,99,576);memset(sprites+576,100,576);memset(sprites+1152,101,576);
    fd2_save_manual_state state={0};state.unit_count=1;state.unit_area[7]=0;
    ctx->initialized=ctx->has_preparation=1;ctx->preparation=*real_state;ctx->preparation.unit_count=1;
    const int stages[]={1,2,11};
    for(size_t stage=0;stage<3;stage++)for(int selected=0;selected<6;selected++)for(unsigned phase=0;phase<4;phase++) {
        controls(vga);state.stage_id=(uint8_t)stages[stage];int type=town_type(state.stage_id);CHECK(type>=0);
        for(size_t i=0;i<P;i++)before[i]=(uint8_t)(i*17u+i/456u+83u);
        for(size_t i=0;i<64000u;i++){town[i]=(uint8_t)(i*11u+i/320u+29u);vga->framebuffer[i]=(uint8_t)(i*13u+7u);}
        CHECK(fd2_field_page_write(vga->field_page,0,0,456,336,before,P,456)==0);
        memcpy(saved,vga,sizeof(*saved));memcpy(want,before,P);
        for(size_t y=0;y<200;y++)memcpy(want+(y+72)*456+72,town+y*320,320);
        prepare_app_rect(want,316,234,62,26,84);
        prepare_app_rect(want,72+k_prepare_town_x[type][selected],72+k_prepare_town_y[type][selected],24,24,
            (uint8_t)(99u+(phase==3u?1u:phase)));
        fd2_save_manual_state state_before=state;
        CHECK(fd2_field_prepare_draw_town(vga,a,&state,selected,phase)==0);
        CHECK(fd2_field_page_read(vga->field_page,0,0,456,336,actual,P,456)==0 && !memcmp(actual,want,P));
        for(int y=0;y<192;y++)memcpy(saved->framebuffer+(y+4)*320+4,want+(y+72)*456+72,312);
        CHECK(!memcmp(saved,vga,sizeof(*vga)) && !memcmp(&state,&state_before,sizeof(state)));
    }
    /* 坏文字在临时页阶段失败，不提前写App或VGA。 */
    memcpy(before,want,P);memcpy(saved,vga,sizeof(*saved));size_t fragments=a->text.fragment_count;a->text.fragment_count=495;
    CHECK(fd2_field_prepare_draw_town(vga,a,&state,0,0)==-1);
    a->text.fragment_count=fragments;
    CHECK(fd2_field_page_read(vga->field_page,0,0,456,336,actual,P,456)==0 && !memcmp(actual,before,P) && !memcmp(saved,vga,sizeof(*vga)));

    for(size_t i=0;i<64000;i++)source[i]=(uint8_t)(i*11u+i/320u*7u+29u);
    for(size_t stage=0;stage<3;stage++)for(int selected=0;selected<6;selected++)for(int phase=0;phase<=10;phase++) {
        controls(vga);service_exact_time=1;ctx->preparation.stage_id=(uint8_t)stages[stage];
        prepare_ui ui={vga,other,real_assets,ctx,NULL,"unused.SAV",-1,0,-1, .service = {0}};
        ui.service.selector=selected;ui.service.town_background=source;
        int type=town_type(ctx->preparation.stage_id),step=128-9*phase;
        int cx=20480+128*(phase*((int)k_prepare_town_x[type][selected]-150)/10);
        int cy=12800+128*(phase*((int)k_prepare_town_y[type][selected]-100)/10);
        CHECK(fd2_field_page_write(vga->field_page,0,0,456,336,before,P,456)==0);memcpy(want,before,P);
        memset(want,0,64000);
        for(int y=0;y<200;y++)for(int x=0;x<320;x++) {
            int sx=cx+(x-160)*step,sy=cy+(y-100)*step;
            if(sx>=0 && sx<40960 && sy>=0 && sy<25600)want[y*320+x]=source[(sy/128)*320+sx/128];
        }
        CHECK(prepare_service_zoom(&ui,phase)==0 && source_uploads==1u && !service_delay_count && !service_tick_count);
        CHECK(fd2_field_page_read(vga->field_page,0,0,456,336,actual,P,456)==0 && !memcmp(actual,want,P));
        CHECK(!memcmp(vga->framebuffer,want,64000));
    }
    /* 同一cold accepted尾；快照只属本次service，成功/HOST都清理，App前缀保留。 */
    for(int stop=0;stop<2;stop++) {
        controls(vga);service_exact_time=1;ctx->preparation.stage_id=11;
        prepare_ui ui={vga,other,real_assets,ctx,NULL,"unused.SAV",-1,0,-1, .service = {0}};
        memcpy(vga->framebuffer,source,64000);CHECK(fd2_field_page_write(vga->field_page,0,0,456,336,before,P,456)==0);
        quit_frame=stop?1u:0u;
        int phase=stop?1:10,step=128-9*phase,cx=20480+128*(phase*(53-150)/10),cy=12800+128*(phase*(163-100)/10);
        memcpy(want,before,P);memset(want,0,64000);
        for(int y=0;y<200;y++)for(int x=0;x<320;x++) {
            int sx=cx+(x-160)*step,sy=cy+(y-100)*step;
            if(sx>=0 && sx<40960 && sy>=0 && sy<25600)want[y*320+x]=source[(sy/128)*320+sx/128];
        }
        CHECK(prepare_service_execute(&ui,"deployment","accepted",2)==(stop?-2:1));
        CHECK(source_uploads==(stop?1u:10u) && !ui.service.town_background && !ui.service.page);
        CHECK(fd2_field_page_read(vga->field_page,0,0,456,336,actual,P,456)==0 && !memcmp(actual,want,P));
    }
    for(int route=0;route<2;route++) {
        controls(vga);service_exact_time=1;ctx->preparation.stage_id=(uint8_t)(route?11:27);
        prepare_ui ui={vga,other,real_assets,ctx,NULL,"unused.SAV",-1,0,-1, .service = {0}};
        CHECK(fd2_field_page_write(vga->field_page,0,0,456,336,before,P,456)==0);
        CHECK(prepare_service_execute(&ui,"deployment",route?"rejected":"notown.dark",2)==(route?0:1));
        CHECK(!ui.service.town_background && source_uploads==(route?0u:1u));
        CHECK(fd2_field_page_read(vga->field_page,0,0,456,336,actual,P,456)==0 && !memcmp(actual,before,P));
    }
    controls(vga);ctx->preparation.stage_id=11;
    prepare_ui bad={vga,other,real_assets,ctx,NULL,"unused.SAV",-1,0,-1, .service = {0}};bad.service.selector=2;bad.service.town_background=source;
    bad.service.host_quit=1;memcpy(saved,vga,sizeof(*saved));
    CHECK(prepare_service_zoom(&bad,11)==-1); /* 参数错误先于既有H。 */
    CHECK(prepare_service_zoom(&bad,1)==1 && !memcmp(saved,vga,sizeof(*vga)));

    /* 独立旧原payload OTHER-061-000@96,165=70；不是从C取expected。
     * 原stage11初始group0空，deployment均16,49；这里只造其一名合法roster前态。 */
    controls(vga);service_exact_time=1;state=*real_state;state.stage_id=11;state.unit_count=1;state.unit_area[7]=0;
    CHECK(prepare_town(real_assets,other,11)==0 && fd2_field_prepare_draw_town(vga,real_assets,&state,2,0)==0);
    CHECK(vga->framebuffer[169u*320u+100u]==70u);
    CHECK(fd2_field_page_read(vga->field_page,178,154,1,1,actual,1,1)==0 && actual[0]==114u);
    memcpy(source,vga->framebuffer,64000);ctx->preparation=state;
    prepare_ui real={vga,other,real_assets,ctx,NULL,"unused.SAV",-1,0,-1, .service = {0}};real.service.selector=2;real.service.town_background=source;
    CHECK(prepare_service_zoom(&real,10)==0);
    CHECK(fd2_field_page_read(vga->field_page,110,86,1,1,actual,1,1)==0 && actual[0]==70u);
    CHECK(fd2_field_page_read(vga->field_page,178,154,1,1,actual,1,1)==0 && actual[0]==114u);
    fd2_field_game *game=calloc(1,sizeof(*game));CHECK(game);
    CHECK(fd2_field_game_open_resources(game,other,11)==0);
    memcpy(&game->units.items[0],state.unit_area,80);game->units.count=1;
    game->units.items[0].x=16;game->units.items[0].y=49;game->units.items[0].direction=game->units.items[0].frame_phase=0;
    CHECK(fd2_field_units_sprite_cache_restore(&game->units)==0 && fd2_field_game_bind_visual(game,vga)==0);
    game->camera_cell_x=game->camera_cell_y=0;game->ordinary_display_mode=0;
    fd2_field_game_render_background(game,vga);
    CHECK(!game->failure && fd2_field_page_read(vga->field_page,110,86,1,1,actual,1,1)==0 && actual[0]==70u && vga->framebuffer[18u*320u+42u]==70u);
    fd2_field_game_close(game);free(game);
    CHECK(fd2_field_page_write(vga->field_page,0,0,456,336,original_page,P,456)==0);*vga=*old;controls(vga);
    fd2_text_entry_close(&a->text);free(a);free(ctx);free(before);free(want);free(actual);free(source);free(town);free(sprites);free(old);free(saved);free(original_page);
    return 0;
}

int main(void) {
    CHECK(default_sound_gain() == 0);
    fd2_archive fdother = {0}; fd2_field_prepare_assets *assets = calloc(1, sizeof(*assets));
    fd2_vga *vga = calloc(1, sizeof(*vga)); fd2_save_manual_state state;
    CHECK(assets && vga && initial_state(&state) == 0);
    vga->field_page=fd2_field_page_create();CHECK(vga->field_page);
    CHECK(fd2_archive_open(&fdother, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_field_prepare_assets_open(assets, &fdother) == 0);
    CHECK(raw_bios_consumer_boundaries(&fdother,assets,&state,vga)==0);
    CHECK(service_animation_tick_contract(&fdother,assets,&state,vga)==0);
    CHECK(deployment_visual_owner_boundary(&fdother,assets,&state,vga)==0);
    CHECK(prepare_app_town_and_zoom_source(&fdother,assets,&state,vga)==0);
    CHECK(notown_entry_restores_dac(&fdother,assets,&state,vga)==0);
    CHECK(town_bootstrap_source_contract(&fdother,assets,&state,vga)==0);
    CHECK(prepare_town_phase_lifecycle(&fdother,assets,&state,vga)==0);
    CHECK(prepare_buffered_input_contract(&fdother,assets,&state,vga)==0);
    CHECK(hotel_wait_write_order(&fdother,assets,&state,vga)==0);
    CHECK(prepare_water_live_order(&fdother,assets,&state,vga)==0);
    CHECK(prepare_phase_owner_contract(&fdother,assets,&state,vga)==0);
    CHECK(prepare_choice_finite_owner(&fdother,assets,&state,vga)==0);
    CHECK(prepare_choice_blink_order(&fdother,assets,&state,vga)==0);
    CHECK(prepare_finite_copy_clock_contract(&fdother,assets,&state,vga)==0);
    CHECK(prepare_host_ticket_contract(&fdother,assets,&state,vga)==0);
    CHECK(member_detail_open_clear_and_flip_preserves_queue(&fdother,assets,&state,vga)==0);
    CHECK(prepare_clear_host_quit_boundaries(&fdother,assets,&state,vga)==0);
    CHECK(prepare_service_fade_returns(&fdother,assets,&state,vga)==0);
    CHECK(service_resource_pages(&fdother, assets, &state, vga) == 0);
    CHECK(service_present_leaves(&fdother,assets,&state,vga)==0);
    CHECK(equipment_empty_wait_contract(&fdother,assets,&state,vga)==0);
    CHECK(service_confirm_shared_owner(&fdother,assets,&state,vga)==0);
    CHECK(service_raw_icon_transparency(&fdother,assets,&state,vga)==0);
    CHECK(picker_retained_pages(&fdother,assets,&state,vga)==0);
    CHECK(picker_retained_modal_boundary(&fdother,assets,&state,vga)==0);
    CHECK(picker_shift_write_set(vga)==0);
    CHECK(picker_steady_dispatch(&fdother,assets,&state,vga)==0);
    CHECK(hotel_arrows_and_initial_wait(&fdother,assets,&state,vga)==0);
    CHECK(service_scroll_and_inventory_prices(&fdother,assets,&state,vga)==0);
    CHECK(service_growth_message_layouts(&fdother,assets,&state,vga)==0);
    CHECK(services_buy_reaches_products(&fdother,assets,&state,vga)==0);
    CHECK(renderer_test(&fdother, assets, &state, vga) == 0);
    CHECK(selector_test() == 0);
    CHECK(title_cancel_and_load(&fdother, &state, vga) == 0);
    CHECK(hotel_save_reopen(&fdother, assets, &state, vga) == 0);
    CHECK(notown_save_direct_picker(&fdother,assets,&state,vga)==0);
    CHECK(save_commits_before_first_close(&fdother,assets,&state,vga)==0);
    CHECK(hotel_load_keeps_sequence(&fdother, assets, &state, vga) == 0);
    CHECK(hotel_notown_and_cache_boundary(&fdother,assets,&state,vga)==0);
    CHECK(town_roundtrip(&fdother, assets, &state, vga) == 0);
    CHECK(failure_and_cancel(&fdother, assets, &state, vga) == 0);
    CHECK(depart_capacity_and_leave(&fdother, assets, &state, vga) == 0);
    CHECK(member_pages(&fdother, assets, &state, vga) == 0);
    CHECK(late_failures(&fdother, assets, &state, vga) == 0);
    CHECK(stage2_preparation_routes(&fdother, assets, &state, vga) == 0);
    fd2_field_prepare_assets_close(assets); fd2_archive_close(&fdother); free(assets);
    fd2_field_page_destroy(vga->field_page);free(vga);
    puts("field_prepare_play: real assets/pure draw/32-roster scroll/title cancel-load/save reopen/town hotel depart/quit passed (no GUI)");
    return 0;
}

/* 正式视觉接口使用NS；本测试仍沿原毫秒调度器。 */
uint64_t prepare_test_ticks_ns(void) {
    if(clock_script_ms) {
        if(clock_reads>=clock_script_count) {
            fprintf(stderr,"unexpected prepare clock read %u\n",clock_reads);exit(1);
        }
        clock_ms=clock_script_ms[clock_reads];
    }
    uint64_t now=prepare_test_ticks()*UINT64_C(1000000);
    clock_reads++;
    if(clock_read_observer)clock_read_observer(clock_reads);
    if(clock_reads==clock_advance_on_read)clock_ms+=clock_read_advance_ms;
    return now;
}
