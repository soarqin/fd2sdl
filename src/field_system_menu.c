/* 炎龙骑士团 2 SDL3 重写 - 战场 field-level 系统菜单骨架
 *
 * 机器码依据：
 * - empty-focus dispatcher @ dual 0x16f55 / code0 0x6f55，
 *   command IDs {7,5,6,4}；
 * - secondary dispatcher @ dual 0x19df7 / code0 0x9df7，
 *   command IDs {12,13,14,15}；
 * - options loop @ dual 0x1728c / code0 0x728c，
 *   base IDs {18,20,22,24}，按状态生成相邻 ID。
 *
 * 本文件只实现可测试的选择和动作分派，不猜测未由 DOSBox 确认的
 * 中文标签、存档槽交互和应用级退出。图形资源由共享的完整
 * FDOTHER[2] loader 提供，四相位开合动画由 field_game 接入。
 */
#include "field_system_menu.h"

#include <string.h>

static void set_ids(fd2_field_system_menu *menu) {
    static const uint8_t parent_ids[FD2_FIELD_SYSTEM_MENU_COUNT] = {
        7, 5, 6, 4
    };
    static const uint8_t secondary_ids[FD2_FIELD_SYSTEM_MENU_COUNT] = {
        12, 13, 14, 15
    };
    static const uint8_t option_base_ids[FD2_FIELD_SYSTEM_MENU_COUNT] = {
        18, 20, 22, 24
    };
    const uint8_t *ids = parent_ids;
    if (menu->page == FD2_FIELD_SYSTEM_PAGE_SECONDARY)
        ids = secondary_ids;
    else if (menu->page == FD2_FIELD_SYSTEM_PAGE_OPTIONS)
        ids = option_base_ids;
    memcpy(menu->command_ids, ids, sizeof(menu->command_ids));
    if (menu->page == FD2_FIELD_SYSTEM_PAGE_OPTIONS) {
        /* code0 0x72c4..0x7318 对四项使用的条件分别是
         * !DS:1e61、!DS:1e62、DS:3af9、!DS:1aab。第三项与其他
         * 三项反向，不能用同一布尔公式生成 command ID。 */
        menu->command_ids[0] = (uint8_t)(ids[0] +
                                         (menu->option_values[0] ? 0 : 1));
        menu->command_ids[1] = (uint8_t)(ids[1] +
                                         (menu->option_values[1] ? 0 : 1));
        menu->command_ids[2] = (uint8_t)(ids[2] +
                                         (menu->option_values[2] ? 1 : 0));
        menu->command_ids[3] = (uint8_t)(ids[3] +
                                         (menu->option_values[3] ? 0 : 1));
    }
}

void fd2_field_system_menu_init(fd2_field_system_menu *menu) {
    if (!menu) return;
    memset(menu, 0, sizeof(*menu));
    menu->page = FD2_FIELD_SYSTEM_PAGE_PARENT;
    menu->selected = 0;
    /* object 2 loader 初值：DS:0x1e61=1、0x1e62=1、0x3af9=0、
     * 0x1aab=1。这里只保存位值；配置语义仍由后续 M8.3 确认。 */
    menu->option_values[0] = 1;
    menu->option_values[1] = 1;
    menu->option_values[2] = 0;
    menu->option_values[3] = 1;
    menu->animation_opening = 1;
    menu->animation_phase = 4;
    set_ids(menu);
}

void fd2_field_system_menu_open_secondary(fd2_field_system_menu *menu) {
    if (!menu) return;
    menu->page = FD2_FIELD_SYSTEM_PAGE_SECONDARY;
    menu->selected = 0;
    memset(menu->disabled, 0, sizeof(menu->disabled));
    menu->highlight_phase = 0;
    menu->animation_opening = 1;
    menu->animation_phase = 4;
    menu->last_highlight_ms = 0;
    set_ids(menu);
}

void fd2_field_system_menu_set_options(
        fd2_field_system_menu *menu,
        const uint8_t values[FD2_FIELD_SYSTEM_MENU_COUNT]) {
    if (!menu || !values) return;
    for (size_t i = 0; i < FD2_FIELD_SYSTEM_MENU_COUNT; i++)
        menu->option_values[i] = values[i] ? 1u : 0u;
    set_ids(menu);
}

void fd2_field_system_menu_set_disabled(
        fd2_field_system_menu *menu,
        const uint8_t disabled[FD2_FIELD_SYSTEM_MENU_COUNT]) {
    if (!menu) return;
    if (disabled)
        memcpy(menu->disabled, disabled, sizeof(menu->disabled));
    else
        memset(menu->disabled, 0, sizeof(menu->disabled));
    if (menu->selected >= FD2_FIELD_SYSTEM_MENU_COUNT ||
        menu->disabled[menu->selected]) {
        int first = fd2_field_command_first_enabled(menu->disabled);
        menu->selected = first >= 0 ? (uint8_t)first : 0;
    }
}

