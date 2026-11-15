#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_visual.h"
#include "bios_clock.h"
#include "tile.h"

/* 独立有理数oracle：测试最大tick65536，乘积在uint64域；不调生产转换。 */
static uint64_t tick_ns(uint64_t tick) {
    return (tick*65536u*UINT64_C(1000000000)+1193181u)/1193182u;
}

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
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
        10, 9, 8, 7, 6, 5, 4, 3, 2, 1,
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

static int test_transition_circle12(void) {
    fd2_vga vga, before;
    memset(&vga, 3, sizeof(vga)); before = vga;
    uint8_t lut[256];
    for (unsigned i=0;i<256;i++) lut[i]=(uint8_t)(i+1u);
    CHECK(fd2_field_transition_lut_circle12(&vga,lut,100,100,5)==0);
    /* 已确认原sqrt/scale12/CHP：r5，dy0..4半宽6/5/5/4/3；端点不写。 */
    static const int half[]={6,5,5,4,3};
    for (int dy=-5;dy<=5;dy++) for (int x=90;x<110;x++) {
        int distance=dy<0 ? -dy:dy;
        int painted=distance<5 && x>=100-half[distance] && x<100+half[distance];
        CHECK(vga.framebuffer[(100+dy+4)*VGA_STRIDE+x+4]==(painted ? 4:3));
    }
    CHECK(mask_border_unchanged(&vga)==0);
    vga=before;
    CHECK(fd2_field_transition_lut_circle12(&vga,lut,0,0,0)==-1);
    CHECK(fd2_field_transition_lut_circle12(&vga,lut,0,0,4097)==-1);
    CHECK(fd2_field_transition_lut_circle12(&vga,NULL,0,0,5)==-1);
    CHECK(!memcmp(&vga,&before,sizeof(vga)));
    CHECK(fd2_field_transition_lut_before_actors_from(&vga,lut,100,100,5,99)==0);
    CHECK(fd2_field_transition_lut_after_actors_from(&vga,lut,100,100,5,99)==0);
    CHECK(vga.framebuffer[(98+4)*VGA_STRIDE+104]==3);
    CHECK(vga.framebuffer[(99+4)*VGA_STRIDE+104]==5);
    CHECK(vga.framebuffer[(100+4)*VGA_STRIDE+104]==5);
    vga=before;
    CHECK(fd2_field_transition_lut_before_actors_from(&vga,lut,100,100,5,-1)==-1);
    CHECK(fd2_field_transition_lut_after_actors_from(&vga,lut,100,100,5,193)==-1);
    CHECK(!memcmp(&vga,&before,sizeof(vga)));
    return 0;
}

static int test_actor_visual_shared_tick(void) {
    fd2_vga vga;memset(&vga,0,sizeof(vga));
    CHECK(fd2_field_actor_visual_tick(&vga,0)==0 && vga.actor_jitter_phase==0);
    CHECK(fd2_field_actor_visual_tick(&vga,tick_ns(1))==1 && vga.actor_jitter_phase==1);
    CHECK(fd2_field_actor_visual_tick(&vga,tick_ns(2)-1u)==0 && vga.actor_jitter_phase==1);
    CHECK(fd2_field_actor_visual_tick(&vga,tick_ns(100))==1 && vga.actor_jitter_phase==0);
    vga.actor_jitter_tick=65535;
    CHECK(fd2_field_actor_visual_tick(&vga,tick_ns(65536))==1);
    CHECK(vga.actor_jitter_tick==0 && vga.actor_jitter_phase==1);
    CHECK(fd2_field_actor_visual_tick(NULL,0)==-1);
    return 0;
}

