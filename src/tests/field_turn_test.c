/* B1 玩家结束回合回归：复现 field_turn_cycle_run @0x1a30b
 * （原登记 FUN_00017a53）首部恢复门，以及
 * field_empty_focus_menu_execute @0x16f55 selector 3 的直接调用。
 * 合成 session 验证 SDL 逻辑与停止边界，不替代原版运行对照。
 */
#include <stdio.h>
#include <string.h>

#include "field_game.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

/* 本逻辑测试显式完成progress ACK，不模拟真实提示、按键或等待RNG。 */
static int ack_progress(fd2_field_game *game) {
    for (unsigned i=0;i<1024;i++) {
        fd2_field_progress_notice notice;
        int state=fd2_field_game_progress_peek(game,&notice);
        if(state<0)return -1;
        if(!state) {
            for(size_t j=0;j<game->event_log_count;j++)game->event_log[j].presentation_deferred=0;
            state=fd2_field_game_progress_resume(game);
            if(state<0)return -1;
            if(!state)return 0;
            if(fd2_field_game_progress_peek(game,&notice)!=1)return -1;
        }
        if(fd2_field_game_progress_ack(game)!=0)return -1;
    }
    return -1;
}
static int end_player_turn_and_ack(fd2_field_game *game) {
    int result=fd2_field_game_end_player_turn(game);
    return result<0?result:ack_progress(game)<0?-1:result;
}
static int end_active_phase_and_ack(fd2_field_game *game) {
    int result=fd2_field_game_end_active_phase(game);
    return result<0?result:ack_progress(game)<0?-1:result;
}
static int process_automatic_action_and_ack(fd2_field_game *game) {
    int result=fd2_field_game_process_automatic_action(game);
    return result<0?result:ack_progress(game)<0?-1:result;
}
static uint32_t next_roll(void *userdata) {
    size_t *calls = userdata;
    (*calls)++;
    return 0;
}

static void init_game(fd2_field_game *game, size_t *rng_calls) {
    memset(game, 0, sizeof(*game));
    memset(game->metadata.turn_events, 0xff,
           sizeof(game->metadata.turn_events));
    game->ready = 1;
    game->map.width = 24;
    game->map.height = 24;
    game->turn_number = 1;
    game->active_side = 2;
    game->selected_unit = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->units.count = 13;
    for (size_t i = 0; i < game->units.count; i++) {
        fd2_field_unit *unit = &game->units.items[i];
        unit->side = 2;
        unit->hp = unit->hp_max = 50;
        unit->x = (uint8_t)(i % 8u);
        unit->y = (uint8_t)(i / 8u);
    }
    game->units.items[11].side = 1;
    game->units.items[11].hp = 30;
    game->units.items[11].detail_status[0] = 2;
    game->units.items[12].side = 0;
    game->units.items[12].hp = 30;
    game->attack_rng = next_roll;
    game->attack_rng_userdata = rng_calls;
}

static int reject_without_changes(fd2_field_game *game, size_t *rng_calls) {
    fd2_field_game before = *game;
    size_t calls = *rng_calls;
    CHECK(end_player_turn_and_ack(game) == -1);
    CHECK(memcmp(game, &before, sizeof(before)) == 0);
    CHECK(*rng_calls == calls);
    return 0;
}

