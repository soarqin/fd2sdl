/* 炎龙骑士团 2 SDL3 重写 - 通用单位基础属性派生
 *
 * 逆向依据：field_unit_stage_template_append @0x10c50（原登记同名；
 * 历史旧地址0x35e6e）按 unit ID 选择两类静态表：ID < 0x44 使用24字节基础
 * record + 11 字节成长 record；ID >= 0x44 使用 10 字节敌军 record。
 * 函数写入 0x1f/0x20/0x21、0x37/0x39/0x3e、0x3b 和 HP/MP 后调用
 * field_unit_combat_stats_recompute @0x1b750（原登记同名，
 * 入口旧别名field_unit_combat_stats_recompute_entry）。
 * 敌军表／经验系数复用field_enemy_base_record_get @0x4e84f（原登记sub_4E84F）。
 */

#include "field_unit_base.h"

#include <stddef.h>
#include <limits.h>
#include <string.h>

#include "field_item.h"
#include "field_unit_stats.h"

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
    uint8_t experience_coefficient;
} fd2_enemy_base_record;

/* 原版表记录来自 FD2.EXE：player base/growth 分别对应运行时
 * DS:0x3da1/0x40a1，enemy 对应 DS:0x3af9。已确认的同格式连续范围为
 * 角色 ID 0..31 与敌军 ID 68..135；ID 32..67 没有对应的同格式基础表。
 * 敌表VA0x61af9末字节由sub_1C81F @0x1c81f（原登记同名）作为经验系数；
 * 本次补齐前逐条校验了既有9字节字段与原680字节表一致。 */
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

/* sub_4E838 @0x4e838 返回 object3 VA 0x61da1 的 24-byte player base。
 * 依次保留其 +2 初始等级、+8..11 法术、+12..17 装备／库存字段。 */
static const uint8_t k_player_initial[32][11] = {
    {1, 0, 0, 0, 0, 0, 132, 192, 255, 255, 255},
    {3, 0, 0, 0, 0, 0, 128, 255, 255, 255, 255},
    {4, 0, 0, 0, 0, 0, 128, 255, 255, 255, 255},
    {23, 0, 0, 0, 0, 36, 147, 255, 255, 255, 255},
    {1, 0, 0, 0, 0, 20, 128, 255, 255, 255, 255},
    {8, 0, 0, 0, 0, 0, 129, 255, 255, 255, 255},
    {18, 0, 0, 0, 0, 23, 135, 255, 255, 255, 255},
    {4, 0, 0, 0, 0, 30, 150, 255, 255, 255, 255},
    {3, 0, 0, 0, 0, 44, 129, 255, 255, 255, 255},
    {1, 1, 0, 0, 0, 52, 164, 255, 255, 255, 255},
    {8, 0, 0, 0, 0, 0, 129, 255, 255, 255, 255},
    {16, 0, 64, 64, 0, 54, 167, 209, 255, 255, 255},
    {10, 0, 0, 0, 0, 62, 172, 255, 255, 255, 255},
    {8, 0, 0, 0, 0, 0, 129, 255, 255, 255, 255},
    {16, 50, 0, 0, 0, 55, 168, 255, 255, 255, 255},
    {23, 0, 0, 0, 0, 63, 174, 255, 255, 255, 255},
    {2, 0, 0, 0, 0, 16, 138, 255, 255, 255, 255},
    {21, 0, 0, 0, 0, 4, 136, 255, 255, 255, 255},
    {1, 0, 0, 0, 0, 7, 138, 255, 255, 255, 255},
    {10, 0, 0, 0, 0, 0, 129, 255, 255, 255, 255},
    {8, 0, 0, 0, 0, 0, 129, 255, 255, 255, 255},
    {5, 0, 193, 124, 0, 57, 169, 255, 255, 255, 255},
    {12, 0, 0, 0, 0, 0, 129, 255, 255, 255, 255},
    {5, 0, 0, 0, 0, 0, 129, 255, 255, 255, 255},
    {8, 0, 0, 0, 0, 0, 129, 255, 255, 255, 255},
    {5, 0, 0, 0, 0, 0, 129, 255, 255, 255, 255},
    {15, 0, 0, 0, 0, 0, 129, 255, 255, 255, 255},
    {5, 0, 0, 0, 0, 17, 139, 255, 255, 255, 255},
    {10, 0, 64, 2, 4, 65, 176, 255, 255, 255, 255},
    {20, 0, 28, 2, 12, 59, 170, 255, 255, 255, 255},
    {1, 0, 0, 0, 0, 72, 178, 255, 255, 255, 255},
    {25, 0, 0, 0, 0, 0, 129, 255, 255, 255, 255},
};

