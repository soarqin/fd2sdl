/* FD2 sub_449E0 @0x449e0 / sub_43230 @0x43230 / sub_42980 @0x42980 /
 * sub_43270 @0x43270（均保原登记名）：原机器码整数渐变与 CC7 重发回归。
 * 使用真实 SAMPLE.AD/FDMUS；同 TU 分别检查逻辑消息和已提交driver，不导出调试 API。
 * 六个历史hash保原身份，旧runner／stub的精确逻辑点未恢复。当前逻辑消息层
 * 另有source-fade-arithmetic-oracle01四完整source的IDIV/accum/eighth独立oracle，
 * 六hash精确同；物理packet另证，不签原CPU/PIC。无窗口、设备或录音。 */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
/* 此观察只记录当前逻辑消息；不借历史stub未恢复的位置扩签原层次。
 * 算式独立依据见文件头；原502硬件提交另由bgm_message_queue_checks检查。 */
static void observe_logical_message(void *,uint8_t,uint8_t,uint8_t);
#define FD2_BGM_NATIVE_TRACE(w,s,a,b) observe_logical_message(w,s,a,b)
#include "../bgm.cpp"
#include "audio_clock_fixture.h"

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#x); return false; } } while (0)

struct MessageObservation { uint32_t tick, status, controller, value; };
static AilDriver *observed_driver = nullptr;
static uint8_t observed_channel = 0;
static uint32_t observed_tick = 0;
static std::vector<MessageObservation> observations;
static void observe_logical_message(void *who,uint8_t status,uint8_t a,uint8_t b) {
    auto *voice=static_cast<BgmVoice *>(who);
    if (observed_driver && voice->driver==observed_driver && (status&0xf0u)==0xb0u &&
        (status&15u)==observed_channel && a==7)
        observations.push_back({observed_tick,status,7,b});
}
static void fold(uint64_t &value, uint32_t word) {
    for (unsigned i = 0; i < 4; ++i) {
        value ^= static_cast<uint8_t>(word >> (8 * i));
        value *= UINT64_C(1099511628211);
    }
}
static void isolated_fade_tick(BgmVoice *voice) {
    ++voice->timer_ticks;advance_sequence_fade(voice);
    flush_driver_messages(voice->driver); /* 给定一次完整service的fade/tail切片。 */
}
static bool setup_driver(AilDriver &driver) {
    CHECK(load_ail_bank("original_game/SAMPLE.AD", driver.instruments));
    reset_driver(&driver);
    /* 合成fade给定原TIMB已注册program0；不是冷bank即热selected。 */
    CHECK(register_instrument(&driver,0,0));
    return true;
}
static bool public_start_uses_controller_volume() {
    fd2_audio_config cfg{}; cfg.sample_rate=48000; cfg.open_device=0; cfg.allow_null=1;
    fd2_audio *audio=fd2_audio_create(&cfg); CHECK(audio);
    CHECK(fd2_test_audio_clock_prepare(audio)==0);
    fd2_bgm_config bg{}; bg.sample_rate=48000; bg.fdmus_path="original_game/FDMUS.DAT";
    bg.ail_bank_path="original_game/SAMPLE.AD";
    fd2_bgm_player *player=fd2_bgm_create(audio,&bg); CHECK(player);
    CHECK(fd2_test_audio_clock_after_mdi(audio)==0);
    CHECK(fd2_bgm_play(player,18,0)==0);
    float samples[512*2];
    for(unsigned left=24000;left;) {
        size_t count=std::min<unsigned>(left,512);
        CHECK(fd2_audio_render_offline(audio,samples,count)==count);left-=static_cast<unsigned>(count);
    }
    unsigned active_count=0; bool attenuated=false;
    for(const Voice &v:player->driver.voices) if(v.active) {
        ++active_count;
        if(player->driver.volumes[v.midi_channel]<127) attenuated=true;
    }
    std::printf("public half-second active=%u CC7_attenuated=%d\n",active_count,attenuated);
    bool ok=active_count>0 && attenuated;
    CHECK(fd2_bios_clock_close()==0);
    fd2_audio_destroy(audio);fd2_bgm_destroy(player);
    CHECK(ok);return true;
}

