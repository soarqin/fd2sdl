/* 炎龙骑士团 2 SDL3 重写 - FD2.SAV 保留式读改写
 *
 * 逆向依据：save_checksum_sum @code0 0x3df09（dual 0x4df09）对
 * size-4 个明文字节求和；save_xor_crypt @code0 0x3df28
 * （dual 0x4df28）执行对称 XOR。战场写档路径 code0
 * 0x9f7a..0xa136 读取并解密完整 0x59cb 字节，更新状态、重算尾部
 * checksum，再加密写回。因此本实现不重建未知区域。
 */

#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif

#include "save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void wr_u32_le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

void fd2_save_xor_crypt(uint8_t *data, size_t size) {
    if (!data) return;
    uint16_t state = 0x00a5;
    for (size_t i = 0; i < size; i++) {
        state = (uint16_t)(state + 0x9014u);
        state = (uint16_t)((state << 3) | (state >> 13));
        data[i] ^= (uint8_t)state;
    }
}

uint32_t fd2_save_checksum_sum(const uint8_t *data, size_t size) {
    if (!data || size < FD2_SAVE_CHECKSUM_SIZE) return 0;
    uint32_t sum = 0;
    for (size_t i = 0; i < size - FD2_SAVE_CHECKSUM_SIZE; i++)
        sum += data[i];
    return sum;
}

int fd2_save_validate_plain(const uint8_t *data, size_t size) {
    if (!data || size != FD2_SAVE_FILE_SIZE) return -1;
    return fd2_save_checksum_sum(data, size) ==
           rd_u32_le(data + size - FD2_SAVE_CHECKSUM_SIZE) ? 0 : -1;
}

int fd2_save_file_open_encrypted(fd2_save_file *save,
                                 const uint8_t *encrypted, size_t size) {
    if (!save || !encrypted || save->data || size != FD2_SAVE_FILE_SIZE)
        return -1;
    uint8_t *data = malloc(size);
    if (!data) return -1;
    memcpy(data, encrypted, size);
    fd2_save_xor_crypt(data, size);
    if (fd2_save_validate_plain(data, size) != 0) {
        free(data);
        return -1;
    }
    save->data = data;
    save->size = size;
    return 0;
}

int fd2_save_file_create_empty(fd2_save_file *save) {
    if (!save || save->data) return -1;
    uint8_t *data = malloc(FD2_SAVE_FILE_SIZE);
    if (!data) return -1;
    memset(data, 0xff, FD2_SAVE_FILE_SIZE);
    wr_u32_le(data + FD2_SAVE_FILE_SIZE - FD2_SAVE_CHECKSUM_SIZE,
              fd2_save_checksum_sum(data, FD2_SAVE_FILE_SIZE));
    save->data = data;
    save->size = FD2_SAVE_FILE_SIZE;
    return 0;
}

int fd2_save_file_open(fd2_save_file *save, const char *path) {
    if (!save || !path || save->data) return -1;
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    uint8_t *encrypted = malloc(FD2_SAVE_FILE_SIZE);
    if (!encrypted) {
        fclose(file);
        return -1;
    }
    size_t got = fread(encrypted, 1, FD2_SAVE_FILE_SIZE, file);
    int extra = fgetc(file);
    int read_error = ferror(file);
    int close_result = fclose(file);
    if (got != FD2_SAVE_FILE_SIZE || extra != EOF || read_error ||
        close_result != 0) {
        free(encrypted);
        return -1;
    }
    int result = fd2_save_file_open_encrypted(
        save, encrypted, FD2_SAVE_FILE_SIZE);
    free(encrypted);
    return result;
}

void fd2_save_file_close(fd2_save_file *save) {
    if (!save) return;
    free(save->data);
    memset(save, 0, sizeof(*save));
}

