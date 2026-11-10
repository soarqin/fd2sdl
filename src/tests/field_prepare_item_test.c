/* G3准备服务原语：G1 2872b/28cbd/28f65完整来源及1b750已证默认adapter。
 * 只测显式raw写点、alias预检和signed资金；不代表正式UI、等待RNG或原版运行。
 * 密集闭包仅涵盖本文件列出的有限操作和hidden80前提，不声称全部事件闭包。 */
#include <stdio.h>
#include <string.h>

#include "field_item.h"
#include "field_prepare_item.h"
#include "field_unit_stats.h"

#define CHECK(test) do { if (!(test)) { fprintf(stderr,"prepare_item:%d: %s\n",__LINE__,#test); return -1; } } while (0)

static void word(fd2_field_unit *unit,size_t offset,uint16_t value) {
    uint8_t *raw=(uint8_t *)unit;raw[offset]=(uint8_t)value;raw[offset+1u]=(uint8_t)(value>>8);
}

static void init_unit(fd2_field_unit *unit,size_t count) {
    memset(unit,0xa5,sizeof(*unit));uint8_t *raw=(uint8_t *)unit;
    memset(raw+0x22,0,6);raw[0x20]=3;
    word(unit,0x37,10);word(unit,0x39,5);word(unit,0x3e,7);
    for (size_t i=0;i<8;i++) {
        raw[0x0a+2u*i]=i<count ? 0:0x80;
        raw[0x0b+2u*i]=i<count ? (uint8_t)(20u+i):(uint8_t)(0xe0u+i);
    }
}

static int dense(const fd2_field_unit *unit) {
    const uint8_t *raw=(const uint8_t *)unit;size_t count=fd2_field_unit_inventory_count(unit);
    for (size_t i=0;i<8;i++) if (((raw[0x0a+2u*i]&0x80u)==0)!=(i<count)) return 0;
    return 1;
}

static int test_projection_raw_rank_and_errors(void) {
    fd2_field_unit unit;init_unit(&unit,0);uint8_t *raw=(uint8_t *)&unit;
    raw[10]=0;raw[11]=20;raw[12]=0x80;raw[13]=21;raw[14]=0;raw[15]=128;
    fd2_field_unit before=unit;uint8_t ids[8];memset(ids,0xcc,sizeof(ids));size_t count=99;
    CHECK(fd2_field_prepare_item_visible_ids(&unit,ids,8,&count)==0 && count==2);
    CHECK(ids[0]==20 && ids[1]==128 && ids[2]==0xcc);
    uint8_t id=0xdd;
    CHECK(fd2_field_prepare_item_selected_id(&unit,1,&id)==0 && id==21);
    CHECK(!memcmp(&unit,&before,sizeof(unit))); /* 视觉第1行是128，原提交raw1是21。 */
    memset(ids,0xcc,sizeof(ids));count=99;id=0xdd;
    CHECK(fd2_field_prepare_item_visible_ids(&unit,ids,1,&count)==-1 && count==99 && ids[0]==0xcc);
    CHECK(fd2_field_prepare_item_visible_ids(&unit,NULL,8,&count)==-1 && count==99);
    CHECK(fd2_field_prepare_item_visible_ids(NULL,ids,8,&count)==-1);
    CHECK(fd2_field_prepare_item_visible_ids(&unit,ids,8,NULL)==-1);
    CHECK(fd2_field_prepare_item_selected_id(&unit,2,&id)==-1 && id==0xdd);
    CHECK(fd2_field_prepare_item_selected_id(NULL,0,&id)==-1);
    CHECK(fd2_field_prepare_item_selected_id(&unit,0,NULL)==-1);
    CHECK(!memcmp(&unit,&before,sizeof(unit)));
    init_unit(&unit,0);CHECK(fd2_field_prepare_item_visible_ids(&unit,NULL,0,&count)==0 && count==0);
    return 0;
}

