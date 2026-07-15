/* 炎龙骑士团 2 SDL3 重写 - 战场 AI 纯查询层
 *
 * corrected code0 0x28cb3 从 actor +0x34 低半字节分派 AI behavior；
 * 0x290b0 按阵营选择最近目标，0x29d8c 对可达目的地作稳定排序；
 * 0x2944b、0x2ab9e、0x2a892 枚举物理／法术／物品候选，
 * 0x2ad8b/0x2afb6/0x2aa94 计算非物理目标分数。本模块只复现已确认的
 * 查询和计划，不提前翻译尚未完成的效果、消耗与 phase 提交。
 */
#include "field_ai.h"

#include <stdlib.h>

#include "field_attack.h"
#include "field_item.h"
#include "field_magic.h"

static uint32_t abs_diff(int a, int b) {
    return a >= b ? (uint32_t)(a - b) : (uint32_t)(b - a);
}

static uint32_t abs_int(int value) {
    return value >= 0 ? (uint32_t)value : (uint32_t)(-value);
}

static uint32_t abs_i64(int64_t value) {
    uint64_t magnitude = value >= 0 ? (uint64_t)value : (uint64_t)(-value);
    return magnitude > UINT32_MAX ? UINT32_MAX : (uint32_t)magnitude;
}

uint8_t fd2_field_ai_behavior(const fd2_field_unit *unit) {
    return unit ? (uint8_t)(unit->ai_behavior_raw & 0x0fu) : 0xffu;
}

int fd2_field_ai_nearest_opponent(const fd2_field_units *units,
                                  size_t actor_index,
                                  uint8_t side,
                                  fd2_field_ai_target *target) {
    if (!units || !target || actor_index >= units->count)
        return -1;

    const fd2_field_unit *actor = &units->items[actor_index];
    size_t best_index = SIZE_MAX;
    uint32_t best_distance = 0xffffu;
    for (size_t i = 0; i < units->count; i++) {
        const fd2_field_unit *candidate = &units->items[i];
        int eligible = side == 0u ? candidate->side != 0u
                                 : candidate->side == 0u;
        if (!eligible) continue;
        uint32_t distance = abs_diff(actor->x, candidate->x) +
                            abs_diff(actor->y, candidate->y);
        if (distance >= best_distance) continue;
        best_index = i;
        best_distance = distance;
    }
    if (best_index == SIZE_MAX) return -1;

    target->unit_index = best_index;
    target->x = units->items[best_index].x;
    target->y = units->items[best_index].y;
    target->manhattan_distance = best_distance;
    return 0;
}

int fd2_field_ai_choose_destination(
        const fd2_field_path_result *range,
        int anchor_x, int anchor_y,
        fd2_field_ai_destination *destination) {
    if (!range || !range->nodes || !destination || range->width <= 0 ||
        range->height <= 0 || anchor_x < 0 || anchor_x >= range->width ||
        anchor_y < 0 || anchor_y >= range->height)
        return -1;

    int best_x = -1;
    int best_y = -1;
    uint32_t best_distance = 0xffu;
    uint32_t best_tie = 0xffu;
    uint32_t best_path_cost = FD2_FIELD_PATH_UNREACHABLE;
    for (int y = 0; y < range->height; y++) {
        for (int x = 0; x < range->width; x++) {
            if (!fd2_field_path_is_destination(range, x, y)) continue;
            int dx = x - anchor_x;
            int dy = y - anchor_y;
            uint32_t distance = abs_int(dx) + abs_int(dy);
            /* corrected code0 0x2a020..0x2a04c：编译结果确实是
             * abs(dx - abs(dy))，不是常见的 abs(abs(dx)-abs(dy))。 */
            uint32_t tie = abs_i64((int64_t)dx - abs_int(dy));
            if (distance > best_distance ||
                (distance == best_distance && tie >= best_tie))
                continue;
            best_x = x;
            best_y = y;
            best_distance = distance;
            best_tie = tie;
            best_path_cost = fd2_field_path_distance(range, x, y);
        }
    }
    if (best_x < 0) return -1;

    destination->x = best_x;
    destination->y = best_y;
    destination->manhattan_distance = best_distance;
    destination->tie_score = best_tie;
    destination->path_cost = best_path_cost;
    return 0;
}

