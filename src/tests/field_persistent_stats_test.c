/* 持久派生依据field_persistent_unit_combat_stats_recompute @0x1145a（原sub_1145A）：只写四个WORD，
 * 不套field_unit_combat_stats_recompute @0x1b750的战场状态修正。 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "field_item.h"
#include "field_unit_base.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    return -1; \
} } while (0)

static void write_word(fd2_field_unit *unit, size_t offset, uint16_t value) {
    uint8_t *raw = (uint8_t *)unit;
    raw[offset] = (uint8_t)value;
    raw[offset + 1u] = (uint8_t)(value >> 8);
}

static fd2_field_unit source_record(void) {
    fd2_field_unit unit;
    uint8_t *raw = (uint8_t *)&unit;
    for (size_t i = 0; i < sizeof(unit); i++) raw[i] = (uint8_t)(i * 37u + 11u);
    for (size_t slot = 0; slot < 8; slot++) {
        raw[0x0a + slot * 2u] = 0x80;
        raw[0x0b + slot * 2u] = 0xff;
    }
    /* 此API没有单位构造／行动能力门，未知ID和现有HP/MP也必须保留。 */
    unit.unit_id = 0xff;
    raw[0x22] = 3;
    raw[0x23] = 5;
    raw[0x24] = 7;
    return unit;
}

static int check_only_derived_changed(fd2_field_unit *unit,
                                    uint16_t attack, uint16_t defense,
                                    uint16_t accuracy, uint16_t evasion) {
    fd2_field_unit expected = *unit;
    write_word(&expected, 0x48, attack);
    write_word(&expected, 0x4a, defense);
    write_word(&expected, 0x4c, accuracy);
    write_word(&expected, 0x4e, evasion);
    CHECK(fd2_field_unit_persistent_stats_recompute(unit) == 0);
    CHECK(memcmp(unit, &expected, sizeof(expected)) == 0);
    return 0;
}

static int test_status_and_negative_words_are_preserved(void) {
    fd2_field_unit unit = source_record();
    write_word(&unit, 0x37, 0xfffe);
    write_word(&unit, 0x39, 0x8000);
    write_word(&unit, 0x3e, 0xfffb);
    CHECK(check_only_derived_changed(&unit, 0xfffe, 0x8000, 0xfffb, 0xfffb) == 0);
    CHECK(fd2_field_unit_persistent_stats_recompute(NULL) == -1);
    return 0;
}

static int test_equipment_wrap_and_hidden_equipped_last_slot(void) {
    const uint8_t *sword = fd2_field_item_record_get(0);
    const uint8_t *armor = fd2_field_item_record_get(132);
    CHECK(sword && armor);
    CHECK(fd2_field_item_i16(sword, 1) == 10 && fd2_field_item_i16(sword, 3) == 95 &&
          fd2_field_item_i16(sword, 5) == 0 && fd2_field_item_i16(sword, 7) == 0);
    CHECK(fd2_field_item_i16(armor, 1) == 0 && fd2_field_item_i16(armor, 3) == 0 &&
          fd2_field_item_i16(armor, 5) == 8 && fd2_field_item_i16(armor, 7) == 0);
    fd2_field_unit unit = source_record();
    uint8_t *raw = (uint8_t *)&unit;
    raw[0x0a] = 0x40; raw[0x0b] = 0;
    raw[0x18] = 0xc0; raw[0x19] = 132;
    write_word(&unit, 0x37, 0xfff8);
    write_word(&unit, 0x39, 0xfffc);
    write_word(&unit, 0x3e, 0xfff0);
    CHECK(check_only_derived_changed(&unit, 2, 4, 79, 0xfff0) == 0);
    return 0;
}

static int test_late_unknown_equipped_id_is_atomic(void) {
    fd2_field_unit unit = source_record();
    uint8_t *raw = (uint8_t *)&unit;
    raw[0x0a] = 0x40; raw[0x0b] = 0;
    write_word(&unit, 0x37, 6);
    write_word(&unit, 0x39, 4);
    write_word(&unit, 0x3e, 2);
    for (size_t i = 0; i < 2; i++) {
        raw[0x18] = 0xc0; raw[0x19] = i ? 0xff : 215;
        fd2_field_unit before = unit;
        CHECK(fd2_field_unit_persistent_stats_recompute(&unit) == -1);
        CHECK(memcmp(&unit, &before, sizeof(unit)) == 0);
    }
    raw[0x18] = 0x80; /* 未装备的未知ID不应被读取；flags80本身不是终止符。 */
    CHECK(check_only_derived_changed(&unit, 16, 4, 97, 2) == 0);
    return 0;
}

int main(void) {
    CHECK(test_status_and_negative_words_are_preserved() == 0);
    CHECK(test_equipment_wrap_and_hidden_equipped_last_slot() == 0);
    CHECK(test_late_unknown_equipped_id_is_atomic() == 0);
    puts("field_persistent_stats: WORD wrap, hidden equipment, status isolation and atomic errors passed");
    return 0;
}
