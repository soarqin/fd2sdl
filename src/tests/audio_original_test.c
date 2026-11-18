/* F5正式48k profile：真实Audio/BIOS/AIL/DIG/SB/channel/mixdown；设备仅替身。
 * strict前缀literal由Python Fraction独立算，不使用被测prefix_frames造expected。
 * 不初始化SDL音频、不访问设备，不签原PCM/PIC相位/绝对onset或可听。 */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "audio.h"
#include "audio_pcm_capture.h"
static uint64_t host_ns;
static unsigned host_reads;
static SDL_ThreadID host_tid = 1;
static const char *env_quiet, *env_capture;
static uint64_t test_now(void) { host_reads++; return host_ns; }
static SDL_ThreadID test_thread(void) { return host_tid; }
static bool test_time(SDL_Time *time) { *time = 0; return true; }
static bool test_date(SDL_Time time, SDL_DateTime *date, bool local) {
    (void)time; (void)local; memset(date, 0, sizeof(*date)); return true;
}
static SDL_Environment *test_environment(void) { return NULL; }
static bool test_error(const char *format, ...) { (void)format; return false; }
static const char *test_env(SDL_Environment *env, const char *name) {
    (void)env;
    if (!strcmp(name, "FD2SDL_TEST_AUDIO_QUIET")) return env_quiet;
    if (!strcmp(name, "FD2SDL_APP_PCM_CAPTURE_DIR")) return env_capture;
    return NULL;
}
static SDL_AudioStream *SDLCALL test_open(SDL_AudioDeviceID, const SDL_AudioSpec *, SDL_AudioStreamCallback, void *);
static bool SDLCALL test_resume(SDL_AudioStream *);
static bool SDLCALL test_pause(SDL_AudioStream *);
static void SDLCALL test_destroy(SDL_AudioStream *);
static bool SDLCALL test_put(SDL_AudioStream *, const void *, int);
static int SDLCALL test_queued(SDL_AudioStream *);
#define SDL_GetTicksNS test_now
#define SDL_GetCurrentThreadID test_thread
#define SDL_GetCurrentTime test_time
#define SDL_TimeToDateTime test_date
#define SDL_GetEnvironmentVariable test_env
#define SDL_GetEnvironment test_environment
#define SDL_SetError test_error
#define SDL_OpenAudioDeviceStream test_open
#define SDL_ResumeAudioStreamDevice test_resume
#define SDL_PauseAudioStreamDevice test_pause
#define SDL_DestroyAudioStream test_destroy
#define SDL_PutAudioStreamData test_put
#define SDL_GetAudioStreamQueued test_queued
#include "bios_clock.c"
#include "../audio.c"
#include "audio_pcm.h"
#include "audio_original_vectors.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #x); return 0; } } while (0)
static struct {
    unsigned opens, resumes, pauses, destroys, puts, push, put_failed, dropped, closed;
    unsigned invalid, fail_put, order_count, queue_queries;
    int fail_open, fail_resume, queued, fail_query;
    size_t success_frames;
    float output[16384];
    char order[128];
} device;
static unsigned char stream_token, capture_token;
static SDL_AudioStream *stream_id(void) { return (SDL_AudioStream *)&stream_token; }
static fd2_audio_pcm_capture *capture_id(void) { return (fd2_audio_pcm_capture *)&capture_token; }
static void order(char value) {
    if (device.order_count < sizeof(device.order)) device.order[device.order_count++] = value;
}
static SDL_AudioStream *SDLCALL test_open(SDL_AudioDeviceID id, const SDL_AudioSpec *spec,
                                         SDL_AudioStreamCallback callback, void *user) {
    device.opens++;
    if (id != SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK || !spec || spec->format != SDL_AUDIO_F32 ||
        spec->channels != 2 || callback || user) device.invalid++;
    return device.fail_open ? NULL : stream_id();
}
static bool SDLCALL test_resume(SDL_AudioStream *stream) {
    device.resumes++; if (stream != stream_id()) device.invalid++;
    return !device.fail_resume;
}
static bool SDLCALL test_pause(SDL_AudioStream *stream) {
    device.pauses++; order('P'); if (stream != stream_id()) device.invalid++;
    return false; /* 即使Pause失败也必须Destroy后才retire。 */
}
static void SDLCALL test_destroy(SDL_AudioStream *stream) {
    device.destroys++; order('D'); if (stream != stream_id()) device.invalid++;
}
static int SDLCALL test_queued(SDL_AudioStream *stream) {
    device.queue_queries++;
    if (stream != stream_id()) device.invalid++;
    return device.fail_query ? -1 : device.queued;
}
static bool SDLCALL test_put(SDL_AudioStream *stream, const void *data, int bytes) {
    device.puts++;
    if (stream != stream_id() || !data || bytes <= 0 || bytes % 8) device.invalid++;
    if (device.puts == device.fail_put) { order('F'); return false; }
    size_t frames = (size_t)bytes / 8u;
    if (device.success_frames + frames <= sizeof(device.output) / (2u * sizeof(float)))
        memcpy(device.output + device.success_frames * 2u, data, (size_t)bytes);
    device.success_frames += frames; device.queued += bytes; order('U'); return true;
}
fd2_audio_pcm_capture *fd2_audio_pcm_capture_open(const char *directory, int rate,
                                                unsigned ms, int quiet) {
    (void)directory; (void)rate; (void)ms; (void)quiet; return capture_id();
}
const float *fd2_audio_pcm_capture_submission(fd2_audio_pcm_capture *capture,
                                            const float *mix, size_t frames) {
    (void)capture; (void)frames; return mix;
}
void fd2_audio_pcm_capture_push(fd2_audio_pcm_capture *capture, const float *mix, size_t frames) {
    (void)mix; (void)frames;
    if (capture) { device.push++; order('C'); }
}
void fd2_audio_pcm_capture_put_failed(fd2_audio_pcm_capture *capture) {
    if (capture) device.put_failed++;
}
void fd2_audio_pcm_capture_output_dropped(fd2_audio_pcm_capture *capture) {
    if (capture) device.dropped++;
}
void fd2_audio_pcm_capture_close(fd2_audio_pcm_capture *capture) {
    if (capture) { device.closed++; order('X'); }
}
typedef struct {
    uint64_t frames;
    unsigned renders, retired;
    float value;
    fd2_audio_retire_reason reason;
    int reenter_result;
    fd2_audio *reenter;
} source;
static size_t source_render(void *user, float *output, size_t frames) {
    source *s = user; s->frames += frames; s->renders++;
    if (s->reenter) s->reenter_result = fd2_audio_set_master_gain(s->reenter, 0);
    for (size_t i = 0; i < frames * 2u; i++) output[i] = s->value;
    return frames;
}
static void source_retired(void *user, fd2_audio_retire_reason reason) {
    source *s = user; s->retired++; s->reason = reason; order('R');
    if (s->reenter) s->reenter_result = fd2_audio_stop_music(s->reenter);
}
static fd2_audio_source make_source(source *s) {
    fd2_audio_source result = { source_render, source_retired, s }; return result;
}
static int reset_test(void) {
    CHECK(active_audio == NULL);
    memset(&device, 0, sizeof(device)); host_ns = 0; host_tid = 1; host_reads = 0;
    env_quiet = env_capture = NULL;
    clock_initialized = 0; /* 独立App scope，随后真实init建立live，不借external测试future。 */
    CHECK(fd2_bios_clock_init() == 0); return 1;
}
static fd2_audio *make_audio(int rate, int live, int allow) {
    fd2_audio_config config = { rate, live, allow, 0 }; return fd2_audio_create(&config);
}

