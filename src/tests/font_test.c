/* font_glyph_blit_16x16 @0x4ed7a（原登记 FUN_0004c4c2）回归。
 * 只操作合成字模与内存VGA；不初始化SDL、窗口或原版资源。
 */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "font.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

typedef struct {
    uint8_t before[16];
    fd2_vga vga;
    uint8_t after[16];
} guarded_vga;

static guarded_vga actual, expected;
static uint8_t glyphs[11 * 0x20];
static const fd2_font font = {glyphs, sizeof(glyphs), 11};

static void reset(void) {
    memset(&actual, 0x6d, sizeof(actual));
    memcpy(&expected, &actual, sizeof(expected));
    memset(glyphs, 0, sizeof(glyphs));
}

static void expected_pixel(int x, int y, uint8_t color) {
    expected.vga.framebuffer[(size_t)y * VGA_STRIDE + (size_t)x] = color;
}

static void expected_rect(int left, int top, int width, int height,
                           uint8_t color) {
    for (int row = top; row < top + height; row++)
        memset(expected.vga.framebuffer + (size_t)row * VGA_STRIDE + left,
               color, (size_t)width);
}

static int test_shadow_direction_and_transparency(void) {
    reset();
    glyphs[3 * 2] = 1; /* row3,col7：big-endian 0x0100。 */
    uint8_t before[sizeof(glyphs)];
    memcpy(before, glyphs, sizeof(before));
    fd2_font_draw_glyph(&actual.vga, &font, 0, 10, 20, 0xcd, 0x4c, -1);
    expected_pixel(17, 23, 0xcd);
    expected_pixel(16, 24, 0x4c);
    expected_pixel(17, 24, 0x4c);
    CHECK(memcmp(&actual, &expected, sizeof(actual)) == 0);
    CHECK(memcmp(glyphs, before, sizeof(before)) == 0);
    return 0;
}

static int test_preclear_and_zero_colors(void) {
    reset();
    glyphs[0] = 0x80;
    fd2_font_draw_glyph(&actual.vga, &font, 0, 10, 20, 0xcd, 0x4c, 7);
    expected_rect(10, 20, 16, 16, 7);
    expected_pixel(10, 20, 0xcd);
    expected_pixel(9, 21, 0x4c);
    expected_pixel(10, 21, 0x4c); /* 不能被下一行未置位像素的bg抹掉。 */
    CHECK(memcmp(&actual, &expected, sizeof(actual)) == 0);

    reset();
    glyphs[0] = 0x80;
    fd2_font_draw_glyph(&actual.vga, &font, 0, 10, 20, 0xcd, 0, -1);
    expected_pixel(10, 20, 0xcd);
    expected_pixel(9, 21, 0);
    expected_pixel(10, 21, 0);
    CHECK(memcmp(&actual, &expected, sizeof(actual)) == 0);

    reset();
    glyphs[0] = 0x80;
    fd2_font_draw_glyph(&actual.vga, &font, 0, 10, 20, 0, 0x4c, 0);
    expected_rect(10, 20, 16, 16, 0); /* 保留SDL接口bg=0的显式黑底。 */
    expected_pixel(9, 21, 0x4c);
    expected_pixel(10, 21, 0x4c);
    CHECK(memcmp(&actual, &expected, sizeof(actual)) == 0);
    return 0;
}

static int test_glyph10_and_invalid_inputs(void) {
    reset();
    memset(glyphs + 10 * 0x20, 0xff, 0x20);
    fd2_font_draw_glyph(&actual.vga, &font, 10, 10, 20, 0xcd, 0x4c, 7);
    expected_rect(10, 20, 16, 16, 7);
    CHECK(memcmp(&actual, &expected, sizeof(actual)) == 0);

    reset();
    memset(glyphs + 10 * 0x20, 0xff, 0x20);
    fd2_font_draw_glyph(&actual.vga, &font, 10, 10, 20, 0xcd, 0x4c, -1);
    CHECK(memcmp(&actual, &expected, sizeof(actual)) == 0);
    fd2_font_draw_glyph(&actual.vga, &font, 11, 10, 20, 0xcd, 0x4c, 7);
    fd2_font_draw_glyph(&actual.vga, &font, UINT16_MAX, 10, 20, 0xcd, 0x4c, 7);
    fd2_font_draw_glyph(&actual.vga, NULL, 0, 10, 20, 0xcd, 0x4c, 7);
    fd2_font empty = {0};
    fd2_font_draw_glyph(&actual.vga, &empty, 0, 10, 20, 0xcd, 0x4c, 7);
    fd2_font_draw_glyph(NULL, &font, 0, 10, 20, 0xcd, 0x4c, 7);
    CHECK(memcmp(&actual, &expected, sizeof(actual)) == 0);
    return 0;
}

typedef struct { int x, y; uint8_t color; } pixel;

static int test_each_destination_is_clipped(void) {
    static const struct {
        int x, y, column;
        size_t count;
        pixel pixels[3];
    } cases[] = {
        {0, 0, 0, 2, {{0, 0, 0xcd}, {0, 1, 0x4c}}},
        {319, 0, 0, 3, {{319, 0, 0xcd}, {318, 1, 0x4c}, {319, 1, 0x4c}}},
        {320, 0, 0, 1, {{319, 1, 0x4c}}},
        {10, -1, 0, 2, {{9, 0, 0x4c}, {10, 0, 0x4c}}},
        {0, 199, 0, 1, {{0, 199, 0xcd}}},
        {320, -1, 0, 1, {{319, 0, 0x4c}}},
        {-1, 10, 0, 0, {{0, 0, 0}}},
        {-15, 10, 15, 2, {{0, 10, 0xcd}, {0, 11, 0x4c}}},
        {320, 200, 0, 0, {{0, 0, 0}}},
        {INT_MAX, INT_MAX, 15, 0, {{0, 0, 0}}},
        {INT_MIN, INT_MIN, 0, 0, {{0, 0, 0}}},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        reset();
        uint16_t bits = (uint16_t)(0x8000u >> cases[i].column);
        glyphs[0] = (uint8_t)(bits >> 8);
        glyphs[1] = (uint8_t)bits;
        fd2_font_draw_glyph(&actual.vga, &font, 0, cases[i].x, cases[i].y,
                            0xcd, 0x4c, -1);
        for (size_t j = 0; j < cases[i].count; j++) {
            const pixel *p = &cases[i].pixels[j];
            expected_pixel(p->x, p->y, p->color);
        }
        CHECK(memcmp(&actual, &expected, sizeof(actual)) == 0);
    }
    reset();
    glyphs[0] = 0x80;
    fd2_font_draw_glyph(&actual.vga, &font, 0, 316, 196, 0xcd, 0x4c, 9);
    expected_rect(316, 196, 4, 4, 9);
    expected_pixel(316, 196, 0xcd);
    expected_pixel(315, 197, 0x4c);
    expected_pixel(316, 197, 0x4c);
    CHECK(memcmp(&actual, &expected, sizeof(actual)) == 0);
    return 0;
}

int main(void) {
    if (test_shadow_direction_and_transparency() ||
        test_preclear_and_zero_colors() ||
        test_glyph10_and_invalid_inputs() ||
        test_each_destination_is_clipped())
        return 1;
    puts("font_test: ok");
    return 0;
}
