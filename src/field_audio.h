#ifndef FD2_FIELD_AUDIO_H
#define FD2_FIELD_AUDIO_H

#include <stddef.h>

#include "audio_pcm.h"

typedef enum {
    FD2_FIELD_SFX_DETAIL_OPEN,
    FD2_FIELD_SFX_DETAIL_CLOSE,
    FD2_FIELD_SFX_ACTOR_GROUP_FLASH,
    FD2_FIELD_SFX_STAGE_TRANSITION,
    FD2_FIELD_SFX_EARTHQUAKE,
} fd2_field_sfx;

typedef enum {
    FD2_FIELD_SFX_BANK_UI = 31,
    FD2_FIELD_SFX_BANK_BATTLE = 80,
} fd2_field_sfx_bank;

typedef struct {
    fd2_pcm_player *player;
    const fd2_pcm_bank *ui_bank;
    const fd2_pcm_bank *battle_bank;
} fd2_field_audio;

int fd2_field_sfx_resolve(fd2_field_sfx cue,
                          fd2_field_sfx_bank *bank,
                          size_t *sample_index);
void fd2_field_audio_init(fd2_field_audio *audio,
                          fd2_pcm_player *player,
                          const fd2_pcm_bank *ui_bank,
                          const fd2_pcm_bank *battle_bank);

/* 所有已确认战场调用都复用原版全局 AIL sample handle。 */
int fd2_field_audio_play(fd2_field_audio *audio, fd2_field_sfx cue);
int fd2_field_audio_stop(fd2_field_audio *audio);

#endif /* FD2_FIELD_AUDIO_H */
