/* 炎龙骑士团 2 SDL3 重写 - 通用单位基础属性派生
 *
 * 逆向依据：field_unit_stage_template_append @0x10c50（原登记同名；
 * 历史旧地址0x35e6e）按 unit ID 选择两类静态表：ID < 0x44 使用24字节基础
 * record + 11 字节成长 record；ID >= 0x44 使用 10 字节敌军 record。
 * 函数写入 0x1f/0x20/0x21、0x37/0x39/0x3e、0x3b 和 HP/MP 后调用
 * field_unit_combat_stats_recompute @0x1b750（原登记同名，
 * 入口旧别名field_unit_combat_stats_recompute_entry）。
 * 敌军表／经验系数复用field_enemy_base_record_get @0x4e84f（原登记sub_4E84F）。
 * G1确认field_promotion_record_get @0x4e7dd（原sub_4E7DD）、
 * field_promotion_candidates_build @0x2ae0e（原sub_2AE0E）与
 * field_promotion_growth_execute @0x2ac7d（原sub_2AC7D）：转职增量不是普通构造。
 * stage32的48/66复用相邻原成长/准备字节的已证24B窗口，
 * 不将相邻成长/准备字节命名为另一张基础表。见common-battle-semantics.md。
 */

#include "field_unit_base.h"

#include <stddef.h>
#include <limits.h>
#include <string.h>

#include "field_item.h"
#include "field_unit_stats.h"
#include "field_reward_flow.h"

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

/* 基础、初始库存和创建成长共用原始记录，避免重复维护字段投影。 */
#include "static_data/unit_base.inc"

_Static_assert(ARRAY_COUNT(k_player_base_records) == 32u &&
               sizeof(k_player_base_records[0]) == 24u &&
               ARRAY_COUNT(k_enemy_records) == 68u && sizeof(k_enemy_records[0]) == 10u &&
               ARRAY_COUNT(k_growth_records) == 68u && sizeof(k_growth_records[0]) == 11u &&
               ARRAY_COUNT(k_learning_records) == 20u && sizeof(k_learning_records[0]) == 12u,
               "unit definition layout must match the supported original records");

int fd2_field_unit_enemy_experience_coefficient(
        uint8_t unit_id, uint8_t *coefficient) {
    if (!coefficient || unit_id < 68u ||
        (size_t)(unit_id - 68u) >= ARRAY_COUNT(k_enemy_records))
        return -1;
    *coefficient = k_enemy_records[unit_id - 68u][9];
    return 0;
}

static uint16_t rd_u16(const uint8_t *record, size_t offset) {
    return (uint16_t)(record[offset] | (uint16_t)record[offset + 1u] << 8);
}

static void wr_u16(uint8_t *record, size_t offset, uint32_t value) {
    uint16_t narrowed = (uint16_t)value;
    record[offset] = (uint8_t)narrowed;
    record[offset + 1u] = (uint8_t)(narrowed >> 8);
}

static int initial_item_stats(void *userdata, uint8_t item_id,
                               fd2_field_item_stat_effect *effect) {
    (void)userdata;
    const uint8_t *item = fd2_field_item_record_get(item_id);
    if (!item || !effect) return -1;
    effect->attack = fd2_field_item_i16(item, 1);
    effect->accuracy = fd2_field_item_i16(item, 3);
    effect->defense = fd2_field_item_i16(item, 5);
    effect->evasion = fd2_field_item_i16(item, 7);
    return 0;
}

const uint8_t *fd2_field_unit_growth_record_get(uint8_t unit_id) {
    return unit_id < ARRAY_COUNT(k_growth_records)
        ? k_growth_records[unit_id] : NULL;
}

const uint8_t *fd2_field_unit_learning_record_get(uint8_t selector) {
    return selector < ARRAY_COUNT(k_learning_records)
        ? k_learning_records[selector] : NULL;
}

/* sub_4E838 @0x4e838（原登记同名）基础getter的实际构造读域。
 * 48/66的窗口身份与调用域保存在冷alias视图；不开放其它32..67新建。 */
const uint8_t *fd2_field_unit_base_record_get(uint8_t unit_id) {
    if (unit_id < ARRAY_COUNT(k_player_base_records))
        return k_player_base_records[unit_id];
    for (size_t i = 0; i < ARRAY_COUNT(k_player_base_alias_views); i++) {
        const uint16_t *view = k_player_base_alias_views[i];
        if (view[0] != unit_id) continue;
        return k_player_base_alias_records[i];
    }
    return NULL;
}

