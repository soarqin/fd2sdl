/* S3结果表五CALLsite的生产Game续点。
 * 0x1197b/0x1a94d/0x1d8a0/0x1d96c/0x1d9fc来源另由完整原caller
 * 机器码核验。此非shipping TU只替换结果叶为wait/result_set，用真实
 * AI/phase/reward/loot caller和现有event_resume观察调用次序；不模拟AI本体。
 * 完整结果函数另有916原码/actual-C差分，不以此替身证明结果语义。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "static_data.h"
static const fd2_static_flow *caller_lookup(const char *,const char *);
#define fd2_static_flow_lookup caller_lookup
#include "../field_game.c"
#undef fd2_static_flow_lookup

#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return -1;}}while(0)
static fd2_static_flow_node result_nodes[]={
    {"wait","wait_key",{0},{-1,-1,-1,-1,1,-1,-1,2},3},
    {"write","result_set",{0},{3,-1,-1,-1,-1,-1,-1,2},1},
    {"failed","fail",{0},{-1,-1,-1,-1,-1,-1,-1,-1},1},
    {"done","end",{0},{-1,-1,-1,-1,-1,-1,-1,-1},1}
};
static const fd2_static_flow result_fixture={"s3.result.caller.fixture",result_nodes,4,0};
static int use_result_fixture=1;
static const fd2_static_flow *caller_lookup(const char *domain,const char *id) {
    if(use_result_fixture && !strcmp(domain,"common_event") && !strcmp(id,"stage.012.result"))return &result_fixture;
    return fd2_static_flow_lookup(domain,id);
}
static unsigned rng_calls;
static uint32_t count_rng(void *p){(*(unsigned *)p)++;return 1;}
static void setup(fd2_field_game *g,uint8_t side) {
    memset(g,0,sizeof(*g));g->ready=1;g->stage=12;g->units.count=5;g->active_side=side;
    g->selected_unit=g->command_selected=-1;g->interaction=FD2_FIELD_INTERACTION_BROWSE;
    g->turn_number=1;g->attack_rng=count_rng;g->attack_rng_userdata=&rng_calls;
    rng_calls=0;result_nodes[1].args[0]=0;
    for(size_t i=0;i<g->units.count;i++) {
        g->units.items[i].side=side;g->units.items[i].hp=100;
        ((uint8_t *)&g->units.items[i])[0x31]=255;
        for(size_t slot=0;slot<8;slot++)((uint8_t *)&g->units.items[i])[10+slot*2]=0x80;
    }
    /* 五种AI本体gate都必须经过outer的结果调用。bit04由13a9f内部拒绝。 */
    g->units.items[0].side=2;g->units.items[1].flags=1;g->units.items[2].flags=0x80;
    g->units.items[3].detail_status[1]=1;g->units.items[4].flags=4;
    for(size_t i=0;i<FD2_FIELD_EVENT_SLOTS;i++)g->metadata.turn_events[i].turn=255;
}
static int waiting(fd2_field_game *g,unsigned completion,size_t expected) {
    fd2_static_flow_node n;size_t actor=SIZE_MAX;
    CHECK(g->event_resume.active && g->event_resume.completion==completion);
    CHECK(fd2_field_game_common_event_peek(g,&n,&actor)==1 && actor==expected && !strcmp(n.op,"wait_key"));
    CHECK(!rng_calls);return 0;
}
static int guarded_ack(fd2_field_game *g) {
    fd2_field_game *before=malloc(sizeof(*before));CHECK(before);*before=*g;
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_NEXT)==-1 && !memcmp(g,before,sizeof(*g)));
    g->runtime_host_quit=1;*before=*g;
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==-1 && !memcmp(g,before,sizeof(*g)));
    g->runtime_host_quit=0;
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0);
    CHECK(!g->event_resume.active && !rng_calls);*before=*g;
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==-1 && !memcmp(g,before,sizeof(*g)));
    free(before);return 0;
}
static int cadence(int trace) {
    fd2_field_game *g=malloc(sizeof(*g));CHECK(g);
    for(unsigned side=0;side<=1;side++) {
        setup(g,(uint8_t)side);size_t total=side?5:10,observed[10];
        for(size_t i=0;i<total;i++) {
            CHECK(fd2_field_game_process_automatic_action(g)==1);
            CHECK(waiting(g,FD2_EVENT_COMPLETE_AI_RESULT,i%5)==0);
            CHECK(g->phase_unit_cursor==i%5+1 && g->phase_ai_pass==(side?0:i/5));
            observed[i]=g->event_resume.actor;
            CHECK(guarded_ack(g)==0 && g->battle_result==FD2_FIELD_BATTLE_ONGOING);
        }
        if(trace) {
            printf("{\"side\":%u,\"actors\":[",side);
            for(size_t i=0;i<total;i++)printf("%s%zu",i?",":"",observed[i]);
            puts("]}");
        }
    }
    /* 非零结果在本次返回后即停止，不再推进下一raw/pass。 */
    setup(g,0);result_nodes[1].args[0]=FD2_FIELD_BATTLE_DEFEAT;
    CHECK(fd2_field_game_process_automatic_action(g)==1 && waiting(g,FD2_EVENT_COMPLETE_AI_RESULT,0)==0);
    CHECK(guarded_ack(g)==0 && g->battle_result==FD2_FIELD_BATTLE_DEFEAT);
    CHECK(fd2_field_game_process_automatic_action(g)==0 && g->phase_unit_cursor==1 && !g->event_resume.active);
    free(g);return 0;
}
static int reject_leaf(const fd2_field_game *g,const fd2_static_flow_node *n,
        const fd2_field_event_resource_view *view,void *p) {(void)g;(void)n;(void)view;(void)p;return -1;}
