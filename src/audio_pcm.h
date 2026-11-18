#ifndef FD2_AUDIO_PCM_H
#define FD2_AUDIO_PCM_H
#include <stddef.h>
#include <stdint.h>
#include "archive.h"
#include "audio.h"
#include "audio_dig.h"

typedef struct {
    fd2_archive archive;
} fd2_pcm_bank;

/* 两种 open 都借用输入数据；parent fdother 或 data buffer 必须保持有效，
 * 直到对应P不再借用；终态先driver quiesce再关闭bank。失败可安全close。 */
int fd2_pcm_bank_open(fd2_pcm_bank *bank, const fd2_archive *fdother,
                      size_t entry_index);
int fd2_pcm_bank_open_mem(fd2_pcm_bank *bank, const uint8_t *data,
                          size_t size);
void fd2_pcm_bank_close(fd2_pcm_bank *bank);
size_t fd2_pcm_bank_count(const fd2_pcm_bank *bank);

/* 正式App只有一份D/P0/P1/build/DMA；Audio借用D连接SB channel。 */
typedef struct fd2_pcm_driver {
    fd2_audio *audio;
    fd2_audio_dig dig;
    int normal_bound, quiesced;
} fd2_pcm_driver;

typedef struct fd2_pcm_player {
    fd2_pcm_driver *driver;
    unsigned handle;
    int source_rate;
} fd2_pcm_player;
/* 53EF1对应成功安装的同driver，与OS stream/静音/未来cue gate分开。 */
static inline int fd2_pcm_player_available(const fd2_pcm_player *player) {
    return player && player->driver && player->driver->audio &&
        player->driver->dig.attached && !player->driver->dig.closed && !player->driver->quiesced;
}

/* 正式Main：MDI创建之后用同Audio安装DIG，两个view借同driver。
 * 每个D只初始化一次；Load/START/换曲不得重调。失败也可能已建立Audio借用
 * （normal_bound=1），D必须活到quiesce→clock_close→AudioDestroy→close结束，
 * 不能仅因返回负值就free/复用。Main的零初始化D覆盖完整App生命周期。 */
int fd2_pcm_driver_init(fd2_pcm_driver *driver,fd2_audio *audio);
int fd2_pcm_player_init_shared(fd2_pcm_player *player,fd2_pcm_driver *driver,
                               unsigned handle,int source_rate);
/* 终态：quiesce有源Reset → clock_close → AudioDestroy → driver_close。 */
int fd2_pcm_driver_quiesce(fd2_pcm_driver *driver);
void fd2_pcm_driver_close(fd2_pcm_driver *driver);
/* 显式external仅有限控制/诊断，不冒正常输出、物理quiesce资格。 */
int fd2_pcm_driver_init_external(fd2_pcm_driver *driver,fd2_audio *audio,
                                const fd2_dig_external_sb16 *provider);
/* 旧implicit接口只返回H_SHARED_DRIVER_REQUIRED，无写；正式必须shared。 */
int fd2_pcm_player_init(fd2_pcm_player *player, fd2_audio *audio,
                        int source_rate);
/* sfx_play @0x25a96（原FUN_000231de）/sfx_play_secondary @0x25b45
 * （原FUN_0002328d）：只抑制后续play/replace/stop，不停既有source。
 * 不影响AudioDestroy/owner退休；close仍要求Audio已先销毁。主线程调用。 */
int fd2_pcm_player_set_enabled(fd2_pcm_player *player, int enabled);

/* loop_count 遵循 Miles sample 语义：0=无限循环，正数=总播放次数。 */
int fd2_pcm_play(fd2_pcm_player *player, const fd2_pcm_bank *bank,
                 size_t sample_index, int loop_count, float gain);

/* 复现一个 AIL sample handle：sfx_play @0x25a96 先调用
 * ail_stop_sample @0x39805（原登记ail_end_sample）：status4→8，保位置及DMA；
 * 真正ail_end_sample为0x398df。随后启动新样本；index -1 仅执行停止。
 * 不同 fd2_pcm_player 对应不同原版 handle，停止／替换不能误停另一 handle。 */
int fd2_pcm_play_replace(fd2_pcm_player *player, const fd2_pcm_bank *bank,
                         size_t sample_index, int loop_count, float gain);
int fd2_pcm_stop(fd2_pcm_player *player);

/* driver quiesce→clock关闭→AudioDestroy→driver close之后关闭view/bank。 */
void fd2_pcm_player_close(fd2_pcm_player *player);

#endif /* FD2_AUDIO_PCM_H */
