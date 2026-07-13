#ifndef FD2_FIELD_MOVE_H
#define FD2_FIELD_MOVE_H

#include <stddef.h>
#include <stdint.h>

#include "field.h"
#include "field_path.h"
#include "field_unit.h"
#include "tile.h"

typedef struct {
    const fd2_field_map *map;
    const fd2_terrain_tileset *terrain;
    const fd2_field_units *units;
    size_t mover_index;
    const uint8_t *movement_profile_costs;
    size_t movement_profile_cost_count;
} fd2_field_move_query_context;

/* 返回指定格的首个单位索引。ignore_index 可用于忽略移动者自身；
 * visible_only 非零时跳过原版 flags bit0 隐藏的单位。重复坐标保持单位表顺序。 */
int fd2_field_move_unit_at(const fd2_field_units *units,
                           int x, int y,
                           int ignore_index,
                           int visible_only);

/* 复现已确认的原版占格与控制区规则：敌方所在格不可进入；友方所在格
 * 可通过但不可停留；进入敌方四邻格后剩余移动力归零。阵营按 side==0
 * 与 side!=0 两类判断。地形 attr[1] 只作为 movement profile cost 索引。 */
int fd2_field_move_query_step(void *context,
                              int from_x, int from_y,
                              int to_x, int to_y,
                              fd2_field_path_step *step);

int fd2_field_move_range_compute(fd2_field_path_result *result,
                                 const fd2_field_map *map,
                                 const fd2_terrain_tileset *terrain,
                                 const fd2_field_units *units,
                                 size_t mover_index,
                                 const uint8_t *movement_profile_costs,
                                 size_t movement_profile_cost_count,
                                 uint32_t movement_points);

#endif /* FD2_FIELD_MOVE_H */
