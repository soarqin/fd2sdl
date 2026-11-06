/* B2基础玩家法术：field_player_magic_command_execute @0x1cff0、
 * field_player_command_execute @0x18d8c及sub_1E292 @0x1e292。
 * 仅测试逻辑提交/拒绝/首错冻结；FIGANI与奖励音画仍待原版配对。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "figani.h"

#define CHECK(e) do { if (!(e)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #e); \
    return 1; } } while (0)

typedef struct {
    fd2_field_game game;
    fd2_archive fdother;
    uint32_t cells[24 * 24];
    fd2_terrain_attr attr;
    uint32_t rolls[32];
    size_t calls;
    uint16_t observed_mp[32], first_target_hp[32];
    uint8_t observed_acted[32];
    int corrupt_cell;
} fixture;

static uint32_t rng(void *data) {
    fixture *f = data;
    size_t i = f->calls++;
    if (i < 32) {
        f->observed_mp[i] = f->game.units.items[0].mp;
        f->observed_acted[i] = (uint8_t)fd2_field_unit_has_acted(&f->game.units.items[0]);
        f->first_target_hp[i] = f->game.units.items[1].hp;
    }
    if (i == 0 && f->corrupt_cell) f->cells[6 * 24 + 6] = 1;
    return f->rolls[i % 32];
}

static void unit(fd2_field_unit *u, uint8_t id, uint8_t side, uint8_t x, uint8_t y) {
    memset(u, 0, sizeof(*u));
    u->unit_id = u->text_id = id; u->side = side; u->x = x; u->y = y;
    u->level = 1; u->movement_profile = 5;
    u->hp = u->hp_max = 28; u->mp = u->mp_max = 8;
    u->direction = 2;
    ((uint8_t *)u)[0x28] = 0xa7; /* 无关字节不得重建。 */
    ((uint8_t *)u)[0x31] = 0xff;
}

static void init(fixture *f) {
    memset(f, 0, sizeof(*f));
    fd2_field_game *g = &f->game;
    if (fd2_archive_open(&f->fdother, "original_game/FDOTHER.DAT") != 0 ||
        fd2_archive_open(&g->figani_archive, "original_game/FIGANI.DAT") != 0) {
        fputs("magic fixture: required FIGANI.DAT unavailable\n", stderr);
        exit(EXIT_FAILURE);
    }
    g->fdother = &f->fdother;
    g->ready = 1; g->active_side = 2; g->turn_number = 7;
    g->selected_unit = 0; g->detail_unit = -1; g->attack_target = -1;
    g->interaction = FD2_FIELD_INTERACTION_COMMAND;
    g->cursor_cell_x = g->cursor_cell_y = 6;
    g->focus_cycle_unit = 3;
    g->map.width = g->map.height = 24; g->map.cells = f->cells;
    g->terrain.attrs = &f->attr; g->terrain.attr_count = 1;
    g->sprites.frame_count = 140u * 12u;
    g->units.count = 4;
    unit(&g->units.items[0], 9, 2, 6, 6);
    g->units.items[0].hp = 20; g->units.items[0].direction = 3;
    g->units.items[0].data_09_1e[0x11] = 1;
    g->units.items[0].data_09_1e[0x12] = 0x20;
    unit(&g->units.items[1], 96, 0, 5, 6);
    g->units.items[1].level = 2; g->units.items[1].movement_profile = 7;
    unit(&g->units.items[2], 0, 2, 8, 6);
    g->units.items[2].hp = g->units.items[2].hp_max = 42;
    g->units.items[2].movement_profile = 1; g->units.items[2].direction = 1;
    unit(&g->units.items[3], 96, 0, 15, 15);
    memset(g->metadata.turn_events, 0xff, sizeof(g->metadata.turn_events));
    memset(g->metadata.cell_lookup, 0xff, sizeof(g->metadata.cell_lookup));
    g->attack_rng = rng; g->attack_rng_userdata = f;
}

