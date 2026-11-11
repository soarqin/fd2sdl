/* G4：10010/1a30b真实83..91 LMI和bank42数字的完整indexed帧对照。
 * 只替换等待/工作页投影/尾刷新，不开窗口；不替代原版WGC或Game接线。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "field_round.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"field_round_test:%d %s\n",__LINE__,#x); return -1; } } while (0)

typedef struct {
    const fd2_field_info_assets *assets;
    uint32_t turn;
    uint8_t intro[VGA_W * VGA_H];
    uint8_t map[VGA_W * VGA_H];
    uint8_t tail[VGA_W * VGA_H];
    unsigned waits, backgrounds, refreshes;
    int fail_wait, fail_result, fail_background, fail_refresh;
} probe;

static void image_expected(uint8_t *pixels, const fd2_image *image, int x, int y) {
    for (int row=0;row<image->height;row++) for (int col=0;col<image->width;col++) {
        uint8_t value=image->pixels[(size_t)row*(size_t)image->width+(size_t)col];
        if(value && x+col>=0 && x+col<VGA_W && y+row>=0 && y+row<VGA_H)
            pixels[(size_t)(y+row)*VGA_W+(size_t)(x+col)]=value;
    }
}

static void number_expected(uint8_t *pixels,const probe *p,int x,int y) {
    uint32_t value=(p->turn & 0x80000000u)?0u:p->turn;
    if(value>999u)image_expected(pixels,&p->assets->digit_overflow[1],x,y);
    else {
        unsigned digit[3]={value/100u,value/10u%10u,value%10u};
        for(unsigned i=0;i<3u;i++)image_expected(pixels,&p->assets->digits[1][digit[i]],x+(int)i*6,y);
    }
}

static int wait_frame(fd2_vga *vga,uint32_t ms,unsigned ticks,void *userdata) {
    probe *p=userdata;unsigned index=p->waits++;
    CHECK(index<15u);
    uint8_t expected[VGA_W*VGA_H];
    if(index<10u) {
        CHECK(ms==(index==9u?500u:70u) && ticks==0u);
        memcpy(expected,p->intro,sizeof(expected));
        unsigned frame=index<9u?index:8u;
        image_expected(expected,&p->assets->round_banner[frame],120,84);
        if(index>=7u)number_expected(expected,p,171,91);
    } else if(index<14u) {
        const unsigned m[4]={2,3,4,9};unsigned offset=m[index-10u]*m[index-10u];
        CHECK(ms==0u && ticks==1u && p->backgrounds==1u);
        memcpy(expected,p->intro,sizeof(expected));
        for(int y=4;y<196;y++)memcpy(expected+(size_t)y*320u+4u,p->map+(size_t)y*320u+4u,312u);
        image_expected(expected,&p->assets->round_banner[8],120,88+(int)offset);
        number_expected(expected,p,171,94+(int)offset);
    } else {
        CHECK(ms==200u && ticks==0u && p->refreshes==1u);
        memcpy(expected,p->tail,sizeof(expected));
    }
    CHECK(!memcmp(expected,vga->framebuffer,sizeof(expected)));
    return p->fail_wait==(int)index?p->fail_result:0;
}

static int background(fd2_vga *vga,void *userdata) {
    probe *p=userdata;p->backgrounds++;
    CHECK(p->backgrounds==1u && p->waits==10u);
    if(p->fail_background)return p->fail_background;
    memcpy(vga->framebuffer,p->map,sizeof(p->map));return 0;
}
static int refresh(fd2_vga *vga,void *userdata) {
    probe *p=userdata;p->refreshes++;
    CHECK(p->backgrounds==1u && p->waits==14u);
    if(p->fail_refresh)return p->fail_refresh;
    memcpy(vga->framebuffer,p->tail,sizeof(p->tail));return 0;
}
static const fd2_field_round_hooks hooks={.wait=wait_frame,.render_background=background,.refresh=refresh};

static void init(fd2_vga *vga,probe *p,const fd2_field_info_assets *assets,uint32_t turn) {
    memset(vga,0,sizeof(*vga));memset(p,0,sizeof(*p));p->assets=assets;p->turn=turn;p->fail_wait=-1;
    for(size_t i=0;i<VGA_W*VGA_H;i++) {
        p->intro[i]=(uint8_t)(31u+i%37u);p->map[i]=(uint8_t)(81u+i%41u);p->tail[i]=(uint8_t)(141u+i%43u);
    }
    memcpy(vga->framebuffer,p->intro,sizeof(p->intro));
    memset(vga->palette,23,sizeof(vga->palette));memset(vga->dac,52,sizeof(vga->dac));
    vga->frame_deadline_ns=5678;vga->frame_interval_ns=4321;
    vga->field_water_phase=12;vga->field_water_last_tick=37;
    vga->actor_jitter_phase=1;vga->actor_jitter_tick=99;
}

static int check_nonpixels(const fd2_vga *vga,const fd2_vga *before) {
    fd2_vga copy=*vga;memcpy(copy.framebuffer,before->framebuffer,sizeof(copy.framebuffer));
    return !memcmp(&copy,before,sizeof(copy));
}

typedef struct { int calls,stop,result;fd2_field_turn_step steps[32];int values[32]; } turn_probe;
static int turn_apply(fd2_field_turn_step step,int value,void *userdata) {
    turn_probe *p=userdata;int n=p->calls++;
    if (n>=32) return -1;
    p->steps[n]=step;p->values[n]=value;
    return n==p->stop?p->result:0;
}
static int check_turn_driver(void) {
    /* 对照1a30b各原caller边界；恢复三wait属于整组，非每actor。 */
    static const fd2_field_turn_step expected[][10]={
        {FD2_FIELD_TURN_RECOVERY_WAIT,FD2_FIELD_TURN_RECOVERY_HIGHLIGHT,FD2_FIELD_TURN_RECOVERY_CUE,
         FD2_FIELD_TURN_RECOVERY_WAIT,FD2_FIELD_TURN_RECOVERY_COMMIT,FD2_FIELD_TURN_RECOVERY_PROJECT,
         FD2_FIELD_TURN_RECOVERY_WAIT,FD2_FIELD_TURN_REFRESH,FD2_FIELD_TURN_ENTER_PHASE},
        {FD2_FIELD_TURN_STOP_IF_DIFFERENT,FD2_FIELD_TURN_ENEMY_TITLE,FD2_FIELD_TURN_CLEAR_ACTED,
         FD2_FIELD_TURN_REFRESH,FD2_FIELD_TURN_ENTER_PHASE},
        {FD2_FIELD_TURN_INCREMENT,FD2_FIELD_TURN_STOP_IF_DIFFERENT,FD2_FIELD_TURN_PLAYER_TITLE,
         FD2_FIELD_TURN_DISPLAY_MODE,FD2_FIELD_TURN_CLEAR_ACTED,FD2_FIELD_TURN_REFRESH,
         FD2_FIELD_TURN_PRIMARY_MUSIC,FD2_FIELD_TURN_ROUND,FD2_FIELD_TURN_FOCUS_RESET,FD2_FIELD_TURN_ENTER_PHASE},
        {FD2_FIELD_TURN_DISPLAY_MODE,FD2_FIELD_TURN_FOCUS_ZERO,FD2_FIELD_TURN_CLEAR_KEYS},
        {FD2_FIELD_TURN_INFO_VISIBLE},
        {FD2_FIELD_TURN_RESET_DIRECTIONS,FD2_FIELD_TURN_DELAY_MS},
        {FD2_FIELD_TURN_RESET_DIRECTIONS,FD2_FIELD_TURN_DELAY_MS,FD2_FIELD_TURN_REFRESH}};
    const int counts[]={9,5,10,3,1,2,3};
    for (unsigned entry=0;entry<7;entry++) {
        turn_probe p={0};p.stop=-1;
        CHECK(fd2_field_turn_run((fd2_field_turn_entry)entry,turn_apply,&p)==0 && p.calls==counts[entry]);
        CHECK(!memcmp(p.steps,expected[entry],(size_t)p.calls*sizeof(p.steps[0])));
        if (!entry) CHECK(p.values[0]==1 && p.values[3]==0 && p.values[6]==0 && p.values[8]==1);
        if (entry==2) CHECK(p.values[3]==0 && p.values[9]==2);
        if (entry==3) CHECK(p.values[0]==1);
        if (entry==4) CHECK(p.values[0]==1);
        if (entry>=5) CHECK(p.values[0]==0 && p.values[1]==20);
        if (entry==6) CHECK(p.values[2]==0);
        for (int stop=0;stop<counts[entry];stop++) for (int result=-1;result<=1;result+=2) {
            turn_probe stopped={0};stopped.stop=stop;stopped.result=result;
            CHECK(fd2_field_turn_run((fd2_field_turn_entry)entry,turn_apply,&stopped)==result && stopped.calls==stop+1);
        }
    }
    turn_probe p={0};p.stop=-1;
    CHECK(fd2_field_turn_run((fd2_field_turn_entry)7,turn_apply,&p)==-1 && !p.calls);
    CHECK(fd2_field_turn_run(FD2_FIELD_TURN_PLAYER_DONE,NULL,&p)==-1 && !p.calls);
    return 0;
}

