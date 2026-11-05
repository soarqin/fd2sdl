/* 炎龙骑士团 2 SDL3 重写 - 法术/技能 7 字节静态表
 * 逆向依据：field_magic_record_get @0x4e866 返回 DS:0x19fd + id*7；
 * 原始表由 object3 `DS:0x19fd`（file `0x7aa11`）提取；
 * field_unit_magic_list_build @0x1c269（原登记同名）枚举五字节位图；
 * field_player_magic_list_select_step @0x1d51d（原登记sub_1D51D）的按键分派；
 * field_magic_damage_apply @0x1c81f（原登记sub_1C81F）、
 * field_hp_heal_apply @0x1c916（原登记sub_1C916）的HP／原始经验贡献。 */

#include "field_magic.h"

#include <limits.h>

#include "field_attack.h"
#include "field_unit_base.h"

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

/* field_unit_magic_list_build @0x1c269（原登记同名）：
 * 仅按actor+0x1a..0x1e的byte/bit顺序枚举，不代入玩家状态或AI评分门。 */
size_t fd2_field_unit_magic_list_build(
        const fd2_field_unit *unit, uint8_t *magic_ids, size_t capacity) {
    if (!unit) return 0;
    size_t count = 0;
    for (size_t byte_index = 0; byte_index < 5u; byte_index++) {
        uint8_t bits = unit->data_09_1e[0x11u + byte_index];
        for (size_t bit = 0; bit < 8u; bit++) {
            if ((bits & (uint8_t)(1u << bit)) == 0) continue;
            if (magic_ids && count < capacity)
                magic_ids[count] = (uint8_t)(byte_index * 8u + bit);
            count++;
        }
    }
    return count;
}

const uint8_t *fd2_field_magic_record_get(uint8_t magic_id) {
    if ((size_t)magic_id >= FD2_FIELD_MAGIC_COUNT) return NULL;
    return MAGIC_RECORDS[magic_id];
}

