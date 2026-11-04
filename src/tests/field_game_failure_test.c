/* B0 必需事件与动作提交失败边界回归。
 * 依据现有 field_actor_path_play @0x13488、field_turn_event_check @0x1a813、
 * field_physical_exchange @0x1548e 与 AI common tail 的已登记执行顺序；
 * 合成 session 只验证 SDL 停止语义，不作为原版玩法对照。
 */
#include <stdio.h>
#include <string.h>

#include "field_ai.h"
#include "field_game.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

typedef struct { size_t calls; } test_rng;

static uint32_t next_roll(void *userdata) {
    static const uint32_t values[] = {50, 0, 99, 0};
    test_rng *rng = userdata;
    return values[rng->calls++ % (sizeof(values) / sizeof(values[0]))];
}

static void set_unit(fd2_field_unit *unit, uint8_t side, int x, int y) {
    memset(unit, 0, sizeof(*unit));
    unit->side = side;
    unit->x = (uint8_t)x;
    unit->y = (uint8_t)y;
    unit->hp = unit->hp_max = 50;
    unit->attack = 200;
    unit->defense = 1;
    unit->accuracy = 100;
    unit->movement_profile = 1;
    unit->movement_points = 2;
    uint8_t *record = (uint8_t *)unit;
    record[0x0a] = 0x40;
    record[0x0b] = 0;
}

static void init_game(fd2_field_game *game, uint32_t cells[64],
                      fd2_terrain_attr *attr, test_rng *rng) {
    memset(game, 0, sizeof(*game));
    memset(cells, 0, 64 * sizeof(*cells));
    memset(attr, 0, sizeof(*attr));
    attr->movement_cost_class = 1;
    game->ready = 1;
    game->turn_number = 1;
    game->active_side = 2;
    game->selected_unit = 0;
    game->command_selected = FD2_FIELD_COMMAND_WAIT;
    game->attack_target = -1;
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    game->map.width = game->map.height = 8;
    game->map.cells = cells;
    game->terrain.attrs = attr;
    game->terrain.attr_count = 1;
    game->units.count = 2;
    set_unit(&game->units.items[0], 2, 1, 1);
    set_unit(&game->units.items[1], 0, 6, 6);
    game->attack_rng = next_roll;
    game->attack_rng_userdata = rng;
}

static void unknown_cell(fd2_field_game *game, uint32_t *cells,
                         int x, int y, uint8_t match) {
    cells[(size_t)y * 8 + (size_t)x] = 1u << 16;
    game->metadata.cell_lookup[0] = (fd2_field_cell_lookup){9, match};
}

static int frozen_after_failure(fd2_field_game *game, test_rng *rng) {
    fd2_field_game before = *game;
    size_t calls = rng->calls;
    CHECK(fd2_field_game_finish_unit_action(game) == -1);
    CHECK(fd2_field_game_confirm_cursor(game) == -1);
    CHECK(fd2_field_game_execute_move(game, NULL, 0) == -1);
    CHECK(fd2_field_game_end_active_phase(game) == -1);
    CHECK(fd2_field_game_process_automatic_action(game) == -1);
    CHECK(memcmp(game, &before, sizeof(before)) == 0);
    CHECK(rng->calls == calls);
    return 0;
}

static void keep_player_phase(fd2_field_game *game) {
    game->units.count = 3;
    set_unit(&game->units.items[2], 2, 7, 7);
}

