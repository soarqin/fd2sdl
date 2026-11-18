#ifndef FD2_FIELD_AUDIO_H
#define FD2_FIELD_AUDIO_H

#include <stddef.h>

#include "audio_pcm.h"

/* ail_sample_volume_pan_tables_rebuild @0x40240（原sub_40240）：
 * sample默认volume100/pan64的U8→stereo16表为(u-128)*202。
 * 这里只复现App cue的名义sample层，不表示DIG master、DAC或OS最终幅度。 */
#define FD2_GAME_SFX_DEFAULT_GAIN (FD2_DIG_DEFAULT_U8_SCALE / 256.0f)

typedef enum {
    FD2_FIELD_SFX_DETAIL_OPEN,
    FD2_FIELD_SFX_DETAIL_CLOSE,
    FD2_FIELD_SFX_COMMAND_MENU,
    FD2_FIELD_SFX_DIALOG_GLYPH,
    FD2_FIELD_SFX_FOOTSTEP_SAMPLE_9,
    FD2_FIELD_SFX_FOOTSTEP_SAMPLE_10,
    FD2_FIELD_SFX_FOOTSTEP_SAMPLE_11,
    FD2_FIELD_SFX_ACTOR_GROUP_ARRIVAL,
    FD2_FIELD_SFX_ACTOR_GROUP_FLASH,
    FD2_FIELD_SFX_STAGE_TRANSITION,
    FD2_FIELD_SFX_EARTHQUAKE,
} fd2_field_sfx;

typedef enum {
    FD2_FIELD_SFX_BANK_UI = 31,
    FD2_FIELD_SFX_BANK_BATTLE = 80,
    FD2_FIELD_SFX_BANK_ARRIVAL = 95,
} fd2_field_sfx_bank;

typedef struct {
    fd2_pcm_player *player;
    const fd2_pcm_bank *ui_bank;
    const fd2_pcm_bank *battle_bank;
    const fd2_pcm_bank *arrival_bank;
    uint8_t footstep_counter;
    /* 借用App既有secondary AIL handle；不会创建或转移音频所有权。 */
    fd2_pcm_player *secondary_player;
} fd2_field_audio;

int fd2_field_sfx_resolve(fd2_field_sfx cue,
                          fd2_field_sfx_bank *bank,
                          size_t *sample_index);

/* field_actor_footstep_play @code0 0x22230 的 sample 与 cadence 选择。
 * 返回 1 表示本相位应播放，0 表示只推进计数，-1 表示输入无效。 */
int fd2_field_footstep_resolve(uint8_t unit_id,
                               uint8_t movement_profile,
                               uint8_t race,
                               uint8_t step_counter,
                               fd2_field_sfx *cue);
int fd2_field_audio_play_footstep(fd2_field_audio *audio,
                                  uint8_t unit_id,
                                  uint8_t movement_profile,
                                  uint8_t race);
void fd2_field_audio_init(fd2_field_audio *audio,
                          fd2_pcm_player *player,
                          const fd2_pcm_bank *ui_bank,
                          const fd2_pcm_bank *battle_bank,
                          const fd2_pcm_bank *arrival_bank);

/* 所有已确认战场调用都复用原版全局 AIL sample handle。 */
int fd2_field_audio_play(fd2_field_audio *audio, fd2_field_sfx cue);

/* sfx_play @0x25a96：借用原primary AIL handle播放临时FDOTHER81/88等源bank。
 * index==-1只走既有stop；disabled门仍在bank读取/stop之前。bank在同步play时
 * 有效，所借App原字节保持到全部引用voice退休；不转移player/原字节所有权，
 * 不改变音量或混音器。关闭临时bank只释放目录，不释放App原数据。
 * 循环次数沿Miles约定0=无限、正数=总次数，坏输入返回-1。 */
int fd2_field_audio_play_borrowed_bank(fd2_field_audio *audio,
    const fd2_pcm_bank *bank, int sample_index, int loop_count);

#endif /* FD2_FIELD_AUDIO_H */
