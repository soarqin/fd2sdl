/* 活动战场 FD2.SAV snapshot codec 测试。
 * S0结果依据：sub_205B4 @0x205b4 与 field_stage1_battle_result_check
 * @0x206c5（原登记sub_206C5）的hidden/side扫描及覆盖顺序；不按HP猜结果。
 * F3恢复写集：sub_10010@101c3整2211B metadata、10391写raw+2，
 * 11019按首次ID顺序返回class；2986f手工恢复没有raw+2写点。
 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if !defined(_WIN32)
#include <sys/stat.h>
#endif

#include "field_game.h"
#include "field_rng.h"
#include "static_data.h"
#include "test_temp.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static void init_game(fd2_field_game *game) {
    memset(game, 0, sizeof(*game));
    game->ready = 1;
    game->stage = 0;
    game->map.width = 24;
    game->map.height = 24;
    /* 无绘制的synthetic fixture仅声明可容纳140个12帧ID。 */
    game->sprites.frame_count = 140u * 12u;
    game->turn_number = 7;
    game->camera_cell_x = 2;
    game->camera_cell_y = 9;
    game->cursor_cell_x = 8;
    game->cursor_cell_y = 13;
    game->active_side = 2;
    game->option_values[0] = 0;
    game->option_values[1] = 1;
    game->option_values[2] = 1;
    game->option_values[3] = 0;
    game->resource_value = 0x12345678u;
    game->metadata.unknown_01 = 4;
    game->metadata.unknown_02 = 30;
    game->interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
    game->selected_unit = 1;
    /* 导出/写档fixture处于可保存边界；详情忙态由专门负例显式开启。 */
    game->detail_unit = -1;
    game->detail_visible = 0;
    game->command_selected = 3;
    game->attack_target = 2;
    game->event_log_count = 2;
    game->event_total_count = 4;
    game->units.count = 3;
    for (size_t i = 0; i < game->units.count; i++) {
        uint8_t *record = (uint8_t *)&game->units.items[i];
        for (size_t j = 0; j < FD2_FIELD_UNIT_RECORD_SIZE; j++)
            record[j] = (uint8_t)(i * 67u + j * 3u + 1u);
        game->units.items[i].x = (uint8_t)(4u + i);
        game->units.items[i].y = (uint8_t)(10u + i);
        game->units.items[i].unit_id = (uint8_t)(20u + i);
        game->units.items[i].sprite_cache_class = (uint8_t)i;
        game->units.items[i].hp = (uint16_t)(100u + i);
        game->units.walk_frames[i] = 5;
        game->units.source_template_indices[i] = (uint16_t)i;
    }
    memset(game->roster, 0x6d, sizeof(game->roster));
    game->roster_count = 2;
    memcpy(game->roster, game->units.items,
           game->roster_count * FD2_FIELD_UNIT_RECORD_SIZE);
    for (size_t i = 0; i < sizeof(game->cell_action_completed); i++)
        game->cell_action_completed[i] = (uint8_t)(i ^ 0x5a);
    fd2_field_system_menu_init(&game->system_menu);
}

static int test_export_preserves_unknown(void) {
    fd2_field_game game;
    init_game(&game);
    fd2_save_battle_snapshot snapshot;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    fd2_save_battle_snapshot before = snapshot;
    CHECK(fd2_field_game_export_battle_snapshot(&game, &snapshot) == 0);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_TURN] == 7);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] == 3);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_STAGE] == 0);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_CAMERA_X] == 2);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_CAMERA_Y] == 9);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_FOCUS_X] == 8);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_FOCUS_Y] == 13);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_FOCUS_REL_X] == 6);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_FOCUS_REL_Y] == 4);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_ROSTER_COUNT] == 2);
    CHECK(memcmp(snapshot.roster, game.roster,
                 sizeof(snapshot.roster)) == 0);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_RESOURCE_VALUE] == 0x78 &&
          snapshot.meta[FD2_SAVE_BATTLE_META_RESOURCE_VALUE + 1u] == 0x56 &&
          snapshot.meta[FD2_SAVE_BATTLE_META_RESOURCE_VALUE + 2u] == 0x34 &&
          snapshot.meta[FD2_SAVE_BATTLE_META_RESOURCE_VALUE + 3u] == 0x12);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_OPTION_EFFECT] == 1 &&
          snapshot.meta[FD2_SAVE_BATTLE_META_OPTION_CAMERA] == 0 &&
          snapshot.meta[FD2_SAVE_BATTLE_META_OPTION_MUSIC] == 0 &&
          snapshot.meta[FD2_SAVE_BATTLE_META_OPTION_SFX] == 1);
    CHECK(memcmp(snapshot.unit_area, game.units.items,
                 3u * FD2_FIELD_UNIT_RECORD_SIZE) == 0);
    CHECK(memcmp(snapshot.unit_area + 3u * FD2_FIELD_UNIT_RECORD_SIZE,
                 before.unit_area + 3u * FD2_FIELD_UNIT_RECORD_SIZE,
                 FD2_SAVE_BATTLE_UNIT_AREA_SIZE -
                     3u * FD2_FIELD_UNIT_RECORD_SIZE) == 0);
    CHECK(memcmp(snapshot.cell_state, game.cell_action_completed,
                 sizeof(game.cell_action_completed)) == 0);
    CHECK(memcmp(snapshot.cell_state + sizeof(game.cell_action_completed),
                 before.cell_state + sizeof(game.cell_action_completed),
                 FD2_SAVE_BATTLE_CELL_STATE_SIZE -
                     sizeof(game.cell_action_completed)) == 0);
    CHECK(memcmp(snapshot.meta + 18,
                 before.meta + 18,
                 FD2_SAVE_BATTLE_META_SIZE - 18) == 0);
    CHECK(snapshot.metadata[0] == game.metadata.shape_index &&
          snapshot.metadata[1] == 4 && snapshot.metadata[2] == 30);
    CHECK(memcmp(snapshot.metadata + 0x83u, before.metadata + 0x83u,
                 FD2_SAVE_BATTLE_METADATA_SIZE - 0x83u) == 0);
    return 0;
}

static int test_runtime_unit_capacity(void) {
    fd2_field_game game;
    init_game(&game);
    game.units.count = FD2_FIELD_MAX_UNITS;
    memset(game.units.items, 0x5a, sizeof(game.units.items));
    fd2_save_battle_snapshot snapshot;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    CHECK(fd2_field_game_export_battle_snapshot(&game, &snapshot) == 0);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] == FD2_FIELD_MAX_UNITS);
    CHECK(memcmp(snapshot.unit_area, game.units.items,
                 sizeof(game.units.items)) == 0);
    for (size_t i = sizeof(game.units.items); i < sizeof(snapshot.unit_area); i++)
        CHECK(snapshot.unit_area[i] == 0xa5);
    fd2_save_battle_snapshot before = snapshot;
    const size_t invalid_counts[] = {
        FD2_FIELD_MAX_UNITS + 1u, FD2_SAVE_BATTLE_MAX_UNITS + 1u, SIZE_MAX};
    for (size_t i = 0; i < sizeof(invalid_counts) / sizeof(invalid_counts[0]); i++) {
        game.units.count = invalid_counts[i];
        CHECK(fd2_field_game_export_battle_snapshot(&game, &snapshot) != 0);
        CHECK(memcmp(&snapshot, &before, sizeof(snapshot)) == 0);
    }
    return 0;
}

static int test_roster_capacity_is_independent(void) {
    fd2_field_game source, target;
    init_game(&source);
    init_game(&target);
    source.roster_count = FD2_SAVE_BATTLE_MAX_ROSTER;
    fd2_save_battle_snapshot snapshot;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    CHECK(fd2_field_game_export_battle_snapshot(&source, &snapshot) == 0);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] == 3);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_ROSTER_COUNT] == 32);
    CHECK(fd2_field_game_import_battle_snapshot(&target, &snapshot) == 0);
    CHECK(target.units.count == 3 && target.roster_count == 32);
    CHECK(memcmp(target.roster, source.roster, sizeof(source.roster)) == 0);
    fd2_save_battle_snapshot before_snapshot = snapshot;
    source.roster_count = FD2_SAVE_BATTLE_MAX_ROSTER + 1u;
    CHECK(fd2_field_game_export_battle_snapshot(&source, &snapshot) != 0);
    CHECK(memcmp(&snapshot, &before_snapshot, sizeof(snapshot)) == 0);
    fd2_field_game before = target;
    snapshot.meta[FD2_SAVE_BATTLE_META_ROSTER_COUNT] = 33;
    CHECK(fd2_field_game_import_battle_snapshot(&target, &snapshot) != 0);
    CHECK(memcmp(&target, &before, sizeof(target)) == 0);
    fd2_field_metadata_close(&target.metadata);
    return 0;
}

/* 独立字节oracle：原1036a读取record+7，11019首次ID排名，10391仅写+2。
 * 不调用SDL cache函数，也不从被测旁路输出反填expected。 */
static void original_active_load_records(uint8_t *records, size_t count) {
    uint8_t first_class[256];
    memset(first_class, 0xff, sizeof(first_class));
    uint8_t next_class = 0;
    for (size_t i = 0; i < count; i++) {
        uint8_t *record = records + i * 0x50u;
        if (first_class[record[7]] == 0xffu)
            first_class[record[7]] = next_class++;
        record[2] = first_class[record[7]];
    }
}

static int original_active_units_match(const fd2_field_units *units,
                                       const fd2_save_battle_snapshot *snapshot) {
    uint8_t expected[FD2_SAVE_BATTLE_UNIT_AREA_SIZE];
    memcpy(expected, snapshot->unit_area, sizeof(expected));
    original_active_load_records(expected, units->count);
    return memcmp(units->items, expected, units->count * 0x50u) == 0;
}

static int test_import_rebuilds_cache_and_writes_only_class(void) {
    const size_t counts[] = {1, 3, 31, 32, 96};
    const uint8_t ids[] = {68, 20, 68, 121, 0, 20, 135, 121};
    for (size_t row = 0; row < sizeof(counts) / sizeof(counts[0]); row++) {
        fd2_field_game source, target;
        init_game(&source); init_game(&target);
        source.units.count = counts[row];
        for (size_t i = 0; i < source.units.count; i++) {
            uint8_t *record = (uint8_t *)&source.units.items[i];
            for (size_t byte = 0; byte < 0x50u; byte++)
                record[byte] = (uint8_t)(i * 37u + byte * 13u + row);
            source.units.items[i].unit_id = ids[i % sizeof(ids)];
            source.units.items[i].x = (uint8_t)(i % 24u);
            source.units.items[i].y = (uint8_t)((i * 3u) % 24u);
            source.units.items[i].flags = (uint8_t)((i & 1u) ? 1u : 0u);
            source.units.items[i].direction = (uint8_t)(i % 4u);
            source.units.items[i].frame_phase = (uint8_t)(i % 3u);
            source.units.items[i].hp = (uint16_t)((i % 3u) ? 27u : 0u);
            source.units.items[i].sprite_cache_class =
                (uint8_t)((row & 1u) ? 0u : 255u);
        }
        fd2_save_battle_snapshot snapshot;
        memset(&snapshot, 0xa5, sizeof(snapshot));
        CHECK(fd2_field_game_export_battle_snapshot(&source, &snapshot) == 0);
        fd2_save_battle_snapshot expected = snapshot;
        original_active_load_records(expected.unit_area, source.units.count);
        fd2_save_battle_snapshot input_before = snapshot;
        if (source.units.count <= 32u) {
            /* 2986f没有10391；共享cache重建叶不能顺手写持久80B。 */
            fd2_field_units manual = source.units;
            CHECK(fd2_field_units_sprite_cache_restore(&manual) == 0);
            CHECK(!memcmp(manual.items, source.units.items, sizeof(manual.items)));
        }
        CHECK(fd2_field_game_import_battle_snapshot(&target, &snapshot) == 0);
        CHECK(target.units.sprite_cache_count == (counts[row] < 6u ?
            (counts[row] == 1u ? 1u : 2u) : 5u));
        CHECK(target.units.sprite_cache_exact == 1);
        CHECK(!memcmp(target.units.items, expected.unit_area,
                      source.units.count * 0x50u));
        CHECK(!memcmp(target.roster, snapshot.roster, sizeof(target.roster)));
        CHECK(!memcmp(&snapshot, &input_before, sizeof(snapshot)));
        for (size_t i = 0; i < target.units.count; i++) {
            CHECK(target.units.sprite_cache_classes[i] == expected.unit_area[i * 0x50u + 2u]);
            CHECK(target.units.sprite_cache_ids[target.units.sprite_cache_classes[i]] ==
                  target.units.items[i].unit_id);
        }
        fd2_save_battle_snapshot exported = snapshot;
        CHECK(fd2_field_game_export_battle_snapshot(&target, &exported) == 0);
        CHECK(!memcmp(&exported, &expected, sizeof(expected)));
        fd2_field_game before = target;
        snapshot.unit_area[5] = 0;
        snapshot.unit_area[0] = 255; /* 无效可见坐标，不能部分写class/metadata。 */
        snapshot.metadata[0x8a2u] ^= 1u;
        CHECK(fd2_field_game_import_battle_snapshot(&target, &snapshot) != 0);
        CHECK(!memcmp(&target, &before, sizeof(target)));
        fd2_field_metadata_close(&target.metadata);
    }
    return 0;
}

