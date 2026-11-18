/* 正式PCM consumer的无设备夹具：同一Audio/AIL owner，显式idle MDI→真DIG。
 * first65536/epoch0是H测试输入，不冒原首次IRQ/PIC phase；不安装idle DIG。
 * 原普通profile 11025/100/64→DIG U8mono22050→SB48000，非legacy24voice。 */
#ifndef FD2_TEST_PCM_CLOCK_FIXTURE_H
#define FD2_TEST_PCM_CLOCK_FIXTURE_H
#include <string.h>
#include "audio_pcm.h"
#include "bios_clock.h"

#define FD2_PCM_TEST_GAIN (101.0f / 128.0f)
typedef struct fd2_pcm_test_fixture {
    fd2_audio *audio;
    fd2_pcm_driver driver;
    fd2_pcm_player primary, secondary;
    unsigned mdi_calls;
} fd2_pcm_test_fixture;

static void fd2_pcm_test_mdi(void *user) { ++*(unsigned *)user; }
static int fd2_pcm_test_open(fd2_pcm_test_fixture *f) {
    memset(f,0,sizeof(*f));
    fd2_audio_config config={.sample_rate=48000,.open_device=0,.allow_null=1,
                             .original_mixer=1};
    f->audio=fd2_audio_create(&config);
    if(!f->audio || !fd2_audio_is_original_mixer(f->audio))return -1;
    uint64_t now=fd2_audio_now_ns(f->audio);
    if(now!=0 || fd2_bios_clock_ail_attach(now,65536u)!=0 ||
       fd2_bios_clock_install_mdi(now,fd2_pcm_test_mdi,&f->mdi_calls)!=0u ||
       fd2_pcm_driver_init(&f->driver,f->audio)!=0 ||
       fd2_pcm_player_init_shared(&f->primary,&f->driver,0u,11025)!=0 ||
       fd2_pcm_player_init_shared(&f->secondary,&f->driver,1u,11025)!=0)return -1;
    if(f->driver.dig.timer_handle!=FD2_AIL_TIMER_DIG ||
       f->driver.dig.timer->slots[0].period_us!=8333u ||
       f->driver.dig.timer->slots[1].period_us!=5000u)return -1;
    return 0;
}

static int fd2_pcm_test_close(fd2_pcm_test_fixture *f) {
    int quiet=fd2_pcm_driver_quiesce(&f->driver);
    int clock=fd2_bios_clock_close();
    fd2_audio_destroy(f->audio);f->audio=NULL;
    fd2_pcm_driver_close(&f->driver);
    fd2_pcm_player_close(&f->secondary);fd2_pcm_player_close(&f->primary);
    return quiet<0 || clock<0 ? -1:0;
}
#endif
