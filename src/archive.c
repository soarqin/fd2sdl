/* .DAT 资源归档；嵌套加载依据 res_load @0x111ba（旧名 FUN_0000e902），
 * 已确认容器格式见 docs/formats/data-formats.md §1。 */
#include "archive.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int fd2_archive_open(fd2_archive *ar, const char *path) {
    if (!ar) { errno = EINVAL; return -1; }
    memset(ar, 0, sizeof(*ar));
    if (!path) { errno = EINVAL; return -1; }

    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    long sz;
    if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) < 0 ||
        fseek(f, 0, SEEK_SET) != 0) {
        int error = errno;
        fclose(f);
        errno = error;
        return -1;
    }
    if (sz < FD2_ARCHIVE_MAGIC_LEN + 4 || (uintmax_t)sz > UINT32_MAX) {
        fclose(f);
        errno = EINVAL;
        return -1;
    }

    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); errno = ENOMEM; return -1; }
    size_t got = fread(buf, 1, (size_t)sz, f);
    int close_result = fclose(f);
    if (got != (size_t)sz || close_result != 0) {
        free(buf);
        errno = EIO;
        return -1;
    }
    if (fd2_archive_open_mem(ar, buf, (size_t)sz) != 0) {
        free(buf);
        return -1;
    }
    ar->owns_data = 1;
    return 0;
}

int fd2_archive_open_mem(fd2_archive *ar, const uint8_t *buf, size_t sz) {
    if (!ar) { errno = EINVAL; return -1; }
    memset(ar, 0, sizeof(*ar));
    if (!buf || sz < FD2_ARCHIVE_MAGIC_LEN + 4 || sz > UINT32_MAX ||
        memcmp(buf, FD2_ARCHIVE_MAGIC, FD2_ARCHIVE_MAGIC_LEN) != 0) {
        errno = EINVAL;
        return -1;
    }

    size_t table_end = rd_u32_le(buf + FD2_ARCHIVE_MAGIC_LEN);
    if (table_end < FD2_ARCHIVE_MAGIC_LEN + 4 || table_end >= sz) {
        errno = EINVAL;
        return -1;
    }
    /* 偏移流在首条目之前结束；嵌套归档不要求顶层的四字节间隙。 */
    size_t count = 0;
    uint32_t previous = 0;
    for (size_t pos = FD2_ARCHIVE_MAGIC_LEN; pos <= table_end - 4; pos += 4) {
        uint32_t offset = rd_u32_le(buf + pos);
        if (offset >= sz || (count && offset <= previous)) break;
        previous = offset;
        count++;
    }
    uint32_t *offsets = malloc(count * sizeof(*offsets));
    if (!offsets) { errno = ENOMEM; return -1; }
    for (size_t i = 0; i < count; i++)
        offsets[i] = rd_u32_le(buf + FD2_ARCHIVE_MAGIC_LEN + i * 4);

    ar->data = (uint8_t *)buf;
    ar->size = sz;
    ar->offsets = offsets;
    ar->count = count;
    return 0;
}

int fd2_archive_get(const fd2_archive *ar, size_t idx,
                     const uint8_t **out_ptr, size_t *out_len) {
    if (!ar || !ar->data || !ar->offsets || !out_ptr || !out_len ||
        idx >= ar->count) {
        errno = EINVAL;
        return -1;
    }
    size_t start = ar->offsets[idx];
    size_t end = idx + 1 < ar->count ? ar->offsets[idx + 1] : ar->size;
    if (start >= ar->size || end > ar->size || end < start) {
        errno = EINVAL;
        return -1;
    }
    *out_ptr = ar->data + start;
    *out_len = end - start;
    return 0;
}

void fd2_archive_close(fd2_archive *ar) {
    if (!ar) return;
    if (ar->owns_data) free(ar->data);
    free(ar->offsets);
    memset(ar, 0, sizeof(*ar));
}
