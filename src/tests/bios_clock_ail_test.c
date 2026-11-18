/* 普通游戏的共享调度：既有独立商余数与MDI→DIG节奏向量。
 * 只替换宿主时间；核音乐／音效／BDA与输出前缀，不维护硬件内部等价门。 */
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>
static uint64_t host_now;
static SDL_ThreadID host_thread=1;
static int rtc_ok;
static uint64_t input_now(void) {return host_now;}
static SDL_ThreadID input_thread(void) {return host_thread;}
static bool input_current(SDL_Time *stamp) {*stamp=0;return rtc_ok!=0;}
static bool input_local(SDL_Time stamp,SDL_DateTime *local,bool yes) {
    (void)stamp;(void)yes;memset(local,0,sizeof(*local));return rtc_ok!=0;
}
#define SDL_GetTicksNS input_now
#define SDL_GetCurrentThreadID input_thread
#define SDL_GetCurrentTime input_current
#define SDL_TimeToDateTime input_local
#include "../bios_clock.c"
#define CHECK(x) do {if(!(x)){fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#x);return 0;}}while(0)

/* Python Fraction(n*5966*10^9/1193182)独立生成；IRQ ordinal不是原phase观测。 */
static const uint64_t edges[]={0,5000076,10000151,15000227,20000302,
    25000378,30000453,35000528,40000604,45000679,50000755,55000830,60000906};