#if !defined(_WIN32)
static int sync_parent_directory(const char *path) {
    if (!path || path[0] == '\0') return -1;
    const char *slash = strrchr(path, '/');
    char *directory = NULL;
    if (!slash) {
        directory = strdup(".");
    } else if (slash == path) {
        directory = strdup("/");
    } else {
        size_t length = (size_t)(slash - path);
        directory = malloc(length + 1u);
        if (directory) {
            memcpy(directory, path, length);
            directory[length] = '\0';
        }
    }
    if (!directory) return -1;
    int flags = O_RDONLY;
#ifdef O_DIRECTORY
    flags |= O_DIRECTORY;
#endif
    int fd = open(directory, flags);
    free(directory);
    if (fd < 0) return -1;
    int sync_result = fsync(fd);
    int close_result = close(fd);
    return sync_result == 0 && close_result == 0 ? 0 : -1;
}
#endif

int fd2_save_file_write(const fd2_save_file *save, const char *path) {
    if (!save || !save->data || !path || path[0] == '\0' ||
        save->size != FD2_SAVE_FILE_SIZE ||
        fd2_save_validate_plain(save->data, save->size) != 0)
        return -1;
    uint8_t *encrypted = malloc(save->size);
    if (!encrypted) return -1;
    memcpy(encrypted, save->data, save->size);
    fd2_save_xor_crypt(encrypted, save->size);

    size_t path_len = strlen(path);
#if defined(_WIN32)
    static LONG temp_counter;
    size_t tmp_capacity = path_len + 48u;
    char *tmp_path = malloc(tmp_capacity);
    HANDLE file = INVALID_HANDLE_VALUE;
    if (!tmp_path) {
        free(encrypted);
        return -1;
    }
    /* CREATE_NEW 拒绝已有文件和符号链接；进程 ID 与原子序号也避免同一
     * 目录内并发 writer 共用固定 .tmp 名称。 */
    for (unsigned int attempt = 0; attempt < 128u; attempt++) {
        unsigned long counter = (unsigned long)InterlockedIncrement(
            &temp_counter);
        int length = snprintf(tmp_path, tmp_capacity, "%s.tmp.%lu.%lu",
                              path, (unsigned long)GetCurrentProcessId(),
                              counter);
        if (length < 0 || (size_t)length >= tmp_capacity) break;
        file = CreateFileA(tmp_path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, NULL);
        if (file != INVALID_HANDLE_VALUE) break;
        if (GetLastError() != ERROR_FILE_EXISTS &&
            GetLastError() != ERROR_ALREADY_EXISTS)
            break;
    }
    if (file == INVALID_HANDLE_VALUE) {
        free(tmp_path);
        free(encrypted);
        return -1;
    }
    size_t offset = 0;
    int result = 0;
    while (offset < save->size) {
        DWORD chunk = (DWORD)(save->size - offset);
        DWORD written = 0;
        if (!WriteFile(file, encrypted + offset, chunk, &written, NULL) ||
            written == 0) {
            result = -1;
            break;
        }
        offset += written;
    }
    if (result == 0 && !FlushFileBuffers(file)) result = -1;
    if (!CloseHandle(file)) result = -1;
    if (result == 0 &&
        !MoveFileExA(tmp_path, path,
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        /* 已完整写入的临时文件保留作恢复；原目标仍然存在。 */
        result = -1;
    } else if (result != 0) {
        (void)DeleteFileA(tmp_path);
    }
#else
    static const char suffix[] = ".tmp.XXXXXX";
    if (path_len > SIZE_MAX - sizeof(suffix)) {
        free(encrypted);
        return -1;
    }
    char *tmp_path = malloc(path_len + sizeof(suffix));
    if (!tmp_path) {
        free(encrypted);
        return -1;
    }
    memcpy(tmp_path, path, path_len);
    memcpy(tmp_path + path_len, suffix, sizeof(suffix));
    /* mkstemp 在目标同目录以 O_CREAT|O_EXCL 建立 0600 文件，不跟随预置
     * 符号链接，也不与另一个 writer 共享固定临时路径。 */
    int fd = mkstemp(tmp_path);
    if (fd < 0) {
        free(tmp_path);
        free(encrypted);
        return -1;
    }
    FILE *file = fdopen(fd, "wb");
    int result = 0;
    if (!file) {
        (void)close(fd);
        result = -1;
    } else {
        size_t written = fwrite(encrypted, 1, save->size, file);
        int flush_result = fflush(file);
        int sync_result = flush_result == 0 ? fsync(fd) : -1;
        int close_result = fclose(file);
        result = written == save->size && flush_result == 0 &&
                 sync_result == 0 && close_result == 0 ? 0 : -1;
    }
    if (result == 0 && rename(tmp_path, path) != 0) {
        result = -1;
    } else if (result == 0 && sync_parent_directory(path) != 0) {
        /* 文件内容已 fsync；rename 后再同步父目录，使替换在断电后也可恢复。 */
        result = -1;
    }
    if (result != 0)
        (void)remove(tmp_path);
#endif
    free(tmp_path);
    free(encrypted);
    return result;
}

int fd2_save_file_get_battle_snapshot(
        const fd2_save_file *save, fd2_save_battle_snapshot *snapshot) {
    if (!save || !save->data || !snapshot ||
        fd2_save_validate_plain(save->data, save->size) != 0)
        return -1;
    fd2_save_battle_snapshot loaded;
    memcpy(loaded.unit_area, save->data + FD2_SAVE_BATTLE_UNIT_BASE,
           sizeof(loaded.unit_area));
    memcpy(loaded.cell_state,
           save->data + FD2_SAVE_BATTLE_CELL_STATE_BASE,
           sizeof(loaded.cell_state));
    memcpy(loaded.meta, save->data + FD2_SAVE_BATTLE_META_BASE,
           sizeof(loaded.meta));
    if (loaded.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] >
        FD2_SAVE_BATTLE_MAX_UNITS)
        return -1;
    *snapshot = loaded;
    return 0;
}

