/* 炎龙骑士团 2 SDL3 重写 - 原版 16 位伪随机流
 *
 * 复现 FUN_00073df7 @0x4ebe3：state = rol16(state + 0x9014, 3)。
 * 原文件及真实 CRT 后共享 state=0；main 的首次 Watcom rand 决定标题前
 * 推进次数。调用方持有唯一状态，特效、等待与战斗结算共用同一流。
 */
#ifndef FD2_FIELD_RNG_H
#define FD2_FIELD_RNG_H

#include <stdint.h>

typedef struct {
    uint16_t state;
} fd2_field_rng;

void fd2_field_rng_seed(fd2_field_rng *rng, uint16_t seed);

/* 新 App 启动一次：原 rand 初态1、共享初态0，返回标题前推进次数。
 * debug_seed 接受无符号、无空白的 0..65535 数字（strtoul base 0）；有效
 * 值直接覆盖推进后的状态并返回0。NULL、空串或非法值恢复正式启动。
 * 此接口不读取宿主时间；Game／Scene／prepare／Load 不重复调用。 */
uint32_t fd2_field_rng_bootstrap(fd2_field_rng *rng, const char *debug_seed);

/* 签名兼容 fd2_field_combat_rng_fn；返回值低 16 位是原版新状态。 */
uint32_t fd2_field_rng_next(void *userdata);

#endif /* FD2_FIELD_RNG_H */
