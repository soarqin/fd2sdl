/* 炎龙骑士团 2 SDL3 重写 - 战场音效 cue 到原版 sample bank 的映射
 *
 * 逆向依据：sfx_play @0x4acaa、field_command_menu_open @code0 0x741c、
 * field_command_menu_close @code0 0x76b4、text_dialog_glyph_step
 * @code0 0x64e8、field_actor_group_flash @0x414ee、
 * field_earthquake_effect @0x4673b、field_stage_transition_effect @0x4982c。
 */

#include "field_audio.h"

#include <string.h>

int fd2_field_sfx_resolve(fd2_field_sfx cue,
                          fd2_field_sfx_bank *bank,
                          size_t *sample_index) {
    if (!bank || !sample_index) return -1;
    switch (cue) {
        case FD2_FIELD_SFX_DETAIL_OPEN:
            *bank = FD2_FIELD_SFX_BANK_UI;
            *sample_index = 5;
            return 0;
        case FD2_FIELD_SFX_DETAIL_CLOSE:
            *bank = FD2_FIELD_SFX_BANK_UI;
            *sample_index = 6;
            return 0;
        case FD2_FIELD_SFX_COMMAND_MENU:
            *bank = FD2_FIELD_SFX_BANK_UI;
            *sample_index = 8;
            return 0;
        case FD2_FIELD_SFX_DIALOG_GLYPH:
            *bank = FD2_FIELD_SFX_BANK_UI;
            *sample_index = 2;
            return 0;
        case FD2_FIELD_SFX_ACTOR_GROUP_FLASH:
            *bank = FD2_FIELD_SFX_BANK_BATTLE;
            *sample_index = 1;
            return 0;
        case FD2_FIELD_SFX_STAGE_TRANSITION:
            *bank = FD2_FIELD_SFX_BANK_BATTLE;
            *sample_index = 11;
            return 0;
        case FD2_FIELD_SFX_EARTHQUAKE:
            *bank = FD2_FIELD_SFX_BANK_BATTLE;
            *sample_index = 13;
            return 0;
    }
    return -1;
}

void fd2_field_audio_init(fd2_field_audio *audio,
                          fd2_pcm_player *player,
                          const fd2_pcm_bank *ui_bank,
                          const fd2_pcm_bank *battle_bank) {
    if (!audio) return;
    memset(audio, 0, sizeof(*audio));
    audio->player = player;
    audio->ui_bank = ui_bank;
    audio->battle_bank = battle_bank;
}

int fd2_field_audio_play(fd2_field_audio *audio, fd2_field_sfx cue) {
    if (!audio || !audio->player) return -1;
    fd2_field_sfx_bank bank_id;
    size_t sample_index;
    if (fd2_field_sfx_resolve(cue, &bank_id, &sample_index) != 0) return -1;
    const fd2_pcm_bank *bank = bank_id == FD2_FIELD_SFX_BANK_UI
                             ? audio->ui_bank : audio->battle_bank;
    if (!bank) return -1;
    return fd2_pcm_play_replace(audio->player, bank, sample_index, 1, 1.0f);
}

int fd2_field_audio_stop(fd2_field_audio *audio) {
    return audio ? fd2_pcm_stop(audio->player) : -1;
}
