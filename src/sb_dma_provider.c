/* SPDX-License-Identifier: MIT
 * 原SB16.DIG 401/402/IRQ与固定官方SB DMA函数的独立有限实现。
 * 保留环缓冲、暂停/恢复和短尾样本；不模拟端口或安装探测。
 */
#include "sb_dma_provider.h"
#include <math.h>
#include <string.h>

static int fail(fd2_sb_dma_provider *s, int error)
{
    if (s && !s->first_error) s->first_error = error;
    return s && s->first_error ? s->first_error : error;
}
static int valid(const fd2_sb_dma_provider *s)
{
    return s && s->add_u8 && s->fill_up && s->cursor < FD2_SB_RING &&
        (s->rate==11025u||s->rate==22050u||s->rate==44100u) &&
        s->left <= FD2_SB_BLOCK && s->total <= FD2_SB_BLOCK &&
        s->mode >= FD2_SB_NONE && s->mode <= FD2_SB_ACK &&
        (s->driver_half == -1 || s->driver_half == 0 || s->driver_half == 1) &&
        s->saved_next_half <= 2 &&
        s->driver_state <= 3;
}
static int ready(fd2_sb_dma_provider *s)
{
    if (s && s->first_error) return s->first_error;
    return valid(s) ? 0 : fail(s, FD2_SB_H_ARGUMENT);
}
static int fill(fd2_sb_dma_provider *s)
{
    return s->fill_up(s->user) < 0 ? fail(s, FD2_SB_H_FILLUP) : s->first_error;
}
static void cancel_end(fd2_sb_dma_provider *s) {
    s->end.pending = 0;
}
static int point_valid(const fd2_sb_point *p) {
    return p && isfinite(p->tick_index) && isfinite(p->service_index) &&
        (p->servicing_event==0 || p->servicing_event==1);
}
static int check_end_on_unmask(fd2_sb_dma_provider *s, const fd2_sb_point *p)
{
    const uint32_t minimum=(s->rate*3u)/1000u;
    if (s->left && s->left < minimum) {
        float count = (float)s->left;
        float delay = (count * 1000.0f) / (float)s->rate;
        double anchor = p->servicing_event ? p->service_index : p->tick_index;
        if (s->next_serial == UINT64_MAX) return fail(s, FD2_SB_H_EVENT);
        s->end.serial = ++s->next_serial;
        s->end.tick = p->tick;
        s->end.index = anchor + (double)delay;
        s->end.delay_ms = delay;
        s->end.count = s->left;
        s->end.pending = 1;
        return 0;
    }
    return 0;
}
static int unmask(fd2_sb_dma_provider *s, const fd2_sb_point *point)
{
    /* Register_Callback->SetMask(masked)->DMA_UNMASKED->DSP_ChangeMode。 */
    if (s->mode == FD2_SB_MASKED) {
        if (fill(s)) return s->first_error;
        s->mode = FD2_SB_DMA;
        return check_end_on_unmask(s, point);
    }
    return 0;
}
int fd2_sb_dma_init(fd2_sb_dma_provider *s, int require_ack, void *user,
    int (*add_u8)(void *, const uint8_t *, size_t), int (*fill_up)(void *))
{
    if (!s || !add_u8 || !fill_up || (require_ack != 0 && require_ack != 1))
        return FD2_SB_H_ARGUMENT;
    memset(s, 0, sizeof(*s));
    s->require_ack = require_ack; s->user = user;s->rate=22050u;
    s->add_u8 = add_u8; s->fill_up = fill_up;
    /* 原driver静态初值0504=0；ffff只在IRQ的pause分支写。 */
    return 0;
}
int fd2_sb_dma_reset(fd2_sb_dma_provider *s) {
    if(!valid(s))return -1;
    int result=s->first_error;
    s->irq_pending=0;
    if(s->mode!=FD2_SB_NONE && fill(s)<0)result=s->first_error;
    s->mode=FD2_SB_NONE;s->left=s->total=0;s->irq_pending=0;
    s->end.pending=0;
    return result;
}
/* 重新开始环缓冲前先提交旧声音前缀；保持续channel历史。 */
int fd2_sb_dma_start_transfer(fd2_sb_dma_provider *s,const uint8_t *ring,
                             size_t bytes,const fd2_sb_point *p)
{
    int r = ready(s);
    if (r) return r;
    if (!ring || bytes != FD2_SB_RING || !p || !isfinite(p->tick_index) ||
        !isfinite(p->service_index) || (p->servicing_event != 0 && p->servicing_event != 1))
        return fail(s, FD2_SB_H_ARGUMENT);
    s->ring = ring; s->cursor = 0;
    /* 先标记新缓冲，再关闭旧声音前缀。 */
    s->total=FD2_SB_BLOCK;
    s->mode = FD2_SB_MASKED;
    /* left/pending重置前关闭旧mixer前缀。 */
    if (fill(s)) return s->first_error;
    s->left = s->total = FD2_SB_BLOCK;
    s->irq_pending = 0;
    s->rate=22050u;
    if(s->configure_22050 && s->configure_22050(s->user)<0)return fail(s,FD2_SB_H_SINK);
    cancel_end(s);
    if (unmask(s, p)) return s->first_error;
    return 0;
}
int fd2_sb_dma_start(fd2_sb_dma_provider *s,const uint8_t *ring,
                     size_t bytes,const fd2_sb_point *p)
{
    int r=ready(s);if(r)return r;
    if(!ring||bytes!=FD2_SB_RING||!point_valid(p))return fail(s,FD2_SB_H_ARGUMENT);
    if(s->driver_state==2||s->driver_state==3){s->driver_state=3;return 0;}
    if(s->saved_next_half){
        s->driver_half=(int16_t)(s->saved_next_half-1u);s->saved_next_half=0;
        if(fd2_sb_dma_resume(s,p)<0)return s->first_error;
        s->driver_state=3;return 0;
    }
    r=fd2_sb_dma_start_transfer(s,ring,bytes,p);if(r)return r;
    s->driver_half=0;s->driver_state=3;return 0;
}
int fd2_sb_dma_bind_rate(fd2_sb_dma_provider *s,int(*set_rate)(void*,uint32_t)) {
    if(ready(s)||s->ring||s->mode!=FD2_SB_NONE||s->set_rate||!set_rate)return -1;
    s->set_rate=set_rate;return 0;
}
int fd2_sb_dma_halt(fd2_sb_dma_provider *s) {
    int r=ready(s);if(r)return r;
    if(fill(s))return s->first_error;
    s->mode=FD2_SB_PAUSED;
    cancel_end(s);
    return 0;
}
int fd2_sb_dma_resume(fd2_sb_dma_provider *s,const fd2_sb_point*p) {
    int r=ready(s);if(r)return r;if(!point_valid(p))return fail(s,FD2_SB_H_ARGUMENT);
    if(fill(s))return s->first_error;
    if(s->mode==FD2_SB_PAUSED){s->mode=FD2_SB_MASKED;return unmask(s,p);}
    return 0;
}
int fd2_sb_dma_set_output_rate(fd2_sb_dma_provider *s,uint32_t rate) {
    int r=ready(s);if(r)return r;
    if(rate!=11025u&&rate!=22050u&&rate!=44100u)return fail(s,FD2_SB_H_ARGUMENT);
    if(s->rate!=rate&&s->mode!=FD2_SB_NONE) {
        if(fill(s))return s->first_error;
        if(!s->set_rate||s->set_rate(s->user,rate)<0)return fail(s,FD2_SB_H_SINK);
    }
    s->rate=rate;return 0; /* no filter reconfiguration: DSP_ChangeRate only SetFreq. */
}
int fd2_sb_dma_read_half(fd2_sb_dma_provider *s, int16_t *half)
{
    int r = ready(s);
    if (r) return r;
    if (!half) return fail(s, FD2_SB_H_ARGUMENT);
    *half = s->driver_half;
    return 0;
}
int fd2_sb_dma_request_pause(fd2_sb_dma_provider *s)
{
    int r = ready(s);
    if (r) return r;
    if (s->driver_state == 3) s->driver_state = 2;
    return 0;
}
static int generate_read(fd2_sb_dma_provider *s, size_t requested)
{
    uint8_t bytes[1024];
    size_t n, first;
    int was_pending, r = ready(s);
    if (r) return r;
    if (!requested) return 0;
    if (!s->ring || !s->left || !s->total) return fail(s, FD2_SB_H_ARGUMENT);
    n = requested;
    if (n > s->left) n = s->left;
    if (n > sizeof(bytes)) n = sizeof(bytes);
    first = FD2_SB_RING - s->cursor;
    if (first > n) first = n;
    memcpy(bytes, s->ring + s->cursor, first);
    if (n > first) memcpy(bytes + first, s->ring, n - first);
    /* 原8237 Read在AddSamples前完整消费；不依converter loaded量回退。 */
    s->cursor = (s->cursor + n) % FD2_SB_RING;
    s->dma_reads += n;
    if (s->add_u8(s->user, bytes, n) < 0) return fail(s, FD2_SB_H_SINK);
    s->left -= (uint32_t)n;
    if (!s->left) {
        cancel_end(s);
        was_pending = s->irq_pending;
        s->irq_pending = 1;
        s->block_ends++;
        s->left = s->total;
        if (s->require_ack && was_pending) s->mode = FD2_SB_ACK;
    }
    return 0;
}
int fd2_sb_dma_generate(fd2_sb_dma_provider *s, size_t requested)
{
    int r = ready(s);
    if (r) return r;
    return s->mode == FD2_SB_DMA ? generate_read(s, requested) : 0;
}
static int acknowledge(fd2_sb_dma_provider *s)
{
    int r=ready(s);if(r)return r;
    s->irq_pending=0;
    if(s->mode==FD2_SB_ACK) {
        if(fill(s))return s->first_error;
        s->mode=FD2_SB_DMA;
    }
    return 0;
}
int fd2_sb_dma_deliver_irq(fd2_sb_dma_provider *s)
{
    int r = ready(s);
    if (r) return r;
    if (!s->irq_pending) return fail(s, FD2_SB_H_ARGUMENT);
    /* 先完成旧缓冲，再按暂停请求或正常播放交接half。 */
    if(acknowledge(s)<0)return s->first_error;
    if (s->driver_state == 2) {
        /* D0的FillUp在暂停写前，仍可消费已提交DMA尾。 */
        if(fd2_sb_dma_halt(s)<0)return s->first_error;
        s->saved_next_half = (uint16_t)((s->driver_half ^ 1) + 1);
        s->driver_half = -1;
        s->driver_state = 0;
    } else if (s->driver_state == 3) {
        s->driver_half ^= 1;
    }
    return 0;
}
int fd2_sb_dma_dispatch_end(fd2_sb_dma_provider *s, uint64_t serial)
{
    int r = ready(s);
    if (r) return r;
    if (!s->end.pending || s->end.serial != serial)
        return fail(s, FD2_SB_H_EVENT);
    /* 真实END_DMA_Event不检查MODE；D0/块末已经RemoveEvents。 */
    s->end.pending = 0;
    return generate_read(s, s->end.count);
}
