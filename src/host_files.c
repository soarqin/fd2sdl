/* 宿主文件边界。配置与 FD2.SAV 共用 UTF-8 路径及同目录原子替换。 */
#ifndef _WIN32
#define _XOPEN_SOURCE 700
#endif
#include "host_files.h"
#include <SDL3/SDL.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
static wchar_t *wide_path(const char *path) {
    int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, NULL, 0);
    wchar_t *wide = n ? malloc((size_t)n * sizeof(*wide)) : NULL;
    if (wide && MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, path, -1, wide, n)) return wide;
    free(wide);
    return NULL;
}
#else
#include <fcntl.h>
#include <unistd.h>
#endif

FILE *fd2_open_read(const char *path) {
#ifdef _WIN32
    wchar_t *wide = wide_path(path);
    if (!wide) { errno = EINVAL; return NULL; }
    FILE *f = _wfopen(wide, L"rb");
    free(wide);
    return f;
#else
    return fopen(path, "rb");
#endif
}

/* SDL_GetPathInfo 不公开解析后的目标名。只有文件系统确认为缺失才能跳过。 */
static char *resolved(const char *path, bool *missing) {
    *missing = false;
#ifdef _WIN32
    wchar_t *wide = wide_path(path);
    if (!wide) return NULL;
    HANDLE h = CreateFileW(wide, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE |
        FILE_SHARE_DELETE, NULL, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
    DWORD error = GetLastError();
    free(wide);
    if (h == INVALID_HANDLE_VALUE) {
        *missing = error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND;
        return NULL;
    }
    DWORD n = GetFinalPathNameByHandleW(h, NULL, 0, FILE_NAME_NORMALIZED);
    wchar_t *name = n ? malloc((size_t)n * sizeof(*name)) : NULL;
    DWORD len = name ? GetFinalPathNameByHandleW(h, name, n, FILE_NAME_NORMALIZED) : 0;
    CloseHandle(h);
    char *out = NULL;
    if (len && len < n) {
        int bytes = WideCharToMultiByte(CP_UTF8, 0, name, -1, NULL, 0, NULL, NULL);
        out = bytes ? malloc(bytes) : NULL;
        if (out && !WideCharToMultiByte(CP_UTF8, 0, name, -1, out, bytes, NULL, NULL)) {
            free(out); out = NULL;
        }
    }
    free(name);
    return out;
#else
    char *out = realpath(path, NULL);
    *missing = !out && errno == ENOENT;
    return out;
#endif
}

static bool within(const char *path, const char *root) {
    size_t n = strlen(root);
    while (n && (root[n-1] == '/'
#ifdef _WIN32
                 || root[n-1] == '\\'
#endif
                 )) --n;
#ifdef _WIN32
    return strlen(path) >= n && SDL_strncasecmp(path, root, n) == 0 && (path[n] == '\\' || !path[n]);
#else
    return strncmp(path, root, n) == 0 && (path[n] == '/' || !path[n]);
#endif
}

static char *parent_directory(const char *path) {
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *back = strrchr(path, '\\');
    if (back && (!slash || back > slash)) slash = back;
#endif
    return slash ? SDL_strndup(path, (size_t)(slash - path) + 1) : SDL_strdup(".");
}

static bool outside_game(const char *directory, const char *existing, const char *game_dir) {
    const char *roots[] = {"original_game", game_dir};
    for (size_t i = 0; i < SDL_arraysize(roots); ++i) {
        bool missing;
        char *root = resolved(roots[i], &missing);
        bool ok = root ? !within(directory, root) && (!existing || !within(existing, root)) : missing;
        free(root);
        if (!ok) return false;
    }
    return true;
}

char *fd2_capture_directory(const char *path, const char *game_dir) {
    bool missing;
    char *dir = resolved(path, &missing);
    if (!dir) return NULL;
    /* 既有 PCM 证据协议还保护任一名为 original_game 的祖先目录。 */
#ifdef _WIN32
    const char *separators = "/\\";
#else
    const char *separators = "/";
#endif
    bool ok = outside_game(dir, NULL, game_dir);
    for (const char *at = dir; ok && *at;) {
        at += strspn(at, separators);
        size_t n = strcspn(at, separators);
#ifdef _WIN32
        ok = n != 13 || SDL_strncasecmp(at, "original_game", n) != 0;
#else
        ok = n != 13 || strncmp(at, "original_game", n) != 0;
#endif
        at += n;
    }
    if (ok) return dir;
    SDL_SetError("protected capture directory: %s", path);
    free(dir);
    return NULL;
}

static bool writable_destination(const char *path, const char *game_dir) {
#ifdef _WIN32
    /* C:name 的父目录取决于每盘当前目录。拒绝该写法及 NTFS 命名流。 */
    const char *name = !strncmp(path, "\\\\?\\", 4) ? path + 4 : path;
    const char *colon = strchr(name, ':');
    if (colon && (colon != name + 1 || !SDL_isalpha((unsigned char)*name) ||
        (colon[1] != '/' && colon[1] != '\\') || strchr(colon + 1, ':')))
        return SDL_SetError("output path requires a regular file: %s", path);
#endif
    char *parent = parent_directory(path);
    bool missing;
    char *directory = parent ? resolved(parent, &missing) : NULL;
    SDL_free(parent);
    char *existing = resolved(path, &missing);
    bool ok = directory && (existing || missing) && outside_game(directory, existing, game_dir);
    free(directory);
    free(existing);
    if (!ok) SDL_SetError("protected or inaccessible output path: %s", path);
    return ok;
}

#if !defined(_WIN32)
static int sync_parent_directory(const char *path) {
    if (!path || path[0] == '\0') return -1;
    char *directory = parent_directory(path);
    if (!directory) return -1;
    int flags = O_RDONLY;
#ifdef O_DIRECTORY
    flags |= O_DIRECTORY;
#endif
    int fd = open(directory, flags);
    SDL_free(directory);
    if (fd < 0) return -1;
    int sync_result = fsync(fd);
    int close_result = close(fd);
    return sync_result == 0 && close_result == 0 ? 0 : -1;
}
#endif

int fd2_replace_file(const char *path, const void *data, size_t size, const char *game_dir) {
    if (!path || !*path || !data || !game_dir || !writable_destination(path, game_dir))
        return -1;
    size_t path_len = strlen(path);
#if defined(_WIN32)
    static LONG temp_counter;
    size_t tmp_capacity = path_len + 48u;
    char *tmp_path = malloc(tmp_capacity);
    HANDLE file = INVALID_HANDLE_VALUE;
    if (!tmp_path) {
        return -1;
    }
    wchar_t *destination = wide_path(path), *temporary = NULL;
    if (!destination) { free(tmp_path); return -1; }
    /* CREATE_NEW 拒绝已有文件和符号链接；进程 ID 与原子序号也避免同一
     * 目录内并发 writer 共用固定 .tmp 名称。 */
    for (unsigned int attempt = 0; attempt < 128u; attempt++) {
        unsigned long counter = (unsigned long)InterlockedIncrement(
            &temp_counter);
        int length = snprintf(tmp_path, tmp_capacity, "%s.tmp.%lu.%lu",
                              path, (unsigned long)GetCurrentProcessId(),
                              counter);
        if (length < 0 || (size_t)length >= tmp_capacity) break;
        free(temporary);
        temporary = wide_path(tmp_path);
        if (!temporary) break;
        file = CreateFileW(temporary, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) break;
        if (GetLastError() != ERROR_FILE_EXISTS &&
            GetLastError() != ERROR_ALREADY_EXISTS)
            break;
    }
    if (file == INVALID_HANDLE_VALUE) {
        free(temporary);
        free(destination);
        free(tmp_path);
        return -1;
    }
    size_t offset = 0;
    int result = 0;
    while (offset < size) {
        DWORD chunk = (DWORD)(size - offset);
        DWORD written = 0;
        if (!WriteFile(file, (const uint8_t *)data + offset, chunk, &written, NULL) ||
            written == 0) {
            result = -1;
            break;
        }
        offset += written;
    }
    if (result == 0 && !FlushFileBuffers(file)) result = -1;
    if (!CloseHandle(file)) result = -1;
    if (result == 0 &&
        !MoveFileExW(temporary, destination,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        /* 已完整写入的临时文件保留作恢复；原目标仍然存在。 */
        result = -1;
    } else if (result != 0) {
        (void)DeleteFileW(temporary);
    }
    free(temporary);
    free(destination);
#else
    static const char suffix[] = ".tmp.XXXXXX";
    if (path_len > SIZE_MAX - sizeof(suffix)) {
        return -1;
    }
    char *tmp_path = malloc(path_len + sizeof(suffix));
    if (!tmp_path) {
        return -1;
    }
    memcpy(tmp_path, path, path_len);
    memcpy(tmp_path + path_len, suffix, sizeof(suffix));
    /* mkstemp 在目标同目录以 O_CREAT|O_EXCL 建立 0600 文件，不跟随预置
     * 符号链接，也不与另一个 writer 共享固定临时路径。 */
    int fd = mkstemp(tmp_path);
    if (fd < 0) {
        free(tmp_path);
        return -1;
    }
    FILE *file = fdopen(fd, "wb");
    int result = 0;
    if (!file) {
        (void)close(fd);
        result = -1;
    } else {
        size_t written = fwrite(data, 1, size, file);
        int sync_result = fflush(file);
        /* IDBFS 的 fsync 同步整个挂载点，只在 rename 后同步。 */
#ifndef __EMSCRIPTEN__
        if (sync_result == 0) sync_result = fsync(fd);
#endif
        int close_result = fclose(file);
        result = written == size && sync_result == 0 && close_result == 0 ? 0 : -1;
    }
    if (result == 0 && rename(tmp_path, path) != 0) {
        result = -1;
    } else if (result == 0 && sync_parent_directory(path) != 0) {
        /* 替换已完成，仍需向调用方报告持久化失败。 */
        result = -1;
    }
    if (result != 0)
        (void)remove(tmp_path);
#endif
    free(tmp_path);
    return result;
}
