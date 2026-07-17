/* M8 正式战场 session 的系统菜单集成测试。
 *
 * 覆盖 field controller code0 0x17e7 到 empty-focus dispatcher
 * code0 0x6f55 的入口边界：玩家 phase、browse、焦点无可见 actor。
 * 菜单对象以外的 session 字节逐字不变，保证 pending 分派和分层取消
 * 不推进回合、不提交单位／事件／镜头状态，也不消费外部 RNG。
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_game.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

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

static void init_game(fd2_field_game *game, test_rng *rng) {
    memset(game, 0, sizeof(*game));
    game->ready = 1;
    game->active_side = 2;
    game->interaction = FD2_FIELD_INTERACTION_BROWSE;
    game->selected_unit = -1;
    game->detail_unit = -1;
    game->attack_target = -1;
    game->map.width = 24;
    game->map.height = 24;
    game->map.cells = calloc((size_t)game->map.width * game->map.height,
                             sizeof(*game->map.cells));
    game->terrain.attrs = calloc(1, sizeof(*game->terrain.attrs));
    game->terrain.attr_count = 1;
    game->camera_cell_x = 3;
    game->camera_cell_y = 7;
    game->cursor_cell_x = 11;
    game->cursor_cell_y = 13;
    game->turn_number = 9;
    game->phase_unit_cursor = 4;
    game->phase_ai_pass = 1;
    game->phase_next_action_ms = 12345;
    game->event_log_count = 1;
    game->event_total_count = 17;
    game->unhandled_event_count = 3;
    game->event_log[0].kind = FD2_FIELD_EVENT_CELL;
    game->event_log[0].turn_number = 8;
    game->event_log[0].action = 0x42;
    game->battle_result = FD2_FIELD_BATTLE_ONGOING;
    game->save_resource_available = 1;
    game->option_values[0] = 1;
    game->option_values[1] = 1;
    game->option_values[2] = 0;
    game->option_values[3] = 1;
    game->units.count = 2;
    game->units.items[0].unit_id = 7;
    game->units.items[0].side = 2;
    game->units.items[0].x = 4;
    game->units.items[0].y = 5;
    game->units.items[0].hp = 20;
    game->units.items[0].hp_max = 20;
    game->units.items[1].unit_id = 19;
    game->units.items[1].side = 0;
    game->units.items[1].x = 18;
    game->units.items[1].y = 6;
    game->units.items[1].hp = 12;
    game->units.items[1].hp_max = 12;
    game->attack_rng = unused_rng;
    game->attack_rng_userdata = rng;
    fd2_field_system_menu_init(&game->system_menu);
}

/* system_menu、last_system_action 和 interaction 是本流程唯一允许变化的
 * session 字段。将它们归一化后比较整个对象，覆盖未来新增业务字段。 */
static int business_state_unchanged(const fd2_field_game *game,
                                    const fd2_field_game *before) {
    fd2_field_game normalized = *game;
    normalized.system_menu = before->system_menu;
    normalized.last_system_action = before->last_system_action;
    normalized.interaction = before->interaction;
    return memcmp(&normalized, before, sizeof(normalized)) == 0;
}

