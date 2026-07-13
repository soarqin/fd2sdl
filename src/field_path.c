/* 炎龙骑士团 2 SDL3 重写 - 参数化战场路径搜索
 *
 * 逆向依据：field_reachable_cells_compute @0x735a4 与
 * field_path_find @0x7370a 均按四邻域传播剩余移动力；具体地形成本、
 * 单位占格与控制区规则由调用方 query 提供，避免在字段未确认时写死。
 */

#include "field_path.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

static int in_bounds(const fd2_field_path_result *result, int x, int y) {
    return result && x >= 0 && y >= 0 &&
           x < result->width && y < result->height;
}

static size_t node_index(const fd2_field_path_result *result, int x, int y) {
    return (size_t)y * (size_t)result->width + (size_t)x;
}

void fd2_field_path_close(fd2_field_path_result *result) {
    if (!result) return;
    free(result->nodes);
    memset(result, 0, sizeof(*result));
}

int fd2_field_path_compute_with_start_policy(
    fd2_field_path_result *result,
    int width, int height,
    int start_x, int start_y,
    uint32_t budget,
    int start_can_stop, int start_can_expand,
    fd2_field_path_query query,
    void *context) {
    if (!result || !query || width <= 0 || height <= 0 ||
        start_x < 0 || start_y < 0 || start_x >= width || start_y >= height)
        return -1;
    if ((size_t)width > SIZE_MAX / (size_t)height)
        return -1;
    size_t count = (size_t)width * (size_t)height;
    /* previous 使用 int32_t，fd2_field_path_build() 以 int 返回步数。 */
    if (count > (size_t)INT32_MAX ||
        count > SIZE_MAX / sizeof(fd2_field_path_node))
        return -1;

    fd2_field_path_close(result);
    result->nodes = calloc(count, sizeof(*result->nodes));
    if (!result->nodes) return -1;
    result->width = width;
    result->height = height;
    result->start_x = start_x;
    result->start_y = start_y;
    result->budget = budget;
    for (size_t i = 0; i < count; i++) {
        result->nodes[i].distance = FD2_FIELD_PATH_UNREACHABLE;
        result->nodes[i].previous = -1;
    }

    size_t start = node_index(result, start_x, start_y);
    result->nodes[start].distance = 0;
    result->nodes[start].can_stop = start_can_stop != 0;
    result->nodes[start].can_expand = start_can_expand != 0;

    /* 原版展开顺序：右、左、下、上；方向码仍为 3、1、0、2。 */
    static const int dx[] = {1, -1, 0, 0};
    static const int dy[] = {0, 0, 1, -1};
    static const uint8_t direction[] = {3, 1, 0, 2};

    for (size_t settled = 0; settled < count; settled++) {
        size_t best = SIZE_MAX;
        uint32_t best_distance = FD2_FIELD_PATH_UNREACHABLE;
        for (size_t i = 0; i < count; i++) {
            const fd2_field_path_node *node = &result->nodes[i];
            if (!node->visited && node->distance < best_distance) {
                best = i;
                best_distance = node->distance;
            }
        }
        if (best == SIZE_MAX || best_distance > budget) break;

        fd2_field_path_node *from = &result->nodes[best];
        from->visited = 1;
        if (!from->can_expand) continue;
        int from_x = (int)(best % (size_t)width);
        int from_y = (int)(best / (size_t)width);

        for (size_t d = 0; d < 4; d++) {
            int to_x = from_x + dx[d];
            int to_y = from_y + dy[d];
            if (!in_bounds(result, to_x, to_y)) continue;

            fd2_field_path_step step = {0};
            int query_result = query(context, from_x, from_y,
                                     to_x, to_y, &step);
            if (query_result < 0) {
                fd2_field_path_close(result);
                return -1;
            }
            if (query_result == 0 ||
                step.cost > budget - best_distance)
                continue;

            uint32_t candidate = best_distance + step.cost;
            size_t next = node_index(result, to_x, to_y);
            fd2_field_path_node *to = &result->nodes[next];
            if (candidate >= to->distance) continue;
            to->distance = candidate;
            to->previous = (int32_t)best;
            to->direction = direction[d];
            to->can_stop = step.can_stop != 0;
            to->can_expand = step.can_expand != 0;
        }
    }
    return 0;
}

int fd2_field_path_compute(fd2_field_path_result *result,
                           int width, int height,
                           int start_x, int start_y,
                           uint32_t budget,
                           fd2_field_path_query query,
                           void *context) {
    return fd2_field_path_compute_with_start_policy(
        result, width, height, start_x, start_y, budget,
        1, 1, query, context);
}

int fd2_field_path_is_destination(const fd2_field_path_result *result,
                                  int x, int y) {
    if (!in_bounds(result, x, y) || !result->nodes) return 0;
    const fd2_field_path_node *node = &result->nodes[node_index(result, x, y)];
    return node->distance != FD2_FIELD_PATH_UNREACHABLE && node->can_stop;
}

uint32_t fd2_field_path_distance(const fd2_field_path_result *result,
                                 int x, int y) {
    if (!in_bounds(result, x, y) || !result->nodes)
        return FD2_FIELD_PATH_UNREACHABLE;
    return result->nodes[node_index(result, x, y)].distance;
}

int fd2_field_path_build(const fd2_field_path_result *result,
                         int target_x, int target_y,
                         uint8_t *directions, size_t capacity) {
    if (!result || !result->nodes ||
        !fd2_field_path_is_destination(result, target_x, target_y))
        return -1;

    size_t count = (size_t)result->width * (size_t)result->height;
    size_t current = node_index(result, target_x, target_y);
    size_t start = node_index(result, result->start_x, result->start_y);
    size_t length = 0;
    while (current != start) {
        if (length >= count) return -1;
        const fd2_field_path_node *node = &result->nodes[current];
        if (node->previous < 0 || (size_t)node->previous >= count)
            return -1;
        length++;
        current = (size_t)node->previous;
    }
    if (length > capacity || (length != 0 && !directions)) return -1;

    current = node_index(result, target_x, target_y);
    for (size_t i = length; i > 0; i--) {
        const fd2_field_path_node *node = &result->nodes[current];
        directions[i - 1] = node->direction;
        current = (size_t)node->previous;
    }
    return (int)length;
}
