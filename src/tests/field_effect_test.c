#include <stdio.h>
#include <string.h>

#include "field_effect.h"

/* 原1c449/1c47f tick1、21783 delay10、24701 delay5的有限顺序回归。
 * 全部时钟/上传为记录桩，不执行SDL/真实等待；expected取原指令与循环。 */
static int played_cues;
static fd2_field_sfx last_cue;
static unsigned presents,quit_after_present,output_fail_after_present;
typedef struct {char kind;uint32_t value;} order_event;
static order_event order_trace[512];
static size_t order_count;
static int order_enabled,order_overflow;
static unsigned wait_calls,quit_after_wait,legacy_calls;
static fd2_vga *waiting_vga;
static void record_order(char kind,uint32_t value) {
    if(!order_enabled)return;
    if(order_count>=sizeof(order_trace)/sizeof(order_trace[0])){order_overflow=1;return;}
    order_trace[order_count++]=(order_event){kind,value};
}
int fd2_field_audio_play(fd2_field_audio *audio, fd2_field_sfx cue) {
    (void)audio;
    played_cues++;
    last_cue = cue;
    record_order('C',(uint32_t)cue);
    return 0;
}
void fd2_vga_upload_current_frame(fd2_vga *vga) {
    record_order('P',vga->framebuffer[0]);
    if(++presents==quit_after_present)vga->input.quit_requested=1;
    if(presents==output_fail_after_present)vga->output_error=FD2_VGA_OUTPUT_LOCK;
}
int fd2_vga_present(fd2_vga *vga) {
    legacy_calls++;fd2_vga_upload_current_frame(vga);
return 0;
}
int fd2_vga_present_timed(fd2_vga *vga, uint32_t frame_ms) {
    (void)frame_ms;legacy_calls++;fd2_vga_upload_current_frame(vga);
return 0;
}
int fd2_vga_wait_frame_deadline(fd2_vga *vga) { (void)vga;legacy_calls++; return 0;
}
int fd2_wait_ticks(uint32_t ticks) {
    record_order('T',ticks);
    if(++wait_calls==quit_after_wait && waiting_vga)waiting_vga->input.quit_requested=1;
return 0;
}
int fd2_delay_ms(uint32_t milliseconds) {
    record_order('M',milliseconds);
    if(++wait_calls==quit_after_wait && waiting_vga)waiting_vga->input.quit_requested=1;
return 0;
}
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
        played_cues=0;presents=0;quit_after_present=output_fail_after_present=0;vga.output_error=0;
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

static void reset_order(fd2_vga *vga) {
    order_count=0;order_overflow=0;order_enabled=1;
    played_cues=0;presents=0;quit_after_present=output_fail_after_present=0;vga->output_error=0;
    wait_calls=0;quit_after_wait=0;legacy_calls=0;waiting_vga=vga;
    vga->input.quit_requested=0;
}
static int order_hook(fd2_field_effect effect,size_t frame,size_t source,
    fd2_vga *vga,void *userdata) {
    (void)effect;
    CHECK(vga->framebuffer[0]==11u+source); /* copy先于hook、upload与等待。 */
    record_order('H',(uint32_t)frame);
    return userdata && *(const int *)userdata==(int)frame ? -1:0;
}
static int host_order_hook(fd2_field_effect effect,size_t frame,size_t source,
    fd2_vga *vga,void *userdata) {
    int result=order_hook(effect,frame,source,vga,userdata);
    return result?result:1;
}
static int expect_order(size_t *at,char kind,uint32_t value) {
    CHECK(*at<order_count);
    CHECK(order_trace[*at].kind==kind && order_trace[*at].value==value);
    (*at)++;return 0;
}
static int order_frames(fd2_field_effect_frames *frames,unsigned effect) {
    size_t count=effect==0?2:effect==1?3:9;
    CHECK(fd2_field_effect_frames_open(frames,(fd2_field_effect)effect,count)==0);
    for(size_t i=0;i<count;i++)memset(fd2_field_effect_snapshot(frames,i),11+(int)i,64000);
    return 0;
}

/* 原flash五次(原页,tick1,solid页,tick1)，尾原页无wait；
 * quake60次copy后delay10，transition九次copy后delay5，无额外累计deadline。 */
static int test_original_wait_order(void) {
    static const unsigned frame_counts[]={11,60,9};
    static const unsigned quake_sources[]={0,1,2,1};
    static const fd2_field_sfx cues[]={FD2_FIELD_SFX_ACTOR_GROUP_FLASH,
        FD2_FIELD_SFX_EARTHQUAKE,FD2_FIELD_SFX_STAGE_TRANSITION};
    fd2_vga vga={0};fd2_field_audio audio={0};
    for(unsigned effect=0;effect<3;effect++) {
        fd2_field_effect_frames frames={0};CHECK(order_frames(&frames,effect)==0);
        reset_order(&vga);
        vga.frame_deadline_ns=UINT64_C(123456789);vga.frame_interval_ns=UINT64_C(55000000);
        CHECK(fd2_field_effect_play_hooked(&frames,&vga,&audio,1,order_hook,NULL)==0);
        size_t at=0;
        for(unsigned frame=0;frame<frame_counts[effect];frame++) {
            unsigned source=effect==0?(frame&1u):effect==1?quake_sources[frame%4u]:frame;
            int cue=effect==1?(frame<43u && frame%6u==0):frame==0;
            if(cue)CHECK(expect_order(&at,'C',(uint32_t)cues[effect])==0);
            CHECK(expect_order(&at,'H',frame)==0);
            CHECK(expect_order(&at,'P',11u+source)==0);
            if(effect==0) {if(frame<10)CHECK(expect_order(&at,'T',1)==0);}
            else CHECK(expect_order(&at,'M',effect==1?10u:5u)==0);
        }
        CHECK(at==order_count && !order_overflow && !legacy_calls);
        CHECK(presents==frame_counts[effect] && wait_calls==(effect==0?10u:frame_counts[effect]));
        CHECK(vga.frame_deadline_ns==UINT64_C(123456789) &&
            vga.frame_interval_ns==UINT64_C(55000000));
        fd2_field_effect_frames_close(&frames);
    }
    order_enabled=0;waiting_vga=NULL;return 0;
}

