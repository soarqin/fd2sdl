/* 炎龙骑士团 2 SDL3 重写 - 通用战场单位运行时结构
 *
 * 逆向依据：DAT_00003a45 的 0x50 字节单位记录、
 * map_scene_render_actors @0x1cb94、map_actor_blit_24x24 @0x1da16、
 * field_visible_actor_find_by_text_id @0x12c60，以及 FDFIELD metadata
 * 26 字节模板与同索引 placement（metadata+2为有效前缀）。战场单actor复现
 * sub_127E0 @0x127e0（原登记同名），acted映色依据sub_4E1A6
 * @0x4e1a6（原登记同名）；129ec恢复全表actor附近遮挡格。
 * raw26从VGA共享BIOS tick旁路读取0/1px抖动，不写业务坐标。
 */

#include "field_unit.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "field_unit_base.h"
#include "field_attack.h"

/* field_actor_terrain_overlays_draw @0x129ec（原sub_129EC），经
 * map_tile_blit_visible @0x12ac6（原FUN_0001020e）恢复角色附近遮挡。
 * 必须在全表actor绘完后调用；不是把每个actor与前景交错绘制。 */
void fd2_field_units_render_terrain_overlays(fd2_vga *vga,
    const fd2_terrain_tileset *terrain, const fd2_field_map *map,
    const fd2_field_units *units, int camera_x, int camera_y, int anim_phase,
    const fd2_field_path_result *range, const uint8_t lut[256]) {
    if (!vga || !terrain || !map || !units || units->count > FD2_FIELD_MAX_UNITS)
        return;
    for (size_t i = 0; i < units->count; i++) {
        const fd2_field_unit *unit = &units->items[i];
        if (fd2_field_unit_is_hidden(unit) ||
            fd2_field_attack_unit_ignores_terrain_modifier(unit)) continue;
        int x = unit->x, y = unit->y;
        int cells[4][2] = {{x, y}, {x, y - 1}};
        size_t count = 2;
        if (unit->frame_phase != 0) {
            if (unit->direction == 0 || unit->direction == 2) {
                cells[count][0] = x;
                cells[count++][1] = y + (unit->direction == 0 ? 1 : -2);
            } else {
                int neighbor = x + (unit->direction == 1 ? -1 : 1);
                cells[count][0] = neighbor; cells[count++][1] = y;
                cells[count][0] = neighbor; cells[count++][1] = y - 1;
            }
        }
        for (size_t j = 0; j < count; j++) {
            int cx = cells[j][0], cy = cells[j][1];
            const uint8_t *colors = range && lut &&
                fd2_field_path_is_destination(range, cx, cy) ? lut : NULL;
            fd2_terrain_render_field_overlay_cell(vga, terrain, map,
                cx, cy, camera_x, camera_y, anim_phase, colors);
        }
    }
}

_Static_assert(offsetof(fd2_field_unit, x) == 0x00, "unit.x offset");
_Static_assert(offsetof(fd2_field_unit, y) == 0x01, "unit.y offset");
_Static_assert(offsetof(fd2_field_unit, sprite_cache_class) == 0x02,
               "unit.sprite_cache_class offset");
_Static_assert(offsetof(fd2_field_unit, direction) == 0x03,
               "unit.direction offset");
_Static_assert(offsetof(fd2_field_unit, frame_phase) == 0x04,
               "unit.frame_phase offset");
_Static_assert(offsetof(fd2_field_unit, flags) == 0x05, "unit.flags offset");
_Static_assert(offsetof(fd2_field_unit, side) == 0x06, "unit.side offset");
_Static_assert(offsetof(fd2_field_unit, unit_id) == 0x07,
               "unit.unit_id offset");
_Static_assert(offsetof(fd2_field_unit, text_id) == 0x08,
               "unit.text_id offset");
_Static_assert(offsetof(fd2_field_unit, race) == 0x1f,
               "unit.race offset");
_Static_assert(offsetof(fd2_field_unit, movement_profile) == 0x20,
               "unit.movement_profile offset");
_Static_assert(offsetof(fd2_field_unit, level) == 0x21,
               "unit.level offset");
