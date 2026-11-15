/* sub_127E0 @0x127e0 / sub_4E1A6 @0x4e1a6：合成FDICON验证frame、
 * 坐标、已行动映色与透明；129ec/12ac6验证actor-local前景恢复，
 * 正式Game绘序覆盖空屋顶cursor回归。不启动窗口、不改bank/actor owner。 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL_main.h>
#include "field_attack.h"
#include "field_ai.h"
#include "field_unit.h"
#include "field_game.h"
#include "field_background.h"
#include "field_unit_base.h"
#include "bios_clock.h"

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

static int test_original_raw_draw_order(void) {
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
    CHECK(vga.framebuffer[68 * VGA_STRIDE + 48] == 13);
    CHECK(vga.framebuffer[80 * VGA_STRIDE + 48] == 1);
    units.items[0].flags = FD2_FIELD_UNIT_FLAG_ACTED;
    fd2_field_units_render(&vga, &bank, &units, 0, 0, 0);
    CHECK(vga.framebuffer[68 * VGA_STRIDE + 48] == 13);
    CHECK(vga.framebuffer[80 * VGA_STRIDE + 48] == 25);
    return 0;
}

static int test_page_actor_raw_order_and_margin(void) {
    uint8_t rle[24*48]; uint32_t offsets[24];
    for(size_t f=0;f<24;f++) {
        offsets[f]=(uint32_t)(f*48);
        for(size_t row=0;row<24;row++) {
            rle[f*48+row*2]=23; rle[f*48+row*2+1]=(uint8_t)(f+1);
        }
    }
    fd2_map_sprite_bank bank={0};
    bank.data=rle;bank.size=sizeof(rle);bank.offsets=offsets;bank.frame_count=24;
    fd2_field_units units={0}; units.count=2;
    units.items[0].x=units.items[1].x=2;
    units.items[0].y=3;units.items[1].y=2;units.items[1].unit_id=1;
    units.items[1].frame_phase=2;
    fd2_field_units before=units;
    fd2_field_page *page=fd2_field_page_create(); CHECK(page);
    CHECK(fd2_field_units_render_page(page,&bank,&units,0,0,0,0,0)==0);
    uint8_t display[320*200]={0};
    CHECK(fd2_field_page_project(page,72,72,display,sizeof(display))==0);
    CHECK(display[72*320+52]==13 && display[84*320+52]==1);
    CHECK(!memcmp(&units,&before,sizeof(units)));
    /* camera上一格的脚部仍写入上沿margin，裁视窗之前必须保留。 */
    units.count=1;units.items[0].x=2;units.items[0].y=1;
    units.items[0].flags=FD2_FIELD_UNIT_FLAG_ACTED;
    units.items[0].detail_status[1]=1;
    CHECK(fd2_field_units_render_page(page,&bank,&units,2,2,3,0,1)==0);
    uint8_t pixel;
    CHECK(fd2_field_page_read(page,73,42,1,1,&pixel,1,1)==0 && pixel==25);
    CHECK(fd2_field_page_read(page,72,42,1,1,&pixel,1,1)==0 && pixel==0);
    /* 已确认class目录外只省该actor绘制，不阻断已证业务。 */
    fd2_field_page *empty=fd2_field_page_create(); CHECK(empty);
    units.count=2;units.items[1]=units.items[0];units.items[1].unit_id=2;
    CHECK(fd2_field_units_render_page(empty,&bank,&units,2,2,0,0,0)==0);
    CHECK(fd2_field_page_read(empty,72,42,1,1,&pixel,1,1)==0 && pixel==25);
    /* 目录内坏流仍整组原子失败，前项不先写。 */
    fd2_field_page *bad=fd2_field_page_create();CHECK(bad);
    units.items[1].unit_id=1;bank.size--;
    units.items[1].direction=3;units.items[1].detail_status[1]=0;
    CHECK(fd2_field_units_render_page(bad,&bank,&units,2,2,2,0,0)==-1);
    CHECK(fd2_field_page_read(bad,72,42,1,1,&pixel,1,1)==0 && pixel==0);
    bank.size++;fd2_field_page_destroy(bad);
    const uint8_t *source=(const uint8_t *)1;size_t length=99;
    CHECK(fd2_map_sprite_frame_source(&bank,24,&source,&length)==-1);
    CHECK(source==(const uint8_t *)1 && length==99);
    /* 原TMP允许offset重复/重排，源getter不可用下一offset减法。 */
    offsets[1]=0;offsets[0]=48;
    CHECK(fd2_map_sprite_frame_source(&bank,1,&source,&length)==0);
    CHECK(source==rle && length==48);
    /* 1da16专用模式不能套普通actor的raw26/acted/hidden规则。 */
    offsets[0]=0;offsets[1]=48;
    units.count=1;units.items[0].x=units.items[0].y=4;
    units.items[0].direction=0;units.items[0].frame_phase=0;
    units.items[0].flags=FD2_FIELD_UNIT_FLAG_ACTED|FD2_FIELD_UNIT_FLAG_HIDDEN;
    CHECK(fd2_field_map_action_page_actor(page,&bank,&units,0,0,0,3,2,0)==0);
    CHECK(fd2_field_page_read(page,168,162,1,1,&pixel,1,1)==0 && pixel==2);
    CHECK(fd2_field_map_action_page_actor(page,&bank,&units,0,0,0,3,2,2)==0);
    CHECK(fd2_field_page_read(page,168,162,1,1,&pixel,1,1)==0 && pixel==200);
    units.items[0].frame_phase=1;units.walk_frames[0]=0;
    CHECK(fd2_field_map_action_page_actor(page,&bank,&units,0,0,0,0,2,0)==0);
    CHECK(fd2_field_page_read(page,168,166,1,1,&pixel,1,1)==0 && pixel==3);
    CHECK(fd2_field_map_action_page_actor(page,&bank,&units,0,0,0,0,2,1)==-1);
    /* 正常127e0移动同样借共享cursor，不以walk_frames局部数组复位。 */
    units.items[0].flags=0;units.items[0].detail_status[1]=0;
    CHECK(fd2_field_units_render_page_actor(page,&bank,&units,0,0,0,0,3,0,0)==0);
    CHECK(fd2_field_page_read(page,168,166,1,1,&pixel,1,1)==0 && pixel==2);
    units.items[0].direction=5;units.items[0].frame_phase=0;
    units.sprite_cache_exact=1;units.sprite_cache_count=1;
    units.sprite_cache_ids[0]=0;units.sprite_cache_classes[0]=0;
    CHECK(fd2_field_map_action_page_actor(page,&bank,&units,0,0,0,0,0,2)==0);
    CHECK(fd2_field_units_render_page_actor(page,&bank,&units,0,0,0,0,0,0,0)==0);
    CHECK(fd2_field_page_read(page,168,166,1,1,&pixel,1,1)==0 && pixel==2);
    units.items[0].frame_phase=1;
    CHECK(fd2_field_map_action_page_actor(page,&bank,&units,0,0,0,0,0,2)==-1);
    /* 1c2da的phase3取frame2，且不借direction/hidden/raw26/位移。 */
    for(size_t row=0;row<24;row++)rle[96+row*2]=0x4b; /* frame2隔点RUN */
    uint8_t baseline[24*24];memset(baseline,77,sizeof(baseline));
    CHECK(fd2_field_page_write(page,168,162,24,24,baseline,sizeof(baseline),24)==0);
    units.items[0].direction=3;units.items[0].frame_phase=2;
    units.items[0].flags=FD2_FIELD_UNIT_FLAG_HIDDEN|FD2_FIELD_UNIT_FLAG_ACTED;
    units.items[0].detail_status[1]=1;
    CHECK(fd2_field_flash_page_actor(page,&bank,&units,0,0,0,3)==0);
    CHECK(fd2_field_page_read(page,168,162,1,1,&pixel,1,1)==0 && pixel==77);
    CHECK(fd2_field_page_read(page,169,162,1,1,&pixel,1,1)==0 && pixel==200);
    fd2_field_page_destroy(empty);fd2_field_page_destroy(page);return 0;
}

static int test_page_tile_skip_and_lut_domain(void) {
    uint8_t rle[120];uint32_t offsets[3]={0,48,72};
    for(size_t row=0;row<24;row++) {
        rle[row*2]=23;rle[row*2+1]=7;
        rle[48+row]=0xd7; /* 整行skip。 */
        rle[72+row*2]=23;rle[72+row*2+1]=0; /* 实写opaque0。 */
    }
    fd2_terrain_attr attrs[3]={0};fd2_terrain_tileset terrain={0};
    terrain.attrs=attrs;terrain.attr_count=3;
    terrain.sheet=(fd2_shape_sheet){24,24,3,rle,sizeof(rle),offsets};
    uint32_t cells[20*20]={0};fd2_field_map map={20,20,cells};
    fd2_field_path_node nodes[20*20]={0};
    nodes[0].can_stop=nodes[1].can_stop=1;
    fd2_field_path_result range={.width=20,.height=20,.nodes=nodes};
    uint8_t lut[256];for(size_t i=0;i<256;i++)lut[i]=(uint8_t)(i+100);
    fd2_field_page *page=fd2_field_page_create();CHECK(page);
    CHECK(fd2_terrain_render_page_block(page,&terrain,&map,72,72,2,1,0,0,
        0,0,NULL,NULL)==0);
    cells[0]=1;cells[1]=2;
    CHECK(fd2_terrain_render_page_block(page,&terrain,&map,72,72,2,1,0,0,
        0,0,&range,lut)==0);
    uint8_t actual[48];
    CHECK(fd2_field_page_read(page,72,72,48,1,actual,48,48)==0);
    for(size_t i=0;i<48;i++)CHECK(actual[i]==(i<24?107:100));
    /* 后格坏流，首格不得先覆写（映当前SKIP结果亦不可提前变207）。 */
    cells[0]=0;terrain.sheet.size=119;
    CHECK(fd2_terrain_render_page_block(page,&terrain,&map,72,72,2,1,0,0,
        0,0,&range,lut)==-1);
    uint8_t after[48];
    CHECK(fd2_field_page_read(page,72,72,48,1,after,48,48)==0);
    CHECK(!memcmp(actual,after,48));
    terrain.sheet.size=120;
    CHECK(fd2_terrain_render_page_block(page,&terrain,&map,72,72,14,8,0,0,
        0,0,NULL,NULL)==0);
    CHECK(fd2_terrain_render_page_block(page,&terrain,&map,72,48,13,9,0,0,
        0,0,NULL,NULL)==0);
    CHECK(fd2_terrain_render_page_block(page,&terrain,&map,72,72,17,8,0,0,
        0,0,NULL,NULL)==-1);
    fd2_field_page_destroy(page);return 0;
}

/* G1独立原资源审计g1_runtime_buffer.py的13个已定义来源点。
 * 只证明真实连续tile写保留历史；不是整个过场/原版截图颜色等价。 */
static int test_stage32_defined_skip_history(void) {
    static const uint16_t points[][3]={{39,141,190},{256,141,190},
        {257,141,189},{258,141,189},{259,141,189},{260,141,189},
        {261,141,189},{262,141,189},{263,141,189},{264,141,189},
        {265,141,189},{266,141,189},{267,141,189}};
    fd2_archive field={0},shape={0};fd2_field_map map={0};
    fd2_terrain_tileset terrain={0};
    CHECK(fd2_archive_open(&field,"original_game/FDFIELD.DAT")==0);
    CHECK(fd2_archive_open(&shape,"original_game/FDSHAP.DAT")==0);
    CHECK(fd2_field_map_open_entry(&map,&field,96)==0);
    CHECK(fd2_terrain_tileset_open_stage(&terrain,&shape,&field,32)==0);
    fd2_field_page *page=fd2_field_page_create(),*fresh=fd2_field_page_create();
    CHECK(page && fresh);
    CHECK(fd2_terrain_render_page_block(page,&terrain,&map,72,72,13,8,3,16,0,0,NULL,NULL)==0);
    CHECK(fd2_terrain_render_page_block(page,&terrain,&map,72,72,13,8,3,15,0,0,NULL,NULL)==0);
    CHECK(fd2_terrain_render_page_block(fresh,&terrain,&map,72,72,13,8,3,15,0,0,NULL,NULL)==0);
    for(size_t i=0;i<sizeof(points)/sizeof(points[0]);i++) {
        uint8_t pixel,initial;
        int x=points[i][0]-4+72,y=points[i][1]-4+72;
        CHECK(fd2_field_page_read(page,x,y,1,1,&pixel,1,1)==0);
        CHECK(fd2_field_page_read(fresh,x,y,1,1,&initial,1,1)==0);
        CHECK(pixel==points[i][2] && initial==0);
    }
    /* 同一真资源走正式Game→App page路径；VGA伪UI不可回灌透明底。
     * 仅resource-only stage32夹具，不开放其battle/S关卡逻辑。 */
    fd2_archive other={0};fd2_field_game *game=calloc(1,sizeof(*game));
    fd2_vga *vga=calloc(1,sizeof(*vga));CHECK(game && vga);
    CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_field_game_open_resources(game,&other,0)==0);
    /* 资源夹具只替换map/shape，不改变stage0支持门或加载晚关玩法。 */
    fd2_field_map_close(&game->map);fd2_terrain_tileset_close(&game->terrain);
    game->map=map;map=(fd2_field_map){0};
    game->terrain=terrain;terrain=(fd2_terrain_tileset){0};
    vga->field_page=fd2_field_page_create();CHECK(vga->field_page);
    CHECK(fd2_field_game_bind_visual(game,vga)==0);
    game->camera_cell_x=3;game->camera_cell_y=16;
    fd2_field_game_render(game,vga);
    CHECK(game->failure==FD2_FIELD_FAILURE_NONE);
    memset(vga->framebuffer,0x6d,sizeof(vga->framebuffer));
    game->camera_cell_y=15;
    fd2_field_game_render(game,vga);
    CHECK(game->failure==FD2_FIELD_FAILURE_NONE);
    for(size_t i=0;i<sizeof(points)/sizeof(points[0]);i++)
        CHECK(vga->framebuffer[points[i][1]*320u+points[i][0]]==points[i][2]);
    fd2_field_page_destroy(vga->field_page);free(vga);
    fd2_field_game_close(game);free(game);fd2_archive_close(&other);
    fd2_field_page_destroy(page);fd2_field_page_destroy(fresh);
    fd2_terrain_tileset_close(&terrain);fd2_field_map_close(&map);
    fd2_archive_close(&shape);fd2_archive_close(&field);return 0;
}

/* 合成两帧屋顶：base已是完整地形；overlay只恢复actor附近的遮挡。
 * 通过正式Game render检查空格cursor，旧全图及整range前景都会覆盖它。 */
static void init_overlay(fd2_field_game *game, uint32_t cells[25],
                         fd2_image images[4], fd2_terrain_attr attrs[4],
                         uint8_t pixels[4][24 * 24]) {
    memset(game, 0, sizeof(*game));
    memset(cells, 0, 25 * sizeof(*cells));
    memset(attrs, 0, 4 * sizeof(*attrs));
    for (size_t i = 0; i < 4; i++) {
        memset(pixels[i], (int)(10 + 10 * i), 24 * 24);
        images[i] = (fd2_image){24, 24, pixels[i]};
    }
    attrs[0].flags = 0x80;
    game->ready = 1;
    game->map = (fd2_field_map){.width=5, .height=5, .cells=cells};
    game->terrain.frames = images; game->terrain.sheet.frame_count = 4;
    game->terrain.attrs = attrs; game->terrain.attr_count = 4;
    game->active_side = 2;
    game->cursor_cell_x = game->cursor_cell_y = 2;
    game->visuals.cursor_frames[0] = (fd2_image){24, 24, pixels[2]};
    game->visuals.cursor_frame_count = 1;
    game->visuals.range_lut_count = FD2_FIELD_RANGE_LUT_COUNT;
    for (size_t i = 0; i < FD2_FIELD_RANGE_LUT_COUNT; i++)
        for (size_t color = 0; color < 256; color++)
            game->visuals.range_luts[i][color] = (uint8_t)(color + 1u);
}

static int test_empty_roof_cursor_and_selection(void) {
    fd2_field_game game;
    uint32_t cells[25]; fd2_image images[4]; fd2_terrain_attr attrs[4];
    uint8_t pixels[4][24 * 24]; fd2_vga vga = {0};
    init_overlay(&game, cells, images, attrs, pixels);
    fd2_field_path_node nodes[25] = {0};
    for (size_t i = 0; i < 25; i++) nodes[i].can_stop = 1;
    game.move_range = (fd2_field_path_result){.width=5, .height=5, .nodes=nodes};
    static const fd2_field_interaction interactions[] = {
        FD2_FIELD_INTERACTION_BROWSE, FD2_FIELD_INTERACTION_UNIT_SELECTED,
        FD2_FIELD_INTERACTION_TARGETING, FD2_FIELD_INTERACTION_MAGIC_TARGETING,
        FD2_FIELD_INTERACTION_ITEM_GIVE_TARGETING
    };
    for (size_t mode = 0; mode < sizeof(interactions)/sizeof(interactions[0]); mode++) {
        game.interaction = interactions[mode];
        fd2_field_game_render(&game, &vga);
        /* Magic的frame1未配置，单独检查空前景不盖base-LUT。 */
        uint8_t expected = interactions[mode] == FD2_FIELD_INTERACTION_MAGIC_TARGETING ? 11 : 30;
        CHECK(vga.framebuffer[53 * VGA_STRIDE + 53] == expected);
        CHECK(vga.framebuffer[0] == 0 && vga.framebuffer[199 * VGA_STRIDE + 319] == 0);
    }
    game.interaction = FD2_FIELD_INTERACTION_BROWSE;
    fd2_field_game_render_background(&game, &vga);
    CHECK(vga.framebuffer[53 * VGA_STRIDE + 53] == 10);
    return 0;
}

