/* field_player_magic_command_execute @0x1cff0（原登记同名）与
 * field_unit_inventory_select @0x1b932（原sub_1B932）的私有modal无窗口
 * 回归；单键步骤@0x1d51d／@0x1b9de、共享RNG等待@0x16c57。
 * 用真实DATO／字体／列表资源和正式core step，替换宿主时钟／呈现。
 * 验证列表到目标／COMMAND的返回编排，不提交施法，不作为原版运行对照。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "field_play.h"
#include "field_rng.h"
#include "portrait.h"
#include "scene.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

typedef struct {
    int frame;
    SDL_Scancode key, second;
} key_frame;
static struct {
    uint64_t now, rng_time[32];
    unsigned interval;
    int frames, presents, waits, render_calls, water_calls, sound_calls, command_calls;
    int command_quit, command_error;
    int closing, quit_frame, quit_final, deadline;
    int allocations, live_buffers, fail_alloc, decode_calls, live_portraits;
    int real_render, panel_after_render;
    fd2_vga *quit_during_render;
    int fail_decode, wrong_portrait_size, step_error;
    int steps, indices[32], statuses[32], rng_calls, bad_cue;
    int closing_start, bad_transition;
    fd2_field_magic_list_key keys[32];
    fd2_field_item_list_key item_keys[32];
    int portrait_at_frame[128];
    key_frame keys_at[16];
    size_t key_count;
    void *portrait_pixels[2];
} controls;
static fd2_image reference_portraits[2];
static fd2_vga reference_page;
static uint8_t reference_background[VGA_W * VGA_H];

static uint32_t modal_rng(void *userdata) {
    (void)userdata;
    if (controls.rng_calls < 32)
        controls.rng_time[controls.rng_calls] = controls.now;
    controls.rng_calls++;
    return 0; /* 原版counter=2：第三个110ms步画3，第四步画0并再消费。 */
}

static uint64_t modal_ticks(void) { return controls.now; }
static void modal_wait(fd2_vga *vga) {
    controls.waits++;
    vga->frame_deadline_ns = vga->frame_interval_ns = 0;
}
static void *modal_malloc(size_t size) {
    controls.allocations++;
    if (controls.allocations == controls.fail_alloc) return NULL;
    void *memory = malloc(size);
    if (memory) controls.live_buffers++;
    return memory;
}
static void modal_free(void *memory) {
    if (memory) controls.live_buffers--;
    free(memory);
}
static int modal_decode(fd2_image *image, const fd2_archive *archive,
                          size_t id, size_t frame) {
    controls.decode_calls++;
    if (controls.decode_calls == controls.fail_decode) return -1;
    int result = fd2_portrait_decode_frame(image, archive, id, frame);
    if (result == 0) {
        controls.portrait_pixels[frame == 3] = image->pixels;
        controls.live_portraits++;
        if (controls.wrong_portrait_size) image->width = 79;
    }
    return result;
}
static void modal_image_free(fd2_image *image) {
    for (size_t i = 0; i < 2; i++) {
        if (image->pixels && image->pixels == controls.portrait_pixels[i]) {
            controls.live_portraits--;
            controls.portrait_pixels[i] = NULL;
        }
    }
    fd2_image_free(image);
}
static void modal_render(fd2_field_game *game, fd2_vga *vga) {
    controls.render_calls++;
    if (controls.real_render)
        fd2_field_game_render(game, vga);
    else
        memset(vga->framebuffer, 0x33, sizeof(vga->framebuffer));
    controls.panel_after_render = game->info_panel_right;
    memcpy(reference_background, vga->framebuffer, sizeof(reference_background));
    memset(&reference_page, 0, sizeof(reference_page));
    memcpy(reference_page.framebuffer, reference_background, sizeof(reference_background));
    fd2_field_detail_draw(&reference_page, &game->info_assets, &game->font,
        &game->ui_text, &game->units.items[game->selected_unit], &reference_portraits[0]);
    if (game->interaction == FD2_FIELD_INTERACTION_ITEM_LIST)
        (void)fd2_field_item_list_draw(&reference_page, &game->info_assets, &game->font,
            &game->ui_text, &game->units.items[game->selected_unit], -1);
    else
        (void)fd2_field_magic_list_draw(&reference_page, &game->info_assets, &game->font,
            &game->ui_text, &game->units.items[game->selected_unit], -1);
    if (controls.quit_during_render) controls.quit_during_render->input.quit_requested = 1;
}
static int modal_water(fd2_vga *vga, uint64_t now) {
    (void)vga; (void)now;
    controls.water_calls++;
    return 0; /* 独立water owner已有回归；此处只检查无键等待调用边界。 */
}
static int modal_audio(fd2_field_audio *audio, fd2_field_sfx cue) {
    (void)audio;
    controls.sound_calls++;
    if (cue != FD2_FIELD_SFX_COMMAND_MENU) controls.bad_cue = 1;
    return 0;
}
static int modal_step(fd2_field_game *game, fd2_field_magic_list_key key,
                        uint8_t *id) {
    int result = controls.step_error && controls.steps > 0 ? -2 :
        fd2_field_game_magic_list_step(game, key, id);
    int index = controls.steps++;
    if (index < 32) {
        controls.keys[index] = key;
        controls.indices[index] = game->magic_list_selected;
        controls.statuses[index] = result;
    }
    if (result == -1 || result == 1) {
        controls.closing = 1;
        controls.closing_start = controls.frames;
    }
    return result;
}
static int modal_item_step(fd2_field_game *game, fd2_field_item_list_key key) {
    int result = controls.step_error && controls.steps > 0 ? -2 :
        fd2_field_game_item_list_step(game, key);
    int index = controls.steps++;
    if (index < 32) {
        controls.item_keys[index] = key;
        controls.indices[index] = game->item_list_selected;
        controls.statuses[index] = result;
    }
    if (result == -1 || result == 1) {
        controls.closing = 1;
        controls.closing_start = controls.frames;
    }
    return result;
}
static int modal_command(fd2_field_game *game, fd2_vga *vga,
                          int opening, uint32_t interval) {
    controls.command_calls++;
    if (controls.command_quit) vga->input.quit_requested = 1;
    if (controls.command_error) return -1;
    return game->interaction == FD2_FIELD_INTERACTION_COMMAND && opening == 1 &&
        interval == 16u ? 0 : -1;
}
static void modal_frame_input(fd2_vga *vga, int frame) {
    for (int key = 1; key < SDL_SCANCODE_COUNT; key++)
        fd2_input_set_test_key_state(&vga->input, (SDL_Scancode)key, 0);
    /* 开／关动画中的键必须耗尽，不落入LIST或调用者。 */
    if (frame == 3 || (controls.closing && frame % 2 == 0) || frame == -1) {
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
    }
    if (!controls.closing) {
        for (size_t i = 0; i < controls.key_count; i++) {
            if (controls.keys_at[i].frame != frame) continue;
            fd2_input_set_test_key_state(&vga->input, controls.keys_at[i].key, 1);
            if (controls.keys_at[i].second)
                fd2_input_set_test_key_state(&vga->input, controls.keys_at[i].second, 1);
        }
    }
    fd2_input_begin_test_frame(&vga->input, controls.now);
    if ((controls.quit_frame && frame == controls.quit_frame) ||
        (controls.quit_final && frame == -1)) vga->input.quit_requested = 1;
}
static int matches_portrait(const fd2_vga *vga, const fd2_image *portrait) {
    for (int y = 0; y < 80; y++) {
        if (memcmp(vga->framebuffer + (10 + y) * VGA_STRIDE + 8,
                    portrait->pixels + y * 80, 80) != 0) return 0;
    }
    return 1;
}
static void modal_present_timed(fd2_vga *vga, uint32_t interval) {
    controls.frames++;
    if (controls.frames <= 12 || controls.closing) {
        uint8_t expected[VGA_W * VGA_H];
        int phase = controls.closing ? controls.frames - controls.closing_start - 1 :
                                      12 - controls.frames;
        fd2_field_detail_transition_frame(expected, reference_page.framebuffer,
            reference_background, phase);
        if (memcmp(expected, vga->framebuffer, sizeof(expected)) != 0)
            controls.bad_transition = 1; /* 关闭必须仍取最初未选中的缓存页。 */
    }
    if (controls.frames > 12 && !controls.closing && controls.frames < 128) {
        controls.portrait_at_frame[controls.frames] =
            matches_portrait(vga, &reference_portraits[0]) ? 1 :
            matches_portrait(vga, &reference_portraits[1]) ? 3 : -1;
    }
    controls.now += controls.interval ? controls.interval : interval;
    vga->frame_deadline_ns = controls.now * UINT64_C(1000000);
    vga->frame_interval_ns = (uint64_t)interval * UINT64_C(1000000);
    modal_frame_input(vga, controls.frames);
    if (controls.frames > 120) {
        controls.deadline = 1;
        vga->input.quit_requested = 1;
    }
}
static void modal_present(fd2_vga *vga) {
    controls.presents++;
    modal_frame_input(vga, -1);
}

