/* 总览modal无窗口回归：测试TU私有替身控制分配、present和当前帧键态；
 * 包含正式field_play.c以测试其私有modal，不给生产代码新增测试hook。 */
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

static uint64_t test_clock;
static int timed_frames, close_frame, quit_frame, modal_started, modal_done;
static int background_calls, dynamic_seen, old_pulse_seen, bad_free;
static int allocations, frees, fail_allocation, route_mode, action_queries;
static int post_modal_queries, next_outer_present, hold_enter;
static SDL_Scancode close_key;
static void *allocated[2];
static fd2_field_game *current_game;
static fd2_field_game route_before;
static fd2_field_rng test_rng;
static int route_owner_unchanged;
static uint16_t route_rng_before;

static void *overview_malloc(size_t size) {
    allocations++;
    if (allocations == fail_allocation) return NULL;
    void *memory = malloc(size);
    if (allocations <= 2) allocated[allocations - 1] = memory;
    return memory;
}

static void overview_free(void *memory) {
    if (!memory) return;
    int found = 0;
    for (size_t i = 0; i < 2; i++) {
        if (allocated[i] == memory) { allocated[i] = NULL; found = 1; break; }
    }
    if (!found) { bad_free = 1; return; }
    frees++;
    free(memory);
}

static uint64_t overview_ticks(void) { return test_clock; }
static void overview_delay(uint32_t ms) { test_clock += ms; }
static void overview_wait_deadline(fd2_vga *vga) {
    vga->frame_deadline_ns = vga->frame_interval_ns = 0;
}

static int owner_unchanged(const fd2_field_game *game,
                            const fd2_field_game *before) {
    fd2_field_game normalized = *game;
    normalized.idle_phase = before->idle_phase;
    normalized.last_idle_ms = before->last_idle_ms;
    normalized.terrain_visual_phase = before->terrain_visual_phase;
    normalized.last_terrain_visual_ms = before->last_terrain_visual_ms;
    normalized.range_visual_phase = before->range_visual_phase;
    normalized.last_range_visual_ms = before->last_range_visual_ms;
    normalized.command_highlight_phase = before->command_highlight_phase;
    normalized.last_command_highlight_ms = before->last_command_highlight_ms;
    normalized.system_menu.highlight_phase = before->system_menu.highlight_phase;
    normalized.system_menu.last_highlight_ms = before->system_menu.last_highlight_ms;
    return memcmp(&normalized, before, sizeof(normalized)) == 0;
}

static void set_physical_frame(fd2_vga *vga, int frame) {
    for (int key = 1; key < SDL_SCANCODE_COUNT; key++)
        fd2_input_set_test_key_state(&vga->input, (SDL_Scancode)key, 0);
    if (hold_enter)
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
    if (frame == 3) { /* 打开动画输入必须消耗，不可关闭稳定页。 */
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_G, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
    }
    if (frame == 13 && close_frame > 13)
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_LSHIFT, 1);
    if (frame == close_frame) {
        fd2_input_set_test_key_state(&vga->input, close_key, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
    }
    if (frame == close_frame + 2) {
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_F2, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_UP, 1);
    }
    fd2_input_begin_test_frame(&vga->input, test_clock);
    if (quit_frame != 0 && frame == quit_frame) vga->input.quit_requested = 1;
}

static void overview_present_timed(fd2_vga *vga, uint32_t ms) {
    if (fd2_input_has_any_key(&vga->input)) old_pulse_seen = 1;
    timed_frames++;
    test_clock += ms;
    vga->frame_deadline_ns = test_clock * 1000000u;
    vga->frame_interval_ns = (uint64_t)ms * 1000000u;
    if (timed_frames > 12 && timed_frames <= close_frame &&
        vga->framebuffer[10 * VGA_STRIDE + 10] != 20)
        dynamic_seen = 1;
    set_physical_frame(vga, timed_frames);
}

static void overview_present(fd2_vga *vga) {
    if (route_mode && !modal_started) {
        /* 正式loop的打开帧：Enter先被action消费，Right仍待modal清除。 */
        test_clock += 16;
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
        fd2_input_begin_test_frame(&vga->input, test_clock);
    } else if (!modal_done) {
        if (fd2_input_has_any_key(&vga->input)) old_pulse_seen = 1;
        modal_done = 1;
        test_clock += 16;
        /* 恢复背景这一帧也产生普通键，返回前必须清掉。 */
        set_physical_frame(vga, 0);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_DOWN, 1);
        fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN, 1);
        fd2_input_begin_test_frame(&vga->input, test_clock);
        if (quit_frame < 0) vga->input.quit_requested = 1;
    } else if (route_mode) {
        next_outer_present++;
        route_owner_unchanged = owner_unchanged(current_game, &route_before) &&
            ((fd2_field_rng *)current_game->attack_rng_userdata)->state == route_rng_before;
        vga->input.quit_requested = 1; /* 只在下一正常呈现帧结束测试。 */
    }
    overview_wait_deadline(vga);
}