static int target(fixture *f, uint8_t id) {
    CHECK(fd2_field_game_begin_magic_list(&f->game) == 0);
    f->game.magic_list_selected = id == 13 ? 1 : 0;
    CHECK(fd2_field_game_begin_magic_targeting(&f->game, id) == 0);
    return 0;
}

static void close_fixture(fixture *f) {
    fd2_archive_close(&f->fdother);
    fd2_archive_close(&f->game.figani_archive);
    fd2_field_metadata_close(&f->game.metadata);
    fd2_field_path_close(&f->game.move_range);
    free(f->game.move_path);
}

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
static int resolve_and_ack(fd2_field_game *game) {
    int result=fd2_field_game_resolve_player_magic(game);
    return result==0?ack_progress(game):result;
}
static int reject_growth(const fd2_field_game *game,const fd2_field_progress_notice *notice,void *unused) {
    (void)game;(void)unused;return notice->fragment==489?-1:0;
}
static int unchanged_rejection(fixture *f) {
    fd2_field_game before = f->game;
    size_t calls = f->calls;
    CHECK(resolve_and_ack(&f->game) == -1);
    if (f->game.failure == FD2_FIELD_FAILURE_ACTION && before.progress_preflight) {
        before.failure=f->game.failure;before.failure_notice=f->game.failure_notice;
        before.phase_next_action_ms=f->game.phase_next_action_ms;
    }
    CHECK(!memcmp(&before, &f->game, sizeof(before)) && f->calls == calls);
    return 0;
}

static int test_capability_gate_does_not_use_mp(void) {
    fixture f; init(&f);
    CHECK(fd2_field_game_magic_is_available(&f.game) == 1);
    f.game.units.items[0].mp = 0;
    CHECK(fd2_field_game_magic_is_available(&f.game) == 1);
    f.game.attack_rng = NULL;
    fd2_field_game before = f.game;
    CHECK(!fd2_field_game_magic_is_available(&f.game));
    CHECK(!memcmp(&before, &f.game, sizeof(before)) && !f.calls);
    f.game.attack_rng = rng;
    f.game.units.items[0].data_09_1e[0x11] |= 2; /* 已知但本阶段未支持ID1。 */
    CHECK(fd2_field_game_magic_is_available(&f.game)); /* 已知未支持项不封禁0/13。 */
    f.game.units.items[0].data_09_1e[0x11] = 0;
    f.game.units.items[0].unit_id = 18; f.game.move_path_length = 2;
    CHECK(fd2_field_game_magic_is_available(&f.game) == 1); /* 仅13、移动例外。 */
    f.game.units.items[0].data_09_1e[0x11] = 1;
    CHECK(fd2_field_game_magic_is_available(&f.game)); /* ID13仍可用，ID0在逐项确认门拒绝。 */
    CHECK(!fd2_field_game_magic_is_available(NULL) && !f.calls);
    close_fixture(&f); return 0;
}

static int test_damage_kill_miss_and_nonlethal_reward(void) {
    for (int which = 0; which < 3; which++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        if (which == 1) f.rolls[0] = 99;
        if (which == 2) {
            g->units.items[1].hp = g->units.items[1].hp_max = 100;
            g->units.items[0].experience = 79;
        }
        g->units.items[0].detail_status[0] = 2;
        f.attr.flags = 0x20; /* MAGIC不执行WAIT专属cell-action，也不原地回血。 */
        CHECK(target(&f, 0) == 0);
        fd2_field_units expected = g->units;
        expected.items[0].mp = 6;
        expected.items[0].flags |= FD2_FIELD_UNIT_FLAG_ACTED;
        expected.items[0].experience = which == 0 ? 42 : which == 1 ? 0 : 97;
        if (which == 0) {
            expected.items[1].hp = 0;
            expected.items[1].flags = 1;
            expected.items[1].direction = 0;
        } else if (which == 2) expected.items[1].hp = 55;
        CHECK(resolve_and_ack(g) == 0);
        CHECK(!memcmp(&expected, &g->units, sizeof(expected)));
        CHECK(f.calls == (which == 1 ? 1u : 9u) && f.observed_mp[0] == 6 && !f.observed_acted[0]);
        CHECK(g->interaction == FD2_FIELD_INTERACTION_BROWSE && g->selected_unit == -1);
        CHECK(g->active_side == 2 && g->turn_number == 7 && g->cursor_cell_x == 5 && g->cursor_cell_y == 6);
        CHECK(!g->move_range.nodes && !g->magic_target_count && !g->event_log_count);
        CHECK(unchanged_rejection(&f) == 0); /* 重复确认不能再扣费/奖励。 */
        close_fixture(&f);
    }
    return 0;
}

