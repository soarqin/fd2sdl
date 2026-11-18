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


/* 4df4c→145cd→4e390→146d1的隔离查询；不改变map。
 * distance是屏蔽友方前BYTE(budget-raw3)（FF为不可达），不是最短路证明；
 * can_stop对应屏蔽后raw3!=FF。previous恒-1，路径必须另调4e4f6。 */
int fd2_field_move_range_compute(fd2_field_path_result *result,
                                 const fd2_field_map *map,
                                 const fd2_terrain_tileset *terrain,
                                 const fd2_field_units *units,
                                 size_t mover_index,
                                 const uint8_t *movement_profile_costs,
                                 size_t movement_profile_cost_count,
                                 uint32_t movement_points);

/* 正式18890入口145cd→4e390→146d1：从当前map owner的raw字节继续，
 * 成功才提交map/result；没有隐式4df4c，失败不提交临时copy。 */
int fd2_field_move_range_apply(fd2_field_path_result *result,
                               fd2_field_map *map,
                               const fd2_terrain_tileset *terrain,
                               const fd2_field_units *units,
                               size_t mover_index,
                               const uint8_t *costs, size_t cost_count,
                               uint32_t movement_points);

/* field_path_find @0x4e4f6（原登记同名）的 mode 参数：
 * mode 0 是普通路径；mode 1 在等剩余成本时比较递归路径形状；mode 2
 * 在抵达 hostile occupied cell 时更新 witness 坐标。三种模式共享
 * 已确认的地形查询。CLEAR_UNIT_FLAGS是SDL额外组合位，不是原版mode4：
 * 仅mode1可搭配，表达14b78在4df4c清图后、尚未重建145cd的调用环境。 */
typedef enum {
    FD2_FIELD_MOVE_PATH_NORMAL = 0,
    FD2_FIELD_MOVE_PATH_EQUAL_ROUTE = 1,
    FD2_FIELD_MOVE_PATH_HOSTILE_WITNESS = 2,
    FD2_FIELD_MOVE_PATH_CLEAR_UNIT_FLAGS = 4
} fd2_field_move_path_mode;

/* 原60073共享输出和60078结果的有界借用视图，不分配／持有持久存储。
 * written为实际已写prefix长度，result是原BYTE（FF、0、长度或witness1），
 * mode保原6017a的本次最终值；frame index32会以绝对y覆盖且pop不恢复。 */
typedef struct {
    uint8_t *bytes;
    size_t capacity,written;
    uint8_t result,mode;
} fd2_field_move_path_output;
/* 与下方query相同的隔离工作图，显式输出view保原方向／XY交替写序。
 * 0完成；-1校验／分配失败（view不改）；-2开始后出错（保已写prefix/result）。
 * H容量门不等同原caller usable extent或首U，外部60181+表collateral另记。 */
int fd2_field_move_path_find_raw(const fd2_field_map *map,
    const fd2_terrain_tileset *terrain,const fd2_field_units *units,size_t mover_index,
    const uint8_t *costs,size_t cost_count,uint32_t movement_points,
    fd2_field_move_path_mode mode,int target_x,int target_y,
    fd2_field_move_path_output *output);

/* 返回1找到typed结果，0未找到，-1错误；输入校验失败不改typed输出。
 * mode0/1投影原result长度，bytes可能在深mode覆盖后含XY；不按初始mode
 * 分裂原输出。mode2投影实际已写的前两byte；directions可选借足raw容量，
 * NULL只提供2B临时视图，深转方向超容量为H错误，不猜原caller extent。
 * 容量不足仍报错，并保合法写前缀，不返回截断成功。 */
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
