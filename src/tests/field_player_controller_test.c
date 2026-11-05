/* B1.4c1b：field_player_unit_action @0x18890、field_controller_input
 * @0x117e7、sub_13565 @0x13565 的共享pending与异步phase续行。
 * 合成session只验证已确认的状态顺序，不代替原版WGC运行对照。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_game.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    return 1; } } while (0)

typedef struct {
    fd2_field_game game;
    uint32_t cells[64];
    fd2_terrain_attr attr;
    size_t rng_calls;
} fixture;

/* 显式core ACK保留本测试后置/错误顺序，不等同提示时钟或按键验收。 */
static int automatic_and_ack(fd2_field_game *game) {
    int result=fd2_field_game_process_automatic_action(game);
    if(result<0)return result;
    for(unsigned i=0;i<1024;i++) {
        fd2_field_progress_notice notice;
        int pending=fd2_field_game_progress_peek(game,&notice);
        if(pending<0)return -1;
        if(!pending) {
            for(size_t j=0;j<game->event_log_count;j++)game->event_log[j].presentation_deferred=0;
            pending=fd2_field_game_progress_resume(game);
            if(pending<0)return -1;
            if(!pending)return result;
        }
        if(fd2_field_game_progress_ack(game)!=0)return -1;
    }
    return -1;
}
static uint32_t rng(void *data) { (*(size_t *)data)++; return 0; }

static void unit(fd2_field_unit *u, uint8_t side, uint8_t x, uint8_t y) {
    memset(u, 0, sizeof(*u));
    u->side = side;
    u->x = x; u->y = y;
    u->hp = u->hp_max = 50;
    u->movement_profile = 1;
    u->movement_points = 3;
    u->ai_behavior_raw = 8; /* 原版确认的无common-tail返回。 */
    ((uint8_t *)u)[0x0a] = 0x40;
    ((uint8_t *)u)[0x0b] = 0;
}

static void init(fixture *f) {
    memset(f, 0, sizeof(*f));
    fd2_field_game *g = &f->game;
    memset(g->metadata.turn_events, 0xff, sizeof(g->metadata.turn_events));
    memset(g->metadata.cell_lookup, 0xff, sizeof(g->metadata.cell_lookup));
    g->ready = 1;
    g->turn_number = 1;
    g->active_side = 2;
    g->selected_unit = -1;
    g->command_selected = -1;
    g->detail_unit = -1;
    g->attack_target = -1;
    g->interaction = FD2_FIELD_INTERACTION_BROWSE;
    g->map.width = g->map.height = 8;
    g->map.cells = f->cells;
    g->terrain.attrs = &f->attr;
    g->terrain.attr_count = 1;
    f->attr.movement_cost_class = 0; /* profile1 class0成本1；class1成本20。 */
    g->units.count = 3;
    unit(&g->units.items[0], 2, 1, 1);
    unit(&g->units.items[1], 0, 6, 6);
    unit(&g->units.items[2], 2, 7, 7);
    g->cursor_cell_x = g->cursor_cell_y = 1;
    g->attack_rng = rng;
    g->attack_rng_userdata = &f->rng_calls;
}

static void close_fixture(fixture *f) {
    fd2_field_path_close(&f->game.move_range);
    free(f->game.move_path);
}

static int command_at(fixture *f, int x, int y) {
    fd2_field_game *g = &f->game;
    CHECK(fd2_field_game_confirm_cursor(g) == 0);
    g->cursor_cell_x = x; g->cursor_cell_y = y;
    CHECK(fd2_field_game_confirm_cursor(g) == 0);
    CHECK(fd2_field_game_execute_move(g, NULL, 0) == 0);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_COMMAND);
    return 0;
}

