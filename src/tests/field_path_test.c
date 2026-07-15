#include <stdio.h>
#include <string.h>

#include "field_move.h"
#include "field_move_profile.h"
#include "field_path.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return -1; \
    } \
} while (0)

typedef struct {
    int width;
    uint8_t costs[16];
    uint8_t blocked[16];
    uint8_t stop[16];
    uint8_t expand[16];
} test_grid;

static int test_query(void *opaque,
                      int from_x, int from_y,
                      int to_x, int to_y,
                      fd2_field_path_step *step) {
    (void)from_x;
    (void)from_y;
    test_grid *grid = opaque;
    int index = to_y * grid->width + to_x;
    if (grid->blocked[index]) return 0;
    step->cost = grid->costs[index];
    step->can_stop = grid->stop[index];
    step->can_expand = grid->expand[index];
    return 1;
}

static void grid_init(test_grid *grid, int width, int count) {
    memset(grid, 0, sizeof(*grid));
    grid->width = width;
    for (int i = 0; i < count; i++) {
        grid->costs[i] = 1;
        grid->stop[i] = 1;
        grid->expand[i] = 1;
    }
}

static int test_weighted_path(void) {
    fd2_field_path_result result = {0};
    test_grid grid;
    grid_init(&grid, 3, 6);
    grid.costs[1] = 5;

    CHECK(fd2_field_path_compute(&result, 3, 2, 0, 0, 4,
                                 test_query, &grid) == 0);
    CHECK(fd2_field_path_distance(&result, 2, 0) == 4);
    uint8_t path[4];
    CHECK(fd2_field_path_build(&result, 2, 0, path, sizeof(path)) == 4);
    CHECK(path[0] == 0 && path[1] == 3 &&
          path[2] == 3 && path[3] == 2);
    CHECK(!fd2_field_path_is_destination(&result, 1, 0));
    fd2_field_path_close(&result);
    return 0;
}

static int test_stop_and_transit(void) {
    fd2_field_path_result result = {0};
    test_grid grid;
    grid_init(&grid, 3, 3);
    grid.stop[1] = 0;
    CHECK(fd2_field_path_compute(&result, 3, 1, 0, 0, 2,
                                 test_query, &grid) == 0);
    CHECK(!fd2_field_path_is_destination(&result, 1, 0));
    CHECK(fd2_field_path_is_destination(&result, 2, 0));
    fd2_field_path_close(&result);

    grid_init(&grid, 3, 3);
    grid.expand[1] = 0;
    CHECK(fd2_field_path_compute(&result, 3, 1, 0, 0, 2,
                                 test_query, &grid) == 0);
    CHECK(fd2_field_path_is_destination(&result, 1, 0));
    CHECK(fd2_field_path_distance(&result, 2, 0) ==
          FD2_FIELD_PATH_UNREACHABLE);
    fd2_field_path_close(&result);
    return 0;
}

static int test_budget_and_tie_order(void) {
    fd2_field_path_result result = {0};
    test_grid grid;
    grid_init(&grid, 1, 1);
    CHECK(fd2_field_path_compute(&result, 46341, 46341, 0, 0, 1,
                                 test_query, &grid) == -1);
    grid_init(&grid, 2, 4);
    CHECK(fd2_field_path_compute(&result, 2, 2, 0, 0, 1,
                                 test_query, &grid) == 0);
    CHECK(fd2_field_path_is_destination(&result, 1, 0));
    CHECK(!fd2_field_path_is_destination(&result, 1, 1));
    fd2_field_path_close(&result);

    CHECK(fd2_field_path_compute(&result, 2, 2, 0, 0, 2,
                                 test_query, &grid) == 0);
    uint8_t path[2];
    CHECK(fd2_field_path_build(&result, 1, 1, path, sizeof(path)) == 2);
    CHECK(path[0] == 3 && path[1] == 0);
    fd2_field_path_close(&result);
    return 0;
}

static int test_movement_profiles(void) {
    const uint8_t *profile0 = fd2_field_movement_profile_get(0);
    const uint8_t *profile1 = fd2_field_movement_profile_get(1);
    const uint8_t *profile5 = fd2_field_movement_profile_get(5);
    CHECK(fd2_field_movement_profile_count() == 29);
    CHECK(profile0 && profile0[13] == 0);
    CHECK(profile1 && profile1[0] == 1 && profile1[17] == 0x14);
    CHECK(profile5 && profile5[0] == 2 && profile5[1] == 0x14 &&
          profile5[2] == 1);
    CHECK(fd2_field_movement_profile_get(29) == NULL);

    /* 原表确有零成本项；确认预算为零时仍能安全收敛并重建路径。 */
    fd2_field_path_result result = FD2_FIELD_PATH_RESULT_INITIALIZER;
    test_grid grid;
    grid_init(&grid, 3, 3);
    memset(grid.costs, 0, 3);
    CHECK(fd2_field_path_compute(&result, 3, 1, 0, 0, 0,
                                 test_query, &grid) == 0);
    CHECK(fd2_field_path_distance(&result, 2, 0) == 0);
    uint8_t path[2];
    CHECK(fd2_field_path_build(&result, 2, 0, path, sizeof(path)) == 2);
    fd2_field_path_close(&result);
    return 0;
}

