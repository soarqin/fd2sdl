/* boot_intro_title_menu @0x1f894：标题必需资源错误出口，无窗口私有TU。
 * 所有后续资源与菜单替身均成功，避免错误被另一个资源失败掩盖。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "archive.h"
#include "app_flow.h"
#include "field.h"
#include "field_prepare_play.h"
#include "animation.h"
#include "image.h"
#include "input.h"
#include "title.h"
#include "vga.h"
#include "scene.h"
#include "bios_clock.h"
#include "bgm.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

/* C14有限stream仅观察真实boot caller；NULL/0时旧测试不变。 */
static int title_comp_active,title_comp_bad,title_comp_last_resource;
/* 本TU的同步SOURCE失败注入，非原版/设备/phase oracle。 */
static int f5_source_error,f5_wait_fault_call,f5_wait_calls;
static int f5_mutate_animation,f5_animation_source_fault,f5_bgm_fail,f5_bgm_calls;
static int f5_output_error,f5_audio_at,f5_audio_calls,f5_audio_fault,f5_audio_status,f5_bgm_output;
static int f5_upload_fault_at,f5_upload_host,f5_upload_queued;
static int title_test_output_error(const fd2_vga *vga){(void)vga;return f5_output_error;}
static int title_test_audio_return(void){
    if(++f5_audio_calls==f5_audio_at){
        if(f5_audio_fault&1)f5_output_error=-91;
        if(f5_audio_fault&2)f5_source_error=-66;
    }
    return f5_audio_status;
}
static int title_test_pcm_replace(fd2_pcm_player *p,const fd2_pcm_bank *b,size_t i,int n,float g){
    if(f5_audio_at)return title_test_audio_return();
    return fd2_pcm_play_replace(p,b,i,n,g);
}
static int title_test_pcm_stop(fd2_pcm_player *p){
    if(f5_audio_at)return title_test_audio_return();
    return fd2_pcm_stop(p);
}
static int title_test_source_error(void) { return f5_source_error; }
static int title_test_bgm_play(fd2_bgm_player *p,size_t track,int loops) {
    f5_bgm_calls++;
    if(f5_bgm_fail) { f5_source_error=-66;return -1; }
    if(f5_bgm_output){f5_output_error=-91;return 0;}
    return fd2_bgm_play(p,track,loops);
}
static int title_ani_active;
static int title_ani_decode(fd2_vga *,const fd2_archive *,int,uint32_t,int,fd2_animation_frame_hook,void *);
static int title_test_intro_animation(fd2_vga *,const fd2_archive *,int,uint32_t,int);
static void title_comp_upload(fd2_vga *vga);
static void title_comp_delay(uint32_t ms);
static void title_comp_wait_ticks(uint32_t ticks);
static void title_comp_before_fade(fd2_vga *,int,int,uint8_t,uint8_t,uint8_t);
static void title_comp_after_fade(int,int,uint8_t,uint8_t,uint8_t);
static void title_comp_fade_endpoint(fd2_vga *,int);
static void title_comp_ani_boundary(fd2_vga *,int);
static int title_comp_read_action(fd2_input_action *action);
static void title_comp_menu_draw(int count,int selected,int highlighted);
static int palette_fault, base_fault, test_animation_result, menu_quit;
static int palette_reads, title_reads, base_decodes, menu_decodes;
static int animation_calls, archive_opens, archive_closes, image_live;
static int menu_frames, action_reads, presents, unexpected;
static uint8_t test_palette_data[768], archive_token;
static int intro_enabled, intro_resource_fault, intro_image_fault;
static int intro_animation_fault, intro_animation_code, intro_skip_scroll;
static size_t last_resource;
static int intro_resource_reads[128], intro_image_decodes[128];
static int intro_animation_calls[9];
static int input_contract, input_clears, clear_had_pending[4], menu_queue_empty;
static int quit_on_clear, presents_at_clear[4];
static uint64_t input_time;
static fd2_vga *input_vga;
static int fade_calls, fade_fail_call, fade_fail_result, fade_directions[32];
static int white_calls, white_error;
static int menu_deferred, menu_idle_polls, menu_bad_delay, menu_flash_delays;
static int menu_selected, read_count, read_frames[4], read_selections[4];
static int title_test_quit(fd2_input *input);

static void title_queue_key(fd2_vga *vga,SDL_Scancode key) {
    fd2_input_set_test_key_state(&vga->input,key,1);
    fd2_input_begin_test_frame(&vga->input,++input_time);
    fd2_input_set_test_key_state(&vga->input,key,0);
    fd2_input_begin_test_frame(&vga->input,++input_time);
}
static void title_test_input_clear(fd2_input *input) {
    if(input_clears<4){clear_had_pending[input_clears]=fd2_input_has_any_key(input);presents_at_clear[input_clears]=presents;}
    input_clears++;fd2_input_clear_keys(input);
    if(input_clears==quit_on_clear)input->quit_requested=1;
}

static int title_test_archive_get(const fd2_archive *ar, size_t index,
                                  const uint8_t **data, size_t *length) {
    if(title_ani_active)return fd2_archive_get(ar,index,data,length);
    if(title_comp_active){title_comp_last_resource=(int)index;return fd2_archive_get(ar,index,data,length);}
    (void)ar;
    last_resource = index;
    if (index < 128) intro_resource_reads[index]++;
    if (index == 8) {
        palette_reads++;
        if (palette_fault == 1) return -1;
        *data = test_palette_data;
        *length = palette_fault == 2 ? 767 : sizeof(test_palette_data);
        return 0;
    }
    if (index == 7) {
        title_reads++;
        *data = &archive_token;
        *length = 1;
        return 0;
    }
    if (intro_enabled) {
        if (index == 0 || index == 0x4c || index == 99 ||
            index == 0x65 || index == 0x66) {
            palette_reads++;
            if ((int)index == intro_resource_fault) return -1;
            *data = test_palette_data;
            *length = sizeof(test_palette_data);
            return 0;
        }
        if ((index >= 0x45 && index <= 0x4b) || index == 100) {
            *data = &archive_token;
            *length = 1;
            return 0;
        }
    }
    unexpected++;
    return -1;
}

