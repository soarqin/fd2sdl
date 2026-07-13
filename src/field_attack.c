/* 炎龙骑士团 2 SDL3 重写 - 普通攻击范围、目标与武器外围效果
 *
 * 逆向依据：field_equipped_item_slot_find @0x40a51、
 * field_unit_item_id_at @0x40936 与 field_target_range_build @0x39a2c。
 * 玩家攻击路径读取武器 record +0x0b/+0x0c，按 profile 0 传播范围，
 * 再以 side_filter=0 保留 side == 0 的可见目标。物理攻击本体同时确认
 * item effect 2 的命中前状态、effect 3 的双击、effect 4 的暴击加值、
 * 地形攻防百分比与免疫判定，以及邻接反击条件。
 */
#include "field_attack.h"

#include <stdlib.h>

#include "field_item.h"
#include "field_move_profile.h"

#define FD2_FIELD_EQUIPMENT_SLOT_COUNT 8u
#define FD2_FIELD_EQUIPMENT_FLAGS_OFFSET 0x0au
#define FD2_FIELD_EQUIPMENT_ITEM_OFFSET 0x0bu
#define FD2_FIELD_EQUIPMENT_SLOT_SIZE 2u
#define FD2_FIELD_EQUIPPED_FLAG 0x40u
#define FD2_FIELD_ITEM_NON_WEAPON_BASE 0x80u
#define FD2_FIELD_ITEM_EFFECT_TYPE_OFFSET 0x09u
#define FD2_FIELD_ITEM_EFFECT_VALUE_OFFSET 0x0au
#define FD2_FIELD_ITEM_MIN_RANGE_OFFSET 0x0bu
#define FD2_FIELD_ITEM_MAX_RANGE_OFFSET 0x0cu
#define FD2_FIELD_ITEM_PRE_HIT_STATUS_EFFECT 2u
#define FD2_FIELD_ITEM_DOUBLE_STRIKE_EFFECT 3u
#define FD2_FIELD_ITEM_CRITICAL_EFFECT 4u
#define FD2_FIELD_DOUBLE_STRIKE_CHANCE 3u
#define FD2_FIELD_PRE_HIT_STATUS_BASE 2u
#define FD2_FIELD_PRE_HIT_STATUS_COUNT 4u
#define FD2_FIELD_TERRAIN_OVERRIDE_UNIT_ID 28u
#define FD2_FIELD_TERRAIN_IMMUNE_PROFILE 19u
#define FD2_FIELD_TERRAIN_IMMUNE_RACE_1 4u
#define FD2_FIELD_TERRAIN_IMMUNE_RACE_2 5u
#define FD2_FIELD_ATTACK_PROFILE 0u

typedef struct {
    const fd2_field_map *map;
    const fd2_terrain_tileset *terrain;
    const uint8_t *costs;
} fd2_field_attack_range_context;

static int attack_range_query(void *userdata,
                              int from_x, int from_y,
                              int to_x, int to_y,
                              fd2_field_path_step *step) {
    (void)from_x;
    (void)from_y;
    fd2_field_attack_range_context *context = userdata;
    if (!context || !context->map || !context->map->cells ||
        !context->terrain || !context->terrain->attrs || !context->costs ||
        !step || to_x < 0 || to_y < 0 ||
        to_x >= context->map->width || to_y >= context->map->height)
        return -1;

    uint32_t cell = context->map->cells[
        (size_t)to_y * (size_t)context->map->width + (size_t)to_x];
    uint16_t terrain_id = (uint16_t)(cell & 0x03ffu);
    if (terrain_id >= context->terrain->attr_count) return -1;
    uint8_t cost_class =
        context->terrain->attrs[terrain_id].movement_cost_class;
    if (cost_class >= FD2_FIELD_MOVEMENT_COST_CLASS_COUNT) return -1;

    step->cost = context->costs[cost_class];
    step->can_stop = 1;
    step->can_expand = 1;
    return 1;
}

static const uint8_t *equipped_weapon_record(
        const fd2_field_unit *attacker) {
    if (!attacker) return NULL;
    const uint8_t *record = (const uint8_t *)attacker;
    for (size_t slot = 0; slot < FD2_FIELD_EQUIPMENT_SLOT_COUNT; slot++) {
        size_t flags_offset = FD2_FIELD_EQUIPMENT_FLAGS_OFFSET +
                              slot * FD2_FIELD_EQUIPMENT_SLOT_SIZE;
        size_t item_offset = FD2_FIELD_EQUIPMENT_ITEM_OFFSET +
                             slot * FD2_FIELD_EQUIPMENT_SLOT_SIZE;
        if ((record[flags_offset] & FD2_FIELD_EQUIPPED_FLAG) == 0 ||
            record[item_offset] >= FD2_FIELD_ITEM_NON_WEAPON_BASE)
            continue;
        return fd2_field_item_record_get(record[item_offset]);
    }
    return NULL;
}