static int test_ai_path_modes(void) {
    uint32_t cells[5] = {0};
    fd2_field_map map = {.width = 5, .height = 1, .cells = cells};
    fd2_terrain_attr attrs[1] = {{.movement_cost_class = 0}};
    fd2_terrain_tileset terrain = {.attrs = attrs, .attr_count = 1};
    fd2_field_units units = {0};
    units.count = 2;
    units.items[0].x = 0; units.items[0].y = 0; units.items[0].side = 0;
    units.items[1].x = 3; units.items[1].y = 0; units.items[1].side = 2;
    uint8_t profile[1] = {1};
    uint8_t directions[5] = {0};
    size_t length = 0;
    int witness_x = -1;
    int witness_y = -1;

    CHECK(fd2_field_move_path_find(
        &map, &terrain, &units, 0, profile, 1, 0x1c,
        FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS, 0, 0,
        NULL, 0, &length, &witness_x, &witness_y) == 1);
    CHECK(length == 0 && witness_x == 3 && witness_y == 0);
    CHECK(fd2_field_move_path_find(
        &map, &terrain, &units, 0, profile, 1, 3,
        FD2_FIELD_MOVE_PATH_NORMAL, 2, 0,
        directions, sizeof(directions), &length, NULL, NULL) == 1);
    CHECK(length == 2 && directions[0] == 3 && directions[1] == 3);
    CHECK(fd2_field_move_path_find(
        &map, &terrain, &units, 0, profile, 1, 1,
        FD2_FIELD_MOVE_PATH_EQUAL_ROUTE, 2, 0,
        directions, sizeof(directions), &length, NULL, NULL) == 0);
    return 0;
}

static int test_original_occupancy_policy(void) {
    fd2_field_path_result result = {0};
    uint32_t cells[8] = {0};
    fd2_field_map map = {.width = 4, .height = 2, .cells = cells};
    fd2_terrain_attr attrs[1] = {{.movement_cost_class = 0}};
    fd2_terrain_tileset terrain = {.attrs = attrs, .attr_count = 1};
    fd2_field_units units = {0};
    units.count = 3;
    units.items[0].x = 0; units.items[0].y = 0; units.items[0].side = 2;
    units.items[1].x = 1; units.items[1].y = 0; units.items[1].side = 2;
    units.items[2].x = 3; units.items[2].y = 0; units.items[2].side = 0;
    uint8_t profile[1] = {1};

    map.cells = NULL;
    CHECK(fd2_field_move_range_compute(&result, &map, &terrain, &units,
                                       0, profile, 1, 5) == -1);
    map.cells = cells;
    CHECK(fd2_field_move_unit_at(&units, 1, 0, -1, 1) == 1);
    CHECK(fd2_field_move_unit_at(&units, 1, 0, 1, 1) == -1);
    CHECK(fd2_field_move_range_compute(&result, &map, &terrain, &units,
                                       0, profile, 1, 5) == 0);
    CHECK(!fd2_field_path_is_destination(&result, 1, 0));
    CHECK(fd2_field_path_distance(&result, 1, 0) == 1);
    CHECK(fd2_field_path_is_destination(&result, 2, 0));
    CHECK(result.nodes[2].can_expand == 0);
    CHECK(fd2_field_path_distance(&result, 3, 0) ==
          FD2_FIELD_PATH_UNREACHABLE);
    fd2_field_path_close(&result);

    units.items[2].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    CHECK(fd2_field_move_range_compute(&result, &map, &terrain, &units,
                                       0, profile, 1, 5) == 0);
    CHECK(fd2_field_path_is_destination(&result, 3, 0));
    fd2_field_path_close(&result);

    units.items[2].flags = 0;
    units.items[2].x = 1;
    CHECK(fd2_field_move_range_compute(&result, &map, &terrain, &units,
                                       0, profile, 1, 5) == 0);
    CHECK(fd2_field_path_distance(&result, 1, 0) ==
          FD2_FIELD_PATH_UNREACHABLE);
    CHECK(fd2_field_path_distance(&result, 0, 1) ==
          FD2_FIELD_PATH_UNREACHABLE);
    fd2_field_path_close(&result);

    units.items[2].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    units.items[1].x = 0;
    CHECK(fd2_field_move_range_compute(&result, &map, &terrain, &units,
                                       0, profile, 1, 5) == 0);
    CHECK(!fd2_field_path_is_destination(&result, 0, 0));
    CHECK(fd2_field_path_is_destination(&result, 0, 1));
    fd2_field_path_close(&result);
    return 0;
}

int main(void) {
    CHECK(test_weighted_path() == 0);
    CHECK(test_stop_and_transit() == 0);
    CHECK(test_budget_and_tie_order() == 0);
    CHECK(test_movement_profiles() == 0);
    CHECK(test_ai_path_modes() == 0);
    CHECK(test_original_occupancy_policy() == 0);
    puts("field path tests: ok");
    return 0;
}
