/* B1.4c2：field_empty_focus_menu_execute @0x16f55 selector1 与
 * field_ai_move_toward_cell @0x14b78。合成session验证逐actor提交和失败
 * 边界，不使用原版捕获坐标作通用默认，也不替代WGC配对。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_game.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

typedef struct {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attr;
    size_t rng_calls;
} fixture;

static uint32_t next_roll(void *data) { (*(size_t *)data)++; return 0; }

static void set_unit(fd2_field_unit *u, uint8_t side, uint8_t x, uint8_t y) {
    memset(u, 0, sizeof(*u));
    u->side = side; u->x = x; u->y = y;
    u->hp = u->hp_max = 50;
    u->movement_profile = 1;
    u->movement_points = 2;
    u->ai_behavior_raw = 8;
}

static void init(fixture *f) {
    memset(f, 0, sizeof(*f));
    fd2_field_game *g = &f->game;
    memset(g->metadata.turn_events, 0xff, sizeof(g->metadata.turn_events));
    memset(g->metadata.cell_lookup, 0xff, sizeof(g->metadata.cell_lookup));
    g->ready = 1; g->turn_number = 1; g->active_side = 2;
    g->selected_unit = g->command_selected = g->attack_target = g->detail_unit = -1;
    g->interaction = FD2_FIELD_INTERACTION_BROWSE;
    g->map.width = g->map.height = 8; g->map.cells = f->cells;
    g->terrain.attr_count = 1; g->terrain.attrs = &f->attr;
    f->attr.movement_cost_class = 0;
    g->units.count = 3;
    set_unit(&g->units.items[0], 2, 1, 1);
    set_unit(&g->units.items[1], 2, 1, 3);
    set_unit(&g->units.items[2], 0, 7, 7);
    g->units.items[2].flags = 4;
    g->cursor_cell_x = 4; g->cursor_cell_y = 2;
    g->attack_rng = next_roll; g->attack_rng_userdata = &f->rng_calls;
    memset(g->roster, 0x5a, sizeof(g->roster)); g->roster_count = 2;
    g->resource_value = 1234;
}

static void close_fixture(fixture *f) {
    fd2_field_path_close(&f->game.move_range);
    free(f->game.move_path);
}

static int frozen(fixture *f) {
    fd2_field_game before = f->game;
    size_t calls = f->rng_calls;
    CHECK(fd2_field_game_march(&f->game, NULL, 0) == -1);
    CHECK(fd2_field_game_process_automatic_action(&f->game) == -1);
    CHECK(fd2_field_game_confirm_cursor(&f->game) == -1);
    CHECK(!memcmp(&before, &f->game, sizeof(before)) && calls == f->rng_calls);
    return 0;
}

static int test_fixed_anchor_and_whole_round(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    fd2_field_unit roster[FD2_SAVE_BATTLE_MAX_ROSTER];
    memcpy(roster, g->roster, sizeof(roster));
    fd2_field_metadata metadata = g->metadata;
    uint8_t cell_state[FD2_FIELD_EVENT_SLOTS];
    memcpy(cell_state, g->cell_action_completed, sizeof(cell_state));
    CHECK(fd2_field_game_march(g, NULL, 0) == 0);
    /* 两名不同起点都朝同一(4,2)；不是朝前一actor移动后的focus。 */
    CHECK(g->units.items[0].x == 3 && g->units.items[0].y == 1);
    CHECK(g->units.items[1].x == 3 && g->units.items[1].y == 3);
    CHECK(fd2_field_unit_has_acted(&g->units.items[0]));
    CHECK(fd2_field_unit_has_acted(&g->units.items[1]));
    CHECK(g->selected_unit == -1 && g->interaction == FD2_FIELD_INTERACTION_BROWSE);
    CHECK(g->active_side == 1 && g->turn_number == 1);
    CHECK(!g->player_post_waiting_phase && !g->pending_cell_event.valid);
    CHECK(g->units.items[0].direction == 0 && g->units.items[1].direction == 0);
    /* 输入owner只能等待phase，不能再调用玩家入口。 */
    CHECK(fd2_field_game_confirm_cursor(g) == -1);
    for (int guard = 0; g->active_side != 2 && guard < 8; guard++)
        CHECK(fd2_field_game_process_automatic_action(g) == 1);
    CHECK(g->active_side == 2 && g->turn_number == 2);
    CHECK(!fd2_field_unit_has_acted(&g->units.items[0]));
    CHECK(!fd2_field_unit_has_acted(&g->units.items[1]));
    CHECK(g->cursor_cell_x == 3 && g->cursor_cell_y == 1);
    CHECK(!memcmp(roster, g->roster, sizeof(roster)) && g->roster_count == 2);
    CHECK(!memcmp(&metadata, &g->metadata, sizeof(metadata)));
    CHECK(!memcmp(cell_state, g->cell_action_completed, sizeof(cell_state)));
    CHECK(g->resource_value == 1234 && f.rng_calls == 0 && g->event_log_count == 0);
    close_fixture(&f);
    return 0;
}

