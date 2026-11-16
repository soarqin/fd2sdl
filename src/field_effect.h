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
    /* effect-local像素资源：flash旧/solid两页、quake三页、mask九页。
     * 只由PREPARE在临时页生成；实际play才按原写点复制像素到App页。
     * 资源/clock不随像素复制，close销毁，不成为第二个持久App owner。 */
    fd2_field_page *page_snapshots[FD2_FIELD_EFFECT_MAX_SNAPSHOTS];
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
/* 成功后归frames，失败仍归caller；不可重复slot或同页多次交接。 */
int fd2_field_effect_frames_take_page(fd2_field_effect_frames *frames,
    size_t index,fd2_field_page *page);
/* 源App写点：flash只在frame0装solid并保留；quake只装首原App页；
 * mask每帧装对应页。不做任何统一“正常尾页”恢复，也不推进clock。 */
int fd2_field_effect_apply_page_frame(const fd2_field_effect_frames *frames,
    fd2_vga *vga,size_t frame);

/* cue/默认VGA snapshot及page写点之后、upload/等待之前的有限逐帧领域叶。
 * caller可重画当前App mask/actors并重新project，不能在PREPARE触发本口。
 * hook不代tick/ms等待，不持帧deadline；本层在成功返回后统一upload并等待。
 * 返回0完成、1hostquit、-1错误；错误保已实际达到的App像素，不造终态。 */
typedef int (*fd2_field_effect_frame_fn)(fd2_field_effect effect,size_t frame,
    size_t source,fd2_vga *vga,void *userdata);
int fd2_field_effect_play_hooked(const fd2_field_effect_frames *frames,
    fd2_vga *vga,fd2_field_audio *audio,int timed,
    fd2_field_effect_frame_fn apply,void *userdata);

/* 同步播放已生成帧。timed=0 用于确定性快速验证，不等待也不呈现；
 * timed!=0 先upload再使用原版绝对BIOS tick／10 ms／5 ms等待；flash末页无等待。
 * 不借用累计deadline。转场加白由field_game wrapper在500 ms尾停顿之后执行。 */
int fd2_field_effect_play(const fd2_field_effect_frames *frames,
                          fd2_vga *vga,
                          fd2_field_audio *audio,
                          int timed);

#endif /* FD2_FIELD_EFFECT_H */
