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
static void reset_spies(void) {
    rng_calls=story_count=story_fragment=scope_calls=next_replies=error_replies=wrong_ack_rejected=0;
    host_after_scope=0;
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
    if(!result && host_after_scope){vga->input.quit_requested=1;return FD2_SCENE_RESULT_HOST_QUIT;}
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
    free(f->game->map.cells);f->game->map.cells=f->cells;fixture_close(f);
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
int main(void) {
    CHECK(run(0,0)==0 && run(4,0)==0 && run(0,1)==0 && run(4,2)==0);
    CHECK(callback82_scope_checks()==0);
    puts("callback82 Play: actual shared helper, both source branches, ACK and HOST/error xy prefixes PASS");return 0;
}
