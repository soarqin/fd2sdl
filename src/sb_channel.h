/* SPDX-License-Identifier: MIT
 * 固定DOSBox-X b6abbd5的SB channel数值合同；本项目独立C实现，非驱动时序。
 * 22050Hz U8 mono→48000Hz，低通11025Hz/order1。
 * 依据mixer.cpp load/AddSamples/Mix/EndFrame及sb-host-profile-source02。
 */
#ifndef FD2_SB_CHANNEL_H
#define FD2_SB_CHANNEL_H
#include <stddef.h>
#include <stdint.h>

enum {
    FD2_SB_CHANNEL_RATE = 22050,
    FD2_SB_CHANNEL_HOST_RATE = 48000,
    FD2_SB_CHANNEL_MS_FRAMES = 48,
    FD2_SB_CHANNEL_WINDOW = 2048,
    FD2_SB_CHANNEL_GAIN_ONE = 8192,
    FD2_SB_CHANNEL_GAIN_MAX = 32768,
    FD2_SB_CHANNEL_ALPHA = 38711
};
enum { FD2_SB_CHANNEL_OK = 0, FD2_SB_CHANNEL_FULL = 1,
       FD2_SB_CHANNEL_INVALID = -1 };

/* 唯一channel状态。window由现SB/provider owner提供并保持有效；不是新的
 * PCM队列/时钟。成员供栈内嵌及独立断言，调用者不得直接改数学游标。 */
typedef struct fd2_sb_channel {
    int32_t (*window)[2];
    size_t written, read, rendered;
    int32_t last, current, delta;
    uint32_t freq_f, freq_fslew;
    int32_t gain[2], lowpass[2];
    int current_loaded;
} fd2_sb_channel;

/* init不清调用者窗；written=0前的字节不作语义输入。gain是明确Q13参数，
 * 范围0..4×unity，不能由本函数猜实际mixer寄存器/捕获gain。 */
int fd2_sb_channel_init(fd2_sb_channel *channel,
                        int32_t window[FD2_SB_CHANNEL_WINDOW][2],
                        int32_t left_q13, int32_t right_q13);
/* 当前1ms窗whole<=48的原Mix请求式；已渲染/预取充足为0。
 * 只给数学需求，绝不加min66、不猜DMA cursor/left/IRQ/时刻。 */
int fd2_sb_channel_input_needed(const fd2_sb_channel *channel, size_t whole,
                                size_t *bytes);
/* 完整给定DMA块经原AddSamples预取至2048窗或输入耗尽。
 * loaded_bytes仅为converter实际读取量；硬件已Read的count不能因此回退或重读。
 * FULL表示窗口截断，保已达前缀；正常provider应遵守其独立请求/读取合同。 */
int fd2_sb_channel_add_u8(fd2_sb_channel *channel, const uint8_t *bytes,
                          size_t count, size_t *loaded_bytes);
/* 表示本轮原Mix最多两次handler尝试已结束，才补残余插值及raw-current hold。
 * 仅新消费前缀作onepole并写out[rendered..whole)，不会重复滤旧前缀。
 * out是独立channel贡献(Q13)，不是music+SFX全局和/DC/最终S16 clip。
 * 无效参数无副作用。 */
int fd2_sb_channel_render_until(fd2_sb_channel *channel, int32_t (*out)[2],
                                size_t out_frames, size_t whole);
/* 固定48k的一毫秒结束；必须已render到48。仅搬余窗，数学/filter不reset。 */
int fd2_sb_channel_end_frame(fd2_sb_channel *channel);
#endif
