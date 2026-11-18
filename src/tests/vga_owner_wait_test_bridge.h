/* 仅旧Battle/effect调用序夹具的依赖ABI适配；不代真实VGA/SDL实现资格。
 * 真观察/等待及SDL输出失败由clock_error_vga直接检查。 */
#ifndef FD2_VGA_OWNER_WAIT_TEST_BRIDGE_H
#define FD2_VGA_OWNER_WAIT_TEST_BRIDGE_H
#include "bios_clock.h"
#ifdef FD2_CLOCK_FAILURE_TEST
#undef fd2_vga_output_error
#undef fd2_vga_observe_checked
#endif
int fd2_vga_output_error(const fd2_vga *vga) {
#ifdef FD2_CLOCK_FAILURE_TEST
    return clock_fault_vga_error(vga);
#else
    return vga?vga->output_error:0; /* 没有真实终端时默认字段为0。 */
#endif
}
int fd2_vga_observe_checked(const fd2_vga *vga,uint64_t now,uint64_t *actual) {
#ifdef FD2_CLOCK_FAILURE_TEST
    return clock_fault_vga_checked(vga,now,actual);
#else
    if(!vga)return -1;
    return fd2_bios_clock_observe_checked(now,actual);
#endif
}
int fd2_vga_delay_ms(fd2_vga *vga,uint32_t ms){(void)vga;return fd2_delay_ms(ms);}
int fd2_vga_wait_ticks(fd2_vga *vga,uint32_t tick_count){(void)vga;return fd2_wait_ticks(tick_count);}
#ifdef FD2_CLOCK_FAILURE_TEST
#define fd2_vga_output_error clock_fault_vga_error
#define fd2_vga_observe_checked clock_fault_vga_checked
#endif
#endif
