/* field_target_range_build @0x39a2c 普通攻击范围测试。 */
#include <stdio.h>
#include <string.h>

#include "field_attack.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "field attack test failed at line %d: %s\n", \
                __LINE__, #expr); \
        return 1; \
    } \
} while (0)

typedef struct {
    const uint32_t *values;
    size_t count;
    size_t cursor;
} effect_rng;

static uint32_t effect_rng_next(void *userdata) {
    effect_rng *rng = userdata;
    if (!rng || rng->cursor >= rng->count) return 0;
    return rng->values[rng->cursor++];
}

static void equip_weapon(fd2_field_unit *unit, uint8_t item_id) {
    uint8_t *record = (uint8_t *)unit;
    record[0x0a] = 0x40;
    record[0x0b] = item_id;
}

int main(void) {
    uint32_t cells[25] = {0};
    fd2_terrain_attr attrs[2] = {{0}};
    attrs[0].movement_cost_class = 1; /* attack profile 0 cost 1 */
    attrs[1].movement_cost_class = 0; /* attack profile 0 cost 0x16 */
    fd2_field_map map = {5, 5, cells};
    fd2_terrain_tileset terrain = {0};
    terrain.attrs = attrs;
    terrain.attr_count = 2;

    fd2_field_unit spear = {0};
    spear.side = 2;
    spear.x = 2;
    spear.y = 2;
    spear.hp = 10;
    equip_weapon(&spear, 20); /* 刺矛：record +0x0b/+0x0c = 1/2 */

    uint8_t min_range = 0;
    uint8_t max_range = 0;
    CHECK(fd2_field_attack_weapon_range(&spear, &min_range, &max_range) == 0);
    CHECK(min_range == 1 && max_range == 2);
    uint8_t critical_bonus = 0xff;
    CHECK(fd2_field_attack_weapon_critical_bonus(&spear,
                                                 &critical_bonus) == 0);
    CHECK(critical_bonus == 0);

    fd2_field_unit critical_weapon = {0};
    equip_weapon(&critical_weapon, 7); /* item +0x09/+0x0a = 4/30 */
    CHECK(fd2_field_attack_weapon_critical_bonus(&critical_weapon,
                                                 &critical_bonus) == 0);
    CHECK(critical_bonus == 30);

    fd2_field_unit status_weapon = {0};
    fd2_field_unit status_target = {0};
    equip_weapon(&status_weapon, 4); /* item effect type/value = 2/10 */
    const uint32_t effect_rolls[] = {9, 3};
    effect_rng status_rng = {
        effect_rolls, sizeof(effect_rolls) / sizeof(effect_rolls[0]), 0};
    int effect_applied = 0;
    CHECK(fd2_field_attack_apply_pre_hit_effect(
              &status_weapon, &status_target, effect_rng_next,
              &status_rng, &effect_applied) == 0);
    CHECK(effect_applied && status_target.detail_status[0] == 5);
    CHECK(status_rng.cursor == 2);

    const uint32_t failed_effect_rolls[] = {10};
    status_rng = (effect_rng){failed_effect_rolls, 1, 0};
    status_target.detail_status[0] = 7;
    effect_applied = 1;
    CHECK(fd2_field_attack_apply_pre_hit_effect(
              &status_weapon, &status_target, effect_rng_next,
              &status_rng, &effect_applied) == 0);
    CHECK(!effect_applied && status_target.detail_status[0] == 7);
    CHECK(status_rng.cursor == 1);

    fd2_field_unit double_weapon = {0};
    equip_weapon(&double_weapon, 71); /* item effect type 3 */
    const uint32_t double_rolls[] = {99};
    status_rng = (effect_rng){double_rolls, 1, 0};
    uint8_t strike_count = 0;
    CHECK(fd2_field_attack_sequence_count(
              &double_weapon, effect_rng_next, &status_rng,
              &strike_count) == 0);
    CHECK(strike_count == 2 && status_rng.cursor == 1);

    const uint32_t random_double_rolls[] = {2};
    status_rng = (effect_rng){random_double_rolls, 1, 0};
    CHECK(fd2_field_attack_sequence_count(
              &spear, effect_rng_next, &status_rng, &strike_count) == 0);
    CHECK(strike_count == 2 && status_rng.cursor == 1);
    const uint32_t single_rolls[] = {3};
    status_rng = (effect_rng){single_rolls, 1, 0};
    CHECK(fd2_field_attack_sequence_count(
              &spear, effect_rng_next, &status_rng, &strike_count) == 0);
    CHECK(strike_count == 1 && status_rng.cursor == 1);

    fd2_field_unit counterattacker = {0};
    fd2_field_unit counter_target = {0};
    equip_weapon(&counterattacker, 0);
    counterattacker.x = 1;
    counterattacker.y = 1;
    counter_target.x = 2;
    counter_target.y = 1;
    CHECK(fd2_field_attack_counterattack_is_available(
        &counterattacker, &counter_target));
    counterattacker.detail_status[1] = 1;
    CHECK(!fd2_field_attack_counterattack_is_available(
        &counterattacker, &counter_target));
    counterattacker.detail_status[1] = 0;
    counter_target.x = 3;
    CHECK(!fd2_field_attack_counterattack_is_available(
        &counterattacker, &counter_target));

    int32_t attack_percent = 0;
    int32_t defense_percent = 0;
    CHECK(fd2_field_attack_terrain_modifiers(
              0, &attack_percent, &defense_percent) == 0);
    CHECK(attack_percent == 5 && defense_percent == 0);
    CHECK(fd2_field_attack_terrain_modifiers(
              1, &attack_percent, &defense_percent) == 0);
    CHECK(attack_percent == 0 && defense_percent == 0);
    CHECK(fd2_field_attack_terrain_modifiers(
              2, &attack_percent, &defense_percent) == 0);
    CHECK(attack_percent == -5 && defense_percent == 10);
    CHECK(fd2_field_attack_terrain_modifiers(
              3, &attack_percent, &defense_percent) == 0);
    CHECK(attack_percent == -5 && defense_percent == 10);
    CHECK(fd2_field_attack_terrain_modifiers(
              4, &attack_percent, &defense_percent) == 0);
    CHECK(attack_percent == -5 && defense_percent == -5);
    CHECK(fd2_field_attack_terrain_modifiers(
              5, &attack_percent, &defense_percent) == 0);
    CHECK(attack_percent == 0 && defense_percent == 0);
    CHECK(fd2_field_attack_terrain_modifiers(
              6, &attack_percent, &defense_percent) == -1);
    uint32_t adjusted = 0;
    CHECK(fd2_field_attack_apply_terrain_modifier(200, -5, &adjusted) == 0);
    CHECK(adjusted == 190);
    CHECK(fd2_field_attack_apply_terrain_modifier(100, 10, &adjusted) == 0);
    CHECK(adjusted == 110);

    fd2_field_unit terrain_unit = {0};
    terrain_unit.movement_profile = 19;
    CHECK(fd2_field_attack_unit_ignores_terrain_modifier(&terrain_unit));
    terrain_unit.movement_profile = 0;
    terrain_unit.race = 4;
    CHECK(fd2_field_attack_unit_ignores_terrain_modifier(&terrain_unit));
    terrain_unit.race = 5;
    CHECK(fd2_field_attack_unit_ignores_terrain_modifier(&terrain_unit));
    terrain_unit.unit_id = 28;
    CHECK(!fd2_field_attack_unit_ignores_terrain_modifier(&terrain_unit));

    fd2_field_unit critical_unit = {0};
    uint8_t base_critical = 0;
    critical_unit.movement_profile = 1;
    CHECK(fd2_field_attack_base_critical_chance(
              &critical_unit, &base_critical) == 0);
    CHECK(base_critical == 5);
    critical_unit.movement_profile = 8;
    CHECK(fd2_field_attack_base_critical_chance(
              &critical_unit, &base_critical) == 0);
    CHECK(base_critical == 18);
    critical_unit.movement_profile = 23;
    CHECK(fd2_field_attack_base_critical_chance(
              &critical_unit, &base_critical) == 0);
    CHECK(base_critical == 30);
    critical_unit.movement_profile = 28;
    CHECK(fd2_field_attack_base_critical_chance(
              &critical_unit, &base_critical) == 0);
    CHECK(base_critical == 0);
    critical_unit.movement_profile = 0;
    CHECK(fd2_field_attack_base_critical_chance(
              &critical_unit, &base_critical) == -1);

    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    CHECK(fd2_field_attack_range_compute(&range, &map, &terrain, &spear) == 0);
    CHECK(!fd2_field_path_is_destination(&range, 2, 2));
    CHECK(fd2_field_path_is_destination(&range, 3, 2));
    CHECK(fd2_field_path_is_destination(&range, 4, 2));
    CHECK(!fd2_field_path_is_destination(&range, 4, 3));
    fd2_field_path_close(&range);

    /* 最大范围不是单纯曼哈顿圆：原函数使用 profile 0 传播，昂贵地形
     * 会阻断预算内不可绕行的目标。 */
    uint32_t corridor_cells[3] = {0, 1, 0};
    fd2_field_map corridor = {3, 1, corridor_cells};
    spear.x = 0;
    spear.y = 0;
    CHECK(fd2_field_attack_range_compute(&range, &corridor, &terrain,
                                         &spear) == 0);
    CHECK(!fd2_field_path_is_destination(&range, 2, 0));
    fd2_field_path_close(&range);

    fd2_field_units units;
    memset(&units, 0, sizeof(units));
    units.count = 3;
    units.items[0] = spear;
    units.items[0].x = 2;
    units.items[0].y = 2;
    units.items[1].side = 0;
    units.items[1].x = 4;
    units.items[1].y = 2;
    units.items[1].hp = 10;
    units.items[2].side = 2;
    units.items[2].x = 3;
    units.items[2].y = 2;
    units.items[2].hp = 10;
    CHECK(fd2_field_attack_target_is_legal(&map, &terrain, &units, 0, 1));
    CHECK(!fd2_field_attack_target_is_legal(&map, &terrain, &units, 0, 2));
    CHECK(fd2_field_attack_target_matches_filter(
        &units.items[1], FD2_FIELD_TARGET_SIDE_ZERO));
    CHECK(!fd2_field_attack_target_matches_filter(
        &units.items[1], FD2_FIELD_TARGET_SIDE_NONZERO));
    CHECK(fd2_field_attack_target_matches_filter(
        &units.items[2], FD2_FIELD_TARGET_SIDE_NONZERO));
    CHECK(fd2_field_attack_target_matches_filter(
        &units.items[2], FD2_FIELD_TARGET_SIDE_TWO));
    CHECK(!fd2_field_attack_target_matches_filter(
        &units.items[2], FD2_FIELD_TARGET_SIDE_ONE));

    /* side 0 的未来敌方流程可使用 filter 3 选择玩家 side 2；这里只验证
     * 已确认的通用 range/filter primitive，不推断 AI 决策。 */
    units.items[0].side = 0;
    CHECK(fd2_field_attack_target_is_legal_for_filter(
        &map, &terrain, &units, 0, 2, FD2_FIELD_TARGET_SIDE_TWO));
    units.items[0].side = 2;
    units.items[1].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    CHECK(!fd2_field_attack_target_is_legal(&map, &terrain, &units, 0, 1));

    fd2_field_unit mixed_slots = {0};
    uint8_t *mixed_record = (uint8_t *)&mixed_slots;
    mixed_record[0x0a] = 0x40;
    mixed_record[0x0b] = 132; /* 已装备防具：weapon 模式必须跳过 */
    mixed_record[0x0c] = 0x40;
    mixed_record[0x0d] = 20;
    CHECK(fd2_field_attack_weapon_range(&mixed_slots, &min_range,
                                        &max_range) == 0);
    CHECK(min_range == 1 && max_range == 2);

    fd2_field_unit no_weapon = {0};
    CHECK(fd2_field_attack_weapon_range(&no_weapon, &min_range,
                                        &max_range) != 0);
    return 0;
}
