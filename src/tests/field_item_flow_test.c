/* field_player_item_command_execute @0x1bbdc（原登记同名）、
 * field_unit_inventory_select @0x1b932（原sub_1B932）的纯session回归。
 * 不执行等待、物品handler或图形入口；合成资源capacity只供Load预检。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_game.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    return 1; } } while (0)

typedef struct {
    fd2_field_game game;
    uint32_t cells[24 * 24];
    fd2_terrain_attr attr;
    size_t rng_calls;
} fixture;

static uint32_t rng(void *data) { (*(size_t *)data)++; return 17; }

static void slot(fd2_field_unit *unit, unsigned index, uint8_t flags, uint8_t id) {
    uint8_t *raw = (uint8_t *)unit;
    raw[0x0a + index * 2] = flags;
    raw[0x0b + index * 2] = id;
}

static int init(fixture *f, int attack_available) {
    memset(f, 0, sizeof(*f));
    fd2_field_game *g = &f->game;
    g->ready = 1; g->active_side = 2; g->turn_number = 7;
    g->selected_unit = 0; g->detail_unit = -1; g->attack_target = -1;
    g->interaction = FD2_FIELD_INTERACTION_COMMAND;
    g->item_list_selected = 7;
    g->map.width = g->map.height = 24; g->map.cells = f->cells;
    g->terrain.attrs = &f->attr; g->terrain.attr_count = 1;
    g->sprites.frame_count = 140u * 12u; /* 合成capacity，不是假称已解码资源。 */
    g->camera_cell_x = 2; g->camera_cell_y = 1;
    g->cursor_cell_x = 4; g->cursor_cell_y = 3;
    g->units.count = 2;
    fd2_field_unit *actor = &g->units.items[0];
    actor->x = 4; actor->y = 3; actor->direction = 2;
    actor->side = 2; actor->movement_profile = 5;
    actor->hp = actor->hp_max = 28;
    actor->detail_status[2] = 1; /* +27禁法与MP0不禁止物品列表。 */
    for (unsigned i = 0; i < 8; i++) slot(actor, i, 0x80, 0xa5);
    slot(actor, 0, 0x40, 0); slot(actor, 1, 0x40, 132); slot(actor, 2, 0, 192);
    fd2_field_unit *enemy = &g->units.items[1];
    enemy->x = (uint8_t)(attack_available ? 5 : 12); enemy->y = 3;
    enemy->unit_id = 96; enemy->movement_profile = 7;
    enemy->hp = enemy->hp_max = 28;
    memset(g->metadata.turn_events, 0xff, sizeof(g->metadata.turn_events));
    memset(g->metadata.cell_lookup, 0xff, sizeof(g->metadata.cell_lookup));
    g->pending_cell_event = (fd2_field_pending_cell_event){1, 9, 4};
    g->phase_unit_cursor = 1; g->event_total_count = 3;
    g->attack_rng = rng; g->attack_rng_userdata = &f->rng_calls;
    g->move_path = malloc(4);
    g->move_range.nodes = calloc(24u * 24u, sizeof(*g->move_range.nodes));
    CHECK(g->move_path && g->move_range.nodes);
    memset(g->move_path, 3, 4);
    g->move_path_capacity = 4; g->move_path_length = 2;
    g->move_preview_valid = 1;
    g->move_origin_x = 2; g->move_origin_y = 3; g->move_origin_direction = 1;
    g->move_range.width = g->move_range.height = 24;
    g->move_range.nodes[17].distance = 7;
    CHECK(fd2_field_game_refresh_commands(g) == 0);
    CHECK(g->command_disabled[FD2_FIELD_COMMAND_ATTACK] == !attack_available);
    g->command_selected = FD2_FIELD_COMMAND_WAIT;
    g->command_highlight_phase = 1; g->command_animation_phase = 3;
    g->command_animation_opening = 1; g->last_command_highlight_ms = 912;
    return 0;
}

static void close_fixture(fixture *f) {
    fd2_field_metadata_close(&f->game.metadata);
    fd2_field_path_close(&f->game.move_range);
    free(f->game.move_path);
}

static void expected_menu(fd2_field_game *g) {
    g->interaction = FD2_FIELD_INTERACTION_ITEM_MENU;
    g->item_list_selected = 0; g->command_selected = 0;
    g->command_disabled[0] = 0;
    g->command_disabled[1] = g->command_disabled[2] = g->command_disabled[3] = 1;
    g->command_highlight_phase = g->command_animation_phase = g->command_animation_opening = 0;
    g->last_command_highlight_ms = 0;
}

