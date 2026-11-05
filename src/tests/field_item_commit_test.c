/* sub_20C6F @0x20c6f（原登记同名）仅草药192/code5分支、
 * sub_211A4 @0x211a4与field_unit_inventory_slot_remove @0x1b8e7
 * （原sub_1B8E7）。仅逻辑提交/预检/首错冻结；不替代原版音画验收。 */
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
    uint32_t rolls[8];
    size_t calls;
    uint8_t observed_id[8], observed_acted[8], observed_ex[8];
    uint16_t observed_mp[8], observed_target_hp[8];
    int corrupt_cell;
} fixture;

static uint32_t rng(void *data) {
    fixture *f = data;
    size_t index = f->calls++;
    if (index < 8) {
        const fd2_field_unit *actor = &f->game.units.items[0];
        f->observed_id[index] = ((const uint8_t *)actor)[0x0f];
        f->observed_acted[index] = (uint8_t)fd2_field_unit_has_acted(actor);
        f->observed_ex[index] = actor->experience;
        f->observed_mp[index] = actor->mp;
        f->observed_target_hp[index] = f->game.units.items[1].hp;
    }
    if (!index && f->corrupt_cell) f->cells[6 * 24 + 6] = 1;
    return f->rolls[index % 8];
}

static void unit(fd2_field_unit *u, uint8_t id, uint8_t side, uint8_t x, uint8_t y) {
    memset(u, 0, sizeof(*u));
    u->unit_id = u->text_id = id; u->side = side; u->x = x; u->y = y;
    u->level = 1; u->movement_profile = 5;
    u->hp = u->hp_max = 42; u->direction = 2;
    ((uint8_t *)u)[0x28] = 0xa7;
    ((uint8_t *)u)[0x31] = 0xff;
}

static void init(fixture *f) {
    memset(f, 0, sizeof(*f));
    fd2_field_game *g = &f->game;
    g->ready = 1; g->active_side = 2; g->turn_number = 7;
    g->selected_unit = 0; g->detail_unit = -1; g->attack_target = -1;
    g->interaction = FD2_FIELD_INTERACTION_COMMAND;
    g->cursor_cell_x = g->cursor_cell_y = 6; g->focus_cycle_unit = 3;
    g->map.width = g->map.height = 24; g->map.cells = f->cells;
    g->terrain.attrs = &f->attr; g->terrain.attr_count = 1;
    g->sprites.frame_count = 140u * 12u;
    g->units.count = 4;
    unit(&g->units.items[0], 0, 2, 6, 6);
    g->units.items[0].hp = 20; g->units.items[0].direction = 3;
    g->units.items[0].detail_status[2] = 1; /* MP0、+27与移动后均可用物品。 */
    const uint8_t inventory[] = {
        0x40,0, 0x40,132, 0,192, 0x80,0, 0x80,0, 0x80,0, 0x80,0, 0x80,201
    };
    memcpy((uint8_t *)&g->units.items[0] + 0x0a, inventory, sizeof(inventory));
    unit(&g->units.items[1], 4, 1, 5, 6);
    g->units.items[1].hp = 20; g->units.items[1].hp_max = 100;
    unit(&g->units.items[2], 0, 2, 8, 6);
    g->units.items[2].direction = 1;
    unit(&g->units.items[3], 96, 0, 15, 15);
    memset(g->metadata.turn_events, 0xff, sizeof(g->metadata.turn_events));
    memset(g->metadata.cell_lookup, 0xff, sizeof(g->metadata.cell_lookup));
    g->attack_rng = rng; g->attack_rng_userdata = f;
}

static int target(fixture *f, int other) {
    CHECK(fd2_field_game_begin_item_menu(&f->game) == 0);
    CHECK(fd2_field_game_confirm_item_menu(&f->game) == 0);
    f->game.item_list_selected = 2;
    CHECK(fd2_field_game_begin_item_targeting(&f->game) == 0);
    if (other) CHECK(fd2_field_game_cycle_item_target(&f->game) == 1);
    return 0;
}

static void close_fixture(fixture *f) {
    fd2_field_metadata_close(&f->game.metadata);
    fd2_field_path_close(&f->game.move_range);
    free(f->game.move_path);
}

