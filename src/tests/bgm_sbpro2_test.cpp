/* SBPRO2.MDI 静态文件偏移 3085/36f6/39cc/3b1e/3b54/3184 回归。
 * 同 TU 观察现有 OPL/逻辑槽；不导出调试 API，不开设备、不生成录音。
 * 公平分配向量来自 27a7a99a readset；音高独立指纹来自原 7fd/97d/9dd
 * 表和 35fa..36a9 指令算式（不是原 kernel 或运行时 OPL 证据）。 */
#include <cstdint>
static void observed_native(void *, uint8_t, uint8_t, uint8_t);
static void observed_message_batch(void *,const uint8_t *,uint32_t,uint32_t);
static void observed_startup_owner(void *,unsigned);
#define FD2_BGM_STARTUP_OWNER_TRACE(d,p) observed_startup_owner(d,p)
#define FD2_BGM_MESSAGE_BATCH_TRACE(d,b,u,c) observed_message_batch(d,b,u,c)
#define FD2_BGM_NATIVE_TRACE(voice,status,a,b) observed_native(voice,status,a,b)
#include "../bgm.cpp"
#include "audio_clock_fixture.h"

#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#x); return false; } } while (0)

static bool setup(AilDriver &voice) {
    CHECK(load_ail_bank("original_game/SAMPLE.AD", voice.instruments));
    reset_driver(&voice);
    /* 私有rawdriver给定前态：显式合法注册全162项，不把冷bank当runtime cache。
     * 此请求顺序只属于该fixture，不称自然曲史；2268B/192slots容量已独立闭合。 */
    for(unsigned bank:{0u,127u})for(unsigned patch=0;patch<128;patch++) {
        int key=instrument_index(uint8_t(patch),uint8_t(bank));
        if(key>=0 && voice.instruments[size_t(key)].present)
            CHECK(register_instrument(&voice,uint8_t(patch),uint8_t(bank)));
    }
    driver_patch(&voice,0,0); driver_patch(&voice,1,0); driver_patch(&voice,2,0);
    return true;
}
static size_t active(const AilDriver &voice, int channel=-1) {
    size_t count=0;
    for (const Voice &slot:voice.voices)
        if(slot.active && (channel<0 || slot.midi_channel==channel)) count++;
    return count;
}
static const Voice *find_note(const AilDriver &voice,uint8_t channel,uint8_t note) {
    for (const Voice &slot:voice.voices)
        if(slot.active && slot.midi_channel==channel && slot.note==note)return &slot;
    return nullptr;
}
static bool pan_regression() {
    AilDriver voice;CHECK(setup(voice));driver_note_on(&voice,0,60,100);
    CHECK(voice.opl.channel[0].cha && voice.opl.channel[0].chb);
    driver_controller(&voice,0,10,27);
    CHECK(!voice.opl.channel[0].cha && voice.opl.channel[0].chb);
    driver_controller(&voice,0,10,28);
    CHECK(voice.opl.channel[0].cha && voice.opl.channel[0].chb);
    driver_controller(&voice,0,10,100);
    CHECK(voice.opl.channel[0].cha && !voice.opl.channel[0].chb);
    return true;
}
static bool pedal_regression() {
    AilDriver voice;CHECK(setup(voice));driver_note_on(&voice,0,60,100);driver_note_on(&voice,0,60,100);
    driver_note_on(&voice,1,60,100);driver_controller(&voice,0,64,64);driver_note_off(&voice,0,60);
    CHECK(active(voice,0)==2 && active(voice,1)==1);
    driver_controller(&voice,0,64,63);CHECK(active(voice,0)==0 && active(voice,1)==1);
    return true;
}
static bool fair_regression() {
    AilDriver voice;CHECK(setup(voice));
    for(uint8_t note=40;note<59;note++)driver_note_on(&voice,0,note,100);
    CHECK(find_note(voice,0,40)!=nullptr);
    CHECK(find_note(voice,0,57)==nullptr);
    CHECK(find_note(voice,0,58)!=nullptr && active(voice)==18);
    return true;
}

