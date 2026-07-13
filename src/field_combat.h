#ifndef FD2_FIELD_COMBAT_H
#define FD2_FIELD_COMBAT_H

#include <stdint.h>

/* 普通攻击的无演出确定性核心。
 *
 * 逆向依据：field_physical_attack_resolve @0x43edb。原函数先完成装备、
 * 地形和特效修正，再以 accuracy-evasion 判定命中，以 critical_chance
 * 判定防御减半，最后按 (attack-defense)*9/10 加有限随机浮动计算伤害。
 * 本模块只接收已经修正后的数值，不猜测尚未确认的装备和地形规则。
 */
typedef uint32_t (*fd2_field_combat_rng_fn)(void *userdata);

/* 在该范围内与原函数的 32 位中间运算一致，且不会触发无定义或
 * 环绕语义。战场正常派生值远低于此上限。 */
#define FD2_FIELD_COMBAT_EFFECTIVE_STAT_MAX (UINT32_MAX / 9u)
#define FD2_FIELD_COMBAT_ACCURACY_MAX INT32_MAX

typedef struct {
    uint32_t attack;
    uint32_t defense;
    uint32_t accuracy;
    uint32_t evasion;
    uint32_t critical_chance;
    uint16_t defender_hp;
} fd2_field_attack_params;

typedef struct {
    uint32_t hit_roll;
    uint32_t critical_roll;
    uint32_t spread_roll;
    uint32_t base_damage;
    uint32_t rolled_damage;    /* 原公式结果，可能大于目标当前 HP */
    uint16_t damage;           /* 实际 HP 扣减，钳制到当前 HP */
    uint16_t hp_after;
    uint8_t hit;
    uint8_t critical;
    uint8_t defeated;
} fd2_field_attack_result;

#define FD2_FIELD_COMBAT_ROLL_UNUSED UINT32_MAX

/* rng 每次返回任意 uint32_t；函数在对应判定点应用原版模数。
 * 本函数从命中判定开始。武器特效可能在原函数中先消耗随机数，调用方
 * 必须先完成该外围步骤，再把同一 RNG 流传入此处。
 *
 * attack/defense 不得超过 FD2_FIELD_COMBAT_EFFECTIVE_STAT_MAX，
 * accuracy/evasion 不得超过 FD2_FIELD_COMBAT_ACCURACY_MAX，
 * critical_chance 不得超过 UINT8_MAX。返回 0 表示成功；空参数、空 RNG
 * 或超出可精确模拟范围时返回 -1。 */
int fd2_field_combat_resolve_attack(
    const fd2_field_attack_params *params,
    fd2_field_combat_rng_fn rng,
    void *rng_userdata,
    fd2_field_attack_result *out);

#endif /* FD2_FIELD_COMBAT_H */
