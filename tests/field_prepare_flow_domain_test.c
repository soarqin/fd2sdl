/* G3准备冷图的领域叶：使用真实80B/物品/转职/稳定重排原语，UI仅作有界
 * 无副作用替身，检验首写前拒绝与晚错误保留。不开窗口，不代替原版验收。 */
#include <stdio.h>
#include <string.h>
#include "field_prepare_play.h"
#include "field_prepare.h"
#include "field_prepare_item.h"
#include "field_unit_base.h"
#include "field_item.h"
#include "static_flow.h"
#include "static_data/preparation.inc"

typedef struct {
    int selector,selected_command,product_index,product_first;
    uint8_t stock[12],candidates[FD2_SAVE_MAX_UNITS],visible_items[8],selection[FD2_SAVE_MAX_UNITS-1u];
    size_t stock_count,candidate_count,item_count,first;
    int member,receiver,raw_slot,cursor;
    uint8_t product_id,captured_item,required_text;
    uint32_t price,previous_money;
    size_t quota;
    fd2_field_prepare_promotion promotion;
    unsigned growth_index,growth_line,sprite_phase,highlight_phase;
    uint16_t growth_delta;
    int notown_mode,done,result,host_quit,irreversible;
} prepare_service;
typedef struct { fd2_field_prepare_assets *assets;fd2_field_play_context *context;prepare_service service; } prepare_ui;

static int fail_text=-1,fail_notice,feedback_result,promotion_result,selection_result;
static int bind_count,promotion_count,sound_count,movement_preflight_y;
static size_t sound_samples[4];static int sound_secondary[4];
static int prepare_service_notice_ready(prepare_ui *ui,unsigned id) {(void)ui;(void)id;return fail_notice ? -1:0;}
static int prepare_service_text_ready(prepare_ui *ui,size_t fragment,const fd2_text_parameters *p,int x,int y) {
    (void)ui;(void)p;(void)x;if(fragment==596u)movement_preflight_y=y;
    return (int)fragment==fail_text ? -1:0;
}
static int prepare_service_promotion_prepare(prepare_ui *ui) {(void)ui;promotion_count++;return promotion_result;}
static int prepare_service_selection_resources(prepare_ui *ui) {(void)ui;return selection_result;}
static int prepare_service_money_animate(prepare_ui *ui,int subtract) {(void)ui;(void)subtract;return feedback_result;}
static int prepare_service_selection_sound(prepare_ui *ui,size_t sample,int secondary) {
    (void)ui;if(sound_count<4){sound_samples[sound_count]=sample;sound_secondary[sound_count]=secondary;}sound_count++;return 0;
}
static int domain_bind_roster(fd2_field_prepare_assets *assets,const fd2_save_manual_state *state) {
    (void)assets;(void)state;bind_count++;return 0;
}
#define fd2_field_prepare_assets_bind_roster domain_bind_roster
#include "../src/field_prepare_service_flow.h"
#undef fd2_field_prepare_assets_bind_roster

