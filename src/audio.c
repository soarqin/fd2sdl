/* 统一Audio owner：music_track_play @0x25977 / sfx_play @0x25a96。
 * 源render/retire与控制仅在App线程执行；PIT前缀沿bios_clock同一时间轴。
 * SDL只消费AudioStream队列，不生成未来声音。保留采样、叠加、尾音和设备清理。 */
#include "audio.h"
#include "settings.h"
#include "audio_pcm_capture.h"
#include "bios_clock.h"
#include "audio_sb.h"
#include "mixdown_dc.h"
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#define FD2_AUDIO_MAX_VOICES 16u
#define FD2_AUDIO_MIX_CHUNK 512u
#define FD2_AUDIO_MAX_GAIN 16.0f
#define NS_PER_SECOND UINT64_C(1000000000)
static const float fd2_test_audio_silence[FD2_AUDIO_MIX_CHUNK * 2u] = {0};

typedef struct {
    int active;
    fd2_audio_bus bus;
    fd2_audio_source source;
    float gain;
    uint64_t serial;
} fd2_audio_voice;
struct fd2_audio {
    int sample_rate, offline, test_quiet;
    SDL_ThreadID thread;
    SDL_AudioStream *stream;
    fd2_audio_pcm_capture *capture;
    fd2_audio_voice voices[FD2_AUDIO_MAX_VOICES];
    float bus_gain[FD2_AUDIO_BUS_COUNT], master_gain;
    fd2_audio_error first_error;
    uint64_t queue_samples, queue_peak_bytes, successful_puts, backlog_drop_chunks;
    uint64_t next_serial, origin_ns, now_ns, closed_frames, edge_ns;
    uint32_t edge_fraction;
    int in_callback, closing, prefix_bound;
    /* 同Audio的原1ms设备phase/work/DC；不是第二BIOS或PCM队列。
     * origin处新ms锚/零DC为显式H启动政策，真实首phase仍待资格。 */
    int original_mixer, original_bound, original_busy;
    int original_quiesced, original_quiescing;
    uint64_t original_ticks;
    size_t original_rendered;
    fd2_audio_sb original_sb;
    fd2_sb_point original_point;
    fd2_mixdown_dc original_dc;
    int32_t original_sb_work[48][2];
    /* 只借当前offline调用者输出，不缓存PCM、不建立第二renderer。 */
    float *offline_output;
    size_t offline_capacity, offline_written;
    float source_buffer[FD2_AUDIO_MIX_CHUNK * 2u];
    float mix_buffer[FD2_AUDIO_MIX_CHUNK * 2u];
};
/* prefix只有一个绑定，两个Audio/offline scope不得争抢同一clock。 */
static fd2_audio *active_audio;
static int audio_creating; /* 同singleton构造期，纯point callback也不能重入创建。 */
static int valid_gain(float value) {
    return isfinite(value) && value >= 0.0f && value <= FD2_AUDIO_MAX_GAIN;
}
static int owner(const fd2_audio *audio) {
    return audio && active_audio == audio && !audio->closing &&
        !audio->in_callback && audio->thread == SDL_GetCurrentThreadID();
}
static void record_first_error(fd2_audio *audio, fd2_audio_error error) {
    if (audio->first_error == FD2_AUDIO_ERROR_NONE) audio->first_error = error;
}
static void retire(fd2_audio *audio, fd2_audio_source source,
                   fd2_audio_retire_reason reason) {
    if (source.retire) {
        audio->in_callback++;
        source.retire(source.userdata, reason);
        audio->in_callback--;
    }
}
static void voice_retire(fd2_audio *audio, fd2_audio_voice *voice,
                         fd2_audio_retire_reason reason) {
    if (!voice->active) return;
    /* 原profile旧chip在首端口写前仍是render source，先关partial再注销。 */
    if(audio->original_mixer && !audio->original_busy && !audio->original_quiesced && !audio->closing)
        (void)fd2_audio_original_fill_up(audio);
    fd2_audio_source source = voice->source;
    memset(voice, 0, sizeof(*voice));
    retire(audio, source, reason);
}
static fd2_audio_voice *voice_for_play(fd2_audio *audio, fd2_audio_bus bus) {
    if (bus == FD2_AUDIO_BUS_MUSIC)
        for (size_t i = 0; i < FD2_AUDIO_MAX_VOICES; i++)
            if (audio->voices[i].active && audio->voices[i].bus == bus)
                voice_retire(audio, &audio->voices[i], FD2_AUDIO_RETIRE_REPLACED);
    for (size_t i = 0; i < FD2_AUDIO_MAX_VOICES; i++)
        if (!audio->voices[i].active) return &audio->voices[i];
    fd2_audio_voice *oldest = NULL;
    for (size_t i = 0; i < FD2_AUDIO_MAX_VOICES; i++) {
        fd2_audio_voice *voice = &audio->voices[i];
        if (voice->bus == bus && (!oldest || voice->serial < oldest->serial))
            oldest = voice;
    }
    if (oldest) voice_retire(audio, oldest, FD2_AUDIO_RETIRE_STOLEN);
    return oldest;
}
static void render_chunk(fd2_audio *audio, float *stereo, size_t frames) {
    memset(stereo, 0, frames * 2u * sizeof(*stereo));
    for (size_t i = 0; i < FD2_AUDIO_MAX_VOICES; i++) {
        fd2_audio_voice *voice = &audio->voices[i];
        if (!voice->active) continue;
        memset(audio->source_buffer, 0, frames * 2u * sizeof(float));
        audio->in_callback++;
        size_t produced = voice->source.render(voice->source.userdata,
                                               audio->source_buffer, frames);
        audio->in_callback--;
        if (produced > frames) produced = frames;
        float gain = voice->gain * audio->bus_gain[voice->bus];
        for (size_t sample = 0; sample < produced * 2u; sample++) {
            float value = audio->source_buffer[sample];
            if (isfinite(value)) stereo[sample] += value * gain;
        }
        if (produced < frames)
            voice_retire(audio, voice, FD2_AUDIO_RETIRE_FINISHED);
    }
    for (size_t sample = 0; sample < frames * 2u; sample++) {
        float value = stereo[sample] * audio->master_gain;
        if (!isfinite(value)) value = 0.0f;
        else if (value > 1.0f) value = 1.0f;
        else if (value < -1.0f) value = -1.0f;
        stereo[sample] = value;
    }
}
/* sample n发生于origin+n/rate秒，只关闭严格早于exact edge的n>=0。
 * 商余分两级，最大rate=1e9仍不溢出uint64；禁止先ceil(edge_ns)。 */
