/* FDTXT 片段读取：依据原版资源的 u16 偏移表与 token 流，
 * 格式及控制码见 docs/formats/data-formats.md；读取不拥有归档数据。 */
#include "text.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

int fd2_text_entry_open_mem(fd2_text_entry *txt,
                             const uint8_t *data, size_t size) {
    memset(txt, 0, sizeof(*txt));
    if (!data || size < 2) return -1;

    uint16_t first = rd_u16_le(data);
    if (first == 0 || (first & 1) != 0 || first > size) return -1;

    size_t count = first / 2;
    if (count == 0 || count > size / 2) return -1;

    uint16_t *offsets = malloc(count * sizeof(*offsets));
    if (!offsets) return -1;

    for (size_t i = 0; i < count; i++) {
        uint16_t off = rd_u16_le(data + i * 2);
        if (off < first || off >= size) {
            free(offsets);
            return -1;
        }
        if (i > 0 && off <= offsets[i - 1]) {
            free(offsets);
            return -1;
        }
        offsets[i] = off;
    }

    txt->data = data;
    txt->size = size;
    txt->offsets = offsets;
    txt->fragment_count = count;
    return 0;
}

int fd2_text_entry_open_entry(fd2_text_entry *txt,
                               const fd2_archive *ar, size_t entry_idx) {
    const uint8_t *data;
    size_t size;
    if (fd2_archive_get(ar, entry_idx, &data, &size) != 0) return -1;
    return fd2_text_entry_open_mem(txt, data, size);
}

int fd2_text_entry_get_fragment(const fd2_text_entry *txt, size_t fragment_idx,
                                 const uint8_t **out_tokens,
                                 size_t *out_token_count) {
    if (!txt || !txt->data || !txt->offsets) return -1;
    if (fragment_idx >= txt->fragment_count) return -1;

    size_t start = txt->offsets[fragment_idx];
    size_t end = (fragment_idx + 1 < txt->fragment_count)
               ? txt->offsets[fragment_idx + 1]
               : txt->size;
    if (end > txt->size || end < start || ((end - start) & 1) != 0) return -1;

    if (out_tokens) *out_tokens = txt->data + start;
    if (out_token_count) *out_token_count = (end - start) / 2;
    return 0;
}

uint16_t fd2_text_token_at(const uint8_t *tokens, size_t token_idx) {
    return rd_u16_le(tokens + token_idx * 2);
}

/* 既有field_play受限展开的共享实现；原依据text_dialog_render_tokens @0x15f84。
 * 不绘制、不等待、不写动态消息owner，也不消费RNG。 */
static int expanded_append(fd2_text_expanded *out, uint16_t token, size_t glyph_count) {
    if (out->count >= FD2_TEXT_EXPANDED_CAPACITY ||
        (token < 0xff00u && token >= glyph_count)) return -1;
    out->tokens[out->count++] = token;
    return 0;
}

static int expand_fragment(const fd2_text_entry *entry, size_t glyph_count,
        const fd2_text_parameters *parameters, size_t fragment, unsigned depth,
        fd2_text_expanded *out) {
    const uint8_t *tokens; size_t count;
    if (depth > 1 || fd2_text_entry_get_fragment(entry, fragment, &tokens, &count) != 0) return -1;
    for (size_t i = 0; i < count; i++) {
        uint16_t token = fd2_text_token_at(tokens, i);
        if (token == 0xffffu) return 0;
        if (token == 0xfffau) {
            char number[16];
            int32_t value = parameters->value <= INT32_MAX ? (int32_t)parameters->value
                : -1 - (int32_t)(UINT32_MAX - parameters->value);
            (void)snprintf(number, sizeof(number), "%ld", (long)value);
            for (size_t j = 0; number[j]; j++)
                if (expanded_append(out, number[j] == '-' ? FD2_TEXT_SAFE_MINUS :
                        (uint16_t)(number[j] - '0'), glyph_count) != 0) return -1;
        } else if (token == 0xfffcu || token == 0xfffbu) {
            size_t nested = token == 0xfffcu ? parameters->new_fragment : parameters->old_fragment;
            if (depth || nested == SIZE_MAX ||
                expand_fragment(entry, glyph_count, parameters, nested, depth + 1, out) != 0) return -1;
        } else {
            if (token >= 0xff00u && token != 0xfffeu && token != 0xfffdu) return -1;
            if (expanded_append(out, token, glyph_count) != 0) return -1;
        }
    }
    return -1;
}

int fd2_text_expand(const fd2_text_entry *entry, size_t glyph_count,
        const fd2_text_parameters *parameters, size_t fragment, fd2_text_expanded *out) {
    if (!entry || !glyph_count || !parameters || !out) return -1;
    memset(out, 0, sizeof(*out));
    return expand_fragment(entry, glyph_count, parameters, fragment, 0, out);
}

void fd2_text_entry_close(fd2_text_entry *txt) {
    free(txt->offsets);
    memset(txt, 0, sizeof(*txt));
}
