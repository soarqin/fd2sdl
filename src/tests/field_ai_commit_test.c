/* M7.3 敌方物理 plan/commit 事务测试。
 * 逆向依据：field_ai_move_toward_cell @0x14b78 最终重建并播放路径；
 * field_physical_exchange @0x1548e 在移动后提交进攻、反击和死亡清理。
 * 物品sub_20C6F @0x20c6f／sub_211A4 @0x211a4共享
 * field_hp_heal_apply @0x1c916（原登记sub_1C916），仍保留各自AI尾链。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_ai.h"
#include "field_game.h"
#include "field_magic.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

typedef struct {
    const uint32_t *values;
    size_t count;
    size_t cursor;
    const fd2_field_game *owner;
    uint32_t first_experience;
} test_rng;

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
static int process_automatic_action_and_ack(fd2_field_game *game) {
    int result=fd2_field_game_process_automatic_action(game);
    return result<0?result:ack_progress(game)<0?-1:result;
}
static int end_phase_and_ack(fd2_field_game *game) {
    int result=fd2_field_game_end_active_phase(game);
    return result<0?result:ack_progress(game)<0?-1:result;
}
static uint32_t next_roll(void *userdata) {
    test_rng *rng = userdata;
    if (!rng || rng->cursor >= rng->count) return 0;
    if (rng->cursor == 0 && rng->owner) rng->first_experience = rng->owner->experience_value;
    return rng->values[rng->cursor++];
}

static void set_unit(fd2_field_unit *unit, uint8_t side, int x, int y) {
    memset(unit, 0, sizeof(*unit));
    unit->side = side;
    unit->x = (uint8_t)x;
    unit->y = (uint8_t)y;
    unit->hp = 50;
    unit->hp_max = 50;
    unit->attack = 200;
    unit->defense = 1;
    unit->accuracy = 100;
    unit->movement_profile = 1;
    unit->movement_points = 1;
    unit->level = 1;
    ((uint8_t *)unit)[0x31] = 0xff; /* 纯AI夹具不携未知loot。 */
}

static void equip_weapon(fd2_field_unit *unit) {
    uint8_t *record = (uint8_t *)unit;
    record[0x0a] = 0x40;
    record[0x0b] = 0; /* item 0，最小／最大射程均为 1。 */
}

static void init_game(fd2_field_game *game,
                      uint32_t *cells,
                      fd2_terrain_attr *attrs,
                      test_rng *rng) {
    memset(game, 0, sizeof(*game));
    /* 未配置回合事件；零填充会意外创建 turn 0 / phase 0 的 action 0。 */
    memset(game->metadata.turn_events, 0xff, sizeof(game->metadata.turn_events));
    memset(cells, 0, 8u * 8u * sizeof(*cells));
    memset(attrs, 0, sizeof(*attrs));
    attrs[0].movement_cost_class = 0; /* profile1 class0成本1；class1成本20。 */
    game->ready = 1;
    game->active_side = 0;
    game->selected_unit = -1;
    game->command_selected = -1;
    game->detail_unit = -1;
    game->attack_target = -1;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->map.width = 8;
    game->map.height = 8;
    game->map.cells = cells;
    game->terrain.attrs = attrs;
    game->terrain.attr_count = 1;
    game->units.count = 2;
    set_unit(&game->units.items[0], 0, 1, 1);
    game->units.items[0].movement_points = 2;
    set_unit(&game->units.items[1], 2, 3, 1);
    equip_weapon(&game->units.items[0]);
    game->attack_rng = next_roll;
    game->attack_rng_userdata = rng;
}

static int build_plan(fd2_field_game *game,
                      fd2_field_ai_physical_candidate *plan) {
    fd2_field_path_result range = FD2_FIELD_PATH_RESULT_INITIALIZER;
    int result = fd2_field_game_compute_move_range(game, 0, &range);
    if (result == 0)
        result = fd2_field_ai_choose_physical_candidate(
            &game->map, &game->terrain, &game->units,
            0, 0, &range, plan);
    fd2_field_path_close(&range);
    return result;
}

static int test_commit(void) {
    static const uint32_t rolls[] = {50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.stage = UINT8_MAX;

    fd2_field_ai_physical_candidate plan;
    CHECK(build_plan(&game, &plan) == 0);
    CHECK(plan.unit_index == 1);
    CHECK(plan.destination_x == 2 && plan.destination_y == 1);
    CHECK(plan.priority >= 6);
    CHECK(fd2_field_game_commit_ai_physical(
              &game, 0, 0, &plan, NULL, 0) == 0);
    CHECK(game.units.items[0].x == 2 && game.units.items[0].y == 1);
    /* AI movement faces right while playing, then common tail calls 0x386f8
     * and resets record +0x03 for the complete actor table. */
    CHECK(game.units.items[0].direction == 0);
    CHECK(game.units.items[1].direction == 0);
    CHECK(game.units.items[1].hp == 0);
    CHECK(game.units.items[1].flags == 1);
    CHECK(game.battle_result == FD2_FIELD_BATTLE_DEFEAT);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.selected_unit == -1);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_BROWSE);
    CHECK(rng.cursor == 4);
    free(game.move_path);
    return 0;
}

static int failing_critical_hook(
        const fd2_field_unit *attacker,
        void *userdata,
        uint8_t *base_chance) {
    (void)attacker;
    (void)userdata;
    (void)base_chance;
    return -1;
}

static int test_common_tail_resets_all_directions(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.stage = UINT8_MAX;
    game.units.items[0].ai_behavior_raw = 2;
    game.units.items[0].movement_points = 0;
    game.units.items[0].direction = 3;
    game.units.items[1].direction = 2;
    game.units.items[1].x = 7;
    game.units.items[1].y = 7;

    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.units.items[0].direction == 0);
    CHECK(game.units.items[1].direction == 0);
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_automatic_mode0(void) {
    static const uint32_t rolls[] = {50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    /* 该夹具直接构造非正式 session；stage 字段不参与 M7 调度。 */
    game.stage = UINT8_MAX;

    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.units.items[0].x == 2 && game.units.items[0].y == 1);
    CHECK(game.units.items[1].hp == 0 && game.units.items[1].flags == 1);
    CHECK(game.battle_result == FD2_FIELD_BATTLE_DEFEAT);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    /* battle_result 终止自动 phase；不能再推进到 side 2。 */
    CHECK(game.active_side == 0);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_BROWSE);
    CHECK(game.command_selected == -1);
    CHECK(rng.cursor == 4);
    return 0;
}

static void teach_magic(fd2_field_unit *unit, uint8_t magic_id) {
    unit->data_09_1e[0x11u + magic_id / 8u] |=
        (uint8_t)(1u << (magic_id % 8u));
}

typedef struct {
    test_rng rolls;
    const fd2_field_game *game;
    int x, y, camera_x, camera_y;
    int wrong_focus;
} focus_rng;

static uint32_t focus_observing_roll(void *userdata) {
    focus_rng *check = userdata;
    if (check->game->cursor_cell_x != check->x || check->game->cursor_cell_y != check->y ||
        check->game->camera_cell_x != check->camera_x || check->game->camera_cell_y != check->camera_y)
        check->wrong_focus = 1;
    return next_roll(&check->rolls);
}

/* 154ad/154de、15360/153c2、15055：先caster后目标的逐格聚焦必须
 * 先于首效果RNG；不能用赋cursor或在phase尾强制居中替代。 */
static int test_ai_focus_before_effect_at_map_edge(void) {
    static const uint32_t rolls[16] = {0};
    for (int kind = 0; kind < 3; kind++) for (int top = 0; top < 2; top++) {
        uint32_t cells[24 * 24] = {0};
        fd2_terrain_attr attrs[1];
        fd2_field_game game;
        focus_rng rng = {{rolls, 16, 0}, &game, 15, top ? 0 : 7, 5, top ? 0 : 2, 0};
        init_game(&game, cells, attrs, &rng.rolls);
        game.stage = UINT8_MAX; game.map.width = game.map.height = 24;
        game.cursor_cell_x = 15; game.cursor_cell_y = top ? 1 : 0;
        game.camera_cell_x = 5; game.camera_cell_y = top ? 1 : 0;
        fd2_field_unit *caster = &game.units.items[0], *target = &game.units.items[1];
        caster->x = target->x = 15; caster->y = top ? 1 : 8; target->y = top ? 0 : 7;
        if (top) caster->unit_id = 121; /* 121只在115b6选择器跳focus，AI wrapper不跳。 */
        caster->movement_points = 0; caster->mp = caster->mp_max = 10;
        caster->attack = 20; target->attack = target->accuracy = 0;
        caster->hp = caster->hp_max = target->hp = target->hp_max = 1000;
        game.attack_rng = focus_observing_roll; game.attack_rng_userdata = &rng;
        if (kind == 0) {
            fd2_field_ai_physical_candidate plan;
            CHECK(build_plan(&game, &plan) == 0 && plan.unit_index == 1);
            fd2_field_ai_physical_candidate stale = plan; stale.destination_x--;
            fd2_field_game before = game;
            CHECK(fd2_field_game_commit_ai_physical(&game, 0, 0, &stale, NULL, 0) == -1);
            CHECK(!memcmp(&game, &before, sizeof(game)) && rng.rolls.cursor == 0);
            CHECK(fd2_field_game_commit_ai_physical(&game, 0, 0, &plan, NULL, 0) == 0);
            CHECK(game.move_origin_camera_x == 5 && game.move_origin_camera_y == (top ? 1 : 2));
        } else if (kind == 1) {
            teach_magic(caster, 0);
            fd2_field_ai_magic_candidate plan;
            CHECK(fd2_field_ai_choose_magic_candidate(&game.map, &game.terrain, &game.units, 0, 0, &plan) == 0);
            CHECK(plan.cast_x == 15 && plan.cast_y == rng.y);
            fd2_field_ai_magic_candidate stale = plan; stale.score++;
            fd2_field_game before = game;
            CHECK(fd2_field_game_commit_ai_magic(&game, 0, 0, &stale) == -1);
            CHECK(!memcmp(&game, &before, sizeof(game)) && rng.rolls.cursor == 0);
            CHECK(fd2_field_game_commit_ai_magic(&game, 0, 0, &plan) == 0);
        } else {
            uint8_t *record = (uint8_t *)caster;
            memset(record + 0x0a, 0x80, 16); record[0x0a] = 0; record[0x0b] = 56;
            fd2_field_ai_item_candidate plan;
            CHECK(fd2_field_ai_choose_item_candidate(&game.map, &game.terrain, &game.units, 0, 0, &plan) == 0);
            CHECK(plan.item_id == 56 && plan.target_x == 15 && plan.target_y == rng.y);
            fd2_field_ai_item_candidate stale = plan; stale.score++;
            fd2_field_game before = game;
            CHECK(fd2_field_game_commit_ai_item(&game, 0, 0, &stale) == -1);
            CHECK(!memcmp(&game, &before, sizeof(game)) && rng.rolls.cursor == 0);
            CHECK(fd2_field_game_commit_ai_item(&game, 0, 0, &plan) == 0);
        }
        CHECK(rng.rolls.cursor > 0 && !rng.wrong_focus);
        CHECK(game.camera_cell_y == rng.camera_y && game.cursor_cell_y == rng.y);
        CHECK(fd2_field_unit_has_acted(caster) && caster->x == 15 && caster->y == (top ? 1 : 8));
    }
    return 0;
}

