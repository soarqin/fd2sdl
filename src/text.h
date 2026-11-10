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

/* 15f84的有限片段展开；复用战场提示与准备服务，不解释流程或拥有热状态。
 * -4/-5仅展开一层动态片段，-6按signed DWORD展开数字，保留-2/-3给呈现owner。
 * SAFE_MINUS只由数字展开产生，资源流不能直接请求；使用者验证/绘制安全负号。
 * 成功0，坏字形/递归/控制码/容量返回-1；out为临时工作结果，失败须丢弃。 */
#define FD2_TEXT_SAFE_MINUS 0xffefu
#define FD2_TEXT_EXPANDED_CAPACITY 128u
typedef struct { uint32_t value; size_t new_fragment, old_fragment; } fd2_text_parameters;
typedef struct {
    uint16_t tokens[FD2_TEXT_EXPANDED_CAPACITY];
    size_t count;
} fd2_text_expanded;
int fd2_text_expand(const fd2_text_entry *entry, size_t glyph_count,
    const fd2_text_parameters *parameters, size_t fragment, fd2_text_expanded *out);
/* 15f84主正文可来自当前stage，-4/-5仍索引同一UI0 names；复用同一展开器。 */
int fd2_text_expand_banked(const fd2_text_entry *primary, const fd2_text_entry *names,
    size_t glyph_count, const fd2_text_parameters *parameters, size_t fragment,
    fd2_text_expanded *out);
/* 15f84的-6路径：独立展开同一个signed DWORD，供长Scene流逐项消费。
 * SAFE_MINUS保持来源标记，不能与原资源的-17说话人控制码混为一谈。
 * 不依赖伪造的临时FDTXT片段，不绘制/等待/读取输入或RNG。 */
int fd2_text_expand_number(uint32_t value, size_t glyph_count,
    fd2_text_expanded *out);

void fd2_text_entry_close(fd2_text_entry *txt);

#endif /* FD2_TEXT_H */
