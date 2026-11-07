/* 私有TU：执行真实audio_stream_callback，mock唯一设备Put；无设备/GUI。 */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif
static int test_render_calls,test_put_calls,bad,quiet_mode,put_success=1;
static bool mock_put(SDL_AudioStream *stream,const void *data,int bytes) {
    (void)stream;test_put_calls++;if(test_render_calls!=test_put_calls||bytes!=512*2*(int)sizeof(float))bad=1;
    const float *v=data;for(int i=0;i<1024;i++)if(v[i]!=(quiet_mode?0.0f:((i&1)?-.25f:.25f)))bad=1;
    return put_success!=0;
}
static int fake_opened,fake_paused,fake_destroyed;
static SDL_AudioStream *mock_open(SDL_AudioDeviceID id,const SDL_AudioSpec *spec,SDL_AudioStreamCallback cb,void *u){(void)id;(void)cb;(void)u;if(spec->format!=SDL_AUDIO_F32||spec->freq!=48000||spec->channels!=2)bad=1;fake_opened++;return (SDL_AudioStream*)1;}
static bool mock_resume(SDL_AudioStream *s){(void)s;return true;}
static bool mock_pause(SDL_AudioStream *s){(void)s;fake_paused++;return true;}
static void mock_destroy(SDL_AudioStream *s){(void)s;fake_destroyed++;}
#define SDL_OpenAudioDeviceStream mock_open
#define SDL_ResumeAudioStreamDevice mock_resume
#define SDL_PauseAudioStreamDevice mock_pause
#define SDL_DestroyAudioStream mock_destroy
#define SDL_PutAudioStreamData mock_put
#ifndef FD2_CAPTURE_AUDIO_IMPL
#define FD2_CAPTURE_AUDIO_IMPL "../audio.c"
#endif
#include FD2_CAPTURE_AUDIO_IMPL
#undef SDL_PutAudioStreamData
#undef SDL_OpenAudioDeviceStream
#undef SDL_ResumeAudioStreamDevice
#undef SDL_PauseAudioStreamDevice
#undef SDL_DestroyAudioStream
#define CHECK(x) do{if(!(x)){fprintf(stderr,"hook check failed %d: %s (%s)\n",__LINE__,#x,SDL_GetError());return 1;}}while(0)
static unsigned pid(void){
#ifdef _WIN32
return GetCurrentProcessId();
#else
return (unsigned)getpid();
#endif
}
static size_t source_render(void *u,float *out,size_t n){(void)u;test_render_calls++;for(size_t i=0;i<n*2;i++)out[i]=(i&1)?-.25f:.25f;return n;}
static void setup(fd2_audio *a){memset(a,0,sizeof(*a));a->master_gain=1;for(size_t i=0;i<FD2_AUDIO_BUS_COUNT;i++)atomic_init(&a->bus_gain_bits[i],gain_bits(1.0f));atomic_init(&a->first_error,FD2_AUDIO_ERROR_NONE);a->voices[0].active=1;a->voices[0].gain=1;a->voices[0].source.render=source_render;atomic_init(&a->command_read,0);atomic_init(&a->command_write,0);}
static int exists(const char *p){SDL_PathInfo s;return SDL_GetPathInfo(p,&s);}
static int write_text(const char *p,const char *s){FILE *f=fopen(p,"wb");if(!f)return -1;int ok=fputs(s,f)>=0;if(fclose(f))ok=0;return ok?0:-1;}
static int run_capture(const char *base,int success){char dir[2200],start[2300],ready[2300],stop[2300],summary[2300],wavepath[2300],id[64];snprintf(dir,sizeof(dir),"%s/hook-%d-%llu",base,success,(unsigned long long)SDL_GetPerformanceCounter());CHECK(SDL_CreateDirectory(dir));snprintf(start,sizeof(start),"%s/start",dir);snprintf(ready,sizeof(ready),"%s/ready.json",dir);snprintf(stop,sizeof(stop),"%s/stop",dir);snprintf(summary,sizeof(summary),"%s/summary.json",dir);snprintf(wavepath,sizeof(wavepath),"%s/app-mix.wav",dir);
 fd2_audio a;setup(&a);a.capture=fd2_audio_pcm_capture_open(dir,48000,4000,1);CHECK(a.capture);snprintf(id,sizeof(id),"%u\n",pid());CHECK(!write_text(start,id));test_render_calls=test_put_calls=bad=0;quiet_mode=1;put_success=success;uint64_t until=SDL_GetTicks()+3800;
 while(!exists(success?ready:summary)){audio_stream_callback(&a,(SDL_AudioStream*)1,4096,0);SDL_Delay(2);CHECK(SDL_GetTicks()<until);}
 CHECK(test_render_calls>0&&test_render_calls==test_put_calls&&!bad);
 if(success){CHECK(!write_text(stop,"stop\n"));until=SDL_GetTicks()+1000;while(!exists(summary)){CHECK(SDL_GetTicks()<until);SDL_Delay(2);}FILE *f=fopen(wavepath,"rb");CHECK(f);unsigned char h[44],x[4];CHECK(fread(h,1,44,f)==44);unsigned n=h[40]|h[41]<<8|h[42]<<16|h[43]<<24;CHECK(n>0&&n%4==0);for(unsigned i=0;i<n/4;i++){CHECK(fread(x,1,4,f)==4);CHECK(x[0]==0&&x[1]==32&&x[2]==0&&x[3]==224);}fclose(f);}
 else {CHECK(!exists(ready)&&!exists(wavepath));FILE *f=fopen(summary,"rb");CHECK(f);char s[2400]={0};fread(s,1,sizeof(s)-1,f);fclose(f);CHECK(strstr(s,"audio_put_failed")&&strstr(s,"\"ok\":false"));}
 fd2_audio_pcm_capture_close(a.capture);return 0;}
