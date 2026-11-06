/* field_ai_move_toward_cell @0x14b78（原登记同名）的私有几何回归。
 * 测试TU包含正式实现，链接时排除core的field_game对象；无生产测试hook，
 * 不改变原始START单位阵营，也不声称已验收March/AI整套交互。 */
#include "field_game.c"
#include <stdio.h>

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

typedef struct {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attrs[2];
    size_t rng_calls;
} geometry_fixture;

static uint32_t geometry_rng(void *data) { (*(size_t *)data)++; return 0; }

static void geometry_unit(fd2_field_unit *u, uint8_t side, uint8_t x, uint8_t y) {
    memset(u, 0, sizeof(*u));
    u->side = side; u->x = x; u->y = y;
    u->hp = u->hp_max = 50;
    u->movement_profile = 1; u->movement_points = 3;
}

static void geometry_init(geometry_fixture *f) {
    memset(f, 0, sizeof(*f));
    fd2_field_game *g = &f->game;
    g->ready = 1; g->turn_number = 1; g->active_side = 2;
    g->selected_unit = g->command_selected = g->detail_unit = g->attack_target = -1;
    g->map.width = g->map.height = 8; g->map.cells = f->cells;
    g->terrain.attrs = f->attrs; g->terrain.attr_count = 2;
    f->attrs[0].movement_cost_class = 0; /* profile1 cost1，合成平地。 */
    f->attrs[1].movement_cost_class = 1; /* profile1 cost20。 */
    memset(g->metadata.turn_events, 0xff, sizeof(g->metadata.turn_events));
    memset(g->metadata.cell_lookup, 0xff, sizeof(g->metadata.cell_lookup));
    g->units.count = 2;
    geometry_unit(&g->units.items[0], 2, 1, 3);
    geometry_unit(&g->units.items[1], 0, 7, 7);
    g->cursor_cell_x = 1; g->cursor_cell_y = 3;
    g->attack_rng = geometry_rng; g->attack_rng_userdata = &f->rng_calls;
}

static int test_requested_anchor_precedes_destination_and_friend_block(void) {
    geometry_fixture f; geometry_init(&f);
    fd2_field_game *g = &f.game;
    /* 两格厚的高成本墙仅北侧可绕，不能先选靠近原anchor的死路(2,3)。 */
    for (size_t y = 2; y < 8; y++) f.cells[y * 8 + 3] = f.cells[y * 8 + 4] = 1;
    g->units.count = 3;
    geometry_unit(&g->units.items[2], 2, 1, 2);
    CHECK(commit_ai_move_toward_anchor(g, 0, 6, 3, NULL, 0, FIELD_PATH_BROWSE) == 1);
    CHECK(g->units.items[0].x == 2 && g->units.items[0].y == 1);
    CHECK(g->units.items[2].x == 1 && g->units.items[2].y == 2);
    CHECK(!fd2_field_unit_has_acted(&g->units.items[0]));
    CHECK(g->failure == FD2_FIELD_FAILURE_NONE && g->event_log_count == 0 && f.rng_calls == 0);
    CHECK(g->move_path == NULL && g->move_path_capacity == 0);
    return 0;
}

static int test_whole_long_route_can_reenter_reachable_area(void) {
    for (int occupied = 0; occupied <= 1; occupied++) {
        geometry_fixture f; geometry_init(&f);
        fd2_field_game *g = &f.game;
        g->units.items[0].movement_points = 4;
        f.cells[3 * 8 + 2] = 1; /* long mode可过cost20；本回合该格不可达。 */
        if (occupied) {
            g->units.items[1].x = 6; g->units.items[1].y = 3;
            g->units.items[1].flags = 4; /* AI禁行动的对手仍可作为anchor。 */
        }
        fd2_field_unit target = g->units.items[1];
        CHECK(commit_ai_move_toward_anchor(g, 0, 6, 3, NULL, 0, FIELD_PATH_BROWSE) == 1);
        /* 长路径越过(2,3)后，(3,3)经北/南绕行仍在4预算内。
         * 未命中即break会丢后续命中；未清40/80又到不了occupied anchor，
         * 两种错误都会退化向原anchor选(4,2)。 */
        CHECK(g->units.items[0].x == 3 && g->units.items[0].y == 3);
        CHECK(!memcmp(&target, &g->units.items[1], sizeof(target)));
        CHECK(g->failure == FD2_FIELD_FAILURE_NONE && f.rng_calls == 0);
        CHECK(!fd2_field_unit_has_acted(&g->units.items[0]) && !g->event_log_count);
    }
    return 0;
}

