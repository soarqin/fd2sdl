/* 炎龙骑士团 2 SDL3 重写 - 通用单位基础属性派生
 *
 * 逆向依据：field_unit_stage_template_append @corrected dual 0x35e6e
 *（code0 0x25e6e）按 unit ID 选择两类静态表：ID < 0x44 使用 24 字节基础
 * record + 11 字节成长 record；ID >= 0x44 使用 10 字节敌军 record。
 * 函数写入 0x1f/0x20/0x21、0x37/0x39/0x3e、0x3b 和 HP/MP 后调用
 * field_unit_combat_stats_recompute @0x4096e（entry 0x40964）。
 */

#include "field_unit_base.h"

#include <stddef.h>
#include <string.h>

#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

typedef struct {
    uint8_t unit_id;
    uint8_t race_code;
    uint8_t movement_profile;
    uint16_t hp_base;
    uint8_t hp_growth;
    uint16_t mp_base;
    uint8_t mp_growth;
    uint16_t attack_base;
    uint8_t attack_growth;
    uint16_t defense_base;
    uint8_t defense_growth;
    uint16_t accuracy_base;
    uint8_t accuracy_growth;
    uint8_t movement_points;
} fd2_player_base_record;

typedef struct {
    uint8_t unit_id;
    uint8_t race_code;
    uint8_t movement_profile;
    uint16_t hp_per_level;
    uint8_t mp_per_level;
    uint8_t attack_per_level;
    uint8_t defense_per_level;
    uint8_t accuracy_per_level;
    uint8_t movement_points;
} fd2_enemy_base_record;

/* 原版表记录来自 FD2.EXE：player base/growth 分别对应运行时
 * DS:0x3da1/0x40a1，enemy 对应 DS:0x3af9。已确认的同格式连续范围为
 * 角色 ID 0..31 与敌军 ID 68..135；ID 32..67 没有对应的同格式基础表。 */
static const fd2_player_base_record k_player_records[] = {
    {0, 1, 1, 42, 8, 0, 0, 0, 6, 0, 4, 0, 2, 4},
    {1, 1, 2, 36, 10, 0, 0, 0, 7, 0, 4, 0, 1, 4},
    {2, 1, 1, 40, 7, 0, 0, 0, 6, 0, 4, 0, 2, 4},
    {3, 1, 2, 30, 12, 0, 0, 12, 6, 0, 5, 8, 1, 4},
    {4, 1, 3, 48, 8, 0, 0, 0, 6, 0, 4, 0, 2, 7},
    {5, 1, 3, 72, 6, 0, 0, 0, 6, 0, 5, 0, 2, 7},
    {6, 1, 3, 70, 10, 0, 0, 0, 7, 0, 5, 0, 2, 6},
    {7, 1, 11, 281, 13, 0, 0, 185, 13, 110, 9, 48, 3, 8},
    {8, 1, 4, 32, 6, 0, 0, 0, 5, 0, 2, 0, 2, 4},
    {9, 1, 5, 28, 5, 8, 4, 0, 3, 0, 2, 0, 1, 4},
    {10, 1, 6, 38, 4, 12, 4, 0, 3, 0, 2, 0, 1, 3},
    {11, 1, 6, 30, 6, 24, 3, 0, 2, 0, 3, 0, 1, 3},
    {12, 1, 8, 52, 11, 0, 0, 0, 8, 19, 5, 0, 1, 5},
    {13, 2, 4, 40, 6, 0, 0, 0, 5, 0, 3, 0, 2, 4},
    {14, 2, 5, 66, 6, 40, 4, 0, 3, 0, 2, 0, 1, 4},
    {15, 3, 8, 100, 10, 0, 0, 40, 8, 40, 4, 10, 1, 5},
    {16, 4, 15, 330, 13, 0, 0, 210, 10, 160, 8, 40, 2, 7},
    {17, 4, 1, 65, 9, 0, 0, 0, 7, 0, 5, 14, 2, 6},
    {18, 1, 9, 280, 12, 8, 6, 170, 9, 130, 7, 45, 2, 5},
    {19, 1, 9, 272, 12, 0, 6, 160, 10, 110, 7, 40, 2, 5},
    {20, 1, 19, 290, 11, 0, 0, 180, 9, 110, 5, 45, 2, 9},
    {21, 1, 22, 220, 10, 140, 12, 80, 8, 77, 7, 35, 2, 4},
    {22, 1, 24, 316, 16, 0, 0, 200, 11, 140, 6, 60, 2, 5},
    {23, 2, 20, 235, 8, 0, 0, 170, 9, 70, 4, 55, 2, 5},
    {24, 2, 14, 190, 9, 150, 18, 50, 6, 60, 4, 36, 2, 4},
    {25, 3, 23, 380, 12, 20, 4, 180, 10, 110, 6, 50, 3, 7},
    {26, 4, 15, 294, 18, 0, 0, 114, 12, 122, 10, 60, 2, 7},
    {27, 4, 15, 300, 14, 0, 0, 210, 10, 130, 9, 45, 2, 7},
    {28, 5, 28, 300, 15, 10, 4, 135, 12, 100, 8, 42, 2, 5},
    {29, 5, 13, 150, 14, 60, 12, 100, 8, 70, 8, 0, 3, 6},
    {30, 6, 25, 50, 8, 0, 0, 0, 7, 0, 6, 0, 2, 4},
    {31, 6, 25, 12, 12, 0, 0, 0, 8, 0, 7, 0, 2, 4},
};

