#define FD2_CLOCK_FAULT_ACTUAL_VGA 1
#include "clock_consumer_fault_fixture.h"
static unsigned output_fault,locks,unlocks,renders,submits;
static uint32_t output_pixels[VGA_W*VGA_H];
static bool test_lock(SDL_Texture*t,const SDL_Rect*r,void**pixels,int*pitch){
 (void)t;(void)r;locks++;if(output_fault&1u)return false;
 *pixels=output_pixels;*pitch=output_fault&8u?1:VGA_W*(int)sizeof(uint32_t);return true;
}
static void test_unlock(SDL_Texture*t){(void)t;unlocks++;}
static bool test_render(SDL_Renderer*r,SDL_Texture*t,const SDL_FRect*a,const SDL_FRect*b){
 (void)r;(void)t;(void)a;(void)b;renders++;return !(output_fault&2u);
}
static bool test_submit(SDL_Renderer*r){(void)r;submits++;return !(output_fault&4u);}
static void test_begin_frame(fd2_input *input);
#define SDL_LockTexture test_lock
#define SDL_UnlockTexture test_unlock
#define SDL_RenderTexture test_render
#define SDL_RenderPresent test_submit
#define fd2_input_begin_frame test_begin_frame
#define main clock_legacy_main
#include "bios_wait_test.c"
#undef main
static void test_begin_frame(fd2_input *input){(void)input;polls++;}
static void output_cases(void){
 const unsigned faults[]={0,1,8,2,4};
 const int errors[]={0,FD2_VGA_OUTPUT_LOCK,FD2_VGA_OUTPUT_PITCH,
                     FD2_VGA_OUTPUT_RENDER,FD2_VGA_OUTPUT_PRESENT};
 for(unsigned n=0;n<5;n++){
  fd2_vga v={0};v.renderer=(SDL_Renderer*)(uintptr_t)1;v.texture=(SDL_Texture*)(uintptr_t)2;
  v.input.queued_count=1;v.input.queued_keys[0].key=FD2_INPUT_KEY_F2;
  memset(v.framebuffer,7,sizeof v.framebuffer);v.dac[21]=3;v.dac[22]=5;v.dac[23]=9;
  reset(0,0);clock_fault_reset(0);output_fault=faults[n];locks=unlocks=renders=submits=0;
  CHECK(fd2_vga_present(&v)==(errors[n]?-1:0));
  CHECK(fd2_vga_output_error(&v)==errors[n]&&!clock_fault_error&&!fd2_bios_clock_error());
  CHECK(clock_fault_calls==1&&polls==1&&clock_reads==1&&v.input.queued_count==1);
  for(size_t i=0;i<sizeof v.framebuffer;i++)CHECK(v.framebuffer[i]==7);
  CHECK(locks==1&&unlocks==(faults[n]==1?0u:1u));
  CHECK(renders==(faults[n]==1||faults[n]==8?0u:1u));
  CHECK(submits==(faults[n]==1||faults[n]==8||faults[n]==2?0u:1u));
  if(faults[n]!=1&&faults[n]!=8)
   for(size_t i=0;i<VGA_W*VGA_H;i++)CHECK(output_pixels[i]==UINT32_C(0xff030509));
  if(errors[n]){
   unsigned old_reads=clock_reads,old_polls=polls;
   CHECK(fd2_vga_present(&v)<0&&locks==1&&clock_reads==old_reads&&polls==old_polls);
   v.input.quit_requested=1;
   CHECK(fd2_vga_wait_movement_tick(&v)<0&&v.input.quit_requested&&polls==old_polls);
  }
 }
}
int main(void){

 fd2_vga target={0};target.input.queued_count=1;target.input.queued_keys[0].key=FD2_INPUT_KEY_F2;target.frame_deadline_ns=99000000;target.frame_interval_ns=16000000;
 reset(0,0);clock_fault_reset(1);CHECK(fd2_vga_present(&target)<0);
 CHECK(target.frame_deadline_ns==99000000 && target.frame_interval_ns==16000000 && polls==1);
 reset(0,0);clock_fault_reset(1);CHECK(fd2_delay_ms(0)<0 && !polls);
 reset(0,0);clock_fault_reset(1);CHECK(fd2_wait_ticks(1)<0 && !polls);
 reset(0,0);clock_fault_reset(0);clock_deadline_unknown=1;
 CHECK(fd2_wait_ticks(1)<0 && clock_fault_calls==2 && polls==1 && !now_ns && !clock_fault_error && target.input.queued_count==1);
 reset(0,0);clock_fault_reset(0);clock_deadline_unknown=1;target.movement_tick=0;
 CHECK(fd2_vga_wait_movement_tick(&target)<0 && !target.movement_tick && polls==1 && !now_ns && !clock_fault_error && target.input.queued_count==1);
 reset(0,0);clock_fault_reset(1);target.input.quit_requested=1;
 /* 时钟首错优先于HOST；普通MAX提示不制造时钟错误。 */
 clock_fault_error=-71;CHECK(fd2_vga_wait_movement_tick(&target)<0 && target.input.quit_requested && !polls);

output_cases();
puts("clock_error_vga: failure prefixes and real VGA SDL-call errors PASS (SDL substitutes; no device/window)");return 0;}
