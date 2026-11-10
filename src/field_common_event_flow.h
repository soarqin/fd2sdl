/* G4共用事件有限原语：53ad5、34dd0/35bee、33499/24bde、
 * 2aedb/240fa与3623c。分支/等待/取消及提交顺序只在common_event冷图。
 * 仅由field_game.c和同ABI测试包含；不分配owner、不回滚已提交记录。 */
#ifndef FD2_FIELD_COMMON_EVENT_FLOW_H
#define FD2_FIELD_COMMON_EVENT_FLOW_H

#include "field_event.h"
#include "field_item.h"
#include "static_flow.h"

static int common_event_actor(const fd2_field_game *game, size_t caller,
        int32_t requested, size_t *actor) {
    if (!game || !actor || requested < -1 ||
        game->units.count > FD2_FIELD_MAX_UNITS) return -1;
    size_t selected = requested == -1 ? caller : (size_t)requested;
    if (selected >= game->units.count) return -1;
    *actor = selected;
    return 0;
}

/* 纯白名单：1=已处理query，0=不是query，-1=数据错误。const Game只读，
 * 不混入演出/写点；stage result图与事件执行共用完全相同的比较宽度。 */
static int common_event_query(const fd2_field_game *game, size_t caller,
        const fd2_static_flow_node *node, unsigned *port) {
    if (!game || !node || !node->op || !port || game->units.count > FD2_FIELD_MAX_UNITS) return -1;
    const char *op = node->op;
    const int32_t *a = node->args;
    size_t actor;
    int result;
    if (!strcmp(op, "branch_caller")) {
        if (caller >= game->units.count || a[1] < 0) return -1;
        result = fd2_field_event_compare_unsigned((uint32_t)caller,
            (fd2_field_event_compare)a[0], (uint32_t)a[1]);
    } else if (!strcmp(op, "branch_event_byte")) {
        if (a[0] < 0 || a[2] < 0 || a[2] > UINT8_MAX) return -1;
        result = fd2_field_event_state_compare(game->cell_action_completed,
            (size_t)a[0], (fd2_field_event_compare)a[1], (uint8_t)a[2]);
    } else if (!strcmp(op, "branch_turn")) {
        result = fd2_field_event_compare_turn(game->turn_number,
            (fd2_field_event_compare)a[0], a[1]);
    } else if (!strcmp(op, "branch_actor")) {
        if (common_event_actor(game, caller, a[0], &actor) != 0 || a[3] < 0) return -1;
        result = fd2_field_event_actor_compare(&game->units, actor,
            (fd2_field_event_actor_field)a[1], (fd2_field_event_compare)a[2], (uint32_t)a[3]);
    } else if (!strcmp(op, "branch_hidden_count")) {
        size_t count;
        if (a[0] < 0 || a[1] < 0 || a[3] < 0 ||
            fd2_field_event_hidden_count(&game->units, (size_t)a[0], (size_t)a[1], &count) != 0)
            return -1;
        result = fd2_field_event_compare_unsigned((uint32_t)count,
            (fd2_field_event_compare)a[2], (uint32_t)a[3]);
    } else if (!strcmp(op, "branch_roster_text")) {
        if (a[0] < 0 || a[0] > UINT8_MAX) return -1;
        result = fd2_field_event_roster_has_text(game->roster, game->roster_count, (uint8_t)a[0]);
    } else if (!strcmp(op, "branch_item")) {
        if (common_event_actor(game, caller, a[0], &actor) != 0 || a[1] < 0 || a[1] > UINT8_MAX)
            return -1;
        result = fd2_field_event_item_find(&game->units, actor, (uint8_t)a[1], NULL);
    } else if (!strcmp(op, "branch_item_occurrences")) {
        size_t count;
        if (a[0] < 0 || a[1] < 0 || a[2] < 0 || a[3] > UINT8_MAX || a[5] < 0 ||
            a[3] < a[2] || fd2_field_event_item_occurrences(&game->units,
                (size_t)a[0], (size_t)a[1], (uint8_t)a[2], (uint8_t)a[3], &count) != 0) return -1;
        result = fd2_field_event_compare_unsigned((uint32_t)count,
            (fd2_field_event_compare)a[4], (uint32_t)a[5]);
    } else return 0;
    if (result < 0) return -1;
    *port = result ? FD2_FLOW_YES : FD2_FLOW_NO;
    return 1;
}

/* 1=本域处理，0=由既有场景/loot/resource领域处理，-1=错误。
 * validate_only不写Game/map/roster，也不取RNG；它只核本原语当前有效域，
 * 不能替代caller逐条资源预检或证明所有未来分支。 */
