/* SPDX-License-Identifier: MIT
 * 原DIG404c0/49690与SB16.DIG09e4/0acc/0682，DOSBox-X b6abbd5
 * MixerChannel::Mix/EndFrame及SB16始终Enable(true)的正常固定profile接点。
 * 数学与物理叶复用sb_channel/sb_dma_provider；无新播放状态或时钟。
 */
#include "audio_sb.h"
#include <string.h>

static int ready(const fd2_audio_sb *sb) {
    return sb && !sb->closed && sb->point && sb->global_fill_up;
}
static int fail(fd2_audio_sb *sb, int error) {
    if (!sb->first_error) sb->first_error = error;
    return sb->first_error;
}
static int sink(void *user, const uint8_t *data, size_t count) {
    fd2_audio_sb *sb = (fd2_audio_sb *)user;
    size_t loaded = 0;
    int result = fd2_sb_channel_add_u8(&sb->channel, data, count, &loaded);
    /* 原DMA Read已完成整count。FULL不可回退cursor或重读loaded以外的尾。 */
    (void)loaded;
    return result < 0 ? fail(sb, -2) : 0;
}
static int fill(void *user) {
    fd2_audio_sb *sb = (fd2_audio_sb *)user;
    if (!ready(sb)) return -1;
    if (sb->global_fill_up(sb->global_user) < 0) return fail(sb, -4);
    return sb->first_error;
}
static int enter_control(fd2_audio_sb *sb) {
    if (!ready(sb)) return -1;
    if (sb->first_error) return sb->first_error;
    if (sb->in_control || sb->in_mix) return fail(sb, -5);
    sb->in_control = 1;
    return 0;
}
static int leave_control(fd2_audio_sb *sb, int result) {
    sb->in_control = 0;
    return result < 0 ? fail(sb, -3) : sb->first_error;
}
static int driver_start(void *user, const uint8_t *ring, uint32_t bytes) {
    fd2_audio_sb *sb = (fd2_audio_sb *)user;
    if (!ready(sb) || !sb->dig || ring != &sb->dig->dma[0][0] ||
        bytes != sizeof(sb->dig->dma)) return -1;
    int result = enter_control(sb);
    if (result < 0) return result;
    result = fd2_sb_dma_start(&sb->dma, ring, bytes, sb->point);
    return leave_control(sb, result);
}
static int driver_half(void *user, int16_t *half) {
    fd2_audio_sb *sb = (fd2_audio_sb *)user;
    if (!ready(sb)) return -1;
    if (sb->first_error) return sb->first_error;
    return fd2_sb_dma_read_half(&sb->dma, half);
}
static int driver_pause(void *user) {
    fd2_audio_sb *sb = (fd2_audio_sb *)user;
    int result = enter_control(sb);
    if (result < 0) return result;
    return leave_control(sb, fd2_sb_dma_request_pause(&sb->dma));
}
int fd2_audio_sb_construct(fd2_audio_sb *sb,const fd2_sb_point *point,
    int32_t left,int32_t right,int (*global_fill_up)(void *),void *user) {
    if (!sb || !point || !global_fill_up ||
        left < 0 || right < 0 ||
        left > FD2_SB_CHANNEL_GAIN_MAX || right > FD2_SB_CHANNEL_GAIN_MAX) return -1;
    memset(sb, 0, sizeof(*sb));
    sb->point = point;
    sb->global_fill_up = global_fill_up; sb->global_user = user;
    (void)fd2_sb_channel_init(&sb->channel, sb->window, left, right);
    int result=fd2_sb_dma_init(&sb->dma, sb, sink, fill);
    if(result<0)return result;
    return 0;
}
int fd2_audio_sb_bind_dig(fd2_audio_sb *sb,fd2_audio_dig *dig) {
    if(!ready(sb)||!dig||sb->dig||sb->first_error||sb->in_mix||sb->in_control)return -1;
    sb->dig=dig;return 0; /* 不memset、不重新初始化唯一channel/LP/window。 */
}
int fd2_audio_sb_external(fd2_audio_sb *sb, fd2_dig_external_sb16 *external) {
    if (!ready(sb) || !sb->dig || !external) return -1;
    *external = (fd2_dig_external_sb16){sb, driver_start, driver_half, driver_pause};
    return 0;
}
int fd2_audio_sb_mix_until(fd2_audio_sb *sb, int32_t (*out)[2],
                          size_t out_frames, size_t whole) {
    if (!ready(sb) || whole > FD2_SB_CHANNEL_MS_FRAMES ||
        out_frames < whole || (whole && !out)) return -1;
    if (sb->first_error) return sb->first_error;
    if (sb->in_mix) return fail(sb, -5);
    sb->in_mix = 1;
    int result = 0;
    for (unsigned attempt = 0; attempt < 2; attempt++) {
        size_t needed = 0;
        result = fd2_sb_channel_input_needed(&sb->channel, whole, &needed);
        if (result < 0 || !needed) break;
        result = fd2_sb_dma_generate(&sb->dma, needed);
        if (result < 0) break;
    }
    if (result >= 0)
        result = fd2_sb_channel_render_until(&sb->channel, out, out_frames, whole);
    sb->in_mix = 0;
    return result < 0 ? fail(sb, -2) : sb->first_error;
}
int fd2_audio_sb_end_frame(fd2_audio_sb *sb) {
    if (!ready(sb) || sb->in_mix) return -1;
    if (sb->first_error) return sb->first_error;
    return fd2_sb_channel_end_frame(&sb->channel);
}
int fd2_audio_sb_dispatch_end(fd2_audio_sb *sb, uint64_t serial) {
    int result = enter_control(sb);
    if (result < 0) return result;
    return leave_control(sb, fd2_sb_dma_dispatch_end(&sb->dma, serial));
}
int fd2_audio_sb_complete_block(fd2_audio_sb *sb) {
    int result = enter_control(sb);
    if (result < 0) return result;
    return leave_control(sb, fd2_sb_dma_complete_block(&sb->dma));
}
int fd2_audio_sb_close(fd2_audio_sb *sb) {
    if (!sb) return -1;
    if (sb->closed) return 0;
    if (!ready(sb) || sb->in_mix || sb->in_control) return -1;
    sb->closed = 1;
    sb->dig = NULL; sb->point = NULL;
    sb->global_user = NULL; sb->global_fill_up = NULL;
    return 0;
}