static int title_test_archive_open_mem(fd2_archive *ar,
                                       const uint8_t *data, size_t length) {
    if(title_ani_active)return fd2_archive_open_mem(ar,data,length);
    if(title_comp_active){int r=fd2_archive_open_mem(ar,data,length);if(!r)archive_opens++;return r;}
    if (data != &archive_token || length != 1) unexpected++;
    memset(ar, 0, sizeof(*ar));
    ar->count = 7;
    archive_opens++;
    return 0;
}

static void title_test_archive_close(fd2_archive *ar) {
    if(title_ani_active){fd2_archive_close(ar);return;}
    if(title_comp_active){fd2_archive_close(ar);archive_closes++;return;}
    memset(ar, 0, sizeof(*ar));
    archive_closes++;
}

static int title_test_image_decode_entry(fd2_image *image,
                                         const fd2_archive *ar, size_t index) {
    if(title_comp_active){int r=fd2_image_decode_entry(image,ar,index);if(!r)image_live++;return r;}
    (void)ar;
    memset(image, 0, sizeof(*image));
    if (index == 0) {
        base_decodes++;
        if (base_fault) return -1;
    } else if (index < 7) {
        menu_decodes++;
    } else {
        unexpected++;
        return -1;
    }
    image->pixels = malloc(1);
    if (!image->pixels) abort();
    image->width = image->height = 1;
    image->pixels[0] = 1;
    image_live++;
    return 0;
}

static int title_test_image_decode_buf(fd2_image *image,
                                       const uint8_t *data, size_t length) {
    if(title_comp_active){int r=fd2_image_decode_buf(image,data,length);if(!r)image_live++;return r;}
    memset(image, 0, sizeof(*image));
    if (data != &archive_token || length != 1 || last_resource >= 128) {
        unexpected++;
        return -1;
    }
    intro_image_decodes[last_resource]++;
    if ((int)last_resource == intro_image_fault) return -1;
    image->width = 320;
    image->height = last_resource >= 0x45 && last_resource <= 0x49 ? 147 : 200;
    image->pixels = malloc((size_t)image->width * image->height);
    if (!image->pixels) abort();
    memset(image->pixels, 1, (size_t)image->width * image->height);
    image_live++;
    return 0;
}

static void title_test_image_free(fd2_image *image) {
    if(title_comp_active){if(image->pixels)image_live--;fd2_image_free(image);return;}
    if (image->pixels) {
        free(image->pixels);
        image_live--;
    }
    memset(image, 0, sizeof(*image));
}

static int title_test_animation(fd2_vga *vga, const fd2_archive *ani,
                                 int index, uint32_t delay, int check_input,
                                 fd2_animation_frame_hook hook, void *userdata) {
    if(f5_mutate_animation) {
        vga->palette[0]=7;vga->dac[0]=28;vga->framebuffer[0]=9;
        f5_source_error=f5_animation_source_fault;
        if(f5_audio_at)title_test_audio_return();
        return test_animation_result;
    }
    if(title_ani_active)return title_ani_decode(vga,ani,index,delay,check_input,hook,userdata);
    if(index!=1){int result=title_test_intro_animation(vga,ani,index,delay,check_input);if(result>=0&&hook)hook(userdata,0);return result;}
    if(title_comp_active){if(index!=1||delay!=15u||!check_input)title_comp_bad=1;title_comp_ani_boundary(vga,index);if(hook)hook(userdata,0);return 0;}
    (void)vga;
    (void)ani;
    if (index != 1 || delay != FD2_TITLE_ANIM_DELAY_FAST_MS || !check_input)
        unexpected++;
    animation_calls++;
    if(input_contract)title_queue_key(vga,SDL_SCANCODE_RETURN);
    if (test_animation_result >= 0 && hook) hook(userdata, 0);
    return test_animation_result;
}

static int title_test_intro_animation(fd2_vga *vga, const fd2_archive *ani,
                                       int index, uint32_t delay, int check_input) {
    if(title_comp_active){uint32_t expected=index==0?15u:(index==5||index==7)?50u:90u;if((index!=0&&index!=3&&index!=4&&index!=5&&index!=6&&index!=7&&index!=8)||delay!=expected||check_input!=(index==3))title_comp_bad=1;title_comp_ani_boundary(vga,index);return 0;}
    (void)vga; (void)ani;
    if (index < 0 || index >= 9 || index == 1 || index == 2) {
        unexpected++;
        return -1;
    }
    uint32_t expected_delay = index == 0 ? FD2_TITLE_ANIM_DELAY_FAST_MS
        : index == 5 || index == 7 ? FD2_TITLE_ANIM_DELAY_MEDIUM_MS
                                  : FD2_TITLE_ANIM_DELAY_SLOW_MS;
    if (delay != expected_delay || check_input != (index == 3)) unexpected++;
    intro_animation_calls[index]++;
    return index == intro_animation_fault ? intro_animation_code : 0;
}