#define malloc modal_malloc
#define free modal_free
#define fd2_portrait_decode_frame modal_decode
#define fd2_image_free modal_image_free
#define fd2_field_game_render modal_render
#define fd2_field_game_magic_list_step modal_step
#define fd2_field_game_item_list_step modal_item_step
#define fd2_field_game_animate_command modal_command
#define fd2_field_audio_play modal_audio
#define fd2_field_water_tick modal_water
#define SDL_GetTicks modal_ticks
#define fd2_vga_present modal_present
#define fd2_vga_present_timed modal_present_timed
#define fd2_vga_wait_frame_deadline modal_wait
#include "field_play.c"
#undef malloc
#undef free
#undef fd2_portrait_decode_frame
#undef fd2_image_free
#undef fd2_field_game_render
#undef fd2_field_game_magic_list_step
#undef fd2_field_game_item_list_step
#undef fd2_field_game_animate_command
#undef fd2_field_audio_play
#undef fd2_field_water_tick
#undef SDL_GetTicks
#undef fd2_vga_present
#undef fd2_vga_present_timed
#undef fd2_vga_wait_frame_deadline

static fd2_archive other, text_archive;
static fd2_field_game fixture;

static void set_magic(fd2_field_unit *unit, unsigned count) {
    memset(unit->data_09_1e + 0x11, 0, 5);
    for (unsigned id = 0; id < count; id++)
        unit->data_09_1e[0x11u + id / 8u] |= (uint8_t)(1u << (id % 8u));
}
static int open_fixture(void) {
    CHECK(fd2_archive_open(&other, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_archive_open(&text_archive, "original_game/FDTXT.DAT") == 0);
    CHECK(fd2_archive_open(&fixture.dato_archive, "original_game/DATO.DAT") == 0);
    CHECK(fd2_field_info_assets_open(&fixture.info_assets, &other) == 0);
    CHECK(fd2_font_open_fdother(&fixture.font, &other, 4) == 0);
    CHECK(fd2_text_entry_open_entry(&fixture.ui_text, &text_archive, 0) == 0);
    CHECK(fd2_portrait_decode_frame(&reference_portraits[0],
        &fixture.dato_archive, 9, 0) == 0);
    CHECK(fd2_portrait_decode_frame(&reference_portraits[1],
        &fixture.dato_archive, 9, 3) == 0);
    CHECK(memcmp(reference_portraits[0].pixels,
                 reference_portraits[1].pixels, 6400) != 0);
    fixture.ready = 1;
    fixture.active_side = 2;
    fixture.selected_unit = 0;
    fixture.interaction = FD2_FIELD_INTERACTION_MAGIC_LIST;
    fixture.attack_rng = modal_rng;
    fixture.option_values[1] = 1;
    fixture.units.count = 1;
    CHECK(fd2_field_unit_init(&fixture.units.items[0], 9, 1, 2, 8, 15) == 0);
    fd2_field_unit *unit = &fixture.units.items[0];
    unit->hp = unit->hp_max = 50;
    unit->mp = unit->mp_max = 40;
    unit->movement_profile = 5;
    for (size_t i = 0; i < 8; i++) unit->data_09_1e[1u + i * 2u] = 0x80;
    set_magic(unit, 5);
    fixture.pending_cell_event.valid = 1;
    fixture.pending_cell_event.action = 7;
    fixture.pending_cell_event.source_slot = 3;
    fixture.move_origin_x = 7;
    fixture.move_origin_y = 14;
    fixture.move_origin_camera_x = 3;
    fixture.move_origin_camera_y = 10;
    fixture.camera_cell_x = 5;
    fixture.camera_cell_y = 11;
    fixture.cursor_cell_x = 8;
    fixture.cursor_cell_y = 15;
    return 0;
}
static void reset(fd2_field_game *game, fd2_vga *vga) {
    memset(&controls, 0, sizeof(controls));
    *game = fixture;
    memset(vga, 0, sizeof(*vga));
    memset(vga->framebuffer, 0xa5, sizeof(vga->framebuffer));
    memset(vga->palette, 31, sizeof(vga->palette));
    memset(vga->dac, 125, sizeof(vga->dac));
    fd2_input_init(&vga->input);
}
static void set_items(fd2_field_unit *unit, unsigned count) {
    for (unsigned slot = 0; slot < 8; slot++) {
        unit->data_09_1e[1u + slot * 2u] = slot < count ? 0 : 0x80;
        unit->data_09_1e[2u + slot * 2u] = 192; /* 已知可Use的药草。 */
    }
}
static int reset_item(fd2_field_game *game, fd2_vga *vga, unsigned count) {
    reset(game, vga);
    set_items(&game->units.items[0], count);
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    CHECK(fd2_field_game_begin_item_menu(game) == 0);
    CHECK(fd2_field_game_confirm_item_menu(game) == 0);
    return 0;
}
static int unchanged_game(const fd2_field_game *game, const fd2_field_game *before) {
    fd2_field_game copy = *game;
    copy.magic_list_selected = before->magic_list_selected;
    copy.item_list_selected = before->item_list_selected;
    return memcmp(&copy, before, sizeof(copy)) == 0;
}
static int released(void) {
    CHECK(controls.live_buffers == 0 && controls.live_portraits == 0);
    CHECK(!controls.deadline && !controls.bad_cue && !controls.bad_transition);
    return 0;
}
static int test_navigation_and_cancel(void) {
    fd2_field_game game;
    fd2_vga vga;
    reset(&game, &vga);
    controls.keys_at[0] = (key_frame){13, SDL_SCANCODE_RIGHT, 0};
    controls.keys_at[1] = (key_frame){15, SDL_SCANCODE_RIGHT, 0};
    controls.keys_at[2] = (key_frame){17, SDL_SCANCODE_DOWN, 0};
    controls.keys_at[3] = (key_frame){19, SDL_SCANCODE_UP, 0};
    controls.keys_at[4] = (key_frame){21, SDL_SCANCODE_LEFT, 0};
    controls.keys_at[5] = (key_frame){23, SDL_SCANCODE_ESCAPE, 0};
    controls.key_count = 6;
    uint8_t id = 0xa5;
    fd2_field_audio audio = {0};
    fd2_field_game before = game;
    CHECK(play_field_magic_list(&game, &vga, &audio, &id) == FIELD_LIST_CANCELLED);
    static const int expected[] = {4,4,0,4,0,0};
    CHECK(controls.steps == 7 && controls.rng_calls == 6);
    CHECK(controls.keys[0] == FD2_FIELD_MAGIC_LIST_KEY_OTHER);
    CHECK(memcmp(controls.indices + 1, expected, sizeof(expected)) == 0);
    CHECK(controls.sound_calls == 4 && id == 0xa5);
    CHECK(controls.frames == 35 && controls.presents == 1 && controls.waits == 1);
    CHECK(controls.render_calls == 1 && controls.water_calls > 0);
    CHECK(unchanged_game(&game, &before));
    CHECK(!fd2_input_has_any_key(&vga.input));
    CHECK(vga.input.previous_down[SDL_SCANCODE_RIGHT]);
    CHECK(vga.input.next_repeat_ms[SDL_SCANCODE_RIGHT] > 0);
    CHECK(vga.frame_deadline_ns == 0 && vga.frame_interval_ns == 0);
    for (size_t i = 0; i < sizeof(vga.framebuffer); i++) CHECK(vga.framebuffer[i] == 0x33);
    for (size_t i = 0; i < sizeof(vga.palette); i++)
        CHECK(vga.palette[i] == 31 && vga.dac[i] == 125);
    return released();
}
static int test_single_item_navigation_sound(void) {
    for (int enabled = 0; enabled <= 1; enabled++) {
        fd2_field_game game;
        fd2_vga vga;
        reset(&game, &vga);
        set_magic(&game.units.items[0], 1);
        game.option_values[1] = (uint8_t)enabled;
        controls.keys_at[0] = (key_frame){13, SDL_SCANCODE_UP, 0};
        controls.keys_at[1] = (key_frame){15, SDL_SCANCODE_DOWN, 0};
        controls.keys_at[2] = (key_frame){17, SDL_SCANCODE_LEFT, 0};
        controls.keys_at[3] = (key_frame){19, SDL_SCANCODE_RIGHT, 0};
        controls.keys_at[4] = (key_frame){21, SDL_SCANCODE_ESCAPE, 0};
        controls.key_count = 5;
        fd2_field_audio audio = {0};
        uint8_t id = 0xa5;
        CHECK(play_field_magic_list(&game, &vga, &audio, &id) == FIELD_LIST_CANCELLED);
        CHECK(controls.sound_calls == enabled * 2 && game.magic_list_selected == 0);
        CHECK(controls.steps == 6 && controls.rng_calls == 5 && id == 0xa5);
        CHECK(released() == 0);
    }
    return 0;
}
static int test_confirm_low_mp_and_other(void) {
    const SDL_Scancode confirms[] = {SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE, SDL_SCANCODE_KP_ENTER};
    for (size_t i = 0; i < sizeof(confirms) / sizeof(confirms[0]); i++) {
        fd2_field_game game;
        fd2_vga vga;
        reset(&game, &vga);
        controls.keys_at[0] = (key_frame){13, confirms[i], 0};
        controls.key_count = 1;
        uint8_t id = 0xa5;
        fd2_field_game before = game;
        CHECK(play_field_magic_list(&game, &vga, NULL, &id) == FIELD_LIST_CONFIRMED);
        CHECK(id == 0 && controls.rng_calls == 1 && controls.steps == 2);
        CHECK(game.units.items[0].mp == 40 && unchanged_game(&game, &before));
        CHECK(released() == 0);
    }
    fd2_field_game game;
    fd2_vga vga;
    reset(&game, &vga);
    game.units.items[0].mp = 1; /* ID0真费用2，只拒确认，不关闭或扣费。 */
    controls.keys_at[0] = (key_frame){13, SDL_SCANCODE_RETURN, 0};
    controls.keys_at[1] = (key_frame){15, SDL_SCANCODE_A, SDL_SCANCODE_DOWN};
    controls.keys_at[2] = (key_frame){17, SDL_SCANCODE_DELETE, 0};
    controls.key_count = 3;
    uint8_t id = 0xa5;
    CHECK(play_field_magic_list(&game, &vga, NULL, &id) == FIELD_LIST_CANCELLED);
    CHECK(controls.steps == 5 && controls.rng_calls == 4 && id == 0xa5);
    CHECK(controls.statuses[1] == 0 && controls.statuses[4] == -1);
    CHECK(controls.keys[2] == FD2_FIELD_MAGIC_LIST_KEY_OTHER);
    CHECK(controls.keys[3] == FD2_FIELD_MAGIC_LIST_KEY_DOWN);
    CHECK(game.units.items[0].mp == 1 && game.magic_list_selected == 1);
    return released();
}
static int test_blink_wait_boundaries(void) {
    fd2_field_game game;
    fd2_vga vga;
    reset(&game, &vga);
    controls.keys_at[0] = (key_frame){45, SDL_SCANCODE_ESCAPE, 0};
    controls.key_count = 1;
    uint8_t id = 0xa5;
    CHECK(play_field_magic_list(&game, &vga, NULL, &id) == FIELD_LIST_CANCELLED);
    CHECK(controls.rng_calls == 2 && controls.rng_time[0] == 192 && controls.rng_time[1] == 640);
    CHECK(controls.portrait_at_frame[33] == 1 && controls.portrait_at_frame[34] == 3);
    CHECK(controls.portrait_at_frame[40] == 3 && controls.portrait_at_frame[41] == 1);
    CHECK(released() == 0);
    reset(&game, &vga);
    controls.keys_at[0] = (key_frame){35, SDL_SCANCODE_A, 0};
    controls.keys_at[1] = (key_frame){66, SDL_SCANCODE_ESCAPE, 0};
    controls.key_count = 2;
    CHECK(play_field_magic_list(&game, &vga, NULL, &id) == FIELD_LIST_CANCELLED);
    CHECK(controls.rng_calls == 3 && controls.rng_time[1] == 560 && controls.rng_time[2] == 1008);
    CHECK(controls.portrait_at_frame[36] == 3 && controls.portrait_at_frame[63] == 3);
    CHECK(controls.portrait_at_frame[64] == 1);
    CHECK(released() == 0);
    reset(&game, &vga);
    controls.interval = 1000; /* 每次只推进一个110ms步骤，不追赶10步。 */
    controls.keys_at[0] = (key_frame){16, SDL_SCANCODE_ESCAPE, 0};
    controls.key_count = 1;
    CHECK(play_field_magic_list(&game, &vga, NULL, &id) == FIELD_LIST_CANCELLED);
    CHECK(controls.rng_calls == 1 && controls.portrait_at_frame[16] == 3);
    return released();
}
static int test_quit_priority(void) {
    const int frames[] = {1,7,12,13,14,25};
    for (size_t i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        fd2_field_game game;
        fd2_vga vga;
        reset(&game, &vga);
        controls.keys_at[0] = (key_frame){13, SDL_SCANCODE_RETURN, 0};
        controls.key_count = 1;
        controls.quit_frame = frames[i];
        uint8_t id = 0xa5;
        fd2_field_game before = game;
        CHECK(play_field_magic_list(&game, &vga, NULL, &id) == FIELD_LIST_HOST_QUIT);
        CHECK(id == 0xa5 && unchanged_game(&game, &before));
        CHECK(controls.steps == (frames[i] > 13 ? 2 : 1));
        CHECK(controls.rng_calls == (frames[i] > 12 ? 1 : 0));
        CHECK(released() == 0);
    }
    fd2_field_game game;
    fd2_vga vga;
    reset(&game, &vga);
    vga.input.quit_requested = 1;
    uint8_t id = 0xa5;
    CHECK(play_field_magic_list(NULL, &vga, NULL, &id) == FIELD_LIST_HOST_QUIT);
    CHECK(!controls.allocations && !controls.decode_calls && !controls.rng_calls);
    reset(&game, &vga);
    controls.keys_at[0] = (key_frame){13, SDL_SCANCODE_RETURN, 0};
    controls.key_count = 1;
    controls.quit_final = 1;
    CHECK(play_field_magic_list(&game, &vga, NULL, &id) == FIELD_LIST_HOST_QUIT);
    CHECK(id == 0xa5 && controls.rng_calls == 1 && controls.presents == 1);
    return released();
}
static int test_resource_errors_and_no_side_effects(void) {
    for (int fault = 0; fault < 13; fault++) {
        fd2_field_game game;
        fd2_vga vga;
        reset(&game, &vga);
        fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 1);
        fd2_input_begin_test_frame(&vga.input, 0);
        switch (fault) {
            case 0: controls.fail_decode = 1; break;
            case 1: controls.fail_decode = 2; break;
            case 2: controls.fail_alloc = 1; break;
            case 3: controls.fail_alloc = 2; break;
            case 4: controls.wrong_portrait_size = 1; break;
            case 5: game.info_assets.detail_top.pixels = NULL; break;
            case 6: game.font.glyph_count = 1; break;
            case 7: game.info_assets.magic_cost.pixels = NULL; break;
            case 8: game.units.items[0].data_09_1e[0x15] |= 0x10; break; /* ID36 */
            case 9: set_magic(&game.units.items[0], 13); break;
            case 10: game.magic_list_selected = 5; break;
            case 11: set_magic(&game.units.items[0], 0); break;
            case 12: game.attack_rng = NULL; break;
        }
        fd2_vga before = vga;
        fd2_field_game game_before = game;
        uint8_t id = 0xa5;
        CHECK(play_field_magic_list(&game, &vga, NULL, &id) == FIELD_LIST_ERROR);
        CHECK(id == 0xa5 && controls.rng_calls == 0 && controls.steps == (fault == 12 ? 0 : 1));
        CHECK(controls.frames == 0 && controls.presents == 0);
        CHECK(memcmp(&vga, &before, sizeof(vga)) == 0);
        CHECK(memcmp(&game, &game_before, sizeof(game)) == 0);
        CHECK(released() == 0);
    }
    fd2_field_game game;
    fd2_vga vga;
    reset(&game, &vga);
    controls.step_error = 1;
    controls.keys_at[0] = (key_frame){13, SDL_SCANCODE_RETURN, 0};
    controls.key_count = 1;
    uint8_t id = 0xa5;
    CHECK(play_field_magic_list(&game, &vga, NULL, &id) == FIELD_LIST_ERROR);
    CHECK(id == 0xa5 && controls.rng_calls == 1 && controls.steps == 2);
    return released();
}
static int test_core_entry_rejections(void) {
    for (int fault = 0; fault < 8; fault++) {
        fd2_field_game game;
        fd2_vga vga;
        reset(&game, &vga);
        switch (fault) {
            case 0: game.units.items[0].flags |= FD2_FIELD_UNIT_FLAG_ACTED; break;
            case 1: game.units.items[0].detail_status[2] = 1; break;
            case 2: game.move_path_length = 1; break;
            case 3: game.active_side = 0; break;
            case 4: game.units.items[0].hp = 0; break;
            case 5: game.units.items[0].side = 0; break;
            case 6: game.units.items[0].flags |= FD2_FIELD_UNIT_FLAG_HIDDEN; break;
            case 7: game.player_post_waiting_phase = 1; break;
        }
        fd2_field_game before = game;
        fd2_vga vga_before = vga;
        uint8_t id = 0xa5;
        CHECK(play_field_magic_list(&game, &vga, NULL, &id) == FIELD_LIST_ERROR);
        CHECK(controls.steps == 1 && controls.keys[0] == FD2_FIELD_MAGIC_LIST_KEY_OTHER);
        CHECK(controls.statuses[0] == -2 && controls.rng_calls == 0);
        CHECK(!controls.allocations && !controls.decode_calls && !controls.render_calls);
        CHECK(id == 0xa5 && memcmp(&game, &before, sizeof(game)) == 0);
        CHECK(memcmp(&vga, &vga_before, sizeof(vga)) == 0);
        CHECK(released() == 0);
    }
    /* 末行名称坏字形不受有效上页／前四行掩盖；首个真键仍未消费。 */
    fd2_field_game game;
    fd2_vga vga;
    reset(&game, &vga);
    uint8_t *broken_text = malloc(game.ui_text.size);
    CHECK(broken_text != NULL);
    memcpy(broken_text, game.ui_text.data, game.ui_text.size);
    const uint8_t *tokens;
    size_t count;
    CHECK(fd2_text_entry_get_fragment(&game.ui_text, 445, &tokens, &count) == 0 && count > 1);
    size_t offset = (size_t)(tokens - game.ui_text.data);
    broken_text[offset] = 1;
    broken_text[offset + 1] = 0xff;
    game.ui_text.data = broken_text;
    fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&vga.input, 0);
    fd2_vga before = vga;
    uint8_t id = 0xa5;
    CHECK(play_field_magic_list(&game, &vga, NULL, &id) == FIELD_LIST_ERROR);
    CHECK(id == 0xa5 && controls.steps == 1 && !controls.rng_calls);
    CHECK(memcmp(&vga, &before, sizeof(vga)) == 0 && fd2_input_has_any_key(&vga.input));
    free(broken_text);
    return released();
}
static int test_selection_routes_target_and_command(void) {
    fd2_field_game game;
    fd2_vga vga;
    uint32_t cells[24 * 24] = {0};
    fd2_terrain_attr attr = {0};
    reset(&game, &vga);
    game.map.width = game.map.height = 24;
    game.map.cells = cells;
    game.terrain.attrs = &attr;
    game.terrain.attr_count = 1;
    set_magic(&game.units.items[0], 1);
    fd2_field_units before = game.units;
    controls.keys_at[0] = (key_frame){13, SDL_SCANCODE_RETURN, 0};
    controls.key_count = 1;
    CHECK(play_field_magic_selection(&game, &vga, NULL) == FD2_SCENE_RESULT_OK);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING);
    CHECK(!game.magic_target_count && !memcmp(&before, &game.units, sizeof(before)));
    CHECK(game.pending_cell_event.valid && controls.command_calls == 0);
    CHECK(fd2_field_game_cancel_magic_targeting(&game) == 0);
    CHECK(game.magic_list_selected == 0 && game.interaction == FD2_FIELD_INTERACTION_MAGIC_LIST);
    CHECK(released() == 0);
    memset(&controls, 0, sizeof(controls));
    controls.keys_at[0] = (key_frame){13, SDL_SCANCODE_ESCAPE, 0};
    controls.key_count = 1;
    CHECK(play_field_magic_selection(&game, &vga, NULL) == FD2_SCENE_RESULT_OK);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_COMMAND && controls.command_calls == 1);
    CHECK(!memcmp(&before, &game.units, sizeof(before)) && game.pending_cell_event.valid);
    CHECK(game.command_selected == FD2_FIELD_COMMAND_MAGIC);
    CHECK(!game.event_total_count && !game.player_post_waiting_phase);
    CHECK(released() == 0);
    for (int error = 0; error < 2; error++) {
        reset(&game, &vga);
        before = game.units;
        controls.keys_at[0] = (key_frame){13, SDL_SCANCODE_ESCAPE, 0};
        controls.key_count = 1;
        controls.command_quit = 1;
        controls.command_error = error;
        CHECK(play_field_magic_selection(&game, &vga, NULL) ==
            (error ? FD2_SCENE_RESULT_ERROR : FD2_SCENE_RESULT_HOST_QUIT));
        CHECK(game.interaction == FD2_FIELD_INTERACTION_COMMAND && controls.command_calls == 1);
        CHECK(!memcmp(&before, &game.units, sizeof(before)) && game.pending_cell_event.valid);
        CHECK(!game.event_total_count && !game.player_post_waiting_phase);
        CHECK(released() == 0);
    }
    reset(&game, &vga);
    controls.quit_frame = 1;
    CHECK(play_field_magic_selection(&game, &vga, NULL) == FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_MAGIC_LIST);
    CHECK(controls.command_calls == 0 && !controls.rng_calls);
    return released();
}

