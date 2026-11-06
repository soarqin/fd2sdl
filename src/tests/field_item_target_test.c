/* field_player_item_command_execute @0x1bbdc（原登记同名）的草药目标，
 * 复用field_target_range_build @0x14818与field_cell_selection_execute @0x115b6。
 * 只测目标session；不声称已提交效果、消耗或完整原版演出。 */
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

static void slot(fd2_field_unit *u, unsigned index, uint8_t flags, uint8_t id) {
    uint8_t *raw = (uint8_t *)u;
    raw[0x0a + index * 2] = flags; raw[0x0b + index * 2] = id;
}

static void unit(fd2_field_unit *u, uint8_t id, uint8_t side, uint8_t x, uint8_t y) {
    memset(u, 0, sizeof(*u));
    u->unit_id = id; u->side = side; u->x = x; u->y = y;
    u->hp = u->hp_max = 42; u->movement_profile = 5;
}

static int init(fixture *f) {
    memset(f, 0, sizeof(*f));
    fd2_field_game *g = &f->game;
    g->ready = 1; g->active_side = 2; g->turn_number = 7;
    g->selected_unit = 0; g->detail_unit = -1; g->attack_target = -1;
    g->interaction = FD2_FIELD_INTERACTION_ITEM_LIST; g->item_list_selected = 2;
    g->cursor_cell_x = g->cursor_cell_y = 6; g->focus_cycle_unit = 3;
    g->map.width = g->map.height = 24; g->map.cells = f->cells;
    g->terrain.attrs = &f->attr; g->terrain.attr_count = 1;
    g->sprites.frame_count = 140u * 12u;
    g->units.count = 7;
    unit(&g->units.items[0], 0, 2, 6, 6);
    g->units.items[0].direction = 3;
    g->units.items[0].detail_status[2] = 1; /* +27、MP0、移动后不禁物品。 */
    for (unsigned i = 0; i < 8; i++) slot(&g->units.items[0], i, 0x80, 0xa5);
    slot(&g->units.items[0], 0, 0x40, 0);
    slot(&g->units.items[0], 1, 0x40, 132);
    slot(&g->units.items[0], 2, 0, 192);
    unit(&g->units.items[1], 4, 1, 5, 6);
    unit(&g->units.items[2], 96, 0, 7, 6);
    unit(&g->units.items[3], 4, 2, 6, 7);
    g->units.items[3].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    unit(&g->units.items[4], 4, 2, 6, 5);
    g->units.items[4].flags = FD2_FIELD_UNIT_FLAG_ACTED; g->units.items[4].hp = 0;
    unit(&g->units.items[5], 4, 1, 15, 15);
    unit(&g->units.items[6], 121, 1, 7, 6);
    g->units.items[6].movement_profile = 10; /* 不额外套F1/race门。 */
    memset(g->metadata.turn_events, 0xff, sizeof(g->metadata.turn_events));
    memset(g->metadata.cell_lookup, 0xff, sizeof(g->metadata.cell_lookup));
    g->pending_cell_event = (fd2_field_pending_cell_event){1, 9, 4};
    g->attack_rng = rng; g->attack_rng_userdata = &f->rng_calls;
    g->move_path = malloc(4);
    g->move_range.nodes = calloc(24u * 24u, sizeof(*g->move_range.nodes));
    CHECK(g->move_path && g->move_range.nodes);
    memset(g->move_path, 3, 4); g->move_path_capacity = 4; g->move_path_length = 2;
    g->move_range.width = g->move_range.height = 24;
    g->move_range.nodes[17].distance = 77;
    g->move_origin_x = 4; g->move_origin_y = 6; g->move_origin_direction = 1;
    g->move_preview_valid = 1;
    return 0;
}

static void close_fixture(fixture *f) {
    fd2_field_metadata_close(&f->game.metadata);
    fd2_field_path_close(&f->game.move_range);
    free(f->game.move_path);
}

/* 本地map用于目标查询而非资源owner；这里只验纯记录Load及UI续态清除。
 * 不放松真实地图重载；任意导入结果后都归还fixture原map借用指针。 */
static int import_record_snapshot(fixture *f, const fd2_save_battle_snapshot *snapshot) {
    uint32_t *cells = f->game.map.cells;
    f->game.map.cells = NULL;
    int result = fd2_field_game_import_battle_snapshot(&f->game, snapshot);
    f->game.map.cells = cells;
    return result;
}

