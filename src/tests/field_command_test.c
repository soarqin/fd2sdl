#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_command.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

#define FRAME_SIZE_20 (4u + FD2_FIELD_COMMAND_ICON_WIDTH * 20u)
#define FRAME_SIZE_16 (4u + FD2_FIELD_COMMAND_ICON_WIDTH * 16u)
#define SHEET_SIZE (FD2_FIELD_COMMAND_FRAME_COUNT * 4u + \
                    74u * FRAME_SIZE_20 + 4u * FRAME_SIZE_16)

static void put_u16le(uint8_t *p, uint16_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
}

static void put_u32le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static uint32_t get_u32le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int is_short_frame(size_t frame) {
    return frame == 48u || frame == 49u || frame == 51u || frame == 52u;
}

static void make_sheet(uint8_t sheet[SHEET_SIZE]) {
    size_t offset = FD2_FIELD_COMMAND_FRAME_COUNT * 4u;
    memset(sheet, 0, SHEET_SIZE);
    for (size_t i = 0; i < FD2_FIELD_COMMAND_FRAME_COUNT; i++) {
        uint16_t height = is_short_frame(i) ? 16u : 20u;
        size_t pixel_count = FD2_FIELD_COMMAND_ICON_WIDTH * (size_t)height;
        put_u32le(sheet + i * 4u, (uint32_t)offset);
        put_u16le(sheet + offset, FD2_FIELD_COMMAND_ICON_WIDTH);
        put_u16le(sheet + offset + 2u, height);
        memset(sheet + offset + 4u, (int)i + 1, pixel_count);
        offset += 4u + pixel_count;
    }
}

static fd2_archive make_archive(uint8_t *sheet, uint32_t offsets[3]) {
    offsets[0] = 0;
    offsets[1] = 0;
    offsets[2] = 0;
    fd2_archive archive = {
        .data = sheet,
        .size = SHEET_SIZE,
        .offsets = offsets,
        .count = 3,
    };
    return archive;
}

static int test_sheet_decode_and_frames(void) {
    uint8_t sheet[SHEET_SIZE];
    uint32_t offsets[3];
    make_sheet(sheet);
    fd2_archive archive = make_archive(sheet, offsets);
    fd2_field_command_assets assets;
    CHECK(fd2_field_command_assets_open(&assets, &archive) == 0);
    CHECK(assets.ready);
    for (size_t i = 0; i < FD2_FIELD_COMMAND_FRAME_COUNT; i++) {
        CHECK(assets.frames[i].width == FD2_FIELD_COMMAND_ICON_WIDTH);
        CHECK(assets.frames[i].height ==
              (is_short_frame(i) ? 16 : FD2_FIELD_COMMAND_ICON_HEIGHT));
        CHECK(assets.frames[i].pixels[0] == i + 1u);
    }
    CHECK(fd2_field_command_frame(
              &assets, FD2_FIELD_COMMAND_ITEM, 0, 0, 0)->pixels[0] == 7);
    CHECK(fd2_field_command_frame(
              &assets, FD2_FIELD_COMMAND_ITEM, 1, 0, 1)->pixels[0] == 8);
    CHECK(fd2_field_command_frame(
              &assets, FD2_FIELD_COMMAND_ITEM, 1, 1, 1)->pixels[0] == 9);
    CHECK(fd2_field_command_frame_id(
              &assets, 25, 1, 0, 1)->pixels[0] == 77);
    CHECK(fd2_field_command_frame_id(
              &assets, 26, 0, 0, 0) == NULL);
    fd2_field_command_assets_close(&assets);
    CHECK(!assets.ready);

    make_sheet(sheet);
    put_u16le(sheet + FD2_FIELD_COMMAND_FRAME_COUNT * 4u, 23);
    archive = make_archive(sheet, offsets);
    CHECK(fd2_field_command_assets_open(&assets, &archive) != 0);

    make_sheet(sheet);
    size_t frame47 = get_u32le(sheet + 47u * 4u);
    put_u16le(sheet + frame47 + 2u, 16);
    archive = make_archive(sheet, offsets);
    CHECK(fd2_field_command_assets_open(&assets, &archive) != 0);
    return 0;
}

static int test_direction_selection(void) {
    uint8_t disabled[FD2_FIELD_COMMAND_COUNT] = {0, 1, 1, 0};
    CHECK(fd2_field_command_first_enabled(disabled) == FD2_FIELD_COMMAND_ATTACK);
    CHECK(fd2_field_command_select_direction(
              FD2_FIELD_COMMAND_ATTACK, FD2_FIELD_COMMAND_DIRECTION_DOWN,
              disabled) == FD2_FIELD_COMMAND_WAIT);
    CHECK(fd2_field_command_select_direction(
              FD2_FIELD_COMMAND_WAIT, FD2_FIELD_COMMAND_DIRECTION_LEFT,
              disabled) == FD2_FIELD_COMMAND_WAIT);
    disabled[FD2_FIELD_COMMAND_ATTACK] = 1;
    CHECK(fd2_field_command_first_enabled(disabled) == FD2_FIELD_COMMAND_WAIT);
    disabled[FD2_FIELD_COMMAND_WAIT] = 1;
    CHECK(fd2_field_command_first_enabled(disabled) == -1);
    return 0;
}

