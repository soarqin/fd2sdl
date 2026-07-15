/* M7.3 敌方物理 plan/commit 事务测试。
 * 逆向依据：field_ai_move_toward_cell @0x39d8c 最终重建并播放路径；
 * field_physical_exchange @0x3a6a2 在移动后提交进攻、反击和死亡清理。
 */
#include <stdio.h>
#include <stdlib.h>
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

typedef struct {
    const uint32_t *values;
    size_t count;
    size_t cursor;
} test_rng;

static uint32_t next_roll(void *userdata) {
    test_rng *rng = userdata;
    if (!rng || rng->cursor >= rng->count) return 0;
    return rng->values[rng->cursor++];
}

static void set_unit(fd2_field_unit *unit, uint8_t side, int x, int y) {
    memset(unit, 0, sizeof(*unit));
    unit->side = side;
    unit->x = (uint8_t)x;
    unit->y = (uint8_t)y;
    unit->hp = 50;
    unit->hp_max = 50;
    unit->attack = 200;
    unit->defense = 1;
    unit->accuracy = 100;
    unit->movement_profile = 1;
    unit->movement_points = 1;
}

static void equip_weapon(fd2_field_unit *unit) {
    uint8_t *record = (uint8_t *)unit;
    record[0x0a] = 0x40;
    record[0x0b] = 0; /* item 0，最小／最大射程均为 1。 */
}

static void init_game(fd2_field_game *game,
                      uint32_t *cells,
                      fd2_terrain_attr *attrs,
                      test_rng *rng) {
    memset(game, 0, sizeof(*game));
    memset(cells, 0, 8u * 8u * sizeof(*cells));
    memset(attrs, 0, sizeof(*attrs));
    attrs[0].movement_cost_class = 1;
    game->ready = 1;
    game->active_side = 0;
    game->selected_unit = -1;
    game->command_selected = -1;
    game->detail_unit = -1;
    game->attack_target = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->map.width = 8;
    game->map.height = 8;
    game->map.cells = cells;
    game->terrain.attrs = attrs;
    game->terrain.attr_count = 1;
    game->units.count = 2;
    set_unit(&game->units.items[0], 0, 1, 1);
    game->units.items[0].movement_points = 2;
    set_unit(&game->units.items[1], 2, 3, 1);
    equip_weapon(&game->units.items[0]);
    game->attack_rng = next_roll;
    game->attack_rng_userdata = rng;
}

static int build_plan(fd2_field_game *game,
                      fd2_field_ai_physical_candidate *plan) {
    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    int result = fd2_field_game_compute_move_range(game, 0, &range);
    if (result == 0)
        result = fd2_field_ai_choose_physical_candidate(
            &game->map, &game->terrain, &game->units,
            0, 0, &range, plan);
    fd2_field_path_close(&range);
    return result;
}

static int test_commit(void) {
    static const uint32_t rolls[] = {50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);

    fd2_field_ai_physical_candidate plan;
    CHECK(build_plan(&game, &plan) == 0);
    CHECK(plan.unit_index == 1);
    CHECK(plan.destination_x == 2 && plan.destination_y == 1);
    CHECK(plan.priority >= 6);
    CHECK(fd2_field_game_commit_ai_physical(
              &game, 0, 0, &plan, NULL, 0) == 0);
    CHECK(game.units.items[0].x == 2 && game.units.items[0].y == 1);
    CHECK(game.units.items[0].direction == 3);
    CHECK(game.units.items[1].hp == 0);
    CHECK(game.units.items[1].flags == 1);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.selected_unit == -1);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_BROWSE);
    CHECK(rng.cursor == 4);
    free(game.move_path);
    return 0;
}

static int failing_critical_hook(
        const fd2_field_unit *attacker,
        void *userdata,
        uint8_t *base_chance) {
    (void)attacker;
    (void)userdata;
    (void)base_chance;
    return -1;
}

static int test_automatic_mode0(void) {
    static const uint32_t rolls[] = {50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    /* stage=0 保留 script 验证用 wait skeleton；非零 stage 才走 M7.3
     * automatic mode0 integration. */
    game.stage = 1;

    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    CHECK(game.units.items[0].x == 2 && game.units.items[0].y == 1);
    CHECK(game.units.items[1].hp == 0 && game.units.items[1].flags == 1);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    /* The target is finalized hidden; remaining-unit accounting skips it and
     * the scheduler may advance straight through the empty side-1 phase. */
    CHECK(game.active_side == 2);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_BROWSE);
    CHECK(game.command_selected == -1);
    CHECK(rng.cursor == 4);
    return 0;
}

static int test_stale_plan(void) {
    static const uint32_t rolls[] = {50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);

    fd2_field_ai_physical_candidate plan;
    CHECK(build_plan(&game, &plan) == 0);
    fd2_field_units before = game.units;
    plan.destination_x = 0;
    CHECK(fd2_field_game_commit_ai_physical(
              &game, 0, 0, &plan, NULL, 0) == -1);
    CHECK(memcmp(&before, &game.units, sizeof(before)) == 0);
    CHECK(rng.cursor == 0);
    CHECK(game.selected_unit == -1);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_BROWSE);

    /* 可失败 callback 必须在移动和 RNG 前拒绝；否则失败重试会看到不同
     * 随机流。生产 AI 提交固定使用已确认 profile 暴击表。 */
    plan.destination_x = 2;
    plan.destination_y = 1;
    game.attack_critical_base = failing_critical_hook;
    before = game.units;
    CHECK(fd2_field_game_commit_ai_physical(
              &game, 0, 0, &plan, NULL, 0) == -1);
    CHECK(memcmp(&before, &game.units, sizeof(before)) == 0);
    CHECK(rng.cursor == 0);
    return 0;
}

int main(void) {
    if (test_commit() != 0 || test_automatic_mode0() != 0 ||
        test_stale_plan() != 0) return 1;
    puts("field_ai_commit_test: ok");
    return 0;
}
