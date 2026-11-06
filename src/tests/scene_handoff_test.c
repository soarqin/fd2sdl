/* new_game_opening_play @0x3231b：真实 32→31→0 资源的无窗口交接回归。
 * 私有 TU 仅替换呈现/等待和故障注入，不增加生产测试入口。
 * 终态来自 FDFIELD placement 与原版 script 0/1/2/5 的写集，不读取 START.SAV。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scene.h"
#include "portrait.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

static uint64_t test_ms;
static int presents, quit_at_frame, quit_on_preview_delay;
static int archive_opens, archive_closes;
static const char *fail_archive_path;
static const fd2_archive *fail_palette_archive;
static int palette_failure, palette_queries;
static int portrait_frame_override;
static size_t portrait_frame_requested;
static int portrait_decode_calls, fail_portrait_decode_at;
static int presents_at_portrait_failure;
static size_t failed_portrait_identity;
static fd2_vga *current_vga;
static int trace_fragments;
static size_t fragment_log[32], fragment_log_count, last_fragment;
static const fd2_image *tracked_cursor;
static unsigned cursor_by_fragment[16];

static int scene_test_get_fragment(const fd2_text_entry *text, size_t fragment,
        const uint8_t **tokens, size_t *count) {
    if (trace_fragments) {
        if (fragment_log_count < 32) fragment_log[fragment_log_count++] = fragment;
        last_fragment = fragment;
    }
    return fd2_text_entry_get_fragment(text, fragment, tokens, count);
}
static void scene_test_sprite_blit(fd2_vga *vga, const fd2_image *image,
        int x, int y, int transparent) {
    if (image == tracked_cursor && last_fragment < 16) cursor_by_fragment[last_fragment]++;
    fd2_map_sprite_blit(vga, image, x, y, transparent);
}


static int scene_test_archive_open(fd2_archive *archive, const char *path) {
    if (fail_archive_path && strcmp(path, fail_archive_path) == 0)
        return -1;
    int result = fd2_archive_open(archive, path);
    if (result == 0) archive_opens++;
    return result;
}

static void scene_test_archive_close(fd2_archive *archive) {
    if (archive && archive->data) archive_closes++;
    fd2_archive_close(archive);
}

static int scene_test_archive_get(const fd2_archive *archive, size_t index,
                                  const uint8_t **data, size_t *size) {
    if (archive == fail_palette_archive && index == 0) {
        palette_queries++;
        if (palette_failure == 1) {
            *data = NULL;
            *size = 0;
            return -1;
        }
        int result = fd2_archive_get(archive, index, data, size);
        if (result == 0 && palette_failure == 2) *size = 767;
        return result;
    }
    return fd2_archive_get(archive, index, data, size);
}

static int scene_test_portrait_decode_frame(fd2_image *image,
                                             const fd2_archive *archive,
                                             size_t identity, size_t frame) {
    portrait_frame_requested = frame;
    portrait_decode_calls++;
    if (portrait_decode_calls == fail_portrait_decode_at) {
        presents_at_portrait_failure = presents;
        failed_portrait_identity = identity;
        return -1;
    }
    return fd2_portrait_decode_frame(image, archive, identity,
        portrait_frame_override >= 0 ? (size_t)portrait_frame_override : frame);
}

static void scene_test_present(fd2_vga *vga) {
    presents++;
    test_ms += 17;
    /* 非 fast 退出用例自动确认所有对白；不创建 SDL 窗口或注入宿主键。 */
    fd2_input_set_test_key_state(&vga->input, SDL_SCANCODE_RETURN,
                                presents % 2);
    fd2_input_begin_test_frame(&vga->input, test_ms);
    if (presents == quit_at_frame || presents > 100000)
        vga->input.quit_requested = 1;
}

static void scene_test_present_timed(fd2_vga *vga, uint32_t ms) {
    test_ms += ms;
    scene_test_present(vga);
}

static void scene_test_delay(uint32_t ms) {
    test_ms += ms;
    if (quit_on_preview_delay && ms == 800 && current_vga)
        current_vga->input.quit_requested = 1;
}

static uint64_t scene_test_ticks(void) { return test_ms; }

