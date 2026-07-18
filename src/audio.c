/* 炎龙骑士团 2 SDL3 重写 - 统一音频设备、bus 与 source 混音层
 *
 * 原版依据：music_track_play @0x4ab8b 管理 XMIDI sequence，
 * sfx_play @0x4acaa 管理数字 sample。SDL 重写不复刻 AIL 驱动，二者统一
 * 渲染为 float32 stereo 后交给 SDL_AudioStream。
 */

#include "audio.h"

#include <math.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>

#define FD2_AUDIO_MAX_VOICES 16u
#define FD2_AUDIO_COMMAND_CAPACITY 64u
#define FD2_AUDIO_MIX_CHUNK 512u
#define FD2_AUDIO_MAX_GAIN 16.0f

typedef enum {
    FD2_AUDIO_COMMAND_PLAY,
    FD2_AUDIO_COMMAND_STOP_BUS,
    FD2_AUDIO_COMMAND_BUS_GAIN,
    FD2_AUDIO_COMMAND_MASTER_GAIN,
} fd2_audio_command_type;

typedef struct {
    fd2_audio_command_type type;
    fd2_audio_bus bus;
    fd2_audio_source source;
    float gain;
} fd2_audio_command;

typedef struct {
    int active;
    fd2_audio_bus bus;
    fd2_audio_source source;
    float gain;
    uint64_t serial;
} fd2_audio_voice;

struct fd2_audio {
    int sample_rate;
    SDL_AudioStream *stream;
    int discard_output;
    fd2_audio_voice voices[FD2_AUDIO_MAX_VOICES];
    float bus_gain[FD2_AUDIO_BUS_COUNT];
    float master_gain;
    uint64_t next_serial;
    fd2_audio_command commands[FD2_AUDIO_COMMAND_CAPACITY];
    _Atomic uint32_t command_read;
    _Atomic uint32_t command_write;
    float source_buffer[FD2_AUDIO_MIX_CHUNK * 2u];
    float mix_buffer[FD2_AUDIO_MIX_CHUNK * 2u];
};

static int valid_gain(float gain) {
    return isfinite(gain) && gain >= 0.0f && gain <= FD2_AUDIO_MAX_GAIN;
}

static void source_retire(fd2_audio_source source,
                          fd2_audio_retire_reason reason) {
    if (source.retire) source.retire(source.userdata, reason);
}

static void voice_retire(fd2_audio_voice *voice,
                         fd2_audio_retire_reason reason) {
    if (!voice || !voice->active) return;
    fd2_audio_source source = voice->source;
    memset(voice, 0, sizeof(*voice));
    source_retire(source, reason);
}

static int command_push(fd2_audio *audio, const fd2_audio_command *command) {
    if (!audio || !command) return -1;
    if (audio->discard_output) {
        if (command->type == FD2_AUDIO_COMMAND_PLAY)
            source_retire(command->source, FD2_AUDIO_RETIRE_DROPPED);
        else if (command->type == FD2_AUDIO_COMMAND_BUS_GAIN)
            audio->bus_gain[command->bus] = command->gain;
        else if (command->type == FD2_AUDIO_COMMAND_MASTER_GAIN)
            audio->master_gain = command->gain;
        return 0;
    }
    uint32_t write = atomic_load_explicit(&audio->command_write,
                                          memory_order_relaxed);
    uint32_t read = atomic_load_explicit(&audio->command_read,
                                         memory_order_acquire);
    if ((uint32_t)(write - read) >= FD2_AUDIO_COMMAND_CAPACITY)
        return -1;
    audio->commands[write % FD2_AUDIO_COMMAND_CAPACITY] = *command;
    atomic_store_explicit(&audio->command_write, write + 1u,
                          memory_order_release);
    return 0;
}

static fd2_audio_voice *voice_for_play(fd2_audio *audio, fd2_audio_bus bus) {
    if (bus == FD2_AUDIO_BUS_MUSIC) {
        for (size_t i = 0; i < FD2_AUDIO_MAX_VOICES; i++) {
            if (audio->voices[i].active &&
                audio->voices[i].bus == FD2_AUDIO_BUS_MUSIC)
                voice_retire(&audio->voices[i], FD2_AUDIO_RETIRE_REPLACED);
        }
    }
    for (size_t i = 0; i < FD2_AUDIO_MAX_VOICES; i++) {
        if (!audio->voices[i].active) return &audio->voices[i];
    }

    /* voice 满时只抢占同 bus 最旧项，避免一个 bus 意外停止另一个 bus。 */
    fd2_audio_voice *oldest = NULL;
    for (size_t i = 0; i < FD2_AUDIO_MAX_VOICES; i++) {
        fd2_audio_voice *voice = &audio->voices[i];
        if (voice->bus == bus && (!oldest || voice->serial < oldest->serial))
            oldest = voice;
    }
    if (oldest) voice_retire(oldest, FD2_AUDIO_RETIRE_STOLEN);
    return oldest;
}