static int test_item_navigation_and_cancel_layers(void) {
    const unsigned counts[] = {1,4,5,8};
    for (size_t n = 0; n < sizeof(counts) / sizeof(counts[0]); n++) {
        for (int enabled = 0; enabled <= 1; enabled++) {
            fd2_field_game game;
            fd2_vga vga;
            CHECK(reset_item(&game, &vga, counts[n]) == 0);
            /* ITEM不借MAGIC的MP、silence或移动后禁法门。 */
            game.units.items[0].mp = 0;
            game.units.items[0].detail_status[2] = 1;
            game.move_path_length = 1;
            game.option_values[1] = (uint8_t)enabled;
            set_magic(&game.units.items[0], 0);
            game.info_assets.magic_cost.pixels = NULL;
            controls.keys_at[0] = (key_frame){13, SDL_SCANCODE_UP, 0};
            controls.keys_at[1] = (key_frame){15, SDL_SCANCODE_DOWN, 0};
            controls.keys_at[2] = (key_frame){17, SDL_SCANCODE_RIGHT, 0};
            controls.keys_at[3] = (key_frame){19, SDL_SCANCODE_LEFT, 0};
            controls.keys_at[4] = (key_frame){21, SDL_SCANCODE_ESCAPE, 0};
            controls.key_count = 5;
            fd2_field_game before = game;
            fd2_field_audio audio = {0};
            uint8_t slot = 0xa5;
            CHECK(play_field_item_list(&game, &vga, &audio, &slot) == FIELD_LIST_CANCELLED);
            int expected[] = {(int)counts[n] - 1,0,counts[n] > 4 ? 4 : 0,0,0};
            CHECK(controls.steps == 6 && controls.rng_calls == 5);
            CHECK(controls.item_keys[0] == FD2_FIELD_ITEM_LIST_KEY_OTHER);
            CHECK(memcmp(controls.indices + 1, expected, sizeof(expected)) == 0);
            CHECK(controls.sound_calls == enabled * (counts[n] > 4 ? 4 : 2));
            CHECK(slot == 0xa5 && unchanged_game(&game, &before));
            CHECK(game.interaction == FD2_FIELD_INTERACTION_ITEM_LIST);
            CHECK(!fd2_input_has_any_key(&vga.input));
            for (size_t i = 0; i < sizeof(vga.framebuffer); i++) CHECK(vga.framebuffer[i] == 0x33);
            for (size_t i = 0; i < sizeof(vga.palette); i++)
                CHECK(vga.palette[i] == 31 && vga.dac[i] == 125);
            CHECK(released() == 0);
            CHECK(fd2_field_game_end_item_list(&game) == 0);
            CHECK(game.interaction == FD2_FIELD_INTERACTION_ITEM_MENU && game.item_list_selected == 0);
            CHECK(game.command_selected == 0 && game.command_disabled[0] == 0);
            for (size_t i = 1; i < FD2_FIELD_COMMAND_COUNT; i++) CHECK(game.command_disabled[i] == 1);
            CHECK(fd2_field_game_end_item_menu(&game) == 0);
            CHECK(game.interaction == FD2_FIELD_INTERACTION_COMMAND);
            CHECK(game.command_disabled[FD2_FIELD_COMMAND_ITEM] == 1);
            CHECK(memcmp(&game.units, &before.units, sizeof(game.units)) == 0);
            CHECK(memcmp(&game.pending_cell_event, &before.pending_cell_event,
                         sizeof(game.pending_cell_event)) == 0);
            CHECK(game.move_path_length == 1 && game.move_path == before.move_path);
            CHECK(game.move_origin_x == before.move_origin_x && game.move_origin_y == before.move_origin_y);
            CHECK(game.camera_cell_x == before.camera_cell_x && game.camera_cell_y == before.camera_cell_y);
            CHECK(game.cursor_cell_x == before.cursor_cell_x && game.cursor_cell_y == before.cursor_cell_y);
            CHECK(game.move_origin_camera_x == before.move_origin_camera_x &&
                  game.move_origin_camera_y == before.move_origin_camera_y);
            CHECK(controls.rng_calls == 5 && !game.event_total_count && !game.player_post_waiting_phase);
        }
    }
    return 0;
}