static int test_raw_mutation_exact_writes(void) {
    for (size_t removed=0;removed<8;removed++) {
        fd2_field_unit unit;init_unit(&unit,8);uint8_t *raw=(uint8_t *)&unit;
        for (size_t i=0;i<8;i++) {raw[10+2*i]=(uint8_t)(i*17u);raw[11+2*i]=(uint8_t)(0xd0u+i);}
        fd2_field_unit expected=unit;uint8_t *want=(uint8_t *)&expected;
        const uint8_t snapshot[16]={0,0xd0,17,0xd1,34,0xd2,51,0xd3,68,0xd4,85,0xd5,102,0xd6,119,0xd7};
        for (size_t i=removed;i<7;i++) {want[10+2*i]=snapshot[2*(i+1)];want[11+2*i]=snapshot[2*(i+1)+1];}
        want[24]=0x80;
        CHECK(fd2_field_prepare_item_remove(&unit,removed)==0 && !memcmp(&unit,&expected,sizeof(unit)));
        CHECK(raw[25]==0xd7); /* raw7删除亦只改flag，stale ID永不清空。 */
    }
    fd2_field_unit unit;init_unit(&unit,8);fd2_field_unit before=unit;
    CHECK(fd2_field_prepare_item_append(&unit,255)==0 && !memcmp(&unit,&before,sizeof(unit)));
    CHECK(fd2_field_prepare_item_remove(&unit,8)==-1 && !memcmp(&unit,&before,sizeof(unit)));
    ((uint8_t *)&unit)[14]=0xc1;((uint8_t *)&unit)[20]=0x80;
    before=unit;fd2_field_unit expected=unit;((uint8_t *)&expected)[14]=0;((uint8_t *)&expected)[15]=255;
    CHECK(fd2_field_prepare_item_append(&unit,255)==1 && !memcmp(&unit,&expected,sizeof(unit)));
    CHECK(((uint8_t *)&unit)[20]==0x80); /* 最早hidden，整flag清0；不清其它洞。 */
    CHECK(fd2_field_prepare_item_remove(NULL,0)==-1 && fd2_field_prepare_item_append(NULL,0)==-1);
    return 0;
}

static int test_equip_groups_and_buy_last_count(void) {
    fd2_field_unit unit;init_unit(&unit,4);uint8_t *raw=(uint8_t *)&unit;
    raw[10]=0x43;raw[11]=20;raw[12]=0xc1;raw[13]=21;
    raw[14]=0x42;raw[15]=128;raw[16]=0;raw[17]=22;
    fd2_field_unit expected=unit;uint8_t *want=(uint8_t *)&expected;
    want[10]=want[12]=0;want[16]=0x40;
    CHECK(fd2_field_prepare_item_equip(&unit,3)==0 && !memcmp(&unit,&expected,sizeof(unit)));
    CHECK(fd2_field_prepare_item_equip(&unit,3)==0 && !memcmp(&unit,&expected,sizeof(unit)));
    expected=unit;want=(uint8_t *)&expected;want[14]=0x40;
    CHECK(fd2_field_prepare_item_equip(&unit,2)==0 && !memcmp(&unit,&expected,sizeof(unit)));
    CHECK(fd2_field_prepare_item_equip(&unit,8)==-1 && !memcmp(&unit,&expected,sizeof(unit)));
    CHECK(fd2_field_prepare_item_equip(NULL,0)==-1);
    /* 原Buy先填首洞raw1，随后按count-1装备raw2；不修正为刚买入的raw1。 */
    init_unit(&unit,3);raw=(uint8_t *)&unit;raw[12]=0x80;raw[13]=77;
    CHECK(fd2_field_prepare_item_append(&unit,23)==1 && raw[13]==23);
    CHECK(fd2_field_prepare_item_equip(&unit,fd2_field_unit_inventory_count(&unit)-1u)==0);
    CHECK(raw[12]==0 && raw[14]==0x40 && raw[15]==22);
    return 0;
}

static int test_default_stats_and_failure(void) {
    fd2_field_unit unit;init_unit(&unit,1);uint8_t *raw=(uint8_t *)&unit;
    raw[10]=0x40;raw[11]=20;raw[0x22]=raw[0x23]=raw[0x24]=2;
    fd2_field_unit expected=unit;word(&expected,0x48,34);word(&expected,0x4a,5);
    word(&expected,0x4c,112);word(&expected,0x4e,22);
    CHECK(fd2_field_prepare_item_recompute(&unit)==0 && !memcmp(&unit,&expected,sizeof(unit)));
    init_unit(&unit,0);raw=(uint8_t *)&unit;raw[0x22]=1;word(&unit,0x37,(uint16_t)-100);
    expected=unit;word(&expected,0x48,(uint16_t)-114);word(&expected,0x4a,5);
    word(&expected,0x4c,7);word(&expected,0x4e,7);
    CHECK(fd2_field_prepare_item_recompute(&unit)==0 && !memcmp(&unit,&expected,sizeof(unit)));
    raw[10]=0xc0;raw[11]=255;expected=unit;
    CHECK(fd2_field_prepare_item_recompute(&unit)==-1 && !memcmp(&unit,&expected,sizeof(unit)));
    CHECK(fd2_field_prepare_item_recompute(NULL)==-1);
    fd2_field_item_stat_effect effect={11,22,33,44},saved=effect;int32_t scaled=123;
    CHECK(fd2_field_unit_original_item_stats(NULL,255,&effect)==-1 && !memcmp(&effect,&saved,sizeof(effect)));
    CHECK(fd2_field_unit_original_item_stats(NULL,20,NULL)==-1);
    CHECK(fd2_field_unit_original_status_scale(NULL,FD2_FIELD_UNIT_STAT_ATTACK,INT32_MAX,&scaled)==-1 && scaled==123);
    CHECK(fd2_field_unit_original_status_scale(NULL,FD2_FIELD_UNIT_STAT_ATTACK,1,NULL)==-1);
    return 0;
}

