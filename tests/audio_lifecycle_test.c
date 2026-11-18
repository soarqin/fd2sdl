/* A8宿主错误边界：仅以单TU替换SDL设备函数，不初始化SDL或访问真实设备。
 * 业务边界依据audio.h和battle-session：只记录错误，不回放source或推进Game。
 * 原版音乐/sample调用依据保留在audio.c；这些宿主故障不冒称DOSBox行为。 */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "bios_clock.h"
static uint64_t observed_ns;
static uint64_t test_now_ns(void) { return observed_ns; }
static int SDLCALL test_queued(SDL_AudioStream *stream) { (void)stream; return 0; }

static SDL_AudioStream *SDLCALL test_open(SDL_AudioDeviceID device,
    const SDL_AudioSpec *spec, SDL_AudioStreamCallback callback, void *userdata);
static bool SDLCALL test_resume(SDL_AudioStream *stream);
static bool SDLCALL test_pause(SDL_AudioStream *stream);
static void SDLCALL test_destroy(SDL_AudioStream *stream);
static bool SDLCALL test_put(SDL_AudioStream *stream, const void *data, int length);

#define SDL_OpenAudioDeviceStream test_open
#define SDL_ResumeAudioStreamDevice test_resume
#define SDL_PauseAudioStreamDevice test_pause
#define SDL_DestroyAudioStream test_destroy
#define SDL_PutAudioStreamData test_put
#define SDL_GetAudioStreamQueued test_queued
#define SDL_GetTicksNS test_now_ns
#include "../src/audio.c"
#undef SDL_OpenAudioDeviceStream
#undef SDL_ResumeAudioStreamDevice
#undef SDL_PauseAudioStreamDevice
#undef SDL_DestroyAudioStream
#undef SDL_PutAudioStreamData
#undef SDL_GetAudioStreamQueued
#undef SDL_GetTicksNS

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "CHECK failed %s:%d: %s\n", __FILE__, __LINE__, #x); \
    return 1; } } while (0)

typedef enum { SUCCEED, FAIL_OPEN, FAIL_RESUME, FAIL_PUT } fault_mode;
static struct {
    fault_mode mode;
    unsigned opens, resumes, pauses, destroys, puts, bad_arguments;
    unsigned fail_put_at;
    int fail_pause;
    size_t attempted_frames;
    SDL_AudioStreamCallback callback;
    void *userdata;
} fake;
static unsigned char stream_token;

static SDL_AudioStream *fake_stream(void) {
    return (SDL_AudioStream *)&stream_token; /* 只比较身份，从不解引用。 */
}

static SDL_AudioStream *SDLCALL test_open(SDL_AudioDeviceID device,
    const SDL_AudioSpec *spec, SDL_AudioStreamCallback callback, void *userdata) {
    fake.opens++;
    if (device != SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK || !spec ||
        spec->format != SDL_AUDIO_F32 || spec->channels != 2 ||
        spec->freq != 48000 || callback || userdata)
        fake.bad_arguments++;
    fake.callback = callback;
    fake.userdata = userdata;
    return fake.mode == FAIL_OPEN ? NULL : fake_stream();
}

static bool SDLCALL test_resume(SDL_AudioStream *stream) {
    fake.resumes++;
    if (stream != fake_stream()) fake.bad_arguments++;
    return fake.mode != FAIL_RESUME;
}

static bool SDLCALL test_pause(SDL_AudioStream *stream) {
    fake.pauses++;
    if (stream != fake_stream()) fake.bad_arguments++;
    return !fake.fail_pause;
}

static void SDLCALL test_destroy(SDL_AudioStream *stream) {
    fake.destroys++;
    if (stream != fake_stream()) fake.bad_arguments++;
}

static bool SDLCALL test_put(SDL_AudioStream *stream, const void *data, int length) {
    fake.puts++;
    if (stream != fake_stream() || !data || length <= 0 ||
        length % (int)(2u * sizeof(float)) != 0) {
        fake.bad_arguments++;
        return false;
    }
    fake.attempted_frames += (size_t)length / (2u * sizeof(float));
    return fake.mode != FAIL_PUT && fake.puts != fake.fail_put_at;
}

typedef struct {
    float value;
    size_t left;
    unsigned renders, retires;
    fd2_audio_retire_reason reason;
} constant_source;

static size_t render_constant(void *userdata, float *stereo, size_t frames) {
    constant_source *source = userdata;
    size_t count = source->left < frames ? source->left : frames;
    source->renders++;
    for (size_t i = 0; i < count * 2u; i++) stereo[i] = source->value;
    source->left -= count;
    return count;
}

static void retire_constant(void *userdata, fd2_audio_retire_reason reason) {
    constant_source *source = userdata;
    source->retires++;
    source->reason = reason;
}

