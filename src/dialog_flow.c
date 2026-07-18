/* 炎龙骑士团 2 SDL3 重写 - 对话 token 等待边界
 *
 * 逆向依据：text_dialog_render_tokens @VA 0x15f84。已有对话框遇到
 * -17..-20 时先调用 FUN_00016c57(0) 等待，再关闭旧框；-3 调用
 * FUN_00016c57(1) 等待继续；第四行溢出则先用
 * dialog_text_scroll_up (FUN_00016e24) 上卷旧
 * 对话，再把逻辑行号退回 2；-1 在关闭最终框前调用 FUN_00016c57(0)。
 */
#include "dialog_flow.h"

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

fd2_dialog_line_transition fd2_dialog_flow_line_transition(int16_t token,
                                                           int line) {
    fd2_dialog_line_transition result = {line, 0, FD2_DIALOG_FLOW_NONE};
    if (token != -2 && token != -3) return result;
    if (result.line >= 3) {
        result.scroll_before_wait = 1;
        result.line--;
    }
    result.line++;
    if (token == -3) result.wait = FD2_DIALOG_FLOW_WAIT_SCROLL;
    return result;
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
    if (token == -2 || token == -3)
        return fd2_dialog_flow_line_transition(token, line).wait;
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
        } else if (token == -2 || token == -3) {
            line = fd2_dialog_flow_line_transition(token, line).line;
        }
    }
    return waits;
}
