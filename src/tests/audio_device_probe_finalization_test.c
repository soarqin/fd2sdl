/* 执行探针真实main/混音callback/收尾；仅SDL设备与时间调用替换，不开设备/GUI。
 * late故障发生于三个source均retire后、destroy等待最后一次callback期间。
 */
#include <SDL3/SDL.h>
#include <stdarg.h>
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static SDL_AudioStream *SDLCALL pure_open(SDL_AudioDeviceID, const SDL_AudioSpec *, SDL_AudioStreamCallback, void *);
static bool SDLCALL pure_put(SDL_AudioStream *, const void *, int);
static bool SDLCALL pure_resume(SDL_AudioStream *);
static bool SDLCALL pure_pause(SDL_AudioStream *);
static void SDLCALL pure_destroy(SDL_AudioStream *);
static bool SDLCALL pure_init(SDL_InitFlags);
static void SDLCALL pure_quit(void);
static Uint64 SDLCALL pure_ticks(void);
static void SDLCALL pure_delay(Uint32);
static bool SDLCALL pure_poll(SDL_Event *);
static const char *SDLCALL pure_backend(void);
static SDL_AudioDeviceID *SDLCALL pure_devices(int *);
static SDL_AudioDeviceID SDLCALL pure_device(SDL_AudioStream *);
static bool SDLCALL pure_format(SDL_AudioDeviceID, SDL_AudioSpec *, int *);
static int pure_printf(const char *, ...);
static int pure_puts(const char *);

#define FD2_DEVICE_PROBE_NATIVE_OPEN pure_open
#define FD2_DEVICE_PROBE_NATIVE_PUT pure_put
#define SDL_ResumeAudioStreamDevice pure_resume
#define SDL_PauseAudioStreamDevice pure_pause
#define SDL_DestroyAudioStream pure_destroy
#define SDL_Init pure_init
#define SDL_Quit pure_quit
#define SDL_GetTicks pure_ticks
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
#undef SDL_ResumeAudioStreamDevice
#undef SDL_PauseAudioStreamDevice
#undef SDL_DestroyAudioStream
#undef SDL_Init
#undef SDL_Quit
#undef SDL_GetTicks
#undef SDL_Delay
#undef SDL_PollEvent
#undef SDL_GetCurrentAudioDriver
#undef SDL_GetAudioPlaybackDevices
#undef SDL_GetAudioStreamDevice
#undef SDL_GetAudioDeviceFormat

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"finalization CHECK line%d: %s\n",__LINE__,#x); return 1; } } while(0)
static struct {
    SDL_AudioStreamCallback callback;
    void *userdata;
    void *sources[3];
    unsigned source_count, pauses, destroys, late_callbacks;
    int late_failure, injecting, all_retired_before_late, first_error_after_late;
    Uint64 ticks;
    char output[16384];
    size_t output_size;
} fake;
static unsigned char stream_token;
static SDL_AudioStream *stream(void) { return (SDL_AudioStream *)&stream_token; }

