/* 离线音频夹具：显式单App AIL完成调用输入，不推定真实首IRQ/PIC。
 * 原正常安装MDI→DIG；无PCM的音乐夹具仍保已证8333/5000共同base。
 * idle DIG仅是本夹具输入，不证明原DIG回调、DMA或波形。 */
#ifndef FD2_TEST_AUDIO_CLOCK_FIXTURE_H
#define FD2_TEST_AUDIO_CLOCK_FIXTURE_H
#include "audio.h"
#include "bios_clock.h"
static void fd2_test_idle_dig(void *user) { (void)user; }
static int fd2_test_audio_clock_prepare(fd2_audio *audio) {
    return audio ? fd2_bios_clock_ail_attach(fd2_audio_now_ns(audio), 65536u) : -1;
}
static int fd2_test_audio_clock_after_mdi(fd2_audio *audio) {
    return audio && fd2_bios_clock_install_dig(fd2_audio_now_ns(audio),
        fd2_test_idle_dig, NULL) == 4u ? 0 : -1;
}
#endif
