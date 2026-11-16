/* B6：sub_2E2B0 @0x2e2b0 / sub_2EBE1 @0x2ebe1 的只读同步演出边界。
 * 结算与共享RNG仍由field_game唯一提交；这里不拥有游戏或资源，不进入存档。 */
#ifndef FD2_FIELD_BATTLE_H
#define FD2_FIELD_BATTLE_H

#include <stddef.h>
#include <stdint.h>
#include "field_unit.h"
#include "field_combat.h"
#include "field_magic.h"

struct fd2_field_game;
typedef enum {
    FD2_FIELD_PHYSICAL_PREPARE = 0,
    FD2_FIELD_PHYSICAL_STRIKE,
    FD2_FIELD_PHYSICAL_FINISH,
    FD2_FIELD_PHYSICAL_SHORT_BEGIN,
    FD2_FIELD_PHYSICAL_COUNTER_REFRESH,
    FD2_FIELD_PHYSICAL_RECOVERY_STEP,
    FD2_FIELD_PHYSICAL_SHORT_MAP
} fd2_field_physical_phase;

typedef enum {
    FD2_FIELD_PHYSICAL_MODE_FULL = 0,
    FD2_FIELD_PHYSICAL_MODE_SHORT,
    FD2_FIELD_PHYSICAL_MODE_RECOVERY
} fd2_field_physical_present_mode;

typedef enum {
    FD2_FIELD_RECOVERY_WAIT = 0,
    FD2_FIELD_RECOVERY_CUE,
    FD2_FIELD_RECOVERY_ACTOR
} fd2_field_recovery_step;

typedef struct {
    fd2_field_physical_phase phase;
    fd2_field_physical_present_mode presentation_mode;
    size_t actor_index;
    size_t target_index;
    uint8_t counterattack;
    uint8_t strike_index;
    uint8_t pre_hit_status_applied;
    /* 本击提交前的只读记录；仅在同步callback期间有效，不可保留指针。
     * STRIKE的game已提交本击，present不能从最终HP反推此前状态。
     * resource_only PREPARE可借用同身份的目的格投影，检查未来BG/TAI环境。 */
    const fd2_field_unit *actor_before;
    const fd2_field_unit *target_before;
    const fd2_field_attack_result *result;
    int action_failed; /* FINISH即使此前失败也调用，以释放本次演出资源。 */
    /* AI后续恢复在首动作RNG前只读预检；成功即释放借用scope，不绘图或写owner。 */
    uint8_t resource_only;
    /* RECOVERY_STEP的顺序、等待与参数来自stationary.recover冷图。
     * WAIT为BIOS tick；CUE为field bank索引；ACTOR为1da16模式2/0。 */
    fd2_field_recovery_step recovery_step;
    uint16_t step_value;
} fd2_field_physical_event;

/* PREPARE完整预检在本次首个业务RNG前；成功后恰一次FINISH。
 * 普通STRIKE只呈现已算结果，不写game、不消费RNG、不再次调用effect。
 * RECOVERY在STEP三次等待后才由Game写HP；没有伤害result或浮字。
 * 返回0成功/-1呈现失败；提交后的失败不得回滚或重新尝试动作。
 * 宿主quit由present owner锁存并停止后续画帧，原子动作完成后由Play分派。
 * full-mode skip仅跳过画帧/cue，不能改成原版short-mode的结算规则。 */
typedef int (*fd2_field_physical_present_fn)(
    const struct fd2_field_game *game,
    const fd2_field_physical_event *event, void *userdata);

/* field_magic_battle_animation @0x2ff01：玩家0/AI FULL0..9；21b18地图治疗13。
 * 资源准备、caster费用呈现、逐目标已算结果分别同步；无队列/额外owner。 */
