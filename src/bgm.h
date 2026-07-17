#ifndef FD2_BGM_H
#define FD2_BGM_H

#include <stddef.h>

#include "archive.h"
#include "audio.h"

/* 原版 music_track_play @0x4ab8b 的单 sequence 播放器。
 * FDMUS.DAT 继续作为 XMIDI 数据源，SAMPLE.AD 继续作为 AIL/AdLib
 * 音色表；SDL 只接收最终 float32 stereo。 */
typedef struct fd2_bgm_player fd2_bgm_player;

typedef struct {
    const char *fdmus_path;
    const char *ail_bank_path;
    int sample_rate;
    int voice_count; /* 0=原版 SBLASTER.MDI 的 9 个 OPL2 voice */
} fd2_bgm_config;

#ifdef __cplusplus
extern "C" {
#endif

fd2_bgm_player *fd2_bgm_create(fd2_audio *audio,
                               const fd2_bgm_config *config);
void fd2_bgm_destroy(fd2_bgm_player *player);

/* loop_count 遵循 AIL sequence 语义：0=无限循环，正数=总播放次数。
 * 同一 track 重复请求不重启；其他 track 会替换 music bus 上的 sequence。 */
int fd2_bgm_play(fd2_bgm_player *player, size_t track, int loop_count);
int fd2_bgm_stop(fd2_bgm_player *player);
int fd2_bgm_current_track(const fd2_bgm_player *player);

/* 测试和资源验证接口。 */
size_t fd2_bgm_track_count(const fd2_bgm_player *player);
int fd2_bgm_track_valid(const fd2_bgm_player *player, size_t track);

#ifdef __cplusplus
}
#endif

#endif /* FD2_BGM_H */
