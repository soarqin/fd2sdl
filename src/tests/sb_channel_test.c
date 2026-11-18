/* SPDX-License-Identifier: MIT
 * 独立Python Fraction/整数oracle；仅选定SB channel数学/有界owner。
 * 不运行DOSBox/原码/SDL/设备，不以C输出生成expected，不签DMA时序。 */
#include "sb_channel.h"
#include <stdio.h>
#include <string.h>
#include "sb_channel_vectors.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#x); return 0; } } while (0)

typedef struct {
    uint32_t before;
    int32_t data[FD2_SB_CHANNEL_WINDOW][2];
    uint32_t after;
} guarded_window;
typedef struct {
    uint32_t before;
    int32_t data[FD2_SB_CHANNEL_MS_FRAMES][2];
    uint32_t after;
} guarded_output;
static void fixture(guarded_window *window, guarded_output *out) {
    window->before=UINT32_C(0x76a51234);window->after=UINT32_C(0x8bcd4321);
    out->before=UINT32_C(0xa35f6421);out->after=UINT32_C(0x59fa9876);
    for (size_t i=0;i<FD2_SB_CHANNEL_WINDOW;i++) {
        window->data[i][0]=(int32_t)(UINT32_C(0x13570000)+i*17u);
        window->data[i][1]=(int32_t)(UINT32_C(0x13570001)+i*17u);
    }
    for (size_t i=0;i<FD2_SB_CHANNEL_MS_FRAMES;i++) {
        out->data[i][0]=-123456789+(int32_t)i*31;
        out->data[i][1]=-123456788+(int32_t)i*31;
    }
}
static uint64_t window_fnv(const int32_t (*data)[2]) {
    uint64_t hash=UINT64_C(14695981039346656037);
    for (size_t i=0;i<FD2_SB_CHANNEL_WINDOW;i++)
        for(size_t side=0;side<2;side++) {
            uint32_t word=(uint32_t)data[i][side];
            for(unsigned bit=0;bit<32;bit+=8) {
                hash^=(word>>bit)&255u;
                hash*=UINT64_C(1099511628211);
            }
        }
    return hash;
}
static int compare_state(const fd2_sb_channel *ch,const sb_vector *expected) {
    const int64_t actual[]={ (int64_t)ch->written,(int64_t)ch->read,(int64_t)ch->rendered,
        ch->last,ch->current,ch->delta,ch->freq_f,ch->freq_fslew,
        ch->gain[0],ch->gain[1],ch->lowpass[0],ch->lowpass[1],ch->current_loaded };
    for(size_t i=0;i<13;i++)if(actual[i]!=expected->state[i]) {
        fprintf(stderr,"%s: state[%zu] actual=%lld expected=%lld\n",expected->label,i,
            (long long)actual[i],(long long)expected->state[i]);return 0;
    }
    return 1;
}
static int test_source_vectors(void) {
    guarded_window window={0};
    guarded_output out={0};
    fd2_sb_channel channel={0};
    unsigned cases=0;
    for(size_t i=0;i<sizeof(sb_vectors)/sizeof(sb_vectors[0]);i++) {
        const sb_vector *v=&sb_vectors[i];
        int status=-99;
        size_t loaded=SIZE_MAX,needed=SIZE_MAX;
        switch(v->op) {
        case V_INIT:
            fixture(&window,&out);memset(&channel,0xb5,sizeof(channel));
            status=fd2_sb_channel_init(&channel,window.data,v->a,v->b);cases++;break;
        case V_ADD:
            CHECK(v->offset<=sizeof(sb_input)&&v->count<=sizeof(sb_input)-v->offset);
            status=fd2_sb_channel_add_u8(&channel,sb_input+v->offset,v->count,&loaded);
            if(loaded!=v->loaded_bytes)fprintf(stderr,"%s: loaded %zu != %zu\n",v->label,loaded,v->loaded_bytes);
            CHECK(loaded==v->loaded_bytes);break;
        case V_NEED:
            status=fd2_sb_channel_input_needed(&channel,(size_t)v->a,&needed);
            if(needed!=v->needed)fprintf(stderr,"%s: requested %zu != %zu\n",v->label,needed,v->needed);
            CHECK(needed==v->needed);break;
        case V_RENDER:status=fd2_sb_channel_render_until(&channel,out.data,48,(size_t)v->a);break;
        case V_END:status=fd2_sb_channel_end_frame(&channel);break;
        default:CHECK(0);
        }
        if(status!=v->result)fprintf(stderr,"%s: rc %d != %d\n",v->label,status,v->result);
        CHECK(status==v->result);
        CHECK(window.before==UINT32_C(0x76a51234)&&window.after==UINT32_C(0x8bcd4321));
        CHECK(out.before==UINT32_C(0xa35f6421)&&out.after==UINT32_C(0x59fa9876));
        CHECK(compare_state(&channel,v));
        if(window_fnv(window.data)!=v->window_fnv)
            fprintf(stderr,"%s: output-window hash differs\n",v->label);
        CHECK(window_fnv(window.data)==v->window_fnv);
        if(memcmp(out.data,v->out,sizeof(out.data))) {
            for(size_t at=0;at<48;at++)for(size_t side=0;side<2;side++)
                if(out.data[at][side]!=v->out[at*2u+side]) {
                    fprintf(stderr,"%s: out[%zu][%zu] %d != %d\n",v->label,at,side,out.data[at][side],v->out[at*2u+side]);
                    return 0;
                }
        }
    }
    CHECK(cases==12);
    printf("sb_channel: %u source families / %zu checkpoints, full48 output+state+window hash PASS\n",
        cases,sizeof(sb_vectors)/sizeof(sb_vectors[0]));
    return 1;
}
static int test_host_failures_do_not_touch_owners(void) {
    guarded_window window;
    guarded_output out;
    fd2_sb_channel channel;
    fixture(&window,&out);
    CHECK(fd2_sb_channel_init(&channel,window.data,8192,8192)==0);
    fd2_sb_channel before;
    guarded_window saved_window;
    guarded_output saved_out;
    memcpy(&before,&channel,sizeof(before));
    memcpy(&saved_window,&window,sizeof(saved_window));
    memcpy(&saved_out,&out,sizeof(saved_out));
    size_t marker=SIZE_MAX;
    const uint8_t byte=255;
    CHECK(fd2_sb_channel_init(NULL,window.data,8192,8192)==-1);
    CHECK(fd2_sb_channel_init(&channel,NULL,8192,8192)==-1);
    CHECK(fd2_sb_channel_init(&channel,window.data,-1,8192)==-1);
    CHECK(fd2_sb_channel_init(&channel,window.data,8192,32769)==-1);
    CHECK(fd2_sb_channel_input_needed(&channel,49,&marker)==-1&&marker==SIZE_MAX);
    CHECK(fd2_sb_channel_input_needed(&channel,48,NULL)==-1);
    CHECK(fd2_sb_channel_add_u8(&channel,NULL,1,&marker)==-1&&marker==SIZE_MAX);
    CHECK(fd2_sb_channel_add_u8(&channel,&byte,1,NULL)==-1);
    CHECK(fd2_sb_channel_render_until(&channel,out.data,47,48)==-1);
    CHECK(fd2_sb_channel_render_until(&channel,out.data,49,49)==-1);
    CHECK(fd2_sb_channel_render_until(&channel,NULL,48,48)==-1);
    CHECK(fd2_sb_channel_end_frame(&channel)==-1);
    CHECK(memcmp(&channel,&before,sizeof(channel))==0);
    CHECK(memcmp(&window,&saved_window,sizeof(window))==0);
    CHECK(memcmp(&out,&saved_out,sizeof(out))==0);
    CHECK(fd2_sb_channel_add_u8(&channel,NULL,0,&marker)==0&&marker==0);
    CHECK(fd2_sb_channel_render_until(&channel,NULL,0,0)==0);
    CHECK(memcmp(&channel,&before,sizeof(channel))==0);
    CHECK(memcmp(&window,&saved_window,sizeof(window))==0);
    return 1;
}
int main(void) {
    if(!test_source_vectors()||!test_host_failures_do_not_touch_owners())return 1;
    puts("sb_channel: owner/cap/argument failure boundaries PASS; no clock/device/original execution");
    return 0;
}
