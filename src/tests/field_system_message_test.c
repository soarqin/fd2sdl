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

/* 原17025/1707a push机器码独立期望，不引用生产renderer坐标常量。 */
#define NOTICE_PROMPT_OFFSET (0xA9F23u - 0xA0000u)
#define NOTICE_RESULT_OFFSET (0xAB6E3u - 0xA0000u)
_Static_assert(NOTICE_PROMPT_OFFSET == 127u * VGA_STRIDE + 99u, "raw prompt address");
_Static_assert(NOTICE_RESULT_OFFSET == 146u * VGA_STRIDE + 99u, "raw result address");

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

enum { ROUTE_PROMPT, ROUTE_ESCAPE, ROUTE_NO, ROUTE_SAVE, ROUTE_LOAD, ROUTE_SAVE_FAILURE };
enum { BAD_NONE, BAD_MISSING, BAD_GLYPH };
static int route_kind, bad_kind, in_route, actions, presents, delay_calls;
static int backend_calls, backend_done, backend_result, closed;
static int choice_active,choice_frames;
static int owner_ok, backend_rng_ok;
static uint64_t mock_ms;
static const uint8_t *expected_present_frame;
static unsigned unexpected_present_frames;
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
    if (expected_present_frame && memcmp(vga->framebuffer, expected_present_frame, VGA_W * VGA_H) != 0) {
        unsigned count = 0; size_t first = 0;
        for (size_t i = 0; i < VGA_W * VGA_H; i++) if (vga->framebuffer[i] != expected_present_frame[i]) {
            if (!count) first = i; count++;
        }
        fprintf(stderr, "present mismatch count=%u first=(%zu,%zu) actual=%u expected=%u\n",
            count, first % VGA_W, first / VGA_W, vga->framebuffer[first], expected_present_frame[first]);
        unexpected_present_frames++;
    }
    mock_ms += 16;
    if (choice_active) {
        choice_frames++;
        fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,0);
        fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RIGHT,0);
        fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_ESCAPE,0);
        if (choice_frames==4) fd2_input_set_test_key_state(&vga->input,
            route_kind==ROUTE_ESCAPE?SDL_SCANCODE_ESCAPE:
            route_kind==ROUTE_NO || route_kind==ROUTE_PROMPT?SDL_SCANCODE_RIGHT:SDL_SCANCODE_RETURN,1);
        if (choice_frames==5 && (route_kind==ROUTE_NO || route_kind==ROUTE_PROMPT))
            fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_RETURN,1);
        fd2_input_begin_test_frame(&vga->input,mock_ms);
    }
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
    game->ui_text.fragment_count = 421;
    game->ui_text.offsets = calloc(421, sizeof(*game->ui_text.offsets));
    if (!game->ui_text.offsets) return -1;
    for (size_t i = 0; i < 421; i++) game->ui_text.offsets[i] = (uint16_t)(i * 4u);
    /* 只使目标片段无效；截短fragment_count会把更高编号prompt一并删掉，
     * 无法证明412取消反馈在19953返回后的错误门。 */
    if (bad == BAD_MISSING) game->ui_text.offsets[target]=(uint16_t)sizeof(ui_bytes);
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
    game->system_menu.selected=3;
    if (route_kind==ROUTE_SAVE || route_kind==ROUTE_LOAD || route_kind==ROUTE_SAVE_FAILURE) {
        fd2_field_system_menu_open_secondary(&game->system_menu);
        game->system_menu.selected=route_kind==ROUTE_LOAD?2:1;
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
        ? FD2_INPUT_ACTION_CONFIRM /* Escape由真实19953 raw输入注入，先打开父菜单selector。 */
        : FD2_INPUT_ACTION_EXIT; /* 旧版吞错必须能结束，不能靠下一处故障伪造通过。 */
    return 1;
}

static int message_refresh(fd2_field_game *game,fd2_vga *vga,int phase_only) {
    (void)game;(void)vga;return phase_only>=0 && phase_only<=3?0:-1;
}
static int message_choice(fd2_field_choice_step_fn apply,void *userdata) {
    choice_active=1;choice_frames=0;
    int result=fd2_field_choice_run(apply,userdata);
    choice_active=0;return result;
}
#define fd2_field_game_runtime_refresh message_refresh
#define fd2_field_choice_run message_choice
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
#undef fd2_field_game_runtime_refresh
#undef fd2_field_choice_run
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
    choice_active=choice_frames=0;
    backend_calls = backend_done = closed = owner_ok = backend_rng_ok = 0;
    backend_result = -99;
    mock_ms = 1000;
    expected_present_frame = NULL; unexpected_present_frames = 0;
}

