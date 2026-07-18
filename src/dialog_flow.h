#ifndef FD2_DIALOG_FLOW_H
#define FD2_DIALOG_FLOW_H

#include <stddef.h>
#include <stdint.h>

/* text_dialog_render_tokens 的末项参数固定传 0x13；字形本身为 16 px，
 * 行间保留 3 px。dialog_text_scroll_up 同样总计上卷 19 px。 */
enum {
    FD2_DIALOG_LINE_PITCH = 19,
};

typedef enum {
    FD2_DIALOG_FLOW_NONE = 0,
    FD2_DIALOG_FLOW_WAIT_BEFORE_SPEAKER,
    FD2_DIALOG_FLOW_WAIT_SCROLL,
    FD2_DIALOG_FLOW_WAIT_END
} fd2_dialog_flow_event;

typedef struct {
    int line;
    int scroll_before_wait;
    fd2_dialog_flow_event wait;
} fd2_dialog_line_transition;

/* 纯 token 状态 helper：给定当前 token 和对话框是否已经打开，返回原版
 * text_dialog_render_tokens 对应的等待边界。 */
fd2_dialog_flow_event fd2_dialog_flow_event_for_token(int16_t token,
                                                       int dialog_is_open,
                                                       int line);

/* -2/-3 的共同状态转换：line==3 时先上卷并退回 line 2，再推进一行；
 * -3 的 continuation wait 排在滚动之后。 */
fd2_dialog_line_transition fd2_dialog_flow_line_transition(int16_t token,
                                                           int line);

/* 统计片段中会阻塞读取按键的边界，供测试验证同一 fragment 的多说话人
 * 不会被错误折叠成只有末尾一次等待。 */
size_t fd2_dialog_flow_count_waits(const uint8_t *tokens,
                                   size_t token_count);

#endif /* FD2_DIALOG_FLOW_H */
