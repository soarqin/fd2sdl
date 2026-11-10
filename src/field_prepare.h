/* G3准备领域：原持久队伍/选阵、2a43e复活、2aa00/2ac7d转职。
 * 只借fd2_save_manual_state；分支/等待/ACK/资金及资源提交顺序归准备冷图。
 * 不新建队伍owner、不推进RNG，不执行SDL输入/呈现或战场行动尾。 */
#ifndef FD2_FIELD_PREPARE_H
#define FD2_FIELD_PREPARE_H

#include <stddef.h>
#include <stdint.h>
#include "save.h"
#include "field_unit.h"

#define FD2_FIELD_PREPARE_COMPANION_CAPACITY (FD2_SAVE_MAX_UNITS - 1u)

typedef enum {
    FD2_FIELD_PREPARE_SELECT_LEFT=0,
    FD2_FIELD_PREPARE_SELECT_RIGHT,
    FD2_FIELD_PREPARE_SELECT_UP,
    FD2_FIELD_PREPARE_SELECT_DOWN,
    FD2_FIELD_PREPARE_SELECT_TOGGLE
} fd2_field_prepare_select_key;

/* 原2af28仅清30byte。32人记录容量的第31同伴位采用显式全零安全扩展，
 * 不读取未初始化栈，不截断完整roster。bitmap/cursor均为UI临时状态。 */
int fd2_field_prepare_selection_init(uint8_t selected[FD2_FIELD_PREPARE_COMPANION_CAPACITY]);
/* 1=方向/切换被处理，0=上下越界，-1=参数/位图错误；拒绝不写。
 * Enter切换后按原右移，左右即使单格仍返回1供caller播放原SFX。 */
int fd2_field_prepare_selection_step(size_t roster_count,
    uint8_t selected[FD2_FIELD_PREPARE_COMPANION_CAPACITY], int *cursor,
    fd2_field_prepare_select_key key);
int fd2_field_prepare_selection_count(size_t roster_count,
    const uint8_t selected[FD2_FIELD_PREPARE_COMPANION_CAPACITY], size_t *count);
/* capacity为原16/20总人数，需已选capacity-1且roster足够。完整a00稳定重排，
 * raw0/count/未用尾不变；调用即提交，最终NO/缺必须成员不能倒回旧顺序。 */
int fd2_field_prepare_roster_reorder(fd2_save_manual_state *state,
    const uint8_t selected[FD2_FIELD_PREPARE_COMPANION_CAPACITY], size_t capacity);
/* 2b439扫描raw1..capacity-1，不筛hidden/HP。1有/0无/-1坏参数。 */
int fd2_field_prepare_prefix_has_text(const fd2_save_manual_state *state,
    size_t capacity, uint8_t text_id);
/* 2b843将找到的raw1..成员稳定移到raw1；1找到/0未找到/-1坏参数。 */
int fd2_field_prepare_roster_front(fd2_save_manual_state *state, uint8_t text_id);
/* 1088d的部署投影，不运行entry/handler或开放stage。metadata byte1给active
 * 名额；位置与roster只在实际拷贝时消费，冷gate可留空但不推进两个指针。
 * 成功才写out，不改完整persistent表；FDICON资源就绪由Game/UI caller预检。 */
int fd2_field_prepare_deployment_project(const fd2_save_manual_state *state,
    size_t stage, const fd2_field_map *map, const fd2_field_metadata *metadata,
    const fd2_field_placements *placements, fd2_field_units *out);

/* 查询均返回0/-1；输出容量不足或坏state时不改输出。不加HP/acted门。 */
int fd2_field_prepare_revive_candidates(const fd2_save_manual_state *state,
    uint8_t *actors, size_t capacity, size_t *count);
int fd2_field_prepare_revive_price(const fd2_save_manual_state *state,
    size_t actor_index, uint32_t *price);
/* 冷图先独立扣费，再清整flags byte，再HP=max；各函数只写所列字段。
 * clear要求仍hidden；restore要求flags已清0。重复扣费防护由flow游标负责。 */
int fd2_field_prepare_revive_clear_flags(fd2_save_manual_state *state, size_t actor_index);
int fd2_field_prepare_revive_restore_hp(fd2_save_manual_state *state, size_t actor_index);

typedef struct {
    uint8_t actor_index, old_unit_id, target_unit_id;
    int8_t special_slot; /* -1普通；其它为原2aedb的raw槽，不是压缩可见序号。 */
} fd2_field_prepare_promotion;

/* 候选原顺序：level>=20、ID<18且非7；不筛hidden/HP/acted。
 * 2aedb先数可见槽，再直接查raw[0..count-1]，保稀疏stale ID原读义。
 * target52消费item90，找不到时拒绝原remove(-1)无定义写，不猜其它槽。 */
int fd2_field_prepare_promotion_plan(const fd2_save_manual_state *state,
    size_t actor_index, fd2_field_prepare_promotion *plan);
int fd2_field_prepare_promotion_candidates(const fd2_save_manual_state *state,
    uint8_t *actors, size_t capacity, size_t *count);
/* plan由本API生成并保持至该操作结束；consume重查当前候选/槽，identity
 * 在consume及演出后仅写profile/unit ID。text ID、技能与其余80B不重造。 */
int fd2_field_prepare_promotion_consume(fd2_save_manual_state *state,
    const fd2_field_prepare_promotion *plan);
int fd2_field_prepare_promotion_identity(fd2_save_manual_state *state,
    const fd2_field_prepare_promotion *plan);

typedef enum {
    FD2_FIELD_PREPARE_GAIN_ATTACK=0,
    FD2_FIELD_PREPARE_GAIN_DEFENSE,
    FD2_FIELD_PREPARE_GAIN_DEXTERITY,
    FD2_FIELD_PREPARE_GAIN_HP_MAX,
    FD2_FIELD_PREPARE_GAIN_MP_MAX,
    FD2_FIELD_PREPARE_GAIN_MOVEMENT
} fd2_field_prepare_gain;

/* 新ID静态增量查询供资源预检/提示；apply仅在对应ACK后调用一次。
 * 顺序/零增量跳过/ACK防重复由冷图，不在这里藏另一套推进器。 */
int fd2_field_prepare_promotion_gain_value(const fd2_field_prepare_promotion *plan,
    fd2_field_prepare_gain kind, uint16_t *value);
int fd2_field_prepare_promotion_gain_apply(fd2_save_manual_state *state,
    const fd2_field_prepare_promotion *plan, fd2_field_prepare_gain kind);
/* 所有ACK后先由caller调用既有1b750重算，再关底框，最后此步写level1/
 * EX0/当前HPMP=max。无学习/重算/整条覆盖；完成后再次apply/finalize拒绝。 */
int fd2_field_prepare_promotion_finalize(fd2_save_manual_state *state,
    const fd2_field_prepare_promotion *plan);

#endif
