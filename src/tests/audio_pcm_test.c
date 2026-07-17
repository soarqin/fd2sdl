#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "audio_pcm.h"
#include "field_audio.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static void put_u32le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static size_t make_index_bank(uint8_t *data, size_t count, uint8_t base) {
    size_t start = 6 + count * 4;
    memcpy(data, "LLLLLL", 6);
    for (size_t i = 0; i < count; i++) {
        put_u32le(data + 6 + i * 4, (uint32_t)(start + i));
        data[start + i] = (uint8_t)(base + i);
    }
    return start + count;
}

static size_t make_bank(uint8_t *data) {
    memcpy(data, "LLLLLL", 6);
    put_u32le(data + 6, 14);
    put_u32le(data + 10, 17);
    data[14] = 0;
    data[15] = 128;
    data[16] = 255;
    data[17] = 128;
    data[18] = 255;
    return 19;
}

static int near(float a, float b) {
    return fabsf(a - b) < 0.0001f;
}

static int test_field_sfx_mapping(void) {
    static const struct {
        fd2_field_sfx cue;
        fd2_field_sfx_bank bank;
        size_t sample;
    } cases[] = {
        {FD2_FIELD_SFX_DETAIL_OPEN, FD2_FIELD_SFX_BANK_UI, 5},
        {FD2_FIELD_SFX_DETAIL_CLOSE, FD2_FIELD_SFX_BANK_UI, 6},
        {FD2_FIELD_SFX_COMMAND_MENU, FD2_FIELD_SFX_BANK_UI, 8},
        {FD2_FIELD_SFX_DIALOG_GLYPH, FD2_FIELD_SFX_BANK_UI, 2},
        {FD2_FIELD_SFX_ACTOR_GROUP_FLASH, FD2_FIELD_SFX_BANK_BATTLE, 1},
        {FD2_FIELD_SFX_STAGE_TRANSITION, FD2_FIELD_SFX_BANK_BATTLE, 11},
        {FD2_FIELD_SFX_EARTHQUAKE, FD2_FIELD_SFX_BANK_BATTLE, 13},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fd2_field_sfx_bank bank;
        size_t sample;
        CHECK(fd2_field_sfx_resolve(cases[i].cue, &bank, &sample) == 0);
        CHECK(bank == cases[i].bank && sample == cases[i].sample);
    }
    fd2_field_sfx_bank bank;
    size_t sample;
    CHECK(fd2_field_sfx_resolve((fd2_field_sfx)99, &bank, &sample) != 0);
    return 0;
}

static int test_field_audio_dispatch(void) {
    uint8_t ui_data[76], battle_data[76];
    fd2_pcm_bank ui_bank, battle_bank;
    CHECK(fd2_pcm_bank_open_mem(
        &ui_bank, ui_data, make_index_bank(ui_data, 14, 128)) == 0);
    CHECK(fd2_pcm_bank_open_mem(
        &battle_bank, battle_data,
        make_index_bank(battle_data, 14, 160)) == 0);
    fd2_audio_config config = {.sample_rate = 11025, .open_device = 0};
    fd2_audio *audio = fd2_audio_create(&config);
    fd2_pcm_player player;
    fd2_field_audio field_audio;
    CHECK(audio && fd2_pcm_player_init(&player, audio, 11025) == 0);
    fd2_field_audio_init(&field_audio, &player, &ui_bank, &battle_bank);

    float output[2];
    CHECK(fd2_field_audio_play(&field_audio, FD2_FIELD_SFX_DETAIL_OPEN) == 0);
    CHECK(fd2_audio_render_offline(audio, output, 1) == 1);
    CHECK(near(output[0], 5.0f / 128.0f));
    CHECK(fd2_field_audio_play(&field_audio, FD2_FIELD_SFX_DIALOG_GLYPH) == 0);
    CHECK(fd2_audio_render_offline(audio, output, 1) == 1);
    CHECK(near(output[0], 2.0f / 128.0f));
    CHECK(fd2_field_audio_play(
        &field_audio, FD2_FIELD_SFX_ACTOR_GROUP_FLASH) == 0);
    CHECK(fd2_audio_render_offline(audio, output, 1) == 1);
    CHECK(near(output[0], 33.0f / 128.0f));
    CHECK(fd2_field_audio_play(
        &field_audio, FD2_FIELD_SFX_STAGE_TRANSITION) == 0);
    CHECK(fd2_audio_render_offline(audio, output, 1) == 1);
    CHECK(near(output[0], 43.0f / 128.0f));

    fd2_field_audio_init(&field_audio, &player, NULL, &battle_bank);
    CHECK(fd2_field_audio_play(&field_audio, FD2_FIELD_SFX_DETAIL_OPEN) != 0);
    fd2_field_audio_init(&field_audio, NULL, &ui_bank, &battle_bank);
    CHECK(fd2_field_audio_play(&field_audio, FD2_FIELD_SFX_EARTHQUAKE) != 0);

    fd2_audio_destroy(audio);
    fd2_pcm_player_close(&player);

    fd2_audio_config fallback_config = {
        .sample_rate = 11025, .open_device = 1, .allow_null = 1,
    };
    fd2_audio *fallback = fd2_audio_create(&fallback_config);
    CHECK(fallback && !fd2_audio_has_device(fallback));
    fd2_pcm_player fallback_player;
    CHECK(fd2_pcm_player_init(&fallback_player, fallback, 11025) == 0);
    fd2_field_audio_init(&field_audio, &fallback_player,
                         &ui_bank, &battle_bank);
    CHECK(fd2_field_audio_play(&field_audio, FD2_FIELD_SFX_EARTHQUAKE) == 0);
    int active = 0;
    for (size_t i = 0; i < FD2_PCM_MAX_VOICES; i++)
        active += atomic_load(&fallback_player.voices[i].state) != 0;
    CHECK(active == 0);
    fd2_audio_destroy(fallback);
    fd2_pcm_player_close(&fallback_player);

    fd2_pcm_bank_close(&battle_bank);
    fd2_pcm_bank_close(&ui_bank);
    return 0;
}

