/* M6 无演出攻击状态机与 B6 只读演出事件测试。
 * 覆盖 field_physical_exchange @0x1548e、field_physical_attack_sequence
 * @0x1e856 与 field_physical_attack_resolve @0x1ecc7：目标／武器预检、
 * 状态效果、双击、反击和固定 RNG 顺序。
 * FULL 计数另据 sub_2EBE1 @0x2ebe1；这些无窗口测试不代表原版图形验收。
 */
#include <stdio.h>
#include <string.h>

#include "field_attack.h"
#include "field_game.h"

typedef struct {
    const uint32_t *values;
    size_t count;
    size_t cursor;
} test_rng;

static uint32_t test_cells[8 * 8];
static fd2_terrain_attr test_terrain_attrs[4];

static uint32_t next_roll(void *userdata) {
    test_rng *rng = userdata;
    if (!rng || rng->cursor >= rng->count) return 0;
    return rng->values[rng->cursor++];
}

typedef struct {
    uint8_t side_zero;
    uint8_t side_two;
} critical_bases;

static int critical_base_by_side(const fd2_field_unit *attacker,
                                 void *userdata,
                                 uint8_t *base_chance) {
    if (!attacker || !base_chance) return -1;
    const critical_bases *bases = userdata;
    *base_chance = attacker->side == 0 && bases ? bases->side_zero :
                   attacker->side == 2 && bases ? bases->side_two : 0;
    return 0;
}

static int adjacent_enemy(const fd2_field_unit *attacker,
                          const fd2_field_unit *defender,
                          void *userdata) {
    (void)userdata;
    if (!attacker || !defender || attacker->side == defender->side) return 0;
    int dx = (int)attacker->x - (int)defender->x;
    int dy = (int)attacker->y - (int)defender->y;
    if (dx < 0) dx = -dx;
    if (dy < 0) dy = -dy;
    return dx + dy == 1;
}

static void init_unit(fd2_field_unit *unit, uint8_t side, int x, int y) {
    memset(unit, 0, sizeof(*unit));
    unit->side = side;
    unit->x = (uint8_t)x;
    unit->y = (uint8_t)y;
    unit->hp = 10;
    unit->hp_max = 10;
    unit->attack = 20;
    unit->accuracy = 100;
    unit->level = 1;
    ((uint8_t *)unit)[0x31] = 0xff; /* synthetic死亡不携未知loot。 */
}

static void equip_weapon(fd2_field_unit *unit, uint8_t item_id) {
    uint8_t *record = (uint8_t *)unit;
    record[0x0a] = 0x40;
    record[0x0b] = item_id;
}

static void init_game(fd2_field_game *game) {
    memset(game, 0, sizeof(*game));
    memset(test_cells, 0, sizeof(test_cells));
    memset(test_terrain_attrs, 0, sizeof(test_terrain_attrs));
    test_terrain_attrs[0].movement_cost_class = 0;
    test_terrain_attrs[1].movement_cost_class = 1;
    test_terrain_attrs[2].movement_cost_class = 2;
    test_terrain_attrs[3].movement_cost_class = 6;
    game->ready = 1;
    game->active_side = 2;
    game->selected_unit = 0;
    game->detail_unit = -1;
    game->attack_target = -1;
    game->interaction = FD2_FIELD_INTERACTION_COMMAND;
    game->map.width = 8;
    game->map.height = 8;
    game->map.cells = test_cells;
    game->terrain.attrs = test_terrain_attrs;
    game->terrain.attr_count = 4;
    game->units.count = 3;
    init_unit(&game->units.items[0], 2, 5, 5);
    init_unit(&game->units.items[1], 0, 6, 5);
    init_unit(&game->units.items[2], 2, 5, 6);
    equip_weapon(&game->units.items[0], 0);
}

static int expect(int condition, const char *label) {
    if (condition) return 0;
    fprintf(stderr, "field game attack test failed: %s\n", label);
    return -1;
}

static int configure_with_bases(fd2_field_game *game, test_rng *rng,
                                critical_bases *bases) {
    return fd2_field_game_set_attack_hooks(
        game, adjacent_enemy, NULL, critical_base_by_side, bases,
        next_roll, rng);
}

static int configure(fd2_field_game *game, test_rng *rng) {
    return configure_with_bases(game, rng, NULL);
}

static int test_weapon_required_with_range_hook(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    fd2_field_game game;
    init_game(&game);
    uint8_t *record = (uint8_t *)&game.units.items[0];
    record[0x0a] = 0;
    record[0x0b] = 0xff;
    if (configure(&game, &rng) != 0) return -1;
    if (expect(!fd2_field_game_attack_target_is_legal(&game, 1),
               "range hook cannot make weaponless target legal") ||
        expect(fd2_field_game_begin_attack(&game) == -1,
               "weaponless attacker cannot enter targeting") ||
        expect(rng.cursor == 0,
               "weapon preflight consumes no RNG"))
        return -1;
    return 0;
}

/* 18d8c→14818→115b6：候选按raw/filter0，不另加HP/acted门；
 * unit121保留轮询槽但不调用12cea。选择/取消不执行战斗提交。 */
