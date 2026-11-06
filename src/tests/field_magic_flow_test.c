/* B2列表状态回归：field_player_magic_command_execute @0x1cff0、
 * field_player_magic_list_select_step @0x1d51d（原登记sub_1D51D）。
 * 合成session验证取消只回COMMAND及Load owner，不执行呈现或施法。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_game.h"

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #expr); \
    return 1; } } while (0)

typedef struct {
    fd2_field_game game;
    uint32_t cells[24 * 24];
    fd2_terrain_attr attr;
    size_t rng_calls;
} fixture;

static uint32_t rng(void *data) { (*(size_t *)data)++; return 17; }

static void learn(fd2_field_unit *actor, unsigned id) {
    actor->data_09_1e[0x11u + id / 8u] |= (uint8_t)(1u << (id % 8u));
}

static int init(fixture *f, int attack_available) {
    memset(f, 0, sizeof(*f));
    fd2_field_game *g = &f->game;
    g->ready = 1;
    g->active_side = 2;
    g->turn_number = 7;
    g->selected_unit = 0;
    g->detail_unit = -1;
    g->attack_target = -1;
    g->interaction = FD2_FIELD_INTERACTION_COMMAND;
    g->magic_list_selected = 9;
    g->map.width = g->map.height = 24;
    g->map.cells = f->cells;
    g->terrain.attrs = &f->attr;
    g->terrain.attr_count = 1;
    g->sprites.frame_count = 140u * 12u;
    g->camera_cell_x = 2; g->camera_cell_y = 1;
    g->cursor_cell_x = 4; g->cursor_cell_y = 3;
    g->units.count = 2;
    fd2_field_unit *actor = &g->units.items[0];
    actor->x = 4; actor->y = 3; actor->direction = 2;
    /* 合成unit18是18890已确认的移动后施法例外；不冒称其初始法术表。 */
    actor->side = 2; actor->unit_id = actor->text_id = 18;
    actor->movement_profile = 5;
    actor->hp = actor->hp_max = 28;
    actor->mp = actor->mp_max = 8;
    actor->data_09_1e[1] = 0x40; /* 已装备weapon52，range1。 */
    actor->data_09_1e[2] = 52;
    learn(actor, 0); learn(actor, 1); learn(actor, 2); learn(actor, 3); learn(actor, 13);
    fd2_field_unit *enemy = &g->units.items[1];
    enemy->x = (uint8_t)(attack_available ? 5 : 12); enemy->y = 3;
    enemy->side = 0; enemy->unit_id = 96; enemy->movement_profile = 7;
    enemy->hp = enemy->hp_max = 28;
    memset(g->metadata.turn_events, 0xff, sizeof(g->metadata.turn_events));
    memset(g->metadata.cell_lookup, 0xff, sizeof(g->metadata.cell_lookup));
    g->pending_cell_event = (fd2_field_pending_cell_event){1, 9, 4};
    g->phase_unit_cursor = 1;
    g->event_total_count = 3;
    g->attack_rng = rng;
    g->attack_rng_userdata = &f->rng_calls;
    g->move_path = malloc(4);
    g->move_range.nodes = calloc(24u * 24u, sizeof(*g->move_range.nodes));
    CHECK(g->move_path && g->move_range.nodes);
    memset(g->move_path, 3, 4);
    g->move_path_capacity = 4; g->move_path_length = 2;
    g->move_preview_valid = 1;
    g->move_origin_x = 2; g->move_origin_y = 3;
    g->move_origin_direction = 1;
    g->move_origin_camera_x = 0; g->move_origin_camera_y = 0;
    g->move_range.width = g->move_range.height = 24;
    g->move_range.nodes[17].distance = 7;
    CHECK(fd2_field_game_refresh_commands(g) == 0);
    CHECK(g->command_disabled[FD2_FIELD_COMMAND_ATTACK] == !attack_available);
    g->command_selected = FD2_FIELD_COMMAND_WAIT;
    g->command_highlight_phase = 1;
    g->command_animation_phase = 3;
    g->command_animation_opening = 1;
    g->last_command_highlight_ms = 912;
    return 0;
}

static void close_fixture(fixture *f) {
    fd2_field_metadata_close(&f->game.metadata);
    fd2_field_path_close(&f->game.move_range);
    free(f->game.move_path);
}

/* 本合成地图只供选择查询，不拥有FDFIELD资源。本例只验记录导入和UI owner，
 * 不冒称地图重载；调用期间显式移出map owner，失败/成功都归还fixture的借用指针。 */
