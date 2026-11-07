/* A8真实设备探针：只供owned串行验收显式执行，不创建窗口或改变设备/混音器。
 * SDL3默认逻辑设备负责迁移；本探针不伪造设备移除，不把静音提交当可听输出。
 * 无参数/--dry-run只检查调用约定，不初始化SDL或打开设备。 */
#include "audio.h"
#include <SDL3/SDL.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 与真实audio.c同TU仅观察宿主调用；Open/Put均透传真实SDL，不换设备。 */
static _Atomic unsigned observed_callbacks, observed_puts, observed_put_successes;
static _Atomic unsigned rejected_nonzero_submissions;
static SDL_AudioStreamCallback implementation_callback;
static SDL_AudioStream *SDLCALL observe_open(SDL_AudioDeviceID id,
    const SDL_AudioSpec *spec, SDL_AudioStreamCallback callback, void *userdata);
static bool SDLCALL observe_put(SDL_AudioStream *stream, const void *data, int bytes);
#define SDL_OpenAudioDeviceStream observe_open
#define SDL_PutAudioStreamData observe_put
#include "../audio.c"
#undef SDL_OpenAudioDeviceStream
#undef SDL_PutAudioStreamData

static void SDLCALL observe_callback(void *userdata, SDL_AudioStream *stream,
                                      int additional, int total) {
    atomic_fetch_add_explicit(&observed_callbacks, 1, memory_order_relaxed);
    implementation_callback(userdata, stream, additional, total);
}
#ifndef FD2_DEVICE_PROBE_NATIVE_OPEN
#define FD2_DEVICE_PROBE_NATIVE_OPEN SDL_OpenAudioDeviceStream
#endif
#ifndef FD2_DEVICE_PROBE_NATIVE_PUT
#define FD2_DEVICE_PROBE_NATIVE_PUT SDL_PutAudioStreamData
#endif
static SDL_AudioStream *SDLCALL observe_open(SDL_AudioDeviceID id,
    const SDL_AudioSpec *spec, SDL_AudioStreamCallback callback, void *userdata) {
    implementation_callback = callback;
    return FD2_DEVICE_PROBE_NATIVE_OPEN(id, spec, observe_callback, userdata);
}
static bool SDLCALL observe_put(SDL_AudioStream *stream, const void *data, int bytes) {
    atomic_fetch_add_explicit(&observed_puts, 1, memory_order_relaxed);
    if (!data || bytes <= 0 || bytes % (int)sizeof(float)) return false;
    const float *values = data;
    for (int i = 0; i < bytes / (int)sizeof(float); i++) {
        if (values[i] != 0.0f) { /* quiet回归时fail-closed，不把声音交给用户设备。 */
            atomic_fetch_add_explicit(&rejected_nonzero_submissions, 1, memory_order_relaxed);
            return false;
        }
    }
    bool ok = FD2_DEVICE_PROBE_NATIVE_PUT(stream, data, bytes);
    if (ok) atomic_fetch_add_explicit(&observed_put_successes, 1, memory_order_relaxed);
    return ok;
}

typedef struct {
    float value;
    _Atomic unsigned frames;
    _Atomic unsigned renders;
    _Atomic unsigned retires;
    _Atomic int reason;
} probe_source;

static size_t render_probe(void *userdata, float *out, size_t frames) {
    probe_source *source = userdata;
    atomic_fetch_add_explicit(&source->renders, 1, memory_order_relaxed);
    for (size_t i = 0; i < frames * 2u; i++) out[i] = source->value;
    atomic_fetch_add_explicit(&source->frames, (unsigned)frames, memory_order_relaxed);
    return frames;
}
static void retire_probe(void *userdata, fd2_audio_retire_reason reason) {
    probe_source *source = userdata;
    atomic_store_explicit(&source->reason, (int)reason, memory_order_relaxed);
    atomic_fetch_add_explicit(&source->retires, 1, memory_order_release);
}
static fd2_audio_source source_for(probe_source *source) {
    return (fd2_audio_source){render_probe, retire_probe, source};
}
static int wait_for(probe_source *source, int retirement, unsigned milliseconds) {
    uint64_t deadline = SDL_GetTicks() + milliseconds;
    do {
        unsigned observed = retirement
            ? atomic_load_explicit(&source->retires, memory_order_acquire)
            : atomic_load_explicit(&source->frames, memory_order_acquire);
        if (observed) return 0;
        SDL_Delay(2);
    } while (SDL_GetTicks() < deadline);
    return -1;
}

