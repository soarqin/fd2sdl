#ifndef FD2_FIELD_MANUAL_SLOT_H
#define FD2_FIELD_MANUAL_SLOT_H

#include <stddef.h>
#include <stdint.h>

#include "save.h"

/* 炎龙骑士团 2 SDL3 重写 - 四槽手工 Save/Load picker
 *
 * 逆向依据：field_manual_slot_picker @code0 0x19bcb 调用
 * field_manual_slot_draw @code0 0x19ab2，选择范围固定为 0..3；四项按
 * y=0x77 + index*0x13 纵向逐行绘制，扫描码 0x48/0x50 向前／向后移动，
 * Enter/Space 确认，Esc 取消。空槽只由 slot+0xa00 的 stage marker
 * 0xff 判定。
 */

typedef enum {
    FD2_FIELD_MANUAL_SLOT_MODE_SAVE = 0,
    FD2_FIELD_MANUAL_SLOT_MODE_LOAD = 1
} fd2_field_manual_slot_mode;

typedef struct {
    fd2_field_manual_slot_mode mode;
    uint8_t selected;
    uint8_t occupied[FD2_SAVE_SLOT_COUNT];
    uint8_t stage_ids[FD2_SAVE_SLOT_COUNT];
    uint8_t unit_counts[FD2_SAVE_SLOT_COUNT];
    int open;
} fd2_field_manual_slot_picker;

void fd2_field_manual_slot_picker_init(
    fd2_field_manual_slot_picker *picker,
    fd2_field_manual_slot_mode mode,
    size_t initial_slot,
    const fd2_save_file *save);
int fd2_field_manual_slot_picker_move(
    fd2_field_manual_slot_picker *picker, int delta);
/* 返回 selected slot；Load 模式选择空槽时返回 -1。 */
int fd2_field_manual_slot_picker_confirm(
    fd2_field_manual_slot_picker *picker);
void fd2_field_manual_slot_picker_cancel(
    fd2_field_manual_slot_picker *picker);

#endif /* FD2_FIELD_MANUAL_SLOT_H */
