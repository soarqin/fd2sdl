/* field_focus_move_to @0x12cea（原登记 FUN_00010432）与
 * field_controller_input @0x117e7（原登记同名）：焦点/镜头纯状态回归。
 * 经正式方向移动和Z入口验证，不新增测试API，不启动SDL或窗口。 */
#include <stdio.h>
#include <string.h>

#include "field_game.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    return 1; } } while (0)

typedef struct {
    uint32_t state;
    size_t calls;
} test_rng;

static uint32_t unused_rng(void *userdata) {
    test_rng *rng = userdata;
    rng->calls++;
    rng->state = rng->state * 1664525u + 1013904223u;
    return rng->state;
}

static uint32_t cells[24 * 24];
static fd2_terrain_attr attr;

static void init_game(fd2_field_game *game, test_rng *rng) {
    memset(game, 0, sizeof(*game));
    game->ready = 1;
    game->stage = 0;
    game->active_side = 2;
    game->turn_number = 9;
    game->selected_unit = -1;
    game->detail_unit = -1;
    game->attack_target = -1;
    game->detail_acknowledged_unit = 3;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->map.width = game->map.height = 24;
    game->map.cells = cells;
    game->terrain.attrs = &attr;
    game->terrain.attr_count = 1;
    game->camera_cell_x = 5;
    game->camera_cell_y = 11;
    game->cursor_cell_x = 8;
    game->cursor_cell_y = 15;
    game->phase_unit_cursor = 4;
    game->phase_next_action_ms = 12345;
    game->pending_cell_event = (fd2_field_pending_cell_event){1, 0x42, 17};
    game->event_log_count = 1;
    game->event_total_count = 18;
    game->event_log[0].action = 0x43;
    game->resource_value = 0x12345678u;
    game->roster_count = 2;
    game->roster[1].data_09_1e[0] = 0xa5;
    game->option_values[0] = game->option_values[1] = 1;
    game->units.count = 1;
    game->units.items[0].side = 2;
    game->units.items[0].hp = game->units.items[0].hp_max = 50;
    game->attack_rng = unused_rng;
    game->attack_rng_userdata = rng;
}

/* 焦点、镜头、轮询位置和详情确认缓存是这两个正式入口允许修改的UI字段。
 * 整个session的其他字段（含单位、pending、owner指针和回合）必须原样保留。 */
static int other_state_unchanged(const fd2_field_game *game,
                                 const fd2_field_game *before) {
    fd2_field_game normalized = *game;
    normalized.camera_cell_x = before->camera_cell_x;
    normalized.camera_cell_y = before->camera_cell_y;
    normalized.cursor_cell_x = before->cursor_cell_x;
    normalized.cursor_cell_y = before->cursor_cell_y;
    normalized.focus_cycle_unit = before->focus_cycle_unit;
    normalized.detail_acknowledged_unit = before->detail_acknowledged_unit;
    return memcmp(&normalized, before, sizeof(normalized)) == 0;
}

static int test_direction_thresholds(void) {
    /* 使用移动前relative；raw 11c73/11b62为jge2，11c19为jle10，11bba为jle5。 */
    const struct {
        int cx, cy, x, y, dx, dy, expected_cx, expected_cy, expected_x, expected_y;
    } cases[] = {
        {5,11, 7,15, -1,0, 5,11, 6,15},
        {5,11, 6,15, -1,0, 4,11, 5,15},
        {5,11, 8,13, 0,-1, 5,11, 8,12},
        {5,11, 8,12, 0,-1, 5,10, 8,11},
        {5,11,15,15, 1,0, 5,11,16,15},
        {5,11,16,15, 1,0, 6,11,17,15},
        {5,11, 8,16, 0,1, 5,11, 8,17},
        {5,11, 8,17, 0,1, 5,12, 8,18},
        {0,11, 1,15, -1,0, 0,11, 0,15},
        {5, 0, 8, 1, 0,-1, 5, 0, 8, 0},
        {11,11,22,15, 1,0,11,11,23,15},
        {5,16, 8,22, 0,1, 5,16, 8,23},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        test_rng rng = {0x31415926u, 0};
        fd2_field_game game;
        init_game(&game, &rng);
        game.camera_cell_x = cases[i].cx;
        game.camera_cell_y = cases[i].cy;
        game.cursor_cell_x = cases[i].x;
        game.cursor_cell_y = cases[i].y;
        fd2_field_game before = game;
        CHECK(fd2_field_game_move_cursor(&game, cases[i].dx, cases[i].dy) == 1);
        CHECK(game.cursor_cell_x == cases[i].expected_x &&
              game.cursor_cell_y == cases[i].expected_y);
        CHECK(game.camera_cell_x == cases[i].expected_cx &&
              game.camera_cell_y == cases[i].expected_cy);
        CHECK(other_state_unchanged(&game, &before));
        CHECK(rng.calls == 0 && rng.state == 0x31415926u);
    }
    return 0;
}

