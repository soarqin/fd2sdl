/* 游戏BIOS时间与音乐／音效服务：原3ea8e/3ec17/3ebb4的整数节奏。
 * 单App epoch/BDA/timer，供游戏等待、RNG与音频输出共用；非DOS硬件模拟。 */
#include "bios_clock.h"
#include <SDL3/SDL.h>
#include <limits.h>
#include <string.h>

enum { PIT_RATE=FD2_BIOS_PIT_RATE, PIT_DIVISOR=65536, BIOS_DAY_TICKS=0x1800b0 };
static uint32_t clock_origin;
static uint64_t clock_epoch_ns;
static int clock_initialized;
static SDL_ThreadID clock_thread;
static uint64_t clock_last_ns;
static uint32_t clock_bda;
static int clock_ail_active, clock_servicing, clock_error;
static int clock_closed;
static int clock_external_input;
static fd2_ail_timer clock_timer;
static fd2_bios_audio_prefix clock_audio_prefix;
static void *clock_audio_user;
/* ns + fraction/PIT_RATE；周期相加不逐IRQ四舍五入。不是第二时钟。 */
typedef struct pit_point { uint64_t ns; uint32_t fraction; } pit_point;
static pit_point clock_cursor, clock_next_irq;

static uint64_t pit_ceil_ns(pit_point p) {
    return p.fraction && p.ns != UINT64_MAX ? p.ns + 1u : p.ns;
}
static int pit_add(pit_point at, uint32_t cycles, pit_point *out) {
    uint64_t numerator=(uint64_t)cycles*UINT64_C(1000000000)+at.fraction;
    uint64_t whole=numerator/PIT_RATE;
    uint32_t fraction=(uint32_t)(numerator%PIT_RATE);
    if (whole>UINT64_MAX-at.ns ||
        (whole==UINT64_MAX-at.ns && fraction!=0u))return -1;
    out->ns=at.ns+whole;out->fraction=fraction;return 0;
}
static int clock_owner_now(uint64_t now_ns) {
    return clock_initialized && SDL_GetCurrentThreadID()==clock_thread &&
        now_ns>=clock_last_ns && (clock_external_input || now_ns<=SDL_GetTicksNS());
}

uint64_t fd2_bios_ticks_from_ns(uint64_t ns) {
    uint64_t cycles=(ns/UINT64_C(1000000000))*PIT_RATE;
    uint64_t fraction=(cycles%PIT_DIVISOR)*UINT64_C(1000000000)+
        (ns%UINT64_C(1000000000))*PIT_RATE;
    return cycles/PIT_DIVISOR+fraction/(UINT64_C(1000000000)*PIT_DIVISOR);
}
static uint64_t fd2_bios_tick_start_ns(uint64_t tick) {
    if(tick>UINT64_MAX/PIT_DIVISOR)return UINT64_MAX;
    uint64_t cycles=tick*PIT_DIVISOR,seconds=cycles/PIT_RATE;
    uint64_t fraction=((cycles%PIT_RATE)*UINT64_C(1000000000)+PIT_RATE-1u)/PIT_RATE;
    if(seconds>UINT64_MAX/UINT64_C(1000000000))return UINT64_MAX;
    uint64_t whole=seconds*UINT64_C(1000000000);
    return fraction>UINT64_MAX-whole?UINT64_MAX:whole+fraction;
}
void fd2_bios_clock_reset(uint32_t initial_ticks,uint64_t epoch_ns) {
    clock_origin=initial_ticks%BIOS_DAY_TICKS;clock_epoch_ns=epoch_ns;clock_initialized=1;
    clock_thread=SDL_GetCurrentThreadID();clock_last_ns=epoch_ns;
    clock_bda=clock_origin;clock_ail_active=clock_servicing=clock_error=0;
    clock_external_input=1;clock_closed=0;
    clock_audio_prefix=NULL;clock_audio_user=NULL;
    clock_cursor.ns=epoch_ns;clock_cursor.fraction=0u;
}
int fd2_bios_clock_init(void) {
    if(clock_initialized)return clock_closed?-1:0;
    SDL_Time stamp;SDL_DateTime local;
    /* 与参照BIOS一样从RTC整秒初始化；不改变系统时钟/时区。 */
    if(!SDL_GetCurrentTime(&stamp) || !SDL_TimeToDateTime(stamp,&local,true))return -1;
    uint64_t seconds=(uint64_t)local.hour*3600u+(uint64_t)local.minute*60u+(uint64_t)local.second;
    uint32_t origin=(uint32_t)(seconds*PIT_RATE/PIT_DIVISOR);
    fd2_bios_clock_reset(origin,SDL_GetTicksNS());
    clock_external_input=0; /* 正式init只接受已实际观测到的SDL时间。 */
    return 0;
}
uint32_t fd2_bios_clock_ticks(uint64_t now_ns) {
    if(clock_ail_active || clock_closed)return clock_bda;
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
    if(clock_closed)return UINT64_MAX;
    if(clock_ail_active) {
        uint64_t edge=pit_ceil_ns(clock_next_irq);
        return clock_error?UINT64_MAX:edge>now_ns?edge:now_ns;
    }
    uint64_t elapsed=now_ns>=clock_epoch_ns?now_ns-clock_epoch_ns:0;
    uint64_t deadline=fd2_bios_tick_start_ns(fd2_bios_ticks_from_ns(elapsed)+ticks);
    return deadline>UINT64_MAX-clock_epoch_ns?UINT64_MAX:clock_epoch_ns+deadline;
}

