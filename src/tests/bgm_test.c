#include "bgm.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { \
    fprintf(stderr, "check failed at %s:%d: %s\n", __FILE__, __LINE__, #x); \
    return 1; \
} } while (0)

static double render_energy(fd2_audio *audio, size_t frames) {
    float buffer[512 * 2];
    double energy = 0.0;
    while (frames > 0) {
        size_t chunk = frames < 512 ? frames : 512;
        size_t got = fd2_audio_render_offline(audio, buffer, chunk);
        if (got != chunk) return -1.0;
        for (size_t i = 0; i < chunk * 2; i++)
            energy += fabs((double)buffer[i]);
        frames -= chunk;
    }
    return energy;
}

int main(void) {
    fd2_audio_config audio_config = {
        .sample_rate = 48000,
        .open_device = 0,
        .allow_null = 1,
    };
    fd2_audio *audio = fd2_audio_create(&audio_config);
    CHECK(audio != NULL);

    fd2_bgm_config config = {
        .fdmus_path = "original_game/FDMUS.DAT",
        .ail_bank_path = "original_game/SAMPLE.AD",
        .sample_rate = 48000,
        /* 测试使用 1 voice，确保常规分配和抢占路径都被大量触发。 */
        .voice_count = 1,
    };
    fd2_bgm_player *bgm = fd2_bgm_create(audio, &config);
    CHECK(bgm != NULL);
    CHECK(fd2_bgm_track_count(bgm) == 20);
    CHECK(!fd2_bgm_track_valid(bgm, 0));
    CHECK(fd2_bgm_track_valid(bgm, 1));
    CHECK(fd2_bgm_track_valid(bgm, 18));
    CHECK(!fd2_bgm_track_valid(bgm, 20));

    CHECK(fd2_bgm_play(bgm, 18, 0) == 0);
    CHECK(fd2_bgm_current_track(bgm) == 18);
    double title_energy = render_energy(audio, 48000 * 3u);
    CHECK(title_energy > 100.0);
    /* 原版重复请求同 track 直接返回，不重启 sequence。 */
    CHECK(fd2_bgm_play(bgm, 18, 0) == 0);

    /* new_game_opening_play @code0 0x22413..0x22417：标题曲之后
     * 以 loop_count=0 切换到过场 track 11。 */
    CHECK(fd2_bgm_play(bgm, 11, 0) == 0);
    CHECK(fd2_bgm_current_track(bgm) == 11);
    CHECK(render_energy(audio, 48000u) > 10.0);
    CHECK(fd2_bgm_stop(bgm) == 0);
    CHECK(fd2_bgm_current_track(bgm) == -1);
    /* 原版 stop 是 4000 ms sequence fade，不是硬切。 */
    CHECK(render_energy(audio, 48000u) > 1.0);
    CHECK(render_energy(audio, 48000u * 4u) >= 0.0);
    CHECK(render_energy(audio, 512) == 0.0);

    /* destroy 顺序与 main 相同：audio retire 后才能释放 BGM source。 */
    fd2_audio_destroy(audio);
    fd2_bgm_destroy(bgm);
    puts("bgm_test: ok");
    return 0;
}
