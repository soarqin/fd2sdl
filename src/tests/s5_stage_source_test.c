#include <stdint.h>
/* S5完整源caller的真实Scene消费门。
 * 240fa/24754/250cc/2548c/25757完整原CPU分支另由verify_s5_stage_callers核；
 * 本TU只替换宿主上传/计时，不把64actor受控状态冒称自然部署或原生GUI。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include "field_game.h"
#include "field_prepare.h"
#include "field_background.h"
#include "field_play.h"
#include "field_item.h"
#include "field_unit_base.h"
#include "static_data.h"
#include "animation.h"
#include "f4_montage_cold_probe.h"
#include "f4_credits_private.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return 1;}}while(0)
static int f4_credits_enabled;
static void f4_credits_before_node(const fd2_static_flow *,const fd2_static_flow_node *);
static void f4_credits_upload(fd2_vga *);
static void f4_credits_input_frame(fd2_input *);
static uint64_t f4_credits_clock(void);
static uint64_t s5_test_ms;
static unsigned s5_clock_reads,s5_water_probe,s5_water_calls,s5_water_bad_owner;
static int s5_water_result;
static fd2_vga *s5_water_owner;
static unsigned s5_presents,s5_uploads,s5_delays,s5_ticks;
static size_t s5_text_entries[16],s5_text_entry_count;
static int s5_visual_timing[1024][4];
static unsigned s5_visual_count,s5_visual_started;
static int s5_ani_status,s5_ani_calls,s5_ani_palette_ok,s5_ani_index,s5_ani_check,s5_ani_real;
static uint32_t s5_ani_delay;
static fd2_vga *s5_active_vga;
static const fd2_static_flow *s5_flow_override;
static unsigned s5_terminal_watch,s5_terminal_seen,s5_terminal_pumps,s5_montage_opens,s5_ending_enters;
static unsigned s5_last_card_started,s5_wait_preflight_calls;
static fd2_field_event_preflight_fn s5_real_resource_preflight;
static unsigned s5_quit_on_delay;
static const char *s5_predicate_last_op,*s5_predicate_last_id;
static void s5_reset_io(fd2_vga *vga);
static const fd2_static_flow *s5_flow_lookup(const char *domain,const char *id) {
    if(s5_flow_override && !strcmp(domain,"stage") && !strcmp(id,s5_flow_override->id))return s5_flow_override;
    return fd2_static_flow_lookup(domain,id);
}
static void s5_upload(fd2_vga *vga){s5_uploads++;f4_credits_upload(vga);}
/* 有限提交的宿主输入泵，时间只由本TU显式源wait/delay推进。 */
static void s5_input_frame(fd2_input *input) {
    if(f4_credits_enabled){f4_credits_input_frame(input);return;}
    s5_presents++;
    fd2_input_set_test_key_state(input,SDL_SCANCODE_RETURN,
        s5_last_card_started?0:(int)(s5_presents&1u));
    fd2_input_begin_test_frame(input,s5_test_ms);
    if(s5_presents>100000u)input->quit_requested=1;
}
static void s5_present(fd2_vga *vga){
    s5_presents++;s5_test_ms+=17;
    fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,
        s5_last_card_started?0:(int)(s5_presents&1u));
    fd2_input_begin_test_frame(&vga->input,s5_test_ms);
    if(s5_presents>100000u)vga->input.quit_requested=1;
}
static void s5_present_timed(fd2_vga *vga,uint32_t ms){
    s5_test_ms+=ms;
    if(s5_terminal_watch && s5_terminal_seen){
        s5_presents++;s5_terminal_pumps++;
        /* 先让原while经历普通ENTER的一轮，再由下一轮宿主泵关闭。 */
        fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,1);
        fd2_input_begin_test_frame(&vga->input,s5_test_ms);
        if(s5_terminal_pumps==2)vga->input.quit_requested=1;
    }else s5_present(vga);
}
static void s5_delay(uint32_t ms){s5_delays++;s5_test_ms+=ms;
    if(s5_quit_on_delay && s5_delays==s5_quit_on_delay)s5_active_vga->input.quit_requested=1;}
