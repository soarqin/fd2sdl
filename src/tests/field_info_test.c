#include <stdio.h>
#include <stdlib.h>
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

typedef struct {
    fd2_field_info_assets assets;
    fd2_font font;
    fd2_text_entry text;
    fd2_field_units units;
    fd2_field_overview data;
    uint8_t glyphs[64];
    uint8_t tokens[8];
    uint16_t offsets[599];
} overview_fixture;

static int make_image(fd2_image *image, int width, int height, uint8_t color) {
    image->pixels = malloc((size_t)width * (size_t)height);
    if (!image->pixels) return -1;
    image->width = width;
    image->height = height;
    memset(image->pixels, color, (size_t)width * (size_t)height);
    return 0;
}

static int init_overview_fixture(overview_fixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    const int sizes[4][2] = {{102, 17}, {170, 117}, {170, 16}, {63, 15}};
    for (size_t i = 0; i < 4; i++)
        CHECK(make_image(&fixture->assets.overview[i], sizes[i][0], sizes[i][1],
                          (uint8_t)(10u + i)) == 0);
    for (size_t bank = 0; bank < 2; bank++) {
        for (size_t digit = 0; digit < 10; digit++)
            CHECK(make_image(&fixture->assets.digits[bank][digit],
                              digit == 1 ? 5 : 6, 8,
                              (uint8_t)(30u + bank * 20u + digit)) == 0);
    }
    CHECK(make_image(&fixture->assets.two_digit_overflow, 12, 8, 80) == 0);
    CHECK(make_image(&fixture->assets.digit_overflow[1], 18, 8, 81) == 0);
    fixture->assets.ready = 1;
    fixture->glyphs[0] = 0x80;
    fixture->glyphs[32] = 0x40;
    fixture->font = (fd2_font){fixture->glyphs, sizeof(fixture->glyphs), 2};
    fixture->tokens[2] = fixture->tokens[3] = 0xff;
    fixture->tokens[4] = 1;
    fixture->tokens[6] = fixture->tokens[7] = 0xff;
    fixture->offsets[598] = 4;
    fixture->text = (fd2_text_entry){fixture->tokens, sizeof(fixture->tokens),
                                     fixture->offsets, 599};
    fixture->data = (fd2_field_overview){0, 1, 0, &fixture->units};
    return 0;
}

static int draw_overview(fd2_vga *vga, const overview_fixture *fixture) {
    return fd2_field_overview_draw(vga, &fixture->assets, &fixture->font,
                                    &fixture->text, &fixture->data);
}

static int check_number(const fd2_vga *vga, int x, int y,
                         const char *digits, uint8_t zero_color) {
    for (size_t i = 0; digits[i]; i++)
        CHECK(vga->framebuffer[y * VGA_STRIDE + x + (int)i * 6] ==
              zero_color + (uint8_t)(digits[i] - '0'));
    return 0;
}

static int test_overview_layout_and_counts(void) {
    overview_fixture fixture;
    CHECK(init_overview_fixture(&fixture) == 0);
    /* START.SAV 已核实的12条阵营／隐藏布局：4玩家、8敌人，其中
     * actor9隐藏。只使用这些已确认的计数相关字段，不复制捕获属性。 */
    fixture.units.count = 12;
    const uint8_t player_ids[] = {0, 9, 4, 30};
    for (size_t i = 0; i < fixture.units.count; i++) {
        fixture.units.items[i].side = i < 4 ? 2 : 0;
        fixture.units.items[i].unit_id = i < 4 ? player_ids[i] : 96;
        fixture.units.items[i].race = i == 3 ? 6 : 1;
    }
    fixture.units.items[9].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    fd2_field_units before = fixture.units;
    fd2_vga vga;
    memset(&vga, 0, sizeof(vga));
    memset(vga.framebuffer, 0x90, sizeof(vga.framebuffer));
    CHECK(draw_overview(&vga, &fixture) == 0);
    CHECK(memcmp(&before, &fixture.units, sizeof(before)) == 0);
    CHECK(vga.framebuffer[19 * VGA_STRIDE + 109] == 10);
    CHECK(vga.framebuffer[37 * VGA_STRIDE + 75] == 11);
    CHECK(vga.framebuffer[155 * VGA_STRIDE + 75] == 12);
    CHECK(vga.framebuffer[172 * VGA_STRIDE + 129] == 13);
    CHECK(vga.framebuffer[37 * VGA_STRIDE + 74] == 0x90);
    CHECK(vga.framebuffer[61 * VGA_STRIDE + 80] == 0xcd);
    CHECK(vga.framebuffer[116 * VGA_STRIDE + 81] == 0xcd);
    CHECK(vga.framebuffer[61 * VGA_STRIDE + 83] == 11); /* 透明背景 */
    CHECK(check_number(&vga, 143, 24, "01", 50) == 0);
    CHECK(check_number(&vga, 188, 24, "001", 50) == 0);
    CHECK(check_number(&vga, 120, 159, "07", 50) == 0);
    CHECK(check_number(&vga, 182, 159, "04", 50) == 0);
    CHECK(check_number(&vga, 228, 159, "00", 50) == 0);
    CHECK(check_number(&vga, 140, 176, "00000000", 30) == 0);

    fixture.units.count = 8;
    memset(fixture.units.items, 0, sizeof(fixture.units.items));
    fixture.units.items[0].side = 2; /* HP=0但可见，仍计数 */
    fixture.units.items[1].side = 2;
    fixture.units.items[1].flags = FD2_FIELD_UNIT_FLAG_ACTED |
                                   FD2_FIELD_UNIT_FLAG_AI_INELIGIBLE;
    fixture.units.items[2].side = 2;
    fixture.units.items[2].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    fixture.units.items[3].side = 2;
    fixture.units.items[3].unit_id = 121;
    fixture.units.items[4].side = 2;
    fixture.units.items[4].race = 10;
    fixture.units.items[5].side = 0;
    fixture.units.items[6].side = 1;
    fixture.units.items[7].side = 3;
    CHECK(draw_overview(&vga, &fixture) == 0);
    CHECK(check_number(&vga, 120, 159, "01", 50) == 0);
    CHECK(check_number(&vga, 182, 159, "02", 50) == 0);
    CHECK(check_number(&vga, 228, 159, "01", 50) == 0);

    fd2_field_info_assets_close(&fixture.assets);
    CHECK(fixture.assets.ready == 0);
    for (size_t i = 0; i < 4; i++)
        CHECK(fixture.assets.overview[i].pixels == NULL &&
              fixture.assets.overview[i].width == 0);
    fd2_field_info_assets_close(&fixture.assets);
    return 0;
}