static int test_same_cell_targets_are_raw_order_and_cost_once(void) {
    for (int mixed = 0; mixed < 2; mixed++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        g->units.count = 5;
        unit(&g->units.items[4], 96, 0, 5, 6);
        g->units.items[4].level = 2; g->units.items[4].movement_profile = 7;
        if (mixed) f.rolls[0] = 99; /* raw1 miss，raw4 hit。 */
        CHECK(target(&f, 0) == 0);
        CHECK(resolve_and_ack(g) == 0);
        CHECK(g->units.items[0].mp == 6 && g->units.items[0].experience == (mixed ? 42 : 84));
        CHECK(g->units.items[1].hp == (mixed ? 28 : 0) && !g->units.items[4].hp);
        CHECK(f.calls == (mixed ? 10u : 18u));
        for (size_t i = 0; i < f.calls; i++) CHECK(f.observed_mp[i] == 6 && !f.observed_acted[i]);
        CHECK(mixed || f.first_target_hp[2] == 24); /* 原full首次impact先写28-28/7，再jitter。 */
        close_fixture(&f);
    }
    return 0;
}

static int test_self_heal_and_full_hp_still_charge_and_act(void) {
    for (int full = 0; full < 2; full++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        if (full) g->units.items[0].hp = g->units.items[0].hp_max;
        CHECK(target(&f, 13) == 0);
        CHECK(resolve_and_ack(g) == 0);
        CHECK(g->units.items[0].hp == 28 && g->units.items[0].mp == 5);
        CHECK(g->units.items[0].experience == (full ? 0 : 11));
        CHECK(fd2_field_unit_has_acted(&g->units.items[0]) && g->units.items[0].direction == 3);
        CHECK(f.calls == 1 && f.observed_mp[0] == 5 && !f.observed_acted[0]);
        close_fixture(&f);
    }
    return 0;
}

static int test_reward_cap_and_last_player_controller(void) {
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    g->units.count = 5;
    g->units.items[3].x = 5; g->units.items[3].y = 6;
    g->units.items[3].level = 2;
    unit(&g->units.items[4], 96, 0, 5, 6);
    g->units.items[4].level = 2;
    CHECK(target(&f, 0) == 0);
    CHECK(resolve_and_ack(g) == 0);
    CHECK(g->units.items[0].experience == 99 && g->units.items[0].mp == 6 && f.calls == 27);
    close_fixture(&f);
    init(&f); g = &f.game;
    g->units.items[2].flags = FD2_FIELD_UNIT_FLAG_ACTED;
    CHECK(target(&f, 0) == 0);
    CHECK(resolve_and_ack(g) == 0);
    CHECK(g->active_side == 1 && g->player_post_waiting_phase && g->player_post_actor == 0);
    CHECK(g->units.items[0].experience == 42 && g->units.items[0].mp == 6 && f.calls == 9);
    CHECK(fd2_field_game_resume_player_controller(g) == 1);
    CHECK(g->units.items[0].experience == 42 && f.calls == 9);
    CHECK(unchanged_rejection(&f) == 0);
    close_fixture(&f); return 0;
}