int fd2_field_ai_physical_candidate_is_better(
        const fd2_field_ai_physical_candidate *candidate,
        const fd2_field_ai_physical_candidate *best,
        int have_best) {
    if (!candidate || !best) return 0;
    if (!have_best) return 1;
    return candidate->priority > best->priority ||
        (candidate->priority == best->priority &&
         candidate->secondary_score > best->secondary_score);
}

static int target_matches_range_filter(const fd2_field_unit *target,
                                       uint8_t filter) {
    if (!target || (target->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0)
        return 0;
    switch (filter) {
        case 0: return target->side == 0u;
        case 1: return target->side != 0u;
        case 2: return target->side == 1u;
        case 3: return target->side == 2u;
        default: return 0;
    }
}

static int collect_range_targets(
        const fd2_field_path_result *range,
        const fd2_field_units *units,
        uint8_t filter,
        uint8_t *indices,
        size_t capacity,
        size_t *count) {
    if (!range || !units || !indices || !count) return -1;
    size_t result_count = 0;
    for (size_t i = 0; i < units->count; i++) {
        const fd2_field_unit *target = &units->items[i];
        if (!target_matches_range_filter(target, filter) ||
            !fd2_field_path_is_destination(range, target->x, target->y))
            continue;
        if (result_count >= capacity || i > UINT8_MAX) return -1;
        indices[result_count++] = (uint8_t)i;
    }
    *count = result_count;
    return 0;
}

static int candidate_target_eligible(const fd2_field_unit *target,
                                     uint8_t side_selector) {
    if (!target || (target->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0)
        return 0;
    return side_selector == 0u ? target->side != 0u : target->side == 0u;
}

static int attack_percent_at(const fd2_field_map *map,
                             const fd2_terrain_tileset *terrain,
                             int x, int y,
                             int use_attack_percent,
                             int32_t *percent) {
    if (!map || !map->cells || !terrain || !terrain->attrs || !percent ||
        x < 0 || y < 0 || x >= map->width || y >= map->height)
        return -1;
    uint32_t cell = map->cells[(size_t)y * (size_t)map->width + (size_t)x];
    uint16_t terrain_id = (uint16_t)(cell & 0x03ffu);
    if (terrain_id >= terrain->attr_count) return -1;
    int32_t attack_percent = 0;
    int32_t defense_percent = 0;
    if (fd2_field_attack_terrain_modifiers(
            terrain->attrs[terrain_id].movement_cost_class,
            &attack_percent, &defense_percent) != 0)
        return -1;
    *percent = use_attack_percent ? attack_percent : defense_percent;
    return 0;
}

int fd2_field_ai_choose_physical_candidate(
        const fd2_field_map *map,
        const fd2_terrain_tileset *terrain,
        const fd2_field_units *units,
        size_t actor_index,
        uint8_t side_selector,
        const fd2_field_path_result *move_range,
        fd2_field_ai_physical_candidate *candidate) {
    if (!map || !terrain || !units ||
        units->count > FD2_FIELD_MAX_UNITS || actor_index >= units->count ||
        map->width > UINT8_MAX || map->height > UINT8_MAX ||
        !move_range || !move_range->nodes || !candidate ||
        move_range->width != map->width || move_range->height != map->height)
        return -1;
    const fd2_field_unit *actor = &units->items[actor_index];
    uint8_t min_range = 0;
    uint8_t max_range = 0;
    if (fd2_field_attack_weapon_range(actor, &min_range, &max_range) != 0)
        return -1;

    int have_best = 0;
    fd2_field_ai_physical_candidate best = {0};
    for (int y = 0; y < move_range->height; y++) {
        for (int x = 0; x < move_range->width; x++) {
            if (!fd2_field_path_is_destination(move_range, x, y)) continue;

            uint32_t adjusted_attack = actor->attack;
            uint32_t adjusted_defense = actor->defense;
            if (!fd2_field_attack_unit_ignores_terrain_modifier(actor)) {
                int32_t attack_percent = 0;
                int32_t defense_percent = 0;
                if (attack_percent_at(map, terrain, x, y, 1,
                                      &attack_percent) != 0 ||
                    attack_percent_at(map, terrain, x, y, 0,
                                      &defense_percent) != 0 ||
                    fd2_field_attack_apply_terrain_modifier(
                        actor->attack, attack_percent,
                        &adjusted_attack) != 0 ||
                    fd2_field_attack_apply_terrain_modifier(
                        actor->defense, defense_percent,
                        &adjusted_defense) != 0)
                    return -1;
            }

            fd2_field_unit positioned_actor = *actor;
            positioned_actor.x = (uint8_t)x;
            positioned_actor.y = (uint8_t)y;
            fd2_field_path_result attack_range =
                FD2_FIELD_PATH_RESULT_INITIALIZER;
            if (fd2_field_attack_range_compute(
                    &attack_range, map, terrain, &positioned_actor) != 0)
                return -1;

            size_t target_count = 0;
            for (size_t i = 0; i < units->count; i++) {
                const fd2_field_unit *target = &units->items[i];
                if (candidate_target_eligible(target, side_selector) &&
                    fd2_field_path_is_destination(
                        &attack_range, target->x, target->y))
                    target_count++;
            }
            if (target_count == 0) {
                fd2_field_path_close(&attack_range);
                continue;
            }

            /* corrected code0 0x29663 只在每个 destination 开始时把
             * target_count 装入 ESI；0x29697/0x296bd/0x296d7 对 ESI 的
             * 修改会传给同一 destination 的下一个 target。这一累积行为
             * 看似反常，但不能在 target 循环内重置。 */
            int32_t score = target_count > INT32_MAX
                ? INT32_MAX : (int32_t)target_count;
            for (size_t i = 0; i < units->count; i++) {
                const fd2_field_unit *target = &units->items[i];
                if (!candidate_target_eligible(target, side_selector))
                    continue;
                if (!fd2_field_path_is_destination(
                        &attack_range, target->x, target->y))
                    continue;
                uint32_t distance = abs_diff(x, target->x) +
                                    abs_diff(y, target->y);

                int32_t priority = 0;
                if (score > target->hp) {
                    score = score > INT32_MAX / 2 ? INT32_MAX : score * 2;
                    priority = 0x12;
                } else {
                    /* 0x3979a..0x397ae：terrain-adjusted attack-defense > 2
                     * 令 priority=8。比较严格按机器码保留 first winner。 */
                    uint32_t adjusted_target_defense = target->defense;
                    if (!fd2_field_attack_unit_ignores_terrain_modifier(target)) {
                        int32_t defense_percent = 0;
                        if (attack_percent_at(
                                map, terrain, target->x, target->y, 0,
                                &defense_percent) != 0 ||
                            fd2_field_attack_apply_terrain_modifier(
                                target->defense, defense_percent,
                                &adjusted_target_defense) != 0) {
                            fd2_field_path_close(&attack_range);
                            return -1;
                        }
                    }
                    priority = adjusted_attack > adjusted_target_defense + 2u
                        ? 8 : 0;
                }

                if (distance == 1u && target->detail_status[1] == 0u) {
                    uint8_t target_min_range = 0;
                    uint8_t target_max_range = 0;
                    if (fd2_field_attack_weapon_range(
                            target, &target_min_range,
                            &target_max_range) == 0 &&
                        target_min_range <= 1u) {
                        uint32_t adjusted_target_attack = target->attack;
                        if (!fd2_field_attack_unit_ignores_terrain_modifier(
                                target)) {
                            int32_t target_attack_percent = 0;
                            if (attack_percent_at(
                                    map, terrain, target->x, target->y, 1,
                                    &target_attack_percent) != 0 ||
                                fd2_field_attack_apply_terrain_modifier(
                                    target->attack, target_attack_percent,
                                    &adjusted_target_attack) != 0) {
                                fd2_field_path_close(&attack_range);
                                return -1;
                            }
                        }
                        int64_t combined = (int64_t)score +
                            (int64_t)adjusted_defense -
                            (int64_t)adjusted_target_attack;
                        score = combined < INT32_MIN ? INT32_MIN :
                            combined > INT32_MAX ? INT32_MAX :
                            (int32_t)combined;
                    }
                }
                if (target->text_id == 0u) {
                    int64_t scaled = (int64_t)score * 3;
                    score = (int32_t)(scaled >= 0 ? scaled / 2
                                                  : -((-scaled) / 2));
                }

                fd2_field_ai_physical_candidate current = {
                    .unit_index = i,
                    .destination_x = x,
                    .destination_y = y,
                    .priority = priority,
                    .secondary_score = score,
                };
                if (fd2_field_ai_physical_candidate_is_better(
                        &current, &best, have_best)) {
                    best = current;
                    have_best = 1;
                }
            }
            fd2_field_path_close(&attack_range);
        }
    }
    if (!have_best) return -1;
    *candidate = best;
    return 0;
}

static int score_targets_valid(const fd2_field_ai_score_targets *targets) {
    if (!targets || !targets->units ||
        targets->units->count > FD2_FIELD_MAX_UNITS ||
        (targets->target_count != 0 && !targets->target_indices))
        return 0;
    for (size_t i = 0; i < targets->target_count; i++) {
        if ((size_t)targets->target_indices[i] >= targets->units->count)
            return 0;
    }
    return 1;
}

static int32_t score_add_saturating(int32_t total, int32_t add) {
    int64_t value = (int64_t)total + (int64_t)add;
    return value > INT32_MAX ? INT32_MAX :
           value < INT32_MIN ? INT32_MIN : (int32_t)value;
}

static int32_t score_zero_byte_targets(
        const fd2_field_ai_score_targets *targets,
        size_t unit_offset, int32_t add) {
    int32_t total = 0;
    for (size_t i = 0; i < targets->target_count; i++) {
        const uint8_t *unit =
            (const uint8_t *)&targets->units->items[targets->target_indices[i]];
        if (unit[unit_offset] == 0u)
            total = score_add_saturating(total, add);
    }
    return total;
}

int fd2_field_ai_magic_targets_score(
        uint8_t magic_id,
        const fd2_field_ai_score_targets *targets,
        int32_t *score) {
    if (!score || !score_targets_valid(targets)) return -1;
    const uint8_t *magic = fd2_field_magic_record_get(magic_id);
    if (!magic) return -1;

    int32_t total = 0;
    uint16_t value = fd2_field_magic_u16(magic, 0);
    if (magic_id < 13u) {
        for (size_t i = 0; i < targets->target_count; i++) {
            const fd2_field_unit *target =
                &targets->units->items[targets->target_indices[i]];
            int32_t add = target->hp >= value ? 8 : 24;
            /* corrected code0 0x2add3 的 DS qword fixup 指向 bound
             * payload code0 0x64358，常量为 1.5；0x4cd08 以 x87
             * round-to-nearest 转为整数。8/24 的结果精确为 12/36。 */
            if (target->text_id == 0u) add = add * 3 / 2;
            total = score_add_saturating(total, add);
        }
    } else if (magic_id < 17u) {
        for (size_t i = 0; i < targets->target_count; i++) {
            const fd2_field_unit *target =
                &targets->units->items[targets->target_indices[i]];
            int32_t add = target->hp < target->hp_max / 3u ? 8 :
                          target->hp < target->hp_max / 2u ? 3 : 0;
            if ((target->ai_behavior_raw & 0x01u) != 0) add *= 2;
            total = score_add_saturating(total, add);
        }
    } else if (magic_id < 20u) {
        total = score_zero_byte_targets(
            targets, (size_t)magic_id + 17u, 3);
    } else if (magic_id == 20u || magic_id == 21u) {
        size_t offset = magic_id == 20u ? 0x25u : 0x26u;
        for (size_t i = 0; i < targets->target_count; i++) {
            const uint8_t *target = (const uint8_t *)&targets->units->items[
                targets->target_indices[i]];
            if (target[offset] != 0u)
                total = score_add_saturating(total, 6);
        }
    } else if (magic_id == 22u) {
        for (size_t i = 0; i < targets->target_count; i++) {
            const fd2_field_unit *target =
                &targets->units->items[targets->target_indices[i]];
            int have_magic = 0;
            for (size_t byte_index = 0; byte_index < 5u; byte_index++) {
                if (target->data_09_1e[0x11u + byte_index] != 0u) {
                    have_magic = 1;
                    break;
                }
            }
            if (target->detail_status[2] == 0u && have_magic)
                total = score_add_saturating(total, 6);
        }
    } else if (magic_id == 26u || magic_id == 27u) {
        total = score_zero_byte_targets(
            targets, magic_id == 26u ? 0x25u : 0x26u, 4);
    }
    *score = total;
    return 0;
}

int fd2_field_ai_magic_candidate_is_better(
        const fd2_field_ai_magic_candidate *candidate,
        const fd2_field_ai_magic_candidate *best,
        int have_best) {
    if (!candidate || !best) return 0;
    if (!have_best) return candidate->score > 0;
    return candidate->score > best->score ||
        (candidate->score == best->score &&
         candidate->tie_value > best->tie_value);
}

static size_t unit_magic_list_build(
        const fd2_field_unit *unit, uint8_t *magic_ids, size_t capacity) {
    if (!unit) return 0;
    size_t count = 0;
    for (size_t byte_index = 0; byte_index < 5u; byte_index++) {
        uint8_t bits = unit->data_09_1e[0x11u + byte_index];
        for (size_t bit = 0; bit < 8u; bit++) {
            if ((bits & (uint8_t)(1u << bit)) == 0) continue;
            if (magic_ids && count < capacity)
                magic_ids[count] = (uint8_t)(byte_index * 8u + bit);
            count++;
        }
    }
    return count;
}

int fd2_field_ai_choose_magic_candidate(
        const fd2_field_map *map,
        const fd2_terrain_tileset *terrain,
        const fd2_field_units *units,
        size_t actor_index,
        uint8_t selector_mode,
        fd2_field_ai_magic_candidate *candidate) {
    if (!map || !terrain || !units || actor_index >= units->count ||
        units->count > FD2_FIELD_MAX_UNITS || !candidate)
        return -1;
    const fd2_field_unit *actor = &units->items[actor_index];
    if (actor->detail_status[2] != 0u) return -1;

    uint8_t magic_ids[40];
    size_t magic_count = unit_magic_list_build(
        actor, magic_ids, sizeof(magic_ids));
    if (magic_count == 0 || magic_count > sizeof(magic_ids)) return -1;

    int have_best = 0;
    fd2_field_ai_magic_candidate best = {0};
    for (size_t mi = 0; mi < magic_count; mi++) {
        uint8_t magic_id = magic_ids[mi];
        const uint8_t *magic = fd2_field_magic_record_get(magic_id);
        if (!magic || magic[5] > actor->mp) continue;

        fd2_field_path_result cast_range =
            FD2_FIELD_PATH_RESULT_INITIALIZER;
        if (fd2_field_target_range_compute(
                &cast_range, map, terrain, actor->x, actor->y,
                0, magic[3]) != 0)
            return -1;
        for (int y = 0; y < cast_range.height; y++) {
            for (int x = 0; x < cast_range.width; x++) {
                if (!fd2_field_path_is_destination(&cast_range, x, y))
                    continue;
                fd2_field_path_result target_range =
                    FD2_FIELD_PATH_RESULT_INITIALIZER;
                if (fd2_field_target_range_compute(
                        &target_range, map, terrain, x, y,
                        0, magic[4]) != 0) {
                    fd2_field_path_close(&cast_range);
                    return -1;
                }
                uint8_t target_indices[FD2_FIELD_MAX_UNITS];
                size_t target_count = 0;
                /* corrected code0 0x2ad54..0x2ad70：selector_mode 非 0
                 * 直接使用 spell +6；为 0 时把 +6==0 反转为 filter 1，
                 * +6!=0 则使用 filter 0。 */
                uint8_t filter = selector_mode != 0u ? magic[6] :
                    magic[6] != 0u ? 0u : 1u;
                if (collect_range_targets(
                        &target_range, units, filter, target_indices,
                        sizeof(target_indices), &target_count) != 0) {
                    fd2_field_path_close(&target_range);
                    fd2_field_path_close(&cast_range);
                    return -1;
                }
                fd2_field_path_close(&target_range);
                if (target_count == 0) continue;

                fd2_field_ai_score_targets targets = {
                    units, target_indices, target_count,
                };
                fd2_field_ai_magic_candidate current = {
                    .magic_id = magic_id,
                    .cast_x = x,
                    .cast_y = y,
                    .tie_value = fd2_field_magic_u16(magic, 0),
                };
                if (fd2_field_ai_magic_targets_score(
                        magic_id, &targets, &current.score) != 0) {
                    fd2_field_path_close(&cast_range);
                    return -1;
                }
                if (fd2_field_ai_magic_candidate_is_better(
                        &current, &best, have_best)) {
                    best = current;
                    have_best = 1;
                }
            }
        }
        fd2_field_path_close(&cast_range);
    }
    if (!have_best) return -1;
    *candidate = best;
    return 0;
}

int fd2_field_ai_item_targets_score(
        uint8_t item_id,
        const fd2_field_ai_score_targets *targets,
        int32_t *score) {
    if (!score || !score_targets_valid(targets)) return -1;
    const uint8_t *item = fd2_field_item_record_get(item_id);
    if (!item) return -1;

    uint8_t code = item[0x0du];
    uint16_t value = (uint16_t)item[0x0eu] |
                     ((uint16_t)item[0x0fu] << 8);
    int32_t total = 0;
    if (code == 5u || code == 13u) {
        for (size_t i = 0; i < targets->target_count; i++) {
            const fd2_field_unit *target =
                &targets->units->items[targets->target_indices[i]];
            /* 0x2ab19 覆盖了入口暂存 value 的 EBP：code 5/13 实际
             * 只比较目标当前／最大 HP。<=1/3 取 8，随后 <1/2 取 3，
             * 其余取 0；+0x34 bit7 再乘 3。 */
            int32_t add = target->hp <= target->hp_max / 3u ? 8 :
                          target->hp <= target->hp_max / 2u ? 3 : 0;
            if ((target->ai_behavior_raw & 0x80u) != 0) add *= 3;
            total = score_add_saturating(total, add);
        }
    } else if (code == 20u || code == 21u || code == 24u) {
        uint16_t threshold = value;
        if (code != 24u) {
            if (value >= FD2_FIELD_MAGIC_COUNT) return -1;
            threshold = fd2_field_magic_u16(
                fd2_field_magic_record_get((uint8_t)value), 0);
        }
        for (size_t i = 0; i < targets->target_count; i++) {
            const fd2_field_unit *target =
                &targets->units->items[targets->target_indices[i]];
            if (threshold < target->hp)
                total = score_add_saturating(total, 8);
        }
    }
    *score = total;
    return 0;
}

int fd2_field_ai_item_candidate_is_better(
        const fd2_field_ai_item_candidate *candidate,
        const fd2_field_ai_item_candidate *best,
        int have_best) {
    if (!candidate || !best) return 0;
    return !have_best ? candidate->score > 0 :
                        candidate->score > best->score;
}

static size_t unit_inventory_count(const fd2_field_unit *unit) {
    if (!unit) return 0;
    const uint8_t *record = (const uint8_t *)unit;
    size_t count = 0;
    for (size_t slot = 0; slot < 8u; slot++) {
        if ((record[0x0au + slot * 2u] & 0x80u) == 0) count++;
    }
    return count;
}

static int collect_line_targets(
        const fd2_field_units *units,
        int origin_x, int origin_y,
        int target_x, int target_y,
        uint8_t steps,
        uint8_t filter_nonzero_side,
        uint8_t *indices,
        size_t capacity,
        size_t *count) {
    if (!units || !indices || !count) return -1;
    /* FUN_00039c16 @0x39c16 从候选格开始，沿轴向朝 actor 原点走
     * `steps` 格；候选距离小于 steps 时会越过 actor 继续同方向。 */
    int dx = target_x == origin_x ? 0 : origin_x < target_x ? -1 : 1;
    int dy = target_x == origin_x
        ? (origin_y < target_y ? -1 : 1) : 0;
    size_t result_count = 0;
    int x = target_x;
    int y = target_y;
    for (uint8_t step = 0; step < steps; step++) {
        x += dx;
        y += dy;
        for (size_t i = 0; i < units->count; i++) {
            const fd2_field_unit *target = &units->items[i];
            if ((target->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) != 0 ||
                target->x != x || target->y != y)
                continue;
            int side_ok = filter_nonzero_side == 0u
                ? target->side != 0u : target->side == 0u;
            if (!side_ok) break;
            if (result_count >= capacity || i > UINT8_MAX) return -1;
            indices[result_count++] = (uint8_t)i;
            break;
        }
    }
    *count = result_count;
    return 0;
}

int fd2_field_ai_choose_item_candidate(
        const fd2_field_map *map,
        const fd2_terrain_tileset *terrain,
        const fd2_field_units *units,
        size_t actor_index,
        uint8_t selector_mode,
        fd2_field_ai_item_candidate *candidate) {
    if (!map || !terrain || !units || actor_index >= units->count ||
        units->count > FD2_FIELD_MAX_UNITS || !candidate)
        return -1;
    const fd2_field_unit *actor = &units->items[actor_index];
    const uint8_t *actor_record = (const uint8_t *)actor;
    size_t inventory_count = unit_inventory_count(actor);
    if (inventory_count == 0) return -1;

    int have_best = 0;
    fd2_field_ai_item_candidate best = {0};
    for (size_t slot = 0; slot < inventory_count; slot++) {
        uint8_t item_id = actor_record[0x0bu + slot * 2u];
        const uint8_t *item = fd2_field_item_record_get(item_id);
        if (!item || item[0x0du] == 0u) continue;

        uint8_t outer_raw = item[0x10u];
        fd2_field_path_result positions =
            FD2_FIELD_PATH_RESULT_INITIALIZER;
        if (fd2_field_target_range_compute(
                &positions, map, terrain, actor->x, actor->y,
                0, outer_raw > 15u ? 0u : outer_raw) != 0)
            return -1;
        if (outer_raw > 15u) {
            uint8_t radius = (uint8_t)(outer_raw - 16u);
            for (int py = 0; py < positions.height; py++) {
                for (int px = 0; px < positions.width; px++) {
                    int dx = px - actor->x;
                    int dy = py - actor->y;
                    if (dx < 0) dx = -dx;
                    if (dy < 0) dy = -dy;
                    if ((py == actor->y && dx <= radius) ||
                        (px == actor->x && dy <= radius)) {
                        fd2_field_path_node *node = &positions.nodes[
                            (size_t)py * (size_t)positions.width +
                            (size_t)px];
                        node->distance = 0;
                        node->can_stop = 1;
                        node->can_expand = 0;
                    }
                }
            }
        }
        for (int y = 0; y < positions.height; y++) {
            for (int x = 0; x < positions.width; x++) {
                if (!fd2_field_path_is_destination(&positions, x, y))
                    continue;
                uint8_t target_indices[FD2_FIELD_MAX_UNITS];
                size_t target_count = 0;
                if (outer_raw > 15u) {
                    if (collect_line_targets(
                            units, actor->x, actor->y, x, y,
                            (uint8_t)(outer_raw - 16u), 0,
                            target_indices, sizeof(target_indices),
                            &target_count) != 0) {
                        fd2_field_path_close(&positions);
                        return -1;
                    }
                } else {
                    fd2_field_path_result target_range =
                        FD2_FIELD_PATH_RESULT_INITIALIZER;
                    if (fd2_field_target_range_compute(
                            &target_range, map, terrain, x, y,
                            0, item[0x12u]) != 0) {
                        fd2_field_path_close(&positions);
                        return -1;
                    }
                    uint8_t filter = selector_mode != 0u ? item[0x11u] :
                        item[0x11u] != 0u ? 0u : 1u;
                    if (collect_range_targets(
                            &target_range, units, filter, target_indices,
                            sizeof(target_indices), &target_count) != 0) {
                        fd2_field_path_close(&target_range);
                        fd2_field_path_close(&positions);
                        return -1;
                    }
                    fd2_field_path_close(&target_range);
                }
                if (target_count == 0) continue;

                fd2_field_ai_score_targets targets = {
                    units, target_indices, target_count,
                };
                fd2_field_ai_item_candidate current = {
                    .item_id = item_id,
                    .slot_index = slot,
                    .target_x = x,
                    .target_y = y,
                };
                if (fd2_field_ai_item_targets_score(
                        item_id, &targets, &current.score) != 0) {
                    fd2_field_path_close(&positions);
                    return -1;
                }
                if (fd2_field_ai_item_candidate_is_better(
                        &current, &best, have_best)) {
                    best = current;
                    have_best = 1;
                }
            }
        }
        fd2_field_path_close(&positions);
    }
    if (!have_best) return -1;
    *candidate = best;
    return 0;
}

fd2_field_ai_action fd2_field_ai_select_attack_action(
        int32_t physical_score,
        int32_t magic_score,
        int32_t item_score,
        int tie_prefer_physical) {
    /* field_ai_try_attack @0x3a104：阈值测试是三项独立的 signed
     * score >= 6；随后比较图中的 B==C>A 平价固定由 magic 胜出。 */
    if (physical_score < 6 && magic_score < 6 && item_score < 6)
        return FD2_FIELD_AI_ACTION_NONE;
    if (physical_score > magic_score && physical_score > item_score)
        return FD2_FIELD_AI_ACTION_PHYSICAL;
    if (physical_score == magic_score && physical_score > item_score)
        return tie_prefer_physical ? FD2_FIELD_AI_ACTION_PHYSICAL
                                   : FD2_FIELD_AI_ACTION_MAGIC;
    if (physical_score == item_score && physical_score > magic_score)
        return tie_prefer_physical ? FD2_FIELD_AI_ACTION_PHYSICAL
                                   : FD2_FIELD_AI_ACTION_ITEM;
    if (magic_score > physical_score && magic_score >= item_score)
        return FD2_FIELD_AI_ACTION_MAGIC;
    if (item_score > physical_score && item_score > magic_score)
        return FD2_FIELD_AI_ACTION_ITEM;
    return FD2_FIELD_AI_ACTION_HANDLED_NOOP;
}

fd2_field_ai_action fd2_field_ai_select_attack_action_for_candidates(
        const fd2_field_unit *actor,
        const fd2_field_unit *physical_target,
        const fd2_field_ai_physical_candidate *physical,
        const fd2_field_ai_magic_candidate *magic,
        const fd2_field_ai_item_candidate *item) {
    if (!actor) return FD2_FIELD_AI_ACTION_NONE;
    int32_t physical_score = physical ? physical->priority : INT32_MIN;
    int32_t magic_score = magic ? magic->score : INT32_MIN;
    int32_t item_score = item ? item->score : INT32_MIN;

    /* corrected code0 0x2a160..0x2a173 先做 unsigned u16 的 32 位
     * subtraction。结果在 underflow 时是很大的 uint32；后续与 spell
     * u16 比较使用 signed jl，但正常战斗 stat 范围不会触及符号位。 */
    uint32_t attack_defense_difference = physical_target
        ? (uint32_t)actor->attack - (uint32_t)physical_target->defense
        : 0u;
    int tie_prefer_physical = 0;
    if (physical_score == magic_score && physical_score > item_score &&
        magic) {
        if (magic->magic_id < 11u) {
            const uint8_t *record =
                fd2_field_magic_record_get(magic->magic_id);
            tie_prefer_physical = !record ||
                (int32_t)fd2_field_magic_u16(record, 0) <
                    (int32_t)attack_defense_difference;
        } else {
            tie_prefer_physical =
                (actor->ai_behavior_raw & 0x40u) != 0u;
        }
    } else if (physical_score == item_score &&
               physical_score > magic_score) {
        tie_prefer_physical =
            (actor->ai_behavior_raw & 0x40u) != 0u;
    }
    return fd2_field_ai_select_attack_action(
        physical_score, magic_score, item_score, tie_prefer_physical);
}
