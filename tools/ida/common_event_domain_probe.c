/* G4真实C有限领域输出，供独立原机器码比较；不初始化SDL/窗口。
 * 仅执行已生成的common.turn.relative/common.random.pair、4df4c纯写点
 * 及20926的typed turn谓词；全96*80B/32B/48B保留，非自然游戏夹具。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "field_rng.h"
#include "static_data.h"
#include "field_common_event_flow.h"

typedef struct {
    fd2_field_rng rng; unsigned calls; const fd2_field_game *game;
    uint8_t units[96u*80u], state[32], slots[48];
} probe_rng;
static uint32_t next_rng(void *p) {
    probe_rng *r=p; r->calls++;
    memcpy(r->units,r->game->units.items,sizeof(r->units));
    memcpy(r->state,r->game->cell_action_completed,sizeof(r->state));
    memcpy(r->slots,r->game->metadata.turn_events,sizeof(r->slots));
    return fd2_field_rng_next(&r->rng);
}
static void hex(const void *data,size_t n) { const uint8_t *p=data; for(size_t i=0;i<n;i++) printf("%02x",p[i]); }
static int run(fd2_field_game *g,const char *id,size_t actor) {
    const fd2_static_flow *f=fd2_static_flow_lookup("common_event",id); if(!f)return -1;
    int pc=f->entry;
    for(unsigned n=0;n<FD2_FLOW_STEP_BUDGET;n++) {
        const fd2_static_flow_node *v=fd2_static_flow_at(f,pc); if(!v)return -1;
        if(!strcmp(v->op,"end"))return 0;
        if(!strcmp(v->op,"fail"))return -1;
        unsigned port=FD2_FLOW_NEXT;
        if(common_event_domain(g,actor,v,&port,0)!=1)return -1;
        pc=fd2_static_flow_edge(f,v,port);
    }
    return -1;
}
static void init(fd2_field_game *g,probe_rng *r,size_t count,uint16_t seed) {
    memset(g,0,sizeof(*g));g->units.count=count;g->roster_count=0;
    for(size_t i=0;i<96;i++)for(size_t b=0;b<80;b++)((uint8_t *)&g->units.items[i])[b]=(uint8_t)(i*13u+b*17u+7u);
    for(size_t i=0;i<32;i++)g->cell_action_completed[i]=(uint8_t)(i*7u+3u);
    for(size_t i=0;i<16;i++)g->metadata.turn_events[i]=(fd2_field_turn_event){(uint8_t)(i+10u),(uint8_t)(i+40u),(uint8_t)(i+80u)};
    fd2_field_rng_seed(&r->rng,seed);r->calls=0;r->game=NULL;g->attack_rng=next_rng;g->attack_rng_userdata=r;
}
static void emit(const char *id,const char *kind,const fd2_field_game *before,
        const fd2_field_game *after,uint16_t seed,const probe_rng *r,int actor,int result,
        const uint32_t *input_cells,size_t cell_count) {
    printf("{\"case_id\":\"%s\",\"kind\":\"%s\",\"count\":%u,\"actor\":%d,\"turn\":%u,\"seed\":%u,\"result\":%d,\"rng_after\":%u,\"rng_calls\":%u,\"input_units_hex\":\"",
        id,kind,(unsigned)before->units.count,actor,before->turn_number,seed,result,r->rng.state,r->calls);
    hex(before->units.items,sizeof(before->units.items));printf("\",\"finish_units_hex\":\"");hex(after->units.items,sizeof(after->units.items));
    printf("\",\"input_state_hex\":\"");hex(before->cell_action_completed,32);printf("\",\"finish_state_hex\":\"");hex(after->cell_action_completed,32);
    printf("\",\"input_turn_slots_hex\":\"");hex(before->metadata.turn_events,48);printf("\",\"finish_turn_slots_hex\":\"");hex(after->metadata.turn_events,48);
    printf("\",\"width\":%d,\"height\":%d,\"input_cells_hex\":\"",before->map.width,before->map.height);hex(input_cells,cell_count*4);
    printf("\",\"finish_cells_hex\":\"");hex(after->map.cells,cell_count*4);
    printf("\",\"rng_input_units_hex\":\"");if(r->calls)hex(r->units,sizeof(r->units));
    printf("\",\"rng_input_state_hex\":\"");if(r->calls)hex(r->state,sizeof(r->state));
    printf("\",\"rng_input_slots_hex\":\"");if(r->calls)hex(r->slots,sizeof(r->slots));puts("\"}");
}
int main(void) {
    fd2_field_game *before=malloc(sizeof(*before)),*after=malloc(sizeof(*after));
    if(!before||!after)return 1;
    probe_rng rng;char id[80];
    for(unsigned side=0;side<2;side++)for(unsigned gate=0;gate<2;gate++)for(unsigned enabled=0;enabled<2;enabled++) {
        init(before,&rng,96,0x7a18);before->turn_number=255;
        before->units.items[1].side=(uint8_t)(side ? 2:0);before->cell_action_completed[17]=(uint8_t)gate;before->cell_action_completed[18]=(uint8_t)enabled;
        *after=*before;rng.game=after;int result=run(after,"common.turn.relative",1);if(result)return 2;
        snprintf(id,sizeof(id),"relative-%u-%u-%u",side,gate,enabled);
        emit(id,"relative",before,after,0x7a18,&rng,1,result,NULL,0);
    }
    const uint16_t seeds[]={0,1,0x7a18,0xffff};
    for(unsigned s=0;s<4;s++)for(unsigned n=0;n<2;n++) {
        size_t count=n ? 96u:3u;init(before,&rng,count,seeds[s]);before->turn_number=255;
        before->cell_action_completed[21]=(uint8_t)(count-3u);*after=*before;rng.game=after;
        int result=run(after,"common.random.pair",0);if(result)return 3;
        snprintf(id,sizeof(id),"pair-%u-%u",(unsigned)count,seeds[s]);
        emit(id,"pair",before,after,seeds[s],&rng,0,result,NULL,0);
    }
    const uint32_t turns[]={0,6,7,UINT32_C(0x7fffffff),UINT32_C(0x80000000),UINT32_MAX};
    for(unsigned i=0;i<6;i++)for(unsigned hidden=0;hidden<2;hidden++) {
        init(before,&rng,96,0x7a18);before->turn_number=turns[i];
        before->units.items[64].flags=(uint8_t)(0xa0u|hidden);*after=*before;
        int result=fd2_field_event_compare_turn(after->turn_number,FD2_EVENT_GT,6);
        if(result<0)return 4;
        result=result && fd2_field_unit_is_hidden(&after->units.items[64]);
        snprintf(id,sizeof(id),"turn-%u-%u",turns[i],hidden);
        emit(id,"turn",before,after,0x7a18,&rng,64,result,NULL,0);
    }
    for(unsigned pattern=0;pattern<2;pattern++) {
        uint32_t input[6],output[6];
        for(unsigned i=0;i<6;i++)input[i]=pattern ? UINT32_C(0xa5fce380)+(uint32_t)i*UINT32_C(0x10203):UINT32_MAX;
        memcpy(output,input,sizeof(input));init(before,&rng,96,0x7a18);
        before->map=(fd2_field_map){.width=3,.height=2,.cells=input};*after=*before;after->map.cells=output;
        int result=fd2_field_event_map_reset_cells(&after->map);if(result)return 5;
        snprintf(id,sizeof(id),"reset-%u",pattern);emit(id,"reset",before,after,0x7a18,&rng,0,result,input,6);
    }
    free(before);free(after);return 0;
}
