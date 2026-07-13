#include <stdio.h>
#include <string.h>

#include "field_effect.h"

/* field_effect.c 的 timed=0 单元测试不触及 SDL；为纯调度目标提供链接桩。 */
static int played_cues;
static fd2_field_sfx last_cue;
int fd2_field_audio_play(fd2_field_audio *audio, fd2_field_sfx cue) {
    (void)audio;
    played_cues++;
    last_cue = cue;
    return 0;
}
void fd2_vga_present(fd2_vga *vga) { (void)vga; }
void fd2_vga_present_timed(fd2_vga *vga, uint32_t frame_ms) {
    (void)vga; (void)frame_ms;
}
void fd2_vga_wait_frame_deadline(fd2_vga *vga) { (void)vga; }

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static int test_frame_sequences(void) {
    CHECK(fd2_field_effect_frame_count(FD2_FIELD_EFFECT_ACTOR_GROUP_FLASH) == 11);
    CHECK(fd2_field_effect_frame_count(FD2_FIELD_EFFECT_EARTHQUAKE) == 60);
    CHECK(fd2_field_effect_frame_count(FD2_FIELD_EFFECT_STAGE_TRANSITION) == 9);
    for (size_t frame = 0; frame < 11; frame++)
        CHECK(fd2_field_effect_source_index(
            FD2_FIELD_EFFECT_ACTOR_GROUP_FLASH, frame) == (int)(frame & 1u));
    static const int quake_cycle[] = {0, 1, 2, 1};
    for (size_t frame = 0; frame < 60; frame++)
        CHECK(fd2_field_effect_source_index(
            FD2_FIELD_EFFECT_EARTHQUAKE, frame) == quake_cycle[frame % 4]);
    for (size_t frame = 0; frame < 9; frame++)
        CHECK(fd2_field_effect_source_index(
            FD2_FIELD_EFFECT_STAGE_TRANSITION, frame) == (int)frame);
    CHECK(fd2_field_effect_source_index(FD2_FIELD_EFFECT_EARTHQUAKE, 60) < 0);
    return 0;
}

static int test_sfx_schedule(void) {
    fd2_field_sfx cue;
    for (size_t frame = 0; frame < 11; frame++) {
        int expected = frame == 0;
        CHECK((fd2_field_effect_sfx(FD2_FIELD_EFFECT_ACTOR_GROUP_FLASH,
                                    frame, &cue) == 0) == expected);
        if (expected) CHECK(cue == FD2_FIELD_SFX_ACTOR_GROUP_FLASH);
    }
    int quake_count = 0;
    for (size_t frame = 0; frame < 60; frame++) {
        int expected = frame < 43 && frame % 6 == 0;
        CHECK((fd2_field_effect_sfx(FD2_FIELD_EFFECT_EARTHQUAKE,
                                    frame, &cue) == 0) == expected);
        if (expected) {
            CHECK(cue == FD2_FIELD_SFX_EARTHQUAKE);
            quake_count++;
        }
    }
    CHECK(quake_count == 8);
    for (size_t frame = 0; frame < 9; frame++) {
        int expected = frame == 0;
        CHECK((fd2_field_effect_sfx(FD2_FIELD_EFFECT_STAGE_TRANSITION,
                                    frame, &cue) == 0) == expected);
        if (expected) CHECK(cue == FD2_FIELD_SFX_STAGE_TRANSITION);
    }
    CHECK(fd2_field_effect_sfx(FD2_FIELD_EFFECT_EARTHQUAKE, 0, NULL) != 0);
    return 0;
}

static int test_owned_frames(void) {
    fd2_field_effect_frames frames = {0};
    CHECK(fd2_field_effect_frames_open(
        &frames, (fd2_field_effect)99, 0) != 0);
    CHECK(frames.storage == NULL);
    CHECK(fd2_field_effect_frames_open(
        &frames, FD2_FIELD_EFFECT_EARTHQUAKE, 3) == 0);
    uint8_t *owned_storage = frames.storage;
    CHECK(fd2_field_effect_frames_open(
        &frames, FD2_FIELD_EFFECT_EARTHQUAKE, 3) != 0);
    CHECK(frames.storage == owned_storage);
    CHECK(frames.bytes_per_snapshot == VGA_W * VGA_H);
    for (size_t i = 0; i < 3; i++) {
        uint8_t *snapshot = fd2_field_effect_snapshot(&frames, i);
        CHECK(snapshot != NULL);
        memset(snapshot, (int)i + 1, frames.bytes_per_snapshot);
    }
    fd2_vga vga = {0};
    fd2_field_audio audio = {0};
    played_cues = 0;
    CHECK(fd2_field_effect_play(&frames, &vga, &audio, 0) == 0);
    CHECK(vga.framebuffer[0] == 2); /* frame 59 -> cycle source 1 */
    CHECK(played_cues == 8 && last_cue == FD2_FIELD_SFX_EARTHQUAKE);
    fd2_field_effect_frames_close(&frames);
    CHECK(frames.storage == NULL);
    CHECK(fd2_field_effect_frames_open(
        &frames, FD2_FIELD_EFFECT_EARTHQUAKE, 4) != 0);
    return 0;
}

static int test_snapshot_copy(void) {
    const uint8_t a[] = {1, 2, 3, 4};
    const uint8_t b[] = {5, 6, 7, 8};
    const uint8_t c[] = {9, 10, 11, 12};
    const uint8_t *sources[] = {a, b, c};
    uint8_t dst[4] = {0};
    CHECK(fd2_field_effect_copy_frame(dst, sources, 3, sizeof(dst),
          FD2_FIELD_EFFECT_EARTHQUAKE, 2) == 0);
    CHECK(memcmp(dst, c, sizeof(dst)) == 0);
    CHECK(fd2_field_effect_copy_frame(dst, sources, 3, sizeof(dst),
          FD2_FIELD_EFFECT_EARTHQUAKE, 3) == 0);
    CHECK(memcmp(dst, b, sizeof(dst)) == 0);
    CHECK(fd2_field_effect_copy_frame(dst, sources, 1, sizeof(dst),
          FD2_FIELD_EFFECT_EARTHQUAKE, 1) != 0);
    return 0;
}

int main(void) {
    CHECK(test_frame_sequences() == 0);
    CHECK(test_sfx_schedule() == 0);
    CHECK(test_owned_frames() == 0);
    CHECK(test_snapshot_copy() == 0);
    puts("field effect tests: ok");
    return 0;
}
