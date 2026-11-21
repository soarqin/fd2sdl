#include "archive.h"
#include "field_game.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); return -1; \
} } while (0)

typedef struct {
    uint8_t x, y, item;
} chest;

static const chest linked_chests[] = {
    {14, 3, 29}, {16, 3, 43}, {15, 4, 51}, {14, 5, 61}, {16, 5, 71}
};

static void place_actor(fd2_field_game *game, const chest *box) {
    game->cursor_cell_x = game->units.items[0].x = box->x;
    game->cursor_cell_y = game->units.items[0].y = box->y;
    game->camera_cell_x = box->x - 6;
    game->camera_cell_y = box->y > 3u ? box->y - 3 : 0;
}

static int open_case(fd2_field_game *game, const fd2_archive *other,
                     size_t stage, const chest *box) {
    fd2_field_stage_capability cap;
    fd2_save_file save = {0};
    fd2_save_battle_snapshot start;
    CHECK(fd2_save_file_open(&save, "original_game/START.SAV") == 0);
    int result = fd2_save_file_get_battle_snapshot(&save, &start);
    fd2_save_file_close(&save);
    CHECK(result == 0 && start.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] == 12);
    CHECK(fd2_field_game_open_resources(game, other, stage) == 0);
    CHECK(fd2_field_stage_capability_get(stage, &cap) == 0 && cap.minimum_units >= 3u);
    game->units.count = cap.minimum_units;
    for (size_t i = 0; i < game->units.count; i++)
        game->units.items[i].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    memcpy(game->units.items, start.unit_area, 2u * FD2_FIELD_UNIT_RECORD_SIZE);
    memcpy(&game->units.items[2], start.unit_area + 4u * FD2_FIELD_UNIT_RECORD_SIZE,
           FD2_FIELD_UNIT_RECORD_SIZE);
    game->units.items[1].x = 1;
    game->units.items[2].x = 2;
    game->units.items[1].y = game->units.items[2].y = 0;
    CHECK(fd2_field_units_sprite_cache_restore(&game->units) == 0);
    /* 保留另一个可行动队友和敌军，隔离待机后的关卡结果。 */
    game->roster_count = 1;
    game->roster[0].text_id = 18;
    game->option_values[1] = 0;
    place_actor(game, box);
    return 0;
}

static int wait_at_cursor(fd2_field_game *game) {
    CHECK(fd2_field_game_confirm_cursor(game) == 0);
    CHECK(fd2_field_game_confirm_cursor(game) == 0);
    CHECK(fd2_field_game_execute_move(game, NULL, 0) == 0);
    CHECK(fd2_field_game_finish_unit_action(game) == 0);
    return 0;
}

static int reply_chest_choice(fd2_field_game *game, fd2_field_loot_reply reply,
                              unsigned result_fragment, uint8_t item) {
    fd2_field_loot_notice notice;
    CHECK(fd2_field_game_loot_peek(game, &notice) == 1);
    CHECK(notice.kind == FD2_FIELD_LOOT_CHOICE && notice.fragment == 421u);
    CHECK(fd2_field_game_loot_reply(game, reply, -1) == 0);
    for (unsigned steps = 0; steps < 6u; steps++) {
        int pending = fd2_field_game_loot_peek(game, &notice);
        CHECK(pending >= 0);
        if (!pending) break;
        if (notice.kind == FD2_FIELD_LOOT_MESSAGE) {
            CHECK(result_fragment && notice.fragment == result_fragment);
            CHECK(result_fragment != 422u || notice.item_id == item);
            CHECK(fd2_field_game_loot_text_ready(game) == 0);
            result_fragment = 0;
        } else CHECK(notice.kind == FD2_FIELD_LOOT_DELAY);
        CHECK(fd2_field_game_loot_reply(game, FD2_FIELD_LOOT_ACK, -1) == 0);
    }
    CHECK(!result_fragment && fd2_field_game_loot_peek(game, &notice) == 0);
    return 0;
}

static int reply_event(fd2_field_game *game, const char *op, int32_t arg, unsigned port) {
    fd2_static_flow_node node;
    size_t actor;
    CHECK(fd2_field_game_common_event_peek(game, &node, &actor) == 1);
    CHECK(actor == 0 && !strcmp(node.op, op) && node.args[0] == arg);
    CHECK(fd2_field_game_common_event_reply(game, port) == 0);
    return 0;
}

static int test_book(fd2_field_game *game, const fd2_archive *other) {
    /* 第 17 章 metadata[0x74..0x76]，普通箱格 (23,17)，物品 92 为心眼之书。 */
    const chest book = {23, 17, 92};
    CHECK(open_case(game, other, 16, &book) == 0);
    CHECK(game->metadata.cell_actions[11].mode == 0 &&
          game->metadata.cell_actions[11].param == book.item);
    CHECK(wait_at_cursor(game) == 0);
    CHECK(reply_chest_choice(game, FD2_FIELD_LOOT_YES, 422, book.item) == 0);
    CHECK(fd2_field_event_item_find(&game->units, 0, book.item, NULL) == 1);
    CHECK(fd2_field_event_item_find(&game->units, 0, 91, NULL) == 0);
    CHECK(game->cell_action_completed[11] == 1);
    return 0;
}