static fd2_audio *make_original(int live,int allow) {
    fd2_audio_config c={48000,live,allow,1};return fd2_audio_create(&c);
}
static uint64_t words_fnv(const int32_t *p,size_t n) {
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<n;i++)for(unsigned sh=0;sh<32;sh+=8) {
        h^=((uint32_t)p[i]>>sh)&255u;h*=UINT64_C(1099511628211);
    }return h;
}
static uint64_t bytes_fnv(const uint8_t *p,size_t n) {
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<n;i++){h^=p[i];h*=UINT64_C(1099511628211);}return h;
}
static void idle_mdi(void *user) { if(user)(*(unsigned *)user)++; }
static int prepare_driver(fd2_audio *audio,fd2_pcm_driver *d) {
    CHECK(fd2_bios_clock_ail_attach(fd2_audio_now_ns(audio),65536)==0);
    CHECK(fd2_bios_clock_install_mdi(fd2_audio_now_ns(audio),idle_mdi,NULL)==0);
    CHECK(fd2_pcm_driver_init(d,audio)==0);
    CHECK(d->dig.timer_handle==4 && d->dig.timer->slots[1].period_us==5000);
    return 1;
}
static int shutdown_driver(fd2_audio *audio,fd2_pcm_driver *d) {
    CHECK(fd2_pcm_driver_quiesce(d)==0);
    CHECK(audio->original_quiesced && audio->original_sb.dma.mode==FD2_SB_NONE);
    CHECK(audio->original_sb.dma.left==0 && audio->original_sb.dma.total==0);
    CHECK(!audio->original_sb.dma.irq_pending && !audio->original_sb.dma.end.pending);
    CHECK(fd2_bios_clock_close()==0);
    fd2_audio_destroy(audio);fd2_pcm_driver_close(d);
    CHECK(fd2_bios_clock_advance(host_ns+1000000)<0);
    return 1;
}
static size_t paired_fm(void *user,float *out,size_t frames) {
    uint64_t *count=user;*count+=frames;
    for(size_t i=0;i<frames;i++){out[i*2]=1024.0f/32768.0f;out[i*2+1]=-1536.0f/32768.0f;}
    return frames;
}
static int original_shared_clock_tape(void) {
    CHECK(reset_test());fd2_audio *audio=make_original(0,1);CHECK(audio);
    fd2_pcm_driver d={0};CHECK(prepare_driver(audio,&d));
    fd2_pcm_player p={0},q={0};
    fd2_pcm_player untouched=p;
    CHECK(fd2_pcm_player_init(&p,audio,11025)==FD2_DIG_H_SHARED_DRIVER_REQUIRED &&
        memcmp(&p,&untouched,sizeof(p))==0);
    CHECK(fd2_pcm_player_init_shared(&p,&d,0,11025)==0);
    CHECK(fd2_pcm_player_init_shared(&q,&d,1,11025)==0);
    CHECK(p.driver==q.driver && p.handle==0 && q.handle==1);
    CHECK(fd2_pcm_player_available(&p) && fd2_pcm_player_available(&q));
    CHECK(fd2_pcm_player_set_enabled(&p,0)==0 && !d.dig.enabled);
    CHECK(fd2_pcm_play_replace(&q,NULL,SIZE_MAX,-1,0)==0); /* 原disabled先资源。 */
    CHECK(fd2_pcm_player_set_enabled(&q,1)==0 && d.dig.enabled);
    uint8_t bytes[2510]={76,76,76,76,76,76,10,0,0,0};
    for(size_t i=0;i<2500;i++)bytes[10+i]=(uint8_t)(i*37u+7u);
    fd2_pcm_bank bank={0};CHECK(fd2_pcm_bank_open_mem(&bank,bytes,sizeof(bytes))==0);
    uint64_t fm_frames=0;fd2_audio_source fm={paired_fm,NULL,&fm_frames};
    CHECK(fd2_audio_play_music_source(audio,fm,1)==0);
    CHECK(fd2_pcm_play_replace(&p,&bank,0,0,101.0f/128.0f)==0);
    float out[96];host_reads=0;
    for(size_t ms=0;ms<110;ms++) {
        CHECK(fd2_audio_render_offline(audio,out,48)==48);
        const original_checkpoint *v=&original_checkpoints[ms];
        for(size_t i=0;i<96;i++)CHECK(out[i]==original_expected_pcm[ms*96u+i]/32768.0f);
        fd2_sb_channel *c=&audio->original_sb.channel;fd2_audio_dig *g=&d.dig;
        fd2_sb_dma_provider *s=&audio->original_sb.dma;
        const int64_t state[]={(int64_t)c->written,(int64_t)c->read,(int64_t)c->rendered,
            c->last,c->current,c->delta,c->freq_f,c->freq_fslew,c->gain[0],c->gain[1],
            c->lowpass[0],c->lowpass[1],c->current_loaded,c->enabled,
            g->playing,g->empty_services,g->entered_active,g->last_half,
            g->samples[0].length,g->samples[0].position,g->samples[0].loop_count,g->samples[0].status,
            g->samples[1].length,g->samples[1].position,g->samples[1].loop_count,g->samples[1].status,
            (int64_t)s->cursor,s->left,s->total,s->mode,s->irq_pending,s->require_ack,
            s->driver_half,s->saved_next_half,s->driver_state,(int64_t)s->dma_reads,(int64_t)s->block_ends};
        for(size_t i=0;i<37;i++)if(state[i]!=v->state[i]) {
            fprintf(stderr,"ms%zu state%zu %lld != %lld\n",ms+1,i,(long long)state[i],(long long)v->state[i]);CHECK(0);
        }
        CHECK(audio->original_dc.adjustment[0]==v->dc[0] && audio->original_dc.adjustment[1]==v->dc[1]);
        CHECK(words_fnv(&audio->original_sb.window[0][0],4096)==v->window);
        CHECK(words_fnv(g->build,2048)==v->build);
        CHECK(bytes_fnv(&g->dma[0][0],4096)==v->dma);
    }
    CHECK(fm_frames==5280 && host_reads==0 && audio->first_error==0);
    CHECK(shutdown_driver(audio,&d));fd2_pcm_player_close(&p);fd2_pcm_player_close(&q);fd2_pcm_bank_close(&bank);
    return 1;
}
static int source_partial_fill_and_no_prefix_fill(void) {
    CHECK(reset_test());fd2_audio *audio=make_original(1,0);CHECK(audio);
    source fm={.value=.25f};CHECK(fd2_audio_play_music_source(audio,make_source(&fm),1)==0);
    host_ns=500000;CHECK(fd2_audio_sync(audio)==0 && fm.frames==0 && device.puts==0);
    CHECK(fd2_audio_original_fill_up(audio)==0 && fm.frames==24 && device.success_frames==24);
    CHECK(clock_last_ns==500000 && audio->edge_ns==500000);
    fm.value=.5f;
    CHECK(fd2_audio_original_fill_up(audio)==0 && fm.frames==24);
    host_ns=1000000;CHECK(fd2_audio_sync(audio)==0 && fm.frames==48 && device.success_frames==48);
    for(size_t i=0;i<96;i++)CHECK(device.output[i]==original_partial_pcm[i]/32768.0f);
    CHECK(audio->original_dc.adjustment[0]==original_partial_dc[0]);
    CHECK(audio->original_dc.adjustment[1]==original_partial_dc[1]);
    CHECK(fd2_audio_quiesce_dig(audio)==0);CHECK(fd2_bios_clock_close()==0);fd2_audio_destroy(audio);
    CHECK(fm.retired==1);return 1;
}
static int given_rational_control_whole48(void) {
    CHECK(reset_test());fd2_audio *audio=make_original(1,0);CHECK(audio);
    fd2_pcm_driver d={0};CHECK(prepare_driver(audio,&d));
    host_ns=1000000;
    /* Explicit low-level preIRQ point just before1ms; float source index rounds1.
     * This is a synthetic rational input, not an observed normal IRQ phase. */
    audio_prefix(audio,999999,FD2_BIOS_PIT_RATE-1u);
    CHECK(audio->original_rendered==0 && audio->original_ticks==0);
    const uint8_t sample[]={128,255};
    CHECK(fd2_audio_dig_start(&d.dig,0,sample,2,1)==0);
    CHECK(audio->original_rendered==48 && audio->original_sb.channel.rendered==0);
    CHECK(audio->original_sb.dma.dma_reads==0 && audio->closed_frames==48);
    fd2_sb_channel before=audio->original_sb.channel;
    CHECK(fd2_audio_sync(audio)==0 && audio->original_ticks==1 && audio->original_rendered==0);
    CHECK(memcmp(&before,&audio->original_sb.channel,sizeof(before))==0);
    CHECK(device.success_frames==48 && audio->first_error==0);
    CHECK(shutdown_driver(audio,&d));return 1;
}
static int original_offline_atomic_and_legacy(void) {
    CHECK(reset_test());fd2_audio_config bad={44100,1,1,1};
    CHECK(fd2_audio_create(&bad)==NULL && device.opens==0);
    fd2_audio *audio=make_original(0,1);CHECK(audio);
    fd2_audio before=*audio;float output[96];memset(output,0x79,sizeof(output));
    CHECK(fd2_audio_render_offline(audio,output,1)==0 && memcmp(&before,audio,sizeof(before))==0);
    CHECK(clock_last_ns==0 && output[0]!=0);
    CHECK(fd2_audio_play_source(audio,FD2_AUDIO_BUS_SFX,(fd2_audio_source){source_render,NULL,NULL},1)<0);
    CHECK(fd2_audio_set_bus_gain(audio,FD2_AUDIO_BUS_MUSIC,.5f)<0);
    CHECK(fd2_audio_quiesce_dig(audio)==0);CHECK(fd2_bios_clock_close()==0);fd2_audio_destroy(audio);
    CHECK(reset_test());audio=make_audio(48000,0,1);CHECK(audio && !fd2_audio_is_original_mixer(audio));
    source fm={.value=.25f};CHECK(fd2_audio_play_music_source(audio,make_source(&fm),1)==0);
    CHECK(fd2_audio_render_offline(audio,output,1)==1 && output[0]==.25f && fm.frames==1);
    fd2_audio_destroy(audio);return 1;
}
static int output_error_null_and_quiet_keep_logic(void) {
    CHECK(reset_test());device.fail_open=1;fd2_audio *audio=make_original(1,1);CHECK(audio);
    source fm={.value=.25f};CHECK(fd2_audio_play_music_source(audio,make_source(&fm),1)==0);
    host_ns=2000000;CHECK(fd2_audio_sync(audio)==0 && fm.frames==96 && device.puts==0);
    CHECK(audio->first_error==FD2_AUDIO_ERROR_OPEN);
    CHECK(fd2_audio_quiesce_dig(audio)==0);CHECK(fd2_bios_clock_close()==0);fd2_audio_destroy(audio);
    CHECK(reset_test());env_quiet="1";audio=make_original(1,0);CHECK(audio);
    fm=(source){.value=.25f};CHECK(fd2_audio_play_music_source(audio,make_source(&fm),1)==0);
    device.fail_put=1;host_ns=1000000;CHECK(fd2_audio_sync(audio)==0 && fm.frames==48);
    host_ns=2000000;CHECK(fd2_audio_sync(audio)==0 && fm.frames==96 && device.success_frames==48);
    CHECK(audio->first_error==FD2_AUDIO_ERROR_PUT && audio->mix_buffer[0]!=0);
    for(size_t i=0;i<96;i++)CHECK(device.output[i]==0);
    CHECK(fd2_audio_quiesce_dig(audio)==0);CHECK(fd2_bios_clock_close()==0);fd2_audio_destroy(audio);
    CHECK(device.invalid==0 && fm.retired==1);return 1;
}
static int replacement_prefill_before_unregister(void) {
    CHECK(reset_test());fd2_audio *audio=make_original(1,0);CHECK(audio);
    source old={.value=.25f,.reenter=audio}, next={.value=.5f};
    CHECK(fd2_audio_play_music_source(audio,make_source(&old),1)==0);
    host_ns=500000;
    CHECK(fd2_audio_play_music_source(audio,make_source(&next),1)==0);
    CHECK(old.frames==24 && old.retired==1 && old.reenter_result==-1 && next.frames==0);
    host_ns=1000000;CHECK(fd2_audio_sync(audio)==0 && next.frames==24);
    for(size_t i=0;i<96;i++)CHECK(device.output[i]==original_partial_pcm[i]/32768.0f);
    CHECK(audio->first_error==0 && audio->original_dc.adjustment[0]==original_partial_dc[0]);
    next.reenter=audio;
    CHECK(fd2_audio_quiesce_dig(audio)==0);CHECK(fd2_bios_clock_close()==0);fd2_audio_destroy(audio);
    CHECK(next.frames==24 && next.retired==1 && next.reenter_result==-1 && device.success_frames==48);
    return 1;
}
static int failed_DIG_install_does_not_quiesce_music(void) {
    CHECK(reset_test());fd2_audio *audio=make_original(1,0);CHECK(audio);
    CHECK(fd2_bios_clock_ail_attach(0,65536)==0);
    for(unsigned i=0;i<15;i++)CHECK(fd2_bios_clock_install_mdi(0,idle_mdi,NULL)!=FD2_AIL_TIMER_INVALID);
    source fm={.value=.25f};CHECK(fd2_audio_play_music_source(audio,make_source(&fm),1)==0);
    fd2_pcm_driver d={0};CHECK(fd2_pcm_driver_init(&d,audio)<0);
    CHECK(d.normal_bound && d.audio==audio && d.dig.closed && !d.dig.attached);
    CHECK(!audio->original_quiesced && audio->original_sb.dma.mode==FD2_SB_NONE);
    host_ns=2000000;CHECK(fd2_audio_sync(audio)==0 && fm.frames==96 && device.success_frames==96);
    CHECK(audio->first_error==0);CHECK(shutdown_driver(audio,&d));CHECK(fm.retired==1);
    return 1;
}

