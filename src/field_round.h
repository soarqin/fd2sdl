/* 10010活动Load与1a30b新玩家回合的共享横幅；不拥有业务或工作页。 */
#ifndef FD2_FIELD_ROUND_H
#define FD2_FIELD_ROUND_H

#include <stdint.h>
#include "field_info.h"
#include "field_page.h"

typedef struct {
    /* 只做本次已绘帧的等待/上传和hostquit检查，不消费普通键或推进
     * RNG/水DAC/idle。ms与ticks恰有一项非零；tick沿共享PIT边界。 */
    int (*wait)(fd2_vga *vga, uint32_t ms, unsigned ticks, void *userdata);
    /* 同一App工作页的纯可见区投影，不画VGA信息板、不推进时钟。
     * 初次drop前只调用一次；不可从intro的VGA图反灌工作页。 */
    int (*render_background)(fd2_vga *vga, void *userdata);
    /* 对应尾部11cac(0)，调用者决定其原视觉刷新/信息板双门。 */
    int (*refresh)(fd2_vga *vga, void *userdata);
    /* sub_1F30A尾仅11eee+127a9：重画原工作页，推进terrain/jitter，
     * 不调用1297d、不画信息板。随后render_background作纯投影。 */
    int (*phase_redraw)(fd2_vga *vga, void *userdata);
    /* sub_1F1CC尾bios clear，只清现输入脉冲，不做ACK/RNG。 */
    int (*clear_keys)(fd2_vga *vga, void *userdata);
} fd2_field_round_hooks;

/* 纯资源门，可在推进回合/Load owner交接前调用；不写任何状态。 */
int fd2_field_round_is_ready(const fd2_field_info_assets *assets);
/* 0完成/1宿主退出/-1资源、冷图或回调失败；无普通键skip/ACK。
 * 只写vga.framebuffer；保palette/input/App时钟，turn只读。
 * 所有顺序/等待参数来自runtime.round；曲目/acted/phase/status与
 * focus轮询索引仍归原caller，Load不得借此执行新回合事件。 */
int fd2_field_round_play(fd2_vga *vga, const fd2_field_info_assets *assets,
    uint32_t turn, const fd2_field_round_hooks *hooks, void *userdata);

typedef enum { FD2_FIELD_PHASE_PLAYER, FD2_FIELD_PHASE_ENEMY } fd2_field_phase_kind;
int fd2_field_phase_is_ready(const fd2_field_info_assets *assets);
/* 1f1cc→caller hold→1f30a的完整有限显示图；PLAYER80/150ms，ENEMY82/20ms。
 * page必须为本App同一53a49 owner；原明确定义的64000B prefix写入保留。
 * 只改framebuffer、DAC16..255与该prefix，base palette及业务只读。
 * hooks.phase_redraw是原有证工作页重画，不建立第二page或重算业务。 */
int fd2_field_phase_play(fd2_vga *vga, const fd2_field_info_assets *assets,
    fd2_field_page *page, fd2_field_phase_kind kind,
    const fd2_field_round_hooks *hooks, void *userdata);

/* 1a30b的四个已有scheduler返回点及原caller恢复门；图只编排有限领域op，不持有新的
 * phase游标、队伍、RNG或存档状态。ENTER_PHASE后的状态提示/AI仍由Game
 * 原progress/controller续态完成，再从对应caller入口继续。 */
typedef enum {
    FD2_FIELD_TURN_PLAYER_DONE,
    FD2_FIELD_TURN_SIDE1_DONE,
    FD2_FIELD_TURN_SIDE0_DONE,
    FD2_FIELD_TURN_PLAYER_READY,
    /* 13565/16f55在整轮正常或战果早返后恢复51AAC；不属于1a30b本体。 */
    FD2_FIELD_TURN_CALLER_RETURN,
    /* 134e4原helper及13a9f紧随的11cac(0)，共用有限执行器。 */
    FD2_FIELD_TURN_DIRECTIONS_RESET,
    FD2_FIELD_TURN_DIRECTIONS_RESET_REFRESH
} fd2_field_turn_entry;
typedef enum {
    FD2_FIELD_TURN_RECOVERY_WAIT,
    FD2_FIELD_TURN_RECOVERY_HIGHLIGHT,
    FD2_FIELD_TURN_RECOVERY_CUE,
    /* 原逐raw次序：同一actor HP提交→normal绘制→acted，随后下一actor。
     * 执行者只在这里调用一次组循环；不能按actor调用13fd4三wait。 */
    FD2_FIELD_TURN_RECOVERY_COMMIT,
    FD2_FIELD_TURN_RECOVERY_PROJECT,
    FD2_FIELD_TURN_REFRESH,
    FD2_FIELD_TURN_STOP_IF_DIFFERENT,
    FD2_FIELD_TURN_ENEMY_TITLE,
    FD2_FIELD_TURN_PLAYER_TITLE,
    FD2_FIELD_TURN_CLEAR_ACTED,
    FD2_FIELD_TURN_ENTER_PHASE,
    FD2_FIELD_TURN_INCREMENT,
    FD2_FIELD_TURN_DISPLAY_MODE,
    FD2_FIELD_TURN_PRIMARY_MUSIC,
    FD2_FIELD_TURN_ROUND,
    FD2_FIELD_TURN_FOCUS_RESET,
    FD2_FIELD_TURN_FOCUS_ZERO,
    FD2_FIELD_TURN_CLEAR_KEYS,
    FD2_FIELD_TURN_INFO_VISIBLE,
    FD2_FIELD_TURN_RESET_DIRECTIONS,
    FD2_FIELD_TURN_DELAY_MS
} fd2_field_turn_step;
typedef int (*fd2_field_turn_step_fn)(fd2_field_turn_step step,
    int value, void *userdata);
