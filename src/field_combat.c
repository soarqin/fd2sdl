/* 炎龙骑士团 2 SDL3 重写 - 普通物理攻击确定性核心
 *
 * 复现 field_physical_attack_resolve @0x43edb 已确认的命中、暴击、
 * 伤害及 HP 钳制路径；地形、装备特效与暴击率来源由调用方处理。
 */
#include "field_combat.h"

#include <string.h>

int fd2_field_combat_resolve_attack(
    const fd2_field_attack_params *params,
    fd2_field_combat_rng_fn rng,
    void *rng_userdata,
    fd2_field_attack_result *out) {
    if (!params || !rng || !out ||
        params->attack > FD2_FIELD_COMBAT_EFFECTIVE_STAT_MAX ||
        params->defense > FD2_FIELD_COMBAT_EFFECTIVE_STAT_MAX ||
        params->accuracy > FD2_FIELD_COMBAT_ACCURACY_MAX ||
        params->evasion > FD2_FIELD_COMBAT_ACCURACY_MAX ||
        params->critical_chance > UINT8_MAX)
        return -1;

    memset(out, 0, sizeof(*out));
    out->critical_roll = FD2_FIELD_COMBAT_ROLL_UNUSED;
    out->spread_roll = FD2_FIELD_COMBAT_ROLL_UNUSED;
    out->hp_after = params->defender_hp;
    out->defeated = params->defender_hp == 0;

    /* field_physical_attack_resolve @0x43edb：
     * rand()%100 < accuracy-evasion。差值为负时必定未命中。 */
    out->hit_roll = rng(rng_userdata) % 100u;
    int64_t hit_chance =
        (int64_t)params->accuracy - (int64_t)params->evasion;
    if ((int64_t)out->hit_roll >= hit_chance) return 0;
    out->hit = 1;

    out->critical_roll = rng(rng_userdata) % 100u;
    out->critical = out->critical_roll < params->critical_chance;
    uint32_t defense = params->defense;
    if (out->critical) defense /= 2u;

    if (params->attack > defense) {
        out->base_damage = (uint32_t)(
            ((uint64_t)params->attack - (uint64_t)defense) * 9u / 10u);
    }

    uint32_t damage = out->base_damage;
    uint32_t spread = damage / 9u;
    if (spread != 0) {
        out->spread_roll = rng(rng_userdata) % spread;
        damage += out->spread_roll;
    }

    /* 原函数将负 HP 钳制为 0。对外结果同时限制在当前 HP，避免
     * uint16_t 下溢，也让演出层可以直接使用实际扣减值。 */
    out->rolled_damage = damage;
    if (damage > params->defender_hp) damage = params->defender_hp;
    out->damage = (uint16_t)damage;
    out->hp_after = (uint16_t)(params->defender_hp - out->damage);
    out->defeated = out->hp_after == 0;
    return 0;
}
