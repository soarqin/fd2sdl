/* 炎龙骑士团 2 SDL3 重写 - 法术/技能 7 字节静态表
 * 逆向依据：field_magic_record_get @0x4e866 返回 DS:0x19fd + id*7；
 * 原始表由 object3 `DS:0x19fd`（file `0x7aa11`）提取。 */

#include "field_magic.h"

#include <limits.h>

#include "field_attack.h"

static const uint32_t MAGIC_DAMAGE_PROFILE_SCALES
    [FD2_FIELD_MAGIC_DAMAGE_PROFILE_COUNT] = {
    10,10,10,10,7,7,10,10,10,10,9,10,5,5,
    8,10,6,8,10,9,5,5,10,8,8,4,10,7,
};

static const uint8_t MAGIC_RECORDS
    [FD2_FIELD_MAGIC_COUNT][FD2_FIELD_MAGIC_RECORD_SIZE] = {
    {0x32,0x00,0x5a,0x05,0x00,0x02,0x00},
    {0x78,0x00,0x5a,0x05,0x00,0x06,0x00},
    {0xfa,0x00,0x5a,0x05,0x01,0x14,0x00},
    {0xf4,0x01,0x55,0x05,0x01,0x2a,0x00},
    {0x28,0x00,0x55,0x04,0x01,0x04,0x00},
    {0x64,0x00,0x50,0x04,0x01,0x0f,0x00},
    {0xdc,0x00,0x50,0x04,0x02,0x1e,0x00},
    {0xc2,0x01,0x50,0x04,0x02,0x3c,0x00},
    {0xb8,0x01,0x64,0x08,0x00,0x18,0x00},
    {0xe7,0x03,0x32,0x03,0x00,0x1e,0x00},
    {0x50,0x00,0x5f,0x00,0x05,0x12,0x00},
    {0xa0,0x00,0x5a,0x00,0x07,0x2d,0x00},
    {0x54,0x01,0x5a,0x00,0x09,0x50,0x00},
    {0x46,0x00,0x00,0x04,0x00,0x03,0x01},
    {0x8c,0x00,0x00,0x04,0x01,0x0a,0x01},
    {0x04,0x01,0x00,0x05,0x02,0x14,0x01},
    {0xf4,0x01,0x00,0x05,0x03,0x28,0x01},
    {0x00,0x00,0x00,0x04,0x02,0x05,0x01},
    {0x00,0x00,0x00,0x04,0x02,0x05,0x01},
    {0x00,0x00,0x00,0x04,0x02,0x08,0x01},
    {0x00,0x00,0x00,0x04,0x02,0x05,0x01},
    {0x00,0x00,0x00,0x04,0x02,0x05,0x01},
    {0x00,0x00,0x00,0x04,0x02,0x08,0x00},
    {0x00,0x00,0x00,0x03,0x00,0x14,0x03},
    {0x00,0x00,0x00,0x05,0x01,0x16,0x00},
    {0x00,0x00,0x00,0x03,0x01,0x18,0x01},
    {0x0a,0x00,0x32,0x04,0x02,0x08,0x00},
    {0x0a,0x00,0x32,0x04,0x02,0x0a,0x00},
    {0x00,0x00,0x00,0x01,0x00,0x16,0x00},
    {0x00,0x00,0x00,0x00,0x02,0x1a,0x00},
    {0x00,0x00,0x00,0x14,0x00,0x18,0x00},
    {0x00,0x00,0x00,0x00,0x02,0x1a,0x00},
    {0x20,0x03,0x5a,0x05,0x03,0x4c,0x00},
    {0x00,0x00,0x00,0x05,0x03,0x34,0x01},
    {0x00,0x00,0x00,0x05,0x03,0x1c,0x01},
    {0x00,0x00,0x00,0x04,0x02,0x24,0x00},
};

const uint8_t *fd2_field_magic_record_get(uint8_t magic_id) {
    if ((size_t)magic_id >= FD2_FIELD_MAGIC_COUNT) return NULL;
    return MAGIC_RECORDS[magic_id];
}

uint16_t fd2_field_magic_u16(const uint8_t *record, size_t offset) {
    if (!record || offset >= FD2_FIELD_MAGIC_RECORD_SIZE - 1u) return 0;
    return (uint16_t)((uint16_t)record[offset] |
                      ((uint16_t)record[offset + 1u] << 8));
}

