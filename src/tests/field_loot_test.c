/* field_loot_descriptors_execute @0x1aa1d（原sub_1AA1D） / field_special_cell_execute @0x190ac（原sub_190AC） / field_completed_chest_tiles_apply @0x12263（原sub_12263）。
 * 私有TU只驱动业务停点；不声称消息、声音或原版演出时序。 */
#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#define fd2_static_flow_lookup loot_test_flow_lookup
#define fd2_field_unit_persistent_create startup_test_persistent_create
#define fd2_field_units_append_template startup_test_append_template
#include "../field_game.c"
#undef fd2_field_units_append_template
#undef fd2_field_unit_persistent_create
#undef fd2_static_flow_lookup
#include <stdio.h>
#include "test_temp.h"
#if !defined(_WIN32)
#include <sys/stat.h>
#endif
const fd2_static_flow *fd2_static_flow_lookup(const char *domain, const char *id);
int fd2_field_unit_persistent_create(fd2_field_unit *unit, uint8_t unit_id);
int fd2_field_units_append_template(fd2_field_units *units, const fd2_field_metadata *metadata,
    const fd2_field_placements *placements, size_t template_index);
static const fd2_static_flow *loot_test_override;
static const fd2_static_flow *startup_test_override;
static const fd2_static_flow *common_test_override;
static const fd2_static_flow *defeat_test_override;
static int startup_test_stat_delta;
static int startup_test_template_hidden;
const fd2_static_flow *loot_test_flow_lookup(const char *domain, const char *id) {
    if (loot_test_override && !strcmp(domain, "loot") && !strcmp(id, loot_test_override->id)) return loot_test_override;
    if (startup_test_override && !strcmp(domain, "scene") && !strcmp(id, startup_test_override->id)) return startup_test_override;
    /* 非shipping绑定夹具：真实callback39→352ca来源图，不改生产stage表。 */
    if (common_test_override && !strcmp(domain,"common_event") && !strcmp(id,"stage.000.action.039")) return common_test_override;
    if (defeat_test_override && !strcmp(domain,"runtime") && !strcmp(id,"runtime.defeat")) return defeat_test_override;
    return fd2_static_flow_lookup(domain, id);
}
int startup_test_persistent_create(fd2_field_unit *unit, uint8_t unit_id) {
    int result = fd2_field_unit_persistent_create(unit, unit_id);
    if (!result) unit->attack = (uint16_t)(unit->attack + startup_test_stat_delta);
    return result;
}
int startup_test_append_template(fd2_field_units *units, const fd2_field_metadata *metadata,
        const fd2_field_placements *placements, size_t template_index) {
    int result = fd2_field_units_append_template(units, metadata, placements, template_index);
    if (!result && startup_test_template_hidden)
        fd2_field_unit_set_hidden(&units->items[units->count - 1u], 1);
    return result;
}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"CHECK %s:%d %s\n",__FILE__,__LINE__,#x);return -1;}}while(0)
static uint32_t rng(void *ctx){(*(unsigned*)ctx)++;return 0;}
static int fixture(fd2_field_game *g,fd2_archive *ar,unsigned *calls){
    CHECK(fd2_archive_open(ar,"original_game/FDOTHER.DAT")==0);
    CHECK(fd2_field_game_open_resources(g,ar,0)==0);
    CHECK(fd2_field_units_set(&g->units,0,0,0,2,7,14)==0);
    CHECK(fd2_field_unit_persistent_create(&g->units.items[0],0)==0);
    CHECK(fd2_field_units_set(&g->units,1,9,9,2,8,14)==0);
    CHECK(fd2_field_unit_persistent_create(&g->units.items[1],9)==0);
    g->selected_unit=0;g->interaction=FD2_FIELD_INTERACTION_COMMAND;g->attack_rng=rng;g->attack_rng_userdata=calls;
    g->move_path_length=1;g->experience_value=0;g->resource_value=5;
    return 0;
}
/* 模拟文字成功停点；不模拟SDL或原版字速/等键RNG。 */
static int reply(fd2_field_game *game, fd2_field_loot_reply response, int slot) {
    if (game->loot.notice.kind == FD2_FIELD_LOOT_MESSAGE && !game->loot.text_ready &&
        fd2_field_game_loot_text_ready(game) != 0) return -1;
    return fd2_field_game_loot_reply(game, response, slot);
}
static int add_dead(fd2_field_game*g,size_t i,uint8_t mode,uint16_t value){
    CHECK(fd2_field_units_set(&g->units,i,96,96,0,9,14)==0);
    fd2_field_unit*u=&g->units.items[i];u->hp=0;u->hp_max=20;
    uint8_t*r=(uint8_t*)u;r[0x31]=mode;r[0x32]=(uint8_t)value;r[0x33]=(uint8_t)(value>>8);
    return 0;
}
typedef struct {
    unsigned count, quit_at;
    uint32_t final_resource;
    size_t dead;
    int check_child, defeat_refresh;
} outer_refresh_log;
static int check_outer_refresh(fd2_field_game *g,fd2_field_runtime_present_step step,
        int value,void *userdata) {
    outer_refresh_log *log=userdata;
    if(step>=FD2_FIELD_RUNTIME_DEFEAT_BEGIN) {
        if(step==FD2_FIELD_RUNTIME_DEFEAT_CLOSE)log->defeat_refresh=1;
        return 0;
    }
    if(step==FD2_FIELD_RUNTIME_REFRESH && log->defeat_refresh) {log->defeat_refresh=0;return 0;}
    CHECK(step==FD2_FIELD_RUNTIME_REFRESH && value==0 && log->count<2);
    log->count++;
    CHECK(!g->progress.notice.kind && !g->event_resume.active && !g->loot.child.active);
    CHECK(!g->loot.source && !g->loot.count && !g->loot.notice.kind);
    CHECK(fd2_field_unit_is_hidden(&g->units.items[log->dead])==(log->count==2));
    CHECK(g->resource_value==(log->count==1 ? 5u:log->final_resource));
    if(log->check_child && log->count==2) {
        int found=0;
        const uint8_t *raw=(const uint8_t *)&g->units.items[1];
        for(unsigned i=0;i<8;i++) if(!(raw[10+i*2]&0x80u) && raw[11+i*2]==211) found++;
        CHECK(found==1);
    }
    return log->quit_at==log->count ? 1:0;
}
typedef struct {
    unsigned calls,rotations,actors,projects,waits,frames,closes,quit_at,fail_at;
    fd2_field_unit initial[5],halted[5];
    int stopped;
} defeat_trace;
static void defeat_fixture(fd2_field_game *g,defeat_trace *t) {
    memset(g,0,sizeof(*g));g->ready=1;g->units.count=5;
    for(size_t i=0;i<5;i++) {
        g->units.items[i].x=g->units.items[i].y=1;
        g->units.items[i].direction=(uint8_t)(i%3+1);
        g->units.items[i].flags=0x40;
    }
    g->units.items[1].hp=10;g->units.items[2].flags=0x81;
    g->units.items[3].x=g->units.items[3].y=200; /* 离屏死者仍逐raw旋转。 */
    memcpy(t->initial,g->units.items,sizeof(t->initial));
}
static int defeat_present(fd2_field_game *g,fd2_field_runtime_present_step step,
        int value,void *userdata) {
    defeat_trace *t=userdata;t->calls++;
    if(t->stopped) {
        CHECK(step==FD2_FIELD_RUNTIME_DEFEAT_CLOSE && !memcmp(t->halted,g->units.items,sizeof(t->halted)));
        t->closes++;return 0;
    }
    if(step==FD2_FIELD_RUNTIME_DEFEAT_BEGIN) {
        CHECK(t->calls==1 && !memcmp(t->initial,g->units.items,sizeof(t->initial)));
    } else if(step==FD2_FIELD_RUNTIME_DEFEAT_ROTATE_PAGE) {
        CHECK(t->rotations<13 && t->projects==t->rotations);t->rotations++;t->actors=0;
    } else if(step==FD2_FIELD_RUNTIME_DEFEAT_ROTATE_ACTOR) {
        const int expected[]={0,1,3,4};CHECK(t->actors<4 && value==expected[t->actors++]);
        CHECK(t->rotations>=1 && t->rotations<=13);
        for(unsigned i=0;i<5;i++) {
            const fd2_field_unit *u=&g->units.items[i];
            unsigned direction=t->initial[i].direction;
            if(i!=1 && i!=2)direction=i<=(unsigned)value ? (t->rotations-1)%4:
                t->rotations>1 ? (t->rotations-2)%4:t->initial[i].direction;
            CHECK(u->direction==direction && u->flags==t->initial[i].flags);
        }
    } else if(step==FD2_FIELD_RUNTIME_DEFEAT_ROTATE_PROJECT) {
        CHECK(t->actors==4 && t->projects+1==t->rotations);t->projects++;
    } else if(step==FD2_FIELD_RUNTIME_DEFEAT_WAIT) {
        CHECK(value==1);t->waits++;
    } else if(step==FD2_FIELD_RUNTIME_DEFEAT_BASE || step==FD2_FIELD_RUNTIME_DEFEAT_CUE ||
              step==FD2_FIELD_RUNTIME_DEFEAT_FRAME || step==FD2_FIELD_RUNTIME_REFRESH) {
        CHECK(t->rotations==13 && t->projects==13);
        for(unsigned i=0;i<5;i++) CHECK(g->units.items[i].flags==(i==1?t->initial[i].flags:1));
        CHECK(g->units.items[2].direction==t->initial[2].direction &&
            g->units.items[1].direction==t->initial[1].direction);
        if(step==FD2_FIELD_RUNTIME_DEFEAT_FRAME)CHECK(value==(int)(68+t->frames++));
        if(step==FD2_FIELD_RUNTIME_REFRESH)CHECK(t->closes==1 && t->frames==12 && t->waits==25);
    } else if(step==FD2_FIELD_RUNTIME_DEFEAT_CLOSE) {
        CHECK(t->frames==12 && t->waits==25);t->closes++;
    } else CHECK(0);
    if(t->calls==t->quit_at || t->calls==t->fail_at) {
        memcpy(t->halted,g->units.items,sizeof(t->halted));t->stopped=1;
        return t->calls==t->quit_at ? 1:-1;
    }
    return 0;
}
static int test_defeat_exact_write_order_and_cancellation(void) {
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);defeat_trace t={0};defeat_fixture(g,&t);
    g->runtime_present=defeat_present;g->runtime_present_userdata=&t;
    CHECK(finalize_zero_hp_units(g)==0 && t.rotations==13 && t.frames==12 && t.closes==1 && !g->runtime_host_quit);
    unsigned boundaries=t.calls;CHECK(boundaries>100);
    for(unsigned stop=1;stop<=boundaries;stop++) {
        t=(defeat_trace){.quit_at=stop};defeat_fixture(g,&t);
        g->runtime_present=defeat_present;g->runtime_present_userdata=&t;
        CHECK(finalize_zero_hp_units(g)==1 && g->runtime_host_quit && t.stopped && t.closes==1 &&
            !g->failure && !memcmp(t.halted,g->units.items,sizeof(t.halted)));
        CHECK(finalize_zero_hp_units(g)==1 && !memcmp(t.halted,g->units.items,sizeof(t.halted)));
    }
    for(unsigned stop=1;stop<=boundaries;stop+=17) {
        t=(defeat_trace){.fail_at=stop};defeat_fixture(g,&t);
        g->runtime_present=defeat_present;g->runtime_present_userdata=&t;
        CHECK(finalize_zero_hp_units(g)==-1 && !g->runtime_host_quit && t.closes==1 &&
            !memcmp(t.halted,g->units.items,sizeof(t.halted)));
    }
    /* 没有屏内见证者时保全部方向，只精确写HP0 flags1，不调任何呈现。 */
    t=(defeat_trace){0};defeat_fixture(g,&t);g->camera_cell_x=g->camera_cell_y=100;
    g->runtime_present=defeat_present;g->runtime_present_userdata=&t;
    CHECK(finalize_zero_hp_units(g)==0 && !t.calls);
    for(unsigned i=0;i<5;i++)CHECK(g->units.items[i].direction==t.initial[i].direction &&
        g->units.items[i].flags==(i==1?t.initial[i].flags:1));
    /* 无callback只跑同图热写，终态不依赖呈现注册。 */
    t=(defeat_trace){0};defeat_fixture(g,&t);CHECK(finalize_zero_hp_units(g)==0);
    CHECK(g->units.items[0].direction==0 && g->units.items[3].direction==0 &&
        g->units.items[2].direction==t.initial[2].direction);
    const fd2_static_flow *flow=fd2_static_flow_lookup("runtime","runtime.defeat");CHECK(flow);
    fd2_static_flow bad=*flow;fd2_static_flow_node *nodes=malloc(flow->count*sizeof(*nodes));CHECK(nodes);
    memcpy(nodes,flow->nodes,flow->count*sizeof(*nodes));bad.nodes=nodes;nodes[flow->entry].op="unknown";
    defeat_test_override=&bad;t=(defeat_trace){0};defeat_fixture(g,&t);
    CHECK(finalize_zero_hp_units(g)==-1 && !memcmp(t.initial,g->units.items,sizeof(t.initial)));
    defeat_test_override=NULL;free(nodes);free(g);return 0;
}
static int test_defeat_three_callers_stop_at_host_quit(void) {
    for(unsigned caller=0;caller<3;caller++) for(int error=0;error<2;error++) {
        fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);
        defeat_trace t={.quit_at=error?0u:3u,.fail_at=error?3u:0u};defeat_fixture(g,&t);
        g->runtime_present=defeat_present;g->runtime_present_userdata=&t;
        if(caller==0) {
            /* 只进collect.finalize，原pending尚未commit，停止不能跑result/descriptor。 */
            fd2_loot_flow_context ctx={0};ctx.game=g;ctx.actor=0;
            CHECK(loot_flow_run(&ctx,fd2_static_flow_label(loot_flow(),"collect.finalize"))==(error?-1:0));
            CHECK(!ctx.started && !g->loot.source && !g->loot.notice.kind);
        } else if(caller==1) {
            const fd2_static_flow *flow=fd2_static_flow_lookup("progress","phase.status");CHECK(flow);
            g->progress.flow_step=(uint8_t)fd2_static_flow_label(flow,"finalize");
            g->progress.status_pass=77;
            CHECK(run_status_flow(g,&g->progress,&g->units,0)==(error?-1:0) && g->progress.status_pass==77 &&
                g->progress.flow_step==(uint8_t)fd2_static_flow_label(flow,"finalize"));
        } else {
            const fd2_static_flow *flow=fd2_static_flow_lookup("common_event","common.defeat.tail");CHECK(flow);
            int pc=fd2_static_flow_label(flow,"finalize");
            g->event_resume=(fd2_field_event_resume){.flow=flow,.pc=pc,.actor=0,.active=1};
            CHECK(common_event_run(g)==(error?-1:0) && g->event_resume.active);
            if(!error)CHECK(g->event_resume.pc==pc);
        }
        CHECK(g->runtime_host_quit==!error && (!g->failure || error) && t.closes==1 &&
            !memcmp(t.halted,g->units.items,sizeof(t.halted)));
        free(g);
    }
    return 0;
}
/* 原1548e的两条路径都走collect→refresh0→finalize→descriptor→refresh0→reward。
 * 实际冷图／Game owner测试，不代替WGC或原版时钟对照。 */
