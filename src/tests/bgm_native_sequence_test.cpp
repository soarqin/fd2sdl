/* ail_mdi_sequences_timer_service @0x43270（原登记 sub_43270）与
 * ail_sequence_pending_notes_release @0x42dd0（原登记 sub_42DD0）32槽/同tick回归。
 * golden来自独立原机器码：仅存15曲短指纹，不嵌入原曲事件/资源/音频。
 * 同TU观察，纯离线，不导出播放器调试API，不开设备或窗口。 */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>
static void observed_message(void *,uint8_t,uint8_t,uint8_t);
#define FD2_BGM_NATIVE_TRACE(voice,status,a,b) observed_message(voice,status,a,b)
#include "../bgm.cpp"

#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#x);return false;}}while(0)
struct Message {uint32_t tick,status,a,b;};
static BgmVoice *observed_voice=nullptr;
static std::vector<Message> messages;
static void observed_message(void *who,uint8_t status,uint8_t a,uint8_t b) {
    if(who==observed_voice)messages.push_back({observed_voice->timer_ticks,status,a,b});
}
static void fold(uint64_t &h,uint32_t word) {
    for(unsigned i=0;i<4;++i){h^=static_cast<uint8_t>(word>>(8*i));h*=UINT64_C(1099511628211);}
}
static uint64_t packet_hash() {
    uint64_t h=UINT64_C(14695981039346656037);
    for(const auto &m:messages){fold(h,m.tick);fold(h,m.status);fold(h,m.a);fold(h,m.b);}
    return h;
}
static void append_note(std::vector<uint8_t> &out,uint8_t key,uint8_t duration) {
    out.insert(out.end(),{0x90,key,100,duration});
}
static void append_eot(std::vector<uint8_t> &out){out.insert(out.end(),{0xff,0x2f,0});}
static void be_word(std::vector<uint8_t> &out,uint32_t n) {
    out.insert(out.end(),{uint8_t(n>>24),uint8_t(n>>16),uint8_t(n>>8),uint8_t(n)});
}
static std::vector<uint8_t> chunk(const char *name,const std::vector<uint8_t> &payload) {
    std::vector<uint8_t> out(name,name+4);be_word(out,static_cast<uint32_t>(payload.size()));
    out.insert(out.end(),payload.begin(),payload.end());if(payload.size()&1)out.push_back(0);return out;
}
static std::vector<uint8_t> xmidi(const std::vector<uint8_t> &evnt) {
    auto info=chunk("INFO",{1,0});std::vector<uint8_t> xdir={'X','D','I','R'};
    xdir.insert(xdir.end(),info.begin(),info.end());auto header=chunk("FORM",xdir);
    auto data=chunk("EVNT",evnt);std::vector<uint8_t> form={'X','M','I','D'};
    form.insert(form.end(),data.begin(),data.end());auto encoded=chunk("FORM",form);
    std::vector<uint8_t> cat={'X','M','I','D'};cat.insert(cat.end(),encoded.begin(),encoded.end());
    auto tail=chunk("CAT ",cat);header.insert(header.end(),tail.begin(),tail.end());return header;
}
static bool setup(AilDriver &driver) {
    CHECK(load_ail_bank("original_game/SAMPLE.AD",driver.instruments));reset_driver(&driver);return true;
}

static bool capacity_before_noteoff() {
    std::vector<uint8_t> stream;
    for(uint8_t note=30;note<63;++note)append_note(stream,note,100);
    stream.push_back(1);append_eot(stream);
    AilDriver driver;CHECK(setup(driver));BgmVoice voice;voice.driver=&driver;
    prepare_sequence(&voice);observed_voice=&voice;messages.clear();
    voice.sequence.evnt=stream.data();voice.sequence.size=stream.size();
    CHECK(begin_sequence(&voice));native_sequence_tick(&voice);
    CHECK(voice.sequence.queue_overflow && voice.sequence.status==2 && voice.timer_ticks==1);
    unsigned on=0,off=0;
    for(const auto &m:messages){if((m.status&0xf0)==0x90)++on;if((m.status&0xf0)==0x80)++off;}
    std::printf("capacity original=33on/32off, actual=%uon/%uoff\n",on,off);
    bool correct=messages.size()==65 && on==33 && off==32;
    if(correct)for(size_t i=0;i<messages.size();++i) {
        correct &= messages[i].status==(i<33?0x90u:0x80u);
        correct &= messages[i].a==(i<33?30+i:30+i-33);
    }
    observed_voice=nullptr;end_sequence(&voice);CHECK(correct);return true;
}

