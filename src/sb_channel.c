/* SPDX-License-Identifier: MIT
 * 固定官方b6abbd5980a885f5f310a4088c59a8688d1b116c的数值关系独立实现。
 * mixer.cpp:234..238/241..290/298..365/367..432/438..610，
 * sblaster.cpp:2674..2719及profile-source02：22050 U8 mono、48k、alpha38711。
 * 无GPL代码复制；无DMA/IRQ、宿主时间、设备、分配或第二renderer。
 */
#include "sb_channel.h"
#include <string.h>

static int gain_valid(int32_t value) {
    return value >= 0 && value <= FD2_SB_CHANNEL_GAIN_MAX;
}
static int rate_valid(uint32_t rate) {return rate==11025u||rate==22050u||rate==44100u;}
static uint32_t alpha_for(uint32_t hz) {return hz==5512u?27467u:hz==11025u?38711u:hz==22050u?48672u:0u;}
static int valid(const fd2_sb_channel *ch) {
    return ch && ch->window && rate_valid(ch->rate) && alpha_for(ch->lowpass_hz)==ch->alpha && ch->alpha && ch->written <= FD2_SB_CHANNEL_WINDOW &&
        ch->read <= ch->written && ch->rendered <= FD2_SB_CHANNEL_MS_FRAMES &&
        ch->freq_f <= FD2_SB_CHANNEL_HOST_RATE + 44100u &&
        ch->freq_fslew <= FD2_SB_CHANNEL_HOST_RATE + 44100u &&
        ch->last >= -32768 && ch->last <= 32512 &&
        ch->current >= -32768 && ch->current <= 32512 &&
        (int64_t)ch->delta == (int64_t)ch->current - ch->last &&
        gain_valid(ch->gain[0]) && gain_valid(ch->gain[1]);
}
int fd2_sb_channel_init(fd2_sb_channel *ch,
                        int32_t window[FD2_SB_CHANNEL_WINDOW][2],
                        int32_t left, int32_t right) {
    if (!ch || !window || !gain_valid(left) || !gain_valid(right)) return -1;
    memset(ch, 0, sizeof(*ch));
    ch->window = window; ch->gain[0] = left; ch->gain[1] = right;
    ch->rate=22050u;ch->lowpass_hz=11025u;ch->alpha=38711u;
    return 0;
}
int fd2_sb_channel_set_gain(fd2_sb_channel *ch, int32_t left, int32_t right) {
    if (!valid(ch) || !gain_valid(left) || !gain_valid(right)) return -1;
    ch->gain[0] = left; ch->gain[1] = right; return 0;
}
int fd2_sb_channel_enable(fd2_sb_channel *ch, int enabled) {
    if (!valid(ch)) return -1;
    enabled = enabled != 0;
    if (enabled != ch->enabled) {
        ch->enabled = enabled;
        if (!enabled) ch->freq_f = 0;
    }
    return 0;
}
int fd2_sb_channel_input_needed(const fd2_sb_channel *ch, size_t whole,
                                size_t *bytes) {
    if (!valid(ch) || !bytes || whole > FD2_SB_CHANNEL_MS_FRAMES) return -1;
    size_t needed = 0;
    if (ch->enabled && whole > ch->rendered && whole > ch->written) {
        uint64_t span = (uint64_t)(whole - ch->written) * ch->rate + ch->freq_f;
        needed = (size_t)((span + FD2_SB_CHANNEL_HOST_RATE - 1u) / FD2_SB_CHANNEL_HOST_RATE);
        if (!ch->current_loaded) needed++;
    }
    *bytes = needed; return 0;
}
static void load_u8(fd2_sb_channel *ch, uint8_t byte) {
    ch->last = ch->current;
    ch->current = ((int32_t)byte - 128) * 256;
    ch->delta = ch->current - ch->last;
    ch->current_loaded = 1;
}
static void append_scaled(fd2_sb_channel *ch, int32_t sample) {
    for (size_t side = 0; side < 2; side++)
        ch->window[ch->written][side] = sample * ch->gain[side];
    ch->written++;
}
/* 固定slew请求0，步长恒等source rate；不引入其它format/ratio分支。 */
static void interpolate_to(fd2_sb_channel *ch, size_t bound) {
    while (ch->written < bound && ch->freq_fslew < FD2_SB_CHANNEL_HOST_RATE) {
        int64_t numerator = (int64_t)ch->delta * ch->freq_fslew;
        int32_t value = ch->last + (int32_t)(numerator / FD2_SB_CHANNEL_HOST_RATE);
        append_scaled(ch, value);
        ch->freq_f += ch->rate;
        ch->freq_fslew += ch->rate;
    }
    /* current=last+delta在本固定无slew-clamp域恒成立，无须改写第二份值。 */
    while (ch->written < bound && ch->freq_f < FD2_SB_CHANNEL_HOST_RATE) {
        append_scaled(ch, ch->current);
        ch->freq_f += ch->rate;
    }
}
int fd2_sb_channel_add_u8(fd2_sb_channel *ch, const uint8_t *bytes,
                          size_t count, size_t *loaded_bytes) {
    if (!valid(ch) || !loaded_bytes || (count && !bytes)) return -1;
    size_t loaded = 0;
    if (ch->written == FD2_SB_CHANNEL_WINDOW) { *loaded_bytes = 0; return 1; }
    if (!ch->current_loaded) {
        if (!count) { *loaded_bytes = 0; return 0; }
        load_u8(ch, bytes[loaded++]);
        if (loaded == count) {
            ch->freq_f = ch->freq_fslew = FD2_SB_CHANNEL_HOST_RATE;
            *loaded_bytes = loaded; return 0;
        }
        load_u8(ch, bytes[loaded++]);
        ch->freq_f = ch->freq_fslew = 0;
    }
    for (;;) {
        if (ch->freq_f >= FD2_SB_CHANNEL_HOST_RATE) {
            if (loaded == count) break;
            load_u8(ch, bytes[loaded++]);
            ch->freq_f -= FD2_SB_CHANNEL_HOST_RATE;
            ch->freq_fslew = ch->freq_f;
        }
        interpolate_to(ch, FD2_SB_CHANNEL_WINDOW);
        if (ch->written == FD2_SB_CHANNEL_WINDOW) break;
    }
    *loaded_bytes = loaded;
    return loaded < count ? FD2_SB_CHANNEL_FULL : FD2_SB_CHANNEL_OK;
}
int fd2_sb_channel_set_rate(fd2_sb_channel *ch,uint32_t rate) {
    if(!valid(ch)||!rate_valid(rate))return -1;
    /* den remains1: SetFreq leaves both phase cursors and all samples intact. */
    ch->rate=rate;return 0;
}
int fd2_sb_channel_set_lowpass(fd2_sb_channel *ch,uint32_t hz) {
    uint32_t alpha=alpha_for(hz);if(!valid(ch)||!alpha)return -1;
    ch->lowpass_hz=hz;ch->alpha=alpha;return 0;
}
int fd2_sb_channel_configure_22050(fd2_sb_channel *ch) {
    if(!valid(ch))return -1;
    ch->rate=22050u;ch->lowpass_hz=11025u;ch->alpha=38711u;return 0;
}
static int32_t floor_shift16(int64_t value) {
    int64_t quotient = value / 65536;
    return (int32_t)(quotient - (value % 65536 < 0));
}
int fd2_sb_channel_render_until(fd2_sb_channel *ch, int32_t (*out)[2],
                                size_t out_frames, size_t whole) {
    if (!valid(ch) || whole > FD2_SB_CHANNEL_MS_FRAMES ||
        out_frames < whole || (whole && !out)) return -1;
    if (whole <= ch->rendered) return 0;
    if (!ch->enabled) { ch->rendered = whole; return 0; }
    if (ch->written < whole) {
        if (ch->current_loaded) interpolate_to(ch, whole);
        if (ch->written < whole) {
            if (ch->freq_f > FD2_SB_CHANNEL_HOST_RATE) ch->freq_f = FD2_SB_CHANNEL_HOST_RATE;
            while (ch->written < whole) {
                /* 原pad是raw current，不乘Q13；不能擅自修成平滑scaled hold。 */
                ch->window[ch->written][0] = ch->current;
                ch->window[ch->written][1] = ch->current;
                ch->written++;
            }
        }
    }
    while (ch->rendered < whole && ch->read < whole) {
        for (size_t side = 0; side < 2; side++) {
            int64_t blended = (int64_t)ch->window[ch->read][side] * ch->alpha +
                (int64_t)ch->lowpass[side] * (65536 - ch->alpha);
            int32_t value = floor_shift16(blended);
            ch->window[ch->read][side] = ch->lowpass[side] = value;
            out[ch->rendered][side] = value;
        }
        ch->read++; ch->rendered++;
    }
    return 0;
}
int fd2_sb_channel_end_frame(fd2_sb_channel *ch) {
    if (!valid(ch) || ch->rendered != FD2_SB_CHANNEL_MS_FRAMES) return -1;
    const size_t step = FD2_SB_CHANNEL_MS_FRAMES;
    if (ch->written <= step) ch->written = ch->read = 0;
    else {
        ch->written -= step;
        ch->read = ch->read >= step ? ch->read - step : 0;
        memmove(ch->window, ch->window + step, ch->written * sizeof(*ch->window));
    }
    ch->rendered = 0; return 0;
}
