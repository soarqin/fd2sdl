#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "audio.h"
#include "audio_pcm.h"
#include "field_audio.h"
#include "pcm_clock_fixture.h"

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static void put_u32le(uint8_t *p, uint32_t value) {
    p[0] = (uint8_t)value;
    p[1] = (uint8_t)(value >> 8);
    p[2] = (uint8_t)(value >> 16);
    p[3] = (uint8_t)(value >> 24);
}

static size_t make_index_bank(uint8_t *data, size_t count, uint8_t base) {
    size_t start = 6 + count * 4;
    memcpy(data, "LLLLLL", 6);
    for (size_t i = 0; i < count; i++) {
        put_u32le(data + 6 + i * 4, (uint32_t)(start + i));
        data[start + i] = (uint8_t)(base + i);
    }
    return start + count;
}

static size_t make_bank(uint8_t *data) {
    memcpy(data, "LLLLLL", 6);
    put_u32le(data + 6, 14);
    put_u32le(data + 10, 17);
    data[14] = 0;
    data[15] = 128;
    data[16] = 255;
    data[17] = 128;
    data[18] = 255;
    return 19;
}

static int test_field_sfx_mapping(void) {
    static const struct {
        fd2_field_sfx cue;
        fd2_field_sfx_bank bank;
        size_t sample;
    } cases[] = {
        {FD2_FIELD_SFX_DETAIL_OPEN, FD2_FIELD_SFX_BANK_UI, 5},
        {FD2_FIELD_SFX_DETAIL_CLOSE, FD2_FIELD_SFX_BANK_UI, 6},
        {FD2_FIELD_SFX_COMMAND_MENU, FD2_FIELD_SFX_BANK_UI, 8},
        {FD2_FIELD_SFX_DIALOG_GLYPH, FD2_FIELD_SFX_BANK_UI, 2},
        {FD2_FIELD_SFX_FOOTSTEP_SAMPLE_9, FD2_FIELD_SFX_BANK_UI, 9},
        {FD2_FIELD_SFX_FOOTSTEP_SAMPLE_10, FD2_FIELD_SFX_BANK_UI, 10},
        {FD2_FIELD_SFX_FOOTSTEP_SAMPLE_11, FD2_FIELD_SFX_BANK_UI, 11},
        {FD2_FIELD_SFX_ACTOR_GROUP_ARRIVAL,
         FD2_FIELD_SFX_BANK_ARRIVAL, 0},
        {FD2_FIELD_SFX_ACTOR_GROUP_FLASH, FD2_FIELD_SFX_BANK_BATTLE, 1},
        {FD2_FIELD_SFX_STAGE_TRANSITION, FD2_FIELD_SFX_BANK_BATTLE, 11},
        {FD2_FIELD_SFX_EARTHQUAKE, FD2_FIELD_SFX_BANK_BATTLE, 13},
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        fd2_field_sfx_bank bank;
        size_t sample;
        CHECK(fd2_field_sfx_resolve(cases[i].cue, &bank, &sample) == 0);
        CHECK(bank == cases[i].bank && sample == cases[i].sample);
    }
    fd2_field_sfx_bank bank = FD2_FIELD_SFX_BANK_ARRIVAL;
    size_t sample = SIZE_MAX;
    CHECK(fd2_field_sfx_resolve((fd2_field_sfx)99, &bank, &sample) != 0);
    CHECK(fd2_field_sfx_resolve((fd2_field_sfx)-1, &bank, &sample) != 0);
    CHECK(fd2_field_sfx_resolve(FD2_FIELD_SFX_DETAIL_OPEN, NULL, &sample) != 0);
    CHECK(fd2_field_sfx_resolve(FD2_FIELD_SFX_DETAIL_OPEN, &bank, NULL) != 0);
    CHECK(bank == FD2_FIELD_SFX_BANK_ARRIVAL && sample == SIZE_MAX);
    return 0;
}

