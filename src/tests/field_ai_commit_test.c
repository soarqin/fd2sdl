/* M7.3 敌方物理 plan/commit 事务测试。
 * 逆向依据：field_ai_move_toward_cell @0x39d8c 最终重建并播放路径；
 * field_physical_exchange @0x3a6a2 在移动后提交进攻、反击和死亡清理。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_ai.h"
#include "field_game.h"
#include "field_magic.h"

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
    game.stage = 1;

    fd2_field_ai_physical_candidate plan;
    CHECK(build_plan(&game, &plan) == 0);
    CHECK(plan.unit_index == 1);
    CHECK(plan.destination_x == 2 && plan.destination_y == 1);
    CHECK(plan.priority >= 6);
    CHECK(fd2_field_game_commit_ai_physical(
              &game, 0, 0, &plan, NULL, 0) == 0);
    CHECK(game.units.items[0].x == 2 && game.units.items[0].y == 1);
    /* AI movement faces right while playing, then common tail calls 0x386f8
     * and resets record +0x03 for the complete actor table. */
    CHECK(game.units.items[0].direction == 0);
    CHECK(game.units.items[1].direction == 0);
    CHECK(game.units.items[1].hp == 0);
    CHECK(game.units.items[1].flags == 1);
    CHECK(game.battle_result == FD2_FIELD_BATTLE_DEFEAT);
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

static int test_common_tail_resets_all_directions(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.stage = 1;
    game.units.items[0].ai_behavior_raw = 2;
    game.units.items[0].movement_points = 0;
    game.units.items[0].direction = 3;
    game.units.items[1].direction = 2;
    game.units.items[1].x = 7;
    game.units.items[1].y = 7;

    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    CHECK(game.units.items[0].direction == 0);
    CHECK(game.units.items[1].direction == 0);
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_automatic_mode0(void) {
    static const uint32_t rolls[] = {50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    /* 该夹具直接构造非正式 session；stage 字段不参与 M7 调度。 */
    game.stage = 1;

    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    CHECK(game.units.items[0].x == 2 && game.units.items[0].y == 1);
    CHECK(game.units.items[1].hp == 0 && game.units.items[1].flags == 1);
    CHECK(game.battle_result == FD2_FIELD_BATTLE_DEFEAT);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    /* battle_result 终止自动 phase；不能再推进到 side 2。 */
    CHECK(game.active_side == 0);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_BROWSE);
    CHECK(game.command_selected == -1);
    CHECK(rng.cursor == 4);
    return 0;
}

static void teach_magic(fd2_field_unit *unit, uint8_t magic_id) {
    unit->data_09_1e[0x11u + magic_id / 8u] |=
        (uint8_t)(1u << (magic_id % 8u));
}

static int test_magic_commit(void) {
    static const uint32_t rolls[] = {0, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].mp = game.units.items[0].mp_max = 10;
    game.units.items[0].movement_profile = 1;
    game.units.items[1].movement_profile = 1;
    teach_magic(&game.units.items[0], 0);
    game.units.items[1].x = 2;
    game.units.items[1].y = 1;
    game.units.items[1].hp = game.units.items[1].hp_max = 100;

    fd2_field_ai_magic_candidate plan;
    CHECK(fd2_field_ai_choose_magic_candidate(
        &game.map, &game.terrain, &game.units, 0, 0, &plan) == 0);
    CHECK(plan.magic_id == 0 && plan.score >= 6);
    CHECK(fd2_field_game_commit_ai_magic(&game, 0, 0, &plan) == 0);
    CHECK(game.units.items[1].hp == 55);
    CHECK(game.units.items[0].mp == 10); /* record +5 仅作 gate，不猜测扣 MP。 */
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.cursor == 2);
    return 0;
}

static int test_magic_damage_dispatch_boundaries(void) {
    static const uint32_t rolls[] = {0, 0};
    test_rng rng = {rolls, 2, 0};
    fd2_field_units units = {0};
    units.count = 1;
    units.items[0].hp = units.items[0].hp_max = 2000;
    units.items[0].movement_profile = 19;
    units.items[0].race = 4;

    /* DS:0x1d01 的 ID 9 wrapper 直接进入 0x1c75e；profile 19/race 4
     * 不会触发 IDs 10..12 专属的 0x1f183 免疫门。 */
    CHECK(fd2_field_magic_apply_known_effect(
        9, 0, &units, next_roll, &rng) == 1);
    CHECK(units.items[0].hp == 1101);
    CHECK(rng.cursor == 2);

    /* ID 10 命中同一对象时在 RNG 前免疫。 */
    units.items[0].hp = 2000;
    rng.cursor = 0;
    CHECK(fd2_field_magic_apply_known_effect(
        10, 0, &units, next_roll, &rng) == 0);
    CHECK(units.items[0].hp == 2000);
    CHECK(rng.cursor == 0);

    /* unit ID 28 是 0x1f183 的显式例外，必须继续命中。 */
    units.items[0].unit_id = 28;
    CHECK(fd2_field_magic_apply_known_effect(
        10, 0, &units, next_roll, &rng) == 1);
    CHECK(units.items[0].hp == 1928);
    CHECK(rng.cursor == 2);
    return 0;
}

