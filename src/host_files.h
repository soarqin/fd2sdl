#ifndef FD2_HOST_FILES_H
#define FD2_HOST_FILES_H
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
/* 返回已解析且在资源树外的 PCM 取证目录；调用方用 free 释放。 */
char *fd2_capture_directory(const char *path, const char *game_dir);
FILE *fd2_open_read(const char *path);
/* 同目录独占临时文件，拒绝写到实际资源树，成功后才替换目标。 */
int fd2_replace_file(const char *path, const void *data, size_t size, const char *game_dir);
#ifdef __cplusplus
}
#endif
#endif