/* FA03：给定合法PAUSED/left1..2与pending IRQ前态，不声称正常SAV或实机可达。
 * 固定b6abbd5 sblaster.cpp1130..1147/2394..2399/3728..3731/3876..3879，
 * PIC_AddEvent664..672；literal由独立Fraction converter按host算式导出。
 * 全ring=128、FM=0、DC=0，因此全部已发布PCM严格为0；不是H静音替代。 */
static const uint8_t remaining_end_source[2]={0,255};
static int given_paused_remaining(fd2_audio *audio,fd2_pcm_driver *d,source *fm,
                                  uint64_t anchor,unsigned left) {
    CHECK(prepare_driver(audio,d));
    CHECK(fd2_audio_play_music_source(audio,make_source(fm),1)==0);
    host_ns=anchor;CHECK(fd2_audio_sync(audio)==0);
    CHECK(audio->closed_frames==0 && audio->original_rendered==0);
    fd2_sb_dma_provider *sb=&audio->original_sb.dma;
    /* source-given: old block has 1/2 bytes, saved half1 resumes as1; D0已暂停。 */
    sb->ring=&d->dig.dma[0][0];sb->cursor=FD2_SB_BLOCK-left;
    sb->left=left;sb->total=FD2_SB_BLOCK;sb->mode=FD2_SB_PAUSED;
    sb->driver_half=-1;sb->saved_next_half=2;sb->driver_state=0;
    CHECK(fd2_audio_dig_start(&d->dig,0,remaining_end_source,2,1)==0);
    CHECK(sb->mode==FD2_SB_DMA && sb->driver_half==1 && sb->driver_state==3);
    CHECK(sb->saved_next_half==0 && sb->end.pending && sb->end.serial==1);
    CHECK(sb->end.count==left && sb->dma_reads==0 && sb->block_ends==0);
    CHECK(!sb->irq_pending && audio->first_error==FD2_AUDIO_ERROR_NONE);
    return 1;
}
static int zero_published_audio(const fd2_audio *audio,const source *fm,size_t frames) {
    CHECK(audio->closed_frames==frames && device.success_frames==frames && fm->frames==frames);
    for(size_t i=0;i<frames*2u;i++)CHECK(device.output[i]==0.0f);
    CHECK(device.invalid==0);return 1;
}
static int original_positive_remaining_END_and_tick(void) {
    /* event delay F32: 1000/22050=0x1.7384fp-5, twice=...p-4.
     * after_ms四元组=reads,left,window_written,phase；独立model.json保全推导。 */
    static const struct {
        uint64_t anchor,before,due;unsigned left,prefill,reads,remaining,written,phase;
    } rows[]={
        {500000,545351,545352,1,24,14,2035,5,63450},
        {500000,590702,590703,2,24,14,2036,5,63450},
        {980000,1025351,1025352,1,47,3,2046,4,62250},
        {980000,1070702,1070703,2,47,2,2048,2,66150}
    };
    for(size_t r=0;r<sizeof(rows)/sizeof(rows[0]);r++) {
        CHECK(reset_test());fd2_audio *audio=make_original(1,0);CHECK(audio);
        fd2_pcm_driver d={0};source fm={0};
        CHECK(given_paused_remaining(audio,&d,&fm,rows[r].anchor,rows[r].left));
        fd2_sb_dma_provider *sb=&audio->original_sb.dma;
        const float delay=rows[r].left==1?0x1.7384fp-5f:0x1.7384fp-4f;
        CHECK(sb->end.delay_ms==delay && sb->end.tick==0);
        CHECK(sb->end.index==(double)rows[r].anchor/1000000.0+(double)delay);
        CHECK(zero_published_audio(audio,&fm,rows[r].prefill));
        fd2_audio_dig d_before=d.dig;
        if(rows[r].anchor==500000) {
            host_ns=rows[r].before;CHECK(fd2_audio_sync(audio)==0);
            CHECK(sb->end.pending && sb->dma_reads==0 && sb->block_ends==0);
            CHECK(zero_published_audio(audio,&fm,24));
            host_ns=rows[r].due;CHECK(fd2_audio_sync(audio)==0);
            CHECK(!sb->end.pending && sb->dma_reads==rows[r].left && sb->left==2048);
            CHECK(sb->block_ends==1 && !sb->irq_pending && sb->driver_half==0);
            CHECK(sb->cursor==2048 && sb->next_serial==1);
            CHECK(audio->original_sb.channel.written==(rows[r].left==1?24u:27u));
            CHECK(audio->original_sb.channel.freq_f==(rows[r].left==1?48000u:66150u));
            CHECK(zero_published_audio(audio,&fm,24));
            CHECK(fd2_audio_sync(audio)==0 && sb->dma_reads==rows[r].left);
        }
        host_ns=1000000;CHECK(fd2_audio_sync(audio)==0);
        CHECK(!sb->end.pending && sb->next_serial==1 && sb->block_ends==1);
        CHECK(sb->dma_reads==rows[r].reads && sb->left==rows[r].remaining);
        CHECK(sb->cursor==2048u-rows[r].left+rows[r].reads);
        CHECK(!sb->irq_pending && sb->driver_half==0 && sb->mode==FD2_SB_DMA);
        CHECK(audio->original_sb.channel.written==rows[r].written);
        CHECK(audio->original_sb.channel.freq_f==rows[r].phase);
        CHECK(audio->original_sb.channel.read==0 && audio->original_sb.channel.rendered==0);
        CHECK(zero_published_audio(audio,&fm,48));
        /* 旧END在下一Mix/块末已取消；越过原到期点不得再读count或再翻half。 */
        host_ns=1100000;CHECK(fd2_audio_sync(audio)==0);
        CHECK(sb->dma_reads==rows[r].reads && sb->block_ends==1 && sb->driver_half==0);
        CHECK(!sb->end.pending && !sb->irq_pending && sb->next_serial==1);
        CHECK(memcmp(&d.dig,&d_before,sizeof(d_before))==0);
        CHECK(audio->first_error==0 && fd2_bios_clock_ticks(host_ns)==0);
        CHECK(zero_published_audio(audio,&fm,48));CHECK(shutdown_driver(audio,&d));
        CHECK(fm.retired==1);
    }return 1;
}
static int original_remaining_pause_resume_cancels_old_END(void) {
    static const uint64_t old_due[2]={545352,590703},new_due[2]={575352,620703};
    for(unsigned left=1;left<=2;left++) {
        CHECK(reset_test());fd2_audio *audio=make_original(1,0);CHECK(audio);
        fd2_pcm_driver d={0};source fm={0};
        CHECK(given_paused_remaining(audio,&d,&fm,500000,left));
        fd2_sb_dma_provider *sb=&audio->original_sb.dma;
        /* 真实第三空服务调用request_pause；pending IRQ是给定外部CPU交付前提。 */
        CHECK(fd2_audio_dig_stop(&d.dig,0)==0);d.dig.empty_services=2;
        CHECK(fd2_audio_dig_service(&d.dig)==1);
        CHECK(!d.dig.playing && sb->driver_state==2 && sb->end.pending);
        host_ns=520000;CHECK(fd2_audio_sync(audio)==0);sb->irq_pending=1;
        CHECK(fd2_audio_sb_deliver_irq(&audio->original_sb)==0);
        CHECK(sb->mode==FD2_SB_PAUSED && sb->driver_state==0 && sb->driver_half==-1);
        CHECK(sb->saved_next_half==1 && !sb->end.pending && !sb->irq_pending);
        CHECK(sb->left==left && sb->dma_reads==0 && sb->block_ends==0);
        host_ns=530000;CHECK(fd2_audio_sync(audio)==0);
        CHECK(fd2_audio_dig_start(&d.dig,0,remaining_end_source,2,1)==0);
        CHECK(sb->mode==FD2_SB_DMA && sb->driver_state==3 && sb->driver_half==0);
        CHECK(sb->end.pending && sb->end.serial==2 && sb->next_serial==2);
        CHECK(sb->end.count==left && sb->left==left && sb->dma_reads==0);
        CHECK(zero_published_audio(audio,&fm,25));
        host_ns=old_due[left-1];CHECK(fd2_audio_sync(audio)==0);
        CHECK(sb->end.pending && sb->end.serial==2 && sb->dma_reads==0);
        host_ns=new_due[left-1]-1u;CHECK(fd2_audio_sync(audio)==0);
        CHECK(sb->end.pending && sb->dma_reads==0 && sb->block_ends==0);
        host_ns=new_due[left-1];CHECK(fd2_audio_sync(audio)==0);
        CHECK(!sb->end.pending && sb->dma_reads==left && sb->block_ends==1);
        CHECK(sb->left==2048 && !sb->irq_pending && sb->driver_half==1);
        CHECK(sb->next_serial==2 && fd2_audio_sync(audio)==0 && sb->dma_reads==left);
        CHECK(zero_published_audio(audio,&fm,25));
        CHECK(audio->first_error==0);CHECK(shutdown_driver(audio,&d));CHECK(fm.retired==1);
    }return 1;
}
static int original_bad_END_keeps_reached_audio_prefix(void) {
    for(unsigned invalid=0;invalid<2;invalid++) {
        CHECK(reset_test());fd2_audio *audio=make_original(1,0);CHECK(audio);
        fd2_pcm_driver d={0};source fm={0};
        CHECK(given_paused_remaining(audio,&d,&fm,500000,1));
        fd2_sb_dma_provider *sb=&audio->original_sb.dma;
        fd2_audio_dig d_before=d.dig;
        fd2_sb_channel channel_before=audio->original_sb.channel;
        fd2_mixdown_dc dc_before=audio->original_dc;
        if(invalid==0)sb->end.index=NAN;
        else {sb->end.tick=UINT64_MAX;sb->end.index=1.0;}
        host_ns=600000;CHECK(fd2_audio_sync(audio)==0);
        CHECK(audio->first_error==FD2_AUDIO_ERROR_SOURCE && !sb->end.pending);
        CHECK(sb->dma_reads==0 && sb->block_ends==0 && sb->left==1 && sb->driver_half==1);
        CHECK(sb->mode==FD2_SB_DMA && !sb->irq_pending && sb->first_error==0);
        CHECK(memcmp(&d.dig,&d_before,sizeof(d_before))==0);
        CHECK(memcmp(&audio->original_sb.channel,&channel_before,sizeof(channel_before))==0);
        CHECK(memcmp(&audio->original_dc,&dc_before,sizeof(dc_before))==0);
        CHECK(zero_published_audio(audio,&fm,24));
        CHECK(fd2_audio_sync(audio)==0 && sb->dma_reads==0);
        /* 有界错误只锁首错；下一合法tick继续其已定义Mix，不能冻结BDA/源或追补END。 */
        host_ns=1000000;CHECK(fd2_audio_sync(audio)==0);
        CHECK(audio->first_error==FD2_AUDIO_ERROR_SOURCE && sb->first_error==0);
        CHECK(sb->dma_reads==14 && sb->block_ends==1 && !sb->end.pending && !sb->irq_pending);
        CHECK(zero_published_audio(audio,&fm,48));CHECK(shutdown_driver(audio,&d));CHECK(fm.retired==1);
    }return 1;
}


