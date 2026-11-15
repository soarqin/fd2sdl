#include "field_visual.h"
/* sub_11AA8 @0x11aa8（原登记同名）：按完整原指令的独立调用序检查。
 * 私有Play TU只替换NS/水色/11cac，真实FIFO和共享clock保留；不启动窗口。 */
#include <stdio.h>
#include <string.h>
#include "field_play.h"
#include "field_game.h"
#include "bios_clock.h"

static uint64_t now_ns,after_refresh_ns;
static unsigned reads,waters,refreshes;
static unsigned menu_refreshes,summary_refreshes,advance_read,water_failure_at;
static uint64_t advance_ns;
static int water_failure,refresh_failure,bad;
static int expected_refresh_mode,observe_source_calls,source_fail_role,source_quit_role;
static unsigned source_calls,source_roles[8],source_values[8];
static fd2_field_game *source_game;
static fd2_vga *source_vga;
static void source_observe(unsigned role,unsigned value) {
    if(!observe_source_calls)return;
    if(source_calls>=8){bad=1;return;}
    source_roles[source_calls]=role;source_values[source_calls++]=value;
    if((unsigned)source_fail_role==role)source_game->failure=FD2_FIELD_FAILURE_ACTION;
    if((unsigned)source_quit_role==role)source_vga->input.quit_requested=1;
}
static void wait_test_present(fd2_vga *target) {(void)target;source_observe(2,0);}
static void wait_test_ticks(uint32_t ticks) {source_observe(3,ticks);}
static void wait_test_delay(uint32_t ms) {source_observe(4,ms);}
static uint64_t wait_test_ns(void) {
    uint64_t value=now_ns;reads++;
    if(reads==advance_read)now_ns=advance_ns;
    return value;
}
static int wait_test_water(fd2_vga *vga,uint64_t time) {
    (void)vga;waters++;if(time!=now_ns)bad=1;
    return water_failure || (water_failure_at && waters==water_failure_at)?-1:0;
}
static int wait_test_refresh(fd2_field_game *game,fd2_vga *vga,int phase) {
    (void)game;(void)vga;refreshes++;if(phase!=expected_refresh_mode)bad=1;
    source_observe(1,(unsigned)phase);
    if(refresh_failure)return -1;
    now_ns=after_refresh_ns;return 0;
}
static int wait_test_menu_refresh(fd2_field_game *game,fd2_vga *vga) {
    (void)game;(void)vga;
    if(waters!=1)bad=1;
    menu_refreshes++;
    return refresh_failure?-1:0;
}
static int wait_test_summary_refresh(fd2_field_game *game,fd2_vga *vga,size_t actor,int left) {
    (void)game;(void)vga;summary_refreshes++;
    if(waters!=2 || actor!=5 || left!=1)bad=1;
    if(refresh_failure)return -1;
    now_ns=after_refresh_ns;return 0;
}
#define SDL_GetTicksNS wait_test_ns
#define fd2_field_water_tick wait_test_water
#define fd2_field_water_tick_live(vga_) wait_test_water((vga_),SDL_GetTicksNS())
#define fd2_field_game_runtime_refresh wait_test_refresh
#define fd2_field_game_runtime_menu_refresh wait_test_menu_refresh
#define fd2_field_game_runtime_summary_refresh wait_test_summary_refresh
#define fd2_vga_present wait_test_present
#define fd2_wait_ticks wait_test_ticks
#define fd2_delay_ms wait_test_delay
#include "../field_play.c"
#undef fd2_vga_present
#undef fd2_wait_ticks
#undef fd2_delay_ms
#undef SDL_GetTicksNS
#undef fd2_field_water_tick_live
#undef fd2_field_water_tick
#undef fd2_field_game_runtime_refresh
#undef fd2_field_game_runtime_menu_refresh
#undef fd2_field_game_runtime_summary_refresh

