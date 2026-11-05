/* B4纯逻辑回归：field_unit_experience_reward_execute @0x1e292（原sub_1E292）、
 * field_growth_stat_present_apply @0x1e529（原sub_1E529）与
 * field_unit_magic_bit_set @0x1d79c（原sub_1D79C）；不代替提示/等待/RNG/原版运行验收。 */
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_unit_base.h"

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); exit(1); \
} } while (0)

static unsigned int groups;
static void put16(fd2_field_unit *unit, size_t offset, uint16_t value) {
    uint8_t *raw = (uint8_t *)unit;
    raw[offset] = (uint8_t)value; raw[offset + 1u] = (uint8_t)(value >> 8);
}
static uint16_t get16(const fd2_field_unit *unit, size_t offset) {
    const uint8_t *raw = (const uint8_t *)unit;
    return (uint16_t)(raw[offset] | (uint16_t)raw[offset + 1u] << 8);
}
static fd2_field_unit actor(uint8_t id, uint8_t level, uint8_t experience) {
    fd2_field_unit unit;
    memset(&unit, 0x5a, sizeof(unit));
    unit.unit_id = id; unit.level = level; unit.experience = experience;
    unit.flags = 0x84; unit.side = 2;
    unit.attack_status = unit.defense_status = unit.hit_evasion_status = 0;
    uint8_t *raw = (uint8_t *)&unit;
    for (size_t i = 0; i < 8u; i++) { raw[10u + i * 2u] = 0x80; raw[11u + i * 2u] = 0xff; }
    raw[0x1a] = 1; raw[0x1b] = 2; raw[0x1c] = 4; raw[0x1d] = 8; raw[0x1e] = 0xf0;
    put16(&unit, 0x37, 100); put16(&unit, 0x39, 200); put16(&unit, 0x3e, 300);
    unit.hp = 19; unit.hp_max = 42; unit.mp = 3; unit.mp_max = 8;
    unit.attack = 1000; unit.defense = 2000; unit.accuracy = 3000; unit.evasion = 4000;
    return unit;
}
static fd2_field_unit sentinel(void) { fd2_field_unit value; memset(&value, 0xa5, sizeof(value)); return value; }
static void unchanged(const fd2_field_unit *a, const fd2_field_unit *b) { CHECK(memcmp(a, b, sizeof(*a)) == 0); }

static void test_complete_tables(size_t size, size_t stride, int growth, uint64_t expected) {
    /* 常量来自独立strict LE提取的748/240B，已与Domain读集逐字相同；
     * 表的权威SHA256另保留在实现注释，FNV仅作无资源回归守卫。 */
    uint64_t digest = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < size / stride; i++) {
        const uint8_t *record = growth ? fd2_field_unit_growth_record_get((uint8_t)i) : fd2_field_unit_learning_record_get((uint8_t)i);
        CHECK(record != NULL);
        for (size_t j = 0; j < stride; j++) {
            digest ^= record[j];
            digest *= UINT64_C(1099511628211);
        }
    }
    CHECK(digest == expected);
    CHECK((growth ? fd2_field_unit_growth_record_get((uint8_t)(size / stride)) : fd2_field_unit_learning_record_get((uint8_t)(size / stride))) == NULL);
    CHECK((growth ? fd2_field_unit_growth_record_get(255) : fd2_field_unit_learning_record_get(255)) == NULL);
    groups++;
}