static void s5_wait_ticks(uint32_t ticks){s5_ticks+=ticks;s5_test_ms+=(uint64_t)ticks*55u;}
static Uint64 s5_clock(void){s5_clock_reads++;return s5_test_ms;}
/* 本TU的既有固定tick向量；跨读钟及live actor顺序另由独立测试签核。 */
uint64_t s5_clock_ns(void);
static int s5_actor_live(fd2_field_visual_state *state) {
    return fd2_field_visual_actor_step(state,s5_clock_ns());
}
static int s5_terrain_live(fd2_field_visual_state *state) {
    return fd2_field_visual_terrain_step(state,s5_clock_ns());
}
static int s5_range_live(fd2_field_visual_state *state,int32_t override) {
    return fd2_field_visual_range_step(state,s5_clock_ns(),override);
}
static int s5_water_live(fd2_vga *vga) {
    if(s5_water_probe) {
        s5_water_calls++;if(vga!=s5_water_owner)s5_water_bad_owner++;
        return s5_water_result;
    }
    return fd2_field_water_tick(vga,s5_clock_ns());
}
static int s5_background_live(fd2_field_page *page,int dx,int dy,int cx,int cy,int px,int py,int advance) {
    return fd2_field_background_draw(page,dx,dy,cx,cy,px,py,s5_clock_ns(),advance);
}
static int s5_actor_page_live(fd2_vga *vga,fd2_field_page *page,
        const fd2_map_sprite_bank *sprites,const fd2_field_units *units,size_t raw,
        int cx,int cy,uint8_t idle,uint8_t cursor,int offset) {
    if(!vga || !page || !sprites || !units || units->count>FD2_FIELD_MAX_UNITS ||
            raw>=units->count || idle>3u || cursor>3u)return -1;
    if(fd2_field_actor_visual_tick(vga,s5_clock_ns())<0)return -1;
    return fd2_field_units_render_page_actor(page,sprites,units,raw,cx,cy,
        idle,cursor,vga->actor_jitter_phase,offset);
}
static int s5_actors_page_live(fd2_vga *vga,fd2_field_page *page,
        const fd2_map_sprite_bank *sprites,const fd2_field_units *units,
        int cx,int cy,uint8_t idle,uint8_t cursor) {
    if(!vga || !page || !sprites || !units || units->count>FD2_FIELD_MAX_UNITS ||
            idle>3u || cursor>3u)return -1;
    for(size_t raw=0;raw<units->count;raw++)if(!fd2_field_unit_is_hidden(&units->items[raw])) {
        if(fd2_field_actor_visual_tick(vga,s5_clock_ns())<0)return -1;
        break;
    }
    return fd2_field_units_render_page(page,sprites,units,cx,cy,idle,cursor,vga->actor_jitter_phase);
}
static const fd2_static_flow_node *s5_node_at(const fd2_static_flow *flow,int pc){
    const fd2_static_flow_node *node=fd2_static_flow_at(flow,pc);
    f4_cold_before_node(flow,node);
    f4_credits_before_node(flow,node);
    if(node && !strncmp(flow->id,"stage.source.",13)){
        s5_predicate_last_op=node->op;s5_predicate_last_id=node->id;
    }
    if(node && !strcmp(flow->id,"stage.cinematic.card_last"))s5_last_card_started=1;
    if(node && s5_terminal_watch){
        if(!strcmp(node->op,"cin_montage_open"))s5_montage_opens++;
        if(!strcmp(node->op,"call_cinematic_ending"))s5_ending_enters++;
        if(!strcmp(node->op,"terminal_wait")){
            s5_terminal_seen++;
        }
    }
    if(node && !strcmp(flow->id,"stage.source.victory.028")){
        int kind=!strcmp(node->op,"palette_add")?1:!strcmp(node->op,"delay")?2:0;
        if(kind==1)s5_visual_started=1;
        if(kind && s5_visual_started){
            if(s5_visual_count>=1024u)abort();
            int *entry=s5_visual_timing[s5_visual_count++];entry[0]=kind;
            memcpy(entry+1,node->args,3*sizeof(int32_t));
        }
    }
    return node;
}
static int s5_text_open(fd2_text_entry *text,const fd2_archive *archive,size_t entry){
    if(s5_text_entry_count<16)s5_text_entries[s5_text_entry_count++]=entry;
    return fd2_text_entry_open_entry(text,archive,entry);
}
/* 同一ANI decoder，只替换窗口/时间；保现spy模式专测错误/HOST前缀。 */
#define rd_u16_le s5_ani_rd_u16_le
#define fd2_vga_present s5_present
#define fd2_vga_upload_current_frame s5_upload
#define fd2_delay_ms s5_delay
#define fd2_animation_play_hooked s5_decode_animation_play_hooked
#define fd2_animation_play s5_decode_animation_play
#include "../animation.c"
#undef rd_u16_le
#undef fd2_vga_present
#undef fd2_vga_upload_current_frame
#undef fd2_delay_ms
#undef fd2_animation_play_hooked
#undef fd2_animation_play
static int s5_animation_play_hooked(fd2_vga *vga,const fd2_archive *ani,int index,
        uint32_t delay,int check,fd2_animation_frame_hook hook,void *userdata){
    (void)ani;s5_ani_calls++;s5_ani_index=index;s5_ani_delay=delay;s5_ani_check=check;
    if(s5_ani_real)return s5_decode_animation_play_hooked(vga,ani,index,delay,check,hook,userdata);
    uint8_t base[768],dac[768];memcpy(base,vga->palette,768);memcpy(dac,vga->dac,768);
    s5_ani_palette_ok=1;
    for(unsigned frame=0;frame<3;frame++){
        memset(vga->palette,0xaa,768);memset(vga->dac,0xbb,768);vga->framebuffer[0]=(uint8_t)(frame+41u);
        if(hook)hook(userdata,(uint16_t)frame);
        if(memcmp(base,vga->palette,768) || memcmp(dac,vga->dac,768) || vga->framebuffer[0]!=frame+41u)
            s5_ani_palette_ok=0;
    }
    return s5_ani_status;
}
#define fd2_scene_palette_fade s5_fd2_scene_palette_fade
#define fd2_scene_stage_transition_finish s5_fd2_scene_stage_transition_finish
#define fd2_scene_play_teleport_scope s5_fd2_scene_play_teleport_scope
#define fd2_scene_play_new_game_prologue s5_fd2_scene_play_new_game_prologue
#define fd2_scene_play_new_game_prologue_handoff s5_fd2_scene_play_new_game_prologue_handoff
#define fd2_scene_play_new_game_prologue_handoff_with_runtime s5_fd2_scene_play_new_game_prologue_handoff_with_runtime
#define fd2_scene_play_common_step s5_fd2_scene_play_common_step
#define fd2_scene_common_step_validate s5_fd2_scene_common_step_validate
#define fd2_scene_common_step_validate_view s5_fd2_scene_common_step_validate_view
#define fd2_scene_common_step_validate_resource_view s5_fd2_scene_common_step_validate_resource_view
#define fd2_scene_arrival_visual_validate s5_fd2_scene_arrival_visual_validate
#define fd2_scene_arrival_visual_prepare s5_fd2_scene_arrival_visual_prepare
#define fd2_scene_arrival_visual_play s5_fd2_scene_arrival_visual_play
#define fd2_scene_arrival_visual_close s5_fd2_scene_arrival_visual_close
#define fd2_scene_stage_victory_validate s5_fd2_scene_stage_victory_validate
#define fd2_scene_play_stage_entry s5_fd2_scene_play_stage_entry
#define fd2_scene_play_stage_victory s5_fd2_scene_play_stage_victory
#define fd2_scene_play_stage_source_flow s5_fd2_scene_play_stage_source_flow
#define fd2_scene_play_stage1_entry s5_fd2_scene_play_stage1_entry
#define fd2_scene_play_stage2_entry s5_fd2_scene_play_stage2_entry
#define fd2_scene_play_stage1_victory s5_fd2_scene_play_stage1_victory
#define fd2_scene_play_field_story s5_fd2_scene_play_field_story
#define fd2_scene_field_story_validate s5_fd2_scene_field_story_validate
#define fd2_scene_play_game_over s5_fd2_scene_play_game_over
#define fd2_scene_field_message_validate s5_fd2_scene_field_message_validate
#define fd2_scene_field_notice_page s5_fd2_scene_field_notice_page
#define fd2_scene_field_message_box s5_fd2_scene_field_message_box
#define fd2_scene_field_message_arrow s5_fd2_scene_field_message_arrow
#include "scene.h"
#define fd2_vga_upload_current_frame s5_upload
#define fd2_input_begin_frame s5_input_frame
#define fd2_vga_present s5_present
#define fd2_vga_present_timed s5_present_timed
#define fd2_delay_ms s5_delay
#define fd2_wait_ticks s5_wait_ticks
#define SDL_GetTicks s5_clock
uint64_t s5_clock_ns(void);
#define SDL_GetTicksNS s5_clock_ns
#define fd2_field_visual_actor_step_live s5_actor_live
#define fd2_field_visual_terrain_step_live s5_terrain_live
#define fd2_field_visual_range_step_live s5_range_live
#define fd2_field_water_tick_live s5_water_live
#define fd2_field_background_draw_live s5_background_live
#define fd2_field_units_render_page_actor_live s5_actor_page_live
#define fd2_field_units_render_page_live s5_actors_page_live
#define fd2_static_flow_lookup s5_flow_lookup
#define fd2_text_entry_open_entry s5_text_open
#define fd2_static_flow_at s5_node_at
#define fd2_animation_play_hooked s5_animation_play_hooked
#define fd2_scene_cinematic_open f4_credits_private_open
#define fd2_scene_cinematic_close f4_credits_private_close
#define fd2_scene_cinematic_page_alloc f4_credits_private_page_alloc
#define fd2_scene_cinematic_page_release f4_credits_private_page_release
#define fd2_scene_cinematic_page_clear f4_credits_private_page_clear
#define fd2_scene_cinematic_work_clear f4_credits_private_work_clear
#define fd2_scene_cinematic_copy f4_credits_private_copy
#define fd2_scene_cinematic_pixels f4_credits_private_pixels
#define fd2_scene_cinematic_sheet_open f4_credits_private_sheet_open
#define fd2_scene_cinematic_sheet_close f4_credits_private_sheet_close
#define fd2_scene_cinematic_sheet_info f4_credits_private_sheet_info
#define fd2_scene_cinematic_sheet_duration f4_credits_private_sheet_duration
#define fd2_scene_cinematic_sheet_blit f4_credits_private_sheet_blit
#define fd2_scene_cinematic_image_blit f4_credits_private_image_blit
#define fd2_scene_cinematic_palette_save f4_credits_private_palette_save
#define fd2_scene_cinematic_palette_validate f4_credits_private_palette_validate
#define fd2_scene_cinematic_palette_load f4_credits_private_palette_load
#define fd2_scene_cinematic_palette_restore f4_credits_private_palette_restore
#define fd2_scene_cinematic_audio_open f4_credits_private_audio_open
#define fd2_scene_cinematic_audio_play f4_credits_private_audio_play
#define fd2_scene_cinematic_audio_close f4_credits_private_audio_close
#define fd2_scene_cinematic_circle_prepare f4_credits_private_circle_prepare
#define fd2_scene_cinematic_circle_frame f4_credits_private_circle_frame
#define fd2_scene_cinematic_circle_close f4_credits_private_circle_close
#define fd2_scene_cinematic_shake_prepare f4_credits_private_shake_prepare
#define fd2_scene_cinematic_shake_frame f4_credits_private_shake_frame
#define fd2_scene_cinematic_credits_actor f4_credits_private_credits_actor
#define fd2_scene_cinematic_credits_assets_open f4_credits_private_credits_assets_open
#define fd2_scene_cinematic_credits_assets_close f4_credits_private_credits_assets_close
#define fd2_scene_cinematic_credits_intro f4_credits_private_credits_intro
#define fd2_scene_cinematic_credits_intro_finish f4_credits_private_credits_intro_finish
#define fd2_scene_cinematic_credits_idle_reset f4_credits_private_credits_idle_reset
#define fd2_scene_cinematic_credits_idle f4_credits_private_credits_idle
#define fd2_scene_cinematic_credits_part f4_credits_private_credits_part
#define fd2_scene_cinematic_credits_part_info f4_credits_private_credits_part_info
#define fd2_scene_cinematic_credits_part_count f4_credits_private_credits_part_count
#define fd2_scene_cinematic_credits_card_prepare f4_credits_private_credits_card_prepare
#define fd2_scene_cinematic_credits_blink_reset f4_credits_private_credits_blink_reset
#define fd2_scene_cinematic_credits_card f4_credits_private_credits_card
#define fd2_scene_cinematic_credits_actor_close f4_credits_private_credits_actor_close
#define fd2_scene_cinematic_credits_skip_set f4_credits_private_credits_skip_set
#define fd2_scene_cinematic_credits_skip_get f4_credits_private_credits_skip_get
#define fd2_scene_cinematic_credits_roll_text f4_credits_private_credits_roll_text
#define fd2_scene_cinematic_montage_open f4_credits_private_montage_open
#define fd2_scene_cinematic_montage_close f4_credits_private_montage_close
#define fd2_scene_cinematic_montage_intro f4_credits_private_montage_intro
#define fd2_scene_cinematic_montage_intro_finish f4_credits_private_montage_intro_finish
#define fd2_scene_cinematic_montage_half f4_credits_private_montage_half
#define fd2_scene_cinematic_montage_current_half f4_credits_private_montage_current_half
#define fd2_scene_cinematic_montage_part_count f4_credits_private_montage_part_count
#define fd2_scene_cinematic_montage_part_info f4_credits_private_montage_part_info
#define fd2_scene_cinematic_montage_rng f4_credits_private_montage_rng
#define fd2_scene_cinematic_montage_hit f4_credits_private_montage_hit
#define fd2_scene_cinematic_montage_cue f4_credits_private_montage_cue
#define fd2_scene_cinematic_montage_frame f4_credits_private_montage_frame
#undef fd2_scene_cinematic_montage_open
#undef fd2_scene_cinematic_montage_intro
#undef fd2_scene_cinematic_montage_intro_finish
#undef fd2_scene_cinematic_montage_half
#undef fd2_scene_cinematic_montage_rng
#undef fd2_scene_cinematic_montage_hit
#undef fd2_scene_cinematic_montage_cue
#undef fd2_scene_cinematic_montage_frame
#undef fd2_scene_cinematic_montage_close
#define fd2_scene_cinematic_montage_open f4_cold_open
#define fd2_scene_cinematic_montage_intro f4_cold_intro
#define fd2_scene_cinematic_montage_intro_finish f4_cold_finish
#define fd2_scene_cinematic_montage_half f4_cold_half
#define fd2_scene_cinematic_montage_rng f4_cold_rng
#define fd2_scene_cinematic_montage_hit f4_cold_hit
#define fd2_scene_cinematic_montage_cue f4_cold_cue
#define fd2_scene_cinematic_montage_frame f4_cold_frame
#define fd2_scene_cinematic_montage_close f4_cold_close
#define fd2_field_game_stage_source_node f4_cold_game_node
#include "../scene.c"
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