/* 0完整入口已执行，1宿主退出，-1定义/回调失败。冷图坏时不调用任何op。
 * callback只接上述有限操作，参数由静态定义校验；没有任意地址/脚本。 */
int fd2_field_turn_run(fd2_field_turn_entry entry,
    fd2_field_turn_step_fn apply, void *userdata);

struct fd2_field_game;
typedef enum {
    FD2_FIELD_RUNTIME_RECOVERY_WAIT,
    FD2_FIELD_RUNTIME_RECOVERY_HIGHLIGHT,
    FD2_FIELD_RUNTIME_RECOVERY_CUE,
    FD2_FIELD_RUNTIME_RECOVERY_NORMAL_ACTOR,
    FD2_FIELD_RUNTIME_RECOVERY_PROJECT,
    FD2_FIELD_RUNTIME_REFRESH,
    FD2_FIELD_RUNTIME_ENEMY_TITLE,
    FD2_FIELD_RUNTIME_PLAYER_TITLE,
    FD2_FIELD_RUNTIME_ROUND,
    FD2_FIELD_RUNTIME_CLEAR_KEYS,
    FD2_FIELD_RUNTIME_DEFEAT_BEGIN,
    FD2_FIELD_RUNTIME_DEFEAT_ROTATE_PAGE,
    FD2_FIELD_RUNTIME_DEFEAT_ROTATE_ACTOR,
    FD2_FIELD_RUNTIME_DEFEAT_ROTATE_PROJECT,
    FD2_FIELD_RUNTIME_DEFEAT_BASE,
    FD2_FIELD_RUNTIME_DEFEAT_CUE,
    FD2_FIELD_RUNTIME_DEFEAT_FRAME,
    FD2_FIELD_RUNTIME_DEFEAT_WAIT,
    /* 即使hostquit已锁存也须释放本次短寿命视觉资源；调用者直接清理。 */
    FD2_FIELD_RUNTIME_DEFEAT_CLOSE,
    FD2_FIELD_RUNTIME_CELL_OPEN_CUE,
    FD2_FIELD_RUNTIME_INFO_VISIBLE,
    FD2_FIELD_RUNTIME_DELAY_MS,
    FD2_FIELD_RUNTIME_MUSIC_TRACK,
    /* 12cea入口及方向helper的11cac(0)；value保原51A83。 */
    FD2_FIELD_RUNTIME_FOCUS_BEGIN,
    FD2_FIELD_RUNTIME_FOCUS_STEP,
    FD2_FIELD_RUNTIME_CURSOR_STEP,
    /* 15055的独立17aa9，不借恢复/回合资源或额外画面提交。 */
    FD2_FIELD_RUNTIME_WAIT_TICKS
} fd2_field_runtime_present_step;
/* Game借Play的同步呈现服务；没有第二份phase/HP/acted/RNG状态。
 * RECOVERY_WAIT.value=tick0/1；NORMAL_ACTOR.value=原raw索引；
 * DEFEAT_ROTATE_ACTOR.value=原raw索引，DEFEAT_FRAME.value=68..79，
 * DEFEAT_WAIT.value=1，CELL_OPEN_CUE.value=12，INFO_VISIBLE.value=0/1，DELAY_MS.value=20/200；
 * WAIT_TICKS.value=0..65535，仅等tick；REFRESH.value=0/3，3对应原11cac(1)并提交。
 * FOCUS_BEGIN/STEP/CURSOR_STEP.value=原mode；已证producer含7/9/11。
 * FOCUS_STEP仅mode!=0/6等1tick；每格CLEAR_KEYS由Game另调；CURSOR_STEP无wait/clear。
 * 其余value=0。
 * 返回0完成/1hostquit/-1失败。纯core未注册只执行已证业务，不冒充呈现。 */
typedef int (*fd2_field_runtime_present_fn)(struct fd2_field_game *game,
    fd2_field_runtime_present_step step, int value, void *userdata);

/* 19953→197e5共用确认框。选择/RNG/图像借现有caller，冷图拥有四步
 * 展开、等待循环、确认/取消边及四步收回；不把整段服务藏在handler。 */
typedef enum {
    FD2_FIELD_CHOICE_BEGIN,
    FD2_FIELD_CHOICE_DRAW,
    FD2_FIELD_CHOICE_WAIT_KEY,
    FD2_FIELD_CHOICE_READ_KEY,
    FD2_FIELD_CHOICE_TICK,
    FD2_FIELD_CHOICE_FINISH,
    FD2_FIELD_CHOICE_SNAPSHOT,
    FD2_FIELD_CHOICE_HAS_FIELD,
    FD2_FIELD_CHOICE_FIELD_REDRAW,
    FD2_FIELD_CHOICE_BLINK,
    FD2_FIELD_CHOICE_STEADY
} fd2_field_choice_step;
/* 常规step返回0成功；WAIT_KEY返回0无键/1有键；TICK返回0未到门/1已到门；
 * HAS_FIELD返回原53A51>1，不从portrait位置推断。READ_KEY返回0继续、
 * 1确认、2取消。所有step均可返回3hostquit、负数错误。DRAW.value为
 * 按钮位移4..16或12..0，steady由TICK投影，不能由业务owner推进。 */
typedef int (*fd2_field_choice_step_fn)(fd2_field_choice_step step,
    int value, void *userdata);
int fd2_field_choice_run(fd2_field_choice_step_fn apply,void *userdata);

#endif