static void test_fixed_upper_and_write_set(void) {
    fd2_field_unit unit = actor(0, 1, 80), before = unit, output = sentinel();
    fd2_field_unit_level_growth plan;
    CHECK(fd2_field_unit_level_growth_prepare(&unit, &plan) == 0);
    const uint8_t expected[] = {8, 6, 3, 12, 0};
    CHECK(plan.level == 2 && plan.learned_count == 0 && memcmp(plan.growth, expected, 5) == 0);
    CHECK(fd2_field_unit_reward_preview(&unit, 20, NULL, NULL, &output) == 1);
    unchanged(&unit, &before);
    CHECK(output.level == 2 && output.experience == 0);
    CHECK(get16(&output, 0x37) == 108 && get16(&output, 0x39) == 206 && get16(&output, 0x3e) == 303);
    CHECK(output.hp == 19 && output.hp_max == 54 && output.mp == 3 && output.mp_max == 8);
    CHECK(output.attack == 108 && output.defense == 206 && output.accuracy == 303 && output.evasion == 303);
    uint8_t *actual = (uint8_t *)&output;
    const uint8_t *original = (const uint8_t *)&before;
    for (size_t i = 0; i < sizeof(unit); i++) {
        int written = i == 0x21 || i == 0x3c || (i >= 0x37 && i <= 0x3a) ||
            (i >= 0x3e && i <= 0x3f) || (i >= 0x42 && i <= 0x43) || (i >= 0x46);
        if (!written) CHECK(actual[i] == original[i]);
    }
    groups++;
}

static void test_nonlevel_and_entry_gates(void) {
    fd2_field_unit unit = actor(9, 1, 79), output = sentinel(), before = unit;
    CHECK(fd2_field_unit_reward_preview(&unit, 20, NULL, NULL, &output) == 1);
    before.experience = 99; unchanged(&output, &before); /* 不重算陈旧派生。 */
    const uint8_t ids[] = {0, 30, 31};
    const uint8_t limits[] = {40, 99, 99};
    for (size_t i = 0; i < 3u; i++) {
        unit = actor(ids[i], limits[i], 99); output = sentinel(); before = output;
        CHECK(fd2_field_unit_reward_preview(&unit, 1000, NULL, NULL, &output) == 0); unchanged(&output, &before);
    }
    unit = actor(9, 4, 90); before = sentinel(); output = before;
    CHECK(fd2_field_unit_reward_preview(&unit, 0, NULL, NULL, &output) == 0); unchanged(&output, &before);
    unit.flags |= 1; unit.unit_id = 255;
    CHECK(fd2_field_unit_reward_preview(&unit, UINT32_MAX, NULL, NULL, &output) == 0); unchanged(&output, &before);
    unit = actor(0, 1, 100);
    CHECK(fd2_field_unit_reward_preview(&unit, 1, NULL, NULL, &output) == 1);
    CHECK(output.level == 2 && output.experience == 1);
    unit = actor(0, 1, 255);
    CHECK(fd2_field_unit_reward_preview(&unit, 1, NULL, NULL, &output) == 1);
    CHECK(output.level == 3 && output.experience == 56);
    groups++;
}

static void test_native_loop_boundaries(void) {
    fd2_field_unit unit = actor(0, 29, 99), output;
    CHECK(fd2_field_unit_reward_preview(&unit, 250, NULL, NULL, &output) == 1);
    CHECK(output.level == 30 && output.experience == 0 && get16(&output, 0x37) == 108);
    unit = actor(0, 30, 80);
    CHECK(fd2_field_unit_reward_preview(&unit, 20, NULL, NULL, &output) == 1);
    CHECK(output.level == 31 && output.experience == 0);
    unit = actor(0, 39, 90);
    CHECK(fd2_field_unit_reward_preview(&unit, 250, NULL, NULL, &output) == 1);
    CHECK(output.level == 42 && output.experience == 40 && get16(&output, 0x37) == 124);
    unit = actor(0, 41, 90);
    CHECK(fd2_field_unit_reward_preview(&unit, 20, NULL, NULL, &output) == 1);
    CHECK(output.level == 42 && output.experience == 10); /* 不加>=40门。 */
    for (uint8_t id = 30; id <= 31; id++) {
        unit = actor(id, 98, 99);
        CHECK(fd2_field_unit_reward_preview(&unit, 250, NULL, NULL, &output) == 1);
        CHECK(output.level == 99 && output.experience == 0);
        unit = actor(id, 100, 80);
        CHECK(fd2_field_unit_reward_preview(&unit, 20, NULL, NULL, &output) == 1);
        CHECK(output.level == 101 && output.experience == 0);
    }
    unit = actor(0, 255, 80);
    CHECK(fd2_field_unit_reward_preview(&unit, 20, NULL, NULL, &output) == 1);
    CHECK(output.level == 0 && output.experience == 0);
    groups++;
}