static int test_item_use_confirmation_and_other(void) {
    const SDL_Scancode confirms[] = {SDL_SCANCODE_RETURN, SDL_SCANCODE_SPACE, SDL_SCANCODE_KP_ENTER};
    for (size_t i = 0; i < sizeof(confirms) / sizeof(confirms[0]); i++) {
        fd2_field_game game;
        fd2_vga vga;
        CHECK(reset_item(&game, &vga, 3) == 0);
        game.units.items[0].data_09_1e[2] = 0;   /* 剑：record+13=0。 */
        game.units.items[0].data_09_1e[4] = 132; /* 甲：record+13=0。 */
        controls.keys_at[0] = (key_frame){13, SDL_SCANCODE_RETURN, 0};
        controls.keys_at[1] = (key_frame){15, SDL_SCANCODE_DOWN, 0};
        controls.keys_at[2] = (key_frame){17, SDL_SCANCODE_SPACE, 0};
        controls.keys_at[3] = (key_frame){19, SDL_SCANCODE_DOWN, 0};
        controls.keys_at[4] = (key_frame){21, SDL_SCANCODE_A, 0};
        controls.keys_at[5] = (key_frame){23, confirms[i], 0};
        controls.key_count = 6;
        fd2_field_game before = game;
        uint8_t slot = 0xa5;
        CHECK(play_field_item_list(&game, &vga, NULL, &slot) == FIELD_LIST_CONFIRMED);
        CHECK(slot == 2 && game.item_list_selected == 2);
        CHECK(controls.steps == 7 && controls.rng_calls == 6);
        CHECK(controls.statuses[1] == 0 && controls.statuses[3] == 0 && controls.statuses[6] == 1);
        CHECK(controls.item_keys[5] == FD2_FIELD_ITEM_LIST_KEY_OTHER);
        CHECK(game.interaction == FD2_FIELD_INTERACTION_ITEM_LIST && unchanged_game(&game, &before));
        CHECK(controls.presents == 1 && !fd2_input_has_any_key(&vga.input));
        CHECK(released() == 0);
    }
    return 0;
}

