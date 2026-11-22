#ifndef FD2_PACING_H
#define FD2_PACING_H
#include "vga.h"
#include "bios_clock.h"
#include "settings.h"

/* 只缩放演出调用者的等待。默认 tick 分支保留原有 BDA 边界和读钟顺序。 */
static inline uint32_t fd2_animation_ms(uint64_t ms) {
    unsigned speed = (unsigned)fd2_get_settings()->animation_speed;
    uint64_t scaled = (ms * 100 + speed / 2) / speed;
    return (uint32_t)SDL_min(scaled, UINT32_MAX);
}
#define fd2_animation_wait_ms(vga, ms) fd2_vga_delay_ms((vga), fd2_animation_ms(ms))
static inline int fd2_animation_wait_ticks(fd2_vga *vga, uint32_t ticks) {
    if (fd2_get_settings()->animation_speed == 100) return fd2_vga_wait_ticks(vga, ticks);
    uint64_t ms = (fd2_bios_tick_start_ns(ticks) + 500000) / 1000000;
    return fd2_vga_delay_ms(vga, fd2_animation_ms(ms));
}
static inline int fd2_text_wait(fd2_vga *vga) {
    int speed = fd2_get_settings()->text_speed;
    if (!speed) return fd2_vga_wait_ticks(vga, 1);
    return fd2_vga_delay_ms(vga, speed == 1 ? 15 : 0);
}
#endif
