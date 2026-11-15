/* 炎龙骑士团 2 SDL3 重写 - 对话 token 等待边界
 *
 * 逆向依据：text_dialog_render_tokens @VA 0x15f84。已有对话框遇到
 * -17..-20 时先调用 FUN_00016c57(0) 等待，再关闭旧框；-3 调用
 * FUN_00016c57(1) 等待继续；第四行溢出则先用
 * dialog_text_scroll_up (FUN_00016e24) 上卷旧
 * 对话，再把逻辑行号退回 2；-1 在关闭最终框前调用 FUN_00016c57(0)。
 */
#include "dialog_flow.h"
#include "bios_clock.h"

#include <string.h>

/* text_dialog_glyph_step @0x164e8（旧Ghidra未独立识别边界）：唯一计数写点。
 * 仅投影该helper两个DWORD的终态和是否重画，实际portrait/cue/wait归caller。 */
int fd2_dialog_visual_glyph_step(fd2_dialog_visual_state *state,
        unsigned *draw_phase) {
    if (!state || !draw_phase || state->glyph_counter>1u || state->mouth_phase>3u)
        return -1;
    if (++state->glyph_counter!=2u) return 0;
    state->glyph_counter=0;
    state->mouth_phase=(state->mouth_phase+1u)&3u;
    *draw_phase=state->mouth_phase==3u ? 1u:(unsigned)state->mouth_phase;
    return 1;
}

/* dialog_wait_key @0x16c57（原登记同名）：入口一次RNG，闭眼后一相位
 * 恢复frame0并再取RNG；闭眼门是post-decrement旧值0，不是减到0。 */
int fd2_dialog_wait_begin(fd2_dialog_wait_state *state, uint64_t now_ns,
    int arrow_mode, fd2_dialog_random_fn rng, void *userdata) {
    if(!state || !rng || (arrow_mode!=0 && arrow_mode!=1)) return -1;
    *state=(fd2_dialog_wait_state){fd2_bios_tick_signed_ns(now_ns),rng(userdata)%30u+2u,
        0,18,0,(uint8_t)arrow_mode};
    return 0;
}

int fd2_dialog_wait_step(fd2_dialog_wait_state *state, uint64_t now_ns,
    fd2_dialog_wait_visual *visual) {
    if(!state || !visual || state->blink_closed>1u ||
        state->arrow_mode>1u || state->arrow_ticks>2u ||
        (state->arrow_frame!=18u && state->arrow_frame!=19u) ||
        (!state->blink_closed && state->countdown>31u)) return -1;
    *visual=(fd2_dialog_wait_visual){-1,-1};
    int32_t tick=fd2_bios_tick_signed_ns(now_ns);
    /* 16d13..16d18只有signed差>=2；低WORD翻负或回退不另造推进门。 */
    if(tick-state->last_tick<2) return 0;
    if(state->arrow_mode && ++state->arrow_ticks==3u) {
        state->arrow_ticks=0;
        state->arrow_frame=state->arrow_frame==18u?19u:18u;
        visual->arrow=state->arrow_frame;
    }
    if(state->blink_closed) {
        visual->portrait=0;
        state->blink_closed=0;
    } else if(state->countdown--==0u) {
        visual->portrait=3;
        state->blink_closed=1;
    }
    return 1;
}

int fd2_dialog_scroll_pixels(uint8_t *pixels, size_t size, unsigned origin) {
    if(origin!=1832u && origin!=36887u) return 0;
    if(!pixels || size<320u*200u) return -1;
    size_t text=origin==1832u ? 9u*320u+15u : 119u*320u+95u;
    for(unsigned step=0;step<6;step++) {
        unsigned delta=step<5?3u:4u;
        for(size_t row=0;row<72;row++)
            memmove(pixels+text+row*320u-1u,
                pixels+text+(row+delta)*320u-1u,208u);
        memset(pixels+text+72u*320u,74,208u);
    }
    return 0;
}

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
