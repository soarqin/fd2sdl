#ifndef FD2_FIELD_PLAY_H
#define FD2_FIELD_PLAY_H

#include <stddef.h>

#include "archive.h"
#include "bgm.h"
#include "field_audio.h"
#include "field_handoff.h"
#include "field_rng.h"
#include "save.h"
#include "vga.h"

typedef enum {
    FD2_FIELD_PLAY_RETURN_COMPLETE = 0,
    FD2_FIELD_PLAY_RETURN_LEAVE_GAME,
    FD2_FIELD_PLAY_RETURN_HOST_QUIT,
    FD2_FIELD_PLAY_RETURN_ERROR,
    FD2_FIELD_PLAY_RETURN_VICTORY,
    FD2_FIELD_PLAY_RETURN_DEFEAT
} fd2_field_play_result;

/* 22ef6→11506→26152的应用期交接。准备页独占完整persistent表；战场
 * 运行时该副本休眠，active仍只归game。三个运行期值不进入原版SAV。 */
typedef struct {
    fd2_save_manual_state preparation;
    fd2_field_rng rng;
    uint32_t experience_value;
    uint32_t message_number;
    fd2_bgm_player *bgm; /* 借App，关闭战场/准备页都不销毁播放器。 */
    int has_preparation;
    int initialized;
    int music_error;
} fd2_field_play_context;

void fd2_field_play_context_init(fd2_field_play_context *context,
                                 fd2_bgm_player *bgm);
/* 仅发起音乐请求，不推进战场；-1沿已有播放器请求4秒异步fade。 */
int fd2_field_play_request_music(fd2_field_play_context *context,
    size_t stage, int track, const char *reason);

/* COMPLETE 只表示 once 快速检查结束；正式战斗分别返回 VICTORY/DEFEAT。
 * LEAVE_GAME 对应 field_secondary_menu_execute @0x19df7 的肯定离场，
 * 最终退出游戏；与宿主关闭 HOST_QUIT 保持独立来源。
 * 正式战场循环入口。handoff 可为空；非空时把开场结束后的动态状态
 * 应用到从 FDFIELD 建立的正式 session。 */
fd2_field_play_result fd2_field_play_run(fd2_vga *vga,
                       const fd2_archive *fdother,
                       size_t stage,
                       int once,
                       const fd2_field_handoff *handoff,
                       fd2_field_audio *field_audio,
                       const char *save_path,
                       int load_active);

/* 正式App入口借同一RNG/动态数字，并在首关结算完整播放后交接准备owner。
 * 旧入口用于独立战场/once检查，不把其enum返回当作已执行跨关。 */
fd2_field_play_result fd2_field_play_run_context(fd2_vga *vga,
                       const fd2_archive *fdother, size_t stage, int once,
                       const fd2_field_handoff *handoff,
                       fd2_field_audio *field_audio, const char *save_path,
                       int load_active, fd2_field_play_context *context);

/* 可达的实时时序验收入口：依次运行三个原版 wrapper，包含 PCM、
 * present deadline、尾停顿和转场渐暗。 */
int fd2_field_effect_play_run(fd2_vga *vga,
                              const fd2_archive *fdother,
                              size_t stage,
                              fd2_field_audio *field_audio);

#endif /* FD2_FIELD_PLAY_H */