static int test_recovery_gates_and_phase_order(void) {
    fd2_field_game game;
    size_t rng_calls = 0;
    init_game(&game, &rng_calls);
    fd2_field_unit *units = game.units.items;
    units[0].hp = 35;
    units[1].hp = 49;
    units[2].hp = 25;
    units[2].flags = 0xa0; /* acted 与其他位保持。 */
    units[3].hp = 25;
    units[3].flags = 0x11; /* hidden 与其他位保持。 */
    units[4].hp = 25;
    units[4].detail_status[0] = 1;
    units[5].hp = 25;
    units[5].detail_status[1] = 1;
    units[7].hp = 3;
    units[7].hp_max = 4;
    units[8].hp = 20;
    units[8].flags = 0x04; /* 恢复门不检查 AI eligibility bit。 */
    units[9].hp = 10;
    units[9].attack_status = 2;
    units[9].defense_status = 3;
    units[9].hit_evasion_status = 4;
    units[10].hp = 60; /* 原版门为 !=，不是 <；合成超上限输入。 */

    CHECK(end_player_turn_and_ack(&game) == 0);
    CHECK(game.active_side == 1 && game.turn_number == 1);
    CHECK(units[0].hp == 45 && units[0].flags == 0x80);
    CHECK(units[1].hp == 50 && units[1].flags == 0x80);
    CHECK(units[2].hp == 25 && units[2].flags == 0xa0);
    CHECK(units[3].hp == 25 && units[3].flags == 0x11);
    CHECK(units[4].hp == 25 && units[4].flags == 0);
    CHECK(units[4].detail_status[0] == 1);
    CHECK(units[5].hp == 25 && units[5].flags == 0);
    CHECK(units[5].detail_status[1] == 1);
    CHECK(units[6].hp == 50 && units[6].flags == 0);
    CHECK(units[7].hp == 3 && units[7].flags == 0x80);
    CHECK(units[8].hp == 30 && units[8].flags == 0x84);
    CHECK(units[9].hp == 20 && units[9].flags == 0x80);
    CHECK(units[9].attack_status == 2 && units[9].defense_status == 3 &&
          units[9].hit_evasion_status == 4);
    CHECK(units[10].hp == 50 && units[10].flags == 0x80);
    CHECK(units[11].hp == 25 && units[11].detail_status[0] == 1);
    CHECK(units[12].hp == 30 && units[12].flags == 0);
    CHECK(game.event_log_count == 0 && rng_calls == 0);
    return reject_without_changes(&game, &rng_calls);
}

static int test_player_turn_guards(void) {
    fd2_field_game game;
    size_t rng_calls = 0;
    CHECK(end_player_turn_and_ack(NULL) == -1);
    init_game(&game, &rng_calls);
    game.ready = 0;
    CHECK(reject_without_changes(&game, &rng_calls) == 0);
    game.ready = 1;
    game.failure = FD2_FIELD_FAILURE_EVENT_UNSUPPORTED;
    CHECK(reject_without_changes(&game, &rng_calls) == 0);
    game.failure = FD2_FIELD_FAILURE_NONE;
    game.active_side = 0;
    CHECK(reject_without_changes(&game, &rng_calls) == 0);
    game.active_side = 2;
    game.selected_unit = 0;
    CHECK(reject_without_changes(&game, &rng_calls) == 0);
    game.selected_unit = -1;
    game.interaction = FD2_FIELD_INTERACTION_SYSTEM_MENU;
    return reject_without_changes(&game, &rng_calls);
}

