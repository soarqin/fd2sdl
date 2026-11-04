/* 纯应用分派：title_action_dispatch @0x25ebb（原登记 title_action_dispatch）
 * 的 START/LOAD/CONTINUE 分支；宿主关闭不伪装成游戏菜单选择。 */
#include "app_flow.h"

fd2_title_action fd2_title_action_from_selection(int selection,
                                                  int continue_available) {
    if (selection == FD2_TITLE_ACTION_HOST_QUIT) return FD2_TITLE_ACTION_HOST_QUIT;
    if (selection == 0) return FD2_TITLE_ACTION_NEW_GAME;
    if (selection == 1) return FD2_TITLE_ACTION_LOAD;
    if (selection == 2 && continue_available) return FD2_TITLE_ACTION_CONTINUE;
    return FD2_TITLE_ACTION_ERROR;
}

fd2_app_flow fd2_app_flow_from_title(fd2_title_action action) {
    switch (action) {
        case FD2_TITLE_ACTION_NEW_GAME:
            return FD2_APP_FLOW_START_NEW_GAME;
        case FD2_TITLE_ACTION_LOAD:
            return FD2_APP_FLOW_LOAD_GAME;
        case FD2_TITLE_ACTION_CONTINUE:
            return FD2_APP_FLOW_CONTINUE_GAME;
        case FD2_TITLE_ACTION_HOST_QUIT:
            return FD2_APP_FLOW_EXIT;
        case FD2_TITLE_ACTION_ERROR:
            return FD2_APP_FLOW_ERROR;
    }
    return FD2_APP_FLOW_ERROR;
}

fd2_app_flow fd2_app_flow_from_field(fd2_app_field_result result) {
    switch (result) {
        case FD2_APP_FIELD_COMPLETE:
            return FD2_APP_FLOW_EXIT;
        case FD2_APP_FIELD_LEAVE_GAME:
            return FD2_APP_FLOW_LEAVE_GAME;
        case FD2_APP_FIELD_VICTORY:
            return FD2_APP_FLOW_BATTLE_VICTORY;
        case FD2_APP_FIELD_DEFEAT:
            return FD2_APP_FLOW_BATTLE_DEFEAT;
        case FD2_APP_FIELD_HOST_QUIT:
            return FD2_APP_FLOW_EXIT;
        case FD2_APP_FIELD_ERROR:
            return FD2_APP_FLOW_ERROR;
    }
    return FD2_APP_FLOW_ERROR;
}