static int test_radial_positions_and_states(void) {
    uint8_t sheet[SHEET_SIZE];
    uint32_t offsets[3];
    make_sheet(sheet);
    fd2_archive archive = make_archive(sheet, offsets);
    fd2_field_command_assets assets;
    fd2_vga *vga = calloc(1, sizeof(*vga));
    CHECK(vga && fd2_field_command_assets_open(&assets, &archive) == 0);
    uint8_t disabled[FD2_FIELD_COMMAND_COUNT] = {0, 1, 1, 0};

    fd2_field_command_draw_animation(vga, &assets, 100, 80,
                                     FD2_FIELD_COMMAND_ATTACK,
                                     disabled, 1, 1, 4);
    /* 最终四向位置：up (0,-18)、left (-24,2)、right (24,2)、down (0,22)。 */
    CHECK(vga->framebuffer[62 * VGA_STRIDE + 100] == 2);
    CHECK(vga->framebuffer[82 * VGA_STRIDE + 76] == 6);
    CHECK(vga->framebuffer[82 * VGA_STRIDE + 124] == 9);
    CHECK(vga->framebuffer[102 * VGA_STRIDE + 100] == 10);

    memset(vga->framebuffer, 0, sizeof(vga->framebuffer));
    fd2_field_command_draw_animation(vga, &assets, 100, 80,
                                     FD2_FIELD_COMMAND_ATTACK,
                                     disabled, 0, 1, 1);
    CHECK(vga->framebuffer[77 * VGA_STRIDE + 100] == 1);
    CHECK(vga->framebuffer[82 * VGA_STRIDE + 94] == 6);
    CHECK(vga->framebuffer[82 * VGA_STRIDE + 106] == 9);
    CHECK(vga->framebuffer[87 * VGA_STRIDE + 100] == 10);

    /* close @code0 0x76b4 使用独立初值；可见四帧不是 opening 的简单倒序。 */
    static const int close_up_y[] = {65, 70, 75};
    static const int close_left_x[] = {82, 88, 94};
    static const int close_right_x[] = {118, 112, 106};
    static const int close_down_y[] = {99, 94, 89};
    for (uint8_t phase = 1; phase <= 3; phase++) {
        memset(vga->framebuffer, 0, sizeof(vga->framebuffer));
        fd2_field_command_draw_animation(vga, &assets, 100, 80,
                                         FD2_FIELD_COMMAND_ATTACK,
                                         disabled, 0, 0, phase);
        CHECK(vga->framebuffer[close_up_y[phase - 1] * VGA_STRIDE + 100] == 1);
        CHECK(vga->framebuffer[82 * VGA_STRIDE + close_left_x[phase - 1]] == 6);
        CHECK(vga->framebuffer[82 * VGA_STRIDE + close_right_x[phase - 1]] == 9);
        CHECK(vga->framebuffer[close_down_y[phase - 1] * VGA_STRIDE + 100] == 10);
    }
    memset(vga->framebuffer, 0, sizeof(vga->framebuffer));
    fd2_field_command_draw_animation(vga, &assets, 100, 80,
                                     FD2_FIELD_COMMAND_ATTACK,
                                     disabled, 0, 0, 4);
    CHECK(vga->framebuffer[80 * VGA_STRIDE + 100] == 1);
    CHECK(vga->framebuffer[82 * VGA_STRIDE + 100] == 9);
    CHECK(vga->framebuffer[84 * VGA_STRIDE + 100] == 10);

    /* field-level 系统 command ID 复用相同四相位坐标，但帧索引按
     * command_id*3+state 取值，不能强转成单位 action enum。 */
    uint8_t system_ids[FD2_FIELD_COMMAND_COUNT] = {7, 5, 6, 4};
    uint8_t system_disabled[FD2_FIELD_COMMAND_COUNT] = {0, 0, 1, 0};
    memset(vga->framebuffer, 0, sizeof(vga->framebuffer));
    fd2_field_command_draw_id_animation(
        vga, &assets, 100, 80, system_ids, 1, system_disabled, 1, 1, 4);
    CHECK(vga->framebuffer[62 * VGA_STRIDE + 100] == 22);
    CHECK(vga->framebuffer[82 * VGA_STRIDE + 76] == 17);
    CHECK(vga->framebuffer[82 * VGA_STRIDE + 124] == 21);
    CHECK(vga->framebuffer[102 * VGA_STRIDE + 100] == 13);

    fd2_field_command_assets_close(&assets);
    free(vga);
    return 0;
}

int main(void) {
    if (test_sheet_decode_and_frames() != 0 ||
        test_direction_selection() != 0 ||
        test_radial_positions_and_states() != 0)
        return 1;
    puts("field_command_test: ok");
    return 0;
}