static int test_local_profiles_zero_budget_and_no_hp_gate(void) {
    const uint8_t unit_ids[] = {7, 28, 7, 28};
    const uint8_t text_ids[] = {0, 0, 28, 28};
    const uint8_t expected_x[] = {3, 1, 2, 2};
    for (size_t which = 0; which < 4; which++) {
        geometry_fixture f; geometry_init(&f);
        fd2_field_game *g = &f.game;
        fd2_field_unit *actor = &g->units.items[0];
        actor->y = 1; actor->movement_points = 2;
        actor->movement_profile = 3; actor->race = 4;
        actor->unit_id = unit_ids[which]; actor->text_id = text_ids[which];
        f.attrs[0].movement_cost_class = 3; /* raw3 cost3、immune19 cost1、text28→1 cost2。 */
        CHECK(commit_ai_move_toward_anchor(g, 0, 4, 1, NULL, 0, FIELD_PATH_BROWSE) == (which == 1 ? 0 : 1));
        CHECK(actor->x == expected_x[which] && actor->y == 1);
        CHECK(actor->movement_profile == 3 && actor->race == 4);
        CHECK(actor->unit_id == unit_ids[which] && actor->text_id == text_ids[which]);
        CHECK(f.rng_calls == 0 && !fd2_field_unit_has_acted(actor));
    }
    geometry_fixture f; geometry_init(&f);
    fd2_field_game *g = &f.game;
    g->units.items[0].movement_points = 0;
    g->pending_cell_event = (fd2_field_pending_cell_event){1, 42, 4};
    CHECK(commit_ai_move_toward_anchor(g, 0, 6, 3, NULL, 0, FIELD_PATH_BROWSE) == 0);
    CHECK(g->units.items[0].x == 1 && g->units.items[0].y == 3);
    CHECK(g->pending_cell_event.valid && g->pending_cell_event.action == 42);
    CHECK(g->failure == FD2_FIELD_FAILURE_NONE && f.rng_calls == 0);
    g->units.items[0].movement_points = 3;
    g->units.items[0].hp = 0;
    CHECK(commit_ai_move_toward_anchor(g, 0, 4, 3, NULL, 0, FIELD_PATH_BROWSE) == 1);
    CHECK(g->units.items[0].x == 4 && g->units.items[0].y == 3);
    CHECK(g->units.items[0].hp == 0 && g->units.items[0].flags == 0 && f.rng_calls == 0);
    return 0;
}

static int test_reject_geometry_before_commit(void) {
    geometry_fixture f; geometry_init(&f);
    fd2_field_game *g = &f.game;
    g->units.items[0].movement_profile = UINT8_MAX;
    fd2_field_game before = *g;
    CHECK(commit_ai_move_toward_anchor(g, 0, 6, 3, NULL, 0, FIELD_PATH_BROWSE) == -1);
    CHECK(!memcmp(g, &before, sizeof(before)) && f.rng_calls == 0);
    return 0;
}

static int test_start_zoc_for_both_selector_groups(void) {
    for (uint8_t side = 0; side <= 2; side++) {
        geometry_fixture f; geometry_init(&f);
        fd2_field_game *g = &f.game;
        geometry_unit(&g->units.items[0], side, 3, 3);
        geometry_unit(&g->units.items[1], side == 0 ? 2 : 0, 3, 2);
        g->units.count = 3;
        geometry_unit(&g->units.items[2], side == 0 ? 0 : side == 1 ? 2 : 1, 4, 3);
        g->units.items[2].flags = 0x84; /* acted/AI-ineligible仍是可见同组占格。 */
        fd2_field_game before = *g;
        fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
        CHECK(fd2_field_game_compute_move_range(g, 0, &range) == 0);
        /* 起点邻敌不禁止离开；右侧经同组actor格继续，不能停在友方格。 */
        CHECK(fd2_field_path_is_destination(&range, 6, 3));
        CHECK(!fd2_field_path_is_destination(&range, 4, 3));
        CHECK(!fd2_field_path_is_destination(&range, 3, 2));
        /* 新进入的邻敌格仍必须停止，不能再迈至(2,1)。 */
        CHECK(fd2_field_path_is_destination(&range, 2, 2));
        CHECK(!fd2_field_path_is_destination(&range, 2, 1));
        fd2_field_path_close(&range);
        const uint8_t *profile = fd2_field_movement_profile_get(1);
        uint8_t directions[64]; size_t length = 0;
        for (int mode = FD2_FIELD_MOVE_PATH_NORMAL; mode <= FD2_FIELD_MOVE_PATH_EQUAL_ROUTE; mode++) {
            CHECK(fd2_field_move_path_find(&g->map, &g->terrain, &g->units, 0,
                profile, FD2_FIELD_MOVEMENT_COST_CLASS_COUNT, 3,
                (fd2_field_move_path_mode)mode, 6, 3, directions, sizeof(directions),
                &length, NULL, NULL) == 1);
            CHECK(length == 3 && directions[0] == 3 && directions[1] == 3 && directions[2] == 3);
            CHECK(fd2_field_move_path_find(&g->map, &g->terrain, &g->units, 0,
                profile, FD2_FIELD_MOVEMENT_COST_CLASS_COUNT, 3,
                (fd2_field_move_path_mode)mode, 2, 1, directions, sizeof(directions),
                &length, NULL, NULL) == 0);
        }
        int witness_x = -1, witness_y = -1;
        CHECK(fd2_field_move_path_find(&g->map, &g->terrain, &g->units, 0,
            profile, FD2_FIELD_MOVEMENT_COST_CLASS_COUNT, 3,
            FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS, 0, 0, NULL, sizeof(directions),
            &length, &witness_x, &witness_y) == 1);
        CHECK(witness_x == 3 && witness_y == 2);
        CHECK(!memcmp(g, &before, sizeof(before)) && f.rng_calls == 0);
        /* 实际共享anchor调用也能离开起点，未改变显式side分组。 */
        CHECK(commit_ai_move_toward_anchor(g, 0, 6, 3, NULL, 0, FIELD_PATH_BROWSE) == 1);
        CHECK(g->units.items[0].x == 6 && g->units.items[0].y == 3);
        CHECK(g->units.items[2].x == 4 && g->units.items[2].flags == 0x84 && f.rng_calls == 0);
    }
    return 0;
}

