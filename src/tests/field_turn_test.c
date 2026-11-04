/* B1 玩家结束回合回归：复现 field_turn_cycle_run @0x1a30b
 * （原登记 FUN_00017a53）首部恢复门，以及
 * field_empty_focus_menu_execute @0x16f55 selector 3 的直接调用。
 * 合成 session 验证 SDL 逻辑与停止边界，不替代原版运行对照。
 */
#include <stdio.h>
#include <string.h>

#include "field_game.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static uint32_t next_roll(void *userdata) {
    size_t *calls = userdata;
    (*calls)++;
    return 0;
}

static void init_game(fd2_field_game *game, size_t *rng_calls) {
    memset(game, 0, sizeof(*game));
    memset(game->metadata.turn_events, 0xff,
           sizeof(game->metadata.turn_events));
    game->ready = 1;
    game->turn_number = 1;
    game->active_side = 2;
    game->selected_unit = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->units.count = 13;
    for (size_t i = 0; i < game->units.count; i++) {
        fd2_field_unit *unit = &game->units.items[i];
        unit->side = 2;
        unit->hp = unit->hp_max = 50;
        unit->x = (uint8_t)(i % 8u);
        unit->y = (uint8_t)(i / 8u);
    }
    game->units.items[11].side = 1;
    game->units.items[11].hp = 30;
    game->units.items[11].detail_status[0] = 2;
    game->units.items[12].side = 0;
    game->units.items[12].hp = 30;
    game->attack_rng = next_roll;
    game->attack_rng_userdata = rng_calls;
}

static int reject_without_changes(fd2_field_game *game, size_t *rng_calls) {
    fd2_field_game before = *game;
    size_t calls = *rng_calls;
    CHECK(fd2_field_game_end_player_turn(game) == -1);
    CHECK(memcmp(game, &before, sizeof(before)) == 0);
    CHECK(*rng_calls == calls);
    return 0;
}

static int test_recovery_gates_and_phase_order(void) {
    fd2_field_game game;
    size_t rng_calls = 0;
    init_game(&game, &rng_calls);
    fd2_field_unit *units = game.units.items;
    units[0].hp = 35;
    units[1].hp = 49;
    units[2].hp = 25;
    units[2].flags = 0xa0; /* acted 与其他位保持。 */
    units[3].hp = 25;
    units[3].flags = 0x11; /* hidden 与其他位保持。 */
    units[4].hp = 25;
    units[4].detail_status[0] = 1;
    units[5].hp = 25;
    units[5].detail_status[1] = 1;
    units[7].hp = 3;
    units[7].hp_max = 4;
    units[8].hp = 20;
    units[8].flags = 0x04; /* 恢复门不检查 AI eligibility bit。 */
    units[9].hp = 10;
    units[9].attack_status = 2;
    units[9].defense_status = 3;
    units[9].hit_evasion_status = 4;
    units[10].hp = 60; /* 原版门为 !=，不是 <；合成超上限输入。 */

    CHECK(fd2_field_game_end_player_turn(&game) == 0);
    CHECK(game.active_side == 1 && game.turn_number == 1);
    CHECK(units[0].hp == 45 && units[0].flags == 0x80);
    CHECK(units[1].hp == 50 && units[1].flags == 0x80);
    CHECK(units[2].hp == 25 && units[2].flags == 0xa0);
    CHECK(units[3].hp == 25 && units[3].flags == 0x11);
    CHECK(units[4].hp == 25 && units[4].flags == 0);
    CHECK(units[4].detail_status[0] == 1);
    CHECK(units[5].hp == 25 && units[5].flags == 0);
    CHECK(units[5].detail_status[1] == 1);
    CHECK(units[6].hp == 50 && units[6].flags == 0);
    CHECK(units[7].hp == 3 && units[7].flags == 0x80);
    CHECK(units[8].hp == 30 && units[8].flags == 0x84);
    CHECK(units[9].hp == 20 && units[9].flags == 0x80);
    CHECK(units[9].attack_status == 2 && units[9].defense_status == 3 &&
          units[9].hit_evasion_status == 4);
    CHECK(units[10].hp == 50 && units[10].flags == 0x80);
    CHECK(units[11].hp == 25 && units[11].detail_status[0] == 1);
    CHECK(units[12].hp == 30 && units[12].flags == 0);
    CHECK(game.event_log_count == 0 && rng_calls == 0);
    return reject_without_changes(&game, &rng_calls);
}