static int test_unknown_handler_preserves_completed_actors(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    g->cursor_cell_y = 1;
    /* 第一名沿row1完成；第二名路径中命中，移动走完后才dispatch。 */
    for (size_t y = 2; y < 8; y++)
        for (size_t x = 0; x < 8; x++) f.cells[y * 8 + x] = 1u << 16;
    g->metadata.cell_lookup[0] = (fd2_field_cell_lookup){9, 0};
    g->pending_cell_event = (fd2_field_pending_cell_event){1, 42, 4};
    CHECK(fd2_field_game_march(g, NULL, 0) == -1);
    CHECK(g->failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
    CHECK(g->failure_notice.unit_index == 1 && g->failure_notice.action == 9);
    CHECK(g->failure_notice.slot == 0);
    CHECK(g->units.items[0].x == 3 && g->units.items[0].y == 1);
    CHECK(fd2_field_unit_has_acted(&g->units.items[0]));
    CHECK(g->units.items[1].x != 1 || g->units.items[1].y != 3);
    CHECK(!fd2_field_unit_has_acted(&g->units.items[1]));
    CHECK(g->units.items[1].direction != 0);
    CHECK(g->pending_cell_event.valid && g->pending_cell_event.action == 9);
    CHECK(g->active_side == 2 && g->turn_number == 1);
    CHECK(g->event_log_count == 1 && f.rng_calls == 0);
    CHECK(frozen(&f) == 0);
    close_fixture(&f);
    return 0;
}

static int test_later_geometry_error_keeps_first_commit(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    g->units.items[1].movement_profile = UINT8_MAX;
    CHECK(fd2_field_game_march(g, NULL, 0) == -1);
    CHECK(g->failure == FD2_FIELD_FAILURE_ACTION && g->failure_notice.unit_index == 1);
    CHECK(g->units.items[0].x == 3 && g->units.items[0].y == 1);
    CHECK(fd2_field_unit_has_acted(&g->units.items[0]));
    CHECK(g->units.items[1].x == 1 && g->units.items[1].y == 3);
    CHECK(!fd2_field_unit_has_acted(&g->units.items[1]));
    CHECK(g->active_side == 2 && g->event_log_count == 0);
    CHECK(f.rng_calls == 0 && frozen(&f) == 0);
    close_fixture(&f);
    return 0;
}

static int test_no_path_still_acted_without_match1_or_recovery(void) {
    for (int zero_budget = 0; zero_budget < 2; zero_budget++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        g->units.items[1].flags = 0x80;
        g->units.items[0].hp = 25;
        g->units.items[0].movement_points = zero_budget ? 0 : 2;
        g->cursor_cell_x = zero_budget ? 4 : 1; g->cursor_cell_y = 1;
        f.cells[1 * 8 + 1] = 1u << 16;
        g->metadata.cell_lookup[0] = (fd2_field_cell_lookup){9, 1};
        g->pending_cell_event = (fd2_field_pending_cell_event){1, 99, 8};
        CHECK(fd2_field_game_march(g, NULL, 0) == 0);
        CHECK(g->units.items[0].x == 1 && g->units.items[0].y == 1);
        CHECK(g->units.items[0].hp == 25); /* 不套用WAIT/AI common tail的恢复。 */
        CHECK(fd2_field_unit_has_acted(&g->units.items[0]));
        CHECK(g->active_side == 1 && g->event_log_count == 0);
        CHECK(!g->pending_cell_event.valid && f.rng_calls == 0);
        close_fixture(&f);
    }
    return 0;
}

static int test_exact_flags_side_and_hp0_gate(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    g->units.count = 8;
    set_unit(&g->units.items[1], 2, 5, 5); g->units.items[1].flags = 0x80;
    set_unit(&g->units.items[2], 2, 6, 5); g->units.items[2].flags = 1;
    set_unit(&g->units.items[3], 2, 5, 6); g->units.items[3].flags = 4;
    set_unit(&g->units.items[4], 1, 6, 6);
    set_unit(&g->units.items[5], 0, 7, 7); g->units.items[5].flags = 4;
    set_unit(&g->units.items[6], 2, 1, 3);
    g->units.items[6].hp = 0;
    g->units.items[6].detail_status[1] = 2;
    g->units.items[6].unit_id = 121; g->units.items[6].race = 10;
    set_unit(&g->units.items[7], 2, 0, 7); /* 失败后的raw actor不能启动。 */
    g->cursor_cell_x = 4; g->cursor_cell_y = 3;
    f.cells[3 * 8 + 2] = f.cells[3 * 8 + 3] = 1u << 16;
    g->metadata.cell_lookup[0] = (fd2_field_cell_lookup){9, 0};
    fd2_field_units before = g->units;
    CHECK(fd2_field_game_march(g, NULL, 0) == -1);
    CHECK(g->failure_notice.unit_index == 6);
    CHECK(g->units.items[6].x == 3 && g->units.items[6].y == 3);
    CHECK(g->units.items[6].hp == 0 && !fd2_field_unit_has_acted(&g->units.items[6]));
    CHECK(fd2_field_unit_has_acted(&g->units.items[0]));
    for (size_t i = 1; i <= 5; i++)
        CHECK(!memcmp(&before.items[i], &g->units.items[i], sizeof(before.items[i])));
    CHECK(!memcmp(&before.items[7], &g->units.items[7], sizeof(before.items[7])));
    CHECK(f.rng_calls == 0 && frozen(&f) == 0);
    close_fixture(&f);
    return 0;
}

static int test_hp0_actor_completes_before_phase_failure(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    g->units.items[1].hp = 0;
    g->metadata.turn_events[0] = (fd2_field_turn_event){1, 99, 1};
    CHECK(fd2_field_game_march(g, NULL, 0) == -1);
    CHECK(g->units.items[1].x == 3 && g->units.items[1].y == 3);
    CHECK(fd2_field_unit_has_acted(&g->units.items[1]));
    CHECK(g->failure_notice.kind == FD2_FIELD_EVENT_TURN && g->failure_notice.action == 99);
    CHECK(g->active_side == 1 && g->event_log_count == 1 && f.rng_calls == 0);
    CHECK(frozen(&f) == 0);
    close_fixture(&f);
    return 0;
}

static int test_guards_and_host_quit_before_batch(void) {
    for (int which = 0; which < 5; which++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        if (which == 0) g->stage = 1;
        if (which == 1) g->active_side = 0;
        if (which == 2) g->player_post_waiting_phase = 1;
        if (which == 3) g->selected_unit = 0;
        if (which == 4) g->failure = FD2_FIELD_FAILURE_ACTION;
        fd2_field_game before = *g;
        CHECK(fd2_field_game_march(g, NULL, 0) == -1);
        CHECK(!memcmp(g, &before, sizeof(before)) && f.rng_calls == 0);
        close_fixture(&f);
    }
    fixture f; init(&f);
    fd2_vga vga = {0}; vga.input.quit_requested = 1;
    fd2_field_game before = f.game;
    CHECK(fd2_field_game_march(&f.game, &vga, 0) == 1);
    CHECK(!memcmp(&f.game, &before, sizeof(before)) && f.rng_calls == 0);
    close_fixture(&f);
    return 0;
}

static int test_real_start_march_geometry_and_phase_entry(void) {
    /* 固定真实夹具的独立原版研究结果；不是通用默认坐标。只断言批次
     * 终点和进入side1，后续原版/SDL整轮输入与WGC仍需另行配对。 */
    fd2_archive fdother = {0};
    fd2_vga vga = {0};
    fd2_field_game game;
    CHECK(fd2_archive_open(&fdother, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_field_game_open(&game, &vga, &fdother, 0) == 0);
    CHECK(fd2_field_game_load_active(&game, "original_game/START.SAV") == 0);
    CHECK(game.stage == 0 && game.turn_number == 1 && game.units.count == 12 && game.roster_count == 4);
    const uint8_t final_x[] = {7, 7, 6, 8}, final_y[] = {15, 14, 15, 16};
    size_t calls = 0;
    game.attack_rng = next_roll; game.attack_rng_userdata = &calls;
    game.cursor_cell_x = 7; game.cursor_cell_y = 15;
    uint32_t resource = game.resource_value;
    fd2_field_unit roster[FD2_SAVE_BATTLE_MAX_ROSTER];
    memcpy(roster, game.roster, sizeof(roster));
    CHECK(fd2_field_game_march(&game, NULL, 0) == 0);
    for (size_t i = 0; i < 4; i++) {
        CHECK(game.units.items[i].x == final_x[i] && game.units.items[i].y == final_y[i]);
        CHECK(fd2_field_unit_has_acted(&game.units.items[i]));
    }
    CHECK(game.active_side == 1 && game.turn_number == 1 && !game.player_post_waiting_phase);
    CHECK(game.units.count == 12 && game.roster_count == 4);
    CHECK(!memcmp(roster, game.roster, sizeof(roster)) && game.resource_value == resource);
    CHECK(game.event_log_count == 0 && game.failure == FD2_FIELD_FAILURE_NONE && calls == 0);
    fd2_field_game_close(&game);
    fd2_archive_close(&fdother);
    return 0;
}

static int test_batch_background_policy_does_not_leak(void) {
    /* 只初始化事件子系统，无SDL窗口/renderer。正式present仍泵送真实
     * input生命周期，NULL texture使其不上传图像；不用生产测试钩子。 */
    CHECK(SDL_Init(SDL_INIT_EVENTS));
    fd2_input_install_interrupt_handlers();
    for (int which = 0; which < 3; which++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        fd2_vga vga = {0};
        fd2_input_init(&vga.input);
        g->units.items[0].y = 6;
        g->units.items[1].flags = 0x80;
        g->cursor_cell_x = 4; g->cursor_cell_y = 6;
        g->info_panel_right = 0;
        if (which == 1) {
            f.cells[6 * 8 + 2] = 1u << 16;
            g->metadata.cell_lookup[0] = (fd2_field_cell_lookup){9, 0};
        }
        if (which == 2) {
            SDL_Event event = {0}; event.type = SDL_EVENT_QUIT;
            CHECK(SDL_PushEvent(&event));
        }
        CHECK(fd2_field_game_march(g, &vga, 0) == (which == 0 ? 0 : which == 1 ? -1 : 1));
        CHECK(g->units.items[0].x == 3 && g->units.items[0].y == 6);
        /* 普通render第一步的focus(1,6)/(2,6)会把panel切到右侧；
         * clean-background不调用panel，也不提交这个UI owner。 */
        CHECK(g->info_panel_right == 0 && f.rng_calls == 0);
        CHECK(fd2_field_unit_has_acted(&g->units.items[0]) == (which == 0));
        g->cursor_cell_x = 1; g->cursor_cell_y = 6;
        fd2_field_game_render(g, &vga);
        CHECK(g->info_panel_right == 1 && f.rng_calls == 0);
        fd2_input_install_interrupt_handlers(); /* 本测试进程清宿主quit latch。 */
        close_fixture(&f);
    }
    for (int march_phase = 0; march_phase < 2; march_phase++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        fd2_vga vga = {0}; fd2_input_init(&vga.input);
        g->units.items[0].x = g->units.items[0].y = 7;
        g->units.items[0].flags = g->units.items[1].flags = 0x80;
        g->units.items[2].x = 1; g->units.items[2].y = 6;
        g->units.items[2].flags = 0;
        g->units.items[2].ai_behavior_raw = 4;
        g->units.items[2].ai_param_35 = 4; g->units.items[2].ai_param_36 = 6;
        CHECK((march_phase ? fd2_field_game_march(g, NULL, 0)
                           : fd2_field_game_end_player_turn(g)) == 0);
        CHECK(g->active_side == 1);
        CHECK(fd2_field_game_process_automatic_action_visual(g, &vga, 0, march_phase) == 1);
        CHECK(g->active_side == 0 && g->info_panel_right == 0);
        CHECK(fd2_field_game_process_automatic_action_visual(g, &vga, 0, march_phase) == 1);
        CHECK(g->units.items[2].x == 3 && g->units.items[2].y == 6);
        /* 同一自动phase：只有March续行隐藏panel，普通END保留原呈现。 */
        CHECK(g->info_panel_right == (march_phase ? 0 : 1) && f.rng_calls == 0);
        g->cursor_cell_x = 1; g->cursor_cell_y = 6;
        fd2_field_game_render(g, &vga);
        CHECK(g->info_panel_right == 1);
        close_fixture(&f);
    }
    size_t normal_rng_calls = 0;
    uint16_t normal_target_hp = 0;
    for (int clean = 0; clean < 2; clean++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        fd2_vga vga = {0}; fd2_input_init(&vga.input);
        g->stage = 1; g->active_side = 0; g->units.count = 2;
        set_unit(&g->units.items[0], 0, 1, 6);
        set_unit(&g->units.items[1], 2, 4, 6);
        g->units.items[0].ai_behavior_raw = 0;
        g->units.items[0].attack = 20; g->units.items[0].accuracy = 100;
        ((uint8_t *)&g->units.items[0])[0x0a] = 0x40; /* equipped weapon0 */
        CHECK(fd2_field_game_process_automatic_action_visual(g, &vga, 0, clean) == 1);
        CHECK(g->units.items[0].x == 3 && g->units.items[0].y == 6);
        CHECK(g->info_panel_right == (clean ? 0 : 1));
        CHECK(f.rng_calls > 0 && g->units.items[1].hp < 50);
        if (!clean) {
            normal_rng_calls = f.rng_calls; normal_target_hp = g->units.items[1].hp;
        } else {
            CHECK(f.rng_calls == normal_rng_calls && g->units.items[1].hp == normal_target_hp);
        }
        close_fixture(&f);
    }
    SDL_QuitSubSystem(SDL_INIT_EVENTS);
    return 0;
}

int main(void) {
    if (test_fixed_anchor_and_whole_round() ||
        test_unknown_handler_preserves_completed_actors() ||
        test_later_geometry_error_keeps_first_commit() ||
        test_no_path_still_acted_without_match1_or_recovery() ||
        test_exact_flags_side_and_hp0_gate() ||
        test_hp0_actor_completes_before_phase_failure() ||
        test_guards_and_host_quit_before_batch() ||
        test_real_start_march_geometry_and_phase_entry() ||
        test_batch_background_policy_does_not_leak()) return 1;
    puts("field_march_test: ok");
    return 0;
}