static int test_item_blink_and_quit(void) {
    fd2_field_game game;
    fd2_vga vga;
    CHECK(reset_item(&game, &vga, 1) == 0);
    controls.keys_at[0] = (key_frame){35, SDL_SCANCODE_A, 0};
    controls.keys_at[1] = (key_frame){66, SDL_SCANCODE_DELETE, 0};
    controls.key_count = 2;
    uint8_t slot = 0xa5;
    CHECK(play_field_item_list(&game, &vga, NULL, &slot) == FIELD_LIST_CANCELLED);
    CHECK(slot == 0xa5 && controls.rng_calls == 3);
    CHECK(controls.rng_time[0] == 192 && controls.rng_time[1] == 560 && controls.rng_time[2] == 1008);
    CHECK(controls.portrait_at_frame[36] == 3 && controls.portrait_at_frame[63] == 3);
    CHECK(controls.portrait_at_frame[64] == 1 && controls.water_calls > 0);
    CHECK(released() == 0);
    const int frames[] = {1,7,12,13,14,25};
    for (size_t i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        CHECK(reset_item(&game, &vga, 1) == 0);
        controls.keys_at[0] = (key_frame){13, SDL_SCANCODE_RETURN, 0};
        controls.key_count = 1;
        controls.quit_frame = frames[i];
        fd2_field_game before = game;
        CHECK(play_field_item_list(&game, &vga, NULL, &slot) == FIELD_LIST_HOST_QUIT);
        CHECK(slot == 0xa5 && unchanged_game(&game, &before));
        CHECK(controls.steps == (frames[i] > 13 ? 2 : 1));
        CHECK(controls.rng_calls == (frames[i] > 12 ? 1 : 0));
        CHECK(released() == 0);
    }
    CHECK(reset_item(&game, &vga, 1) == 0);
    vga.input.quit_requested = 1;
    CHECK(play_field_item_list(NULL, &vga, NULL, &slot) == FIELD_LIST_HOST_QUIT);
    CHECK(!controls.allocations && !controls.decode_calls && !controls.steps && !controls.rng_calls);
    CHECK(reset_item(&game, &vga, 1) == 0);
    controls.keys_at[0] = (key_frame){13, SDL_SCANCODE_RETURN, 0};
    controls.key_count = 1;
    controls.quit_final = 1;
    CHECK(play_field_item_list(&game, &vga, NULL, &slot) == FIELD_LIST_HOST_QUIT);
    CHECK(slot == 0xa5 && controls.rng_calls == 1 && controls.presents == 1);
    return released();
}