struct Golden {uint32_t ticks,events;uint64_t state,packets;};
static const Golden synthetic[] = {
{10,6,UINT64_C(0x30680d779e4f9362),UINT64_C(0xfa53d53d3c2f5bf1)},
    {9,6,UINT64_C(0x6cb59ee41daafb77),UINT64_C(0x7a0350971d925f9c)},
    {5,4,UINT64_C(0x799b874a95c77e2c),UINT64_C(0x34f53a92c76564fc)},
    {1,65,UINT64_C(0x868a9d75370ae2f3),UINT64_C(0xf1ee6f525453486e)},
    {12,13,UINT64_C(0xd107d8ae2bd89a55),UINT64_C(0x82e3d422a1c84dd2)},
    {2,4,UINT64_C(0xaf0a6fc91b75b9ee),UINT64_C(0x47c3d7287d35a2c5)},
};
struct TrackGolden {size_t index;uint32_t ticks,peak,events;uint64_t state,packets;};
static const TrackGolden tracks[] = {
{1,5421,7,2834,UINT64_C(0x2dfe861606455aad),UINT64_C(0x7b6de06c92e78c57)},
    {3,6679,13,3121,UINT64_C(0xb9fdf511f78d7c39),UINT64_C(0x828a8e1737c54c6b)},
    {4,7373,12,3043,UINT64_C(0x2188af7742b5a8ac),UINT64_C(0xbe6009ef37b1634d)},
    {6,6904,11,4662,UINT64_C(0xa3d4ab7bec918f59),UINT64_C(0x0740bb0d77758618)},
    {8,14983,14,5918,UINT64_C(0xd90cbe68969bd8f2),UINT64_C(0xb8fe6c61e2288208)},
    {10,5760,13,2416,UINT64_C(0x88adb65f42de4bc5),UINT64_C(0xe84c699e40955e54)},
    {11,13691,8,1256,UINT64_C(0x564f59f3084f37e7),UINT64_C(0x6aea422ccf64efc5)},
    {12,1441,15,1336,UINT64_C(0x6e65e57c39786034),UINT64_C(0xa8a575157ed58461)},
    {13,7680,9,3406,UINT64_C(0xa134870df7e6b8ec),UINT64_C(0x34efb5bd6cb4bde0)},
    {14,4608,8,1433,UINT64_C(0x46728fc230a635bb),UINT64_C(0x7cda584c509eccba)},
    {15,5256,11,2011,UINT64_C(0xc96ce27f1321fe34),UINT64_C(0x91784ea27603df03)},
    {16,321,8,194,UINT64_C(0x995a77edc2dc1777),UINT64_C(0x24e4c9696e935e3a)},
    {17,885,9,456,UINT64_C(0xe2b9792499354ea0),UINT64_C(0x19e122983bb231e0)},
    {18,8238,25,4836,UINT64_C(0xebd8f60775ae184d),UINT64_C(0x245b0713dedab930)},
    {19,2076,12,818,UINT64_C(0xe2f4e2d810934ae9),UINT64_C(0xeb8dbc70c7d360f9)},
};
static const TrackGolden two_loops[] = {
    {1,10842,7,5518,UINT64_C(0x95f953eb63760159),UINT64_C(0x88dbf77743ffc477)},
    {3,13358,13,6062,UINT64_C(0x7999726ce804941d),UINT64_C(0xbc57483cec90fd35)},
    {4,14746,12,5846,UINT64_C(0xb83b088a698b1cb7),UINT64_C(0x31204f9fdfafae52)},
    {6,13808,11,9144,UINT64_C(0xd3044ad2074bcada),UINT64_C(0x8142933727a302cc)},
    {8,29966,14,11626,UINT64_C(0xfbb2c6235972a67f),UINT64_C(0xf25f3c6ec93893f9)},
    {10,11520,13,4592,UINT64_C(0x0dc5920007a626d1),UINT64_C(0x5c9193836566c199)},
    {11,27382,8,2422,UINT64_C(0x4470b55a30abaa6b),UINT64_C(0x05085c8cb2abf580)},
    {12,2882,15,2492,UINT64_C(0x937c8eec93f275f3),UINT64_C(0xf5a70bb81713805c)},
    {13,15360,9,6602,UINT64_C(0x50eb9329e7f0092c),UINT64_C(0x0dbd3b127055241d)},
    {14,9216,8,2626,UINT64_C(0x46f908084caf52cf),UINT64_C(0xd37a17386de6a2f3)},
    {15,10512,11,3782,UINT64_C(0x14d6c3160e98cb96),UINT64_C(0xb6f418c6691c8d47)},
    {16,642,8,388,UINT64_C(0x07a5cc15e77b9ced),UINT64_C(0x44073db766f97154)},
    {17,1770,9,912,UINT64_C(0x081949cc2ace8317),UINT64_C(0xf3fa3d2c2531ce79)},
    {18,16476,25,9372,UINT64_C(0xeacbc5ea9dd3dada),UINT64_C(0xe31b3a6c2a7bec81)},
    {19,4152,12,1486,UINT64_C(0x2cc25ff5cef7a1ac),UINT64_C(0x0273ab1edfd79eba)},
};
static void state_hash(uint64_t &h,const BgmVoice &voice) {
    const NativeSequence &s=voice.sequence;
    fold(h,voice.timer_ticks);fold(h,s.status);fold(h,static_cast<uint32_t>(s.cursor));fold(h,s.delay);
    fold(h,s.pending_count);fold(h,static_cast<uint32_t>(voice.sequence_volume));
    fold(h,voice.fade_accumulator_us);fold(h,s.loop_remaining);
    for(const DurationSlot &slot:s.pending) {
        fold(h,static_cast<uint32_t>(slot.channel));
        if(slot.channel>=0){fold(h,slot.note);fold(h,slot.remaining);}
    }
}
static bool collect(BgmVoice &voice,const Golden &g,uint32_t expected_peak=0) {
    uint64_t state=UINT64_C(14695981039346656037);uint32_t peak=0;
    observed_voice=&voice;messages.clear();
    for(uint32_t n=0;n<g.ticks;++n){CHECK(voice.sequence.status==4);native_sequence_tick(&voice);
        state_hash(state,voice);peak=std::max(peak,voice.sequence.pending_count);}
    auto packets=packet_hash();
    std::printf("ticks=%u state=%016llx packets=%016llx events=%zu peak=%u\n",g.ticks,
        static_cast<unsigned long long>(state),static_cast<unsigned long long>(packets),messages.size(),peak);
    CHECK(state==g.state && packets==g.packets && messages.size()==g.events);
    if(expected_peak)CHECK(peak==expected_peak);
    observed_voice=nullptr;return true;
}
static bool synthetic_native_vectors() {
    std::vector<std::vector<uint8_t>> streams(6);
    append_note(streams[0],70,2);append_note(streams[0],71,4);streams[0].push_back(2);
    append_note(streams[0],72,2);streams[0].push_back(8);append_eot(streams[0]);
    append_note(streams[1],60,2);append_note(streams[1],60,4);streams[1].push_back(2);
    append_note(streams[1],60,5);streams[1].push_back(10);append_eot(streams[1]);
    append_note(streams[2],60,0);streams[2].insert(streams[2].end(),{0,0xb0,64,127,1,0xb0,64,0,2});append_eot(streams[2]);
    for(uint8_t n=30;n<63;++n)append_note(streams[3],n,100);streams[3].push_back(1);append_eot(streams[3]);
    streams[4]={0xb0,7,127};append_note(streams[4],60,10);streams[4].push_back(1);append_eot(streams[4]);
    append_note(streams[5],60,10);append_note(streams[5],61,10);streams[5].push_back(1);append_eot(streams[5]);
    for(size_t i=0;i<streams.size();++i) {
        AilDriver driver;CHECK(setup(driver));BgmVoice voice;voice.driver=&driver;voice.track=18;prepare_sequence(&voice);
        voice.sequence.evnt=streams[i].data();voice.sequence.size=streams[i].size();voice.sequence.loop_remaining=i==4?0:1;
        CHECK(begin_sequence(&voice));CHECK(collect(voice,synthetic[i]));
        observed_voice=nullptr;end_sequence(&voice);
    }
    return true;
}
static bool all_original_tracks() {
    fd2_archive archive{};CHECK(fd2_archive_open(&archive,"original_game/FDMUS.DAT")==0);
    CHECK(archive.count==20);
    for(uint32_t loops=1;loops<=2;++loops)for(size_t index=0;index<15;++index) {
        const auto &t=loops==1?tracks[index]:two_loops[index];
        AilDriver driver;CHECK(setup(driver));BgmVoice voice;voice.driver=&driver;voice.track=static_cast<int>(t.index);prepare_sequence(&voice);
        const uint8_t *entry=nullptr;size_t size=0;CHECK(fd2_archive_get(&archive,t.index,&entry,&size)==0);
        auto before=driver;NativeSequence prepared;
        CHECK(prepare_native_sequence(prepared,entry,size));CHECK(!std::memcmp(&before,&driver,sizeof(driver)));
        voice.sequence=prepared;voice.sequence.loop_remaining=loops;CHECK(begin_sequence(&voice));
        std::printf("real track %zu loops=%u ",t.index,loops);CHECK(collect(voice,{t.ticks,t.events,t.state,t.packets},t.peak));
        CHECK(voice.sequence.status==2 && !voice.sequence.pending_count && !voice.sequence.queue_overflow);
        uint32_t tick=voice.timer_ticks;auto prior_messages=messages.size();observed_voice=&voice;
        native_sequence_tick(&voice);CHECK(voice.timer_ticks==tick && messages.size()==prior_messages);
        observed_voice=nullptr;end_sequence(&voice);
    }
    fd2_archive_close(&archive);return true;
}
static bool preflight_and_unreachable_tail() {
    std::vector<uint8_t> valid={0x90,60,100,0,1,0xff,0x2f,0,0xf1,0xff};
    auto encoded=xmidi(valid);NativeSequence out;CHECK(prepare_native_sequence(out,encoded.data(),encoded.size()));
    CHECK(out.size==valid.size());
    AilDriver driver;CHECK(setup(driver));BgmVoice voice;voice.driver=&driver;prepare_sequence(&voice);
    voice.sequence=out;voice.sequence.loop_remaining=0;CHECK(begin_sequence(&voice));
    for(unsigned i=0;i<12;++i)native_sequence_tick(&voice);
    CHECK(voice.sequence.status==4 && !voice.sequence.queue_overflow);
    end_sequence(&voice);
    for(const std::vector<uint8_t> &bad:std::vector<std::vector<uint8_t>>{
        {0xf1,0xff,0x2f,0},{0xb0,110,127,0xff,0x2f,0},{0x90,60,100,0x80},
        {0xff,0x51,2,1,2,0xff,0x2f,0},{1,2,3}}) {
        auto file=xmidi(bad);auto before=out;CHECK(!prepare_native_sequence(out,file.data(),file.size()));
        CHECK(!std::memcmp(&out,&before,sizeof(out)));
    }
    return true;
}
static bool finite_release_and_driver_clock() {
    std::vector<uint8_t> data;append_note(data,60,1);data.push_back(1);append_eot(data);
    AilDriver driver;CHECK(setup(driver));BgmVoice voice;voice.driver=&driver;prepare_sequence(&voice);
    voice.sequence.evnt=data.data();voice.sequence.size=data.size();CHECK(begin_sequence(&voice));
    native_sequence_tick(&voice);native_sequence_tick(&voice);CHECK(voice.sequence.status==2 && voice.timer_ticks==2);
    float samples[512*2];auto phase=driver.timer_sample_units;render_opl_frames(&voice,samples,512);
    CHECK(voice.timer_ticks==2 && driver.timer_sample_units!=phase && driver.active_sequence==&voice);
    observed_voice=&voice;messages.clear();end_sequence(&voice);CHECK(messages.empty() && !driver.active_sequence);
    observed_voice=nullptr;return true;
}
int main() {
    if(!capacity_before_noteoff())return 1;
    if(!synthetic_native_vectors() || !all_original_tracks() || !preflight_and_unreachable_tail() ||
       !finite_release_and_driver_clock())return 1;
    std::puts("bgm_native_sequence_test: ok (no device/GUI/runtime waveform)");return 0;
}
