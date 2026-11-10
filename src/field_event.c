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
 * G4纯原语另对应 field_map_runtime_cell_bits_reset @0x4df4c（原sub_4DF4C）、
 * field_actor_tail_defeat_apply @0x35f10（原sub_35F10）；固定动态来源保留
 * field_event_turn_group_pair_camera_flash @0x35638（原sub_35638）的AL宽度。
 * 它调用的 field_camera_group_append_flash @0x35b78（原登记field_camera_music_flash）
 * 是pan/group/flash/wait共有尾，不播放音乐；具体组合留冷节点。
 */

#include "field_event.h"
#include "field_item.h"

typedef struct {
    uint8_t id;
    const uint8_t *bytes;
    size_t size;
} fd2_static_movement_script;

/* 原指针表0x627d8的已确认读集与完整字节流校验见生成器及来源清单。 */
#include "static_data/movement_scripts.inc"
#include "static_data/common_event.inc"

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
        if (actor_count > 32u) return -1; /* 原1366a两组32B局部数组。 */
        size_t pair_bytes = (size_t)actor_count * 2u;
        if (pair_bytes > script_size - pos) return -1;
        const uint8_t *pairs = script + pos;
        pos += pair_bytes;

        for (size_t i = 0; i < actor_count; i++) {
            uint8_t actor = pairs[i * 2u];
            uint8_t direction = pairs[i * 2u + 1u];
            if (actor >= updated.count ||
                (direction > 3u && !(mode == 0x80u && direction == 5u))) return -1;
            if (mode != 0) updated.items[actor].direction = direction;
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

/* G4：原53ad5的完整32B、33499/24bde持久查询、2aedb prefix查物，
 * 34dd0/35bee等相对turn BYTE写点及3623c单RNG配对。这里没有ACK、
 * 故事序列、隐式重算或第二事件owner。旧登记sub_*均见canonical VA。 */
static int event_compare_i64(int64_t value, fd2_field_event_compare op,
        int64_t expected) {
    switch (op) {
    case FD2_EVENT_EQ: return value == expected;
    case FD2_EVENT_NE: return value != expected;
    case FD2_EVENT_LT: return value < expected;
    case FD2_EVENT_LE: return value <= expected;
    case FD2_EVENT_GT: return value > expected;
    case FD2_EVENT_GE: return value >= expected;
    default: return -1;
    }
}

int fd2_field_event_compare_unsigned(uint32_t value,
        fd2_field_event_compare comparison, uint32_t expected) {
    return event_compare_i64((int64_t)value, comparison, (int64_t)expected);
}

int fd2_field_event_compare_turn(uint32_t value,
        fd2_field_event_compare comparison, int32_t expected) {
    int64_t signed_value = value <= INT32_MAX ? (int64_t)value :
        (int64_t)value - INT64_C(0x100000000);
    return event_compare_i64(signed_value, comparison, expected);
}

int fd2_field_event_state_compare(const uint8_t state[FD2_FIELD_EVENT_STATE_SIZE],
        size_t index, fd2_field_event_compare comparison, uint8_t expected) {
    if (!state || index >= FD2_FIELD_EVENT_STATE_SIZE) return -1;
    return fd2_field_event_compare_unsigned(state[index], comparison, expected);
}

int fd2_field_event_state_write(uint8_t state[FD2_FIELD_EVENT_STATE_SIZE],
        size_t first, size_t last, uint8_t value, int add) {
    if (!state || first > last || last >= FD2_FIELD_EVENT_STATE_SIZE ||
        (add != 0 && add != 1)) return -1;
    for (size_t i = first; i <= last; i++)
        state[i] = add ? (uint8_t)(state[i] + value) : value;
    return 0;
}

int fd2_field_event_state_from_count(uint8_t state[FD2_FIELD_EVENT_STATE_SIZE],
        size_t index, const fd2_field_units *units, uint8_t subtract) {
    if (!state || !units || index >= FD2_FIELD_EVENT_STATE_SIZE ||
        units->count > FD2_FIELD_MAX_UNITS || units->count < subtract) return -1;
    state[index] = (uint8_t)(units->count - subtract);
    return 0;
}

int fd2_field_event_schedule_turn(fd2_field_metadata *metadata, size_t slot,
        uint32_t now, uint8_t delta) {
    if (!metadata || slot >= FD2_FIELD_EVENT_SLOTS || delta > 2u) return -1;
    metadata->turn_events[slot].turn = (uint8_t)(now + (uint32_t)delta);
    return 0;
}

static int event_actor_range(const fd2_field_units *units, size_t first,
        size_t last) {
    return units && units->count <= FD2_FIELD_MAX_UNITS && first <= last &&
        last < units->count;
}

int fd2_field_event_actor_compare(const fd2_field_units *units, size_t actor,
        fd2_field_event_actor_field field, fd2_field_event_compare comparison,
        uint32_t expected) {
    if (!event_actor_range(units, actor, actor)) return -1;
    const fd2_field_unit *unit = &units->items[actor];
    uint32_t value;
    switch (field) {
    case FD2_EVENT_ACTOR_HIDDEN: value = (unit->flags & 1u) != 0; break;
    case FD2_EVENT_ACTOR_SIDE: value = unit->side; break;
    case FD2_EVENT_ACTOR_TEXT: value = unit->text_id; break;
    case FD2_EVENT_ACTOR_ID: value = unit->unit_id; break;
    case FD2_EVENT_ACTOR_HP: value = unit->hp; break;
    case FD2_EVENT_ACTOR_HP_MAX: value = unit->hp_max; break;
    case FD2_EVENT_ACTOR_ACTED: value = (unit->flags & 0x80u) != 0; break;
    case FD2_EVENT_ACTOR_INVENTORY_COUNT:
        value = (uint32_t)fd2_field_unit_inventory_count(unit); break;
    default: return -1;
    }
    return fd2_field_event_compare_unsigned(value, comparison, expected);
}

int fd2_field_event_hidden_count(const fd2_field_units *units,
        size_t first, size_t last, size_t *count) {
    if (!count || !event_actor_range(units, first, last)) return -1;
    size_t found = 0;
    for (size_t i = first; i <= last; i++)
        found += (units->items[i].flags & 1u) != 0;
    *count = found;
    return 0;
}

int fd2_field_event_roster_has_text(const fd2_field_unit *roster,
        size_t count, uint8_t text_id) {
    if (!roster || count > 32u) return -1;
    for (size_t i = 0; i < count; i++)
        if (roster[i].text_id == text_id) return 1;
    return 0;
}

static int event_item_slot(const fd2_field_unit *unit, uint8_t item,
        size_t *slot) {
    size_t count = fd2_field_unit_inventory_count(unit);
    const uint8_t *raw = (const uint8_t *)unit;
    for (size_t i = 0; i < count; i++) {
        if (raw[0x0bu + i * 2u] != item) continue;
        if (slot) *slot = i;
        return 1;
    }
    return 0;
}

int fd2_field_event_item_find(const fd2_field_units *units, size_t actor,
        uint8_t item, size_t *slot) {
    if (!event_actor_range(units, actor, actor)) return -1;
    return event_item_slot(&units->items[actor], item, slot);
}

int fd2_field_event_item_append(fd2_field_units *units, size_t actor,
        uint8_t item) {
    if (!event_actor_range(units, actor, actor)) return -1;
    return fd2_field_unit_inventory_item_append(&units->items[actor], item);
}

int fd2_field_event_item_remove(fd2_field_units *units, size_t actor,
        uint8_t item) {
    if (!event_actor_range(units, actor, actor)) return -1;
    size_t slot;
    if (!event_item_slot(&units->items[actor], item, &slot)) return 0;
    return fd2_field_unit_inventory_slot_remove(&units->items[actor], slot) == 0 ? 1 : -1;
}

int fd2_field_event_item_occurrences(const fd2_field_units *units,
        size_t first_actor, size_t last_actor, uint8_t first_item,
        uint8_t last_item, size_t *count) {
    if (!count || !event_actor_range(units, first_actor, last_actor) ||
        first_item > last_item) return -1;
    size_t found = 0;
    for (unsigned item = first_item; item <= last_item; item++)
        for (size_t actor = first_actor; actor <= last_actor; actor++)
            found += (size_t)event_item_slot(&units->items[actor], (uint8_t)item, NULL);
    *count = found;
    return 0;
}

int fd2_field_event_item_consume(fd2_field_units *units,
        size_t first_actor, size_t last_actor, uint8_t first_item,
        uint8_t last_item) {
    if (!event_actor_range(units, first_actor, last_actor) ||
        first_item > last_item) return -1;
    /* 全部索引已验证；共享remove不会在合法raw0..7中途失败。各次移槽
     * 是原循环的真实提交，不借第二张队伍或事后整批回滚。 */
    for (unsigned item = first_item; item <= last_item; item++)
        for (size_t actor = first_actor; actor <= last_actor; actor++)
            if (fd2_field_event_item_remove(units, actor, (uint8_t)item) < 0) return -1;
    return 0;
}

int fd2_field_event_cell_item(const fd2_field_map *map, int x, int y, uint8_t *item) {
    if (!map || !map->cells || !item || map->width <= 0 || map->height <= 0 ||
        map->width > 256 || map->height > 256 ||
        x < 0 || y < 0 || x >= map->width || y >= map->height ||
        (size_t)map->width > SIZE_MAX / (size_t)map->height) return -1;
    uint32_t cell = map->cells[(size_t)y * (size_t)map->width + (size_t)x];
    size_t index = (cell >> 16) & 0xffu;
    if (index >= sizeof(k_common_event_cell_items) / sizeof(k_common_event_cell_items[0])) return -1;
    *item = k_common_event_cell_items[index];
    return 0;
}

int fd2_field_event_child_descriptor(size_t index, const uint8_t **descriptor) {
    if (!descriptor || index >= sizeof(k_common_event_nested_descriptors) /
        sizeof(k_common_event_nested_descriptors[0])) return -1;
    *descriptor = k_common_event_nested_descriptors[index];
    return 0;
}

int fd2_field_event_map_reset_cells(fd2_field_map *map) {
    if (!map || !map->cells || map->width <= 0 || map->height <= 0 ||
        map->width > 256 || map->height > 256 ||
        (size_t)map->width > SIZE_MAX / (size_t)map->height) return -1;
    size_t count = (size_t)map->width * (size_t)map->height;
    for (size_t i=0; i<count; i++)
        map->cells[i] = (map->cells[i] & UINT32_C(0x001f03ff)) | UINT32_C(0xff000000);
    return 0;
}

int fd2_field_event_hp_zero_tail(fd2_field_units *units, size_t first) {
    /* 35f10@35f39用signed JL；非负first>=count是空循环，不要求raw存在。 */
    if (!units || units->count > FD2_FIELD_MAX_UNITS || first > INT32_MAX) return -1;
    for (size_t i=first; i<units->count; i++) units->items[i].hp = 0;
    return 0;
}

/* 35468/3553f有符号向0除2；356b3/35833原DWORD push；35638在AL内
 * (turn-14)*2/+1后MOVZX；362e8从state16零扩展构造actor/story/group。
 * 全部是已确认的固定来源形态，不接受地址、offset、算术opcode或表达式。 */
int fd2_field_event_source_value(fd2_field_event_value_source source, uint32_t turn,
        const uint8_t state[FD2_FIELD_EVENT_STATE_SIZE], uint32_t *value) {
    if (!value) return -1;
    uint32_t result;
    switch (source) {
    case FD2_EVENT_VALUE_TURN_DWORD: result=turn; break;
    case FD2_EVENT_VALUE_TURN_BYTE: result=(uint8_t)turn; break;
    case FD2_EVENT_VALUE_TURN_SIGNED_HALF: {
        int64_t signed_turn=turn<=INT32_MAX ? (int64_t)turn : (int64_t)turn-INT64_C(0x100000000);
        result=(uint32_t)(signed_turn/2); break;
    }
    case FD2_EVENT_VALUE_TURN_PAIR_EVEN_BYTE:
        result=(uint8_t)(((uint8_t)turn-14u)*2u); break;
    case FD2_EVENT_VALUE_TURN_PAIR_ODD_BYTE:
        result=(uint8_t)(((uint8_t)turn-14u)*2u+1u); break;
    case FD2_EVENT_VALUE_STATE16:
    case FD2_EVENT_VALUE_STATE16_REVERSE_ACTOR:
    case FD2_EVENT_VALUE_STATE16_FIRST_ACTOR:
    case FD2_EVENT_VALUE_STATE16_SECOND_ACTOR:
    case FD2_EVENT_VALUE_STATE16_STORY:
        if (!state) return -1;
        switch (source) {
        case FD2_EVENT_VALUE_STATE16: result=state[16]; break;
        case FD2_EVENT_VALUE_STATE16_REVERSE_ACTOR: result=24u-state[16]; break;
        case FD2_EVENT_VALUE_STATE16_FIRST_ACTOR: result=2u*state[16]+25u; break;
        case FD2_EVENT_VALUE_STATE16_SECOND_ACTOR: result=2u*state[16]+26u; break;
        default: result=state[16]+2u; break;
        }
        break;
    default: return -1;
    }
    *value=result;return 0;
}

int fd2_field_event_mark_random_pair(fd2_field_units *units, size_t base,
        fd2_field_event_rng_fn rng, void *userdata) {
    if (!rng || base > FD2_FIELD_MAX_UNITS - 3u ||
        !event_actor_range(units, base, base + 2u)) return -1;
    uint32_t value = rng(userdata);
    units->items[base + value % 3u].flags |= 0x80u;
    /* 原RNG为WORD域；用DWORD加法保留已证原调用，非第二次取样。 */
    units->items[base + (value + 1u) % 3u].flags |= 0x80u;
    return 0;
}

int fd2_field_event_child_begin(fd2_field_event_resume *continuation,
        const fd2_static_flow *flow, int parent_pc, size_t actor,
        size_t actor_count) {
    if (!continuation || continuation->child_wait || !flow ||
        !fd2_static_flow_at(flow, parent_pc) || parent_pc > INT16_MAX ||
        actor_count > FD2_FIELD_MAX_UNITS || actor >= actor_count) return -1;
    continuation->flow = flow;
    continuation->pc = (int16_t)parent_pc;
    continuation->actor = (uint8_t)actor;
    continuation->active = 1;
    continuation->child_wait = 1;
    return 0;
}

int fd2_field_event_child_finish(fd2_field_event_resume *continuation,
        int succeeded, int *parent_pc, size_t *actor) {
    if (!continuation || !continuation->active || !continuation->child_wait || !parent_pc || !actor ||
        !fd2_static_flow_at(continuation->flow, continuation->pc) ||
        (succeeded != 0 && succeeded != 1)) return -1;
    /* 失败保留父续态供原owner冻结/诊断；不能清空后重入或重放已ACK写点。 */
    if (!succeeded) return -1;
    *parent_pc = continuation->pc;
    *actor = continuation->actor;
    continuation->child_wait = 0;
    return 0;
}
