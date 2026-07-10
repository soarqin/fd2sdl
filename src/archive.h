#ifndef FD2_ARCHIVE_H
#define FD2_ARCHIVE_H

#include <stddef.h>
#include <stdint.h>

/* 炎龙骑士团 2 .DAT 归档读取
 * 格式见 docs/03-data-formats.md：
 *   [0:6]   magic "LLLLLL"
 *   [6:]    u32 偏移流，严格递增
 *   末条目至文件末尾
 */

#define FD2_ARCHIVE_MAGIC "LLLLLL"
#define FD2_ARCHIVE_MAGIC_LEN 6

typedef struct {
    char     *path;       /* 源文件路径（仅供调试） */
    uint8_t  *data;       /* 整个文件载入内存 */
    size_t    size;       /* 文件大小 */
    uint32_t *offsets;    /* 条目偏移表 */
    size_t    count;      /* 条目数 */
    int       owns_data;  /* 是否拥有 data 内存（open_mem 时不拥有） */
} fd2_archive;

/* 加载 .DAT 文件。成功返回 0，失败返回 -1 并设置 errno。 */
int  fd2_archive_open(fd2_archive *ar, const char *path);

/* 从内存缓冲区打开 .DAT 归档（用于嵌套归档）。不拷贝数据，不负责释放。 */
int  fd2_archive_open_mem(fd2_archive *ar, const uint8_t *data, size_t size);

/* 获取第 idx 条目，写入 *out_ptr / *out_len（指向 ar 内部内存，无需释放）。 */
int  fd2_archive_get(const fd2_archive *ar, size_t idx,
                     const uint8_t **out_ptr, size_t *out_len);

/* 释放资源。 */
void fd2_archive_close(fd2_archive *ar);

#endif /* FD2_ARCHIVE_H */