static int test_field_footstep_mapping(void) {
    fd2_field_sfx cue;
    /* profile 5 的类别 0：sample 9、每 6 相位一次。 */
    CHECK(fd2_field_footstep_resolve(9, 5, 1, 0, &cue) == 1);
    CHECK(cue == FD2_FIELD_SFX_FOOTSTEP_SAMPLE_9);
    CHECK(fd2_field_footstep_resolve(9, 5, 1, 1, &cue) == 0);
    CHECK(fd2_field_footstep_resolve(9, 5, 1, 6, &cue) == 1);
    /* profile 1 的类别 1：sample 9、每 4 相位一次。 */
    CHECK(fd2_field_footstep_resolve(0, 1, 1, 4, &cue) == 1);
    CHECK(cue == FD2_FIELD_SFX_FOOTSTEP_SAMPLE_9);
    /* profile 3 的类别 2：sample 11、每 9 相位一次。 */
    CHECK(fd2_field_footstep_resolve(4, 3, 1, 9, &cue) == 1);
    CHECK(cue == FD2_FIELD_SFX_FOOTSTEP_SAMPLE_11);
    /* unit ID 0x1c 仍走 profile 表；特殊 profile/race 固定 sample 10、
     * 每 6 相位一次。未知 profile 默认类别 1。 */
    CHECK(fd2_field_footstep_resolve(0x1c, 3, 1, 9, &cue) == 1);
    CHECK(cue == FD2_FIELD_SFX_FOOTSTEP_SAMPLE_11);
    CHECK(fd2_field_footstep_resolve(0, 19, 1, 6, &cue) == 1);
    CHECK(cue == FD2_FIELD_SFX_FOOTSTEP_SAMPLE_10);
    CHECK(fd2_field_footstep_resolve(0, 3, 4, 6, &cue) == 1);
    CHECK(fd2_field_footstep_resolve(0, 0, 0, 4, &cue) == 1);
    CHECK(cue == FD2_FIELD_SFX_FOOTSTEP_SAMPLE_9);
    CHECK(fd2_field_footstep_resolve(0, 1, 1, 0, NULL) == -1);
    return 0;
}

/* 完整40240/49690/49891的既有独立公式：初fraction=1/2，11025→22050。
 * 单个N byte片段写2N-1个build值；byte指针floor((j+1)/2)，每值乘202。
 * 这里核field_audio→真实shared driver的绑定，不把该表当48k final PCM。 */
static int source_build(unsigned byte) { return ((int)byte-128)*202; }
static uint8_t source_u8(int value) {
    if(value>32767)value=32767;
    else if(value< -32768)value= -32768;
    return (uint8_t)((value+32768)/256);
}

static int test_field_default_amplitude_table(void) {
    uint8_t data[35+256];make_index_bank(data,6,128);
    for(unsigned i=0;i<256;i++)data[35+i]=(uint8_t)i;
    fd2_pcm_bank bank;CHECK(fd2_pcm_bank_open_mem(&bank,data,sizeof(data))==0);
    fd2_pcm_test_fixture f;CHECK(fd2_pcm_test_open(&f)==0);
    fd2_field_audio field_audio;
    fd2_field_audio_init(&field_audio,&f.primary,&bank,NULL,NULL);
    CHECK(fd2_field_audio_play(&field_audio,FD2_FIELD_SFX_DETAIL_OPEN)==0);
    CHECK(f.primary.driver==&f.driver && f.secondary.driver==&f.driver);
    CHECK(f.driver.dig.samples[0].data==data+35 &&
          f.driver.dig.samples[0].length==256 && f.driver.dig.samples[0].status==4);
    CHECK(fd2_audio_dig_service(&f.driver.dig)==1);
    CHECK(f.driver.dig.last_half==0 && f.driver.dig.entered_active==1 &&
          f.driver.dig.samples[0].status==2 && f.driver.dig.samples[0].position==256);
    for(unsigned i=0;i<FD2_DIG_HALF;i++) {
        int expected=i<511u ? source_build((i+1u)/2u):0;
        CHECK(f.driver.dig.build[i]==expected);
        CHECK(f.driver.dig.dma[1][i]==source_u8(expected));
        CHECK(f.driver.dig.dma[0][i]==128);
    }
    CHECK(fd2_pcm_test_close(&f)==0);fd2_pcm_bank_close(&bank);return 0;
}