int fd2_field_unit_level_growth_prepare(
        const fd2_field_unit *unit, fd2_field_unit_level_growth *result) {
    if (!unit || !result || unit->unit_id >= ARRAY_COUNT(k_growth_records))
        return -1;
    const uint8_t *record = fd2_field_unit_growth_record_get(unit->unit_id);
    fd2_field_unit_level_growth prepared = {0};
    prepared.level = (uint8_t)(unit->level + 1u);
    memset(prepared.learned_magic, 0xff, sizeof(prepared.learned_magic));
    /* field_growth_stat_present_apply @0x1e529（原sub_1E529）的指令VA0x1e548原字节EB0E
     * 跳过随机块，直接取每对第二byte；不能调用创建等级公式。 */
    for (size_t i = 0; i < ARRAY_COUNT(prepared.growth); i++)
        prepared.growth[i] = record[i * 2u + 1u];
    if (record[10] != 0xffu) {
        const uint8_t *learning = fd2_field_unit_learning_record_get(record[10]);
        if (!learning) return -1;
        for (size_t i = 0; i < 6u; i++) {
            if (learning[i * 2u] != prepared.level) continue;
            uint8_t magic_id = learning[i * 2u + 1u];
            /* 只支持原表已证36项，不把ff/未知ID写出原5B位图。 */
            if (magic_id >= 36u) return -1;
            prepared.learned_magic[prepared.learned_count++] = magic_id;
        }
    }
    *result = prepared;
    return 0;
}

int fd2_field_unit_reward_preview(
        const fd2_field_unit *source, uint32_t gain,
        fd2_field_unit_stat_scale_fn scale, void *scale_userdata,
        fd2_field_unit *out) {
    if (!source || !out) return -1;
    fd2_field_unit updated = *source;
    const fd2_static_flow *flow = fd2_reward_graph();
    if (!flow) return -1;
    fd2_reward_cursor cursor = {0};
    cursor.step = flow->entry;
    fd2_reward_notice notice;
    for (unsigned budget = 0; budget < FD2_FLOW_STEP_BUDGET; budget++) {
        int result = fd2_reward_advance(flow, &updated, gain, &cursor, &notice,
            initial_item_stats, NULL, scale, scale_userdata);
        if (result == 2) return 0;
        if (result < 0) return -1;
        if (result == 1) { *out = updated; return 1; }
        if (fd2_reward_ack(flow, &cursor) != 0) return -1;
    }
    return -1;
}

/* field_persistent_unit_combat_stats_recompute @0x1145a（原sub_1145A）不读战场状态22..24，只按flags40累计。
 * 复用同一装备公式；安全int32和的最终WORD等价原逐WORD回绕。 */
int fd2_field_unit_persistent_stats_recompute(fd2_field_unit *unit) {
    if (!unit) return -1;
    fd2_field_unit updated = *unit;
    uint8_t *record = (uint8_t *)&updated;
    memset(record + 0x22, 0, 3);
    if (fd2_field_unit_combat_stats_recompute(
            &updated, initial_item_stats, NULL, NULL, NULL) != 0)
        return -1;
    memcpy((uint8_t *)unit + 0x48, record + 0x48, 8);
    return 0;
}

/* sub_112A5 @0x112a5 与 field_persistent_unit_combat_stats_recompute @0x1145a（原sub_1145A）：
 * 初始持久玩家只写58字节，装备派生后一次提交；保留其余raw字节。 */
int fd2_field_unit_persistent_create(fd2_field_unit *unit, uint8_t unit_id) {
    if (!unit || unit_id >= ARRAY_COUNT(k_player_base_records)) return -1;
    const uint8_t *initial = k_player_base_records[unit_id];
    fd2_field_unit updated = *unit;
    fd2_field_unit derived = *unit;
    derived.unit_id = unit_id;
    if (fd2_field_unit_base_apply(&derived, initial[2]) != 0) return -1;
    uint8_t *record = (uint8_t *)&updated;
    const uint8_t *base = (const uint8_t *)&derived;
    record[5] = 0;
    record[6] = 2;
    record[7] = record[8] = unit_id;
    record[9] = 0;
    record[10] = record[12] = 0x40;
    record[11] = initial[12];
    record[13] = initial[13];
    for (size_t i = 0; i < 4; i++) {
        record[14 + i * 2u] = initial[14 + i] == 0xff ? 0x80 : 0;
        record[15 + i * 2u] = initial[14 + i];
    }
    record[22] = record[24] = 0x80;
    memcpy(record + 26, initial + 8, 4);
    record[30] = 0;
    memcpy(record + 31, base + 31, 3);
    memset(record + 34, 0, 6);
    record[49] = 0xff;
    memcpy(record + 55, base + 55, 5);
    record[60] = 0;
    memcpy(record + 62, base + 62, 10);
    if (fd2_field_unit_persistent_stats_recompute(&updated) != 0)
        return -1;
    *unit = updated;
    return 0;
}