static bool allocator_vectors() {
    AilDriver voice;CHECK(setup(voice));CHECK(voice.opl.newm==1);
    for(unsigned c=0;c<16;c++)CHECK(voice.volumes[c]==127 && voice.pans[c]==64 && voice.pitch_ranges[c]==2);
    for(uint8_t i=0;i<18;i++){
        driver_note_on(&voice,0,static_cast<uint8_t>(40+i),100);
        CHECK(voice.voices[i].opl_channel==i);
        CHECK(voice.opl.channel[i].slotz[0]->key && voice.opl.channel[i].slotz[1]->key);
    }
    CHECK(voice.last_free_channel==17 && voice.assigned_counts[0]==18);
    driver_note_on(&voice,0,58,100);
    CHECK(!voice.voices[17].active && voice.voices[18].opl_channel==17);
    CHECK(voice.last_free_channel==17 && voice.assigned_counts[0]==18);
    CHECK(setup(voice));
    for(uint8_t i=0;i<3;i++)driver_note_on(&voice,0,static_cast<uint8_t>(40+i),100);
    driver_note_off(&voice,0,40);driver_note_on(&voice,0,70,100);
    CHECK(voice.voices[0].opl_channel==3 && voice.last_free_channel==3);
    CHECK(voice.hardware_channels[0]==kUnassigned && voice.assigned_counts[0]==3);
    /* 跨 channel 的 assigned count 影响公平分数，不能只测同 channel 平分。 */
    CHECK(setup(voice));
    for(uint8_t i=0;i<12;i++)driver_note_on(&voice,0,static_cast<uint8_t>(40+i),100);
    for(uint8_t i=0;i<6;i++)driver_note_on(&voice,1,static_cast<uint8_t>(60+i),100);
    driver_note_on(&voice,2,90,100);
    CHECK(!voice.voices[11].active && voice.voices[18].opl_channel==11);
    CHECK(voice.assigned_counts[0]==11 && voice.assigned_counts[1]==6 && voice.assigned_counts[2]==1);
    CHECK(setup(voice));
    driver_controller(&voice,0,112,64);
    for(uint8_t i=0;i<18;i++)driver_note_on(&voice,0,static_cast<uint8_t>(40+i),100);
    driver_note_on(&voice,1,90,100);driver_note_on(&voice,2,91,100);
    CHECK(active(voice)==20 && voice.voices[18].opl_channel==kUnassigned && voice.voices[19].opl_channel==kUnassigned);
    driver_note_on(&voice,0,92,100);CHECK(active(voice)==20 && !find_note(voice,0,92));
    /* 3127本体无硬件早返保活；39cc返回后3a12另有无条件active=0。 */
    release_voice(&voice,voice.voices[18]);CHECK(find_note(voice,1,90));
    driver_note_off(&voice,1,90);CHECK(!find_note(voice,1,90));
    driver_controller(&voice,0,112,0);driver_controller(&voice,1,112,64);
    /* CC112 只存值，不能提前重分配。 */
    CHECK(voice.voices[18].opl_channel==kUnassigned);
    driver_note_off(&voice,0,40);driver_note_on(&voice,0,93,100);
    CHECK(voice.voices[0].opl_channel==0);
    /* 一个 36f6 调用内可交换两个原未分配逻辑槽，先 release 再 program。 */
    CHECK(setup(voice));driver_controller(&voice,0,112,64);
    for(uint8_t i=0;i<18;i++)driver_note_on(&voice,0,static_cast<uint8_t>(40+i),100);
    driver_note_on(&voice,1,90,100);driver_controller(&voice,0,112,0);driver_note_on(&voice,2,91,100);
    CHECK(voice.voices[19].opl_channel==17 && voice.voices[18].opl_channel==16);
    CHECK(voice.assigned_counts[0]==16 && voice.assigned_counts[1]==1 && voice.assigned_counts[2]==1);
    CHECK(voice.last_free_channel==17);
    return true;
}
static bool sustain_reset_and_reuse() {
    AilDriver voice;CHECK(setup(voice));
    driver_note_on(&voice,0,60,100);driver_controller(&voice,0,64,127);driver_note_on(&voice,0,61,100);
    driver_note_off(&voice,0,60);CHECK(voice.voices[0].deferred);
    driver_controller(&voice,0,7,90);driver_controller(&voice,0,10,100);driver_controller(&voice,0,6,12);
    driver_controller(&voice,0,112,80);driver_controller(&voice,0,1,127);driver_controller(&voice,0,11,30);
    driver_bend(&voice,0,50,1);driver_patch(&voice,0,5);
    driver_controller(&voice,0,121,0);
    CHECK(!find_note(voice,0,60) && find_note(voice,0,61));
    CHECK(voice.pedals[0]==0 && voice.expressions[0]==127 && voice.modulations[0]==0 && voice.bends[0]==8192);
    CHECK(voice.pans[0]==100 && voice.volumes[0]==90 && voice.pitch_ranges[0]==12 && voice.priorities[0]==80 && voice.patches[0]==5);
    driver_note_on(&voice,0,62,100);CHECK(!voice.voices[0].deferred && voice.voices[0].note==62);
    driver_controller(&voice,0,64,0);CHECK(find_note(voice,0,62) && find_note(voice,0,61));
    driver_controller(&voice,0,64,127);driver_controller(&voice,0,123,0);
    CHECK(active(voice,0)==2 && voice.voices[0].deferred && voice.voices[1].deferred);
    driver_controller(&voice,0,64,0);CHECK(active(voice,0)==0);
    driver_note_on(&voice,0,63,100);driver_note_on(&voice,0,63,0);CHECK(active(voice)==0);
    /* 3b1e 重入 39cc 会释放同原 note 的所有匹配，包含后来重按的实例。 */
    driver_note_on(&voice,0,64,100);driver_controller(&voice,0,64,127);driver_note_off(&voice,0,64);
    driver_note_on(&voice,0,64,100);CHECK(active(voice)==2);
    driver_controller(&voice,0,64,0);CHECK(active(voice)==0);
    return true;
}
static bool fixed_note_reentry() {
    AilDriver voice;CHECK(setup(voice));
    uint8_t note=35;
    while(note<=75){int i=instrument_index(note,127);if(voice.instruments[i].present && static_cast<uint8_t>(voice.instruments[i].note_offset)!=note)break;note++;}
    CHECK(note<=75);driver_note_on(&voice,9,note,100);CHECK(active(voice,9)==1);
    uint8_t fixed=find_note(voice,9,note)->tone;CHECK(fixed!=note);
    driver_controller(&voice,9,123,0);CHECK(active(voice,9)==1);
    driver_controller(&voice,9,64,127);driver_note_off(&voice,9,note);
    driver_controller(&voice,9,64,0);CHECK(active(voice,9)==1);
    driver_note_off(&voice,9,note);CHECK(active(voice,9)==0);
    return true;
}
static bool selective_controls() {
    AilDriver voice;CHECK(setup(voice));driver_note_on(&voice,0,60,100);
    uint16_t frequency=voice.opl.channel[0].f_num;uint8_t block=voice.opl.channel[0].block;
    uint8_t ar=voice.opl.channel[0].slotz[0]->reg_ar;
    driver_controller(&voice,0,7,0);CHECK(voice.opl.channel[0].slotz[1]->reg_tl==63);
    CHECK(voice.opl.channel[0].f_num==frequency && voice.opl.channel[0].block==block && voice.opl.channel[0].slotz[0]->reg_ar==ar);
    driver_controller(&voice,0,1,64);CHECK(voice.opl.channel[0].slotz[0]->reg_vib==1);
    driver_controller(&voice,0,1,0);CHECK(voice.opl.channel[0].slotz[0]->reg_vib==((voice.instruments[0].mod_20>>6)&1));
    driver_bend(&voice,0,96,0);frequency=voice.opl.channel[0].f_num;block=voice.opl.channel[0].block;
    driver_controller(&voice,0,6,12);CHECK(voice.opl.channel[0].f_num==frequency && voice.opl.channel[0].block==block);
    driver_bend(&voice,0,96,0);CHECK(voice.opl.channel[0].f_num!=frequency || voice.opl.channel[0].block!=block);
    driver_controller(&voice,0,0,127);driver_controller(&voice,0,32,127);CHECK(voice.banks[0]==0);
    driver_controller(&voice,0,114,127);driver_note_on(&voice,0,61,100);
    CHECK(find_note(voice,0,61) && find_note(voice,0,61)->instrument==0);
    driver_patch(&voice,0,0);driver_note_on(&voice,0,62,100);CHECK(!find_note(voice,0,62));
    /* 已鸣音保存原音色，后来的 program/bank 不能重解释旧 voice。 */
    driver_controller(&voice,0,10,0);CHECK(find_note(voice,0,60)->instrument==0);
    driver_note_on(&voice,9,0,100);CHECK(active(voice,9)==0);
    return true;
}
static bool pitch_static_vectors() {
    struct Vector{uint8_t tone;int8_t offset;uint16_t bend;uint8_t range;uint16_t expected;};
    const Vector vectors[]={ {0,0,8192,2,345},{11,0,8192,2,651},{12,0,8192,2,345},
        {60,0,8192,2,3762},{127,0,8192,2,7685},{60,0,0,2,3687},{60,0,16383,2,3846},
        {127,-128,32,127,447},{0,127,16383,127,597},{41,0,8192,2,1945} };
    Voice slot;
    for(const Vector &v:vectors){slot.tone=v.tone;slot.note_offset=v.offset;CHECK(ail_frequency(slot,v.bend,v.range)==v.expected);}
    uint64_t hash=UINT64_C(14695981039346656037);
    const uint8_t ranges[]={0,1,2,12,127};slot.note_offset=0;
    for(uint8_t range:ranges)for(unsigned note=0;note<128;note++)for(unsigned bend=0;bend<16384;bend+=32){
        slot.tone=static_cast<uint8_t>(note);uint16_t value=ail_frequency(slot,static_cast<uint16_t>(bend),range);
        hash=(hash^(value&255u))*UINT64_C(1099511628211);hash=(hash^(value>>8))*UINT64_C(1099511628211);
    }
    CHECK(hash==UINT64_C(0x6a5c6a19eea4c46e));
    return true;
}

