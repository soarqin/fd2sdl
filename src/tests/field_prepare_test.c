/* G3纯领域：2af28/2b777、2a43e、2ae0e/2aa00/2ac7d的原字段与分段边界。
 * 金额/成长golden取G1原EXE读窗；不启动SDL/原版或声称服务运行已验。 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "field_prepare.h"
#include "field_prepare_item.h"
#include "field_unit.h"

#define CHECK(e) do { if(!(e)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#e);return -1;} } while(0)
static void put(fd2_save_manual_state *s,size_t i,const fd2_field_unit *u) {memcpy(s->unit_area+i*80u,u,80);}
static fd2_field_unit get(const fd2_save_manual_state *s,size_t i) {fd2_field_unit u;memcpy(&u,s->unit_area+i*80u,80);return u;}
static void word(uint8_t *p,uint16_t n) {p[0]=(uint8_t)n;p[1]=(uint8_t)(n>>8);}
static void unit_init(fd2_field_unit *u,uint8_t id) {
    for(size_t i=0;i<80;i++)((uint8_t *)u)[i]=(uint8_t)(i*7u+id);
    u->unit_id=id;u->text_id=(uint8_t)(id+70u);u->movement_profile=1;u->level=20;
    u->flags=0xa1;u->hp=17;u->hp_max=73;u->mp=11;u->mp_max=37;
    u->attack_status=u->defense_status=u->hit_evasion_status=0;
    for(size_t i=0;i<8;i++){((uint8_t *)u)[10+2*i]=128;((uint8_t *)u)[11+2*i]=(uint8_t)(200+i);}
    ((uint8_t *)u)[10]=64;((uint8_t *)u)[11]=0;
    ((uint8_t *)u)[12]=64;((uint8_t *)u)[13]=128;
}
static void state_init(fd2_save_manual_state *s,uint8_t count) {
    memset(s,0xa6,sizeof(*s));s->stage_id=1;s->unit_count=count;s->gold_or_flags=10000;
    for(size_t i=0;i<count;i++){fd2_field_unit u;unit_init(&u,(uint8_t)i);put(s,i,&u);}
}
static int test_selection_navigation_and_32nd_bit(void) {
    uint8_t selected[31];memset(selected,0xcc,sizeof(selected));
    CHECK(fd2_field_prepare_selection_init(selected)==0);
    for(size_t i=0;i<31;i++)CHECK(!selected[i]);
    int cursor=0;size_t n=99;
    CHECK(fd2_field_prepare_selection_step(32,selected,&cursor,FD2_FIELD_PREPARE_SELECT_LEFT)==1 && cursor==30);
    CHECK(fd2_field_prepare_selection_step(32,selected,&cursor,FD2_FIELD_PREPARE_SELECT_TOGGLE)==1 && cursor==0 && selected[30]==1);
    CHECK(fd2_field_prepare_selection_count(32,selected,&n)==0 && n==1);
    CHECK(fd2_field_prepare_selection_step(32,selected,&cursor,FD2_FIELD_PREPARE_SELECT_DOWN)==1 && cursor==10);
    CHECK(fd2_field_prepare_selection_step(32,selected,&cursor,FD2_FIELD_PREPARE_SELECT_DOWN)==1 && cursor==20);
    CHECK(fd2_field_prepare_selection_step(32,selected,&cursor,FD2_FIELD_PREPARE_SELECT_DOWN)==1 && cursor==30);
    CHECK(fd2_field_prepare_selection_step(32,selected,&cursor,FD2_FIELD_PREPARE_SELECT_DOWN)==0 && cursor==30);
    CHECK(fd2_field_prepare_selection_step(32,selected,&cursor,FD2_FIELD_PREPARE_SELECT_UP)==1 && cursor==20);
    cursor=9;CHECK(fd2_field_prepare_selection_step(32,selected,&cursor,FD2_FIELD_PREPARE_SELECT_UP)==0 && cursor==9);
    uint8_t before[31];memcpy(before,selected,31);int old=cursor;
    CHECK(fd2_field_prepare_selection_step(32,selected,&cursor,(fd2_field_prepare_select_key)99)==-1 && cursor==old && !memcmp(before,selected,31));
    selected[3]=2;memcpy(before,selected,31);n=99;
    CHECK(fd2_field_prepare_selection_count(32,selected,&n)==-1 && n==99);
    CHECK(fd2_field_prepare_selection_step(32,selected,&cursor,FD2_FIELD_PREPARE_SELECT_TOGGLE)==-1 && cursor==old && !memcmp(before,selected,31));
    CHECK(fd2_field_prepare_selection_init(NULL)==-1);
    return 0;
}
static int test_reorder_complete_owner_and_front(void) {
    for(size_t capacity=16;capacity<=20;capacity+=4) {
        fd2_save_manual_state s,expected,old;state_init(&s,32);old=s;expected=s;
        uint8_t selected[31]={0};size_t chosen=0;
        /* 含第31同伴；顺序由原raw决定，不由点击先后决定。 */
        selected[30]=1;chosen++;
        for(size_t i=0;i<30 && chosen<capacity-1;i+=2){selected[i]=1;chosen++;}
        for(size_t i=0;i<30 && chosen<capacity-1;i++)if(!selected[i]){selected[i]=1;chosen++;}
        size_t index=1;
        for(int pass=0;pass<2;pass++)for(size_t i=1;i<32;i++)
            if((selected[i-1]!=0)==(pass==0))memcpy(expected.unit_area+80u*index++,old.unit_area+80u*i,80);
        CHECK(index==32 && fd2_field_prepare_roster_reorder(&s,selected,capacity)==0 && !memcmp(&s,&expected,sizeof(s)));
        CHECK(!memcmp(s.unit_area,old.unit_area,80) && s.unit_count==32);
        uint8_t wanted=s.unit_area[(capacity-1u)*80u+8u];
        CHECK(fd2_field_prepare_prefix_has_text(&s,capacity,wanted)==1);
        CHECK(fd2_field_prepare_prefix_has_text(&s,capacity,old.unit_area[8])==0); /* raw0排除。 */
        expected=s;uint8_t moved[80];memcpy(moved,s.unit_area+(capacity-1u)*80u,80);
        memmove(expected.unit_area+160,expected.unit_area+80,(capacity-2u)*80u);memcpy(expected.unit_area+80,moved,80);
        CHECK(fd2_field_prepare_roster_front(&s,wanted)==1 && !memcmp(&s,&expected,sizeof(s)));
        old=s;CHECK(fd2_field_prepare_roster_front(&s,255)==0 && !memcmp(&s,&old,sizeof(s)));
        /* 最终NO没有“撤销”API；保留已提交顺序再保存即可。 */
        selected[0]^=1;old=s;
        CHECK(fd2_field_prepare_roster_reorder(&s,selected,capacity)==-1 && !memcmp(&s,&old,sizeof(s)));
    }
    fd2_save_manual_state s,expected;state_init(&s,17);expected=s;
    uint8_t selected[31]={0};for(size_t i=1;i<16;i++)selected[i]=1;
    size_t out=1;for(size_t i=2;i<17;i++)memcpy(expected.unit_area+80u*out++,s.unit_area+80u*i,80);
    memcpy(expected.unit_area+80u*out,s.unit_area+80u,80);
    CHECK(fd2_field_prepare_roster_reorder(&s,selected,16)==0 && !memcmp(&s,&expected,sizeof(s)));
    CHECK(fd2_field_prepare_roster_reorder(&s,selected,20)==-1);
    CHECK(fd2_field_prepare_prefix_has_text(&s,20,1)==-1);
    state_init(&s,5);s.unit_area[80u+8u]=9;s.unit_area[320u+8u]=9;expected=s;
    uint8_t last[80];memcpy(last,s.unit_area+320u,80);
    memmove(expected.unit_area+160u,expected.unit_area+80u,240u);
    memcpy(expected.unit_area+80u,last,80);
    CHECK(fd2_field_prepare_roster_front(&s,9)==1 && !memcmp(&s,&expected,sizeof(s)));
    return 0;
}
static int test_revive_source_write_points(void) {
    fd2_save_manual_state s,old,expected;state_init(&s,3);
    fd2_field_unit u=get(&s,0);u.flags=0;u.hp=0;put(&s,0,&u);
    u=get(&s,1);u.flags=0x85;u.hp=12;u.movement_profile=9;u.level=8;put(&s,1,&u);
    u=get(&s,2);u.flags=0x81;u.hp=0;u.movement_profile=28;u.level=255;put(&s,2,&u);
    old=s;uint8_t list[32]={0};size_t count=99;
    CHECK(fd2_field_prepare_revive_candidates(&s,list,32,&count)==0 && count==2 && list[0]==1 && list[1]==2 && !memcmp(&s,&old,sizeof(s)));
    count=99;list[0]=77;CHECK(fd2_field_prepare_revive_candidates(&s,list,1,&count)==-1 && count==99 && list[0]==77);
    uint32_t price=0;CHECK(fd2_field_prepare_revive_price(&s,1,&price)==0 && price==9600); /* 原profile9系数1200。 */
    CHECK(fd2_field_prepare_revive_price(&s,2,&price)==0 && price==25500);
    price=9600;s.gold_or_flags=price-1;old=s;
    CHECK(!fd2_field_prepare_money_can_pay(s.gold_or_flags,price) && !memcmp(&s,&old,sizeof(s)));
    s.gold_or_flags=price;expected=s;expected.gold_or_flags=0;expected.unit_area[80+5]=0;
    CHECK(fd2_field_prepare_money_subtract(&s.gold_or_flags,price)==0);
    CHECK(fd2_field_prepare_revive_clear_flags(&s,1)==0 && !memcmp(&s,&expected,sizeof(s)));
    old=s;CHECK(fd2_field_prepare_revive_clear_flags(&s,1)==-1 && !memcmp(&s,&old,sizeof(s)));
    u=get(&expected,1);u.hp=u.hp_max;put(&expected,1,&u);
    CHECK(fd2_field_prepare_revive_restore_hp(&s,1)==0 && !memcmp(&s,&expected,sizeof(s)));
    CHECK(fd2_field_prepare_revive_restore_hp(&s,2)==-1); /* flags尚未清。 */
    for(uint8_t profile=0;profile<30;profile++) {
        if(profile && profile!=7 && profile!=26 && profile!=27 && profile!=29)continue;
        u=get(&s,2);u.movement_profile=profile;put(&s,2,&u);old=s;price=123;
        CHECK(fd2_field_prepare_revive_price(&s,2,&price)==-1 && price==123);
        CHECK(fd2_field_prepare_revive_clear_flags(&s,2)==-1 && !memcmp(&s,&old,sizeof(s)));
    }
    return 0;
}
static int test_promotion_candidates_and_priority(void) {
    fd2_save_manual_state s,old;state_init(&s,4);
    const uint8_t ids[]={0,7,18,9};
    for(size_t i=0;i<4;i++){fd2_field_unit u=get(&s,i);u.unit_id=ids[i];u.level=i ? 20:19;put(&s,i,&u);}
    fd2_field_unit u=get(&s,3);u.hp=0;u.flags=0x81;
    ((uint8_t *)&u)[14]=0;((uint8_t *)&u)[15]=88;
    ((uint8_t *)&u)[16]=0;((uint8_t *)&u)[17]=90;put(&s,3,&u);old=s;
    uint8_t list[32]={0};size_t count=99;
    CHECK(fd2_field_prepare_promotion_candidates(&s,list,32,&count)==0 && count==1 && list[0]==3 && !memcmp(&s,&old,sizeof(s)));
    fd2_field_prepare_promotion plan={99,99,99,99},before=plan;
    CHECK(fd2_field_prepare_promotion_plan(&s,0,&plan)==-1 && !memcmp(&plan,&before,sizeof(plan)));
    CHECK(fd2_field_prepare_promotion_plan(&s,1,&plan)==-1);
    CHECK(fd2_field_prepare_promotion_plan(&s,3,&plan)==0 && plan.actor_index==3 && plan.old_unit_id==9 && plan.target_unit_id==52 && plan.special_slot==3);
    CHECK(!memcmp(&s,&old,sizeof(s)));
    return 0;
}
static int test_promotion_consume_identity_and_raw_prefix(void) {
    fd2_save_manual_state s,old,expected;state_init(&s,1);
    fd2_field_unit u=get(&s,0);((uint8_t *)&u)[14]=0;((uint8_t *)&u)[15]=89;put(&s,0,&u);
    fd2_field_prepare_promotion plan;CHECK(fd2_field_prepare_promotion_plan(&s,0,&plan)==0 && plan.target_unit_id==50 && plan.special_slot==2);
    old=s;expected=s;uint8_t *r=expected.unit_area;memmove(r+14,r+16,10);r[24]=128;
    CHECK(fd2_field_prepare_promotion_consume(&s,&plan)==0 && !memcmp(&s,&expected,sizeof(s)));
    CHECK(fd2_field_prepare_promotion_consume(&s,&plan)==-1 && !memcmp(&s,&expected,sizeof(s)));
    expected.unit_area[7]=50;expected.unit_area[32]=17;
    CHECK(fd2_field_prepare_promotion_identity(&s,&plan)==0 && !memcmp(&s,&expected,sizeof(s)));
    CHECK(s.unit_area[8]==old.unit_area[8] && !memcmp(s.unit_area+26,old.unit_area+26,5));
    CHECK(fd2_field_prepare_promotion_identity(&s,&plan)==-1 && !memcmp(&s,&expected,sizeof(s)));
    /* count2后原raw1隐藏stale89仍被2aedb读；raw3可见89不决定槽号。 */
    state_init(&s,1);u=get(&s,0);for(size_t i=0;i<8;i++)((uint8_t *)&u)[10+2*i]=128;
    ((uint8_t *)&u)[10]=0;((uint8_t *)&u)[11]=0;
    ((uint8_t *)&u)[13]=89;((uint8_t *)&u)[16]=0;((uint8_t *)&u)[17]=89;put(&s,0,&u);
    CHECK(fd2_field_prepare_promotion_plan(&s,0,&plan)==0 && plan.target_unit_id==50 && plan.special_slot==1);
    old=s;expected=s;memmove(expected.unit_area+12,expected.unit_area+14,12);expected.unit_area[24]=128;
    CHECK(fd2_field_prepare_promotion_consume(&s,&plan)==0 && !memcmp(&s,&expected,sizeof(s)));
    /* unit2的映射ff如被stale命中会选52，但52缺90时原remove(-1)未定义。 */
    state_init(&s,1);u=get(&s,0);u.unit_id=2;((uint8_t *)&u)[12]=128;((uint8_t *)&u)[13]=255;
    ((uint8_t *)&u)[16]=0;((uint8_t *)&u)[17]=1;put(&s,0,&u);old=s;memset(&plan,0x5a,sizeof(plan));
    fd2_field_prepare_promotion before=plan;
    CHECK(fd2_field_prepare_promotion_plan(&s,0,&plan)==-1 && !memcmp(&plan,&before,sizeof(plan)) && !memcmp(&s,&old,sizeof(s)));
    return 0;
}
static int test_promotion_ack_deltas_and_finalize(void) {
    fd2_save_manual_state s,old,expected;state_init(&s,1);
    fd2_field_unit u=get(&s,0);u.movement_points=255;
    word(u.base_attack_le,65530);word(u.base_defense_le,65533);word(u.dexterity_le,65534);
    u.hp_max=u.mp_max=65530;put(&s,0,&u);old=s;
    fd2_field_prepare_promotion plan;CHECK(fd2_field_prepare_promotion_plan(&s,0,&plan)==0 && plan.target_unit_id==32 && plan.special_slot==-1);
    static const uint16_t gains[]={12,8,5,13,9,1}; /* 原growth32每对第二byte，promotion32 MV1。 */
    static const uint8_t offsets[]={0x37,0x39,0x3e,0x42,0x46,0x3b};
    for(unsigned kind=0;kind<6;kind++){uint16_t value=999;CHECK(fd2_field_prepare_promotion_gain_value(&plan,(fd2_field_prepare_gain)kind,&value)==0 && value==gains[kind]);}
    CHECK(!memcmp(&s,&old,sizeof(s)));
    CHECK(fd2_field_prepare_promotion_gain_apply(&s,&plan,FD2_FIELD_PREPARE_GAIN_ATTACK)==-1 && !memcmp(&s,&old,sizeof(s)));
    CHECK(fd2_field_prepare_promotion_consume(&s,&plan)==0 && !memcmp(&s,&old,sizeof(s)));
    expected=s;expected.unit_area[7]=32;expected.unit_area[32]=9;
    CHECK(fd2_field_prepare_promotion_identity(&s,&plan)==0 && !memcmp(&s,&expected,sizeof(s)));
    for(unsigned kind=0;kind<6;kind++) {
        old=s;uint16_t value;CHECK(fd2_field_prepare_promotion_gain_value(&plan,(fd2_field_prepare_gain)kind,&value)==0 && !memcmp(&s,&old,sizeof(s)));
        if(kind==5)expected.unit_area[offsets[kind]]=(uint8_t)(expected.unit_area[offsets[kind]]+gains[kind]);
        else {uint8_t *p=expected.unit_area+offsets[kind];word(p,(uint16_t)((uint16_t)(p[0]|(uint16_t)p[1]<<8)+gains[kind]));}
        CHECK(fd2_field_prepare_promotion_gain_apply(&s,&plan,(fd2_field_prepare_gain)kind)==0 && !memcmp(&s,&expected,sizeof(s)));
    }
    CHECK(s.unit_area[33]==20 && s.unit_area[60]==old.unit_area[60]); /* 所有ACK后仍未finalize。 */
    u=get(&expected,0);CHECK(fd2_field_prepare_item_recompute(&u)==0);put(&expected,0,&u);
    u=get(&s,0);CHECK(fd2_field_prepare_item_recompute(&u)==0);put(&s,0,&u);
    u=get(&expected,0);u.level=1;u.experience=0;u.hp=u.hp_max;u.mp=u.mp_max;put(&expected,0,&u);
    CHECK(fd2_field_prepare_promotion_finalize(&s,&plan)==0 && !memcmp(&s,&expected,sizeof(s)));
    old=s;CHECK(fd2_field_prepare_promotion_finalize(&s,&plan)==-1 && !memcmp(&s,&old,sizeof(s)));
    CHECK(fd2_field_prepare_promotion_gain_apply(&s,&plan,FD2_FIELD_PREPARE_GAIN_ATTACK)==-1 && !memcmp(&s,&old,sizeof(s)));
    state_init(&s,1);u=get(&s,0);u.unit_id=16;put(&s,0,&u);
    CHECK(fd2_field_prepare_promotion_plan(&s,0,&plan)==0 && plan.target_unit_id==48);
    uint16_t value=999;CHECK(fd2_field_prepare_promotion_gain_value(&plan,FD2_FIELD_PREPARE_GAIN_MP_MAX,&value)==0 && !value);
    CHECK(fd2_field_prepare_promotion_gain_value(&plan,FD2_FIELD_PREPARE_GAIN_MOVEMENT,&value)==0 && !value);
    return 0;
}
static int test_bad_inputs_preserve_output(void) {
    fd2_save_manual_state s,old;state_init(&s,1);old=s;
    size_t count=55;uint8_t actor=77;
    CHECK(fd2_field_prepare_revive_candidates(NULL,&actor,1,&count)==-1 && count==55 && actor==77);
    CHECK(fd2_field_prepare_promotion_candidates(&s,NULL,0,&count)==-1 && count==55);
    s.unit_count=33;old=s;uint32_t price=7;
    CHECK(fd2_field_prepare_revive_price(&s,0,&price)==-1 && price==7 && !memcmp(&s,&old,sizeof(s)));
    CHECK(fd2_field_prepare_roster_front(&s,1)==-1 && !memcmp(&s,&old,sizeof(s)));
    fd2_field_prepare_promotion bad={0,7,39,-1};uint16_t value=77;
    CHECK(fd2_field_prepare_promotion_gain_value(&bad,FD2_FIELD_PREPARE_GAIN_ATTACK,&value)==-1 && value==77);
    return 0;
}
int main(void) {
    CHECK(test_selection_navigation_and_32nd_bit()==0);
    CHECK(test_reorder_complete_owner_and_front()==0);
    CHECK(test_revive_source_write_points()==0);
    CHECK(test_promotion_candidates_and_priority()==0);
    CHECK(test_promotion_consume_identity_and_raw_prefix()==0);
    CHECK(test_promotion_ack_deltas_and_finalize()==0);
    CHECK(test_bad_inputs_preserve_output()==0);
    puts("field_prepare: 7 source-domain/write-boundary groups PASS (no GUI/RNG)");return 0;
}
