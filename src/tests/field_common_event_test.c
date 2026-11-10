/* G4共用事件领域与冷图的直接回归；UI叶为显式ACK/错误替身，
 * 不冒称真实输入、原版等待RNG或图形／音频对照。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "field_item.h"
#include "static_data.h"
#include "field_common_event_flow.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return -1; } } while (0)

static void inventory_empty(fd2_field_unit *unit) {
    uint8_t *raw = (uint8_t *)unit;
    for (unsigned i=0;i<8;i++) raw[0x0a+i*2]=0x80;
}

typedef struct { unsigned calls, stories, fragments[8]; int fail_story; } trace;
static uint32_t test_rng(void *userdata) {
    trace *t=userdata; t->calls++; return 2;
}

static int run(fd2_field_game *game, const char *id, size_t caller, trace *t) {
    const fd2_static_flow *flow=fd2_static_flow_lookup("common_event",id);
    if (!flow) return -1;
    int pc=flow->entry;
    for (unsigned count=0;count<FD2_FLOW_STEP_BUDGET;count++) {
        const fd2_static_flow_node *node=fd2_static_flow_at(flow,pc);
        if (!node || !strcmp(node->op,"fail")) return -1;
        if (!strcmp(node->op,"end")) return 0;
        unsigned port=FD2_FLOW_NEXT;
        int handled=common_event_domain(game,caller,node,&port,0);
        if (handled<0) return -1;
        if (!handled) {
            if (strcmp(node->op,"story")) return -1;
            if (t->stories>=8) return -1;
            t->fragments[t->stories++]=(unsigned)node->args[0];
            port=t->fail_story ? FD2_FLOW_ERROR : FD2_FLOW_ACK;
        }
        pc=fd2_static_flow_edge(flow,node,port);
    }
    return -1;
}

static int test_once_and_late_failure(void) {
    fd2_field_game *game=calloc(1,sizeof(*game)); CHECK(game);
    game->units.count=28;
    for (unsigned i=0;i<28;i++) { inventory_empty(&game->units.items[i]); game->units.items[i].ai_behavior_raw=0xa2; }
    trace t={0};
    CHECK(run(game,"common.state.once",0,&t)==0);
    CHECK(t.stories==1 && t.fragments[0]==3 && game->cell_action_completed[16]==1);
    for (unsigned i=0;i<28;i++) CHECK(game->units.items[i].ai_behavior_raw==(i>=24 ? 0xa7:0xa2));
    CHECK(run(game,"common.state.once",0,&t)==0 && t.stories==1);

    game->cell_action_completed[16]=0; t=(trace){.fail_story=1};
    CHECK(run(game,"common.inventory.once",0,&t)==-1);
    CHECK(fd2_field_unit_inventory_count(&game->units.items[0])==1 &&
        game->cell_action_completed[16]==0 && t.stories==1);
    /* 实际物品已经提交，晚文本失败不得整单位/roster回滚。正式owner在此冻结；
     * 本纯test不再重启失败事件，不能拿新图entry重跑冒充合法续行。 */
    CHECK(((uint8_t *)&game->units.items[0])[0x0b]==89);
    game->cell_action_completed[16]=1; t=(trace){0};
    CHECK(run(game,"common.inventory.once",0,&t)==0 && t.stories==0);
    game->cell_action_completed[16]=0;
    CHECK(run(game,"common.inventory.once",1,&t)==0 && t.stories==0);
    free(game); return 0;
}

static int test_scheduling_and_rng(void) {
    fd2_field_game *game=calloc(1,sizeof(*game)); CHECK(game);
    game->units.count=3; game->units.items[2].side=1;
    game->turn_number=255; game->cell_action_completed[18]=1;
    game->metadata.turn_events[2]=(fd2_field_turn_event){77,19,2};
    trace t={0}; game->attack_rng=test_rng; game->attack_rng_userdata=&t;
    CHECK(run(game,"common.turn.relative",2,&t)==0);
    CHECK(game->metadata.turn_events[2].turn==255 &&
        game->metadata.turn_events[2].action==19 && game->metadata.turn_events[2].phase==2 &&
        game->cell_action_completed[17]==1);
    game->turn_number=300;
    CHECK(run(game,"common.turn.relative",2,&t)==0 && game->metadata.turn_events[2].turn==255);
    game->turn_number=255; game->cell_action_completed[21]=0;
    CHECK(run(game,"common.random.pair",2,&t)==0 && t.calls==1);
    CHECK(game->metadata.turn_events[2].turn==0 &&
        game->units.items[2].flags==0x80 && game->units.items[0].flags==0x80 &&
        game->units.items[1].flags==0);
    fd2_static_flow_node node={.op="mark_random_pair",.args={21}};
    game->cell_action_completed[21]=1;
    fd2_field_units before=game->units; unsigned port=77;
    CHECK(common_event_domain(game,2,&node,&port,0)==-1 && t.calls==1);
    CHECK(!memcmp(&before,&game->units,sizeof(before)));
    free(game); return 0;
}

