/* B2普通area0目标：field_player_magic_command_execute @0x1cff0、
 * field_target_range_build @0x14818、sub_14742 @0x14742与
 * field_cell_selection_execute @0x115b6。合成数据不冒称原版施法验收。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

typedef struct {
    fd2_field_game game;
    uint32_t cells[24 * 24];
    fd2_terrain_attr attr;
    size_t rng_calls;
} fixture;

static uint32_t rng(void *data) { (*(size_t *)data)++; return 17; }

static void set_unit(fd2_field_unit *u, uint8_t id, uint8_t side, uint8_t x, uint8_t y) {
    memset(u, 0, sizeof(*u));
    u->unit_id = id; u->side = side; u->x = x; u->y = y;
    u->hp = u->hp_max = 28; u->mp = u->mp_max = 8;
    u->movement_profile = 5;
}

static int init(fixture *f) {
    memset(f, 0, sizeof(*f));
    fd2_field_game *g = &f->game;
    g->ready = 1; g->active_side = 2; g->turn_number = 7;
    g->selected_unit = 0; g->detail_unit = -1; g->attack_target = -1;
    g->interaction = FD2_FIELD_INTERACTION_MAGIC_LIST;
    g->cursor_cell_x = g->cursor_cell_y = 6;
    g->focus_cycle_unit = 3;
    g->map.width = g->map.height = 24; g->map.cells = f->cells;
    g->terrain.attrs = &f->attr; g->terrain.attr_count = 1;
    g->sprites.frame_count = 140u * 12u;
    g->units.count = 6;
    set_unit(&g->units.items[0], 9, 2, 6, 6);
    g->units.items[0].data_09_1e[0x11] = 1;
    g->units.items[0].data_09_1e[0x12] = 0x20; /* 合成学习0/13。 */
    set_unit(&g->units.items[1], 96, 0, 5, 6);
    set_unit(&g->units.items[2], 4, 1, 7, 6);
    set_unit(&g->units.items[3], 96, 0, 6, 7);
    g->units.items[3].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    set_unit(&g->units.items[4], 96, 0, 6, 4);
    g->units.items[4].flags = FD2_FIELD_UNIT_FLAG_ACTED;
    g->units.items[4].hp = 0; /* 原版target不加HP/acted门。 */
    set_unit(&g->units.items[5], 96, 0, 15, 15);
    memset(g->metadata.turn_events, 0xff, sizeof(g->metadata.turn_events));
    memset(g->metadata.cell_lookup, 0xff, sizeof(g->metadata.cell_lookup));
    g->pending_cell_event = (fd2_field_pending_cell_event){1, 9, 4};
    g->attack_rng = rng; g->attack_rng_userdata = &f->rng_calls;
    g->move_path = malloc(4);
    g->move_range.nodes = calloc(1, sizeof(*g->move_range.nodes));
    CHECK(g->move_path && g->move_range.nodes);
    memset(g->move_path, 3, 4); g->move_path_capacity = 4;
    g->move_range.width = g->move_range.height = 1;
    g->move_range.nodes[0].distance = 77;
    g->move_origin_x = 4; g->move_origin_y = 6;
    g->move_origin_direction = 1;
    return 0;
}

static void close_fixture(fixture *f) {
    fd2_field_metadata_close(&f->game.metadata);
    fd2_field_path_close(&f->game.move_range);
    free(f->game.move_path);
}