int fd2_field_magic_damage_profile_scale(
        uint8_t movement_profile, uint32_t *scale) {
    if (!scale || movement_profile == 0u ||
        movement_profile > FD2_FIELD_MAGIC_DAMAGE_PROFILE_COUNT)
        return -1;
    *scale = MAGIC_DAMAGE_PROFILE_SCALES[movement_profile - 1u];
    return 0;
}

static int apply_damage_value(uint16_t value,
                              uint8_t accuracy,
                              fd2_field_unit *target,
                              fd2_field_magic_rng_fn rng,
                              void *rng_userdata) {
    uint32_t profile_scale = 0;
    if (!target || !rng ||
        fd2_field_magic_damage_profile_scale(
            target->movement_profile, &profile_scale) != 0)
        return -1;
    uint32_t hit_roll = rng(rng_userdata) % 100u;
    if (hit_roll >= accuracy) return 0;
    int32_t signed_power = (int16_t)value;
    int64_t scaled = (int64_t)signed_power * (int64_t)profile_scale / 10;
    if (scaled < 0 || scaled > INT32_MAX) return -1;
    uint32_t power = (uint32_t)scaled;
    uint32_t spread = rng(rng_userdata) % 100u;
    /* 0x1c81f：base*9/10 + (rng%100)*base/1000，两个除法都用
     * signed idiv 并在相加前分别截断。 */
    uint64_t damage = (uint64_t)(power * 9u / 10u) +
                      (uint64_t)(spread * power / 1000u);
    if (damage > target->hp) damage = target->hp;
    target->hp = (uint16_t)(target->hp - (uint16_t)damage);
    return 1;
}

static int apply_damage(uint8_t magic_id,
                        fd2_field_unit *target,
                        fd2_field_magic_rng_fn rng,
                        void *rng_userdata) {
    const uint8_t *record = fd2_field_magic_record_get(magic_id);
    if (!record) return -1;
    /* DS:0x1d01 的 IDs 0..8 wrappers 进入 0x55115 动画；其主体在
     * corrected dual 0x30958 逐目标调用 0x1c75e 并以提交后的 HP 驱动
     * 插值演出。ID 9 的独立 wrapper @0x214ad 也直接进入同一 core。
     * 只有 IDs 10..12
     * 在 0x1c7c6..0x1c7eb 额外调用 0x1f183 并对命中对象直接免疫。
     * 该判定与 magic record +6 的目标阵营 filter 无关。 */
    if (magic_id >= 10u && magic_id <= 12u &&
        fd2_field_attack_unit_ignores_terrain_modifier(target))
        return 0;
    return apply_damage_value(
        fd2_field_magic_u16(record, 0), record[2], target,
        rng, rng_userdata);
}

static int apply_raw_damage(uint32_t power,
                            fd2_field_unit *target,
                            fd2_field_magic_rng_fn rng,
                            void *rng_userdata) {
    if (!target || !rng) return -1;
    uint32_t spread = rng(rng_userdata) % 100u;
    uint64_t damage = (uint64_t)(power * 9u / 10u) +
                      (uint64_t)(spread * power / 1000u);
    if (damage > target->hp) damage = target->hp;
    target->hp = (uint16_t)(target->hp - (uint16_t)damage);
    return 1;
}

static int apply_heal(uint8_t magic_id,
                      fd2_field_unit *target,
                      fd2_field_magic_rng_fn rng,
                      void *rng_userdata) {
    const uint8_t *record = fd2_field_magic_record_get(magic_id);
    if (!target || !record || !rng) return -1;
    /* DS:0x1d01[13..16] wrappers @0x21ad9/0x21b99/0x2211c/0x22153
     * 最终都逐目标调用 0x1c8ed→0x1c916。该 core 直接取 record u16 +0，
     * 不做命中或 profile 免疫；每个目标仅消费一次 spread RNG，即使
     * HP 已满也照常消费，最后把结果 cap 到 hp_max。 */
    uint32_t power = fd2_field_magic_u16(record, 0);
    uint32_t spread = rng(rng_userdata) % 100u;
    uint64_t heal = (uint64_t)(power * 9u / 10u) +
                    (uint64_t)(spread * power / 1000u);
    uint64_t hp = (uint64_t)target->hp + heal;
    target->hp = (uint16_t)(hp > target->hp_max ? target->hp_max : hp);
    return 1;
}