static int test_ai_line_item_and_no_action_focus(void) {
    static const uint32_t rolls[16] = {0};
    uint32_t cells[24 * 24] = {0};
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    focus_rng rng = {{rolls, 16, 0}, &game, 15, 7, 5, 1, 0};
    init_game(&game, cells, attrs, &rng.rolls);
    game.stage = UINT8_MAX; game.map.width = game.map.height = 24;
    game.cursor_cell_x = 15; game.cursor_cell_y = 0; game.camera_cell_x = 5;
    fd2_field_unit *caster = &game.units.items[0], *target = &game.units.items[1];
    caster->x = target->x = 15; caster->y = 8; target->y = 7;
    caster->hp = caster->hp_max = target->hp = target->hp_max = 1000;
    uint8_t *record = (uint8_t *)caster;
    memset(record + 0x0a, 0x80, 16); record[0x0a] = 0; record[0x0b] = 79;
    game.attack_rng = focus_observing_roll; game.attack_rng_userdata = &rng;
    fd2_field_ai_item_candidate plan;
    CHECK(fd2_field_ai_choose_item_candidate(&game.map, &game.terrain, &game.units, 0, 0, &plan) == 0);
    CHECK(plan.item_id == 79 && plan.target_x == 15 && plan.target_y < 7);
    /* 15055远端夹到y0，再focus首raw目标y7，最终camera1，不是直接center。 */
    CHECK(fd2_field_game_commit_ai_item(&game, 0, 0, &plan) == 0);
    CHECK(rng.rolls.cursor > 0 && !rng.wrong_focus);
    CHECK(game.cursor_cell_y == 7 && game.camera_cell_y == 1);

    init_game(&game, cells, attrs, &rng.rolls);
    game.stage = UINT8_MAX; game.map.width = game.map.height = 24;
    game.cursor_cell_x = 15; game.cursor_cell_y = 0; game.camera_cell_x = 5;
    game.units.items[0].x = 2; game.units.items[0].y = 20;
    game.units.items[0].ai_behavior_raw = 8;
    game.units.items[0].direction = 3; rng.rolls.cursor = 0;
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.cursor_cell_x == 15 && game.cursor_cell_y == 0 &&
          game.camera_cell_x == 5 && game.camera_cell_y == 0);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]) &&
          game.units.items[0].direction == 3 && rng.rolls.cursor == 0);
    return 0;
}

static int test_magic_commit(void) {
    static const uint32_t rolls[] = {0, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].mp = game.units.items[0].mp_max = 10;
    game.units.items[0].movement_profile = 1;
    game.units.items[1].movement_profile = 1;
    teach_magic(&game.units.items[0], 0);
    game.units.items[1].x = 2;
    game.units.items[1].y = 1;
    game.units.items[1].hp = game.units.items[1].hp_max = 100;

    fd2_field_ai_magic_candidate plan;
    CHECK(fd2_field_ai_choose_magic_candidate(
        &game.map, &game.terrain, &game.units, 0, 0, &plan) == 0);
    CHECK(plan.magic_id == 0 && plan.score >= 6);
    CHECK(fd2_field_game_commit_ai_magic(&game, 0, 0, &plan) == 0);
    CHECK(game.units.items[1].hp == 55);
    CHECK(game.units.items[0].mp == 8); /* 21227→1ca89在逐目标效果前扣一次2。 */
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.cursor == 2);
    return 0;
}

typedef struct {
    test_rng rng;
    const fd2_field_unit *actor;
    uint16_t expected_mp;
    int wrong_mp;
} magic_cost_rng;

static uint32_t cost_observing_roll(void *userdata) {
    magic_cost_rng *check = userdata;
    if (check->actor->mp != check->expected_mp) check->wrong_mp = 1;
    return next_roll(&check->rng);
}

static int test_magic_cast_cost_before_targets(void) {
    static const uint32_t rolls[16] = {0};
    for (uint8_t id = 0; id < 28u; id++) {
        /* 23/24尚未开放；25的AI评分恒0，不伪造可提交candidate。 */
        if (id >= 23u && id <= 25u) continue;
        uint32_t cells[8 * 8];
        fd2_terrain_attr attrs[1];
        fd2_field_game game;
        test_rng unused = {rolls, 16, 0};
        init_game(&game, cells, attrs, &unused);
        game.stage = UINT8_MAX;
        game.units.count = 3;
        game.units.items[0].mp = game.units.items[0].mp_max = 200;
        teach_magic(&game.units.items[0], id);
        const uint8_t *record = fd2_field_magic_record_get(id);
        uint8_t cost_id = id == 17u ? 18u : id; /* 原226ea实参，不按相同现值省略。 */
        const uint8_t *cost = fd2_field_magic_record_get(cost_id);
        for (size_t i = 1; i < 3; i++) {
            set_unit(&game.units.items[i], record[6] ? 0 : 2, (int)i + 1, 1);
            game.units.items[i].hp_max = 1000;
            game.units.items[i].hp = id >= 13u && id <= 16u ? 1 : 1000;
            if (id == 20u || id == 21u)
                game.units.items[i].detail_status[id - 20u] = 1;
            if (id == 22u) teach_magic(&game.units.items[i], 0);
        }
        magic_cost_rng check = {{rolls, 16, 0}, &game.units.items[0],
                               (uint16_t)(200u - cost[5]), 0};
        game.experience_value = 7;check.rng.owner = &game;
        game.attack_rng = cost_observing_roll;
        game.attack_rng_userdata = &check;
        fd2_field_ai_magic_candidate plan;
        CHECK(fd2_field_ai_choose_magic_candidate(
            &game.map, &game.terrain, &game.units, 0, 0, &plan) == 0);
        CHECK(plan.magic_id == id && plan.score >= 6);
        uint8_t targets[FD2_FIELD_MAX_UNITS];
        size_t target_count;
        CHECK(fd2_field_ai_collect_magic_targets(
            &game.map, &game.terrain, &game.units, id,
            plan.cast_x, plan.cast_y, 0,
            targets, sizeof(targets), &target_count) == 0);
        if (record[4] != 0u) CHECK(target_count >= 2u);
        size_t cure_calls = 0;
        if (id == 20u || id == 21u)
            for (size_t i = 0; i < target_count; i++)
                if (game.units.items[targets[i]].detail_status[id - 20u]) cure_calls++;
        CHECK(game.units.items[0].mp == 200 && check.rng.cursor == 0);
        CHECK(fd2_field_game_commit_ai_magic(&game, 0, 0, &plan) == 0);
        CHECK(game.units.items[0].mp == check.expected_mp && !check.wrong_mp);
        CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
        CHECK(game.experience_value == 0);
        if(check.rng.cursor) CHECK(check.rng.first_experience == 7);
        if (id != 20u && id != 21u)
            CHECK(check.rng.cursor > 0u);
        else
            CHECK(check.rng.cursor == cure_calls);
    }
    return 0;
}

static int test_magic_miss_and_immunity_still_cost(void) {
    static const uint32_t rolls[] = {99};
    for (int immune = 0; immune < 2; immune++) {
        test_rng rng = {rolls, 1, 0};
        uint32_t cells[8 * 8];
        fd2_terrain_attr attrs[1];
        fd2_field_game game;
        init_game(&game, cells, attrs, &rng);
        uint8_t id = immune ? 10u : 0u;
        teach_magic(&game.units.items[0], id);
        game.units.items[0].mp = game.units.items[0].mp_max = 100;
        if (immune) game.units.items[1].movement_profile = 19;
        fd2_field_ai_magic_candidate plan;
        CHECK(fd2_field_ai_choose_magic_candidate(
            &game.map, &game.terrain, &game.units, 0, 0, &plan) == 0);
        CHECK(fd2_field_game_commit_ai_magic(&game, 0, 0, &plan) == 0);
        CHECK(game.units.items[0].mp == 100u - fd2_field_magic_record_get(id)[5]);
        CHECK(game.units.items[1].hp == 50);
        CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
        CHECK(rng.cursor == (immune ? 0u : 1u));
    }
    return 0;
}

static int test_magic_preflight_preserves_cost(void) {
    static const uint32_t rolls[] = {0, 0};
    for (int failure = 0; failure < 5; failure++) {
        test_rng rng = {rolls, 2, 0};
        uint32_t cells[8 * 8];
        fd2_terrain_attr attrs[1];
        fd2_field_game game;
        init_game(&game, cells, attrs, &rng);
        teach_magic(&game.units.items[0], 0);
        game.units.items[0].mp = game.units.items[0].mp_max = 10;
        fd2_field_ai_magic_candidate plan;
        CHECK(fd2_field_ai_choose_magic_candidate(
            &game.map, &game.terrain, &game.units, 0, 0, &plan) == 0);
        switch (failure) {
            case 0: ++plan.score; break;
            case 1: game.units.items[1].movement_profile = 0; break;
            case 2: fd2_field_unit_set_hidden(&game.units.items[1], 1); break;
            case 3: game.units.items[0].mp = 1; break;
            case 4: plan.magic_id = 23; break;
        }
        game.experience_value=7;
        fd2_field_game before = game;
        CHECK(fd2_field_game_commit_ai_magic(&game, 0, 0, &plan) == -1);
        CHECK(memcmp(&game, &before, sizeof(game)) == 0 && rng.cursor == 0);
    }
    return 0;
}

static int test_magic_damage_dispatch_boundaries(void) {
    static const uint32_t rolls[] = {0, 0};
    test_rng rng = {rolls, 2, 0};
    fd2_field_units units = {0};
    units.count = 1;
    units.items[0].hp = units.items[0].hp_max = 2000;
    units.items[0].movement_profile = 19;
    units.items[0].race = 4;

    /* DS:0x1d01 的 ID 9 wrapper 直接进入 0x1c75e；profile 19/race 4
     * 不会触发 IDs 10..12 专属的 0x1f183 免疫门。 */
    CHECK(fd2_field_magic_apply_known_effect(
        9, 0, &units, next_roll, &rng) == 1);
    CHECK(units.items[0].hp == 1101);
    CHECK(rng.cursor == 2);

    /* ID 10 命中同一对象时在 RNG 前免疫。 */
    units.items[0].hp = 2000;
    rng.cursor = 0;
    CHECK(fd2_field_magic_apply_known_effect(
        10, 0, &units, next_roll, &rng) == 0);
    CHECK(units.items[0].hp == 2000);
    CHECK(rng.cursor == 0);

    /* unit ID 28 是 0x1f183 的显式例外，必须继续命中。 */
    units.items[0].unit_id = 28;
    CHECK(fd2_field_magic_apply_known_effect(
        10, 0, &units, next_roll, &rng) == 1);
    CHECK(units.items[0].hp == 1928);
    CHECK(rng.cursor == 2);
    return 0;
}

