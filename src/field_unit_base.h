#ifndef FD2_FIELD_UNIT_BASE_H
#define FD2_FIELD_UNIT_BASE_H

#include <stdint.h>

#include "field_unit.h"
#include "field_unit_stats.h"

/* field_growth_record_get @0x4e821（原sub_4E821）与 field_magic_learning_record_get @0x4e7f2（原sub_4E7F2）
 * 的完整已证只读数据：unit_id 对应11B成长row，row+10选择12B学习row。
 * 68/20是当前数据读集范围，不代表所有ID均可作为玩家运行中升级。 */
const uint8_t *fd2_field_unit_growth_record_get(uint8_t unit_id);
const uint8_t *fd2_field_unit_learning_record_get(uint8_t selector);

typedef struct {
    uint8_t level;
    uint8_t growth[5]; /* baseAP/baseDP/DX/maxHP/maxMP，取每对第二byte。 */
    uint8_t learned_magic[6]; /* 按原学习表顺序；已拥有的bit仍报告。 */
    uint8_t learned_count;
} fd2_field_unit_level_growth;

/* field_growth_stat_present_apply @0x1e529（原sub_1E529）及 field_unit_magic_bit_set @0x1d79c（原sub_1D79C）
 * 的纯数据预计算，不执行levelcap或奖励入口门。支持已证成长row ID0..67；
 * 下一等级按byte回绕。未知学习ID（含level255命中ff哨兵）明确拒绝。
 * 成功0，只写result；失败-1，unit/result均不变。无RNG、UI或派生重算。 */
int fd2_field_unit_level_growth_prepare(
    const fd2_field_unit *unit, fd2_field_unit_level_growth *result);

/* field_unit_experience_reward_execute @0x1e292（原sub_1E292）的纯终态预检，不代替原提示前后逐步写序。
 * 0=原入口早退（gain0/hidden/精确等值levelcap），1=写入out，-1=SDL拒绝。
 * 0/-1均不写out；source在out不是其显式别名时不变，支持out==source。
 * gain按已证非负signed-int
 * 范围检查，不给玩家加cap99：caller仍负责117e7/1548e各自归属和贡献生命周期。
 * 活跃奖励仅支持已有成长row的ID0..67；逐级WORD相加、学习OR和既有装备派生在80B副本完成。
 * 当前HP/MP、未知字节不改；状态缩放借既有回调。没有RNG、音画、53EC8或session写入。 */
int fd2_field_unit_reward_preview(
    const fd2_field_unit *source, uint32_t gain,
    fd2_field_unit_stat_scale_fn scale, void *scale_userdata,
    fd2_field_unit *out);

/* 按原版静态角色/敌军基础表和等级公式写入单位基础战斗字段。
 * 当前登记已由 stage 0、新游戏存档或运行时构造器交叉验证的 unit ID；
 * 未登记 ID 返回 -1，且不修改记录。 */
int fd2_field_unit_base_apply(fd2_field_unit *unit, uint8_t level);

/* field_enemy_base_record_get @0x4e84f（原登记sub_4E84F）：object3 VA0x61af9的68条
 * 10-byte敌军record，sub_1C81F @0x1c81f读取末字节经验系数。
 * 只支持unit ID68..135；失败不写coefficient，不猜未知ID的系数。 */
int fd2_field_unit_enemy_experience_coefficient(
    uint8_t unit_id, uint8_t *coefficient);

/* field_persistent_unit_combat_stats_recompute @0x1145a（原sub_1145A）的持久记录装备派生：仅写48..4f的四个WORD，
 * 忽略战场状态22..24，遍历全部8槽的flags40（包括同时flags80的槽）。
 * 不构造单位、不改其余72B；成功0，NULL或未知已装备ID失败-1且整记录不变。 */
int fd2_field_unit_persistent_stats_recompute(fd2_field_unit *unit);

/* sub_112A5 @0x112a5（原登记同名）的持久玩家初始构造及
 * field_persistent_unit_combat_stats_recompute @0x1145a（原sub_1145A） 装备派生。只改已确认写集；未写 raw 字节沿用
 * 输入记录。支持原版 player base 的 ID 0..31，失败时不修改记录。 */
int fd2_field_unit_persistent_create(fd2_field_unit *unit, uint8_t unit_id);

/* 将 FDFIELD 26 字节模板按原版 stage constructor 重排到新单位记录，
 * 并调用基础表等级派生。保留调用方预设的 x/y；失败时不修改记录。 */
int fd2_field_unit_stage_template_apply(
    fd2_field_unit *unit,
    const fd2_field_unit_template *template_record);

#endif /* FD2_FIELD_UNIT_BASE_H */