static int test_give_alias_and_recipient_unchanged_stats(void) {
    for (size_t n=1;n<8;n++) for (size_t selected=0;selected<n;selected++) {
        fd2_field_unit unit;init_unit(&unit,n);uint8_t *raw=(uint8_t *)&unit;
        raw[10+2*selected]=0x43;
        fd2_field_unit before=unit;uint8_t id=0xdd;
        CHECK(fd2_field_prepare_item_give_preflight(&unit,selected,&unit,&id)==1 && id==20u+selected);
        CHECK(!memcmp(&unit,&before,sizeof(unit)));
        CHECK(fd2_field_prepare_item_remove(&unit,selected)==0);
        CHECK(fd2_field_prepare_item_append(&unit,id)==1 && fd2_field_prepare_item_recompute(&unit)==0);
        CHECK(dense(&unit) && fd2_field_unit_inventory_count(&unit)==n);
        for (size_t i=0;i<n;i++) {
            uint8_t expected=(uint8_t)(i==n-1u ? 20u+selected:20u+i+(i>=selected));
            CHECK(raw[11+2*i]==expected && raw[10+2*i]==0);
        }
        CHECK(raw[25]==((const uint8_t *)&before)[25]);
        fd2_field_unit restored=unit;
        memcpy((uint8_t *)&restored+10,(const uint8_t *)&before+10,16);
        memcpy((uint8_t *)&restored+0x48,(const uint8_t *)&before+0x48,8);
        CHECK(!memcmp(&restored,&before,sizeof(before))); /* HP/MP/EX/acted等不动。 */
    }
    fd2_field_unit sender,recipient;init_unit(&sender,2);init_unit(&recipient,0);
    ((uint8_t *)&sender)[10]=0x40;
    fd2_field_unit old_sender=sender,old_recipient=recipient;uint8_t id=0xdd;
    CHECK(fd2_field_prepare_item_give_preflight(&sender,0,&recipient,&id)==1 && id==20);
    CHECK(!memcmp(&sender,&old_sender,sizeof(sender)) && !memcmp(&recipient,&old_recipient,sizeof(recipient)));
    CHECK(fd2_field_prepare_item_remove(&sender,0)==0 && fd2_field_prepare_item_append(&recipient,id)==1);
    CHECK(fd2_field_prepare_item_recompute(&sender)==0);
    ((uint8_t *)&old_recipient)[10]=0;((uint8_t *)&old_recipient)[11]=20;
    CHECK(!memcmp(&recipient,&old_recipient,sizeof(recipient))); /* 收方派生不重算。 */
    init_unit(&sender,8);old_sender=sender;id=0xdd;
    CHECK(fd2_field_prepare_item_give_preflight(&sender,0,&sender,&id)==0 && id==0xdd);
    CHECK(!memcmp(&sender,&old_sender,sizeof(sender)));
    init_unit(&sender,2);init_unit(&recipient,0);((uint8_t *)&sender)[12]=0x40;((uint8_t *)&sender)[13]=255;
    old_sender=sender;old_recipient=recipient;
    CHECK(fd2_field_prepare_item_give_preflight(&sender,0,&recipient,&id)==-1 && id==0xdd);
    CHECK(!memcmp(&sender,&old_sender,sizeof(sender)) && !memcmp(&recipient,&old_recipient,sizeof(recipient)));
    CHECK(fd2_field_prepare_item_give_preflight(NULL,0,&recipient,&id)==-1);
    CHECK(fd2_field_prepare_item_give_preflight(&sender,8,&recipient,&id)==-1);
    CHECK(fd2_field_prepare_item_give_preflight(&sender,0,NULL,&id)==-1);
    CHECK(fd2_field_prepare_item_give_preflight(&sender,0,&recipient,NULL)==-1);
    return 0;
}