static int test_path_bad_origin_rejected_before_outputs(void) {
    for (int which = 0; which < 3; which++) {
        geometry_fixture f; geometry_init(&f);
        fd2_field_game *g = &f.game;
        g->units.items[0].x = which == 1 ? 3 : which == 0 ? 8 : UINT8_MAX;
        g->units.items[0].y = which == 1 ? 8 : 3;
        fd2_field_game before = *g;
        const uint8_t *profile = fd2_field_movement_profile_get(1);
        for (int mode = FD2_FIELD_MOVE_PATH_NORMAL; mode <= FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS; mode++) {
            uint8_t path[64], original[64];
            memset(path, 0xa5, sizeof(path)); memcpy(original, path, sizeof(path));
            size_t length = 12345;
            int witness_x = 77, witness_y = 88;
            CHECK(fd2_field_move_path_find(&g->map, &g->terrain, &g->units, 0,
                profile, FD2_FIELD_MOVEMENT_COST_CLASS_COUNT, 3,
                (fd2_field_move_path_mode)mode, 6, 3, path, sizeof(path),
                &length, &witness_x, &witness_y) == -1);
            CHECK(length == 12345 && witness_x == 77 && witness_y == 88);
            CHECK(!memcmp(path, original, sizeof(path)));
            CHECK(!memcmp(g, &before, sizeof(before)) && f.rng_calls == 0);
        }
    }
    return 0;
}

static int test_real_start_fixed_anchor_geometry(void) {
    /* 原版只读START夹具，非通用初始坐标；独立原版研究probe的固定anchor
     * (7,15)四终点来自 original-march-round2.SAV。只检共享几何，不跑AI、
     * phase、handler/acted尾、WGC或Save，不把此测试称March运行验收。 */
    fd2_archive fdother = {0};
    fd2_vga vga = {0};
    fd2_field_game game;
    CHECK(fd2_archive_open(&fdother, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_field_game_open(&game, &vga, &fdother, 0) == 0);
    CHECK(fd2_field_game_load_active(&game, "original_game/START.SAV") == 0);
    CHECK(game.stage == 0 && game.turn_number == 1 && game.units.count == 12 && game.roster_count == 4);
    const uint8_t ids[] = {0, 9, 4, 30};
    const uint8_t initial_x[] = {7, 10, 8, 11}, initial_y[] = {14, 15, 16, 17};
    const uint8_t final_x[] = {7, 7, 6, 8}, final_y[] = {15, 14, 15, 16};
    fd2_field_units original_units = game.units;
    size_t calls = 0;
    game.attack_rng = geometry_rng; game.attack_rng_userdata = &calls;
    for (size_t i = 0; i < 4; i++) {
        CHECK(game.units.items[i].unit_id == ids[i] && game.units.items[i].side == 2);
        CHECK(game.units.items[i].x == initial_x[i] && game.units.items[i].y == initial_y[i]);
        CHECK(commit_ai_move_toward_anchor(&game, i, 7, 15, NULL, 0, FIELD_PATH_BROWSE) == 1);
        CHECK(game.units.items[i].x == final_x[i] && game.units.items[i].y == final_y[i]);
        CHECK(game.units.items[i].side == 2 && !fd2_field_unit_has_acted(&game.units.items[i]));
        CHECK(game.move_path == NULL && game.move_path_capacity == 0);
    }
    CHECK(!memcmp(game.units.items + 4, original_units.items + 4,
                  8 * sizeof(game.units.items[0])));
    CHECK(game.failure == FD2_FIELD_FAILURE_NONE && game.event_log_count == 0 && calls == 0);
    CHECK(game.turn_number == 1 && game.active_side == 2 && !game.player_post_waiting_phase);
    fd2_field_game_close(&game);
    fd2_archive_close(&fdother);
    return 0;
}

int main(void) {
    if (test_requested_anchor_precedes_destination_and_friend_block() ||
        test_whole_long_route_can_reenter_reachable_area() ||
        test_local_profiles_zero_budget_and_no_hp_gate() ||
        test_reject_geometry_before_commit() ||
        test_start_zoc_for_both_selector_groups() ||
        test_path_bad_origin_rejected_before_outputs() ||
        test_real_start_fixed_anchor_geometry()) return 1;
    puts("field_geometry_test: ok");
    return 0;
}
