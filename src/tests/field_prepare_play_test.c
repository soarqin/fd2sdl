/* 26152/29300/29bcb准备页：真实资源纯绘制及替身present的逐帧输入。
 * 不开窗口，不替代DOSBox/WGC验收；文件只在宿主临时路径写隔离容器。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test_temp.h"
#include "field_prepare_play.h"

Uint64 SDLCALL prepare_test_ticks(void);
void prepare_test_present(fd2_vga *vga);
void prepare_test_present_timed(fd2_vga *vga, uint32_t delay);
void prepare_test_deadline(fd2_vga *vga);
static void *prepare_test_calloc(size_t count, size_t size);
static int prepare_test_pcm_replace(fd2_pcm_player *player, const fd2_pcm_bank *bank,
    size_t sample, int loop_count, float gain);
static int prepare_test_bgm_option(fd2_bgm_player *, int, int);
static int prepare_test_state_save(const char *path, size_t slot,
    const fd2_save_manual_state *state);
static int prepare_test_magic_draw(fd2_vga *vga, const fd2_field_info_assets *assets,
    const fd2_font *font, const fd2_text_entry *text,
    const fd2_field_unit *unit, int selected);

#define SDL_GetTicks prepare_test_ticks
#define fd2_vga_present prepare_test_present
#define fd2_vga_present_timed prepare_test_present_timed
#define fd2_vga_wait_frame_deadline prepare_test_deadline
#define calloc prepare_test_calloc
#define fd2_pcm_play_replace prepare_test_pcm_replace
#define fd2_bgm_set_music_enabled prepare_test_bgm_option
#define fd2_save_manual_state_save prepare_test_state_save
#define fd2_field_magic_list_draw prepare_test_magic_draw
#include "../field_prepare_play.c"
#undef SDL_GetTicks
#undef fd2_vga_present
#undef fd2_vga_present_timed
#undef fd2_vga_wait_frame_deadline
#undef calloc
#undef fd2_pcm_play_replace
#undef fd2_bgm_set_music_enabled
#undef fd2_save_manual_state_save
#undef fd2_field_magic_list_draw

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #expr); return 1; } } while (0)

static int option_sync_count, option_sync_enabled, option_sync_fade;
static int prepare_test_bgm_option(fd2_bgm_player *player, int enabled, int fade) {
    (void)player; option_sync_count++; option_sync_enabled=enabled; option_sync_fade=fade; return 0;
}
static uint64_t clock_ms;
static int service_exact_time,service_capture_first;
static unsigned service_delay_count;
static uint32_t service_delays[6000];
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
static unsigned frame_number, quit_frame;
static struct { unsigned frame; SDL_Scancode key; } pulses[20];
static size_t pulse_count;
static int music_tracks[20];
static size_t music_stages[20], music_count;
static unsigned fail_frame_allocation_at, sound_calls, fail_sound_call;
static size_t sound_samples[32];
static int sound_loops[32];
static float sound_last_gain;
static unsigned magic_draw_calls, fail_magic_draw_call, state_save_calls;
static int probe_title, title_background_ok, title_palette_ok;
static uint8_t expected_title_palette[VGA_PALETTE_SIZE];
static int slot_upper_probe,slot_upper_changed;
static uint8_t slot_upper_before[112u*VGA_W];

static void *prepare_test_calloc(size_t count, size_t size) {
    if (count == 1 && size == sizeof(fd2_vga) && fail_frame_allocation_at &&
        frame_number >= fail_frame_allocation_at) return NULL;
    return calloc(count, size);
}
static int prepare_test_pcm_replace(fd2_pcm_player *player, const fd2_pcm_bank *bank,
        size_t sample, int loop_count, float gain) {
    (void)player; (void)bank;
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

Uint64 SDLCALL prepare_test_ticks(void) { return clock_ms; }
void prepare_test_present(fd2_vga *vga) {
    clock_ms += 16; frame_number++;
    if(slot_upper_probe && memcmp(vga->framebuffer,slot_upper_before,sizeof(slot_upper_before)))slot_upper_changed=1;
    memset(vga->input.test_key_state, 0, sizeof(vga->input.test_key_state));
    for (size_t i = 0; i < pulse_count; i++) if (pulses[i].frame == frame_number)
        fd2_input_set_test_key_state(&vga->input, pulses[i].key, 1);
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
    if (probe_title && frame_number == 70) {
        title_background_ok = 1;
        for (size_t i = 0; i < 112u * 320u; i++) if (vga->framebuffer[i] != 0) title_background_ok = 0;
        title_palette_ok = memcmp(vga->palette, expected_title_palette, VGA_PALETTE_SIZE) == 0;
        for (size_t i = 0; i < VGA_PALETTE_SIZE; i++)
            if (vga->dac[i] != (uint8_t)((expected_title_palette[i] << 2) | (expected_title_palette[i] >> 4))) title_palette_ok = 0;
    }
    if ((quit_frame && frame_number == quit_frame) || frame_number > (service_exact_time ? 6000u:150u))
        vga->input.quit_requested = 1;
}
void prepare_test_present_timed(fd2_vga *vga, uint32_t delay) {
    uint64_t before=clock_ms;
    if(service_capture_first){memcpy(service_first_frame,vga->framebuffer,sizeof(service_first_frame));service_capture_first=0;}
    if(service_delay_count<6000u)service_delays[service_delay_count++]=delay;
    prepare_test_present(vga);if(service_exact_time)clock_ms=before+delay;
}
void prepare_test_deadline(fd2_vga *vga) { (void)vga; }

int fd2_field_play_request_music(fd2_field_play_context *context,
        size_t stage, int track, const char *reason) {
    (void)reason;
    if (!context || !context->initialized || music_count >= 20) return -1;
    music_stages[music_count] = stage;
    music_tracks[music_count++] = track;
    return 0;
}

static void controls(fd2_vga *vga) {
    fd2_input_init(&vga->input); clock_ms = 0; frame_number = quit_frame = 0;
    pulse_count = music_count = 0; memset(pulses, 0, sizeof(pulses));
    memset(music_stages, 0, sizeof(music_stages));
    fail_frame_allocation_at = sound_calls = fail_sound_call = 0;
    memset(sound_samples,0,sizeof(sound_samples));
    memset(sound_loops,0,sizeof(sound_loops));
    magic_draw_calls = fail_magic_draw_call = state_save_calls = 0;
    probe_title = title_background_ok = title_palette_ok = 0;
    service_exact_time=service_capture_first=0;service_delay_count=0;
    confirm_probe_assets=NULL;confirm_probe_hits=0;
    service_route_assets=NULL;service_route_products=NULL;service_route_menu_hits=service_route_product_hits=0;
    slot_upper_probe=slot_upper_changed=0;
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
        memset(expected->framebuffer, 0, sizeof(expected->framebuffer));
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
    char path[FD2_TEST_PATH_MAX]; CHECK(fd2_test_temp_path(path) == 0); CHECK(remove(path) == 0);
    CHECK(fd2_save_manual_state_save(path, 0, state) == 0);
    fd2_field_play_context context = {0}; context.initialized = 1;
    context.experience_value = 631; context.message_number = 99;
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
    CHECK(context.rng.state == 0x1234 && context.experience_value == 631);
    CHECK(title_background_ok && title_palette_ok);
    CHECK(option_sync_count==0 && music_count==0);
    for (size_t i = 0; i < sizeof(vga->framebuffer); i++) CHECK(vga->framebuffer[i] == 0);
    controls(vga); pulse(70, SDL_SCANCODE_RETURN);
    CHECK(fd2_field_prepare_load_title(vga, fdother, &context, path, NULL) == FD2_FIELD_PREPARE_DEPART);
    CHECK(context.has_preparation && context.preparation.stage_id == state->stage_id && context.preparation.unit_count == state->unit_count);
    CHECK(option_sync_count==1 && option_sync_enabled==(state->options[2]!=0) && option_sync_fade==0 && music_count==0);
    CHECK(context.music_enabled==(state->options[2]!=0) && context.sfx_enabled==(state->options[3]!=0));
    CHECK(memcmp(context.preparation.unit_area, state->unit_area, sizeof(state->unit_area)) == 0);
    CHECK(context.experience_value == 631 && context.rng.state == 0x1234);
    controls(vga); quit_frame = 3;
    before = context.preparation;
    CHECK(fd2_field_prepare_load_title(vga, fdother, &context, path, NULL) == FD2_FIELD_PREPARE_HOST_QUIT);
    CHECK(memcmp(&before, &context.preparation, sizeof(before)) == 0);
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
    context.preparation = *state; context.music_enabled=state->options[2]!=0;context.sfx_enabled=state->options[3]!=0; context.experience_value = 99;
    fd2_field_rng_seed(&context.rng, 17);
    prepare_ui ui = {vga, fdother, a, &context, NULL, path, -1, 0, -1};
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
    context.preparation = *state; context.music_enabled=state->options[2]!=0;context.sfx_enabled=state->options[3]!=0; context.experience_value = 42; context.message_number = 7;
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
    prepare_ui ui={vga,fdother,a,&context,NULL,path,-1,0,-1};
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
        prepare_ui ui={vga,fdother,a,&context,&audio,path,-1,0,-1};
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
    prepare_ui ui={vga,fdother,a,&context,NULL,path,-1,0,-1};
    uint16_t old=a->text.offsets[660];a->text.offsets[660]=(uint16_t)(a->text.size+1u);
    controls(vga);pulse(6,SDL_SCANCODE_RETURN);CHECK(slots_run(&ui,1,PREPARE_SLOT_HOTEL)==-1 && state_save_calls==0u);
    a->text.offsets[660]=old;CHECK(read_container(path,after)==0 && !memcmp(before,after,sizeof(before)));
    CHECK(remove(path)==0);return 0;
}

static int failure_and_cancel(fd2_archive *fdother, fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state, fd2_vga *vga) {
    char path[FD2_TEST_PATH_MAX]; CHECK(fd2_test_temp_path(path) == 0); CHECK(remove(path) == 0);
    fd2_field_play_context context = {0}; context.initialized = context.has_preparation = 1;
    context.preparation = *state; context.music_enabled=state->options[2]!=0;context.sfx_enabled=state->options[3]!=0; context.experience_value = 0xfeed1234u;
    prepare_ui ui = {vga, fdother, a, &context, NULL, path, -1, 0, -1};
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
    CHECK(remove(path) == 0);
    (void)fdother;
    return 0;
}

static int depart_capacity_and_leave(fd2_archive *fdother, fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state, fd2_vga *vga) {
    fd2_field_play_context context = {0}; context.initialized = context.has_preparation = 1;
    context.preparation = *state; context.music_enabled=state->options[2]!=0;context.sfx_enabled=state->options[3]!=0;
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
    prepare_ui selection_ui={vga,fdother,a,&context,NULL,"unused.SAV",-1,0,-1};
    selection_ui.service.quota=16;selection_ui.service.cursor=0;
    CHECK(fd2_field_prepare_assets_bind_roster(a,&context.preparation)==0);
    controls(vga);
    CHECK(prepare_service_selection_open(&selection_ui)==0 && selection_ui.service.page_kind==3 && frame_number==12u);
    CHECK(!memcmp(&context.preparation,&before,sizeof(before)) && context.preparation.unit_count==17);
    CHECK(prepare_service_selection_close(&selection_ui)==0 && frame_number==24u &&
          !memcmp(&context.preparation,&before,sizeof(before)));
    prepare_service_cleanup(&selection_ui);
    context.preparation = *state; context.music_enabled=state->options[2]!=0;context.sfx_enabled=state->options[3]!=0;
    CHECK(fd2_field_prepare_assets_bind_roster(a,&context.preparation)==0);
    prepare_ui ui = {vga, fdother, a, &context, NULL, "unused.SAV", -1, 0, -1};
    controls(vga); pulse(5, SDL_SCANCODE_RIGHT); pulse(7, SDL_SCANCODE_RIGHT); pulse(9, SDL_SCANCODE_RIGHT);
    pulse(11, SDL_SCANCODE_RETURN); pulse(22, SDL_SCANCODE_RETURN);
    CHECK(hotel_run(&ui) == 3); /* yes=真实离开，不是回标题或回城镇。 */
    CHECK(memcmp(&context.preparation, state, sizeof(*state)) == 0);
    return 0;
}