static int import_record_snapshot(fixture *f, const fd2_save_battle_snapshot *snapshot) {
    uint32_t *cells = f->game.map.cells;
    f->game.map.cells = NULL;
    int result = fd2_field_game_import_battle_snapshot(&f->game, snapshot);
    f->game.map.cells = cells;
    return result;
}

static int test_begin_navigation_confirm_and_cancel_preserve_owner(void) {
    for (int attack = 0; attack < 2; attack++) {
        fixture f;
        CHECK(init(&f, attack) == 0);
        fd2_field_game *g = &f.game;
        /* 1/2/3的拒绝必须来自逐项支持门，不能被原8MP的费用不足掩盖。 */
        g->units.items[0].mp = g->units.items[0].mp_max = 64;
        fd2_field_game expected = *g;
        CHECK(g->command_disabled[FD2_FIELD_COMMAND_MAGIC] == 0); /* 已支持的13仍可用。 */
        g->command_selected = FD2_FIELD_COMMAND_MAGIC;
        expected = *g;
        expected.magic_list_selected = 0;
        expected.interaction = FD2_FIELD_INTERACTION_MAGIC_LIST;
        CHECK(fd2_field_game_confirm_command(g) == 0);
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        uint8_t confirmed = 0xa5;
        for (int selected = 0; selected < 4; selected++) {
            /* unit18的0尚未开放，1/2/3也只显示；完整80B/MP/RNG不提交。 */
            CHECK(fd2_field_game_magic_list_step(g, FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, &confirmed) == 0);
            CHECK(confirmed == 0xa5 && !memcmp(g, &expected, sizeof(expected)) && f.rng_calls == 0);
            CHECK(fd2_field_game_magic_list_step(g, FD2_FIELD_MAGIC_LIST_KEY_DOWN, &confirmed) == 0);
            expected.magic_list_selected = selected + 1;
            CHECK(!memcmp(g, &expected, sizeof(expected)));
        }
        CHECK(fd2_field_game_magic_list_step(g, FD2_FIELD_MAGIC_LIST_KEY_DOWN, &confirmed) == 0);
        expected.magic_list_selected = 0;
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        CHECK(fd2_field_game_magic_list_step(g, FD2_FIELD_MAGIC_LIST_KEY_UP, &confirmed) == 0);
        expected.magic_list_selected = 4;
        CHECK(confirmed == 0xa5 && !memcmp(g, &expected, sizeof(expected)));
        CHECK(fd2_field_game_magic_list_step(g, FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, &confirmed) == 1);
        CHECK(confirmed == 13 && !memcmp(g, &expected, sizeof(expected)));
        CHECK(fd2_field_game_magic_list_step(g, FD2_FIELD_MAGIC_LIST_KEY_CANCEL, &confirmed) == -1);
        CHECK(confirmed == 13 && !memcmp(g, &expected, sizeof(expected)));
        /* 宿主完成关闭动画才end；另一轮检查统一Esc分派也只关闭列表。 */
        CHECK((attack ? fd2_field_game_cancel_selection(g) :
                        fd2_field_game_end_magic_list(g)) == (attack ? 1 : 0));
        expected.magic_list_selected = 0;
        expected.interaction = FD2_FIELD_INTERACTION_COMMAND;
        /* 未开放项不连带禁用13；ATTACK不可用时首项为MAGIC，ITEM仍可查看。 */
        CHECK(!g->command_disabled[FD2_FIELD_COMMAND_ITEM]);
        expected.command_selected = attack ? FD2_FIELD_COMMAND_ATTACK : FD2_FIELD_COMMAND_MAGIC;
        expected.command_highlight_phase = 0;
        expected.command_animation_phase = 0;
        expected.command_animation_opening = 0;
        expected.last_command_highlight_ms = 0;
        CHECK(!memcmp(g, &expected, sizeof(expected)));
        CHECK(g->move_range.nodes[17].distance == 7);
        CHECK(g->move_path_length == 2 && g->move_path[0] == 3 && g->move_path[1] == 3);
        CHECK(f.rng_calls == 0 && g->units.items[0].direction == 2);
        CHECK(g->pending_cell_event.valid && g->pending_cell_event.action == 9);
        CHECK(fd2_field_game_begin_magic_list(g) == 0 && g->magic_list_selected == 0);
        close_fixture(&f);
    }
    return 0;
}

