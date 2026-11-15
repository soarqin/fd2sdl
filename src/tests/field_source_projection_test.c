/* S2未来身份/条件/换图预检：使用真实typed原语与局部资源callback，
 * 不把合成组合称作原关卡；真实资源另测Scene只读资源view。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "scene.h"
#include "field_common_event_flow.h"

#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return -1;}}while(0)

static int common_event_leaf(const char *op) {
    return !strcmp(op,"story") || !strcmp(op,"pan") || !strcmp(op,"map_replace") || !strcmp(op,"move");
}
static int common_event_preflight_leaf_view(const fd2_field_game *g,const fd2_static_flow_node *n,
        size_t actor,const fd2_field_event_resource_view *view) {
    (void)actor;return g->event_preflight?g->event_preflight(g,n,view,g->event_preflight_userdata):0;
}
#include "field_common_event_projection.h"

typedef struct {unsigned stories, future, maps, rng;int reject;} trace;
static uint32_t rng(void *data){((trace *)data)->rng++;return 1;}
static int inspect(const fd2_field_game *g,const fd2_static_flow_node *n,
        const fd2_field_event_resource_view *view,void *data) {
    trace *t=data;
    if(!view || !view->units || g->units.count!=1 || g->units.items[0].unit_id!=0)return -1;
    if(!strcmp(n->op,"story")) {
        t->stories++;
        if(n->args[0]==2 || t->reject)return -1; /* 已知不达边不能因此误拒真实路径。 */
        if(view->units->count==2 && view->units->items[1].unit_id==5)t->future++;
    }
    if(!strcmp(n->op,"pan")) {
        if(!view->has_map_override || view->map_entry!=3 || view->shape_index!=0)return -1;
        t->maps++;
    }
    return 0;
}
static fd2_static_flow_node node(const char *op) {
    fd2_static_flow_node n={0};n.op=op;
    for(unsigned p=0;p<FD2_FLOW_PORT_COUNT;p++)n.edges[p]=-1;
    return n;
}
static fd2_field_game *game(trace *t) {
    fd2_field_game *g=calloc(1,sizeof(*g));if(!g)return NULL;
    g->ready=1;g->units.count=1;
    if(fd2_field_unit_init(&g->units.items[0],0,0,2,1,1)!=0){free(g);return NULL;}
    g->event_preflight=inspect;g->event_preflight_userdata=t;g->attack_rng=rng;g->attack_rng_userdata=t;
    return g;
}
static int run_unchanged(fd2_field_game *g,fd2_static_flow_node *nodes,size_t count,int expected) {
    fd2_field_game *before=malloc(sizeof(*before));CHECK(before);*before=*g;
    fd2_static_flow flow={"source-mechanism-combination",nodes,(uint16_t)count,0};
    int result=common_event_preflight_identity(g,&flow,0);
    CHECK(result==expected);CHECK(!memcmp(before,g,sizeof(*g)));
    free(before);return 0;
}
static int test_identity_write_then_known_branch(void) {
    trace t={0};fd2_field_game *g=game(&t);CHECK(g);
    fd2_static_flow_node nodes[5];nodes[0]=node("unit_field_write");
    nodes[0].args[2]=FD2_UNIT_TEXT_ID;nodes[0].args[3]=FD2_UNIT_SET;nodes[0].args[4]=9;nodes[0].edges[FD2_FLOW_NEXT]=1;
    nodes[1]=node("branch_actor");nodes[1].args[1]=FD2_EVENT_ACTOR_TEXT;nodes[1].args[2]=FD2_EVENT_EQ;nodes[1].args[3]=9;
    nodes[1].edges[FD2_FLOW_YES]=2;nodes[1].edges[FD2_FLOW_NO]=3;
    nodes[2]=node("story");nodes[2].args[0]=1;nodes[2].edges[FD2_FLOW_ACK]=4;
    nodes[3]=node("story");nodes[3].args[0]=2;nodes[3].edges[FD2_FLOW_ACK]=4;nodes[4]=node("end");
    CHECK(run_unchanged(g,nodes,5,1)==0 && t.stories==1 && t.rng==0);
    t.reject=1;CHECK(run_unchanged(g,nodes,5,-1)==0 && t.rng==0);
    free(g);return 0;
}
static int test_append_future_raw_and_capacity(void) {
    trace t={0};fd2_field_game *g=game(&t);CHECK(g);
    fd2_field_unit_template template={0};template.bytes[0]=2;template.bytes[1]=5;template.bytes[21]=7;
    fd2_field_placement placement={3,4,99};g->metadata.unit_templates=&template;g->metadata.unit_template_count=1;g->metadata.unknown_02=1;
    g->placements.records=&placement;g->placements.count=1;
    fd2_static_flow_node nodes[4];nodes[0]=node("append_group");nodes[0].args[0]=7;nodes[0].edges[FD2_FLOW_NEXT]=1;
    nodes[1]=node("branch_actor");nodes[1].args[0]=1;nodes[1].args[1]=FD2_EVENT_ACTOR_ID;nodes[1].args[2]=FD2_EVENT_EQ;nodes[1].args[3]=5;
    nodes[1].edges[FD2_FLOW_YES]=2;nodes[1].edges[FD2_FLOW_NO]=3;
    nodes[2]=node("story");nodes[2].args[0]=1;nodes[2].edges[FD2_FLOW_ACK]=3;nodes[3]=node("end");
    CHECK(run_unchanged(g,nodes,4,1)==0 && t.future==1 && t.rng==0);
    /* constructor的HP尚无投影：不能用临时0把未来结果分支压成单边。 */
    nodes[1].op="result_leader_dead";nodes[1].args[1]=1;
    nodes[1].edges[FD2_FLOW_NO]=4;
    fd2_static_flow_node both[5];memcpy(both,nodes,sizeof(nodes));
    both[4]=node("story");both[4].args[0]=2;both[4].edges[FD2_FLOW_ACK]=3;
    CHECK(run_unchanged(g,both,5,-1)==0 && !t.rng);
    g->units.count=FD2_FIELD_MAX_UNITS;CHECK(run_unchanged(g,nodes,4,-1)==0);
    free(g);return 0;
}
static int test_state_roster_and_map_paths(void) {
    trace t={0};fd2_field_game *g=game(&t);CHECK(g);
    fd2_static_flow_node nodes[7];nodes[0]=node("roster_append");nodes[0].args[0]=18;nodes[0].edges[FD2_FLOW_NEXT]=1;
    nodes[1]=node("event_byte_write");nodes[1].args[0]=nodes[1].args[1]=16;nodes[1].args[2]=1;nodes[1].edges[FD2_FLOW_NEXT]=2;
    nodes[2]=node("branch_event_byte");nodes[2].args[0]=16;nodes[2].args[1]=FD2_EVENT_EQ;nodes[2].args[2]=1;
    nodes[2].edges[FD2_FLOW_YES]=3;nodes[2].edges[FD2_FLOW_NO]=6;
    nodes[3]=node("branch_roster_text");nodes[3].args[0]=18;nodes[3].edges[FD2_FLOW_YES]=4;nodes[3].edges[FD2_FLOW_NO]=6;
    nodes[4]=node("map_replace");nodes[4].args[0]=3;nodes[4].edges[FD2_FLOW_NEXT]=5;
    nodes[5]=node("pan");nodes[5].edges[FD2_FLOW_NEXT]=6;nodes[6]=node("end");
    CHECK(run_unchanged(g,nodes,7,1)==0 && t.maps==1 && t.rng==0);
    g->roster_count=FD2_SAVE_BATTLE_MAX_ROSTER;CHECK(run_unchanged(g,nodes,7,-1)==0);
    free(g);return 0;
}
static int test_unknown_inventory_keeps_known_state_branch(void) {
    trace t={0};fd2_field_game *g=game(&t);CHECK(g);
    fd2_static_flow_node nodes[6];nodes[0]=node("unit_field_write");
    nodes[0].args[2]=FD2_UNIT_TEXT_ID;nodes[0].args[4]=9;nodes[0].edges[FD2_FLOW_NEXT]=1;
    nodes[1]=node("grant_party_item");nodes[1].args[0]=89;nodes[1].edges[FD2_FLOW_NEXT]=2;
    nodes[2]=node("branch_event_byte");nodes[2].args[0]=16;nodes[2].args[1]=FD2_EVENT_EQ;
    nodes[2].edges[FD2_FLOW_YES]=3;nodes[2].edges[FD2_FLOW_NO]=4;
    nodes[3]=node("story");nodes[3].args[0]=1;nodes[3].edges[FD2_FLOW_ACK]=5;
    nodes[4]=node("story");nodes[4].args[0]=2;nodes[4].edges[FD2_FLOW_ACK]=5;nodes[5]=node("end");
    CHECK(run_unchanged(g,nodes,6,1)==0 && t.stories==1 && !t.rng);
    free(g);return 0;
}
static int test_scene_future_map_resource_guard(void) {
    fd2_archive other={0};CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);
    CHECK(fd2_field_game_open_resources(g,&other,0)==0);
    const uint8_t *metadata;size_t size;
    CHECK(fd2_archive_get(&g->field_archive,4,&metadata,&size)==0 && size>0);
    fd2_field_event_resource_view view={&g->units,g->roster,g->roster_count,3,metadata[0],1};
    fd2_static_flow_node pan=node("pan");fd2_field_map original=g->map;
    CHECK(fd2_scene_common_step_validate_resource_view(g,&other,&pan,&view)==0);
    view.shape_index=1000000;CHECK(fd2_scene_common_step_validate_resource_view(g,&other,&pan,&view)==-1);
    CHECK(g->map.cells==original.cells && g->map.width==original.width && g->map.height==original.height);
    fd2_field_game_close(g);free(g);fd2_archive_close(&other);return 0;
}