static const fd2_enemy_base_record k_enemy_records[] = {
    {68, 1, 2, 18, 0, 5, 2, 1, 4, 30},
    {69, 1, 3, 20, 0, 8, 2, 1, 7, 30},
    {70, 1, 2, 16, 0, 5, 2, 1, 4, 30},
    {71, 3, 8, 20, 0, 10, 6, 2, 5, 30},
    {72, 2, 12, 46, 0, 12, 7, 4, 5, 30},
    {73, 4, 15, 24, 0, 16, 11, 5, 7, 30},
    {74, 1, 2, 12, 0, 5, 2, 1, 4, 30},
    {75, 1, 2, 12, 0, 5, 2, 1, 4, 30},
    {76, 1, 2, 14, 0, 5, 4, 1, 4, 30},
    {77, 1, 2, 18, 0, 8, 4, 2, 4, 45},
    {78, 1, 2, 23, 0, 10, 5, 2, 3, 70},
    {79, 1, 2, 30, 0, 16, 7, 4, 4, 120},
    {80, 1, 2, 54, 0, 24, 9, 5, 4, 120},
    {81, 1, 2, 60, 0, 25, 15, 6, 4, 120},
    {82, 1, 3, 15, 0, 8, 1, 1, 7, 40},
    {83, 1, 3, 20, 0, 8, 2, 2, 8, 75},
    {84, 1, 11, 44, 4, 26, 12, 4, 8, 180},
    {85, 1, 3, 48, 0, 22, 6, 5, 8, 160},
    {86, 1, 19, 31, 0, 16, 7, 3, 9, 80},
    {87, 1, 4, 12, 0, 4, 2, 2, 4, 40},
    {88, 1, 4, 36, 0, 17, 8, 6, 4, 128},
    {89, 1, 12, 30, 0, 20, 10, 6, 5, 130},
    {90, 1, 5, 10, 5, 2, 1, 2, 3, 50},
    {91, 1, 5, 39, 20, 12, 5, 4, 3, 130},
    {92, 1, 13, 29, 24, 14, 6, 4, 4, 200},
    {93, 1, 6, 12, 4, 5, 2, 1, 3, 50},
    {94, 1, 6, 41, 17, 17, 7, 5, 3, 120},
    {95, 1, 14, 33, 20, 18, 8, 4, 4, 160},
    {96, 1, 7, 14, 0, 7, 1, 1, 4, 21},
    {97, 1, 7, 24, 0, 8, 3, 2, 4, 33},
    {98, 1, 7, 70, 0, 34, 26, 12, 5, 200},
    {99, 1, 1, 36, 0, 20, 12, 4, 6, 110},
    {100, 1, 8, 45, 0, 22, 12, 5, 5, 150},
    {101, 1, 16, 46, 0, 24, 12, 6, 5, 160},
    {102, 8, 2, 16, 0, 8, 2, 1, 5, 40},
    {103, 8, 2, 22, 0, 10, 3, 2, 5, 60},
    {104, 10, 26, 80, 80, 20, 10, 4, 0, 250},
    {105, 10, 26, 80, 80, 20, 10, 4, 0, 250},
    {106, 4, 2, 25, 0, 13, 8, 3, 6, 100},
    {107, 4, 5, 20, 30, 11, 6, 3, 6, 120},
    {108, 5, 26, 38, 4, 28, 16, 6, 5, 170},
    {109, 5, 26, 40, 5, 30, 18, 6, 6, 180},
    {110, 5, 26, 40, 6, 33, 20, 6, 6, 200},
    {111, 6, 25, 40, 0, 20, 16, 3, 4, 80},
    {112, 6, 25, 45, 0, 22, 12, 4, 7, 120},
    {113, 6, 25, 100, 0, 0, 20, 0, 0, 180},
    {114, 6, 25, 36, 0, 18, 14, 5, 4, 150},
    {115, 6, 25, 34, 0, 15, 16, 3, 4, 150},
    {116, 6, 25, 100, 0, 20, 13, 3, 5, 140},
    {117, 1, 7, 30, 0, 5, 3, 2, 4, 99},
    {118, 1, 3, 12, 0, 8, 3, 1, 6, 80},
    {119, 1, 10, 120, 0, 20, 12, 5, 4, 120},
    {120, 1, 10, 90, 0, 20, 10, 4, 5, 255},
    {121, 9, 28, 42, 0, 31, 28, 5, 4, 100},
    {122, 7, 26, 150, 14, 22, 10, 3, 2, 250},
    {123, 7, 26, 160, 20, 20, 16, 4, 3, 250},
    {124, 7, 26, 145, 28, 23, 10, 4, 5, 250},
    {125, 7, 26, 140, 25, 24, 9, 4, 4, 250},
    {126, 9, 26, 300, 200, 15, 8, 3, 5, 255},
    {127, 10, 26, 120, 80, 20, 10, 4, 0, 250},
    {128, 9, 26, 500, 20, 20, 10, 5, 5, 255},
    {129, 9, 26, 500, 20, 20, 10, 5, 5, 255},
    {130, 9, 26, 500, 20, 20, 10, 5, 5, 255},
    {131, 9, 26, 500, 20, 20, 10, 5, 5, 255},
    {132, 1, 22, 12, 0, 1, 3, 0, 4, 1},
    {133, 1, 27, 12, 0, 1, 3, 1, 4, 1},
    {134, 1, 27, 14, 0, 1, 2, 1, 4, 1},
    {135, 1, 27, 33, 0, 1, 2, 2, 4, 1},
};