static int test_magic_heal_boundaries(void) {
    static const uint32_t rolls[] = {0, 99};
    test_rng rng = {rolls, 2, 0};
    fd2_field_units units = {0};
    units.count = 1;
    units.items[0].hp_max = 1000;

    /* IDs 13..16 的 wrappers 只改变 magic ID，均逐目标调用
     * 0x1c8ed→0x1c916。恢复不做命中判定；每目标只消费 spread RNG。 */
    units.items[0].hp = 100;
    CHECK(fd2_field_magic_apply_known_effect(
        13, 0, &units, next_roll, &rng) == 1);
    CHECK(units.items[0].hp == 163); /* 70*9/10 + 0 */
    CHECK(rng.cursor == 1);

    CHECK(fd2_field_magic_apply_known_effect(
        14, 0, &units, next_roll, &rng) == 1);
    CHECK(units.items[0].hp == 302); /* 140*9/10 + 99*140/1000 */
    CHECK(rng.cursor == 2);

    /* HP cap 在提交中执行；即使已满 HP，core 仍消费一次 RNG 并返回。 */
    static const uint32_t cap_rolls[] = {99, 50};
    rng = (test_rng){cap_rolls, 2, 0};
    units.items[0].hp = 990;
    CHECK(fd2_field_magic_apply_known_effect(
        15, 0, &units, next_roll, &rng) == 1);
    CHECK(units.items[0].hp == 1000);
    CHECK(rng.cursor == 1);
    CHECK(fd2_field_magic_apply_known_effect(
        16, 0, &units, next_roll, &rng) == 1);
    CHECK(units.items[0].hp == 1000);
    CHECK(rng.cursor == 2);
    return 0;
}

static int test_unknown_magic_rejected(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].side = 1;
    game.active_side = 1;
    teach_magic(&game.units.items[0], 23);
    game.units.items[0].mp = game.units.items[0].mp_max = 10;
    fd2_field_units before = game.units;
    CHECK(fd2_field_magic_apply_known_effect(
        23, 1, &game.units, next_roll, &rng) == -1);
    CHECK(fd2_field_magic_apply_known_effect(
        28, 1, &game.units, next_roll, &rng) == -1);
    CHECK(memcmp(&before, &game.units, sizeof(before)) == 0);
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_magic_status_commit(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].mp = game.units.items[0].mp_max = 10;
    game.units.items[0].attack = 100;
    game.units.items[0].defense = 80;
    game.units.items[0].accuracy = 90;
    game.units.items[0].evasion = 95;

    CHECK(fd2_field_magic_apply_known_effect(
        17, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].attack_status == 2);
    CHECK(game.units.items[0].attack == 116);
    CHECK(rng.cursor == 1);

    rng.cursor = 0;
    CHECK(fd2_field_magic_apply_known_effect(
        18, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].defense_status == 2);
    CHECK(game.units.items[0].defense == 93);

    rng.cursor = 0;
    CHECK(fd2_field_magic_apply_known_effect(
        19, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].hit_evasion_status == 2);
    CHECK(game.units.items[0].accuracy == 105);
    CHECK(game.units.items[0].evasion == 110);

    /* 已有状态在 RNG 和属性写入前退出。三种 wrapper 只检查各自 byte，
     * 不续期、不叠加，也不读取其他状态。 */
    rng.cursor = 0;
    CHECK(fd2_field_magic_apply_known_effect(
        17, 0, &game.units, next_roll, &rng) == 0);
    CHECK(game.units.items[0].attack_status == 2);
    CHECK(game.units.items[0].attack == 116);
    CHECK(fd2_field_magic_apply_known_effect(
        18, 0, &game.units, next_roll, &rng) == 0);
    CHECK(game.units.items[0].defense_status == 2);
    CHECK(game.units.items[0].defense == 93);
    CHECK(fd2_field_magic_apply_known_effect(
        19, 0, &game.units, next_roll, &rng) == 0);
    CHECK(game.units.items[0].accuracy == 105);
    CHECK(game.units.items[0].evasion == 110);
    CHECK(rng.cursor == 0);

    /* ID 19 的机器码是 16 位 add；锁定溢出回绕。 */
    game.units.items[0].hit_evasion_status = 0;
    game.units.items[0].accuracy = UINT16_MAX - 4u;
    game.units.items[0].evasion = UINT16_MAX - 9u;
    CHECK(fd2_field_magic_apply_known_effect(
        19, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].accuracy == 10);
    CHECK(game.units.items[0].evasion == 5);
    CHECK(rng.cursor == 1);

    /* IDs 17..19 只检查各自状态 byte，不排除 profile 25/26。 */
    game.units.items[0].movement_profile = 25;
    game.units.items[0].attack_status = 0;
    game.units.items[0].attack = 100;
    rng.cursor = 0;
    CHECK(fd2_field_magic_apply_known_effect(
        17, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].attack == 116);

    CHECK(fd2_field_magic_apply_known_effect(
        20, 0, &game.units, next_roll, &rng) == 0);
    CHECK(fd2_field_magic_apply_known_effect(
        21, 0, &game.units, next_roll, &rng) == 0);
    CHECK(rng.cursor == 1);
    game.units.items[0].detail_status[0] = 4;
    game.units.items[0].detail_status[1] = 5;
    CHECK(fd2_field_magic_apply_known_effect(
        20, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].detail_status[0] == 0);
    CHECK(game.units.items[0].detail_status[1] == 5);
    CHECK(fd2_field_magic_apply_known_effect(
        21, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].detail_status[1] == 0);
    CHECK(rng.cursor == 1);

    game.units.items[0].flags &= (uint8_t)~FD2_FIELD_UNIT_FLAG_ACTED;
    CHECK(fd2_field_magic_apply_known_effect(
        25, 0, &game.units, next_roll, &rng) == 0);
    game.units.items[0].flags = FD2_FIELD_UNIT_FLAG_AI_INELIGIBLE |
                               FD2_FIELD_UNIT_FLAG_ACTED;
    CHECK(fd2_field_magic_apply_known_effect(
        25, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].flags == FD2_FIELD_UNIT_FLAG_AI_INELIGIBLE);
    CHECK(rng.cursor == 1);

    static const uint32_t status_rolls[] = {0, 0, 0};
    rng = (test_rng){status_rolls, 3, 0};
    game.units.items[0].movement_profile = 1;
    game.units.items[0].hp = 50;
    CHECK(fd2_field_magic_apply_known_effect(
        22, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].hp == 41);
    CHECK(game.units.items[0].detail_status[2] == 2);
    CHECK(rng.cursor == 3);

    /* IDs 26/27 只是共享 0x22cda 的状态 offset wrappers，RNG 顺序与
     * ID 22 完全相同：gate、raw damage spread、duration。 */
    static const uint32_t shared_rolls[] = {0, 99, 3};
    rng = (test_rng){shared_rolls, 3, 0};
    game.units.items[0].hp = 50;
    game.units.items[0].detail_status[0] = 0;
    CHECK(fd2_field_magic_apply_known_effect(
        26, 0, &game.units, next_roll, &rng) == 1);
    CHECK(game.units.items[0].hp == 41);
    CHECK(game.units.items[0].detail_status[0] == 5);
    CHECK(rng.cursor == 3);

    rng.cursor = 0;
    game.units.items[0].detail_status[1] = 1;
    CHECK(fd2_field_magic_apply_known_effect(
        27, 0, &game.units, next_roll, &rng) == 0);
    CHECK(game.units.items[0].hp == 41);
    CHECK(game.units.items[0].detail_status[1] == 1);
    CHECK(rng.cursor == 0);

    rng.cursor = 0;
    game.units.items[0].detail_status[1] = 0;
    game.units.items[0].movement_profile = 26;
    CHECK(fd2_field_magic_apply_known_effect(
        27, 0, &game.units, next_roll, &rng) == 0);
    CHECK(game.units.items[0].detail_status[1] == 0);
    CHECK(rng.cursor == 0);

    static const uint32_t miss_rolls[] = {50};
    rng = (test_rng){miss_rolls, 1, 0};
    game.units.items[0].movement_profile = 1;
    CHECK(fd2_field_magic_apply_known_effect(
        27, 0, &game.units, next_roll, &rng) == 0);
    CHECK(game.units.items[0].detail_status[1] == 0);
    CHECK(rng.cursor == 1);
    return 0;
}

static int test_item_magic_commit(void) {
    static const uint32_t rolls[] = {0, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[1].hp = game.units.items[1].hp_max = 300;
    game.units.items[1].x = 2;
    game.units.items[1].y = 1;
    uint8_t *record = (uint8_t *)&game.units.items[0];
    memset(record + 0x0a, 0x80, 0x10);
    for (size_t slot = 0; slot < 8; slot++)
        record[0x0bu + slot * 2u] = 0xff;
    record[0x0a] = 0;
    record[0x0b] = 11; /* code 20，value 2（magic ID 2）。 */
    game.units.items[0].side = 1;
    game.active_side = 1;

    fd2_field_ai_item_candidate plan;
    CHECK(fd2_field_ai_choose_item_candidate(
        &game.map, &game.terrain, &game.units, 0, 0, &plan) == 0);
    CHECK(plan.item_id == 11 && plan.score >= 6);
    game.experience_value=7;rng.owner=&game;
    CHECK(fd2_field_game_commit_ai_item(&game, 0, 0, &plan) == 0);
    CHECK(game.units.items[1].hp == 75);
    CHECK(record[0x0b] == 11); /* code 20 不走 code 5 的消耗路径。 */
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.cursor == 2 && rng.first_experience == 7 && game.experience_value == 0);
    return 0;
}

