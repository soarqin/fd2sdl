#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_manual_slot.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static void put_u32le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static fd2_save_file make_save(void) {
    fd2_save_file save = {0};
    save.data = malloc(FD2_SAVE_FILE_SIZE);
    if (!save.data) return save;
    save.size = FD2_SAVE_FILE_SIZE;
    memset(save.data, 0xff, save.size);
    for (size_t i = 0; i < FD2_SAVE_SLOT_COUNT; i++) {
        size_t meta = FD2_SAVE_SLOT_BASE + i * FD2_SAVE_SLOT_SIZE +
                      FD2_SAVE_UNIT_TABLE_SIZE;
        save.data[meta] = i == 1u ? 0u : 0xffu;
        save.data[meta + 1u] = i == 1u ? 2u : 0xffu;
    }
    put_u32le(save.data + save.size - FD2_SAVE_CHECKSUM_SIZE,
              fd2_save_checksum_sum(save.data, save.size));
    return save;
}

int main(void) {
    fd2_save_file save = make_save();
    CHECK(save.data != NULL);

    fd2_field_manual_slot_picker picker;
    fd2_field_manual_slot_picker_init(
        &picker, FD2_FIELD_MANUAL_SLOT_MODE_LOAD, 0, &save);
    CHECK(picker.open && picker.selected == 0);
    CHECK(!picker.occupied[0] && picker.occupied[1]);
    CHECK(picker.stage_ids[1] == 0 && picker.unit_counts[1] == 2);
    CHECK(fd2_field_manual_slot_picker_move(&picker, -1) == 0);
    CHECK(fd2_field_manual_slot_picker_confirm(&picker) == -1);
    CHECK(picker.open);
    CHECK(fd2_field_manual_slot_picker_move(&picker, 1) == 1);
    CHECK(fd2_field_manual_slot_picker_confirm(&picker) == 1);
    CHECK(!picker.open);

    fd2_field_manual_slot_picker_init(
        &picker, FD2_FIELD_MANUAL_SLOT_MODE_SAVE, 3, &save);
    CHECK(fd2_field_manual_slot_picker_move(&picker, 1) == 0);
    CHECK(fd2_field_manual_slot_picker_confirm(&picker) == 3);

    fd2_field_manual_slot_picker_init(
        &picker, FD2_FIELD_MANUAL_SLOT_MODE_SAVE, 99, &save);
    CHECK(picker.selected == 0);
    fd2_field_manual_slot_picker_cancel(&picker);
    CHECK(!picker.open);

    fd2_save_file_close(&save);
    puts("field_manual_slot_test: ok");
    return 0;
}