static int test_player_wait_unknown_cell(void) {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attr;
    test_rng rng = {0};
    init_game(&game, cells, &attr, &rng);
    keep_player_phase(&game);
    unknown_cell(&game, cells, 1, 1, 1);
    CHECK(fd2_field_game_finish_unit_action(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
    CHECK(game.failure_notice.kind == FD2_FIELD_EVENT_CELL);
    CHECK(game.failure_notice.action == 9 && game.failure_notice.unit_index == 0);
    CHECK(game.event_log_count == 1 && game.unhandled_event_count == 1);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.active_side == 2 && game.turn_number == 1);
    CHECK(rng.calls == 0);
    return frozen_after_failure(&game, &rng);
}

/* @0x18890整路径仅收集pending，未知handler必须等COMMAND完成后才失败。 */
static int test_player_move_finishes_before_event_dispatch(void) {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attr;
    test_rng rng = {0};
    uint8_t path[] = {3, 3};
    init_game(&game, cells, &attr, &rng);
    unknown_cell(&game, cells, 2, 1, 0);
    game.interaction = FD2_FIELD_INTERACTION_MOVING;
    game.move_path = path;
    game.move_path_length = game.move_path_capacity = sizeof(path);
    game.move_preview_valid = 1;
    CHECK(fd2_field_game_execute_move(&game, NULL, 0) == 0);
    CHECK(game.failure == FD2_FIELD_FAILURE_NONE);
    CHECK(game.units.items[0].x == 3 && game.units.items[0].y == 1);
    CHECK(game.cursor_cell_x == 3 && game.cursor_cell_y == 1);
    CHECK(game.event_log_count == 0 && game.pending_cell_event.valid);
    CHECK(game.pending_cell_event.action == 9 && game.pending_cell_event.source_slot == 0);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_COMMAND);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.calls == 0);
    return 0;
}

static int test_command_cancel_reentry_clears_pending(void) {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attr;
    test_rng rng = {0};
    init_game(&game, cells, &attr, &rng);
    uint8_t path[64];
    game.move_path = path;
    game.move_path_capacity = sizeof(path);
    game.move_origin_x = game.units.items[0].x;
    game.move_origin_y = game.units.items[0].y;
    game.units.items[0].x = 2;
    game.pending_cell_event = (fd2_field_pending_cell_event){1, 42, 4};
    CHECK(fd2_field_game_cancel_selection(&game) == 1);
    CHECK(!game.pending_cell_event.valid);
    CHECK(game.units.items[0].x == 1 && game.units.items[0].y == 1);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.event_log_count == 0 && rng.calls == 0);
    fd2_field_path_close(&game.move_range);
    return 0;
}

static int test_invalid_cell_data_is_not_no_event(void) {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attr;
    test_rng rng = {0};
    init_game(&game, cells, &attr, &rng);
    cells[1u * 8u + 1u] = 1; /* attr_count 只有 1，该 terrain ID 越界。 */
    CHECK(fd2_field_game_finish_unit_action(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_DATA);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.calls == 0);
    return frozen_after_failure(&game, &rng);
}