static void overview_background(fd2_field_game *game, fd2_vga *vga) {
    if (route_mode && !modal_started) {
        route_before = *game;
        route_rng_before = ((fd2_field_rng *)game->attack_rng_userdata)->state;
    }
    modal_started = 1;
    background_calls++;
    memset(vga->framebuffer, 20 + game->idle_phase, sizeof(vga->framebuffer));
}

static int overview_open(fd2_field_game *, fd2_vga *, const fd2_archive *, size_t);
static int overview_take_action(fd2_input *input, fd2_input_context context,
                                 fd2_input_action *action) {
    action_queries++;
    if (modal_done && !next_outer_present) post_modal_queries++;
    return fd2_input_take_action(input, context, action);
}

#define malloc overview_malloc
#define free overview_free
#define SDL_GetTicks overview_ticks
#define fd2_delay_ms overview_delay
#define fd2_vga_present overview_present
#define fd2_vga_present_timed overview_present_timed
#define fd2_vga_wait_frame_deadline overview_wait_deadline
#define fd2_field_game_render_background overview_background
#define fd2_field_game_open overview_open
#define fd2_input_take_action overview_take_action
#include "../field_play.c"
#undef malloc
#undef free
#undef SDL_GetTicks
#undef fd2_delay_ms
#undef fd2_vga_present
#undef fd2_vga_present_timed
#undef fd2_vga_wait_frame_deadline
#undef fd2_field_game_render_background
#undef fd2_field_game_open
#undef fd2_input_take_action

static void reset_controls(void) {
    test_clock = 1000;
    timed_frames = modal_started = modal_done = background_calls = 0;
    dynamic_seen = old_pulse_seen = bad_free = allocations = frees = 0;
    fail_allocation = route_mode = action_queries = post_modal_queries = 0;
    next_outer_present = route_owner_unchanged = 0;
    close_frame = 15;
    quit_frame = 0;
    close_key = SDL_SCANCODE_A;
    hold_enter = 0;
    memset(allocated, 0, sizeof(allocated));
    fd2_field_rng_seed(&test_rng, 0x3141);
}

static int image_init(fd2_image *image, int width, int height, uint8_t color) {
    image->pixels = malloc((size_t)width * (size_t)height);
    if (!image->pixels) return -1;
    image->width = width;
    image->height = height;
    memset(image->pixels, color, (size_t)width * (size_t)height);
    return 0;
}

static int init_game(fd2_field_game *game) {
    memset(game, 0, sizeof(*game));
    game->ready = 1;
    game->stage = 0;
    game->active_side = 2;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->selected_unit = game->detail_unit = game->attack_target = -1;
    game->detail_acknowledged_unit = -1;
    game->turn_number = 17;
    game->phase_next_action_ms = 12345;
    game->phase_unit_cursor = 3;
    game->camera_cell_x = 5;
    game->camera_cell_y = 11;
    game->cursor_cell_x = 8;
    game->cursor_cell_y = 15;
    game->map.width = game->map.height = 24;
    game->resource_value = 12345678;
    game->units.count = 2;
    game->units.items[0].side = 2;
    game->units.items[1].side = 0;
    game->units.items[0].hp = game->units.items[1].hp = 1;
    game->roster_count = 1;
    memset(game->roster, 0xa5, sizeof(game->roster));
    game->cell_action_completed[0] = 1;
    game->last_idle_ms = game->last_terrain_visual_ms =
        game->last_range_visual_ms = test_clock;
    game->attack_rng = fd2_field_rng_next;
    game->attack_rng_userdata = &test_rng;
    fd2_field_system_menu_init(&game->system_menu);
    const int sizes[4][2] = {{102,17},{170,117},{170,16},{63,15}};
    for (size_t i = 0; i < 4; i++)
        CHECK(image_init(&game->info_assets.overview[i], sizes[i][0], sizes[i][1],
                          (uint8_t)(30 + i)) == 0);
    for (size_t bank = 0; bank < 2; bank++) {
        for (size_t digit = 0; digit < 10; digit++)
            CHECK(image_init(&game->info_assets.digits[bank][digit],
                              digit == 1 ? 5 : 6, 8, (uint8_t)(50 + digit)) == 0);
    }
    CHECK(image_init(&game->info_assets.digit_overflow[1], 18, 8, 80) == 0);
    CHECK(image_init(&game->info_assets.two_digit_overflow, 12, 8, 81) == 0);
    game->info_assets.ready = 1;
    static const uint8_t glyph[32] = {0x80};
    static const uint8_t text[] = {0,0,0xff,0xff,0,0,0xff,0xff};
    game->font = (fd2_font){glyph, sizeof(glyph), 1};
    game->ui_text.offsets = calloc(599, sizeof(*game->ui_text.offsets));
    CHECK(game->ui_text.offsets != NULL);
    game->ui_text.offsets[598] = 4;
    game->ui_text.data = text;
    game->ui_text.size = sizeof(text);
    game->ui_text.fragment_count = 599;
    current_game = game;
    return 0;
}