static int test_raw_order_filter_and_cycle(void) {
    fixture f; CHECK(init(&f) == 0);
    fd2_field_game *g = &f.game;
    fd2_field_units before_units = g->units;
    CHECK(fd2_field_game_begin_magic_targeting(g, 0) == 0);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_MAGIC_TARGETING);
    CHECK(g->magic_target_count == 2 && g->magic_target_candidates[0] == 1 && g->magic_target_candidates[1] == 4);
    CHECK(g->magic_target_cycle == 0 && g->cursor_cell_x == 5 && g->cursor_cell_y == 6);
    CHECK(fd2_field_game_magic_target_is_legal(g) == 1);
    CHECK(fd2_field_game_cycle_magic_target(g) == 1);
    CHECK(g->magic_target_cycle == 1 && g->cursor_cell_x == 6 && g->cursor_cell_y == 4);
    CHECK(fd2_field_game_magic_target_is_legal(g) == 1); /* HP0/acted也合法。 */
    CHECK(fd2_field_game_cycle_magic_target(g) == 1 && g->magic_target_cycle == 0);
    CHECK(g->cursor_cell_x == 5 && g->cursor_cell_y == 6 && g->focus_cycle_unit == 3);
    CHECK(!memcmp(&before_units, &g->units, sizeof(before_units)) && f.rng_calls == 0);
    CHECK(fd2_field_game_cancel_magic_targeting(g) == 0);
    g->magic_list_selected = 1;
    CHECK(fd2_field_game_begin_magic_targeting(g, 13) == 0);
    CHECK(g->magic_target_count == 2 && g->magic_target_candidates[0] == 0 && g->magic_target_candidates[1] == 2);
    CHECK(g->cursor_cell_x == 6 && g->cursor_cell_y == 6);
    CHECK(fd2_field_game_cycle_magic_target(g) == 1 && g->cursor_cell_x == 7);
    CHECK(fd2_field_game_magic_target_is_legal(g) == 1);
    CHECK(g->magic_list_selected == 1 && g->focus_cycle_unit == 3 && f.rng_calls == 0);
    close_fixture(&f); return 0;
}

static int test_121_keeps_its_cycle_slot(void) {
    fixture f; CHECK(init(&f) == 0);
    fd2_field_game *g = &f.game;
    g->units.items[1].unit_id = 121;
    CHECK(fd2_field_game_begin_magic_targeting(g, 0) == 0);
    CHECK(g->magic_target_count == 2 && g->magic_target_candidates[0] == 1);
    CHECK(g->cursor_cell_x == 6 && g->cursor_cell_y == 6); /* 首项121不focus。 */
    CHECK(fd2_field_game_cycle_magic_target(g) == 1 && g->magic_target_cycle == 1);
    CHECK(g->cursor_cell_y == 4);
    CHECK(fd2_field_game_cycle_magic_target(g) == 1 && g->magic_target_cycle == 0);
    CHECK(g->cursor_cell_x == 6 && g->cursor_cell_y == 4); /* 121占槽但不移动。 */
    CHECK(fd2_field_game_cancel_magic_targeting(g) == 0);
    g->units.items[4].flags |= FD2_FIELD_UNIT_FLAG_HIDDEN;
    CHECK(fd2_field_game_begin_magic_targeting(g, 0) == 0 && g->magic_target_count == 1);
    CHECK(fd2_field_game_cycle_magic_target(g) == 1 && g->magic_target_cycle == 0);
    CHECK(g->cursor_cell_x == 6 && g->cursor_cell_y == 6 && f.rng_calls == 0);
    close_fixture(&f); return 0;
}

static int test_empty_range_and_illegal_confirmation_do_not_submit(void) {
    fixture f; CHECK(init(&f) == 0);
    fd2_field_game *g = &f.game;
    g->units.items[1].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    g->units.items[4].flags |= FD2_FIELD_UNIT_FLAG_HIDDEN;
    CHECK(fd2_field_game_begin_magic_targeting(g, 0) == 0 && g->magic_target_count == 0);
    fd2_field_game before = *g;
    CHECK(fd2_field_game_cycle_magic_target(g) == 0 && !memcmp(g, &before, sizeof(before)));
    CHECK(fd2_field_game_magic_target_is_legal(g) == 0);
    for (int i = 0; i < 6; i++) CHECK(fd2_field_game_move_cursor(g, -1, 0) == 1);
    CHECK(g->cursor_cell_x == 0 && !fd2_field_path_is_destination(&g->move_range, 0, 6));
    before = *g;
    CHECK(fd2_field_game_magic_target_is_legal(g) == 0);
    CHECK(fd2_field_game_confirm_cursor(g) == -1); /* 尚未接入提交。 */
    CHECK(!memcmp(g, &before, sizeof(before)) && f.rng_calls == 0);
    CHECK(fd2_field_game_cancel_magic_targeting(g) == 0);
    CHECK(g->cursor_cell_x == 6 && g->cursor_cell_y == 6 && !g->move_range.nodes);
    close_fixture(&f); return 0;
}