static void put_u32le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static int write_fixture(const char *path) {
    uint8_t *plain = malloc(FD2_SAVE_FILE_SIZE);
    if (!plain) return -1;
    for (size_t i = 0; i < FD2_SAVE_FILE_SIZE; i++)
        plain[i] = (uint8_t)(i * 19u + 7u);
    plain[FD2_SAVE_BATTLE_META_BASE + FD2_SAVE_BATTLE_META_UNIT_COUNT] = 0;
    plain[FD2_SAVE_BATTLE_META_BASE + FD2_SAVE_BATTLE_META_ROSTER_COUNT] = 0;
    put_u32le(plain + FD2_SAVE_FILE_SIZE - FD2_SAVE_CHECKSUM_SIZE,
              fd2_save_checksum_sum(plain, FD2_SAVE_FILE_SIZE));
    fd2_save_file save = {.data = plain, .size = FD2_SAVE_FILE_SIZE};
    int result = fd2_save_file_write(&save, path);
    free(plain);
    return result;
}

static int test_first_active_save(void) {
    char path[FD2_TEST_PATH_MAX];
    CHECK(fd2_test_temp_path(path) == 0);
    CHECK(remove(path) == 0);
    fd2_field_game source, target;
    init_game(&source);
    init_game(&target);
    CHECK(fd2_field_game_save_active(&source, path) == 0);
    CHECK(source.save_resource_available == 1);
    fd2_save_file save = {0};
    fd2_save_battle_snapshot snapshot;
    CHECK(fd2_save_file_open(&save, path) == 0);
    CHECK(fd2_save_file_get_battle_snapshot(&save, &snapshot) == 0);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] == source.units.count);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_ROSTER_COUNT] == source.roster_count);
    for (size_t slot = 0; slot < FD2_SAVE_SLOT_COUNT; slot++)
        CHECK(!fd2_save_file_slot_valid(&save, slot));
    CHECK(memcmp(snapshot.roster, source.roster, sizeof(source.roster)) == 0);
    for (size_t i = 0x83u; i < FD2_SAVE_BATTLE_METADATA_SIZE; i++)
        CHECK(save.data[i] == 0xff);
    fd2_save_file_close(&save);
    CHECK(fd2_field_game_load_active(&target, path) == 0);
    CHECK(target.units.count == source.units.count);
    CHECK(memcmp(target.units.items, source.units.items,
                 source.units.count * FD2_FIELD_UNIT_RECORD_SIZE) == 0);
    CHECK(target.turn_number == source.turn_number);
    fd2_field_metadata_close(&target.metadata);
    CHECK(remove(path) == 0);
    return 0;
}

static int test_first_manual_then_active_save(void) {
    char path[FD2_TEST_PATH_MAX];
    CHECK(fd2_test_temp_path(path) == 0);
    CHECK(remove(path) == 0);
    fd2_field_game game;
    init_game(&game);
    CHECK(fd2_field_game_save_manual_slot(&game, path, 2) == 0);
    fd2_save_file save = {0};
    fd2_save_manual_slot manual_before, manual_after;
    CHECK(fd2_save_file_open(&save, path) == 0);
    CHECK(fd2_save_file_get_manual_slot(&save, 2, &manual_before) == 0);
    fd2_save_file_close(&save);
    CHECK(fd2_field_game_save_resource_available(&game, path) == 0);
    CHECK(fd2_field_game_save_active(&game, path) == 0);
    CHECK(fd2_field_game_save_resource_available(&game, path) == 1);
    CHECK(fd2_save_file_open(&save, path) == 0);
    CHECK(fd2_save_file_get_manual_slot(&save, 2, &manual_after) == 0);
    CHECK(memcmp(&manual_before, &manual_after, sizeof(manual_before)) == 0);
    fd2_save_file_close(&save);
    CHECK(remove(path) == 0);
    return 0;
}

static int test_loaded_roster_survives_save_to_other_files(void) {
    char path_a[FD2_TEST_PATH_MAX], path_b[FD2_TEST_PATH_MAX];
    char path_new[FD2_TEST_PATH_MAX];
    CHECK(fd2_test_temp_path(path_a) == 0 && remove(path_a) == 0);
    CHECK(fd2_test_temp_path(path_b) == 0 && remove(path_b) == 0);
    CHECK(fd2_test_temp_path(path_new) == 0 && remove(path_new) == 0);
    fd2_field_game source, loaded;
    init_game(&source);
    init_game(&loaded);
    CHECK(fd2_field_game_save_active(&source, path_a) == 0);
    CHECK(fd2_field_game_load_active(&loaded, path_a) == 0);
    CHECK(memcmp(loaded.roster, source.roster, sizeof(source.roster)) == 0);

    CHECK(write_fixture(path_b) == 0);
    fd2_save_file save = {0};
    fd2_save_battle_snapshot before, after;
    CHECK(fd2_save_file_open(&save, path_b) == 0);
    CHECK(fd2_save_file_get_battle_snapshot(&save, &before) == 0);
    CHECK(memcmp(before.roster, loaded.roster, sizeof(before.roster)) != 0);
    fd2_save_file_close(&save);
    CHECK(fd2_field_game_save_active(&loaded, path_b) == 0);
    CHECK(fd2_save_file_open(&save, path_b) == 0);
    CHECK(fd2_save_file_get_battle_snapshot(&save, &after) == 0);
    CHECK(memcmp(after.roster, source.roster, sizeof(after.roster)) == 0);
    CHECK(after.meta[FD2_SAVE_BATTLE_META_ROSTER_COUNT] == source.roster_count);
    /* 19df7写完整metadata来源A，不再继承目标B的opaque尾。 */
    for (size_t i = 0x83u; i < sizeof(after.metadata); i++)
        CHECK(after.metadata[i] == 0xff);
    fd2_save_file_close(&save);

    CHECK(fd2_field_game_save_active(&loaded, path_new) == 0);
    CHECK(fd2_save_file_open(&save, path_new) == 0);
    CHECK(fd2_save_file_get_battle_snapshot(&save, &after) == 0);
    CHECK(memcmp(after.roster, source.roster, sizeof(after.roster)) == 0);
    CHECK(after.meta[FD2_SAVE_BATTLE_META_ROSTER_COUNT] == source.roster_count);
    for (size_t i = 0x83u; i < sizeof(after.metadata); i++)
        CHECK(after.metadata[i] == 0xff);
    fd2_save_file_close(&save);
    fd2_field_metadata_close(&loaded.metadata);
    CHECK(remove(path_a) == 0 && remove(path_b) == 0 && remove(path_new) == 0);
    return 0;
}

static int test_import_is_transactional(void) {
    fd2_field_game game;
    init_game(&game);
    fd2_save_battle_snapshot snapshot;
    memset(&snapshot, 0xcc, sizeof(snapshot));
    CHECK(fd2_field_game_export_battle_snapshot(&game, &snapshot) == 0);

    fd2_field_game target;
    init_game(&target);
    target.detail_unit=1;target.detail_visible=1;
    target.detail_portrait=(fd2_image){.width=80,.height=80,.pixels=calloc(80u*80u,1)};
    CHECK(target.detail_portrait.pixels);
    target.units.items[0].hp = 1;
    target.turn_number = 99;
    target.camera_cell_x = 0;
    target.cursor_cell_x = 0;
    CHECK(fd2_field_game_import_battle_snapshot(&target, &snapshot) == 0);
    CHECK(target.units.count == game.units.count);
    CHECK(target.roster_count == game.roster_count);
    CHECK(memcmp(target.roster, snapshot.roster, sizeof(target.roster)) == 0);
    CHECK(memcmp(target.units.items, game.units.items,
                 game.units.count * FD2_FIELD_UNIT_RECORD_SIZE) == 0);
    CHECK(target.units.items[0].hp == game.units.items[0].hp);
    CHECK(target.turn_number == 7);
    CHECK(target.camera_cell_x == 2 && target.camera_cell_y == 9);
    CHECK(target.cursor_cell_x == 8 && target.cursor_cell_y == 13);
    CHECK(memcmp(target.cell_action_completed, game.cell_action_completed,
                 sizeof(game.cell_action_completed)) == 0);
    CHECK(target.interaction == FD2_FIELD_INTERACTION_BROWSE);
    CHECK(target.active_side == 2 && target.phase_unit_cursor == 0 &&
          target.phase_ai_pass == 0);
    CHECK(memcmp(target.option_values, game.option_values,
                 sizeof(target.option_values)) == 0);
    CHECK(target.resource_value == game.resource_value);
    CHECK(target.stage_runtime_entry_offset == 0u); /* stage0没有准备页row。 */
    CHECK(memcmp(target.system_menu.option_values, game.option_values,
                 sizeof(target.option_values)) == 0);
    CHECK(target.selected_unit == -1 && target.detail_unit == -1);
    CHECK(!target.detail_visible && !target.detail_portrait.pixels);
    CHECK(target.command_selected == -1 && target.attack_target == -1);
    CHECK(target.command_animation_opening == 0 &&
          target.command_animation_phase == 0);
    CHECK(target.system_menu.animation_opening == 0 &&
          target.system_menu.animation_phase == 0);
    CHECK(target.event_log_count == 0 && target.event_total_count == 0);
    for (size_t i = 0; i < target.units.count; i++) {
        CHECK(target.units.walk_frames[i] == 1);
        CHECK(target.units.source_template_indices[i] ==
              FD2_FIELD_UNIT_NO_TEMPLATE);
    }

    fd2_field_game before = target;
    snapshot.meta[FD2_SAVE_BATTLE_META_STAGE] = 1;
    CHECK(fd2_field_game_import_battle_snapshot(&target, &snapshot) != 0);
    CHECK(memcmp(&target, &before, sizeof(target)) == 0);
    snapshot.meta[FD2_SAVE_BATTLE_META_STAGE] = 0;
    snapshot.unit_area[0] = 99;
    CHECK(fd2_field_game_import_battle_snapshot(&target, &snapshot) != 0);
    CHECK(memcmp(&target, &before, sizeof(target)) == 0);
    return 0;
}

static void prepare_confirmed_action(fd2_field_game *game,
                                     fd2_field_system_action action) {
    fd2_field_system_menu_init(&game->system_menu);
    fd2_field_system_menu_open_secondary(&game->system_menu);
    game->system_menu.confirmation_action = action;
    game->system_menu.confirmation_accepted_fragment =
        action == FD2_FIELD_SYSTEM_ACTION_SAVE
            ? FD2_FIELD_SYSTEM_TEXT_SAVE_SUCCESS
            : FD2_FIELD_SYSTEM_TEXT_LOAD_ACCEPTED;
    game->system_menu.confirmation_failure_fragment =
        action == FD2_FIELD_SYSTEM_ACTION_SAVE
            ? FD2_FIELD_SYSTEM_TEXT_SAVE_FAILURE : 0;
}

static int test_manual_save_and_load_unavailable(void) {
    char path[FD2_TEST_PATH_MAX];
    CHECK(fd2_test_temp_path(path) == 0);
    CHECK(remove(path) == 0);
    fd2_field_game source;
    init_game(&source);
    CHECK(fd2_field_game_save_manual_slot(&source, path, 2) == 0);

    fd2_save_file save = {0};
    fd2_save_manual_slot raw;
    fd2_save_manual_state state;
    CHECK(fd2_save_file_open(&save, path) == 0);
    CHECK(fd2_save_file_get_manual_slot(&save, 2, &raw) == 0);
    CHECK(fd2_save_manual_slot_decode(&raw, &state) == 0);
    CHECK(state.stage_id == 0 && state.unit_count == source.roster_count);
    CHECK(state.gold_or_flags == source.resource_value);
    CHECK(state.options[0] == source.option_values[3] &&
          state.options[1] == source.option_values[2] &&
          state.options[2] == source.option_values[0] &&
          state.options[3] == source.option_values[1]);
    CHECK(memcmp(state.unit_area, source.roster,
                 FD2_SAVE_UNIT_TABLE_SIZE) == 0);
    fd2_save_file_close(&save);

    fd2_field_game target;
    init_game(&target);
    memset(target.units.items, 0, sizeof(target.units.items));
    target.resource_value = 0;
    memset(target.option_values, 0, sizeof(target.option_values));
    fd2_field_game before = target;
    CHECK(fd2_field_game_load_manual_slot(&target, path, 2) != 0);
    CHECK(memcmp(&target, &before, sizeof(target)) == 0);
    CHECK(fd2_field_game_load_manual_slot(&target, path, 0) != 0);
    CHECK(memcmp(&target, &before, sizeof(target)) == 0);
    CHECK(remove(path) == 0);
    return 0;
}