static void expected_consumed(fd2_field_unit *actor) {
    const uint8_t inventory[] = {
        0x40,0, 0x40,132, 0x80,0, 0x80,0, 0x80,0, 0x80,0, 0x80,201, 0x80,201
    };
    memcpy((uint8_t *)actor + 0x0a, inventory, sizeof(inventory));
}

static int reject_unchanged(fixture *f) {
    fd2_field_game before = f->game;
    size_t calls = f->calls;
    CHECK(fd2_field_game_resolve_player_item(&f->game) == -1);
    CHECK(!memcmp(&before, &f->game, sizeof(before)) && f->calls == calls);
    return 0;
}

static int test_full_injured_and_ex99_keep_mp_ex_direction(void) {
    for (unsigned full = 0; full < 2; full++) {
        for (unsigned ex99 = 0; ex99 < 2; ex99++) {
            fixture f; init(&f);
            fd2_field_game *g = &f.game;
            g->units.items[0].experience = ex99 ? 99 : 0;
            if (full) g->units.items[0].hp = 42;
            g->move_path_length = 2; /* 与MAGIC不同，普通unit0移动后仍可用。 */
            f.attr.flags = 0x20; /* 不误入WAIT特殊cell-action。 */
            CHECK(target(&f, 0) == 0);
            fd2_field_units expected = g->units;
            expected.items[0].hp = 42;
            expected.items[0].flags |= FD2_FIELD_UNIT_FLAG_ACTED;
            expected_consumed(&expected.items[0]);
            CHECK(fd2_field_game_resolve_player_item(g) == 0);
            CHECK(!memcmp(&expected, &g->units, sizeof(expected)));
            CHECK(f.calls == 1 && f.observed_id[0] == 192 && !f.observed_acted[0]);
            CHECK(f.observed_mp[0] == 0 && f.observed_ex[0] == (ex99 ? 99 : 0));
            CHECK(g->interaction == FD2_FIELD_INTERACTION_BROWSE && g->selected_unit == -1);
            CHECK(g->active_side == 2 && !g->player_post_waiting_phase && !g->pending_cell_event.valid);
            CHECK(!g->move_range.nodes && !g->magic_target_count && !g->magic_target_cycle && !g->item_list_selected);
            CHECK(g->cursor_cell_x == 6 && g->cursor_cell_y == 6 && g->focus_cycle_unit == 3);
            CHECK(reject_unchanged(&f) == 0); /* 不能重复消耗或重试。 */
            close_fixture(&f);
        }
    }
    CHECK(fd2_field_game_resolve_player_item(NULL) == -1);
    return 0;
}

static int test_same_cell_raw_order_one_removal_and_no_wait_heal(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    g->units.count = 5;
    unit(&g->units.items[4], 4, 2, 5, 6);
    g->units.items[4].hp = 30; g->units.items[4].hp_max = 100;
    g->units.items[4].experience = 77; g->units.items[4].mp = 9;
    g->units.items[0].experience = 99; g->units.items[0].mp = 8;
    f.rolls[1] = 99;
    CHECK(target(&f, 1) == 0 && g->cursor_cell_x == 5);
    fd2_field_units expected = g->units;
    expected.items[1].hp = 56; expected.items[4].hp = 69;
    expected.items[0].flags |= FD2_FIELD_UNIT_FLAG_ACTED;
    expected_consumed(&expected.items[0]);
    CHECK(fd2_field_game_resolve_player_item(g) == 0);
    CHECK(!memcmp(&expected, &g->units, sizeof(expected)));
    CHECK(f.calls == 2 && f.observed_target_hp[0] == 20 && f.observed_target_hp[1] == 56);
    for (size_t i = 0; i < f.calls; i++)
        CHECK(f.observed_id[i] == 192 && !f.observed_acted[i] && f.observed_mp[i] == 8 && f.observed_ex[i] == 99);
    CHECK(g->units.items[0].hp == 20 && g->units.items[0].direction == 3 && g->units.items[2].direction == 1);
    close_fixture(&f); return 0;
}

