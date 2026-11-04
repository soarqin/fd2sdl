/* sub_127E0 @0x127e0 / sub_4E1A6 @0x4e1a6：合成FDICON验证frame、
 * 坐标、已行动映色与透明，不启动窗口，也不改变bank或actor owner。 */
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "field_unit.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

static uint8_t frames[24 * 24 * 24];
static void init_frames(void) {
    for (size_t i = 0; i < 24; i++)
        memset(frames + i * 24 * 24, (int)(i + 1), 24 * 24);
}

static int test_frame_and_coordinates(void) {
    init_frames();
    fd2_map_sprite_bank bank = {0};
    bank.decoded_frames = frames;
    bank.frame_count = 24;
    fd2_field_unit unit = {0};
    unit.x = unit.y = 2;
    fd2_field_unit before = unit;
    fd2_vga vga = {0};
    fd2_field_unit_render(&vga, &bank, &unit, 0, 0, 0, 3);
    CHECK(vga.framebuffer[42 * VGA_STRIDE + 48] == 2); /* idle3→frame1 */
    CHECK(vga.framebuffer[41 * VGA_STRIDE + 48] == 0);
    CHECK(memcmp(&unit, &before, sizeof(unit)) == 0);
    memset(vga.framebuffer, 0, sizeof(vga.framebuffer));
    unit.direction = 3;
    unit.frame_phase = 2;
    fd2_field_unit_render(&vga, &bank, &unit, 1, 0, 0, 2);
    CHECK(vga.framebuffer[42 * VGA_STRIDE + 56] == 11); /* dir3+walk1, +8px */
    CHECK(vga.framebuffer[42 * VGA_STRIDE + 55] == 0);
    return 0;
}

static int test_acted_mapping_and_skip(void) {
    init_frames();
    frames[0] = 0; frames[1] = 0xc7; frames[2] = 0x40; frames[3] = 0xff;
    uint8_t before[sizeof(frames)];
    memcpy(before, frames, sizeof(frames));
    fd2_map_sprite_bank bank = {0};
    bank.decoded_frames = frames; bank.frame_count = 24;
    fd2_field_unit unit = {0};
    unit.x = unit.y = 2;
    unit.flags = FD2_FIELD_UNIT_FLAG_ACTED;
    fd2_field_unit old_unit = unit;
    fd2_vga vga = {0};
    memset(vga.framebuffer, 201, sizeof(vga.framebuffer));
    fd2_field_unit_render(&vga, &bank, &unit, 0, 0, 0, 0);
    CHECK(vga.framebuffer[42 * VGA_STRIDE + 48] == 201);
    CHECK(vga.framebuffer[42 * VGA_STRIDE + 49] == 31);
    CHECK(vga.framebuffer[42 * VGA_STRIDE + 50] == 24);
    CHECK(vga.framebuffer[42 * VGA_STRIDE + 51] == 31);
    CHECK(vga.framebuffer[42 * VGA_STRIDE + 52] == 25);
    CHECK(memcmp(before, frames, sizeof(frames)) == 0);
    CHECK(memcmp(&unit, &old_unit, sizeof(unit)) == 0);
    return 0;
}

static int test_clip_and_reject(void) {
    init_frames();
    fd2_map_sprite_bank bank = {0};
    bank.decoded_frames = frames; bank.frame_count = 24;
    fd2_field_unit unit = {0};
    unit.flags = FD2_FIELD_UNIT_FLAG_ACTED;
    fd2_vga vga = {0};
    memset(vga.framebuffer, 201, sizeof(vga.framebuffer));
    fd2_field_unit_render(&vga, &bank, &unit, 0, 5, 5, 0); /* -5,-11 */
    CHECK(vga.framebuffer[0] == 25);
    CHECK(vga.framebuffer[12 * VGA_STRIDE + 18] == 25);
    CHECK(vga.framebuffer[13 * VGA_STRIDE + 18] == 201);
    CHECK(vga.framebuffer[12 * VGA_STRIDE + 19] == 201);
    uint8_t before[VGA_W * VGA_H];
    memcpy(before, vga.framebuffer, sizeof(before));
    fd2_field_unit_render(&vga, &bank, &unit, 0, INT_MIN, INT_MAX, 0);
    fd2_field_unit_render(&vga, &bank, &unit, 0, INT_MAX, INT_MIN, 0);
    unit.flags |= FD2_FIELD_UNIT_FLAG_HIDDEN;
    fd2_field_unit_render(&vga, &bank, &unit, 0, 0, 0, 0);
    unit.flags = 0; unit.unit_id = 2; /* frame24越界 */
    fd2_field_unit_render(&vga, &bank, &unit, 0, 0, 0, 0);
    fd2_field_unit_render(&vga, &bank, NULL, 0, 0, 0, 0);
    CHECK(memcmp(before, vga.framebuffer, sizeof(before)) == 0);
    return 0;
}

static int test_depth_order(void) {
    init_frames();
    fd2_map_sprite_bank bank = {0};
    bank.decoded_frames = frames; bank.frame_count = 24;
    fd2_field_units units = {0};
    units.count = 2;
    units.items[0].x = units.items[1].x = 2;
    units.items[0].y = 3;
    units.items[1].y = 2;
    units.items[1].unit_id = 1;
    units.items[1].frame_phase = 2;
    fd2_vga vga = {0};
    fd2_field_units_render(&vga, &bank, &units, 0, 0, 0);
    CHECK(vga.framebuffer[60 * VGA_STRIDE + 48] == 13);
    CHECK(vga.framebuffer[68 * VGA_STRIDE + 48] == 1);
    units.items[0].flags = FD2_FIELD_UNIT_FLAG_ACTED;
    fd2_field_units_render(&vga, &bank, &units, 0, 0, 0);
    CHECK(vga.framebuffer[68 * VGA_STRIDE + 48] == 25);
    return 0;
}

int main(void) {
    CHECK(test_frame_and_coordinates() == 0);
    CHECK(test_acted_mapping_and_skip() == 0);
    CHECK(test_clip_and_reject() == 0);
    CHECK(test_depth_order() == 0);
    puts("field_unit_render_test: ok");
    return 0;
}
