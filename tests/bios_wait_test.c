/* bios_tick_delay @0x17aa9正式VGA等待：独立固定PIT边界与HOST/FIFO写集。
 * 只替换宿主NS/休眠/事件泵；不启动窗口、设备或原版。 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "vga.h"
#include "bios_clock.h"

static uint64_t now_ns;
static unsigned polls,quit_poll;
static unsigned clock_reads,script_at,script_count;
static const uint64_t *clock_script;
static uint64_t fake_ticks(void) {
    clock_reads++;
    if(clock_script) {
        if(script_at>=script_count){fprintf(stderr,"bios_wait unexpected clock read\n");exit(1);}
        now_ns=clock_script[script_at++];
    }
    return now_ns;
}
static void fake_delay(Uint32 ms) {now_ns+=(uint64_t)ms*1000000u;}
static void fake_precise(Uint64 ns) {now_ns+=ns;}
static void fake_poll(fd2_input *input) {(void)input;polls++;}
static int fake_quit(void) {return quit_poll && polls>=quit_poll;}
static int fake_take_quit(fd2_input *input) {return (input && input->quit_requested)||fake_quit();}
#define SDL_GetTicksNS fake_ticks
#define SDL_Delay fake_delay
#define SDL_DelayPrecise fake_precise
#define fd2_input_poll_host_events fake_poll
#define fd2_input_host_quit_requested fake_quit
#define fd2_input_take_quit fake_take_quit
#include "../src/vga.c"

#define CHECK(x) do {if(!(x)){fprintf(stderr,"bios_wait:%d %s\n",__LINE__,#x);exit(1);}} while(0)
static void reset(uint32_t origin,uint64_t now) {
    fd2_bios_clock_reset(origin,0);now_ns=now;polls=quit_poll=clock_reads=script_at=script_count=0;clock_script=NULL;
}
static void no_service(void *unused) {(void)unused;}
int main(void) {
    fd2_input input;fd2_input_init(&input);
    fd2_input_set_test_key_state(&input,SDL_SCANCODE_F2,1);
    fd2_input_begin_test_frame(&input,0);
    fd2_input before=input;
    reset(0,0);fd2_wait_ticks(0);CHECK(now_ns==0 && polls==1 && clock_reads==3);
    /* 独立source三读：start0、比较已到首PIT tick、fresh尾多1ns。 */
    static const uint64_t reads[]={0,UINT64_C(54925402),UINT64_C(54925403)};
    reset(0,0);clock_script=reads;script_count=3;fd2_wait_ticks(1);
    CHECK(script_at==3 && clock_reads==3 && now_ns==UINT64_C(54925403) && polls==1);
    reset(0,1);fd2_wait_ticks(1);CHECK(now_ns==UINT64_C(54925402));
    CHECK(!memcmp(&input,&before,sizeof(input))); /* pump不读/清普通队列。 */
    reset(0,UINT64_C(54925401));fd2_wait_ticks(1);
    CHECK(now_ns==UINT64_C(54925402)); /* 距下一个边界1ns，不是另等55ms。 */
    reset(0,UINT64_C(54925402));fd2_wait_ticks(2);
    CHECK(now_ns==UINT64_C(164776204));
    reset(0,0);fd2_wait_ticks(65535);CHECK(now_ns==UINT64_C(3599536164643));
    reset(0x7fff,0);fd2_wait_ticks(2);CHECK(now_ns==UINT64_C(109850803));
    reset(0xffff,0);fd2_wait_ticks(2);CHECK(now_ns==UINT64_C(109850803));
    reset(0x1800af,0);fd2_wait_ticks(1);CHECK(now_ns==UINT64_C(54925402));
    reset(0,0);quit_poll=1;fd2_wait_ticks(10);CHECK(now_ns==0 && polls==1 && clock_reads==1);
    CHECK(!memcmp(&input,&before,sizeof(input)));
    fd2_vga vga={0};
    reset(10,0);CHECK(fd2_vga_wait_movement_tick(&vga)==0 && !now_ns && vga.movement_tick==10);
    CHECK(fd2_vga_wait_movement_tick(&vga)==0 && now_ns==UINT64_C(54925402) && vga.movement_tick==11);
    now_ns=UINT64_C(164776204); /* renderer已跨两个tick，不另等新鲜1tick。 */
    CHECK(fd2_vga_wait_movement_tick(&vga)==0 && now_ns==UINT64_C(164776204) && vga.movement_tick==13);
    reset(0x7fff,0);vga.movement_tick=0x7fff;
    CHECK(fd2_vga_wait_movement_tick(&vga)==0 && now_ns==UINT64_C(54925402) && vga.movement_tick==-32768);
    reset(0,0);vga.movement_tick=0;quit_poll=1;
    CHECK(fd2_vga_wait_movement_tick(&vga)==1 && !now_ns && !vga.movement_tick);
    CHECK(fd2_vga_wait_movement_tick(NULL)==-1);
    CHECK(!memcmp(&input,&before,sizeof(input)));
    /* 独立源条件：BIOS在第11/22IRQ才链。主业务已跨11IRQ但未泵时，
     * 17aa9入口必须先取当前BDA=1，不能用旧0在首poll后立即结束。 */
    reset(0,0);CHECK(fd2_bios_clock_ail_attach(0,65536u)==0);
    CHECK(fd2_bios_clock_install_mdi(0,no_service,NULL)==0u);
    CHECK(fd2_bios_clock_install_dig(0,no_service,NULL)==FD2_AIL_TIMER_DIG);
    now_ns=UINT64_C(55000830);CHECK(fd2_bios_clock_ticks(now_ns)==0u);
    fd2_wait_ticks(1);CHECK(now_ns==UINT64_C(110001660) && fd2_bios_clock_ticks(now_ns)==2u);
    CHECK(!memcmp(&input,&before,sizeof(input)));
    reset(0,0);
    puts("bios_wait: production wait boundaries, AIL current entry, 65535/WORD/day wrap, HOST and FIFO protection passed; no GUI/original");
    return 0;
}
