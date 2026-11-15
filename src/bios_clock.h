/* BIOS 046Ch时间：原17aa9/1297d/11eee及固定DOSBox-X IBM PIT接口。 */
#ifndef FD2_BIOS_CLOCK_H
#define FD2_BIOS_CLOCK_H
#include <stdint.h>

/* 1193182Hz / 65536；转换不写游戏相位，也不补追视觉步骤。 */
uint64_t fd2_bios_ticks_from_ns(uint64_t elapsed_ns);
uint64_t fd2_bios_ticks_from_ms(uint64_t elapsed_ms);
uint64_t fd2_bios_tick_start_ns(uint64_t tick);

/* 唯一App硬件clock；普通START/Load/换页不重新初始化。reset供确定性时间轴。 */
int fd2_bios_clock_init(void);
void fd2_bios_clock_reset(uint32_t initial_ticks, uint64_t epoch_ns);
uint32_t fd2_bios_clock_ticks(uint64_t now_ns);
uint16_t fd2_bios_tick_low_ns(uint64_t now_ns);
int32_t fd2_bios_tick_signed_ns(uint64_t now_ns);
uint64_t fd2_bios_clock_deadline_ns(uint64_t now_ns, uint32_t ticks);
#endif