/* 正式callback先成功READY，再失败一次Put、再恢复；旧实现会错误封ok。 */
static int put_failure_after_ready(const char *base, int earlier_error) {
 char dir[2200],start[2300],ready[2300],stop[2300],summary[2300],wavepath[2300],id[64];
 snprintf(dir,sizeof(dir),"%s/put-after-ready-%d-%llu",base,earlier_error,(unsigned long long)SDL_GetPerformanceCounter());
 CHECK(SDL_CreateDirectory(dir));
 snprintf(start,sizeof(start),"%s/start",dir);snprintf(ready,sizeof(ready),"%s/ready.json",dir);
 snprintf(stop,sizeof(stop),"%s/stop",dir);snprintf(summary,sizeof(summary),"%s/summary.json",dir);
 snprintf(wavepath,sizeof(wavepath),"%s/app-mix.wav",dir);
 fd2_audio a;setup(&a);a.capture=fd2_audio_pcm_capture_open(dir,48000,4000,1);CHECK(a.capture);
 snprintf(id,sizeof(id),"%u\n",pid());CHECK(!write_text(start,id));
 quiet_mode=1;put_success=1;test_render_calls=test_put_calls=bad=0;
 uint64_t until=SDL_GetTicks()+3000;
 while(!exists(ready)){audio_stream_callback(&a,(SDL_AudioStream*)1,4096,0);SDL_Delay(2);CHECK(SDL_GetTicks()<until);}
 int calls=test_render_calls;
 if(earlier_error){float invalid[2]={NAN,0};fd2_audio_pcm_capture_push(a.capture,invalid,1);}
 put_success=0;audio_stream_callback(&a,(SDL_AudioStream*)1,4096,0);
 CHECK(test_render_calls==calls+1&&test_put_calls==calls+1&&!bad);
 fd2_audio_error error=FD2_AUDIO_ERROR_NONE;
 CHECK(fd2_audio_get_first_error(&a,&error)==0&&error==FD2_AUDIO_ERROR_PUT);
 put_success=1;audio_stream_callback(&a,(SDL_AudioStream*)1,4096,0);
 CHECK(test_render_calls==calls+2&&test_put_calls==calls+2&&!bad);
 CHECK(fd2_audio_get_first_error(&a,&error)==0&&error==FD2_AUDIO_ERROR_PUT);
 CHECK(!write_text(stop,"stop\n"));until=SDL_GetTicks()+1500;
 while(!exists(summary)){SDL_Delay(2);CHECK(SDL_GetTicks()<until);}
 size_t size=0;char *result=SDL_LoadFile(summary,&size);CHECK(result&&size);
 CHECK(strstr(result,"\"ok\":false")&&strstr(result,"\"ready_published\":true"));
 CHECK(strstr(result,earlier_error?"nonfinite_mix":"audio_put_failed"));
 CHECK(strstr(result,"\"callback_quiescent\":true")&&strstr(result,"\"wave_finalized\":true"));
 SDL_free(result);FILE *wave=fopen(wavepath,"rb");CHECK(wave);unsigned char header[44];
 CHECK(fread(header,1,sizeof(header),wave)==sizeof(header));
 unsigned bytes=header[40]|header[41]<<8|header[42]<<16|header[43]<<24;
 CHECK(bytes>0&&bytes%4==0);fclose(wave);
 fd2_audio_pcm_capture_close(a.capture);a.capture=NULL;return 0;
}