int fd2_field_system_menu_select_direction(
        fd2_field_system_menu *menu, fd2_field_command_direction direction) {
    if (!menu || direction < FD2_FIELD_COMMAND_DIRECTION_UP ||
        direction > FD2_FIELD_COMMAND_DIRECTION_DOWN)
        return 0;
    if (menu->page == FD2_FIELD_SYSTEM_PAGE_CONFIRMATION) {
        uint8_t yes = direction == FD2_FIELD_COMMAND_DIRECTION_RIGHT ||
                      direction == FD2_FIELD_COMMAND_DIRECTION_DOWN;
        if (menu->confirmation_yes == yes) return 0;
        menu->confirmation_yes = yes;
        menu->selected = yes;
        return 1;
    }
    int selected = fd2_field_command_select_direction(
        menu->selected, direction, menu->disabled);
    if (selected == menu->selected) return 0;
    menu->selected = (uint8_t)selected;
    return 1;
}

static void open_confirmation(fd2_field_system_menu *menu,
                              fd2_field_system_action action,
                              size_t prompt, size_t accepted, size_t failure,
                              fd2_field_system_page return_page,
                              size_t return_selected) {
    menu->page = FD2_FIELD_SYSTEM_PAGE_CONFIRMATION;
    menu->selected = 0;
    memcpy(menu->confirmation_disabled, menu->disabled,
           sizeof(menu->disabled));
    memset(menu->disabled, 0, sizeof(menu->disabled));
    menu->confirmation_action = action;
    menu->confirmation_prompt_fragment = (uint16_t)prompt;
    menu->confirmation_accepted_fragment = (uint16_t)accepted;
    menu->confirmation_failure_fragment = (uint16_t)failure;
    menu->confirmation_return_page = (uint8_t)return_page;
    menu->confirmation_return_selected = (uint8_t)return_selected;
    menu->confirmation_yes = 0;
    menu->highlight_phase = 0;
    menu->last_highlight_ms = 0;
}

fd2_field_system_action fd2_field_system_menu_confirm(
        fd2_field_system_menu *menu) {
    if (!menu) return FD2_FIELD_SYSTEM_ACTION_NONE;
    if (menu->page == FD2_FIELD_SYSTEM_PAGE_CONFIRMATION) {
        if (!menu->confirmation_yes) {
            menu->page = (fd2_field_system_page)menu->confirmation_return_page;
            menu->selected = menu->confirmation_return_selected;
            memcpy(menu->disabled, menu->confirmation_disabled,
                   sizeof(menu->disabled));
            set_ids(menu);
            return FD2_FIELD_SYSTEM_ACTION_BACK;
        }
        /* accepted 只报告一次 action，并回到原 dispatcher 的 selector；
         * Save/Load 由 field_play 提交，其余后端仍可保持 pending。 */
        fd2_field_system_action action = menu->confirmation_action;
        menu->page = (fd2_field_system_page)menu->confirmation_return_page;
        menu->selected = menu->confirmation_return_selected;
        memcpy(menu->disabled, menu->confirmation_disabled,
               sizeof(menu->disabled));
        menu->confirmation_yes = 0;
        set_ids(menu);
        return action;
    }
    if (menu->selected >= FD2_FIELD_SYSTEM_MENU_COUNT ||
        menu->disabled[menu->selected])
        return FD2_FIELD_SYSTEM_ACTION_NONE;

    size_t index = menu->selected;
    if (menu->page == FD2_FIELD_SYSTEM_PAGE_PARENT) {
        switch (index) {
            case 0:
                /* code0 0x6fed 调用 0x9df7 的次级 dispatcher；后者的
                 * selector 0 才调用屏幕过渡页 0xb1e7。 */
                fd2_field_system_menu_open_secondary(menu);
                return FD2_FIELD_SYSTEM_ACTION_OPEN_SECONDARY;
            case 1:
                open_confirmation(menu,
                                  FD2_FIELD_SYSTEM_ACTION_MARCH_CONFIRM,
                                  FD2_FIELD_SYSTEM_TEXT_MARCH_PROMPT,
                                  FD2_FIELD_SYSTEM_TEXT_MARCH_ACCEPTED, 0,
                                  FD2_FIELD_SYSTEM_PAGE_PARENT, index);
                return FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION;
            case 2:
                menu->page = FD2_FIELD_SYSTEM_PAGE_OPTIONS;
                menu->selected = 0;
                memset(menu->disabled, 0, sizeof(menu->disabled));
                menu->highlight_phase = 0;
                menu->animation_opening = 1;
                menu->animation_phase = 4;
                menu->last_highlight_ms = 0;
                set_ids(menu);
                return FD2_FIELD_SYSTEM_ACTION_OPEN_OPTIONS;
            case 3:
                open_confirmation(menu,
                                  FD2_FIELD_SYSTEM_ACTION_END_TURN_CONFIRM,
                                  FD2_FIELD_SYSTEM_TEXT_END_TURN_PROMPT,
                                  FD2_FIELD_SYSTEM_TEXT_END_TURN_ACCEPTED, 0,
                                  FD2_FIELD_SYSTEM_PAGE_PARENT, index);
                return FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION;
        }
    } else if (menu->page == FD2_FIELD_SYSTEM_PAGE_SECONDARY) {
        switch (index) {
            case 0: return FD2_FIELD_SYSTEM_ACTION_SCREEN_TRANSITION;
            case 1:
                open_confirmation(menu, FD2_FIELD_SYSTEM_ACTION_SAVE,
                                  FD2_FIELD_SYSTEM_TEXT_SAVE_PROMPT,
                                  FD2_FIELD_SYSTEM_TEXT_SAVE_SUCCESS,
                                  FD2_FIELD_SYSTEM_TEXT_SAVE_FAILURE,
                                  FD2_FIELD_SYSTEM_PAGE_SECONDARY, index);
                return FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION;
            case 2:
                open_confirmation(menu, FD2_FIELD_SYSTEM_ACTION_LOAD,
                                  FD2_FIELD_SYSTEM_TEXT_LOAD_PROMPT,
                                  FD2_FIELD_SYSTEM_TEXT_LOAD_ACCEPTED, 0,
                                  FD2_FIELD_SYSTEM_PAGE_SECONDARY, index);
                return FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION;
            case 3:
                open_confirmation(menu, FD2_FIELD_SYSTEM_ACTION_LEAVE_BATTLE,
                                  FD2_FIELD_SYSTEM_TEXT_LEAVE_PROMPT,
                                  FD2_FIELD_SYSTEM_TEXT_LEAVE_ACCEPTED, 0,
                                  FD2_FIELD_SYSTEM_PAGE_SECONDARY, index);
                return FD2_FIELD_SYSTEM_ACTION_OPEN_CONFIRMATION;
        }
    } else if (menu->page == FD2_FIELD_SYSTEM_PAGE_OPTIONS) {
        /* options loop @code0 0x735b..0x73e2：四个 selector 均立即翻转
         * 对应字节并回到同一页；第三项只在 command-ID 显示条件上反向。 */
        menu->option_values[index] ^= 1u;
        set_ids(menu);
        return FD2_FIELD_SYSTEM_ACTION_OPTIONS_TOGGLE_PENDING;
    }
    return FD2_FIELD_SYSTEM_ACTION_NONE;
}