static int test_item_errors_before_input(void) {
    for (int fault = 0; fault < 20; fault++) {
        fd2_field_game game;
        fd2_vga vga;
        CHECK(reset_item(&game, &vga, 8) == 0);
        uint8_t *broken_text = NULL;
        switch (fault) {
            case 0: controls.fail_decode = 1; break;
            case 1: controls.fail_decode = 2; break;
            case 2: controls.fail_alloc = 1; break;
            case 3: controls.fail_alloc = 2; break;
            case 4: controls.wrong_portrait_size = 1; break;
            case 5: game.info_assets.detail_top.pixels = NULL; break;
            case 6: game.info_assets.detail_bottom.pixels = NULL; break;
            case 7: game.info_assets.detail_icons[8].pixels = NULL; break;
            case 8: game.units.items[0].data_09_1e[16] = FD2_FIELD_ITEM_COUNT; break;
            case 9: game.units.items[0].data_09_1e[3] = 0x80; break; /* hole */
            case 10: game.item_list_selected = 8; break;
            case 11: set_items(&game.units.items[0], 0); break;
            case 12: game.attack_rng = NULL; break;
            case 13: game.units.items[0].flags |= FD2_FIELD_UNIT_FLAG_ACTED; break;
            case 14: game.units.items[0].flags |= FD2_FIELD_UNIT_FLAG_HIDDEN; break;
            case 15: game.units.items[0].hp = 0; break;
            case 16: game.active_side = 0; break;
            case 17: game.player_post_waiting_phase = 1; break;
            case 18: game.units.items[0].side = 0; break;
            case 19: {
                /* 最后raw7独立坏名称，前七槽和上页仍合法。 */
                game.units.items[0].data_09_1e[16] = 193;
                const uint8_t *tokens;
                size_t count;
                CHECK(fd2_text_entry_get_fragment(&game.ui_text, 181u + 193u, &tokens, &count) == 0 && count > 1);
                broken_text = malloc(game.ui_text.size);
                CHECK(broken_text != NULL);
                memcpy(broken_text, game.ui_text.data, game.ui_text.size);
                size_t offset = (size_t)(tokens - game.ui_text.data);
                broken_text[offset] = 1;
                broken_text[offset + 1] = 0xff;
                game.ui_text.data = broken_text;
                break;
            }
        }
        fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 1);
        fd2_input_begin_test_frame(&vga.input, 0);
        fd2_vga before = vga;
        fd2_field_game game_before = game;
        uint8_t slot = 0xa5;
        CHECK(play_field_item_list(&game, &vga, NULL, &slot) == FIELD_LIST_ERROR);
        CHECK(slot == 0xa5 && controls.rng_calls == 0 && controls.steps == (fault == 12 ? 0 : 1));
        CHECK(!controls.frames && !controls.presents && fd2_input_has_any_key(&vga.input));
        CHECK(memcmp(&vga, &before, sizeof(vga)) == 0 && memcmp(&game, &game_before, sizeof(game)) == 0);
        free(broken_text);
        CHECK(released() == 0);
    }
    fd2_field_game game;
    fd2_vga vga;
    CHECK(reset_item(&game, &vga, 1) == 0);
    controls.step_error = 1;
    controls.keys_at[0] = (key_frame){13, SDL_SCANCODE_RETURN, 0};
    controls.key_count = 1;
    uint8_t slot = 0xa5;
    CHECK(play_field_item_list(&game, &vga, NULL, &slot) == FIELD_LIST_ERROR);
    CHECK(slot == 0xa5 && controls.rng_calls == 1 && controls.steps == 2);
    return released();
}

