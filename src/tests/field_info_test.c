/* field_unit_magic_list_draw @0x1ceed（原登记sub_1CEED）列表与现有详情资源回归。
 * field_unit_summary_draw @0x18c6d（原登记sub_18C6D）含四张原版WGC资源参考。
 * field_player_magic_list_select_step @0x1d51d（原登记sub_1D51D）仅作
 * selected索引依据；本测试不调用选择／施法，不创建SDL窗口。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_info.h"
#include "field_item.h"
#include "field_magic.h"
#include "portrait.h"

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
    uint8_t tokens[16];
    uint16_t offsets[601];
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
    fixture->text = (fd2_text_entry){fixture->tokens, 8,
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

static int test_stage1_overview_conditions(void) {
    overview_fixture fixture;
    CHECK(init_overview_fixture(&fixture) == 0);
    fixture.data.stage = 1;
    fixture.text.size = sizeof(fixture.tokens);
    fixture.text.fragment_count = 601;
    fixture.offsets[599] = 8;
    fixture.offsets[600] = 12;
    /* 第二关两段字形与第一关相反，不能复用597/598条件。 */
    fixture.tokens[8] = 1;
    fixture.tokens[10] = fixture.tokens[11] = 0xff;
    fixture.tokens[14] = fixture.tokens[15] = 0xff;
    fixture.units.count = 3;
    fixture.units.items[0].side = 0;
    fixture.units.items[1].side = 1;
    fixture.units.items[2].side = 2;
    fd2_field_units before = fixture.units;
    fd2_vga vga = {0};
    CHECK(draw_overview(&vga, &fixture) == 0);
    CHECK(check_number(&vga, 143, 24, "02", 50) == 0);
    CHECK(vga.framebuffer[61 * VGA_STRIDE + 81] == 0xcd);
    CHECK(vga.framebuffer[61 * VGA_STRIDE + 80] != 0xcd);
    CHECK(vga.framebuffer[116 * VGA_STRIDE + 80] == 0xcd);
    CHECK(vga.framebuffer[116 * VGA_STRIDE + 81] != 0xcd);
    CHECK(check_number(&vga, 120, 159, "01", 50) == 0);
    CHECK(check_number(&vga, 182, 159, "01", 50) == 0);
    CHECK(check_number(&vga, 228, 159, "01", 50) == 0);
    CHECK(!memcmp(&before, &fixture.units, sizeof(before)));
    fixture.text.fragment_count = 600;
    CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
    fixture.text.fragment_count = 601;
    fixture.tokens[12] = 2;
    CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
    fixture.tokens[12] = 0;
    fixture.data.stage = 2;
    CHECK(overview_rejects_unchanged(&vga, &fixture) == 0);
    fd2_field_info_assets_close(&fixture.assets);
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

typedef struct {
    fd2_archive other, text_archive;
    fd2_field_info_assets assets;
    fd2_font font;
    fd2_text_entry text;
} magic_fixture;

static void close_magic_fixture(magic_fixture *fixture) {
    fd2_field_info_assets_close(&fixture->assets);
    fd2_text_entry_close(&fixture->text);
    fd2_archive_close(&fixture->text_archive);
    fd2_archive_close(&fixture->other);
}

static int open_magic_fixture(magic_fixture *fixture) {
    memset(fixture, 0, sizeof(*fixture));
    if (fd2_archive_open(&fixture->other, "original_game/FDOTHER.DAT") != 0 ||
        fd2_archive_open(&fixture->text_archive, "original_game/FDTXT.DAT") != 0 ||
        fd2_field_info_assets_open(&fixture->assets, &fixture->other) != 0 ||
        fd2_font_open_fdother(&fixture->font, &fixture->other, 4) != 0 ||
        fd2_text_entry_open_entry(&fixture->text, &fixture->text_archive, 0) != 0) {
        close_magic_fixture(fixture);
        return -1;
    }
    return 0;
}

static uint32_t test_u32(const uint8_t *data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) | ((uint32_t)data[3] << 24);
}

