#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "field_ai.h"
#include "field_magic.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static void set_unit(fd2_field_units *units, size_t index,
                     uint8_t side, uint8_t x, uint8_t y) {
    fd2_field_unit *unit = &units->items[index];
    memset(unit, 0, sizeof(*unit));
    unit->side = side;
    unit->x = x;
    unit->y = y;
    if (units->count <= index) units->count = index + 1u;
}

static int test_behavior_and_nearest_target(void) {
    fd2_field_unit unit = {0};
    unit.ai_behavior_raw = 0xabu;
    CHECK(fd2_field_ai_behavior(&unit) == 0x0bu);
    CHECK(fd2_field_ai_behavior(NULL) == 0xffu);

    fd2_field_units units = {0};
    set_unit(&units, 0, 0, 5, 5);
    set_unit(&units, 1, 2, 3, 5);
    set_unit(&units, 2, 1, 7, 5);
    set_unit(&units, 3, 0, 5, 4);
    /* 原函数不检查目标状态；最近的隐藏／HP 0 项仍按 actor 顺序命中。 */
    units.items[1].flags = FD2_FIELD_UNIT_FLAG_HIDDEN |
                           FD2_FIELD_UNIT_FLAG_ACTED;
    units.items[1].hp = 0;

    fd2_field_ai_target target;
    fd2_field_units units_before = units;
    CHECK(fd2_field_ai_nearest_opponent(&units, 0, 0, &target) == 0);
    CHECK(memcmp(&units, &units_before, sizeof(units)) == 0);
    CHECK(target.unit_index == 1);
    CHECK(target.x == 3 && target.y == 5);
    CHECK(target.manhattan_distance == 2);

    set_unit(&units, 4, 1, 0, 0);
    set_unit(&units, 5, 0, 1, 0);
    CHECK(fd2_field_ai_nearest_opponent(&units, 4, 1, &target) == 0);
    CHECK(target.unit_index == 5 && target.manhattan_distance == 1);

    fd2_field_units no_target = {0};
    set_unit(&no_target, 0, 0, 1, 1);
    set_unit(&no_target, 1, 0, 2, 2);
    CHECK(fd2_field_ai_nearest_opponent(
              &no_target, 0, 0, &target) == -1);
    /* helper 不校验 actor.side；非 0 selector 会把 actor 自身视为
     * 首个距离 0 的 side 0 候选。这一行为只用于锁定机器码语义。 */
    CHECK(fd2_field_ai_nearest_opponent(&units, 0, 1, &target) == 0);
    CHECK(target.unit_index == 0 && target.manhattan_distance == 0);
    CHECK(fd2_field_ai_nearest_opponent(&units, 0, 2, &target) == 0);
    CHECK(target.unit_index == 0);
    CHECK(fd2_field_ai_nearest_opponent(
              &units, units.count, 0, &target) == -1);
    return 0;
}

static void init_range(fd2_field_path_result *range,
                       fd2_field_path_node *nodes,
                       int width, int height) {
    memset(nodes, 0, (size_t)width * (size_t)height * sizeof(*nodes));
    for (int i = 0; i < width * height; i++)
        nodes[i].distance = FD2_FIELD_PATH_UNREACHABLE;
    *range = (fd2_field_path_result){
        .width = width,
        .height = height,
        .start_x = 2,
        .start_y = 2,
        .budget = 9,
        .nodes = nodes,
    };
}

static void add_destination(fd2_field_path_result *range,
                            int x, int y, uint32_t cost, int can_stop) {
    fd2_field_path_node *node =
        &range->nodes[(size_t)y * (size_t)range->width + (size_t)x];
    node->distance = cost;
    node->can_stop = can_stop ? 1u : 0u;
}

