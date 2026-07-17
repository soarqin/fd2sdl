/* 炎龙骑士团 2 SDL3 重写 - 四槽手工 Save/Load picker 状态机
 *
 * field_manual_slot_picker @code0 0x19bcb 将选择限制在 0..3；
 * field_manual_slot_draw @code0 0x19ab2 按 y=0x77 + index*0x13 纵向绘制四项，
 * 只检查 slot+0xa00 是否为 0xff 来区分空槽与有效槽。本模块不猜测
 * 可见标签或 runtime restore。
 */

#include "field_manual_slot.h"

#include <string.h>

void fd2_field_manual_slot_picker_init(
        fd2_field_manual_slot_picker *picker,
        fd2_field_manual_slot_mode mode,
        size_t initial_slot,
        const fd2_save_file *save) {
    if (!picker) return;
    memset(picker, 0, sizeof(*picker));
    picker->mode = mode;
    picker->selected = initial_slot < FD2_SAVE_SLOT_COUNT
        ? (uint8_t)initial_slot : 0u;
    picker->open = 1;
    for (size_t i = 0; i < FD2_SAVE_SLOT_COUNT; i++) {
        fd2_save_manual_slot slot;
        if (fd2_save_file_get_manual_slot_raw(save, i, &slot) != 0)
            continue;
        picker->stage_ids[i] = slot.meta[0];
        picker->unit_counts[i] = slot.meta[1];
        picker->occupied[i] = slot.meta[0] != 0xffu;
    }
}

int fd2_field_manual_slot_picker_move(
        fd2_field_manual_slot_picker *picker, int delta) {
    if (!picker || !picker->open || delta == 0) return 0;
    int selected = (int)picker->selected + (delta < 0 ? -1 : 1);
    if (selected < 0 || selected >= (int)FD2_SAVE_SLOT_COUNT) return 0;
    picker->selected = (uint8_t)selected;
    return 1;
}

int fd2_field_manual_slot_picker_confirm(
        fd2_field_manual_slot_picker *picker) {
    if (!picker || !picker->open) return -1;
    if (picker->mode == FD2_FIELD_MANUAL_SLOT_MODE_LOAD &&
        !picker->occupied[picker->selected])
        return -1;
    picker->open = 0;
    return picker->selected;
}

void fd2_field_manual_slot_picker_cancel(
        fd2_field_manual_slot_picker *picker) {
    if (picker) picker->open = 0;
}
