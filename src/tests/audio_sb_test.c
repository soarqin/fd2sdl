/* SPDX-License-Identifier: MIT
 * 完整DIG→实际SB provider→channel组合，独立有理channel+区间DIG event tape。
 * 只替换BIOS安装入口为显式timer注册记录；不测epoch/IRQ真实交付或设备。
 */
#include "audio_sb.h"
#include <stdio.h>
#include <string.h>
#include "dig_sb_vectors.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#x); return 0; } } while(0)

static fd2_ail_timer timer;
static fd2_ail_timer_callback installed_callback;
static void *installed_user;
static unsigned releases;
fd2_ail_timer *fd2_bios_clock_ail_owner_after_irqs(uint64_t now) {
    (void)now; return &timer;
}
uint32_t fd2_bios_clock_install_dig(uint64_t now, fd2_ail_timer_callback cb, void *user) {
    (void)now; installed_callback=cb; installed_user=user; return 0;
}
int fd2_ail_timer_release(fd2_ail_timer *t, uint32_t handle) {
    if(t!=&timer || handle!=0)return -1;
    releases++; return 0;
}
typedef struct fixture {
    uint32_t before;
    fd2_audio_dig dig;
    fd2_audio_sb sb;
    fd2_sb_point point;
    int32_t output[48][2];
    uint32_t after;
    size_t fill_whole;
    unsigned fills;
    int fill_error;
} fixture;
static int global_fill(void *user) {
    fixture *f=(fixture *)user;
    f->fills++;
    if(f->fill_error)return -1;
    return fd2_audio_sb_mix_until(&f->sb,f->output,48,f->fill_whole);
}
static int init(fixture *f) {
    memset(f,0,sizeof(*f));
    f->before=UINT32_C(0x76a51234);f->after=UINT32_C(0x8bcd4321);
    fd2_audio_dig_init(&f->dig);
    CHECK(fd2_audio_sb_init(&f->sb,&f->dig,&f->point,1,8192,4096,global_fill,f)==0);
    fd2_dig_external_sb16 external;
    CHECK(fd2_audio_sb_external(&f->sb,&external)==0);
    installed_callback=NULL;installed_user=NULL;
    CHECK(fd2_audio_dig_attach_external(&f->dig,&external,0)==0);
    CHECK(installed_callback && installed_user==&f->dig);
    CHECK(f->sb.dig==&f->dig && f->sb.channel.window==f->sb.window);
    return 1;
}
static uint64_t fnv_bytes(const uint8_t *bytes,size_t count) {
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<count;i++){h^=bytes[i];h*=UINT64_C(1099511628211);}
    return h;
}
static uint64_t fnv_words(const int32_t *words,size_t count) {
    uint64_t h=UINT64_C(14695981039346656037);
    for(size_t i=0;i<count;i++)for(unsigned shift=0;shift<32;shift+=8) {
        h^=((uint32_t)words[i]>>shift)&255u;h*=UINT64_C(1099511628211);
    }
    return h;
}
static int compare(const fixture *f,const bridge_vector *v) {
    const fd2_sb_channel *c=&f->sb.channel;
    const fd2_audio_dig *d=&f->dig;
    const fd2_sb_dma_provider *s=&f->sb.dma;
    const int64_t state[]={
        (int64_t)c->written,(int64_t)c->read,(int64_t)c->rendered,
        c->last,c->current,c->delta,c->freq_f,c->freq_fslew,
        c->gain[0],c->gain[1],c->lowpass[0],c->lowpass[1],c->current_loaded,c->enabled,
        d->playing,d->empty_services,d->entered_active,d->last_half,
        d->samples[0].length,d->samples[0].position,d->samples[0].loop_count,d->samples[0].status,
        d->samples[1].length,d->samples[1].position,d->samples[1].loop_count,d->samples[1].status,
        (int64_t)s->cursor,s->left,s->total,s->mode,s->irq_pending,s->require_ack,
        s->driver_half,s->saved_next_half,s->driver_state,(int64_t)s->dma_reads,
        (int64_t)s->block_ends,f->fills};
    CHECK(sizeof(state)==sizeof(v->state));
    for(size_t i=0;i<sizeof(state)/sizeof(state[0]);i++) if(state[i]!=v->state[i]) {
        fprintf(stderr,"%s: state[%zu] %lld != %lld\n",v->label,i,
            (long long)state[i],(long long)v->state[i]);return 0;
    }
    CHECK(d->first_error==0 && s->first_error==0 && f->sb.first_error==0);
    CHECK(f->sb.in_mix==0 && f->sb.in_control==0);
    CHECK(f->before==UINT32_C(0x76a51234) && f->after==UINT32_C(0x8bcd4321));
    CHECK(fnv_bytes(&d->dma[0][0],sizeof(d->dma))==v->dma_fnv);
    CHECK(fnv_words(d->build,2048)==v->build_fnv);
    CHECK(fnv_words(&f->sb.window[0][0],4096)==v->window_fnv);
    for(size_t i=0;i<48;i++)for(size_t side=0;side<2;side++)
        if(f->output[i][side]!=v->out[i*2u+side]) {
            fprintf(stderr,"%s: output[%zu][%zu] %d != %d\n",v->label,i,side,
                f->output[i][side],v->out[i*2u+side]);return 0;
        }
    return 1;
}
static int source_tape(void) {
    fixture f={0};unsigned cases=0;
    for(size_t i=0;i<sizeof(bridge_vectors)/sizeof(bridge_vectors[0]);i++) {
        const bridge_vector *v=&bridge_vectors[i];int result=0;
        f.point.tick=v->tick;f.point.tick_index=0;f.point.service_index=0;
        f.fill_whole=v->fill_whole;
        switch(v->op) {
        case B_INIT:CHECK(init(&f));cases++;break;
        case B_START:
            CHECK(v->a>=0&&v->a<2&&v->b>=0&&v->b<4&&v->c>=0);
            result=fd2_audio_dig_start(&f.dig,(unsigned)v->a,bridge_inputs[v->b],
                bridge_lengths[v->b],(uint32_t)v->c);break;
        case B_STOP:result=fd2_audio_dig_stop(&f.dig,(unsigned)v->a);break;
        case B_SERVICE:installed_callback(installed_user);break;
        case B_MIX:result=fd2_audio_sb_mix_until(&f.sb,f.output,48,(size_t)v->a);break;
        case B_IRQ:result=fd2_audio_sb_deliver_irq(&f.sb);break;
        case B_END:
            result=fd2_audio_sb_end_frame(&f.sb);
            memset(f.output,0,sizeof(f.output));break;
        case B_GAIN:result=fd2_audio_sb_set_gain(&f.sb,v->a,v->b);break;
        default:CHECK(0);
        }
        if(result!=0)fprintf(stderr,"%s: result %d\n",v->label,result);
        CHECK(result==0);CHECK(compare(&f,v));
    }
    CHECK(cases==3);
    printf("audio_sb: %u explicit CPU/tick families / %zu checkpoints, complete48 Q13 outputs+38 fields+full window/build/DMA hashes PASS\n",
        cases,sizeof(bridge_vectors)/sizeof(bridge_vectors[0]));
    return 1;
}
static int errors_preserve_prefix(void) {
    fixture f;CHECK(init(&f));
    fixture before;memcpy(&before,&f,sizeof(before));
    fd2_dig_external_sb16 external={0};
    CHECK(fd2_audio_sb_external(&f.sb,&external)==0);
    int16_t marker=7;
    CHECK(external.start(external.user,bridge_input_0,4096)<0);
    CHECK(fd2_audio_sb_mix_until(&f.sb,f.output,48,49)<0);
    CHECK(fd2_audio_sb_mix_until(&f.sb,NULL,48,1)<0);
    CHECK(fd2_audio_sb_end_frame(&f.sb)<0);
    CHECK(fd2_audio_sb_set_gain(&f.sb,-1,1)<0);
    CHECK(memcmp(&f,&before,sizeof(f))==0);
    /* start source writesP/status and MASKED/ring before failing global FillUp. */
    f.fill_error=1;
    CHECK(fd2_audio_dig_start(&f.dig,0,bridge_input_0,2500,1)<0);
    CHECK(f.dig.samples[0].status==4 && f.dig.samples[0].position==0);
    CHECK(f.dig.playing==0 && f.dig.first_error==FD2_DIG_H_SB16_START);
    CHECK(f.sb.dma.mode==FD2_SB_MASKED && f.sb.dma.ring==&f.dig.dma[0][0]);
    CHECK(f.sb.first_error==-4 && f.sb.dma.first_error==FD2_SB_H_FILLUP);
    CHECK(f.sb.dma.dma_reads==0 && f.sb.channel.current_loaded==0 && f.fills==1);
    CHECK(fd2_audio_sb_mix_until(&f.sb,f.output,48,48)==-4 && f.fills==1);
    CHECK(external.read_half(external.user,&marker)==-4 && marker==7);
    CHECK(fd2_audio_sb_close(&f.sb)==0 && fd2_audio_sb_close(&f.sb)==0);
    memcpy(&before,&f,sizeof(f));
    CHECK(external.start(external.user,&f.dig.dma[0][0],4096)<0);
    CHECK(external.read_half(external.user,&marker)<0 && marker==7);
    CHECK(external.request_pause(external.user)<0);
    CHECK(fd2_audio_sb_mix_until(&f.sb,f.output,48,48)<0);
    CHECK(fd2_audio_sb_deliver_irq(&f.sb)<0);
    CHECK(memcmp(&f,&before,sizeof(f))==0);
    fd2_audio_dig_close(&f.dig);CHECK(releases>0);
    return 1;
}
int main(void) {
    if(!source_tape()||!errors_preserve_prefix())return 1;
    puts("audio_sb: no-clock finite bridge; argument/FillUp-first-error/closed-borrow prefixes PASS");
    return 0;
}
