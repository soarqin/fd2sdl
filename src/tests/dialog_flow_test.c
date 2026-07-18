#include <stdio.h>

#include "dialog_flow.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "check failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return -1; \
    } \
} while (0)

#define TOK(value) \
    (uint8_t)((uint16_t)(value) & 0xffu), \
    (uint8_t)(((uint16_t)(value) >> 8) & 0xffu)

static int test_wait_boundary_unit_cases(void) {
    CHECK(FD2_DIALOG_LINE_PITCH == 19);
    CHECK(fd2_dialog_flow_event_for_token(-18, 0, 0) ==
          FD2_DIALOG_FLOW_NONE);
    CHECK(fd2_dialog_flow_event_for_token(-19, 1, 0) ==
          FD2_DIALOG_FLOW_WAIT_BEFORE_SPEAKER);
    CHECK(fd2_dialog_flow_event_for_token(-3, 1, 1) ==
          FD2_DIALOG_FLOW_WAIT_SCROLL);
    CHECK(fd2_dialog_flow_event_for_token(-2, 1, 3) ==
          FD2_DIALOG_FLOW_NONE);
    CHECK(fd2_dialog_flow_event_for_token(-1, 1, 0) ==
          FD2_DIALOG_FLOW_WAIT_END);

    fd2_dialog_line_transition line =
        fd2_dialog_flow_line_transition(-2, 3);
    CHECK(line.scroll_before_wait == 1);
    CHECK(line.line == 3);
    CHECK(line.wait == FD2_DIALOG_FLOW_NONE);

    line = fd2_dialog_flow_line_transition(-3, 3);
    CHECK(line.scroll_before_wait == 1);
    CHECK(line.line == 3);
    CHECK(line.wait == FD2_DIALOG_FLOW_WAIT_SCROLL);

    line = fd2_dialog_flow_line_transition(-3, 2);
    CHECK(line.scroll_before_wait == 0);
    CHECK(line.line == 3);
    CHECK(line.wait == FD2_DIALOG_FLOW_WAIT_SCROLL);
    return 0;
}

static int test_multi_speaker_fragment_waits(void) {
    /* 摘取 FDTXT[33] fragment 0 的控制流形状：六个说话人控制码、
     * 一个 -3 和 fragment 结束。应有五次说话人切换等待、一次显式
     * continuation 等待和一次结束等待。参数 token 必须被 parser 跳过。 */
    static const uint8_t stage32_fragment_shape[] = {
        TOK(-18), TOK(0), TOK(1), TOK(-2),
        TOK(-19), TOK(0), TOK(2),
        TOK(-18), TOK(0), TOK(3),
        TOK(-19), TOK(0), TOK(4), TOK(-3),
        TOK(-19), TOK(1), TOK(5),
        TOK(-18), TOK(0), TOK(6), TOK(-1),
    };
    CHECK(fd2_dialog_flow_count_waits(
              stage32_fragment_shape,
              sizeof(stage32_fragment_shape) / 2u) == 7);

    /* 摘取 FDTXT[1] fragment 0 的控制流形状：五个说话人、一次显式
     * continuation 和结束，共六个等待。 */
    static const uint8_t stage0_fragment_shape[] = {
        TOK(-18), TOK(0), TOK(1),
        TOK(-17), TOK(4), TOK(2), TOK(-3),
        TOK(-18), TOK(0), TOK(3),
        TOK(-17), TOK(9), TOK(4),
        TOK(-17), TOK(30), TOK(5), TOK(-1),
    };
    CHECK(fd2_dialog_flow_count_waits(
              stage0_fragment_shape,
              sizeof(stage0_fragment_shape) / 2u) == 6);
    return 0;
}

int main(void) {
    if (test_wait_boundary_unit_cases() != 0 ||
        test_multi_speaker_fragment_waits() != 0)
        return 1;
    puts("dialog_flow_test: ok");
    return 0;
}