/* object3 VA0x620a1，strict LE == Domain；SHA256 75bedc7308088625abc0b70c10115d94903008e7fb907a0acaadc22dc45af2a5。 */
static const uint8_t k_growth_records[68][11] = {
    {0x06, 0x08, 0x04, 0x06, 0x02, 0x03, 0x08, 0x0c, 0x00, 0x00, 0xff},
    {0x07, 0x0a, 0x04, 0x06, 0x01, 0x03, 0x0a, 0x0f, 0x00, 0x00, 0xff},
    {0x06, 0x09, 0x04, 0x06, 0x02, 0x03, 0x07, 0x0c, 0x00, 0x00, 0xff},
    {0x06, 0x08, 0x05, 0x07, 0x01, 0x01, 0x0c, 0x10, 0x00, 0x00, 0xff},
    {0x06, 0x09, 0x04, 0x05, 0x02, 0x02, 0x08, 0x0b, 0x00, 0x00, 0xff},
    {0x06, 0x08, 0x05, 0x07, 0x02, 0x03, 0x06, 0x08, 0x00, 0x00, 0xff},
    {0x07, 0x0a, 0x05, 0x08, 0x02, 0x03, 0x0a, 0x0d, 0x00, 0x00, 0xff},
    {0x0d, 0x11, 0x09, 0x0c, 0x03, 0x05, 0x0d, 0x12, 0x00, 0x00, 0xff},
    {0x05, 0x08, 0x02, 0x04, 0x02, 0x03, 0x06, 0x09, 0x00, 0x00, 0xff},
    {0x03, 0x06, 0x02, 0x05, 0x01, 0x03, 0x05, 0x09, 0x04, 0x08, 0x00},
    {0x03, 0x05, 0x02, 0x06, 0x01, 0x02, 0x04, 0x08, 0x04, 0x07, 0x01},
    {0x02, 0x04, 0x03, 0x07, 0x01, 0x02, 0x06, 0x09, 0x03, 0x06, 0x02},
    {0x08, 0x0a, 0x05, 0x06, 0x01, 0x04, 0x0b, 0x0e, 0x00, 0x00, 0xff},
    {0x05, 0x07, 0x03, 0x04, 0x02, 0x03, 0x06, 0x09, 0x00, 0x00, 0xff},
    {0x03, 0x05, 0x02, 0x03, 0x01, 0x03, 0x06, 0x08, 0x04, 0x08, 0x03},
    {0x08, 0x0c, 0x04, 0x06, 0x01, 0x03, 0x0a, 0x0c, 0x00, 0x00, 0xff},
    {0x0a, 0x0f, 0x08, 0x0d, 0x02, 0x04, 0x0d, 0x11, 0x00, 0x00, 0xff},
    {0x07, 0x0b, 0x05, 0x08, 0x02, 0x03, 0x09, 0x0d, 0x00, 0x00, 0xff},
    {0x09, 0x0f, 0x07, 0x0b, 0x02, 0x04, 0x0c, 0x10, 0x06, 0x08, 0x05},
    {0x0a, 0x0f, 0x07, 0x0a, 0x02, 0x03, 0x0c, 0x10, 0x06, 0x08, 0xff},
    {0x09, 0x0f, 0x05, 0x07, 0x02, 0x03, 0x0b, 0x11, 0x00, 0x00, 0xff},
    {0x08, 0x0c, 0x07, 0x0b, 0x02, 0x03, 0x0a, 0x0c, 0x0c, 0x0f, 0xff},
    {0x0b, 0x0e, 0x06, 0x08, 0x02, 0x04, 0x10, 0x14, 0x00, 0x00, 0xff},
    {0x09, 0x0e, 0x04, 0x07, 0x02, 0x05, 0x08, 0x0c, 0x00, 0x00, 0xff},
    {0x06, 0x08, 0x04, 0x07, 0x02, 0x05, 0x09, 0x0b, 0x12, 0x18, 0xff},
    {0x0a, 0x0d, 0x06, 0x09, 0x03, 0x06, 0x0c, 0x0f, 0x04, 0x07, 0xff},
    {0x0c, 0x11, 0x0a, 0x0c, 0x02, 0x04, 0x12, 0x19, 0x00, 0x00, 0xff},
    {0x0a, 0x0f, 0x09, 0x0d, 0x02, 0x04, 0x0e, 0x12, 0x00, 0x00, 0xff},
    {0x0c, 0x0f, 0x08, 0x0c, 0x02, 0x03, 0x0f, 0x16, 0x04, 0x06, 0x11},
    {0x08, 0x0e, 0x08, 0x0c, 0x03, 0x06, 0x0e, 0x16, 0x0c, 0x10, 0x08},
    {0x07, 0x0e, 0x06, 0x0d, 0x02, 0x04, 0x08, 0x0f, 0x00, 0x00, 0xff},
    {0x08, 0x0e, 0x07, 0x0e, 0x02, 0x04, 0x0c, 0x0f, 0x00, 0x00, 0xff},
    {0x08, 0x0c, 0x05, 0x08, 0x03, 0x05, 0x0a, 0x0d, 0x07, 0x09, 0x04},
    {0x0d, 0x11, 0x09, 0x0b, 0x02, 0x04, 0x0f, 0x14, 0x00, 0x00, 0xff},
    {0x08, 0x0a, 0x06, 0x09, 0x02, 0x04, 0x0a, 0x0f, 0x05, 0x07, 0x06},
    {0x0d, 0x12, 0x0b, 0x0f, 0x02, 0x04, 0x10, 0x16, 0x00, 0x00, 0xff},
    {0x09, 0x0d, 0x07, 0x09, 0x02, 0x04, 0x0c, 0x10, 0x00, 0x00, 0xff},
    {0x09, 0x0d, 0x08, 0x0a, 0x02, 0x04, 0x0b, 0x0f, 0x00, 0x00, 0xff},
    {0x0a, 0x0e, 0x08, 0x0a, 0x02, 0x04, 0x0c, 0x10, 0x00, 0x00, 0xff},
    {0x08, 0x0c, 0x03, 0x06, 0x02, 0x04, 0x0a, 0x0f, 0x00, 0x00, 0xff},
    {0x09, 0x0b, 0x07, 0x0a, 0x02, 0x04, 0x09, 0x0d, 0x00, 0x00, 0xff},
    {0x08, 0x0c, 0x04, 0x07, 0x02, 0x05, 0x09, 0x0d, 0x0f, 0x14, 0x07},
    {0x08, 0x0c, 0x05, 0x09, 0x03, 0x05, 0x0b, 0x0d, 0x0c, 0x10, 0x09},
    {0x07, 0x0b, 0x06, 0x0b, 0x03, 0x05, 0x0c, 0x0e, 0x0a, 0x0e, 0x12},
    {0x0a, 0x0e, 0x06, 0x09, 0x03, 0x05, 0x0d, 0x12, 0x00, 0x00, 0xff},
    {0x08, 0x0c, 0x04, 0x07, 0x02, 0x04, 0x08, 0x0c, 0x00, 0x00, 0xff},
    {0x08, 0x0e, 0x06, 0x09, 0x03, 0x04, 0x08, 0x0b, 0x12, 0x16, 0x08},
    {0x09, 0x0d, 0x07, 0x0a, 0x02, 0x04, 0x0e, 0x12, 0x00, 0x00, 0xff},
    {0x0a, 0x0c, 0x06, 0x08, 0x02, 0x03, 0x0f, 0x14, 0x00, 0x00, 0xff},
    {0x0b, 0x0e, 0x08, 0x0b, 0x02, 0x04, 0x0c, 0x12, 0x00, 0x00, 0xff},
    {0x0a, 0x0f, 0x07, 0x0a, 0x02, 0x04, 0x0c, 0x0f, 0x08, 0x0c, 0x0a},
    {0x0d, 0x10, 0x0a, 0x0c, 0x02, 0x03, 0x0f, 0x14, 0x08, 0x0c, 0x0f},
    {0x09, 0x0c, 0x06, 0x09, 0x03, 0x05, 0x0c, 0x12, 0x14, 0x1e, 0x13},
    {0x0d, 0x10, 0x0b, 0x0f, 0x02, 0x03, 0x10, 0x16, 0x08, 0x0c, 0x10},
    {0x09, 0x0d, 0x07, 0x09, 0x02, 0x04, 0x0c, 0x10, 0x00, 0x00, 0xff},
    {0x09, 0x0e, 0x08, 0x0a, 0x02, 0x04, 0x0b, 0x0f, 0x00, 0x00, 0xff},
    {0x0a, 0x0e, 0x07, 0x09, 0x02, 0x04, 0x0b, 0x0f, 0x00, 0x00, 0xff},
    {0x08, 0x0c, 0x03, 0x05, 0x02, 0x04, 0x0a, 0x0c, 0x00, 0x00, 0xff},
    {0x0c, 0x0e, 0x07, 0x0a, 0x02, 0x04, 0x0a, 0x0f, 0x00, 0x00, 0xff},
    {0x08, 0x0c, 0x05, 0x08, 0x02, 0x05, 0x0c, 0x0f, 0x0b, 0x10, 0x0e},
    {0x09, 0x0d, 0x05, 0x08, 0x03, 0x05, 0x0b, 0x0d, 0x0e, 0x12, 0x0b},
    {0x08, 0x0d, 0x05, 0x0a, 0x03, 0x05, 0x0d, 0x10, 0x0a, 0x0e, 0x0c},
    {0x0c, 0x0f, 0x07, 0x09, 0x03, 0x06, 0x0e, 0x12, 0x00, 0x00, 0xff},
    {0x09, 0x0d, 0x04, 0x07, 0x02, 0x04, 0x09, 0x0d, 0x00, 0x00, 0xff},
    {0x08, 0x0a, 0x07, 0x09, 0x03, 0x05, 0x08, 0x0b, 0x0e, 0x12, 0x0d},
    {0x0a, 0x0f, 0x07, 0x09, 0x02, 0x05, 0x0e, 0x12, 0x00, 0x00, 0xff},
    {0x0a, 0x0c, 0x0a, 0x0c, 0x02, 0x02, 0x0c, 0x12, 0x00, 0x00, 0xff},
    {0x08, 0x0c, 0x08, 0x0a, 0x03, 0x03, 0x0a, 0x0f, 0x00, 0x00, 0xff}
};