#undef fd2_input_begin_frame
#undef fd2_scene_cinematic_montage_open
#undef fd2_scene_cinematic_montage_intro
#undef fd2_scene_cinematic_montage_intro_finish
#undef fd2_scene_cinematic_montage_half
#undef fd2_scene_cinematic_montage_rng
#undef fd2_scene_cinematic_montage_hit
#undef fd2_scene_cinematic_montage_cue
#undef fd2_scene_cinematic_montage_frame
#undef fd2_scene_cinematic_montage_close
#undef fd2_field_game_stage_source_node
#undef fd2_scene_palette_fade
#undef fd2_scene_stage_transition_finish
#undef fd2_scene_play_teleport_scope
#undef fd2_scene_play_new_game_prologue
#undef fd2_scene_play_new_game_prologue_handoff
#undef fd2_scene_play_new_game_prologue_handoff_with_runtime
#undef fd2_scene_play_common_step
#undef fd2_scene_common_step_validate
#undef fd2_scene_common_step_validate_view
#undef fd2_scene_common_step_validate_resource_view
#undef fd2_scene_arrival_visual_validate
#undef fd2_scene_arrival_visual_prepare
#undef fd2_scene_arrival_visual_play
#undef fd2_scene_arrival_visual_close
#undef fd2_scene_stage_victory_validate
#undef fd2_scene_play_stage_entry
#undef fd2_scene_play_stage_victory
#undef fd2_scene_play_stage_source_flow
#undef fd2_scene_play_stage1_entry
#undef fd2_scene_play_stage2_entry
#undef fd2_scene_play_stage1_victory
#undef fd2_scene_play_field_story
#undef fd2_scene_field_story_validate
#undef fd2_scene_play_game_over
#undef fd2_scene_field_message_validate
#undef fd2_scene_field_notice_page
#undef fd2_scene_field_message_box
#undef fd2_scene_field_message_arrow
#undef fd2_vga_upload_current_frame
#undef fd2_vga_present
#undef fd2_vga_present_timed
#undef fd2_delay_ms
#undef fd2_wait_ticks
#undef SDL_GetTicks
#undef fd2_field_visual_actor_step_live
#undef fd2_field_visual_terrain_step_live
#undef fd2_field_visual_range_step_live
#undef fd2_field_water_tick_live
#undef fd2_field_background_draw_live
#undef fd2_field_units_render_page_actor_live
#undef fd2_field_units_render_page_live
#undef fd2_static_flow_lookup
#undef fd2_text_entry_open_entry
#undef fd2_static_flow_at
#undef fd2_animation_play_hooked
static void s5_reset_io(fd2_vga *vga){
    s5_presents=s5_uploads=s5_delays=s5_ticks=0;s5_test_ms=1000;s5_text_entry_count=0;
    s5_active_vga=vga;s5_flow_override=NULL;
    s5_visual_count=s5_visual_started=0;
    s5_ani_status=s5_ani_calls=s5_ani_palette_ok=s5_ani_index=s5_ani_check=s5_ani_real=0;s5_ani_delay=0;
    s5_terminal_watch=s5_terminal_seen=s5_terminal_pumps=s5_montage_opens=s5_ending_enters=0;
    s5_last_card_started=s5_wait_preflight_calls=0;
    s5_real_resource_preflight=NULL;s5_quit_on_delay=0;
    s5_predicate_last_op=s5_predicate_last_id=NULL;
    scene_host_quit_requested=scene_render_error=0;
}
#include "s5_cinematic_flow_checks.h"

/* 只指定已证有效下标/字段，不借source after构造expected。
 * 64条相同合法sprite使script按textID定位均有真实actor；不改变原资源。 */
