/* F1/PageUp纯绘制与modal无窗口回归；测试TU控制呈现，不新增生产hook。
 * 依据sub_2000A @0x2000a、field_view_render_scaled @0x1f558、
 * sub_1F6EF @0x1f6ef。原版probe02只提供原版运行证据，尚无SDL配对。 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "field_play.h"
#include "field_rng.h"
#include "scene.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

static uint32_t cells[24 * 24];
static fd2_image test_frames[288];
static fd2_terrain_attr attrs[288];
static uint8_t pixels[288][24 * 24];
static uint64_t clock_ms;
static int timed_frames, close_frame, quit_frame, restored, old_pulse;
static int render_calls, tick_calls, hold_f1, route_mode, outer_frames;
static int after_restore_queries, route_closed;
static int bad_geometry, bad_pulse;
static int water_calls, water_changes, bad_frozen_water, bad_restore_order;
static int frozen_water_valid;
static uint8_t frozen_dac[768], frozen_water_phase;
static uint16_t frozen_water_tick;
static uint64_t restore_water_ms;
static SDL_Scancode close_key;
static fd2_field_game route_before;
static fd2_field_rng test_rng;
static uint16_t route_rng_before;
static fd2_field_game *live_game;
static int check_scaled_frame(const fd2_field_game *, const fd2_vga *, int);

static void init_game(fd2_field_game *game) {
    memset(game, 0, sizeof(*game));
    game->ready = 1;
    game->active_side = 2;
    game->selected_unit = game->detail_unit = game->attack_target = -1;
    game->detail_acknowledged_unit = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->map = (fd2_field_map){24, 24, cells};
    game->terrain.sheet.width = game->terrain.sheet.height = 24;
    game->terrain.sheet.frame_count = 288;
    game->terrain.frames = test_frames;
    game->terrain.attrs = attrs;
    game->terrain.attr_count = 288;
    game->camera_cell_x = 5;
    game->camera_cell_y = 11;
    game->cursor_cell_x = 8;
    game->cursor_cell_y = 15;
    game->turn_number = 13;
    game->resource_value = 123456;
    game->idle_phase = 3;
    game->terrain_visual_phase = 1;
    game->last_idle_ms = game->last_terrain_visual_ms = clock_ms;
    game->units.count = 2;
    game->units.items[0] = (fd2_field_unit){.x=7,.y=14,.side=2,.hp=1};
    game->units.items[1] = (fd2_field_unit){.x=3,.y=4,.side=0,.hp=1};
    game->roster_count = 1;
    memset(game->roster, 0xa5, sizeof(game->roster));
    game->pending_cell_event.valid = 0;
    fd2_field_system_menu_init(&game->system_menu);
    fd2_field_rng_seed(&test_rng, 0x3141);
    game->attack_rng = fd2_field_rng_next;
    game->attack_rng_userdata = &test_rng;
    for (int y = 0; y < 24; y++)
        for (int x = 0; x < 24; x++)
            cells[y * 24 + x] = (uint32_t)((x * 7 + y * 13) % 288) | 0xf0000000u;
    for (size_t i = 0; i < 288; i++) {
        test_frames[i] = (fd2_image){24, 24, pixels[i]};
        attrs[i].flags = 0x1c; /* 非零动画phase会选不同帧；本页须原始ID。 */
        for (int y = 0; y < 24; y++)
            for (int x = 0; x < 24; x++)
                pixels[i][y * 24 + x] = (uint8_t)(i + y * 3 + x * 5);
    }
    live_game = game;
}

static void controls_reset(void) {
    clock_ms = 1000;
    timed_frames = restored = old_pulse = render_calls = tick_calls = 0;
    hold_f1 = route_mode = outer_frames = after_restore_queries = route_closed = 0;
    bad_geometry = bad_pulse = 0;
    water_calls = water_changes = bad_frozen_water = bad_restore_order = 0;
    frozen_water_valid = 0;
    restore_water_ms = 0;
    close_frame = 11;
    quit_frame = 0;
    close_key = SDL_SCANCODE_A;
    live_game = NULL;
}

static uint64_t auxiliary_ticks(void) { return clock_ms; }
static void auxiliary_wait(fd2_vga *vga) {
    vga->frame_deadline_ns = vga->frame_interval_ns = 0;
}

