#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_visual.h"
#include "tile.h"

/* tile.c 的测试链接只需要清屏 primitive，不创建 SDL window。 */
void fd2_vga_clear(fd2_vga *vga, uint8_t color) {
    if (vga) memset(vga->framebuffer, color, sizeof(vga->framebuffer));
}

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return -1; \
    } \
} while (0)

static void wr_u16_le(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
}

static void wr_u32_le(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static int build_archive(fd2_archive *archive) {
    const size_t cursor_header = 6u + FD2_FIELD_CURSOR_FRAME_COUNT * 4u;
    const size_t cursor_frame_size = 1u + 2u + 23u * 2u;
    const size_t cursor_size = cursor_header +
        FD2_FIELD_CURSOR_FRAME_COUNT * cursor_frame_size;
    const size_t lut_header = 6u + FD2_FIELD_RANGE_LUT_COUNT * 4u;
    const size_t lut_size = lut_header + FD2_FIELD_RANGE_LUT_COUNT * 256u;
    const size_t offsets[] = {0, 1, 1 + cursor_size,
                              2 + cursor_size};
    const size_t total = 2 + cursor_size + lut_size;

    memset(archive, 0, sizeof(*archive));
    archive->data = calloc(total, 1);
    archive->offsets = calloc(4, sizeof(*archive->offsets));
    if (!archive->data || !archive->offsets) return -1;
    archive->size = total;
    archive->count = 4;
    for (size_t i = 0; i < 4; i++)
        archive->offsets[i] = (uint32_t)offsets[i];

    uint8_t *cursor = archive->data + offsets[1];
    wr_u16_le(cursor, 24);
    wr_u16_le(cursor + 2, 24);
    wr_u16_le(cursor + 4, FD2_FIELD_CURSOR_FRAME_COUNT);
    for (size_t frame = 0; frame < FD2_FIELD_CURSOR_FRAME_COUNT; frame++) {
        size_t start = cursor_header + frame * cursor_frame_size;
        wr_u32_le(cursor + 6 + frame * 4, (uint32_t)start);
        cursor[start] = 0xcbu; /* SKIP 12 */
        cursor[start + 1] = 11; /* RUN 12 */
        cursor[start + 2] = (uint8_t)(frame + 1);
        for (size_t row = 1; row < 24; row++) {
            size_t offset = start + 3u + (row - 1u) * 2u;
            cursor[offset] = 23; /* RUN 24 */
            cursor[offset + 1] = (uint8_t)(frame + 1);
        }
    }

    uint8_t *lmi = archive->data + offsets[3];
    memcpy(lmi, "LMI1", 4);
    wr_u16_le(lmi + 4, FD2_FIELD_RANGE_LUT_COUNT);
    for (size_t frame = 0; frame < FD2_FIELD_RANGE_LUT_COUNT; frame++) {
        size_t start = lut_header + frame * 256u;
        wr_u32_le(lmi + 6 + frame * 4, (uint32_t)start);
        memset(lmi + start, (int)frame, 256u);
    }
    return 0;
}

static int test_resources_and_sequence(void) {
    fd2_archive archive;
    CHECK(build_archive(&archive) == 0);
    fd2_field_visuals visuals;
    CHECK(fd2_field_visuals_open(&visuals, &archive) == 0);
    CHECK(visuals.cursor_frame_count == FD2_FIELD_CURSOR_FRAME_COUNT);
    CHECK(visuals.range_lut_count == FD2_FIELD_RANGE_LUT_COUNT);
    for (size_t frame = 0; frame < FD2_FIELD_CURSOR_FRAME_COUNT; frame++) {
        const fd2_image *image = fd2_field_cursor_frame(&visuals, frame);
        CHECK(image && image->width == 24 && image->height == 24);
        CHECK(image->pixels[0] == 0 && image->pixels[11] == 0 &&
              image->pixels[12] == frame + 1 &&
              image->pixels[24 * 24 - 1] == frame + 1);
    }
    static const uint8_t expected[FD2_FIELD_RANGE_PHASE_COUNT] = {
        8, 1, 0, 0, 0, 4, 2, 5, 1, 0,
        0, 0, 4, 2, 5, 1, 0, 0, 0, 4,
    };
    for (size_t phase = 0; phase < FD2_FIELD_RANGE_PHASE_COUNT; phase++)
        CHECK(fd2_field_range_lut(&visuals, phase)[0] == expected[phase]);
    for (size_t frame = 0; frame < FD2_FIELD_RANGE_LUT_COUNT; frame++)
        CHECK(fd2_field_lut_frame(&visuals, frame)[0] == frame);

    fd2_field_visuals_close(&visuals);
    free(archive.offsets);
    free(archive.data);
    return 0;
}

static int test_terrain_animation_and_transparent_lut(void) {
    fd2_terrain_attr attr = {0};
    fd2_terrain_tileset terrain = {0};
    terrain.attrs = &attr;
    terrain.attr_count = 1;
    terrain.sheet.frame_count = 8;
    size_t frame = 99;

    attr.flags = 0x04;
    CHECK(fd2_terrain_base_frame_from_cell_animated(
              &terrain, 0, 1, 0, &frame) == 0 && frame == 1);
    attr.flags = 0x08;
    CHECK(fd2_terrain_base_frame_from_cell_animated(
              &terrain, 0, 1, 0, &frame) == 0 && frame == 2);
    attr.flags = 0x10;
    CHECK(fd2_terrain_base_frame_from_cell_animated(
              &terrain, 0, 0, 3, &frame) == 0 && frame == 1);
    attr.flags = 0x88;
    CHECK(fd2_terrain_overlay_frame_from_cell(
              &terrain, 0, 1, &frame) == 1 && frame == 3);

    fd2_vga vga;
    memset(&vga, 0, sizeof(vga));
    vga.framebuffer[0] = 5;
    vga.framebuffer[1] = 5;
    uint8_t pixels[] = {0, 3};
    fd2_image image = {.width = 2, .height = 1, .pixels = pixels};
    uint8_t lut[256];
    for (size_t i = 0; i < 256; i++) lut[i] = (uint8_t)i;
    lut[0] = 9;
    lut[3] = 7;
    fd2_tileset_blit_lut(&vga, &image, 0, 0, 0, lut);
    CHECK(vga.framebuffer[0] == 5 && vga.framebuffer[1] == 7);
    return 0;
}

static int test_scaled_terrain_sampling(void) {
    uint32_t cells[2] = {0, 1};
    fd2_field_map map = {.width = 2, .height = 1, .cells = cells};
    fd2_terrain_attr attrs[2] = {{0}, {0}};
    uint8_t pixels[2][24 * 24];
    fd2_image images[2];
    for (size_t frame = 0; frame < 2; frame++) {
        for (int y = 0; y < 24; y++)
            for (int x = 0; x < 24; x++)
                pixels[frame][y * 24 + x] =
                    (uint8_t)(frame * 100 + x + 1);
        images[frame] = (fd2_image){
            .width = 24, .height = 24, .pixels = pixels[frame],
        };
    }
    fd2_terrain_tileset terrain = {
        .frames = images, .attrs = attrs, .attr_count = 2,
    };
    terrain.sheet.frame_count = 2;
    fd2_vga vga = {0};
    fd2_terrain_render_field_scaled_animated(
        &vga, &terrain, &map, 13 * 0x600, 8 * 0x600, 128, 0, 0);
    CHECK(vga.framebuffer[4 * VGA_STRIDE + 4] == 1);
    CHECK(vga.framebuffer[4 * VGA_STRIDE + 27] == 24);
    CHECK(vga.framebuffer[4 * VGA_STRIDE + 28] == 101);
    CHECK(vga.framebuffer[4 * VGA_STRIDE + 51] == 124);
    CHECK(vga.framebuffer[4 * VGA_STRIDE + 52] == 0);

    memset(vga.framebuffer, 0xaa, sizeof(vga.framebuffer));
    fd2_terrain_render_field_scaled_animated(
        &vga, &terrain, &map, 13 * 0x600, 8 * 0x600 - 128,
        128, 0, 0);
    CHECK(vga.framebuffer[4 * VGA_STRIDE + 4] == 0);
    CHECK(vga.framebuffer[5 * VGA_STRIDE + 4] == 1);

    fd2_terrain_render_field_scaled_animated(
        &vga, &terrain, &map, INT_MIN, INT_MAX, INT_MAX, 0, 0);
    CHECK(vga.framebuffer[4 * VGA_STRIDE + 4] == 0);
    return 0;
}

static int test_palette_apply_clipping(void) {
    fd2_vga vga;
    memset(&vga, 0, sizeof(vga));
    memset(vga.framebuffer, 3, sizeof(vga.framebuffer));
    uint8_t lut[256];
    for (size_t i = 0; i < 256; i++) lut[i] = (uint8_t)i;
    lut[3] = 9;
    fd2_field_apply_palette_lut(&vga, -1, -1, 3, 3, lut);
    CHECK(vga.framebuffer[0] == 9);
    CHECK(vga.framebuffer[1] == 9);
    CHECK(vga.framebuffer[VGA_STRIDE] == 9);
    CHECK(vga.framebuffer[VGA_STRIDE + 1] == 9);
    CHECK(vga.framebuffer[2] == 3);
    fd2_field_apply_palette_lut(&vga, INT_MAX, INT_MAX,
                                INT_MAX, INT_MAX, lut);
    fd2_field_apply_palette_lut(&vga, INT_MIN, INT_MIN,
                                INT_MAX, INT_MAX, lut);
    CHECK(vga.framebuffer[2] == 3);
    return 0;
}

/* 219ad/22046原kernel+真实sqrt/__CHP，CW127f，无hook：
 * vectors SHA dc13930868b3a6ea792256e2da38e58f7118260eba42ce54846d0878aae3c0c5。
 * 以下固定点/范围哈希来自原指令输出，不以被测整数公式重算预期。 */
static uint64_t mask_hash_u32(uint64_t hash, uint32_t value) {
    for (unsigned i = 0; i < 4; i++) {
        hash ^= (value >> (8 * i)) & 255u;
        hash *= UINT64_C(0x100000001b3);
    }
    return hash;
}

static int mask_border_unchanged(const fd2_vga *vga) {
    for (int y = 0; y < VGA_H; y++) for (int x = 0; x < VGA_W; x++)
        if (x < 4 || x >= 316 || y < 4 || y >= 196)
            CHECK(vga->framebuffer[y * VGA_STRIDE + x] == 3);
    return 0;
}

static int test_transition_native_rounding_and_vectors(void) {
    fd2_vga vga = {0};
    uint8_t lut[256];
    for (unsigned i = 0; i < 256; i++) lut[i] = (uint8_t)(i + 1u);
    static const struct { int radius, dy, half; } circle[] = {
        {1,0,1}, {2,1,2}, {3,1,4}, {3,0,4}, {5,1,7},
        {5,0,8}, {10,1,15}, {10,0,16}, {256,1,409},
    };
    for (size_t i = 0; i < sizeof(circle) / sizeof(circle[0]); i++) {
        int cx = circle[i].half > 156 ? -100 : 156;
        memset(vga.framebuffer, 3, sizeof(vga.framebuffer));
        CHECK(fd2_field_transition_lut_before_actors(&vga, lut,
                  cx, 96, circle[i].radius) == 0);
        for (int x = 0; x < 312; x++)
            CHECK(vga.framebuffer[(100-circle[i].dy)*VGA_STRIDE+x+4] ==
                  (x >= cx-circle[i].half && x < cx+circle[i].half ? 4 : 3));
        CHECK(mask_border_unchanged(&vga) == 0);
    }
    static const struct { int radius, half; } rectangle[] = {
        {1,1}, {2,3}, {3,4}, {5,8}, {10,16}, {16,25},
        {24,38}, {40,64}, {160,256}, {192,307}, {4096,6553},
    };
    for (size_t i = 0; i < sizeof(rectangle)/sizeof(rectangle[0]); i++) {
        int cx = rectangle[i].radius == 4096 ? -6500 : 0;
        memset(vga.framebuffer, 3, sizeof(vga.framebuffer));
        CHECK(fd2_field_transition_lut_after_actors(&vga, lut,
                  cx, 1, rectangle[i].radius) == 0);
        for (int x = 0; x < 312; x++)
            CHECK(vga.framebuffer[4*VGA_STRIDE+x+4] ==
                  (x >= cx-rectangle[i].half && x < cx+rectangle[i].half ? 4 : 3));
        CHECK(mask_border_unchanged(&vga) == 0);
    }
    /* r=2的上下端行严格排除，右端x为exclusive。 */
    memset(vga.framebuffer, 3, sizeof(vga.framebuffer));
    CHECK(fd2_field_transition_lut_before_actors(&vga,lut,10,10,2) == 0);
    CHECK(vga.framebuffer[12*VGA_STRIDE+14] == 3);
    CHECK(vga.framebuffer[16*VGA_STRIDE+14] == 3);
    CHECK(vga.framebuffer[14*VGA_STRIDE+16] == 4);
    CHECK(vga.framebuffer[14*VGA_STRIDE+17] == 3);

    uint64_t circle_hash = UINT64_C(0xcbf29ce484222325);
    for (unsigned radius = 1; radius <= 128; radius++) {
        memset(vga.framebuffer, 3, sizeof(vga.framebuffer));
        CHECK(fd2_field_transition_lut_before_actors(&vga,lut,0,0,(int)radius) == 0);
        for (unsigned dy = 0; dy < radius; dy++) {
            unsigned width = 0;
            for (int x = 0; x < 312; x++)
                width += vga.framebuffer[(dy+4)*VGA_STRIDE+x+4] == 4;
            for (unsigned x = 0; x < 312; x++)
                CHECK(vga.framebuffer[(dy+4)*VGA_STRIDE+x+4] == (x < width ? 4 : 3));
            circle_hash = mask_hash_u32(circle_hash, radius);
            circle_hash = mask_hash_u32(circle_hash, dy);
            circle_hash = mask_hash_u32(circle_hash, width);
        }
    }
    CHECK(circle_hash == UINT64_C(0xe1127548a75397c2)); /* 8256原指令三元组。 */
    uint64_t rect_hash = UINT64_C(0xcbf29ce484222325);
    for (unsigned radius = 1; radius <= 192; radius++) {
        memset(vga.framebuffer, 3, sizeof(vga.framebuffer));
        CHECK(fd2_field_transition_lut_after_actors(&vga,lut,0,1,(int)radius) == 0);
        unsigned width = 0;
        for (int x = 0; x < 312; x++) width += vga.framebuffer[4*VGA_STRIDE+x+4] == 4;
        rect_hash = mask_hash_u32(rect_hash, radius);
        rect_hash = mask_hash_u32(rect_hash, width);
    }
    CHECK(rect_hash == UINT64_C(0x56d3a7241525bf10));
    return 0;
}

static int test_transition_invalid_and_extreme_clipping(void) {
    fd2_vga vga, before;
    memset(&vga, 3, sizeof(vga)); before = vga;
    uint8_t lut[256], old_lut[256];
    for (unsigned i = 0; i < 256; i++) lut[i] = (uint8_t)(i+1u);
    memcpy(old_lut, lut, sizeof(lut));
    static const int invalid[] = {INT_MIN, -1, 0, 4097, INT_MAX};
    for (size_t i = 0; i < sizeof(invalid)/sizeof(invalid[0]); i++) {
        CHECK(fd2_field_transition_lut_before_actors(&vga,lut,0,0,invalid[i]) == -1);
        CHECK(fd2_field_transition_lut_after_actors(&vga,lut,0,0,invalid[i]) == -1);
    }
    CHECK(fd2_field_transition_lut_before_actors(NULL,lut,0,0,1) == -1);
    CHECK(fd2_field_transition_lut_after_actors(NULL,lut,0,0,1) == -1);
    CHECK(fd2_field_transition_lut_before_actors(&vga,NULL,0,0,1) == -1);
    CHECK(fd2_field_transition_lut_after_actors(&vga,NULL,0,0,1) == -1);
    CHECK(!memcmp(&vga,&before,sizeof(vga)));
    CHECK(fd2_field_transition_lut_before_actors(&vga,lut,INT_MIN,INT_MAX,4096) == 0);
    CHECK(fd2_field_transition_lut_after_actors(&vga,lut,INT_MAX,INT_MIN,4096) == 0);
    CHECK(!memcmp(&vga,&before,sizeof(vga)) && !memcmp(lut,old_lut,sizeof(lut)));
    CHECK(fd2_field_transition_lut_before_actors(&vga,lut,311,191,1) == 0);
    CHECK(vga.framebuffer[195*VGA_STRIDE+314] == 4 &&
          vga.framebuffer[195*VGA_STRIDE+315] == 4);
    CHECK(mask_border_unchanged(&vga) == 0);
    CHECK(!memcmp((uint8_t*)&vga+sizeof(vga.framebuffer),
                  (uint8_t*)&before+sizeof(before.framebuffer),
                  sizeof(vga)-sizeof(vga.framebuffer)));
    return 0;
}

int main(void) {
    CHECK(test_resources_and_sequence() == 0);
    CHECK(test_terrain_animation_and_transparent_lut() == 0);
    CHECK(test_scaled_terrain_sampling() == 0);
    CHECK(test_palette_apply_clipping() == 0);
    CHECK(test_transition_native_rounding_and_vectors() == 0);
    CHECK(test_transition_invalid_and_extreme_clipping() == 0);
    puts("field visual tests: ok");
    return 0;
}