static int test_preflight_errors_keep_all_owners_and_rng(void) {
    for (unsigned which = 0; which < 17; which++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        CHECK(target(&f, 1) == 0);
        switch (which) {
            case 0: g->interaction = FD2_FIELD_INTERACTION_ITEM_LIST; break;
            case 1: g->item_list_selected = 0; break;
            case 2: ((uint8_t *)&g->units.items[0])[0x0f] = 255; break;
            case 3: ((uint8_t *)&g->units.items[0])[0x0c] = 0x80; break;
            case 4: g->attack_rng = NULL; break;
            case 5: g->units.items[0].flags |= FD2_FIELD_UNIT_FLAG_ACTED; break;
            case 6: g->units.items[1].movement_profile = 255; g->units.items[1].x = 255; break;
            case 7: g->units.items[1].side = 0; break;
            case 8: g->units.items[1].flags |= FD2_FIELD_UNIT_FLAG_HIDDEN; break;
            case 9: g->units.items[3].x = 255; break; /* 非目标晚raw也先检查。 */
            case 10: g->pending_cell_event = (fd2_field_pending_cell_event){1,9,0}; break;
            case 11:
                f.cells[6 * 24 + 6] = 1u << 16;
                g->metadata.cell_lookup[0] = (fd2_field_cell_lookup){9,1}; break;
            case 12:
                g->cursor_cell_x = g->units.items[1].x = 23;
                g->move_range.nodes[6 * 24 + 23].distance = 0;
                g->move_range.nodes[6 * 24 + 23].can_stop = 1;
                CHECK(fd2_field_game_item_target_is_legal(g) == 1); break;
            case 13:
                g->units.items[2].flags = FD2_FIELD_UNIT_FLAG_ACTED;
                g->metadata.turn_events[0] = (fd2_field_turn_event){7,9,1}; break;
            case 14:
                g->units.items[2].flags = FD2_FIELD_UNIT_FLAG_ACTED;
                g->metadata.turn_events[0] = (fd2_field_turn_event){7,0,1};
                g->event_log_count = FD2_FIELD_EVENT_LOG_CAPACITY; break;
            case 15:
                g->units.items[3].hp = 0;
                ((uint8_t *)&g->units.items[3])[0x31] = 0; break;
            case 16:
                g->units.items[2].hp = 0;
                g->metadata.turn_events[0] = (fd2_field_turn_event){7,9,1}; break;
        }
        CHECK(reject_unchanged(&f) == 0 && !f.calls);
        CHECK(g->move_range.nodes != NULL);
        close_fixture(&f);
    }
    return 0;
}

static int test_revive_prediction_and_live_drop_descriptor(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    g->units.items[2].x = 5; g->units.items[2].y = 6; g->units.items[2].hp = 0;
    ((uint8_t *)&g->units.items[1])[0x31] = 0; /* 活目标不会死，无需拒其descriptor。 */
    ((uint8_t *)&g->units.items[2])[0x31] = 0; /* HP0目标必先恢复存活。 */
    g->metadata.turn_events[0] = (fd2_field_turn_event){7,9,1};
    g->event_log_count = FD2_FIELD_EVENT_LOG_CAPACITY;
    CHECK(target(&f, 1) == 0);
    CHECK(fd2_field_game_resolve_player_item(g) == 0);
    CHECK(g->units.items[1].hp == 56 && g->units.items[2].hp == 36);
    CHECK(g->units.items[2].flags == 0 && g->active_side == 2 && !g->player_post_waiting_phase);
    CHECK(!g->failure && f.calls == 2 && g->event_log_count == FD2_FIELD_EVENT_LOG_CAPACITY);
    close_fixture(&f); return 0;
}

