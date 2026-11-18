/* 显式external SB状态；期望来自404c0/49690/SB16原源，非理想DMA时间线。 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
static SDL_ThreadID test_thread=1;
static Uint64 input_time(void) {return 0;}
static SDL_ThreadID input_thread(void) {return test_thread;}
static bool input_current(SDL_Time *v) {*v=0;return false;}
static bool input_local(SDL_Time v,SDL_DateTime *out,bool local) {
    (void)v;(void)out;(void)local;return false;
}
#define SDL_GetTicksNS input_time
#define SDL_GetCurrentThreadID input_thread
#define SDL_GetCurrentTime input_current
#define SDL_TimeToDateTime input_local
#include "bios_clock.c"
#include "audio_pcm.h"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#x);return 0;}}while(0)
#define GAIN (101.0f/128.0f)
struct fd2_audio { uint64_t now; unsigned syncs; int fail; };
int fd2_audio_sync(fd2_audio *a) {++a->syncs;return a->fail?-1:0;}
uint64_t fd2_audio_now_ns(const fd2_audio *a) {return a->now;}
/* 此目标只测explicit external；新normalrenderer入口必须有符号且不可误调用。 */
static unsigned unexpected_normal_api;
int fd2_audio_is_original_mixer(const fd2_audio *a) {(void)a;unexpected_normal_api++;return 0;}
int fd2_audio_bind_dig(fd2_audio *a,struct fd2_audio_dig *d,fd2_dig_external_sb16 *external) {
    (void)a;(void)d;(void)external;unexpected_normal_api++;return -1;
}
int fd2_audio_quiesce_dig(fd2_audio *a) {(void)a;unexpected_normal_api++;return -1;}

