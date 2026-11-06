/* 炎龙骑士团 2 SDL3 重写 - 普通攻击范围、目标与武器外围效果 */
#ifndef FD2_FIELD_ATTACK_H
#define FD2_FIELD_ATTACK_H

#include <stddef.h>
#include <stdint.h>

#include "field.h"
#include "field_combat.h"
#include "field_path.h"
#include "field_unit.h"
#include "tile.h"

/* field_equipped_item_slot_find @0x1b83d 的 weapon 模式：按槽位顺序查找
 * flag 0x40 且 item_id < 0x80 的首件装备，再读取物品记录 +0x0b/+0x0c
 * 作为普通攻击最小／最大范围。 */
int fd2_field_attack_weapon_range(const fd2_field_unit *attacker,
                                  uint8_t *min_range,
                                  uint8_t *max_range);

/* field_physical_attack_resolve @0x1ecc7：首件已装备武器 record +0x09
 * 为特效类型；类型 4 时将 +0x0a 累加到职业／移动 profile 暴击基础值。
 * 基础表 DS:0x24a8 的可靠静态映射仍待运行时确认。 */
int fd2_field_attack_weapon_critical_bonus(const fd2_field_unit *attacker,
                                           uint8_t *bonus);

/* item +0x09 == 2 时，field_physical_attack_resolve 在命中判定前先消费
 * rand()%100；小于 item +0x0a 时再消费 rand()%4，并把防御者 +0x25
 * 写为 2..5。无演出实现只提交状态字节和 RNG 顺序。 */
int fd2_field_attack_apply_pre_hit_effect(
    const fd2_field_unit *attacker,
    fd2_field_unit *defender,
    fd2_field_combat_rng_fn rng,
    void *rng_userdata,
    int *applied);

/* SDL 的逻辑分支选择，不是原版函数 ABI 或存档字段。跳过完整演出的
 * 画面仍须使用 FULL，不能切到无演出的 SHORT 改变攻击次数。 */
typedef enum {
    FD2_FIELD_ATTACK_SEQUENCE_SHORT = 0,
    FD2_FIELD_ATTACK_SEQUENCE_FULL = 1
} fd2_field_attack_sequence_mode;

/* 不消费 RNG 的次数上界，供整次动作的 loot 等死亡分支预检使用。
 * SHORT 总是 2；FULL 仅 type3 武器为 3，其他武器为 2。
 * 无效参数/模式/武器返回 -1 且不写 strike_count，不改 attacker。 */
int fd2_field_attack_sequence_max_count_for_mode(
    const fd2_field_unit *attacker,
    fd2_field_attack_sequence_mode mode,
    uint8_t *strike_count);

/* 普通战斗序列的次数上限：先且仅消费一次 sequence RNG。
 * SHORT：field_physical_attack_sequence @0x1e856，两条件均赋值 2；
 * FULL：sub_2EBE1 @0x2ebe1，1 + (rand()%100 < 3) + (effect type == 3)。
 * FULL 的 type3 标记由 sub_2F7B6 @0x2f7b6 在命中判定前写出，
 * 首击 miss 也加一次；目标 HP0 的提前停止仍由执行者负责。
 * 此接口不执行单击、预览或演出。无效参数/模式/武器返回 -1，
 * 不消费 RNG，不写 strike_count；成功返回 0，不改 attacker。 */
int fd2_field_attack_sequence_count_for_mode(
    const fd2_field_unit *attacker,
    fd2_field_attack_sequence_mode mode,
    fd2_field_combat_rng_fn rng,
    void *rng_userdata,
    uint8_t *strike_count);

/* 保留原接口与 SHORT 语义（field_physical_attack_sequence @0x1e856）。 */
int fd2_field_attack_sequence_count(
    const fd2_field_unit *attacker,
    fd2_field_combat_rng_fn rng,
    void *rng_userdata,
    uint8_t *strike_count);

/* field_counterattack_is_available @0x1f0dc：反击者 +0x26 必须为 0，
 * 双方曼哈顿距离必须为 1，且反击者首件已装备武器最小射程为 1。 */
int fd2_field_attack_counterattack_is_available(
    const fd2_field_unit *counterattacker,
    const fd2_field_unit *target);

/* field_unit_ignores_terrain_combat_modifier @0x1f183：unit 28 强制使用
 * 地形；其他单位 movement profile 19 或 race 4/5 时跳过地形攻防修正。 */
int fd2_field_attack_unit_ignores_terrain_modifier(
    const fd2_field_unit *unit);

/* DS:0x1a12 / DS:0x1a2a 分别是按 movement_cost_class 0..5 查询的
 * 攻击／防御百分比；完整表已由 DOSBox debugger 运行时捕获。 */
int fd2_field_attack_terrain_modifiers(uint8_t movement_cost_class,
                                       int32_t *attack_percent,
                                       int32_t *defense_percent);
int fd2_field_attack_apply_terrain_modifier(uint32_t stat,
                                            int32_t percent,
                                            uint32_t *adjusted);

/* field_physical_attack_resolve @0x1ecc7 从
 * DS:0x24a8[movement_profile-1] 读取基础暴击率。完整 30 字节表已由
 * DOSBox debugger 捕获；profile 0 或 >30 返回 -1。 */
int fd2_field_attack_base_critical_chance(const fd2_field_unit *attacker,
                                          uint8_t *base_chance);

/* field_target_range_build @0x14818 的 profile 0 范围传播核心。
 * 从指定格传播 max_range，并按曼哈顿距离排除小于 min_range 的格子；
 * 不收集 actor，也不修改单位。 */
int fd2_field_target_range_compute(fd2_field_path_result *result,
                                   const fd2_field_map *map,
                                   const fd2_terrain_tileset *terrain,
                                   int origin_x, int origin_y,
                                   uint8_t min_range,
                                   uint8_t max_range);

/* 普通攻击包装：从已装备武器 record +0x0b/+0x0c 读取范围。 */
int fd2_field_attack_range_compute(fd2_field_path_result *result,
                                   const fd2_field_map *map,
                                   const fd2_terrain_tileset *terrain,
                                   const fd2_field_unit *attacker);

typedef enum {
    FD2_FIELD_TARGET_SIDE_ZERO = 0,
    FD2_FIELD_TARGET_SIDE_NONZERO = 1,
    FD2_FIELD_TARGET_SIDE_ONE = 2,
    FD2_FIELD_TARGET_SIDE_TWO = 3
} fd2_field_target_filter;

/* field_target_range_build 的四种 side filter：0→side 0、1→非 0、
 * 2→side 1、3→side 2。返回 1 表示匹配，0 表示不匹配。 */
int fd2_field_attack_target_matches_filter(
    const fd2_field_unit *target, fd2_field_target_filter filter);
int fd2_field_attack_target_is_legal_for_filter(
    const fd2_field_map *map,
    const fd2_terrain_tileset *terrain,
    const fd2_field_units *units,
    size_t attacker_index,
    size_t target_index,
    fd2_field_target_filter filter);

/* 玩家普通攻击固定使用 filter 0，只保留 side 0；玩家 actor 属于 side 2。
 * HP 为 0 的 SDL 单位也不再作为可选目标。 */
int fd2_field_attack_target_is_legal(const fd2_field_map *map,
                                     const fd2_terrain_tileset *terrain,
                                     const fd2_field_units *units,
                                     size_t attacker_index,
                                     size_t target_index);

#endif /* FD2_FIELD_ATTACK_H */
