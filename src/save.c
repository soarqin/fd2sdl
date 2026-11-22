/* 炎龙骑士团 2 SDL3 重写 - FD2.SAV 保留式读改写
 *
 * 逆向依据：save_checksum_sum @0x4df09 / save_xor_crypt @0x4df28
 *（原登记同名）对size-4明文字节求和／执行对称XOR。
 * field_secondary_menu_execute @0x19df7（原登记同名）与
 * field_manual_slot_save @0x2968d（原sub_2968D）分别
 * 更新活动快照／手工槽；均读取完整0x59cb容器、重算checksum再写回。
 * 已有容器的未修改区域与未知字节始终保留，手工槽不重建活动战场。
 * 新目标19fcd..19fe2只写四个stage哨兵；4df1d/4df1e随后读未赋值
 * payload参与checksum。create_empty全ff是该U之后的确定安全策略，
 * 不是原版全文件初始化写集，不声称复现任意旧heap。
 */

#include "save.h"
#include "host_files.h"
#include "settings.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

/* 接管独占缓冲区；校验通过后才交给调用者。 */
static int save_open_owned(fd2_save_file *save, uint8_t *data, size_t size) {
    fd2_save_xor_crypt(data, size);
    if (fd2_save_validate_plain(data, size) != 0) {
        free(data);
        return -1;
    }
    save->data = data;
    save->size = size;
    return 0;
}

int fd2_save_file_open_encrypted(fd2_save_file *save,
                                 const uint8_t *encrypted, size_t size) {
    if (!save || !encrypted || save->data || size != FD2_SAVE_FILE_SIZE)
        return -1;
    uint8_t *data = malloc(size);
    if (!data) return -1;
    memcpy(data, encrypted, size);
    return save_open_owned(save, data, size);
}

/* 写回末尾 4 字节校验和；所有切片更新在原缓冲区完成后调用。 */
static void save_reseal(uint8_t *data, size_t size) {
    wr_u32_le(data + size - FD2_SAVE_CHECKSUM_SIZE,
              fd2_save_checksum_sum(data, size));
}

int fd2_save_file_create_empty(fd2_save_file *save) {
    if (!save || save->data) return -1;
    uint8_t *data = malloc(FD2_SAVE_FILE_SIZE);
    if (!data) return -1;
    memset(data, 0xff, FD2_SAVE_FILE_SIZE); /* H：确定的新文件空槽/未知区。 */
    save_reseal(data, FD2_SAVE_FILE_SIZE);
    save->data = data;
    save->size = FD2_SAVE_FILE_SIZE;
    return 0;
}

/* 消费已打开的流，普通读取与保留式更新使用同一次打开的文件。 */
static int save_read_file(fd2_save_file *save, FILE *file) {
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
    return save_open_owned(save, encrypted, FD2_SAVE_FILE_SIZE);
}

int fd2_save_file_open(fd2_save_file *save, const char *path) {
    if (!save || !path || save->data) return -1;
    FILE *file = fd2_open_read(path);
    return file ? save_read_file(save, file) : -1;
}

int fd2_save_file_open_for_update(fd2_save_file *save, const char *path) {
    if (!save || !path || save->data) return -1;
    FILE *existing = fd2_open_read(path);
    if (existing) return save_read_file(save, existing);
    if (errno != ENOENT || fd2_save_file_create_empty(save) != 0) return -1;
    /* ff是空档哨兵，不是有效的actor/roster count；不建立可加载战场。 */
    save->data[FD2_SAVE_BATTLE_META_BASE + FD2_SAVE_BATTLE_META_UNIT_COUNT] = 0;
    save->data[FD2_SAVE_BATTLE_META_BASE + FD2_SAVE_BATTLE_META_ROSTER_COUNT] = 0;
    save_reseal(save->data, save->size);
    return 0;
}

void fd2_save_file_close(fd2_save_file *save) {
    if (!save) return;
    free(save->data);
    memset(save, 0, sizeof(*save));
}

int fd2_save_file_write(const fd2_save_file *save, const char *path) {
    if (!save || !save->data || !path || path[0] == '\0' ||
        save->size != FD2_SAVE_FILE_SIZE ||
        fd2_save_validate_plain(save->data, save->size) != 0)
        return -1;
    uint8_t *encrypted = malloc(save->size);
    if (!encrypted) return -1;
    memcpy(encrypted, save->data, save->size);
    fd2_save_xor_crypt(encrypted, save->size);

    int result = fd2_replace_file(path, encrypted, save->size, fd2_get_settings()->game_dir);
    free(encrypted);
    return result;
}

int fd2_save_file_get_battle_snapshot(
        const fd2_save_file *save, fd2_save_battle_snapshot *snapshot) {
    if (!save || !save->data || !snapshot ||
        fd2_save_validate_plain(save->data, save->size) != 0)
        return -1;
    fd2_save_battle_snapshot loaded;
    memcpy(loaded.metadata, save->data + FD2_SAVE_BATTLE_METADATA_BASE,
           sizeof(loaded.metadata));
    memcpy(loaded.roster, save->data + FD2_SAVE_BATTLE_ROSTER_BASE,
           sizeof(loaded.roster));
    memcpy(loaded.unit_area, save->data + FD2_SAVE_BATTLE_UNIT_BASE,
           sizeof(loaded.unit_area));
    memcpy(loaded.cell_state,
           save->data + FD2_SAVE_BATTLE_CELL_STATE_BASE,
           sizeof(loaded.cell_state));
    memcpy(loaded.meta, save->data + FD2_SAVE_BATTLE_META_BASE,
           sizeof(loaded.meta));
    if (loaded.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] >
            FD2_SAVE_BATTLE_MAX_UNITS ||
        loaded.meta[FD2_SAVE_BATTLE_META_ROSTER_COUNT] >
            FD2_SAVE_BATTLE_MAX_ROSTER)
        return -1;
    *snapshot = loaded;
    return 0;
}