static int test_all_preflight_rejections_are_before_mp_and_rng(void) {
    for (unsigned which = 0; which < 17; which++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        CHECK(target(&f, 0) == 0);
        switch (which) {
            case 0: g->units.items[0].mp = 1; break;
            case 1: g->units.items[0].unit_id = 18; break;
            case 2: g->units.items[0].experience = 58; g->progress_preflight=reject_growth; break; /* 可能升级的资源先拒绝。 */
            case 3: g->units.items[1].unit_id = 136; break;
            case 4: g->units.items[1].movement_profile = 0; break;
            case 5: g->units.items[1].hp_max = 0; break;
            case 6: ((uint8_t *)&g->units.items[1])[0x31] = 4; break;
            case 7: g->units.items[0].data_09_1e[0x15] |= 0x10; break; /* 未知bit36。 */
            case 8: g->attack_rng = NULL; break;
            case 9:
                g->cursor_cell_x = g->units.items[1].x = 23;
                g->move_range.nodes[6 * 24 + 23].distance = 0;
                g->move_range.nodes[6 * 24 + 23].can_stop = 1;
                CHECK(fd2_field_game_magic_target_is_legal(g) == 1); /* 旧mask已被破坏。 */
                break; /* 提交必须重建当前geometry，不能信旧mask。 */
            case 10: g->units.items[1].x = 255; break;
            case 11: g->pending_cell_event = (fd2_field_pending_cell_event){1,9,0}; break;
            case 12:
                f.cells[6 * 24 + 6] = 1u << 16;
                g->metadata.cell_lookup[0] = (fd2_field_cell_lookup){9,1}; break;
            case 13:
                g->units.items[2].flags = FD2_FIELD_UNIT_FLAG_ACTED;
                g->metadata.turn_events[0] = (fd2_field_turn_event){7,9,1}; break;
            case 14:
                g->units.items[2].flags = FD2_FIELD_UNIT_FLAG_ACTED;
                g->metadata.turn_events[0] = (fd2_field_turn_event){7,0,1};
                g->event_log_count = FD2_FIELD_EVENT_LOG_CAPACITY; break;
            case 15: g->units.items[0].detail_status[2] = 1; break;
            case 16:
                g->units.items[1].hp = g->units.items[1].hp_max = 100;
                g->units.items[0].experience = 80; g->progress_preflight=reject_growth; break; /* 最大20升级资源，不能赌实际18。 */
        }
        CHECK(unchanged_rejection(&f) == 0 && !f.calls);
        close_fixture(&f);
    }
    CHECK(resolve_and_ack(NULL) == -1);
    return 0;
}

static int test_post_commit_failure_preserves_cost_effect_and_freezes(void) {
    for (int phase_failure = 0; phase_failure < 2; phase_failure++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        if (phase_failure) {
            g->units.items[2].flags = FD2_FIELD_UNIT_FLAG_ACTED;
            g->metadata.turn_events[0] = (fd2_field_turn_event){7,0,1};
            /* 已知action0执行缺少部署资源；不是未知handler预检。 */
        } else f.corrupt_cell = 1; /* 测试回调在首RNG后破坏cell，模拟后段错误。 */
        CHECK(target(&f, 0) == 0);
        CHECK(resolve_and_ack(g) == -1);
        CHECK(g->failure == (phase_failure ? FD2_FIELD_FAILURE_EVENT_EXECUTION : FD2_FIELD_FAILURE_EVENT_DATA));
        CHECK(g->units.items[0].mp == 6 && g->units.items[1].hp == 0 && g->units.items[1].flags == 1);
        CHECK(fd2_field_unit_has_acted(&g->units.items[0]) && f.calls == 9);
        CHECK(g->units.items[0].experience == (phase_failure ? 42 : 0)); /* lookup先于EX写。 */
        CHECK(unchanged_rejection(&f) == 0);
        close_fixture(&f);
    }
    return 0;
}