static int test_magic_item_tail_failure_preserves_commit(void) {
    static const uint32_t rolls[] = {0, 0};
    for (int item_case = 0; item_case < 2; item_case++) {
        test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
        uint32_t cells[8 * 8];
        fd2_terrain_attr attrs[1];
        fd2_field_game game;
        init_game(&game, cells, attrs, &rng);
        game.units.items[1].x = 2;
        game.units.items[1].y = 1;
        cells[1u * 8u + 1u] = 1u << 16;
        game.metadata.cell_lookup[0] = (fd2_field_cell_lookup){9, 1};
        fd2_field_ai_magic_candidate magic;
        fd2_field_ai_item_candidate item;
        game.experience_value=7;rng.owner=&game;
        if (item_case) {
            game.units.items[1].hp = game.units.items[1].hp_max = 300;
            game.active_side = game.units.items[0].side = 1;
            uint8_t *record = (uint8_t *)&game.units.items[0];
            memset(record + 0x0a, 0x80, 0x10);
            for (size_t slot = 0; slot < 8; slot++)
                record[0x0bu + slot * 2u] = 0xff;
            record[0x0a] = 0;
            record[0x0b] = 11;
            CHECK(fd2_field_ai_choose_item_candidate(&game.map, &game.terrain,
                      &game.units, 0, 0, &item) == 0);
            CHECK(fd2_field_game_commit_ai_item(&game, 0, 0, &item) == -1);
            CHECK(game.units.items[1].hp == 75 && record[0x0b] == 11);
        } else {
            game.units.items[0].mp = game.units.items[0].mp_max = 10;
            game.units.items[1].hp = game.units.items[1].hp_max = 100;
            teach_magic(&game.units.items[0], 0);
            CHECK(fd2_field_ai_choose_magic_candidate(&game.map, &game.terrain,
                      &game.units, 0, 0, &magic) == 0);
            CHECK(fd2_field_game_commit_ai_magic(&game, 0, 0, &magic) == -1);
            CHECK(game.units.items[1].hp == 55 && game.units.items[0].mp == 8);
        }
        CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
        CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
        CHECK(game.event_log_count == 1 && game.event_log[0].action == 9);
        CHECK(rng.cursor == 2 && rng.first_experience==7 && game.experience_value==0);
        fd2_field_game before = game;
        CHECK((item_case ? fd2_field_game_commit_ai_item(&game, 0, 0, &item)
                        : fd2_field_game_commit_ai_magic(&game, 0, 0, &magic)) == -1);
        CHECK(process_automatic_action_and_ack(&game) == -1);
        CHECK(memcmp(&game, &before, sizeof(game)) == 0 && rng.cursor == 2);
    }
    return 0;
}

static int test_item_magic_wrapper_boundaries(void) {
    static const uint32_t rolls[] = {0, 0};
    test_rng rng = {rolls, 2, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].side = 1;
    game.active_side = 1;
    game.units.items[1].x = 2;
    game.units.items[1].y = 1;
    game.units.items[1].hp = game.units.items[1].hp_max = 1000;
    uint8_t *record = (uint8_t *)&game.units.items[0];
    memset(record + 0x0a, 0x80, 0x10);
    for (size_t slot = 0; slot < 8; slot++)
        record[0x0bu + slot * 2u] = 0xff;

    /* item 38：code 21、value 1。0x2111a wrapper 不进入 code 5 的
     * inventory remove；效果仍按 magic ID 1 使用两次 RNG。 */
    record[0x0a] = 0;
    record[0x0b] = 38;
    fd2_field_ai_item_candidate plan;
    CHECK(fd2_field_ai_choose_item_candidate(
        &game.map, &game.terrain, &game.units, 0, 0, &plan) == 0);
    CHECK(plan.item_id == 38 && plan.score >= 6);
    CHECK(fd2_field_game_commit_ai_item(&game, 0, 0, &plan) == 0);
    CHECK(game.units.items[1].hp == 892);
    CHECK(record[0x0b] == 38);
    CHECK(rng.cursor == 2);

    /* item 79：code 24、value 3，共用 code 20 的 0x20f6d wrapper。
     * 该轴线物品同样保留原 slot。 */
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].side = 1;
    game.active_side = 1;
    game.units.items[1].x = 5;
    game.units.items[1].y = 1;
    game.units.items[1].hp = game.units.items[1].hp_max = 1000;
    record = (uint8_t *)&game.units.items[0];
    memset(record + 0x0a, 0x80, 0x10);
    for (size_t slot = 0; slot < 8; slot++)
        record[0x0bu + slot * 2u] = 0xff;
    record[0x0a] = 0;
    record[0x0b] = 79;
    rng.cursor = 0;
    CHECK(fd2_field_ai_choose_item_candidate(
        &game.map, &game.terrain, &game.units, 0, 0, &plan) == 0);
    CHECK(plan.item_id == 79 && plan.score >= 6);
    CHECK(fd2_field_game_commit_ai_item(&game, 0, 0, &plan) == 0);
    CHECK(game.units.items[1].hp == 550);
    CHECK(record[0x0b] == 79);
    CHECK(rng.cursor == 2);
    return 0;
}

static int test_item_heal_boundaries(void) {
    static const uint32_t rolls[] = {99, 50};
    test_rng rng = {rolls, 2, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].side = 1;
    game.active_side = 1;
    game.units.items[0].hp = game.units.items[0].hp_max = 100;
    game.units.items[1].side = 1;
    game.units.items[1].hp = 10;
    game.units.items[1].hp_max = 100;
    /* 同格只用于锁定逐目标 core：任一有效中心都同时收集两条 raw
     * actor 记录，避免候选器为同分选择只覆盖受伤目标的中心。 */
    game.units.items[1].x = 1;
    game.units.items[1].y = 1;
    uint8_t *record = (uint8_t *)&game.units.items[0];
    memset(record + 0x0a, 0x80, 0x10);
    for (size_t slot = 0; slot < 8; slot++)
        record[0x0bu + slot * 2u] = 0xff;
    record[0x0a] = 0x40;
    record[0x0b] = 40; /* code 13、value 100、范围可覆盖两名友军。 */

    fd2_field_ai_item_candidate plan;
    CHECK(fd2_field_ai_choose_item_candidate(
        &game.map, &game.terrain, &game.units, 0, 1, &plan) == 0);
    CHECK(plan.item_id == 40 && plan.score >= 6);
    CHECK(fd2_field_game_commit_ai_item(&game, 0, 1, &plan) == 0);
    /* target list 按 raw actor index：满 HP actor 0 仍先消费 RNG；actor 1
     * 再消费下一值并按 90%+5% 恢复到 cap。 */
    CHECK(game.units.items[0].hp == 100);
    CHECK(game.units.items[1].hp == 100);
    CHECK(rng.cursor == 2);
    CHECK(record[0x0a] == 0x40 && record[0x0b] == 40);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    return 0;
}

static uint32_t count_heal_roll(void *userdata) {
    test_rng *rng = userdata;
    size_t index = rng->cursor++;
    return index < rng->count ? rng->values[index] : 0;
}

static int test_shared_item_hp_core_preserves_caller_tail(void) {
    static const uint32_t rolls[] = {0, 99};
    const uint8_t ids[] = {192, 40}; /* code5 power40消耗；code13 power100保留。 */
    for (size_t c = 0; c < sizeof(ids); c++) {
        test_rng rng = {rolls, 2, 0};
        uint32_t cells[8 * 8];
        fd2_terrain_attr attrs[1];
        fd2_field_game game;
        init_game(&game, cells, attrs, &rng);
        game.stage = UINT8_MAX;
        game.active_side = game.units.items[0].side = 1;
        game.attack_rng = count_heal_roll;
        game.units.items[0].hp = game.units.items[0].hp_max = 1000;
        game.units.items[0].mp = 7;
        game.units.items[0].experience = 91;
        game.units.items[0].direction = 3;
        game.units.items[1].side = 1;
        game.units.items[1].x = game.units.items[1].y = 1;
        game.units.items[1].hp = 10;
        game.units.items[1].hp_max = 1000;
        game.units.items[1].mp = 9;
        game.units.items[1].experience = 99;
        game.units.items[1].direction = 2;
        uint8_t *record = (uint8_t *)&game.units.items[0];
        for (size_t slot = 0; slot < 8; slot++) {
            record[0x0au + slot * 2u] = 0x80;
            record[0x0bu + slot * 2u] = 0xff;
        }
        record[0x0a] = 0x40;
        record[0x0b] = ids[c];
        fd2_field_ai_item_candidate plan;
        CHECK(fd2_field_ai_choose_item_candidate(&game.map, &game.terrain,
                  &game.units, 0, 1, &plan) == 0);
        CHECK(plan.item_id == ids[c] && plan.score >= 6 && rng.cursor == 0);
        fd2_field_ai_item_candidate stale = plan;
        stale.item_id = c == 0 ? 40 : 192;
        fd2_field_game before = game;
        CHECK(fd2_field_game_commit_ai_item(&game, 0, 1, &stale) == -1);
        CHECK(memcmp(&game, &before, sizeof(game)) == 0 && rng.cursor == 0);
        CHECK(fd2_field_game_commit_ai_item(&game, 0, 1, &plan) == 0);
        /* 满HP raw0仍消耗第一个0；raw1应使用99，不能交换或重复消费。 */
        CHECK(rng.cursor == 2 && game.units.items[0].hp == 1000);
        CHECK(game.units.items[1].hp == (c == 0 ? 49 : 109));
        CHECK(game.units.items[0].mp == 7 && game.units.items[1].mp == 9);
        CHECK(game.units.items[0].experience == 91 && game.units.items[1].experience == 99);
        CHECK(record[0x0a] == (c == 0 ? 0x80 : 0x40));
        CHECK(record[0x0b] == (c == 0 ? 0xff : 40));
        CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
        CHECK(!fd2_field_unit_has_acted(&game.units.items[1]));
        CHECK(game.units.items[0].direction == 0 && game.units.items[1].direction == 0);
        CHECK(game.interaction == FD2_FIELD_INTERACTION_BROWSE && game.selected_unit == -1);
        before = game;
        CHECK(fd2_field_game_commit_ai_item(&game, 0, 1, &plan) == -1);
        CHECK(memcmp(&game, &before, sizeof(game)) == 0 && rng.cursor == 2);
    }
    return 0;
}

static int test_shared_item_hp_core_tail_failure_keeps_commit(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.active_side = game.units.items[0].side = 1;
    game.attack_rng = count_heal_roll;
    game.units.items[0].hp = 10;
    game.units.items[0].hp_max = 100;
    game.units.items[0].mp = 7;
    game.units.items[0].experience = 91;
    uint8_t *record = (uint8_t *)&game.units.items[0];
    for (size_t slot = 0; slot < 8; slot++) {
        record[0x0au + slot * 2u] = 0x80;
        record[0x0bu + slot * 2u] = 0xff;
    }
    record[0x0a] = 0;
    record[0x0b] = 192;
    record[0x19] = 77;
    cells[1u * 8u + 1u] = 1u << 16;
    game.metadata.cell_lookup[0] = (fd2_field_cell_lookup){9, 1};
    fd2_field_ai_item_candidate plan;
    CHECK(fd2_field_ai_choose_item_candidate(&game.map, &game.terrain,
              &game.units, 0, 1, &plan) == 0);
    CHECK(plan.item_id == 192 && rng.cursor == 0);
    CHECK(fd2_field_game_commit_ai_item(&game, 0, 1, &plan) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
    CHECK(game.units.items[0].hp == 46 && game.units.items[0].mp == 7);
    CHECK(game.units.items[0].experience == 91 && rng.cursor == 1);
    CHECK(record[0x0a] == 0x80 && record[0x0b] == 0xff && record[0x19] == 77);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    fd2_field_game before = game;
    CHECK(fd2_field_game_commit_ai_item(&game, 0, 1, &plan) == -1);
    CHECK(process_automatic_action_and_ack(&game) == -1);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0 && rng.cursor == 1);
    return 0;
}