static int test_actor_local_overlay_cells_and_immune(void) {
    fd2_field_game game;
    uint32_t cells[25]; fd2_image images[4]; fd2_terrain_attr attrs[4];
    uint8_t pixels[4][24 * 24]; fd2_vga vga = {0};
    init_overlay(&game, cells, images, attrs, pixels);
    game.units.count = 1;
    fd2_field_unit *unit = &game.units.items[0];
    unit->x = unit->y = 2;
    for (unsigned walking = 0; walking < 2; walking++) for (unsigned direction = 0; direction < 4; direction++) {
        unit->frame_phase = (uint8_t)walking; unit->direction = (uint8_t)direction;
        fd2_field_units before = game.units;
        memset(vga.framebuffer, 9, sizeof(vga.framebuffer));
        fd2_field_units_render_terrain_overlays(&vga, &game.terrain, &game.map,
            &game.units, -4, -4, 0, NULL, NULL);
        for (int y = 0; y < 5; y++) for (int x = 0; x < 5; x++) {
            int selected = x == 2 && (y == 2 || y == 1);
            if (walking) {
                if (direction == 0) selected |= x == 2 && y == 3;
                else if (direction == 2) selected |= x == 2 && y == 0;
                else selected |= x == (direction == 1 ? 1 : 3) && (y == 1 || y == 2);
            }
            CHECK(vga.framebuffer[(5 + y*24) * VGA_STRIDE + 5 + x*24] == (selected ? 20 : 9));
        }
        CHECK(!memcmp(&before, &game.units, sizeof(before)));
    }
    unit->frame_phase = 0;
    for (unsigned kind = 0; kind < 5; kind++) {
        unit->flags = kind == 0 ? FD2_FIELD_UNIT_FLAG_HIDDEN : 0;
        unit->movement_profile = kind == 1 || kind == 4 ? 19 : 1;
        unit->race = kind == 2 ? 4 : kind == 3 ? 5 : 0;
        unit->unit_id = kind == 4 ? 28 : 0;
        unit->hp = 0; /* native没有HP门，28仍强制不免遮挡。 */
        memset(vga.framebuffer, 9, sizeof(vga.framebuffer));
        fd2_field_units_render_terrain_overlays(&vga, &game.terrain, &game.map,
            &game.units, -4, -4, 0, NULL, NULL);
        CHECK(vga.framebuffer[53 * VGA_STRIDE + 53] == (kind == 4 ? 20 : 9));
    }
    return 0;
}

static int test_overlay_lut_clip_and_readonly(void) {
    fd2_field_game game;
    uint32_t cells[25]; fd2_image images[4]; fd2_terrain_attr attrs[4];
    uint8_t pixels[4][24 * 24]; fd2_vga vga = {0};
    init_overlay(&game, cells, images, attrs, pixels);
    game.units.count = 1; game.units.items[0].x = game.units.items[0].y = 2;
    fd2_field_path_node nodes[25] = {0}; nodes[12].can_stop = 1;
    fd2_field_path_result range = {.width=5, .height=5, .nodes=nodes};
    uint8_t lut[256]; for (size_t i = 0; i < 256; i++) lut[i] = (uint8_t)(i + 7u);
    pixels[1][0] = 0;
    fd2_field_units before = game.units;
    fd2_field_path_node old_nodes[25]; memcpy(old_nodes, nodes, sizeof(nodes));
    memset(vga.framebuffer, 9, sizeof(vga.framebuffer));
    fd2_field_units_render_terrain_overlays(&vga, &game.terrain, &game.map,
        &game.units, -4, -4, 0, &range, lut);
    CHECK(vga.framebuffer[53 * VGA_STRIDE + 53] == 27);
    CHECK(vga.framebuffer[29 * VGA_STRIDE + 53] == 20);
    CHECK(vga.framebuffer[52 * VGA_STRIDE + 52] == 9); /* transparent0不映成7 */
    CHECK(vga.framebuffer[77 * VGA_STRIDE + 53] == 9); /* 无actor不追加range前景 */
    CHECK(!memcmp(&before, &game.units, sizeof(before)) && !memcmp(nodes, old_nodes, sizeof(nodes)));
    uint8_t snapshot[sizeof(vga.framebuffer)]; memcpy(snapshot, vga.framebuffer, sizeof(snapshot));
    fd2_terrain_render_field_overlay_cell(&vga, &game.terrain, &game.map,
        -1, 0, 0, 0, 0, NULL);
    fd2_terrain_render_field_overlay_cell(&vga, &game.terrain, &game.map,
        0, -1, 0, 0, 0, NULL);
    fd2_terrain_render_field_overlay_cell(&vga, &game.terrain, &game.map,
        0, 0, INT_MIN, INT_MAX, 0, NULL);
    CHECK(!memcmp(snapshot, vga.framebuffer, sizeof(snapshot)));
    memset(vga.framebuffer, 9, sizeof(vga.framebuffer));
    fd2_terrain_render_field_overlay_cell(&vga, &game.terrain, &game.map,
        0, 0, 23, 23, 0, NULL);
    CHECK(vga.framebuffer[0] == 20 && vga.framebuffer[1] == 9 && vga.framebuffer[VGA_STRIDE] == 9);
    /* 独立地图preview的旧全图前景入口保留，不借此推导actor集合。 */
    fd2_terrain_render_field_overlay(&vga, &game.terrain, &game.map, -4, -4, 0);
    CHECK(vga.framebuffer[77 * VGA_STRIDE + 53] == 20);
    return 0;
}

static uint32_t return_rng(void *userdata) {
    unsigned *calls = userdata;
    return ++*calls;
}

typedef struct {
    fd2_field_game *game;
    fd2_vga *vga;
    SDL_AtomicInt stop,done;
    int fired,failed;
} movement_host_probe;

/* Game只在主线程读取；worker仅把有限观察排入真实SDL事件泵，不开窗口。 */
static void SDLCALL movement_host_on_main(void *userdata) {
    movement_host_probe *probe=userdata;
    if(SDL_GetAtomicInt(&probe->stop))return;
    const fd2_field_unit *actor=&probe->game->units.items[0];
    if(actor->x==12u && actor->y==4u && actor->frame_phase==2u) {
        probe->vga->input.quit_requested=1;probe->fired++;
        SDL_SetAtomicInt(&probe->stop,1);
    }
}
static int SDLCALL movement_host_worker(void *userdata) {
    movement_host_probe *probe=userdata;
    uint64_t until=SDL_GetTicks()+3000u;
    while(!SDL_GetAtomicInt(&probe->stop) && SDL_GetTicks()<until) {
        if(!SDL_RunOnMainThread(movement_host_on_main,probe,true)){probe->failed=1;break;}
        SDL_Delay(1);
    }
    SDL_SetAtomicInt(&probe->done,1);return 0;
}

/* 真实Game→present→13460等待；首格已提交，次格相位2才送HOST。
 * 同时覆盖AI两种借用路径及March，纯像素/事件进程，不代WGC原版对照。 */
static int test_movement_host_prefix(void) {
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    for(unsigned kind=0;kind<4u;kind++) {
        fd2_input_install_interrupt_handlers();
        fd2_field_game *game=calloc(1,sizeof(*game));fd2_vga *vga=calloc(1,sizeof(*vga));
        CHECK(game && vga);
        uint32_t cells[256]={0};fd2_image images[4];fd2_terrain_attr attrs[4];uint8_t pixels[4][576];
        init_overlay(game,cells,images,attrs,pixels);
        game->map.width=game->map.height=16;game->turn_number=17;
        game->selected_unit=game->command_selected=game->attack_target=game->detail_unit=-1;
        game->interaction=FD2_FIELD_INTERACTION_BROWSE;
        memset(game->metadata.turn_events,0xff,sizeof(game->metadata.turn_events));
        memset(game->metadata.cell_lookup,0xff,sizeof(game->metadata.cell_lookup));
        cells[4u*16u+12u]=1u<<16;
        game->metadata.cell_lookup[0]=(fd2_field_cell_lookup){9,0};
        CHECK(fd2_field_units_set(&game->units,0,0,0,kind==1u || kind==2u?0:2,11,4)==0);
        CHECK(fd2_field_units_set(&game->units,1,1,1,2,kind==1u?14:1,kind==1u?4:1)==0);
        for(size_t i=0;i<2u;i++) {
            fd2_field_unit *unit=&game->units.items[i];
            unit->hp=unit->hp_max=50;unit->attack=200;unit->defense=1;unit->accuracy=100;
            unit->movement_profile=1;unit->movement_points=3;unit->level=1;
            ((uint8_t *)unit)[0x31]=255;
        }
        ((uint8_t *)&game->units.items[0])[0x0a]=0x40;
        game->active_side=kind==1u || kind==2u?0:2;
        game->phase_ai_pass=1;
        game->cursor_cell_x=13;game->cursor_cell_y=4;
        unsigned calls=0;game->attack_rng=return_rng;game->attack_rng_userdata=&calls;
        fd2_input_init(&vga->input);vga->info_panel_suppressed=1;
        game->visual_owner=&vga->field_visual;
        uint8_t *owned_path=malloc(4);CHECK(owned_path);memset(owned_path,3,4);
        game->move_path=owned_path;game->move_path_capacity=4;game->move_path_length=3;
        fd2_field_unit other=game->units.items[1];
        movement_host_probe probe={.game=game,.vga=vga};SDL_Thread *thread=NULL;
        if(kind==0u || kind==3u) {
            thread=SDL_CreateThread(movement_host_worker,"movement-host",&probe);CHECK(thread);
        } else vga->input.quit_requested=1;
        int result;
        if(kind==0u) {
            game->selected_unit=0;game->move_path_length=2;game->move_preview_valid=1;
            game->interaction=FD2_FIELD_INTERACTION_MOVING;
            result=fd2_field_game_execute_move(game,vga,55);
        } else if(kind==1u) {
            game->stage=UINT8_MAX;
            fd2_field_path_result range=FD2_FIELD_PATH_RESULT_INITIALIZER;
            fd2_field_ai_physical_candidate plan;
            CHECK(fd2_field_game_compute_move_range(game,0,&range)==0);
            CHECK(fd2_field_ai_choose_physical_candidate(&game->map,&game->terrain,&game->units,0,0,&range,&plan)==0);
            fd2_field_path_close(&range);CHECK(plan.priority>=6 && plan.destination_x!=11);
            result=fd2_field_game_commit_ai_physical(game,0,0,&plan,vga,55);
        } else if(kind==2u) {
            game->stage=UINT8_MAX;game->units.items[0].ai_behavior_raw=7;
            game->units.items[0].ai_param_35=13;game->units.items[0].ai_param_36=4;
            result=fd2_field_game_process_automatic_action_visual(game,vga,55,0);
        } else result=fd2_field_game_march(game,vga,55);
        if(thread) {
            SDL_SetAtomicInt(&probe.stop,1);
            while(!SDL_GetAtomicInt(&probe.done)){SDL_PumpEvents();SDL_Delay(1);}
            SDL_WaitThread(thread,NULL);
        }
        CHECK(result==(kind==1u?0:1) && game->runtime_host_quit && game->failure==FD2_FIELD_FAILURE_NONE);
        CHECK(!calls && !memcmp(&other,&game->units.items[1],sizeof(other)));
        CHECK(game->selected_unit==0 && game->interaction==FD2_FIELD_INTERACTION_MOVING &&
            game->units.items[0].direction==3 && !fd2_field_unit_has_acted(&game->units.items[0]));
        CHECK(game->turn_number==17u && game->active_side==(kind==1u || kind==2u?0:2) &&
            game->turn_post_caller==FD2_FIELD_TURN_POST_NONE && !game->event_log_count &&
            !game->event_resume.active && game->progress.resume==FD2_FIELD_PROGRESS_RESUME_NONE && !game->loot.source);
        CHECK(game->move_path==owned_path && game->move_path_capacity==4 &&
            game->move_path_length==(kind==0u?2u:3u) && game->move_preview_valid==(kind==0u));
        if(thread) {
            CHECK(probe.fired==1 && !probe.failed && game->units.items[0].x==12u &&
                game->units.items[0].y==4u && game->units.items[0].frame_phase==2u && game->units.walk_frames[0]==2u);
            CHECK(game->camera_cell_x==1 && game->camera_pixel_offset_x==8 &&
                game->cursor_cell_x==12 && game->cursor_cell_y==4 &&
                game->pending_cell_event.valid && game->pending_cell_event.action==9 &&
                game->pending_cell_event.source_slot==0);
        } else CHECK(game->units.items[0].x==11u && game->units.items[0].frame_phase==1u && !game->pending_cell_event.valid);
        if(kind==2u)CHECK(!game->ordinary_display_mode && game->phase_unit_cursor==1u);
        game->failure=FD2_FIELD_FAILURE_EVENT_DATA;
        fd2_field_game before=*game;
        CHECK(fd2_field_game_execute_move(game,vga,55)==-1 && !memcmp(game,&before,sizeof(before)));
        free(owned_path);free(vga);free(game);
    }
    fd2_input_install_interrupt_handlers();SDL_QuitSubSystem(SDL_INIT_EVENTS);return 0;
}

static int test_battle_return_plain_cursor_and_invalid(void) {
    fd2_field_game game;
    uint32_t cells[25]; fd2_image images[4]; fd2_terrain_attr attrs[4];
    uint8_t pixels[4][24 * 24]; fd2_vga vga = {0};
    init_overlay(&game, cells, images, attrs, pixels);
    game.visuals.cursor_frames[1] = (fd2_image){24, 24, pixels[3]};
    game.visuals.cursor_frame_count = 2;
    fd2_field_path_node nodes[25] = {0};
    for (size_t i = 0; i < 25; i++) nodes[i].can_stop = 1;
    game.move_range = (fd2_field_path_result){.width=5, .height=5, .nodes=nodes};
    unsigned calls = 0;
    game.attack_rng = return_rng; game.attack_rng_userdata = &calls;
    const fd2_field_interaction modes[] = {FD2_FIELD_INTERACTION_MAGIC_TARGETING,
        FD2_FIELD_INTERACTION_ITEM_TARGETING, FD2_FIELD_INTERACTION_MOVING};
    for (unsigned side = 0; side <= 2; side += 2) {
        game.active_side = (uint8_t)side;
        for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
            game.interaction = modes[i];
            fd2_field_game before = game;
            memset(vga.framebuffer, 0xa5, sizeof(vga.framebuffer));
            fd2_field_game_render_battle_return(&game, &vga);
            CHECK(vga.framebuffer[53 * VGA_STRIDE + 53] == 30); /* 普通frame0，不是magic frame1或range色11。 */
            CHECK(vga.framebuffer[0] == 0 && vga.framebuffer[63999] == 0);
            CHECK(!memcmp(&game, &before, sizeof(game)) && calls == 0);
        }
    }
    memset(&vga, 0xa5, sizeof(vga));
    fd2_vga untouched = vga;
    game.ready = 0;
    fd2_field_game before = game;
    fd2_field_game_render_battle_return(NULL, &vga);
    fd2_field_game_render_battle_return(&game, &vga);
    fd2_field_game_render_battle_return(&game, NULL);
    CHECK(!memcmp(&vga, &untouched, sizeof(vga)) && !memcmp(&game, &before, sizeof(game)) && !calls);
    game.ready = 1;
    before = game;
    fd2_field_game_render_battle_return(&game, NULL);
    CHECK(!memcmp(&vga, &untouched, sizeof(vga)) && !memcmp(&game, &before, sizeof(game)) && !calls);
    return 0;
}