static int test_post_commit_failure_keeps_effect_consumption_and_acted(void) {
    for (unsigned phase = 0; phase < 2; phase++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        g->units.items[0].experience = 99;
        if (phase) {
            g->units.items[2].flags = FD2_FIELD_UNIT_FLAG_ACTED;
            g->metadata.turn_events[0] = (fd2_field_turn_event){7,0,1};
        } else f.corrupt_cell = 1; /* 首RNG后注入后段cell错误，不是预检遗漏。 */
        CHECK(target(&f, 0) == 0);
        CHECK(fd2_field_game_resolve_player_item(g) == -1);
        CHECK(g->failure == (phase ? FD2_FIELD_FAILURE_EVENT_EXECUTION : FD2_FIELD_FAILURE_EVENT_DATA));
        CHECK(g->units.items[0].hp == 42 && g->units.items[0].mp == 0 && g->units.items[0].experience == 99);
        CHECK(fd2_field_unit_has_acted(&g->units.items[0]) && f.calls == 1);
        fd2_field_unit expected = g->units.items[0];
        expected_consumed(&expected);
        CHECK(!memcmp((uint8_t *)&expected + 0x0a, (uint8_t *)&g->units.items[0] + 0x0a, 16));
        CHECK(reject_unchanged(&f) == 0);
        close_fixture(&f);
    }
    return 0;
}

static int test_last_player_controller_and_death_cleanup(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    g->units.items[2].flags = FD2_FIELD_UNIT_FLAG_ACTED;
    CHECK(target(&f, 0) == 0);
    CHECK(fd2_field_game_resolve_player_item(g) == 0);
    CHECK(g->active_side == 1 && g->player_post_waiting_phase && g->player_post_actor == 0);
    CHECK(fd2_field_game_resume_player_controller(g) == 1 && f.calls == 1);
    close_fixture(&f);

    init(&f); g = &f.game;
    g->units.items[1].hp = 0; g->units.items[1].direction = 2;
    g->units.items[3].hp = 0; g->units.items[3].x = g->units.items[3].y = 23;
    g->units.items[3].direction = 1;
    CHECK(target(&f, 0) == 0);
    CHECK(fd2_field_game_resolve_player_item(g) == 0);
    CHECK(g->units.items[1].flags == 1 && g->units.items[3].flags == 1);
    CHECK(g->units.items[1].direction == 0 && g->units.items[3].direction == 0);
    CHECK(g->units.items[0].direction == 3 && g->units.items[2].direction == 1 && f.calls == 1);
    close_fixture(&f); return 0;
}

static int test_formal_query_and_weapon_only_list(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    g->units.items[0].experience = 99; g->move_path_length = 2;
    fd2_field_game before = *g;
    CHECK(fd2_field_game_item_is_available(g) == 1 && !memcmp(g, &before, sizeof(before)) && !f.calls);
    ((uint8_t *)&g->units.items[0])[0x0e] = 0x80; /* 只留剑/甲，仍可看Use列表。 */
    CHECK(fd2_field_game_item_is_available(g) == 1);
    CHECK(fd2_field_game_refresh_commands(g) == 0 && !g->command_disabled[FD2_FIELD_COMMAND_ITEM]);
    CHECK(g->command_selected == FD2_FIELD_COMMAND_ITEM);
    CHECK(fd2_field_game_confirm_cursor(g) == 0 && g->interaction == FD2_FIELD_INTERACTION_ITEM_MENU);
    CHECK(fd2_field_game_confirm_cursor(g) == 0 && g->interaction == FD2_FIELD_INTERACTION_ITEM_LIST);
    before = *g;
    CHECK(fd2_field_game_confirm_cursor(g) == -1 && !memcmp(g, &before, sizeof(before))); /* LIST由modal消费键。 */
    CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_CONFIRM) == 0);
    CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_DOWN) == 0 && g->item_list_selected == 1);
    CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_CONFIRM) == 0);
    CHECK(fd2_field_game_cancel_selection(g) == 1 && g->interaction == FD2_FIELD_INTERACTION_ITEM_MENU);
    CHECK(fd2_field_game_cancel_selection(g) == 1 && g->interaction == FD2_FIELD_INTERACTION_COMMAND);
    CHECK(g->command_selected == FD2_FIELD_COMMAND_ITEM && !f.calls);
    close_fixture(&f);

    CHECK(fd2_field_game_item_is_available(NULL) == 0);
    for (unsigned which = 0; which < 16; which++) {
        init(&f); g = &f.game;
        switch (which) {
            case 0: g->attack_rng = NULL; break;
            case 1: g->stage = 1; break;
            case 2: g->active_side = 1; break;
            case 3: g->player_post_waiting_phase = 1; break;
            case 4: g->units.items[0].flags |= FD2_FIELD_UNIT_FLAG_ACTED; break;
            case 5: g->selected_unit = 4; break;
            case 6: g->units.count = FD2_FIELD_MAX_UNITS + 1u; break;
            case 7: ((uint8_t *)&g->units.items[0])[0x0f] = 255; break;
            case 8: ((uint8_t *)&g->units.items[0])[0x0f] = 215; break;
            case 9: ((uint8_t *)&g->units.items[0])[0x0c] = 0x80; break;
            case 10: for (unsigned i = 0; i < 8; i++) ((uint8_t *)&g->units.items[0])[0x0a + 2 * i] = 0x80; break;
            case 11: g->units.items[0].flags |= FD2_FIELD_UNIT_FLAG_HIDDEN; break;
            case 12: g->units.items[0].hp = 0; break;
            case 13: g->failure = FD2_FIELD_FAILURE_EVENT_DATA; break;
            case 14: g->ready = 0; break;
            case 15: g->units.items[0].side = 1; break;
        }
        before = *g;
        CHECK(fd2_field_game_item_is_available(g) == 0 && !memcmp(g, &before, sizeof(before)) && !f.calls);
        close_fixture(&f);
    }
    return 0;
}

