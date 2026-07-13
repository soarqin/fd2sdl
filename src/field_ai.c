/* 炎龙骑士团 2 SDL3 重写 - 战场 AI 纯查询层
 *
 * corrected code0 0x28cb3 从 actor +0x34 低半字节分派 AI behavior；
 * 0x290b0 按阵营选择最近目标，0x29d8c 对可达目的地作稳定排序。
 * 本模块只复现已确认的查询，不提前翻译尚未完成的攻击／移动提交。
 */
#include "field_ai.h"

static uint32_t abs_diff(int a, int b) {
    return a >= b ? (uint32_t)(a - b) : (uint32_t)(b - a);
}

static uint32_t abs_int(int value) {
    return value >= 0 ? (uint32_t)value : (uint32_t)(-value);
}

static uint32_t abs_i64(int64_t value) {
    uint64_t magnitude = value >= 0 ? (uint64_t)value : (uint64_t)(-value);
    return magnitude > UINT32_MAX ? UINT32_MAX : (uint32_t)magnitude;
}

uint8_t fd2_field_ai_behavior(const fd2_field_unit *unit) {
    return unit ? (uint8_t)(unit->ai_behavior_raw & 0x0fu) : 0xffu;
}

int fd2_field_ai_nearest_opponent(const fd2_field_units *units,
                                  size_t actor_index,
                                  uint8_t side,
                                  fd2_field_ai_target *target) {
    if (!units || !target || actor_index >= units->count)
        return -1;

    const fd2_field_unit *actor = &units->items[actor_index];
    size_t best_index = SIZE_MAX;
    uint32_t best_distance = 0xffffu;
    for (size_t i = 0; i < units->count; i++) {
        const fd2_field_unit *candidate = &units->items[i];
        int eligible = side == 0u ? candidate->side != 0u
                                 : candidate->side == 0u;
        if (!eligible) continue;
        uint32_t distance = abs_diff(actor->x, candidate->x) +
                            abs_diff(actor->y, candidate->y);
        if (distance >= best_distance) continue;
        best_index = i;
        best_distance = distance;
    }
    if (best_index == SIZE_MAX) return -1;

    target->unit_index = best_index;
    target->x = units->items[best_index].x;
    target->y = units->items[best_index].y;
    target->manhattan_distance = best_distance;
    return 0;
}

int fd2_field_ai_choose_destination(
        const fd2_field_path_result *range,
        int anchor_x, int anchor_y,
        fd2_field_ai_destination *destination) {
    if (!range || !range->nodes || !destination || range->width <= 0 ||
        range->height <= 0 || anchor_x < 0 || anchor_x >= range->width ||
        anchor_y < 0 || anchor_y >= range->height)
        return -1;

    int best_x = -1;
    int best_y = -1;
    uint32_t best_distance = 0xffu;
    uint32_t best_tie = 0xffu;
    uint32_t best_path_cost = FD2_FIELD_PATH_UNREACHABLE;
    for (int y = 0; y < range->height; y++) {
        for (int x = 0; x < range->width; x++) {
            if (!fd2_field_path_is_destination(range, x, y)) continue;
            int dx = x - anchor_x;
            int dy = y - anchor_y;
            uint32_t distance = abs_int(dx) + abs_int(dy);
            /* corrected code0 0x2a020..0x2a04c：编译结果确实是
             * abs(dx - abs(dy))，不是常见的 abs(abs(dx)-abs(dy))。 */
            uint32_t tie = abs_i64((int64_t)dx - abs_int(dy));
            if (distance > best_distance ||
                (distance == best_distance && tie >= best_tie))
                continue;
            best_x = x;
            best_y = y;
            best_distance = distance;
            best_tie = tie;
            best_path_cost = fd2_field_path_distance(range, x, y);
        }
    }
    if (best_x < 0) return -1;

    destination->x = best_x;
    destination->y = best_y;
    destination->manhattan_distance = best_distance;
    destination->tie_score = best_tie;
    destination->path_cost = best_path_cost;
    return 0;
}
