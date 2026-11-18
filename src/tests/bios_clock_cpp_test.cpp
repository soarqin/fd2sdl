/* 真C++ TU链接C11两header/core；不启动SDL设备或GUI。 */
#include "bios_clock.h"
#include "ail_timer.h"
#include <cstdint>
#include <cstdio>
static void callback(void *p) {++*static_cast<unsigned *>(p);}
int main() {
    unsigned calls=0;fd2_ail_timer timer;
    fd2_ail_timer_fresh_init(&timer,nullptr);
    std::uint32_t h=fd2_ail_timer_register(&timer,callback);
    if(h!=0 || fd2_ail_timer_set_user(&timer,h,&calls,nullptr)!=0 ||
       fd2_ail_timer_set_frequency(&timer,h,200)!=0 || fd2_ail_timer_start(&timer,h)!=0 ||
       fd2_ail_timer_irq(&timer)!=1 || calls!=1)return 1;
    fd2_bios_clock_reset(0,0);
    if(fd2_bios_clock_set_audio_prefix(nullptr,nullptr)!=0)return 1;
    if(fd2_bios_ticks_from_ns(54925402)!=1 || fd2_bios_clock_ticks(54925402)!=1 ||
       fd2_bios_clock_ail_state()!=nullptr)return 1;
    if(fd2_bios_clock_ail_attach(0,65536)!=0 || fd2_bios_clock_advance(54925402)!=0 ||
       fd2_bios_clock_ticks(0)!=1)return 1;
    if(fd2_bios_clock_close()!=0 || fd2_bios_clock_advance(54925403)!=-1)return 1;
    std::puts("bios_clock_cpp: actual C++ consumer linked both C11 interfaces");return 0;
}
