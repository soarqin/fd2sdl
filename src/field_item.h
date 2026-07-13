#ifndef FD2_FIELD_ITEM_H
#define FD2_FIELD_ITEM_H

#include <stddef.h>
#include <stdint.h>

#define FD2_FIELD_ITEM_RECORD_SIZE 23u
#define FD2_FIELD_ITEM_COUNT 215u

/* field_item_record_get @0x73ad0：DS:0x02ad + item_id*0x17。
 * 表内容来自 corrected code0 0x684c1，对应 FD2.EXE file offset 0x792c1。 */
const uint8_t *fd2_field_item_record_get(uint8_t item_id);
int16_t fd2_field_item_i16(const uint8_t *record, size_t offset);

#endif /* FD2_FIELD_ITEM_H */