static int test_player_filter_raw_order_and_cycle(void) {
    fixture f; CHECK(init(&f) == 0);
    fd2_field_game *g = &f.game;
    fd2_field_units before = g->units;
    CHECK(fd2_field_game_begin_item_targeting(g) == 0);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_ITEM_TARGETING);
    const uint8_t expected[] = {0, 1, 4, 6};
    CHECK(g->magic_target_count == sizeof(expected));
    CHECK(!memcmp(g->magic_target_candidates, expected, sizeof(expected)));
    CHECK(g->magic_target_cycle == 0 && !g->magic_target_id);
    CHECK(g->cursor_cell_x == 6 && g->cursor_cell_y == 6);
    CHECK(fd2_field_game_item_target_is_legal(g) == 1);
    CHECK(fd2_field_game_cycle_item_target(g) == 1 && g->cursor_cell_x == 5);
    CHECK(fd2_field_game_cycle_item_target(g) == 1 && g->cursor_cell_x == 6 && g->cursor_cell_y == 5);
    CHECK(fd2_field_game_item_target_is_legal(g) == 1); /* HP0/acted仍可选。 */
    CHECK(fd2_field_game_cycle_item_target(g) == 1 && g->magic_target_cycle == 3);
    CHECK(g->cursor_cell_x == 6 && g->cursor_cell_y == 5); /* 121占槽不focus。 */
    CHECK(fd2_field_game_cycle_item_target(g) == 1 && g->magic_target_cycle == 0);
    CHECK(g->cursor_cell_x == 6 && g->cursor_cell_y == 6 && g->focus_cycle_unit == 3);
    CHECK(!memcmp(&before, &g->units, sizeof(before)) && f.rng_calls == 0);
    close_fixture(&f); return 0;
}

static int test_first_121_and_no_target_slot(void) {
    fixture f; CHECK(init(&f) == 0);
    fd2_field_game *g = &f.game;
    fd2_field_unit actor = g->units.items[0];
    g->units.items[0] = g->units.items[6];
    g->units.items[0].x = 5;
    g->units.items[2] = actor; g->selected_unit = 2;
    CHECK(fd2_field_game_begin_item_targeting(g) == 0);
    CHECK(g->magic_target_candidates[0] == 0 && g->cursor_cell_x == 6);
    CHECK(fd2_field_game_cycle_item_target(g) == 1 && g->cursor_cell_x == 5);
    g->magic_target_count = 0; g->magic_target_cycle = 0;
    fd2_field_game before = *g;
    CHECK(fd2_field_game_cycle_item_target(g) == 0 && !memcmp(g, &before, sizeof(before)));
    CHECK(f.rng_calls == 0);
    close_fixture(&f); return 0;
}

static int test_outside_cancel_preserves_move_and_pending(void) {
    fixture f; CHECK(init(&f) == 0);
    fd2_field_game *g = &f.game;
    fd2_field_units before_units = g->units;
    uint8_t *path = g->move_path;
    CHECK(fd2_field_game_begin_item_targeting(g) == 0);
    CHECK(fd2_field_game_move_cursor(g, 0, 1) == 1);
    CHECK(fd2_field_game_item_target_is_legal(g) == 0); /* hidden actor不算。 */
    while (g->cursor_cell_x < 23) CHECK(fd2_field_game_move_cursor(g, 1, 0) == 1);
    while (g->cursor_cell_y < 23) CHECK(fd2_field_game_move_cursor(g, 0, 1) == 1);
    fd2_field_game before = *g;
    CHECK(fd2_field_game_item_target_is_legal(g) == 0);
    CHECK(fd2_field_game_confirm_cursor(g) == -1); /* 正式分派也拒绝范围外格。 */
    CHECK(!memcmp(g, &before, sizeof(before)));
    CHECK(fd2_field_game_cancel_selection(g) == 1);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_ITEM_LIST && g->item_list_selected == 0);
    CHECK(g->cursor_cell_x == 6 && g->cursor_cell_y == 6 && g->camera_cell_x == 5 && g->camera_cell_y == 5);
    CHECK(!g->move_range.nodes && !g->magic_target_count && !g->magic_target_cycle);
    CHECK(g->move_path == path && g->move_path_length == 2 && g->move_path[1] == 3);
    CHECK(g->move_preview_valid && g->move_origin_x == 4 && g->move_origin_direction == 1);
    CHECK(g->pending_cell_event.valid && g->pending_cell_event.action == 9 && g->pending_cell_event.source_slot == 4);
    CHECK(!memcmp(&before_units, &g->units, sizeof(before_units)) && !g->event_log_count && f.rng_calls == 0);
    close_fixture(&f); return 0;
}