static void title_test_clear(fd2_vga *vga, uint8_t fill) {
    memset(vga->framebuffer, fill, sizeof(vga->framebuffer));
}
static void title_test_palette(fd2_vga *vga, const uint8_t *data) {
    if(title_comp_active || title_ani_active){fd2_vga_set_palette(vga,data);return;}
    (void)vga;
    if (data != test_palette_data) unexpected++;
}
static void title_test_brightness(fd2_vga *vga, int brightness) {
    if(title_comp_active || title_ani_active){fd2_vga_set_brightness(vga,brightness);return;}
    (void)vga;
    (void)brightness;
}
static int title_test_palette_add(fd2_vga *vga,int first,int last,int add) {
    white_calls++;
    if(first!=0 || last!=255 || add!=64){unexpected++;return -1;}
    if(white_error)return -1;
    if(fd2_vga_palette_add(vga,first,last,add)!=0)return -1;
    for(size_t i=0;i<sizeof(vga->dac);i++)if(vga->dac[i]!=255u){unexpected++;return -1;}
    return 0;
}
static void title_test_fade(fd2_vga *vga, int start, int steps,
                            uint8_t r, uint8_t g, uint8_t b) {
    if(title_comp_active){title_comp_before_fade(vga,start,steps,r,g,b);fd2_vga_palette_fade_to(vga,start,steps,r,g,b);title_comp_after_fade(start,steps,r,g,b);return;}
    (void)vga; (void)start; (void)steps; (void)r; (void)g; (void)b;
}
static int title_test_present(fd2_vga *vga) {
    if(title_comp_active){title_comp_bad=1;return 0;}
    vga->frame_deadline_ns=vga->frame_interval_ns=0;presents++;return 0;
}
static void title_test_upload(fd2_vga *vga) {
    if(title_comp_active){title_comp_upload(vga);return;}
    if(++presents==f5_upload_fault_at){
        f5_output_error=-91;f5_upload_queued=(int)vga->input.queued_count;
        if(f5_upload_host)vga->input.quit_requested=1;
    }
}
static int title_test_delay(uint32_t delay) {
    if(++f5_wait_calls==f5_wait_fault_call) { f5_source_error=-79;return -1; }
    if(title_comp_active){title_comp_delay(delay);return 0;}
    if (!input_contract || !menu_frames) return 0;
    if (delay==FD2_TITLE_CONFIRM_FLASH_DELAY_MS) { menu_flash_delays++;return 0; }
    if (delay!=1u || !menu_deferred || menu_idle_polls || menu_frames!=1) {
        menu_bad_delay++;return 0;
    }
    menu_idle_polls++;
    if (menu_deferred==2) input_vga->input.quit_requested=1;
    else {
        /* 无映射键也只读一项；下一轮须保留同一菜单，再读方向与确认。 */
        title_queue_key(input_vga,SDL_SCANCODE_A);
        title_queue_key(input_vga,SDL_SCANCODE_DOWN);
        title_queue_key(input_vga,SDL_SCANCODE_RETURN);
    }
    return 0;
}
/* 此TU只核真实caller的次序/返回传播；65/64级与DAC公式由Scene原码门验证。 */
static int title_test_palette_fade(fd2_vga *vga,int fade_in,int fast) {
    if(title_ani_active)return fd2_scene_palette_fade(vga,fade_in,1);
    if(title_comp_active){if(fast)title_comp_bad=1;int r=fd2_scene_palette_fade(vga,fade_in,1);title_comp_fade_endpoint(vga,fade_in);return r;}
    if ((fade_in!=0 && fade_in!=1) || fast || fade_calls>=32) { unexpected++;return FD2_SCENE_RESULT_ERROR; }
    fade_directions[fade_calls++]=fade_in;
    if (fade_calls==fade_fail_call) return fade_fail_result;
    if (title_test_quit(&vga->input)) return FD2_SCENE_RESULT_HOST_QUIT;
    for(int i=0;i<(fade_in?65:64);i++)title_test_upload(vga);
    return FD2_SCENE_RESULT_OK;
}
static int title_test_observe(const fd2_input *input) {
    if(title_comp_active)return 0;
    (void)input;
    return intro_skip_scroll;
}
static int title_test_quit(fd2_input *input) {
    if(title_comp_active)return title_comp_bad||input->quit_requested;
    return input->quit_requested || (menu_quit && menu_frames > 0);
}
static int title_test_action(fd2_input *input, fd2_input_context context,
                              fd2_input_action *action) {
    if(title_comp_active){if(context!=FD2_INPUT_CONTEXT_TITLE)title_comp_bad=1;return title_comp_read_action(action);}
    if(input_contract){
        action_reads++;
        int result=fd2_input_take_action(input,context,action);
        if (result) {
            if (read_count>=4) unexpected++;
            else {read_frames[read_count]=menu_frames;read_selections[read_count]=menu_selected;}
            read_count++;
        }
        return result;
    }
    (void)input;
    if (context != FD2_INPUT_CONTEXT_TITLE || action_reads++) unexpected++;
    *action = FD2_INPUT_ACTION_CONFIRM;
    return 1;
}
static int title_test_menu(int count, int selected, int highlighted,
                            fd2_title_draw_item_fn draw, void *userdata) {
    if(title_comp_active){title_comp_menu_draw(count,selected,highlighted);return fd2_title_draw_menu_frame(count,selected,highlighted,draw,userdata);}
    menu_frames++;
    menu_selected=selected;
    if(input_contract && menu_frames==1) {
        menu_queue_empty=!fd2_input_has_any_key(&input_vga->input);
        if (!menu_deferred) {
            title_queue_key(input_vga,SDL_SCANCODE_DOWN);
            title_queue_key(input_vga,SDL_SCANCODE_RETURN);
        }
    }
    return fd2_title_draw_menu_frame(count, selected, highlighted, draw, userdata);
}

static fd2_field_play_context *chain_context;
static fd2_field_play_result chain_results[3];
static int chain_battles, chain_preparations;
static size_t chain_stages[3];
static int chain_loaded[3], chain_handoffs[3];
static int chain_first_load_stage;
static fd2_field_prepare_result chain_prepare_result;
static fd2_field_play_result chain_battle(fd2_vga *vga, const fd2_archive *fdother,
        size_t stage, int once, const fd2_field_handoff *handoff, fd2_field_audio *audio,
        const char *save_path, int load_active, fd2_field_play_context *context) {
    (void)vga;(void)fdother;(void)audio;(void)save_path;
    if(context!=chain_context || once || chain_battles>=3)abort();
    int index=chain_battles++;
    chain_stages[index]=stage;chain_loaded[index]=load_active;chain_handoffs[index]=handoff!=NULL;
    if(index==0 && chain_first_load_stage>=0)stage=(size_t)chain_first_load_stage;
    context->rng.state++;context->experience_value+=2;context->message_number+=3;
    if(stage>0u && fd2_field_stage_is_supported(stage))context->has_preparation=0;
    if(stage==0u && chain_results[index]==FD2_FIELD_PLAY_RETURN_VICTORY){
        context->preparation.stage_id=1;context->preparation.unit_count=5;context->has_preparation=1;
    }
    return chain_results[index];
}
static fd2_field_prepare_result chain_prepare(fd2_vga *vga, const fd2_archive *fdother,
        fd2_field_play_context *context, const char *save_path, fd2_field_audio *audio) {
    (void)vga;(void)fdother;(void)save_path;(void)audio;
    if(context!=chain_context)abort();
    chain_preparations++;return chain_prepare_result;
}

static int title_test_wait_ticks(uint32_t ticks) {
    if(title_comp_active){title_comp_wait_ticks(ticks);return 0;}
    return title_test_delay(ticks);
}

