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

/* stage 0/31 action 0..3 使用的 DAT_000027d8 移动脚本。object3
 * target_offset 到 raw 视图扣除 0x28b8；脚本 3/4/6/7/8 分别位于
 * raw object3 offset 0x119/0x138/0x14e/0x163/0x178。 */
static const uint8_t k_stage0_event_move_03[] = {
    0x03,0x02,0x04,0x0e,0x01,0x0f,0x02,0x10,0x02,0x11,0x02,
    0x02,0x04,0x0e,0x01,0x0f,0x01,0x10,0x01,0x11,0x01,
    0x02,0x04,0x0e,0x02,0x0f,0x01,0x10,0x02,0x11,0x01,
};
static const uint8_t k_stage0_event_move_04[] = {
    0x02,0x01,0x05,0x12,0x02,0x13,0x02,0x14,0x03,0x15,0x02,
    0x16,0x03,0x01,0x01,0x12,0x03,
};
static const uint8_t k_stage0_event_move_06[] = {
    0x02,0x05,0x04,0x17,0x01,0x18,0x01,0x19,0x01,0x1a,0x01,
    0x01,0x04,0x17,0x00,0x18,0x01,0x19,0x02,0x1a,0x03,
};
static const uint8_t k_stage0_event_move_07[] = {
    0x05,0x02,0x01,0x0c,0x00,0x86,0x01,0x0c,0x01,0x86,0x01,
    0x0c,0x03,0x8a,0x01,0x0c,0x01,0x80,0x01,0x0c,0x01,
};
static const uint8_t k_stage0_event_move_08[] = {
    0x03,0x01,0x01,0x0d,0x00,0x88,0x01,0x0d,0x03,0x88,0x01,
    0x0d,0x01,
};

/* field_stage1_entry_play/field_stage1_turn3_execute script9，canonical数据VA0x62a3d。 */
static const uint8_t k_stage1_event_move_09[] = {
    0x06, 0x02, 0x05, 0x00, 0x01, 0x01, 0x01, 0x02, 0x01, 0x03, 0x01, 0x04, 0x01, 0x01, 0x03, 0x00, 0x01, 0x01, 0x00, 0x02, 0x02, 0x01, 0x04, 0x00, 0x01, 0x01, 0x01, 0x02, 0x01, 0x03, 0x02, 0x01, 0x03, 0x00, 0x01, 0x02, 0x02, 0x03, 0x01, 0x01, 0x02, 0x00, 0x02, 0x02, 0x01, 0x88, 0x01, 0x00, 0x01
};
/* field_stage1_entry_play/field_stage1_turn3_execute script10，canonical数据VA0x62a6e。 */
static const uint8_t k_stage1_event_move_10[] = {
    0x06, 0x01, 0x01, 0x00, 0x01, 0x88, 0x01, 0x00, 0x02, 0x88, 0x01, 0x00, 0x01, 0x88, 0x01, 0x00, 0x00, 0x88, 0x01, 0x00, 0x01, 0x88, 0x01, 0x00, 0x03
};
/* field_stage1_entry_play/field_stage1_turn3_execute script11，canonical数据VA0x62a87。 */
static const uint8_t k_stage1_event_move_11[] = {
    0x07, 0x01, 0x02, 0x05, 0x00, 0x06, 0x00, 0x01, 0x01, 0x05, 0x00, 0x88, 0x01, 0x05, 0x01, 0x88, 0x01, 0x05, 0x00, 0x88, 0x01, 0x05, 0x01, 0x88, 0x02, 0x05, 0x03, 0x00, 0x01, 0x80, 0x02, 0x05, 0x03, 0x00, 0x01
};
/* field_stage1_entry_play/field_stage1_turn3_execute script12，canonical数据VA0x62aaa。 */
static const uint8_t k_stage1_event_move_12[] = {
    0x03, 0x01, 0x06, 0x0b, 0x02, 0x0c, 0x02, 0x0d, 0x02, 0x0e, 0x02, 0x0f, 0x02, 0x10, 0x02, 0x01, 0x05, 0x0b, 0x02, 0x0c, 0x01, 0x0d, 0x01, 0x0e, 0x03, 0x0f, 0x03, 0x01, 0x05, 0x0b, 0x02, 0x0c, 0x01, 0x0d, 0x02, 0x0e, 0x03, 0x0f, 0x02
};
/* field_stage1_entry_play/field_stage1_turn3_execute script13，canonical数据VA0x62ad1。 */
static const uint8_t k_stage1_event_move_13[] = {
    0x04, 0x01, 0x06, 0x15, 0x00, 0x16, 0x00, 0x17, 0x00, 0x18, 0x00, 0x19, 0x00, 0x1a, 0x00, 0x01, 0x05, 0x15, 0x00, 0x16, 0x01, 0x17, 0x01, 0x18, 0x03, 0x19, 0x03, 0x01, 0x05, 0x15, 0x00, 0x16, 0x01, 0x17, 0x00, 0x18, 0x03, 0x19, 0x00, 0x84, 0x02, 0x16, 0x00, 0x18, 0x00
};