static int test_overview_resource_numbers(void) {
    overview_fixture fixture;
    CHECK(init_overview_fixture(&fixture) == 0);
    fd2_vga vga = {0};
    const struct { uint32_t value; const char *digits; } cases[] = {
        {0u, "00000000"}, {99999999u, "99999999"},
        {100000000u, "10000000"}, {123456789u, "12345678"},
        {INT32_MAX, "21474836"}, {0x80000000u, "00000000"},
        {UINT32_MAX, "00000000"}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fixture.data.resource_value = cases[i].value;
        CHECK(draw_overview(&vga, &fixture) == 0);
        CHECK(check_number(&vga, 140, 176, cases[i].digits, 30) == 0);
    }
    fixture.data.turn_number = 1000;
    CHECK(draw_overview(&vga, &fixture) == 0);
    CHECK(vga.framebuffer[24 * VGA_STRIDE + 188] == 81);
    fixture.data.turn_number = UINT32_MAX;
    CHECK(draw_overview(&vga, &fixture) == 0);
    CHECK(check_number(&vga, 188, 24, "000", 50) == 0);
    fd2_field_info_assets_close(&fixture.assets);
    return 0;
}

static int overview_rejects_unchanged(fd2_vga *vga,
                                      const overview_fixture *fixture) {
    uint8_t before[VGA_W * VGA_H];
    memcpy(before, vga->framebuffer, sizeof(before));
    CHECK(draw_overview(vga, fixture) == -1);
    CHECK(memcmp(before, vga->framebuffer, sizeof(before)) == 0);
    return 0;
}

static int test_overview_rejects_before_drawing(void) {
    overview_fixture fixture;
    CHECK(init_overview_fixture(&fixture) == 0);
    fd2_vga vga = {0};
    memset(vga.framebuffer, 0xa5, sizeof(vga.framebuffer));
    for (size_t i = 0; i < 4; i++) {
        fd2_image image = fixture.assets.overview[i];
        fixture.assets.overview[i].pixels = NULL;
        CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
        fixture.assets.overview[i] = image;
        fixture.assets.overview[i].width--;
        CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
        fixture.assets.overview[i] = image;
    }
    fixture.assets.ready = 0;
    CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
    fixture.assets.ready = 1;
    uint8_t *pixels = fixture.assets.digits[0][9].pixels;
    fixture.assets.digits[0][9].pixels = NULL;
    CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
    fixture.assets.digits[0][9].pixels = pixels;
    fixture.font.glyphs = NULL;
    CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
    fixture.font.glyphs = fixture.glyphs;
    fixture.font.size = 31;
    CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
    fixture.font.size = sizeof(fixture.glyphs);
    fixture.text.fragment_count = 598; /* 第二段缺失也不能先画第一段 */
    CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
    fixture.text.fragment_count = 599;
    fixture.offsets[598] = 10;
    CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
    fixture.offsets[598] = 4;
    fixture.tokens[4] = 2; /* 第二段字形越界 */
    CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
    fixture.tokens[4] = 1;
    fixture.tokens[6] = fixture.tokens[7] = 0; /* 第二段没有结束符 */
    CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
    fixture.tokens[6] = fixture.tokens[7] = 0xff;
    fixture.text.data = NULL;
    CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
    fixture.text.data = fixture.tokens;
    fixture.data.stage = 1;
    CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
    fixture.data.stage = 0;
    fixture.units.count = FD2_FIELD_MAX_UNITS + 1u;
    CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
    fd2_field_info_assets_close(&fixture.assets);
    return 0;
}

static int test_text_fragment_span_rejection(void) {
    const uint8_t data[] = {0, 1, 2, 3, 4};
    uint16_t offsets[] = {0, 6};
    fd2_text_entry text = {data, 4, offsets, 2};
    const uint8_t *tokens = data + 2;
    size_t count = 123;
    CHECK(fd2_text_entry_get_fragment(&text, 0, &tokens, &count) == -1);
    CHECK(tokens == data + 2 && count == 123);
    /* 公共接口只要求span长度偶数；不额外要求绝对offset偶数。 */
    offsets[0] = 1;
    offsets[1] = 3;
    text.size = sizeof(data);
    CHECK(fd2_text_entry_get_fragment(&text, 0, &tokens, &count) == 0);
    CHECK(tokens == data + 1 && count == 1);
    return 0;
}

int main(void) {
    CHECK(test_detail_transition() == 0);
    CHECK(test_item_table() == 0);
    CHECK(test_panel_layout() == 0);
    CHECK(test_overview_layout_and_counts() == 0);
    CHECK(test_overview_resource_numbers() == 0);
    CHECK(test_overview_rejects_before_drawing() == 0);
    CHECK(test_text_fragment_span_rejection() == 0);
    puts("field_info_test: ok");
    return 0;
}