static int test_magic_asset_owner(void) {
    fd2_archive other = {0};
    fd2_field_info_assets assets = {0};
    const uint8_t *sheet;
    size_t size;
    CHECK(fd2_archive_open(&other, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_archive_get(&other, 5, &sheet, &size) == 0);
    uint32_t start = test_u32(sheet + 6 + 92 * 4);
    uint32_t end = test_u32(sheet + 6 + 93 * 4);
    CHECK(start < end && end <= size && end - start == 164);
    CHECK(sheet[start] == 20 && sheet[start + 1] == 0 &&
          sheet[start + 2] == 8 && sheet[start + 3] == 0);
    CHECK(fd2_field_info_assets_open(&assets, &other) == 0);
    fd2_image summary = {0};
    size_t summary_start = test_u32(sheet + 6 + 22 * 4);
    size_t summary_end = test_u32(sheet + 6 + 23 * 4);
    CHECK(fd2_lmi_decode_image(&summary, sheet + summary_start,
                               summary_end - summary_start) == 0);
    CHECK(assets.unit_summary.width == 149 && assets.unit_summary.height == 42);
    CHECK(assets.unit_summary.pixels != summary.pixels &&
          !memcmp(assets.unit_summary.pixels, summary.pixels, 149u * 42u));
    const size_t bar_frames[8] = {23, 26, 24, 25, 27, 28, 29, 30};
    for (size_t i = 0; i < 8; i++) {
        size_t offset = test_u32(sheet + 6 + bar_frames[i] * 4);
        CHECK(assets.bars[i].width == 1 && assets.bars[i].height == 5);
        CHECK(assets.bars[i].pixels != sheet + offset + 4);
        CHECK(!memcmp(assets.bars[i].pixels, sheet + offset + 4, 5));
    }
    CHECK(assets.magic_cost.width == 20 && assets.magic_cost.height == 8);
    CHECK(assets.magic_cost.pixels != sheet + start + 4);
    CHECK(memcmp(assets.magic_cost.pixels, sheet + start + 4, 160) == 0);
    uint8_t expected[160];
    memcpy(expected, sheet + start + 4, sizeof(expected));
    uint8_t *broken = malloc(size);
    CHECK(broken != NULL);
    memcpy(broken, sheet, size);
    /* 像素长度仍合法，让raw decoder先分配；尺寸门失败须回收整owner。 */
    broken[start] = 40;
    broken[start + 2] = 4;
    CHECK(fd2_field_info_assets_open_mem(&assets, broken, size) == -1);
    for (size_t i = 0; i < sizeof(assets); i++) CHECK(((uint8_t *)&assets)[i] == 0);
    CHECK(fd2_field_info_assets_open(&assets, &other) == 0);
    memcpy(broken, sheet, size);
    broken[summary_start] = 150;
    CHECK(fd2_field_info_assets_open_mem(&assets, broken, size) == -1);
    for (size_t i = 0; i < sizeof(assets); i++) CHECK(((uint8_t *)&assets)[i] == 0);
    CHECK(fd2_field_info_assets_open(&assets, &other) == 0);
    /* 最后一张bar已分配后才触发尺寸拒绝，先前所有owner必须一并释放。 */
    memcpy(broken, sheet, size);
    uint32_t late_bar = test_u32(sheet + 6 + 30 * 4);
    broken[late_bar] = 5; broken[late_bar + 2] = 1;
    CHECK(fd2_field_info_assets_open_mem(&assets, broken, size) == -1);
    for (size_t i = 0; i < sizeof(assets); i++) CHECK(((uint8_t *)&assets)[i] == 0);
    CHECK(fd2_field_info_assets_open(&assets, &other) == 0);
    free(broken);
    fd2_archive_close(&other);
    CHECK(memcmp(assets.magic_cost.pixels, expected, sizeof(expected)) == 0);
    CHECK(!memcmp(assets.unit_summary.pixels, summary.pixels, 149u * 42u));
    fd2_image_free(&summary);
    fd2_field_info_assets_close(&assets);
    CHECK(!assets.ready && !assets.magic_cost.pixels && !assets.magic_cost.width &&
          !assets.unit_summary.pixels && !assets.unit_summary.width);
    fd2_field_info_assets_close(&assets);
    return 0;
}

static void magic_bit(fd2_field_unit *unit, unsigned id) {
    unit->data_09_1e[0x11u + id / 8u] |= (uint8_t)(1u << (id % 8u));
}

static void expected_blit(uint8_t *dst, const fd2_image *image,
                           int x, int y, int transparent) {
    for (int row = 0; row < image->height; row++) {
        for (int column = 0; column < image->width; column++) {
            uint8_t value = image->pixels[(size_t)row * (size_t)image->width + (size_t)column];
            if (transparent < 0 || value != (uint8_t)transparent)
                dst[(size_t)(y + row) * VGA_STRIDE + (size_t)(x + column)] = value;
        }
    }
}

static int test_magic_list_real_layout_and_selection(void) {
    static const int positions[12][2] = {
        {18,103}, {18,125}, {18,147}, {18,169},
        {118,103}, {118,125}, {118,147}, {118,169},
        {218,103}, {218,125}, {218,147}, {218,169}
    };
    /* canonical0x619fd的前12项+5；独立固定费用防止错读+6或更换数字bank。 */
    static const unsigned costs[12] = {2,6,20,42,4,15,30,60,24,30,18,45};
    const int selections[] = {-1, 0, 3, 4, 11};
    magic_fixture fixture;
    CHECK(open_magic_fixture(&fixture) == 0);
    fd2_field_unit unit = {0};
    unit.mp = 0; /* 不足MP仍显示同一列表，不在纯绘制层改变名称颜色。 */
    unit.mp_max = 8;
    for (unsigned i = 0; i < 12; i++) magic_bit(&unit, i);
    fd2_field_unit before_unit = unit;
    fd2_field_info_assets before_assets = fixture.assets;
    for (size_t selected = 0; selected < sizeof(selections) / sizeof(selections[0]); selected++) {
        fd2_vga actual = {0}, expected = {0};
        memset(actual.framebuffer, 0xa5, sizeof(actual.framebuffer));
        memset(actual.palette, 11, sizeof(actual.palette));
        memset(actual.dac, 17, sizeof(actual.dac));
        memset(&actual.input, 23, sizeof(actual.input));
        actual.field_water_phase = 7;
        actual.frame_deadline_ns = 1234;
        fd2_vga before_vga = actual;
        memcpy(expected.framebuffer, actual.framebuffer, sizeof(actual.framebuffer));
        expected_blit(expected.framebuffer, &fixture.assets.detail_bottom, 5, 94, -1);
        for (unsigned i = 0; i < 12; i++) {
            const uint8_t *tokens;
            size_t count;
            CHECK(fd2_text_entry_get_fragment(&fixture.text, 441u + i, &tokens, &count) == 0);
            CHECK(count == 4 && fd2_text_token_at(tokens, 3) == 0xffffu);
            for (size_t glyph = 0; glyph < 3; glyph++) {
                fd2_font_draw_glyph(&expected, &fixture.font, fd2_text_token_at(tokens, glyph),
                    positions[i][0] + (int)glyph * 16, positions[i][1],
                    selections[selected] == (int)i ? 201u : 205u, 76, -1);
            }
            int x = positions[i][0], y = positions[i][1];
            expected_blit(expected.framebuffer, &fixture.assets.magic_cost, x + 50, y + 5, -1);
            expected_blit(expected.framebuffer, &fixture.assets.digits[1][costs[i] / 10], x + 73, y + 5, 0);
            expected_blit(expected.framebuffer, &fixture.assets.digits[1][costs[i] % 10], x + 79, y + 5, 0);
        }
        CHECK(fd2_field_magic_list_draw(&actual, &fixture.assets, &fixture.font,
                                        &fixture.text, &unit, selections[selected]) == 0);
        CHECK(memcmp(actual.framebuffer, expected.framebuffer, sizeof(actual.framebuffer)) == 0);
        memcpy(actual.framebuffer, before_vga.framebuffer, sizeof(actual.framebuffer));
        CHECK(memcmp(&actual, &before_vga, sizeof(actual)) == 0);
        CHECK(memcmp(&unit, &before_unit, sizeof(unit)) == 0);
        CHECK(memcmp(&fixture.assets, &before_assets, sizeof(before_assets)) == 0);
    }
    close_magic_fixture(&fixture);
    return 0;
}

static int test_magic_empty_name_and_opaque_cost(void) {
    magic_fixture fixture;
    CHECK(open_magic_fixture(&fixture) == 0);
    fd2_field_unit unit = {0};
    magic_bit(&unit, 31);
    fd2_vga vga = {0};
    memset(vga.framebuffer, 0xa5, sizeof(vga.framebuffer));
    CHECK(fd2_field_magic_record_get(31)[5] == 26);
    CHECK(fd2_field_magic_list_draw(&vga, &fixture.assets, &fixture.font,
                                    &fixture.text, &unit, 0) == 0);
    uint8_t expected[VGA_W * VGA_H];
    memset(expected, 0xa5, sizeof(expected));
    expected_blit(expected, &fixture.assets.detail_bottom, 5, 94, -1);
    expected_blit(expected, &fixture.assets.magic_cost, 68, 108, -1);
    expected_blit(expected, &fixture.assets.digits[1][2], 91, 108, 0);
    expected_blit(expected, &fixture.assets.digits[1][6], 97, 108, 0);
    CHECK(memcmp(expected, vga.framebuffer, sizeof(expected)) == 0);
    memset(&unit, 0, sizeof(unit));
    magic_bit(&unit, 0);
    fixture.assets.magic_cost.pixels[0] = 0;
    CHECK(fd2_field_magic_list_draw(&vga, &fixture.assets, &fixture.font,
                                    &fixture.text, &unit, -1) == 0);
    CHECK(vga.framebuffer[108 * VGA_STRIDE + 68] == 0);
    memcpy(expected, vga.framebuffer, sizeof(expected));
    CHECK(fd2_field_magic_list_draw(&vga, &fixture.assets, &fixture.font,
                                    &fixture.text, &unit, 0) == 0);
    size_t changed = 0;
    for (size_t i = 0; i < sizeof(expected); i++) {
        if (expected[i] != vga.framebuffer[i]) {
            CHECK(expected[i] == 205 && vga.framebuffer[i] == 201);
            changed++;
        }
    }
    CHECK(changed == 150); /* 已校准原版ID0的选中前景写集。 */
    close_magic_fixture(&fixture);
    return 0;
}

static int magic_rejects_unchanged(fd2_vga *vga, const magic_fixture *fixture,
                                   const fd2_field_unit *unit, int selected) {
    fd2_vga before = *vga;
    fd2_field_unit before_unit = {0};
    if (unit) before_unit = *unit;
    CHECK(fd2_field_magic_list_draw(vga, &fixture->assets, &fixture->font,
                                    &fixture->text, unit, selected) == -1);
    CHECK(memcmp(vga, &before, sizeof(before)) == 0);
    if (unit) CHECK(memcmp(unit, &before_unit, sizeof(before_unit)) == 0);
    return 0;
}

static int test_magic_rejects_before_drawing(void) {
    magic_fixture fixture;
    CHECK(open_magic_fixture(&fixture) == 0);
    fd2_field_unit unit = {0};
    fd2_vga vga = {0};
    memset(vga.framebuffer, 0xa5, sizeof(vga.framebuffer));
    CHECK(magic_rejects_unchanged(&vga, &fixture, &unit, 0) == 0);
    CHECK(fd2_field_magic_list_draw(&vga, &fixture.assets, &fixture.font,
                                    &fixture.text, &unit, -1) == 0);
    uint8_t empty[VGA_W * VGA_H];
    memset(empty, 0xa5, sizeof(empty));
    expected_blit(empty, &fixture.assets.detail_bottom, 5, 94, -1);
    CHECK(memcmp(vga.framebuffer, empty, sizeof(empty)) == 0);
    for (unsigned i = 0; i < 12; i++) magic_bit(&unit, i);
    CHECK(magic_rejects_unchanged(&vga, &fixture, &unit, -2) == 0);
    CHECK(magic_rejects_unchanged(&vga, &fixture, &unit, 12) == 0);
    magic_bit(&unit, 12);
    CHECK(magic_rejects_unchanged(&vga, &fixture, &unit, 0) == 0);
    memset(unit.data_09_1e + 0x11, 0xff, 5);
    CHECK(fd2_field_unit_magic_list_build(&unit, NULL, 0) == 40);
    CHECK(magic_rejects_unchanged(&vga, &fixture, &unit, 0) == 0);
    for (unsigned id = 36; id < 40; id++) {
        memset(&unit, 0, sizeof(unit));
        magic_bit(&unit, 0);
        magic_bit(&unit, id);
        CHECK(magic_rejects_unchanged(&vga, &fixture, &unit, 0) == 0);
    }
    memset(&unit, 0, sizeof(unit));
    for (unsigned i = 0; i < 12; i++) magic_bit(&unit, i);
    CHECK(magic_rejects_unchanged(&vga, &fixture, NULL, 0) == 0);
    uint8_t *pixels = fixture.assets.magic_cost.pixels;
    fixture.assets.magic_cost.pixels = NULL;
    CHECK(magic_rejects_unchanged(&vga, &fixture, &unit, 0) == 0);
    fixture.assets.magic_cost.pixels = pixels;
    fixture.assets.magic_cost.width = 19;
    CHECK(magic_rejects_unchanged(&vga, &fixture, &unit, 0) == 0);
    fixture.assets.magic_cost.width = 20;
    fixture.assets.detail_bottom.height = 98;
    CHECK(magic_rejects_unchanged(&vga, &fixture, &unit, 0) == 0);
    fixture.assets.detail_bottom.height = 99;
    fixture.assets.digits[1][9].height = 7;
    CHECK(magic_rejects_unchanged(&vga, &fixture, &unit, 0) == 0);
    fixture.assets.digits[1][9].height = 8;
    fixture.font.size = 31;
    CHECK(magic_rejects_unchanged(&vga, &fixture, &unit, 0) == 0);
    fixture.font.size = fixture.font.glyph_count * 32u;
    size_t fragments = fixture.text.fragment_count;
    fixture.text.fragment_count = 452; /* 前11项正常，最后一项缺失也不能先画前缀。 */
    CHECK(magic_rejects_unchanged(&vga, &fixture, &unit, 0) == 0);
    fixture.text.fragment_count = fragments;
    uint8_t *bad_text = malloc(fixture.text.size);
    CHECK(bad_text != NULL);
    memcpy(bad_text, fixture.text.data, fixture.text.size);
    const uint8_t *original_text = fixture.text.data;
    fixture.text.data = bad_text;
    size_t last = fixture.text.offsets[452];
    bad_text[last] = (uint8_t)fixture.font.glyph_count;
    bad_text[last + 1] = (uint8_t)(fixture.font.glyph_count >> 8);
    CHECK(magic_rejects_unchanged(&vga, &fixture, &unit, 0) == 0);
    bad_text[last] = 0xfe; bad_text[last + 1] = 0xff;
    CHECK(magic_rejects_unchanged(&vga, &fixture, &unit, 0) == 0);
    fixture.text.data = original_text;
    free(bad_text);
    close_magic_fixture(&fixture);
    return 0;
}

static int test_equipment_attribute_coordinates(void) {
    magic_fixture fixture;
    CHECK(open_magic_fixture(&fixture) == 0);
    fd2_field_unit unit = {0};
    const uint8_t slots[8][2] = {
        {0x40,0}, {0x80,0xff}, {0x40,132}, {0,192},
        {0,0}, {0,132}, {0,192}, {0x80,0xff}
    };
    for (size_t i = 0; i < 8; i++) {
        unit.data_09_1e[1u + 2u * i] = slots[i][0];
        unit.data_09_1e[2u + 2u * i] = slots[i][1];
    }
    fd2_field_unit before = unit;
    fd2_vga actual = {0}, expected = {0};
    fd2_field_detail_draw(&actual, &fixture.assets, &fixture.font,
        &fixture.text, &unit, NULL);
    expected_blit(expected.framebuffer, &fixture.assets.detail_bottom, 5, 94, -1);
    /* 184c0：独立固定的visible位置；raw slot1的空洞不占行，列1从第5项起。 */
    const struct {
        unsigned id, value;
        int name_x, name_y, icon_x, icon_y, number_x;
        size_t type_icon, attribute_icon;
    } rows[] = {
        {0,10,42,103,110,107,135,3,5},
        {132,8,42,125,110,129,135,4,6},
        {192,40,42,147,110,151,135,2,7},
        {0,10,42,169,110,173,135,0,5},
        {132,8,192,103,260,107,285,1,6},
        {192,40,192,125,260,129,285,2,7},
    };
    for (size_t i = 0; i < sizeof(rows) / sizeof(rows[0]); i++) {
        expected_blit(expected.framebuffer, &fixture.assets.detail_icons[rows[i].type_icon],
            rows[i].name_x - 29, rows[i].name_y - 2, 0);
        const uint8_t *tokens;
        size_t count;
        CHECK(fd2_text_entry_get_fragment(&fixture.text, rows[i].id + 181u, &tokens, &count) == 0);
        for (size_t glyph = 0; glyph < count && fd2_text_token_at(tokens, glyph) != 0xffffu; glyph++)
            fd2_font_draw_glyph(&expected, &fixture.font, fd2_text_token_at(tokens, glyph),
                rows[i].name_x + (int)glyph * 16, rows[i].name_y, 205, 76, -1);
        expected_blit(expected.framebuffer, &fixture.assets.detail_icons[rows[i].attribute_icon],
            rows[i].icon_x, rows[i].icon_y, 0);
        unsigned digits[] = {rows[i].value / 100u, rows[i].value / 10u % 10u, rows[i].value % 10u};
        for (size_t digit = 0; digit < 3; digit++)
            expected_blit(expected.framebuffer, &fixture.assets.digits[1][digits[digit]],
                rows[i].number_x + 6 * (int)digit, rows[i].icon_y, 0);
    }
    for (int row = 94; row < 193; row++)
        CHECK(memcmp(actual.framebuffer + row * VGA_STRIDE + 5,
            expected.framebuffer + row * VGA_STRIDE + 5, 310) == 0);
    CHECK(memcmp(&unit, &before, sizeof(unit)) == 0);
    close_magic_fixture(&fixture);
    return 0;
}

static void item_slot(fd2_field_unit *unit, size_t slot, uint8_t flags, uint8_t id) {
    unit->data_09_1e[1u + slot * 2u] = flags;
    unit->data_09_1e[2u + slot * 2u] = id;
}

static void empty_inventory(fd2_field_unit *unit) {
    memset(unit, 0, sizeof(*unit));
    for (size_t i = 0; i < 8; i++) item_slot(unit, i, 0x80, 255);
}

static int fallback_item(unsigned id) {
    /* canonical602ad的215×23表，逐项审计的frame41分支集合；不是未知ID降级。 */
    static const uint8_t ids[] = {88,89,90,91,92,93,94,95,96,100,
        196,197,198,199,200,201,202,203,204,205,208,209,210,211,212,213,214};
    for (size_t i = 0; i < sizeof(ids); i++) if (ids[i] == id) return 1;
    return 0;
}

static int expected_item_list(fd2_vga *expected, const magic_fixture *fixture,
                               const fd2_field_unit *unit, int selected) {
    static const int positions[8][2] = {{42,103},{42,125},{42,147},{42,169},
                                       {192,103},{192,125},{192,147},{192,169}};
    expected_blit(expected->framebuffer, &fixture->assets.detail_bottom, 5, 94, -1);
    size_t visible = 0;
    for (size_t raw = 0; raw < 8; raw++) {
        uint8_t flags = unit->data_09_1e[1u + raw * 2u], id = unit->data_09_1e[2u + raw * 2u];
        if (flags & 0x80) continue;
        const uint8_t *record = fd2_field_item_record_get(id), *tokens;
        size_t glyphs;
        CHECK(record && fd2_text_entry_get_fragment(&fixture->text, 181u + id, &tokens, &glyphs) == 0);
        int x = positions[visible][0], y = positions[visible][1];
        size_t type = (record[0] < 21 ? 0u : record[0] < 32 ? 1u : 2u) + ((flags & 0x40) ? 3u : 0u);
        expected_blit(expected->framebuffer, &fixture->assets.detail_icons[type], x - 29, y - 2, -1);
        for (size_t i = 0; i < glyphs && fd2_text_token_at(tokens, i) != 0xffffu; i++)
            fd2_font_draw_glyph(expected, &fixture->font, fd2_text_token_at(tokens, i),
                x + (int)i * 16, y, selected == (int)raw ? 201u : 205u, 76, -1);
        if (fallback_item(id)) {
            expected_blit(expected->framebuffer, &fixture->assets.digit_overflow[0], x + 68, y + 4, 0);
        } else {
            size_t attribute = record[0] < 21 ? 5u : record[0] < 32 ? 6u : record[13] == 5 ? 7u : 8u;
            size_t offset = record[0] < 21 ? 1u : record[0] < 32 ? 5u : 14u;
            int value = (int16_t)((unsigned)record[offset] | (unsigned)record[offset + 1u] << 8);
            unsigned number = (unsigned)(value < 0 ? 0 : value);
            expected_blit(expected->framebuffer, &fixture->assets.detail_icons[attribute], x + 68, y + 4, -1);
            CHECK(number <= 999u); /* 当前全表极值2..999，无猜测截断。 */
            unsigned digits[] = {number / 100u, number / 10u % 10u, number % 10u};
            for (size_t i = 0; i < 3; i++)
                expected_blit(expected->framebuffer, &fixture->assets.digits[1][digits[i]], x + 93 + 6 * (int)i, y + 4, 0);
        }
        visible++;
    }
    return 0;
}

static int check_item_list(const magic_fixture *fixture, const fd2_field_unit *unit, int selected) {
    fd2_vga actual = {0}, expected = {0};
    memset(actual.framebuffer, 0xa5, sizeof(actual.framebuffer));
    memset(actual.palette, 11, sizeof(actual.palette));
    memset(actual.dac, 17, sizeof(actual.dac));
    memset(&actual.input, 23, sizeof(actual.input));
    actual.field_water_phase = 7; actual.frame_deadline_ns = 1234;
    fd2_vga before = actual;
    fd2_field_unit before_unit = *unit;
    fd2_field_info_assets before_assets = fixture->assets;
    memcpy(expected.framebuffer, actual.framebuffer, sizeof(expected.framebuffer));
    CHECK(expected_item_list(&expected, fixture, unit, selected) == 0);
    CHECK(fd2_field_item_list_draw(&actual, &fixture->assets, &fixture->font, &fixture->text, unit, selected) == 0);
    CHECK(memcmp(actual.framebuffer, expected.framebuffer, sizeof(actual.framebuffer)) == 0);
    memcpy(actual.framebuffer, before.framebuffer, sizeof(actual.framebuffer));
    CHECK(memcmp(&actual, &before, sizeof(actual)) == 0);
    CHECK(memcmp(unit, &before_unit, sizeof(*unit)) == 0);
    CHECK(memcmp(&fixture->assets, &before_assets, sizeof(before_assets)) == 0);
    return 0;
}

static int test_item_list_layout_raw_selection_and_all_records(void) {
    magic_fixture fixture;
    CHECK(open_magic_fixture(&fixture) == 0);
    const size_t counts[] = {1,4,5,8};
    const uint8_t ids[8] = {0,132,192,206,108,201,128,195};
    for (size_t n = 0; n < sizeof(counts) / sizeof(counts[0]); n++) {
        fd2_field_unit unit;
        empty_inventory(&unit);
        for (size_t i = 0; i < counts[n]; i++) item_slot(&unit, i, i < 2 ? 0x40 : 0, ids[i]);
        CHECK(check_item_list(&fixture, &unit, -1) == 0);
        CHECK(check_item_list(&fixture, &unit, 0) == 0);
        CHECK(check_item_list(&fixture, &unit, (int)counts[n] - 1) == 0);
    }
    fd2_field_unit holes;
    empty_inventory(&holes);
    for (size_t raw = 0; raw < 8; raw++)
        if (raw != 1 && raw != 4) item_slot(&holes, raw, raw == 0 ? 0x40 : 0, ids[raw]);
    CHECK(check_item_list(&fixture, &holes, 2) == 0); /* 选raw2，不是visible2。 */
    CHECK(check_item_list(&fixture, &holes, 7) == 0); /* raw7已压到第二列第二行。 */
    for (unsigned id = 0; id < FD2_FIELD_ITEM_COUNT; id++) {
        fd2_field_unit unit;
        empty_inventory(&unit);
        item_slot(&unit, 0, 0, (uint8_t)id);
        CHECK(check_item_list(&fixture, &unit, 0) == 0);
    }
    close_magic_fixture(&fixture);
    return 0;
}

static int test_item_raw_icons_copy_zero_and_fallback_position(void) {
    magic_fixture fixture;
    CHECK(open_magic_fixture(&fixture) == 0);
    const uint8_t *sheet;
    size_t sheet_size;
    CHECK(fd2_archive_get(&fixture.other, 5, &sheet, &sheet_size) == 0);
    for (size_t i = 0; i < 9; i++) {
        uint32_t start = test_u32(sheet + 6u + (59u + i) * 4u);
        uint32_t end = test_u32(sheet + 6u + (60u + i) * 4u);
        const fd2_image *image = &fixture.assets.detail_icons[i];
        size_t bytes = (size_t)image->width * (size_t)image->height;
        CHECK(start < end && end <= sheet_size && end - start == 4u + bytes);
        CHECK(image->pixels != sheet + start + 4u);
        CHECK(memcmp(image->pixels, sheet + start + 4u, bytes) == 0);
    }
    /* RLE的cache0=skip仅在本资源没有实际写0时成立；frame41/52
     * 是18×8溢出图，不能套仅允许6px数字的Python布局门。 */
    for (size_t frame = 41; frame <= 52; frame++) {
        uint32_t start = test_u32(sheet + 6u + frame * 4u);
        uint32_t end = test_u32(sheet + 6u + (frame + 1u) * 4u);
        CHECK(start < end && end <= sheet_size && end - start >= 4u);
        const uint8_t *data = sheet + start;
        unsigned width = (unsigned)data[0] | (unsigned)data[1] << 8;
        unsigned height = (unsigned)data[2] | (unsigned)data[3] << 8;
        size_t source = 4;
        CHECK(height == 8 && width == (frame == 41 || frame == 52 ? 18u : frame == 43 ? 5u : 6u));
        for (unsigned row = 0; row < height; row++) {
            unsigned x = 0;
            while (x < width) {
                CHECK(source < end - start);
                uint8_t control = data[source++];
                unsigned mode = control & 0xc0u, count = (control & 0x3fu) + 1u;
                unsigned span = mode == 0x40u ? count * 2u : count;
                CHECK(span <= width - x);
                if (mode != 0xc0u) {
                    size_t values = mode == 0x80u ? count : 1u;
                    CHECK(source + values <= end - start);
                    for (size_t i = 0; i < values; i++) CHECK(data[source + i] != 0);
                    source += values;
                }
                x += span;
            }
        }
        CHECK(source == end - start);
    }
    fd2_field_unit unit;
    empty_inventory(&unit);
    item_slot(&unit, 0, 0, 192);
    fixture.assets.detail_icons[2].pixels[0] = 0;
    fixture.assets.detail_icons[7].pixels[0] = 0;
    fd2_vga vga = {0};
    memset(vga.framebuffer, 0xa5, sizeof(vga.framebuffer));
    CHECK(fd2_field_item_list_draw(&vga, &fixture.assets, &fixture.font, &fixture.text, &unit, -1) == 0);
    CHECK(vga.framebuffer[101 * 320 + 13] == 0 && vga.framebuffer[107 * 320 + 110] == 0);
    CHECK(check_item_list(&fixture, &unit, 0) == 0);
    item_slot(&unit, 0, 0x40, 192); /* raw位即使不合常规，也不由renderer改写。 */
    CHECK(check_item_list(&fixture, &unit, -1) == 0);
    item_slot(&unit, 0, 0, 201);
    CHECK(check_item_list(&fixture, &unit, -1) == 0);
    /* 失败旧实现没有fallback；真实frame41必须出现在110而非number135。 */
    fd2_vga expected = {0};
    memset(vga.framebuffer, 0xa5, sizeof(vga.framebuffer));
    fd2_field_detail_draw(&vga, &fixture.assets, &fixture.font, &fixture.text, &unit, NULL);
    memcpy(expected.framebuffer, vga.framebuffer, sizeof(expected.framebuffer));
    expected_blit(expected.framebuffer, &fixture.assets.digit_overflow[0], 110, 107, 0);
    CHECK(memcmp(expected.framebuffer, vga.framebuffer, sizeof(expected.framebuffer)) == 0);
    close_magic_fixture(&fixture);
    return 0;
}

static int reject_item_list(magic_fixture *fixture, fd2_field_unit *unit, int selected) {
    fd2_vga vga = {0};
    memset(vga.framebuffer, 0xa5, sizeof(vga.framebuffer));
    fd2_vga before = vga;
    fd2_field_unit before_unit = *unit;
    CHECK(fd2_field_item_list_draw(&vga, &fixture->assets, &fixture->font, &fixture->text, unit, selected) == -1);
    CHECK(memcmp(&vga, &before, sizeof(vga)) == 0 && !memcmp(unit, &before_unit, sizeof(*unit)));
    return 0;
}

static int test_item_list_preflight_leaves_all_state_unchanged(void) {
    magic_fixture fixture;
    CHECK(open_magic_fixture(&fixture) == 0);
    fd2_field_unit unit;
    empty_inventory(&unit);
    CHECK(check_item_list(&fixture, &unit, -1) == 0);
    CHECK(reject_item_list(&fixture, &unit, 0) == 0);
    const uint8_t ids[] = {0,132,192,201,0,132,192,206};
    for (size_t i = 0; i < 8; i++) item_slot(&unit, i, 0, ids[i]);
    CHECK(reject_item_list(&fixture, &unit, -2) == 0);
    CHECK(reject_item_list(&fixture, &unit, 8) == 0);
    item_slot(&unit, 1, 0x80, 255);
    CHECK(reject_item_list(&fixture, &unit, 1) == 0);
    CHECK(check_item_list(&fixture, &unit, 7) == 0);
    item_slot(&unit, 7, 0, 215);
    CHECK(reject_item_list(&fixture, &unit, 0) == 0); /* 未选中的末项也完整检查。 */
    item_slot(&unit, 7, 0, 206);
    fd2_image image = fixture.assets.detail_icons[8];
    fixture.assets.detail_icons[8].height = 7;
    CHECK(reject_item_list(&fixture, &unit, 0) == 0);
    CHECK(reject_item_list(&fixture, &unit, 7) == 0);
    fixture.assets.detail_icons[8] = image;
    image = fixture.assets.digit_overflow[0];
    fixture.assets.digit_overflow[0].pixels = NULL;
    CHECK(reject_item_list(&fixture, &unit, 0) == 0);
    fixture.assets.digit_overflow[0] = image;
    const uint8_t *original_text = fixture.text.data, *tokens;
    size_t count;
    CHECK(fd2_text_entry_get_fragment(&fixture.text, 181u + 206u, &tokens, &count) == 0 && count > 1);
    uint8_t *broken = malloc(fixture.text.size);
    CHECK(broken);
    memcpy(broken, original_text, fixture.text.size);
    size_t offset = (size_t)(tokens - original_text);
    broken[offset] = 1; broken[offset + 1] = 0xff;
    fixture.text.data = broken;
    CHECK(reject_item_list(&fixture, &unit, 0) == 0);
    CHECK(reject_item_list(&fixture, &unit, 7) == 0);
    fixture.text.data = original_text;
    free(broken);
    fd2_vga vga = {0}, before = {0};
    CHECK(fd2_field_item_list_draw(NULL, &fixture.assets, &fixture.font, &fixture.text, &unit, -1) == -1);
    CHECK(fd2_field_item_list_draw(&vga, NULL, &fixture.font, &fixture.text, &unit, -1) == -1);
    CHECK(fd2_field_item_list_draw(&vga, &fixture.assets, NULL, &fixture.text, &unit, -1) == -1);
    CHECK(fd2_field_item_list_draw(&vga, &fixture.assets, &fixture.font, NULL, &unit, -1) == -1);
    CHECK(fd2_field_item_list_draw(&vga, &fixture.assets, &fixture.font, &fixture.text, NULL, -1) == -1);
    CHECK(!memcmp(&vga, &before, sizeof(vga)));
    close_magic_fixture(&fixture);
    return 0;
}

static int test_bad_inventory_does_not_hide_magic_upper_page(void) {
    magic_fixture fixture;
    CHECK(open_magic_fixture(&fixture) == 0);
    fd2_field_unit unit;
    empty_inventory(&unit);
    item_slot(&unit, 0, 0x40, 0);
    item_slot(&unit, 7, 0, 206);
    magic_bit(&unit, 0);
    fd2_field_unit before_unit = unit;
    fd2_vga expected = {0};
    memset(expected.framebuffer, 0xa5, sizeof(expected.framebuffer));
    fd2_field_detail_draw(&expected, &fixture.assets, &fixture.font, &fixture.text, &unit, NULL);
    CHECK(fd2_field_magic_list_draw(&expected, &fixture.assets, &fixture.font, &fixture.text, &unit, -1) == 0);
    const uint8_t *original_text = fixture.text.data, *tokens;
    size_t count;
    CHECK(fd2_text_entry_get_fragment(&fixture.text, 181u + 206u, &tokens, &count) == 0 && count > 1);
    uint8_t *broken = malloc(fixture.text.size);
    CHECK(broken);
    memcpy(broken, original_text, fixture.text.size);
    size_t offset = (size_t)(tokens - original_text);
    broken[offset] = 1; broken[offset + 1] = 0xff;
    fd2_image icon = fixture.assets.detail_icons[8];
    for (int fault = 0; fault < 2; fault++) {
        if (fault == 0) fixture.text.data = broken;
        else fixture.assets.detail_icons[8].height = 7;
        CHECK(reject_item_list(&fixture, &unit, 7) == 0);
        fd2_vga actual = {0};
        memset(actual.framebuffer, 0xa5, sizeof(actual.framebuffer));
        /* 与真实MAGIC modal相同：detail供上页，再用合法法术底板覆盖库存。
         * 坏物品字模／无关item icon不成为该owner的上页依赖。 */
        fd2_field_detail_draw(&actual, &fixture.assets, &fixture.font, &fixture.text, &unit, NULL);
        CHECK(fd2_field_magic_list_draw(&actual, &fixture.assets, &fixture.font, &fixture.text, &unit, -1) == 0);
        CHECK(memcmp(&actual, &expected, sizeof(actual)) == 0);
        CHECK(memcmp(&unit, &before_unit, sizeof(unit)) == 0);
        fixture.text.data = original_text;
        fixture.assets.detail_icons[8] = icon;
    }
    free(broken);
    close_magic_fixture(&fixture);
    return 0;
}

static int detail_region_equal(const fd2_vga *a, const fd2_vga *b,
                               int x, int y, int width, int height) {
    for (int row = 0; row < height; row++)
        CHECK(!memcmp(a->framebuffer + (y + row) * VGA_STRIDE + x,
                      b->framebuffer + (y + row) * VGA_STRIDE + x, (size_t)width));
    return 0;
}

static void expected_detail_digits(fd2_vga *vga, const fd2_field_info_assets *assets,
                                  int x, int y, const char *text, size_t bank) {
    for (size_t i = 0; text[i]; i++)
        fd2_map_sprite_blit(vga, &assets->digits[bank][(size_t)(text[i] - '0')],
                            x + (int)i * 6, y, 0);
}

static int test_detail_signed_words_and_native_bars(void) {
    magic_fixture fixture;
    CHECK(open_magic_fixture(&fixture) == 0);
    const struct {
        uint16_t current, maximum;
        const char *digits, *max_digits;
        size_t bank;
        int length; /* 0空条、负值两cap、102全条；不是客户端unsigned近似。 */
    } cases[] = {
        {65516, 40, "000", "040", 1, -49},
        {65517, 40, "000", "040", 1, -46},
        {20, 40, "020", "040", 1, 51},
        {0, 40, "000", "040", 1, 0},
        {40, 40, "040", "040", 0, 102},
        {65535, 65535, "000", "000", 0, 102},
        {32768, 32768, "000", "000", 0, 102},
        {65535, 100, "000", "100", 1, 0},
        {1, 65535, "001", "000", 1, -100},
        {1, 0, "001", "000", 1, -1000}, /* max0不调用17d6f。 */
        {0, 0, "000", "000", 0, -1000}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fd2_field_unit unit;
        empty_inventory(&unit);
        unit.hp = unit.mp = cases[i].current;
        unit.hp_max = unit.mp_max = cases[i].maximum;
        fd2_field_unit before = unit;
        fd2_vga actual = {0}, expected = {0};
        memset(actual.framebuffer, 0xa5, sizeof(actual.framebuffer));
        memset(expected.framebuffer, 0xa5, sizeof(expected.framebuffer));
        fd2_field_detail_draw(&actual, &fixture.assets, &fixture.font, &fixture.text, &unit, NULL);
        fd2_map_sprite_blit(&expected, &fixture.assets.detail_top, 92, 7, -1);
        for (size_t bank = 0; bank < 2; bank++) {
            int y = bank ? 52 : 33;
            int length = cases[i].length;
            if (length == 0) {
                for (int column = 1; column <= 101; column++)
                    fd2_map_sprite_blit(&expected, &fixture.assets.bars[6], 198 + column, y, -1);
                fd2_map_sprite_blit(&expected, &fixture.assets.bars[7], 300, y, -1);
            } else if (length != -1000) {
                fd2_map_sprite_blit(&expected, &fixture.assets.bars[bank], 198, y, -1);
                for (int column = 1; column < length; column++)
                    fd2_map_sprite_blit(&expected, &fixture.assets.bars[2 + bank * 2], 198 + column, y, -1);
                fd2_map_sprite_blit(&expected, &fixture.assets.bars[3 + bank * 2],
                                    198 + (length < 0 ? 1 : length), y, -1);
            }
            CHECK(detail_region_equal(&actual, &expected, 198, y, 103, 5) == 0);
            expected_detail_digits(&expected, &fixture.assets, 267, bank ? 59 : 41, cases[i].digits, cases[i].bank);
            expected_detail_digits(&expected, &fixture.assets, 293, bank ? 59 : 41, cases[i].max_digits, 0);
            CHECK(detail_region_equal(&actual, &expected, 267, bank ? 59 : 41, 44, 8) == 0);
        }
        CHECK(!memcmp(&unit, &before, sizeof(unit)));
    }
    /* DX/四派生都是movsx WORD；LV/EX/MV仍是u8，255显示两位溢出93。 */
    fd2_field_unit unit;
    empty_inventory(&unit);
    unit.hp = unit.hp_max = unit.mp = unit.mp_max = 1;
    unit.accuracy = unit.attack = unit.evasion = unit.defense = 65535;
    ((uint8_t *)&unit)[0x3e] = ((uint8_t *)&unit)[0x3f] = 0xff;
    unit.level = unit.experience = unit.movement_points = 255;
    unit.attack_status = unit.defense_status = unit.hit_evasion_status = 1;
    fd2_field_unit before = unit;
    fd2_vga actual = {0}, expected = {0};
    fd2_field_detail_draw(&actual, &fixture.assets, &fixture.font, &fixture.text, &unit, NULL);
    fd2_map_sprite_blit(&expected, &fixture.assets.detail_top, 92, 7, -1);
    const int words[5][3] = {{117, 55, 1}, {117, 67, 2}, {157, 67, 2}, {117, 79, 2}, {157, 79, 2}};
    for (size_t i = 0; i < 5; i++) {
        expected_detail_digits(&expected, &fixture.assets, words[i][0], words[i][1], "000", (size_t)words[i][2]);
        CHECK(detail_region_equal(&actual, &expected, words[i][0], words[i][1], 18, 8) == 0);
    }
    const int bytes[3][2] = {{157, 33}, {157, 44}, {157, 55}};
    for (size_t i = 0; i < 3; i++) {
        fd2_map_sprite_blit(&expected, &fixture.assets.two_digit_overflow, bytes[i][0], bytes[i][1], 0);
        CHECK(detail_region_equal(&actual, &expected, bytes[i][0], bytes[i][1], 12, 8) == 0);
    }
    CHECK(!memcmp(&unit, &before, sizeof(unit)));
    close_magic_fixture(&fixture);
    return 0;
}

static uint64_t summary_hash(const fd2_vga *vga, int x) {
    uint64_t value = UINT64_C(0xcbf29ce484222325);
    for (int y = 9; y < 51; y++)
        for (int column = x; column < x + 149; column++) {
            value ^= vga->framebuffer[y * VGA_STRIDE + column];
            value *= UINT64_C(0x100000001b3);
        }
    return value;
}

static int test_unit_summary_native_panels(void) {
    magic_fixture fixture;
    CHECK(open_magic_fixture(&fixture) == 0);
    /* 四张已存WGC的完整149×42资源参考（6258px各bad0、delta<=3）；
     * native-summary-evidence.json SHA88a81c5a...759fc7c3。
     * 固定indexed参考来自同原资源，不把RGB容差或旧图当SDL实机。 */
    const struct { const char *record; int relative_x; uint64_t expected; } cases[] = {
        {"0b0c06000000020303004020408080ff80ff80ff80ff80ff80ff00000000000102030000000000004b0000ff200000ff22ff00000000001e000f000400010b003600360000000000320011006a000b00", 7, UINT64_C(0xea94a4c18f53acee)},
        {"090c00000000020000004000408400c080ff80ff80ff800080c90000000000010101000000000000c9c500c1c9c700c1ceff00c1cec1cb060004000400cc02002a002a000000000010000c0061000200", 6, UINT64_C(0xcc68854b399e7af7)},
        {"071103000000021e1e00404840b280ff80ff80ff80ff80f180ed00000000000619010000000000003ff53ff518f5841010fff0ee10ed8407000600045cf50200290032000000000016000e005c000200", 0, UINT64_C(0x99775bdbffa2730b)},
        {"070e00000000020000004000408400c080ff80ff80ff800080c90000000000010101000000000000c9c500c1c9c700c1ceff00c1cec1cb060004000400cc02002a002a000000000010000c0061000200", 12, UINT64_C(0xcc68854b399e7af7)}
    };
    fd2_field_info_assets assets_before = fixture.assets;
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fd2_field_unit unit;
        CHECK(strlen(cases[i].record) == sizeof(unit) * 2);
        for (size_t j = 0; j < sizeof(unit); j++) {
            const char *hex = "0123456789abcdef";
            const char *hi = strchr(hex, cases[i].record[j * 2]);
            const char *lo = strchr(hex, cases[i].record[j * 2 + 1]);
            CHECK(hi && lo);
            ((uint8_t *)&unit)[j] = (uint8_t)((hi - hex) * 16 + lo - hex);
        }
        fd2_field_unit before = unit;
        fd2_vga actual = {0};
        memset(actual.framebuffer, 0xa5, sizeof(actual.framebuffer));
        fd2_vga nonpixels = actual;
        CHECK(fd2_field_unit_summary_draw(&actual, &fixture.assets, &fixture.font,
                  &fixture.text, &unit, cases[i].relative_x) == 0);
        int x = cases[i].relative_x >= 7 ? 9 : 160;
        CHECK(summary_hash(&actual, x) == cases[i].expected);
        for (int y = 0; y < VGA_H; y++)
            for (int column = 0; column < VGA_W; column++)
                if (y < 9 || y >= 51 || column < x || column >= x + 149)
                    CHECK(actual.framebuffer[y * VGA_STRIDE + column] == 0xa5);
        memcpy(nonpixels.framebuffer, actual.framebuffer, sizeof(actual.framebuffer));
        CHECK(!memcmp(&actual, &nonpixels, sizeof(actual)));
        CHECK(!memcmp(&unit, &before, sizeof(unit)));
        CHECK(!memcmp(&fixture.assets, &assets_before, sizeof(assets_before)));
    }
    close_magic_fixture(&fixture);
    return 0;
}

