/* field_options_menu_execute @0x1728c（原同名登记）/music_track_play @0x25977
 * （原登记 FUN_0004ab8b）配置与sequence-only渐变；纯离线，不开设备。 */
#include "../bgm.cpp"
#include "audio_clock_fixture.h"
#define CHECK(x) do { if (!(x)) { std::fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#x); return false; } } while(0)
struct Fixture {
 fd2_audio *audio=nullptr;fd2_bgm_player *player=nullptr;
 ~Fixture(){(void)fd2_bios_clock_close();fd2_audio_destroy(audio);fd2_bgm_destroy(player);}
 bool open(){fd2_audio_config a{};a.sample_rate=48000;audio=fd2_audio_create(&a);
  if(fd2_test_audio_clock_prepare(audio)!=0)return false;
  fd2_bgm_config b{};b.fdmus_path="original_game/FDMUS.DAT";b.ail_bank_path="original_game/SAMPLE.AD";b.sample_rate=48000;
  player=fd2_bgm_create(audio,&b);return player && fd2_test_audio_clock_after_mdi(audio)==0;}
 bool render(size_t n){float data[1024];while(n){size_t take=std::min<size_t>(512,n);if(fd2_audio_render_offline(audio,data,take)!=take)return false;n-=take;}return true;}
};
/* MDIslot period8333、base5000：两次IRQ最多10000150.5ns；48k的481帧
 * 严格跨一个service最大间隔。原400帧8.333ms不能保证新callback。 */
