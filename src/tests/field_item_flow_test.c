/* field_player_item_command_execute @0x1bbdc（原登记同名）、
 * field_unit_inventory_select @0x1b932（原sub_1B932）的纯session回归。
 * Drop尾另依据field_unit_inventory_slot_remove @0x1b8e7（原sub_1B8E7）
 * 与field_unit_combat_stats_recompute @0x1b750（原登记同名）。
 * 不执行等待、Use handler或图形入口；合成资源capacity只供Load预检。 */
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

/* 合成map只用于选择查询；本Load段只验记录与临时owner，不声称地图重载。
 * 调用期间显式移出map owner，并在成功/失败后归还fixture借用指针。 */
static int import_record_snapshot(fixture *f, const fd2_save_battle_snapshot *snapshot) {
    uint32_t *cells = f->game.map.cells;
    f->game.map.cells = NULL;
    int result = fd2_field_game_import_battle_snapshot(&f->game, snapshot);
    f->game.map.cells = cells;
    return result;
}

static void expected_menu(fd2_field_game *g) {
    g->interaction = FD2_FIELD_INTERACTION_ITEM_MENU;
    g->item_list_selected = 0; g->command_selected = 0;
    g->command_disabled[0] = 0;
    g->command_disabled[1] = 1; g->command_disabled[2] = 0;
    g->command_disabled[3] = 0;
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
        slot(&g->units.items[0], 2, 0, 255); /* 未知记录不能借旧cached能力进入。 */
        expected = *g;
        CHECK(fd2_field_game_confirm_command(g) == -1);
        CHECK(!memcmp(g, &expected, sizeof(expected))); /* cached enabled也须重验。 */
        slot(&g->units.items[0], 2, 0, 192);
        expected = *g;
        CHECK(fd2_field_game_begin_item_menu(g) == 0);
        expected_menu(&expected);
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        for (unsigned direction = 0; direction < 2; direction++) {
            CHECK(fd2_field_game_select_item_menu_direction(g, (fd2_field_command_direction)direction) == 0);
            CHECK(!memcmp(g, &expected, sizeof(expected))); /* UP保持Use，LEFT的Give无邻格禁用。 */
        }
        CHECK(fd2_field_game_select_item_menu_direction(g, FD2_FIELD_COMMAND_DIRECTION_DOWN) == 1);
        expected.command_selected = 3;
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        CHECK(fd2_field_game_select_item_menu_direction(g, FD2_FIELD_COMMAND_DIRECTION_UP) == 1);
        expected.command_selected = 0;
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
        case 11: g->stage = 2; break; /* B5已支持0/1，2仍未开放。 */
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
        g->command_disabled[selected] = 1;
        before = *g;
        CHECK(fd2_field_game_confirm_item_menu(g) == -1 && !memcmp(g, &before, sizeof(before)));
    }
    g->command_selected = 0;
    g->command_disabled[0] = 0; g->command_disabled[3] = 0;
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
    g->item_give_recipient = 1; g->item_give_selected = 6; g->item_transfer_committed = 1;
    CHECK(fd2_field_game_export_battle_snapshot(g, &another) == 0);
    CHECK(!memcmp(&snapshot, &another, sizeof(snapshot)));
    CHECK(fd2_field_game_begin_item_menu(g) == 0);
    CHECK(fd2_field_game_confirm_item_menu(g) == 0);
    g->item_list_selected = 2;
    fd2_field_game before = *g;
    snapshot.metadata[0] ^= 1;
    CHECK(import_record_snapshot(&f, &snapshot) == -1);
    CHECK(!memcmp(g, &before, sizeof(before)));
    CHECK(g->move_range.nodes[17].distance == 7 && g->move_path[1] == 3);
    snapshot.metadata[0] ^= 1;
    CHECK(import_record_snapshot(&f, &snapshot) == 0);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_BROWSE && g->selected_unit == -1 && g->item_list_selected == 0);
    CHECK(g->command_selected == -1 && g->move_path_length == 0 && !g->move_range.nodes && !g->pending_cell_event.valid);
    CHECK(!g->item_transfer_committed && !g->item_give_recipient && !g->item_give_selected);
    CHECK(!memcmp(g->units.items, before.units.items, sizeof(g->units.items)) && f.rng_calls == 0);
    close_fixture(&f);
    return 0;
}

