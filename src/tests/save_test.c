#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if !defined(_WIN32)
#include <unistd.h>
#endif

#include "save.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static void put_u32le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static void make_plain(uint8_t plain[FD2_SAVE_FILE_SIZE]) {
    for (size_t i = 0; i < FD2_SAVE_FILE_SIZE; i++)
        plain[i] = (uint8_t)(i * 37u + 11u);
    for (size_t slot = 0; slot < FD2_SAVE_SLOT_COUNT; slot++) {
        size_t offset = FD2_SAVE_SLOT_BASE + slot * FD2_SAVE_SLOT_SIZE;
        memset(plain + offset, 0x80 + (int)slot, FD2_SAVE_SLOT_SIZE);
        uint8_t *meta = plain + offset + FD2_SAVE_UNIT_TABLE_SIZE;
        meta[0] = slot < 2 ? (uint8_t)(slot + 1u) : 0xffu;
        meta[1] = slot < 2 ? 2u : 0xffu;
    }
    put_u32le(plain + FD2_SAVE_FILE_SIZE - FD2_SAVE_CHECKSUM_SIZE,
              fd2_save_checksum_sum(plain, FD2_SAVE_FILE_SIZE));
}

static int test_create_empty(void) {
    fd2_save_file save = {0};
    CHECK(fd2_save_file_create_empty(&save) == 0);
    CHECK(save.size == FD2_SAVE_FILE_SIZE);
    CHECK(fd2_save_validate_plain(save.data, save.size) == 0);
    for (size_t slot = 0; slot < FD2_SAVE_SLOT_COUNT; slot++)
        CHECK(!fd2_save_file_slot_valid(&save, slot));
    CHECK(fd2_save_file_create_empty(&save) != 0);
    fd2_save_file_close(&save);
    return 0;
}

static int test_xor_and_checksum(void) {
    uint8_t *plain = malloc(FD2_SAVE_FILE_SIZE);
    uint8_t *copy = malloc(FD2_SAVE_FILE_SIZE);
    CHECK(plain && copy);
    make_plain(plain);
    memcpy(copy, plain, FD2_SAVE_FILE_SIZE);
    fd2_save_xor_crypt(copy, FD2_SAVE_FILE_SIZE);
    CHECK(memcmp(copy, plain, FD2_SAVE_FILE_SIZE) != 0);
    fd2_save_xor_crypt(copy, FD2_SAVE_FILE_SIZE);
    CHECK(memcmp(copy, plain, FD2_SAVE_FILE_SIZE) == 0);
    CHECK(fd2_save_validate_plain(copy, FD2_SAVE_FILE_SIZE) == 0);
    copy[0x1234] ^= 1u;
    CHECK(fd2_save_validate_plain(copy, FD2_SAVE_FILE_SIZE) != 0);
    CHECK(fd2_save_validate_plain(copy, FD2_SAVE_FILE_SIZE - 1u) != 0);
    free(copy);
    free(plain);
    return 0;
}

