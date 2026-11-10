/* G2动作呈现：sub_2E2B0 @0x2e2b0、sub_2EBE1 @0x2ebe1、
 * sub_2E9A8 @0x2e9a8、field_magic_battle_animation @0x2ff01（原同名）、
 * sub_1C4CC @0x1c4cc、field_derived_magic_full_execute @0x2cf30（原sub_2CF30）、
 * field_composite_magic_full_execute @0x2d80d（原sub_2D80D）、
 * sub_22253 @0x22253及field_item_use_effect_execute @0x20c6f（原sub_20C6F）。
 * SHORT/恢复：field_physical_exchange @0x1548e（原同名）、field_short_strike_present
 * @0x1e98c（原sub_1E98C）、field_short_counter_refresh @0x1e611（原sub_1E611）、
 * field_stationary_recover_present_apply @0x13fd4（原sub_13FD4），只读消费Game冷阶段。
 * 业务只读同步服务；视觉窄口借Game，业务/RNG由Game提交。 */
#ifndef FD2_BATTLE_ANIMATION_H
#define FD2_BATTLE_ANIMATION_H

#include "archive.h"
#include "audio_pcm.h"
#include "field_battle.h"
#include "vga.h"

typedef struct fd2_battle_animation_exchange fd2_battle_animation_exchange;
typedef struct fd2_battle_magic_exchange fd2_battle_magic_exchange;

/* 1c2da→4e127的有效地图常色，只读唯一冷定义；与宿主framebuffer stride无关。 */
uint8_t fd2_battle_animation_map_solid_color(void);
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

/* ctx首次调用前清零；这里只绑定借用，BG/TAI仅FULL PREPARE按需打开。
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
/* FULL/SHORT/RECOVERY共用此回调；资源-only PREPARE立即释放且不绘图。
 * SHORT_MAP、BEGIN、COUNTER_REFRESH及RECOVERY_STEP由Game冷图明确调度；
 * 失败后仍恰一次FINISH。正常尾只读重画当前地图，错误/quit恢复进入快照。 */
int fd2_battle_animation_physical_present(const struct fd2_field_game *game,
    const fd2_field_physical_event *event, void *userdata);

/* FULL0..9、地图core、DERIVED逐part及COMPOSITE冷图分组；资格由Game决定。
 * PREPARE验证实际资源，STEP/四段teleport消费Game明确写点，TARGET不再effect。
 * Item按原code使用地图本体，POPUP_GROUP先于重算／压槽；无Magic大mask／重复费用。
 * FX用同一纯engine只读重放每段记录，不调用Game RNG或改原单位／存档owner。 */
int fd2_battle_animation_magic_present(const struct fd2_field_game *game,
    const fd2_field_magic_present_event *event, void *userdata);

/* 2fb2c/2fe14：准备期转职只借App资源和VGA，不建立Game或业务owner。
 * open耗物前完成所有clip/BG解析且不改VGA；冷图决定阶段/次数，音乐仍归caller。
 * 无业务RNG/音效；Esc不构成skip，host quit返回-2，其它错误-1。
 * BEGIN value=等待tick；ENTER=入场帧数；IDLE=循环tick；WHITE=渐白帧数；
 * NEW_FRAME=水平偏移；RESTORE须value0。默认源参数1/9/16/20/200/24/0。
 * close(restore=1)只恢复视觉快照，不能回滚调用方已提交的物品/身份。 */
/* 原合法16→48的FIGANI144为empty：仅省缺失sprite/目录读取，保BG/等待；
 * 这是原owner外图形读取的安全扩展，不宣称其未定义像素等价。 */
typedef struct fd2_prepare_promotion fd2_prepare_promotion;
typedef enum {
    FD2_PREPARE_PROMOTION_BEGIN,
    FD2_PREPARE_PROMOTION_OLD_ENTER,
    FD2_PREPARE_PROMOTION_OLD_IDLE,
    FD2_PREPARE_PROMOTION_WHITE,
    FD2_PREPARE_PROMOTION_NEW_FRAME,
    FD2_PREPARE_PROMOTION_NEW_IDLE,
    FD2_PREPARE_PROMOTION_RESTORE
} fd2_prepare_promotion_phase;
int fd2_prepare_promotion_open(fd2_prepare_promotion **out, fd2_vga *vga,
    const fd2_archive *backgrounds, const fd2_archive *figani,
    uint8_t old_unit_id, uint8_t new_unit_id);
int fd2_prepare_promotion_step(fd2_prepare_promotion *scope,
    fd2_prepare_promotion_phase phase, unsigned value);
void fd2_prepare_promotion_close(fd2_prepare_promotion *scope, int restore);

#endif
