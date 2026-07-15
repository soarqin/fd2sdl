/* 炎龙骑士团 2 SDL3 重写 - 战场移动查询策略
 *
 * 逆向依据：
 * - field_movement_profile_get @0x73ab9：record offset 0x20 选择 20 字节
 *   movement profile；
 * - field_reachable_cells_compute @0x735a4：record offset 0x3b 作为预算；
 * - FUN_0004bc06 / FUN_0004bdc8：FDSHAP attr[1] 索引 profile 中的成本；
 * - field_opponent_zoc_build @0x397e1、field_opponent_zoc_mark_unit
 *   @0x39839、field_cell_zoc_mark @0x398bb：敌方格标记 0x40，四邻格
 *   标记 0x80；
 * - field_friendly_destination_block @0x398e5：同阵营其他单位格在范围
 *   传播后设为不可停留。
 */

#include "field_move.h"

#include <stdlib.h>

static int same_faction(uint8_t a, uint8_t b) {
    return (a == 0) == (b == 0);
}

static int hostile_occupied(const fd2_field_move_query_context *context,
                            int x, int y) {
    if (!context || !context->units ||
        context->mover_index >= context->units->count)
        return 0;
    const fd2_field_unit *mover =
        &context->units->items[context->mover_index];
    for (size_t i = 0; i < context->units->count; i++) {
        const fd2_field_unit *unit = &context->units->items[i];
        if (i == context->mover_index ||
            (unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0 ||
            unit->x != x || unit->y != y)
            continue;
        if (!same_faction(mover->side, unit->side)) return 1;
    }
    return 0;
}

int fd2_field_move_unit_at(const fd2_field_units *units,
                           int x, int y,
                           int ignore_index,
                           int visible_only) {
    if (!units || x < 0 || x > 0xff || y < 0 || y > 0xff) return -1;
    for (size_t i = 0; i < units->count; i++) {
        const fd2_field_unit *unit = &units->items[i];
        if ((int)i == ignore_index) continue;
        if (visible_only &&
            (unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0)
            continue;
        if (unit->x == x && unit->y == y) return (int)i;
    }
    return -1;
}

static int opponent_adjacent(const fd2_field_move_query_context *context,
                             int x, int y) {
    const fd2_field_unit *mover =
        &context->units->items[context->mover_index];
    for (size_t i = 0; i < context->units->count; i++) {
        const fd2_field_unit *unit = &context->units->items[i];
        if (i == context->mover_index ||
            (unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0 ||
            same_faction(mover->side, unit->side))
            continue;
        int dx = abs((int)unit->x - x);
        int dy = abs((int)unit->y - y);
        if (dx + dy == 1) return 1;
    }
    return 0;
}

int fd2_field_move_query_step(void *opaque,
                              int from_x, int from_y,
                              int to_x, int to_y,
                              fd2_field_path_step *step) {
    (void)from_x;
    (void)from_y;
    fd2_field_move_query_context *context = opaque;
    if (!context || !context->map || !context->map->cells ||
        !context->terrain || !context->units || !step ||
        context->mover_index >= context->units->count ||
        !context->movement_profile_costs ||
        to_x < 0 || to_y < 0 ||
        to_x >= context->map->width || to_y >= context->map->height)
        return -1;

    uint32_t cell = context->map->cells[
        (size_t)to_y * (size_t)context->map->width + (size_t)to_x];
    uint16_t terrain_id = (uint16_t)(cell & 0x03ffu);
    if (!context->terrain->attrs ||
        terrain_id >= context->terrain->attr_count)
        return -1;
    uint8_t cost_class =
        context->terrain->attrs[terrain_id].movement_cost_class;
    if (cost_class >= context->movement_profile_cost_count)
        return -1;

    const fd2_field_unit *mover =
        &context->units->items[context->mover_index];
    int friendly_occupied = 0;
    /* 剧情 placement 允许重复坐标；移动判定必须检查该格全部单位，
     * 不能只依赖首个命中的 actor。任一敌方存在都使该格不可进入。 */
    for (size_t i = 0; i < context->units->count; i++) {
        const fd2_field_unit *occupant = &context->units->items[i];
        if (i == context->mover_index ||
            (occupant->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0 ||
            occupant->x != to_x || occupant->y != to_y)
            continue;
        if (!same_faction(mover->side, occupant->side)) return 0;
        friendly_occupied = 1;
    }

    step->cost = context->movement_profile_costs[cost_class];
    step->can_stop = !friendly_occupied;
    step->can_expand = !opponent_adjacent(context, to_x, to_y);
    return 1;
}

int fd2_field_move_range_compute(fd2_field_path_result *result,
                                 const fd2_field_map *map,
                                 const fd2_terrain_tileset *terrain,
                                 const fd2_field_units *units,
                                 size_t mover_index,
                                 const uint8_t *movement_profile_costs,
                                 size_t movement_profile_cost_count,
                                 uint32_t movement_points) {
    if (!result || !map || !map->cells || !terrain || !units ||
        mover_index >= units->count || !movement_profile_costs ||
        movement_profile_cost_count == 0)
        return -1;
    const fd2_field_unit *mover = &units->items[mover_index];
    fd2_field_move_query_context context = {
        .map = map,
        .terrain = terrain,
        .units = units,
        .mover_index = mover_index,
        .movement_profile_costs = movement_profile_costs,
        .movement_profile_cost_count = movement_profile_cost_count,
    };
    int start_can_stop = 1;
    for (size_t i = 0; i < units->count; i++) {
        const fd2_field_unit *occupant = &units->items[i];
        if (i == mover_index ||
            (occupant->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0 ||
            occupant->x != mover->x || occupant->y != mover->y)
            continue;
        if (same_faction(mover->side, occupant->side))
            start_can_stop = 0;
    }
    return fd2_field_path_compute_with_start_policy(
        result, map->width, map->height,
        mover->x, mover->y, movement_points,
        start_can_stop,
        !opponent_adjacent(&context, mover->x, mover->y),
        fd2_field_move_query_step, &context);
}

typedef struct {
    fd2_field_move_query_context query;
    fd2_field_move_path_mode mode;
    int target_x;
    int target_y;
    uint8_t *best_remaining;
    uint8_t *best_turn_score;
    uint8_t *stack;
    size_t stack_capacity;
    uint8_t *directions;
    size_t directions_capacity;
    size_t best_length;
    int found_target;
    int witness_x;
    int witness_y;
    int found_witness;
    int failed;
} fd2_field_move_path_search;

static uint8_t path_turn_score(const uint8_t *directions, size_t length) {
    if (!directions || length == 0) return 0;
    uint8_t transitions = 0;
    for (size_t i = 1; i < length; i++) {
        if (directions[i] != directions[i - 1u] && transitions != UINT8_MAX)
            transitions++;
    }
    /* 0x4e71f 统计递归栈中相邻 direction 发生变化的次数，返回 count*4。 */
    return transitions > 63u ? 0xfcu : (uint8_t)(transitions << 2);
}

static int path_cell_cost(const fd2_field_move_path_search *search,
                          int x, int y, uint8_t *cost) {
    if (!search || !cost || x < 0 || y < 0 ||
        x >= search->query.map->width || y >= search->query.map->height)
        return -1;
    uint32_t cell = search->query.map->cells[
        (size_t)y * (size_t)search->query.map->width + (size_t)x];
    uint16_t terrain_id = (uint16_t)(cell & 0x03ffu);
    if (!search->query.terrain->attrs ||
        terrain_id >= search->query.terrain->attr_count)
        return -1;
    uint8_t cost_class =
        search->query.terrain->attrs[terrain_id].movement_cost_class;
    if (cost_class >= search->query.movement_profile_cost_count) return -1;
    *cost = search->query.movement_profile_costs[cost_class];
    return 0;
}

static void move_path_search_visit(fd2_field_move_path_search *search,
                                   int x, int y,
                                   uint8_t remaining,
                                   size_t depth) {
    if (!search || search->failed || depth > search->stack_capacity) return;
    if (search->mode != FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS &&
        x == search->target_x && y == search->target_y &&
        (!search->found_target || depth <= search->best_length)) {
        if (depth > search->directions_capacity ||
            (depth != 0 && !search->directions)) {
            search->failed = 1;
            return;
        }
        if (depth != 0)
            memcpy(search->directions, search->stack, depth);
        search->best_length = depth;
        search->found_target = 1;
    }

    /* field_path_find @0x7370a / 正确 dual 0x4e4f6 的递归顺序。 */
    static const int dx[] = {1, -1, 0, 0};
    static const int dy[] = {0, 0, 1, -1};
    static const uint8_t direction[] = {3, 1, 0, 2};
    for (size_t d = 0; d < 4u && !search->failed; d++) {
        int nx = x + dx[d];
        int ny = y + dy[d];
        if (nx < 0 || ny < 0 || nx >= search->query.map->width ||
            ny >= search->query.map->height)
            continue;
        uint8_t cost = 0;
        if (path_cell_cost(search, nx, ny, &cost) != 0) {
            search->failed = 1;
            return;
        }
        if (cost > remaining) continue;
        uint8_t next_remaining = (uint8_t)(remaining - cost);
        int hostile = hostile_occupied(&search->query, nx, ny);
        if (hostile && search->mode != FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS)
            continue;
        if (hostile) {
            /* mode 2 的 success callback 在普通 ZOC／destination 阻挡前
             * 观察 hostile occupied cell。 */
            search->witness_x = nx;
            search->witness_y = ny;
            search->found_witness = 1;
            continue;
        }

        size_t index = (size_t)ny * (size_t)search->query.map->width +
                       (size_t)nx;
        if (depth >= search->stack_capacity) continue;
        search->stack[depth] = direction[d];
        uint8_t turn_score = path_turn_score(search->stack, depth + 1u);
        if (next_remaining < search->best_remaining[index]) continue;
        if (next_remaining == search->best_remaining[index] &&
            search->best_remaining[index] != 0u &&
            (search->mode != FD2_FIELD_MOVE_PATH_EQUAL_ROUTE ||
             turn_score <= search->best_turn_score[index]))
            continue;

        search->best_remaining[index] = next_remaining;
        search->best_turn_score[index] = turn_score;
        /* mode 2 的 witness probe 由 path finder 自身识别 hostile cell，
         * 不应用普通 reachable-range 的 ZOC stop；否则永远无法越过首个
         * hostile 邻格观察 occupied cell。 */
        if (search->mode != FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS &&
            opponent_adjacent(&search->query, nx, ny)) {
            next_remaining = 0;
            search->best_remaining[index] = 0;
        }
        move_path_search_visit(
            search, nx, ny, next_remaining, depth + 1u);
    }
}

int fd2_field_move_path_find(const fd2_field_map *map,
                             const fd2_terrain_tileset *terrain,
                             const fd2_field_units *units,
                             size_t mover_index,
                             const uint8_t *movement_profile_costs,
                             size_t movement_profile_cost_count,
                             uint32_t movement_points,
                             fd2_field_move_path_mode mode,
                             int target_x, int target_y,
                             uint8_t *directions, size_t capacity,
                             size_t *path_length,
                             int *witness_x, int *witness_y) {
    if (!map || !map->cells || map->width <= 0 || map->height <= 0 ||
        !terrain || !units || mover_index >= units->count ||
        !movement_profile_costs || movement_profile_cost_count == 0 ||
        movement_points > UINT8_MAX ||
        mode > FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS || !path_length ||
        (mode != FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS &&
         (target_x < 0 || target_y < 0 || target_x >= map->width ||
          target_y >= map->height)))
        return -1;
    if ((size_t)map->width > SIZE_MAX / (size_t)map->height) return -1;
    size_t count = (size_t)map->width * (size_t)map->height;
    uint8_t *work = calloc(count * 3u, sizeof(uint8_t));
    if (!work) return -1;

    *path_length = 0;
    if (witness_x) *witness_x = -1;
    if (witness_y) *witness_y = -1;
    fd2_field_move_path_search search = {
        .query = {
            .map = map,
            .terrain = terrain,
            .units = units,
            .mover_index = mover_index,
            .movement_profile_costs = movement_profile_costs,
            .movement_profile_cost_count = movement_profile_cost_count,
        },
        .mode = mode,
        .target_x = target_x,
        .target_y = target_y,
        .best_remaining = work,
        .best_turn_score = work + count,
        .stack = work + count * 2u,
        .stack_capacity = count,
        .directions = directions,
        .directions_capacity = capacity,
        .best_length = SIZE_MAX,
        .witness_x = -1,
        .witness_y = -1,
    };
    const fd2_field_unit *mover = &units->items[mover_index];
    search.best_remaining[
        (size_t)mover->y * (size_t)map->width + (size_t)mover->x] =
        (uint8_t)movement_points;
    move_path_search_visit(
        &search, mover->x, mover->y, (uint8_t)movement_points, 0);

    int found = mode == FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS
        ? search.found_witness : search.found_target;
    if (!search.failed && found) {
        if (mode == FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS) {
            if (witness_x) *witness_x = search.witness_x;
            if (witness_y) *witness_y = search.witness_y;
        } else {
            *path_length = search.best_length;
        }
    }
    free(work);
    return search.failed ? -1 : found;
}
