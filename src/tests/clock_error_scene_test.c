#include "clock_consumer_fault_fixture.h"
/* 同时到达的故障事实只在本夹具注入；不改变生产poll/getter。 */
static int scene_host_quit_requested;
static int scene_priority_coterminal;
static int scene_priority_wait(void){
 int result=clock_fault_wait();
 if(result<0 && scene_priority_coterminal)scene_host_quit_requested=1;
 return result;
}
#define clock_fault_wait scene_priority_wait
#define main clock_legacy_main
#include "scene_handoff_test.c"
#undef main
#undef clock_fault_wait
static unsigned clock_new_rng_calls;
static uint32_t clock_new_rng(void *u){(void)u;clock_new_rng_calls++;return 0;}
static int clock_failure_main(void){

 fd2_vga target={0};int skip=0;clock_fault_reset(0);clock_wait_at=1;
 scene_host_quit_requested=scene_render_error=0;
 CHECK(wait_for_dialog_step(&target,&skip)<0 && !skip && !scene_host_quit_requested && !target.input.quit_requested);
 fd2_ui_sheet ui={0};fd2_dialog_state dialog={0};fd2_scene_field_state field={0};
 field.runtime.rng=clock_new_rng;clock_fault_reset(1);clock_new_rng_calls=0;
 CHECK(dialog_wait(&target,NULL,&ui,&dialog,&field,0,&skip)<0 && !clock_new_rng_calls && !skip);
 target.input.quit_requested=1;clock_fault_error=-71;
 CHECK(dialog_wait(&target,NULL,&ui,&dialog,&field,0,&skip)<0 && !skip && !scene_host_quit_requested && target.input.quit_requested);

/* Source已锁+HOST的入口分类：不得先poll或改色区。 */
 reset_controls(&target);clock_fault_reset(0);scene_host_quit_requested=1;target.input.quit_requested=1;
 palette_tracking=1;clock_fault_error=-71;
 CHECK(scene_test_palette_fade(&target,1,0)==FD2_SCENE_RESULT_ERROR && !palette_attempts && !palette_uploads && !clock_wait_calls);
 CHECK(target.input.quit_requested && scene_host_quit_requested);
 /* walker里已提交首dark64后，失败wait同时报告HOST；末分类仍须ERROR。 */
 reset_controls(&target);clock_fault_reset(0);scene_host_quit_requested=0;
 memset(target.palette,31,sizeof(target.palette));memset(target.dac,124,sizeof(target.dac));
 target.frame_deadline_ns=123456789;target.frame_interval_ns=7654321;
 palette_tracking=1;scene_priority_coterminal=1;clock_wait_at=1;
 CHECK(scene_test_palette_fade(&target,1,0)==FD2_SCENE_RESULT_ERROR && clock_fault_error==-71 && scene_host_quit_requested);
 CHECK(palette_attempts==1 && palette_uploads==1 && !palette_delays && clock_wait_calls==1);
 for(size_t i=0;i<768;i++)CHECK(target.palette[i]==31 && target.dac[i]==0);
 CHECK(target.frame_deadline_ns==123456789 && target.frame_interval_ns==7654321);
 scene_priority_coterminal=0;
 /* 健康HOST仍按原接口返回HOST，不被这个纯getter边界改成ERROR。 */
 reset_controls(&target);clock_fault_reset(0);scene_host_quit_requested=1;
 CHECK(scene_test_palette_fade(&target,1,0)==FD2_SCENE_RESULT_HOST_QUIT && !clock_wait_calls);
 /* 其余直接HOST早返的正式入口；arrival view给同一真实page/合法flow。 */
 reset_controls(&target);clock_fault_reset(0);clock_fault_error=-71;target.input.quit_requested=1;scene_host_quit_requested=0;
 fd2_archive archive={0};fd2_scene_arrival_visual *unopened=NULL;
 CHECK(scene_test_game_over(&target,&archive,NULL,0)==FD2_SCENE_RESULT_ERROR && !scene_host_quit_requested);
 CHECK(scene_test_arrival_prepare(&unopened,&target,&archive,0)==FD2_SCENE_RESULT_ERROR && !unopened);
 fd2_scene_arrival_visual arrival={0};arrival.base=target.field_page;arrival.owner=target.field_page;
 fd2_terrain_tileset terrain={0};fd2_field_map map={0};fd2_map_sprite_bank sprites={0};fd2_scene_field_state state={0};
 CHECK(arrival_flow_ready(fd2_static_flow_lookup("runtime","runtime.arrival")));
 CHECK(arrival_visual_play_view(&arrival,&target,&terrain,&map,&sprites,&state,NULL,0)==FD2_SCENE_RESULT_ERROR && !arrival.started && !scene_host_quit_requested);
 clock_fault_reset(0);CHECK(arrival_visual_play_view(&arrival,&target,&terrain,&map,&sprites,&state,NULL,0)==FD2_SCENE_RESULT_HOST_QUIT && !arrival.started);
 fd2_field_page_destroy(test_page);test_page=NULL;target.field_page=NULL;
 puts("clock_error_scene: PASS (new injected failure prefixes only; no GUI/device)");return 0;}

