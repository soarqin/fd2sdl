/* 普通游戏时钟/输出失败的可控接口注入；不建立硬件或第二时钟。 */
#ifndef FD2_CLOCK_CONSUMER_FAULT_FIXTURE_H
#define FD2_CLOCK_CONSUMER_FAULT_FIXTURE_H
#include "bios_clock.h"
#include "vga.h"
static unsigned clock_fault_calls,clock_fault_at,clock_wait_calls,clock_wait_at;
static int clock_fault_error,clock_deadline_unknown,clock_output_error;
static unsigned clock_output_at,clock_output_wait_at;
static unsigned clock_audio_at,clock_audio_calls,clock_audio_fault;
static unsigned clock_upload_calls,clock_upload_at;
static int clock_upload_host;
static void clock_fault_upload(fd2_vga *vga){
 if(++clock_upload_calls==clock_upload_at){
  vga->output_error=FD2_VGA_OUTPUT_LOCK;
  if(clock_upload_host)vga->input.quit_requested=1;
 }
}
static int clock_audio_return(int result) {
 if(++clock_audio_calls==clock_audio_at){
  if(clock_audio_fault&1u)clock_output_error=-91;
  if(clock_audio_fault&2u)clock_fault_error=-71;
 }
 return result;
}
static void clock_fault_reset(unsigned at) {
 clock_fault_calls=clock_wait_calls=0;clock_fault_at=at;clock_wait_at=0;
 clock_fault_error=clock_deadline_unknown=clock_output_error=0;clock_output_at=clock_output_wait_at=0;
 clock_audio_at=clock_audio_calls=clock_audio_fault=0;
 clock_upload_calls=clock_upload_at=0;clock_upload_host=0;
}
static int clock_fault_checked(uint64_t now,uint64_t *actual) {
 if(clock_fault_error)return clock_fault_error;
 if(++clock_fault_calls==clock_fault_at){clock_fault_error=-71;return -71;}
 return fd2_bios_clock_observe_checked(now,actual);
}
static int clock_fault_get_error(void){return clock_fault_error?clock_fault_error:fd2_bios_clock_error();}
static uint64_t clock_fault_deadline(uint64_t now,uint32_t ticks){
 return clock_deadline_unknown?UINT64_MAX:fd2_bios_clock_deadline_ns(now,ticks);
}
static int clock_fault_wait(void){
 if(clock_fault_error)return clock_fault_error;
 if(++clock_wait_calls==clock_wait_at){clock_fault_error=-71;return -71;}
 if(clock_output_wait_at && clock_wait_calls==clock_output_wait_at){clock_output_error=-91;return -91;}
 return 0;
}

#ifndef FD2_CLOCK_FAULT_ACTUAL_VGA
/* 只注入已公开checked/纯getter的返回合同；真正VGA共享实现由新增直接组另核。 */
static int clock_fault_vga_error(const fd2_vga *vga){return vga&&vga->output_error<0?vga->output_error:clock_output_error;}
static int clock_fault_vga_checked(const fd2_vga *vga,uint64_t now,uint64_t *actual){
 if(!vga || clock_fault_get_error()<0 || clock_fault_vga_error(vga)<0)return -1;
 uint64_t completed;
 int status=clock_fault_checked(now,&completed);if(status<0)return status;
 if(clock_output_at && clock_fault_calls==clock_output_at)clock_output_error=-91;
 if(clock_fault_vga_error(vga)<0)return -1;
 *actual=completed;return 0;
}
#define fd2_vga_observe_checked clock_fault_vga_checked
#define fd2_vga_output_error clock_fault_vga_error
#endif
#define FD2_CLOCK_FAILURE_TEST 1
#define fd2_bios_clock_observe_checked clock_fault_checked
#define fd2_bios_clock_error clock_fault_get_error
#define fd2_bios_clock_deadline_ns clock_fault_deadline
#endif