static int test_validation_and_dynamic_item(void) {
    fd2_field_game *game=calloc(1,sizeof(*game)), *before=malloc(sizeof(*before)); CHECK(game && before);
    game->units.count=2; game->event_resume.active=1;
    inventory_empty(&game->units.items[0]); inventory_empty(&game->units.items[1]);
    uint32_t cell=4u<<16; game->map=(fd2_field_map){.width=1,.height=1,.cells=&cell};
    fd2_static_flow_node prepare={.op="cell_item_prepare"}; unsigned port;
    *before=*game;
    CHECK(common_event_domain(game,0,&prepare,&port,1)==1 && !memcmp(game,before,sizeof(*game)));
    CHECK(common_event_domain(game,0,&prepare,&port,0)==1 && game->event_resume.has_item &&
        game->event_resume.item_id==71);
    fd2_static_flow_node append={.op="event_item_append",.args={-1,-1}};
    *before=*game;
    CHECK(common_event_domain(game,1,&append,&port,1)==1 && port==FD2_FLOW_YES && !memcmp(game,before,sizeof(*game)));
    CHECK(common_event_domain(game,1,&append,&port,0)==1 && port==FD2_FLOW_YES &&
        ((uint8_t *)&game->units.items[1])[0x0b]==71 && !fd2_field_unit_inventory_count(&game->units.items[0]));
    game->event_resume.has_item=0; *before=*game;
    CHECK(common_event_domain(game,1,&append,&port,0)==-1 && !memcmp(game,before,sizeof(*game)));
    cell=0x84u<<16; *before=*game;
    CHECK(common_event_domain(game,0,&prepare,&port,0)==-1 && !memcmp(game,before,sizeof(*game)));
    /* 不能把超界完整BYTE2 &31后当成合法4。 */
    fd2_static_flow_node write={.op="event_byte_write",.args={0,32,1,0}};
    CHECK(common_event_domain(game,0,&write,&port,0)==-1 && !memcmp(game,before,sizeof(*game)));
    fd2_static_flow_node unknown={.op="unimplemented_event"};
    CHECK(common_event_domain(game,0,&unknown,&port,0)==0);
    const fd2_field_game *query_owner=game;
    fd2_static_flow_node query={.op="branch_turn",.args={FD2_EVENT_LT,15}};
    game->turn_number=UINT32_MAX;*before=*game;
    CHECK(common_event_query(query_owner,0,&query,&port)==1 && port==FD2_FLOW_YES &&
        !memcmp(game,before,sizeof(*game)));
    CHECK(common_event_query(query_owner,0,&append,&port)==0 && !memcmp(game,before,sizeof(*game)));
    query.args[0]=6;
    CHECK(common_event_query(query_owner,0,&query,&port)==-1 && !memcmp(game,before,sizeof(*game)));
    /* battle result没有caller；只接显式raw actor，绝不能把SIZE_MAX补成actor0。 */
    game->units.items[1].hp=0x1234;*before=*game;
    query=(fd2_static_flow_node){.op="branch_actor",.args={1,FD2_EVENT_ACTOR_HP,FD2_EVENT_GE,0x1234}};
    CHECK(common_event_query(game,SIZE_MAX,&query,&port)==1 && port==FD2_FLOW_YES &&
        !memcmp(game,before,sizeof(*game)));
    query.args[0]=-1;port=0x7e;
    CHECK(common_event_query(game,SIZE_MAX,&query,&port)==-1 && port==0x7e &&
        !memcmp(game,before,sizeof(*game)));
    query=(fd2_static_flow_node){.op="branch_item",.args={-1,1}};
    CHECK(common_event_query(game,SIZE_MAX,&query,&port)==-1 && port==0x7e &&
        !memcmp(game,before,sizeof(*game)));
    query=(fd2_static_flow_node){.op="branch_caller",.args={FD2_EVENT_EQ,0}};
    CHECK(common_event_query(game,SIZE_MAX,&query,&port)==-1 && port==0x7e &&
        !memcmp(game,before,sizeof(*game)));
    free(before); free(game); return 0;
}