static int test_ai_physical_outer_refresh(void) {
    for(int mode=0;mode<2;mode++) for(int descriptor=0;descriptor<2;descriptor++)
    for(unsigned quit=0;quit<3;quit++) {
        fd2_field_game *g=calloc(1,sizeof(*g));fd2_archive ar;unsigned calls=0;
        CHECK(g && fixture(g,&ar,&calls)==0);
        CHECK(add_dead(g,2,descriptor?1:255,25)==0);
        g->option_values[2]=(uint8_t)mode;
        /* 走真实exchange wrapper，固定两方为合法零伤害以只核外围尾序。 */
        g->units.items[0].attack=g->units.items[1].attack=0;
        g->units.items[0].accuracy=g->units.items[1].accuracy=0;
        outer_refresh_log log={0,quit,descriptor?30u:5u,2,0};
        g->runtime_present=check_outer_refresh;g->runtime_present_userdata=&log;
        CHECK(resolve_physical_exchange(g,0,1,0)==0 && log.count== (quit==1?1u:descriptor?1u:2u));
        CHECK(!g->failure);
        if(quit==1) {
            CHECK(g->runtime_host_quit && !g->loot.source && !g->loot.notice.kind &&
                !fd2_field_unit_is_hidden(&g->units.items[2]) && g->resource_value==5 && !g->progress.notice.kind);
        } else if(descriptor) {
            CHECK(g->loot.notice.fragment==435 && g->resource_value==5);
            CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && log.count==2);
        }
        CHECK(g->runtime_host_quit==(quit!=0));
        CHECK(!g->progress.notice.kind && !g->loot.notice.kind && !g->loot.source);
        CHECK(g->resource_value==(quit==1?5u:log.final_resource));
        fd2_field_game_close(g);fd2_archive_close(&ar);free(g);
    }
    /* 首／次刷新宿主退出不能启动奖励；正奖励也必须等第二次刷新返回。 */
    for(unsigned quit=0;quit<3;quit++) {
        fd2_field_game *g=calloc(1,sizeof(*g));fd2_archive ar;unsigned calls=0;
        CHECK(g && fixture(g,&ar,&calls)==0 && add_dead(g,2,1,25)==0);
        outer_refresh_log log={0,quit,30,2,0};
        g->runtime_present=check_outer_refresh;g->runtime_present_userdata=&log;g->experience_value=5;
        CHECK(begin_defeated_loot(g,1,0,PLAYER_ACTION_AI_PHYSICAL,0,0)==(quit==1?0:1));
        if(quit!=1)CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && log.count==2);
        CHECK(g->runtime_host_quit==(quit!=0) && !g->failure && !calls);
        CHECK((g->progress.notice.kind!=FD2_FIELD_PROGRESS_NONE)==(quit==0));
        CHECK(g->resource_value==(quit==1?5u:30u));
        fd2_field_game_close(g);fd2_archive_close(&ar);free(g);
    }
    for(int kind=PLAYER_ACTION_AI_MAGIC;kind<=PLAYER_ACTION_AI_ITEM;kind++)
    for(int descriptor=0;descriptor<2;descriptor++) {
        fd2_field_game *g=calloc(1,sizeof(*g));fd2_archive ar;unsigned calls=0;
        CHECK(g && fixture(g,&ar,&calls)==0 && add_dead(g,2,descriptor?1:255,25)==0);
        outer_refresh_log log={0,0,30,2,0};
        g->runtime_present=check_outer_refresh;g->runtime_present_userdata=&log;
        CHECK(begin_defeated_loot(g,1,0,(player_action_kind)kind,0,0)==descriptor);
        if(descriptor)CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
        CHECK(!log.count && !g->runtime_host_quit && !g->failure && !calls);
        fd2_field_game_close(g);fd2_archive_close(&ar);free(g);
    }
    return 0;
}
static int test_defeated_item_and_money(void){
    fd2_field_game*g=calloc(1,sizeof(*g));fd2_archive ar;unsigned calls=0;CHECK(fixture(g,&ar,&calls)==0);
    CHECK(add_dead(g,2,0,192)==0);CHECK(add_dead(g,3,1,1000)==0);
    uint16_t attack=g->units.items[0].attack;
    CHECK(begin_defeated_loot(g,0,0,PLAYER_ACTION_ITEM,0,0)==1);
    CHECK(g->loot.notice.fragment==432 && g->loot.notice.wait_key && ((uint8_t*)&g->units.items[0])[0x10]==0x80);
    CHECK(fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_ACK,-1)==-1);
    CHECK(fd2_field_game_loot_text_ready(g)==0 && ((uint8_t*)&g->units.items[0])[0x11]==192);
    fd2_field_unit committed=g->units.items[0];
    CHECK(fd2_field_game_loot_text_ready(g)==-1 && !memcmp(&committed,&g->units.items[0],80));
    CHECK(g->units.items[2].flags==1 && !fd2_field_unit_has_acted(&g->units.items[0]));
    CHECK(fd2_field_game_end_player_turn(g)==-1 && fd2_field_game_move_cursor(g,1,0)==0);
    CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
    CHECK(g->loot.notice.fragment==435 && g->message_number==1000 && g->resource_value==5);
    CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
    CHECK(g->loot.notice.kind==0 && g->resource_value==1005 && fd2_field_unit_has_acted(&g->units.items[0]));
    CHECK(g->units.items[0].attack==attack && calls==0);
    CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==-1);
    fd2_field_game_close(g);fd2_archive_close(&ar);free(g);return 0;
}
static int test_full_swap_and_no(void){
    for(int accept=0;accept<2;accept++){
        fd2_field_game*g=calloc(1,sizeof(*g));fd2_archive ar;unsigned calls=0;CHECK(fixture(g,&ar,&calls)==0);
        uint8_t*r=(uint8_t*)&g->units.items[0];for(size_t i=0;i<8;i++){r[10+i*2]=0;r[11+i*2]=(uint8_t)i;}
        fd2_field_unit before=g->units.items[0];CHECK(add_dead(g,2,0,192)==0);
        CHECK(begin_defeated_loot(g,0,0,PLAYER_ACTION_ITEM,0,0)==1);
        CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && g->loot.notice.wait_ms==100);
        CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && g->loot.notice.fragment==433);
        CHECK(reply(g,accept?FD2_FIELD_LOOT_YES:FD2_FIELD_LOOT_NO,-1)==0);
        if(accept){CHECK(g->loot.notice.kind==FD2_FIELD_LOOT_INVENTORY);
            CHECK(reply(g,FD2_FIELD_LOOT_SLOT,3)==0);CHECK(r[25]==192 && r[24]==0);
            for(size_t i=0;i<7;i++)CHECK(r[11+i*2]==(i<3?i:i+1));
        }else{CHECK(g->loot.notice.fragment==434 && !g->loot.notice.wait_key && g->loot.notice.wait_ms==200);
            CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
            before.flags|=0x80;CHECK(!memcmp(&before,&g->units.items[0],sizeof(before)));}
        CHECK(calls==0);fd2_field_game_close(g);fd2_archive_close(&ar);free(g);
    }return 0;
}
static int test_callback_and_story(void){
    fd2_field_game*g=calloc(1,sizeof(*g));fd2_archive ar;unsigned calls=0;CHECK(fixture(g,&ar,&calls)==0);
    CHECK(add_dead(g,2,2,4)==0);
    for(size_t i=3;i<14;i++){CHECK(fd2_field_units_set(&g->units,i,0,0,2,1,1)==0);g->units.items[i].hp=1;((uint8_t*)&g->units.items[i])[0x31]=0xff;}
    CHECK(add_dead(g,14,3,8)==0);
    CHECK(begin_defeated_loot(g,0,0,PLAYER_ACTION_ITEM,0,0)==1 && g->loot.notice.wait_ms==200);
    CHECK(g->units.items[13].side==2);
    CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
    CHECK(g->units.items[13].side==1 && g->event_resume.active && !g->event_log_count);
    fd2_field_loot_notice hidden_notice;
    CHECK(fd2_field_game_loot_peek(g,&hidden_notice)==0); /* 父PC等待时不重发旧notice。 */
    fd2_static_flow_node node;size_t actor;
    CHECK(fd2_field_game_common_event_peek(g,&node,&actor)==1 && !strcmp(node.op,"story") && node.args[0]==7);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0 && !g->event_resume.active);
    CHECK(g->loot.notice.kind==FD2_FIELD_LOOT_STORY && g->loot.notice.fragment==8);
    CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && calls==0);
    fd2_field_game_close(g);fd2_archive_close(&ar);free(g);return 0;
}
static int common_resource_calls,common_resource_reject;
static int common_resource_check(const fd2_field_game *g,const fd2_static_flow_node *node,const fd2_field_event_resource_view *view,void *ctx) {
    (void)view;
    (void)g;(void)ctx;common_resource_calls++;
    return common_resource_reject || strcmp(node->op,"story") || node->args[0]!=11 ? -1:0;
}
typedef struct { unsigned calls,info_calls,clear_calls;int quit,error; } cell_cue_trace;
static int cell_cue_present(fd2_field_game *g,fd2_field_runtime_present_step step,int value,void *userdata) {
    cell_cue_trace *t=userdata;(void)g;
    /* 13565正常返回的独立有限叶；不是cue，也不能继承cue注入的故障。 */
    if(step==FD2_FIELD_RUNTIME_INFO_VISIBLE) {
        CHECK(value==1);t->info_calls++;return 0;
    }
    if(step==FD2_FIELD_RUNTIME_CLEAR_KEYS) {
        CHECK(value==0);t->clear_calls++;return 0;
    }
    CHECK(step==FD2_FIELD_RUNTIME_CELL_OPEN_CUE && value==12);t->calls++;
    return t->error?-1:t->quit?1:0;
}
static int test_cross_stage_cell_resources_and_cancel(void) {
    const size_t stages[]={2,6};const uint8_t x[]={14,17},y[]={25,10},modes[]={0,1};
    const uint16_t values[]={45,5000};
    for(unsigned scenario=0;scenario<2;scenario++) {
        fd2_field_game *g=calloc(1,sizeof(*g)),*before=malloc(sizeof(*before));fd2_archive ar;unsigned calls=0;
        CHECK(g && before && fixture(g,&ar,&calls)==0);
        /* 窄机制夹具只借原资源，不冒称已执行本关正式entry/轨迹。 */
        fd2_field_map_close(&g->map);fd2_field_metadata_close(&g->metadata);fd2_terrain_tileset_close(&g->terrain);
        CHECK(fd2_field_map_open_stage(&g->map,&g->field_archive,stages[scenario])==0 &&
            fd2_field_metadata_open_stage(&g->metadata,&g->field_archive,stages[scenario])==0 &&
            fd2_terrain_tileset_open_stage(&g->terrain,&g->fdshap_archive,&g->field_archive,stages[scenario])==0);
        g->stage=stages[scenario];g->units.items[0].x=x[scenario];g->units.items[0].y=y[scenario];
        uint32_t cell=fd2_field_map_cell(&g->map,x[scenario],y[scenario]);unsigned index=cell>>16&31;
        CHECK(index<16 && g->metadata.cell_actions[index].mode==modes[scenario] &&
            g->metadata.cell_actions[index].param==values[scenario]);
        cell_cue_trace trace={0};g->runtime_present=cell_cue_present;g->runtime_present_userdata=&trace;
        *before=*g;CHECK(loot_flow_preflight_cell(g,0)==0 && !memcmp(before,g,sizeof(*g)) && !calls && !trace.calls);
        CHECK(fd2_field_stage_battle_is_supported(g->stage) && begin_wait_cell(g,0)==1);
        CHECK(g->loot.notice.kind==FD2_FIELD_LOOT_CHOICE);
        CHECK(reply(g,FD2_FIELD_LOOT_CANCEL,-1)==0 && g->loot.notice.wait_ms==100);
        CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && g->loot.notice.fragment==412 && g->loot.notice.wait_ms==200);
        CHECK(fd2_field_game_loot_text_ready(g)==0 && g->loot.text_ready && !trace.calls && !calls &&
            !g->cell_action_completed[index] && g->resource_value==before->resource_value &&
            !memcmp(&g->units,&before->units,sizeof(g->units)) && fd2_field_map_cell(&g->map,x[scenario],y[scenario])==cell);
        /* 尚有真实loot等待，已发布battle也不能越过它直接进controller。 */
        CHECK(fd2_field_game_complete_player_controller(g,0)==-1);
        fd2_field_game_close(g);fd2_archive_close(&ar);free(before);free(g);
    }
    return 0;
}
static int test_cell_callback_full_word_cold_order(void) {
    const uint8_t modes[]={2,3,255};
    for(unsigned variant=0;variant<3;variant++) for(int cancel=0;cancel<2;cancel++) {
        fd2_field_game *g=calloc(1,sizeof(*g)),*before=malloc(sizeof(*before));fd2_archive ar;unsigned calls=0;
        CHECK(g && before && fixture(g,&ar,&calls)==0);
        g->units.items[0].x=7;g->units.items[0].y=13;
        g->metadata.cell_actions[0].mode=modes[variant];g->metadata.cell_actions[0].param=39;
        common_test_override=fd2_static_flow_lookup("common_event","common.loot.child.0");CHECK(common_test_override);
        common_resource_reject=0;g->event_preflight=common_resource_check;
        cell_cue_trace trace={0};g->runtime_present=cell_cue_present;g->runtime_present_userdata=&trace;
        *before=*g;CHECK(loot_flow_preflight_cell(g,0)==0 && !memcmp(g,before,sizeof(*g)) && !trace.calls);
        CHECK(begin_wait_cell(g,0)==1 && g->loot.notice.fragment==421);
        CHECK(reply(g,cancel?FD2_FIELD_LOOT_CANCEL:FD2_FIELD_LOOT_YES,-1)==0);
        if(cancel) {
            CHECK(!trace.calls && g->loot.notice.wait_ms==100);
            CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && g->loot.notice.fragment==412);
            CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && !g->loot.source && !g->event_resume.active);
        } else {
            CHECK(trace.calls==1 && g->loot.notice.wait_ms==300 && !g->loot.notice.close_box && !g->event_resume.active);
            CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && !g->loot.notice.close_box && g->loot.notice.wait_ms==200);
            CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && g->loot.notice.close_box && !g->loot.notice.wait_ms);
            CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && g->event_resume.child_wait && g->event_resume.actor==0);
            CHECK(fd2_field_game_loot_text_ready(g)==0 && fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
            CHECK(g->event_resume.active && !g->event_resume.child_wait);
            CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0 && !g->event_resume.active && !g->loot.source);
        }
        CHECK(trace.calls==(unsigned)!cancel && !g->cell_action_completed[0] && !g->event_log_count &&
            !g->failure && !calls && g->resource_value==5 && fd2_field_unit_has_acted(&g->units.items[0]));
        /* actor1仍可行动，13565未进整轮；119a6仅clear，不借caller_return恢复51AAC。 */
        CHECK(!trace.info_calls && trace.clear_calls==1 && !fd2_field_unit_has_acted(&g->units.items[1]) &&
            !g->player_post_waiting_phase && g->turn_post_caller==FD2_FIELD_TURN_POST_NONE);
        CHECK(fd2_field_unit_inventory_count(&g->units.items[0])==
            fd2_field_unit_inventory_count(&before->units.items[0])+(cancel?0:1));
        CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==-1 && trace.calls==(unsigned)!cancel &&
            !trace.info_calls && trace.clear_calls==1);
        common_test_override=NULL;fd2_field_game_close(g);fd2_archive_close(&ar);free(before);free(g);
    }
    for(int failure=0;failure<4;failure++) {
        fd2_field_game *g=calloc(1,sizeof(*g)),*before=malloc(sizeof(*before));fd2_archive ar;unsigned calls=0;
        CHECK(g && before && fixture(g,&ar,&calls)==0);g->units.items[0].x=7;g->units.items[0].y=13;
        g->metadata.cell_actions[0].mode=2;g->metadata.cell_actions[0].param=failure?0x127:0;
        common_test_override=fd2_static_flow_lookup("common_event","common.loot.child.0");
        common_resource_reject=0;g->event_preflight=common_resource_check;
        cell_cue_trace trace={0};g->runtime_present=cell_cue_present;g->runtime_present_userdata=&trace;
        if(failure<2) {
            *before=*g;CHECK(loot_flow_preflight_cell(g,0)==-1 && !memcmp(g,before,sizeof(*g)) && !trace.calls && !calls);
        } else {
            g->metadata.cell_actions[0].param=39;trace.quit=failure==2;trace.error=failure==3;
            CHECK(loot_flow_preflight_cell(g,0)==0 && begin_wait_cell(g,0)==1);
            fd2_field_units units=g->units;
            CHECK(reply(g,FD2_FIELD_LOOT_YES,-1)==(failure==2?0:-1) && trace.calls==1 &&
                g->runtime_host_quit==(failure==2) && !g->event_resume.active &&
                !memcmp(&g->units,&units,sizeof(units)) && !g->cell_action_completed[0]);
            CHECK(g->failure==(failure==3?FD2_FIELD_FAILURE_ACTION:FD2_FIELD_FAILURE_NONE));
        }
        common_test_override=NULL;fd2_field_game_close(g);fd2_archive_close(&ar);free(before);free(g);
    }
    return 0;
}
static int test_callback_late_bad_word_keeps_committed_prefix(void) {
    for(int cell=0;cell<2;cell++) {
        fd2_field_game *g=calloc(1,sizeof(*g));fd2_archive ar;unsigned calls=0;
        CHECK(g && fixture(g,&ar,&calls)==0);
        common_test_override=fd2_static_flow_lookup("common_event","common.loot.child.0");CHECK(common_test_override);
        common_resource_reject=0;g->event_preflight=common_resource_check;
        cell_cue_trace trace={0};g->runtime_present=cell_cue_present;g->runtime_present_userdata=&trace;
        if(cell) {
            g->units.items[0].x=7;g->units.items[0].y=13;
            g->metadata.cell_actions[0].mode=2;g->metadata.cell_actions[0].param=39;
            CHECK(loot_flow_preflight_cell(g,0)==0 && begin_wait_cell(g,0)==1);
            CHECK(reply(g,FD2_FIELD_LOOT_YES,-1)==0 && trace.calls==1 && g->loot.notice.wait_ms==300);
            CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && g->loot.notice.wait_ms==200);
            CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && g->loot.notice.close_box);
        } else {
            CHECK(add_dead(g,2,2,39)==0);
            CHECK(begin_defeated_loot(g,1,0,PLAYER_ACTION_ITEM,0,0)==1 && g->loot.notice.wait_ms==200 &&
                g->units.items[2].flags==1);
        }
        g->loot.descriptors[g->loot.cursor][2]=1; /* 已预检39，晚坏成WORD0x0127。 */
        fd2_field_units committed=g->units;uint32_t resource=g->resource_value;
        CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==-1 && g->failure==FD2_FIELD_FAILURE_ACTION &&
            !g->event_resume.active && !g->event_log_count && !calls &&
            !memcmp(&committed,&g->units,sizeof(committed)) && g->resource_value==resource &&
            !g->cell_action_completed[0] && g->loot.cursor==0 && g->loot.descriptors[0][2]==1 &&
            trace.calls==(unsigned)cell);
        common_test_override=NULL;fd2_field_game_close(g);fd2_archive_close(&ar);free(g);
    }
    return 0;
}
static int test_common_callback_preflight_and_parent_child(void) {
    const fd2_static_flow *source=fd2_static_flow_lookup("common_event","common.loot.child.0");CHECK(source);
    for(int full=0;full<2;full++) {
        fd2_field_game *g=calloc(1,sizeof(*g)),*before=malloc(sizeof(*before));
        fd2_archive ar;unsigned calls=0;CHECK(g && before && fixture(g,&ar,&calls)==0);
        common_test_override=source;common_resource_calls=common_resource_reject=0;
        g->event_preflight=common_resource_check;
        g->event_log_count=FD2_FIELD_EVENT_LOG_CAPACITY;
        uint8_t descriptor[]={2,39,0};*before=*g;
        CHECK(loot_descriptor_preflight(g,1,descriptor)==0 && common_resource_calls==1 && calls==0);
        CHECK(!memcmp(g,before,sizeof(*g))); /* caller1/count2/full legacy log合法，纯预检不写。 */
        common_resource_reject=1;
        CHECK(loot_descriptor_preflight(g,1,descriptor)==-1 && !memcmp(g,before,sizeof(*g)) && !calls);
        common_resource_reject=0;common_test_override=NULL;
        CHECK(loot_descriptor_preflight(g,1,descriptor)==-1 && !memcmp(g,before,sizeof(*g)));
        common_test_override=source;descriptor[2]=1;
        CHECK(loot_descriptor_preflight(g,1,descriptor)==-1 && !memcmp(g,before,sizeof(*g)));
        descriptor[2]=0;
        uint8_t *raw=(uint8_t *)&g->units.items[1];
        if(full)for(unsigned i=0;i<8;i++){raw[10+i*2]=0;raw[11+i*2]=(uint8_t)i;}
        fd2_field_unit original=g->units.items[1];
        CHECK(add_dead(g,2,2,39)==0 && add_dead(g,3,1,25)==0);
        CHECK(begin_defeated_loot(g,1,0,PLAYER_ACTION_ITEM,0,0)==1 && g->loot.count==2);
        CHECK(g->loot.notice.kind==FD2_FIELD_LOOT_DELAY && reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
        CHECK(g->event_resume.active && g->event_resume.from_loot==1 && g->event_resume.actor==1 &&
            g->event_resume.child_wait && g->loot.child.actor_index==1 && g->loot.cursor==0);
        uint8_t parent[sizeof(g->loot.descriptors)];memcpy(parent,g->loot.descriptors,sizeof(parent));
        fd2_field_loot_notice notice;
        CHECK(fd2_field_game_loot_peek(g,&notice)==1 && notice.fragment==432 && notice.item_id==211);
        CHECK(fd2_field_game_loot_text_ready(g)==0 && fd2_field_game_loot_text_ready(g)==-1);
        CHECK(fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
        if(full) {
            CHECK(fd2_field_game_loot_peek(g,&notice)==1 && notice.kind==FD2_FIELD_LOOT_DELAY);
            CHECK(fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
            CHECK(fd2_field_game_loot_peek(g,&notice)==1 && notice.kind==FD2_FIELD_LOOT_CHOICE);
            CHECK(fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_CANCEL,-1)==0);
            CHECK(fd2_field_game_loot_text_ready(g)==0 && fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
            CHECK(!memcmp(&original,&g->units.items[1],sizeof(original)));
        } else CHECK(fd2_field_unit_inventory_count(&g->units.items[1])==
            fd2_field_unit_inventory_count(&original)+1);
        fd2_static_flow_node node;size_t actor;
        CHECK(!g->event_resume.child_wait && fd2_field_game_common_event_peek(g,&node,&actor)==1 &&
            actor==1 && !strcmp(node.op,"story") && node.args[0]==11);
        CHECK(g->loot.count==2 && !g->loot.cursor && !memcmp(parent,g->loot.descriptors,sizeof(parent)));
        CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0 && !g->event_resume.active);
        /* common已呈现，不再emit legacy FIELD_EVENT；父直接到下一money。 */
        CHECK(g->loot.cursor==1 && g->loot.notice.kind==FD2_FIELD_LOOT_MESSAGE && g->loot.notice.fragment==435);
        CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==-1 && g->resource_value==5);
        CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && g->resource_value==30 && !g->loot.source && !calls);
        CHECK(g->event_log_count==FD2_FIELD_EVENT_LOG_CAPACITY && !g->failure);
        CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==-1 && g->resource_value==30);
        common_test_override=NULL;fd2_field_game_close(g);fd2_archive_close(&ar);free(g);free(before);
    }
    return 0;
}
static int child_host_save_identity(const char *path,uint8_t bytes[FD2_SAVE_FILE_SIZE],uint64_t identity[2]) {
    FILE *file=fopen(path,"rb");if(!file)return -1;
    int read_ok=fread(bytes,1,FD2_SAVE_FILE_SIZE,file)==FD2_SAVE_FILE_SIZE && fgetc(file)==EOF;
    if(fclose(file)!=0 || !read_ok)return -1;
#if defined(_WIN32)
    WIN32_FILE_ATTRIBUTE_DATA info;
    if(!GetFileAttributesExA(path,GetFileExInfoStandard,&info))return -1;
    identity[0]=((uint64_t)info.nFileSizeHigh<<32)|info.nFileSizeLow;
    identity[1]=((uint64_t)info.ftLastWriteTime.dwHighDateTime<<32)|info.ftLastWriteTime.dwLowDateTime;
#else
    struct stat info;if(stat(path,&info)!=0)return -1;
    identity[0]=(uint64_t)info.st_size;
    identity[1]=(uint64_t)info.st_mtim.tv_sec*UINT64_C(1000000000)+(uint64_t)info.st_mtim.tv_nsec;
#endif
    return 0;
}
static int test_common_child_host_quit_freezes_all_owners(void) {
    const fd2_static_flow *source=fd2_static_flow_lookup("common_event","common.loot.child.0");CHECK(source);
    for(int stop=0;stop<3;stop++)for(int error=0;error<2;error++) {
        fd2_field_game *g=calloc(1,sizeof(*g)),*before=malloc(sizeof(*before));
        fd2_archive ar;unsigned calls=0;CHECK(g && before && fixture(g,&ar,&calls)==0);
        common_test_override=source;common_resource_reject=0;g->event_preflight=common_resource_check;
        if(stop==2)for(unsigned i=0;i<8;i++) {
            uint8_t *raw=(uint8_t *)&g->units.items[1];raw[10+i*2]=0;raw[11+i*2]=(uint8_t)i;
        }
        CHECK(add_dead(g,2,2,39)==0 && add_dead(g,3,1,25)==0);
        CHECK(begin_defeated_loot(g,1,0,PLAYER_ACTION_ITEM,0,0)==1 && reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
        CHECK(g->event_resume.active && g->event_resume.child_wait && g->loot.child.active &&
            g->loot.child.notice.kind==FD2_FIELD_LOOT_MESSAGE && g->loot.child.notice.fragment==432);
        if(stop)CHECK(fd2_field_game_loot_text_ready(g)==0);
        if(stop==2) {
            CHECK(fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_ACK,-1)==0 &&
                fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_ACK,-1)==0 &&
                g->loot.child.notice.kind==FD2_FIELD_LOOT_CHOICE);
        }
        char path[FD2_TEST_PATH_MAX];fd2_save_file disk={0};
        CHECK(fd2_test_temp_path(path)==0 && fd2_save_file_open(&disk,"original_game/START.SAV")==0);
        CHECK(fd2_save_file_write(&disk,path)==0);fd2_save_file_close(&disk);
        uint8_t saved[FD2_SAVE_FILE_SIZE],after[FD2_SAVE_FILE_SIZE];uint64_t identity[2],after_identity[2];
        CHECK(child_host_save_identity(path,saved,identity)==0);
        g->runtime_host_quit=1;if(error)g->failure=FD2_FIELD_FAILURE_ACTION;*before=*g;
        /* PC/child notice/ready/active、父descriptor、单位/roster/资金及RNG均锁住；
         * 合法ACK/choice返回0只是宿主已退出，不得借此清child并恢复父业务。 */
        CHECK(fd2_field_game_loot_text_ready(g)==(!error && stop==0?0:-1));
        CHECK(!memcmp(g,before,sizeof(*g)) && !calls);
        CHECK(fd2_field_game_loot_reply(g,stop==2?FD2_FIELD_LOOT_CANCEL:FD2_FIELD_LOOT_ACK,-1)==
            (!error && stop!=0?0:-1));
        CHECK(!memcmp(g,before,sizeof(*g)) && !calls);
        if(stop==2) {
            CHECK(fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_YES,-1)==(error?-1:0));
            CHECK(!memcmp(g,before,sizeof(*g)) && !calls);
        }
        fd2_save_battle_snapshot output,sentry;memset(&output,0xa5,sizeof(output));sentry=output;
        CHECK(fd2_field_game_export_battle_snapshot(g,&output)==-1 && !memcmp(&output,&sentry,sizeof(output)));
        CHECK(fd2_field_game_save_active(g,path)==-1 && fd2_field_game_save_manual_slot(g,path,2)==-1);
        CHECK(!memcmp(g,before,sizeof(*g)) && !calls && child_host_save_identity(path,after,after_identity)==0);
        CHECK(!memcmp(saved,after,sizeof(saved)) && !memcmp(identity,after_identity,sizeof(identity)));
        CHECK(remove(path)==0);
        common_test_override=NULL;fd2_field_game_close(g);fd2_archive_close(&ar);free(before);free(g);
    }
    return 0;
}
static int test_ai_physical_child_does_not_finish_parent(void) {
    fd2_field_game *g=calloc(1,sizeof(*g));fd2_archive ar;unsigned calls=0;
    CHECK(g && fixture(g,&ar,&calls)==0);
    common_test_override=fd2_static_flow_lookup("common_event","common.loot.child.0");CHECK(common_test_override);
    common_resource_calls=common_resource_reject=0;g->event_preflight=common_resource_check;
    CHECK(add_dead(g,2,2,39)==0 && add_dead(g,3,1,25)==0);
    outer_refresh_log log={0,0,30,2,1};
    g->runtime_present=check_outer_refresh;g->runtime_present_userdata=&log;g->experience_value=5;
    CHECK(begin_defeated_loot(g,1,0,PLAYER_ACTION_AI_PHYSICAL,0,0)==1 && log.count==1);
    CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && g->event_resume.child_wait && log.count==1);
    CHECK(fd2_field_game_loot_text_ready(g)==0 && fd2_field_game_loot_reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
    CHECK(!g->event_resume.child_wait && g->event_resume.active && !g->loot.cursor && log.count==1 &&
        !g->progress.notice.kind && g->loot.action_kind==PLAYER_ACTION_AI_PHYSICAL);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0 && g->loot.cursor==1 && log.count==1);
    CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && log.count==2 && g->resource_value==30 &&
        g->progress.notice.kind!=FD2_FIELD_PROGRESS_NONE && !g->event_resume.active && !g->loot.source);
    CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==-1 && log.count==2 && !calls && !g->failure);
    common_test_override=NULL;fd2_field_game_close(g);fd2_archive_close(&ar);free(g);return 0;
}
static int test_wait_cell(int mode,int full,int accept){
    fd2_field_game*g=calloc(1,sizeof(*g));fd2_archive ar;unsigned calls=0;CHECK(fixture(g,&ar,&calls)==0);
    fd2_field_unit*u=&g->units.items[0];u->x=7;u->y=13;u->hp=u->hp_max;
    uint8_t*r=(uint8_t*)u;if(full)for(size_t i=0;i<8;i++){r[10+i*2]=0;r[11+i*2]=(uint8_t)i;}
    uint32_t before=fd2_field_map_cell(&g->map,7,13);CHECK(((before>>16)&31u)==0);
    g->metadata.cell_actions[0].mode=(uint8_t)mode;g->metadata.cell_actions[0].param=mode?0:192;
    g->message_number=777;
    CHECK(fd2_field_game_finish_unit_action(g)==0 && g->loot.notice.fragment==421);
    CHECK(reply(g,accept?FD2_FIELD_LOOT_YES:FD2_FIELD_LOOT_CANCEL,-1)==0);
    CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
    if(!accept){CHECK(g->loot.notice.fragment==412);CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
        CHECK(g->cell_action_completed[0]==0 && fd2_field_map_cell(&g->map,7,13)==before);}
    else if(mode){CHECK(g->loot.notice.fragment==427 && g->loot.notice.value==777);
        CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && g->resource_value==782 && g->cell_action_completed[0]==1);}
    else if(!full){CHECK(g->loot.notice.fragment==422 && g->cell_action_completed[0]==0);
        CHECK(fd2_field_game_loot_text_ready(g)==0 && g->cell_action_completed[0]==1);
        CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
        uint32_t after=fd2_field_map_cell(&g->map,7,13);CHECK((uint16_t)after==(uint16_t)((uint16_t)before+1u) && (after&0xff000000u)==(before&0xff000000u) && !(after&0xff0000u));}
    else {CHECK(g->loot.notice.fragment==422 && !g->cell_action_completed[0]);
        CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
        CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && g->loot.notice.fragment==423);
        CHECK(reply(g,FD2_FIELD_LOOT_YES,-1)==0);
        CHECK(reply(g,FD2_FIELD_LOOT_SLOT,0)==0);
        CHECK(g->metadata.cell_actions[0].param==0 && r[25]==192);
        CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && g->loot.notice.fragment==425 && g->loot.notice.old_item_id==0);
        CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
        CHECK(!g->cell_action_completed[0] && fd2_field_map_cell(&g->map,7,13)==before);}
    CHECK(!g->loot.notice.kind && fd2_field_unit_has_acted(u) && calls==0);
    fd2_field_game_close(g);fd2_archive_close(&ar);free(g);return 0;
}
static int reject_resource(const fd2_field_game*g,const fd2_field_loot_notice*n,void*ctx){(void)g;(*(unsigned*)ctx)++;return n->fragment==425?-1:0;}
static int test_resource_error_before_wait(void){
    fd2_field_game*g=calloc(1,sizeof(*g));fd2_archive ar;unsigned calls=0,checks=0;CHECK(fixture(g,&ar,&calls)==0);
    g->units.items[0].x=7;g->units.items[0].y=13;g->units.items[0].hp=1;g->move_path_length=0;
    g->loot_preflight=reject_resource;g->loot_preflight_userdata=&checks;fd2_field_units before=g->units;
    CHECK(fd2_field_game_finish_unit_action(g)==-1 && g->failure==FD2_FIELD_FAILURE_ACTION);
    CHECK(calls==0 && checks>0 && !memcmp(&before,&g->units,sizeof(before)));
    fd2_field_game_close(g);fd2_archive_close(&ar);free(g);return 0;
}
typedef struct { fd2_field_game *game; int tracks[8]; uint8_t sides[8]; size_t count; } music_log;
static void music_request(size_t stage,int track,fd2_field_music_reason reason,void*userdata){
    music_log*log=userdata;(void)reason;if(stage!=0 || log->count>=8)abort();
    log->tracks[log->count]=track;log->sides[log->count++]=log->game->active_side;
}
static int test_music_and_message_owner(void){
    fd2_field_game*g=calloc(1,sizeof(*g));fd2_archive ar;unsigned calls=0;CHECK(fixture(g,&ar,&calls)==0);
    music_log log={0};log.game=g;CHECK(fd2_field_game_set_music_request(g,music_request,&log)==0);
    g->active_side=1;g->units.items[0].side=0;g->units.items[0].detail_status[0]=1;
    g->units.items[0].hp=20;g->message_number=777;
    advance_phase(g);CHECK(log.count==1 && log.tracks[0]==-1 && log.sides[0]==1);
    CHECK(g->progress.notice.fragment==487 && g->message_number==4 && g->units.items[0].hp==16);
    CHECK(fd2_field_game_progress_ack(g)==0 && g->progress.notice.fragment==484 && log.count==1);
    CHECK(fd2_field_game_progress_ack(g)==0 && log.count==2 && log.tracks[1]==12 && log.sides[1]==0 && g->message_number==4);
    advance_phase(g);CHECK(log.count==4 && log.tracks[2]==-1 && log.tracks[3]==19 && log.sides[2]==0 && log.sides[3]==0);
    CHECK(g->active_side==2 && calls==0);
    fd2_field_game_close(g);fd2_archive_close(&ar);free(g);return 0;
}
/* 改冷图的YES边即可改变业务选择；不能由C内的原状态switch继续主导。 */
static int test_graph_choice_and_unknown_operation(void) {
    const fd2_static_flow *source = fd2_static_flow_lookup("loot", "loot");
    CHECK(source && source->count);
    fd2_static_flow_node *nodes = malloc(source->count * sizeof(*nodes));
    CHECK(nodes);
    fd2_static_flow changed = *source; changed.nodes = nodes;
    for (int bad = 0; bad < 2; bad++) {
        memcpy(nodes, source->nodes, source->count * sizeof(*nodes));
        int at = fd2_static_flow_label(source, bad ? "cell.grant.422" : "cell.ask_swap");
        CHECK(at >= 0);
        if (bad) nodes[at].op = "unknown_loot_operation";
        else nodes[at].edges[FD2_FLOW_YES] = nodes[at].edges[FD2_FLOW_NO];
        loot_test_override = &changed;
        fd2_field_game *g = calloc(1, sizeof(*g)); fd2_archive ar; unsigned calls = 0;
        CHECK(fixture(g, &ar, &calls) == 0);
        fd2_field_unit *u = &g->units.items[0]; u->x = 7; u->y = 13; u->hp = u->hp_max;
        uint8_t *raw = (uint8_t *)u;
        if (!bad) for (size_t i = 0; i < 8; i++) { raw[10+i*2] = 0; raw[11+i*2] = (uint8_t)i; }
        CHECK(fd2_field_game_finish_unit_action(g) == 0);
        CHECK(reply(g, FD2_FIELD_LOOT_YES, -1) == 0);
        CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0 && g->loot.notice.fragment == 422);
        fd2_field_unit before = *u;
        if (bad) {
            CHECK(fd2_field_game_loot_text_ready(g) == -1);
            CHECK(g->failure == FD2_FIELD_FAILURE_ACTION && !memcmp(u, &before, sizeof(before)));
            CHECK(!g->cell_action_completed[0]);
        } else {
            CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0);
            CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0 && g->loot.notice.fragment == 423);
            CHECK(reply(g, FD2_FIELD_LOOT_YES, -1) == 0 && g->loot.notice.fragment == 424);
            CHECK(!memcmp(u, &before, sizeof(before)) && !g->cell_action_completed[0]);
            CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0 && !g->loot.source);
        }
        CHECK(calls == 0);
        fd2_field_game_close(g); fd2_archive_close(&ar); free(g);
        loot_test_override = NULL;
    }
    free(nodes); return 0;
}
static int test_invalid_reply_and_slot_are_read_only(void) {
    fd2_field_game *g = calloc(1, sizeof(*g)); fd2_archive ar; unsigned calls = 0;
    CHECK(fixture(g, &ar, &calls) == 0);
    uint8_t *raw = (uint8_t *)&g->units.items[0];
    for (size_t i = 0; i < 8; i++) { raw[10+i*2] = 0; raw[11+i*2] = (uint8_t)i; }
    CHECK(add_dead(g, 2, 0, 192) == 0);
    CHECK(begin_defeated_loot(g, 0, 0, PLAYER_ACTION_ITEM, 0, 0) == 1);
    fd2_field_loot before = g->loot;
    CHECK(fd2_field_game_loot_reply(g, FD2_FIELD_LOOT_ACK, -1) == -1);
    CHECK(!memcmp(&before, &g->loot, sizeof(before)) && !g->failure);
    CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0);
    CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0 && g->loot.notice.fragment == 433);
    CHECK(reply(g, FD2_FIELD_LOOT_YES, -1) == 0);
    before = g->loot; fd2_field_unit actor = g->units.items[0];
    CHECK(fd2_field_game_loot_reply(g, FD2_FIELD_LOOT_SLOT, 8) == -1);
    CHECK(fd2_field_game_loot_reply(g, FD2_FIELD_LOOT_YES, 0) == -1);
    CHECK(!memcmp(&before, &g->loot, sizeof(before)) && !memcmp(&actor, &g->units.items[0], sizeof(actor)) && !g->failure);
    CHECK(reply(g, FD2_FIELD_LOOT_CANCEL, -1) == 0);
    CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0 && g->loot.notice.fragment == 434);
    CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0 && !g->loot.source && calls == 0);
    fd2_field_game_close(g); fd2_archive_close(&ar); free(g); return 0;
}
static int test_cell_picker_cancel_and_money_variants(void) {
    for (int scenario = 0; scenario < 3; scenario++) {
        fd2_field_game *g = calloc(1, sizeof(*g)); fd2_archive ar; unsigned calls = 0;
        CHECK(fixture(g, &ar, &calls) == 0);
        fd2_field_unit *u = &g->units.items[0]; u->x = 7; u->y = 13; u->hp = u->hp_max;
        uint32_t before_cell = fd2_field_map_cell(&g->map, 7, 13);
        if (scenario == 0) {
            uint8_t *r = (uint8_t *)u;
            for (size_t i = 0; i < 8; i++) { r[10+i*2] = 0; r[11+i*2] = (uint8_t)i; }
        } else {
            g->metadata.cell_actions[0].mode = 1; g->metadata.cell_actions[0].param = 1234;
            if (scenario == 2) {
                size_t terrain = fd2_field_cell_terrain(before_cell);
                g->terrain.attrs[terrain].flags = (uint8_t)((g->terrain.attrs[terrain].flags & ~0x60u) | 0x40u);
            }
        }
        fd2_field_unit before = *u;
        CHECK(fd2_field_game_finish_unit_action(g) == 0);
        CHECK(g->loot.notice.fragment == (scenario == 2 ? 428u : 421u));
        CHECK(reply(g, FD2_FIELD_LOOT_YES, -1) == 0);
        CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0);
        if (scenario == 0) {
            CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0);
            CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0 && g->loot.notice.fragment == 423);
            CHECK(reply(g, FD2_FIELD_LOOT_YES, -1) == 0);
            CHECK(reply(g, FD2_FIELD_LOOT_CANCEL, -1) == 0 && g->loot.notice.wait_ms == 100);
            CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0 && g->loot.notice.fragment == 424);
            CHECK(g->loot.notice.open_box && g->loot.notice.close_box && !g->loot.notice.wait_key);
            CHECK(!memcmp(u, &before, sizeof(before)) && !g->cell_action_completed[0]);
            CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0);
            CHECK(fd2_field_map_cell(&g->map, 7, 13) == before_cell);
        } else {
            CHECK(g->loot.notice.fragment == (scenario == 2 ? 430u : 426u));
            CHECK(g->loot.notice.value == 1234 && g->message_number == 1234 && g->resource_value == 5);
            CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0);
            CHECK(g->resource_value == 1239 && g->cell_action_completed[0]);
            if (scenario == 2) CHECK(fd2_field_map_cell(&g->map, 7, 13) == before_cell);
        }
        CHECK(!g->loot.source && !g->failure && calls == 0);
        fd2_field_game_close(g); fd2_archive_close(&ar); free(g);
    }
    return 0;
}
static int test_bad_edges_cannot_bypass_primitive_bounds_or_preflight(void) {
    const fd2_static_flow *source = fd2_static_flow_lookup("loot", "loot");
    CHECK(source && source->count);
    fd2_static_flow_node *nodes = malloc(source->count * sizeof(*nodes)); CHECK(nodes);
    fd2_static_flow changed = *source; changed.nodes = nodes;
    for (int scenario = 0; scenario < 3; scenario++) {
        memcpy(nodes, source->nodes, source->count * sizeof(*nodes));
        const char *from[] = {"preflight.cell", "collect.more", "cell.pick"};
        const char *to[] = {"cell.init", "collect.copy", "cell.remember"};
        unsigned port[] = {FD2_FLOW_YES, FD2_FLOW_NO, FD2_FLOW_SLOT};
        int at = fd2_static_flow_label(source, from[scenario]);
        int target = fd2_static_flow_label(source, to[scenario]); CHECK(at >= 0 && target >= 0);
        nodes[at].edges[port[scenario]] = (int16_t)target; loot_test_override = &changed;
        fd2_field_game *g = calloc(1, sizeof(*g)); fd2_archive ar; unsigned calls = 0;
        CHECK(fixture(g, &ar, &calls) == 0);
        fd2_field_unit *u = &g->units.items[0]; u->x = 7; u->y = 13; u->hp = u->hp_max;
        if (scenario == 2) {
            uint8_t *raw = (uint8_t *)u;
            for (size_t i = 0; i < 8; i++) { raw[10+i*2] = 0; raw[11+i*2] = (uint8_t)i; }
            CHECK(fd2_field_game_finish_unit_action(g) == 0);
            CHECK(reply(g, FD2_FIELD_LOOT_YES, -1) == 0);
            CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0);
            CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0);
            CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0);
            CHECK(reply(g, FD2_FIELD_LOOT_YES, -1) == 0 && g->loot.notice.kind == FD2_FIELD_LOOT_INVENTORY);
        }
        fd2_field_units before_units = g->units; fd2_field_loot before_loot = g->loot;
        if (scenario == 0) CHECK(fd2_field_game_finish_unit_action(g) == -1);
        else if (scenario == 1) CHECK(begin_defeated_loot(g, 0, 0, PLAYER_ACTION_ITEM, 0, 0) == -1);
        else CHECK(fd2_field_game_loot_reply(g, FD2_FIELD_LOOT_SLOT, INT32_MAX) == -1);
        CHECK(g->failure == FD2_FIELD_FAILURE_ACTION && calls == 0);
        CHECK(!memcmp(&before_units, &g->units, sizeof(before_units)) && !memcmp(&before_loot, &g->loot, sizeof(before_loot)));
        CHECK(!g->cell_action_completed[0]);
        fd2_field_game_close(g); fd2_archive_close(&ar); free(g); loot_test_override = NULL;
    }
    free(nodes);
    /* 源记录/格号同样在领域原语本身校验，不依赖图中某个先行guard。 */
    fd2_field_game *g = calloc(1, sizeof(*g)); fd2_archive ar; unsigned calls = 0;
    CHECK(fixture(g, &ar, &calls) == 0);
    fd2_loot_flow_context ctx = loot_flow_reply_context(g, -1);
    ctx.scan = g->units.count; CHECK(loot_flow_action(&ctx, 1) == -1 && loot_flow_action(&ctx, 2) == -1);
    g->loot.cell_index = FD2_FIELD_EVENT_SLOTS;
    fd2_field_metadata metadata = g->metadata;
    CHECK(loot_flow_action(&ctx, 16) == -1 && loot_flow_action(&ctx, 18) == -1);
    CHECK(!memcmp(&metadata, &g->metadata, sizeof(metadata)));
    CHECK(execute_loot_callback(g) == -1 && !g->event_log_count);
    g->loot.cell_index = 0; g->loot.count = 1; g->loot.descriptors[0][1] = 255;
    fd2_field_unit before_item = g->units.items[0];
    CHECK(loot_flow_action(&ctx, 7) == -1 && loot_flow_action(&ctx, 10) == -1);
    CHECK(!memcmp(&before_item, &g->units.items[0], sizeof(before_item)));
    fd2_field_game_close(g); fd2_archive_close(&ar); free(g); return 0;
}
static int count_action_input_clear(fd2_field_game *game,fd2_field_runtime_present_step step,int value,void *userdata) {
    (void)game;CHECK(step==FD2_FIELD_RUNTIME_CLEAR_KEYS && value==0);(*(unsigned*)userdata)++;return 0;
}
static int test_action_finish_kind_branch_comes_from_cold_flow(void) {
    const fd2_static_flow *source = fd2_static_flow_lookup("loot", "action.finish");
    CHECK(source && source->count);
    fd2_static_flow_node *nodes = malloc(source->count * sizeof(*nodes)); CHECK(nodes);
    memcpy(nodes, source->nodes, source->count * sizeof(*nodes));
    int branch = fd2_static_flow_label(source, "physical.keys");
    int mark = fd2_static_flow_label(source, "kind.magic"); CHECK(branch >= 0 && mark >= 0);
    nodes[branch].edges[FD2_FLOW_YES] = (int16_t)mark;
    fd2_static_flow changed = *source; changed.nodes = nodes;
    for (int modified = 0; modified < 2; modified++) {
        fd2_field_game *g = calloc(1, sizeof(*g)); fd2_archive ar; unsigned calls = 0;
        CHECK(fixture(g, &ar, &calls) == 0);
        g->units.items[0].direction = 3; g->units.items[1].direction = 2;
        unsigned clears=0;g->runtime_present=count_action_input_clear;g->runtime_present_userdata=&clears;
        loot_test_override = modified ? &changed : NULL;
        CHECK(finish_unit_action_internal(g, PLAYER_ACTION_PHYSICAL, 0) == 0);
        CHECK(g->selected_unit == -1 && g->interaction == FD2_FIELD_INTERACTION_BROWSE);
        /* 原18d8c的reset在collect之前，action.finish不能再改朝向。 */
        CHECK(g->units.items[0].direction == 3 && g->units.items[1].direction == 2);
        CHECK(clears==(modified?1u:2u));
        CHECK(fd2_field_unit_has_acted(&g->units.items[0]) && !g->failure && calls == 0);
        fd2_field_game_close(g); fd2_archive_close(&ar); free(g); loot_test_override = NULL;
    }
    /* 同步action图不能冒充可恢复loot等待点，防止step被按错误图解释。 */
    fd2_field_game *g = calloc(1, sizeof(*g)); fd2_archive ar; unsigned calls = 0;
    CHECK(fixture(g, &ar, &calls) == 0);
    fd2_static_flow_node invalid_wait = {0}; invalid_wait.id = "invalid.wait";
    invalid_wait.op = "emit"; invalid_wait.args[0] = FD2_FIELD_LOOT_MESSAGE;
    invalid_wait.args[1] = 432; invalid_wait.views = FD2_FLOW_LOGIC;
    fd2_static_flow invalid_flow = {"action.finish", &invalid_wait, 1, 0};
    fd2_loot_flow_context ctx = loot_flow_reply_context(g, -1); ctx.flow = &invalid_flow;
    fd2_field_loot before = g->loot;
    CHECK(loot_flow_run(&ctx, 0) == -1 && g->failure == FD2_FIELD_FAILURE_ACTION);
    CHECK(!memcmp(&before, &g->loot, sizeof(before)) && calls == 0);
    fd2_field_game_close(g); fd2_archive_close(&ar); free(g);
    free(nodes); return 0;
}
static int test_controller_rejection_preserves_existing_failure_boundary(void) {
    fd2_field_game *g = calloc(1, sizeof(*g)); fd2_archive ar; unsigned calls = 0;
    CHECK(fixture(g, &ar, &calls) == 0);
    fd2_field_unit *u = &g->units.items[0]; u->side = 1; u->hp = u->hp_max;
    g->active_side = 1;
    size_t at = (size_t)u->y * (size_t)g->map.width + u->x;
    g->map.cells[at] &= ~UINT32_C(0x1f0000);
    g->terrain.attrs[fd2_field_cell_terrain(g->map.cells[at])].flags &= (uint8_t)~0x60u;
    CHECK(fd2_field_game_finish_unit_action(g) == -1);
    /* 原尾已经提交acted/清理；controller拒绝不补造ACTION failure。 */
    CHECK(fd2_field_unit_has_acted(u) && g->selected_unit == -1);
    CHECK(g->interaction == FD2_FIELD_INTERACTION_BROWSE && !g->loot.source);
    CHECK(g->failure == FD2_FIELD_FAILURE_NONE && calls == 0);
    fd2_field_game_close(g); fd2_archive_close(&ar); free(g); return 0;
}
/* old compiled core 的独立无窗口快照：default-init.bin，SHA256
 * 0f64854938e211872c559f4e59b8cd5228d361c5a9a9bba4f2c924c31c32b01f。
 * 只固化前12个active和4个persistent的完整80B，未复制旧初始化执行路径。 */
