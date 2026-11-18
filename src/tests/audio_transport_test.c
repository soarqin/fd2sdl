/* F5主线程transport：真实bios/AIL整数核心；SDL设备/时钟/observer仅替身。
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

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __func__, __LINE__, #x); return 0; } } while (0)
static struct {
    unsigned opens, resumes, pauses, destroys, puts, push, put_failed, dropped, closed;
    unsigned invalid, fail_put, order_count;
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
    fd2_audio_config config = { rate, live, allow }; return fd2_audio_create(&config);
}
static int test_exact_fraction(void) {
    CHECK(reset_test());
    fd2_audio *audio = make_audio(44100, 0, 1); CHECK(audio);
    uint64_t count = 0;
    CHECK(prefix_frames(audio, UINT64_C(24050362811), 445398u, &count) == 0 && count == UINT64_C(1060621));
    CHECK(prefix_frames(audio, UINT64_C(24050362812), 0, &count) == 0 && count == UINT64_C(1060622));
    audio->sample_rate = 48000;
    CHECK(prefix_frames(audio, UINT64_C(574503666666), 128788u, &count) == 0 && count == UINT64_C(27576176));
    CHECK(prefix_frames(audio, UINT64_C(574503666667), 0, &count) == 0 && count == UINT64_C(27576177));
    CHECK(prefix_frames(audio, UINT64_MAX, FD2_BIOS_PIT_RATE - 1u, &count) == 0 && count == UINT64_C(885443715538059));
    CHECK(prefix_frames(audio, 1, FD2_BIOS_PIT_RATE, &count) == -1);
    fd2_audio_destroy(audio); return 1;
}
static int test_closed_prefix_and_future(void) {
    CHECK(reset_test()); fd2_audio *audio = make_audio(48000, 1, 0); CHECK(audio);
    source s = { .value = .25f }; CHECK(fd2_audio_play_music_source(audio, make_source(&s), 1) == 0);
    CHECK(s.frames == 0 && device.puts == 0 && audio->closed_frames == 0);
    CHECK(fd2_bios_clock_advance(1000000) == -1 && s.frames == 0); /* 未到的live输入拒绝。 */
    (void)fd2_bios_clock_ticks(UINT64_MAX); CHECK(s.frames == 0);
    host_ns = 62500; CHECK(fd2_audio_sync(audio) == 0 && s.frames == 3);
    CHECK(device.success_frames == 3 && device.push == 0);
    CHECK(fd2_audio_sync(audio) == 0 && s.frames == 3); /* 同刻不重复sample。 */
    host_ns = 62501; CHECK(fd2_audio_sync(audio) == 0 && s.frames == 4);
    CHECK(fd2_audio_now_ns(audio) == host_ns && device.invalid == 0);
    fd2_audio_destroy(audio); CHECK(clock_audio_prefix == NULL && clock_audio_user == NULL); return 1;
}
static int test_fraction_at_sample_boundary(void) {
    CHECK(reset_test()); fd2_audio *audio = make_audio(48000, 0, 1); CHECK(audio);
    source s = { .value = .125f }; CHECK(fd2_audio_play_music_source(audio, make_source(&s), 1) == 0);
    audio_prefix(audio, 62500, 0); CHECK(s.frames == 3);
    audio_prefix(audio, 62500, 1); CHECK(s.frames == 4);
    audio_prefix(audio, 62500, 1); CHECK(s.frames == 4);
    fd2_audio_destroy(audio); return 1;
}
static int test_sync_command_order(void) {
    CHECK(reset_test()); fd2_audio *audio = make_audio(1000, 1, 0); CHECK(audio);
    source a = { .value = .125f }, b = { .value = .5f };
    CHECK(fd2_audio_play_music_source(audio, make_source(&a), 1) == 0);
    host_ns = 1000000; CHECK(fd2_audio_play_music_source(audio, make_source(&b), 1) == 0);
    CHECK(a.frames == 1 && a.retired == 1 && a.reason == FD2_AUDIO_RETIRE_REPLACED && b.frames == 0);
    host_ns = 2000000; CHECK(fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_MUSIC, .5f) == 0);
    host_ns = 3000000; CHECK(fd2_audio_set_master_gain(audio, .5f) == 0);
    host_ns = 4000000; CHECK(fd2_audio_stop_music(audio) == 0);
    CHECK(b.frames == 3 && b.retired == 1 && b.reason == FD2_AUDIO_RETIRE_STOPPED);
    const float expected[] = { .125f, .5f, .25f, .125f };
    CHECK(device.success_frames == 4);
    for (size_t i = 0; i < 4; i++) CHECK(device.output[i*2] == expected[i] && device.output[i*2+1] == expected[i]);
    for (unsigned i = 0; i < 200; i++) CHECK(fd2_audio_set_master_gain(audio, (float)(i & 1u)) == 0);
    CHECK(device.success_frames == 4); /* 无64命令容量、无延迟队列。 */
    fd2_audio_destroy(audio); return 1;
}
static int test_owner_and_invalid_no_effect(void) {
    CHECK(reset_test()); fd2_audio *audio = make_audio(48000, 1, 0); CHECK(audio);
    source s = { .value = .25f };
    CHECK(make_audio(48000, 0, 1) == NULL); /* 不得reset旧owner。 */
    CHECK(fd2_audio_play_music_source(audio, make_source(&s), 1) == 0);
    host_ns = 1000000; host_tid = 2;
    CHECK(fd2_audio_sync(audio) == -1 && fd2_audio_stop_music(audio) == -1 && s.frames == 0);
    fd2_audio_destroy(audio); CHECK(device.destroys == 0 && active_audio == audio);
    host_tid = 1;
    CHECK(fd2_audio_set_master_gain(audio, NAN) == -1 && s.frames == 0);
    CHECK(fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_COUNT, 0) == -1 && s.frames == 0);
    float gain = 9; CHECK(fd2_audio_get_bus_gain_target(audio, FD2_AUDIO_BUS_COUNT, &gain) == -1 && gain == 9);
    CHECK(fd2_audio_sync(audio) == 0 && s.frames == 48);
    fd2_audio_destroy(audio);
    CHECK(make_audio(1000000001, 0, 1) == NULL && active_audio == NULL);
    return 1;
}
static int test_offline_same_clock_no_wall(void) {
    CHECK(reset_test()); fd2_audio *audio = make_audio(44100, 0, 1); CHECK(audio);
    source s = { .value = .25f }; CHECK(fd2_audio_play_music_source(audio, make_source(&s), 1) == 0);
    host_ns = UINT64_MAX; host_reads = 0;
    float output[2];
    for (unsigned i = 0; i < 1000; i++) {
        CHECK(fd2_audio_render_offline(audio, output, 1) == 1);
        CHECK(output[0] == .25f && output[1] == .25f);
    }
    CHECK(s.frames == 1000 && host_reads == 0 && fd2_audio_now_ns(audio) == UINT64_C(22675736));
    CHECK(clock_last_ns == UINT64_C(22675736) && device.opens == 0);
    fd2_audio_destroy(audio); CHECK(active_audio == NULL);
    audio = make_audio(48000, 0, 1); CHECK(audio && fd2_audio_now_ns(audio) == 0 && clock_last_ns == 0);
    fd2_audio_destroy(audio); return 1;
}
static void irq_count(void *user) { (*(unsigned *)user)++; }
static int test_null_logic_and_irq(void) {
    CHECK(reset_test()); device.fail_open = 1;
    fd2_audio *audio = make_audio(48000, 1, 1); CHECK(audio && !fd2_audio_has_device(audio));
    source s = { .value = .25f }; CHECK(fd2_audio_play_music_source(audio, make_source(&s), 1) == 0 && s.retired == 0);
    unsigned midi = 0, digital = 0;
    CHECK(fd2_bios_clock_ail_attach(0, 65536) == 0);
    CHECK(fd2_bios_clock_install_mdi(0, irq_count, &midi) == 0);
    CHECK(fd2_bios_clock_install_dig(0, irq_count, &digital) ==FD2_AIL_TIMER_DIG);
    host_ns = 100000000; CHECK(fd2_audio_sync(audio) == 0);
    /* Fraction(100ms*1193182/(5966*1e9)) floor=19 IRQ；floor(19*5000/8333)=11。 */
    CHECK(s.frames == 4800 && midi == 11 && digital == 19 && clock_bda == 1 && device.puts == 0);
    CHECK(fd2_audio_stop_music(audio) == 0 && s.retired == 1 && s.reason == FD2_AUDIO_RETIRE_STOPPED);
    fd2_audio_error error; CHECK(fd2_audio_get_first_error(audio, &error) == 0 && error == FD2_AUDIO_ERROR_OPEN);
    fd2_audio_destroy(audio);
    CHECK(reset_test()); device.fail_resume = 1;
    audio = make_audio(48000, 1, 1); CHECK(audio && device.destroys == 1);
    CHECK(fd2_audio_get_first_error(audio, &error) == 0 && error == FD2_AUDIO_ERROR_RESUME);
    fd2_audio_destroy(audio);
    CHECK(reset_test()); device.fail_open = 1; CHECK(make_audio(48000, 1, 0) == NULL && active_audio == NULL);
    return 1;
}
static int test_put_prefix_capture_and_close(void) {
    CHECK(reset_test()); env_capture = "test-observer";
    fd2_audio *audio = make_audio(48000, 1, 0); CHECK(audio);
    source s = { .value = .25f }; CHECK(fd2_audio_play_music_source(audio, make_source(&s), 1) == 0);
    device.fail_put = 2;
    host_ns = 20000000; CHECK(fd2_audio_sync(audio) == 0);
    CHECK(s.frames == 960 && device.puts == 2 && device.push == 1 && device.put_failed == 1 && device.success_frames == 512);
    CHECK(device.order[0] == 'U' && device.order[1] == 'C' && device.order[2] == 'F');
    device.fail_query = 1; host_ns = 21000000; CHECK(fd2_audio_sync(audio) == 0 && s.frames == 1008);
    fd2_audio_error error; CHECK(fd2_audio_get_first_error(audio, &error) == 0 && error == FD2_AUDIO_ERROR_PUT);
    unsigned midi = 0, digital = 0;
    CHECK(fd2_bios_clock_ail_attach(host_ns, 65536) == 0);
    CHECK(fd2_bios_clock_install_mdi(host_ns, irq_count, &midi) == 0);
    CHECK(fd2_bios_clock_install_dig(host_ns, irq_count, &digital) ==FD2_AIL_TIMER_DIG);
    host_ns = 100000000; CHECK(fd2_audio_sync(audio) == 0);
    /* 仅21ms之后挂接：79ms含15IRQ，floor(15*5000/8333)=9。 */
    CHECK(s.frames == 4800 && midi == 9 && digital == 15 && clock_bda == 1);
    device.order_count = 0; fd2_audio_destroy(audio);
    CHECK(device.order_count == 4 && !memcmp(device.order, "PDRX", 4));
    CHECK(s.retired == 1 && s.reason == FD2_AUDIO_RETIRE_DESTROYED && device.invalid == 0);
    CHECK(clock_audio_prefix == NULL && clock_audio_user == NULL && device.closed == 1);
    host_ns++; CHECK(fd2_bios_clock_advance(host_ns) == 0); /* 无悬空Audio callback。 */
    return 1;
}
static int test_queue_bound_continues_logic(void) {
    CHECK(reset_test()); env_capture = "test-observer";
    fd2_audio *audio = make_audio(1000, 1, 0); CHECK(audio);
    source s = { .value = .25f }; CHECK(fd2_audio_play_music_source(audio, make_source(&s), 1) == 0);
    host_ns = 1000000000; CHECK(fd2_audio_sync(audio) == 0);
    CHECK(s.frames == 1000 && device.success_frames == 500 && device.queued == 4000 && device.dropped == 1);
    fd2_audio_error error; CHECK(fd2_audio_get_first_error(audio, &error) == 0 && error == FD2_AUDIO_ERROR_BACKLOG);
    device.queued = 0; host_ns = 1001000000; CHECK(fd2_audio_sync(audio) == 0);
    CHECK(s.frames == 1001 && device.success_frames == 501 && device.queued == 8); /* 只接新帧、不追补。 */
    CHECK(fd2_audio_get_first_error(audio, &error) == 0 && error == FD2_AUDIO_ERROR_BACKLOG);
    fd2_audio_destroy(audio); return 1;
}
static int test_quiet_and_reentry(void) {
    CHECK(reset_test()); env_quiet = "1"; env_capture = "test-observer";
    fd2_audio *audio = make_audio(48000, 1, 0); CHECK(audio);
    source s = { .value = .25f, .reenter = audio };
    CHECK(fd2_audio_play_music_source(audio, make_source(&s), 1) == 0);
    host_ns = 1000000; CHECK(fd2_audio_sync(audio) == 0);
    CHECK(s.frames == 48 && s.reenter_result == -1 && device.push == 1);
    for (size_t i = 0; i < 96; i++) CHECK(device.output[i] == 0);
    CHECK(audio->mix_buffer[0] == .25f && audio->master_gain == 1);
    fd2_audio_destroy(audio); CHECK(s.retired == 1 && s.reenter_result == -1);
    return 1;
}
static void step_source(void *user) { ((source *)user)->value += .03125f; }
static int test_irq_prefix_order(void) {
    CHECK(reset_test()); fd2_audio *audio = make_audio(48000, 0, 1); CHECK(audio);
    source s = {0}; CHECK(fd2_audio_play_music_source(audio, make_source(&s), 1) == 0);
    CHECK(fd2_bios_clock_ail_attach(0, 65536) == 0);
    CHECK(fd2_bios_clock_install_dig(0, step_source, &s) == FD2_AIL_TIMER_DIG);
    float output[4800 * 2]; host_reads = 0;
    CHECK(fd2_audio_render_offline(audio, output, 4800) == 4800 && s.frames == 4800 && host_reads == 0);
    for (uint64_t i = 0; i < 4800; i++) {
        /* sample i之前/同刻已有floor((i/48000)/(5966/1193182))次IRQ。 */
        uint64_t prior = i * UINT64_C(1193182) / (UINT64_C(48000) * 5966u);
        float expected = (float)prior * .03125f;
        CHECK(output[i*2] == expected && output[i*2+1] == expected);
    }
    CHECK(s.value == 19 * .03125f && clock_bda == 1);
    fd2_audio_destroy(audio); return 1;
}
int main(void) {
    if (!test_exact_fraction() || !test_closed_prefix_and_future() ||
        !test_fraction_at_sample_boundary() || !test_sync_command_order() ||
        !test_owner_and_invalid_no_effect() || !test_offline_same_clock_no_wall() ||
        !test_null_logic_and_irq() || !test_put_prefix_capture_and_close() ||
        !test_queue_bound_continues_logic() || !test_quiet_and_reentry() ||
        !test_irq_prefix_order()) return 1;
    puts("audio_transport: 11 source/clock/transport tests passed; no device opened"); return 0;
}