static int s5_prepare(fd2_field_game *game,const fd2_archive *other,unsigned stage){
    fd2_save_manual_state input={0};input.stage_id=(uint8_t)stage;input.unit_count=20;
    for(unsigned i=0;i<20;i++){fd2_field_unit unit={0};
        CHECK(fd2_field_unit_persistent_create(&unit,(uint8_t)i)==0);
        memcpy(input.unit_area+i*80,&unit,80);
    }
    char id[64];snprintf(id,sizeof(id),"stage.source.entry.%03u",stage);
    CHECK(fd2_field_game_prepare_stage_source_flow(game,other,&input,id)==0);
    CHECK(game->map.width>10 && game->map.height>10);
    memset(&game->units,0,sizeof(game->units));
    for(unsigned i=0;i<64;i++){
        CHECK(fd2_field_units_set(&game->units,i,0,(uint8_t)i,2,
            3+(int)(i%5u),3+(int)(i/5u)%(int)(game->map.height-4u))==0);
        fd2_field_unit *unit=&game->units.items[i];unit->hp=unit->hp_max=100;unit->mp=unit->mp_max=10;
        unit->level=1;unit->movement_points=4;
        for(unsigned slot=0;slot<8;slot++){((uint8_t *)unit)[10+slot*2]=0x80;((uint8_t *)unit)[11+slot*2]=255;}
    }
    game->turn_number=1;game->camera_cell_x=game->camera_cell_y=0;
    game->cursor_cell_x=game->cursor_cell_y=4;game->ordinary_display_mode=1;
    return 0;
}
typedef struct {unsigned stage,stop,count,rng;int stories[64];} s5_trace;
static uint32_t s5_rng(void *data){((s5_trace *)data)->rng++;return 7;}
static int s5_preflight(const fd2_field_game *game,const fd2_static_flow_node *node,
        const fd2_field_event_resource_view *view,void *data){
    (void)game;(void)view;s5_trace *trace=data;
    if(!strcmp(node->op,"story")){
        if(trace->count>=64)return -1;trace->stories[trace->count++]=node->args[0];
        if((trace->stage==20 && (node->args[0]==6 || node->args[0]==7)) ||
            (trace->stage==22 && (trace->stop ? node->args[0]>=10 && node->args[0]<=13 : node->args[0]==14)) ||
            (trace->stage==26 && trace->stop && (node->args[0]==9 || node->args[0]==13)) ||
            (trace->stage==28 && node->args[0]==(trace->stop?11:15)) || (trace->stage==29 && node->args[0]==0))return -1;
    }
    return 0;
}
static void s5_item(fd2_field_unit *unit,unsigned slot,unsigned id){
    ((uint8_t *)unit)[10+slot*2]=0;((uint8_t *)unit)[11+slot*2]=(uint8_t)id;
}
/* CLI每行stage/variant/turn。stdout只含机读结果，实际资源/Scene错误保留stderr。 */
static int s5_vector(const fd2_archive *other,unsigned stage,unsigned variant,int turn,int emit){
    fd2_field_game *game=calloc(1,sizeof(*game));fd2_vga *vga=calloc(1,sizeof(*vga));
    CHECK(game && vga);CHECK(s5_prepare(game,other,stage)==0);
    vga->field_page=fd2_field_page_create();CHECK(vga->field_page);fd2_input_init(&vga->input);
    CHECK(fd2_field_game_bind_visual(game,vga)==0);
    const uint8_t *palette;size_t palette_size;CHECK(fd2_archive_get(other,0,&palette,&palette_size)==0 && palette_size>=768);
    fd2_vga_set_palette(vga,palette);s5_reset_io(vga);game->turn_number=(uint32_t)turn;
    s5_trace trace={stage,1,0,0,{0}};game->event_preflight=s5_preflight;game->event_preflight_userdata=&trace;
    game->attack_rng=s5_rng;game->attack_rng_userdata=&trace;
    if(stage==20){
        if(variant<=7)for(unsigned i=0;i<variant;i++)s5_item(&game->units.items[i],0,209);
        else if(variant==8)for(unsigned i=0;i<6;i++)s5_item(&game->units.items[0],i,209+i);
        else if(variant==9)for(unsigned i=0;i<6;i++)s5_item(&game->units.items[0],i,209);
        else CHECK(0);
    } else if(stage==22){
        if(variant&1u)s5_item(&game->units.items[0],0,100);
        if(!(variant&2u))game->roster[18].text_id=70;
        trace.stop=!(variant&8u);
    } else if(stage==26){
        if(variant&1u)s5_item(&game->units.items[0],0,100);
        trace.stop=!(variant&2u);
    } else if(stage==28)trace.stop=!(variant&1u);
    else CHECK(stage==29);
    unsigned raw20_cache=game->units.items[20].sprite_cache_class;
    char id[64];snprintf(id,sizeof(id),"stage.source.victory.%03u",stage);
    int result=s5_fd2_scene_play_stage_source_flow(vga,other,game,NULL,1,id);
    int prepared=-2,notown=fd2_field_prepare_stage_is_notown(game->stage);
    fd2_save_manual_state preparation={0};
    if(stage==26 && (variant&2u) && result==0)prepared=fd2_field_game_export_preparation(game,&preparation);
    if(emit){
        printf("{\"stage\":%u,\"variant\":%u,\"turn\":%d,\"result\":%d,\"stories\":[",stage,variant,turn,result);
        for(unsigned i=0;i<trace.count;i++)printf("%s%d",i?",":"",trace.stories[i]);
        printf("],\"hot_stage\":%zu,\"count\":%zu,\"roster_count\":%zu,\"rng_calls\":%u,\"raw20\":[%u,%u,%u,%u],\"raw20_cache_before\":%u,\"NoTown\":%d,\"export_preparation\":%d,\"prepared_stage\":%u,\"text_entries\":[",
            game->stage,game->units.count,game->roster_count,trace.rng,game->units.items[20].unit_id,
            game->units.items[20].text_id,game->units.items[20].flags,game->units.items[20].sprite_cache_class,
            raw20_cache,notown,prepared,(unsigned)preparation.stage_id);
        for(size_t i=0;i<s5_text_entry_count;i++)printf("%s%zu",i?",":"",s5_text_entries[i]);
        printf("],\"visual_timing\":[");
        for(unsigned i=0;i<s5_visual_count;i++)printf("%s[%d,%d,%d,%d]",i?",":"",s5_visual_timing[i][0],
            s5_visual_timing[i][1],s5_visual_timing[i][2],s5_visual_timing[i][3]);
        puts("]}");
    }else{
        CHECK(trace.count && !trace.rng);
        if(stage==20)CHECK(result==-1 && trace.stories[trace.count-1]==((variant==6 || variant==8)?7:6));
        if(stage==22)CHECK(result==-1 && trace.stories[trace.count-1]==((variant&2u)?10:(turn<15?13:12)));
        if(stage==26 && trace.stop)CHECK(result==-1 && trace.stories[trace.count-1]==((variant&1u)?9:13));
        if(stage==26 && !trace.stop)CHECK(result==0 && prepared==0 && notown && preparation.stage_id==27);
        if(stage==28)CHECK(result==-1 && trace.stories[trace.count-1]==11 && game->units.items[20].unit_id==126 &&
            game->units.items[20].text_id==126 && game->units.items[20].sprite_cache_class==raw20_cache && (game->units.items[20].flags&1u));
        if(stage==29)CHECK(result==-1 && trace.stories[trace.count-1]==0 && game->stage==30);
    }
    fd2_field_game_close(game);fd2_field_page_destroy(vga->field_page);free(vga);free(game);return 0;
}
typedef struct {const fd2_archive *other;unsigned source_stage,validated,target;} s5_owner_trace;
static int s5_owner_preflight(const fd2_field_game *game,const fd2_static_flow_node *node,
        const fd2_field_event_resource_view *view,void *userdata){
    s5_owner_trace *trace=userdata;
    if(strcmp(node->op,"story"))return 0;
    CHECK(view && view->units==&game->units && view->roster==game->roster &&
        view->roster_count==game->roster_count && view->text_override);
    size_t expected=trace->source_stage==29 && game->stage==30 ? 31:trace->source_stage+1u;
    const uint8_t *bytes;size_t size;
    CHECK(fd2_archive_get(&game->fdtxt_archive,expected,&bytes,&size)==0);
    const fd2_text_entry *borrowed=view->text_override;fd2_text_entry before=*borrowed;
    CHECK(borrowed->data==bytes && borrowed->size==size && borrowed->fragment_count>0 && borrowed->offsets);
    size_t offset_bytes=borrowed->fragment_count*sizeof(*borrowed->offsets);
    uint16_t *offsets=malloc(offset_bytes);CHECK(offsets);memcpy(offsets,borrowed->offsets,offset_bytes);
    size_t open_count=s5_text_entry_count;
    CHECK(s5_fd2_scene_common_step_validate_resource_view(game,trace->other,node,view)==0);
    CHECK(!memcmp(&before,borrowed,sizeof(before)) && !memcmp(offsets,borrowed->offsets,offset_bytes));
    /* 缺片段与损坏借用入口必须拒绝，不能为通过而另按热stage重开文本。 */
    fd2_static_flow_node missing=*node;missing.args[0]=INT_MAX;
    CHECK(s5_fd2_scene_common_step_validate_resource_view(game,trace->other,&missing,view)==-1);
    fd2_text_entry invalid=*borrowed;invalid.fragment_count=0;
    fd2_field_event_resource_view bad_view=*view;bad_view.text_override=&invalid;
    CHECK(s5_fd2_scene_common_step_validate_resource_view(game,trace->other,node,&bad_view)==-1);
    CHECK(s5_fd2_scene_common_step_validate_resource_view(game,trace->other,node,NULL)==-1);
    CHECK(!memcmp(&before,borrowed,sizeof(before)) && !memcmp(offsets,borrowed->offsets,offset_bytes));
    CHECK(s5_text_entry_count==open_count);free(offsets);trace->validated++;
    if((trace->source_stage==22 && node->args[0]==14) || (trace->source_stage==29 && node->args[0]==0)){
        CHECK(game->stage==(trace->source_stage==22?23u:30u));trace->target++;
        return -1; /* 真实validator已通过；只在声明的收口点停止本机制夹具。 */
    }
    return 0;
}
static int s5_text_owner_case(const fd2_archive *other,unsigned stage){
    fd2_field_game *game=calloc(1,sizeof(*game));fd2_vga *vga=calloc(1,sizeof(*vga));
    CHECK(game && vga && s5_prepare(game,other,stage)==0);
    vga->field_page=fd2_field_page_create();CHECK(vga->field_page);fd2_input_init(&vga->input);
    /* 与正式Play相同，Scene/teleport共借此VGA唯一visual与背景owner。 */
    CHECK(fd2_field_game_bind_visual(game,vga)==0);
    const uint8_t *palette;size_t size;CHECK(fd2_archive_get(other,0,&palette,&size)==0 && size>=768);
    fd2_vga_set_palette(vga,palette);s5_reset_io(vga);
    if(stage==22){game->roster[18].text_id=70;game->turn_number=14;}
    s5_owner_trace trace={other,stage,0,0};game->event_preflight=s5_owner_preflight;game->event_preflight_userdata=&trace;
    char id[64];snprintf(id,sizeof(id),"stage.source.victory.%03u",stage);
    CHECK(s5_fd2_scene_play_stage_source_flow(vga,other,game,NULL,1,id)==-1);
    CHECK(trace.validated>=3 && trace.target==1);
    CHECK(s5_text_entry_count==(stage==22?1u:2u) && s5_text_entries[0]==stage+1u);
    if(stage==29)CHECK(s5_text_entries[1]==31);
    fd2_field_game_close(game);fd2_field_page_destroy(vga->field_page);free(vga);free(game);return 0;
}
typedef struct {unsigned rotations,closed,stories,rng;int host;} s5_failure_trace;
static uint32_t s5_failure_rng(void *userdata){((s5_failure_trace *)userdata)->rng++;return 7;}
static int s5_failure_story(const fd2_field_game *game,const fd2_static_flow_node *node,
        const fd2_field_event_resource_view *view,void *userdata){
    (void)game;(void)view;s5_failure_trace *trace=userdata;
    if(!strcmp(node->op,"story")){trace->stories++;return node->args[0]==10?0:-1;}
    return 0;
}
static int s5_failure_runtime(fd2_field_game *game,fd2_field_runtime_present_step step,
        int value,void *userdata){
    s5_failure_trace *trace=userdata;
    if(step==FD2_FIELD_RUNTIME_DEFEAT_ROTATE_ACTOR){
        trace->rotations++;CHECK(value==(int)trace->rotations-1 && game->units.items[value].direction==0);
        if(trace->rotations==2)return trace->host?1:-1;
    }
    if(step==FD2_FIELD_RUNTIME_DEFEAT_CLOSE)trace->closed++;
    return 0;
}
static int s5_game_leaf_failure_prefix(const fd2_archive *other){
    for(int host=0;host<2;host++){
        fd2_field_game *game=calloc(1,sizeof(*game));fd2_vga *vga=calloc(1,sizeof(*vga));
        CHECK(game && vga && s5_prepare(game,other,28)==0);
        vga->field_page=fd2_field_page_create();CHECK(vga->field_page);fd2_input_init(&vga->input);
        CHECK(fd2_field_game_bind_visual(game,vga)==0);
        const uint8_t *palette;size_t size;CHECK(fd2_archive_get(other,0,&palette,&size)==0 && size>=768);
        fd2_vga_set_palette(vga,palette);s5_reset_io(vga);
        for(size_t i=0;i<game->units.count;i++)game->units.items[i].direction=3;
        game->units.items[0].hp=game->units.items[1].hp=0;
        fd2_field_unit roster[32];memcpy(roster,game->roster,sizeof(roster));
        s5_failure_trace trace={0,0,0,0,host};
        game->event_preflight=s5_failure_story;game->event_preflight_userdata=&trace;
        game->runtime_present=s5_failure_runtime;game->runtime_present_userdata=&trace;
        game->attack_rng=s5_failure_rng;game->attack_rng_userdata=&trace;
        CHECK(s5_fd2_scene_play_stage_source_flow(vga,other,game,NULL,1,"stage.source.victory.028")==
            (host?FD2_SCENE_RESULT_HOST_QUIT:FD2_SCENE_RESULT_ERROR));
        CHECK(trace.rotations==2 && trace.closed==1 && trace.stories==1 && !trace.rng);
        CHECK(game->stage==28 && game->units.count==64 && game->roster_count==20 &&
            !memcmp(roster,game->roster,sizeof(roster)) && game->runtime_host_quit==(unsigned)host);
        for(size_t i=0;i<game->units.count;i++){
            /* 只到第二次真实ROTATE_ACTOR写点；后续方向/hidden/ID/招募均未到达。 */
            CHECK(game->units.items[i].direction==(i<2?0:3) && game->units.items[i].flags==0);
            CHECK(game->units.items[i].hp==((i<2 || i>=20)?0:100));
        }
        CHECK(game->units.items[20].unit_id==0 && game->units.items[20].text_id==20);
        fd2_field_game_close(game);fd2_field_page_destroy(vga->field_page);free(vga);free(game);
    }
    return 0;
}
/* 完整source终章实际walker；只在原terminal无限叶到达时由测试注入HOST，
 * 不替换ending/credits/montage子图。ANI解码/宿主时钟仍沿本TU显式I/O替身。
 * 原caller对热26/30与resource30的合同另有完整Domain；不是自然App通关门。 */