/* stage1 script14，canonical数据VA0x62afe。 */
static const uint8_t k_stage1_event_move_14[] = {
    0x01,0x02,0x01,0x1b,0x00
};

/* stage1 script15，canonical数据VA0x62b03。 */
static const uint8_t k_stage1_event_move_15[] = {
    0x01,0x80,0x05,0x00,0x00,0x01,0x00,0x02,0x00,0x03,0x00,0x04,0x00
};

/* stage1 script16，canonical数据VA0x62b10。 */
static const uint8_t k_stage1_event_move_16[] = {
    0x06,0x03,0x01,0x1b,0x01,0x02,0x01,0x1b,0x02,0x01,0x01,0x1b,0x01,0x01,0x01,0x1b,0x02,0x03,0x01,0x1b,0x01,0x03,0x01,0x1b,0x02
};

/* stage2 script17，canonical数据VA0x62b29。 */
static const uint8_t k_stage2_event_move_17[] = {
    0x05,0x06,0x01,0x06,0x00,0x8f,0x01,0x06,0x00,0x8f,0x01,0x06,0x01,0x8f,0x01,0x06,0x03,0x8f,0x01,0x06,0x00
};

/* stage2 script18，canonical数据VA0x62b3e。 */
static const uint8_t k_stage2_event_move_18[] = {
    0x01,0x88,0x06,0x00,0x02,0x01,0x02,0x02,0x02,0x03,0x02,0x04,0x02,0x05,0x02
};

/* stage2 script19，canonical数据VA0x62b4d。 */
static const uint8_t k_stage2_event_move_19[] = {
    0x03,0x05,0x08,0x07,0x00,0x08,0x00,0x09,0x00,0x0a,0x00,0x0b,0x00,0x0c,0x00,0x0d,0x00,0x0e,0x00,0x80,0x01,0x06,0x00,0x8f,0x01,0x06,0x02
};

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
                                         const uint8_t **script,
                                         size_t *script_size) {
    if (!script || !script_size) return -1;
    switch (script_id) {
        case 3:
            *script = k_stage0_event_move_03;
            *script_size = sizeof(k_stage0_event_move_03);
            return 0;
        case 4:
            *script = k_stage0_event_move_04;
            *script_size = sizeof(k_stage0_event_move_04);
            return 0;
        case 6:
            *script = k_stage0_event_move_06;
            *script_size = sizeof(k_stage0_event_move_06);
            return 0;
        case 7:
            *script = k_stage0_event_move_07;
            *script_size = sizeof(k_stage0_event_move_07);
            return 0;
        case 8:
            *script = k_stage0_event_move_08;
            *script_size = sizeof(k_stage0_event_move_08);
            return 0;
        case 9:
            *script = k_stage1_event_move_09;
            *script_size = sizeof(k_stage1_event_move_09);
            return 0;
        case 10:
            *script = k_stage1_event_move_10;
            *script_size = sizeof(k_stage1_event_move_10);
            return 0;
        case 11:
            *script = k_stage1_event_move_11;
            *script_size = sizeof(k_stage1_event_move_11);
            return 0;
        case 12:
            *script = k_stage1_event_move_12;
            *script_size = sizeof(k_stage1_event_move_12);
            return 0;
        case 13:
            *script = k_stage1_event_move_13;
            *script_size = sizeof(k_stage1_event_move_13);
            return 0;
        case 14:
            *script = k_stage1_event_move_14;
            *script_size = sizeof(k_stage1_event_move_14);
            return 0;
        case 15:
            *script = k_stage1_event_move_15;
            *script_size = sizeof(k_stage1_event_move_15);
            return 0;
        case 16:
            *script = k_stage1_event_move_16;
            *script_size = sizeof(k_stage1_event_move_16);
            return 0;
        case 17:
            *script = k_stage2_event_move_17;
            *script_size = sizeof(k_stage2_event_move_17);
            return 0;
        case 18:
            *script = k_stage2_event_move_18;
            *script_size = sizeof(k_stage2_event_move_18);
            return 0;
        case 19:
            *script = k_stage2_event_move_19;
            *script_size = sizeof(k_stage2_event_move_19);
            return 0;
        default:
            *script = NULL;
            *script_size = 0;
            return -1;
    }
}

int fd2_field_stage0_movement_script_get(uint8_t script_id,
        const uint8_t **script, size_t *script_size) {
    if (script_id >= 9) {
        if (script) *script = NULL;
        if (script_size) *script_size = 0;
        return -1;
    }
    return fd2_field_movement_script_get(script_id, script, script_size);
}
