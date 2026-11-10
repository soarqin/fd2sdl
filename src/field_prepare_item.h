#ifndef FD2_FIELD_PREPARE_ITEM_H
#define FD2_FIELD_PREPARE_ITEM_H

#include <stddef.h>
#include <stdint.h>

#include "field_unit.h"

/* 28cbd/28f65显示压缩可见ID，提交仍以同一数字直接读raw槽，不做反向映射。
 * 查询无写/RNG；空间不足或坏参数不写输出。raw选择允许已知稀疏，不整理库存。 */
int fd2_field_prepare_item_visible_ids(const fd2_field_unit *unit,
    uint8_t *ids, size_t capacity, size_t *count);
int fd2_field_prepare_item_selected_id(const fd2_field_unit *unit,
    size_t selected, uint8_t *item_id);

/* 1b8e7/1bb8c的独立写点：remove左移所有后续raw pair，仅末flag80，保末ID；
 * append找首hidden，整flag0/id写入，成功1、满槽0、坏参数-1。ID为原byte，
 * 不把未知hidden stale值改成ff；服务须在首写前完成所需record/资源检查。 */
int fd2_field_prepare_item_remove(fd2_field_unit *unit, size_t raw_slot);
int fd2_field_prepare_item_append(fd2_field_unit *unit, uint8_t item_id);

/* 1c142仅按ID<128/>=128清同组带40的整flag，再选中40；不查职业或重算。
 * Buy立即装备的调用方传追加后count-1，不能用实际首洞位置替代原槽号。
 * 1b750重算独立，区别于1145a持久构造派生；不写HP/MP/EX/acted。 */
int fd2_field_prepare_item_equip(fd2_field_unit *unit, size_t raw_slot);
int fd2_field_prepare_item_recompute(fd2_field_unit *unit);
int fd2_field_prepare_item_sell_price(uint8_t item_id, uint32_t *price);
/* 28632商品预览：四DWORD顺序AP/DP/HIT/EV，base signed WORD+新效果，
 * 仅保留相反kind<=20/>20组的旧装备；不应用22..24状态、不窄化、不改unit。 */
int fd2_field_prepare_item_preview_stats(const fd2_field_unit *unit,
    uint8_t item_id, int32_t stats[4]);

/* 28f65收方count==8先拒，包括满包自给。成功1/满包0/参数或派生不支持-1；
 * 仅shadow预检，输出原raw ID。sender==recipient使用同一shadow先移再追加。
 * 调用者按冷图执行remove(sender)→append(recipient)→recompute(sender)，
 * 不以两份sender/recipient副本先后覆盖同一live记录，不加距离/HP/hidden门。 */
int fd2_field_prepare_item_give_preflight(const fd2_field_unit *sender,
    size_t selected, const fd2_field_unit *recipient, uint8_t *item_id);

/* 28946/2894c是两个DWORD作signed CMP/JGE；26ab5/26bcc为模2^32加减。
 * 金额函数只写现有money字段，不播放动画、不检查UI、不创建第二owner。
 * 动画后的原业务写点由准备冷图调用，金额滚动内部先写新钱再等待。 */
int fd2_field_prepare_money_can_pay(uint32_t money, uint32_t amount);
int fd2_field_prepare_money_add(uint32_t *money, uint32_t amount);
int fd2_field_prepare_money_subtract(uint32_t *money, uint32_t amount);

#endif
