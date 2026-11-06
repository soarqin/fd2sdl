/* 应用分派回归：保留胜败、游戏内离场、宿主退出与快速检查的区别。 */
#include <stdio.h>

#include "app_flow.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

int main(void) {
    CHECK(fd2_title_action_from_selection(0, 1) ==
          FD2_TITLE_ACTION_NEW_GAME);
    CHECK(fd2_title_action_from_selection(1, 1) ==
          FD2_TITLE_ACTION_LOAD);
    CHECK(fd2_title_action_from_selection(2, 1) ==
          FD2_TITLE_ACTION_CONTINUE);
    CHECK(fd2_title_action_from_selection(1, 0) ==
          FD2_TITLE_ACTION_LOAD);
    CHECK(fd2_title_action_from_selection(0, 0) ==
          FD2_TITLE_ACTION_NEW_GAME);
    CHECK(fd2_title_action_from_selection(-1, 1) ==
          FD2_TITLE_ACTION_HOST_QUIT);
    CHECK(fd2_title_action_from_selection(2, 0) == FD2_TITLE_ACTION_ERROR);
    CHECK(fd2_title_action_from_selection(-2, 1) == FD2_TITLE_ACTION_ERROR);
    CHECK(fd2_app_flow_from_title(FD2_TITLE_ACTION_ERROR) == FD2_APP_FLOW_ERROR);
    CHECK(fd2_app_flow_from_title(FD2_TITLE_ACTION_NEW_GAME) ==
          FD2_APP_FLOW_START_NEW_GAME);
    CHECK(fd2_app_flow_from_title(FD2_TITLE_ACTION_LOAD) ==
          FD2_APP_FLOW_LOAD_GAME);
    CHECK(fd2_app_flow_from_title(FD2_TITLE_ACTION_CONTINUE) ==
          FD2_APP_FLOW_CONTINUE_GAME);
    CHECK(fd2_app_flow_from_title(FD2_TITLE_ACTION_HOST_QUIT) ==
          FD2_APP_FLOW_EXIT);
    CHECK(fd2_app_flow_from_field(FD2_APP_FIELD_LEAVE_GAME) ==
          FD2_APP_FLOW_LEAVE_GAME);
    CHECK(fd2_app_flow_from_field(FD2_APP_FIELD_LEAVE_GAME) !=
          fd2_app_flow_from_field(FD2_APP_FIELD_HOST_QUIT));
    CHECK(fd2_app_flow_from_field(FD2_APP_FIELD_COMPLETE) ==
          FD2_APP_FLOW_EXIT);
    CHECK(fd2_app_flow_from_field(FD2_APP_FIELD_HOST_QUIT) ==
          FD2_APP_FLOW_EXIT);
    CHECK(fd2_app_flow_from_field(FD2_APP_FIELD_ERROR) ==
          FD2_APP_FLOW_ERROR);
    CHECK(fd2_app_flow_from_field(FD2_APP_FIELD_VICTORY) ==
          FD2_APP_FLOW_BATTLE_VICTORY);
    CHECK(fd2_app_flow_from_field(FD2_APP_FIELD_DEFEAT) ==
          FD2_APP_FLOW_BATTLE_DEFEAT);
    CHECK(fd2_app_flow_from_title((fd2_title_action)99) ==
          FD2_APP_FLOW_ERROR);
    CHECK(fd2_app_flow_from_field((fd2_app_field_result)99) ==
          FD2_APP_FLOW_ERROR);
    CHECK(fd2_app_flow_from_field((fd2_app_field_result)-1) ==
          FD2_APP_FLOW_ERROR);
    CHECK(fd2_app_flow_after_battle(FD2_APP_FIELD_VICTORY,1)==FD2_APP_FLOW_PREPARE);
    CHECK(fd2_app_flow_after_battle(FD2_APP_FIELD_VICTORY,-1)==FD2_APP_FLOW_ERROR);
    CHECK(fd2_app_flow_after_battle(FD2_APP_FIELD_VICTORY,0)==FD2_APP_FLOW_ERROR);
    CHECK(fd2_app_flow_after_battle(FD2_APP_FIELD_VICTORY,2)==FD2_APP_FLOW_ERROR);
    CHECK(fd2_app_flow_after_battle(FD2_APP_FIELD_DEFEAT,-1)==FD2_APP_FLOW_TITLE);
    CHECK(fd2_app_flow_after_battle(FD2_APP_FIELD_HOST_QUIT,1)==FD2_APP_FLOW_EXIT);
    CHECK(fd2_app_flow_after_battle(FD2_APP_FIELD_LEAVE_GAME,1)==FD2_APP_FLOW_LEAVE_GAME);
    CHECK(fd2_app_flow_after_battle(FD2_APP_FIELD_ERROR,1)==FD2_APP_FLOW_ERROR);
    CHECK(fd2_app_flow_from_prepare(FD2_APP_PREPARE_DEPART)==FD2_APP_FLOW_BATTLE);
    CHECK(fd2_app_flow_from_prepare(FD2_APP_PREPARE_TITLE)==FD2_APP_FLOW_TITLE);
    CHECK(fd2_app_flow_from_prepare(FD2_APP_PREPARE_HOST_QUIT)==FD2_APP_FLOW_EXIT);
    CHECK(fd2_app_flow_from_prepare(FD2_APP_PREPARE_LEAVE_GAME)==FD2_APP_FLOW_LEAVE_GAME);
    CHECK(fd2_app_flow_from_prepare(FD2_APP_PREPARE_ERROR)==FD2_APP_FLOW_ERROR);
    CHECK(fd2_app_flow_from_prepare((fd2_app_prepare_result)99)==FD2_APP_FLOW_ERROR);
    puts("app_flow_test: title/prepare/0-to-1/gameover/host routes passed");
    return 0;
}
