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
static unsigned frame_number, quit_frame;
static struct { unsigned frame; SDL_Scancode key; } pulses[20];
static size_t pulse_count;
static int music_tracks[20];
static size_t music_stages[20], music_count;
static unsigned fail_frame_allocation_at, sound_calls, fail_sound_call;
static float sound_last_gain;
static unsigned magic_draw_calls, fail_magic_draw_call, state_save_calls;
static int probe_title, title_background_ok, title_palette_ok;
static uint8_t expected_title_palette[VGA_PALETTE_SIZE];

static void *prepare_test_calloc(size_t count, size_t size) {
    if (count == 1 && size == sizeof(fd2_vga) && fail_frame_allocation_at &&
        frame_number >= fail_frame_allocation_at) return NULL;
    return calloc(count, size);
}
static int prepare_test_pcm_replace(fd2_pcm_player *player, const fd2_pcm_bank *bank,
        size_t sample, int loop_count, float gain) {
    (void)player; (void)bank; (void)sample; (void)loop_count;
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
    memset(vga->input.test_key_state, 0, sizeof(vga->input.test_key_state));
    for (size_t i = 0; i < pulse_count; i++) if (pulses[i].frame == frame_number)
        fd2_input_set_test_key_state(&vga->input, pulses[i].key, 1);
    fd2_input_begin_test_frame(&vga->input, clock_ms);
    if (probe_title && frame_number == 70) {
        title_background_ok = 1;
        for (size_t i = 0; i < 112u * 320u; i++) if (vga->framebuffer[i] != 0) title_background_ok = 0;
        title_palette_ok = memcmp(vga->palette, expected_title_palette, VGA_PALETTE_SIZE) == 0;
        for (size_t i = 0; i < VGA_PALETTE_SIZE; i++)
            if (vga->dac[i] != (uint8_t)((expected_title_palette[i] << 2) | (expected_title_palette[i] >> 4))) title_palette_ok = 0;
    }
    if ((quit_frame && frame_number == quit_frame) || frame_number > 150)
        vga->input.quit_requested = 1;
}
void prepare_test_present_timed(fd2_vga *vga, uint32_t delay) {
    (void)delay; prepare_test_present(vga);
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
    magic_draw_calls = fail_magic_draw_call = state_save_calls = 0;
    probe_title = title_background_ok = title_palette_ok = 0;
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
    CHECK(fd2_field_prepare_draw_town(vga, a, state, 5, 0) == -1);
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

static int town_roundtrip(fd2_archive *fdother, const fd2_save_manual_state *state, fd2_vga *vga) {
    fd2_field_play_context context = {0}; context.initialized = context.has_preparation = 1;
    context.preparation = *state; context.music_enabled=state->options[2]!=0;context.sfx_enabled=state->options[3]!=0; context.experience_value = 42; context.message_number = 7;
    controls(vga);
    pulse(1, SDL_SCANCODE_RETURN); /* town0 ->hotel */
    pulse(6, SDL_SCANCODE_ESCAPE); /* hotel ->town */
    pulse(12, SDL_SCANCODE_LEFT); pulse(14, SDL_SCANCODE_LEFT);
    pulse(15, SDL_SCANCODE_RETURN); pulse(20, SDL_SCANCODE_RETURN);
    CHECK(fd2_field_prepare_run(vga, fdother, &context, "unused.SAV", NULL) == FD2_FIELD_PREPARE_DEPART);
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
    CHECK(music_count == 3 && music_tracks[2] == 10); /* 确认Yes后人数门才回城。 */
    context.preparation = *state; context.music_enabled=state->options[2]!=0;context.sfx_enabled=state->options[3]!=0;
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
    uint8_t before[VGA_W * VGA_H];
    memset(vga->framebuffer, 0x63, sizeof(vga->framebuffer)); memcpy(before, vga->framebuffer, sizeof(before));
    controls(vga); pulse(12, SDL_SCANCODE_ESCAPE);
    CHECK(member_detail(&ui, 0) == 0);
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
        if (fault < 4) fail_sound_call = fault < 2 ? 2u : fault + 1u;
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
    pulse(4,SDL_SCANCODE_RETURN); pulse(10,SDL_SCANCODE_RETURN); quit_frame = 16;
    CHECK(fd2_field_prepare_run(vga,fdother,&context,"unused.SAV",NULL) == FD2_FIELD_PREPARE_DEPART);
    fd2_field_play_context expected_depart = before;
    (void)fd2_field_rng_next(&expected_depart.rng); /* choice新等待入口的唯一一取；关闭段不再取。 */
    CHECK(expected_depart.rng.state == 0x99aeu);
    printf("stage2 depart RNG: before=%04x after=%04x expected=%04x\n",
        before.rng.state, context.rng.state, expected_depart.rng.state);
    CHECK(!memcmp(&context,&expected_depart,sizeof(context))); /* 仅已证等待RNG变化，无owner交接。 */
    CHECK(music_count == 2 && music_tracks[0] == 10 && music_tracks[1] == -1 &&
          music_stages[0] == 2 && music_stages[1] == 2);

    controls(vga); context.preparation.stage_id = 3; before = context;
    fd2_vga unchanged = *vga;
    CHECK(fd2_field_prepare_run(vga,fdother,&context,"unused.SAV",NULL) == FD2_FIELD_PREPARE_ERROR);
    CHECK(!frame_number && !music_count && !memcmp(&context,&before,sizeof(context)) &&
          !memcmp(vga,&unchanged,sizeof(*vga)));
    context.preparation = state; context.music_enabled=state.options[2]!=0;context.sfx_enabled=state.options[3]!=0; context.preparation.unit_area[7] = 255; before = context;
    controls(vga); unchanged = *vga;
    CHECK(fd2_field_prepare_run(vga,fdother,&context,"unused.SAV",NULL) == FD2_FIELD_PREPARE_ERROR);
    CHECK(!frame_number && !music_count && !memcmp(&context,&before,sizeof(context)) &&
          !memcmp(vga,&unchanged,sizeof(*vga)));

    CHECK(town_type(1) == 0 && town_type(2) == 2 && town_type(3) == -1);
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

int main(void) {
    CHECK(default_sound_gain() == 0);
    fd2_archive fdother = {0}; fd2_field_prepare_assets *assets = calloc(1, sizeof(*assets));
    fd2_vga *vga = calloc(1, sizeof(*vga)); fd2_save_manual_state state;
    CHECK(assets && vga && initial_state(&state) == 0);
    CHECK(fd2_archive_open(&fdother, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_field_prepare_assets_open(assets, &fdother) == 0);
    CHECK(renderer_test(&fdother, assets, &state, vga) == 0);
    CHECK(selector_test() == 0);
    CHECK(title_cancel_and_load(&fdother, &state, vga) == 0);
    CHECK(hotel_save_reopen(&fdother, assets, &state, vga) == 0);
    CHECK(hotel_load_keeps_sequence(&fdother, assets, &state, vga) == 0);
    CHECK(town_roundtrip(&fdother, &state, vga) == 0);
    CHECK(failure_and_cancel(&fdother, assets, &state, vga) == 0);
    CHECK(depart_capacity_and_leave(&fdother, assets, &state, vga) == 0);
    CHECK(member_pages(&fdother, assets, &state, vga) == 0);
    CHECK(late_failures(&fdother, assets, &state, vga) == 0);
    CHECK(stage2_preparation_routes(&fdother, assets, &state, vga) == 0);
    fd2_field_prepare_assets_close(assets); fd2_archive_close(&fdother); free(assets); free(vga);
    puts("field_prepare_play: real assets/pure draw/32-roster scroll/title cancel-load/save reopen/town hotel depart/quit passed (no GUI)");
    return 0;
}