static int test_battle_snapshot_preservation(void) {
    uint8_t *plain = malloc(FD2_SAVE_FILE_SIZE);
    uint8_t *encrypted = malloc(FD2_SAVE_FILE_SIZE);
    uint8_t *before = malloc(FD2_SAVE_FILE_SIZE);
    CHECK(plain && encrypted && before);
    make_plain(plain);
    plain[FD2_SAVE_BATTLE_META_BASE + FD2_SAVE_BATTLE_META_UNIT_COUNT] = 2;
    put_u32le(plain + FD2_SAVE_FILE_SIZE - FD2_SAVE_CHECKSUM_SIZE,
              fd2_save_checksum_sum(plain, FD2_SAVE_FILE_SIZE));
    memcpy(encrypted, plain, FD2_SAVE_FILE_SIZE);
    fd2_save_xor_crypt(encrypted, FD2_SAVE_FILE_SIZE);
    fd2_save_file save = {0};
    CHECK(fd2_save_file_open_encrypted(
              &save, encrypted, FD2_SAVE_FILE_SIZE) == 0);
    fd2_save_battle_snapshot snapshot;
    CHECK(fd2_save_file_get_battle_snapshot(&save, &snapshot) == 0);
    memcpy(before, save.data, save.size);
    snapshot.unit_area[0] ^= 0x11;
    snapshot.cell_state[31] ^= 0x22;
    snapshot.meta[FD2_SAVE_BATTLE_META_TURN] = 7;
    CHECK(fd2_save_file_update_battle_snapshot(&save, &snapshot) == 0);
    CHECK(fd2_save_validate_plain(save.data, save.size) == 0);
    size_t checksum_begin = save.size - FD2_SAVE_CHECKSUM_SIZE;
    for (size_t i = 0; i < save.size; i++) {
        int allowed = (i >= FD2_SAVE_BATTLE_UNIT_BASE &&
                       i < FD2_SAVE_BATTLE_UNIT_BASE +
                           FD2_SAVE_BATTLE_UNIT_AREA_SIZE) ||
                      (i >= FD2_SAVE_BATTLE_CELL_STATE_BASE &&
                       i < FD2_SAVE_BATTLE_CELL_STATE_BASE +
                           FD2_SAVE_BATTLE_CELL_STATE_SIZE) ||
                      (i >= FD2_SAVE_BATTLE_META_BASE &&
                       i < FD2_SAVE_BATTLE_META_BASE +
                           FD2_SAVE_BATTLE_META_SIZE) ||
                      i >= checksum_begin;
        if (!allowed) CHECK(save.data[i] == before[i]);
    }
    snapshot.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] =
        FD2_SAVE_BATTLE_MAX_UNITS + 1u;
    memcpy(before, save.data, save.size);
    CHECK(fd2_save_file_update_battle_snapshot(&save, &snapshot) != 0);
    CHECK(memcmp(before, save.data, save.size) == 0);
    fd2_save_file_close(&save);
    free(before);
    free(encrypted);
    free(plain);
    return 0;
}

static int test_slots_and_preservation(void) {
    uint8_t *plain = malloc(FD2_SAVE_FILE_SIZE);
    uint8_t *encrypted = malloc(FD2_SAVE_FILE_SIZE);
    uint8_t *before = malloc(FD2_SAVE_FILE_SIZE);
    CHECK(plain && encrypted && before);
    make_plain(plain);
    memcpy(encrypted, plain, FD2_SAVE_FILE_SIZE);
    fd2_save_xor_crypt(encrypted, FD2_SAVE_FILE_SIZE);

    fd2_save_file save = {0};
    CHECK(fd2_save_file_open_encrypted(
              &save, encrypted, FD2_SAVE_FILE_SIZE) == 0);
    CHECK(fd2_save_file_slot_valid(&save, 0));
    CHECK(fd2_save_file_slot_valid(&save, 1));
    CHECK(!fd2_save_file_slot_valid(&save, 2));
    CHECK(!fd2_save_file_slot_valid(&save, FD2_SAVE_SLOT_COUNT));

    fd2_save_slot slot;
    CHECK(fd2_save_file_get_slot(&save, 0, &slot) == 0);
    CHECK(slot.stage_id == 1 && slot.unit_count == 2);
    fd2_save_manual_slot raw_slot;
    fd2_save_manual_state manual_state;
    CHECK(fd2_save_file_get_manual_slot(&save, 0, &raw_slot) == 0);
    CHECK(fd2_save_manual_slot_decode(&raw_slot, &manual_state) == 0);
    CHECK(manual_state.stage_id == 1 && manual_state.unit_count == 2);
    manual_state.gold_or_flags = 0x12345678u;
    manual_state.options[0] ^= 1u;
    CHECK(fd2_save_manual_slot_encode(&raw_slot, &manual_state) == 0);
    CHECK(fd2_save_file_update_manual_slot(&save, 0, &raw_slot) == 0);
    CHECK(fd2_save_validate_plain(save.data, save.size) == 0);
    CHECK(fd2_save_file_get_slot(&save, 2, &slot) != 0);

    memcpy(before, save.data, save.size);
    fd2_save_unit units[2];
    memset(units, 0x33, sizeof(units));
    uint8_t meta[FD2_SAVE_SLOT_META_SIZE];
    memcpy(meta, before + FD2_SAVE_SLOT_BASE + FD2_SAVE_UNIT_TABLE_SIZE,
           sizeof(meta));
    meta[0] = 9;
    meta[1] = 2;
    meta[6] ^= 0x5a;
    CHECK(fd2_save_file_update_slot(&save, 0, units, 2, meta) == 0);
    CHECK(fd2_save_validate_plain(save.data, save.size) == 0);

    size_t slot_begin = FD2_SAVE_SLOT_BASE;
    size_t unit_end = slot_begin + sizeof(units);
    size_t meta_begin = slot_begin + FD2_SAVE_UNIT_TABLE_SIZE;
    size_t meta_end = meta_begin + FD2_SAVE_SLOT_META_SIZE;
    size_t checksum_begin = FD2_SAVE_FILE_SIZE - FD2_SAVE_CHECKSUM_SIZE;
    for (size_t i = 0; i < save.size; i++) {
        int allowed = (i >= slot_begin && i < unit_end) ||
                      (i >= meta_begin && i < meta_end) ||
                      i >= checksum_begin;
        if (!allowed) CHECK(save.data[i] == before[i]);
    }
    CHECK(memcmp(save.data + slot_begin, units, sizeof(units)) == 0);
    CHECK(memcmp(save.data + meta_begin, meta, sizeof(meta)) == 0);

    meta[1] = 3;
    memcpy(before, save.data, save.size);
    CHECK(fd2_save_file_update_slot(&save, 0, units, 2, meta) != 0);
    CHECK(memcmp(save.data, before, save.size) == 0);
    CHECK(fd2_save_file_update_slot(
              &save, FD2_SAVE_SLOT_COUNT, units, 2, meta) != 0);

    fd2_save_file_close(&save);
    free(before);
    free(encrypted);
    free(plain);
    return 0;
}

