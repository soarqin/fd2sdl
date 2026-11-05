/* field_player_item_command_execute @0x1bbdc（原登记同名）的菜单ID8..11，
 * 复用field_command_menu_draw @0x179d5及open/close @0x1741c/@0x176b4。
 * 合成资源锁定session接线与actor最后覆盖；不声明真实资源像素或WGC通过。 */
#include <stdio.h>
#include <string.h>
#include "field_game.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

typedef struct {
    fd2_field_game game;
    uint8_t commands[FD2_FIELD_COMMAND_FRAME_COUNT][24 * 20];
    uint8_t actors[12 * 24 * 24];
} fixture;

static void init(fixture *f) {
    memset(f, 0, sizeof(*f));
    fd2_field_game *g = &f->game;
    g->ready = 1; g->active_side = 2;
    g->interaction = FD2_FIELD_INTERACTION_ITEM_MENU;
    g->selected_unit = 0;
    g->map.width = g->map.height = 24;
    g->cursor_cell_x = g->cursor_cell_y = 3;
    g->units.count = 1;
    g->units.items[0].x = g->units.items[0].y = 3;
    g->units.items[0].side = 2;
    g->units.items[0].hp = g->units.items[0].hp_max = 42;
    memset(f->actors, 220, sizeof(f->actors));
    g->sprites.decoded_frames = f->actors;
    g->sprites.frame_count = 12;
    g->command_assets.ready = 1;
    g->command_assets.frame_count = FD2_FIELD_COMMAND_FRAME_COUNT;
    for (size_t i = 0; i < FD2_FIELD_COMMAND_FRAME_COUNT; i++) {
        memset(f->commands[i], (int)(32u + i), sizeof(f->commands[i]));
        g->command_assets.frames[i] = (fd2_image){24, 20, f->commands[i]};
    }
}

static void fill(fd2_vga *vga, int x, int y, int w, int h, uint8_t color) {
    for (int row = 0; row < h; row++)
        memset(vga->framebuffer + (y + row) * VGA_STRIDE + x, color, (size_t)w);
}

static int check_frame(fixture *f, int opening, int phase) {
    fd2_field_game *g = &f->game;
    fd2_vga actual = {0}, expected = {0};
    memset(actual.framebuffer, 0xa5, sizeof(actual.framebuffer));
    memset(expected.framebuffer, 0xa5, sizeof(expected.framebuffer));
    fd2_field_game_render_background(g, &expected);
    int dx[] = {0, opening ? -6 * phase : -24 + 6 * phase,
                   opening ? 6 * phase : 24 - 6 * phase, 0};
    int dy[] = {opening ? 2 - 5 * phase : -20 + 5 * phase, 2, 2,
                opening ? 2 + 5 * phase : 24 - 5 * phase};
    for (int item = 0; item < 4; item++) {
        int id = (g->interaction == FD2_FIELD_INTERACTION_ITEM_MENU ? 8 : 0) + item;
        int frame = id * 3 + 2 * g->command_disabled[item];
        if (!opening && !phase && item == g->command_selected)
            frame += g->command_highlight_phase;
        fill(&expected, 76 + dx[item], 76 + dy[item], 24, 20, (uint8_t)(32 + frame));
    }
    /* actor上移6px，重画发生在最后一个图标之后，不依靠透明角漏出。 */
    fill(&expected, 76, 70, 24, 24, 220);
    g->command_animation_opening = (uint8_t)opening;
    g->command_animation_phase = (uint8_t)phase;
    fd2_field_game_render(g, &actual);
    CHECK(memcmp(actual.framebuffer, expected.framebuffer, sizeof(actual.framebuffer)) == 0);
    return 0;
}

static int test_ids_states_animation_and_actor_overlay(void) {
    fixture f;
    init(&f);
    for (int selected = 0; selected < 4; selected++) {
        f.game.command_selected = selected;
        for (unsigned highlight = 0; highlight < 2; highlight++) {
            f.game.command_highlight_phase = (uint8_t)highlight;
            CHECK(check_frame(&f, 0, 0) == 0);
        }
    }
    /* SDL支持Use/Drop；Give/EQT的禁用帧不是原版能力声明。 */
    f.game.command_selected = 0;
    f.game.command_disabled[1] = f.game.command_disabled[2] = 1;
    f.game.command_disabled[3] = 0;
    CHECK(check_frame(&f, 0, 0) == 0);
    for (int opening = 0; opening < 2; opening++)
        for (int phase = 1; phase <= 4; phase++) CHECK(check_frame(&f, opening, phase) == 0);
    f.game.interaction = FD2_FIELD_INTERACTION_COMMAND;
    CHECK(check_frame(&f, 0, 0) == 0); /* 正常COMMAND仍使用0..3。 */
    return 0;
}

static int test_tick_animation_entry_and_nonmenu_isolation(void) {
    fixture f;
    init(&f);
    fd2_vga vga = {0}; /* 无窗口／renderer；present函数只走空owner。 */
    fd2_field_game_tick(&f.game, 1);
    fd2_field_game_tick(&f.game, 221);
    CHECK(f.game.command_highlight_phase == 1 && f.game.last_command_highlight_ms == 221);
    fd2_field_units before = f.game.units;
    CHECK(fd2_field_game_animate_command(&f.game, &vga, 1, 0) == 0);
    CHECK(f.game.command_animation_phase == 0 && !f.game.command_animation_opening);
    CHECK(fd2_field_game_animate_command(&f.game, &vga, 0, 0) == 0);
    CHECK(memcmp(&before, &f.game.units, sizeof(before)) == 0);
    f.game.interaction = FD2_FIELD_INTERACTION_ITEM_LIST;
    fd2_vga expected = {0};
    fd2_field_game_render_background(&f.game, &expected);
    fd2_field_game_render(&f.game, &vga);
    CHECK(memcmp(vga.framebuffer, expected.framebuffer, sizeof(vga.framebuffer)) == 0);
    fd2_field_game snapshot = f.game;
    CHECK(fd2_field_game_animate_command(&f.game, &vga, 1, 0) == -1);
    CHECK(memcmp(&snapshot, &f.game, sizeof(snapshot)) == 0);
    return 0;
}

int main(void) {
    CHECK(test_ids_states_animation_and_actor_overlay() == 0);
    CHECK(test_tick_animation_entry_and_nonmenu_isolation() == 0);
    puts("field item menu render tests: ok");
    return 0;
}
