/* 真实probe main/Audio sync/Put/cleanup；仅替换平台设备和时间，不开设备。
 * v2没有销流中的RT renderer。验证最后Put首错、销流禁止再借userdata、
 * 同步retire-once及destroy后counter不变；不能回签旧callback捕获记录。 */
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
static SDL_AudioStream *SDLCALL pure_open(SDL_AudioDeviceID,const SDL_AudioSpec *,SDL_AudioStreamCallback,void *);
static bool SDLCALL pure_put(SDL_AudioStream *,const void *,int);
static int SDLCALL pure_queued(SDL_AudioStream *);
static bool SDLCALL pure_resume(SDL_AudioStream *);
static bool SDLCALL pure_pause(SDL_AudioStream *);
static void SDLCALL pure_destroy(SDL_AudioStream *);
static bool SDLCALL pure_init(SDL_InitFlags);
static void SDLCALL pure_quit(void);
static Uint64 SDLCALL pure_ticks(void);
static Uint64 SDLCALL pure_ns(void);
static void SDLCALL pure_delay(Uint32);
static bool SDLCALL pure_poll(SDL_Event *);
static const char *SDLCALL pure_backend(void);
static SDL_AudioDeviceID *SDLCALL pure_devices(int *);
static SDL_AudioDeviceID SDLCALL pure_device(SDL_AudioStream *);
static bool SDLCALL pure_format(SDL_AudioDeviceID,SDL_AudioSpec *,int *);
static int pure_printf(const char *,...);
static int pure_puts(const char *);
#define FD2_DEVICE_PROBE_NATIVE_OPEN pure_open
#define FD2_DEVICE_PROBE_NATIVE_PUT pure_put
#define FD2_DEVICE_PROBE_NATIVE_QUEUED pure_queued
#define SDL_ResumeAudioStreamDevice pure_resume
#define SDL_PauseAudioStreamDevice pure_pause
#define SDL_DestroyAudioStream pure_destroy
#define SDL_Init pure_init
#define SDL_Quit pure_quit
#define SDL_GetTicks pure_ticks
#define SDL_GetTicksNS pure_ns
#define SDL_Delay pure_delay
#define SDL_PollEvent pure_poll
#define SDL_GetCurrentAudioDriver pure_backend
#define SDL_GetAudioPlaybackDevices pure_devices
#define SDL_GetAudioStreamDevice pure_device
#define SDL_GetAudioDeviceFormat pure_format
#define printf pure_printf
#define puts pure_puts
#define main actual_probe_main
#ifndef FD2_PROBE_SOURCE
#define FD2_PROBE_SOURCE "audio_device_probe.c"
#endif
#include FD2_PROBE_SOURCE
#undef main
#undef puts
#undef printf
#undef SDL_GetTicksNS
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_Init
#undef SDL_Quit
#define CHECK(x) do {if(!(x)){fprintf(stderr,"finalization:%d: %s\n",__LINE__,#x);return 1;}}while(0)
static struct {
    void *sources[3];
    unsigned source_count, opens, pauses, destroys, post_pause_puts, bad;
    unsigned fail_put_at;
    int arm_failure_at_pause, paused, closed_at_pause, all_retired_at_pause, sync_rejected;
    Uint64 ticks;
    char output[16384];
    size_t output_size;
} fake;
static unsigned char stream_token;
static SDL_AudioStream *stream(void){return (SDL_AudioStream *)&stream_token;}
static int pure_printf(const char *format,...){
    va_list args;va_start(args,format);
    int n=vsnprintf(fake.output+fake.output_size,sizeof(fake.output)-fake.output_size,format,args);
    va_end(args);
    if(n>0&&(size_t)n<sizeof(fake.output)-fake.output_size)fake.output_size+=(size_t)n;
    return n;
}
static int pure_puts(const char *s){return pure_printf("%s\n",s);}
static bool SDLCALL pure_init(SDL_InitFlags f){return f==SDL_INIT_AUDIO;}
static void SDLCALL pure_quit(void){}
static Uint64 SDLCALL pure_ticks(void){return fake.ticks;}
static Uint64 SDLCALL pure_ns(void){return fake.ticks*UINT64_C(1000000);}
static void SDLCALL pure_delay(Uint32 ms){fake.ticks+=ms;}
static bool SDLCALL pure_poll(SDL_Event *event){(void)event;return false;}
static const char *SDLCALL pure_backend(void){return "pure-mocked-backend";}
static SDL_AudioDeviceID *SDLCALL pure_devices(int *n){*n=0;return NULL;}
static SDL_AudioDeviceID SDLCALL pure_device(SDL_AudioStream *s){return s==stream()?1:0;}
static bool SDLCALL pure_format(SDL_AudioDeviceID id,SDL_AudioSpec *s,int *f){
    if(id!=1)return false;*s=(SDL_AudioSpec){SDL_AUDIO_F32,2,48000};*f=8;return true;
}
static SDL_AudioStream *SDLCALL pure_open(SDL_AudioDeviceID id,const SDL_AudioSpec *s,
                                        SDL_AudioStreamCallback callback,void *user){
    fake.opens++;
    if(id!=SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK||!s||callback||user){fake.bad++;return NULL;}
    return stream();
}
static bool SDLCALL pure_resume(SDL_AudioStream *s){return s==stream();}
static bool SDLCALL pure_put(SDL_AudioStream *s,const void *data,int bytes){
    if(s!=stream()||!data||bytes<=0||bytes%8){fake.bad++;return false;}
    if(fake.paused)fake.post_pause_puts++;
    return observed_puts!=fake.fail_put_at && !(fake.paused&&fake.arm_failure_at_pause);
}
static int SDLCALL pure_queued(SDL_AudioStream *s){
    if(s!=stream()){fake.bad++;return -1;}
    fd2_audio *audio=active_audio;
    if(audio)for(size_t i=0;i<FD2_AUDIO_MAX_VOICES;i++)if(audio->voices[i].active){
        void *p=audio->voices[i].source.userdata;unsigned j;
        for(j=0;j<fake.source_count;j++)if(fake.sources[j]==p)break;
        if(j==fake.source_count&&fake.source_count<3)fake.sources[fake.source_count++]=p;
    }
    return 0; /* 模型设备每次消费完；不假称真实queue测量。 */
}
static bool SDLCALL pure_pause(SDL_AudioStream *s){
    fake.pauses++;fake.paused=1;
    fake.closed_at_pause=fd2_bios_clock_advance(pure_ns())==-1;
    fake.all_retired_at_pause=fake.source_count==3;
    for(unsigned i=0;i<fake.source_count;i++){
        probe_source *source=fake.sources[i]; /* main仍在scope；返回后不再解引用。 */
        fake.all_retired_at_pause&=atomic_load(&source->retires)==1;
    }
    fake.sync_rejected=active_audio && fd2_audio_sync(active_audio)==-1;
    return s==stream();
}
static void SDLCALL pure_destroy(SDL_AudioStream *s){if(s==stream())fake.destroys++;else fake.bad++;}
static int run_case(unsigned fail_at,int armed){
    memset(&fake,0,sizeof(fake));fake.fail_put_at=fail_at;fake.arm_failure_at_pause=armed;
    atomic_store(&observed_syncs,0);atomic_store(&observed_puts,0);
    atomic_store(&observed_put_successes,0);atomic_store(&rejected_nonzero_submissions,0);
    atomic_store(&observed_queue_peak,0);atomic_store(&wrong_owner_calls,0);callback_binding_errors=0;
    fd2_bios_clock_reset(0,0); /* 本用例明确external输入，非墙钟等待。 */
    char *argv[]={(char *)"pure-probe",(char *)"--execute",(char *)"100"};
    int result=actual_probe_main(3,argv);
    CHECK(result==(fail_at?1:0));
    CHECK(fake.opens==1&&fake.pauses==1&&fake.destroys==1&&!fake.bad);
    CHECK(fake.all_retired_at_pause&&fake.closed_at_pause&&fake.sync_rejected);
    CHECK(active_audio==NULL && fake.post_pause_puts==0);
    CHECK(atomic_load(&observed_puts)>3 && !atomic_load(&wrong_owner_calls));
    CHECK(atomic_load(&observed_puts)==atomic_load(&observed_put_successes)+(fail_at?1u:0u));
    CHECK(strstr(fake.output,"fd2-a8-device-probe-v2")&&strstr(fake.output,"\"device_callback_bound\":false"));
    CHECK(strstr(fake.output,fail_at?"\"first_error_before_destroy\":3":"\"first_error_before_destroy\":0"));
    CHECK(strstr(fake.output,fail_at?"\"ok\":false":"\"ok\":true"));
    unsigned puts_before=atomic_load(&observed_puts);
    CHECK(fd2_bios_clock_advance(pure_ns()+1)==-1&&atomic_load(&observed_puts)==puts_before);
    return 0;
}
int main(void){
    CHECK(run_case(3,0)==0); /* 合法主线程中途Put失败，源仍走完retire/清理。 */
    CHECK(run_case(0,1)==0); /* 关闭时已武装失败，但没有任何晚Put/RT借用。 */
    CHECK(run_case(0,0)==0);
    CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_TEST_AUDIO_QUIET"));
    puts("audio device finalization:3 real-main/mock-platform cases PASS; no device/GUI");
    return 0;
}
