/* SPDX-License-Identifier: MIT
 * 原SB16.DIG 401/402的双缓冲播放关系；只保游戏实际消费的完成/暂停。
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
        s->left <= FD2_SB_BLOCK && s->total <= FD2_SB_BLOCK &&
        s->mode >= FD2_SB_NONE && s->mode <= FD2_SB_PAUSED &&
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
static int resume_buffer(fd2_sb_dma_provider *,const fd2_sb_point *);
static int point_valid(const fd2_sb_point *p) {
    return p && isfinite(p->tick_index);
}
static int schedule_short_tail(fd2_sb_dma_provider *s, const fd2_sb_point *p)
{
    const uint32_t minimum=(22050u*3u)/1000u;
    if (s->left && s->left < minimum) {
        float count = (float)s->left;
        float delay = (count * 1000.0f) / 22050.0f;
        double anchor = p->tick_index;
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
static int begin_buffer(fd2_sb_dma_provider *s, const fd2_sb_point *point)
{
    /* 先关闭旧声音前缀，再允许从新缓冲取得样本。 */
    if (s->mode == FD2_SB_UPDATING) {
        if (fill(s)) return s->first_error;
        s->mode = FD2_SB_RUNNING;
        return schedule_short_tail(s, point);
    }
    return 0;
}
int fd2_sb_dma_init(fd2_sb_dma_provider *s, void *user,
    int (*add_u8)(void *, const uint8_t *, size_t), int (*fill_up)(void *))
{
    if (!s || !add_u8 || !fill_up)
        return FD2_SB_H_ARGUMENT;
    memset(s, 0, sizeof(*s));
    s->user = user;
    s->add_u8 = add_u8; s->fill_up = fill_up;
    /* 首次使用half0；暂停后以-1表示暂不请求新的DIG半块。 */
    return 0;
}
/* 重新开始环缓冲前先提交旧声音前缀；保持续channel历史。 */
static int start_transfer(fd2_sb_dma_provider *s,const uint8_t *ring,
                             size_t bytes,const fd2_sb_point *p)
{
    int r = ready(s);
    if (r) return r;
    if (!ring || bytes != FD2_SB_RING || !point_valid(p))
        return fail(s, FD2_SB_H_ARGUMENT);
    s->ring = ring; s->cursor = 0;
    /* 先标记新缓冲，再关闭旧声音前缀。 */
    s->total=FD2_SB_BLOCK;
    s->mode = FD2_SB_UPDATING;
    /* left/pending重置前关闭旧mixer前缀。 */
    if (fill(s)) return s->first_error;
    s->left = s->total = FD2_SB_BLOCK;
    s->block_ready = 0;
    cancel_end(s);
    if (begin_buffer(s, p)) return s->first_error;
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
        if(resume_buffer(s,p)<0)return s->first_error;
        s->driver_state=3;return 0;
    }
    r=start_transfer(s,ring,bytes,p);if(r)return r;
    s->driver_half=0;s->driver_state=3;return 0;
}
static int halt_buffer(fd2_sb_dma_provider *s) {
    int r=ready(s);if(r)return r;
    if(fill(s))return s->first_error;
    s->mode=FD2_SB_PAUSED;
    cancel_end(s);
    return 0;
}
static int resume_buffer(fd2_sb_dma_provider *s,const fd2_sb_point*p) {
    int r=ready(s);if(r)return r;if(!point_valid(p))return fail(s,FD2_SB_H_ARGUMENT);
    if(fill(s))return s->first_error;
    if(s->mode==FD2_SB_PAUSED){s->mode=FD2_SB_UPDATING;return begin_buffer(s,p);}
    return 0;
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
    int r = ready(s);
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
    /* 缓冲先消费整count，再送采样器；不能因预取窗截断而重复读尾部。 */
    s->cursor = (s->cursor + n) % FD2_SB_RING;
    s->dma_reads += n;
    if (s->add_u8(s->user, bytes, n) < 0) return fail(s, FD2_SB_H_SINK);
    s->left -= (uint32_t)n;
    if (!s->left) {
        cancel_end(s);
        s->block_ready = 1;
        s->block_ends++;
        s->left = s->total;
    }
    return 0;
}
int fd2_sb_dma_generate(fd2_sb_dma_provider *s, size_t requested)
{
    int r = ready(s);
    if (r) return r;
    return s->mode == FD2_SB_RUNNING ? generate_read(s, requested) : 0;
}
int fd2_sb_dma_complete_block(fd2_sb_dma_provider *s)
{
    int r = ready(s);
    if (r) return r;
    if (!s->block_ready) return fail(s, FD2_SB_H_ARGUMENT);
    /* 先完成旧缓冲，再按暂停请求或正常播放交接half。 */
    s->block_ready=0;
    if (s->driver_state == 2) {
        /* 暂停前仍须提交已到达的声音尾部。 */
        if(halt_buffer(s)<0)return s->first_error;
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
    /* 已排定短尾只消费一次；暂停或块完成会取消该任务。 */
    s->end.pending = 0;
    return generate_read(s, s->end.count);
}