typedef struct { int calls,stop,result,exit_port,waits,ticks,reads,finishes,draws,field,snapshots,redraws;int offsets[16]; } choice_probe;
static int choice_apply(fd2_field_choice_step step,int value,void *userdata) {
    choice_probe *p=userdata;int n=p->calls++;
    if (n==p->stop) return p->result;
    if (step==FD2_FIELD_CHOICE_DRAW || step==FD2_FIELD_CHOICE_STEADY) {if(p->draws>=16)return -1;p->offsets[p->draws++]=value;}
    else if (step==FD2_FIELD_CHOICE_WAIT_KEY) {p->waits++;return p->ticks>=2;}
    else if (step==FD2_FIELD_CHOICE_READ_KEY) return p->reads++?p->exit_port:0;
    else if (step==FD2_FIELD_CHOICE_TICK) {p->ticks++;return 1;}
    else if (step==FD2_FIELD_CHOICE_FINISH) p->finishes++;
    else if (step==FD2_FIELD_CHOICE_HAS_FIELD) return p->field;
    else if (step==FD2_FIELD_CHOICE_SNAPSHOT) p->snapshots++;
    else if (step==FD2_FIELD_CHOICE_FIELD_REDRAW) p->redraws++;
    return 0;
}
static int check_choice_driver(void) {
    const int offsets[]={4,8,12,16,16,16,12,8,4,0};
    for (int field=0;field<=1;field++) for (int exit_port=1;exit_port<=2;exit_port++) {
        choice_probe p={0};p.stop=-1;p.exit_port=exit_port;p.field=field;
        CHECK(fd2_field_choice_run(choice_apply,&p)==0 && p.draws==10 && p.ticks==2 && p.waits==4 && p.reads==2 && p.finishes==1);
        CHECK(p.snapshots==(field?1:3) && p.redraws==(field?4:0));
        CHECK(!memcmp(offsets,p.offsets,sizeof(offsets)));
        for (int stop=0;stop<p.calls;stop++) for(int mode=0;mode<2;mode++) {
            choice_probe stopped={0};stopped.stop=stop;stopped.result=mode?3:-1;stopped.exit_port=exit_port;stopped.field=field;
            CHECK(fd2_field_choice_run(choice_apply,&stopped)==(mode?1:-1) && stopped.calls==stop+1);
        }
    }
    CHECK(fd2_field_choice_run(NULL,NULL)==-1);return 0;
}

