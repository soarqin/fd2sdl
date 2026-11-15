#include <stdio.h>
#include <string.h>

#include "dialog_flow.h"
#include "bios_clock.h"

/* 独立ceil(tick*65536e9/1193182)，此文件只用有界tick输入。 */
static uint64_t tick_ns(uint64_t tick) {
    return (tick*65536u*UINT64_C(1000000000)+1193181u)/1193182u;
}

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "check failed: %s (%s:%d)\n", #expr, __FILE__, __LINE__); \
        return -1; \
    } \
} while (0)

#define TOK(value) \
    (uint8_t)((uint16_t)(value) & 0xffu), \
    (uint8_t)(((uint16_t)(value) >> 8) & 0xffu)

static int test_shared_glyph_phase_survives_display_only_close_and_new_consumer(void) {
    fd2_dialog_visual_state state={0};
    static const unsigned expected[]={1,2,1,0,1};
    for (size_t i=0;i<sizeof(expected)/sizeof(expected[0]);i++) {
        unsigned draw=99;
        CHECK(fd2_dialog_visual_glyph_step(&state,&draw)==0 && draw==99 && state.glyph_counter==1);
        /* 闭嘴/换框仅改变显示方，不调用计数重置；下一consumer接同一state。 */
        draw=0;
        CHECK(fd2_dialog_visual_glyph_step(&state,&draw)==1 && draw==expected[i] && !state.glyph_counter);
    }
    CHECK(state.mouth_phase==1);
    fd2_dialog_visual_state before=state;unsigned draw=88;
    CHECK(fd2_dialog_visual_glyph_step(NULL,&draw)==-1 && draw==88);
    CHECK(fd2_dialog_visual_glyph_step(&state,NULL)==-1 && state.mouth_phase==before.mouth_phase && state.glyph_counter==before.glyph_counter);
    state.glyph_counter=2;
    CHECK(fd2_dialog_visual_glyph_step(&state,&draw)==-1 && state.glyph_counter==2 && state.mouth_phase==before.mouth_phase && draw==88);
    state.glyph_counter=0;state.mouth_phase=4;
    CHECK(fd2_dialog_visual_glyph_step(&state,&draw)==-1 && state.mouth_phase==4 && !state.glyph_counter && draw==88);
    return 0;
}

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

typedef struct { unsigned calls; uint32_t value; } random_probe;
static uint32_t counted_random(void *userdata) {
    random_probe *probe=userdata;probe->calls++;return probe->value;
}
/* 合成呈现完成点：只为纯phase叶喂入下一轮计数/锚。
 * 真正portrait→RNG→新clock顺序由正式detail/Scene consumer分别验证。 */
static int advance_wait_fixture(fd2_dialog_wait_state *state,uint64_t ns,
        fd2_dialog_random_fn rng,void *userdata,fd2_dialog_wait_visual *visual) {
    int result=fd2_dialog_wait_step(state,ns,visual);
    if(result>0) {
        if(visual->portrait==0)state->countdown=rng(userdata)%30u+2u;
        state->last_tick=fd2_bios_tick_signed_ns(ns);
    }
    return result;
}
static int test_wait_source_postdecrement_and_rng(void) {
    fd2_dialog_wait_state state;
    random_probe rng={0,0};
    CHECK(fd2_dialog_wait_begin(&state,0,1,counted_random,&rng)==0);
    CHECK(rng.calls==1 && state.countdown==2);
    fd2_dialog_wait_visual visual={77,88};
    CHECK(advance_wait_fixture(&state,tick_ns(2)-1u,counted_random,&rng,&visual)==0);
    CHECK(visual.portrait==-1&&visual.arrow==-1&&state.countdown==2);
    CHECK(advance_wait_fixture(&state,tick_ns(2),counted_random,&rng,&visual)==1);
    CHECK(state.countdown==1 && visual.portrait==-1 && rng.calls==1);
    CHECK(advance_wait_fixture(&state,tick_ns(4),counted_random,&rng,&visual)==1);
    CHECK(state.countdown==0 && visual.portrait==-1);
    CHECK(advance_wait_fixture(&state,tick_ns(6),counted_random,&rng,&visual)==1);
    CHECK(visual.portrait==3&&visual.arrow==19&&state.blink_closed&&rng.calls==1);
    rng.value=29;
    CHECK(advance_wait_fixture(&state,tick_ns(8),counted_random,&rng,&visual)==1);
    CHECK(visual.portrait==0&&visual.arrow==-1&&!state.blink_closed&&
        state.countdown==31&&rng.calls==2);
    CHECK(advance_wait_fixture(&state,tick_ns(181),counted_random,&rng,&visual)==1);
    CHECK(state.countdown==30&&rng.calls==2); /* 不补追全部过去tick。 */
    fd2_dialog_wait_state paused=state;
    /* 16d05..16d18：两个signed WORD相减后cmp2/jl，没有prepare式倒退门。 */
    CHECK(advance_wait_fixture(&state,0,counted_random,&rng,&visual)==0);
    CHECK(!memcmp(&state,&paused,sizeof(state)) && rng.calls==2 &&
        visual.portrait==-1 && visual.arrow==-1);
    CHECK(advance_wait_fixture(&state,tick_ns(182),counted_random,&rng,&visual)==0);
    CHECK(!memcmp(&state,&paused,sizeof(state)) && rng.calls==2);
    CHECK(advance_wait_fixture(&state,tick_ns(183),counted_random,&rng,&visual)==1);
    CHECK(state.countdown==29&&visual.arrow==18&&rng.calls==2);
    CHECK(fd2_dialog_wait_begin(&state,0,0,counted_random,&rng)==0);
    for(unsigned i=1;i<=3;i++) {
        CHECK(advance_wait_fixture(&state,tick_ns(i*2u),counted_random,&rng,&visual)==1);
        CHECK(visual.arrow==-1);
    }
    fd2_dialog_wait_state before=state;visual=(fd2_dialog_wait_visual){77,88};
    CHECK(fd2_dialog_wait_begin(&state,tick_ns(18),0,NULL,&rng)==-1);
    CHECK(state.last_tick==before.last_tick&&state.countdown==before.countdown&&
        visual.portrait==77&&visual.arrow==88);
    CHECK(fd2_dialog_wait_begin(&state,tick_ns(18),2,counted_random,&rng)==-1);
    CHECK(state.last_tick==before.last_tick&&state.countdown==before.countdown);
    return 0;
}

