#include "clock_consumer_fault_fixture.h"
#include "bgm.h"
static unsigned output_music_calls;
static int output_bgm_play(fd2_bgm_player *p,size_t t,int loops){(void)p;(void)t;(void)loops;output_music_calls++;clock_output_error=-91;return 0;}
static int output_bgm_stop(fd2_bgm_player *p){(void)p;output_music_calls++;clock_output_error=-91;return 0;}
#define fd2_bgm_play output_bgm_play
#define fd2_bgm_stop output_bgm_stop
#define main clock_legacy_main
#include "field_progress_play_test.c"
#undef main
static unsigned clock_new_rng_calls;
static uint32_t clock_new_rng(void *u){(void)u;clock_new_rng_calls++;return 0;}
static int clock_failure_main(void){

 fd2_vga target={0};fd2_field_game game={0};field_progress_box box={0};
 game.attack_rng=clock_new_rng;
 clock_fault_reset(1);clock_new_rng_calls=0;
 CHECK(progress_wait(&box,&game,&target,NULL,0,0)==FD2_SCENE_RESULT_ERROR && !clock_new_rng_calls);
 target.input.quit_requested=1;clock_fault_error=-71;
 CHECK(progress_wait(&box,&game,&target,NULL,0,0)==FD2_SCENE_RESULT_ERROR && target.input.quit_requested && !clock_new_rng_calls);

puts("clock_error_play: PASS (new injected failure prefixes only; no GUI/device)");return 0;}

static unsigned output_music_rng,output_music_present;
static uint32_t output_music_random(void *u){(void)u;output_music_rng++;return 0;}
static int output_music_submit(fd2_field_game*g,fd2_field_runtime_present_step s,int v,void*u){(void)g;(void)s;(void)v;(void)u;output_music_present++;return 0;}
/* 首次提交失败与portrait第二次提交失败都不能进入下一字/cue/等待。 */
static int output_copy_cases(void){
 const uint8_t bits[32]={0};
 for(unsigned fail=1;fail<=2;fail++)for(unsigned host=0;host<2;host++){
  fd2_vga target={0};fd2_field_game game={0};field_progress_box box={0};
  fd2_pcm_player player={0};fd2_pcm_bank bank={0};fd2_field_audio audio={0};
  bank.archive.count=13;audio.player=&player;audio.ui_bank=&bank;
  game.font=(fd2_font){bits,sizeof bits,1};game.attack_rng=clock_new_rng;
  field_progress_text text={0};text.tokens[0]=text.tokens[1]=0;text.count=2;
  memset(target.framebuffer,0xa7,sizeof target.framebuffer);
  target.dialog_visual.glyph_counter=fail==2?1u:0u;
  target.input.queued_count=fail==1?1u:0u;target.input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
  target.frame_deadline_ns=123456789;target.frame_interval_ns=876543;
  finite_probe=1;finite_background=NULL;finite_plain=0;finite_quit=0;finite_upload_cost=0;
  clock_fault_reset(0);clock_upload_at=fail;clock_upload_host=(int)host;clock_audio_at=99;clock_new_rng_calls=0;
  CHECK(field_text_play(&box,&game,&target,NULL,&audio,&text,100,120,0)==FD2_SCENE_RESULT_ERROR);
  CHECK(clock_upload_calls==fail&&finite_plain==fail&&!clock_audio_calls&&!clock_new_rng_calls&&!clock_wait_calls&&!clock_fault_calls);
  CHECK(!clock_fault_error&&target.output_error==FD2_VGA_OUTPUT_LOCK&&target.input.quit_requested==(int)host);
  CHECK(target.input.queued_count==(fail==1?1u:0u)&&target.frame_deadline_ns==123456789&&target.frame_interval_ns==876543);
  for(unsigned y=120;y<136;y++)for(unsigned x=100;x<116;x++)CHECK(target.framebuffer[y*320u+x]==74);
  for(unsigned y=120;y<136;y++)for(unsigned x=116;x<132;x++)CHECK(target.framebuffer[y*320u+x]==0xa7);
  CHECK(target.dialog_visual.mouth_phase==(fail==2?1u:0u));
  fd2_vga saved=target;
  CHECK(field_text_play(&box,&game,&target,NULL,&audio,&text,100,120,0)==FD2_SCENE_RESULT_ERROR&&!memcmp(&saved,&target,sizeof target));
 }
 /* 零等待/fast caller仍须接住上传负错，不能归为HOST。 */
 fd2_vga target={0};field_runtime_presentation runtime={0};runtime.fast=1;runtime.vga=&target;
 clock_fault_reset(0);clock_upload_at=1;clock_upload_host=1;finite_plain=0;
 CHECK(runtime_wait(&target,0,0,&runtime)<0&&!runtime.host_quit&&target.input.quit_requested&&clock_upload_calls==1&&!clock_wait_calls);
 finite_probe=0;return 0;
}

