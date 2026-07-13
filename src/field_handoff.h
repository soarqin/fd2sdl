#ifndef FD2_FIELD_HANDOFF_H
#define FD2_FIELD_HANDOFF_H

#include <stddef.h>
#include <stdint.h>

#include "field_unit.h"

/* 过场交还正式战场时保留的动态状态。正式 session 仍从 FDFIELD 重新
 * 建立带模板字段的单位记录，只从这里接收坐标、朝向、隐藏状态和镜头，
 * 避免用过场的简化 actor 记录覆盖阵营与后续战斗属性。 */
typedef struct {
    size_t stage;
    fd2_field_units units;
    int camera_cell_x;
    int camera_cell_y;
    int focus_cell_x;
    int focus_cell_y;
    uint8_t idle_phase;
    int valid;
} fd2_field_handoff;

#endif /* FD2_FIELD_HANDOFF_H */