static fd2_audio_source constant(constant_source *source) {
    return (fd2_audio_source){render_constant, retire_constant, source};
}

static void new_scope(void) {
    memset(&fake, 0, sizeof(fake)); observed_ns = 0;
    fd2_bios_clock_reset(0, 0); /* 当前App外部输入，设备调用全部替换。 */
}
static int close_frames(fd2_audio *audio, uint64_t total) {
    observed_ns = total * UINT64_C(1000000000) / 48000u;
    return fd2_audio_sync(audio);
}

static int test_synchronous_bus_targets_without_queue(void) {
    memset(&fake, 0, sizeof(fake));
    fd2_audio_config config = {.sample_rate = 48000, .open_device = 0};
    fd2_audio *audio = fd2_audio_create(&config);
    CHECK(audio && !fd2_audio_has_device(audio));
    constant_source music = {.value = 0.25f, .left = 64};
    constant_source sfx = {.value = 0.5f, .left = 64};
    CHECK(fd2_audio_play_music_source(audio, constant(&music), 1.0f) == 0);
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX, constant(&sfx), 1.0f) == 0);
    for (unsigned i = 0; i < 62; i++)
        CHECK(fd2_audio_set_master_gain(audio, 1.0f) == 0);
    CHECK(fd2_audio_set_master_gain(audio, 1.0f) == 0);
    for (unsigned i = 0; i < 100; i++) {
        CHECK(fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_MUSIC, (float)(i & 1u)) == 0);
        CHECK(fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_SFX, (float)(i & 1u)) == 0);
    }
    CHECK(fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_MUSIC, 0.0f) == 0);
    CHECK(fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_SFX, 0.5f) == 0);
    float target = -1.0f;
    CHECK(fd2_audio_get_bus_gain_target(audio, FD2_AUDIO_BUS_MUSIC, &target) == 0 && target == 0.0f);
    CHECK(fd2_audio_get_bus_gain_target(audio, FD2_AUDIO_BUS_SFX, &target) == 0 && target == 0.5f);
    CHECK(!music.renders && !sfx.renders && !music.retires && !sfx.retires);
    CHECK(fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_SFX, NAN) == -1);
    CHECK(fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_SFX, -1.0f) == -1);
    CHECK(fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_SFX, 17.0f) == -1);
    CHECK(fd2_audio_get_bus_gain_target(audio, FD2_AUDIO_BUS_SFX, &target) == 0 && target == 0.5f);
    float output[4];
    CHECK(fd2_audio_render_offline(audio, output, 2) == 2);
    for (size_t i = 0; i < 4; i++) CHECK(output[i] == 0.25f);
    CHECK(music.renders == 1 && sfx.renders == 1 && music.left == 62 && sfx.left == 62);
    CHECK(fd2_audio_set_master_gain(audio, 0.5f) == 0);
    CHECK(fd2_audio_render_offline(audio, output, 1) == 1 && output[0] == 0.125f && output[1] == 0.125f);
    CHECK(fd2_audio_stop_bus(audio, FD2_AUDIO_BUS_SFX) == 0);
    CHECK(fd2_audio_render_offline(audio, output, 1) == 1 && output[0] == 0 && output[1] == 0);
    CHECK(sfx.retires == 1 && sfx.reason == FD2_AUDIO_RETIRE_STOPPED);
    fd2_audio_error error = FD2_AUDIO_ERROR_PUT;
    CHECK(fd2_audio_get_first_error(audio, &error) == 0 && error == FD2_AUDIO_ERROR_NONE);
    fd2_audio_destroy(audio);
    CHECK(music.retires == 1 && music.reason == FD2_AUDIO_RETIRE_DESTROYED && sfx.retires == 1);
    CHECK(!fake.opens && !fake.resumes && !fake.pauses && !fake.destroys && !fake.puts);
    return 0;
}

static int test_invalid_getters_preserve_output(void) {
    fd2_audio_config config = {.open_device = 0};
    fd2_audio *audio = fd2_audio_create(&config);
    CHECK(audio);
    float target = 42.0f;
    fd2_audio_error error = FD2_AUDIO_ERROR_PUT;
    CHECK(fd2_audio_get_bus_gain_target(NULL, FD2_AUDIO_BUS_MUSIC, &target) == -1 && target == 42.0f);
    CHECK(fd2_audio_get_bus_gain_target(audio, FD2_AUDIO_BUS_COUNT, &target) == -1 && target == 42.0f);
    CHECK(fd2_audio_get_bus_gain_target(audio, FD2_AUDIO_BUS_MUSIC, NULL) == -1);
    CHECK(fd2_audio_get_first_error(NULL, &error) == -1 && error == FD2_AUDIO_ERROR_PUT);
    CHECK(fd2_audio_get_first_error(audio, NULL) == -1);
    CHECK(fd2_audio_set_bus_gain(NULL, FD2_AUDIO_BUS_MUSIC, 0) == -1);
    CHECK(fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_COUNT, 0) == -1);
    CHECK(fd2_audio_get_bus_gain_target(audio, FD2_AUDIO_BUS_MUSIC, &target) == 0 && target == 1.0f);
    fd2_audio_destroy(audio);
    return 0;
}

