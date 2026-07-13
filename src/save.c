/* 炎龙骑士团 2 SDL3 重写 - FD2.SAV 存档读取
 *
 * 逆向依据：FUN_0004b670 @0x7313c (save_xor_crypt) 的对称 XOR
 * 加/解密；FUN_00027313 @0x27313 选择 slot 后从
 * `0x312b + slot * 0xa28` 复制 0xa00 字节单位表到 `DAT_00003bf7`，
 * 随后读取 stage_id/unit_count 等 0x28 字节附加状态。
 */

#include "save.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FD2_SAVE_SLOT_BASE 0x312b
#define FD2_SAVE_SLOT_SIZE 0x0a28
#define FD2_SAVE_UNIT_TABLE_SIZE 0x0a00

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void fd2_save_xor_crypt(uint8_t *data, size_t size) {
    /* 复现 FUN_0004b670 @0x7313c：u16 state 从 0x00a5 开始，
     * 每字节先加 0x9014，再 rol16(3)，用低 8 位 XOR。 */
    uint16_t state = 0x00a5;
    for (size_t i = 0; i < size; i++) {
        state = (uint16_t)(state + 0x9014u);
        state = (uint16_t)((state << 3) | (state >> 13));
        data[i] ^= (uint8_t)state;
    }
}

int fd2_save_load_slot(fd2_save_slot *slot, const char *path, size_t slot_index) {
    if (!slot || !path) return -1;
    memset(slot, 0, sizeof(*slot));

    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return -1; }

    uint8_t *data = malloc((size_t)sz);
    if (!data) { fclose(f); return -1; }
    if (fread(data, 1, (size_t)sz, f) != (size_t)sz) {
        free(data);
        fclose(f);
        return -1;
    }
    fclose(f);

    fd2_save_xor_crypt(data, (size_t)sz);

    size_t off = FD2_SAVE_SLOT_BASE + slot_index * FD2_SAVE_SLOT_SIZE;
    if (off + FD2_SAVE_SLOT_SIZE > (size_t)sz) {
        free(data);
        return -1;
    }

    const uint8_t *p = data + off;
    const uint8_t *meta = p + FD2_SAVE_UNIT_TABLE_SIZE;
    if (meta[0] == 0xff || meta[1] == 0xff) {
        free(data);
        return -1;
    }

    slot->stage_id = meta[0];
    slot->unit_count = meta[1];
    slot->gold_or_flags = rd_u32_le(meta + 2);
    memcpy(slot->flags, meta + 6, sizeof(slot->flags));

    size_t count = slot->unit_count;
    if (count > FD2_SAVE_MAX_UNITS) count = FD2_SAVE_MAX_UNITS;
    if (count * FD2_SAVE_UNIT_RECORD_SIZE > FD2_SAVE_UNIT_TABLE_SIZE) {
        free(data);
        return -1;
    }

    for (size_t i = 0; i < count; i++) {
        memcpy(slot->units[i].bytes,
               p + i * FD2_SAVE_UNIT_RECORD_SIZE,
               FD2_SAVE_UNIT_RECORD_SIZE);
    }

    free(data);
    return 0;
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