int fd2_field_magic_list_select_step(
        const fd2_field_unit *unit, int *selected, fd2_field_magic_list_key key,
        uint8_t *confirmed_id) {
    if (!unit || !selected || !confirmed_id ||
        key < FD2_FIELD_MAGIC_LIST_KEY_OTHER || key > FD2_FIELD_MAGIC_LIST_KEY_CANCEL)
        return -2;
    uint8_t ids[40];
    size_t count = fd2_field_unit_magic_list_build(unit, ids, sizeof(ids));
    int index = *selected;
    if (count == 0 || count > sizeof(ids) || index < 0 || (size_t)index >= count)
        return -2;
    /* 未选中的后项也先验证，不能导航后或确认写ID后才发现未知位。 */
    for (size_t i = 0; i < count; i++) {
        if (!fd2_field_magic_record_get(ids[i])) return -2;
    }
    switch (key) {
        case FD2_FIELD_MAGIC_LIST_KEY_UP:
            *selected = index == 0 ? (int)count - 1 : index - 1;
            break;
        case FD2_FIELD_MAGIC_LIST_KEY_DOWN:
            *selected = (size_t)index + 1u == count ? 0 : index + 1;
            break;
        case FD2_FIELD_MAGIC_LIST_KEY_LEFT:
            if (index >= 4) *selected = index - 4;
            break;
        case FD2_FIELD_MAGIC_LIST_KEY_RIGHT:
            if ((size_t)index + 4u < count) *selected = index + 4;
            break;
        case FD2_FIELD_MAGIC_LIST_KEY_CONFIRM:
            if (unit->mp < fd2_field_magic_record_get(ids[index])[5]) return 0;
            *confirmed_id = ids[index];
            return 1;
        case FD2_FIELD_MAGIC_LIST_KEY_CANCEL:
            return -1;
        case FD2_FIELD_MAGIC_LIST_KEY_OTHER:
            break;
    }
    return 0;
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

static int scaled_damage_power(uint16_t value, uint32_t profile_scale,
                                 uint32_t *power) {
    int64_t scaled = (int64_t)(int16_t)value * (int64_t)profile_scale / 10;
    if (scaled < 0 || scaled > INT32_MAX) return -1;
    *power = (uint32_t)scaled;
    return 0;
}

/* 1c81f／1c916的两次除法各自截断；上界查询以spread99复用同一公式。 */
static uint64_t spread_value(uint32_t power, uint32_t spread) {
    return (uint64_t)power * 9u / 10u + (uint64_t)spread * power / 1000u;
}

static unsigned reward_level(const fd2_field_unit *unit) {
    return unit->level + (unit->movement_profile > 8u && unit->movement_profile < 25u ? 30u : 0u);
}

/* 1c916/1c9dd共享signed差值idiv/ADD DWORD；物品最终清贡献，不能夹负数。 */
static int heal_reward_ready(const fd2_field_unit *unit, int mp) {
    return unit->unit_id >= 75u || (mp ? unit->mp_max : unit->hp_max) != 0;
}

static int heal_apply(fd2_field_unit *target, uint16_t power,
        fd2_field_magic_rng_fn rng, void *userdata, uint32_t *reward, int mp) {
    if (!target || !rng || (reward && !heal_reward_ready(target, mp))) return -1;
    uint16_t before = mp ? target->mp : target->hp;
    uint16_t maximum = mp ? target->mp_max : target->hp_max;
    uint64_t updated = (uint64_t)before + spread_value(power, rng(userdata) % 100u);
    uint16_t after = updated > maximum ? maximum : (uint16_t)updated;
    if (mp) target->mp = after; else target->hp = after;
    if (reward && target->unit_id < 75u) {
        int64_t value = (int64_t)((int32_t)after - before) * 40 *
            (mp ? target->level : reward_level(target)) / maximum;
        *reward += (uint32_t)value;
    }
    return 1;
}

int fd2_field_hp_heal_apply_with_reward(fd2_field_unit *target, uint16_t power,
        fd2_field_magic_rng_fn rng, void *userdata, uint32_t *reward) {
    return heal_apply(target, power, rng, userdata, reward, 0);
}
int fd2_field_mp_heal_apply_with_reward(fd2_field_unit *target, uint16_t power,
        fd2_field_magic_rng_fn rng, void *userdata, uint32_t *reward) {
    return heal_apply(target, power, rng, userdata, reward, 1);
}
int fd2_field_hp_heal_apply(fd2_field_unit *target, uint16_t power,
        fd2_field_magic_rng_fn rng, void *userdata) {
    return heal_apply(target, power, rng, userdata, NULL, 0);
}
int fd2_field_mp_heal_apply(fd2_field_unit *target, uint16_t power,
        fd2_field_magic_rng_fn rng, void *userdata) {
    return heal_apply(target, power, rng, userdata, NULL, 1);
}

static int damage_reward(const fd2_field_unit *target, uint32_t damage,
                          uint32_t *value) {
    *value = 0;
    if (target->unit_id < 68u) return 0;
    uint8_t coefficient;
    if (fd2_field_unit_enemy_experience_coefficient(target->unit_id, &coefficient) != 0)
        return -1;
    uint32_t full = (uint32_t)target->level * coefficient;
    uint64_t product = (uint64_t)damage * full;
    if (target->hp && (!target->hp_max || product > INT32_MAX)) return -1;
    *value = target->hp ? (uint32_t)(product / target->hp_max) : full;
    return 0;
}

static int apply_damage_value(uint16_t value,
                              uint8_t accuracy,
                              fd2_field_unit *target,
                              fd2_field_magic_rng_fn rng,
                              void *rng_userdata,
                              uint32_t *raw_damage, uint32_t *reward) {
    uint32_t profile_scale = 0;
    if (!target || !rng ||
        fd2_field_magic_damage_profile_scale(
            target->movement_profile, &profile_scale) != 0)
        return -1;
    uint32_t hit_roll = rng(rng_userdata) % 100u;
    if (hit_roll >= accuracy) return 0;
    uint32_t power;
    if (scaled_damage_power(value, profile_scale, &power) != 0) return -1;
    uint32_t spread = rng(rng_userdata) % 100u;
    /* 0x1c81f：base*9/10 + (rng%100)*base/1000，两个除法都用
     * signed idiv 并在相加前分别截断。 */
    uint64_t damage = spread_value(power, spread);
    if (raw_damage) *raw_damage = (uint32_t)damage;
    if (damage > target->hp) damage = target->hp;
    target->hp = (uint16_t)(target->hp - (uint16_t)damage);
    if (reward) {
        uint32_t value;
        if (damage_reward(target, (uint32_t)spread_value(power, spread), &value) != 0) return -1;
        *reward += value;
    }
    return 1;
}

static int apply_damage(uint8_t magic_id,
                        fd2_field_unit *target,
                        fd2_field_magic_rng_fn rng,
                        void *rng_userdata, uint32_t *reward) {
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
        rng, rng_userdata, NULL, reward);
}

