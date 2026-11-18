/* 43160/443d0/43270完整source独立字面与有限模型；不采原CPU/设备/PIC。
 * 原15曲只加同S内部状态hash，旧消息／状态golden不改。 */
struct BeatTrackVector {
    size_t track;uint32_t loops,ticks;uint64_t hash;
    uint32_t raw_tempo,numerator,exponent;
};
#include "fixtures/sequence_beat_vectors.h"
static const BeatTrackVector *beat_vector(size_t track,uint32_t loops) {
    for(const auto &v:beat_track_vectors)if(v.track==track && v.loops==loops)return &v;
    return nullptr;
}
static void beat_state_hash(uint64_t &h,const BgmVoice &v) {
    const auto &s=v.sequence;
    const uint32_t values[]={v.timer_ticks,v.tempo_percent,v.target_tempo,
        v.tempo_fade_accumulator_us,v.tempo_fade_interval_us,v.tempo_accumulator,
        v.beat,v.measure,v.beats_per_measure,v.beat_increment,v.beat_phase,v.beat_denominator,
        static_cast<uint32_t>(s.cursor),s.delay,s.loop_remaining,s.status};
    for(uint32_t n:values)fold(h,n);
}
static bool source_reset_and_success_init_time_writers() {
    AilDriver driver;BgmVoice s;s.driver=&driver;s.tempo_percent=175;s.target_tempo=90;s.tempo_fade_accumulator_us=333;
    s.tempo_fade_interval_us=777;s.tempo_accumulator=91;
    s.beat=9;s.measure=8;s.beats_per_measure=7;s.beat_increment=19;s.beat_phase=23;s.beat_denominator=31;
    s.timer_ticks=37;reset_sequence_event_state(&s);
    CHECK(s.beat==0 && s.measure==UINT32_MAX && s.beat_phase==0 && s.beat_increment==0);
    CHECK(s.beats_per_measure==4 && s.beat_denominator==8000000 && !s.timer_ticks);
    CHECK(s.tempo_percent==175 && s.target_tempo==90 && s.tempo_accumulator==91);
    CHECK(s.tempo_fade_accumulator_us==333 && s.tempo_fade_interval_us==777);
    prepare_sequence(&s);
    CHECK(s.tempo_percent==100 && s.target_tempo==100 && !s.tempo_accumulator);
    CHECK(!s.tempo_fade_accumulator_us && !s.tempo_fade_interval_us);
    CHECK(s.measure==UINT32_MAX && !s.beat_increment && s.beat_denominator==8000000);
    return true;
}
static bool source_meta_parameters_and_signed_shift() {
    const uint8_t stream[]={0xff,0x51,3,0x07,0xa1,0x20,0xff,0x58,4,3,2,24,8,127,0xff,0x2f,0};
    AilDriver driver;CHECK(setup(driver));BgmVoice s;s.driver=&driver;prepare_sequence(&s);
    s.sequence.evnt=stream;s.sequence.size=sizeof(stream);CHECK(begin_sequence(&s));
    CHECK(native_sequence_tick(&s));
    CHECK(s.tempo_percent==100 && s.target_tempo==100 && !s.tempo_accumulator);
    CHECK(s.beat_denominator==8000000 && s.beats_per_measure==3 && s.beat_increment==133333);
    CHECK(s.beat==0 && s.measure==0 && s.beat_phase==0 && s.sequence.cursor==14 && s.sequence.delay==127);
    struct Shift {uint8_t exponent;uint32_t increment;};
    const Shift shifts[]={{0,33333},{1,66666},{2,133333},{3,266666},
        {30,0x50000000},{31,0xa0000000},{32,0x40000000},{33,0x80000000},{34,133333},{255,0xa0000000}};
    for(const auto &v:shifts)CHECK(sequence_beat_increment(v.exponent)==v.increment);
    s.measure=9;s.beat=2;s.beat_phase=55;
    sequence_meta_time_signature(&s,4,1);
    CHECK(s.measure==10 && s.beat==0 && s.beat_phase==uint32_t(0u-66666u));
    CHECK(s.beats_per_measure==4 && s.beat_increment==66666);
    sequence_beat_step(&s);CHECK(s.beat_phase==0 && !s.beat && s.measure==10);
    s.measure=UINT32_MAX;sequence_meta_time_signature(&s,3,31);
    CHECK(!s.measure && s.beat_increment==0xa0000000 && s.beat_phase==0x60000000);
    sequence_beat_step(&s);CHECK(!s.beat_phase && !s.beat && !s.measure);
    end_sequence(&s);return true;
}
static bool source_beat_single_if_and_signed_words() {
    AilDriver driver;BgmVoice s;s.driver=&driver;s.beat_phase=30;s.beat_increment=50;s.beat_denominator=7;
    s.beat=2;s.beats_per_measure=4;s.measure=8;
    sequence_beat_step(&s);CHECK(s.beat_phase==73 && s.beat==3 && s.measure==8);
    sequence_beat_step(&s);CHECK(s.beat_phase==116 && !s.beat && s.measure==9);
    s.beat_phase=0x80000000;s.beat_increment=0;s.beat_denominator=1;
    sequence_beat_step(&s);CHECK(s.beat_phase==0x80000000 && !s.beat && s.measure==9);
    s.beat_phase=0x7fffffff;s.beat_increment=1;
    sequence_beat_step(&s);CHECK(s.beat_phase==0x80000000 && !s.beat);
    return true;
}
static bool source_tempo_accumulator_and_outer_tick() {
    const uint8_t stream[]={127,0xff,0x2f,0};
    AilDriver driver;CHECK(setup(driver));BgmVoice s;s.driver=&driver;prepare_sequence(&s);
    s.sequence.evnt=stream;s.sequence.size=sizeof(stream);CHECK(begin_sequence(&s));
    s.tempo_percent=s.target_tempo=50;
    CHECK(native_sequence_tick(&s) && s.timer_ticks==1 && s.tempo_accumulator==50 && !s.sequence.cursor && !s.sequence.delay);
    CHECK(native_sequence_tick(&s) && s.timer_ticks==2 && !s.tempo_accumulator && s.sequence.cursor==1 && s.sequence.delay==127);
    CHECK(native_sequence_tick(&s) && s.tempo_accumulator==50 && s.sequence.delay==127);
    CHECK(native_sequence_tick(&s) && s.timer_ticks==4 && !s.tempo_accumulator && s.sequence.delay==126);
    s.tempo_percent=s.target_tempo=250;s.tempo_accumulator=0;s.sequence.delay=10;
    s.beat_phase=0;s.beat_increment=3;s.beat_denominator=100;
    CHECK(native_sequence_tick(&s) && s.timer_ticks==5 && s.tempo_accumulator==50 && s.sequence.delay==8 && s.beat_phase==6);
    CHECK(native_sequence_tick(&s) && s.timer_ticks==6 && !s.tempo_accumulator && s.sequence.delay==5 && s.beat_phase==15);
    s.tempo_percent=s.target_tempo=0;s.sequence_volume=10;s.target_volume=11;
    s.fade_accumulator_us=0;s.fade_interval_us=8333;
    CHECK(native_sequence_tick(&s) && s.timer_ticks==7 && s.sequence.delay==5 && s.beat_phase==15 && s.sequence_volume==11);
    s.tempo_percent=s.target_tempo=100;s.tempo_accumulator=0x7fffffff;
    CHECK(native_sequence_tick(&s) && s.timer_ticks==8 && s.tempo_accumulator==0x80000063 && s.sequence.delay==5);
    end_sequence(&s);return true;
}
static bool source_tempo_fade_after_steps_not_meta51() {
    const uint8_t stream[]={127,0xff,0x2f,0};
    AilDriver driver;CHECK(setup(driver));BgmVoice s;s.driver=&driver;prepare_sequence(&s);
    s.sequence.evnt=stream;s.sequence.size=sizeof(stream);CHECK(begin_sequence(&s));
    s.tempo_percent=50;s.target_tempo=100;s.tempo_fade_interval_us=0;
    CHECK(native_sequence_tick(&s) && s.tempo_percent==100 && s.tempo_accumulator==50 && !s.sequence.cursor);
    CHECK(s.tempo_fade_accumulator_us==8333);
    CHECK(native_sequence_tick(&s) && s.sequence.cursor==1 && s.sequence.delay==127 && s.tempo_accumulator==50);
    CHECK(s.tempo_fade_accumulator_us==8333); /* equality skips whole fade branch */
    s.tempo_percent=103;s.target_tempo=100;s.tempo_fade_accumulator_us=0;s.tempo_fade_interval_us=1000;
    advance_sequence_tempo_fade(&s);CHECK(s.tempo_percent==100 && s.tempo_fade_accumulator_us==5333);
    s.tempo_percent=99;s.target_tempo=100;s.tempo_fade_accumulator_us=0xfffffff0;s.tempo_fade_interval_us=10000;
    advance_sequence_tempo_fade(&s);CHECK(s.tempo_percent==99 && s.tempo_fade_accumulator_us==8317);
    end_sequence(&s);return true;
}
static bool source_EOT_rewind_writer_set_and_no_same_tick_consume() {
    const uint8_t stream[]={0xff,0x51,3,7,0xa1,0x20,0xff,0x58,4,4,2,24,8,1,0xff,0x2f,0,0};
    for(uint32_t loops:{0u,1u,2u}) {
        AilDriver driver;CHECK(setup(driver));BgmVoice s;s.driver=&driver;prepare_sequence(&s);
        s.sequence.evnt=stream;s.sequence.size=sizeof(stream);s.sequence.loop_remaining=loops;CHECK(begin_sequence(&s));
        CHECK(native_sequence_tick(&s) && s.measure==0 && !s.beat_phase && s.sequence.cursor==14);
        s.beat=3;s.measure=17;s.beat_phase=555;
        s.sequence_volume=10;s.target_volume=20;s.fade_accumulator_us=12;s.fade_interval_us=1000;
        s.target_tempo=110;s.tempo_fade_accumulator_us=23;s.tempo_fade_interval_us=1000;
        CHECK(native_sequence_tick(&s));
        CHECK(s.beat_increment==133333 && s.beat_denominator==8000000 && s.beats_per_measure==4);
        CHECK(s.sequence_volume==10 && s.fade_accumulator_us==12 && s.tempo_percent==100 && s.tempo_fade_accumulator_us==23);
        if(loops==1) {
            CHECK(s.sequence.status==2 && s.sequence.cursor==17 && !s.sequence.loop_remaining);
            CHECK(s.beat==3 && s.measure==17 && s.beat_phase==555); /* final EOT does not reset */
        } else {
            CHECK(s.sequence.status==4 && !s.sequence.cursor && !s.beat && s.measure==UINT32_MAX && !s.beat_phase);
            CHECK(s.sequence.loop_remaining==(loops?loops-1:0));
            s.target_tempo=100;s.sequence_volume=s.target_volume=10;
            CHECK(native_sequence_tick(&s) && s.measure==0 && !s.beat_phase && s.sequence.cursor==14);
        }
        end_sequence(&s);
    }
    return true;
}
static bool source_meta_beat_checks() {
    return source_reset_and_success_init_time_writers() && source_meta_parameters_and_signed_shift() &&
        source_beat_single_if_and_signed_words() && source_tempo_accumulator_and_outer_tick() &&
        source_tempo_fade_after_steps_not_meta51() && source_EOT_rewind_writer_set_and_no_same_tick_consume();
}
