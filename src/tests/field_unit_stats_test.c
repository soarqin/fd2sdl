#include "field_unit_stats.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    fd2_field_item_stat_effect effects[256];
    uint8_t valid[256];
    int calls;
} item_table;

static int expect(int condition, const char *message) {
    if (condition) return 0;
    fprintf(stderr, "field unit stats test failed: %s\n", message);
    return -1;
}

static void write_i16(fd2_field_unit *unit, size_t offset, int16_t value) {
    memcpy((uint8_t *)unit + offset, &value, sizeof(value));
}

static void set_base(fd2_field_unit *unit,
                     int16_t attack, int16_t defense, int16_t accuracy) {
    memset(unit, 0, sizeof(*unit));
    write_i16(unit, 0x37, attack);
    write_i16(unit, 0x39, defense);
    write_i16(unit, 0x3e, accuracy);
}

static int lookup_item(void *userdata, uint8_t item_id,
                       fd2_field_item_stat_effect *effect) {
    item_table *table = userdata;
    table->calls++;
    if (!table->valid[item_id]) return -1;
    *effect = table->effects[item_id];
    return 0;
}

static int half_scale(void *userdata, fd2_field_unit_scaled_stat stat,
                      int32_t value, int32_t *scaled_value) {
    unsigned *mask = userdata;
    *mask |= 1u << (unsigned)stat;
    *scaled_value = value / 2;
    return 0;
}

static int test_base_and_accuracy_boost(void) {
    fd2_field_unit unit;
    set_base(&unit, 30, 20, 40);
    ((uint8_t *)&unit)[0x24] = 1;
    if (fd2_field_unit_combat_stats_recompute(
            &unit, NULL, NULL, NULL, NULL) != 0)
        return -1;
    return expect(unit.attack == 30 && unit.defense == 20 &&
                      unit.accuracy == 55 && unit.evasion == 55,
                  "base fields or shared accuracy/evasion boost mismatch");
}

static int test_equipment_accumulation(void) {
    fd2_field_unit unit;
    item_table table = {0};
    set_base(&unit, 30, 20, 40);
    table.valid[7] = 1;
    table.effects[7] = (fd2_field_item_stat_effect){5, 3, -2, 4};
    table.valid[9] = 1;
    table.effects[9] = (fd2_field_item_stat_effect){-1, 2, 6, -3};
    uint8_t *record = (uint8_t *)&unit;
    record[0x0a] = FD2_FIELD_UNIT_EQUIPPED_FLAG;
    record[0x0b] = 7;
    record[0x0c] = FD2_FIELD_UNIT_EQUIPPED_FLAG | 0x80;
    record[0x0d] = 9;
    record[0x0e] = 0x80; /* 未装备槽不得查询。 */
    record[0x0f] = 99;

    if (fd2_field_unit_combat_stats_recompute(
            &unit, lookup_item, &table, NULL, NULL) != 0)
        return -1;
    return expect(unit.attack == 34 && unit.defense == 24 &&
                      unit.accuracy == 45 && unit.evasion == 41 &&
                      table.calls == 2,
                  "equipped item effects or slot filtering mismatch");
}

static int test_status_scale(void) {
    fd2_field_unit unit;
    unsigned mask = 0;
    set_base(&unit, 31, 21, 40);
    ((uint8_t *)&unit)[0x22] = 1;
    ((uint8_t *)&unit)[0x23] = 1;
    if (fd2_field_unit_combat_stats_recompute(
            &unit, NULL, NULL, half_scale, &mask) != 0)
        return -1;
    return expect(unit.attack == 15 && unit.defense == 10 &&
                      unit.accuracy == 40 && unit.evasion == 40 &&
                      mask == 3,
                  "status scaling callback mismatch");
}

static int test_signed_low16_writeback(void) {
    fd2_field_unit unit;
    set_base(&unit, -1, -32768, -2);
    if (fd2_field_unit_combat_stats_recompute(
            &unit, NULL, NULL, NULL, NULL) != 0)
        return -1;
    return expect(unit.attack == UINT16_MAX && unit.defense == 0x8000 &&
                      unit.accuracy == 0xfffe && unit.evasion == 0xfffe,
                  "signed bases must preserve the original low-16-bit writeback");
}

static int test_failure_is_transactional(void) {
    fd2_field_unit unit;
    item_table table = {0};
    set_base(&unit, 30, 20, 40);
    unit.attack = 1;
    unit.defense = 2;
    unit.accuracy = 3;
    unit.evasion = 4;
    ((uint8_t *)&unit)[0x0a] = FD2_FIELD_UNIT_EQUIPPED_FLAG;
    ((uint8_t *)&unit)[0x0b] = 77;

    if (fd2_field_unit_combat_stats_recompute(
            &unit, lookup_item, &table, NULL, NULL) == 0)
        return -1;
    if (expect(unit.attack == 1 && unit.defense == 2 &&
                   unit.accuracy == 3 && unit.evasion == 4,
               "lookup failure must not partially write derived fields") != 0)
        return -1;

    ((uint8_t *)&unit)[0x0a] = 0;
    ((uint8_t *)&unit)[0x22] = 1;
    return expect(fd2_field_unit_combat_stats_recompute(
                      &unit, NULL, NULL, NULL, NULL) != 0 &&
                      unit.attack == 1 && unit.defense == 2 &&
                      unit.accuracy == 3 && unit.evasion == 4,
                  "missing status scaler must fail transactionally");
}

int main(void) {
    if (test_base_and_accuracy_boost() != 0) return 1;
    if (test_equipment_accumulation() != 0) return 1;
    if (test_status_scale() != 0) return 1;
    if (test_signed_low16_writeback() != 0) return 1;
    if (test_failure_is_transactional() != 0) return 1;
    puts("field unit stats tests passed");
    return 0;
}