static int test_file_roundtrip(void) {
    char path[FD2_TEST_PATH_MAX];
    CHECK(fd2_test_temp_path(path) == 0);
    CHECK(remove(path) == 0);
    CHECK(write_fixture(path) == 0);
    fd2_field_game source;
    init_game(&source);
    CHECK(fd2_field_game_save_resource_available(&source, path) == 0);
    prepare_confirmed_action(&source, FD2_FIELD_SYSTEM_ACTION_SAVE);
    size_t result_fragment = 0;
    CHECK(fd2_field_game_execute_storage_action(
              &source, FD2_FIELD_SYSTEM_ACTION_SAVE, path,
              &result_fragment) == 1);
    CHECK(result_fragment == FD2_FIELD_SYSTEM_TEXT_SAVE_SUCCESS);
    CHECK(fd2_field_game_save_resource_available(&source, path) == 1);

    fd2_field_game target;
    init_game(&target);
    memset(target.units.items, 0, sizeof(target.units.items));
    target.turn_number = 88;
    prepare_confirmed_action(&target, FD2_FIELD_SYSTEM_ACTION_LOAD);
    CHECK(fd2_field_game_execute_storage_action(
              &target, FD2_FIELD_SYSTEM_ACTION_LOAD, path,
              &result_fragment) == 1);
    CHECK(result_fragment == FD2_FIELD_SYSTEM_TEXT_LOAD_ACCEPTED);
    CHECK(target.system_result_fragment == 0 &&
          target.system_result_until_ms == 0);
    CHECK(target.turn_number == source.turn_number);
    CHECK(target.units.count == source.units.count);
    CHECK(memcmp(target.units.items, source.units.items,
                 source.units.count * FD2_FIELD_UNIT_RECORD_SIZE) == 0);

    FILE *file = fopen(path, "r+b");
    CHECK(file != NULL);
    CHECK(fseek(file, 20, SEEK_SET) == 0);
    int byte = fgetc(file);
    CHECK(byte != EOF && fseek(file, 20, SEEK_SET) == 0);
    CHECK(fputc(byte ^ 1, file) != EOF && fclose(file) == 0);
    fd2_field_game before = target;
    CHECK(fd2_field_game_save_resource_available(&target, path) == 0);
    prepare_confirmed_action(&target, FD2_FIELD_SYSTEM_ACTION_LOAD);
    before = target;
    result_fragment = 99;
    CHECK(fd2_field_game_execute_storage_action(
              &target, FD2_FIELD_SYSTEM_ACTION_LOAD, path,
              &result_fragment) == 0);
    CHECK(result_fragment == 0);
    fd2_field_game expected = before;
    expected.save_resource_available = 0;
    CHECK(memcmp(&target, &expected, sizeof(target)) == 0);
    CHECK(remove(path) == 0);
    return 0;
}

static int read_save_bytes(const char *path, uint8_t bytes[FD2_SAVE_FILE_SIZE]) {
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    int result = fread(bytes, 1, FD2_SAVE_FILE_SIZE, file) == FD2_SAVE_FILE_SIZE &&
                 fgetc(file) == EOF ? 0 : -1;
    if (fclose(file) != 0) result = -1;
    return result;
}

/* 4df09/4df28独立文件oracle；不调用被测checksum/XOR或snapshot exporter。 */
static void original_save_encrypted(const uint8_t *plain, uint8_t *encrypted) {
    memcpy(encrypted, plain, 0x59cbu);
    uint32_t sum = 0;
    for (size_t i = 0; i < 0x59c7u; i++) sum += plain[i];
    for (unsigned byte = 0; byte < 4u; byte++)
        encrypted[0x59c7u + byte] = (uint8_t)(sum >> (byte * 8u));
    unsigned state = 0xa5u;
    for (size_t i = 0; i < 0x59cbu; i++) {
        unsigned added = (state + 0x9014u) & 65535u;
        state = ((added * 8u) & 65535u) + added / 8192u;
        encrypted[i] ^= (uint8_t)state;
    }
}

static int write_original_plain(const char *path, const uint8_t *plain) {
    uint8_t encrypted[FD2_SAVE_FILE_SIZE];
    original_save_encrypted(plain, encrypted);
    FILE *file = fopen(path, "wb");
    if (!file) return -1;
    int result = fwrite(encrypted, 1, sizeof(encrypted), file) ==
        sizeof(encrypted) ? 0 : -1;
    if (fclose(file) != 0) result = -1;
    return result;
}

