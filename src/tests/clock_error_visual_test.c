#include "clock_consumer_fault_fixture.h"
#define main clock_legacy_main
#include "field_visual_live_test.c"
#undef main
static int clock_failure_main(void){

 memset(&visual,0,sizeof(visual));CLOCK(5,6);clock_fault_reset(2);
 CHECK(private_visual_actor_step_live(&live_output_owner,&visual)<0 && visual.actor_idle_phase==1 && !visual.actor_idle_tick && !visual.cursor_phase);
 memset(&visual,0,sizeof(visual));visual.terrain_tick=9;visual.range_phase=19;CLOCK(10,11);clock_fault_reset(2);
 CHECK(private_visual_tiles_step_live(&live_output_owner,&visual,-1)<0 && visual.terrain_phase==1 && visual.terrain_tick==9 && visual.range_phase==19);
 memset(&vga,0,sizeof(vga));CLOCK(2,3);clock_fault_reset(2);
 CHECK(private_water_tick_live(&vga)<0 && vga.field_water_phase==1 && !vga.field_water_last_tick);
 fd2_vga expected={0};CHECK(private_water_palette_apply(&expected,1)==0);
 CHECK(!memcmp(vga.dac,expected.dac,768) && !vga.input.quit_requested);
 vga.input.quit_requested=1;clock_fault_error=-71;unsigned calls=(unsigned)clock_at;
 CHECK(private_water_tick_live(&vga)<0 && vga.input.quit_requested && clock_at==calls);

/* 11f76/120b1已完成work/source运输后fresh读失败，目的页尚未写。 */
 setup_background(BACKGROUND_WAVE);CLOCK(10,11);clock_fault_reset(2);
 CHECK(private_background_draw_from_live(&live_output_owner,&owner,&target,72,72,0,0,0,0,1)<0);
 CHECK(owner.background.last_tick==8 && !owner.background.phase);
 static const unsigned wave[16]={2,3,3,4,4,4,3,3,2,1,1,0,0,0,1,1};
 for(size_t row=0;row<192;row++)for(size_t x=0;x<312;x++)CHECK(background_work[row*320+x]==background_source[row*320+x+wave[row&15u]]);
 for(size_t i=0;i<sizeof(target.pixels);i++)CHECK(target.pixels[i]==0xa5);
 setup_background(BACKGROUND_SCROLL);for(size_t row=0;row<192;row++)memset(background_source+row*312,(int)row,312);
 CLOCK(10,11);clock_fault_reset(2);
 CHECK(private_background_draw_from_live(&live_output_owner,&owner,&target,72,72,0,0,0,0,1)<0 && owner.background.last_tick==8);
 for(size_t row=0;row<192;row++)for(size_t x=0;x<312;x++)CHECK(background_source[row*312+x]==(uint8_t)(row?row-1u:191u));
 for(size_t i=0;i<sizeof(target.pixels);i++)CHECK(target.pixels[i]==0xa5);
 puts("clock_error_visual: PASS (new injected failure prefixes only; no GUI/device)");return 0;}

static int output_owner_cases(void){
  memset(&visual,0,sizeof(visual));CLOCK(5,6);clock_fault_reset(0);clock_output_at=2;
  CHECK(private_visual_actor_step_live(&live_output_owner,&visual)<0);
  CHECK(!clock_fault_error&&clock_output_error==-91&&clock_fault_calls==2);
  CHECK(visual.actor_idle_phase==1&&!visual.actor_idle_tick&&!visual.cursor_phase);
  fd2_field_visual_state saved=visual;clock_fault_reset(0);clock_output_error=-91;
  CHECK(private_visual_range_step_live(&live_output_owner,&visual,7)<0&&!memcmp(&saved,&visual,sizeof(saved))&&!clock_fault_calls);
  setup_background(BACKGROUND_WAVE);CLOCK(10,11);clock_fault_reset(0);clock_output_at=2;
  CHECK(private_background_draw_from_live(&live_output_owner,&owner,&target,72,72,0,0,0,0,1)<0);
  CHECK(!clock_fault_error&&owner.background.last_tick==8&&!owner.background.phase);
  static const unsigned wave[16]={2,3,3,4,4,4,3,3,2,1,1,0,0,0,1,1};
  for(size_t row=0;row<192;row++)for(size_t x=0;x<312;x++)CHECK(background_work[row*320+x]==background_source[row*320+x+wave[row&15u]]);
  for(size_t i=0;i<sizeof(target.pixels);i++)CHECK(target.pixels[i]==0xa5);
  puts("output-owner visual: real live helper prefix/fixed leaf write oracle PASS");return 0;
}
int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--output-owner"))return output_owner_cases();
 return clock_failure_main();
}
