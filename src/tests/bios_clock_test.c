/* 独立PIT有理数oracle；header常数来自固定DOSBox-X source，非C输出反生成。 */
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>
static int fail_time,fail_local;
static bool test_current_time(SDL_Time *stamp) { *stamp=0;return !fail_time; }
static bool test_local_time(SDL_Time stamp,SDL_DateTime *local,bool is_local) {
    (void)stamp; if(!is_local)abort();
    *local=(SDL_DateTime){0};local->hour=12;local->minute=34;local->second=56;return !fail_local;
}
static uint64_t test_ticks_ns(void) {return 1234;}
#define SDL_GetCurrentTime test_current_time
#define SDL_TimeToDateTime test_local_time
#define SDL_GetTicksNS test_ticks_ns
#include "../bios_clock.c"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"bios_clock_test:%d %s\n",__LINE__,#x);exit(1);}} while(0)

int main(void) {
    /* Python Fraction: ceil(n*65536*10^9/1193182)，与实现分拆算法独立。 */
    static const struct {uint64_t tick,ns;} edges[]={
        {1,54925402},{2,109850803},{3,164776204},{18,988657221},
        {32767,1799740619621},{32768,1799795545022},
        {65535,3599536164643},{65536,3599591090044},{1573040,86399853031642}};
    CHECK(fd2_bios_ticks_from_ns(0)==0);
    for(size_t i=0;i<sizeof(edges)/sizeof(edges[0]);i++) {
        CHECK(fd2_bios_tick_start_ns(edges[i].tick)==edges[i].ns);
        CHECK(fd2_bios_ticks_from_ns(edges[i].ns-1)==edges[i].tick-1);
        CHECK(fd2_bios_ticks_from_ns(edges[i].ns)==edges[i].tick);
    }
    CHECK(fd2_bios_ticks_from_ns(UINT64_MAX)==UINT64_C(335850875661));
    CHECK(fd2_bios_tick_start_ns(UINT64_MAX)==UINT64_MAX);
    fail_time=1;CHECK(fd2_bios_clock_init()==-1 && !clock_initialized);
    fail_time=0;fail_local=1;CHECK(fd2_bios_clock_init()==-1 && !clock_initialized);
    fail_local=0;CHECK(fd2_bios_clock_init()==0 && clock_initialized);
    uint32_t origin=clock_origin;
    CHECK(fd2_bios_clock_init()==0 && clock_origin==origin && clock_epoch_ns==1234);
    fd2_bios_clock_reset(0,0);
    CHECK(fd2_bios_tick_signed_ns(edges[4].ns)==32767);
    CHECK(fd2_bios_tick_signed_ns(edges[5].ns)==-32768);
    CHECK(fd2_bios_tick_low_ns(edges[6].ns)==65535 && fd2_bios_tick_low_ns(edges[7].ns)==0);
    CHECK(fd2_bios_clock_deadline_ns(54925401,1)==54925402);
    CHECK(fd2_bios_clock_deadline_ns(54925402,1)==109850803);
    CHECK(fd2_bios_clock_deadline_ns(123,0)==123);
    fd2_bios_clock_reset(BIOS_DAY_TICKS-1,1000);
    CHECK(fd2_bios_clock_ticks(999)==BIOS_DAY_TICKS-1);
    CHECK(fd2_bios_clock_ticks(1000+edges[0].ns)==0);
    CHECK(fd2_bios_clock_deadline_ns(1000,2)==1000+edges[1].ns);
    fd2_bios_clock_reset(0,0);
    puts("bios_clock_test: exact PIT fractions, boundaries, day/WORD wrap, startup failure and deadlines passed");
    return 0;
}
