#include <stdio.h>
#include <string.h>

#include "field_unit_base.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return -1; \
    } \
} while (0)

static uint16_t rd_u16(const fd2_field_unit *unit, size_t offset) {
    const uint8_t *record = (const uint8_t *)unit;
    return (uint16_t)(record[offset] | ((uint16_t)record[offset + 1] << 8));
}

static int check_unit(uint8_t id, uint8_t level,
                      uint8_t race_code, uint8_t profile, uint8_t movement,
                      uint16_t hp, uint16_t mp,
                      uint16_t attack, uint16_t defense, uint16_t accuracy) {
    fd2_field_unit unit;
    memset(&unit, 0xa5, sizeof(unit));
    unit.unit_id = id;
    CHECK(fd2_field_unit_base_apply(&unit, level) == 0);
    const uint8_t *record = (const uint8_t *)&unit;
    CHECK(record[0x1f] == race_code);
    CHECK(unit.movement_profile == profile);
    CHECK(record[0x21] == level);
    CHECK(unit.movement_points == movement);
    CHECK(unit.hp == hp && unit.hp_max == hp);
    CHECK(unit.mp == mp && unit.mp_max == mp);
    CHECK(rd_u16(&unit, 0x37) == attack);
    CHECK(rd_u16(&unit, 0x39) == defense);
    CHECK(rd_u16(&unit, 0x3e) == accuracy);
    /* 本层不写最终装备派生字段。 */
    CHECK(unit.attack == 0xa5a5 && unit.defense == 0xa5a5 &&
          unit.accuracy == 0xa5a5 && unit.evasion == 0xa5a5);
    return 0;
}

int main(void) {
    /* 新游戏 stage 0 玩家基线。 */
    CHECK(check_unit(0, 1, 1, 1, 4, 42, 0, 6, 4, 2) == 0);
    CHECK(check_unit(9, 1, 1, 5, 4, 28, 8, 3, 2, 1) == 0);
    CHECK(check_unit(4, 1, 1, 3, 7, 48, 0, 6, 4, 2) == 0);
    CHECK(check_unit(30, 1, 6, 25, 4, 50, 0, 7, 6, 2) == 0);

    /* unit 1 level 2 的 HP=46 与本地 stage 2 存档记录一致。 */
    CHECK(check_unit(1, 2, 1, 2, 4, 46, 0, 14, 8, 2) == 0);
    CHECK(check_unit(3, 3, 1, 2, 4, 54, 0, 30, 15, 11) == 0);
    CHECK(check_unit(68, 2, 1, 2, 4, 36, 0, 10, 4, 2) == 0);
    CHECK(check_unit(96, 2, 1, 7, 4, 28, 0, 14, 2, 2) == 0);
    CHECK(check_unit(97, 3, 1, 7, 4, 72, 0, 24, 9, 6) == 0);

    fd2_field_unit_template stage_template = {{
        0x00, 0x61, 0x01, 0x01, 0x03,
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x05, 0x00, 0x00, 0x00, 0x00
    }};
    fd2_field_unit staged;
    memset(&staged, 0xa5, sizeof(staged));
    staged.x = 9;
    staged.y = 12;
    CHECK(fd2_field_unit_stage_template_apply(&staged, &stage_template) == 0);
    const uint8_t *staged_record = (const uint8_t *)&staged;
    CHECK(staged.x == 9 && staged.y == 12);
    CHECK(staged.side == 0 && staged.unit_id == 97 && staged.text_id == 97);
    CHECK(staged_record[0x0a] == 0x40 && staged_record[0x0b] == 0xff);
    CHECK(staged_record[0x0c] == 0x80);
    CHECK(staged_record[0x0e] == 0x80 && staged_record[0x0f] == 0xff);
    CHECK(staged_record[0x3c] == 0xff && staged_record[0x3d] == 1);
    CHECK(staged.hp == 72 && staged.movement_profile == 7);

    fd2_field_unit_template equipped_template = {{
        0x02, 0x01, 0xee, 0x00, 0x01,
        0x20, 0x21, 0x34, 0xff, 0xff, 0xff, 0xff, 0xff,
        0x11, 0x12, 0x13, 0x14,
        0xa1, 0xa2, 0xa3, 0x00, 0x03,
        0xb1, 0xb2, 0xb3, 0x00
    }};
    CHECK(fd2_field_unit_stage_template_apply(
              &staged, &equipped_template) == 0);
    staged_record = (const uint8_t *)&staged;
    CHECK(staged_record[0x0a] == 0x40 && staged_record[0x0b] == 0x20);
    CHECK(staged_record[0x0c] == 0x40 && staged_record[0x0d] == 0x21);
    CHECK(staged_record[0x0e] == 0 && staged_record[0x0f] == 0x34);
    CHECK(staged_record[0x10] == 0x80 && staged_record[0x11] == 0xff);
    CHECK(memcmp(staged_record + 0x1a, "\x11\x12\x13\x14", 4) == 0);
    CHECK(staged_record[0x31] == 0xb1);
    CHECK(staged_record[0x32] == 0xb2 && staged_record[0x33] == 0xb3);
    CHECK(staged_record[0x34] == 0xa1 && staged_record[0x35] == 0xa2 &&
          staged_record[0x36] == 0xa3);
    CHECK(staged_record[0x3c] == 0 && staged_record[0x3d] == 0xee);

    fd2_field_unit unsupported;
    memset(&unsupported, 0x5a, sizeof(unsupported));
    unsupported.unit_id = 40;
    fd2_field_unit saved = unsupported;
    CHECK(fd2_field_unit_base_apply(&unsupported, 1) == -1);
    CHECK(memcmp(&unsupported, &saved, sizeof(saved)) == 0);
    stage_template.bytes[1] = 40;
    CHECK(fd2_field_unit_stage_template_apply(
              &unsupported, &stage_template) == -1);
    CHECK(memcmp(&unsupported, &saved, sizeof(saved)) == 0);
    puts("field unit base tests: ok");
    return 0;
}
