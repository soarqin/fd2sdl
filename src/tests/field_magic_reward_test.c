/* 共享HP恢复、基础法术结果／共享贡献回归，不开窗口。
 * 依据sub_1C81F @0x1c81f、sub_1C916 @0x1c916、sub_1E292 @0x1e292
 * （原登记同名），以及18d8c归一／117e7 cap99。
 * 原版enemy表VA0x61af9由既有strict LE loader提取后逐条核对既有9字节。
 * 本测试锁效果贡献与caster归一；升级分段由game专用回归，不声明FIGANI时序。 */
#include <stdio.h>
#include <string.h>
#include "field_magic.h"
#include "field_unit_base.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

typedef struct {
    uint32_t values[2];
    unsigned calls;
    const fd2_field_unit *watch;
    uint16_t hp_before, mp_before;
    int changed_before_rng;
} test_rng;

static uint32_t next_roll(void *userdata) {
    test_rng *rng = userdata;
    if (rng->watch && (rng->watch->hp != rng->hp_before ||
                      rng->watch->mp != rng->mp_before))
        rng->changed_before_rng = 1;
    uint32_t value = rng->values[rng->calls % 2u];
    rng->calls++;
    return value;
}
static fd2_field_unit unit(uint8_t id, uint8_t profile,
                            uint8_t level, uint16_t hp, uint16_t maximum) {
    fd2_field_unit result;
    memset(&result, 0x5a, sizeof(result));
    result.flags = 0;
    result.unit_id = id;
    result.movement_profile = profile;
    result.level = level;
    result.hp = hp;
    result.hp_max = maximum;
    result.mp = result.mp_max = 100;
    result.experience = 0;
    return result;
}

static int test_shared_hp_heal_power_and_write_set(void) {
    const uint16_t powers[] = {0, 1, 25, 40, 70, UINT16_MAX};
    const uint16_t hp_cases[][2] = {
        {0, 42}, {20, 42}, {42, 42}, {1, UINT16_MAX}, {UINT16_MAX, UINT16_MAX},
        /* 纯HP层保留既有夹顶行为，不新增调用方的数据支持门。 */
        {100, 42}, {1, 0}
    };
    CHECK(fd2_field_magic_u16(fd2_field_magic_record_get(13), 0) == 70);
    for (size_t p = 0; p < sizeof(powers) / sizeof(powers[0]); p++) {
        for (size_t h = 0; h < sizeof(hp_cases) / sizeof(hp_cases[0]); h++) {
            for (uint32_t spread = 0; spread < 100; spread++) {
                fd2_field_unit target = unit(255, 255, 0, hp_cases[h][0], hp_cases[h][1]);
                target.flags = 0x85;
                target.experience = 99;
                fd2_field_unit expected = target;
                uint64_t healed = (uint64_t)target.hp + (uint64_t)powers[p] * 9u / 10u +
                                  (uint64_t)spread * powers[p] / 1000u;
                expected.hp = (uint16_t)(healed > target.hp_max ? target.hp_max : healed);
                test_rng rng = {{spread + 300u, 0}, 0, &target, target.hp, target.mp, 0};
                CHECK(fd2_field_hp_heal_apply(&target, powers[p], next_roll, &rng) == 1);
                CHECK(rng.calls == 1 && !rng.changed_before_rng);
                CHECK(memcmp(&target, &expected, sizeof(target)) == 0);
            }
        }
    }
    fd2_field_unit target = unit(0, 1, 1, 0, 100);
    test_rng rng = {{UINT32_MAX, 0}, 0, &target, target.hp, target.mp, 0};
    CHECK(fd2_field_hp_heal_apply(&target, 40, next_roll, &rng) == 1);
    CHECK(target.hp == 39 && rng.calls == 1 && !rng.changed_before_rng);
    fd2_field_unit before = target;
    rng.calls = 0;
    CHECK(fd2_field_hp_heal_apply(NULL, 40, next_roll, &rng) == -1);
    CHECK(fd2_field_hp_heal_apply(&target, 40, NULL, &rng) == -1);
    CHECK(fd2_field_hp_heal_apply(NULL, 40, NULL, NULL) == -1);
    CHECK(rng.calls == 0 && memcmp(&target, &before, sizeof(target)) == 0);
    return 0;
}