static int test_last_slot_overlap_and_zero_units(void) {
    uint8_t *plain = malloc(FD2_SAVE_FILE_SIZE);
    uint8_t *encrypted = malloc(FD2_SAVE_FILE_SIZE);
    uint8_t *before = malloc(FD2_SAVE_FILE_SIZE);
    CHECK(plain && encrypted && before);
    make_plain(plain);
    memcpy(encrypted, plain, FD2_SAVE_FILE_SIZE);
    fd2_save_xor_crypt(encrypted, FD2_SAVE_FILE_SIZE);
    fd2_save_file save = {0};
    CHECK(fd2_save_file_open_encrypted(
              &save, encrypted, FD2_SAVE_FILE_SIZE) == 0);
    memcpy(before, save.data, save.size);

    uint8_t meta[FD2_SAVE_SLOT_META_SIZE];
    memset(meta, 0xa7, sizeof(meta));
    meta[0] = 7;
    meta[1] = 0;
    CHECK(fd2_save_file_update_slot(
              &save, 3, NULL, 0, meta) == 0);
    size_t meta_begin = FD2_SAVE_SLOT_BASE + 3u * FD2_SAVE_SLOT_SIZE +
                        FD2_SAVE_UNIT_TABLE_SIZE;
    CHECK(memcmp(save.data + meta_begin, meta,
                 FD2_SAVE_LAST_SLOT_META_SIZE) == 0);
    CHECK(fd2_save_validate_plain(save.data, save.size) == 0);
    for (size_t i = 0; i < save.size; i++) {
        int allowed = (i >= meta_begin &&
                       i < meta_begin + FD2_SAVE_LAST_SLOT_META_SIZE) ||
                      i >= save.size - FD2_SAVE_CHECKSUM_SIZE;
        if (!allowed) CHECK(save.data[i] == before[i]);
    }
    fd2_save_file_close(&save);
    free(before);
    free(encrypted);
    free(plain);
    return 0;
}