static int open_drop(fixture *f) {
    fd2_field_game *g = &f->game;
    fd2_field_unit *actor = &g->units.items[0];
    actor->base_attack_le[0] = 6;
    actor->base_defense_le[0] = 4;
    actor->dexterity_le[0] = 2;
    actor->attack = actor->defense = actor->accuracy = actor->evasion = 999;
    actor->experience = 99; actor->hp = 20; actor->mp = 7; actor->mp_max = 9;
    CHECK(fd2_field_game_begin_item_menu(g) == 0);
    CHECK(g->command_disabled[3] == 0);
    g->command_selected = 3;
    CHECK(fd2_field_game_confirm_item_menu(g) == 0);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_ITEM_LIST && g->command_selected == 3);
    CHECK(g->item_list_selected == 0);
    return 0;
}

static int test_drop_cancel_recomputes_without_rollback(void) {
    for (int through_cancel = 0; through_cancel < 2; through_cancel++) {
        fixture f;
        CHECK(init(&f, 0) == 0 && open_drop(&f) == 0);
        fd2_field_game *g = &f.game;
        CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_CONFIRM) == 1);
        CHECK(fd2_field_game_begin_item_targeting(g) == -1); /* mode0不走Use目标。 */
        CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_DOWN) == 0);
        CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_CONFIRM) == 1); /* code0装备可确认。 */
        g->attack_rng = NULL; /* 纯Drop不借正式Use或共享等待RNG门。 */
        fd2_field_game expected = *g;
        expected.units.items[0].attack = 16; expected.units.items[0].defense = 12;
        expected.units.items[0].accuracy = 97; expected.units.items[0].evasion = 2;
        expected_menu(&expected);
        CHECK((through_cancel ? fd2_field_game_cancel_selection(g)
                              : fd2_field_game_end_item_list(g)) == (through_cancel ? 1 : 0));
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        CHECK(g->move_range.nodes[17].distance == 7 && g->move_path[1] == 3 && f.rng_calls == 0);
        CHECK(fd2_field_game_end_item_list(g) == -1);
        CHECK(fd2_field_game_resolve_item_drop(g) == -1);
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        close_fixture(&f);
    }
    return 0;
}

static int test_drop_removal_equipment_and_stale_tail(void) {
    for (unsigned selected = 0; selected < 3; selected++) {
        fixture f;
        CHECK(init(&f, 0) == 0 && open_drop(&f) == 0);
        fd2_field_game *g = &f.game;
        g->item_list_selected = (int)selected;
        fd2_field_game expected = *g;
        uint8_t *raw = (uint8_t *)&expected.units.items[0];
        memmove(raw + 0x0a + selected * 2u, raw + 0x0c + selected * 2u, (7u - selected) * 2u);
        raw[0x18] = 0x80;
        expected.units.items[0].attack = selected == 0 ? 6 : 16;
        expected.units.items[0].defense = selected == 1 ? 4 : 12;
        expected.units.items[0].accuracy = selected == 0 ? 2 : 97;
        expected.units.items[0].evasion = 2;
        expected_menu(&expected);
        CHECK(fd2_field_game_resolve_item_drop(g) == 0);
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        CHECK(((const uint8_t *)&g->units.items[0])[0x19] == 0xa5); /* 不清末ID。 */
        CHECK(fd2_field_game_resolve_item_drop(g) == -1 && !memcmp(g, &expected, sizeof(expected)));
        CHECK(f.rng_calls == 0 && g->move_range.nodes[17].distance == 7 && g->move_path[1] == 3);
        close_fixture(&f);
    }
    fixture f;
    CHECK(init(&f, 0) == 0);
    slot(&f.game.units.items[0], 2, 0, 193); /* 非零未知Use handler不影响纯Drop。 */
    CHECK(open_drop(&f) == 0);
    f.game.item_list_selected = 2;
    CHECK(fd2_field_game_resolve_item_drop(&f.game) == 0);
    CHECK(fd2_field_unit_inventory_count(&f.game.units.items[0]) == 2 && f.rng_calls == 0);
    close_fixture(&f);
    CHECK(init(&f, 0) == 0);
    for (unsigned i = 3; i < 8; i++) slot(&f.game.units.items[0], i, 0, 192);
    CHECK(open_drop(&f) == 0);
    f.game.item_list_selected = 7;
    CHECK(fd2_field_game_resolve_item_drop(&f.game) == 0);
    const uint8_t *raw = (const uint8_t *)&f.game.units.items[0];
    CHECK(fd2_field_unit_inventory_count(&f.game.units.items[0]) == 7);
    CHECK(raw[0x18] == 0x80 && raw[0x19] == 192 && raw[0x16] == 0 && raw[0x17] == 192);
    CHECK(f.game.units.items[0].attack == 16 && f.game.units.items[0].accuracy == 97 && f.rng_calls == 0);
    close_fixture(&f);
    return 0;
}

