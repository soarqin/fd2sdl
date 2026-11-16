/* F4：仅测试TU的正式cold montage观察。原31529/2e6a4/2ebe1的保留
 * PC事件和整页是外部expected；本文件只记录真实C调用，不解释冷图。
 * inactive时完全旁路，不产生第二生产owner或公共API。 */
#ifndef FD2_F4_MONTAGE_COLD_PROBE_H
#define FD2_F4_MONTAGE_COLD_PROBE_H
#include "scene_cinematic.h"
#include "bios_clock.h"
#include "figani.h"
static unsigned f4_cold_enabled,f4_cold_rows,f4_cold_rng_calls,f4_cold_wait_nodes,f4_cold_wait_total;
static uint32_t f4_cold_rng_value;
static uint64_t f4_cold_clock_tick;
static unsigned f4_cold_clock_reads;
static const char *f4_cold_flow,*f4_cold_node;
static fd2_field_game *f4_cold_game;
static fd2_vga *f4_cold_vga;
static int f4_cold_output_failed;
/* 最多half0两遍、half1一遍，各22个已证part；保每次纯getter调用账。 */
typedef struct {
    unsigned event_before,half,part,entry,cue;
    unsigned rng_before,rng_after,clock_before,clock_after;
    uint16_t hp_before[2],hp_after[2];
    const char *flow,*node;
} f4_cold_cue_call;
static f4_cold_cue_call f4_cold_cues[66];
static unsigned f4_cold_cue_count;
static void f4_cold_before_node(const fd2_static_flow *flow,const fd2_static_flow_node *node) {
    if(!f4_cold_enabled || !flow || !node)return;
    f4_cold_flow=flow->id;f4_cold_node=node->id;
    if(!strcmp(node->op,"delay_ticks")) {
        f4_cold_wait_nodes++;
        if(node->args[0]<0 || node->args[0]>65535)f4_cold_output_failed=1;
        else f4_cold_wait_total+=(unsigned)node->args[0];
    }
}
static uint32_t f4_cold_random(void *userdata) {
    (void)userdata;f4_cold_rng_calls++;return f4_cold_rng_value;
}
static void f4_cold_emit(const char *command,int status,int yes,int frame) {
    if(!f4_cold_enabled)return;
    if(!f4_cold_flow || !f4_cold_node || !f4_cold_game || !f4_cold_vga){f4_cold_output_failed=1;return;}
    printf("{\"command\":\"%s\",\"flow\":\"%s\",\"node\":\"%s\",\"status\":%d,\"hp\":[%u,%u],\"rng\":%u,\"yes\":%d,\"pixels_hex\":\"",
        command,f4_cold_flow,f4_cold_node,status,(unsigned)f4_cold_game->units.items[0].hp,
        (unsigned)f4_cold_game->units.items[1].hp,f4_cold_rng_calls,yes);
    if(frame && !status)for(size_t i=0;i<64000;i++)printf("%02x",f4_cold_vga->framebuffer[i]);
    puts("\"}");f4_cold_rows++;
    if(status || ferror(stdout))f4_cold_output_failed=1;
}
static int f4_cold_open(fd2_scene_cinematic *scope,int first,int second,int scene) {
    int result=fd2_scene_cinematic_montage_open(scope,first,second,scene);
    if(f4_cold_enabled){char command[80];snprintf(command,sizeof(command),"o %d %d %d",first,second,scene);f4_cold_emit(command,result,-1,0);}return result;
}
static int f4_cold_intro(fd2_scene_cinematic *scope,unsigned phase) {
    int result=fd2_scene_cinematic_montage_intro(scope,phase);
    if(f4_cold_enabled){char command[40];snprintf(command,sizeof(command),"i %u",phase);f4_cold_emit(command,result,-1,1);}return result;
}
static int f4_cold_finish(fd2_scene_cinematic *scope) {
    int result=fd2_scene_cinematic_montage_intro_finish(scope);f4_cold_emit("f",result,-1,0);return result;
}
static int f4_cold_half(fd2_scene_cinematic *scope,unsigned half) {
    int result=fd2_scene_cinematic_montage_half(scope,half);
    if(f4_cold_enabled){char command[40];snprintf(command,sizeof(command),"h %u",half);f4_cold_emit(command,result,-1,0);}return result;
}
static int f4_cold_rng(fd2_scene_cinematic *scope,unsigned threshold,int *yes) {
    int result=fd2_scene_cinematic_montage_rng(scope,threshold,yes);
    if(f4_cold_enabled){char command[40];snprintf(command,sizeof(command),"r %u",f4_cold_rng_value);f4_cold_emit(command,result,!result&&yes?*yes:-1,0);}return result;
}
static int f4_cold_hit(fd2_scene_cinematic *scope) {
    int result=fd2_scene_cinematic_montage_hit(scope);f4_cold_emit("t",result,-1,0);return result;
}
static int f4_cold_cue(fd2_scene_cinematic *scope,size_t part) {
    if(!f4_cold_enabled)return fd2_scene_cinematic_montage_cue(scope,part);
    if(f4_cold_cue_count>=66u){f4_cold_output_failed=1;return -1;}
    f4_cold_cue_call *call=&f4_cold_cues[f4_cold_cue_count++];memset(call,0,sizeof(*call));
    call->event_before=f4_cold_rows;call->part=(unsigned)part;call->flow=f4_cold_flow;call->node=f4_cold_node;
    call->rng_before=f4_cold_rng_calls;call->clock_before=f4_cold_clock_reads;
    for(unsigned i=0;i<2;i++)call->hp_before[i]=f4_cold_game->units.items[i].hp;
    int result=fd2_scene_cinematic_montage_cue(scope,part);
    /* 原2efe4/2eff0以part+5 cue门控25a96，不是part+4 effect_marker。
     * 从正式Game已加载的同一FIGANI buffer取字段；不重开资源、不猜cue。 */
    fd2_figani_clip clip;fd2_figani_part metadata;unsigned half;
    int observed=fd2_scene_cinematic_montage_current_half(scope,&half);
    if(!observed && half<2u) {
        call->half=half;call->entry=3u*f4_cold_game->units.items[half].unit_id+1u;
        observed=fd2_figani_open_entry(&clip,&f4_cold_game->figani_archive,call->entry);
        if(!observed)observed=fd2_figani_get_part(&clip,part,&metadata);
        if(!observed)call->cue=metadata.cue;
    } else observed=-1;
    call->rng_after=f4_cold_rng_calls;call->clock_after=f4_cold_clock_reads;
    for(unsigned i=0;i<2;i++)call->hp_after[i]=f4_cold_game->units.items[i].hp;
    if(observed || result || call->rng_before!=call->rng_after || call->clock_before!=call->clock_after ||
        memcmp(call->hp_before,call->hp_after,sizeof(call->hp_before))) {
        f4_cold_output_failed=1;
        char command[40];snprintf(command,sizeof(command),"c %u",(unsigned)part);f4_cold_emit(command,-1,-1,0);return -1;
    }
    /* cue0的真实C helper只是纯metadata getter，不对应原25a96 PC事件；
     * 不伪造c行，每次调用仍完整写入summary.cue_audit，交独立raw+5核。 */
    if(call->cue){char command[40];snprintf(command,sizeof(command),"c %u",(unsigned)part);f4_cold_emit(command,0,-1,0);}
    return result;
}
static int f4_cold_frame(fd2_scene_cinematic *scope,size_t part) {
    int result=fd2_scene_cinematic_montage_frame(scope,part);
    if(f4_cold_enabled){char command[40];snprintf(command,sizeof(command),"d %u",(unsigned)part);f4_cold_emit(command,result,-1,1);}return result;
}
static int f4_cold_close(fd2_scene_cinematic *scope) {
    int result=fd2_scene_cinematic_montage_close(scope);f4_cold_emit("q",result,-1,0);return result;
}
static int f4_cold_game_node(fd2_field_game *game,const fd2_static_flow_node *node,unsigned *port) {
    int result=fd2_field_game_stage_source_node(game,node,port);
    if(f4_cold_enabled && node && !strcmp(node->op,"unit_field_write") && result==1) {
        if(node->args[0]!=node->args[1] || node->args[0]<0 || node->args[0]>1 ||
            node->args[2]!=FD2_UNIT_HP || node->args[3]!=FD2_UNIT_SET || node->args[4]!=63680) {
            f4_cold_output_failed=1;return -1;
        }
        char command[40];snprintf(command,sizeof(command),"p %d %d",node->args[0],node->args[4]);f4_cold_emit(command,0,-1,0);
    }
    return result;
}
#endif
