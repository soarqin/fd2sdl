/* 自然策略纯回归；metadata/script来自原资源，24x24草地/单位为合成fixture。
 * 不是原版战斗验收，不写SAV或复现原RNG。 */
#define main natural_planner_entry
#include "field_natural_plan.c"
#undef main
#define CHECK(x) do {if(!(x)){fprintf(stderr,"CHECK %s:%d %s\n",__FILE__,__LINE__,#x);return -1;}}while(0)

static int fixture_unit(fd2_field_game *g,size_t raw,uint8_t id,uint8_t side,int x,int y) {
    fd2_field_unit *u=&g->units.items[raw];
    CHECK(fd2_field_unit_init(u,id,id,side,x,y)==0);
    u->hp=u->hp_max=42;u->movement_points=4;u->movement_profile=1;
    u->attack=24;u->defense=12;u->accuracy=100;
    uint8_t *bytes=(uint8_t *)u;bytes[10]=0x40;bytes[11]=0;
    return 0;
}
static int test_hero_rest_and_primary_keep_known_and_future_coverage_minimal(void) {
    fd2_archive ar={0};CHECK(fd2_archive_open(&ar,"original_game/FDOTHER.DAT")==0);
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);
    CHECK(fd2_field_game_open_resources(g,&ar,0)==0);
    uint32_t grass=fd2_field_map_cell(&g->map,7,14)&1023u;
    for(int i=0;i<24*24;i++)g->map.cells[i]=grass;
    fd2_field_units_clear(&g->units);g->units.count=18;
    for(size_t i=0;i<18;i++)g->units.items[i].flags=1;
    CHECK(fixture_unit(g,0,0,2,3,16)==0);
    CHECK(fixture_unit(g,4,96,0,3,14)==0);
    CHECK(fixture_unit(g,6,96,0,3,13)==0);
    CHECK(fixture_unit(g,13,3,2,11,12)==0);
    g->turn_number=5;g->active_side=2;g->interaction=FD2_FIELD_INTERACTION_BROWSE;
    fd2_field_rng rng;fd2_field_rng_seed(&rng,0x7a18);
    CHECK(fd2_field_game_set_attack_hooks(g,NULL,NULL,NULL,NULL,fd2_field_rng_next,&rng)==0);
    fd2_field_game *before=malloc(sizeof(*g));CHECK(before);*before=*g;uint16_t seed=rng.state;
    fd2_field_units future;CHECK(potential_units(g,&future)==0 && future.count==27);
    CHECK(future.items[18].unit_id==97 && future.items[18].x==3 && future.items[18].y==21);
    CHECK(threat_coverage(&future,3,16)>threat_coverage(&g->units,3,16));
    natural_action rest;CHECK(plan_rest(g,0,&rest)==0);
    fd2_field_path_result range=FD2_FIELD_PATH_RESULT_INITIALIZER;
    CHECK(fd2_field_game_compute_move_range(g,0,&range)==0);
    int minimum=INT_MAX;
    for(int y=0;y<24;y++)for(int x=0;x<24;x++)if(fd2_field_path_is_destination(&range,x,y)) {
        int n=threat_coverage(&future,x,y);if(n<minimum)minimum=n;
    }
    CHECK(fd2_field_path_is_destination(&range,rest.x,rest.y));
    CHECK(threat_coverage(&future,rest.x,rest.y)==minimum);
    CHECK(rest.x!=3 || rest.y!=16);
    fd2_field_game *query=malloc(sizeof(*g));CHECK(query);*query=*g;
    natural_action primary={PLAN_ATTACK,13,4,rest.x,rest.y,-1,-1,999,999};
    project_primary_move(query,&primary);
    CHECK(!memcmp(&query->units.items[4],&g->units.items[4],sizeof(fd2_field_unit)));
    CHECK(query->units.items[4].hp==42 && !(query->units.items[4].flags&1)); /* 未假定primary击杀。 */
    natural_action alternate;CHECK(plan_rest(query,0,&alternate)==0);
    CHECK(alternate.x!=primary.x || alternate.y!=primary.y);
    CHECK(!memcmp(g,before,sizeof(*g)) && rng.state==seed);
    query->units.items[13].flags|=0x80;
    natural_choices choices;CHECK(choose(query,&choices)==0 && choices.count);
    CHECK(choices.actions[0].actor==0);
    CHECK(potential_units(query,&future)==0);
    fd2_field_path_close(&range);CHECK(fd2_field_game_compute_move_range(query,0,&range)==0);
    minimum=INT_MAX;
    for(int y=0;y<24;y++)for(int x=0;x<24;x++)if(fd2_field_path_is_destination(&range,x,y)) {
        int n=threat_coverage(&future,x,y);if(n<minimum)minimum=n;
    }
    for(size_t i=0;i<choices.count;i++)CHECK(threat_coverage(&future,choices.actions[i].x,choices.actions[i].y)==minimum);
    /* 魔法射程只作保守覆盖，不把普通攻击低估伤害当作安全保证。 */
    fd2_field_units spells={0};spells.count=1;spells.items[0]=g->units.items[4];
    ((uint8_t *)&spells.items[0])[10]=0x80;((uint8_t *)&spells.items[0])[0x1a]=1;spells.items[0].mp=2;
    CHECK(threat_coverage(&spells,10,14)==1); /* 移动4+Fire cast5，距离7。 */
    CHECK(!memcmp(g,before,sizeof(*g)) && rng.state==seed);
    fd2_field_path_close(&range);free(query);free(before);fd2_field_game_close(g);free(g);fd2_archive_close(&ar);
    return 0;
}
static int test_stage1_preserves_hidden_roster_and_plans_real_dimensions(void) {
    fd2_archive ar={0};CHECK(fd2_archive_open(&ar,"original_game/FDOTHER.DAT")==0);
    fd2_save_manual_state state={0};state.stage_id=1;state.unit_count=5;
    const uint8_t ids[5]={0,9,4,30,1};
    for(size_t i=0;i<5;i++) {
        fd2_field_unit u={0};CHECK(fd2_field_unit_persistent_create(&u,ids[i])==0);
        if(i==1 || i==2 || i==4)u.flags|=1;
        memcpy(state.unit_area+i*sizeof(u),&u,sizeof(u));
    }
    fd2_field_game *g=calloc(1,sizeof(*g));CHECK(g);
    CHECK(fd2_field_game_prepare_stage1(g,&ar,&state)==0 && g->map.width==27 && g->map.height==21);
    fd2_field_game *before=malloc(sizeof(*g));CHECK(before);*before=*g;
    fd2_field_units future;CHECK(potential_units(g,&future)==0 && future.count==27);
    CHECK(!memcmp(future.items,g->units.items,21*sizeof(fd2_field_unit)));
    natural_choices choices;CHECK(choose(g,&choices)==0 && choices.count>0);
    for(size_t i=0;i<choices.count;i++) {
        CHECK(choices.actions[i].actor==0 || choices.actions[i].actor==3);
        CHECK(choices.actions[i].x>=0 && choices.actions[i].x<27 && choices.actions[i].y>=0 && choices.actions[i].y<21);
    }
    CHECK(!memcmp(before,g,sizeof(*g)));
    g->units=future;CHECK(potential_units(g,&future)==0 && future.count==27);
    g->units.count=22;CHECK(potential_units(g,&future)==-1);
    free(before);fd2_field_game_close(g);free(g);fd2_archive_close(&ar);return 0;
}
int main(void) {return test_hero_rest_and_primary_keep_known_and_future_coverage_minimal()==0 &&
    test_stage1_preserves_hidden_roster_and_plans_real_dimensions()==0?0:1;}