static int test_zero_movement_command_and_cancel(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    g->units.items[0].movement_points = 0;
    fd2_field_units before = g->units;
    CHECK(fd2_field_game_confirm_cursor(g) == 0);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED);
    size_t reachable = 0;
    for (int y = 0; y < g->map.height; y++)
        for (int x = 0; x < g->map.width; x++)
            reachable += fd2_field_path_is_destination(&g->move_range, x, y) != 0;
    CHECK(reachable == 1 && fd2_field_path_is_destination(&g->move_range, 1, 1));
    CHECK(fd2_field_game_move_cursor(g, 1, 0) == 1);
    CHECK(fd2_field_game_confirm_cursor(g) == -1 && !g->move_preview_valid);
    CHECK(g->units.items[0].x == 1 && g->units.items[0].y == 1);
    CHECK(fd2_field_game_move_cursor(g, -1, 0) == 1);
    CHECK(fd2_field_game_confirm_cursor(g) == 0);
    CHECK(fd2_field_game_execute_move(g, NULL, 0) == 0);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_COMMAND && g->move_path_length == 0);
    CHECK(fd2_field_game_cancel_selection(g) == 1);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED);
    CHECK(fd2_field_game_cancel_selection(g) == 1);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_BROWSE && g->selected_unit == -1);
    /* execute_move将walk_frames呈现缓存归静止帧1；80B记录/其余旁路均不变。 */
    before.walk_frames[0] = 1;
    CHECK(memcmp(&before, &g->units, sizeof(before)) == 0 && f.rng_calls == 0);
    CHECK(g->turn_number == 1 && g->active_side == 2 && !g->pending_cell_event.valid);
    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    g->units.items[0].movement_profile = UINT8_MAX;
    CHECK(fd2_field_game_compute_move_range(g, 0, &range) == -1 && !range.nodes);
    close_fixture(&f);
    return 0;
}

static int frozen(fixture *f) {
    fd2_field_game *g = &f->game;
    fd2_field_game before = *g;
    size_t calls = f->rng_calls;
    CHECK(fd2_field_game_finish_unit_action(g) == -1);
    CHECK(fd2_field_game_confirm_cursor(g) == -1);
    CHECK(fd2_field_game_cancel_selection(g) == 0);
    CHECK(automatic_and_ack(g) == -1);
    CHECK(fd2_field_game_resume_player_controller(g) == -1);
    CHECK(!memcmp(g, &before, sizeof(before)) && calls == f->rng_calls);
    return 0;
}

static int test_full_path_last_match_and_cancel(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    g->pending_cell_event = (fd2_field_pending_cell_event){1, 99, 9};
    f.cells[1 * 8 + 2] = 1u << 16;
    f.cells[1 * 8 + 3] = 2u << 16;
    g->metadata.cell_lookup[0] = (fd2_field_cell_lookup){7, 0};
    g->metadata.cell_lookup[1] = (fd2_field_cell_lookup){9, 0};
    CHECK(command_at(&f, 4, 1) == 0);
    CHECK(g->units.items[0].x == 4 && g->pending_cell_event.valid);
    CHECK(g->pending_cell_event.action == 9 && g->pending_cell_event.source_slot == 1);
    CHECK(g->event_log_count == 0 && !fd2_field_unit_has_acted(&g->units.items[0]));
    CHECK(g->units.items[0].direction == 0 && f.rng_calls == 0);
    /* 无效/禁用确认与下级攻击目标取消都保留移动及共享slot。 */
    g->command_selected = FD2_FIELD_COMMAND_MAGIC;
    fd2_field_game before = *g;
    CHECK(fd2_field_game_confirm_command(g) == -1);
    CHECK(!memcmp(g, &before, sizeof(before)));
    g->units.items[1].x = 5; g->units.items[1].y = 1;
    CHECK(fd2_field_game_begin_attack(g) == 0);
    CHECK(fd2_field_game_cancel_attack(g) == 1);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_COMMAND);
    CHECK(g->units.items[0].x == 4 && g->pending_cell_event.action == 9);
    CHECK(fd2_field_game_cancel_selection(g) == 1);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED);
    CHECK(g->units.items[0].x == 1 && g->cursor_cell_x == 1);
    CHECK(!g->pending_cell_event.valid && !g->player_post_waiting_phase);
    CHECK(fd2_field_game_cancel_selection(g) == 1);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_BROWSE && g->active_side == 2);
    CHECK(!fd2_field_unit_has_acted(&g->units.items[0]) && f.rng_calls == 0);
    close_fixture(&f);
    return 0;
}