static void input_frame(fd2_vga *vga, int frame) {
    for (int key = 1; key < SDL_SCANCODE_COUNT; key++)
        fd2_input_set_test_key_state(&vga->input, (SDL_Scancode)key, 0);
    if (hold_f1) fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_F1, 1);
    if (frame == 3 || frame == close_frame + 2) {
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_DOWN, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_G, 1);
    }
    if (frame == close_frame) {
        fd2_input_set_test_key_state(&vga->input, close_key, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
    }
    fd2_input_begin_test_frame(&vga->input, clock_ms);
    if (frame == quit_frame && quit_frame > 0) vga->input.quit_requested = 1;
}

static void auxiliary_present_timed(fd2_vga *vga, uint32_t ms) {
    if (fd2_input_has_any_key(&vga->input)) old_pulse = 1;
    timed_frames++;
    if (!frozen_water_valid) {
        memcpy(frozen_dac, vga->dac, sizeof(frozen_dac));
        frozen_water_phase = vga->field_water_phase;
        frozen_water_tick = vga->field_water_last_tick;
        frozen_water_valid = 1;
    }
    if (memcmp(frozen_dac, vga->dac, sizeof(frozen_dac)) != 0 ||
        frozen_water_phase != vga->field_water_phase ||
        frozen_water_tick != vga->field_water_last_tick)
        bad_frozen_water = 1; /* 7开/稳定/6关的每次present前均检查。 */
    if (timed_frames <= 7 || timed_frames > close_frame) {
        int phase = timed_frames <= 7 ? timed_frames : close_frame + 7 - timed_frames;
        if (check_scaled_frame(live_game, vga, phase) != 0) bad_geometry = 1;
    } else {
        static const int phases[16] = {7,6,5,4,3,2,1,0,0,1,2,3,4,5,6,7};
        int expected = phases[((timed_frames - 8) / 7) % 16];
        int x = live_game->cursor_cell_x * 4 + 112;
        int y = live_game->cursor_cell_y * 4 + 52;
        if (vga->framebuffer[y * VGA_STRIDE + x] != 24 + expected) bad_pulse = 1;
    }
    clock_ms += ms;
    vga->frame_deadline_ns = clock_ms * 1000000u;
    vga->frame_interval_ns = (uint64_t)ms * 1000000u;
    input_frame(vga, timed_frames);
}

static void auxiliary_present(fd2_vga *vga) {
    if (route_mode && timed_frames == 0) {
        outer_frames++;
        clock_ms += 16;
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_F1, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
        fd2_input_begin_test_frame(&vga->input, clock_ms);
        route_before = *live_game;
        route_rng_before = ((fd2_field_rng *)live_game->attack_rng_userdata)->state;
    } else if (!restored) {
        restored = 1;
        clock_ms += 16;
        input_frame(vga, 0);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_DOWN, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
        fd2_input_begin_test_frame(&vga->input, clock_ms);
        if (quit_frame == -1) vga->input.quit_requested = 1;
    } else if (route_mode) {
        outer_frames++;
        vga->input.quit_requested = 1;
    }
    auxiliary_wait(vga);
}

static void auxiliary_render(fd2_field_game *game, fd2_vga *vga) {
    (void)game;
    render_calls++;
    if (timed_frames && !restored && water_calls == 0) bad_restore_order = 1;
    memset(vga->framebuffer, 0x99, sizeof(vga->framebuffer));
}
static void auxiliary_tick(fd2_field_game *game, uint64_t now) {
    (void)game; (void)now;
    tick_calls++;
}
static int auxiliary_open(fd2_field_game *game, fd2_vga *vga,
                            const fd2_archive *archive, size_t stage) {
    (void)vga; (void)archive; (void)stage;
    init_game(game);
    live_game = game;
    return 0;
}
static void auxiliary_close(fd2_field_game *game) {
    /* B6 Play先解除同步演出借用；其余Game与RNG仍逐字保持。 */
    int detached = !game->physical_present && !game->physical_present_userdata &&
        !game->magic_present && !game->magic_present_userdata;
    route_before.physical_present = NULL; route_before.physical_present_userdata = NULL;
    route_before.magic_present = NULL; route_before.magic_present_userdata = NULL;
    route_closed = detached && live_game == game &&
        memcmp(game, &route_before, sizeof(*game)) == 0 &&
        ((fd2_field_rng *)game->attack_rng_userdata)->state == route_rng_before;
}
static int auxiliary_take_action(fd2_input *input, fd2_input_context context,
                                   fd2_input_action *action) {
    if (restored && outer_frames == 1) after_restore_queries++;
    return fd2_input_take_action(input, context, action);
}