static int overview_open(fd2_field_game *game, fd2_vga *vga,
                          const fd2_archive *archive, size_t stage) {
    (void)vga; (void)archive; (void)stage;
    if (init_game(game) != 0) return -1;
    fd2_field_system_menu_open_secondary(&game->system_menu);
    game->interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
    return 0;
}

static int test_modal_keys_and_owner(void) {
    const SDL_Scancode keys[] = {SDL_SCANCODE_A, SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_F2, SDL_SCANCODE_ESCAPE, SDL_SCANCODE_RETURN};
    for (size_t i = 0; i < sizeof(keys)/sizeof(keys[0]); i++) {
        reset_controls();
        close_key = keys[i];
        if (i == 0) close_frame = 24; /* 等到idle动画发生变化。 */
        fd2_field_game game;
        CHECK(init_game(&game) == 0);
        fd2_field_game before = game;
        fd2_vga vga = {0};
        fd2_input_init(&vga.input);
        fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 1);
        fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RIGHT, 1);
        fd2_input_begin_test_frame(&vga.input, test_clock);
        CHECK(play_field_overview(&game, &vga) == FD2_SCENE_RESULT_OK);
        CHECK(timed_frames == close_frame + 12 && modal_done);
        CHECK(background_calls == 1 + close_frame - 12);
        CHECK(!old_pulse_seen && !fd2_input_has_any_key(&vga.input));
        CHECK(allocations == 2 && frees == 2 && !bad_free);
        CHECK(!vga.frame_deadline_ns && !vga.frame_interval_ns);
        CHECK(vga.framebuffer[10 * VGA_STRIDE + 10] == 20);
        CHECK(owner_unchanged(&game, &before) && test_rng.state == 0x3141);
        if (i == 0) CHECK(dynamic_seen);
        fd2_field_game_close(&game);
    }
    return 0;
}

static int test_modal_failure_and_quit(void) {
    for (int mode = 0; mode < 8; mode++) {
        reset_controls();
        fd2_field_game game;
        CHECK(init_game(&game) == 0);
        fd2_vga vga = {0};
        memset(vga.framebuffer, 0x77, sizeof(vga.framebuffer));
        fd2_input_init(&vga.input);
        fd2_field_game before = game;
        if (mode < 2) fail_allocation = mode + 1;
        if (mode == 2) game.ui_text.fragment_count = 598;
        if (mode == 3) vga.input.quit_requested = 1;
        if (mode == 4) quit_frame = 5;
        if (mode == 5) quit_frame = close_frame; /* quit与关闭键同帧 */
        if (mode == 6) quit_frame = close_frame + 3;
        if (mode == 7) quit_frame = -1; /* 恢复背景的最后一次present */
        int expected = mode < 3 ? FD2_SCENE_RESULT_ERROR : FD2_SCENE_RESULT_HOST_QUIT;
        CHECK(play_field_overview(&game, &vga) == expected);
        if (mode == 2) game.ui_text.fragment_count = before.ui_text.fragment_count;
        CHECK(owner_unchanged(&game, &before) && test_rng.state == 0x3141);
        CHECK(!allocated[0] && !allocated[1] && !bad_free);
        CHECK(frees == (mode == 0 || mode == 3 ? 0 : mode == 1 ? 1 : 2));
        CHECK(!vga.frame_deadline_ns && !vga.frame_interval_ns);
        if (mode < 4) {
            CHECK(timed_frames == 0 && background_calls == 0);
            for (size_t p = 0; p < sizeof(vga.framebuffer); p++)
                CHECK(vga.framebuffer[p] == 0x77);
        } else if (mode == 7) CHECK(timed_frames == close_frame + 12 && modal_done);
        else CHECK(timed_frames == quit_frame && !modal_done);
        fd2_field_game_close(&game);
    }
    return 0;
}