/* object3 VA0x626b3，strict LE == Domain；SHA256 839200667f837136d3426ab952e455b1a63b3bbba8b9175eb895d4408ddd6fb9。 */
static const uint8_t k_learning_records[20][12] = {
    {0x05, 0x11, 0x09, 0x01, 0x0f, 0x1a, 0x15, 0x02, 0x1a, 0x1b, 0xff, 0xff},
    {0x07, 0x14, 0x0b, 0x0e, 0x10, 0x12, 0x18, 0x16, 0x1e, 0x17, 0xff, 0xff},
    {0x12, 0x13, 0x18, 0x15, 0x1e, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0x14, 0x0a, 0x18, 0x06, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0x04, 0x18, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0x02, 0x1e, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0x04, 0x1d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0x03, 0x1b, 0x07, 0x03, 0x0d, 0x13, 0x16, 0x0b, 0xff, 0xff, 0xff, 0xff},
    {0x04, 0x02, 0x08, 0x07, 0x0c, 0x0b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0x03, 0x0f, 0x09, 0x15, 0x12, 0x13, 0x17, 0x10, 0xff, 0xff, 0xff, 0xff},
    {0x04, 0x0f, 0x08, 0x03, 0x0e, 0x07, 0x14, 0x08, 0x18, 0x0c, 0x1e, 0x17},
    {0x03, 0x0f, 0x09, 0x08, 0x10, 0x13, 0x15, 0x15, 0x1a, 0x10, 0xff, 0xff},
    {0x03, 0x0f, 0x08, 0x08, 0x0f, 0x1b, 0x14, 0x15, 0x19, 0x10, 0xff, 0xff},
    {0x04, 0x0f, 0x0a, 0x08, 0x0f, 0x16, 0x14, 0x14, 0x18, 0x07, 0xff, 0xff},
    {0x03, 0x0f, 0x09, 0x08, 0x10, 0x15, 0x15, 0x12, 0x19, 0x03, 0xff, 0xff},
    {0x04, 0x12, 0x0b, 0x0f, 0x12, 0x14, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0x03, 0x13, 0x09, 0x0f, 0x10, 0x14, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0x0f, 0x0b, 0x14, 0x12, 0x17, 0x0b, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
    {0x04, 0x0f, 0x0a, 0x16, 0x12, 0x10, 0x18, 0x12, 0xff, 0xff, 0xff, 0xff},
    {0x02, 0x22, 0x06, 0x23, 0x0b, 0x17, 0x0f, 0x03, 0x13, 0x21, 0x17, 0x20}
};

int fd2_field_unit_enemy_experience_coefficient(
        uint8_t unit_id, uint8_t *coefficient) {
    if (!coefficient || unit_id < 68u ||
        (size_t)(unit_id - 68u) >= ARRAY_COUNT(k_enemy_records))
        return -1;
    *coefficient = k_enemy_records[unit_id - 68u].experience_coefficient;
    return 0;
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
    /* field_unit_experience_reward_execute @0x1e292（原sub_1E292）：只有精确等值门，早退不消费
     * 全局贡献。此纯函数连out也不写，由game保留各caller的53EC8 owner。 */
    uint8_t limit = source->unit_id == 30u || source->unit_id == 31u ? 99u : 40u;
    if (!gain || (source->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) || source->level == limit)
        return 0;
    if (source->unit_id >= ARRAY_COUNT(k_growth_records) ||
        gain > INT32_MAX - (uint32_t)source->experience)
        return -1;
    uint32_t total = source->experience + gain;
    fd2_field_unit updated = *source;
    const uint8_t offsets[5] = {0x37, 0x39, 0x3e, 0x42, 0x46};
    while (total >= 100u) {
        fd2_field_unit_level_growth level;
        if (fd2_field_unit_level_growth_prepare(&updated, &level) != 0)
            return -1;
        updated.level = level.level;
        uint8_t *raw = (uint8_t *)&updated;
        for (size_t i = 0; i < ARRAY_COUNT(offsets); i++) {
            size_t offset = offsets[i];
            uint16_t value = (uint16_t)(raw[offset] | (uint16_t)raw[offset + 1u] << 8);
            wr_u16(raw, offset, (uint32_t)value + level.growth[i]);
        }
        for (size_t i = 0; i < level.learned_count; i++) {
            uint8_t id = level.learned_magic[i];
            raw[0x1au + id / 8u] |= (uint8_t)(1u << (id % 8u));
        }
        if (fd2_field_unit_combat_stats_recompute(&updated,
                initial_item_stats, NULL, scale, scale_userdata) != 0)
            return -1;
        total -= 100u;
        /* 原循环只在新level30或特殊ID新99清余数；不擅加新level40门。 */
        if (updated.level == 30u ||
            ((updated.unit_id == 30u || updated.unit_id == 31u) && updated.level == 99u))
            total = 0;
    }
    updated.experience = (uint8_t)total;
    *out = updated;
    return 1;
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
    if (!unit || unit_id >= ARRAY_COUNT(k_player_initial)) return -1;
    const uint8_t *initial = k_player_initial[unit_id];
    fd2_field_unit updated = *unit;
    fd2_field_unit derived = *unit;
    derived.unit_id = unit_id;
    if (fd2_field_unit_base_apply(&derived, initial[0]) != 0) return -1;
    uint8_t *record = (uint8_t *)&updated;
    const uint8_t *base = (const uint8_t *)&derived;
    record[5] = 0;
    record[6] = 2;
    record[7] = record[8] = unit_id;
    record[9] = 0;
    record[10] = record[12] = 0x40;
    record[11] = initial[5];
    record[13] = initial[6];
    for (size_t i = 0; i < 4; i++) {
        record[14 + i * 2u] = initial[7 + i] == 0xff ? 0x80 : 0;
        record[15 + i * 2u] = initial[7 + i];
    }
    record[22] = record[24] = 0x80;
    memcpy(record + 26, initial + 1, 4);
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
