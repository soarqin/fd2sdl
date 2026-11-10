/* G4 1f1cc/1f30a/4eb59：真实80/81/82，46帧全indexed/DAC及整page写集。
 * 替身仅等待/原工作页投影/closing重画/清键，不开窗口、不代替原版运行。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_round.h"

#define CHECK(x) do { if(!(x)){fprintf(stderr,"field_phase_test:%d %s\n",__LINE__,#x);return -1;} } while(0)
typedef struct {
    fd2_field_page *page;const fd2_field_info_assets *assets;int kind;
    uint8_t original[64000],map[64000],after[64000],palette[768];
    uint8_t page_before[FD2_FIELD_PAGE_SIZE];
    unsigned waits,backgrounds,clears,redraws;int fail_wait,fail_result;
} phase_probe;

static void image(uint8_t *pixels,const fd2_image *sprite,int x,int y) {
    for(int sy=0;sy<sprite->height;sy++)for(int sx=0;sx<sprite->width;sx++) {
        int px=x+sx,py=y+sy;uint8_t color=sprite->pixels[(size_t)sy*(size_t)sprite->width+(size_t)sx];
        if(color && px>=4 && px<316 && py>=4 && py<196)pixels[(size_t)py*320u+(size_t)px]=color;
    }
}
static void labels(uint8_t *pixels,const phase_probe *p,int offset) {
    image(pixels,&p->assets->phase_labels[p->kind?2:0],89-offset,86);
    image(pixels,&p->assets->phase_labels[1],169+offset,86);
}
static void mosaic(uint8_t *pixels,const phase_probe *p,unsigned factor) {
    memset(pixels,0,64000);
    /* 独立按块填充；不复用consumer的逐像素除法或读取冷JSON。 */
    for(unsigned y=4;y<196;y+=factor)for(unsigned x=4;x<316;x+=factor) {
        uint8_t value=p->original[y*320u+x];
        for(unsigned dy=0;dy<factor && y+dy<196;dy++)
            for(unsigned dx=0;dx<factor && x+dx<316;dx++)pixels[(y+dy)*320u+x+dx]=value;
    }
    labels(pixels,p,0);
}
static int wait_frame(fd2_vga *vga,uint32_t ms,unsigned ticks,void *userdata) {
    phase_probe *p=userdata;unsigned n=p->waits++;uint8_t expected[64000];int depth=-1;
    CHECK(n<46u);
    if(n<7u || n>=41u) {
        const unsigned opening[]={100,75,50,25,0,1,0};
        int close=n>=41u;unsigned offset=close?(n-41u)*25u:opening[n];
        if(close){memset(expected,0,sizeof(expected));depth=0;}else memcpy(expected,p->original,sizeof(expected));
        const uint8_t *map=close?p->after:p->map;
        for(unsigned y=4;y<196;y++)memcpy(expected+y*320u+4u,map+y*320u+4u,312u);
        labels(expected,p,(int)offset);
    } else {
        unsigned factor;
        if(n<=22u){factor=n-6u;depth=(int)n-7;}
        else if(n==23u){factor=16;depth=15;}
        else {factor=41u-n;depth=40-(int)n;}
        mosaic(expected,p,factor);
    }
    if(n==23u)CHECK(ms==(p->kind?20u:150u) && !ticks && p->clears==1u);
    else CHECK(!ms && ticks==1u);
    CHECK(!memcmp(expected,vga->framebuffer,sizeof(expected)));
    for(unsigned i=0;i<768u;i++) {
        uint8_t wanted=52;
        if(i>=48u && depth>=0){unsigned value=p->palette[i]>(unsigned)depth?p->palette[i]-(unsigned)depth:0;wanted=(uint8_t)((value<<2)|(value>>4));}
        CHECK(vga->dac[i]==wanted);
    }
    CHECK(p->backgrounds==(n>=41u?2u:1u) && p->redraws==(n>=41u?1u:0u));
    return p->fail_wait==(int)n?p->fail_result:0;
}
static int background(fd2_vga *vga,void *userdata) {
    phase_probe *p=userdata;p->backgrounds++;
    CHECK((p->backgrounds==1u && p->waits==0u) || (p->backgrounds==2u && p->waits==41u));
    return fd2_field_page_project(p->page,72,72,vga->framebuffer,sizeof(vga->framebuffer));
}
static int redraw(fd2_vga *vga,void *userdata) {
    (void)vga;phase_probe *p=userdata;p->redraws++;CHECK(p->redraws==1u && p->waits==41u);
    return fd2_field_page_write(p->page,72,72,312,192,p->after+1284u,64000u-1284u,320u);
}
static int clear_keys(fd2_vga *vga,void *userdata) {
    (void)vga;phase_probe *p=userdata;p->clears++;CHECK(p->clears==1u && p->waits==23u);return 0;
}
static const fd2_field_round_hooks hooks={.wait=wait_frame,.render_background=background,.phase_redraw=redraw,.clear_keys=clear_keys};
static int init(fd2_vga *vga,phase_probe *p,const fd2_field_info_assets *assets,int kind) {
    fd2_field_page *page=p->page;memset(p,0,sizeof(*p));p->page=page;p->assets=assets;p->kind=kind;p->fail_wait=-1;
    memset(vga,0,sizeof(*vga));
    for(size_t i=0;i<64000u;i++){p->original[i]=(uint8_t)(19u+i%31u);p->map[i]=(uint8_t)(67u+i%39u);p->after[i]=(uint8_t)(129u+i%43u);}
    for(size_t i=0;i<FD2_FIELD_PAGE_SIZE;i++)p->page_before[i]=(uint8_t)(i*3u+17u);
    CHECK(fd2_field_page_write(page,0,0,456,336,p->page_before,sizeof(p->page_before),456)==0);
    CHECK(fd2_field_page_write(page,72,72,312,192,p->map+1284u,64000u-1284u,320u)==0);
    CHECK(fd2_field_page_read(page,0,0,456,336,p->page_before,sizeof(p->page_before),456)==0);
    memcpy(vga->framebuffer,p->original,64000u);for(size_t i=0;i<768u;i++)p->palette[i]=vga->palette[i]=(uint8_t)(i%64u);
    memset(vga->dac,52,sizeof(vga->dac));vga->field_water_phase=13;vga->field_water_last_tick=777;
    vga->actor_jitter_phase=1;vga->actor_jitter_tick=92;vga->frame_deadline_ns=123456;return 0;
}
static int other_fields_unchanged(const fd2_vga *vga,const fd2_vga *before) {
    fd2_vga normalized=*vga;memcpy(normalized.framebuffer,before->framebuffer,sizeof(normalized.framebuffer));
    memcpy(normalized.dac+48,before->dac+48,sizeof(normalized.dac)-48u);return !memcmp(&normalized,before,sizeof(normalized));
}
static int final_page(const phase_probe *p) {
    uint8_t *expected=malloc(FD2_FIELD_PAGE_SIZE),*actual=malloc(FD2_FIELD_PAGE_SIZE);CHECK(expected && actual);
    memcpy(expected,p->page_before,FD2_FIELD_PAGE_SIZE);memcpy(expected,p->original,64000u);
    for(size_t y=0;y<192u;y++)memcpy(expected+(72u+y)*456u+72u,p->after+(4u+y)*320u+4u,312u);
    CHECK(fd2_field_page_read(p->page,0,0,456,336,actual,FD2_FIELD_PAGE_SIZE,456)==0);
    CHECK(!memcmp(expected,actual,FD2_FIELD_PAGE_SIZE));free(actual);free(expected);return 0;
}
int main(void) {
    fd2_archive archive={0};fd2_field_info_assets assets={0};CHECK(fd2_archive_open(&archive,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_field_info_assets_open(&assets,&archive)==0 && fd2_field_phase_is_ready(&assets));
    fd2_vga *vga=calloc(1,sizeof(*vga)),*before=calloc(1,sizeof(*before));phase_probe *p=calloc(1,sizeof(*p));CHECK(vga && before && p);
    p->page=fd2_field_page_create();CHECK(p->page);
    for(int kind=0;kind<2;kind++) {
        CHECK(init(vga,p,&assets,kind)==0);*before=*vga;
        CHECK(fd2_field_phase_play(vga,&assets,p->page,(fd2_field_phase_kind)kind,&hooks,p)==0);
        CHECK(p->waits==46u && p->backgrounds==2u && p->clears==1u && p->redraws==1u && other_fields_unchanged(vga,before));
        CHECK(final_page(p)==0);
    }
    for(int stop=0;stop<46;stop++)for(int result=-1;result<=1;result+=2) {
        CHECK(init(vga,p,&assets,0)==0);*before=*vga;p->fail_wait=stop;p->fail_result=result;
        CHECK(fd2_field_phase_play(vga,&assets,p->page,FD2_FIELD_PHASE_PLAYER,&hooks,p)==result);
        CHECK(p->waits==(unsigned)stop+1u && p->clears==(stop>=23?1u:0u) &&
            p->redraws==(stop>=41?1u:0u) && other_fields_unchanged(vga,before));
    }
    for(size_t i=0;i<3u;i++) {
        CHECK(init(vga,p,&assets,0)==0);*before=*vga;
        uint8_t *old=assets.phase_labels[i].pixels;assets.phase_labels[i].pixels=NULL;
        CHECK(fd2_field_phase_play(vga,&assets,p->page,FD2_FIELD_PHASE_PLAYER,&hooks,p)==-1 && !p->waits && !memcmp(vga,before,sizeof(*vga)));
        assets.phase_labels[i].pixels=old;
    }
    CHECK(init(vga,p,&assets,0)==0);*before=*vga;
    CHECK(fd2_field_phase_play(vga,&assets,NULL,FD2_FIELD_PHASE_PLAYER,&hooks,p)==-1 && !memcmp(vga,before,sizeof(*vga)));
    CHECK(fd2_field_phase_play(vga,&assets,p->page,(fd2_field_phase_kind)9,&hooks,p)==-1 && !memcmp(vga,before,sizeof(*vga)));
    fd2_field_info_assets_close(&assets);fd2_archive_close(&archive);fd2_field_page_destroy(p->page);free(p);free(before);free(vga);
    puts("field_phase_test: real labels/46 indexed+DAC frames/page prefix/92 wait failures passed");return 0;
}