static int test_shared_render_clocks(void) {
    fd2_field_visual_state s={0},before;
    CHECK(fd2_field_visual_actor_step(&s,tick_ns(4))==1);
    CHECK(s.actor_idle_phase==0 && s.cursor_phase==1 && !s.actor_idle_tick);
    CHECK(fd2_field_visual_actor_step(&s,tick_ns(5))==1);
    CHECK(s.actor_idle_phase==1 && s.cursor_phase==2 && s.actor_idle_tick==5);
    CHECK(fd2_field_visual_actor_step(&s,tick_ns(100))==1);
    CHECK(s.actor_idle_phase==2 && s.actor_idle_tick==100); /* 单步，不追赶。 */
    CHECK(fd2_field_visual_actor_step(&s,tick_ns(100))==1 && s.cursor_phase==0);
    CHECK(fd2_field_visual_tiles_step(&s,tick_ns(100),-1)==1);
    CHECK(s.terrain_phase==1 && s.range_phase==1 && s.range_tick==100);
    CHECK(fd2_field_visual_tiles_step(&s,tick_ns(102),-1)==1);
    CHECK(s.terrain_phase==0 && s.range_phase==1 && s.range_tick==100);
    CHECK(fd2_field_visual_tiles_step(&s,tick_ns(103),-1)==1);
    CHECK(s.terrain_phase==1 && s.range_phase==2 && s.range_tick==103);
    CHECK(fd2_field_visual_tiles_step(&s,tick_ns(103),19)==1);
    CHECK(s.range_phase==19 && s.range_tick==103);
    CHECK(fd2_field_visual_tiles_step(&s,tick_ns(106),-1)==1 && s.range_phase==0);
    s.actor_idle_tick=s.range_tick=s.terrain_tick=32767;
    CHECK(fd2_field_visual_actor_step(&s,tick_ns(32768))==1);
    CHECK(s.actor_idle_tick==0xffff8000u && s.actor_idle_phase==3);
    CHECK(fd2_field_visual_tiles_step(&s,tick_ns(32768),-1)==1);
    CHECK(s.terrain_tick==0xffff8000u && s.range_tick==0xffff8000u);
    CHECK(fd2_field_visual_actor_step(&s,tick_ns(65536))==1);
    CHECK(s.actor_idle_tick==0 && s.actor_idle_phase==0);
    before=s;
    CHECK(fd2_field_visual_tiles_step(&s,0,20)==-1);
    CHECK(!memcmp(&before,&s,sizeof(s)));
    s.actor_idle_phase=4;before=s;
    CHECK(fd2_field_visual_actor_step(&s,1)==-1);
    CHECK(!memcmp(&before,&s,sizeof(s)));
    CHECK(fd2_field_visual_tiles_step(NULL,0,-1)==-1);
    return 0;
}

static int test_transition_page_bridge(void) {
    static uint8_t original[FD2_FIELD_PAGE_SIZE],actual[FD2_FIELD_PAGE_SIZE];
    for(size_t i=0;i<sizeof(original);i++)original[i]=(uint8_t)(i*13u+i/456u);
    uint8_t lut[256];for(size_t i=0;i<256;i++)lut[i]=(uint8_t)(i+37u);
    fd2_field_page *page=fd2_field_page_create();CHECK(page);
    for(unsigned mode=0;mode<2;mode++) {
        CHECK(fd2_field_page_write(page,0,0,456,336,original,sizeof(original),456)==0);
        fd2_vga direct={0},projected={0};
        memset(direct.framebuffer,117,64000);memset(projected.framebuffer,117,64000);
        CHECK(fd2_field_page_project(page,72,72,direct.framebuffer,64000)==0);
        if(!mode) {
            CHECK(fd2_field_transition_lut_before_actors_from(&direct,lut,50,90,34,17)==0);
            CHECK(fd2_field_transition_page_before_actors_from(page,lut,50,90,34,17)==0);
            /* 模拟夹在22046两pass间的独立actor写点。 */
            uint8_t actor[4]={9,10,11,12};
            direct.framebuffer[104*320+54]=9;direct.framebuffer[104*320+55]=10;
            direct.framebuffer[105*320+54]=11;direct.framebuffer[105*320+55]=12;
            CHECK(fd2_field_page_write(page,122,172,2,2,actor,4,2)==0);
            CHECK(fd2_field_transition_lut_after_actors_from(&direct,lut,50,90,34,17)==0);
            CHECK(fd2_field_transition_page_after_actors_from(page,lut,50,90,34,17)==0);
        } else {
            CHECK(fd2_field_transition_lut_circle12(&direct,lut,50,90,34)==0);
            CHECK(fd2_field_transition_page_circle12(page,lut,50,90,34)==0);
        }
        CHECK(fd2_field_page_project(page,72,72,projected.framebuffer,64000)==0);
        CHECK(!memcmp(direct.framebuffer,projected.framebuffer,64000));
        CHECK(fd2_field_page_read(page,0,0,456,336,actual,sizeof(actual),456)==0);
        for(int y=0;y<336;y++)for(int x=0;x<456;x++)
            if(x<72 || x>=384 || y<72 || y>=264)
                CHECK(actual[y*456+x]==original[y*456+x]);
        CHECK(fd2_field_transition_page_before_actors_from(page,lut,0,0,0,0)==-1);
        static uint8_t unchanged[FD2_FIELD_PAGE_SIZE];
        CHECK(fd2_field_page_read(page,0,0,456,336,unchanged,sizeof(unchanged),456)==0);
        CHECK(!memcmp(actual,unchanged,sizeof(actual)));
    }
    fd2_field_page_destroy(page);return 0;
}