static int test_drop_last_slot_returns_command(void) {
    fixture f;
    CHECK(init(&f, 0) == 0);
    for (unsigned i = 1; i < 8; i++) slot(&f.game.units.items[0], i, 0x80, 0xa5);
    CHECK(open_drop(&f) == 0);
    fd2_field_game *g = &f.game;
    fd2_field_game before = *g;
    CHECK(fd2_field_game_resolve_item_drop(g) == 0);
    CHECK(fd2_field_unit_inventory_count(&g->units.items[0]) == 0);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_COMMAND && g->item_list_selected == 0);
    CHECK(g->command_selected == FD2_FIELD_COMMAND_WAIT && g->command_disabled[2]);
    CHECK(g->units.items[0].attack == 6 && g->units.items[0].defense == 4 &&
          g->units.items[0].accuracy == 2 && g->units.items[0].evasion == 2);
    CHECK(((const uint8_t *)&g->units.items[0])[0x19] == 0xa5);
    before.units.items[0] = g->units.items[0];
    before.interaction = g->interaction; before.item_list_selected = 0;
    before.command_selected = g->command_selected;
    memcpy(before.command_disabled, g->command_disabled, sizeof(before.command_disabled));
    before.command_highlight_phase = before.command_animation_phase = before.command_animation_opening = 0;
    before.last_command_highlight_ms = 0;
    CHECK(!memcmp(g, &before, sizeof(before)) && f.rng_calls == 0);
    CHECK(fd2_field_game_resolve_item_drop(g) == -1 && !memcmp(g, &before, sizeof(before)));
    close_fixture(&f);
    return 0;
}

static int test_drop_rejects_without_actor_or_owner_writes(void) {
    CHECK(fd2_field_game_resolve_item_drop(NULL) == -1);
    for (unsigned which = 0; which < 20; which++) {
        fixture f;
        CHECK(init(&f, 0) == 0 && open_drop(&f) == 0);
        fd2_field_game *g = &f.game;
        if (which < 15) invalidate(g, which);
        else if (which == 15) g->command_selected = 0;
        else if (which == 16) g->item_list_selected = -1;
        else if (which == 17) g->item_list_selected = 3;
        else if (which == 18) g->interaction = FD2_FIELD_INTERACTION_COMMAND;
        else slot(&g->units.items[0], 7, 0xc0, 255); /* 隐藏但equipped，重算晚错。 */
        fd2_field_game before = *g;
        CHECK(fd2_field_game_resolve_item_drop(g) == -1);
        CHECK(!memcmp(g, &before, sizeof(before)));
        if (which != 15) {
            CHECK(fd2_field_game_end_item_list(g) == -1);
            CHECK(!memcmp(g, &before, sizeof(before)));
        }
        CHECK(f.rng_calls == 0);
        close_fixture(&f);
    }
    return 0;
}

