/* field_unit_inventory_count @0x1b8a6（原登记同名）与
 * field_unit_inventory_select_step @0x1b9de（原登记sub_1B9DE）的纯逻辑回归。
 * 不执行16c57等待、SFX、共享RNG或物品handler，不创建窗口。 */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "field_item.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    return 1; } } while (0)

static void set_slot(fd2_field_unit *unit, size_t slot, uint8_t flags, uint8_t id) {
    uint8_t *record = (uint8_t *)unit;
    record[0x0au + slot * 2u] = flags;
    record[0x0bu + slot * 2u] = id;
}

static void init_unit(fd2_field_unit *unit, size_t count) {
    memset(unit, 0xa5, sizeof(*unit));
    for (size_t slot = 0; slot < 8u; slot++)
        set_slot(unit, slot, slot < count ? 0x40u : 0x80u,
                 slot < count ? 0u : 0xffu);
}

static int step(const fd2_field_unit *unit, int index, fd2_field_item_list_key key,
                int require_usable, int result, int expected_index) {
    fd2_field_unit before = *unit;
    CHECK(fd2_field_item_list_select_step(unit, &index, key, require_usable) == result);
    CHECK(index == expected_index);
    CHECK(memcmp(unit, &before, sizeof(before)) == 0);
    return 0;
}

static int test_count_preserves_raw_slots(void) {
    CHECK(fd2_field_unit_inventory_count(NULL) == 0);
    for (unsigned mask = 0; mask < 256u; mask++) {
        fd2_field_unit unit;
        init_unit(&unit, 0);
        size_t expected = 0;
        for (size_t slot = 0; slot < 8u; slot++) {
            int occupied = (mask & (1u << slot)) != 0;
            /* 计数不查未知ID，也不要求槽前缀密集。 */
            set_slot(&unit, slot, occupied ? 0x7fu : 0xffu, 0xffu);
            expected += (size_t)occupied;
        }
        fd2_field_unit before = unit;
        CHECK(fd2_field_unit_inventory_count(&unit) == expected);
        CHECK(memcmp(&unit, &before, sizeof(unit)) == 0);
    }
    for (unsigned flags = 0; flags < 256u; flags++) {
        fd2_field_unit unit;
        init_unit(&unit, 0);
        set_slot(&unit, 7, (uint8_t)flags, 0xffu);
        CHECK(fd2_field_unit_inventory_count(&unit) == (flags < 0x80u ? 1u : 0u));
    }
    return 0;
}

static int test_navigation_boundaries(void) {
    static const struct {
        size_t count;
        int index;
        fd2_field_item_list_key key;
        int expected;
    } cases[] = {
        {1,0,FD2_FIELD_ITEM_LIST_KEY_UP,0},
        {1,0,FD2_FIELD_ITEM_LIST_KEY_DOWN,0},
        {1,0,FD2_FIELD_ITEM_LIST_KEY_LEFT,0},
        {1,0,FD2_FIELD_ITEM_LIST_KEY_RIGHT,0},
        {4,0,FD2_FIELD_ITEM_LIST_KEY_UP,3},
        {4,3,FD2_FIELD_ITEM_LIST_KEY_DOWN,0},
        {4,2,FD2_FIELD_ITEM_LIST_KEY_UP,1},
        {4,0,FD2_FIELD_ITEM_LIST_KEY_RIGHT,0},
        {4,3,FD2_FIELD_ITEM_LIST_KEY_LEFT,3},
        {5,0,FD2_FIELD_ITEM_LIST_KEY_UP,4},
        {5,4,FD2_FIELD_ITEM_LIST_KEY_DOWN,0},
        {5,0,FD2_FIELD_ITEM_LIST_KEY_RIGHT,4},
        {5,1,FD2_FIELD_ITEM_LIST_KEY_RIGHT,1},
        {5,4,FD2_FIELD_ITEM_LIST_KEY_LEFT,0},
        {5,4,FD2_FIELD_ITEM_LIST_KEY_RIGHT,4},
        {5,3,FD2_FIELD_ITEM_LIST_KEY_DOWN,4},
        {8,0,FD2_FIELD_ITEM_LIST_KEY_UP,7},
        {8,7,FD2_FIELD_ITEM_LIST_KEY_DOWN,0},
        {8,3,FD2_FIELD_ITEM_LIST_KEY_RIGHT,7},
        {8,4,FD2_FIELD_ITEM_LIST_KEY_RIGHT,4},
        {8,7,FD2_FIELD_ITEM_LIST_KEY_LEFT,3},
        {8,3,FD2_FIELD_ITEM_LIST_KEY_LEFT,3},
        {8,6,FD2_FIELD_ITEM_LIST_KEY_UP,5},
        {8,6,FD2_FIELD_ITEM_LIST_KEY_DOWN,7}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fd2_field_unit unit;
        init_unit(&unit, cases[i].count);
        CHECK(step(&unit, cases[i].index, cases[i].key, 1, 0, cases[i].expected) == 0);
    }
    return 0;
}

