#ifndef FD2_FIELD_EVENT_H
#define FD2_FIELD_EVENT_H

#include <stddef.h>
#include <stdint.h>

#include "field.h"
#include "field_unit.h"
#include "tile.h"

typedef struct {
    size_t slot;
    uint8_t action;
    uint8_t phase;
} fd2_field_turn_event_match;

/* 复现 field_turn_event_check @0x3fa27 的纯查询部分：只返回
 * turn==turn_number 且 phase 匹配的记录，不执行回调表。 */
size_t fd2_field_turn_events_find(
    const fd2_field_metadata *metadata,
    uint32_t turn_number,
    uint8_t phase,
    fd2_field_turn_event_match *out,
    size_t capacity);

/* 复现 field_cell_event_lookup @0x38c58：地图 cell 第 2 字节低 5 位
 * 是 1-based lookup ID；地形 attr.flags 的 0x60 位会禁止触发。
 * 玩家移动每步查询 match_arg 0；玩家行动及 AI common tail 查询 1。
 * 返回 1 表示命中，0 表示无事件，-1 表示输入数据无效。 */
int fd2_field_cell_event_find(
    const fd2_field_map *map,
    const fd2_terrain_tileset *terrain,
    const fd2_field_metadata *metadata,
    int x, int y,
    uint8_t match_arg,
    uint8_t *event_code,
    size_t *slot);

/* 复现 field_movement_script_play @0x3887e 的无演出状态提交部分。
 * 先完整校验脚本与 actor 索引，失败时不修改单位表。mode 最高位为 1
 * 时只提交朝向；否则按低 7 位步数移动，并将步态复位为静止。 */
int fd2_field_movement_script_apply(fd2_field_units *units,
                                    const uint8_t *script,
                                    size_t script_size);

/* stage 0/31 action 0..3 使用的已确认 DAT_000027d8 脚本。
 * 返回 0 并提供只读字节流；未知 script_id 返回 -1。 */
int fd2_field_stage0_movement_script_get(uint8_t script_id,
                                         const uint8_t **script,
                                         size_t *script_size);

#endif /* FD2_FIELD_EVENT_H */
