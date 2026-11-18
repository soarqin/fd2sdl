/* 同步Audio成功/OS失败的正式Scene bridge边界；仅Audio返回/纯getter替身。 */
#include "clock_consumer_fault_fixture.h"
#include "field_audio.h"
#include <stdio.h>
#include <string.h>
static fd2_vga *audio_target;
static int audio_host;
static int bridge_play(fd2_field_audio *a,fd2_field_sfx c){(void)a;(void)c;int r=clock_audio_return(0);if(audio_host)audio_target->input.quit_requested=1;return r;}
static int bridge_borrow(fd2_field_audio *a,const fd2_pcm_bank*b,int s,int n){(void)a;(void)b;(void)s;(void)n;int r=clock_audio_return(0);if(audio_host)audio_target->input.quit_requested=1;return r;}
#define fd2_field_audio_play bridge_play
#define fd2_field_audio_play_borrowed_bank bridge_borrow
#define fd2_scene_cinematic_open postfix_fd2_scene_cinematic_open
#define fd2_scene_cinematic_close postfix_fd2_scene_cinematic_close
#define fd2_scene_cinematic_montage_open postfix_fd2_scene_cinematic_montage_open
#define fd2_scene_cinematic_montage_close postfix_fd2_scene_cinematic_montage_close
#define fd2_scene_cinematic_montage_intro postfix_fd2_scene_cinematic_montage_intro
#define fd2_scene_cinematic_montage_intro_finish postfix_fd2_scene_cinematic_montage_intro_finish
#define fd2_scene_cinematic_montage_half postfix_fd2_scene_cinematic_montage_half
#define fd2_scene_cinematic_montage_current_half postfix_fd2_scene_cinematic_montage_current_half
#define fd2_scene_cinematic_montage_part_count postfix_fd2_scene_cinematic_montage_part_count
#define fd2_scene_cinematic_montage_part_info postfix_fd2_scene_cinematic_montage_part_info
#define fd2_scene_cinematic_montage_rng postfix_fd2_scene_cinematic_montage_rng
#define fd2_scene_cinematic_montage_hit postfix_fd2_scene_cinematic_montage_hit
#define fd2_scene_cinematic_montage_cue postfix_fd2_scene_cinematic_montage_cue
#define fd2_scene_cinematic_montage_frame postfix_fd2_scene_cinematic_montage_frame
#define fd2_scene_cinematic_page_alloc postfix_fd2_scene_cinematic_page_alloc
#define fd2_scene_cinematic_page_release postfix_fd2_scene_cinematic_page_release
#define fd2_scene_cinematic_page_clear postfix_fd2_scene_cinematic_page_clear
#define fd2_scene_cinematic_work_clear postfix_fd2_scene_cinematic_work_clear
#define fd2_scene_cinematic_copy postfix_fd2_scene_cinematic_copy
#define fd2_scene_cinematic_pixels postfix_fd2_scene_cinematic_pixels
#define fd2_scene_cinematic_sheet_open postfix_fd2_scene_cinematic_sheet_open
#define fd2_scene_cinematic_sheet_close postfix_fd2_scene_cinematic_sheet_close
#define fd2_scene_cinematic_sheet_info postfix_fd2_scene_cinematic_sheet_info
#define fd2_scene_cinematic_sheet_duration postfix_fd2_scene_cinematic_sheet_duration
#define fd2_scene_cinematic_sheet_blit postfix_fd2_scene_cinematic_sheet_blit
#define fd2_scene_cinematic_image_blit postfix_fd2_scene_cinematic_image_blit
#define fd2_scene_cinematic_palette_save postfix_fd2_scene_cinematic_palette_save
#define fd2_scene_cinematic_palette_validate postfix_fd2_scene_cinematic_palette_validate
#define fd2_scene_cinematic_palette_load postfix_fd2_scene_cinematic_palette_load
#define fd2_scene_cinematic_palette_restore postfix_fd2_scene_cinematic_palette_restore
#define fd2_scene_cinematic_audio_open postfix_fd2_scene_cinematic_audio_open
#define fd2_scene_cinematic_audio_play postfix_fd2_scene_cinematic_audio_play
#define fd2_scene_cinematic_audio_close postfix_fd2_scene_cinematic_audio_close
#define fd2_scene_cinematic_circle_prepare postfix_fd2_scene_cinematic_circle_prepare
#define fd2_scene_cinematic_circle_frame postfix_fd2_scene_cinematic_circle_frame
#define fd2_scene_cinematic_circle_close postfix_fd2_scene_cinematic_circle_close
#define fd2_scene_cinematic_shake_prepare postfix_fd2_scene_cinematic_shake_prepare
#define fd2_scene_cinematic_shake_frame postfix_fd2_scene_cinematic_shake_frame
#define fd2_scene_cinematic_credits_assets_open postfix_fd2_scene_cinematic_credits_assets_open
#define fd2_scene_cinematic_credits_assets_close postfix_fd2_scene_cinematic_credits_assets_close
#define fd2_scene_cinematic_credits_actor_close postfix_fd2_scene_cinematic_credits_actor_close
#define fd2_scene_cinematic_credits_actor postfix_fd2_scene_cinematic_credits_actor
#define fd2_scene_cinematic_credits_intro postfix_fd2_scene_cinematic_credits_intro
#define fd2_scene_cinematic_credits_intro_finish postfix_fd2_scene_cinematic_credits_intro_finish
#define fd2_scene_cinematic_credits_idle_reset postfix_fd2_scene_cinematic_credits_idle_reset
#define fd2_scene_cinematic_credits_idle postfix_fd2_scene_cinematic_credits_idle
#define fd2_scene_cinematic_credits_part postfix_fd2_scene_cinematic_credits_part
#define fd2_scene_cinematic_credits_part_info postfix_fd2_scene_cinematic_credits_part_info
#define fd2_scene_cinematic_credits_part_count postfix_fd2_scene_cinematic_credits_part_count
#define fd2_scene_cinematic_credits_card_prepare postfix_fd2_scene_cinematic_credits_card_prepare
#define fd2_scene_cinematic_credits_blink_reset postfix_fd2_scene_cinematic_credits_blink_reset
#define fd2_scene_cinematic_credits_card postfix_fd2_scene_cinematic_credits_card
#define fd2_scene_cinematic_credits_roll_text postfix_fd2_scene_cinematic_credits_roll_text
#define fd2_scene_cinematic_credits_skip_set postfix_fd2_scene_cinematic_credits_skip_set
#define fd2_scene_cinematic_credits_skip_get postfix_fd2_scene_cinematic_credits_skip_get
#define fd2_scene_teleport_validate postfix_fd2_scene_teleport_validate
#define fd2_scene_teleport_begin postfix_fd2_scene_teleport_begin
#define fd2_scene_teleport_sprite postfix_fd2_scene_teleport_sprite
#define fd2_scene_teleport_icon postfix_fd2_scene_teleport_icon
#define fd2_scene_teleport_sound postfix_fd2_scene_teleport_sound
#define fd2_scene_teleport_contract postfix_fd2_scene_teleport_contract
#define fd2_scene_teleport_commit postfix_fd2_scene_teleport_commit
#define fd2_scene_teleport_wipe postfix_fd2_scene_teleport_wipe
#define fd2_scene_teleport_top postfix_fd2_scene_teleport_top
#define fd2_scene_teleport_expand postfix_fd2_scene_teleport_expand
#define fd2_scene_teleport_release postfix_fd2_scene_teleport_release
#define fd2_scene_transition_bank80 postfix_fd2_scene_transition_bank80
#define fd2_scene_transition_begin postfix_fd2_scene_transition_begin
#define fd2_scene_transition_play_sfx postfix_fd2_scene_transition_play_sfx
#define fd2_scene_transition_frame postfix_fd2_scene_transition_frame
#define fd2_scene_transition_release postfix_fd2_scene_transition_release
#include "../scene_cinematic.c"
#include "../scene_teleport.c"
#include "../scene_transition.c"