static int test_loaded_metadata_owner_complete_files_and_reload(void) {
    /* START合法快照/当前真实资源前缀；只给不解释的metadata尾不同来源。
     * 这是后端跨文件owner写集，不宣称固定FD2.SAV UI自然发生过。 */
    fd2_save_file source = {0};
    fd2_archive other = {0}, field = {0};
    const uint8_t *resource;
    size_t resource_size;
    CHECK(fd2_archive_open(&field, "original_game/FDFIELD.DAT") == 0);
    CHECK(fd2_archive_get(&field, 1, &resource, &resource_size) == 0);
    CHECK(resource_size < 0x8a3u);
    CHECK(fd2_save_file_open(&source, "original_game/START.SAV") == 0);
    uint8_t a[FD2_SAVE_FILE_SIZE], b[FD2_SAVE_FILE_SIZE];
    uint8_t expected[FD2_SAVE_FILE_SIZE], encoded[FD2_SAVE_FILE_SIZE];
    uint8_t actual[FD2_SAVE_FILE_SIZE];
    memcpy(a, source.data, sizeof(a));
    memcpy(b, source.data, sizeof(b));
    fd2_save_file_close(&source);
    for (size_t i = resource_size; i < 0x8a3u; i++) {
        a[i] = (uint8_t)(i * 13u + 0x27u);
        b[i] = (uint8_t)(i * 7u + 0x91u);
    }
    size_t count = a[0x30c4u];
    CHECK(count > 0 && count <= 96u);
    memset(b + 0x12a3u + count * 80u, 0x6c, 0x1e00u - count * 80u);
    memset(b + 0x30d5u, 0x9e, 0x68u - 18u);
    for (size_t slot = 0; slot < 4u; slot++) {
        size_t tail = 0x312bu + slot * 0xa28u + 0xa00u + 10u;
        size_t end = 0x312bu + (slot + 1u) * 0xa28u;
        if (end > 0x59c7u) end = 0x59c7u;
        memset(b + tail, 0x3b, end - tail);
    }
    char paths[3][FD2_TEST_PATH_MAX];
    for (size_t i = 0; i < 3; i++)
        CHECK(fd2_test_temp_path(paths[i]) == 0 && remove(paths[i]) == 0);
    CHECK(write_original_plain(paths[0], a) == 0);
    CHECK(write_original_plain(paths[1], b) == 0);
    fd2_field_game *loaded = calloc(1, sizeof(*loaded));
    fd2_field_game *before = malloc(sizeof(*before));
    CHECK(loaded && before);
    CHECK(fd2_archive_open(&other, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_field_game_open_resources(loaded, &other, 0) == 0);
    CHECK(loaded->metadata.source_size == resource_size);
    CHECK(!memcmp(loaded->metadata.source_bytes, resource, resource_size));
    CHECK(fd2_field_game_load_active(loaded, paths[0]) == 0);
    CHECK(loaded->metadata.source_size == 2211u);
    CHECK(!memcmp(loaded->metadata.source_bytes, a, 2211u));
    /* 已解析字段是唯一可变真值；backing仍留Load来源而导出必须用新值。 */
    loaded->metadata.turn_events[2].turn ^= 0x5au;
    loaded->metadata.unit_templates[0].bytes[0x11] ^= 0x37u;
    a[0x09u] ^= 0x5au;
    a[0x94u] ^= 0x37u;
    CHECK(loaded->metadata.source_bytes[0x09u] != a[0x09u]);
    CHECK(loaded->metadata.source_bytes[0x94u] != a[0x94u]);
    /* 19df7：整metadata+roster、count*80、32B cell、meta前18B。
     * 其它目标字节原样保留；末4B独立checksum，再比较整22987B密文。 */
    for (size_t target = 0; target < 3; target++) {
        if (target == 0) memcpy(expected, a, sizeof(expected));
        else if (target == 1) memcpy(expected, b, sizeof(expected));
        /* 新目标整ff是H确定初始化；原19fcd只写四slot哨兵，
         * 此分支不是原fresh目标未知区的全字节oracle。 */
        else memset(expected, 0xff, sizeof(expected));
        memcpy(expected, a, 0x12a3u);
        memcpy(expected + 0x12a3u, a + 0x12a3u, count * 80u);
        original_active_load_records(expected + 0x12a3u, count);
        memcpy(expected + 0x30a3u, a + 0x30a3u, 32u);
        memcpy(expected + 0x30c3u, a + 0x30c3u, 18u);
        original_save_encrypted(expected, encoded);
        CHECK(fd2_field_game_save_active(loaded, paths[target]) == 0);
        CHECK(read_save_bytes(paths[target], actual) == 0);
        CHECK(!memcmp(actual, encoded, sizeof(actual)));
    }
    /* 取消只退出菜单，不替换metadata owner/actor或写文件。 */
    loaded->interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
    fd2_field_system_menu_init(&loaded->system_menu);
    *before = *loaded;
    CHECK(fd2_field_game_cancel_system_menu(loaded) == FD2_FIELD_SYSTEM_ACTION_CANCEL);
    CHECK(!memcmp(&loaded->metadata, &before->metadata, sizeof(loaded->metadata)));
    CHECK(!memcmp(&loaded->units, &before->units, sizeof(loaded->units)));
    CHECK(read_save_bytes(paths[2], actual) == 0 && !memcmp(actual, encoded, sizeof(actual)));
    /* A→B→A重载各自完整来源，预检/错误都不清已有owner。 */
    CHECK(write_original_plain(paths[1], b) == 0);
    CHECK(fd2_field_game_load_active(loaded, paths[1]) == 0);
    CHECK(loaded->metadata.source_size == 2211u &&
          !memcmp(loaded->metadata.source_bytes, b, 2211u));
    *before = *loaded;
    CHECK(fd2_field_game_save_resource_available(loaded, paths[0]) == 1);
    CHECK(!memcmp(loaded, before, sizeof(*loaded)));
    CHECK(fd2_field_game_load_active(loaded, paths[0]) == 0);
    CHECK(!memcmp(loaded->metadata.source_bytes, a, 2211u));
    *before = *loaded;
    b[0] = 29; /* checksum有效但shape不匹配，不能替换来源/部分写class。 */
    CHECK(write_original_plain(paths[1], b) == 0);
    CHECK(fd2_field_game_load_active(loaded, paths[1]) != 0);
    CHECK(!memcmp(loaded, before, sizeof(*loaded)));
    fd2_field_game_close(loaded);
    free(loaded); free(before);
    fd2_archive_close(&other); fd2_archive_close(&field);
    for (size_t i = 0; i < 3; i++) CHECK(remove(paths[i]) == 0);
    return 0;
}

static int test_invalid_existing_save_is_not_replaced(void) {
    char path[FD2_TEST_PATH_MAX];
    CHECK(fd2_test_temp_path(path) == 0);
    CHECK(remove(path) == 0 && write_fixture(path) == 0);
    uint8_t before_file[FD2_SAVE_FILE_SIZE], after_file[FD2_SAVE_FILE_SIZE];
    CHECK(read_save_bytes(path, before_file) == 0);
    FILE *file = fopen(path, "r+b");
    CHECK(file != NULL && fseek(file, 20, SEEK_SET) == 0);
    before_file[20] ^= 1;
    CHECK(fputc(before_file[20], file) != EOF && fclose(file) == 0);
    fd2_field_game game;
    init_game(&game);
    fd2_field_game before = game;
    CHECK(fd2_field_game_save_active(&game, path) != 0);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    CHECK(fd2_field_game_save_manual_slot(&game, path, 1) != 0);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    CHECK(read_save_bytes(path, after_file) == 0);
    CHECK(memcmp(after_file, before_file, sizeof(before_file)) == 0);

    /* 截断文件也属于原档，不能当作不存在而重建。 */
    const size_t truncated_size = 16;
    file = fopen(path, "wb");
    CHECK(file != NULL);
    CHECK(fwrite(before_file, 1, truncated_size, file) == truncated_size);
    CHECK(fclose(file) == 0);
    CHECK(fd2_field_game_save_active(&game, path) != 0);
    CHECK(fd2_field_game_save_manual_slot(&game, path, 1) != 0);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    file = fopen(path, "rb");
    CHECK(file != NULL);
    CHECK(fread(after_file, 1, truncated_size, file) == truncated_size);
    CHECK(fgetc(file) == EOF && fclose(file) == 0);
    CHECK(memcmp(after_file, before_file, truncated_size) == 0);
    CHECK(remove(path) == 0);
    return 0;
}

static void stop_session(fd2_field_game *game) {
    game->failure = FD2_FIELD_FAILURE_EVENT_UNSUPPORTED;
    memset(&game->failure_notice, 0x5a, sizeof(game->failure_notice));
}

static int test_stopped_session_save_and_restore(void) {
    char path[FD2_TEST_PATH_MAX];
    CHECK(fd2_test_temp_path(path) == 0);
    CHECK(remove(path) == 0 && write_fixture(path) == 0);
    fd2_field_game game;
    init_game(&game);
    CHECK(fd2_field_game_save_active(&game, path) == 0);
    CHECK(fd2_field_game_save_manual_slot(&game, path, 2) == 0);
    uint8_t before_file[FD2_SAVE_FILE_SIZE], after_file[FD2_SAVE_FILE_SIZE];
    CHECK(read_save_bytes(path, before_file) == 0);
    stop_session(&game);
    game.pending_cell_event = (fd2_field_pending_cell_event){1, 9, 3};
    game.player_post_actor = 1;
    game.player_post_waiting_phase = 1;
    fd2_field_game before = game;
    fd2_save_battle_snapshot snapshot, before_snapshot;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    before_snapshot = snapshot;
    CHECK(fd2_field_game_export_battle_snapshot(&game, &snapshot) != 0);
    CHECK(memcmp(&snapshot, &before_snapshot, sizeof(snapshot)) == 0);
    CHECK(fd2_field_game_save_active(&game, path) != 0);
    CHECK(fd2_field_game_save_manual_slot(&game, path, 2) != 0);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    CHECK(read_save_bytes(path, after_file) == 0);
    CHECK(memcmp(before_file, after_file, sizeof(before_file)) == 0);

    game.units.items[0].hp = 1;
    CHECK(fd2_field_game_load_active(&game, path) == 0);
    fd2_field_event_notice empty_notice = {0};
    CHECK(game.failure == FD2_FIELD_FAILURE_NONE);
    CHECK(!game.pending_cell_event.valid);
    CHECK(!game.player_post_waiting_phase && game.player_post_actor == 0);
    CHECK(memcmp(&game.failure_notice, &empty_notice, sizeof(empty_notice)) == 0);
    CHECK(game.units.items[0].hp == 100);
    stop_session(&game);
    game.units.items[0].hp = 2;
    before = game;
    CHECK(fd2_field_game_load_manual_slot(&game, path, 2) != 0);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    CHECK(read_save_bytes(path, after_file) == 0);
    CHECK(memcmp(before_file, after_file, sizeof(before_file)) == 0);
    CHECK(remove(path) == 0);
    return 0;
}

static int test_failed_load_preserves_stopped_session(void) {
    char path[FD2_TEST_PATH_MAX];
    CHECK(fd2_test_temp_path(path) == 0);
    CHECK(remove(path) == 0 && write_fixture(path) == 0);
    fd2_field_game source, target;
    init_game(&source);
    init_game(&target);
    CHECK(fd2_field_game_save_active(&source, path) == 0);
    CHECK(fd2_field_game_save_manual_slot(&source, path, 2) == 0);
    stop_session(&target);
    target.pending_cell_event = (fd2_field_pending_cell_event){1, 42, 5};
    target.player_post_actor = 1;
    target.player_post_waiting_phase = 1;
    fd2_field_game before = target;
    uint8_t valid_file[FD2_SAVE_FILE_SIZE], before_file[FD2_SAVE_FILE_SIZE];
    uint8_t after_file[FD2_SAVE_FILE_SIZE];
    CHECK(read_save_bytes(path, valid_file) == 0);

    FILE *file = fopen(path, "r+b");
    CHECK(file != NULL && fseek(file, 20, SEEK_SET) == 0);
    CHECK(fputc(valid_file[20] ^ 1, file) != EOF && fclose(file) == 0);
    CHECK(read_save_bytes(path, before_file) == 0);
    CHECK(fd2_field_game_load_active(&target, path) != 0);
    CHECK(memcmp(&target, &before, sizeof(target)) == 0);
    CHECK(fd2_field_game_load_manual_slot(&target, path, 2) != 0);
    CHECK(memcmp(&target, &before, sizeof(target)) == 0);
    CHECK(read_save_bytes(path, after_file) == 0);
    CHECK(memcmp(before_file, after_file, sizeof(before_file)) == 0);

    /* 校验和有效但 stage 不支持，也不能清除原停止原因或写回文件。 */
    CHECK(write_fixture(path) == 0);
    CHECK(fd2_field_game_save_active(&source, path) == 0);
    CHECK(fd2_field_game_save_manual_slot(&source, path, 2) == 0);
    fd2_save_file save = {0};
    fd2_save_battle_snapshot snapshot;
    fd2_save_manual_slot slot;
    fd2_save_manual_state state;
    CHECK(fd2_save_file_open(&save, path) == 0);
    CHECK(fd2_save_file_get_battle_snapshot(&save, &snapshot) == 0);
    snapshot.meta[FD2_SAVE_BATTLE_META_STAGE] = 1;
    CHECK(fd2_save_file_update_battle_snapshot(&save, &snapshot) == 0);
    CHECK(fd2_save_file_get_manual_slot(&save, 2, &slot) == 0);
    CHECK(fd2_save_manual_slot_decode(&slot, &state) == 0);
    state.stage_id = 1;
    CHECK(fd2_save_manual_slot_encode(&slot, &state) == 0);
    CHECK(fd2_save_file_update_manual_slot(&save, 2, &slot) == 0);
    CHECK(fd2_save_file_write(&save, path) == 0);
    fd2_save_file_close(&save);
    CHECK(read_save_bytes(path, before_file) == 0);
    CHECK(fd2_field_game_import_battle_snapshot(&target, &snapshot) != 0);
    CHECK(memcmp(&target, &before, sizeof(target)) == 0);
    CHECK(fd2_field_game_load_active(&target, path) != 0);
    CHECK(memcmp(&target, &before, sizeof(target)) == 0);
    CHECK(fd2_field_game_load_manual_slot(&target, path, 2) != 0);
    CHECK(memcmp(&target, &before, sizeof(target)) == 0);
    CHECK(read_save_bytes(path, after_file) == 0);
    CHECK(memcmp(before_file, after_file, sizeof(before_file)) == 0);
    CHECK(remove(path) == 0);
    return 0;
}

static int test_metadata_serialization_and_atomic_import(void) {
    fd2_field_game source;
    init_game(&source);
    fd2_field_unit_template templates[2];
    for (size_t i = 0; i < sizeof(templates); i++)
        ((uint8_t *)templates)[i] = (uint8_t)(i * 7u + 3u);
    source.metadata.unit_template_count = 2;
    source.metadata.unit_templates = templates;
    for (size_t i = 0; i < FD2_FIELD_EVENT_SLOTS; i++) {
        source.metadata.turn_events[i] = (fd2_field_turn_event){
            (uint8_t)(i + 1u), (uint8_t)(i + 2u), (uint8_t)(i % 3u)};
        source.metadata.cell_lookup[i] = (fd2_field_cell_lookup){
            (uint8_t)(i + 30u), (uint8_t)(i & 1u)};
        source.metadata.cell_actions[i] = (fd2_field_cell_action){
            (uint8_t)(i + 4u), (uint16_t)(0x1200u + i)};
    }
    fd2_save_battle_snapshot snapshot;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    CHECK(fd2_field_game_export_battle_snapshot(&source, &snapshot) == 0);
    const size_t used = 0x83u + sizeof(templates);
    CHECK(memcmp(snapshot.metadata + 0x83u, templates, sizeof(templates)) == 0);
    for (size_t i = 0; i < FD2_FIELD_EVENT_SLOTS; i++) {
        CHECK(snapshot.metadata[0x03u + i * 3u] == i + 1u);
        CHECK(snapshot.metadata[0x04u + i * 3u] == i + 2u);
        CHECK(snapshot.metadata[0x05u + i * 3u] == i % 3u);
        CHECK(snapshot.metadata[0x33u + i * 2u] == i + 30u);
        CHECK(snapshot.metadata[0x34u + i * 2u] == (i & 1u));
        CHECK(snapshot.metadata[0x53u + i * 3u] == i + 4u);
        CHECK(snapshot.metadata[0x54u + i * 3u] == i);
        CHECK(snapshot.metadata[0x55u + i * 3u] == 0x12);
    }
    for (size_t i = used; i < sizeof(snapshot.metadata); i++)
        CHECK(snapshot.metadata[i] == 0xa5);

    fd2_field_game target;
    init_game(&target);
    CHECK(fd2_field_metadata_open_mem(&target.metadata, snapshot.metadata, used) == 0);
    target.metadata.turn_events[0].turn = 99;
    target.metadata.cell_actions[0].param = 0xffff;
    fd2_field_game before = target;
    fd2_field_unit_template before_templates[2];
    memcpy(before_templates, target.metadata.unit_templates, sizeof(before_templates));
    snapshot.metadata[0] = 29; /* 旧 stage 29 prefix，不能重载未知 shape。 */
    CHECK(fd2_field_game_import_battle_snapshot(&target, &snapshot) != 0);
    CHECK(memcmp(&target, &before, sizeof(target)) == 0);
    CHECK(memcmp(target.metadata.unit_templates, before_templates,
                 sizeof(before_templates)) == 0);
    snapshot.metadata[0] = source.metadata.shape_index;
    /* +1/+2 尚无静态不变语义；同 shape 的快照必须原样恢复这两字节。 */
    snapshot.metadata[1] = 0x42;
    snapshot.metadata[2] = 0x99;
    CHECK(fd2_field_game_import_battle_snapshot(&target, &snapshot) == 0);
    CHECK(target.metadata.unit_template_count == 2);
    CHECK(target.metadata.source_size == 2211u &&
          !memcmp(target.metadata.source_bytes, snapshot.metadata, 2211u));
    CHECK(target.metadata.unknown_01 == 0x42 && target.metadata.unknown_02 == 0x99);
    CHECK(memcmp(target.metadata.unit_templates, templates, sizeof(templates)) == 0);
    for (size_t i = 0; i < FD2_FIELD_EVENT_SLOTS; i++) {
        CHECK(target.metadata.turn_events[i].turn == source.metadata.turn_events[i].turn);
        CHECK(target.metadata.turn_events[i].action == source.metadata.turn_events[i].action);
        CHECK(target.metadata.turn_events[i].phase == source.metadata.turn_events[i].phase);
        CHECK(target.metadata.cell_lookup[i].event_code == source.metadata.cell_lookup[i].event_code);
        CHECK(target.metadata.cell_lookup[i].match_arg == source.metadata.cell_lookup[i].match_arg);
        CHECK(target.metadata.cell_actions[i].mode == source.metadata.cell_actions[i].mode);
        CHECK(target.metadata.cell_actions[i].param == source.metadata.cell_actions[i].param);
    }
    fd2_field_metadata_close(&target.metadata);
    return 0;
}

static int test_metadata_capacity_and_invalid_export(void) {
    fd2_field_game game;
    init_game(&game);
    fd2_field_unit_template templates[80];
    memset(templates, 0x5a, sizeof(templates));
    game.metadata.unit_template_count = 80;
    game.metadata.unit_templates = templates;
    fd2_save_battle_snapshot snapshot;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    CHECK(fd2_field_game_export_battle_snapshot(&game, &snapshot) == 0);
    CHECK(memcmp(snapshot.metadata + 0x83u, templates, sizeof(templates)) == 0);
    fd2_save_battle_snapshot before = snapshot;
    game.metadata.unit_template_count = 81;
    CHECK(fd2_field_game_export_battle_snapshot(&game, &snapshot) != 0);
    CHECK(memcmp(&snapshot, &before, sizeof(snapshot)) == 0);
    game.metadata.unit_template_count = 1;
    game.metadata.unit_templates = NULL;
    CHECK(fd2_field_game_export_battle_snapshot(&game, &snapshot) != 0);
    CHECK(memcmp(&snapshot, &before, sizeof(snapshot)) == 0);
    CHECK(fd2_field_game_import_battle_snapshot(&game, &snapshot) != 0);
    CHECK(fd2_field_game_import_battle_snapshot(&game, NULL) != 0);
    /* 原资源parser不因序列化backing定长而新增拒绝；超过窗口的完整模板
     * 仍按既有格式解析。现Save的80模板容量门保持原有合同，不在此扩门。 */
    uint8_t larger_resource[0x83u + 81u * FD2_FIELD_UNIT_TEMPLATE_SIZE];
    for (size_t i = 0; i < sizeof(larger_resource); i++)
        larger_resource[i] = (uint8_t)(i * 11u + 3u);
    fd2_field_metadata metadata = {0};
    CHECK(fd2_field_metadata_open_mem(&metadata, larger_resource,
                                      sizeof(larger_resource)) == 0);
    CHECK(metadata.unit_template_count == 81u && metadata.source_size == 2211u);
    CHECK(!memcmp(metadata.source_bytes, larger_resource, 2211u));
    CHECK(!memcmp(metadata.unit_templates[80].bytes, larger_resource +
                  0x83u + 80u * FD2_FIELD_UNIT_TEMPLATE_SIZE,
                  FD2_FIELD_UNIT_TEMPLATE_SIZE));
    fd2_field_metadata_close(&metadata);
    for (size_t i = 0; i < sizeof(metadata); i++)
        CHECK(((const uint8_t *)&metadata)[i] == 0);
    game.metadata.unit_template_count = 80;
    game.metadata.unit_templates = templates;
    game.metadata.source_size = FD2_FIELD_METADATA_BACKING_SIZE + 1u;
    CHECK(fd2_field_game_export_battle_snapshot(&game, &snapshot) != 0);
    CHECK(!memcmp(&snapshot, &before, sizeof(snapshot)));
    return 0;
}

static int test_stage0_metadata_matches_resource_and_preserves_tail(void) {
    fd2_archive field = {0};
    CHECK(fd2_archive_open(&field, "original_game/FDFIELD.DAT") == 0);
    const uint8_t *resource;
    size_t size;
    CHECK(fd2_archive_get(&field, 1, &resource, &size) == 0);
    CHECK(size == 0x83u + 31u * FD2_FIELD_UNIT_TEMPLATE_SIZE);
    CHECK(resource[0] == 0 && resource[1] == 4 && resource[2] == 30);
    fd2_field_game game;
    init_game(&game);
    CHECK(fd2_field_metadata_open_mem(&game.metadata, resource, size) == 0);
    fd2_save_file save = {0};
    CHECK(fd2_save_file_create_empty(&save) == 0);
    save.data[FD2_SAVE_BATTLE_META_BASE + FD2_SAVE_BATTLE_META_UNIT_COUNT] = 0;
    save.data[FD2_SAVE_BATTLE_META_BASE + FD2_SAVE_BATTLE_META_ROSTER_COUNT] = 0;
    put_u32le(save.data + save.size - FD2_SAVE_CHECKSUM_SIZE,
              fd2_save_checksum_sum(save.data, save.size));
    uint8_t preserved[FD2_SAVE_BATTLE_UNIT_BASE];
    memcpy(preserved, save.data, sizeof(preserved));
    fd2_save_battle_snapshot snapshot;
    CHECK(fd2_save_file_get_battle_snapshot(&save, &snapshot) == 0);
    CHECK(fd2_field_game_export_battle_snapshot(&game, &snapshot) == 0);
    CHECK(fd2_save_file_update_battle_snapshot(&save, &snapshot) == 0);
    CHECK(memcmp(save.data, resource, size) == 0);
    /* fresh资源只有size字节来源；其后暂保目标，不能签作原堆初值。
     * 整个raw roster仍来自session，与metadata来源是不同区域。 */
    CHECK(memcmp(save.data + size, preserved + size,
                 FD2_SAVE_BATTLE_ROSTER_BASE - size) == 0);
    CHECK(memcmp(save.data + FD2_SAVE_BATTLE_ROSTER_BASE, game.roster,
                 sizeof(game.roster)) == 0);
    CHECK(game.metadata.unit_template_count == 31);
    fd2_save_file_close(&save);
    fd2_field_metadata_close(&game.metadata);
    fd2_archive_close(&field);
    return 0;
}

static unsigned load_rng_calls;

static uint32_t count_load_rng(void *userdata) {
    load_rng_calls++;
    return fd2_field_rng_next(userdata);
}

static int test_original_start_load_focus_and_sprite_bounds(void) {
    fd2_archive fdother = {0};
    fd2_field_game game = {0};
    fd2_vga *vga = calloc(1, sizeof(*vga));
    fd2_save_file save = {0};
    fd2_save_battle_snapshot original, candidate;
    fd2_field_rng rng;
    fd2_field_rng_seed(&rng, 0x3141);
    uint8_t source_before[FD2_SAVE_FILE_SIZE], source_after[FD2_SAVE_FILE_SIZE];
    uint8_t file_before[FD2_SAVE_FILE_SIZE], file_after[FD2_SAVE_FILE_SIZE];
    char path[FD2_TEST_PATH_MAX];
    CHECK(vga != NULL && fd2_test_temp_path(path) == 0);
    CHECK(fd2_archive_open(&fdother, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_field_game_open(&game, vga, &fdother, 0) == 0);
    CHECK(game.sprites.frame_count == 1680u);
    CHECK(read_save_bytes("original_game/START.SAV", source_before) == 0);
    CHECK(fd2_save_file_open(&save, "original_game/START.SAV") == 0);
    CHECK(fd2_save_file_get_battle_snapshot(&save, &original) == 0);
    CHECK(fd2_save_file_write(&save, path) == 0);
    game.attack_rng = count_load_rng;
    game.attack_rng_userdata = &rng;
    load_rng_calls = 0;
    game.focus_cycle_unit = 3;

    /* 已有合法checksum；仅变更普通可见actor的资源ID，不能隐形成功。 */
    const uint8_t bad_ids[] = {140, 255, 140, 140};
    const uint8_t flags[] = {0, 0, 0x80, 0x04};
    for (size_t i = 0; i < sizeof(bad_ids); i++) {
        candidate = original;
        candidate.unit_area[7] = bad_ids[i];
        candidate.unit_area[5] = flags[i];
        if (i == 3) candidate.unit_area[0x1f] = 10; /* 普通renderer无race豁免。 */
        CHECK(fd2_save_file_update_battle_snapshot(&save, &candidate) == 0);
        CHECK(fd2_save_file_write(&save, path) == 0);
        CHECK(read_save_bytes(path, file_before) == 0);
        fd2_field_game before = game;
        CHECK(fd2_field_game_import_battle_snapshot(&game, &candidate) != 0);
        CHECK(fd2_field_game_load_active(&game, path) != 0);
        CHECK(fd2_field_game_save_resource_available(&game, path) == 0);
        CHECK(memcmp(&game, &before, sizeof(game)) == 0);
        CHECK(game.focus_cycle_unit == 3 && load_rng_calls == 0 && rng.state == 0x3141);
        CHECK(read_save_bytes(path, file_after) == 0);
        CHECK(memcmp(file_before, file_after, sizeof(file_before)) == 0);
    }

    candidate = original;
    candidate.unit_area[7] = 139; /* 最后完整12帧ID合法。 */
    CHECK(fd2_field_game_import_battle_snapshot(&game, &candidate) == 0);
    CHECK(game.units.items[0].unit_id == 139 && game.focus_cycle_unit == 0);
    candidate.unit_area[7] = 121;
    candidate.unit_area[0x1f] = 10;
    CHECK(fd2_field_game_import_battle_snapshot(&game, &candidate) == 0);
    CHECK(game.units.items[0].unit_id == 121 && game.units.items[0].race == 10);

    /* hidden资源不参与普通renderer，保留其未知ID而不新增F1门。 */
    candidate = original;
    size_t hidden = 9u * FD2_FIELD_UNIT_RECORD_SIZE;
    candidate.unit_area[hidden + 5u] |= FD2_FIELD_UNIT_FLAG_HIDDEN;
    candidate.unit_area[hidden + 7u] = 255;
    candidate.unit_area[hidden] = 254;
    candidate.unit_area[hidden + 1u] = 255; /* 原stage1缺部署槽也可保越界hidden坐标。 */
    CHECK(fd2_field_game_import_battle_snapshot(&game, &candidate) == 0);
    CHECK(game.units.items[9].unit_id == 255 &&
          fd2_field_unit_is_hidden(&game.units.items[9]) &&
          game.units.items[9].x == 254 && game.units.items[9].y == 255);

    game.focus_cycle_unit = 3;
    CHECK(fd2_field_game_load_active(&game, "original_game/START.SAV") == 0);
    CHECK(game.focus_cycle_unit == 0 && load_rng_calls == 0 && rng.state == 0x3141);
    CHECK(fd2_field_game_cycle_focus(&game) == 0);
    CHECK(game.cursor_cell_x == game.units.items[0].x &&
          game.cursor_cell_y == game.units.items[0].y &&
          game.focus_cycle_unit == 1 && load_rng_calls == 0 && rng.state == 0x3141);
    CHECK(read_save_bytes("original_game/START.SAV", source_after) == 0);
    CHECK(memcmp(source_before, source_after, sizeof(source_before)) == 0);
    fd2_save_file_close(&save);
    fd2_field_game_close(&game);
    fd2_archive_close(&fdother);
    free(vga);
    CHECK(remove(path) == 0);
    return 0;
}

/* 独立按原函数写序计算：205b4先2、side0可见写0、raw0隐藏写1；
 * 206c5再以raw5..10全hidden覆盖1。这里映射原1/2到SDL枚举，不读冷图。 */
static fd2_field_battle_result original_record_result(const fd2_field_game *game) {
    unsigned result = 2;
    for (size_t i = 0; i < game->units.count; i++)
        if (!(game->units.items[i].flags & 1) && game->units.items[i].side == 0)
            result = 0;
    if (game->units.items[0].flags & 1) result = 1;
    if (game->stage == 1) {
        unsigned hidden = 0;
        for (size_t i = 5; i <= 10; i++) hidden += !!(game->units.items[i].flags & 1);
        if (hidden == 6) result = 1;
    }
    return result == 1 ? FD2_FIELD_BATTLE_DEFEAT :
        result == 2 ? FD2_FIELD_BATTLE_VICTORY : FD2_FIELD_BATTLE_ONGOING;
}

static int test_import_preserves_result_latch_and_original_query(void) {
    static const unsigned villager_masks[] = {0, 1, 32, 63};
    for (size_t stage = 0; stage <= 1; stage++) {
        fd2_field_game source, target;
        init_game(&source); init_game(&target);
        source.stage = target.stage = stage;
        source.units.count = stage ? 12 : 2;
        size_t enemy = stage ? 11 : 1;
        for (size_t i = 0; i < source.units.count; i++) {
            memset(&source.units.items[i], (int)(0x30u + i), FD2_FIELD_UNIT_RECORD_SIZE);
            source.units.items[i].x = (uint8_t)(4u + i);
            source.units.items[i].y = (uint8_t)(10u + i);
            source.units.items[i].unit_id = (uint8_t)(20u + i);
            source.units.items[i].flags = 0;
            source.units.items[i].side = (uint8_t)(1u + (i & 1u));
            source.units.items[i].hp = source.units.items[i].hp_max = 20;
        }
        source.units.items[0].side = 2;
        fd2_field_rng rng;
        fd2_field_rng_seed(&rng, 0x3141);
        target.attack_rng = count_load_rng;
        target.attack_rng_userdata = &rng;
        load_rng_calls = 0;
        for (unsigned leader_hidden = 0; leader_hidden < 2; leader_hidden++)
        for (unsigned enemy_hidden = 0; enemy_hidden < 2; enemy_hidden++)
        for (unsigned side = 0; side < 3; side++)
        for (unsigned hp = 0; hp < 4; hp++)
        for (size_t mask = 0; mask < (stage ? 4u : 1u); mask++) {
            source.units.items[0].flags = (uint8_t)leader_hidden;
            source.units.items[0].hp = (hp & 1u) ? 20 : 0;
            source.units.items[enemy].flags = (uint8_t)enemy_hidden;
            source.units.items[enemy].side = (uint8_t)side;
            source.units.items[enemy].hp = (hp & 2u) ? 20 : 0;
            if (stage) for (size_t i = 5; i <= 10; i++) {
                source.units.items[i].flags = (uint8_t)((villager_masks[mask] >> (i - 5u)) & 1u);
                source.units.items[i].hp = (hp & 1u) ? 20 : 0;
            }
            fd2_save_battle_snapshot snapshot;
            memset(&snapshot, 0xa5, sizeof(snapshot));
            CHECK(fd2_field_game_export_battle_snapshot(&source, &snapshot) == 0);
            fd2_save_battle_snapshot before_snapshot = snapshot;
            fd2_field_battle_result expected = original_record_result(&source);
            /* 10010仅恢复，不调用1197b；旧53ECC由实际controller后置点更新。 */
            target.battle_result = hp & 1u ? FD2_FIELD_BATTLE_VICTORY : FD2_FIELD_BATTLE_DEFEAT;
            fd2_field_battle_result latch = target.battle_result;
            target.focus_cycle_unit = 2;
            CHECK(fd2_field_game_import_battle_snapshot(&target, &snapshot) == 0);
            CHECK(target.battle_result == latch && target.focus_cycle_unit == 0);
            CHECK(!memcmp(&snapshot, &before_snapshot, sizeof(snapshot)));
            CHECK(original_active_units_match(&target.units, &snapshot));
            fd2_field_game before_query = target;
            for (unsigned repeat = 0; repeat < 3; repeat++)
                CHECK(fd2_field_game_battle_result(&target) == expected);
            CHECK(!memcmp(&target, &before_query, sizeof(target)));
            CHECK(target.attack_rng == count_load_rng && target.attack_rng_userdata == &rng);
            CHECK(load_rng_calls == 0 && rng.state == 0x3141);
        }
        fd2_field_metadata_close(&target.metadata);
    }
    /* 原函数正常域之外仅验SDL安全门，不向原码oracle发送空表/缺村民。 */
    fd2_field_game defensive = {0};
    defensive.ready = 1;
    defensive.battle_result = FD2_FIELD_BATTLE_VICTORY;
    for (size_t stage = 0; stage <= 1; stage++) {
        defensive.stage = stage;
        for (size_t count = 0; count < (stage ? 11u : 1u); count++) {
            defensive.units.count = count;
            fd2_field_game before = defensive;
            CHECK(fd2_field_game_battle_result(&defensive) == FD2_FIELD_BATTLE_DEFEAT);
            CHECK(!memcmp(&defensive, &before, sizeof(before)));
        }
    }
    return 0;
}

static int test_original_start_roster_and_recruit_transaction(void) {
    fd2_archive fdother = {0};
    CHECK(fd2_archive_open(&fdother, "original_game/FDOTHER.DAT") == 0);
    fd2_vga *vga = calloc(1, sizeof(*vga));
    CHECK(vga);
    fd2_field_game game;
    CHECK(fd2_field_game_open(&game, vga, &fdother, 0) == 0);
    CHECK(game.roster_count == 4 && game.units.count == 12);
    const uint8_t ids[] = {0, 9, 4, 30};
    for (size_t i = 0; i < sizeof(ids); i++) {
        CHECK(game.roster[i].unit_id == ids[i] && game.roster[i].side == 2);
        CHECK(game.units.items[i].unit_id == ids[i]);
    }
    CHECK(((const uint8_t *)&game.roster[1])[0x1a] == 1);
    CHECK(((const uint8_t *)&game.units.items[1])[0x1a] == 1);
    fd2_field_handoff handoff = {0};
    handoff.valid = 1;
    handoff.units = game.units;
    ((uint8_t *)&handoff.units.items[1])[0x1a] = 0;
    handoff.camera_cell_x = game.camera_cell_x;
    handoff.camera_cell_y = game.camera_cell_y;
    handoff.focus_cell_x = game.cursor_cell_x;
    handoff.focus_cell_y = game.cursor_cell_y;
    CHECK(fd2_field_game_apply_handoff(&game, &handoff) == 0);
    CHECK(game.roster_count == 4 &&
          ((const uint8_t *)&game.roster[1])[0x1a] == 1 &&
          ((const uint8_t *)&game.units.items[1])[0x1a] == 1);

    fd2_save_file save = {0};
    fd2_save_battle_snapshot snapshot;
    CHECK(fd2_save_file_open(&save, "original_game/START.SAV") == 0);
    CHECK(fd2_save_file_get_battle_snapshot(&save, &snapshot) == 0);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] == 12 &&
          snapshot.meta[FD2_SAVE_BATTLE_META_ROSTER_COUNT] == 4);
    CHECK(fd2_field_game_import_battle_snapshot(&game, &snapshot) == 0);
    CHECK(game.units.count == 12 && game.roster_count == 4);
    CHECK(memcmp(game.roster, snapshot.roster, sizeof(game.roster)) == 0);

    game.turn_number = 3;
    CHECK(fd2_field_game_end_player_turn(&game) == 0);
    CHECK(game.roster_count == 5 && game.roster[4].unit_id == 1 &&
          game.roster[4].level == 3);
    /* 真续态只执行到首PAN；后续story/增援仍由同一父PC等待，
     * 不再追加已处理notice或预先执行整段再Scene重放。 */
    fd2_static_flow_node leaf;size_t actor;
    CHECK(game.units.count==13 && game.event_log_count==0 && game.event_total_count==0);
    CHECK(game.event_resume.active &&
          fd2_field_game_common_event_peek(&game,&leaf,&actor)==1);
    CHECK(!strcmp(leaf.op,"pan") && leaf.args[0]==5 && leaf.args[1]==8 && actor==0);

    CHECK(fd2_field_game_import_battle_snapshot(&game, &snapshot) == 0);
    CHECK(!game.event_resume.active);
    game.turn_number = 3;
    /* 缺group3身份所需placement，完整只读预检应在招募前拒绝。
     * 这是预检无写合同，不是执行后回滚已提交前缀。 */
    game.placements.count = 0;
    fd2_field_unit before_roster[FD2_SAVE_BATTLE_MAX_ROSTER];
    memcpy(before_roster, game.roster, sizeof(before_roster));
    fd2_field_units before_units = game.units;
    CHECK(fd2_field_game_end_player_turn(&game) != 0);
    CHECK(game.failure == FD2_FIELD_FAILURE_ACTION);
    CHECK(!game.event_resume.active && game.event_log_count==0 && game.event_total_count==0);
    CHECK(game.roster_count == 4);
    CHECK(memcmp(game.roster, before_roster, sizeof(before_roster)) == 0);
    CHECK(memcmp(&game.units, &before_units, sizeof(before_units)) == 0);
    fd2_save_file_close(&save);
    fd2_field_game_close(&game);
    free(vga);
    fd2_archive_close(&fdother);
    return 0;
}

