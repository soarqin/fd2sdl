/* B6 FULL0..9集成：field_ai_magic_action_execute @0x15311（原同名）→
 * field_magic_battle_animation @0x2ff01（原FUN_00055115）及sub_2B996 @0x2b996。
 * impact表取原机器kernel向量；真实资源只读。此测试不启动SDL/窗口/音频设备，
 * 不把skip/quit的离屏重放当作原版画面、声音或随机流实机验收。 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "battle_animation.h"
#include "field_game.h"
#include "field_rng.h"
#include "figani.h"

#define CHECK(e) do { if (!(e)) { fprintf(stderr,"CHECK %s:%d: %s\n",__FILE__,__LINE__,#e); return -1; } } while (0)
#define OCHECK(e) do { if (!(e)) { fprintf(stderr,"observer id%u phase%d line%d: %s\n",o->id,(int)eve->phase,__LINE__,#e); o->bad=1; return -1; } } while (0)
#define SAMPLE_CAP 4096u
#define TARGET_CAP 3u
static const unsigned impacts[10][3] = {
    {7,7,7},{8,8,8},{6,6,6},{14,20,20},{6,6,6},
    {9,11,11},{11,12,12},{7,8,8},{16,0,0},{27,0,0}
};
static const unsigned denominators[10] = {7,8,6,13,6,6,5,5,16,20};

typedef struct { uint32_t value, xp; uint16_t hp[TARGET_CAP], mp; } sample;
typedef struct {
    fd2_field_game *game;
    fd2_vga *vga;
    fd2_battle_animation renderer;
    fd2_field_rng rng;
    int zero, overflow;
    size_t calls, target_count;
    uint8_t targets[TARGET_CAP];
    sample samples[SAMPLE_CAP];
} fixture;
typedef struct {
    unsigned seen[4], impacts[TARGET_CAP];
    size_t calls[5], tape_count[5];
    uint32_t tape[5][512];
    fd2_field_magic_basic_result effect[TARGET_CAP];
    uint16_t target_hp[TARGET_CAP];
    uint32_t target_xp[TARGET_CAP], finish_xp;
    int8_t jitter[TARGET_CAP][FD2_FIELD_MAGIC_IMPACT_CAPACITY];
    int failed_finish;
} observation;
typedef struct {
    fixture *f;
    observation out;
    uint8_t id, cost_count;
    unsigned next;
    int renderer_mode, fail_phase, resource_phase, reject_tape_phase, bad;
    fd2_archive invalid_archive;
    fd2_field_game *before;
} observer;
typedef struct {
    fd2_field_units units;
    size_t calls;
    uint16_t rng_state;
    sample samples[SAMPLE_CAP];
    observation events;
} outcome;

static uint32_t random_value(void *data) {
    fixture *f = data;
    uint32_t value = f->zero ? 0u : fd2_field_rng_next(&f->rng);
    if (f->calls >= SAMPLE_CAP) { f->overflow=1; return value; }
    sample *s=&f->samples[f->calls++];
    s->value=value; s->xp=f->game->experience_value; s->mp=f->game->units.items[1].mp;
    for (size_t i=0;i<f->target_count;i++) s->hp[i]=f->game->units.items[f->targets[i]].hp;
    return value;
}

/* 保留真实START的完整记录作底，再显式设置本例已知字段。远处raw0保持
 * stage0主角存活，raw4保留敌方，使目标死亡不抢跑到结算/下一轮RNG。 */