/* core 中仍有正式 scene.o；本 TU 的公开入口私有改名，避免重复定义。 */
#define fd2_scene_play_new_game_prologue_handoff scene_test_play_handoff
#define fd2_scene_play_new_game_prologue scene_test_play_prologue
#define fd2_scene_play_field_event scene_test_play_event
#define fd2_scene_play_stage1_entry scene_test_stage1_entry
#define fd2_scene_play_stage2_entry scene_test_stage2_entry
#define fd2_scene_play_stage1_victory scene_test_stage1_victory
#define fd2_scene_play_field_story scene_test_field_story
#define fd2_scene_field_story_validate scene_test_story_validate
#define fd2_scene_play_game_over scene_test_game_over
#define fd2_scene_field_message_validate scene_test_message_validate
#define fd2_scene_field_message_box scene_test_message_box
#define fd2_scene_field_message_arrow scene_test_message_arrow
#define fd2_archive_open scene_test_archive_open
#define fd2_archive_close scene_test_archive_close
#define fd2_archive_get scene_test_archive_get
#define fd2_portrait_decode_frame scene_test_portrait_decode_frame
#define fd2_text_entry_get_fragment scene_test_get_fragment
#define fd2_map_sprite_blit scene_test_sprite_blit
#define fd2_vga_present scene_test_present
#define fd2_vga_present_timed scene_test_present_timed
#define fd2_delay_ms scene_test_delay
#define SDL_GetTicks scene_test_ticks
#include "../scene.c"
#undef fd2_scene_play_new_game_prologue_handoff
#undef fd2_scene_play_new_game_prologue
#undef fd2_scene_play_field_event
#undef fd2_scene_play_stage1_entry
#undef fd2_scene_play_stage2_entry
#undef fd2_scene_play_stage1_victory
#undef fd2_scene_play_field_story
#undef fd2_scene_field_story_validate
#undef fd2_scene_play_game_over
#undef fd2_scene_field_message_validate
#undef fd2_scene_field_message_box
#undef fd2_scene_field_message_arrow
#undef fd2_archive_open
#undef fd2_archive_close
#undef fd2_archive_get
#undef fd2_portrait_decode_frame
#undef fd2_text_entry_get_fragment
#undef fd2_map_sprite_blit
#undef fd2_vga_present
#undef fd2_vga_present_timed
#undef fd2_delay_ms
#undef SDL_GetTicks

static void reset_controls(fd2_vga *vga) {
    memset(vga, 0, sizeof(*vga));
    fd2_input_init(&vga->input);
    test_ms = 1000;
    presents = quit_at_frame = quit_on_preview_delay = 0;
    archive_opens = archive_closes = 0;
    fail_archive_path = NULL;
    fail_palette_archive = NULL;
    palette_failure = palette_queries = 0;
    portrait_frame_override = -1;
    portrait_frame_requested = (size_t)-1;
    portrait_decode_calls = fail_portrait_decode_at = 0;
    presents_at_portrait_failure = -1;
    failed_portrait_identity = (size_t)-1;
    trace_fragments = 0; fragment_log_count = 0; last_fragment = 0;
    tracked_cursor = NULL; memset(cursor_by_fragment, 0, sizeof(cursor_by_fragment));
    current_vga = vga;
}

static int test_natural_stage0_handoff(const fd2_archive *fdother,
                                      int *final_present) {
    static const uint8_t expected[][4] = {
        /* unit_id, x, y, flags.hidden；全部方向应为 0。 */
        {0, 7, 14, 0}, {9, 10, 15, 0}, {4, 8, 16, 0}, {30, 11, 17, 0},
        {96, 1, 4, 0}, {96, 2, 2, 0}, {96, 4, 2, 0}, {96, 6, 1, 0},
        {96, 3, 18, 0}, {96, 4, 23, 1}, {96, 2, 18, 0}, {96, 5, 19, 0}
    };
    fd2_vga vga;
    fd2_field_handoff handoff;
    reset_controls(&vga);
    memset(&handoff, 0xa5, sizeof(handoff));
    CHECK(scene_test_play_handoff(&vga, fdother, NULL, NULL, 1,
                                  &handoff) == FD2_SCENE_RESULT_OK);
    CHECK(handoff.valid && handoff.stage == 0);
    CHECK(handoff.units.count == sizeof(expected) / sizeof(expected[0]));
    for (size_t i = 0; i < handoff.units.count; i++) {
        const fd2_field_unit *unit = &handoff.units.items[i];
        CHECK(unit->unit_id == expected[i][0]);
        CHECK(unit->x == expected[i][1] && unit->y == expected[i][2]);
        CHECK(unit->direction == 0 && unit->frame_phase == 0);
        CHECK(fd2_field_unit_is_hidden(unit) == expected[i][3]);
        CHECK(handoff.units.walk_frames[i] == 1);
    }
    /* TEXT2 最后说话人是 raw11(5,19)；原版收尾还须移到 raw0。
     * 保留逐格边缘卷动历史，不能改成按 actor0 直接居中。 */
    CHECK(handoff.focus_cell_x == 7 && handoff.focus_cell_y == 14);
    CHECK(handoff.camera_cell_x == 0 && handoff.camera_cell_y == 13);
    CHECK(archive_opens == 4 && archive_closes == archive_opens);
    CHECK(presents > 3);
    *final_present = presents;
    return 0;
}

static int test_resource_error_has_no_handoff(const fd2_archive *fdother) {
    fd2_vga vga;
    fd2_field_handoff handoff;
    reset_controls(&vga);
    fail_archive_path = "original_game/FDTXT.DAT";
    memset(&handoff, 0xa5, sizeof(handoff));
    CHECK(scene_test_play_handoff(&vga, fdother, NULL, NULL, 1,
                                  &handoff) == FD2_SCENE_RESULT_ERROR);
    CHECK(!handoff.valid && handoff.units.count == 0);
    CHECK(archive_opens == 2 && archive_closes == archive_opens);
    CHECK(presents == 0);
    /* 字体/UI 无资源时也不能交接；使用真实空 archive 的正式错误出口。 */
    fd2_archive empty = {0};
    reset_controls(&vga);
    memset(&handoff, 0xa5, sizeof(handoff));
    CHECK(scene_test_play_handoff(&vga, &empty, NULL, NULL, 1,
                                  &handoff) == FD2_SCENE_RESULT_ERROR);
    CHECK(!handoff.valid && handoff.units.count == 0);
    CHECK(archive_opens == 4 && archive_closes == archive_opens);
    return 0;
}

