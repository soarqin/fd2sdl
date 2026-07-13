#ifndef FD2_FIELD_MOVE_PROFILE_H
#define FD2_FIELD_MOVE_PROFILE_H

#include <stddef.h>
#include <stdint.h>

#define FD2_FIELD_MOVEMENT_PROFILE_COUNT 29
#define FD2_FIELD_MOVEMENT_COST_CLASS_COUNT 20

/* 原版静态 movement profile 表。每行由 actor record offset 0x20 选择，
 * FDSHAP attr[1] 再索引行内成本。 */
const uint8_t *fd2_field_movement_profile_get(uint8_t profile_id);
size_t fd2_field_movement_profile_count(void);

#endif /* FD2_FIELD_MOVE_PROFILE_H */
