/* S2：完整205b4..20c6f结果族的候选const图／既有common续态回归。
 * --s2-vectors由独立原机器码oracle喂原80B记录；story是明确ACK替身，
 * 不生成原义expected，不声称真实输入／文本RNG／自然新关卡已经验收。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "field_game.h"
#include "field_common_event_flow.h"

#define CHECK(x) do { if (!(x)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#x); return -1; } } while (0)

typedef struct { unsigned fragment; fd2_field_battle_result latch; } story_entry;
static unsigned rng_calls;
static uint32_t count_rng(void *userdata) { (*(unsigned *)userdata)++;return 7; }

static void setup(fd2_field_game *game, size_t count) {
    memset(game,0,sizeof(*game)); game->ready=1; game->units.count=count;
    rng_calls=0;game->attack_rng=count_rng;game->attack_rng_userdata=&rng_calls;
    for(size_t i=0;i<count;i++) { game->units.items[i].side=1;game->units.items[i].hp=100; }
    if(count)game->units.items[0].side=2;
    if(count>1)game->units.items[1].side=0;
}

/* 除原53ECC和既有event_resume外，整个Game（含RNG／存档记录／owner指针）守卫。 */
static int same_owner(const fd2_field_game *game,const fd2_field_game *before) {
    fd2_field_game *copy=malloc(sizeof(*copy));if(!copy)return 0;
    *copy=*game;copy->battle_result=before->battle_result;copy->event_resume=before->event_resume;
    int equal=!memcmp(copy,before,sizeof(*copy)) && !rng_calls;free(copy);return equal;
}

static int run(fd2_field_game *game,const char *id,unsigned stop,
        fd2_field_battle_result *result,story_entry stories[4],size_t *count,int *stopped) {
    *count=0;*stopped=0;
    if(!strncmp(id,"stage.result.",13))return fd2_field_game_result_query_flow(game,id,result);
    if(strncmp(id,"common.result.source_",21) || fd2_field_game_common_event_begin(game,id,0)!=0)return -1;
    while(game->event_resume.active) {
        fd2_static_flow_node node;size_t actor;
        if(fd2_field_game_common_event_peek(game,&node,&actor)!=1 || actor!=0 ||
                strcmp(node.op,"story") || *count>=4)return -1;
        stories[*count]=(story_entry){(unsigned)node.args[0],game->battle_result};(*count)++;
        if(stop==*count){*stopped=1;break;}
        if(fd2_field_game_common_event_reply(game,FD2_FLOW_ACK)!=0)return -1;
    }
    *result=game->battle_result;return 0;
}

static int decode(const char *text,uint8_t *out,size_t capacity,size_t *size) {
    size_t len=strlen(text);*size=0;
    if(!strcmp(text,"-"))return 0;
    if(len%2 || len/2>capacity)return -1;
    for(size_t i=0;i<len;i+=2) {
        unsigned value=0;
        for(size_t j=0;j<2;j++) {
            char ch=text[i+j];unsigned digit;
            if(ch>='0' && ch<='9')digit=(unsigned)(ch-'0');
            else if(ch>='a' && ch<='f')digit=(unsigned)(ch-'a'+10);
            else return -1;
            value=value*16+digit;
        }
        out[(*size)++]=(uint8_t)value;
    }
    return 0;
}