int fd2_save_file_update_battle_snapshot(
        fd2_save_file *save, const fd2_save_battle_snapshot *snapshot) {
    if (!save || !save->data || !snapshot ||
        fd2_save_validate_plain(save->data, save->size) != 0 ||
        snapshot->meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] >
            FD2_SAVE_BATTLE_MAX_UNITS)
        return -1;
    uint8_t *shadow = malloc(save->size);
    if (!shadow) return -1;
    memcpy(shadow, save->data, save->size);
    memcpy(shadow + FD2_SAVE_BATTLE_UNIT_BASE, snapshot->unit_area,
           sizeof(snapshot->unit_area));
    memcpy(shadow + FD2_SAVE_BATTLE_CELL_STATE_BASE, snapshot->cell_state,
           sizeof(snapshot->cell_state));
    memcpy(shadow + FD2_SAVE_BATTLE_META_BASE, snapshot->meta,
           sizeof(snapshot->meta));
    wr_u32_le(shadow + save->size - FD2_SAVE_CHECKSUM_SIZE,
              fd2_save_checksum_sum(shadow, save->size));
    memcpy(save->data, shadow, save->size);
    free(shadow);
    return 0;
}

static int slot_offset(size_t slot_index, size_t *offset) {
    if (!offset || slot_index >= FD2_SAVE_SLOT_COUNT) return -1;
    size_t value = FD2_SAVE_SLOT_BASE + slot_index * FD2_SAVE_SLOT_SIZE;
    if (value > FD2_SAVE_FILE_SIZE ||
        FD2_SAVE_SLOT_SIZE > FD2_SAVE_FILE_SIZE - value)
        return -1;
    *offset = value;
    return 0;
}