static int auxiliary_water_tick(fd2_vga *vga, uint64_t now) {
    water_calls++;
    if (timed_frames > 0 && !restored) {
        if (timed_frames != close_frame + 6) bad_restore_order = 1;
        restore_water_ms = now;
    }
    int result = fd2_field_water_tick(vga, now); /* 使用真实前置实现，不伪造色值。 */
    if (result == 1) water_changes++;
    return result;
}

#define fd2_field_water_tick auxiliary_water_tick
#define SDL_GetTicks auxiliary_ticks
#define fd2_vga_present auxiliary_present
#define fd2_vga_present_timed auxiliary_present_timed
#define fd2_vga_wait_frame_deadline auxiliary_wait
#define fd2_field_game_render auxiliary_render
#define fd2_field_game_tick auxiliary_tick
#define fd2_field_game_open auxiliary_open
#define fd2_field_game_close auxiliary_close
#define fd2_input_take_action auxiliary_take_action
#include "field_play.c"
#undef fd2_field_water_tick
#undef SDL_GetTicks
#undef fd2_vga_present
#undef fd2_vga_present_timed
#undef fd2_vga_wait_frame_deadline
#undef fd2_field_game_render
#undef fd2_field_game_tick
#undef fd2_field_game_open
#undef fd2_field_game_close
#undef fd2_input_take_action

static int floor_tile(int value) {
    return value >= 0 ? value / 3072 : -((-value + 3071) / 3072);
}

static int check_scaled_frame(const fd2_field_game *game, const fd2_vga *vga,
                                int phase) {
    int step = 128 + phase * 640 / 7;
    int start_x = game->camera_cell_x * 3072 + 13 * 1536;
    int start_y = game->camera_cell_y * 3072 + 8 * 1536;
    int cx = start_x + (24 * 1536 - start_x) * (phase + 1) / 8;
    int cy = start_y + (24 * 1536 - start_y) * (phase + 1) / 8;
    int checked_zero = 0, checked_negative = 0;
    for (int y = 0; y < 200; y++) {
        for (int x = 0; x < 320; x++) {
            uint8_t expected = 0;
            if (x >= 4 && x < 316 && y >= 4 && y < 196) {
                int wx = cx + (x - 160) * step;
                int wy = cy + (y - 100) * step;
                int tx = floor_tile(wx), ty = floor_tile(wy);
                if (wx < 0 || wy < 0) checked_negative++;
                if (tx >= 0 && tx < 24 && ty >= 0 && ty < 24) {
                    int px = (wx - tx * 3072) / 128;
                    int py = (wy - ty * 3072) / 128;
                    expected = (uint8_t)((tx * 7 + ty * 13) % 288 + py * 3 + px * 5);
                }
            }
            if (expected == 0) checked_zero++;
            CHECK(vga->framebuffer[y * VGA_STRIDE + x] == expected);
        }
    }
    CHECK(checked_zero > 0);
    if (phase == 7) CHECK(checked_negative > 0);
    return 0;
}

static int test_full_frame_and_transition(void) {
    fd2_field_game game;
    fd2_vga vga = {0};
    controls_reset(); init_game(&game);
    fd2_field_game before = game;
    uint8_t cache_before[sizeof(pixels)];
    memcpy(cache_before, pixels, sizeof(pixels));
    CHECK(auxiliary_field_ready(&game));
    for (int phase = 1; phase <= 7; phase++) {
        memset(vga.framebuffer, 0xcd, sizeof(vga.framebuffer));
        auxiliary_transition_frame(&game, &vga, phase);
        CHECK(check_scaled_frame(&game, &vga, phase) == 0);
    }
    for (int phase = 6; phase >= 1; phase--) {
        auxiliary_transition_frame(&game, &vga, phase);
        CHECK(check_scaled_frame(&game, &vga, phase) == 0);
    }
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    CHECK(memcmp(pixels, cache_before, sizeof(pixels)) == 0);
    return 0;
}