static int test_move_source_boundary_preserves_entry(void) {
    trace t={0};fd2_field_game *g=game(&t);CHECK(g);
    fd2_static_flow_node nodes[3];nodes[0]=node("move");nodes[0].args[0]=75;
    nodes[0].edges[FD2_FLOW_NEXT]=1;
    nodes[1]=node("story");nodes[1].args[0]=2;nodes[1].edges[FD2_FLOW_ACK]=2;
    nodes[2]=node("end");
    CHECK(run_unchanged(g,nodes,3,1)==0 && !t.stories && !t.rng);
    nodes[0].args[0]=106;CHECK(run_unchanged(g,nodes,3,-1)==0);
    /* 233c6只写xy/dir，不以该SET把后续身份分支变成已构造actor。 */
    nodes[0]=node("unit_field_write");nodes[0].args[0]=nodes[0].args[1]=72;
    nodes[0].args[2]=FD2_UNIT_X;nodes[0].args[4]=22;nodes[0].edges[FD2_FLOW_NEXT]=1;
    nodes[1]=node("end");CHECK(run_unchanged(g,nodes,2,1)==0);
    nodes[0].args[2]=FD2_UNIT_ID;CHECK(run_unchanged(g,nodes,2,-1)==0);
    nodes[0].op="unit_ai_write";nodes[0].args[2]=1;CHECK(run_unchanged(g,nodes,2,-1)==0);
    free(g);return 0;
}
int main(void) {
    CHECK(test_identity_write_then_known_branch()==0);
    CHECK(test_append_future_raw_and_capacity()==0);
    CHECK(test_state_roster_and_map_paths()==0);
    CHECK(test_unknown_inventory_keeps_known_state_branch()==0);
    CHECK(test_scene_future_map_resource_guard()==0);
    CHECK(test_move_source_boundary_preserves_entry()==0);
    puts("S2 bounded source projection: ok");return 0;
}
