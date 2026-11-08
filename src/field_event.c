/* 炎龙骑士团 2 SDL3 重写 - FDFIELD 回合与格子事件查询
 *
 * 逆向依据：
 * - field_turn_event_check @0x1a813 遍历 metadata offset 0x03 的
 *   16×3 字节表，比较当前回合和 phase 后按 action 查回调表；
 * - field_cell_event_lookup @0x13a44 从 32 位 map cell 的 byte 2
 *   读取低 5 位 1-based ID，查询 metadata offset 0x33 的 16×2 表；
 * - `DS:0x1b91` 回调 ABI 已由 DOSBox 与 LE fixup 交叉确认：cdecl
 *   `handler(actor_index)`，回合事件传 0，调用者清理 4 字节栈。
 *
 * 本模块实现纯查询和可复用的移动脚本状态提交；剧情 handler 的分组、
 * 镜头和事务边界由 session 层接入，演出不混入查询核心。
 */

#include "field_event.h"

typedef struct {
    uint8_t id;
    const uint8_t *bytes;
    size_t size;
} fd2_static_movement_script;

/* 原指针表0x627d8的已确认读集与完整字节流校验见生成器及来源清单。 */
#include "static_data/movement_scripts.inc"

size_t fd2_field_turn_events_find(
    const fd2_field_metadata *metadata,
    uint32_t turn_number,
    uint8_t phase,
    fd2_field_turn_event_match *out,
    size_t capacity) {
    if (!metadata || turn_number > UINT8_MAX) return 0;

    size_t count = 0;
    for (size_t i = 0; i < FD2_FIELD_EVENT_SLOTS; i++) {
        const fd2_field_turn_event *event = &metadata->turn_events[i];
        if (event->turn != (uint8_t)turn_number ||
            event->phase != phase)
            continue;
        if (out && count < capacity) {
            out[count].slot = i;
            out[count].action = event->action;
            out[count].phase = event->phase;
        }
        count++;
    }
    return count;
}

int fd2_field_cell_event_find(
    const fd2_field_map *map,
    const fd2_terrain_tileset *terrain,
    const fd2_field_metadata *metadata,
    int x, int y,
    uint8_t match_arg,
    uint8_t *event_code,
    size_t *slot) {
    if (!map || !map->cells || !terrain || !terrain->attrs || !metadata ||
        x < 0 || y < 0 || x >= map->width || y >= map->height)
        return -1;

    uint32_t cell = map->cells[(size_t)y * (size_t)map->width + (size_t)x];
    uint16_t terrain_id = (uint16_t)(cell & 0x03ffu);
    if (terrain_id >= terrain->attr_count) return -1;
    if ((terrain->attrs[terrain_id].flags & 0x60u) != 0) return 0;

    uint8_t lookup_id = (uint8_t)((cell >> 16) & 0x1fu);
    if (lookup_id == 0 || lookup_id > FD2_FIELD_EVENT_SLOTS) return 0;
    size_t index = (size_t)lookup_id - 1u;
    const fd2_field_cell_lookup *lookup = &metadata->cell_lookup[index];
    if (lookup->event_code == 0xffu || lookup->match_arg != match_arg)
        return 0;

    if (event_code) *event_code = lookup->event_code;
    if (slot) *slot = index;
    return 1;
}

static void move_one(fd2_field_unit *unit, uint8_t direction) {
    unit->direction = direction & 3u;
    switch (unit->direction) {
        case 0: unit->y++; break;
        case 1: unit->x--; break;
        case 2: unit->y--; break;
        case 3: unit->x++; break;
    }
}

int fd2_field_movement_script_apply(fd2_field_units *units,
                                    const uint8_t *script,
                                    size_t script_size) {
    if (!units || !script || script_size == 0 ||
        units->count > FD2_FIELD_MAX_UNITS)
        return -1;

    fd2_field_units updated = *units;
    size_t pos = 1;
    size_t group_count = script[0];
    for (size_t group = 0; group < group_count; group++) {
        if (pos + 2u > script_size) return -1;
        uint8_t mode = script[pos++];
        uint8_t actor_count = script[pos++];
        size_t pair_bytes = (size_t)actor_count * 2u;
        if (pair_bytes > script_size - pos) return -1;
        const uint8_t *pairs = script + pos;
        pos += pair_bytes;

        for (size_t i = 0; i < actor_count; i++) {
            uint8_t actor = pairs[i * 2u];
            uint8_t direction = pairs[i * 2u + 1u];
            if (actor >= updated.count || direction > 3u) return -1;
            updated.items[actor].direction = direction;
        }
        if ((mode & 0x80u) != 0) continue;

        for (uint8_t step = 0; step < mode; step++) {
            for (size_t i = 0; i < actor_count; i++) {
                uint8_t actor = pairs[i * 2u];
                move_one(&updated.items[actor], pairs[i * 2u + 1u]);
                updated.walk_frames[actor] = 1;
                updated.items[actor].frame_phase = 0;
            }
        }
    }
    if (pos != script_size) return -1;
    *units = updated;
    return 0;
}

int fd2_field_movement_script_get(uint8_t script_id,
        const uint8_t **script, size_t *script_size) {
    if (!script || !script_size) return -1;
    for (size_t i = 0; i < sizeof(k_movement_scripts)/sizeof(k_movement_scripts[0]); i++) {
        if (k_movement_scripts[i].id != script_id) continue;
        *script = k_movement_scripts[i].bytes;
        *script_size = k_movement_scripts[i].size;
        return 0;
    }
    *script = NULL;
    *script_size = 0;
    return -1;
}

int fd2_field_stage0_movement_script_get(uint8_t script_id,
        const uint8_t **script, size_t *script_size) {
    /* 保留旧stage0事件查询的支持门；开场统一调用完整getter。 */
    if (script_id < 3u || script_id == 5u || script_id >= 9u) {
        if (script) *script = NULL;
        if (script_size) *script_size = 0;
        return -1;
    }
    return fd2_field_movement_script_get(script_id, script, script_size);
}
