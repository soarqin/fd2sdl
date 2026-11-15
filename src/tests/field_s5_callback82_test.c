/* 362e8 STATE16→有限actor来源、原AI写点及begin/call的纯资源投影。
 * 本文件不声称已运行22253/Play；真实共享helper由单独原码/C门验收。 */
#include "field_game.h"
#include "field_common_event_flow.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(e) do {if(!(e)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e);return 1;}}while(0)
static int common_event_leaf(const char *op) {
    return !strcmp(op,"teleport_begin") || !strcmp(op,"call_teleport") || !strcmp(op,"story");
}
static unsigned begins,calls,stories,projected_state,inactive_actor;
static int common_event_preflight_leaf_view(const fd2_field_game *game,
        const fd2_static_flow_node *node,size_t actor,const fd2_field_event_resource_view *view) {
    (void)actor;
    if(!game || !view || !view->units || game->cell_action_completed[16]!=0)return -1;
    if(inactive_actor) {
        fd2_field_unit expected;memset(&expected,0xa5,sizeof(expected));
        if(!strcmp(node->op,"teleport_begin")) {
            begins++;if(node->args[0]!=(int)inactive_actor)return -1;
        } else if(!strcmp(node->op,"call_teleport"))calls++;
        else if(!strcmp(node->op,"story")) {
            stories++;expected.x=21;expected.y=18;
        }
        return view->units->count==29 && !memcmp(&expected,&view->units->items[inactive_actor],sizeof(expected))?0:-1;
    }
    if(!strcmp(node->op,"teleport_begin")) {
        begins++;
        if(node->args[0]!=(int)(24-projected_state) || view->units->items[24-projected_state].x!=3)return -1;
    } else if(!strcmp(node->op,"call_teleport"))calls++;
    else if(!strcmp(node->op,"story")) {
        stories++;
        if(view->units->items[24-projected_state].x!=22 || view->units->items[24-projected_state].y!=18 ||
                view->units->items[24-projected_state].ai_behavior_raw!=0)return -1;
    }
    return 0;
}
#include "field_common_event_projection.h"