static void place(fd2_field_unit *u,uint8_t id,uint8_t side,uint8_t x,uint8_t y) {
    u->unit_id=u->text_id=id; u->side=side;u->x=x;u->y=y;
    u->flags=u->direction=u->frame_phase=0;u->level=2;
    u->attack_status=u->defense_status=u->hit_evasion_status=0;
    memset(u->detail_status,0,sizeof(u->detail_status));
    memset(u->data_09_1e,0,sizeof(u->data_09_1e));
    ((uint8_t *)u)[0x31]=0xff;
    u->hp=u->hp_max=100;u->mp=u->mp_max=500;
    u->movement_profile=1;u->movement_points=2;
}
static void close_fixture(fixture *f) {
    fd2_battle_animation_close(&f->renderer);
    if(f->game)fd2_field_game_close(f->game);
    free(f->game);free(f->vga);free(f);
}
static fixture *open_fixture(const fd2_archive *fdother,uint8_t id,size_t targets,
        int zero,uint16_t seed,uint8_t side) {
    fixture *f=calloc(1,sizeof(*f));if(!f)return NULL;
    f->game=calloc(1,sizeof(*f->game));f->vga=calloc(1,sizeof(*f->vga));
    if(!f->game || !f->vga || fd2_field_game_open_resources(f->game,fdother,0)!=0 ||
        fd2_field_game_load_active(f->game,"original_game/START.SAV")!=0) goto fail;
    fd2_field_game *g=f->game;
    fd2_field_unit base=g->units.items[1];
    for(size_t i=0;i<5;i++)g->units.items[i]=base;
    g->units.count=5;
    place(&g->units.items[0],0,2,23,23);
    place(&g->units.items[1],9,side,8,14);
    place(&g->units.items[2],96,side ? 0:2,9,14);
    place(&g->units.items[3],97,side ? 0:2,9,15);
    place(&g->units.items[4],96,0,23,21);
    if(targets==1)g->units.items[3].flags=FD2_FIELD_UNIT_FLAG_HIDDEN;
    /* radius0原collector允许同格的不同raw；ID8是合法共享FX续态，ID9应拒绝。 */
    if(id>=8 && targets>1) { g->units.items[3].x=9;g->units.items[3].y=14; }
    g->units.items[1].data_09_1e[0x11u+id/8u]=(uint8_t)(1u<<(id%8u));
    g->active_side=side;g->selected_unit=g->attack_target=g->detail_unit=-1;
    g->interaction=FD2_FIELD_INTERACTION_BROWSE;g->failure=FD2_FIELD_FAILURE_NONE;
    g->option_values[1]=1;g->option_values[2]=0;
    g->experience_value=7;g->event_log_count=0;g->pending_cell_event.valid=0;
    memset(g->metadata.turn_events,0xff,sizeof(g->metadata.turn_events));
    memset(g->metadata.cell_lookup,0xff,sizeof(g->metadata.cell_lookup));
    memset(&g->progress,0,sizeof(g->progress));memset(&g->loot,0,sizeof(g->loot));
    g->attack_rng=random_value;g->attack_rng_userdata=f;
    f->zero=zero;fd2_field_rng_seed(&f->rng,seed);
    memset(f->vga->framebuffer,201,sizeof(f->vga->framebuffer));
    if(fd2_battle_animation_open(&f->renderer,f->vga,NULL,NULL,fdother,&g->figani_archive)!=0)goto fail;
    return f;
fail:
    close_fixture(f);return NULL;
}
static int query(fixture *f,uint8_t id,fd2_field_ai_magic_candidate *plan) {
    fd2_field_game *g=f->game;
    uint8_t selector=g->active_side ? 1:0;
    CHECK(fd2_field_ai_choose_magic_candidate(&g->map,&g->terrain,&g->units,1,selector,plan)==0);
    CHECK(plan->magic_id==id && plan->score>=6);
    CHECK(fd2_field_ai_collect_magic_targets(&g->map,&g->terrain,&g->units,id,
        plan->cast_x,plan->cast_y,selector,f->targets,TARGET_CAP,&f->target_count)==0);
    CHECK(f->target_count>0 && f->target_count<=TARGET_CAP);
    return 0;
}
static int observe(const fd2_field_game *g,const fd2_field_magic_present_event *eve,void *data) {
    observer *o=data;fixture *f=o->f;
    OCHECK(g==f->game && eve->actor_index==1 && eve->magic_id==o->id && eve->phase<=3);
    OCHECK(eve->target_count==f->target_count && eve->cast_cost_count==o->cost_count);
    OCHECK(!memcmp(eve->targets,f->targets,f->target_count));
    o->out.seen[eve->phase]++;
    size_t slot=eve->phase==FD2_FIELD_MAGIC_PRESENT_FINISH ? 4u:
        eve->phase==FD2_FIELD_MAGIC_PRESENT_TARGET ? 2u+o->next:(size_t)eve->phase;
    OCHECK(slot<5 && eve->fx_random_count<=512);
    o->out.calls[slot]=f->calls;o->out.tape_count[slot]=eve->fx_random_count;
    if(eve->fx_random_count)memcpy(o->out.tape[slot],eve->fx_random,eve->fx_random_count*sizeof(uint32_t));
    if(eve->phase==FD2_FIELD_MAGIC_PRESENT_PREPARE) {
        OCHECK(!f->calls && !eve->fx_random_count && g->units.items[1].mp==500);
        OCHECK(eve->actor_before && eve->actor_before->mp==500);
    } else if(eve->phase==FD2_FIELD_MAGIC_PRESENT_CAST) {
        OCHECK(g->units.items[1].mp==(uint16_t)(500u-fd2_field_magic_record_get(o->id)[5]*o->cost_count));
        OCHECK(g->experience_value==7 && !eve->result && !eve->target_before);
        OCHECK(f->calls==(size_t)((o->id==4 || o->id==5) ? 6:0));
    } else if(eve->phase==FD2_FIELD_MAGIC_PRESENT_TARGET) {
        OCHECK(o->next<f->target_count && eve->target_index==f->targets[o->next]);
        OCHECK(eve->target_before && eve->result && eve->result->hp_before==eve->target_before->hp);
        OCHECK(!fd2_field_unit_has_acted(&g->units.items[1]));
        unsigned n=o->next++;
        o->out.effect[n]=*eve->result;o->out.target_hp[n]=g->units.items[eve->target_index].hp;
        o->out.target_xp[n]=g->experience_value;o->out.impacts[n]=eve->impact_count;
        memcpy(o->out.jitter[n],eve->impact_jitter,eve->impact_count);
        if(f->zero) {
            OCHECK(eve->result->hit && eve->result->raw_damage>0);
            OCHECK(eve->impact_count==impacts[o->id][n]);
            OCHECK(g->units.items[eve->target_index].hp==
                (eve->impact_count ? eve->result->hp_after:eve->result->hp_before));
            for(size_t j=0;j<eve->impact_count;j++)OCHECK(eve->impact_jitter[j]==1);
        }
    } else {
        OCHECK(!eve->result && !eve->target_before && !eve->actor_before);
        o->out.failed_finish=eve->action_failed;o->out.finish_xp=g->experience_value;
    }
    if(o->renderer_mode) {
        memcpy(o->before,g,sizeof(*g));
        const fd2_archive *old=f->renderer.fdother;
        if((int)eve->phase==o->resource_phase)f->renderer.fdother=&o->invalid_archive;
        fd2_field_magic_present_event damaged=*eve;
        uint32_t invalid_values[512];
        if((int)eve->phase==o->reject_tape_phase) {
            OCHECK(eve->fx_random_count>0);
            memcpy(invalid_values,eve->fx_random,eve->fx_random_count*sizeof(uint32_t));
            invalid_values[0]=UINT16_MAX+1u;damaged.fx_random=invalid_values;
        }
        int result=fd2_battle_animation_magic_present(g,&damaged,&f->renderer);
        f->renderer.fdother=old;
        OCHECK(!memcmp(o->before,g,sizeof(*g)));
        if(result!=0)return result;
    }
    return (int)eve->phase==o->fail_phase ? -1:0;
}
static void init_observer(observer *o,fixture *f,uint8_t id,int mode) {
    memset(o,0,sizeof(*o));o->f=f;o->id=id;o->renderer_mode=mode>=2;
    o->fail_phase=o->resource_phase=o->reject_tape_phase=-1;o->before=malloc(sizeof(*o->before));
    fd2_figani_magic_cast cast;
    if(fd2_figani_magic_cast_plan(&cast,&f->game->figani_archive,9)==0)o->cost_count=cast.cost_marker_count;
    if(mode==2)fd2_battle_animation_set_skip(&f->renderer,1);
    if(mode==3)f->vga->input.quit_requested=1;
    if(mode) { f->game->magic_present=observe;f->game->magic_present_userdata=o; }
}
static int run(const fd2_archive *fdother,uint8_t id,size_t count,int mode,int zero,
        uint16_t seed,outcome *out) {
    fixture *f=open_fixture(fdother,id,count,zero,seed,0);CHECK(f);
    fd2_field_ai_magic_candidate plan;CHECK(query(f,id,&plan)==0 && f->target_count==count);
    observer *o=calloc(1,sizeof(*o));CHECK(o);init_observer(o,f,id,mode);CHECK(o->before);
    CHECK(fd2_field_game_commit_ai_magic(f->game,1,0,&plan)==0);
    CHECK(!f->overflow && !o->bad && !f->game->failure);
    CHECK(f->game->experience_value==0 && fd2_field_unit_has_acted(&f->game->units.items[1]));
    CHECK(!f->renderer.magic_exchange && !f->renderer.exchange && !f->renderer.failed);
    CHECK(!f->vga->renderer && !f->vga->texture && !f->vga->frame_deadline_ns);
    /* skip正常尾允许纯画回当前地图；quit才恢复进入前快照。两者均无宿主呈现。 */
    if(mode==3)for(size_t i=0;i<sizeof(f->vga->framebuffer);i++)CHECK(f->vga->framebuffer[i]==201);
    CHECK(SDL_WasInit(0)==0);
    if(mode==3)CHECK(f->renderer.host_quit && f->vga->input.quit_requested);
    if(mode) {
        CHECK(o->out.seen[0]==1 && o->out.seen[1]==1 && o->out.seen[2]==count && o->out.seen[3]==1);
        size_t fx=0,hits=0,jitters=0;
        for(size_t i=0;i<5;i++)fx+=o->out.tape_count[i];
        uint32_t xp=7;
        for(size_t i=0;i<count;i++) { hits+=o->out.effect[i].hit!=0;jitters+=o->out.impacts[i];
            xp+=o->out.effect[i].experience_value;CHECK(o->out.target_xp[i]==xp); }
        CHECK(o->out.finish_xp==xp && f->calls==count+hits+jitters+fx);
        if(zero && count==1) {
            CHECK(fx==(size_t)((id==4 || id==5) ? 10:0));
            /* 非随机FX的每次jitter入口已见刚写入HP；第2次effect RNG仍见before。 */
            if(id!=4 && id!=5) {
                CHECK(f->samples[0].hp[0]==100 && f->samples[1].hp[0]==100);
                int before=100,after=(int16_t)o->out.effect[0].hp_after;
                for(unsigned i=0;i<impacts[id][0];i++) {
                    unsigned step=i+1<denominators[id] ? i+1:denominators[id];
                    CHECK(f->samples[2+i].hp[0]==(uint16_t)(before-(int)step*(before-after)/(int)denominators[id]));
                }
            }
        }
    }
    memset(out,0,sizeof(*out));out->units=f->game->units;out->calls=f->calls;out->rng_state=f->rng.state;
    memcpy(out->samples,f->samples,f->calls*sizeof(sample));out->events=o->out;
    free(o->before);free(o);close_fixture(f);return 0;
}
static int compare_modes(const fd2_archive *fdother,uint8_t id,size_t count,int zero,uint16_t seed) {
    outcome *baseline=calloc(1,sizeof(*baseline)),*observer_result=calloc(1,sizeof(*baseline));
    outcome *current=calloc(1,sizeof(*baseline));CHECK(baseline && observer_result && current);
    for(int mode=0;mode<4;mode++) {
        CHECK(run(fdother,id,count,mode,zero,seed,current)==0);
        if(!mode)*baseline=*current;
        else { CHECK(!memcmp(&baseline->units,&current->units,sizeof(current->units)));
            CHECK(baseline->calls==current->calls && baseline->rng_state==current->rng_state);
            CHECK(!memcmp(baseline->samples,current->samples,current->calls*sizeof(sample)));
            if(mode==1)*observer_result=*current;
            else CHECK(!memcmp(&observer_result->events,&current->events,sizeof(current->events))); }
    }
    if(zero && id==8 && count>1) {
        CHECK(observer_result->events.impacts[1]==0 && observer_result->events.effect[1].raw_damage>0);
        CHECK(observer_result->events.effect[1].hp_after<observer_result->events.effect[1].hp_before);
        CHECK(observer_result->events.target_hp[1]==observer_result->events.effect[1].hp_before);
    }
    printf("FULL id%u targets%zu %s seed%u RNG%zu: NULL/observer/skip/quit equal\n",
        id,count,zero ? "zero":"native16",seed,baseline->calls);
    free(baseline);free(observer_result);free(current);return 0;
}
static int test_rejection_and_postcommit_failure(const fd2_archive *fdother) {
    for(int phase=0;phase<4;phase++) {
        fixture *f=open_fixture(fdother,4,1,1,0,0);CHECK(f);
        fd2_field_ai_magic_candidate plan;CHECK(query(f,4,&plan)==0);
        observer *o=calloc(1,sizeof(*o));CHECK(o);init_observer(o,f,4,1);CHECK(o->before);o->fail_phase=phase;
        fd2_field_units before=f->game->units;
        CHECK(fd2_field_game_commit_ai_magic(f->game,1,0,&plan)==-1 && !o->bad);
        CHECK(f->game->failure==FD2_FIELD_FAILURE_ACTION && o->out.seen[0]==1);
        CHECK(o->out.seen[3]==(unsigned)(phase!=0));
        if(!phase) { CHECK(!f->calls && !memcmp(&before,&f->game->units,sizeof(before))); }
        else { CHECK(f->game->units.items[1].mp==496);
            CHECK(f->calls==(size_t)(phase==1 ? 6:18));
            CHECK(f->game->units.items[2].hp==(phase==1 ? 100:o->out.effect[0].hp_after));
            CHECK(o->out.failed_finish==(phase!=3)); }
        memcpy(o->before,f->game,sizeof(*o->before));size_t calls=f->calls;observation events=o->out;
        CHECK(fd2_field_game_commit_ai_magic(f->game,1,0,&plan)==-1);
        CHECK(!memcmp(o->before,f->game,sizeof(*o->before)) && calls==f->calls &&
            !memcmp(&events,&o->out,sizeof(events)));
        free(o->before);free(o);close_fixture(f);
    }
    /* 真实renderer资源拒绝在PREPARE，不以跳过演出绕过bank/palette预检。 */
    fixture *f=open_fixture(fdother,0,1,1,0,0);CHECK(f);
    fd2_field_ai_magic_candidate plan;CHECK(query(f,0,&plan)==0);
    observer *o=calloc(1,sizeof(*o));CHECK(o);init_observer(o,f,0,2);CHECK(o->before);o->resource_phase=0;
    fd2_field_units before=f->game->units;
    CHECK(fd2_field_game_commit_ai_magic(f->game,1,0,&plan)==-1 && !f->calls);
    CHECK(!memcmp(&before,&f->game->units,sizeof(before)) && o->out.seen[0]==1 && !o->out.seen[3]);
    CHECK(f->renderer.failed && !f->renderer.magic_exchange);
    free(o->before);free(o);close_fixture(f);
    /* CAST/TARGET真实renderer拒绝非法16bit FX记录，错误只来自私有callback
     * 借用view，不改原Game tape、effect或RNG；FINISH必须清一次exchange。 */
    for(int phase=1;phase<=2;phase++) {
        f=open_fixture(fdother,4,1,1,0,0);CHECK(f);CHECK(query(f,4,&plan)==0);
        o=calloc(1,sizeof(*o));CHECK(o);init_observer(o,f,4,2);CHECK(o->before);
        o->reject_tape_phase=phase;
        CHECK(fd2_field_game_commit_ai_magic(f->game,1,0,&plan)==-1 && !o->bad);
        CHECK(f->game->failure==FD2_FIELD_FAILURE_ACTION && f->renderer.failed);
        CHECK(!f->renderer.magic_exchange && o->out.seen[3]==1 && o->out.failed_finish);
        CHECK(f->calls==(size_t)(phase==1 ? 6:18) && f->game->units.items[1].mp==496);
        CHECK(f->game->units.items[2].hp==(phase==1 ? 100:o->out.effect[0].hp_after));
        memcpy(o->before,f->game,sizeof(*o->before));size_t calls=f->calls;
        CHECK(fd2_field_game_commit_ai_magic(f->game,1,0,&plan)==-1);
        CHECK(!memcmp(o->before,f->game,sizeof(*o->before)) && f->calls==calls && o->out.seen[3]==1);
        free(o->before);free(o);close_fixture(f);
    }
    f=open_fixture(fdother,0,1,1,0,0);CHECK(f);CHECK(query(f,0,&plan)==0);
    fd2_field_game *whole=malloc(sizeof(*whole));CHECK(whole);memcpy(whole,f->game,sizeof(*whole));
    plan.score++;
    CHECK(fd2_field_game_commit_ai_magic(f->game,1,0,&plan)==-1 && !f->calls);
    CHECK(!memcmp(whole,f->game,sizeof(*whole)));free(whole);close_fixture(f);
    /* 不可播放metadata在业务effect前拒绝；无猜测fallback或重复施法。 */
    for(int kind=0;kind<2;kind++) {
        f=open_fixture(fdother,9,kind ? 1:2,1,0,kind ? 1:0);CHECK(f);
        CHECK(query(f,9,&plan)==0 && f->target_count==(size_t)(kind ? 1:2));
        before=f->game->units;
        CHECK(fd2_field_game_commit_ai_magic(f->game,1,(uint8_t)kind,&plan)==-1 && !f->calls);
        CHECK(!memcmp(&before,&f->game->units,sizeof(before)));
        CHECK(f->game->failure==FD2_FIELD_FAILURE_ACTION);
        close_fixture(f);
    }
    return 0;
}
int main(void) {
    fd2_archive fdother={0};CHECK(fd2_archive_open(&fdother,"original_game/FDOTHER.DAT")==0);
    for(uint8_t id=0;id<10;id++) {
        CHECK(compare_modes(&fdother,id,1,1,0)==0);
        CHECK(compare_modes(&fdother,id,1,0,0x7a18)==0);
    }
    const uint8_t multi[]={3,5,6,7,8};
    for(size_t i=0;i<sizeof(multi);i++) {
        CHECK(compare_modes(&fdother,multi[i],2,1,0)==0);
        CHECK(compare_modes(&fdother,multi[i],2,0,0x1234)==0);
    }
    CHECK(test_rejection_and_postcommit_failure(&fdother)==0);
    fd2_archive_close(&fdother);
    puts("field_magic_full_test: 10 singles/5 multis x 2 streams x 4 modes, failure boundaries passed (headless only)");
    return 0;
}
