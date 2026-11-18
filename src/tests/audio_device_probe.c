/* A8真实设备探针：只供owned串行验收显式执行，不创建窗口或改变设备/混音器。
 * SDL3默认逻辑设备负责迁移；本探针不伪造设备移除，不把静音提交当可听输出。
 * 无参数/--dry-run只检查调用约定，不初始化SDL或打开设备。 */
#include "audio.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* 与真实audio.c同TU仅观察宿主调用；Open/Put均透传真实SDL，不换设备。 */
static SDL_AtomicInt observed_syncs, observed_puts, observed_put_successes;
static SDL_AtomicInt rejected_nonzero_submissions;
static unsigned callback_binding_errors;
static SDL_AtomicInt observed_queue_peak;
static SDL_ThreadID probe_thread;
static SDL_AtomicInt wrong_owner_calls;
static SDL_AudioStream *SDLCALL observe_open(SDL_AudioDeviceID id,
    const SDL_AudioSpec *spec, SDL_AudioStreamCallback callback, void *userdata);
static bool SDLCALL observe_put(SDL_AudioStream *stream, const void *data, int bytes);
static int SDLCALL observe_queued(SDL_AudioStream *stream);
#define SDL_OpenAudioDeviceStream observe_open
#define SDL_PutAudioStreamData observe_put
#define SDL_GetAudioStreamQueued observe_queued
#include "../audio.c"
#undef SDL_OpenAudioDeviceStream
#undef SDL_PutAudioStreamData
#undef SDL_GetAudioStreamQueued

#ifndef FD2_DEVICE_PROBE_NATIVE_OPEN
#define FD2_DEVICE_PROBE_NATIVE_OPEN SDL_OpenAudioDeviceStream
#endif
#ifndef FD2_DEVICE_PROBE_NATIVE_QUEUED
#define FD2_DEVICE_PROBE_NATIVE_QUEUED SDL_GetAudioStreamQueued
#endif
#ifndef FD2_DEVICE_PROBE_NATIVE_PUT
#define FD2_DEVICE_PROBE_NATIVE_PUT SDL_PutAudioStreamData
#endif
static SDL_AudioStream *SDLCALL observe_open(SDL_AudioDeviceID id,
    const SDL_AudioSpec *spec, SDL_AudioStreamCallback callback, void *userdata) {
    if(callback || userdata){callback_binding_errors++;return NULL;}
    return FD2_DEVICE_PROBE_NATIVE_OPEN(id, spec, NULL, NULL);
}
static bool SDLCALL observe_put(SDL_AudioStream *stream, const void *data, int bytes) {
    SDL_AddAtomicInt(&observed_puts, 1);
    if(SDL_GetCurrentThreadID()!=probe_thread)SDL_AddAtomicInt(&wrong_owner_calls,1);
    if (!data || bytes <= 0 || bytes % (int)sizeof(float)) return false;
    const float *values = data;
    for (int i = 0; i < bytes / (int)sizeof(float); i++) {
        if (values[i] != 0.0f) { /* quiet回归时fail-closed，不把声音交给用户设备。 */
            SDL_AddAtomicInt(&rejected_nonzero_submissions, 1);
            return false;
        }
    }
    bool ok = FD2_DEVICE_PROBE_NATIVE_PUT(stream, data, bytes);
    if (ok) SDL_AddAtomicInt(&observed_put_successes, 1);
    return ok;
}

static int SDLCALL observe_queued(SDL_AudioStream *stream) {
    int queued=FD2_DEVICE_PROBE_NATIVE_QUEUED(stream);
    if(queued>=0){
        int peak=SDL_GetAtomicInt(&observed_queue_peak);
        while (queued > peak &&
               !SDL_CompareAndSwapAtomicInt(&observed_queue_peak, peak, queued)) {
            peak = SDL_GetAtomicInt(&observed_queue_peak);
        }
    }
    return queued;
}
static int probe_sync(fd2_audio *audio) {
    SDL_AddAtomicInt(&observed_syncs,1);
    return fd2_audio_sync(audio);
}

typedef struct {
    float value;
    SDL_AtomicInt frames;
    SDL_AtomicInt renders;
    SDL_AtomicInt retires;
    SDL_AtomicInt reason;
} probe_source;