static int prefix_frames(const fd2_audio *audio, uint64_t whole,
                          uint32_t fraction, uint64_t *frames) {
    if (fraction >= FD2_BIOS_PIT_RATE || whole < audio->origin_ns) return -1;
    uint64_t elapsed = whole - audio->origin_ns;
    uint64_t seconds = elapsed / NS_PER_SECOND;
    uint64_t rate = (uint64_t)audio->sample_rate;
    if (seconds > UINT64_MAX / rate) return -1;
    uint64_t base = seconds * rate;
    uint64_t rem = (elapsed % NS_PER_SECOND) * rate;
    uint64_t partial = rem / NS_PER_SECOND;
    uint64_t numerator = (rem % NS_PER_SECOND) * FD2_BIOS_PIT_RATE +
                          (uint64_t)fraction * rate;
    uint64_t denominator = NS_PER_SECOND * FD2_BIOS_PIT_RATE;
    uint64_t tail = numerator / denominator + (numerator % denominator != 0u);
    if (partial > UINT64_MAX - base || tail > UINT64_MAX - base - partial)
        return -1;
    *frames = base + partial + tail;
    return 0;
}
static uint64_t queue_capacity_frames(const fd2_audio *audio) {
    /* H：500ms输入F32队列（最低一帧）；SDL int长度再界定极端host率。 */
    uint64_t frames = ((uint64_t)audio->sample_rate + 1u) / 2u;
    uint64_t limit = INT_MAX / (2u * sizeof(float));
    return frames < limit ? frames : limit;
}
static void submit_chunk(fd2_audio *audio, size_t frames) {

    if (audio->offline_output) {
        size_t available = audio->offline_capacity - audio->offline_written;
        if (frames > available) {
            record_first_error(audio, FD2_AUDIO_ERROR_CLOCK);
            return;
        }
        memcpy(audio->offline_output + audio->offline_written * 2u,
               audio->mix_buffer, frames * 2u * sizeof(float));
        audio->offline_written += frames;
        return;
    }
    if (!audio->stream) return; /* null已render，仅不作设备提交。 */
    int queued = SDL_GetAudioStreamQueued(audio->stream);
    uint64_t bytes = (uint64_t)frames * 2u * sizeof(float);
    uint64_t cap = queue_capacity_frames(audio) * 2u * sizeof(float);
    if (queued < 0) {
        record_first_error(audio, FD2_AUDIO_ERROR_PUT);
        fd2_audio_pcm_capture_put_failed(audio->capture);
        return;
    }
    audio->queue_samples++;
    if ((uint64_t)queued > audio->queue_peak_bytes) audio->queue_peak_bytes=(uint64_t)queued;
    if ((uint64_t)queued > cap || bytes > cap - (uint64_t)queued) {
        record_first_error(audio, FD2_AUDIO_ERROR_BACKLOG);
        audio->backlog_drop_chunks++;
        fd2_audio_pcm_capture_output_dropped(audio->capture);
        return; /* 保已提交前缀，丢新输出；源/IRQ继续，不追补旧声音。 */
    }
    const float *submitted = audio->test_quiet ? fd2_test_audio_silence :
        fd2_audio_pcm_capture_submission(audio->capture, audio->mix_buffer, frames);
    if (!submitted) {
        record_first_error(audio, FD2_AUDIO_ERROR_PUT);
        fd2_audio_pcm_capture_put_failed(audio->capture);
        return;
    }
    if (!SDL_PutAudioStreamData(audio->stream, submitted, (int)bytes)) {
        record_first_error(audio, FD2_AUDIO_ERROR_PUT);
        fd2_audio_pcm_capture_put_failed(audio->capture);
        return;
    }
    /* v3：主线程成功Put后的mix，不是callback、可听或设备时钟证据。 */
    audio->successful_puts++;
    fd2_audio_pcm_capture_push(audio->capture, audio->mix_buffer, frames);
}
#include "audio_original.h"