static int test_phase_prediction_hides_dead_players_but_keeps_healed_players(void) {
    const uint8_t spells[] = {0, 13};
    for (size_t i = 0; i < sizeof(spells); i++) {
        fixture f; init(&f);
        f.game.units.items[2].hp = 0; /* 非本次同格目标；finalize将其隐藏。 */
        f.game.metadata.turn_events[0] = (fd2_field_turn_event){7,9,1};
        CHECK(target(&f, spells[i]) == 0);
        CHECK(unchanged_rejection(&f) == 0 && !f.calls);
        close_fixture(&f);
    }
    fixture f; init(&f);
    fd2_field_game *g = &f.game;
    g->units.items[2].x = g->units.items[2].y = 6;
    g->units.items[2].hp = 0; /* ID13同格：先治疗，再13565仍可行动。 */
    g->metadata.turn_events[0] = (fd2_field_turn_event){7,9,1};
    g->event_log_count = FD2_FIELD_EVENT_LOG_CAPACITY; /* 未进入side1，不需要notice。 */
    CHECK(target(&f, 13) == 0);
    CHECK(resolve_and_ack(g) == 0);
    CHECK(g->units.items[0].mp == 5 && g->units.items[0].experience == 51);
    CHECK(g->units.items[2].hp == 42 && g->units.items[2].flags == 0);
    CHECK(g->active_side == 2 && !g->player_post_waiting_phase && !g->failure);
    CHECK(g->event_log_count == FD2_FIELD_EVENT_LOG_CAPACITY && f.calls == 2); /* 两个治疗目标各1次，无Magic0 impact。 */
    close_fixture(&f); return 0;
}

static int test_death_direction_extended_viewport_contract(void) {
    const struct { uint8_t x,y,hidden; int rotates; } cases[] = {
        {4,4,0,1}, {18,14,0,1}, {3,10,0,0}, {19,10,0,0},
        {10,3,0,0}, {10,15,0,0}, {4,4,1,0}
    };
    for (size_t c = 0; c < sizeof(cases) / sizeof(cases[0]); c++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        g->units.items[0].x = g->units.items[0].y = 10;
        g->units.items[0].hp = 28;
        g->cursor_cell_x = g->cursor_cell_y = 10;
        g->camera_cell_x = g->camera_cell_y = 5;
        g->units.items[1].x = cases[c].x; g->units.items[1].y = cases[c].y;
        g->units.items[1].hp = 0;
        g->units.items[1].flags = cases[c].hidden ? 0x85 : 0x84;
        g->units.items[1].frame_phase = 2;
        g->units.items[2].x = 12; g->units.items[2].y = 10;
        g->units.items[3].x = g->units.items[3].y = 23;
        g->units.items[3].hp = 0; g->units.items[3].direction = 1;
        g->units.count = 5;
        unit(&g->units.items[4], 96, 0, 10, 10);
        g->units.items[4].hp = 0; g->units.items[4].flags = 0x85; g->units.items[4].direction = 3;
        CHECK(target(&f, 13) == 0);
        CHECK(resolve_and_ack(g) == 0);
        CHECK(g->units.items[1].direction == (cases[c].rotates ? 0 : 2));
        CHECK(g->units.items[3].direction == (cases[c].rotates ? 0 : 1));
        CHECK(g->units.items[4].direction == 3); /* hidden死者不参与旋转。 */
        CHECK(g->units.items[1].flags == 1 && g->units.items[3].flags == 1 && g->units.items[4].flags == 1);
        CHECK(g->units.items[1].frame_phase == 2);
        CHECK(g->units.items[0].direction == 3 && g->units.items[2].direction == 1 && f.calls == 1);
        close_fixture(&f);
    }
    return 0;
}