static int apply_raw_damage(uint32_t power, fd2_field_unit *target,
        fd2_field_magic_rng_fn rng, void *userdata, uint32_t *reward) {
    if (!target || !rng) return -1;
    uint32_t damage = (uint32_t)spread_value(power, rng(userdata) % 100u);
    target->hp = damage >= target->hp ? 0 : (uint16_t)(target->hp - damage);
    if (reward) {
        uint32_t value;
        if (damage_reward(target, damage, &value) != 0) return -1;
        *reward += value;
    }
    return 1;
}

static int apply_heal(uint8_t magic_id,
                      fd2_field_unit *target,
                      fd2_field_magic_rng_fn rng,
                      void *rng_userdata, uint32_t *reward) {
    const uint8_t *record = fd2_field_magic_record_get(magic_id);
    if (!target || !record || !rng) return -1;
    /* DS:0x1d01[13..16] wrappers @0x21ad9/0x21b99/0x2211c/0x22153
     * 最终都逐目标调用 0x1c8ed→0x1c916。该 core 直接取 record u16 +0，
     * 不做命中或 profile 免疫；每个目标仅消费一次 spread RNG，即使
     * HP 已满也照常消费，最后把结果 cap 到 hp_max。 */
    return fd2_field_hp_heal_apply_with_reward(target, fd2_field_magic_u16(record, 0),
                                   rng, rng_userdata, reward);
}

/* sub_1C81F／sub_1C916（原登记同名）：damage用未夹HP的rawDamage，
 * heal用实际HP增量。目标等级权重在这里，caster权重只在最终归一处。 */
static int basic_experience_value(uint8_t magic_id,
                                   const fd2_field_unit *before,
                                   uint16_t hp_after, uint32_t raw_damage,
                                   uint32_t *value) {
    uint64_t experience = 0;
    if (magic_id == 0 && before->unit_id >= 68u) {
        uint8_t coefficient;
        if (fd2_field_unit_enemy_experience_coefficient(
                before->unit_id, &coefficient) != 0) return -1;
        uint32_t full = (uint32_t)before->level * coefficient;
        experience = hp_after == 0 ? full :
            (uint64_t)raw_damage * full / before->hp_max;
    } else if (magic_id == 13 && before->unit_id < 75u) {
        unsigned level = before->level;
        if (before->movement_profile > 8u && before->movement_profile < 25u)
            level += 30u;
        int64_t change = (int64_t)((int32_t)hp_after - before->hp) * 40 * level / before->hp_max;
        *value = (uint32_t)change;
        return 0;
    }
    if (experience > UINT32_MAX) return -1;
    *value = (uint32_t)experience;
    return 0;
}

