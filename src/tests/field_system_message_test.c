/* B1必需系统提示错误出口；真实存储后端只写Temp隔离副本，不创建窗口。
 * 测试TU私有呈现/输入替身，正式text/font解析与Save/Load不替换。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "field_play.h"
#include "field_rng.h"
#include "scene.h"
#include "test_temp.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

enum { ROUTE_PROMPT, ROUTE_ESCAPE, ROUTE_NO, ROUTE_SAVE, ROUTE_LOAD, ROUTE_SAVE_FAILURE };
enum { BAD_NONE, BAD_MISSING, BAD_GLYPH };
static int route_kind, bad_kind, in_route, actions, presents, delay_calls;
static int backend_calls, backend_done, backend_result, closed;
static int owner_ok, backend_rng_ok;
static uint64_t mock_ms;
static uint32_t original_resource, submitted_resource;
static uint32_t resource_at_close;
static uint16_t rng_before_backend;
static uint32_t opened_turn;
static uint8_t opened_side;
static fd2_field_unit opened_units[FD2_FIELD_MAX_UNITS];
static char storage_path[FD2_TEST_PATH_MAX];
static uint8_t ui_bytes[421 * 4];
static const uint8_t glyph_bytes[64] = {0x80,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0x40};

static uint64_t message_ticks(void) { return mock_ms; }
static void message_delay(uint32_t ms) { delay_calls++; mock_ms += ms; }
static void message_wait(fd2_vga *vga) {
    vga->frame_deadline_ns = vga->frame_interval_ns = 0;
}
static void message_present(fd2_vga *vga) {
    presents++;
    mock_ms += 16;
    if (in_route && backend_done) vga->input.quit_requested = 1;
}
static void message_present_timed(fd2_vga *vga, uint32_t ms) {
    message_present(vga);
    mock_ms += ms;
}

static int prepare_text(fd2_field_game *game, size_t target, int bad) {
    fd2_text_entry_close(&game->ui_text);
    for (size_t i = 0; i < 421; i++) {
        ui_bytes[i * 4] = 1;
        ui_bytes[i * 4 + 1] = 0;
        ui_bytes[i * 4 + 2] = ui_bytes[i * 4 + 3] = 0xff;
    }
    game->font = (fd2_font){glyph_bytes, sizeof(glyph_bytes), 2};
    game->ui_text.data = ui_bytes;
    game->ui_text.size = sizeof(ui_bytes);
    game->ui_text.fragment_count = bad == BAD_MISSING ? target : 421;
    game->ui_text.offsets = calloc(421, sizeof(*game->ui_text.offsets));
    if (!game->ui_text.offsets) return -1;
    for (size_t i = 0; i < 421; i++) game->ui_text.offsets[i] = (uint16_t)(i * 4u);
    if (bad == BAD_GLYPH) ui_bytes[target * 4] = 2; /* 首个越界字形。 */
    return 0;
}

static int message_open(fd2_field_game *game, fd2_vga *vga,
                          const fd2_archive *archive, size_t stage) {
    if (fd2_field_game_open(game, vga, archive, stage) != 0) return -1;
    /* 资源与初始actor都来自真实START；UI文本只替换本次坏fragment。 */
    if (fd2_field_game_load_active(game, "original_game/START.SAV") != 0) return -1;
    original_resource = game->resource_value;
    submitted_resource = original_resource ^ 0x01020304u;
    game->resource_value = submitted_resource;
    fd2_field_system_menu_init(&game->system_menu);
    game->interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
    if (route_kind == ROUTE_PROMPT) {
        game->system_menu.selected = 3;
    } else {
        game->system_menu.page = FD2_FIELD_SYSTEM_PAGE_CONFIRMATION;
        game->system_menu.confirmation_action = route_kind == ROUTE_SAVE || route_kind == ROUTE_SAVE_FAILURE
            ? FD2_FIELD_SYSTEM_ACTION_SAVE : route_kind == ROUTE_LOAD
            ? FD2_FIELD_SYSTEM_ACTION_LOAD : FD2_FIELD_SYSTEM_ACTION_END_TURN_CONFIRM;
        game->system_menu.confirmation_prompt_fragment = route_kind == ROUTE_LOAD
            ? FD2_FIELD_SYSTEM_TEXT_LOAD_PROMPT : FD2_FIELD_SYSTEM_TEXT_SAVE_PROMPT;
        game->system_menu.confirmation_accepted_fragment = route_kind == ROUTE_LOAD
            ? FD2_FIELD_SYSTEM_TEXT_LOAD_ACCEPTED : route_kind == ROUTE_SAVE || route_kind == ROUTE_SAVE_FAILURE
            ? FD2_FIELD_SYSTEM_TEXT_SAVE_SUCCESS : FD2_FIELD_SYSTEM_TEXT_END_TURN_ACCEPTED;
        game->system_menu.confirmation_failure_fragment = FD2_FIELD_SYSTEM_TEXT_SAVE_FAILURE;
        game->system_menu.confirmation_return_page = FD2_FIELD_SYSTEM_PAGE_SECONDARY;
        game->system_menu.confirmation_return_selected = 1;
        game->system_menu.confirmation_yes = route_kind != ROUTE_NO;
    }
    size_t target = route_kind == ROUTE_PROMPT ? FD2_FIELD_SYSTEM_TEXT_END_TURN_PROMPT
        : route_kind == ROUTE_ESCAPE || route_kind == ROUTE_NO || route_kind == ROUTE_SAVE_FAILURE
        ? FD2_FIELD_SYSTEM_TEXT_CANCELLED : route_kind == ROUTE_LOAD
        ? FD2_FIELD_SYSTEM_TEXT_LOAD_ACCEPTED : FD2_FIELD_SYSTEM_TEXT_SAVE_SUCCESS;
    if (prepare_text(game, target, bad_kind) != 0) return -1;
    opened_turn = game->turn_number;
    opened_side = game->active_side;
    memcpy(opened_units, game->units.items, sizeof(opened_units));
    return 0;
}