static int test_focus_cycle_and_confirm(void) {
    fd2_field_game game;
    test_rng rng = {0x31415926u, 0};
    init_game(&game, &rng);

    game.units.count = 6;
    for (size_t i = 0; i < game.units.count; i++) {
        game.units.items[i].x = (uint8_t)(2 + i);
        game.units.items[i].y = (uint8_t)(4 + i);
        game.units.items[i].hp = 10;
        game.units.items[i].hp_max = 10;
        game.units.items[i].movement_profile = 0;
        game.units.items[i].movement_points = 3;
    }
    game.units.items[0].side = 0;
    game.units.items[1].side = 2;
    game.units.items[1].flags = FD2_FIELD_UNIT_FLAG_HIDDEN;
    game.units.items[2].side = 2;
    game.units.items[2].flags = FD2_FIELD_UNIT_FLAG_AI_INELIGIBLE;
    game.units.items[3].side = 2;
    game.units.items[3].flags = FD2_FIELD_UNIT_FLAG_ACTED;
    game.units.items[4].side = 2;
    game.units.items[5].side = 2;

    CHECK(fd2_field_game_cycle_focus(&game) == 4);
    CHECK(game.cursor_cell_x == game.units.items[4].x &&
          game.cursor_cell_y == game.units.items[4].y &&
          game.focus_cycle_unit == 5);
    CHECK(fd2_field_game_cycle_focus(&game) == 5);
    CHECK(game.focus_cycle_unit == 0);
    CHECK(fd2_field_game_cycle_focus(&game) == 4);

    /* field controller @code0 0x17e7：普通可操作角色上的第一次确认
     * 直接进入移动／操作路径，不先打开个人详情。 */
    game.cursor_cell_x = game.units.items[4].x;
    game.cursor_cell_y = game.units.items[4].y;
    CHECK(fd2_field_game_confirm_cursor(&game) == 4);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_UNIT_SELECTED);
    CHECK(game.selected_unit == 4 && !game.detail_visible);
    fd2_field_path_close(&game.move_range);
    free(game.move_path);
    free(game.map.cells);
    free(game.terrain.attrs);
    return 0;
}

static int test_open_gate(void) {
    fd2_field_game game;
    test_rng rng = {0x12345678u, 0};
    init_game(&game, &rng);

    game.ready = 0;
    CHECK(fd2_field_game_open_system_menu(&game) == -1);
    game.ready = 1;
    game.active_side = 0;
    CHECK(fd2_field_game_open_system_menu(&game) == -1);
    game.active_side = 2;
    game.interaction = FD2_FIELD_INTERACTION_UNIT_SELECTED;
    CHECK(fd2_field_game_open_system_menu(&game) == -1);
    game.interaction = FD2_FIELD_INTERACTION_BROWSE;

    game.cursor_cell_x = game.units.items[0].x;
    game.cursor_cell_y = game.units.items[0].y;
    CHECK(fd2_field_game_open_system_menu(&game) == -1);
    /* HP==0 但尚未 hidden 的 actor 仍可见，不能把“空焦点”扩大为
     * “没有存活 actor”。 */
    game.units.items[0].hp = 0;
    CHECK(fd2_field_game_open_system_menu(&game) == -1);
    fd2_field_unit_set_hidden(&game.units.items[0], 1);

    fd2_field_game before = game;
    CHECK(fd2_field_game_open_system_menu(&game) == 0);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU);
    CHECK(game.system_menu.page == FD2_FIELD_SYSTEM_PAGE_PARENT);
    CHECK(game.system_menu.selected == 0);
    CHECK(business_state_unchanged(&game, &before));
    CHECK(rng.calls == 0 && rng.state == 0x12345678u);
    return 0;
}

static int test_layered_cancel(void) {
    fd2_field_game game;
    test_rng rng = {0x89abcdefu, 0};
    init_game(&game, &rng);
    fd2_field_game before = game;
    CHECK(fd2_field_game_open_system_menu(&game) == 0);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_SECONDARY);
    CHECK(game.system_menu.page == FD2_FIELD_SYSTEM_PAGE_SECONDARY);
    CHECK(fd2_field_game_cancel_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_BACK);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU);
    CHECK(game.system_menu.page == FD2_FIELD_SYSTEM_PAGE_PARENT);
    CHECK(fd2_field_game_cancel_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_CANCEL);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_BROWSE);
    CHECK(business_state_unchanged(&game, &before));
    CHECK(rng.calls == 0 && rng.state == 0x89abcdefu);

    CHECK(fd2_field_game_open_system_menu(&game) == 0);
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_RIGHT) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_OPTIONS);
    CHECK(fd2_field_game_cancel_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_BACK);
    CHECK(fd2_field_game_cancel_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_CANCEL);
    CHECK(business_state_unchanged(&game, &before));
    return 0;
}

