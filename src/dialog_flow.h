#ifndef FD2_DIALOG_FLOW_H
#define FD2_DIALOG_FLOW_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    FD2_DIALOG_FLOW_NONE = 0,
    FD2_DIALOG_FLOW_WAIT_BEFORE_SPEAKER,
    FD2_DIALOG_FLOW_WAIT_PAGE,
    FD2_DIALOG_FLOW_WAIT_END
} fd2_dialog_flow_event;

/* 纯 token 状态 helper：给定当前 token 和对话框是否已经打开，返回原版
 * text_dialog_render_tokens 对应的等待边界。 */
fd2_dialog_flow_event fd2_dialog_flow_event_for_token(int16_t token,
                                                       int dialog_is_open,
                                                       int line);

/* 统计片段中会阻塞读取按键的边界，供测试验证同一 fragment 的多说话人
 * 不会被错误折叠成只有末尾一次等待。 */
size_t fd2_dialog_flow_count_waits(const uint8_t *tokens,
                                   size_t token_count);

#endif /* FD2_DIALOG_FLOW_H */
