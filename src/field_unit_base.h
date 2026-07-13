#ifndef FD2_FIELD_UNIT_BASE_H
#define FD2_FIELD_UNIT_BASE_H

#include <stdint.h>

#include "field_unit.h"

/* 按原版静态角色/敌军基础表和等级公式写入单位基础战斗字段。
 * 当前登记已由 stage 0、新游戏存档或运行时构造器交叉验证的 unit ID；
 * 未登记 ID 返回 -1，且不修改记录。 */
int fd2_field_unit_base_apply(fd2_field_unit *unit, uint8_t level);

/* 将 FDFIELD 26 字节模板按原版 stage constructor 重排到新单位记录，
 * 并调用基础表等级派生。保留调用方预设的 x/y；失败时不修改记录。 */
int fd2_field_unit_stage_template_apply(
    fd2_field_unit *unit,
    const fd2_field_unit_template *template_record);

#endif /* FD2_FIELD_UNIT_BASE_H */