static int member_pages(fd2_archive *fdother, fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state, fd2_vga *vga) {
    fd2_field_play_context context = {0}; context.initialized = context.has_preparation = 1;
    context.preparation = *state; context.music_enabled=state->options[2]!=0;context.sfx_enabled=state->options[3]!=0; context.experience_value = 321;
    prepare_ui ui = {vga, fdother, a, &context, NULL, "unused.SAV", -1, 0, -1};
    fd2_pcm_player player={0};fd2_pcm_bank bank={0};fd2_field_audio audio={0};
    audio.player=&player;audio.ui_bank=&bank;ui.audio=&audio;
    uint8_t before[VGA_W * VGA_H];
    memset(vga->framebuffer, 0x63, sizeof(vga->framebuffer)); memcpy(before, vga->framebuffer, sizeof(before));
    controls(vga); pulse(12, SDL_SCANCODE_ESCAPE);
    CHECK(member_detail(&ui, 0) == 0);
    CHECK(sound_calls==4u && sound_samples[0]==5u && sound_samples[1]==5u &&
        sound_samples[2]==6u && sound_samples[3]==6u);
    CHECK(memcmp(before, vga->framebuffer, sizeof(before)) == 0);
    controls(vga); pulse(12, SDL_SCANCODE_ESCAPE); pulse(27, SDL_SCANCODE_ESCAPE);
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
    context.preparation = *state; context.music_enabled=state->options[2]!=0;context.sfx_enabled=state->options[3]!=0; context.experience_value = 321;
    fd2_pcm_player player = {0}; fd2_pcm_bank bank = {0};
    fd2_field_audio audio = {0}; audio.player = &player; audio.ui_bank = &bank;
    prepare_ui ui = {vga, fdother, a, &context, &audio, path, -1, 0, -1};
    /* 首次读键已经成功后，关页／翻页失败仍须返回ERROR，不能沿用0。
     * 等待RNG是合法UI副作用；这里锁准备表、贡献owner与零storage提交。 */
    for (unsigned fault = 0; fault < 5; fault++) {
        controls(vga); pulse(12, SDL_SCANCODE_ESCAPE); pulse(27, SDL_SCANCODE_ESCAPE);
        if (fault < 4) {
            const unsigned late_cues[4]={3u,3u,4u,5u};
            fail_sound_call=late_cues[fault]; /* 新两次open cue后，关全页/翻页才失败。 */
        }
        else fail_magic_draw_call = 2;
        CHECK(member_detail(&ui, fault == 0 ? 0u : 1u) == -1);
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
    controls(vga); pulse(6, SDL_SCANCODE_RETURN); pulse(24, SDL_SCANCODE_ESCAPE);
    fail_frame_allocation_at = 37; /* 首个详情正常关闭，下一成员列表才分配失败。 */
    CHECK(roster_run(&ui) == -1 && frame_number == 37);
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
    context.preparation = state; context.music_enabled=state.options[2]!=0;context.sfx_enabled=state.options[3]!=0; context.experience_value = 0xfeed1234u;
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
    context.preparation = state; context.music_enabled=state.options[2]!=0;context.sfx_enabled=state.options[3]!=0; context.preparation.unit_area[7] = 255; before = context;
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
    context.preparation=*stage1; context.has_preparation=1; before=context;
    prepare_ui ui={vga,fdother,assets,&context,NULL,path,-1,0,-1};
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
    CHECK(fd2_save_manual_state_save(path,0,&loaded)==0);
    uint8_t before[FD2_SAVE_FILE_SIZE],after[FD2_SAVE_FILE_SIZE]; CHECK(read_container(path,before)==0);
    fd2_field_play_context context={0};context.initialized=context.has_preparation=1;
    context.preparation=*state;context.music_enabled=state->options[2]!=0;context.sfx_enabled=state->options[3]!=0;context.bgm=(fd2_bgm_player *)&context;context.experience_value=99;
    prepare_ui ui={vga,fdother,assets,&context,NULL,path,-1,0,-1};
    controls(vga);option_sync_count=0;
    CHECK(music(&ui,13,"hotel")==0);
    pulse(6,SDL_SCANCODE_RETURN);pulse(13,SDL_SCANCODE_RETURN);
    CHECK(slots_run(&ui,0,0)==2);
    CHECK(option_sync_count==2 && option_sync_enabled==(loaded.options[2]!=0) && option_sync_fade==0);
    CHECK(context.music_enabled==(loaded.options[2]!=0) && context.sfx_enabled==(loaded.options[3]!=0));
    CHECK(music_count==1 && music_tracks[0]==13); /* Load不是退出酒店，不得发-1/10/13。 */
    CHECK(!memcmp(&context.preparation,&loaded,sizeof(loaded)) && context.experience_value==99);
    CHECK(read_container(path,after)==0 && !memcmp(before,after,sizeof(before)));
    context.preparation.options[2]=2;context.preparation.options[3]=3;
    CHECK(sync_audio_options(&ui)==0 && context.music_enabled==2 && context.sfx_enabled==3);
    CHECK(option_sync_enabled==1 && option_sync_fade==0 && music_count==1);
    CHECK(remove(path)==0);return 0;
}

static int hotel_notown_and_cache_boundary(fd2_archive *fdother,fd2_field_prepare_assets *assets,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    char path[FD2_TEST_PATH_MAX];CHECK(fd2_test_temp_path(path)==0 && remove(path)==0);
    fd2_field_play_context context={0};context.initialized=context.has_preparation=1;
    context.preparation=*state;context.experience_value=0x12345678u;fd2_field_rng_seed(&context.rng,0x4321);
    prepare_ui ui={vga,fdother,assets,&context,NULL,path,-1,0,-1};
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
    prepare_ui ui = {0}; ui.audio = &audio; ui.context = &context;
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
    prepare_ui ui={0};ui.vga=vga;ui.fdother=fdother;ui.assets=assets;ui.context=context;
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
        controls(vga);memset(vga->framebuffer,0x61,VGA_W*VGA_H);*expected=*vga;
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
    context->experience_value=0x12345678u;fd2_field_rng_seed(&context->rng,0x4321);
    CHECK(fd2_field_prepare_assets_open_service(assets,fdother,1)==0 &&
        fd2_field_prepare_assets_bind_roster(assets,state)==0);
    const uint8_t *row=fd2_field_stage_prepare_record_get(state->stage_id);CHECK(row);
    uint8_t stock[12];size_t count=0;
    while(count<12u && row[3u+count]!=255u){stock[count]=row[3u+count];count++;}
    CHECK(count && fd2_field_prepare_draw_items(products,assets,stock,count,0,0,0)==0);
    fd2_map_sprite_blit(products,&assets->service.scroll_arrows[4],298,120,-1);
    fd2_map_sprite_blit(products,&assets->service.scroll_arrows[count>6u ? 3u:4u],298,181,-1);
    CHECK(fd2_field_prepare_draw_town(vga,assets,state,1,0)==0);
    prepare_ui ui={vga,fdother,assets,context,NULL,"unused.SAV",-1,0,-1};
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
    prepare_ui ui={0};ui.vga=vga;ui.fdother=fdother;ui.assets=assets;ui.context=context;
    for(int kind=0;kind<2;kind++) {
        controls(vga);service_exact_time=1;ui.service=(prepare_service){0};
        ui.service.selector=1;ui.service.notice_origin=36887;ui.service.growth_line=3;
        ui.service.new_fragment=ui.service.old_fragment=SIZE_MAX;
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
            ui.service.growth_line==(kind ? 2u:3u) && ui.service.input_epoch==1u &&
            frame_number==1u && !service_delay_count && !sound_calls && !memcmp(context,before,sizeof(*before)));
    }
    free(source);free(expected);free(before);free(context);return 0;
}