static int vectors(void) {
    char line[32768],id[80],state[65],roster[15361],units[15361],extra;
    fd2_field_game *game=malloc(sizeof(*game)),*before=malloc(sizeof(*before));
    if(!game || !before){free(game);free(before);return 1;}
    int failed=0;
    while(fgets(line,sizeof(line),stdin)) {
        unsigned n,turn,initial,stop;size_t state_size,roster_size,unit_size;
        if(!strchr(line,'\n') || sscanf(line,"%79s %u %u %u %u %64s %15360s %15360s %c",
                id,&n,&turn,&initial,&stop,state,roster,units,&extra)!=8 || n>96 ||
                initial>2 || stop>2){failed=1;break;}
        setup(game,n);game->turn_number=turn;game->battle_result=(fd2_field_battle_result)initial;
        if(decode(state,game->cell_action_completed,32,&state_size)!=0 || state_size!=32 ||
           decode(roster,(uint8_t *)game->roster,sizeof(game->roster),&roster_size)!=0 || roster_size%80 ||
           decode(units,(uint8_t *)game->units.items,sizeof(game->units.items),&unit_size)!=0 || unit_size!=n*80u) {
            failed=1;break;
        }
        game->roster_count=roster_size/80;*before=*game;
        fd2_field_battle_result result=FD2_FIELD_BATTLE_ONGOING;story_entry stories[4];size_t count;int stopped;
        int status=run(game,id,stop,&result,stories,&count,&stopped);
        printf("{\"status\":%d,\"result\":%d,\"stopped_at_story\":%s,\"owner_unchanged\":%s,\"stories\":[",
            status,(int)result,stopped?"true":"false",same_owner(game,before)?"true":"false");
        for(size_t i=0;i<count;i++)printf("%s{\"fragment\":%u,\"latch\":%d}",i?",":"",stories[i].fragment,(int)stories[i].latch);
        puts("]}");
    }
    if(ferror(stdin))failed=1;
    free(game);free(before);return failed;
}

static int invalid_domains(void) {
    static const struct { const char *id;size_t minimum; } sources[]={
        {"stage.result.source_020707",52},{"stage.result.source_02073d",15},
        {"stage.result.source_020822",65},{"stage.result.source_02084a",66},
        {"stage.result.source_0208cf",53},{"stage.result.source_020926",1},
        {"stage.result.source_020a51",18},{"stage.result.source_020a87",2},
        {"stage.result.source_020aaf",19},{"stage.result.source_020b14",17},
        {"stage.result.source_020b3c",3}};
    fd2_field_game *game=malloc(sizeof(*game)),*before=malloc(sizeof(*before));CHECK(game && before);
    for(size_t i=0;i<sizeof(sources)/sizeof(*sources);i++) {
        setup(game,sources[i].minimum-1);*before=*game;
        fd2_field_battle_result result=FD2_FIELD_BATTLE_VICTORY;
        CHECK(fd2_field_game_result_query_flow(game,sources[i].id,&result)==-1 &&
              result==FD2_FIELD_BATTLE_VICTORY && !memcmp(game,before,sizeof(*game)));
    }
    setup(game,83);*before=*game;fd2_field_battle_result result=FD2_FIELD_BATTLE_VICTORY;
    CHECK(fd2_field_game_result_query_flow(game,"stage.result.030",&result)==-1 && result==FD2_FIELD_BATTLE_VICTORY);
    CHECK(fd2_field_game_result_query_flow(game,"common.result.source_020765",&result)==-1);
    CHECK(fd2_field_game_result_query_flow(game,"stage.entry.001",&result)==-1 && !memcmp(game,before,sizeof(*game)));
    /* result_set只写既有latch；预检和坏值不改Game，不经const谓词执行。 */
    unsigned port;fd2_static_flow_node set={.op="result_set",.args={2}};
    CHECK(common_event_query(game,SIZE_MAX,&set,&port)==0);
    CHECK(common_event_domain(game,0,&set,&port,1)==1 && !memcmp(game,before,sizeof(*game)));
    set.args[0]=3;CHECK(common_event_domain(game,0,&set,&port,0)==-1 && !memcmp(game,before,sizeof(*game)));
    free(game);free(before);return 0;
}

static int reject_story(const fd2_field_game *game,const fd2_static_flow_node *node,
        const fd2_field_event_resource_view *view,void *userdata) {
    (void)game;(void)view;
    if(!strcmp(node->op,"story")){(*(unsigned *)userdata)++;return -1;}
    return 0;
}

