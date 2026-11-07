/* sub_127E0 @0x127e0 / sub_4E1A6 @0x4e1a6：合成FDICON验证frame、
 * 坐标、已行动映色与透明；129ec/12ac6验证actor-local前景恢复，
 * 正式Game绘序覆盖空屋顶cursor回归。不启动窗口、不改bank/actor owner。 */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_attack.h"
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

static uint32_t return_rng(void *userdata) {
    unsigned *calls = userdata;
    return ++*calls;
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
        CHECK(fd2_field_game_tick_visual(g,cases[i].now)==0);
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
    CHECK(fd2_field_game_tick_visual(g,10000)==-1 && !memcmp(g,before,sizeof(*g)) && !calls);
    g->failure=FD2_FIELD_FAILURE_NONE;g->ready=0;*before=*g;
    CHECK(fd2_field_game_tick_visual(g,10000)==-1 && !memcmp(g,before,sizeof(*g)));
    CHECK(fd2_field_game_tick_visual(NULL,10000)==-1);
    free(cells);free(before);free(pages);fd2_field_game_close(g);free(g);fd2_archive_close(&fdother);return 0;
}

/* 原1297d长时间无调用也只加一次，且下一次从新last计时。旧追赶循环真RED。 */
static int test_normal_tick_single_visual_advance(void) {
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);
    g->ready=1;g->active_side=2;
    fd2_field_game_tick(g,0);
    CHECK(g->idle_phase==0 && g->last_idle_ms==0);
    fd2_field_game_tick(g,1000);
    CHECK(g->idle_phase==0 && g->last_idle_ms==1000);
    fd2_field_game_tick(g,4000);
    CHECK(g->idle_phase==1 && g->last_idle_ms==4000);
    fd2_field_game_tick(g,4200);
    CHECK(g->idle_phase==1 && g->last_idle_ms==4000);
    fd2_field_game_tick(g,4475);
    CHECK(g->idle_phase==2 && g->last_idle_ms==4475);
    free(g);return 0;
}

int main(void) {
    CHECK(test_normal_tick_single_visual_advance() == 0);
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
    CHECK(test_depth_order() == 0);
    puts("field_unit_render_test: ok");
    return 0;
}
