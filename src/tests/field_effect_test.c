#include <stdio.h>
#include <string.h>

#include "field_effect.h"

/* field_effect.c 的 timed=0 单元测试不触及 SDL；为纯调度目标提供链接桩。 */
static int played_cues;
static fd2_field_sfx last_cue;
static unsigned presents,quit_after_present;
int fd2_field_audio_play(fd2_field_audio *audio, fd2_field_sfx cue) {
    (void)audio;
    played_cues++;
    last_cue = cue;
    return 0;
}
void fd2_vga_present(fd2_vga *vga) {
    if(++presents==quit_after_present)vga->input.quit_requested=1;
}
void fd2_vga_present_timed(fd2_vga *vga, uint32_t frame_ms) {
    (void)frame_ms;fd2_vga_present(vga);
}
void fd2_vga_wait_frame_deadline(fd2_vga *vga) { (void)vga; }
int fd2_input_take_quit(fd2_input *input) {return input && input->quit_requested;}

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

typedef struct {unsigned calls;int fail_at;uint8_t expected_page;int check_each;} hook_state;
static int effect_frame_hook(fd2_field_effect effect,size_t frame,size_t source,
    fd2_vga *vga,void *userdata) {
    hook_state *state=userdata;uint8_t pixel;
    CHECK(source==(size_t)fd2_field_effect_source_index(effect,frame));
    CHECK(fd2_field_page_read(vga->field_page,72,72,1,1,&pixel,1,1)==0);
    CHECK(pixel==(state->check_each?(uint8_t)(100+frame):state->expected_page));
    state->calls++;
    return state->fail_at==(int)frame?-1:0;
}

static int test_page_payloads_and_frame_barriers(void) {
    fd2_vga vga={0};vga.field_page=fd2_field_page_create();CHECK(vga.field_page);
    fd2_field_audio audio={0};
    for(unsigned effect=0;effect<=2;effect++) {
        fd2_field_effect_frames frames={0};size_t count=effect==0?2:effect==1?3:9;
        CHECK(fd2_field_effect_frames_open(&frames,(fd2_field_effect)effect,count)==0);
        for(size_t i=0;i<count;i++) {
            memset(fd2_field_effect_snapshot(&frames,i),(int)i+1,64000);
            fd2_field_page *page=fd2_field_page_create();CHECK(page);
            uint8_t pixel=effect==0?(i?200:11):effect==1?(uint8_t)(77+i):(uint8_t)(100+i);
            CHECK(fd2_field_page_write(page,72,72,1,1,&pixel,1,1)==0);
            CHECK(fd2_field_effect_frames_take_page(&frames,i,page)==0);
            CHECK(fd2_field_effect_frames_take_page(&frames,(i+1)%count,page)==-1);
        }
        hook_state state={0,-1,effect==0?200:77,effect==2};
        played_cues=0;presents=0;quit_after_present=0;
        CHECK(fd2_field_effect_play_hooked(&frames,&vga,&audio,0,effect_frame_hook,&state)==0);
        CHECK(state.calls==fd2_field_effect_frame_count((fd2_field_effect)effect));
        uint8_t pixel;
        CHECK(fd2_field_page_read(vga.field_page,72,72,1,1,&pixel,1,1)==0);
        CHECK(pixel==(effect==0?200:effect==1?77:108));
        CHECK(vga.framebuffer[0]==(effect==0?1:effect==1?2:9));
        /* 关闭/错误/宿主退出均不替caller猜一个正常尾页。 */
        uint8_t initial=33;CHECK(fd2_field_page_write(vga.field_page,72,72,1,1,&initial,1,1)==0);
        vga.input.quit_requested=1;state.calls=0;played_cues=0;
        CHECK(fd2_field_effect_play_hooked(&frames,&vga,&audio,1,effect_frame_hook,&state)==1);
        CHECK(!state.calls && !played_cues);
        CHECK(fd2_field_page_read(vga.field_page,72,72,1,1,&pixel,1,1)==0 && pixel==33);
        vga.input.quit_requested=0;state.calls=0;state.fail_at=2;played_cues=0;presents=0;
        CHECK(fd2_field_effect_play_hooked(&frames,&vga,&audio,1,effect_frame_hook,&state)==-1);
        CHECK(state.calls==3 && presents==2);
        state.fail_at=-1;state.calls=0;presents=0;quit_after_present=2;
        CHECK(fd2_field_effect_play_hooked(&frames,&vga,&audio,1,effect_frame_hook,&state)==1);
        CHECK(state.calls==2 && presents==2);
        vga.input.quit_requested=0;quit_after_present=0;
        fd2_field_effect_frames_close(&frames);
        CHECK(!frames.storage && !frames.page_snapshots[0]);
    }
    fd2_field_page_destroy(vga.field_page);return 0;
}

int main(void) {
    CHECK(test_frame_sequences() == 0);
    CHECK(test_sfx_schedule() == 0);
    CHECK(test_owned_frames() == 0);
    CHECK(test_snapshot_copy() == 0);
    CHECK(test_page_payloads_and_frame_barriers()==0);
    puts("field effect tests: ok");
    return 0;
}
