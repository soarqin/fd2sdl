/* 原422c0/42270/43270/42dd0/447d0完整指令的字面packet与提交前缀。
 * 期望来自source，不从当前C反生；不签CPU/PIC/端口或自然声音。
 * 本header只在现SBPRO2私有TU内消费，不加shipping API/owner。 */
struct SourceMessagePacket {
    uint32_t count;
    std::vector<uint8_t> bytes;
};
static AilDriver *packet_driver=nullptr;
static std::vector<SourceMessagePacket> message_packets;
static bool packet_observation_invalid=false;
static void observed_message_batch(void *who,const uint8_t *bytes,uint32_t used,uint32_t count) {
    if(who!=packet_driver)return;
    if(!bytes || used>512 || !count || count>256) {packet_observation_invalid=true;return;}
    message_packets.push_back({count,{bytes,bytes+used}});
}
static void source_begin_packets(AilDriver &driver) {
    packet_driver=&driver;message_packets.clear();packet_observation_invalid=false;
}
static void source_end_packets(){packet_driver=nullptr;}

static bool source_initializer_sixteen_packets() {
    /* 原首次controls在503前，冷bank已读但物理cache尚空。 */
    AilDriver driver;CHECK(load_ail_bank("original_game/SAMPLE.AD",driver.instruments));
    reset_driver(&driver);source_begin_packets(driver);
    initialize_driver_controls(&driver,127,2);
    CHECK(message_packets.size()==16 && !packet_observation_invalid);
    for(uint8_t ch=0;ch<16;ch++) {
        const uint8_t b=static_cast<uint8_t>(0xb0u|ch);
        const std::vector<uint8_t> expected={b,114,0,static_cast<uint8_t>(0xc0u|ch),0,
            static_cast<uint8_t>(0xe0u|ch),0,64,b,112,0,b,1,0,b,7,127,b,10,64,b,11,127,
            b,64,0,b,91,40,b,93,0,b,100,0,b,101,0,b,38,0,b,6,2};
        CHECK(expected.size()==44 && message_packets[ch].count==15 && message_packets[ch].bytes==expected);
        CHECK(driver.patches[ch]==0 && driver.selected_instruments[ch]==-1 && driver.banks[ch]==0);
        CHECK(driver.volumes[ch]==127 && driver.expressions[ch]==127 && driver.pans[ch]==64 && driver.pitch_ranges[ch]==2);
        CHECK(driver.bends[ch]==8192 && !driver.pedals[ch] && !driver.priorities[ch] && !driver.modulations[ch]);
    }
    CHECK(!driver.queued_messages && !driver.queued_bytes && active(driver)==0);
    source_end_packets();return true;
}
static bool source_queue_literal_commit() {
    AilDriver driver;CHECK(setup(driver));BgmVoice voice;voice.driver=&driver;
    prepare_sequence(&voice);CHECK(begin_sequence(&voice));source_begin_packets(driver);
    seq_patch(&voice,0,0);seq_controller(&voice,0,7,100);seq_note_on(&voice,0,60,100,1000);
    const std::vector<uint8_t> expected={0xc0,0,0xb0,7,100,0x90,60,100};
    CHECK(driver.queued_messages==3 && driver.queued_bytes==8 && message_packets.empty());
    CHECK(driver.volumes[0]==127 && active(driver)==0); /* 原硬件尚未收到消息。 */
    CHECK(std::equal(expected.begin(),expected.end(),driver.message_bytes.begin()));
    flush_driver_messages(&driver);
    CHECK(!packet_observation_invalid && message_packets.size()==1);
    CHECK(message_packets[0].count==3 && message_packets[0].bytes==expected);
    CHECK(driver.queued_messages==0 && driver.queued_bytes==0 && driver.volumes[0]==100 && active(driver)==1);
    CHECK(std::equal(expected.begin(),expected.end(),driver.message_bytes.begin())); /* flush不清旧storage。 */
    source_end_packets();end_sequence(&voice);mdi_timer_service(&driver);return true;
}
static bool source_queue_exact512() {
    AilDriver driver;CHECK(setup(driver));source_begin_packets(driver);
    for(unsigned i=0;i<256;i++)queue_driver_message(&driver,0xc0,5,0);
    CHECK(driver.queued_bytes==512 && driver.queued_messages==256 && message_packets.empty());
    CHECK(driver.patches[0]==0); /* >512才flush，==512不提交。 */
    queue_driver_message(&driver,0xb0,7,77);
    CHECK(message_packets.size()==1 && message_packets[0].count==256 && message_packets[0].bytes.size()==512);
    for(unsigned i=0;i<256;i++)CHECK(message_packets[0].bytes[i*2]==0xc0 && message_packets[0].bytes[i*2+1]==5);
    CHECK(driver.patches[0]==5 && driver.volumes[0]==127 && driver.queued_bytes==3 && driver.queued_messages==1);
    CHECK(driver.message_bytes[511]==5); /* 新三字节只盖前缀。 */
    flush_driver_messages(&driver);
    CHECK(!packet_observation_invalid && message_packets.size()==2);
    CHECK(message_packets[1].count==1 && message_packets[1].bytes==std::vector<uint8_t>({0xb0,7,77}));
    CHECK(driver.volumes[0]==77 && driver.message_bytes[511]==5);
    flush_driver_messages(&driver);CHECK(message_packets.size()==2);source_end_packets();return true;
}
static bool source_queue_mixed_fit() {
    AilDriver driver;CHECK(setup(driver));source_begin_packets(driver);
    for(unsigned i=0;i<170;i++)queue_driver_message(&driver,0xb0,7,66);
    CHECK(driver.queued_bytes==510 && driver.volumes[0]==127 && message_packets.empty());
    queue_driver_message(&driver,0xc0,4,0);
    CHECK(driver.queued_bytes==512 && driver.queued_messages==171 && message_packets.empty());
    queue_driver_message(&driver,0xb0,7,99);
    CHECK(message_packets.size()==1 && message_packets[0].count==171 && message_packets[0].bytes.size()==512);
    for(unsigned i=0;i<170;i++)CHECK(message_packets[0].bytes[i*3]==0xb0 && message_packets[0].bytes[i*3+1]==7 && message_packets[0].bytes[i*3+2]==66);
    CHECK(message_packets[0].bytes[510]==0xc0 && message_packets[0].bytes[511]==4);
    CHECK(driver.volumes[0]==66 && driver.patches[0]==4 && driver.queued_messages==1);
    flush_driver_messages(&driver);CHECK(driver.volumes[0]==99 && message_packets.size()==2);
    CHECK(!packet_observation_invalid);source_end_packets();return true;
}
static bool source_stop_two_phase_queue() {
    AilDriver driver;CHECK(setup(driver));BgmVoice voice;voice.driver=&driver;
    prepare_sequence(&voice);CHECK(begin_sequence(&voice));
    seq_controller(&voice,0,64,127);seq_controller(&voice,0,112,64);seq_note_on(&voice,0,60,100,1000);
    flush_driver_messages(&driver);CHECK(active(driver)==1 && driver.pedals[0]==127);
    source_begin_packets(driver);post_driver_message(&voice,0xb0,7,80);
    stop_sequence_events(&voice);
    /* 42dd0先提交此前queued CC7和32槽note-off；pedal仍127，物理voice延留。 */
    CHECK(message_packets.size()==1 && message_packets[0].count==2);
    CHECK(message_packets[0].bytes==std::vector<uint8_t>({0xb0,7,80,0x80,60,0}));
    CHECK(driver.volumes[0]==80 && active(driver)==1 && driver.voices[0].deferred);
    CHECK(driver.pedals[0]==127 && driver.priorities[0]==64 && voice.sequence.status==8);
    const std::vector<uint8_t> reset={0xb0,64,0,0xb0,112,0};
    CHECK(driver.queued_bytes==6 && driver.queued_messages==2);
    CHECK(std::equal(reset.begin(),reset.end(),driver.message_bytes.begin()));
    end_sequence(&voice);CHECK(!driver.active_sequence && message_packets.size()==1 && active(driver)==1);
    mdi_timer_service(&driver); /* 无active sequence仍经43270共同tail。 */
    CHECK(message_packets.size()==2 && message_packets[1].count==2 && message_packets[1].bytes==reset);
    CHECK(active(driver)==0 && !driver.pedals[0] && !driver.priorities[0] && !driver.queued_messages);
    CHECK(!packet_observation_invalid);source_end_packets();return true;
}
static bool source_service_tail_and_early_return() {
    AilDriver driver;CHECK(setup(driver));BgmVoice voice;voice.driver=&driver;prepare_sequence(&voice);
    const uint8_t evnt[]={0xc0,0,0xb0,7,90,0x90,60,100,1,1,0xff,0x2f,0};
    voice.sequence.evnt=evnt;voice.sequence.size=sizeof(evnt);voice.sequence.loop_remaining=1;
    CHECK(begin_sequence(&voice));source_begin_packets(driver);
    native_sequence_tick(&voice);
    CHECK(message_packets.size()==1 && message_packets[0].count==3);
    CHECK(message_packets[0].bytes==std::vector<uint8_t>({0xc0,0,0xb0,7,90,0x90,60,100}));
    CHECK(driver.volumes[0]==90 && active(driver)==1 && !driver.queued_messages);
    native_sequence_tick(&voice);
    CHECK(voice.sequence.status==2 && message_packets.size()==2 && active(driver)==0);
    CHECK(message_packets[1].bytes==std::vector<uint8_t>({0x80,60,0}));
    post_driver_message(&voice,0xb0,7,77);
    CHECK(driver.volumes[0]==90 && driver.queued_messages==1);
    native_sequence_tick(&voice); /* status2不解析EVNT，但仍共同flush。 */
    CHECK(driver.volumes[0]==77 && message_packets.size()==3 && !driver.queued_messages);
    CHECK(!packet_observation_invalid);source_end_packets();end_sequence(&voice);return true;
}
static bool source_preflight_and_foreign_begin_preserve_pending() {
    AilDriver driver;CHECK(setup(driver));BgmVoice first,foreign;first.driver=&driver;foreign.driver=&driver;
    prepare_sequence(&first);prepare_sequence(&foreign);CHECK(begin_sequence(&first));
    post_driver_message(&first,0xb0,7,88);const auto before=driver_bytes(driver);
    const uint8_t bad[]={0xff};NativeSequence prepared;
    CHECK(!prepare_native_sequence(prepared,bad,sizeof(bad)) && driver_bytes(driver)==before);
    CHECK(!begin_sequence(&foreign) && driver_bytes(driver)==before);
    /* 这两条是宿主预检/owner拒绝，不声称原坏资源执行也原子。 */
    flush_driver_messages(&driver);CHECK(driver.volumes[0]==88);end_sequence(&first);return true;
}
static bool source_message_queue_checks() {
    return source_initializer_sixteen_packets() && source_queue_literal_commit() && source_queue_exact512() && source_queue_mixed_fit() &&
        source_stop_two_phase_queue() && source_service_tail_and_early_return() &&
        source_preflight_and_foreign_begin_preserve_pending();
}
