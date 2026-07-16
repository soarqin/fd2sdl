#ifndef FD2_FIELD_MAGIC_H
#define FD2_FIELD_MAGIC_H

#include <stddef.h>
#include <stdint.h>

#include "field_unit.h"

#define FD2_FIELD_MAGIC_RECORD_SIZE 7u
#define FD2_FIELD_MAGIC_COUNT 36u
#define FD2_FIELD_MAGIC_DAMAGE_PROFILE_COUNT 28u

/* field_magic_record_get @0x4e866：DS:0x19fd + magic_id*7。
 * 表内容来自 object3 `DS:0x19fd`，对应 FD2.EXE file offset `0x7aa11`。
 * 字段在 AI 评分层暂按原始 offset 使用，避免提前赋予玩法语义。 */
const uint8_t *fd2_field_magic_record_get(uint8_t magic_id);
uint16_t fd2_field_magic_u16(const uint8_t *record, size_t offset);

/* field_magic_damage_profile_apply @0x1c768 使用的 object2 DS:0x1f96
 * 28 项 u32 倍率表；索引 0 对应 profile 1。 */
int fd2_field_magic_damage_profile_scale(
    uint8_t movement_profile, uint32_t *scale);

/* 无演出法术效果核心。返回 1 表示命中并提交，0 表示法术 miss／免疫，
 * -1 表示当前 ID 或数据尚不支持。IDs 0..8 经 DS:0x1d01 wrapper
 * 进入 0x55115 动画并由其主体调用 0x1c75e，ID 9 wrapper 直接调用
 * 该 core；IDs 10..12 在 core 内额外执行 0x1f183 免疫门。
 * RNG 调用顺序与 0x1c768/0x1c81f：先命中，再按伤害浮动消费一次。 */
typedef uint32_t (*fd2_field_magic_rng_fn)(void *userdata);
int fd2_field_magic_apply_known_effect(
    uint8_t magic_id,
    uint8_t target_unit_index,
    fd2_field_units *units,
    fd2_field_magic_rng_fn rng,
    void *rng_userdata);

#endif /* FD2_FIELD_MAGIC_H */
