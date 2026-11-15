/* S3正式source入口、结果1197b续行及只读准备交接。
 * 原函数结果另由完整机器码oracle给expected；此处验证真实consumer写序/
 * 无副作用与owner门，不启动GUI，不宣称自然战斗或原版动态对照。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"

#define CHECK(x) do { if(!(x)){fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x);return -1;} }while(0)
static unsigned rng_calls;
static uint32_t rng(void *p){(*(unsigned *)p)++;return 1;}
static void setup(fd2_field_game *game,size_t stage,size_t count) {
    memset(game,0,sizeof(*game));game->ready=1;game->stage=stage;game->units.count=count;
    game->active_side=2;game->selected_unit=game->command_selected=-1;
    game->interaction=FD2_FIELD_INTERACTION_BROWSE;game->turn_number=1;
    rng_calls=0;game->attack_rng=rng;game->attack_rng_userdata=&rng_calls;
    for(size_t i=0;i<count;i++){game->units.items[i].side=1;game->units.items[i].hp=100;}
    if(count)game->units.items[0].side=2;
    if(count>1)game->units.items[1].side=0;
}
static int prepare_boundary(const fd2_archive *other) {
    fd2_field_game *game=calloc(1,sizeof(*game));CHECK(game);
    fd2_save_manual_state state={0};state.stage_id=2;state.unit_count=6;
    static const uint8_t ids[]={0,9,4,30,1,8};
    for(size_t i=0;i<6;i++) {
        fd2_field_unit unit={0};CHECK(fd2_field_unit_persistent_create(&unit,ids[i])==0);
        memcpy(state.unit_area+i*sizeof(unit),&unit,sizeof(unit));
    }
    state.unit_area[sizeof(state.unit_area)-1]=0x9a;
    CHECK(fd2_field_game_prepare_stage(game,other,&state)==0 && game->ready && !game->units.count);
    CHECK(game->stage_runtime_entry_offset==0x238du+31u && game->roster_count==6);
    CHECK(!memcmp(game->roster,state.unit_area,sizeof(game->roster)));
    fd2_field_game_close(game);
    CHECK(fd2_field_game_prepare_stage2(game,other,&state)==0 && !game->units.count);
    CHECK(game->stage_runtime_entry_offset==0x238du+31u);fd2_field_game_close(game);
    CHECK(fd2_field_game_prepare_stage_source_flow(game,other,&state,"stage.source.entry.002")==0);
    CHECK(!game->units.count && !game->stage_runtime_entry_offset);fd2_field_game_close(game);
    state.stage_id=3;CHECK(fd2_field_game_prepare_stage2(game,other,&state)==-1 && !game->ready);
    free(game);return 0;
}
static int preparation_export(void) {
    fd2_field_game *game=malloc(sizeof(*game)),*before=malloc(sizeof(*before));CHECK(game && before);
    setup(game,3,0);game->roster_count=FD2_SAVE_BATTLE_MAX_ROSTER;game->resource_value=0xabcdef;
    for(size_t i=0;i<sizeof(game->roster);i++)((uint8_t *)game->roster)[i]=(uint8_t)(i*13u+7u);
    for(size_t i=0;i<4;i++)game->option_values[i]=(uint8_t)(0xf0u+i);
    fd2_save_manual_state state;*before=*game;
    CHECK(fd2_field_game_export_preparation(game,&state)==0 && !memcmp(game,before,sizeof(*game)));
    CHECK(state.stage_id==3 && state.unit_count==32 && state.gold_or_flags==0xabcdef);
    CHECK(!memcmp(state.unit_area,game->roster,sizeof(game->roster)));
    CHECK(state.options[0]==0xf3 && state.options[1]==0xf2 && state.options[2]==0xf0 && state.options[3]==0xf1);
    /* S5已接1..29及NoTown准备；此处只核export owner/门，不签自然roster。 */
    for(size_t stage=1;stage<=29;stage++) {
        *game=*before;game->stage=stage;
        CHECK(fd2_field_game_export_preparation(game,&state)==0 && state.stage_id==stage);
        CHECK(state.unit_count==32 && !memcmp(state.unit_area,game->roster,sizeof(game->roster)));
    }
    for(int bad=0;bad<4;bad++) {
        *game=*before;if(bad==0)game->event_resume.active=1;if(bad==1)game->roster_count=33;
        if(bad==2)game->stage=30;if(bad==3)game->ready=0;
        memset(&state,0xa5,sizeof(state));fd2_save_manual_state unchanged=state;
        CHECK(fd2_field_game_export_preparation(game,&state)==-1 && !memcmp(&state,&unchanged,sizeof(state)));
    }
    free(game);free(before);return 0;
}
static int conditional_domains(void) {
    fd2_field_game *game=malloc(sizeof(*game)),*before=malloc(sizeof(*before));CHECK(game && before);
    for(unsigned turn=5;turn<=6;turn++) {
        setup(game,12,59);game->turn_number=turn;*before=*game;
        int result=fd2_field_game_common_event_begin(game,"common.result.source_020765",0);
        if(turn==5)CHECK(result==0 && !game->event_resume.active && game->battle_result==FD2_FIELD_BATTLE_ONGOING);
        else CHECK(result==-1 && !memcmp(game,before,sizeof(*game)));
    }
    for(unsigned turn=6;turn<=7;turn++) {
        setup(game,18,64);game->turn_number=turn;*before=*game;fd2_field_battle_result result=FD2_FIELD_BATTLE_DEFEAT;
        int status=fd2_field_game_result_query_flow(game,"stage.result.source_020926",&result);
        if(turn==6)CHECK(status==0 && result==FD2_FIELD_BATTLE_ONGOING);
        else CHECK(status==-1 && result==FD2_FIELD_BATTLE_DEFEAT);
        CHECK(!memcmp(game,before,sizeof(*game)));
    }
    for(int found=0;found<2;found++) {
        setup(game,16,52);game->roster_count=1;game->roster[0].text_id=found?18:17;
        game->roster[0].flags=1;*before=*game;
        int result=fd2_field_game_common_event_begin(game,"common.result.source_020872",0);
        if(found)CHECK(result==0 && !game->event_resume.active && game->battle_result==FD2_FIELD_BATTLE_ONGOING);
        else CHECK(result==-1 && !memcmp(game,before,sizeof(*game)));
    }
    free(game);free(before);return 0;
}
static int post_result(void) {
    static const size_t stages[]={12,16,19},counts[]={59,53,83};
    static const int fragments[]={10,2,10};
    static const fd2_field_battle_result at_story[]={FD2_FIELD_BATTLE_DEFEAT,FD2_FIELD_BATTLE_ONGOING,FD2_FIELD_BATTLE_DEFEAT};
    static const fd2_field_battle_result after_ack[]={FD2_FIELD_BATTLE_DEFEAT,FD2_FIELD_BATTLE_DEFEAT,FD2_FIELD_BATTLE_VICTORY};
    fd2_field_game *game=malloc(sizeof(*game)),*before=malloc(sizeof(*before));CHECK(game && before);
    for(size_t which=0;which<3;which++) {
        setup(game,stages[which],counts[which]);
        if(which==0)for(size_t i=15;i<=26;i++)game->units.items[i].flags=1;
        if(which==1)game->units.items[52].flags=1;
        if(which==2) {
            for(size_t i=36;i<=82;i++)if(i!=52)game->units.items[i].flags=1;
        }
        CHECK(fd2_field_game_complete_player_controller(game,0)==0);
        CHECK(game->event_resume.active && game->event_resume.completion==FD2_EVENT_COMPLETE_PLAYER_RESULT);
        CHECK(game->battle_result==at_story[which] && !rng_calls);
        fd2_static_flow_node node;size_t actor;
        CHECK(fd2_field_game_common_event_peek(game,&node,&actor)==1 && actor==0 &&
            !strcmp(node.op,"story") && node.args[0]==fragments[which]);
        *before=*game;
        for(size_t n=0;n<8;n++)CHECK(fd2_field_game_battle_result(game)==at_story[which]);
        CHECK(!memcmp(game,before,sizeof(*game)) && !rng_calls);
        CHECK(fd2_field_game_common_event_reply(game,FD2_FLOW_ACK)==0);
        CHECK(!game->event_resume.active && !game->event_resume.completion && game->battle_result==after_ack[which]);
        CHECK(!game->player_post_waiting_phase && !game->pending_cell_event.valid && !rng_calls);
        *before=*game;CHECK(fd2_field_game_common_event_reply(game,FD2_FLOW_ACK)==-1 && !memcmp(game,before,sizeof(*game)));
    }
    /* story之前已经提交的latch保留；HOST不ACK晚写，也不重进1197b。 */
    setup(game,16,53);game->units.items[52].flags=1;
    CHECK(fd2_field_game_complete_player_controller(game,0)==0 && game->event_resume.active);
    game->runtime_host_quit=1;*before=*game;
    CHECK(fd2_field_game_common_event_reply(game,FD2_FLOW_ACK)==-1 && !memcmp(game,before,sizeof(*game)));
    game->runtime_host_quit=0;
    CHECK(fd2_field_game_common_event_reply(game,FD2_FLOW_ERROR)==-1 && game->battle_result==FD2_FIELD_BATTLE_ONGOING);
    CHECK(game->failure!=FD2_FIELD_FAILURE_NONE && !rng_calls);
    free(game);free(before);return 0;
}
int main(void) {
    fd2_archive other={0};CHECK(fd2_archive_open(&other,"original_game/FDOTHER.DAT")==0);
    int result=prepare_boundary(&other) || preparation_export() || conditional_domains() || post_result();
    fd2_archive_close(&other);if(result)return 1;
    puts("S3 Game source/result/handoff contract tests passed");return 0;
}