static int test_target_focus_and_cycle(void) {
    test_rng rng = {NULL, 0, 0};
    fd2_field_game game;
    init_game(&game);
    game.cursor_cell_x = 5;
    game.cursor_cell_y = 5;
    game.focus_cycle_unit = 2;
    game.units.count = 5;
    game.units.items[1].hp = 0;
    fd2_field_unit_set_acted(&game.units.items[1], 1);
    init_unit(&game.units.items[3], 0, 5, 4);
    init_unit(&game.units.items[4], 0, 4, 5);
    game.units.items[4].unit_id = 121;
    game.move_path_length = 1;
    game.move_origin_x = 4;
    game.move_origin_y = 5;
    game.pending_cell_event.valid = 1;
    game.pending_cell_event.action = 7;
    game.pending_cell_event.source_slot = 3;
    fd2_field_unit units[5];
    memcpy(units, game.units.items, sizeof(units));
    if (configure(&game, &rng) != 0 ||
        expect(fd2_field_game_begin_attack(&game) == 0,
               "physical target selection opens") ||
        expect(game.magic_target_count == 3 && game.magic_target_cycle == 0 &&
               game.magic_target_candidates[0] == 1 &&
               game.magic_target_candidates[1] == 3 &&
               game.magic_target_candidates[2] == 4,
               "filter0 candidates retain raw order including HP0/acted/121") ||
        expect(game.cursor_cell_x == 6 && game.cursor_cell_y == 5,
               "initial focus moves to the first raw candidate") ||
        expect(fd2_field_game_cycle_attack_target(&game) == 1 &&
               game.cursor_cell_x == 5 && game.cursor_cell_y == 4,
               "Z advances to the next raw target") ||
        expect(fd2_field_game_cycle_attack_target(&game) == 1 &&
               game.magic_target_cycle == 2 &&
               game.cursor_cell_x == 5 && game.cursor_cell_y == 4,
               "121 consumes one slot without moving focus") ||
        expect(fd2_field_game_cycle_attack_target(&game) == 1 &&
               game.magic_target_cycle == 0 &&
               game.cursor_cell_x == 6 && game.cursor_cell_y == 5,
               "target cycling wraps to the first raw candidate") ||
        expect(fd2_field_game_cancel_attack(&game) == 1 &&
               game.interaction == FD2_FIELD_INTERACTION_COMMAND &&
               game.cursor_cell_x == 5 && game.cursor_cell_y == 5 &&
               !game.move_range.nodes && !game.magic_target_count &&
               !game.magic_target_cycle,
               "cancel clears target owner and restores caster command focus") ||
        expect(game.focus_cycle_unit == 2 && game.move_path_length == 1 &&
               game.move_origin_x == 4 && game.move_origin_y == 5 &&
               game.pending_cell_event.valid && game.pending_cell_event.action == 7 &&
               game.pending_cell_event.source_slot == 3 &&
               memcmp(units, game.units.items, sizeof(units)) == 0 && rng.cursor == 0,
               "target focus/cancel preserve movement, pending, units and RNG") ||
        expect(fd2_field_game_cycle_attack_target(&game) == -1,
               "target cycling cannot escape into COMMAND"))
        return -1;

    /* 首槽121不跳到后面的普通目标；hidden槽不进入候选。 */
    game.units.items[1].unit_id = 121;
    game.units.items[4].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    if (expect(fd2_field_game_begin_attack(&game) == 0 &&
               game.magic_target_count == 2 && game.magic_target_cycle == 0 &&
               game.cursor_cell_x == 5 && game.cursor_cell_y == 5,
               "first 121 retains incoming focus instead of skipping the slot") ||
        expect(fd2_field_game_cycle_attack_target(&game) == 1 &&
               game.cursor_cell_x == 5 && game.cursor_cell_y == 4,
               "cycling from first 121 reaches the next candidate") ||
        expect(fd2_field_game_cancel_attack(&game) == 1,
               "121 selection can be cancelled"))
        return -1;
    game.units.items[1].flags |= FD2_FIELD_UNIT_FLAG_HIDDEN;
    if (expect(fd2_field_game_begin_attack(&game) == 0 &&
               game.magic_target_count == 1 && game.magic_target_candidates[0] == 3 &&
               game.cursor_cell_x == 5 && game.cursor_cell_y == 4,
               "hidden raw slots are skipped at initial focus") ||
        expect(fd2_field_game_cycle_attack_target(&game) == 1 &&
               game.magic_target_cycle == 0 &&
               game.cursor_cell_x == 5 && game.cursor_cell_y == 4,
               "single target wraps without another focus owner") ||
        expect(fd2_field_game_cancel_attack(&game) == 1 && rng.cursor == 0,
               "reopening and cancelling never consume combat RNG"))
        return -1;
    return 0;
}

static int test_target_selection_rejection_is_atomic(void) {
    test_rng rng = {NULL, 0, 0};
    fd2_field_game game, before;
    init_game(&game);
    if (configure(&game, &rng) != 0 ||
        fd2_field_attack_range_compute(&game.move_range, &game.map, &game.terrain,
            &game.units.items[0]) != 0)
        return -1;
    game.units.count = 4;
    init_unit(&game.units.items[3], 1, 8, 0);
    before = game;
    if (expect(fd2_field_game_begin_attack(&game) == -1 &&
               memcmp(&game, &before, sizeof(game)) == 0 && rng.cursor == 0,
               "late invalid visible coordinates reject before range/focus handoff"))
        return -1;
    game.units.count = 3;
    game.units.items[1].side = 1;
    before = game;
    if (expect(fd2_field_game_begin_attack(&game) == -1 &&
               memcmp(&game, &before, sizeof(game)) == 0 && rng.cursor == 0,
               "no legal enemy keeps the disabled attack and existing owner unchanged"))
        return -1;
    game.units.items[1].side = 0;
    if (fd2_field_game_begin_attack(&game) != 0) return -1;
    game.magic_target_candidates[0] = FD2_FIELD_MAX_UNITS;
    before = game;
    if (expect(fd2_field_game_cycle_attack_target(&game) == -1 &&
               memcmp(&game, &before, sizeof(game)) == 0 && rng.cursor == 0,
               "invalid cached raw slot rejects before focus/cycle changes"))
        return -1;
    fd2_field_path_close(&game.move_range);
    return 0;
}