int fd2_field_attack_weapon_range(const fd2_field_unit *attacker,
                                  uint8_t *min_range,
                                  uint8_t *max_range) {
    if (!min_range || !max_range) return -1;
    const uint8_t *item = equipped_weapon_record(attacker);
    if (!item) return -1;
    *min_range = item[FD2_FIELD_ITEM_MIN_RANGE_OFFSET];
    *max_range = item[FD2_FIELD_ITEM_MAX_RANGE_OFFSET];
    return *min_range <= *max_range ? 0 : -1;
}

int fd2_field_attack_weapon_critical_bonus(const fd2_field_unit *attacker,
                                           uint8_t *bonus) {
    if (!bonus) return -1;
    const uint8_t *item = equipped_weapon_record(attacker);
    if (!item) return -1;
    *bonus = item[FD2_FIELD_ITEM_EFFECT_TYPE_OFFSET] ==
                     FD2_FIELD_ITEM_CRITICAL_EFFECT
                 ? item[FD2_FIELD_ITEM_EFFECT_VALUE_OFFSET]
                 : 0;
    return 0;
}

int fd2_field_attack_apply_pre_hit_effect(
        const fd2_field_unit *attacker,
        fd2_field_unit *defender,
        fd2_field_combat_rng_fn rng,
        void *rng_userdata,
        int *applied) {
    if (!attacker || !defender || !rng || !applied) return -1;
    *applied = 0;
    const uint8_t *item = equipped_weapon_record(attacker);
    if (!item) return -1;
    if (item[FD2_FIELD_ITEM_EFFECT_TYPE_OFFSET] !=
        FD2_FIELD_ITEM_PRE_HIT_STATUS_EFFECT)
        return 0;

    uint32_t effect_roll = rng(rng_userdata) % 100u;
    if (effect_roll >= item[FD2_FIELD_ITEM_EFFECT_VALUE_OFFSET]) return 0;
    defender->detail_status[0] = (uint8_t)(
        rng(rng_userdata) % FD2_FIELD_PRE_HIT_STATUS_COUNT +
        FD2_FIELD_PRE_HIT_STATUS_BASE);
    *applied = 1;
    return 0;
}

int fd2_field_attack_sequence_count(
        const fd2_field_unit *attacker,
        fd2_field_combat_rng_fn rng,
        void *rng_userdata,
        uint8_t *strike_count) {
    if (!attacker || !rng || !strike_count) return -1;
    const uint8_t *item = equipped_weapon_record(attacker);
    if (!item) return -1;
    uint32_t double_roll = rng(rng_userdata) % 100u;
    *strike_count =
        item[FD2_FIELD_ITEM_EFFECT_TYPE_OFFSET] ==
                    FD2_FIELD_ITEM_DOUBLE_STRIKE_EFFECT ||
                double_roll < FD2_FIELD_DOUBLE_STRIKE_CHANCE
            ? 2
            : 1;
    return 0;
}

int fd2_field_attack_counterattack_is_available(
        const fd2_field_unit *counterattacker,
        const fd2_field_unit *target) {
    if (!counterattacker || !target || counterattacker->detail_status[1] != 0)
        return 0;
    int distance = abs((int)counterattacker->x - (int)target->x) +
                   abs((int)counterattacker->y - (int)target->y);
    if (distance != 1) return 0;
    uint8_t min_range = 0;
    uint8_t max_range = 0;
    return fd2_field_attack_weapon_range(counterattacker, &min_range,
                                         &max_range) == 0 &&
           min_range == 1;
}

int fd2_field_attack_unit_ignores_terrain_modifier(
        const fd2_field_unit *unit) {
    if (!unit || unit->unit_id == FD2_FIELD_TERRAIN_OVERRIDE_UNIT_ID)
        return 0;
    return unit->movement_profile == FD2_FIELD_TERRAIN_IMMUNE_PROFILE ||
           unit->race == FD2_FIELD_TERRAIN_IMMUNE_RACE_1 ||
           unit->race == FD2_FIELD_TERRAIN_IMMUNE_RACE_2;
}

int fd2_field_attack_terrain_modifiers(uint8_t movement_cost_class,
                                       int32_t *attack_percent,
                                       int32_t *defense_percent) {
    static const int32_t attack[] = {5, 0, -5, -5, -5, 0};
    static const int32_t defense[] = {0, 0, 10, 10, -5, 0};
    if (!attack_percent || !defense_percent ||
        movement_cost_class >= sizeof(attack) / sizeof(attack[0]))
        return -1;
    *attack_percent = attack[movement_cost_class];
    *defense_percent = defense[movement_cost_class];
    return 0;
}