static int test_offscreen_and_edge_focus(void) {
    const struct { int x, y, cx, cy; } cases[] = {
        {2,15,1,11}, {20,15,9,11}, {8,5,5,4}, {8,22,5,16},
        {2,5,1,4}, {20,22,9,16}, {0,0,0,0}, {23,23,11,16},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        test_rng rng = {0x31415926u, 0};
        fd2_field_game game;
        init_game(&game, &rng);
        game.units.items[0].x = (uint8_t)cases[i].x;
        game.units.items[0].y = (uint8_t)cases[i].y;
        fd2_field_game before = game;
        CHECK(fd2_field_game_cycle_focus(&game) == 0);
        CHECK(game.cursor_cell_x == cases[i].x && game.cursor_cell_y == cases[i].y);
        CHECK(game.camera_cell_x == cases[i].cx && game.camera_cell_y == cases[i].cy);
        CHECK(other_state_unchanged(&game, &before));
        CHECK(rng.calls == 0 && rng.state == 0x31415926u);
    }
    return 0;
}

static int test_same_target_and_world_clamp(void) {
    test_rng rng = {0x31415926u, 0};
    fd2_field_game game;
    init_game(&game, &rng);
    /* 同目标即使relative0且camera可卷，也不能人为居中或补一格边距。 */
    game.cursor_cell_x = game.units.items[0].x = 5;
    game.cursor_cell_y = game.units.items[0].y = 11;
    fd2_field_game before = game;
    CHECK(fd2_field_game_cycle_focus(&game) == 0);
    CHECK(game.camera_cell_x == 5 && game.camera_cell_y == 11);
    CHECK(other_state_unchanged(&game, &before));
    game.camera_cell_x = game.camera_cell_y = 0;
    game.cursor_cell_x = game.cursor_cell_y = 0;
    before = game;
    CHECK(fd2_field_game_move_cursor(&game, -1, 0) == 0);
    CHECK(fd2_field_game_move_cursor(&game, 0, -1) == 0);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    game.camera_cell_x = 11;
    game.camera_cell_y = 16;
    game.cursor_cell_x = game.cursor_cell_y = 23;
    before = game;
    CHECK(fd2_field_game_move_cursor(&game, 1, 0) == 0);
    CHECK(fd2_field_game_move_cursor(&game, 0, 1) == 0);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    CHECK(rng.calls == 0 && rng.state == 0x31415926u);
    return 0;
}

static int test_cycle_gates_and_invalid_target(void) {
    test_rng rng = {0x31415926u, 0};
    fd2_field_game game;
    init_game(&game, &rng);
    game.units.count = 6;
    for (size_t i = 0; i < game.units.count; i++) {
        game.units.items[i].side = 2;
        game.units.items[i].x = 2;
        game.units.items[i].y = 5;
    }
    game.units.items[0].side = 0;
    game.units.items[1].flags = 1;
    game.units.items[2].flags = 4;
    game.units.items[3].flags = 0x80;
    game.units.items[4].hp = 0; /* native flags&0x85/side门不额外检查HP。 */
    game.units.items[5].x = 20;
    game.units.items[5].y = 22;
    fd2_field_game before = game;
    CHECK(fd2_field_game_cycle_focus(&game) == 4);
    CHECK(game.focus_cycle_unit == 5 && game.camera_cell_x == 1 && game.camera_cell_y == 4);
    CHECK(other_state_unchanged(&game, &before));
    CHECK(fd2_field_game_cycle_focus(&game) == 5 && game.focus_cycle_unit == 0);
    CHECK(fd2_field_game_cycle_focus(&game) == 4 && game.focus_cycle_unit == 5);
    CHECK(other_state_unchanged(&game, &before));
    for (size_t i = 0; i < game.units.count; i++) game.units.items[i].flags = 1;
    before = game;
    CHECK(fd2_field_game_cycle_focus(&game) == -1);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);

    const uint8_t invalid[][2] = {{24,5}, {2,24}, {255,255}};
    for (size_t i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        init_game(&game, &rng);
        game.units.count = 2;
        game.focus_cycle_unit = 1;
        game.units.items[1].side = 2;
        game.units.items[1].x = invalid[i][0];
        game.units.items[1].y = invalid[i][1];
        before = game;
        CHECK(fd2_field_game_cycle_focus(&game) == -1);
        CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    }
    CHECK(rng.calls == 0 && rng.state == 0x31415926u);
    return 0;
}

int main(void) {
    if (test_direction_thresholds() || test_offscreen_and_edge_focus() ||
        test_same_target_and_world_clamp() || test_cycle_gates_and_invalid_target())
        return 1;
    puts("field_focus_test: ok");
    return 0;
}