static const unsigned mdi_counts[]={0,0,1,1,2,3,3,4,4,5,6,6,7};
typedef struct trace {
    unsigned dig,mdi,seen;
    uint32_t last_bda;
    unsigned reenter, reenter_rejected;
    fd2_ail_timer *timer;
} trace;
static void dig(void *user) {
    trace *t=(trace *)user;
    t->dig++;
    t->last_bda=fd2_bios_clock_ticks(UINT64_MAX);
    if(t->reenter) {
        t->seen++;
        if(fd2_bios_clock_advance(host_now)==-1 &&
           fd2_bios_clock_ail_owner_after_irqs(host_now)==NULL)t->reenter_rejected++;
    }
}
static void mdi(void *user) {((trace *)user)->mdi++;}
static int start(trace *t,uint32_t initial) {
    memset(t,0,sizeof(*t));host_now=0;host_thread=1;
    fd2_bios_clock_reset(initial,0);
    CHECK(fd2_bios_clock_ail_attach(0,65536u)==0);
    CHECK(fd2_bios_clock_install_mdi(0,mdi,t)==0u);
    CHECK(fd2_bios_clock_install_dig(0,dig,t)==FD2_AIL_TIMER_DIG);
    t->timer=fd2_bios_clock_ail_owner_after_irqs(0);
    CHECK(t->timer && t->timer->base_us==5000u && t->timer->interval_count==5966u);
    CHECK(t->timer->slots[1].period_us==5000u && t->timer->slots[0].period_us==8333u);
    return 1;
}
static int eleven_irqs_and_read_deadline(void) {
    trace t;CHECK(start(&t,7));
    for(unsigned i=1;i<=12;i++) {
        host_now=edges[i]-1u;
        CHECK(fd2_bios_clock_advance(host_now)==0);
        CHECK(t.dig==i-1u && fd2_bios_clock_deadline_ns(host_now,1)==edges[i]);
        fd2_ail_timer before=*t.timer;
        uint32_t bda=fd2_bios_clock_ticks(host_now);
        CHECK(fd2_bios_clock_ticks(UINT64_MAX)==bda);
        CHECK(fd2_bios_clock_deadline_ns(UINT64_MAX,999)==UINT64_MAX);
        CHECK(memcmp(&before,t.timer,sizeof(before))==0);
        host_now=edges[i];CHECK(fd2_bios_clock_advance(host_now)==0);
        CHECK(t.dig==i && t.mdi==mdi_counts[i]);
        CHECK(fd2_bios_clock_ticks(host_now)==7u+(i>=11u));
        if(i==11u)CHECK(t.last_bda==7u); /* BIOS只在ordinary callback之后 */
    }
    return 1;
}
static int reentrant_and_main_thread_guards(void) {
    trace t;CHECK(start(&t,0));t.reenter=1;
    host_now=edges[11];CHECK(fd2_bios_clock_advance(host_now)==0);
    CHECK(t.dig==11 && t.reenter_rejected==11 && t.seen==11);
    CHECK(t.last_bda==0 && fd2_bios_clock_ticks(host_now)==1);
    fd2_ail_timer before=*t.timer;
    host_thread=2;CHECK(fd2_bios_clock_advance(host_now)==-1);
    CHECK(fd2_bios_clock_ail_owner_after_irqs(host_now)==NULL);
    host_thread=1;CHECK(fd2_bios_clock_advance(host_now-1)==-1);
    CHECK(memcmp(&before,t.timer,sizeof(before))==0);
    CHECK(fd2_bios_clock_ticks(UINT64_MAX)==1);
    /* public init建立live域；与reset的external域分账，不依赖参数名字。 */
    clock_initialized=0;host_now=0;rtc_ok=1;memset(&t,0,sizeof(t));
    CHECK(fd2_bios_clock_init()==0 && !clock_external_input);rtc_ok=0;
    CHECK(fd2_bios_clock_ail_attach(0,65536)==0);
    CHECK(fd2_bios_clock_install_mdi(0,mdi,&t)==0);
    CHECK(fd2_bios_clock_install_dig(0,dig,&t)==FD2_AIL_TIMER_DIG);
    host_now=edges[1]-1;
    CHECK(fd2_bios_clock_advance(edges[1])==-1 && !t.dig && !t.mdi);
    CHECK(fd2_bios_clock_ticks(UINT64_MAX)==0);
    host_now=edges[1];CHECK(fd2_bios_clock_advance(host_now)==0 && t.dig==1);
    return 1;
}
static int external_input_can_advance_without_wall(void) {
    trace t;CHECK(start(&t,0));CHECK(clock_external_input && host_now==0);
    CHECK(fd2_bios_clock_advance(edges[11])==0); /* 同clock主控虚拟now，不等墙钟 */
    CHECK(t.dig==11 && t.mdi==6 && fd2_bios_clock_ticks(0)==1);
    fd2_ail_timer before=*t.timer;
    CHECK(fd2_bios_clock_ticks(UINT64_MAX)==1);
    CHECK(fd2_bios_clock_deadline_ns(UINT64_MAX,1)==UINT64_MAX);
    CHECK(memcmp(&before,t.timer,sizeof(before))==0 && host_now==0 && clock_epoch_ns==0);
    return 1;
}
static int install_at_control_boundary(void) {
    trace t={0};host_thread=1;host_now=0;fd2_bios_clock_reset(0,0);
    CHECK(fd2_bios_clock_ail_attach(0,65536)==0);
    host_now=1000000;
    CHECK(fd2_bios_clock_install_mdi(host_now,mdi,&t)==FD2_AIL_TIMER_MDI);
    CHECK(fd2_bios_clock_deadline_ns(host_now,1)==9333180);
    host_now=2000000;
    CHECK(fd2_bios_clock_install_dig(host_now,dig,&t)==FD2_AIL_TIMER_DIG);
    CHECK(fd2_bios_clock_deadline_ns(host_now,1)==7000076);
    t.timer=fd2_bios_clock_ail_owner_after_irqs(host_now);
    CHECK(t.timer && !t.timer->slots[0].accumulator_us && !t.timer->slots[1].accumulator_us && !t.timer->bios_accumulator_us);
    fd2_ail_timer before=*t.timer;
    CHECK(fd2_bios_clock_install_dig(host_now,dig,&t)==FD2_AIL_TIMER_INVALID);
    CHECK(!memcmp(&before,t.timer,sizeof before));
    CHECK(fd2_bios_clock_ticks(host_now)==0 && clock_epoch_ns==0);
    return 1;
}