static int test_formal_use_drop_capabilities_and_cached_rechecks(void) {
    fixture f;
    CHECK(init(&f, 0) == 0);
    fd2_field_game *g = &f.game;
    fd2_field_game before = *g;
    CHECK(fd2_field_game_item_use_is_available(g) && fd2_field_game_item_drop_is_available(g));
    CHECK(!memcmp(g, &before, sizeof(before)) && !f.rng_calls);
    slot(&g->units.items[0], 2, 0, 193);
    CHECK(fd2_field_game_item_use_is_available(g) && fd2_field_game_item_drop_is_available(g));
    CHECK(fd2_field_game_item_is_available(g));
    CHECK(fd2_field_game_begin_item_menu(g) == 0);
    CHECK(g->command_selected == 0 && !g->command_disabled[0] && !g->command_disabled[2] && !g->command_disabled[3]);
    g->command_selected = 0;
    before = *g;
    slot(&g->units.items[0], 2, 0, 255); /* 未知ID不能借cached能力确认。 */
    before = *g;
    CHECK(fd2_field_game_confirm_cursor(g) == -1 && !memcmp(g, &before, sizeof(before)));
    slot(&g->units.items[0], 2, 0, 193);
    g->command_selected = 3;
    g->attack_rng = NULL;
    before = *g;
    CHECK(!fd2_field_game_item_is_available(g) && !fd2_field_game_item_drop_is_available(g));
    CHECK(fd2_field_game_confirm_cursor(g) == -1 && !memcmp(g, &before, sizeof(before)));
    CHECK(fd2_field_game_confirm_item_menu(g) == 0); /* 纯步进不执行真实等待。 */
    g->item_list_selected = 2;
    CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_CONFIRM) == 1);
    CHECK(fd2_field_game_resolve_item_drop(g) == 0 && g->command_selected == 0);
    CHECK(!g->command_disabled[0] && !g->command_disabled[3] && !f.rng_calls);
    close_fixture(&f);

    CHECK(init(&f, 0) == 0);
    slot(&f.game.units.items[0], 7, 0xc0, 255);
    CHECK(fd2_field_game_item_use_is_available(&f.game) && !fd2_field_game_item_drop_is_available(&f.game));
    CHECK(fd2_field_game_begin_item_menu(&f.game) == 0 && f.game.command_disabled[3]);
    f.game.command_selected = 3; f.game.command_disabled[3] = 0;
    before = f.game;
    CHECK(fd2_field_game_confirm_cursor(&f.game) == -1 && !memcmp(&f.game, &before, sizeof(before)));
    slot(&f.game.units.items[0], 2, 0, 255);
    CHECK(!fd2_field_game_item_is_available(&f.game)); /* Use与Drop的失败分别独立。 */
    close_fixture(&f);
    CHECK(!fd2_field_game_item_use_is_available(NULL) && !fd2_field_game_item_drop_is_available(NULL));
    return 0;
}

static int test_drop_inventory_survives_outer_movement_cancel(void) {
    fixture f;
    CHECK(init(&f, 0) == 0);
    f.game.units.items[0].movement_points = 3; /* 真正重开移动范围的合法预算。 */
    CHECK(open_drop(&f) == 0);
    fd2_field_game *g = &f.game;
    g->item_list_selected = 0; /* 删除装备武器，随后取消临时移动。 */
    CHECK(fd2_field_game_resolve_item_drop(g) == 0);
    fd2_field_unit dropped = g->units.items[0];
    CHECK(fd2_field_game_end_item_menu(g) == 0);
    CHECK(fd2_field_game_cancel_selection(g) == 1);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED);
    CHECK(g->units.items[0].x == 2 && g->units.items[0].y == 3 && g->units.items[0].direction == 1);
    dropped.x = 2; dropped.y = 3; dropped.direction = 1; dropped.frame_phase = 0;
    CHECK(!memcmp(&g->units.items[0], &dropped, sizeof(dropped)) && !f.rng_calls);
    CHECK(!g->pending_cell_event.valid && !fd2_field_unit_has_acted(&g->units.items[0]));
    close_fixture(&f);
    return 0;
}