static unsigned output_upload_count,output_fail_upload,output_input_count,output_cue_count;
static int output_inject_host;
static void output_upload_fail(fd2_vga *vga){
 if(++output_upload_count==output_fail_upload){
  vga->output_error=FD2_VGA_OUTPUT_LOCK;
  if(output_inject_host)vga->input.quit_requested=1;
 }
}
static void output_leaf_observer(fd2_vga *vga,char kind,uint32_t value){
 (void)vga;(void)value;
 if(kind=='I')output_input_count++;
 if(kind=='A')output_cue_count++;
}
/* 提交失败保当前字形/portrait前缀，不能继续发新cue、消费HOST或画下一字。 */
static int output_glyph_failure_cases(void){
 fd2_archive dato={0};CHECK(fd2_archive_open(&dato,"original_game/DATO.DAT")==0);
 const uint8_t bits[32]={0};fd2_font font={bits,sizeof bits,1};fd2_field_audio audio={0};
 for(unsigned stage=0;stage<3;stage++)for(unsigned host=0;host<2;host++){
  fd2_vga target;reset_controls(&target);clock_fault_reset(0);scene_host_quit_requested=0;
  memset(target.framebuffer,0x6d,sizeof target.framebuffer);
  target.input.queued_count=stage==2?0u:1u;target.input.queued_keys[0].key=FD2_INPUT_KEY_OTHER;
  fd2_dialog_state state={0};state.x=100;state.y=120;state.area=FD2_DIALOG_BOTTOM;
  state.has_portrait=stage==2;state.portrait_id=75;
  target.dialog_visual.glyph_counter=stage==2?1u:0u;
  int slow=1,skip=0;
  output_upload_count=output_input_count=output_cue_count=0;output_fail_upload=stage;
  output_inject_host=(int)host;scene_upload_probe=output_upload_fail;dialog_probe_observer=output_leaf_observer;
  if(!stage){target.output_error=FD2_VGA_OUTPUT_LOCK;target.input.quit_requested=(int)host;}
  fd2_vga before=target;
  CHECK(scene_text_glyph(&target,&dato,NULL,&font,&state,0,74,&slow,&skip,&audio)<0);
  CHECK(!clock_fault_error&&!clock_fault_calls&&!clock_wait_calls&&!output_cue_count&&!skip&&slow==1);
  CHECK(!scene_host_quit_requested&&target.input.quit_requested==(int)host);
  CHECK(output_upload_count==stage&&output_input_count==(stage?1u:0u));
  CHECK(target.input.queued_count==before.input.queued_count);
  if(!stage)CHECK(!memcmp(&target,&before,sizeof target)&&state.x==100);
  else {
   CHECK(state.x==116);
   for(unsigned y=120;y<136;y++)for(unsigned x=100;x<116;x++)CHECK(target.framebuffer[y*320u+x]==74);
   CHECK(target.dialog_visual.glyph_counter==(stage==2?0u:before.dialog_visual.glyph_counter));
   CHECK(target.dialog_visual.mouth_phase==(stage==2?1u:before.dialog_visual.mouth_phase));
  }
  fd2_dialog_state saved=state;before=target;
  CHECK(scene_text_glyph(&target,&dato,NULL,&font,&state,0,74,&slow,&skip,&audio)<0);
  CHECK(!memcmp(&target,&before,sizeof target)&&!memcmp(&state,&saved,sizeof state));
  CHECK(output_upload_count==stage&&output_input_count==(stage?1u:0u)&&!output_cue_count);
  scene_upload_probe=NULL;dialog_probe_observer=NULL;
 }
 fd2_archive_close(&dato);fd2_field_page_destroy(test_page);test_page=NULL;
 /* marker/tiles纯上传失败须先于poll和10ms尾；不泵/清普通队列。 */
 fd2_vga target;reset_controls(&target);clock_fault_reset(0);scene_host_quit_requested=0;
 output_upload_count=0;output_fail_upload=1;output_inject_host=1;scene_upload_probe=output_upload_fail;
 target.input.queued_count=1;target.input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
 uint64_t started=test_ms;
 CHECK(dialog_present_delay(&target)<0&&test_ms==started&&!clock_wait_calls&&!scene_host_quit_requested);
 CHECK(target.input.quit_requested&&target.input.queued_count==1&&output_upload_count==1);
 scene_upload_probe=NULL;fd2_field_page_destroy(test_page);test_page=NULL;
 return 0;
}