static int test_confirmed_start_slot_sample_and_use_gate(void) {
    fd2_field_unit unit;
    init_unit(&unit, 0);
    /* 用已确认的START raw0三个槽字节构造样例：短剑0、皮甲132、药草192。
     * 此处不读取START.SAV，也不执行原版或完整装备构造。
     * +0x40装备标志不禁止选择，也不赋予可使用code。 */
    set_slot(&unit, 0, 0x40, 0);
    set_slot(&unit, 1, 0x40, 132);
    set_slot(&unit, 2, 0x00, 192);
    CHECK(fd2_field_unit_inventory_count(&unit) == 3);
    CHECK(fd2_field_item_record_get(0)[0x0d] == 0);
    CHECK(fd2_field_item_record_get(132)[0x0d] == 0);
    CHECK(fd2_field_item_record_get(192)[0x0d] == 5);
    CHECK(step(&unit, 0, FD2_FIELD_ITEM_LIST_KEY_CONFIRM, 1, 0, 0) == 0);
    CHECK(step(&unit, 1, FD2_FIELD_ITEM_LIST_KEY_CONFIRM, 1, 0, 1) == 0);
    CHECK(step(&unit, 0, FD2_FIELD_ITEM_LIST_KEY_UP, 1, 0, 2) == 0);
    CHECK(step(&unit, 2, FD2_FIELD_ITEM_LIST_KEY_CONFIRM, 1, 1, 2) == 0);
    CHECK(step(&unit, 0, FD2_FIELD_ITEM_LIST_KEY_CONFIRM, 0, 1, 0) == 0);
    CHECK(step(&unit, 1, FD2_FIELD_ITEM_LIST_KEY_CONFIRM, 0, 1, 1) == 0);
    CHECK(step(&unit, 2, FD2_FIELD_ITEM_LIST_KEY_CANCEL, 1, -1, 2) == 0);
    CHECK(step(&unit, 2, FD2_FIELD_ITEM_LIST_KEY_OTHER, 1, 0, 2) == 0);

    /* 只判断record+0x0d非零，不把已实现的code5/13作为列表白名单。 */
    set_slot(&unit, 2, 0x7f, 214);
    CHECK(fd2_field_item_record_get(214)[0x0d] == 16);
    CHECK(step(&unit, 2, FD2_FIELD_ITEM_LIST_KEY_CONFIRM, -9, 1, 2) == 0);
    unit.hp = unit.mp = 0;
    unit.flags = 0xff;
    unit.detail_status[2] = 1;
    CHECK(step(&unit, 2, FD2_FIELD_ITEM_LIST_KEY_CONFIRM, 1, 1, 2) == 0);
    return 0;
}

static int test_support_rejections_preserve_outputs(void) {
    fd2_field_unit unit;
    init_unit(&unit, 0);
    CHECK(step(&unit, 0, FD2_FIELD_ITEM_LIST_KEY_CANCEL, 0, -2, 0) == 0);
    init_unit(&unit, 3);
    CHECK(step(&unit, -1, FD2_FIELD_ITEM_LIST_KEY_UP, 0, -2, -1) == 0);
    CHECK(step(&unit, 3, FD2_FIELD_ITEM_LIST_KEY_DOWN, 0, -2, 3) == 0);
    CHECK(step(&unit, INT_MIN, FD2_FIELD_ITEM_LIST_KEY_UP, 0, -2, INT_MIN) == 0);
    CHECK(step(&unit, INT_MAX, FD2_FIELD_ITEM_LIST_KEY_UP, 0, -2, INT_MAX) == 0);
    CHECK(step(&unit, 0, (fd2_field_item_list_key)-1, 0, -2, 0) == 0);
    CHECK(step(&unit, 0, (fd2_field_item_list_key)7, 0, -2, 0) == 0);
    int selected = 1;
    CHECK(fd2_field_item_list_select_step(NULL, &selected,
              FD2_FIELD_ITEM_LIST_KEY_UP, 0) == -2);
    CHECK(selected == 1);
    fd2_field_unit before = unit;
    CHECK(fd2_field_item_list_select_step(&unit, NULL,
              FD2_FIELD_ITEM_LIST_KEY_UP, 0) == -2);
    CHECK(memcmp(&unit, &before, sizeof(unit)) == 0);

    for (unsigned id = FD2_FIELD_ITEM_COUNT; id <= UINT8_MAX; id++) {
        init_unit(&unit, 3);
        set_slot(&unit, 2, 0x00, (uint8_t)id);
        /* 坏项未选中时也先拒绝，不产生半次导航或确认。 */
        CHECK(step(&unit, 0, FD2_FIELD_ITEM_LIST_KEY_UP, 1, -2, 0) == 0);
        CHECK(step(&unit, 0, FD2_FIELD_ITEM_LIST_KEY_CONFIRM, 0, -2, 0) == 0);
        CHECK(step(&unit, 0, FD2_FIELD_ITEM_LIST_KEY_CANCEL, 1, -2, 0) == 0);
    }
    for (size_t hole = 0; hole < 7u; hole++) {
        init_unit(&unit, 8);
        set_slot(&unit, hole, 0x80, 0xff);
        CHECK(fd2_field_unit_inventory_count(&unit) == 7);
        CHECK(step(&unit, 0, FD2_FIELD_ITEM_LIST_KEY_DOWN, 0, -2, 0) == 0);
        CHECK(step(&unit, 0, FD2_FIELD_ITEM_LIST_KEY_CONFIRM, 1, -2, 0) == 0);
        CHECK(step(&unit, 0, FD2_FIELD_ITEM_LIST_KEY_CANCEL, 1, -2, 0) == 0);
    }
    init_unit(&unit, 1); /* 不可见尾槽未知ID不应阻塞有效前缀。 */
    CHECK(step(&unit, 0, FD2_FIELD_ITEM_LIST_KEY_CONFIRM, 0, 1, 0) == 0);
    return 0;
}