static int open_eqt(fixture *f, int second_weapon) {
    CHECK(init(f, 0) == 0);
    fd2_field_game *g = &f->game;
    fd2_field_unit *actor = &g->units.items[0];
    actor->movement_profile = 1;
    actor->base_attack_le[0] = 6; actor->base_defense_le[0] = 4; actor->dexterity_le[0] = 2;
    actor->attack = actor->defense = actor->accuracy = actor->evasion = 999;
    actor->experience = 99; actor->hp = 20; actor->mp = 7; actor->mp_max = 9;
    if (second_weapon) slot(actor, 2, 0, 1);
    CHECK(fd2_field_game_begin_item_menu(g) == 0 && !g->command_disabled[2]);
    g->command_selected = 2; g->command_disabled[2] = 0;
    CHECK(fd2_field_game_confirm_cursor(g) == 0); /* 正式EQT与纯session共用同一owner。 */
    CHECK(g->interaction == FD2_FIELD_INTERACTION_ITEM_LIST && g->command_selected == 2);
    return 0;
}

static int test_eqt_replaces_and_stays_on_same_raw_slot(void) {
    fixture f;
    CHECK(open_eqt(&f, 1) == 0);
    fd2_field_game *g = &f.game;
    g->attack_rng = NULL;
    CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_DOWN) == 0);
    CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_DOWN) == 0 && g->item_list_selected == 2);
    CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_CONFIRM) == 1);
    fd2_field_game expected = *g;
    slot(&expected.units.items[0], 0, 0, 0); slot(&expected.units.items[0], 2, 0x40, 1);
    expected.units.items[0].attack = 26; expected.units.items[0].defense = 12;
    expected.units.items[0].accuracy = 97; expected.units.items[0].evasion = 2;
    CHECK(fd2_field_game_equip_item(g) == 1 && !memcmp(g, &expected, sizeof(expected)));
    CHECK(fd2_field_game_equip_item(g) == 1 && !memcmp(g, &expected, sizeof(expected))); /* 非toggle。 */
    CHECK(g->move_range.nodes[17].distance == 7 && g->move_path[1] == 3 && !f.rng_calls);
    close_fixture(&f);
    return 0;
}

static int test_eqt_incompatible_and_cancel_do_not_recompute(void) {
    for (int cancel = 0; cancel < 2; cancel++) {
        fixture f;
        CHECK(open_eqt(&f, 0) == 0);
        fd2_field_game *g = &f.game;
        g->item_list_selected = 2;
        g->attack_rng = NULL;
        fd2_field_game expected = *g;
        CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_CONFIRM) == 1); /* mode0接受草药。 */
        CHECK(fd2_field_game_equip_item(g) == 0 && !memcmp(g, &expected, sizeof(expected)));
        CHECK(fd2_field_game_equip_item(g) == 0 && !memcmp(g, &expected, sizeof(expected)));
        CHECK((cancel ? fd2_field_game_cancel_selection(g) : fd2_field_game_end_item_list(g)) == (cancel ? 1 : 0));
        expected_menu(&expected);
        CHECK(!memcmp(g, &expected, sizeof(expected)) && g->units.items[0].attack == 999 && !f.rng_calls);
        close_fixture(&f);
    }
    return 0;
}

static int test_eqt_grouping_uses_item_id_and_whole_flag_byte(void) {
    for (int armor = 0; armor < 2; armor++) {
        fixture f;
        CHECK(open_eqt(&f, 1) == 0);
        fd2_field_game *g = &f.game;
        slot(&g->units.items[0], 0, 0x65, 127); /* 127/128同category21但分组不同。 */
        slot(&g->units.items[0], 1, 0x44, 132);
        slot(&g->units.items[0], 2, 0x20, armor ? 128 : 1);
        g->item_list_selected = 2;
        fd2_field_game expected = *g;
        slot(&expected.units.items[0], armor ? 1 : 0, 0, armor ? 132 : 127);
        slot(&expected.units.items[0], 2, 0x40, armor ? 128 : 1);
        expected.units.items[0].attack = armor ? 6 : 26;
        expected.units.items[0].defense = armor ? 46 : 12;
        expected.units.items[0].accuracy = armor ? 2 : 97;
        expected.units.items[0].evasion = armor ? 7 : 2;
        CHECK(fd2_field_game_equip_item(g) == 1 && !memcmp(g, &expected, sizeof(expected)) && !f.rng_calls);
        close_fixture(&f);
    }
    return 0;
}

