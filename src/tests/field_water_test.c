/* sub_4E31C @0x4e31c（原登记sub_4E31C）水DAC纯视觉回归。
 * 不创建窗口；真实资源/START只读，所有推进均由显式调用提供mock时钟。 */
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "archive.h"
#include "field_game.h"
#include "field_visual.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

static uint8_t initial_palette[768];

static int load_palette(void) {
    fd2_archive archive = {0};
    const uint8_t *data;
    size_t size;
    CHECK(fd2_archive_open(&archive, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_archive_get(&archive, 0, &data, &size) == 0 && size >= 768);
    memcpy(initial_palette, data, sizeof(initial_palette));
    fd2_archive_close(&archive);
    return 0;
}

static uint8_t expected_component(unsigned phase, unsigned i, unsigned channel) {
    /* 独立raw审计确认FDOTHER初始bank每分量恰少1；不复制生产常量。 */
    uint8_t value = (uint8_t)(initial_palette[(224u + ((phase + i) & 15u)) * 3u + channel] + 1u);
    return (uint8_t)((value << 2) | (value >> 4));
}

static int check_dac(const fd2_vga *vga, unsigned phase) {
    for (unsigned i = 0; i < 16; i++)
        for (unsigned c = 0; c < 3; c++)
            CHECK(vga->dac[(224u + i) * 3u + c] == expected_component(phase, i, c));
    return 0;
}

static int test_palette_write_set_and_errors(void) {
    fd2_vga vga = {0};
    memset(vga.framebuffer, 0x5a, sizeof(vga.framebuffer));
    memset(vga.palette, 0x11, sizeof(vga.palette));
    memset(vga.dac, 0x77, sizeof(vga.dac));
    vga.field_water_phase = 9;
    vga.field_water_last_tick = 123;
    fd2_input_init(&vga.input);
    fd2_input_set_test_key_state(&vga.input, SDL_SCANCODE_F1, 1);
    fd2_input_begin_test_frame(&vga.input, 1000);
    for (unsigned phase = 0; phase < 16; phase++) {
        fd2_vga expected = vga;
        for (unsigned i = 0; i < 16; i++)
            for (unsigned c = 0; c < 3; c++)
                expected.dac[(224u + i) * 3u + c] = expected_component(phase, i, c);
        CHECK(fd2_field_water_palette_apply(&vga, phase) == 0);
        CHECK(memcmp(&vga, &expected, sizeof(vga)) == 0);
    }
    fd2_vga before = vga;
    CHECK(fd2_field_water_palette_apply(NULL, 0) == -1);
    CHECK(fd2_field_water_palette_apply(&vga, 16) == -1);
    CHECK(fd2_field_water_palette_apply(&vga, UINT_MAX) == -1);
    CHECK(memcmp(&vga, &before, sizeof(vga)) == 0);
    return 0;
}

static int test_tick_boundaries_and_single_step(void) {
    fd2_vga vga = {0};
    fd2_input_init(&vga.input);
    fd2_vga_set_palette(&vga, initial_palette);
    fd2_vga before = vga;
    const uint64_t not_due[] = {0, 54, 55, 109};
    for (size_t i = 0; i < sizeof(not_due)/sizeof(not_due[0]); i++) {
        CHECK(fd2_field_water_tick(&vga, not_due[i]) == 0);
        CHECK(memcmp(&vga, &before, sizeof(vga)) == 0);
    }
    CHECK(fd2_field_water_tick(&vga, 110) == 1);
    CHECK(vga.field_water_phase == 1 && vga.field_water_last_tick == 2);
    CHECK(check_dac(&vga, 1) == 0); /* 首次更新先increment，不先画phase0。 */
    before = vga;
    CHECK(fd2_field_water_tick(&vga, 110) == 0);
    CHECK(fd2_field_water_tick(&vga, 219) == 0);
    CHECK(memcmp(&vga, &before, sizeof(vga)) == 0);
    CHECK(fd2_field_water_tick(&vga, 220) == 1);
    CHECK(vga.field_water_phase == 2 && vga.field_water_last_tick == 4);
    CHECK(fd2_field_water_tick(&vga, 1000000) == 1);
    CHECK(vga.field_water_phase == 3 && vga.field_water_last_tick == (uint16_t)(1000000u / 55u));
    CHECK(check_dac(&vga, 3) == 0);
    before = vga;
    CHECK(fd2_field_water_tick(&vga, 1000000) == 0);
    CHECK(memcmp(&vga, &before, sizeof(vga)) == 0);
    CHECK(memcmp(vga.palette, initial_palette, sizeof(initial_palette)) == 0);
    return 0;
}

static int test_low16_wrap_phase_wrap_and_quit(void) {
    fd2_vga vga = {0};
    fd2_input_init(&vga.input);
    vga.field_water_last_tick = UINT16_MAX;
    vga.field_water_phase = 15;
    CHECK(fd2_field_water_tick(&vga, UINT64_C(65536) * 55u) == 0); /* unsigned差1。 */
    CHECK(fd2_field_water_tick(&vga, UINT64_C(65537) * 55u) == 1);
    CHECK(vga.field_water_phase == 0 && vga.field_water_last_tick == 1);
    CHECK(check_dac(&vga, 0) == 0);
    vga.field_water_last_tick = 1000;
    CHECK(fd2_field_water_tick(&vga, 500u * 55u) == 1); /* 原unsigned差，不当signed负数跳过。 */
    CHECK(vga.field_water_phase == 1 && vga.field_water_last_tick == 500);
    for (unsigned i = 0; i < 16; i++) {
        CHECK(fd2_field_water_tick(&vga, (502u + i * 2u) * 55u) == 1);
        CHECK(vga.field_water_phase == ((i + 2u) & 15u));
        CHECK(check_dac(&vga, vga.field_water_phase) == 0);
    }
    vga.input.quit_requested = 1;
    fd2_vga before = vga;
    CHECK(fd2_field_water_tick(&vga, 3000u * 55u) == 0);
    CHECK(memcmp(&vga, &before, sizeof(vga)) == 0);
    vga.field_water_phase = 255;
    CHECK(fd2_field_water_tick(&vga, 3000u * 55u) == 0); /* hostquit先于能力门。 */
    vga.input.quit_requested = 0;
    before = vga;
    CHECK(fd2_field_water_tick(&vga, 3000u * 55u) == -1);
    CHECK(memcmp(&vga, &before, sizeof(vga)) == 0);
    CHECK(fd2_field_water_tick(NULL, 0) == -1);
    return 0;
}

static int test_load_render_pause_and_resume_owner(void) {
    fd2_archive archive = {0};
    fd2_field_game game = {0};
    fd2_vga vga = {0};
    CHECK(fd2_archive_open(&archive, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_field_game_open(&game, &vga, &archive, 0) == 0);
    CHECK(fd2_field_water_tick(&vga, 110) == 1);
    fd2_vga before_load = vga;
    CHECK(fd2_field_game_load_active(&game, "original_game/START.SAV") == 0);
    CHECK(game.units.count == 12 && game.roster_count == 4);
    CHECK(memcmp(&vga, &before_load, sizeof(vga)) == 0);
    fd2_field_game_render(&game, &vga); /* 首次常规render确定info侧后再比较game owner。 */
    fd2_field_game before_render = game;
    uint8_t frozen[768];
    memcpy(frozen, vga.dac, sizeof(frozen));
    for (int i = 0; i < 20; i++) {
        fd2_field_game_render(&game, &vga);
        fd2_field_game_render_background(&game, &vga);
        CHECK(vga.field_water_phase == 1 && vga.field_water_last_tick == 2);
        CHECK(memcmp(vga.dac, frozen, sizeof(frozen)) == 0);
    }
    CHECK(memcmp(&game, &before_render, sizeof(game)) == 0);
    /* 无tick的暂停时间不隐式推进；恢复时只一步，不追赶F1停留时间。 */
    CHECK(fd2_field_water_tick(&vga, 600000) == 1);
    CHECK(vga.field_water_phase == 2);
    CHECK(check_dac(&vga, 2) == 0);
    uint16_t last = vga.field_water_last_tick;
    fd2_field_game_close(&game);
    CHECK(fd2_field_game_open(&game, &vga, &archive, 0) == 0);
    CHECK(vga.field_water_phase == 2 && vga.field_water_last_tick == last);
    /* 新session可重装base palette，但不能重置app级时钟/phase。 */
    CHECK(fd2_field_water_tick(&vga, 600110) == 1);
    CHECK(vga.field_water_phase == 3 && check_dac(&vga, 3) == 0);
    fd2_field_game_close(&game);
    fd2_archive_close(&archive);
    return 0;
}

int main(void) {
    CHECK(load_palette() == 0);
    CHECK(test_palette_write_set_and_errors() == 0);
    CHECK(test_tick_boundaries_and_single_step() == 0);
    CHECK(test_low16_wrap_phase_wrap_and_quit() == 0);
    CHECK(test_load_render_pause_and_resume_owner() == 0);
    puts("field water tests: ok");
    return 0;
}