static int pure_printf(const char *format, ...) {
    va_list args; va_start(args,format);
    int n=vsnprintf(fake.output+fake.output_size,sizeof(fake.output)-fake.output_size,format,args);
    va_end(args);
    if (n>0 && (size_t)n<sizeof(fake.output)-fake.output_size)fake.output_size+=(size_t)n;
    return n;
}
static int pure_puts(const char *text) { return pure_printf("%s\n",text); }
static bool SDLCALL pure_init(SDL_InitFlags flags) { return flags==SDL_INIT_AUDIO; }
static void SDLCALL pure_quit(void) { }
static Uint64 SDLCALL pure_ticks(void) { return fake.ticks; }
static bool SDLCALL pure_poll(SDL_Event *event) { (void)event;return false; }
static const char *SDLCALL pure_backend(void) { return "pure-mocked-backend"; }
static SDL_AudioDeviceID *SDLCALL pure_devices(int *count) { *count=0;return NULL; }
static SDL_AudioDeviceID SDLCALL pure_device(SDL_AudioStream *s) { return s==stream()?1:0; }
static bool SDLCALL pure_format(SDL_AudioDeviceID id,SDL_AudioSpec *spec,int *frames) {
    if(id!=1)return false;*spec=(SDL_AudioSpec){SDL_AUDIO_F32,2,48000};*frames=8;return true;
}
static SDL_AudioStream *SDLCALL pure_open(SDL_AudioDeviceID id,const SDL_AudioSpec *spec,
                                        SDL_AudioStreamCallback callback,void *userdata) {
    if(id!=SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK || !spec || !callback || !userdata)return NULL;
    fake.callback=callback;fake.userdata=userdata;return stream();
}
static bool SDLCALL pure_resume(SDL_AudioStream *s) { return s==stream(); }
static bool SDLCALL pure_put(SDL_AudioStream *s,const void *data,int bytes) {
    if(s!=stream() || !data || bytes!=8*(int)(2*sizeof(float)))return false;
    return !(fake.injecting && fake.late_failure);
}
static void remember_sources(void) {
    fd2_audio *audio=fake.userdata;
    for(size_t i=0;i<FD2_AUDIO_MAX_VOICES;i++)if(audio->voices[i].active) {
        void *p=audio->voices[i].source.userdata;unsigned j;
        for(j=0;j<fake.source_count;j++)if(fake.sources[j]==p)break;
        if(j==fake.source_count && fake.source_count<3)fake.sources[fake.source_count++]=p;
    }
}
static void SDLCALL pure_delay(Uint32 milliseconds) {
    fake.ticks+=milliseconds;
    if(fake.callback){fake.callback(fake.userdata,stream(),8*(int)(2*sizeof(float)),0);remember_sources();}
}
static bool SDLCALL pure_pause(SDL_AudioStream *s) {
    fake.pauses++;
    fake.all_retired_before_late=fake.source_count==3;
    for(unsigned i=0;i<fake.source_count;i++) {
        probe_source *source=fake.sources[i];
        fake.all_retired_before_late &= atomic_load(&source->retires)==1;
    }
    /* 真实收尾窗口：main已判成功，但本callback的Put尚未完成；destroy仍须等它。 */
    fake.injecting=1;
    fake.callback(fake.userdata,stream(),8*(int)(2*sizeof(float)),0);
    fake.late_callbacks++;
    fd2_audio_error error=FD2_AUDIO_ERROR_NONE;
    (void)fd2_audio_get_first_error(fake.userdata,&error);
    fake.first_error_after_late=(int)error;
    fake.injecting=0;
    return s==stream();
}
static void SDLCALL pure_destroy(SDL_AudioStream *s) {
    if(s==stream()){fake.destroys++;fake.callback=NULL;}
}
static int run_case(int late_failure) {
    memset(&fake,0,sizeof(fake));fake.late_failure=late_failure;
    atomic_store(&observed_callbacks,0);atomic_store(&observed_puts,0);
    atomic_store(&observed_put_successes,0);atomic_store(&rejected_nonzero_submissions,0);
    char *argv[]={(char *)"private-pure-probe",(char *)"--execute",(char *)"100"};
    int result=actual_probe_main(3,argv);
    fprintf(stderr,"pure finalization late_failure=%d rc=%d puts=%u success=%u late_after_retire=%d first_after=%d\n",
        late_failure,result,atomic_load(&observed_puts),atomic_load(&observed_put_successes),
        fake.all_retired_before_late,fake.first_error_after_late);
    CHECK(result==(late_failure?1:0)); /* 旧代码在此真实返回0，而不是缺新字段造成RED。 */
    CHECK(fake.pauses==1 && fake.destroys==1 && fake.late_callbacks==1 && fake.all_retired_before_late);
    CHECK(atomic_load(&observed_puts)>1 && !atomic_load(&rejected_nonzero_submissions));
    CHECK(fake.first_error_after_late==(late_failure?FD2_AUDIO_ERROR_PUT:FD2_AUDIO_ERROR_NONE));
    CHECK(strstr(fake.output,"\"first_error_before_destroy\":0"));
    CHECK(strstr(fake.output,late_failure?"\"all_submissions_succeeded_after_destroy\":false":"\"all_submissions_succeeded_after_destroy\":true"));
    CHECK(strstr(fake.output,late_failure?"\"stage\":\"final_submission_check\"":"\"stage\":\"complete\""));
    CHECK(atomic_load(&observed_puts)==atomic_load(&observed_put_successes)+(unsigned)late_failure);
    return 0;
}
int main(void) {
    CHECK(run_case(1)==0);
    CHECK(run_case(0)==0);
    CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(),"FD2SDL_TEST_AUDIO_QUIET"));
    puts("audio device probe finalization: 2 real-main/callback mocked-platform cases passed; no device/GUI");
    return 0;
}