static int test_magic_heal_boundaries(void) {
    static const uint32_t rolls[] = {0, 99};
    test_rng rng = {rolls, 2, 0};
    fd2_field_units units = {0};
    units.count = 1;
    units.items[0].hp_max = 1000;

    /* IDs 13..16 的 wrappers 只改变 magic ID，均逐目标调用
     * 0x1c8ed→0x1c916。恢复不做命中判定；每目标只消费 spread RNG。 */
    units.items[0].hp = 100;
    CHECK(fd2_field_magic_apply_known_effect(
        13, 0, &units, next_roll, &rng) == 1);
    CHECK(units.items[0].hp == 163); /* 70*9/10 + 0 */
    CHECK(rng.cursor == 1);

    CHECK(fd2_field_magic_apply_known_effect(
        14, 0, &units, next_roll, &rng) == 1);
    CHECK(units.items[0].hp == 302); /* 140*9/10 + 99*140/1000 */
    CHECK(rng.cursor == 2);

    /* HP cap 在提交中执行；即使已满 HP，core 仍消费一次 RNG 并返回。 */
    static const uint32_t cap_rolls[] = {99, 50};
    rng = (test_rng){cap_rolls, 2, 0};
    units.items[0].hp = 990;
    CHECK(fd2_field_magic_apply_known_effect(
        15, 0, &units, next_roll, &rng) == 1);
    CHECK(units.items[0].hp == 1000);
    CHECK(rng.cursor == 1);
    CHECK(fd2_field_magic_apply_known_effect(
        16, 0, &units, next_roll, &rng) == 1);
    CHECK(units.items[0].hp == 1000);
    CHECK(rng.cursor == 2);
    return 0;
}

