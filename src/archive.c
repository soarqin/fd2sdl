#include "archive.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 读取小端 u32 */
static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int fd2_archive_open(fd2_archive *ar, const char *path) {
    memset(ar, 0, sizeof(*ar));

    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return -1; }

    uint8_t *buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, (size_t)sz, f) != (size_t)sz) {
        free(buf); fclose(f); return -1;
    }
    fclose(f);

    if (sz < FD2_ARCHIVE_MAGIC_LEN ||
        memcmp(buf, FD2_ARCHIVE_MAGIC, FD2_ARCHIVE_MAGIC_LEN) != 0) {
        free(buf);
        fprintf(stderr, "fd2_archive: bad magic in %s\n", path);
        return -1;
    }

    ar->path = strdup(path);
    ar->data = buf;
    ar->size = (size_t)sz;
    ar->owns_data = 1;

    /* 读取偏移流：严格递增、均在文件范围内 */
    size_t cap = 16;
    ar->offsets = malloc(cap * sizeof(uint32_t));
    ar->count = 0;
    size_t i = 0;
    while (1) {
        size_t pos = FD2_ARCHIVE_MAGIC_LEN + i * 4;
        if (pos + 4 > ar->size) break;
        uint32_t o = rd_u32_le(ar->data + pos);
        if (o >= ar->size) break;                 /* 越界 */
        if (ar->count && o <= ar->offsets[ar->count - 1]) break; /* 非递增 */
        if (ar->count == cap) {
            cap *= 2;
            ar->offsets = realloc(ar->offsets, cap * sizeof(uint32_t));
        }
        ar->offsets[ar->count++] = o;
        i++;
    }
    return 0;
}

/* 从内存缓冲区打开 .DAT 归档（用于嵌套归档）。
 * 复现 FUN_0000e902 @0x463ce 的资源加载：FDOTHER.DAT[7] 是嵌套 .DAT。
 * 不拷贝数据，不负责释放 buf。 */
int fd2_archive_open_mem(fd2_archive *ar, const uint8_t *buf, size_t sz) {
    memset(ar, 0, sizeof(*ar));

    if (sz < FD2_ARCHIVE_MAGIC_LEN ||
        memcmp(buf, FD2_ARCHIVE_MAGIC, FD2_ARCHIVE_MAGIC_LEN) != 0) {
        fprintf(stderr, "fd2_archive_open_mem: bad magic\n");
        return -1;
    }

    ar->data = (uint8_t *)buf;
    ar->size = sz;
    ar->owns_data = 0;

    size_t cap = 16;
    ar->offsets = malloc(cap * sizeof(uint32_t));
    ar->count = 0;
    size_t i = 0;
    while (1) {
        size_t pos = FD2_ARCHIVE_MAGIC_LEN + i * 4;
        if (pos + 4 > ar->size) break;
        uint32_t o = rd_u32_le(ar->data + pos);
        if (o >= ar->size) break;
        if (ar->count && o <= ar->offsets[ar->count - 1]) break;
        if (ar->count == cap) {
            cap *= 2;
            ar->offsets = realloc(ar->offsets, cap * sizeof(uint32_t));
        }
        ar->offsets[ar->count++] = o;
        i++;
    }
    return 0;
}

int fd2_archive_get(const fd2_archive *ar, size_t idx,
                     const uint8_t **out_ptr, size_t *out_len) {
    if (idx >= ar->count) return -1;
    uint32_t o = ar->offsets[idx];
    uint32_t nxt = (idx + 1 < ar->count) ? ar->offsets[idx + 1]
                                         : (uint32_t)ar->size;
    if (o >= ar->size || nxt > ar->size || nxt < o) return -1;
    *out_ptr = ar->data + o;
    *out_len = (size_t)(nxt - o);
    return 0;
}

void fd2_archive_close(fd2_archive *ar) {
    free(ar->path);
    if (ar->owns_data) free(ar->data);
    free(ar->offsets);
    memset(ar, 0, sizeof(*ar));
}