_Static_assert(offsetof(fd2_field_unit, detail_status) == 0x25,
               "unit.detail_status offset");
_Static_assert(offsetof(fd2_field_unit, ai_behavior_raw) == 0x34,
               "unit.ai_behavior_raw offset");
_Static_assert(offsetof(fd2_field_unit, ai_param_35) == 0x35,
               "unit.ai_param_35 offset");
_Static_assert(offsetof(fd2_field_unit, ai_param_36) == 0x36,
               "unit.ai_param_36 offset");
_Static_assert(offsetof(fd2_field_unit, base_attack_le) == 0x37,
               "unit.base_attack offset");
_Static_assert(offsetof(fd2_field_unit, movement_points) == 0x3b,
               "unit.movement_points offset");
_Static_assert(offsetof(fd2_field_unit, experience) == 0x3c,
               "unit.experience offset");
_Static_assert(offsetof(fd2_field_unit, dexterity_le) == 0x3e,
               "unit.dexterity offset");
_Static_assert(offsetof(fd2_field_unit, hp) == 0x40, "unit.hp offset");
_Static_assert(offsetof(fd2_field_unit, hp_max) == 0x42,
               "unit.hp_max offset");
_Static_assert(offsetof(fd2_field_unit, mp) == 0x44, "unit.mp offset");
_Static_assert(offsetof(fd2_field_unit, mp_max) == 0x46,
               "unit.mp_max offset");
_Static_assert(offsetof(fd2_field_unit, attack) == 0x48,
               "unit.attack offset");
_Static_assert(offsetof(fd2_field_unit, defense) == 0x4a,
               "unit.defense offset");
_Static_assert(offsetof(fd2_field_unit, accuracy) == 0x4c,
               "unit.accuracy offset");
_Static_assert(offsetof(fd2_field_unit, evasion) == 0x4e,
               "unit.evasion offset");
_Static_assert(sizeof(fd2_field_unit) == FD2_FIELD_UNIT_RECORD_SIZE,
               "unit record size");

void fd2_field_units_clear(fd2_field_units *units) {
    if (units) memset(units, 0, sizeof(*units));
}

/* fdicon_cache_append_unit @0x11019（原登记同名）：同ID复用首次插入class。 */
int fd2_field_units_sprite_cache_append(fd2_field_units *units, size_t actor) {
    if (!units || actor >= units->count || units->count > FD2_FIELD_MAX_UNITS ||
        units->sprite_cache_count > FD2_FIELD_MAX_UNITS) return -1;
    size_t cache = 0;
    while (cache < units->sprite_cache_count &&
        units->sprite_cache_ids[cache] != units->items[actor].unit_id) cache++;
    if (cache == FD2_FIELD_MAX_UNITS) return -1;
    if (units->sprite_cache_count == 0) units->sprite_cache_exact = 1;
    if (cache == units->sprite_cache_count) {
        units->sprite_cache_ids[cache] = units->items[actor].unit_id;
        units->sprite_cache_count++;
    }
    units->items[actor].sprite_cache_class = (uint8_t)cache;
    units->sprite_cache_classes[actor] = (uint8_t)cache;
    return 0;
}

/* sub_10010 @0x10010: Load忽略已存class，按当前actor顺序和当前unit ID重建。
 * SDL保持SAV的80B，原重写record+2的结果仅放runtime旁路。 */
int fd2_field_units_sprite_cache_restore(fd2_field_units *units) {
    if (!units || units->count > FD2_FIELD_MAX_UNITS) return -1;
    uint8_t ids[FD2_FIELD_MAX_UNITS];
    uint8_t classes[FD2_FIELD_MAX_UNITS] = {0};
    memset(ids,0xff,sizeof(ids));
    size_t count=0;
    for (size_t i=0;i<units->count;i++) {
        size_t slot=0;
        while (slot<count && ids[slot]!=units->items[i].unit_id) slot++;
        if (slot==count) ids[count++]=units->items[i].unit_id;
        classes[i]=(uint8_t)slot;
    }
    memcpy(units->sprite_cache_ids,ids,sizeof(ids));
    memcpy(units->sprite_cache_classes,classes,sizeof(classes));
    units->sprite_cache_count=count;
    units->sprite_cache_exact=1;
    return 0;
}