static int test_item_commit(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].side = 1;
    game.active_side = 1;
    game.units.items[0].hp = 10;
    game.units.items[0].hp_max = 100;
    uint8_t *record = (uint8_t *)&game.units.items[0];
    memset(record + 0x0a, 0x80, 0x10);
    for (size_t slot = 0; slot < 8; slot++)
        record[0x0bu + slot * 2u] = 0xff;
    /* 把 code 5 放在中间槽；remove 必须压紧其后的完整 flag/item 对，
     * 并只把最后槽写成 0x80/0xff。 */
    record[0x0a] = 0x40;
    record[0x0b] = 11;  /* code 20，本夹具评分为 0，不应被移除。 */
    record[0x0c] = 0;
    record[0x0d] = 192; /* code 5，value 40。 */
    record[0x0e] = 0x20;
    record[0x0f] = 38;  /* 后继槽，验证双字节压紧。 */
    record[0x19] = 77;  /* 末槽隐藏 ID 是 stale 数据，remove 不应清除。 */

    fd2_field_ai_item_candidate plan;
    CHECK(fd2_field_ai_choose_item_candidate(
        &game.map, &game.terrain, &game.units, 0, 1, &plan) == 0);
    CHECK(plan.item_id == 192 && plan.slot_index == 1 && plan.score >= 6);
    CHECK(fd2_field_game_commit_ai_item(&game, 0, 1, &plan) == 0);
    CHECK(game.units.items[0].hp == 46);
    CHECK(record[0x0a] == 0x40 && record[0x0b] == 11);
    CHECK(record[0x0c] == 0x20 && record[0x0d] == 38);
    /* 原 helper 只隐藏末槽，不清 stale item ID。 */
    CHECK(record[0x18] == 0x80 && record[0x19] == 77);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.cursor == 1);
    return 0;
}

static int test_behavior5_cell_action_commit(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 5;
    game.units.items[0].data_3d = 1;
    game.units.items[0].movement_points = 2;
    game.units.items[1].x = 7;
    game.units.items[1].y = 7;
    cells[1u * 8u + 2u] = 1u << 16;
    attrs[0].flags = 0x20;
    game.metadata.cell_actions[1].mode = 0;
    game.metadata.cell_actions[1].param = 192;
    uint8_t *record = (uint8_t *)&game.units.items[0];
    memset(record + 0x0a, 0x80, 0x10);
    for (size_t slot = 0; slot < 8; slot++)
        record[0x0bu + slot * 2u] = 0xff;

    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.units.items[0].x == 2 && game.units.items[0].y == 1);
    CHECK(record[0x0a] == 0 && record[0x0b] == 192);
    CHECK(record[0x31] == 0 && record[0x32] == 192 && record[0x33] == 0);
    CHECK((game.units.items[0].ai_behavior_raw & 0x0fu) == 7u);
    CHECK(game.cell_action_completed[1] == 1);
    /* 本actor之后仍须扫描被跳过的raw尾，才进入下一玩家phase。 */
    CHECK(game.active_side == 0);
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.active_side == 2);
    /* actor 0 completes the side 0 phase; the following turn boundary clears
     * acted for the complete table. */
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.cursor == 0);

    /* 0x13cf8 忽略 inventory add 的返回值；满库存不能阻止完成位和
     * behavior 7 提交。 */
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 5;
    game.units.items[0].data_3d = 1;
    game.units.items[0].movement_points = 2;
    game.units.items[1].x = 7;
    game.units.items[1].y = 7;
    memset(cells, 0, sizeof(cells));
    cells[1u * 8u + 2u] = 1u << 16;
    attrs[0].flags = 0x20;
    game.metadata.cell_actions[1].mode = 0;
    game.metadata.cell_actions[1].param = 192;
    record = (uint8_t *)&game.units.items[0];
    for (size_t slot = 0; slot < 8; slot++) {
        record[0x0au + slot * 2u] = 0;
        record[0x0bu + slot * 2u] = (uint8_t)slot;
    }
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.cell_action_completed[1] == 1);
    CHECK((game.units.items[0].ai_behavior_raw & 0x0fu) == 7u);
    for (size_t slot = 0; slot < 8; slot++)
        CHECK(record[0x0bu + slot * 2u] == (uint8_t)slot);

    /* 完成位已设置时，原版回跳 behavior 0 的攻击／移动 fallback；不能
     * 再次提交 cell action。 */
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 5;
    game.units.items[0].data_3d = 3;
    game.units.items[0].movement_points = 2;
    game.cell_action_completed[3] = 1;
    memset(cells, 0, sizeof(cells));
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK((game.units.items[0].ai_behavior_raw & 0x0fu) == 5u);
    CHECK(game.cell_action_completed[3] == 1);
    /* 本actor之后仍须扫描被跳过的raw尾，才进入下一玩家phase。 */
    CHECK(game.active_side == 0);
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.active_side == 2);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    return 0;
}

static int test_behavior2_recover_after_queries(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 2;
    game.units.items[0].hp = 40;
    game.units.items[0].hp_max = 100;
    uint8_t *record = (uint8_t *)&game.units.items[0];
    memset(record + 0x0a, 0x80, 0x10);
    for (size_t slot = 0; slot < 8; slot++)
        record[0x0bu + slot * 2u] = 0xff;

    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.units.items[0].hp == 60);
    /* 本actor之后仍须扫描被跳过的raw尾，才进入下一玩家phase。 */
    CHECK(game.active_side == 0);
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.active_side == 2);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_behavior11_magic_then_physical(void) {
    static const uint32_t rolls[] = {0, 0, 50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 11;
    game.units.items[0].mp = game.units.items[0].mp_max = 10;
    teach_magic(&game.units.items[0], 0);
    game.units.items[1].hp = game.units.items[1].hp_max = 300;

    CHECK(process_automatic_action_and_ack(&game) == 1);
    /* 正确 dual 0x13e07..0x13e43：magic 后仍重算并执行 physical。 */
    CHECK(rng.cursor == 6);
    CHECK(game.units.items[1].hp < 255);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.active_side == 0);
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.active_side == 2);
    return 0;
}

/* B5的mode3剧情先完成，再从同一个scheduler入口续原AI11物理段。 */
static int test_behavior11_loot_story_then_physical(void) {
    static const uint32_t rolls[] = {0, 0, 50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[64]; fd2_terrain_attr attrs[1]; fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 11;
    game.units.items[0].mp = game.units.items[0].mp_max = 10;
    teach_magic(&game.units.items[0], 0);
    game.units.items[1].hp = game.units.items[1].hp_max = 300;
    game.units.count = 3; set_unit(&game.units.items[2], 0, 7, 7);
    game.units.items[2].hp = 0;
    ((uint8_t *)&game.units.items[2])[0x31] = 3;
    ((uint8_t *)&game.units.items[2])[0x32] = 8;
    CHECK(fd2_field_game_process_automatic_action(&game) == 1);
    CHECK(game.loot.notice.kind == FD2_FIELD_LOOT_STORY && game.loot.notice.fragment == 8);
    CHECK(game.units.items[1].hp == 255 && game.units.items[0].mp == 8 && rng.cursor == 2);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(fd2_field_game_loot_reply(&game, FD2_FIELD_LOOT_ACK, -1) == 0);
    CHECK(!game.loot.notice.kind && game.loot.source && !game.experience_value);
    CHECK(fd2_field_game_move_cursor(&game, 1, 0) == 0);
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(!game.loot.source && game.units.items[1].hp < 255 && rng.cursor == 6);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    return 0;
}

static int test_behavior11_terminal_story_still_finishes_actor(void) {
    static const uint32_t rolls[] = {0, 0};
    test_rng rng = {rolls, 2, 0}; uint32_t cells[64]; fd2_terrain_attr attrs[1];
    fd2_field_game game; init_game(&game,cells,attrs,&rng);
    fd2_field_unit enemy=game.units.items[0],hero=game.units.items[1];
    game.units.items[0]=hero;game.units.items[0].hp=1;
    game.units.items[1]=enemy;game.units.items[1].ai_behavior_raw=11;
    game.units.items[1].mp=game.units.items[1].mp_max=10;
    teach_magic(&game.units.items[1],0);
    game.units.count=3;set_unit(&game.units.items[2],0,7,7);game.units.items[2].hp=0;
    ((uint8_t*)&game.units.items[2])[0x31]=3;((uint8_t*)&game.units.items[2])[0x32]=8;
    CHECK(fd2_field_game_process_automatic_action(&game)==1);
    CHECK(game.loot.notice.kind==FD2_FIELD_LOOT_STORY && rng.cursor==2);
    CHECK(fd2_field_game_battle_result(&game)==FD2_FIELD_BATTLE_DEFEAT);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[1]));
    CHECK(fd2_field_game_loot_reply(&game,FD2_FIELD_LOOT_ACK,-1)==0 && game.loot.source);
    CHECK(fd2_field_game_process_automatic_action(&game)==1);
    CHECK(!game.loot.source && fd2_field_unit_has_acted(&game.units.items[1]) && rng.cursor==2);
    CHECK(fd2_field_game_battle_result(&game)==FD2_FIELD_BATTLE_DEFEAT);
    return 0;
}

static int test_behavior11_magic_without_physical_candidate(void) {
    static const uint32_t rolls[] = {0, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 11;
    game.units.items[0].mp = game.units.items[0].mp_max = 10;
    teach_magic(&game.units.items[0], 0);
    game.units.items[1].hp = game.units.items[1].hp_max = 300;
    game.units.items[0].movement_profile = 28; /* no magic profile: physical candidate is absent. */

    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.units.items[1].hp == 255); /* magic damage remains committed. */
    CHECK(rng.cursor == 2);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.active_side == 0);
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.active_side == 2);
    return 0;
}

static int external_critical_base(const fd2_field_unit *unit, void *userdata,
                                  uint8_t *base) {
    (void)unit;
    (void)userdata;
    *base = 0;
    return 0;
}

