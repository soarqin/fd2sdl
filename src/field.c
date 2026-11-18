#include "field.h"

/* B5准备表依据sub_4E809 @0x4e809；音乐表依据music_track_play @0x25977
 * 与field_turn_cycle_run @0x1a30b，均为纯只读数据，不推进session。
 * metadata来源保留依据sub_10010 @0x10010整块恢复与
 * field_secondary_menu_execute @0x19df7整块写回；未知尾不推断业务。 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "static_flow.h"

/* 准备及音乐参数由 data/static/tables/stage.json 维护。 */
#include "static_data/stage.inc"
#include "static_data.h"

const fd2_static_flow *fd2_field_stage_flow_find(const char *id) {
    return fd2_static_flow_lookup("stage", id);
}

/* main @0x25bf4 的entry/victory分派：只有明确发布的同一冷图才走
 * 单次来源consumer；source库存存在本身不授予正式入口能力。 */
int fd2_field_stage_source_is_bound(size_t stage, const char *kind) {
    if (!kind || (strcmp(kind, "entry") && strcmp(kind, "victory"))) return 0;
    char bound_id[64], source_id[64];
    int a = snprintf(bound_id, sizeof(bound_id), "stage.%s.%03zu", kind, stage);
    int b = snprintf(source_id, sizeof(source_id), "stage.source.%s.%03zu", kind, stage);
    if (a < 0 || b < 0 || (size_t)a >= sizeof(bound_id) || (size_t)b >= sizeof(source_id)) return 0;
    const fd2_static_flow *bound = fd2_static_flow_lookup("stage", bound_id);
    const fd2_static_flow *source = fd2_static_flow_lookup("stage", source_id);
    return bound && source && bound->nodes == source->nodes && bound->count == source->count &&
        bound->entry == source->entry;
}

int fd2_field_stage_capability_get(size_t stage, fd2_field_stage_capability *out) {
    char id[64];
    int written = snprintf(id, sizeof(id), "stage.capability.%03zu", stage);
    if (!out || written < 0 || (size_t)written >= sizeof(id)) return -1;
    const fd2_static_flow *flow = fd2_static_flow_lookup("stage", id);
    const fd2_static_flow_node *node = fd2_static_flow_at(flow, flow ? flow->entry : -1);
    if (!node || strcmp(node->op, "stage_capability")) return -1;
    *out = (fd2_field_stage_capability){(uint8_t)node->args[0], (uint8_t)node->args[1],
        (uint8_t)node->args[2], (uint8_t)node->args[3]};
    return 0;
}

int fd2_field_stage_is_supported(size_t stage) {
    fd2_field_stage_capability capability;
    return fd2_field_stage_capability_get(stage, &capability) == 0;
}

int fd2_field_stage_battle_is_supported(size_t stage) {
    fd2_field_stage_capability capability;
    return fd2_field_stage_capability_get(stage, &capability) == 0 && capability.battle_enabled;
}

const uint8_t *fd2_field_stage_prepare_record_get(size_t stage) {
    if (stage == 0 || stage > sizeof(k_stage_prepare) / sizeof(k_stage_prepare[0]))
        return NULL;
    return k_stage_prepare[stage - 1u];
}

const fd2_field_stage_music *fd2_field_stage_music_get(size_t stage) {
    return stage < sizeof(k_stage_music) / sizeof(k_stage_music[0])
        ? &k_stage_music[stage] : NULL;
}

