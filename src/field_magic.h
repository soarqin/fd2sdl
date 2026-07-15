#ifndef FD2_FIELD_MAGIC_H
#define FD2_FIELD_MAGIC_H

#include <stddef.h>
#include <stdint.h>

#define FD2_FIELD_MAGIC_RECORD_SIZE 7u
#define FD2_FIELD_MAGIC_COUNT 36u

/* field_magic_record_get @0x4e866：DS:0x19fd + magic_id*7。
 * 表内容来自 object3 `DS:0x19fd`，对应 FD2.EXE file offset `0x7aa11`。
 * 字段在 AI 评分层暂按原始 offset 使用，避免提前赋予玩法语义。 */
const uint8_t *fd2_field_magic_record_get(uint8_t magic_id);
uint16_t fd2_field_magic_u16(const uint8_t *record, size_t offset);

#endif /* FD2_FIELD_MAGIC_H */
