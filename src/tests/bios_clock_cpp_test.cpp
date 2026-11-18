/* 真C++ consumer链接普通C11共享调度；不启动设备或GUI。 */
#include "bios_clock.h"
#include <cstdint>
#include <cstdio>
static void callback(void *p) {++*static_cast<unsigned *>(p);}
int main() {
    unsigned music=0,sfx=0;fd2_ail_timer timer;
    fd2_ail_timer_fresh_init(&timer);
    if(fd2_ail_timer_install(&timer,FD2_AIL_TIMER_MDI,callback,&music)!=0 ||
       fd2_ail_timer_install(&timer,FD2_AIL_TIMER_DIG,callback,&sfx)!=0 ||
       fd2_ail_timer_step(&timer)!=0 || music!=0 || sfx!=1)return 1;
    if(fd2_ail_timer_release(&timer,FD2_AIL_TIMER_DIG)!=0 ||
       fd2_ail_timer_step(&timer)!=0 || music!=1 || sfx!=1)return 1;
    fd2_bios_clock_reset(0,0);
    if(fd2_bios_clock_set_audio_prefix(nullptr,nullptr)!=0)return 1;
    if(fd2_bios_ticks_from_ns(54925402)!=1 || fd2_bios_clock_ticks(54925402)!=1 ||
       fd2_bios_clock_ail_state()!=nullptr)return 1;
    if(fd2_bios_clock_ail_attach(0,65536)!=0 || fd2_bios_clock_advance(54925402)!=0 ||
       fd2_bios_clock_ticks(0)!=1)return 1;
    if(fd2_bios_clock_close()!=0 || fd2_bios_clock_advance(54925403)!=-1)return 1;
    std::puts("bios_clock_cpp: C++ consumer linked ordinary shared C11 interfaces");return 0;
}
