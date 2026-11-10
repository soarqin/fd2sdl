#include <stdio.h>
#include <string.h>

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
    CHECK(menu.animation_opening == 0 && menu.animation_phase == 0);
    CHECK(menu.highlight_phase == 0 && menu.last_highlight_ms == 0);
    CHECK(menu.command_ids[0] == 7 && menu.command_ids[1] == 5);
    CHECK(menu.command_ids[2] == 6 && menu.command_ids[3] == 4);

    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_SECONDARY);
    CHECK(menu.page == FD2_FIELD_SYSTEM_PAGE_SECONDARY);
    CHECK(menu.animation_opening == 0 && menu.animation_phase == 0);
    CHECK(menu.command_ids[0] == 12 && menu.command_ids[3] == 15);

    CHECK(fd2_field_system_menu_cancel(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_BACK);
    CHECK(menu.page == FD2_FIELD_SYSTEM_PAGE_PARENT);
    CHECK(menu.animation_opening == 0 && menu.animation_phase == 0);
    CHECK(fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_RIGHT));
    CHECK(menu.selected == 2);
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_OPTIONS);
    CHECK(menu.page == FD2_FIELD_SYSTEM_PAGE_OPTIONS);
    CHECK(menu.animation_opening == 0 && menu.animation_phase == 0);
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
    CHECK(menu.confirmation_yes && menu.selected == 0);
    CHECK(!fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_LEFT));
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
    CHECK(menu.confirmation_yes && menu.selected == 0);
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_LOAD);
    CHECK(menu.page == FD2_FIELD_SYSTEM_PAGE_SECONDARY);
    CHECK(fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_DOWN));
    CHECK(menu.selected == 3);
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION);
    CHECK(menu.confirmation_yes && menu.selected == 0);
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

static int test_confirmation_selection(void) {
    fd2_field_system_menu menu;
    fd2_field_system_menu_init(&menu);
    menu.selected = 3;
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION);
    CHECK(menu.confirmation_yes && menu.selected == 0);
    CHECK(!fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_DOWN));
    CHECK(fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_RIGHT));
    CHECK(!menu.confirmation_yes && menu.selected == 1);
    CHECK(!fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_UP));
    CHECK(!fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_RIGHT));
    CHECK(fd2_field_system_menu_confirm(&menu) == FD2_FIELD_SYSTEM_ACTION_BACK);
    CHECK(menu.page == FD2_FIELD_SYSTEM_PAGE_PARENT && menu.selected == 3);
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION);
    CHECK(menu.confirmation_yes && menu.selected == 0);
    CHECK(fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_RIGHT));
    CHECK(fd2_field_system_menu_select_direction(
              &menu, FD2_FIELD_COMMAND_DIRECTION_LEFT));
    CHECK(menu.confirmation_yes && menu.selected == 0);
    CHECK(fd2_field_system_menu_cancel(&menu) == FD2_FIELD_SYSTEM_ACTION_BACK);
    CHECK(menu.page == FD2_FIELD_SYSTEM_PAGE_PARENT && menu.selected == 3);
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION);
    CHECK(fd2_field_system_menu_confirm(&menu) ==
          FD2_FIELD_SYSTEM_ACTION_END_TURN_CONFIRM);
    return 0;
}