static int ai_failure_prefix(void) {
    fd2_field_game *g=malloc(sizeof(*g));CHECK(g);
    for(int resource=0;resource<2;resource++) {
        setup(g,0);fd2_field_units units=g->units;
        if(resource)g->event_preflight=reject_leaf;
        CHECK(fd2_field_game_process_automatic_action(g)==(resource?-1:1));
        if(!resource) {
            CHECK(waiting(g,FD2_EVENT_COMPLETE_AI_RESULT,0)==0);
            CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ERROR)==-1);
        }
        CHECK(g->failure!=FD2_FIELD_FAILURE_NONE && g->phase_unit_cursor==1 && !g->phase_ai_pass);
        CHECK(g->battle_result==FD2_FIELD_BATTLE_ONGOING && !memcmp(&g->units,&units,sizeof(units)) && !rng_calls);
    }
    free(g);return 0;
}
static int phase_status(void) {
    fd2_field_game *g=malloc(sizeof(*g));CHECK(g);
    for(int failure=0;failure<3;failure++) {
        setup(g,1);g->units.count=1;g->units.items[0].side=1;g->units.items[0].flags=0;
        g->map.width=g->map.height=1;g->units.items[0].hp_max=100;
        /* 六种计时器不在结果等待期间提前递减；2->1不触发呈现/派生重算。 */
        memset(&g->units.items[0].attack_status,2,6);
        if(failure==1)g->event_preflight=reject_leaf;
        apply_phase_status_tick(g,1);
        CHECK(g->progress.notice.kind==FD2_FIELD_PROGRESS_STATUS && g->progress.notice.fragment==487);
        CHECK(g->units.items[0].hp==90 && !g->event_resume.active);
        CHECK(fd2_field_game_progress_ack(g)==(failure==1?-1:0));
        for(size_t i=0;i<6;i++)CHECK((&g->units.items[0].attack_status)[i]==2);
        if(failure==1)CHECK(g->failure!=FD2_FIELD_FAILURE_NONE && !g->event_resume.active);
        else {
            CHECK(waiting(g,FD2_EVENT_COMPLETE_STATUS_RESULT,0)==0);
            fd2_field_progress_notice consumed;
            fd2_field_game *before=malloc(sizeof(*before));CHECK(before);*before=*g;
            CHECK(fd2_field_game_progress_peek(g,&consumed)==0);
            CHECK(fd2_field_game_progress_ack(g)==-1 && !memcmp(g,before,sizeof(*g)) && !rng_calls);
            free(before);
            if(failure==2) {
                CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ERROR)==-1);
                for(size_t i=0;i<6;i++)CHECK((&g->units.items[0].attack_status)[i]==2);
            } else {
                CHECK(guarded_ack(g)==0 && g->progress.resume==FD2_FIELD_PROGRESS_RESUME_NONE);
                for(size_t i=0;i<6;i++)CHECK((&g->units.items[0].attack_status)[i]==1);
            }
        }
        CHECK(!rng_calls);
    }
    free(g);return 0;
}
static int reward_and_loot(void) {
    fd2_field_game *g=malloc(sizeof(*g));CHECK(g);
    for(int route=0;route<2;route++)for(int error=0;error<3;error++) {
        setup(g,1);g->experience_value=17;
        if(error==2)g->event_preflight=reject_leaf;
        if(!route) {
            /* 奖励CLOSE的真实ACK提交experience，之后outer才调用结果表。 */
            const fd2_static_flow *f=fd2_reward_graph();int pc=fd2_static_flow_label(f,"close");CHECK(pc>=0);
            g->progress.flow_step=(uint8_t)pc;g->progress.total=17;
            g->progress.resume=FD2_FIELD_PROGRESS_RESUME_AI_SCHEDULER;g->progress.resume_actor=3;
            g->progress.notice=(fd2_field_progress_notice){FD2_FIELD_PROGRESS_CLOSE,3,0,0,0,11};
            CHECK(fd2_field_game_progress_ack(g)==(error==2?-1:0) && g->experience_value==0);
            CHECK(g->units.items[3].experience==17);
            if(error!=2)CHECK(waiting(g,FD2_EVENT_COMPLETE_AI_RESULT,3)==0);
        } else {
            /* 已结束物理/奖励的loot父图继续到finish.result，不能重启drop.begin。 */
            fd2_field_loot saved={0};saved.actor_index=1;saved.action_actor=3;
            saved.resume=FD2_FIELD_PROGRESS_RESUME_AI_SCHEDULER;
            CHECK(loot_flow_resume_ai_tail(g,&saved)==(error==2?-1:0));
            if(error!=2)CHECK(waiting(g,FD2_EVENT_COMPLETE_LOOT_RESULT,3)==0);
            CHECK(g->loot.step==fd2_static_flow_label(loot_flow(),"finish.result"));
        }
        if(error==2)CHECK(g->failure!=FD2_FIELD_FAILURE_NONE && !g->event_resume.active);
        else if(error)CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ERROR)==-1 && g->failure!=FD2_FIELD_FAILURE_NONE);
        else CHECK(guarded_ack(g)==0 && !g->loot.source && !g->progress.resume);
        CHECK(!rng_calls);
    }
    free(g);return 0;
}
static int loot_children(void) {
    fd2_field_game *g=malloc(sizeof(*g)),*before=malloc(sizeof(*before));CHECK(g && before);
    static const char *const flows[]={"common.loot.child.0","common.loot.child.1"};
    for(size_t which=0;which<2;which++)for(unsigned side=0;side<3;side++)for(int full=0;full<2;full++) {
        setup(g,2);g->units.count=1;g->units.items[0].side=(uint8_t)side;
        /* 未持有source owner的既有父缓存也不能被child的no-op清空。 */
        g->loot.count=9;g->loot.cursor=7;g->loot.action_actor=3;g->loot.raw_reward=0x12345678u;
        fd2_field_loot parent=g->loot;
        for(size_t s=0;s<8;s++) {uint8_t *u=(uint8_t *)&g->units.items[0];u[10+s*2]=full?0:0x80;u[11+s*2]=full?(uint8_t)(s+1):0xa5;}
        fd2_field_unit original=g->units.items[0];
        CHECK(fd2_field_game_common_event_begin(g,flows[which],0)==0);
        int child_seen=0;
        for(unsigned budget=0;g->event_resume.active && budget<32;budget++) {
            if(g->event_resume.child_wait) {
                child_seen=1;fd2_field_loot_notice n;CHECK(fd2_field_game_loot_peek(g,&n)==1);
                if(n.kind==FD2_FIELD_LOOT_MESSAGE && !g->loot.child.text_ready) {
                    g->runtime_host_quit=1;*before=*g;
                    CHECK(fd2_field_game_loot_text_ready(g)==0 && !memcmp(g,before,sizeof(*g)));
                    g->runtime_host_quit=0;CHECK(fd2_field_game_loot_text_ready(g)==0);
                    CHECK(fd2_field_game_loot_peek(g,&n)==1);
                }
                fd2_field_loot_reply reply=(n.kind==FD2_FIELD_LOOT_CHOICE || n.kind==FD2_FIELD_LOOT_INVENTORY)?FD2_FIELD_LOOT_CANCEL:FD2_FIELD_LOOT_ACK;
                *before=*g;CHECK(fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_SLOT,9)==-1 && !memcmp(g,before,sizeof(*g)));
                g->runtime_host_quit=1;*before=*g;
                CHECK(fd2_field_game_loot_reply(g,reply,-1)==0 && !memcmp(g,before,sizeof(*g)));
                g->runtime_host_quit=0;CHECK(fd2_field_game_loot_reply(g,reply,-1)==0);
            } else {
                fd2_static_flow_node n;size_t actor;CHECK(fd2_field_game_common_event_peek(g,&n,&actor)==1);
                CHECK(!strcmp(n.op,"story") && actor==0);
                CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0);
            }
        }
        CHECK(!g->event_resume.active && !g->loot.child.active && !rng_calls);
        CHECK(!memcmp(&g->loot,&parent,sizeof(parent)));
        CHECK(child_seen==(side==2));
        if(side!=2 || full)CHECK(!memcmp(&g->units.items[0],&original,sizeof(original)));
        else CHECK(((uint8_t *)&g->units.items[0])[10]==0 && ((uint8_t *)&g->units.items[0])[11]==(which?213:211));
        *before=*g;CHECK(fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_ACK,-1)==-1 && !memcmp(g,before,sizeof(*g)));
    }
    free(g);free(before);return 0;
}
static int actual_result_graph_routes(void) {
    /* 再以正式stage12绑定/真实20765源图逐个验证四种返回owner。
     * 此处不替换结果叶；12/16/19真实结果值/文本优先级另由916差分及
     * s3_game_contract三图测试覆盖。不会将测试替身登记为shipping图。 */
    fd2_field_game *g=malloc(sizeof(*g));CHECK(g);use_result_fixture=0;
    for(int route=0;route<4;route++) {
        setup(g,1);g->units.count=59;g->units.items[1].side=0;
        for(size_t i=5;i<59;i++){g->units.items[i].side=1;g->units.items[i].hp=100;((uint8_t *)&g->units.items[i])[49]=255;}
        for(size_t i=0;i<59;i++)memset(&g->units.items[i].attack_status,0,6);
        for(size_t i=15;i<=26;i++)g->units.items[i].flags|=1;
        unsigned expected=FD2_EVENT_COMPLETE_AI_RESULT;size_t actor=0;
        if(route==0)CHECK(fd2_field_game_process_automatic_action(g)==1);
        else if(route==1) {
            apply_phase_status_tick(g,1);expected=FD2_EVENT_COMPLETE_STATUS_RESULT;
        } else if(route==2) {
            fd2_field_loot saved={0};saved.actor_index=1;saved.action_actor=3;
            saved.resume=FD2_FIELD_PROGRESS_RESUME_AI_SCHEDULER;
            CHECK(loot_flow_resume_ai_tail(g,&saved)==0);expected=FD2_EVENT_COMPLETE_LOOT_RESULT;actor=3;
        } else {
            const fd2_static_flow *f=fd2_reward_graph();int pc=fd2_static_flow_label(f,"close");CHECK(pc>=0);
            g->progress.flow_step=(uint8_t)pc;g->progress.total=g->experience_value=17;
            g->progress.resume=FD2_FIELD_PROGRESS_RESUME_AI_SCHEDULER;g->progress.resume_actor=3;
            g->progress.notice=(fd2_field_progress_notice){FD2_FIELD_PROGRESS_CLOSE,3,0,0,0,11};
            CHECK(fd2_field_game_progress_ack(g)==0 && g->units.items[3].experience==17);actor=3;
        }
        fd2_static_flow_node n;size_t observed;
        CHECK(g->event_resume.active && g->event_resume.completion==expected);
        CHECK(fd2_field_game_common_event_peek(g,&n,&observed)==1 && observed==actor && !strcmp(n.op,"story") && n.args[0]==10);
        CHECK(g->battle_result==FD2_FIELD_BATTLE_DEFEAT && !rng_calls);
        CHECK(guarded_ack(g)==0 && g->battle_result==FD2_FIELD_BATTLE_DEFEAT);
    }
    use_result_fixture=1;free(g);return 0;
}
static int imported_conditional_domain_failure(void) {
    /* 真实资源+公共snapshot Load接受64人；turn6合法早期与turn7缺raw64
     * 仍是不同查询域。拒绝放在真实读点，不能抬cap或把它伪造为胜/败。 */
    fd2_archive other={0};CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    fd2_field_game *g=calloc(1,sizeof(*g)),*before=malloc(sizeof(*before));CHECK(g && before);
    CHECK(fd2_field_game_open_resources(g,&other,18)==0);
    g->units.count=64;g->turn_number=6;
    for(size_t i=0;i<64;i++) {
        memset(&g->units.items[i],0,sizeof(g->units.items[i]));g->units.items[i].side=1;
        g->units.items[i].hp=g->units.items[i].hp_max=100;((uint8_t *)&g->units.items[i])[49]=255;
    }
    g->units.items[0].side=2;g->units.items[1].side=0;
    fd2_save_battle_snapshot snapshot={0};CHECK(fd2_field_game_export_battle_snapshot(g,&snapshot)==0);
    CHECK(fd2_field_game_import_battle_snapshot(g,&snapshot)==0);
    fd2_field_battle_result result=FD2_FIELD_BATTLE_VICTORY;*before=*g;
    CHECK(fd2_field_game_battle_result_query(g,&result)==0 && result==FD2_FIELD_BATTLE_ONGOING && !memcmp(g,before,sizeof(*g)));
    snapshot.meta[FD2_SAVE_BATTLE_META_TURN]=7;
    for(int route=0;route<7;route++) {
        CHECK(fd2_field_game_import_battle_snapshot(g,&snapshot)==0 && g->units.count==64 && g->turn_number==7);
        rng_calls=0;g->attack_rng=count_rng;g->attack_rng_userdata=&rng_calls;
        result=FD2_FIELD_BATTLE_VICTORY;*before=*g;
        CHECK(fd2_field_game_battle_result_query(g,&result)==-1 && result==FD2_FIELD_BATTLE_VICTORY && !memcmp(g,before,sizeof(*g)));
        fd2_field_units units=g->units;
        if(route==0)CHECK(fd2_field_game_complete_player_controller(g,0)==-1);
        else if(route==1) {g->active_side=1;CHECK(fd2_field_game_process_automatic_action(g)==-1);}
        else if(route==2) {g->active_side=1;apply_phase_status_tick(g,1);}
        else if(route==3)CHECK(begin_defeated_loot(g,0,0,PLAYER_ACTION_AI_PHYSICAL,0,0)==-1);
        else if(route==4) {g->active_side=1;g->turn_post_caller=FD2_FIELD_TURN_POST_END;CHECK(fd2_field_game_resume_player_controller(g)==-1);}
        else if(route==5) {g->active_side=1;g->player_post_waiting_phase=1;CHECK(fd2_field_game_resume_player_controller(g)==-1);}
        else {
            fd2_field_units output,unchanged;memset(&output,0xa5,sizeof(output));unchanged=output;
            CHECK(fd2_field_game_settlement_project_units(g,&output)==-1 && !memcmp(&output,&unchanged,sizeof(output)));
            CHECK(!memcmp(g,before,sizeof(*g)));
        }
        CHECK((route==6 || g->failure!=FD2_FIELD_FAILURE_NONE) && !g->event_resume.active && !rng_calls);
        CHECK(g->battle_result==FD2_FIELD_BATTLE_ONGOING && !memcmp(&g->units,&units,sizeof(units)));
    }
    fd2_field_game_close(g);fd2_archive_close(&other);free(g);free(before);return 0;
}
int main(int argc,char **argv) {
    int trace=argc==2 && !strcmp(argv[1],"--caller-vectors");
    if(argc>1 && !trace)return 1;
    if(cadence(trace) || ai_failure_prefix() || phase_status() || reward_and_loot() || loot_children() || actual_result_graph_routes() || imported_conditional_domain_failure())return 1;
    if(!trace)puts("S3 five-callsite/result-yield owner tests passed (test-only result leaf)");
    return 0;
}
