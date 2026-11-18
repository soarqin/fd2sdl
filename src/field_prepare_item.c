/* 准备服务的物品与资金有限原语。
 * 依据：field_shop_buy_execute @0x2872b（原sub_2872B）、
 * field_shop_sell_execute @0x28cbd（原sub_28CBD）、
 * field_prepare_item_give_execute @0x28f65（原sub_28F65）完整G1读集。
 * 原槽1b722/1b8e7/1bb8c/1c142及资金26a7a/26b91只保各自写点；
 * 分支、动画、等待、取消与调用顺序由准备冷图消费，不新建owner。
 * G3规范命名（完整Domain函数/xref/原资源依据，非ABI推断）：
 * field_shop_item_stats_preview @0x28632（原登记 sub_28632）。
 */
#include "field_prepare_item.h"

#include <string.h>

#include "field_item.h"
#include "field_unit_stats.h"

int fd2_field_prepare_item_visible_ids(const fd2_field_unit *unit,
        uint8_t *ids, size_t capacity, size_t *count) {
    if (!unit || !count) return -1;
    const uint8_t *raw=(const uint8_t *)unit;
    uint8_t visible[8];size_t used=0;
    for (size_t slot=0;slot<8;slot++)
        if (!(raw[0x0a+2u*slot]&0x80u)) visible[used++]=raw[0x0b+2u*slot];
    if (used>capacity || (used && !ids)) return -1;
    if (used) memcpy(ids,visible,used);
    *count=used;return 0;
}

int fd2_field_prepare_item_selected_id(const fd2_field_unit *unit,
        size_t selected, uint8_t *item_id) {
    if (!unit || !item_id || selected>=fd2_field_unit_inventory_count(unit)) return -1;
    /* 原压缩行索引直接传1b722；包括bit80置位的raw，不扫描“第N个可见”。 */
    *item_id=((const uint8_t *)unit)[0x0b+2u*selected];return 0;
}

int fd2_field_prepare_item_remove(fd2_field_unit *unit, size_t raw_slot) {
    return fd2_field_unit_inventory_slot_remove(unit,raw_slot);
}

int fd2_field_prepare_item_append(fd2_field_unit *unit, uint8_t item_id) {
    return fd2_field_unit_inventory_item_append(unit,item_id);
}

int fd2_field_prepare_item_equip(fd2_field_unit *unit, size_t raw_slot) {
    return fd2_field_unit_equipment_slot_set(unit,raw_slot);
}

int fd2_field_prepare_item_recompute(fd2_field_unit *unit) {
    return fd2_field_unit_combat_stats_recompute_original(unit);
}

int fd2_field_prepare_item_sell_price(uint8_t item_id, uint32_t *price) {
    const uint8_t *item=fd2_field_item_record_get(item_id);
    if (!item || !price) return -1;
    *price=3u*fd2_field_item_purchase_price(item)/4u;return 0;
}

/* sub_28632 @0x28632（原同名），28713..28723完整DWORD输出。
 * 这是商品比较专用算式，不能以1b750或1c142的ID128分组替代。 */
int fd2_field_prepare_item_preview_stats(const fd2_field_unit *unit,
        uint8_t item_id, int32_t stats[4]) {
    const uint8_t *item=fd2_field_item_record_get(item_id);
    if (!unit || !item || !stats) return -1;
    const uint8_t *raw=(const uint8_t *)unit;int16_t base[3];
    memcpy(&base[0],raw+0x37,2);memcpy(&base[1],raw+0x39,2);memcpy(&base[2],raw+0x3e,2);
    fd2_field_item_stat_effect effect;
    if (fd2_field_unit_original_item_stats(NULL,item_id,&effect)!=0) return -1;
    int32_t result[4]={base[0]+effect.attack,base[1]+effect.defense,
        base[2]+effect.accuracy,base[2]+effect.evasion};
    for (size_t slot=0;slot<8u;slot++) {
        size_t at=0x0a+2u*slot;
        if (!(raw[at]&0x40u)) continue;
        const uint8_t *old=fd2_field_item_record_get(raw[at+1u]);
        if (!old) return -1;
        if ((item[0]<=20u)==(old[0]<=20u)) continue;
        if (fd2_field_unit_original_item_stats(NULL,raw[at+1u],&effect)!=0) return -1;
        result[0]+=effect.attack;result[1]+=effect.defense;
        result[2]+=effect.accuracy;result[3]+=effect.evasion;
    }
    memcpy(stats,result,sizeof(result));return 0;
}

int fd2_field_prepare_item_give_preflight(const fd2_field_unit *sender,
        size_t selected, const fd2_field_unit *recipient, uint8_t *item_id) {
    if (!sender || !recipient || !item_id || selected>=fd2_field_unit_inventory_count(sender)) return -1;
    /* 包括self满8，容量门在移槽之前。只将正常record/派生失败前置，不写live。 */
    if (fd2_field_unit_inventory_count(recipient)==8u) return 0;
    uint8_t id;
    if (fd2_field_prepare_item_selected_id(sender,selected,&id)!=0 || !fd2_field_item_record_get(id)) return -1;
    fd2_field_unit source=*sender,destination=*recipient;
    fd2_field_unit *to=sender==recipient ? &source:&destination;
    if (fd2_field_prepare_item_remove(&source,selected)!=0 ||
        fd2_field_prepare_item_append(to,id)!=1 || fd2_field_prepare_item_recompute(&source)!=0) return -1;
    *item_id=id;return 1;
}

static int64_t signed_money(uint32_t value) {
    return value<=INT32_MAX ? (int64_t)value:(int64_t)value-INT64_C(0x100000000);
}

int fd2_field_prepare_money_can_pay(uint32_t money, uint32_t amount) {
    return signed_money(money)>=signed_money(amount);
}

int fd2_field_prepare_money_add(uint32_t *money, uint32_t amount) {
    if (!money) return -1;
    *money+=amount;return 0;
}

int fd2_field_prepare_money_subtract(uint32_t *money, uint32_t amount) {
    if (!money) return -1;
    *money-=amount;return 0;
}