static int test_behavior11_committed_magic_then_physical_failure_stops(void) {
    static const uint32_t rolls[] = {0, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 11;
    game.units.items[0].mp = game.units.items[0].mp_max = 10;
    teach_magic(&game.units.items[0], 0);
    game.units.items[1].hp = game.units.items[1].hp_max = 300;
    /* AI physical 只支持已预检的内置 critical 表；该 hook 使后段拒绝，
     * 不影响先执行的 magic。不能吞掉后段失败或回滚已经消费的 RNG。 */
    game.attack_critical_base = external_critical_base;
    CHECK(process_automatic_action_and_ack(&game) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_ACTION);
    CHECK(game.units.items[1].hp == 255);
    CHECK(game.units.items[0].mp == 8); /* 已扣费和magic效果不因后段失败退款。 */
    CHECK(rng.cursor == 2);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.active_side == 0);
    fd2_field_game before = game;
    CHECK(process_automatic_action_and_ack(&game) == -1);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0);
    CHECK(rng.cursor == 2);
    return 0;
}

static int test_behavior7_hide_on_arrival(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].ai_behavior_raw = 7;
    game.units.items[0].ai_param_35 = game.units.items[0].x;
    game.units.items[0].ai_param_36 = game.units.items[0].y;

    CHECK(process_automatic_action_and_ack(&game) == 1);
    /* behavior 7 的 0x32975 先精确写 flags=1，common tail 随后追加 acted。 */
    CHECK((game.units.items[0].flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.active_side == 0);
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_phase_status_tick(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.stage = UINT8_MAX;
    game.active_side = 2;

    /* enter_phase(1) must not touch the side 0 actor. */
    game.units.items[0].hp_max = 50;
    game.units.items[0].hp = 50;
    game.units.items[0].detail_status[0] = 1;
    game.units.items[0].attack_status = 2;
    CHECK(end_phase_and_ack(&game) == 0);
    CHECK(game.active_side == 1);
    /* Both loops start at raw actor index 0 but still filter by side, so the
     * side 0 actor remains untouched while entering side 1. */
    CHECK(game.units.items[0].hp == 50);
    CHECK(game.units.items[0].detail_status[0] == 1);
    CHECK(game.units.items[0].attack_status == 2);

    /* The side 1 actor receives one poison tick and all six timers decrement
     * once. The test RNG must remain untouched by this phase prelude. */
    game.units.items[1].hp_max = 50;
    game.units.items[1].hp = 50;
    game.units.items[1].detail_status[0] = 1;
    game.units.items[1].attack_status = 2;
    game.units.items[1].defense_status = 1;
    game.units.items[1].hit_evasion_status = 3;
    game.units.items[1].detail_status[1] = 2;
    game.units.items[1].detail_status[2] = 1;
    game.units.items[1].base_attack_le[0] = 100;
    game.units.items[1].base_attack_le[1] = 0;
    game.units.items[1].base_defense_le[0] = 80;
    game.units.items[1].base_defense_le[1] = 0;
    game.units.items[1].dexterity_le[0] = 40;
    game.units.items[1].dexterity_le[1] = 0;
    game.units.items[1].attack = 115;
    game.units.items[1].defense = 92;
    game.units.items[1].accuracy = 55;
    game.units.items[1].evasion = 55;
    game.active_side = 0;
    CHECK(end_phase_and_ack(&game) == 0);
    CHECK(game.active_side == 2);
    CHECK(game.units.items[1].hp == 45);
    CHECK(game.units.items[1].detail_status[0] == 0);
    CHECK(game.units.items[1].attack_status == 1);
    CHECK(game.units.items[1].defense_status == 0);
    CHECK(game.units.items[1].hit_evasion_status == 2);
    CHECK(game.units.items[1].detail_status[1] == 1);
    CHECK(game.units.items[1].detail_status[2] == 0);
    /* defense_status expired and therefore loses its 1.15 multiplier;
     * attack/hit-evasion remain active. */
    CHECK(game.units.items[1].attack == 114); /* 原binary64 1.15截断，不是数学115。 */
    CHECK(game.units.items[1].defense == 80);
    CHECK(game.units.items[1].accuracy == 55);
    CHECK(game.units.items[1].evasion == 55);
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_ai_flag_gate(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.units.items[0].flags = FD2_FIELD_UNIT_FLAG_AI_INELIGIBLE;
    game.units.items[1].side = 2;
    game.active_side = 0;
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.active_side == 2);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_behavior8_outer_loop_boundary(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;

    /* side 1：behavior 8 从 0x13d9a 直接返回 AI caller，不执行
     * common tail。因此所在格即使匹配 match 1，也不能产生 cell event，
     * 不能置 acted，也不能调用全表 direction reset。outer loop 仍在下次
     * scheduler step 越过其余 raw actors 并推进到 side 0。 */
    init_game(&game, cells, attrs, &rng);
    game.stage = UINT8_MAX;
    game.active_side = 1;
    game.units.count = 3;
    set_unit(&game.units.items[0], 1, 1, 1);
    game.units.items[0].ai_behavior_raw = 8;
    game.units.items[0].direction = 3;
    set_unit(&game.units.items[1], 0, 6, 6);
    game.units.items[1].direction = 2;
    set_unit(&game.units.items[2], 2, 7, 7);
    cells[1u * 8u + 1u] = 1u << 16;
    game.metadata.cell_lookup[0] = (fd2_field_cell_lookup){9, 1};
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.active_side == 1);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.units.items[0].direction == 3);
    CHECK(game.units.items[1].direction == 2);
    CHECK(game.event_log_count == 0);
    CHECK(game.unhandled_event_count == 0);
    CHECK(rng.cursor == 0);
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.active_side == 0);

    /* side 0：第一轮候选不足后立即进入第二轮；behavior 8 仍不 acted。
     * phase cursor 保证下一次调用结束本轮，不能因 remaining_units 仍含
     * 该 actor 而重复执行或永久停留。 */
    init_game(&game, cells, attrs, &rng);
    game.stage = UINT8_MAX;
    game.units.items[0].ai_behavior_raw = 8;
    game.units.items[0].direction = 3;
    game.units.items[1].direction = 2;
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.active_side == 0);
    CHECK(game.phase_ai_pass == 1);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.units.items[0].direction == 3);
    CHECK(game.units.items[1].direction == 2);
    CHECK(rng.cursor == 0);
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.active_side == 2);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_phase_acted_lifecycle(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.stage = UINT8_MAX;
    game.active_side = 2;
    game.units.items[0].side = 2;
    game.units.items[1].side = 1;
    fd2_field_unit_set_acted(&game.units.items[0], 1);
    fd2_field_unit_set_acted(&game.units.items[1], 1);

    /* side 2→1 has no clear-all call in field_turn_cycle_run. */
    CHECK(end_phase_and_ack(&game) == 0);
    CHECK(game.active_side == 1);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(fd2_field_unit_has_acted(&game.units.items[1]));

    /* side1 outer已扫描raw尾，先全表clear，再进入phase0事件/status。 */
    CHECK(end_phase_and_ack(&game) == 0);
    CHECK(game.active_side == 0);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(!fd2_field_unit_has_acted(&game.units.items[1]));
    game.units.items[0].side = 0;
    game.units.items[1].side = 2;
    game.units.items[0].ai_behavior_raw = 8; /* confirmed early return. */
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(!fd2_field_unit_has_acted(&game.units.items[1]));

    fd2_field_unit_set_acted(&game.units.items[0], 1);
    fd2_field_unit_set_acted(&game.units.items[1], 1);
    CHECK(end_phase_and_ack(&game) == 0);
    CHECK(game.active_side == 2);
    CHECK(game.turn_number == 1);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(!fd2_field_unit_has_acted(&game.units.items[1]));
    return 0;
}

static int test_stage0_handler_gate_preempts_ai(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.stage = 0;
    game.active_side = 0;
    /* stage 0 actor 0 是主角；保留其可见，隐藏唯一 side 0 actor。
     * DS:0x1b19[0] 应在任何 AI 或 phase 推进前返回胜利。 */
    game.units.items[0].side = 2;
    game.units.items[0].flags = 0;
    game.units.items[1].side = 0;
    game.units.items[1].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    CHECK(process_automatic_action_and_ack(&game) == 0);
    CHECK(game.battle_result == FD2_FIELD_BATTLE_VICTORY);
    CHECK(game.active_side == 0);
    CHECK(!fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(!fd2_field_unit_has_acted(&game.units.items[1]));
    CHECK(rng.cursor == 0);

    /* 主角隐藏优先覆盖为失败，即使 side 0 也已经全灭。 */
    game.units.items[0].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    CHECK(process_automatic_action_and_ack(&game) == 0);
    CHECK(game.battle_result == FD2_FIELD_BATTLE_DEFEAT);
    CHECK(game.active_side == 0);
    CHECK(rng.cursor == 0);
    return 0;
}

static int test_battle_result_query(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.stage = UINT8_MAX; /* 纯夹具用未开放stage哨兵覆盖通用side存活查询；不套stage1村民私有门。 */

    /* init_game 的 actor 0 为 side 0，actor 1 为 side 2。 */
    CHECK(fd2_field_game_battle_result(&game) ==
          FD2_FIELD_BATTLE_ONGOING);
    /* DS:0x1b19[0] / dual 0x205b4：stage 0 在无可见 side 0 actor
     * 时结束，不等待尚未加载的增援 group。 */
    game.units.items[0].hp = 0;
    game.units.items[0].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    CHECK(fd2_field_game_battle_result(&game) ==
          FD2_FIELD_BATTLE_VICTORY);

    init_game(&game, cells, attrs, &rng);
    game.stage = 0;
    game.units.items[0].side = 2; /* actor 0 是 stage 0 主角。 */
    game.units.items[1].side = 0;
    game.units.items[0].hp = 0;
    game.units.items[0].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    CHECK(fd2_field_game_battle_result(&game) ==
          FD2_FIELD_BATTLE_DEFEAT);
    return 0;
}

static int test_ai_cell_event_pending_then_tail(void) {
    static const uint32_t rolls[] = {50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.stage = UINT8_MAX;
    game.units.items[1].x = 4;
    game.units.items[1].y = 1;
    /* 中间格lookup0收集pending7，不立即dispatch；最终格lookup1
     * 命中才覆盖为9，整个动作／acted完成后只dispatch最后事件。 */
    cells[1u * 8u + 2u] = 1u << 16;
    cells[1u * 8u + 3u] = 2u << 16;
    game.metadata.cell_lookup[0] = (fd2_field_cell_lookup){7, 0};
    game.metadata.cell_lookup[1] = (fd2_field_cell_lookup){9, 1};

    fd2_field_ai_physical_candidate plan;
    CHECK(build_plan(&game, &plan) == 0);
    CHECK(plan.destination_x == 3 && plan.destination_y == 1);
    CHECK(fd2_field_game_commit_ai_physical(
              &game, 0, 0, &plan, NULL, 0) == -1);
    CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.event_log_count == 1);
    CHECK(game.event_total_count == 1);
    CHECK(game.unhandled_event_count == 1);
    CHECK(game.event_log[0].kind == FD2_FIELD_EVENT_CELL);
    CHECK(game.event_log[0].action == 9);
    CHECK(game.event_log[0].unit_index == 0);
    CHECK(game.event_log[0].x == 3 && game.event_log[0].y == 1);
    CHECK(game.event_log[0].slot == 1);

    /* 正式自动 scheduler 的 outer side loop 应在发现尚无 handler 的
     * notice 后显式停机；不能继续执行下一个 actor。直接 commit API
     * 同样返回失败，并保留已提交动作和 acted。 */
    init_game(&game, cells, attrs, &rng);
    game.stage = UINT8_MAX;
    game.units.items[1].x = 7;
    game.units.items[1].y = 7;
    game.units.items[0].ai_behavior_raw = 8;
    cells[1u * 8u + 1u] = 1u << 16;
    game.metadata.cell_lookup[0] = (fd2_field_cell_lookup){9, 1};
    game.units.items[0].ai_behavior_raw = 2;
    game.units.items[0].hp = game.units.items[0].hp_max;
    game.units.items[0].movement_points = 0;
    CHECK(process_automatic_action_and_ack(&game) == -1);
    CHECK(game.event_log_count == 1);
    CHECK(game.unhandled_event_count == 1);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.active_side == 0);
    return 0;
}