static int test_formal_confirmation_rechecks_whole_list_but_cancel_does_not(void) {
    for (unsigned layer = 0; layer < 3; layer++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        CHECK(fd2_field_game_refresh_commands(g) == 0 && !g->command_disabled[FD2_FIELD_COMMAND_ITEM]);
        if (layer == 1) CHECK(fd2_field_game_confirm_cursor(g) == 0);
        if (layer == 2) CHECK(target(&f, 0) == 0);
        /* 当前选中草药不变，另一可见项成为未知ID时仍须重验。 */
        ((uint8_t *)&g->units.items[0])[0x0b] = 255;
        fd2_field_game before = *g;
        CHECK(fd2_field_game_confirm_cursor(g) == -1 && !memcmp(g, &before, sizeof(before)) && !f.calls);
        ((uint8_t *)&g->units.items[0])[0x0b] = 0;
        if (layer) {
            CHECK(fd2_field_game_cancel_selection(g) == 1);
            if (layer == 2) {
                CHECK(g->interaction == FD2_FIELD_INTERACTION_ITEM_LIST && !g->item_list_selected);
                CHECK(fd2_field_game_cancel_selection(g) == 1);
            }
            if (g->interaction == FD2_FIELD_INTERACTION_ITEM_MENU)
                CHECK(fd2_field_game_cancel_selection(g) == 1);
            CHECK(g->interaction == FD2_FIELD_INTERACTION_COMMAND && !g->command_disabled[FD2_FIELD_COMMAND_ITEM]);
        }
        CHECK(!f.calls);
        close_fixture(&f);
    }
    return 0;
}

static int test_formal_route_returns_the_nonzero_actor_index(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    fd2_field_unit actor = g->units.items[0];
    g->units.items[0] = g->units.items[1];
    g->units.items[1] = actor; g->units.items[1].experience = 99;
    g->selected_unit = 1;
    CHECK(fd2_field_game_refresh_commands(g) == 0 && g->command_selected == FD2_FIELD_COMMAND_ITEM);
    CHECK(fd2_field_game_confirm_cursor(g) == 1 && g->interaction == FD2_FIELD_INTERACTION_ITEM_MENU);
    CHECK(fd2_field_game_confirm_cursor(g) == 1 && g->interaction == FD2_FIELD_INTERACTION_ITEM_LIST);
    CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_DOWN) == 0);
    CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_DOWN) == 0 && g->item_list_selected == 2);
    CHECK(fd2_field_game_item_list_step(g, FD2_FIELD_ITEM_LIST_KEY_CONFIRM) == 1);
    CHECK(fd2_field_game_begin_item_targeting(g) == 0 && !f.calls);
    CHECK(g->cursor_cell_x == 5 && g->cursor_cell_y == 6);
    CHECK(fd2_field_game_confirm_cursor(g) == 1);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_BROWSE && g->selected_unit == -1);
    CHECK(fd2_field_unit_has_acted(&g->units.items[1]) && g->units.items[1].experience == 99);
    CHECK(g->units.items[1].mp == 0 && g->units.items[1].hp == 20 && g->units.items[0].hp == 56 && f.calls == 1);
    fd2_field_unit expected = g->units.items[1]; expected_consumed(&expected);
    CHECK(!memcmp((uint8_t *)&expected + 0x0a, (uint8_t *)&g->units.items[1] + 0x0a, 16));
    fd2_field_game before = *g;
    CHECK(fd2_field_game_confirm_cursor(g) == -1 && !memcmp(g, &before, sizeof(before)) && f.calls == 1);
    close_fixture(&f); return 0;
}