fd2_field_system_action fd2_field_system_menu_cancel(
        fd2_field_system_menu *menu) {
    if (!menu) return FD2_FIELD_SYSTEM_ACTION_CANCEL;
    if (menu->page == FD2_FIELD_SYSTEM_PAGE_CONFIRMATION) {
        menu->page = (fd2_field_system_page)menu->confirmation_return_page;
        menu->selected = menu->confirmation_return_selected;
        memcpy(menu->disabled, menu->confirmation_disabled,
               sizeof(menu->disabled));
        menu->confirmation_yes = 0;
        set_ids(menu);
        return FD2_FIELD_SYSTEM_ACTION_BACK;
    }
    if (menu->page == FD2_FIELD_SYSTEM_PAGE_PARENT)
        return FD2_FIELD_SYSTEM_ACTION_CANCEL;
    menu->page = FD2_FIELD_SYSTEM_PAGE_PARENT;
    menu->selected = 0;
    memset(menu->disabled, 0, sizeof(menu->disabled));
    menu->highlight_phase = 0;
    menu->animation_opening = 1;
    menu->animation_phase = 4;
    menu->last_highlight_ms = 0;
    set_ids(menu);
    return FD2_FIELD_SYSTEM_ACTION_BACK;
}

int fd2_field_system_menu_get_last_result_fragment(
        const fd2_field_system_menu *menu, int success, size_t *fragment) {
    if (!menu || !fragment ||
        menu->confirmation_action == FD2_FIELD_SYSTEM_ACTION_NONE)
        return -1;
    uint16_t value = success ? menu->confirmation_accepted_fragment
                             : menu->confirmation_failure_fragment;
    if (value == 0) return -1;
    *fragment = value;
    return 0;
}

int fd2_field_system_menu_get_confirmation_fragment(
        const fd2_field_system_menu *menu,
        fd2_field_system_confirm_part part, size_t *fragment) {
    if (!menu || !fragment ||
        menu->page != FD2_FIELD_SYSTEM_PAGE_CONFIRMATION)
        return -1;
    switch (part) {
        case FD2_FIELD_SYSTEM_CONFIRM_PROMPT:
            *fragment = menu->confirmation_prompt_fragment;
            return 0;
        case FD2_FIELD_SYSTEM_CONFIRM_ACCEPTED:
            *fragment = menu->confirmation_accepted_fragment;
            return 0;
        case FD2_FIELD_SYSTEM_CONFIRM_FAILURE:
            *fragment = menu->confirmation_failure_fragment;
            return 0;
    }
    return -1;
}