struct TrackObservation { BgmVoice voice;unsigned pedals=0,notes=0,callbacks=0;bool held=false,released=false; };
static TrackObservation *current_observation=nullptr;
static void observed_native(void *voice,uint8_t status,uint8_t a,uint8_t b) {
    auto *o=current_observation;
    if(!o || voice!=&o->voice)return;
    ++o->callbacks;
    if((status&0xf0)==0x90 && b)++o->notes;
    if((status&0xf0)==0xb0 && a==64) {++o->pedals;if(b>=64)o->held=true;else o->released=true;}
}
static std::vector<uint8_t> driver_bytes(const AilDriver &driver) {
    const auto *begin=reinterpret_cast<const uint8_t*>(&driver);return {begin,begin+sizeof(driver)};
}
/* SBPRO2.MDI文件偏移3bc0..3c4d未列CC120，直接3bac返回；不是GM hard-off。 */
static bool ignored_controller120() {
    AilDriver voice;CHECK(setup(voice));
    driver_note_on(&voice,0,60,100);driver_note_on(&voice,1,61,100);
    driver_controller(&voice,0,64,127);driver_note_off(&voice,0,60);
    driver_note_on(&voice,0,62,100);
    CHECK(active(voice,0)==2 && active(voice,1)==1 && find_note(voice,0,60)->deferred);
    auto before=driver_bytes(voice);
    driver_controller(&voice,0,120,0);CHECK(driver_bytes(voice)==before);
    driver_controller(&voice,1,120,127);CHECK(driver_bytes(voice)==before);
    driver_controller(&voice,2,120,64);CHECK(driver_bytes(voice)==before);
    driver_controller(&voice,0,64,0);
    CHECK(!find_note(voice,0,60) && find_note(voice,0,62) && find_note(voice,1,61));
    return true;
}

