#ifndef FD2_FIELD_ITEM_H
#define FD2_FIELD_ITEM_H

#include <stddef.h>
#include <stdint.h>

#include "field_unit.h"

#define FD2_FIELD_ITEM_RECORD_SIZE 23u
#define FD2_FIELD_ITEM_COUNT 215u

/* field_unit_inventory_count @0x1b8a6（原登记同名）：遍历原始8槽，
 * flag bit80未置位即计数，不按装备位、物品ID或前缀密集性过滤。
 * NULL返回0；查询不修改单位或消费RNG。 */
size_t fd2_field_unit_inventory_count(const fd2_field_unit *unit);

/* SDL动作枚举，不是BIOS扫描码；Enter/Space由宿主统一为CONFIRM。 */
typedef enum {
    FD2_FIELD_ITEM_LIST_KEY_OTHER = 0,
    FD2_FIELD_ITEM_LIST_KEY_UP,
    FD2_FIELD_ITEM_LIST_KEY_DOWN,
    FD2_FIELD_ITEM_LIST_KEY_LEFT,
    FD2_FIELD_ITEM_LIST_KEY_RIGHT,
    FD2_FIELD_ITEM_LIST_KEY_CONFIRM,
    FD2_FIELD_ITEM_LIST_KEY_CANCEL
} fd2_field_item_list_key;

/* field_unit_inventory_select_step @0x1b9de（原登记sub_1B9DE）的纯按键部分。
 * selected是原始槽索引，不是跳过空槽后的序号；仅导航写入。
 * 返回0继续、-1取消、1确认；参数／空库存／非密集前缀／未知ID返回-2，
 * 拒绝时不写selected。后两项是SDL支持门，不修整原始槽或stale ID。
 * require_usable为0接受有效槽，非0仅在item record+0x0d非0时确认。
 * 不执行物品handler、扣资源、acted、等待、SFX或UI共享RNG。
 * Up/Down即使单项回绕也有原版SFX；声音由将来的modal owner处理。 */
int fd2_field_item_list_select_step(
    const fd2_field_unit *unit, int *selected, fd2_field_item_list_key key,
    int require_usable);

/* field_item_record_get @0x4e8bc：DS:0x02ad + item_id*0x17。
 * 表内容来自 corrected code0 0x684c1，对应 FD2.EXE file offset 0x792c1。 */
const uint8_t *fd2_field_item_record_get(uint8_t item_id);
int16_t fd2_field_item_i16(const uint8_t *record, size_t offset);

/* sub_1C1C3 @0x1c1c3与sub_4E88E @0x4e88e（原登记同名）的已证许可子集。
 * 仅profile1：item category与0x61891前6B比较；第7B及其他profile未确认。
 * 兼容1、不兼容0、未知profile/物品ID为-1；无状态写入或RNG。
 * 不是完整原版职业表或EQT能力开放门。 */
int fd2_field_item_equip_compatibility(uint8_t profile, uint8_t item_id);

#endif /* FD2_FIELD_ITEM_H */