int fd2_bios_clock_advance(uint64_t now_ns) {
    if(clock_closed || clock_servicing || !clock_owner_now(now_ns))return -1;
    if(clock_error)return -1;
    clock_servicing=1;
    while(clock_ail_active && pit_ceil_ns(clock_next_irq)<=now_ns) {
        int result,schedule_error;
        if(clock_audio_prefix)clock_audio_prefix(clock_audio_user,clock_next_irq.ns,clock_next_irq.fraction);
        /* 先关闭到当前边界的输出，再执行同一轮音乐／音效服务。 */
        clock_cursor=clock_next_irq;
        schedule_error=pit_add(clock_cursor,clock_timer.interval_count,&clock_next_irq);
        result=fd2_ail_timer_step(&clock_timer);
        /* 游戏tick在本轮音乐／音效回调之后提交，保原等待与RNG读点。 */
        if(result>0)clock_bda=clock_bda+1u==BIOS_DAY_TICKS?0u:clock_bda+1u;
        if(result<0 || schedule_error || clock_error) {
            clock_error=1;break;
        }
    }
    if(!clock_error && clock_audio_prefix)clock_audio_prefix(clock_audio_user,now_ns,0u);
    clock_servicing=0;
    if(clock_error) {
        clock_last_ns=pit_ceil_ns(clock_cursor);
        return -1;
    }
    clock_last_ns=now_ns;clock_cursor.ns=now_ns;clock_cursor.fraction=0u;
    return 0;
}

int fd2_bios_clock_set_audio_prefix(fd2_bios_audio_prefix callback,void *user) {
    if(!clock_initialized || clock_servicing || SDL_GetCurrentThreadID()!=clock_thread ||
       (clock_closed && callback))return -1;
    clock_audio_prefix=callback;clock_audio_user=callback?user:NULL;
    return 0;
}

int fd2_bios_clock_observe_checked(uint64_t now_ns,uint64_t *actual_ns) {
    if(!actual_ns)return -1;
    /* 未初始化的独立数学向量不建立App owner；正式Main已显式init。 */
    if(!clock_initialized){*actual_ns=now_ns;return 0;}
    if(fd2_bios_clock_advance(now_ns)<0)return -1;
    *actual_ns=now_ns;return 0;
}
int fd2_bios_clock_error(void) {return clock_error?-1:0;}

int fd2_bios_clock_close(void) {
    if(!clock_initialized || clock_servicing || SDL_GetCurrentThreadID()!=clock_thread)return -1;
    if(!clock_closed) {
        clock_bda=fd2_bios_clock_ticks(clock_last_ns);
        clock_audio_prefix=NULL;clock_audio_user=NULL;clock_closed=1;
    }
    return 0;
}

int fd2_bios_clock_ail_attach(uint64_t now_ns,uint32_t first_irq_cycles) {
    pit_point first,at;
    uint32_t bda;
    if(clock_closed || clock_ail_active || clock_servicing || !clock_owner_now(now_ns) ||
       first_irq_cycles>65537u)return -1;
    at.ns=now_ns;at.fraction=0u;
    if(pit_add(at,first_irq_cycles,&first)!=0)return -1;
    bda=fd2_bios_clock_ticks(now_ns);
    fd2_ail_timer_fresh_init(&clock_timer);
    clock_cursor=at;clock_next_irq=first;clock_bda=bda;
    clock_last_ns=now_ns;clock_error=0;clock_ail_active=1;
    return 0;
}

fd2_ail_timer *fd2_bios_clock_ail_owner_after_irqs(uint64_t now_ns) {
    if(!clock_ail_active || fd2_bios_clock_advance(now_ns)!=0)return NULL;
    return &clock_timer;
}
const fd2_ail_timer *fd2_bios_clock_ail_state(void) {
    return clock_ail_active?&clock_timer:NULL;
}
static uint32_t install_service(uint64_t now_ns,uint32_t service,
                                fd2_ail_timer_callback callback,void *user) {
    if(!callback)return FD2_AIL_TIMER_INVALID;
    fd2_ail_timer *timer=fd2_bios_clock_ail_owner_after_irqs(now_ns);
    if(!timer)return FD2_AIL_TIMER_INVALID;
    uint32_t old_base=timer->base_us;
    if(fd2_ail_timer_install(timer,service,callback,user)<0)return FD2_AIL_TIMER_INVALID;
    if(timer->base_us!=old_base &&
       pit_add(clock_cursor,timer->interval_count,&clock_next_irq)!=0)clock_error=1;
    return clock_error?FD2_AIL_TIMER_INVALID:service;
}
uint32_t fd2_bios_clock_install_dig(uint64_t now_ns,
                                  fd2_ail_timer_callback callback,void *user) {
    return install_service(now_ns,FD2_AIL_TIMER_DIG,callback,user);
}
uint32_t fd2_bios_clock_install_mdi(uint64_t now_ns,
                                  fd2_ail_timer_callback callback,void *user) {
    return install_service(now_ns,FD2_AIL_TIMER_MDI,callback,user);
}