static bool actual_four_tracks() {
    fd2_archive archive{};CHECK(fd2_archive_open(&archive,"original_game/FDMUS.DAT")==0);
    AilDriver driver;CHECK(load_ail_bank("original_game/SAMPLE.AD",driver.instruments));reset_driver(&driver);
    struct Track {size_t index;unsigned notes,pedals;};const Track tracks[]={{18,2256,0},{11,513,66},{19,329,0},{12,572,0}};
    for(const Track &track:tracks) {
        TrackObservation o;o.voice.driver=&driver;prepare_sequence(&o.voice,track.index!=16 && track.index!=17);
        const uint8_t *data=nullptr;size_t size=0;CHECK(fd2_archive_get(&archive,track.index,&data,&size)==0);
        NativeSequence prepared;auto before=driver_bytes(driver);current_observation=&o;
        CHECK(prepare_native_sequence(prepared,data,size));
        CHECK(o.callbacks==0 && driver_bytes(driver)==before);
        o.voice.sequence=prepared;o.voice.sequence.loop_remaining=1;
        /* 正式446bc..446f2：旧包preflush先于503；同driver cache跨曲保留。 */
        flush_driver_messages(&driver);
        if(prepared.install_timbres)CHECK(install_sequence_timbres(&driver,prepared));
        CHECK(begin_sequence(&o.voice));int inherited_rr=driver.last_free_channel;
        for(unsigned i=0;i<60000 && o.voice.sequence.status==4;++i)native_sequence_tick(&o.voice);
        CHECK(o.voice.sequence.status==2 && !o.voice.sequence.queue_overflow);
        std::printf("track %zu notes=%u pedal=%u inherited_RR=%d final_RR=%d ended=1\n",track.index,o.notes,o.pedals,inherited_rr,driver.last_free_channel);
        CHECK(o.notes==track.notes && o.pedals==track.pedals);
        if(track.index==11)CHECK(o.held && o.released);
        end_sequence(&o.voice);CHECK(!driver.active_sequence);current_observation=nullptr;
    }
    fd2_archive_close(&archive);return true;
}