static fd2_static_flow_node node(const char *op,int next) {
    fd2_static_flow_node n={0};n.op=op;
    for(unsigned i=0;i<FD2_FLOW_PORT_COUNT;i++)n.edges[i]=-1;
    n.edges[FD2_FLOW_NEXT]=(int16_t)next;return n;
}
static int test_selectors_and_AI_write(void) {
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));CHECK(game && before);
    game->ready=1;game->units.count=35;
    for(unsigned i=0;i<35;i++) {
        game->units.items[i].x=3;game->units.items[i].y=4;game->units.items[i].ai_behavior_raw=7;
    }
    uint32_t value;uint8_t state[32]={0};
    for(unsigned s=0;s<=4;s++) {
        state[16]=(uint8_t)s;
        CHECK(common_event_teleport_actor(-1,state,&value)==0 && value==24-s);
        CHECK(common_event_teleport_actor(-2,state,&value)==0 && value==25+2*s);
        CHECK(common_event_teleport_actor(-3,state,&value)==0 && value==26+2*s);
    }
    CHECK(common_event_teleport_actor(0,NULL,&value)==0 && value==0);
    CHECK(common_event_teleport_actor(-4,state,&value)==-1 && common_event_teleport_actor(-1,NULL,&value)==-1);
    game->cell_action_completed[16]=4;
    fd2_static_flow_node write=node("unit_ai_write_source",-1);
    write.args[0]=FD2_EVENT_VALUE_STATE16_REVERSE_ACTOR;write.args[1]=0;
    unsigned port=0;*before=*game;
    CHECK(common_event_domain(game,0,&write,&port,1)==1 && !memcmp(game,before,sizeof(*game)));
    CHECK(common_event_domain(game,0,&write,&port,0)==1 && game->units.items[20].ai_behavior_raw==0);
    before->units.items[20].ai_behavior_raw=0;CHECK(!memcmp(game,before,sizeof(*game)));
    game->cell_action_completed[16]=255;*before=*game;
    CHECK(common_event_domain(game,0,&write,&port,0)==-1 && !memcmp(game,before,sizeof(*game)));
    write.args[0]=10;CHECK(common_event_domain(game,0,&write,&port,0)==-1 && !memcmp(game,before,sizeof(*game)));
    write.args[0]=FD2_EVENT_VALUE_STATE16;write.args[1]=256;
    CHECK(common_event_domain(game,0,&write,&port,0)==-1 && !memcmp(game,before,sizeof(*game)));
    free(before);free(game);return 0;
}
static int test_projection_scope_and_future_state(void) {
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));CHECK(game && before);
    game->ready=1;game->units.count=35;
    for(unsigned i=0;i<35;i++) {
        game->units.items[i].x=3;game->units.items[i].y=4;game->units.items[i].ai_behavior_raw=7;
    }
    fd2_static_flow_node nodes[6];
    nodes[0]=node("event_byte_write",1);nodes[0].args[0]=16;nodes[0].args[1]=16;nodes[0].args[2]=4;
    nodes[1]=node("teleport_begin",2);nodes[1].args[0]=-1;
    nodes[1].args[1]=nodes[1].args[3]=22;nodes[1].args[2]=nodes[1].args[4]=18;
    nodes[2]=node("call_teleport",3);
    nodes[3]=node("unit_ai_write_source",4);nodes[3].args[0]=FD2_EVENT_VALUE_STATE16_REVERSE_ACTOR;
    nodes[4]=node("story",-1);nodes[4].edges[FD2_FLOW_ACK]=5;nodes[5]=node("end",-1);
    fd2_static_flow flow={"S5-source-projection-scope",nodes,6,0};*before=*game;
    for(projected_state=0;projected_state<=4;projected_state++) {
        nodes[0].args[2]=(int32_t)projected_state;begins=calls=stories=0;
        CHECK(common_event_preflight_identity(game,&flow,0)==1 && begins==1 && calls==1 && stories==1);
        CHECK(!memcmp(game,before,sizeof(*game)));
    }
    projected_state=4;nodes[0].args[2]=4;
    nodes[1].edges[FD2_FLOW_NEXT]=5;
    CHECK(common_event_preflight_identity(game,&flow,0)==-1 && !memcmp(game,before,sizeof(*game)));
    nodes[0].edges[FD2_FLOW_NEXT]=2;
    CHECK(common_event_preflight_identity(game,&flow,0)==-1 && !memcmp(game,before,sizeof(*game)));
    nodes[0].edges[FD2_FLOW_NEXT]=1;nodes[1].edges[FD2_FLOW_NEXT]=3;
    CHECK(common_event_preflight_identity(game,&flow,0)==-1 && !memcmp(game,before,sizeof(*game)));
    nodes[1].edges[FD2_FLOW_NEXT]=2;nodes[0].args[2]=255;
    CHECK(common_event_preflight_identity(game,&flow,0)==-1 && !memcmp(game,before,sizeof(*game)));
    free(before);free(game);return 0;
}
static int test_inactive_xy_projection(void) {
    fd2_field_game *game=calloc(1,sizeof(*game)),*before=malloc(sizeof(*before));CHECK(game && before);
    game->ready=1;game->units.count=29;
    memset(game->units.items+29,0xa5,(FD2_FIELD_MAX_UNITS-29)*sizeof(fd2_field_unit));
    fd2_static_flow_node nodes[5];
    nodes[0]=node("event_byte_write",1);nodes[0].args[0]=16;nodes[0].args[1]=16;nodes[0].args[2]=2;
    nodes[1]=node("teleport_begin",2);nodes[1].args[0]=-2;
    nodes[1].args[1]=nodes[1].args[3]=21;nodes[1].args[2]=nodes[1].args[4]=18;
    nodes[2]=node("call_teleport",3);nodes[3]=node("story",-1);nodes[3].edges[FD2_FLOW_ACK]=4;
    nodes[4]=node("end",-1);
    fd2_static_flow flow={"F1-inactive-xy-projection",nodes,5,0};*before=*game;
    const int selectors[]={-2,-3,95};const unsigned actors[]={29,30,95};
    for(unsigned i=0;i<3;i++) {
        nodes[1].args[0]=selectors[i];inactive_actor=actors[i];begins=calls=stories=0;
        CHECK(common_event_preflight_identity(game,&flow,0)==1 && begins==1 && calls==1 && stories==1);
        CHECK(!memcmp(game,before,sizeof(*game)));
    }
    nodes[1].args[0]=96;CHECK(common_event_preflight_identity(game,&flow,0)==-1 && !memcmp(game,before,sizeof(*game)));
    nodes[1].args[0]=-2;nodes[0].args[2]=36;
    CHECK(common_event_preflight_identity(game,&flow,0)==-1 && !memcmp(game,before,sizeof(*game)));
    fd2_static_flow_node write=node("unit_ai_write",-1);write.args[0]=write.args[1]=29;write.args[2]=1;
    unsigned port=0;
    CHECK(common_event_domain(game,0,&write,&port,0)==-1 && !memcmp(game,before,sizeof(*game)));
    write=node("branch_actor",-1);write.args[0]=29;write.args[1]=FD2_EVENT_ACTOR_ID;
    CHECK(common_event_query(game,0,&write,&port)==-1 && !memcmp(game,before,sizeof(*game)));
    inactive_actor=0;free(before);free(game);return 0;
}
int main(void) {
    CHECK(test_selectors_and_AI_write()==0 && test_projection_scope_and_future_state()==0 && test_inactive_xy_projection()==0);
    puts("field_s5_callback82: selector/AI writes and local resource projection PASS (no GUI)");return 0;
}