/* 只在此测试TU替换I/O；包含完整main，未增加生产hook或复制标题逻辑。 */
#define fd2_field_play_run_context chain_battle
#define fd2_field_prepare_run chain_prepare
#define main fd2_program_main
#define fd2_bios_clock_error title_test_source_error
#define fd2_vga_output_error title_test_output_error
#define fd2_pcm_play_replace title_test_pcm_replace
#define fd2_pcm_stop title_test_pcm_stop
#define fd2_bgm_play title_test_bgm_play
#define fd2_archive_get title_test_archive_get
#define fd2_archive_open_mem title_test_archive_open_mem
#define fd2_archive_close title_test_archive_close
#define fd2_image_decode_entry title_test_image_decode_entry
#define fd2_image_decode_buf title_test_image_decode_buf
#define fd2_image_free title_test_image_free
#define fd2_animation_play_hooked title_test_animation
#define fd2_animation_play title_test_intro_animation
#define fd2_vga_clear title_test_clear
#define fd2_vga_set_palette title_test_palette
#define fd2_vga_set_brightness title_test_brightness
#define fd2_vga_palette_add title_test_palette_add
#define fd2_scene_palette_fade title_test_palette_fade
#define fd2_vga_palette_fade_to title_test_fade
#define fd2_vga_present title_test_present
#define fd2_vga_upload_current_frame title_test_upload
#define fd2_delay_ms title_test_delay
#define fd2_vga_delay_ms(vga,value) title_test_delay(value)
#define fd2_wait_ticks title_test_wait_ticks
#define fd2_vga_wait_ticks(vga,value) title_test_wait_ticks(value)
#define fd2_input_take_quit title_test_quit
#define fd2_input_take_action title_test_action
#define fd2_input_has_any_key title_test_observe
#define fd2_input_clear_keys title_test_input_clear
#define fd2_title_draw_menu_frame title_test_menu
#ifndef FD2_TITLE_TEST_MAIN
#define FD2_TITLE_TEST_MAIN "main.c"
#endif
#include FD2_TITLE_TEST_MAIN
#undef main
#undef fd2_bios_clock_error
#undef fd2_vga_output_error
#undef fd2_pcm_play_replace
#undef fd2_pcm_stop
#undef fd2_bgm_play
#undef fd2_input_has_any_key
#undef fd2_input_clear_keys
#undef fd2_scene_palette_fade
#undef fd2_vga_palette_add

static void reset_test(void) {
    f5_source_error=f5_wait_fault_call=f5_wait_calls=0;
    f5_upload_fault_at=f5_upload_host=f5_upload_queued=0;
    f5_output_error=f5_audio_at=f5_audio_calls=f5_audio_fault=f5_audio_status=f5_bgm_output=0;
    f5_mutate_animation=f5_animation_source_fault=f5_bgm_fail=f5_bgm_calls=0;
    palette_fault = base_fault = test_animation_result = menu_quit = 0;
    palette_reads = title_reads = base_decodes = menu_decodes = 0;
    animation_calls = archive_opens = archive_closes = image_live = 0;
    menu_frames = action_reads = presents = unexpected = 0;
    intro_enabled = intro_skip_scroll = 0;
    intro_resource_fault = intro_image_fault = intro_animation_fault = -1;
    intro_animation_code = 0;
    input_contract=input_clears=menu_queue_empty=quit_on_clear=0;input_time=0;input_vga=NULL;
    fade_calls=fade_fail_call=fade_fail_result=0;memset(fade_directions,0,sizeof(fade_directions));
    white_calls=white_error=0;
    menu_deferred=menu_idle_polls=menu_bad_delay=menu_flash_delays=0;
    menu_selected=read_count=0;
    memset(read_frames,0,sizeof(read_frames));memset(read_selections,0,sizeof(read_selections));
    memset(clear_had_pending,0,sizeof(clear_had_pending));
    memset(presents_at_clear,0,sizeof(presents_at_clear));
    memset(intro_resource_reads, 0, sizeof(intro_resource_reads));
    memset(intro_image_decodes, 0, sizeof(intro_image_decodes));
    memset(intro_animation_calls, 0, sizeof(intro_animation_calls));
}

static int run_case(const char *name) {
    reset_test();
    fd2_title_action expected = FD2_TITLE_ACTION_ERROR;
    if (!strcmp(name, "palette-missing")) palette_fault = 1;
    else if (!strcmp(name, "palette-short")) palette_fault = 2;
    else if (!strcmp(name, "base-error")) base_fault = 1;
    else if (!strcmp(name, "animation-error")) test_animation_result = -1;
    else if (!strcmp(name, "title-white-error")) white_error = 1;
    else if (!strcmp(name, "animation-quit")) {
        test_animation_result = -2;
        expected = FD2_TITLE_ACTION_HOST_QUIT;
    } else if (!strcmp(name, "menu-quit")) {
        menu_quit = 1;
        expected = FD2_TITLE_ACTION_HOST_QUIT;
    } else if (!strcmp(name, "normal") || !strcmp(name, "animation-skip")) {
        test_animation_result = !strcmp(name, "animation-skip");
        expected = FD2_TITLE_ACTION_NEW_GAME;
    } else {
        fprintf(stderr, "unknown case: %s\n", name);
        return 1;
    }
    fd2_vga vga = {0};
    CHECK(boot_intro_title(&vga, NULL, NULL, 0, 1) == expected);
    CHECK(palette_reads == 1 && unexpected == 0 && image_live == 0);
    CHECK(archive_opens == archive_closes);
    if (palette_fault) {
        CHECK(!animation_calls && !title_reads && !base_decodes && !presents);
    } else {
        CHECK(animation_calls == 1);
    }
    if (expected == FD2_TITLE_ACTION_ERROR || test_animation_result == -2) {
        CHECK(!menu_decodes && !menu_frames && !action_reads);
        if (test_animation_result < 0) CHECK(!title_reads && !base_decodes && !presents);
        if (base_fault) CHECK(title_reads == 1 && base_decodes == 1 && !presents);
        if (white_error) CHECK(white_calls==1 && !title_reads && !base_decodes && !presents);
    } else {
        CHECK(title_reads == 2 && base_decodes == 1 && menu_decodes == 6);
        CHECK(menu_frames == (menu_quit ? 1 : 1 + FD2_TITLE_CONFIRM_FLASH_FRAMES));
        CHECK(action_reads == (menu_quit ? 0 : 1) && presents > 0);
        if (menu_quit) {
            CHECK(vga.framebuffer[0] == 1);
        } else {
            static const uint8_t cleared[VGA_H * VGA_STRIDE] = {0};
            CHECK(memcmp(vga.framebuffer, cleared, sizeof(cleared)) == 0);
        }
    }
    return 0;
}

