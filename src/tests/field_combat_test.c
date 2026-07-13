#include "field_combat.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    const uint32_t *values;
    size_t count;
    size_t cursor;
} rng_sequence;

static uint32_t sequence_next(void *userdata) {
    rng_sequence *sequence = userdata;
    if (!sequence || sequence->cursor >= sequence->count) return 0;
    return sequence->values[sequence->cursor++];
}

static int expect(int condition, const char *message) {
    if (condition) return 0;
    fprintf(stderr, "field combat test failed: %s\n", message);
    return -1;
}

static int resolve(const fd2_field_attack_params *params,
                   const uint32_t *values,
                   size_t count,
                   fd2_field_attack_result *result,
                   size_t *draw_count) {
    rng_sequence sequence = {values, count, 0};
    int rc = fd2_field_combat_resolve_attack(
        params, sequence_next, &sequence, result);
    if (draw_count) *draw_count = sequence.cursor;
    return rc;
}

static int test_miss_boundary(void) {
    const fd2_field_attack_params params = {
        .attack = 50, .defense = 20,
        .accuracy = 50, .evasion = 20,
        .critical_chance = 10, .defender_hp = 42,
    };
    const uint32_t rolls[] = {30};
    fd2_field_attack_result result;
    size_t draws;
    if (resolve(&params, rolls, 1, &result, &draws) != 0) return -1;
    return expect(!result.hit && result.damage == 0 &&
                      result.hp_after == 42 && draws == 1 &&
                      result.critical_roll == FD2_FIELD_COMBAT_ROLL_UNUSED &&
                      result.spread_roll == FD2_FIELD_COMBAT_ROLL_UNUSED,
                  "roll == accuracy-evasion must miss");
}

static int test_hit_damage_spread(void) {
    const fd2_field_attack_params params = {
        .attack = 50, .defense = 20,
        .accuracy = 50, .evasion = 20,
        .critical_chance = 10, .defender_hp = 42,
    };
    const uint32_t rolls[] = {29, 99, 5};
    fd2_field_attack_result result;
    size_t draws;
    if (resolve(&params, rolls, 3, &result, &draws) != 0) return -1;
    return expect(result.hit && !result.critical &&
                      result.base_damage == 27 && result.spread_roll == 2 &&
                      result.rolled_damage == 29 && result.damage == 29 && result.hp_after == 13 &&
                      !result.defeated && draws == 3,
                  "normal hit formula or random spread mismatch");
}

static int test_critical_halves_defense(void) {
    const fd2_field_attack_params params = {
        .attack = 50, .defense = 20,
        .accuracy = 100, .evasion = 0,
        .critical_chance = 25, .defender_hp = 50,
    };
    const uint32_t rolls[] = {0, 24, 7};
    fd2_field_attack_result result;
    if (resolve(&params, rolls, 3, &result, NULL) != 0) return -1;
    return expect(result.critical && result.base_damage == 36 &&
                      result.spread_roll == 3 && result.damage == 39 &&
                      result.hp_after == 11,
                  "critical hit must halve defense before base damage");
}

static int test_zero_and_lethal_damage(void) {
    fd2_field_attack_params params = {
        .attack = 10, .defense = 20,
        .accuracy = 100, .evasion = 0,
        .critical_chance = 0, .defender_hp = 8,
    };
    const uint32_t zero_rolls[] = {0, 99};
    fd2_field_attack_result result;
    size_t draws;
    if (resolve(&params, zero_rolls, 2, &result, &draws) != 0) return -1;
    if (expect(result.hit && result.damage == 0 && result.hp_after == 8 &&
                   !result.defeated && draws == 2,
               "attack below defense must not underflow") != 0)
        return -1;

    params.attack = 100;
    params.defense = 0;
    const uint32_t lethal_rolls[] = {0, 99, 8};
    if (resolve(&params, lethal_rolls, 3, &result, NULL) != 0) return -1;
    return expect(result.rolled_damage == 98 && result.damage == 8 &&
                      result.hp_after == 0 && result.defeated,
                  "lethal damage must saturate at current HP");
}

static int test_supported_stat_domain(void) {
    fd2_field_attack_params params = {
        .attack = FD2_FIELD_COMBAT_EFFECTIVE_STAT_MAX,
        .defense = 0,
        .accuracy = FD2_FIELD_COMBAT_ACCURACY_MAX,
        .evasion = 0,
        .critical_chance = UINT8_MAX,
        .defender_hp = UINT16_MAX,
    };
    const uint32_t rolls[] = {0, 99, 0};
    fd2_field_attack_result result;
    if (resolve(&params, rolls, 3, &result, NULL) != 0) return -1;
    if (expect(result.base_damage == 429496729u &&
                   result.rolled_damage == result.base_damage &&
                   result.damage == UINT16_MAX && result.defeated,
               "maximum supported effective stats must remain exact") != 0)
        return -1;

    params.attack = FD2_FIELD_COMBAT_EFFECTIVE_STAT_MAX + 1u;
    if (expect(resolve(&params, rolls, 3, &result, NULL) == -1,
               "overflow-prone attack value must be rejected") != 0)
        return -1;
    params.attack = 1;
    params.accuracy = (uint32_t)FD2_FIELD_COMBAT_ACCURACY_MAX + 1u;
    if (expect(resolve(&params, rolls, 3, &result, NULL) == -1,
               "out-of-domain accuracy must be rejected") != 0)
        return -1;
    params.accuracy = 1;
    params.critical_chance = (uint32_t)UINT8_MAX + 1u;
    return expect(resolve(&params, rolls, 3, &result, NULL) == -1,
                  "critical chance wider than original byte must be rejected");
}

static int test_negative_hit_chance_and_invalid_input(void) {
    const fd2_field_attack_params params = {
        .attack = 50, .defense = 0,
        .accuracy = 5, .evasion = 10,
        .critical_chance = 100, .defender_hp = 20,
    };
    const uint32_t rolls[] = {0};
    fd2_field_attack_result result;
    if (resolve(&params, rolls, 1, &result, NULL) != 0) return -1;
    if (expect(!result.hit, "negative hit chance must always miss") != 0)
        return -1;
    if (expect(fd2_field_combat_resolve_attack(NULL, sequence_next, NULL,
                                               &result) == -1,
               "NULL params must be rejected") != 0)
        return -1;
    if (expect(fd2_field_combat_resolve_attack(&params, NULL, NULL,
                                               &result) == -1,
               "NULL RNG must be rejected") != 0)
        return -1;
    return expect(fd2_field_combat_resolve_attack(&params, sequence_next,
                                                  NULL, NULL) == -1,
                  "NULL output must be rejected");
}

int main(void) {
    if (test_miss_boundary() != 0 ||
        test_hit_damage_spread() != 0 ||
        test_critical_halves_defense() != 0 ||
        test_zero_and_lethal_damage() != 0 ||
        test_supported_stat_domain() != 0 ||
        test_negative_hit_chance_and_invalid_input() != 0)
        return 1;
    puts("field combat tests passed");
    return 0;
}