static void expected_summary_bar(fd2_vga *vga, const fd2_field_info_assets *assets,
                                  size_t bank, int x, int y, int length) {
    if (length == -1000) return; /* max0：原调用不绘任何column。 */
    if (length == 0) {
        for (int i = 1; i <= 101; i++) expected_blit(vga->framebuffer, &assets->bars[6], x + i, y, -1);
        expected_blit(vga->framebuffer, &assets->bars[7], x + 102, y, -1);
    } else {
        expected_blit(vga->framebuffer, &assets->bars[bank], x, y, -1);
        for (int i = 1; i < length; i++) expected_blit(vga->framebuffer, &assets->bars[2 + 2 * bank], x + i, y, -1);
        expected_blit(vga->framebuffer, &assets->bars[3 + 2 * bank], x + (length > 1 ? length : 1), y, -1);
    }
}

static int test_unit_summary_signed_bars_and_numbers(void) {
    magic_fixture fixture;
    CHECK(open_magic_fixture(&fixture) == 0);
    const struct { uint16_t current, maximum; int length; size_t bank; const char *digits; } cases[] = {
        {0, 40, 0, 1, "000"}, {40, 40, 102, 0, "040"}, {41, 50, 83, 1, "041"},
        {65516, 40, -49, 1, "000"}, {65535, 65535, 102, 0, "000"},
        {32768, 32768, 102, 0, "000"}, {1, 65535, -100, 1, "001"},
        {1, 0, -1000, 1, "001"}, {0, 0, -1000, 0, "000"},
        {1000, 1000, 102, 0, NULL}, {1001, 2000, 51, 1, NULL}
    };
    const uint8_t *name;
    size_t names;
    CHECK(fd2_text_entry_get_fragment(&fixture.text, 1, &name, &names) == 0);
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fd2_field_unit unit = {0};
        unit.hp = unit.mp = cases[i].current;
        unit.hp_max = unit.mp_max = cases[i].maximum;
        unit.level = 255;
        /* 库存/状态不是本板资源依赖，不以详情或物品预检替代姓名预检。 */
        memset(unit.data_09_1e, 0xff, sizeof(unit.data_09_1e));
        fd2_field_unit before = unit;
        for (int side = 0; side < 2; side++) {
            int x = side ? 9 : 160;
            fd2_vga actual = {0}, expected = {0};
            memset(actual.framebuffer, 0xa5, sizeof(actual.framebuffer));
            memset(expected.framebuffer, 0xa5, sizeof(expected.framebuffer));
            CHECK(fd2_field_unit_summary_draw(&actual, &fixture.assets, &fixture.font,
                        &fixture.text, &unit, side ? 7 : 6) == 0);
            expected_blit(expected.framebuffer, &fixture.assets.unit_summary, x, 9, -1);
            expected_summary_bar(&expected, &fixture.assets, 0, x + 21, 31, cases[i].length);
            expected_summary_bar(&expected, &fixture.assets, 1, x + 21, 40, cases[i].length);
            expected_blit(expected.framebuffer, &fixture.assets.two_digit_overflow, x + 132, 13, 0);
            for (int row = 0; row < 2; row++) {
                int y = row ? 39 : 30;
                if (cases[i].digits) expected_detail_digits(&expected, &fixture.assets, x + 126, y, cases[i].digits, cases[i].bank);
                else expected_blit(expected.framebuffer, &fixture.assets.digit_overflow[cases[i].bank], x + 126, y, 0);
            }
            for (size_t j = 0; j < names; j++) {
                uint16_t token = fd2_text_token_at(name, j);
                if (token == 0xffff) break;
                fd2_font_draw_glyph(&expected, &fixture.font, token, x + 5 + (int)j * 16, 13, 205, 76, -1);
            }
            CHECK(!memcmp(actual.framebuffer, expected.framebuffer, sizeof(actual.framebuffer)));
            CHECK(!memcmp(&unit, &before, sizeof(unit)));
        }
    }
    close_magic_fixture(&fixture);
    return 0;
}