typedef struct {
    const char *name;
    int resource, image, animation, animation_code, skip;
    fd2_title_action expected;
} intro_case;

static const intro_case intro_cases[] = {
    {"intro-palette76", 76,-1,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-palette99", 99,-1,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-palette101",101,-1,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-palette102",102,-1,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-palette0",   0,-1,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-image74",   -1,74,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-image69",   -1,69,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-image100",  -1,100,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-image75",   -1,75,-1,0,0,FD2_TITLE_ACTION_ERROR},
    {"intro-ani3", -1,-1,3,-1,0,FD2_TITLE_ACTION_ERROR},
    {"intro-ani4", -1,-1,4,-1,0,FD2_TITLE_ACTION_ERROR},
    {"intro-ani5", -1,-1,5,-1,0,FD2_TITLE_ACTION_ERROR},
    {"intro-ani6", -1,-1,6,-1,0,FD2_TITLE_ACTION_ERROR},
    {"intro-ani7", -1,-1,7,-1,0,FD2_TITLE_ACTION_ERROR},
    {"intro-ani8", -1,-1,8,-1,0,FD2_TITLE_ACTION_ERROR},
    {"intro-ani0", -1,-1,0,-1,0,FD2_TITLE_ACTION_ERROR},
    {"intro-normal", -1,-1,-1,0,0,FD2_TITLE_ACTION_NEW_GAME},
    {"intro-scroll-skip", -1,-1,-1,0,1,FD2_TITLE_ACTION_NEW_GAME},
    {"intro-ani3-skip", -1,-1,3,1,0,FD2_TITLE_ACTION_NEW_GAME},
    {"intro-ani3-quit", -1,-1,3,-2,0,FD2_TITLE_ACTION_HOST_QUIT},
    {"intro-ani4-quit", -1,-1,4,-2,0,FD2_TITLE_ACTION_HOST_QUIT},
};

static int run_intro_case(const intro_case *test) {
    reset_test();
    intro_enabled = 1;
    intro_resource_fault = test->resource;
    intro_image_fault = test->image;
    intro_animation_fault = test->animation;
    intro_animation_code = test->animation_code;
    intro_skip_scroll = test->skip;
    fd2_vga vga = {0};
    CHECK(boot_intro_title(&vga, NULL, NULL, 1, 1) == test->expected);
    CHECK(!unexpected && !image_live && archive_opens == archive_closes);
    if (test->expected != FD2_TITLE_ACTION_NEW_GAME) {
        CHECK(!title_reads && !base_decodes && !menu_decodes &&
              !menu_frames && !action_reads && !animation_calls);
        if (test->resource >= 0) CHECK(intro_resource_reads[test->resource] == 1);
        if (test->image >= 0) CHECK(intro_image_decodes[test->image] == 1);
        if (test->animation >= 0) {
            const int order[] = {3,4,5,6,7,8,0};
            int after_failure = 0;
            for (size_t i = 0; i < sizeof(order) / sizeof(order[0]); i++) {
                if (after_failure) CHECK(!intro_animation_calls[order[i]]);
                if (order[i] == test->animation) after_failure = 1;
            }
            CHECK(intro_animation_calls[test->animation] == 1);
        }
        if (test->resource == 0) CHECK(!intro_animation_calls[5]);
    } else {
        CHECK(animation_calls == 1 && menu_decodes == 6 && action_reads == 1);
        CHECK(intro_animation_calls[3] == 1);
        const int rest[] = {4,5,6,7,8,0};
        for (size_t i = 0; i < sizeof(rest) / sizeof(rest[0]); i++)
            CHECK(intro_animation_calls[rest[i]] == !test->skip);
    }
    return 0;
}