static int test_rejections_are_atomic(void) {
    CHECK(fd2_field_game_begin_item_targeting(NULL) == -1);
    CHECK(fd2_field_game_item_target_is_legal(NULL) == -1);
    CHECK(fd2_field_game_cycle_item_target(NULL) == -1);
    CHECK(fd2_field_game_cancel_item_targeting(NULL) == -1);
    for (unsigned which = 0; which < 15; which++) {
        fixture f; CHECK(init(&f) == 0);
        fd2_field_game *g = &f.game;
        switch (which) {
            case 0: g->item_list_selected = 0; break; /* code0不是已支持草药。 */
            case 1: slot(&g->units.items[0], 2, 0, 255); break;
            case 2: g->item_list_selected = 3; break;
            case 3: g->interaction = FD2_FIELD_INTERACTION_COMMAND; break;
            case 4: g->units.items[0].flags = FD2_FIELD_UNIT_FLAG_ACTED; break;
            case 5: g->units.count = FD2_FIELD_MAX_UNITS + 1u; break;
            case 6: g->cursor_cell_x = 5; break;
            case 7: g->camera_cell_x = -1; break;
            case 8: g->map.cells = NULL; break;
            case 9: g->terrain.attr_count = 0; break;
            case 10: g->units.items[5].x = 255; break; /* 晚raw且不在范围。 */
            case 11: slot(&g->units.items[0], 1, 0x80, 132); break;
            case 12: slot(&g->units.items[0], 0, 0x40, 215); break;
            case 13: g->player_post_waiting_phase = 1; break;
            case 14: g->failure = FD2_FIELD_FAILURE_EVENT_DATA; break;
        }
        fd2_field_game before = *g;
        CHECK(fd2_field_game_begin_item_targeting(g) == -1);
        CHECK(!memcmp(g, &before, sizeof(before)) && g->move_range.nodes[17].distance == 77 && f.rng_calls == 0);
        close_fixture(&f);
    }
    return 0;
}

static int test_live_slot_and_load_owner(void) {
    fixture f; CHECK(init(&f) == 0);
    fd2_field_game *g = &f.game;
    fd2_save_battle_snapshot snapshot;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    CHECK(fd2_field_game_export_battle_snapshot(g, &snapshot) == 0);
    CHECK(fd2_field_game_begin_item_targeting(g) == 0);
    slot(&g->units.items[0], 2, 0, 255);
    fd2_field_game before = *g;
    CHECK(fd2_field_game_item_target_is_legal(g) == -1 && !memcmp(g, &before, sizeof(before)));
    CHECK(fd2_field_game_cycle_item_target(g) == -1 && !memcmp(g, &before, sizeof(before)));
    CHECK(fd2_field_game_cancel_item_targeting(g) == -1 && !memcmp(g, &before, sizeof(before)));
    slot(&g->units.items[0], 2, 0, 192);
    g->magic_target_candidates[1] = 255;
    before = *g;
    CHECK(fd2_field_game_cycle_item_target(g) == -1 && !memcmp(g, &before, sizeof(before)));
    snapshot.metadata[0] ^= 1;
    CHECK(import_record_snapshot(&f, &snapshot) == -1 && !memcmp(g, &before, sizeof(before)));
    snapshot.metadata[0] ^= 1;
    CHECK(import_record_snapshot(&f, &snapshot) == 0);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_BROWSE && !g->item_list_selected);
    CHECK(!g->magic_target_id && !g->magic_target_count && !g->magic_target_cycle);
    for (size_t i = 0; i < sizeof(g->magic_target_candidates); i++) CHECK(!g->magic_target_candidates[i]);
    CHECK(f.rng_calls == 0);
    close_fixture(&f); return 0;
}

static int test_frame1_and_range_lut(void) {
    fixture f; CHECK(init(&f) == 0);
    fd2_field_game *g = &f.game;
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
        for (size_t j = 0; j < 256; j++) g->visuals.range_luts[i][j] = j == 10 ? 42 : (uint8_t)j;
    CHECK(fd2_field_game_begin_item_targeting(g) == 0);
    fd2_vga vga = {0};
    fd2_field_game_render(g, &vga);
    CHECK(vga.framebuffer[(4 + 6 * 24) * VGA_W + 4 + 6 * 24] == 77);
    CHECK(vga.framebuffer[(4 + 6 * 24) * VGA_W + 4 + 5 * 24] == 42);
    CHECK(vga.framebuffer[(4 + 1 * 24) * VGA_W + 4 + 1 * 24] == 10);
    CHECK(!f.rng_calls && g->units.items[0].mp == 0);
    close_fixture(&f); return 0;
}

