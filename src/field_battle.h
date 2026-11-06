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
    FD2_FIELD_PHYSICAL_FINISH
} fd2_field_physical_phase;

typedef struct {
    fd2_field_physical_phase phase;
    size_t actor_index;
    size_t target_index;
    uint8_t counterattack;
    uint8_t strike_index;
    uint8_t pre_hit_status_applied;
    /* 本击提交前的只读记录；仅在同步callback期间有效，不可保留指针。
     * STRIKE的game已提交本击，present不能从最终HP反推此前状态。 */
    const fd2_field_unit *actor_before;
    const fd2_field_unit *target_before;
    const fd2_field_attack_result *result;
    int action_failed; /* FINISH即使此前失败也调用，以释放本次演出资源。 */
} fd2_field_physical_event;

/* PREPARE完整预检在本次首个业务RNG前；成功后恰一次FINISH。
 * STRIKE只呈现已算结果，不写game、不消费RNG、不再次调用effect。
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
    FD2_FIELD_MAGIC_PRESENT_FINISH
} fd2_field_magic_present_phase;

#define FD2_FIELD_MAGIC_IMPACT_CAPACITY 64u

typedef struct {
    fd2_field_magic_present_phase phase;
    size_t actor_index;
    uint8_t magic_id;
    uint8_t cast_cost_count; /* PREPARE前从真实caster intro导出；13固定wrapper一次。 */
    const uint8_t *targets; /* 仅同步调用期间借用完整目标序列。 */
    size_t target_count;
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