static int test_progress_owner_load_and_save_boundaries(void) {
    fd2_field_game source,target;init_game(&source);init_game(&target);
    fd2_save_battle_snapshot snapshot={0};
    CHECK(fd2_field_game_export_battle_snapshot(&source,&snapshot)==0);
    target.experience_value=0xffffffed;
    target.progress.notice=(fd2_field_progress_notice){FD2_FIELD_PROGRESS_GAIN,0,488,5,0,0};
    target.progress.total=105;target.progress.resume=FD2_FIELD_PROGRESS_RESUME_PLAYER;
    fd2_save_battle_snapshot sentry;memset(&sentry,0xa5,sizeof(sentry));
    fd2_save_battle_snapshot before_sentry=sentry;
    CHECK(fd2_field_game_export_battle_snapshot(&target,&sentry)!=0);
    CHECK(memcmp(&sentry,&before_sentry,sizeof(sentry))==0);
    fd2_save_battle_snapshot invalid=snapshot;
    invalid.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT]=FD2_FIELD_MAX_UNITS+1u;
    fd2_field_game before=target;
    CHECK(fd2_field_game_import_battle_snapshot(&target,&invalid)!=0);
    CHECK(memcmp(&target,&before,sizeof(target))==0);
    CHECK(fd2_field_game_import_battle_snapshot(&target,&snapshot)==0);
    CHECK(target.experience_value==0xffffffed && !target.progress.resume && !target.progress.notice.kind && !target.progress.total);
    target.progress.resume=FD2_FIELD_PROGRESS_RESUME_PHASE;
    CHECK(fd2_field_game_export_battle_snapshot(&target,&sentry)!=0);
    CHECK(memcmp(&sentry,&before_sentry,sizeof(sentry))==0);
    char path[FD2_TEST_PATH_MAX];CHECK(fd2_test_temp_path(path)==0);CHECK(remove(path)==0);
    CHECK(fd2_field_game_save_active(&target,path)==-1);
    CHECK(fd2_field_game_save_manual_slot(&target,path,0)==-1);
    FILE *unexpected=fopen(path,"rb");CHECK(unexpected==NULL);
    return 0;
}