typedef enum {
    FD2_FIELD_MAGIC_PRESENT_PREPARE = 0,
    FD2_FIELD_MAGIC_PRESENT_CAST,
    FD2_FIELD_MAGIC_PRESENT_TARGET,
    FD2_FIELD_MAGIC_PRESENT_FINISH,
    FD2_FIELD_MAGIC_PRESENT_STEP,
    FD2_FIELD_MAGIC_PRESENT_TELEPORT_OUT,
    FD2_FIELD_MAGIC_PRESENT_TELEPORT_IN,
    FD2_FIELD_MAGIC_PRESENT_POPUP_CLEAR,
    FD2_FIELD_MAGIC_PRESENT_TELEPORT_OUT_COMMIT,
    FD2_FIELD_MAGIC_PRESENT_TELEPORT_IN_COMMIT,
    FD2_FIELD_MAGIC_PRESENT_POPUP_GROUP,
    /* 1cff0→1d6c8：双selector成功后的玩家MAP成本前缀，不用于AI/Item/FULL。
     * 无exchange：0成功/skip、1HOST、-1错误；不触业务/RNG，异常保VGA前缀。 */
    FD2_FIELD_MAGIC_PRESENT_MAP_CALLER_PREAMBLE,
    /* 15055射线物品：2dfc8 caster场景→2189a(80,-4)→200ms/渐白。
     * target_count同时是原EDI的part起点；无需exchange/第二业务状态。
     * 不执行后续20c6f地图效果；0成功/skip、1HOST、-1错误。 */
    FD2_FIELD_MAGIC_PRESENT_AI_RAY_ITEM,
    /* 2185f/21a9e：只读radial先于21548内1ca89扣费，后继CAST重放quake。
     * 仅普通MAP Magic11/12；不用于Item/DERIVED/COMPOSITE，不增加业务owner。
     * 此相位及普通MAP10..12的CAST/TARGET：0成功/skip、1HOST、-1真实ERROR；
     * Game锁HOST、保已达费用/HP/RNG并FINISH正常清理；FINISH不输出HOST。 */
    FD2_FIELD_MAGIC_PRESENT_MAP_WRAPPER_PRELUDE
} fd2_field_magic_present_phase;

typedef enum {
    FD2_FIELD_MAGIC_MODE_FULL = 0,
    FD2_FIELD_MAGIC_MODE_MAP,
    FD2_FIELD_MAGIC_MODE_DERIVED,
    FD2_FIELD_MAGIC_MODE_COMPOSITE
} fd2_field_magic_present_mode;

typedef enum {
    FD2_FIELD_MAGIC_ACTION_MAGIC = 0,
    FD2_FIELD_MAGIC_ACTION_ITEM
} fd2_field_magic_action_kind;

#define FD2_FIELD_MAGIC_IMPACT_CAPACITY 64u

typedef struct {
    fd2_field_magic_present_phase phase;
    fd2_field_magic_present_mode presentation_mode;
    fd2_field_magic_action_kind action_kind;
    size_t actor_index;
    uint8_t magic_id;
    uint8_t effect_id;
    uint8_t item_code;
    uint16_t item_power;
    uint8_t group_pass;
    uint16_t part_index;
    uint8_t marker_count;
    const fd2_field_unit *target_after;
    int destination_x, destination_y;
    uint8_t cast_cost_count; /* PREPARE前从真实caster intro导出；13固定wrapper一次。 */
    const uint8_t *targets; /* 仅同步调用期间借用完整目标序列。 */
    size_t target_count;
    size_t effect_target_count; /* 0=全部；MAP9/永久stat仅首项，FX仍使用完整目标组。 */
    size_t target_index;
    const fd2_field_unit *actor_before;
    const fd2_field_unit *target_before;
    /* effect的raw结果；FULL会先恢复before再插值，部分FX后项可能不写HP。
     * 因而result.hp_after不能冒充此callback时Game中的最终HP。 */
    const fd2_field_magic_basic_result *result;
    /* FULL的raw_damage非零且phase5==1时按原顺序记录1-rand()%3。
     * HP按原分母封顶后仍消费后续jitter；impact数不等于HP分母。
     * raw0及Magic13为0项；present只读，不能消费真实RNG。 */
    int8_t impact_jitter[FD2_FIELD_MAGIC_IMPACT_CAPACITY];
    uint8_t impact_count;
    /* 原FX自身的共享RNG(4/5)片段；effect/jitter不混入。仅同步callback借用，
     * renderer经同一纯engine的只读reader重放，不能调用Game RNG。 */
    const uint32_t *fx_random;
    size_t fx_random_count;
    int action_failed;
} fd2_field_magic_present_event;

typedef int (*fd2_field_magic_present_fn)(
    const struct fd2_field_game *game,
    const fd2_field_magic_present_event *event, void *userdata);

#endif /* FD2_FIELD_BATTLE_H */