static int test_field_audio_dispatch(void) {
    uint8_t ui_data[76],battle_data[76],arrival_data[16];
    fd2_pcm_bank ui,battle,arrival;
    CHECK(fd2_pcm_bank_open_mem(&ui,ui_data,make_index_bank(ui_data,14,128))==0);
    CHECK(fd2_pcm_bank_open_mem(&battle,battle_data,make_index_bank(battle_data,14,160))==0);
    CHECK(fd2_pcm_bank_open_mem(&arrival,arrival_data,make_index_bank(arrival_data,1,192))==0);
    fd2_pcm_test_fixture f;CHECK(fd2_pcm_test_open(&f)==0);
    CHECK(!fd2_audio_has_device(f.audio)); /* null输出仍保P/DMA逻辑，不开设备。 */
    fd2_field_audio a;fd2_field_audio_init(&a,&f.primary,&ui,&battle,&arrival);
    static const struct {fd2_field_sfx cue;unsigned bank,index;} cases[]={
        {FD2_FIELD_SFX_DETAIL_OPEN,0,5},{FD2_FIELD_SFX_DIALOG_GLYPH,0,2},
        {FD2_FIELD_SFX_FOOTSTEP_SAMPLE_11,0,11},
        {FD2_FIELD_SFX_ACTOR_GROUP_ARRIVAL,2,0},
        {FD2_FIELD_SFX_ACTOR_GROUP_FLASH,1,1},
        {FD2_FIELD_SFX_STAGE_TRANSITION,1,11},{FD2_FIELD_SFX_EARTHQUAKE,1,13}};
    const uint8_t *bases[]={ui_data+62,battle_data+62,arrival_data+10};
    fd2_dig_sample secondary=f.driver.dig.samples[1];
    uint8_t initial_dma[4096];memcpy(initial_dma,f.driver.dig.dma,sizeof(initial_dma));
    for(unsigned i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        CHECK(fd2_field_audio_play(&a,cases[i].cue)==0);
        const fd2_dig_sample *p=&f.driver.dig.samples[0];
        CHECK(p->data==bases[cases[i].bank]+cases[i].index && p->length==1 &&
              !p->position && p->loop_count==1 && p->status==4);
        CHECK(!memcmp(&secondary,&f.driver.dig.samples[1],sizeof(secondary)));
        CHECK(!memcmp(initial_dma,f.driver.dig.dma,sizeof(initial_dma)));
    }
    CHECK(fd2_field_audio_play_footstep(&a,0,1,1)==0 && a.footstep_counter==1);
    CHECK(f.driver.dig.samples[0].data==ui_data+62+9);
    for(unsigned i=0;i<3;i++) {
        fd2_audio_dig saved=f.driver.dig;
        CHECK(fd2_field_audio_play_footstep(&a,0,1,1)==0);
        CHECK(!memcmp(&saved,&f.driver.dig,sizeof(saved)));
    }
    CHECK(a.footstep_counter==4 && fd2_field_audio_play_footstep(&a,0,1,1)==0 &&
          a.footstep_counter==5 && f.driver.dig.samples[0].data==ui_data+62+9);
    fd2_audio_dig saved=f.driver.dig;
    fd2_field_audio_init(&a,&f.primary,NULL,&battle,&arrival);
    CHECK(fd2_field_audio_play(&a,FD2_FIELD_SFX_DETAIL_OPEN)<0);
    fd2_field_audio_init(&a,NULL,&ui,&battle,&arrival);
    CHECK(fd2_field_audio_play(&a,FD2_FIELD_SFX_EARTHQUAKE)<0);
    CHECK(!memcmp(&saved,&f.driver.dig,sizeof(saved)));
    CHECK(fd2_pcm_test_close(&f)==0);
    fd2_pcm_bank_close(&arrival);fd2_pcm_bank_close(&battle);fd2_pcm_bank_close(&ui);return 0;
}

static int test_one_shot_and_loop(void) {
    uint8_t data[19];fd2_pcm_bank bank;
    CHECK(fd2_pcm_bank_open_mem(&bank,data,make_bank(data))==0 && fd2_pcm_bank_count(&bank)==2);
    static const int one[]={-25856,0,0,25654,25654};
    static const int twice[]={0,25654,25654,0,25654,25654};
    for(unsigned c=0;c<2;c++) {
        fd2_pcm_test_fixture f;CHECK(fd2_pcm_test_open(&f)==0);
        CHECK(fd2_pcm_play(&f.primary,&bank,c,c+1,FD2_PCM_TEST_GAIN)==0);
        CHECK(fd2_audio_dig_service(&f.driver.dig)==1);
        unsigned count=c?6u:5u;const int *expected=c?twice:one;
        for(unsigned i=0;i<FD2_DIG_HALF;i++) {
            int value=i<count?expected[i]:0;
            CHECK(f.driver.dig.build[i]==value && f.driver.dig.dma[1][i]==source_u8(value));
            CHECK(f.driver.dig.dma[0][i]==128);
        }
        CHECK(f.driver.dig.samples[0].status==2 && f.driver.dig.samples[0].loop_count==1 &&
              f.driver.dig.samples[0].position==(c?2u:3u));
        CHECK(fd2_pcm_test_close(&f)==0);
    }
    fd2_pcm_bank_close(&bank);return 0;
}