int fd2_field_unit_init(fd2_field_unit *unit,
                        uint8_t unit_id, uint8_t text_id, uint8_t side,
                        int x, int y) {
    if (!unit || x < 0 || x > 0xff || y < 0 || y > 0xff)
        return -1;

    memset(unit, 0, sizeof(*unit));
    unit->x = (uint8_t)x;
    unit->y = (uint8_t)y;
    unit->side = side;
    unit->unit_id = unit_id;
    unit->text_id = text_id;
    return 0;
}

int fd2_field_units_set(fd2_field_units *units, size_t index,
                        uint8_t unit_id, uint8_t text_id, uint8_t side,
                        int x, int y) {
    if (!units || index >= FD2_FIELD_MAX_UNITS || units->count > FD2_FIELD_MAX_UNITS ||
        units->sprite_cache_count > FD2_FIELD_MAX_UNITS)
        return -1;
    fd2_field_units saved = *units;
    if (fd2_field_unit_init(&units->items[index], unit_id, text_id, side,
                            x, y) != 0)
        return -1;
    units->walk_frames[index] = 1;
    units->source_template_indices[index] = FD2_FIELD_UNIT_NO_TEMPLATE;
    if (units->count <= index) units->count = index + 1;
    if (fd2_field_units_sprite_cache_append(units, index) != 0) {
        *units = saved;
        return -1;
    }
    return 0;
}

int fd2_field_unit_template_count(const fd2_field_metadata *meta, size_t *count) {
    if (!meta || !count || meta->unknown_02 > meta->unit_template_count ||
        (meta->unknown_02 && !meta->unit_templates)) return -1;
    *count = meta->unknown_02;
    return 0;
}

int fd2_field_unit_deployment_position(const fd2_field_metadata *meta,
        const fd2_field_placements *placements, size_t ordinal,
        uint8_t *x, uint8_t *y) {
    size_t count;
    if (!placements || !x || !y ||
        fd2_field_unit_template_count(meta, &count) != 0 ||
        ordinal >= meta->unknown_01 || count > placements->count ||
        ordinal >= placements->count - count || !placements->records) return -1;
    *x = (uint8_t)placements->records[count + ordinal].x;
    *y = (uint8_t)placements->records[count + ordinal].y;
    return 0;
}

static int placement_for_template(const fd2_field_metadata *meta,
                                  const fd2_field_placements *placements,
                                  size_t template_index,
                                  const fd2_field_placement **out) {
    size_t count;
    if (!placements || !out ||
        fd2_field_unit_template_count(meta, &count) != 0 ||
        template_index >= count || template_index >= placements->count ||
        !placements->records)
        return -1;
    *out = &placements->records[template_index];
    return 0;
}

int fd2_field_units_append_template(fd2_field_units *units,
                                    const fd2_field_metadata *meta,
                                    const fd2_field_placements *placements,
                                    size_t template_index) {
    if (!units || units->count >= FD2_FIELD_MAX_UNITS ||
        !meta || template_index >= meta->unit_template_count)
        return -1;

    const fd2_field_placement *placement;
    if (placement_for_template(meta, placements, template_index,
                               &placement) != 0)
        return -1;
    fd2_field_unit unit;
    memset(&unit, 0, sizeof(unit));
    unit.x = (uint8_t)placement->x;
    unit.y = (uint8_t)placement->y;

    /* field_unit_stage_template_append @0x10c50（原登记同名）重排模板，
     * 并从静态基础表派生战斗字段。 */
    if (fd2_field_unit_stage_template_apply(
            &unit, &meta->unit_templates[template_index]) != 0)
        return -1;

    fd2_field_units shadow = *units;
    size_t index = shadow.count;
    shadow.items[index] = unit;
    shadow.walk_frames[index] = 1;
    shadow.source_template_indices[index] = (uint16_t)template_index;
    shadow.count++;
    if (fd2_field_units_sprite_cache_append(&shadow, index) != 0) return -1;
    *units = shadow;
    return 0;
}

