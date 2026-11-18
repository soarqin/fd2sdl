/* 独立源literal/商余数：3ea8e/3ec17/3ebb4及main MDI->DIG。
 * 有界完成调用时序；mock SDL只给host输入，不是原版/PIC/CPU实测。 */
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
    unsigned rebase;
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
    if(t->rebase && t->dig==1u)
        (void)fd2_ail_timer_set_period(t->timer,4u,10000u);
}
static void mdi(void *user) {((trace *)user)->mdi++;}
static int start(trace *t,uint32_t initial) {
    memset(t,0,sizeof(*t));host_now=0;host_thread=1;
    fd2_bios_clock_reset(initial,0);
    CHECK(fd2_bios_clock_ail_attach(0,65536u)==0);
    CHECK(fd2_bios_clock_install_mdi(0,mdi,t)==0u);
    CHECK(fd2_bios_clock_install_dig(0,dig,t)==4u);
    t->timer=fd2_bios_clock_ail_owner_after_irqs(0);
    CHECK(t->timer && t->timer->base_us==5000u && t->timer->pit_divisor==5966u);
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
    CHECK(fd2_bios_clock_install_dig(0,dig,&t)==4);
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
static int defer_bios_and_release_pending(void) {
    trace t;CHECK(start(&t,0));fd2_ail_timer_defer_enter(t.timer);
    host_now=edges[11];CHECK(fd2_bios_clock_advance(host_now)==0);
    CHECK(t.dig==0 && t.mdi==0 && fd2_bios_clock_ticks(host_now)==1);
    CHECK(t.timer->slots[1].pending==11 && t.timer->slots[0].pending==6);
    fd2_ail_timer_slot retained=t.timer->slots[1];
    CHECK(fd2_ail_timer_release(t.timer,4)==0);
    CHECK(t.timer->slots[1].status==0 && t.timer->slots[1].pending==retained.pending);
    CHECK(t.timer->slots[1].callback==retained.callback && t.timer->slots[1].user==retained.user);
    CHECK(t.timer->base_us==5000 && t.timer->pit_divisor==5966);
    fd2_ail_timer_defer_leave(t.timer);
    host_now=edges[12];CHECK(fd2_bios_clock_advance(host_now)==0);
    CHECK(t.dig==11 && t.mdi==7 && !t.timer->slots[1].pending);
    CHECK(fd2_bios_clock_ticks(host_now)==1);
    return 1;
}
static int busy_irq_does_not_accumulate(void) {
    trace t;CHECK(start(&t,0));
    t.timer->busy=1;
    host_now=edges[1];CHECK(fd2_bios_clock_advance(host_now)==0);
    CHECK(t.dig==0 && t.mdi==0 && !t.timer->slots[15].accumulator_us && t.timer->busy==1);
    t.timer->busy=0;
    host_now=edges[11];CHECK(fd2_bios_clock_advance(host_now)==0);
    CHECK(t.dig==10 && t.mdi==6 && fd2_bios_clock_ticks(host_now)==0);
    host_now=edges[12];CHECK(fd2_bios_clock_advance(host_now)==0);
    CHECK(t.dig==11 && t.mdi==6 && fd2_bios_clock_ticks(host_now)==1);
    return 1;
}
static int reprogram_at_control_boundary(void) {
    trace t={0};host_thread=1;host_now=0;fd2_bios_clock_reset(0,0);
    CHECK(fd2_bios_clock_ail_attach(0,65536)==0);
    host_now=1000000;
    CHECK(fd2_bios_clock_install_mdi(host_now,mdi,&t)==0);
    CHECK(fd2_bios_clock_deadline_ns(host_now,1)==9333180);
    host_now=2000000;
    CHECK(fd2_bios_clock_install_dig(host_now,dig,&t)==4);
    CHECK(fd2_bios_clock_deadline_ns(host_now,1)==7000076);
    t.timer=fd2_bios_clock_ail_owner_after_irqs(host_now);
    CHECK(fd2_ail_timer_set_period(t.timer,4,20000)==0);
    CHECK(t.timer->base_us==8333 && t.timer->pit_divisor==9943);
    CHECK(fd2_bios_clock_deadline_ns(host_now,1)==10333180);
    CHECK(fd2_bios_clock_ticks(host_now)==0 && clock_epoch_ns==0);
    return 1;
}
static int callback_reprogram_keeps_fraction(void) {
    trace t;CHECK(start(&t,0));t.rebase=1;
    host_now=edges[1];CHECK(fd2_bios_clock_advance(host_now)==0);
    CHECK(t.timer->base_us==8333 && t.timer->pit_divisor==9943);
    /* ceil((5966+9943)*1e9/rate)，不能round(firstIRQ)后再加而漂1ns。 */
    CHECK(fd2_bios_clock_deadline_ns(host_now,1)==13333256);
    CHECK(!t.timer->slots[0].accumulator_us && !t.timer->slots[1].accumulator_us);
    host_now=13333255;CHECK(fd2_bios_clock_advance(host_now)==0 && t.dig==1 && t.mdi==0);
    host_now=13333256;CHECK(fd2_bios_clock_advance(host_now)==0 && t.dig==1 && t.mdi==1);
    return 1;
}
static int explicit_same_time_irq_first(void) {
    trace t;CHECK(start(&t,0));host_now=edges[1];
    t.timer=fd2_bios_clock_ail_owner_after_irqs(host_now);
    CHECK(t.timer && t.dig==1); /* API名所声明政策，不称原实测tie */
    CHECK(fd2_ail_timer_stop(t.timer,4)==0);
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
static int lowword_zero_and_one(void) {
    trace t={0};fd2_ail_timer *timer;uint32_t h;
    host_now=0;host_thread=1;fd2_bios_clock_reset(0,0);
    CHECK(fd2_bios_clock_ail_attach(0,65536)==0);
    CHECK(fd2_bios_clock_deadline_ns(0,1)==54925402);
    host_now=54925402;CHECK(fd2_bios_clock_advance(host_now)==0 && fd2_bios_clock_ticks(host_now)==1);
    for(unsigned i=0;i<2;i++) {
        host_now=0;fd2_bios_clock_reset(0,0);memset(&t,0,sizeof(t));
        CHECK(fd2_bios_clock_ail_attach(0,65536)==0);
        timer=fd2_bios_clock_ail_owner_after_irqs(0);
        h=fd2_ail_timer_register(timer,dig);CHECK(h==0);
        CHECK(fd2_ail_timer_set_user(timer,h,&t,NULL)==0);
        CHECK(fd2_ail_timer_set_period(timer,h,i?1u:54920u)==0);
        CHECK(fd2_ail_timer_start(timer,h)==0);
        CHECK(timer->pit_divisor==(i?1u:65536u)); /* raw高位仍保留 */
        uint64_t edge=i?54926240:54925402;
        CHECK(fd2_bios_clock_deadline_ns(0,1)==edge);
        host_now=edge-1;CHECK(fd2_bios_clock_advance(host_now)==0 && !t.dig);
        host_now=edge;CHECK(fd2_bios_clock_advance(host_now)==0 && t.dig==1);
        CHECK(fd2_bios_clock_ticks(host_now)==0);
    }
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
    t.timer->busy=1u;host_now=edges[1];CHECK(fd2_bios_clock_advance(host_now)==0);
    CHECK(p.count==2u && p.edges[0]==edges[1] && p.edges[1]==edges[1] && !t.dig && !p.bad);
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
static int h_error_preserves_arrived_prefix(void) {
    trace t;CHECK(start(&t,0));
    /* 坏callback是H安全输入，不执行原坏CALL；已扣pending/acc留原前缀。 */
    t.timer->slots[1].callback=NULL;
    host_now=edges[1];CHECK(fd2_bios_clock_advance(host_now)==-1);
    CHECK(!t.timer->busy && !t.timer->slots[1].pending && t.timer->slots[15].accumulator_us==5000);
    CHECK(fd2_bios_clock_ticks(UINT64_MAX)==0 && fd2_bios_clock_deadline_ns(host_now,1)==UINT64_MAX);
    fd2_ail_timer before=*t.timer;
    host_now=edges[12];CHECK(fd2_bios_clock_advance(host_now)==-1);
    CHECK(memcmp(&before,t.timer,sizeof(before))==0);
    uint64_t actual=77;
    CHECK(fd2_bios_clock_error()<0 && fd2_bios_clock_observe_checked(host_now,&actual)<0);
    CHECK(actual==77 && memcmp(&before,t.timer,sizeof(before))==0);
    host_now=UINT64_MAX;fd2_bios_clock_reset(3,UINT64_MAX);
    CHECK(fd2_bios_clock_ail_attach(host_now,1)==-1 && !clock_ail_active);
    return 1;
}
static int app_close_preserves_pending_and_ends_borrow(void) {
    trace t;prefix_trace p={0};CHECK(start(&t,4));
    CHECK(fd2_bios_clock_set_audio_prefix(audio_prefix,&p)==0);
    host_now=edges[1];CHECK(fd2_bios_clock_advance(host_now)==0 && t.dig==1u);
    t.timer->slots[0].pending=2u;
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
    CHECK(fd2_bios_clock_observe_ns(host_now)==host_now);
    CHECK(fd2_bios_tick_low_ns(host_now)==1u && t.dig==11u);
    uint64_t actual=77;
    CHECK(!fd2_bios_clock_error() && fd2_bios_clock_observe_checked(host_now,&actual)==0);
    CHECK(actual==host_now && t.dig==11u);
    fd2_ail_timer before=*t.timer;
    CHECK(fd2_bios_clock_ticks(UINT64_MAX)==1u && memcmp(&before,t.timer,sizeof(before))==0);
    CHECK(fd2_bios_clock_observe_ns(host_now)==host_now && t.dig==11u);
    CHECK(fd2_bios_clock_close()==0);
    CHECK(fd2_bios_clock_observe_ns(UINT64_MAX)==UINT64_MAX && t.dig==11u);
    return 1;
}
int main(void) {
    static int (*const tests[])(void)={eleven_irqs_and_read_deadline,
        reentrant_and_main_thread_guards,external_input_can_advance_without_wall,
        defer_bios_and_release_pending,
        busy_irq_does_not_accumulate,reprogram_at_control_boundary,
        callback_reprogram_keeps_fraction,explicit_same_time_irq_first,
        day_and_signed_word_wrap,lowword_zero_and_one,
        default_mode_epoch_and_attach_guards,h_error_preserves_arrived_prefix,
        audio_prefix_is_before_irq_and_not_clock_authority,audio_prefix_keeps_sub_ns_sample_boundary,
        app_close_preserves_pending_and_ends_borrow,observed_read_commits_only_the_supplied_now};
    for(size_t i=0;i<sizeof(tests)/sizeof(tests[0]);i++)if(!tests[i]())return 1;
    puts("bios_clock_ail: 16 scheduling/owner boundary groups passed; no emulator/runtime equivalence claim");
    return 0;
}
