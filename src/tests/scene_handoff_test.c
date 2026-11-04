/* new_game_opening_play @0x3231b：真实 32→31→0 资源的无窗口交接回归。
 * 私有 TU 仅替换呈现/等待和故障注入，不增加生产测试入口。
 * 终态来自 FDFIELD placement 与原版 script 0/1/2/5 的写集，不读取 START.SAV。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "scene.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

static uint64_t test_ms;
static int presents, quit_at_frame, quit_on_preview_delay;
static int archive_opens, archive_closes;
static const char *fail_archive_path;
static fd2_vga *current_vga;

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
#define fd2_archive_open scene_test_archive_open
#define fd2_archive_close scene_test_archive_close
#define fd2_vga_present scene_test_present
#define fd2_vga_present_timed scene_test_present_timed
#define fd2_delay_ms scene_test_delay
#define SDL_GetTicks scene_test_ticks
#include "../scene.c"
#undef fd2_scene_play_new_game_prologue_handoff
#undef fd2_scene_play_new_game_prologue
#undef fd2_scene_play_field_event
#undef fd2_archive_open
#undef fd2_archive_close
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

int main(void) {
    fd2_archive fdother = {0};
    CHECK(fd2_archive_open(&fdother, "original_game/FDOTHER.DAT") == 0);
    int final_present = 0;
    CHECK(test_natural_stage0_handoff(&fdother, &final_present) == 0);
    CHECK(test_resource_error_has_no_handoff(&fdother) == 0);
    CHECK(test_host_quit_has_no_handoff(&fdother, final_present) == 0);
    fd2_archive_close(&fdother);
    puts("scene_handoff_test: ok");
    return 0;
}
