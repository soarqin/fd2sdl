/* 炎龙骑士团 2 SDL3 重写 - 战场 AI 纯查询层
 *
 * 逆向依据：field_side1_phase_execute @0x42a1f、
 * field_side0_phase_execute @0x42ace、field_ai_unit_execute @0x38cb3、
 * field_ai_move_toward_nearest @0x390b0 与
 * field_ai_move_toward_cell @0x39d8c。
 */
#ifndef FD2_FIELD_AI_H
#define FD2_FIELD_AI_H

#include <stddef.h>
#include <stdint.h>

#include "field_path.h"
#include "field_unit.h"

typedef struct {
    size_t unit_index;
    int x;
    int y;
    uint32_t manhattan_distance;
} fd2_field_ai_target;

typedef struct {
    int x;
    int y;
    uint32_t manhattan_distance;
    uint32_t tie_score;
    uint32_t path_cost;
} fd2_field_ai_destination;

/* field_ai_unit_execute @0x38cb3 只取 record[0x34] 的低半字节，
 * 已确认分支为 0..5、7..11；高半字节语义仍不命名。 */
uint8_t fd2_field_ai_behavior(const fd2_field_unit *unit);

/* field_ai_move_toward_nearest @0x390b0 的目标扫描：
 * side 0 选择首个最近的 side != 0 单位；side 非 0 选择首个最近的
 * side == 0 单位。同距保持 actor 表中先出现者。原函数不校验 actor
 * 自身阵营，也不检查目标的 hidden/HP/acted，调用方不得在此层
 * 自行补充过滤。 */
int fd2_field_ai_nearest_opponent(const fd2_field_units *units,
                                  size_t actor_index,
                                  uint8_t side,
                                  fd2_field_ai_target *target);

/* field_ai_move_toward_cell @0x39d8c 在已经生成并完成友方占格过滤的
 * 可达表中按行优先扫描可停留格。先最小化到 anchor 的曼哈顿距离；
 * 平价时最小化机器码中的非对称分数 abs(dx - abs(dy))。此函数只查询，
 * 不构造路径，也不修改单位或可达表。 */
int fd2_field_ai_choose_destination(
    const fd2_field_path_result *range,
    int anchor_x, int anchor_y,
    fd2_field_ai_destination *destination);

#endif /* FD2_FIELD_AI_H */