/* 关闭观察窗口后，后续Put首错仍记audio对象，但不改封存窗口结果。 */
static int put_failure_after_stopped_window(const char *base) {
 char dir[2200],start[2300],ready[2300],stop[2300],summary[2300],id[64];
 snprintf(dir,sizeof(dir),"%s/put-after-stop-%llu",base,(unsigned long long)SDL_GetPerformanceCounter());
 CHECK(SDL_CreateDirectory(dir));snprintf(start,sizeof(start),"%s/start",dir);
 snprintf(ready,sizeof(ready),"%s/ready.json",dir);snprintf(stop,sizeof(stop),"%s/stop",dir);
 snprintf(summary,sizeof(summary),"%s/summary.json",dir);
 fd2_audio a;setup(&a);a.capture=fd2_audio_pcm_capture_open(dir,48000,4000,1);CHECK(a.capture);
 snprintf(id,sizeof(id),"%u\n",pid());CHECK(!write_text(start,id));
 quiet_mode=1;put_success=1;test_render_calls=test_put_calls=bad=0;uint64_t until=SDL_GetTicks()+3000;
 while(!exists(ready)){audio_stream_callback(&a,(SDL_AudioStream*)1,4096,0);SDL_Delay(2);CHECK(SDL_GetTicks()<until);}
 CHECK(!write_text(stop,"stop\n"));until=SDL_GetTicks()+1500;
 while(!exists(summary)){SDL_Delay(2);CHECK(SDL_GetTicks()<until);}
 size_t n=0,m=0;char *before=SDL_LoadFile(summary,&n);CHECK(before&&strstr(before,"\"ok\":true"));
 put_success=0;audio_stream_callback(&a,(SDL_AudioStream*)1,4096,0);
 fd2_audio_error error=FD2_AUDIO_ERROR_NONE;CHECK(fd2_audio_get_first_error(&a,&error)==0&&error==FD2_AUDIO_ERROR_PUT);
 char *after=SDL_LoadFile(summary,&m);CHECK(after&&n==m&&!memcmp(before,after,n));
 CHECK(!bad&&test_render_calls==test_put_calls);SDL_free(before);SDL_free(after);
 fd2_audio_pcm_capture_close(a.capture);a.capture=NULL;
 /* NULL observer同样不影响首错和恢复后的单次render/Put语义。 */
 setup(&a);quiet_mode=0;put_success=0;test_render_calls=test_put_calls=bad=0;
 audio_stream_callback(&a,(SDL_AudioStream*)1,4096,0);
 CHECK(test_render_calls==1&&test_put_calls==1&&!bad);
 CHECK(fd2_audio_get_first_error(&a,&error)==0&&error==FD2_AUDIO_ERROR_PUT);
 put_success=1;audio_stream_callback(&a,(SDL_AudioStream*)1,4096,0);
 CHECK(test_render_calls==2&&test_put_calls==2&&!bad);
 return 0;
}

static int destroy_waiting_worker(const char *base){char dir[2200],summary[2300];snprintf(dir,sizeof(dir),"%s/destroy-%llu",base,(unsigned long long)SDL_GetPerformanceCounter());CHECK(SDL_CreateDirectory(dir));fd2_audio_config config={48000,0,1};fd2_audio *a=fd2_audio_create(&config);CHECK(a);a->capture=fd2_audio_pcm_capture_open(dir,48000,45000,1);CHECK(a->capture);uint64_t began=SDL_GetTicks();fd2_audio_destroy(a);CHECK(SDL_GetTicks()-began<1000);snprintf(summary,sizeof(summary),"%s/summary.json",dir);uint64_t until=SDL_GetTicks()+500;while(!exists(summary)){CHECK(SDL_GetTicks()<until);SDL_Delay(2);}FILE *f=fopen(summary,"rb");CHECK(f);char text[2400]={0};fread(text,1,sizeof(text)-1,f);fclose(f);CHECK(strstr(text,"host_shutdown")&&strstr(text,"\"ok\":false")&&strstr(text,"\"callback_quiescent\":true"));return 0;}