static uint16_t add_status_scaled_stat(uint16_t value) {
    /* DS:0x0210/0x0218 均为 double 0.15。x87 fistp 使用默认 nearest-even。 */
    uint32_t numerator = (uint32_t)value * 15u + 100u;
    uint32_t quotient = numerator / 100u;
    uint32_t remainder = numerator % 100u;
    if (remainder > 50u || (remainder == 50u && (quotient & 1u) != 0u))
        quotient++;
    return (uint16_t)(value + (uint16_t)quotient);
}

int fd2_field_magic_apply_known_effect(
        uint8_t magic_id,
        uint8_t target_unit_index,
        fd2_field_units *units,
        fd2_field_magic_rng_fn rng,
        void *rng_userdata) {
    if (!units || (size_t)target_unit_index >= units->count) return -1;
    fd2_field_unit *target = &units->items[target_unit_index];
    if ((target->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0) return -1;
    if (magic_id <= 12u)
        return apply_damage(magic_id, target, rng, rng_userdata);
    if (magic_id <= 16u)
        return apply_heal(magic_id, target, rng, rng_userdata);
    if (magic_id >= 17u && magic_id <= 19u) {
        /* 0x226ea/0x2282f/0x22960：状态为 0 时直接消费一次 RNG，
         * 把 RNG%4+2 写入 +0x22/+0x23/+0x24；没有 50% gate。 */
        uint8_t *status = magic_id == 17u ? &target->attack_status :
                          magic_id == 18u ? &target->defense_status :
                                            &target->hit_evasion_status;
        if (*status != 0u) return 0;
        if (!rng) return -1;
        *status = (uint8_t)(rng(rng_userdata) % 4u + 2u);
        if (magic_id == 17u) {
            target->attack = add_status_scaled_stat(target->attack);
        } else if (magic_id == 18u) {
            target->defense = add_status_scaled_stat(target->defense);
        } else {
            /* 0x22a47/0x22a4c 是 16 位 add，溢出按原机自然回绕。 */
            target->accuracy = (uint16_t)(target->accuracy + 15u);
            target->evasion = (uint16_t)(target->evasion + 15u);
        }
        return 1;
    }
    if (magic_id == 20u) {
        /* 0x22a85 wrapper 传 offset +0x25 给共享 0x22af6；仅在 byte
         * 非零时清零，不消费 RNG，也不触发属性重算。 */
        if (target->detail_status[0] == 0u) return 0;
        target->detail_status[0] = 0;
        return 1;
    }
    if (magic_id == 21u) {
        /* 0x22bc6 wrapper 同一 core，offset 改为 +0x26。 */
        if (target->detail_status[1] == 0u) return 0;
        target->detail_status[1] = 0;
        return 1;
    }
    if (magic_id == 22u || magic_id == 26u || magic_id == 27u) {
        /* 0x22be1/0x22cbf/0x22e41 仅改变传给共享 0x22cda 的状态
         * offset：ID 22→+0x27、26→+0x25、27→+0x26。三者共用
         * status==0、profile 25/26 免疫、50% gate、固定伤害 10 和
         * duration RNG 顺序。 */
        uint8_t *status = magic_id == 22u ? &target->detail_status[2] :
                          magic_id == 26u ? &target->detail_status[0] :
                                            &target->detail_status[1];
        if (*status != 0u || target->movement_profile == 25u ||
            target->movement_profile == 26u)
            return 0;
        if (!rng || rng(rng_userdata) % 100u >= 50u) return 0;
        /* 0x22dd0：成功 gate 后直接调用 0x1c81f(target,10)，不走
         * profile scale 或 hit roll；随后再消费 RNG 写 duration。 */
        if (apply_raw_damage(10u, target, rng, rng_userdata) < 0)
            return -1;
        *status = (uint8_t)(rng(rng_userdata) % 4u + 2u);
        return 1;
    }
    if (magic_id == 25u) {
        /* 0x22c04：逐目标 test flags&0x80；命中时仅执行 flags&=0x7f，
         * 其他 flag bits 保持，且不消费 RNG。 */
        if ((target->flags & FD2_FIELD_UNIT_FLAG_ACTED) == 0u) return 0;
        target->flags &= (uint8_t)~FD2_FIELD_UNIT_FLAG_ACTED;
        return 1;
    }
    /* IDs 23/24、28..35 的特殊 handler 尚无完整 gameplay 语义。 */
    return -1;
}