static int test_open_and_resume_failures(void) {
    for (int mode = FAIL_OPEN; mode <= FAIL_RESUME; mode++) {
        for (int allow = 0; allow <= 1; allow++) {
            new_scope(); fake.mode = (fault_mode)mode;
            fd2_audio_config config = {.sample_rate = 48000, .open_device = 1, .allow_null = allow};
            fd2_audio *audio = fd2_audio_create(&config);
            CHECK(fake.opens == 1 && !fake.bad_arguments && !fake.pauses && !fake.puts);
            CHECK(fake.resumes == (unsigned)(mode == FAIL_RESUME));
            CHECK(fake.destroys == (unsigned)(mode == FAIL_RESUME));
            if (!allow) { CHECK(!audio); continue; }
            CHECK(audio && !fd2_audio_has_device(audio));
            fd2_audio_error error = FD2_AUDIO_ERROR_NONE;
            fd2_audio_error expected = mode == FAIL_OPEN ? FD2_AUDIO_ERROR_OPEN : FD2_AUDIO_ERROR_RESUME;
            CHECK(fd2_audio_get_first_error(audio, &error) == 0 && error == expected);
            constant_source sources[80] = {{0}};
            for (size_t i = 0; i < 80; i++) {
                sources[i].value = 0.25f; sources[i].left = 16;
                CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX, constant(&sources[i]), 1) == 0);
                CHECK(!sources[i].renders); /* null仍保active；满16只抢同bus最旧。 */
            }
            for (size_t i = 0; i < 80; i++)
                CHECK(sources[i].retires == (unsigned)(i < 64) &&
                      (i >= 64 || sources[i].reason == FD2_AUDIO_RETIRE_STOLEN));
            CHECK(fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_SFX, 0) == 0);
            CHECK(close_frames(audio, 32) == 0);
            for (size_t i = 64; i < 80; i++)
                CHECK(sources[i].renders == 1 && sources[i].left == 0 &&
                      sources[i].retires == 1 && sources[i].reason == FD2_AUDIO_RETIRE_FINISHED);
            CHECK(fd2_audio_stop_bus(audio, FD2_AUDIO_BUS_SFX) == 0);
            CHECK(fd2_audio_get_first_error(audio, &error) == 0 && error == expected);
            fd2_audio_destroy(audio);
            CHECK(fake.opens == 1 && !fake.pauses && !fake.bad_arguments);
            CHECK(fake.destroys == (unsigned)(mode == FAIL_RESUME));
            for (size_t i = 0; i < 80; i++) CHECK(sources[i].retires == 1 && sources[i].renders == (unsigned)(i >= 64));
        }
    }
    return 0;
}

static int test_put_failure_is_sticky_without_replay(void) {
    new_scope(); fake.mode = FAIL_PUT;
    fd2_audio_config config = {.sample_rate = 48000, .open_device = 1, .allow_null = 1};
    fd2_audio *audio = fd2_audio_create(&config);
    CHECK(audio && fd2_audio_has_device(audio) && !fake.callback && !fake.userdata);
    constant_source source = {.value = 0.25f, .left = 2000};
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX, constant(&source), 1) == 0);
    CHECK(close_frames(audio, 519) == 0);
    CHECK(source.renders == 2 && source.left == 1481 && !source.retires);
    CHECK(fake.puts == 2 && fake.attempted_frames == 519 && !fake.destroys && !fake.pauses);
    fd2_audio_error error = FD2_AUDIO_ERROR_NONE;
    CHECK(fd2_audio_get_first_error(audio, &error) == 0 && error == FD2_AUDIO_ERROR_PUT);
    CHECK(fd2_audio_has_device(audio)); /* 历史故障不等于由callback销毁stream。 */
    fake.mode = SUCCEED;
    CHECK(close_frames(audio, 522) == 0);
    CHECK(source.renders == 3 && source.left == 1478 && fake.puts == 3);
    CHECK(fd2_audio_get_first_error(audio, &error) == 0 && error == FD2_AUDIO_ERROR_PUT);
    fd2_audio_destroy(audio);
    CHECK(fake.opens == 1 && fake.resumes == 1 && fake.pauses == 1 && fake.destroys == 1);
    CHECK(source.retires == 1 && source.reason == FD2_AUDIO_RETIRE_DESTROYED && !fake.bad_arguments);
    return 0;
}

