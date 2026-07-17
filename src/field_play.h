#ifndef FD2_FIELD_PLAY_H
#define FD2_FIELD_PLAY_H

#include <stddef.h>

#include "archive.h"
#include "field_audio.h"
#include "field_handoff.h"
#include "vga.h"

typedef enum {
    FD2_FIELD_PLAY_RETURN_COMPLETE = 0,
    FD2_FIELD_PLAY_RETURN_TITLE,
    FD2_FIELD_PLAY_RETURN_HOST_QUIT,
    FD2_FIELD_PLAY_RETURN_ERROR
} fd2_field_play_result;

/* 正式战场循环入口。handoff 可为空；非空时把开场结束后的动态状态
 * 应用到从 FDFIELD 建立的正式 session。 */
fd2_field_play_result fd2_field_play_run(fd2_vga *vga,
                       const fd2_archive *fdother,
                       size_t stage,
                       int once,
                       const fd2_field_handoff *handoff,
                       fd2_field_audio *field_audio,
                       const char *save_path,
                       int load_active);

/* 可达的实时时序验收入口：依次运行三个原版 wrapper，包含 PCM、
 * present deadline、尾停顿和转场渐暗。 */
int fd2_field_effect_play_run(fd2_vga *vga,
                              const fd2_archive *fdother,
                              size_t stage,
                              fd2_field_audio *field_audio);

#endif /* FD2_FIELD_PLAY_H */