static int same_time_release_after_completed_callback(void) {
    trace t;CHECK(start(&t,0));host_now=edges[1];
    t.timer=fd2_bios_clock_ail_owner_after_irqs(host_now);
    CHECK(t.timer && t.dig==1); /* API名所声明政策，不称原实测tie */
    CHECK(fd2_ail_timer_release(t.timer,FD2_AIL_TIMER_DIG)==0);
    host_now=edges[2];CHECK(fd2_bios_clock_advance(host_now)==0);
    CHECK(t.dig==1 && t.mdi==1 && t.timer->base_us==5000);
    return 1;
}
static int day_and_signed_word_wrap(void) {
    trace t;CHECK(start(&t,0x1800afu));host_now=edges[11];
    CHECK(fd2_bios_clock_advance(host_now)==0 && fd2_bios_clock_ticks(host_now)==0);
    CHECK(start(&t,0x7fffu));host_now=edges[11];
    CHECK(fd2_bios_clock_advance(host_now)==0 && fd2_bios_tick_signed_ns(host_now)==-32768);
    CHECK(start(&t,0xffffu));host_now=edges[11];
    CHECK(fd2_bios_clock_advance(host_now)==0 && fd2_bios_tick_low_ns(host_now)==0);
    return 1;
}
static int default_mode_epoch_and_attach_guards(void) {
    host_now=1000;host_thread=1;fd2_bios_clock_reset(17,1000);
    CHECK(fd2_bios_clock_ail_state()==NULL);
    CHECK(fd2_bios_clock_ticks(1000+54925402)==18);
    CHECK(fd2_bios_clock_deadline_ns(1000,2)==1000+109850803);
    CHECK(fd2_bios_clock_ticks(UINT64_MAX)>0 && !clock_ail_active && clock_last_ns==1000);
    /* reset外部域可给未来输入；live拒绝已在独立组测试。 */
    CHECK(fd2_bios_clock_ail_attach(1000,65538)==-1 && !clock_ail_active);
    host_now=1000+54925402;
    CHECK(fd2_bios_clock_ail_attach(host_now,123)==0);
    CHECK(clock_origin==17 && clock_epoch_ns==1000 && fd2_bios_clock_ticks(UINT64_MAX)==18);
    CHECK(fd2_bios_clock_ail_attach(host_now,123)==-1);
    CHECK(fd2_bios_clock_deadline_ns(host_now,0)==host_now);
    /* explicit pending edge0只本夹具资格：BDA首次在advance，read不触发。 */
    host_now=0;fd2_bios_clock_reset(5,0);
    CHECK(fd2_bios_clock_ail_attach(0,0)==0 && fd2_bios_clock_ticks(0)==5);
    CHECK(fd2_bios_clock_deadline_ns(0,1)==0);
    CHECK(fd2_bios_clock_advance(0)==0 && fd2_bios_clock_ticks(0)==6);
    return 1;
}
typedef struct prefix_trace {
    unsigned count,bad;
    uint64_t edges[16];
    uint32_t fractions[16];
    uint32_t bda[16];
} prefix_trace;
static void audio_prefix(void *user,uint64_t edge,uint32_t fraction) {
    prefix_trace *p=(prefix_trace *)user;
    if(p->count>=16u){p->bad=1;return;}
    p->edges[p->count]=edge+(fraction!=0u);p->fractions[p->count]=fraction;
    p->bda[p->count]=fd2_bios_clock_ticks(UINT64_MAX);p->count++;
    if(fd2_bios_clock_advance(edge)!=-1 || fd2_bios_clock_set_audio_prefix(NULL,NULL)!=-1)p->bad=1;
}
static int audio_prefix_is_before_irq_and_not_clock_authority(void) {
    trace t;prefix_trace p={0};CHECK(start(&t,0));
    CHECK(fd2_bios_clock_set_audio_prefix(audio_prefix,&p)==0);
    host_now=edges[11];CHECK(fd2_bios_clock_advance(host_now)==0);
    CHECK(!p.bad && p.count==12u && t.dig==11u && t.mdi==6u);
    CHECK(p.fractions[0]==511350u && p.fractions[11]==0u);
    for(unsigned i=0;i<11u;i++)CHECK(p.edges[i]==edges[i+1u] && p.bda[i]==0u);
    CHECK(p.edges[11]==edges[11] && p.bda[11]==1u);
    fd2_ail_timer before=*t.timer;
    CHECK(fd2_bios_clock_ticks(UINT64_MAX)==1u && p.count==12u);
    CHECK(memcmp(&before,t.timer,sizeof(before))==0);
    CHECK(fd2_bios_clock_set_audio_prefix(NULL,&p)==0);
    host_now=edges[12];CHECK(fd2_bios_clock_advance(host_now)==0 && p.count==12u);
    CHECK(fd2_bios_clock_set_audio_prefix(audio_prefix,&p)==0);
    fd2_bios_clock_reset(0,0);host_now=1;
    CHECK(fd2_bios_clock_advance(host_now)==0 && p.count==12u); /* 新owner不借旧user。 */
    CHECK(start(&t,0));memset(&p,0,sizeof(p));
    CHECK(fd2_bios_clock_set_audio_prefix(audio_prefix,&p)==0);
    CHECK(fd2_bios_clock_set_audio_prefix(NULL,NULL)==0);
    return 1;
}
typedef struct exact_prefix_point {uint64_t ns;uint32_t fraction;unsigned count;} exact_prefix_point;
static void remember_exact_prefix(void *user,uint64_t ns,uint32_t fraction) {
    exact_prefix_point *p=(exact_prefix_point *)user;
    if(fraction){p->ns=ns;p->fraction=fraction;p->count++;}
}
static int audio_prefix_keeps_sub_ns_sample_boundary(void) {
    /* 独立Fraction(n*5966*10^9/1193182)：ceil(ns)分别会越过44.1k/48k一个sample。
     * 这里只核接口完整保留有理点；真正sample converter另按同字面向量核。 */
    static const struct {unsigned irq;uint64_t ns;uint32_t fraction;} cases[]={
        {4810u,UINT64_C(24050362811),445398u},
        {114899u,UINT64_C(574503666666),128788u}};
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        trace t;exact_prefix_point p={0};CHECK(start(&t,0));
        CHECK(fd2_bios_clock_set_audio_prefix(remember_exact_prefix,&p)==0);
        host_now=cases[i].ns+1u;CHECK(fd2_bios_clock_advance(host_now)==0);
        CHECK(p.ns==cases[i].ns && p.fraction==cases[i].fraction && p.count==cases[i].irq);
        CHECK(t.dig==cases[i].irq);
        CHECK(fd2_bios_clock_set_audio_prefix(NULL,NULL)==0);
    }
    return 1;
}
static int error_preserves_arrived_prefix(void) {
    trace t={0};host_thread=1;
    const uint64_t epoch=UINT64_MAX-UINT64_C(6000000);
    host_now=epoch;fd2_bios_clock_reset(3,epoch);
    CHECK(fd2_bios_clock_ail_attach(epoch,1)==0);
    CHECK(fd2_bios_clock_install_dig(epoch,dig,&t)==FD2_AIL_TIMER_DIG);
    t.timer=fd2_bios_clock_ail_owner_after_irqs(epoch);CHECK(t.timer);
    /* 首个有效5ms点已服务；下一边界数值溢出时不回滚已到的callback。 */
    host_now=epoch+edges[1];CHECK(fd2_bios_clock_advance(host_now)==-1 && t.dig==1);
    CHECK(fd2_bios_clock_ticks(0)==3 && fd2_bios_clock_error()<0);
    fd2_ail_timer before=*t.timer;uint64_t actual=77;
    CHECK(fd2_bios_clock_observe_checked(host_now,&actual)<0 && actual==77);
    CHECK(!memcmp(&before,t.timer,sizeof before) && t.dig==1);
    host_now=UINT64_MAX;fd2_bios_clock_reset(3,UINT64_MAX);
    CHECK(fd2_bios_clock_ail_attach(host_now,1)==-1 && !clock_ail_active);
    return 1;
}

