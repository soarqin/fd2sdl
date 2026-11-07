/* 纯边界：合成callback样本/文件协议/并发stop；不打开音频设备或GUI。 */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "audio.h"
#include "audio_pcm_capture.h"
#include <SDL3/SDL.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
#define CHECK(x) do{if(!(x)){fprintf(stderr,"check failed %s:%d: %s (%s)\n",__FILE__,__LINE__,#x,SDL_GetError());return 1;}}while(0)
static char base[2048];static unsigned next_case;
static unsigned pid(void){
#ifdef _WIN32
return GetCurrentProcessId();
#else
return (unsigned)getpid();
#endif
}
static void path(char *out,const char *dir,const char *name){snprintf(out,2200,"%s/%s",dir,name);}
static int file_exists(const char *p){SDL_PathInfo x;return SDL_GetPathInfo(p,&x);}
static int new_case(char *out){snprintf(out,2048,"%s/case-%02u",base,next_case++);return SDL_CreateDirectory(out)?0:-1;}
static int write_text(const char *p,const char *text){SDL_IOStream *f=SDL_IOFromFile(p,"wb");if(!f)return -1;int ok=SDL_WriteIO(f,text,strlen(text))==strlen(text);if(!SDL_CloseIO(f))ok=0;return ok?0:-1;}
static int start(const char *dir,unsigned owner){char p[2200],s[64];path(p,dir,"start");snprintf(s,sizeof(s),"%u\n",owner);return write_text(p,s);}
static int stop(const char *dir){char p[2200];path(p,dir,"stop");return write_text(p,"stop\n");}
static int wait_file(const char *dir,const char *name,unsigned ms){char p[2200];path(p,dir,name);uint64_t end=SDL_GetTicks()+ms;while(!file_exists(p)){if(SDL_GetTicks()>=end)return 0;SDL_Delay(2);}return 1;}
static int contains(const char *dir,const char *name,const char *part){char p[2200],s[4096]={0};path(p,dir,name);FILE *f=fopen(p,"rb");if(!f)return 0;fread(s,1,sizeof(s)-1,f);fclose(f);return strstr(s,part)!=NULL;}
static uint32_t u32(const unsigned char *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void samples(float *a){for(unsigned i=0;i<512;i++){a[i*2]=.25f;a[i*2+1]=-.25f;}}
static int ready(fd2_audio_pcm_capture *c,const char *dir){float a[1024];samples(a);uint64_t end=SDL_GetTicks()+1000;char p[2200];path(p,dir,"ready.json");while(!file_exists(p)){fd2_audio_pcm_capture_push(c,a,512);SDL_Delay(2);if(SDL_GetTicks()>=end)return 0;}return 1;}
static int inspect_wave(const char *dir){char p[2200];path(p,dir,"app-mix.wav");FILE *f=fopen(p,"rb");CHECK(f);unsigned char h[44];CHECK(fread(h,1,44,f)==44);CHECK(!memcmp(h,"RIFF",4)&&!memcmp(h+8,"WAVEfmt ",8));CHECK(u32(h+24)==48000&&h[20]==1&&h[22]==2&&h[34]==16);unsigned bytes=u32(h+40);CHECK(bytes&&bytes%4==0);unsigned char sample[4];for(unsigned i=0;i<bytes/4;i++){CHECK(fread(sample,1,4,f)==4);CHECK(sample[0]==0&&sample[1]==32&&sample[2]==0&&sample[3]==224);}CHECK(fgetc(f)==EOF);fclose(f);return 0;}
static int normal(void){char dir[2048];CHECK(!new_case(dir));fd2_audio_pcm_capture *c=fd2_audio_pcm_capture_open(dir,48000,2000,0);CHECK(c);float a[1024],saved[1024];samples(a);memcpy(saved,a,sizeof(a));fd2_audio_pcm_capture_push(c,a,512);CHECK(!start(dir,pid()));CHECK(ready(c,dir));fd2_audio_pcm_capture_push(c,a,512);CHECK(!memcmp(a,saved,sizeof(a)));CHECK(!stop(dir));CHECK(wait_file(dir,"summary.json",1000));CHECK(contains(dir,"summary.json","\"ok\":true"));CHECK(contains(dir,"summary.json","\"device_output_validated\":false"));CHECK(contains(dir,"summary.json","\"A8_validated\":false"));CHECK(!inspect_wave(dir));fd2_audio_pcm_capture_close(c);return 0;}
static int reject_config(void){char dir[2048],bad[2200];CHECK(!new_case(dir));CHECK(!fd2_audio_pcm_capture_open(dir,44100,100,0));CHECK(!fd2_audio_pcm_capture_open(dir,48000,0,0));CHECK(!fd2_audio_pcm_capture_open(dir,48000,120001,0));path(bad,dir,"original_game");CHECK(SDL_CreateDirectory(bad));CHECK(!fd2_audio_pcm_capture_open(bad,48000,100,0));path(bad,dir,"app-mix.wav");CHECK(!write_text(bad,"KEEP"));CHECK(!fd2_audio_pcm_capture_open(dir,48000,100,0));return 0;}
static int wrong_owner(void){char dir[2048];CHECK(!new_case(dir));fd2_audio_pcm_capture *c=fd2_audio_pcm_capture_open(dir,48000,1000,0);CHECK(c);CHECK(!start(dir,pid()+1));CHECK(wait_file(dir,"summary.json",1000));CHECK(contains(dir,"summary.json","wrong_start_pid"));CHECK(!contains(dir,"summary.json","\"ok\":true"));fd2_audio_pcm_capture_close(c);return 0;}
static int no_callback(void){char dir[2048];CHECK(!new_case(dir));fd2_audio_pcm_capture *c=fd2_audio_pcm_capture_open(dir,48000,1000,0);CHECK(c);CHECK(!start(dir,pid()));CHECK(wait_file(dir,"summary.json",3800));CHECK(contains(dir,"summary.json","no_real_callback"));CHECK(contains(dir,"summary.json","\"frames\":0"));fd2_audio_pcm_capture_close(c);return 0;}
static int bad_chunk(int nonfinite){char dir[2048],p[2200];CHECK(!new_case(dir));fd2_audio_pcm_capture *c=fd2_audio_pcm_capture_open(dir,48000,nonfinite?1000:1,0);CHECK(c);CHECK(!start(dir,pid()));float a[1024];samples(a);if(nonfinite)a[3]=NAN;path(p,dir,"summary.json");uint64_t end=SDL_GetTicks()+1000;while(!file_exists(p)){fd2_audio_pcm_capture_push(c,a,nonfinite?512:49);SDL_Delay(2);CHECK(SDL_GetTicks()<end);}CHECK(contains(dir,"summary.json",nonfinite?"nonfinite_mix":"buffer_capacity"));CHECK(!contains(dir,"summary.json","\"ok\":true"));fd2_audio_pcm_capture_close(c);return 0;}
static int early_stop(void){char dir[2048];CHECK(!new_case(dir));fd2_audio_pcm_capture *c=fd2_audio_pcm_capture_open(dir,48000,1000,0);CHECK(c);CHECK(!stop(dir));CHECK(wait_file(dir,"summary.json",1000));CHECK(contains(dir,"summary.json","stop_before_start"));fd2_audio_pcm_capture_close(c);return 0;}
static int output_collision(void){char dir[2048],p[2200];CHECK(!new_case(dir));fd2_audio_pcm_capture *c=fd2_audio_pcm_capture_open(dir,48000,2000,0);CHECK(c);CHECK(!start(dir,pid()));CHECK(ready(c,dir));path(p,dir,"app-mix.wav");CHECK(!write_text(p,"KEEP"));CHECK(!stop(dir));CHECK(wait_file(dir,"summary.json",1000));CHECK(contains(dir,"summary.json","io_error"));FILE *f=fopen(p,"rb");CHECK(f);char b[5]={0};CHECK(fread(b,1,5,f)==4&&!strcmp(b,"KEEP"));fclose(f);fd2_audio_pcm_capture_close(c);return 0;}
typedef struct{fd2_audio_pcm_capture *c;_Atomic int quit;} Producer;
static int SDLCALL producer(void *u){Producer *p=u;float a[1024];samples(a);while(!atomic_load(&p->quit)){fd2_audio_pcm_capture_push(p->c,a,512);SDL_Delay(1);}return 0;}
static int concurrent_stop(void){char dir[2048];CHECK(!new_case(dir));fd2_audio_pcm_capture *c=fd2_audio_pcm_capture_open(dir,48000,2000,0);CHECK(c);CHECK(!start(dir,pid()));Producer p={c};atomic_init(&p.quit,0);SDL_Thread *t=SDL_CreateThread(producer,"synthetic-observer-callback",&p);CHECK(t);CHECK(wait_file(dir,"ready.json",1000));SDL_Delay(20);CHECK(!stop(dir));CHECK(wait_file(dir,"summary.json",1200));CHECK(contains(dir,"summary.json","\"ok\":true"));CHECK(contains(dir,"summary.json","\"callback_quiescent\":true"));CHECK(!inspect_wave(dir));atomic_store(&p.quit,1);SDL_WaitThread(t,NULL);fd2_audio_pcm_capture_close(c);return 0;}
static int utf8_paths(void){char dir[2048],p[2200];CHECK(!new_case(dir));path(p,dir,"采集证据");CHECK(SDL_CreateDirectory(p));fd2_audio_pcm_capture *c=fd2_audio_pcm_capture_open(p,48000,2000,1);CHECK(c);CHECK(!start(p,pid()));CHECK(ready(c,p));CHECK(!stop(p));CHECK(wait_file(p,"summary.json",1000));char wavepath[2200];path(wavepath,p,"app-mix.wav");size_t size=0;void *b=SDL_LoadFile(wavepath,&size);CHECK(b&&size>44);CHECK(u32((unsigned char*)b+24)==48000);SDL_free(b);fd2_audio_pcm_capture_close(c);return 0;}
static int offline_boundary(void){
CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_APP_PCM_CAPTURE_DIR"));
fd2_audio_config config={48000,0,1};fd2_audio *a=fd2_audio_create(&config);CHECK(a);float x[8];CHECK(fd2_audio_render_offline(a,x,4)==4);for(unsigned i=0;i<8;i++)CHECK(x[i]==0);fd2_audio_destroy(a);
CHECK(SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_APP_PCM_CAPTURE_DIR",base,true));
CHECK(!fd2_audio_create(&config));
CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_APP_PCM_CAPTURE_DIR"));
return 0;}
int main(int argc,char **argv){CHECK(argc==2);CHECK(SDL_Init(0));snprintf(base,sizeof(base),"%s/%llu-%u",argv[1],(unsigned long long)SDL_GetPerformanceCounter(),pid());CHECK(SDL_CreateDirectory(base));CHECK(!reject_config());CHECK(!normal());CHECK(!wrong_owner());CHECK(!no_callback());CHECK(!bad_chunk(0));CHECK(!bad_chunk(1));CHECK(!early_stop());CHECK(!output_collision());CHECK(!concurrent_stop());CHECK(!utf8_paths());CHECK(!offline_boundary());SDL_Quit();printf("audio_pcm_capture_test:11groupsPASS; synthetic-only; no device/GUI\nartifacts:%s\n",base);return 0;}