static int test_wait_signed_word_rollover_does_not_use_prepare_backwards_gate(void) {
    fd2_dialog_wait_state state;
    random_probe rng={0,0};fd2_dialog_wait_visual visual={77,88};
    CHECK(fd2_dialog_wait_begin(&state,tick_ns(32767),0,counted_random,&rng)==0);
    CHECK(state.last_tick==32767 && rng.calls==1);
    fd2_dialog_wait_state before=state;
    const unsigned rolled[]={32768,65535,65536,65537};
    for(unsigned i=0;i<sizeof(rolled)/sizeof(rolled[0]);i++) {
        CHECK(advance_wait_fixture(&state,tick_ns(rolled[i]),counted_random,&rng,&visual)==0);
        CHECK(!memcmp(&state,&before,sizeof(state)) && rng.calls==1);
        CHECK(visual.portrait==-1 && visual.arrow==-1);
    }
    CHECK(fd2_dialog_wait_begin(&state,tick_ns(65535),0,counted_random,&rng)==0);
    CHECK(state.last_tick==-1 && rng.calls==2);
    before=state;
    CHECK(advance_wait_fixture(&state,tick_ns(65536),counted_random,&rng,&visual)==0);
    CHECK(!memcmp(&state,&before,sizeof(state)) && rng.calls==2);
    CHECK(advance_wait_fixture(&state,tick_ns(65537)-1u,counted_random,&rng,&visual)==0);
    CHECK(!memcmp(&state,&before,sizeof(state)) && rng.calls==2);
    CHECK(advance_wait_fixture(&state,tick_ns(65537),counted_random,&rng,&visual)==1);
    CHECK(state.last_tick==1 && state.countdown==1 && rng.calls==2);
    return 0;
}

static int test_scroll_cpu_write_set(void) {
    static uint8_t actual[64000],reference[64000],step_source[64000];
    for(unsigned origin_index=0;origin_index<2;origin_index++) {
        for(size_t i=0;i<sizeof(actual);i++)actual[i]=(uint8_t)(i*19u+i/320u);
        memcpy(reference,actual,sizeof(actual));
        unsigned origin=origin_index?36887u:1832u;
        size_t text=origin_index?119u*320u+95u:9u*320u+15u;
        /* 独立逐byte模型：每轮读该轮快照，验证整64000B写集，非只验一行。
         * 目标行在源行之前，原顺序memmove与此轮快照读等价。 */
        for(unsigned step=0;step<6;step++) {
            memcpy(step_source,reference,sizeof(reference));
            unsigned delta=step==5?4u:3u;
            for(size_t row=0;row<72;row++)for(size_t x=0;x<208;x++)
                reference[text+row*320u+x-1u]=step_source[text+(row+delta)*320u+x-1u];
            for(size_t x=0;x<208;x++)reference[text+72u*320u+x]=74;
        }
        CHECK(fd2_dialog_scroll_pixels(actual,sizeof(actual),origin)==0);
        CHECK(!memcmp(actual,reference,sizeof(actual)));
        memcpy(step_source,actual,sizeof(actual));
        CHECK(fd2_dialog_scroll_pixels(actual,sizeof(actual)-1,origin)==-1);
        CHECK(!memcmp(actual,step_source,sizeof(actual)));
        CHECK(fd2_dialog_scroll_pixels(actual,sizeof(actual),0)==0);
        CHECK(!memcmp(actual,step_source,sizeof(actual)));
        CHECK(fd2_dialog_scroll_pixels(NULL,0,origin)==-1);
        CHECK(fd2_dialog_scroll_pixels(NULL,0,1)==0);
    }
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
    fd2_bios_clock_reset(0,0);
    if (test_shared_glyph_phase_survives_display_only_close_and_new_consumer()!=0 ||
        test_wait_source_postdecrement_and_rng()!=0 ||
        test_wait_signed_word_rollover_does_not_use_prepare_backwards_gate()!=0 ||
        test_scroll_cpu_write_set()!=0 ||
        test_wait_boundary_unit_cases() != 0 ||
        test_multi_speaker_fragment_waits() != 0)
        return 1;
    puts("dialog_flow_test: ok");
    return 0;
}