static int test_two_handles_stop_and_failed_replace_prefix(void) {
    uint8_t data[19];fd2_pcm_bank bank;
    CHECK(fd2_pcm_bank_open_mem(&bank,data,make_bank(data))==0);
    fd2_pcm_test_fixture f;CHECK(fd2_pcm_test_open(&f)==0);
    CHECK(fd2_pcm_play_replace(&f.primary,&bank,0,0,FD2_PCM_TEST_GAIN)==0);
    CHECK(fd2_pcm_play_replace(&f.secondary,&bank,1,0,FD2_PCM_TEST_GAIN)==0);
    CHECK(fd2_audio_dig_service(&f.driver.dig)==1 && f.driver.dig.entered_active==2);
    uint8_t tail[4096];int32_t build[FD2_DIG_HALF];
    memcpy(tail,f.driver.dig.dma,sizeof(tail));memcpy(build,f.driver.dig.build,sizeof(build));
    fd2_dig_sample primary=f.driver.dig.samples[0],secondary=f.driver.dig.samples[1];
    CHECK(primary.status==4 && secondary.status==4);
    CHECK(fd2_pcm_stop(&f.primary)==0);primary.status=8;
    CHECK(!memcmp(&primary,&f.driver.dig.samples[0],sizeof(primary)));
    CHECK(!memcmp(&secondary,&f.driver.dig.samples[1],sizeof(secondary)));
    CHECK(!memcmp(tail,f.driver.dig.dma,sizeof(tail)) && !memcmp(build,f.driver.dig.build,sizeof(build)));
    CHECK(fd2_pcm_play_replace(&f.secondary,&bank,2,1,FD2_PCM_TEST_GAIN)==FD2_DIG_H_ARGUMENT);
    secondary.status=8;
    CHECK(!memcmp(&secondary,&f.driver.dig.samples[1],sizeof(secondary)));
    CHECK(!memcmp(tail,f.driver.dig.dma,sizeof(tail)) && !memcmp(build,f.driver.dig.build,sizeof(build)));
    CHECK(fd2_pcm_play_replace(&f.primary,&bank,1,3,FD2_PCM_TEST_GAIN)==0);
    CHECK(f.driver.dig.samples[0].status==4 && f.driver.dig.samples[0].data==data+17 &&
          f.driver.dig.samples[0].length==2 && !f.driver.dig.samples[0].position &&
          f.driver.dig.samples[0].loop_count==3 && f.driver.dig.playing==1);
    CHECK(!memcmp(&secondary,&f.driver.dig.samples[1],sizeof(secondary)));
    CHECK(!memcmp(tail,f.driver.dig.dma,sizeof(tail)) && !memcmp(build,f.driver.dig.build,sizeof(build)));
    fd2_audio_dig saved=f.driver.dig;
    CHECK(fd2_pcm_play_replace(&f.primary,&bank,0,-1,FD2_PCM_TEST_GAIN)==FD2_DIG_H_ARGUMENT);
    CHECK(fd2_pcm_play_replace(&f.primary,&bank,0,1,1.0f)==FD2_DIG_H_PROFILE);
    CHECK(!memcmp(&saved,&f.driver.dig,sizeof(saved)));
    CHECK(fd2_pcm_test_close(&f)==0);fd2_pcm_bank_close(&bank);return 0;
}