static int test_ai_pending_last_match_and_no_path(void) {
    static const uint32_t rolls[] = {50, 0, 99, 0};
    for (int final_match = 0; final_match < 2; final_match++) {
        test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
        uint32_t cells[64];
        fd2_terrain_attr attrs[1];
        fd2_field_game game;
        init_game(&game, cells, attrs, &rng);
        game.stage = UINT8_MAX;
        game.units.items[0].movement_points = 3;
        game.units.items[1].x = 5;
        cells[1u * 8u + 2u] = 1u << 16;
        cells[1u * 8u + 3u] = 2u << 16;
        game.metadata.cell_lookup[0] = (fd2_field_cell_lookup){7, 0};
        game.metadata.cell_lookup[1] = (fd2_field_cell_lookup){8, 0};
        if (final_match) {
            cells[1u * 8u + 4u] = 3u << 16;
            game.metadata.cell_lookup[2] = (fd2_field_cell_lookup){9, 1};
        }
        fd2_field_ai_physical_candidate plan;
        CHECK(build_plan(&game, &plan) == 0);
        CHECK(plan.destination_x == 4 && plan.destination_y == 1);
        uint8_t *owned_path = malloc(2);
        CHECK(owned_path != NULL);
        owned_path[0] = 0x6d;
        owned_path[1] = 0x7e;
        game.move_path = owned_path;
        game.move_path_capacity = 2;
        game.move_path_length = 1;
        game.move_preview_valid = 0;
        CHECK(fd2_field_game_commit_ai_physical(
                  &game, 0, 0, &plan, NULL, 0) == -1);
        CHECK(game.failure == FD2_FIELD_FAILURE_EVENT_UNSUPPORTED);
        CHECK(game.units.items[0].x == 4 && game.units.items[0].y == 1);
        CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
        CHECK(game.units.items[0].direction == 0);
        CHECK(rng.cursor == 4); /* 未在第一个未知lookup0处提早停止。 */
        CHECK(game.event_log_count == 1 && game.event_total_count == 1);
        CHECK(game.failure_notice.action == (final_match ? 9 : 8));
        CHECK(game.failure_notice.slot == (size_t)(final_match ? 2 : 1));
        CHECK(game.failure_notice.x == 4 && game.failure_notice.y == 1);
        CHECK(game.pending_cell_event.valid &&
              game.pending_cell_event.action == (final_match ? 9 : 8));
        CHECK(game.move_path == owned_path && game.move_path_capacity == 2 &&
              game.move_path_length == 1 && game.move_preview_valid == 0);
        CHECK(owned_path[0] == 0x6d && owned_path[1] == 0x7e);
        fd2_field_game before = game;
        CHECK(process_automatic_action_and_ack(&game) == -1);
        CHECK(fd2_field_game_commit_ai_physical(
                  &game, 0, 0, &plan, NULL, 0) == -1);
        CHECK(memcmp(&game, &before, sizeof(game)) == 0 && rng.cursor == 4);
        free(owned_path);
        game.move_path = NULL;
    }

    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[64];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.stage = UINT8_MAX;
    /* 相邻敌方ZOC禁止origin展开；预算仍有效，唯一候选是原地。 */
    game.units.items[0].movement_points = 1;
    game.units.items[1].x = 2;
    cells[1u * 8u + 1u] = 1u << 16;
    game.metadata.cell_lookup[0] = (fd2_field_cell_lookup){7, 0};
    fd2_field_ai_physical_candidate plan;
    CHECK(build_plan(&game, &plan) == 0);
    CHECK(plan.destination_x == 1 && plan.destination_y == 1);
    game.pending_cell_event = (fd2_field_pending_cell_event){1, 42, 4};
    fd2_field_ai_physical_candidate invalid = plan;
    invalid.destination_x = 0;
    fd2_field_game before = game;
    CHECK(fd2_field_game_commit_ai_physical(
              &game, 0, 0, &invalid, NULL, 0) == -1);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0 && rng.cursor == 0);
    CHECK(fd2_field_game_commit_ai_physical(
              &game, 0, 0, &plan, NULL, 0) == 0);
    CHECK(game.units.items[0].x == 1 && game.units.items[0].y == 1);
    CHECK(game.event_log_count == 0 && !game.pending_cell_event.valid);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]) && rng.cursor == 4);
    return 0;
}

static int test_ai_outer_pending_dispatch_without_rng(void) {
    test_rng rng = {NULL, 0, 0};
    uint32_t cells[64];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);
    game.stage = UINT8_MAX;
    game.phase_ai_pass = 1;
    game.units.items[1].x = game.units.items[1].y = 7;
    game.units.items[0].ai_behavior_raw = 4;
    game.units.items[0].ai_param_35 = 4;
    game.units.items[0].ai_param_36 = 1;
    cells[1u * 8u + 2u] = 1u << 16;
    cells[1u * 8u + 3u] = 2u << 16;
    game.metadata.cell_lookup[0] = (fd2_field_cell_lookup){7, 0};
    game.metadata.cell_lookup[1] = (fd2_field_cell_lookup){8, 0};
    game.pending_cell_event = (fd2_field_pending_cell_event){1, 42, 4};
    CHECK(process_automatic_action_and_ack(&game) == -1);
    CHECK(game.failure_notice.action == 8 && game.failure_notice.slot == 1);
    CHECK(game.units.items[0].x == 3 && game.units.items[0].y == 1);
    CHECK(fd2_field_unit_has_acted(&game.units.items[0]));
    CHECK(game.event_log_count == 1 && game.active_side == 0 && rng.cursor == 0);
    fd2_field_game before = game;
    CHECK(process_automatic_action_and_ack(&game) == -1);
    CHECK(memcmp(&game, &before, sizeof(game)) == 0 && rng.cursor == 0);

    /* 新actor调度起点清旧pending；无任何lookup命中不能沿用42。 */
    init_game(&game, cells, attrs, &rng);
    game.stage = UINT8_MAX;
    game.phase_ai_pass = 1;
    game.units.items[0].ai_behavior_raw = 2;
    game.units.items[0].movement_points = 0;
    game.units.items[1].x = game.units.items[1].y = 7;
    game.pending_cell_event = (fd2_field_pending_cell_event){1, 42, 4};
    CHECK(process_automatic_action_and_ack(&game) == 1);
    CHECK(game.failure == FD2_FIELD_FAILURE_NONE && game.event_log_count == 0);
    CHECK(!game.pending_cell_event.valid && rng.cursor == 0);
    return 0;
}

static int test_stale_plan(void) {
    static const uint32_t rolls[] = {50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    uint32_t cells[8 * 8];
    fd2_terrain_attr attrs[1];
    fd2_field_game game;
    init_game(&game, cells, attrs, &rng);

    fd2_field_ai_physical_candidate plan;
    CHECK(build_plan(&game, &plan) == 0);
    fd2_field_units before = game.units;
    plan.destination_x = 0;
    CHECK(fd2_field_game_commit_ai_physical(
              &game, 0, 0, &plan, NULL, 0) == -1);
    CHECK(memcmp(&before, &game.units, sizeof(before)) == 0);
    CHECK(rng.cursor == 0);
    CHECK(game.selected_unit == -1);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_BROWSE);

    /* 可失败 callback 必须在移动和 RNG 前拒绝；否则失败重试会看到不同
     * 随机流。生产 AI 提交固定使用已确认 profile 暴击表。 */
    plan.destination_x = 2;
    plan.destination_y = 1;
    game.attack_critical_base = failing_critical_hook;
    before = game.units;
    CHECK(fd2_field_game_commit_ai_physical(
              &game, 0, 0, &plan, NULL, 0) == -1);
    CHECK(memcmp(&before, &game.units, sizeof(before)) == 0);
    CHECK(rng.cursor == 0);
    return 0;
}

