/* field_player_magic_list_select_step @0x1d51d（原登记sub_1D51D）的纯按键回归。
 * 不执行16c57等待／UI共享RNG，不创建窗口；状态码与确认ID分开验证。 */
#include <stdio.h>
#include <string.h>

#include "field_magic.h"
#include "field_unit_base.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    return 1; } } while (0)

static void set_magic(fd2_field_unit *unit, unsigned id) {
    unit->data_09_1e[0x11u + id / 8u] |= (uint8_t)(1u << (id % 8u));
}

static void init_unit(fd2_field_unit *unit, unsigned count) {
    memset(unit, 0x5a, sizeof(*unit));
    memset(unit->data_09_1e + 0x11, 0, 5);
    unit->mp = UINT16_MAX;
    for (unsigned i = 0; i < count; i++) set_magic(unit, i);
}

static int step(const fd2_field_unit *unit, int index, fd2_field_magic_list_key key,
                 int result, int expected_index, uint8_t expected_id) {
    fd2_field_unit before = *unit;
    uint8_t confirmed = 0xa5;
    CHECK(fd2_field_magic_list_select_step(unit, &index, key, &confirmed) == result);
    CHECK(index == expected_index);
    CHECK(confirmed == expected_id);
    CHECK(memcmp(unit, &before, sizeof(before)) == 0);
    return 0;
}

static int test_navigation_boundaries(void) {
    const struct { unsigned count; int index; fd2_field_magic_list_key key; int expected; } cases[] = {
        {1,0,FD2_FIELD_MAGIC_LIST_KEY_UP,0},
        {1,0,FD2_FIELD_MAGIC_LIST_KEY_DOWN,0},
        {1,0,FD2_FIELD_MAGIC_LIST_KEY_LEFT,0},
        {1,0,FD2_FIELD_MAGIC_LIST_KEY_RIGHT,0},
        {4,0,FD2_FIELD_MAGIC_LIST_KEY_UP,3},
        {4,3,FD2_FIELD_MAGIC_LIST_KEY_DOWN,0},
        {4,2,FD2_FIELD_MAGIC_LIST_KEY_UP,1},
        {4,0,FD2_FIELD_MAGIC_LIST_KEY_RIGHT,0},
        {5,0,FD2_FIELD_MAGIC_LIST_KEY_UP,4},
        {5,4,FD2_FIELD_MAGIC_LIST_KEY_DOWN,0},
        {5,0,FD2_FIELD_MAGIC_LIST_KEY_RIGHT,4},
        {5,1,FD2_FIELD_MAGIC_LIST_KEY_RIGHT,1},
        {5,4,FD2_FIELD_MAGIC_LIST_KEY_LEFT,0},
        {5,3,FD2_FIELD_MAGIC_LIST_KEY_DOWN,4},
        {8,0,FD2_FIELD_MAGIC_LIST_KEY_UP,7},
        {8,7,FD2_FIELD_MAGIC_LIST_KEY_DOWN,0},
        {8,3,FD2_FIELD_MAGIC_LIST_KEY_RIGHT,7},
        {8,4,FD2_FIELD_MAGIC_LIST_KEY_RIGHT,4},
        {8,7,FD2_FIELD_MAGIC_LIST_KEY_LEFT,3},
        {12,0,FD2_FIELD_MAGIC_LIST_KEY_UP,11},
        {12,11,FD2_FIELD_MAGIC_LIST_KEY_DOWN,0},
        {12,3,FD2_FIELD_MAGIC_LIST_KEY_RIGHT,7},
        {12,7,FD2_FIELD_MAGIC_LIST_KEY_RIGHT,11},
        {12,8,FD2_FIELD_MAGIC_LIST_KEY_RIGHT,8},
        {12,11,FD2_FIELD_MAGIC_LIST_KEY_LEFT,7},
        {12,3,FD2_FIELD_MAGIC_LIST_KEY_LEFT,3},
        {36,0,FD2_FIELD_MAGIC_LIST_KEY_UP,35},
        {36,35,FD2_FIELD_MAGIC_LIST_KEY_DOWN,0},
        {36,31,FD2_FIELD_MAGIC_LIST_KEY_RIGHT,35},
        {36,32,FD2_FIELD_MAGIC_LIST_KEY_RIGHT,32},
        {36,35,FD2_FIELD_MAGIC_LIST_KEY_LEFT,31}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fd2_field_unit unit;
        init_unit(&unit, cases[i].count);
        CHECK(step(&unit, cases[i].index, cases[i].key, 0, cases[i].expected, 0xa5) == 0);
    }
    return 0;
}