static int utf8_env_create_destroy(const char *base,int from_test_quiet){char dir[2200],summary[2300];snprintf(dir,sizeof(dir),"%s/中文环境-%llu",base,(unsigned long long)SDL_GetPerformanceCounter());CHECK(SDL_CreateDirectory(dir));CHECK(SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_APP_PCM_CAPTURE_DIR",dir,true));CHECK(SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_APP_PCM_CAPTURE_MAX_MS","45000",true));CHECK(SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_APP_PCM_CAPTURE_QUIET",from_test_quiet?"0":"1",true));CHECK(SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_TEST_AUDIO_QUIET",from_test_quiet?"1":"0",true));fd2_audio_config config={48000,1,0};fake_opened=fake_paused=fake_destroyed=0;fd2_audio *a=fd2_audio_create(&config);CHECK(a&&a->capture&&fake_opened==1);fd2_audio_destroy(a);CHECK(fake_paused==1&&fake_destroyed==1);snprintf(summary,sizeof(summary),"%s/summary.json",dir);size_t n=0;char *text=SDL_LoadFile(summary,&n);CHECK(text&&strstr(text,"host_shutdown")&&strstr(text,"\"suppress_device_output\":true"));SDL_free(text);CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_APP_PCM_CAPTURE_DIR"));CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_APP_PCM_CAPTURE_MAX_MS"));CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_APP_PCM_CAPTURE_QUIET"));CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_TEST_AUDIO_QUIET"));return 0;}

/* 无observer：真实create读env，唯一mix→等长silent Put，无17MB缓冲/worker。 */
static int quiet_without_observer(void){
 CHECK(SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_TEST_AUDIO_QUIET","1",true));
 fd2_audio_config config={48000,1,0};fd2_audio *a=fd2_audio_create(&config);
 CHECK(a&&a->test_quiet&&!a->capture);
 a->voices[0].active=1;a->voices[0].gain=1;a->voices[0].source.render=source_render;
 quiet_mode=1;test_render_calls=test_put_calls=bad=0;put_success=1;
 audio_stream_callback(a,(SDL_AudioStream*)1,4096,0);
 CHECK(test_render_calls==1&&test_put_calls==1&&!bad&&!a->capture);
 for(size_t i=0;i<1024;i++)CHECK(a->mix_buffer[i]==((i&1)?-.25f:.25f));
 fd2_audio_destroy(a);
 CHECK(SDL_SetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_TEST_AUDIO_QUIET","invalid",true));
 int opens=fake_opened;CHECK(!fd2_audio_create(&config)&&fake_opened==opens);
 CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_TEST_AUDIO_QUIET"));return 0;
}

int main(int argc,char **argv){CHECK(argc==2);CHECK(SDL_Init(0));CHECK(SDL_CreateDirectory(argv[1]));
CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_APP_PCM_CAPTURE_DIR"));
CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_TEST_AUDIO_QUIET"));
CHECK(!put_failure_after_ready(argv[1],0));
CHECK(!put_failure_after_ready(argv[1],1));
CHECK(!put_failure_after_stopped_window(argv[1]));
CHECK(!destroy_waiting_worker(argv[1]));
CHECK(!utf8_env_create_destroy(argv[1],0));
CHECK(!utf8_env_create_destroy(argv[1],1));
CHECK(!quiet_without_observer());
fd2_audio a;setup(&a);quiet_mode=0;test_render_calls=test_put_calls=bad=0;put_success=1;audio_stream_callback(&a,(SDL_AudioStream*)1,4096,0);CHECK(test_render_calls==1&&test_put_calls==1&&!bad);CHECK(!run_capture(argv[1],1));CHECK(!run_capture(argv[1],0));SDL_Quit();puts("audio callback hook: render-once -> equal-size silent successfulPut -> original-mix tap; failedPutnotapped; defaultunchanged PASS");return 0;}