static int test_wait_error_and_host_prefix(void) {
    fd2_vga vga={0};fd2_field_audio audio={0};
    for(unsigned effect=0;effect<3;effect++) {
        fd2_field_effect_frames frames={0};CHECK(order_frames(&frames,effect)==0);
        reset_order(&vga);int fail_at=2;
        CHECK(fd2_field_effect_play_hooked(&frames,&vga,&audio,1,order_hook,&fail_at)==-1);
        CHECK(presents==2 && wait_calls==2 && order_count && !legacy_calls);
        CHECK(order_trace[order_count-1].kind=='H' && order_trace[order_count-1].value==2);
        CHECK(vga.framebuffer[0]==(effect==0?11u:13u)); /* 第三copy已提交，失败不upload/wait或回滚。 */
        reset_order(&vga);quit_after_present=2;
        CHECK(fd2_field_effect_play_hooked(&frames,&vga,&audio,1,order_hook,NULL)==1);
        CHECK(presents==2 && wait_calls==1 && order_trace[order_count-1].kind=='P' && !legacy_calls);
        reset_order(&vga);quit_after_wait=1;
        CHECK(fd2_field_effect_play_hooked(&frames,&vga,&audio,1,order_hook,NULL)==1);
        CHECK(presents==1 && wait_calls==1 && !legacy_calls);
        CHECK(order_trace[order_count-1].kind==(effect==0?'T':'M'));
        reset_order(&vga);vga.input.quit_requested=1;vga.framebuffer[0]=33;
        CHECK(fd2_field_effect_play_hooked(&frames,&vga,&audio,1,order_hook,NULL)==1);
        CHECK(!order_count && !played_cues && !presents && !wait_calls && vga.framebuffer[0]==33);
        reset_order(&vga);
        CHECK(fd2_field_effect_play_hooked(&frames,&vga,&audio,1,host_order_hook,NULL)==1);
        CHECK(!presents && !wait_calls && order_count==2 && order_trace[1].kind=='H');
        CHECK(vga.framebuffer[0]==11 && !legacy_calls); /* hook正返回保copy/页前缀，不upload或wait。 */
        reset_order(&vga);
        CHECK(fd2_field_effect_play_hooked(&frames,&vga,&audio,0,order_hook,NULL)==0);
        CHECK(!presents && !wait_calls && !legacy_calls); /* headless不偷加入等待。 */
        fd2_field_effect_frames_close(&frames);
    }
    order_enabled=0;waiting_vga=NULL;return 0;
}

static int output_error_hook(fd2_field_effect effect,size_t frame,size_t source,
    fd2_vga *vga,void *userdata){
 (void)effect;(void)frame;(void)source;(void)userdata;
 vga->output_error=FD2_VGA_OUTPUT_LOCK;vga->input.quit_requested=1;return 1;
}
static int test_output_error_prefix(void){
 fd2_vga vga={0};fd2_field_audio audio={0};fd2_field_effect_frames frames={0};
 CHECK(order_frames(&frames,0)==0);
 for(unsigned last=0;last<2;last++)for(unsigned host=0;host<2;host++){
  reset_order(&vga);output_fail_after_present=last?11u:1u;quit_after_present=host?output_fail_after_present:0;
  CHECK(fd2_field_effect_play_hooked(&frames,&vga,&audio,1,order_hook,NULL)<0);
  CHECK(presents==output_fail_after_present&&wait_calls==(last?10u:0u)&&played_cues==1&&!legacy_calls);
  CHECK(vga.framebuffer[0]==11&&vga.input.quit_requested==(int)host&&vga.output_error==FD2_VGA_OUTPUT_LOCK);
  unsigned saved_presents=presents,saved_waits=wait_calls;
  CHECK(fd2_field_effect_play_hooked(&frames,&vga,&audio,1,order_hook,NULL)<0&&presents==saved_presents&&wait_calls==saved_waits&&played_cues==1);
 }
 reset_order(&vga);
 CHECK(fd2_field_effect_play_hooked(&frames,&vga,&audio,0,output_error_hook,NULL)<0);
 CHECK(!presents&&!wait_calls&&played_cues==1&&vga.framebuffer[0]==11&&vga.input.quit_requested);
 fd2_field_effect_frames_close(&frames);order_enabled=0;waiting_vga=NULL;return 0;
}

int main(void) {
    CHECK(test_frame_sequences() == 0);
    CHECK(test_sfx_schedule() == 0);
    CHECK(test_owned_frames() == 0);
    CHECK(test_snapshot_copy() == 0);
    CHECK(test_page_payloads_and_frame_barriers()==0);
    CHECK(test_original_wait_order()==0);
    CHECK(test_wait_error_and_host_prefix()==0);
    CHECK(test_output_error_prefix()==0);
    puts("field effect tests: ok");
    return 0;
}

#include "vga_owner_wait_test_bridge.h"