static int test_later_put_failure_and_failed_pause_cleanup(void) {
    new_scope(); fake.fail_put_at = 2;
    fd2_audio_config config = {.sample_rate = 48000, .open_device = 1, .allow_null = 0};
    fd2_audio *audio = fd2_audio_create(&config);
    CHECK(audio && !fake.callback && !fake.userdata);
    constant_source source = {.value = 0.25f, .left = 2000};
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_MUSIC, constant(&source), 1) == 0);
    CHECK(close_frames(audio, 1536) == 0);
    CHECK(source.renders == 3 && source.left == 464 && fake.puts == 3);
    fd2_audio_error error = FD2_AUDIO_ERROR_NONE;
    CHECK(fd2_audio_get_first_error(audio, &error) == 0 && error == FD2_AUDIO_ERROR_PUT);
    CHECK(close_frames(audio, 1543) == 0);
    CHECK(source.renders == 4 && source.left == 457 && fake.puts == 4);
    fake.fail_pause = 1;
    fd2_audio_destroy(audio); /* Pause失败也必须Destroy，再retire一次；callback不销毁stream。 */
    CHECK(fake.opens == 1 && fake.resumes == 1 && fake.pauses == 1 && fake.destroys == 1);
    CHECK(source.retires == 1 && source.reason == FD2_AUDIO_RETIRE_DESTROYED && !fake.bad_arguments);
    return 0;
}

static size_t other_render_identity(void *user,float *stereo,size_t frames) {
    (void)user;(void)stereo;(void)frames;return 0;
}
static int test_abandon_after_clock_failure(void) {
    new_scope();
    fd2_audio_config config={.sample_rate=48000,.open_device=0};
    fd2_audio *audio=fd2_audio_create(&config);CHECK(audio);
    constant_source install={.value=0.25f,.left=64},other={.value=0.5f,.left=64};
    fd2_audio_source source=constant(&install);
    CHECK(fd2_audio_play_music_source(audio,source,1)==0);
    CHECK(fd2_audio_play_source(audio,FD2_AUDIO_BUS_SFX,constant(&other),1)==0);
    CHECK(fd2_bios_clock_close()==0 && fd2_audio_sync(audio)==-1);
    fd2_audio_source wrong=source;wrong.retire=NULL;
    CHECK(fd2_audio_abandon_source(audio,wrong)==0);
    wrong=source;wrong.render=other_render_identity;CHECK(fd2_audio_abandon_source(audio,wrong)==0);
    wrong=source;wrong.userdata=NULL;CHECK(fd2_audio_abandon_source(audio,wrong)==0);
    wrong=source;wrong.render=NULL;CHECK(fd2_audio_abandon_source(audio,wrong)==-1);
    CHECK(fd2_audio_abandon_source(NULL,source)==-1);
    const uint64_t before=audio->closed_frames,now=audio->now_ns;
    CHECK(fd2_audio_abandon_source(audio,source)==1);
    CHECK(install.retires==1 && install.reason==FD2_AUDIO_RETIRE_DROPPED);
    CHECK(fd2_audio_abandon_source(audio,source)==0 && !other.retires);
    CHECK(!install.renders && !other.renders && audio->closed_frames==before && audio->now_ns==now);
    fd2_audio_error error;CHECK(fd2_audio_get_first_error(audio,&error)==0 && error==FD2_AUDIO_ERROR_CLOCK);
    fd2_audio_destroy(audio);
    CHECK(install.retires==1 && other.retires==1 && other.reason==FD2_AUDIO_RETIRE_DESTROYED);
    CHECK(!fake.opens && !fake.puts);return 0;
}

int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--abandon-source")) {
        CHECK(test_abandon_after_clock_failure()==0);
        puts("audio failed-install source: exact identity, clock-failure cleanup, no advance PASS");return 0;
    }
    if(argc!=1)return 2;
    /* 清理本测试进程继承的取证env，保证所有设备调用都只落宏替身。 */
    CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "FD2SDL_APP_PCM_CAPTURE_DIR"));
    CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "FD2SDL_APP_PCM_CAPTURE_MAX_MS"));
    CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "FD2SDL_APP_PCM_CAPTURE_QUIET"));
    CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "FD2SDL_TEST_AUDIO_QUIET"));
    CHECK(test_synchronous_bus_targets_without_queue() == 0);
    CHECK(test_invalid_getters_preserve_output() == 0);
    CHECK(test_open_and_resume_failures() == 0);
    CHECK(test_put_failure_is_sticky_without_replay() == 0);
    CHECK(test_later_put_failure_and_failed_pause_cleanup() == 0);
    CHECK(test_abandon_after_clock_failure()==0);
    puts("audio lifecycle tests: 6 groups ok (SDL device calls replaced; no real device)");
    return 0;
}
