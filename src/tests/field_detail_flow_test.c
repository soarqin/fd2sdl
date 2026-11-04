/* field_unit_detail_present @0x17aed（原sub_17AED）的装备→法术→关闭，无窗口私有TU。
 * 包含正式game/play，仅替换呈现、时钟和故障点；构建排除core field_game对象。
 * 详情完整16c57等待／眨眼RNG尚未验收，本测试不为它虚构随机消费。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "field_play.h"
#include "portrait.h"
#include "scene.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

static struct {
    uint64_t clock;
    int allocations, buffers, fail_alloc, decode_failure;
    int animation, animation_frames, timed_frames, quit_at, quit_final, quit_blank;
    int bad_geometry, check_geometry, route, route_actor, route_stage;
    int equipment_keys, magic_keys, controllers, closed, bad_route;
    fd2_field_units final_units;
    int final_selected, final_detail, final_visible;
    uint32_t final_turn;
    uint8_t final_side;
    SDL_Scancode advance_key;
} control;
static fd2_field_game *live_game;
static uint8_t detail_background[VGA_W * VGA_H], detail_equipment_frame[VGA_W * VGA_H], detail_magic_frame[VGA_W * VGA_H];
static fd2_archive detail_other_archive;

int detail_real_open(fd2_field_game *, fd2_vga *, const fd2_archive *, size_t);
void detail_real_close(fd2_field_game *);
void detail_real_render(fd2_field_game *, fd2_vga *);
int detail_real_controller(fd2_field_game *, size_t);
int detail_real_animate(fd2_field_game *, fd2_vga *, int,
                          fd2_field_detail_phase_fn, void *);
int detail_real_flip(fd2_field_game *, fd2_vga *, fd2_field_detail_phase_fn, void *);

static void *detail_alloc(size_t size) {
    control.allocations++;
    if (control.allocations == control.fail_alloc) return NULL;
    void *pointer = malloc(size);
    if (pointer) control.buffers++;
    return pointer;
}
static void detail_free(void *pointer) {
    if (pointer) control.buffers--;
    free(pointer);
}
static int detail_decode(fd2_image *image, const fd2_archive *archive,
                           size_t id, size_t frame) {
    if (control.decode_failure) return -1;
    return fd2_portrait_decode_frame(image, archive, id, frame);
}
static void test_input(fd2_vga *vga, SDL_Scancode key, int animation) {
    for (int i = 1; i < SDL_SCANCODE_COUNT; i++)
        fd2_input_set_test_key_state(&vga->input, (SDL_Scancode)i, 0);
    if (key) fd2_input_set_test_key_state(&vga->input, key, 1);
    if (animation) fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RIGHT, 1);
    fd2_input_begin_test_frame(&vga->input, control.clock);
}
static void prepare_expected(fd2_field_game *game) {
    fd2_vga page = {0};
    int visible = game->detail_visible;
    game->detail_visible = 0;
    detail_real_render(game, &page);
    game->detail_visible = visible;
    memcpy(detail_background, page.framebuffer, sizeof(detail_background));
    fd2_field_detail_draw(&page, &game->info_assets, &game->font, &game->ui_text,
        &game->units.items[game->detail_unit], &game->detail_portrait);
    memcpy(detail_equipment_frame, page.framebuffer, sizeof(detail_equipment_frame));
    (void)fd2_field_magic_list_draw(&page, &game->info_assets, &game->font,
        &game->ui_text, &game->units.items[game->detail_unit], -1);
    memcpy(detail_magic_frame, page.framebuffer, sizeof(detail_magic_frame));
}
static void expected_bottom(uint8_t *dst, const uint8_t *page, int phase) {
    memcpy(dst, detail_background, sizeof(detail_background));
    for (int y = 7; y <= 92; y++) {
        memcpy(dst + y * 320 + 5, page + y * 320 + 5, 86);
        memcpy(dst + y * 320 + 92, page + y * 320 + 92, 223);
    }
    if (phase == 7) return;
    int top = 94 + phase * 16;
    for (int y = top; y < 200 && y < top + 102; y++)
        memcpy(dst + y * 320 + 5, page + (94 + y - top) * 320 + 5, 310);
}
static void detail_timed(fd2_vga *vga, uint32_t interval) {
    control.timed_frames++;
    control.animation_frames++;
    if (control.check_geometry) {
        uint8_t expected[VGA_W * VGA_H];
        int index = control.animation_frames;
        if (control.animation == 3) {
            expected_bottom(expected, index <= 8 ? detail_equipment_frame : detail_magic_frame,
                index <= 8 ? index - 1 : 15 - index);
        } else {
            fd2_field_detail_transition_frame(expected,
                live_game->detail_page == FD2_FIELD_DETAIL_MAGIC ? detail_magic_frame : detail_equipment_frame,
                detail_background, control.animation == 1 ? 12 - index : index - 1);
        }
        if (memcmp(vga->framebuffer, expected, sizeof(expected))) control.bad_geometry = 1;
    }
    control.clock += interval;
    test_input(vga, SDL_SCANCODE_F1, 1);
    if (control.quit_at && control.animation_frames == control.quit_at)
        vga->input.quit_requested = 1;
    vga->frame_deadline_ns = 1;
}
static void detail_present(fd2_vga *vga) {
    control.clock += 16;
    if (control.animation) {
        if (control.animation == 3) {
            control.animation_frames++;
            if (control.check_geometry) {
                uint8_t expected[VGA_W * VGA_H];
                expected_bottom(expected, detail_equipment_frame, 7);
                if (memcmp(vga->framebuffer, expected, sizeof(expected))) control.bad_geometry = 1;
            }
        }
        if (control.check_geometry && control.animation == 2 &&
            memcmp(vga->framebuffer, detail_background, sizeof(detail_background))) control.bad_geometry = 1;
        test_input(vga, SDL_SCANCODE_F1, 1);
        if (control.quit_final || (control.quit_blank && control.animation == 3))
            vga->input.quit_requested = 1;
        return;
    }
    if (!control.route) { test_input(vga, 0, 0); return; }
    if (control.route_stage == 0) {
        test_input(vga, control.route == 2 ? SDL_SCANCODE_RETURN : SDL_SCANCODE_F2, 0);
        control.route_stage = 1;
    } else if (live_game->detail_visible && live_game->detail_page == FD2_FIELD_DETAIL_EQUIPMENT) {
        if (control.route_stage != 1) control.bad_route = 1;
        control.equipment_keys++;
        control.route_stage = 2;
        test_input(vga, control.advance_key, 0);
    } else if (live_game->detail_visible) {
        if (control.route_stage != 2 || control.controllers) control.bad_route = 1;
        control.magic_keys++;
        control.route_stage = 3;
        test_input(vga, SDL_SCANCODE_ESCAPE, 0);
    } else {
        test_input(vga, 0, 0);
        vga->input.quit_requested = 1;
    }
    if (control.clock > 5000) { control.bad_route = 1; vga->input.quit_requested = 1; }
}
static void detail_wait(fd2_vga *vga) {
    vga->frame_deadline_ns = vga->frame_interval_ns = 0;
}
static uint64_t detail_ticks(void) { return control.clock; }
static void detail_delay(uint32_t ms) { control.clock += ms; }
static int detail_water(fd2_vga *vga, uint64_t ms) { (void)vga; (void)ms; return 0; }

#define malloc detail_alloc
#define free detail_free
#define fd2_portrait_decode_frame detail_decode
#define fd2_field_game_open detail_real_open
#define fd2_field_game_close detail_real_close
#define fd2_field_game_render detail_real_render
#define fd2_field_game_complete_player_controller detail_real_controller
#define fd2_field_game_animate_detail detail_real_animate
#define fd2_field_game_animate_detail_magic_page detail_real_flip
#define fd2_vga_present detail_present
#define fd2_vga_present_timed detail_timed
#define fd2_vga_wait_frame_deadline detail_wait
#include "field_game.c"
#undef fd2_field_game_open
#undef fd2_field_game_close
#undef fd2_field_game_render
#undef fd2_field_game_complete_player_controller
#undef fd2_field_game_animate_detail
#undef fd2_field_game_animate_detail_magic_page

int fd2_field_game_open(fd2_field_game *game, fd2_vga *vga,
                        const fd2_archive *archive, size_t stage) {
    int result = detail_real_open(game, vga, archive, stage);
    if (result == 0) result = fd2_field_game_load_active(game, "original_game/START.SAV");
    live_game = game;
    if (result == 0 && control.route) {
        game->cursor_cell_x = game->units.items[control.route_actor].x;
        game->cursor_cell_y = game->units.items[control.route_actor].y;
        if (control.route == 2) game->units.items[control.route_actor].flags |= FD2_FIELD_UNIT_FLAG_ACTED;
    }
    return result;
}
void fd2_field_game_close(fd2_field_game *game) {
    if (control.route) {
        control.closed++;
        control.final_units = game->units;
        control.final_detail = game->detail_unit;
        control.final_visible = game->detail_visible;
        control.final_selected = game->selected_unit;
        control.final_side = game->active_side;
        control.final_turn = game->turn_number;
    }
    detail_real_close(game);
    live_game = NULL;
}
void fd2_field_game_render(fd2_field_game *game, fd2_vga *vga) {
    detail_real_render(game, vga);
}
int fd2_field_game_complete_player_controller(fd2_field_game *game, size_t actor) {
    control.controllers++;
    return detail_real_controller(game, actor);
}
int fd2_field_game_animate_detail(fd2_field_game *game, fd2_vga *vga,
        int opening, fd2_field_detail_phase_fn callback, void *data) {
    control.animation = opening ? 1 : 2;
    control.animation_frames = 0;
    if (control.check_geometry) prepare_expected(game);
    int result = detail_real_animate(game, vga, opening, callback, data);
    control.animation = 0;
    return result;
}
int fd2_field_game_animate_detail_magic_page(fd2_field_game *game, fd2_vga *vga,
        fd2_field_detail_phase_fn callback, void *data) {
    control.animation = 3;
    control.animation_frames = 0;
    if (control.check_geometry) prepare_expected(game);
    int result = detail_real_flip(game, vga, callback, data);
    control.animation = 0;
    return result;
}

#define SDL_GetTicks detail_ticks
#define fd2_delay_ms detail_delay
#define fd2_field_water_tick detail_water
#include "field_play.c"
#undef malloc
#undef free
#undef fd2_portrait_decode_frame
#undef fd2_vga_present
#undef fd2_vga_present_timed
#undef fd2_vga_wait_frame_deadline
#undef SDL_GetTicks
#undef fd2_delay_ms
#undef fd2_field_water_tick

static void reset(void) { memset(&control, 0, sizeof(control)); control.advance_key = SDL_SCANCODE_ESCAPE; }
static int open_game(fd2_field_game *game, fd2_vga *vga) {
    memset(vga, 0, sizeof(*vga));
    return fd2_field_game_open(game, vga, &detail_other_archive, 0);
}
static int test_any_key_routes_and_controller(void) {
    const SDL_Scancode keys[] = {SDL_SCANCODE_ESCAPE, SDL_SCANCODE_A,
        SDL_SCANCODE_UP, SDL_SCANCODE_Z, SDL_SCANCODE_F2};
    for (int route = 1; route <= 2; route++) {
        for (int actor = 0; actor <= 1; actor++) {
            for (size_t key = 0; key < sizeof(keys) / sizeof(keys[0]); key++) {
                reset();
                control.route = route; control.route_actor = actor;
                control.advance_key = keys[key]; control.check_geometry = 1;
                fd2_vga vga = {0};
                CHECK(fd2_field_play_run(&vga, &detail_other_archive, 0, 0, NULL, NULL,
                    "original_game/START.SAV", 0) == FD2_FIELD_PLAY_RETURN_HOST_QUIT);
                CHECK(control.closed == 1 && control.controllers == (route == 2 ? 1 : 0));
                CHECK(control.equipment_keys == 1 && control.magic_keys == actor);
                CHECK(control.final_detail == -1 && !control.final_visible && control.final_selected == -1);
                CHECK(control.final_side == 2 && control.final_turn == 1);
                CHECK(control.final_units.items[actor].mp == (actor ? 8 : 0));
                CHECK(!control.buffers && !control.bad_geometry && !control.bad_route);
            }
        }
    }
    return 0;
}
static int test_page_owner_and_quit(void) {
    for (int phase = 0; phase < 4; phase++) {
        reset();
        fd2_field_game game; fd2_vga vga;
        CHECK(open_game(&game, &vga) == 0);
        CHECK(fd2_field_game_open_detail(&game, 1) == 0);
        fd2_field_units units = game.units;
        int x = game.cursor_cell_x, y = game.cursor_cell_y;
        control.check_geometry = 1;
        if (phase == 0) {
            control.quit_at = 4;
            CHECK(fd2_field_game_animate_detail(&game, &vga, 1, NULL, NULL) == 1);
        } else if (phase == 1) {
            control.quit_at = 11;
            CHECK(play_field_detail_step(&game, &vga, NULL) == FD2_SCENE_RESULT_HOST_QUIT);
        } else if (phase == 2) {
            CHECK(play_field_detail_step(&game, &vga, NULL) == FD2_SCENE_RESULT_OK);
            CHECK(game.detail_visible && game.detail_page == FD2_FIELD_DETAIL_MAGIC);
            control.quit_final = 1;
            CHECK(play_field_detail_step(&game, &vga, NULL) == FD2_SCENE_RESULT_HOST_QUIT);
        } else {
            control.quit_blank = 1;
            CHECK(play_field_detail_step(&game, &vga, NULL) == FD2_SCENE_RESULT_HOST_QUIT);
            CHECK(control.timed_frames == 7 && control.animation_frames == 8);
            CHECK(game.detail_page == FD2_FIELD_DETAIL_EQUIPMENT);
        }
        CHECK(game.detail_visible && game.selected_unit == -1 && !control.controllers);
        CHECK(!memcmp(&units, &game.units, sizeof(units)));
        CHECK(game.cursor_cell_x == x && game.cursor_cell_y == y);
        CHECK(!control.buffers && !control.bad_geometry);
        fd2_field_game_close(&game);
    }
    return 0;
}
static int test_animation_allocation_failures(void) {
    /* 非空法术列表的完整预检scratch、背景、页面各自失败，都不能先呈现或改owner。 */
    for (int flip = 0; flip <= 1; flip++) {
        for (int allocation = 1; allocation <= 3; allocation++) {
            reset();
            fd2_field_game game; fd2_vga vga;
            CHECK(open_game(&game, &vga) == 0);
            CHECK(fd2_field_game_open_detail(&game, 1) == 0);
            fd2_field_game before = game; fd2_vga pixels = vga;
            control.fail_alloc = control.allocations + allocation;
            int result = flip ? play_field_detail_step(&game, &vga, NULL) :
                fd2_field_game_animate_detail(&game, &vga, 1, NULL, NULL);
            CHECK(result == -1);
            CHECK(!memcmp(&game, &before, sizeof(game)));
            CHECK(!memcmp(&vga, &pixels, sizeof(vga)));
            CHECK(!control.timed_frames && !control.animation_frames && !control.buffers);
            fd2_field_game_close(&game);
        }
    }
    return 0;
}
static int test_late_magic_glyph_rejects_first_page(void) {
    reset();
    fd2_field_game game; fd2_vga vga;
    CHECK(open_game(&game, &vga) == 0);
    CHECK(fd2_field_game_open_detail(&game, 1) == 0);
    /* 保留真实上半/装备/ID0名称，只破坏新增末项ID13的第一个字模token。 */
    game.units.items[1].data_09_1e[0x12] |= 0x20;
    const uint8_t *source = game.ui_text.data, *last_tokens;
    size_t count;
    CHECK(fd2_text_entry_get_fragment(&game.ui_text, 441u + 13u, &last_tokens, &count) == 0);
    CHECK(count > 1 && fd2_text_token_at(last_tokens, 0) < game.font.glyph_count);
    uint8_t *bad_text = malloc(game.ui_text.size);
    CHECK(bad_text);
    memcpy(bad_text, source, game.ui_text.size);
    size_t offset = (size_t)(last_tokens - source);
    bad_text[offset] = 1; bad_text[offset + 1] = 0xff;
    game.ui_text.data = bad_text;
    fd2_field_game before = game; fd2_vga pixels = vga;
    CHECK(fd2_field_game_open_detail(&game, 1) == -1);
    CHECK(!memcmp(&game, &before, sizeof(game)) && !memcmp(&vga, &pixels, sizeof(vga)));
    CHECK(!control.timed_frames && !control.buffers);
    game.ui_text.data = source;
    free(bad_text);
    fd2_field_game_close(&game);
    return 0;
}
static int test_preflight_and_load_cleanup(void) {
    for (int fault = 0; fault < 6; fault++) {
        reset();
        fd2_field_game game; fd2_vga vga;
        CHECK(open_game(&game, &vga) == 0);
        CHECK(fd2_field_game_open_detail(&game, 1) == 0);
        switch (fault) {
            case 0: game.units.items[1].data_09_1e[0x15] |= 0x10; break;
            case 1: game.units.items[1].data_09_1e[0x11] = 0xff;
                    game.units.items[1].data_09_1e[0x12] = 0x1f; break;
            case 2: game.units.items[1].data_09_1e[2] = 255; break;
            case 3: game.info_assets.detail_top.height = 85; break;
            case 4: control.decode_failure = 1; break;
            case 5: control.fail_alloc = control.allocations + 1; break;
        }
        fd2_field_game before = game; fd2_vga pixels = vga;
        CHECK(fd2_field_game_open_detail(&game, 1) == -1);
        CHECK(!memcmp(&before, &game, sizeof(game)) && !memcmp(&pixels, &vga, sizeof(vga)));
        CHECK(!control.buffers);
        fd2_field_game_close(&game);
    }
    reset();
    fd2_field_game game; fd2_vga vga;
    CHECK(open_game(&game, &vga) == 0);
    CHECK(fd2_field_game_open_detail(&game, 1) == 0);
    CHECK(play_field_detail_step(&game, &vga, NULL) == FD2_SCENE_RESULT_OK);
    CHECK(game.detail_page == FD2_FIELD_DETAIL_MAGIC);
    fd2_save_file source = {0}; fd2_save_battle_snapshot snapshot;
    CHECK(fd2_save_file_open(&source, "original_game/START.SAV") == 0);
    CHECK(fd2_save_file_get_battle_snapshot(&source, &snapshot) == 0);
    fd2_save_file_close(&source);
    snapshot.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] = 65;
    fd2_field_game before = game;
    CHECK(fd2_field_game_import_battle_snapshot(&game, &snapshot) == -1);
    CHECK(!memcmp(&game, &before, sizeof(game)));
    snapshot.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] = 12;
    CHECK(fd2_field_game_import_battle_snapshot(&game, &snapshot) == 0);
    CHECK(!game.detail_visible && game.detail_page == FD2_FIELD_DETAIL_EQUIPMENT &&
        game.detail_unit == -1 && !game.detail_portrait.pixels && game.detail_acknowledged_unit == -1);
    fd2_field_game_close(&game);
    CHECK(!control.buffers);
    return 0;
}
int main(void) {
    CHECK(fd2_archive_open(&detail_other_archive, "original_game/FDOTHER.DAT") == 0);
    CHECK(test_any_key_routes_and_controller() == 0);
    CHECK(test_page_owner_and_quit() == 0);
    CHECK(test_animation_allocation_failures() == 0);
    CHECK(test_late_magic_glyph_rejects_first_page() == 0);
    CHECK(test_preflight_and_load_cleanup() == 0);
    fd2_archive_close(&detail_other_archive);
    puts("field detail flow tests: ok");
    return 0;
}
