/* S2 stage entry/victory源机制：205da/233c6完整写序与原Game/Scene owner。
 * 原机器码效果另由转换工具逐向量对照；本测试无原生窗口，不替代WGC。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "scene.h"
#include "field_page.h"
#include "static_data.h"

#define CHECK(x) do { if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return -1;} }while(0)

typedef struct {fd2_vga *vga;int stop_story,host;unsigned stories,rng;size_t count;int fragment;} trace;
static uint32_t no_rng(void *data){((trace *)data)->rng++;return 1;}
static int prefix(const fd2_field_game *g,const fd2_static_flow_node *n,
        const fd2_field_event_resource_view *view,void *data) {
    trace *t=data;(void)view;
    if(!strcmp(n->op,"story")) {
        t->stories++;t->count=g->units.count;t->fragment=n->args[0];
        if(n->args[0]==t->stop_story) {
            if(t->host){t->vga->input.quit_requested=1;return 0;}
            return -1;
        }
    }
    return 0;
}
static int manual(fd2_save_manual_state *state,unsigned stage) {
    static const uint8_t ids[]={0,9,4,30,1,2};
    memset(state,0,sizeof(*state));state->stage_id=(uint8_t)stage;
    state->unit_count=stage==1?5:6;state->gold_or_flags=1234;
    for(size_t i=0;i<state->unit_count;i++) {
        fd2_field_unit u={0};CHECK(fd2_field_unit_persistent_create(&u,ids[i])==0);
        memcpy(state->unit_area+i*80,&u,80);
    }
    return 0;
}
static fd2_static_flow_node node(const char *op) {fd2_static_flow_node n={0};n.op=op;return n;}
static int prepare(fd2_field_game *g,const fd2_archive *other,unsigned stage) {
    fd2_save_manual_state state;CHECK(manual(&state,stage)==0);char id[64];
    snprintf(id,sizeof(id),"stage.source.entry.%03u",stage);
    return fd2_field_game_prepare_stage_source_flow(g,other,&state,id);
}
static int test_source_resource_and_setup_boundary(const fd2_archive *other) {
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);
    CHECK(fd2_field_game_open_resources(g,other,3)==0 && g->ready && !g->units.count);
    fd2_field_game_close(g);
    CHECK(prepare(g,other,3)==0 && g->ready && g->units.count==0 && g->roster_count==6);
    CHECK(g->map.cells && g->sprites.frame_count && g->metadata.unknown_01);
    fd2_field_game_close(g);
    fd2_save_manual_state state;CHECK(manual(&state,3)==0);
    CHECK(fd2_field_game_prepare_stage_source_flow(g,other,&state,"stage.source.entry.002")==-1 && !g->ready);
    /* S5已开放entry29；resource30只属内部credits，没有正式战场入口。 */
    CHECK(manual(&state,30)==0);
    CHECK(fd2_field_game_prepare_stage_source_flow(g,other,&state,"stage.source.entry.030")==-1 && !g->ready);
    CHECK(prepare(g,other,1)==0 && g->units.count==0);
    unsigned port;fd2_static_flow_node reset=node("entry_result_reset");
    g->battle_result=FD2_FIELD_BATTLE_DEFEAT;CHECK(fd2_field_game_stage_source_node(g,&reset,&port)==1);
    CHECK(g->battle_result==FD2_FIELD_BATTLE_ONGOING && g->units.count==0);
    fd2_static_flow_node setup=node("stage_setup_source");
    fd2_field_placements placements=g->placements;g->placements.records=NULL;
    CHECK(fd2_field_game_stage_source_node(g,&setup,&port)==-1 && g->ready && !g->units.count);
    CHECK(g->battle_result==FD2_FIELD_BATTLE_ONGOING && g->roster_count==5);
    g->placements=placements;
    CHECK(fd2_field_game_stage_source_node(g,&setup,&port)==1 && g->units.count==5);
    fd2_field_game_close(g);free(g);return 0;
}
static int test_actual_scene_prefix(const fd2_archive *other,fd2_vga *vga) {
    for(int host=0;host<2;host++) {
        fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);CHECK(prepare(g,other,1)==0);
        trace t={vga,0,host,0,0,0};g->event_preflight=prefix;g->event_preflight_userdata=&t;
        g->attack_rng=no_rng;g->attack_rng_userdata=&t;g->battle_result=FD2_FIELD_BATTLE_DEFEAT;
        const void *map=g->map.cells;const uint8_t *archive=g->field_archive.data;
        vga->input.quit_requested=1;
        CHECK(fd2_scene_play_stage_source_flow(vga,other,g,NULL,1,"stage.source.entry.001")==FD2_SCENE_RESULT_HOST_QUIT);
        CHECK(!g->units.count && g->battle_result==FD2_FIELD_BATTLE_DEFEAT && !t.rng);
        vga->input.quit_requested=0;
        int result=fd2_scene_play_stage_source_flow(vga,other,g,NULL,1,"stage.source.entry.001");
        CHECK(result==(host?FD2_SCENE_RESULT_HOST_QUIT:FD2_SCENE_RESULT_ERROR));
        CHECK(t.stories==1 && t.count==5 && g->units.count==5 && g->turn_number==1);
        CHECK(g->battle_result==FD2_FIELD_BATTLE_ONGOING && !t.rng);
        CHECK(g->map.cells==map && g->field_archive.data==archive && g->event_preflight_userdata==&t);
        vga->input.quit_requested=0;fd2_field_game_close(g);free(g);
    }
    return 0;
}
static int test_capacity_retains_prior_prefix(const fd2_archive *other) {
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);CHECK(prepare(g,other,1)==0);
    fd2_static_flow_node setup=node("stage_setup_source");unsigned port;
    CHECK(fd2_field_game_stage_source_node(g,&setup,&port)==1);
    for(size_t i=g->units.count;i<FD2_FIELD_MAX_UNITS;i++)g->units.items[i]=g->units.items[0];
    g->units.count=FD2_FIELD_MAX_UNITS;
    fd2_static_flow_node write=node("unit_field_write");write.args[2]=FD2_UNIT_STATUS_26;write.args[4]=100;
    CHECK(fd2_field_game_stage_source_node(g,&write,&port)==1);
    fd2_field_units before=g->units;fd2_static_flow_node append=node("append_group");append.args[0]=1;
    CHECK(fd2_field_game_stage_source_node(g,&append,&port)==-1);
    CHECK(!memcmp(&before,&g->units,sizeof(before)) && g->units.items[0].detail_status[1]==100);
    fd2_field_game_close(g);free(g);return 0;
}
static int test_explicit_source_id_and_shipping_story_domain(const fd2_archive *other,fd2_vga *vga) {
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);CHECK(prepare(g,other,3)==0);
    CHECK(fd2_field_stage_is_supported(3) && fd2_field_stage_battle_is_supported(3));
    CHECK(fd2_scene_field_story_validate(g,0)==0); /* S3公开该静态对白域；仍未执行初始化。 */
    CHECK(fd2_scene_play_stage_source_flow(vga,other,g,NULL,1,"stage.entry.003")==FD2_SCENE_RESULT_ERROR);
    CHECK(!g->units.count);
    trace t={vga,-1,0,0,0,0};g->event_preflight=prefix;g->event_preflight_userdata=&t;
    g->attack_rng=no_rng;g->attack_rng_userdata=&t;
    CHECK(fd2_scene_play_stage_source_flow(vga,other,g,NULL,1,"stage.source.entry.003")==FD2_SCENE_RESULT_OK);
    CHECK(t.stories>0 && !t.rng && g->units.count>=g->metadata.unknown_01);
    CHECK(fd2_field_stage_is_supported(3) && fd2_scene_field_story_validate(g,0)==0);
    fd2_field_game_close(g);free(g);return 0;
}
static int test_filtered_position_original_bounds(void) {
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);g->ready=1;g->units.count=20;
    for(size_t i=0;i<96;i++){g->units.items[i].unit_id=31;g->units.items[i].x=1;g->units.items[i].y=2;}
    g->units.items[18].unit_id=30;g->units.items[16].flags=1;g->units.items[17].hp=0;
    fd2_field_units before=g->units;
    fd2_static_flow_node filter=node("unit_id_position");unsigned port;
    filter.args[0]=16;filter.args[1]=31;filter.args[2]=16;filter.args[3]=6;
    CHECK(fd2_field_game_stage_source_node(g,&filter,&port)==1);
    for(size_t i=0;i<96;i++) {
        fd2_field_unit expected=before.items[i];
        if(i>=16 && i<20 && i!=18){expected.x=16;expected.y=6;}
        CHECK(!memcmp(&expected,&g->units.items[i],80));
    }
    before=g->units;filter.args[0]=97;
    CHECK(fd2_field_game_stage_source_node(g,&filter,&port)==-1 && !memcmp(&before,&g->units,sizeof(before)));
    free(g);return 0;
}
static int test_source_rejects_owned_continuations_unchanged(const fd2_archive *other,fd2_vga *vga) {
    fd2_field_game *g=calloc(1,sizeof(*g)),*before=malloc(sizeof(*before));
    fd2_vga *vga_before=malloc(sizeof(*vga_before));CHECK(g && before && vga_before);
    CHECK(prepare(g,other,1)==0 && fd2_field_game_stage_source_is_idle(g));
    for(unsigned which=0;which<15;which++) {
        if(which==0)g->event_resume.active=1;
        if(which==1)g->loot.child.active=1;
        if(which==2)g->loot.source=1;
        if(which==3)g->progress.notice.kind=FD2_FIELD_PROGRESS_GAIN;
        if(which==4)g->progress.resume=FD2_FIELD_PROGRESS_RESUME_PHASE;
        if(which==5)g->player_post_waiting_phase=1;
        if(which==6)g->turn_post_caller=FD2_FIELD_TURN_POST_MARCH;
        if(which==7)g->loot.resume=FD2_FIELD_PROGRESS_RESUME_PLAYER;
        if(which==8)g->loot.notice.kind=FD2_FIELD_LOOT_MESSAGE;
        if(which==9)g->event_resume.child_wait=1;
        if(which==10)g->event_resume.completion=FD2_EVENT_COMPLETE_TURN;
        if(which==11)g->runtime_host_quit=1;
        if(which==12){g->event_log_count=1;g->event_log[0].presentation_deferred=1;}
        if(which==13)g->interaction=(fd2_field_interaction)(FD2_FIELD_INTERACTION_BROWSE+1);
        if(which==14)g->event_log_count=sizeof(g->event_log)/sizeof(g->event_log[0])+1;
        *before=*g;*vga_before=*vga;
        CHECK(!fd2_field_game_stage_source_is_idle(g));
        CHECK(fd2_scene_play_stage_source_flow(vga,other,g,NULL,1,"stage.source.entry.001")==FD2_SCENE_RESULT_ERROR);
        CHECK(!memcmp(g,before,sizeof(*g)) && !memcmp(vga,vga_before,sizeof(*vga)));
        memset(&g->event_resume,0,sizeof(g->event_resume));memset(&g->loot,0,sizeof(g->loot));
        memset(&g->progress,0,sizeof(g->progress));g->player_post_waiting_phase=0;
        g->turn_post_caller=FD2_FIELD_TURN_POST_NONE;g->runtime_host_quit=0;
        g->event_log_count=0;memset(g->event_log,0,sizeof(g->event_log));g->interaction=FD2_FIELD_INTERACTION_BROWSE;
        CHECK(fd2_field_game_stage_source_is_idle(g));
    }
    fd2_field_game_close(g);free(g);free(before);free(vga_before);return 0;
}
static int test_dynamic_text_source_prefix(const fd2_archive *other,fd2_vga *vga) {
    for(unsigned value=0;value<3;value++) {
        unsigned byte=value==2?255:value;
        fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);CHECK(prepare(g,other,25)==0);
        fd2_static_flow_node setup=node("stage_setup_source");unsigned port;
        CHECK(fd2_field_game_stage_source_node(g,&setup,&port)==1 && g->units.count>=16);
        g->cell_action_completed[12]=(uint8_t)byte;g->message_new_fragment=777;
        trace t={vga,(int)byte+5,0,0,0,0,0};g->event_preflight=prefix;g->event_preflight_userdata=&t;
        g->attack_rng=no_rng;g->attack_rng_userdata=&t;
        CHECK(fd2_scene_play_stage_source_flow(vga,other,g,NULL,1,"stage.source.victory.025")==FD2_SCENE_RESULT_ERROR);
        CHECK(t.stories==1 && t.fragment==(int)byte+5 && !t.rng && g->message_new_fragment==777);
        CHECK(g->stage==25 && g->cell_action_completed[12]==byte);
        fd2_field_game_close(g);free(g);
    }
    return 0;
}
static int test_actual_layout_prefix(const fd2_archive *other,fd2_vga *vga) {
    for(int hidden=0;hidden<2;hidden++) {
        fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);CHECK(prepare(g,other,2)==0);
        fd2_static_flow_node setup=node("stage_setup_source");unsigned port;
        CHECK(fd2_field_game_stage_source_node(g,&setup,&port)==1 && g->units.count==g->metadata.unknown_01);
        fd2_static_flow_node group0=node("append_group");
        CHECK(fd2_field_game_stage_source_node(g,&group0,&port)==1 && g->units.count==6);
        /* 原stage2 group0无匹配；9条group1在entry2对白后加入。
         * 本胜利机制夹具构造该真实15人资源域，不冒称已跑entry2全剧情。 */
        fd2_static_flow_node group1=node("append_group");group1.args[0]=1;
        CHECK(fd2_field_game_stage_source_node(g,&group1,&port)==1 && g->units.count==15);
        for(size_t i=0;i<g->units.count;i++)g->units.items[i].flags|=0x80;
        fd2_field_unit_set_hidden(&g->units.items[6],hidden);
        fd2_field_units before=g->units;size_t roster=g->roster_count;
        trace t={vga,hidden?6:7,0,0,0,0};g->event_preflight=prefix;g->event_preflight_userdata=&t;
        g->attack_rng=no_rng;g->attack_rng_userdata=&t;
        CHECK(fd2_scene_play_stage_source_flow(vga,other,g,NULL,1,"stage.source.victory.002")==FD2_SCENE_RESULT_ERROR);
        CHECK(t.stories==1 && !t.rng && g->stage==2 && g->roster_count==roster);
        if(hidden)CHECK(!memcmp(&before,&g->units,sizeof(before)));
        else {
            /* 真实EXE520ba/520c1/520c8原字节由原码/C probe独立逐字节对照；
             * 此处核真实Scene已消费布局、所有acted清零，首个story错误不执行招募/next。 */
            for(size_t i=0;i<g->units.count;i++)CHECK(!(g->units.items[i].flags&0x80));
            CHECK(memcmp(&before,&g->units,sizeof(before)) && g->cursor_cell_x==2 && g->cursor_cell_y==0);
            CHECK(g->camera_cell_x==2 && g->camera_cell_y==0);
        }
        fd2_field_game_close(g);free(g);
    }
    return 0;
}
static int hex_read(const char *s,uint8_t *out,size_t size) {
    if(strlen(s)!=size*2)return -1;
    for(size_t i=0;i<size;i++){unsigned x;if(sscanf(s+i*2,"%2x",&x)!=1)return -1;out[i]=(uint8_t)x;}
    return 0;
}
/* 仅测试程序的原码差分入口：直接消费正式cold节点，不实现期望公式。 */
static int layout_vectors(void) {
    char id[96],raw[96*80*2+1];unsigned start,count;
    fd2_field_game *g=calloc(1,sizeof(*g));if(!g)return 1;
    while(scanf("%95s %u %u %15360s",id,&start,&count,raw)==4) {
        if(count>96 || hex_read(raw,(uint8_t *)g->units.items,96*80)!=0)return 2;
        g->ready=1;g->units.count=count;
        const fd2_static_flow *flow=fd2_static_flow_lookup("stage",id);if(!flow)return 3;
        const fd2_static_flow_node *n=fd2_static_flow_at(flow,(int)start);unsigned port;
        if(!n || strcmp(n->op,"call_fade_out"))return 4;
        for(unsigned budget=0;budget<65536;budget++) {
            int handled=fd2_field_game_stage_source_node(g,n,&port);
            if(handled<0)return 5;
            if(!handled && strcmp(n->op,"call_fade_out") && strcmp(n->op,"call_fade_in") && strcmp(n->op,"refresh") && strcmp(n->op,"delay"))return 6;
            if(!strcmp(n->op,"delay") && n->args[0]==200)break;
            n=fd2_static_flow_at(flow,fd2_static_flow_edge(flow,n,handled?port:FD2_FLOW_NEXT));if(!n)return 7;
        }
        const uint8_t *bytes=(const uint8_t *)g->units.items;
        for(size_t i=0;i<96*80;i++)printf("%02x",bytes[i]);
        printf(" %u %d %d %d %d %d %d\n",g->ordinary_display_mode,g->cursor_cell_x,g->cursor_cell_y,
            g->camera_cell_x,g->camera_cell_y,g->camera_pixel_offset_x,g->camera_pixel_offset_y);
    }
    free(g);return 0;
}
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--layout-vectors"))return layout_vectors();
    fd2_archive other={0};if(fd2_archive_open(&other,"original_game/FDOTHER.DAT")!=0)return 1;
    fd2_vga *vga=calloc(1,sizeof(*vga));if(!vga)return 1;
    vga->field_page=fd2_field_page_create();if(!vga->field_page)return 1;
    const uint8_t *palette;size_t size;
    if(fd2_archive_get(&other,0,&palette,&size)!=0 || size<768)return 1;
    fd2_vga_set_palette(vga,palette);fd2_input_init(&vga->input);
    int failed=test_source_resource_and_setup_boundary(&other) || test_actual_scene_prefix(&other,vga) ||
        test_capacity_retains_prior_prefix(&other) || test_actual_layout_prefix(&other,vga) ||
        test_explicit_source_id_and_shipping_story_domain(&other,vga) || test_filtered_position_original_bounds() ||
        test_dynamic_text_source_prefix(&other,vga) || test_source_rejects_owned_continuations_unchanged(&other,vga);
    fd2_field_page_destroy(vga->field_page);free(vga);fd2_archive_close(&other);
    if(!failed)puts("S2 source transition resource/prefix/Scene tests passed (no native GUI)");
    return failed?1:0;
}
