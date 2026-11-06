/* sub_127E0 @0x127e0 / sub_4E1A6 @0x4e1a6：合成FDICON验证frame、
 * 坐标、已行动映色与透明；129ec/12ac6验证actor-local前景恢复，
 * 正式Game绘序覆盖空屋顶cursor回归。不启动窗口、不改bank/actor owner。 */
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include "field_unit.h"
#include "field_game.h"

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

int main(void) {
    CHECK(test_empty_roof_cursor_and_selection() == 0);
    CHECK(test_actor_local_overlay_cells_and_immune() == 0);
    CHECK(test_overlay_lut_clip_and_readonly() == 0);
    CHECK(test_frame_and_coordinates() == 0);
    CHECK(test_acted_mapping_and_skip() == 0);
    CHECK(test_clip_and_reject() == 0);
    CHECK(test_depth_order() == 0);
    puts("field_unit_render_test: ok");
    return 0;
}