#define CHECK(e) do {if(!(e)){fprintf(stderr,"input_wait:%d %s\n",__LINE__,#e);return 1;}}while(0)
static fd2_field_game game;
static fd2_vga vga;
static void reset(uint32_t origin,uint64_t ns,uint16_t anchor) {
    memset(&game,0,sizeof(game));memset(&vga,0,sizeof(vga));game.ready=1;
    fd2_input_init(&vga.input);vga.browse_input_tick=anchor;
    fd2_bios_clock_reset(origin,0);now_ns=after_refresh_ns=ns;
    reads=waters=refreshes=0;water_failure=refresh_failure=bad=0;
    menu_refreshes=summary_refreshes=advance_read=water_failure_at=0;advance_ns=0;
    expected_refresh_mode=observe_source_calls=source_fail_role=source_quit_role=0;source_calls=0;
}

/* 15055的独立wait不借恢复/回合资源；对话/设备实际延迟另有clock测试。 */
static int test_source_runtime_waits_and_refresh(void) {
    const int tick_values[]={0,1,2,65535};
    for(unsigned fast=0;fast<2;fast++)for(size_t i=0;i<sizeof(tick_values)/sizeof(*tick_values);i++) {
        reset(0,0,0);vga.info_panel_suppressed=1;
        source_game=&game;source_vga=&vga;observe_source_calls=1;
        field_runtime_presentation runtime={.vga=&vga,.fast=(int)fast};
        fd2_field_game before_game=game;fd2_vga before_vga=vga;
        CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_WAIT_TICKS,tick_values[i],&runtime)==0);
        CHECK(!bad && source_calls==(fast?0u:1u));
        if(!fast)CHECK(source_roles[0]==3 && source_values[0]==(unsigned)tick_values[i]);
        CHECK(!memcmp(&game,&before_game,sizeof(game)) && !memcmp(&vga,&before_vga,sizeof(vga)));
        CHECK(!reads && !waters && !refreshes && !runtime.host_quit);
        source_calls=0;
        CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_DELAY_MS,200,&runtime)==0);
        CHECK(!bad && source_calls==(fast?0u:1u));
        if(!fast)CHECK(source_roles[0]==4 && source_values[0]==200);
        CHECK(!memcmp(&game,&before_game,sizeof(game)) && !memcmp(&vga,&before_vga,sizeof(vga)));
    }
    reset(0,0,0);source_game=&game;source_vga=&vga;observe_source_calls=1;
    field_runtime_presentation runtime={.vga=&vga};
    CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_WAIT_TICKS,-1,&runtime)==-1 && !source_calls);
    CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_WAIT_TICKS,65536,&runtime)==-1 && !source_calls);
    CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_DELAY_MS,199,&runtime)==-1 && !source_calls);
    CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_REFRESH,1,&runtime)==-1 && !source_calls);
    expected_refresh_mode=3;
    CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_REFRESH,3,&runtime)==0 && source_calls==2 &&
        source_roles[0]==1 && source_values[0]==3 && source_roles[1]==2 && !bad);
    source_calls=0;
    CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_WAIT_TICKS,1,&runtime)==0 && source_calls==1 && source_roles[0]==3);
    CHECK(play_runtime_step(&game,FD2_FIELD_RUNTIME_WAIT_TICKS,2,&runtime)==0 && source_calls==2 && source_roles[1]==3);
    CHECK(!waters && refreshes==1 && !vga.info_panel_suppressed && !bad);
    for(unsigned mode=0;mode<5;mode++) {
        reset(0,0,0);source_game=&game;source_vga=&vga;observe_source_calls=1;
        runtime=(field_runtime_presentation){.vga=&vga};
        if(mode==0)vga.input.quit_requested=1;
        if(mode==1){vga.input.quit_requested=1;game.failure=FD2_FIELD_FAILURE_ACTION;}
        if(mode==2)game.runtime_host_quit=1;
        if(mode>=3)source_quit_role=3;
        if(mode==4)source_fail_role=3;
        int result=play_runtime_step(&game,FD2_FIELD_RUNTIME_WAIT_TICKS,2,&runtime);
        CHECK(result==((mode==1 || mode==4)?-1:1) && source_calls==(mode>=3?1u:0u));
        CHECK(!waters && !refreshes && !reads);
    }
    return 0;
}
int main(void) {
    reset(0,0,0);
    CHECK(play_browse_wait_step(&game,&vga)==0 && waters==1 && !refreshes && reads==2 && !bad);
    /* 1tick有理数边界，draw期间进入2tick；锚必须保存新读取的2。 */
    reset(0,UINT64_C(54925402),0);after_refresh_ns=UINT64_C(109850803);
    CHECK(play_browse_wait_step(&game,&vga)==0 && waters==1 && refreshes==1 && reads==3 &&
        vga.browse_input_tick==2 && !bad);
    reads=waters=refreshes=0;
    CHECK(play_browse_wait_step(&game,&vga)==0 && waters==1 && !refreshes && reads==2 && vga.browse_input_tick==2);
    /* 原输入优先：即使远跨多个tick，已排键也不先推进任何视觉。 */
    fd2_input_set_test_key_state(&vga.input,SDL_SCANCODE_RIGHT,1);
    fd2_input_begin_test_frame(&vga.input,0);fd2_input before=vga.input;
    now_ns=UINT64_C(219701606);reads=waters=refreshes=0;
    CHECK(play_browse_wait_step(&game,&vga)==0 && !reads && !waters && !refreshes &&
        vga.browse_input_tick==2 && !memcmp(&before,&vga.input,sizeof(before)));
    reset(0x7fff,UINT64_C(54925402),0x7fff);
    CHECK(play_browse_wait_step(&game,&vga)==0 && refreshes==1 && vga.browse_input_tick==0x8000);
    reset(0xffff,UINT64_C(54925402),0xffff);
    CHECK(play_browse_wait_step(&game,&vga)==0 && refreshes==1 && !vga.browse_input_tick);
    reset(0,UINT64_C(54925402),0);water_failure=1;
    CHECK(play_browse_wait_step(&game,&vga)==FD2_SCENE_RESULT_ERROR && reads==1 && !refreshes && !vga.browse_input_tick);
    reset(0,UINT64_C(54925402),0);refresh_failure=1;
    CHECK(play_browse_wait_step(&game,&vga)==FD2_SCENE_RESULT_ERROR && reads==2 && refreshes==1 && !vga.browse_input_tick);
    reset(0,UINT64_C(54925402),0);vga.input.quit_requested=1;
    CHECK(play_browse_wait_step(&game,&vga)==FD2_SCENE_RESULT_HOST_QUIT && !reads && !waters && !refreshes);
    reset(0,UINT64_C(54925402),0);game.failure=FD2_FIELD_FAILURE_ACTION;
    CHECK(play_browse_wait_step(&game,&vga)==FD2_SCENE_RESULT_ERROR && !reads && !waters && !refreshes);
    /* 12dac的局部ESI是索引，不能把入口强制初始化为当前clock。 */
    reset(0,0,0);int32_t local=3;
    CHECK(play_selector_wait_step(&game,&vga,&local)==0 && refreshes==1 && local==0);
    reads=waters=refreshes=0;
    CHECK(play_selector_wait_step(&game,&vga,&local)==0 && !refreshes && reads==2);
    local=3; /* 下一个读键调用重新带入同一caller槽；clock没变也可刷新。 */
    CHECK(play_selector_wait_step(&game,&vga,&local)==0 && refreshes==1 && local==0);
    reset(0,UINT64_C(54925402),0);local=1;
    CHECK(play_selector_wait_step(&game,&vga,&local)==0 && !refreshes);
    local=0;after_refresh_ns=UINT64_C(109850803);
    CHECK(play_selector_wait_step(&game,&vga,&local)==0 && refreshes==1 && local==2);
    reset(0xffff,0,0);local=95;
    CHECK(play_selector_wait_step(&game,&vga,&local)==0 && local==-1 && refreshes==1);
    fd2_input_set_test_key_state(&vga.input,SDL_SCANCODE_SPACE,1);
    fd2_input_begin_test_frame(&vga.input,0);local=7;reads=waters=refreshes=0;
    CHECK(play_selector_wait_step(&game,&vga,&local)==0 && local==7 && !reads && !waters && !refreshes);
    CHECK(selector_input_context(FD2_FIELD_INTERACTION_TARGETING) &&
        selector_input_context(FD2_FIELD_INTERACTION_UNIT_SELECTED) &&
        !selector_input_context(FD2_FIELD_INTERACTION_BROWSE) &&
        !selector_input_context(FD2_FIELD_INTERACTION_COMMAND));
    /* 17898没有外层tick门；空队同tick两次都画，早键不water/highlight/画。 */
    reset(0,0,0);game.interaction=FD2_FIELD_INTERACTION_COMMAND;
    CHECK(play_command_wait_step(&game,&vga)==0 && waters==1 && menu_refreshes==1 &&
        reads==2 && !bad && !vga.menu_highlight_phase);
    waters=0;
    CHECK(play_command_wait_step(&game,&vga)==0 && waters==1 && menu_refreshes==2 && reads==4);
    reset(0,UINT64_C(219701606),0);game.interaction=FD2_FIELD_INTERACTION_COMMAND;
    /* water先读4，highlight比较4，phase写后fresh读5存锚。 */
    advance_read=2;advance_ns=UINT64_C(274627008);
    CHECK(play_command_wait_step(&game,&vga)==0 && reads==3 && waters==1 && menu_refreshes==1 &&
        vga.menu_highlight_phase==1 && vga.menu_highlight_tick==5 &&
        game.command_highlight_phase==1 && !bad);
    fd2_input_set_test_key_state(&vga.input,SDL_SCANCODE_RETURN,1);
    fd2_input_begin_test_frame(&vga.input,0);reads=waters=menu_refreshes=0;
    CHECK(play_command_wait_step(&game,&vga)==0 && !reads && !waters && !menu_refreshes &&
        vga.menu_highlight_tick==5 && vga.menu_highlight_phase==1);
    reset(0,UINT64_C(219701606),0);game.interaction=FD2_FIELD_INTERACTION_ITEM_MENU;water_failure=1;
    CHECK(play_command_wait_step(&game,&vga)==-1 && reads==1 && !menu_refreshes && !vga.menu_highlight_phase);
    reset(0,UINT64_C(219701606),0);game.interaction=FD2_FIELD_INTERACTION_SYSTEM_MENU;refresh_failure=1;
    CHECK(play_command_wait_step(&game,&vga)==-1 && reads==3 && menu_refreshes==1 &&
        vga.menu_highlight_phase==1 && vga.menu_highlight_tick==4 && game.system_menu.highlight_phase==1);
    reset(0,0,0);game.interaction=FD2_FIELD_INTERACTION_SYSTEM_MENU;
    game.system_menu.page=FD2_FIELD_SYSTEM_PAGE_CONFIRMATION;
    CHECK(!command_input_context(&game) && play_command_wait_step(&game,&vga)==-1 && !reads);
    /* 18b84这里只签到达18c65后的续体。first候选先画不等于原首次ESI证明。 */
    reset(0,UINT64_C(54925402),0);after_refresh_ns=UINT64_C(109850803);
    int have_anchor=0,ready=0;int32_t summary_anchor=0x12345678;
    CHECK(play_summary_wait_step(&game,&vga,5,1,&have_anchor,&summary_anchor,&ready)==0 &&
        have_anchor && !ready && summary_anchor==2 && waters==2 && reads==3 && summary_refreshes==1 && !bad);
    reads=waters=summary_refreshes=0;
    CHECK(play_summary_wait_step(&game,&vga,5,1,&have_anchor,&summary_anchor,&ready)==0 &&
        !ready && summary_anchor==2 && waters==1 && reads==2 && !summary_refreshes && !bad);
    reset(0x7fff,UINT64_C(54925402),0);have_anchor=1;summary_anchor=32767;
    CHECK(play_summary_wait_step(&game,&vga,5,1,&have_anchor,&summary_anchor,&ready)==0 &&
        summary_anchor==-32768 && waters==2 && reads==4 && summary_refreshes==1 && !bad);
    reset(0xffff,UINT64_C(54925402),0);have_anchor=1;summary_anchor=-1;
    CHECK(play_summary_wait_step(&game,&vga,5,1,&have_anchor,&summary_anchor,&ready)==0 &&
        summary_anchor==0 && waters==2 && reads==4 && summary_refreshes==1 && !bad);
    reset(0,0,0);have_anchor=1;summary_anchor=0x10000; /* 不得截成低WORD后误判相等。 */
    CHECK(play_summary_wait_step(&game,&vga,5,1,&have_anchor,&summary_anchor,&ready)==0 &&
        !summary_anchor && summary_refreshes==1 && waters==2 && !bad);
    fd2_input_set_test_key_state(&vga.input,SDL_SCANCODE_A,1);fd2_input_begin_test_frame(&vga.input,0);
    before=vga.input;reads=waters=summary_refreshes=0;summary_anchor=17;
    CHECK(play_summary_wait_step(&game,&vga,5,1,&have_anchor,&summary_anchor,&ready)==0 &&
        ready && summary_anchor==17 && !reads && !waters && !summary_refreshes &&
        !memcmp(&before,&vga.input,sizeof(before)));
    for(unsigned failure=0;failure<3;failure++) {
        reset(0,UINT64_C(54925402),0);have_anchor=1;summary_anchor=17;
        water_failure_at=failure==0?1:failure==1?2:0;refresh_failure=failure==2;
        CHECK(play_summary_wait_step(&game,&vga,5,1,&have_anchor,&summary_anchor,&ready)==-1 &&
            have_anchor && summary_anchor==17 && waters==(failure==0?1u:2u) &&
            summary_refreshes==(failure==2?1u:0u) && reads==(failure==0?1u:3u) && !bad);
    }
    reset(0,0,0);have_anchor=0;summary_anchor=17;vga.input.quit_requested=1;
    CHECK(play_summary_wait_step(&game,&vga,5,1,&have_anchor,&summary_anchor,&ready)==FD2_SCENE_RESULT_HOST_QUIT &&
        !have_anchor && summary_anchor==17 && !reads && !waters && !summary_refreshes);
    game.failure=FD2_FIELD_FAILURE_ACTION;
    CHECK(play_summary_wait_step(&game,&vga,5,1,&have_anchor,&summary_anchor,&ready)==-1 &&
        !reads && !waters && !summary_refreshes);
    /* 宿主期限与原视觉调用分离；不为处理结果超时多调1297d。 */
    reset(0,0,0);game.active_side=2;game.visual_owner=&vga.field_visual;
    vga.field_visual.cursor_phase=7;fd2_field_visual_state visual=vga.field_visual;
    game.system_result_fragment=410;game.system_result_until_ms=50;
    fd2_field_game_tick_deadlines(&game,50);
    CHECK(!game.system_result_fragment && !game.system_result_until_ms &&
        !memcmp(&visual,&vga.field_visual,sizeof(visual)));
    CHECK(test_source_runtime_waits_and_refresh()==0);
    puts("field_input_wait: FIFO-first, tick gates/fresh anchors/wrap, summary continuation (first ESI unqualified), independent source waits/refresh, failure/deadlines passed; no GUI");
    return 0;
}