static int load_preflight_probe(const fd2_field_game *game,
        const fd2_field_progress_notice *notice, void *userdata) {
    (void)game; (void)notice;
    (*(unsigned *)userdata)++;
    return -1; /* Load必须只借此owner，不能调用它推进奖励。 */
}

static int load_loot_probe(const fd2_field_game *game,
        const fd2_field_loot_notice *notice, void *userdata) {
    (void)game; (void)notice;
    (*(unsigned *)userdata)++;
    return -1;
}

static int load_music_probe(size_t stage, int track,
        fd2_field_music_reason reason, void *userdata) {
    (void)stage; (void)track; (void)reason;
    (*(unsigned *)userdata)++;
    return 0;
}

static int test_stage1_manual_owner_preflight(void) {
    fd2_save_file source = {0};
    fd2_save_battle_snapshot snapshot;
    CHECK(fd2_save_file_open(&source, "original_game/START.SAV") == 0);
    CHECK(fd2_save_file_get_battle_snapshot(&source, &snapshot) == 0);
    fd2_save_manual_state state = {0};
    state.stage_id = 1;
    state.unit_count = snapshot.meta[FD2_SAVE_BATTLE_META_ROSTER_COUNT];
    memcpy(state.unit_area, snapshot.roster, sizeof(state.unit_area));
    state.gold_or_flags = 0x98765432u;
    memcpy(state.options, (uint8_t[]){1, 0, 1, 0}, 4);
    /* 手工Load原29974整0xa00恢复，29a17虽调用11019却不存返回class。
     * 预检也不准把persistent raw2按ID重写，保全80B与inactive尾。 */
    for (size_t i = 0; i < state.unit_count; i++)
        state.unit_area[i * 80u + 2u] = (uint8_t)(0xd0u + i);
    fd2_save_manual_state before = state;
    CHECK(fd2_field_game_validate_manual_state(&state) == 0);
    CHECK(memcmp(&state, &before, sizeof(state)) == 0);
    char path[FD2_TEST_PATH_MAX];
    CHECK(fd2_test_temp_path(path) == 0 && remove(path) == 0);
    CHECK(fd2_save_manual_state_save(path, 1, &state) == 0);
    fd2_save_manual_state restored;
    CHECK(fd2_save_manual_state_load(path, 1, &restored) == 0);
    CHECK(restored.stage_id == before.stage_id &&
          restored.unit_count == before.unit_count &&
          restored.gold_or_flags == before.gold_or_flags &&
          !memcmp(restored.options, before.options, sizeof(restored.options)) &&
          !memcmp(restored.unit_area, before.unit_area, sizeof(restored.unit_area)));
    CHECK(remove(path) == 0);
    /* owner32与部署选择器容量独立；这里不得截断、造默认成员或拒17..32。 */
    for (size_t i = state.unit_count; i < FD2_SAVE_MAX_UNITS; i++)
        memcpy(state.unit_area + i * FD2_SAVE_UNIT_RECORD_SIZE,
               state.unit_area, FD2_SAVE_UNIT_RECORD_SIZE);
    state.unit_count = FD2_SAVE_MAX_UNITS;
    before = state;
    CHECK(fd2_field_game_validate_manual_state(&state) == 0);
    CHECK(memcmp(&state, &before, sizeof(state)) == 0);
    state.unit_count = 0;
    CHECK(fd2_field_game_validate_manual_state(&state) == 0);
    state = before;
    state.stage_id = 0;
    CHECK(fd2_field_game_validate_manual_state(&state) != 0);
    state.stage_id = 2;
    CHECK(fd2_field_game_validate_manual_state(&state) == 0);
    state.stage_id = 21;
    CHECK(fd2_field_game_validate_manual_state(&state) == 0);
    CHECK(fd2_field_stage_is_supported(21) && fd2_field_stage_battle_is_supported(21));
    /* S5候选沿26行普通准备及NoTown27..29核全局浏览资源。
     * 所有完整32人/金额原bits/unknown尾不变，codec与entry支持门保持独立。 */
    for (uint8_t stage=1;stage<30;stage++) {
        state.stage_id=stage;fd2_save_manual_state unchanged=state;
        CHECK(fd2_field_game_validate_manual_state(&state)==0);
        CHECK(!memcmp(&state,&unchanged,sizeof(state)));
        CHECK(fd2_field_stage_is_supported(stage) && fd2_field_stage_battle_is_supported(stage));
    }
    CHECK(!fd2_field_stage_prepare_record_get(27));
    const uint8_t invalid_stages[]={0,30,31,32,255};
    for (size_t i=0;i<sizeof(invalid_stages);i++) {
        state.stage_id=invalid_stages[i];fd2_save_manual_state unchanged=state;
        CHECK(fd2_field_game_validate_manual_state(&state)!=0 && !memcmp(&state,&unchanged,sizeof(state)));
    }
    state = before;
    state.unit_count = FD2_SAVE_MAX_UNITS + 1u;
    CHECK(fd2_field_game_validate_manual_state(&state) != 0);
    state = before;
    state.unit_area[7] = 140; /* 普通可浏览持久成员必须有完整FDICON资源。 */
    CHECK(fd2_field_game_validate_manual_state(&state) != 0);
    state = before;
    state.unit_area[0x0a] = 0x40; state.unit_area[0x0b] = 255;
    fd2_save_manual_state invalid = state;
    CHECK(fd2_field_game_validate_manual_state(&state) != 0);
    CHECK(memcmp(&state, &invalid, sizeof(state)) == 0);
    fd2_save_file_close(&source);
    return 0;
}

