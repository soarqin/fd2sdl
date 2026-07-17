/* 炎龙骑士团 2 SDL3 重写 - Miles 数字样本 bank 播放后端
 *
 * 逆向依据：sfx_play @0x4acaa 从嵌套 LLLLLL bank 取得无头样本地址和
 * 字节长度；ail_init_sample @0x5e735 默认 11025 Hz、8-bit mono。
 */

#include "audio_pcm.h"

#include <limits.h>
#include <string.h>

#define FD2_PCM_VOICE_FREE 0
#define FD2_PCM_VOICE_ACTIVE 1

int fd2_pcm_bank_open_mem(fd2_pcm_bank *bank, const uint8_t *data,
                          size_t size) {
    if (!bank) return -1;
    memset(bank, 0, sizeof(*bank));
    if (!data) return -1;
    return fd2_archive_open_mem(&bank->archive, data, size);
}

int fd2_pcm_bank_open(fd2_pcm_bank *bank, const fd2_archive *fdother,
                      size_t entry_index) {
    if (!bank) return -1;
    memset(bank, 0, sizeof(*bank));
    if (!fdother) return -1;
    const uint8_t *data = NULL;
    size_t size = 0;
    if (fd2_archive_get(fdother, entry_index, &data, &size) != 0) return -1;
    return fd2_pcm_bank_open_mem(bank, data, size);
}

void fd2_pcm_bank_close(fd2_pcm_bank *bank) {
    if (!bank) return;
    fd2_archive_close(&bank->archive);
    memset(bank, 0, sizeof(*bank));
}

size_t fd2_pcm_bank_count(const fd2_pcm_bank *bank) {
    return bank ? bank->archive.count : 0;
}

static int voice_has_next_loop(const fd2_pcm_voice *voice) {
    return voice->loop_count == 0 || voice->loop_count > 1;
}

static void voice_advance(fd2_pcm_voice *voice) {
    uint64_t advance = voice->fraction + voice->source_rate;
    uint64_t skipped = advance / voice->output_rate;
    voice->fraction = advance % voice->output_rate;
    uint64_t next = (uint64_t)voice->index + skipped;
    uint64_t crossings = next / voice->length;
    voice->index = (size_t)(next % voice->length);
    if (crossings == 0 || voice->loop_count == 0) return;
    if (crossings >= (uint64_t)voice->loop_count) {
        voice->finished = 1;
        return;
    }
    voice->loop_count -= (int)crossings;
}

static size_t pcm_render(void *userdata, float *stereo, size_t frames) {
    fd2_pcm_voice *voice = userdata;
    size_t produced = 0;
    while (produced < frames && !voice->finished &&
           !atomic_load_explicit(&voice->stop_requested,
                                 memory_order_acquire)) {
        size_t index = voice->index;
        uint8_t first = voice->data[index];
        uint8_t second = first;
        if (index + 1u < voice->length)
            second = voice->data[index + 1u];
        else if (voice_has_next_loop(voice))
            second = voice->data[0];
        float a = ((float)first - 128.0f) / 128.0f;
        float b = ((float)second - 128.0f) / 128.0f;
        float t = (float)((double)voice->fraction / voice->output_rate);
        float sample = a + (b - a) * t;
        stereo[produced * 2u] = sample;
        stereo[produced * 2u + 1u] = sample;
        voice_advance(voice);
        produced++;
    }
    return produced;
}

static void pcm_retire(void *userdata, fd2_audio_retire_reason reason) {
    (void)reason;
    fd2_pcm_voice *voice = userdata;
    atomic_store_explicit(&voice->state, FD2_PCM_VOICE_FREE,
                          memory_order_release);
}

int fd2_pcm_player_init(fd2_pcm_player *player, fd2_audio *audio,
                        int source_rate) {
    if (!player || !audio || source_rate <= 0) return -1;
    int output_rate = fd2_audio_sample_rate(audio);
    if (output_rate <= 0) return -1;
    memset(player, 0, sizeof(*player));
    player->audio = audio;
    player->source_rate = source_rate;
    for (size_t i = 0; i < FD2_PCM_MAX_VOICES; i++) {
        atomic_init(&player->voices[i].state, FD2_PCM_VOICE_FREE);
        atomic_init(&player->voices[i].stop_requested, 0);
    }
    return 0;
}