static bool sequence_cache_and_retirement() {
    AilDriver driver;CHECK(setup(driver));BgmVoice first,second;
    first.driver=&driver;prepare_sequence(&first);CHECK(begin_sequence(&first));
    seq_patch(&first,0,5);seq_controller(&first,0,7,77);seq_controller(&first,0,10,100);seq_controller(&first,0,6,12);
    seq_controller(&first,0,64,127);seq_controller(&first,0,112,64);seq_note_on(&first,0,60,100,1000000);seq_note_off(&first,0,60);
    flush_driver_messages(&driver); /* 原42270：构造给定已提交before，不把私有seq_*当独立硬件API。 */
    CHECK(active(driver)==1 && driver.voices[0].deferred);int rr=driver.last_free_channel;
    second.driver=&driver;auto before=driver_bytes(driver);prepare_sequence(&second);CHECK(!begin_sequence(&second));CHECK(driver_bytes(driver)==before);
    bgm_retire(&first,FD2_AUDIO_RETIRE_REPLACED);
    CHECK(!driver.active_sequence && active(driver)==1 && driver.voices[0].deferred && driver.queued_bytes==6);
    mdi_timer_service(&driver); /* stop后原下一service才提交缓存的两个reset。 */
    CHECK(active(driver)==0 && driver.last_free_channel==rr);
    CHECK(driver.pedals[0]==0 && driver.priorities[0]==0 && driver.volumes[0]==77 && driver.pans[0]==100 && driver.pitch_ranges[0]==12 && driver.patches[0]==5);
    CHECK(begin_sequence(&second));seq_note_on(&second,0,61,100,1000000);flush_driver_messages(&driver);
    CHECK(find_note(driver,0,61)->opl_channel==(rr+1)%18);
    /* 旧 source 迟到的 retire 不能清理已激活的新 source。 */
    before=driver_bytes(driver);bgm_retire(&first,FD2_AUDIO_RETIRE_DESTROYED);CHECK(driver_bytes(driver)==before);
    /* 原 stop 按本 seq 缓存，而非按全 driver channel 值扫清。 */
    driver_controller(&driver,1,64,127);driver_controller(&driver,1,112,80);
    bgm_retire(&second,FD2_AUDIO_RETIRE_STOPPED);CHECK(driver.pedals[1]==127 && driver.priorities[1]==80);
    const fd2_audio_retire_reason reasons[]={FD2_AUDIO_RETIRE_FINISHED,FD2_AUDIO_RETIRE_STOPPED,FD2_AUDIO_RETIRE_REPLACED,FD2_AUDIO_RETIRE_STOLEN,FD2_AUDIO_RETIRE_DROPPED,FD2_AUDIO_RETIRE_DESTROYED};
    for(auto reason:reasons){BgmVoice voice;voice.driver=&driver;prepare_sequence(&voice);CHECK(begin_sequence(&voice));
        seq_controller(&voice,0,64,127);seq_controller(&voice,0,112,127);seq_note_on(&voice,0,64,100,1000000);
        flush_driver_messages(&driver);
        bgm_retire(&voice,reason);CHECK(!driver.active_sequence && driver.queued_bytes==6);
        mdi_timer_service(&driver);CHECK(!find_note(driver,0,64) && !driver.pedals[0] && !driver.priorities[0]);}
    return true;
}

