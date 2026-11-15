/* 完整362e8冷图→真实Game peek/reply→正式Play→唯一Scene传送helper。
 * 仅story ACK与最后上传后的HOST通知为明确I/O替身；不自行写teleport坐标。 */
#include "field_play.h"
#include "field_game.h"
#include "scene.h"
#include "scene_teleport.h"
#include "static_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(e) do {if(!(e)){fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#e);return 1;}}while(0)
static unsigned rng_calls,story_count,story_fragment,scope_calls,next_replies,error_replies,wrong_ack_rejected;
static int host_after_scope;
static int emit_coalesced;
static void reset_spies(void) {
    rng_calls=story_count=story_fragment=scope_calls=next_replies=error_replies=wrong_ack_rejected=0;
    host_after_scope=emit_coalesced=0;
}
static int callback82_step(fd2_vga *vga,const fd2_archive *fdother,fd2_field_game *game,
        const fd2_static_flow_node *node,fd2_field_audio *audio,int fast) {
    if(!strcmp(node->op,"story")){story_count++;story_fragment=(unsigned)node->args[0];return 0;}
    return fd2_scene_play_common_step(vga,fdother,game,node,audio,fast);
}
static int callback82_scope(fd2_vga *vga,const fd2_archive *fdother,fd2_field_game *game,
        fd2_field_audio *audio,int fast,fd2_scene_teleport_scope **scope) {
    scope_calls++;
    int result=fd2_scene_play_teleport_scope(vga,fdother,game,audio,fast,scope);
    /* 模拟最后真实上传后到达宿主通知；完整真实helper已执行，未手写xy。 */
    if(emit_coalesced) {
        unsigned actor=scope_calls==1?22:scope_calls==2?29:30;
        const uint8_t *raw=(const uint8_t *)&game->units.items[actor];
        printf("{\"prefix\":%u,\"actor\":%u,\"active_count\":%zu,\"record_hex\":\"",scope_calls,actor,game->units.count);
        for(unsigned i=0;i<sizeof(fd2_field_unit);i++)printf("%02x",raw[i]);puts("\"}");
    }
    if(!result && host_after_scope==(int)scope_calls){vga->input.quit_requested=1;return FD2_SCENE_RESULT_HOST_QUIT;}
    return result;
}
static int callback82_reply(fd2_field_game *game,unsigned port) {
    fd2_static_flow_node node;size_t actor;
    if(fd2_field_game_common_event_peek(game,&node,&actor)==1 && !strcmp(node.op,"call_teleport")) {
        if(port==FD2_FLOW_NEXT) {
            fd2_field_game *before=malloc(sizeof(*before));if(!before)abort();*before=*game;
            if(fd2_field_game_common_event_reply(game,FD2_FLOW_ACK)!=-1 || memcmp(game,before,sizeof(*before)))abort();
            free(before);wrong_ack_rejected++;next_replies++;
        } else if(port==FD2_FLOW_ERROR)error_replies++;
    }
    return fd2_field_game_common_event_reply(game,port);
}
#define fd2_scene_play_common_step callback82_step
#define fd2_scene_play_teleport_scope callback82_scope
#define fd2_field_game_common_event_reply callback82_reply
#include "../field_play.c"
#undef fd2_field_game_common_event_reply
#undef fd2_scene_play_teleport_scope
#undef fd2_scene_play_common_step
#include "s5_effect_fixture.h"