static int s5_source_resource_preflight(const fd2_field_game *game,
        const fd2_static_flow_node *node,const fd2_field_event_resource_view *view,void *userdata) {
    if(!strcmp(node->op,"delay_ms"))s5_wait_preflight_calls++;
    return s5_real_resource_preflight(game,node,view,userdata);
}
static int s5_bind_real_preflight(fd2_field_game *game,const fd2_archive *other) {
    CHECK(fd2_field_play_bind_event_preflight(game,other)==0 && game->event_preflight);
    s5_real_resource_preflight=game->event_preflight;
    game->event_preflight=s5_source_resource_preflight;return 0;
}
/* 35022先story/pan/appendg3才到script43首未构造身份；244b6先
 * 233c6纯写raw72并完成script66前46组。只签这些source的producer合同，
 * 并不把任意count外字段解释为通用U；完整96×80原CPU比较另留独立收据。 */
static int s5_source_movement_prefix(const fd2_archive *other) {
    for(int host=0;host<2;host++) {
        fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));
        fd2_vga *vga=calloc(1,sizeof(*vga));CHECK(game && before && vga && s5_prepare(game,other,11)==0);
        game->units.count=25;memset(game->units.items+25,0xa5,71*80);
        CHECK(fd2_field_units_sprite_cache_restore(&game->units)==0);
        vga->field_page=fd2_field_page_create();CHECK(vga->field_page);fd2_input_init(&vga->input);
        CHECK(fd2_field_game_bind_visual(game,vga)==0);
        s5_reset_io(vga);CHECK(s5_bind_real_preflight(game,other)==0);
        CHECK(fd2_field_game_common_event_begin(game,"common.callback.037",15)==0);
        const char *ops[]={"story","pan","move"};
        for(unsigned i=0;i<3;i++) {
            fd2_static_flow_node node;size_t actor;
            CHECK(fd2_field_game_common_event_peek(game,&node,&actor)==1 && !strcmp(node.op,ops[i]));
            if(i<2) {
                CHECK(s5_fd2_scene_play_common_step(vga,other,game,&node,NULL,1)==0);
                CHECK(fd2_field_game_common_event_reply(game,i?FD2_FLOW_NEXT:FD2_FLOW_ACK)==0);
            }else {
                CHECK(node.args[0]==43 && game->units.count==31 &&
                    game->camera_cell_x==15 && game->camera_cell_y==34);
                *before=*game;if(host)vga->input.quit_requested=1;
                CHECK(s5_fd2_scene_play_common_step(vga,other,game,&node,NULL,1)==
                    (host?FD2_SCENE_RESULT_HOST_QUIT:FD2_SCENE_RESULT_ERROR));
                CHECK(!memcmp(game,before,sizeof(*game)));
                if(host)game->runtime_host_quit=1;
                else CHECK(fd2_field_game_common_event_reply(game,FD2_FLOW_ERROR)==-1);
                *before=*game;
                CHECK(fd2_field_game_common_event_reply(game,FD2_FLOW_NEXT)==-1 &&
                    !memcmp(game,before,sizeof(*game)) && game->units.count==31);
            }
        }
        fd2_field_game_close(game);fd2_field_page_destroy(vga->field_page);free(game);free(before);free(vga);
    }
    for(size_t count=66;count<=72;count+=6) {
        fd2_field_game *game=calloc(1,sizeof(*game));fd2_vga *vga=calloc(1,sizeof(*vga));
        CHECK(game && vga && s5_prepare(game,other,21)==0);fd2_field_units_clear(&game->units);
        /* FDFIELD[63]为45×50。仅16个部署槽用受控人物；NPC须取原g0/g1
         * placement，不能把后50条排到y=50..59再借12ac6越map owner读底。
         * 此前F1的66/66g1原CPU输入也采用这两组来源，坐标expected不变。 */
        CHECK(game->map.width==45 && game->map.height==50);
        for(size_t i=0;i<16;i++) {
            CHECK(fd2_field_units_set(&game->units,i,i==1?24:0,i==1?24:(uint8_t)i,2,
                20+(int)(i%4),42+(int)(i/4))==0);
            game->units.items[i].hp=game->units.items[i].hp_max=100;
        }
        CHECK(fd2_field_units_append_group(&game->units,&game->metadata,&game->placements,0)==0 &&
              game->units.count==66);
        if(count==72)CHECK(fd2_field_units_append_group(&game->units,&game->metadata,&game->placements,1)==0);
        CHECK(game->units.count==count);
        memset(game->units.items+count,0xa5,(96-count)*80);
        fd2_field_unit raw72=game->units.items[72];raw72.x=22;raw72.y=25;raw72.direction=2;
        fd2_field_unit tail[23];memcpy(tail,game->units.items+73,sizeof(tail));
        vga->field_page=fd2_field_page_create();CHECK(vga->field_page);fd2_input_init(&vga->input);
        CHECK(fd2_field_game_bind_visual(game,vga)==0);
        s5_reset_io(vga);CHECK(s5_bind_real_preflight(game,other)==0);
        game->battle_result=FD2_FIELD_BATTLE_VICTORY;
        CHECK(s5_fd2_scene_play_stage_source_flow(vga,other,game,NULL,1,"stage.source.victory.021")==-1);
        const uint8_t xy[][2]={{22,19},{22,18},{22,17},{23,17},{21,17},{21,18},{21,19},{23,18},
            {23,19},{20,18},{20,19},{20,20},{24,18},{24,19},{21,20},{23,25}};
        for(size_t i=0;i<16;i++) {
            if(game->units.items[i].x!=xy[i][0] || game->units.items[i].y!=xy[i][1])
                fprintf(stderr,"source66 count=%zu raw=%zu xy=%u,%u expected=%u,%u last=%s/%s camera=%d,%d render=%d host=%d quit=%d presents=%u clock=%llu\n",
                    count,i,game->units.items[i].x,game->units.items[i].y,xy[i][0],xy[i][1],
                    s5_predicate_last_id?s5_predicate_last_id:"-",s5_predicate_last_op?s5_predicate_last_op:"-",
                    game->camera_cell_x,game->camera_cell_y,scene_render_error,scene_host_quit_requested,
                    vga->input.quit_requested,s5_presents,(unsigned long long)s5_test_ms);
            CHECK(game->units.items[i].x==xy[i][0] && game->units.items[i].y==xy[i][1]);
        }
        CHECK(game->units.count==count && game->stage==21 && game->failure==FD2_FIELD_FAILURE_NONE &&
            !game->runtime_host_quit && game->camera_cell_x==16 && game->camera_cell_y==16 &&
            !memcmp(game->units.items+72,&raw72,80) && !memcmp(game->units.items+73,tail,sizeof(tail)));
        fd2_field_game_close(game);fd2_field_page_destroy(vga->field_page);free(game);free(vga);
    }
    return 0;
}
static int s5_source_wait_domain(const fd2_archive *other) {
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));
    fd2_vga *vga=calloc(1,sizeof(*vga));
    CHECK(game && before && vga && s5_prepare(game,other,29)==0);
    vga->field_page=fd2_field_page_create();CHECK(vga->field_page);fd2_input_init(&vga->input);
    CHECK(fd2_field_game_bind_visual(game,vga)==0);
    s5_reset_io(vga);CHECK(s5_bind_real_preflight(game,other)==0);
    *before=*game;
    CHECK(fd2_field_play_bind_event_preflight(NULL,other)==-1 &&
        fd2_field_play_bind_event_preflight(game,NULL)==-1 && !memcmp(game,before,sizeof(*game)));
    fd2_static_flow_node common=s5_cin_node("old.wait","delay_ms");
    fd2_field_event_resource_view view={&game->units,game->roster,game->roster_count,-1,-1,0,NULL};
    common.args[0]=2000;CHECK(game->event_preflight(game,&common,&view,game->event_preflight_userdata)==-1);
    common.args[0]=1000;CHECK(game->event_preflight(game,&common,&view,game->event_preflight_userdata)==0);
    CHECK(!memcmp(game,before,sizeof(*game)));
    fd2_static_flow_node nodes[2]={s5_cin_node("wait","delay"),s5_cin_node("done","end")};
    nodes[0].edges[FD2_FLOW_NEXT]=1;
    fd2_static_flow flow={"stage.source.victory.029",nodes,2,0};
    const int32_t values[]={0,2000,65535,-1,65536};
    for(size_t i=0;i<sizeof(values)/sizeof(values[0]);i++) {
        fd2_input_init(&vga->input);s5_reset_io(vga);CHECK(s5_bind_real_preflight(game,other)==0);
        s5_flow_override=&flow;nodes[0].args[0]=values[i];*before=*game;
        int result=s5_fd2_scene_play_stage_source_flow(vga,other,game,NULL,0,flow.id);
        CHECK(result==(i<3?FD2_SCENE_RESULT_OK:FD2_SCENE_RESULT_ERROR));
        CHECK(!s5_wait_preflight_calls && s5_delays==(i<3?1u:0u));
        CHECK(!memcmp(game,before,sizeof(*game)));
    }
    fd2_input_init(&vga->input);s5_reset_io(vga);CHECK(s5_bind_real_preflight(game,other)==0);
    s5_flow_override=&flow;nodes[0].args[0]=2000;s5_quit_on_delay=1;*before=*game;
    CHECK(s5_fd2_scene_play_stage_source_flow(vga,other,game,NULL,0,flow.id)==FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(s5_delays==1 && s5_test_ms==3000 && !s5_wait_preflight_calls && !memcmp(game,before,sizeof(*game)));
    fd2_field_game_close(game);fd2_field_page_destroy(vga->field_page);free(game);free(before);free(vga);
    return 0;
}
static int s5_full_ending(const fd2_archive *other,unsigned stage,int real_slow){
    fd2_field_game *game=calloc(1,sizeof(*game));fd2_vga *vga=calloc(1,sizeof(*vga));
    CHECK(game && vga && s5_prepare(game,other,stage)==0);
    for(size_t i=20;i<31;i++)CHECK(fd2_field_unit_persistent_create(&game->roster[i],(uint8_t)i)==0);
    game->roster_count=31;
    fd2_field_unit before[32];memcpy(before,game->roster,sizeof(before));
    vga->field_page=fd2_field_page_create();CHECK(vga->field_page);fd2_input_init(&vga->input);
    CHECK(fd2_field_game_bind_visual(game,vga)==0);
    const uint8_t *palette,*final;size_t palette_size,final_size;
    CHECK(fd2_archive_get(other,0,&palette,&palette_size)==0 && palette_size==768 &&
        fd2_archive_get(other,59,&final,&final_size)==0);
    fd2_image expected={0};CHECK(fd2_image_decode_buf(&expected,final,final_size)==0 &&
        expected.width==320 && expected.height==200);
    fd2_vga_set_palette(vga,palette);s5_reset_io(vga);
    s5_trace trace={stage,0,0,0,{0}};game->attack_rng=s5_rng;game->attack_rng_userdata=&trace;
    char id[64];snprintf(id,sizeof(id),"stage.source.victory.%03u",stage);
    s5_terminal_watch=1;
    s5_ani_real=real_slow;
    CHECK(s5_bind_real_preflight(game,other)==0);
    CHECK(s5_fd2_scene_play_stage_source_flow(vga,other,game,NULL,!real_slow,id)==FD2_SCENE_RESULT_HOST_QUIT);
    s5_terminal_watch=0;
    CHECK(s5_terminal_seen==1 && s5_terminal_pumps==2 && s5_ending_enters==1 && s5_montage_opens==20);
    /* 完整31529只调用一次ANI2；ANI0属于244b6，另由真实decoder门覆盖。 */
    if(real_slow)CHECK(s5_ani_calls==1 && s5_ani_index==2);
    CHECK(!s5_wait_preflight_calls);
    CHECK(game->stage==(stage==26?26u:30u) && game->metadata.unknown_01==31 &&
        game->units.count==31 && game->roster_count==31 && game->turn_number==1);
    CHECK(!memcmp(vga->framebuffer,expected.pixels,64000) && !memcmp(vga->palette,palette,768));
    for(size_t i=0;i<768;i++)CHECK(vga->dac[i]==(uint8_t)((palette[i]<<2)|(palette[i]>>4)));
    CHECK(s5_text_entry_count==(stage==26?2u:3u) && s5_text_entries[0]==stage+1u &&
        s5_text_entries[s5_text_entry_count-1]==31);
    if(stage==29)CHECK(s5_text_entries[1]==31);
    for(size_t i=0;i<31;i++){
        before[i].flags=0;before[i].hp=before[i].hp_max;before[i].mp=before[i].mp_max;
    }
    CHECK(!memcmp(before,game->roster,sizeof(before)));
    fd2_save_manual_state prepared;memset(&prepared,0xa5,sizeof(prepared));
    fd2_save_manual_state untouched=prepared;
    if(stage==29){
        CHECK(fd2_field_game_export_preparation(game,&prepared)==-1 && !memcmp(&prepared,&untouched,sizeof(prepared)));
    }else{
        /* Game没有Scene终端游标；热26的纯复制API仍合法。正式Play只在
         * Scene OK时调用export，以上HOST出口由caller阻止后续准备交接。 */
        CHECK(fd2_field_game_export_preparation(game,&prepared)==0 && prepared.stage_id==26 &&
            prepared.unit_count==31 && !memcmp(prepared.unit_area,game->roster,sizeof(game->roster)));
    }
    fd2_image_free(&expected);fd2_field_game_close(game);fd2_field_page_destroy(vga->field_page);free(vga);free(game);
    return 0;
}