static int test_host_quit_has_no_handoff(const fd2_archive *fdother,
                                        int final_present) {
    fd2_vga vga;
    fd2_field_handoff handoff;
    const int frames[] = {1, final_present};
    for (size_t i = 0; i < sizeof(frames) / sizeof(frames[0]); i++) {
        reset_controls(&vga);
        quit_at_frame = frames[i];
        memset(&handoff, 0xa5, sizeof(handoff));
        CHECK(scene_test_play_handoff(&vga, fdother, NULL, NULL, 1,
                                      &handoff) == FD2_SCENE_RESULT_HOST_QUIT);
        CHECK(!handoff.valid && handoff.units.count == 0);
        CHECK(archive_opens == 4 && archive_closes == archive_opens);
    }
    /* 最后 800 ms 预览也属于可退出流程，不能先导出 valid 再返回 quit。 */
    reset_controls(&vga);
    quit_on_preview_delay = 1;
    memset(&handoff, 0xa5, sizeof(handoff));
    CHECK(scene_test_play_handoff(&vga, fdother, NULL, NULL, 0,
                                  &handoff) == FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(!handoff.valid && handoff.units.count == 0);
    CHECK(archive_opens == 4 && archive_closes == archive_opens);
    CHECK(presents < 100000);
    return 0;
}

static int test_required_palette_error_has_no_handoff(const fd2_archive *fdother) {
    /* 只故障 FDOTHER[0]：字体/UI/FDICON 均真实有效，不能由更早的错误假通过。 */
    for (int failure = 1; failure <= 2; failure++) {
        fd2_vga vga;
        fd2_field_handoff handoff;
        reset_controls(&vga);
        fail_palette_archive = fdother;
        palette_failure = failure;
        memset(&handoff, 0xa5, sizeof(handoff));
        CHECK(scene_test_play_handoff(&vga, fdother, NULL, NULL, 1,
                                      &handoff) == FD2_SCENE_RESULT_ERROR);
        CHECK(palette_queries == 1);
        CHECK(!handoff.valid && handoff.units.count == 0);
        CHECK(presents == 0);
        CHECK(archive_opens == 4 && archive_closes == archive_opens);
    }
    return 0;
}

static int test_real_portrait_orientation(void) {
    static const size_t identities[] = {0, 4, 9, 30, 48, 66, 96};
    static const int frames[] = {0, 3};
    fd2_archive dato = {0};
    CHECK(fd2_archive_open(&dato, "original_game/DATO.DAT") == 0);
    for (size_t id = 0; id < sizeof(identities) / sizeof(identities[0]); id++) {
        for (size_t phase = 0; phase < sizeof(frames) / sizeof(frames[0]); phase++) {
            fd2_image original = {0};
            CHECK(fd2_portrait_decode_frame(&original, &dato, identities[id],
                                            (size_t)frames[phase]) == 0);
            CHECK(original.width == 80 && original.height == 80);
            for (int area = FD2_DIALOG_TOP; area <= FD2_DIALOG_BOTTOM; area++) {
                fd2_vga vga;
                reset_controls(&vga);
                memset(vga.framebuffer, 0xa5, sizeof(vga.framebuffer));
                /* 正式scene目前固定请求frame0；替身喂真实0/3，验证共同方向路径，
                 * 不声称实现了native眨眼计时或RNG。 */
                portrait_frame_override = frames[phase];
                CHECK(draw_portrait(&vga, &dato, (fd2_dialog_area)area,
                                    (uint16_t)identities[id]) == 0);
                CHECK(portrait_frame_requested == 0 && presents == 0);
                int x = area == FD2_DIALOG_TOP ? 232 : 8;
                int y = area == FD2_DIALOG_TOP ? 5 : 115;
                size_t asymmetric = 0;
                for (int row = 0; row < VGA_H; row++) {
                    for (int column = 0; column < VGA_W; column++) {
                        uint8_t expected = 0xa5;
                        if (column >= x && column < x + 80 &&
                            row >= y && row < y + 80) {
                            int sx = column - x;
                            if (area == FD2_DIALOG_BOTTOM) sx = 79 - sx;
                            size_t index = (size_t)(row - y) * 80u + (size_t)sx;
                            expected = original.pixels[index];
                            if (expected != original.pixels[
                                    (size_t)(row - y) * 80u + (size_t)(79 - sx)])
                                asymmetric++;
                        }
                        CHECK(vga.framebuffer[(size_t)row * VGA_STRIDE + (size_t)column] == expected);
                    }
                }
                CHECK(asymmetric > 0);
            }
            fd2_image_free(&original);
        }
    }
    fd2_archive_close(&dato);
    return 0;
}

static int test_portrait_decode_error_has_no_handoff(const fd2_archive *fdother) {
    /* 真正运行开场，只有指定次立绘解码返回错误；其他资源全部真实有效。
     * 首次bottom ID0和第二次top ID48分别覆盖打开／切换说话人的整条传播链。 */
    static const size_t expected_ids[] = {0, 48};
    for (int failure = 1; failure <= 2; failure++) {
        fd2_vga vga;
        fd2_field_handoff handoff;
        reset_controls(&vga);
        fail_portrait_decode_at = failure;
        memset(&handoff, 0xa5, sizeof(handoff));
        CHECK(scene_test_play_handoff(&vga, fdother, NULL, NULL, 1,
                                      &handoff) == FD2_SCENE_RESULT_ERROR);
        CHECK(portrait_decode_calls == failure);
        CHECK(failed_portrait_identity == expected_ids[failure - 1]);
        CHECK(portrait_frame_requested == 0);
        CHECK(!scene_host_quit_requested);
        CHECK(!handoff.valid && handoff.units.count == 0);
        CHECK(presents_at_portrait_failure > 0 && presents == presents_at_portrait_failure);
        CHECK(archive_opens == 4 && archive_closes == archive_opens);
    }
    return 0;
}

static int test_stage1_presentation(const fd2_archive *fdother) {
    fd2_save_manual_state prepared={0};
    prepared.stage_id=1;prepared.unit_count=5;prepared.gold_or_flags=1234;
    const uint8_t ids[]={0,9,4,30,1};
    for(size_t i=0;i<5;i++) {
        fd2_field_unit actor={0};
        CHECK(fd2_field_unit_persistent_create(&actor,ids[i])==0);
        memcpy(prepared.unit_area+i*80,&actor,80);
    }
    fd2_field_game *game=calloc(1,sizeof(*game));CHECK(game);
    CHECK(fd2_field_game_prepare_stage1(game,fdother,&prepared)==0);
    fd2_field_units expected=game->units;
    fd2_vga vga;reset_controls(&vga);
    CHECK(scene_test_stage1_entry(&vga,fdother,game,NULL,1)==0);
    CHECK(game->units.count==21 && game->turn_number==1 && game->active_side==2);
    CHECK(game->cursor_cell_x==20 && game->cursor_cell_y==14);
    /* B5 C9原版25页后首leader WGC(268,28)，合法Save反推camera9,13；
     * 不沿用业务prepare的近似镜头或最后一次PAN6,12。 */
    CHECK(game->camera_cell_x==9 && game->camera_cell_y==13);
    for(size_t i=0;i<21;i++)CHECK(!memcmp(&game->units.items[i],&expected.items[i],80));
    CHECK(!memcmp(game->roster,prepared.unit_area,sizeof(game->roster)));
    CHECK(archive_opens==archive_closes);
    CHECK(scene_test_story_validate(game,3)==0);
    CHECK(scene_test_story_validate(game,65535)==-1);
    fd2_field_game_close(game);
    prepared.unit_count=4;
    CHECK(fd2_field_game_prepare_stage1(game,fdother,&prepared)==0);
    expected=game->units;
    reset_controls(&vga);
    CHECK(scene_test_stage1_entry(&vga,fdother,game,NULL,1)==0);
    uint8_t absent[80]={0};absent[0]=254;absent[5]=1;
    CHECK(!memcmp(&game->units.items[4],absent,80));
    for(size_t i=0;i<21;i++)CHECK(!memcmp(&game->units.items[i],&expected.items[i],80));
    /* 中途退出不交还半套脚本坐标；属性与持久owner也不被呈现覆盖。 */
    reset_controls(&vga);quit_at_frame=3;
    fd2_field_units before=game->units;
    CHECK(scene_test_stage1_entry(&vga,fdother,game,NULL,0)==FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(!memcmp(&before,&game->units,sizeof(before)) && archive_opens==archive_closes);
    fd2_field_game_close(game);free(game);
    return 0;
}

static int make_stage1_victory(fd2_field_game *game, fd2_vga *vga,
        const fd2_archive *fdother, int missing_villager, int full) {
    fd2_save_manual_state prepared={0};prepared.stage_id=1;prepared.unit_count=5;
    const uint8_t ids[]={0,9,4,30,1};
    for(size_t i=0;i<5;i++) {
        fd2_field_unit unit={0};CHECK(fd2_field_unit_persistent_create(&unit,ids[i])==0);
        memcpy(prepared.unit_area+i*80,&unit,80);
    }
    CHECK(fd2_field_game_prepare_stage1(game,fdother,&prepared)==0);
    game->turn_number=3;CHECK(fd2_field_game_end_active_phase(game)==0);
    CHECK(game->event_log_count==1 && game->units.count==27);
    reset_controls(vga);
    CHECK(scene_test_play_event(vga,fdother,game,&game->event_log[0],NULL,1)==0);
    CHECK(fd2_field_game_progress_resume(game)==0);
    for(size_t i=0;i<game->units.count;i++) {
        if(game->units.items[i].side==0) {game->units.items[i].flags|=1;game->units.items[i].hp=0;}
        if(full && game->units.items[i].side==2) memset((uint8_t*)&game->units.items[i]+10,0,16);
    }
    if(missing_villager)game->units.items[7].flags|=1;
    CHECK(fd2_field_game_battle_result(game)==FD2_FIELD_BATTLE_VICTORY);
    return 0;
}

static int test_stage1_victory_projection(const fd2_archive *fdother) {
    fd2_field_game *game=calloc(1,sizeof(*game)), *before=calloc(1,sizeof(*before));CHECK(game&&before);
    fd2_vga vga;
    for(int variant=0;variant<3;variant++)for(unsigned display=0;display<=1;display++) {
        CHECK(make_stage1_victory(game,&vga,fdother,variant==1,variant==2)==0);
        game->ordinary_display_mode=(uint8_t)display;*before=*game;
        reset_controls(&vga);trace_fragments=1;
        tracked_cursor=fd2_field_cursor_frame(&game->visuals,0);
        CHECK(scene_test_stage1_victory(&vga,fdother,game,NULL,1)==0);
        CHECK(!memcmp(game,before,sizeof(*game)) && archive_opens==archive_closes);
        const size_t first=variant==1?7u:6u;
        const size_t expected[]={first,8,9,10,first,8,9,10};
        CHECK(fragment_log_count==8 && !memcmp(fragment_log,expected,sizeof(expected)));
        /* 165ac聚焦前清51A83，后写1但弹框/关闭只恢复缓存；
         * 135dd随后PAN再清0，因此各fragment只有首个入场帧可继承cursor。 */
        CHECK(cursor_by_fragment[first]==display);
        CHECK(!cursor_by_fragment[8]);
        CHECK(!cursor_by_fragment[9] && !cursor_by_fragment[10]);
        int final_present=presents;
        if(variant==0 && display==1) {
            const int quit_frames[]={1,final_present};
            for(size_t i=0;i<2;i++) {
                reset_controls(&vga);quit_at_frame=quit_frames[i];
                CHECK(scene_test_stage1_victory(&vga,fdother,game,NULL,1)==FD2_SCENE_RESULT_HOST_QUIT);
                CHECK(!memcmp(game,before,sizeof(*game)) && archive_opens==archive_closes);
            }
            reset_controls(&vga);fail_portrait_decode_at=1;
            CHECK(scene_test_stage1_victory(&vga,fdother,game,NULL,1)==FD2_SCENE_RESULT_ERROR);
            CHECK(!presents && !memcmp(game,before,sizeof(*game)) && archive_opens==archive_closes);
        }
        fd2_field_game_close(game);
    }
    free(before);free(game);return 0;
}

/* 165ac/135dd局部显示状态与弹框缓存分开验证；不改真实Game或替换focus逻辑。 */
static int test_stage1_dialog_display(const fd2_archive *fdother) {
    fd2_field_game *game=calloc(1,sizeof(*game)), *before=calloc(1,sizeof(*before));
    CHECK(game && before);
    fd2_vga vga;CHECK(make_stage1_victory(game,&vga,fdother,0,0)==0);
    *before=*game;
    fd2_archive fdtxt={0};fd2_text_entry text={0};fd2_ui_sheet ui={0};
    CHECK(fd2_archive_open(&fdtxt,"original_game/FDTXT.DAT")==0);
    CHECK(fd2_text_entry_open_entry(&text,&fdtxt,2)==0);
    CHECK(ui_sheet_open_fdother(&ui,fdother,5)==0);
    const fd2_image *normal=fd2_field_cursor_frame(&game->visuals,0);CHECK(normal);
    for(unsigned hidden=0;hidden<=1;hidden++)for(unsigned display=0;display<=1;display++) {
        reset_controls(&vga);scene_host_quit_requested=0;
        tracked_cursor=normal;trace_fragments=1;
        fd2_scene_field_state state={0};
        state.units=game->units;state.persistent=game->roster;state.persistent_count=game->roster_count;
        state.camera_cell_x=game->camera_cell_x;state.camera_cell_y=game->camera_cell_y;
        state.focus_cell_x=game->cursor_cell_x;state.focus_cell_y=game->cursor_cell_y;
        state.ordinary_cursor=normal;state.cursor=display?normal:NULL;
        fd2_field_unit_set_hidden(&state.units.items[6],hidden);
        /* -19,6是直接raw，不因hidden跳过165ac；fast只略宿主等待。 */
        CHECK(opening_text(&vga,&game->terrain,&game->map,&game->dato_archive,&ui,
            &game->sprites,&state,&game->font,&text,NULL,6,1)==0);
        CHECK(state.cursor==normal && !state.dialog_cursor_suppressed);
        CHECK(cursor_by_fragment[6]==display);
        CHECK(state.focus_cell_x==state.units.items[6].x &&
              state.focus_cell_y==state.units.items[6].y);
        unsigned drawn=cursor_by_fragment[6];
        pan_camera_to(&vga,&game->terrain,&game->map,&game->sprites,&state,14,2,1);
        CHECK(!state.cursor && !state.dialog_cursor_suppressed && cursor_by_fragment[6]==drawn);
        /* hidden textID不会聚焦，a3=0保留当前mode，不把普通cursor自动打开。 */
        fd2_dialog_state dialog={0};dialog.speaker_actor_idx=-1;
        CHECK(set_dialog_area(&vga,&game->terrain,&game->map,&game->dato_archive,&ui,
            &game->sprites,&state,&dialog,FD2_DIALOG_TOP,game->units.items[6].unit_id,-1,1,NULL)==0);
        CHECK(!state.cursor && !state.dialog_cursor_suppressed);
        /* 旧开场没有ordinary_cursor借用，零初始化仍始终无选择层。 */
        state.ordinary_cursor=NULL;
        CHECK(opening_text(&vga,&game->terrain,&game->map,&game->dato_archive,&ui,
            &game->sprites,&state,&game->font,&text,NULL,6,1)==0);
        CHECK(!state.cursor && !state.dialog_cursor_suppressed && cursor_by_fragment[6]==drawn);
        CHECK(!memcmp(game,before,sizeof(*game)));
    }
    fd2_text_entry_close(&text);fd2_archive_close(&fdtxt);
    fd2_field_game_close(game);free(before);free(game);return 0;
}

static int test_stage2_presentation(const fd2_archive *fdother) {
    fd2_save_manual_state prepared={0};prepared.stage_id=2;prepared.unit_count=6;
    const uint8_t ids[]={0,9,4,30,1,8};
    for(size_t i=0;i<6;i++) {
        fd2_field_unit unit={0};CHECK(fd2_field_unit_persistent_create(&unit,ids[i])==0);
        memcpy(prepared.unit_area+i*80,&unit,80);
    }
    fd2_field_game *game=calloc(1,sizeof(*game)), *before=calloc(1,sizeof(*before));CHECK(game&&before);
    CHECK(fd2_field_game_prepare_stage2(game,fdother,&prepared)==0);*before=*game;
    fd2_vga vga;reset_controls(&vga);trace_fragments=1;
    CHECK(scene_test_stage2_entry(&vga,fdother,game,NULL,1)==0);
    const size_t expected[]={0,1,2,3,0,1,2,3};
    CHECK(fragment_log_count==8 && !memcmp(fragment_log,expected,sizeof(expected)));
    CHECK(game->units.count==15 && game->stage==2 && game->turn_number==1 && game->active_side==2);
    /* 32e8c fragment3 text8先聚焦(7,24)，12cea滚至map高26-8=18；
     * 最后12d7b Hero(7,22)保rel(4,4)。镜头终态属于完整Scene的handoff，
     * 不是只构造单位表的prepare_stage2或最后PAN(3,17)。 */
    CHECK(game->cursor_cell_x==7 && game->cursor_cell_y==22 &&
          game->camera_cell_x==3 && game->camera_cell_y==18);
    CHECK(!memcmp(game->units.items,before->units.items,15*80) &&
          !memcmp(game->roster,before->roster,sizeof(game->roster)) && archive_opens==archive_closes);
    for(size_t i=0;i<15;i++)CHECK(game->units.items[i].direction==0);
    *before=*game;reset_controls(&vga);quit_at_frame=1;
    CHECK(scene_test_stage2_entry(&vga,fdother,game,NULL,1)==FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(!memcmp(game,before,sizeof(*game)) && archive_opens==archive_closes);
    reset_controls(&vga);fail_portrait_decode_at=1;
    CHECK(scene_test_stage2_entry(&vga,fdother,game,NULL,1)==FD2_SCENE_RESULT_ERROR);
    CHECK(!presents && !memcmp(game,before,sizeof(*game)) && archive_opens==archive_closes);
    fd2_field_game_close(game);free(before);free(game);return 0;
}

static int test_turn_event_presentation(const fd2_archive *fdother) {
    /* 34778的fragment4实际有9个等待页；fast仅替换宿主等待，不跳资源/脚本。 */
    fd2_archive fdtxt={0};fd2_text_entry text={0};
    const uint8_t *tokens;size_t token_count;
    CHECK(fd2_archive_open(&fdtxt,"original_game/FDTXT.DAT")==0);
    CHECK(fd2_text_entry_open_entry(&text,&fdtxt,2)==0);
    CHECK(fd2_text_entry_get_fragment(&text,4,&tokens,&token_count)==0);
    CHECK(fd2_dialog_flow_count_waits(tokens,token_count)==9);
    fd2_text_entry_close(&text);fd2_archive_close(&fdtxt);
    for (size_t stage=0;stage<=1;stage++) {
        fd2_field_game *game=calloc(1,sizeof(*game));
        fd2_field_game *before=malloc(sizeof(*before));
        CHECK(game && before);
        fd2_vga vga;reset_controls(&vga);
        if (stage==0) {
            CHECK(fd2_field_game_open(game,&vga,fdother,0)==0);
        } else {
            fd2_save_manual_state prepared={0};
            prepared.stage_id=1;prepared.unit_count=5;
            const uint8_t ids[]={0,9,4,30,1};
            for (size_t i=0;i<5;i++) {
                fd2_field_unit unit={0};
                CHECK(fd2_field_unit_persistent_create(&unit,ids[i])==0);
                memcpy(prepared.unit_area+i*80,&unit,80);
            }
            CHECK(fd2_field_game_prepare_stage1(game,fdother,&prepared)==0);
        }
        game->turn_number=3;
        CHECK(fd2_field_game_end_active_phase(game)==0);
        CHECK(game->event_log_count==1);
        fd2_field_event_notice *notice=&game->event_log[0];
        CHECK(notice->action==(stage ? 6 : 0) && notice->handled &&
              notice->presentation_deferred);
        CHECK(notice->presentation_unit_count==(stage ? 21 : 12));
        CHECK(game->units.count==(stage ? 27 : 14));
        if (!stage) {
            /* strictLE原表：unit1/LV1与unit3/LV3，装备32/AP20 AC95、128/DP2。 */
            CHECK(game->units.items[12].attack==27 && game->units.items[12].defense==6 &&
                  game->units.items[12].accuracy==96 && game->units.items[12].evasion==1);
            CHECK(game->units.items[13].attack==50 && game->units.items[13].defense==17 &&
                  game->units.items[13].accuracy==106 && game->units.items[13].evasion==11);
        }
        if (stage) {
            /* 原版已知新6人31/5/98/3，旧呈现模板恰缺这4个WORD。 */
            fd2_field_units replay=game->units;
            replay.count=notice->presentation_unit_count;
            CHECK(fd2_field_units_append_group_placed(&replay,&game->metadata,
                &game->placements,&game->map,3,1)==0);
            const uint8_t *script;size_t script_size;
            CHECK(fd2_field_movement_script_get(13,&script,&script_size)==0);
            CHECK(fd2_field_movement_script_apply(&replay,script,script_size)==0);
            size_t differences=0;
            for(size_t i=0;i<game->units.count;i++)for(size_t b=0;b<80;b++) {
                if (((uint8_t *)&replay.items[i])[b]==
                    ((uint8_t *)&game->units.items[i])[b]) continue;
                CHECK(i>=21 && (b==0x48 || b==0x4a || b==0x4c || b==0x4e));
                differences++;
            }
            CHECK(differences==24);
            for (size_t i=21;i<27;i++) {
                CHECK(game->units.items[i].attack==31 && game->units.items[i].defense==5 &&
                      game->units.items[i].accuracy==98 && game->units.items[i].evasion==3);
            }
        }
        *before=*game;reset_controls(&vga);
        CHECK(scene_test_play_event(&vga,fdother,game,notice,NULL,1)==FD2_SCENE_RESULT_OK);
        CHECK(!memcmp(&game->units,&before->units,sizeof(game->units)));
        CHECK(!memcmp(game->roster,before->roster,sizeof(game->roster)));
        CHECK(!notice->presentation_deferred && game->turn_number==3 && game->active_side==1);
        CHECK(game->experience_value==before->experience_value &&
              game->message_number==before->message_number);
        CHECK(archive_opens==2 && archive_closes==2 && presents>0);
        int final_present=presents;
        if (!stage) {
            /* 同ID的持久角色以Game来源为准，不能强套呈现模板idx9。 */
            *game=*before;game->units.source_template_indices[12]=FD2_FIELD_UNIT_NO_TEMPLATE;
            fd2_field_units expected=game->units;reset_controls(&vga);
            CHECK(scene_test_play_event(&vga,fdother,game,&game->event_log[0],NULL,1)==0);
            CHECK(!memcmp(&game->units,&expected,sizeof(expected)));
        }
        for (int failure=0;failure<2;failure++) {
            *game=*before;
            size_t first=game->event_log[0].presentation_unit_count;
            if (failure==0) game->units.items[first].unit_id^=1;
            else game->units.source_template_indices[first]++;
            fd2_scene_field_state state={0};state.units=game->units;
            state.units.count=first;
            fd2_field_units original=state.units;
            reset_controls(&vga);
            CHECK(field_event_append_group(&vga,fdother,game,&state,3,0,NULL,1,NULL)==-1);
            CHECK(!memcmp(&state.units,&original,sizeof(original)) && presents==0);
        }
        /* 借用静态属性不等于接受任意终态；脚本坐标不符仍须整80B拒绝。 */
        *game=*before;
        game->units.items[game->event_log[0].presentation_unit_count].x++;
        fd2_field_game *wrong_end=malloc(sizeof(*wrong_end));CHECK(wrong_end);
        *wrong_end=*game;reset_controls(&vga);
        CHECK(scene_test_play_event(&vga,fdother,game,&game->event_log[0],NULL,1)==FD2_SCENE_RESULT_ERROR);
        CHECK(!memcmp(game,wrong_end,sizeof(*game)) && archive_opens==archive_closes);
        free(wrong_end);
        const int quit_frames[]={1,final_present};
        for (size_t i=0;i<sizeof(quit_frames)/sizeof(quit_frames[0]);i++) {
            *game=*before;reset_controls(&vga);quit_at_frame=quit_frames[i];
            CHECK(scene_test_play_event(&vga,fdother,game,&game->event_log[0],NULL,1)==
                  FD2_SCENE_RESULT_HOST_QUIT);
            CHECK(!memcmp(game,before,sizeof(*game)));
            CHECK(archive_opens==2 && archive_closes==2);
        }
        *game=*before;reset_controls(&vga);fail_archive_path="original_game/DATO.DAT";
        CHECK(scene_test_play_event(&vga,fdother,game,&game->event_log[0],NULL,1)==FD2_SCENE_RESULT_ERROR);
        CHECK(!memcmp(game,before,sizeof(*game)) && presents==0);
        CHECK(archive_opens==1 && archive_closes==1);
        fd2_field_game_close(game);free(before);free(game);
    }
    return 0;
}

static int test_dialog_identity_lookup(void) {
    fd2_scene_field_state state={0};
    state.units.count=3;
    state.units.items[0].text_id=1;state.units.items[0].unit_id=1;state.units.items[0].flags=1;
    state.units.items[1].text_id=1;state.units.items[1].unit_id=51;
    state.units.items[2].text_id=1;state.units.items[2].unit_id=50;
    CHECK(resolve_portrait_control(&state,FD2_TEXT_BOTTOM_PORTRAIT,1)==51);
    CHECK(resolve_speaker_actor(&state,FD2_TEXT_BOTTOM_PORTRAIT,1)==1);
    state.units.items[1].flags=state.units.items[2].flags=1;
    CHECK(resolve_portrait_control(&state,FD2_TEXT_BOTTOM_PORTRAIT,1)==50);
    CHECK(resolve_speaker_actor(&state,FD2_TEXT_BOTTOM_PORTRAIT,1)==-1);
    fd2_field_unit persistent[2]={{0}};
    persistent[0].text_id=persistent[1].text_id=1;
    persistent[0].unit_id=1;persistent[1].unit_id=51;
    state.persistent=persistent;state.persistent_count=2;
    CHECK(resolve_portrait_control(&state,FD2_TEXT_BOTTOM_PORTRAIT,1)==50); /* active hidden仍先于roster */
    state.units.count=0;
    CHECK(resolve_portrait_control(&state,FD2_TEXT_BOTTOM_PORTRAIT,1)==51);
    CHECK(resolve_speaker_actor(&state,FD2_TEXT_BOTTOM_PORTRAIT,1)==-1);
    CHECK(resolve_portrait_control(&state,FD2_TEXT_BOTTOM_PORTRAIT,39)==39);
    CHECK(persistent[1].unit_id==51 && state.persistent_count==2);
    return 0;
}

static int test_game_over_frames(const fd2_archive *fdother) {
    fd2_vga vga;reset_controls(&vga);
    memset(vga.framebuffer,0xa5,sizeof(vga.framebuffer));
    const uint8_t *data;size_t size;
    CHECK(fd2_archive_get(fdother,79,&data,&size)==0 && size>=16);
    uint8_t expected[VGA_W*VGA_H]={0};
    for(size_t i=0;i<2;i++) {
        uint32_t start=rd_u32_le(data+8+4*i);
        uint32_t end=i? (uint32_t)size:rd_u32_le(data+12);
        fd2_image image={0};
        CHECK(fd2_image_decode_buf(&image,data+start+9,end-start-9)==0);
        int x=rd_u16_le(data+start),y=rd_u16_le(data+start+2);
        CHECK(x==(i?130:69) && y==(i?141:61));
        size_t opaque=0;
        for(int row=0;row<image.height;row++)for(int col=0;col<image.width;col++) {
            uint8_t pixel=image.pixels[row*image.width+col];
            if(pixel)opaque++;
            expected[(y+row)*VGA_STRIDE+x+col]=pixel;
        }
        CHECK(opaque==(i?263u:9848u));
        fd2_image_free(&image);
    }
    uint64_t start=test_ms;
    CHECK(scene_test_game_over(&vga,fdother,NULL,0)==0);
    CHECK(!memcmp(vga.framebuffer,expected,sizeof(expected)));
    CHECK(test_ms-start>=46u*55u); /* 普通Enter脉冲不能略过固定停顿。 */
    reset_controls(&vga);quit_at_frame=1;
    memset(vga.framebuffer,0xa5,sizeof(vga.framebuffer));
    CHECK(scene_test_game_over(&vga,fdother,NULL,0)==FD2_SCENE_RESULT_HOST_QUIT);
    CHECK(vga.framebuffer[0]==0xa5); /* clear0在tick1/fade之后。 */
    return 0;
}

int main(int argc, char **argv) {
    fd2_archive fdother = {0};
    CHECK(fd2_archive_open(&fdother, "original_game/FDOTHER.DAT") == 0);
    if(argc==2 && strcmp(argv[1],"--b5")==0) {
        CHECK(test_dialog_identity_lookup()==0);
        CHECK(test_stage1_presentation(&fdother)==0);
        CHECK(test_stage1_victory_projection(&fdother)==0);
        CHECK(test_stage1_dialog_display(&fdother)==0);
        CHECK(test_stage2_presentation(&fdother)==0);
        CHECK(test_turn_event_presentation(&fdother)==0);
        CHECK(test_game_over_frames(&fdother)==0);
        fd2_archive_close(&fdother);
        puts("scene_transition: stage0/stage1 event identity and committed attributes, host quit, GAME OVER passed (no GUI)");
        return 0;
    }
    int final_present = 0;
    CHECK(test_natural_stage0_handoff(&fdother, &final_present) == 0);
    CHECK(test_resource_error_has_no_handoff(&fdother) == 0);
    CHECK(test_required_palette_error_has_no_handoff(&fdother) == 0);
    CHECK(test_real_portrait_orientation() == 0);
    CHECK(test_portrait_decode_error_has_no_handoff(&fdother) == 0);
    CHECK(test_host_quit_has_no_handoff(&fdother, final_present) == 0);
    fd2_archive_close(&fdother);
    puts("scene_handoff_test: ok");
    return 0;
}