int main(void) {
    CHECK(check_turn_driver()==0 && check_choice_driver()==0);
    fd2_archive archive={0};fd2_field_info_assets assets={0};
    CHECK(fd2_archive_open(&archive,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_field_info_assets_open(&assets,&archive)==0 && fd2_field_round_is_ready(&assets));
    fd2_vga *vga=calloc(1,sizeof(*vga)),*before=calloc(1,sizeof(*before));probe *p=calloc(1,sizeof(*p));
    CHECK(vga && before && p);
    const uint32_t turns[]={0,2,999,1000,UINT32_MAX};
    for(size_t i=0;i<sizeof(turns)/sizeof(turns[0]);i++) {
        init(vga,p,&assets,turns[i]);*before=*vga;
        CHECK(fd2_field_round_play(vga,&assets,turns[i],&hooks,p)==0);
        CHECK(p->waits==15u && p->backgrounds==1u && p->refreshes==1u && check_nonpixels(vga,before));
    }
    /* 每一个已呈现等待边界分别退出/报错，不继续后续回调或重放。 */
    for(int result=-1;result<=1;result+=2)for(int stop=0;stop<15;stop++) {
        init(vga,p,&assets,2);*before=*vga;p->fail_wait=stop;p->fail_result=result;
        CHECK(fd2_field_round_play(vga,&assets,2,&hooks,p)==result);
        CHECK(p->waits==(unsigned)stop+1u && p->backgrounds==(stop>=10?1u:0u) &&
            p->refreshes==(stop>=14?1u:0u) && check_nonpixels(vga,before));
    }
    for(int stop=0;stop<2;stop++)for(int result=-1;result<=1;result+=2) {
        init(vga,p,&assets,2);*before=*vga;
        if(stop)p->fail_refresh=result;else p->fail_background=result;
        CHECK(fd2_field_round_play(vga,&assets,2,&hooks,p)==result && p->waits==(stop?14u:10u));
        CHECK(check_nonpixels(vga,before));
    }
    for(size_t i=0;i<9u;i++) {
        init(vga,p,&assets,2);*before=*vga;uint8_t *old=assets.round_banner[i].pixels;assets.round_banner[i].pixels=NULL;
        CHECK(!fd2_field_round_is_ready(&assets));
        CHECK(fd2_field_round_play(vga,&assets,2,&hooks,p)==-1 && !p->waits && !p->backgrounds && !p->refreshes && !memcmp(vga,before,sizeof(*vga)));
        assets.round_banner[i].pixels=old;
    }
    init(vga,p,&assets,2);*before=*vga;
    fd2_field_round_hooks broken=hooks;broken.refresh=NULL;
    CHECK(fd2_field_round_play(vga,&assets,2,&broken,p)==-1 && !memcmp(vga,before,sizeof(*vga)));
    fd2_field_info_assets_close(&assets);CHECK(!fd2_field_round_is_ready(&assets));fd2_archive_close(&archive);
    free(p);free(before);free(vga);puts("field_round_test: real resources/full indexed frames/15 waits/owner and failure boundaries passed");return 0;
}