static int test_cancel_preserves_temporary_move_and_controller_owner(void) {
    fixture f; CHECK(init(&f) == 0);
    fd2_field_game *g = &f.game;
    g->units.items[0].unit_id = 18; /* 已确认移动施法例外，非自然学习声明。 */
    g->units.items[0].direction = 3;
    g->move_path_length = 2; g->move_preview_valid = 1;
    fd2_field_units units = g->units;
    uint8_t *path = g->move_path;
    CHECK(fd2_field_game_begin_magic_targeting(g, 0) == 0);
    while (g->cursor_cell_x < 23) CHECK(fd2_field_game_move_cursor(g, 1, 0) == 1);
    while (g->cursor_cell_y < 23) CHECK(fd2_field_game_move_cursor(g, 0, 1) == 1);
    CHECK(fd2_field_game_cancel_selection(g) == 1);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_MAGIC_LIST && g->magic_list_selected == 0);
    CHECK(g->cursor_cell_x == 6 && g->cursor_cell_y == 6);
    CHECK(g->camera_cell_x == 5 && g->camera_cell_y == 5); /* 12cea逐格回移，不还原camera快照。 */
    CHECK(!g->move_range.nodes && g->move_path == path && g->move_path_length == 2 && g->move_path[1] == 3);
    CHECK(g->move_preview_valid == 1 && g->move_origin_x == 4 && g->move_origin_direction == 1);
    CHECK(g->pending_cell_event.valid && g->pending_cell_event.action == 9 && g->pending_cell_event.source_slot == 4);
    CHECK(g->selected_unit == 0 && !g->player_post_waiting_phase && g->active_side == 2 && g->turn_number == 7);
    CHECK(!memcmp(&units, &g->units, sizeof(units)) && !g->event_log_count && f.rng_calls == 0);
    CHECK(!g->magic_target_count && !g->magic_target_cycle);
    close_fixture(&f); return 0;
}

static int test_begin_rejections_leave_the_existing_query_untouched(void) {
    CHECK(fd2_field_game_begin_magic_targeting(NULL, 0) == -1);
    CHECK(fd2_field_game_magic_target_is_legal(NULL) == -1);
    CHECK(fd2_field_game_cycle_magic_target(NULL) == -1);
    CHECK(fd2_field_game_cancel_magic_targeting(NULL) == -1);
    for (unsigned which = 0; which < 15; which++) {
        fixture f; CHECK(init(&f) == 0);
        fd2_field_game *g = &f.game;
        uint8_t id = 0;
        switch (which) {
            case 0: id = 1; break;
            case 1: id = 13; break; /* 当前选中仍是0。 */
            case 2: g->units.items[0].mp = 1; break;
            case 3: g->magic_list_selected = 2; break;
            case 4: g->interaction = FD2_FIELD_INTERACTION_COMMAND; break;
            case 5: g->move_path_length = 2; break; /* unit9移动后禁止。 */
            case 6: g->units.count = FD2_FIELD_MAX_UNITS + 1u; break;
            case 7: g->cursor_cell_x = 5; break;
            case 8: g->camera_cell_x = -1; break;
            case 9: g->map.cells = NULL; break;
            case 10: g->terrain.attr_count = 0; break;
            case 11: g->units.items[1].x = 255; break;
            case 12: g->units.items[0].data_09_1e[0x15] = 0x10; break;
            case 13: g->units.items[0].detail_status[2] = 1; break;
            case 14: g->failure = FD2_FIELD_FAILURE_EVENT_DATA; break;
        }
        fd2_field_game before = *g;
        CHECK(fd2_field_game_begin_magic_targeting(g, id) == -1);
        CHECK(!memcmp(g, &before, sizeof(before)) && g->move_range.nodes[0].distance == 77 && f.rng_calls == 0);
        close_fixture(&f);
    }
    return 0;
}

