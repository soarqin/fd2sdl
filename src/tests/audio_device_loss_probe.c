/* A8私有静音探针：pinned SDL原内部removed通知→Zombie消费，不等于OS拔插。
 * 复用已校正的quiet device_probe真实render/Put、replace、两bus和retire检查。 */
#include "audio.h"
#include "sdl_local_disconnect.h"
static int inject_local_disconnect(fd2_audio *audio);
#define FD2_DEVICE_PROBE_AFTER_SOURCES_READY(audio) inject_local_disconnect(audio)
#define main fd2_original_device_probe_main
#include "audio_device_probe.c"
#undef main
#undef FD2_DEVICE_PROBE_AFTER_SOURCES_READY

static fd2_private_disconnect_report loss;
static unsigned callbacks_before_loss, callbacks_after_loss;
static unsigned puts_before_loss, puts_after_loss;
static int loss_progress, backend_wasapi;

static int inject_local_disconnect(fd2_audio *audio) {
    const char *backend = SDL_GetCurrentAudioDriver();
    backend_wasapi = backend && !strcmp(backend, "wasapi");
    if (!backend_wasapi || !audio || !audio->stream ||
        fd2_private_disconnect_owned_stream(audio->stream, &loss) != 0) return -1;
    callbacks_before_loss = atomic_load(&observed_callbacks);
    puts_before_loss = atomic_load(&observed_put_successes);
    uint64_t deadline = SDL_GetTicks() + 2000;
    do {
        callbacks_after_loss = atomic_load(&observed_callbacks);
        puts_after_loss = atomic_load(&observed_put_successes);
        if (callbacks_after_loss > callbacks_before_loss && puts_after_loss > puts_before_loss) {
            loss_progress = 1;
            return 0;
        }
        SDL_Delay(2);
    } while (SDL_GetTicks() < deadline);
    return -1;
}

int main(int argc, char **argv) {
    if (argc == 1 || (argc == 2 && !strcmp(argv[1], "--dry-run"))) {
        puts("{\"schema\":\"fd2-a8-sdl-local-disconnect-probe-v1\",\"dry_run\":true,\"GUI_created\":false,\"device_opened\":false,\"SDL_Init_called\":false}");
        return 0;
    }
    if (argc != 3 || strcmp(argv[1], "--execute")) {
        fputs("usage: fd2_audio_device_loss_probe [--dry-run | --execute 100..10000_MS]\n", stderr);
        return 2;
    }
    char *end = NULL;
    unsigned long duration = strtoul(argv[2], &end, 10);
    if (!*argv[2] || *end || duration < 100 || duration > 10000) return 2;
    /* 只清/设本子进程环境；避免dynapi加载另一份SDL后内部对象不相容。 */
#ifdef _WIN32
    if (_putenv_s("SDL_DYNAMIC_API", "") != 0) return 1;
#endif
    SDL_Environment *env = SDL_GetEnvironment();
    if (!SDL_SetEnvironmentVariable(env, SDL_HINT_AUDIO_DRIVER, "wasapi", true)) return 1;
    int result = fd2_original_device_probe_main(argc, argv);
    int ok = result == 0 && backend_wasapi && loss_progress && loss.ref_acquired && loss.ref_released &&
             loss.owned_before && loss.owned_after && loss.default_logical &&
             loss.disconnected_called && !loss.zombie_before && loss.zombie_after == 1 &&
             !loss.shutdown_before && !loss.shutdown_after &&
             loss.wait_function_changed && loss.get_buffer_function_changed && loss.play_function_changed;
    printf("{\"schema\":\"fd2-a8-sdl-local-disconnect-probe-v1\",\"ok\":%s,\"base_probe_exit\":%d,\"backend_wasapi\":%s,\"logical_device_id\":%u,\"physical_device_id\":%u,\"own_before\":%d,\"own_after\":%d,\"opened_as_default\":%d,\"ref_acquired\":%d,\"ref_released\":%d,\"SDL_internal_disconnect_called\":%d,\"zombie_before\":%d,\"zombie_after\":%d,\"shutdown_before\":%d,\"shutdown_after\":%d,\"functions_changed\":{\"WaitDevice\":%d,\"GetDeviceBuf\":%d,\"PlayDevice\":%d},\"callbacks_before_loss\":%u,\"callbacks_after_loss\":%u,\"successful_puts_before_loss\":%u,\"successful_puts_after_loss\":%u,\"callback_progress_after_loss\":%s,\"test_quiet\":true,\"GUI_created\":false,\"global_device_or_mixer_changed\":false,\"physical_unplug_validated\":false,\"WASAPI_Recovery_validated\":false,\"default_route_change_validated\":false,\"audible_output_validated\":false,\"A7_validated\":false,\"A8_stage_complete\":false}\n",
        ok ? "true" : "false", result, backend_wasapi ? "true" : "false", (unsigned)loss.logical_id, (unsigned)loss.physical_id,
        loss.owned_before, loss.owned_after, loss.default_logical, loss.ref_acquired, loss.ref_released,
        loss.disconnected_called, loss.zombie_before, loss.zombie_after, loss.shutdown_before,
        loss.shutdown_after, loss.wait_function_changed, loss.get_buffer_function_changed,
        loss.play_function_changed, callbacks_before_loss, callbacks_after_loss,
        puts_before_loss, puts_after_loss, loss_progress ? "true" : "false");
    return ok ? 0 : 1;
}
