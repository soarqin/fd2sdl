/* 炎龙骑士团 2 SDL3 重写 - 对话 token 等待边界
 *
 * 逆向依据：text_dialog_render_tokens @VA 0x15f84。已有对话框遇到
 * -17..-20 时先调用 FUN_00016c57(0) 等待，再关闭旧框；-3 和第四行
 * 换页调用 FUN_00016c57(1)；-1 在关闭最终框前调用 FUN_00016c57(0)。
 */
#include "dialog_flow.h"

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

fd2_dialog_flow_event fd2_dialog_flow_event_for_token(int16_t token,
                                                       int dialog_is_open,
                                                       int line) {
    if (token == -1)
        return dialog_is_open ? FD2_DIALOG_FLOW_WAIT_END
                              : FD2_DIALOG_FLOW_NONE;
    if (token >= -20 && token <= -17)
        return dialog_is_open ? FD2_DIALOG_FLOW_WAIT_BEFORE_SPEAKER
                              : FD2_DIALOG_FLOW_NONE;
    if (token == -3 || (token == -2 && line >= 3))
        return FD2_DIALOG_FLOW_WAIT_PAGE;
    return FD2_DIALOG_FLOW_NONE;
}

size_t fd2_dialog_flow_count_waits(const uint8_t *tokens,
                                   size_t token_count) {
    if (!tokens) return 0;
    size_t waits = 0;
    int dialog_is_open = 0;
    int line = 0;
    for (size_t i = 0; i < token_count; i++) {
        int16_t token = (int16_t)rd_u16_le(tokens + i * 2u);
        fd2_dialog_flow_event event =
            fd2_dialog_flow_event_for_token(token, dialog_is_open, line);
        if (event != FD2_DIALOG_FLOW_NONE) waits++;
        if (token == -1) break;
        if (token >= -20 && token <= -17) {
            dialog_is_open = 1;
            line = 0;
            if (i + 1u < token_count) i++;
        } else if (token == -3 || (token == -2 && line >= 3)) {
            line = 0;
        } else if (token == -2) {
            line++;
        }
    }
    return waits;
}