static int test_destination_order(void) {
    fd2_field_path_node nodes[25];
    fd2_field_path_result range;
    init_range(&range, nodes, 5, 5);

    /* 四项到 anchor (2,2) 都是距离 2。机器码的非对称 tie score
     * 令右上 (3,1) 的 0 胜过左上 (1,1) 的 2；右下与其完全平价，
     * 因行优先稳定扫描不会覆盖右上。 */
    add_destination(&range, 2, 0, 8, 1);
    add_destination(&range, 1, 1, 7, 1);
    add_destination(&range, 3, 1, 6, 1);
    add_destination(&range, 3, 3, 5, 1);
    fd2_field_ai_destination destination;
    fd2_field_path_node nodes_before[25];
    memcpy(nodes_before, nodes, sizeof(nodes));
    CHECK(fd2_field_ai_choose_destination(
              &range, 2, 2, &destination) == 0);
    CHECK(memcmp(nodes, nodes_before, sizeof(nodes)) == 0);
    CHECK(destination.x == 3 && destination.y == 1);
    CHECK(destination.manhattan_distance == 2);
    CHECK(destination.tie_score == 0);
    CHECK(destination.path_cost == 6);

    init_range(&range, nodes, 5, 5);
    /* closer but can_stop=0 的格必须忽略；其余完全平价时保留行优先项。 */
    add_destination(&range, 2, 2, 0, 0);
    add_destination(&range, 2, 1, 4, 1);
    add_destination(&range, 1, 2, 3, 1);
    CHECK(fd2_field_ai_choose_destination(
              &range, 2, 2, &destination) == 0);
    CHECK(destination.x == 2 && destination.y == 1);
    CHECK(destination.path_cost == 4);

    init_range(&range, nodes, 5, 5);
    CHECK(fd2_field_ai_choose_destination(
              &range, 2, 2, &destination) == -1);
    CHECK(fd2_field_ai_choose_destination(
              &range, 5, 2, &destination) == -1);
    return 0;
}

static void init_flat_map(fd2_field_map *map,
                          fd2_terrain_tileset *terrain,
                          uint32_t *cells,
                          fd2_terrain_attr *attrs,
                          int width, int height) {
    memset(cells, 0, (size_t)width * (size_t)height * sizeof(*cells));
    memset(attrs, 0, sizeof(*attrs));
    attrs[0].movement_cost_class = 1; /* 攻防地形百分比均为 0。 */
    *map = (fd2_field_map){
        .width = width,
        .height = height,
        .cells = cells,
    };
    *terrain = (fd2_terrain_tileset){
        .attrs = attrs,
        .attr_count = 1,
    };
}

static void equip_weapon(fd2_field_unit *unit, uint8_t item_id) {
    uint8_t *record = (uint8_t *)unit;
    record[0x0a] = 0x40u;
    record[0x0b] = item_id;
}

