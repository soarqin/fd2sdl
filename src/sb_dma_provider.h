/* SPDX-License-Identifier: MIT
 * SB16.DIG 09e4/0acc/0682的双缓冲/短尾语义；不是设备或IRQ模型。
 * U8 mono22050/4096B环/2048B块，服务于Main的共同采样混音；不自读时间。
 */
#ifndef FD2_SB_DMA_PROVIDER_H
#define FD2_SB_DMA_PROVIDER_H
#include <stddef.h>
#include <stdint.h>
enum { FD2_SB_RING = 4096, FD2_SB_BLOCK = 2048 };
enum { FD2_SB_NONE, FD2_SB_RUNNING, FD2_SB_UPDATING, FD2_SB_PAUSED };
enum { FD2_SB_OK = 0, FD2_SB_H_ARGUMENT = -1, FD2_SB_H_SINK = -2,
       FD2_SB_H_FILLUP = -3, FD2_SB_H_EVENT = -4 };

/* 同Audio时间轴中的毫秒窗和窗内位置；本核不采宿主时间。 */
typedef struct fd2_sb_point {
    uint64_t tick;
    double tick_index;
} fd2_sb_point;
typedef struct fd2_sb_end_event {
    uint64_t serial, tick;
    double index;
    float delay_ms;
    uint32_t count;
    int pending;
} fd2_sb_end_event;

typedef struct fd2_sb_dma_provider {
    /* 唯一DIG控制核的DMA借用；不另存sample或DMA副本。 */
    const uint8_t *ring;
    size_t cursor;
    uint32_t left, total;
    int mode, block_ready;
    int16_t driver_half;       /* 当前供DIG查询的半块，-1表示暂停。 */
    uint16_t saved_next_half; /* 恢复播放时继续的半块。 */
    uint16_t driver_state;    /* 0=暂停、2=请求暂停、3=播放。 */
    fd2_sb_end_event end;
    uint64_t next_serial, dma_reads, block_ends;
    int first_error;
    void *user;
    /* DMA Read已经消费count，converter loaded统计绝不能回退此count。 */
    int (*add_u8)(void *user, const uint8_t *bytes, size_t count);
    /* 当前同epoch mixer前缀；须调用全局FillUp，不能只清SB窗。 */
    int (*fill_up)(void *user);
} fd2_sb_dma_provider;

/* 建立空播放缓冲；持续采样相位由同一个Audio channel保存。 */
int fd2_sb_dma_init(fd2_sb_dma_provider *sb, void *user,
    int (*add_u8)(void *, const uint8_t *, size_t), int (*fill_up)(void *));
int fd2_sb_dma_start(fd2_sb_dma_provider *sb, const uint8_t *ring,
                     size_t bytes, const fd2_sb_point *point);
int fd2_sb_dma_read_half(fd2_sb_dma_provider *sb, int16_t *half);
int fd2_sb_dma_request_pause(fd2_sb_dma_provider *sb);
/* 一次普通采样器请求；未开始/暂停/更新缓冲时不取得新样本。 */
int fd2_sb_dma_generate(fd2_sb_dma_provider *sb, size_t requested);
/* 在完成旧块后交接half或执行已请求的暂停。 */
int fd2_sb_dma_complete_block(fd2_sb_dma_provider *sb);
/* Audio选择已到的短尾后调用；serial防旧事件重复消费样本。 */
int fd2_sb_dma_dispatch_end(fd2_sb_dma_provider *sb, uint64_t serial);
#endif
