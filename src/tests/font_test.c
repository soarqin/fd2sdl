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

/* 原4ed7a独立点列：合成glyph0仅(0,0)/(15,0)/(0,1)/(15,15)置位。
 * expected不调用任何被测绘制器／bit循环，按原fg->左下->下的store序列。 */
static void linear_source_points(int64_t offset,unsigned stride,uint8_t fg,uint8_t shadow,unsigned count) {
    static const int dots[4][2]={{0,0},{15,0},{0,1},{15,15}};
    for(unsigned i=0;i<count;i++) {
        size_t at=(size_t)(offset+(int64_t)dots[i][1]*stride+dots[i][0]);
        expected.vga.framebuffer[at]=fg;
        expected.vga.framebuffer[at+stride-1u]=shadow;
        expected.vga.framebuffer[at+stride]=shadow;
    }
}
static void linear_source_fixture(void) {
    glyphs[0]=0x80;glyphs[1]=1;glyphs[2]=0x80;glyphs[31]=1;
}
static void linear_source_clear(size_t offset,unsigned stride,uint8_t color) {
    for(size_t row=0;row<16u;row++)memset(expected.vga.framebuffer+offset+row*stride,color,16);
}
static int test_linear_row_crossing_and_styles(void) {
    static const struct {size_t offset;uint32_t stride;uint8_t bg;} cases[]={
        {319,320,0},{319,320,74},{1367,456,0},{1367,456,74},{319,0x10140u,74}};
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        reset();linear_source_fixture();uint8_t saved[sizeof(glyphs)];memcpy(saved,glyphs,sizeof(saved));
        unsigned stride=i==4u ? 320u:(unsigned)cases[i].stride;
        if(cases[i].bg)linear_source_clear(cases[i].offset,stride,cases[i].bg);
        linear_source_points((int64_t)cases[i].offset,stride,205,76,4);
        CHECK(fd2_font_draw_glyph_linear(actual.vga.framebuffer,sizeof(actual.vga.framebuffer),&font,
            0,(int64_t)cases[i].offset,cases[i].stride,205,76,cases[i].bg)==0);
        CHECK(!memcmp(&actual,&expected,sizeof(actual)) && !memcmp(glyphs,saved,sizeof(saved)));
    }
    reset();linear_source_fixture();linear_source_points(319,320,0,0,4);
    CHECK(fd2_font_draw_glyph_linear(actual.vga.framebuffer,sizeof(actual.vga.framebuffer),&font,
        0,319,320,0,0,0)==0 && !memcmp(&actual,&expected,sizeof(actual)));
    /* raw10只清底、不读font；不改变旧XY的bg=0黑底合同。 */
    reset();linear_source_clear(319,320,74);
    CHECK(fd2_font_draw_glyph_linear(actual.vga.framebuffer,sizeof(actual.vga.framebuffer),NULL,
        10,319,320,205,76,74)==0 && !memcmp(&actual,&expected,sizeof(actual)));
    reset();CHECK(fd2_font_draw_glyph_linear(actual.vga.framebuffer,sizeof(actual.vga.framebuffer),NULL,
        10,INT64_MAX,320,205,76,0)==0 && !memcmp(&actual,&expected,sizeof(actual)));
    return 0;
}
static int test_linear_owner_error_prefix(void) {
    reset();linear_source_fixture();expected.vga.framebuffer[0]=205;
    /* 首fg合法，随后左下shadow=-1才越owner；不预先拒绝整个glyph。 */
    CHECK(fd2_font_draw_glyph_linear(actual.vga.framebuffer,sizeof(actual.vga.framebuffer),&font,
        0,0,0,205,76,0)==-1 && !memcmp(&actual,&expected,sizeof(actual)));
    reset();linear_source_fixture();memset(expected.vga.framebuffer,74,17);
    /* bg预清第一行16B＋第二行首B已写，下一B越17B声明owner。 */
    CHECK(fd2_font_draw_glyph_linear(actual.vga.framebuffer,17,&font,
        0,0,16,205,76,74)==-1 && !memcmp(&actual,&expected,sizeof(actual)));
    reset();linear_source_fixture();expected.vga.framebuffer[63999]=205;
    CHECK(fd2_font_draw_glyph_linear(actual.vga.framebuffer,sizeof(actual.vga.framebuffer),&font,
        0,63999,320,205,76,0)==-1 && !memcmp(&actual,&expected,sizeof(actual)));
    reset();linear_source_fixture();linear_source_clear(319,320,74);
    /* 15f84负数首'-'给DWORD fffffffd，SHL后font-96；只测C安全错误和定义前缀，
     * 不执行原读，不给越owner字体生成expected。 */
    CHECK(fd2_font_draw_glyph_linear(actual.vga.framebuffer,sizeof(actual.vga.framebuffer),&font,
        UINT32_MAX-2u,319,320,205,76,74)==-1 && !memcmp(&actual,&expected,sizeof(actual)));
    reset();linear_source_fixture();linear_source_clear(319,320,74);
    CHECK(fd2_font_draw_glyph_linear(actual.vga.framebuffer,sizeof(actual.vga.framebuffer),NULL,
        0,319,320,205,76,74)==-1 && !memcmp(&actual,&expected,sizeof(actual)));
    reset();uint8_t two_bytes[2]={0x80,1};fd2_font short_font={two_bytes,2,1};
    linear_source_points(319,320,205,76,2);
    CHECK(fd2_font_draw_glyph_linear(actual.vga.framebuffer,sizeof(actual.vga.framebuffer),&short_font,
        0,319,320,205,76,0)==-1 && !memcmp(&actual,&expected,sizeof(actual)));
    reset();glyphs[1]=1; /* 第一个真写的delta=15，必须检查int64加法溢出。 */
    CHECK(fd2_font_draw_glyph_linear(actual.vga.framebuffer,sizeof(actual.vga.framebuffer),&font,
        0,INT64_MAX,320,205,76,0)==-1 && !memcmp(&actual,&expected,sizeof(actual)));
    CHECK(fd2_font_draw_glyph_linear(NULL,0,&font,0,0,320,205,76,0)==-1);
    return 0;
}

int main(void) {
    if (test_shadow_direction_and_transparency() ||
        test_preclear_and_zero_colors() ||
        test_glyph10_and_invalid_inputs() ||
        test_each_destination_is_clipped() ||
        test_linear_row_crossing_and_styles() ||
        test_linear_owner_error_prefix())
        return 1;
    puts("font_test: ok");
    return 0;
}