static int emit_clock_vectors(void) {
    /* 输入只有原DWORD及BIOS WORD，预期由独立Unicorn原指令读取产生。 */
    static const struct {unsigned kind,tick;int override;uint32_t s[7];} cases[]={
        {0,0,-1,{0,0,0,0,0,0,0}},
        {0,4,-1,{0,0,3,0,0,0,0}},
        {0,5,-1,{0,0,1,0,0,0,0}},
        {0,100,-1,{3,5,2,1,3,19,2}},
        {0,32768,-1,{1,32767,0,0,0,0,0}},
        {0,65535,-1,{2,0xffff8000u,0,0,0,0,0}},
        {0,0,-1,{3,0xffffffffu,0,0,0,0,0}},
        {0,0,-1,{3,32767,3,0,0,0,0}},
        {1,0,-1,{0,0,0,0,0,19,0}},
        {1,2,-1,{0,0,0,1,1,19,0}},
        {1,3,-1,{0,0,0,1,1,19,0}},
        {1,100,-1,{3,9,3,0,0,5,5}},
        {1,32768,-1,{0,0,0,1,32767,19,32767}},
        {1,65535,-1,{0,0,0,1,0xffff8000u,19,0xffff8000u}},
        {1,0,-1,{0,0,0,1,0xffff8000u,19,0xffff8000u}},
        {1,0,-1,{0,0,0,1,0xffffffffu,19,0xffffffffu}},
        {1,0,-1,{0,0,0,1,32767,19,32767}},
        {1,100,19,{0,0,0,0,3,0,99}},
        {1,100,0,{0,0,0,1,100,19,99}},
        {1,65535,7,{0,0,0,1,0xffffffffu,7,50}}
    };
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        const uint32_t *in=cases[i].s;
        fd2_field_visual_state s={in[0],in[1],in[2],in[3],in[4],in[5],in[6]};
        int result=cases[i].kind?fd2_field_visual_tiles_step(&s,tick_ns(cases[i].tick),cases[i].override):
            fd2_field_visual_actor_step(&s,tick_ns(cases[i].tick));
        if(result<0)return 1;
        printf("{\"kind\":%u,\"tick\":%u,\"override\":%d,\"initial\":[%u,%u,%u,%u,%u,%u,%u],"
            "\"final\":[%u,%u,%u,%u,%u,%u,%u]}\n",cases[i].kind,cases[i].tick,cases[i].override,
            in[0],in[1],in[2],in[3],in[4],in[5],in[6],s.actor_idle_phase,s.actor_idle_tick,
            s.cursor_phase,s.terrain_phase,s.terrain_tick,s.range_phase,s.range_tick);
    }
    return 0;
}

static int test_auxiliary_color_source_domain(void) {
    static const unsigned original_bases[3]={32,80,72};
    for(unsigned side=0;side<3;side++)for(unsigned phase=0;phase<8;phase++) {
        uint8_t color=255;
        CHECK(fd2_field_auxiliary_marker_color(side,phase,&color)==0);
        CHECK(color==original_bases[side]+phase);
    }
    uint8_t untouched=173;
    CHECK(fd2_field_auxiliary_marker_color(3,0,&untouched)==-1 && untouched==173);
    CHECK(fd2_field_auxiliary_marker_color(0,8,&untouched)==-1 && untouched==173);
    CHECK(fd2_field_auxiliary_marker_color(0,0,NULL)==-1);
    return 0;
}

int main(int argc,char **argv) {
    fd2_bios_clock_reset(0,0);
    if(argc==2 && !strcmp(argv[1],"--clock-jsonl"))return emit_clock_vectors();
    CHECK(test_resources_and_sequence() == 0);
    CHECK(test_terrain_animation_and_transparent_lut() == 0);
    CHECK(test_scaled_terrain_sampling() == 0);
    CHECK(test_palette_apply_clipping() == 0);
    CHECK(test_transition_native_rounding_and_vectors() == 0);
    CHECK(test_transition_invalid_and_extreme_clipping() == 0);
    CHECK(test_transition_circle12() == 0);
    CHECK(test_actor_visual_shared_tick() == 0);
    CHECK(test_shared_render_clocks() == 0);
    CHECK(test_transition_page_bridge()==0);
    CHECK(test_auxiliary_color_source_domain()==0);
    puts("field visual tests: ok");
    return 0;
}
