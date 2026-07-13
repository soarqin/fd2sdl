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
