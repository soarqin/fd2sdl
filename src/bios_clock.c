/* BIOS 046Ch：DOSBox-X b6abbd5 timer.h/PIT0及bios.cpp RTC/IRQ0。
 * 只投影clock输入；AIL/PIC改变IRQ调度的相位资格由F5另核，不伪称墙钟等值。 */
#include "bios_clock.h"
#include <SDL3/SDL.h>
#include <limits.h>

enum { PIT_RATE=1193182, PIT_DIVISOR=65536, BIOS_DAY_TICKS=0x1800b0 };
static uint32_t clock_origin;
static uint64_t clock_epoch_ns;
static int clock_initialized;

uint64_t fd2_bios_ticks_from_ns(uint64_t ns) {
    uint64_t cycles=(ns/UINT64_C(1000000000))*PIT_RATE;
    uint64_t fraction=(cycles%PIT_DIVISOR)*UINT64_C(1000000000)+
        (ns%UINT64_C(1000000000))*PIT_RATE;
    return cycles/PIT_DIVISOR+fraction/(UINT64_C(1000000000)*PIT_DIVISOR);
}
uint64_t fd2_bios_ticks_from_ms(uint64_t ms) {
    const uint64_t denominator=UINT64_C(1000)*PIT_DIVISOR;
    return (ms/denominator)*PIT_RATE+(ms%denominator)*PIT_RATE/denominator;
}
uint64_t fd2_bios_tick_start_ns(uint64_t tick) {
    if(tick>UINT64_MAX/PIT_DIVISOR)return UINT64_MAX;
    uint64_t cycles=tick*PIT_DIVISOR,seconds=cycles/PIT_RATE;
    uint64_t fraction=((cycles%PIT_RATE)*UINT64_C(1000000000)+PIT_RATE-1u)/PIT_RATE;
    if(seconds>UINT64_MAX/UINT64_C(1000000000))return UINT64_MAX;
    uint64_t whole=seconds*UINT64_C(1000000000);
    return fraction>UINT64_MAX-whole?UINT64_MAX:whole+fraction;
}
void fd2_bios_clock_reset(uint32_t initial_ticks,uint64_t epoch_ns) {
    clock_origin=initial_ticks%BIOS_DAY_TICKS;clock_epoch_ns=epoch_ns;clock_initialized=1;
}
int fd2_bios_clock_init(void) {
    if(clock_initialized)return 0;
    SDL_Time stamp;SDL_DateTime local;
    /* 与参照BIOS一样从RTC整秒初始化；不改变系统时钟/时区。 */
    if(!SDL_GetCurrentTime(&stamp) || !SDL_TimeToDateTime(stamp,&local,true))return -1;
    uint64_t seconds=(uint64_t)local.hour*3600u+(uint64_t)local.minute*60u+(uint64_t)local.second;
    uint32_t origin=(uint32_t)(seconds*PIT_RATE/PIT_DIVISOR);
    fd2_bios_clock_reset(origin,SDL_GetTicksNS());
    return 0;
}
uint32_t fd2_bios_clock_ticks(uint64_t now_ns) {
    uint64_t elapsed=now_ns>=clock_epoch_ns?now_ns-clock_epoch_ns:0;
    return (uint32_t)((clock_origin+fd2_bios_ticks_from_ns(elapsed))%BIOS_DAY_TICKS);
}
uint16_t fd2_bios_tick_low_ns(uint64_t now_ns) {
    return (uint16_t)fd2_bios_clock_ticks(now_ns);
}
int32_t fd2_bios_tick_signed_ns(uint64_t now_ns) {
    uint16_t low=fd2_bios_tick_low_ns(now_ns);
    return low<0x8000u?(int32_t)low:(int32_t)low-65536;
}
uint64_t fd2_bios_clock_deadline_ns(uint64_t now_ns,uint32_t ticks) {
    if(!ticks)return now_ns;
    uint64_t elapsed=now_ns>=clock_epoch_ns?now_ns-clock_epoch_ns:0;
    uint64_t deadline=fd2_bios_tick_start_ns(fd2_bios_ticks_from_ns(elapsed)+ticks);
    return deadline>UINT64_MAX-clock_epoch_ns?UINT64_MAX:clock_epoch_ns+deadline;
}