int fd2_save_file_slot_valid(const fd2_save_file *save, size_t slot_index) {
    size_t offset;
    if (!save || !save->data || save->size != FD2_SAVE_FILE_SIZE ||
        slot_offset(slot_index, &offset) != 0)
        return 0;
    const uint8_t *meta = save->data + offset + FD2_SAVE_UNIT_TABLE_SIZE;
    /* 原版 hand_load @code0 0x198f9..0x19934 只检查 slot+0xa00；
     * unit_count 是导入当前 session 时再验证的语义字段。 */
    return meta[0] != 0xffu;
}

int fd2_save_file_get_manual_slot_raw(const fd2_save_file *save,
                                      size_t slot_index,
                                      fd2_save_manual_slot *slot) {
    size_t offset;
    if (!slot || !save || !save->data ||
        save->size != FD2_SAVE_FILE_SIZE ||
        fd2_save_validate_plain(save->data, save->size) != 0 ||
        slot_offset(slot_index, &offset) != 0)
        return -1;

    fd2_save_manual_slot loaded;
    memcpy(loaded.unit_area, save->data + offset,
           sizeof(loaded.unit_area));
    memcpy(loaded.meta, save->data + offset + FD2_SAVE_UNIT_TABLE_SIZE,
           sizeof(loaded.meta));
    *slot = loaded;
    return 0;
}

int fd2_save_file_get_manual_slot(const fd2_save_file *save,
                                  size_t slot_index,
                                  fd2_save_manual_slot *slot) {
    if (!slot || !fd2_save_file_slot_valid(save, slot_index) ||
        fd2_save_file_get_manual_slot_raw(save, slot_index, slot) != 0 ||
        slot->meta[1] > FD2_SAVE_MAX_UNITS)
        return -1;
    return 0;
}

int fd2_save_manual_slot_decode(const fd2_save_manual_slot *slot,
                                fd2_save_manual_state *state) {
    if (!slot || !state || slot->meta[0] == 0xffu ||
        slot->meta[1] > FD2_SAVE_MAX_UNITS)
        return -1;
    fd2_save_manual_state decoded = {0};
    decoded.stage_id = slot->meta[0];
    decoded.unit_count = slot->meta[1];
    decoded.gold_or_flags = rd_u32_le(slot->meta + 2u);
    memcpy(decoded.options, slot->meta + 6u, sizeof(decoded.options));
    memcpy(decoded.unit_area, slot->unit_area, sizeof(decoded.unit_area));
    *state = decoded;
    return 0;
}

int fd2_save_manual_slot_encode(fd2_save_manual_slot *slot,
                                const fd2_save_manual_state *state) {
    if (!slot || !state || state->stage_id == 0xffu ||
        state->unit_count > FD2_SAVE_MAX_UNITS)
        return -1;
    memcpy(slot->unit_area, state->unit_area, sizeof(slot->unit_area));
    slot->meta[0] = state->stage_id;
    slot->meta[1] = state->unit_count;
    wr_u32_le(slot->meta + 2u, state->gold_or_flags);
    memcpy(slot->meta + 6u, state->options, sizeof(state->options));
    return 0;
}

int fd2_save_file_update_manual_slot(fd2_save_file *save, size_t slot_index,
                                     const fd2_save_manual_slot *slot) {
    size_t offset;
    if (!save || !save->data || !slot || save->size != FD2_SAVE_FILE_SIZE ||
        fd2_save_validate_plain(save->data, save->size) != 0 ||
        slot->meta[0] == 0xffu || slot->meta[1] > FD2_SAVE_MAX_UNITS ||
        slot_offset(slot_index, &offset) != 0)
        return -1;

    uint8_t *shadow = malloc(save->size);
    if (!shadow) return -1;
    memcpy(shadow, save->data, save->size);
    memcpy(shadow + offset, slot->unit_area, sizeof(slot->unit_area));
    size_t meta_size = slot_index + 1u == FD2_SAVE_SLOT_COUNT
                     ? FD2_SAVE_LAST_SLOT_META_SIZE
                     : FD2_SAVE_SLOT_META_SIZE;
    memcpy(shadow + offset + FD2_SAVE_UNIT_TABLE_SIZE, slot->meta,
           meta_size);
    wr_u32_le(shadow + save->size - FD2_SAVE_CHECKSUM_SIZE,
              fd2_save_checksum_sum(shadow, save->size));
    memcpy(save->data, shadow, save->size);
    free(shadow);
    return 0;
}