static int test_real_renderer_preflight_panel_owner(void) {
    uint32_t cells[24 * 24] = {0};
    fd2_terrain_attr attr = {0};
    for (int item = 0; item <= 1; item++) {
        for (int initial_side = 0; initial_side <= 1; initial_side++) {
            /* 晚坏字模、预检quit、确认、取消、首呈现quit分别锁定边界。 */
            for (int outcome = 0; outcome < 5; outcome++) {
                fd2_field_game game;
                fd2_vga vga;
                if (item) CHECK(reset_item(&game, &vga, 8) == 0);
                else reset(&game, &vga);
                controls.real_render = 1;
                game.map.width = game.map.height = 24;
                game.map.cells = cells;
                game.terrain.attrs = &attr;
                game.terrain.attr_count = 1;
                game.info_panel_right = initial_side;
                game.cursor_cell_x = game.units.items[0].x = initial_side ? 15 : 6;
                game.cursor_cell_y = game.units.items[0].y = 17;
                /* camera(5,11)下的rel(1/10,6)各自触发真实换侧分支。 */
                uint8_t *broken_text = NULL;
                if (outcome == 0) {
                    size_t fragment = 445; /* MAGIC第五项，前四项／上页合法。 */
                    if (item) {
                        game.units.items[0].data_09_1e[16] = 193;
                        fragment = 181u + 193u; /* ITEM最后raw7独立名称。 */
                    }
                    const uint8_t *tokens;
                    size_t count;
                    CHECK(fd2_text_entry_get_fragment(&game.ui_text, fragment, &tokens, &count) == 0 && count > 1);
                    broken_text = malloc(game.ui_text.size);
                    CHECK(broken_text != NULL);
                    memcpy(broken_text, game.ui_text.data, game.ui_text.size);
                    size_t offset = (size_t)(tokens - game.ui_text.data);
                    broken_text[offset] = 1;
                    broken_text[offset + 1] = 0xff;
                    game.ui_text.data = broken_text;
                }
                if (outcome <= 1) {
                    fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 1);
                    fd2_input_begin_test_frame(&vga.input, 0);
                    if (outcome == 1) controls.quit_during_render = &vga;
                } else if (outcome == 4) {
                    controls.quit_frame = 1;
                } else {
                    controls.keys_at[0] = (key_frame){13,
                        outcome == 2 ? SDL_SCANCODE_RETURN : SDL_SCANCODE_ESCAPE, 0};
                    controls.key_count = 1;
                }
                fd2_field_game before = game;
                fd2_vga vga_before = vga;
                uint8_t selected = 0xa5;
                field_list_result result = item
                    ? play_field_item_list(&game, &vga, NULL, &selected)
                    : play_field_magic_list(&game, &vga, NULL, &selected);
                field_list_result expected = outcome == 0 ? FIELD_LIST_ERROR :
                    outcome == 2 ? FIELD_LIST_CONFIRMED :
                    outcome == 3 ? FIELD_LIST_CANCELLED : FIELD_LIST_HOST_QUIT;
                CHECK(result == expected);
                CHECK(controls.render_calls == 1 && controls.panel_after_render == !initial_side);
                CHECK(selected == (outcome == 2 ? 0 : 0xa5));
                if (outcome <= 1) {
                    /* 真renderer已换侧，晚错仍须恢复整个session与普通键。 */
                    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
                    if (outcome == 1) vga_before.input.quit_requested = 1;
                    CHECK(memcmp(&vga, &vga_before, sizeof(vga)) == 0);
                    CHECK(!controls.frames && !controls.presents && !controls.rng_calls);
                    CHECK(fd2_input_has_any_key(&vga.input));
                } else {
                    before.info_panel_right = !initial_side;
                    CHECK(unchanged_game(&game, &before));
                    CHECK(controls.frames > 0 && controls.rng_calls == (outcome == 4 ? 0 : 1));
                }
                free(broken_text);
                CHECK(released() == 0);
            }
        }
    }
    return 0;
}

