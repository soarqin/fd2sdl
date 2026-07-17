#ifndef FD2_SCENE_H
#define FD2_SCENE_H

#include "archive.h"
#include "bgm.h"
#include "field_audio.h"
#include "field_game.h"
#include "field_handoff.h"
#include "vga.h"

typedef enum {
    FD2_SCENE_RESULT_OK = 0,
    FD2_SCENE_RESULT_ERROR = -1,
    FD2_SCENE_RESULT_HOST_QUIT = -2
} fd2_scene_result;

/* 完整新游戏初始过场预览。
 *
 * 按 new_game_opening_play @code0 0x2231b 播放 stage 32、31、0 的全部
 * 开场镜头、移动、对白和登场特效，在第一关正式交还控制前结束。
 * 资源来自 FDFIELD/FDSHAP、FDICON.B24、DATO、FDTXT 与 FDOTHER。
 */
int fd2_scene_play_new_game_prologue(fd2_vga *vga,
                                     const fd2_archive *fdother,
                                     fd2_bgm_player *bgm,
                                     fd2_field_audio *audio,
                                     int once);

/* 与预览入口播放相同过场，并在成功结束时导出 stage 0 动态状态。 */
int fd2_scene_play_new_game_prologue_handoff(
        fd2_vga *vga,
        const fd2_archive *fdother,
        fd2_bgm_player *bgm,
        fd2_field_audio *audio,
        int once,
        fd2_field_handoff *handoff);

/* 播放 stage 0/31 turn action 的镜头、登场 LMI1、移动和 FDTXT 对话。
 * notice 中保存状态事务提交前的单位数与镜头；成功后清除 deferred。 */
int fd2_scene_play_field_event(fd2_vga *vga,
                               const fd2_archive *fdother,
                               fd2_field_game *game,
                               fd2_field_event_notice *notice,
                               fd2_field_audio *audio,
                               int fast);

#endif /* FD2_SCENE_H */