/* 原51a83=0仍保留info；与物理返回只应在未被actor/前景遮住的cursor格有差异。 */
static int test_magic_background_hides_cursor_without_writes(void) {
    fd2_field_game game;
    uint32_t cells[25]; fd2_image images[4]; fd2_terrain_attr attrs[4];
    uint8_t pixels[4][24 * 24]; fd2_vga pages[2] = {0};
    init_overlay(&game, cells, images, attrs, pixels);
    game.interaction = FD2_FIELD_INTERACTION_MAGIC_TARGETING;
    game.last_idle_ms = game.last_terrain_visual_ms = 1000;
    unsigned calls = 0;
    game.attack_rng = return_rng; game.attack_rng_userdata = &calls;
    fd2_field_game before = game;
    fd2_field_game_render_battle_return(&game, &pages[0]);
    fd2_field_game_render_effect_background(&game, &pages[1]);
    size_t changed = 0;
    for (int y = 0; y < 200; y++) for (int x = 0; x < 320; x++) {
        if (pages[0].framebuffer[y * VGA_STRIDE + x] ==
            pages[1].framebuffer[y * VGA_STRIDE + x]) continue;
        CHECK(x >= 52 && x < 76 && y >= 52 && y < 76);
        CHECK(pages[0].framebuffer[y * VGA_STRIDE + x] == 30);
        CHECK(pages[1].framebuffer[y * VGA_STRIDE + x] == 10);
        changed++;
    }
    CHECK(changed == 24 * 24 && !memcmp(&game, &before, sizeof(game)) && !calls);
    memset(&pages[1], 0xa5, sizeof(pages[1])); pages[0] = pages[1];
    fd2_field_game_render_effect_background(NULL, &pages[1]);
    fd2_field_game_render_effect_background(&game, NULL);
    game.ready = 0; before = game;
    fd2_field_game_render_effect_background(&game, &pages[1]);
    CHECK(!memcmp(&pages[0], &pages[1], sizeof(pages[0])));
    CHECK(!memcmp(&game, &before, sizeof(game)) && !calls);
    return 0;
}

/* 用既有正常BROWSE绘制作为独立预期；只暂改/恢复已声明fixture字段。
 * 此处的before仅用于memcmp，不关闭或转移任何共享resource的浅快照。 */
static int expected_return_browse(fd2_field_game *game, fd2_vga *vga) {
    fd2_field_game before = *game;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->active_side = 2;
    game->selected_unit = -1;
    game->detail_visible = 0;
    game->system_result_fragment = 0;
    game->manual_slot_result_fragment = 0;
    memset(vga->framebuffer, 0, sizeof(vga->framebuffer));
    fd2_field_game_render(game, vga);
    game->interaction = before.interaction;
    game->active_side = before.active_side;
    game->selected_unit = before.selected_unit;
    game->detail_visible = before.detail_visible;
    game->system_result_fragment = before.system_result_fragment;
    game->manual_slot_result_fragment = before.manual_slot_result_fragment;
    game->info_panel_right = before.info_panel_right;
    CHECK(!memcmp(game, &before, sizeof(before)));
    return 0;
}

static int test_battle_return_current_map_and_hp(void) {
    fd2_archive fdother = {0};
    fd2_field_game *game = calloc(1, sizeof(*game));
    fd2_vga *pages = calloc(4, sizeof(*pages));
    CHECK(game && pages);
    CHECK(fd2_archive_open(&fdother, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_field_game_open_resources(game, &fdother, 0) == 0);
    CHECK(fd2_field_game_load_active(game, "original_game/START.SAV") == 0);
    fd2_field_unit *hero = &game->units.items[0];
    CHECK(hero->hp > 1 && hero->x >= 2 && hero->y >= 6);
    game->camera_cell_x = hero->x - 2; game->camera_cell_y = hero->y - 6;
    game->cursor_cell_x = hero->x; game->cursor_cell_y = hero->y;
    game->info_panel_right = 0; /* rel(2,6)应投影到右板，但const返回不提交该owner。 */
    unsigned calls = 0;
    game->attack_rng = return_rng; game->attack_rng_userdata = &calls;
    CHECK(expected_return_browse(game, &pages[0]) == 0);
    hero->hp--; /* 只改私有当前record，模拟已提交伤害；绝不写SAV或重算战斗。 */
    CHECK(expected_return_browse(game, &pages[1]) == 0);
    size_t hp_pixels_changed = 0;
    for (int y = 161; y < 195; y++) for (int x = 246; x < 315; x++)
        hp_pixels_changed += pages[0].framebuffer[y * VGA_STRIDE + x] !=
                             pages[1].framebuffer[y * VGA_STRIDE + x];
    CHECK(hp_pixels_changed > 0); /* 旧快照不能冒当前HP的返回页。 */
    CHECK(fd2_field_attack_range_compute(&game->move_range, &game->map,
        &game->terrain, hero) == 0);
    game->range_visual_phase = 0;
    game->selected_unit = 0;
    game->command_selected = FD2_FIELD_COMMAND_WAIT;
    game->command_highlight_phase = 1;
    memset(game->command_disabled, 0, sizeof(game->command_disabled));
    const fd2_field_interaction modes[] = {FD2_FIELD_INTERACTION_TARGETING,
        FD2_FIELD_INTERACTION_COMMAND, FD2_FIELD_INTERACTION_ITEM_MENU,
        FD2_FIELD_INTERACTION_MANUAL_SLOT};
    uint32_t cells_before[24 * 24];
    CHECK(game->map.width == 24 && game->map.height == 24);
    memcpy(cells_before, game->map.cells, sizeof(cells_before));
    for (unsigned side = 0; side <= 2; side += 2) for (size_t i = 0; i < sizeof(modes) / sizeof(modes[0]); i++) {
        game->active_side = (uint8_t)side; game->interaction = modes[i];
        fd2_field_game before = *game;
        CHECK(expected_return_browse(game, &pages[1]) == 0);
        memset(&pages[2], 0x5a, sizeof(pages[2]));
        pages[2].field_page=NULL; /* 该fixture只验只读VGA返回页，不借App工作页。 */
        pages[2].info_panel_suppressed=0;
        fd2_vga vga_before = pages[2];
        fd2_field_game_render_battle_return(game, &pages[2]);
        CHECK(!memcmp(pages[2].framebuffer, pages[1].framebuffer, sizeof(pages[2].framebuffer)));
        CHECK(!memcmp((uint8_t *)&pages[2] + sizeof(pages[2].framebuffer),
                      (uint8_t *)&vga_before + sizeof(vga_before.framebuffer),
                      sizeof(vga_before) - sizeof(vga_before.framebuffer)));
        CHECK(!memcmp(game, &before, sizeof(before)) && calls == 0 && !game->info_panel_right);
        CHECK(!memcmp(game->map.cells, cells_before, sizeof(cells_before)));
        if (side == 2) {
            /* 负对照证明原fixture确有范围、COMMAND或modal，非空页自洽。 */
            memset(pages[3].framebuffer, 0, sizeof(pages[3].framebuffer));
            fd2_field_game_render(game, &pages[3]);
            game->info_panel_right = before.info_panel_right;
            CHECK(!memcmp(game, &before, sizeof(before)));
            CHECK(memcmp(pages[3].framebuffer, pages[2].framebuffer, sizeof(pages[2].framebuffer)) != 0);
        }
    }
    fd2_field_game_close(game);
    fd2_archive_close(&fdother);
    free(pages); free(game);
    return 0;
}

/* 22046的22077 CALL127a9位于两个219ad pass之间。固定色可区分
 * terrain两次LUT、actor/actor-FG仅后一次LUT，而非复刻mask几何公式。 */
static int test_transition_mask_actor_layer_and_preview(void) {
    fd2_field_game *game = calloc(1, sizeof(*game));
    fd2_field_game *before = malloc(sizeof(*before));
    fd2_vga *pages = calloc(3, sizeof(*pages));
    uint32_t cells[25], old_cells[25];
    fd2_image images[4]; fd2_terrain_attr attrs[4];
    uint8_t pixels[4][24*24], old_frames[sizeof(frames)];
    fd2_field_path_node nodes[25] = {0}, old_nodes[25];
    CHECK(game && before && pages);
    init_overlay(game,cells,images,attrs,pixels);
    memset(frames,100,12*24*24);
    memset(frames+12*24*24,150,12*24*24);
    memcpy(old_frames,frames,sizeof(frames));
    game->sprites.decoded_frames = frames; game->sprites.frame_count = 24;
    game->units.count = 2;
    game->units.items[0].x = 1; game->units.items[0].y = 2;
    game->units.items[0].movement_profile = 19; /* 1f183：不恢复actor前景。 */
    game->units.items[1].unit_id = 1;
    game->units.items[1].x = 3; game->units.items[1].y = 2;
    game->units.items[1].movement_profile = 1;
    game->selected_unit = 0; game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    game->command_highlight_phase = 1;
    for (size_t i = 0; i < 25; i++) nodes[i].can_stop = 1;
    game->move_range = (fd2_field_path_result){.width=5,.height=5,.nodes=nodes};
    unsigned calls = 0;
    game->attack_rng = return_rng; game->attack_rng_userdata = &calls;
    memcpy(old_cells,cells,sizeof(cells)); memcpy(old_nodes,nodes,sizeof(nodes));
    uint8_t lut[256], old_lut[256];
    for (unsigned i = 0; i < 256; i++) lut[i] = (uint8_t)(i+1u);
    memcpy(old_lut,lut,sizeof(lut));
    for (unsigned side = 0; side <= 2; side += 2) {
        game->active_side = (uint8_t)side; *before = *game;
        memset(&pages[0],0xa5,sizeof(pages[0]));
        /* 此夹具刻意走decoded旧VGA；填充metadata不能伪造App owner指针。 */
        pages[0].field_page=NULL;
        pages[1] = pages[0];
        CHECK(fd2_field_game_render_transition_mask(game,&pages[0],lut,40,60,40) == 0);
        CHECK(pages[0].framebuffer[56*VGA_STRIDE+32] == 101); /* actor只后pass。 */
        CHECK(pages[0].framebuffer[56*VGA_STRIDE+80] == 21); /* actor-FG只后pass。 */
        CHECK(pages[0].framebuffer[56*VGA_STRIDE+56] == 12); /* terrain两pass。 */
        CHECK(pages[0].framebuffer[8*VGA_STRIDE+44] == 11); /* 上矩形，圆外。 */
        CHECK(pages[0].framebuffer[8*VGA_STRIDE+108] == 10); /* 半宽64，右端exclusive。 */
        CHECK(pages[0].framebuffer[0] == 0 && pages[0].framebuffer[63999] == 0);
        CHECK(!memcmp((uint8_t*)&pages[0]+sizeof(pages[0].framebuffer),
                      (uint8_t*)&pages[1]+sizeof(pages[1].framebuffer),
                      sizeof(pages[0])-sizeof(pages[0].framebuffer)));
        if (side == 0) memcpy(pages[2].framebuffer,pages[0].framebuffer,sizeof(pages[0].framebuffer));
        else CHECK(!memcmp(pages[2].framebuffer,pages[0].framebuffer,sizeof(pages[0].framebuffer)));
        CHECK(!memcmp(game,before,sizeof(*game)) && !calls);
    }
    /* preview每帧必须复用同一合成入口，9→1原LUT而非range phase。 */
    for (size_t i = 0; i < FD2_FIELD_RANGE_LUT_COUNT; i++)
        for (unsigned color = 0; color < 256; color++)
            game->visuals.range_luts[i][color] = (uint8_t)(color+i);
    *before = *game;
    fd2_field_effect_frames effect = {0};
    CHECK(fd2_field_game_prepare_stage_transition(game,&pages[0],40,60,40,1,&effect) == 0);
    CHECK(effect.snapshot_count == 9);
    for (size_t frame = 0; frame < 9; frame++) {
        const uint8_t *snapshot = fd2_field_effect_snapshot(&effect,frame);
        unsigned level = (unsigned)(9-frame);
        CHECK(snapshot && snapshot[56*VGA_STRIDE+32] == 100+level);
        CHECK(snapshot[56*VGA_STRIDE+80] == 20+level);
        CHECK(snapshot[56*VGA_STRIDE+56] == 10+2*level);
        CHECK(fd2_field_game_render_transition_mask(game,&pages[1],
                  game->visuals.range_luts[9-frame],40,60,40+(int)frame) == 0);
        CHECK(!memcmp(snapshot,pages[1].framebuffer,sizeof(pages[1].framebuffer)));
    }
    CHECK(!memcmp(game,before,sizeof(*game)) && !calls);
    fd2_field_effect_frames_close(&effect);
    CHECK(!effect.storage);

    memset(&pages[0],0x5a,sizeof(pages[0])); pages[1] = pages[0];
    CHECK(fd2_field_game_render_transition_mask(NULL,&pages[0],lut,0,0,40) == -1);
    CHECK(fd2_field_game_render_transition_mask(game,NULL,lut,0,0,40) == -1);
    CHECK(fd2_field_game_render_transition_mask(game,&pages[0],NULL,0,0,40) == -1);
    CHECK(fd2_field_game_render_transition_mask(game,&pages[0],lut,0,0,0) == -1);
    CHECK(fd2_field_game_render_transition_mask(game,&pages[0],lut,0,0,4097) == -1);
    CHECK(fd2_field_game_prepare_stage_transition(game,&pages[0],0,0,1,-1,&effect) == -1);
    CHECK(fd2_field_game_prepare_stage_transition(game,&pages[0],0,0,4096,1,&effect) == -1);
    CHECK(!effect.storage);
    CHECK(!memcmp(game,before,sizeof(*game)) && !calls);
    game->ready = 0; *before = *game;
    CHECK(fd2_field_game_render_transition_mask(game,&pages[0],lut,0,0,40) == -1);
    CHECK(!memcmp(game,before,sizeof(*game)) && !memcmp(&pages[0],&pages[1],sizeof(pages[0])));
    CHECK(!memcmp(cells,old_cells,sizeof(cells)) && !memcmp(nodes,old_nodes,sizeof(nodes)));
    CHECK(!memcmp(frames,old_frames,sizeof(frames)) && !memcmp(lut,old_lut,sizeof(lut)));
    free(pages); free(before); free(game);
    return 0;
}


static int unchanged_except_visual(const fd2_field_game *g,const fd2_field_game *before) {
    fd2_field_game *copy=malloc(sizeof(*copy));if(!copy)return 0;*copy=*g;
    copy->idle_phase=before->idle_phase;copy->last_idle_ms=before->last_idle_ms;
    copy->terrain_visual_phase=before->terrain_visual_phase;
    copy->last_terrain_visual_ms=before->last_terrain_visual_ms;
    int same=!memcmp(copy,before,sizeof(*copy));free(copy);return same;
}

/* 同一真实Game锚由普通tick和11cac点借用；页面读取当前相位，无第二套时钟。 */
static int test_visual_phase_projection_during_blocking_animation(void) {
    fd2_archive fdother={0};
    fd2_field_game *g=calloc(1,sizeof(*g)),*before=malloc(sizeof(*before));
    fd2_vga *pages=calloc(2,sizeof(*pages));CHECK(g && before && pages);
    CHECK(fd2_archive_open(&fdother,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_field_game_open_resources(g,&fdother,0)==0);
    CHECK(fd2_field_game_load_active(g,"original_game/START.SAV")==0);
    g->active_side=2;g->interaction=FD2_FIELD_INTERACTION_BROWSE;
    g->cursor_cell_x=g->units.items[0].x;g->cursor_cell_y=g->units.items[0].y;
    g->camera_cell_x=g->cursor_cell_x-4;g->camera_cell_y=g->cursor_cell_y-4;
    g->idle_phase=g->terrain_visual_phase=0;
    g->last_idle_ms=g->last_terrain_visual_ms=0;
    unsigned calls=0;g->attack_rng=return_rng;g->attack_rng_userdata=&calls;
    const struct { uint64_t now,last_idle,last_terrain;uint8_t idle,terrain; } cases[]={
        {0,0,0,0,0},{1000,1000,1000,0,0},{4000,4000,4000,1,1},
        {4200,4000,4200,1,0},{4475,4475,4475,2,1},
        {100,100,100,3,0},{300,100,300,3,1},{375,375,375,0,0}
    };
    uint32_t *cells=malloc((size_t)g->map.width*g->map.height*sizeof(*cells));CHECK(cells);
    size_t map_bytes=(size_t)g->map.width*g->map.height*sizeof(*cells);
    memcpy(cells,g->map.cells,map_bytes);
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        *before=*g;
        CHECK(fd2_field_game_tick_visual(g,(cases[i].now)*UINT64_C(1000000))==0);
        CHECK(g->idle_phase==cases[i].idle && g->terrain_visual_phase==cases[i].terrain);
        CHECK(g->last_idle_ms==cases[i].last_idle && g->last_terrain_visual_ms==cases[i].last_terrain);
        CHECK(unchanged_except_visual(g,before) && !calls && !memcmp(cells,g->map.cells,map_bytes));
        *before=*g;
        fd2_field_game_render_effect_background(g,&pages[0]);
        size_t cursor_count=g->visuals.cursor_frame_count;g->visuals.cursor_frame_count=0;
        fd2_field_game_render_battle_return(g,&pages[1]);g->visuals.cursor_frame_count=cursor_count;
        CHECK(!memcmp(pages[0].framebuffer,pages[1].framebuffer,64000));
        CHECK(!memcmp(g,before,sizeof(*g)) && !calls);
        /* 独立重画底栏：phase3按原1acf3映射1，HP数字及terrain不随idle改变。 */
        fd2_vga *panel=calloc(1,sizeof(*panel));CHECK(panel);
        uint32_t cell=fd2_field_map_cell(&g->map,g->cursor_cell_x,g->cursor_cell_y);
        size_t frame;CHECK(fd2_terrain_base_frame_from_cell(&g->terrain,cell,&frame)==0);
        const fd2_terrain_attr *attr=fd2_terrain_attr_get(&g->terrain,fd2_field_cell_terrain(cell));CHECK(attr);
        int32_t attack,defense;CHECK(fd2_field_attack_terrain_modifiers(attr->movement_cost_class,&attack,&defense)==0);
        fd2_field_unit *u=&g->units.items[0];
        fd2_image unit={24,24,g->sprites.decoded_frames+((size_t)u->unit_id*12u+(g->idle_phase==3?1u:g->idle_phase))*576u};
        fd2_field_info_draw(panel,&g->info_assets,0,fd2_terrain_frame(&g->terrain,frame),&unit,u->hp,u->hp_max,attack,defense);
        for(int y=161;y<195;y++)CHECK(!memcmp(pages[0].framebuffer+y*320+5,panel->framebuffer+y*320+5,69));
        free(panel);
    }
    *before=*g;g->failure=FD2_FIELD_FAILURE_EVENT_DATA;before->failure=g->failure;
    CHECK(fd2_field_game_tick_visual(g,(10000)*UINT64_C(1000000))==-1 && !memcmp(g,before,sizeof(*g)) && !calls);
    g->failure=FD2_FIELD_FAILURE_NONE;g->ready=0;*before=*g;
    CHECK(fd2_field_game_tick_visual(g,(10000)*UINT64_C(1000000))==-1 && !memcmp(g,before,sizeof(*g)));
    CHECK(fd2_field_game_tick_visual(NULL,(10000)*UINT64_C(1000000))==-1);
    free(cells);free(before);free(pages);fd2_field_game_close(g);free(g);fd2_archive_close(&fdother);return 0;
}

/* 原1297d长时间无调用也只加一次，且下一次从新last计时。旧追赶循环真RED。 */
static int test_normal_tick_single_visual_advance(void) {
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);
    g->ready=1;g->active_side=2;
    fd2_field_game_tick(g,(0)*UINT64_C(1000000));
    CHECK(g->idle_phase==0 && g->last_idle_ms==0);
    fd2_field_game_tick(g,(1000)*UINT64_C(1000000));
    CHECK(g->idle_phase==0 && g->last_idle_ms==1000);
    fd2_field_game_tick(g,(4000)*UINT64_C(1000000));
    CHECK(g->idle_phase==1 && g->last_idle_ms==4000);
    fd2_field_game_tick(g,(4200)*UINT64_C(1000000));
    CHECK(g->idle_phase==1 && g->last_idle_ms==4000);
    fd2_field_game_tick(g,(4475)*UINT64_C(1000000));
    CHECK(g->idle_phase==2 && g->last_idle_ms==4475);
    free(g);return 0;
}

