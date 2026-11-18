#include "clock_consumer_fault_fixture.h"
#define main clock_legacy_main
#include "field_prepare_play_test.c"
#undef main
static int clock_failure_main(void){

 fd2_vga target={0};fd2_field_play_context context={0};prepare_ui ui={0};
 target.input.queued_count=1;target.input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
 ui.vga=&target;ui.context=&context;prepare_wait wait={0};fd2_input_event event={0};
 fd2_field_rng before=context.rng;clock_fault_reset(1);clock_ms=0;clock_reads=0;
 CHECK(read_key_state(&ui,&event,1,0,&wait)<0 && !wait.initialized && target.input.queued_count==1 && !memcmp(&before,&context.rng,sizeof(before)));
 target.input.quit_requested=1;clock_fault_error=-71;CHECK(read_key_state(&ui,&event,1,0,&wait)<0 && target.input.quit_requested && !ui.service.host_quit);
 target.input.quit_requested=0;clock_fault_reset(0);clock_wait_at=1;
 target.frame_deadline_ns=123456789;target.frame_interval_ns=876543;uint64_t local=0;
 CHECK(prepare_host_present(&ui,&local)<0 && target.frame_deadline_ns==123456789 && target.frame_interval_ns==876543 && !ui.service.host_quit);

CHECK(prepare_host_present(&ui,&local)<0 && clock_wait_calls==1 && target.frame_deadline_ns==123456789 && target.frame_interval_ns==876543);
clock_fault_error=-71;state_save_calls=0;fd2_vga saved=target;
 CHECK(prepare_service_execute(&ui,"service.hotel",NULL,0)<0 && !state_save_calls);
 CHECK(fd2_field_prepare_run(&target,NULL,&context,"never-written-by-source-error.sav",NULL)==FD2_FIELD_PREPARE_ERROR && !state_save_calls && !memcmp(&saved,&target,sizeof(saved)));
puts("clock_error_prepare: PASS (new injected failure prefixes only; no GUI/device)");return 0;}

static int output_copy_cases(void){
 fd2_field_play_context context={0};
 for(unsigned kind=0;kind<3;kind++)for(unsigned host=0;host<2;host++){
  fd2_vga target={0};prepare_ui ui={0};ui.vga=&target;ui.context=&context;
  target.input.queued_count=1;target.input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
  target.frame_deadline_ns=123456789;target.frame_interval_ns=876543;
  memset(target.framebuffer,0xa7,sizeof target.framebuffer);fd2_field_rng rng=context.rng;
  clock_fault_reset(0);clock_upload_at=1;clock_upload_host=(int)host;
  choice_timing_probe=1;source_uploads=frame_number=choice_plain=0;quit_frame=0;
  int result=kind==0?present_copy(&ui):kind==1?prepare_service_present(&ui,0):prepare_service_present_ticks(&ui,0);
  CHECK(result<0&&clock_upload_calls==1&&source_uploads==1&&!clock_wait_calls&&!clock_fault_calls&&!clock_fault_error);
  CHECK(!ui.service.host_quit&&target.input.quit_requested==(int)host&&target.input.queued_count==1&&!memcmp(&rng,&context.rng,sizeof rng));
  CHECK(target.frame_deadline_ns==123456789&&target.frame_interval_ns==876543);
  fd2_vga saved=target;
  CHECK(present_copy(&ui)<0&&source_uploads==1&&!memcmp(&saved,&target,sizeof target));
 }
 /* bottom_slide必须返回上传ERROR，不能把负值折叠为HOST或绘下一相位。 */
 fd2_vga target={0};prepare_ui ui={0};ui.vga=&target;ui.context=&context;
 uint8_t background[64000],page[64000];memset(background,0xa7,sizeof background);memset(page,0x55,sizeof page);
 clock_fault_reset(0);clock_upload_at=1;clock_upload_host=1;source_uploads=frame_number=choice_plain=0;
 CHECK(bottom_slide(&ui,background,page,1)<0&&!ui.service.host_quit&&source_uploads==1&&!clock_wait_calls);
 for(unsigned y=0;y<200;y++)for(unsigned x=0;x<320;x++)CHECK(target.framebuffer[y*320u+x]==(y>=177&&x>=5&&x<315?0x55:0xa7));
 choice_timing_probe=0;return 0;
}