static void message_close(fd2_field_game *game) {
    closed++;
    owner_ok = game->turn_number == opened_turn && game->active_side == opened_side &&
        memcmp(opened_units, game->units.items, sizeof(opened_units)) == 0;
    resource_at_close = game->resource_value;
    if (backend_calls)
        backend_rng_ok &= ((fd2_field_rng *)game->attack_rng_userdata)->state == rng_before_backend;
    fd2_field_game_close(game);
}

static int message_storage(fd2_field_game *game, fd2_field_system_action action,
                             const char *path, size_t *fragment) {
    backend_calls++;
    rng_before_backend = ((fd2_field_rng *)game->attack_rng_userdata)->state;
    int result = fd2_field_game_execute_storage_action(game, action, path, fragment);
    backend_result = result;
    backend_done = 1;
    backend_rng_ok = ((fd2_field_rng *)game->attack_rng_userdata)->state == rng_before_backend;
    return result;
}

static int message_action(fd2_input *input, fd2_input_context context,
                            fd2_input_action *action) {
    (void)input; (void)context;
    actions++;
    *action = actions == 1
        ? route_kind == ROUTE_ESCAPE ? FD2_INPUT_ACTION_CANCEL : FD2_INPUT_ACTION_CONFIRM
        : FD2_INPUT_ACTION_EXIT; /* 旧版吞错必须能结束，不能靠下一处故障伪造通过。 */
    return 1;
}

#define SDL_GetTicks message_ticks
#define fd2_delay_ms message_delay
#define fd2_vga_present message_present
#define fd2_vga_present_timed message_present_timed
#define fd2_vga_wait_frame_deadline message_wait
#define fd2_field_game_open message_open
#define fd2_field_game_close message_close
#define fd2_field_game_execute_storage_action message_storage
#define fd2_input_take_action message_action
#include "field_play.c"
#undef SDL_GetTicks
#undef fd2_delay_ms
#undef fd2_vga_present
#undef fd2_vga_present_timed
#undef fd2_vga_wait_frame_deadline
#undef fd2_field_game_open
#undef fd2_field_game_close
#undef fd2_field_game_execute_storage_action
#undef fd2_input_take_action

static void reset_controls(void) {
    in_route = actions = presents = delay_calls = 0;
    backend_calls = backend_done = closed = owner_ok = backend_rng_ok = 0;
    backend_result = -99;
    mock_ms = 1000;
}

static int test_fragment_preflight_is_pixel_atomic(void) {
    uint8_t tokens[8] = {1,0,0xfe,0xff,0,0,0xff,0xff};
    uint16_t offset = 0;
    fd2_field_game game = {0};
    game.ready = 1;
    game.font = (fd2_font){glyph_bytes, sizeof(glyph_bytes), 2};
    game.ui_text = (fd2_text_entry){tokens, sizeof(tokens), &offset, 1};
    for (int mode = 0; mode < 8; mode++) {
        reset_controls();
        fd2_vga vga = {0};
        memset(vga.framebuffer, 0x77, sizeof(vga.framebuffer));
        uint8_t before[sizeof(vga.framebuffer)];
        memcpy(before, vga.framebuffer, sizeof(before));
        tokens[0] = 1; tokens[2] = 0xfe; tokens[3] = 0xff;
        tokens[6] = tokens[7] = 0xff;
        game.ui_text.fragment_count = 1;
        game.ui_text.size = sizeof(tokens);
        game.font.glyph_count = 2; game.font.size = sizeof(glyph_bytes);
        if (mode == 0) game.ui_text.fragment_count = 0;
        if (mode == 1) tokens[4] = 2; /* 先有合法glyph，再坏glyph；不能画半段。 */
        if (mode == 2) tokens[2] = 0xef; /* 未恢复的control。 */
        if (mode == 3) { tokens[6] = 0; tokens[7] = 0; } /* 无terminator。 */
        if (mode == 4) game.font.size = 32;
        if (mode == 5) game.font.glyph_count = 0;
        if (mode == 6) game.ui_text.size = 7;
        if (mode == 7) game.ui_text.size = 0;
        CHECK(play_system_confirmation_text(&game, &vga, 0, 0) == -1);
        CHECK(memcmp(before, vga.framebuffer, sizeof(before)) == 0);
        CHECK(!presents && !delay_calls);
        tokens[4] = 0;
    }
    reset_controls();
    fd2_vga vga = {0};
    game.ui_text.fragment_count = 1; game.ui_text.size = sizeof(tokens);
    game.font.glyph_count = 2; game.font.size = sizeof(glyph_bytes);
    tokens[2] = 0xfe; tokens[3] = 0xff; tokens[6] = tokens[7] = 0xff;
    CHECK(play_system_confirmation_text(&game, &vga, 0, 0) == 0);
    CHECK(presents == 1 && delay_calls == 1);
    CHECK(vga.framebuffer[112 * VGA_STRIDE + 5] == 0xcd);
    return 0;
}

