/* SPDX-License-Identifier: MIT
 * 固定DOSBox-X b6abbd5 mixer.cpp共同DC/record输出数学叶；非设备/mixer。
 * 输入是有效的FM和SB int32 Q13贡献；gain和dc初值必须由现owner显式给出。
 */
#ifndef FD2_MIXDOWN_DC_H
#define FD2_MIXDOWN_DC_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
enum { FD2_MIXDOWN_OK=0, FD2_MIXDOWN_H_ARGUMENT=-1,
       FD2_MIXDOWN_H_SOURCE_I32=-2 };
typedef struct fd2_mixdown_dc { int32_t adjustment[2]; } fd2_mixdown_dc;
typedef struct fd2_mixdown_frame {
    int32_t summed[2], corrected[2];
    int16_t pcm[2];
} fd2_mixdown_frame;
/* 固定48000Hz step349。先sum，再用旧DC输出/更新未来DC，最后recordgain/clip。
 * gain已是原float recordvol*8192转换后的int32，不在此处猜/解析float增益。
 * 输入任意int32 gain均有安全int64乘法，不引现代volume上限。
 * 源int32 sum/ns/DCnext不在定义域时返回有名H，整帧无写；这是宿主预检政策，
 * 不冒充原发生溢出时的prefix/物理wrap，也不据此缩小游戏正常producer域。
 * 无clock、分配、队列、默认DC状态或黏滞错误；首错仍归唯一Audio owner。 */
int fd2_mixdown_dc_step(fd2_mixdown_dc *state,const int32_t fm[2],
    const int32_t sb[2],const int32_t record_q13[2],int dc_enabled,
    fd2_mixdown_frame *out);
#ifdef __cplusplus
}
#endif
#endif
