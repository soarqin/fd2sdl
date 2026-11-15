#include "field_visual.h"
#include <stdint.h>
/* S2 pal_partial_set11df2/bios_tick_delay17aa9：实际VGA helper与私有Play叶。
 * 时钟/呈现为显式替身；不创建窗口，不冒充真实DAC/BIOS/WGC验收。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "field_play.h"
#include "scene.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return -1; } } while(0)
static uint16_t clocks[16];static unsigned clock_count,clock_at,frames,uploads,quit_frame,exhausted;
static unsigned rng_calls;
static unsigned ms_mode,ms_now,ms_requests[16],ms_request_count;
static uint32_t count_rng(void *userdata) { (*(unsigned *)userdata)++;return 7; }
static uint64_t test_ticks(void) {
    if(ms_mode)return ms_now;
    if(clock_at>=clock_count){exhausted=1;return (uint64_t)clocks[clock_count?clock_count-1:0]*55u;}
    return (uint64_t)clocks[clock_at++]*55u;
}
static void test_present_timed(fd2_vga *vga,uint32_t ms) {
    if(ms_mode) {
        if(ms_request_count<16)ms_requests[ms_request_count++]=ms;
        ms_now+=ms;
    }
    frames++;
    if((quit_frame && frames==quit_frame) || frames>32)vga->input.quit_requested=1;
}
static void test_upload(fd2_vga *vga) {(void)vga;uploads++;}
#define SDL_GetTicks test_ticks
uint64_t test_ticks_ns(void);
#define SDL_GetTicksNS test_ticks_ns
#define fd2_field_water_tick_live(vga_) fd2_field_water_tick((vga_),SDL_GetTicksNS())
#define fd2_vga_present_timed test_present_timed
#define fd2_vga_upload_current_frame test_upload
#include "field_play.c"
#undef SDL_GetTicks
#undef fd2_vga_present_timed
#undef fd2_vga_upload_current_frame

static void palette(fd2_vga *vga,unsigned seed) {
    memset(vga,0,sizeof(*vga));
    for(unsigned i=0;i<768;i++){vga->palette[i]=(uint8_t)((i*17u+seed)%64u);vga->dac[i]=0xa5;}
    memset(vga->framebuffer,0x96,sizeof(vga->framebuffer));
    vga->field_water_last_tick=123;vga->field_water_phase=2;vga->actor_jitter_tick=77;
    vga->input.queued_count=1;vga->input.queued_keys[0].key=FD2_INPUT_KEY_Z;
}

static void reset_clock(uint16_t start,const uint16_t *polls,unsigned count,uint16_t tail) {
    memset(clocks,0,sizeof(clocks));clocks[0]=start;
    memcpy(clocks+1,polls,count*sizeof(*polls));clocks[count+1]=tail;
    clock_count=count+2;clock_at=frames=uploads=quit_frame=exhausted=0;
}

static int vectors(void) {
    char line[512],kind[16],extra;fd2_vga *vga=malloc(sizeof(*vga)),*before=malloc(sizeof(*before));
    if(!vga || !before){free(vga);free(before);return 1;}int failed=0;
    while(fgets(line,sizeof(line),stdin)) {
        if(sscanf(line,"%15s",kind)!=1){failed=1;break;}
        if(!strcmp(kind,"palette") || !strcmp(kind,"subtract") || !strcmp(kind,"brightness")) {
            int first,last,add;unsigned seed;
            if(sscanf(line,"%15s %d %d %d %u %c",kind,&first,&last,&add,&seed,&extra)!=5){failed=1;break;}
            palette(vga,seed);*before=*vga;int status;
            if(!strcmp(kind,"brightness")) {
                if(first!=0 || last!=255){failed=1;break;}
                fd2_vga_set_brightness(vga,add);status=0;
            } else status=!strcmp(kind,"palette")?fd2_vga_palette_add(vga,first,last,add):
                fd2_vga_palette_subtract(vga,first,last,add);
            printf("{\"kind\":\"%s\",\"status\":%d,\"dac_hex\":\"",kind,status);
            for(unsigned i=0;i<768;i++)printf("%02x",vga->dac[i]);
            memcpy(before->dac,vga->dac,768);
            printf("\",\"other_unchanged\":%s}\n",!memcmp(before,vga,sizeof(*vga))?"true":"false");
        } else if(!strcmp(kind,"ticks")) {
            int goal,used;unsigned start,count,tail;uint16_t polls[12];
            if(sscanf(line,"%15s %d %u %u %n",kind,&goal,&start,&count,&used)!=4 || count<1 || count>12 || start>65535){failed=1;break;}
            char *at=line+used;
            for(unsigned i=0;i<count;i++) {
                unsigned value;
                if(sscanf(at,"%u %n",&value,&used)!=1 || value>65535){failed=1;break;}
                polls[i]=(uint16_t)value;at+=used;
            }
            if(failed || sscanf(at,"%u %c",&tail,&extra)!=1 || tail>65535){failed=1;break;}
            palette(vga,0);*before=*vga;reset_clock((uint16_t)start,polls,count,(uint16_t)tail);
            int status=common_event_delay_ticks(vga,goal,0);
            printf("{\"kind\":\"ticks\",\"status\":%d,\"clock_reads\":%u,\"frames\":%u,\"owner_unchanged\":%s,\"script_exact\":%s}\n",
                status,clock_at,frames,!memcmp(before,vga,sizeof(*vga))?"true":"false",
                !exhausted && clock_at==clock_count?"true":"false");
        } else {failed=1;break;}
    }
    if(ferror(stdin))failed=1;free(vga);free(before);return failed;
}

static int safety(void) {
    fd2_vga *vga=malloc(sizeof(*vga)),*before=malloc(sizeof(*before));CHECK(vga && before);
    palette(vga,0);*before=*vga;
    CHECK(fd2_vga_palette_add(vga,0,255,-1)==-1 && !memcmp(vga,before,sizeof(*vga)));
    CHECK(fd2_vga_palette_add(vga,0,255,256)==-1 && !memcmp(vga,before,sizeof(*vga)));
    CHECK(fd2_vga_palette_add(vga,-1,255,0)==-1 && fd2_vga_palette_add(vga,0,256,0)==-1);
    vga->palette[700]=64;*before=*vga;
    CHECK(fd2_vga_palette_add(vga,0,255,63)==-1 && !memcmp(vga,before,sizeof(*vga)));
    CHECK(fd2_vga_palette_add(vga,9,5,255)==0 && !memcmp(vga,before,sizeof(*vga)));
    uint16_t now=100;reset_clock(100,&now,1,100);*before=*vga;
    CHECK(common_event_delay_ticks(vga,65536,0)==FD2_SCENE_RESULT_ERROR && !clock_at && !memcmp(vga,before,sizeof(*vga)));
    CHECK(common_event_delay_ticks(vga,1,1)==0 && !clock_at && !memcmp(vga,before,sizeof(*vga)));
    vga->input.quit_requested=1;CHECK(common_event_delay_ticks(vga,1,1)==FD2_SCENE_RESULT_HOST_QUIT && !clock_at);
    fd2_field_game game={0};fd2_archive other={0};
    fd2_static_flow_node node={.op="delay_ticks",.args={-1}};
    CHECK(common_event_resources_preflight(&game,&node,NULL,&other)==0);
    node.args[0]=65536;CHECK(common_event_resources_preflight(&game,&node,NULL,&other)==-1);
    node=(fd2_static_flow_node){.op="palette_add",.args={0,255,255}};
    CHECK(common_event_resources_preflight(&game,&node,NULL,&other)==0);
    node.args[2]=-1;CHECK(common_event_resources_preflight(&game,&node,NULL,&other)==-1);
    free(vga);free(before);return 0;
}

static const fd2_static_flow_node play_nodes[]={
    {.id="flash",.op="palette_add",.args={0,255,255},.edges={1,-1,-1,-1,-1,-1,-1,4}},
    {.id="wait",.op="delay_ticks",.args={1},.edges={2,-1,-1,-1,-1,-1,-1,4}},
    {.id="restore",.op="palette_add",.args={0,255,0},.edges={3,-1,-1,-1,-1,-1,-1,4}},
    {.id="end",.op="end",.edges={-1,-1,-1,-1,-1,-1,-1,-1}},
    {.id="error",.op="fail",.edges={-1,-1,-1,-1,-1,-1,-1,-1}}};
static const fd2_static_flow play_flow={"test.s2.visual",play_nodes,5,0};

static int play_consumer(void) {
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));
    fd2_vga *vga=malloc(sizeof(*vga));CHECK(game && before && vga);fd2_archive other={0};
    for(unsigned mode=0;mode<3;mode++) {
        memset(game,0,sizeof(*game));game->ready=1;game->units.count=1;
        rng_calls=0;game->attack_rng=count_rng;game->attack_rng_userdata=&rng_calls;
        game->units.items[0].side=2;game->event_resume=(fd2_field_event_resume){.flow=&play_flow,.active=1};
        *before=*game;palette(vga,0);uint16_t polls[]={100,101};reset_clock(100,polls,2,101);
        if(mode==1)quit_frame=1;
        if(mode==2)vga->palette[700]=64;
        int status=play_common_events(game,vga,&other,NULL,0);
        if(mode==0) {
            CHECK(status==0 && !game->event_resume.active && uploads==2 && frames==1 && clock_at==4 && !exhausted);
            for(unsigned i=0;i<768;i++)CHECK(vga->dac[i]==(uint8_t)((vga->palette[i]<<2)|(vga->palette[i]>>4)));
            CHECK(fd2_field_game_common_event_reply(game,FD2_FLOW_NEXT)==-1);
        } else if(mode==1) {
            CHECK(status==FD2_SCENE_RESULT_HOST_QUIT && game->runtime_host_quit && game->event_resume.pc==1 && uploads==1);
            for(unsigned i=0;i<768;i++)CHECK(vga->dac[i]==255);
        } else {
            CHECK(status==FD2_SCENE_RESULT_ERROR && game->failure!=FD2_FIELD_FAILURE_NONE && uploads==0 && !clock_at);
            for(unsigned i=0;i<768;i++)CHECK(vga->dac[i]==0xa5);
        }
        CHECK(!memcmp(&game->units,&before->units,sizeof(game->units)) && game->battle_result==before->battle_result &&
              !memcmp(game->cell_action_completed,before->cell_action_completed,32) && !rng_calls);
        CHECK(vga->input.queued_count==1 && vga->input.queued_keys[0].key==FD2_INPUT_KEY_Z &&
              vga->field_water_last_tick==123 && vga->field_water_phase==2 && vga->actor_jitter_tick==77);
    }
    free(game);free(before);free(vga);return 0;
}

static int subtract_and_short_ms(void) {
    fd2_vga *vga=malloc(sizeof(*vga)),*before=malloc(sizeof(*before));CHECK(vga && before);
    for(unsigned darkness=0;darkness<=64;darkness++) {
        palette(vga,17);*before=*vga;
        CHECK(fd2_vga_palette_subtract(vga,0,255,(int32_t)darkness)==0);
        for(unsigned i=0;i<768;i++) {
            unsigned value=vga->palette[i]>darkness?vga->palette[i]-darkness:0;
            CHECK(vga->dac[i]==(uint8_t)((value<<2)|(value>>4)));
        }
        memcpy(before->dac,vga->dac,768);CHECK(!memcmp(before,vga,sizeof(*vga)));
        /* 旧void接口沿同一真实原语；全基色域必须能区分比例缩放。 */
        palette(vga,17);*before=*vga;fd2_vga_set_brightness(vga,(int)darkness);
        for(unsigned i=0;i<768;i++) {
            unsigned value=vga->palette[i]>darkness?vga->palette[i]-darkness:0;
            CHECK(vga->dac[i]==(uint8_t)((value<<2)|(value>>4)));
        }
        memcpy(before->dac,vga->dac,768);CHECK(!memcmp(before,vga,sizeof(*vga)));
    }
    const int clamp_values[]={-1,65,255};
    for(unsigned n=0;n<sizeof(clamp_values)/sizeof(clamp_values[0]);n++) {
        palette(vga,17);*before=*vga;fd2_vga_set_brightness(vga,clamp_values[n]);
        for(unsigned i=0;i<768;i++)CHECK(vga->dac[i]==(clamp_values[n]<0 ?
            (uint8_t)((vga->palette[i]<<2)|(vga->palette[i]>>4)):0));
        memcpy(before->dac,vga->dac,768);CHECK(!memcmp(before,vga,sizeof(*vga)));
    }
    palette(vga,0);*before=*vga;
    CHECK(fd2_vga_palette_subtract(vga,0,255,-1)==-1 && !memcmp(before,vga,sizeof(*vga)));
    CHECK(fd2_vga_palette_subtract(vga,0,255,65)==-1 && !memcmp(before,vga,sizeof(*vga)));
    CHECK(fd2_vga_palette_subtract(vga,-1,255,0)==-1 && !memcmp(before,vga,sizeof(*vga)));
    CHECK(fd2_vga_palette_subtract(vga,0,256,0)==-1 && !memcmp(before,vga,sizeof(*vga)));
    CHECK(fd2_vga_palette_subtract(vga,9,5,64)==0 && !memcmp(before,vga,sizeof(*vga)));
    vga->palette[767]=64;*before=*vga;
    CHECK(fd2_vga_palette_subtract(vga,0,255,0)==-1 && !memcmp(before,vga,sizeof(*vga)));
    const unsigned waits[]={2,8,20};
    for(unsigned n=0;n<3;n++) {
        palette(vga,0);*before=*vga;
        ms_mode=1;ms_now=ms_request_count=frames=quit_frame=0;
        CHECK(loot_fixed_wait(vga,waits[n],0)==0 && ms_now==waits[n]);
        CHECK(ms_request_count==(waits[n]>16?2u:1u) && ms_requests[0]==(waits[n]>16?16u:waits[n]));
        if(waits[n]>16)CHECK(ms_requests[1]==waits[n]-16);
        CHECK(!memcmp(before,vga,sizeof(*vga)));
    }
    palette(vga,0);*before=*vga;ms_now=ms_request_count=frames=0;
    CHECK(loot_fixed_wait(vga,2,1)==0 && !ms_request_count && !ms_now && !memcmp(before,vga,sizeof(*vga)));
    quit_frame=1;
    CHECK(loot_fixed_wait(vga,8,0)==FD2_SCENE_RESULT_HOST_QUIT && ms_request_count==1 && ms_requests[0]==8);
    CHECK(vga->input.queued_count==1 && vga->input.queued_keys[0].key==FD2_INPUT_KEY_Z &&
        vga->field_water_last_tick==123 && vga->field_water_phase==2 && vga->actor_jitter_tick==77);
    ms_mode=quit_frame=0;free(vga);free(before);return 0;
}