/* 1e611只经11eee的terrain钟；独立期望冻结两字段写集和原组合四字段合同。
 * 这是既有55/275ms宿主近似的C回归，不是DOS BIOS时钟或全帧对照。 */
static int test_terrain_only_visual_clock(void) {
    fd2_field_game *g=calloc(1,sizeof(*g)),*expected=malloc(sizeof(*expected)),*terrain=malloc(sizeof(*terrain));
    CHECK(g && expected && terrain);
    uint32_t cell=0x12345678u;unsigned calls=0;
    g->ready=1;g->active_side=2;g->selected_unit=0;g->interaction=FD2_FIELD_INTERACTION_COMMAND;
    g->map.width=g->map.height=1;g->map.cells=&cell;
    g->units.count=1;g->units.items[0].hp=43;
    g->idle_phase=3;g->last_idle_ms=876;
    g->range_visual_phase=2;g->last_range_visual_ms=654;
    g->command_highlight_phase=1;g->last_command_highlight_ms=321;
    g->attack_rng=return_rng;g->attack_rng_userdata=&calls;
    const struct { uint64_t now,last;uint8_t phase; } only[]={
        {0,0,0},{7,7,0},{61,7,0},{62,62,1},{62,62,1},
        {117,117,0},{118,117,0},{999999,999999,1},
        {100,100,0},{100,100,0},{154,100,0},{155,155,1}
    };
    for(size_t i=0;i<sizeof(only)/sizeof(only[0]);i++) {
        *expected=*g;expected->terrain_visual_phase=only[i].phase;
        expected->last_terrain_visual_ms=only[i].last;
        CHECK(fd2_field_game_tick_terrain_visual(g,(only[i].now)*UINT64_C(1000000))==0);
        CHECK(!memcmp(g,expected,sizeof(*g)) && cell==0x12345678u && !calls);
    }
    g->idle_phase=2;g->last_idle_ms=1000;
    g->terrain_visual_phase=1;g->last_terrain_visual_ms=1000;
    const struct { uint64_t now,idle_last,terrain_last;uint8_t idle,phase; } combined[]={
        {1000,1000,1000,2,1},{1054,1000,1000,2,1},{1055,1000,1055,2,0},
        {1274,1000,1274,2,1},{1275,1275,1274,3,1},{9000,9000,9000,0,0},
        {10,10,10,1,1},{10,10,10,1,1}
    };
    for(size_t i=0;i<sizeof(combined)/sizeof(combined[0]);i++) {
        *terrain=*g;*expected=*g;
        expected->terrain_visual_phase=combined[i].phase;
        expected->last_terrain_visual_ms=combined[i].terrain_last;
        CHECK(fd2_field_game_tick_terrain_visual(terrain,(combined[i].now)*UINT64_C(1000000))==0);
        CHECK(!memcmp(terrain,expected,sizeof(*terrain)));
        expected->idle_phase=combined[i].idle;expected->last_idle_ms=combined[i].idle_last;
        CHECK(fd2_field_game_tick_visual(g,(combined[i].now)*UINT64_C(1000000))==0);
        CHECK(!memcmp(g,expected,sizeof(*g)) && cell==0x12345678u && !calls);
    }
    g->ready=0;*expected=*g;
    CHECK(fd2_field_game_tick_terrain_visual(g,(10000)*UINT64_C(1000000))==-1 && !memcmp(g,expected,sizeof(*g)));
    CHECK(fd2_field_game_tick_visual(g,(10000)*UINT64_C(1000000))==-1 && !memcmp(g,expected,sizeof(*g)));
    g->ready=1;g->failure=FD2_FIELD_FAILURE_ACTION;*expected=*g;
    CHECK(fd2_field_game_tick_terrain_visual(g,(10000)*UINT64_C(1000000))==-1 && !memcmp(g,expected,sizeof(*g)));
    CHECK(fd2_field_game_tick_visual(g,(10000)*UINT64_C(1000000))==-1 && !memcmp(g,expected,sizeof(*g)));
    CHECK(fd2_field_game_tick_terrain_visual(NULL,(10000)*UINT64_C(1000000))==-1 && !calls);
    free(terrain);free(expected);free(g);return 0;
}

/* 1c2da→4e127的真实常色来自原stride456低byte=200，arg_C无效。
 * 使用有孔的独立sprite检查全部像素、边裁和旧cache身份，不借生产getter算期望。 */
static int test_actor_group_flash_solid_color(void) {
    fd2_field_game *game=calloc(1,sizeof(*game)), *before=malloc(sizeof(*before));
    fd2_vga *vga=calloc(1,sizeof(*vga));
    uint8_t *baseline=malloc(VGA_W*VGA_H), *colored=malloc(VGA_W*VGA_H);
    CHECK(game && before && vga && baseline && colored);
    uint32_t cells[25], old_cells[25];
    fd2_image images[4]; fd2_terrain_attr attrs[4]; uint8_t pixels[4][24*24];
    init_overlay(game,cells,images,attrs,pixels);
    init_frames();
    for (int row=0;row<24;row++) for (int col=0;col<24;col++)
        frames[2u*576u+(size_t)row*24u+(size_t)col]=(row+col)%3 ? 73u:0u;
    uint8_t old_frames[sizeof(frames)];memcpy(old_frames,frames,sizeof(frames));
    memcpy(old_cells,cells,sizeof(cells));
    game->sprites.decoded_frames=frames;game->sprites.frame_count=24;
    game->units.count=1;
    CHECK(fd2_field_units_sprite_cache_append(&game->units,0)==0);
    /* 原hot身份写不刷新cache；flash固定朝向、idle3→2，忽略步行与raw26。 */
    game->units.items[0].unit_id=1;game->units.items[0].direction=3;
    game->units.items[0].frame_phase=6;game->units.items[0].detail_status[1]=7;
    game->units.walk_frames[0]=1;game->idle_phase=3;
    CHECK(fd2_field_unit_cached_sprite_id(&game->units,0)==0);
    unsigned calls=0;game->attack_rng=return_rng;game->attack_rng_userdata=&calls;
    const uint8_t actor[]={0}, invalid[]={1}, colors[]={0,64,94,192,200,255};
    const int placements[][4]={{2,2,0,0},{0,0,8,8},{4,4,-200,-80}};
    for (size_t p=0;p<sizeof(placements)/sizeof(placements[0]);p++) {
        game->units.items[0].x=(uint8_t)placements[p][0];
        game->units.items[0].y=(uint8_t)placements[p][1];
        game->camera_pixel_offset_x=placements[p][2];
        game->camera_pixel_offset_y=placements[p][3];
        int left=placements[p][0]*24+4-placements[p][2];
        int top=placements[p][1]*24-2-placements[p][3];
        *before=*game;
        for (size_t c=0;c<sizeof(colors);c++) {
            fd2_field_effect_frames effect={0};
            CHECK(fd2_field_game_prepare_actor_group_flash(game,vga,actor,1,colors[c],&effect)==0);
            CHECK(effect.snapshot_count==2 && effect.snapshots[0] && effect.snapshots[1]);
            if (!c) {
                memcpy(baseline,effect.snapshots[0],VGA_W*VGA_H);
                memcpy(colored,effect.snapshots[1],VGA_W*VGA_H);
            }
            CHECK(!memcmp(baseline,effect.snapshots[0],VGA_W*VGA_H));
            CHECK(!memcmp(colored,effect.snapshots[1],VGA_W*VGA_H));
            size_t changed=0;
            for (int y=0;y<VGA_H;y++) for (int x=0;x<VGA_W;x++) {
                size_t index=(size_t)y*VGA_STRIDE+(size_t)x;
                int row=y-top,col=x-left;
                int solid=x>=4 && x<316 && y>=4 && y<196 &&
                    row>=0 && row<24 && col>=0 && col<24 &&
                    old_frames[2u*576u+(size_t)row*24u+(size_t)col]!=0;
                CHECK(effect.snapshots[1][index]==(solid ? 200u:baseline[index]));
                changed+=(size_t)(effect.snapshots[1][index]!=baseline[index]);
            }
            CHECK(changed && !memcmp(game,before,sizeof(*game)) && !calls);
            CHECK(!memcmp(cells,old_cells,sizeof(cells)) && !memcmp(frames,old_frames,sizeof(frames)));
            /* 预备后直接释放即取消，没有业务或视觉owner写点。 */
            fd2_field_effect_frames_close(&effect);
            CHECK(!effect.storage && !memcmp(game,before,sizeof(*game)) && !calls);
        }
        fd2_field_effect_frames rejected={0}, empty={0};
        CHECK(fd2_field_game_prepare_actor_group_flash(game,vga,invalid,1,94,&rejected)==-1);
        CHECK(!memcmp(&rejected,&empty,sizeof(rejected)) && !memcmp(vga->framebuffer,baseline,VGA_W*VGA_H));
        CHECK(!memcmp(game,before,sizeof(*game)) && !calls);
    }
    free(colored);free(baseline);free(vga);free(before);free(game);return 0;
}

/* 原once特效组合检查迁入真实资源的无窗口夹具；timed=0、audio=NULL，
 * 保留flash／earthquake／transition的错误门、快照差异与最终帧。 */
static int validate_field_effects(fd2_field_game *game, fd2_vga *vga,
                                  fd2_field_audio *field_audio) {
    if (!game || !game->ready || !vga || game->units.count == 0) return -1;
    int result = -1;
    fd2_field_effect_frames effect_frames = {0};
    uint8_t units[] = {0};
    uint8_t invalid_unit[] = {(uint8_t)game->units.count};

    if (fd2_field_game_prepare_actor_group_flash(
            game, vga, invalid_unit, 1, 0xc0, &effect_frames) == 0 ||
        effect_frames.storage != NULL ||
        fd2_field_game_prepare_stage_transition(
            game, vga, 0, 0, 0, 8, &effect_frames) == 0 ||
        effect_frames.storage != NULL)
        goto done;
    if (fd2_field_game_prepare_stage_transition(
            game, vga, INT_MIN, INT_MAX, 10, 8, &effect_frames) != 0)
        goto done;
    fd2_field_effect_frames_close(&effect_frames);

    if (fd2_field_game_prepare_actor_group_flash(
            game, vga, units, 1, 0xc0, &effect_frames) != 0 ||
        !effect_frames.snapshots[0] || !effect_frames.snapshots[1] ||
        memcmp(effect_frames.snapshots[0], effect_frames.snapshots[1], VGA_W * VGA_H) == 0 ||
        fd2_field_effect_play(&effect_frames, vga, NULL, 0) != 0 ||
        memcmp(vga->framebuffer, effect_frames.snapshots[0], VGA_W * VGA_H) != 0)
        goto done;
    fd2_field_effect_frames_close(&effect_frames);
    if (fd2_field_game_play_actor_group_flash(
            game, vga, field_audio, units, 1, 0xc0, 0) != 0)
        goto done;

    if (fd2_field_game_prepare_earthquake(game, vga, &effect_frames) != 0 ||
        memcmp(effect_frames.snapshots[0], effect_frames.snapshots[1], VGA_W * VGA_H) == 0 ||
        memcmp(effect_frames.snapshots[1], effect_frames.snapshots[2], VGA_W * VGA_H) == 0 ||
        fd2_field_effect_play(&effect_frames, vga, NULL, 0) != 0 ||
        /* 第 59 帧是 cycle[3]，应落在 source 1。 */
        memcmp(vga->framebuffer, effect_frames.snapshots[1], VGA_W * VGA_H) != 0)
        goto done;
    fd2_field_effect_frames_close(&effect_frames);
    if (fd2_field_game_play_earthquake(
            game, vga, field_audio, 0) != 0)
        goto done;

    int center_x = (game->cursor_cell_x - game->camera_cell_x) * 24 + 12;
    int center_y = (game->cursor_cell_y - game->camera_cell_y) * 24 + 16;
    if (fd2_field_game_prepare_stage_transition(
            game, vga, center_x, center_y, 10, 8, &effect_frames) != 0 ||
        memcmp(effect_frames.snapshots[0], effect_frames.snapshots[8], VGA_W * VGA_H) == 0 ||
        fd2_field_effect_play(&effect_frames, vga, NULL, 0) != 0 ||
        memcmp(vga->framebuffer, effect_frames.snapshots[8], VGA_W * VGA_H) != 0)
        goto done;
    fd2_field_effect_frames_close(&effect_frames);
    if (fd2_field_game_play_stage_transition(
            game, vga, field_audio, center_x, center_y, 10, 8, 0) != 0)
        goto done;
    result = 0;

done:
    fd2_field_effect_frames_close(&effect_frames);
    fd2_field_game_render(game, vga);
    return result;
}

static int test_resource_effect_wrappers(void) {
    fd2_archive fdother = {0};
    fd2_field_game *game = calloc(1, sizeof(*game));
    fd2_vga *vga = calloc(1, sizeof(*vga));
    CHECK(game && vga);
    CHECK(fd2_archive_open(&fdother, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_field_game_open(game, vga, &fdother, 0) == 0);
    CHECK(validate_field_effects(game, vga, NULL) == 0);
    fd2_field_game_close(game);
    fd2_archive_close(&fdother);
    free(vga);
    free(game);
    return 0;
}

/* 24618最后的11df2是加色变白；调用真实Game→Scene固定冷图，
 * 无窗口。坏基色只在9帧已提交后拒绝，不回滚最后mask帧或写Game/RNG。 */
static int test_transition_whitening_finish(void) {
    fd2_archive other={0};
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));
    fd2_vga *vga=calloc(1,sizeof(*vga));
    uint8_t *last=malloc(VGA_W*VGA_H);CHECK(game && before && vga && last);
    CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_field_game_open(game,vga,&other,0)==0);
    unsigned calls=0;game->attack_rng=return_rng;game->attack_rng_userdata=&calls;
    for(int bad=0;bad<2;bad++) {
        uint8_t palette[768],dac[768];
        for(size_t i=0;i<sizeof(palette);i++)palette[i]=(uint8_t)(i%64u);
        fd2_vga_set_palette(vga,palette);
        fd2_field_effect_frames frames={0};
        CHECK(fd2_field_game_prepare_stage_transition(game,vga,84,88,10,8,&frames)==0);
        memcpy(last,frames.snapshots[8],VGA_W*VGA_H);
        fd2_field_effect_frames_close(&frames);
        if(bad)vga->palette[767]=64;
        memcpy(dac,vga->dac,sizeof(dac));*before=*game;
        CHECK(fd2_field_game_play_stage_transition(game,vga,NULL,84,88,10,8,1)==(bad?-1:0));
        CHECK(!memcmp(game,before,sizeof(*game)) && !calls &&
            !memcmp(vga->framebuffer,last,VGA_W*VGA_H));
        if(bad)CHECK(!memcmp(vga->dac,dac,sizeof(dac)));
        else for(size_t i=0;i<sizeof(dac);i++) {
            unsigned value=palette[i]+62u;if(value>63u)value=63u;
            CHECK(vga->dac[i]==(uint8_t)((value<<2)|(value>>4)));
        }
    }
    vga->input.quit_requested=1;*before=*game;
    CHECK(fd2_field_game_play_stage_transition(game,vga,NULL,84,88,10,8,1)==1 &&
        !memcmp(game,before,sizeof(*game)) && !calls);
    fd2_field_game_close(game);fd2_archive_close(&other);
    free(last);free(vga);free(before);free(game);return 0;
}