static int test_unknown_magic_rejected(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].side = 1;
    game.active_side = 1;
    teach_magic(&game.units.items[0], 23);
    game.units.items[0].mp = game.units.items[0].mp_max = 10;
    fd2_field_units before = game.units;
    CHECK(fd2_field_magic_apply_known_effect(
        23, 1, &game.units, next_roll, &rng) == -1);
    CHECK(fd2_field_magic_apply_known_effect(
        28, 1, &game.units, next_roll, &rng) == -1);
    CHECK(memcmp(&before, &game.units, sizeof(before)) == 0);
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_magic_status_commit(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].mp = game.units.items[0].mp_max = 10;
    game.units.items[0].attack = 100;
    game.units.items[0].defense = 80;
    game.units.items[0].accuracy = 90;
    game.units.items[0].evasion = 95;

    CHECK(fd2_field_magic_apply_known_effect(
        17, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].attack_status == 2);
    CHECK(game.units.items[0].attack == 116);
    CHECK(rng.cursor == 1);

    rng.cursor = 0;
    CHECK(fd2_field_magic_apply_known_effect(
        18, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].defense_status == 2);
    CHECK(game.units.items[0].defense == 93);

    rng.cursor = 0;
    CHECK(fd2_field_magic_apply_known_effect(
        19, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].hit_evasion_status == 2);
    CHECK(game.units.items[0].accuracy == 105);
    CHECK(game.units.items[0].evasion == 110);

    /* 已有状态在 RNG 和属性写入前退出。三种 wrapper 只检查各自 byte，
     * 不续期、不叠加，也不读取其他状态。 */
    rng.cursor = 0;
    CHECK(fd2_field_magic_apply_known_effect(
        17, 0, &game.units, next_roll, &rng) == 0);
    CHECK(game.units.items[0].attack_status == 2);
    CHECK(game.units.items[0].attack == 116);
    CHECK(fd2_field_magic_apply_known_effect(
        18, 0, &game.units, next_roll, &rng) == 0);
    CHECK(game.units.items[0].defense_status == 2);
    CHECK(game.units.items[0].defense == 93);
    CHECK(fd2_field_magic_apply_known_effect(
        19, 0, &game.units, next_roll, &rng) == 0);
    CHECK(game.units.items[0].accuracy == 105);
    CHECK(game.units.items[0].evasion == 110);
    CHECK(rng.cursor == 0);

    /* ID 19 的机器码是 16 位 add；锁定溢出回绕。 */
    game.units.items[0].hit_evasion_status = 0;
    game.units.items[0].accuracy = UINT16_MAX - 4u;
    game.units.items[0].evasion = UINT16_MAX - 9u;
    CHECK(fd2_field_magic_apply_known_effect(
        19, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].accuracy == 10);
    CHECK(game.units.items[0].evasion == 5);
    CHECK(rng.cursor == 1);

    /* IDs 17..19 只检查各自状态 byte，不排除 profile 25/26。 */
    game.units.items[0].movement_profile = 25;
    game.units.items[0].attack_status = 0;
    game.units.items[0].attack = 100;
    rng.cursor = 0;
    CHECK(fd2_field_magic_apply_known_effect(
        17, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].attack == 116);

    CHECK(fd2_field_magic_apply_known_effect(
        20, 0, &game.units, next_roll, &rng) == 0);
    CHECK(fd2_field_magic_apply_known_effect(
        21, 0, &game.units, next_roll, &rng) == 0);
    CHECK(rng.cursor == 1);
    game.units.items[0].detail_status[0] = 4;
    game.units.items[0].detail_status[1] = 5;
    CHECK(fd2_field_magic_apply_known_effect(
        20, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].detail_status[0] == 0);
    CHECK(game.units.items[0].detail_status[1] == 5);
    CHECK(fd2_field_magic_apply_known_effect(
        21, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].detail_status[1] == 0);
    CHECK(rng.cursor == 1);

    game.units.items[0].flags &= (uint8_t)~FD2_FIELD_UNIT_FLAG_ACTED;
    CHECK(fd2_field_magic_apply_known_effect(
        25, 0, &game.units, next_roll, &rng) == 0);
    game.units.items[0].flags = FD2_FIELD_UNIT_FLAG_AI_INELIGIBLE |
                               FD2_FIELD_UNIT_FLAG_ACTED;
    CHECK(fd2_field_magic_apply_known_effect(
        25, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].flags == FD2_FIELD_UNIT_FLAG_AI_INELIGIBLE);
    CHECK(rng.cursor == 1);

    static const uint32_t status_rolls[] = {0, 0, 0};
    rng = (test_rng){status_rolls, 3, 0};
    game.units.items[0].movement_profile = 1;
    game.units.items[0].hp = 50;
    CHECK(fd2_field_magic_apply_known_effect(
        22, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].hp == 41);
    CHECK(game.units.items[0].detail_status[2] == 2);
    CHECK(rng.cursor == 3);

    /* IDs 26/27 只是共享 0x22cda 的状态 offset wrappers，RNG 顺序与
     * ID 22 完全相同：gate、raw damage spread、duration。 */
    static const uint32_t shared_rolls[] = {0, 99, 3};
    rng = (test_rng){shared_rolls, 3, 0};
    game.units.items[0].hp = 50;
    game.units.items[0].detail_status[0] = 0;
    CHECK(fd2_field_magic_apply_known_effect(
        26, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].hp == 41);
    CHECK(game.units.items[0].detail_status[0] == 5);
    CHECK(rng.cursor == 3);

    rng.cursor = 0;
    game.units.items[0].detail_status[1] = 1;
    CHECK(fd2_field_magic_apply_known_effect(
        27, 0, &game.units, next_roll, &rng) == 0);
    CHECK(game.units.items[0].hp == 41);
    CHECK(game.units.items[0].detail_status[1] == 1);
    CHECK(rng.cursor == 0);

    rng.cursor = 0;
    game.units.items[0].detail_status[1] = 0;
    game.units.items[0].movement_profile = 26;
    CHECK(fd2_field_magic_apply_known_effect(
        27, 0, &game.units, next_roll, &rng) == 0);
    CHECK(game.units.items[0].detail_status[1] == 0);
    CHECK(rng.cursor == 0);

    static const uint32_t miss_rolls[] = {50};
    rng = (test_rng){miss_rolls, 1, 0};
    game.units.items[0].movement_profile = 1;
    CHECK(fd2_field_magic_apply_known_effect(
        27, 0, &game.units, next_roll, &rng) == 0);
    CHECK(game.units.items[0].detail_status[1] == 0);
    CHECK(rng.cursor == 1);
    return 0;
}

static int test_item_magic_commit(void) {
    static const uint32_t rolls[] = {0, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[1].hp = game.units.items[1].hp_max = 300;
    game.units.items[1].x = 2;
    game.units.items[1].y = 1;
    uint8_t *record = (uint8_t *)&game.units.items[0];
    memset(record + 0x0a, 0x80, 0x10);
    for (size_t slot = 0; slot < 8; slot++)
        record[0x0bu + slot * 2u] = 0xff;
    record[0x0a] = 0;
    record[0x0b] = 11; /* code 20，value 2（magic ID 2）。 */
    game.units.items[0].side = 1;
    game.active_side = 1;

    fd2_field_ai_item_candidate plan;
    CHECK(fd2_field_ai_choose_item_candidate(
        &game.map, &game.terrain, &game.units, 0, 0, &plan) == 0);
    CHECK(plan.item_id == 11 && plan.score >= 6);
    CHECK(fd2_field_game_commit_ai_item(&game, 0, 0, &plan) == 0);
    CHECK(game.units.items[1].hp == 75);
    CHECK(record[0x0b] == 11); /* code 20 不走 code 5 的消耗路径。 */
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.cursor == 2);
    return 0;
}

