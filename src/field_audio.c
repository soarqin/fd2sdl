/* 炎龙骑士团 2 SDL3 重写 - 战场音效 cue 到原版 sample bank 的映射
 *
 * 逆向依据：sfx_play @0x4acaa、field_command_menu_open @code0 0x741c、
 * field_command_menu_close @code0 0x76b4、text_dialog_glyph_step
 * @code0 0x64e8、field_actor_footstep_play @code0 0x22230、
 * field_actor_group_arrival_effect @code0 0x22999、field_actor_group_flash
 * @0x414ee、
 * field_earthquake_effect @0x4673b、field_stage_transition_effect @0x4982c。
 */

#include "field_audio.h"

#include <string.h>

static const uint8_t k_footstep_classes[] = {
    1, 1, 2, 1, 0, 0, 1, 1, 1, 1,
    2, 1, 0, 0, 3, 1, 1, 1, 3, 1,
    0, 0, 1, 1, 1, 1, 1, 1, 1,
};

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
        case FD2_FIELD_SFX_FOOTSTEP_SAMPLE_9:
            *bank = FD2_FIELD_SFX_BANK_UI;
            *sample_index = 9;
            return 0;
        case FD2_FIELD_SFX_FOOTSTEP_SAMPLE_10:
            *bank = FD2_FIELD_SFX_BANK_UI;
            *sample_index = 10;
            return 0;
        case FD2_FIELD_SFX_FOOTSTEP_SAMPLE_11:
            *bank = FD2_FIELD_SFX_BANK_UI;
            *sample_index = 11;
            return 0;
        case FD2_FIELD_SFX_ACTOR_GROUP_ARRIVAL:
            *bank = FD2_FIELD_SFX_BANK_ARRIVAL;
            *sample_index = 0;
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

int fd2_field_footstep_resolve(uint8_t unit_id,
                               uint8_t movement_profile,
                               uint8_t race,
                               uint8_t step_counter,
                               fd2_field_sfx *cue) {
    if (!cue) return -1;

    /* field_unit_ignores_terrain_combat_modifier @code0 0xf183 对 unit ID
     * 0x1c 返回 0，所以该 ID 仍走 profile 表；只有 profile 19 或
     * race 4/5 进入 sample 10 的特殊分支。 */
    if (unit_id != 0x1cu &&
        (movement_profile == 19u || race == 4u || race == 5u)) {
        *cue = FD2_FIELD_SFX_FOOTSTEP_SAMPLE_10;
        return step_counter % 6u == 0u;
    }

    uint8_t class_id = 1;
    if (movement_profile > 0u &&
        movement_profile <= sizeof(k_footstep_classes))
        class_id = k_footstep_classes[movement_profile - 1u];

    if (class_id == 0u || class_id == 1u) {
        *cue = FD2_FIELD_SFX_FOOTSTEP_SAMPLE_9;
        return step_counter % (class_id == 0u ? 6u : 4u) == 0u;
    }
    *cue = FD2_FIELD_SFX_FOOTSTEP_SAMPLE_11;
    return step_counter % 9u == 0u;
}

void fd2_field_audio_init(fd2_field_audio *audio,
                          fd2_pcm_player *player,
                          const fd2_pcm_bank *ui_bank,
                          const fd2_pcm_bank *battle_bank,
                          const fd2_pcm_bank *arrival_bank) {
    if (!audio) return;
    memset(audio, 0, sizeof(*audio));
    audio->player = player;
    audio->ui_bank = ui_bank;
    audio->battle_bank = battle_bank;
    audio->arrival_bank = arrival_bank;
}

int fd2_field_audio_play_footstep(fd2_field_audio *audio,
                                  uint8_t unit_id,
                                  uint8_t movement_profile,
                                  uint8_t race) {
    if (!audio) return -1;
    fd2_field_sfx cue;
    int play = fd2_field_footstep_resolve(
        unit_id, movement_profile, race, audio->footstep_counter, &cue);
    audio->footstep_counter++;
    if (play < 0) return -1;
    return play == 0 || !audio->player ? 0 : fd2_field_audio_play(audio, cue);
}

int fd2_field_audio_play(fd2_field_audio *audio, fd2_field_sfx cue) {
    if (!audio || !audio->player) return -1;
    fd2_field_sfx_bank bank_id;
    size_t sample_index;
    if (fd2_field_sfx_resolve(cue, &bank_id, &sample_index) != 0) return -1;
    const fd2_pcm_bank *bank = bank_id == FD2_FIELD_SFX_BANK_UI
                             ? audio->ui_bank
                             : bank_id == FD2_FIELD_SFX_BANK_BATTLE
                                 ? audio->battle_bank
                                 : audio->arrival_bank;
    if (!bank) return -1;
    return fd2_pcm_play_replace(audio->player, bank, sample_index, 1, 1.0f);
}

int fd2_field_audio_stop(fd2_field_audio *audio) {
    return audio ? fd2_pcm_stop(audio->player) : -1;
}
