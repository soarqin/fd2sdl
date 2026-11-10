/* 炎龙骑士团 2 SDL3 重写 - 通用单位战斗属性派生
 *
 * 逆向依据：field_unit_combat_stats_recompute @0x1b750（entry 0x40964）
 *（field_unit_combat_stats_recompute）读取
 * DAT_00003a45 的 0x50 字节单位记录、8 个装备槽和装备表，并写入
 * 0x48/0x4a/0x4c；DOSBox CPU trace 进一步确认共享尾部写入 0x4e。
 */

#include "field_unit_stats.h"
#include "field_item.h"

#include <limits.h>
#include <string.h>

int fd2_field_unit_original_item_stats(void *userdata, uint8_t item_id,
                                      fd2_field_item_stat_effect *effect) {
    (void)userdata;
    if (!effect) return -1;
    const uint8_t *record = fd2_field_item_record_get(item_id);
    if (!record) return -1;
    effect->attack = fd2_field_item_i16(record, 1);
    effect->accuracy = fd2_field_item_i16(record, 3);
    effect->defense = fd2_field_item_i16(record, 5);
    effect->evasion = fd2_field_item_i16(record, 7);
    return 0;
}

int fd2_field_unit_original_status_scale(void *userdata,
                                        fd2_field_unit_scaled_stat stat,
                                        int32_t value, int32_t *scaled_value) {
    (void)userdata;
    (void)stat;
    if (!scaled_value) return -1;
    /* field_unit_combat_stats_recompute @0x1b750（原登记同名）的两段
     * 1b7ef/1b80a：原CW127f乘binary64 1.15后，__CHP临时向0截断。
     * 原码全域[-65536,65535]已对照；volatile固定乘积53bit舍入，勿用115/100。 */
    volatile double product = (double)value * 1.15;
    if (product < INT32_MIN || product > INT32_MAX) return -1;
    *scaled_value = (int32_t)product;
    return 0;
}

int fd2_field_unit_combat_stats_recompute_original(fd2_field_unit *unit) {
    return fd2_field_unit_combat_stats_recompute(unit,
        fd2_field_unit_original_item_stats, NULL, fd2_field_unit_original_status_scale, NULL);
}

static int16_t read_i16(const uint8_t *record, size_t offset) {
    int16_t value;
    memcpy(&value, record + offset, sizeof(value));
    return value;
}

int fd2_field_unit_combat_stats_recompute(
    fd2_field_unit *unit,
    fd2_field_item_stat_lookup_fn lookup,
    void *lookup_userdata,
    fd2_field_unit_stat_scale_fn scale,
    void *scale_userdata) {
    if (!unit) return -1;

    const uint8_t *record = (const uint8_t *)unit;
    int32_t attack = read_i16(record, 0x37);
    int32_t defense = read_i16(record, 0x39);
    int32_t accuracy = read_i16(record, 0x3e);
    if (record[0x24] != 0) accuracy += 15;
    int32_t evasion = accuracy;

    for (size_t i = 0; i < FD2_FIELD_UNIT_EQUIPMENT_SLOT_COUNT; i++) {
        size_t slot = 0x0a + i * 2;
        if ((record[slot] & FD2_FIELD_UNIT_EQUIPPED_FLAG) == 0)
            continue;
        if (!lookup) return -1;

        fd2_field_item_stat_effect effect;
        if (lookup(lookup_userdata, record[slot + 1], &effect) != 0)
            return -1;
        attack += effect.attack;
        defense += effect.defense;
        accuracy += effect.accuracy;
        evasion += effect.evasion;
    }

    if (record[0x22] != 0) {
        int32_t scaled;
        if (!scale || scale(scale_userdata, FD2_FIELD_UNIT_STAT_ATTACK,
                            attack, &scaled) != 0)
            return -1;
        attack = scaled;
    }
    if (record[0x23] != 0) {
        int32_t scaled;
        if (!scale || scale(scale_userdata, FD2_FIELD_UNIT_STAT_DEFENSE,
                            defense, &scaled) != 0)
            return -1;
        defense = scaled;
    }

    /* 原版最终使用 16 位 mov 写回；保留低 16 位。所有输入相加仍在
     * int32_t 的可表示范围内，因此这里不依赖有符号溢出。 */
    unit->attack = (uint16_t)attack;
    unit->defense = (uint16_t)defense;
    unit->accuracy = (uint16_t)accuracy;
    unit->evasion = (uint16_t)evasion;
    return 0;
}