static int test_eqt_hidden_equipped_wakeup_requires_dense_known_result(void) {
    for (int contiguous = 0; contiguous < 2; contiguous++) {
        fixture f;
        CHECK(open_eqt(&f, 1) == 0);
        fd2_field_game *g = &f.game;
        slot(&g->units.items[0], contiguous ? 3 : 7, 0xc0, 0);
        g->item_list_selected = 2;
        fd2_field_game expected = *g;
        if (contiguous) {
            slot(&expected.units.items[0], 0, 0, 0);
            slot(&expected.units.items[0], 2, 0x40, 1);
            slot(&expected.units.items[0], 3, 0, 0);
            expected.units.items[0].attack = 26; expected.units.items[0].defense = 12;
            expected.units.items[0].accuracy = 97; expected.units.items[0].evasion = 2;
        }
        CHECK(fd2_field_game_equip_item(g) == (contiguous ? 1 : -1));
        CHECK(!memcmp(g, &expected, sizeof(expected)) && !f.rng_calls);
        CHECK(fd2_field_unit_inventory_count(&g->units.items[0]) == (contiguous ? 4u : 3u));
        close_fixture(&f);
    }
    return 0;
}

static int test_eqt_rejections_are_atomic(void) {
    CHECK(fd2_field_game_equip_item(NULL) == -1);
    for (unsigned which = 0; which < 21; which++) {
        fixture f;
        CHECK(open_eqt(&f, 1) == 0);
        fd2_field_game *g = &f.game;
        g->item_list_selected = 2;
        if (which < 15) invalidate(g, which);
        else if (which == 15) g->units.items[0].movement_profile = 29;
        else if (which == 16) g->item_list_selected = -1;
        else if (which == 17) g->item_list_selected = 3;
        else if (which == 18) g->command_selected = 0;
        else if (which == 19) g->interaction = FD2_FIELD_INTERACTION_ITEM_MENU;
        else {
            slot(&g->units.items[0], 2, 0, 132);
            slot(&g->units.items[0], 7, 0xc0, 255); /* 同组会清hidden，源lookup也必须先拒。 */
        }
        fd2_field_game before = *g;
        CHECK(fd2_field_game_equip_item(g) == -1 && !memcmp(g, &before, sizeof(before)) && !f.rng_calls);
        close_fixture(&f);
    }
    return 0;
}

static int test_eqt_flags_and_stats_survive_outer_move_cancel(void) {
    fixture f;
    CHECK(open_eqt(&f, 1) == 0);
    fd2_field_game *g = &f.game;
    g->units.items[0].movement_points = 3;
    g->item_list_selected = 2;
    CHECK(fd2_field_game_equip_item(g) == 1);
    fd2_field_unit equipped = g->units.items[0];
    CHECK(fd2_field_game_end_item_list(g) == 0 && fd2_field_game_end_item_menu(g) == 0);
    CHECK(fd2_field_game_cancel_selection(g) == 1 && g->interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED);
    equipped.x = 2; equipped.y = 3; equipped.direction = 1; equipped.frame_phase = 0;
    CHECK(!memcmp(&g->units.items[0], &equipped, sizeof(equipped)) && !f.rng_calls);
    close_fixture(&f);
    return 0;
}

static int open_give(fixture *f, int full) {
    CHECK(init(f, 0) == 0);
    fd2_field_game *g = &f->game;
    fd2_field_unit *receiver = &g->units.items[1];
    receiver->side = 2; receiver->x = 5; receiver->y = 3;
    receiver->attack = 777; receiver->defense = 888;
    for (unsigned i = 0; i < 8; i++) slot(receiver, i, full ? 0 : 0x80, (uint8_t)(full ? i : 200+i));
    CHECK(fd2_field_game_item_give_is_available(g));
    CHECK(fd2_field_game_begin_item_menu(g) == 0 && !g->command_disabled[1]);
    g->command_selected = 1;
    CHECK(fd2_field_game_confirm_item_menu(g) == 0);
    g->item_list_selected = 2;
    CHECK(fd2_field_game_begin_item_give(g) == 0);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING &&
        g->cursor_cell_x == 5 && g->cursor_cell_y == 3);
    return 0;
}