static int play_subtract_consumer(void) {
    const fd2_static_flow_node nodes[]={
        {.id="subtract",.op="palette_subtract",.args={0,255,1},.edges={1,-1,-1,-1,-1,-1,-1,4}},
        {.id="wait",.op="delay_ms",.args={2},.edges={2,-1,-1,-1,-1,-1,-1,4}},
        {.id="black",.op="palette_subtract",.args={0,255,63},.edges={3,-1,-1,-1,-1,-1,-1,4}},
        {.id="done",.op="end",.edges={-1,-1,-1,-1,-1,-1,-1,-1}},
        {.id="error",.op="fail",.edges={-1,-1,-1,-1,-1,-1,-1,-1}}};
    const fd2_static_flow flow={"test.s2.subtract",nodes,5,0};
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));
    fd2_vga *vga=malloc(sizeof(*vga));CHECK(game && before && vga);fd2_archive other={0};
    for(unsigned mode=0;mode<3;mode++) {
        memset(game,0,sizeof(*game));game->ready=1;game->units.count=1;
        game->event_resume=(fd2_field_event_resume){.flow=&flow,.active=1};*before=*game;
        palette(vga,17);ms_mode=1;ms_now=ms_request_count=frames=uploads=0;quit_frame=mode==1?1u:0u;
        if(mode==2)vga->palette[767]=64;
        int status=play_common_events(game,vga,&other,NULL,0);
        if(mode==0) {
            CHECK(!status && !game->event_resume.active && uploads==2 && ms_now==2 && ms_request_count==1);
            for(unsigned i=0;i<768;i++)CHECK(vga->dac[i]==0);
        } else if(mode==1) {
            CHECK(status==FD2_SCENE_RESULT_HOST_QUIT && game->runtime_host_quit && game->event_resume.pc==1 && uploads==1);
            for(unsigned i=0;i<768;i++) {unsigned value=vga->palette[i]?vga->palette[i]-1u:0;CHECK(vga->dac[i]==(uint8_t)((value<<2)|(value>>4)));}
        } else {
            CHECK(status==FD2_SCENE_RESULT_ERROR && game->failure!=FD2_FIELD_FAILURE_NONE && !uploads && !ms_request_count);
            for(unsigned i=0;i<768;i++)CHECK(vga->dac[i]==0xa5);
        }
        CHECK(!memcmp(&game->units,&before->units,sizeof(game->units)) &&
            !memcmp(game->cell_action_completed,before->cell_action_completed,32) &&
            vga->input.queued_count==1 && vga->input.queued_keys[0].key==FD2_INPUT_KEY_Z &&
            vga->field_water_last_tick==123 && vga->field_water_phase==2 && vga->actor_jitter_tick==77);
    }
    ms_mode=quit_frame=0;free(game);free(before);free(vga);return 0;
}

int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--s2-visual-vectors"))return vectors();
    if(argc!=1 || safety()!=0 || play_consumer()!=0 || subtract_and_short_ms()!=0 || play_subtract_consumer()!=0)return 1;
    puts("S2 visual leaf tests passed");return 0;
}

/* 正式视觉接口使用NS；本测试仍沿原毫秒调度器。 */
uint64_t test_ticks_ns(void) { return test_ticks()*UINT64_C(1000000); }
