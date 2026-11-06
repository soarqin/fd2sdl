/* B5依据sub_1088D/11506/32D18/206C5及34778的直接业务回归；不宣称演出。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#x); return -1; } } while(0)

static int test_settlement(void) {
    fd2_field_game *g = calloc(1,sizeof(*g)); CHECK(g);
    g->ready=1;g->stage=0;g->active_side=2;g->interaction=FD2_FIELD_INTERACTION_BROWSE;
    g->roster_count=3;memset(g->roster,0xa5,sizeof(g->roster));
    for(size_t i=0;i<3;i++){ memset(&g->roster[i],0,sizeof(g->roster[i]));CHECK(fd2_field_unit_persistent_create(&g->roster[i],(uint8_t)i)==0); }
    g->units.count=4;
    g->units.items[0]=g->roster[0];g->units.items[0].flags=0x80;g->units.items[0].hp=1;g->units.items[0].mp=0;
    memset((uint8_t*)&g->units.items[0]+0x22,7,6);
    g->units.items[1]=g->roster[1];g->units.items[1].flags=0x81;g->units.items[1].hp=0;g->units.items[1].mp=0;
    g->units.items[2]=g->roster[2];g->units.items[2].text_id=0;g->units.items[2].flags=1; /* hidden text0不能覆盖hero */
    g->units.items[3]=g->roster[2];g->units.items[3].side=1;g->units.items[3].hp=2; /* 不按side2筛同步 */
    g->resource_value=12345;g->experience_value=77;g->message_number=998;
    g->option_values[0]=1;g->option_values[1]=0;g->option_values[2]=1;g->option_values[3]=0;
    fd2_save_manual_state out;memset(&out,0x6d,sizeof(out));
    fd2_field_unit roster[32];memcpy(roster,g->roster,sizeof(roster));
    CHECK(fd2_field_game_settlement_prepare(g,&out)==0);
    CHECK(!memcmp(g->roster,roster,sizeof(roster)) && g->stage==0 && g->ready);
    const fd2_field_unit *p=(const fd2_field_unit*)out.unit_area;
    CHECK(out.stage_id==1 && out.unit_count==3 && out.gold_or_flags==12345);
    CHECK(p[0].hp==p[0].hp_max && p[0].mp==p[0].mp_max && p[0].flags==0);
    CHECK(p[1].hp==0 && p[1].mp==p[1].mp_max && p[1].flags==1);
    CHECK(p[2].side==1 && p[2].hp==p[2].hp_max);
    for(size_t i=0;i<6;i++)CHECK(((const uint8_t*)&p[0])[0x22+i]==0);
    CHECK(!memcmp(out.unit_area+3*80,(uint8_t*)roster+3*80,sizeof(roster)-3*80));
    CHECK(out.options[0]==0 && out.options[1]==1 && out.options[2]==1 && out.options[3]==0);
    fd2_save_manual_state prior=out;
    ((uint8_t *)&g->units.items[3])[0x18]=0xc0;
    ((uint8_t *)&g->units.items[3])[0x19]=215;
    CHECK(fd2_field_game_settlement_prepare(g,&out)==-1 && !memcmp(&prior,&out,sizeof(out)));
    CHECK(!memcmp(g->roster,roster,sizeof(roster)) && g->ready && g->stage==0);
    CHECK(fd2_field_game_settlement_commit(g,&out)==-1 && !memcmp(&prior,&out,sizeof(out)));
    ((uint8_t *)&g->units.items[3])[0x18]=0x80;
    CHECK(fd2_field_game_settlement_commit(g,&out)==0 && !g->ready && g->stage==1);
    CHECK(g->experience_value==77 && g->message_number==998 && !memcmp(g->roster,out.unit_area,sizeof(g->roster)));
    CHECK(fd2_field_game_settlement_commit(g,&out)==-1);
    free(g);return 0;
}
static int test_stage1(void) {
    fd2_archive ar; CHECK(fd2_archive_open(&ar,"original_game/FDOTHER.DAT")==0);
    fd2_save_manual_state state={0};state.stage_id=1;state.unit_count=5;state.gold_or_flags=9876;
    memset(state.unit_area,0x5a,sizeof(state.unit_area));
    const uint8_t ids[5]={0,9,4,30,1};
    for(size_t i=0;i<5;i++){
        fd2_field_unit unit={0};CHECK(fd2_field_unit_persistent_create(&unit,ids[i])==0);
        unit.hp=unit.hp_max;memcpy(state.unit_area+i*80,&unit,80);
    }
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);
    CHECK(fd2_field_game_prepare_stage1(g,&ar,&state)==0);
    CHECK(g->stage==1 && g->turn_number==1 && g->active_side==2 && g->units.count==21 && g->roster_count==5);
    CHECK(g->resource_value==9876 && !memcmp(g->roster,state.unit_area,sizeof(g->roster)));
    static const uint8_t xy[5][2]={{20,14},{23,16},{22,13},{23,14},{24,15}};
    for(size_t i=0;i<5;i++)CHECK(g->units.items[i].x==xy[i][0] && g->units.items[i].y==xy[i][1] && g->units.items[i].unit_id==ids[i]);
    CHECK(fd2_field_game_group_count(g,1)==6 && fd2_field_game_group_count(g,2)==10);
    /* 原始模板/装备表与原版entry Save均确认这些派生值，不能只验身份/站位。 */
    CHECK(g->units.items[5].attack==3 && g->units.items[5].defense==8 &&
          g->units.items[5].accuracy==3 && g->units.items[5].evasion==3);
    CHECK(g->units.items[6].attack==3 && g->units.items[6].defense==11 &&
          g->units.items[6].accuracy==3 && g->units.items[6].evasion==3);
    for(size_t i=11;i<21;i++)
        CHECK(g->units.items[i].attack==24 && g->units.items[i].defense==4 &&
              g->units.items[i].accuracy==97 && g->units.items[i].evasion==2);
    CHECK(fd2_field_game_battle_result(g)==FD2_FIELD_BATTLE_ONGOING);
    g->turn_number=3;
    CHECK(fd2_field_game_end_active_phase(g)==0);
    CHECK(g->active_side==1 && g->units.count==27 && fd2_field_game_group_count(g,3)==6);
    for(size_t i=21;i<27;i++)
        CHECK(g->units.items[i].attack==31 && g->units.items[i].defense==5 &&
              g->units.items[i].accuracy==98 && g->units.items[i].evasion==3);
    CHECK(g->event_log_count==1 && g->event_log[0].action==6 && g->event_log[0].presentation_deferred);
    for(size_t i=5;i<=10;i++)CHECK(g->units.items[i].ai_param_35==26 && g->units.items[i].ai_param_36==15);
    g->event_log[0].presentation_deferred=0;
    CHECK(fd2_field_game_progress_resume(g)==0);
    for(size_t i=5;i<=10;i++)g->units.items[i].flags|=1;
    CHECK(fd2_field_game_battle_result(g)==FD2_FIELD_BATTLE_DEFEAT);
    fd2_field_game_close(g);
    CHECK(fd2_field_game_prepare_stage1(g,&ar,&state)==0);
    size_t bad_template=0;
    while(bad_template<g->metadata.unit_template_count &&
          g->metadata.unit_templates[bad_template].bytes[0x15]!=3)bad_template++;
    CHECK(bad_template<g->metadata.unit_template_count);
    g->metadata.unit_templates[bad_template].bytes[5]=215; /* 未知装备派生不可提交半组。 */
    fd2_field_units before_group=g->units;
    g->turn_number=3;
    CHECK(fd2_field_game_end_player_turn(g)==-1);
    CHECK(!memcmp(&before_group,&g->units,sizeof(before_group)));
    fd2_field_game_close(g);
    state.unit_count=4;
    CHECK(fd2_field_game_prepare_stage1(g,&ar,&state)==0);
    fd2_field_unit empty={0};empty.flags=1;empty.x=254; /* 原zero槽仍被1366a向左2步。 */
    CHECK(!memcmp(&g->units.items[4],&empty,sizeof(empty)));
    fd2_field_handoff handoff={0};handoff.valid=1;handoff.stage=1;handoff.units=g->units;
    handoff.camera_cell_x=g->camera_cell_x;handoff.camera_cell_y=g->camera_cell_y;
    handoff.focus_cell_x=g->cursor_cell_x;handoff.focus_cell_y=g->cursor_cell_y;
    CHECK(fd2_field_game_apply_handoff(g,&handoff)==0 && g->stage_runtime_entry_offset==0x238du);
    fd2_field_game_close(g);state.unit_count=0;
    CHECK(fd2_field_game_prepare_stage1(g,&ar,&state)==-1 && !g->ready && !g->map.cells);
    state.unit_count=17;CHECK(fd2_field_game_prepare_stage1(g,&ar,&state)==-1);
    free(g);fd2_archive_close(&ar);return 0;
}
static int test_script_bytes(void) {
    /* 与原始EXE连续299B独立比对；canonicalVA62a3d对应已核file7ba51。 */
    const size_t sizes[11]={49,25,35,39,45,5,13,25,21,15,27};
    uint8_t bytes[299];FILE*f=fopen("original_game/FD2.EXE","rb");CHECK(f);
    CHECK(!fseek(f,0x7ba51L,SEEK_SET) && fread(bytes,1,sizeof(bytes),f)==sizeof(bytes));CHECK(!fclose(f));
    size_t at=0;for(size_t i=0;i<11;i++){const uint8_t*script=NULL;size_t size=0;
        CHECK(fd2_field_movement_script_get((uint8_t)(9+i),&script,&size)==0 && size==sizes[i]);
        CHECK(!memcmp(script,bytes+at,size));at+=size;}
    const uint8_t*script=(const uint8_t*)1;size_t size=1;
    CHECK(fd2_field_movement_script_get(20,&script,&size)==-1 && !script && !size);
    return 0;
}
static int test_group_landing(void){
    fd2_field_unit_template t={{0}};t.bytes[0]=1;t.bytes[1]=134;t.bytes[4]=3;t.bytes[5]=0xff;t.bytes[6]=0x80;
    for(size_t i=7;i<13;i++)t.bytes[i]=0xff;t.bytes[0x15]=1;t.bytes[0x16]=0xff;
    fd2_field_metadata m={0};m.unit_templates=&t;m.unit_template_count=1;
    fd2_field_placement pos={1,1,134};fd2_field_placements p={1,&pos};
    uint32_t cells[9]={0};fd2_field_map map={3,3,cells};fd2_field_units units={0};
    for(int y=0;y<3;y++)for(int x=0;x<3;x++){if(y==1 && x!=1)continue;
        CHECK(fd2_field_units_set(&units,units.count,0,0,2,x,y)==0);}
    fd2_field_units before=units;
    CHECK(fd2_field_units_append_group_placed(&units,&m,&p,&map,1,0)==0);
    CHECK(units.count==before.count+1 && units.items[before.count].x==2 && units.items[before.count].y==1);
    units=before;CHECK(fd2_field_units_append_group_placed(&units,&m,&p,&map,1,1)==0);
    CHECK(units.items[before.count].x==1 && units.items[before.count].y==1 && units.items[before.count].flags==0);
    units=before;t.bytes[1]=255;CHECK(fd2_field_units_append_group_placed(&units,&m,&p,&map,1,0)==-1);
    CHECK(!memcmp(&units,&before,sizeof(units)));return 0;
}
static int test_stage1_settlement_stage2(void) {
    fd2_archive ar={0};CHECK(fd2_archive_open(&ar,"original_game/FDOTHER.DAT")==0);
    fd2_save_manual_state state={0};state.stage_id=1;state.unit_count=5;state.gold_or_flags=321;
    memset(state.unit_area,0x5a,sizeof(state.unit_area));
    const uint8_t ids[5]={0,9,4,30,1};
    for(size_t i=0;i<5;i++) {
        fd2_field_unit u={0};CHECK(fd2_field_unit_persistent_create(&u,ids[i])==0);
        memcpy(state.unit_area+i*sizeof(u),&u,sizeof(u));
    }
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);
    CHECK(fd2_field_game_prepare_stage1(g,&ar,&state)==0);
    fd2_field_units projected,prior;memset(&projected,0x3d,sizeof(projected));prior=projected;
    for(size_t i=11;i<21;i++)g->units.items[i].flags|=1;
    CHECK(fd2_field_game_settlement_project_units(g,&projected)==-1);
    CHECK(!memcmp(&projected,&prior,sizeof(prior))); /* 未经turn3必经事件不得提前结算。 */
    fd2_field_game_close(g);CHECK(fd2_field_game_prepare_stage1(g,&ar,&state)==0);
    g->turn_number=3;CHECK(fd2_field_game_end_active_phase(g)==0 && g->units.count==27);
    g->event_log[0].presentation_deferred=0;CHECK(fd2_field_game_progress_resume(g)==0);
    for(size_t i=11;i<27;i++)g->units.items[i].flags|=1;
    /* 奖励可给hidden玩家；raw0全满，raw1首空为slot3。 */
    for(size_t i=0;i<g->units.count;i++)for(size_t slot=0;slot<8;slot++) {
        uint8_t *r=(uint8_t*)&g->units.items[i];r[10+2*slot]=0;r[11+2*slot]=192;
    }
    g->units.items[1].flags|=1;((uint8_t*)&g->units.items[1])[16]=0x80;
    fd2_field_units before=g->units;
    CHECK(fd2_field_game_settlement_project_units(g,&projected)==0 && projected.count==28);
    CHECK(!memcmp(&g->units,&before,sizeof(before)));
    CHECK(((uint8_t*)&projected.items[1])[16]==0 && ((uint8_t*)&projected.items[1])[17]==198);
    for(size_t i=0;i<5;i++)CHECK(projected.items[i].direction==0);
    CHECK(projected.items[27].unit_id==8 && projected.items[27].direction==2);
    fd2_save_manual_state out;memset(&out,0x6b,sizeof(out));
    CHECK(fd2_field_game_settlement_prepare(g,&out)==0 && out.stage_id==2 && out.unit_count==6);
    CHECK(out.gold_or_flags==321 && !memcmp(g->roster,state.unit_area,sizeof(g->roster)));
    const fd2_field_unit *roster=(const fd2_field_unit*)out.unit_area;
    CHECK(roster[5].unit_id==8 && roster[5].hp==roster[5].hp_max && roster[5].direction==2);
    CHECK(!memcmp(out.unit_area+6*80,state.unit_area+6*80,sizeof(state.unit_area)-6*80));
    /* 有村民死亡：fragment7分支不追加198；背包全满也必须静默保留库存。 */
    g->units.items[5].flags|=1;
    CHECK(fd2_field_game_settlement_project_units(g,&prior)==0 && ((uint8_t*)&prior.items[1])[16]==0x80);
    g->units.items[5].flags&=(uint8_t)~1u;((uint8_t*)&g->units.items[1])[16]=0;
    CHECK(fd2_field_game_settlement_project_units(g,&prior)==0);
    for(size_t i=0;i<27;i++)CHECK(!memcmp((uint8_t*)&prior.items[i]+10,(uint8_t*)&g->units.items[i]+10,16));
    ((uint8_t*)&g->units.items[1])[16]=0x80;
    /* 坏group4装备、满roster均在写out和live之前拒绝。 */
    size_t index=0;while(index<g->metadata.unit_template_count && g->metadata.unit_templates[index].bytes[0x15]!=4)index++;
    CHECK(index<g->metadata.unit_template_count);
    uint8_t equipment=g->metadata.unit_templates[index].bytes[5];g->metadata.unit_templates[index].bytes[5]=215;
    fd2_save_manual_state oldout=out;
    CHECK(fd2_field_game_settlement_commit(g,&out)==-1 && !memcmp(&oldout,&out,sizeof(out)) && g->ready);
    g->metadata.unit_templates[index].bytes[5]=equipment;g->roster_count=32;
    CHECK(fd2_field_game_settlement_prepare(g,&out)==-1 && !memcmp(&oldout,&out,sizeof(out)));
    g->roster_count=5;CHECK(fd2_field_game_settlement_commit(g,&out)==0 && !g->ready && g->stage==2 && g->roster_count==6);
    CHECK(fd2_field_game_settlement_commit(g,&out)==-1);
    fd2_field_game_close(g);
    CHECK(fd2_field_game_validate_manual_state(&out)==0);
    CHECK(fd2_field_game_prepare_stage2(g,&ar,&out)==0);
    CHECK(g->stage==2 && g->units.count==15 && g->roster_count==6 && g->map.width==18 && g->map.height==26);
    CHECK(g->metadata.shape_index==2 && g->metadata.unknown_01==6 && g->metadata.unit_template_count==40);
    CHECK(g->stage_runtime_entry_offset==0x23ac && g->ordinary_display_mode==1);
    CHECK(!memcmp(g->roster,out.unit_area,sizeof(g->roster)) && (g->units.items[1].flags&1u));
    for(size_t i=0;i<15;i++)CHECK(g->units.items[i].direction==0);
    fd2_save_battle_snapshot snap;memset(&snap,0xa6,sizeof(snap));
    CHECK(fd2_field_game_export_battle_snapshot(g,&snap)==0);
    CHECK(fd2_field_game_import_battle_snapshot(g,&snap)==0 && g->stage_runtime_entry_offset==0x23ac);
    CHECK(g->ordinary_display_mode==1 && !memcmp(g->roster,out.unit_area,sizeof(g->roster)));
    /* 第二关已确认入口/浏览/存读档，未知turn3事件不由按钮或自动WAIT绕过。 */
    prior=g->units;uint32_t turn=g->turn_number;uint8_t side=g->active_side;
    CHECK(fd2_field_game_end_player_turn(g)==-1 && g->turn_number==turn && g->active_side==side);
    CHECK(fd2_field_game_begin_player_controller(g,0)==-1 && !memcmp(&prior,&g->units,sizeof(prior)));
    CHECK(fd2_field_game_complete_player_controller(g,0)==-1 && !memcmp(&prior,&g->units,sizeof(prior)));
    snap.meta[FD2_SAVE_BATTLE_META_UNIT_COUNT]=14;prior=g->units;
    CHECK(fd2_field_game_import_battle_snapshot(g,&snap)==-1 && !memcmp(&prior,&g->units,sizeof(prior)));
    fd2_field_game_close(g);out.unit_count=17;
    CHECK(fd2_field_game_prepare_stage2(g,&ar,&out)==-1 && !g->ready && !g->map.cells);
    free(g);fd2_archive_close(&ar);return 0;
}
int main(void){CHECK(test_settlement()==0);CHECK(test_stage1()==0);CHECK(test_script_bytes()==0);CHECK(test_group_landing()==0);CHECK(test_stage1_settlement_stage2()==0);puts("field_stage_transition: settlement projections, village branches and real stage1/2 entries passed");return 0;}