static int test_low_mp_and_list_data_gates(void) {
    fixture f;
    CHECK(init(&f, 0) == 0);
    fd2_field_game *g = &f.game;
    g->units.items[0].unit_id = 9;
    g->move_path_length = 0; /* START unit9只在零步COMMAND可施法。 */
    g->units.items[0].mp = 0;
    g->units.items[0].detail_status[0] = 2; /* +25不等于+27禁法状态。 */
    CHECK(fd2_field_game_begin_magic_list(g) == 0);
    fd2_field_game before = *g;
    uint8_t confirmed = 0xa5;
    CHECK(fd2_field_game_magic_list_step(g, FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, &confirmed) == 0);
    CHECK(confirmed == 0xa5 && !memcmp(g, &before, sizeof(before)));
    CHECK(fd2_field_game_magic_list_step(g, FD2_FIELD_MAGIC_LIST_KEY_OTHER, NULL) == -2);
    CHECK(!memcmp(g, &before, sizeof(before)));
    CHECK(fd2_field_game_end_magic_list(g) == 0);
    for (unsigned count = 0; count <= 13; count += count == 0 ? 12 : 1) {
        memset(g->units.items[0].data_09_1e + 0x11, 0, 5);
        for (unsigned id = 0; id < count; id++) learn(&g->units.items[0], id);
        before = *g;
        CHECK(fd2_field_game_begin_magic_list(g) == (count == 12 ? 0 : -1));
        if (count == 12) CHECK(fd2_field_game_end_magic_list(g) == 0);
        else CHECK(!memcmp(g, &before, sizeof(before)));
    }
    for (unsigned id = 36; id < 40; id++) {
        memset(g->units.items[0].data_09_1e + 0x11, 0, 5);
        learn(&g->units.items[0], 0); learn(&g->units.items[0], id);
        before = *g;
        CHECK(fd2_field_game_begin_magic_list(g) == -1);
        CHECK(!memcmp(g, &before, sizeof(before)));
    }
    CHECK(f.rng_calls == 0);
    close_fixture(&f);
    return 0;
}

static int test_movement_gate_uses_exact_unit_ids(void) {
    const unsigned ids[] = {9, 17, 18, 19, 20, 33, 34, 35};
    for (size_t i = 0; i < sizeof(ids) / sizeof(ids[0]); i++) {
        for (int moved = 0; moved < 2; moved++) {
            fixture f;
            CHECK(init(&f, 0) == 0);
            fd2_field_game *g = &f.game;
            g->units.items[0].unit_id = (uint8_t)ids[i];
            g->move_path_length = moved ? 2u : 0u;
            fd2_field_game before = *g;
            int allowed = !moved || ids[i] == 18 || ids[i] == 19 || ids[i] == 34;
            CHECK(fd2_field_game_begin_magic_list(g) == (allowed ? 0 : -1));
            if (allowed) CHECK(fd2_field_game_end_magic_list(g) == 0);
            else CHECK(!memcmp(g, &before, sizeof(before)));
            CHECK(f.rng_calls == 0);
            close_fixture(&f);
        }
    }
    fixture f;
    CHECK(init(&f, 0) == 0);
    fd2_field_game *g = &f.game;
    g->units.items[0].unit_id = 9;
    g->move_path_length = 0;
    CHECK(fd2_field_game_begin_magic_list(g) == 0);
    g->move_path_length = 2; /* 已打开后失效，也不能绕过同一入口约束。 */
    fd2_field_game before = *g;
    uint8_t confirmed = 0xa5;
    CHECK(fd2_field_game_magic_list_step(g, FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, &confirmed) == -2);
    CHECK(fd2_field_game_end_magic_list(g) == -1);
    CHECK(!memcmp(g, &before, sizeof(before)) && confirmed == 0xa5 && f.rng_calls == 0);
    close_fixture(&f);
    return 0;
}

static void invalidate(fd2_field_game *g, unsigned which) {
    switch (which) {
        case 0: g->ready = 0; break;
        case 1: g->failure = FD2_FIELD_FAILURE_EVENT_DATA; break;
        case 2: g->player_post_waiting_phase = 1; break;
        case 3: g->active_side = 1; break;
        case 4: g->units.count = FD2_FIELD_MAX_UNITS + 1u; break;
        case 5: g->selected_unit = -1; break;
        case 6: g->selected_unit = (int)g->units.count; break;
        case 7: g->units.items[0].side = 0; break;
        case 8: g->units.items[0].flags |= FD2_FIELD_UNIT_FLAG_HIDDEN; break;
        case 9: g->units.items[0].flags |= FD2_FIELD_UNIT_FLAG_ACTED; break;
        case 10: g->units.items[0].hp = 0; break;
        case 11: g->units.items[0].detail_status[2] = 1; break;
    }
}

