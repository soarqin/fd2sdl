#ifndef FD2_DIALOG_FLOW_H
#define FD2_DIALOG_FLOW_H

#include <stddef.h>
#include <stdint.h>

/* text_dialog_render_tokens 的末项参数固定传 0x13；字形本身为 16 px，
 * 行间保留 3 px。dialog_text_scroll_up 同样总计上卷 19 px。 */
enum {
    FD2_DIALOG_LINE_PITCH = 19,
};

/* text_dialog_glyph_step @0x164e8的53A14/53A10为App全局DWORD。
 * VGA初始化时置0；闭嘴、换人、关闭框、活动/手工Load都不复位。
 * 不保存于SAV，不在每个dialog/progress/prepare局部另存一份。 */
typedef struct {
    uint32_t glyph_counter;
    uint32_t mouth_phase;
} fd2_dialog_visual_state;

/* 仅实际逐字helper调用：每两次glyph递增mouth(0..3)，显示3映射1。
 * 返回1才需重画portrait并写draw_phase；0只推进glyph，不改输出；
 * -1坏参数/非法正常域，state/output均不写。cue2与wait1由真实caller执行。 */
int fd2_dialog_visual_glyph_step(fd2_dialog_visual_state *state,
    unsigned *draw_phase);

typedef uint32_t (*fd2_dialog_random_fn)(void *userdata);
/* 16c57局部等待状态，不跨wait保存，不是另一个App mouth owner。
 * BIOS两tick采用现宿主110ms近似，逾时一次只推进一次，不追赶。 */
typedef struct {
    uint64_t last_ms;
    uint32_t countdown;
    uint8_t blink_closed, arrow_frame, arrow_ticks, arrow_mode;
} fd2_dialog_wait_state;
typedef struct {
    int portrait; /* -1不画，0睁眼，3闭眼；不写共享mouth DWORD。 */
    int arrow;    /* -1不画，否则18/19。 */
} fd2_dialog_wait_visual;
int fd2_dialog_wait_begin(fd2_dialog_wait_state *state, uint64_t now_ms,
    int arrow_mode, fd2_dialog_random_fn rng, void *userdata);
int fd2_dialog_wait_update(fd2_dialog_wait_state *state, uint64_t now_ms,
    fd2_dialog_random_fn rng, void *userdata, fd2_dialog_wait_visual *visual);
/* dialog_text_scroll_up @0x16e24：仅原portrait origin1832/36887有写。
 * 208×72按5次3px+1次4px上卷，底一行填74；其余origin成功no-op。
 * 仅CPU复制，无present/wait/RNG/input；有效origin要求完整64000B显示页。 */
int fd2_dialog_scroll_pixels(uint8_t *pixels, size_t size, unsigned origin);

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
