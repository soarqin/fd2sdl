/* AIL 16槽有限整数叶：sub_3ea8e / sub_3ec17 / sub_3f236 等。
 * 仅外部 IRQ 输入；不读宿主时间、不持有 BDA、不中断式渲染未来音频。 */
#ifndef FD2_AIL_TIMER_H
#define FD2_AIL_TIMER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FD2_AIL_TIMER_SLOTS 16u
#define FD2_AIL_TIMER_CALLBACK_SLOTS 15u
#define FD2_AIL_TIMER_INVALID UINT32_MAX
#define FD2_AIL_TIMER_BIOS_US UINT32_C(54925)

typedef void (*fd2_ail_timer_callback)(void *user);
typedef struct fd2_ail_timer fd2_ail_timer;

typedef struct fd2_ail_timer_slot {
    uint32_t status;
    uint32_t period_us;
    uint32_t accumulator_us;
    uint32_t pending;
    fd2_ail_timer_callback callback;
    void *user;
} fd2_ail_timer_slot;

/* 都是同步有限边界，不建立线程／时钟：
 * program_pit：原3ebb4完成低／高字节写且恢复flags之后；只通知raw divisor。
 * before_acc_clear：原3ec17逐槽清计数之前的同步观察点。
 * ack_irq：原3eb11或3ebad的EOI点；bios_chain：busy清回后的一次旧IRQ链。
 * hook不得另起时钟、销毁timer或重入fresh_init。 */
typedef struct fd2_ail_timer_hooks {
    void (*program_pit)(fd2_ail_timer *timer, uint32_t divisor, void *user);
    void (*before_acc_clear)(fd2_ail_timer *timer, unsigned slot, void *user);
    void (*ack_irq)(fd2_ail_timer *timer, void *user);
    void (*bios_chain)(fd2_ail_timer *timer, void *user);
    void *user;
} fd2_ail_timer_hooks;

/* 宿主表示，不承诺原BSS二进制布局。defer_depth保留DWORD位型，避免signed溢出。 */
struct fd2_ail_timer {
    fd2_ail_timer_slot slots[FD2_AIL_TIMER_SLOTS];
    uint32_t base_us;
    uint32_t pit_divisor;
    uint32_t busy;
    uint32_t defer_depth;
    fd2_ail_timer_hooks hooks;
};

/* 只初始化新宿主owner。不是原warm重装／release／vector安装的替代。 */
void fd2_ail_timer_fresh_init(fd2_ail_timer *timer,
                             const fd2_ail_timer_hooks *hooks);
uint32_t fd2_ail_timer_divisor(uint32_t base_us);
void fd2_ail_timer_defer_enter(fd2_ail_timer *timer);
void fd2_ail_timer_defer_leave(fd2_ail_timer *timer);

/* 原handle为4字节偏移0,4,...56；60保留给BIOS。INVALID start/stop无副作用。
 * 错位／越界／NULL callback在宿主H域拒绝，不推导原无效输入的行为。 */
uint32_t fd2_ail_timer_register(fd2_ail_timer *timer,
                               fd2_ail_timer_callback callback);
int fd2_ail_timer_set_user(fd2_ail_timer *timer, uint32_t handle,
                          void *user, void **previous);
int fd2_ail_timer_start(fd2_ail_timer *timer, uint32_t handle);
int fd2_ail_timer_stop(fd2_ail_timer *timer, uint32_t handle);
/* sub_3f2a5/3f2cc：release仅清status，保留pending等；不释放宿主user。
 * all按handle56..0倒序，保留BIOS槽60；不因此重算base或program PIT。 */
int fd2_ail_timer_release(fd2_ail_timer *timer, uint32_t handle);
int fd2_ail_timer_release_all(fd2_ail_timer *timer);
int fd2_ail_timer_set_period(fd2_ail_timer *timer, uint32_t handle,
                            uint32_t period_us);
int fd2_ail_timer_set_frequency(fd2_ail_timer *timer, uint32_t handle,
                               uint32_t hz);

/* 外部调用一次表示一个已到达IRQ。1=普通完成，0=busy只EOI，-1=H输入错误。
 * callback非NULL是已注册槽的正常合同；损坏槽在扣pending处停止，保已达前缀。
 * 回调可按原顺序操作本owner；不承诺任意CPU指令间IRQ／PIC原子性。 */
int fd2_ail_timer_irq(fd2_ail_timer *timer);

#ifdef __cplusplus
}
#endif

#endif