static int test_invalid_actor_and_context_are_atomic(void) {
    CHECK(fd2_field_game_begin_magic_list(NULL) == -1);
    CHECK(fd2_field_game_magic_list_step(NULL, FD2_FIELD_MAGIC_LIST_KEY_CANCEL, NULL) == -2);
    CHECK(fd2_field_game_end_magic_list(NULL) == -1);
    for (unsigned which = 0; which < 12; which++) {
        fixture f;
        CHECK(init(&f, 0) == 0);
        fd2_field_game *g = &f.game;
        invalidate(g, which);
        fd2_field_game before = *g;
        CHECK(fd2_field_game_begin_magic_list(g) == -1);
        CHECK(!memcmp(g, &before, sizeof(before)));
        g->interaction = FD2_FIELD_INTERACTION_MAGIC_LIST;
        before = *g;
        uint8_t confirmed = 0xa5;
        CHECK(fd2_field_game_magic_list_step(g, FD2_FIELD_MAGIC_LIST_KEY_CANCEL, &confirmed) == -2);
        CHECK(fd2_field_game_end_magic_list(g) == -1);
        CHECK(confirmed == 0xa5 && !memcmp(g, &before, sizeof(before)) && f.rng_calls == 0);
        close_fixture(&f);
    }
    fixture f;
    CHECK(init(&f, 0) == 0);
    fd2_field_game *g = &f.game;
    uint8_t confirmed = 0xa5;
    fd2_field_game before = *g;
    CHECK(fd2_field_game_magic_list_step(g, FD2_FIELD_MAGIC_LIST_KEY_CONFIRM, &confirmed) == -2);
    CHECK(fd2_field_game_end_magic_list(g) == -1);
    CHECK(confirmed == 0xa5 && !memcmp(g, &before, sizeof(before)));
    g->interaction = FD2_FIELD_INTERACTION_BROWSE;
    before = *g;
    CHECK(fd2_field_game_begin_magic_list(g) == -1);
    CHECK(!memcmp(g, &before, sizeof(before)));
    close_fixture(&f);
    return 0;
}

static int test_load_resets_list_index_only_on_success(void) {
    fixture f;
    CHECK(init(&f, 0) == 0);
    fd2_field_game *g = &f.game;
    fd2_save_battle_snapshot snapshot, another;
    memset(&snapshot, 0xa5, sizeof(snapshot));
    CHECK(fd2_field_game_export_battle_snapshot(g, &snapshot) == 0);
    another = snapshot;
    g->magic_list_selected = 2;
    CHECK(fd2_field_game_export_battle_snapshot(g, &another) == 0);
    CHECK(!memcmp(&snapshot, &another, sizeof(snapshot))); /* 索引不进SAV。 */
    CHECK(fd2_field_game_begin_magic_list(g) == 0);
    g->magic_list_selected = 4;
    fd2_field_game before = *g;
    snapshot.metadata[0] ^= 1;
    CHECK(import_record_snapshot(&f, &snapshot) == -1);
    CHECK(!memcmp(g, &before, sizeof(before)));
    CHECK(g->move_range.nodes[17].distance == 7 && g->move_path[1] == 3);
    snapshot.metadata[0] ^= 1;
    CHECK(import_record_snapshot(&f, &snapshot) == 0);
    CHECK(g->magic_list_selected == 0 && g->interaction == FD2_FIELD_INTERACTION_BROWSE);
    CHECK(g->selected_unit == -1 && g->move_path_length == 0 && !g->move_range.nodes);
    CHECK(g->units.items[0].mp == 8 && !fd2_field_unit_has_acted(&g->units.items[0]));
    CHECK(f.rng_calls == 0);
    close_fixture(&f);
    return 0;
}

int main(void) {
    CHECK(test_begin_navigation_confirm_and_cancel_preserve_owner() == 0);
    CHECK(test_low_mp_and_list_data_gates() == 0);
    CHECK(test_movement_gate_uses_exact_unit_ids() == 0);
    CHECK(test_invalid_actor_and_context_are_atomic() == 0);
    CHECK(test_load_resets_list_index_only_on_success() == 0);
    puts("field_magic_flow_test: ok");
    return 0;
}
