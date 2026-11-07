/* B6物理、FULL0..9及基本13演出：sub_2E2B0 @0x2e2b0、sub_2EBE1 @0x2ebe1、
 * sub_2E9A8 @0x2e9a8、field_magic_battle_animation @0x2ff01（原同名）、
 * sub_21B18 @0x21b18。业务只读同步服务；视觉窄口借Game，业务/RNG由Game提交。 */
#ifndef FD2_BATTLE_ANIMATION_H
#define FD2_BATTLE_ANIMATION_H

#include "archive.h"
#include "audio_pcm.h"
#include "field_battle.h"
#include "vga.h"

typedef struct fd2_battle_animation_exchange fd2_battle_animation_exchange;
typedef struct fd2_battle_magic_exchange fd2_battle_magic_exchange;
typedef struct {
    fd2_vga *vga;
    struct fd2_field_game *visual_owner; /* Play借入真实Game；只经tick_visual写四视觉字段。 */
    fd2_pcm_player *primary;
    fd2_pcm_player *secondary;
    const fd2_archive *fdother; /* 借用App；须晚于服务和全部PCM voice关闭。 */
    const fd2_archive *figani; /* 借用Game字段地址；活动exchange期间禁止Load/close。 */
    fd2_archive backgrounds;
    fd2_archive terrain_images;
    fd2_battle_animation_exchange *exchange;
    fd2_battle_magic_exchange *magic_exchange;
    int skip;
    int host_quit;
    int failed; /* 任一present错误锁存；Play在资源拒绝后走ERROR，不作为noop。 */
} fd2_battle_animation;

/* ctx首次调用前清零；这里只绑定借用，BG/TAI在首个物理/FULL PREPARE按需打开。
 * 不创建音频/窗口，不改变VGA或Game。NULL player表示无声，仍验证cue资源。
 * 每动作按51E62(game.option_values[1])快照门控cue/stop，关音效不略资源预检。
 * visual_owner必须是回调收到的同一Game，服务关闭须早于该Game资源释放。 */
int fd2_battle_animation_open(fd2_battle_animation *ctx, fd2_vga *vga,
    fd2_pcm_player *primary, fd2_pcm_player *secondary,
    const fd2_archive *fdother, const fd2_archive *figani,
    struct fd2_field_game *visual_owner);
/* full-mode skip只跳画帧/cue，不能切换Game攻击次数模式。 */
void fd2_battle_animation_set_skip(fd2_battle_animation *ctx, int skip);
void fd2_battle_animation_close(fd2_battle_animation *ctx);
/* 可直接注册为fd2_field_physical_present_fn；失败后仍接收一次FINISH。
 * 正常尾以Game只读battle_return重画当前地图；错误/quit恢复进入前快照。 */
int fd2_battle_animation_physical_present(const struct fd2_field_game *game,
    const fd2_field_physical_event *event, void *userdata);

/* FULL0..9及基本13；资格仍由Game决定，不在呈现层新增caster限制。
 * FX用同一纯engine只读重放每段记录，TARGET不再次effect或调用Game RNG。 */
int fd2_battle_animation_magic_present(const struct fd2_field_game *game,
    const fd2_field_magic_present_event *event, void *userdata);

#endif