int fd2_field_attack_apply_terrain_modifier(uint32_t stat,
                                            int32_t percent,
                                            uint32_t *adjusted) {
    if (!adjusted) return -1;
    int64_t result = (int64_t)stat +
                     ((int64_t)stat * (int64_t)percent) / 100;
    if (result < 0 || result > UINT32_MAX) return -1;
    *adjusted = (uint32_t)result;
    return 0;
}

int fd2_field_attack_base_critical_chance(
        const fd2_field_unit *attacker, uint8_t *base_chance) {
    static const uint8_t critical_by_profile[] = {
        5, 3, 3, 5, 3, 3, 0, 18, 5, 3,
        3, 12, 3, 3, 12, 10, 6, 3, 3, 7,
        3, 3, 30, 18, 0, 0, 0, 0, 0, 0,
    };
    if (!attacker || !base_chance || attacker->movement_profile == 0 ||
        attacker->movement_profile >
            sizeof(critical_by_profile) / sizeof(critical_by_profile[0]))
        return -1;
    *base_chance = critical_by_profile[attacker->movement_profile - 1u];
    return 0;
}

int fd2_field_attack_range_compute(fd2_field_path_result *result,
                                   const fd2_field_map *map,
                                   const fd2_terrain_tileset *terrain,
                                   const fd2_field_unit *attacker) {
    if (!result || !map || !map->cells || map->width <= 0 || map->height <= 0 ||
        !terrain || !terrain->attrs || !attacker ||
        attacker->x >= map->width || attacker->y >= map->height)
        return -1;

    uint8_t min_range = 0;
    uint8_t max_range = 0;
    if (fd2_field_attack_weapon_range(attacker, &min_range, &max_range) != 0)
        return -1;
    const uint8_t *costs =
        fd2_field_movement_profile_get(FD2_FIELD_ATTACK_PROFILE);
    if (!costs) return -1;

    fd2_field_attack_range_context context = {map, terrain, costs};
    if (fd2_field_path_compute(result, map->width, map->height,
                               attacker->x, attacker->y, max_range,
                               attack_range_query, &context) != 0)
        return -1;

    /* 原函数在 profile 传播后另按曼哈顿距离排除 distance < min_range，
     * 不是按累计地形成本做下限判断。 */
    for (int y = 0; y < result->height; y++) {
        for (int x = 0; x < result->width; x++) {
            int dx = x - (int)attacker->x;
            int dy = y - (int)attacker->y;
            if (dx < 0) dx = -dx;
            if (dy < 0) dy = -dy;
            if (dx + dy >= min_range) continue;
            size_t index = (size_t)y * (size_t)result->width + (size_t)x;
            result->nodes[index].distance = FD2_FIELD_PATH_UNREACHABLE;
            result->nodes[index].can_stop = 0;
            result->nodes[index].can_expand = 0;
        }
    }
    return 0;
}

int fd2_field_attack_target_matches_filter(
        const fd2_field_unit *target, fd2_field_target_filter filter) {
    if (!target) return 0;
    switch (filter) {
        case FD2_FIELD_TARGET_SIDE_ZERO:
            return target->side == 0;
        case FD2_FIELD_TARGET_SIDE_NONZERO:
            return target->side != 0;
        case FD2_FIELD_TARGET_SIDE_ONE:
            return target->side == 1;
        case FD2_FIELD_TARGET_SIDE_TWO:
            return target->side == 2;
        default:
            return 0;
    }
}

int fd2_field_attack_target_is_legal_for_filter(
        const fd2_field_map *map,
        const fd2_terrain_tileset *terrain,
        const fd2_field_units *units,
        size_t attacker_index,
        size_t target_index,
        fd2_field_target_filter filter) {
    if (!map || !terrain || !units || attacker_index >= units->count ||
        target_index >= units->count || attacker_index == target_index)
        return 0;
    const fd2_field_unit *attacker = &units->items[attacker_index];
    const fd2_field_unit *target = &units->items[target_index];
    if (attacker->hp == 0 || target->hp == 0 ||
        (attacker->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0 ||
        (target->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0 ||
        !fd2_field_attack_target_matches_filter(target, filter))
        return 0;

    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    int result = fd2_field_attack_range_compute(&range, map, terrain,
                                                attacker);
    int legal = result == 0 &&
                fd2_field_path_is_destination(&range, target->x, target->y);
    fd2_field_path_close(&range);
    return legal;
}

int fd2_field_attack_target_is_legal(const fd2_field_map *map,
                                     const fd2_terrain_tileset *terrain,
                                     const fd2_field_units *units,
                                     size_t attacker_index,
                                     size_t target_index) {
    if (!units || attacker_index >= units->count ||
        units->items[attacker_index].side != 2)
        return 0;
    return fd2_field_attack_target_is_legal_for_filter(
        map, terrain, units, attacker_index, target_index,
        FD2_FIELD_TARGET_SIDE_ZERO);
}
