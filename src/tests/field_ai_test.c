#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "field_ai.h"

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

int main(void) {
    if (test_behavior_and_nearest_target() != 0 ||
        test_destination_order() != 0)
        return 1;
    puts("field_ai_test: ok");
    return 0;
}