static int test_item_magic_wrapper_boundaries(void) {
    static const uint32_t rolls[] = {0, 0};
    test_rng rng = {rolls, 2, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].side = 1;
    game.active_side = 1;
    game.units.items[1].x = 2;
    game.units.items[1].y = 1;
    game.units.items[1].hp = game.units.items[1].hp_max = 1000;
    uint8_t *record = (uint8_t *)&game.units.items[0];
    memset(record + 0x0a, 0x80, 0x10);
    for (size_t slot = 0; slot < 8; slot++)
        record[0x0bu + slot * 2u] = 0xff;

    /* item 38：code 21、value 1。0x2111a wrapper 不进入 code 5 的
     * inventory remove；效果仍按 magic ID 1 使用两次 RNG。 */
    record[0x0a] = 0;
    record[0x0b] = 38;
    fd2_field_ai_item_candidate plan;
    CHECK(fd2_field_ai_choose_item_candidate(
        &game.map, &game.terrain, &game.units, 0, 0, &plan) == 0);
    CHECK(plan.item_id == 38 && plan.score >= 6);
    CHECK(fd2_field_game_commit_ai_item(&game, 0, 0, &plan) == 0);
    CHECK(game.units.items[1].hp == 892);
    CHECK(record[0x0b] == 38);
    CHECK(rng.cursor == 2);

    /* item 79：code 24、value 3，共用 code 20 的 0x20f6d wrapper。
     * 该轴线物品同样保留原 slot。 */
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].side = 1;
    game.active_side = 1;
    game.units.items[1].x = 5;
    game.units.items[1].y = 1;
    game.units.items[1].hp = game.units.items[1].hp_max = 1000;
    record = (uint8_t *)&game.units.items[0];
    memset(record + 0x0a, 0x80, 0x10);
    for (size_t slot = 0; slot < 8; slot++)
        record[0x0bu + slot * 2u] = 0xff;
    record[0x0a] = 0;
    record[0x0b] = 79;
    rng.cursor = 0;
    CHECK(fd2_field_ai_choose_item_candidate(
        &game.map, &game.terrain, &game.units, 0, 0, &plan) == 0);
    CHECK(plan.item_id == 79 && plan.score >= 6);
    CHECK(fd2_field_game_commit_ai_item(&game, 0, 0, &plan) == 0);
    CHECK(game.units.items[1].hp == 550);
    CHECK(record[0x0b] == 79);
    CHECK(rng.cursor == 2);
    return 0;
}

static int test_item_heal_boundaries(void) {
    static const uint32_t rolls[] = {99, 50};
    test_rng rng = {rolls, 2, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].side = 1;
    game.active_side = 1;
    game.units.items[0].hp = game.units.items[0].hp_max = 100;
    game.units.items[1].side = 1;
    game.units.items[1].hp = 10;
    game.units.items[1].hp_max = 100;
    /* 同格只用于锁定逐目标 core：任一有效中心都同时收集两条 raw
     * actor 记录，避免候选器为同分选择只覆盖受伤目标的中心。 */
    game.units.items[1].x = 1;
    game.units.items[1].y = 1;
    uint8_t *record = (uint8_t *)&game.units.items[0];
    memset(record + 0x0a, 0x80, 0x10);
    for (size_t slot = 0; slot < 8; slot++)
        record[0x0bu + slot * 2u] = 0xff;
    record[0x0a] = 0x40;
    record[0x0b] = 40; /* code 13、value 100、范围可覆盖两名友军。 */

    fd2_field_ai_item_candidate plan;
    CHECK(fd2_field_ai_choose_item_candidate(
        &game.map, &game.terrain, &game.units, 0, 1, &plan) == 0);
    CHECK(plan.item_id == 40 && plan.score >= 6);
    CHECK(fd2_field_game_commit_ai_item(&game, 0, 1, &plan) == 0);
    /* target list 按 raw actor index：满 HP actor 0 仍先消费 RNG；actor 1
     * 再消费下一值并按 90%+5% 恢复到 cap。 */
    CHECK(game.units.items[0].hp == 100);
    CHECK(game.units.items[1].hp == 100);
    CHECK(rng.cursor == 2);
    CHECK(record[0x0a] == 0x40 && record[0x0b] == 40);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    return 0;
}