static int test_give_empty_and_sticky_action_tail(void) {
    fixture f; CHECK(open_give(&f, 0) == 0);
    fd2_field_game *g = &f.game;
    fd2_field_unit receiver = g->units.items[1];
    slot(&receiver, 0, 0, 192);
    CHECK(fd2_field_game_confirm_cursor(g) == 0);
    CHECK(!memcmp(&receiver, &g->units.items[1], sizeof(receiver)));
    CHECK(fd2_field_unit_inventory_count(&g->units.items[0]) == 2 && g->item_transfer_committed == 1);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_ITEM_MENU && !fd2_field_unit_has_acted(&g->units.items[0]));
    CHECK(g->move_path_length == 2 && g->pending_cell_event.valid && !f.rng_calls);
    /* 后续Give取消/Drop取消均不能抹掉先前53C53=1。 */
    g->command_selected = 1;
    CHECK(fd2_field_game_confirm_item_menu(g) == 0);
    CHECK(fd2_field_game_end_item_list(g) == 0 && g->item_transfer_committed);
    g->command_selected = 3;
    CHECK(fd2_field_game_confirm_item_menu(g) == 0 && fd2_field_game_end_item_list(g) == 0 && g->item_transfer_committed);
    CHECK(fd2_field_game_end_item_menu(g) == 0);
    g->pending_cell_event = (fd2_field_pending_cell_event){0};
    CHECK(fd2_field_game_cancel_selection(g) == 1);
    CHECK(g->units.items[0].x == 4 && g->units.items[0].y == 3 && g->units.items[0].direction == 2);
    CHECK(fd2_field_unit_has_acted(&g->units.items[0]) && !f.rng_calls && g->selected_unit == -1);
    CHECK(fd2_field_unit_inventory_count(&g->units.items[0]) == 2 && g->item_transfer_committed);
    close_fixture(&f); return 0;
}

static int test_give_full_exchange_is_remove_append_sender_only_stats(void) {
    fixture f; CHECK(open_give(&f, 1) == 0);
    fd2_field_game *g = &f.game;
    slot(&g->units.items[1], 4, 0x40, 4);
    CHECK(fd2_field_game_resolve_item_give(g) == 0 && g->interaction == FD2_FIELD_INTERACTION_ITEM_GIVE_LIST);
    CHECK(g->item_give_selected == 0 && !g->item_transfer_committed && g->cursor_cell_x == 4);
    g->item_give_selected = 4;
    CHECK(fd2_field_game_item_give_list_step(g, FD2_FIELD_ITEM_LIST_KEY_CONFIRM) == 1);
    CHECK(fd2_field_game_resolve_item_give(g) == 0);
    const uint8_t *raw = (const uint8_t *)&g->units.items[1];
    CHECK(raw[0x0b+4*2] == 5 && raw[0x0b+7*2] == 192 && raw[0x0a+7*2] == 0);
    CHECK(g->units.items[1].attack == 777 && g->units.items[1].defense == 888);
    raw = (const uint8_t *)&g->units.items[0];
    CHECK(raw[0x0f] == 4 && raw[0x0e] == 0 && g->item_transfer_committed);
    CHECK(!fd2_field_unit_has_acted(&g->units.items[0]) && !f.rng_calls);
    close_fixture(&f); return 0;
}