static int test_runtime_wait_and_committed_error(void) {
    fd2_field_game *g=calloc(1,sizeof(*g)); CHECK(g);
    g->ready=1; g->units.count=1; inventory_empty(&g->units.items[0]);
    CHECK(fd2_field_game_common_event_begin(g,"common.inventory.once",0)==0);
    fd2_static_flow_node node; size_t actor;
    CHECK(fd2_field_game_common_event_peek(g,&node,&actor)==1 && actor==0 && !strcmp(node.op,"story"));
    CHECK(fd2_field_game_progress_resume(g)==1);
    CHECK(((uint8_t *)&g->units.items[0])[0x0b]==89 && g->cell_action_completed[16]==0);
    CHECK(fd2_field_game_common_event_begin(g,"common.inventory.once",0)==-1);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_NEXT)==-1 && g->cell_action_completed[16]==0);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0 && !g->event_resume.active &&
        g->cell_action_completed[16]==1);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==-1);
    CHECK(fd2_field_game_common_event_begin(g,"common.inventory.once",0)==0 && !g->event_resume.active);
    CHECK(fd2_field_unit_inventory_count(&g->units.items[0])==1);
    g->cell_action_completed[16]=0;
    CHECK(fd2_field_game_common_event_begin(g,"common.inventory.once",0)==0);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ERROR)==-1 && g->event_resume.active &&
        g->failure==FD2_FIELD_FAILURE_ACTION && fd2_field_unit_inventory_count(&g->units.items[0])==2);
    CHECK(g->cell_action_completed[16]==0); free(g); return 0;
}

static int test_runtime_child_parent_queue_and_cancel(void) {
    fd2_field_game *g=calloc(1,sizeof(*g)); CHECK(g);
    g->ready=1; g->units.count=2; g->units.items[1].side=2;
    inventory_empty(&g->units.items[0]);inventory_empty(&g->units.items[1]);
    g->loot.count=2;g->loot.cursor=1;g->loot.actor_index=0;g->loot.source=1;
    CHECK(fd2_field_game_common_event_begin(g,"common.loot.child.2",1)==-1);
    g->loot.source=0;
    memset(g->loot.descriptors,0x5a,sizeof(g->loot.descriptors));
    uint8_t parent[sizeof(g->loot.descriptors)]; memcpy(parent,g->loot.descriptors,sizeof(parent));
    CHECK(fd2_field_game_common_event_begin(g,"common.loot.child.2",1)==0 && g->event_resume.child_wait);
    fd2_field_loot_notice notice;
    CHECK(fd2_field_game_loot_peek(g,&notice)==1 && notice.actor_index==1 && notice.item_id==101 &&
        notice.kind==FD2_FIELD_LOOT_MESSAGE && fd2_field_unit_inventory_count(&g->units.items[1])==0);
    CHECK(fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_ACK,-1)==-1);
    CHECK(fd2_field_game_loot_text_ready(g)==0 && fd2_field_unit_inventory_count(&g->units.items[1])==1);
    CHECK(fd2_field_game_loot_text_ready(g)==-1);
    CHECK(fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && !g->event_resume.child_wait);
    fd2_static_flow_node node;size_t actor;
    CHECK(fd2_field_game_common_event_peek(g,&node,&actor)==1 && !strcmp(node.op,"story") &&
        node.args[0]==11 && actor==1);
    CHECK(g->loot.count==2 && g->loot.cursor==1 && g->loot.actor_index==0 && g->loot.source==0 &&
        !memcmp(parent,g->loot.descriptors,sizeof(parent)));
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0 && !g->event_resume.active);
    g->cell_action_completed[16]=0;
    CHECK(fd2_field_game_common_event_begin(g,"common.inventory.once",0)==0);
    g->pending_cell_event.valid=1;g->player_post_waiting_phase=1;g->player_post_actor=0;
    g->event_resume.completion=FD2_EVENT_COMPLETE_PLAYER_PENDING;
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0 && !g->pending_cell_event.valid &&
        !g->player_post_waiting_phase && !g->event_resume.active);
    /* 满包子的No/Cancel是完成子描述符；父图继续，原库存与父队列不回滚。 */
    uint8_t *raw=(uint8_t *)&g->units.items[1];
    for(unsigned i=0;i<8;i++){raw[0x0a+i*2]=0;raw[0x0b+i*2]=1;}
    fd2_field_unit before=g->units.items[1];
    CHECK(fd2_field_game_common_event_begin(g,"common.loot.child.2",1)==0);
    CHECK(fd2_field_game_loot_text_ready(g)==0);
    CHECK(fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
    CHECK(fd2_field_game_loot_peek(g,&notice)==1 && notice.kind==FD2_FIELD_LOOT_DELAY);
    CHECK(fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
    CHECK(fd2_field_game_loot_peek(g,&notice)==1 && notice.kind==FD2_FIELD_LOOT_CHOICE);
    CHECK(fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_CANCEL,-1)==0);
    CHECK(fd2_field_game_loot_text_ready(g)==0 && fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
    CHECK(!g->event_resume.child_wait && !memcmp(&before,&g->units.items[1],sizeof(before)) &&
        !memcmp(parent,g->loot.descriptors,sizeof(parent)));
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0);free(g);return 0;
}