static void test_learning_and_five_byte_preservation(void) {
    fd2_field_unit unit = actor(9, 4, 95), before = unit, output;
    fd2_field_unit_level_growth plan;
    CHECK(fd2_field_unit_level_growth_prepare(&unit, &plan) == 0);
    CHECK(plan.level == 5 && plan.learned_count == 1 && plan.learned_magic[0] == 17);
    CHECK(fd2_field_unit_reward_preview(&unit, 5, NULL, NULL, &output) == 1);
    CHECK(output.level == 5 && output.experience == 0);
    const uint8_t *raw = (const uint8_t *)&output;
    const uint8_t *old = (const uint8_t *)&before;
    for (size_t i = 0; i < 5u; i++) CHECK(raw[0x1a + i] == (uint8_t)(old[0x1a + i] | (i == 2u ? 2u : 0u)));
    CHECK(output.hp == before.hp && output.mp == before.mp && output.hp_max == 51 && output.mp_max == 16);
    unchanged(&unit, &before);
    /* selector17的15/23均学习11：已有位仍产生本级通知，OR本身幂等。 */
    for (uint8_t level = 14; level <= 22; level += 8) {
        unit = actor(28, level, 80); ((uint8_t *)&unit)[0x1b] |= 8;
        CHECK(fd2_field_unit_level_growth_prepare(&unit, &plan) == 0);
        CHECK(plan.learned_count == 1 && plan.learned_magic[0] == 11);
        CHECK(fd2_field_unit_reward_preview(&unit, 20, NULL, NULL, &output) == 1);
        CHECK(((uint8_t *)&output)[0x1b] == ((uint8_t *)&unit)[0x1b]);
    }
    groups++;
}

static void test_word_wrap_equipment_and_alias(void) {
    fd2_field_unit unit = actor(0, 1, 80), output;
    put16(&unit, 0x37, 65535); put16(&unit, 0x39, 65535); put16(&unit, 0x3e, 65535);
    unit.hp_max = 65535; unit.mp_max = 65535;
    uint8_t *raw = (uint8_t *)&unit; raw[10] = 0xc0; raw[11] = 0; /* hidden+equipped也参与重算。 */
    CHECK(fd2_field_unit_reward_preview(&unit, 20, NULL, NULL, &output) == 1);
    CHECK(get16(&output, 0x37) == 7 && get16(&output, 0x39) == 5 && get16(&output, 0x3e) == 2);
    CHECK(output.hp_max == 11 && output.mp_max == 65535 && output.hp == 19 && output.mp == 3);
    CHECK(output.attack == 17 && output.accuracy == 97); /* Sword0: AP10/HIT95。 */
    CHECK(((uint8_t *)&output)[10] == 0xc0 && ((uint8_t *)&output)[11] == 0);
    CHECK(fd2_field_unit_reward_preview(&unit, 20, NULL, NULL, &unit) == 1);
    unchanged(&unit, &output);
    unit = actor(9, 1, 80);
    unit.hp_max = unit.mp_max = 65535;
    CHECK(fd2_field_unit_reward_preview(&unit, 20, NULL, NULL, &output) == 1);
    CHECK(output.hp_max == 8 && output.mp_max == 7 && output.hp == 19 && output.mp == 3);
    groups++;
}

typedef struct { unsigned int calls; int fail_at; } scale_context;
static int scale_stat(void *opaque, fd2_field_unit_scaled_stat stat, int32_t value, int32_t *scaled) {
    scale_context *context = opaque;
    CHECK(stat == FD2_FIELD_UNIT_STAT_ATTACK || stat == FD2_FIELD_UNIT_STAT_DEFENSE);
    context->calls++;
    if ((int)context->calls == context->fail_at) return -1;
    *scaled = value + 7; return 0;
}