/* F3参数补证：独立expected来自S2 transitions-candidate18的原caller
 * vectors93..101/128..135/148..151及33499原指令14例；不读冷图生成期望。
 * 232e8/23a0a/23e74、entry14/victory14仅签受控参数与真实Scene caller，
 * 不是自然招募/死亡/回合producer或新原CPU运行。 */
typedef struct {unsigned count,rng,stop;int host;fd2_vga *vga;int stories[8];} s5_predicate_trace;
static uint32_t s5_predicate_rng(void *data){((s5_predicate_trace *)data)->rng++;return 7;}
static int s5_predicate_story(const fd2_field_game *game,const fd2_static_flow_node *node,
        const fd2_field_event_resource_view *view,void *data){
    (void)game;(void)view;s5_predicate_trace *t=data;
    if(!strcmp(node->op,"story")){
        if(t->count>=8)return -1;t->stories[t->count++]=node->args[0];
        if(t->stop && t->count==t->stop){
            if(t->host)t->vga->input.quit_requested=1;
            else return -1;
        }
    }
    return 0;
}
static int s5_predicate_stage_case(const fd2_archive *other,unsigned stage,int entry,
        unsigned state,unsigned hidden,int turn,unsigned hpmax,int recruited,unsigned stop,int host){
    fd2_field_game *g=calloc(1,sizeof(*g));fd2_vga *vga=calloc(1,sizeof(*vga));
    CHECK(g && vga && s5_prepare(g,other,stage)==0);
    vga->field_page=fd2_field_page_create();CHECK(vga->field_page);fd2_input_init(&vga->input);
    CHECK(fd2_field_game_bind_visual(g,vga)==0);s5_reset_io(vga);
    if(stage==15)g->roster_count=16;
    if(stage==14){
        /* 完整合法persistent记录移除12；正臂将原12放最后槽，故不在
         * 16个deployment内。HP0/hidden仍须被33499看见，不造text70。 */
        fd2_field_unit twelve=g->roster[12];
        memmove(g->roster+12,g->roster+13,7*sizeof(*g->roster));g->roster_count=19;
        if(recruited){twelve.hp=0;twelve.flags|=1;g->roster[g->roster_count++]=twelve;}
    }
    if(!entry){
        /* 真实entry产生身份/placement：19为16+67=83，14为16+58，
         * 15为16+60；不能用64人通用页代正常victory的raw producer。
         * 19早臂追加group1到raw83，原script60@62fe8才有合法身份。 */
        CHECK(s5_fd2_scene_play_stage_entry(vga,other,g,NULL,1)==FD2_SCENE_RESULT_OK);
        if(stage==19)CHECK(g->units.count==83);
        if(stage==14)CHECK(g->units.count==74);
        if(stage==15)CHECK(g->units.count==76);
        if(stage==6){
            /* 原entry6为9部署+group1的25条；正式turn10/action25在
             * state16=1时追加group2十条，才产生victory读取的raw43。
             * 这里只控制该callback前提并执行真实Game/Scene，不签自然lookup链。 */
            CHECK(g->units.count==34);g->cell_action_completed[16]=1;
            CHECK(fd2_field_game_common_event_begin(g,"stage.006.action.025",0)==0);
            for(unsigned i=0;g->event_resume.active;i++){
                fd2_static_flow_node node;size_t actor;CHECK(i<8 && fd2_field_game_common_event_peek(g,&node,&actor)==1);
                CHECK(s5_fd2_scene_play_common_step(vga,other,g,&node,NULL,1)==FD2_SCENE_RESULT_OK);
                CHECK(fd2_field_game_common_event_reply(g,!strcmp(node.op,"story")?FD2_FLOW_ACK:FD2_FLOW_NEXT)==0);
            }
            CHECK(g->units.count==44 && g->cell_action_completed[17]==1);
        }
        s5_reset_io(vga);
    }
    g->turn_number=(uint32_t)turn;
    if(stage==6){g->cell_action_completed[17]=(uint8_t)state;g->units.items[43].flags=(uint8_t)hidden;}
    if(stage==15){
        g->units.items[0].hp_max=(uint16_t)hpmax;
        for(unsigned i=0;i<8;i++)g->units.items[66+i].flags=(uint8_t)(i<hidden);
    }
    size_t count=g->units.count,roster=g->roster_count;
    s5_predicate_trace t={0,0,stop,host,vga,{0}};
    g->event_preflight=s5_predicate_story;g->event_preflight_userdata=&t;
    g->attack_rng=s5_predicate_rng;g->attack_rng_userdata=&t;
    char id[64];snprintf(id,sizeof(id),"stage.source.%s.%03u",entry?"entry":"victory",stage);
    int result=s5_fd2_scene_play_stage_source_flow(vga,other,g,NULL,1,id);
    int want[8]={0};unsigned n=0,added=0;
    if(stage==6){want[n++]=(state==1 && !hidden)?4:5;added=(state==1 && !hidden);}
    if(stage==15){
        if(hidden<=4 && turn<=18 && hpmax>=320){want[n++]=4;added=1;}
        else {want[n++]=2;want[n++]=3;}
    }
    if(stage==19){want[n++]=11;want[n++]=12;added=1;
        if(turn<=15){want[n++]=14;want[n++]=15;want[n++]=16;added++;}want[n++]=13;}
    if(stage==14){if(entry){want[n++]=recruited?0:3;want[n++]=recruited?1:4;want[n++]=recruited?2:5;}
        else {want[n++]=recruited?12:13;added=1;}}
    if(stop){n=stop;added=0;}
    if(result!=(stop?(host?FD2_SCENE_RESULT_HOST_QUIT:FD2_SCENE_RESULT_ERROR):FD2_SCENE_RESULT_OK) ||
            t.count!=n || memcmp(want,t.stories,n*sizeof(int)))
        fprintf(stderr,"predicate stage=%u entry=%d state=%u hidden=%u turn=%d hpmax=%u recruit=%d stop=%u host=%d result=%d stories=%u [%d,%d,%d,%d,%d,%d] active=%zu roster=%zu failure=%d last=%s/%s\n",
            stage,entry,state,hidden,turn,hpmax,recruited,stop,host,result,t.count,
            t.stories[0],t.stories[1],t.stories[2],t.stories[3],t.stories[4],t.stories[5],
            g->units.count,g->roster_count,g->failure,s5_predicate_last_id?s5_predicate_last_id:"-",s5_predicate_last_op?s5_predicate_last_op:"-");
    CHECK(result==(stop?(host?FD2_SCENE_RESULT_HOST_QUIT:FD2_SCENE_RESULT_ERROR):FD2_SCENE_RESULT_OK));
    CHECK(t.count==n && !memcmp(want,t.stories,n*sizeof(int)) && !t.rng);
    CHECK(g->roster_count==roster+added);
    if(!entry)CHECK(g->units.count>=count && g->stage==(stop?stage:stage+1));
    if(stage==15 && added)CHECK(g->roster[roster].text_id==18);
    if(stage==19 && !stop){CHECK(g->roster[roster].text_id==25);if(added==2)CHECK(g->roster[roster+1].text_id==28);}
    if(stage==6 && added)CHECK(g->roster[roster].text_id==12);
    if(stage==14 && !entry && !stop)CHECK(g->roster[roster].text_id==15);
    fd2_field_game_close(g);fd2_field_page_destroy(vga->field_page);free(vga);free(g);return 0;
}
static int s5_predicate_stage_parameters(const fd2_archive *other){
    for(unsigned state=0;state<3;state++)for(unsigned hidden=0;hidden<2;hidden++)
        CHECK(s5_predicate_stage_case(other,6,0,state,hidden,1,100,0,0,0)==0);
    for(unsigned hidden=4;hidden<=5;hidden++)for(int turn=18;turn<=19;turn++)for(unsigned hp=319;hp<=320;hp++)
        CHECK(s5_predicate_stage_case(other,15,0,0,hidden,turn,hp,0,0,0)==0);
    CHECK(s5_predicate_stage_case(other,15,0,0,4,-1,320,0,0,0)==0);
    CHECK(s5_predicate_stage_case(other,15,0,0,4,18,321,0,0,0)==0);
    const int turns[]={15,16,-1};for(unsigned i=0;i<3;i++)
        CHECK(s5_predicate_stage_case(other,19,0,0,0,turns[i],100,0,0,0)==0);
    for(int entry=0;entry<2;entry++)for(int recruited=0;recruited<2;recruited++)
        CHECK(s5_predicate_stage_case(other,14,entry,0,0,1,100,recruited,0,0)==0);
    for(int host=0;host<2;host++)CHECK(s5_predicate_stage_case(other,15,0,0,4,18,320,0,1,host)==0);
    return 0;
}

