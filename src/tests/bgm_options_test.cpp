/* field_options_menu_execute @0x1728c（原同名登记）/music_track_play @0x25977
 * （原登记 FUN_0004ab8b）配置与sequence-only渐变；纯离线，不开设备。 */
#include "../bgm.cpp"
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#x); return false; } } while(0)
struct Fixture {
 fd2_audio *audio=nullptr;fd2_bgm_player *player=nullptr;
 ~Fixture(){fd2_audio_destroy(audio);fd2_bgm_destroy(player);}
 bool open(){fd2_audio_config a{};a.sample_rate=48000;audio=fd2_audio_create(&a);
  fd2_bgm_config b{};b.fdmus_path="original_game/FDMUS.DAT";b.ail_bank_path="original_game/SAMPLE.AD";b.sample_rate=48000;
  player=fd2_bgm_create(audio,&b);return player!=nullptr;}
 bool render(size_t n){float data[1024];while(n){size_t take=std::min<size_t>(512,n);if(fd2_audio_render_offline(audio,data,take)!=take)return false;n-=take;}return true;}
};
static int option(Fixture &f,int enabled,int fade) {
 return fd2_bgm_set_music_enabled(f.player,enabled,fade);
}
static bool toggle_is_sequence_fade() {
 Fixture f;CHECK(f.open());CHECK(fd2_bgm_play(f.player,18,0)==0 && f.render(24000));
 BgmVoice *v=f.player->driver.active_sequence;CHECK(v && v->sequence_volume>0 && v->sequence_volume<127);
 int current=v->sequence_volume;auto cursor=v->sequence.cursor;auto ticks=v->timer_ticks;auto before=f.player->driver;
 CHECK(option(f,0,1)==0);CHECK(!std::memcmp(&before,&f.player->driver,sizeof(before)));
 CHECK(f.render(400));CHECK(v->target_volume==0 && v->fade_interval_us==1000000u/unsigned(current));
 CHECK(v->sequence_volume>=current-2 && v->sequence_volume<=current && v->timer_ticks>ticks);
 CHECK(v->sequence.cursor>=cursor);CHECK(option(f,1,1)==0 && f.render(400));
 CHECK(v->target_volume==127 && v==f.player->driver.active_sequence && v->track==18);
 return true;
}
static bool load_flag_and_same_track() {
 Fixture f;CHECK(f.open());CHECK(fd2_bgm_play(f.player,18,0)==0 && f.render(24000));
 BgmVoice *first=f.player->driver.active_sequence;int volume=first->sequence_volume;
 auto cursor=first->sequence.cursor;auto ticks=first->timer_ticks;auto before=f.player->driver;
 CHECK(option(f,0,0)==0);CHECK(fd2_bgm_play(f.player,18,0)==0);
 CHECK(!std::memcmp(&before,&f.player->driver,sizeof(before)) && first->sequence.cursor==cursor && first->timer_ticks==ticks);
 CHECK(first->target_volume==127 && first->sequence_volume==volume);
 CHECK(fd2_bgm_play(f.player,11,0)==0 && f.render(400));
 BgmVoice *second=f.player->driver.active_sequence;
 CHECK(second!=first && second->sequence_volume==0 && second->target_volume==0 && second->timer_ticks>0);
 CHECK(f.render(24000));cursor=second->sequence.cursor;ticks=second->timer_ticks;
 CHECK(option(f,1,1)==0 && f.render(400));CHECK(second->sequence_volume>0 && second->target_volume==127 && second->timer_ticks>ticks && second->sequence.cursor>=cursor);
 return true;
}
static bool repeat_stop_is_not_deduplicated() {
 Fixture f;CHECK(f.open());CHECK(fd2_bgm_play(f.player,18,0)==0 && f.render(144000));
 BgmVoice *v=f.player->driver.active_sequence;CHECK(v && v->sequence_volume==127);
 CHECK(fd2_bgm_stop(f.player)==0 && f.render(400));uint32_t first=v->fade_interval_us;
 CHECK(f.render(48000));int current=v->sequence_volume;CHECK(current>0 && current<127);
 CHECK(fd2_bgm_current_track(f.player)==-1 && fd2_bgm_stop(f.player)==0 && f.render(400));
 CHECK(v->fade_interval_us==4000000u/unsigned(current) && v->fade_interval_us>first);
 CHECK(v==f.player->driver.active_sequence && v->target_volume==0);return true;
}
static bool successful_load_restarts_once_and_failure_preserves() {
 Fixture f;CHECK(f.open());CHECK(fd2_bgm_play(f.player,19,0)==0 && f.render(24000));
 BgmVoice *first=f.player->driver.active_sequence;auto before=f.player->driver;int track=fd2_bgm_current_track(f.player);
 CHECK(fd2_bgm_set_music_enabled(f.player,2,1)==-1 && fd2_bgm_set_music_enabled(f.player,0,2)==-1);
 CHECK(!std::memcmp(&before,&f.player->driver,sizeof(before)) && fd2_bgm_current_track(f.player)==track);
 /* 已成功导入后的唯一 -1→primary；坏Load不调用此序列。 */
 CHECK(option(f,0,0)==0 && fd2_bgm_stop(f.player)==0 && fd2_bgm_play(f.player,19,0)==0 && f.render(400));
 BgmVoice *second=f.player->driver.active_sequence;CHECK(second!=first && first->state.load()==2);
 CHECK(second->track==19 && second->sequence_volume==0 && second->target_volume==0);
 auto ticks=second->timer_ticks;CHECK(fd2_bgm_play(f.player,19,0)==0 && f.render(400));
 CHECK(second==f.player->driver.active_sequence && second->timer_ticks>ticks);return true;
}
int main(){if(!toggle_is_sequence_fade())return 1;
 if(!load_flag_and_same_track() || !repeat_stop_is_not_deduplicated() || !successful_load_restarts_once_and_failure_preserves())return 1;
 std::puts("bgm_options_test: ok (no device/GUI)");return 0;}