static void chain_reset(fd2_field_play_context *context) {
    memset(context,0,sizeof(*context));context->initialized=1;
    context->rng.state=0x1234;context->experience_value=7;context->message_number=99;
    chain_context=context;chain_battles=chain_preparations=0;
    chain_first_load_stage=-1;
    memset(chain_results,0,sizeof(chain_results));chain_prepare_result=FD2_FIELD_PREPARE_DEPART;
}
static int test_app_chain(void) {
    fd2_field_play_context context;fd2_field_handoff handoff={0};chain_reset(&context);
    fd2_vga vga={0};fd2_input_init(&vga.input);vga.info_panel_suppressed=1;
    chain_results[0]=FD2_FIELD_PLAY_RETURN_VICTORY;chain_results[1]=FD2_FIELD_PLAY_RETURN_HOST_QUIT;
    CHECK(run_field_chain(&vga,NULL,NULL,"unused",&context,FD2_APP_FLOW_BATTLE,0,&handoff,0)==FD2_APP_FLOW_EXIT);
    CHECK(vga.info_panel_suppressed==1);
    CHECK(chain_battles==2 && chain_preparations==1 && chain_stages[0]==0 && chain_stages[1]==1);
    CHECK(chain_handoffs[0] && !chain_handoffs[1] && !chain_loaded[0] && !chain_loaded[1]);
    CHECK(context.rng.state==0x1236 && context.experience_value==11 && context.message_number==105);
    /* 第一场入口是stage1；战场Load恢复stage0，实际结算后仍交同一准备owner。 */
    chain_reset(&context);chain_first_load_stage=0;
    chain_results[0]=FD2_FIELD_PLAY_RETURN_VICTORY;chain_results[1]=FD2_FIELD_PLAY_RETURN_HOST_QUIT;
    CHECK(run_field_chain(&vga,NULL,NULL,"unused",&context,FD2_APP_FLOW_BATTLE,1,NULL,1)==FD2_APP_FLOW_EXIT);
    CHECK(vga.info_panel_suppressed==1);
    CHECK(chain_battles==2 && chain_preparations==1 && chain_stages[0]==1 && chain_stages[1]==1);
    CHECK(chain_loaded[0] && !chain_loaded[1] && !chain_handoffs[0] && !chain_handoffs[1]);
    CHECK(context.rng.state==0x1236 && context.experience_value==11 && context.message_number==105);
    chain_reset(&context);chain_results[0]=FD2_FIELD_PLAY_RETURN_DEFEAT;
    CHECK(run_field_chain(&vga,NULL,NULL,"unused",&context,FD2_APP_FLOW_BATTLE,1,NULL,1)==FD2_APP_FLOW_TITLE);
    CHECK(!vga.info_panel_suppressed);
    CHECK(chain_battles==1 && !chain_preparations && chain_stages[0]==1 && chain_loaded[0] && !chain_handoffs[0]);
    /* 仅返回VICTORY而没有提交准备owner仍必须报错，与B7开放stage1结算无关。 */
    chain_reset(&context);chain_results[0]=FD2_FIELD_PLAY_RETURN_VICTORY;vga.info_panel_suppressed=1;
    CHECK(run_field_chain(&vga,NULL,NULL,"unused",&context,FD2_APP_FLOW_BATTLE,1,NULL,0)==FD2_APP_FLOW_ERROR);
    CHECK(vga.info_panel_suppressed==1);
    CHECK(chain_battles==1 && !chain_preparations);
    const fd2_field_prepare_result endings[]={FD2_FIELD_PREPARE_TITLE,FD2_FIELD_PREPARE_HOST_QUIT,FD2_FIELD_PREPARE_LEAVE_GAME,FD2_FIELD_PREPARE_ERROR};
    const fd2_app_flow flows[]={FD2_APP_FLOW_TITLE,FD2_APP_FLOW_EXIT,FD2_APP_FLOW_LEAVE_GAME,FD2_APP_FLOW_ERROR};
    for(unsigned stage=1;stage<=2;stage++)for(size_t i=0;i<sizeof(endings)/sizeof(endings[0]);i++){
        chain_reset(&context);context.has_preparation=1;context.preparation.stage_id=(uint8_t)stage;
        vga.info_panel_suppressed=1;
        fd2_field_play_context before=context;chain_prepare_result=endings[i];
        CHECK(run_field_chain(&vga,NULL,NULL,"unused",&context,FD2_APP_FLOW_PREPARE,0,NULL,0)==flows[i]);
        CHECK(vga.info_panel_suppressed==(i==0 || i==2 ? 0:1));
        CHECK(!chain_battles && chain_preparations==1 && !memcmp(&context,&before,sizeof(context)));
    }
    /* B7 stage2出阵成功后由Play消费prep owner；宿主退出不重复进入准备页。 */
    chain_reset(&context);context.has_preparation=1;context.preparation.stage_id=2;context.preparation.unit_count=6;
    vga.info_panel_suppressed=1;
    chain_results[0]=FD2_FIELD_PLAY_RETURN_HOST_QUIT;
    CHECK(run_field_chain(&vga,NULL,NULL,"unused",&context,FD2_APP_FLOW_PREPARE,0,NULL,0)==FD2_APP_FLOW_EXIT);
    CHECK(vga.info_panel_suppressed==1);
    CHECK(chain_battles==1 && chain_preparations==1 && chain_stages[0]==2);
    CHECK(!chain_loaded[0] && !chain_handoffs[0] && !context.has_preparation);
    CHECK(context.preparation.stage_id==2 && context.preparation.unit_count==6);
    CHECK(context.rng.state==0x1235 && context.experience_value==9 && context.message_number==102);
    /* S3已发布stage3及20入场；此处只核App路由，20不冒称战斗开放。 */
    const unsigned published[]={3,20,21,29};
    for(size_t i=0;i<sizeof(published)/sizeof(published[0]);i++) {
        chain_reset(&context);context.has_preparation=1;
        context.preparation.stage_id=(uint8_t)published[i];context.preparation.unit_count=6;
        chain_results[0]=FD2_FIELD_PLAY_RETURN_HOST_QUIT;
        CHECK(fd2_field_stage_is_supported(published[i]));
        CHECK(run_field_chain(&vga,NULL,NULL,"unused",&context,FD2_APP_FLOW_PREPARE,0,NULL,0)==FD2_APP_FLOW_EXIT);
        CHECK(chain_battles==1 && chain_preparations==1 && chain_stages[0]==published[i] && !context.has_preparation);
    }
    /* S5已绑定21..29；资源30是内部尾声，不能充作普通战场入口。 */
    chain_reset(&context);context.has_preparation=1;context.preparation.stage_id=30;
    CHECK(!fd2_field_stage_is_supported(30));
    vga.info_panel_suppressed=1;
    fd2_field_play_context before=context;
    CHECK(run_field_chain(&vga,NULL,NULL,"unused",&context,FD2_APP_FLOW_PREPARE,0,NULL,0)==FD2_APP_FLOW_ERROR);
    CHECK(vga.info_panel_suppressed==1);
    CHECK(!chain_battles && chain_preparations==1 && !memcmp(&context,&before,sizeof(context)));
    return 0;
}

static int test_title_clear_and_two_ordered_keys(void) {
    reset_test();input_contract=1;
    fd2_vga vga={0};input_vga=&vga;fd2_input_init(&vga.input);
    CHECK(boot_intro_title(&vga,NULL,NULL,0,1)==FD2_TITLE_ACTION_LOAD);
    CHECK(input_clears==1 && clear_had_pending[0] && menu_queue_empty);
    CHECK(action_reads==2 && !fd2_input_has_any_key(&vga.input));
    CHECK(read_count==2 && read_frames[0]==1 && read_frames[1]==2 &&
          read_selections[0]==0 && read_selections[1]==1);
    CHECK(!menu_idle_polls && !menu_bad_delay && menu_flash_delays==8);
    CHECK(!unexpected && !image_live && archive_opens==archive_closes);
    for(int skip=0;skip<2;skip++) {
        reset_test();intro_enabled=1;intro_skip_scroll=skip;
        CHECK(boot_intro_title(&vga,NULL,NULL,1,1)==FD2_TITLE_ACTION_NEW_GAME);
        CHECK(input_clears==3); /* scroll入口、LABEL32的100ms后、title fade后。 */
    }
    for(int point=1;point<=3;point++) {
        reset_test();memset(&vga,0,sizeof(vga));fd2_input_init(&vga.input);
        intro_enabled=intro_skip_scroll=1;quit_on_clear=point;
        CHECK(boot_intro_title(&vga,NULL,NULL,1,1)==FD2_TITLE_ACTION_HOST_QUIT);
        CHECK(input_clears==point && presents==presents_at_clear[point-1] && !menu_frames && !action_reads);
        CHECK(!image_live && archive_opens==archive_closes && !unexpected);
    }
    return 0;
}