static int output_cinematic_copy_case(void){
 fd2_vga target;reset_controls(&target);clock_fault_reset(0);scene_host_quit_requested=0;
 fd2_archive other={0};fd2_field_game game={0};fd2_scene_field_state state={0};scene_flow_context c={0};
 c.vga=&target;c.fdother=&other;c.game=c.mutable_game=&game;c.state=&state;c.source_flow=1;
 state.camera_cell_x=71;game.camera_cell_x=23;
 CHECK(fd2_scene_cinematic_open(&c.cinematic,&target,&game,&other)==0);
 output_upload_count=0;output_fail_upload=1;output_inject_host=1;scene_upload_probe=output_upload_fail;
 fd2_static_flow_node node={.op="cin_present"};unsigned port=99;
 CHECK(scene_cinematic_node(&c,&node,&port)<0&&output_upload_count==1&&state.camera_cell_x==71);
 CHECK(!scene_host_quit_requested&&!clock_fault_error&&!clock_fault_calls&&target.input.quit_requested);
 scene_upload_probe=NULL;fd2_scene_cinematic_close(&c.cinematic);fd2_field_page_destroy(test_page);test_page=NULL;
 return 0;
}

static int output_owner_cases(void){
  fd2_vga target={0};int skip=0;fd2_ui_sheet ui={0};fd2_dialog_state dialog={0};fd2_scene_field_state field={0};
  field.runtime.rng=clock_new_rng;clock_new_rng_calls=0;clock_fault_reset(0);clock_output_error=-91;
  scene_host_quit_requested=0;target.input.quit_requested=1;
  CHECK(dialog_wait(&target,NULL,&ui,&dialog,&field,0,&skip)<0&&!clock_new_rng_calls&&!skip&&!scene_host_quit_requested&&target.input.quit_requested&&!clock_fault_calls);
  target.input.quit_requested=0;clock_fault_reset(0);clock_output_at=1;
  CHECK(dialog_wait(&target,NULL,&ui,&dialog,&field,0,&skip)<0&&!clock_new_rng_calls&&!clock_fault_error&&clock_fault_calls==1);
  CHECK(output_glyph_failure_cases()==0);
  CHECK(output_cinematic_copy_case()==0);
  puts("output-owner scene: dialog/glyph/portrait failures preserve prefix, stop cue/FIFO/HOST suffix PASS");return 0;
}

static int audio_return_cases(void){
 fd2_vga target={0};scene_flow_context c={0};c.vga=&target;c.bgm=(fd2_bgm_player *)&target;
 fd2_static_flow_node nodes[3]={{.op="bgm",.args={18}},{.op="brightness",.args={63}},{.op="end"}};
 for(unsigned n=0;n<3;n++){nodes[n].views=FD2_FLOW_PRESENT;for(unsigned p=0;p<FD2_FLOW_PORT_COUNT;p++)nodes[n].edges[p]=-1;}
 nodes[0].edges[FD2_FLOW_NEXT]=1;nodes[1].edges[FD2_FLOW_NEXT]=2;
 fd2_static_flow flow={"test.audio.return",nodes,3,0};
 for(unsigned stop=0;stop<2;stop++){
  memset(&target,0,sizeof target);memset(target.palette,31,768);memset(target.dac,124,768);
  target.input.queued_count=1;target.input.queued_keys[0].key=FD2_INPUT_KEY_ENTER;
  nodes[0].args[0]=stop?-1:18;clock_fault_reset(0);clock_audio_at=1;clock_audio_fault=1;
  scene_host_quit_requested=0;fd2_vga before=target;
  CHECK(scene_flow_run(&c,&flow)<0&&clock_audio_calls==1&&!clock_fault_error&&!scene_host_quit_requested);
  CHECK(!memcmp(&target,&before,sizeof target)&&!clock_fault_calls);
 }
 puts("audio-return Scene: real cold walker stops before next brightness/FIFO PASS");return 0;
}

int main(int argc,char **argv){
 if(argc==2&&!strcmp(argv[1],"--audio-return"))return audio_return_cases();
 if(argc==2&&!strcmp(argv[1],"--output-owner"))return output_owner_cases();
 return clock_failure_main();
}