int fd2_save_file_get_slot(const fd2_save_file *save, size_t slot_index,
                           fd2_save_slot *slot) {
    size_t offset;
    if (!slot || !save || !save->data ||
        save->size != FD2_SAVE_FILE_SIZE ||
        slot_offset(slot_index, &offset) != 0 ||
        !fd2_save_file_slot_valid(save, slot_index))
        return -1;

    const uint8_t *raw_meta = save->data + offset + FD2_SAVE_UNIT_TABLE_SIZE;
    if (raw_meta[1] == 0xffu || raw_meta[1] > FD2_SAVE_MAX_UNITS)
        return -1;
    fd2_save_slot loaded = {0};
    const uint8_t *data = save->data + offset;
    const uint8_t *meta = data + FD2_SAVE_UNIT_TABLE_SIZE;
    loaded.stage_id = meta[0];
    loaded.unit_count = meta[1];
    loaded.gold_or_flags = rd_u32_le(meta + 2);
    memcpy(loaded.flags, meta + 6, sizeof(loaded.flags));
    memcpy(loaded.units, data,
           (size_t)loaded.unit_count * FD2_SAVE_UNIT_RECORD_SIZE);
    *slot = loaded;
    return 0;
}

int fd2_save_file_update_slot(fd2_save_file *save, size_t slot_index,
                              const fd2_save_unit *units,
                              size_t unit_count,
                              const uint8_t meta_28[FD2_SAVE_SLOT_META_SIZE]) {
    size_t offset;
    if (!save || !save->data || save->size != FD2_SAVE_FILE_SIZE ||
        fd2_save_validate_plain(save->data, save->size) != 0 ||
        !meta_28 || unit_count > FD2_SAVE_MAX_UNITS ||
        (unit_count != 0 && !units) || meta_28[0] == 0xffu ||
        meta_28[1] != unit_count || slot_offset(slot_index, &offset) != 0)
        return -1;

    uint8_t *shadow = malloc(save->size);
    if (!shadow) return -1;
    memcpy(shadow, save->data, save->size);
    uint8_t *data = shadow + offset;
    if (unit_count != 0)
        memcpy(data, units, unit_count * FD2_SAVE_UNIT_RECORD_SIZE);
    size_t meta_size = slot_index + 1u == FD2_SAVE_SLOT_COUNT
                     ? FD2_SAVE_LAST_SLOT_META_SIZE
                     : FD2_SAVE_SLOT_META_SIZE;
    memcpy(data + FD2_SAVE_UNIT_TABLE_SIZE, meta_28, meta_size);
    wr_u32_le(shadow + save->size - FD2_SAVE_CHECKSUM_SIZE,
              fd2_save_checksum_sum(shadow, save->size));
    memcpy(save->data, shadow, save->size);
    free(shadow);
    return 0;
}

int fd2_save_load_slot(fd2_save_slot *slot, const char *path,
                       size_t slot_index) {
    if (!slot) return -1;
    fd2_save_file save = {0};
    if (fd2_save_file_open(&save, path) != 0) return -1;
    int result = fd2_save_file_get_slot(&save, slot_index, slot);
    fd2_save_file_close(&save);
    return result;
}

uint8_t fd2_save_unit_class(const fd2_save_unit *unit) {
    return unit ? unit->bytes[2] : 0;
}

uint8_t fd2_save_unit_portrait(const fd2_save_unit *unit) {
    return unit ? unit->bytes[7] : 0xff;
}

uint8_t fd2_save_unit_name_id(const fd2_save_unit *unit) {
    return unit ? unit->bytes[8] : 0xff;
}