static int test_raw_options_and_cancel(void) {
    const uint8_t raw_values[] = {0, 1, 0xfe, 0xff};
    for (size_t item = 0; item < 4; item++) for (size_t i = 0; i < 4; i++) {
        fd2_field_system_menu menu;
        fd2_field_system_menu_init(&menu);
        uint8_t values[4] = {0xfe, 0xff, 0xfe, 0xff}; values[item] = raw_values[i];
        fd2_field_system_menu_set_options(&menu, values);
        CHECK(memcmp(menu.option_values, values, sizeof(values)) == 0);
        menu.selected = 2;
        CHECK(fd2_field_system_menu_confirm(&menu) == FD2_FIELD_SYSTEM_ACTION_OPEN_OPTIONS);
        menu.selected = (uint8_t)item;
        CHECK(menu.command_ids[item] == (uint8_t)((item == 0 ? 18u : item == 1 ? 20u : item == 2 ? 22u : 24u) +
            (item == 2 ? values[item] != 0u : values[item] == 0u)));
        CHECK(fd2_field_system_menu_confirm(&menu) == FD2_FIELD_SYSTEM_ACTION_OPTIONS_TOGGLE_PENDING);
        values[item] = item < 2u ? (uint8_t)(values[item] == 0u) : (uint8_t)(values[item] ^ 1u);
        CHECK(memcmp(menu.option_values, values, sizeof(values)) == 0);
        CHECK(fd2_field_system_menu_cancel(&menu) == FD2_FIELD_SYSTEM_ACTION_BACK);
        CHECK(memcmp(menu.option_values, values, sizeof(values)) == 0);
    }
    return 0;
}

static int test_original_shared_highlight_clock(void) {
    uint32_t phase=0,tick=0;
    CHECK(fd2_field_system_menu_highlight_tick(&phase,&tick,3,0)==0 && phase==0 && tick==0);
    CHECK(fd2_field_system_menu_highlight_tick(&phase,&tick,4,0)==1 && phase==1 && tick==4);
    CHECK(fd2_field_system_menu_highlight_tick(&phase,&tick,400,0)==1 && phase==0 && tick==400);
    CHECK(fd2_field_system_menu_highlight_tick(&phase,&tick,400,0)==0 && phase==0 && tick==400);
    phase=1;
    CHECK(fd2_field_system_menu_highlight_tick(&phase,&tick,402,1)==1 && phase==2 && tick==402);
    CHECK(fd2_field_system_menu_highlight_tick(&phase,&tick,403,1)==0 && phase==2 && tick==402);
    CHECK(fd2_field_system_menu_highlight_tick(&phase,&tick,404,1)==1 && phase==3 && tick==404);
    CHECK(fd2_field_system_menu_highlight_tick(&phase,&tick,600,1)==1 && phase==0 && tick==600);
    phase=3;fd2_field_system_menu_highlight_finish(&phase);CHECK(phase==0 && tick==600);
    CHECK(fd2_field_system_menu_highlight_tick(&phase,&tick,603,0)==0 && phase==0 && tick==600);
    tick=UINT32_MAX-1u;CHECK(fd2_field_system_menu_highlight_tick(&phase,&tick,1,1)==1 && phase==1 && tick==1);
    phase=0;tick=32767u;
    CHECK(fd2_field_system_menu_highlight_tick(&phase,&tick,32768u,0)==1 && phase==1 && tick==0xffff8000u);
    phase=0;tick=UINT32_MAX;
    CHECK(fd2_field_system_menu_highlight_tick(&phase,&tick,0,1)==0 && phase==0 && tick==UINT32_MAX);
    CHECK(fd2_field_system_menu_highlight_tick(&phase,&tick,1,1)==1 && phase==1 && tick==1);
    phase=0;tick=10;
    CHECK(fd2_field_system_menu_highlight_tick(&phase,&tick,9,0)==1 && phase==1 && tick==9);
    uint32_t old_phase=phase,old_tick=tick;
    CHECK(fd2_field_system_menu_highlight_tick(NULL,&tick,100,0)==-1);
    CHECK(fd2_field_system_menu_highlight_tick(&phase,NULL,100,0)==-1);
    CHECK(fd2_field_system_menu_highlight_tick(&phase,&phase,100,0)==-1);
    CHECK(fd2_field_system_menu_highlight_tick(&phase,&tick,100,2)==-1 && phase==old_phase && tick==old_tick);
    return 0;
}

int main(void) {
    if (test_parent_dispatch() != 0 ||
        test_save_load_and_options() != 0 ||
        test_disabled_and_cancel() != 0 ||
        test_raw_options_and_cancel() != 0 ||
        test_original_shared_highlight_clock() != 0 ||
        test_confirmation_selection() != 0)
        return 1;
    puts("field_system_menu_test: ok");
    return 0;
}
