#ifndef FD2_FIELD_UNIT_STATS_H
#define FD2_FIELD_UNIT_STATS_H

#include <stdint.h>

#include "field_unit.h"

/* 原版装备记录由 8 个两字节槽组成；slot flag bit 0x40 表示已装备。 */
#define FD2_FIELD_UNIT_EQUIPMENT_SLOT_COUNT 8
#define FD2_FIELD_UNIT_EQUIPPED_FLAG 0x40u

typedef struct {
    int16_t attack;
    int16_t accuracy;
    int16_t defense;
    int16_t evasion;
} fd2_field_item_stat_effect;

typedef enum {
    FD2_FIELD_UNIT_STAT_ATTACK = 0,
    FD2_FIELD_UNIT_STAT_DEFENSE = 1,
} fd2_field_unit_scaled_stat;

/* 返回 0 并填充 effect 表示成功；其他返回值使重算失败且不改写单位。 */
typedef int (*fd2_field_item_stat_lookup_fn)(void *userdata,
                                             uint8_t item_id,
                                             fd2_field_item_stat_effect *effect);

/* record[0x22]/[0x23] 非零时，原版会对攻击/防御执行 x87 比例修正。
 * 该比例和舍入边界尚未完整提取，由上层注入已确认实现。 */
typedef int (*fd2_field_unit_stat_scale_fn)(void *userdata,
                                            fd2_field_unit_scaled_stat stat,
                                            int32_t value,
                                            int32_t *scaled_value);

/* 复现 field_unit_combat_stats_recompute @0x4096e（entry 0x40964）
 *（field_unit_combat_stats_recompute）的已确认派生链：
 *
 * - 基础 attack/defense/accuracy 来自 record 0x37/0x39/0x3e；
 * - record 0x24 非零时，accuracy 与 evasion 的共同基础值加 15；
 * - 8 个装备槽位于 0x0a..0x19，装备效果分别累加四项属性；
 * - 最终写入 record 0x48/0x4a/0x4c/0x4e。
 *
 * 若已装备槽存在而 lookup 为 NULL，或状态缩放需要 scale 但未提供，
 * 返回 -1。失败时四个派生字段保持不变。原始未确认字节不改写。 */
int fd2_field_unit_combat_stats_recompute(
    fd2_field_unit *unit,
    fd2_field_item_stat_lookup_fn lookup,
    void *lookup_userdata,
    fd2_field_unit_stat_scale_fn scale,
    void *scale_userdata);

#endif /* FD2_FIELD_UNIT_STATS_H */