int fd2_field_magic_basic_reward_max(
        uint8_t magic_id, const fd2_field_unit *target, uint32_t *value) {
    if (!target || !value || (magic_id != 0 && magic_id != 13) ||
        (target->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) ||
        target->hp_max == 0 || target->hp > target->hp_max)
        return -1;
    const uint8_t *record = fd2_field_magic_record_get(magic_id);
    uint32_t power = fd2_field_magic_u16(record, 0);
    uint32_t damage = 0;
    uint16_t hp_after;
    if (magic_id == 0) {
        uint32_t scale;
        if (fd2_field_magic_damage_profile_scale(target->movement_profile, &scale) != 0 ||
            scaled_damage_power((uint16_t)power, scale, &power) != 0)
            return -1;
        damage = (uint32_t)spread_value(power, 99);
        hp_after = damage >= target->hp ? 0 : (uint16_t)(target->hp - damage);
    } else {
        uint64_t hp = (uint64_t)target->hp + spread_value(power, 99);
        hp_after = hp > target->hp_max ? target->hp_max : (uint16_t)hp;
    }
    return basic_experience_value(magic_id, target, hp_after, damage, value);
}

int fd2_field_magic_apply_player_basic_effect(
        uint8_t magic_id, fd2_field_unit *target,
        fd2_field_magic_rng_fn rng, void *rng_userdata,
        fd2_field_magic_basic_result *result) {
    uint32_t maximum;
    if (!rng || !result ||
        fd2_field_magic_basic_reward_max(magic_id, target, &maximum) != 0)
        return -1;
    fd2_field_unit before = *target;
    fd2_field_unit updated = before;
    fd2_field_magic_basic_result effect = {0};
    effect.hp_before = before.hp;
    int hit;
    if (magic_id == 0) {
        const uint8_t *record = fd2_field_magic_record_get(magic_id);
        hit = apply_damage_value(fd2_field_magic_u16(record, 0), record[2],
            &updated, rng, rng_userdata, &effect.raw_damage, NULL);
    } else {
        hit = apply_heal(magic_id, &updated, rng, rng_userdata, NULL);
    }
    if (hit < 0) return -1;
    effect.hit = (uint8_t)hit;
    effect.hp_after = updated.hp;
    if (hit && basic_experience_value(magic_id, &before, updated.hp,
                effect.raw_damage, &effect.experience_value) != 0)
        return -1;
    *target = updated;
    *result = effect;
    return hit;
}

int fd2_field_magic_reward_normalize(
        const fd2_field_unit *caster, uint64_t raw_sum, uint32_t *value) {
    if (!caster || !value || raw_sum > INT32_MAX) return -1;
    unsigned divisor = caster->level + (caster->movement_profile > 8u ? 30u : 0u);
    if (divisor == 0) return -1;
    *value = (uint32_t)(raw_sum / divisor);
    return 0;
}

static uint16_t add_status_scaled_stat(uint16_t value) {
    /* 227db/22920→__CHP @0x37af4：原CW127f的两块原码均已全u16域
     * 验证等价此整数floor；勿借1b750的独立1.15 nearest-even重算。 */
    return (uint16_t)(value + (uint16_t)(((uint32_t)value * 15u + 100u) / 100u));
}