static const fd2_enemy_base_record k_enemy_records[] = {
    {68, 1, 2, 18, 0, 5, 2, 1, 4},
    {69, 1, 3, 20, 0, 8, 2, 1, 7},
    {70, 1, 2, 16, 0, 5, 2, 1, 4},
    {71, 3, 8, 20, 0, 10, 6, 2, 5},
    {72, 2, 12, 46, 0, 12, 7, 4, 5},
    {73, 4, 15, 24, 0, 16, 11, 5, 7},
    {74, 1, 2, 12, 0, 5, 2, 1, 4},
    {75, 1, 2, 12, 0, 5, 2, 1, 4},
    {76, 1, 2, 14, 0, 5, 4, 1, 4},
    {77, 1, 2, 18, 0, 8, 4, 2, 4},
    {78, 1, 2, 23, 0, 10, 5, 2, 3},
    {79, 1, 2, 30, 0, 16, 7, 4, 4},
    {80, 1, 2, 54, 0, 24, 9, 5, 4},
    {81, 1, 2, 60, 0, 25, 15, 6, 4},
    {82, 1, 3, 15, 0, 8, 1, 1, 7},
    {83, 1, 3, 20, 0, 8, 2, 2, 8},
    {84, 1, 11, 44, 4, 26, 12, 4, 8},
    {85, 1, 3, 48, 0, 22, 6, 5, 8},
    {86, 1, 19, 31, 0, 16, 7, 3, 9},
    {87, 1, 4, 12, 0, 4, 2, 2, 4},
    {88, 1, 4, 36, 0, 17, 8, 6, 4},
    {89, 1, 12, 30, 0, 20, 10, 6, 5},
    {90, 1, 5, 10, 5, 2, 1, 2, 3},
    {91, 1, 5, 39, 20, 12, 5, 4, 3},
    {92, 1, 13, 29, 24, 14, 6, 4, 4},
    {93, 1, 6, 12, 4, 5, 2, 1, 3},
    {94, 1, 6, 41, 17, 17, 7, 5, 3},
    {95, 1, 14, 33, 20, 18, 8, 4, 4},
    {96, 1, 7, 14, 0, 7, 1, 1, 4},
    {97, 1, 7, 24, 0, 8, 3, 2, 4},
    {98, 1, 7, 70, 0, 34, 26, 12, 5},
    {99, 1, 1, 36, 0, 20, 12, 4, 6},
    {100, 1, 8, 45, 0, 22, 12, 5, 5},
    {101, 1, 16, 46, 0, 24, 12, 6, 5},
    {102, 8, 2, 16, 0, 8, 2, 1, 5},
    {103, 8, 2, 22, 0, 10, 3, 2, 5},
    {104, 10, 26, 80, 80, 20, 10, 4, 0},
    {105, 10, 26, 80, 80, 20, 10, 4, 0},
    {106, 4, 2, 25, 0, 13, 8, 3, 6},
    {107, 4, 5, 20, 30, 11, 6, 3, 6},
    {108, 5, 26, 38, 4, 28, 16, 6, 5},
    {109, 5, 26, 40, 5, 30, 18, 6, 6},
    {110, 5, 26, 40, 6, 33, 20, 6, 6},
    {111, 6, 25, 40, 0, 20, 16, 3, 4},
    {112, 6, 25, 45, 0, 22, 12, 4, 7},
    {113, 6, 25, 100, 0, 0, 20, 0, 0},
    {114, 6, 25, 36, 0, 18, 14, 5, 4},
    {115, 6, 25, 34, 0, 15, 16, 3, 4},
    {116, 6, 25, 100, 0, 20, 13, 3, 5},
    {117, 1, 7, 30, 0, 5, 3, 2, 4},
    {118, 1, 3, 12, 0, 8, 3, 1, 6},
    {119, 1, 10, 120, 0, 20, 12, 5, 4},
    {120, 1, 10, 90, 0, 20, 10, 4, 5},
    {121, 9, 28, 42, 0, 31, 28, 5, 4},
    {122, 7, 26, 150, 14, 22, 10, 3, 2},
    {123, 7, 26, 160, 20, 20, 16, 4, 3},
    {124, 7, 26, 145, 28, 23, 10, 4, 5},
    {125, 7, 26, 140, 25, 24, 9, 4, 4},
    {126, 9, 26, 300, 200, 15, 8, 3, 5},
    {127, 10, 26, 120, 80, 20, 10, 4, 0},
    {128, 9, 26, 500, 20, 20, 10, 5, 5},
    {129, 9, 26, 500, 20, 20, 10, 5, 5},
    {130, 9, 26, 500, 20, 20, 10, 5, 5},
    {131, 9, 26, 500, 20, 20, 10, 5, 5},
    {132, 1, 22, 12, 0, 1, 3, 0, 4},
    {133, 1, 27, 12, 0, 1, 3, 1, 4},
    {134, 1, 27, 14, 0, 1, 2, 1, 4},
    {135, 1, 27, 33, 0, 1, 2, 2, 4},
};