static int test_magic_and_item_scoring(void) {
    fd2_field_units units = {0};
    set_unit(&units, 0, 0, 0, 0);
    set_unit(&units, 1, 0, 0, 0);
    uint8_t ids[] = {0, 1};
    fd2_field_ai_score_targets targets = {&units, ids, 2};
    int32_t score = -1;

    units.items[0].hp = 60;
    units.items[1].hp = 40;
    units.items[1].text_id = 1;
    CHECK(fd2_field_ai_magic_targets_score(0, &targets, &score) == 0);
    /* magic 0 的 record +0 为 50：首项 HP>=50 得 8 且 text_id 0
     * 乘 1.5 为 12；次项 HP<50 得 24。 */
    CHECK(score == 36);

    units.items[0].hp_max = units.items[1].hp_max = 90;
    units.items[0].hp = 29; /* <1/3：8；behavior bit 0 再翻倍。 */
    units.items[0].ai_behavior_raw = 1;
    units.items[1].hp = 44; /* >=1/3 且 <1/2：3。 */
    CHECK(fd2_field_ai_magic_targets_score(13, &targets, &score) == 0);
    CHECK(score == 19);
    units.items[0].hp = 30; /* 等于 1/3 时进入 3 分区间并翻倍。 */
    units.items[1].hp = 45; /* 等于 1/2 时为 0。 */
    CHECK(fd2_field_ai_magic_targets_score(13, &targets, &score) == 0);
    CHECK(score == 6);

    units.items[0].attack_status = 0;
    units.items[1].attack_status = 1;
    CHECK(fd2_field_ai_magic_targets_score(17, &targets, &score) == 0);
    CHECK(score == 3);
    units.items[0].detail_status[0] = 1;
    units.items[1].detail_status[0] = 0;
    CHECK(fd2_field_ai_magic_targets_score(20, &targets, &score) == 0);
    CHECK(score == 6);
    CHECK(fd2_field_ai_magic_targets_score(26, &targets, &score) == 0);
    CHECK(score == 4);

    /* magic 22：+0x27 必须为 0，且 +0x1a..+0x1e 任一 bit 非 0。 */
    units.items[0].data_09_1e[0x11] = 1;
    units.items[1].data_09_1e[0x11] = 1;
    units.items[1].detail_status[2] = 1;
    CHECK(fd2_field_ai_magic_targets_score(22, &targets, &score) == 0);
    CHECK(score == 6);
    CHECK(fd2_field_ai_magic_targets_score(23, &targets, &score) == 0);
    CHECK(score == 0);

    /* item code 5：入口 value 会在循环内被目标当前 HP 覆盖。
     * 当前 HP <= 最大 HP/3 取 8；中间区间取 3；+0x34 bit7 三倍。 */
    units.items[0].hp = 30;
    units.items[0].hp_max = 90;
    units.items[1].hp = 60;
    units.items[1].hp_max = 150;
    units.items[0].ai_behavior_raw = 0x80;
    units.items[1].ai_behavior_raw = 0;
    CHECK(fd2_field_ai_item_targets_score(192, &targets, &score) == 0);
    CHECK(score == 27);

    /* item 11: dispatch 20、raw value 2，先取 magic record 2 的
     * u16 +0 == 250；只给 HP 严格大于 250 的目标 +8。 */
    units.items[0].hp = 250;
    units.items[1].hp = 251;
    CHECK(fd2_field_ai_item_targets_score(11, &targets, &score) == 0);
    CHECK(score == 8);
    CHECK(fd2_field_ai_item_targets_score(0, &targets, &score) == 0);
    CHECK(score == 0);

    uint32_t cells[25];
    fd2_terrain_attr attrs[1];
    fd2_field_map map;
    fd2_terrain_tileset terrain;
    init_flat_map(&map, &terrain, cells, attrs, 5, 5);

    /* magic 0：actor 已知 bit 0、MP 足够；selector_mode 0 且 spell +6
     * 为 0 时使用 filter 1，故选择 side 非 0 的 (2,1) 作为施法中心。 */
    fd2_field_units magic_units = {0};
    set_unit(&magic_units, 0, 0, 1, 1);
    set_unit(&magic_units, 1, 2, 2, 1);
    magic_units.items[0].data_09_1e[0x11] = 1;
    magic_units.items[0].mp = 10;
    magic_units.items[1].hp = 40;
    magic_units.items[1].text_id = 1;
    fd2_field_ai_magic_candidate magic_choice;
    fd2_field_units magic_before = magic_units;
    CHECK(fd2_field_ai_choose_magic_candidate(
              &map, &terrain, &magic_units, 0, 0, &magic_choice) == 0);
    CHECK(magic_choice.magic_id == 0);
    CHECK(magic_choice.cast_x == 2 && magic_choice.cast_y == 1);
    CHECK(magic_choice.score == 24 && magic_choice.tie_value == 50);
    CHECK(memcmp(&magic_before, &magic_units, sizeof(magic_units)) == 0);

    /* item 192：outer range 1、inner range 0、filter 0；目标格本身
     * 成为严格正分候选。其余空槽 item 0 的 dispatch code 为 0。 */
    fd2_field_units item_units = {0};
    set_unit(&item_units, 0, 2, 1, 1);
    set_unit(&item_units, 1, 0, 2, 1);
    uint8_t *actor_raw = (uint8_t *)&item_units.items[0];
    actor_raw[0x0a] = 0;
    actor_raw[0x0b] = 192;
    item_units.items[1].hp = 30;
    item_units.items[1].hp_max = 90;
    fd2_field_ai_item_candidate item_choice;
    fd2_field_units item_before = item_units;
    CHECK(fd2_field_ai_choose_item_candidate(
              &map, &terrain, &item_units, 0, 0, &item_choice) == 0);
    CHECK(item_choice.item_id == 192 && item_choice.slot_index == 0);
    CHECK(item_choice.target_x == 2 && item_choice.target_y == 1);
    CHECK(item_choice.score == 8);
    CHECK(memcmp(&item_before, &item_units, sizeof(item_units)) == 0);

    fd2_field_ai_magic_candidate magic_best = {
        .score = 8, .tie_value = 50,
    };
    fd2_field_ai_magic_candidate magic_current = {
        .score = 8, .tie_value = 50,
    };
    CHECK(!fd2_field_ai_magic_candidate_is_better(
              &magic_current, &magic_best, 1));
    magic_current.tie_value = 51;
    CHECK(fd2_field_ai_magic_candidate_is_better(
              &magic_current, &magic_best, 1));
    magic_current.score = 0;
    CHECK(!fd2_field_ai_magic_candidate_is_better(
              &magic_current, &magic_best, 0));

    fd2_field_ai_item_candidate item_best = {.score = 8};
    fd2_field_ai_item_candidate item_current = {.score = 8};
    CHECK(!fd2_field_ai_item_candidate_is_better(
              &item_current, &item_best, 1));
    item_current.score = 9;
    CHECK(fd2_field_ai_item_candidate_is_better(
              &item_current, &item_best, 1));
    item_current.score = 0;
    CHECK(!fd2_field_ai_item_candidate_is_better(
              &item_current, &item_best, 0));

    fd2_field_units before = units;
    CHECK(fd2_field_ai_magic_targets_score(13, &targets, &score) == 0);
    CHECK(fd2_field_ai_item_targets_score(192, &targets, &score) == 0);
    CHECK(memcmp(&before, &units, sizeof(units)) == 0);
    ids[1] = FD2_FIELD_MAX_UNITS;
    CHECK(fd2_field_ai_magic_targets_score(0, &targets, &score) == -1);
    CHECK(fd2_field_ai_item_targets_score(192, &targets, &score) == -1);
    ids[1] = 1;
    units.count = FD2_FIELD_MAX_UNITS + 1u;
    CHECK(fd2_field_ai_magic_targets_score(0, &targets, &score) == -1);
    CHECK(fd2_field_ai_item_targets_score(192, &targets, &score) == -1);
    CHECK(fd2_field_magic_u16(
              fd2_field_magic_record_get(0), SIZE_MAX) == 0);
    return 0;
}

