/* F5：原accepted502的104个reg/value与当前正式单S/512B包首写批次联结。
 * 原oracle：2026-11-17 normal-title02 observer64254eb3，writer review600de0e4。
 * 只保uint16LE reg+uint8 value的短指纹，不分发资源/端口原trace。
 * 当前公共create/play→共享AIL/DIG→真实Audio/Nuked；无设备/窗口。
 * 相同内容不证明原包的timer caller身份、首IRQ/PIC时点或整段波形。
 */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <SDL3/SDL.h>
#include "../third_party/nukedopl3.h"
static void packet_write(opl3_chip *,uint16_t,uint8_t);
static void packet_batch(void *,const uint8_t *,uint32_t,uint32_t);
#define OPL3_WriteReg packet_write
#define FD2_BGM_MESSAGE_BATCH_TRACE(d,b,u,n) packet_batch(d,b,u,n)
#include "../bgm.cpp"
#undef OPL3_WriteReg
extern "C" {
#include "audio_pcm.h"
}

static bool armed,bad_chip,multiple_batches;
static opl3_chip *expected_chip;
static uint64_t words_hash=UINT64_C(14695981039346656037);
static uint32_t writes,batches,current_count,current_bytes,first_batch,first_count,first_bytes;
static void packet_batch(void *,const uint8_t *,uint32_t used,uint32_t count) {
    if(!armed)return;
    ++batches;current_count=count;current_bytes=used;
}
static void packet_write(opl3_chip *chip,uint16_t reg,uint8_t value) {
    if(armed) {
        bad_chip|=chip!=expected_chip;
        if(!writes) {first_batch=batches;first_count=current_count;first_bytes=current_bytes;}
        else multiple_batches|=first_batch!=batches;
        const uint8_t bytes[]={uint8_t(reg),uint8_t(reg>>8),value};
        for(uint8_t byte:bytes){words_hash^=byte;words_hash*=UINT64_C(1099511628211);}
        ++writes;
    }
    OPL3_WriteReg(chip,reg,value);
}
int main() {
    (void)&reset_driver; /* 旧私有TU兼容helper；这里只取地址，不执行。 */
    fd2_audio_config ac{};ac.sample_rate=48000;ac.allow_null=1;ac.original_mixer=1;
    fd2_audio *audio=fd2_audio_create(&ac);
    if(!audio)return 1;
    bool okay=fd2_bios_clock_ail_attach(fd2_audio_now_ns(audio),65536u)==0;
    fd2_bgm_config bc{};bc.fdmus_path="original_game/FDMUS.DAT";
    bc.ail_bank_path="original_game/SAMPLE.AD";bc.sample_rate=48000;
    fd2_bgm_player *player=okay?fd2_bgm_create(audio,&bc):nullptr;
    fd2_pcm_driver dig{};
    okay=okay && player && fd2_pcm_driver_init(&dig,audio)==0;
    okay=okay && dig.dig.timer_handle==4u && fd2_bgm_play(player,18,0)==0;
    expected_chip=player?&player->driver.opl:nullptr;
    armed=okay;
    unsigned milliseconds=0;
    float output[96];
    /* New bounded prefix only; no old24s render or whole-track suite. */
    for(;okay && !writes && milliseconds<20;milliseconds++)
        okay=fd2_audio_render_offline(audio,output,48)==48;
    armed=false;
    fd2_audio_error error=FD2_AUDIO_ERROR_NONE;
    okay=okay && fd2_audio_get_first_error(audio,&error)==0 && !error;
    std::printf("first packet: writes=%u hash=%016llx batches=%u count=%u bytes=%u "
                "elapsed_H_ms=%u sequence_tick=%u\n",writes,
                static_cast<unsigned long long>(words_hash),batches,first_count,first_bytes,
                milliseconds,player?player->sequence.timer_ticks:0u);
    /* The hash is independent observed原reg/value. Count32 is checked separately
     * against this C batch; nativeCX32 alone did not prove payload identity. */
    okay=okay && !fd2_audio_has_device(audio) && !bad_chip && !multiple_batches &&
        writes==104u && words_hash==UINT64_C(0x0813266f04ed078a) && first_count==32u;
    if(dig.audio)okay=(fd2_pcm_driver_quiesce(&dig)==0)&&okay;
    else okay=(fd2_audio_quiesce_dig(audio)==0)&&okay;
    okay=(fd2_bios_clock_close()==0)&&okay;
    fd2_audio_destroy(audio);fd2_bgm_destroy(player);fd2_pcm_driver_close(&dig);
    if(!okay)return 1;
    std::puts("first packet: native104 content/order + current public owner PASS; timing/PCM unqualified");
    return 0;
}