static int reject_story_resource(const fd2_field_game *game,
        const fd2_field_loot_notice *notice, void *userdata) {
    (void)game; (void)userdata; return notice->kind==FD2_FIELD_LOOT_STORY?-1:0;
}
static int test_ai_effect_unknown_loot_is_before_first_rng(void) {
    static const uint32_t rolls[]={0,0};
    for(int item_case=0;item_case<2;item_case++) {
        test_rng r={rolls,2,0};uint32_t cells[64];fd2_terrain_attr attrs[1];fd2_field_game game;
        init_game(&game,cells,attrs,&r);game.units.items[1].x=2;game.units.items[1].y=1;
        game.units.items[1].hp=10;game.units.items[1].hp_max=100;
        ((uint8_t*)&game.units.items[1])[0x31]=3;
        game.loot_preflight=reject_story_resource;
        game.experience_value=7;
        fd2_field_ai_magic_candidate magic;fd2_field_ai_item_candidate item;
        if(item_case) {
            game.active_side=game.units.items[0].side=1;
            /* Item AI仅对HP>power的目标评分；用非目标既存死者证明全表门。 */
            game.units.items[1].hp=game.units.items[1].hp_max=300;
            ((uint8_t*)&game.units.items[1])[0x31]=0xff;
            game.units.count=3;set_unit(&game.units.items[2],0,7,7);
            game.units.items[2].hp=0;((uint8_t*)&game.units.items[2])[0x31]=2;
            ((uint8_t*)&game.units.items[2])[0x32]=99;
            uint8_t *raw=(uint8_t*)&game.units.items[0];memset(raw+10,0x80,16);raw[10]=0;raw[11]=11;
            CHECK(fd2_field_ai_choose_item_candidate(&game.map,&game.terrain,&game.units,0,0,&item)==0);
        } else {
            game.units.items[0].mp=100;teach_magic(&game.units.items[0],0);
            CHECK(fd2_field_ai_choose_magic_candidate(&game.map,&game.terrain,&game.units,0,0,&magic)==0);
        }
        fd2_field_game before=game;
        CHECK((item_case?fd2_field_game_commit_ai_item(&game,0,0,&item):fd2_field_game_commit_ai_magic(&game,0,0,&magic))==-1);
        CHECK(r.cursor==0 && !memcmp(&before.units,&game.units,sizeof(game.units)) &&
              game.experience_value==before.experience_value);
        CHECK(game.failure==(item_case?FD2_FIELD_FAILURE_NONE:FD2_FIELD_FAILURE_ACTION));
    }
    return 0;
}
/* 同步loot的非side2 mode0/1会直接终止队列，不能丢掉public direct尾。
 * 使用真实AI score/targets：item11只对HP>magic2.power250评分，故同格
 * HP300保证合法score，HP1记录证明实际击杀与descriptor收集。 */
static int synchronous_item_fixture(fd2_field_game *game, uint32_t cells[64],
        fd2_terrain_attr attrs[1], test_rng *rng, uint8_t descriptor,
        fd2_field_ai_item_candidate *plan) {
    init_game(game,cells,attrs,rng);
    game->active_side=1;game->units.items[0].side=1;
    game->units.items[0].direction=3;
    uint8_t *raw=(uint8_t*)&game->units.items[0];
    for(size_t slot=0;slot<8;slot++){raw[10+2*slot]=0x80;raw[11+2*slot]=0xff;}
    raw[10]=0;raw[11]=11;
    game->units.count=14;
    for(size_t i=1;i<14;i++){set_unit(&game->units.items[i],0,7,7);game->units.items[i].flags=1;}
    for(size_t i=1;i<=2;i++){
        game->units.items[i].x=2;game->units.items[i].y=1;game->units.items[i].flags=0;
        game->units.items[i].hp_max=300;game->units.items[i].direction=2;
    }
    game->units.items[1].hp=1;game->units.items[2].hp=300;
    raw=(uint8_t*)&game->units.items[1];raw[0x31]=descriptor;
    raw[0x32]=descriptor==0?192:descriptor==1?0xe8:8;raw[0x33]=descriptor==1?3:0;
    game->units.items[13].side=2; /* 已知callback4用于数common tail dispatch。 */
    cells[1*8+1]=1u<<16;
    game->metadata.cell_lookup[0].event_code=4;game->metadata.cell_lookup[0].match_arg=1;
    game->resource_value=5;game->experience_value=7;rng->owner=game;
    CHECK(fd2_field_ai_choose_item_candidate(&game->map,&game->terrain,&game->units,0,1,plan)==0);
    printf("sync fixture candidate item=%u score=%d target=%d,%d\n",plan->item_id,plan->score,plan->target_x,plan->target_y);
    CHECK(plan->item_id==11 && plan->score==8);
    uint8_t targets[FD2_FIELD_MAX_UNITS];size_t target_count=0;
    CHECK(fd2_field_ai_collect_item_targets(&game->map,&game->terrain,&game->units,0,11,
        plan->target_x,plan->target_y,1,targets,sizeof(targets),&target_count)==0 &&
        target_count==2 && targets[0]==1 && targets[1]==2);
    return 0;
}
static int test_item_synchronous_loot_keeps_direct_and_scheduler_tail_once(void) {
    int failures=0;
    for(int scheduler=0;scheduler<=1;scheduler++)for(uint8_t mode=0;mode<2;mode++) {
        static const uint32_t rolls[16]={0};test_rng rng={rolls,16,0};
        uint32_t cells[64];fd2_terrain_attr attrs[1];fd2_field_game game;
        fd2_field_ai_item_candidate plan;
        CHECK(synchronous_item_fixture(&game,cells,attrs,&rng,mode,&plan)==0);
        int result=scheduler?fd2_field_game_process_automatic_action(&game):
            fd2_field_game_commit_ai_item(&game,0,1,&plan);
        int okay=result==(scheduler?1:0) && !game.failure && !game.loot.source &&
            !game.loot.notice.kind && !game.loot.resume && !game.progress.resume &&
            fd2_field_unit_has_acted(&game.units.items[0]) &&
            game.units.items[0].direction==0 && game.units.items[2].direction==0 &&
            game.units.items[1].hp==0 && game.units.items[1].flags==1 && game.units.items[2].hp==75 &&
            rng.cursor==4 && rng.first_experience==7 && !game.experience_value &&
            game.resource_value==5 && game.event_log_count==1 && game.event_total_count==1 &&
            game.event_log[0].action==4 && game.units.items[13].side==1;
        printf("sync item tail scheduler=%d descriptor=%u result=%d acted=%d events=%zu hp=%u/%u rng=%zu loot_source=%u loot_resume=%u pass=%d\n",
            scheduler,mode,result,fd2_field_unit_has_acted(&game.units.items[0]),game.event_log_count,
            game.units.items[1].hp,game.units.items[2].hp,rng.cursor,game.loot.source,game.loot.resume,okay);
        if(!okay)failures++;
    }
    CHECK(failures==0);return 0;
}
static int test_item_async_loot_and_rejection_boundaries_remain(void) {
    static const uint32_t rolls[16]={0};test_rng rng={rolls,16,0};
    uint32_t cells[64];fd2_terrain_attr attrs[1];fd2_field_game game;fd2_field_ai_item_candidate plan;
    CHECK(synchronous_item_fixture(&game,cells,attrs,&rng,3,&plan)==0);
    CHECK(fd2_field_game_commit_ai_item(&game,0,1,&plan)==0);
    CHECK(game.loot.notice.kind==FD2_FIELD_LOOT_STORY && game.loot.notice.fragment==8 &&
        game.loot.resume==FD2_FIELD_PROGRESS_RESUME_AI_DIRECT && game.loot.ai_common_tail &&
        !fd2_field_unit_has_acted(&game.units.items[0]) && game.event_log_count==0 && rng.cursor==4);
    fd2_field_game before=game;
    CHECK(fd2_field_game_loot_reply(&game,FD2_FIELD_LOOT_CANCEL,-1)==-1 &&
        !memcmp(&before,&game,sizeof(game)) && rng.cursor==4);
    CHECK(fd2_field_game_loot_reply(&game,FD2_FIELD_LOOT_ACK,-1)==0);
    CHECK(!game.loot.source && !game.progress.resume && fd2_field_unit_has_acted(&game.units.items[0]) &&
        game.event_log_count==1 && game.event_total_count==1 && game.units.items[13].side==1 && rng.cursor==4);
    before=game;
    CHECK(fd2_field_game_loot_reply(&game,FD2_FIELD_LOOT_ACK,-1)==-1 && !memcmp(&before,&game,sizeof(game)));
    rng=(test_rng){rolls,16,0};CHECK(synchronous_item_fixture(&game,cells,attrs,&rng,3,&plan)==0);
    game.loot_preflight=reject_story_resource;before=game;
    CHECK(fd2_field_game_commit_ai_item(&game,0,1,&plan)==-1 && game.failure==FD2_FIELD_FAILURE_ACTION &&
        !memcmp(&game.units,&before.units,sizeof(game.units)) && rng.cursor==0 && game.experience_value==7 && !game.loot.source);
    rng=(test_rng){rolls,16,0};CHECK(synchronous_item_fixture(&game,cells,attrs,&rng,0,&plan)==0);
    ((uint8_t*)&game.units.items[0])[11]=38;before=game;
    CHECK(fd2_field_game_commit_ai_item(&game,0,1,&plan)==-1 && !memcmp(&before,&game,sizeof(game)) && rng.cursor==0);
    puts("async item resume/cancel/double ACK/preflight/stale boundaries: ok");
    return 0;
}

int main(void) {
    if (test_item_async_loot_and_rejection_boundaries_remain() != 0 ||
        test_item_synchronous_loot_keeps_direct_and_scheduler_tail_once() != 0 ||
        test_ai_focus_before_effect_at_map_edge() != 0 ||
        test_ai_line_item_and_no_action_focus() != 0 ||
        test_ai_effect_unknown_loot_is_before_first_rng() != 0 ||
        test_commit() != 0 ||
        test_ai_cell_event_pending_then_tail() != 0 ||
        test_ai_pending_last_match_and_no_path() != 0 ||
        test_ai_outer_pending_dispatch_without_rng() != 0 ||
        test_common_tail_resets_all_directions() != 0 ||
        test_automatic_mode0() != 0 ||
        test_magic_commit() != 0 ||
        test_magic_cast_cost_before_targets() != 0 ||
        test_magic_miss_and_immunity_still_cost() != 0 ||
        test_magic_preflight_preserves_cost() != 0 ||
        test_magic_damage_dispatch_boundaries() != 0 ||
        test_magic_heal_boundaries() != 0 ||
        test_unknown_magic_rejected() != 0 ||
        test_magic_status_commit() != 0 ||
        test_item_magic_commit() != 0 ||
        test_magic_item_tail_failure_preserves_commit() != 0 ||
        test_item_magic_wrapper_boundaries() != 0 ||
        test_item_heal_boundaries() != 0 ||
        test_shared_item_hp_core_preserves_caller_tail() != 0 ||
        test_shared_item_hp_core_tail_failure_keeps_commit() != 0 ||
        test_item_commit() != 0 ||
        test_behavior5_cell_action_commit() != 0 ||
        test_behavior2_recover_after_queries() != 0 ||
        test_behavior11_magic_then_physical() != 0 ||
        test_behavior11_loot_story_then_physical() != 0 ||
        test_behavior11_terminal_story_still_finishes_actor() != 0 ||
        test_behavior11_magic_without_physical_candidate() != 0 ||
        test_behavior11_committed_magic_then_physical_failure_stops() != 0 ||
        test_behavior7_hide_on_arrival() != 0 ||
        test_phase_status_tick() != 0 || test_ai_flag_gate() != 0 ||
        test_behavior8_outer_loop_boundary() != 0 ||
        test_phase_acted_lifecycle() != 0 ||
        test_stage0_handler_gate_preempts_ai() != 0 ||
        test_battle_result_query() != 0 || test_stale_plan() != 0) return 1;
    puts("field_ai_commit_test: ok");
    return 0;
}
