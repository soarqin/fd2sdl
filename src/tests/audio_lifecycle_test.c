/* A8宿主错误边界：仅以单TU替换SDL设备函数，不初始化SDL或访问真实设备。
 * 业务边界依据audio.h和battle-session：只记录错误，不回放source或推进Game。
 * 原版音乐/sample调用依据保留在audio.c；这些宿主故障不冒称DOSBox行为。 */
#include <SDL3/SDL.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"

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
#include "../audio.c"
#undef SDL_OpenAudioDeviceStream
#undef SDL_ResumeAudioStreamDevice
#undef SDL_PauseAudioStreamDevice
#undef SDL_DestroyAudioStream
#undef SDL_PutAudioStreamData

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
        spec->freq != 48000 || !callback || !userdata)
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

static int test_latest_bus_targets_under_queue_pressure(void) {
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
    CHECK(fd2_audio_set_master_gain(audio, 1.0f) == -1);
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
            memset(&fake, 0, sizeof(fake)); fake.mode = (fault_mode)mode;
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
                CHECK(sources[i].retires == 1 && !sources[i].renders &&
                      sources[i].reason == FD2_AUDIO_RETIRE_DROPPED);
            }
            CHECK(fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_SFX, 0) == 0);
            CHECK(fd2_audio_stop_bus(audio, FD2_AUDIO_BUS_SFX) == 0);
            CHECK(fd2_audio_get_first_error(audio, &error) == 0 && error == expected);
            fd2_audio_destroy(audio);
            CHECK(fake.opens == 1 && !fake.pauses && !fake.bad_arguments);
            CHECK(fake.destroys == (unsigned)(mode == FAIL_RESUME));
            for (size_t i = 0; i < 80; i++) CHECK(sources[i].retires == 1 && !sources[i].renders);
        }
    }
    return 0;
}

static int test_put_failure_is_sticky_without_replay(void) {
    memset(&fake, 0, sizeof(fake)); fake.mode = FAIL_PUT;
    fd2_audio_config config = {.sample_rate = 48000, .open_device = 1, .allow_null = 1};
    fd2_audio *audio = fd2_audio_create(&config);
    CHECK(audio && fd2_audio_has_device(audio) && fake.callback && fake.userdata);
    constant_source source = {.value = 0.25f, .left = 2000};
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX, constant(&source), 1) == 0);
    fake.callback(fake.userdata, fake_stream(), (512 + 7) * (int)(2u * sizeof(float)), 0);
    CHECK(source.renders == 1 && source.left == 1488 && !source.retires);
    CHECK(fake.puts == 1 && fake.attempted_frames == 512 && !fake.destroys && !fake.pauses);
    fd2_audio_error error = FD2_AUDIO_ERROR_NONE;
    CHECK(fd2_audio_get_first_error(audio, &error) == 0 && error == FD2_AUDIO_ERROR_PUT);
    CHECK(fd2_audio_has_device(audio)); /* 历史故障不等于由callback销毁stream。 */
    fake.mode = SUCCEED;
    fake.callback(fake.userdata, fake_stream(), 3 * (int)(2u * sizeof(float)), 0);
    CHECK(source.renders == 2 && source.left == 1485 && fake.puts == 2);
    CHECK(fd2_audio_get_first_error(audio, &error) == 0 && error == FD2_AUDIO_ERROR_PUT);
    fd2_audio_destroy(audio);
    CHECK(fake.opens == 1 && fake.resumes == 1 && fake.pauses == 1 && fake.destroys == 1);
    CHECK(source.retires == 1 && source.reason == FD2_AUDIO_RETIRE_DESTROYED && !fake.bad_arguments);
    return 0;
}

static int test_later_put_failure_and_failed_pause_cleanup(void) {
    memset(&fake, 0, sizeof(fake)); fake.fail_put_at = 2;
    fd2_audio_config config = {.sample_rate = 48000, .open_device = 1, .allow_null = 0};
    fd2_audio *audio = fd2_audio_create(&config);
    CHECK(audio && fake.callback);
    constant_source source = {.value = 0.25f, .left = 2000};
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_MUSIC, constant(&source), 1) == 0);
    fake.callback(fake.userdata, fake_stream(), 1536 * (int)(2u * sizeof(float)), 0);
    CHECK(source.renders == 2 && source.left == 976 && fake.puts == 2);
    fd2_audio_error error = FD2_AUDIO_ERROR_NONE;
    CHECK(fd2_audio_get_first_error(audio, &error) == 0 && error == FD2_AUDIO_ERROR_PUT);
    fake.callback(fake.userdata, fake_stream(), 7 * (int)(2u * sizeof(float)), 0);
    CHECK(source.renders == 3 && source.left == 969 && fake.puts == 3);
    fake.fail_pause = 1;
    fd2_audio_destroy(audio); /* Pause失败也必须Destroy，再retire一次；callback不销毁stream。 */
    CHECK(fake.opens == 1 && fake.resumes == 1 && fake.pauses == 1 && fake.destroys == 1);
    CHECK(source.retires == 1 && source.reason == FD2_AUDIO_RETIRE_DESTROYED && !fake.bad_arguments);
    return 0;
}

int main(void) {
    /* 清理本测试进程继承的取证env，保证所有设备调用都只落宏替身。 */
    CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "FD2SDL_APP_PCM_CAPTURE_DIR"));
    CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "FD2SDL_APP_PCM_CAPTURE_MAX_MS"));
    CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "FD2SDL_APP_PCM_CAPTURE_QUIET"));
    CHECK(SDL_UnsetEnvironmentVariable(SDL_GetEnvironment(), "FD2SDL_TEST_AUDIO_QUIET"));
    CHECK(test_latest_bus_targets_under_queue_pressure() == 0);
    CHECK(test_invalid_getters_preserve_output() == 0);
    CHECK(test_open_and_resume_failures() == 0);
    CHECK(test_put_failure_is_sticky_without_replay() == 0);
    CHECK(test_later_put_failure_and_failed_pause_cleanup() == 0);
    puts("audio lifecycle tests: 5 groups ok (SDL device calls replaced; no real device)");
    return 0;
}