static int test_live_target_revalidation_and_load(void) {
    fixture f; CHECK(init(&f) == 0);
    fd2_field_game *g = &f.game;
    fd2_save_battle_snapshot snapshot;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    CHECK(fd2_field_game_export_battle_snapshot(g, &snapshot) == 0);
    CHECK(fd2_field_game_begin_magic_targeting(g, 0) == 0);
    g->units.items[1].side = 1;
    fd2_field_game before = *g;
    CHECK(fd2_field_game_magic_target_is_legal(g) == 0 && !memcmp(g, &before, sizeof(before)));
    g->units.items[1].side = 0;
    g->units.items[0].mp = 1;
    before = *g;
    CHECK(fd2_field_game_magic_target_is_legal(g) == -1 && !memcmp(g, &before, sizeof(before)));
    g->units.items[0].mp = 8;
    g->magic_target_candidates[1] = 255;
    before = *g;
    CHECK(fd2_field_game_cycle_magic_target(g) == -1 && !memcmp(g, &before, sizeof(before)));
    snapshot.metadata[0] ^= 1;
    CHECK(fd2_field_game_import_battle_snapshot(g, &snapshot) == -1 && !memcmp(g, &before, sizeof(before)));
    snapshot.metadata[0] ^= 1;
    CHECK(fd2_field_game_import_battle_snapshot(g, &snapshot) == 0);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_BROWSE && g->magic_list_selected == 0);
    CHECK(!g->magic_target_id && !g->magic_target_count && !g->magic_target_cycle);
    for (size_t i = 0; i < sizeof(g->magic_target_candidates); i++) CHECK(!g->magic_target_candidates[i]);
    CHECK(f.rng_calls == 0);
    close_fixture(&f); return 0;
}

static int test_target_render_uses_frame1_and_range_lut(void) {
    fixture f; CHECK(init(&f) == 0);
    fd2_field_game *g = &f.game;
    g->units.count = 1;
    g->magic_list_selected = 1;
    uint8_t tile_pixels[24 * 24], cursor_pixels[2][24 * 24];
    memset(tile_pixels, 10, sizeof(tile_pixels));
    memset(cursor_pixels[0], 66, sizeof(cursor_pixels[0]));
    memset(cursor_pixels[1], 77, sizeof(cursor_pixels[1]));
    fd2_image tile = {24, 24, tile_pixels};
    g->terrain.frames = &tile; g->terrain.sheet.frame_count = 1;
    g->visuals.cursor_frame_count = 2;
    for (size_t i = 0; i < 2; i++) g->visuals.cursor_frames[i] = (fd2_image){24, 24, cursor_pixels[i]};
    g->visuals.range_lut_count = FD2_FIELD_RANGE_LUT_COUNT;
    for (size_t i = 0; i < FD2_FIELD_RANGE_LUT_COUNT; i++)
        for (size_t j = 0; j < 256; j++) g->visuals.range_luts[i][j] = (uint8_t)j;
    for (size_t i = 0; i < FD2_FIELD_RANGE_LUT_COUNT; i++) g->visuals.range_luts[i][10] = 42;
    CHECK(fd2_field_game_begin_magic_targeting(g, 13) == 0);
    fd2_vga vga = {0};
    fd2_field_game_render(g, &vga);
    CHECK(vga.framebuffer[(4 + 6 * 24) * VGA_W + 4 + 6 * 24] == 77);
    CHECK(vga.framebuffer[(4 + 5 * 24) * VGA_W + 4 + 6 * 24] == 42);
    CHECK(vga.framebuffer[(4 + 1 * 24) * VGA_W + 4 + 1 * 24] == 10);
    CHECK(f.rng_calls == 0 && g->units.items[0].mp == 8);
    close_fixture(&f); return 0;
}

int main(void) {
    CHECK(test_raw_order_filter_and_cycle() == 0);
    CHECK(test_121_keeps_its_cycle_slot() == 0);
    CHECK(test_empty_range_and_illegal_confirmation_do_not_submit() == 0);
    CHECK(test_cancel_preserves_temporary_move_and_controller_owner() == 0);
    CHECK(test_begin_rejections_leave_the_existing_query_untouched() == 0);
    CHECK(test_live_target_revalidation_and_load() == 0);
    CHECK(test_target_render_uses_frame1_and_range_lut() == 0);
    puts("field_magic_target_test: ok");
    return 0;
}