static int prefix_errors(void) {
    static const char *const ids[]={"common.result.source_020765","common.result.source_020872",
        "common.result.source_020957","common.result.source_020b72","common.result.source_020bf5"};
    fd2_field_game *game=malloc(sizeof(*game)),*before=malloc(sizeof(*before));CHECK(game && before);
    const size_t minimum[]={27,53,83,2,21};
    for(size_t i=0;i<5;i++) {
        setup(game,minimum[i]-1);game->battle_result=FD2_FIELD_BATTLE_VICTORY;*before=*game;
        CHECK(fd2_field_game_common_event_begin(game,ids[i],0)==-1 && !memcmp(game,before,sizeof(*game)));
        setup(game,83);unsigned rejected=0;game->event_preflight=reject_story;game->event_preflight_userdata=&rejected;
        if(i==0)for(size_t j=15;j<=26;j++)game->units.items[j].flags=1;
        if(i==1)game->units.items[52].flags=1;
        if(i==2)for(size_t j=53;j<=60;j++)game->units.items[j].flags=1;
        if(i>=3)game->units.items[1].flags=1;
        /* 确实到达文本的前置态；未到达的资源不要求失败。 */
        *before=*game;
        CHECK(!rng_calls);
        CHECK(fd2_field_game_common_event_begin(game,ids[i],0)==-1 && rejected && !memcmp(game,before,sizeof(*game)));
    }
    for(size_t i=0;i<5;i++)for(unsigned mode=0;mode<3;mode++) {
        setup(game,83);game->turn_number=7;
        if(i==0)for(size_t j=15;j<=26;j++)game->units.items[j].flags=1;
        if(i==0)game->units.items[59].flags=1;
        if(i==1)game->units.items[52].flags=1;
        if(i==2)for(size_t j=53;j<=60;j++)game->units.items[j].flags=1;
        if(i>=3)game->units.items[1].flags=1;
        if(i==4)game->units.items[20].flags=1;
        fd2_field_battle_result result;story_entry stories[4];size_t count;int stopped;
        CHECK(run(game,ids[i],1,&result,stories,&count,&stopped)==0 && stopped && count==1);
        *before=*game;
        if(mode==0) {
            CHECK(fd2_field_game_common_event_reply(game,FD2_FLOW_CANCEL)==-1 && !memcmp(game,before,sizeof(*game)));
            CHECK(fd2_field_game_common_event_reply(game,FD2_FLOW_NEXT)==-1 && !memcmp(game,before,sizeof(*game)));
        } else if(mode==1) {
            CHECK(fd2_field_game_common_event_reply(game,FD2_FLOW_ERROR)==-1 && game->battle_result==result &&
                  !memcmp(&game->units,&before->units,sizeof(game->units)) &&
                  !memcmp(game->cell_action_completed,before->cell_action_completed,32));
            *before=*game;
            CHECK(fd2_field_game_common_event_reply(game,FD2_FLOW_ACK)==-1 && !memcmp(game,before,sizeof(*game)));
        } else {
            game->runtime_host_quit=1;*before=*game;
            CHECK(fd2_field_game_common_event_reply(game,FD2_FLOW_ACK)==-1 && !memcmp(game,before,sizeof(*game)));
        }
    }
    /* 原30槽正常战场已由S5完整接入；内部30..32不增造普通战场。 */
    for(size_t stage=0;stage<30;stage++)CHECK(fd2_field_stage_battle_is_supported(stage));
    for(size_t stage=30;stage<=32;stage++)CHECK(!fd2_field_stage_battle_is_supported(stage));
    free(game);free(before);return 0;
}

int main(int argc,char **argv) {
    if(argc==2 && !strcmp(argv[1],"--s2-vectors"))return vectors();
    if(argc!=1 || invalid_domains()!=0 || prefix_errors()!=0)return 1;
    puts("field result source tests passed");return 0;
}