struct PlayerFixture {
    fd2_audio *audio=nullptr;fd2_bgm_player *player=nullptr;
    ~PlayerFixture(){(void)fd2_bios_clock_close();fd2_audio_destroy(audio);fd2_bgm_destroy(player);}
    bool open(){fd2_audio_config ac{};ac.sample_rate=48000;ac.allow_null=1;audio=fd2_audio_create(&ac);if(fd2_test_audio_clock_prepare(audio)!=0)return false;
        fd2_bgm_config bc{};bc.fdmus_path="original_game/FDMUS.DAT";bc.ail_bank_path="original_game/SAMPLE.AD";bc.sample_rate=48000;
        player=fd2_bgm_create(audio,&bc);return player && fd2_test_audio_clock_after_mdi(audio)==0;}
};

static bool audio_handoff_and_invalid_submission() {
    PlayerFixture f;CHECK(f.open());CHECK(fd2_bgm_play(f.player,18,0)==0);
    /* begin同步：sample0之前已有唯一sequence/0->127的原渐变，不等未来render。 */
    BgmVoice *first=f.player->driver.active_sequence;
    CHECK(first && fd2_bgm_current_track(f.player)==18 && first->sequence.status==4 && first->timer_ticks==0);
    CHECK(first->sequence_volume==0 && first->target_volume==127);
    float samples[1024];CHECK(fd2_audio_render_offline(f.audio,samples,512)==512);
    auto before=driver_bytes(f.player->driver);size_t position=first->sequence.cursor;
    uint32_t tick=first->timer_ticks;CHECK(fd2_bgm_play(f.player,18,0)==0);
    CHECK(driver_bytes(f.player->driver)==before && first->sequence.cursor==position && first->timer_ticks==tick);
    /* 已无64项命令队列；200个同刻合法控制直接成功，不借废队列造假故障。 */
    for(unsigned n=0;n<200;n++)CHECK(fd2_audio_set_master_gain(f.audio,1.0f)==0);
    CHECK(driver_bytes(f.player->driver)==before && first->timer_ticks==tick);
    CHECK(fd2_bgm_play(f.player,999,0)==-1 && fd2_bgm_play(f.player,11,-1)==-1);
    CHECK(driver_bytes(f.player->driver)==before && fd2_bgm_current_track(f.player)==18);
    CHECK(fd2_audio_render_offline(f.audio,samples,512)==512);
    /* 仅损坏本测试player拥有的RAM；文件/原资源只读。独立EVNT gate拒绝。 */
    const uint8_t *entry=nullptr;size_t size=0;CHECK(fd2_archive_get(&f.player->fdmus,11,&entry,&size)==0 && size>=16);
    auto *writable=const_cast<uint8_t*>(entry);std::vector<uint8_t> original(entry,entry+size);
    std::memset(writable,0,size);std::memcpy(writable,"FORM",4);std::memcpy(writable+8,"XMID",4);
    before=driver_bytes(f.player->driver);int failed=fd2_bgm_play(f.player,11,0);std::copy(original.begin(),original.end(),writable);
    CHECK(failed==-1 && driver_bytes(f.player->driver)==before && fd2_bgm_current_track(f.player)==18);
    /* 1024帧=21.333ms；本夹具MDI只在IRQ2/4服务两次，原fade得1。 */
    CHECK(first->timer_ticks==2 && first->sequence_volume==1);
    const uint8_t before_volume14=f.player->driver.volumes[14];
    CHECK(f.player->driver.queued_bytes==0);
    seq_controller(first,14,7,77);seq_controller(first,14,10,100);seq_controller(first,14,6,12);
    const std::array<uint8_t,9> pending14={0xbe,7,0,0xbe,10,100,0xbe,6,12};
    CHECK(first->volume_cache[14]==77 && f.player->driver.volumes[14]==before_volume14);
    CHECK(f.player->driver.queued_bytes==pending14.size() &&
          std::equal(pending14.begin(),pending14.end(),f.player->driver.message_bytes.begin()));
    seq_controller(first,14,64,127);seq_controller(first,14,112,64);seq_patch(first,14,0);seq_note_on(first,14,100,100,1000000);
    CHECK(fd2_bgm_play(f.player,11,0)==0);
    BgmVoice *second=f.player->driver.active_sequence;
    CHECK(second && second==first && fd2_bgm_current_track(f.player)==11 && second->sequence.status==4 && second->timer_ticks==0);
    CHECK(f.player->driver.pedals[14]==0 && f.player->driver.priorities[14]==0 && !find_note(f.player->driver,14,100));
    CHECK(f.player->driver.volumes[14]==0 && f.player->driver.pans[14]==100 && f.player->driver.pitch_ranges[14]==12);
    CHECK(fd2_audio_render_offline(f.audio,samples,512)==512);
    CHECK(fd2_bios_clock_close()==0);
    fd2_audio_destroy(f.audio);f.audio=nullptr;CHECK(!f.player->driver.active_sequence);
    return true;
}
static bool closed_owner_rejects_without_driver_writes() {
    PlayerFixture f;CHECK(f.open());CHECK(fd2_bgm_play(f.player,18,0)==0);
    float samples[1024];CHECK(fd2_audio_render_offline(f.audio,samples,512)==512);
    BgmVoice *voice=f.player->driver.active_sequence;CHECK(voice);
    const auto before=driver_bytes(f.player->driver);
    uint32_t tick=voice->timer_ticks;size_t cursor=voice->sequence.cursor;
    int volume=voice->sequence_volume,target=voice->target_volume;
    CHECK(fd2_bios_clock_close()==0);
    /* 源同cache早返；合法不同请求先25997/cache，再遇host负错。 */
    CHECK(fd2_bgm_play(f.player,18,0)==0 && fd2_bgm_current_track(f.player)==18);
    CHECK(fd2_bgm_play(f.player,11,0)==-1 && fd2_bgm_current_track(f.player)==11);
    CHECK(fd2_bgm_play(f.player,11,0)==0); /* 此次只到原cache早返。 */
    CHECK(fd2_bgm_stop(f.player)==-1 && fd2_bgm_current_track(f.player)==-1);
    CHECK(fd2_bgm_set_music_enabled(f.player,0,1)==-1);
    CHECK(driver_bytes(f.player->driver)==before);
    CHECK(voice==f.player->driver.active_sequence && voice->timer_ticks==tick && voice->sequence.cursor==cursor);
    CHECK(voice->sequence_volume==volume && voice->target_volume==target && f.player->music_enabled);
    return true;
}

