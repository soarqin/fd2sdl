#include "field.h"

#include <stdlib.h>
#include <string.h>

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
    if (width != 0 && count / (size_t)width != (size_t)height)
        return -1;
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

int fd2_field_map_open_stage(fd2_field_map *map,
                              const fd2_archive *ar, size_t stage_idx) {
    /* FDFIELD.DAT: [stage*3 + 0] 地图、[+1]/[+2] 为事件/单位等附表。 */
    return fd2_field_map_open_entry(map, ar, stage_idx * 3);
}

int fd2_field_metadata_open_mem(fd2_field_metadata *meta,
                                const uint8_t *data, size_t size) {
    /* stage metadata 结构由 FUN_00017f5b @0x3fa27、
     * FUN_0001118c @0x38c58、FUN_000111e7 路径交叉确认：
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
    return 0;
}

int fd2_field_metadata_open_stage(fd2_field_metadata *meta,
                                  const fd2_archive *ar, size_t stage_idx) {
    const uint8_t *data;
    size_t size;
    if (fd2_archive_get(ar, stage_idx * 3 + 1, &data, &size) != 0) return -1;
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
    if (fd2_archive_get(ar, stage_idx * 3 + 2, &data, &size) != 0) return -1;
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