static int start_item(fixture *f, uint8_t id) {
    fd2_field_game *g = &f->game;
    ((uint8_t *)&g->units.items[0])[0x0f] = id;
    CHECK(fd2_field_game_item_use_is_available(g));
    CHECK(fd2_field_game_begin_item_menu(g) == 0 && !g->command_disabled[0]);
    CHECK(fd2_field_game_confirm_item_menu(g) == 0);
    g->item_list_selected = 2;
    CHECK(fd2_field_game_begin_item_targeting(g) == 0);
    return 0;
}

static int test_all_twenty_use_codes_keep_distinct_cost_and_tail(void) {
    static const uint8_t ids[] = {192,196,197,198,199,200,206,210,211,212,213,214,94,95,96,56,38,57,101,79};
    for (size_t n = 0; n < sizeof(ids); n++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        fd2_field_unit *actor = &g->units.items[0];
        actor->experience = 99; actor->mp = 3; actor->mp_max = 100;
        actor->attack = 16; actor->defense = 12; actor->accuracy = 80; actor->evasion = 5;
        actor->text_id = 24;
        actor->movement_points = 255;
        actor->base_attack_le[0] = 10; actor->base_defense_le[0] = 20; actor->dexterity_le[0] = 30;
        g->units.items[1].side = 0; g->units.items[1].x = 7; g->units.items[1].hp = g->units.items[1].hp_max = 1000;
        const uint8_t *record = fd2_field_item_record_get(ids[n]);
        unsigned code = record[0x0d];
        if (code == 23) g->units.items[1].side = 1;
        if (code == 6) actor->detail_status[0] = 5;
        if (code == 7) { g->units.items[1].side = 1; g->units.items[1].detail_status[1] = 5; }
        CHECK(start_item(&f, ids[n]) == 0);
        if (code == 7) { g->cursor_cell_x = 7; g->cursor_cell_y = 6; }
        if (code == 23) {
            CHECK(fd2_field_game_resolve_player_item(g) == 0 && g->interaction == FD2_FIELD_INTERACTION_ITEM_TELEPORTING && !f.calls);
            g->cursor_cell_x = 7; g->cursor_cell_y = 7;
        }
        CHECK(fd2_field_game_resolve_player_item(g) == 0);
        CHECK(g->selected_unit == -1 && fd2_field_unit_has_acted(actor) && actor->experience == 99 && actor->direction == 3);
        CHECK(actor->mp == (code == 23 ? (uint16_t)(3-20) : code == 11 ? 75 : 3));
        int removes = code == 5 || code == 6 || code == 7 || code == 11 ||
            (code >= 8 && code <= 10) || (code >= 17 && code <= 19);
        CHECK(fd2_field_unit_inventory_count(actor) == (removes ? 2u : 3u));
        if (!removes) CHECK(((uint8_t *)actor)[0x0f] == ids[n]);
        if (code == 5 || code == 13) CHECK(actor->hp == 42);
        if (code == 6) CHECK(!actor->detail_status[0] && actor->hp == 29 && f.calls == 1);
        if (code == 7) CHECK(!g->units.items[1].detail_status[1] && f.calls == 1);
        if (code == 8) CHECK(actor->base_attack_le[0] == 19 && !f.calls);
        if (code == 9) CHECK(actor->base_defense_le[0] == 29 && !f.calls);
        if (code == 10) CHECK(actor->dexterity_le[0] == 37 && !f.calls);
        if (code == 12) CHECK(actor->hit_evasion_status == 2 && actor->accuracy == 95 && actor->evasion == 20 && f.calls == 1);
        if (code == 14) CHECK(g->units.items[1].hp == 991 && g->units.items[1].detail_status[1] == 2 && f.calls == 3);
        if (code == 15) CHECK(actor->defense == 14 && actor->defense_status == 2 && f.calls == 1);
        if (code == 16) CHECK(actor->attack == 19 && actor->attack_status == 2 && f.calls == 1);
        if (code == 17) CHECK(actor->hp_max == 62 && actor->hp == 20 && !f.calls);
        if (code == 18) CHECK(actor->mp_max == 120 && !f.calls);
        if (code == 19) CHECK(actor->movement_points == 0 && actor->experience == 99 && !f.calls);
        if (code == 20 || code == 21 || code == 24) CHECK(g->units.items[1].hp < 1000 && f.calls == 2);
        if (code == 22) CHECK(g->units.items[1].hp == 991 && g->units.items[1].detail_status[2] == 2 && f.calls == 3);
        if (code == 23) CHECK(g->units.items[1].x == 7 && g->units.items[1].y == 7 && actor->x == 6 && actor->y == 6 && !f.calls);
        close_fixture(&f);
    }
    return 0;
}