static int test_dense_operation_closure_with_declared_source_flags(void) {
    for (size_t n=0;n<=8;n++) {
        fd2_field_unit base;init_unit(&base,n);CHECK(dense(&base));
        fd2_field_unit unit=base;
        CHECK(fd2_field_prepare_item_append(&unit,128)==(n==8 ? 0:1) && dense(&unit));
        for (size_t selected=0;selected<n;selected++) {
            unit=base;CHECK(fd2_field_prepare_item_remove(&unit,selected)==0 && dense(&unit));
            CHECK(fd2_field_unit_inventory_count(&unit)==n-1u);
            CHECK(fd2_field_prepare_item_append(&unit,128)==1 && dense(&unit));
            CHECK(fd2_field_unit_inventory_count(&unit)==n);
            unit=base;CHECK(fd2_field_prepare_item_equip(&unit,selected)==0 && dense(&unit));
            unit=base;uint8_t id=0;
            CHECK(fd2_field_prepare_item_give_preflight(&unit,selected,&unit,&id)==(n==8 ? 0:1));
            CHECK(!memcmp(&unit,&base,sizeof(unit)));
        }
    }
    /* 明确反例：hidden40不是上述来源前提，原整flag清0可以产生洞。
     * 不能以有条件闭包宣称任意SAV／所有事件写入永远密集。 */
    fd2_field_unit unit;init_unit(&unit,1);uint8_t *raw=(uint8_t *)&unit;
    raw[14]=0xc0;raw[15]=21;
    CHECK(fd2_field_prepare_item_equip(&unit,0)==0 && raw[14]==0 && !dense(&unit));
    return 0;
}

static int test_money_signed_wrap_and_price(void) {
    const struct { uint32_t money,amount;int enough; } cases[]={
        {0,0,1},{0,1,0},{99,100,0},{100,100,1},{0x7fffffff,65535,1},
        {0x80000000,0,0},{0x80000000,1,0},{0xffffffff,0,0},
        {0xffffffff,0xffffffff,1},{0x80000000,0xffffffff,0},{0,0xffffffff,1}
    };
    for (size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++)
        CHECK(fd2_field_prepare_money_can_pay(cases[i].money,cases[i].amount)==cases[i].enough);
    struct { uint32_t before,money,after; } state={0xa5a5a5a5,0xffffffff,0x5a5a5a5a};
    CHECK(fd2_field_prepare_money_add(&state.money,1)==0 && state.money==0);
    CHECK(fd2_field_prepare_money_subtract(&state.money,1)==0 && state.money==0xffffffff);
    state.money=0x7fffffff;
    CHECK(fd2_field_prepare_money_add(&state.money,1)==0 && state.money==0x80000000);
    CHECK(fd2_field_prepare_money_subtract(&state.money,1)==0 && state.money==0x7fffffff);
    CHECK(state.before==0xa5a5a5a5 && state.after==0x5a5a5a5a);
    CHECK(fd2_field_prepare_money_add(NULL,1)==-1 && fd2_field_prepare_money_subtract(NULL,1)==-1);
    for (unsigned id=0;id<FD2_FIELD_ITEM_COUNT;id++) {
        const uint8_t *item=fd2_field_item_record_get((uint8_t)id);uint32_t price=0xffffffff;
        uint32_t purchase=(uint32_t)item[19]+256u*item[20];
        CHECK(fd2_field_prepare_item_sell_price((uint8_t)id,&price)==0 && price==purchase-purchase/4u-(purchase%4u!=0));
    }
    uint32_t price=0xdeadbeef;
    CHECK(fd2_field_prepare_item_sell_price(255,&price)==-1 && price==0xdeadbeef);
    CHECK(fd2_field_prepare_item_sell_price(20,NULL)==-1);
    return 0;
}

int main(void) {
    CHECK(test_projection_raw_rank_and_errors()==0);
    CHECK(test_raw_mutation_exact_writes()==0);
    CHECK(test_equip_groups_and_buy_last_count()==0);
    CHECK(test_default_stats_and_failure()==0);
    CHECK(test_give_alias_and_recipient_unchanged_stats()==0);
    CHECK(test_dense_operation_closure_with_declared_source_flags()==0);
    CHECK(test_money_signed_wrap_and_price()==0);
    puts("field_prepare_item: 7 pure groups PASS; no UI/window/RNG/original-runtime claim");return 0;
}