static void test_preflight_errors_do_not_write(void) {
    fd2_field_unit unit = actor(9, 4, 95), before, output = sentinel(), output_before = output;
    fd2_field_unit_level_growth plan, old_plan;
    memset(&plan, 0xa5, sizeof(plan)); old_plan = plan;
    CHECK(fd2_field_unit_level_growth_prepare(NULL, &plan) == -1 && memcmp(&plan, &old_plan, sizeof(plan)) == 0);
    CHECK(fd2_field_unit_level_growth_prepare(&unit, NULL) == -1);
    CHECK(fd2_field_unit_reward_preview(NULL, 1, NULL, NULL, &output) == -1); unchanged(&output, &output_before);
    CHECK(fd2_field_unit_reward_preview(&unit, 1, NULL, NULL, NULL) == -1);
    CHECK(fd2_field_unit_reward_preview(&unit, INT32_MAX, NULL, NULL, &output) == -1); unchanged(&output, &output_before);
    for (unsigned id = 68; id <= 255; id++) {
        unit.unit_id = id; before = unit;
        CHECK(fd2_field_unit_level_growth_prepare(&unit, &plan) == -1 && memcmp(&plan, &old_plan, sizeof(plan)) == 0);
        CHECK(fd2_field_unit_reward_preview(&unit, 5, NULL, NULL, &output) == -1);
        unchanged(&unit, &before); unchanged(&output, &output_before);
    }
    for (uint8_t id = 32; id < 68; id++) {
        unit = actor(id, 1, 95); before = unit;
        CHECK(fd2_field_unit_level_growth_prepare(&unit, &plan) == 0);
        CHECK(plan.level == 2);
        CHECK(fd2_field_unit_reward_preview(&unit, 5, NULL, NULL, &output) == 1);
        CHECK(output.level == 2 && output.experience == 0);
        unchanged(&unit, &before);
    }
    output = output_before; plan = old_plan;
    unit = actor(9, 254, 80); before = unit;
    CHECK(fd2_field_unit_level_growth_prepare(&unit, &plan) == -1); /* level255会命中ff/ff，不越界OR。 */
    CHECK(fd2_field_unit_reward_preview(&unit, 20, NULL, NULL, &output) == -1);
    unchanged(&unit, &before); unchanged(&output, &output_before);
    unit = actor(0, 1, 80); ((uint8_t *)&unit)[24] = 0xc0; ((uint8_t *)&unit)[25] = 215; before = unit;
    CHECK(fd2_field_unit_reward_preview(&unit, 20, NULL, NULL, &output) == -1);
    unchanged(&unit, &before); unchanged(&output, &output_before);
    unit = actor(9, 4, 95); unit.attack_status = 2; unit.defense_status = 3; before = unit;
    CHECK(fd2_field_unit_reward_preview(&unit, 5, NULL, NULL, &output) == -1);
    unchanged(&unit, &before); unchanged(&output, &output_before);
    scale_context context = {0, 2};
    CHECK(fd2_field_unit_reward_preview(&unit, 5, scale_stat, &context, &output) == -1);
    CHECK(context.calls == 2); unchanged(&unit, &before); unchanged(&output, &output_before);
    context.calls = 0; context.fail_at = 0;
    CHECK(fd2_field_unit_reward_preview(&unit, 5, scale_stat, &context, &output) == 1);
    CHECK(context.calls == 2 && output.attack == 113 && output.defense == 212);
    CHECK(output.attack_status == 2 && output.defense_status == 3);
    groups++;
}

int main(void) {
    CHECK(sizeof(fd2_field_unit) == 80);
    test_complete_tables(748, 11, 1, UINT64_C(0x12a56491fb722c5a));
    test_complete_tables(240, 12, 0, UINT64_C(0x095482fa653a43a5));
    test_fixed_upper_and_write_set(); test_nonlevel_and_entry_gates();
    test_native_loop_boundaries(); test_learning_and_five_byte_preservation();
    test_word_wrap_equipment_and_alias(); test_preflight_errors_do_not_write();
    printf("field_growth_test: %u groups passed; pure logic, no UI\n", groups);
    return 0;
}