static int test_use_area_empty_center_and_teleport_cancel(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    CHECK(start_item(&f, 211) == 0);
    g->cursor_cell_x = 6; g->cursor_cell_y = 7; /* 空中心，Manhattan1有caster。 */
    CHECK(fd2_field_game_item_target_is_legal(g) == 1);
    CHECK(fd2_field_game_resolve_player_item(g) == 0 && g->units.items[0].hp == 42 && f.calls == 1);
    close_fixture(&f);
    for (int gate = 0; gate < 3; gate++) {
        init(&f); g = &f.game; g->units.items[0].text_id = 24; g->units.items[0].mp_max = 20;
        if (gate == 0) g->units.items[0].text_id = 23;
        if (gate == 1) g->units.items[0].mp_max = 19;
        CHECK(start_item(&f, 101) == 0);
        fd2_field_units before = g->units;
        CHECK(fd2_field_game_resolve_player_item(g) == 0);
        if (gate == 2) {
            CHECK(g->interaction == FD2_FIELD_INTERACTION_ITEM_TELEPORTING);
            g->cursor_cell_x = g->units.items[2].x; g->cursor_cell_y = g->units.items[2].y;
            CHECK(fd2_field_game_item_target_is_legal(g) == 0);
            CHECK(fd2_field_game_cancel_item_targeting(g) == 0);
        }
        CHECK(g->interaction == FD2_FIELD_INTERACTION_ITEM_LIST && !g->item_list_selected && !f.calls);
        CHECK(!memcmp(&before, &g->units, sizeof(before)) && g->cursor_cell_x == 6 && g->cursor_cell_y == 6);
        close_fixture(&f);
    }
    return 0;
}

static int test_cure_heal_revive_prediction_and_no_status_consumption(void) {
    for (int paralysis = 0; paralysis < 2; paralysis++) {
        for (int status = 0; status < 2; status++) {
            fixture f; init(&f); fd2_field_game *g = &f.game;
            g->units.items[1].side = 2; g->units.items[1].hp = 0;
            g->units.items[1].detail_status[paralysis] = status ? 5 : 0;
            ((uint8_t *)&g->units.items[1])[0x31] = 0; /* 仅被实际复活才排除未知loot。 */
            g->units.items[2].flags = 0x80;
            g->metadata.turn_events[0] = (fd2_field_turn_event){7,9,1};
            CHECK(start_item(&f, (uint8_t)(196+paralysis)) == 0);
            g->cursor_cell_x = 5; g->cursor_cell_y = 6;
            if (status) {
                CHECK(fd2_field_game_resolve_player_item(g) == 0 && f.calls == 1);
                CHECK(g->units.items[1].hp == 9 && !g->units.items[1].detail_status[paralysis]);
                CHECK(g->active_side == 2 && !g->failure && !g->player_post_waiting_phase);
            } else CHECK(reject_unchanged(&f) == 0 && !f.calls);
            close_fixture(&f);
        }
        fixture f; init(&f); CHECK(start_item(&f, (uint8_t)(196+paralysis)) == 0);
        CHECK(fd2_field_game_resolve_player_item(&f.game) == 0 && !f.calls);
        CHECK(fd2_field_unit_inventory_count(&f.game.units.items[0]) == 2 && f.game.units.items[0].hp == 20);
        close_fixture(&f);
    }
    return 0;
}