int fd2_field_units_append_group(fd2_field_units *units,
                                 const fd2_field_metadata *meta,
                                 const fd2_field_placements *placements,
                                 uint8_t group) {
    size_t count;
    if (!units || !placements || units->count > FD2_FIELD_MAX_UNITS ||
        fd2_field_unit_template_count(meta, &count) != 0) return -1;
    fd2_field_units shadow = *units;
    for (size_t i = 0; i < count; i++) {
        if (meta->unit_templates[i].bytes[0x15] != group)
            continue;
        if (fd2_field_units_append_template(&shadow, meta, placements, i) != 0)
            return -1;
    }
    *units = shadow;
    return 0;
}

int fd2_field_units_project_group_identity(fd2_field_units *projection,
        const fd2_field_metadata *meta, const fd2_field_placements *placements,
        uint8_t group) {
    size_t count;
    if (!projection || projection->count>FD2_FIELD_MAX_UNITS ||
        fd2_field_unit_template_count(meta,&count)!=0) return -1;
    fd2_field_units shadow=*projection;
    for (size_t i=0;i<count;i++) {
        if (meta->unit_templates[i].bytes[0x15]!=group) continue;
        const fd2_field_placement *position;
        if (shadow.count>=FD2_FIELD_MAX_UNITS ||
            placement_for_template(meta,placements,i,&position)!=0) return -1;
        size_t actor=shadow.count;
        const uint8_t *tpl=meta->unit_templates[i].bytes;
        if (fd2_field_unit_init(&shadow.items[actor],tpl[1],tpl[1],tpl[0],
                (uint8_t)position->x,(uint8_t)position->y)!=0) return -1;
        shadow.walk_frames[actor]=1;
        shadow.source_template_indices[actor]=(uint16_t)i;
        shadow.count++;
    }
    *projection=shadow;
    return 0;
}

/* field_unit_stage_template_append @0x10c50（原登记同名）与53AFA：
 * 非force时按占格40找最近空格；距离平价取后出现的row-major格。 */
int fd2_field_units_append_group_placed(fd2_field_units *units,
        const fd2_field_metadata *meta, const fd2_field_placements *placements,
        const fd2_field_map *map, uint8_t group, int force) {
    if (!units || !meta || !placements || !map || !map->cells ||
        map->width < 1 || map->height < 1 || map->width > 256 || map->height > 256 ||
        units->count > FD2_FIELD_MAX_UNITS) return -1;
    size_t count;
    if (fd2_field_unit_template_count(meta, &count) != 0) return -1;
    fd2_field_units shadow = *units;
    for (size_t t = 0; t < count; t++) {
        if (meta->unit_templates[t].bytes[0x15] != group) continue;
        size_t raw = shadow.count;
        if (fd2_field_units_append_template(&shadow, meta, placements, t) != 0) return -1;
        fd2_field_unit *unit = &shadow.items[raw];
        if (!force) {
            unsigned best = 255u; /* 原最近格搜索初界255；无候选拒绝未初始化坐标。 */
            int bx = -1, by = -1;
            for (int y = 0; y < map->height; y++)
                for (int x = 0; x < map->width; x++) {
                    int occupied = 0;
                    for (size_t i = 0; i < raw; i++)
                        if (!fd2_field_unit_is_hidden(&shadow.items[i]) &&
                            shadow.items[i].x == x && shadow.items[i].y == y) occupied = 1;
                    if (occupied) continue;
                    unsigned d = (unsigned)(abs(x - unit->x) + abs(y - unit->y));
                    if (d <= best) { best = d; bx = x; by = y; }
                }
            if (bx < 0) return -1;
            unit->x = (uint8_t)bx; unit->y = (uint8_t)by;
        } else if (unit->x >= map->width || unit->y >= map->height) return -1;
    }
    *units = shadow;
    return 0;
}