static int test_item_commit(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].side = 1;
    game.active_side = 1;
    game.units.items[0].hp = 10;
    game.units.items[0].hp_max = 100;
    uint8_t *record = (uint8_t *)&game.units.items[0];
    memset(record + 0x0a, 0x80, 0x10);
    for (size_t slot = 0; slot < 8; slot++)
        record[0x0bu + slot * 2u] = 0xff;
    /* 把 code 5 放在中间槽；remove 必须压紧其后的完整 flag/item 对，
     * 并只把最后槽写成 0x80/0xff。 */
    record[0x0a] = 0x40;
    record[0x0b] = 11;  /* code 20，本夹具评分为 0，不应被移除。 */
    record[0x0c] = 0;
    record[0x0d] = 192; /* code 5，value 40。 */
    record[0x0e] = 0x20;
    record[0x0f] = 38;  /* 后继槽，验证双字节压紧。 */
    record[0x19] = 77;  /* 末槽隐藏 ID 是 stale 数据，remove 不应清除。 */

    fd2_field_ai_item_candidate plan;
    CHECK(fd2_field_ai_choose_item_candidate(
        &game.map, &game.terrain, &game.units, 0, 1, &plan) == 0);
    CHECK(plan.item_id == 192 && plan.slot_index == 1 && plan.score >= 6);
    CHECK(fd2_field_game_commit_ai_item(&game, 0, 1, &plan) == 0);
    CHECK(game.units.items[0].hp == 46);
    CHECK(record[0x0a] == 0x40 && record[0x0b] == 11);
    CHECK(record[0x0c] == 0x20 && record[0x0d] == 38);
    /* 原 helper 只隐藏末槽，不清 stale item ID。 */
    CHECK(record[0x18] == 0x80 && record[0x19] == 77);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.cursor == 1);
    return 0;
}

static int test_behavior5_cell_action_commit(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 5;
    game.units.items[0].data_3d = 1;
    game.units.items[0].movement_points = 2;
    game.units.items[1].x = 7;
    game.units.items[1].y = 7;
    cells[1u * 8u + 2u] = 1u << 16;
    attrs[0].flags = 0x20;
    game.metadata.cell_actions[1].mode = 0;
    game.metadata.cell_actions[1].param = 192;
    uint8_t *record = (uint8_t *)&game.units.items[0];
    memset(record + 0x0a, 0x80, 0x10);
    for (size_t slot = 0; slot < 8; slot++)
        record[0x0bu + slot * 2u] = 0xff;

    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    CHECK(game.units.items[0].x == 2 && game.units.items[0].y == 1);
    CHECK(record[0x0a] == 0 && record[0x0b] == 192);
    CHECK(record[0x31] == 0 && record[0x32] == 192 && record[0x33] == 0);
    CHECK((game.units.items[0].ai_behavior_raw & 0x0fu) == 7u);
    CHECK(game.cell_action_completed[1] == 1);
    CHECK(game.active_side == 2);
    /* actor 0 completes the side 0 phase; the following turn boundary clears
     * acted for the complete table. */
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.cursor == 0);

    /* 0x13cf8 忽略 inventory add 的返回值；满库存不能阻止完成位和
     * behavior 7 提交。 */
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 5;
    game.units.items[0].data_3d = 1;
    game.units.items[0].movement_points = 2;
    game.units.items[1].x = 7;
    game.units.items[1].y = 7;
    memset(cells, 0, sizeof(cells));
    cells[1u * 8u + 2u] = 1u << 16;
    attrs[0].flags = 0x20;
    game.metadata.cell_actions[1].mode = 0;
    game.metadata.cell_actions[1].param = 192;
    record = (uint8_t *)&game.units.items[0];
    for (size_t slot = 0; slot < 8; slot++) {
        record[0x0au + slot * 2u] = 0;
        record[0x0bu + slot * 2u] = (uint8_t)slot;
    }
    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    CHECK(game.cell_action_completed[1] == 1);
    CHECK((game.units.items[0].ai_behavior_raw & 0x0fu) == 7u);
    for (size_t slot = 0; slot < 8; slot++)
        CHECK(record[0x0bu + slot * 2u] == (uint8_t)slot);

    /* 完成位已设置时，原版回跳 behavior 0 的攻击／移动 fallback；不能
     * 再次提交 cell action。 */
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 5;
    game.units.items[0].data_3d = 3;
    game.units.items[0].movement_points = 2;
    game.cell_action_completed[3] = 1;
    memset(cells, 0, sizeof(cells));
    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    CHECK((game.units.items[0].ai_behavior_raw & 0x0fu) == 5u);
    CHECK(game.cell_action_completed[3] == 1);
    CHECK(game.active_side == 2);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    return 0;
}