static int test_one_shot_and_loop(void) {
    uint8_t data[19];
    fd2_pcm_bank bank;
    CHECK(fd2_pcm_bank_open_mem(&bank, data, make_bank(data)) == 0);
    CHECK(fd2_pcm_bank_count(&bank) == 2);
    fd2_audio_config config = {.sample_rate = 11025, .open_device = 0};
    fd2_audio *audio = fd2_audio_create(&config);
    fd2_pcm_player player;
    CHECK(audio && fd2_pcm_player_init(&player, audio, 11025) == 0);

    CHECK(fd2_pcm_play(&player, &bank, 0, 1, 1.0f) == 0);
    float output[8];
    CHECK(fd2_audio_render_offline(audio, output, 4) == 4);
    CHECK(near(output[0], -1.0f));
    CHECK(near(output[2], 0.0f));
    CHECK(near(output[4], 127.0f / 128.0f));
    CHECK(near(output[6], 0.0f));
    for (size_t i = 0; i < 4; i++) CHECK(near(output[i * 2], output[i * 2 + 1]));

    CHECK(fd2_pcm_play(&player, &bank, 1, 2, 1.0f) == 0);
    float looped[10];
    CHECK(fd2_audio_render_offline(audio, looped, 5) == 5);
    CHECK(near(looped[0], 0.0f));
    CHECK(near(looped[2], 127.0f / 128.0f));
    CHECK(near(looped[4], 0.0f));
    CHECK(near(looped[6], 127.0f / 128.0f));
    CHECK(near(looped[8], 0.0f));

    CHECK(fd2_pcm_play(&player, &bank, 0, 1, 1.0f) == 0);
    float replaced[2];
    CHECK(fd2_audio_render_offline(audio, replaced, 1) == 1);
    CHECK(near(replaced[0], -1.0f));
    CHECK(fd2_pcm_play_replace(&player, &bank, 1, 1, 1.0f) == 0);
    CHECK(fd2_audio_render_offline(audio, replaced, 1) == 1);
    CHECK(near(replaced[0], 0.0f));
    int active = 0;
    for (size_t i = 0; i < FD2_PCM_MAX_VOICES; i++)
        active += atomic_load(&player.voices[i].state) != 0;
    CHECK(active == 1);
    CHECK(fd2_pcm_stop(&player) == 0);
    CHECK(fd2_audio_render_offline(audio, replaced, 1) == 1);
    CHECK(near(replaced[0], 0.0f));
    active = 0;
    for (size_t i = 0; i < FD2_PCM_MAX_VOICES; i++)
        active += atomic_load(&player.voices[i].state) != 0;
    CHECK(active == 0);

    CHECK(fd2_pcm_play(&player, &bank, 2, 1, 1.0f) != 0);
    CHECK(fd2_pcm_play(&player, &bank, 0, -1, 1.0f) != 0);
    fd2_audio_destroy(audio);
    fd2_pcm_player_close(&player);
    fd2_pcm_bank_close(&bank);
    return 0;
}

static int test_replacement_burst(void) {
    uint8_t data[19];
    fd2_pcm_bank bank;
    CHECK(fd2_pcm_bank_open_mem(&bank, data, make_bank(data)) == 0);
    fd2_audio_config config = {.sample_rate = 11025, .open_device = 0};
    fd2_audio *audio = fd2_audio_create(&config);
    fd2_pcm_player player;
    CHECK(audio && fd2_pcm_player_init(&player, audio, 11025) == 0);

    /* 主线程可以在音频回调消费 command 之前收到多次导航和确认；最后
     * 一次确认不得因等待 retire 的旧 voice 占满 pool 而丢失。 */
    for (size_t i = 0; i < FD2_PCM_MAX_VOICES; i++)
        CHECK(fd2_pcm_play_replace(&player, &bank, i & 1u, 1, 1.0f) == 0);
    float output[2];
    CHECK(fd2_audio_render_offline(audio, output, 1) == 1);
    CHECK(near(output[0], 0.0f));
    int active = 0;
    for (size_t i = 0; i < FD2_PCM_MAX_VOICES; i++)
        active += atomic_load(&player.voices[i].state) != 0;
    CHECK(active == 1);

    fd2_audio_destroy(audio);
    fd2_pcm_player_close(&player);
    fd2_pcm_bank_close(&bank);
    return 0;
}