static int read_bytes(const char *path, uint8_t data[FD2_SAVE_FILE_SIZE]) {
    FILE *file = fopen(path, "rb");
    if (!file) return -1;
    size_t count = fread(data, 1, FD2_SAVE_FILE_SIZE, file);
    int close_result = fclose(file);
    return count == FD2_SAVE_FILE_SIZE && close_result == 0 ? 0 : -1;
}

static int test_formal_errors_preserve_storage_facts(void) {
    fd2_archive archive = {0};
    CHECK(fd2_archive_open(&archive, "original_game/FDOTHER.DAT") == 0);
    for (int kind = ROUTE_PROMPT; kind <= ROUTE_SAVE_FAILURE; kind++) {
        for (int bad = BAD_NONE; bad <= BAD_GLYPH; bad++) {
            reset_controls(); in_route = 1; route_kind = kind; bad_kind = bad;
            CHECK(fd2_test_temp_path(storage_path) == 0);
            fd2_save_file source = {0};
            CHECK(fd2_save_file_open(&source, "original_game/START.SAV") == 0);
            CHECK(fd2_save_file_write(&source, storage_path) == 0);
            fd2_save_file_close(&source);
            if (kind == ROUTE_SAVE_FAILURE) {
                FILE *file = fopen(storage_path, "r+b");
                CHECK(file != NULL && fputc(0, file) != EOF && fclose(file) == 0);
            }
            uint8_t before[FD2_SAVE_FILE_SIZE], after[FD2_SAVE_FILE_SIZE];
            CHECK(read_bytes(storage_path, before) == 0);
            fd2_vga vga = {0};
            fd2_input_init(&vga.input);
            fd2_field_play_result result = fd2_field_play_run(
                &vga, &archive, 0, 0, NULL, NULL, storage_path, 0);
            CHECK(result == (bad == BAD_NONE ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR));
            CHECK(closed == 1 && owner_ok);
            CHECK(backend_calls == (kind >= ROUTE_SAVE ? 1 : 0));
            if (bad != BAD_NONE) CHECK(actions == 1);
            CHECK(read_bytes(storage_path, after) == 0);
            if (kind == ROUTE_SAVE) {
                CHECK(backend_result == 1 && backend_rng_ok);
                CHECK(memcmp(before, after, sizeof(before)) != 0);
                fd2_save_file saved = {0}; fd2_save_battle_snapshot snapshot;
                CHECK(fd2_save_file_open(&saved, storage_path) == 0);
                CHECK(fd2_save_file_get_battle_snapshot(&saved, &snapshot) == 0);
                uint32_t value = (uint32_t)snapshot.meta[10] | (uint32_t)snapshot.meta[11] << 8 |
                    (uint32_t)snapshot.meta[12] << 16 | (uint32_t)snapshot.meta[13] << 24;
                CHECK(value == submitted_resource && resource_at_close == submitted_resource);
                fd2_save_file_close(&saved);
            } else {
                CHECK(memcmp(before, after, sizeof(before)) == 0);
                if (kind == ROUTE_LOAD) CHECK(backend_result == 1 && backend_rng_ok && resource_at_close == original_resource);
                if (kind == ROUTE_SAVE_FAILURE) CHECK(backend_result == 0 && backend_rng_ok);
            }
            CHECK(remove(storage_path) == 0);
        }
    }
    fd2_archive_close(&archive);
    return 0;
}

static int test_real_required_fragments(void) {
    fd2_archive archive = {0}; fd2_field_game game = {0}; fd2_vga vga = {0};
    CHECK(fd2_archive_open(&archive, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_field_game_open(&game, &vga, &archive, 0) == 0);
    for (size_t fragment = 410; fragment <= 420; fragment++) {
        const uint8_t *tokens; size_t count;
        CHECK(system_confirmation_fragment(&game, fragment, &tokens, &count) == 0);
        CHECK(count != 0);
    }
    fd2_field_game_close(&game); fd2_archive_close(&archive);
    return 0;
}

int main(void) {
    CHECK(test_fragment_preflight_is_pixel_atomic() == 0);
    CHECK(test_formal_errors_preserve_storage_facts() == 0);
    CHECK(test_real_required_fragments() == 0);
    puts("field system message tests: ok");
    return 0;
}
