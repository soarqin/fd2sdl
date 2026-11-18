/* 音乐／音效的共同整数调度：保留原8333/5000us累计节奏与54925us游戏tick。
 * 两个实际服务共用一个时基；不模拟DOS句柄、IRQ、EOI或中断嵌套。 */
#ifndef FD2_AIL_TIMER_H
#define FD2_AIL_TIMER_H

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/* 服务ID不透明，不再表示原4字节偏移。 */
enum { FD2_AIL_TIMER_MDI, FD2_AIL_TIMER_DIG, FD2_AIL_TIMER_SLOTS };
#define FD2_AIL_TIMER_INVALID UINT32_MAX
#define FD2_AIL_TIMER_BIOS_US UINT32_C(54925)
typedef void (*fd2_ail_timer_callback)(void *user);
typedef struct fd2_ail_timer_slot {
    uint32_t period_us, accumulator_us;
    fd2_ail_timer_callback callback;
    void *user;
} fd2_ail_timer_slot;
typedef struct fd2_ail_timer {
    fd2_ail_timer_slot slots[FD2_AIL_TIMER_SLOTS];
    uint32_t base_us, interval_count, bios_accumulator_us;
} fd2_ail_timer;

void fd2_ail_timer_fresh_init(fd2_ail_timer *timer);
/* 一次完成实际服务的安装；重复安装同服务失败且不改timer。
 * 较短周期成为共同base时，保留已验的全部累计清零行为。 */
int fd2_ail_timer_install(fd2_ail_timer *timer, uint32_t service,
                          fd2_ail_timer_callback callback, void *user);
/* 只结束对应借用，不重定共同边界。INVALID可供未安装的清理路径调用。 */
int fd2_ail_timer_release(fd2_ail_timer *timer, uint32_t service);
/* 仅clock owner调用：先算两个到期项，再按音乐→音效执行，最后返回游戏tick。
 * 0/1为本轮游戏tick数，-1为坏输入。回调不递归推进／控制此调度器。 */
int fd2_ail_timer_step(fd2_ail_timer *timer);

#ifdef __cplusplus
}
#endif
#endif
