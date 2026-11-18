/* SBPRO2 reset完成后的chip借用必须先于首writer，原普通writer有FillUp。
 * 仅检当前owner边界和失败清理；显式离线2ms不是原首相位或真实CPU耗时。 */
static unsigned startup_mode=0,startup_seen=0;
static bool startup_invalid=false;
static void observed_startup_owner(void *who,unsigned phase) {
    if(!startup_mode)return;
    auto *driver=static_cast<AilDriver *>(who);
    if(phase==1) {
        startup_seen++;
        if(!driver->source_live || driver->timer ||
           driver->timer_handle!=FD2_AIL_TIMER_INVALID || driver->opl.newm!=0)startup_invalid=true;
        const uint64_t before=driver->opl.writebuf_samplecnt;
        float output[192];
        if(fd2_audio_render_offline(driver->audio,output,96)!=96 ||
           driver->opl.writebuf_samplecnt<=before)startup_invalid=true;
        for(float value:output)if(value!=0)startup_invalid=true;
    } else if(phase==2) {
        startup_seen++;
        if(!driver->source_live || driver->opl.newm!=1 || driver->timer ||
           driver->queued_messages || driver->queued_bytes)startup_invalid=true;
        if(startup_mode==2 && fd2_bios_clock_close()!=0)startup_invalid=true;
    } else startup_invalid=true;
}
static bool startup_owner_checks() {
    for(unsigned profile=0;profile<=1;profile++)for(unsigned mode=1;mode<=3;mode++) {
        fd2_audio_config ac{};ac.sample_rate=48000;ac.original_mixer=static_cast<int>(profile);
        fd2_audio *audio=fd2_audio_create(&ac);CHECK(audio);
        CHECK(fd2_test_audio_clock_prepare(audio)==0);
        if(mode==3) {
            /* 预占同一MDI角色，构造必须失败并撤销刚安装的Audio借用。 */
            CHECK(fd2_bios_clock_install_mdi(fd2_audio_now_ns(audio),
                fd2_test_idle_dig,nullptr)==FD2_AIL_TIMER_MDI);
        }
        fd2_bgm_config config{};config.fdmus_path="original_game/FDMUS.DAT";
        config.ail_bank_path="original_game/SAMPLE.AD";config.sample_rate=48000;
        startup_mode=mode;startup_seen=0;startup_invalid=false;
        fd2_bgm_player *player=fd2_bgm_create(audio,&config);
        startup_mode=0;CHECK(!startup_invalid && startup_seen==2);
        if(mode==1) {
            CHECK(player && player->driver.source_live && player->driver.timer);
            CHECK(fd2_test_audio_clock_after_mdi(audio)==0);
            float output[192];CHECK(fd2_audio_render_offline(audio,output,96)==96);
            CHECK(fd2_bios_clock_close()==0);fd2_audio_destroy(audio);
            CHECK(!player->driver.source_live);fd2_bgm_destroy(player);
        } else {
            CHECK(!player);
            fd2_audio_error error;CHECK(fd2_audio_get_first_error(audio,&error)==0);
            CHECK(error==(mode==2?FD2_AUDIO_ERROR_CLOCK:FD2_AUDIO_ERROR_NONE));
            if(mode==3)CHECK(fd2_bios_clock_close()==0);
            /* 已撤销借用，AudioDestroy不得再回调刚被释放的driver。 */
            fd2_audio_destroy(audio);
        }
    }
    CHECK(create_rejects_synchronously_dropped_chip());return true;
}