static int test_unknown_event_preserves_recovery_and_stops_status(void) {
    fd2_field_game game;
    size_t rng_calls = 0;
    init_game(&game, &rng_calls);
    game.units.items[0].hp = 35;
    game.metadata.turn_events[0] = (fd2_field_turn_event){
        .turn = 1, .action = 0xff, .phase = 1};
    game.metadata.turn_events[1] = (fd2_field_turn_event){
        .turn = 1, .action = 0, .phase = 1};
    CHECK(end_player_turn_and_ack(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
    CHECK(game.active_side == 1 && game.turn_number == 1);
    CHECK(game.units.items[0].hp == 45 &&
          fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.failure_notice.kind == FD2_FIELD_EVENT_TURN &&
          game.failure_notice.action == 0xff && game.failure_notice.slot == 0);
    CHECK(game.event_log_count == 1 && !game.event_log[0].handled);
    CHECK(game.units.count == 13 && game.units.items[11].hp == 30 &&
          game.units.items[11].detail_status[0] == 2);
    CHECK(rng_calls == 0);
    return reject_without_changes(&game, &rng_calls);
}

static int test_capacity_stops_before_handler(void) {
    fd2_field_game game;
    size_t rng_calls = 0;
    init_game(&game, &rng_calls);
    game.units.items[0].hp = 35;
    game.event_log_count = FD2_FIELD_EVENT_LOG_CAPACITY;
    game.metadata.turn_events[0] = (fd2_field_turn_event){
        .turn = 1, .action = 0, .phase = 1};
    CHECK(end_player_turn_and_ack(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_CAPACITY);
    CHECK(game.units.items[0].hp == 45 &&
          fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.event_log_count == FD2_FIELD_EVENT_LOG_CAPACITY);
    CHECK(game.event_total_count == 0 && game.units.count == 13);
    CHECK(game.units.items[11].hp == 30 &&
          game.units.items[11].detail_status[0] == 2);
    CHECK(rng_calls == 0);
    return reject_without_changes(&game, &rng_calls);
}

static int test_fast_skip_is_separate(void) {
    fd2_field_game game;
    size_t rng_calls = 0;
    init_game(&game, &rng_calls);
    game.units.items[0].hp = 35;
    CHECK(end_active_phase_and_ack(&game) == 0);
    CHECK(game.active_side == 1 && game.units.items[0].hp == 35);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(fd2_field_unit_has_acted(&game.units.items[6]));
    CHECK(rng_calls == 0);
    return 0;
}

static int test_new_round_restores_leader_focus_and_cycle(void) {
    fd2_field_game game;
    size_t rng_calls = 0;
    init_game(&game, &rng_calls);
    game.map.width = game.map.height = 24;
    game.active_side = 0;
    game.units.items[0].x = 7;
    game.units.items[0].y = 14;
    game.cursor_cell_x = game.cursor_cell_y = 23;
    game.camera_cell_x = 11;
    game.camera_cell_y = 16;
    game.focus_cycle_unit = 3;
    game.detail_acknowledged_unit = 4;
    CHECK(end_active_phase_and_ack(&game) == 0);
    CHECK(game.turn_number == 2 && game.active_side == 2);
    CHECK(game.cursor_cell_x == 7 && game.cursor_cell_y == 14);
    CHECK(game.camera_cell_x == 6 && game.camera_cell_y == 13);
    CHECK(game.focus_cycle_unit == 0 && game.detail_acknowledged_unit == -1);
    CHECK(fd2_field_game_cycle_focus(&game) == 0);
    CHECK(game.focus_cycle_unit == 1 && rng_calls == 0);

    /* 地图边缘不能为保持阈值而卷出边界。 */
    game.active_side = 0;
    game.units.items[0].x = game.units.items[0].y = 0;
    CHECK(end_active_phase_and_ack(&game) == 0);
    CHECK(game.cursor_cell_x == 0 && game.cursor_cell_y == 0);
    CHECK(game.camera_cell_x == 0 && game.camera_cell_y == 0);

    game.active_side = 0;
    game.cursor_cell_x = game.cursor_cell_y = 10;
    game.focus_cycle_unit = 3;
    game.metadata.turn_events[0] = (fd2_field_turn_event){
        .turn = 4, .action = 0xff, .phase = 2};
    CHECK(end_active_phase_and_ack(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
    CHECK(game.cursor_cell_x == 10 && game.cursor_cell_y == 10);
    CHECK(game.focus_cycle_unit == 0);
    CHECK(rng_calls == 0);
    return 0;
}

static void init_status_focus(fd2_field_game *game, size_t *rng_calls) {
    init_game(game, rng_calls);
    game->units.count = 3;
    memset(game->units.items, 0, sizeof(game->units.items));
    for (size_t i = 0; i < game->units.count; i++) {
        fd2_field_unit *unit = &game->units.items[i];
        unit->side = i == 0 ? 2u : i == 1 ? 1u : 0u;
        unit->unit_id = i == 0 ? 0u : 96u;
        unit->x = unit->y = i == 0 ? 2u : 20u;
        unit->hp = unit->hp_max = 50;
        unit->movement_profile = unit->level = 1;
        for (size_t slot = 0; slot < 8; slot++)
            unit->data_09_1e[1u + 2u * slot] = 0x80;
    }
    /* 不执行AI common tail，才能观察毒伤死亡方向是否真实保留到下一回合。 */
    game->units.items[2].flags = FD2_FIELD_UNIT_FLAG_AI_INELIGIBLE;
}

static int test_poison_death_focus_survives_round_and_export(void) {
    fd2_field_game game;
    size_t calls = 0;
    init_status_focus(&game, &calls);
    fd2_field_unit *poisoned = &game.units.items[1];
    poisoned->hp = 1; poisoned->hp_max = 10;
    poisoned->direction = 3; poisoned->detail_status[0] = 1;
    CHECK(end_player_turn_and_ack(&game) == 0);
    /* 右／下检查移动前相对格，最终保留相对(11,6)。 */
    CHECK(game.camera_cell_x == 9 && game.camera_cell_y == 14);
    CHECK(game.cursor_cell_x == 20 && game.cursor_cell_y == 20);
    CHECK(poisoned->hp == 0 && poisoned->flags == 1 && poisoned->direction == 0);
    CHECK(poisoned->detail_status[0] == 1); /* finalize先隐藏，死者不再递减计时。 */
    CHECK(process_automatic_action_and_ack(&game) == 1 && game.active_side == 0);
    CHECK(process_automatic_action_and_ack(&game) == 1 && game.active_side == 2);
    CHECK(game.turn_number == 2 && game.cursor_cell_x == 2 && game.cursor_cell_y == 2);
    CHECK(game.camera_cell_x == 1 && game.camera_cell_y == 1 && poisoned->direction == 0);
    fd2_save_battle_snapshot snapshot = {0};
    CHECK(fd2_field_game_export_battle_snapshot(&game, &snapshot) == 0);
    fd2_field_unit saved;
    memcpy(&saved, snapshot.unit_area + FD2_FIELD_UNIT_RECORD_SIZE, sizeof(saved));
    CHECK(saved.hp == 0 && saved.flags == 1 && saved.direction == 0);
    CHECK(snapshot.meta[FD2_SAVE_BATTLE_META_CAMERA_X] == 1 &&
        snapshot.meta[FD2_SAVE_BATTLE_META_CAMERA_Y] == 1 && calls == 0);
    return 0;
}

static int test_poison_focus_uses_raw_order_even_for_zero_hp_or_damage(void) {
    fd2_field_game game;
    size_t calls = 0;
    init_status_focus(&game, &calls);
    game.units.count = 7;
    fd2_field_unit *units = game.units.items;
    for (size_t i = 1; i < game.units.count; i++) {
        units[i].side = 1;
        units[i].hp = units[i].hp_max = 50;
        units[i].detail_status[0] = 2;
    }
    units[1].hp = 5; units[1].hp_max = 9; /* damage0仍需先聚焦(20,20)。 */
    units[2].x = units[2].y = 3;
    units[2].hp = 0; units[2].direction = 3; /* HP0仍聚焦，改变后续镜头路径。 */
    units[3].x = 22; units[3].y = 5; units[3].hp = 20; units[3].hp_max = 100;
    units[4].x = 1; units[4].y = 22; units[4].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    units[5].x = 1; units[5].y = 23; units[5].side = 0;
    units[6].detail_status[0] = 0; units[6].attack_status = 2;
    CHECK(end_player_turn_and_ack(&game) == 0);
    CHECK(game.cursor_cell_x == 22 && game.cursor_cell_y == 5);
    CHECK(game.camera_cell_x == 11 && game.camera_cell_y == 2);
    CHECK(units[1].hp == 5 && units[1].detail_status[0] == 1);
    /* 最后聚焦存活raw3后，raw2不在扩视区：不能无条件把死者方向改0。 */
    CHECK(units[2].flags == 1 && units[2].direction == 3 && units[2].detail_status[0] == 2);
    CHECK(units[3].hp == 10 && units[3].detail_status[0] == 1);
    CHECK(units[4].hp == 50 && units[4].detail_status[0] == 2);
    CHECK(units[5].hp == 50 && units[5].detail_status[0] == 2);
    CHECK(units[6].hp == 50 && units[6].attack_status == 1 && calls == 0);
    return 0;
}

static int test_status_expiration_focus_and_failure_order(void) {
    for (int broken_stats = 0; broken_stats < 2; broken_stats++) {
        fd2_field_game game;
        size_t calls = 0;
        init_status_focus(&game, &calls);
        game.units.count = 5;
        fd2_field_unit *poisoned = &game.units.items[1];
        poisoned->hp = 50; poisoned->hp_max = 100; poisoned->detail_status[0] = 2;
        fd2_field_unit *expired = &game.units.items[3];
        expired->side = 1; expired->hp = expired->hp_max = 50;
        expired->x = expired->y = 3;
        expired->attack_status = expired->defense_status = 1;
        expired->base_attack_le[0] = 10; expired->base_defense_le[0] = 20;
        expired->dexterity_le[0] = 30;
        for (size_t slot = 0; slot < 8; slot++)
            expired->data_09_1e[1u + 2u * slot] = 0x80;
        game.units.items[4].side = 1;
        game.units.items[4].hp = game.units.items[4].hp_max = 50;
        game.units.items[4].attack_status = 2;
        if (broken_stats) {
            expired->data_09_1e[1] = 0x40;
            expired->data_09_1e[2] = 255;
        }
        CHECK(end_player_turn_and_ack(&game) == (broken_stats ? -1 : 0));
        CHECK(game.cursor_cell_x == 3 && game.cursor_cell_y == 3);
        CHECK(game.camera_cell_x == 2 && game.camera_cell_y == 2);
        CHECK(poisoned->hp == 40 && poisoned->detail_status[0] == 1);
        CHECK(expired->attack_status == 0 && expired->defense_status == (broken_stats ? 1 : 0));
        CHECK(game.units.items[4].attack_status == (broken_stats ? 2 : 1));
        if (broken_stats) {
            CHECK(game.failure == FD2_FIELD_FAILURE_ACTION && game.failure_notice.unit_index == 3);
            CHECK(reject_without_changes(&game, &calls) == 0);
        } else {
            CHECK(expired->attack == 10 && expired->defense == 20 &&
                expired->accuracy == 30 && expired->evasion == 30);
        }
        CHECK(calls == 0);
    }
    return 0;
}

static int test_poison_bad_focus_stops_after_hp_write(void) {
    fd2_field_game game;
    size_t calls = 0;
    init_status_focus(&game, &calls);
    game.units.items[1].x = 255;
    game.units.items[1].hp = 1; game.units.items[1].hp_max = 10;
    game.units.items[1].direction = 3; game.units.items[1].detail_status[0] = 2;
    game.units.items[2].side = 1; game.units.items[2].detail_status[0] = 2;
    CHECK(end_player_turn_and_ack(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_ACTION && game.failure_notice.unit_index == 1);
    CHECK(game.units.items[1].hp == 0 && game.units.items[1].flags == 0 &&
        game.units.items[1].direction == 3 && game.units.items[1].detail_status[0] == 2);
    CHECK(game.units.items[2].hp == 50 && game.units.items[2].detail_status[0] == 2);
    CHECK(game.cursor_cell_x == 0 && game.cursor_cell_y == 0 &&
        game.camera_cell_x == 0 && game.camera_cell_y == 0 && calls == 0);
    return reject_without_changes(&game, &calls);
}

int main(void) {
    if (test_recovery_gates_and_phase_order() ||
        test_player_turn_guards() ||
        test_unknown_event_preserves_recovery_and_stops_status() ||
        test_capacity_stops_before_handler() ||
        test_fast_skip_is_separate() ||
        test_new_round_restores_leader_focus_and_cycle() ||
        test_poison_death_focus_survives_round_and_export() ||
        test_poison_focus_uses_raw_order_even_for_zero_hp_or_damage() ||
        test_status_expiration_focus_and_failure_order() ||
        test_poison_bad_focus_stops_after_hp_write())
        return 1;
    puts("field_turn_test: ok");
    return 0;
}
