#ifndef FD2_FIELD_SYSTEM_MENU_H
#define FD2_FIELD_SYSTEM_MENU_H

#include <stdint.h>

#include "field_command.h"

/* 炎龙骑士团 2 SDL3 重写 - 战场 field-level 系统菜单
 *
 * 逆向依据：空焦点菜单 code0 0x6f55（dual 0x16f55）、
 * 次级 field-level 子菜单 code0 0x9df7（dual 0x19df7）和设置页
 * code0 0x728c（dual 0x1728c）。该模块只管理菜单页、选择和
 * 已确认的动作分派；单位 attack/magic/item/wait 菜单仍由
 * field_command.c 单独管理。
 */

#define FD2_FIELD_SYSTEM_MENU_COUNT 4u

typedef enum {
    FD2_FIELD_SYSTEM_PAGE_PARENT = 0,
    FD2_FIELD_SYSTEM_PAGE_SECONDARY,
    FD2_FIELD_SYSTEM_PAGE_OPTIONS,
    FD2_FIELD_SYSTEM_PAGE_CONFIRMATION
} fd2_field_system_page;

/* FDTXT[0] 的已确认确认／结果片段索引。 */
#define FD2_FIELD_SYSTEM_TEXT_SAVE_PROMPT 0x19au
#define FD2_FIELD_SYSTEM_TEXT_SAVE_SUCCESS 0x19bu
#define FD2_FIELD_SYSTEM_TEXT_SAVE_FAILURE 0x19cu
#define FD2_FIELD_SYSTEM_TEXT_LOAD_PROMPT 0x19du
#define FD2_FIELD_SYSTEM_TEXT_LOAD_ACCEPTED 0x19eu
/* 手工 Load 失败的原版专用片段尚未由 picker 调用链确认；0 表示
 * 保持当前页面，不伪造活动快照或商店限制提示。 */
#define FD2_FIELD_SYSTEM_TEXT_LOAD_FAILURE 0u
/* code0 0x1982c/0x19a70：四槽 hand_save/hand_load 的完成片段。 */
#define FD2_FIELD_MANUAL_TEXT_SAVE_SUCCESS 0x294u
#define FD2_FIELD_MANUAL_TEXT_LOAD_SUCCESS 0x1deu
#define FD2_FIELD_SYSTEM_TEXT_LEAVE_PROMPT 0x19fu
#define FD2_FIELD_SYSTEM_TEXT_LEAVE_ACCEPTED 0x1a0u
#define FD2_FIELD_SYSTEM_TEXT_MARCH_PROMPT 0x1a1u
#define FD2_FIELD_SYSTEM_TEXT_MARCH_ACCEPTED 0x1a2u
#define FD2_FIELD_SYSTEM_TEXT_END_TURN_PROMPT 0x1a3u
#define FD2_FIELD_SYSTEM_TEXT_END_TURN_ACCEPTED 0x1a4u

typedef enum {
    FD2_FIELD_SYSTEM_CONFIRM_PROMPT = 0,
    FD2_FIELD_SYSTEM_CONFIRM_ACCEPTED,
    FD2_FIELD_SYSTEM_CONFIRM_FAILURE
} fd2_field_system_confirm_part;

typedef enum {
    FD2_FIELD_SYSTEM_ACTION_NONE = 0,
    FD2_FIELD_SYSTEM_ACTION_OPEN_SECONDARY,
    FD2_FIELD_SYSTEM_ACTION_OPEN_OPTIONS,
    FD2_FIELD_SYSTEM_ACTION_SCREEN_TRANSITION,
    FD2_FIELD_SYSTEM_ACTION_MARCH_CONFIRM,
    FD2_FIELD_SYSTEM_ACTION_END_TURN_CONFIRM,
    FD2_FIELD_SYSTEM_ACTION_SAVE,
    FD2_FIELD_SYSTEM_ACTION_LOAD,
    FD2_FIELD_SYSTEM_ACTION_LEAVE_BATTLE,
    FD2_FIELD_SYSTEM_ACTION_OPTIONS_TOGGLE_PENDING,
    FD2_FIELD_SYSTEM_ACTION_BACK,
    FD2_FIELD_SYSTEM_ACTION_CANCEL,
    FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION
} fd2_field_system_action;

typedef struct {
    fd2_field_system_page page;
    uint8_t selected;
    uint8_t command_ids[FD2_FIELD_SYSTEM_MENU_COUNT];
    uint8_t disabled[FD2_FIELD_SYSTEM_MENU_COUNT];
    /* 0/1 对应活动快照 meta[14..17] 的四个原版配置字节；可见标签
     * 与后三项的业务名称仍待 DOSBox 人工确认。 */
    uint8_t option_values[FD2_FIELD_SYSTEM_MENU_COUNT];
    /* 与单位 command 菜单分离的共享 primitive 呈现状态。 */
    uint8_t highlight_phase;
    uint8_t animation_phase;
    uint8_t animation_opening;
    uint64_t last_highlight_ms;
    fd2_field_system_action confirmation_action;
    uint16_t confirmation_prompt_fragment;
    uint16_t confirmation_accepted_fragment;
    uint16_t confirmation_failure_fragment;
    uint8_t confirmation_return_page;
    uint8_t confirmation_return_selected;
    uint8_t confirmation_yes;
    uint8_t confirmation_disabled[FD2_FIELD_SYSTEM_MENU_COUNT];
} fd2_field_system_menu;

void fd2_field_system_menu_init(fd2_field_system_menu *menu);
void fd2_field_system_menu_open_secondary(fd2_field_system_menu *menu);
void fd2_field_system_menu_set_options(
    fd2_field_system_menu *menu,
    const uint8_t values[FD2_FIELD_SYSTEM_MENU_COUNT]);
void fd2_field_system_menu_set_disabled(
    fd2_field_system_menu *menu,
    const uint8_t disabled[FD2_FIELD_SYSTEM_MENU_COUNT]);
int fd2_field_system_menu_select_direction(
    fd2_field_system_menu *menu, fd2_field_command_direction direction);
fd2_field_system_action fd2_field_system_menu_confirm(
    fd2_field_system_menu *menu);
fd2_field_system_action fd2_field_system_menu_cancel(
    fd2_field_system_menu *menu);
int fd2_field_system_menu_get_confirmation_fragment(
    const fd2_field_system_menu *menu,
    fd2_field_system_confirm_part part,
    size_t *fragment);
/* 确认已返回来源 dispatcher 后，后端用其展示一次成功／失败结果。 */
int fd2_field_system_menu_get_last_result_fragment(
    const fd2_field_system_menu *menu, int success, size_t *fragment);

#endif /* FD2_FIELD_SYSTEM_MENU_H */