static int test_sparse_ids_and_real_mp_cost(void) {
    static const struct { unsigned id; uint16_t cost; } cases[] = {
        {0,2}, {2,20}, {13,3}, {31,26}, {35,36}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fd2_field_unit unit;
        init_unit(&unit, 0);
        set_magic(&unit, cases[i].id);
        CHECK(fd2_field_magic_record_get((uint8_t)cases[i].id)[5] == cases[i].cost);
        unit.mp = (uint16_t)(cases[i].cost - 1u);
        CHECK(step(&unit, 0, FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, 0, 0, 0xa5) == 0);
        unit.mp = cases[i].cost;
        CHECK(step(&unit, 0, FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, 1, 0, (uint8_t)cases[i].id) == 0);
        unit.mp = UINT16_MAX;
        CHECK(step(&unit, 0, FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, 1, 0, (uint8_t)cases[i].id) == 0);
    }
    fd2_field_unit unit;
    init_unit(&unit, 1);
    set_magic(&unit, 13);
    set_magic(&unit, 35);
    CHECK(step(&unit, 1, FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, 1, 1, 13) == 0);
    CHECK(step(&unit, 2, FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, 1, 2, 35) == 0);
    return 0;
}

static int test_cancel_other_and_insufficient_mp_are_read_only(void) {
    fd2_field_unit unit;
    init_unit(&unit, 8);
    unit.mp = 0;
    CHECK(step(&unit, 6, FD2_FIELD_MAGIC_LIST_KEY_OTHER, 0, 6, 0xa5) == 0);
    CHECK(step(&unit, 6, FD2_FIELD_MAGIC_LIST_KEY_CANCEL, -1, 6, 0xa5) == 0);
    CHECK(step(&unit, 6, FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, 0, 6, 0xa5) == 0);
    /* MP不足不禁导航，也不改变单位状态字段或移除法术。 */
    CHECK(step(&unit, 6, FD2_FIELD_MAGIC_LIST_KEY_UP, 0, 5, 0xa5) == 0);
    return 0;
}

static int test_invalid_data_and_parameters_preserve_outputs(void) {
    fd2_field_unit unit;
    init_unit(&unit, 0);
    CHECK(step(&unit, 0, FD2_FIELD_MAGIC_LIST_KEY_OTHER, -2, 0, 0xa5) == 0);
    init_unit(&unit, 4);
    CHECK(step(&unit, -1, FD2_FIELD_MAGIC_LIST_KEY_UP, -2, -1, 0xa5) == 0);
    CHECK(step(&unit, 4, FD2_FIELD_MAGIC_LIST_KEY_UP, -2, 4, 0xa5) == 0);
    CHECK(step(&unit, 1, (fd2_field_magic_list_key)-1, -2, 1, 0xa5) == 0);
    CHECK(step(&unit, 1, (fd2_field_magic_list_key)7, -2, 1, 0xa5) == 0);
    for (unsigned id = 36; id < 40; id++) {
        init_unit(&unit, 1);
        set_magic(&unit, id);
        /* 当前选中ID0合法，后项未知仍须先拒绝导航／确认／取消。 */
        CHECK(step(&unit, 0, FD2_FIELD_MAGIC_LIST_KEY_UP, -2, 0, 0xa5) == 0);
        CHECK(step(&unit, 0, FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, -2, 0, 0xa5) == 0);
        CHECK(step(&unit, 0, FD2_FIELD_MAGIC_LIST_KEY_CANCEL, -2, 0, 0xa5) == 0);
    }
    init_unit(&unit, 40);
    CHECK(step(&unit, 0, FD2_FIELD_MAGIC_LIST_KEY_DOWN, -2, 0, 0xa5) == 0);
    init_unit(&unit, 1);
    fd2_field_unit before = unit;
    int selected = 0;
    uint8_t confirmed = 0xa5;
    CHECK(fd2_field_magic_list_select_step(NULL, &selected,
              FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, &confirmed) == -2);
    CHECK(selected == 0 && confirmed == 0xa5);
    CHECK(fd2_field_magic_list_select_step(&unit, NULL,
              FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, &confirmed) == -2);
    CHECK(confirmed == 0xa5);
    CHECK(fd2_field_magic_list_select_step(&unit, &selected,
              FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, NULL) == -2);
    CHECK(selected == 0 && memcmp(&unit, &before, sizeof(unit)) == 0);
    return 0;
}

static uint32_t effect_rng(void *data) { (*(unsigned *)data)++; return 0; }