static void wr_u16(uint8_t *record, size_t offset, uint32_t value) {
    uint16_t narrowed = (uint16_t)value;
    record[offset] = (uint8_t)narrowed;
    record[offset + 1u] = (uint8_t)(narrowed >> 8);
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

    if (fd2_field_unit_base_apply(&updated, tpl[0x04]) != 0)
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

    for (size_t i = 0; i < ARRAY_COUNT(k_player_records); i++) {
        const fd2_player_base_record *base = &k_player_records[i];
        if (base->unit_id != updated.unit_id) continue;
        uint32_t level_minus_one = (uint32_t)level - 1u;
        hp = base->hp_base + (uint32_t)base->hp_growth * level_minus_one;
        mp = base->mp_base + (uint32_t)base->mp_growth * level_minus_one;
        attack = base->attack_base + (uint32_t)base->attack_growth * level;
        defense = base->defense_base + (uint32_t)base->defense_growth * level;
        accuracy = base->accuracy_base + (uint32_t)base->accuracy_growth * level;
        record[0x1f] = base->race_code;
        updated.movement_profile = base->movement_profile;
        updated.movement_points = base->movement_points;
        goto commit;
    }

    for (size_t i = 0; i < ARRAY_COUNT(k_enemy_records); i++) {
        const fd2_enemy_base_record *base = &k_enemy_records[i];
        if (base->unit_id != updated.unit_id) continue;
        hp = (uint32_t)base->hp_per_level * level;
        mp = (uint32_t)base->mp_per_level * level;
        attack = (uint32_t)base->attack_per_level * level;
        defense = (uint32_t)base->defense_per_level * level;
        accuracy = (uint32_t)base->accuracy_per_level * level;
        record[0x1f] = base->race_code;
        updated.movement_profile = base->movement_profile;
        updated.movement_points = base->movement_points;
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