static int test_final_lookup_after_acted_and_no_retry(void) {
    for (int final_match = 0; final_match < 2; final_match++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        f.cells[1 * 8 + 2] = 1u << 16;
        f.cells[1 * 8 + 3] = 2u << 16;
        g->metadata.cell_lookup[0] = (fd2_field_cell_lookup){7, 0};
        g->metadata.cell_lookup[1] = (fd2_field_cell_lookup){9, (uint8_t)(final_match ? 1 : 2)};
        CHECK(command_at(&f, 3, 1) == 0);
        CHECK(g->pending_cell_event.action == 7);
        CHECK(fd2_field_game_finish_unit_action(g) == -1);
        CHECK(g->failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
        CHECK(fd2_field_unit_has_acted(&g->units.items[0]));
        CHECK(g->units.items[0].x == 3 && g->selected_unit == -1);
        CHECK(g->failure_notice.action == (final_match ? 9 : 7));
        CHECK(g->failure_notice.slot == (final_match ? 1u : 0u));
        CHECK(g->failure_notice.x == 3 && g->failure_notice.unit_index == 0);
        CHECK(g->event_log_count == 1 && f.rng_calls == 0);
        CHECK(frozen(&f) == 0);
        close_fixture(&f);
    }
    return 0;
}

static int test_wait_recovery_and_special_cell_gate(void) {
    for (int which = 0; which < 6; which++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        g->units.items[0].hp = which == 1 ? 49 : 25;
        if (which == 2) g->units.items[0].detail_status[0] = 1;
        CHECK(command_at(&f, which == 3 ? 2 : 1, 1) == 0);
        if (which == 4) g->units.items[0].detail_status[1] = 1;
        if (which == 5) f.attr.flags = 0x20;
        uint16_t before = g->units.items[0].hp;
        if (which == 5) {
            g->pending_cell_event = (fd2_field_pending_cell_event){1, 7, 3};
            CHECK(fd2_field_game_finish_unit_action(g) == -1);
            CHECK(g->failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
            CHECK(g->failure_notice.action == UINT8_MAX);
            CHECK(!fd2_field_unit_has_acted(&g->units.items[0]));
            CHECK(g->units.items[0].hp == before && g->pending_cell_event.action == 7);
            CHECK(frozen(&f) == 0);
        } else {
            CHECK(fd2_field_game_finish_unit_action(g) == 0);
            CHECK(g->units.items[0].hp == (which == 0 ? 35 : which == 1 ? 50 : before));
            CHECK(fd2_field_unit_has_acted(&g->units.items[0]));
            CHECK(!g->pending_cell_event.valid && !g->player_post_waiting_phase);
        }
        CHECK(f.rng_calls == 0);
        close_fixture(&f);
    }
    return 0;
}

static int test_13565_gate_and_selection_cancel(void) {
    for (int which = 0; which < 5; which++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        fd2_field_unit *other = &g->units.items[2];
        g->units.items[0].detail_status[1] = 1; /* Enter详情来源原actor，不满足13565。 */
        if (which == 0) other->hp = 0; /* HP0且未hidden仍阻止phase。 */
        if (which == 1) other->flags = 4; /* bit04也阻止phase。 */
        if (which == 2) other->detail_status[1] = 1;
        if (which == 3) other->flags = 0x80;
        if (which == 4) other->flags = 1;
        CHECK(fd2_field_game_complete_player_controller(g, 0) == 0);
        CHECK(g->active_side == (which < 2 ? 2 : 1));
        CHECK(g->player_post_waiting_phase == (which >= 2));
        CHECK(!fd2_field_unit_has_acted(&g->units.items[0]));
        CHECK(f.rng_calls == 0);
        close_fixture(&f);
    }
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    CHECK(fd2_field_game_confirm_cursor(g) == 0);
    /* 选择后状态阻断：Esc返回仍做13565，且不acted、不lookup1。 */
    g->units.items[0].detail_status[1] = 1;
    g->units.items[2].detail_status[1] = 1;
    g->metadata.turn_events[0] = (fd2_field_turn_event){1, 99, 1};
    CHECK(fd2_field_game_cancel_selection(g) == 0);
    CHECK(g->failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
    CHECK(g->failure_notice.kind == FD2_FIELD_EVENT_TURN && g->failure_notice.action == 99);
    CHECK(!fd2_field_unit_has_acted(&g->units.items[0]));
    CHECK(g->player_post_waiting_phase && g->player_post_actor == 0);
    CHECK(frozen(&f) == 0);
    close_fixture(&f);
    return 0;
}

static int test_phase_uses_shared_slot_and_clears_continuation(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    g->units.items[2].flags = 0x80;
    CHECK(command_at(&f, 1, 1) == 0);
    g->pending_cell_event = (fd2_field_pending_cell_event){1, 99, 7};
    CHECK(fd2_field_game_finish_unit_action(g) == 0);
    CHECK(g->active_side == 1 && g->player_post_waiting_phase);
    CHECK(g->pending_cell_event.valid && g->pending_cell_event.action == 99);
    CHECK(fd2_field_game_resume_player_controller(g) == 1);
    fd2_field_game before = *g;
    fd2_save_battle_snapshot snapshot, original;
    memset(&snapshot, 0xa5, sizeof(snapshot)); original = snapshot;
    CHECK(fd2_field_game_export_battle_snapshot(g, &snapshot) == -1);
    CHECK(!memcmp(&snapshot, &original, sizeof(snapshot)));
    CHECK(fd2_field_game_confirm_cursor(g) == -1);
    CHECK(fd2_field_game_end_player_turn(g) == -1);
    CHECK(!memcmp(g, &before, sizeof(before)));
    /* side1无行动actor，扫描raw表清掉玩家pending；不保护为私有队列。 */
    CHECK(automatic_and_ack(g) == 1);
    CHECK(g->active_side == 0 && !g->pending_cell_event.valid);
    for (int guard = 0; g->active_side != 2 && guard < 8; guard++)
        CHECK(automatic_and_ack(g) == 1);
    CHECK(g->active_side == 2 && g->turn_number == 2);
    CHECK(!g->player_post_waiting_phase && !g->pending_cell_event.valid);
    CHECK(g->player_post_actor == 0 && g->event_log_count == 0 && f.rng_calls == 0);
    before = *g;
    CHECK(fd2_field_game_resume_player_controller(g) == 0);
    CHECK(!memcmp(g, &before, sizeof(before)));
    close_fixture(&f);
    return 0;
}

static int test_resume_reads_current_slot_original_actor_and_terminal(void) {
    for (int terminal = 0; terminal < 2; terminal++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        g->player_post_waiting_phase = 1;
        g->player_post_actor = 2;
        g->active_side = terminal ? 1 : 2;
        if (terminal) g->units.items[1].flags = 1;
        /* 模拟已有phase写入当前slot；没有可用的成功cell handler，故未知时冻结。 */
        g->pending_cell_event = (fd2_field_pending_cell_event){1, 23, 4};
        CHECK(fd2_field_game_resume_player_controller(g) == -1);
        CHECK(g->failure_notice.unit_index == 2 && g->failure_notice.action == 23);
        CHECK(g->failure_notice.slot == 4 && g->failure_notice.x == 7);
        CHECK(g->player_post_waiting_phase && g->pending_cell_event.valid);
        CHECK(g->event_log_count == 1 && f.rng_calls == 0);
        CHECK(frozen(&f) == 0);
        close_fixture(&f);
    }
    return 0;
}

static int test_phase_overwrites_player_slot_before_unknown_handler(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    g->units.items[2].flags = 0x80;
    g->units.count = 4;
    unit(&g->units.items[3], 0, 6, 7);
    g->units.items[3].flags = 4;
    g->units.items[1].side = 1;
    g->units.items[1].ai_behavior_raw = 2;
    g->units.items[1].movement_points = 0;
    f.cells[6 * 8 + 6] = 1u << 16;
    g->metadata.cell_lookup[0] = (fd2_field_cell_lookup){23, 1};
    CHECK(command_at(&f, 1, 1) == 0);
    g->pending_cell_event = (fd2_field_pending_cell_event){1, 99, 7};
    CHECK(fd2_field_game_finish_unit_action(g) == 0);
    CHECK(g->player_post_waiting_phase && g->pending_cell_event.action == 99);
    CHECK(automatic_and_ack(g) == -1);
    CHECK(g->failure_notice.kind == FD2_FIELD_EVENT_CELL);
    CHECK(g->failure_notice.unit_index == 1 && g->failure_notice.action == 23);
    CHECK(g->pending_cell_event.action == 23 && g->pending_cell_event.source_slot == 0);
    CHECK(g->player_post_waiting_phase && g->player_post_actor == 0);
    CHECK(fd2_field_unit_has_acted(&g->units.items[0]));
    CHECK(fd2_field_unit_has_acted(&g->units.items[1]));
    CHECK(g->event_log_count == 1 && f.rng_calls == 0);
    CHECK(frozen(&f) == 0);
    close_fixture(&f);
    return 0;
}

static int test_phase0_clear_precedes_event_and_status_failure(void) {
    for (int status_failure = 0; status_failure < 2; status_failure++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        g->active_side = 1;
        g->phase_unit_cursor = g->units.count; /* side1实际actor已返回，只余raw尾。 */
        for (size_t i = 0; i < g->units.count; i++) g->units.items[i].flags = 0x80;
        g->pending_cell_event = (fd2_field_pending_cell_event){1, 77, 4};
        if (!status_failure) {
            g->metadata.turn_events[0] = (fd2_field_turn_event){1, 99, 0};
            g->units.items[1].detail_status[0] = 2;
        } else {
            g->units.items[1].attack_status = 1;
            ((uint8_t *)&g->units.items[1])[0x0b] = 255; /* 派生重算必须拒绝。 */
        }
        CHECK(automatic_and_ack(g) == -1);
        CHECK(g->active_side == 0);
        for (size_t i = 0; i < g->units.count; i++)
            CHECK(!fd2_field_unit_has_acted(&g->units.items[i]));
        if (!status_failure) {
            CHECK(g->failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
            CHECK(g->failure_notice.kind == FD2_FIELD_EVENT_TURN);
            CHECK(g->units.items[1].detail_status[0] == 2);
        } else {
            CHECK(g->failure == FD2_FIELD_FAILURE_ACTION);
            CHECK(g->failure_notice.unit_index == 1);
            CHECK(g->units.items[1].attack_status == 0);
        }
        /* clear acted不清pending；phase0首raw actor尚未开始就已失败。 */
        CHECK(g->pending_cell_event.valid && g->pending_cell_event.action == 77);
        CHECK(frozen(&f) == 0);
        close_fixture(&f);
    }
    return 0;
}

int main(void) {
    if (test_zero_movement_command_and_cancel() ||
        test_full_path_last_match_and_cancel() ||
        test_final_lookup_after_acted_and_no_retry() ||
        test_wait_recovery_and_special_cell_gate() ||
        test_13565_gate_and_selection_cancel() ||
        test_phase_uses_shared_slot_and_clears_continuation() ||
        test_phase_overwrites_player_slot_before_unknown_handler() ||
        test_phase0_clear_precedes_event_and_status_failure() ||
        test_resume_reads_current_slot_original_actor_and_terminal()) return 1;
    puts("field_player_controller_test: ok");
    return 0;
}
