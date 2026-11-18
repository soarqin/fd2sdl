#include "clock_consumer_fault_fixture.h"
#define main clock_legacy_main
#include "battle_animation_test.c"
#undef main
static int clock_failure_main(void){

 fd2_battle_animation ctx={0};ctx.vga=&screen;memset(&screen,0,sizeof(screen));frames=0;
 clock_fault_reset(0);clock_wait_at=1;delays=0;
 CHECK(flash(&ctx,1,32,3)<0 && frames==1 && delays==0 && screen.dac[0]==4 && screen.dac[1]==130 && screen.dac[2]==12 && !ctx.host_quit && !ctx.skip);
 memset(&screen,0,sizeof(screen));screen.input.quit_requested=1;frames=0;clock_fault_error=-71;
 CHECK(present(&ctx,0)<0 && !frames && screen.input.quit_requested && !ctx.host_quit);

/* 等待失败时释放exchange，但不进入正常尾restore/stop8/归还旧DAC。 */
 memset(&ctx,0,sizeof(ctx));ctx.vga=&screen;memset(&screen,0xa7,sizeof(screen));memset(&screen.input,0,sizeof(screen.input));
 fd2_vga before=screen;ctx.exchange=calloc(1,sizeof(*ctx.exchange));CHECK(ctx.exchange);
 ctx.exchange->mode=FD2_FIELD_PHYSICAL_MODE_FULL;ctx.exchange->started=1;ctx.exchange->attack[0].clip.split_mode=1;
 frames=0;clock_fault_reset(0);clock_wait_at=1;
 CHECK(restore(&ctx,NULL,1)<0 && !ctx.exchange && !frames && !memcmp(&screen,&before,sizeof(screen)));
 ctx.exchange=calloc(1,sizeof(*ctx.exchange));CHECK(ctx.exchange);ctx.magic_exchange=calloc(1,sizeof(*ctx.magic_exchange));CHECK(ctx.magic_exchange);
 clock_fault_error=-71;before=screen;fd2_battle_animation_close(&ctx);
 CHECK(!ctx.exchange && !ctx.magic_exchange && !memcmp(&screen,&before,sizeof(screen)));
 puts("clock_error_battle: PASS (new injected failure prefixes only; no GUI/device)");return 0;}

static int output_copy_cases(void){
 for(unsigned wait=0;wait<2;wait++)for(unsigned host=0;host<2;host++){
  fd2_battle_animation ctx={0};ctx.vga=&screen;memset(&screen,0,sizeof screen);
  screen.input.queued_count=1;screen.input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
  screen.frame_deadline_ns=123456789;screen.frame_interval_ns=876543;
  clock_fault_reset(0);clock_upload_at=1;clock_upload_host=(int)host;frames=input_pumps=0;
  CHECK(present(&ctx,wait)<0&&frames==1&&input_pumps==1&&!clock_wait_calls&&!clock_fault_calls&&!clock_fault_error);
  CHECK(!ctx.host_quit&&!ctx.skip&&screen.input.quit_requested==(int)host&&screen.input.queued_count==1);
  CHECK(screen.frame_deadline_ns==123456789&&screen.frame_interval_ns==876543);
  fd2_vga saved=screen;
  CHECK(present(&ctx,wait)<0&&frames==1&&input_pumps==1&&!memcmp(&saved,&screen,sizeof screen));
 }
 return 0;
}

static int output_owner_cases(void){
 fd2_battle_animation ctx={0};ctx.vga=&screen;memset(&screen,0,sizeof(screen));frames=0;
 clock_fault_reset(0);clock_output_wait_at=1;delays=0;
 CHECK(flash(&ctx,1,32,3)<0&&frames==1&&delays==0&&!clock_fault_error&&clock_output_error==-91);
 CHECK(screen.dac[0]==4&&screen.dac[1]==130&&screen.dac[2]==12&&!ctx.host_quit&&!ctx.skip);
 frames=0;screen.input.quit_requested=1;
 CHECK(present(&ctx,0)<0&&!frames&&screen.input.quit_requested&&!ctx.host_quit);
 /* 仅release已建立的exchange；不提交正常restore、stop8、旧DAC回写。 */
 memset(&ctx,0,sizeof(ctx));ctx.vga=&screen;memset(&screen,0xa7,sizeof(screen));memset(&screen.input,0,sizeof(screen.input));
 fd2_vga before=screen;ctx.exchange=calloc(1,sizeof(*ctx.exchange));CHECK(ctx.exchange);
 ctx.exchange->mode=FD2_FIELD_PHYSICAL_MODE_FULL;ctx.exchange->started=1;ctx.exchange->attack[0].clip.split_mode=1;
 frames=0;clock_fault_reset(0);clock_output_wait_at=1;
 CHECK(restore(&ctx,NULL,1)<0&&!ctx.exchange&&!frames&&!memcmp(&screen,&before,sizeof(screen))&&!clock_fault_error);
 CHECK(output_copy_cases()==0);
 puts("output-owner battle: real flash/restore return prefixes, output-only ERROR/free-only cleanup PASS");return 0;
}

static int audio_return_cases(void){
 fd2_battle_animation ctx={0};ctx.vga=&screen;fd2_pcm_player p={0};fd2_pcm_bank bank={0};ctx.primary=ctx.secondary=&p;
 for(unsigned stop=0;stop<2;stop++)for(unsigned fault=1;fault<=3;fault++){
  memset(&screen,0,sizeof screen);screen.input.queued_count=1;screen.input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
  clock_fault_reset(0);clock_audio_at=1;clock_audio_fault=fault;frames=input_pumps=0;ctx.host_quit=0;
  fd2_vga before=screen;
  CHECK((stop?battle_audio_stop(&ctx,&p):play_cue(&ctx,&bank,1,0))<0);
  CHECK(clock_audio_calls==1&&!!clock_fault_error==!!(fault&2u)&&!!clock_output_error==!!(fault&1u));
  screen.input.quit_requested=1;CHECK(play_cue(&ctx,&bank,1,0)<0&&clock_audio_calls==1&&!ctx.host_quit);
  screen.input.quit_requested=0;CHECK(!memcmp(&before,&screen,sizeof screen)&&!frames&&!input_pumps);
 }
 /* 同一FX发射器不得在音效0+OS错后继续下一PART写work。 */
 fd2_battle_magic_exchange exchange={0};ctx.magic_exchange=&exchange;exchange.sfx_enabled=1;
 uint8_t data=0;uint32_t offsets[1]={0};exchange.sound.archive=(fd2_archive){.data=&data,.size=1,.offsets=offsets,.count=1};
 magic_fx_reader reader={0};reader.ctx=&ctx;
 fd2_magic_fx_emission sample={.kind=FD2_MAGIC_FX_SECONDARY_SAMPLE,.index=0};
 clock_fault_reset(0);clock_audio_at=1;clock_audio_fault=1;
 CHECK(magic_fx_emit(&sample,&reader)<0&&clock_audio_calls==1&&!clock_fault_error);
 sample.kind=FD2_MAGIC_FX_PART;CHECK(magic_fx_emit(&sample,&reader)<0&&clock_audio_calls==1);
 puts("audio-return Battle: shared play/stop/FX error prefixes PASS");return 0;
}

int main(int argc,char **argv){if(argc==2&&!strcmp(argv[1],"--audio-return"))return audio_return_cases();if(argc==2&&!strcmp(argv[1],"--output-owner"))return output_owner_cases();return clock_failure_main();}