#define CHECK(e) do{if(!(e)){fprintf(stderr,"%d: %s\n",__LINE__,#e);return 1;}}while(0)
int main(void){
 fd2_vga target={0};audio_target=&target;fd2_field_game game={0};game.ready=1;
 fd2_field_page *page=(fd2_field_page *)&game;target.field_page=page;
 fd2_scene_transition_scope transition={.game=&game,.vga=&target,.page=page,.base=page};
 fd2_scene_teleport_scope teleport={.game=&game,.vga=&target,.owner=page,.base=page};
 fd2_field_audio audio={0};fd2_pcm_player player={0};audio.player=&player;
 for(unsigned which=0;which<4;which++)for(unsigned fault=1;fault<=3;fault++){
  clock_fault_reset(0);clock_audio_at=1;clock_audio_fault=fault;audio_host=1;target.input.quit_requested=0;
  target.input.queued_count=1;target.input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
  fd2_field_game before=game;uint8_t pixels[64000];memcpy(pixels,target.framebuffer,64000);
  fd2_scene_cinematic *cin=calloc(1,sizeof(*cin));CHECK(cin);cin->vga=&target;cin->audio_ready=1;
  uint8_t bytes[11]={'L','L','L','L','L','L',10,0,0,0,128};
  CHECK(fd2_pcm_bank_open_mem(&cin->audio_bank,bytes,sizeof bytes)==0);
  int result=which==0?fd2_scene_transition_play_sfx(&transition,&audio):which==1?fd2_scene_teleport_sound(&teleport,&audio):
      which==2?fd2_scene_cinematic_audio_play(cin,&audio,1):fd2_scene_cinematic_audio_close(cin,&audio);
  CHECK(result<0&&clock_audio_calls==1&&!!clock_fault_error==!!(fault&2u)&&!!clock_output_error==!!(fault&1u));
  CHECK(target.input.quit_requested&&target.input.queued_count==1&&cin->audio_ready&&cin->audio_bank.archive.count==1);
  CHECK(!memcmp(&game,&before,sizeof game)&&!memcmp(pixels,target.framebuffer,64000)&&!clock_fault_calls);
  CHECK(fd2_scene_transition_play_sfx(&transition,&audio)<0&&fd2_scene_teleport_sound(&teleport,&audio)<0&&clock_audio_calls==1);
  CHECK(fd2_scene_cinematic_audio_play(cin,&audio,1)<0&&fd2_scene_cinematic_audio_close(cin,&audio)<0&&clock_audio_calls==1);
  fd2_scene_cinematic_close(&cin);CHECK(!cin&&clock_audio_calls==1);
 }
 puts("audio-return Scene bridges: transition/teleport/cinematic play-stop, ERROR-before-HOST/free-only PASS");return 0;
}