static int test_unit_summary_atomic_rejection(void) {
    magic_fixture fixture;
    CHECK(open_magic_fixture(&fixture) == 0);
    fd2_field_unit unit = {0}, before = unit;
    fd2_vga actual = {0};
    memset(&actual, 0xa5, sizeof(actual));
    fd2_vga unchanged = actual;
#define SUMMARY_REJECT(font, text, actor) do { \
    CHECK(fd2_field_unit_summary_draw(&actual, &fixture.assets, (font), (text), (actor), 7) == -1); \
    CHECK(!memcmp(&actual, &unchanged, sizeof(actual))); \
    CHECK(!memcmp(&unit, &before, sizeof(unit))); \
} while (0)
    fd2_image saved = fixture.assets.bars[7];
    fixture.assets.bars[7].pixels = NULL;
    SUMMARY_REJECT(&fixture.font, &fixture.text, &unit);
    fixture.assets.bars[7] = saved;
    saved = fixture.assets.digits[1][9];
    fixture.assets.digits[1][9].width = 7;
    SUMMARY_REJECT(&fixture.font, &fixture.text, &unit);
    fixture.assets.digits[1][9] = saved;
    saved = fixture.assets.unit_summary;
    fixture.assets.unit_summary.height = 43;
    SUMMARY_REJECT(&fixture.font, &fixture.text, &unit);
    fixture.assets.unit_summary = saved;
    fd2_font short_font = fixture.font;
    short_font.glyph_count = 37; /* 姓名36合法、末字37失败，不能先画前字。 */
    SUMMARY_REJECT(&short_font, &fixture.text, &unit);
    short_font = fixture.font; short_font.size = 0;
    SUMMARY_REJECT(&short_font, &fixture.text, &unit);
    uint8_t bad_tokens[18];
    memset(bad_tokens, 0, sizeof(bad_tokens));
    for (size_t i = 0; i < 8; i++) bad_tokens[2 * i] = 36;
    bad_tokens[16] = bad_tokens[17] = 0xff;
    uint16_t offsets[2] = {0, 0};
    fd2_text_entry bad_text = {bad_tokens, sizeof(bad_tokens), offsets, 2};
    SUMMARY_REJECT(&fixture.font, &bad_text, &unit); /* 名称不能越过LV。 */
    bad_tokens[2] = 0xfe; bad_tokens[3] = 0xff;
    SUMMARY_REJECT(&fixture.font, &bad_text, &unit); /* 未证控制符。 */
    SUMMARY_REJECT(NULL, &fixture.text, &unit);
    SUMMARY_REJECT(&fixture.font, NULL, &unit);
    SUMMARY_REJECT(&fixture.font, &fixture.text, NULL);