static fd2_pcm_voice *claim_voice(fd2_pcm_player *player) {
    for (size_t i = 0; i < FD2_PCM_MAX_VOICES; i++) {
        int expected = FD2_PCM_VOICE_FREE;
        if (atomic_compare_exchange_strong_explicit(
                &player->voices[i].state, &expected, FD2_PCM_VOICE_ACTIVE,
                memory_order_acquire, memory_order_relaxed))
            return &player->voices[i];
    }
    return NULL;
}

static int play_on_voice(fd2_pcm_player *player,
                         fd2_pcm_voice *voice,
                         const fd2_pcm_bank *bank,
                         size_t sample_index,
                         int loop_count,
                         float gain) {
    if (!voice) return -1;
    const uint8_t *data = NULL;
    size_t length = 0;
    if (fd2_archive_get(&bank->archive, sample_index, &data, &length) != 0 ||
        length == 0 || length > UINT32_MAX)
        goto fail;

    voice->data = data;
    voice->length = length;
    voice->index = 0;
    voice->fraction = 0;
    voice->source_rate = (uint32_t)player->source_rate;
    voice->output_rate = (uint32_t)fd2_audio_sample_rate(player->audio);
    voice->loop_count = loop_count;
    voice->finished = 0;
    atomic_store_explicit(&voice->stop_requested, 0, memory_order_release);
    fd2_audio_source source = {
        .render = pcm_render,
        .retire = pcm_retire,
        .userdata = voice,
    };
    if (fd2_audio_play_source(player->audio, FD2_AUDIO_BUS_SFX,
                              source, gain) != 0) {
        goto fail;
    }
    return 0;

fail:
    atomic_store_explicit(&voice->state, FD2_PCM_VOICE_FREE,
                          memory_order_release);
    return -1;
}

int fd2_pcm_play(fd2_pcm_player *player, const fd2_pcm_bank *bank,
                 size_t sample_index, int loop_count, float gain) {
    if (!player || !player->audio || !bank || loop_count < 0) return -1;
    return play_on_voice(player, claim_voice(player), bank, sample_index,
                         loop_count, gain);
}

int fd2_pcm_stop(fd2_pcm_player *player) {
    if (!player || !player->audio) return -1;
    for (size_t i = 0; i < FD2_PCM_MAX_VOICES; i++) {
        if (atomic_load_explicit(&player->voices[i].state,
                                 memory_order_acquire) == FD2_PCM_VOICE_ACTIVE)
            atomic_store_explicit(&player->voices[i].stop_requested, 1,
                                  memory_order_release);
    }
    return 0;
}

int fd2_pcm_play_replace(fd2_pcm_player *player, const fd2_pcm_bank *bank,
                         size_t sample_index, int loop_count, float gain) {
    if (!player || !player->audio || !bank || loop_count < 0) return -1;
    fd2_pcm_voice *voice = claim_voice(player);
    if (!voice) return -1;
    /* 先为新 cue 保留 voice，再停止该 AIL handle 的旧 voice。这样连续
     * replacement 即使音频回调尚未 retire，也不会先停旧声后因无槽
     * 丢失确认 cue。当前新 voice 的 stop_requested 仍为上一生命周期值，
     * 因此 stop 循环只处理其余 ACTIVE voice。 */
    for (size_t i = 0; i < FD2_PCM_MAX_VOICES; i++) {
        if (&player->voices[i] == voice) continue;
        if (atomic_load_explicit(&player->voices[i].state,
                                 memory_order_acquire) == FD2_PCM_VOICE_ACTIVE)
            atomic_store_explicit(&player->voices[i].stop_requested, 1,
                                  memory_order_release);
    }
    return play_on_voice(player, voice, bank, sample_index, loop_count, gain);
}

void fd2_pcm_player_close(fd2_pcm_player *player) {
    if (!player) return;
    player->audio = NULL;
    player->source_rate = 0;
}