static void command_apply(fd2_audio *audio,
                          const fd2_audio_command *command) {
    switch (command->type) {
        case FD2_AUDIO_COMMAND_PLAY: {
            fd2_audio_voice *voice = voice_for_play(audio, command->bus);
            if (!voice) {
                source_retire(command->source, FD2_AUDIO_RETIRE_DROPPED);
                break;
            }
            voice->active = 1;
            voice->bus = command->bus;
            voice->source = command->source;
            voice->gain = command->gain;
            voice->serial = audio->next_serial++;
            break;
        }
        case FD2_AUDIO_COMMAND_STOP_BUS:
            for (size_t i = 0; i < FD2_AUDIO_MAX_VOICES; i++) {
                if (audio->voices[i].active &&
                    audio->voices[i].bus == command->bus)
                    voice_retire(&audio->voices[i], FD2_AUDIO_RETIRE_STOPPED);
            }
            break;
        case FD2_AUDIO_COMMAND_BUS_GAIN:
            audio->bus_gain[command->bus] = command->gain;
            break;
        case FD2_AUDIO_COMMAND_MASTER_GAIN:
            audio->master_gain = command->gain;
            break;
    }
}

static void commands_consume(fd2_audio *audio) {
    uint32_t read = atomic_load_explicit(&audio->command_read,
                                         memory_order_relaxed);
    uint32_t write = atomic_load_explicit(&audio->command_write,
                                          memory_order_acquire);
    while (read != write) {
        command_apply(audio,
                      &audio->commands[read % FD2_AUDIO_COMMAND_CAPACITY]);
        read++;
    }
    atomic_store_explicit(&audio->command_read, read, memory_order_release);
}

static void render_chunk(fd2_audio *audio, float *stereo, size_t frames) {
    commands_consume(audio);
    memset(stereo, 0, frames * 2u * sizeof(*stereo));
    for (size_t i = 0; i < FD2_AUDIO_MAX_VOICES; i++) {
        fd2_audio_voice *voice = &audio->voices[i];
        if (!voice->active || !voice->source.render) continue;
        memset(audio->source_buffer, 0,
               frames * 2u * sizeof(*audio->source_buffer));
        size_t produced = voice->source.render(
            voice->source.userdata, audio->source_buffer, frames);
        if (produced > frames) produced = frames;
        float gain = voice->gain * audio->bus_gain[voice->bus];
        for (size_t sample = 0; sample < produced * 2u; sample++) {
            float value = audio->source_buffer[sample];
            if (isfinite(value)) stereo[sample] += value * gain;
        }
        if (produced < frames)
            voice_retire(voice, FD2_AUDIO_RETIRE_FINISHED);
    }
    for (size_t sample = 0; sample < frames * 2u; sample++) {
        float value = stereo[sample] * audio->master_gain;
        if (!isfinite(value)) value = 0.0f;
        else if (value > 1.0f) value = 1.0f;
        else if (value < -1.0f) value = -1.0f;
        stereo[sample] = value;
    }
}

static void SDLCALL audio_stream_callback(void *userdata,
                                          SDL_AudioStream *stream,
                                          int additional_amount,
                                          int total_amount) {
    (void)total_amount;
    fd2_audio *audio = userdata;
    const int frame_bytes = (int)(sizeof(float) * 2u);
    int frames_left = additional_amount / frame_bytes;
    while (audio && frames_left > 0) {
        size_t frames = (size_t)frames_left;
        if (frames > FD2_AUDIO_MIX_CHUNK) frames = FD2_AUDIO_MIX_CHUNK;
        render_chunk(audio, audio->mix_buffer, frames);
        if (!SDL_PutAudioStreamData(stream, audio->mix_buffer,
                                    (int)(frames * (size_t)frame_bytes)))
            return;
        frames_left -= (int)frames;
    }
}