int main(void) {
    CHECK(open_fixture() == 0);
    CHECK(test_navigation_and_cancel() == 0);
    CHECK(test_single_item_navigation_sound() == 0);
    CHECK(test_confirm_low_mp_and_other() == 0);
    CHECK(test_blink_wait_boundaries() == 0);
    CHECK(test_quit_priority() == 0);
    CHECK(test_resource_errors_and_no_side_effects() == 0);
    CHECK(test_core_entry_rejections() == 0);
    CHECK(test_selection_routes_target_and_command() == 0);
    CHECK(test_item_navigation_and_cancel_layers() == 0);
    CHECK(test_item_use_confirmation_and_other() == 0);
    CHECK(test_item_blink_and_quit() == 0);
    CHECK(test_item_errors_before_input() == 0);
    CHECK(test_real_renderer_preflight_panel_owner() == 0);
    fd2_image_free(&reference_portraits[1]);
    fd2_image_free(&reference_portraits[0]);
    fd2_field_info_assets_close(&fixture.info_assets);
    fd2_text_entry_close(&fixture.ui_text);
    fd2_archive_close(&fixture.dato_archive);
    fd2_archive_close(&text_archive);
    fd2_archive_close(&other);
    puts("field magic/item list play tests: ok");
    return 0;
}