static int effect_page_read(const fd2_field_page *page,uint8_t *pixels) {
    return fd2_field_page_read(page,0,0,FD2_FIELD_PAGE_STRIDE,
        FD2_FIELD_PAGE_HEIGHT,pixels,FD2_FIELD_PAGE_SIZE,FD2_FIELD_PAGE_STRIDE);
}

/* field_view_refresh @0x11cac：1297d先写actor/cursor，原实参0才调4e31c，
 * 随后11eee推进terrain/range，127e0才推进raw26。使用真实水色非法域，
 * 锁住错误前缀而不是只比较最终相位；不注入图形/时钟替身。 */
static int tick_in_read_span(uint32_t bits,uint16_t first,uint16_t last) {
    uint16_t low=(uint16_t)bits;
    uint32_t signed_bits=low<0x8000u?low:(uint32_t)((int32_t)low-65536);
    return bits==signed_bits && (uint16_t)(low-first)<=(uint16_t)(last-first);
}

static int test_runtime_refresh_original_clock_order(void) {
    fd2_archive other={0};
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));
    fd2_vga *vga=calloc(1,sizeof(*vga)),*vga_before=malloc(sizeof(*vga_before));
    uint8_t *page_before=malloc(FD2_FIELD_PAGE_SIZE),*page_after=malloc(FD2_FIELD_PAGE_SIZE);
    CHECK(game && before && vga && vga_before && page_before && page_after);
    CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_field_game_open_resources(game,&other,0)==0);
    CHECK(fd2_field_units_set(&game->units,0,0,0,2,3,3)==0);
    game->units.items[0].hp=31;game->units.items[0].hp_max=49;
    game->cursor_cell_x=game->cursor_cell_y=3;game->ordinary_display_mode=0;
    unsigned calls=0;game->attack_rng=return_rng;game->attack_rng_userdata=&calls;
    vga->field_page=fd2_field_page_create();CHECK(vga->field_page);
    vga->info_panel_suppressed=1;
    CHECK(fd2_field_game_bind_visual(game,vga)==0);
    memset(vga->framebuffer,0x7b,sizeof(vga->framebuffer));
    vga->field_visual=(fd2_field_visual_state){0};
    vga->field_visual.actor_idle_phase=2;vga->field_visual.cursor_phase=3;
    vga->field_visual.actor_idle_tick=0x12345678u;
    vga->field_visual.terrain_phase=1;vga->field_visual.terrain_tick=0x12345678u;
    vga->field_visual.range_phase=7;vga->field_visual.range_tick=0x12345678u;
    vga->actor_jitter_phase=1;vga->actor_jitter_tick=123;
    vga->field_water_phase=255;vga->field_water_last_tick=321;
    game->idle_phase=2;game->terrain_visual_phase=1;game->range_visual_phase=7;
    game->visual_now_ns=456;
    *before=*game;*vga_before=*vga;
    CHECK(effect_page_read(vga->field_page,page_before)==0);
    CHECK(fd2_field_game_runtime_refresh(game,vga,0)==-1);
    CHECK(vga->field_visual.actor_idle_phase==3 && vga->field_visual.cursor_phase==0 &&
        vga->field_visual.actor_idle_tick!=0x12345678u && game->idle_phase==3);
    CHECK(vga->field_visual.terrain_phase==1 && vga->field_visual.terrain_tick==0x12345678u &&
        vga->field_visual.range_phase==7 && vga->field_visual.range_tick==0x12345678u &&
        game->terrain_visual_phase==1 && game->range_visual_phase==7 && game->visual_now_ns==456);
    before->idle_phase=3;
    vga_before->field_visual.actor_idle_phase=3;vga_before->field_visual.cursor_phase=0;
    vga_before->field_visual.actor_idle_tick=vga->field_visual.actor_idle_tick;
    CHECK(!memcmp(before,game,sizeof(*game)) && !memcmp(vga_before,vga,sizeof(*vga)) && !calls);
    CHECK(effect_page_read(vga->field_page,page_after)==0 && !memcmp(page_before,page_after,FD2_FIELD_PAGE_SIZE));

    /* 后段range错误不执行尚未到达的VGA清边与Game panel投影。 */
    vga->field_water_phase=0;vga->field_visual.range_phase=20;
    game->info_panel_right=0;vga->info_panel_right=1;
    CHECK(fd2_field_game_runtime_refresh(game,vga,0)==-1);
    CHECK(game->info_panel_right==0 && vga->info_panel_right==1);
    for(size_t p=0;p<sizeof(vga->framebuffer);p++)CHECK(vga->framebuffer[p]==0x7b);

    const int modes[]={0,3};
    for(size_t i=0;i<sizeof(modes)/sizeof(modes[0]);i++) {
        vga->field_visual.actor_idle_phase=2;vga->field_visual.cursor_phase=3;
        vga->field_visual.actor_idle_tick=0x12345678u;
        vga->field_visual.terrain_phase=1;vga->field_visual.terrain_tick=0x12345678u;
        vga->field_visual.range_phase=7;vga->field_visual.range_tick=0x12345678u;
        uint16_t old_tick=(uint16_t)(fd2_bios_tick_low_ns(SDL_GetTicksNS())-10u);
        vga->actor_jitter_phase=1;vga->actor_jitter_tick=old_tick;
        vga->field_water_phase=6;vga->field_water_last_tick=old_tick;
        uint8_t water_dac[48];memcpy(water_dac,vga->dac+224u*3u,sizeof(water_dac));
        uint16_t first=fd2_bios_tick_low_ns(SDL_GetTicksNS());
        CHECK(fd2_field_game_runtime_refresh(game,vga,modes[i])==0);
        uint16_t last=fd2_bios_tick_low_ns(SDL_GetTicksNS());
        /* 真时钟可能跨tick；不得再用过期visual_now_ns强令所有读点同值。 */
        CHECK(vga->field_visual.actor_idle_phase==3 && vga->field_visual.cursor_phase==0 &&
            tick_in_read_span(vga->field_visual.actor_idle_tick,first,last) && game->idle_phase==3);
        CHECK(vga->field_visual.terrain_phase==0 && tick_in_read_span(vga->field_visual.terrain_tick,first,last) &&
            vga->field_visual.range_phase==8 && tick_in_read_span(vga->field_visual.range_tick,first,last) &&
            game->terrain_visual_phase==0 && game->range_visual_phase==8);
        CHECK(vga->actor_jitter_phase==0 &&
            (uint16_t)(vga->actor_jitter_tick-first)<=(uint16_t)(last-first) && !calls);
        if(modes[i]==0)CHECK(vga->field_water_phase==7 &&
            (uint16_t)(vga->field_water_last_tick-first)<=(uint16_t)(last-first));
        else CHECK(vga->field_water_phase==6 && vga->field_water_last_tick==old_tick &&
            !memcmp(water_dac,vga->dac+224u*3u,sizeof(water_dac)));
    }
    fd2_field_game_close(game);fd2_field_page_destroy(vga->field_page);
    fd2_archive_close(&other);free(page_before);free(page_after);free(vga_before);free(vga);free(before);free(game);
    return 0;
}

/* 真实FDICON/FDSHAP/FDOTHER经过正式Game入口；两个有界actor是资源夹具，
 * 不把合成owner检查登记为原版运行。153216B写集与完整Game/VGA快照覆盖
 * PREPARE取消/第二actor坏RLE，并区分1c2da、21548、24618的实际尾页。 */
static int test_app_page_effect_owners(void) {
    fd2_archive other={0};
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));
    fd2_vga *vga=calloc(1,sizeof(*vga)),*vga_before=malloc(sizeof(*vga_before));
    uint8_t *saved=malloc(FD2_FIELD_PAGE_SIZE),*actual=malloc(FD2_FIELD_PAGE_SIZE);
    uint8_t *expected=malloc(FD2_FIELD_PAGE_SIZE);
    CHECK(game && before && vga && vga_before && saved && actual && expected);
    CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_field_game_open_resources(game,&other,0)==0);
    CHECK(fd2_field_units_set(&game->units,0,0,0,2,3,3)==0);
    CHECK(fd2_field_units_set(&game->units,1,1,1,0,5,3)==0);
    game->units.items[0].hp=31;game->units.items[0].hp_max=49;
    game->units.items[1].hp=17;game->units.items[1].hp_max=38;
    game->camera_cell_x=game->camera_cell_y=0;
    game->cursor_cell_x=3;game->cursor_cell_y=3;
    game->ordinary_display_mode=0;
    unsigned calls=0;game->attack_rng=return_rng;game->attack_rng_userdata=&calls;
    vga->field_page=fd2_field_page_create();CHECK(vga->field_page);
    vga->info_panel_suppressed=1;
    CHECK(fd2_field_game_bind_visual(game,vga)==0);
    /* 11cbc原JNZ：只有arg0调用4e31c。宿主已过水色门也不能把
     * 10010的arg1（mode3）或纯11eee/127a9重画误作推进。 */
    uint16_t water_before=(uint16_t)(SDL_GetTicksNS()*1193182u/UINT64_C(65536000000000)-10u);
    vga->field_water_last_tick=water_before;vga->field_water_phase=6;
    uint8_t water_dac[48];memcpy(water_dac,vga->dac+224u*3u,sizeof(water_dac));
    const int no_water_modes[]={3,1,2};
    for(size_t i=0;i<3;i++) {
        CHECK(fd2_field_game_runtime_refresh(game,vga,no_water_modes[i])==0);
        CHECK(vga->field_water_last_tick==water_before && vga->field_water_phase==6);
        CHECK(!memcmp(water_dac,vga->dac+224u*3u,sizeof(water_dac)) && !calls);
    }
    CHECK(fd2_field_game_runtime_refresh(game,vga,0)==0);
    CHECK(vga->field_water_last_tick!=water_before && vga->field_water_phase==7 && !calls);
    memset(saved,85,FD2_FIELD_PAGE_SIZE);
    CHECK(fd2_field_page_write(vga->field_page,0,0,FD2_FIELD_PAGE_STRIDE,
        FD2_FIELD_PAGE_HEIGHT,saved,FD2_FIELD_PAGE_SIZE,FD2_FIELD_PAGE_STRIDE)==0);
    CHECK(fd2_field_game_effect_refresh(game,vga)==0);
    /* 1c2da固定idle3→2，真实页的opaque/SKIP由原RLE决定。 */
    game->idle_phase=3;vga->field_visual.actor_idle_phase=3;
    const uint8_t pair[]={0,1};
    fd2_field_effect_frames frames={0},empty={0};
    *before=*game;*vga_before=*vga;
    CHECK(effect_page_read(vga->field_page,saved)==0);
    CHECK(fd2_field_game_prepare_actor_group_flash(game,vga,pair,2,94,&frames)==0);
    CHECK(frames.page_snapshots[1] && !frames.page_snapshots[0]);
    CHECK(effect_page_read(frames.page_snapshots[1],expected)==0);
    CHECK(memcmp(saved,expected,FD2_FIELD_PAGE_SIZE)!=0);
    CHECK(!memcmp(game,before,sizeof(*game)) && !memcmp(vga,vga_before,sizeof(*vga)));
    CHECK(effect_page_read(vga->field_page,actual)==0 && !memcmp(saved,actual,FD2_FIELD_PAGE_SIZE));
    CHECK(fd2_field_effect_play(&frames,vga,NULL,0)==0);
    CHECK(effect_page_read(vga->field_page,actual)==0 && !memcmp(expected,actual,FD2_FIELD_PAGE_SIZE));
    CHECK(!memcmp(vga->framebuffer,frames.snapshots[0],VGA_W*VGA_H));
    CHECK(!memcmp(game,before,sizeof(*game)) && !calls);
    fd2_field_effect_frames_close(&frames);
    CHECK(fd2_field_page_write(vga->field_page,0,0,FD2_FIELD_PAGE_STRIDE,
        FD2_FIELD_PAGE_HEIGHT,saved,FD2_FIELD_PAGE_SIZE,FD2_FIELD_PAGE_STRIDE)==0);
    memcpy(vga->framebuffer,vga_before->framebuffer,sizeof(vga->framebuffer));
    CHECK(fd2_field_game_play_actor_group_flash(game,vga,NULL,pair,2,255,0)==0);
    CHECK(effect_page_read(vga->field_page,actual)==0 && !memcmp(expected,actual,FD2_FIELD_PAGE_SIZE));
    CHECK(!memcmp(vga->framebuffer,vga_before->framebuffer,sizeof(vga->framebuffer)));
    CHECK(!memcmp(game,before,sizeof(*game)) && !calls);

    /* 21548 PREPARE读原App，实际begin只写原page0；播放0/1/2/1不把
     * 临时page1/page2换成App owner，调用尾才11cac且没有HP/RNG写入。 */
    *before=*game;*vga_before=*vga;
    CHECK(effect_page_read(vga->field_page,saved)==0);
    CHECK(fd2_field_game_prepare_earthquake(game,vga,&frames)==0);
    CHECK(frames.page_snapshots[0] && frames.page_snapshots[1] && frames.page_snapshots[2]);
    CHECK(effect_page_read(frames.page_snapshots[0],expected)==0);
    CHECK(!memcmp(game,before,sizeof(*game)) && !memcmp(vga,vga_before,sizeof(*vga)));
    CHECK(effect_page_read(vga->field_page,actual)==0 && !memcmp(saved,actual,FD2_FIELD_PAGE_SIZE));
    fd2_field_effect_frames_close(&frames);
    CHECK(fd2_field_game_begin_earthquake(game,vga,&frames)==0);
    CHECK(effect_page_read(vga->field_page,actual)==0 && !memcmp(expected,actual,FD2_FIELD_PAGE_SIZE));
    CHECK(!memcmp(game,before,sizeof(*game)) && !calls);
    CHECK(vga->field_page==vga_before->field_page &&
        !memcmp(&vga->field_visual,&vga_before->field_visual,sizeof(vga->field_visual)));
    CHECK(fd2_field_effect_play(&frames,vga,NULL,0)==0);
    CHECK(effect_page_read(vga->field_page,actual)==0 && !memcmp(expected,actual,FD2_FIELD_PAGE_SIZE));
    CHECK(!memcmp(vga->framebuffer,frames.snapshots[1],VGA_W*VGA_H));
    fd2_field_effect_frames_close(&frames);
    CHECK(fd2_field_game_play_earthquake(game,vga,NULL,0)==0);
    CHECK(!memcmp(&game->units,&before->units,sizeof(game->units)) && !calls);
    CHECK(vga->field_page==vga_before->field_page && game->visual_owner==&vga->field_visual);
    fd2_vga *projection=calloc(1,sizeof(*projection));CHECK(projection);
    projection->field_page=fd2_field_page_clone(vga->field_page);CHECK(projection->field_page);
    projection->field_visual=vga->field_visual;projection->info_panel_suppressed=1;
    projection->actor_jitter_phase=vga->actor_jitter_phase;
    fd2_field_game_render_effect_background(game,projection);
    CHECK(!memcmp(vga->framebuffer,projection->framebuffer,sizeof(vga->framebuffer)));
    CHECK(effect_page_read(projection->field_page,expected)==0);
    CHECK(effect_page_read(vga->field_page,actual)==0 && !memcmp(expected,actual,FD2_FIELD_PAGE_SIZE));
    fd2_field_page_destroy(projection->field_page);free(projection);

    /* 24618 PREPARE全程只写临时页；实际最后仍是LUT1 masked页，不能
     * 因其它恢复法术21eb1有尾11cac而统一恢复本入口。 */
    *before=*game;*vga_before=*vga;
    CHECK(effect_page_read(vga->field_page,saved)==0);
    CHECK(fd2_field_game_prepare_stage_transition(game,vga,84,88,10,8,&frames)==0);
    CHECK(frames.snapshot_count==9 && frames.page_snapshots[8]);
    CHECK(!memcmp(game,before,sizeof(*game)) && !memcmp(vga,vga_before,sizeof(*vga)));
    CHECK(effect_page_read(vga->field_page,actual)==0 && !memcmp(saved,actual,FD2_FIELD_PAGE_SIZE));
    fd2_field_effect_frames_close(&frames);
    CHECK(fd2_field_game_play_stage_transition(game,vga,NULL,84,88,10,8,0)==0);
    CHECK(!memcmp(&game->units,&before->units,sizeof(game->units)) && !calls);
    fd2_field_page *base=fd2_field_page_create();CHECK(base);
    game->failure=FD2_FIELD_FAILURE_ACTION;*before=*game;*vga_before=*vga;
    CHECK(fd2_field_game_effect_terrain_begin(game,vga,base)==-1 &&
        !memcmp(game,before,sizeof(*game)) && !memcmp(vga,vga_before,sizeof(*vga)));
    game->failure=FD2_FIELD_FAILURE_NONE;
    projection=calloc(1,sizeof(*projection));CHECK(projection);
    projection->field_page=fd2_field_page_create();CHECK(projection->field_page);
    projection->field_visual=vga->field_visual;
    projection->actor_jitter_tick=vga->actor_jitter_tick;
    projection->actor_jitter_phase=vga->actor_jitter_phase;
    CHECK(fd2_field_game_effect_terrain_page(game,vga,base)==0);
    CHECK(fd2_field_game_effect_mask_frame(game,projection,base,
        fd2_field_lut_frame(&game->visuals,1),84,88,74,0,0)==0);
    CHECK(effect_page_read(projection->field_page,expected)==0);
    CHECK(effect_page_read(vga->field_page,actual)==0 && !memcmp(expected,actual,FD2_FIELD_PAGE_SIZE));
    CHECK(!memcmp(vga->framebuffer,projection->framebuffer,sizeof(vga->framebuffer)));
    CHECK(memcmp(saved,actual,FD2_FIELD_PAGE_SIZE)!=0);
    fd2_field_page_destroy(base);fd2_field_page_destroy(projection->field_page);free(projection);

    /* 第二actor的目录索引合法但RLE截断：已完成第一actor的临时写入
     * 不能漏到App/VGA/Game；三种PREPARE都释放整个候选。 */
    *before=*game;*vga_before=*vga;
    CHECK(effect_page_read(vga->field_page,saved)==0);
    size_t frame=fd2_field_unit_cached_sprite_id(&game->units,1)*12u;
    CHECK(frame+2<game->sprites.frame_count && game->sprites.size<=UINT32_MAX);
    /* 1c2da固定idle3→2，127e0普通静止idle3→1。破坏此actor三帧
     * 各自的合法目录项，不能把不同原语的选帧规则强行等同。 */
    uint32_t offsets[3];
    memcpy(offsets,game->sprites.offsets+frame,sizeof(offsets));
    for(size_t i=0;i<3;i++)game->sprites.offsets[frame+i]=(uint32_t)game->sprites.size;
    CHECK(fd2_field_game_prepare_actor_group_flash(game,vga,pair,2,0,&frames)==-1);
    CHECK(!memcmp(&frames,&empty,sizeof(frames)));
    CHECK(fd2_field_game_prepare_earthquake(game,vga,&frames)==-1);
    CHECK(!memcmp(&frames,&empty,sizeof(frames)));
    CHECK(fd2_field_game_prepare_stage_transition(game,vga,84,88,10,8,&frames)==-1);
    CHECK(!memcmp(&frames,&empty,sizeof(frames)));
    memcpy(game->sprites.offsets+frame,offsets,sizeof(offsets));
    CHECK(!memcmp(game,before,sizeof(*game)) && !memcmp(vga,vga_before,sizeof(*vga)) && !calls);
    CHECK(effect_page_read(vga->field_page,actual)==0 && !memcmp(saved,actual,FD2_FIELD_PAGE_SIZE));
    CHECK(fd2_field_game_prepare_stage_transition(game,vga,84,88,0,8,&frames)==-1);
    CHECK(!memcmp(&frames,&empty,sizeof(frames)) && !memcmp(game,before,sizeof(*game)));
    CHECK(!memcmp(vga,vga_before,sizeof(*vga)));

    /* 在正式入口之前已收到hostquit，任何候选/视觉/业务均不得启动。 */
    vga->input.quit_requested=1;*vga_before=*vga;
    CHECK(fd2_field_game_play_actor_group_flash(game,vga,NULL,pair,2,0,0)==1);
    CHECK(fd2_field_game_play_earthquake(game,vga,NULL,0)==1);
    CHECK(fd2_field_game_play_stage_transition(game,vga,NULL,84,88,10,8,0)==1);
    CHECK(!memcmp(game,before,sizeof(*game)) && !memcmp(vga,vga_before,sizeof(*vga)) && !calls);
    CHECK(effect_page_read(vga->field_page,actual)==0 && !memcmp(saved,actual,FD2_FIELD_PAGE_SIZE));
    fd2_field_page_destroy(vga->field_page);
    fd2_field_game_close(game);fd2_archive_close(&other);
    free(expected);free(actual);free(saved);free(vga_before);free(vga);free(before);free(game);
    return 0;
}

