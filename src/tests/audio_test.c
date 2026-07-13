#include <math.h>
#include <stdio.h>
#include <stdatomic.h>
#include <string.h>

#include "audio.h"
#include <SDL3/SDL.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

typedef struct {
    float value;
    size_t frames_left;
    _Atomic int render_count;
    _Atomic int retire_count;
    _Atomic int last_reason;
} constant_source;

static size_t render_constant(void *userdata, float *stereo, size_t frames) {
    constant_source *source = userdata;
    atomic_fetch_add_explicit(&source->render_count, 1, memory_order_relaxed);
    size_t produced = frames;
    if (produced > source->frames_left) produced = source->frames_left;
    for (size_t i = 0; i < produced * 2u; i++) stereo[i] = source->value;
    source->frames_left -= produced;
    return produced;
}

static void retire_constant(void *userdata, fd2_audio_retire_reason reason) {
    constant_source *source = userdata;
    atomic_store_explicit(&source->last_reason, reason, memory_order_relaxed);
    atomic_fetch_add_explicit(&source->retire_count, 1, memory_order_release);
}

static fd2_audio_source constant(constant_source *source) {
    return (fd2_audio_source){
        .render = render_constant,
        .retire = retire_constant,
        .userdata = source,
    };
}

static int near(float a, float b) {
    return fabsf(a - b) < 0.00001f;
}

static int test_mix_and_bus_controls(void) {
    fd2_audio_config config = {.sample_rate = 32000, .open_device = 0};
    fd2_audio *audio = fd2_audio_create(&config);
    CHECK(audio && !fd2_audio_has_device(audio));
    CHECK(fd2_audio_sample_rate(audio) == 32000);

    constant_source music = {.value = 0.25f, .frames_left = 32};
    constant_source sfx = {.value = 0.5f, .frames_left = 32};
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_MUSIC,
          constant(&music), 1.0f) == 0);
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX,
          constant(&sfx), 1.0f) == 0);
    CHECK(fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_SFX, 0.5f) == 0);

    float output[16] = {0};
    CHECK(fd2_audio_render_offline(audio, output, 8) == 8);
    for (size_t i = 0; i < 16; i++) CHECK(near(output[i], 0.5f));

    CHECK(fd2_audio_stop_bus(audio, FD2_AUDIO_BUS_SFX) == 0);
    CHECK(fd2_audio_set_master_gain(audio, 0.5f) == 0);
    CHECK(atomic_load(&sfx.retire_count) == 0);
    memset(output, 0, sizeof(output));
    CHECK(fd2_audio_render_offline(audio, output, 8) == 8);
    for (size_t i = 0; i < 16; i++) CHECK(near(output[i], 0.125f));
    CHECK(atomic_load(&sfx.retire_count) == 1);
    CHECK(atomic_load(&sfx.last_reason) == FD2_AUDIO_RETIRE_STOPPED);

    fd2_audio_destroy(audio);
    CHECK(atomic_load(&music.retire_count) == 1);
    CHECK(atomic_load(&music.last_reason) == FD2_AUDIO_RETIRE_DESTROYED);
    return 0;
}

static int test_end_and_clipping(void) {
    fd2_audio_config config = {.open_device = 0};
    fd2_audio *audio = fd2_audio_create(&config);
    CHECK(audio);
    constant_source loud = {.value = 0.75f, .frames_left = 2};
    constant_source loud2 = {.value = 0.75f, .frames_left = 2};
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX,
          constant(&loud), 1.0f) == 0);
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX,
          constant(&loud2), 1.0f) == 0);
    float output[12];
    for (size_t i = 0; i < 12; i++) output[i] = -9.0f;
    CHECK(fd2_audio_render_offline(audio, output, 6) == 6);
    for (size_t i = 0; i < 4; i++) CHECK(near(output[i], 1.0f));
    for (size_t i = 4; i < 12; i++) CHECK(near(output[i], 0.0f));
    CHECK(atomic_load(&loud.retire_count) == 1);
    CHECK(atomic_load(&loud.last_reason) == FD2_AUDIO_RETIRE_FINISHED);
    CHECK(atomic_load(&loud2.retire_count) == 1);
    constant_source invalid = {.value = NAN, .frames_left = 1};
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX,
                                constant(&invalid), 1.0f) == 0);
    float sanitized[2] = {-1.0f, -1.0f};
    CHECK(fd2_audio_render_offline(audio, sanitized, 1) == 1);
    CHECK(near(sanitized[0], 0.0f) && near(sanitized[1], 0.0f));
    fd2_audio_destroy(audio);
    return 0;
}

