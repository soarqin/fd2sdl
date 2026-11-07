/* sfx_play @0x25a96（原FUN_000231de）/sfx_play_secondary @0x25b45
 * （原FUN_0002328d）：51E62仅gate未来cue，owner退休不受gate影响。离线纯测。 */
#include "audio_pcm.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
static int observed_play(fd2_audio*,fd2_audio_bus,fd2_audio_source,float);
#define fd2_audio_play_source observed_play
#include "../audio_pcm.c"
#undef fd2_audio_play_source
#define CHECK(x) do{if(!(x)){fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#x);return 1;}}while(0)
typedef struct{fd2_audio_source source;unsigned retire_count;} Observation;
static Observation observations[48];static size_t observation_count;
static size_t observed_render(void *p,float *out,size_t n){Observation *o=p;return o->source.render(o->source.userdata,out,n);}
static void observed_retire(void *p,fd2_audio_retire_reason why){Observation *o=p;++o->retire_count;if(o->source.retire)o->source.retire(o->source.userdata,why);}
static int observed_play(fd2_audio *a,fd2_audio_bus bus,fd2_audio_source source,float gain){
 if(observation_count==48)return -1;Observation *o=&observations[observation_count++];o->source=source;o->retire_count=0;
 fd2_audio_source wrapped={observed_render,observed_retire,o};return fd2_audio_play_source(a,bus,wrapped,gain);
}
static int option(fd2_pcm_player *p,int enabled){
 return fd2_pcm_player_set_enabled(p,enabled);
}
static void make_bank(uint8_t *data){memcpy(data,"LLLLLL",6);data[6]=10;data[7]=data[8]=data[9]=0;memset(data+10,192,8);}
static int old_sample_survives_off_owner_still_retires(void){
 uint8_t bytes[18];make_bank(bytes);fd2_pcm_bank bank;CHECK(fd2_pcm_bank_open_mem(&bank,bytes,sizeof(bytes))==0);
 fd2_audio_config config={11025,0,1};fd2_audio *a=fd2_audio_create(&config);fd2_pcm_player p;CHECK(a&&fd2_pcm_player_init(&p,a,11025)==0);
 CHECK(fd2_pcm_play_replace(&p,&bank,0,0,1.f)==0);float output[4];CHECK(fd2_audio_render_offline(a,output,1)==1);
 CHECK(option(&p,0)==0);CHECK(fd2_audio_render_offline(a,output,1)==1);float ongoing=output[0];
 CHECK(fd2_pcm_play_replace(&p,&bank,0,1,1.f)==0);CHECK(fd2_pcm_stop(&p)==0);
 CHECK(fd2_audio_render_offline(a,output,1)==1);float after_cue=output[0];size_t count=observation_count;
 fd2_audio_destroy(a);unsigned retired=observations[0].retire_count;int free_state=atomic_load(&p.voices[0].state);
 fd2_pcm_player_close(&p);fd2_pcm_bank_close(&bank);memset(bytes,0xcc,sizeof(bytes));
 CHECK(fabsf(ongoing-.5f)<.0001f && fabsf(after_cue-.5f)<.0001f);
 CHECK(count==1 && retired==1 && free_state==0);
 /* 原bank内存已无效后，重开全新owner不得再访问或二次退休旧source。 */
 make_bank(bytes);CHECK(fd2_pcm_bank_open_mem(&bank,bytes,sizeof(bytes))==0);a=fd2_audio_create(&config);CHECK(a&&fd2_pcm_player_init(&p,a,11025)==0);
 CHECK(fd2_pcm_play_replace(&p,&bank,0,1,1.f)==0 && fd2_audio_render_offline(a,output,1)==1);
 fd2_audio_destroy(a);fd2_pcm_player_close(&p);fd2_pcm_bank_close(&bank);
 CHECK(observation_count==2 && observations[0].retire_count==1 && observations[1].retire_count==1);return 0;
}
static int two_handles_and_bad_option(void){
 uint8_t bytes[18];make_bank(bytes);fd2_pcm_bank bank;CHECK(fd2_pcm_bank_open_mem(&bank,bytes,sizeof(bytes))==0);
 fd2_audio_config config={11025,0,1};fd2_audio *a=fd2_audio_create(&config);fd2_pcm_player p,q;CHECK(a&&fd2_pcm_player_init(&p,a,11025)==0&&fd2_pcm_player_init(&q,a,11025)==0);
 CHECK(fd2_pcm_play_replace(&p,&bank,0,0,.5f)==0&&fd2_pcm_play_replace(&q,&bank,0,0,.5f)==0);float out[2];CHECK(fd2_audio_render_offline(a,out,1)==1);
 size_t count=observation_count;CHECK(option(&p,0)==0&&option(&q,0)==0);
 CHECK(fd2_pcm_play(&p,NULL,99,-1,1.f)==0&&fd2_pcm_play_replace(&q,NULL,99,-1,1.f)==0);
 CHECK(fd2_pcm_stop(&p)==0&&fd2_pcm_stop(&q)==0&&observation_count==count);
 CHECK(fd2_audio_render_offline(a,out,1)==1&&fabsf(out[0]-.5f)<.0001f);
 CHECK(fd2_pcm_player_set_enabled(&p,2)==-1&&p.enabled==0);
 CHECK(option(&p,1)==0&&option(&q,1)==0&&fd2_pcm_stop(&p)==0&&fd2_pcm_stop(&q)==0);
 CHECK(fd2_audio_render_offline(a,out,1)==1&&out[0]==0.f);fd2_audio_destroy(a);fd2_pcm_player_close(&p);fd2_pcm_player_close(&q);fd2_pcm_bank_close(&bank);return 0;
}
int main(void){if(old_sample_survives_off_owner_still_retires())return 1;
 if(two_handles_and_bad_option())return 1;
 puts("audio_options_test: ok (no device/GUI)");return 0;}
