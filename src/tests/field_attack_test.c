/* field_target_range_build @0x14818 普通攻击范围；
 * field_physical_attack_sequence @0x1e856 与 sub_2EBE1 @0x2ebe1 的
 * 次数分支回归。只验证计数，不是完整演出或原版实机三击验收。 */
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

static int test_sequence_modes(void) {
    fd2_field_unit normal = {0}, type3 = {0}, no_weapon = {0};
    equip_weapon(&normal, 20);
    equip_weapon(&type3, 71);
    /* type3 在 sub_2F7B6 @0x2f7b6 的指令VA 0x2f984 先写标记，0x2f98b 才取命中 RNG。
     * accuracy=0 的必 miss 数值也不能让计数丢掉这次武器追加。 */
    type3.accuracy = 0;
    fd2_field_unit before_normal = normal, before_type3 = type3;
    for (uint32_t roll = 0; roll < 100; roll++) {
        const fd2_field_unit *units[] = {&normal, &type3};
        for (size_t weapon = 0; weapon < 2; weapon++) {
            const uint32_t rolls[] = {roll, UINT32_MAX};
            effect_rng rng = {rolls, 2, 0};
            uint8_t count = 0, maximum = 0;
            CHECK(fd2_field_attack_sequence_max_count_for_mode(units[weapon],
                FD2_FIELD_ATTACK_SEQUENCE_FULL, &maximum) == 0);
            CHECK(maximum == (weapon ? 3 : 2) && rng.cursor == 0);
            CHECK(fd2_field_attack_sequence_count_for_mode(units[weapon],
                FD2_FIELD_ATTACK_SEQUENCE_FULL, effect_rng_next, &rng, &count) == 0);
            CHECK(count == 1u + (roll < 3u) + weapon && count <= maximum &&
                  rng.cursor == 1);
            rng.cursor = 0;
            CHECK(fd2_field_attack_sequence_max_count_for_mode(units[weapon],
                FD2_FIELD_ATTACK_SEQUENCE_SHORT, &maximum) == 0);
            CHECK(maximum == 2 && rng.cursor == 0);
            CHECK(fd2_field_attack_sequence_count_for_mode(units[weapon],
                FD2_FIELD_ATTACK_SEQUENCE_SHORT, effect_rng_next, &rng, &count) == 0);
            CHECK(count == ((roll < 3u || weapon) ? 2 : 1) && count <= maximum &&
                  rng.cursor == 1);
            uint8_t short_count = count;
            rng.cursor = 0;
            CHECK(fd2_field_attack_sequence_count(units[weapon], effect_rng_next,
                                                  &rng, &count) == 0);
            CHECK(count == short_count && count <= 2 && rng.cursor == 1);
        }
    }
    const uint32_t modulo_rolls[] = {102, 99};
    effect_rng rng = {modulo_rolls, 2, 0};
    uint8_t count = 0;
    CHECK(fd2_field_attack_sequence_count_for_mode(&type3,
        FD2_FIELD_ATTACK_SEQUENCE_FULL, effect_rng_next, &rng, &count) == 0);
    CHECK(count == 3 && rng.cursor == 1);
    CHECK(memcmp(&normal, &before_normal, sizeof(normal)) == 0);
    CHECK(memcmp(&type3, &before_type3, sizeof(type3)) == 0);

    rng.cursor = 0;
    count = 0xa5;
    CHECK(fd2_field_attack_sequence_count_for_mode(&type3,
        (fd2_field_attack_sequence_mode)-1, effect_rng_next, &rng, &count) == -1);
    CHECK(fd2_field_attack_sequence_count_for_mode(&type3,
        (fd2_field_attack_sequence_mode)2, effect_rng_next, &rng, &count) == -1);
    CHECK(fd2_field_attack_sequence_count_for_mode(&no_weapon,
        FD2_FIELD_ATTACK_SEQUENCE_FULL, effect_rng_next, &rng, &count) == -1);
    CHECK(fd2_field_attack_sequence_count_for_mode(NULL,
        FD2_FIELD_ATTACK_SEQUENCE_FULL, effect_rng_next, &rng, &count) == -1);
    CHECK(fd2_field_attack_sequence_count_for_mode(&type3,
        FD2_FIELD_ATTACK_SEQUENCE_FULL, NULL, &rng, &count) == -1);
    CHECK(fd2_field_attack_sequence_count_for_mode(&type3,
        FD2_FIELD_ATTACK_SEQUENCE_FULL, effect_rng_next, &rng, NULL) == -1);
    CHECK(fd2_field_attack_sequence_max_count_for_mode(&type3,
        (fd2_field_attack_sequence_mode)-1, &count) == -1);
    CHECK(fd2_field_attack_sequence_max_count_for_mode(&type3,
        (fd2_field_attack_sequence_mode)2, &count) == -1);
    CHECK(fd2_field_attack_sequence_max_count_for_mode(&no_weapon,
        FD2_FIELD_ATTACK_SEQUENCE_FULL, &count) == -1);
    CHECK(fd2_field_attack_sequence_max_count_for_mode(NULL,
        FD2_FIELD_ATTACK_SEQUENCE_FULL, &count) == -1);
    CHECK(fd2_field_attack_sequence_max_count_for_mode(&type3,
        FD2_FIELD_ATTACK_SEQUENCE_FULL, NULL) == -1);
    CHECK(rng.cursor == 0 && count == 0xa5);
    CHECK(memcmp(&type3, &before_type3, sizeof(type3)) == 0);
    return 0;
}

int main(void) {
    CHECK(test_sequence_modes() == 0);
    uint32_t cells[25] = {0};
    fd2_terrain_attr attrs[2] = {{0}};
    attrs[0].movement_cost_class = 0;
    attrs[1].movement_cost_class = 1; /* 正确攻击profile0全20类均为成本1。 */
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

    /* 真实profile0全20类都是1；旧表错位制造出的昂贵地形不应挡住射程2。 */
    uint32_t corridor_cells[3] = {0, 1, 0};
    fd2_field_map corridor = {3, 1, corridor_cells};
    spear.x = 0;
    spear.y = 0;
    for (uint8_t cost_class = 0; cost_class < 20; cost_class++) {
        attrs[1].movement_cost_class = cost_class;
        CHECK(fd2_field_attack_range_compute(&range, &corridor, &terrain,
                                             &spear) == 0);
        CHECK(fd2_field_path_is_destination(&range, 2, 0));
        CHECK(fd2_field_path_distance(&range, 2, 0) == 2);
        fd2_field_path_close(&range);
    }
    attrs[1].movement_cost_class = 1;

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