static int test_fragment_preflight_is_pixel_atomic(void) {
    uint8_t tokens[8] = {1,0,0xfe,0xff,0,0,0xff,0xff};
    uint16_t offsets[421] = {0};
    offsets[420] = sizeof(tokens);
    fd2_field_game game = {0};
    field_progress_box box={0};
    game.ready = 1;
    game.font = (fd2_font){glyph_bytes, sizeof(glyph_bytes), 2};
    game.ui_text = (fd2_text_entry){tokens, sizeof(tokens), offsets, 421};
    for (int mode = 0; mode < 8; mode++) {
        reset_controls();
        fd2_vga vga = {0};
        memset(vga.framebuffer, 0x77, sizeof(vga.framebuffer));
        uint8_t before[sizeof(vga.framebuffer)];
        memcpy(before, vga.framebuffer, sizeof(before));
        tokens[0] = 1; tokens[2] = 0xfe; tokens[3] = 0xff;
        tokens[6] = tokens[7] = 0xff;
        game.ui_text.fragment_count = 421;
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
        if (mode == 1) vga.input.quit_requested=1;
        CHECK(play_system_confirmation_text(&game, &vga, &box, NULL, 419, 0) == -1);
        CHECK(memcmp(before, vga.framebuffer, sizeof(before)) == 0);
        CHECK(!presents && !delay_calls);
        if (mode == 1) CHECK(vga.input.quit_requested); /* 独立坏字形不能靠quit洗成HOST。 */
        tokens[4] = 0;
    }
    reset_controls();
    fd2_vga vga = {0};
    game.ui_text.fragment_count = 421; game.ui_text.size = sizeof(tokens);
    game.font.glyph_count = 2; game.font.size = sizeof(glyph_bytes);
    tokens[2] = 0xfe; tokens[3] = 0xff; tokens[6] = tokens[7] = 0xff;
    fd2_archive fdother = {0};
    CHECK(fd2_archive_open(&fdother, "original_game/FDOTHER.DAT") == 0);
    game.fdother = &fdother;
    CHECK(fd2_archive_open(&game.dato_archive, "original_game/DATO.DAT") == 0);
    game.system_menu.confirmation_prompt_fragment = 419;
    memset(vga.framebuffer, 0x77, sizeof(vga.framebuffer));
    for (size_t i = 0; i < sizeof(vga.palette); i++) {
        vga.palette[i] = (uint8_t)(i % 64u);
        vga.dac[i] = (uint8_t)(i % 256u);
    }
    uint8_t original_palette[VGA_PALETTE_SIZE], original_dac[VGA_PALETTE_SIZE];
    memcpy(original_palette, vga.palette, sizeof(original_palette));
    memcpy(original_dac, vga.dac, sizeof(original_dac));
    fd2_vga *expected = calloc(1, sizeof(*expected));
    fd2_field_game *game_before = malloc(sizeof(*game_before));
    CHECK(expected && game_before);
    memcpy(game_before, &game, sizeof(game));
    memcpy(expected->framebuffer, vga.framebuffer, sizeof(vga.framebuffer));
    CHECK(fd2_scene_field_message_box(expected, &fdother) == 0);
    fd2_image portrait = {0};
    CHECK(fd2_portrait_decode_frame(&portrait, &game.dato_archive, 75, 0) == 0);
    CHECK(portrait.width == 80 && portrait.height == 80);
    for (int row = 0; row < 80; row++) for (int col = 0; col < 80; col++)
        expected->framebuffer[(115 + row) * VGA_STRIDE + 87 - col] = portrait.pixels[row * 80 + col];
    fd2_image_free(&portrait);
    fd2_font_draw_glyph(expected, &game.font, 1, NOTICE_PROMPT_OFFSET % VGA_STRIDE, NOTICE_PROMPT_OFFSET / VGA_STRIDE, 0xcd, 0x4c, 0x4a);
    fd2_font_draw_glyph(expected, &game.font, 0, NOTICE_RESULT_OFFSET % VGA_STRIDE, NOTICE_RESULT_OFFSET / VGA_STRIDE, 0xcd, 0x4c, 0x4a);
    /* 必须核首个真正present的整页，不能等下一轮稳态render掩盖亮色占位。 */
    expected_present_frame = NULL;
    CHECK(play_system_confirmation_text(&game, &vga, &box, NULL, 419, 0) == 0);
    CHECK(presents == 8 && delay_calls == 2 && unexpected_present_frames == 0);
    CHECK(box.page && box.portraits[0].pixels);progress_box_free(&box);
    CHECK(memcmp(vga.framebuffer, expected->framebuffer, sizeof(vga.framebuffer)) == 0);
    CHECK(memcmp(vga.palette, original_palette, sizeof(original_palette)) == 0 &&
          memcmp(vga.dac, original_dac, sizeof(original_dac)) == 0);
    CHECK(memcmp(&game, game_before, sizeof(game)) == 0);
    expected_present_frame = NULL;
    for (int invalid = 0; invalid < 2; invalid++) {
        fd2_archive empty = {0};
        reset_controls();
        game.fdother = invalid ? &empty : NULL;
        memset(vga.framebuffer, 0x77, sizeof(vga.framebuffer));
        memset(expected->framebuffer, 0x77, sizeof(expected->framebuffer));
        CHECK(play_system_confirmation_text(&game, &vga, &box, NULL, 419, 0) == -1);
        CHECK(memcmp(vga.framebuffer, expected->framebuffer, sizeof(vga.framebuffer)) == 0);
        CHECK(!presents && !delay_calls);
    }
    /* 最后一张UI图损坏也须在落任何像素之前失败。只改本测试heap副本。 */
    const uint8_t *sheet; size_t sheet_size;
    CHECK(fd2_archive_get(&fdother, 5, &sheet, &sheet_size) == 0 && sheet_size > 86);
    fd2_archive broken = fdother;
    broken.data = malloc(fdother.size); CHECK(broken.data);
    memcpy(broken.data, fdother.data, fdother.size);
    const uint8_t *slot = sheet + 6 + 19 * 4;
    size_t frame = (size_t)slot[0] | (size_t)slot[1] << 8 | (size_t)slot[2] << 16 | (size_t)slot[3] << 24;
    CHECK(frame + 4 <= sheet_size);
    size_t absolute = (size_t)(sheet - fdother.data) + frame;
    broken.data[absolute] = broken.data[absolute + 1] = 0;
    game.fdother = &broken;
    reset_controls();
    CHECK(play_system_confirmation_text(&game, &vga, &box, NULL, 419, 0) == -1 && !presents && !delay_calls);
    CHECK(memcmp(vga.framebuffer, expected->framebuffer, sizeof(vga.framebuffer)) == 0);
    free(broken.data); game.fdother = &fdother;
    size_t original_portraits = game.dato_archive.count;
    game.dato_archive.count = 75; /* END仍须原75，不能改用Hero0回退。 */
    CHECK(play_system_confirmation_text(&game, &vga, &box, NULL, 419, 0) == -1 && !presents && !delay_calls);
    CHECK(memcmp(vga.framebuffer, expected->framebuffer, sizeof(vga.framebuffer)) == 0);
    game.dato_archive.count = original_portraits;
    free(game_before); free(expected);
    fd2_archive_close(&game.dato_archive);
    fd2_archive_close(&fdother);
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
            vga.field_page=fd2_field_page_create();CHECK(vga.field_page);
            fd2_input_init(&vga.input);
            fd2_field_play_result result = fd2_field_play_run(
                &vga, &archive, 0, 0, NULL, NULL, storage_path, 0);
            fprintf(stderr,"system formal kind=%d bad=%d result=%d backend=%d/%d actions=%d frames=%d closed=%d owner=%d resource=%u\n",
                kind,bad,(int)result,backend_calls,backend_result,actions,presents,closed,owner_ok,resource_at_close);
            CHECK(result == (bad == BAD_NONE ? FD2_FIELD_PLAY_RETURN_HOST_QUIT : FD2_FIELD_PLAY_RETURN_ERROR));
            CHECK(closed == 1 && owner_ok);
            int executed=kind>=ROUTE_SAVE && !(kind==ROUTE_LOAD && bad!=BAD_NONE);
            CHECK(backend_calls==executed);
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
                if (kind == ROUTE_LOAD) CHECK(bad==BAD_NONE
                    ?backend_result==1 && backend_rng_ok && resource_at_close==original_resource
                    :backend_calls==0 && resource_at_close==submitted_resource);
                if (kind == ROUTE_SAVE_FAILURE) CHECK(backend_result == 0 && backend_rng_ok);
            }
            fd2_field_page_destroy(vga.field_page);
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
    game.system_menu.confirmation_prompt_fragment = 419;
    for (size_t fragment = 410; fragment <= 420; fragment++)
        CHECK(fd2_field_game_render_system_notice(&game, &vga, fragment) == 0);
    fd2_field_game_close(&game); fd2_archive_close(&archive);
    return 0;
}