int fd2_field_unit_stage_template_apply(
        fd2_field_unit *unit,
        const fd2_field_unit_template *template_record) {
    if (!unit || !template_record) return -1;

    fd2_field_unit updated;
    memset(&updated, 0, sizeof(updated));
    updated.x = unit->x;
    updated.y = unit->y;
    const uint8_t *tpl = template_record->bytes;
    uint8_t *record = (uint8_t *)&updated;
    updated.side = tpl[0x00];
    updated.unit_id = tpl[0x01];
    updated.text_id = tpl[0x01];

    if (tpl[0x05] != 0xffu) {
        record[0x0a] = 0x40;
        record[0x0b] = tpl[0x05];
        record[0x0c] = 0x40;
        record[0x0d] = tpl[0x06];
    } else {
        record[0x0a] = 0x40;
        record[0x0b] = tpl[0x06];
        record[0x0c] = 0x80;
    }
    for (size_t i = 0; i < 6; i++) {
        uint8_t item = tpl[0x07 + i];
        record[0x0e + i * 2u] = item == 0xffu ? 0x80 : 0;
        record[0x0f + i * 2u] = item;
    }
    memcpy(record + 0x1a, tpl + 0x0d, 4);
    record[0x31] = tpl[0x16];
    record[0x32] = tpl[0x17];
    record[0x33] = tpl[0x18];
    record[0x34] = tpl[0x11];
    record[0x35] = tpl[0x12];
    record[0x36] = tpl[0x13];
    record[0x3c] = updated.side == 2 ? 0 : 0xff;
    record[0x3d] = tpl[0x02];

    if (fd2_field_unit_base_apply(&updated, tpl[0x04]) != 0 ||
        fd2_field_unit_combat_stats_recompute(&updated,initial_item_stats,NULL,NULL,NULL) != 0)
        return -1;
    *unit = updated;
    return 0;
}

int fd2_field_unit_base_apply(fd2_field_unit *unit, uint8_t level) {
    if (!unit || level == 0) return -1;

    fd2_field_unit updated = *unit;
    uint8_t *record = (uint8_t *)&updated;
    uint32_t hp;
    uint32_t mp;
    uint32_t attack;
    uint32_t defense;
    uint32_t accuracy;

    const uint8_t *player_base = fd2_field_unit_base_record_get(updated.unit_id);
    if (player_base && updated.unit_id < ARRAY_COUNT(k_growth_records)) {
        const uint8_t *growth = k_growth_records[updated.unit_id];
        uint32_t level_minus_one = (uint32_t)level - 1u;
        hp = rd_u16(player_base, 3) + (uint32_t)growth[6] * level_minus_one;
        mp = rd_u16(player_base, 5) + (uint32_t)growth[8] * level_minus_one;
        attack = rd_u16(player_base, 18) + (uint32_t)growth[0] * level;
        defense = rd_u16(player_base, 20) + (uint32_t)growth[2] * level;
        accuracy = rd_u16(player_base, 22) + (uint32_t)growth[4] * level;
        record[0x1f] = player_base[0];
        updated.movement_profile = player_base[1];
        updated.movement_points = player_base[7];
        goto commit;
    }

    if (updated.unit_id >= 68u &&
        (size_t)(updated.unit_id - 68u) < ARRAY_COUNT(k_enemy_records)) {
        const uint8_t *base = k_enemy_records[updated.unit_id - 68u];
        hp = (uint32_t)rd_u16(base, 2) * level;
        mp = (uint32_t)base[4] * level;
        attack = (uint32_t)base[5] * level;
        defense = (uint32_t)base[6] * level;
        accuracy = (uint32_t)base[7] * level;
        record[0x1f] = base[0];
        updated.movement_profile = base[1];
        updated.movement_points = base[8];
        goto commit;
    }
    return -1;

commit:
    record[0x21] = level;
    wr_u16(record, 0x37, attack);
    wr_u16(record, 0x39, defense);
    wr_u16(record, 0x3e, accuracy);
    updated.hp = (uint16_t)hp;
    updated.hp_max = (uint16_t)hp;
    updated.mp = (uint16_t)mp;
    updated.mp_max = (uint16_t)mp;
    *unit = updated;
    return 0;
}