static int test_replacement_burst(void) {
    uint8_t data[19];fd2_pcm_bank bank;
    CHECK(fd2_pcm_bank_open_mem(&bank,data,make_bank(data))==0);
    fd2_pcm_test_fixture f;CHECK(fd2_pcm_test_open(&f)==0);
    CHECK(fd2_pcm_play_replace(&f.secondary,&bank,1,0,FD2_PCM_TEST_GAIN)==0);
    fd2_dig_sample other=f.driver.dig.samples[1];
    uint8_t dma[4096];memcpy(dma,f.driver.dig.dma,sizeof(dma));
    fd2_ail_timer clock=*fd2_bios_clock_ail_state();
    /* 97是固定有界输入长度，故意超过旧24槽；不是新的容量或功能分母。 */
    for(unsigned i=0;i<97;i++) {
        CHECK(fd2_pcm_play_replace(&f.primary,&bank,i&1u,i%3u+1u,FD2_PCM_TEST_GAIN)==0);
        CHECK(!memcmp(&other,&f.driver.dig.samples[1],sizeof(other)));
        CHECK(!memcmp(dma,f.driver.dig.dma,sizeof(dma)));
    }
    CHECK(f.driver.dig.samples[0].data==data+14 && f.driver.dig.samples[0].length==3 &&
          f.driver.dig.samples[0].status==4 && !f.driver.dig.samples[0].position &&
          f.driver.dig.samples[0].loop_count==1);
    CHECK(!memcmp(&clock,fd2_bios_clock_ail_state(),sizeof(clock)) && fd2_audio_now_ns(f.audio)==0);
    CHECK(fd2_pcm_test_close(&f)==0);fd2_pcm_bank_close(&bank);return 0;
}

static int test_profile_rejection_and_borrowed_lifecycle(void) {
    uint8_t data[19];fd2_pcm_bank bank;
    CHECK(fd2_pcm_bank_open_mem(&bank,data,make_bank(data))==0);
    fd2_pcm_test_fixture f;CHECK(fd2_pcm_test_open(&f)==0);
    fd2_pcm_player rejected,saved_player;memset(&rejected,0xa5,sizeof(rejected));saved_player=rejected;
    fd2_audio_dig saved=f.driver.dig;fd2_ail_timer clock=*fd2_bios_clock_ail_state();
    CHECK(fd2_pcm_player_init(&rejected,f.audio,11025)==FD2_DIG_H_SHARED_DRIVER_REQUIRED); /* 正式不能误走legacy。 */
    CHECK(fd2_pcm_player_init_shared(&rejected,&f.driver,0,INT_MAX)==FD2_DIG_H_PROFILE);
    CHECK(fd2_pcm_player_init_shared(&rejected,&f.driver,2,11025)==FD2_DIG_H_ARGUMENT);
    CHECK(!memcmp(&saved_player,&rejected,sizeof(rejected)) &&
          !memcmp(&saved,&f.driver.dig,sizeof(saved)) &&
          !memcmp(&clock,fd2_bios_clock_ail_state(),sizeof(clock)));
    CHECK(fd2_pcm_play_replace(&f.primary,&bank,0,0,FD2_PCM_TEST_GAIN)==0);
    CHECK(fd2_pcm_test_close(&f)==0);
    CHECK(f.driver.dig.closed && !f.driver.dig.timer && !f.driver.audio &&
          !f.primary.driver && !f.secondary.driver);
    saved=f.driver.dig;
    CHECK(fd2_pcm_stop(&f.primary)<0 && fd2_bios_clock_advance(UINT64_C(1000000000))<0);
    CHECK(!memcmp(&saved,&f.driver.dig,sizeof(saved)));
    fd2_pcm_bank_close(&bank);memset(data,0xcc,sizeof(data));
    /* 正确终结借用后重用同buffer；新scope不再引用旧directory或旧P callback。 */
    CHECK(fd2_pcm_bank_open_mem(&bank,data,make_bank(data))==0 && fd2_pcm_test_open(&f)==0);
    CHECK(fd2_pcm_play_replace(&f.secondary,&bank,1,1,FD2_PCM_TEST_GAIN)==0);
    CHECK(f.driver.dig.samples[0].status==2 && f.driver.dig.samples[0].data==NULL &&
          f.driver.dig.samples[1].data==data+17);
    CHECK(fd2_pcm_test_close(&f)==0);fd2_pcm_bank_close(&bank);
    fd2_pcm_bank invalid;CHECK(fd2_pcm_bank_open_mem(&invalid,NULL,0)<0);fd2_pcm_bank_close(&invalid);
    return 0;
}

int main(void) {
    CHECK(test_field_sfx_mapping()==0);
    CHECK(test_field_footstep_mapping()==0);
    CHECK(test_field_default_amplitude_table()==0);
    CHECK(test_field_audio_dispatch()==0);
    CHECK(test_one_shot_and_loop()==0);
    CHECK(test_two_handles_stop_and_failed_replace_prefix()==0);
    CHECK(test_replacement_burst()==0);
    CHECK(test_profile_rejection_and_borrowed_lifecycle()==0);
    puts("audio PCM: shared normal consumer/build/DMA/tail/gate/lifecycle PASS; no device/PIC-wave claim");
    return 0;
}