static int test_original_constructor_domain(void) {
    /* EXE object3的LE文件连续映射已由strictLE独立验证器校验；这里直接读原窗，
     * 不以生产JSON/getter的值计算预期。0x619fd->file0x7aa11同一映射。 */
    FILE *exe=fopen("original_game/FD2.EXE","rb"); CHECK(exe);
    uint8_t growth[68][11],base[32][24],enemy[68][10],alias48[24],alias66[24],items[215][23];
    #define RAW_READ(va, out) do { CHECK(!fseek(exe,(long)((va)+0x19014u),SEEK_SET)); \
        CHECK(fread((out),1,sizeof(out),exe)==sizeof(out)); } while(0)
    RAW_READ(0x620a1u,growth); RAW_READ(0x61da1u,base); RAW_READ(0x61af9u,enemy);
    RAW_READ(0x62221u,alias48); RAW_READ(0x623d1u,alias66);
    RAW_READ(0x602adu,items);
    #undef RAW_READ
    CHECK(!fclose(exe));
    fd2_archive ar={0};CHECK(fd2_archive_open(&ar,"original_game/FDFIELD.DAT")==0);
    size_t checked=0;
    for (size_t stage=0;stage<33;stage++) {
        const uint8_t *raw;size_t raw_size;
        CHECK(fd2_archive_get(&ar,stage*3+1,&raw,&raw_size)==0 && raw_size>=131);
        fd2_field_metadata metadata={0};fd2_field_placements placements={0};
        CHECK(fd2_field_metadata_open_stage(&metadata,&ar,stage)==0);
        CHECK(fd2_field_placements_open_stage(&placements,&ar,stage)==0);
        size_t count=SIZE_MAX;CHECK(fd2_field_unit_template_count(&metadata,&count)==0);
        CHECK(count==raw[2] && count<=metadata.unit_template_count);
        for (size_t i=0;i<count;i++) {
            const uint8_t *tpl=raw+131+i*26;
            uint8_t id=tpl[1],level=tpl[4];CHECK(level!=0);
            const uint8_t *b=id<32 ? base[id] : id==48 ? alias48 : id==66 ? alias66 : NULL;
            unsigned hp,mp,ap,dp,dx;
            uint8_t race,profile,move;
            if (id<68) {
                CHECK(b);
                hp=b[3]+256u*b[4]+growth[id][6]*(unsigned)(level-1);
                mp=b[5]+256u*b[6]+growth[id][8]*(unsigned)(level-1);
                ap=b[18]+256u*b[19]+growth[id][0]*(unsigned)level;
                dp=b[20]+256u*b[21]+growth[id][2]*(unsigned)level;
                dx=b[22]+256u*b[23]+growth[id][4]*(unsigned)level;
                race=b[0];profile=b[1];move=b[7];
            } else {
                CHECK(id<136);b=enemy[id-68];
                hp=(b[2]+256u*b[3])*level;mp=b[4]*level;ap=b[5]*level;
                dp=b[6]*level;dx=b[7]*level;race=b[0];profile=b[1];move=b[8];
            }
            fd2_field_units units={0};
            CHECK(fd2_field_units_append_template(&units,&metadata,&placements,i)==0 && units.count==1);
            const fd2_field_unit *u=&units.items[0];const uint8_t *r=(const uint8_t *)u;
            CHECK(u->unit_id==id && u->text_id==id && u->side==tpl[0]);
            CHECK(u->x==(uint8_t)placements.records[i].x && u->y==(uint8_t)placements.records[i].y);
            CHECK(u->level==level && u->race==race && u->movement_profile==profile && u->movement_points==move);
            CHECK(u->hp==(uint16_t)hp && u->hp_max==(uint16_t)hp && u->mp==(uint16_t)mp && u->mp_max==(uint16_t)mp);
            CHECK((unsigned)(r[0x37]+256u*r[0x38])==(uint16_t)ap);
            CHECK((unsigned)(r[0x39]+256u*r[0x3a])==(uint16_t)dp);
            CHECK((unsigned)(r[0x3e]+256u*r[0x3f])==(uint16_t)dx);
            int32_t stats[4]={(int32_t)ap,(int32_t)dx,(int32_t)dp,(int32_t)dx};
            /* 原模板只将前1/2个槽置40，未装备槽不参与。独立原item+1/3/5/7。 */
            uint8_t equipment[2]={tpl[5]==255 ? tpl[6] : tpl[5],tpl[6]};
            size_t equipped=tpl[5]==255 ? 1u : 2u;
            for (size_t slot=0;slot<equipped;slot++) {
                CHECK(equipment[slot]<215);
                for (size_t stat=0;stat<4;stat++) {
                    const uint8_t *item=items[equipment[slot]]+1+stat*2;
                    unsigned bits=item[0]+256u*item[1];
                    stats[stat]+=bits<32768u ? (int32_t)bits : (int32_t)bits-65536;
                }
            }
            CHECK(u->attack==(uint16_t)stats[0] && u->accuracy==(uint16_t)stats[1]);
            CHECK(u->defense==(uint16_t)stats[2] && u->evasion==(uint16_t)stats[3]);
            CHECK(!memcmp(r+0x1a,tpl+0x0d,4) && !memcmp(r+0x34,tpl+0x11,3));
            CHECK(r[0x31]==tpl[0x16] && r[0x32]==tpl[0x17] && r[0x33]==tpl[0x18]);
            CHECK(units.source_template_indices[0]==i);
            checked++;
        }
        if (stage==0 || stage==31 || stage==32) {
            CHECK(count==30 && metadata.unit_template_count>count);
            fd2_field_units units={0};
            CHECK(fd2_field_units_append_template(&units,&metadata,&placements,count)==-1 && !units.count);
            uint8_t group=stage==32 ? 255 : 10;size_t expected=0;
            for (size_t i=0;i<count;i++) if (metadata.unit_templates[i].bytes[0x15]==group) expected++;
            CHECK(fd2_field_units_append_group(&units,&metadata,&placements,group)==0 && units.count==expected);
        }
        fd2_field_placements_close(&placements);fd2_field_metadata_close(&metadata);
    }
    fd2_archive_close(&ar);CHECK(checked==1887);
    printf("original constructor fields: %zu effective templates / 33 stages\n",checked);
    return 0;
}

static int test_unit_domain_errors_and_mutations(void) {
    fd2_field_unit_template templates[2]={{{0}},{{0}}};
    templates[0].bytes[1]=0;templates[0].bytes[4]=1;templates[0].bytes[0x15]=3;
    templates[1]=templates[0];templates[1].bytes[1]=255;
    fd2_field_metadata meta={0};meta.unit_templates=templates;meta.unit_template_count=2;meta.unknown_02=2;meta.unknown_01=1;
    fd2_field_placement positions[3]={{0x102,0x203,255},{4,5,0},{12,13,254}};
    fd2_field_placements placements={3,positions};fd2_field_units units={0};
    CHECK(fd2_field_units_append_template(&units,&meta,&placements,0)==0);
    CHECK(units.items[0].x==2 && units.items[0].y==3); /* 同index/低byte，忽略第三WORD。 */
    fd2_field_units saved=units;
    CHECK(fd2_field_units_append_group(&units,&meta,&placements,3)==-1 && !memcmp(&saved,&units,sizeof(units)));
    fd2_field_units identity=saved;
    CHECK(fd2_field_units_project_group_identity(&identity,&meta,&placements,3)==0 && identity.count==3);
    CHECK(identity.items[2].unit_id==255 && identity.items[2].text_id==255);
    CHECK(identity.items[2].hp_max==0 && identity.sprite_cache_count==saved.sprite_cache_count);
    CHECK(!memcmp(&saved,&units,sizeof(units))); /* 资源身份投影不代替真实handler。 */
    uint8_t x=77,y=88;
    CHECK(fd2_field_unit_deployment_position(&meta,&placements,0,&x,&y)==0 && x==12 && y==13);
    CHECK(fd2_field_unit_deployment_position(&meta,&placements,1,&x,&y)==-1 && x==12 && y==13);
    meta.unknown_02=3;size_t count=77;
    CHECK(fd2_field_unit_template_count(&meta,&count)==-1 && count==77);
    CHECK(fd2_field_units_append_group(&units,&meta,&placements,3)==-1 && !memcmp(&saved,&units,sizeof(units)));
    memset(&units,0,sizeof(units));
    for (size_t i=0;i<96;i++) CHECK(fd2_field_units_set(&units,i,(uint8_t)(i%3),0,0,1,1)==0);
    CHECK(units.count==96);saved=units;
    CHECK(fd2_field_units_set(&units,96,1,0,0,1,1)==-1 && !memcmp(&saved,&units,sizeof(units)));
    units.items[65].ai_behavior_raw=0xa5;
    CHECK(fd2_field_units_ai_behavior_write(&units,65,95,3)==0 && units.items[65].ai_behavior_raw==0xa3);
    CHECK(fd2_field_units_field_write(&units,65,95,FD2_UNIT_AI_RAW,FD2_UNIT_AND,0x80)==0 && units.items[65].ai_behavior_raw==0x80);
    CHECK(fd2_field_units_field_write(&units,95,95,FD2_UNIT_HP,FD2_UNIT_SET,0x1234)==0 && units.items[95].hp==0x1234);
    CHECK(fd2_field_units_field_write(&units,95,95,FD2_UNIT_STATUS_26,FD2_UNIT_SET,100)==0 && units.items[95].detail_status[1]==100);
    saved=units;
    CHECK(fd2_field_units_field_write(&units,95,96,FD2_UNIT_FLAGS,FD2_UNIT_SET,1)==-1);
    CHECK(fd2_field_units_field_write(&units,95,95,(fd2_field_unit_field)99,FD2_UNIT_SET,1)==-1);
    CHECK(fd2_field_units_field_write(&units,95,95,FD2_UNIT_FLAGS,FD2_UNIT_SET,256)==-1);
    CHECK(!memcmp(&saved,&units,sizeof(units)));
    units.count=2;memset(&units.items[95],0xa5,sizeof(units.items[95]));saved=units;
    CHECK(fd2_field_units_field_write(&units,95,95,FD2_UNIT_X,FD2_UNIT_SET,22)==0);
    CHECK(fd2_field_units_field_write(&units,95,95,FD2_UNIT_Y,FD2_UNIT_SET,25)==0);
    CHECK(fd2_field_units_field_write(&units,95,95,FD2_UNIT_DIRECTION,FD2_UNIT_SET,2)==0);
    saved.items[95].x=22;saved.items[95].y=25;saved.items[95].direction=2;
    CHECK(units.count==2 && !memcmp(&saved,&units,sizeof(units)));
    CHECK(fd2_field_units_field_write(&units,95,96,FD2_UNIT_X,FD2_UNIT_SET,1)==-1);
    CHECK(fd2_field_units_field_write(&units,95,95,FD2_UNIT_X,FD2_UNIT_AND,1)==-1);
    CHECK(fd2_field_units_field_write(&units,95,95,FD2_UNIT_ID,FD2_UNIT_SET,1)==-1);
    CHECK(fd2_field_units_field_write(&units,95,95,FD2_UNIT_HP,FD2_UNIT_SET,1)==-1);
    CHECK(fd2_field_units_field_write(&units,95,95,FD2_UNIT_FRAME_PHASE,FD2_UNIT_SET,1)==-1);
    CHECK(fd2_field_units_ai_behavior_write(&units,95,95,1)==-1);
    CHECK(!memcmp(&saved,&units,sizeof(units)));
    return 0;
}

