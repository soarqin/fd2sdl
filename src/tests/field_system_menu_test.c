#include <stdio.h>

#include "field_system_menu.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static int test_parent_dispatch(void) {
    fd2_field_system_menu menu;
    fd2_field_system_menu_init(&menu);
    CHECK(menu.page == FD2_FIELD_SYSTEM_PAGE_PARENT);
    CHECK(menu.animation_opening == 1 && menu.animation_phase == 4);
    CHECK(menu.highlight_phase == 0 && menu.last_highlight_ms == 0);
    CHECK(menu.command_ids[0] == 7 && menu.command_ids[1] == 5);
    CHECK(menu.command_ids[2] == 6 && menu.command_ids[3] == 4);

    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_SECONDARY);
    CHECK(menu.page == FD2_FIELD_SYSTEM_PAGE_SECONDARY);
    CHECK(menu.animation_opening == 1 && menu.animation_phase == 4);
    CHECK(menu.command_ids[0] == 12 && menu.command_ids[3] == 15);

    CHECK(fd2_field_system_menu_cancel(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_BACK);
    CHECK(menu.page == FD2_FIELD_SYSTEM_PAGE_PARENT);
    CHECK(fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_RIGHT));
    CHECK(menu.selected == 2);
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_OPTIONS);
    CHECK(menu.page == FD2_FIELD_SYSTEM_PAGE_OPTIONS);
    return 0;
}

static int test_save_load_and_options(void) {
    fd2_field_system_menu menu;
    fd2_field_system_menu_init(&menu);
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_SECONDARY);
    CHECK(fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_LEFT));
    CHECK(menu.selected == 1);
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION);
    CHECK(menu.page == FD2_FIELD_SYSTEM_PAGE_CONFIRMATION);
    CHECK(menu.confirmation_prompt_fragment == 0x19a);
    CHECK(fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_RIGHT));
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_SAVE);
    size_t result_fragment = 0;
    CHECK(fd2_field_system_menu_get_last_result_fragment(
              &menu, 1, &result_fragment) == 0 &&
          result_fragment == FD2_FIELD_SYSTEM_TEXT_SAVE_SUCCESS);
    CHECK(fd2_field_system_menu_get_last_result_fragment(
              &menu, 0, &result_fragment) == 0 &&
          result_fragment == FD2_FIELD_SYSTEM_TEXT_SAVE_FAILURE);
    CHECK(menu.page == FD2_FIELD_SYSTEM_PAGE_SECONDARY);
    CHECK(fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_RIGHT));
    CHECK(menu.selected == 2);
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION);
    CHECK(fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_RIGHT));
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_LOAD);
    CHECK(menu.page == FD2_FIELD_SYSTEM_PAGE_SECONDARY);
    CHECK(fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_DOWN));
    CHECK(menu.selected == 3);
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION);
    CHECK(fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_RIGHT));
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_LEAVE_BATTLE);

    fd2_field_system_menu_init(&menu);
    CHECK(fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_RIGHT));
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_OPTIONS);
    CHECK(menu.command_ids[0] == 18);
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_OPTIONS_TOGGLE_PENDING);
    CHECK(menu.option_values[0] == 0 && menu.command_ids[0] == 19);
    CHECK(fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_RIGHT));
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_OPTIONS_TOGGLE_PENDING);
    CHECK(menu.option_values[2] == 1 && menu.command_ids[2] == 23);
    return 0;
}

static int test_disabled_and_cancel(void) {
    fd2_field_system_menu menu;
    uint8_t disabled[FD2_FIELD_SYSTEM_MENU_COUNT] = {1, 0, 1, 0};
    fd2_field_system_menu_init(&menu);
    fd2_field_system_menu_set_disabled(&menu, disabled);
    CHECK(menu.selected == 1);
    CHECK(!fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_UP));
    CHECK(menu.selected == 1);
    CHECK(fd2_field_system_menu_cancel(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_CANCEL);
    return 0;
}

int main(void) {
    if (test_parent_dispatch() != 0 ||
        test_save_load_and_options() != 0 ||
        test_disabled_and_cancel() != 0)
        return 1;
    puts("field_system_menu_test: ok");
    return 0;
}
