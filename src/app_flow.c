/* 标题与战场之间的纯应用分派；宿主关闭不伪装成游戏菜单选择。 */
#include "app_flow.h"

fd2_title_action fd2_title_action_from_selection(int selection,
                                                  int load_available) {
    if (selection < 0) return FD2_TITLE_ACTION_HOST_QUIT;
    if (load_available) {
        if (selection == 0) return FD2_TITLE_ACTION_NEW_GAME;
        if (selection == 1) return FD2_TITLE_ACTION_LOAD;
        if (selection == 2) return FD2_TITLE_ACTION_EXIT;
    } else {
        if (selection == 0) return FD2_TITLE_ACTION_NEW_GAME;
        if (selection == 1) return FD2_TITLE_ACTION_EXIT;
    }
    return (fd2_title_action)99;
}

fd2_app_flow fd2_app_flow_from_title(fd2_title_action action) {
    switch (action) {
        case FD2_TITLE_ACTION_NEW_GAME:
            return FD2_APP_FLOW_START_NEW_GAME;
        case FD2_TITLE_ACTION_LOAD:
            return FD2_APP_FLOW_LOAD_GAME;
        case FD2_TITLE_ACTION_EXIT:
        case FD2_TITLE_ACTION_HOST_QUIT:
            return FD2_APP_FLOW_EXIT;
    }
    return FD2_APP_FLOW_ERROR;
}

fd2_app_flow fd2_app_flow_from_field(fd2_app_field_result result) {
    switch (result) {
        case FD2_APP_FIELD_COMPLETE:
        case FD2_APP_FIELD_TITLE:
            return FD2_APP_FLOW_SHOW_TITLE;
        case FD2_APP_FIELD_HOST_QUIT:
            return FD2_APP_FLOW_EXIT;
        case FD2_APP_FIELD_ERROR:
            return FD2_APP_FLOW_ERROR;
    }
    return FD2_APP_FLOW_ERROR;
}