static int service_present_leaves(fd2_archive *fdother,fd2_field_prepare_assets *a,
        const fd2_save_manual_state *state,fd2_vga *vga) {
    fd2_field_play_context *context=calloc(1,sizeof(*context)),*before=malloc(sizeof(*before));
    fd2_vga *expected=calloc(1,sizeof(*expected));CHECK(context&&before&&expected);
    context->initialized=context->has_preparation=1;context->preparation=*state;
    prepare_ui ui={0};ui.vga=vga;ui.assets=a;ui.fdother=fdother;ui.context=context;
    ui.menu_selected=ui.roster_selected=-1;ui.service.new_fragment=ui.service.old_fragment=SIZE_MAX;
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
    prepare_service_cleanup(&ui);ui.service=(prepare_service){0};

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
    (void)fd2_field_rng_next(&expected_rng);ui.service.sprite_phase=0;service_repaints=0;pulse(10,SDL_SCANCODE_DOWN);
    CHECK(prepare_service_read_key(&ui,&event,2,service_test_redraw)==0 && event.key==FD2_INPUT_KEY_DOWN);
    CHECK(service_repaints==2 && ui.service.highlight_phase==3u && ui.service.sprite_phase==1u && context->rng.state==expected_rng.state);
    controls(vga);service_exact_time=1;ui.service=(prepare_service){0};ui.service.notice_origin=36887;
    ui.service.new_fragment=ui.service.old_fragment=SIZE_MAX;context->message_number=7;
    fd2_field_rng_seed(&context->rng,0x7a18);expected_rng=context->rng;(void)fd2_field_rng_next(&expected_rng);
    pulse(1,SDL_SCANCODE_SPACE);*before=*context;
    CHECK(prepare_service_text(&ui,490,95,138)==0 && ui.service.input_epoch==1u && !ui.service.deferred_key_ready);
    CHECK(context->rng.state==expected_rng.state && frame_number<6u);
    before->rng=context->rng;CHECK(!memcmp(context,before,sizeof(*before)));
    controls(vga);service_exact_time=1;ui.service=(prepare_service){0};ui.service.notice_origin=36887;
    ui.service.new_fragment=ui.service.old_fragment=SIZE_MAX;
    fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,1);fd2_input_begin_test_frame(&vga->input,clock_ms);
    fd2_field_rng_seed(&context->rng,0x7a18);expected_rng=context->rng;(void)fd2_field_rng_next(&expected_rng);
    CHECK(prepare_service_text(&ui,490,95,138)==0 && ui.service.input_epoch==1u &&
        !ui.service.deferred_key_ready && context->rng.state==expected_rng.state && frame_number==1u);

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
    prepare_service_cleanup(&ui);prepare_service_cleanup(&ui);
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
    prepare_ui ui={vga,fdother,a,context,NULL,"unused.SAV",-1,0,-1};
    controls(vga);memset(vga->framebuffer,0x61,VGA_W*VGA_H);memcpy(expected,vga->framebuffer,VGA_W*VGA_H);
    ui.service.picker_mode=PREPARE_PICK_MENU;ui.service.highlight_phase=2;
    size_t zero=0,opaque=0;
    for(size_t i=0;i<4u;i++) {
        const fd2_image *icon=&a->service.icons[2u*i+(i==0u)];
        for(int pixel=0;pixel<icon->width*icon->height;pixel++)if(icon->pixels[pixel])opaque++;else zero++;
        transparent_expected(expected,icon,201+26*(int)i,169);
    }
    CHECK(zero && opaque && prepare_service_picker_draw(&ui)==0 && !memcmp(vga->framebuffer,expected,VGA_W*VGA_H));
    controls(vga);service_capture_first=1;memset(vga->framebuffer,0x61,VGA_W*VGA_H);memcpy(expected,vga->framebuffer,VGA_W*VGA_H);
    transparent_expected(expected,&a->commands.frames[49],244,168);
    transparent_expected(expected,&a->commands.frames[51],252,168);
    pulse(4,SDL_SCANCODE_RETURN);unsigned port=99;
    CHECK(prepare_service_confirm(&ui,&port)==0 && port==FD2_FLOW_YES &&
        !memcmp(service_first_frame,expected,VGA_W*VGA_H));
    free(expected);free(context);return 0;
}

int main(void) {
    CHECK(default_sound_gain() == 0);
    fd2_archive fdother = {0}; fd2_field_prepare_assets *assets = calloc(1, sizeof(*assets));
    fd2_vga *vga = calloc(1, sizeof(*vga)); fd2_save_manual_state state;
    CHECK(assets && vga && initial_state(&state) == 0);
    CHECK(fd2_archive_open(&fdother, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_field_prepare_assets_open(assets, &fdother) == 0);
    CHECK(service_resource_pages(&fdother, assets, &state, vga) == 0);
    CHECK(service_present_leaves(&fdother,assets,&state,vga)==0);
    CHECK(service_raw_icon_transparency(&fdother,assets,&state,vga)==0);
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
    fd2_field_prepare_assets_close(assets); fd2_archive_close(&fdother); free(assets); free(vga);
    puts("field_prepare_play: real assets/pure draw/32-roster scroll/title cancel-load/save reopen/town hotel depart/quit passed (no GUI)");
    return 0;
}