int fd2_save_file_update_battle_snapshot(
        fd2_save_file *save, const fd2_save_battle_snapshot *snapshot) {
    if (!save || !save->data || !snapshot ||
        fd2_save_validate_plain(save->data, save->size) != 0 ||
        snapshot->meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] >
            FD2_SAVE_BATTLE_MAX_UNITS ||
        snapshot->meta[FD2_SAVE_BATTLE_META_ROSTER_COUNT] >
            FD2_SAVE_BATTLE_MAX_ROSTER)
        return -1;
    /* 校验全部通过后不再有失败路径，直接原地写入各切片再重封校验和。 */
    uint8_t *data = save->data;
    memcpy(data + FD2_SAVE_BATTLE_METADATA_BASE, snapshot->metadata,
           sizeof(snapshot->metadata));
    memcpy(data + FD2_SAVE_BATTLE_ROSTER_BASE, snapshot->roster,
           sizeof(snapshot->roster));
    memcpy(data + FD2_SAVE_BATTLE_UNIT_BASE, snapshot->unit_area,
           sizeof(snapshot->unit_area));
    memcpy(data + FD2_SAVE_BATTLE_CELL_STATE_BASE, snapshot->cell_state,
           sizeof(snapshot->cell_state));
    memcpy(data + FD2_SAVE_BATTLE_META_BASE, snapshot->meta,
           sizeof(snapshot->meta));
    save_reseal(data, save->size);
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

/* 末槽 meta 与末尾校验和重叠，只写入较短的前缀。 */
static size_t slot_meta_size(size_t slot_index) {
    return slot_index + 1u == FD2_SAVE_SLOT_COUNT
         ? FD2_SAVE_LAST_SLOT_META_SIZE
         : FD2_SAVE_SLOT_META_SIZE;
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
    fd2_save_manual_slot loaded;
    if (!slot || !fd2_save_file_slot_valid(save, slot_index) ||
        fd2_save_file_get_manual_slot_raw(save, slot_index, &loaded) != 0 ||
        loaded.meta[1] > FD2_SAVE_MAX_UNITS)
        return -1;
    *slot = loaded;
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

int fd2_save_manual_state_load(const char *path, size_t slot_index,
                              fd2_save_manual_state *out) {
    if (!path || !out || slot_index >= FD2_SAVE_SLOT_COUNT) return -1;
    fd2_save_file save = {0};
    fd2_save_manual_slot slot;
    int result = fd2_save_file_open(&save, path) == 0 &&
                 fd2_save_file_get_manual_slot(&save, slot_index, &slot) == 0 &&
                 fd2_save_manual_slot_decode(&slot, out) == 0 ? 0 : -1;
    fd2_save_file_close(&save);
    return result;
}

int fd2_save_manual_state_save(const char *path, size_t slot_index,
                              const fd2_save_manual_state *state) {
    if (!path || !state || slot_index >= FD2_SAVE_SLOT_COUNT ||
        state->stage_id == 0xffu || state->unit_count > FD2_SAVE_MAX_UNITS)
        return -1;
    fd2_save_file save = {0};
    fd2_save_manual_slot slot;
    int result = fd2_save_file_open_for_update(&save, path) == 0 &&
                 fd2_save_file_get_manual_slot_raw(&save, slot_index, &slot) == 0 &&
                 fd2_save_manual_slot_encode(&slot, state) == 0 &&
                 fd2_save_file_update_manual_slot(&save, slot_index, &slot) == 0 &&
                 fd2_save_file_write(&save, path) == 0 ? 0 : -1;
    fd2_save_file_close(&save);
    return result;
}

int fd2_save_file_update_manual_slot(fd2_save_file *save, size_t slot_index,
                                     const fd2_save_manual_slot *slot) {
    size_t offset;
    if (!save || !save->data || !slot || save->size != FD2_SAVE_FILE_SIZE ||
        fd2_save_validate_plain(save->data, save->size) != 0 ||
        slot->meta[0] == 0xffu || slot->meta[1] > FD2_SAVE_MAX_UNITS ||
        slot_offset(slot_index, &offset) != 0)
        return -1;

    memcpy(save->data + offset, slot->unit_area, sizeof(slot->unit_area));
    memcpy(save->data + offset + FD2_SAVE_UNIT_TABLE_SIZE, slot->meta,
           slot_meta_size(slot_index));
    save_reseal(save->data, save->size);
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

    uint8_t *data = save->data + offset;
    if (unit_count != 0)
        memcpy(data, units, unit_count * FD2_SAVE_UNIT_RECORD_SIZE);
    memcpy(data + FD2_SAVE_UNIT_TABLE_SIZE, meta_28, slot_meta_size(slot_index));
    save_reseal(save->data, save->size);
    return 0;
}