/* 私有loss探针只在已有真实callback之后插入，不修改产品audio.c。 */
#ifndef FD2_DEVICE_PROBE_AFTER_SOURCES_READY
#define FD2_DEVICE_PROBE_AFTER_SOURCES_READY(audio) 0
#endif

int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "--dry-run") == 0)) {
        puts("{\"schema\":\"fd2-a8-device-probe-v1\",\"dry_run\":true,\"device_opened\":false,\"GUI_created\":false}");
        return 0;
    }
    if (argc != 3 || strcmp(argv[1], "--execute") != 0) {
        fputs("usage: fd2_audio_device_probe [--dry-run | --execute 100..10000_MS]\n", stderr);
        return 2;
    }
    char *end = NULL;
    unsigned long duration = strtoul(argv[2], &end, 10);
    if (!*argv[2] || *end || duration < 100 || duration > 10000) return 2;
    /* 子进程env：固定安静，不能意外继承捕获目录/worker；不改用户session音量。 */
    SDL_Environment *environment = SDL_GetEnvironment();
    if (!SDL_UnsetEnvironmentVariable(environment, "FD2SDL_APP_PCM_CAPTURE_DIR") ||
        !SDL_UnsetEnvironmentVariable(environment, "FD2SDL_APP_PCM_CAPTURE_MAX_MS") ||
        !SDL_UnsetEnvironmentVariable(environment, "FD2SDL_APP_PCM_CAPTURE_QUIET") ||
        !SDL_SetEnvironmentVariable(environment, "FD2SDL_TEST_AUDIO_QUIET", "1", true)) return 1;
    if (!SDL_Init(SDL_INIT_AUDIO)) {
        fprintf(stderr, "SDL audio init failed: %s\n", SDL_GetError());
        puts("{\"schema\":\"fd2-a8-device-probe-v1\",\"ok\":false,\"stage\":\"SDL_Init\",\"device_opened\":false}");
        return 1;
    }
    const char *driver = SDL_GetCurrentAudioDriver();
    if (!driver || !strcmp(driver, "dummy") || !strcmp(driver, "disk")) {
        fputs("A8 actual device probe requires a non-dummy/non-disk SDL backend\n", stderr);
        puts("{\"schema\":\"fd2-a8-device-probe-v1\",\"ok\":false,\"stage\":\"virtual_backend_rejected\",\"device_opened\":false}");
        SDL_Quit(); return 1;
    }
    int device_count = 0;
    SDL_AudioDeviceID *playback_devices = SDL_GetAudioPlaybackDevices(&device_count);
    SDL_AudioDeviceID logical_device = 0;
    SDL_AudioSpec device_format = {0};
    int device_sample_frames = 0;
    int format_observed = 0;
    int ok = 0, opened = 0, removed = 0, added = 0;
    const char *stage = "create";
    probe_source music = {.value = 0.0625f}, replacement = {.value = 0.125f}, sfx = {.value = -0.0625f};
    atomic_init(&music.frames, 0); atomic_init(&music.renders, 0); atomic_init(&music.retires, 0); atomic_init(&music.reason, -1);
    atomic_init(&replacement.frames, 0); atomic_init(&replacement.renders, 0); atomic_init(&replacement.retires, 0); atomic_init(&replacement.reason, -1);
    atomic_init(&sfx.frames, 0); atomic_init(&sfx.renders, 0); atomic_init(&sfx.retires, 0); atomic_init(&sfx.reason, -1);
    fd2_audio_error error = FD2_AUDIO_ERROR_NONE;
    fd2_audio_config config = {48000, 1, 1};
    fd2_audio *audio = fd2_audio_create(&config);
    if (!audio) goto done;
    opened = fd2_audio_has_device(audio);
    if (!opened) goto done;
    logical_device = SDL_GetAudioStreamDevice(audio->stream);
    format_observed = SDL_GetAudioDeviceFormat(logical_device, &device_format, &device_sample_frames);
    if (!logical_device || !format_observed) goto done;
    stage = "source callbacks";
    if (fd2_audio_play_music_source(audio, source_for(&music), 1) != 0 ||
        fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX, source_for(&sfx), 1) != 0 ||
        wait_for(&music, 0, 2000) != 0 || wait_for(&sfx, 0, 2000) != 0) goto done;
    stage = "private local disconnect and callback progress";
    if (FD2_DEVICE_PROBE_AFTER_SOURCES_READY(audio) != 0) goto done;
    stage = "replace and config restore";
    if (fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_MUSIC, 0) != 0 ||
        fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_SFX, 0) != 0 ||
        fd2_audio_play_music_source(audio, source_for(&replacement), 1) != 0 ||
        wait_for(&music, 1, 2000) != 0 || wait_for(&replacement, 0, 2000) != 0 ||
        fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_MUSIC, 1) != 0 ||
        fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_SFX, 1) != 0) goto done;
    uint64_t deadline = SDL_GetTicks() + duration;
    do {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_AUDIO_DEVICE_REMOVED) removed++;
            if (event.type == SDL_EVENT_AUDIO_DEVICE_ADDED) added++;
        }
        SDL_Delay(5);
    } while (SDL_GetTicks() < deadline);
    stage = "stop and retire";
    if (fd2_audio_stop_music(audio) != 0 ||
        fd2_audio_stop_bus(audio, FD2_AUDIO_BUS_SFX) != 0 ||
        wait_for(&replacement, 1, 2000) != 0 || wait_for(&sfx, 1, 2000) != 0) goto done;
    if (fd2_audio_get_first_error(audio, &error) != 0 || error != FD2_AUDIO_ERROR_NONE ||
        atomic_load(&music.retires) != 1 || atomic_load(&replacement.retires) != 1 ||
        atomic_load(&sfx.retires) != 1 || atomic_load(&music.reason) != FD2_AUDIO_RETIRE_REPLACED ||
        atomic_load(&replacement.reason) != FD2_AUDIO_RETIRE_STOPPED ||
        atomic_load(&sfx.reason) != FD2_AUDIO_RETIRE_STOPPED ||
        !atomic_load(&observed_callbacks) || !atomic_load(&observed_put_successes) ||
        atomic_load(&rejected_nonzero_submissions)) goto done;
    ok = 1; stage = "complete";