static int test_player_turn_guards(void) {
    fd2_field_game game;
    size_t rng_calls = 0;
    CHECK(fd2_field_game_end_player_turn(NULL) == -1);
    init_game(&game, &rng_calls);
    game.ready = 0;
    CHECK(reject_without_changes(&game, &rng_calls) == 0);
    game.ready = 1;
    game.failure = FD2_FIELD_FAILURE_EVENT_UNSUPPORTED;
    CHECK(reject_without_changes(&game, &rng_calls) == 0);
    game.failure = FD2_FIELD_FAILURE_NONE;
    game.active_side = 0;
    CHECK(reject_without_changes(&game, &rng_calls) == 0);
    game.active_side = 2;
    game.selected_unit = 0;
    CHECK(reject_without_changes(&game, &rng_calls) == 0);
    game.selected_unit = -1;
    game.interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
    return reject_without_changes(&game, &rng_calls);
}

static int test_unknown_event_preserves_recovery_and_stops_status(void) {
    fd2_field_game game;
    size_t rng_calls = 0;
    init_game(&game, &rng_calls);
    game.units.items[0].hp = 35;
    game.metadata.turn_events[0] = (fd2_field_turn_event){
        .turn = 1, .action = 0xff, .phase = 1};
    game.metadata.turn_events[1] = (fd2_field_turn_event){
        .turn = 1, .action = 0, .phase = 1};
    CHECK(fd2_field_game_end_player_turn(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
    CHECK(game.active_side == 1 && game.turn_number == 1);
    CHECK(game.units.items[0].hp == 45 &&
          fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.failure_notice.kind == FD2_FIELD_EVENT_TURN &&
          game.failure_notice.action == 0xff && game.failure_notice.slot == 0);
    CHECK(game.event_log_count == 1 && !game.event_log[0].handled);
    CHECK(game.units.count == 13 && game.units.items[11].hp == 30 &&
          game.units.items[11].detail_status[0] == 2);
    CHECK(rng_calls == 0);
    return reject_without_changes(&game, &rng_calls);
}

static int test_capacity_stops_before_handler(void) {
    fd2_field_game game;
    size_t rng_calls = 0;
    init_game(&game, &rng_calls);
    game.units.items[0].hp = 35;
    game.event_log_count = FD2_FIELD_EVENT_LOG_CAPACITY;
    game.metadata.turn_events[0] = (fd2_field_turn_event){
        .turn = 1, .action = 0, .phase = 1};
    CHECK(fd2_field_game_end_player_turn(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_CAPACITY);
    CHECK(game.units.items[0].hp == 45 &&
          fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.event_log_count == FD2_FIELD_EVENT_LOG_CAPACITY);
    CHECK(game.event_total_count == 0 && game.units.count == 13);
    CHECK(game.units.items[11].hp == 30 &&
          game.units.items[11].detail_status[0] == 2);
    CHECK(rng_calls == 0);
    return reject_without_changes(&game, &rng_calls);
}

static int test_fast_skip_is_separate(void) {
    fd2_field_game game;
    size_t rng_calls = 0;
    init_game(&game, &rng_calls);
    game.units.items[0].hp = 35;
    CHECK(fd2_field_game_end_active_phase(&game) == 0);
    CHECK(game.active_side == 1 && game.units.items[0].hp == 35);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(fd2_field_unit_has_acted(&game.units.items[6]));
    CHECK(rng_calls == 0);
    return 0;
}

int main(void) {
    if (test_recovery_gates_and_phase_order() ||
        test_player_turn_guards() ||
        test_unknown_event_preserves_recovery_and_stops_status() ||
        test_capacity_stops_before_handler() ||
        test_fast_skip_is_separate())
        return 1;
    puts("field_turn_test: ok");
    return 0;
}
