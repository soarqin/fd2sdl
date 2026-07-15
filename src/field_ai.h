/* 炎龙骑士团 2 SDL3 重写 - 战场 AI 纯查询层
 *
 * 逆向依据：field_side1_phase_execute @0x42a1f、
 * field_side0_phase_execute @0x42ace、field_ai_unit_execute @0x38cb3、
 * field_ai_move_toward_nearest @0x390b0 与
 * field_ai_move_toward_cell @0x39d8c、field_ai_try_attack @0x3a104、
 * field_ai_item_targets_score @0x3aa94 与
 * field_ai_magic_targets_score @0x3ad8b。
 */
#ifndef FD2_FIELD_AI_H
#define FD2_FIELD_AI_H

#include <stddef.h>
#include <stdint.h>

#include "field.h"
#include "field_path.h"
#include "field_unit.h"
#include "tile.h"

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

typedef struct {
    size_t unit_index;
    int destination_x;
    int destination_y;
    int32_t priority;
    int32_t secondary_score;
} fd2_field_ai_physical_candidate;

/* 0x3ad8b / 0x3aa94 的纯目标列表评分输入。target_indices 保持
 * field_target_range_build 的 actor 表顺序，评分器不修改列表或单位。 */
typedef struct {
    const fd2_field_units *units;
    const uint8_t *target_indices;
    size_t target_count;
} fd2_field_ai_score_targets;

typedef struct {
    uint8_t magic_id;
    int cast_x;
    int cast_y;
    int32_t score;
    uint16_t tie_value;
} fd2_field_ai_magic_candidate;

typedef struct {
    uint8_t item_id;
    size_t slot_index;
    int target_x;
    int target_y;
    int32_t score;
} fd2_field_ai_item_candidate;

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

/* 0x3944b（corrected code0 0x2944b）的普通物理候选比较核心。
 * priority 只取已确认的 0/8/18；同 priority 时 secondary_score 严格
 * 更大才替换，因此保持原函数的遍历顺序稳定性。 */
int fd2_field_ai_physical_candidate_is_better(
    const fd2_field_ai_physical_candidate *candidate,
    const fd2_field_ai_physical_candidate *best,
    int have_best);

/* 从已经生成的可停留移动范围中，枚举“移动后可普通攻击”的
 * (destination,target) 对。candidate_score 由 0x3944b 的机器码公式
 * 生成；不修改单位、范围或 RNG。当前只复现最终候选比较，不生成
 * move path，也不提交战斗。 */
int fd2_field_ai_choose_physical_candidate(
    const fd2_field_map *map,
    const fd2_terrain_tileset *terrain,
    const fd2_field_units *units,
    size_t actor_index,
    uint8_t side_selector,
    const fd2_field_path_result *move_range,
    fd2_field_ai_physical_candidate *candidate);

/* field_ai_magic_targets_score @0x3ad8b 与
 * field_ai_magic_zero_byte_targets_score @0x3afb6。magic_id 的分支和
 * 原始 unit offset 比较逐字节复现，不扣 MP。候选查询同时复现
 * 0x3ab9e 的已知法术、MP、施法中心、目标范围和稳定 tie-break；
 * 仍不执行 0x3a525 的法术效果提交。 */
int fd2_field_ai_magic_targets_score(
    uint8_t magic_id,
    const fd2_field_ai_score_targets *targets,
    int32_t *score);
int fd2_field_ai_magic_candidate_is_better(
    const fd2_field_ai_magic_candidate *candidate,
    const fd2_field_ai_magic_candidate *best,
    int have_best);
int fd2_field_ai_choose_magic_candidate(
    const fd2_field_map *map,
    const fd2_terrain_tileset *terrain,
    const fd2_field_units *units,
    size_t actor_index,
    uint8_t selector_mode,
    fd2_field_ai_magic_candidate *candidate);

/* field_ai_item_targets_score @0x3aa94：实现 code 5/13/20/21/24；
 * 其中 code 20/21 依赖 magic_record_get(raw_value)。候选查询复现
 * 0x3a892 的槽位、普通／轴线范围和严格高分替换；未知 code 返回 0，
 * 不执行 0x45e83 的物品效果或消耗。 */
int fd2_field_ai_item_targets_score(
    uint8_t item_id,
    const fd2_field_ai_score_targets *targets,
    int32_t *score);
int fd2_field_ai_item_candidate_is_better(
    const fd2_field_ai_item_candidate *candidate,
    const fd2_field_ai_item_candidate *best,
    int have_best);
int fd2_field_ai_choose_item_candidate(
    const fd2_field_map *map,
    const fd2_terrain_tileset *terrain,
    const fd2_field_units *units,
    size_t actor_index,
    uint8_t selector_mode,
    fd2_field_ai_item_candidate *candidate);

/* field_ai_try_attack @0x3a104（corrected code0 0x2a104）在三类候选
 * 分数中使用的动作选择图。三者都低于 6 时返回 NONE；达到阈值但三项
 * 完全相等时，原函数返回成功却不调用任何提交 helper，故返回 NONE。
 * A 与 magic/item 平价时需要 tie_prefer_physical（由调用方按已确认的
 * record +0x34 / magic record 规则计算）。 */
typedef enum {
    /* 三项都低于 6：0x38cbd 必须继续 0x39335/nearest fallback。 */
    FD2_FIELD_AI_ACTION_NONE = 0,
    FD2_FIELD_AI_ACTION_PHYSICAL,
    FD2_FIELD_AI_ACTION_MAGIC,
    FD2_FIELD_AI_ACTION_ITEM,
    /* 三项完全相等：0x3a104 返回成功却不调用 helper，停止 fallback。 */
    FD2_FIELD_AI_ACTION_HANDLED_NOOP
} fd2_field_ai_action;

fd2_field_ai_action fd2_field_ai_select_attack_action(
    int32_t physical_score,
    int32_t magic_score,
    int32_t item_score,
    int tie_prefer_physical);

/* field_ai_try_attack @0x3a104 的完整平价门控。物理候选目标防御用于
 * 计算 actor.attack-target.defense；减法保持原版 unsigned u16→32 位
 * 环绕。magic ID <11 时以 magic record u16(+0) 与该差值比较；ID>=11
 * 时以 actor +0x34 bit6 决定物理／法术平价。物理／物品平价则同样由
 * bit6 决定。函数只选择，不提交动作。 */
fd2_field_ai_action fd2_field_ai_select_attack_action_for_candidates(
    const fd2_field_unit *actor,
    const fd2_field_unit *physical_target,
    const fd2_field_ai_physical_candidate *physical,
    const fd2_field_ai_magic_candidate *magic,
    const fd2_field_ai_item_candidate *item);

#endif /* FD2_FIELD_AI_H */