static int setup(fixture *f,unsigned state,int bad_overlay) {
    CHECK(fixture_open(f)==0);
    fd2_field_game *g=f->game;
    g->map.width=g->map.height=40;g->map.cells=calloc(1600,sizeof(uint32_t));CHECK(g->map.cells);
    g->units.count=35;
    for(unsigned i=0;i<35;i++) {
        g->units.items[i]=g->units.items[0];g->units.items[i].ai_behavior_raw=0xa7;
    }
    CHECK(fd2_field_units_sprite_cache_restore(&g->units)==0);
    g->cell_action_completed[16]=(uint8_t)state;
    if(bad_overlay) {
        /* 首次terrain只读合法base1；xy提交后才需overlay2，故真实mask在写后失败。 */
        g->map.cells[18*40+22]=1;g->terrain.attrs[1].flags=0x80;
        f->terrain[2*600]=0xff;
    }
    return 0;
}
static void close_game_fixture(fixture *f) {
    free(f->game->metadata.unit_templates);free(f->game->placements.records);
    free(f->game->map.cells);f->game->map.cells=f->cells;fixture_close(f);
}
/* 只替代对白I/O；teleport/pan/focus的正式资源预检不能绕过。 */
static int coalesced_preflight(const fd2_field_game *game,const fd2_static_flow_node *node,
        const fd2_field_event_resource_view *view,void *userdata) {
    return !strcmp(node->op,"story")?0:common_event_resources_preflight(game,node,view,userdata);
}
static int coalesced(unsigned host_prefix,int emit) {
    fixture *f=malloc(sizeof(*f));CHECK(f && setup(f,2,0)==0);
    fd2_field_game *g=f->game;g->units.count=27;
    memset(g->units.items+27,0xa5,(FD2_FIELD_MAX_UNITS-27)*sizeof(fd2_field_unit));
    CHECK(fd2_field_units_sprite_cache_restore(&g->units)==0);
    g->metadata.unit_templates=calloc(2,sizeof(fd2_field_unit_template));
    g->placements.records=calloc(2,sizeof(fd2_field_placement));
    CHECK(g->metadata.unit_templates && g->placements.records);
    g->metadata.unknown_02=2;g->metadata.unit_template_count=g->placements.count=2;
    for(unsigned i=0;i<2;i++) {
        uint8_t *t=g->metadata.unit_templates[i].bytes;t[4]=1;t[5]=0xff;t[6]=0x80;
        for(unsigned j=7;j<13;j++)t[j]=0xff;
        t[0x15]=2;t[0x16]=0xff;
        g->placements.records[i]=(fd2_field_placement){(uint16_t)(21+2*i),18,0};
    }
    g->event_preflight=coalesced_preflight;g->event_preflight_userdata=&f->fdother;
    fd2_static_flow_node boundary={0};boundary.op="teleport_begin";
    boundary.args[0]=95;boundary.args[1]=boundary.args[3]=21;boundary.args[2]=boundary.args[4]=18;
    fd2_field_event_resource_view view={&g->units,g->roster,g->roster_count,-1,-1,0};
    CHECK(common_event_resources_preflight(g,&boundary,&view,&f->fdother)==0);
    boundary.args[0]=96;CHECK(common_event_resources_preflight(g,&boundary,&view,&f->fdother)==-1);
    boundary.op="notice_actor";boundary.args[0]=29;
    CHECK(common_event_resources_preflight(g,&boundary,&view,&f->fdother)==-1);
    *f->before=*g;host_after_scope=(int)host_prefix;emit_coalesced=emit;
    CHECK(fd2_field_game_common_event_begin(g,"common.callback.082",0)==0);
    int status=play_common_events(g,f->vga,&f->fdother,NULL,1);
    CHECK(status==(host_prefix?FD2_SCENE_RESULT_HOST_QUIT:FD2_SCENE_RESULT_OK));
    CHECK(story_count==1 && story_fragment==4 && !rng_calls);
    CHECK(scope_calls==(host_prefix?host_prefix:3) && g->cell_action_completed[16]==2);
    CHECK(g->units.count==(host_prefix==1?27u:29u));
    CHECK(g->units.items[22].x==22 && g->units.items[22].y==18);
    CHECK(g->units.items[22].ai_behavior_raw==(host_prefix==1?0xa7:0xa0));
    for(unsigned actor=29;actor<FD2_FIELD_MAX_UNITS;actor++) {
        fd2_field_unit expected;memset(&expected,0xa5,sizeof(expected));
        if(actor==29 && host_prefix!=1) {expected.x=21;expected.y=18;}
        if(actor==30 && !host_prefix) {expected.x=23;expected.y=18;}
        CHECK(!memcmp(&expected,&g->units.items[actor],sizeof(expected)));
    }
    CHECK(host_prefix?(g->runtime_host_quit && g->event_resume.active && !error_replies):
        (!g->event_resume.active && next_replies==3 && wrong_ack_rejected==3 && g->ordinary_display_mode==1));
    close_game_fixture(f);free(f);return 0;
}
static int run(unsigned state,int mode) {
    fixture *f=malloc(sizeof(*f));CHECK(f && setup(f,state,mode==2)==0);
    fd2_field_game *g=f->game;
    const char *id="common.callback.082";
    CHECK(fd2_static_flow_lookup("common_event",id));
    CHECK(fd2_field_game_common_event_begin(g,id,0)==0);
    host_after_scope=mode==1;
    int status=play_common_events(g,f->vga,&f->fdother,NULL,1);
    CHECK(story_count==1 && story_fragment==state+2 && rng_calls==0);
    unsigned main_actor=24-state;
    CHECK(g->units.items[main_actor].x==22 && g->units.items[main_actor].y==18);
    if(mode==0) {
        CHECK(status==0 && g->failure==FD2_FIELD_FAILURE_NONE && !g->event_resume.active);
        CHECK(scope_calls==(state==4?1u:3u) && next_replies==scope_calls && wrong_ack_rejected==scope_calls && !error_replies);
        CHECK(g->ordinary_display_mode==1 && g->units.items[main_actor].ai_behavior_raw==(state==4?0xab:0xa0));
        if(state!=4) {
            CHECK(g->units.items[25+2*state].x==21 && g->units.items[25+2*state].y==18);
            CHECK(g->units.items[26+2*state].x==23 && g->units.items[26+2*state].y==18);
        }
        *f->before=*g;CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==-1 && !memcmp(g,f->before,sizeof(*g)));
    } else {
        CHECK(scope_calls==1 && !next_replies && !wrong_ack_rejected && g->ordinary_display_mode==0);
        CHECK(g->units.items[main_actor].ai_behavior_raw==0xa7); /* helper后面的AI写点未到。 */
        if(mode==1)CHECK(status==-2 && g->runtime_host_quit && !error_replies && g->event_resume.active);
        else CHECK(status==-1 && g->failure!=FD2_FIELD_FAILURE_NONE && error_replies==1);
        *f->before=*g;CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_NEXT)==-1 && !memcmp(g,f->before,sizeof(*g)));
    }
    CHECK(g->cell_action_completed[16]==state && g->units.count==35);
    close_game_fixture(f);free(f);return 0;
}
/* 六种display/HOST/失败前缀由同目标私有Scene TU走真实节点边界。 */
int callback82_scope_checks(void);
int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--f1-coalesced-callback82"))return coalesced(0,1);
    CHECK(argc==1);
    CHECK(run(0,0)==0 && run(4,0)==0 && run(0,1)==0 && run(4,2)==0);
    CHECK(coalesced(0,0)==0 && coalesced(1,0)==0 && coalesced(2,0)==0);
    CHECK(callback82_scope_checks()==0);
    puts("callback82 Play: actual shared helper, both source branches, ACK and HOST/error xy prefixes PASS");return 0;
}