static int app_close_ends_borrow(void) {
    trace t;prefix_trace p={0};CHECK(start(&t,4));
    CHECK(fd2_bios_clock_set_audio_prefix(audio_prefix,&p)==0);
    host_now=edges[1];CHECK(fd2_bios_clock_advance(host_now)==0 && t.dig==1u);
    fd2_ail_timer before=*t.timer;unsigned count=p.count;
    host_thread=2;CHECK(fd2_bios_clock_close()==-1);host_thread=1;
    CHECK(fd2_bios_clock_close()==0 && fd2_bios_clock_close()==0);
    CHECK(memcmp(&before,t.timer,sizeof(before))==0);
    host_now=edges[12];CHECK(fd2_bios_clock_advance(host_now)==-1 && t.dig==1u && p.count==count);
    CHECK(fd2_bios_clock_ticks(UINT64_MAX)==4u && fd2_bios_clock_deadline_ns(host_now,1)==UINT64_MAX);
    CHECK(fd2_bios_clock_set_audio_prefix(audio_prefix,&p)==-1 && fd2_bios_clock_set_audio_prefix(NULL,NULL)==0);
    CHECK(fd2_bios_clock_ail_owner_after_irqs(host_now)==NULL && fd2_bios_clock_init()==-1);
    CHECK(fd2_bios_clock_install_mdi(host_now,mdi,&t)==FD2_AIL_TIMER_INVALID);
    CHECK(memcmp(&before,t.timer,sizeof(before))==0);
    host_now=0;fd2_bios_clock_reset(0,0);CHECK(!clock_closed && fd2_bios_clock_advance(0)==0);
    return 1;
}
static int observed_read_commits_only_the_supplied_now(void) {
    trace t;CHECK(start(&t,0));
    host_now=edges[11];CHECK(fd2_bios_clock_ticks(host_now)==0u && !t.dig);
    uint64_t actual=77;
    CHECK(fd2_bios_clock_observe_checked(host_now,&actual)==0 && actual==host_now);
    CHECK(fd2_bios_tick_low_ns(host_now)==1u && t.dig==11u);
    fd2_ail_timer before=*t.timer;
    CHECK(fd2_bios_clock_ticks(UINT64_MAX)==1u && !memcmp(&before,t.timer,sizeof before));
    CHECK(fd2_bios_clock_observe_checked(host_now,&actual)==0 && t.dig==11u);
    CHECK(fd2_bios_clock_close()==0);actual=77;
    CHECK(fd2_bios_clock_observe_checked(UINT64_MAX,&actual)<0 && actual==77 && t.dig==11u);
    return 1;
}

int main(void) {
    static int (*const tests[])(void)={eleven_irqs_and_read_deadline,
        reentrant_and_main_thread_guards,external_input_can_advance_without_wall,
        install_at_control_boundary,same_time_release_after_completed_callback,
        day_and_signed_word_wrap,default_mode_epoch_and_attach_guards,
        audio_prefix_is_before_irq_and_not_clock_authority,audio_prefix_keeps_sub_ns_sample_boundary,
        error_preserves_arrived_prefix,app_close_ends_borrow,observed_read_commits_only_the_supplied_now};
    for(size_t i=0;i<sizeof(tests)/sizeof(tests[0]);i++)if(!tests[i]())return 1;
    puts("bios_clock_ail: 12 shared cadence/output/owner boundary groups passed");
    return 0;
}