static int test_original_cache_raw_direction(void) {
    fd2_field_units units={0};
    CHECK(fd2_field_units_set(&units,0,7,7,0,2,2)==0);
    CHECK(fd2_field_units_set(&units,1,1,1,0,3,3)==0);
    CHECK(fd2_field_units_set(&units,2,9,9,0,4,4)==0);
    CHECK(fd2_field_units_set(&units,3,1,1,0,5,5)==0);
    CHECK(units.sprite_cache_count==3 && units.items[3].sprite_cache_class==1);
    CHECK(fd2_field_units_field_write(&units,1,1,FD2_UNIT_ID,FD2_UNIT_SET,126)==0);
    CHECK(fd2_field_unit_cached_frame_index(&units,1,0)==12); /* 原只改ID，cache仍旧unit1。 */
    CHECK(units.items[1].unit_id==126 && units.items[1].sprite_cache_class==1);
    units.items[1].unit_id=1;
    units.items[1].direction=5;
    CHECK(fd2_field_unit_cached_frame_index(&units,1,0)==9u*12u+3u);
    uint8_t *pixels=calloc(120u,24u*24u);CHECK(pixels);
    memset(pixels+(9u*12u+3u)*576u,77,576u);
    fd2_map_sprite_bank bank={0};bank.decoded_frames=pixels;bank.frame_count=120;
    fd2_vga vga={0};
    fd2_field_units_render_actor(&vga,&bank,&units,1,0,0,0,-12);
    CHECK(vga.framebuffer[54u*VGA_STRIDE+72u]==77 && vga.framebuffer[53u*VGA_STRIDE+72u]==0);
    CHECK(units.items[1].x==3 && units.items[1].y==3 && units.items[1].direction==5);
    free(pixels);
    fd2_field_unit records[96];memcpy(records,units.items,sizeof(records));
    memset(units.sprite_cache_ids,0,sizeof(units.sprite_cache_ids));
    CHECK(fd2_field_units_sprite_cache_restore(&units)==0 && units.sprite_cache_exact);
    CHECK(!memcmp(records,units.items,sizeof(records)) && fd2_field_unit_cached_frame_index(&units,1,3)==9u*12u+4u);
    units.items[2].direction=5;
    CHECK(fd2_field_unit_cached_frame_index(&units,2,0)==SIZE_MAX); /* 下一个class不存在。 */
    units.items[2].direction=0;
    for (size_t i=0;i<units.count;i++) units.items[i].sprite_cache_class=0;
    memcpy(records,units.items,sizeof(records));
    CHECK(fd2_field_units_sprite_cache_restore(&units)==0 && units.sprite_cache_exact);
    CHECK(!memcmp(records,units.items,sizeof(records)) && fd2_field_unit_cached_frame_index(&units,1,0)==9u*12u+3u);
    /* 原Load当前ID重建：热改身份前仍旧sprite，Load后变成新ID；raw80B不改。 */
    units.items[1].direction=0;units.items[1].unit_id=126;
    CHECK(fd2_field_unit_cached_frame_index(&units,1,0)==12);
    memcpy(records,units.items,sizeof(records));
    CHECK(fd2_field_units_sprite_cache_restore(&units)==0);
    CHECK(!memcmp(records,units.items,sizeof(records)) && fd2_field_unit_cached_frame_index(&units,1,0)==126u*12u);
    units.items[1].unit_id=1;CHECK(fd2_field_units_sprite_cache_restore(&units)==0);
    units.items[1].direction=0;
    CHECK(fd2_field_unit_cached_frame_index(&units,1,0)==12);
    units.items[1].direction=255;
    CHECK(fd2_field_unit_cached_frame_index(&units,1,0)==SIZE_MAX);
    return 0;
}

static int test_raw26_shared_phase_projection(void) {
    init_frames();fd2_map_sprite_bank bank={0};bank.decoded_frames=frames;bank.frame_count=24;
    fd2_field_unit unit={0};unit.x=unit.y=2;unit.direction=1;
    unit.frame_phase=2;unit.detail_status[1]=100;
    fd2_field_unit saved=unit;fd2_vga vga={0};
    CHECK(fd2_field_unit_frame_index(&unit,2,1)==3); /* raw26强制dir1/frame0。 */
    fd2_field_unit_render(&vga,&bank,&unit,2,0,0,1);
    CHECK(vga.framebuffer[42u*VGA_STRIDE+40u]==4);
    memset(vga.framebuffer,0,sizeof(vga.framebuffer));vga.actor_jitter_phase=1;
    fd2_field_unit_render(&vga,&bank,&unit,2,0,0,1);
    CHECK(vga.framebuffer[42u*VGA_STRIDE+41u]==4 && !vga.framebuffer[42u*VGA_STRIDE+40u]);
    unit.flags=FD2_FIELD_UNIT_FLAG_ACTED;saved.flags=unit.flags;
    memset(vga.framebuffer,0,sizeof(vga.framebuffer));
    fd2_field_unit_render(&vga,&bank,&unit,2,0,0,1);
    CHECK(vga.framebuffer[42u*VGA_STRIDE+41u]==28);
    CHECK(!memcmp(&unit,&saved,sizeof(unit)));return 0;
}

/* 17898/179d5的独立组合：真实资源叶按源顺序造工作页，核菜单无122dc、
 * 图标与focus重画确实回同一page。phase借最终owner；多次读钟由live专测核。 */
static int test_runtime_menu_page_composition(void) {
    fd2_archive other={0};
    fd2_field_game *game=calloc(1,sizeof(*game));
    fd2_vga *vga=calloc(1,sizeof(*vga)),*expected=calloc(1,sizeof(*expected));
    uint8_t *actual_pixels=malloc(FD2_FIELD_PAGE_SIZE),*expected_pixels=malloc(FD2_FIELD_PAGE_SIZE);
    CHECK(game && vga && expected && actual_pixels && expected_pixels);
    CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_field_game_open_resources(game,&other,0)==0);
    CHECK(fd2_field_units_set(&game->units,0,0,0,2,3,3)==0);
    game->units.items[0].hp=game->units.items[0].hp_max=31;
    game->active_side=2;game->selected_unit=0;game->cursor_cell_x=game->cursor_cell_y=3;
    game->command_selected=2;game->command_animation_phase=0;game->command_animation_opening=0;
    game->ordinary_display_mode=1;game->option_values[3]=0;
    unsigned rng_calls=0;game->attack_rng=return_rng;game->attack_rng_userdata=&rng_calls;
    vga->field_page=fd2_field_page_create();CHECK(vga->field_page);
    CHECK(fd2_field_game_bind_visual(game,vga)==0);
    fd2_field_page *oracle=fd2_field_page_create();CHECK(oracle);
    const uint8_t ids[3][4]={{0,1,2,3},{8,9,10,11},{7,5,6,4}};
    for(unsigned kind=0;kind<4;kind++) {
        memset(expected_pixels,0x5d,FD2_FIELD_PAGE_SIZE);
        CHECK(fd2_field_page_write(vga->field_page,0,0,456,336,expected_pixels,FD2_FIELD_PAGE_SIZE,456)==0);
        CHECK(fd2_field_page_copy_pixels(oracle,vga->field_page)==0);
        memset(vga->framebuffer,0,sizeof(vga->framebuffer));
        memset(expected->framebuffer,0,sizeof(expected->framebuffer));
        vga->field_water_phase=6;vga->field_water_last_tick=321;
        uint8_t saved_dac[768];memcpy(saved_dac,vga->dac,sizeof(saved_dac));
        game->interaction=kind==0?FD2_FIELD_INTERACTION_COMMAND:kind==1?
            FD2_FIELD_INTERACTION_ITEM_MENU:kind==2?FD2_FIELD_INTERACTION_SYSTEM_MENU:FD2_FIELD_INTERACTION_BROWSE;
        game->command_highlight_phase=1;game->system_menu.highlight_phase=1;
        game->system_menu.selected=2;
        if(kind==3)CHECK(fd2_field_game_runtime_overview_refresh(game,vga,0)==0);
        else CHECK(fd2_field_game_runtime_menu_refresh(game,vga)==0);
        CHECK(fd2_terrain_render_page_block(oracle,&game->terrain,&game->map,72,72,13,8,0,0,
            game->terrain_visual_phase,game->idle_phase,NULL,NULL)==0);
        CHECK(fd2_field_units_render_page(oracle,&game->sprites,&game->units,0,0,game->idle_phase,
            (uint8_t)vga->field_visual.cursor_phase,vga->actor_jitter_phase)==0);
        CHECK(fd2_field_units_render_page_terrain_overlays(oracle,&game->terrain,&game->map,&game->units,
            0,0,game->terrain_visual_phase,NULL,NULL)==0);
        CHECK(fd2_field_page_project(oracle,72,72,expected->framebuffer,sizeof(expected->framebuffer))==0);
        if(kind<3) {
            uint8_t disabled[4]={0};
            fd2_field_command_draw_id_animation(expected,&game->command_assets,76,76,ids[kind],2,disabled,1,0,0);
            CHECK(fd2_field_page_write(oracle,72,72,312,192,expected->framebuffer+1284,
                sizeof(expected->framebuffer)-1284,320)==0);
            CHECK(fd2_field_units_render_page_actor(oracle,&game->sprites,&game->units,0,0,0,game->idle_phase,
                (uint8_t)vga->field_visual.cursor_phase,vga->actor_jitter_phase,0)==0);
        }
        CHECK(effect_page_read(vga->field_page,actual_pixels)==0 && effect_page_read(oracle,expected_pixels)==0);
        CHECK(!memcmp(actual_pixels,expected_pixels,FD2_FIELD_PAGE_SIZE));
        CHECK(fd2_field_page_project(oracle,72,72,expected->framebuffer,sizeof(expected->framebuffer))==0 &&
            !memcmp(vga->framebuffer,expected->framebuffer,sizeof(vga->framebuffer)));
        CHECK(vga->field_water_phase==6 && vga->field_water_last_tick==321 &&
            !memcmp(saved_dac,vga->dac,sizeof(saved_dac)) && !rng_calls);
    }
    fd2_field_page_destroy(oracle);fd2_field_page_destroy(vga->field_page);
    fd2_field_game_close(game);fd2_archive_close(&other);
    free(actual_pixels);free(expected_pixels);free(expected);free(vga);free(game);
    return 0;
}

/* 18b84 body的独立叶组合：真实资源，完整153216B页/64000B投影。
 * 只借最终phase核绘序；逐读取钟另测，不签未接的首次ESI与后续比较门。 */
static int summary_body_oracle(fd2_field_game *game,fd2_vga *vga,fd2_field_page *page,
        fd2_vga *expected,int left,int failed_summary) {
    CHECK(fd2_field_background_draw_from(vga->field_page,page,72,72,0,0,0,0,0,0)==0);
    CHECK(fd2_terrain_render_page_block(page,&game->terrain,&game->map,72,72,13,8,0,0,
        game->terrain_visual_phase,game->idle_phase,NULL,NULL)==0);
    CHECK(fd2_field_page_project(page,72,72,expected->framebuffer,sizeof(expected->framebuffer))==0);
    /* 本组只用原mode0/1/7；7是122dc真实default，不强制普通cursor。 */
    if(game->ordinary_display_mode==1) {
        const fd2_image *cursor=fd2_field_cursor_frame(&game->visuals,0);CHECK(cursor);
        fd2_map_sprite_blit(expected,cursor,game->cursor_cell_x*24+4,game->cursor_cell_y*24+4,0);
    }
    CHECK(fd2_field_page_write(page,72,72,312,192,expected->framebuffer+1284,
        sizeof(expected->framebuffer)-1284,320)==0);
    CHECK(fd2_field_units_render_page(page,&game->sprites,&game->units,0,0,game->idle_phase,
        (uint8_t)vga->field_visual.cursor_phase,vga->actor_jitter_phase)==0);
    CHECK(fd2_field_units_render_page_terrain_overlays(page,&game->terrain,&game->map,&game->units,
        0,0,game->terrain_visual_phase,NULL,NULL)==0);
    if(!failed_summary) {
        CHECK(fd2_field_page_project(page,72,72,expected->framebuffer,sizeof(expected->framebuffer))==0);
        int x=left?9:160;
        CHECK(fd2_field_unit_summary_draw_at(expected,&game->info_assets,&game->font,&game->ui_text,
            &game->units.items[0],x,9)==0);
        CHECK(fd2_field_page_write(page,x+68,77,149,42,expected->framebuffer+9u*320u+(size_t)x,
            sizeof(expected->framebuffer)-(9u*320u+(size_t)x),320)==0);
        CHECK(fd2_field_game_render_page_info(game,expected,page,&game->units,0,0,
            game->cursor_cell_x,game->cursor_cell_y,game->idle_phase)==0);
    }
    return 0;
}

static int test_runtime_summary_body(void) {
    fd2_archive other={0};
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));
    fd2_vga *vga=calloc(1,sizeof(*vga)),*expected=calloc(1,sizeof(*expected)),*old_vga=malloc(sizeof(*old_vga));
    uint8_t *actual_pixels=malloc(FD2_FIELD_PAGE_SIZE),*expected_pixels=malloc(FD2_FIELD_PAGE_SIZE);
    CHECK(game && before && vga && expected && old_vga && actual_pixels && expected_pixels);
    CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_field_game_open_resources(game,&other,0)==0);
    CHECK(fd2_field_units_set(&game->units,0,0,0,2,6,6)==0 &&
        fd2_field_units_set(&game->units,1,1,1,2,1,0)==0 &&
        fd2_field_units_set(&game->units,2,2,2,2,8,0)==0);
    for(size_t raw=0;raw<3;raw++) {
        game->units.items[raw].hp=31;game->units.items[raw].hp_max=49;
        game->units.items[raw].mp=11;game->units.items[raw].mp_max=19;
    }
    game->active_side=2;game->selected_unit=0;game->interaction=FD2_FIELD_INTERACTION_UNIT_SELECTED;
    game->option_values[3]=1;game->system_result_fragment=7;game->system_result_until_ms=1;
    unsigned rng_calls=0;game->attack_rng=return_rng;game->attack_rng_userdata=&rng_calls;
    vga->field_page=fd2_field_page_create();CHECK(vga->field_page);
    CHECK(fd2_field_game_bind_visual(game,vga)==0);
    fd2_field_page *oracle=fd2_field_page_clone(vga->field_page);CHECK(oracle);
    /* 第三项显式交付入口锁存的raw0/右板，而热selected/focus已不同；body
     * 不得重选二者。这是参数生命周期性质，不称等待期间自然移动focus。 */
    const int focus[]={6,7,7,0,0},left[]={0,1,0,0,0},display[]={1,0,1,7,1};
    for(unsigned kind=0;kind<5;kind++) {
        int failed=kind==4;
        game->selected_unit=kind==2?1:0;
        game->cursor_cell_x=focus[kind];game->cursor_cell_y=6;
        game->units.items[0].x=(uint8_t)(kind==2?6:focus[kind]);
        game->ordinary_display_mode=(uint8_t)display[kind];game->info_panel_right=0;
        vga->info_panel_right=expected->info_panel_right=0;
        vga->info_panel_suppressed=expected->info_panel_suppressed=0;
        memset(expected_pixels,0x5d,FD2_FIELD_PAGE_SIZE);
        CHECK(fd2_field_page_write(vga->field_page,0,0,456,336,expected_pixels,FD2_FIELD_PAGE_SIZE,456)==0 &&
            fd2_field_page_copy_pixels(oracle,vga->field_page)==0);
        memset(vga->framebuffer,0x77,sizeof(vga->framebuffer));
        memset(expected->framebuffer,0,sizeof(expected->framebuffer));
        memset(vga->palette,0x35,sizeof(vga->palette));memset(vga->dac,0x6b,sizeof(vga->dac));
        vga->field_water_phase=9;vga->field_water_last_tick=123;
        /* 离屏fixture只构造本地FIFO，不把稍后free的VGA注册为全局宿主owner。 */
        memset(&vga->input,0,sizeof(vga->input));
        fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_A,1);
        fd2_input_begin_test_frame(&vga->input,1000);CHECK(fd2_input_has_any_key(&vga->input));
        if(failed)game->info_assets.unit_summary.width=148;
        *before=*game;*old_vga=*vga;
        CHECK(fd2_field_game_runtime_summary_refresh(game,vga,0,left[kind])==(failed?-1:0));
        CHECK(summary_body_oracle(game,vga,oracle,expected,left[kind],failed)==0);
        CHECK(effect_page_read(vga->field_page,actual_pixels)==0 && effect_page_read(oracle,expected_pixels)==0 &&
            !memcmp(actual_pixels,expected_pixels,FD2_FIELD_PAGE_SIZE));
        if(failed) {
            CHECK(!memcmp(vga->framebuffer,old_vga->framebuffer,sizeof(vga->framebuffer)) &&
                vga->info_panel_right==0 && game->info_panel_right==0);
            int changed=0;
            for(size_t p=0;p<FD2_FIELD_PAGE_SIZE;p++)if(actual_pixels[p]!=0x5d){changed=1;break;}
            CHECK(changed); /* 页保留实际tile/actor前缀，未回滚为初始哨兵。 */
        } else {
            CHECK(fd2_field_page_project(oracle,72,72,expected->framebuffer,sizeof(expected->framebuffer))==0 &&
                !memcmp(vga->framebuffer,expected->framebuffer,sizeof(vga->framebuffer)) &&
                vga->info_panel_right==expected->info_panel_right && game->info_panel_right==vga->info_panel_right);
            before->info_panel_right=game->info_panel_right;
        }
        before->idle_phase=game->idle_phase;before->terrain_visual_phase=game->terrain_visual_phase;
        before->range_visual_phase=game->range_visual_phase;
        CHECK(!memcmp(before,game,sizeof(*game)) && !rng_calls &&
            !memcmp(&vga->input,&old_vga->input,sizeof(vga->input)) &&
            !memcmp(vga->palette,old_vga->palette,sizeof(vga->palette)) &&
            !memcmp(vga->dac,old_vga->dac,sizeof(vga->dac)) &&
            vga->field_water_phase==9 && vga->field_water_last_tick==123);
        if(failed)game->info_assets.unit_summary.width=149;
    }
    /* 坏raw/板侧在首个clock/page写入之前拒绝。 */
    for(int invalid=0;invalid<2;invalid++) {
        *before=*game;*old_vga=*vga;CHECK(effect_page_read(vga->field_page,expected_pixels)==0);
        CHECK(fd2_field_game_runtime_summary_refresh(game,vga,invalid?0:game->units.count,invalid?2:0)==-1);
        CHECK(!memcmp(before,game,sizeof(*game)) && !memcmp(old_vga,vga,sizeof(*vga)) &&
            effect_page_read(vga->field_page,actual_pixels)==0 &&
            !memcmp(actual_pixels,expected_pixels,FD2_FIELD_PAGE_SIZE) && !rng_calls);
    }
    fd2_field_page_destroy(oracle);fd2_field_page_destroy(vga->field_page);
    fd2_field_game_close(game);fd2_archive_close(&other);
    free(actual_pixels);free(expected_pixels);free(old_vga);free(expected);free(vga);free(before);free(game);
    return 0;
}

