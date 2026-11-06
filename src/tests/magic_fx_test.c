/* B6纯FX回归；29向量由原0x524c6十入口、真实part+6 duration与
 * 原4ebe3/__CHP/sin/cos机器码局部执行取得，仅draw/audio被替换为记录。
 * FNV覆盖每次phase返回、完整part/destination/cue顺序和每个FX RNG值。
 * 不代表DOSBox/SDL图形、声音或Game伤害/经验验收。
 * 原relocated镜像SHA与完整向量由私有Domain readset核对；不提交原素材。 */
#include "magic_fx.h"
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define CHECK(e) do { if (!(e)) { fprintf(stderr,"%s:%d: %s\n",__FILE__,__LINE__,#e); return 1; } } while (0)
#define RECORD_CAPACITY 16384u

typedef struct {
    uint8_t id, side;
    uint16_t seed;
    unsigned targets, rng_count, rng_final, impact_total;
    uint64_t hash;
    unsigned impacts[64];
} golden;
static const golden vectors[] = {
    {0,0,31256,3,0,31256,21,UINT64_C(0xc91e05817dc19831),{7,7,7}},
    {0,2,31256,3,0,31256,21,UINT64_C(0xd2362a9632ef4e0d),{7,7,7}},
    {1,0,31256,3,0,31256,24,UINT64_C(0xb02fa6e905060a1e),{8,8,8}},
    {1,2,31256,3,0,31256,24,UINT64_C(0xc96a77707647b386),{8,8,8}},
    {2,0,31256,3,0,31256,18,UINT64_C(0xa5aa47a1ed894741),{6,6,6}},
    {2,2,31256,3,0,31256,18,UINT64_C(0x7f431162af6b48f2),{6,6,6}},
    {3,0,31256,3,0,31256,54,UINT64_C(0xf562ed05c01613ce),{14,20,20}},
    {3,2,31256,3,0,31256,54,UINT64_C(0xad606be0706a989a),{14,20,20}},
    {4,0,31256,3,42,15699,18,UINT64_C(0xfb297c4082f17683),{6,6,6}},
    {4,2,31256,3,42,15699,18,UINT64_C(0x0f7b44aa2e013303),{6,6,6}},
    {5,0,31256,3,46,11135,31,UINT64_C(0x0bb098fe4d631339),{9,11,11}},
    {5,2,31256,3,46,11135,31,UINT64_C(0xfbe936e00af47dad),{9,11,11}},
    {6,0,31256,3,0,31256,35,UINT64_C(0xe2d5612512301471),{11,12,12}},
    {6,2,31256,3,0,31256,35,UINT64_C(0x0e5db2667e50df9a),{11,12,12}},
    {7,0,31256,3,0,31256,23,UINT64_C(0x77f7a7bbfdabb502),{7,8,8}},
    {7,2,31256,3,0,31256,23,UINT64_C(0xb29de0430fd703b2),{7,8,8}},
    {8,0,31256,3,0,31256,16,UINT64_C(0x398cc8b46229ef44),{16,0,0}},
    {8,2,31256,3,0,31256,16,UINT64_C(0x398cc8b46229ef44),{16,0,0}},
    {9,0,31256,1,0,31256,27,UINT64_C(0x9cac890815c2e637),{27}},
    {4,0,0,3,42,17030,18,UINT64_C(0x5904554f3ca9c456),{6,6,6}},
    {4,0,65535,3,42,33981,18,UINT64_C(0x3f6d6aff1cf513b3),{6,6,6}},
    {4,2,0,3,42,17030,18,UINT64_C(0x7e35d4effddbf98e),{6,6,6}},
    {4,2,65535,3,42,33981,18,UINT64_C(0x0ea74430c1d65b0f),{6,6,6}},
    {5,0,0,3,46,23050,31,UINT64_C(0xc5add4312d6132db),{9,11,11}},
    {5,0,65535,3,46,48629,31,UINT64_C(0xda0c8d9145587423),{9,11,11}},
    {5,2,0,3,46,23050,31,UINT64_C(0xc6d60c19010a13d7),{9,11,11}},
    {5,2,65535,3,46,48629,31,UINT64_C(0xbd8a3859b8c62dd7),{9,11,11}},
    {4,0,31256,64,1002,9850,384,UINT64_C(0xc6980656f1d50df8),{6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6,6}},
    {5,0,31256,64,1144,62119,702,UINT64_C(0x8bb3ffe914ae01d3),{9,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11}},

};
static const uint8_t durations_zero[255] = {0};
static const uint8_t durations_wheel[18] = {2,2,2,2,2,3,3,3,7,2,2,2,2,2,2,1,1,1};

typedef struct {
    uint64_t hash;
    uint16_t rng_state;
    uint32_t recorded[RECORD_CAPACITY];
    const uint32_t *replay;
    size_t replay_count;
    unsigned rng_count, emissions, phase;
} observed;

static void hash_event(observed *o, uint32_t kind, uint32_t phase,
        uint32_t value, uint32_t delta) {
    uint32_t words[4] = {kind,phase,value,delta};
    for (size_t i=0;i<4;i++) for (size_t b=0;b<4;b++) {
        o->hash ^= (words[i] >> (8u*b)) & 255u;
        o->hash *= UINT64_C(1099511628211);
    }
}
static uint32_t random_next(void *userdata) {
    observed *o=userdata;
    uint32_t value;
    if (o->rng_count >= RECORD_CAPACITY) return UINT32_MAX;
    if (o->replay) {
        if (o->rng_count >= o->replay_count) return UINT32_MAX;
        value=o->replay[o->rng_count];
    } else {
        uint16_t state=(uint16_t)(o->rng_state+0x9014u);
        state=(uint16_t)((state<<3)|(state>>13));
        o->rng_state=state; value=state;
    }
    o->recorded[o->rng_count++]=value;
    hash_event(o,3,o->phase,value,0);
    return value;
}
static int emitted(const fd2_magic_fx_emission *e, void *userdata) {
    observed *o=userdata;
    CHECK(e && e->phase==o->phase);
    uint32_t delta=(uint32_t)(e->x+640*e->y);
    hash_event(o,(uint32_t)e->kind,e->phase,e->index,delta);
    o->emissions++;
    return 0;
}
static int step(fd2_magic_fx *fx, observed *o, uint8_t phase,
        int visible, int *value) {
    o->phase=phase;
    int status=fd2_magic_fx_step(fx,phase,random_next,o,visible ? emitted : NULL,o,value);
    if(status) fprintf(stderr,"step failed id=%u side=%u phase=%u\n",fx->id,fx->side,phase);
    CHECK(status==0);
    hash_event(o,4,phase,(uint32_t)*value,0);
    return 0;
}
static int cast(fd2_magic_fx *fx, observed *o, const golden *v,
        int visible, unsigned impacts[64]) {
    fd2_magic_fx_parameters parameters;
    CHECK(fd2_magic_fx_parameters_get(v->id,v->side,&parameters)==0);
    fd2_magic_fx_parts parts={v->id==2 ? durations_wheel : durations_zero,
        parameters.minimum_part_count};
    CHECK(fd2_magic_fx_init(fx,v->id,v->side,parts)==0);
    o->hash=UINT64_C(14695981039346656037);o->rng_state=v->seed;
    int count=0,value=0;
    CHECK(step(fx,o,0,visible,&count)==0);
    CHECK(count>=0 && count<=60);
    for(int i=0;i<count;i++) {
        CHECK(step(fx,o,1,visible,&value)==0); CHECK(step(fx,o,2,visible,&value)==0);
    }
    for(unsigned target=0;target<v->targets;target++) {
        CHECK(step(fx,o,3,visible,&count)==0);
        CHECK(count>=0 && count<=60);
        for(int i=0;i<count;i++) {
            CHECK(step(fx,o,4,visible,&value)==0); CHECK(step(fx,o,5,visible,&value)==0);
            if(value==1) impacts[target]++;
        }
        if(target+1<v->targets) for(int i=0;i<9;i++) {
            CHECK(step(fx,o,4,visible,&value)==0); CHECK(step(fx,o,5,visible,&value)==0);
        }
    }
    CHECK(step(fx,o,6,visible,&count)==0);
    CHECK(count>=0 && count<=60);
    for(int i=0;i<count;i++) {
        CHECK(step(fx,o,7,visible,&value)==0); CHECK(step(fx,o,8,visible,&value)==0);
    }
    return 0;
}
static int native_vectors_and_replay(void) {
    for(size_t n=0;n<sizeof(vectors)/sizeof(vectors[0]);n++) {
        const golden *v=&vectors[n];
        fd2_magic_fx normal,skipped,replayed;
        observed real={0},skip={0},replay={0};
        unsigned impacts[64]={0},skip_impacts[64]={0},replay_impacts[64]={0};
        CHECK(cast(&normal,&real,v,1,impacts)==0);
        if(real.hash!=v->hash) fprintf(stderr,"id=%u side=%u seed=%u targets=%u got=%016" PRIx64 " expected=%016" PRIx64 "\n",v->id,v->side,v->seed,v->targets,real.hash,v->hash);
        CHECK(real.hash==v->hash && real.rng_count==v->rng_count && real.rng_state==v->rng_final);
        CHECK(memcmp(impacts,v->impacts,sizeof(impacts))==0);
        CHECK(cast(&skipped,&skip,v,0,skip_impacts)==0);
        CHECK(skip.emissions==0 && memcmp(&normal,&skipped,sizeof(normal))==0);
        CHECK(skip.rng_count==real.rng_count && skip.rng_state==real.rng_state);
        CHECK(memcmp(skip.recorded,real.recorded,real.rng_count*sizeof(uint32_t))==0);
        CHECK(memcmp(impacts,skip_impacts,sizeof(impacts))==0);
        replay.replay=real.recorded;replay.replay_count=real.rng_count;
        CHECK(cast(&replayed,&replay,v,1,replay_impacts)==0);
        CHECK(replay.rng_state==v->seed); /* 只读录制reader没有推进真实流。 */
        CHECK(replay.rng_count==real.rng_count && replay.hash==real.hash);
        CHECK(memcmp(&normal,&replayed,sizeof(normal))==0);
        CHECK(memcmp(impacts,replay_impacts,sizeof(impacts))==0);
    }
    return 0;
}
static int parameters_and_invalid_inputs(void) {
    static const uint8_t hp[10]={7,8,6,13,6,6,5,5,16,20};
    static const uint8_t samples[10]={2,2,4,3,2,2,4,2,3,3};
    static const uint16_t hostile[10]={20,21,27,43,23,25,33,38,30,44};
    for(uint8_t id=0;id<10;id++) {
        fd2_magic_fx_parameters p;
        CHECK(fd2_magic_fx_parameters_get(id,0,&p)==0);
        CHECK(p.effect_entry==hostile[id] && p.hp_denominator==hp[id] && p.minimum_sample_count==samples[id]);
    }
    fd2_magic_fx_parameters p,before;
    memset(&p,0xa5,sizeof(p));before=p;
    CHECK(fd2_magic_fx_parameters_get(10,0,&p)==-1 && memcmp(&p,&before,sizeof(p))==0);
    CHECK(fd2_magic_fx_parameters_get(9,1,&p)==-1 && memcmp(&p,&before,sizeof(p))==0);
    CHECK(fd2_magic_fx_parameters_get(9,2,&p)==-1 && memcmp(&p,&before,sizeof(p))==0);
    fd2_magic_fx fx,old;
    memset(&fx,0xa5,sizeof(fx));old=fx;
    CHECK(fd2_magic_fx_init(&fx,2,0,(fd2_magic_fx_parts){durations_zero,17})==-1 && memcmp(&fx,&old,sizeof(fx))==0);
    CHECK(fd2_magic_fx_init(&fx,0,0,(fd2_magic_fx_parts){NULL,16})==-1 && memcmp(&fx,&old,sizeof(fx))==0);
    CHECK(fd2_magic_fx_init(&fx,4,0,(fd2_magic_fx_parts){durations_zero,14})==0);
    old=fx;observed o={0};int value=12345;
    CHECK(fd2_magic_fx_step(&fx,0,NULL,NULL,emitted,&o,&value)==-1);
    CHECK(value==12345 && !o.emissions && !o.rng_count && memcmp(&fx,&old,sizeof(fx))==0);
    CHECK(fd2_magic_fx_step(&fx,9,random_next,&o,emitted,&o,&value)==-1);
    CHECK(fd2_magic_fx_step(&fx,0,random_next,&o,emitted,&o,NULL)==-1);
    CHECK(!o.emissions && !o.rng_count && memcmp(&fx,&old,sizeof(fx))==0);
    return 0;
}
static int duration_zero_and_part_bounds(void) {
    uint8_t durations[18]={0};
    fd2_magic_fx fx;
    CHECK(fd2_magic_fx_init(&fx,2,2,(fd2_magic_fx_parts){durations,18})==0);
    int value=0;
    CHECK(fd2_magic_fx_step(&fx,0,NULL,NULL,NULL,NULL,&value)==0 && value==29);
    CHECK(fd2_magic_fx_step(&fx,1,NULL,NULL,NULL,NULL,&value)==0);
    CHECK(fx.state.wheel.frame==0 && fx.state.wheel.timer==1);
    fx.state.wheel.timer=255;
    CHECK(fd2_magic_fx_step(&fx,1,NULL,NULL,NULL,NULL,&value)==0);
    CHECK(fx.state.wheel.frame==1 && fx.state.wheel.timer==0);
    fx.state.wheel.frame=18;value=12345;fd2_magic_fx before=fx;
    CHECK(fd2_magic_fx_step(&fx,1,NULL,NULL,NULL,NULL,&value)==-1 && value==12345);
    CHECK(memcmp(&fx,&before,sizeof(fx))==0);
    return 0;
}
static int fail_emit(const fd2_magic_fx_emission *e,void *userdata) {
    (void)e;(*(unsigned *)userdata)++;return -1;
}
static uint32_t bad_random(void *userdata) { (*(unsigned *)userdata)++;return UINT32_MAX; }
static int late_callback_errors_are_not_success_or_second_commit(void) {
    fd2_magic_fx fx;int value=12345;unsigned calls=0;
    CHECK(fd2_magic_fx_init(&fx,6,0,(fd2_magic_fx_parts){durations_zero,10})==0);
    CHECK(fd2_magic_fx_step(&fx,0,NULL,NULL,fail_emit,&calls,&value)==-1);
    CHECK(value==12345 && calls==1);
    CHECK(fd2_magic_fx_init(&fx,4,0,(fd2_magic_fx_parts){durations_zero,14})==0);
    calls=0;
    CHECK(fd2_magic_fx_step(&fx,0,bad_random,&calls,NULL,NULL,&value)==-1);
    CHECK(value==12345 && calls==1);
    return 0;
}
int main(void) {
    CHECK(parameters_and_invalid_inputs()==0);
    CHECK(duration_zero_and_part_bounds()==0);
    CHECK(late_callback_errors_are_not_success_or_second_commit()==0);
    CHECK(native_vectors_and_replay()==0);
    puts("magic_fx: 29 original-kernel vectors; emitted/skip/read-only replay agree");
    return 0;
}
