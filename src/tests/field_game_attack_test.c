/* M6 无演出攻击状态机测试。
 * 覆盖 field_physical_exchange @0x3a6a2、field_physical_attack_sequence
 * @0x43a6a 与 field_physical_attack_resolve @0x43edb：目标／武器预检、
 * 状态效果、双击、反击和固定 RNG 顺序。
 */
#include <stdio.h>
#include <string.h>

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
               "illegal target preserves acted bit") ||
        expect(fd2_field_game_cancel_attack(&game) == 1,
               "targeting cancel succeeds") ||
        expect(game.interaction == FD2_FIELD_INTERACTION_COMMAND,
               "cancel returns to command"))
        return -1;
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

int main(void) {
    return test_weapon_required_with_range_hook() ||
                   test_illegal_and_cancel() ||
                   test_acted_preflight_rollback() ||
                   test_hit_via_confirm_flow() ||
                   test_pre_hit_effect_order() ||
                   test_profile_critical_table() ||
                   test_weapon_critical_bonus() ||
                   test_unknown_terrain_class_fails_before_rng() ||
                   test_terrain_modifiers_in_exchange() ||
                   test_double_strike_sequence() ||
                   test_lethal_first_strike_stops_exchange() ||
                   test_adjacent_counterattack() ||
                   test_miss_completes_action()
               ? 1
               : 0;
}