static uint16_t rd_u16_le(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

static uint32_t rd_u32_le(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int fd2_field_map_open_mem(fd2_field_map *map,
                            const uint8_t *data, size_t size) {
    memset(map, 0, sizeof(*map));
    if (!data || size < 8) return -1;

    int width = rd_u16_le(data);
    int height = rd_u16_le(data + 2);
    if (width <= 0 || height <= 0 || width > 256 || height > 256)
        return -1;

    size_t count = (size_t)width * (size_t)height;
    if (size != 4 + count * 4)
        return -1;

    uint32_t *cells = malloc(count * sizeof(*cells));
    if (!cells) return -1;

    for (size_t i = 0; i < count; i++) {
        cells[i] = rd_u32_le(data + 4 + i * 4);
    }

    map->width = width;
    map->height = height;
    map->cells = cells;
    return 0;
}

int fd2_field_map_open_entry(fd2_field_map *map,
                              const fd2_archive *ar, size_t entry_idx) {
    const uint8_t *data;
    size_t size;
    if (fd2_archive_get(ar, entry_idx, &data, &size) != 0) return -1;
    return fd2_field_map_open_mem(map, data, size);
}

int fd2_field_stage_entry(const fd2_archive *ar, size_t stage, unsigned slot,
                          const uint8_t **data, size_t *size) {
    if (slot >= 3 || stage > (SIZE_MAX - slot) / 3u) return -1;
    return fd2_archive_get(ar, stage * 3u + slot, data, size);
}

int fd2_field_map_open_stage(fd2_field_map *map,
                              const fd2_archive *ar, size_t stage_idx) {
    /* FDFIELD.DAT: [stage*3 + 0] 地图、[+1]/[+2] 为事件/单位等附表。 */
    const uint8_t *data;
    size_t size;
    if (fd2_field_stage_entry(ar, stage_idx, 0, &data, &size) != 0) return -1;
    return fd2_field_map_open_mem(map, data, size);
}

int fd2_field_metadata_open_mem(fd2_field_metadata *meta,
                                const uint8_t *data, size_t size) {
    /* stage metadata 结构由 FUN_00017f5b @0x1a813、
     * FUN_0001118c @0x13a44、FUN_000111e7 路径交叉确认：
     * 0x03 起 16*3 回合事件（turn/action/phase），0x33 起 16*2 cell lookup，
     * 0x53 起 16*3 cell action，0x83 起 26 字节单位模板。 */
    memset(meta, 0, sizeof(*meta));
    if (!data || size < 0x83) return -1;
    if ((size - 0x83) % FD2_FIELD_UNIT_TEMPLATE_SIZE != 0) return -1;

    meta->shape_index = data[0];
    meta->unknown_01 = data[1];
    meta->unknown_02 = data[2];

    for (size_t i = 0; i < FD2_FIELD_EVENT_SLOTS; i++) {
        size_t off = 0x03 + i * 3;
        meta->turn_events[i].turn = data[off];
        meta->turn_events[i].action = data[off + 1];
        meta->turn_events[i].phase = data[off + 2];
    }
    for (size_t i = 0; i < FD2_FIELD_EVENT_SLOTS; i++) {
        size_t off = 0x33 + i * 2;
        meta->cell_lookup[i].event_code = data[off];
        meta->cell_lookup[i].match_arg = data[off + 1];
    }
    for (size_t i = 0; i < FD2_FIELD_EVENT_SLOTS; i++) {
        size_t off = 0x53 + i * 3;
        meta->cell_actions[i].mode = data[off];
        meta->cell_actions[i].param = rd_u16_le(data + off + 1);
    }

    meta->unit_template_count = (size - 0x83) / FD2_FIELD_UNIT_TEMPLATE_SIZE;
    if (meta->unit_template_count > 0) {
        meta->unit_templates = malloc(meta->unit_template_count * sizeof(*meta->unit_templates));
        if (!meta->unit_templates) {
            memset(meta, 0, sizeof(*meta));
            return -1;
        }
        for (size_t i = 0; i < meta->unit_template_count; i++) {
            memcpy(meta->unit_templates[i].bytes,
                   data + 0x83 + i * FD2_FIELD_UNIT_TEMPLATE_SIZE,
                   FD2_FIELD_UNIT_TEMPLATE_SIZE);
        }
    }
    meta->source_size = size < sizeof(meta->source_bytes) ?
        size : sizeof(meta->source_bytes);
    memcpy(meta->source_bytes, data, meta->source_size);
    return 0;
}

int fd2_field_metadata_open_stage(fd2_field_metadata *meta,
                                  const fd2_archive *ar, size_t stage_idx) {
    const uint8_t *data;
    size_t size;
    if (fd2_field_stage_entry(ar, stage_idx, 1, &data, &size) != 0) return -1;
    return fd2_field_metadata_open_mem(meta, data, size);
}

void fd2_field_metadata_close(fd2_field_metadata *meta) {
    free(meta->unit_templates);
    memset(meta, 0, sizeof(*meta));
}

int fd2_field_placements_open_mem(fd2_field_placements *placements,
                                  const uint8_t *data, size_t size) {
    /* placement(entry stage*3+2) 的 6 字节记录由 stage 0/31/32 样本
     * 与 metadata 26 字节模板 offset 0x01 的 unit id 交叉验证。 */
    memset(placements, 0, sizeof(*placements));
    if (!data || size < 2) return -1;

    size_t count = rd_u16_le(data);
    if (size != 2 + count * 6) return -1;

    if (count > 0) {
        placements->records = malloc(count * sizeof(*placements->records));
        if (!placements->records) return -1;
        for (size_t i = 0; i < count; i++) {
            size_t off = 2 + i * 6;
            placements->records[i].x = rd_u16_le(data + off);
            placements->records[i].y = rd_u16_le(data + off + 2);
            placements->records[i].unit_id = rd_u16_le(data + off + 4);
        }
    }
    placements->count = count;
    return 0;
}

int fd2_field_placements_open_stage(fd2_field_placements *placements,
                                    const fd2_archive *ar, size_t stage_idx) {
    const uint8_t *data;
    size_t size;
    if (fd2_field_stage_entry(ar, stage_idx, 2, &data, &size) != 0) return -1;
    return fd2_field_placements_open_mem(placements, data, size);
}

void fd2_field_placements_close(fd2_field_placements *placements) {
    free(placements->records);
    memset(placements, 0, sizeof(*placements));
}

uint32_t fd2_field_map_cell(const fd2_field_map *map, int x, int y) {
    if (!map || !map->cells || x < 0 || y < 0 ||
        x >= map->width || y >= map->height)
        return 0;
    return map->cells[(size_t)y * (size_t)map->width + (size_t)x];
}

uint16_t fd2_field_cell_terrain(uint32_t cell) {
    return (uint16_t)(cell & 0x03ffu);
}

uint32_t fd2_field_cell_flags(uint32_t cell) {
    return cell >> 10;
}

void fd2_field_map_close(fd2_field_map *map) {
    free(map->cells);
    memset(map, 0, sizeof(*map));
}
