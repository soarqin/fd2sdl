/* 应用侧测试PCM：观察audio.c真实设备callback的最终float stereo。
 * 在Windows进程session静音之前，不复播source、不改变设备/mute/gain。
 * 原版依据仍为music_track_play @0x25977 / sfx_play @0x25a96；此模块只取证。
 */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif
#include "audio_pcm_capture.h"
#include <SDL3/SDL.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <limits.h>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>
#include <io.h>
#else
#include <unistd.h>
#endif

#define READY_TIMEOUT_MS 3000u
#define START_TIMEOUT_MS 120000u
#define QUIET_TIMEOUT_MS 1000u

enum { E_NONE, E_OWNER, E_STOP_EARLY, E_NO_CALLBACK, E_CAPACITY,
       E_NONFINITE, E_FRAMES, E_IO, E_NOT_QUIET, E_HOST, E_TIME, E_START, E_AUDIO_PUT };
struct fd2_audio_pcm_capture {
    SDL_Thread *thread;
    float *buffer;
    uint32_t capacity, max_ms, pid;
    char *directory, *start, *stop, *ready, *summary, *wav;
    _Atomic unsigned enabled, busy, written, error, closing;
    uint64_t first_qpc,last_qpc;
    int ready_published, suppress_device_output;
    float silence[512*2];
};
static void fail(fd2_audio_pcm_capture *c,unsigned error) {
    unsigned expected=E_NONE;
    (void)atomic_compare_exchange_strong(&c->error,&expected,error);
}
static const char *error_name(unsigned e) {
    static const char *const names[]={"","wrong_start_pid","stop_before_ready","no_real_callback",
        "buffer_capacity","nonfinite_mix","invalid_chunk","io_error","callback_not_quiet",
        "host_shutdown","time_limit","start_timeout","audio_put_failed"};
    return e<sizeof(names)/sizeof(*names)?names[e]:"unknown_error";
}
static char *joined(const char *dir,const char *name) {
    size_t a=strlen(dir),b=strlen(name);char *p=malloc(a+b+2);
    if(p) {
#ifdef _WIN32
        snprintf(p,a+b+2,"%s\\%s",dir,name);
#else
        snprintf(p,a+b+2,"%s/%s",dir,name);
#endif
    }
    return p;
}
static int exists(const char *p) {
    SDL_PathInfo info;return SDL_GetPathInfo(p,&info);
}
static char *resolved_directory(const char *path) {
#ifdef _WIN32
    wchar_t wide[32768],real[32768];
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,32768))return NULL;
    HANDLE h=CreateFileW(wide,FILE_READ_ATTRIBUTES,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
        NULL,OPEN_EXISTING,FILE_FLAG_BACKUP_SEMANTICS,NULL);
    if(h==INVALID_HANDLE_VALUE)return NULL;
    DWORD n=GetFinalPathNameByHandleW(h,real,32768,FILE_NAME_NORMALIZED);CloseHandle(h);
    if(!n||n>=32768)return NULL;
    for(wchar_t *s=real;*s;) {
        while(*s==L'/'||*s==L'\\')s++;
        wchar_t *end=s;while(*end&&*end!=L'/'&&*end!=L'\\')end++;
        if(end-s==13&&!_wcsnicmp(s,L"original_game",13))return NULL;s=end;
    }
    int bytes=WideCharToMultiByte(CP_UTF8,0,real,-1,NULL,0,NULL,NULL);
    char *out=bytes>0?malloc((size_t)bytes):NULL;
    if(out&&!WideCharToMultiByte(CP_UTF8,0,real,-1,out,bytes,NULL,NULL)){free(out);out=NULL;}
    return out;
#else
    char *out=realpath(path,NULL);if(!out)return NULL;
    for(char *s=out;*s;) {
        while(*s=='/')s++;char *end=s;while(*end&&*end!='/')end++;
        if(end-s==13&&!strncmp(s,"original_game",13)){free(out);return NULL;}s=end;
    }
    return out;
#endif
}
static FILE *new_file(const char *path) {
#ifdef _WIN32
    wchar_t wide[32768];
    if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,wide,32768))return NULL;
    return _wfopen(wide,L"wbx");
#else
    return fopen(path,"wbx");