static int test_marker_filter_order_and_pulse(void) {
    fd2_field_game game;
    fd2_vga vga = {0};
    controls_reset(); init_game(&game);
    game.units.count = 7;
    game.units.items[0].flags = FD2_FIELD_UNIT_FLAG_ACTED;
    game.units.items[0].hp = 0; /* native不按HP/acted/AI可执行位过滤。 */
    game.units.items[1].flags = FD2_FIELD_UNIT_FLAG_AI_INELIGIBLE;
    game.units.items[2] = (fd2_field_unit){.x=1,.y=1,.side=1};
    game.units.items[3] = (fd2_field_unit){.x=2,.y=2,.side=255,.flags=1};
    game.units.items[4] = (fd2_field_unit){.x=3,.y=3,.side=255,.unit_id=121};
    game.units.items[5] = (fd2_field_unit){.x=4,.y=4,.side=255,.race=10};
    game.units.items[6] = (fd2_field_unit){.x=3,.y=4,.side=2};
    game.cursor_cell_x = 7; game.cursor_cell_y = 14;
    CHECK(auxiliary_field_ready(&game));
    for (int phase = 0; phase < 8; phase++) {
        memset(vga.framebuffer, 0xcd, sizeof(vga.framebuffer));
        auxiliary_markers(&game, &vga, phase);
        int changed = 0;
        for (int y = 0; y < 200; y++) {
            for (int x = 0; x < 320; x++) {
                uint8_t expected = 0xcd;
                if (x >= 140 && x < 143 && y >= 108 && y < 111) expected = (uint8_t)(24 + phase);
                if (x >= 124 && x < 127 && y >= 68 && y < 71) expected = (uint8_t)(72 + phase);
                if (x >= 116 && x < 119 && y >= 56 && y < 59) expected = (uint8_t)(80 + phase);
                CHECK(vga.framebuffer[y * VGA_STRIDE + x] == expected);
                if (expected != 0xcd) changed++;
            }
        }
        CHECK(changed == 27);
    }
    int phase = 7, direction = -1;
    static const int expected[] = {7,6,5,4,3,2,1,0,0,1,2,3,4,5,6,7,7,6};
    for (size_t i = 0; i < sizeof(expected)/sizeof(expected[0]); i++) {
        CHECK(phase == expected[i]);
        auxiliary_next_phase(&phase, &direction);
    }
    return 0;
}

static int test_modal_keys_owner_and_held(void) {
    const SDL_Scancode keys[] = {SDL_SCANCODE_A, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_F1, SDL_SCANCODE_PAGEUP, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_RETURN};
    for (size_t k = 0; k < sizeof(keys)/sizeof(keys[0]); k++) {
        fd2_field_game game;
        fd2_vga vga = {0};
        controls_reset(); init_game(&game);
        close_key = keys[k];
        hold_f1 = close_key != SDL_SCANCODE_F1;
        fd2_input_init(&vga.input);
        fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_F1, 1);
        fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RIGHT, 1);
        fd2_input_begin_test_frame(&vga.input, clock_ms);
        uint64_t repeat_at = vga.input.next_repeat_ms[SDL_SCANCODE_F1];
        memset(vga.palette, 11, sizeof(vga.palette));
        memset(vga.dac, 37, sizeof(vga.dac)); /* 非初始palette，不得重置水DAC。 */
        vga.field_water_phase = 7;
        vga.field_water_last_tick = (uint16_t)(clock_ms / 55u);
        fd2_field_game before = game;
        CHECK(play_field_auxiliary(&game, &vga) == FD2_SCENE_RESULT_OK);
        CHECK(timed_frames == close_frame + 6 && restored && render_calls == 1);
        CHECK(!old_pulse && !fd2_input_has_any_key(&vga.input));
        CHECK(!bad_geometry && !bad_pulse);
        CHECK(!tick_calls && test_rng.state == 0x3141);
        CHECK(memcmp(&game, &before, sizeof(game)) == 0);
        for (size_t p = 0; p < sizeof(vga.framebuffer); p++) CHECK(vga.framebuffer[p] == 0x99);
        for (size_t p = 0; p < sizeof(vga.palette); p++) CHECK(vga.palette[p] == 11);
        fd2_vga expected_dac = {0};
        memset(expected_dac.dac, 37, sizeof(expected_dac.dac));
        CHECK(fd2_field_water_palette_apply(&expected_dac, 8) == 0);
        CHECK(memcmp(vga.dac, expected_dac.dac, sizeof(vga.dac)) == 0);
        CHECK(!bad_frozen_water && !bad_restore_order);
        CHECK(water_calls == 1 && water_changes == 1);
        CHECK(vga.field_water_phase == 8 &&
              vga.field_water_last_tick == (uint16_t)(restore_water_ms / 55u));
        CHECK(!vga.frame_deadline_ns && !vga.frame_interval_ns);
        if (hold_f1) {
            CHECK(vga.input.previous_down[SDL_SCANCODE_F1]);
            CHECK(vga.input.next_repeat_ms[SDL_SCANCODE_F1] == repeat_at);
            fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_DOWN, 0);
            fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 0);
            fd2_input_begin_test_frame(&vga.input, repeat_at - 1);
            CHECK(!fd2_input_has_any_key(&vga.input));
            fd2_input_begin_test_frame(&vga.input, repeat_at);
            fd2_input_event event;
            CHECK(fd2_input_take_key(&vga.input, &event) && event.repeat && event.source == SDL_SCANCODE_F1);
        }
    }
    controls_reset();
    fd2_field_game game;
    fd2_vga vga = {0};
    init_game(&game);
    fd2_input_init(&vga.input);
    close_frame = 130; /* mock时钟跨过0/7的两个重复端点，不实际等待。 */
    vga.field_water_phase = 15;
    vga.field_water_last_tick = (uint16_t)(clock_ms / 55u);
    CHECK(play_field_auxiliary(&game, &vga) == FD2_SCENE_RESULT_OK);
    CHECK(!bad_geometry && !bad_pulse && timed_frames == 136);
    CHECK(!bad_frozen_water && !bad_restore_order);
    CHECK(water_calls == 1 && water_changes == 1 && vga.field_water_phase == 0);
    CHECK(vga.field_water_last_tick == (uint16_t)(restore_water_ms / 55u));
    return 0;
}