static int test_independent_sample_handles(void) {
    uint8_t bank_a_data[19], bank_b_data[19];
    fd2_pcm_bank bank_a, bank_b;
    CHECK(fd2_pcm_bank_open_mem(&bank_a, bank_a_data,
                                make_bank(bank_a_data)) == 0);
    CHECK(fd2_pcm_bank_open_mem(&bank_b, bank_b_data,
                                make_bank(bank_b_data)) == 0);
    fd2_audio_config config = {.sample_rate = 11025, .open_device = 0};
    fd2_audio *audio = fd2_audio_create(&config);
    fd2_pcm_player primary, secondary;
    CHECK(audio && fd2_pcm_player_init(&primary, audio, 11025) == 0);
    CHECK(fd2_pcm_player_init(&secondary, audio, 11025) == 0);

    CHECK(fd2_pcm_play_replace(&primary, &bank_a, 0, 1, 1.0f) == 0);
    CHECK(fd2_pcm_play_replace(&secondary, &bank_b, 1, 1, 1.0f) == 0);
    float output[2];
    CHECK(fd2_audio_render_offline(audio, output, 1) == 1);
    /* primary sample[0]=-1，secondary sample[0]=0；两 handle 同时活动。 */
    CHECK(near(output[0], -1.0f));
    CHECK(fd2_pcm_stop(&primary) == 0);
    CHECK(fd2_audio_render_offline(audio, output, 1) == 1);
    /* 停 primary 不能停止 secondary；其第二个 byte 为 255。 */
    CHECK(near(output[0], 127.0f / 128.0f));

    fd2_audio_destroy(audio);
    fd2_pcm_player_close(&secondary);
    fd2_pcm_player_close(&primary);
    fd2_pcm_bank_close(&bank_b);
    fd2_pcm_bank_close(&bank_a);
    return 0;
}

static int test_infinite_extreme_downsampling(void) {
    uint8_t data[19];
    fd2_pcm_bank bank;
    CHECK(fd2_pcm_bank_open_mem(&bank, data, make_bank(data)) == 0);
    fd2_audio_config config = {.sample_rate = 1, .open_device = 0};
    fd2_audio *audio = fd2_audio_create(&config);
    fd2_pcm_player player;
    CHECK(audio && fd2_pcm_player_init(&player, audio, INT_MAX) == 0);
    CHECK(fd2_pcm_play(&player, &bank, 1, 0, 1.0f) == 0);
    float output[6];
    CHECK(fd2_audio_render_offline(audio, output, 3) == 3);
    CHECK(near(output[0], 0.0f));
    CHECK(near(output[2], 127.0f / 128.0f));
    CHECK(near(output[4], 0.0f));
    fd2_audio_destroy(audio);
    fd2_pcm_player_close(&player);
    fd2_pcm_bank_close(&bank);

    fd2_pcm_bank invalid;
    CHECK(fd2_pcm_bank_open_mem(&invalid, NULL, 0) != 0);
    fd2_pcm_bank_close(&invalid);
    return 0;
}

static int test_resampling(void) {
    uint8_t data[19];
    fd2_pcm_bank bank;
    CHECK(fd2_pcm_bank_open_mem(&bank, data, make_bank(data)) == 0);
    fd2_audio_config config = {.sample_rate = 22050, .open_device = 0};
    fd2_audio *audio = fd2_audio_create(&config);
    fd2_pcm_player player;
    CHECK(audio && fd2_pcm_player_init(&player, audio, 11025) == 0);
    CHECK(fd2_pcm_play(&player, &bank, 0, 1, 1.0f) == 0);
    float output[14];
    CHECK(fd2_audio_render_offline(audio, output, 7) == 7);
    const float expected[] = {-1.0f, -0.5f, 0.0f,
                              63.5f / 128.0f, 127.0f / 128.0f,
                              127.0f / 128.0f, 0.0f};
    for (size_t i = 0; i < 7; i++) CHECK(near(output[i * 2], expected[i]));
    fd2_audio_destroy(audio);
    fd2_pcm_player_close(&player);
    fd2_pcm_bank_close(&bank);
    return 0;
}

int main(void) {
    CHECK(test_field_sfx_mapping() == 0);
    CHECK(test_field_audio_dispatch() == 0);
    CHECK(test_one_shot_and_loop() == 0);
    CHECK(test_replacement_burst() == 0);
    CHECK(test_independent_sample_handles() == 0);
    CHECK(test_resampling() == 0);
    CHECK(test_infinite_extreme_downsampling() == 0);
    puts("audio PCM tests: ok");
    return 0;
}
