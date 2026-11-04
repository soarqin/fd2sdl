#ifndef FD2_APP_FLOW_H
#define FD2_APP_FLOW_H

typedef enum {
    FD2_TITLE_ACTION_ERROR = -2,
    FD2_TITLE_ACTION_HOST_QUIT = -1,
    FD2_TITLE_ACTION_NEW_GAME = 0,
    FD2_TITLE_ACTION_LOAD,
    FD2_TITLE_ACTION_CONTINUE
} fd2_title_action;

typedef enum {
    FD2_APP_FLOW_SHOW_TITLE = 0,
    FD2_APP_FLOW_START_NEW_GAME,
    FD2_APP_FLOW_LOAD_GAME,
    FD2_APP_FLOW_EXIT,
    FD2_APP_FLOW_ERROR,
    FD2_APP_FLOW_BATTLE_VICTORY,
    FD2_APP_FLOW_BATTLE_DEFEAT,
    FD2_APP_FLOW_CONTINUE_GAME
} fd2_app_flow;

/* title_action_dispatch @0x25ebb：0=START，1=四槽 LOAD，2=活动快照
 * CONTINUE；对应 boot_intro_title_menu @0x1f894 的实际资源顺序。
 * SDL 只在当前支持的活动快照可恢复时开放第三项；LOAD 不冒充退出。 */
fd2_title_action fd2_title_action_from_selection(int selection,
                                                  int continue_available);
fd2_app_flow fd2_app_flow_from_title(fd2_title_action action);
/* field_play_result 的稳定数值镜像，避免纯分派模块依赖 SDL。 */
typedef enum {
    FD2_APP_FIELD_COMPLETE = 0,
    FD2_APP_FIELD_TITLE,
    FD2_APP_FIELD_HOST_QUIT,
    FD2_APP_FIELD_ERROR,
    FD2_APP_FIELD_VICTORY,
    FD2_APP_FIELD_DEFEAT
} fd2_app_field_result;

_Static_assert(FD2_APP_FIELD_COMPLETE == 0 &&
               FD2_APP_FIELD_TITLE == 1 &&
               FD2_APP_FIELD_HOST_QUIT == 2 &&
               FD2_APP_FIELD_ERROR == 3 &&
               FD2_APP_FIELD_VICTORY == 4 &&
               FD2_APP_FIELD_DEFEAT == 5,
               "app/field result ABI changed");

fd2_app_flow fd2_app_flow_from_field(fd2_app_field_result result);

#endif /* FD2_APP_FLOW_H */