static int test_cross_stage_active_owner_is_transactional(void) {
    fd2_archive fdother = {0};
    fd2_field_game game = {0}, stage1 = {0};
    fd2_save_file source = {0};
    fd2_save_battle_snapshot original, next;
    CHECK(fd2_archive_open(&fdother, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_save_file_open(&source, "original_game/START.SAV") == 0);
    CHECK(fd2_save_file_get_battle_snapshot(&source, &original) == 0);
    CHECK(fd2_field_game_open_resources(&game, &fdother, 0) == 0);
    /* 1088d@1097a：实际资源的每cell按原4df4c reset，非资源raw0假默认。 */
    const uint8_t *raw_map;size_t raw_map_size;
    CHECK(fd2_archive_get(&game.field_archive,0,&raw_map,&raw_map_size)==0 &&
        raw_map_size==4u+(size_t)game.map.width*(size_t)game.map.height*4u);
    for(size_t i=0;i<(size_t)game.map.width*(size_t)game.map.height;i++) {
        const uint8_t *raw=raw_map+4u+i*4u;
        uint32_t cell=(uint32_t)raw[0]|((uint32_t)raw[1]<<8)|((uint32_t)raw[2]<<16)|((uint32_t)raw[3]<<24);
        CHECK(game.map.cells[i]==((cell&UINT32_C(0x001f03ff))|UINT32_C(0xff000000)));
    }
    CHECK(fd2_field_game_import_battle_snapshot(&game, &original) == 0);
    /* 10010@10276：失败不动live mark；正常Load从资源重建后raw3全ff。 */
    size_t marker=2u*(size_t)game.map.width+2u;
    game.map.cells[marker]&=UINT32_C(0x00ffffff);
    uint32_t marked=game.map.cells[marker];
    fd2_save_battle_snapshot invalid_marker_snapshot=original;
    invalid_marker_snapshot.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT]=97;
    CHECK(fd2_field_game_import_battle_snapshot(&game,&invalid_marker_snapshot)!=0 &&
        game.map.cells[marker]==marked);
    CHECK(fd2_field_game_import_battle_snapshot(&game,&original)==0);
    for(size_t i=0;i<(size_t)game.map.width*(size_t)game.map.height;i++)
        CHECK((game.map.cells[i]>>24)==0xffu);
    /* 10010→12263：完成态不只是一组byte，必须从原地图重建；否则
     * 同session再次载入未开启的旧档会留下已开箱底图。 */
    uint32_t closed = fd2_field_map_cell(&game.map, 7, 13);
    size_t cell_index = (closed >> 16) & 31u;
    CHECK((game.terrain.attrs[fd2_field_cell_terrain(closed)].flags & 0x60u) == 0x20u);
    fd2_save_battle_snapshot opened = original;
    opened.cell_state[cell_index] = 1;
    CHECK(fd2_field_game_import_battle_snapshot(&game, &opened) == 0);
    CHECK(fd2_field_map_cell(&game.map, 7, 13) ==
          ((closed & UINT32_C(0xff000000)) | (uint16_t)((uint16_t)closed + 1u)));
    CHECK(fd2_field_game_import_battle_snapshot(&game, &original) == 0);
    CHECK(fd2_field_map_cell(&game.map, 7, 13) == closed);
    CHECK(fd2_field_game_open_resources(&stage1, &fdother, 1) == 0);
    /* 本夹具只隔离跨stage存取，不声称是32d18的真实stage1开场部署。 */
    stage1.units = game.units;
    stage1.roster_count = game.roster_count;
    memcpy(stage1.roster, game.roster, sizeof(stage1.roster));
    for (size_t i = 0; i < stage1.units.count; i++) {
        stage1.units.items[i].x = (uint8_t)(i % 8u + 2u);
        stage1.units.items[i].y = (uint8_t)(i % 5u + 10u);
    }
    stage1.turn_number = 6;
    stage1.camera_cell_x = 3; stage1.camera_cell_y = 8;
    stage1.cursor_cell_x = 8; stage1.cursor_cell_y = 13;
    for (size_t i = 0; i < sizeof(stage1.cell_action_completed); i++)
        stage1.cell_action_completed[i] = (uint8_t)(i * 7u + 3u);
    stage1.resource_value = 1234567u;
    next = original;
    stage1.units.count = 10;
    CHECK(fd2_field_game_export_battle_snapshot(&stage1, &next) != 0);
    CHECK(memcmp(&next, &original, sizeof(next)) == 0);
    stage1.units.count = game.units.count;
    CHECK(fd2_field_game_export_battle_snapshot(&stage1, &next) == 0);
    next.meta[FD2_SAVE_BATTLE_META_OPTION_MUSIC] = 0x7fu;
    next.meta[FD2_SAVE_BATTLE_META_OPTION_CAMERA] = 4u;
    CHECK(next.metadata[0] == 1 && stage1.metadata.unit_template_count == 40);
    fd2_field_game_close(&stage1);

    fd2_field_rng rng;
    fd2_field_rng_seed(&rng, 0x5678);
    unsigned preflight_calls = 0;
    game.attack_rng = count_load_rng; game.attack_rng_userdata = &rng;
    game.progress_preflight = load_preflight_probe;
    game.progress_preflight_userdata = &preflight_calls;
    game.loot_preflight = load_loot_probe;
    game.loot_preflight_userdata = &preflight_calls;
    game.music_request = load_music_probe;
    game.music_request_userdata = &preflight_calls;
    game.experience_value = 631;
    game.message_number = 12345;
    game.loot.notice.kind = FD2_FIELD_LOOT_MESSAGE;
    game.loot.source = 1;
    game.loot.step = 3;
    game.pending_cell_event.valid = 1;
    game.player_post_waiting_phase = 1;
    game.focus_cycle_unit = 3;
    game.battle_result = FD2_FIELD_BATTLE_DEFEAT;
    load_rng_calls = 0;
    fd2_save_battle_snapshot sentry = next, sentry_before = next;
    CHECK(fd2_field_game_export_battle_snapshot(&game, &sentry) != 0);
    CHECK(memcmp(&sentry, &sentry_before, sizeof(sentry)) == 0);
    fd2_field_game before = game;
    fd2_save_battle_snapshot bad = next;
    bad.metadata[0] = 0; /* stage1标记不能配stage0 shape。 */
    CHECK(fd2_field_game_import_battle_snapshot(&game, &bad) != 0);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    bad = next;
    bad.unit_area[7] = 140; bad.unit_area[5] = 0;
    CHECK(fd2_field_game_import_battle_snapshot(&game, &bad) != 0);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    bad = next;
    bad.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] = FD2_FIELD_MAX_UNITS + 1u;
    CHECK(fd2_field_game_import_battle_snapshot(&game, &bad) != 0);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    bad = next;
    bad.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT] = 10;
    CHECK(fd2_field_game_import_battle_snapshot(&game, &bad) != 0);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);

    char path[FD2_TEST_PATH_MAX];
    CHECK(fd2_test_temp_path(path) == 0);
    CHECK(fd2_save_file_update_battle_snapshot(&source, &next) == 0);
    CHECK(fd2_save_file_write(&source, path) == 0);
    uint8_t bytes_before[FD2_SAVE_FILE_SIZE], bytes_after[FD2_SAVE_FILE_SIZE];
    CHECK(read_save_bytes(path, bytes_before) == 0);
    CHECK(fd2_field_game_save_resource_available(&game, path) == 1);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    CHECK(fd2_field_game_load_active(&game, path) == 0);
    CHECK(game.stage == 1 && game.map.width == 27 && game.map.height == 21);
    CHECK(game.battle_result == FD2_FIELD_BATTLE_DEFEAT);
    CHECK(game.metadata.shape_index == 1 && game.metadata.unit_template_count == 40);
    CHECK(game.metadata.source_size == 2211u &&
          !memcmp(game.metadata.source_bytes, next.metadata, 2211u));
    CHECK(game.stage_runtime_entry_offset == 0x238du && game.turn_number == 6);
    CHECK(game.fdother == &fdother && game.font.glyphs != NULL);
    CHECK(game.experience_value == 631 && game.attack_rng == count_load_rng &&
          game.attack_rng_userdata == &rng && game.progress_preflight == load_preflight_probe &&
          game.progress_preflight_userdata == &preflight_calls);
    CHECK(game.message_number == 12345 && game.loot_preflight == load_loot_probe &&
          game.loot_preflight_userdata == &preflight_calls && game.music_request == load_music_probe &&
          game.music_request_userdata == &preflight_calls && !game.loot.notice.kind && !game.loot.step);
    CHECK(load_rng_calls == 0 && preflight_calls == 0 && rng.state == 0x5678);
    CHECK(game.focus_cycle_unit == 0 && !game.pending_cell_event.valid &&
          !game.player_post_waiting_phase && game.interaction == FD2_FIELD_INTERACTION_BROWSE);
    CHECK(memcmp(game.cell_action_completed, next.cell_state, sizeof(next.cell_state)) == 0);
    fd2_save_battle_snapshot exported = original;
    CHECK(fd2_field_game_export_battle_snapshot(&game, &exported) == 0);
    CHECK(memcmp(exported.cell_state, next.cell_state, sizeof(next.cell_state)) == 0);
    CHECK(memcmp(exported.roster, next.roster, sizeof(next.roster)) == 0);
    CHECK(exported.meta[FD2_SAVE_BATTLE_META_OPTION_MUSIC] == 0x7fu &&
          exported.meta[FD2_SAVE_BATTLE_META_OPTION_CAMERA] == 4u);
    CHECK(original_active_units_match(&game.units, &next));
    CHECK(!memcmp(exported.unit_area, game.units.items,
                  game.units.count * FD2_SAVE_UNIT_RECORD_SIZE));
    CHECK(!memcmp(exported.metadata, next.metadata, sizeof(next.metadata)));
    CHECK(read_save_bytes(path, bytes_after) == 0);
    CHECK(memcmp(bytes_before, bytes_after, sizeof(bytes_before)) == 0);
    /* 反向再次预备/交接，旧资源关闭一次；没有ctor、phase或intro副作用。 */
    game.battle_result = FD2_FIELD_BATTLE_VICTORY;
    CHECK(fd2_field_game_import_battle_snapshot(&game, &original) == 0);
    CHECK(game.stage == 0 && game.stage_runtime_entry_offset == 0 &&
          game.experience_value == 631 && game.attack_rng_userdata == &rng &&
          game.battle_result == FD2_FIELD_BATTLE_VICTORY);
    CHECK(game.metadata.source_size == 2211u &&
          !memcmp(game.metadata.source_bytes, original.metadata, 2211u));
    game.battle_result = FD2_FIELD_BATTLE_DEFEAT;
    CHECK(fd2_field_game_import_battle_snapshot(&game, &next) == 0);
    CHECK(load_rng_calls == 0 && preflight_calls == 0 && rng.state == 0x5678 &&
          game.battle_result == FD2_FIELD_BATTLE_DEFEAT);
    fd2_field_game_close(&game);
    fd2_save_file_close(&source);
    fd2_archive_close(&fdother);
    CHECK(remove(path) == 0);
    return 0;
}

/* S3只测10010的资源/存档交接：构造沿1088d部署槽及10b4e原group，
 * 不把跳过对白/移动的隔离前态称作完整entry或原版自然战局。 */
static int prepare_batch_save_game(fd2_field_game *game,const fd2_archive *other,
        const fd2_save_battle_snapshot *original,size_t stage) {
    fd2_save_manual_state state={0};
    state.stage_id=(uint8_t)stage;
    state.unit_count=original->meta[FD2_SAVE_BATTLE_META_ROSTER_COUNT];
    memcpy(state.unit_area,original->roster,sizeof(state.unit_area));
    char id[64];snprintf(id,sizeof(id),"stage.source.entry.%03zu",stage);
    CHECK(fd2_field_game_prepare_stage_source_flow(game,other,&state,id)==0);
    fd2_static_flow_node setup={0};setup.op="stage_setup_source";
    unsigned port;
    CHECK(fd2_field_game_stage_source_node(game,&setup,&port)>0);
    const char *ids[]={"stage.source.initialize",id};
    for(size_t i=0;i<2;i++) {
        const fd2_static_flow *flow=fd2_static_flow_lookup("stage",ids[i]);
        CHECK(flow);
        for(size_t j=0;j<flow->count;j++) {
            const fd2_static_flow_node *node=fd2_static_flow_at(flow,(int)j);
            if(!strcmp(node->op,"append_group"))
                CHECK(fd2_field_game_stage_source_node(game,node,&port)>0);
        }
    }
    return 0;
}