struct Golden {
    int initial, target; uint32_t milliseconds, ticks; uint8_t raw, logical, mapped;
    uint64_t state_hash, event_hash;
};
static const Golden golden[] = {
{0,127,2000,248,127,3,12,UINT64_C(0x421af229187b2ed7),UINT64_C(0x493badc3112561e0)},
    {127,0,4000,488,127,3,12,UINT64_C(0xd36f5e1dff985d53),UINT64_C(0xad8eaef7cea8d226)},
    {10,13,1,16,100,3,12,UINT64_C(0xd012e1d49fc93155),UINT64_C(0x646e006e555f6e88)},
    {100,20,0,8,91,3,12,UINT64_C(0x46257875b1145cad),UINT64_C(0x04934c33f9e74691)},
    {42,42,2000,8,127,3,12,UINT64_C(0x8bbc44a4c1c8f32d),UINT64_C(0x4d25767f9dce13f5)},
    {126,127,10,16,127,3,12,UINT64_C(0xf66ebc35e4194961),UINT64_C(0x023e8fd9b169c601)},
};
static bool native_vectors_and_message_writes() {
    for(const Golden &g:golden) {
        AilDriver driver; CHECK(setup_driver(driver));
        BgmVoice voice; voice.driver=&driver; prepare_sequence(&voice);CHECK(begin_sequence(&voice));
        voice.channel_map[g.logical]=g.mapped;
        set_sequence_volume(&voice,g.initial,0);
        seq_controller(&voice,g.logical,7,g.raw);
        seq_patch(&voice,g.logical,0);seq_note_on(&voice,g.logical,60,100,1000000);
        flush_driver_messages(&driver); /* 独立stub golden的已提交起态；不宣称真实初始化时间。 */
        CHECK(driver.voices[0].active && driver.voices[0].opl_channel==0);
        observed_driver=&driver;observed_channel=g.mapped;observed_tick=0;observations.clear();
        set_sequence_volume(&voice,g.target,g.milliseconds);
        uint64_t state=UINT64_C(14695981039346656037);
        for(uint32_t tick=1;tick<=g.ticks;++tick) {
            observed_tick=tick;isolated_fade_tick(&voice);
            fold(state,tick);fold(state,static_cast<uint32_t>(voice.sequence_volume));
            fold(state,voice.fade_accumulator_us);fold(state,driver.volumes[g.mapped]);
        }
        uint64_t events=UINT64_C(14695981039346656037);
        fold(events,static_cast<uint32_t>(observations.size()));
        for(const auto &o:observations) {fold(events,o.tick);fold(events,o.status);fold(events,o.controller);fold(events,o.value);}
        observed_driver=nullptr;
        std::printf("native fade %d->%d/%ums state=%016llx events=%016llx CC7=%zu\n",
            g.initial,g.target,g.milliseconds,static_cast<unsigned long long>(state),
            static_cast<unsigned long long>(events),observations.size());
        CHECK(state==g.state_hash && events==g.event_hash);
        CHECK(voice.volume_cache[g.logical]==g.raw);
        CHECK(voice.sequence_volume==g.target && voice.driver->active_sequence==&voice);
        end_sequence(&voice);
    }
    return true;
}
static bool uncached_and_mapped_controller() {
    AilDriver driver;CHECK(setup_driver(driver));
    BgmVoice voice;voice.driver=&driver;prepare_sequence(&voice);CHECK(begin_sequence(&voice));
    driver_patch(&driver,0,0);driver_note_on(&driver,0,60,100);
    observed_driver=&driver;observed_channel=0;observations.clear();
    set_sequence_volume(&voice,0,0);set_sequence_volume(&voice,127,2000);
    for(unsigned n=0;n<248;++n)isolated_fade_tick(&voice);
    observed_driver=nullptr;
    CHECK(observations.empty() && driver.volumes[0]==127);
    voice.channel_map[3]=12;set_sequence_volume(&voice,63,0);seq_controller(&voice,3,7,100);
    CHECK(voice.volume_cache[3]==100 && driver.volumes[12]==127 && driver.queued_messages==1);
    flush_driver_messages(&driver);
    CHECK(driver.volumes[12]==49 && driver.volumes[3]==127);
    set_sequence_volume(&voice,127,0);
    CHECK(voice.volume_cache[3]==100 && driver.volumes[12]==49 && driver.queued_messages==1);
    flush_driver_messages(&driver);
    CHECK(driver.volumes[12]==100 && driver.volumes[3]==127);
    seq_patch(&voice,3,0);seq_note_on(&voice,3,61,100,1000000);seq_controller(&voice,3,64,127);
    end_sequence(&voice);
    CHECK(driver.pedals[12]==127 && driver.queued_messages==1 && !driver.active_sequence);
    mdi_timer_service(&driver); /* 447d0尾reset留包；inactive43270仍flush。 */
    CHECK(driver.pedals[12]==0 && driver.pedals[3]==0);
    /* 旧 source 的迟到渐变不可写已接管的新 sequence/driver。 */
    BgmVoice next;next.driver=&driver;prepare_sequence(&next);CHECK(begin_sequence(&next));
    const auto before=std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(&driver),
        reinterpret_cast<const uint8_t*>(&driver)+sizeof(driver));
    set_sequence_volume(&voice,7,0);isolated_fade_tick(&voice);
    CHECK(!std::memcmp(before.data(),&driver,before.size()));
    end_sequence(&next);return true;
}
static bool render_chunk_invariance() {
    AilDriver a,b;CHECK(setup_driver(a));CHECK(setup_driver(b));
    BgmVoice va,vb;va.driver=&a;vb.driver=&b;prepare_sequence(&va);prepare_sequence(&vb);
    CHECK(begin_sequence(&va) && begin_sequence(&vb));
    seq_patch(&va,0,0);seq_patch(&vb,0,0);seq_controller(&va,0,7,127);seq_controller(&vb,0,7,127);
    seq_note_on(&va,0,60,100,1000000);seq_note_on(&vb,0,60,100,1000000);
    set_sequence_volume(&va,0,0);set_sequence_volume(&vb,0,0);
    set_sequence_volume(&va,127,2000);set_sequence_volume(&vb,127,2000);
    flush_driver_messages(&a);flush_driver_messages(&b); /* 保原chunk测试确有发声before。 */
    std::vector<float> first(16001*2),second(16001*2);
    static const uint8_t silent[]={127,0xff,0x2f,0};
    va.sequence.evnt=vb.sequence.evnt=silent;va.sequence.size=vb.sequence.size=sizeof(silent);
    render_opl_frames(va.driver,first.data(),16001);
    size_t at=0;const size_t chunks[]={1,399,512,3,400,2048,17};unsigned chunk=0;
    while(at<16001) {size_t n=std::min(chunks[chunk++%7],16001-at);
        render_opl_frames(vb.driver,second.data()+at*2,n);at+=n;}
    CHECK(first==second && va.sequence_volume==vb.sequence_volume && va.timer_ticks==vb.timer_ticks);
    CHECK(a.volumes==b.volumes && va.timer_ticks==0u && vb.timer_ticks==0u);
    mdi_timer_service(&a);mdi_timer_service(&b);
    CHECK(va.timer_ticks==1u && vb.timer_ticks==1u && va.sequence_volume==vb.sequence_volume);
    end_sequence(&va);end_sequence(&vb);return true;
}
static bool real_stop_and_cross_track_owner() {
    fd2_audio_config cfg{};cfg.sample_rate=48000;cfg.allow_null=1;
    fd2_audio *audio=fd2_audio_create(&cfg);CHECK(audio);
    CHECK(fd2_test_audio_clock_prepare(audio)==0);
    fd2_bgm_config bg{};bg.sample_rate=48000;bg.fdmus_path="original_game/FDMUS.DAT";bg.ail_bank_path="original_game/SAMPLE.AD";
    fd2_bgm_player *player=fd2_bgm_create(audio,&bg);CHECK(player);
    CHECK(fd2_test_audio_clock_after_mdi(audio)==0);
    float buffer[512*2];
    const unsigned tracks[]={18,11,19,12};
    for(unsigned track:tracks) {
        CHECK(fd2_bgm_play(player,track,0)==0);
        CHECK(fd2_audio_render_offline(audio,buffer,1)==1);
        BgmVoice *voice=player->driver.active_sequence;CHECK(voice && fd2_bgm_current_track(player)==static_cast<int>(track));
        CHECK(voice->target_volume==127 && voice->start_fade);
        for(unsigned i=0;i<200;++i)CHECK(fd2_audio_render_offline(audio,buffer,512)==512);
        CHECK(voice->sequence_volume==127 && voice->timer_ticks>240);
        const auto snapshot=std::vector<uint8_t>(reinterpret_cast<const uint8_t*>(&player->driver),
            reinterpret_cast<const uint8_t*>(&player->driver)+sizeof(player->driver));
        CHECK(fd2_bgm_play(player,999,0)==-1);
        CHECK(!std::memcmp(snapshot.data(),&player->driver,snapshot.size()));
        uint32_t ticks=voice->timer_ticks;
        CHECK(fd2_bgm_play(player,track,0)==0 && voice->timer_ticks==ticks);
        CHECK(fd2_bgm_stop(player)==0);
        for(unsigned i=0;i<400;++i)CHECK(fd2_audio_render_offline(audio,buffer,512)==512);
        CHECK(fd2_bgm_current_track(player)==-1 && voice->sequence_volume==0);
        CHECK(player->driver.active_sequence==voice && voice->sequence.status==4);
        ticks=voice->timer_ticks;
        CHECK(fd2_audio_render_offline(audio,buffer,512)==512 && voice->timer_ticks>ticks);
    }
    CHECK(fd2_bios_clock_close()==0);
    fd2_audio_destroy(audio);CHECK(!player->driver.active_sequence);fd2_bgm_destroy(player);return true;
}

int main() {
    if(!public_start_uses_controller_volume())return 1;
    if(!native_vectors_and_message_writes() || !uncached_and_mapped_controller() ||
       !render_chunk_invariance() || !real_stop_and_cross_track_owner())return 1;
    std::puts("bgm_sequence_volume_test: ok");return 0;
}