int fd2_field_magic_apply_known_effect_with_reward(
        uint8_t magic_id,
        uint8_t target_unit_index,
        fd2_field_units *units,
        fd2_field_magic_rng_fn rng,
        void *rng_userdata, uint32_t *reward) {
    if (!units || (size_t)target_unit_index >= units->count) return -1;
    fd2_field_unit *target = &units->items[target_unit_index];
    if ((target->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0) return -1;
    if (reward && fd2_field_magic_effect_reward_preflight(magic_id, target) != 0) return -1;
    if (magic_id <= 12u)
        return apply_damage(magic_id, target, rng, rng_userdata, reward);
    if (magic_id <= 16u)
        return apply_heal(magic_id, target, rng, rng_userdata, reward);
    if (magic_id <= 19u) {
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
        if (reward) *reward += 2u * reward_level(target);
        return 1;
    }
    if (magic_id == 20u) {
        /* 22a85→22af6：状态非零先1c916(power10)，再清状态；
         * HP满亦耗一次spread RNG；无状态时不恢复、不消费。 */
        if (target->detail_status[0] == 0u) return 0;
        if (fd2_field_hp_heal_apply_with_reward(target, 10u, rng, rng_userdata, reward) < 0) return -1;
        target->detail_status[0] = 0;
        if (reward) *reward += 4u * reward_level(target);
        return 1;
    }
    if (magic_id == 21u) {
        /* 0x22bc6 wrapper 同一 core，offset 改为 +0x26。 */
        if (target->detail_status[1] == 0u) return 0;
        if (fd2_field_hp_heal_apply_with_reward(target, 10u, rng, rng_userdata, reward) < 0) return -1;
        target->detail_status[1] = 0;
        if (reward) *reward += 4u * reward_level(target);
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
        if (!rng) return -1;
        if (rng(rng_userdata) % 100u >= 50u) return 0;
        /* 0x22dd0：成功 gate 后直接调用 0x1c81f(target,10)，不走
         * profile scale 或 hit roll；随后再消费 RNG 写 duration。 */
        if (apply_raw_damage(10u, target, rng, rng_userdata, reward) < 0)
            return -1;
        *status = (uint8_t)(rng(rng_userdata) % 4u + 2u);
        if (reward) *reward += 8u * target->level;
        return 1;
    }
    if (magic_id == 25u) {
        /* 0x22c04：逐目标 test flags&0x80；命中时仅执行 flags&=0x7f，
         * 其他 flag bits 保持，且不消费 RNG。 */
        if ((target->flags & FD2_FIELD_UNIT_FLAG_ACTED) == 0u) return 0;
        target->flags &= (uint8_t)~FD2_FIELD_UNIT_FLAG_ACTED;
        if (reward) *reward += 8u * reward_level(target);
        return 1;
    }
    /* IDs 23/24、28..35 的特殊 handler 尚无完整 gameplay 语义。 */
    return -1;
}

int fd2_field_magic_apply_known_effect(uint8_t id, uint8_t index,
        fd2_field_units *units, fd2_field_magic_rng_fn rng, void *userdata) {
    return fd2_field_magic_apply_known_effect_with_reward(id, index, units, rng, userdata, NULL);
}

/* RNG前只核本分支实际会引用的贡献除数/敌方系数；免疫/已有状态不假查。 */
int fd2_field_magic_effect_reward_preflight(uint8_t id, const fd2_field_unit *unit) {
    if (!unit || (unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN)) return -1;
    int damage = id <= 12u;
    if (id >= 10u && id <= 12u && fd2_field_attack_unit_ignores_terrain_modifier(unit)) return 0;
    if (id == 22u || id == 26u || id == 27u) {
        uint8_t status = unit->detail_status[id == 22u ? 2 : id == 26u ? 0 : 1];
        if (status || unit->movement_profile == 25u || unit->movement_profile == 26u) return 0;
        damage = 1;
    }
    if (damage) {
        uint32_t power = 10, scale;
        if (id <= 12u && (fd2_field_magic_damage_profile_scale(unit->movement_profile, &scale) != 0 ||
            scaled_damage_power(fd2_field_magic_u16(fd2_field_magic_record_get(id), 0), scale, &power) != 0)) return -1;
        if (unit->unit_id >= 68u) {
            uint8_t coefficient;
            if (fd2_field_unit_enemy_experience_coefficient(unit->unit_id, &coefficient) != 0 || !unit->hp_max) return -1;
            if (spread_value(power, 99u) * unit->level * coefficient > INT32_MAX) return -1;
        }
        return 0;
    }
    if (id >= 13u && id <= 16u) return heal_reward_ready(unit, 0) ? 0 : -1;
    if (id == 20u || id == 21u)
        return !unit->detail_status[id - 20u] || heal_reward_ready(unit, 0) ? 0 : -1;
    return (id >= 17u && id <= 19u) || id == 25u ? 0 : -1;
}