#undef SUMMARY_REJECT
    close_magic_fixture(&fixture);
    return 0;
}

int main(void) {
    CHECK(test_detail_transition() == 0);
    CHECK(test_item_table() == 0);
    CHECK(test_panel_layout() == 0);
    CHECK(test_overview_layout_and_counts() == 0);
    CHECK(test_overview_resource_numbers() == 0);
    CHECK(test_stage1_overview_conditions() == 0);
    CHECK(test_overview_rejects_before_drawing() == 0);
    CHECK(test_text_fragment_span_rejection() == 0);
    CHECK(test_magic_asset_owner() == 0);
    CHECK(test_magic_list_real_layout_and_selection() == 0);
    CHECK(test_magic_empty_name_and_opaque_cost() == 0);
    CHECK(test_magic_rejects_before_drawing() == 0);
    CHECK(test_equipment_attribute_coordinates() == 0);
    CHECK(test_item_list_layout_raw_selection_and_all_records() == 0);
    CHECK(test_item_raw_icons_copy_zero_and_fallback_position() == 0);
    CHECK(test_item_list_preflight_leaves_all_state_unchanged() == 0);
    CHECK(test_bad_inventory_does_not_hide_magic_upper_page() == 0);
    CHECK(test_detail_signed_words_and_native_bars() == 0);
    CHECK(test_unit_summary_native_panels() == 0);
    CHECK(test_unit_summary_signed_bars_and_numbers() == 0);
    CHECK(test_unit_summary_atomic_rejection() == 0);
    puts("field_info_test: ok");
    return 0;
}