static int test_runtime_dynamic_sources(void) {
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);g->ready=1;g->units.count=1;
    fd2_field_unit_template template={{0}};
    template.bytes[0]=1;template.bytes[1]=134;template.bytes[4]=3;
    template.bytes[5]=0xff;template.bytes[6]=0x80;
    for(unsigned i=7;i<13;i++)template.bytes[i]=0xff;
    template.bytes[0x15]=0;template.bytes[0x16]=0xff;
    g->metadata.unit_templates=&template;g->metadata.unit_template_count=1;g->metadata.unknown_02=1;
    /* 原DWORD不匹配或无模板均是空操作；不要求本不会访问的map/placement。 */
    for(unsigned i=0;i<3;i++){
        g->turn_number=i==0?256u:i==1?UINT32_MAX:7u;
        CHECK(fd2_field_game_common_event_begin(g,"common.group.turn.source",0)==0 && !g->event_resume.active);
        CHECK(g->units.count==1);
    }
    uint32_t cells[9]={0};fd2_field_placement placement={1,1,134};
    g->map=(fd2_field_map){3,3,cells};g->placements=(fd2_field_placements){1,&placement};
    g->sprites.frame_count=SIZE_MAX;g->stage=98;g->turn_number=0;
    CHECK(fd2_field_game_common_event_begin(g,"common.group.turn.source",0)==0 && g->units.count==2);
    CHECK(fd2_field_game_common_event_begin(g,"common.group.turn.source",0)==0 && g->units.count==3);
    CHECK(g->units.source_template_indices[1]==0 && g->units.source_template_indices[2]==0);
    g->cell_action_completed[16]=4;
    CHECK(fd2_field_game_common_event_begin(g,"common.story.state.source",0)==0);
    fd2_static_flow_node node;size_t actor;
    CHECK(fd2_field_game_common_event_peek(g,&node,&actor)==1 && !strcmp(node.op,"story") && node.args[0]==6);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0);
    free(g);return 0;
}

static int reject_unreachable_item_text(const fd2_field_game *game,
        const fd2_static_flow_node *node,void *userdata) {
    (void)game;(void)userdata;
    return !strcmp(node->op,"message") && node->args[0]==422 ? -1:0;
}

static int test_runtime_preflight_obeys_before_write_branch(void) {
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);g->ready=1;g->units.count=1;
    g->event_preflight=reject_unreachable_item_text;
    /* 满包分支不读map/index也不显示422；资源门不能因未到达分支误拒。 */
    CHECK(fd2_field_game_common_event_begin(g,"common.cell.grant",0)==0);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_NEXT)==0);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_NEXT)==0);
    fd2_static_flow_node node;size_t actor;
    CHECK(fd2_field_game_common_event_peek(g,&node,&actor)==1 && !strcmp(node.op,"message") && node.args[0]==480);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_NEXT)==0);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_NEXT)==0);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_NEXT)==0 && !g->event_resume.active);
    inventory_empty(&g->units.items[0]);
    CHECK(fd2_field_game_common_event_begin(g,"common.cell.grant",0)==-1 && !g->event_resume.active &&
        g->failure==FD2_FIELD_FAILURE_NONE);
    free(g);return 0;
}

static int test_runtime_prior_presentation_blocks_writes(void) {
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);g->ready=1;g->units.count=1;
    inventory_empty(&g->units.items[0]);g->event_log_count=1;
    g->event_log[0].handled=1;g->event_log[0].presentation_deferred=1;
    CHECK(fd2_field_game_common_event_begin(g,"common.inventory.once",0)==0 && g->event_resume.waiting_legacy);
    CHECK(fd2_field_unit_inventory_count(&g->units.items[0])==0);
    CHECK(fd2_field_game_common_event_resume(g)==1 && fd2_field_unit_inventory_count(&g->units.items[0])==0);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==-1);
    g->event_log[0].presentation_deferred=0;
    CHECK(fd2_field_game_common_event_resume(g)==0 && !g->event_resume.waiting_legacy &&
        fd2_field_unit_inventory_count(&g->units.items[0])==1);
    CHECK(fd2_field_game_common_event_resume(g)==-1);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0 && g->cell_action_completed[16]==1);
    free(g);return 0;
}

int main(void) {
    CHECK(test_once_and_late_failure()==0);
    CHECK(test_scheduling_and_rng()==0);
    CHECK(test_validation_and_dynamic_item()==0);
    CHECK(test_runtime_wait_and_committed_error()==0);
    CHECK(test_runtime_child_parent_queue_and_cancel()==0);
    CHECK(test_runtime_dynamic_sources()==0);
    CHECK(test_runtime_preflight_obeys_before_write_branch()==0);
    CHECK(test_runtime_prior_presentation_blocks_writes()==0);
    puts("field common event tests: ok"); return 0;
}