#define CHECK(e) do {if(!(e)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#e);return 1;}} while(0)
static void put(fd2_save_manual_state *s,size_t i,const fd2_field_unit *u) {memcpy(s->unit_area+i*80u,u,80);}
static fd2_field_unit get(const fd2_save_manual_state *s,size_t i) {fd2_field_unit u;memcpy(&u,s->unit_area+i*80u,80);return u;}
static int invoke(prepare_ui *ui,const char *op,int argument,unsigned *port) {
    fd2_static_flow_node node={0};node.op=op;node.args[0]=argument;return prepare_service_domain(ui,&node,port);
}
static int init(prepare_ui *ui,fd2_field_play_context *context,fd2_field_prepare_assets *assets,size_t count) {
    memset(ui,0,sizeof(*ui));memset(context,0,sizeof(*context));memset(assets,0,sizeof(*assets));
    fail_text=-1;fail_notice=feedback_result=promotion_result=selection_result=0;bind_count=promotion_count=sound_count=0;movement_preflight_y=-1;
    ui->context=context;ui->assets=assets;context->has_preparation=1;context->preparation.unit_count=(uint8_t)count;
    context->preparation.stage_id=1;context->preparation.gold_or_flags=10000;context->rng.state=0x7a18;
    assets->ready=assets->service.ready=1;assets->sprites.decoded_frames=(uint8_t *)assets;assets->sprites.frame_count=136u*12u;
    for(size_t i=0;i<count;i++) {
        fd2_field_unit unit;memset(&unit,0xa6,sizeof(unit));
        CHECK(fd2_field_unit_persistent_create(&unit,(uint8_t)(i%32u))==0);
        unit.flags=0;unit.text_id=(uint8_t)i;unit.attack_status=unit.defense_status=unit.hit_evasion_status=0;
        put(&context->preparation,i,&unit);
    }
    return 0;
}
static int test_give_alias_and_invalid_preflight(void) {
    prepare_ui ui;fd2_field_play_context context;fd2_field_prepare_assets assets;CHECK(init(&ui,&context,&assets,2)==0);
    fd2_save_manual_state *s=&context.preparation;fd2_field_unit unit=get(s,0);
    for(size_t i=0;i<8;i++){((uint8_t *)&unit)[10+2*i]=128;((uint8_t *)&unit)[11+2*i]=(uint8_t)(200+i);}
    CHECK(fd2_field_prepare_item_append(&unit,0)==1 && fd2_field_prepare_item_append(&unit,128)==1 && fd2_field_prepare_item_append(&unit,10)==1);
    CHECK(fd2_field_prepare_item_equip(&unit,0)==0 && fd2_field_prepare_item_equip(&unit,1)==0);
    put(s,0,&unit);ui.service.member=ui.service.receiver=0;ui.service.raw_slot=0;
    fd2_field_play_context before=context;unsigned port=99;
    CHECK(invoke(&ui,"preflight",2,&port)==1 && port==FD2_FLOW_NEXT && !memcmp(&context,&before,sizeof(context)));
    fd2_field_unit expected=unit;CHECK(fd2_field_prepare_item_remove(&expected,0)==0 && fd2_field_prepare_item_append(&expected,0)==1 && fd2_field_prepare_item_recompute(&expected)==0);
    CHECK(invoke(&ui,"item_capture",0,&port)==1 && ui.service.captured_item==0);
    CHECK(invoke(&ui,"item_remove",0,&port)==1 && invoke(&ui,"item_append",1,&port)==1 && invoke(&ui,"recompute",0,&port)==1);
    CHECK(!memcmp(s->unit_area,&expected,80) && !memcmp(s->unit_area+80,before.preparation.unit_area+80,sizeof(s->unit_area)-80));
    CHECK(s->gold_or_flags==before.preparation.gold_or_flags && context.rng.state==before.rng.state);
    unit=get(s,0);((uint8_t *)&unit)[10]=64;((uint8_t *)&unit)[11]=255;put(s,0,&unit);before=context;
    CHECK(invoke(&ui,"preflight",1,&port)==-1 && port==FD2_FLOW_ERROR && !memcmp(&context,&before,sizeof(context)));
    return 0;
}
static int test_money_commit_precedes_host_or_error(void) {
    prepare_ui ui;fd2_field_play_context context;fd2_field_prepare_assets assets;CHECK(init(&ui,&context,&assets,1)==0);
    unsigned port;ui.service.price=91;fd2_field_play_context before=context;feedback_result=1;
    CHECK(invoke(&ui,"money_sub",0,&port)==-1 && port==FD2_FLOW_ERROR && ui.service.host_quit && ui.service.irreversible);
    CHECK(ui.service.previous_money==10000 && context.preparation.gold_or_flags==9909 && !memcmp(context.preparation.unit_area,before.preparation.unit_area,sizeof(before.preparation.unit_area)));
    feedback_result=-1;ui.service.host_quit=0;context.preparation.gold_or_flags=UINT32_MAX;ui.service.price=3;
    CHECK(invoke(&ui,"money_add",0,&port)==-1 && !ui.service.host_quit && context.preparation.gold_or_flags==2 && ui.service.previous_money==UINT32_MAX);
    return 0;
}
static int test_buy_and_manual_equipment_preflight(void) {
    prepare_ui ui;fd2_field_play_context context;fd2_field_prepare_assets assets;CHECK(init(&ui,&context,&assets,1)==0);
    fd2_save_manual_state *s=&context.preparation;unsigned port;ui.service.selector=1;ui.service.product_id=0;
    CHECK(invoke(&ui,"stock_prepare",0,&port)==1 && ui.service.stock_count<=12u);
    CHECK(invoke(&ui,"price_prepare",0,&port)==1 && invoke(&ui,"message_name",0,&port)==1 && ui.context->message_new_fragment==181);
    fd2_field_unit unit=get(s,0);for(size_t i=0;i<8;i++)((uint8_t *)&unit)[10+2*i]=128;
    CHECK(fd2_field_prepare_item_append(&unit,128)==1);put(s,0,&unit);
    fd2_field_play_context before=context;fail_text=k_prepare_buy_messages[3][1];
    CHECK(invoke(&ui,"preflight",0,&port)==-1 && !memcmp(&context,&before,sizeof(context)));
    fail_text=-1;CHECK(invoke(&ui,"preflight",0,&port)==1 && !memcmp(&context,&before,sizeof(context)));
    CHECK(invoke(&ui,"item_append",0,&port)==1 && s->gold_or_flags==before.preparation.gold_or_flags);
    fd2_field_unit expected=unit;CHECK(fd2_field_prepare_item_append(&expected,0)==1 && fd2_field_prepare_item_equip(&expected,1)==0);
    CHECK(invoke(&ui,"equipment_set",0,&port)==1 && !memcmp(&expected,s->unit_area,80));
    CHECK(invoke(&ui,"recompute",0,&port)==1);
    ui.service.raw_slot=1;CHECK(invoke(&ui,"equipment_compatible",0,&port)==1 && port==FD2_FLOW_YES);
    before=context;CHECK(invoke(&ui,"preflight",6,&port)==1 && !memcmp(&context,&before,sizeof(context)));
    CHECK(invoke(&ui,"equipment_set",1,&port)==1);
    before=context;ui.service.raw_slot=8;CHECK(invoke(&ui,"preflight",6,&port)==-1 && !memcmp(&context,&before,sizeof(context)));
    return 0;
}
static int test_revive_and_promotion_split_writes(void) {
    prepare_ui ui;fd2_field_play_context context;fd2_field_prepare_assets assets;CHECK(init(&ui,&context,&assets,2)==0);
    fd2_save_manual_state *s=&context.preparation;fd2_field_unit unit=get(s,1);unit.flags=0x85;unit.hp=3;unit.hp_max=77;unit.mp=2;unit.mp_max=40;put(s,1,&unit);
    ui.service.member=1;unsigned port;CHECK(invoke(&ui,"price_prepare",2,&port)==1);
    fd2_field_play_context before=context;CHECK(invoke(&ui,"preflight",3,&port)==1 && !memcmp(&context,&before,sizeof(context)));
    CHECK(invoke(&ui,"revive_flags_clear",0,&port)==1);unit=get(s,1);CHECK(!unit.flags && unit.hp==3 && unit.mp==2);
    CHECK(invoke(&ui,"revive_hp",0,&port)==1);unit=get(s,1);CHECK(unit.hp==77 && unit.mp==2 && unit.level==before.preparation.unit_area[80+0x21]);
    ui.service.member=0;unit=get(s,0);unit.level=20;put(s,0,&unit);before=context;
    fail_text=490;CHECK(invoke(&ui,"preflight",4,&port)==-1 && !memcmp(&context,&before,sizeof(context)) && promotion_count==0);
    fail_text=-1;CHECK(invoke(&ui,"preflight",4,&port)==1 && promotion_count==1 && !memcmp(&context,&before,sizeof(context)) && bind_count==0);
    CHECK(movement_preflight_y==176); /* 2ac7d沿最后line3，不再次走1e529的scroll。 */
    CHECK(invoke(&ui,"promotion_item_consume",0,&port)==1 && invoke(&ui,"promotion_identity",0,&port)==1 && ui.service.growth_line==1);
    unit=get(s,0);CHECK(unit.unit_id==32 && unit.level==20 && unit.text_id==before.preparation.unit_area[8]);
    CHECK(invoke(&ui,"cache_rebuild",0,&port)==1 && bind_count==1);
    fd2_field_play_context after_identity=context;
    uint16_t expected_gain=(uint16_t)(8u+fd2_field_rng_next(&after_identity.rng)%4u);
    CHECK(invoke(&ui,"growth_select",0,&port)==1 && ui.service.growth_delta==expected_gain &&
        context.rng.state==after_identity.rng.state && !memcmp(s,&after_identity.preparation,sizeof(*s)));
    uint16_t old=(uint16_t)(unit.base_attack_le[0]|unit.base_attack_le[1]<<8);
    CHECK(invoke(&ui,"growth_commit",0,&port)==1 && ui.service.growth_line==2);
    unit=get(s,0);CHECK((uint16_t)(unit.base_attack_le[0]|unit.base_attack_le[1]<<8)==(uint16_t)(old+expected_gain) &&
        context.rng.state==after_identity.rng.state);
    for(int i=1;i<5;i++){CHECK(invoke(&ui,"growth_select",i,&port)==1);CHECK(invoke(&ui,"growth_commit",0,&port)==1);}
    unsigned line=ui.service.growth_line;CHECK(invoke(&ui,"movement_prepare",0,&port)==1 && invoke(&ui,"movement_commit",0,&port)==1 && ui.service.growth_line==line);
    CHECK(invoke(&ui,"recompute",0,&port)==1 && invoke(&ui,"promotion_finalize",0,&port)==1);
    unit=get(s,0);CHECK(unit.level==1 && unit.experience==0 && unit.hp==unit.hp_max && unit.mp==unit.mp_max);
    CHECK(invoke(&ui,"promotion_finalize",0,&port)==-1);
    return 0;
}
static int test_selection_final_cancel_and_ui_leaf(void) {
    prepare_ui ui;fd2_field_play_context context;fd2_field_prepare_assets assets;CHECK(init(&ui,&context,&assets,32)==0);
    unsigned port=99;CHECK(invoke(&ui,"quota_set",16,&port)==1 && invoke(&ui,"preflight",5,&port)==1);
    fd2_field_play_context rejected=context;fail_text=657;
    CHECK(invoke(&ui,"preflight",5,&port)==-1 && !memcmp(&rejected,&context,sizeof(context)));
    fail_text=658;CHECK(invoke(&ui,"preflight",5,&port)==-1 && !memcmp(&rejected,&context,sizeof(context)));
    fail_text=-1;fail_notice=1;CHECK(invoke(&ui,"preflight",5,&port)==-1 && !memcmp(&rejected,&context,sizeof(context)));
    fail_notice=0;
    CHECK(invoke(&ui,"selection_reset",0,&port)==1);ui.service.cursor=3;
    CHECK(invoke(&ui,"selection_toggle",0,&port)==1 && ui.service.cursor==3 && ui.service.selection[3]==1);
    CHECK(invoke(&ui,"selection_cursor_next",0,&port)==1 && ui.service.cursor==4 && sound_count==2 && sound_samples[0]==7 && sound_secondary[0]==1 && sound_samples[1]==0 && !sound_secondary[1]);
    memset(ui.service.selection,0,sizeof(ui.service.selection));for(size_t i=1;i<16;i++)ui.service.selection[i]=1;
    fd2_save_manual_state before=context.preparation;
    CHECK(invoke(&ui,"selection_full",0,&port)==1 && port==FD2_FLOW_YES);
    CHECK(invoke(&ui,"selection_reorder",0,&port)==1 && !memcmp(context.preparation.unit_area,before.unit_area,80));
    CHECK(context.preparation.unit_area[80+8]==2 && context.preparation.unit_area[16*80+8]==1);
    fd2_save_manual_state sorted=context.preparation;CHECK(invoke(&ui,"finish",0,&port)==1 && ui.service.done && ui.service.result==0 && !memcmp(&sorted,&context.preparation,sizeof(sorted)));
    port=77;CHECK(invoke(&ui,"pick_member",0,&port)==0 && port==77);
    CHECK(invoke(&ui,"quota_set",17,&port)==-1 && port==FD2_FLOW_ERROR && ui.service.quota==16);
    context.preparation.unit_count=15;before=context.preparation;CHECK(invoke(&ui,"preflight",5,&port)==-1 && !memcmp(&before,&context.preparation,sizeof(before)));
    return 0;
}
int main(void) {
    if(test_give_alias_and_invalid_preflight() || test_money_commit_precedes_host_or_error() || test_buy_and_manual_equipment_preflight() || test_revive_and_promotion_split_writes() || test_selection_final_cancel_and_ui_leaf())return 1;
    puts("prepare flow domain: alias/preflight/split commit/money/selection passed");return 0;
}
