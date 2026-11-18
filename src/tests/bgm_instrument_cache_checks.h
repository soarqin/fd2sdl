/* SBPRO2.MDI普通乐器缓存回归：2ca6/2f15/3a2f/3bd8。
 * 从旧私有cache测试保留三族资源/声音行为期望；不收费、不运行DOS运输。
 * 原SAMPLE.AD/FDMUS只读，不分发音色或派生媒体。 */
#ifndef FD2_BGM_INSTRUMENT_CACHE_CHECKS_H
#define FD2_BGM_INSTRUMENT_CACHE_CHECKS_H
static bool instrument_cache_fresh(AilDriver &driver) {
    CHECK(load_ail_bank("original_game/SAMPLE.AD",driver.instruments));
    reset_driver(&driver);
    CHECK(driver.instrument_clock==0 && driver.instrument_bytes==0);
    for(int i:driver.selected_instruments)CHECK(i==-1);
    for(int i:driver.drum_instruments)CHECK(i==-1);
    return true;
}
static bool instrument_cache_install(AilDriver &driver,fd2_archive &archive,size_t track) {
    const uint8_t *bytes=nullptr;size_t size=0;
    CHECK(fd2_archive_get(&archive,track,&bytes,&size)==0);
    NativeSequence parsed;CHECK(prepare_native_sequence(parsed,bytes,size));
    CHECK(install_sequence_timbres(&driver,parsed));return true;
}
static bool actual_T18_T19_registration_and_cache_references(void) {
    AilDriver driver;CHECK(instrument_cache_fresh(driver));
    driver_patch(&driver,0,50);
    CHECK(driver.patches[0]==50 && driver.selected_instruments[0]==-1);
    struct ArchiveOwner {fd2_archive value{};~ArchiveOwner(){fd2_archive_close(&value);}} archive_owner;
    fd2_archive &archive=archive_owner.value;
    CHECK(fd2_archive_open(&archive,"original_game/FDMUS.DAT")==0);
    CHECK(instrument_cache_install(driver,archive,18));
    /* 原资源TIMB18给的原顺序，不读取被测cache造expected。 */
    const uint8_t patch[]={92,50,52,47,88,95,117,38,49};
    const uint8_t bank[]={0,0,0,0,0,0,0,127,127};
    CHECK(driver.instrument_clock==9 && driver.instrument_bytes==126);
    for(size_t i=0;i<9;i++) {
        const auto &slot=driver.instrument_cache[i];
        CHECK(slot.patch==patch[i] && slot.bank==bank[i] && slot.flags==0x80);
        CHECK(slot.offset==i*14 && slot.touched==i);
    }
    CHECK(driver.selected_instruments[0]==1);
    driver_note_on(&driver,0,60,100);
    CHECK(driver.voices[0].active && driver.voices[0].instrument==1);
    CHECK(driver.instrument_clock==10 && driver.instrument_cache[1].touched==10);
    CHECK(driver.opl.channel[0].slotz[0]->key && driver.opl.channel[0].slotz[1]->key);
    driver_note_on(&driver,9,38,100);
    CHECK(driver.drum_instruments[38]==7 && driver.voices[1].instrument==7);
    CHECK(driver.instrument_clock==11 && driver.instrument_cache[7].touched==11);
    driver_controller(&driver,0,113,127);CHECK(driver.instrument_cache[1].flags==0xc0);
    driver_controller(&driver,0,113,0);CHECK(driver.instrument_cache[1].flags==0x80);
    const Voice before=driver.voices[0];
    CHECK(instrument_cache_install(driver,archive,19));
    /* TIMB19 [90,92,22,88,36/127,38/127]：只有90/22/36是新项。 */
    CHECK(driver.instrument_clock==14 && driver.instrument_bytes==168);
    CHECK(driver.instrument_cache[9].patch==90 && driver.instrument_cache[9].touched==11);
    CHECK(driver.instrument_cache[10].patch==22 && driver.instrument_cache[10].touched==12);
    CHECK(driver.instrument_cache[11].patch==36 && driver.instrument_cache[11].bank==127 && driver.instrument_cache[11].touched==13);
    CHECK(!std::memcmp(&before,&driver.voices[0],sizeof(before)) && driver.selected_instruments[0]==1);
    CHECK(instrument_cache_install(driver,archive,18));
    CHECK(driver.instrument_clock==14 && driver.instrument_bytes==168);
    CHECK(driver.instrument_cache[1].touched==10 && driver.drum_instruments[38]==7);
    const uint8_t old_b0=driver.voices[0].b0;
    const auto old_fnum=driver.opl.channel[0].f_num;
    driver_note_off(&driver,0,60);
    CHECK(!driver.voices[0].active && !driver.voices[0].key_bits && !driver.voices[0].dirty);
    CHECK(driver.voices[0].b0==old_b0 && driver.opl.channel[0].f_num==old_fnum);
    CHECK(!driver.opl.channel[0].slotz[0]->key && !driver.opl.channel[0].slotz[1]->key);
    CHECK(driver.instrument_clock==14); /* keyoff不touch原cache。 */
    driver_note_on(&driver,0,61,100);
    CHECK(driver.voices[0].active && driver.voices[0].instrument==1 && driver.voices[0].key_bits==0x20 && !driver.voices[0].dirty);
    CHECK(driver.instrument_clock==15 && driver.instrument_cache[1].touched==15);
    return true;
}
static bool twenty_slots_full_still_touch_and_never_alias_cold_key(void) {
    AilDriver driver;CHECK(instrument_cache_fresh(driver));
    CHECK(register_instrument(&driver,50,0));
    /* 独立原SAMPLE.AD key50的14B，不再断言旧cold-index侧引。 */
    const uint8_t payload50[]={14,0,0,177,139,113,17,0,6,97,64,66,21,1};
    CHECK(!std::memcmp(driver.instrument_cache[0].payload.data(),payload50,14));
    for(uint8_t ch=0;ch<3;ch++)driver_patch(&driver,ch,50);
    driver_controller(&driver,0,112,64);
    for(uint8_t n=40;n<58;n++)driver_note_on(&driver,0,n,100);
    driver_note_on(&driver,1,90,100);driver_note_on(&driver,2,91,100);
    for(const Voice &voice:driver.voices)CHECK(voice.active && voice.instrument==0);
    CHECK(driver.instrument_clock==21 && driver.instrument_cache[0].touched==21);
    const auto voices=driver.voices;const auto channels=driver.hardware_channels;
    driver_note_on(&driver,0,92,100);
    CHECK(driver.instrument_clock==22 && driver.instrument_cache[0].touched==22);
    CHECK(!std::memcmp(voices.data(),driver.voices.data(),sizeof(driver.voices)) && channels==driver.hardware_channels);
    return true;
}
static bool complete_fixed_bank_fits_original_owner(void) {
    AilDriver driver;CHECK(instrument_cache_fresh(driver));
    unsigned count=0;
    /* 是显式合法合成register请求，不称实际自然曲史；逐key无payload改写。 */
    for(unsigned bank:{0u,127u})for(unsigned patch=0;patch<128;patch++) {
        const int cold=instrument_index(uint8_t(patch),uint8_t(bank));
        if(cold<0 || !driver.instruments[size_t(cold)].present)continue;
        CHECK(register_instrument(&driver,uint8_t(patch),uint8_t(bank)));count++;
    }
    CHECK(count==162 && driver.instrument_bytes==2268 && driver.instrument_clock==162);
    for(unsigned i=0;i<162;i++)CHECK(driver.instrument_cache[i].flags==0x80 && driver.instrument_cache[i].offset==14u*i);
    for(unsigned i=162;i<192;i++)CHECK(driver.instrument_cache[i].flags==0);
    return true;
}

static bool instrument_cache_checks() {
    return actual_T18_T19_registration_and_cache_references() &&
        twenty_slots_full_still_touch_and_never_alias_cold_key() &&
        complete_fixed_bank_fits_original_owner();
}
#endif