static int test_layers_preserve_move_pending_units_and_rng(void) {
    for (int attack = 0; attack < 2; attack++) {
        fixture f;
        CHECK(init(&f, attack) == 0);
        fd2_field_game *g = &f.game;
        fd2_field_game expected = *g;
        g->command_selected = FD2_FIELD_COMMAND_ITEM;
        slot(&g->units.items[0], 2, 0, 193); /* 已知但未支持的非零Use code。 */
        expected = *g;
        CHECK(fd2_field_game_confirm_command(g) == -1);
        CHECK(!memcmp(g, &expected, sizeof(expected))); /* cached enabled也须重验。 */
        slot(&g->units.items[0], 2, 0, 192);
        expected = *g;
        CHECK(fd2_field_game_begin_item_menu(g) == 0);
        expected_menu(&expected);
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        for (unsigned direction = 0; direction < 4; direction++) {
            CHECK(fd2_field_game_select_item_menu_direction(g, (fd2_field_command_direction)direction) == 0);
            CHECK(!memcmp(g, &expected, sizeof(expected))); /* 仅Use；不给Equip/Drop假可用。 */
        }
        CHECK(fd2_field_game_begin_item_menu(g) == -1);
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        CHECK(fd2_field_game_confirm_item_menu(g) == 0);
        expected.interaction = FD2_FIELD_INTERACTION_ITEM_LIST;
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        CHECK(fd2_field_game_confirm_item_menu(g) == -1);
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_CONFIRM) == 0);
        CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_UP) == 0);
        expected.item_list_selected = 2;
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_CONFIRM) == 1);
        CHECK(!memcmp(g, &expected, sizeof(expected))); /* 只报告可确认，不消费药草。 */
        CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_CANCEL) == -1);
        CHECK(!memcmp(g, &expected, sizeof(expected))); /* step不负责关闭。 */
        CHECK((attack ? fd2_field_game_cancel_selection(g) : fd2_field_game_end_item_list(g)) == (attack ? 1 : 0));
        expected_menu(&expected);
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        CHECK(fd2_field_game_confirm_item_menu(g) == 0 && g->item_list_selected == 0);
        CHECK(fd2_field_game_end_item_list(g) == 0);
        CHECK((attack ? fd2_field_game_end_item_menu(g) : fd2_field_game_cancel_selection(g)) == (attack ? 0 : 1));
        expected.interaction = FD2_FIELD_INTERACTION_COMMAND;
        expected.command_disabled[0] = (uint8_t)!attack;
        expected.command_disabled[1] = 1;
        expected.command_disabled[2] = 0;
        expected.command_disabled[3] = 0;
        expected.command_selected = attack ? FD2_FIELD_COMMAND_ATTACK : FD2_FIELD_COMMAND_ITEM;
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        CHECK(g->move_range.nodes[17].distance == 7 && g->move_path_length == 2 && g->move_path[1] == 3);
        CHECK(g->pending_cell_event.valid && g->pending_cell_event.action == 9 && f.rng_calls == 0);
        CHECK(fd2_field_game_end_item_menu(g) == -1);
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        close_fixture(&f);
    }
    return 0;
}

static void invalidate(fd2_field_game *g, unsigned which) {
    switch (which) {
        case 0: g->ready = 0; break;
        case 1: g->failure = FD2_FIELD_FAILURE_EVENT_DATA; break;
        case 2: g->player_post_waiting_phase = 1; break;
        case 3: g->active_side = 1; break;
        case 4: g->units.count = FD2_FIELD_MAX_UNITS + 1u; break;
        case 5: g->selected_unit = -1; break;
        case 6: g->selected_unit = (int)g->units.count; break;
        case 7: g->units.items[0].side = 0; break;
        case 8: g->units.items[0].flags |= FD2_FIELD_UNIT_FLAG_HIDDEN; break;
        case 9: g->units.items[0].flags |= FD2_FIELD_UNIT_FLAG_ACTED; break;
        case 10: g->units.items[0].hp = 0; break;
        case 11: g->stage = 1; break;
        case 12: for (unsigned i = 0; i < 8; i++) slot(&g->units.items[0], i, 0x80, 0xff); break;
        case 13: slot(&g->units.items[0], 1, 0x80, 0xff); break;
        case 14: slot(&g->units.items[0], 2, 0, 255); break;
    }
}

