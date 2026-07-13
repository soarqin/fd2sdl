/* 炎龙骑士团 2 SDL3 重写 - 原版 16 位伪随机流
 *
 * 反汇编依据：FUN_00073df7 @0x73df7。原代码先以 16 位加法累加
 * 0x9014，再连续执行三次 ROL AX,1；EAX 先清零，因此返回新 AX。
 */
#include "field_rng.h"

void fd2_field_rng_seed(fd2_field_rng *rng, uint16_t seed) {
    if (rng) rng->state = seed;
}

uint32_t fd2_field_rng_next(void *userdata) {
    fd2_field_rng *rng = userdata;
    if (!rng) return 0;
    uint16_t state = (uint16_t)(rng->state + 0x9014u);
    state = (uint16_t)((state << 3) | (state >> 13));
    rng->state = state;
    return state;
}