static int test_boost_native_fpu_floor_vectors_and_wrap(void) {
    static const uint16_t before[] = {0,10,16,20,40,60,100,65535};
    static const uint16_t delta[] = {1,2,3,4,7,10,16,9831};
    for (unsigned id = 17; id <= 18; id++) for (size_t n = 0; n < sizeof(before)/sizeof(before[0]); n++) {
        fd2_field_units units = {0}; units.count = 1;
        units.items[0].attack = units.items[0].defense = before[n];
        unsigned calls = 0;
        CHECK(fd2_field_magic_apply_known_effect((uint8_t)id, 0, &units, effect_rng, &calls) == 1 && calls == 1);
        CHECK((id == 17 ? units.items[0].attack : units.items[0].defense) == (uint16_t)(before[n] + delta[n]));
        CHECK((id == 17 ? units.items[0].defense : units.items[0].attack) == before[n]);
        fd2_field_units snapshot = units;
        CHECK(fd2_field_magic_apply_known_effect((uint8_t)id, 0, &units, effect_rng, &calls) == 0 && calls == 1);
        CHECK(!memcmp(&snapshot, &units, sizeof(units)));
    }
    fd2_field_unit unit = {0}; unit.hp = 5; unit.mp = 3; unit.mp_max = 50;
    unsigned calls = 0;
    CHECK(fd2_field_mp_heal_apply(&unit, 80, effect_rng, &calls) == 1 && unit.mp == 50 && unit.hp == 5 && calls == 1);
    CHECK(fd2_field_mp_heal_apply(&unit, 80, effect_rng, &calls) == 1 && unit.mp == 50 && calls == 2);
    fd2_field_unit snapshot = unit;
    CHECK(fd2_field_mp_heal_apply(&unit, 80, NULL, &calls) == -1 && !memcmp(&snapshot, &unit, sizeof(unit)) && calls == 2);
    return 0;
}

static int test_cures_heal_before_clear_and_require_rng_only_when_active(void) {
    for (unsigned id = 20; id <= 21; id++) {
        fd2_field_units units = {0}; units.count = 1;
        fd2_field_unit *unit = &units.items[0]; unit->hp = 20; unit->hp_max = 50;
        unit->detail_status[id-20] = 7;
        unsigned calls = 0; fd2_field_unit before = *unit;
        CHECK(fd2_field_magic_apply_known_effect((uint8_t)id,0,&units,NULL,NULL) == -1);
        CHECK(!memcmp(&before, unit, sizeof(before)));
        CHECK(fd2_field_magic_apply_known_effect((uint8_t)id,0,&units,effect_rng,&calls) == 1);
        CHECK(unit->hp == 29 && !unit->detail_status[id-20] && calls == 1);
        before = *unit;
        CHECK(fd2_field_magic_apply_known_effect((uint8_t)id,0,&units,NULL,NULL) == 0 && !memcmp(&before,unit,sizeof(before)) && calls == 1);
    }
    return 0;
}

typedef struct {
    unsigned calls;
    uint32_t values[3];
    const fd2_field_unit *target;
    const uint32_t *owner;
    uint32_t expected_damage_reward;
    int wrong_write_order;
} result_rng_state;

static uint32_t result_rng(void *data) {
    result_rng_state *state = data;
    if (state->calls == 2 && state->target &&
        (state->target->hp != 11 || *state->owner != state->expected_damage_reward ||
         state->target->detail_status[2] != 0)) state->wrong_write_order = 1;
    return state->values[state->calls++];
}

static int test_extended_results_preserve_core_write_order_and_raw_amounts(void) {
    fd2_field_units units = {0}; units.count = 1;
    fd2_field_unit *unit = &units.items[0];
    unit->unit_id = 9; unit->movement_profile = 1; unit->level = 3;
    unit->hp = 1000; unit->hp_max = 1000;
    result_rng_state state = {0};
    fd2_field_magic_basic_result effect;
    uint32_t owner = 0;
    CHECK(fd2_field_magic_apply_effect_result(32, 0, &units, -1,
        result_rng, &state, &owner, &effect) == 1);
    CHECK(state.calls == 2 && unit->hp == 280 && effect.raw_damage == 720 && effect.hit);
    CHECK(effect.hp_before == 1000 && effect.hp_after == 280 && !effect.raw_heal);
    state = (result_rng_state){0}; unit->hp = 1000;
    CHECK(fd2_field_magic_apply_effect_result(13, 0, &units, 800,
        result_rng, &state, &owner, &effect) == 1);
    CHECK(state.calls == 1 && unit->hp == 1000 && effect.raw_heal == 720 && effect.experience_value == 0);
    state = (result_rng_state){0}; unit->attack = 100;
    CHECK(fd2_field_magic_apply_effect_result(17, 0, &units, -1,
        result_rng, &state, &owner, &effect) == 1);
    CHECK(unit->attack == 116 && effect.raw_heal == 16 && effect.experience_value == 6 && state.calls == 1);
    state = (result_rng_state){0}; unit->unit_id = 68; unit->hp = 20; unit->hp_max = 100;
    uint8_t coefficient;
    CHECK(fd2_field_unit_enemy_experience_coefficient(68, &coefficient) == 0);
    owner = 7;
    state.target = unit; state.owner = &owner;
    state.expected_damage_reward = 7u + 9u * 3u * coefficient / 100u;
    CHECK(fd2_field_magic_apply_effect_result(22, 0, &units, -1,
        result_rng, &state, &owner, &effect) == 1);
    CHECK(state.calls == 3 && !state.wrong_write_order && unit->detail_status[2] == 2);
    CHECK(owner == state.expected_damage_reward + 24u && effect.raw_damage == 9);
    fd2_field_unit before = *unit; fd2_field_magic_basic_result sentinel;
    memset(&sentinel, 0x5a, sizeof(sentinel)); effect = sentinel;
    CHECK(fd2_field_magic_apply_effect_result(17, 0, &units, 800,
        result_rng, &state, &owner, &effect) == -1);
    CHECK(!memcmp(unit, &before, sizeof(before)) && !memcmp(&effect, &sentinel, sizeof(effect)) && state.calls == 3);
    return 0;
}