static uint32_t notice_rng(void *unused) { (void)unused; backend_rng_ok = 0; return 0; }

static int expected_notice(fd2_vga *vga, fd2_field_game *game, unsigned portrait,
        size_t prompt, size_t result) {
    CHECK(fd2_scene_field_message_box(vga, game->fdother) == 0);
    fd2_image image = {0};
    CHECK(fd2_portrait_decode_frame(&image, &game->dato_archive, portrait, 0) == 0 && image.width == 80 && image.height == 80);
    for (int y = 0; y < 80; y++) for (int x = 0; x < 80; x++)
        vga->framebuffer[(115 + y) * VGA_STRIDE + 87 - x] = image.pixels[y * 80 + x];
    fd2_image_free(&image);
    size_t fragments[2] = {prompt, result};
    for (unsigned line = 0; line < (result ? 2u : 1u); line++) {
        const uint8_t *tokens; size_t count;
        CHECK(fd2_text_entry_get_fragment(&game->ui_text, fragments[line], &tokens, &count) == 0);
        size_t destination = line ? NOTICE_RESULT_OFFSET : NOTICE_PROMPT_OFFSET;
        int x = (int)(destination % VGA_STRIDE), y = (int)(destination / VGA_STRIDE);
        for (size_t i = 0; i < count; i++) {
            uint16_t t = fd2_text_token_at(tokens, i); if (t == 0xffffu) break;
            if (t == 0xfffeu) { x = (int)(destination % VGA_STRIDE); y += 19; continue; }
            CHECK(t < game->font.glyph_count);
            fd2_font_draw_glyph(vga, &game->font, t, x, y, 0xcd, 0x4c, 0x4a); x += 16;
        }
    }
    return 0;
}