static int test_equip_compatibility_full_table(void) {
    CHECK(fd2_field_item_equip_compatibility(1, 0) == 1);
    CHECK(fd2_field_item_equip_compatibility(1, 1) == 1);
    CHECK(fd2_field_item_equip_compatibility(1, 127) == 1);
    CHECK(fd2_field_item_equip_compatibility(1, 128) == 1);
    CHECK(fd2_field_item_equip_compatibility(1, 132) == 1);
    CHECK(fd2_field_item_equip_compatibility(1, 192) == 0);
    for (unsigned profile = 0; profile <= UINT8_MAX; profile++)
        if (profile >= 29) CHECK(fd2_field_item_equip_compatibility((uint8_t)profile, 0) == -1);
    for (unsigned id = FD2_FIELD_ITEM_COUNT; id <= UINT8_MAX; id++)
        CHECK(fd2_field_item_equip_compatibility(1, (uint8_t)id) == -1);
    /* 原始29行独立边界向量；第7B为01，不能误当第七个许可。 */
    CHECK(fd2_field_item_equip_compatibility(0, 0) == 0);
    CHECK(fd2_field_item_equip_compatibility(2, 32) == 1);
    CHECK(fd2_field_item_equip_compatibility(2, 0) == 0);
    CHECK(fd2_field_item_equip_compatibility(5, 52) == 1);
    CHECK(fd2_field_item_equip_compatibility(25, 72) == 1);
    CHECK(fd2_field_item_equip_compatibility(26, 0) == 0);
    CHECK(fd2_field_item_equip_compatibility(27, 0) == 0);
    CHECK(fd2_field_item_equip_compatibility(28, 62) == 1);
    for (unsigned id = 0; id < FD2_FIELD_ITEM_COUNT; id++) {
        const uint8_t *record = fd2_field_item_record_get((uint8_t)id);
        int expected = record[0] == 1 || record[0] == 0x15 || record[0] == 0x16;
        CHECK(fd2_field_item_equip_compatibility(1, (uint8_t)id) == expected);
    }
    static const uint64_t licenses[29] = {
        UINT64_C(0x0), UINT64_C(0x600002), UINT64_C(0x1e00010), UINT64_C(0xe00008), UINT64_C(0x600020), UINT64_C(0x2200040), UINT64_C(0x2200040), UINT64_C(0x600004), UINT64_C(0x4202080), UINT64_C(0x600202), UINT64_C(0x1e00010), UINT64_C(0x1e00408), UINT64_C(0x601020), UINT64_C(0x2200040), UINT64_C(0x2200040), UINT64_C(0x600802), UINT64_C(0x4200080), UINT64_C(0xe00202), UINT64_C(0x1e00010), UINT64_C(0xe00008), UINT64_C(0xe00020), UINT64_C(0x2200040), UINT64_C(0x2200040), UINT64_C(0x600802), UINT64_C(0x4200080), UINT64_C(0x8000100), UINT64_C(0x0), UINT64_C(0x0), UINT64_C(0x4200080)
    };
    for (unsigned profile = 0; profile < 29; profile++)
        for (unsigned id = 0; id < FD2_FIELD_ITEM_COUNT; id++) {
            unsigned category = fd2_field_item_record_get((uint8_t)id)[0];
            int expected = category < 64 && ((licenses[profile] >> category) & 1u);
            CHECK(fd2_field_item_equip_compatibility((uint8_t)profile, (uint8_t)id) == expected);
        }
    return 0;
}

int main(void) {
    CHECK(test_count_preserves_raw_slots() == 0);
    CHECK(test_navigation_boundaries() == 0);
    CHECK(test_confirmed_start_slot_sample_and_use_gate() == 0);
    CHECK(test_support_rejections_preserve_outputs() == 0);
    CHECK(test_equip_compatibility_full_table() == 0);
    puts("field_item_select_test: ok");
    return 0;
}
