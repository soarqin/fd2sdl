#include <stdio.h>
#include <string.h>

#include "field_info.h"
#include "field_item.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s (%s:%d)\n", \
                #expr, __FILE__, __LINE__); \
        return -1; \
    } \
} while (0)

static int test_detail_transition(void) {
    uint8_t background[VGA_W * VGA_H];
    uint8_t detail[VGA_W * VGA_H];
    uint8_t dst[VGA_W * VGA_H];
    memset(background, 0x11, sizeof(background));
    for (int y = 0; y < VGA_H; y++)
        for (int x = 0; x < VGA_W; x++)
            detail[y * VGA_STRIDE + x] =
                (uint8_t)(0x40u + ((unsigned)x + (unsigned)y * 3u) % 0xb0u);

#define PIX(buf, x, y) ((buf)[(y) * VGA_STRIDE + (x)])
    fd2_field_detail_transition_frame(dst, detail, background, 11);
    CHECK(PIX(dst, 0, 7) == PIX(detail, 80, 7));
    CHECK(PIX(dst, 10, 92) == PIX(detail, 90, 92));
    CHECK(PIX(dst, 11, 7) == 0x11 && PIX(dst, 0, 93) == 0x11);

    fd2_field_detail_transition_frame(dst, detail, background, 8);
    CHECK(PIX(dst, 0, 7) == PIX(detail, 32, 7));
    CHECK(PIX(dst, 58, 92) == PIX(detail, 90, 92));
    CHECK(PIX(dst, 59, 7) == 0x11);
    CHECK(PIX(dst, 92, 0) == PIX(detail, 92, 80));
    CHECK(PIX(dst, 314, 12) == PIX(detail, 314, 92));
    CHECK(PIX(dst, 92, 13) == 0x11);

    fd2_field_detail_transition_frame(dst, detail, background, 5);
    CHECK(PIX(dst, 5, 7) == PIX(detail, 5, 7));
    CHECK(PIX(dst, 90, 92) == PIX(detail, 90, 92));
    CHECK(PIX(dst, 92, 60) == PIX(detail, 92, 92));
    CHECK(PIX(dst, 92, 61) == 0x11);
    CHECK(PIX(dst, 5, 174) == PIX(detail, 5, 94));
    CHECK(PIX(dst, 314, 199) == PIX(detail, 314, 119));

    fd2_field_detail_transition_frame(dst, detail, background, 0);
    CHECK(PIX(dst, 5, 7) == PIX(detail, 5, 7));
    CHECK(PIX(dst, 314, 92) == PIX(detail, 314, 92));
    CHECK(PIX(dst, 5, 94) == PIX(detail, 5, 94));
    CHECK(PIX(dst, 314, 195) == PIX(detail, 314, 195));
    CHECK(PIX(dst, 4, 94) == 0x11 && PIX(dst, 315, 195) == 0x11);
    for (int phase = 0; phase < 12; phase++) {
        int opening_sfx = fd2_field_detail_sfx_for_phase(1, phase);
        int closing_sfx = fd2_field_detail_sfx_for_phase(0, phase);
        CHECK(opening_sfx == ((phase == 11 || phase == 5) ? 5 : -1));
        CHECK(closing_sfx == ((phase == 0 || phase == 7) ? 6 : -1));
    }
#undef PIX
    return 0;
}

static int test_item_table(void) {
    const uint8_t *sword = fd2_field_item_record_get(0);
    const uint8_t *armor = fd2_field_item_record_get(132);
    const uint8_t *herb = fd2_field_item_record_get(192);
    CHECK(sword && sword[0] == 1 &&
          fd2_field_item_i16(sword, 1) == 10 &&
          fd2_field_item_i16(sword, 3) == 95);
    CHECK(armor && armor[0] == 0x16 &&
          fd2_field_item_i16(armor, 5) == 8);
    CHECK(herb && herb[0] == 0x20 && herb[0x0d] == 5 &&
          fd2_field_item_i16(herb, 0x0e) == 40);
    CHECK(fd2_field_item_record_get(214) != NULL);
    CHECK(fd2_field_item_record_get(215) == NULL);
    return 0;
}

static int test_panel_layout(void) {
    fd2_field_info_assets assets = {0};
    uint8_t panel[69 * 34];
    uint8_t plus = 20, minus = 21;
    uint8_t digit_pixels[3][10];
    uint8_t terrain_pixels[24 * 24];
    uint8_t unit_pixels[24 * 24];
    memset(panel, 9, sizeof(panel));
    memset(terrain_pixels, 3, sizeof(terrain_pixels));
    memset(unit_pixels, 4, sizeof(unit_pixels));
    for (size_t i = 0; i < 10; i++) {
        digit_pixels[0][i] = (uint8_t)(30 + i);
        digit_pixels[1][i] = (uint8_t)(50 + i);
        digit_pixels[2][i] = (uint8_t)(70 + i);
    }
    assets.panel = (fd2_image){69, 34, panel};
    assets.signs[0] = (fd2_image){1, 1, &plus};
    assets.signs[1] = (fd2_image){1, 1, &minus};
    for (size_t bank = 0; bank < 3; bank++)
        for (size_t i = 0; i < 10; i++)
            assets.digits[bank][i] =
                (fd2_image){1, 1, &digit_pixels[bank][i]};
    assets.ready = 1;
    fd2_image terrain = {24, 24, terrain_pixels};
    fd2_image unit = {24, 24, unit_pixels};
    fd2_vga vga;
    memset(&vga, 0, sizeof(vga));

    fd2_field_info_draw(&vga, &assets, 0, &terrain, NULL, 0, 0, 5, -10);
    CHECK(vga.framebuffer[161 * VGA_STRIDE + 5] == 9);
    CHECK(vga.framebuffer[166 * VGA_STRIDE + 11] == 3);
    CHECK(vga.framebuffer[169 * VGA_STRIDE + 48] == plus);
    CHECK(vga.framebuffer[169 * VGA_STRIDE + 56] == 30);
    CHECK(vga.framebuffer[169 * VGA_STRIDE + 62] == 35);
    CHECK(vga.framebuffer[180 * VGA_STRIDE + 48] == minus);
    CHECK(vga.framebuffer[180 * VGA_STRIDE + 56] == 31);
    CHECK(vga.framebuffer[180 * VGA_STRIDE + 62] == 30);

    memset(&vga, 0, sizeof(vga));
    fd2_field_info_draw(&vga, &assets, 1, &terrain, &unit, 42, 42, 5, 0);
    CHECK(vga.framebuffer[161 * VGA_STRIDE + 246] == 9);
    CHECK(vga.framebuffer[166 * VGA_STRIDE + 252] == 4);
    CHECK(vga.framebuffer[182 * VGA_STRIDE + 255] == 30);
    CHECK(vga.framebuffer[182 * VGA_STRIDE + 261] == 34);
    CHECK(vga.framebuffer[182 * VGA_STRIDE + 267] == 32);

    memset(&vga, 0, sizeof(vga));
    fd2_field_info_draw(&vga, &assets, 1, &terrain, &unit, 41, 42, 5, 0);
    CHECK(vga.framebuffer[182 * VGA_STRIDE + 255] == 50);
    CHECK(vga.framebuffer[182 * VGA_STRIDE + 261] == 54);
    CHECK(vga.framebuffer[182 * VGA_STRIDE + 267] == 51);
    return 0;
}

int main(void) {
    CHECK(test_detail_transition() == 0);
    CHECK(test_item_table() == 0);
    CHECK(test_panel_layout() == 0);
    puts("field_info_test: ok");
    return 0;
}