fd2_audio *fd2_audio_create(const fd2_audio_config *config) {
    int sample_rate = config && config->sample_rate > 0
                    ? config->sample_rate : 48000;
    int open_device = config ? config->open_device : 1;
    int allow_null = config ? config->allow_null : 1;
    fd2_audio *audio = calloc(1, sizeof(*audio));
    if (!audio) return NULL;
    audio->sample_rate = sample_rate;
    audio->master_gain = 1.0f;
    audio->bus_gain[FD2_AUDIO_BUS_MUSIC] = 1.0f;
    audio->bus_gain[FD2_AUDIO_BUS_SFX] = 1.0f;
    audio->next_serial = 1;
    atomic_init(&audio->command_read, 0);
    atomic_init(&audio->command_write, 0);

    if (open_device) {
        SDL_AudioSpec spec = {
            .format = SDL_AUDIO_F32,
            .channels = 2,
            .freq = sample_rate,
        };
        audio->stream = SDL_OpenAudioDeviceStream(
            SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec,
            audio_stream_callback, audio);
        if (audio->stream && !SDL_ResumeAudioStreamDevice(audio->stream)) {
            SDL_DestroyAudioStream(audio->stream);
            audio->stream = NULL;
        }
        if (!audio->stream && !allow_null) {
            free(audio);
            return NULL;
        }
        if (!audio->stream) audio->discard_output = 1;
    }
    return audio;
}

void fd2_audio_destroy(fd2_audio *audio) {
    if (!audio) return;
    if (audio->stream) {
        /* 先暂停并销毁 stream，使实时回调退出，再销毁 source userdata。 */
        SDL_PauseAudioStreamDevice(audio->stream);
        SDL_DestroyAudioStream(audio->stream);
        audio->stream = NULL;
    }
    for (size_t i = 0; i < FD2_AUDIO_MAX_VOICES; i++)
        voice_retire(&audio->voices[i], FD2_AUDIO_RETIRE_DESTROYED);
    uint32_t read = atomic_load_explicit(&audio->command_read,
                                         memory_order_relaxed);
    uint32_t write = atomic_load_explicit(&audio->command_write,
                                          memory_order_relaxed);
    while (read != write) {
        fd2_audio_command *command =
            &audio->commands[read % FD2_AUDIO_COMMAND_CAPACITY];
        if (command->type == FD2_AUDIO_COMMAND_PLAY)
            source_retire(command->source, FD2_AUDIO_RETIRE_DESTROYED);
        read++;
    }
    free(audio);
}

int fd2_audio_has_device(const fd2_audio *audio) {
    return audio && audio->stream != NULL;
}

int fd2_audio_sample_rate(const fd2_audio *audio) {
    return audio ? audio->sample_rate : 0;
}

int fd2_audio_play_source(fd2_audio *audio, fd2_audio_bus bus,
                          fd2_audio_source source, float gain) {
    if (!audio || bus < 0 || bus >= FD2_AUDIO_BUS_COUNT ||
        !source.render || !valid_gain(gain))
        return -1;
    fd2_audio_command command = {
        .type = FD2_AUDIO_COMMAND_PLAY,
        .bus = bus,
        .source = source,
        .gain = gain,
    };
    return command_push(audio, &command);
}

int fd2_audio_stop_bus(fd2_audio *audio, fd2_audio_bus bus) {
    if (!audio || bus < 0 || bus >= FD2_AUDIO_BUS_COUNT) return -1;
    fd2_audio_command command = {
        .type = FD2_AUDIO_COMMAND_STOP_BUS,
        .bus = bus,
    };
    return command_push(audio, &command);
}

int fd2_audio_play_music_source(fd2_audio *audio, fd2_audio_source source,
                                float gain) {
    return fd2_audio_play_source(audio, FD2_AUDIO_BUS_MUSIC, source, gain);
}

int fd2_audio_stop_music(fd2_audio *audio) {
    return fd2_audio_stop_bus(audio, FD2_AUDIO_BUS_MUSIC);
}

int fd2_audio_set_bus_gain(fd2_audio *audio, fd2_audio_bus bus, float gain) {
    if (!audio || bus < 0 || bus >= FD2_AUDIO_BUS_COUNT || !valid_gain(gain))
        return -1;
    fd2_audio_command command = {
        .type = FD2_AUDIO_COMMAND_BUS_GAIN,
        .bus = bus,
        .gain = gain,
    };
    return command_push(audio, &command);
}

int fd2_audio_set_master_gain(fd2_audio *audio, float gain) {
    if (!audio || !valid_gain(gain)) return -1;
    fd2_audio_command command = {
        .type = FD2_AUDIO_COMMAND_MASTER_GAIN,
        .gain = gain,
    };
    return command_push(audio, &command);
}

size_t fd2_audio_render_offline(fd2_audio *audio, float *stereo,
                                size_t frames) {
    if (!audio || !stereo || audio->stream) return 0;
    size_t rendered = 0;
    while (rendered < frames) {
        size_t chunk = frames - rendered;
        if (chunk > FD2_AUDIO_MIX_CHUNK) chunk = FD2_AUDIO_MIX_CHUNK;
        render_chunk(audio, stereo + rendered * 2u, chunk);
        rendered += chunk;
    }
    return rendered;
}
