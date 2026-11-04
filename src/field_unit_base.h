#ifndef FD2_FIELD_UNIT_BASE_H
#define FD2_FIELD_UNIT_BASE_H

#include <stdint.h>

#include "field_unit.h"

/* 按原版静态角色/敌军基础表和等级公式写入单位基础战斗字段。
 * 当前登记已由 stage 0、新游戏存档或运行时构造器交叉验证的 unit ID；
 * 未登记 ID 返回 -1，且不修改记录。 */
int fd2_field_unit_base_apply(fd2_field_unit *unit, uint8_t level);

/* sub_112A5 @0x112a5（原登记同名）的持久玩家初始构造及
 * sub_1145A @0x1145a 装备派生。只改已确认写集；未写 raw 字节沿用
 * 输入记录。支持原版 player base 的 ID 0..31，失败时不修改记录。 */
int fd2_field_unit_persistent_create(fd2_field_unit *unit, uint8_t unit_id);

/* 将 FDFIELD 26 字节模板按原版 stage constructor 重排到新单位记录，
 * 并调用基础表等级派生。保留调用方预设的 x/y；失败时不修改记录。 */
int fd2_field_unit_stage_template_apply(
    fd2_field_unit *unit,
    const fd2_field_unit_template *template_record);

#endif /* FD2_FIELD_UNIT_BASE_H */