#endif
}
static int finish_file(FILE *file) {
    int ok=!ferror(file)&&fflush(file)==0;
#ifdef _WIN32
    if(ok)ok=_commit(_fileno(file))==0;
#else
    if(ok)ok=fsync(fileno(file))==0;
#endif
    if(fclose(file))ok=0;return ok?0:-1;
}
static int publish(const char *path,const char *text) {
    char *tmp=malloc(strlen(path)+5);if(!tmp)return -1;sprintf(tmp,"%s.tmp",path);
    FILE *file=new_file(tmp);int ok=file!=NULL;
    if(file){ok=fwrite(text,1,strlen(text),file)==strlen(text);if(finish_file(file))ok=0;}
    if(ok) {
#ifdef _WIN32
        wchar_t a[32768],b[32768];
        ok=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,tmp,-1,a,32768)&&
           MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,b,32768)&&MoveFileW(a,b);
#else
        ok=link(tmp,path)==0;if(ok)unlink(tmp);
#endif
    }
    if(!ok)SDL_RemovePath(tmp);free(tmp);return ok?0:-1;
}
static void le(unsigned char *p,uint32_t v,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(unsigned char)(v>>(8*i));}
static int write_wave(fd2_audio_pcm_capture *c,unsigned frames) {
    FILE *file=new_file(c->wav);if(!file)return -1;
    unsigned char h[44]={0};memcpy(h,"RIFF",4);le(h+4,36+frames*4,4);
    memcpy(h+8,"WAVEfmt ",8);le(h+16,16,4);le(h+20,1,2);le(h+22,2,2);
    le(h+24,48000,4);le(h+28,48000*4,4);le(h+32,4,2);le(h+34,16,2);
    memcpy(h+36,"data",4);le(h+40,frames*4,4);
    int ok=fwrite(h,1,sizeof(h),file)==sizeof(h);
    for(unsigned at=0;ok&&at<frames;) {
        unsigned n=frames-at;if(n>512)n=512;unsigned char pcm[512*4];
        for(unsigned i=0;i<n*2;i++) {
            float v=c->buffer[(size_t)at*2+i];
            long q=lroundf(v*32767.0f);le(pcm+2*i,(uint16_t)(int16_t)q,2);
        }
        ok=fwrite(pcm,1,n*4,file)==n*4;at+=n;
    }
    if(finish_file(file))ok=0;return ok?0:-1;
}
static int read_start_pid(fd2_audio_pcm_capture *c) {
    SDL_IOStream *io=SDL_IOFromFile(c->start,"rb");if(!io)return 1;
    char s[48]={0};Sint64 size=SDL_GetIOSize(io);size_t n=SDL_ReadIO(io,s,sizeof(s)-1);SDL_CloseIO(io);
    if(size<0||size>47)return -1;
    if(!n||n!=(size_t)size||s[n-1]!='\n')return 1;
    if(strlen(s)!=n)return -1;
    char *end;unsigned long v=strtoul(s,&end,10);
    while(*end=='\r'||*end=='\n')end++;
    return n&&v==c->pid&&*end==0?0:-1;
}
static int quiet(fd2_audio_pcm_capture *c) {
    /* seq_cst busy/enabled约束：关门前进入的callback必须完成，关门后进入的只读false。 */
    atomic_store(&c->enabled,0);
    uint64_t until=SDL_GetTicks()+QUIET_TIMEOUT_MS;
    while(atomic_load(&c->busy)) {if(SDL_GetTicks()>=until){fail(c,E_NOT_QUIET);return 0;}SDL_Delay(1);}
    return 1;
}
static int SDLCALL capture_worker(void *userdata) {
    fd2_audio_pcm_capture *c=userdata;const char *reason="host_shutdown";
    uint64_t deadline=SDL_GetTicks()+START_TIMEOUT_MS;
    while(!exists(c->start)) {
        if(atomic_load(&c->closing)){fail(c,E_HOST);goto done;}
        if(exists(c->stop)){fail(c,E_STOP_EARLY);reason="stop_before_start";goto done;}
        if(SDL_GetTicks()>=deadline){fail(c,E_START);reason="start_timeout";goto done;}
        SDL_Delay(2);
    }
    deadline=SDL_GetTicks()+READY_TIMEOUT_MS;
    int owner;
    while((owner=read_start_pid(c))==1) {
        if(atomic_load(&c->closing)){fail(c,E_HOST);goto done;}
        if(exists(c->stop)){fail(c,E_STOP_EARLY);reason="stop_before_ready";goto done;}
        if(SDL_GetTicks()>=deadline){owner=-1;break;}
        SDL_Delay(2);
    }
    if(owner){fail(c,E_OWNER);reason="bad_start";goto done;}
    atomic_store(&c->enabled,1);deadline=SDL_GetTicks()+READY_TIMEOUT_MS;
    while(!atomic_load_explicit(&c->written,memory_order_acquire)) {
        if(atomic_load(&c->error)){reason="capture_error";goto done;}
        if(atomic_load(&c->closing)){fail(c,E_HOST);goto done;}
        if(exists(c->stop)){fail(c,E_STOP_EARLY);reason="stop_before_ready";goto done;}
        if(SDL_GetTicks()>=deadline){fail(c,E_NO_CALLBACK);reason="no_callback";goto done;}
        SDL_Delay(2);
    }
    char text[1600];
    snprintf(text,sizeof(text),"{\"schema\":\"fd2-app-mix-capture-v1\",\"ready\":true,\"pid\":%u,\"backend\":\"fd2-audio-pre-os-session\",\"sample_rate\":48000,\"channels\":2,\"bits\":16,\"source_format\":\"F32-stereo\",\"clock_source\":\"SDL_performance_counter_at_real_mix_callback\",\"suppress_device_output\":%s,\"max_ms\":%u,\"first_callback_qpc\":%llu,\"qpc_frequency\":%llu,\"device_output_validated\":false,\"A8_validated\":false}\n",c->pid,c->suppress_device_output?"true":"false",c->max_ms,(unsigned long long)c->first_qpc,(unsigned long long)SDL_GetPerformanceFrequency());
    if(publish(c->ready,text)){fail(c,E_IO);reason="ready_io_error";goto done;}
    c->ready_published=1;deadline=SDL_GetTicks()+c->max_ms;
    for(;;) {
        if(atomic_load(&c->error)){reason="capture_error";break;}
        if(exists(c->stop)){reason="stop";break;}
        if(atomic_load(&c->closing)){fail(c,E_HOST);break;}
        if(SDL_GetTicks()>=deadline){fail(c,E_TIME);reason="time_limit";break;}
        SDL_Delay(2);
    }
done:;
    int quiescent=quiet(c);unsigned frames=atomic_load_explicit(&c->written,memory_order_acquire);
    int wave_ok=quiescent&&frames&&write_wave(c,frames)==0;
    if(quiescent&&frames&&!wave_ok)fail(c,E_IO);
    unsigned error=atomic_load(&c->error);
    int ok=!error&&c->ready_published&&wave_ok&&!strcmp(reason,"stop");
    uint64_t first=quiescent?c->first_qpc:0,last=quiescent?c->last_qpc:0;
    char summary[1800];
    snprintf(summary,sizeof(summary),"{\"schema\":\"fd2-app-mix-capture-v1\",\"ok\":%s,\"pid\":%u,\"backend\":\"fd2-audio-pre-os-session\",\"ready_published\":%s,\"source_format\":\"F32-stereo\",\"clock_source\":\"SDL_performance_counter_at_real_mix_callback\",\"suppress_device_output\":%s,\"sample_rate\":48000,\"channels\":2,\"bits\":16,\"frames\":%u,\"data_bytes\":%u,\"capacity_frames\":%u,\"max_ms\":%u,\"first_callback_qpc\":%llu,\"last_callback_qpc\":%llu,\"qpc_frequency\":%llu,\"stop_reason\":\"%s\",\"error_code\":%u,\"error\":\"%s\",\"callback_quiescent\":%s,\"wave_finalized\":%s,\"wav\":\"app-mix.wav\",\"device_output_validated\":false,\"A8_validated\":false}\n",ok?"true":"false",c->pid,c->ready_published?"true":"false",c->suppress_device_output?"true":"false",frames,frames*4,c->capacity,c->max_ms,(unsigned long long)first,(unsigned long long)last,(unsigned long long)SDL_GetPerformanceFrequency(),reason,error,error_name(error),quiescent?"true":"false",wave_ok?"true":"false");
    if(publish(c->summary,summary))SDL_Log("app PCM summary publish failed: %s",SDL_GetError());
    return ok?0:1;
}
fd2_audio_pcm_capture *fd2_audio_pcm_capture_open(const char *directory,int rate,unsigned max_ms,int suppress_device_output) {
    if(!directory||rate!=48000||!max_ms||max_ms>120000||(suppress_device_output!=0&&suppress_device_output!=1)){SDL_SetError("app PCM requires48000Hz and1..120000ms");return NULL;}
    fd2_audio_pcm_capture *c=calloc(1,sizeof(*c));if(!c)return NULL;
    c->directory=resolved_directory(directory);if(!c->directory)goto bad;
    SDL_PathInfo directory_info;
    if(!SDL_GetPathInfo(c->directory,&directory_info)||directory_info.type!=SDL_PATHTYPE_DIRECTORY)goto bad;
    c->suppress_device_output=suppress_device_output;
    c->start=joined(c->directory,"start");c->stop=joined(c->directory,"stop");
    c->ready=joined(c->directory,"ready.json");c->summary=joined(c->directory,"summary.json");c->wav=joined(c->directory,"app-mix.wav");
    if(!c->start||!c->stop||!c->ready||!c->summary||!c->wav)goto bad;
    const char *paths[]={c->start,c->stop,c->ready,c->summary,c->wav};
    for(size_t i=0;i<sizeof(paths)/sizeof(*paths);i++)if(exists(paths[i]))goto bad;
    const char *temps[]={"ready.json.tmp","summary.json.tmp"};
    for(size_t i=0;i<2;i++){char *tmp=joined(c->directory,temps[i]);if(!tmp)goto bad;int old=exists(tmp);free(tmp);if(old)goto bad;}
    c->capacity=(unsigned)((uint64_t)rate*max_ms/1000);c->max_ms=max_ms;
#ifdef _WIN32
    c->pid=GetCurrentProcessId();
#else
    c->pid=(unsigned)getpid();
#endif
    c->buffer=malloc((size_t)c->capacity*2*sizeof(float));if(!c->buffer)goto bad;
    atomic_init(&c->enabled,0);atomic_init(&c->busy,0);atomic_init(&c->written,0);atomic_init(&c->error,0);atomic_init(&c->closing,0);
    if(!atomic_is_lock_free(&c->enabled)||!atomic_is_lock_free(&c->busy)||!atomic_is_lock_free(&c->written)||!atomic_is_lock_free(&c->error))goto bad;
    c->thread=SDL_CreateThread(capture_worker,"app-mix-writer",c);if(!c->thread)goto bad;
    return c;
bad:
    free(c->buffer);free(c->directory);free(c->start);free(c->stop);free(c->ready);free(c->summary);free(c->wav);free(c);
    SDL_SetError("app PCM invalid/new output directory or allocation/thread failure");return NULL;
}
const float *fd2_audio_pcm_capture_submission(fd2_audio_pcm_capture *c,const float *mix,size_t frames) {
    if(c && frames>512){fail(c,E_FRAMES);return NULL;}
    return c && c->suppress_device_output ? c->silence : mix;
}
void fd2_audio_pcm_capture_push(fd2_audio_pcm_capture *c,const float *stereo,size_t frames) {
    if(!c)return;
    if(atomic_fetch_add(&c->busy,1)){fail(c,E_FRAMES);atomic_fetch_sub(&c->busy,1);return;}
    if(atomic_load(&c->enabled)) {
        unsigned at=atomic_load_explicit(&c->written,memory_order_relaxed);
        if(!stereo||!frames||frames>512)fail(c,E_FRAMES);
        else if(frames>c->capacity-at)fail(c,E_CAPACITY);
        else {
            int finite=1;for(size_t i=0;i<frames*2;i++)if(!isfinite(stereo[i])||stereo[i]>1||stereo[i]<-1){finite=0;break;}
            if(!finite)fail(c,E_NONFINITE);
            else {
                uint64_t qpc=SDL_GetPerformanceCounter();if(!at)c->first_qpc=qpc;c->last_qpc=qpc;
                memcpy(c->buffer+(size_t)at*2,stereo,frames*2*sizeof(float));
                atomic_store_explicit(&c->written,at+(unsigned)frames,memory_order_release);
            }
        }
    }
    atomic_fetch_sub(&c->busy,1);
}
void fd2_audio_pcm_capture_put_failed(fd2_audio_pcm_capture *c) {
    if(!c)return;
    /* 与成功push使用同一接收窗口；quiet关闭门并等已进入的通知结束。 */
    atomic_fetch_add(&c->busy,1);
    if(atomic_load(&c->enabled))fail(c,E_AUDIO_PUT);
    atomic_fetch_sub(&c->busy,1);
}
void fd2_audio_pcm_capture_close(fd2_audio_pcm_capture *c) {
    if(!c)return;atomic_store(&c->closing,1);SDL_WaitThread(c->thread,NULL);
    free(c->buffer);free(c->directory);free(c->start);free(c->stop);free(c->ready);free(c->summary);free(c->wav);free(c);
}