static int test_formal_command_routes_basic_magic(void) {
    fixture f;
    init(&f);
    fd2_field_game *g = &f.game;
    CHECK(fd2_field_game_refresh_commands(g) == 0);
    CHECK(!g->command_disabled[FD2_FIELD_COMMAND_MAGIC]);
    CHECK(g->command_selected == FD2_FIELD_COMMAND_MAGIC);
    CHECK(fd2_field_game_confirm_cursor(g) == 0);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_MAGIC_LIST && !f.calls);
    uint8_t id = 0xff;
    CHECK(fd2_field_game_magic_list_step(g, FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, &id) == 1 && id == 0);
    CHECK(fd2_field_game_begin_magic_targeting(g, id) == 0);
    CHECK(fd2_field_game_confirm_cursor(g) == 0);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_BROWSE && g->selected_unit == -1);
    CHECK(ack_progress(g) == 0);
    CHECK(g->units.items[0].mp == 6 && g->units.items[0].experience == 42 && f.calls == 9);
    CHECK(g->units.items[1].hp == 0 && g->units.items[1].flags == 1);
    CHECK(fd2_field_unit_has_acted(&g->units.items[0]));
    fd2_field_game before = *g;
    CHECK(fd2_field_game_confirm_cursor(g) == -1);
    CHECK(!memcmp(&before, g, sizeof(before)) && f.calls == 9);
    close_fixture(&f);

    init(&f);
    g->units.items[0].mp = 0;
    CHECK(fd2_field_game_refresh_commands(g) == 0 && !g->command_disabled[FD2_FIELD_COMMAND_MAGIC]);
    CHECK(fd2_field_game_confirm_command(g) == 0);
    before = *g;
    CHECK(fd2_field_game_magic_list_step(g, FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, &id) == 0);
    CHECK(!memcmp(&before, g, sizeof(before)) && !f.calls);
    CHECK(fd2_field_game_cancel_selection(g) == 1);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_COMMAND && g->command_selected == FD2_FIELD_COMMAND_MAGIC);
    g->units.items[0].data_09_1e[0x15] |= 0x10; /* 未知36才拒整表。 */
    CHECK(fd2_field_game_refresh_commands(g) == 0 && g->command_disabled[FD2_FIELD_COMMAND_MAGIC]);
    g->command_selected = FD2_FIELD_COMMAND_MAGIC;
    before = *g;
    CHECK(fd2_field_game_confirm_command(g) == -1);
    CHECK(!memcmp(&before, g, sizeof(before)) && !f.calls);
    close_fixture(&f);
    return 0;
}

typedef struct {
    fixture *f;
    int fail_phase;
    unsigned seen[4];
    unsigned target_calls;
    size_t observed_calls[2];
    int8_t jitters[2][7];
} magic_observer;

static int observe_magic(const fd2_field_game *game,
        const fd2_field_magic_present_event *event, void *userdata) {
    magic_observer *o = userdata;
    CHECK(event->phase <= FD2_FIELD_MAGIC_PRESENT_FINISH);
    o->seen[event->phase]++;
    CHECK(game == &o->f->game && event->actor_index == 0);
    if (event->phase == FD2_FIELD_MAGIC_PRESENT_PREPARE) {
        CHECK(o->f->calls == 0 && event->actor_before->mp == 8);
        CHECK(game->units.items[0].mp == 8 && event->targets && event->target_count);
    } else if (event->phase == FD2_FIELD_MAGIC_PRESENT_CAST) {
        CHECK(o->f->calls == 0 && event->actor_before->mp == 8);
        CHECK(game->units.items[0].mp == (uint16_t)(8u -
              fd2_field_magic_record_get(event->magic_id)[5] * event->cast_cost_count));
        CHECK(!event->target_before && !event->result);
    } else if (event->phase == FD2_FIELD_MAGIC_PRESENT_TARGET) {
        CHECK(o->target_calls < 2 && event->actor_before && event->target_before && event->result);
        CHECK(event->target_before->hp == event->result->hp_before);
        CHECK(game->units.items[event->target_index].hp == event->result->hp_after);
        CHECK(!fd2_field_unit_has_acted(&game->units.items[0]));
        CHECK(event->impact_count == (event->magic_id == 0 && event->result->hit ? 7 : 0));
        o->observed_calls[o->target_calls] = o->f->calls;
        memcpy(o->jitters[o->target_calls], event->impact_jitter, event->impact_count);
        o->target_calls++;
    } else {
        CHECK(!event->actor_before && !event->target_before && !event->result);
        CHECK(event->action_failed == (o->fail_phase >= 0 &&
              o->fail_phase != FD2_FIELD_MAGIC_PRESENT_FINISH));
    }
    return (int)event->phase == o->fail_phase ? -1 : 0;
}