static constexpr size_t source_service_frames=481;
static int option(Fixture &f,int enabled,int fade) {
 return fd2_bgm_set_music_enabled(f.player,enabled,fade);
}
static bool toggle_is_sequence_fade() {
 Fixture f;CHECK(f.open());CHECK(fd2_bgm_play(f.player,18,0)==0 && f.render(24000));
 BgmVoice *v=f.player->driver.active_sequence;CHECK(v && v->sequence_volume>0 && v->sequence_volume<127);
 int current=v->sequence_volume;auto cursor=v->sequence.cursor;auto ticks=v->timer_ticks;auto before=f.player->driver;
 /* 原449e0->43230只queue当前CC7；43270共同tail才送502。字面包来自原prestate算式。 */
 CHECK(before.queued_messages==0 && before.queued_bytes==0);
 std::vector<uint8_t> pending_cc7;
 for(size_t logical=0;logical<kMidiChannels;++logical)if(v->volume_cache[logical]>=0) {
  unsigned mapped=v->channel_map[logical];CHECK(mapped<kMidiChannels);
  pending_cc7.push_back(static_cast<uint8_t>(0xb0u|mapped));pending_cc7.push_back(7);
  pending_cc7.push_back(static_cast<uint8_t>(unsigned(v->volume_cache[logical])*unsigned(current)/127u));
 }
 const fd2_ail_timer timer_before=*before.timer;
 CHECK(option(f,0,1)==0);
 CHECK(f.player->driver.volumes==before.volumes && f.player->driver.active_sequence==v);
 CHECK(f.player->driver.queued_messages==pending_cc7.size()/3u && f.player->driver.queued_bytes==pending_cc7.size());
 CHECK(std::equal(pending_cc7.begin(),pending_cc7.end(),f.player->driver.message_bytes.begin()));
 CHECK(!std::memcmp(&timer_before,before.timer,sizeof(timer_before)) && v->sequence.cursor==cursor);
 CHECK(v->target_volume==0 && v->fade_interval_us==1000000u/unsigned(current));
 CHECK(v->sequence_volume==current && v->timer_ticks==ticks); /* 控制同步，时间不偷进。 */
 CHECK(f.render(source_service_frames));
 CHECK(f.player->driver.queued_messages==0 && f.player->driver.queued_bytes==0);
 CHECK(v->sequence_volume>=current-2 && v->sequence_volume<=current && v->timer_ticks>ticks);
 CHECK(v->sequence.cursor>=cursor);CHECK(option(f,1,1)==0 && f.render(source_service_frames));
 CHECK(v->target_volume==127 && v==f.player->driver.active_sequence && fd2_bgm_current_track(f.player)==18);
 return true;
}
static bool load_flag_and_same_track() {
 Fixture f;CHECK(f.open());CHECK(fd2_bgm_play(f.player,18,0)==0 && f.render(24000));
 BgmVoice *first=f.player->driver.active_sequence;int volume=first->sequence_volume;
 auto cursor=first->sequence.cursor;auto ticks=first->timer_ticks;auto before=f.player->driver;
 CHECK(option(f,0,0)==0);CHECK(fd2_bgm_play(f.player,18,0)==0);
 CHECK(!std::memcmp(&before,&f.player->driver,sizeof(before)) && first->sequence.cursor==cursor && first->timer_ticks==ticks);
 CHECK(first->target_volume==127 && first->sequence_volume==volume);
 CHECK(fd2_bgm_play(f.player,11,0)==0 && f.render(source_service_frames));
 BgmVoice *second=f.player->driver.active_sequence;
 CHECK(second==first && second->sequence_volume==0 && second->target_volume==0 && second->timer_ticks>0);
 CHECK(f.render(24000));cursor=second->sequence.cursor;ticks=second->timer_ticks;
 CHECK(option(f,1,1)==0 && f.render(source_service_frames));CHECK(second->sequence_volume>0 && second->target_volume==127 && second->timer_ticks>ticks && second->sequence.cursor>=cursor);
 return true;
}
static bool repeat_stop_is_not_deduplicated() {
 Fixture f;CHECK(f.open());CHECK(fd2_bgm_play(f.player,18,0)==0 && f.render(144000));
 BgmVoice *v=f.player->driver.active_sequence;CHECK(v && v->sequence_volume==127);
 CHECK(fd2_bgm_stop(f.player)==0);uint32_t first=v->fade_interval_us;
 CHECK(v->target_volume==0 && first==4000000u/127u);
 CHECK(f.render(48000));int current=v->sequence_volume;CHECK(current>0 && current<127);
 CHECK(fd2_bgm_current_track(f.player)==-1 && fd2_bgm_stop(f.player)==0);
 CHECK(v->sequence_volume==current);
 CHECK(v->fade_interval_us==4000000u/unsigned(current) && v->fade_interval_us>first);
 CHECK(v==f.player->driver.active_sequence && v->target_volume==0);return true;
}
static bool successful_load_restarts_once_and_failure_preserves() {
 Fixture f;CHECK(f.open());CHECK(fd2_bgm_play(f.player,19,0)==0 && f.render(24000));
 BgmVoice *first=f.player->driver.active_sequence;auto before=f.player->driver;int track=fd2_bgm_current_track(f.player);
 CHECK(fd2_bgm_set_music_enabled(f.player,2,1)==-1 && fd2_bgm_set_music_enabled(f.player,0,2)==-1);
 CHECK(!std::memcmp(&before,&f.player->driver,sizeof(before)) && fd2_bgm_current_track(f.player)==track);
 /* 已成功导入后的唯一 -1→primary；坏Load不调用此序列。 */
 CHECK(option(f,0,0)==0 && fd2_bgm_stop(f.player)==0 && fd2_bgm_play(f.player,19,0)==0 && f.render(source_service_frames));
 BgmVoice *second=f.player->driver.active_sequence;CHECK(second==first);
 CHECK(fd2_bgm_current_track(f.player)==19 && second->sequence_volume==0 && second->target_volume==0);
 auto ticks=second->timer_ticks;CHECK(fd2_bgm_play(f.player,19,0)==0 && f.render(source_service_frames));
 CHECK(second==f.player->driver.active_sequence && second->timer_ticks>ticks);return true;
}
int main(){if(!toggle_is_sequence_fade())return 1;
 if(!load_flag_and_same_track() || !repeat_stop_is_not_deduplicated() || !successful_load_restarts_once_and_failure_preserves())return 1;
 std::puts("bgm_options_test: ok (no device/GUI)");return 0;}