static int common_event_domain(fd2_field_game *game, size_t caller,
        const fd2_static_flow_node *node, unsigned *port, int validate_only) {
    if (!game || !node || !node->op || !port || game->units.count > FD2_FIELD_MAX_UNITS ||
        (validate_only != 0 && validate_only != 1)) return -1;
    const char *op = node->op;
    const int32_t *a = node->args;
    int query = common_event_query(game, caller, node, port);
    if (query != 0) return query;
    size_t actor;
    int result;
    *port = FD2_FLOW_NEXT;
    if (!strcmp(op, "event_byte_write")) {
        if (a[0] < 0 || a[1] < a[0] || a[1] > 31 ||
            a[2] < 0 || a[2] > UINT8_MAX || (a[3] != 0 && a[3] != 1)) return -1;
        if (!validate_only && fd2_field_event_state_write(game->cell_action_completed,
            (size_t)a[0], (size_t)a[1], (uint8_t)a[2], a[3]) != 0) return -1;
        return 1;
    } else if (!strcmp(op, "event_byte_from_count")) {
        if (a[0] < 0 || a[0] > 31 || a[1] < 0 || a[1] > UINT8_MAX ||
            game->units.count > FD2_FIELD_MAX_UNITS || game->units.count < (size_t)a[1]) return -1;
        if (!validate_only) game->cell_action_completed[a[0]] = (uint8_t)(game->units.count - (size_t)a[1]);
        return 1;
    } else if (!strcmp(op, "turn_relative")) {
        if (a[0] < 0 || a[0] > 15 || a[1] < 0 || a[1] > 2) return -1;
        if (!validate_only && fd2_field_event_schedule_turn(&game->metadata, (size_t)a[0],
            game->turn_number, (uint8_t)a[1]) != 0) return -1;
        return 1;
    } else if (!strcmp(op, "event_item_append") || !strcmp(op, "event_item_remove")) {
        if (common_event_actor(game, caller, a[0], &actor) != 0 || a[1] < -1 || a[1] > UINT8_MAX)
            return -1;
        uint8_t item;
        if (a[1] == -1) {
            if (!game->event_resume.active || !game->event_resume.has_item) return -1;
            item = game->event_resume.item_id;
        } else item = (uint8_t)a[1];
        if (!strcmp(op, "event_item_append")) {
            if (validate_only) result = fd2_field_unit_inventory_count(&game->units.items[actor]) < 8u;
            else result = fd2_field_event_item_append(&game->units, actor, item);
        } else {
            if (validate_only) result = fd2_field_event_item_find(&game->units, actor, item, NULL);
            else result = fd2_field_event_item_remove(&game->units, actor, item);
        }
    } else if (!strcmp(op, "cell_item_prepare")) {
        uint8_t item;
        if (!game->event_resume.active || fd2_field_event_cell_item(&game->map,
            game->cursor_cell_x, game->cursor_cell_y, &item) != 0) return -1;
        if (!validate_only) {
            game->event_resume.item_id = item;
            game->event_resume.has_item = 1;
            /* 35854@35916在422文本前写原53AD9；Load/跨Scene沿既有owner保留。 */
            game->message_new_fragment = (uint32_t)item + 181u;
        }
        return 1;
    } else if (!strcmp(op, "map_reset_cells")) {
        if (!game->map.cells || game->map.width <= 0 || game->map.height <= 0 ||
            game->map.width > 256 || game->map.height > 256 ||
            (size_t)game->map.width > SIZE_MAX / (size_t)game->map.height) return -1;
        if (!validate_only && fd2_field_event_map_reset_cells(&game->map) != 0) return -1;
        return 1;
    } else if (!strcmp(op, "unit_hp_zero_tail")) {
        if (a[0] < 0 || game->units.count > FD2_FIELD_MAX_UNITS) return -1;
        if (!validate_only && fd2_field_event_hp_zero_tail(&game->units, (size_t)a[0]) != 0) return -1;
        return 1;
    } else if (!strcmp(op, "event_item_consume")) {
        if (a[0] < 0 || a[1] < a[0] || a[2] < 0 || a[3] < a[2] || a[3] > UINT8_MAX ||
            game->units.count > FD2_FIELD_MAX_UNITS || (size_t)a[1] >= game->units.count) return -1;
        if (!validate_only && fd2_field_event_item_consume(&game->units, (size_t)a[0], (size_t)a[1],
            (uint8_t)a[2], (uint8_t)a[3]) != 0) return -1;
        return 1;
    } else if (!strcmp(op, "unit_field_write") || !strcmp(op, "unit_ai_write")) {
        if (a[0] < 0 || a[1] < a[0] || (size_t)a[1] >= game->units.count ||
            game->units.count > FD2_FIELD_MAX_UNITS) return -1;
        /* G2已有typed writer完整参数校验；借单个临时units用于纯验证，
         * 不复制Game/资源owner，也不承诺对已提交事件作回滚。 */
        fd2_field_units validation;
        fd2_field_units *units = &game->units;
        if (validate_only) { validation = game->units; units = &validation; }
        if (!strcmp(op, "unit_ai_write")) {
            if (a[2] < 0 || a[2] > UINT8_MAX) return -1;
            result = fd2_field_units_ai_behavior_write(units, (size_t)a[0], (size_t)a[1], (uint8_t)a[2]);
        } else {
            if (a[4] < 0 || a[4] > UINT16_MAX) return -1;
            result = fd2_field_units_field_write(units, (size_t)a[0], (size_t)a[1],
                (fd2_field_unit_field)a[2], (fd2_field_unit_write)a[3], (uint16_t)a[4]);
        }
        return result == 0 ? 1 : -1;
    } else if (!strcmp(op, "mark_random_pair")) {
        if (a[0] < 0 || a[0] > 31) return -1;
        size_t base = game->cell_action_completed[a[0]];
        if (!game->attack_rng || game->units.count > FD2_FIELD_MAX_UNITS ||
            base > FD2_FIELD_MAX_UNITS - 3u || base + 2u >= game->units.count) return -1;
        if (!validate_only && fd2_field_event_mark_random_pair(&game->units, base,
            game->attack_rng, game->attack_rng_userdata) != 0) return -1;
        return 1;
    } else return 0;
    if (result < 0) return -1;
    *port = result ? FD2_FLOW_YES : FD2_FLOW_NO;
    return 1;
}

#endif