/* field_event_actor17_exchange_item11_once @0x35730（原sub_35730）：
 * G4完整173B+3531b尾，caller0/state0→story0→真实exchange/finalize→
 * hidden17（不是side）→state0/tiles/refresh/child item11→contribution0。
 * field_event_key208_recruit31 @0x35a0d（原sub_35A0D）：完整363B，
 * remove208先于message3/ACK，动画后才state12/group1/roster31/story4。
 * 此组只用静态原源expected；Game业务真实，呈现/动画显式ACK，非新原CPU。 */
static int s5_predicate_common_case(const fd2_archive *other,int key,unsigned state,int positive,int stop){
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g && s5_prepare(g,other,key?25:24)==0);
    s5_predicate_trace t={0};g->attack_rng=s5_predicate_rng;g->attack_rng_userdata=&t;
    unsigned initial_roster=(unsigned)g->roster_count,initial_count=(unsigned)g->units.count;
    g->experience_value=777;g->cell_action_completed[key?12:0]=(uint8_t)state;
    if(key && positive)s5_item(&g->units.items[0],0,208);
    if(!key){
        fd2_field_unit *a=&g->units.items[17],*b=&g->units.items[0];
        /* 通用sprite夹具的movement_profile=0未经过原base构造，不能进
         * critical/profile读取。用同一正式base_apply派生合法profile后，
         * 只控制本例的HP/攻击/装备与几何，不猜一个profile常数。 */
        CHECK(fd2_field_unit_base_apply(a,1)==0 && fd2_field_unit_base_apply(b,1)==0);
        a->side=0;a->x=3;a->y=3;a->hp=a->hp_max=(uint16_t)(positive?1:1000);a->attack=1;a->defense=100;
        b->x=4;b->y=3;b->attack=(uint16_t)(positive?500:1);b->defense=500;
        a->accuracy=b->accuracy=100;a->evasion=b->evasion=0;
        ((uint8_t*)a)[10]=((uint8_t*)b)[10]=0x40;((uint8_t*)a)[11]=((uint8_t*)b)[11]=0;
    }
    CHECK(fd2_field_game_common_event_begin(g,key?"common.key.consume":"common.loot.physical",0)==0);
    unsigned messages=0,stories=0,animations=0,child=0,steps=0;int fragment=-1;
    for(;steps<128 && g->event_resume.active;steps++){
        if(g->loot.child.active){
            fd2_field_loot_notice notice;CHECK(fd2_field_game_loot_peek(g,&notice)==1);
            CHECK(notice.kind==FD2_FIELD_LOOT_MESSAGE && notice.item_id==11);child++;
            CHECK(g->cell_action_completed[0]==1);
            CHECK(fd2_field_game_loot_text_ready(g)==0);
            CHECK(fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_ACK,-1)==0);continue;
        }
        fd2_static_flow_node node;size_t actor;CHECK(fd2_field_game_common_event_peek(g,&node,&actor)==1 && actor==0);
        if(!strcmp(node.op,"message")){messages++;fragment=node.args[0];
            if(key && positive){CHECK(fragment==3 && !fd2_field_unit_inventory_count(&g->units.items[0]) && !g->cell_action_completed[12]);
                if(stop){fd2_field_units before=g->units;unsigned rng=t.rng;
                    if(stop==2)g->runtime_host_quit=1;
                    CHECK(fd2_field_game_common_event_reply(g,stop==2?FD2_FLOW_NEXT:FD2_FLOW_ERROR)==-1);
                    CHECK(!memcmp(&before,&g->units,sizeof(before)) && !g->cell_action_completed[12] && t.rng==rng &&
                        g->roster_count==initial_roster);break;}
            }
        }
        if(!strcmp(node.op,"story")){stories++;fragment=node.args[0];
            if(key)CHECK(fragment==4 && g->cell_action_completed[12]==1 && g->roster_count==initial_roster+1);
            else {CHECK(fragment==0 && !g->cell_action_completed[0] && !t.rng);
                if(stop){if(stop==2)g->runtime_host_quit=1;
                    CHECK(fd2_field_game_common_event_reply(g,stop==2?FD2_FLOW_ACK:FD2_FLOW_ERROR)==-1);
                    CHECK(!t.rng && !g->cell_action_completed[0] && g->units.items[17].hp==(positive?1:1000));break;}
            }
        }
        if(!strcmp(node.op,"raw_animation")){animations++;CHECK(key && !g->cell_action_completed[12] && g->roster_count==initial_roster);}
        unsigned port=(!strcmp(node.op,"story") || !strcmp(node.op,"wait_key"))?FD2_FLOW_ACK:FD2_FLOW_NEXT;
        int reply_result=fd2_field_game_common_event_reply(g,port);
        if(reply_result)fprintf(stderr,"common predicate key=%d state=%u positive=%d stop=%d op=%s port=%u result=%d failure=%d host=%u child=%u active=%u pc=%d hp0=%u hp17=%u profile0=%u profile17=%u rng=%u\n",
            key,state,positive,stop,node.op,port,reply_result,g->failure,g->runtime_host_quit,g->loot.child.active,
            g->event_resume.active,g->event_resume.pc,g->units.items[0].hp,g->units.items[17].hp,
            g->units.items[0].movement_profile,g->units.items[17].movement_profile,t.rng);
        CHECK(reply_result==0);
    }
    CHECK(steps<128);
    if(!stop){
        CHECK(!g->event_resume.active && !g->failure && !g->runtime_host_quit);
        if(state){CHECK(!messages && !stories && !animations && !child && !t.rng &&
            g->cell_action_completed[key?12:0]==state && g->roster_count==initial_roster && g->units.count==initial_count);}
        else if(key){CHECK(messages==1 && fragment==(positive?4:2) && stories==(unsigned)positive &&
            animations==(unsigned)positive && !t.rng && g->cell_action_completed[12]==(unsigned)positive &&
            g->roster_count==initial_roster+(unsigned)positive);
            if(positive)CHECK(g->roster[initial_roster].text_id==31 && g->units.count>initial_count);
        }else {CHECK(stories==1 && g->cell_action_completed[0]==(unsigned)positive &&
            fd2_field_unit_is_hidden(&g->units.items[17])==positive && g->experience_value==0);
            /* slot0的实际weapon0也属于原八slot库存；child另增item11。 */
            CHECK(child==(unsigned)positive && fd2_field_unit_inventory_count(&g->units.items[0])==1u+(unsigned)positive);}
    }
    fd2_field_game_close(g);free(g);return 0;
}
static int s5_predicate_common_parameters(const fd2_archive *other){
    for(int key=0;key<2;key++)for(unsigned state=0;state<2;state++)for(int positive=0;positive<2;positive++)
        CHECK(s5_predicate_common_case(other,key,state,positive,0)==0);
    for(int key=0;key<2;key++)for(int stop=1;stop<=2;stop++)
        CHECK(s5_predicate_common_case(other,key,0,1,stop)==0);
    return 0;
}
/* 原31529独立保留的单pair首尾；只切入口和close之后的唯一边。
 * 中间真实ending节点、两半RNG分支及共享montage_round均由正式walker消费。
 * 这不是整个ending/credits；不以C/cold生成任何expected。 */