static int test_full_magic_events_and_jitter_order(void) {
    for (int present = 0; present < 2; present++) {
        fixture f; init(&f);
        fd2_field_game *g = &f.game;
        g->units.count = 5;
        unit(&g->units.items[4], 96, 0, 5, 6);
        g->units.items[4].level = 2; g->units.items[4].movement_profile = 7;
        /* 每个target两次effect RNG之后才是7个impact；下个target从9开始。 */
        for (size_t i = 0; i < 7; i++) {
            f.rolls[2 + i] = (uint32_t)i;
            f.rolls[11 + i] = (uint32_t)(6 - i);
        }
        magic_observer observer = {&f, -1, {0}, 0, {0}, {{0}}};
        if (present) { g->magic_present = observe_magic; g->magic_present_userdata = &observer; }
        CHECK(target(&f, 0) == 0 && resolve_and_ack(g) == 0);
        CHECK(f.calls == 18 && !g->units.items[1].hp && !g->units.items[4].hp);
        CHECK(g->units.items[0].mp == 6 && g->units.items[0].experience == 84);
        CHECK(fd2_field_unit_has_acted(&g->units.items[0]));
        if (present) {
            CHECK(observer.seen[0] == 1 && observer.seen[1] == 1 &&
                  observer.seen[2] == 2 && observer.seen[3] == 1);
            CHECK(observer.observed_calls[0] == 9 && observer.observed_calls[1] == 18);
            for (size_t i = 0; i < 7; i++) {
                CHECK(f.first_target_hp[2 + i] == 28u - (unsigned)(i + 1u) * 28u / 7u);
                CHECK(observer.jitters[0][i] == 1 - (int)(i % 3));
                CHECK(observer.jitters[1][i] == 1 - (int)((6 - i) % 3));
            }
        }
        close_fixture(&f);
    }
    for (int kind = 0; kind < 2; kind++) {
        fixture f; init(&f);
        magic_observer observer = {&f, -1, {0}, 0, {0}, {{0}}};
        f.game.magic_present = observe_magic; f.game.magic_present_userdata = &observer;
        if (!kind) f.rolls[0] = 99; /* miss不生成7次抖动，也不能重试施法。 */
        CHECK(target(&f, kind ? 13 : 0) == 0 && resolve_and_ack(&f.game) == 0);
        CHECK(f.calls == 1 && observer.target_calls == 1 && observer.observed_calls[0] == 1);
        CHECK(observer.seen[3] == 1);
        close_fixture(&f);
    }
    return 0;
}

static int test_magic_present_failure_boundaries(void) {
    for (int phase = FD2_FIELD_MAGIC_PRESENT_PREPARE;
             phase <= FD2_FIELD_MAGIC_PRESENT_FINISH; phase++) {
        fixture f; init(&f);
        magic_observer observer = {&f, phase, {0}, 0, {0}, {{0}}};
        f.game.magic_present = observe_magic; f.game.magic_present_userdata = &observer;
        CHECK(target(&f, 0) == 0);
        fd2_field_game before = f.game;
        CHECK(fd2_field_game_resolve_player_magic(&f.game) == -1);
        CHECK(observer.seen[0] == 1);
        if (phase == FD2_FIELD_MAGIC_PRESENT_PREPARE) {
            CHECK(!memcmp(&before, &f.game, sizeof(before)) && !f.calls);
            CHECK(!observer.seen[1] && !observer.seen[2] && !observer.seen[3]);
        } else {
            CHECK(f.game.units.items[0].mp == 6 && f.game.failure == FD2_FIELD_FAILURE_ACTION);
            CHECK(observer.seen[1] == 1 && observer.seen[3] == 1);
            CHECK(f.calls == (phase == FD2_FIELD_MAGIC_PRESENT_CAST ? 0 : 9));
            CHECK(f.game.units.items[1].hp == (phase == FD2_FIELD_MAGIC_PRESENT_CAST ? 28 : 0));
            before = f.game;
            unsigned finished = observer.seen[3];
            CHECK(fd2_field_game_resolve_player_magic(&f.game) == -1);
            CHECK(!memcmp(&before, &f.game, sizeof(before)) && observer.seen[3] == finished);
        }
        close_fixture(&f);
    }
    return 0;
}

