/* 游戏BIOS低WORD时间投影与音乐／音效共同调度；一个App epoch/owner。 */
#ifndef FD2_BIOS_CLOCK_H
#define FD2_BIOS_CLOCK_H
#include <stdint.h>
#include "ail_timer.h"
enum { FD2_BIOS_PIT_RATE = 1193182 };
#ifdef __cplusplus
extern "C" {
#endif
uint64_t fd2_bios_ticks_from_ns(uint64_t elapsed_ns);
/* 给定游戏 tick 的首个整数纳秒，超出范围返回 UINT64_MAX。 */
uint64_t fd2_bios_tick_start_ns(uint64_t tick);
/* 正式init只建立一次epoch；START/Load/换曲不重置。
 * reset只供新离线scope/确定性测试显式输入，不另建wall clock。 */
int fd2_bios_clock_init(void);
void fd2_bios_clock_reset(uint32_t initial_ticks,uint64_t epoch_ns);
/* 纯读：AIL启用时读已提交BDA；未启用时按同epoch作整数投影。 */
uint32_t fd2_bios_clock_ticks(uint64_t now_ns);
uint16_t fd2_bios_tick_low_ns(uint64_t now_ns);
int32_t fd2_bios_tick_signed_ns(uint64_t now_ns);
/* AIL启用时给下一调度唤醒提示，醒后重新比较BDA；MAX不能作为无限等待。 */
uint64_t fd2_bios_clock_deadline_ns(uint64_t now_ns,uint32_t ticks);
/* 仅owner线程，时间不倒退，回调不递归；live不能超出已观察的SDL时间。
 * 离线scope允许显式虚拟now。失败保已达回调/输出前缀，不伪装HOST。 */
int fd2_bios_clock_advance(uint64_t observed_now_ns);
/* checked观察失败不写actual_ns。 */
int fd2_bios_clock_observe_checked(uint64_t observed_now_ns,uint64_t *actual_ns);
/* 纯读唯一clock的调度首错；设备输出错误仍归Audio/VGA各自owner。 */
int fd2_bios_clock_error(void);
/* Audio在每个调度边界前与最终now关闭输出前缀，不驱动逻辑clock。
 * callback不递归advance、不改BDA/RNG；输出失败不停止游戏时间。
 * 精确点为edge_ns + pit_fraction/FD2_BIOS_PIT_RATE纳秒。 */
typedef void (*fd2_bios_audio_prefix)(void *user,uint64_t edge_ns,uint32_t pit_fraction);
int fd2_bios_clock_set_audio_prefix(fd2_bios_audio_prefix,void *user);
/* App终态先禁止推进/安装并解除输出借用，再释放driver userdata。
 * 保已到达BDA；不用于Load/START或设备退化。 */
int fd2_bios_clock_close(void);
/* 一次attach保epoch/BDA；首周期是现App兼容政策，正常Main传65536。 */
int fd2_bios_clock_ail_attach(uint64_t observed_now_ns,uint32_t first_irq_cycles);
/* 控制前关闭<=now的调度/输出前缀，借同一个两服务timer。 */
fd2_ail_timer *fd2_bios_clock_ail_owner_after_irqs(uint64_t observed_now_ns);
const fd2_ail_timer *fd2_bios_clock_ail_state(void);
/* 音乐120Hz／音效200Hz；返回不透明服务ID，失败返INVALID。 */
uint32_t fd2_bios_clock_install_dig(uint64_t,fd2_ail_timer_callback,void *user);
uint32_t fd2_bios_clock_install_mdi(uint64_t,fd2_ail_timer_callback,void *user);
#ifdef __cplusplus
}
#endif
#endif