static int test_illegal_and_cancel(void) {
    static const uint32_t rolls[] = {0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    fd2_field_game game;
    init_game(&game);
    if (configure(&game, &rng) != 0 ||
        !fd2_field_game_attack_target_is_legal(&game, 1) ||
        fd2_field_game_attack_target_is_legal(&game, 2) ||
        fd2_field_game_begin_attack(&game) != 0)
        return -1;

    game.cursor_cell_x = game.units.items[0].x;
    game.cursor_cell_y = game.units.items[0].y;
    if (expect(fd2_field_game_resolve_attack(&game) == -1,
               "self target is rejected") ||
        expect(rng.cursor == 0, "illegal target consumes no RNG") ||
        expect(game.units.items[1].hp == 10, "illegal target preserves HP") ||
        expect(!fd2_field_unit_has_acted(&game.units.items[0]),
               "illegal target preserves acted bit"))
        return -1;

    game.cursor_cell_x = 0;
    game.cursor_cell_y = 0;
    if (expect(fd2_field_game_cancel_attack(&game) == 1,
               "targeting cancel succeeds") ||
        expect(game.cursor_cell_x == game.units.items[0].x &&
               game.cursor_cell_y == game.units.items[0].y,
               "targeting cancel restores attacker focus") ||
        expect(game.interaction == FD2_FIELD_INTERACTION_COMMAND,
               "cancel returns to command") ||
        expect(game.command_selected == FD2_FIELD_COMMAND_ATTACK &&
               !game.command_disabled[FD2_FIELD_COMMAND_ATTACK] &&
               game.command_animation_opening == 0 &&
               game.command_animation_phase == 0,
               "targeting cancel restores the static command menu") ||
        expect(rng.cursor == 0,
               "targeting cancel and menu rebuild consume no RNG"))
        return -1;
    fd2_field_path_close(&game.move_range);
    return 0;
}

static int test_acted_preflight_rollback(void) {
    static const uint32_t rolls[] = {0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    fd2_field_game game;
    init_game(&game);
    if (configure(&game, &rng) != 0 ||
        fd2_field_game_begin_attack(&game) != 0)
        return -1;

    game.cursor_cell_x = game.units.items[1].x;
    game.cursor_cell_y = game.units.items[1].y;
    fd2_field_unit_set_acted(&game.units.items[0], 1);
    if (expect(fd2_field_game_resolve_attack(&game) == -1,
               "acted attacker is rejected before resolve") ||
        expect(rng.cursor == 0, "acted attacker consumes no RNG") ||
        expect(game.units.items[1].hp == 10, "acted attacker preserves HP") ||
        expect(game.interaction == FD2_FIELD_INTERACTION_TARGETING,
               "acted attacker preserves targeting state"))
        return -1;
    fd2_field_path_close(&game.move_range);
    return 0;
}

static int test_hit_via_confirm_flow(void) {
    static const uint32_t rolls[] = {50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    fd2_field_game game;
    init_game(&game);
    game.units.items[2].hp = 0; /* post-exchange sweep 必须处理全部 actor */
    if (configure(&game, &rng) != 0) return -1;

    game.cursor_cell_x = game.units.items[1].x;
    game.cursor_cell_y = game.units.items[1].y;
    if (expect(fd2_field_game_confirm_cursor(&game) == 0,
               "command confirm enters targeting") ||
        expect(game.interaction == FD2_FIELD_INTERACTION_TARGETING,
               "targeting state entered") ||
        expect(game.move_range.nodes != NULL,
               "targeting exposes a weapon-range overlay") ||
        expect(fd2_field_game_confirm_cursor(&game) == 0,
               "target confirm resolves attack") ||
        expect(game.interaction == FD2_FIELD_INTERACTION_BROWSE,
               "resolved attack finishes action") ||
        expect(game.last_attack_valid && game.last_attack.hit,
               "hit result recorded") ||
        expect(game.units.items[1].hp == 0, "defender HP reaches zero") ||
        expect(game.units.items[1].flags == 1,
               "defeated defender flags are replaced with 1") ||
        expect(game.units.items[2].flags == 1,
               "post-exchange sweep hides every zero-HP actor") ||
        expect(fd2_field_unit_has_acted(&game.units.items[0]),
               "attacker is marked acted") ||
        expect(game.move_range.nodes == NULL,
               "resolved attack clears targeting range") ||
        expect(game.magic_target_count == 0 && game.magic_target_cycle == 0,
               "resolved physical action clears the shared target owner") ||
        expect(fd2_field_game_battle_result(&game) ==
                   FD2_FIELD_BATTLE_VICTORY,
               "lethal final hostile updates battle result") ||
        expect(rng.cursor == 4,
               "sequence roll precedes hit/critical/spread rolls"))
        return -1;
    return 0;
}

static int test_pre_hit_effect_order(void) {
    static const uint32_t rolls[] = {50, 9, 3, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    fd2_field_game game;
    init_game(&game);
    equip_weapon(&game.units.items[0], 4); /* effect type/value = 2/10 */
    if (configure(&game, &rng) != 0 ||
        fd2_field_game_begin_attack(&game) != 0)
        return -1;

    game.cursor_cell_x = game.units.items[1].x;
    game.cursor_cell_y = game.units.items[1].y;
    if (expect(fd2_field_game_resolve_attack(&game) == 0,
               "pre-hit effect attack resolves") ||
        expect(game.units.items[1].detail_status[0] == 5,
               "pre-hit effect writes defender +0x25") ||
        expect(game.last_attack_valid && game.last_attack.hit,
               "hit follows pre-hit effect") ||
        expect(rng.cursor == 6,
               "sequence/effect rolls precede hit/critical/spread rolls"))
        return -1;
    return 0;
}

static int test_profile_critical_table(void) {
    static const uint32_t rolls[] = {50, 0, 4, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    fd2_field_game game;
    init_game(&game);
    game.units.items[0].movement_profile = 1; /* base critical 5 */
    game.units.items[1].hp = 20;
    game.units.items[1].hp_max = 20;
    game.units.items[1].defense = 10;
    if (fd2_field_game_set_attack_hooks(
            &game, adjacent_enemy, NULL, NULL, NULL,
            next_roll, &rng) != 0 ||
        fd2_field_game_begin_attack(&game) != 0)
        return -1;
    game.cursor_cell_x = game.units.items[1].x;
    game.cursor_cell_y = game.units.items[1].y;
    if (expect(fd2_field_game_resolve_attack(&game) == 0,
               "profile critical table attack resolves") ||
        expect(game.last_attack_valid && game.last_attack.critical,
               "profile 1 critical base accepts roll 4") ||
        expect(rng.cursor == 4,
               "profile table lookup consumes no RNG"))
        return -1;
    return 0;
}

static int test_weapon_critical_bonus(void) {
    static const uint32_t rolls[] = {50, 0, 29, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    fd2_field_game game;
    init_game(&game);
    equip_weapon(&game.units.items[0], 7); /* effect type/value = 4/30 */
    game.units.items[1].defense = 10;
    if (configure(&game, &rng) != 0 ||
        fd2_field_game_begin_attack(&game) != 0)
        return -1;

    game.cursor_cell_x = game.units.items[1].x;
    game.cursor_cell_y = game.units.items[1].y;
    if (expect(fd2_field_game_resolve_attack(&game) == 0,
               "critical weapon attack resolves") ||
        expect(game.last_attack_valid && game.last_attack.critical,
               "weapon +0x0a is added to critical threshold") ||
        expect(rng.cursor == 4,
               "sequence roll precedes three critical-hit rolls"))
        return -1;
    return 0;
}

static int test_unknown_terrain_class_fails_before_rng(void) {
    static const uint32_t rolls[] = {50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    fd2_field_game game;
    init_game(&game);
    test_cells[5 * 8 + 5] = 3; /* attr 3 使用非法 class 6 */
    if (configure(&game, &rng) != 0 ||
        fd2_field_game_begin_attack(&game) != 0)
        return -1;
    game.cursor_cell_x = game.units.items[1].x;
    game.cursor_cell_y = game.units.items[1].y;
    if (expect(fd2_field_game_resolve_attack(&game) == -1,
               "unknown terrain class is rejected") ||
        expect(rng.cursor == 0,
               "unsupported terrain preflight consumes no RNG") ||
        expect(game.units.items[1].hp == 10,
               "unsupported terrain preserves defender HP") ||
        expect(game.interaction == FD2_FIELD_INTERACTION_TARGETING,
               "unsupported terrain preserves targeting state"))
        return -1;
    fd2_field_path_close(&game.move_range);
    return 0;
}

static int test_terrain_modifiers_in_exchange(void) {
    static const uint32_t rolls[] = {50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    fd2_field_game game;
    init_game(&game);
    test_cells[5 * 8 + 5] = 2; /* class 2: attack -5% */
    test_cells[5 * 8 + 6] = 2; /* class 2: defense +10% */
    game.units.items[0].attack = 200;
    game.units.items[1].defense = 100;
    game.units.items[1].hp = 100;
    game.units.items[1].hp_max = 100;
    if (configure(&game, &rng) != 0 ||
        fd2_field_game_begin_attack(&game) != 0)
        return -1;

    game.cursor_cell_x = game.units.items[1].x;
    game.cursor_cell_y = game.units.items[1].y;
    if (expect(fd2_field_game_resolve_attack(&game) == 0,
               "terrain-modified attack resolves") ||
        expect(game.last_attack.base_damage == 72,
               "attack -5% and defense +10% precede damage formula") ||
        expect(game.units.items[1].hp == 28,
               "terrain-modified damage commits to HP") ||
        expect(rng.cursor == 4,
               "terrain arithmetic consumes no additional RNG"))
        return -1;
    return 0;
}

static int test_double_strike_sequence(void) {
    static const uint32_t rolls[] = {99, 0, 99, 0, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    fd2_field_game game;
    init_game(&game);
    equip_weapon(&game.units.items[0], 71); /* effect type 3 */
    game.units.items[1].hp = 50;
    game.units.items[1].hp_max = 50;
    game.units.items[1].defense = 10;
    if (configure(&game, &rng) != 0 ||
        fd2_field_game_begin_attack(&game) != 0)
        return -1;

    game.cursor_cell_x = game.units.items[1].x;
    game.cursor_cell_y = game.units.items[1].y;
    if (expect(fd2_field_game_resolve_attack(&game) == 0,
               "double-strike attack resolves") ||
        expect(game.last_attack_valid && game.last_attack_strikes == 2,
               "effect type 3 performs two strikes") ||
        expect(game.units.items[1].hp == 32,
               "both strikes commit HP damage") ||
        expect(!game.last_counterattack_valid,
               "unarmed defender cannot counterattack") ||
        expect(rng.cursor == 7,
               "one sequence roll precedes both strike cores"))
        return -1;
    return 0;
}

static int test_lethal_first_strike_stops_exchange(void) {
    static const uint32_t rolls[] = {99, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    fd2_field_game game;
    init_game(&game);
    equip_weapon(&game.units.items[0], 71); /* effect type 3 */
    equip_weapon(&game.units.items[1], 0);
    if (configure(&game, &rng) != 0 ||
        fd2_field_game_begin_attack(&game) != 0)
        return -1;

    game.cursor_cell_x = game.units.items[1].x;
    game.cursor_cell_y = game.units.items[1].y;
    if (expect(fd2_field_game_resolve_attack(&game) == 0,
               "lethal first strike resolves") ||
        expect(game.last_attack_strikes == 1,
               "HP zero suppresses second strike") ||
        expect(game.units.items[1].hp == 0,
               "first strike reduces armed defender to zero HP") ||
        expect(game.units.items[1].flags == 1,
               "lethal first strike finalizes defender flags") ||
        expect(!game.last_counterattack_valid,
               "zero-HP armed defender cannot counterattack") ||
        expect(rng.cursor == 4,
               "no second-strike or counterattack RNG is consumed"))
        return -1;
    return 0;
}

static int test_adjacent_counterattack(void) {
    static const uint32_t rolls[] = {50, 0, 99, 0, 50, 0, 99, 0};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    fd2_field_game game;
    init_game(&game);
    equip_weapon(&game.units.items[0], 0);
    equip_weapon(&game.units.items[1], 0);
    game.units.items[1].hp = 50;
    game.units.items[1].hp_max = 50;
    game.units.items[1].defense = 10;
    game.units.items[1].attack = 20;
    game.units.items[0].defense = 10;
    critical_bases bases = {.side_zero = 100, .side_two = 0};
    if (configure_with_bases(&game, &rng, &bases) != 0 ||
        fd2_field_game_begin_attack(&game) != 0)
        return -1;

    game.cursor_cell_x = game.units.items[1].x;
    game.cursor_cell_y = game.units.items[1].y;
    if (expect(fd2_field_game_resolve_attack(&game) == 0,
               "adjacent counterattack exchange resolves") ||
        expect(game.last_attack_valid && game.last_attack_strikes == 1,
               "initial sequence recorded") ||
        expect(game.last_counterattack_valid &&
                   game.last_counterattack_strikes == 1,
               "eligible defender counterattacks once") ||
        expect(game.last_counterattack.critical,
               "counterattacker uses its own critical base") ||
        expect(game.units.items[0].hp == 0,
               "critical counterattack may reduce attacker to zero HP") ||
        expect(game.units.items[0].flags == 0x81,
               "dead initiating attacker is hidden then marked acted") ||
        expect(game.units.items[1].hp == 41,
               "initial attack remains committed") ||
        expect(fd2_field_unit_has_acted(&game.units.items[0]),
               "zero-HP initiating attacker still completes its action") ||
        expect(!fd2_field_unit_has_acted(&game.units.items[1]),
               "counterattack does not consume defender action") ||
        expect(rng.cursor == 8,
               "counterattack continues the shared RNG stream"))
        return -1;
    return 0;
}

static int test_command_menu_model(void) {
    static const uint32_t rolls[] = {0};
    test_rng rng = {rolls, 1, 0};
    fd2_field_game game;
    init_game(&game);
    /* 已知193与192共享code5，扩展Use仍保留独立Drop能力。 */
    ((uint8_t *)&game.units.items[0])[0x19] = 193;
    if (configure(&game, &rng) != 0 ||
        fd2_field_game_refresh_commands(&game) != 0)
        return -1;
    if (expect(game.command_selected == FD2_FIELD_COMMAND_ATTACK,
               "attack is the first enabled command") ||
        expect(!game.command_disabled[FD2_FIELD_COMMAND_ATTACK],
               "attack enabled with legal target") ||
        expect(game.command_disabled[FD2_FIELD_COMMAND_MAGIC] &&
               !game.command_disabled[FD2_FIELD_COMMAND_ITEM] &&
               fd2_field_game_item_use_is_available(&game) && fd2_field_game_item_drop_is_available(&game),
               "empty magic is disabled; known herb Use and Drop remain enabled") ||
        expect(!game.command_disabled[FD2_FIELD_COMMAND_WAIT],
               "wait remains enabled") ||
        expect(fd2_field_game_select_command_direction(
                   &game, FD2_FIELD_COMMAND_DIRECTION_LEFT) == 0 &&
               game.command_selected == FD2_FIELD_COMMAND_ATTACK,
               "disabled magic direction preserves selection") ||
        expect(fd2_field_game_select_command_direction(
                   &game, FD2_FIELD_COMMAND_DIRECTION_DOWN) == 1 &&
               game.command_selected == FD2_FIELD_COMMAND_WAIT,
               "down selects wait") ||
        expect(fd2_field_game_confirm_command(&game) == 0,
               "wait command resolves") ||
        expect(fd2_field_unit_has_acted(&game.units.items[0]) &&
               game.interaction == FD2_FIELD_INTERACTION_BROWSE,
               "wait completes the selected unit action") ||
        expect(rng.cursor == 0, "menu and wait consume no RNG"))
        return -1;

    init_game(&game);
    rng.cursor = 0;
    if (configure(&game, &rng) != 0) return -1;
    game.units.items[1].x = 0;
    game.units.items[1].y = 0;
    if (fd2_field_game_refresh_commands(&game) != 0 ||
        expect(game.command_disabled[FD2_FIELD_COMMAND_ATTACK],
               "attack disabled without a legal target") ||
        expect(game.command_selected == FD2_FIELD_COMMAND_ITEM &&
               !game.command_disabled[FD2_FIELD_COMMAND_ITEM],
               "weapon-only item list is first when attack is unavailable") ||
        expect(fd2_field_game_select_command_direction(
                   &game, FD2_FIELD_COMMAND_DIRECTION_UP) == 0 &&
               game.command_selected == FD2_FIELD_COMMAND_ITEM,
               "disabled attack direction preserves item"))
        return -1;

    init_game(&game);
    rng.cursor = 0;
    if (configure(&game, &rng) != 0 ||
        fd2_field_game_refresh_commands(&game) != 0 ||
        fd2_field_game_confirm_command(&game) != 0 ||
        expect(game.interaction == FD2_FIELD_INTERACTION_TARGETING,
               "attack command enters targeting") ||
        expect(rng.cursor == 0, "attack command preflight consumes no RNG"))
        return -1;
    fd2_field_path_close(&game.move_range);
    return 0;
}

static int test_miss_completes_action(void) {
    static const uint32_t rolls[] = {50, 99};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    fd2_field_game game;
    init_game(&game);
    game.units.items[0].accuracy = 50;
    if (configure(&game, &rng) != 0 ||
        fd2_field_game_begin_attack(&game) != 0)
        return -1;

    game.cursor_cell_x = game.units.items[1].x;
    game.cursor_cell_y = game.units.items[1].y;
    if (expect(fd2_field_game_resolve_attack(&game) == 0,
               "miss resolves") ||
        expect(game.last_attack_valid && !game.last_attack.hit,
               "miss result recorded") ||
        expect(game.units.items[1].hp == 10, "miss preserves defender HP") ||
        expect(fd2_field_unit_has_acted(&game.units.items[0]),
               "miss still finishes attacker action") ||
        expect(rng.cursor == 2,
               "miss consumes sequence and hit rolls only"))
        return -1;
    return 0;
}

typedef struct {
    fd2_field_physical_phase phase;
    size_t actor_index, target_index, rng_cursor;
    uint8_t counter, strike, status_applied;
    int failed;
    fd2_field_failure failure;
    fd2_field_unit actor_before, target_before;
    fd2_field_attack_result result;
    uint16_t actor_hp_after, target_hp_after;
    uint8_t target_status_after;
} physical_observation;

typedef struct {
    test_rng *rng;
    physical_observation events[8];
    size_t count, prepares, strikes, finishes;
    int invalid, fail_phase, fail_strike, fail_cleanup;
} physical_observer;

/* 只复制同步借用的事件和读取 RNG 位置，不消费 RNG、重算 effect 或写 Game。 */
static int observe_physical(const fd2_field_game *game,
        const fd2_field_physical_event *event, void *userdata) {
    physical_observer *observer = userdata;
    if (!game || !event || !observer || observer->count >= 8 ||
        event->actor_index >= game->units.count || event->target_index >= game->units.count) {
        if (observer) observer->invalid = 1;
        return -1;
    }
    physical_observation *seen = &observer->events[observer->count++];
    seen->phase = event->phase;
    seen->actor_index = event->actor_index;
    seen->target_index = event->target_index;
    seen->counter = event->counterattack;
    seen->strike = event->strike_index;
    seen->status_applied = event->pre_hit_status_applied;
    seen->failed = event->action_failed;
    seen->failure = game->failure;
    seen->rng_cursor = observer->rng->cursor;
    seen->actor_hp_after = game->units.items[event->actor_index].hp;
    seen->target_hp_after = game->units.items[event->target_index].hp;
    seen->target_status_after = game->units.items[event->target_index].detail_status[0];
    if (event->phase == FD2_FIELD_PHYSICAL_PREPARE) {
        observer->prepares++;
        if (event->actor_before || event->target_before || event->result) observer->invalid = 1;
    } else if (event->phase == FD2_FIELD_PHYSICAL_STRIKE) {
        observer->strikes++;
        if (!event->actor_before || !event->target_before || !event->result) {
            observer->invalid = 1;
            return -1;
        }
        seen->actor_before = *event->actor_before;
        seen->target_before = *event->target_before;
        seen->result = *event->result;
    } else if (event->phase == FD2_FIELD_PHYSICAL_FINISH) observer->finishes++;
    else observer->invalid = 1;
    if (observer->invalid) return -1;
    if (event->phase == FD2_FIELD_PHYSICAL_FINISH && observer->fail_cleanup) return -1;
    return (int)event->phase == observer->fail_phase &&
        (event->phase != FD2_FIELD_PHYSICAL_STRIKE ||
         observer->strikes == (size_t)observer->fail_strike) ? -1 : 0;
}

static void attach_observer(fd2_field_game *game, physical_observer *observer,
                            test_rng *rng) {
    memset(observer, 0, sizeof(*observer));
    observer->rng = rng;
    observer->fail_phase = -1;
    game->physical_present = observe_physical;
    game->physical_present_userdata = observer;
}

static int begin_observed_attack(fd2_field_game *game, test_rng *rng,
                                  physical_observer *observer) {
    init_game(game);
    game->units.items[0].hp = game->units.items[0].hp_max = 100;
    game->units.items[1].hp = game->units.items[1].hp_max = 100;
    game->units.items[0].defense = game->units.items[1].defense = 10;
    equip_weapon(&game->units.items[0], 71);
    attach_observer(game, observer, rng);
    return configure(game, rng) || fd2_field_game_begin_attack(game) ? -1 : 0;
}

static int test_full_strike_events_and_short_selection(void) {
    static const uint32_t rolls[] = {2, 0, 99, 0, 0, 99, 0, 0, 99, 0, 55};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    fd2_field_game game;
    physical_observer observer;
    if (begin_observed_attack(&game, &rng, &observer) != 0) return -1;
    if (expect(game.option_values[2] == 0 && fd2_field_game_resolve_attack(&game) == 0,
               "FULL option resolves type3 plus random third strike") ||
        expect(observer.count == 5 && observer.prepares == 1 && observer.strikes == 3 &&
               observer.finishes == 1 && !observer.invalid,
               "one prepare, three committed strike events, one finish") ||
        expect(observer.events[0].rng_cursor == 0 && observer.events[4].rng_cursor == 10,
               "prepare is before sequence RNG; finish follows all committed RNG") ||
        expect(game.last_attack_strikes == 3 && rng.cursor == 10 &&
               game.units.items[1].hp == 73 && fd2_field_unit_has_acted(&game.units.items[0]),
               "FULL three intermediate HP commits are retained")) return -1;
    for (size_t i = 0; i < 3; i++) {
        const physical_observation *seen = &observer.events[i + 1];
        if (expect(seen->phase == FD2_FIELD_PHYSICAL_STRIKE && seen->actor_index == 0 &&
                   seen->target_index == 1 && !seen->counter && seen->strike == i &&
                   !seen->status_applied && seen->rng_cursor == 4 + 3 * i &&
                   seen->actor_before.hp == 100 && seen->target_before.hp == 100 - 9 * i &&
                   seen->result.hit && seen->result.damage == 9 &&
                   seen->result.hp_after == 91 - 9 * i && seen->target_hp_after == 91 - 9 * i,
                   "STRIKE receives its own before pair, index and already committed result")) return -1;
    }
    rng.cursor = 0;
    if (begin_observed_attack(&game, &rng, &observer) != 0) return -1;
    game.option_values[2] = 1; /* 本例明确验证原 SHORT 选项，而非统一修改旧夹具。 */
    if (expect(fd2_field_game_resolve_attack(&game) == 0 && game.last_attack_strikes == 2 &&
               game.units.items[1].hp == 82 && rng.cursor == 7 && observer.count == 0,
               "SHORT retains two-strike logic and never invokes the FULL presenter")) return -1;
    rng.cursor = 0;
    if (begin_observed_attack(&game, &rng, &observer) != 0) return -1;
    equip_weapon(&game.units.items[1], 0);
    game.units.items[1].hp = 1;
    if (expect(fd2_field_game_resolve_attack(&game) == 0 && game.last_attack_strikes == 1 &&
               !game.last_counterattack_valid && rng.cursor == 4 && observer.count == 3 &&
               observer.strikes == 1 && observer.finishes == 1 &&
               observer.events[1].target_before.hp == 1 && observer.events[1].result.defeated &&
               observer.events[1].target_hp_after == 0 && game.units.items[1].flags == 1,
               "FULL lethal first strike still finishes exactly once and suppresses remaining strikes/counter")) return -1;
    return 0;
}

static int test_full_status_before_and_counter_order(void) {
    static const uint32_t rolls[] = {99, 9, 3, 0, 99, 0,
                                    2, 0, 99, 0, 0, 99, 0, 0, 99, 0, 55};
    test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
    fd2_field_game game;
    physical_observer observer;
    if (begin_observed_attack(&game, &rng, &observer) != 0) return -1;
    equip_weapon(&game.units.items[0], 4);
    equip_weapon(&game.units.items[1], 71);
    if (expect(fd2_field_game_resolve_attack(&game) == 0 && observer.count == 6 &&
               observer.strikes == 4 && observer.finishes == 1 && !observer.invalid,
               "initial status strike precedes the complete counter sequence") ||
        expect(observer.events[1].status_applied && observer.events[1].target_before.detail_status[0] == 0 &&
               observer.events[1].target_status_after == 5 && observer.events[1].rng_cursor == 6,
               "event preserves pre-effect state while Game already contains status and damage") ||
        expect(game.last_attack_strikes == 1 && game.last_counterattack_strikes == 3 &&
               game.units.items[0].hp == 73 && game.units.items[1].hp == 91 && rng.cursor == 16 &&
               !fd2_field_unit_has_acted(&game.units.items[1]),
               "counter uses its own FULL count and does not spend the defender action")) return -1;
    for (size_t i = 0; i < 3; i++) {
        const physical_observation *seen = &observer.events[i + 2];
        if (expect(seen->counter == 1 && seen->actor_index == 1 && seen->target_index == 0 &&
                   seen->strike == i && !seen->status_applied &&
                   seen->actor_before.detail_status[0] == 5 && seen->actor_before.hp == 91 &&
                   seen->target_before.hp == 100 - 9 * i && seen->rng_cursor == 10 + 3 * i,
                   "counter event indexes restart and preserve the earlier status write")) return -1;
    }
    return 0;
}

static int test_presenter_error_boundaries(void) {
    static const uint32_t rolls[] = {2, 0, 99, 0, 0, 99, 0, 0, 99, 0, 55};
    const int failures[] = {FD2_FIELD_PHYSICAL_PREPARE,
                           FD2_FIELD_PHYSICAL_STRIKE, FD2_FIELD_PHYSICAL_FINISH};
    for (size_t i = 0; i < sizeof(failures) / sizeof(failures[0]); i++) {
        test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
        fd2_field_game game;
        physical_observer observer;
        if (begin_observed_attack(&game, &rng, &observer) != 0) return -1;
        observer.fail_phase = failures[i];
        observer.fail_strike = 2;
        observer.fail_cleanup = failures[i] == FD2_FIELD_PHYSICAL_STRIKE;
        fd2_field_game before = game;
        if (expect(fd2_field_game_resolve_attack(&game) == -1 && !observer.invalid,
                   "presenter failure is returned without retrying or swallowing the error")) return -1;
        if (failures[i] == FD2_FIELD_PHYSICAL_PREPARE) {
            if (expect(memcmp(&game, &before, sizeof(game)) == 0 && rng.cursor == 0 &&
                       observer.count == 1 && observer.finishes == 0,
                       "PREPARE failure preserves whole Game and RNG, with no FINISH")) return -1;
        } else {
            size_t strikes = failures[i] == FD2_FIELD_PHYSICAL_STRIKE ? 2 : 3;
            if (expect(observer.strikes == strikes && observer.finishes == 1 &&
                       rng.cursor == 1 + 3 * strikes && game.units.items[1].hp == 100 - 9 * strikes &&
                       game.failure == FD2_FIELD_FAILURE_ACTION && game.failure_notice.unit_index == 0,
                       "post-commit failure retains HP/RNG and latches the original action failure") ||
                expect(observer.events[observer.count - 1].failed ==
                           (failures[i] == FD2_FIELD_PHYSICAL_STRIKE),
                       "cleanup FINISH marks a preceding strike failure exactly once")) return -1;
            if (failures[i] == FD2_FIELD_PHYSICAL_STRIKE &&
                expect(observer.events[observer.count - 1].failure == FD2_FIELD_FAILURE_ACTION,
                       "cleanup error cannot replace the already latched first error")) return -1;
            before = game;
            size_t events = observer.count;
            if (expect(fd2_field_game_resolve_attack(&game) == -1 &&
                       memcmp(&game, &before, sizeof(game)) == 0 && observer.count == events &&
                       rng.cursor == 1 + 3 * strikes,
                       "failed committed action cannot be retried or FINISH called twice")) return -1;
        }
        fd2_field_path_close(&game.move_range);
    }
    return 0;
}

static int test_mode_aware_unknown_loot_preflight(void) {
    static const uint32_t rolls[] = {2, 0, 99, 0, 0, 99, 0,
                                    2, 0, 99, 0, 0, 99, 0, 55};
    for (int counter = 0; counter < 2; counter++) for (int type3 = 0; type3 < 2; type3++) {
        test_rng rng = {rolls, sizeof(rolls) / sizeof(rolls[0]), 0};
        fd2_field_game game;
        physical_observer observer;
        if (begin_observed_attack(&game, &rng, &observer) != 0) return -1;
        equip_weapon(&game.units.items[0], !counter && type3 ? 71 : 0);
        if (counter) equip_weapon(&game.units.items[1], type3 ? 71 : 0);
        game.units.items[0].defense = game.units.items[1].defense = 0;
        size_t victim = counter ? 0 : 1;
        /* 地形后 AP21/DP0，单击实际18、最大19：45>2*19，而三击可致死。
         * 无type3实际两击后HP9，不能粗略用FULL三击上界误拒。 */
        game.units.items[victim].hp = game.units.items[victim].hp_max = 45;
        ((uint8_t *)&game.units.items[victim])[0x31] = 9; /* 未支持 descriptor，不能进 RNG 后才拒。 */
        fd2_field_game before = game;
        int result = fd2_field_game_resolve_attack(&game);
        if (type3) {
            if (expect(result == -1 && rng.cursor == 0 && observer.count == 0 &&
                       memcmp(&game, &before, sizeof(game)) == 0,
                       "unknown loot at HP between two/three maximum hits rejects before PREPARE/RNG for either side")) return -1;
        } else if (expect(result == 0 && game.units.items[victim].hp == 9 &&
                          observer.prepares == 1 && observer.finishes == 1,
                          "non-type3 FULL uses two-hit upper bound instead of falsely rejecting at three")) return -1;
        fd2_field_path_close(&game.move_range);
    }
    return 0;
}

static int test_load_preserves_physical_present_owner(void) {
    fd2_archive fdother = {0};
    fd2_field_game game = {0}, next = {0};
    test_rng rng = {NULL, 0, 0};
    physical_observer observer;
    fd2_save_battle_snapshot snapshot;
    int result = -1;
    if (expect(fd2_archive_open(&fdother, "original_game/FDOTHER.DAT") == 0,
               "owner fixture reads FDOTHER from the source-root cwd") ||
        expect(fd2_field_game_open_resources(&game, &fdother, 0) == 0,
               "owner fixture opens stage0 resources") ||
        expect(fd2_field_game_load_active(&game, "original_game/START.SAV") == 0,
               "owner fixture loads actual START") ||
        expect(fd2_field_game_open_resources(&next, &fdother, 1) == 0,
               "owner fixture opens stage1 resources")) goto done;
    /* 仅隔离跨stage的owner交接，不冒称原32d18部署或原版运行证据。 */
    next.units = game.units;
    next.roster_count = game.roster_count;
    memcpy(next.roster, game.roster, sizeof(next.roster));
    for (size_t i = 0; i < next.units.count; i++) {
        next.units.items[i].x = (uint8_t)(2 + i % 8);
        next.units.items[i].y = (uint8_t)(10 + i / 8);
    }
    next.camera_cell_x = 0; next.camera_cell_y = 8;
    next.cursor_cell_x = 5; next.cursor_cell_y = 13;
    attach_observer(&game, &observer, &rng);
    game.attack_rng = next_roll;
    game.attack_rng_userdata = &rng;
    if (expect(fd2_field_game_export_battle_snapshot(&next, &snapshot) == 0,
               "owner fixture exports its controlled stage1 snapshot") ||
        expect(fd2_field_game_import_battle_snapshot(&game, &snapshot) == 0,
               "owner fixture imports stage1 transactionally")) goto done;
    result = expect(game.stage == 1 && game.physical_present == observe_physical &&
        game.physical_present_userdata == &observer && game.attack_rng_userdata == &rng &&
        observer.count == 0 && rng.cursor == 0,
        "successful cross-stage Load carries borrowed physical callback/context without calling it or RNG");
done:
    fd2_field_game_close(&next);
    fd2_field_game_close(&game);
    fd2_archive_close(&fdother);
    return result;
}

int main(void) {
#define RUN_TEST(test) do { if (test() != 0) { \
    fprintf(stderr, "field game attack case failed: %s\n", #test); \
    return 1; } } while (0)
    RUN_TEST(test_full_strike_events_and_short_selection);
    RUN_TEST(test_full_status_before_and_counter_order);
    RUN_TEST(test_presenter_error_boundaries);
    RUN_TEST(test_mode_aware_unknown_loot_preflight);
    RUN_TEST(test_load_preserves_physical_present_owner);
    RUN_TEST(test_weapon_required_with_range_hook);
    RUN_TEST(test_target_focus_and_cycle);
    RUN_TEST(test_target_selection_rejection_is_atomic);
    RUN_TEST(test_illegal_and_cancel);
    RUN_TEST(test_acted_preflight_rollback);
    RUN_TEST(test_hit_via_confirm_flow);
    RUN_TEST(test_pre_hit_effect_order);
    RUN_TEST(test_profile_critical_table);
    RUN_TEST(test_weapon_critical_bonus);
    RUN_TEST(test_unknown_terrain_class_fails_before_rng);
    RUN_TEST(test_terrain_modifiers_in_exchange);
    RUN_TEST(test_double_strike_sequence);
    RUN_TEST(test_lethal_first_strike_stops_exchange);
    RUN_TEST(test_adjacent_counterattack);
    RUN_TEST(test_command_menu_model);
    RUN_TEST(test_miss_completes_action);
#undef RUN_TEST
    return 0;
}