static int test_give_cancels_and_late_errors_are_atomic(void) {
    for (int full = 0; full < 2; full++) {
        fixture f; CHECK(open_give(&f, full) == 0);
        fd2_field_game *g = &f.game;
        fd2_field_unit recipient = g->units.items[1];
        if (full) CHECK(fd2_field_game_resolve_item_give(g) == 0);
        g->units.items[0].attack = 999;
        CHECK(fd2_field_game_cancel_item_give(g) == 0);
        CHECK(g->interaction == FD2_FIELD_INTERACTION_ITEM_MENU && !g->item_transfer_committed);
        CHECK(g->units.items[0].attack != 999 && !memcmp(&recipient, &g->units.items[1], sizeof(recipient)) && !f.rng_calls);
        close_fixture(&f);
    }
    fixture f; CHECK(open_give(&f, 1) == 0);
    CHECK(fd2_field_game_resolve_item_give(&f.game) == 0);
    slot(&f.game.units.items[1], 7, 0, 255);
    fd2_field_game before = f.game;
    CHECK(fd2_field_game_resolve_item_give(&f.game) == -1 && !memcmp(&before, &f.game, sizeof(before)) && !f.rng_calls);
    close_fixture(&f); return 0;
}

static int test_give_stack_first_visible_and_candidate_rules(void) {
    fixture f; CHECK(open_give(&f, 0) == 0);
    fd2_field_game *g = &f.game;
    g->units.count = 3;
    g->units.items[2] = g->units.items[1];
    g->units.items[1].side = 0; /* 14742有side2，但12c0d取先出现的enemy。 */
    CHECK(fd2_field_game_resolve_item_give(g) == 0);
    CHECK(fd2_field_unit_inventory_count(&g->units.items[1]) == 1 && !fd2_field_unit_inventory_count(&g->units.items[2]));
    close_fixture(&f);
    CHECK(init(&f, 0) == 0); g = &f.game;
    g->units.items[1].side = 2; g->units.items[1].x = 4; g->units.items[1].y = 3;
    CHECK(!fd2_field_game_item_give_is_available(g)); /* 中心整格排除。 */
    g->units.items[1].x = 5; g->units.items[1].hp = 0; g->units.items[1].flags = 0x80; g->units.items[1].unit_id = 121;
    CHECK(fd2_field_game_item_give_is_available(g));
    CHECK(fd2_field_game_begin_item_menu(g) == 0); g->command_selected = 1;
    CHECK(fd2_field_game_confirm_item_menu(g) == 0); g->item_list_selected = 2;
    CHECK(fd2_field_game_begin_item_give(g) == 0 && g->magic_target_count == 1 && g->cursor_cell_x == 4);
    CHECK(fd2_field_game_cycle_item_give_target(g) == 1 && g->cursor_cell_x == 4 && !f.rng_calls);
    close_fixture(&f); return 0;
}

int main(void) {
    CHECK(test_give_empty_and_sticky_action_tail() == 0);
    CHECK(test_give_full_exchange_is_remove_append_sender_only_stats() == 0);
    CHECK(test_give_cancels_and_late_errors_are_atomic() == 0);
    CHECK(test_give_stack_first_visible_and_candidate_rules() == 0);
    CHECK(test_layers_preserve_move_pending_units_and_rng() == 0);
    CHECK(test_rejections_have_no_write_set() == 0);
    CHECK(test_load_and_snapshot_owner_boundaries() == 0);
    CHECK(test_drop_cancel_recomputes_without_rollback() == 0);
    CHECK(test_drop_removal_equipment_and_stale_tail() == 0);
    CHECK(test_drop_last_slot_returns_command() == 0);
    CHECK(test_drop_rejects_without_actor_or_owner_writes() == 0);
    CHECK(test_formal_use_drop_capabilities_and_cached_rechecks() == 0);
    CHECK(test_drop_inventory_survives_outer_movement_cancel() == 0);
    CHECK(test_eqt_replaces_and_stays_on_same_raw_slot() == 0);
    CHECK(test_eqt_incompatible_and_cancel_do_not_recompute() == 0);
    CHECK(test_eqt_grouping_uses_item_id_and_whole_flag_byte() == 0);
    CHECK(test_eqt_hidden_equipped_wakeup_requires_dense_known_result() == 0);
    CHECK(test_eqt_rejections_are_atomic() == 0);
    CHECK(test_eqt_flags_and_stats_survive_outer_move_cancel() == 0);
    puts("field_item_flow_test: ok");
    return 0;
}
