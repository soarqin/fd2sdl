/* G3真实C有限领域输出给原码差分工具；不包含prepare UI，不启动SDL/窗口。
 * 夹具只在内存构造，完整a00和未知80B均输出；不是自然游玩存档来源。
 * 原码执行方只消费这里的输入，不导入SDL冷表/期望算法。 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "field_prepare.h"
#include "field_prepare_item.h"
#include "field_item.h"

typedef struct {const char *label;uint32_t money;fd2_field_unit unit;} trace_entry;
static void hex(const uint8_t *data,size_t size){for(size_t i=0;i<size;i++)printf("%02x",data[i]);}
static void put(fd2_save_manual_state *s,size_t i,const fd2_field_unit *u){memcpy(s->unit_area+i*80u,u,80);}
static fd2_field_unit get(const fd2_save_manual_state *s,size_t i){fd2_field_unit u;memcpy(&u,s->unit_area+i*80u,80);return u;}
static void init(fd2_save_manual_state *s,size_t count){
    memset(s,0,sizeof(*s));s->unit_count=(uint8_t)count;s->stage_id=1;s->gold_or_flags=10000;
    for(size_t i=0;i<32;i++){
        fd2_field_unit u;for(size_t b=0;b<80;b++)((uint8_t *)&u)[b]=(uint8_t)(i*37u+b*11u+7u);
        u.flags=0;u.unit_id=(uint8_t)(i%18u);u.text_id=(uint8_t)i;u.movement_profile=1;u.level=20;
        u.attack_status=u.defense_status=u.hit_evasion_status=0;memset(u.detail_status,0,3);
        u.hp=7;u.hp_max=(uint16_t)(100u+i);u.mp=11;u.mp_max=33;
        for(size_t k=0;k<8;k++){((uint8_t *)&u)[10+2*k]=128;((uint8_t *)&u)[11+2*k]=(uint8_t)(200+k);}
        ((uint8_t *)&u)[10]=64;((uint8_t *)&u)[11]=0;((uint8_t *)&u)[12]=64;((uint8_t *)&u)[13]=128;
        put(s,i,&u);
    }
}
static void emit(const char *id,const char *kind,const fd2_save_manual_state *in,const fd2_save_manual_state *out,
        int arg0,int arg1,int result,const uint8_t *selection,const uint8_t *actors,const uint8_t *targets,size_t n,
        const trace_entry *trace,size_t trace_count){
    printf("{\"case_id\":\"%s\",\"kind\":\"%s\",\"count\":%u,\"money\":%u,\"args\":[%d,%d],\"selection_hex\":\"",id,kind,in->unit_count,in->gold_or_flags,arg0,arg1);
    if(selection)hex(selection,31);
    printf("\",\"input_roster_hex\":\"");hex(in->unit_area,sizeof(in->unit_area));
    printf("\",\"finish_roster_hex\":\"");hex(out->unit_area,sizeof(out->unit_area));
    printf("\",\"finish_money\":%u,\"result\":%d,\"candidates_hex\":\"",out->gold_or_flags,result);if(actors)hex(actors,n);
    printf("\",\"promotion_targets_hex\":\"");if(targets)hex(targets,n);
    printf("\",\"commit_trace\":[");
    for(size_t i=0;i<trace_count;i++){
        if(i)printf(",");printf("{\"label\":\"%s\",\"money\":%u,\"actor_hex\":\"",trace[i].label,trace[i].money);
        hex((const uint8_t *)&trace[i].unit,80);printf("\"}");
    }
    puts("]}");
}
int main(void){
    fd2_save_manual_state in,out;char id[80];uint8_t selected[31],actors[32],targets[32];size_t n;int result;
    for(size_t capacity=16;capacity<=20;capacity+=4)for(size_t count=capacity;count<=32;count+=count==capacity ? 1u:32u-count){
        init(&in,count);out=in;memset(selected,0,31);size_t used=0;selected[count-2u]=1;used++;
        for(size_t i=0;i+1u<count && used<capacity-1u;i+=2)if(!selected[i]){selected[i]=1;used++;}
        for(size_t i=0;i+1u<count && used<capacity-1u;i++)if(!selected[i]){selected[i]=1;used++;}
        result=fd2_field_prepare_roster_reorder(&out,selected,capacity);
        snprintf(id,sizeof(id),"reorder-%u-%u",(unsigned)capacity,(unsigned)count);
        emit(id,"reorder",&in,&out,(int)capacity,0,result,selected,NULL,NULL,0,NULL,0);
        if(count==32)break;
    }
    for(int duplicate=0;duplicate<2;duplicate++){
        init(&in,32);if(duplicate)in.unit_area[4u*80u+8u]=30;out=in;
        result=fd2_field_prepare_roster_front(&out,30);
        emit(duplicate ? "front-last-match":"front-unique","front",&in,&out,30,0,result,NULL,NULL,NULL,0,NULL,0);
    }
    const size_t counts[]={0,1,7,16,32};
    const uint8_t flags[]={0,1,4,128,133,161};
    for(size_t c=0;c<sizeof(counts)/sizeof(counts[0]);c++){
        init(&in,counts[c]);for(size_t i=0;i<counts[c];i++){fd2_field_unit u=get(&in,i);u.flags=flags[i%6u];u.hp=(uint16_t)(i&1u ? 0:17);put(&in,i,&u);}out=in;
        result=fd2_field_prepare_revive_candidates(&out,actors,32,&n);snprintf(id,sizeof(id),"hidden-%u",(unsigned)counts[c]);
        emit(id,"hidden",&in,&out,0,0,result,NULL,actors,NULL,n,NULL,0);
    }
    for(int special=0;special<2;special++){
        init(&in,32);
        for(size_t i=0;i<32;i++){fd2_field_unit u=get(&in,i);u.unit_id=(uint8_t)i;u.level=(uint8_t)(i%3u==0 ? 19:i%3u==1 ? 20:255);u.flags=flags[i%6u];put(&in,i,&u);}
        if(special){fd2_field_unit u=get(&in,0);u.level=20;fd2_field_prepare_item_append(&u,89);put(&in,0,&u);u=get(&in,9);u.level=20;fd2_field_prepare_item_append(&u,90);put(&in,9,&u);}
        out=in;result=fd2_field_prepare_promotion_candidates(&out,actors,32,&n);
        for(size_t i=0;i<n;i++){fd2_field_prepare_promotion plan;if(fd2_field_prepare_promotion_plan(&out,actors[i],&plan)!=0)return 2;targets[i]=plan.target_unit_id;}
        emit(special ? "promotion-special":"promotion-ordinary","promotion",&in,&out,0,0,result,NULL,actors,targets,n,NULL,0);
    }
    for(int mode=0;mode<6;mode++){
        init(&in,2);fd2_field_unit u=get(&in,1);u.flags=0x85;u.hp=15;u.hp_max=77;u.mp=2;u.mp_max=40;u.movement_profile=9;u.level=8;put(&in,1,&u);
        in.gold_or_flags=mode==0 ? 10000:mode==1 ? 9600:mode==2 ? 9599:mode==3 ? 0x80000000u:10000;
        out=in;uint32_t price;trace_entry trace[3];size_t written=0;result=0;
        if(fd2_field_prepare_revive_price(&out,1,&price)!=0)return 2;
        if(mode<4 && fd2_field_prepare_money_can_pay(out.gold_or_flags,price)){
            if(fd2_field_prepare_money_subtract(&out.gold_or_flags,price)!=0)return 2;
            trace[written++]=(trace_entry){"money",out.gold_or_flags,get(&out,1)};
            if(fd2_field_prepare_revive_clear_flags(&out,1)!=0)return 2;
            trace[written++]=(trace_entry){"flags",out.gold_or_flags,get(&out,1)};
            if(fd2_field_prepare_revive_restore_hp(&out,1)!=0)return 2;
            trace[written++]=(trace_entry){"hp",out.gold_or_flags,get(&out,1)};result=1;
        }
        snprintf(id,sizeof(id),"revive-%d",mode);emit(id,"revive",&in,&out,mode,1,result,NULL,NULL,NULL,0,trace,written);
    }
    const uint32_t monies[]={1000,0,UINT32_MAX,0x80000000u};
    for(int subtract=0;subtract<2;subtract++)for(size_t i=0;i<4;i++){
        init(&in,1);in.gold_or_flags=monies[i];out=in;
        result=subtract ? fd2_field_prepare_money_subtract(&out.gold_or_flags,1):fd2_field_prepare_money_add(&out.gold_or_flags,1);
        snprintf(id,sizeof(id),"money-%s-%u",subtract ? "sub":"add",(unsigned)i);
        emit(id,"money",&in,&out,subtract,1,result,NULL,NULL,NULL,0,NULL,0);
    }
    for(int slot=0;slot<8;slot++){
        init(&in,2);fd2_field_unit u=get(&in,1);for(size_t k=0;k<8;k++){((uint8_t *)&u)[10+2*k]=(uint8_t)(k%3u==0 ? 0xc0:k%3u==1 ? 0:0x40);((uint8_t *)&u)[11+2*k]=(uint8_t)(k%2u ? 128u+k:k);}
        put(&in,1,&u);out=in;result=fd2_field_prepare_item_remove(&u,(size_t)slot);put(&out,1,&u);
        snprintf(id,sizeof(id),"item-remove-%d",slot);emit(id,"remove",&in,&out,1,slot,result,NULL,NULL,NULL,0,NULL,0);
        out=in;u=get(&out,1);result=fd2_field_prepare_item_equip(&u,(size_t)slot);put(&out,1,&u);
        snprintf(id,sizeof(id),"item-equip-%d",slot);emit(id,"equip",&in,&out,1,slot,result,NULL,NULL,NULL,0,NULL,0);
    }
    for(int full=0;full<2;full++){
        init(&in,2);fd2_field_unit u=get(&in,1);for(size_t k=0;k<8;k++)((uint8_t *)&u)[10+2*k]=full ? 0: k==3 ? 0x80:0;
        put(&in,1,&u);out=in;result=fd2_field_prepare_item_append(&u,214);put(&out,1,&u);
        emit(full ? "item-append-full":"item-append-hole","append",&in,&out,1,214,result,NULL,NULL,NULL,0,NULL,0);
    }
    return ferror(stdout) ? 2:0;
}
