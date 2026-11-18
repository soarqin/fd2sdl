/* 原sub_404C0 @0x404c0／sub_49690 @0x49690／sub_4809B @0x4809b。
 * DIG profile来自固定原运行22050/kind0/flags0/2048；样本默认11025/100/64。
 * 两handle共享整数混合和缓冲尾音，外层Audio负责采样时点及设备输出。 */
#include "audio_dig.h"
#include <stddef.h>
#include <string.h>

static int fail(fd2_audio_dig *d,int error) {
    if(!d->first_error)d->first_error=error;
    return error;
}
static int valid(const fd2_audio_dig *d) { return d && !d->closed; }
void fd2_audio_dig_init(fd2_audio_dig *d) {
    if(!d)return;
    memset(d,0,sizeof(*d));memset(d->dma,128,sizeof(d->dma));
    for(unsigned h=0;h<FD2_DIG_HANDLES;h++) {
        d->samples[h].status=2;d->samples[h].loop_count=1;
    }
    d->enabled=1;d->last_half=-1;d->timer_handle=FD2_AIL_TIMER_INVALID;
}
static void service_timer(void *user) { (void)fd2_audio_dig_service(user); }
int fd2_audio_dig_attach_external(fd2_audio_dig *d,
    const fd2_dig_external_sb16 *p,uint64_t now) {
    if(!valid(d) || d->attached)return FD2_DIG_H_ARGUMENT;
    if(!p || !p->start || !p->read_half || !p->request_pause)
        return FD2_DIG_H_SB16_TIMELINE;
    fd2_ail_timer *t=fd2_bios_clock_ail_owner_after_irqs(now);
    if(!t)return fail(d,FD2_DIG_H_CLOCK);
    d->sb16=*p;d->timer=t;
    d->timer_handle=fd2_bios_clock_install_dig(now,service_timer,d);
    if(d->timer_handle==FD2_AIL_TIMER_INVALID)return fail(d,FD2_DIG_H_CLOCK);
    d->attached=1;return 0;
}
int fd2_audio_dig_stop(fd2_audio_dig *d,unsigned h) {
    if(!valid(d) || h>=FD2_DIG_HANDLES)return FD2_DIG_H_ARGUMENT;
    if(!d->enabled)return 0;
    if(d->first_error)return d->first_error;
    if(d->samples[h].status==4)d->samples[h].status=8;
    return 0;
}
int fd2_audio_dig_start(fd2_audio_dig *d,unsigned h,
    const uint8_t *data,uint32_t length,uint32_t loops) {
    if(!valid(d) || h>=FD2_DIG_HANDLES)return FD2_DIG_H_ARGUMENT;
    if(!d->enabled)return 0;
    if(!data || !length)return FD2_DIG_H_ARGUMENT;
    if(d->first_error)return d->first_error;
    if(!d->attached)return FD2_DIG_H_SB16_TIMELINE;
    /* wrapper已先stop8；资源解析成功后init/source/loop/start唯一P写集。 */
    d->samples[h]=(fd2_dig_sample){data,length,0,loops,2};
    d->samples[h].status=4;
    if(!d->playing) {
        if(d->sb16.start(d->sb16.user,&d->dma[0][0],(uint32_t)sizeof(d->dma))<0)
            return fail(d,FD2_DIG_H_SB16_START);
        d->playing=1;
    }
    return 0;
}
/* ail_sample_build_mix @0x49690 / ail_sample_u8_resample_accumulate @0x49430
 * （原sub_49690/sub_49430）：11025→22050，每服务/loop片段重置半相位。
 * 两P共同加和后才夹幅，不插值、不对已经提交的DMA反向消声。 */
static int mix_one(fd2_audio_dig *d,fd2_dig_sample *p) {
    if(!p->data||!p->length||p->position>p->length)return FD2_DIG_H_ARGUMENT;
    uint32_t out=0;
    for(;;) {
        uint32_t position=p->position,fraction=UINT32_C(0x80000000);
        while(position<p->length && out<FD2_DIG_HALF) {
            uint8_t byte=p->data[position];
            uint64_t next=(uint64_t)fraction+UINT64_C(0x80000000);
            fraction=(uint32_t)next;position+=(uint32_t)(next>>32);
            d->build[out++]+=((int32_t)byte-128)*FD2_DIG_DEFAULT_U8_SCALE;
        }
        p->position=position;
        if(position<p->length)return 0;
        if(p->loop_count) {
            if(p->loop_count==1) {p->status=2;return 0;}
            --p->loop_count;
        }
        p->position=0;
        if(out==FD2_DIG_HALF)return 0;
    }
}
/* ail_dig_build_convert @0x49891 / 0x4809b / 0x47d88
 * （原sub_49891/sub_4809B/sub_47D88）：有贡献先S16夹幅，再取U8高字节。 */
static void format_half(fd2_audio_dig *d,unsigned half) {
    uint8_t *to=d->dma[half];
    for(unsigned n=0;n<FD2_DIG_HALF;n++) {
        int32_t value=d->build[n];
        if(d->entered_active) {
            if(value>32767)value=32767;
            else if(value< -32768)value=-32768;
        }
        to[n]=(uint8_t)(((uint32_t)value^UINT32_C(0x8000))>>8);
    }
}
int fd2_audio_dig_service(fd2_audio_dig *d) {
    if(!valid(d))return FD2_DIG_H_ARGUMENT;
    if(d->first_error)return d->first_error;
    if(!d->attached)return FD2_DIG_H_SB16_TIMELINE;
    if(!d->playing)return 0;
    int16_t half=-1;
    if(d->sb16.read_half(d->sb16.user,&half)<0 || half < -1 || half > 1)
        return fail(d,FD2_DIG_H_SB16_READ);
    if(half==-1 || half==d->last_half)return 0;
    /* 无效借用结构是宿主H，先保原last/build/DMA；正常source已由start检查。 */
    for(unsigned h=0;h<FD2_DIG_HANDLES;h++) {
        const fd2_dig_sample *s=&d->samples[h];
        if(s->status==4 && (!s->data || !s->length || s->position>s->length))
            return fail(d,FD2_DIG_H_ARGUMENT);
    }
    d->last_half=half;memset(d->build,0,sizeof(d->build));d->entered_active=0;
    for(unsigned h=0;h<FD2_DIG_HANDLES;h++) {
        fd2_dig_sample *s=&d->samples[h];
        if(s->status!=4)continue;
        ++d->entered_active;
        int mixed=mix_one(d,s);if(mixed<0)return fail(d,mixed);
    }
    format_half(d,(unsigned)half^1u);
    if(d->entered_active)d->empty_services=0;
    else {
        uint32_t previous=d->empty_services++;
        if(previous==2 && d->playing) {
            if(d->sb16.request_pause(d->sb16.user)<0)return fail(d,FD2_DIG_H_SB16_PAUSE);
            d->playing=0;
        }
    }
    return 1;
}
void fd2_audio_dig_close(fd2_audio_dig *d) {
    if(!valid(d))return;
    /* App终态不推进时间；clock_close已拒绝以后IRQ，再释放borrowed callback。 */
    if(d->timer && d->timer_handle!=FD2_AIL_TIMER_INVALID)
        (void)fd2_ail_timer_release(d->timer,d->timer_handle);
    d->timer=NULL;d->timer_handle=FD2_AIL_TIMER_INVALID;d->attached=0;d->closed=1;
    memset(&d->sb16,0,sizeof(d->sb16));
}
