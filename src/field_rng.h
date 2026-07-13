/* 炎龙骑士团 2 SDL3 重写 - 原版 16 位伪随机流
 *
 * 复现 FUN_00073df7 @0x73df7：state = rol16(state + 0x9014, 3)。
 * loader 初始 state 已由 DOSBox debugger 捕获为 0x7a18；调用方仍显式
 * 持有状态，确保战斗外围特效与伤害结算共用同一 RNG 流。
 */
#ifndef FD2_FIELD_RNG_H
#define FD2_FIELD_RNG_H

#include <stdint.h>

typedef struct {
    uint16_t state;
} fd2_field_rng;

void fd2_field_rng_seed(fd2_field_rng *rng, uint16_t seed);

/* 签名兼容 fd2_field_combat_rng_fn；返回值低 16 位是原版新状态。 */
uint32_t fd2_field_rng_next(void *userdata);

#endif /* FD2_FIELD_RNG_H */
