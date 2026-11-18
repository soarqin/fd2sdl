/* 25977/443d0/43160/44790/43270空条目与换曲表现回归。
 * 2026-11-18原Town G1/G2录音支持重播；首服务短指纹独立取自原FDMUS，
 * 不以C输出当期望；保真实消息、重播、淡出与错误前缀。 */
/* 原track1 EVNT首服务：17条MIDI／47B，cursor99、delay10；CC7按volume0缩放。
 * 仅保短指纹，不嵌入原曲谱／音频。pedal前缀来自原stop顺序的独立已有期望。 */
static bool replay_intro_packet(const SourceMessagePacket &packet,bool pedal_prefix) {
    const size_t skip=pedal_prefix?3u:0u;
    CHECK(packet.count==17u+(pedal_prefix?1u:0u) && packet.bytes.size()==47u+skip);
    if(pedal_prefix)CHECK(packet.bytes[0]==0xb0 && packet.bytes[1]==64 && packet.bytes[2]==0);
    uint64_t hash=UINT64_C(14695981039346656037);
    for(size_t i=skip;i<packet.bytes.size();++i)hash=(hash^packet.bytes[i])*UINT64_C(1099511628211);
    CHECK(hash==UINT64_C(0xf3666a302ea4389d));return true;
}
static bool G_replay_and_valid_recovery() {
    PlayerFixture f;CHECK(f.open());auto &s=f.player->sequence;
    CHECK(fd2_bgm_play(f.player,1,0)==0);
    const uint8_t *resource=nullptr;size_t bytes=0;
    CHECK(fd2_archive_get(&f.player->fdmus,1,&resource,&bytes)==0 && bytes==6164);
    CHECK(s.sequence.evnt==resource+80 && s.sequence.size==6084); /* independent resource-prefix01 */
    BgmVoice *same=&s;const auto *old_evnt=s.sequence.evnt;
    /* 换曲先释放旧音符和pedal，再应用重播的音量与loop。 */
    s.sequence_volume=44;s.target_volume=88;s.fade_accumulator_us=123;s.fade_interval_us=999;
    seq_controller(&s,0,7,100);seq_controller(&s,0,64,127);
    seq_note_on(&s,0,60,100,1000000);seq_note_on(&s,1,61,100,3);
    flush_driver_messages(&f.player->driver);source_begin_packets(f.player->driver);
    CHECK(fd2_bgm_play(f.player,2,0)==0);
    CHECK(&f.player->sequence==same && f.player->driver.active_sequence==same && fd2_bgm_current_track(f.player)==2);
    CHECK(s.sequence_volume==0 && s.target_volume==127 && s.sequence.loop_remaining==0);
    CHECK(s.sequence.evnt==old_evnt && s.sequence.size==6084 && s.sequence.cursor==0); /* 同player/DAT中的已解码score。 */
    CHECK(s.sequence.pending_count==0 && s.sequence.pending[0].channel==-1);
    CHECK(message_packets.size()==1 && message_packets[0].count==2);
    const std::vector<uint8_t> offs={0x80,60,0,0x81,61,0};CHECK(message_packets[0].bytes==offs);
    CHECK(f.player->driver.queued_messages==1 && f.player->driver.queued_bytes==3);
    const std::vector<uint8_t> pedal_reset={0xb0,64,0};
    CHECK(std::equal(pedal_reset.begin(),pedal_reset.end(),f.player->driver.message_bytes.begin()));
    /* 首服务应从同一score开头，绝非接旧cursor。 */
    CHECK(native_sequence_tick(&s));
    CHECK(s.timer_ticks==1 && s.sequence.cursor==99 && s.sequence.delay==10 && s.sequence.pending_count==7);
    CHECK(message_packets.size()==2 && replay_intro_packet(message_packets[1],true));
    const unsigned channels[]={9,9,1,2,3,2,2},notes[]={36,42,45,69,76,74,72};
    const uint32_t remaining[]={42,10,20,327,20,327,10};
    for(unsigned i=0;i<7;++i)CHECK(s.sequence.pending[i].channel==int(channels[i]) &&
        s.sequence.pending[i].note==notes[i] && s.sequence.pending[i].remaining==remaining[i]);
    const size_t cursor=s.sequence.cursor;
    CHECK(fd2_bgm_play(f.player,2,0)==0);
    CHECK(s.timer_ticks==1 && s.sequence.cursor==cursor && !f.player->driver.queued_messages);
    CHECK(fd2_bgm_stop(f.player)==0 && fd2_bgm_current_track(f.player)==-1);
    CHECK(fd2_bgm_set_music_enabled(f.player,0,1)==0 && fd2_bgm_set_music_enabled(f.player,1,1)==0);
    CHECK(s.sequence.status==4 && s.target_volume==127);
    CHECK(fd2_bgm_play(f.player,3,0)==0);
    CHECK(fd2_archive_get(&f.player->fdmus,3,&resource,&bytes)==0 && bytes==6796);
    CHECK(&f.player->sequence==same && s.sequence.evnt==resource+82 && s.sequence.size==6714);
    CHECK(s.sequence.status==4 && s.timer_ticks==0 && s.sequence_volume==0);
    CHECK(f.player->driver.source_live);source_end_packets();return true;
}
static bool first_empty_resource_is_idle_then_valid_recovers() {
    PlayerFixture f;CHECK(f.open());auto &s=f.player->sequence;
    CHECK(!s.sequence.evnt && !s.sequence.size);
    CHECK(fd2_bgm_play(f.player,0,0)==0);
    CHECK(!s.sequence.evnt && !s.sequence.size && fd2_bgm_current_track(f.player)==0);
    CHECK(!native_sequence_tick(&s) && s.timer_ticks==0 && s.sequence.delay==0);
    CHECK(s.sequence.status==4 && f.player->driver.source_live);
    CHECK(fd2_bgm_play(f.player,1,0)==0 && s.sequence.evnt && s.sequence.size);
    CHECK(native_sequence_tick(&s) && s.sequence.cursor==99 && s.sequence.pending_count==7);
    return true;
}
static bool reset_keeps_score_and_runtime_settings() {
    AilDriver driver;BgmVoice s;s.driver=&driver;static const uint8_t opaque[4]={1,2,3,4};
    s.sequence.evnt=opaque;s.sequence.size=4;s.sequence.cursor=3;s.sequence.status=4;
    s.sequence.pending[5]={2,77,123};s.sequence.pending_count=1;s.sequence.delay=9;s.sequence.loop_remaining=7;
    s.sequence_volume=52;s.target_volume=17;s.fade_accumulator_us=23;s.fade_interval_us=41;s.timer_ticks=9;
    reset_sequence_event_state(&s);
    CHECK(s.sequence.evnt==opaque && s.sequence.size==4 && s.sequence.cursor==3 && s.sequence.status==4);
    CHECK(s.sequence.pending[5].channel==-1);
    CHECK(!s.sequence.pending_count && !s.sequence.delay && s.sequence.loop_remaining==1 && !s.timer_ticks);
    CHECK(s.sequence_volume==52 && s.target_volume==17 && s.fade_accumulator_us==23 && s.fade_interval_us==41);
    for(size_t i=0;i<16;i++)CHECK(s.channel_map[i]==i && s.pedal_cache[i]==-1 && s.priority_cache[i]==-1 && s.volume_cache[i]==-1);
    return true;
}
static bool bad_arguments_resource_and_host_keep_real_prefixes() {
    PlayerFixture f;CHECK(f.open());CHECK(fd2_bgm_play(f.player,1,0)==0);
    auto before=driver_bytes(f.player->driver);auto &s=f.player->sequence;const auto *evnt=s.sequence.evnt;
    CHECK(fd2_bgm_play(f.player,2,-1)==-1 && fd2_bgm_play(f.player,999,0)==-1);
    CHECK(fd2_bgm_current_track(f.player)==1 && driver_bytes(f.player->driver)==before);
    const uint8_t *data=nullptr;size_t size=0;CHECK(fd2_archive_get(&f.player->fdmus,2,&data,&size)==0 && size==3);
    auto *owned=const_cast<uint8_t *>(data);uint8_t saved=owned[0];owned[0]=0x21;
    int bad=fd2_bgm_play(f.player,2,0);owned[0]=saved;
    CHECK(bad==-1 && fd2_bgm_current_track(f.player)==1 && s.sequence.evnt==evnt);
    CHECK(fd2_bios_clock_close()==0);
    CHECK(fd2_bgm_play(f.player,3,0)==-1);
    CHECK(fd2_bgm_current_track(f.player)==3 && driver_bytes(f.player->driver)==before && s.sequence.evnt==evnt);
    CHECK(fd2_bgm_play(f.player,3,0)==0);
    CHECK(fd2_bgm_stop(f.player)==-1 && fd2_bgm_current_track(f.player)==-1);
    CHECK(fd2_bgm_set_music_enabled(f.player,0,1)==-1 && f.player->music_enabled);
    return true;
}
static bool first_empty_family_keeps_live_chip() {
    const unsigned tracks[]={0,2,5,7,9}; /* independent resource-prefix01, NOT production dispatch */
    for(unsigned track:tracks) {
        PlayerFixture f;CHECK(f.open());CHECK(fd2_bgm_play(f.player,track,0)==0);
        auto &s=f.player->sequence;auto before=driver_bytes(f.player->driver);
        CHECK(fd2_bgm_current_track(f.player)==static_cast<int>(track));
        CHECK(!s.sequence.evnt && !s.sequence.size && !native_sequence_tick(&s));
        CHECK(driver_bytes(f.player->driver)==before);
        uint64_t samples=f.player->driver.opl.writebuf_samplecnt;float out[192];
        CHECK(fd2_audio_render_offline(f.audio,out,96)==96 && f.player->driver.opl.writebuf_samplecnt>samples);
        CHECK(f.player->driver.source_live && !s.sequence.evnt && !s.sequence.size);
        fd2_audio_error error;CHECK(fd2_audio_get_first_error(f.audio,&error)==0 && error==FD2_AUDIO_ERROR_NONE);
        CHECK(fd2_bios_clock_close()==0);fd2_audio_destroy(f.audio);f.audio=nullptr;
        CHECK(!f.player->driver.source_live);
        CHECK(fd2_bgm_play(f.player,1,0)==-1); /* true closed binding, no unsafe reinit */
    }return true;
}
static bool empty_resource_family_replays_one_score() {
    const unsigned tracks[]={0,2,5,7,9}; /* 原资源五个相同3B，不是产品逐号分支。 */
    for(unsigned track:tracks) {
        PlayerFixture f;CHECK(f.open());if(track==5)CHECK(fd2_bgm_set_music_enabled(f.player,0,0)==0);
        CHECK(fd2_bgm_play(f.player,1,0)==0);auto &s=f.player->sequence;
        CHECK(native_sequence_tick(&s) && s.sequence.cursor==99);
        const auto *score=s.sequence.evnt;BgmVoice *same=&s;
        source_begin_packets(f.player->driver);
        CHECK(fd2_bgm_play(f.player,track,0)==0 && &f.player->sequence==same);
        CHECK(s.sequence.evnt==score && s.sequence.size==6084 && !s.sequence.cursor && !s.timer_ticks);
        CHECK(s.sequence.loop_remaining==0);
        CHECK(s.target_volume==(track==5?0:127));
        CHECK(message_packets.size()==1 && message_packets[0].count==7); /* 32槽内实际7音符先释放。 */
        CHECK(native_sequence_tick(&s) && s.timer_ticks==1 && s.sequence.cursor==99 && s.sequence.delay==10);
        CHECK(message_packets.size()==2 && replay_intro_packet(message_packets[1],false));
        CHECK(fd2_bgm_current_track(f.player)==int(track));
        source_end_packets();
    }return true;
}
static bool single_sentinel_checks() {
    return G_replay_and_valid_recovery() && first_empty_resource_is_idle_then_valid_recovers() &&
        reset_keeps_score_and_runtime_settings() && bad_arguments_resource_and_host_keep_real_prefixes() &&
        first_empty_family_keeps_live_chip() && empty_resource_family_replays_one_score();
}