static int test_derived_damage_uses_signed_ap_dp_and_caps_only_for_markers(void) {
    fd2_field_unit caster = {0}, target = {0};
    caster.attack = 200; target.unit_id = 9; target.defense = 50;
    target.hp = target.hp_max = 100;
    result_rng_state state = {0}; fd2_field_magic_basic_result effect;
    uint32_t owner = 19;
    CHECK(fd2_field_magic_apply_derived_result(28, &caster, &target,
        result_rng, &state, &owner, &effect) == 1);
    CHECK(state.calls == 1 && target.hp == 0 && effect.raw_damage == 315 && effect.signed_damage == 100);
    CHECK(owner == 19 && caster.attack == 200);
    caster.attack = 10; target.defense = 30; target.hp = 90;
    state = (result_rng_state){0}; state.values[0] = 50;
    CHECK(fd2_field_magic_apply_derived_result(29, &caster, &target,
        result_rng, &state, &owner, &effect) == 1);
    CHECK(state.calls == 1 && target.hp == 106 && effect.signed_damage == -16 &&
        effect.raw_damage == (uint32_t)-16); /* 1c81f不把负power/超过maxHP夹零。 */
    fd2_field_unit before = target;
    CHECK(fd2_field_magic_apply_derived_result(27, &caster, &target,
        result_rng, &state, &owner, &effect) == -1);
    CHECK(state.calls == 1 && !memcmp(&target, &before, sizeof(target)));
    return 0;
}

static int test_item_heal_result_keeps_raw_popup_and_signed_mp_reward(void) {
    fd2_field_unit target = {0};
    target.unit_id = 9; target.level = 3; target.movement_profile = 12;
    target.hp = 17; target.hp_max = 100; target.mp = 98; target.mp_max = 100;
    result_rng_state state = {0}; fd2_field_magic_basic_result result;
    uint32_t owner = 11;
    CHECK(fd2_field_magic_apply_heal_result(&target, 20, 1, result_rng,
        &state, &owner, &result) == 1);
    CHECK(state.calls == 1 && target.mp == 100 && target.hp == 17 && result.raw_heal == 18 &&
        result.hp_before == 17 && result.hp_after == 17 && result.hit &&
        result.experience_value == 2 && owner == 13); /* MP贡献只用level，不加转职30。 */
    target.mp = 110;
    CHECK(fd2_field_magic_apply_heal_result(&target, 20, 1, result_rng,
        &state, &owner, &result) == 1);
    CHECK(state.calls == 2 && target.mp == 100 && result.raw_heal == 18 &&
        result.experience_value == (uint32_t)-12 && owner == 1);
    fd2_field_unit before = target;
    memset(&result, 0x5a, sizeof(result)); fd2_field_magic_basic_result sentinel = result;
    target.mp_max = 0; before = target;
    CHECK(fd2_field_magic_apply_heal_result(&target, 20, 1, result_rng,
        &state, &owner, &result) == -1);
    CHECK(state.calls == 2 && owner == 1 && !memcmp(&target, &before, sizeof(before)) &&
        !memcmp(&result, &sentinel, sizeof(result)));
    return 0;
}

int main(void) {
    CHECK(test_item_heal_result_keeps_raw_popup_and_signed_mp_reward() == 0);
    CHECK(test_extended_results_preserve_core_write_order_and_raw_amounts() == 0);
    CHECK(test_derived_damage_uses_signed_ap_dp_and_caps_only_for_markers() == 0);
    CHECK(test_cures_heal_before_clear_and_require_rng_only_when_active() == 0);
    CHECK(test_boost_native_fpu_floor_vectors_and_wrap() == 0);
    CHECK(test_navigation_boundaries() == 0);
    CHECK(test_sparse_ids_and_real_mp_cost() == 0);
    CHECK(test_cancel_other_and_insufficient_mp_are_read_only() == 0);
    CHECK(test_invalid_data_and_parameters_preserve_outputs() == 0);
    puts("field_magic_test: ok");
    return 0;
}