/* 生命周期证据，不验证Nuked音色或原PIC：固定host的OPL channel独立
 * 于AIL status；没有sequence和新曲start之前也不能被Audio误退役。 */
static bool chip_outlives_sequence() {
    PlayerFixture f;CHECK(f.open());
    CHECK(f.player->driver.source_live && !f.player->driver.active_sequence);
    float samples[192];
    uint64_t before=f.player->driver.opl.writebuf_samplecnt;
    CHECK(fd2_audio_render_offline(f.audio,samples,96)==96);
    CHECK(f.player->driver.opl.writebuf_samplecnt>before && f.player->driver.source_live);
    CHECK(fd2_bgm_play(f.player,18,0)==0);
    BgmVoice *voice=f.player->driver.active_sequence;CHECK(voice);
    end_sequence(voice);
    CHECK(!f.player->driver.active_sequence && f.player->driver.source_live);
    before=f.player->driver.opl.writebuf_samplecnt;
    CHECK(fd2_audio_render_offline(f.audio,samples,96)==96);
    CHECK(f.player->driver.opl.writebuf_samplecnt>before);
    /* 明确给定pre-start窗口；同chip继续，不解释任何新event。 */
    voice->sequence.status=2;
    before=f.player->driver.opl.writebuf_samplecnt;
    CHECK(bgm_render(&f.player->driver,samples,96)==96);
    CHECK(f.player->driver.opl.writebuf_samplecnt>before);
    /* 公开Audio接口移除chip后，BGM不得把无renderer的新请求报成功。 */
    CHECK(fd2_audio_stop_music(f.audio)==0 && !f.player->driver.source_live);
    CHECK(fd2_bgm_play(f.player,11,0)==-1 && fd2_bgm_current_track(f.player)==18);
    CHECK(fd2_bios_clock_close()==0);
    fd2_audio_destroy(f.audio);f.audio=nullptr;
    CHECK(!f.player->driver.source_live && !f.player->driver.active_sequence);
    CHECK(bgm_render(&f.player->driver,samples,96)==0);
    return true;
}