static int test_enemy_coefficient_table(void) {
    /* 独立LE提取680B SHA256=bc304be826fe3ff0ff129155da8461263
     * 50852c9b67c6b08a2d7c2d103cc99df；这里锁68个末字节的FNV1a。 */
    uint64_t hash = UINT64_C(14695981039346656037);
    for (unsigned id = 68; id <= 135; id++) {
        uint8_t coefficient = 0;
        CHECK(fd2_field_unit_enemy_experience_coefficient((uint8_t)id, &coefficient) == 0);
        hash = (hash ^ coefficient) * UINT64_C(1099511628211);
    }
    CHECK(hash == UINT64_C(0x4b42ced2a9fd1660));
    const struct { uint8_t id, coefficient; } samples[] = {
        {68,30},{84,180},{104,250},{120,255},{135,1}
    };
    for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); i++) {
        uint8_t coefficient = 0;
        CHECK(fd2_field_unit_enemy_experience_coefficient(samples[i].id, &coefficient) == 0);
        CHECK(coefficient == samples[i].coefficient);
    }
    const uint8_t unknown[] = {0, 32, 67, 136, 255};
    for (size_t i = 0; i < sizeof(unknown); i++) {
        uint8_t coefficient = 0xa5;
        CHECK(fd2_field_unit_enemy_experience_coefficient(unknown[i], &coefficient) == -1);
        CHECK(coefficient == 0xa5);
    }
    CHECK(fd2_field_unit_enemy_experience_coefficient(68, NULL) == -1);
    return 0;
}
static int test_damage_result_and_reward(void) {
    fd2_field_unit target = unit(68, 1, 3, 100, 1000);
    fd2_field_unit before = target;
    test_rng rng = {{89,99},0,&target,100,100,0};
    fd2_field_magic_basic_result result;
    uint32_t maximum = 0;
    CHECK(fd2_field_magic_basic_reward_max(0, &target, &maximum) == 0 && maximum == 4);
    CHECK(fd2_field_magic_apply_player_basic_effect(0, &target, next_roll, &rng, &result) == 1);
    CHECK(result.hit && result.hp_before == 100 && result.hp_after == 51);
    CHECK(result.raw_damage == 49 && result.experience_value == 4);
    CHECK(rng.calls == 2 && !rng.changed_before_rng);
    before.hp = 51;
    CHECK(memcmp(&target, &before, sizeof(target)) == 0);

    target = unit(68, 1, 3, 10, 1000);
    rng = (test_rng){{0,99},0,NULL,0,0,0};
    CHECK(fd2_field_magic_basic_reward_max(0, &target, &maximum) == 0 && maximum == 90);
    CHECK(fd2_field_magic_apply_player_basic_effect(0, &target, next_roll, &rng, &result) == 1);
    CHECK(result.raw_damage == 49 && result.hp_before - result.hp_after == 10);
    CHECK(result.experience_value == 90 && target.hp == 0); /* 死亡给完整level*coef。 */

    target = unit(68, 1, 3, 49, 100);
    CHECK(fd2_field_magic_basic_reward_max(0, &target, &maximum) == 0 && maximum == 90);
    rng = (test_rng){{0,0},0,NULL,0,0,0};
    CHECK(fd2_field_magic_apply_player_basic_effect(0, &target, next_roll, &rng, &result) == 1);
    CHECK(result.raw_damage == 45 && target.hp == 4 && result.experience_value == 40);

    target = unit(68, 1, 3, 100, 100);
    before = target;
    rng = (test_rng){{90,99},0,NULL,0,0,0};
    CHECK(fd2_field_magic_apply_player_basic_effect(0, &target, next_roll, &rng, &result) == 0);
    CHECK(!result.hit && !result.raw_damage && !result.experience_value);
    CHECK(result.hp_before == 100 && result.hp_after == 100 && rng.calls == 1);
    CHECK(memcmp(&target, &before, sizeof(target)) == 0);

    target = unit(67, 1, 3, 100, 100);
    rng = (test_rng){{0,99},0,NULL,0,0,0};
    CHECK(fd2_field_magic_apply_player_basic_effect(0, &target, next_roll, &rng, &result) == 1);
    CHECK(result.raw_damage == 49 && result.experience_value == 0);
    return 0;
}
static int test_heal_weights_and_full_hp(void) {
    const uint8_t profiles[] = {8,9,24,25};
    const uint32_t raw_values[] = {50,806,806,50};
    for (size_t i = 0; i < sizeof(profiles); i++) {
        fd2_field_unit target = unit(74, profiles[i], 2, 10, 100);
        test_rng rng = {{0,0},0,&target,10,100,0};
        fd2_field_magic_basic_result result;
        CHECK(fd2_field_magic_apply_player_basic_effect(13, &target, next_roll, &rng, &result) == 1);
        CHECK(result.hit && result.hp_before == 10 && result.hp_after == 73);
        CHECK(!result.raw_damage && result.experience_value == raw_values[i]);
        CHECK(rng.calls == 1 && !rng.changed_before_rng);
    }
    fd2_field_unit target = unit(9, 9, 2, 95, 100);
    test_rng rng = {{99,0},0,NULL,0,0,0};
    fd2_field_magic_basic_result result;
    CHECK(fd2_field_magic_apply_player_basic_effect(13, &target, next_roll, &rng, &result) == 1);
    CHECK(target.hp == 100 && result.experience_value == 64); /* Δ5*40*32/100，不用raw69。 */
    uint32_t maximum = 1;
    CHECK(fd2_field_magic_basic_reward_max(13, &target, &maximum) == 0 && maximum == 0);
    CHECK(fd2_field_magic_apply_player_basic_effect(13, &target, next_roll, &rng, &result) == 1);
    CHECK(target.hp == 100 && result.experience_value == 0 && rng.calls == 2);
    target = unit(75, 9, 2, 10, 100);
    rng = (test_rng){{0,0},0,NULL,0,0,0};
    CHECK(fd2_field_magic_apply_player_basic_effect(13, &target, next_roll, &rng, &result) == 1);
    CHECK(target.hp == 73 && result.experience_value == 0 && rng.calls == 1);
    return 0;
}
static int test_caster_normalization_caps_and_errors(void) {
    const uint8_t profiles[] = {8,9,24,25};
    const uint32_t gains[] = {25,1,1,1};
    for (size_t i = 0; i < sizeof(profiles); i++) {
        fd2_field_unit caster = unit(9, profiles[i], 2, 50, 50);
        caster.flags = 1; caster.experience = 99; /* 不把1e292门混入18d8c除法。 */
        fd2_field_unit before = caster;
        uint32_t gain = 0xa5;
        CHECK(fd2_field_magic_reward_normalize(&caster, 50, &gain) == 0 && gain == gains[i]);
        CHECK(memcmp(&caster, &before, sizeof(caster)) == 0);
    }
    fd2_field_unit caster = unit(9, 5, 3, 50, 50);
    uint32_t gain = 0xa5;
    CHECK(fd2_field_magic_reward_normalize(&caster, 1000, &gain) == 0 && gain == 333);
    CHECK(fd2_field_magic_reward_normalize(&caster, 10, &gain) == 0 && gain == 3);
    gain = 0xa5;
    CHECK(fd2_field_magic_reward_normalize(&caster, UINT32_MAX, &gain) == -1 && gain == 0xa5);
    CHECK(fd2_field_magic_reward_normalize(NULL, 0, &gain) == -1 && gain == 0xa5);
    CHECK(fd2_field_magic_reward_normalize(&caster, 0, NULL) == -1);
    caster.level = 0;
    CHECK(fd2_field_magic_reward_normalize(&caster, 0, &gain) == -1 && gain == 0xa5);
    caster.movement_profile = 25;
    CHECK(fd2_field_magic_reward_normalize(&caster, 60, &gain) == 0 && gain == 2);
    return 0;
}
static int test_maximum_query_matches_effect_and_existing_rng(void) {
    for (unsigned magic = 0; magic <= 13; magic += 13) {
        for (unsigned profile = 1; profile <= 28; profile++) {
            const uint16_t initial_hp[] = {10,49,50,100,400};
            for (size_t hp = 0; hp < sizeof(initial_hp) / sizeof(initial_hp[0]); hp++) {
                fd2_field_unit original = unit(magic == 0 ? 68 : 9,
                    (uint8_t)profile, 3, initial_hp[hp], 400);
                uint32_t bound = 0, observed_maximum = 0;
                CHECK(fd2_field_magic_basic_reward_max((uint8_t)magic, &original, &bound) == 0);
                for (unsigned spread = 0; spread < 100; spread++) {
                    fd2_field_unit target = original;
                    fd2_field_units existing = {0};
                    existing.count = 1;
                    existing.items[0] = original;
                    test_rng player_rng = {{magic == 0 ? 0 : spread, spread},0,NULL,0,0,0};
                    test_rng existing_rng = player_rng;
                    fd2_field_magic_basic_result result;
                    CHECK(fd2_field_magic_apply_player_basic_effect((uint8_t)magic,
                        &target, next_roll, &player_rng, &result) == 1);
                    CHECK(fd2_field_magic_apply_known_effect((uint8_t)magic, 0,
                        &existing, next_roll, &existing_rng) == 1);
                    CHECK(memcmp(&target, &existing.items[0], sizeof(target)) == 0);
                    CHECK(player_rng.calls == existing_rng.calls && player_rng.calls == (magic == 0 ? 2u : 1u));
                    CHECK(result.experience_value <= bound);
                    if (result.experience_value > observed_maximum)
                        observed_maximum = result.experience_value;
                }
                CHECK(observed_maximum == bound);
            }
        }
    }
    return 0;
}
static int test_multi_target_preflight_and_no_upgrade_rng(void) {
    fd2_field_unit targets[2] = {unit(68,1,1,100,270),unit(68,1,1,100,270)};
    fd2_field_unit caster = unit(9,5,3,50,50);
    uint64_t raw_maximum = 0;
    for (size_t i = 0; i < 2; i++) {
        uint32_t maximum;
        CHECK(fd2_field_magic_basic_reward_max(0, &targets[i], &maximum) == 0 && maximum == 5);
        raw_maximum += maximum;
    }
    uint32_t gain = 0xa5;
    caster.experience = 97;
    CHECK(fd2_field_magic_reward_normalize(&caster, raw_maximum, &gain) == 0 && gain == 3);
    test_rng rng = {{0,0},0,NULL,0,0,0};
    uint64_t raw_actual = 0;
    for (size_t i = 0; i < 2; i++) {
        fd2_field_magic_basic_result result;
        CHECK(fd2_field_magic_apply_player_basic_effect(0, &targets[i], next_roll, &rng, &result) == 1);
        CHECK(result.experience_value == 5);
        raw_actual += result.experience_value;
    }
    CHECK(fd2_field_magic_reward_normalize(&caster, raw_actual, &gain) == 0 && gain == 3);
    CHECK(caster.experience == 97 && caster.mp == 100 && rng.calls == 4);
    return 0;
}
static int test_rejection_preserves_target_result_and_rng(void) {
    for (int fault = 0; fault < 8; fault++) {
        fd2_field_unit target = unit(68,1,3,50,100);
        uint8_t magic = 0;
        switch (fault) {
            case 0: target.hp_max = 0; break;
            case 1: target.hp = 101; break;
            case 2: target.flags = FD2_FIELD_UNIT_FLAG_HIDDEN; break;
            case 3: target.movement_profile = 0; break;
            case 4: target.movement_profile = 29; break;
            case 5: target.unit_id = 136; break;
            case 6: magic = 1; break;
            case 7: magic = 35; break;
        }
        fd2_field_unit before = target;
        fd2_field_magic_basic_result result, result_before;
        memset(&result, 0xa5, sizeof(result));
        result_before = result;
        test_rng rng = {{0,0},0,NULL,0,0,0};
        uint32_t maximum = 0xa5a5a5a5;
        CHECK(fd2_field_magic_basic_reward_max(magic, &target, &maximum) == -1);
        CHECK(maximum == 0xa5a5a5a5);
        CHECK(fd2_field_magic_apply_player_basic_effect(magic, &target, next_roll, &rng, &result) == -1);
        CHECK(memcmp(&result, &result_before, sizeof(result)) == 0);
        CHECK(memcmp(&target, &before, sizeof(target)) == 0 && rng.calls == 0);
    }
    fd2_field_unit target = unit(68,1,3,50,100);
    fd2_field_magic_basic_result result;
    test_rng rng = {{0,0},0,NULL,0,0,0};
    uint32_t maximum = 0xa5a5a5a5;
    CHECK(fd2_field_magic_basic_reward_max(0, NULL, &maximum) == -1 && maximum == 0xa5a5a5a5);
    CHECK(fd2_field_magic_basic_reward_max(0, &target, NULL) == -1);
    CHECK(fd2_field_magic_apply_player_basic_effect(0, NULL, next_roll, &rng, &result) == -1);
    CHECK(fd2_field_magic_apply_player_basic_effect(0, &target, NULL, &rng, &result) == -1);
    CHECK(fd2_field_magic_apply_player_basic_effect(0, &target, next_roll, &rng, NULL) == -1);
    CHECK(rng.calls == 0);
    return 0;
}
typedef struct { uint32_t *owner; unsigned calls; uint32_t seen[3]; } owner_rng;
static uint32_t read_owner(void *userdata) {
    owner_rng *rng = userdata;
    rng->seen[rng->calls++] = *rng->owner;
    return 0;
}
static int test_shared_contribution_write_points(void) {
    fd2_field_units units = {0}; units.count = 1;
    units.items[0] = unit(68,1,3,100,100);
    units.items[0].detail_status[0] = 0;
    uint32_t value = 7;
    owner_rng rng = {&value,0,{0}};
    CHECK(fd2_field_magic_apply_known_effect_with_reward(26,0,&units,read_owner,&rng,&value) == 1);
    CHECK(units.items[0].hp == 91 && units.items[0].detail_status[0] == 2);
    /* gate/spread先见7，duration已见damage floor(9*90/100)=8。 */
    CHECK(rng.calls == 3 && rng.seen[0] == 7 && rng.seen[1] == 7 && rng.seen[2] == 15);
    CHECK(value == 39); /* 再加8*raw LV，不加profile权重。 */
    units.items[0] = unit(9,9,2,20,100);
    units.items[0].detail_status[0] = 1;
    value = 0; rng = (owner_rng){&value,0,{0}};
    CHECK(fd2_field_magic_apply_known_effect_with_reward(20,0,&units,read_owner,&rng,&value) == 1);
    CHECK(units.items[0].hp == 29 && !units.items[0].detail_status[0]);
    CHECK(value == 115 + 128 && rng.calls == 1);
    units.items[0].hp = 120;
    value = 3; rng = (owner_rng){&value,0,{0}};
    CHECK(fd2_field_hp_heal_apply_with_reward(&units.items[0],0,read_owner,&rng,&value) == 1);
    CHECK(units.items[0].hp == 100 && value == (uint32_t)(3 - 256));
    units.items[0].mp = 120; units.items[0].mp_max = 100;
    value = 3; rng = (owner_rng){&value,0,{0}};
    CHECK(fd2_field_mp_heal_apply_with_reward(&units.items[0],0,read_owner,&rng,&value) == 1);
    CHECK(units.items[0].mp == 100 && value == (uint32_t)(3 - 16)); /* MP无profile+30。 */
    units.items[0] = unit(75,9,2,20,100); value = 3;
    rng = (owner_rng){&value,0,{0}};
    CHECK(fd2_field_hp_heal_apply_with_reward(&units.items[0],10,read_owner,&rng,&value) == 1 && value == 3);
    units.items[0].mp_max = 0; units.items[0].mp = 10;
    CHECK(fd2_field_mp_heal_apply_with_reward(&units.items[0],0,read_owner,&rng,&value) == 1 && value == 3);
    units.items[0] = unit(9,9,2,20,0); value = 3;
    fd2_field_unit before = units.items[0]; rng.calls = 0;
    CHECK(fd2_field_hp_heal_apply_with_reward(&units.items[0],10,read_owner,&rng,&value) == -1);
    CHECK(value == 3 && rng.calls == 0 && memcmp(&before,&units.items[0],sizeof(before)) == 0);
    units.items[0].detail_status[0] = 0;
    CHECK(fd2_field_magic_apply_known_effect_with_reward(20,0,&units,read_owner,&rng,&value) == 0);
    CHECK(value == 3 && rng.calls == 0);
    return 0;
}
int main(void) {
    CHECK(test_shared_hp_heal_power_and_write_set() == 0);
    CHECK(test_shared_contribution_write_points() == 0);
    CHECK(test_enemy_coefficient_table() == 0);
    CHECK(test_damage_result_and_reward() == 0);
    CHECK(test_heal_weights_and_full_hp() == 0);
    CHECK(test_caster_normalization_caps_and_errors() == 0);
    CHECK(test_maximum_query_matches_effect_and_existing_rng() == 0);
    CHECK(test_multi_target_preflight_and_no_upgrade_rng() == 0);
    CHECK(test_rejection_preserves_target_result_and_rng() == 0);
    puts("field magic reward tests: ok");
    return 0;
}
