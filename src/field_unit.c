/* 炎龙骑士团 2 SDL3 重写 - 通用战场单位运行时结构
 *
 * 逆向依据：DAT_00003a45 的 0x50 字节单位记录、
 * map_scene_render_actors @0x41db2、map_actor_blit_24x24 @0x42c34、
 * field_visible_actor_find_by_text_id @0x37e74，以及 FDFIELD metadata
 * 26 字节模板与 placement 的顺序匹配关系。
 */

#include "field_unit.h"

#include <stddef.h>
#include <string.h>

#include "field_unit_base.h"

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
    if (!units || index >= FD2_FIELD_MAX_UNITS)
        return -1;
    if (fd2_field_unit_init(&units->items[index], unit_id, text_id, side,
                            x, y) != 0)
        return -1;
    units->walk_frames[index] = 1;
    units->source_template_indices[index] = FD2_FIELD_UNIT_NO_TEMPLATE;
    if (units->count <= index) units->count = index + 1;
    return 0;
}

static int placement_for_template(const fd2_field_metadata *meta,
                                  const fd2_field_placements *placements,
                                  size_t template_index,
                                  const fd2_field_placement **out) {
    if (!meta || !placements || !out ||
        template_index >= meta->unit_template_count)
        return -1;

    uint8_t unit_id = meta->unit_templates[template_index].bytes[1];
    size_t ordinal = 0;
    for (size_t i = 0; i < template_index; i++) {
        if (meta->unit_templates[i].bytes[1] == unit_id)
            ordinal++;
    }

    for (size_t i = 0; i < placements->count; i++) {
        if (placements->records[i].unit_id != unit_id)
            continue;
        if (ordinal == 0) {
            *out = &placements->records[i];
            return 0;
        }
        ordinal--;
    }
    return -1;
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
    if (placement->x > 0xffu || placement->y > 0xffu)
        return -1;

    fd2_field_unit unit;
    memset(&unit, 0, sizeof(unit));
    unit.x = (uint8_t)placement->x;
    unit.y = (uint8_t)placement->y;

    /* field_unit_stage_template_append @corrected dual 0x35e6e
     *（code0 0x25e6e）重排模板，并从静态基础表派生战斗字段。 */
    if (fd2_field_unit_stage_template_apply(
            &unit, &meta->unit_templates[template_index]) != 0)
        return -1;

    size_t index = units->count;
    units->items[index] = unit;
    units->walk_frames[index] = 1;
    units->source_template_indices[index] = (uint16_t)template_index;
    units->count++;
    return 0;
}

int fd2_field_units_append_group(fd2_field_units *units,
                                 const fd2_field_metadata *meta,
                                 const fd2_field_placements *placements,
                                 uint8_t group) {
    if (!units || !meta || !placements) return -1;

    size_t initial_count = units->count;
    for (size_t i = 0; i < meta->unit_template_count; i++) {
        if (meta->unit_templates[i].bytes[0x15] != group)
            continue;
        if (fd2_field_units_append_template(units, meta, placements, i) == 0)
            continue;
        for (size_t j = initial_count; j < units->count; j++) {
            memset(&units->items[j], 0, sizeof(units->items[j]));
            units->walk_frames[j] = 0;
            units->source_template_indices[j] = FD2_FIELD_UNIT_NO_TEMPLATE;
        }
        units->count = initial_count;
        return -1;
    }
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
    /* field_actor_mark_acted @0x38726 设置 flags bit 0x80；
     * field_all_actors_clear_acted @0x3874a 在阶段切换时清除。 */
    if (!unit) return;
    if (acted)
        unit->flags |= FD2_FIELD_UNIT_FLAG_ACTED;
    else
        unit->flags &= (uint8_t)~FD2_FIELD_UNIT_FLAG_ACTED;
}

int fd2_field_units_find_visible_text_id(const fd2_field_units *units,
                                         uint8_t text_id) {
    if (!units) return -1;
    for (size_t i = 0; i < units->count; i++) {
        const fd2_field_unit *unit = &units->items[i];
        if (!fd2_field_unit_is_hidden(unit) && unit->text_id == text_id)
            return (int)i;
    }
    return -1;
}

size_t fd2_field_unit_frame_index(const fd2_field_unit *unit,
                                  uint8_t walk_frame,
                                  uint8_t idle_phase) {
    if (!unit) return SIZE_MAX;
    uint8_t frame = unit->frame_phase != 0 ? walk_frame : idle_phase;
    if (frame == 3) frame = 1;
    return (size_t)unit->unit_id * 12u +
           (size_t)(unit->direction & 3u) * 3u +
           (size_t)frame;
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
        size_t frame_idx = fd2_field_unit_frame_index(
            unit, units->walk_frames[i], idle_phase);
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
        const fd2_field_unit *unit = &units->items[unit_index];
        size_t frame_idx = fd2_field_unit_frame_index(
            unit, units->walk_frames[unit_index], idle_phase);
        int sx = (int)unit->x * 24 - camera_x;
        int sy = (int)unit->y * 24 - camera_y - 6;
        int pixel_step = (int)unit->frame_phase * 4;
        switch (unit->direction & 3u) {
            case 0: sy += pixel_step; break;
            case 1: sx -= pixel_step; break;
            case 2: sy -= pixel_step; break;
            case 3: sx += pixel_step; break;
        }
        fd2_map_sprite_blit_frame(vga, sprites, frame_idx, sx, sy, 0);
    }
}