static int test_secondary_dynamic_disabled(void) {
    fd2_field_game game;
    test_rng rng = {0x42424242u, 0};
    init_game(&game, &rng);
    fd2_field_game_set_save_resource_available(&game, 0);
    CHECK(fd2_field_game_open_system_menu(&game) == 0);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_SECONDARY);
    CHECK(!game.system_menu.disabled[0]);
    CHECK(!game.system_menu.disabled[1]);
    CHECK(game.system_menu.disabled[2]);
    CHECK(!game.system_menu.disabled[3]);
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_RIGHT) == 0);
    CHECK(game.system_menu.selected == 0);
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_LEFT) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION);
    CHECK(fd2_field_game_cancel_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_BACK);
    CHECK(game.system_menu.disabled[2]);
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_DOWN) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION);
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_RIGHT) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_LEAVE_BATTLE);
    CHECK(game.system_menu.disabled[2]);
    CHECK(fd2_field_game_cancel_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_BACK);
    fd2_field_game_set_save_resource_available(&game, 1);
    fd2_field_unit_set_acted(&game.units.items[0], 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_SECONDARY);
    CHECK(game.system_menu.disabled[1]);
    CHECK(!game.system_menu.disabled[2]);
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_LEFT) == 0);
    CHECK(game.system_menu.selected == 0);

    /* hidden actor 的 stale acted bit 不触发原版动态门。 */
    CHECK(fd2_field_game_cancel_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_BACK);
    fd2_field_unit_set_hidden(&game.units.items[0], 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_SECONDARY);
    CHECK(!game.system_menu.disabled[1]);
    CHECK(!game.system_menu.disabled[2]);
    CHECK(rng.calls == 0 && rng.state == 0x42424242u);
    return 0;
}

static int test_manual_slot_interaction_is_independent(void) {
    static const char path[] = "/tmp/fd2sdl-field-manual-slot-missing.sav";
    (void)remove(path);
    fd2_field_game game;
    test_rng rng = {0x10203040u, 0};
    init_game(&game, &rng);
    CHECK(fd2_field_game_open_system_menu(&game) == 0);
    fd2_field_system_menu before_menu = game.system_menu;
    CHECK(fd2_field_game_open_manual_slot_picker(
              &game, path, FD2_FIELD_MANUAL_SLOT_MODE_SAVE, 1) == 0);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_MANUAL_SLOT);
    CHECK(game.manual_slot_picker.open &&
          game.manual_slot_picker.selected == 1);
    CHECK(!game.manual_slot_picker.occupied[0] &&
          !game.manual_slot_picker.occupied[1] &&
          !game.manual_slot_picker.occupied[2] &&
          !game.manual_slot_picker.occupied[3]);
    CHECK(fd2_field_game_move_manual_slot_picker(&game, 1) == 1);
    CHECK(game.manual_slot_picker.selected == 2);
    CHECK(memcmp(&game.system_menu, &before_menu,
                 sizeof(before_menu)) == 0);
    CHECK(fd2_field_game_confirm_manual_slot_picker(&game) == 2);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU);
    CHECK(!game.manual_slot_picker.open);

    CHECK(fd2_field_game_open_manual_slot_picker(
              &game, path, FD2_FIELD_MANUAL_SLOT_MODE_LOAD, 0) == 0);
    CHECK(fd2_field_game_confirm_manual_slot_picker(&game) == -1);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_MANUAL_SLOT);
    CHECK(fd2_field_game_cancel_manual_slot_picker(&game) == 1);
    CHECK(game.interaction == FD2_FIELD_INTERACTION_SYSTEM_MENU);
    CHECK(rng.calls == 0 && rng.state == 0x10203040u);
    return 0;
}

static int test_highlight_state_is_independent(void) {
    fd2_field_game game;
    test_rng rng = {0x27182818u, 0};
    init_game(&game, &rng);
    game.command_highlight_phase = 1;
    game.last_command_highlight_ms = 77;
    CHECK(fd2_field_game_open_system_menu(&game) == 0);
    CHECK(game.system_menu.highlight_phase == 0);
    fd2_field_game_tick(&game, 1000);
    CHECK(game.command_highlight_phase == 0 &&
          game.last_command_highlight_ms == 0);
    CHECK(game.system_menu.highlight_phase == 0 &&
          game.system_menu.last_highlight_ms == 1000);
    fd2_field_game_tick(&game, 1220);
    CHECK(game.system_menu.highlight_phase == 1 &&
          game.system_menu.last_highlight_ms == 1220);
    fd2_field_game_tick(&game, 1440);
    CHECK(game.system_menu.highlight_phase == 0 &&
          game.system_menu.last_highlight_ms == 1440);
    CHECK(rng.calls == 0 && rng.state == 0x27182818u);
    return 0;
}