static const uint8_t expected_startup_units[12][80] = {
    {
        0x07, 0x14, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x00, 0x40, 0x84, 0x00, 0xc0, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x04, 0x00, 0x04,
        0x00, 0x00, 0x02, 0x00, 0x2a, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x0c, 0x00, 0x61, 0x00, 0x02, 0x00,
    },
    {
        0x0a, 0x15, 0x00, 0x00, 0x00, 0x00, 0x02, 0x09, 0x09, 0x00, 0x40, 0x34, 0x40, 0xa4, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0x00, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x02, 0x00, 0x04,
        0x00, 0x00, 0x01, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x08, 0x00, 0x08, 0x00, 0x0b, 0x00, 0x07, 0x00, 0x56, 0x00, 0x01, 0x00,
    },
    {
        0x08, 0x16, 0x00, 0x00, 0x00, 0x00, 0x02, 0x04, 0x04, 0x00, 0x40, 0x14, 0x40, 0x80, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x04, 0x00, 0x07,
        0x00, 0x00, 0x02, 0x00, 0x30, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x06, 0x00, 0x5c, 0x00, 0x02, 0x00,
    },
    {
        0x0b, 0x17, 0x00, 0x00, 0x00, 0x00, 0x02, 0x1e, 0x1e, 0x00, 0x40, 0x48, 0x40, 0xb2, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x19, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x06, 0x00, 0x04,
        0x00, 0x00, 0x02, 0x00, 0x32, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x00, 0x0e, 0x00, 0x5c, 0x00, 0x02, 0x00,
    },
    {
        0x01, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x00, 0x40, 0x00, 0x40, 0x80, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0xff, 0x80, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x02, 0x00, 0x04,
        0xff, 0x01, 0x02, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    {
        0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x00, 0x40, 0x00, 0x40, 0x80, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0xff, 0x80, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x02, 0x00, 0x04,
        0xff, 0x01, 0x02, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    {
        0x04, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x00, 0x40, 0x00, 0x40, 0x80, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0xff, 0x80, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x02, 0x00, 0x04,
        0xff, 0x01, 0x02, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    {
        0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x00, 0x40, 0x00, 0x40, 0x80, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0xff, 0x80, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x02, 0x00, 0x04,
        0xff, 0x01, 0x02, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    {
        0x01, 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x00, 0x40, 0x00, 0x40, 0x80, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0xff, 0x80, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x02, 0x00, 0x04,
        0xff, 0x01, 0x02, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    {
        0x02, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x00, 0x40, 0x00, 0x40, 0x80, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0xff, 0x80, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x02, 0x00, 0x04,
        0xff, 0x01, 0x02, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    {
        0x03, 0x16, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x00, 0x40, 0x00, 0x40, 0x80, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0xff, 0x80, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x02, 0x00, 0x04,
        0xff, 0x01, 0x02, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
    {
        0x04, 0x17, 0x00, 0x00, 0x00, 0x00, 0x00, 0x60, 0x60, 0x00, 0x40, 0x00, 0x40, 0x80, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0xff, 0x80, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0e, 0x00, 0x02, 0x00, 0x04,
        0xff, 0x01, 0x02, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    },
};
static const uint8_t expected_startup_roster[4][80] = {
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x00, 0x40, 0x84, 0x00, 0xc0, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x04, 0x00, 0x04,
        0x00, 0x00, 0x02, 0x00, 0x2a, 0x00, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x0c, 0x00, 0x61, 0x00, 0x02, 0x00,
    },
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x09, 0x09, 0x00, 0x40, 0x34, 0x40, 0xa4, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0x00, 0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x01, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x02, 0x00, 0x04,
        0x00, 0x00, 0x01, 0x00, 0x1c, 0x00, 0x1c, 0x00, 0x08, 0x00, 0x08, 0x00, 0x0b, 0x00, 0x07, 0x00, 0x56, 0x00, 0x01, 0x00,
    },
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x04, 0x04, 0x00, 0x40, 0x14, 0x40, 0x80, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x03, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x00, 0x04, 0x00, 0x07,
        0x00, 0x00, 0x02, 0x00, 0x30, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1a, 0x00, 0x06, 0x00, 0x5c, 0x00, 0x02, 0x00,
    },
    {
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x1e, 0x1e, 0x00, 0x40, 0x48, 0x40, 0xb2, 0x80, 0xff, 0x80, 0xff, 0x80, 0xff,
        0x80, 0xff, 0x80, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x19, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0x00, 0x06, 0x00, 0x04,
        0x00, 0x00, 0x02, 0x00, 0x32, 0x00, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x16, 0x00, 0x0e, 0x00, 0x5c, 0x00, 0x02, 0x00,
    },
};

static void corrected_startup_records(uint8_t expected[12][80]) {
    /* 原11019按4玩家再group1/2去重，模板1b750初始装备派生。
     * 独立原EXE物品表及既有oracle确认96的四WORD为24/4/97/2。
     * 保留旧编译快照作其余字段的基准，不拿被测结果填期望。 */
    const uint8_t classes[12] = {0,1,2,3,4,4,4,4,4,4,4,4};
    const uint8_t stats[8] = {24,0,4,0,97,0,2,0};
    memcpy(expected, expected_startup_units, sizeof(expected_startup_units));
    for (size_t i = 0; i < 12; i++) {
        expected[i][2] = classes[i];
        if (i >= 4) memcpy(expected[i] + 0x48, stats, sizeof(stats));
    }
}

static int test_initial_units_match_corrected_original_contract(void) {
    fd2_field_game *g = calloc(1, sizeof(*g)); CHECK(g);
    fd2_archive ar = {0}; fd2_vga vga = {0};
    CHECK(fd2_archive_open(&ar, "original_game/FDOTHER.DAT") == 0);
    CHECK(fd2_field_game_open(g, &vga, &ar, 0) == 0);
    CHECK(g->units.count == 12 && g->roster_count == 4);
    const uint8_t zero[80] = {0};
    uint8_t corrected[12][80]; corrected_startup_records(corrected);
    for (size_t i = 0; i < FD2_FIELD_MAX_UNITS; i++) {
        uint8_t expected[80];
        memcpy(expected, i < 12 ? corrected[i] : zero, sizeof(expected));
        if (memcmp(&g->units.items[i], expected, 80)) {
            const uint8_t *actual = (const uint8_t *)&g->units.items[i];
            for (size_t j = 0; j < 80; j++) if (actual[j] != expected[j])
                fprintf(stderr, "startup raw%zu +%02zx: expected=%02x actual=%02x\n", i, j, expected[j], actual[j]);
        }
        CHECK(!memcmp(&g->units.items[i], expected, 80));
        CHECK(g->units.walk_frames[i] == (i < 12));
        uint16_t source = i < 4 ? FD2_FIELD_UNIT_NO_TEMPLATE : i < 12 ? (uint16_t)(i - 4) : 0;
        CHECK(g->units.source_template_indices[i] == source);
    }
    for (size_t i = 0; i < FD2_SAVE_BATTLE_MAX_ROSTER; i++)
        CHECK(!memcmp(&g->roster[i], i < 4 ? expected_startup_roster[i] : zero, 80));
    fd2_field_game_close(g); fd2_archive_close(&ar); free(g); return 0;
}

static int test_initial_units_follow_shared_cold_choices(void) {
    const fd2_static_flow *source = fd2_static_flow_lookup("scene", "scene.init.000");
    CHECK(source && source->count);
    fd2_static_flow_node *nodes = malloc(source->count * sizeof(*nodes)); CHECK(nodes);
    memcpy(nodes, source->nodes, source->count * sizeof(*nodes));
    fd2_static_flow changed = *source; changed.nodes = nodes;
    int first = fd2_static_flow_label(source, "init_player_01");
    int second = fd2_static_flow_label(source, "init_player_02");
    int templates = fd2_static_flow_label(source, "init_templates_04");
    int hidden = fd2_static_flow_label(source, "hide_opening_templates");
    CHECK(first >= 0 && second >= 0 && templates >= 0 && hidden >= 0);
    nodes[first].args[1] = source->nodes[second].args[1];
    nodes[second].args[1] = source->nodes[first].args[1];
    nodes[templates].args[1] = 7;
    nodes[hidden].args[1] = 10;
    fd2_field_game *g = calloc(1, sizeof(*g)); CHECK(g);
    fd2_archive ar = {0}; fd2_vga vga = {0};
    CHECK(fd2_archive_open(&ar, "original_game/FDOTHER.DAT") == 0);
    startup_test_override = &changed; startup_test_stat_delta = 1;
    CHECK(fd2_field_game_open(g, &vga, &ar, 0) == 0);
    startup_test_override = NULL; startup_test_stat_delta = 0;
    CHECK(g->units.count == 11 && g->roster_count == 4);
    fd2_field_unit expected;
    memcpy(&expected, expected_startup_roster[2], sizeof(expected)); expected.attack++;
    CHECK(!memcmp(&g->roster[1], &expected, sizeof(expected)));
    size_t effective_templates;
    CHECK(fd2_field_unit_template_count(&g->metadata, &effective_templates) == 0);
    const fd2_field_placement *position = &g->placements.records[effective_templates + 1];
    expected.x = (uint8_t)position->x; expected.y = (uint8_t)position->y;
    expected.sprite_cache_class = 1; /* 冷图交换9/4，第二个新ID仍是cache slot1。 */
    CHECK(!memcmp(&g->units.items[1], &expected, sizeof(expected)));
    CHECK(g->units.items[2].unit_id == 9 && g->roster[2].unit_id == 9);
    for (size_t i = 4; i < g->units.count; i++)
        CHECK(g->units.source_template_indices[i] == i - 4);
    fd2_field_game_close(g);
    /* metadata分组不是第二份成员选择；冷图指定的模板索引仍生效。 */
    CHECK(fd2_field_game_open_resources(g, &ar, 0) == 0);
    g->metadata.unit_templates[0].bytes[0x15] = 77;
    unsigned calls = 0; g->attack_rng = rng; g->attack_rng_userdata = &calls;
    CHECK(load_initial_units(g, source) == 0 && calls == 0);
    CHECK(g->units.count == 12 && g->units.source_template_indices[4] == 0);
    uint8_t corrected[12][80]; corrected_startup_records(corrected);
    CHECK(!memcmp(g->units.items, corrected, sizeof(corrected)));
    fd2_field_game_close(g);
    /* hidden=0是不额外隐藏，不应清除构造算法已经给出的hidden。 */
    startup_test_template_hidden = 1;
    CHECK(fd2_field_game_open(g, &vga, &ar, 0) == 0);
    startup_test_template_hidden = 0;
    for (size_t i = 4; i < g->units.count; i++) CHECK(fd2_field_unit_is_hidden(&g->units.items[i]));
    fd2_field_game_close(g); fd2_archive_close(&ar); free(g); free(nodes); return 0;
}

static int test_initial_units_reject_bad_or_missing_logic_definitions(void) {
    const fd2_static_flow *source = fd2_static_flow_lookup("scene", "scene.init.000"); CHECK(source);
    fd2_static_flow_node *nodes = malloc(source->count * sizeof(*nodes)); CHECK(nodes);
    fd2_static_flow changed = *source; changed.nodes = nodes;
    fd2_field_game *g = calloc(1, sizeof(*g)); CHECK(g);
    fd2_archive ar = {0}; fd2_vga vga = {0};
    CHECK(fd2_archive_open(&ar, "original_game/FDOTHER.DAT") == 0);
    for (int mode = 0; mode < 3; mode++) {
        memcpy(nodes, source->nodes, source->count * sizeof(*nodes));
        if (!mode) nodes[source->entry].args[2] = 64;
        else if (mode == 1) {
            for (size_t i = 0; i < source->count; i++)
                if (strcmp(nodes[i].op, "end")) nodes[i].views = FD2_FLOW_PRESENT;
        } else {
            int hidden = fd2_static_flow_label(source, "hide_opening_templates"); CHECK(hidden >= 0);
            nodes[hidden].views = FD2_FLOW_LOGIC | FD2_FLOW_PRESENT;
        }
        startup_test_override = &changed;
        CHECK(fd2_field_game_open(g, &vga, &ar, 0) == -1);
        CHECK(!g->ready && !g->units.count && !g->roster_count);
        startup_test_override = NULL;
    }
    fd2_field_game_close(g); fd2_archive_close(&ar); free(g); free(nodes); return 0;
}
/* 原1aa1d switch default必须继续后续descriptor；255在collector单独排除。
 * 同时核对纯预检无stage特判／资源查询／RNG／业务写，不能只接受mode203。 */
static int test_descriptor_default_and_collector_sentinel(void) {
    fd2_field_game *g = calloc(1, sizeof(*g)); fd2_archive ar; unsigned calls = 0;
    CHECK(g && fixture(g, &ar, &calls) == 0);
    g->stage = 14;
    fd2_field_units before = g->units;
    for (unsigned mode = 4; mode <= 255; mode++) {
        uint8_t d[3] = {(uint8_t)mode, 255, 255};
        CHECK(loot_descriptor_preflight(g, 0, d) == 0);
        CHECK(!memcmp(&g->units, &before, sizeof(before)) && calls == 0);
        CHECK(g->resource_value == 5 && g->loot.count == 0 && g->event_log_count == 0);
    }
    /* 普通物品／金钱预检取决于领域数据与呈现资源，不取决于stage编号。
     * 此处是stage0资源逻辑夹具，不声明已适配stage14剧情。 */
    const uint8_t item_drop[3] = {0, 192, 0}, money_drop[3] = {1, 57, 0};
    CHECK(loot_descriptor_preflight(g, 0, item_drop) == 0);
    CHECK(loot_descriptor_preflight(g, 0, money_drop) == 0);
    CHECK(!memcmp(&g->units, &before, sizeof(before)) && calls == 0 && g->resource_value == 5);
    g->stage = 0;
    CHECK(add_dead(g, 2, 203, UINT16_MAX) == 0);
    CHECK(add_dead(g, 3, 255, UINT16_MAX) == 0);
    CHECK(add_dead(g, 4, 1, 57) == 0);
    CHECK(begin_defeated_loot(g, 0, 0, PLAYER_ACTION_ITEM, 0, 0) == 1);
    CHECK(g->loot.count == 2 && g->loot.cursor == 1 && g->loot.notice.fragment == 435);
    CHECK(calls == 0 && g->resource_value == 5 && g->event_log_count == 0);
    CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == 0 && g->resource_value == 62);
    CHECK(fd2_field_unit_has_acted(&g->units.items[0]) && calls == 0);
    CHECK(reply(g, FD2_FIELD_LOOT_ACK, -1) == -1 && g->resource_value == 62);
    fd2_field_game_close(g); fd2_archive_close(&ar); free(g); return 0;
}

typedef struct {unsigned calls,stop;int result,physical;} clear_tail_trace;
static int clear_tail_present(fd2_field_game *game,fd2_field_runtime_present_step step,int value,void *data) {
    clear_tail_trace *trace=data;
    if(step!=FD2_FIELD_RUNTIME_CLEAR_KEYS || value)return -1;
    trace->calls++;
    if(!fd2_field_unit_has_acted(&game->units.items[0]))return -1;
    if(trace->physical && trace->calls==1) {
        if(game->selected_unit!=0 || game->interaction!=FD2_FIELD_INTERACTION_COMMAND)return -1;
    } else if(game->selected_unit!=-1 || game->interaction!=FD2_FIELD_INTERACTION_BROWSE ||
        game->pending_cell_event.valid || game->player_post_waiting_phase)return -1;
    return trace->calls==trace->stop?trace->result:0;
}
static int test_source_physical_and_controller_clear_boundaries(void) {
    for(int kind=0;kind<4;kind++)for(int stop=0;stop<=2;stop++)for(int error=0;error<2;error++) {
        if(kind!=PLAYER_ACTION_PHYSICAL && stop==2)continue;
        fd2_field_game *g=calloc(1,sizeof(*g));fd2_archive ar;unsigned calls=0;CHECK(fixture(g,&ar,&calls)==0);
        clear_tail_trace trace={0,(unsigned)stop,error?-1:1,kind==PLAYER_ACTION_PHYSICAL};
        g->runtime_present=clear_tail_present;g->runtime_present_userdata=&trace;
        int result=loot_flow_finish_unit_action(g,(player_action_kind)kind,0);
        CHECK(result==((stop && error)?-1:0));
        CHECK(trace.calls==(stop?(unsigned)stop:kind==PLAYER_ACTION_PHYSICAL?2u:1u));
        CHECK(calls==0 && fd2_field_unit_has_acted(&g->units.items[0]) && !g->progress.notice.kind);
        CHECK(g->runtime_host_quit==(stop && !error) &&
            g->failure==((stop && error)?FD2_FIELD_FAILURE_ACTION:FD2_FIELD_FAILURE_NONE));
        if(kind==PLAYER_ACTION_PHYSICAL && stop==1)CHECK(g->selected_unit==0 && g->interaction==FD2_FIELD_INTERACTION_COMMAND);
        else CHECK(g->selected_unit==-1 && g->interaction==FD2_FIELD_INTERACTION_BROWSE && !g->pending_cell_event.valid);
        fd2_field_game_close(g);fd2_archive_close(&ar);free(g);
    }
    /* common pending完成也必须经过同一119a6尾，不能只修同步return。 */
    static const fd2_static_flow_node end_node={.id="end",.op="end",.edges={-1,-1,-1,-1,-1,-1,-1,-1}};
    static const fd2_static_flow flow={"test.controller.end",&end_node,1,0};
    for(int error=0;error<2;error++) {
        fd2_field_game *g=calloc(1,sizeof(*g));fd2_archive ar;unsigned calls=0;CHECK(fixture(g,&ar,&calls)==0);
        fd2_field_unit_set_acted(&g->units.items[0],1);g->selected_unit=-1;g->interaction=FD2_FIELD_INTERACTION_BROWSE;
        g->pending_cell_event=(fd2_field_pending_cell_event){1,39,0};g->player_post_waiting_phase=1;g->player_post_actor=0;
        g->event_resume=(fd2_field_event_resume){.flow=&flow,.pc=0,.actor=0,.active=1,
            .completion=FD2_EVENT_COMPLETE_PLAYER_PENDING};
        clear_tail_trace trace={0,1,error?-1:1,0};g->runtime_present=clear_tail_present;g->runtime_present_userdata=&trace;
        CHECK(common_event_run(g)==(error?-1:0));
        CHECK(trace.calls==1 && !g->event_resume.active && !g->pending_cell_event.valid && !g->player_post_waiting_phase && !calls);
        CHECK(g->runtime_host_quit==!error && g->failure==(error?FD2_FIELD_FAILURE_ACTION:FD2_FIELD_FAILURE_NONE));
        fd2_field_game_close(g);fd2_archive_close(&ar);free(g);
    }
    return 0;
}

typedef struct {unsigned delays,refreshes,stop;int result,acted;fd2_field_units prefix;} direction_wait_trace;
static int direction_wait_present(fd2_field_game *g,fd2_field_runtime_present_step step,int value,void *userdata) {
    direction_wait_trace *t=userdata;
    CHECK(step==FD2_FIELD_RUNTIME_DELAY_MS || step==FD2_FIELD_RUNTIME_REFRESH);
    if(step==FD2_FIELD_RUNTIME_DELAY_MS) {
        CHECK(value==20);t->delays++;
        for(size_t i=0;i<g->units.count;i++)CHECK(g->units.items[i].direction==0);
        if(t->acted>=0)CHECK(fd2_field_unit_has_acted(&g->units.items[0])==t->acted);
        if(t->delays==t->stop){t->prefix=g->units;return t->result;}
    } else {CHECK(value==0 && t->delays==1);t->refreshes++;}
    return 0;
}
static int test_original_direction_reset_wait_and_caller_boundaries(void) {
    for(int refresh=0;refresh<2;refresh++)for(int terminal=-1;terminal<=1;terminal++) {
        fd2_field_game g={0};g.ready=1;g.units.count=3;
        for(size_t i=0;i<3;i++) {
            g.units.items[i].direction=(uint8_t)(i+1);g.units.items[i].frame_phase=5;
            g.units.walk_frames[i]=2;g.units.items[i].flags=0x40;
        }
        direction_wait_trace trace={.stop=terminal?1:0,.result=terminal,.acted=-1};
        g.runtime_present=direction_wait_present;g.runtime_present_userdata=&trace;
        CHECK(run_turn_entry(&g,refresh?FD2_FIELD_TURN_DIRECTIONS_RESET_REFRESH:
            FD2_FIELD_TURN_DIRECTIONS_RESET)==terminal);
        CHECK(trace.delays==1 && trace.refreshes==(!terminal && refresh) && g.runtime_host_quit==(terminal==1));
        for(size_t i=0;i<3;i++)CHECK(!g.units.items[i].direction && g.units.items[i].frame_phase==5 &&
            g.units.walk_frames[i]==2 && g.units.items[i].flags==0x40);
        CHECK(!g.failure); /* adapter回传错误，实际caller按自身合同锁失败。 */
    }
    /* 13a9f的acted之后reset20→11cac0；16f55的acted之前reset20。
     * 错误/关闭窗口都不得越过各自下一业务写点。 */
    for(int march=0;march<2;march++)for(int terminal=-1;terminal<=1;terminal++) {
        if(march && !terminal)continue;
        fd2_field_game *g=calloc(1,sizeof(*g));fd2_archive ar;unsigned calls=0;CHECK(fixture(g,&ar,&calls)==0);
        for(size_t i=0;i<g->units.count;i++)g->units.items[i].direction=3;
        direction_wait_trace trace={.stop=terminal?1:0,.result=terminal,.acted=march?0:1};
        g->runtime_present=direction_wait_present;g->runtime_present_userdata=&trace;
        int result;
        if(march) {
            g->event_resume=(fd2_field_event_resume){.actor=0,.active=1,.completion=FD2_EVENT_COMPLETE_MARCH};
            result=common_event_finish(g);
        } else result=finish_and_dispatch_ai_action(g,0);
        CHECK(result==(terminal<0?-1:0) && trace.delays==1 && trace.refreshes==(!terminal && !march));
        CHECK(fd2_field_unit_has_acted(&g->units.items[0])==!march && !calls);
        if(terminal)CHECK(!memcmp(&g->units,&trace.prefix,sizeof(g->units)) && g->selected_unit==0 &&
            g->interaction==FD2_FIELD_INTERACTION_COMMAND);
        fd2_field_game_close(g);fd2_archive_close(&ar);free(g);
    }
    /* 35730裸2e2b0不借18d8c/1548e的全表reset；FULL caller一次、SHORT两次。
     * 真正执行同一HP/RNG核心，第三raw哨兵区分错误继承的全表写。 */
    for(int variant=0;variant<3;variant++)for(int stop=0;stop<=2;stop++) {
        if(stop && (!variant || (variant==1 && stop==2)))continue;
        fd2_field_game *g=calloc(1,sizeof(*g));fd2_archive ar;unsigned calls=0;CHECK(fixture(g,&ar,&calls)==0);
        CHECK(fd2_field_units_set(&g->units,2,0,0,2,9,14)==0);
        for(size_t i=0;i<3;i++) {
            g->units.items[i].hp=g->units.items[i].hp_max=1000;
            g->units.items[i].direction=3;g->units.items[i].attack=30;
            ((uint8_t*)&g->units.items[i])[0x31]=0xff;
        }
        g->units.items[1].side=0;g->units.items[1].defense=0;
        direction_wait_trace trace={.stop=(unsigned)stop,.result=1,.acted=0};
        g->runtime_present=direction_wait_present;g->runtime_present_userdata=&trace;
        field_action_flow action={0};action.game=g;action.actor=0;action.target=1;
        action.mode=variant==2?FD2_FIELD_PHYSICAL_MODE_SHORT:FD2_FIELD_PHYSICAL_MODE_FULL;
        CHECK(run_action_flow(&action,variant==2?"physical.short":variant==1?"physical.caller.full":"physical.full")==0);
        CHECK(trace.delays==(stop?(unsigned)stop:(unsigned)variant) && !trace.refreshes &&
            g->runtime_host_quit==(stop!=0) && !g->failure && !g->loot.source && !g->progress.notice.kind);
        CHECK(g->units.items[2].direction==(variant?0:3) && !fd2_field_unit_has_acted(&g->units.items[0]));
        if(stop)CHECK(!memcmp(&trace.prefix,&g->units,sizeof(g->units)));
        fd2_field_game_close(g);fd2_archive_close(&ar);free(g);
    }
    return 0;
}

typedef struct {size_t expected;unsigned projected,actual;} identity_probe;
static int identity_resource_probe(const fd2_field_game *g,const fd2_static_flow_node *n,
        const fd2_field_event_resource_view *view,void *data) {
    identity_probe *p=data;
    if(strcmp(n->op,"story"))return -1;
    if(view){p->projected++;return view->units && view->units->count==p->expected?0:-1;}
    p->actual++;return g->units.count==p->expected?0:-1;
}
static int test_linear_identity_preflight_retains_DWORD_and_write_domain(void) {
    const int32_t groups[]={256,511,-1};
    for(size_t test=0;test<3;test++) {
        fd2_field_game *g=calloc(1,sizeof(*g)),*before=malloc(sizeof(*before));fd2_archive ar;unsigned rolls=0;
        CHECK(g && before && fixture(g,&ar,&rolls)==0);
        g->metadata.unit_templates[0].bytes[0x15]=(uint8_t)groups[test]; /* 会抓到截BYTE的错误alias。 */
        fd2_static_flow_node nodes[]={
            {.id="append",.op="append_group",.args={groups[test],0},.edges={1,-1,-1,-1,-1,-1,-1,3}},
            {.id="story",.op="story",.args={0},.edges={-1,-1,-1,-1,2,-1,-1,3}},
            {.id="end",.op="end",.edges={-1,-1,-1,-1,-1,-1,-1,-1}},
            {.id="fail",.op="fail",.edges={-1,-1,-1,-1,-1,-1,-1,-1}}};
        fd2_static_flow flow={"identity.width",nodes,4,0};identity_probe probe={g->units.count,0,0};
        g->event_preflight=identity_resource_probe;g->event_preflight_userdata=&probe;*before=*g;
        CHECK(common_event_preflight_flow(g,&flow,0)==0 && probe.projected==1 && !probe.actual && !rolls);
        CHECK(!memcmp(g,before,sizeof(*g)));
        CHECK(common_event_bound_begin(g,&flow,0)==0 && g->units.count==probe.expected &&
            probe.projected==2 && probe.actual==1 && !rolls);
        CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0 && !g->event_resume.active);
        fd2_field_game_close(g);fd2_archive_close(&ar);free(g);free(before);
    }
    for(int invalid=0;invalid<2;invalid++) {
        fd2_field_game *g=calloc(1,sizeof(*g)),*before=malloc(sizeof(*before));fd2_archive ar;unsigned rolls=0;
        CHECK(g && before && fixture(g,&ar,&rolls)==0);
        fd2_static_flow_node nodes[]={
            {.id="side",.op="unit_field_write",.args={0,0,FD2_UNIT_SIDE,FD2_UNIT_SET,65537},.edges={1,-1,-1,-1,-1,-1,-1,3}},
            {.id="story",.op="story",.args={0},.edges={-1,-1,-1,-1,2,-1,-1,3}},
            {.id="end",.op="end",.edges={-1,-1,-1,-1,-1,-1,-1,-1}},
            {.id="fail",.op="fail",.edges={-1,-1,-1,-1,-1,-1,-1,-1}}};
        if(invalid){nodes[0].args[0]=nodes[0].args[1]=2;nodes[0].args[4]=1;}
        fd2_static_flow flow={"identity.bad.side",nodes,4,0};identity_probe probe={g->units.count,0,0};
        g->event_preflight=identity_resource_probe;g->event_preflight_userdata=&probe;*before=*g;
        CHECK(common_event_preflight_flow(g,&flow,0)==-1 && !probe.projected && !probe.actual && !rolls);
        CHECK(!memcmp(g,before,sizeof(*g)));
        fd2_field_game_close(g);fd2_archive_close(&ar);free(g);free(before);
    }
    return 0;
}
static int test_core_event_preflight_without_visual_callback_and_full_log(void) {
    fd2_field_game *g=calloc(1,sizeof(*g)),*before=malloc(sizeof(*before));fd2_archive ar;unsigned rolls=0;
    CHECK(g && before && fixture(g,&ar,&rolls)==0 && !g->event_preflight);
    static const fd2_static_flow_node nodes[]={
        {.id="move",.op="move",.args={7,0},.edges={1,-1,-1,-1,-1,-1,-1,2}},
        {.id="done",.op="end",.edges={-1,-1,-1,-1,-1,-1,-1,-1}},
        {.id="bad",.op="fail",.edges={-1,-1,-1,-1,-1,-1,-1,-1}}};
    static const fd2_static_flow bad_raw={"test.core.future.raw",nodes,3,0};
    *before=*g;
    CHECK(common_event_preflight_flow(g,&bad_raw,0)==-1 && !memcmp(g,before,sizeof(*g)) && !rolls);
    CHECK(common_event_bound_begin(g,&bad_raw,0)==-1 && !memcmp(g,before,sizeof(*g)) && !rolls);
    /* 合法callback4无大人数猜门；日志已满仍能只用父PC执行，无新notice。
     * 对照前面真实脚本raw坏例，不以撤销容量门冒充未知数据合法。 */
    for(size_t i=2;i<14;i++) {
        CHECK(fd2_field_units_set(&g->units,i,0,0,2,1,1)==0);
        g->units.items[i].hp=g->units.items[i].hp_max=10;
        ((uint8_t*)&g->units.items[i])[0x31]=0xff;
    }
    for(size_t i=1;i<14;i++)fd2_field_unit_set_acted(&g->units.items[i],1);
    g->event_log_count=FD2_FIELD_EVENT_LOG_CAPACITY;
    for(size_t i=0;i<g->event_log_count;i++)g->event_log[i].handled=1;
    g->metadata.turn_events[0]=(fd2_field_turn_event){(uint8_t)g->turn_number,4,1};
    *before=*g;
    CHECK(player_effect_tail_is_supported(g,0,NULL,0,0,0,g->units.items[0].x,g->units.items[0].y)==1 &&
        !memcmp(g,before,sizeof(*g)) && !rolls);
    const fd2_static_flow *flow=known_common_event_flow(g,4);CHECK(flow);
    CHECK(common_event_preflight_flow(g,flow,0)==0 && !memcmp(g,before,sizeof(*g)) && !rolls);
    CHECK(common_event_bound_begin(g,flow,0)==0 && g->units.items[13].side==1 &&
        g->event_log_count==FD2_FIELD_EVENT_LOG_CAPACITY && g->event_resume.active);
    CHECK(fd2_field_game_common_event_reply(g,FD2_FLOW_ACK)==0 && !g->event_resume.active && !rolls &&
        g->event_log_count==FD2_FIELD_EVENT_LOG_CAPACITY);
    fd2_field_game_close(g);fd2_archive_close(&ar);free(before);free(g);return 0;
}

int main(void){CHECK(test_linear_identity_preflight_retains_DWORD_and_write_domain()==0);
    CHECK(test_core_event_preflight_without_visual_callback_and_full_log()==0);
    CHECK(test_original_direction_reset_wait_and_caller_boundaries()==0);
    CHECK(test_source_physical_and_controller_clear_boundaries()==0);
    CHECK(test_initial_units_match_corrected_original_contract()==0);
    CHECK(test_cross_stage_cell_resources_and_cancel()==0);
    CHECK(test_cell_callback_full_word_cold_order()==0);
    CHECK(test_callback_late_bad_word_keeps_committed_prefix()==0);
    CHECK(test_defeat_exact_write_order_and_cancellation()==0);
    CHECK(test_defeat_three_callers_stop_at_host_quit()==0);
    CHECK(test_ai_physical_outer_refresh()==0);
    CHECK(test_initial_units_follow_shared_cold_choices()==0);
    CHECK(test_initial_units_reject_bad_or_missing_logic_definitions()==0);
    CHECK(test_defeated_item_and_money()==0);CHECK(test_full_swap_and_no()==0);CHECK(test_callback_and_story()==0);
    CHECK(test_common_callback_preflight_and_parent_child()==0);
    CHECK(test_common_child_host_quit_freezes_all_owners()==0);
    CHECK(test_ai_physical_child_does_not_finish_parent()==0);
    CHECK(test_wait_cell(0,0,0)==0);CHECK(test_wait_cell(0,0,1)==0);CHECK(test_wait_cell(0,1,1)==0);CHECK(test_wait_cell(1,0,1)==0);
    CHECK(test_resource_error_before_wait()==0);CHECK(test_music_and_message_owner()==0);
    CHECK(test_graph_choice_and_unknown_operation()==0);CHECK(test_invalid_reply_and_slot_are_read_only()==0);
    CHECK(test_cell_picker_cancel_and_money_variants()==0);
    CHECK(test_bad_edges_cannot_bypass_primitive_bounds_or_preflight()==0);
    CHECK(test_action_finish_kind_branch_comes_from_cold_flow()==0);
    CHECK(test_controller_rejection_preserves_existing_failure_boundary()==0);
    CHECK(test_descriptor_default_and_collector_sentinel()==0);
    puts("field_loot: data-driven branches, queued drops, callbacks, cells and write boundaries passed");return 0;}
