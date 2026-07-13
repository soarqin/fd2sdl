/* 炎龙骑士团 2 SDL3 重写 - 战场全屏效果帧调度
 *
 * 逆向依据：field_actor_group_flash @0x414ee、
 * field_earthquake_effect @0x4673b、field_stage_transition_effect @0x4982c。
 * 这里只复现已确认的 buffer 顺序与 SFX 时序；缩放／palette 视觉参数仍由
 * 对应 renderer 生成 snapshot，避免按效果名称猜测像素。
 */

#include "field_effect.h"

#include <stdlib.h>
#include <string.h>

size_t fd2_field_effect_frame_count(fd2_field_effect effect) {
    switch (effect) {
        case FD2_FIELD_EFFECT_ACTOR_GROUP_FLASH: return 11;
        case FD2_FIELD_EFFECT_EARTHQUAKE: return 60;
        case FD2_FIELD_EFFECT_STAGE_TRANSITION: return 9;
    }
    return 0;
}

int fd2_field_effect_source_index(fd2_field_effect effect, size_t frame) {
    if (frame >= fd2_field_effect_frame_count(effect)) return -1;
    switch (effect) {
        case FD2_FIELD_EFFECT_ACTOR_GROUP_FLASH:
            /* 5 轮「原始→修改」，最后恢复原始帧。 */
            return (frame & 1u) ? 1 : 0;
        case FD2_FIELD_EFFECT_EARTHQUAKE: {
            static const int cycle[4] = {0, 1, 2, 1};
            return cycle[frame % 4u];
        }
        case FD2_FIELD_EFFECT_STAGE_TRANSITION:
            return (int)frame;
    }
    return -1;
}

int fd2_field_effect_sfx(fd2_field_effect effect, size_t frame,
                         fd2_field_sfx *cue) {
    if (!cue || frame >= fd2_field_effect_frame_count(effect)) return -1;
    switch (effect) {
        case FD2_FIELD_EFFECT_ACTOR_GROUP_FLASH:
            if (frame == 0) {
                *cue = FD2_FIELD_SFX_ACTOR_GROUP_FLASH;
                return 0;
            }
            break;
        case FD2_FIELD_EFFECT_EARTHQUAKE:
            if (frame < 43 && frame % 6u == 0) {
                *cue = FD2_FIELD_SFX_EARTHQUAKE;
                return 0;
            }
            break;
        case FD2_FIELD_EFFECT_STAGE_TRANSITION:
            if (frame == 0) {
                *cue = FD2_FIELD_SFX_STAGE_TRANSITION;
                return 0;
            }
            break;
    }
    return -1;
}

int fd2_field_effect_copy_frame(uint8_t *dst,
                                const uint8_t *const *snapshots,
                                size_t snapshot_count,
                                size_t bytes,
                                fd2_field_effect effect,
                                size_t frame) {
    if (!dst || !snapshots || bytes == 0) return -1;
    int index = fd2_field_effect_source_index(effect, frame);
    if (index < 0 || (size_t)index >= snapshot_count || !snapshots[index])
        return -1;
    memcpy(dst, snapshots[index], bytes);
    return 0;
}

static size_t required_snapshots(fd2_field_effect effect) {
    switch (effect) {
        case FD2_FIELD_EFFECT_ACTOR_GROUP_FLASH: return 2;
        case FD2_FIELD_EFFECT_EARTHQUAKE: return 3;
        case FD2_FIELD_EFFECT_STAGE_TRANSITION: return 9;
    }
    return 0;
}

int fd2_field_effect_frames_open(fd2_field_effect_frames *frames,
                                 fd2_field_effect effect,
                                 size_t snapshot_count) {
    size_t required = required_snapshots(effect);
    if (!frames || frames->storage || required == 0 ||
        snapshot_count != required ||
        snapshot_count > FD2_FIELD_EFFECT_MAX_SNAPSHOTS ||
        snapshot_count > SIZE_MAX / (VGA_W * VGA_H))
        return -1;
    uint8_t *storage = calloc(snapshot_count, VGA_W * VGA_H);
    if (!storage) return -1;
    memset(frames, 0, sizeof(*frames));
    frames->storage = storage;
    frames->effect = effect;
    frames->snapshot_count = snapshot_count;
    frames->bytes_per_snapshot = VGA_W * VGA_H;
    for (size_t i = 0; i < snapshot_count; i++)
        frames->snapshots[i] = frames->storage + i * frames->bytes_per_snapshot;
    return 0;
}

uint8_t *fd2_field_effect_snapshot(fd2_field_effect_frames *frames,
                                   size_t index) {
    if (!frames || !frames->storage || index >= frames->snapshot_count)
        return NULL;
    return frames->storage + index * frames->bytes_per_snapshot;
}

void fd2_field_effect_frames_close(fd2_field_effect_frames *frames) {
    if (!frames) return;
    free(frames->storage);
    memset(frames, 0, sizeof(*frames));
}

int fd2_field_effect_play(const fd2_field_effect_frames *frames,
                          fd2_vga *vga,
                          fd2_field_audio *audio,
                          int timed) {
    size_t required = frames ? required_snapshots(frames->effect) : 0;
    if (!frames || !frames->storage || !vga || required == 0 ||
        frames->snapshot_count != required ||
        frames->bytes_per_snapshot != VGA_W * VGA_H)
        return -1;
    size_t frame_count = fd2_field_effect_frame_count(frames->effect);
    for (size_t frame = 0; frame < frame_count; frame++) {
        fd2_field_sfx cue;
        if (audio && fd2_field_effect_sfx(frames->effect, frame, &cue) == 0)
            (void)fd2_field_audio_play(audio, cue);
        if (fd2_field_effect_copy_frame(vga->framebuffer, frames->snapshots,
                frames->snapshot_count, frames->bytes_per_snapshot,
                frames->effect, frame) != 0)
            return -1;
        if (!timed) continue;
        if (frames->effect == FD2_FIELD_EFFECT_ACTOR_GROUP_FLASH) {
            if (frame < 10) fd2_vga_present_timed(vga, 55);
            else fd2_vga_present(vga);
        } else if (frames->effect == FD2_FIELD_EFFECT_EARTHQUAKE) {
            fd2_vga_present_timed(vga, 10);
        } else {
            fd2_vga_present_timed(vga, 5);
        }
    }
    if (timed) fd2_vga_wait_frame_deadline(vga);
    return 0;
}