static int test_physical_candidate_and_dispatch(void) {
    fd2_field_units units = {0};
    set_unit(&units, 0, 0, 1, 1);
    set_unit(&units, 1, 2, 2, 1);
    set_unit(&units, 2, 1, 1, 2);
    units.items[0].attack = 20;
    units.items[0].defense = 10;
    units.items[0].hp = units.items[0].hp_max = 30;
    units.items[0].movement_profile = 1;
    equip_weapon(&units.items[0], 0); /* item 0，射程 1。 */
    for (size_t i = 1; i < units.count; i++) {
        units.items[i].text_id = (uint8_t)i;
        units.items[i].hp = units.items[i].hp_max = 10;
        units.items[i].defense = 4;
        units.items[i].attack = 5;
        units.items[i].movement_profile = 1;
        equip_weapon(&units.items[i], 0);
    }

    uint32_t cells[16];
    fd2_terrain_attr attrs[1];
    fd2_field_map map;
    fd2_terrain_tileset terrain;
    init_flat_map(&map, &terrain, cells, attrs, 4, 4);
    fd2_field_path_node nodes[16];
    fd2_field_path_result range;
    init_range(&range, nodes, 4, 4);
    add_destination(&range, 1, 1, 0, 1);

    fd2_field_units units_before = units;
    fd2_field_path_node nodes_before[16];
    memcpy(nodes_before, nodes, sizeof(nodes));
    fd2_field_ai_physical_candidate candidate;
    CHECK(fd2_field_ai_choose_physical_candidate(
              &map, &terrain, &units, 0, 0, &range, &candidate) == 0);
    CHECK(candidate.unit_index == 2);
    CHECK(candidate.destination_x == 1 && candidate.destination_y == 1);
    /* target 0 以 score=2 经反击调整到 7；target 1 继承 score=7，
     * 再调整到 12，故同 priority 下替换为后者。 */
    CHECK(candidate.priority == 8);
    CHECK(candidate.secondary_score == 12);
    CHECK(memcmp(&units, &units_before, sizeof(units)) == 0);
    CHECK(memcmp(nodes, nodes_before, sizeof(nodes)) == 0);

    /* selector 非 0 只保留 side 0；actor 自身会成为候选，但自身格不在
     * 武器 min range 1 内，故这里无目标。 */
    CHECK(fd2_field_ai_choose_physical_candidate(
              &map, &terrain, &units, 0, 1, &range, &candidate) == -1);
    fd2_field_units invalid_units = units;
    invalid_units.count = FD2_FIELD_MAX_UNITS + 1u;
    CHECK(fd2_field_ai_choose_physical_candidate(
              &map, &terrain, &invalid_units, 0, 0,
              &range, &candidate) == -1);
    fd2_field_map oversized_map = map;
    oversized_map.width = UINT8_MAX + 1;
    CHECK(fd2_field_ai_choose_physical_candidate(
              &oversized_map, &terrain, &units, 0, 0,
              &range, &candidate) == -1);

    /* 0x29663 只在 destination 开始时载入 target_count；第一个 target
     * 的 text_id==0 会把 ESI 从 2 改为 3，第二个 target 继承 3，因
     * 3 > HP(2) 进入 priority 18。若错误地逐 target 重置为 2，则第二
     * 项只能得到 priority 8，最终仍会保留第一项。 */
    units.items[1].text_id = 0;
    units.items[1].hp = 10;
    units.items[2].text_id = 2;
    units.items[2].hp = 2;
    CHECK(fd2_field_ai_choose_physical_candidate(
              &map, &terrain, &units, 0, 0, &range, &candidate) == 0);
    CHECK(candidate.unit_index == 2);
    CHECK(candidate.priority == 0x12);
    /* target 0: 2 + (10-5) = 7，text_id 0 再变为 10；target 1 继承
     * 10，因 10 > HP(2) 翻倍为 20，再加反击调整 5。 */
    CHECK(candidate.secondary_score == 25);

    fd2_field_ai_physical_candidate best = {
        .priority = 8,
        .secondary_score = 10,
    };
    fd2_field_ai_physical_candidate lower = {
        .priority = 0x12,
        .secondary_score = -100,
    };
    CHECK(fd2_field_ai_physical_candidate_is_better(&lower, &best, 1));
    lower.priority = 8;
    lower.secondary_score = 10;
    CHECK(!fd2_field_ai_physical_candidate_is_better(&lower, &best, 1));
    lower.secondary_score = 11;
    CHECK(fd2_field_ai_physical_candidate_is_better(&lower, &best, 1));

    CHECK(fd2_field_ai_select_attack_action(5, 5, 5, 0) ==
          FD2_FIELD_AI_ACTION_NONE);
    CHECK(fd2_field_ai_select_attack_action(8, 7, 6, 0) ==
          FD2_FIELD_AI_ACTION_PHYSICAL);
    CHECK(fd2_field_ai_select_attack_action(8, 8, 6, 0) ==
          FD2_FIELD_AI_ACTION_MAGIC);
    CHECK(fd2_field_ai_select_attack_action(8, 8, 6, 1) ==
          FD2_FIELD_AI_ACTION_PHYSICAL);
    CHECK(fd2_field_ai_select_attack_action(8, 6, 8, 0) ==
          FD2_FIELD_AI_ACTION_ITEM);
    CHECK(fd2_field_ai_select_attack_action(7, 8, 8, 0) ==
          FD2_FIELD_AI_ACTION_MAGIC);
    CHECK(fd2_field_ai_select_attack_action(8, 8, 8, 1) ==
          FD2_FIELD_AI_ACTION_HANDLED_NOOP);

    fd2_field_unit actor = {.attack = 20};
    fd2_field_unit physical_target = {.defense = 10};
    fd2_field_ai_physical_candidate physical_choice = {.priority = 8};
    fd2_field_ai_magic_candidate magic_choice = {
        .magic_id = 0, .score = 8,
    };
    fd2_field_ai_item_candidate item_choice = {.score = 0};
    CHECK(fd2_field_ai_select_attack_action_for_candidates(
              &actor, &physical_target, &physical_choice,
              &magic_choice, &item_choice) == FD2_FIELD_AI_ACTION_MAGIC);
    actor.attack = 80;
    CHECK(fd2_field_ai_select_attack_action_for_candidates(
              &actor, &physical_target, &physical_choice,
              &magic_choice, &item_choice) == FD2_FIELD_AI_ACTION_PHYSICAL);
    magic_choice.magic_id = 13;
    actor.ai_behavior_raw = 0;
    CHECK(fd2_field_ai_select_attack_action_for_candidates(
              &actor, &physical_target, &physical_choice,
              &magic_choice, &item_choice) == FD2_FIELD_AI_ACTION_MAGIC);
    actor.ai_behavior_raw = 0x40;
    CHECK(fd2_field_ai_select_attack_action_for_candidates(
              &actor, &physical_target, &physical_choice,
              &magic_choice, &item_choice) == FD2_FIELD_AI_ACTION_PHYSICAL);
    magic_choice.score = 0;
    item_choice.score = 8;
    actor.ai_behavior_raw = 0;
    CHECK(fd2_field_ai_select_attack_action_for_candidates(
              &actor, &physical_target, &physical_choice,
              &magic_choice, &item_choice) == FD2_FIELD_AI_ACTION_ITEM);
    actor.ai_behavior_raw = 0x40;
    CHECK(fd2_field_ai_select_attack_action_for_candidates(
              &actor, &physical_target, &physical_choice,
              &magic_choice, &item_choice) == FD2_FIELD_AI_ACTION_PHYSICAL);
    return 0;
}

int main(void) {
    if (test_behavior_and_nearest_target() != 0 ||
        test_destination_order() != 0 ||
        test_magic_and_item_scoring() != 0 ||
        test_physical_candidate_and_dispatch() != 0)
        return 1;
    puts("field_ai_test: ok");
    return 0;
}