static void audio_prefix(void *user, uint64_t whole, uint32_t fraction) {
    fd2_audio *audio = user;
    uint64_t target=0;
    if (!owner(audio) || whole < audio->edge_ns ||
        (whole == audio->edge_ns && fraction < audio->edge_fraction) ||
        fraction >= FD2_BIOS_PIT_RATE || whole < audio->origin_ns ||
        (!audio->original_mixer && (prefix_frames(audio, whole, fraction, &target) != 0 ||
        target < audio->closed_frames))) {
        record_first_error(audio, FD2_AUDIO_ERROR_CLOCK);
        return; /* 不把输出错误返回给BIOS或停止BDA/IRQ。 */
    }
    audio->edge_ns = audio->now_ns = whole;
    audio->edge_fraction = fraction;
    if(audio->original_mixer) {
        original_prefix(audio,whole,fraction);
        return;
    }
    while (audio->closed_frames < target) {
        uint64_t remaining = target - audio->closed_frames;
        size_t chunk = remaining < FD2_AUDIO_MIX_CHUNK ?
                       (size_t)remaining : FD2_AUDIO_MIX_CHUNK;
        uint64_t capacity = queue_capacity_frames(audio);
        if ((uint64_t)chunk > capacity) chunk = (size_t)capacity;
        render_chunk(audio, audio->mix_buffer, chunk);
        audio->closed_frames += chunk;
        submit_chunk(audio, chunk);
    }
}
uint64_t fd2_audio_now_ns(const fd2_audio *audio) {
    if (!audio) return 0;
    return audio->offline ? audio->now_ns : SDL_GetTicksNS();
}
int fd2_audio_sync(fd2_audio *audio) {
    if (!owner(audio) || audio->original_quiesced) return -1;
    if (fd2_bios_clock_advance(fd2_audio_now_ns(audio)) != 0) {
        record_first_error(audio, FD2_AUDIO_ERROR_CLOCK);
        return -1;
    }
    return 0; /* 输出首错另查get_first_error，不阻控制/源逻辑。 */
}
fd2_audio *fd2_audio_create(const fd2_audio_config *config) {
    int sample_rate = config && config->sample_rate > 0 ? config->sample_rate : 48000;
    int open_device = config ? config->open_device : 1;
    int allow_null = config ? config->allow_null : 1;
    int original=config?config->original_mixer:0;
    if (active_audio || audio_creating || sample_rate > 1000000000 ||
        (original!=0 && original!=1) || (original && sample_rate!=48000)) {
        SDL_SetError("audio requires one owner and a rate <= 1e9 Hz");
        return NULL;
    }
    fd2_audio *audio = calloc(1, sizeof(*audio));
    if (!audio) return NULL;
    audio_creating=1;
    audio->sample_rate = sample_rate;
    audio->original_mixer = original;
    audio->offline = !open_device;
    audio->thread = SDL_GetCurrentThreadID();
    audio->master_gain = 1.0f;
    audio->next_serial = 1;
    for (size_t i = 0; i < FD2_AUDIO_BUS_COUNT; i++) audio->bus_gain[i] = 1.0f;
    const char *test_quiet = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "FD2SDL_TEST_AUDIO_QUIET");
    if (test_quiet && strcmp(test_quiet, "0") && strcmp(test_quiet, "1")) {
        SDL_SetError("test audio quiet must be 0 or 1"); goto failed;
    }
    const fd2_settings *settings = fd2_get_settings();
    audio->test_quiet = settings->mute || (test_quiet && strcmp(test_quiet, "1") == 0);
    const char *capture_dir = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "FD2SDL_APP_PCM_CAPTURE_DIR");
    unsigned capture_ms = 45000;
    if (capture_dir && *capture_dir) {
        const char *value = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "FD2SDL_APP_PCM_CAPTURE_MAX_MS");
        if (value) {
            char *end; unsigned long n = strtoul(value, &end, 10);
            if (!*value || *end || !n || n > 120000) {
                SDL_SetError("FD2SDL_APP_PCM_CAPTURE_MAX_MS must be 1..120000"); goto failed;
            }
            capture_ms = (unsigned)n;
        }
        if (!open_device) { SDL_SetError("app PCM requires a playback stream"); goto failed; }
    }
    if (open_device) {
        SDL_AudioSpec spec = { .format = SDL_AUDIO_F32, .channels = 2, .freq = sample_rate };
        SDL_AudioDeviceID device = fd2_output_device(settings->device);
        audio->stream = SDL_OpenAudioDeviceStream(device, &spec, NULL, NULL);
        if (!audio->stream && device != SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK) {
            SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "cannot open %s; trying default", settings->device);
            audio->stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
        }
        /* 主音量只在设备流上应用，原混音、采集点与 AIL 时钟保持各自的顺序。 */
        if (audio->stream && settings->volume != 100 &&
            !SDL_SetAudioStreamGain(audio->stream, (float)settings->volume / 100.0f)) {
            SDL_DestroyAudioStream(audio->stream);
            audio->stream = NULL;
        }
        if (!audio->stream) record_first_error(audio, FD2_AUDIO_ERROR_OPEN);
        if (capture_dir && *capture_dir) {
            const char *quiet = SDL_GetEnvironmentVariable(SDL_GetEnvironment(), "FD2SDL_APP_PCM_CAPTURE_QUIET");
            int suppress = audio->test_quiet || (quiet && strcmp(quiet, "1") == 0);
            if ((quiet && strcmp(quiet, "0") && strcmp(quiet, "1")) || !audio->stream ||
                (audio->capture = fd2_audio_pcm_capture_open(capture_dir, sample_rate, capture_ms, suppress)) == NULL)
                goto failed;
        }
        if (audio->stream && !SDL_ResumeAudioStreamDevice(audio->stream)) {
            record_first_error(audio, FD2_AUDIO_ERROR_RESUME);
            SDL_DestroyAudioStream(audio->stream); audio->stream = NULL;
            if (audio->capture) goto failed;
        }
        if (!audio->stream && !allow_null) goto failed;
    }
    /* channel属于Audio lifetime，未绑定音效时也保留持续混音窗口。 */
    if(original && fd2_audio_sb_construct(&audio->original_sb,&audio->original_point,
        8192,8192,original_global_fill,audio)<0)goto failed;
    if (audio->offline) fd2_bios_clock_reset(0, 0);
    audio->now_ns = audio->origin_ns = audio->edge_ns = audio->offline ? 0 : SDL_GetTicksNS();
    /* Main线程关闭已到时刻的声音前缀；设备只消费Stream，不推进游戏。 */
    if (fd2_bios_clock_advance(audio->now_ns) != 0 ||
        fd2_bios_clock_set_audio_prefix(audio_prefix, audio) != 0) {
        SDL_SetError("audio requires the initialized BIOS owner"); goto failed;
    }
    audio->prefix_bound=1;
    active_audio = audio;audio_creating=0;
    return audio;
