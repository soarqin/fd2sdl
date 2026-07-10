#ifndef FD2_SAVE_H
#define FD2_SAVE_H

#include <stddef.h>
#include <stdint.h>

#define FD2_SAVE_UNIT_RECORD_SIZE 0x50
#define FD2_SAVE_MAX_UNITS 32

typedef struct {
    uint8_t bytes[FD2_SAVE_UNIT_RECORD_SIZE];
} fd2_save_unit;

typedef struct {
    uint8_t stage_id;
    uint8_t unit_count;
    uint32_t gold_or_flags;
    uint8_t flags[4];
    fd2_save_unit units[FD2_SAVE_MAX_UNITS];
} fd2_save_slot;

int fd2_save_load_slot(fd2_save_slot *slot, const char *path, size_t slot_index);

uint8_t fd2_save_unit_class(const fd2_save_unit *unit);
uint8_t fd2_save_unit_portrait(const fd2_save_unit *unit);
uint8_t fd2_save_unit_name_id(const fd2_save_unit *unit);

#endif /* FD2_SAVE_H */
