#ifndef FD2_APP_FLOW_H
#define FD2_APP_FLOW_H

typedef enum {
    FD2_TITLE_ACTION_HOST_QUIT = -1,
    FD2_TITLE_ACTION_NEW_GAME = 0,
    FD2_TITLE_ACTION_LOAD,
    FD2_TITLE_ACTION_EXIT
} fd2_title_action;

typedef enum {
    FD2_APP_FLOW_SHOW_TITLE = 0,
    FD2_APP_FLOW_START_NEW_GAME,
    FD2_APP_FLOW_LOAD_GAME,
    FD2_APP_FLOW_EXIT,
    FD2_APP_FLOW_ERROR
} fd2_app_flow;

/* code0 boot title @0xf894 的选择器在无可读活动快照时只有两项：
 * new game、exit；有快照时插入 active snapshot load。四槽 hand_load
 * 属于 field command dispatcher @0x19300，不使用此 action。 */
fd2_title_action fd2_title_action_from_selection(int selection,
                                                  int load_available);
fd2_app_flow fd2_app_flow_from_title(fd2_title_action action);
/* field_play_result 的稳定数值镜像，避免纯分派模块依赖 SDL。 */
typedef enum {
    FD2_APP_FIELD_COMPLETE = 0,
    FD2_APP_FIELD_TITLE,
    FD2_APP_FIELD_HOST_QUIT,
    FD2_APP_FIELD_ERROR
} fd2_app_field_result;

_Static_assert(FD2_APP_FIELD_COMPLETE == 0 &&
               FD2_APP_FIELD_TITLE == 1 &&
               FD2_APP_FIELD_HOST_QUIT == 2 &&
               FD2_APP_FIELD_ERROR == 3,
               "app/field result ABI changed");

fd2_app_flow fd2_app_flow_from_field(fd2_app_field_result result);

#endif /* FD2_APP_FLOW_H */