failed:
    if (audio->stream) {
        SDL_PauseAudioStreamDevice(audio->stream);
        SDL_DestroyAudioStream(audio->stream);
    }
    fd2_audio_pcm_capture_close(audio->capture);
    free(audio);audio_creating=0;
    return NULL;
}
void fd2_audio_destroy(fd2_audio *audio) {
    if (!audio || !owner(audio)) return;
    if(audio->original_mixer && !audio->original_quiesced)
        (void)fd2_audio_quiesce_dig(audio);
    audio->closing = 1;
    if (audio->stream) {
        SDL_PauseAudioStreamDevice(audio->stream);
        SDL_DestroyAudioStream(audio->stream);
        audio->stream = NULL;
    }
    /* 先停设备再解绑；retire/observer close后方可释放本owner。 */
    if(audio->prefix_bound)(void)fd2_bios_clock_set_audio_prefix(NULL, NULL);
    active_audio = NULL;
    if(audio->original_mixer)(void)fd2_audio_sb_close(&audio->original_sb);
    for (size_t i = 0; i < FD2_AUDIO_MAX_VOICES; i++)
        voice_retire(audio, &audio->voices[i], FD2_AUDIO_RETIRE_DESTROYED);
    fd2_audio_pcm_capture_close(audio->capture);
    free(audio);
}
int fd2_audio_has_device(const fd2_audio *audio) { return audio && audio->stream != NULL; }
int fd2_audio_sample_rate(const fd2_audio *audio) { return audio ? audio->sample_rate : 0; }
int fd2_audio_get_first_error(const fd2_audio *audio, fd2_audio_error *error) {
    if (!audio || !error) return -1;
    *error = audio->first_error; return 0;
}
int fd2_audio_get_transport_stats(const fd2_audio *audio,fd2_audio_transport_stats *stats) {
    if(!owner(audio)||!stats)return -1;
    *stats=(fd2_audio_transport_stats){audio->first_error,audio->queue_samples,
        audio->queue_peak_bytes,audio->successful_puts,audio->backlog_drop_chunks};
    return 0;
}
int fd2_audio_play_source(fd2_audio *audio, fd2_audio_bus bus,
                          fd2_audio_source source, float gain) {
    if (!owner(audio) || bus < 0 || bus >= FD2_AUDIO_BUS_COUNT ||
        !source.render || !valid_gain(gain) ||
        (audio->original_mixer && (bus!=FD2_AUDIO_BUS_MUSIC || gain!=1.0f)) ||
        fd2_audio_sync(audio) != 0) return -1;
    fd2_audio_voice *voice = voice_for_play(audio, bus);
    if (!voice) { retire(audio, source, FD2_AUDIO_RETIRE_DROPPED); return 0; }
    voice->active = 1; voice->bus = bus; voice->source = source;
    voice->gain = gain; voice->serial = audio->next_serial++;
    return 0;
}
int fd2_audio_stop_bus(fd2_audio *audio, fd2_audio_bus bus) {
    if (!owner(audio) || bus < 0 || bus >= FD2_AUDIO_BUS_COUNT || fd2_audio_sync(audio) != 0) return -1;
    for (size_t i = 0; i < FD2_AUDIO_MAX_VOICES; i++)
        if (audio->voices[i].active && audio->voices[i].bus == bus)
            voice_retire(audio, &audio->voices[i], FD2_AUDIO_RETIRE_STOPPED);
    return 0;
}
int fd2_audio_set_bus_gain(fd2_audio *audio, fd2_audio_bus bus, float gain) {
    if (!owner(audio) || bus < 0 || bus >= FD2_AUDIO_BUS_COUNT ||
        !valid_gain(gain) || (audio->original_mixer && gain!=1.0f) ||
        fd2_audio_sync(audio) != 0) return -1;
    audio->bus_gain[bus] = gain; return 0;
}
int fd2_audio_get_bus_gain_target(const fd2_audio *audio, fd2_audio_bus bus, float *gain) {
    if (!audio || !gain || bus < 0 || bus >= FD2_AUDIO_BUS_COUNT) return -1;
    *gain = audio->bus_gain[bus]; return 0;
}
int fd2_audio_play_music_source(fd2_audio *audio, fd2_audio_source source, float gain) {
    return fd2_audio_play_source(audio, FD2_AUDIO_BUS_MUSIC, source, gain);
}
int fd2_audio_abandon_source(fd2_audio *audio,fd2_audio_source source) {
    if(!owner(audio)||!source.render)return -1;
    int removed=0;
    for(size_t i=0;i<FD2_AUDIO_MAX_VOICES;i++) {
        fd2_audio_voice *voice=&audio->voices[i];
        if(voice->active && voice->source.render==source.render &&
           voice->source.retire==source.retire && voice->source.userdata==source.userdata) {
            voice_retire(audio,voice,FD2_AUDIO_RETIRE_DROPPED);removed++;
        }
    }
    return removed;
}
int fd2_audio_stop_music(fd2_audio *audio) { return fd2_audio_stop_bus(audio, FD2_AUDIO_BUS_MUSIC); }
int fd2_audio_set_master_gain(fd2_audio *audio, float gain) {
    if (!owner(audio) || !valid_gain(gain) || fd2_audio_sync(audio) != 0) return -1;
    if(audio->original_mixer && audio->master_gain!=gain &&
        fd2_audio_original_fill_up(audio)<0)return -1;
    audio->master_gain = gain; return 0;
}
size_t fd2_audio_render_offline(fd2_audio *audio, float *stereo, size_t frames) {
    if (!owner(audio) || !audio->offline || !stereo || !frames ||
        frames > SIZE_MAX / (2u * sizeof(float)) ||
        (uint64_t)frames > UINT64_MAX - audio->closed_frames) return 0;
    if(audio->original_mixer && (frames%48u || audio->original_rendered ||
        (audio->now_ns-audio->origin_ns)%UINT64_C(1000000)))return 0;
    uint64_t total = audio->closed_frames + (uint64_t)frames;
    uint64_t rate = (uint64_t)audio->sample_rate;
    uint64_t seconds = total / rate;
    uint64_t rem_ns = (total % rate) * NS_PER_SECOND / rate;
    if (seconds > UINT64_MAX / NS_PER_SECOND ||
        rem_ns > UINT64_MAX - seconds * NS_PER_SECOND) {
        record_first_error(audio, FD2_AUDIO_ERROR_CLOCK); return 0;
    }
    uint64_t target = seconds * NS_PER_SECOND + rem_ns;
    audio->offline_output = stereo;
    audio->offline_capacity = frames; audio->offline_written = 0;
    /* floor(N/rate)在rate<=1e9时严格晚于sample N-1，恰关闭N帧。
     * IRQ前仍先经同一prefix，之后回调；不读wall time或驱动第二timer。 */
    if (fd2_bios_clock_advance(target) != 0) record_first_error(audio, FD2_AUDIO_ERROR_CLOCK);
    size_t written = audio->offline_written;
    audio->offline_output = NULL; audio->offline_capacity = audio->offline_written = 0;
    return written;
}