static int f4_cold_pair_view(const fd2_static_flow *source,fd2_static_flow *view,
        fd2_static_flow_node **owned,unsigned first,unsigned second,unsigned scene,
        int *open_index,int *close_index) {
    if(!source || !view || !owned || *owned || source->count>10000u)return -1;
    int found=-1,end=-1;unsigned opens=0,closes=0,ends=0;
    for(size_t i=0;i<source->count;i++) {
        const fd2_static_flow_node *n=&source->nodes[i];
        if(!strcmp(n->op,"cin_montage_open")) {
            opens++;
            if(n->args[0]==(int32_t)first && n->args[1]==(int32_t)second && n->args[2]==(int32_t)scene) {
                if(found>=0)return -1;found=(int)i;
            }
        }
        if(!strcmp(n->op,"cin_montage_close"))closes++;
        if(!strcmp(n->op,"end")){ends++;end=(int)i;}
    }
    if(opens!=20u || closes!=20u || ends!=1u || found<0)return -1;
    uint8_t *seen=calloc(source->count,1);int *queue=malloc(source->count*sizeof(*queue));
    if(!seen || !queue){free(seen);free(queue);return -1;}
    size_t read=0,count=1;queue[0]=found;seen[found]=1;int close=-1,failed=0;
    while(read<count && !failed) {
        int at=queue[read++];const fd2_static_flow_node *n=&source->nodes[at];
        if(!strcmp(n->op,"cin_montage_close")) {
            if(close>=0 && close!=at){failed=1;break;}close=at;continue;
        }
        if(at!=found && !strcmp(n->op,"cin_montage_open")){failed=1;break;}
        for(unsigned port=0;port<FD2_FLOW_PORT_COUNT;port++) {
            if(port==FD2_FLOW_ERROR)continue;
            int next=n->edges[port];if(next<0)continue;
            if((size_t)next>=source->count){failed=1;break;}
            if(!seen[next]){seen[next]=1;queue[count++]=next;}
        }
    }
    free(seen);free(queue);if(failed || close<0)return -1;
    fd2_static_flow_node *copy=malloc(source->count*sizeof(*copy));if(!copy)return -1;
    memcpy(copy,source->nodes,source->count*sizeof(*copy));copy[close].edges[FD2_FLOW_NEXT]=end;
    /* 除声明的单一close-next边外，所有node/args/ports逐byte保真。 */
    for(size_t i=0;i<source->count;i++) {
        fd2_static_flow_node check;memcpy(&check,&copy[i],sizeof(check));
        if((int)i==close)check.edges[FD2_FLOW_NEXT]=source->nodes[i].edges[FD2_FLOW_NEXT];
        if(memcmp(&check,&source->nodes[i],sizeof(check))){free(copy);return -1;}
    }
    *view=*source;view->entry=found;view->nodes=copy;*owned=copy;
    *open_index=found;*close_index=close;return 0;
}
static int f4_montage_cold_cli(fd2_archive *other) {
    char line[128],extra;unsigned first,second,scene,random;
    if(!fgets(line,sizeof(line),stdin) || sscanf(line,"%u %u %u %u %c",&first,&second,&scene,&random,&extra)!=4 ||
        first>=128u || second>=128u || scene<1u || scene>63u || (random!=0u && random!=50u) ||
        fgets(line,sizeof(line),stdin) || ferror(stdin))return 1;
    fd2_field_game *game=calloc(1,sizeof(*game));fd2_vga *vga=calloc(1,sizeof(*vga));
    fd2_scene_field_state *state=calloc(1,sizeof(*state));scene_flow_context context={0};
    fd2_static_flow view={0};fd2_static_flow_node *owned=NULL;int open=-1,close=-1,result=-1;
    const fd2_static_flow *source=fd2_static_flow_lookup("stage","stage.cinematic.ending");
    if(!game || !vga || !state || f4_cold_pair_view(source,&view,&owned,first,second,scene,&open,&close))goto done;
    if(fd2_archive_open(&game->figani_archive,"original_game/FIGANI.DAT"))goto done;
    game->ready=1;game->stage=26;game->units.count=2;
    game->units.items[0].unit_id=(uint8_t)first;game->units.items[0].side=first<76u?2:0;
    game->units.items[1].unit_id=(uint8_t)second;game->units.items[1].side=second<76u?2:0;
    game->attack_rng=f4_cold_random;game->attack_rng_userdata=NULL;
    const uint8_t *palette;size_t palette_size;
    if(fd2_archive_get(other,0,&palette,&palette_size) || palette_size!=768)goto done;
    fd2_vga_set_palette(vga,palette); /* 继承原2779夹具的palette0输入，不猜初始DAC。 */
    context.vga=vga;context.fdother=other;context.game=context.mutable_game=game;
    context.state=state;context.source_flow=1;context.fast=0;
    s5_reset_io(vga);scene_source_load(&context);fd2_bios_clock_reset(0,0);
    f4_cold_game=game;f4_cold_vga=vga;f4_cold_rng_value=random;
    f4_cold_rows=f4_cold_rng_calls=f4_cold_wait_nodes=f4_cold_wait_total=f4_cold_clock_reads=0;
    f4_cold_clock_tick=100;f4_cold_output_failed=0;f4_cold_cue_count=0;f4_cold_enabled=1;
    result=scene_flow_run(&context,&view);
    f4_cold_enabled=0;
    printf("{\"kind\":\"summary\",\"status\":%d,\"rows\":%u,\"rng\":%u,\"wait_nodes\":%u,\"wait_total\":%u,\"clock_reads\":%u,\"uploads\":%u,\"presents\":%u,\"delay_calls\":%u,\"entry\":\"%s\",\"close\":\"%s\",\"flow_nodes\":%u,\"only_close_next_cut\":true,\"whole_ending_claimed\":false,\"cue_audit\":[",
        result,f4_cold_rows,f4_cold_rng_calls,f4_cold_wait_nodes,f4_cold_wait_total,f4_cold_clock_reads,
        s5_uploads,s5_presents,s5_delays,source->nodes[open].id,source->nodes[close].id,(unsigned)source->count);
    for(unsigned i=0;i<f4_cold_cue_count;i++) {
        const f4_cold_cue_call *call=&f4_cold_cues[i];
        printf("%s{\"event_before\":%u,\"half\":%u,\"part\":%u,\"entry\":%u,\"cue\":%u,\"flow\":\"%s\",\"node\":\"%s\",\"rng_before\":%u,\"rng_after\":%u,\"clock_before\":%u,\"clock_after\":%u,\"hp_before\":[%u,%u],\"hp_after\":[%u,%u]}",
            i?",":"",call->event_before,call->half,call->part,call->entry,call->cue,call->flow,call->node,
            call->rng_before,call->rng_after,call->clock_before,call->clock_after,
            (unsigned)call->hp_before[0],(unsigned)call->hp_before[1],(unsigned)call->hp_after[0],(unsigned)call->hp_after[1]);
    }
    puts("]}");
    if(f4_cold_output_failed || ferror(stdout))result=-1;
done:
    f4_cold_enabled=0;fd2_scene_cinematic_close(&context.cinematic);
    if(game)fd2_archive_close(&game->figani_archive);
    free(owned);free(state);free(vga);free(game);return result?1:0;
}

#include "f4_credits_cold_probe.h"
static uint64_t f4_credits_clock(void) {
    f4_credits_clock_reads++;return fd2_bios_tick_start_ns(f4_credits_clock_tick++);
}
int main(int argc,char **argv){
    fd2_archive other={0};CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    if(argc==2 && !strcmp(argv[1],"--f4-credits-cold")) {int result=f4_credits_cold_cli(&other);fd2_archive_close(&other);return result;}
    if(argc==2 && !strcmp(argv[1],"--f4-montage-cold")) {int result=f4_montage_cold_cli(&other);fd2_archive_close(&other);return result;}
    if(argc==2 && !strcmp(argv[1],"--s5-stage-vectors")){unsigned stage,variant;int turn;
        while(scanf("%u %u %d",&stage,&variant,&turn)==3)CHECK(s5_vector(&other,stage,variant,turn,1)==0);
    }else{
        CHECK(s5_predicate_stage_parameters(&other)==0);
        CHECK(s5_predicate_common_parameters(&other)==0);
        CHECK(s5_cinematic_checks()==0);
        CHECK(s5_text_owner_case(&other,22)==0 && s5_text_owner_case(&other,29)==0);
        CHECK(s5_game_leaf_failure_prefix(&other)==0);
        CHECK(s5_source_wait_domain(&other)==0);
        CHECK(s5_source_movement_prefix(&other)==0);
        CHECK(s5_full_ending(&other,26,0)==0 && s5_full_ending(&other,29,0)==0);
        CHECK(s5_full_ending(&other,29,1)==0);
        CHECK(s5_vector(&other,20,6,1,0)==0 && s5_vector(&other,20,9,1,0)==0);
        CHECK(s5_vector(&other,22,0,14,0)==0 && s5_vector(&other,22,0,15,0)==0);
        CHECK(s5_vector(&other,26,1,1,0)==0 && s5_vector(&other,26,3,1,0)==0 &&
            s5_vector(&other,28,0,1,0)==0 && s5_vector(&other,29,0,1,0)==0);
        puts("S5 Scene source branch/prefix and fixed cinematic consumer gates PASS (no GUI)");
    }
    fd2_archive_close(&other);return 0;
}

/* 正式视觉接口使用NS；本测试仍沿原毫秒调度器。 */
uint64_t s5_clock_ns(void) {
    if(f4_credits_enabled)return f4_credits_clock();
    if(f4_cold_enabled){f4_cold_clock_reads++;return fd2_bios_tick_start_ns(f4_cold_clock_tick++);}
    return s5_clock()*UINT64_C(1000000);
}
