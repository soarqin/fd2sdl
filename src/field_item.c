/* 炎龙骑士团 2 SDL3 重写 - 物品/装备 23 字节静态表
 * 逆向依据：field_item_record_get @0x4e8bc 直接返回
 * DS:0x02ad + item_id*0x17；表在 corrected code0 0x684c1；
 * field_unit_inventory_count @0x1b8a6（原登记同名）的8槽计数；
 * field_unit_inventory_select_step @0x1b9de（原登记sub_1B9DE）的按键分派；
 * field_unit_can_equip_item @0x1c1c3（原sub_1C1C3）经
 * field_equipment_profile_get @0x4e88e（原sub_4E88E）读取装备许可。 */

#include "field_item.h"

#include "static_data/items.inc"

_Static_assert(sizeof(ITEM_RECORDS) == FD2_FIELD_ITEM_COUNT * FD2_FIELD_ITEM_RECORD_SIZE &&
               sizeof(ITEM_RECORDS[0]) == FD2_FIELD_ITEM_RECORD_SIZE &&
               sizeof(profiles) == 29u * 7u && sizeof(profiles[0]) == 7u,
               "item definition layout must match the supported original records");

const uint8_t *fd2_field_item_record_get(uint8_t item_id) {
    if ((size_t)item_id >= FD2_FIELD_ITEM_COUNT) return NULL;
    return ITEM_RECORDS[item_id];
}

int fd2_field_item_equip_compatibility(uint8_t profile, uint8_t item_id) {
    const uint8_t *record = fd2_field_item_record_get(item_id);
    if ((size_t)profile >= 29u || !record) return -1;
    /* field_equipment_profile_get @0x4e88e（原sub_4E88E）：object3 0x6188a..0x61955共29×7B，原getter没有index门。
     * 这里的0..28为已确认数据支持界限；1c1c3仅比较每行前6B。 */

    for (size_t i = 0; i < 6u; i++)
        if (record[0] == profiles[profile][i]) return 1;
    return 0;
}

size_t fd2_field_unit_inventory_count(const fd2_field_unit *unit) {
    if (!unit) return 0;
    const uint8_t *record = (const uint8_t *)unit;
    size_t count = 0;
    for (size_t slot = 0; slot < 8u; slot++) {
        if ((record[0x0au + slot * 2u] & 0x80u) == 0) count++;
    }
    return count;
}

int fd2_field_item_list_select_step(
        const fd2_field_unit *unit, int *selected, fd2_field_item_list_key key,
        int require_usable) {
    if (!unit || !selected ||
        key < FD2_FIELD_ITEM_LIST_KEY_OTHER || key > FD2_FIELD_ITEM_LIST_KEY_CANCEL)
        return -2;
    const uint8_t *record = (const uint8_t *)unit;
    size_t count = fd2_field_unit_inventory_count(unit);
    int index = *selected;
    if (count == 0 || index < 0 || (size_t)index >= count) return -2;
    /* 原版按0..count-1直接读raw槽，而绘制会跳空槽；不猜测空洞映射。
     * 仅允许密集前缀，且完整验证ID，不能导航后才发现未选中的坏项。 */
    for (size_t slot = 0; slot < 8u; slot++) {
        int occupied = (record[0x0au + slot * 2u] & 0x80u) == 0;
        if (occupied != (slot < count)) return -2;
        if (occupied && !fd2_field_item_record_get(record[0x0bu + slot * 2u]))
            return -2;
    }
    switch (key) {
        case FD2_FIELD_ITEM_LIST_KEY_UP:
            *selected = index == 0 ? (int)count - 1 : index - 1;
            break;
        case FD2_FIELD_ITEM_LIST_KEY_DOWN:
            *selected = (size_t)index + 1u == count ? 0 : index + 1;
            break;
        case FD2_FIELD_ITEM_LIST_KEY_LEFT:
            if (index >= 4) *selected = index - 4;
            break;
        case FD2_FIELD_ITEM_LIST_KEY_RIGHT:
            if (index <= 3 && (size_t)index + 4u < count) *selected = index + 4;
            break;
        case FD2_FIELD_ITEM_LIST_KEY_CONFIRM:
            return !require_usable ||
                fd2_field_item_record_get(record[0x0bu + (size_t)index * 2u])[0x0du] != 0;
        case FD2_FIELD_ITEM_LIST_KEY_CANCEL:
            return -1;
        case FD2_FIELD_ITEM_LIST_KEY_OTHER:
            break;
    }
    return 0;
}

int16_t fd2_field_item_i16(const uint8_t *record, size_t offset) {
    if (!record || offset + 1u >= FD2_FIELD_ITEM_RECORD_SIZE) return 0;
    return (int16_t)((uint16_t)record[offset] |
                     ((uint16_t)record[offset + 1u] << 8));
}