static int test_write_roundtrip_and_overwrite(void) {
    static const char path[] = "/tmp/fd2sdl-save-test.sav";
    (void)remove(path);
    uint8_t *plain = malloc(FD2_SAVE_FILE_SIZE);
    uint8_t *encrypted = malloc(FD2_SAVE_FILE_SIZE);
    CHECK(plain && encrypted);
    make_plain(plain);
    memcpy(encrypted, plain, FD2_SAVE_FILE_SIZE);
    fd2_save_xor_crypt(encrypted, FD2_SAVE_FILE_SIZE);
    fd2_save_file save = {0};
    CHECK(fd2_save_file_open_encrypted(
              &save, encrypted, FD2_SAVE_FILE_SIZE) == 0);
    CHECK(fd2_save_file_write(&save, path) == 0);
    fd2_save_file reopened = {0};
    CHECK(fd2_save_file_open(&reopened, path) == 0);
    CHECK(memcmp(reopened.data, save.data, save.size) == 0);
    fd2_save_file_close(&reopened);

    save.data[0x20] ^= 0x5a;
    put_u32le(save.data + save.size - FD2_SAVE_CHECKSUM_SIZE,
              fd2_save_checksum_sum(save.data, save.size));
    CHECK(fd2_save_file_write(&save, path) == 0);
    CHECK(fd2_save_file_open(&reopened, path) == 0);
    CHECK(memcmp(reopened.data, save.data, save.size) == 0);
    fd2_save_file_close(&reopened);
    fd2_save_file_close(&save);
    free(encrypted);
    free(plain);
    CHECK(remove(path) == 0);
    return 0;
}

#if !defined(_WIN32)
static int test_preexisting_tmp_symlink_is_ignored(void) {
    static const char path[] = "/tmp/fd2sdl-save-symlink.sav";
    static const char fixed_tmp[] = "/tmp/fd2sdl-save-symlink.sav.tmp";
    static const char victim[] = "/tmp/fd2sdl-save-symlink-victim";
    (void)remove(path);
    (void)remove(fixed_tmp);
    (void)remove(victim);
    FILE *victim_file = fopen(victim, "wb");
    CHECK(victim_file != NULL);
    static const uint8_t sentinel[] = {0x21, 0x43, 0x65, 0x87};
    CHECK(fwrite(sentinel, 1, sizeof(sentinel), victim_file) ==
          sizeof(sentinel));
    CHECK(fclose(victim_file) == 0);
    CHECK(symlink(victim, fixed_tmp) == 0);

    uint8_t *plain = malloc(FD2_SAVE_FILE_SIZE);
    CHECK(plain != NULL);
    make_plain(plain);
    fd2_save_file save = {.data = plain, .size = FD2_SAVE_FILE_SIZE};
    CHECK(fd2_save_file_write(&save, path) == 0);
    uint8_t actual[sizeof(sentinel)] = {0};
    victim_file = fopen(victim, "rb");
    CHECK(victim_file != NULL);
    CHECK(fread(actual, 1, sizeof(actual), victim_file) == sizeof(actual));
    CHECK(fclose(victim_file) == 0);
    CHECK(memcmp(actual, sentinel, sizeof(sentinel)) == 0);
    free(plain);
    CHECK(remove(path) == 0);
    CHECK(remove(fixed_tmp) == 0);
    CHECK(remove(victim) == 0);
    return 0;
}
#endif

static int test_original_save_if_available(void) {
    FILE *probe = fopen("original_game/FD2.SAV", "rb");
    if (!probe) return 0;
    fclose(probe);
    fd2_save_file save = {0};
    CHECK(fd2_save_file_open(&save, "original_game/FD2.SAV") == 0);
    CHECK(save.size == FD2_SAVE_FILE_SIZE);
    CHECK(fd2_save_validate_plain(save.data, save.size) == 0);
    /* 本地开发者存档可能已被原版游戏改写，只验证通用格式不变量。 */
    CHECK(save.size == FD2_SAVE_FILE_SIZE);
    CHECK(fd2_save_validate_plain(save.data, save.size) == 0);
    fd2_save_file_close(&save);
    return 0;
}

int main(void) {
    if (test_create_empty() != 0 ||
        test_xor_and_checksum() != 0 ||
        test_battle_snapshot_preservation() != 0 ||
        test_slots_and_preservation() != 0 ||
        test_last_slot_overlap_and_zero_units() != 0 ||
        test_write_roundtrip_and_overwrite() != 0 ||
#if !defined(_WIN32)
        test_preexisting_tmp_symlink_is_ignored() != 0 ||
#endif
        test_original_save_if_available() != 0)
        return 1;
    puts("save_test: ok");
    return 0;
}