static int test_replacement_and_voice_steal(void) {
    fd2_audio_config config = {.open_device = 0};
    fd2_audio *audio = fd2_audio_create(&config);
    CHECK(audio);
    constant_source music[2] = {
        {.value = 0.1f, .frames_left = 100},
        {.value = 0.2f, .frames_left = 100},
    };
    float frame[2];
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_MUSIC,
                                constant(&music[0]), 1.0f) == 0);
    CHECK(fd2_audio_render_offline(audio, frame, 1) == 1);
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_MUSIC,
                                constant(&music[1]), 1.0f) == 0);
    CHECK(fd2_audio_render_offline(audio, frame, 1) == 1);
    CHECK(atomic_load(&music[0].retire_count) == 1);
    CHECK(atomic_load(&music[0].last_reason) ==
          FD2_AUDIO_RETIRE_REPLACED);
    CHECK(near(frame[0], 0.2f));

    constant_source voices[17];
    memset(voices, 0, sizeof(voices));
    for (size_t i = 0; i < 17; i++) {
        voices[i].value = 0.01f;
        voices[i].frames_left = 100;
    }
    for (size_t i = 0; i < 15; i++)
        CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX,
                                    constant(&voices[i]), 1.0f) == 0);
    CHECK(fd2_audio_render_offline(audio, frame, 1) == 1);
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX,
                                constant(&voices[15]), 1.0f) == 0);
    CHECK(fd2_audio_render_offline(audio, frame, 1) == 1);
    CHECK(atomic_load(&voices[0].retire_count) == 1);
    CHECK(atomic_load(&voices[0].last_reason) == FD2_AUDIO_RETIRE_STOLEN);

    /* 所有 16 slots 被 music+SFX 占用时，仍只能抢占同 bus 最旧 voice。 */
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX,
                                constant(&voices[16]), 1.0f) == 0);
    CHECK(fd2_audio_render_offline(audio, frame, 1) == 1);
    CHECK(atomic_load(&voices[1].retire_count) == 1);
    fd2_audio_destroy(audio);
    return 0;
}

static int test_pending_source_retired_on_destroy(void) {
    fd2_audio_config config = {.open_device = 0};
    fd2_audio *audio = fd2_audio_create(&config);
    CHECK(audio);
    constant_source source = {.value = 0.1f, .frames_left = 10};
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX,
                                constant(&source), 1.0f) == 0);
    fd2_audio_destroy(audio);
    CHECK(atomic_load(&source.retire_count) == 1);
    CHECK(atomic_load(&source.last_reason) == FD2_AUDIO_RETIRE_DESTROYED);
    return 0;
}

static int test_dummy_device(void) {
    CHECK(SDL_InitSubSystem(SDL_INIT_AUDIO));
    fd2_audio_config config = {
        .sample_rate = 48000,
        .open_device = 1,
        .allow_null = 0,
    };
    fd2_audio *audio = fd2_audio_create(&config);
    CHECK(audio && fd2_audio_has_device(audio));
    constant_source source = {.value = 0.1f, .frames_left = 1000000};
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX,
                                constant(&source), 1.0f) == 0);
    for (int i = 0; i < 100 &&
         atomic_load_explicit(&source.render_count, memory_order_acquire) == 0;
         i++)
        SDL_Delay(10);
    CHECK(atomic_load_explicit(&source.render_count, memory_order_acquire) > 0);
    CHECK(fd2_audio_stop_bus(audio, FD2_AUDIO_BUS_SFX) == 0);
    for (int i = 0; i < 100 &&
         atomic_load_explicit(&source.retire_count, memory_order_acquire) == 0;
         i++)
        SDL_Delay(10);
    CHECK(atomic_load_explicit(&source.retire_count, memory_order_acquire) == 1);
    fd2_audio_destroy(audio);
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    return 0;
}

static int test_device_failure_null_fallback(void) {
    fd2_audio_config config = {
        .sample_rate = 48000,
        .open_device = 1,
        .allow_null = 1,
    };
    fd2_audio *audio = fd2_audio_create(&config);
    CHECK(audio && !fd2_audio_has_device(audio));
    constant_source source = {.value = 0.1f, .frames_left = 10};
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX,
                                constant(&source), 1.0f) == 0);
    CHECK(atomic_load(&source.retire_count) == 1);
    CHECK(atomic_load(&source.last_reason) == FD2_AUDIO_RETIRE_DROPPED);
    CHECK(fd2_audio_stop_bus(audio, FD2_AUDIO_BUS_SFX) == 0);
    fd2_audio_destroy(audio);
    return 0;
}

static int test_validation_and_bounded_queue(void) {
    fd2_audio_config config = {.open_device = 0};
    fd2_audio *audio = fd2_audio_create(&config);
    CHECK(audio);
    CHECK(fd2_audio_set_bus_gain(audio, FD2_AUDIO_BUS_COUNT, 1.0f) != 0);
    CHECK(fd2_audio_set_master_gain(audio, -1.0f) != 0);
    CHECK(fd2_audio_set_master_gain(audio, NAN) != 0);
    CHECK(fd2_audio_play_source(audio, FD2_AUDIO_BUS_SFX,
          (fd2_audio_source){0}, 1.0f) != 0);
    CHECK(fd2_audio_set_master_gain(audio, 17.0f) != 0);

    for (size_t i = 0; i < 64; i++)
        CHECK(fd2_audio_set_master_gain(audio, 1.0f) == 0);
    CHECK(fd2_audio_set_master_gain(audio, 1.0f) != 0);
    float sample[2];
    CHECK(fd2_audio_render_offline(audio, sample, 1) == 1);
    CHECK(fd2_audio_set_master_gain(audio, 1.0f) == 0);
    fd2_audio_destroy(audio);
    return 0;
}

int main(void) {
    CHECK(test_mix_and_bus_controls() == 0);
    CHECK(test_end_and_clipping() == 0);
    CHECK(test_replacement_and_voice_steal() == 0);
    CHECK(test_pending_source_retired_on_destroy() == 0);
    CHECK(test_dummy_device() == 0);
    CHECK(test_device_failure_null_fallback() == 0);
    CHECK(test_validation_and_bounded_queue() == 0);
    puts("audio tests: ok");
    return 0;
}