done:
    if (audio) (void)fd2_audio_get_first_error(audio, &error);
    fd2_audio_destroy(audio); /* 先停止真实callback，再让stack source失效。 */
    /* retire在render的命令消费阶段发布；同次callback的Put可能随后才失败。
     * 销流后counter已稳定，不能只用销毁前的首错快照决定成功。 */
    unsigned final_puts = atomic_load(&observed_puts);
    unsigned final_successes = atomic_load(&observed_put_successes);
    unsigned final_rejected = atomic_load(&rejected_nonzero_submissions);
    int all_submissions_succeeded = final_puts > 0 && final_puts == final_successes && !final_rejected;
    if (ok && !all_submissions_succeeded) { ok = 0; stage = "final_submission_check"; }
    printf("{\"schema\":\"fd2-a8-device-probe-v1\",\"ok\":%s,\"stage\":\"%s\",\"device_opened\":%s,\"first_error_before_destroy\":%d,\"all_submissions_succeeded_after_destroy\":%s,\"backend\":\"%s\",\"logical_device_id\":%u,\"format_observed\":%s,\"actual_frequency\":%d,\"actual_channels\":%d,\"actual_format\":%u,\"device_sample_frames\":%d,\"callback_calls\":%u,\"put_calls\":%u,\"successful_puts\":%u,\"rejected_nonzero_submissions\":%u,\"source_frames\":[%u,%u,%u],\"source_render_calls\":[%u,%u,%u],\"retire_counts\":[%u,%u,%u],\"added_events\":%d,\"removed_events\":%d,\"playback_device_ids\":[",
        ok ? "true" : "false", stage, opened ? "true" : "false", (int)error,
        all_submissions_succeeded ? "true" : "false",
        driver, (unsigned)logical_device, format_observed ? "true" : "false", device_format.freq,
        device_format.channels, (unsigned)device_format.format, device_sample_frames,
        atomic_load(&observed_callbacks), final_puts, final_successes, final_rejected,
        atomic_load(&music.frames), atomic_load(&replacement.frames), atomic_load(&sfx.frames),
        atomic_load(&music.renders), atomic_load(&replacement.renders), atomic_load(&sfx.renders),
        atomic_load(&music.retires), atomic_load(&replacement.retires), atomic_load(&sfx.retires), added, removed);
    for (int i = 0; playback_devices && i < device_count; i++) printf("%s%u", i ? "," : "", (unsigned)playback_devices[i]);
    puts("],\"test_quiet\":true,\"observer_created\":false,\"GUI_created\":false,\"device_volume_changed\":false,\"device_routing_changed\":false,\"preOS_only\":true,\"physical_unplug_validated\":false,\"audible_output_validated\":false,\"A7_validated\":false,\"A8_stage_complete\":false}");
    if (!ok) fprintf(stderr, "A8 device probe failed at %s: %s\n", stage, SDL_GetError());
    SDL_free(playback_devices);
    SDL_Quit();
    return ok ? 0 : 1;
}
