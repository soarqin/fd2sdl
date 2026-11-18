/* sfx_play25a96/25b45：51E62先于sync/资源/stop；两view共享唯一gate。
 * 39805/41770仅stop8，DMA共同尾与正在借用的P不因禁用消失。无设备纯测。 */
#include "audio_pcm.h"
#include <stdio.h>
#include <string.h>
static unsigned control_syncs;
static int observed_sync(fd2_audio *audio);
#define fd2_audio_sync observed_sync
#include "../audio_pcm.c"
#undef fd2_audio_sync
#include "pcm_clock_fixture.h"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"%s:%d: %s\n",__func__,__LINE__,#x);return 1;}}while(0)
static int observed_sync(fd2_audio *audio) {++control_syncs;return fd2_audio_sync(audio);}

static void make_bank(uint8_t data[18]) {
    memcpy(data,"LLLLLL",6);data[6]=10;data[7]=data[8]=data[9]=0;
    memset(data+10,192,8);
}

static int disabled_gate_is_before_sync_resource_and_stop(void) {
    uint8_t bytes[18];make_bank(bytes);fd2_pcm_bank bank;
    CHECK(fd2_pcm_bank_open_mem(&bank,bytes,sizeof(bytes))==0);
    fd2_pcm_test_fixture f;CHECK(fd2_pcm_test_open(&f)==0);
    CHECK(fd2_pcm_play_replace(&f.primary,&bank,0,0,FD2_PCM_TEST_GAIN)==0 &&
          fd2_pcm_play_replace(&f.secondary,&bank,0,0,FD2_PCM_TEST_GAIN)==0);
    CHECK(fd2_audio_dig_service(&f.driver.dig)==1 &&
          f.driver.dig.samples[0].status==4 && f.driver.dig.samples[1].status==4);
    /* 任一view写的是同一51E62；无sync、无原播放源停止或DMA清零。 */
    unsigned syncs=control_syncs;
    fd2_audio_dig before=f.driver.dig;
    CHECK(fd2_pcm_player_set_enabled(&f.primary,0)==0 && f.driver.dig.enabled==0);
    before.enabled=0;CHECK(!memcmp(&before,&f.driver.dig,sizeof(before)) && control_syncs==syncs);
    fd2_pcm_driver saved=f.driver;fd2_pcm_player p=f.primary,q=f.secondary;
    fd2_ail_timer clock=*fd2_bios_clock_ail_state();
    uint64_t now=fd2_audio_now_ns(f.audio);
    /* 非法资源/循环/gain不能在disabled时泄入资源或sync；不执行越界读。 */
    CHECK(fd2_pcm_play(&f.primary,NULL,SIZE_MAX,-1,1.0f)==0);
    CHECK(fd2_pcm_play_replace(&f.secondary,NULL,SIZE_MAX,-1,0.0f)==0);
    CHECK(fd2_pcm_stop(&f.primary)==0 && fd2_pcm_stop(&f.secondary)==0);
    CHECK(control_syncs==syncs && !memcmp(&saved,&f.driver,sizeof(saved)) &&
          !memcmp(&p,&f.primary,sizeof(p)) && !memcmp(&q,&f.secondary,sizeof(q)) &&
          !memcmp(&clock,fd2_bios_clock_ail_state(),sizeof(clock)) && fd2_audio_now_ns(f.audio)==now);
    CHECK(fd2_pcm_player_set_enabled(&f.secondary,2)<0 && !memcmp(&saved,&f.driver,sizeof(saved)));
    CHECK(fd2_pcm_player_set_enabled(&f.secondary,1)==0 && f.driver.dig.enabled==1);
    uint8_t tail[4096];memcpy(tail,f.driver.dig.dma,sizeof(tail));
    fd2_dig_sample primary=f.driver.dig.samples[0],secondary=f.driver.dig.samples[1];
    CHECK(fd2_pcm_stop(&f.primary)==0);primary.status=8;
    CHECK(!memcmp(&primary,&f.driver.dig.samples[0],sizeof(primary)) &&
          !memcmp(&secondary,&f.driver.dig.samples[1],sizeof(secondary)) &&
          !memcmp(tail,f.driver.dig.dma,sizeof(tail)));
    /* disabled从不挡App终态quiesce／clock close／借用释放。 */
    CHECK(fd2_pcm_player_set_enabled(&f.primary,0)==0);
    CHECK(fd2_pcm_test_close(&f)==0 && f.driver.dig.closed && !f.driver.dig.timer &&
          !f.driver.audio && !f.primary.driver && !f.secondary.driver);
    fd2_pcm_bank_close(&bank);memset(bytes,0xcc,sizeof(bytes));return 0;
}

static int close_and_new_scope_do_not_reuse_borrowed_source(void) {
    for(unsigned generation=0;generation<2;generation++) {
        uint8_t bytes[18];make_bank(bytes);memset(bytes+10,generation?255:0,8);
        fd2_pcm_bank bank;CHECK(fd2_pcm_bank_open_mem(&bank,bytes,sizeof(bytes))==0);
        fd2_pcm_test_fixture f;CHECK(fd2_pcm_test_open(&f)==0);
        CHECK(f.driver.dig.samples[0].status==2 && f.driver.dig.samples[1].status==2 &&
              !f.driver.dig.samples[0].data && !f.driver.dig.samples[1].data);
        CHECK(fd2_pcm_play_replace(&f.secondary,&bank,0,0,FD2_PCM_TEST_GAIN)==0 &&
              f.driver.dig.samples[1].data==bytes+10);
        CHECK(fd2_pcm_player_set_enabled(&f.secondary,0)==0);
        CHECK(fd2_pcm_test_close(&f)==0);
        fd2_pcm_bank_close(&bank);memset(bytes,0xcc,sizeof(bytes));
        fd2_audio_dig saved=f.driver.dig;
        CHECK(fd2_pcm_stop(&f.secondary)<0 && fd2_bios_clock_advance(UINT64_C(1000000000))<0 &&
              !memcmp(&saved,&f.driver.dig,sizeof(saved)));
    }
    return 0;
}

int main(void) {
    if(disabled_gate_is_before_sync_resource_and_stop() ||
       close_and_new_scope_do_not_reuse_borrowed_source())return 1;
    puts("audio options: shared gate/no-sync/stop8/tail/borrowed lifecycle PASS; no device/GUI");
    return 0;
}