static int test_modal_preconditions(void) {
    for (int mode = 0; mode < 6; mode++) {
        reset_controls();
        fd2_field_game game;
        CHECK(init_game(&game) == 0);
        if (mode == 0) game.stage = 1;
        if (mode == 1) game.active_side = 0;
        if (mode == 2) game.selected_unit = 0;
        if (mode == 3) game.interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
        if (mode == 4) game.detail_visible = 1;
        if (mode == 5) game.failure = FD2_FIELD_FAILURE_EVENT_UNSUPPORTED;
        fd2_field_game before = game;
        fd2_vga vga = {0};
        fd2_input_init(&vga.input);
        CHECK(play_field_overview(&game, &vga) == FD2_SCENE_RESULT_ERROR);
        CHECK(allocations == 0 && timed_frames == 0 && background_calls == 0);
        CHECK(owner_unchanged(&game, &before) && test_rng.state == 0x3141);
        fd2_field_game_close(&game);
    }
    return 0;
}

static int test_transition_geometry(void) {
    uint8_t source[VGA_W * VGA_H];
    for (int y = 0; y < VGA_H; y++) {
        for (int x = 0; x < VGA_W; x++)
            source[y * VGA_STRIDE + x] = (uint8_t)(0x40 + (x + y * 3) % 0x80);
    }
    fd2_vga vga = {0};
#define PIX(p, x, y) ((p)[(y) * VGA_STRIDE + (x)])
#define FRAME(phase, opening) do { \
    memset(vga.framebuffer, 0x11, sizeof(vga.framebuffer)); \
    overview_transition_frame(&vga, source, phase, opening); } while (0)
    FRAME(0, 1);
    CHECK(PIX(vga.framebuffer, 275, 37) == PIX(source, 75, 37));
    CHECK(PIX(vga.framebuffer, 319, 153) == PIX(source, 119, 153));
    CHECK(PIX(vga.framebuffer, 274, 37) == 0x11);
    FRAME(0, 0);
    CHECK(PIX(vga.framebuffer, 0, 37) == PIX(source, 200, 37));
    CHECK(PIX(vga.framebuffer, 44, 153) == PIX(source, 244, 153));
    CHECK(PIX(vga.framebuffer, 45, 37) == 0x11);
    FRAME(3, 1);
    CHECK(PIX(vga.framebuffer, 109, 0) == PIX(source, 109, 24));
    CHECK(PIX(vga.framebuffer, 109, 11) == PIX(source, 109, 35));
    CHECK(PIX(vga.framebuffer, 109, 12) == 0x11);
    FRAME(5, 1);
    CHECK(PIX(vga.framebuffer, 75, 191) == 0x11);
    FRAME(6, 1);
    CHECK(PIX(vga.framebuffer, 75, 182) == PIX(source, 75, 155));
    CHECK(PIX(vga.framebuffer, 244, 197) == PIX(source, 244, 170));
    FRAME(8, 1);
    CHECK(PIX(vga.framebuffer, 129, 188) == 0x11);
    FRAME(9, 1);
    CHECK(PIX(vga.framebuffer, 129, 184) == PIX(source, 129, 172));
    CHECK(PIX(vga.framebuffer, 191, 198) == PIX(source, 191, 186));
    FRAME(11, 1);
    CHECK(PIX(vga.framebuffer, 129, 176) == PIX(source, 129, 172));
    FRAME(12, 1);
    CHECK(PIX(vga.framebuffer, 129, 172) == PIX(source, 129, 172));
#undef FRAME
#undef PIX
    return 0;
}

static int test_modal_preserves_held_lifecycle(void) {
    reset_controls();
    hold_enter = 1;
    fd2_field_game game;
    CHECK(init_game(&game) == 0);
    fd2_vga vga = {0};
    fd2_input_init(&vga.input);
    fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&vga.input, test_clock);
    uint64_t repeat_at = vga.input.next_repeat_ms[SDL_SCANCODE_RETURN];
    CHECK(play_field_overview(&game, &vga) == FD2_SCENE_RESULT_OK);
    CHECK(vga.input.previous_down[SDL_SCANCODE_RETURN]);
    CHECK(vga.input.next_repeat_ms[SDL_SCANCODE_RETURN] == repeat_at);
    fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_DOWN, 0);
    fd2_input_begin_test_frame(&vga.input, repeat_at - 1);
    CHECK(!fd2_input_has_any_key(&vga.input));
    fd2_input_begin_test_frame(&vga.input, repeat_at);
    fd2_input_event event;
    CHECK(fd2_input_take_key(&vga.input, &event) && event.repeat &&
          event.source == SDL_SCANCODE_RETURN);
    fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 0);
    fd2_input_begin_test_frame(&vga.input, repeat_at + 1);
    fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_RETURN, 1);
    fd2_input_begin_test_frame(&vga.input, repeat_at + 2);
    CHECK(fd2_input_take_key(&vga.input, &event) && !event.repeat);
    fd2_field_game_close(&game);
    return 0;
}