static int test_behavior2_recover_after_queries(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 2;
    game.units.items[0].hp = 40;
    game.units.items[0].hp_max = 100;
    uint8_t *record = (uint8_t *)&game.units.items[0];
    memset(record + 0x0a, 0x80, 0x10);
    for (size_t slot = 0; slot < 8; slot++)
        record[0x0bu + slot * 2u] = 0xff;

    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    CHECK(game.units.items[0].hp == 60);
    CHECK(game.active_side == 2);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_behavior11_magic_then_physical(void) {
    static const uint32_t rolls[] = {0, 0, 50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 11;
    game.units.items[0].mp = game.units.items[0].mp_max = 10;
    teach_magic(&game.units.items[0], 0);
    game.units.items[1].hp = game.units.items[1].hp_max = 300;

    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    /* 正确 dual 0x13e07..0x13e43：magic 后仍重算并执行 physical。 */
    CHECK(rng.cursor == 6);
    CHECK(game.units.items[1].hp < 255);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.active_side == 2);
    return 0;
}

static int test_behavior11_magic_survives_physical_preflight_failure(void) {
    static const uint32_t rolls[] = {0, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 11;
    game.units.items[0].mp = game.units.items[0].mp_max = 10;
    teach_magic(&game.units.items[0], 0);
    game.units.items[1].hp = game.units.items[1].hp_max = 300;
    game.units.items[0].movement_profile = 28; /* no magic profile: physical candidate is absent. */

    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    CHECK(game.units.items[1].hp == 255); /* magic damage remains committed. */
    CHECK(rng.cursor == 2);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.active_side == 2);
    return 0;
}

static int test_behavior7_hide_on_arrival(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 7;
    game.units.items[0].ai_param_35 = game.units.items[0].x;
    game.units.items[0].ai_param_36 = game.units.items[0].y;

    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    /* behavior 7 的 0x32975 先精确写 flags=1，common tail 随后追加 acted。 */
    CHECK((game.units.items[0].flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.active_side == 0);
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_phase_status_tick(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.stage = 1;
    game.active_side = 2;

    /* enter_phase(1) must not touch the side 0 actor. */
    game.units.items[0].hp_max = 50;
    game.units.items[0].hp = 50;
    game.units.items[0].detail_status[0] = 1;
    game.units.items[0].attack_status = 2;
    CHECK(fd2_field_game_end_active_phase(&game) == 0);
    CHECK(game.active_side == 1);
    /* Both loops start at raw actor index 0 but still filter by side, so the
     * side 0 actor remains untouched while entering side 1. */
    CHECK(game.units.items[0].hp == 50);
    CHECK(game.units.items[0].detail_status[0] == 1);
    CHECK(game.units.items[0].attack_status == 2);

    /* The side 1 actor receives one poison tick and all six timers decrement
     * once. The test RNG must remain untouched by this phase prelude. */
    game.units.items[1].hp_max = 50;
    game.units.items[1].hp = 50;
    game.units.items[1].detail_status[0] = 1;
    game.units.items[1].attack_status = 2;
    game.units.items[1].defense_status = 1;
    game.units.items[1].hit_evasion_status = 3;
    game.units.items[1].detail_status[1] = 2;
    game.units.items[1].detail_status[2] = 1;
    game.units.items[1].base_attack_le[0] = 100;
    game.units.items[1].base_attack_le[1] = 0;
    game.units.items[1].base_defense_le[0] = 80;
    game.units.items[1].base_defense_le[1] = 0;
    game.units.items[1].dexterity_le[0] = 40;
    game.units.items[1].dexterity_le[1] = 0;
    game.units.items[1].attack = 115;
    game.units.items[1].defense = 92;
    game.units.items[1].accuracy = 55;
    game.units.items[1].evasion = 55;
    game.active_side = 0;
    CHECK(fd2_field_game_end_active_phase(&game) == 0);
    CHECK(game.active_side == 2);
    CHECK(game.units.items[1].hp == 45);
    CHECK(game.units.items[1].detail_status[0] == 0);
    CHECK(game.units.items[1].attack_status == 1);
    CHECK(game.units.items[1].defense_status == 0);
    CHECK(game.units.items[1].hit_evasion_status == 2);
    CHECK(game.units.items[1].detail_status[1] == 1);
    CHECK(game.units.items[1].detail_status[2] == 0);
    /* defense_status expired and therefore loses its 1.15 multiplier;
     * attack/hit-evasion remain active. */
    CHECK(game.units.items[1].attack == 115);
    CHECK(game.units.items[1].defense == 80);
    CHECK(game.units.items[1].accuracy == 55);
    CHECK(game.units.items[1].evasion == 55);
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_ai_flag_gate(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].flags = FD2_FIELD_UNIT_FLAG_AI_INELIGIBLE;
    game.units.items[1].side = 2;
    game.active_side = 0;
    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    CHECK(game.active_side == 2);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_behavior8_outer_loop_boundary(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;

    /* side 1：behavior 8 从 0x13d9a 直接返回 AI caller，不执行
     * common tail。因此所在格即使匹配 match 1，也不能产生 cell event，
     * 不能置 acted，也不能调用全表 direction reset。outer loop 仍在下次
     * scheduler step 越过其余 raw actors 并推进到 side 0。 */
    init_game(&game, cells, attrs, &rng);
    game.stage = 1;
    game.active_side = 1;
    game.units.count = 3;
    set_unit(&game.units.items[0], 1, 1, 1);
    game.units.items[0].ai_behavior_raw = 8;
    game.units.items[0].direction = 3;
    set_unit(&game.units.items[1], 0, 6, 6);
    game.units.items[1].direction = 2;
    set_unit(&game.units.items[2], 2, 7, 7);
    cells[1u * 8u + 1u] = 1u << 16;
    game.metadata.cell_lookup[0] = (fd2_field_cell_lookup){9, 1};
    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    CHECK(game.active_side == 1);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.units.items[0].direction == 3);
    CHECK(game.units.items[1].direction == 2);
    CHECK(game.event_log_count == 0);
    CHECK(game.unhandled_event_count == 0);
    CHECK(rng.cursor == 0);
    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    CHECK(game.active_side == 0);

    /* side 0：第一轮候选不足后立即进入第二轮；behavior 8 仍不 acted。
     * phase cursor 保证下一次调用结束本轮，不能因 remaining_units 仍含
     * 该 actor 而重复执行或永久停留。 */
    init_game(&game, cells, attrs, &rng);
    game.stage = 1;
    game.units.items[0].ai_behavior_raw = 8;
    game.units.items[0].direction = 3;
    game.units.items[1].direction = 2;
    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    CHECK(game.active_side == 0);
    CHECK(game.phase_ai_pass == 1);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.units.items[0].direction == 3);
    CHECK(game.units.items[1].direction == 2);
    CHECK(rng.cursor == 0);
    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    CHECK(game.active_side == 2);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_phase_acted_lifecycle(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.stage = 1;
    game.active_side = 2;
    game.units.items[0].side = 2;
    game.units.items[1].side = 1;
    fd2_field_unit_set_acted(&game.units.items[0], 1);
    fd2_field_unit_set_acted(&game.units.items[1], 1);

    /* side 2→1 has no clear-all call in field_turn_cycle_run. */
    CHECK(fd2_field_game_end_active_phase(&game) == 0);
    CHECK(game.active_side == 1);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(fd2_field_unit_has_acted(&game.units.items[1]));

    /* side 1→0 clear-all is delayed until the first side 0 scheduler step,
     * because SDL advances one actor at a time. */
    CHECK(fd2_field_game_end_active_phase(&game) == 0);
    CHECK(game.active_side == 0);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(fd2_field_unit_has_acted(&game.units.items[1]));
    game.units.items[0].side = 0;
    game.units.items[1].side = 2;
    game.units.items[0].ai_behavior_raw = 8; /* confirmed early return. */
    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(!fd2_field_unit_has_acted(&game.units.items[1]));

    fd2_field_unit_set_acted(&game.units.items[0], 1);
    fd2_field_unit_set_acted(&game.units.items[1], 1);
    CHECK(fd2_field_game_end_active_phase(&game) == 0);
    CHECK(game.active_side == 2);
    CHECK(game.turn_number == 1);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(!fd2_field_unit_has_acted(&game.units.items[1]));
    return 0;
}

static int test_stage0_handler_gate_preempts_ai(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.stage = 0;
    game.active_side = 0;
    /* stage 0 actor 0 是主角；保留其可见，隐藏唯一 side 0 actor。
     * DS:0x1b19[0] 应在任何 AI 或 phase 推进前返回胜利。 */
    game.units.items[0].side = 2;
    game.units.items[0].flags = 0;
    game.units.items[1].side = 0;
    game.units.items[1].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    CHECK(fd2_field_game_process_automatic_action(&game) == 0);
    CHECK(game.battle_result == FD2_FIELD_BATTLE_VICTORY);
    CHECK(game.active_side == 0);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(!fd2_field_unit_has_acted(&game.units.items[1]));
    CHECK(rng.cursor == 0);

    /* 主角隐藏优先覆盖为失败，即使 side 0 也已经全灭。 */
    game.units.items[0].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    CHECK(fd2_field_game_process_automatic_action(&game) == 0);
    CHECK(game.battle_result == FD2_FIELD_BATTLE_DEFEAT);
    CHECK(game.active_side == 0);
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_battle_result_query(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.stage = 1; /* 先覆盖非 stage 0 的通用 side 存活查询。 */

    /* init_game 的 actor 0 为 side 0，actor 1 为 side 2。 */
    CHECK(fd2_field_game_battle_result(&game) ==
          FD2_FIELD_BATTLE_ONGOING);
    /* DS:0x1b19[0] / dual 0x205b4：stage 0 在无可见 side 0 actor
     * 时结束，不等待尚未加载的增援 group。 */
    game.units.items[0].hp = 0;
    game.units.items[0].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    CHECK(fd2_field_game_battle_result(&game) ==
          FD2_FIELD_BATTLE_VICTORY);

    init_game(&game, cells, attrs, &rng);
    game.stage = 0;
    game.units.items[0].side = 2; /* actor 0 是 stage 0 主角。 */
    game.units.items[1].side = 0;
    game.units.items[0].hp = 0;
    game.units.items[0].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    CHECK(fd2_field_game_battle_result(&game) ==
          FD2_FIELD_BATTLE_DEFEAT);
    return 0;
}

static int test_ai_cell_event_common_tail_only(void) {
    static const uint32_t rolls[] = {50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.stage = 1;
    game.units.items[1].x = 4;
    game.units.items[1].y = 1;
    /* 两步 AI 路径：中间格只匹配玩家逐步查询参数 0，最终格匹配
     * AI common tail 参数 1。原版 AI 不沿途触发前者。 */
    cells[1u * 8u + 2u] = 1u << 16;
    cells[1u * 8u + 3u] = 2u << 16;
    game.metadata.cell_lookup[0] = (fd2_field_cell_lookup){7, 0};
    game.metadata.cell_lookup[1] = (fd2_field_cell_lookup){9, 1};

    fd2_field_ai_physical_candidate plan;
    CHECK(build_plan(&game, &plan) == 0);
    CHECK(plan.destination_x == 3 && plan.destination_y == 1);
    CHECK(fd2_field_game_commit_ai_physical(
              &game, 0, 0, &plan, NULL, 0) == 0);
    CHECK(game.event_log_count == 1);
    CHECK(game.event_total_count == 1);
    CHECK(game.unhandled_event_count == 1);
    CHECK(game.event_log[0].kind == FD2_FIELD_EVENT_CELL);
    CHECK(game.event_log[0].action == 9);
    CHECK(game.event_log[0].unit_index == 0);
    CHECK(game.event_log[0].x == 3 && game.event_log[0].y == 1);
    CHECK(game.event_log[0].slot == 1);

    /* 正式自动 scheduler 的 outer side loop 应在发现尚无 handler 的
     * notice 后显式停机；不能继续执行下一个 actor。直接 commit API
     * 只负责事务本身，因此上面的返回值仍为成功。 */
    init_game(&game, cells, attrs, &rng);
    game.stage = 1;
    game.units.items[1].x = 7;
    game.units.items[1].y = 7;
    game.units.items[0].ai_behavior_raw = 8;
    cells[1u * 8u + 1u] = 1u << 16;
    game.metadata.cell_lookup[0] = (fd2_field_cell_lookup){9, 1};
    game.units.items[0].ai_behavior_raw = 2;
    game.units.items[0].hp = game.units.items[0].hp_max;
    game.units.items[0].movement_points = 0;
    CHECK(fd2_field_game_process_automatic_action(&game) == -1);
    CHECK(game.event_log_count == 1);
    CHECK(game.unhandled_event_count == 1);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.active_side == 0);
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
    if (test_commit() != 0 ||
        test_ai_cell_event_common_tail_only() != 0 ||
        test_common_tail_resets_all_directions() != 0 ||
        test_automatic_mode0() != 0 ||
        test_magic_commit() != 0 ||
        test_magic_damage_dispatch_boundaries() != 0 ||
        test_magic_heal_boundaries() != 0 ||
        test_unknown_magic_rejected() != 0 ||
        test_magic_status_commit() != 0 ||
        test_item_magic_commit() != 0 ||
        test_item_magic_wrapper_boundaries() != 0 ||
        test_item_heal_boundaries() != 0 ||
        test_item_commit() != 0 ||
        test_behavior5_cell_action_commit() != 0 ||
        test_behavior2_recover_after_queries() != 0 ||
        test_behavior11_magic_then_physical() != 0 ||
        test_behavior11_magic_survives_physical_preflight_failure() != 0 ||
        test_behavior7_hide_on_arrival() != 0 ||
        test_phase_status_tick() != 0 || test_ai_flag_gate() != 0 ||
        test_behavior8_outer_loop_boundary() != 0 ||
        test_phase_acted_lifecycle() != 0 ||
        test_stage0_handler_gate_preempts_ai() != 0 ||
        test_battle_result_query() != 0 || test_stale_plan() != 0) return 1;
    puts("field_ai_commit_test: ok");
    return 0;
}