static int test_batch_stage_snapshot_capability_and_growth(void) {
    static const size_t stages[]={2,3,12};
    static const size_t initial_counts[]={15,24,59};
    fd2_archive other={0};fd2_save_file file={0};
    fd2_save_battle_snapshot original;
    CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_save_file_open(&file,"original_game/START.SAV")==0);
    CHECK(fd2_save_file_get_battle_snapshot(&file,&original)==0);
    fd2_field_game *live=calloc(1,sizeof(*live)),*source=calloc(1,sizeof(*source));
    fd2_field_game *before=malloc(sizeof(*before));
    CHECK(live && source && before);
    CHECK(fd2_field_game_open_resources(live,&other,0)==0);
    CHECK(fd2_field_game_import_battle_snapshot(live,&original)==0);
    fd2_field_rng rng;fd2_field_rng_seed(&rng,0x6271);
    live->attack_rng=count_load_rng;live->attack_rng_userdata=&rng;load_rng_calls=0;
    unsigned preflight_calls=0;
    live->progress_preflight=load_preflight_probe;
    live->progress_preflight_userdata=&preflight_calls;
    live->music_request=load_music_probe;live->music_request_userdata=&preflight_calls;
    char path[FD2_TEST_PATH_MAX];CHECK(fd2_test_temp_path(path)==0);
    for(size_t si=0;si<sizeof(stages)/sizeof(stages[0]);si++) {
        CHECK(prepare_batch_save_game(source,&other,&original,stages[si])==0);
        CHECK(source->units.count==initial_counts[si]);
        fd2_field_stage_capability cap;
        CHECK(fd2_field_stage_capability_get(stages[si],&cap)==0 && !cap.exact_units);
        for(unsigned growth=0;growth<2;growth++) {
            if(growth) {
                size_t prior=source->units.count;
                CHECK(fd2_field_units_append_group_placed(&source->units,&source->metadata,
                    &source->placements,&source->map,2,1)==0 && source->units.count>prior);
            }
            /* F1原turn writer/Save BYTE域已证；这里单独核254/255的
             * 存取边界，不执行默认255 callback、不伪造256自然续局。 */
            for(unsigned boundary_turn=254;boundary_turn<=255;boundary_turn++) {
            /* 控制隐藏前态允许原BYTE回绕；raw0可见HP0仍原样保存。 */
            for(size_t i=0;i<source->units.count;i++)source->units.items[i].flags|=1u;
            source->units.items[0].flags=0;source->units.items[0].x=source->units.items[0].y=0;
            source->units.items[0].hp=0;
            size_t last=source->units.count-1u;
            source->units.items[last].x=source->units.items[last].y=255;
            source->turn_number=boundary_turn;source->active_side=2;
            source->camera_cell_x=source->camera_cell_y=0;
            source->cursor_cell_x=source->cursor_cell_y=0;
            /* schedule_turn的BYTE是快照状态，不要求等于DAT，也不把
             * 默认action255预检成不可Load；实际到达由原dispatcher拒绝。 */
            source->metadata.turn_events[15]=(fd2_field_turn_event){255,255,0};
            source->metadata.turn_events[0].turn=17;
            source->cell_action_completed[31]=0x9d;
            fd2_save_battle_snapshot snap=original;
            memset(snap.meta+18,0x7b,sizeof(snap.meta)-18u);
            size_t prefix=0x83u+source->metadata.unit_template_count*FD2_FIELD_UNIT_TEMPLATE_SIZE;
            memset(snap.metadata+prefix,0xa7,sizeof(snap.metadata)-prefix);
            CHECK(fd2_field_game_export_battle_snapshot(source,&snap)==0);
            fd2_save_battle_snapshot frozen=snap;
            CHECK(fd2_save_file_update_battle_snapshot(&file,&snap)==0 && fd2_save_file_write(&file,path)==0);
            *before=*live;
            CHECK(fd2_field_game_save_resource_available(live,path)==1 && !memcmp(before,live,sizeof(*live)));
            live->battle_result=FD2_FIELD_BATTLE_DEFEAT;
            CHECK(fd2_field_game_load_active(live,path)==0);
            CHECK(live->stage==stages[si] && live->units.count==source->units.count &&
                live->battle_result==FD2_FIELD_BATTLE_DEFEAT && live->turn_number==boundary_turn &&
                live->active_side==2 && live->focus_cycle_unit==0 && !live->event_resume.active &&
                live->interaction==FD2_FIELD_INTERACTION_BROWSE && live->fdother==&other);
            CHECK(original_active_units_match(&live->units,&snap));
            CHECK(live->metadata.turn_events[0].turn==17 && live->metadata.turn_events[15].action==255 &&
                live->cell_action_completed[31]==0x9d && !load_rng_calls && !preflight_calls && rng.state==0x6271);
            CHECK(live->attack_rng==count_load_rng && live->attack_rng_userdata==&rng &&
                live->progress_preflight==load_preflight_probe && live->progress_preflight_userdata==&preflight_calls &&
                live->music_request==load_music_probe && live->music_request_userdata==&preflight_calls);
            fd2_save_battle_snapshot roundtrip=frozen;
            fd2_save_battle_snapshot load_expected=frozen;
            original_active_load_records(load_expected.unit_area,live->units.count);
            CHECK(fd2_field_game_export_battle_snapshot(live,&roundtrip)==0 &&
                  !memcmp(&roundtrip,&load_expected,sizeof(load_expected)));
            *before=*live;
            fd2_save_battle_snapshot bad=snap;
            bad.unit_area[last*FD2_FIELD_UNIT_RECORD_SIZE+5u]&=0xfeu;
            CHECK(fd2_field_game_import_battle_snapshot(live,&bad)==-1 && !memcmp(before,live,sizeof(*live)));
            bad=snap;bad.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT]=(uint8_t)(cap.minimum_units-1u);
            CHECK(fd2_save_file_update_battle_snapshot(&file,&bad)==0 && fd2_save_file_write(&file,path)==0);
            CHECK(fd2_field_game_save_resource_available(live,path)==0 && !memcmp(before,live,sizeof(*live)));
            CHECK(fd2_field_game_load_active(live,path)==-1 && !memcmp(before,live,sizeof(*live)));
            bad=snap;bad.meta[FD2_SAVE_BATTLE_META_STAGE]=29;
            CHECK(fd2_field_game_import_battle_snapshot(live,&bad)==-1 && !memcmp(before,live,sizeof(*live)));
            CHECK(!load_rng_calls && !preflight_calls && rng.state==0x6271 && !memcmp(&snap,&frozen,sizeof(snap)));
            }
        }
        fd2_field_game_close(source);
    }
    fd2_field_game_close(live);free(live);free(source);free(before);
    fd2_save_file_close(&file);fd2_archive_close(&other);CHECK(remove(path)==0);
    return 0;
}
/* Win32保留100ns FILETIME；Linux保留纳秒，不能用粗秒mtime掩盖重写。 */
static int file_write_identity(const char *path,uint64_t identity[2]) {
#if defined(_WIN32)
    WIN32_FILE_ATTRIBUTE_DATA info;
    if(!GetFileAttributesExA(path,GetFileExInfoStandard,&info))return -1;
    identity[0]=((uint64_t)info.nFileSizeHigh<<32)|info.nFileSizeLow;
    identity[1]=((uint64_t)info.ftLastWriteTime.dwHighDateTime<<32)|info.ftLastWriteTime.dwLowDateTime;
#else
    struct stat info;
    if(stat(path,&info)!=0)return -1;
    identity[0]=(uint64_t)info.st_size;
    identity[1]=(uint64_t)info.st_mtim.tv_sec*UINT64_C(1000000000)+(uint64_t)info.st_mtim.tv_nsec;
#endif
    return 0;
}
static uint32_t detail_busy_random(void *userdata) {
    (*(unsigned *)userdata)++;return 0;
}
static int test_detail_busy_preserves_outputs_and_files(void) {
    /* 真实START→Game详情open；不伪造portrait或把本API负例说成UI故障。 */
    fd2_archive other={0};fd2_field_game game;fd2_vga vga={0};
    CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_field_game_open(&game,&vga,&other,0)==0);
    CHECK(fd2_field_game_load_active(&game,"original_game/START.SAV")==0);
    char path[FD2_TEST_PATH_MAX],missing[FD2_TEST_PATH_MAX];
    CHECK(fd2_test_temp_path(path)==0 && remove(path)==0);
    CHECK(fd2_test_temp_path(missing)==0 && remove(missing)==0);
    CHECK(fd2_field_game_save_active(&game,path)==0);
    CHECK(fd2_field_game_save_manual_slot(&game,path,2)==0);
    uint8_t before_file[FD2_SAVE_FILE_SIZE],after_file[FD2_SAVE_FILE_SIZE];
    uint64_t before_identity[2],after_identity[2];
    CHECK(read_save_bytes(path,before_file)==0 && file_write_identity(path,before_identity)==0);
    unsigned rng_calls=0;
    game.attack_rng=detail_busy_random;game.attack_rng_userdata=&rng_calls;
    CHECK(fd2_field_game_open_detail(&game,1)==0 && game.detail_visible);
    fd2_field_game before=game;
    fd2_save_battle_snapshot output,sentry;
    memset(&output,0xa5,sizeof(output));sentry=output;
    CHECK(fd2_field_game_export_battle_snapshot(&game,&output)==-1);
    CHECK(!memcmp(&output,&sentry,sizeof(output)) && !memcmp(&before,&game,sizeof(game)));
    for(int manual=0;manual<2;manual++) {
        int result=manual?fd2_field_game_save_manual_slot(&game,path,2):fd2_field_game_save_active(&game,path);
        CHECK(result==-1 && !memcmp(&before,&game,sizeof(game)) && !rng_calls);
        CHECK(read_save_bytes(path,after_file)==0 && file_write_identity(path,after_identity)==0);
        CHECK(!memcmp(before_file,after_file,sizeof(before_file)) &&
            !memcmp(before_identity,after_identity,sizeof(before_identity)));
        result=manual?fd2_field_game_save_manual_slot(&game,missing,2):fd2_field_game_save_active(&game,missing);
        CHECK(result==-1 && !memcmp(&before,&game,sizeof(game)) && !rng_calls);
        FILE *unexpected=fopen(missing,"rb");CHECK(unexpected==NULL);
    }
    fd2_field_game_close_detail(&game);
    CHECK(!game.detail_visible && !game.detail_portrait.pixels);
    CHECK(fd2_field_game_export_battle_snapshot(&game,&output)==0);
    CHECK(fd2_field_game_save_active(&game,path)==0 && fd2_field_game_save_manual_slot(&game,path,2)==0);
    CHECK(!rng_calls);
    /* 外来turn>BYTE不属F1已证正常前缀；安全拒绝不得改文件/owner。
     * 手工slot没有turn字段，不能给它另加这个活动Save门。 */
    CHECK(read_save_bytes(path,before_file)==0 && file_write_identity(path,before_identity)==0);
    const uint32_t invalid_turns[]={256u,UINT32_MAX};
    for(size_t i=0;i<sizeof(invalid_turns)/sizeof(invalid_turns[0]);i++) {
        game.turn_number=invalid_turns[i];before=game;output=sentry;
        CHECK(fd2_field_game_export_battle_snapshot(&game,&output)==-1 &&
            !memcmp(&output,&sentry,sizeof(output)) && !memcmp(&before,&game,sizeof(game)));
        CHECK(fd2_field_game_save_active(&game,path)==-1 && !memcmp(&before,&game,sizeof(game)));
        CHECK(read_save_bytes(path,after_file)==0 && file_write_identity(path,after_identity)==0 &&
            !memcmp(before_file,after_file,sizeof(before_file)) &&
            !memcmp(before_identity,after_identity,sizeof(before_identity)));
        CHECK(fd2_field_game_save_active(&game,missing)==-1 && !memcmp(&before,&game,sizeof(game)) && !rng_calls);
        FILE *unexpected=fopen(missing,"rb");CHECK(unexpected==NULL);
    }
    fd2_field_game_close(&game);fd2_archive_close(&other);
    CHECK(remove(path)==0);
    return 0;
}

/* test-only stdin协议供完整原码向量逐条比较真实C consumer，不解析JSON。
 * 正常源域以外的count不送入原函数；取消/恢复时点由上面的回归单独验证。 */
static int result_vector_probe(void) {
    unsigned stage, count;
    char hex[FD2_FIELD_MAX_UNITS * FD2_FIELD_UNIT_RECORD_SIZE * 2u + 1u];
    for (;;) {
        int parsed = scanf("%u %u %15360s", &stage, &count, hex);
        if (parsed == EOF) return 0;
        if (parsed != 3 || stage > 1 || count > FD2_FIELD_MAX_UNITS ||
            count < (stage ? 11u : 1u) || strlen(hex) != count * FD2_FIELD_UNIT_RECORD_SIZE * 2u)
            return 2;
        fd2_field_game game = {0};
        game.ready = 1; game.stage = stage; game.units.count = count;
        uint8_t *records = (uint8_t *)game.units.items;
        for (size_t i = 0; i < count * FD2_FIELD_UNIT_RECORD_SIZE; i++) {
            unsigned value = 0;
            for (size_t j = 0; j < 2; j++) {
                char c = hex[i * 2u + j];
                unsigned digit = c >= '0' && c <= '9' ? (unsigned)(c - '0') :
                    c >= 'a' && c <= 'f' ? (unsigned)(c - 'a') + 10u :
                    c >= 'A' && c <= 'F' ? (unsigned)(c - 'A') + 10u : 16u;
                if (digit == 16u) return 2;
                value = value * 16u + digit;
            }
            records[i] = (uint8_t)value;
        }
        printf("%u %d\n", stage, (int)fd2_field_game_battle_result(&game));
    }
}

int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "--s0-vectors")) return result_vector_probe();
    if (argc != 1) return 2;
    if (test_batch_stage_snapshot_capability_and_growth() != 0) return 1;
    if(test_detail_busy_preserves_outputs_and_files()!=0)return 1;
    if (test_stage1_manual_owner_preflight() != 0 ||
        test_cross_stage_active_owner_is_transactional() != 0) return 1;
    if (test_progress_owner_load_and_save_boundaries() != 0 ||
        test_export_preserves_unknown() != 0 ||
        test_runtime_unit_capacity() != 0 ||
        test_roster_capacity_is_independent() != 0 ||
        test_import_rebuilds_cache_and_writes_only_class() != 0 ||
        test_first_active_save() != 0 ||
        test_first_manual_then_active_save() != 0 ||
        test_loaded_roster_survives_save_to_other_files() != 0 ||
        test_loaded_metadata_owner_complete_files_and_reload() != 0 ||
        test_import_is_transactional() != 0 ||
        test_manual_save_and_load_unavailable() != 0 ||
        test_file_roundtrip() != 0 ||
        test_invalid_existing_save_is_not_replaced() != 0 ||
        test_stopped_session_save_and_restore() != 0 ||
        test_failed_load_preserves_stopped_session() != 0 ||
        test_metadata_serialization_and_atomic_import() != 0 ||
        test_metadata_capacity_and_invalid_export() != 0 ||
        test_stage0_metadata_matches_resource_and_preserves_tail() != 0 ||
        test_original_start_load_focus_and_sprite_bounds() != 0 ||
        test_import_preserves_result_latch_and_original_query() != 0 ||
        test_original_start_roster_and_recruit_transaction() != 0)
        return 1;
    puts("field_save_test: ok");
    return 0;
}
