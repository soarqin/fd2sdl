/* 炎龙骑士团 2 SDL3 重写 - 原版 16 位伪随机流
 *
 * 反汇编依据：field_rng_next @0x4ebe3（原FUN_00073df7）。原代码先以 16 位加法累加
 * 0x9014，再连续执行三次 ROL AX,1；EAX 先清零，因此返回新 AX。
 * 启动依据原 main @0x25bf4 的指令0x25d90..0x25db1 与 F1 startup-original-cpu／
 * startup-native02：CRT 后 rand DWORD=1、共享 RNG WORD=0，标题前按
 * 首次 Watcom rand %256 推进共享流；BIOS 低 WORD 只初始化视觉时钟。
 */
#include "field_rng.h"

#include <stdlib.h>

void fd2_field_rng_seed(fd2_field_rng *rng, uint16_t seed) {
    if (rng) rng->state = seed;
}

uint32_t fd2_field_rng_bootstrap(fd2_field_rng *rng, const char *debug_seed) {
    if (!rng) return 0;
    /* 显式调试值是推进后的流状态，保留历史固定 seed 夹具。 */
    if (debug_seed && debug_seed[0] >= '0' && debug_seed[0] <= '9') {
        char *end = NULL;
        unsigned long parsed = strtoul(debug_seed, &end, 0);
        if (end && *end == '\0' && parsed <= UINT16_MAX) {
            fd2_field_rng_seed(rng, (uint16_t)parsed);
            return 0;
        }
    }
    fd2_field_rng_seed(rng, 0);
    /* 只重建本次启动的 CRT rand 来源，不另持有运行期随机流。 */
    uint32_t rand_state = 1;
    rand_state = rand_state * UINT32_C(1103515245) + UINT32_C(12345);
    uint32_t rand_value = (rand_state >> 16) & UINT32_C(0x7fff);
    uint32_t calls = rand_value % 256u;
    for (uint32_t i = 0; i < calls; i++) (void)fd2_field_rng_next(rng);
    return calls;
}

uint32_t fd2_field_rng_next(void *userdata) {
    fd2_field_rng *rng = userdata;
    if (!rng) return 0;
    uint16_t state = (uint16_t)(rng->state + 0x9014u);
    state = (uint16_t)((state << 3) | (state >> 13));
    rng->state = state;
    return state;
}