static int output_owner_cases(void){
  fd2_vga target={0};fd2_field_play_context context={0};prepare_ui ui={0};
  target.input.queued_count=1;target.input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
  ui.vga=&target;ui.context=&context;prepare_wait wait={0};fd2_input_event event={0};
  fd2_field_rng saved=context.rng;clock_fault_reset(0);clock_output_error=-91;target.input.quit_requested=1;
  CHECK(read_key_state(&ui,&event,1,0,&wait)<0&&!wait.initialized&&!ui.service.host_quit&&target.input.quit_requested);
  CHECK(target.input.queued_count==1&&!memcmp(&saved,&context.rng,sizeof(saved))&&!clock_fault_calls);
  clock_fault_reset(0);target.input.quit_requested=0;clock_ms=0;clock_reads=0;clock_output_at=1;
  CHECK(read_key_state(&ui,&event,1,0,&wait)<0&&!wait.initialized&&clock_fault_calls==1&&!clock_fault_error);
  CHECK(target.input.queued_count==1&&!memcmp(&saved,&context.rng,sizeof(saved)));
  state_save_calls=0;CHECK(fd2_field_prepare_run(&target,NULL,&context,"never-written-output-error.sav",NULL)==FD2_FIELD_PREPARE_ERROR&&!state_save_calls);
  CHECK(output_copy_cases()==0);
  puts("output-owner prepare: ERROR priority/no RNG/key/save suffix PASS");return 0;
}

static int audio_return_cases(void){
 fd2_vga target={0};fd2_field_play_context context={0};prepare_ui ui={0};
 fd2_pcm_driver driver={0};fd2_pcm_player p={0},q={0};fd2_pcm_bank bank={0};fd2_field_audio audio={0};
 driver.audio=(fd2_audio *)&target;driver.dig.attached=1;p.driver=q.driver=&driver;q.handle=1;
 audio.player=&p;audio.secondary_player=&q;audio.ui_bank=&bank;ui.vga=&target;ui.context=&context;ui.audio=&audio;
 target.input.queued_count=1;target.input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
 fd2_field_rng rng=context.rng;uint8_t pixels[64000];memset(target.framebuffer,0xa7,64000);memcpy(pixels,target.framebuffer,64000);
 for(unsigned fault=1;fault<=3;fault++)for(unsigned secondary=0;secondary<2;secondary++){
  clock_fault_reset(0);clock_audio_at=1;clock_audio_fault=fault;
  CHECK((secondary?prepare_service_selection_sound(&ui,7,1):sound_loop(&ui,0,1))<0);
  CHECK(clock_audio_calls==1&&!!clock_fault_error==!!(fault&2u)&&!!clock_output_error==!!(fault&1u));
  CHECK(!memcmp(pixels,target.framebuffer,64000)&&!memcmp(&rng,&context.rng,sizeof(rng))&&target.input.queued_count==1&&!clock_fault_calls);
  target.input.quit_requested=1;CHECK(sound(&ui,0)<0&&clock_audio_calls==1&&!ui.service.host_quit&&target.input.quit_requested);
  target.input.quit_requested=0;
 }
 /* 已到达四个option拷贝保留，但BGM同步0+OS错后不得执行P0/P1 enabled写。 */
 clock_fault_reset(0);clock_audio_at=1;clock_audio_fault=1;option_sync_count=0;
 context.has_preparation=1;context.bgm=(fd2_bgm_player *)&context;
 context.preparation.options[0]=1;context.preparation.options[1]=0;context.preparation.options[2]=0;context.preparation.options[3]=1;
 context.music_enabled=1;context.sfx_enabled=0;driver.dig.enabled=0;
 CHECK(sync_context_options(&ui)<0&&clock_audio_calls==1&&option_sync_count==1&&!driver.dig.enabled);
 CHECK(!context.music_enabled&&context.sfx_enabled&&context.camera_option==1&&!context.effect_option);
 /* pre门拒绝新的option写；普通已关闭/缺音频政策仍返回0。 */
 context.music_enabled=1;CHECK(sync_context_options(&ui)<0&&context.music_enabled&&clock_audio_calls==1);
 clock_fault_reset(0);context.preparation.options[3]=0;CHECK(sound(&ui,0)==0&&!clock_audio_calls);
 context.has_preparation=0;fd2_static_flow_node music_node={.op="music",.args={18,0}};unsigned port=99;
 clock_fault_reset(0);clock_audio_at=1;clock_audio_fault=1;
 CHECK(prepare_service_ui(&ui,&music_node,&port)<0&&port==FD2_FLOW_ERROR&&!ui.service.host_quit&&clock_audio_calls==1);
 CHECK(!memcmp(pixels,target.framebuffer,64000)&&!memcmp(&rng,&context.rng,sizeof(rng))&&target.input.queued_count==1&&!state_save_calls);
 puts("audio-return Prepare: primary/secondary/options/cold-music prefixes PASS");return 0;
}

int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--audio-return"))return audio_return_cases();
 if(argc==2&&!strcmp(argv[1],"--output-owner"))return output_owner_cases();
 return clock_failure_main();
}
