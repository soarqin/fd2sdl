#include "text.h"

#include <stdlib.h>
#include <string.h>

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
    if (end < start || ((end - start) & 1) != 0) return -1;

    if (out_tokens) *out_tokens = txt->data + start;
    if (out_token_count) *out_token_count = (end - start) / 2;
    return 0;
}

uint16_t fd2_text_token_at(const uint8_t *tokens, size_t token_idx) {
    return rd_u16_le(tokens + token_idx * 2);
}

void fd2_text_entry_close(fd2_text_entry *txt) {
    free(txt->offsets);
    memset(txt, 0, sizeof(*txt));
}