static int check_linked_chests(const fd2_field_game *game, const uint32_t *closed, int opened) {
    for (size_t i = 0; i < SDL_arraysize(linked_chests); i++) {
        const chest *box = &linked_chests[i];
        uint32_t cell = fd2_field_map_cell(&game->map, box->x, box->y);
        uint32_t open_cell = (closed[i] & UINT32_C(0xff000000)) | (uint16_t)(closed[i] + 1u);
        CHECK(cell == (opened ? open_cell : closed[i]));
        CHECK(game->cell_action_completed[i] == opened);
    }
    return 0;
}

static int test_linked_chests(fd2_field_game *game, const fd2_archive *other) {
    CHECK(open_case(game, other, 25, &linked_chests[0]) == 0);
    uint32_t closed[SDL_arraysize(linked_chests)];
    for (size_t i = 0; i < SDL_arraysize(linked_chests); i++) {
        const chest *box = &linked_chests[i];
        closed[i] = fd2_field_map_cell(&game->map, box->x, box->y);
        CHECK((closed[i] >> 16 & 0xffu) == i);
        CHECK(game->metadata.cell_actions[i].mode == 2 && game->metadata.cell_actions[i].param == 58);
    }
    const uint32_t other_box = fd2_field_map_cell(&game->map, 30, 33);
    fd2_save_battle_snapshot initial = {0};
    CHECK(fd2_field_game_export_battle_snapshot(game, &initial) == 0);

    size_t initial_count = fd2_field_unit_inventory_count(&game->units.items[0]);
    CHECK(wait_at_cursor(game) == 0);
    CHECK(reply_chest_choice(game, FD2_FIELD_LOOT_CANCEL, 412, 0) == 0);
    CHECK(fd2_field_unit_inventory_count(&game->units.items[0]) == initial_count);
    CHECK(!game->event_resume.active && check_linked_chests(game, closed, 0) == 0);

    for (size_t i = 0; i <= SDL_arraysize(linked_chests); i++) {
        int full = i == SDL_arraysize(linked_chests);
        const chest *box = &linked_chests[full ? 0 : i];
        CHECK(fd2_field_game_import_battle_snapshot(game, &initial) == 0);
        place_actor(game, box);
        if (full) memset(game->units.items[0].data_09_1e + 1, 0, 16);
        size_t count = fd2_field_unit_inventory_count(&game->units.items[0]);
        CHECK(!full || count == 8);
        CHECK(wait_at_cursor(game) == 0);
        CHECK(reply_chest_choice(game, FD2_FIELD_LOOT_YES, 0, 0) == 0);
        CHECK(reply_event(game, "clear_keys", 0, FD2_FLOW_NEXT) == 0);
        CHECK(reply_event(game, "notice_actor", -1, FD2_FLOW_NEXT) == 0);
        CHECK(reply_event(game, "message", full ? 480 : 422, FD2_FLOW_NEXT) == 0);
        CHECK(reply_event(game, "portrait_frame", 0, FD2_FLOW_NEXT) == 0);
        CHECK(fd2_field_unit_inventory_count(&game->units.items[0]) == count);
        CHECK(reply_event(game, "wait_key", 0, FD2_FLOW_ACK) == 0);
        CHECK(fd2_field_unit_inventory_count(&game->units.items[0]) == count + (size_t)!full);
        CHECK(check_linked_chests(game, closed, 0) == 0);
        CHECK(reply_event(game, "close_box", 0, FD2_FLOW_NEXT) == 0);
        CHECK(!game->event_resume.active && check_linked_chests(game, closed, !full) == 0);
        CHECK(!game->cell_action_completed[5] && fd2_field_map_cell(&game->map, 30, 33) == other_box);
        CHECK(fd2_field_event_item_find(&game->units, 0, box->item, NULL) == !full);
        if (full) continue;
        fd2_save_battle_snapshot opened = initial;
        CHECK(fd2_field_game_export_battle_snapshot(game, &opened) == 0);
        CHECK(fd2_field_game_import_battle_snapshot(game, &opened) == 0);
        CHECK(check_linked_chests(game, closed, 1) == 0);
        fd2_field_unit_set_acted(&game->units.items[0], 0);
        place_actor(game, &linked_chests[(i + 1u) % SDL_arraysize(linked_chests)]);
        CHECK(wait_at_cursor(game) == 0);
        fd2_field_loot_notice notice;
        CHECK(!game->event_resume.active && fd2_field_game_loot_peek(game, &notice) == 0);
        CHECK(fd2_field_unit_inventory_count(&game->units.items[0]) == count + 1u);
    }
    return 0;
}

int main(void) {
    fd2_archive other = {0};
    fd2_field_game *game = calloc(1, sizeof(*game));
    if (!game || fd2_archive_open(&other, "original_game/FDOTHER.DAT") != 0) {
        free(game);
        return 1;
    }
    int result = test_book(game, &other);
    fd2_field_game_close(game);
    if (result == 0) result = test_linked_chests(game, &other);
    fd2_field_game_close(game);
    fd2_archive_close(&other);
    free(game);
    return result != 0;
}
