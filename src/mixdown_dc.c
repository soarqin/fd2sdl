/* SPDX-License-Identifier: MIT
 * 依据官方mixer.cpp:65..84/699..735/1246与mixer.h:44..45独立实现。
 * 共同FM+SB int32Q13 -> DC -> record Q13 -> shift26 -> S16，保持整数。
 */
#include "mixdown_dc.h"
#include <limits.h>

static int fits_i32(int64_t value) { return value>=INT32_MIN && value<=INT32_MAX; }
/* 显式floor除法复现固定MSVC算术右移，避免C11负数右移的实现依赖。 */
static int64_t floor_div(int64_t value,int64_t divisor)
{
    int64_t q=value/divisor;
    return q-(value%divisor<0);
}
static int16_t clip16(int64_t value)
{
    if(value>INT16_MAX)return INT16_MAX;
    if(value<INT16_MIN)return INT16_MIN;
    return (int16_t)value;
}
int fd2_mixdown_dc_step(fd2_mixdown_dc *state,const int32_t fm[2],
    const int32_t sb[2],const int32_t gain[2],int enabled,fd2_mixdown_frame *out)
{
    fd2_mixdown_frame frame;
    int32_t next[2];
    unsigned ch;
    if(!state||!fm||!sb||!gain||!out||(enabled!=0&&enabled!=1))return FD2_MIXDOWN_H_ARGUMENT;
    for(ch=0;ch<2;ch++) {
        int64_t sum=(int64_t)fm[ch]+sb[ch];
        int64_t ns=sum;
        int64_t dc=state->adjustment[ch];
        if(!fits_i32(sum))return FD2_MIXDOWN_H_SOURCE_I32;
        if(enabled) {
            int64_t scale,delta;
            ns+=dc;
            if(!fits_i32(ns))return FD2_MIXDOWN_H_SOURCE_I32;
            /* 两端等于30000<<13时原走64倍快支，不是<=。 */
            scale=(ns>-INT64_C(245760000)&&ns<INT64_C(245760000))?1:64;
            delta=floor_div(ns*INT64_C(349)*scale,INT64_C(16777216));
            dc-=delta;
            if(!fits_i32(dc))return FD2_MIXDOWN_H_SOURCE_I32;
        }
        frame.summed[ch]=(int32_t)sum;
        frame.corrected[ch]=(int32_t)ns;
        frame.pcm[ch]=clip16(floor_div(ns*(int64_t)gain[ch],INT64_C(67108864)));
        next[ch]=(int32_t)dc;
    }
    /* 仅合法整帧提交；不在中途给调用者半帧或暗改未来DC。 */
    state->adjustment[0]=next[0];state->adjustment[1]=next[1];
    *out=frame;
    return 0;
}
