#ifndef FD2_FIELD_MAGIC_H
#define FD2_FIELD_MAGIC_H

#include <stddef.h>
#include <stdint.h>

#include "field_unit.h"

#define FD2_FIELD_MAGIC_RECORD_SIZE 7u
#define FD2_FIELD_MAGIC_COUNT 36u
#define FD2_FIELD_MAGIC_DAMAGE_PROFILE_COUNT 28u

/* field_unit_magic_list_build @0x1c269（原登记同名）：按actor
 * +0x1a..0x1e的40个bit递增枚举，保留超出36条record表的未知ID。
 * 返回总数；output为NULL只计数，否则最多写capacity个ID。capacity是
 * SDL的缓冲边界，不是原版ABI。空unit返回0且不写output。
 * 不检查状态/MP，不提交消耗、acted、phase、pending或RNG。 */
size_t fd2_field_unit_magic_list_build(
    const fd2_field_unit *unit, uint8_t *magic_ids, size_t capacity);

/* SDL动作枚举，不是BIOS扫描码；Enter/Space由宿主adapter统一为CONFIRM。 */
typedef enum {
    FD2_FIELD_MAGIC_LIST_KEY_OTHER = 0,
    FD2_FIELD_MAGIC_LIST_KEY_UP,
    FD2_FIELD_MAGIC_LIST_KEY_DOWN,
    FD2_FIELD_MAGIC_LIST_KEY_LEFT,
    FD2_FIELD_MAGIC_LIST_KEY_RIGHT,
    FD2_FIELD_MAGIC_LIST_KEY_CONFIRM,
    FD2_FIELD_MAGIC_LIST_KEY_CANCEL
} fd2_field_magic_list_key;

/* field_player_magic_list_select_step @0x1d51d（原登记sub_1D51D）
 * 的纯按键部分：0继续、-1取消、1确认；SDL数据／参数拒绝返回-2。
 * selected与confirmed_id均须非NULL；只有导航写selected，确认成功才写ID。
 * 预检完整40bit列表，保留原版全表回绕；12项仅是renderer的可见能力门。
 * 不修改unit，不扣MP，不执行16c57等待／UI共享RNG，后者由play owner负责。
 * 返回值不是法术ID，也不是完整native函数或宿主退出结果。 */
int fd2_field_magic_list_select_step(
    const fd2_field_unit *unit, int *selected, fd2_field_magic_list_key key,
    uint8_t *confirmed_id);

/* field_magic_record_get @0x4e866：DS:0x19fd + magic_id*7。
 * 表内容来自 object3 `DS:0x19fd`，对应 FD2.EXE file offset `0x7aa11`。
 * 字段在 AI 评分层暂按原始 offset 使用，避免提前赋予玩法语义。 */
const uint8_t *fd2_field_magic_record_get(uint8_t magic_id);
uint16_t fd2_field_magic_u16(const uint8_t *record, size_t offset);

/* field_magic_damage_profile_apply @0x1c75e 使用的 object2 DS:0x1f96
 * 28 项 u32 倍率表；索引 0 对应 profile 1。 */
int fd2_field_magic_damage_profile_scale(
    uint8_t movement_profile, uint32_t *scale);

/* 无演出法术效果核心。返回 1 表示命中并提交，0 表示法术 miss／免疫，
 * -1 表示当前 ID 或数据尚不支持。IDs 0..8 经 DS:0x1d01 wrapper
 * 进入 0x55115 动画并由其主体调用 0x1c75e，ID 9 wrapper 直接调用
 * 该 core；IDs 10..12 在 core 内额外执行 0x1f183 免疫门。
 * RNG 调用顺序与 0x1c768/0x1c81f：先命中，再按伤害浮动消费一次。 */
typedef uint32_t (*fd2_field_magic_rng_fn)(void *userdata);

/* field_hp_heal_apply @0x1c916（原登记sub_1C916）的纯HP部分。
 * power取调用方的原始u16；两段整数截断后加HP并夹到hp_max。
 * 返回1完成/-1空target或rng；拒绝不写入、不耗RNG。完成时恰耗一次
 * spread RNG，power0／满HP亦如此；除HP外不改unit，不写MP／EX／acted。
 * 范围、目标及数据支持门、原始经验贡献与各动作尾链均由调用方负责。 */
int fd2_field_hp_heal_apply(
    fd2_field_unit *target, uint16_t power,
    fd2_field_magic_rng_fn rng, void *rng_userdata);

/* field_mp_heal_apply @0x1c9dd（原sub_1C9DD）：与HP共享两段spread截断，
 * 只改MP并夹到max；满MP仍耗一次RNG，mp_max==0的跳过由caller负责。 */
int fd2_field_mp_heal_apply(fd2_field_unit *target, uint16_t power,
    fd2_field_magic_rng_fn rng, void *rng_userdata);

/* SDL无演出基础法术的单目标结果，不含MP／acted／死亡清理／升级。
 * experience_value是sub_1C81F @0x1c81f／sub_1C916 @0x1c916
 * （原登记同名）向53EC8累加的原始贡献，尚未按caster等级归一。 */
typedef struct {
    uint8_t hit;
    uint16_t hp_before;
    uint16_t hp_after;
    uint32_t raw_damage; /* 命中伤害在HP夹零前的数值；miss／治疗为0。 */
    uint32_t experience_value;
} fd2_field_magic_basic_result;

/* 仅ID0伤害／ID13治疗。最坏原始经验贡献查询不耗RNG、不写target；
 * 未知敌军系数、坏profile／HP等失败时不写output。调用方先以uint64
 * 累加全部目标上界，再经basic_reward_gain预检，之后才扣MP和执行效果。 */
int fd2_field_magic_basic_reward_max(
    uint8_t magic_id, const fd2_field_unit *target, uint32_t *experience_value);

/* 复用既有效果公式／RNG；返回1命中、0miss、-1拒绝（不写结果）。
 * 单目标结果只提交target HP与output，不扣MP、不写caster EX。
 * 本接口不替调用方完成整次cast的升级预检或多目标提交边界。 */
int fd2_field_magic_apply_player_basic_effect(
    uint8_t magic_id, fd2_field_unit *target,
    fd2_field_magic_rng_fn rng, void *rng_userdata,
    fd2_field_magic_basic_result *result);

/* field_player_command_execute @0x18d8c的caster归一／controller
 * @0x117e7 cap99及sub_1E292 @0x1e292（原登记同名）的非升级门。
 * raw_sum须用uint64累加；超过UINT32_MAX拒绝，不接受先回绕的总数。
 * hidden或原版等值levelcap给gain0；其他只支持EX+gain<100。
 * 0成功/-1拒绝，失败不写gain；不写unit，不消费RNG。 */
int fd2_field_magic_basic_reward_gain(
    const fd2_field_unit *caster, uint64_t raw_sum, uint8_t *gain);

int fd2_field_magic_apply_known_effect(
    uint8_t magic_id,
    uint8_t target_unit_index,
    fd2_field_units *units,
    fd2_field_magic_rng_fn rng,
    void *rng_userdata);

#endif /* FD2_FIELD_MAGIC_H */
