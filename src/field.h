#ifndef FD2_FIELD_H
#define FD2_FIELD_H

#include <stddef.h>
#include <stdint.h>

#include "archive.h"

/* FDFIELD.DAT 战场地图读取
 *
 * 已用 original_game/FDFIELD.DAT 验证：每 3 个条目为一组，组内第 0 个
 * 条目是地图网格：u16 width + u16 height + width*height 个 u32 cell。
 * cell 低 10 位在反编译 FUN_0001020e @0x12ac6 中作为地形/图块索引读取：
 *   (*(ushort *)(cell_ptr + 4) & 0x3ff)
 */

/* sub_4E809 @0x4e809（原登记中性名）：31B城镇准备/商店记录，
 * 数据VA0x6238d，按stage-1索引；完整26条至下一独立学习表0x626b3。
 * SDL只支持这段已证数据，stage0/27+返回NULL；不是原getter的原生边界门。
 * 不代表对应关卡或商店已开放。 */
#define FD2_FIELD_PREPARE_RECORD_SIZE 31
const uint8_t *fd2_field_stage_prepare_record_get(size_t stage);

typedef struct {
    uint8_t primary;
    uint8_t alternate;
} fd2_field_stage_music;

/* music_track_play @0x25977与field_turn_cycle_run @0x1a30b读取
 * 数据VA0x51e63/0x51e81的两张30B表；未知stage返回NULL。
 * 纯查询不播放、不消费RNG，不由此解除关卡能力门。 */
const fd2_field_stage_music *fd2_field_stage_music_get(size_t stage);

typedef struct {
    int width;
    int height;
    uint32_t *cells; /* row-major, width*height */
} fd2_field_map;

#define FD2_FIELD_EVENT_SLOTS 16
#define FD2_FIELD_UNIT_TEMPLATE_SIZE 26

typedef struct {
    uint8_t turn;
    uint8_t action;
    uint8_t phase;
} fd2_field_turn_event;

typedef struct {
    uint8_t event_code;
    uint8_t match_arg;
} fd2_field_cell_lookup;

typedef struct {
    uint8_t mode;
    uint16_t param;
} fd2_field_cell_action;

typedef struct {
    uint8_t bytes[FD2_FIELD_UNIT_TEMPLATE_SIZE];
} fd2_field_unit_template;

typedef struct {
    uint8_t shape_index;
    uint8_t unknown_01;
    uint8_t unknown_02;
    fd2_field_turn_event turn_events[FD2_FIELD_EVENT_SLOTS];
    fd2_field_cell_lookup cell_lookup[FD2_FIELD_EVENT_SLOTS];
    fd2_field_cell_action cell_actions[FD2_FIELD_EVENT_SLOTS];
    size_t unit_template_count;
    fd2_field_unit_template *unit_templates;
} fd2_field_metadata;

typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t unit_id; /* 匹配 metadata 26 字节模板 offset 0x01 */
} fd2_field_placement;

typedef struct {
    size_t count;
    fd2_field_placement *records;
} fd2_field_placements;

int  fd2_field_map_open_mem(fd2_field_map *map,
                            const uint8_t *data, size_t size);
int  fd2_field_map_open_entry(fd2_field_map *map,
                              const fd2_archive *ar, size_t entry_idx);
int  fd2_field_map_open_stage(fd2_field_map *map,
                              const fd2_archive *ar, size_t stage_idx);

int  fd2_field_metadata_open_mem(fd2_field_metadata *meta,
                                 const uint8_t *data, size_t size);
int  fd2_field_metadata_open_stage(fd2_field_metadata *meta,
                                   const fd2_archive *ar, size_t stage_idx);
void fd2_field_metadata_close(fd2_field_metadata *meta);

int  fd2_field_placements_open_mem(fd2_field_placements *placements,
                                   const uint8_t *data, size_t size);
int  fd2_field_placements_open_stage(fd2_field_placements *placements,
                                     const fd2_archive *ar, size_t stage_idx);
void fd2_field_placements_close(fd2_field_placements *placements);

uint32_t fd2_field_map_cell(const fd2_field_map *map, int x, int y);
uint16_t fd2_field_cell_terrain(uint32_t cell);
uint32_t fd2_field_cell_flags(uint32_t cell);

void fd2_field_map_close(fd2_field_map *map);

#endif /* FD2_FIELD_H */