static int test_title_blocked_read_one(void) {
    for(int host=0;host<2;host++) {
        reset_test();input_contract=1;menu_deferred=host?2:1;
        fd2_vga vga={0};input_vga=&vga;fd2_input_init(&vga.input);
        CHECK(boot_intro_title(&vga,NULL,NULL,0,1)==
            (host?FD2_TITLE_ACTION_HOST_QUIT:FD2_TITLE_ACTION_LOAD));
        CHECK(menu_idle_polls==1 && !menu_bad_delay && !unexpected && !image_live);
        CHECK(archive_opens==archive_closes && input_clears==1);
        if(host) {
            CHECK(read_count==0 && menu_frames==1 && !menu_flash_delays);
        } else {
            CHECK(read_count==3 && read_frames[0]==1 && read_frames[1]==2 && read_frames[2]==3);
            CHECK(read_selections[0]==0 && read_selections[1]==0 && read_selections[2]==1);
            CHECK(menu_frames==3+FD2_TITLE_CONFIRM_FLASH_FRAMES && menu_flash_delays==8);
            CHECK(!fd2_input_has_any_key(&vga.input));
        }
    }
    return 0;
}

static int test_intro_fade_callers(void) {
    static const int directions[]={1,0,0,1, 0,1,0,1, 0,0,0,1,
        0,0,0,1, 0,0,1, 0,1, 0,1,0,1, 0};
    fd2_vga vga={0};
    reset_test();intro_enabled=1;
    vga.frame_deadline_ns=UINT64_C(999999999999);vga.frame_interval_ns=16000000;
    CHECK(boot_intro_title(&vga,NULL,NULL,1,1)==FD2_TITLE_ACTION_NEW_GAME);
    CHECK(vga.frame_deadline_ns==UINT64_C(999999999999) && vga.frame_interval_ns==16000000);
    CHECK(fade_calls==(int)(sizeof(directions)/sizeof(directions[0])) &&
        !memcmp(fade_directions,directions,sizeof(directions)));
    for(int fault=1;fault<=26;fault++)for(int host=0;host<2;host++) {
        reset_test();memset(&vga,0,sizeof(vga));intro_enabled=1;
        fade_fail_call=fault;fade_fail_result=host?FD2_SCENE_RESULT_HOST_QUIT:FD2_SCENE_RESULT_ERROR;
        CHECK(boot_intro_title(&vga,NULL,NULL,1,1)==
            (host?FD2_TITLE_ACTION_HOST_QUIT:FD2_TITLE_ACTION_ERROR));
        CHECK(fade_calls==fault && !image_live && archive_opens==archive_closes && !unexpected);
        if(fault<26)CHECK(!menu_frames && !action_reads);
        else CHECK(vga.framebuffer[0]==1);
    }
    return 0;
}

/* 真实boot/palette caller的八个新故障场景；无窗口/资源写入。 */
static int test_title_source_errors(void) {
    reset_test();fd2_vga vga={0};
    memset(vga.framebuffer,42,sizeof vga.framebuffer);fd2_vga before=vga;
    f5_bgm_fail=1;
    CHECK(boot_intro_title(&vga,(fd2_bgm_player *)(void *)&vga,NULL,1,0)==FD2_TITLE_ACTION_ERROR);
    CHECK(f5_source_error==-66&&f5_bgm_calls==1&&!memcmp(&vga,&before,sizeof vga));
    CHECK(!palette_reads&&!title_reads&&!base_decodes&&!action_reads&&!image_live);
    /* 首tick、scroll首等待、title fade：失败立即停止且释放已借图像。 */
    for(int mode=0;mode<3;mode++) {
        reset_test();memset(&vga,0,sizeof vga);intro_enabled=mode!=2;
        f5_wait_fault_call=mode==1?3:1;
        CHECK(boot_intro_title(&vga,NULL,NULL,mode!=2,1)==FD2_TITLE_ACTION_ERROR);
        CHECK(f5_source_error==-79&&f5_wait_calls==f5_wait_fault_call);
        CHECK(!image_live&&archive_opens==archive_closes&&!action_reads&&!menu_frames&&!unexpected);
        CHECK(!vga.input.quit_requested);
        if(mode==0)CHECK(fade_calls==1&&!input_clears);
    }
    reset_test();memset(&vga,0,sizeof vga);fd2_input_init(&vga.input);
    input_contract=menu_deferred=1;input_vga=&vga;f5_wait_fault_call=42;
    CHECK(boot_intro_title(&vga,NULL,NULL,0,1)==FD2_TITLE_ACTION_ERROR);
    CHECK(f5_source_error==-79&&f5_wait_calls==42&&menu_frames==1&&read_count==0);
    CHECK(!image_live&&archive_opens==archive_closes&&!menu_idle_polls&&!vga.input.quit_requested);
    /* SOURCE首错不回滚已到色区；正常decode错/HOST仍沿原borrow合同。 */
    for(int mode=0;mode<3;mode++) {
        reset_test();memset(&vga,0,sizeof vga);
        memset(vga.palette,31,sizeof vga.palette);memset(vga.dac,124,sizeof vga.dac);
        f5_mutate_animation=1;f5_animation_source_fault=mode==0?-61:0;
        test_animation_result=mode==2?-2:-1;
        CHECK(title_animation_play(&vga,1,15,1,NULL,NULL)==test_animation_result);
        CHECK(vga.framebuffer[0]==9);
        for(size_t i=0;i<sizeof vga.palette;i++) {
            CHECK(vga.palette[i]==(mode==0&&i==0?7:31));
            CHECK(vga.dac[i]==(mode==0&&i==0?28:124));
        }
    }
    reset_test();return 0;
}


/* 41级title渐变之后的完整palette提交、第一菜单页提交两个无wait边界。
 * 已达绘制保留；失败不可clear/消费队列、误分HOST或追加确认/移动音效。 */
static int test_title_upload_errors(void){
 for(int menu=0;menu<2;menu++)for(int host=0;host<2;host++){
  reset_test();fd2_vga vga={0};fd2_input_init(&vga.input);
  fd2_pcm_player player={0};fd2_pcm_bank bank={0};fd2_title_audio audio={&player,&player,&bank,&bank};
  input_contract=1;input_vga=&vga; /* ANI替身已排入一个早到RETURN。 */
  f5_upload_fault_at=42+menu;f5_upload_host=host;f5_audio_at=999;
  CHECK(boot_intro_title(&vga,NULL,&audio,0,1)==FD2_TITLE_ACTION_ERROR);
  CHECK(f5_output_error==-91&&!f5_source_error&&presents==42+menu);
  CHECK(vga.input.quit_requested==host&&input_clears==menu);
  CHECK(vga.input.queued_count==(unsigned)f5_upload_queued&&f5_upload_queued==(menu?2:1));
  CHECK(!action_reads&&!read_count&&!menu_flash_delays&&menu_frames==menu);
  CHECK(f5_audio_calls==3&&!image_live&&archive_opens==archive_closes&&!unexpected);
 }
 reset_test();return 0;
}