static int test_options_toggle_is_menu_local(void) {
    fd2_field_game game;
    test_rng rng = {0x13579bdfu, 0};
    init_game(&game, &rng);
    fd2_field_game before = game;
    CHECK(fd2_field_game_open_system_menu(&game) == 0);
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_RIGHT) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_OPTIONS);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_OPTIONS_TOGGLE_PENDING);
    CHECK(game.system_menu.option_values[0] == 0);
    CHECK(game.system_menu.command_ids[0] == 19);
    CHECK(game.option_values[0] == 0);
    fd2_field_game normalized = game;
    normalized.option_values[0] = before.option_values[0];
    CHECK(business_state_unchanged(&normalized, &before));
    CHECK(rng.calls == 0 && rng.state == 0x13579bdfu);
    return 0;
}

static int test_pending_actions_are_non_destructive(void) {
    fd2_field_game game;
    test_rng rng = {0x31415926u, 0};
    init_game(&game, &rng);
    fd2_field_game before = game;

    CHECK(fd2_field_game_open_system_menu(&game) == 0);
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_LEFT) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION);
    CHECK(game.system_menu.page == FD2_FIELD_SYSTEM_PAGE_CONFIRMATION);
    CHECK(game.system_menu.confirmation_prompt_fragment == 0x1a1);
    CHECK(business_state_unchanged(&game, &before));
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_RIGHT) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_MARCH_CONFIRM);
    CHECK(game.system_menu.page == FD2_FIELD_SYSTEM_PAGE_PARENT);
    CHECK(business_state_unchanged(&game, &before));
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_DOWN) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION);
    CHECK(game.system_menu.confirmation_prompt_fragment == 0x1a3);
    CHECK(business_state_unchanged(&game, &before));
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_RIGHT) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_END_TURN_CONFIRM);
    CHECK(game.system_menu.page == FD2_FIELD_SYSTEM_PAGE_PARENT);
    CHECK(business_state_unchanged(&game, &before));

    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_UP) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_SECONDARY);
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_LEFT) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION);
    CHECK(game.system_menu.confirmation_prompt_fragment == 0x19a);
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_RIGHT) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_SAVE);
    CHECK(game.system_menu.page == FD2_FIELD_SYSTEM_PAGE_SECONDARY);
    CHECK(business_state_unchanged(&game, &before));
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_RIGHT) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION);
    CHECK(game.system_menu.confirmation_prompt_fragment == 0x19d);
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_RIGHT) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_LOAD);
    CHECK(game.system_menu.page == FD2_FIELD_SYSTEM_PAGE_SECONDARY);
    CHECK(business_state_unchanged(&game, &before));
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_DOWN) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION);
    CHECK(game.system_menu.confirmation_prompt_fragment == 0x19f);
    CHECK(fd2_field_game_select_system_menu_direction(
              &game, FD2_FIELD_COMMAND_DIRECTION_RIGHT) == 1);
    CHECK(fd2_field_game_confirm_system_menu(&game) ==
          FD2_FIELD_SYSTEM_ACTION_LEAVE_BATTLE);
    CHECK(game.system_menu.page == FD2_FIELD_SYSTEM_PAGE_SECONDARY);
    CHECK(business_state_unchanged(&game, &before));
    CHECK(rng.calls == 0 && rng.state == 0x31415926u);
    return 0;
}

int main(void) {
    if (test_focus_cycle_and_confirm() != 0 || test_open_gate() != 0 ||
        test_layered_cancel() != 0 ||
        test_secondary_dynamic_disabled() != 0 ||
        test_manual_slot_interaction_is_independent() != 0 ||
        test_highlight_state_is_independent() != 0 ||
        test_options_toggle_is_menu_local() != 0 ||
        test_pending_actions_are_non_destructive() != 0)
        return 1;
    puts("field_game_system_menu_test: ok");
    return 0;
}
