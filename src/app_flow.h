#ifndef FD2_APP_FLOW_H
#define FD2_APP_FLOW_H

#include <stddef.h>

typedef enum {
    FD2_TITLE_ACTION_ERROR = -2,
    FD2_TITLE_ACTION_HOST_QUIT = -1,
    FD2_TITLE_ACTION_NEW_GAME = 0,
    FD2_TITLE_ACTION_LOAD,
    FD2_TITLE_ACTION_CONTINUE
} fd2_title_action;

typedef enum {
    FD2_APP_FLOW_LEAVE_GAME = 0,
    FD2_APP_FLOW_START_NEW_GAME,
    FD2_APP_FLOW_LOAD_GAME,
    FD2_APP_FLOW_EXIT,
    FD2_APP_FLOW_ERROR,
    FD2_APP_FLOW_BATTLE_VICTORY,
    FD2_APP_FLOW_BATTLE_DEFEAT,
    FD2_APP_FLOW_CONTINUE_GAME,
    FD2_APP_FLOW_PREPARE,
    FD2_APP_FLOW_BATTLE,
    FD2_APP_FLOW_TITLE
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
    FD2_APP_FIELD_LEAVE_GAME,
    FD2_APP_FIELD_HOST_QUIT,
    FD2_APP_FIELD_ERROR,
    FD2_APP_FIELD_VICTORY,
    FD2_APP_FIELD_DEFEAT
} fd2_app_field_result;

_Static_assert(FD2_APP_FIELD_COMPLETE == 0 &&
               FD2_APP_FIELD_LEAVE_GAME == 1 &&
               FD2_APP_FIELD_HOST_QUIT == 2 &&
               FD2_APP_FIELD_ERROR == 3 &&
               FD2_APP_FIELD_VICTORY == 4 &&
               FD2_APP_FIELD_DEFEAT == 5,
               "app/field result ABI changed");

fd2_app_flow fd2_app_flow_from_field(fd2_app_field_result result);

/* 26152返回0才出阵、1是离场；SDL宿主/错误与标题取消保持独立。
 * 数值与field_prepare_play结果对应，纯分派不引入SDL依赖。 */
typedef enum {
    FD2_APP_PREPARE_ERROR = -1,
    FD2_APP_PREPARE_DEPART = 0,
    FD2_APP_PREPARE_TITLE = 1,
    FD2_APP_PREPARE_HOST_QUIT = 2,
    FD2_APP_PREPARE_LEAVE_GAME = 3
} fd2_app_prepare_result;
fd2_app_flow fd2_app_flow_from_prepare(fd2_app_prepare_result result);
/* play仅在stage0完整结算后返回胜利，并导出stage1准备owner。
 * active Load可跨关，不能再用进入play前的stage重复分派；gameover已播完。 */
fd2_app_flow fd2_app_flow_after_battle(fd2_app_field_result result,
    int prepared_stage);

#endif /* FD2_APP_FLOW_H */