static int test_area_cursor_resources_and_cross_range_node_owner(void) {
    static const struct {uint8_t id, area;} cases[] = {{192,0},{210,1},{102,2},{79,4}};
    for (size_t n = 0; n < sizeof(cases)/sizeof(cases[0]); n++) {
        fixture f; CHECK(init(&f) == 0); fd2_field_game *g = &f.game;
        uint8_t tile_pixels[576], cursor_pixels[20][576];
        memset(tile_pixels, 10, sizeof(tile_pixels));
        fd2_image tile = {24,24,tile_pixels};
        g->terrain.frames = &tile; g->terrain.sheet.frame_count = 1;
        g->visuals.cursor_frame_count = 20;
        for (unsigned i = 0; i < 20; i++) {
            memset(cursor_pixels[i], 100+(int)i, 576);
            g->visuals.cursor_frames[i] = (fd2_image){24,24,cursor_pixels[i]};
        }
        slot(&g->units.items[0], 2, 0, cases[n].id);
        CHECK(fd2_field_game_begin_item_targeting(g) == 0);
        g->cursor_cell_x = 6; g->cursor_cell_y = 4;
        fd2_vga vga = {0}; fd2_field_game before = *g;
        fd2_field_game_render(g, &vga);
        CHECK(g->move_range.nodes == before.move_range.nodes && !f.rng_calls);
        unsigned center = vga.framebuffer[(4+4*24)*VGA_W+4+6*24];
        CHECK(center == (cases[n].area == 0 ? 101u : cases[n].area == 1 ? 114u : cases[n].area == 2 ? 101u : 10u));
        if (cases[n].area == 1) {
            CHECK(vga.framebuffer[(4+3*24)*VGA_W+4+6*24] == 102);
            CHECK(vga.framebuffer[(4+4*24)*VGA_W+4+5*24] == 103);
            CHECK(vga.framebuffer[(4+4*24)*VGA_W+4+7*24] == 104);
            CHECK(vga.framebuffer[(4+5*24)*VGA_W+4+6*24] == 105);
        }
        if (cases[n].area == 2) {
            CHECK(vga.framebuffer[(4+2*24)*VGA_W+4+6*24] == 102);
            CHECK(vga.framebuffer[(4+3*24)*VGA_W+4+5*24] == 106);
            CHECK(vga.framebuffer[(4+3*24)*VGA_W+4+6*24] == 110);
            CHECK(vga.framebuffer[(4+4*24)*VGA_W+4+5*24] == 111);
        }
        if (cases[n].area == 4) {
            CHECK(fd2_field_path_is_destination(&g->move_range, 6,20));
            CHECK(fd2_field_path_is_destination(&g->move_range, 20,6));
            CHECK(!fd2_field_path_is_destination(&g->move_range, 7,7));
            g->cursor_cell_x = g->cursor_cell_y = 6;
            CHECK(fd2_field_game_move_cursor(g, 1,0) == 1 && fd2_field_game_move_cursor(g, 0,1) == 1);
            CHECK(fd2_field_path_is_destination(&g->move_range, 7,7));
            CHECK(fd2_field_game_item_target_is_legal(g) == 1); /* area4附近enemy，而非初始十字永久拒绝。 */
        }
        close_fixture(&f);
    }
    return 0;
}

static int test_teleport_first_stage_excludes_whole_caster_cell(void) {
    for (int neighbor = 0; neighbor < 2; neighbor++) {
        fixture f; CHECK(init(&f) == 0); fd2_field_game *g = &f.game;
        g->units.count = 2; g->units.items[1].x = neighbor ? 7 : 6; g->units.items[1].y = 6;
        slot(&g->units.items[0], 2, 0, 101);
        CHECK(fd2_field_game_begin_item_targeting(g) == 0);
        CHECK(g->magic_target_count == (neighbor ? 1u : 0u));
        CHECK(!fd2_field_path_is_destination(&g->move_range, 6,6));
        CHECK(fd2_field_game_item_target_is_legal(g) == neighbor);
        if (neighbor) CHECK(g->magic_target_candidates[0] == 1 && g->cursor_cell_x == 7);
        else {
            fd2_field_game before = *g;
            CHECK(fd2_field_game_resolve_player_item(g) == -1 && !memcmp(&before,g,sizeof(before)) && !f.rng_calls);
        }
        close_fixture(&f);
    }
    fixture f; CHECK(init(&f) == 0);
    f.game.units.count = 2; f.game.units.items[1].x = f.game.units.items[1].y = 6;
    CHECK(fd2_field_game_begin_item_targeting(&f.game) == 0);
    CHECK(fd2_field_path_is_destination(&f.game.move_range, 6,6) && fd2_field_game_item_target_is_legal(&f.game) == 1);
    close_fixture(&f); return 0;
}

int main(void) {
    CHECK(test_teleport_first_stage_excludes_whole_caster_cell() == 0);
    CHECK(test_area_cursor_resources_and_cross_range_node_owner() == 0);
    CHECK(test_player_filter_raw_order_and_cycle() == 0);
    CHECK(test_first_121_and_no_target_slot() == 0);
    CHECK(test_outside_cancel_preserves_move_and_pending() == 0);
    CHECK(test_rejections_are_atomic() == 0);
    CHECK(test_live_slot_and_load_owner() == 0);
    CHECK(test_frame1_and_range_lut() == 0);
    puts("field item target tests: ok");
    return 0;
}