static int output_owner_cases(void){
 fd2_vga target={0};fd2_field_game game={0};field_progress_box box={0};game.attack_rng=clock_new_rng;
 clock_fault_reset(0);clock_output_error=-91;clock_new_rng_calls=0;target.input.quit_requested=1;
 CHECK(progress_wait(&box,&game,&target,NULL,0,0)==FD2_SCENE_RESULT_ERROR&&!clock_new_rng_calls&&target.input.quit_requested&&!clock_fault_calls);
 target.input.quit_requested=0;clock_fault_reset(0);clock_output_at=1;
 CHECK(progress_wait(&box,&game,&target,NULL,0,0)==FD2_SCENE_RESULT_ERROR&&!clock_new_rng_calls&&!clock_fault_error&&clock_fault_calls==1);
 /* runtime.turncycle side1首先stop；side0先increment再stop。回调负值须阻止后继title/clear/status/RNG。 */
 fd2_field_play_context music_context={0};music_context.initialized=1;music_context.bgm=(fd2_bgm_player *)&target;
 field_music_scope music_scope={&music_context,&target};
 for(unsigned side=0;side<2;side++){
  memset(&game,0,sizeof(game));game.ready=1;game.stage=0;game.active_side=(uint8_t)side;game.turn_number=7;
  game.selected_unit=-1;game.interaction=FD2_FIELD_INTERACTION_BROWSE;game.units.count=1;game.units.items[0].flags=0x80;game.units.items[0].side=2;
  game.phase_unit_cursor=9;game.phase_ai_pass=1;game.ordinary_display_mode=2;
  game.attack_rng=output_music_random;game.runtime_present=output_music_submit;
  clock_fault_reset(0);output_music_calls=output_music_rng=output_music_present=0;
  CHECK(fd2_field_game_set_music_request(&game,field_phase_music,&music_scope)==0);
  CHECK(fd2_field_game_end_active_phase(&game)<0&&game.failure==FD2_FIELD_FAILURE_ACTION);
  CHECK(output_music_calls==1&&!output_music_rng&&!output_music_present&&!clock_fault_error&&clock_output_error==-91);
  CHECK(game.active_side==side&&game.turn_number==(side?7u:8u)&&game.units.items[0].flags==0x80&&game.phase_unit_cursor==9&&game.phase_ai_pass==1&&game.ordinary_display_mode==2);
 }
 CHECK(output_copy_cases()==0);
 puts("output-owner play: no RNG/HOST suffix and real Game callback completion preserves prior increment PASS");return 0;
}

static int audio_return_cases(void){
 fd2_vga target={0};fd2_field_game game={0};fd2_pcm_player player={0};fd2_pcm_bank bank={0};
 fd2_field_audio audio={0};field_runtime_presentation runtime={0};
 bank.archive.count=13;audio.player=audio.secondary_player=&player;audio.ui_bank=&bank;
 runtime.vga=&target;runtime.audio=&audio;game.option_values[1]=1;
 game.units.count=1;game.units.items[0].side=2;game.units.items[0].hp=1;game.units.items[0].hp_max=10;
 target.input.queued_count=1;target.input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
 for(unsigned kind=0;kind<3;kind++){
  clock_fault_reset(0);clock_audio_at=1;clock_audio_fault=1;runtime.host_quit=0;
  runtime.defeat=(fd2_field_defeat_visual *)&game;
  fd2_field_game before=game;fd2_vga saved=target;
  fd2_field_runtime_present_step op=kind==0?FD2_FIELD_RUNTIME_CELL_OPEN_CUE:kind==1?FD2_FIELD_RUNTIME_RECOVERY_CUE:FD2_FIELD_RUNTIME_DEFEAT_CUE;
  CHECK(play_runtime_step(&game,op,kind==0?12:0,&runtime)<0);
  CHECK(clock_audio_calls==1&&clock_output_error==-91&&!clock_fault_error&&!runtime.host_quit);
  CHECK(!memcmp(&game,&before,sizeof game)&&!memcmp(&target,&saved,sizeof target)&&!clock_fault_calls);
  target.input.quit_requested=1;
  CHECK(play_runtime_step(&game,op,kind==0?12:0,&runtime)<0&&clock_audio_calls==1&&!runtime.host_quit&&target.input.quit_requested);
  target.input.quit_requested=0;
 }
 puts("audio-return Play: three formal runtime cues stop before Game suffix/HOST/FIFO PASS");return 0;
}

int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--audio-return"))return audio_return_cases();
 if(argc==2&&!strcmp(argv[1],"--output-owner"))return output_owner_cases();
 return clock_failure_main();
}
