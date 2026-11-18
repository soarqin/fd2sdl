/* SPDX-License-Identifier: MIT
 * 原404c0/49690与SB16.DIG401/402/0682、固定DOSBox-X SB/channel有限桥。
 * 唯一DIG状态借用现owner；不含新clock、IRQ调度器、PCM FIFO或float renderer。
 */
#ifndef FD2_AUDIO_SB_H
#define FD2_AUDIO_SB_H
#include "audio_dig.h"
#include "sb_dma_provider.h"
#include "sb_channel.h"

typedef struct fd2_audio_sb {
    fd2_audio_dig *dig;
    fd2_sb_dma_provider dma;
    fd2_sb_channel channel;
    int32_t window[FD2_SB_CHANNEL_WINDOW][2];
    /* 由parent同clock更新的当前source点；仅借用，不自行累加或采walltime。 */
    const fd2_sb_point *point;
    void *global_user;
    int (*global_fill_up)(void *user);
    int first_error, in_mix, in_control, closed;
} fd2_audio_sb;

/* Audio构造时唯一一次channel/window准备；DIG未绑定仍消费同窗NONE/pad。
 * 原first phase/时点归caller，不借此接口默认原Main起点。 */
int fd2_audio_sb_construct(fd2_audio_sb *,const fd2_sb_point *,
    int32_t left_q13,int32_t right_q13,int (*global_fill_up)(void *),void *);
int fd2_audio_sb_bind_dig(fd2_audio_sb *,fd2_audio_dig *);
/* 给现DIG attach的三叶，不另安装timer或重建P/DMA。 */
int fd2_audio_sb_external(fd2_audio_sb *sb, fd2_dig_external_sb16 *external);
/* 原实际Mix/FillUp已决定whole后调用；最多两个handler请求再补pad。
 * out只是caller借用的本ms整数Q13贡献，不能先转float后做共同DC。 */
int fd2_audio_sb_mix_until(fd2_audio_sb *sb, int32_t (*out)[2],
                          size_t out_frames, size_t whole);
/* 原实际1ms tick尾；不得用每个prefix或设备chunk代替。 */
int fd2_audio_sb_end_frame(fd2_audio_sb *sb);
/* 由同Audio时间轴处理短尾与已消费块，不建立中断/设备时间轴。 */
int fd2_audio_sb_dispatch_end(fd2_audio_sb *sb, uint64_t serial);
int fd2_audio_sb_complete_block(fd2_audio_sb *sb);
/* parent先终结共享clock/输出借用，再关闭本缓冲回调。
 * 保已达P/DMA/channel写集供诊断。不得在FillUp/控制回调内关闭owner。 */
int fd2_audio_sb_close(fd2_audio_sb *sb);
#endif