static size_t render_probe(void *userdata, float *out, size_t frames) {
    probe_source *source = userdata;
    if(SDL_GetCurrentThreadID()!=probe_thread)SDL_AddAtomicInt(&wrong_owner_calls,1);
    SDL_AddAtomicInt(&source->renders, 1);
    for (size_t i = 0; i < frames * 2u; i++) out[i] = source->value;
    SDL_AddAtomicInt(&source->frames, (int)frames);
    return frames;
}
static void retire_probe(void *userdata, fd2_audio_retire_reason reason) {
    probe_source *source = userdata;
    if(SDL_GetCurrentThreadID()!=probe_thread)SDL_AddAtomicInt(&wrong_owner_calls,1);
    SDL_SetAtomicInt(&source->reason, (int)reason);
    SDL_AddAtomicInt(&source->retires, 1);
}
static fd2_audio_source source_for(probe_source *source) {
    return (fd2_audio_source){render_probe, retire_probe, source};
}
static int wait_for(fd2_audio *audio, probe_source *source, int retirement, unsigned milliseconds) {
    uint64_t deadline = SDL_GetTicks() + milliseconds;
    do {
        int observed = retirement
            ? SDL_GetAtomicInt(&source->retires)
            : SDL_GetAtomicInt(&source->frames);
        if (observed) return 0;
        if(probe_sync(audio)!=0)return -1;
        SDL_Delay(2);
    } while (SDL_GetTicks() < deadline);
    return -1;
}

int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && strcmp(argv[1], "--dry-run") == 0)) {
        puts("{\"schema\":\"fd2-a8-device-probe-v2\",\"dry_run\":true,\"device_opened\":false,\"GUI_created\":false}");
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
        puts("{\"schema\":\"fd2-a8-device-probe-v2\",\"ok\":false,\"stage\":\"SDL_Init\",\"device_opened\":false}");
        return 1;
    }
    const char *driver = SDL_GetCurrentAudioDriver();
    if (!driver || !strcmp(driver, "dummy") || !strcmp(driver, "disk")) {
        fputs("A8 actual device probe requires a non-dummy/non-disk SDL backend\n", stderr);
        puts("{\"schema\":\"fd2-a8-device-probe-v2\",\"ok\":false,\"stage\":\"virtual_backend_rejected\",\"device_opened\":false}");
        SDL_Quit(); return 1;
    }
    if(fd2_bios_clock_init()!=0){SDL_Quit();return 1;}
    probe_thread=SDL_GetCurrentThreadID();
    int device_count = 0;
    SDL_AudioDeviceID *playback_devices = SDL_GetAudioPlaybackDevices(&device_count);
    SDL_AudioDeviceID logical_device = 0;
    SDL_AudioSpec device_format = {0};
    int device_sample_frames = 0;
    int format_observed = 0;
    int ok = 0, opened = 0, removed = 0, added = 0;
    const char *stage = "create";
    probe_source music = {.value = 0.0625f, .reason = {-1}};
    probe_source replacement = {.value = 0.125f, .reason = {-1}};
    probe_source sfx = {.value = -0.0625f, .reason = {-1}};
    fd2_audio_error error = FD2_AUDIO_ERROR_NONE;
    fd2_audio_config config = {48000, 1, 1, 0};
    fd2_audio *audio = fd2_audio_create(&config);
    if (!audio) goto done;
    opened = fd2_audio_has_device(audio);
    if (!opened) goto done;
    logical_device = SDL_GetAudioStreamDevice(audio->stream);
    format_observed = SDL_GetAudioDeviceFormat(logical_device, &device_format, &device_sample_frames);
    if (!logical_device || !format_observed) goto done;
    stage = "main-thread source sync";
    if (fd2_audio_play_music_source(audio, source_for(&music), 1) != 0 ||
        fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX, source_for(&sfx), 1) != 0 ||
        wait_for(audio, &music, 0, 2000) != 0 || wait_for(audio, &sfx, 0, 2000) != 0) goto done;
#ifdef FD2_DEVICE_PROBE_AFTER_SOURCES_READY
    /* 私有断连探针只在主线程已提交 source 后运行。 */
    stage = "private local disconnect and main-thread sync progress";
    if (FD2_DEVICE_PROBE_AFTER_SOURCES_READY(audio) != 0) goto done;
