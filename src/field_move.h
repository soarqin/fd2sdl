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

/* field_path_find @0x7370a（正确 dual 0x4e4f6）的 mode 参数：
 * mode 0 是普通路径；mode 1 在等剩余成本时比较递归路径形状；mode 2
 * 在抵达 hostile occupied cell 时更新 witness 坐标。三种模式共享
 * 已确认的地形、占格与 ZOC 查询策略。 */
typedef enum {
    FD2_FIELD_MOVE_PATH_NORMAL = 0,
    FD2_FIELD_MOVE_PATH_EQUAL_ROUTE = 1,
    FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS = 2
} fd2_field_move_path_mode;

/* 返回 1 表示找到目标／witness，0 表示未找到，-1 表示输入或数据错误。
 * mode 0/1 把最终方向序列写入 directions/path_length；mode 2 只写
 * witness_x/y。方向缓冲容量不足属于错误，不返回截断路径。 */
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
                             int *witness_x, int *witness_y);

#endif /* FD2_FIELD_MOVE_H */
