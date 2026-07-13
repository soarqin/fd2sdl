#ifndef FD2_AUDIO_PCM_H
#define FD2_AUDIO_PCM_H

#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

#include "archive.h"
#include "audio.h"

#define FD2_PCM_MAX_VOICES 24

typedef struct {
    fd2_archive archive;
} fd2_pcm_bank;

/* 两种 open 都借用输入数据；parent fdother 或 data buffer 必须保持有效，
 * 直到 bank 关闭且引用它的全部 voice 已 retire。失败时 bank 保持可安全 close。 */
int fd2_pcm_bank_open(fd2_pcm_bank *bank, const fd2_archive *fdother,
                      size_t entry_index);
int fd2_pcm_bank_open_mem(fd2_pcm_bank *bank, const uint8_t *data,
                          size_t size);
void fd2_pcm_bank_close(fd2_pcm_bank *bank);
size_t fd2_pcm_bank_count(const fd2_pcm_bank *bank);

typedef struct {
    _Atomic int state;
    const uint8_t *data;
    size_t length;
    size_t index;
    uint64_t fraction;
    uint32_t source_rate;
    uint32_t output_rate;
    int loop_count;
    int finished;
} fd2_pcm_voice;

typedef struct {
    fd2_audio *audio;
    int source_rate;
    fd2_pcm_voice voices[FD2_PCM_MAX_VOICES];
} fd2_pcm_player;

/* source_rate 来自 AIL sample 初始化；FD2 的已确认默认值为 11025 Hz。 */
int fd2_pcm_player_init(fd2_pcm_player *player, fd2_audio *audio,
                        int source_rate);

/* loop_count 遵循 Miles sample 语义：0=无限循环，正数=总播放次数。 */
int fd2_pcm_play(fd2_pcm_player *player, const fd2_pcm_bank *bank,
                 size_t sample_index, int loop_count, float gain);

/* 复现原版全局 AIL sample handle：sfx_play @0x4acaa 先调用
 * ail_end_sample @0x5ea19，再启动新样本；index -1 仅执行停止。 */
int fd2_pcm_play_replace(fd2_pcm_player *player, const fd2_pcm_bank *bank,
                         size_t sample_index, int loop_count, float gain);
int fd2_pcm_stop(fd2_pcm_player *player);

/* 必须先销毁 fd2_audio（触发所有 source retire），再关闭 bank/player。 */
void fd2_pcm_player_close(fd2_pcm_player *player);

#endif /* FD2_AUDIO_PCM_H */
