#ifndef FD2_FIELD_EFFECT_H
#define FD2_FIELD_EFFECT_H

#include <stddef.h>
#include <stdint.h>

#include "field_audio.h"
#include "vga.h"

#define FD2_FIELD_EFFECT_MAX_SNAPSHOTS 9

typedef enum {
    FD2_FIELD_EFFECT_ACTOR_GROUP_FLASH,
    FD2_FIELD_EFFECT_EARTHQUAKE,
    FD2_FIELD_EFFECT_STAGE_TRANSITION,
} fd2_field_effect;

typedef struct {
    fd2_field_effect effect;
    uint8_t *storage;
    const uint8_t *snapshots[FD2_FIELD_EFFECT_MAX_SNAPSHOTS];
    size_t snapshot_count;
    size_t bytes_per_snapshot;
} fd2_field_effect_frames;

size_t fd2_field_effect_frame_count(fd2_field_effect effect);

/* 返回当前帧使用的预渲染 source 索引：
 * flash: 0=原帧、1=修改帧；earthquake: 3 个缩放／偏移帧按 0,1,2,1；
 * stage transition: 0..8 已按实际播放顺序排列。失败返回 -1。 */
int fd2_field_effect_source_index(fd2_field_effect effect, size_t frame);

/* 返回当前帧应重启的原版 cue；无 cue 返回 -1。 */
int fd2_field_effect_sfx(fd2_field_effect effect, size_t frame,
                         fd2_field_sfx *cue);

/* 从调用方预先生成的 snapshots 复制当前帧。本层不猜测 tile 缩放参数。 */
int fd2_field_effect_copy_frame(uint8_t *dst,
                                const uint8_t *const *snapshots,
                                size_t snapshot_count,
                                size_t bytes,
                                fd2_field_effect effect,
                                size_t frame);

/* snapshot 生命周期位于主线程；音频回调不访问这些 buffer。
 * frames 必须零初始化或已由 close 清空；对已打开对象重复 open 返回失败。 */
int fd2_field_effect_frames_open(fd2_field_effect_frames *frames,
                                 fd2_field_effect effect,
                                 size_t snapshot_count);
uint8_t *fd2_field_effect_snapshot(fd2_field_effect_frames *frames,
                                   size_t index);
void fd2_field_effect_frames_close(fd2_field_effect_frames *frames);

/* 同步播放已生成帧。timed=0 用于确定性快速验证，不等待也不呈现；
 * timed!=0 使用原版 1 BIOS tick／10 ms／5 ms 节奏。转场的渐暗由
 * field_game wrapper 在 500 ms 尾停顿之后执行。 */
int fd2_field_effect_play(const fd2_field_effect_frames *frames,
                          fd2_vga *vga,
                          fd2_field_audio *audio,
                          int timed);

#endif /* FD2_FIELD_EFFECT_H */