#endif
    stage = "replace and config restore";
    if (fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_MUSIC, 0) != 0 ||
        fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_SFX, 0) != 0 ||
        fd2_audio_play_music_source(audio, source_for(&replacement), 1) != 0 ||
        wait_for(audio, &music, 1, 2000) != 0 || wait_for(audio, &replacement, 0, 2000) != 0 ||
        fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_MUSIC, 1) != 0 ||
        fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_SFX, 1) != 0) goto done;
    uint64_t deadline = SDL_GetTicks() + duration;
    do {
        if(probe_sync(audio)!=0)goto done;
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
        wait_for(audio, &replacement, 1, 2000) != 0 || wait_for(audio, &sfx, 1, 2000) != 0) goto done;
    if (fd2_audio_get_first_error(audio, &error) != 0 || error != FD2_AUDIO_ERROR_NONE ||
        SDL_GetAtomicInt(&music.retires) != 1 || SDL_GetAtomicInt(&replacement.retires) != 1 ||
        SDL_GetAtomicInt(&sfx.retires) != 1 || SDL_GetAtomicInt(&music.reason) != FD2_AUDIO_RETIRE_REPLACED ||
        SDL_GetAtomicInt(&replacement.reason) != FD2_AUDIO_RETIRE_STOPPED ||
        SDL_GetAtomicInt(&sfx.reason) != FD2_AUDIO_RETIRE_STOPPED ||
        callback_binding_errors || SDL_GetAtomicInt(&wrong_owner_calls) ||
        !SDL_GetAtomicInt(&observed_syncs) || !SDL_GetAtomicInt(&observed_put_successes) ||
        SDL_GetAtomicInt(&rejected_nonzero_submissions)) goto done;
    ok = 1; stage = "complete";
done:
    if (audio) (void)fd2_audio_get_first_error(audio, &error);
    if(fd2_bios_clock_close()!=0){ok=0;stage="clock_close";}
    fd2_audio_destroy(audio); /* 先停stream再retire，随后stack source才可失效。 */
    /* Put已同步完成；销流阶段不得再渲染/提交或借stack source userdata。
     * 保销毁后的counter复核，不能把关闭前的快照当可听输出。 */
    int final_puts = SDL_GetAtomicInt(&observed_puts);
    int final_successes = SDL_GetAtomicInt(&observed_put_successes);
    int final_rejected = SDL_GetAtomicInt(&rejected_nonzero_submissions);
    int all_submissions_succeeded = final_puts > 0 && final_puts == final_successes && !final_rejected;
    if (ok && !all_submissions_succeeded) { ok = 0; stage = "final_submission_check"; }
    printf("{\"schema\":\"fd2-a8-device-probe-v2\",\"ok\":%s,\"stage\":\"%s\",\"device_opened\":%s,\"first_error_before_destroy\":%d,\"all_submissions_succeeded_after_destroy\":%s,\"backend\":\"%s\",\"logical_device_id\":%u,\"format_observed\":%s,\"actual_frequency\":%d,\"actual_channels\":%d,\"actual_format\":%u,\"device_sample_frames\":%d,\"transport_owner\":\"main-thread\",\"device_callback_bound\":false,\"source_owner_violations\":%d,\"sampled_queue_peak_bytes\":%d,\"sync_calls\":%d,\"put_calls\":%d,\"successful_puts\":%d,\"rejected_nonzero_submissions\":%d,\"source_frames\":[%d,%d,%d],\"source_render_calls\":[%d,%d,%d],\"retire_counts\":[%d,%d,%d],\"added_events\":%d,\"removed_events\":%d,\"playback_device_ids\":[",
        ok ? "true" : "false", stage, opened ? "true" : "false", (int)error,
        all_submissions_succeeded ? "true" : "false",
        driver, (unsigned)logical_device, format_observed ? "true" : "false", device_format.freq,
        device_format.channels, (unsigned)device_format.format, device_sample_frames,
        SDL_GetAtomicInt(&wrong_owner_calls),SDL_GetAtomicInt(&observed_queue_peak),SDL_GetAtomicInt(&observed_syncs), final_puts, final_successes, final_rejected,
        SDL_GetAtomicInt(&music.frames), SDL_GetAtomicInt(&replacement.frames), SDL_GetAtomicInt(&sfx.frames),
        SDL_GetAtomicInt(&music.renders), SDL_GetAtomicInt(&replacement.renders), SDL_GetAtomicInt(&sfx.renders),
        SDL_GetAtomicInt(&music.retires), SDL_GetAtomicInt(&replacement.retires), SDL_GetAtomicInt(&sfx.retires), added, removed);
    for (int i = 0; playback_devices && i < device_count; i++) printf("%s%u", i ? "," : "", (unsigned)playback_devices[i]);
    puts("],\"test_quiet\":true,\"observer_created\":false,\"GUI_created\":false,\"device_volume_changed\":false,\"device_routing_changed\":false,\"preOS_only\":true,\"physical_unplug_validated\":false,\"audible_output_validated\":false,\"A7_validated\":false,\"A8_stage_complete\":false}");
    if (!ok) fprintf(stderr, "A8 device probe failed at %s: %s\n", stage, SDL_GetError());
    SDL_free(playback_devices);
    SDL_Quit();
    return ok ? 0 : 1;
}