static int test_player_attack_preserves_committed_damage(void) {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attr;
    test_rng rng = {0};
    init_game(&game, cells, &attr, &rng);
    keep_player_phase(&game);
    game.units.items[1].x = 2;
    game.units.items[1].y = 1;
    unknown_cell(&game, cells, 1, 1, 1);
    CHECK(fd2_field_game_begin_attack(&game) == 0);
    game.cursor_cell_x = 2;
    game.cursor_cell_y = 1;
    CHECK(fd2_field_game_resolve_attack(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
    CHECK(game.units.items[1].hp == 0);
    CHECK(fd2_field_unit_is_hidden(&game.units.items[1]));
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.calls == 4);
    CHECK(frozen_after_failure(&game, &rng) == 0);
    fd2_field_path_close(&game.move_range);
    return 0;
}

static int reject_counter_after_rng(const fd2_field_unit *unit, void *userdata,
                                    uint8_t *base) {
    test_rng *rng = userdata;
    /* preflight 时 RNG 尚未消费；反击真正执行时注入后置失败。 */
    if (unit->side == 0 && rng->calls != 0) return -1;
    *base = 0;
    return 0;
}

static int test_counter_failure_keeps_first_strike_and_rng(void) {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attr;
    test_rng rng = {0};
    init_game(&game, cells, &attr, &rng);
    game.units.items[0].attack = 20;
    game.units.items[0].hp = game.units.items[0].hp_max = 100;
    game.units.items[1].x = 2;
    game.units.items[1].y = 1;
    game.units.items[1].hp = game.units.items[1].hp_max = 100;
    game.attack_critical_base = reject_counter_after_rng;
    game.attack_critical_base_userdata = &rng;
    CHECK(fd2_field_game_begin_attack(&game) == 0);
    game.cursor_cell_x = 2;
    game.cursor_cell_y = 1;
    CHECK(fd2_field_game_resolve_attack(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_ACTION);
    CHECK(game.units.items[1].hp > 0 && game.units.items[1].hp < 100);
    CHECK(game.units.items[0].hp == 100 && !fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.calls == 5);
    uint16_t committed_hp = game.units.items[1].hp;
    CHECK(fd2_field_game_resolve_attack(&game) == -1);
    CHECK(rng.calls == 5 && game.units.items[1].hp == committed_hp);
    CHECK(frozen_after_failure(&game, &rng) == 0);
    fd2_field_path_close(&game.move_range);
    return 0;
}

static uint32_t next_double_roll(void *userdata) {
    static const uint32_t values[] = {0, 0, 99, 0};
    test_rng *rng = userdata;
    return values[rng->calls++ % (sizeof(values) / sizeof(values[0]))];
}

static int reject_second_strike(const fd2_field_unit *unit, void *userdata,
                                uint8_t *base) {
    (void)unit;
    test_rng *rng = userdata;
    if (rng->calls >= 4) return -1;
    *base = 0;
    return 0;
}

static int test_double_strike_failure_keeps_first_strike(void) {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attr;
    test_rng rng = {0};
    init_game(&game, cells, &attr, &rng);
    game.units.items[0].attack = 20;
    game.units.items[1].x = 2;
    game.units.items[1].y = 1;
    game.units.items[1].hp = game.units.items[1].hp_max = 100;
    game.attack_rng = next_double_roll;
    game.attack_critical_base = reject_second_strike;
    game.attack_critical_base_userdata = &rng;
    CHECK(fd2_field_game_begin_attack(&game) == 0);
    game.cursor_cell_x = 2;
    game.cursor_cell_y = 1;
    CHECK(fd2_field_game_resolve_attack(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_ACTION);
    CHECK(game.units.items[1].hp > 0 && game.units.items[1].hp < 100);
    CHECK(rng.calls == 4);
    CHECK(frozen_after_failure(&game, &rng) == 0);
    fd2_field_path_close(&game.move_range);
    return 0;
}

static void fill_notices(fd2_field_game *game, size_t count) {
    game->event_log_count = count;
    game->event_total_count = (uint32_t)count;
    for (size_t i = 0; i < count; i++) {
        game->event_log[i].kind = FD2_FIELD_EVENT_TURN;
        game->event_log[i].handled = 1;
        game->event_log[i].presentation_deferred = 1;
        game->event_log[i].slot = i;
    }
}

static int test_notice_last_slot_and_full_capacity(void) {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attr;
    test_rng rng = {0};
    init_game(&game, cells, &attr, &rng);
    keep_player_phase(&game);
    fill_notices(&game, FD2_FIELD_EVENT_LOG_CAPACITY - 1);
    unknown_cell(&game, cells, 1, 1, 1);
    CHECK(fd2_field_game_finish_unit_action(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
    CHECK(game.event_log_count == FD2_FIELD_EVENT_LOG_CAPACITY);
    CHECK(game.event_log[FD2_FIELD_EVENT_LOG_CAPACITY - 1].action == 9);
    CHECK(game.event_log[0].presentation_deferred == 1);

    init_game(&game, cells, &attr, &rng);
    keep_player_phase(&game);
    fill_notices(&game, FD2_FIELD_EVENT_LOG_CAPACITY);
    unknown_cell(&game, cells, 1, 1, 1);
    fd2_field_event_notice before[FD2_FIELD_EVENT_LOG_CAPACITY];
    memcpy(before, game.event_log, sizeof(before));
    CHECK(fd2_field_game_finish_unit_action(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_CAPACITY);
    CHECK(game.event_log_count == FD2_FIELD_EVENT_LOG_CAPACITY);
    CHECK(memcmp(before, game.event_log, sizeof(before)) == 0);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.calls == 0);
    return frozen_after_failure(&game, &rng);
}

static int test_turn_capacity_before_handler_and_status(void) {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attr;
    test_rng rng = {0};
    init_game(&game, cells, &attr, &rng);
    fill_notices(&game, FD2_FIELD_EVENT_LOG_CAPACITY);
    game.selected_unit = -1;
    game.interaction = FD2_FIELD_INTERACTION_BROWSE;
    game.units.items[1].side = 1;
    game.units.items[1].detail_status[0] = 1;
    game.units.items[1].attack_status = 2;
    game.metadata.turn_events[0] = (fd2_field_turn_event){.turn = 1, .action = 0, .phase = 1};
    fd2_field_units units = game.units;
    CHECK(fd2_field_game_end_active_phase(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_CAPACITY);
    CHECK(game.failure_notice.kind == FD2_FIELD_EVENT_TURN);
    CHECK(game.units.count == units.count);
    CHECK(game.units.items[1].hp == units.items[1].hp);
    CHECK(game.units.items[1].attack_status == 2);
    CHECK(game.event_log_count == FD2_FIELD_EVENT_LOG_CAPACITY);
    CHECK(rng.calls == 0);
    return frozen_after_failure(&game, &rng);
}

static int test_unknown_turn_stops_later_events_and_status(void) {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attr;
    test_rng rng = {0};
    init_game(&game, cells, &attr, &rng);
    game.selected_unit = -1;
    game.interaction = FD2_FIELD_INTERACTION_BROWSE;
    game.units.items[1].side = 1;
    game.units.items[1].detail_status[0] = 1;
    game.units.items[1].attack_status = 2;
    game.metadata.turn_events[0] = (fd2_field_turn_event){.turn = 1, .action = 0xff, .phase = 1};
    game.metadata.turn_events[1] = (fd2_field_turn_event){.turn = 1, .action = 0, .phase = 1};
    CHECK(fd2_field_game_end_active_phase(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
    CHECK(game.failure_notice.kind == FD2_FIELD_EVENT_TURN);
    CHECK(game.failure_notice.action == 0xff && game.failure_notice.slot == 0);
    CHECK(game.event_log_count == 1 && game.event_log[0].action == 0xff);
    CHECK(game.units.count == 2 && game.units.items[1].hp == 50);
    CHECK(game.units.items[1].attack_status == 2);
    CHECK(rng.calls == 0);
    return frozen_after_failure(&game, &rng);
}

static int test_turn_execution_failure_stops_later_events(void) {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attr;
    test_rng rng = {0};
    init_game(&game, cells, &attr, &rng);
    game.selected_unit = -1;
    game.interaction = FD2_FIELD_INTERACTION_BROWSE;
    game.units.items[1].side = 1;
    game.units.items[1].detail_status[0] = 1;
    /* 已知 action 0 缺少 group 3 的 placement，应原子拒绝其逻辑部分；
     * 下一条 unknown action 与 poison tick 都不能继续。 */
    game.metadata.turn_events[0] = (fd2_field_turn_event){.turn = 1, .action = 0, .phase = 1};
    game.metadata.turn_events[1] = (fd2_field_turn_event){.turn = 1, .action = 0xff, .phase = 1};
    CHECK(fd2_field_game_end_active_phase(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_EXECUTION);
    CHECK(game.failure_notice.action == 0 && game.failure_notice.slot == 0);
    CHECK(game.event_log_count == 1 && !game.event_log[0].handled);
    CHECK(game.units.count == 2 && game.units.items[1].hp == 50);
    CHECK(game.camera_cell_x == 0 && game.camera_cell_y == 0);
    return frozen_after_failure(&game, &rng);
}

static int test_status_recompute_failure_stops_next_actor(void) {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attr;
    test_rng rng = {0};
    init_game(&game, cells, &attr, &rng);
    game.selected_unit = -1;
    game.interaction = FD2_FIELD_INTERACTION_BROWSE;
    game.units.items[1].side = 1;
    game.units.items[1].attack_status = 1;
    ((uint8_t *)&game.units.items[1])[0x0b] = 255; /* 超出215项装备表。 */
    game.units.count = 3;
    set_unit(&game.units.items[2], 1, 5, 5);
    game.units.items[2].attack_status = 2;
    CHECK(fd2_field_game_end_active_phase(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_ACTION);
    CHECK(game.failure_notice.unit_index == 1);
    CHECK(game.units.items[1].attack_status == 0);
    CHECK(game.units.items[1].attack == 200);
    CHECK(game.units.items[2].attack_status == 2);
    CHECK(game.event_log_count == 0 && rng.calls == 0);
    return frozen_after_failure(&game, &rng);
}

static int test_ai_commit_failure_does_not_rollback_or_repeat(void) {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attr;
    test_rng rng = {0};
    init_game(&game, cells, &attr, &rng);
    game.active_side = 0;
    game.selected_unit = -1;
    game.interaction = FD2_FIELD_INTERACTION_BROWSE;
    game.units.items[0].side = 0;
    game.units.items[1].side = 2;
    game.units.items[1].x = 4;
    game.units.items[1].y = 1;
    cells[1u * 8u + 2u] = 1u << 16;
    cells[1u * 8u + 3u] = 2u << 16;
    game.metadata.cell_lookup[0] = (fd2_field_cell_lookup){7, 0};
    game.metadata.cell_lookup[1] = (fd2_field_cell_lookup){9, 1};

    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    fd2_field_ai_physical_candidate plan;
    CHECK(fd2_field_game_compute_move_range(&game, 0, &range) == 0);
    CHECK(fd2_field_ai_choose_physical_candidate(&game.map, &game.terrain,
              &game.units, 0, 0, &range, &plan) == 0);
    fd2_field_path_close(&range);
    CHECK(plan.destination_x == 3 && plan.destination_y == 1);
    CHECK(fd2_field_game_commit_ai_physical(&game, 0, 0, &plan, NULL, 0) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
    CHECK(game.units.items[0].x == 3 && game.units.items[0].y == 1);
    CHECK(game.units.items[1].hp == 0);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.calls == 4);
    CHECK(game.event_log_count == 1 && game.event_log[0].action == 9);
    fd2_field_game before = game;
    CHECK(fd2_field_game_commit_ai_physical(&game, 0, 0, &plan, NULL, 0) == -1);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0 && rng.calls == 4);
    return frozen_after_failure(&game, &rng);
}

int main(void) {
    if (test_player_wait_unknown_cell() ||
        test_player_move_finishes_before_event_dispatch() ||
        test_command_cancel_reentry_clears_pending() ||
        test_invalid_cell_data_is_not_no_event() ||
        test_player_attack_preserves_committed_damage() ||
        test_counter_failure_keeps_first_strike_and_rng() ||
        test_double_strike_failure_keeps_first_strike() ||
        test_notice_last_slot_and_full_capacity() ||
        test_turn_capacity_before_handler_and_status() ||
        test_unknown_turn_stops_later_events_and_status() ||
        test_turn_execution_failure_stops_later_events() ||
        test_status_recompute_failure_stops_next_actor() ||
        test_ai_commit_failure_does_not_rollback_or_repeat())
        return 1;
    puts("field_game_failure_test: ok");
    return 0;
}