static int test_public_loop_ends_input_frame(void) {
    reset_controls();
    route_mode = 1;
    fd2_vga vga = {0};
    fd2_input_init(&vga.input);
    fd2_archive archive = {0};
    CHECK(fd2_field_play_run(&vga, &archive, 0, 0, NULL, NULL, NULL, 0) ==
          FD2_FIELD_PLAY_RETURN_HOST_QUIT);
    CHECK(modal_done && next_outer_present == 1);
    CHECK(action_queries == 1 && post_modal_queries == 0);
    CHECK(route_owner_unchanged && test_rng.state == 0x3141);
    CHECK(allocations == 2 && frees == 2 && !bad_free);
    return 0;
}

static int test_real_background(void) {
    fd2_archive archive = {0};
    CHECK(fd2_archive_open(&archive, "original_game/FDOTHER.DAT") == 0);
    fd2_field_game game = {0};
    fd2_vga vga = {0};
    CHECK(fd2_field_game_open(&game, &vga, &archive, 0) == 0);
    game.cursor_cell_x = game.camera_cell_x + 1;
    game.cursor_cell_y = game.camera_cell_y + 6;
    int camera_x = game.camera_cell_x * 24 - FD2_FIELD_VIEW_BORDER;
    int camera_y = game.camera_cell_y * 24 - FD2_FIELD_VIEW_BORDER;
    fd2_terrain_render_field_base_animated(&vga, &game.terrain, &game.map,
        camera_x, camera_y, game.terrain_visual_phase, game.idle_phase);
    fd2_field_units_render(&vga, &game.sprites, &game.units,
                           camera_x, camera_y, game.idle_phase);
    fd2_terrain_render_field_overlay(&vga, &game.terrain, &game.map,
                                     camera_x, camera_y, game.terrain_visual_phase);
    for (int y = 0; y < VGA_H; y++) {
        uint8_t *row = vga.framebuffer + y * VGA_STRIDE;
        if (y < 4 || y >= VGA_H - 4) memset(row, 0, VGA_W);
        else { memset(row, 0, 4); memset(row + VGA_W - 4, 0, 4); }
    }
    uint8_t expected[VGA_W * VGA_H];
    memcpy(expected, vga.framebuffer, sizeof(expected));
    fd2_field_interaction states[] = {FD2_FIELD_INTERACTION_BROWSE,
        FD2_FIELD_INTERACTION_UNIT_SELECTED, FD2_FIELD_INTERACTION_COMMAND,
        FD2_FIELD_INTERACTION_SYSTEM_MENU};
    for (size_t i = 0; i < sizeof(states)/sizeof(states[0]); i++) {
        game.interaction = states[i];
        game.selected_unit = 0;
        game.detail_visible = 1;
        game.detail_unit = 0;
        game.info_panel_right = 0;
        memset(vga.framebuffer, 0xff, sizeof(vga.framebuffer));
        fd2_field_game_render_background(&game, &vga);
        CHECK(memcmp(expected, vga.framebuffer, sizeof(expected)) == 0);
        CHECK(game.info_panel_right == 0);
    }
    game.detail_visible = 0;
    game.interaction = FD2_FIELD_INTERACTION_BROWSE;
    game.selected_unit = -1;
    fd2_field_game_render(&game, &vga);
    CHECK(game.info_panel_right == 1);
    CHECK(memcmp(expected, vga.framebuffer, sizeof(expected)) != 0);
    fd2_field_game_close(&game);
    fd2_archive_close(&archive);
    return 0;
}

int main(void) {
    CHECK(test_modal_keys_and_owner() == 0);
    CHECK(test_modal_failure_and_quit() == 0);
    CHECK(test_modal_preconditions() == 0);
    CHECK(test_transition_geometry() == 0);
    CHECK(test_modal_preserves_held_lifecycle() == 0);
    CHECK(test_public_loop_ends_input_frame() == 0);
    CHECK(test_real_background() == 0);
    puts("field_play_overview_test: ok");
    return 0;
}
