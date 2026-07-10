#ifndef FD2_TEXT_H
#define FD2_TEXT_H

#include <stddef.h>
#include <stdint.h>

#include "archive.h"

/* FDTXT.DAT 文本/脚本片段读取
 *
 * 实测每个条目以 u16 偏移表开头；第一个偏移值即表长度，
 * 因此 fragment_count = first_offset / 2。片段内容是 u16 token 流，
 * 不是可直接按 Big5/GBK 解码的字节串；0xffff/0xfffe/0xffef 等值表现为控制码。
 */

typedef struct {
    const uint8_t *data; /* 指向归档条目内部，不拥有 */
    size_t size;
    uint16_t *offsets;
    size_t fragment_count;
} fd2_text_entry;

int  fd2_text_entry_open_mem(fd2_text_entry *txt,
                             const uint8_t *data, size_t size);
int  fd2_text_entry_open_entry(fd2_text_entry *txt,
                               const fd2_archive *ar, size_t entry_idx);

/* 返回片段的 u16 token 数量。out_tokens 指向归档条目内部，需按小端读取。 */
int  fd2_text_entry_get_fragment(const fd2_text_entry *txt, size_t fragment_idx,
                                 const uint8_t **out_tokens,
                                 size_t *out_token_count);

uint16_t fd2_text_token_at(const uint8_t *tokens, size_t token_idx);

void fd2_text_entry_close(fd2_text_entry *txt);

#endif /* FD2_TEXT_H */
