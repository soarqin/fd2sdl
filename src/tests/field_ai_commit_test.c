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
    /* The target is finalized hidden; remaining-unit accounting skips it and
     * the scheduler may advance straight through the empty side-1 phase. */
    CHECK(game.active_side == 2);
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
    game.units.items[0].flags &= (uint8_t)~FD2_FIELD_UNIT_FLAG_ACTED;
    CHECK(fd2_field_magic_apply_known_effect(
        25, 0, &game.units, next_roll, &rng) == 0);

    static const uint32_t status_rolls[] = {0, 0, 0};
    rng = (test_rng){status_rolls, 3, 0};
    game.units.items[0].movement_profile = 1;
    game.units.items[0].hp = 50;
    CHECK(fd2_field_magic_apply_known_effect(
        22, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].hp == 41);
    CHECK(game.units.items[0].detail_status[2] == 2);
    CHECK(rng.cursor == 3);
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
    record[0x0a] = 0;
    record[0x0b] = 192; /* code 5，value 40。 */

    fd2_field_ai_item_candidate plan;
    CHECK(fd2_field_ai_choose_item_candidate(
        &game.map, &game.terrain, &game.units, 0, 1, &plan) == 0);
    CHECK(plan.item_id == 192 && plan.score >= 6);
    CHECK(fd2_field_game_commit_ai_item(&game, 0, 1, &plan) == 0);
    CHECK(game.units.items[0].hp == 46);
    CHECK(record[0x0a] == 0x80 && record[0x0b] == 0xff);
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
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
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
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
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
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
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
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
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
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
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
    CHECK(game.active_side == 2);
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

    /* init_game 的 actor 0 为 side 0，actor 1 为 side 2。 */
    CHECK(fd2_field_game_battle_result(&game) ==
          FD2_FIELD_BATTLE_ONGOING);
    game.units.items[0].hp = 0;
    game.units.items[0].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    CHECK(fd2_field_game_battle_result(&game) ==
          FD2_FIELD_BATTLE_ONGOING); /* stage 0 尚有第 4/5 回合增援。 */
    fd2_field_unit_template templates[2] = {{{0}}, {{0}}};
    game.units.count = 0;
    game.metadata.unit_template_count = 2;
    game.metadata.unit_templates = templates;
    game.metadata.unit_templates[0].bytes[0x15] = 4;
    game.metadata.unit_templates[1].bytes[0x15] = 5;
    set_unit(&game.units.items[0], 2, 3, 1);
    game.units.items[0].side = 2;
    game.units.source_template_indices[0] = FD2_FIELD_UNIT_NO_TEMPLATE;
    set_unit(&game.units.items[1], 0, 0, 0);
    game.units.items[1].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    game.units.source_template_indices[1] = 0;
    set_unit(&game.units.items[2], 0, 0, 0);
    game.units.items[2].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    game.units.source_template_indices[2] = 1;
    game.units.count = 3;
    CHECK(fd2_field_game_battle_result(&game) ==
          FD2_FIELD_BATTLE_VICTORY);
    game.units.items[0].hp = 0;
    game.units.items[0].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    game.units.items[1].hp = 50;
    game.units.items[1].flags = 0;
    CHECK(fd2_field_game_battle_result(&game) ==
          FD2_FIELD_BATTLE_DEFEAT);
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
        test_magic_commit() != 0 || test_magic_status_commit() != 0 ||
        test_item_magic_commit() != 0 || test_item_commit() != 0 ||
        test_behavior5_cell_action_commit() != 0 ||
        test_behavior2_recover_after_queries() != 0 ||
        test_behavior11_magic_then_physical() != 0 ||
        test_behavior11_magic_survives_physical_preflight_failure() != 0 ||
        test_behavior7_hide_on_arrival() != 0 ||
        test_battle_result_query() != 0 || test_stale_plan() != 0) return 1;
    puts("field_ai_commit_test: ok");
    return 0;
}