int fd2_field_unit_is_hidden(const fd2_field_unit *unit) {
    return !unit || (unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0;
}

void fd2_field_unit_set_hidden(fd2_field_unit *unit, int hidden) {
    if (!unit) return;
    if (hidden)
        unit->flags |= FD2_FIELD_UNIT_FLAG_HIDDEN;
    else
        unit->flags &= (uint8_t)~FD2_FIELD_UNIT_FLAG_HIDDEN;
}

int fd2_field_unit_has_acted(const fd2_field_unit *unit) {
    return !unit || (unit->flags & FD2_FIELD_UNIT_FLAG_ACTED) != 0;
}

void fd2_field_unit_set_acted(fd2_field_unit *unit, int acted) {
    /* field_actor_mark_acted @0x13512 设置 flags bit 0x80；
     * field_all_actors_clear_acted @0x13536 在阶段切换时清除。 */
    if (!unit) return;
    if (acted)
        unit->flags |= FD2_FIELD_UNIT_FLAG_ACTED;
    else
        unit->flags &= (uint8_t)~FD2_FIELD_UNIT_FLAG_ACTED;
}

int fd2_field_units_find_visible_text_id(const fd2_field_units *units,
                                         uint8_t text_id) {
    if (!units || units->count>FD2_FIELD_MAX_UNITS) return -1;
    for (size_t i = 0; i < units->count; i++) {
        const fd2_field_unit *unit = &units->items[i];
        if (!fd2_field_unit_is_hidden(unit) && unit->text_id == text_id)
            return (int)i;
    }
    return -1;
}

/* callback内联写（235f9/3332b/34dd0/351e6等）与field_actor_range_status_set
 * @0x344f2（原登记同名）。这里只执行一个有界领域写点，不合并事件顺序。 */
int fd2_field_units_field_write(fd2_field_units *units, size_t first, size_t last,
        fd2_field_unit_field field, fd2_field_unit_write operation, uint16_t value) {
    if (!units || units->count > FD2_FIELD_MAX_UNITS || first > last ||
        last >= units->count || operation < FD2_UNIT_SET || operation > FD2_UNIT_OR)
        return -1;
    size_t offset;
    switch (field) {
        case FD2_UNIT_X: offset=offsetof(fd2_field_unit,x); break;
        case FD2_UNIT_Y: offset=offsetof(fd2_field_unit,y); break;
        case FD2_UNIT_DIRECTION: offset=offsetof(fd2_field_unit,direction); break;
        case FD2_UNIT_FRAME_PHASE: offset=offsetof(fd2_field_unit,frame_phase); break;
        case FD2_UNIT_FLAGS: offset=offsetof(fd2_field_unit,flags); break;
        case FD2_UNIT_SIDE: offset=offsetof(fd2_field_unit,side); break;
        case FD2_UNIT_ID: offset=offsetof(fd2_field_unit,unit_id); break;
        case FD2_UNIT_TEXT_ID: offset=offsetof(fd2_field_unit,text_id); break;
        case FD2_UNIT_ATTACK_STATUS: offset=offsetof(fd2_field_unit,attack_status); break;
        case FD2_UNIT_DEFENSE_STATUS: offset=offsetof(fd2_field_unit,defense_status); break;
        case FD2_UNIT_HIT_STATUS: offset=offsetof(fd2_field_unit,hit_evasion_status); break;
        case FD2_UNIT_STATUS_25: offset=offsetof(fd2_field_unit,detail_status); break;
        case FD2_UNIT_STATUS_26: offset=offsetof(fd2_field_unit,detail_status)+1u; break;
        case FD2_UNIT_STATUS_27: offset=offsetof(fd2_field_unit,detail_status)+2u; break;
        case FD2_UNIT_DESCRIPTOR: offset=offsetof(fd2_field_unit,data_28_33)+9u; break;
        case FD2_UNIT_AI_RAW: offset=offsetof(fd2_field_unit,ai_behavior_raw); break;
        case FD2_UNIT_AI_PARAM_35: offset=offsetof(fd2_field_unit,ai_param_35); break;
        case FD2_UNIT_AI_PARAM_36: offset=offsetof(fd2_field_unit,ai_param_36); break;
        case FD2_UNIT_HP: offset=offsetof(fd2_field_unit,hp); break;
        case FD2_UNIT_MP: offset=offsetof(fd2_field_unit,mp); break;
        default: return -1;
    }
    int word = field == FD2_UNIT_HP || field == FD2_UNIT_MP;
    if (!word && value > UINT8_MAX) return -1;
    for (size_t i=first;i<=last;i++) {
        uint8_t *record=(uint8_t *)&units->items[i];
        uint16_t old=record[offset];
        if (word) old|=(uint16_t)record[offset+1u]<<8;
        uint16_t changed=operation==FD2_UNIT_SET ? value :
            operation==FD2_UNIT_AND ? (uint16_t)(old&value) : (uint16_t)(old|value);
        record[offset]=(uint8_t)changed;
        if (word) record[offset+1u]=(uint8_t)(changed>>8);
    }
    return 0;
}

int fd2_field_units_ai_behavior_write(fd2_field_units *units,
        size_t first, size_t last, uint8_t value) {
    if (!units || units->count > FD2_FIELD_MAX_UNITS || first > last ||
        last >= units->count) return -1;
    for (size_t i=first;i<=last;i++)
        units->items[i].ai_behavior_raw=(uint8_t)((units->items[i].ai_behavior_raw&0xf0u)|value);
    return 0;
}

size_t fd2_field_unit_frame_index(const fd2_field_unit *unit,
                                  uint8_t walk_frame,
                                  uint8_t idle_phase) {
    if (!unit || unit->direction > 3u) return SIZE_MAX;
    uint8_t frame = unit->frame_phase != 0 ? walk_frame : idle_phase;
    if (frame>3u) return SIZE_MAX;
    if (frame == 3) frame = 1;
    if (unit->detail_status[1]) frame = 0;
    return (size_t)unit->unit_id * 12u +
           (size_t)unit->direction * 3u +
           (size_t)frame;
}

size_t fd2_field_unit_cached_sprite_id(const fd2_field_units *units, size_t actor) {
    if (!units || units->count > FD2_FIELD_MAX_UNITS || actor >= units->count)
        return SIZE_MAX;
    size_t cache=units->sprite_cache_classes[actor];
    if (units->sprite_cache_exact && units->sprite_cache_count <= FD2_FIELD_MAX_UNITS &&
        cache < units->sprite_cache_count && units->sprite_cache_ids[cache] != 0xffu)
        return units->sprite_cache_ids[cache];
    return units->items[actor].unit_id;
}

size_t fd2_field_unit_cached_frame_index(const fd2_field_units *units,
        size_t actor, uint8_t idle_phase) {
    size_t sprite=fd2_field_unit_cached_sprite_id(units,actor);
    if (sprite==SIZE_MAX) return SIZE_MAX;
    const fd2_field_unit *unit = &units->items[actor];
    if (unit->direction <= 3u) {
        size_t frame=fd2_field_unit_frame_index(unit,units->walk_frames[actor],idle_phase);
        if (frame==SIZE_MAX) return SIZE_MAX;
        return sprite*12u + frame%12u;
    }
    if (unit->direction != 5u || unit->frame_phase != 0 || !units->sprite_cache_exact ||
        units->sprite_cache_count > FD2_FIELD_MAX_UNITS) return SIZE_MAX;
    size_t cache = (size_t)units->sprite_cache_classes[actor] + 1u;
    if (cache >= units->sprite_cache_count || units->sprite_cache_ids[cache] == 0xffu)
        return SIZE_MAX;
    if (idle_phase>3u) return SIZE_MAX;
    uint8_t frame = idle_phase == 3u ? 1u : idle_phase;
    if (unit->detail_status[1]) frame=0;
    return (size_t)units->sprite_cache_ids[cache] * 12u + 3u + frame;
}

static void render_frame(fd2_vga *vga,
                           const fd2_map_sprite_bank *sprites,
                           const fd2_field_unit *unit, uint8_t walk_frame,
                           int camera_x, int camera_y, uint8_t idle_phase,
                           size_t frame_idx) {
    (void)walk_frame; (void)idle_phase;
    if (!vga || !sprites || !sprites->decoded_frames || !unit ||
        fd2_field_unit_is_hidden(unit)) return;
    if (frame_idx >= sprites->frame_count) return;
    int64_t sx = (int64_t)unit->x * 24 - camera_x;
    if (unit->detail_status[1]) sx += vga->actor_jitter_phase & 1u;
    int64_t sy = (int64_t)unit->y * 24 - camera_y - 6;
    int64_t pixel_step = (int64_t)unit->frame_phase * 4;
    switch (unit->direction & 3u) {
        case 0: sy += pixel_step; break;
        case 1: sx -= pixel_step; break;
        case 2: sy -= pixel_step; break;
        case 3: sx += pixel_step; break;
    }
    if (sx <= -24 || sx >= VGA_W || sy <= -24 || sy >= VGA_H) return;
    if (!fd2_field_unit_has_acted(unit)) {
        fd2_map_sprite_blit_frame(vga, sprites, frame_idx, (int)sx, (int)sy, 0);
        return;
    }
    /* sub_4E1A6：仅映色实际写入的pixel，RLE skip保持底图。
     * 真实FDICON全部1680帧无opaque color0，缓存0仅代表skip；
     * 小栈缓冲保留共享decoded cache，不增加持久mask或修改资源。 */
    uint8_t mapped[24 * 24];
    const uint8_t *pixels = sprites->decoded_frames + frame_idx * sizeof(mapped);
    for (size_t i = 0; i < sizeof(mapped); i++)
        mapped[i] = pixels[i] ? (uint8_t)((pixels[i] & 7u) + 24u) : 0;
    const fd2_image image = {24, 24, mapped};
    fd2_map_sprite_blit(vga, &image, (int)sx, (int)sy, 0);
}

void fd2_field_unit_render(fd2_vga *vga, const fd2_map_sprite_bank *sprites,
        const fd2_field_unit *unit, uint8_t walk_frame,
        int camera_x, int camera_y, uint8_t idle_phase) {
    render_frame(vga, sprites, unit, walk_frame, camera_x, camera_y, idle_phase,
        fd2_field_unit_frame_index(unit, walk_frame, idle_phase));
}

void fd2_field_units_render_actor(fd2_vga *vga,
        const fd2_map_sprite_bank *sprites, const fd2_field_units *units,
        size_t actor, int camera_x, int camera_y, uint8_t idle_phase, int offset_y) {
    size_t frame = fd2_field_unit_cached_frame_index(units, actor, idle_phase);
    if (frame == SIZE_MAX) return;
    render_frame(vga, sprites, &units->items[actor], units->walk_frames[actor],
        camera_x, camera_y - offset_y, idle_phase, frame);
}

void fd2_field_units_render(fd2_vga *vga,
                            const fd2_map_sprite_bank *sprites,
                            const fd2_field_units *units,
                            int camera_x, int camera_y,
                            uint8_t idle_phase) {
    if (!vga || !sprites || !units) return;

    size_t order[FD2_FIELD_MAX_UNITS];
    size_t order_count = 0;
    for (size_t i = 0; i < units->count && i < FD2_FIELD_MAX_UNITS; i++) {
        const fd2_field_unit *unit = &units->items[i];
        size_t frame_idx = fd2_field_unit_cached_frame_index(units, i, idle_phase);
        if (fd2_field_unit_is_hidden(unit) || frame_idx >= sprites->frame_count)
            continue;

        size_t pos = order_count++;
        order[pos] = i;
        while (pos > 0) {
            const fd2_field_unit *a = &units->items[order[pos - 1]];
            const fd2_field_unit *b = &units->items[order[pos]];
            if (a->y < b->y || (a->y == b->y && a->x <= b->x)) break;
            size_t temp = order[pos - 1];
            order[pos - 1] = order[pos];
            order[pos] = temp;
            pos--;
        }
    }

    for (size_t oi = 0; oi < order_count; oi++) {
        size_t unit_index = order[oi];
        fd2_field_units_render_actor(vga, sprites, units, unit_index,
                              camera_x, camera_y, idle_phase, 0);
    }
}
