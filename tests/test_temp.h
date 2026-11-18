/* 文件回归使用宿主临时目录，不依赖 Linux 的 /tmp，也不写原版证据。 */
#ifndef FD2_TEST_TEMP_H
#define FD2_TEST_TEMP_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#endif

#define FD2_TEST_PATH_MAX 4096

/* 返回独占创建的空文件；调用者按场景删除，并负责最终清理。 */
static int fd2_test_temp_path(char path[FD2_TEST_PATH_MAX]) {
#if defined(_WIN32)
    char directory[MAX_PATH];
    DWORD length = GetTempPathA(MAX_PATH, directory);
    if (length == 0 || length >= MAX_PATH) return -1;
    return GetTempFileNameA(directory, "fd2", 0, path) != 0 ? 0 : -1;
#else
    const char *directory = getenv("TMPDIR");
    if (!directory || !*directory) directory = "/tmp";
    int length = snprintf(path, FD2_TEST_PATH_MAX, "%s/fd2sdl-test-XXXXXX", directory);
    if (length < 0 || length >= FD2_TEST_PATH_MAX) return -1;
    int fd = mkstemp(path);
    if (fd < 0) return -1;
    return close(fd);
#endif
}

#endif