static int test_invalid_preflight_and_quit(void) {
    for (int mode = 0; mode < 15; mode++) {
        fd2_field_game game;
        fd2_vga vga = {0};
        controls_reset(); init_game(&game);
        fd2_input_init(&vga.input);
        memset(vga.framebuffer, 0x77, sizeof(vga.framebuffer));
        if (mode == 0) game.stage = 1;
        if (mode == 1) game.active_side = 0;
        if (mode == 2) game.selected_unit = 0;
        if (mode == 3) game.interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
        if (mode == 4) game.detail_visible = 1;
        if (mode == 5) game.failure = FD2_FIELD_FAILURE_EVENT_UNSUPPORTED;
        if (mode == 6) game.terrain.shape_index = 1;
        if (mode == 7) game.terrain.sheet.frame_count = 287;
        if (mode == 8) test_frames[0].pixels = NULL;
        if (mode == 9) game.cursor_cell_x = INT_MAX;
        if (mode == 10) game.camera_cell_y = INT_MIN;
        if (mode == 11) game.units.count = FD2_FIELD_MAX_UNITS + 1;
        if (mode == 12) game.units.items[0].side = 3;
        if (mode == 13) game.units.items[0].x = 24;
        if (mode == 14) game.player_post_waiting_phase = 1;
        fd2_field_game before = game;
        CHECK(play_field_auxiliary(&game, &vga) == FD2_SCENE_RESULT_ERROR);
        CHECK(!timed_frames && !restored && !render_calls && !tick_calls);
        CHECK(water_calls == 0 && water_changes == 0);
        CHECK(memcmp(&game, &before, sizeof(game)) == 0 && test_rng.state == 0x3141);
        for (size_t p = 0; p < sizeof(vga.framebuffer); p++) CHECK(vga.framebuffer[p] == 0x77);
    }
    const int quit_at[] = {1, 7, 8, 11, 13, 17, -1, 0};
    for (size_t q = 0; q < sizeof(quit_at)/sizeof(quit_at[0]); q++) {
        fd2_field_game game;
        fd2_vga vga = {0};
        controls_reset(); init_game(&game);
        fd2_input_init(&vga.input);
        quit_frame = quit_at[q];
        if (quit_frame == 0) {
            vga.input.quit_requested = 1;
            game.terrain.frames = NULL; /* quit先于资源预检。 */
        }
        fd2_field_game before = game;
        CHECK(play_field_auxiliary(&game, &vga) == FD2_SCENE_RESULT_HOST_QUIT);
        CHECK(memcmp(&game, &before, sizeof(game)) == 0 && !tick_calls);
        CHECK(!vga.frame_deadline_ns && !vga.frame_interval_ns);
        CHECK(timed_frames == (quit_frame > 0 ? quit_frame : quit_frame < 0 ? close_frame + 6 : 0));
        CHECK(!bad_frozen_water && !bad_restore_order);
        CHECK(water_calls == (quit_frame < 0 ? 1 : 0));
        CHECK(water_changes == (quit_frame < 0 ? 1 : 0));
    }
    return 0;
}