/* 仅观察实际submit分支；独立literal输入0/1000/192000，不从WAV推统计。
 * 三次success/失败边界与owner无副作用，是宿主H传输合同，非原设备时序。 */
static int transport_stats_are_lifetime_owner_readonly(void) {
    CHECK(reset_test());env_quiet="1";fd2_audio *audio=make_original(1,0);CHECK(audio);
    source fm={.value=.25f};CHECK(fd2_audio_play_music_source(audio,make_source(&fm),1)==0);
    host_ns=1000000;CHECK(fd2_audio_sync(audio)==0);
    device.queued=1000;host_ns=2000000;CHECK(fd2_audio_sync(audio)==0);
    device.queued=192000;host_ns=3000000;CHECK(fd2_audio_sync(audio)==0);
    device.queued=0;device.fail_put=device.puts+1u;
    host_ns=4000000;CHECK(fd2_audio_sync(audio)==0 && fm.frames==192);
    fd2_audio_transport_stats stats;
    CHECK(fd2_audio_get_transport_stats(audio,&stats)==0);
    CHECK(stats.first_error==FD2_AUDIO_ERROR_BACKLOG && stats.queue_samples==4);
    CHECK(stats.queue_peak_bytes==192000 && stats.successful_puts==2 && stats.backlog_drop_chunks==1);
    CHECK(device.puts==3 && device.queue_queries==4); /* failedPut不是success，也不是backlog。 */
    CHECK(device.output[0]==0.0f && audio->mix_buffer[0]!=0.0f); /* quiet成功Put也计数。 */
    CHECK(fd2_audio_quiesce_dig(audio)==0 && fd2_bios_clock_close()==0);
    fd2_audio snapshot;memcpy(&snapshot,audio,sizeof(snapshot));
    unsigned reads=host_reads,queries=device.queue_queries;
    CHECK(fd2_audio_get_transport_stats(audio,&stats)==0);
    CHECK(memcmp(&snapshot,audio,sizeof(snapshot))==0 && reads==host_reads && queries==device.queue_queries);
    fd2_audio_transport_stats untouched;memset(&untouched,0xa5,sizeof(untouched));
    fd2_audio_transport_stats saved;memcpy(&saved,&untouched,sizeof(saved));
    CHECK(fd2_audio_get_transport_stats(NULL,&untouched)==-1 && !memcmp(&saved,&untouched,sizeof(saved)));
    CHECK(fd2_audio_get_transport_stats(audio,NULL)==-1);
    host_tid=2;CHECK(fd2_audio_get_transport_stats(audio,&untouched)==-1);
    CHECK(!memcmp(&saved,&untouched,sizeof(saved)));host_tid=1;
    CHECK(!memcmp(&snapshot,audio,sizeof(snapshot)) && reads==host_reads && queries==device.queue_queries);
    fd2_audio_destroy(audio);CHECK(fm.retired==1);
    /* 负GetQueued不计有效sample；Put失败只记实际query，首错仍可读。 */
    for(unsigned failure=0;failure<2;failure++) {
        CHECK(reset_test());audio=make_original(1,0);CHECK(audio);
        fm=(source){.value=.25f};CHECK(fd2_audio_play_music_source(audio,make_source(&fm),1)==0);
        if(failure==0)device.fail_query=1;else device.fail_put=1;
        host_ns=1000000;CHECK(fd2_audio_sync(audio)==0 && fm.frames==48);
        CHECK(fd2_audio_get_transport_stats(audio,&stats)==0);
        CHECK(stats.first_error==FD2_AUDIO_ERROR_PUT && stats.queue_samples==failure);
        CHECK(stats.queue_peak_bytes==0 && stats.successful_puts==0 && stats.backlog_drop_chunks==0);
        CHECK(fd2_audio_quiesce_dig(audio)==0 && fd2_bios_clock_close()==0);fd2_audio_destroy(audio);
    }
    CHECK(reset_test());device.fail_open=1;audio=make_original(1,1);CHECK(audio);
    host_ns=1000000;CHECK(fd2_audio_sync(audio)==0);
    CHECK(fd2_audio_get_transport_stats(audio,&stats)==0 && stats.first_error==FD2_AUDIO_ERROR_OPEN);
    CHECK(!stats.queue_samples && !stats.queue_peak_bytes && !stats.successful_puts && !stats.backlog_drop_chunks);
    CHECK(fd2_audio_quiesce_dig(audio)==0 && fd2_bios_clock_close()==0);fd2_audio_destroy(audio);
    CHECK(reset_test());audio=make_original(0,1);CHECK(audio);float offline[96];
    CHECK(fd2_audio_render_offline(audio,offline,48)==48);
    CHECK(fd2_audio_get_transport_stats(audio,&stats)==0 && stats.first_error==0);
    CHECK(!stats.queue_samples && !stats.queue_peak_bytes && !stats.successful_puts && !stats.backlog_drop_chunks);
    CHECK(fd2_audio_quiesce_dig(audio)==0 && fd2_bios_clock_close()==0);fd2_audio_destroy(audio);
    return 1;
}

int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--transport-stats")) {
        if(!transport_stats_are_lifetime_owner_readonly())return 1;
        puts("audio_original: transport stats owner/submit branches PASS; no device");return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--remaining-end")) {
        if(!original_positive_remaining_END_and_tick() ||
           !original_remaining_pause_resume_cancels_old_END() ||
           !original_bad_END_keeps_reached_audio_prefix())return 1;
        puts("audio_original: 3 given-state remaining-END families PASS; no device or guest");return 0;
    }
    if(argc!=1)return 2;
    if(!original_shared_clock_tape() || !source_partial_fill_and_no_prefix_fill() ||
       !given_rational_control_whole48() || !original_offline_atomic_and_legacy() ||
       !output_error_null_and_quiet_keep_logic() || !replacement_prefill_before_unregister() ||
       !failed_DIG_install_does_not_quiesce_music() ||
       !original_positive_remaining_END_and_tick() ||
       !original_remaining_pause_resume_cancels_old_END() ||
       !original_bad_END_keeps_reached_audio_prefix() ||
       !transport_stats_are_lifetime_owner_readonly())return 1;
    puts("audio_original: 11 finite owner/clock/source families PASS; no device or original CPU");return 0;
}
