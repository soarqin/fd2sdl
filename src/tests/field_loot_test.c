/* field_loot_descriptors_execute @0x1aa1d（原sub_1AA1D） / field_special_cell_execute @0x190ac（原sub_190AC） / field_completed_chest_tiles_apply @0x12263（原sub_12263）。
 * 私有TU只驱动业务停点；不声称消息、声音或原版演出时序。 */
#define fd2_static_flow_lookup loot_test_flow_lookup
#include "../field_game.c"
#undef fd2_static_flow_lookup
#include <stdio.h>
const fd2_static_flow *fd2_static_flow_lookup(const char *domain, const char *id);
static const fd2_static_flow *loot_test_override;
const fd2_static_flow *loot_test_flow_lookup(const char *domain, const char *id) {
    if (loot_test_override && !strcmp(domain, "loot") && !strcmp(id, loot_test_override->id)) return loot_test_override;
    return fd2_static_flow_lookup(domain, id);
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
    CHECK(g->units.items[13].side==1 && g->loot.notice.kind==FD2_FIELD_LOOT_FIELD_EVENT && g->event_log_count==1);
    g->event_log[0].presentation_deferred=0;
    CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0);
    CHECK(g->loot.notice.kind==FD2_FIELD_LOOT_STORY && g->loot.notice.fragment==8);
    CHECK(reply(g,FD2_FIELD_LOOT_ACK,-1)==0 && calls==0);
    fd2_field_game_close(g);fd2_archive_close(&ar);free(g);return 0;
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
static int test_action_finish_kind_branch_comes_from_cold_flow(void) {
    const fd2_static_flow *source = fd2_static_flow_lookup("loot", "action.finish");
    CHECK(source && source->count);
    fd2_static_flow_node *nodes = malloc(source->count * sizeof(*nodes)); CHECK(nodes);
    memcpy(nodes, source->nodes, source->count * sizeof(*nodes));
    int branch = fd2_static_flow_label(source, "kind.physical");
    int mark = fd2_static_flow_label(source, "action.acted"); CHECK(branch >= 0 && mark >= 0);
    nodes[branch].edges[FD2_FLOW_YES] = (int16_t)mark;
    fd2_static_flow changed = *source; changed.nodes = nodes;
    for (int modified = 0; modified < 2; modified++) {
        fd2_field_game *g = calloc(1, sizeof(*g)); fd2_archive ar; unsigned calls = 0;
        CHECK(fixture(g, &ar, &calls) == 0);
        g->units.items[0].direction = 3; g->units.items[1].direction = 2;
        loot_test_override = modified ? &changed : NULL;
        CHECK(finish_unit_action_internal(g, PLAYER_ACTION_PHYSICAL, 0) == 0);
        CHECK(g->selected_unit == -1 && g->interaction == FD2_FIELD_INTERACTION_BROWSE);
        CHECK(g->units.items[0].direction == (modified ? 3 : 0));
        CHECK(g->units.items[1].direction == (modified ? 2 : 0));
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
int main(void){CHECK(test_defeated_item_and_money()==0);CHECK(test_full_swap_and_no()==0);CHECK(test_callback_and_story()==0);
    CHECK(test_wait_cell(0,0,0)==0);CHECK(test_wait_cell(0,0,1)==0);CHECK(test_wait_cell(0,1,1)==0);CHECK(test_wait_cell(1,0,1)==0);
    CHECK(test_resource_error_before_wait()==0);CHECK(test_music_and_message_owner()==0);
    CHECK(test_graph_choice_and_unknown_operation()==0);CHECK(test_invalid_reply_and_slot_are_read_only()==0);
    CHECK(test_cell_picker_cancel_and_money_variants()==0);
    CHECK(test_bad_edges_cannot_bypass_primitive_bounds_or_preflight()==0);
    CHECK(test_action_finish_kind_branch_comes_from_cold_flow()==0);
    CHECK(test_controller_rejection_preserves_existing_failure_boundary()==0);
    puts("field_loot: data-driven branches, queued drops, callbacks, cells and write boundaries passed");return 0;}
