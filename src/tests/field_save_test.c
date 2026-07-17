/* 活动战场 FD2.SAV snapshot codec 测试。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_game.h"

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
    for (size_t i = 0; i < sizeof(game->cell_action_completed); i++)
        game->cell_action_completed[i] = (uint8_t)(i ^ 0x5a);
    fd2_field_system_menu_init(&game->system_menu);
}

static int test_export_preserves_unknown(void) {
    fd2_field_game game;
    init_game(&game);
    fd2_save_battle_snapshot snapshot;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    uint8_t before[sizeof(snapshot)];
    memcpy(before, &snapshot, sizeof(snapshot));
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
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT_COPY] == 3);
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
                 before + 3u * FD2_FIELD_UNIT_RECORD_SIZE,
                 FD2_SAVE_BATTLE_UNIT_AREA_SIZE -
                     3u * FD2_FIELD_UNIT_RECORD_SIZE) == 0);
    CHECK(memcmp(snapshot.cell_state, game.cell_action_completed,
                 sizeof(game.cell_action_completed)) == 0);
    CHECK(memcmp(snapshot.cell_state + sizeof(game.cell_action_completed),
                 before + FD2_SAVE_BATTLE_UNIT_AREA_SIZE +
                     sizeof(game.cell_action_completed),
                 FD2_SAVE_BATTLE_CELL_STATE_SIZE -
                     sizeof(game.cell_action_completed)) == 0);
    CHECK(memcmp(snapshot.meta + 18,
                 before + FD2_SAVE_BATTLE_UNIT_AREA_SIZE +
                     FD2_SAVE_BATTLE_CELL_STATE_SIZE + 18,
                 FD2_SAVE_BATTLE_META_SIZE - 18) == 0);
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
    put_u32le(plain + FD2_SAVE_FILE_SIZE - FD2_SAVE_CHECKSUM_SIZE,
              fd2_save_checksum_sum(plain, FD2_SAVE_FILE_SIZE));
    fd2_save_file save = {.data = plain, .size = FD2_SAVE_FILE_SIZE};
    int result = fd2_save_file_write(&save, path);
    free(plain);
    return result;
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

static int test_manual_slot_roundtrip(void) {
    static const char path[] = "/tmp/fd2sdl-field-manual-test.sav";
    (void)remove(path);
    fd2_field_game source;
    init_game(&source);
    CHECK(fd2_field_game_save_manual_slot(&source, path, 2) == 0);

    fd2_save_file save = {0};
    fd2_save_manual_slot raw;
    fd2_save_manual_state state;
    CHECK(fd2_save_file_open(&save, path) == 0);
    CHECK(fd2_save_file_get_manual_slot(&save, 2, &raw) == 0);
    CHECK(fd2_save_manual_slot_decode(&raw, &state) == 0);
    CHECK(state.stage_id == 0 && state.unit_count == source.units.count);
    CHECK(state.gold_or_flags == source.resource_value);
    CHECK(state.options[0] == source.option_values[3] &&
          state.options[1] == source.option_values[2] &&
          state.options[2] == source.option_values[0] &&
          state.options[3] == source.option_values[1]);
    CHECK(memcmp(state.unit_area, source.units.items,
                 FD2_SAVE_UNIT_TABLE_SIZE) == 0);
    fd2_save_file_close(&save);

    fd2_field_game target;
    init_game(&target);
    memset(target.units.items, 0, sizeof(target.units.items));
    target.resource_value = 0;
    memset(target.option_values, 0, sizeof(target.option_values));
    CHECK(fd2_field_game_load_manual_slot(&target, path, 2) == 0);
    CHECK(target.units.count == source.units.count);
    CHECK(memcmp(target.units.items, source.units.items,
                 FD2_SAVE_UNIT_TABLE_SIZE) == 0);
    CHECK(target.resource_value == source.resource_value);
    CHECK(target.stage_runtime_entry_offset == 0x238du);
    CHECK(memcmp(target.option_values, source.option_values,
                 sizeof(target.option_values)) == 0);
    CHECK(target.interaction == FD2_FIELD_INTERACTION_BROWSE);
    CHECK(target.system_result_fragment == 0 &&
          target.system_result_until_ms == 0 &&
          target.manual_slot_result_fragment == 0 &&
          target.manual_slot_result_until_ms == 0);
    for (size_t i = 0; i < target.units.count; i++) {
        CHECK(target.units.walk_frames[i] == 1);
        CHECK(target.units.source_template_indices[i] ==
              FD2_FIELD_UNIT_NO_TEMPLATE);
    }

    fd2_field_game before = target;
    CHECK(fd2_field_game_load_manual_slot(&target, path, 0) != 0);
    CHECK(memcmp(&target, &before, sizeof(target)) == 0);
    CHECK(remove(path) == 0);
    return 0;
}

static int test_file_roundtrip(void) {
    static const char path[] = "/tmp/fd2sdl-field-save-test.sav";
    (void)remove(path);
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

int main(void) {
    if (test_export_preserves_unknown() != 0 ||
        test_import_is_transactional() != 0 ||
        test_manual_slot_roundtrip() != 0 ||
        test_file_roundtrip() != 0)
        return 1;
    puts("field_save_test: ok");
    return 0;
}