static int test_formal_route_no_key_fallthrough(void) {
    for (int key = 0; key < 2; key++) {
        controls_reset();
        route_mode = 1;
        close_key = key ? SDL_SCANCODE_RETURN : SDL_SCANCODE_RIGHT;
        fd2_vga vga = {0};
        fd2_input_init(&vga.input);
        fd2_archive archive = {0};
        CHECK(fd2_field_play_run(&vga, &archive, 0, 0, NULL, NULL, NULL, 0) == FD2_FIELD_PLAY_RETURN_HOST_QUIT);
        CHECK(restored && route_closed && outer_frames == 2);
        CHECK(!after_restore_queries && timed_frames == close_frame + 6);
        CHECK(tick_calls == 2); /* 仅modal前后两次outer tick。 */
    }
    return 0;
}

static uint64_t framebuffer_hash(const fd2_vga *vga) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < sizeof(vga->framebuffer); i++)
        hash = (hash ^ vga->framebuffer[i]) * UINT64_C(1099511628211);
    return hash;
}

static int test_real_stage0_original_probe_indices(void) {
    fd2_archive archive = {0};
    fd2_field_game game = {0};
    fd2_vga vga = {0};
    CHECK(fd2_archive_open(&archive, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_field_game_open(&game, &vga, &archive, 0) == 0);
    CHECK(fd2_field_game_load_active(&game, "original_game/START.SAV") == 0);
    CHECK(game.units.count == 12 && game.roster_count == 4);
    /* probe02在START焦点(8,15)向左一格空地，native WGC完整帧已过。
     * 常数来自独立Python raw-map/shape oracle的64000索引字节FNV1a；
     * 包含所有黑边与0色，不忽略透明色。并非SDL截图或RGB自比较。 */
    static const uint64_t expected[8] = {
        UINT64_C(0x044a271d72c68e67), UINT64_C(0x96ad271b316edae7),
        UINT64_C(0x6db48fc463b62c9b), UINT64_C(0xb38df99d88ce5fb3),
        UINT64_C(0x7c62cdf4c0062257), UINT64_C(0x6612b50896382be7),
        UINT64_C(0xf637b6466cb3e923), UINT64_C(0x4c87740d1ee7781b)
    };
    game.cursor_cell_x = 7;
    game.cursor_cell_y = 15;
    CHECK(auxiliary_field_ready(&game));
    fd2_field_game before = game;
    uint8_t palette_before[768], dac_before[768];
    memcpy(palette_before, vga.palette, sizeof(palette_before));
    memcpy(dac_before, vga.dac, sizeof(dac_before));
    auxiliary_transition_frame(&game, &vga, 7);
    CHECK(framebuffer_hash(&vga) == UINT64_C(0xa1d60e0b92728d88));
    for (int phase = 0; phase < 8; phase++) {
        auxiliary_markers(&game, &vga, phase);
        CHECK(framebuffer_hash(&vga) == expected[phase]);
    }
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    CHECK(memcmp(vga.palette, palette_before, sizeof(palette_before)) == 0);
    CHECK(memcmp(vga.dac, dac_before, sizeof(dac_before)) == 0);
    fd2_field_game_close(&game);
    fd2_archive_close(&archive);
    return 0;
}

int main(void) {
    CHECK(test_full_frame_and_transition() == 0);
    CHECK(test_marker_filter_order_and_pulse() == 0);
    CHECK(test_modal_keys_owner_and_held() == 0);
    CHECK(test_invalid_preflight_and_quit() == 0);
    CHECK(test_formal_route_no_key_fallthrough() == 0);
    CHECK(test_real_stage0_original_probe_indices() == 0);
    puts("field auxiliary tests: ok");
    return 0;
}