static int menu_base_oracle(fd2_field_game *game,fd2_vga *vga,fd2_field_page *page,fd2_vga *expected) {
    CHECK(fd2_field_background_draw_from(vga->field_page,page,72,72,0,0,0,0,0,0)==0);
    CHECK(fd2_terrain_render_page_block(page,&game->terrain,&game->map,72,72,13,8,0,0,
        game->terrain_visual_phase,game->idle_phase,NULL,NULL)==0);
    CHECK(fd2_field_units_render_page(page,&game->sprites,&game->units,0,0,game->idle_phase,
        (uint8_t)vga->field_visual.cursor_phase,vga->actor_jitter_phase)==0);
    CHECK(fd2_field_units_render_page_terrain_overlays(page,&game->terrain,&game->map,&game->units,
        0,0,game->terrain_visual_phase,NULL,NULL)==0);
    CHECK(fd2_field_game_render_page_info(game,expected,page,&game->units,0,0,
        game->cursor_cell_x,game->cursor_cell_y,game->idle_phase)==0);
    return 0;
}

static int menu_phase_oracle(fd2_field_game *game,fd2_vga *vga,fd2_field_page *page,
        fd2_vga *expected,const uint8_t saved[72*72],const uint8_t ids[4],
        const uint8_t disabled[4],int opening,int phase,int actor) {
    int x=48+game->cursor_cell_x*24,y=48+game->cursor_cell_y*24;
    CHECK(fd2_field_page_write(page,x,y,72,72,saved,72*72,72)==0);
    fd2_vga *icons=calloc(1,sizeof(*icons));CHECK(icons);
    CHECK(fd2_field_page_read(page,x,y,72,72,icons->framebuffer,sizeof(icons->framebuffer),320)==0);
    fd2_field_command_draw_id_animation(icons,&game->command_assets,24,24,ids,-1,disabled,0,opening,(uint8_t)phase);
    CHECK(fd2_field_page_write(page,x,y,72,72,icons->framebuffer,sizeof(icons->framebuffer),320)==0);
    free(icons);
    if(actor>=0)CHECK(fd2_field_units_render_page_actor(page,&game->sprites,&game->units,(size_t)actor,
        0,0,game->idle_phase,(uint8_t)vga->field_visual.cursor_phase,vga->actor_jitter_phase,0)==0);
    memset(expected->framebuffer,0,sizeof(expected->framebuffer));
    CHECK(fd2_field_page_project(page,72,72,expected->framebuffer,sizeof(expected->framebuffer))==0);
    return 0;
}

typedef struct {
    fd2_field_game *game;fd2_vga *vga;int error,fired,phase,bad_thread;
    SDL_ThreadID main_thread;SDL_AtomicInt queued;
} menu_prefix_probe;
static void SDLCALL menu_prefix_on_main(void *userdata) {
    menu_prefix_probe *probe=userdata;
    if(SDL_GetCurrentThreadID()!=probe->main_thread){probe->bad_thread=1;return;}
    probe->fired++;probe->phase=probe->game->command_animation_phase;
    if(probe->phase!=1)return; /* 错时点不伪造成首present前缀。 */
    if(probe->error)probe->vga->actor_jitter_phase=2; /* 下一direct叶的明确拒绝域。 */
    else probe->vga->input.quit_requested=1;
}
static int SDLCALL menu_prefix_queue(void *userdata) {
    menu_prefix_probe *probe=userdata;
    SDL_SetAtomicInt(&probe->queued,SDL_RunOnMainThread(menu_prefix_on_main,probe,false)?1:-1);
    return 0;
}

/* 1741c/176b4共用五种caller表，真实资源全page/VGA与跨函数原底图。
 * 只核clock调用族/索引及组合；不声明四相位都实际被显示器扫描。 */
static int test_menu_transition_source_owner(void) {
    /* EVENTS本身不建立SDL_MainThreadID；否则RunOnMainThread会在worker立即调用。 */
    SDL_SetMainReady();
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_archive other={0};fd2_field_game *game=calloc(1,sizeof(*game));
    fd2_vga *vga=calloc(1,sizeof(*vga)),*expected=calloc(1,sizeof(*expected));
    uint8_t *actual=malloc(FD2_FIELD_PAGE_SIZE),*want=malloc(FD2_FIELD_PAGE_SIZE);
    CHECK(game && vga && expected && actual && want);
    CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0 &&
        fd2_field_game_open_resources(game,&other,0)==0);
    vga->field_page=fd2_field_page_create();CHECK(vga->field_page);
    CHECK(fd2_field_game_bind_visual(game,vga)==0);
    fd2_field_page *oracle=fd2_field_page_clone(vga->field_page);CHECK(oracle);
    const uint8_t ids[5][4]={{0,1,2,3},{8,9,10,11},{7,5,6,4},{12,13,14,15},{18,21,22,25}};
    const int focus[5][2]={{0,0},{12,7},{6,4},{2,6},{10,6}};
    unsigned rng_calls=0;game->attack_rng=return_rng;game->attack_rng_userdata=&rng_calls;
    for(unsigned kind=0;kind<7;kind++) {
        unsigned table=kind<5?kind:0;
        CHECK(fd2_field_units_set(&game->units,0,0,0,2,focus[table][0],focus[table][1])==0);
        game->units.items[0].hp=31;game->units.items[0].hp_max=49;
        game->active_side=2;game->selected_unit=0;game->cursor_cell_x=focus[table][0];game->cursor_cell_y=focus[table][1];
        game->interaction=table==0?FD2_FIELD_INTERACTION_COMMAND:table==1?
            FD2_FIELD_INTERACTION_ITEM_MENU:FD2_FIELD_INTERACTION_SYSTEM_MENU;
        game->system_menu.page=table==3?FD2_FIELD_SYSTEM_PAGE_SECONDARY:
            table==4?FD2_FIELD_SYSTEM_PAGE_OPTIONS:FD2_FIELD_SYSTEM_PAGE_PARENT;
        memcpy(game->system_menu.command_ids,ids[table],4);
        uint8_t disabled[4]={0,1,0,0};memcpy(game->command_disabled,disabled,4);
        memcpy(game->system_menu.disabled,disabled,4);
        game->ordinary_display_mode=1;game->option_values[3]=1;game->failure=FD2_FIELD_FAILURE_NONE;
        game->command_backdrop_owner=NULL;game->command_animation_phase=game->system_menu.animation_phase=0;
        game->command_animation_opening=game->system_menu.animation_opening=0;
        vga->input.quit_requested=0;vga->actor_jitter_phase=0;vga->field_visual=(fd2_field_visual_state){0};
        vga->field_visual.actor_idle_tick=vga->field_visual.terrain_tick=vga->field_visual.range_tick=0x12345678u;
        vga->field_visual.cursor_phase=3;vga->field_water_phase=9;vga->field_water_last_tick=123;
        vga->info_panel_right=expected->info_panel_right=0;game->info_panel_right=0;
        memset(vga->dac,0x61,768);uint8_t dac[768];memcpy(dac,vga->dac,768);
        /* present显式交付此input；heap fixture不得注册成稍后悬空的全局owner。 */
        memset(&vga->input,0,sizeof(vga->input));
        fd2_input_set_test_key_state(&vga->input,SDL_SCANCODE_A,1);
        fd2_input_begin_test_frame(&vga->input,1000);
        memset(want,0x5d,FD2_FIELD_PAGE_SIZE);
        CHECK(fd2_field_page_write(vga->field_page,0,0,456,336,want,FD2_FIELD_PAGE_SIZE,456)==0 &&
            fd2_field_page_copy_pixels(oracle,vga->field_page)==0);
        menu_prefix_probe probe={0};probe.game=game;probe.vga=vga;probe.error=kind==6;
        probe.main_thread=SDL_GetCurrentThreadID();SDL_Thread *thread=NULL;
        if(kind>=5) {
            thread=SDL_CreateThread(menu_prefix_queue,"menu-prefix",&probe);CHECK(thread);
            uint64_t until=SDL_GetTicks()+3000u;
            while(!SDL_GetAtomicInt(&probe.queued) && SDL_GetTicks()<until)SDL_Delay(1);
            if(SDL_GetAtomicInt(&probe.queued)!=1) {
                SDL_WaitThread(thread,NULL);CHECK(SDL_GetAtomicInt(&probe.queued)==1);
            }
        }
        int result=table<2?fd2_field_game_animate_command(game,vga,1,999):
            fd2_field_game_animate_system_menu(game,vga,1,999);
        if(thread)SDL_WaitThread(thread,NULL);
        CHECK(result==(kind==5?1:kind==6?-1:0) && (!thread ||
            (probe.fired==1 && probe.phase==1 && !probe.bad_thread)));
        if(vga->field_visual.cursor_phase!=0 || vga->field_water_phase!=9 ||
            vga->field_water_last_tick!=123 || memcmp(dac,vga->dac,768) || rng_calls ||
            vga->frame_deadline_ns || !fd2_input_has_any_key(&vga->input))
            fprintf(stderr,"menu source kind=%u cursor=%u water=%u water_tick=%u dac=%d rng=%u deadline=%llu queued=%u phase=%u/%u\n",
                kind,(unsigned)vga->field_visual.cursor_phase,(unsigned)vga->field_water_phase,
                (unsigned)vga->field_water_last_tick,memcmp(dac,vga->dac,768),rng_calls,
                (unsigned long long)vga->frame_deadline_ns,(unsigned)vga->input.queued_count,
                (unsigned)game->command_animation_phase,(unsigned)game->system_menu.animation_phase);
        CHECK(vga->field_visual.cursor_phase==0 && vga->field_water_phase==9 &&
            vga->field_water_last_tick==123 && !memcmp(dac,vga->dac,768) && !rng_calls &&
            !vga->frame_deadline_ns && fd2_input_has_any_key(&vga->input));
        if(kind==6) {
            CHECK(vga->actor_jitter_phase==2);
            vga->actor_jitter_phase=0; /* raw26=0的纯像素oracle不读错误clock状态。 */
        }
        CHECK(menu_base_oracle(game,vga,oracle,expected)==0);
        uint8_t saved[72*72];int x=48+game->cursor_cell_x*24,y=48+game->cursor_cell_y*24;
        CHECK(fd2_field_page_read(oracle,x,y,72,72,saved,sizeof(saved),72)==0 &&
            !memcmp(saved,game->command_backdrop,sizeof(saved)) && game->command_backdrop_owner==vga->field_page);
        if(kind==6) {
            CHECK(menu_phase_oracle(game,vga,oracle,expected,saved,ids[table],disabled,1,1,0)==0);
            CHECK(!memcmp(vga->framebuffer,expected->framebuffer,64000));
            CHECK(menu_phase_oracle(game,vga,oracle,expected,saved,ids[table],disabled,1,2,-1)==0);
            CHECK(game->command_animation_phase==2 && game->command_animation_opening==1);
        } else CHECK(menu_phase_oracle(game,vga,oracle,expected,saved,ids[table],disabled,1,kind==5?1:4,0)==0);
        CHECK(effect_page_read(vga->field_page,actual)==0 && effect_page_read(oracle,want)==0 &&
            !memcmp(actual,want,FD2_FIELD_PAGE_SIZE));
        if(kind>=5)continue;
        CHECK(!memcmp(vga->framebuffer,expected->framebuffer,64000));
        fd2_field_visual_state before=vga->field_visual;
        /* wait可改当前page；close必须取open保存的历史底图，不能重新截图。 */
        uint8_t changed=247;CHECK(fd2_field_page_write(vga->field_page,x+2,y+2,1,1,&changed,1,1)==0);
        changed=246;CHECK(fd2_field_page_write(vga->field_page,0,0,1,1,&changed,1,1)==0 &&
            fd2_field_page_write(oracle,0,0,1,1,&changed,1,1)==0);
        CHECK((table<2?fd2_field_game_animate_command(game,vga,0,999):
            fd2_field_game_animate_system_menu(game,vga,0,999))==0);
        CHECK(!memcmp(&before,&vga->field_visual,sizeof(before)) && !game->command_backdrop_owner &&
            fd2_input_has_any_key(&vga->input)); /* 尾clear在Play成功边界。 */
        CHECK(menu_phase_oracle(game,vga,oracle,expected,saved,ids[table],disabled,0,4,0)==0 &&
            !memcmp(vga->framebuffer,expected->framebuffer,64000));
        CHECK(fd2_field_page_write(oracle,x,y,72,72,saved,sizeof(saved),72)==0 &&
            effect_page_read(vga->field_page,actual)==0 && effect_page_read(oracle,want)==0 &&
            !memcmp(actual,want,FD2_FIELD_PAGE_SIZE) && !rng_calls);
        CHECK((table<2?fd2_field_game_animate_command(game,vga,0,999):
            fd2_field_game_animate_system_menu(game,vga,0,999))==-1); /* 无有效open owner。 */
    }
    /* 入口HOST是另一个确切前缀：不能执行base/保存/phase或清键。 */
    fd2_field_game *saved_game=malloc(sizeof(*game));fd2_vga *saved_vga=malloc(sizeof(*vga));
    CHECK(saved_game && saved_vga);vga->input.quit_requested=1;
    *saved_game=*game;*saved_vga=*vga;CHECK(effect_page_read(vga->field_page,want)==0);
    CHECK(fd2_field_game_animate_command(game,vga,1,999)==1 &&
        !memcmp(saved_game,game,sizeof(*game)) && !memcmp(saved_vga,vga,sizeof(*vga)) &&
        effect_page_read(vga->field_page,actual)==0 && !memcmp(actual,want,FD2_FIELD_PAGE_SIZE));
    free(saved_game);free(saved_vga);
    fd2_field_page_destroy(oracle);fd2_field_page_destroy(vga->field_page);fd2_field_game_close(game);
    fd2_archive_close(&other);free(actual);free(want);free(expected);free(vga);free(game);SDL_Quit();return 0;
}

int main(void) {
    CHECK(test_menu_transition_source_owner() == 0);
    CHECK(test_runtime_summary_body() == 0);
    CHECK(test_runtime_menu_page_composition() == 0);
    CHECK(test_runtime_refresh_original_clock_order() == 0);
    CHECK(test_original_constructor_domain() == 0);
    CHECK(test_unit_domain_errors_and_mutations() == 0);
    CHECK(test_original_cache_raw_direction() == 0);
    CHECK(test_raw26_shared_phase_projection() == 0);
    CHECK(test_actor_group_flash_solid_color() == 0);
    CHECK(test_resource_effect_wrappers() == 0);
    CHECK(test_transition_whitening_finish() == 0);
    CHECK(test_app_page_effect_owners() == 0);
    CHECK(test_normal_tick_single_visual_advance() == 0);
    CHECK(test_terrain_only_visual_clock() == 0);
    CHECK(test_magic_background_hides_cursor_without_writes() == 0);
    CHECK(test_visual_phase_projection_during_blocking_animation() == 0);
    CHECK(test_battle_return_plain_cursor_and_invalid() == 0);
    CHECK(test_battle_return_current_map_and_hp() == 0);
    CHECK(test_transition_mask_actor_layer_and_preview() == 0);
    CHECK(test_empty_roof_cursor_and_selection() == 0);
    CHECK(test_actor_local_overlay_cells_and_immune() == 0);
    CHECK(test_overlay_lut_clip_and_readonly() == 0);
    CHECK(test_frame_and_coordinates() == 0);
    CHECK(test_acted_mapping_and_skip() == 0);
    CHECK(test_clip_and_reject() == 0);
    CHECK(test_original_raw_draw_order() == 0);
    CHECK(test_page_actor_raw_order_and_margin() == 0);
    CHECK(test_page_tile_skip_and_lut_domain() == 0);
    CHECK(test_stage32_defined_skip_history() == 0);
    CHECK(test_movement_host_prefix() == 0);
    puts("field_unit_render_test: ok");
    return 0;
}