typedef struct sb_input { int16_t half;unsigned starts,reads,pauses;int fail_start,fail_read,fail_pause;const uint8_t *dma; } sb_input;
static int start(void *u,const uint8_t *dma,uint32_t bytes) {
    sb_input *s=u;++s->starts;s->dma=dma;
    return bytes!=4096 || s->fail_start?-1:0;
}
static int read_half(void *u,int16_t *half) {
    sb_input *s=u;++s->reads;*half=s->half;return s->fail_read?-1:0;
}
static int pause_next_irq(void *u) {sb_input *s=u;++s->pauses;return s->fail_pause?-1:0;}
static void mdi(void *u) {++*(unsigned *)u;}
static unsigned mdi_calls;
static int setup(fd2_pcm_driver *d,fd2_pcm_player *p,fd2_pcm_player *q,struct fd2_audio *a,sb_input *s) {
    fd2_bios_clock_reset(0,0);test_thread=1;mdi_calls=0;
    CHECK(fd2_bios_clock_ail_attach(0,5966)==0);
    CHECK(fd2_bios_clock_install_mdi(0,mdi,&mdi_calls)==0);
    fd2_dig_external_sb16 provider={s,start,read_half,pause_next_irq};
    CHECK(fd2_pcm_driver_init_external(d,a,&provider)==0);
    CHECK(d->dig.timer_handle==4);
    CHECK(d->dig.timer->slots[0].period_us==8333 && d->dig.timer->slots[1].period_us==5000);
    CHECK(d->dig.timer->slots[1].user==&d->dig);
    CHECK(fd2_pcm_player_init_shared(p,d,0,11025)==0);
    CHECK(fd2_pcm_player_init_shared(q,d,1,11025)==0);
    return 1;
}
static void cleanup(fd2_pcm_driver *d) {(void)fd2_bios_clock_close();fd2_pcm_driver_close(d);}
static const uint8_t bank_bytes[]={ 'L','L','L','L','L','L',14,0,0,0,16,0,0,0,0,255,128,64,192 };
static int initial_and_missing(void) {
    fd2_audio_dig d,saved;fd2_audio_dig_init(&d);saved=d;
    CHECK(d.last_half==-1 && !d.empty_services && !d.playing);
    for(unsigned h=0;h<2;h++)for(unsigned i=0;i<2048;i++)CHECK(d.dma[h][i]==128);
    CHECK(fd2_audio_dig_attach_external(&d,NULL,0)==FD2_DIG_H_SB16_TIMELINE);
    CHECK(!memcmp(&d,&saved,sizeof(d)));
    CHECK(fd2_audio_dig_start(&d,0,bank_bytes,1,1)==FD2_DIG_H_SB16_TIMELINE);
    CHECK(!memcmp(&d,&saved,sizeof(d)));
    return 1;
}
static int shared_controls_and_tail(void) {
    fd2_pcm_driver d;fd2_pcm_player p,q;struct fd2_audio a={0};sb_input s={0};
    CHECK(setup(&d,&p,&q,&a,&s));
    fd2_pcm_bank bank={0};CHECK(fd2_pcm_bank_open_mem(&bank,bank_bytes,sizeof(bank_bytes))==0);
    CHECK(fd2_pcm_play_replace(&p,&bank,0,0,GAIN)==0 && s.starts==1);
    CHECK(fd2_pcm_play_replace(&q,&bank,1,0,GAIN)==0 && s.starts==1);
    CHECK(d.dig.samples[0].status==4 && d.dig.samples[1].status==4);
    CHECK(fd2_audio_dig_service(&d.dig)==1 && d.dig.last_half==0 && d.dig.entered_active==2);
    /* 第一sample[0]与第二[128]：sum=-25856，U8=27；当前half0保持128。 */
    CHECK(d.dig.build[0]==-25856 && d.dig.dma[1][0]==27);
    for(unsigned i=0;i<2048;i++)CHECK(d.dig.dma[0][i]==128);
    fd2_audio_dig saved=d.dig;unsigned reads=s.reads;
    CHECK(fd2_audio_dig_service(&d.dig)==0 && s.reads==reads+1);
    CHECK(!memcmp(&d.dig,&saved,sizeof(saved)));
    uint8_t tail[4096];memcpy(tail,d.dig.dma,sizeof(tail));
    uint32_t pos=d.dig.samples[0].position,loops=d.dig.samples[0].loop_count;
    CHECK(fd2_pcm_stop(&p)==0 && d.dig.samples[0].status==8);
    CHECK(d.dig.samples[0].position==pos && d.dig.samples[0].loop_count==loops);
    CHECK(d.dig.samples[1].status==4 && !memcmp(tail,d.dig.dma,sizeof(tail)));
    CHECK(fd2_pcm_play_replace(&q,&bank,99,1,GAIN)==FD2_DIG_H_ARGUMENT);
    CHECK(d.dig.samples[1].status==8 && !memcmp(tail,d.dig.dma,sizeof(tail)));
    CHECK(fd2_pcm_play_replace(&p,&bank,1,3,GAIN)==0 && s.starts==1);
    CHECK(d.dig.samples[0].status==4 && !d.dig.samples[0].position && d.dig.samples[0].loop_count==3);
    CHECK(!memcmp(tail,d.dig.dma,sizeof(tail)));
    CHECK(fd2_pcm_player_set_enabled(&p,0)==0);saved=d.dig;unsigned syncs=a.syncs;
    CHECK(fd2_pcm_play_replace(&q,NULL,99,-1,0.0f)==0 && fd2_pcm_stop(&q)==0 && a.syncs==syncs);
    CHECK(!memcmp(&saved,&d.dig,sizeof(saved)));
    CHECK(fd2_pcm_player_set_enabled(&q,1)==0);
    fd2_pcm_bank_close(&bank);cleanup(&d);return 1;
}
static int timer_new_half_and_empty(void) {
    fd2_pcm_driver d;fd2_pcm_player p,q;struct fd2_audio a={0};sb_input s={0};
    CHECK(setup(&d,&p,&q,&a,&s));
    static const uint8_t data[]={255};
    CHECK(fd2_audio_dig_start(&d.dig,0,data,1,1)==0);
    CHECK(fd2_bios_clock_advance(5000076)==0);
    CHECK(d.dig.last_half==0 && d.dig.samples[0].status==2 && d.dig.entered_active==1 && !d.dig.empty_services);
    CHECK(d.dig.dma[1][0]==228 && d.dig.dma[1][1]==128);
    CHECK(fd2_bios_clock_advance(10000151)==0 && !d.dig.empty_services); /* same supplied half */
    s.half=1;CHECK(fd2_bios_clock_advance(15000227)==0 && d.dig.empty_services==1);
    s.half=0;CHECK(fd2_bios_clock_advance(20000302)==0 && d.dig.empty_services==2);
    s.half=1;CHECK(fd2_bios_clock_advance(25000378)==0 && d.dig.empty_services==3);
    CHECK(s.pauses==1 && !d.dig.playing && mdi_calls==3);
    unsigned reads=s.reads;CHECK(fd2_bios_clock_advance(30000453)==0 && s.reads==reads);
    /* 此处provider未自动暂停/翻half：实际SB IRQ时间仍是欠项。 */
    CHECK(s.half==1);
    for(unsigned h=0;h<2;h++)for(unsigned i=0;i<2048;i++)CHECK(d.dig.dma[h][i]==128);
    cleanup(&d);return 1;
}
static int errors_keep_prefix(void) {
    static const uint8_t data[]={0,255};
    fd2_pcm_driver d;fd2_pcm_player p,q;struct fd2_audio a={0};sb_input s={0};
    CHECK(setup(&d,&p,&q,&a,&s));s.fail_start=1;
    CHECK(fd2_audio_dig_start(&d.dig,0,data,2,1)==FD2_DIG_H_SB16_START);
    CHECK(d.dig.samples[0].status==4 && !d.dig.playing && d.dig.first_error==FD2_DIG_H_SB16_START);
    CHECK(fd2_audio_dig_stop(&d.dig,0)==FD2_DIG_H_SB16_START && d.dig.samples[0].status==4);
    CHECK(fd2_bios_clock_advance(20000302)==0 && mdi_calls==2);
    cleanup(&d);
    memset(&a,0,sizeof(a));memset(&s,0,sizeof(s));CHECK(setup(&d,&p,&q,&a,&s));
    CHECK(fd2_audio_dig_start(&d.dig,0,data,2,1)==0);s.half=2;
    fd2_audio_dig saved=d.dig;
    CHECK(fd2_audio_dig_service(&d.dig)==FD2_DIG_H_SB16_READ);
    CHECK(d.dig.last_half==saved.last_half && !memcmp(d.dig.dma,saved.dma,sizeof(saved.dma)));
    CHECK(d.dig.samples[0].position==saved.samples[0].position && d.dig.first_error==FD2_DIG_H_SB16_READ);
    cleanup(&d);
    memset(&a,0,sizeof(a));memset(&s,0,sizeof(s));CHECK(setup(&d,&p,&q,&a,&s));
    CHECK(fd2_audio_dig_start(&d.dig,0,data,2,1)==0);CHECK(fd2_audio_dig_service(&d.dig)==1);
    s.half=1;CHECK(fd2_audio_dig_service(&d.dig)==1);s.half=0;CHECK(fd2_audio_dig_service(&d.dig)==1);
    s.half=1;s.fail_pause=1;CHECK(fd2_audio_dig_service(&d.dig)==FD2_DIG_H_SB16_PAUSE);
    CHECK(d.dig.empty_services==3 && d.dig.playing && d.dig.last_half==1 && s.pauses==1);
    for(unsigned h=0;h<2;h++)for(unsigned i=0;i<2048;i++)CHECK(d.dig.dma[h][i]==128);
    cleanup(&d);return 1;
}
static int read_u32(FILE *f,uint32_t *v) {
    uint8_t b[4];if(fread(b,1,4,f)!=4)return 0;
    *v=(uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;return 1;
}
static int vectors(const char *path) {
    FILE *f=fopen(path,"rb");CHECK(f);char magic[8];uint32_t n;
    CHECK(fread(magic,1,8,f)==8 && !memcmp(magic,"D2DH01\0",8));CHECK(read_u32(f,&n) && n==22);
    for(uint32_t c=0;c<n;c++) {
        fd2_pcm_driver d;fd2_pcm_player p,q;struct fd2_audio a={0};sb_input s={0};
        CHECK(setup(&d,&p,&q,&a,&s));uint8_t data[2][8192],expected_dma[2048];uint32_t expected[2][4],entered;
        for(unsigned h=0;h<2;h++) {
            fd2_dig_sample *v=&d.dig.samples[h];
            CHECK(read_u32(f,&v->status) && read_u32(f,&v->position) && read_u32(f,&v->loop_count) && read_u32(f,&v->length));
            CHECK(v->length<=8192 && fread(data[h],1,v->length,f)==v->length);v->data=data[h];
        }
        CHECK(read_u32(f,&entered));for(unsigned h=0;h<2;h++)for(unsigned k=0;k<4;k++)CHECK(read_u32(f,&expected[h][k]));
        int32_t build[2048];for(unsigned i=0;i<2048;i++){uint32_t b;CHECK(read_u32(f,&b));memcpy(&build[i],&b,4);}
        CHECK(fread(expected_dma,1,2048,f)==2048);d.dig.playing=1;
        CHECK(fd2_audio_dig_service(&d.dig)==1 && d.dig.entered_active==entered);
        for(unsigned h=0;h<2;h++)CHECK(d.dig.samples[h].status==expected[h][0] && d.dig.samples[h].position==expected[h][1] && d.dig.samples[h].loop_count==expected[h][2]);
        CHECK(!memcmp(build,d.dig.build,sizeof(build)) && !memcmp(expected_dma,d.dig.dma[1],2048));
        for(unsigned i=0;i<2048;i++)CHECK(d.dig.dma[0][i]==128);
        cleanup(&d);
    }
    CHECK(!unexpected_normal_api);CHECK(fgetc(f)==EOF);fclose(f);printf("shared DIG: 22 retained independent full-half vectors PASS\n");return 1;
}
#include "pcm_assets_probe.h"

int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--assets-stream"))return assets_stream()?0:1;
    if(argc==2)return vectors(argv[1])?0:1;
    if(argc!=1)return 2;
    if(!initial_and_missing() || !shared_controls_and_tail() || !timer_new_half_and_empty() || !errors_keep_prefix())return 1;
    if(unexpected_normal_api)return 1;
    puts("shared DIG: owner/gates/timer/new-half/stop8/tail/error-prefix PASS; external provider only");return 0;
}