static int test_general_use_preflight_and_permanent_word_wrap(void) {
    fixture f; init(&f);
    CHECK(start_item(&f, 56) == 0);
    f.game.units.items[1].side = 0; f.game.cursor_cell_x = 5; f.game.cursor_cell_y = 6;
    f.game.units.items[1].movement_profile = 0;
    CHECK(reject_unchanged(&f) == 0 && !f.calls);
    close_fixture(&f);
    init(&f); f.game.units.items[1].side = 0;
    f.game.units.items[2] = f.game.units.items[1];
    f.game.units.items[2].hp = 0; f.game.units.items[2].movement_profile = 0;
    CHECK(start_item(&f, 56) == 0);
    CHECK(reject_unchanged(&f) == 0 && !f.calls); /* 晚raw HP0也须先校damage profile。 */
    close_fixture(&f);
    init(&f); f.game.units.items[1].side = 0; f.game.units.items[1].hp = 1;
    ((uint8_t *)&f.game.units.items[1])[0x31] = 0;
    CHECK(start_item(&f, 56) == 0);
    CHECK(reject_unchanged(&f) == 0 && !f.calls);
    close_fixture(&f);
    init(&f); CHECK(start_item(&f, 198) == 0);
    ((uint8_t *)&f.game.units.items[0])[0x18] = 0xc0;
    ((uint8_t *)&f.game.units.items[0])[0x19] = 255;
    CHECK(reject_unchanged(&f) == 0 && !f.calls);
    close_fixture(&f);
    init(&f); f.game.units.items[0].base_attack_le[0] = f.game.units.items[0].base_attack_le[1] = 255;
    CHECK(start_item(&f, 198) == 0);
    CHECK(fd2_field_game_resolve_player_item(&f.game) == 0 && !f.calls);
    CHECK(f.game.units.items[0].base_attack_le[0] == 8 && f.game.units.items[0].base_attack_le[1] == 0);
    close_fixture(&f);
    init(&f); f.game.units.items[0].hp_max = 65530;
    CHECK(start_item(&f, 94) == 0);
    CHECK(fd2_field_game_resolve_player_item(&f.game) == 0 && f.game.units.items[0].hp_max == 14 && f.game.units.items[0].hp == 20 && !f.calls);
    close_fixture(&f);
    static const uint8_t no_rng_ids[] = {196,197,206};
    for (size_t i = 0; i < sizeof(no_rng_ids); i++) {
        init(&f); CHECK(start_item(&f, no_rng_ids[i]) == 0);
        f.game.attack_rng = NULL;
        CHECK(reject_unchanged(&f) == 0 && !f.calls);
        close_fixture(&f);
    }
    return 0;
}

int main(void) {
    CHECK(test_cure_heal_revive_prediction_and_no_status_consumption() == 0);
    CHECK(test_general_use_preflight_and_permanent_word_wrap() == 0);
    CHECK(test_all_twenty_use_codes_keep_distinct_cost_and_tail() == 0);
    CHECK(test_use_area_empty_center_and_teleport_cancel() == 0);
    CHECK(test_full_injured_and_ex99_keep_mp_ex_direction() == 0);
    CHECK(test_same_cell_raw_order_one_removal_and_no_wait_heal() == 0);
    CHECK(test_preflight_errors_keep_all_owners_and_rng() == 0);
    CHECK(test_revive_prediction_and_live_drop_descriptor() == 0);
    CHECK(test_post_commit_failure_keeps_effect_consumption_and_acted() == 0);
    CHECK(test_last_player_controller_and_death_cleanup() == 0);
    CHECK(test_formal_query_and_weapon_only_list() == 0);
    CHECK(test_formal_confirmation_rechecks_whole_list_but_cancel_does_not() == 0);
    CHECK(test_formal_route_returns_the_nonzero_actor_index() == 0);
    puts("field item commit tests: ok");
    return 0;
}