/* 只核同步Audio返回0而END已锁OS错的真实Main/title调用边界。 */
static int test_title_audio_return_errors(void){
 fd2_vga vga={0};fd2_pcm_player p={0};fd2_pcm_bank bank={0};
 fd2_title_audio audio={&p,&p,&bank,&bank};
 for(unsigned stop=0;stop<2;stop++)for(unsigned fault=1;fault<=3;fault++){
  reset_test();memset(&vga,0,sizeof vga);vga.input.queued_count=1;vga.input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
  fd2_vga before=vga;f5_audio_at=1;f5_audio_fault=(int)fault;
  CHECK((stop?title_letter_fly_audio_stop(&vga,&audio):title_audio_play(&vga,&audio,FD2_TITLE_SFX_MOVE))<0);
  CHECK(f5_audio_calls==1&&!!f5_source_error==!!(fault&2u)&&!!f5_output_error==!!(fault&1u));
  vga.input.quit_requested=1;CHECK(title_audio_play(&vga,&audio,FD2_TITLE_SFX_MOVE)<0&&f5_audio_calls==1);
  vga.input.quit_requested=0;CHECK(!memcmp(&vga,&before,sizeof vga));
 }
 reset_test();memset(&vga,0xa7,sizeof vga);fd2_vga before=vga;f5_bgm_output=1;
 CHECK(boot_intro_title(&vga,(fd2_bgm_player *)&vga,NULL,1,0)==FD2_TITLE_ACTION_ERROR);
 CHECK(f5_bgm_calls==1&&!f5_source_error&&f5_output_error==-91&&!memcmp(&vga,&before,sizeof vga));
 CHECK(!palette_reads&&!title_reads&&!action_reads&&!image_live&&!presents);
 /* 原ANI0 cue→stop→ENTER为三个已达调用；第四次MOVE返回点失败：保已读方向，不改selection、不读后继确认。 */
 reset_test();memset(&vga,0,sizeof vga);fd2_input_init(&vga.input);
 input_contract=1;input_vga=&vga;f5_audio_at=4;f5_audio_fault=1;
 CHECK(boot_intro_title(&vga,NULL,&audio,0,1)==FD2_TITLE_ACTION_ERROR);
 CHECK(f5_audio_calls==4&&!f5_source_error&&f5_output_error==-91&&read_count==1&&menu_frames==1&&menu_selected==0&&!menu_flash_delays);
 CHECK(vga.input.queued_count==1&&!image_live&&archive_opens==archive_closes&&!unexpected);
 /* 真实CONFIRM失败也不能进入8次确认flash；普通可选SFX资源失败仍非Source/OS。 */
 reset_test();memset(&vga,0,sizeof vga);f5_audio_at=4;f5_audio_fault=1;
 CHECK(boot_intro_title(&vga,NULL,&audio,0,1)==FD2_TITLE_ACTION_ERROR&&f5_audio_calls==4&&menu_frames==1&&action_reads==1);
 CHECK(!image_live&&archive_opens==archive_closes);
 reset_test();memset(&vga,0,sizeof vga);f5_audio_at=1;f5_audio_status=-55;
 CHECK(title_audio_play(&vga,&audio,FD2_TITLE_SFX_MOVE)==-55&&!title_owner_failed(&vga));
 /* ANI完成点发生OS错保已达色区，禁止后继stop/旧palette回写。 */
 reset_test();memset(&vga,0,sizeof vga);memset(vga.palette,31,768);memset(vga.dac,124,768);
 f5_mutate_animation=1;f5_audio_at=1;f5_audio_fault=1;
 CHECK(title_animation_play(&vga,1,15,1,NULL,NULL)<0&&vga.palette[0]==7&&vga.dac[0]==28&&vga.framebuffer[0]==9);
 CHECK(title_letter_fly_audio_stop(&vga,&audio)<0&&f5_audio_calls==1&&!f5_source_error);
 reset_test();puts("audio-return Main: BGM/title/menu/ANI-stop prefixes and optional policy PASS");return 0;
}

#include "f4_title_composition_probe.h"
#include "f4_title_animation_probe.h"

int main(int argc, char **argv) {
    if(argc==2&&!strcmp(argv[1],"--audio-return"))return test_title_audio_return_errors();
    if(argc==2&&!strcmp(argv[1],"--f5-source-errors"))return test_title_source_errors();
    if(argc==2&&!strcmp(argv[1],"--output-errors"))return test_title_upload_errors();
    if(argc>1&&!strcmp(argv[1],"--f4-title-ani"))return argc==3?f4_title_ani_cli(argv[2]):1;
    if(argc>1&&!strcmp(argv[1],"--f4-title"))return argc==3?f4_title_cli(argv[2]):1;
    if (argc == 2) {
        for (size_t i = 0; i < sizeof(intro_cases) / sizeof(intro_cases[0]); i++)
            if (!strcmp(argv[1], intro_cases[i].name)) return run_intro_case(&intro_cases[i]);
        return run_case(argv[1]);
    }
    const char *cases[] = {"palette-missing", "palette-short", "base-error",
                          "animation-error", "animation-quit", "title-white-error", "menu-quit",
                          "normal", "animation-skip"};
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++)
        if (run_case(cases[i])) return 1;
    for (size_t i = 0; i < sizeof(intro_cases) / sizeof(intro_cases[0]); i++)
        if (run_intro_case(&intro_cases[i])) return 1;
    CHECK(test_title_animation_palette_owner()==0);
    CHECK(test_app_chain()==0);
    CHECK(test_title_clear_and_two_ordered_keys()==0);
    CHECK(test_title_blocked_read_one()==0);
    CHECK(test_intro_fade_callers()==0);
    CHECK(test_title_source_errors()==0);
    CHECK(test_title_upload_errors()==0);
    puts("main_title_resource: 30 resource cases + App chain/fade callers passed; no SDL initialization/window");
    return 0;
}