static size_t occupied_sfx_render(void *,float *out,size_t frames) {
    std::fill(out,out+frames*2u,0.0f);return frames;
}
static void occupied_sfx_retire(void *user,fd2_audio_retire_reason reason) {
    if(reason==FD2_AUDIO_RETIRE_STOLEN)++*static_cast<unsigned *>(user);
}
static bool create_rejects_synchronously_dropped_chip() {
    fd2_audio_config ac{};ac.sample_rate=48000;
    fd2_audio *audio=fd2_audio_create(&ac);CHECK(audio);
    CHECK(fd2_test_audio_clock_prepare(audio)==0);
    /* generic诊断profile可以占满槽；不按private容量写expected。
     * 同bus的旧SFX被轮替，最终无MUSIC可取代且没有空槽。 */
    unsigned stolen=0;
    for(unsigned i=0;i<64 && !stolen;i++)
        CHECK(fd2_audio_play_source(audio,FD2_AUDIO_BUS_SFX,
            fd2_audio_source{occupied_sfx_render,occupied_sfx_retire,&stolen},1.0f)==0);
    CHECK(stolen>0);
    fd2_bgm_config bc{};bc.fdmus_path="original_game/FDMUS.DAT";
    bc.ail_bank_path="original_game/SAMPLE.AD";bc.sample_rate=48000;
    fd2_bgm_player *player=fd2_bgm_create(audio,&bc);
    CHECK(!player);
    CHECK(fd2_bios_clock_close()==0);fd2_audio_destroy(audio);
    return true;
}

static bool default_capacity_playback() {
    fd2_audio_config ac{};ac.sample_rate=48000;ac.allow_null=1;
    fd2_audio *audio=fd2_audio_create(&ac);CHECK(audio);
    CHECK(fd2_test_audio_clock_prepare(audio)==0);
    fd2_bgm_config bc{};bc.fdmus_path="original_game/FDMUS.DAT";bc.ail_bank_path="original_game/SAMPLE.AD";bc.sample_rate=48000;bc.voice_count=0;
    fd2_bgm_player *player=fd2_bgm_create(audio,&bc);CHECK(player);
    CHECK(fd2_test_audio_clock_after_mdi(audio)==0);
    std::vector<float> samples(96000);
    auto render_second=[&](){double e=0;for(size_t left=48000;left;){size_t n=std::min<size_t>(left,512);if(fd2_audio_render_offline(audio,samples.data(),n)!=n)return -1.0;for(size_t i=0;i<n*2;i++)e+=std::abs(samples[i]);left-=n;}return e;};
    CHECK(fd2_bgm_play(player,18,0)==0);
    for(int i=0;i<3;i++)CHECK(render_second()>=0);
    CHECK(fd2_bgm_play(player,18,0)==0);CHECK(render_second()>=0);
    CHECK(fd2_bgm_stop(player)==0);CHECK(render_second()>=0);

    CHECK(fd2_bgm_play(player,11,0)==0);double energy=render_second();
    bool audible=energy>10.0;
    std::printf("default18 replacement first-second energy=%g\n",energy);
    CHECK(fd2_bios_clock_close()==0);
    fd2_audio_destroy(audio);fd2_bgm_destroy(player);CHECK(audible);return true;
}

#include "bgm_instrument_cache_checks.h"
#include "bgm_message_queue_checks.h"
#include "bgm_single_sentinel_checks.h"
#include "bgm_startup_owner_checks.h"

int main(int argc,char **argv){
    if(argc==2 && !std::strcmp(argv[1],"--startup-owner")) {
        if(!startup_owner_checks())return 1;
        std::puts("bgm startup: pre-writer render, failed-install release, duplicate-role rejection PASS");return 0;
    }
    if(argc==2 && !std::strcmp(argv[1],"--sentinel-prefix")) {
        if(!single_sentinel_checks())return 1;
        std::puts("bgm empty resources: previous-score replay, idle/recovery and error prefixes PASS; no GUI/device");return 0;
    }
    if(argc!=1)return 2;
    if(!instrument_cache_checks())return 1;
    if(!startup_owner_checks())return 1;
    if(!single_sentinel_checks())return 1;
    if(!source_message_queue_checks())return 1;
    if(!pan_regression() || !pedal_regression() || !fair_regression())return 1;
    if(!allocator_vectors() || !sustain_reset_and_reuse() || !fixed_note_reentry() ||
       !selective_controls() || !ignored_controller120() || !pitch_static_vectors() || !actual_four_tracks() || !sequence_cache_and_retirement() ||
       !audio_handoff_and_invalid_submission() || !closed_owner_rejects_without_driver_writes() || !chip_outlives_sequence() ||
       !create_rejects_synchronously_dropped_chip() || !default_capacity_playback())return 1;
    std::puts("bgm_sbpro2_test: driver/cache/queue/replay/owner regressions passed (no device/capture)");return 0;
}
