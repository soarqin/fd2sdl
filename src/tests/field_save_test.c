/* 活动战场 FD2.SAV snapshot codec 测试。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_game.h"
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
    game->detail_unit = 1;
    game->detail_visible = 1;
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
        FD2_FIELD_MAX_UNITS + 1u, FD2_SAVE_BATTLE_MAX_UNITS, SIZE_MAX};
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
    CHECK(memcmp(after.metadata + 0x83u, before.metadata + 0x83u,
                 sizeof(after.metadata) - 0x83u) == 0);
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
    CHECK(target.stage_runtime_entry_offset == 0x238du);
    CHECK(memcmp(target.system_menu.option_values, game.option_values,
                 sizeof(target.option_values)) == 0);
    CHECK(target.selected_unit == -1 && target.detail_unit == -1);
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
    /* metadata 未使用尾来自目标容器；整个 raw roster 来自 session。 */
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
    CHECK(game.event_log_count == 1 && game.event_log[0].handled);

    CHECK(fd2_field_game_import_battle_snapshot(&game, &snapshot) == 0);
    game.turn_number = 3;
    game.placements.count = 0; /* 招募后 group 3 失败，必须一起回滚。 */
    fd2_field_unit before_roster[FD2_SAVE_BATTLE_MAX_ROSTER];
    memcpy(before_roster, game.roster, sizeof(before_roster));
    fd2_field_units before_units = game.units;
    CHECK(fd2_field_game_end_player_turn(&game) != 0);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_EXECUTION);
    CHECK(game.roster_count == 4);
    CHECK(memcmp(game.roster, before_roster, sizeof(before_roster)) == 0);
    CHECK(memcmp(&game.units, &before_units, sizeof(before_units)) == 0);
    fd2_save_file_close(&save);
    fd2_field_game_close(&game);
    free(vga);
    fd2_archive_close(&fdother);
    return 0;
}

int main(void) {
    if (test_export_preserves_unknown() != 0 ||
        test_runtime_unit_capacity() != 0 ||
        test_roster_capacity_is_independent() != 0 ||
        test_first_active_save() != 0 ||
        test_first_manual_then_active_save() != 0 ||
        test_loaded_roster_survives_save_to_other_files() != 0 ||
        test_import_is_transactional() != 0 ||
        test_manual_save_and_load_unavailable() != 0 ||
        test_file_roundtrip() != 0 ||
        test_invalid_existing_save_is_not_replaced() != 0 ||
        test_stopped_session_save_and_restore() != 0 ||
        test_failed_load_preserves_stopped_session() != 0 ||
        test_metadata_serialization_and_atomic_import() != 0 ||
        test_metadata_capacity_and_invalid_export() != 0 ||
        test_stage0_metadata_matches_resource_and_preserves_tail() != 0 ||
        test_original_start_roster_and_recruit_transaction() != 0)
        return 1;
    puts("field_save_test: ok");
    return 0;
}