static int test_notice_resource_owner_and_stable_page(void) {
    fd2_archive archive={0};
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));
    fd2_vga *vga=calloc(1,sizeof(*vga)),*expected=calloc(1,sizeof(*expected));
    CHECK(game && before && vga && expected);
    CHECK(fd2_archive_open(&archive,"original_game/FDOTHER.DAT")==0 && fd2_field_game_open(game,vga,&archive,0)==0);
    game->attack_rng=notice_rng;game->attack_rng_userdata=NULL;
    const size_t prompts[]={410,413,415,417,419};
    for (unsigned i=0;i<5;i++) for (unsigned result_kind=0;result_kind<2;result_kind++) {
        field_progress_box box={0};size_t prompt=prompts[i];
        game->system_menu.confirmation_prompt_fragment=(uint16_t)prompt;
        unsigned portrait=prompt==417?game->units.items[0].unit_id:75;
        memset(vga->framebuffer,0x77,sizeof(vga->framebuffer));
        memcpy(expected->framebuffer,vga->framebuffer,sizeof(vga->framebuffer));
        CHECK(expected_notice(expected,game,portrait,prompt,0)==0);
        memcpy(before,game,sizeof(*before));reset_controls();backend_rng_ok=1;
        CHECK(play_system_confirmation_text(game,vga,&box,NULL,prompt,0)==0);
        CHECK(presents>6 && delay_calls>0 && box.page && backend_rng_ok);
        CHECK(!memcmp(expected->framebuffer,vga->framebuffer,sizeof(vga->framebuffer)));
        CHECK(!memcmp(before,game,sizeof(*before)));
        unsigned frames_before=(unsigned)presents,delays_before=(unsigned)delay_calls;
        size_t result=result_kind?412u:prompt+1u;
        CHECK(play_system_confirmation_text(game,vga,&box,NULL,result,0)==0);
        CHECK(!box.page && (unsigned)presents>frames_before+5u && (unsigned)delay_calls>delays_before && backend_rng_ok);
        for (size_t p=0;p<sizeof(vga->framebuffer);p++) CHECK(vga->framebuffer[p]==0x77);
        CHECK(!memcmp(before,game,sizeof(*before)));
    }
    game->units.count=0;
    CHECK(fd2_field_game_render_system_notice(game,vga,419)==0);
    memcpy(expected->framebuffer,vga->framebuffer,sizeof(vga->framebuffer));
    CHECK(fd2_field_game_render_system_notice(game,vga,417)==-1 &&
        !memcmp(expected->framebuffer,vga->framebuffer,sizeof(vga->framebuffer)));
    fd2_field_game_close(game);fd2_archive_close(&archive);
    free(game);free(before);free(vga);free(expected);return 0;
}

int main(void) {
    CHECK(test_notice_resource_owner_and_stable_page() == 0);
    CHECK(test_fragment_preflight_is_pixel_atomic() == 0);
    CHECK(test_formal_errors_preserve_storage_facts() == 0);
    CHECK(test_real_required_fragments() == 0);
    puts("field system message tests: ok");
    return 0;
}