static int test_rejections_have_no_write_set(void) {
    CHECK(fd2_field_game_begin_item_menu(NULL) == -1);
    CHECK(fd2_field_game_confirm_item_menu(NULL) == -1);
    CHECK(fd2_field_game_item_list_step(NULL, FD2_FIELD_ITEM_LIST_KEY_CANCEL) == -2);
    CHECK(fd2_field_game_end_item_list(NULL) == -1);
    CHECK(fd2_field_game_end_item_menu(NULL) == -1);
    for (unsigned which = 0; which < 15; which++) {
        fixture f;
        CHECK(init(&f, 0) == 0);
        fd2_field_game *g = &f.game;
        invalidate(g, which);
        fd2_field_game before = *g;
        CHECK(fd2_field_game_begin_item_menu(g) == -1 && !memcmp(g, &before, sizeof(before)));
        g->interaction = FD2_FIELD_INTERACTION_ITEM_MENU;
        before = *g;
        CHECK(fd2_field_game_confirm_item_menu(g) == -1);
        CHECK(fd2_field_game_select_item_menu_direction(g, FD2_FIELD_COMMAND_DIRECTION_LEFT) == -1);
        CHECK(fd2_field_game_end_item_menu(g) == -1 && !memcmp(g, &before, sizeof(before)));
        g->interaction = FD2_FIELD_INTERACTION_ITEM_LIST;
        before = *g;
        CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_CANCEL) == -2);
        CHECK(fd2_field_game_end_item_list(g) == -1 && !memcmp(g, &before, sizeof(before)));
        CHECK(f.rng_calls == 0);
        close_fixture(&f);
    }
    fixture f;
    CHECK(init(&f, 0) == 0);
    fd2_field_game *g = &f.game;
    CHECK(fd2_field_game_begin_item_menu(g) == 0);
    fd2_field_game before = *g;
    CHECK(fd2_field_game_select_item_menu_direction(g, (fd2_field_command_direction)-1) == -1);
    CHECK(!memcmp(g, &before, sizeof(before)));
    for (int selected = 1; selected < 4; selected++) {
        g->command_selected = selected;
        before = *g;
        CHECK(fd2_field_game_confirm_item_menu(g) == -1 && !memcmp(g, &before, sizeof(before)));
    }
    g->command_selected = 0;
    CHECK(fd2_field_game_confirm_item_menu(g) == 0);
    g->item_list_selected = 8;
    before = *g;
    CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_UP) == -2);
    CHECK(fd2_field_game_end_item_list(g) == -1 && !memcmp(g, &before, sizeof(before)));
    close_fixture(&f);
    return 0;
}

static int test_load_and_snapshot_owner_boundaries(void) {
    fixture f;
    CHECK(init(&f, 0) == 0);
    fd2_field_game *g = &f.game;
    fd2_save_battle_snapshot snapshot, another;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    CHECK(fd2_field_game_export_battle_snapshot(g, &snapshot) == 0);
    another = snapshot;
    g->item_list_selected = 2;
    CHECK(fd2_field_game_export_battle_snapshot(g, &another) == 0);
    CHECK(!memcmp(&snapshot, &another, sizeof(snapshot)));
    CHECK(fd2_field_game_begin_item_menu(g) == 0);
    CHECK(fd2_field_game_confirm_item_menu(g) == 0);
    g->item_list_selected = 2;
    fd2_field_game before = *g;
    snapshot.metadata[0] ^= 1;
    CHECK(fd2_field_game_import_battle_snapshot(g, &snapshot) == -1);
    CHECK(!memcmp(g, &before, sizeof(before)));
    CHECK(g->move_range.nodes[17].distance == 7 && g->move_path[1] == 3);
    snapshot.metadata[0] ^= 1;
    CHECK(fd2_field_game_import_battle_snapshot(g, &snapshot) == 0);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_BROWSE && g->selected_unit == -1 && g->item_list_selected == 0);
    CHECK(g->command_selected == -1 && g->move_path_length == 0 && !g->move_range.nodes && !g->pending_cell_event.valid);
    CHECK(!memcmp(g->units.items, before.units.items, sizeof(g->units.items)) && f.rng_calls == 0);
    close_fixture(&f);
    return 0;
}

int main(void) {
    CHECK(test_layers_preserve_move_pending_units_and_rng() == 0);
    CHECK(test_rejections_have_no_write_set() == 0);
    CHECK(test_load_and_snapshot_owner_boundaries() == 0);
    puts("field_item_flow_test: ok");
    return 0;
}