/* Game正式资格保持B2的unit9；仅私有RAM复制品改marker测试通用费用合同。
 * 真正68角色的分布由figani测试、renderer测试与Domain读集各自验证。 */
static int test_caster_cost_markers_zero_multiple_and_wrap(void) {
    const struct { uint8_t count; uint16_t before, after; } cases[] = {
        {1, 8, 6}, {0, 8, 8}, {2, 8, 4}, {4, 8, 0}, {4, 2, 65530}
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        for (int with_present = 0; with_present < 2; with_present++) {
            fixture f; init(&f);
            fd2_field_game *g = &f.game;
            fd2_figani_magic_cast plan;
            CHECK(fd2_figani_magic_cast_plan(&plan, &g->figani_archive, 9) == 0);
            CHECK(plan.clip.intro_end >= cases[i].count);
            for (size_t part_index = 0; part_index < plan.clip.intro_end; part_index++) {
                fd2_figani_part part;
                CHECK(fd2_figani_get_part(&plan.clip, part_index, &part) == 0);
                /* image_data=part+9，marker=part+4，只改本fixture拥有的heap。 */
                uint8_t *marker = (uint8_t *)part.image_data - 5;
                *marker = (uint8_t)(part_index < cases[i].count);
            }
            g->units.items[0].mp = cases[i].before;
            f.rolls[0] = 99;
            magic_observer observer = {&f, -1, {0}, 0, {0}, {{0}}};
            if (with_present && cases[i].before == 8) {
                g->magic_present = observe_magic; g->magic_present_userdata = &observer;
            }
            CHECK(target(&f, 0) == 0 && resolve_and_ack(g) == 0);
            CHECK(g->units.items[0].mp == cases[i].after && f.calls == 1);
            CHECK(g->units.items[1].hp == 28 && !g->failure);
            if (with_present && cases[i].before == 8) CHECK(observer.seen[0] == 1 && observer.seen[3] == 1);
            close_fixture(&f);
        }
    }
    fixture f; init(&f);
    CHECK(target(&f, 0) == 0);
    size_t count = f.game.figani_archive.count;
    f.game.figani_archive.count = 0;
    fd2_field_units before = f.game.units;
    CHECK(fd2_field_game_resolve_player_magic(&f.game) == -1 && !f.calls);
    CHECK(f.game.failure == FD2_FIELD_FAILURE_ACTION && !memcmp(&before, &f.game.units, sizeof(before)));
    CHECK(fd2_field_game_resolve_player_magic(&f.game) == -1 && !f.calls);
    f.game.figani_archive.count = count;
    close_fixture(&f);
    return 0;
}

int main(void) {
    CHECK(test_caster_cost_markers_zero_multiple_and_wrap() == 0);
    CHECK(test_full_magic_events_and_jitter_order() == 0);
    CHECK(test_magic_present_failure_boundaries() == 0);
    CHECK(test_capability_gate_does_not_use_mp() == 0);
    CHECK(test_damage_kill_miss_and_nonlethal_reward() == 0);
    CHECK(test_same_cell_targets_are_raw_order_and_cost_once() == 0);
    CHECK(test_self_heal_and_full_hp_still_charge_and_act() == 0);
    CHECK(test_reward_cap_and_last_player_controller() == 0);
    CHECK(test_all_preflight_rejections_are_before_mp_and_rng() == 0);
    CHECK(test_post_commit_failure_preserves_cost_effect_and_freezes() == 0);
    CHECK(test_phase_prediction_hides_dead_players_but_keeps_healed_players() == 0);
    CHECK(test_death_direction_extended_viewport_contract() == 0);
    CHECK(test_formal_command_routes_basic_magic() == 0);
    puts("field_magic_commit_test: ok");
    return 0;
}
