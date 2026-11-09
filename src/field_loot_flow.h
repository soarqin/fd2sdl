/* 掉落与特殊格：field_loot_descriptors_execute @0x1aa1d、
 * field_special_cell_execute @0x190ac（原sub_1AA1D/sub_190AC）。
 * 分支、等待和顺序来自loot冷图；本文件仅提供原有领域读写和边界检查。
 * 仅由field_game.c包含，不另建session或序列化执行状态。
 */
#include "static_data.h"

static int complete_cell_tiles(fd2_field_game *game);

typedef struct {
    fd2_field_game *game;
    const fd2_static_flow *flow;
    fd2_field_loot pending, saved;
    size_t actor, scan;
    int magic_collector, raw_slot, last, started, preflight, override;
    uint8_t descriptor[3], cell_index, cell_flags, remembered;
    uint32_t message_number, normalized_reward;
} fd2_loot_flow_context;

static const fd2_static_flow *loot_flow(void) {
    return fd2_static_flow_lookup("loot", "loot");
}

static const uint8_t *loot_flow_descriptor(const fd2_loot_flow_context *ctx) {
    const fd2_field_loot *loot = &ctx->game->loot;
    if (ctx->override) return ctx->descriptor;
    return loot->cursor < loot->count ? loot->descriptors[loot->cursor] : ctx->descriptor;
}

static int loot_resource_ready(fd2_field_game *game, const fd2_field_loot_notice *notice) {
    if (!game->loot_preflight || !game->loot_preflight(game, notice, game->loot_preflight_userdata)) return 1;
    fail_action(game, notice->actor_index);
    return 0;
}

int fd2_field_game_set_loot_preflight(fd2_field_game *game,
        fd2_field_loot_preflight_fn preflight, void *userdata) {
    if (!game || !game->ready || progress_pending(game)) return -1;
    game->loot_preflight = preflight; game->loot_preflight_userdata = userdata;
    return 0;
}

static int inventory_visible_is_known(const fd2_field_unit *unit) {
    const uint8_t *raw = (const uint8_t *)unit;
    for (size_t i = 0; i < 8u; i++)
        if (!(raw[0x0au + i * 2u] & 0x80u) && !fd2_field_item_record_get(raw[0x0bu + i * 2u])) return 0;
    return 1;
}

static fd2_field_loot_notice loot_flow_notice(const fd2_loot_flow_context *ctx,
        const fd2_static_flow_node *node) {
    const uint8_t *d = loot_flow_descriptor(ctx);
    uint16_t value = (uint16_t)(d[1] | (uint16_t)d[2] << 8);
    unsigned flags = (unsigned)node->args[2];
    fd2_field_loot_notice n = {0};
    n.actor_index = ctx->actor; n.kind = (fd2_field_loot_kind)node->args[0];
    n.fragment = (uint16_t)(node->args[4] ? value : node->args[1]);
    n.open_box = (uint8_t)(flags & 1u); n.append_line = (uint8_t)((flags >> 1) & 1u);
    n.close_box = (uint8_t)((flags >> 2) & 1u); n.wait_key = (uint8_t)((flags >> 3) & 1u);
    n.wait_ms = (uint16_t)node->args[3]; n.item_id = d[1]; n.old_item_id = ctx->remembered;
    n.value = node->args[5] ? (ctx->preflight ? ctx->message_number : ctx->game->message_number) : value;
    return n;
}

static int loot_flow_query(fd2_loot_flow_context *ctx, int field, int *out) {
    fd2_field_game *g = ctx->game; const fd2_field_loot *l = &g->loot;
    const uint8_t *d = loot_flow_descriptor(ctx);
    const fd2_field_unit *u = ctx->actor < g->units.count ? &g->units.items[ctx->actor] : NULL;
    const fd2_field_unit *scan = ctx->scan < g->units.count ? &g->units.items[ctx->scan] : NULL;
    switch (field) {
    case 0: *out = l->cursor < l->count; break;
    case 1: *out = d[0]; break;
    case 2: if (!u) return -1; *out = u->side; break;
    case 3: { if (!u) return -1; fd2_field_unit shadow = *u;
        *out = inventory_first_empty_add(&shadow, d[1]) == 1; break; }
    case 4: *out = (ctx->cell_flags & 0x20u) != 0; break;
    case 5: *out = d[1] | (int)d[2] << 8; break;
    case 6: *out = l->source; break;
    case 7: *out = ctx->saved.action_kind; break;
    case 8: *out = ctx->saved.resume; break;
    case 9: *out = ctx->saved.ai_common_tail; break;
    case 10: *out = ctx->saved.ai_after_magic; break;
    case 11: *out = ctx->last; break;
    case 12: *out = g->battle_result; break;
    case 13: *out = ctx->scan < g->units.count; break;
    case 14: if (!scan) return -1; *out = fd2_field_unit_is_hidden(scan); break;
    case 15: if (!scan) return -1; *out = scan->hp; break;
    case 16: if (!scan) return -1; *out = ((const uint8_t *)scan)[0x31]; break;
    case 17: *out = ctx->magic_collector; break;
    case 18: *out = (int)ctx->pending.count; break;
    case 19: *out = ctx->cell_flags != 0; break;
    case 20: *out = g->cell_action_completed[ctx->cell_index]; break;
    case 21: *out = (int)g->stage; break;
    case 22: *out = ctx->cell_index; break;
    case 23: *out = u && ctx->raw_slot >= 0 && ctx->raw_slot < 8 &&
        !(((const uint8_t *)u)[0x0a + ctx->raw_slot * 2] & 0x80u); break;
    case 24: *out = (int)g->units.count; break;
    case 25: *out = (int)g->event_log_count; break;
    case 26: if (!u) return -1; *out = unit_can_act_for_side(u, g->active_side); break;
    case 27: if (!u) return -1; *out = u->side == g->active_side; break;
    case 28: if (!u) return -1; *out = u->hp; break;
    case 29: if (!u) return -1; *out = fd2_field_unit_has_acted(u); break;
    case 30: *out = (int)g->move_path_length; break;
    default: return -1;
    }
    return 0;
}

static int loot_flow_run(fd2_loot_flow_context *ctx, int pc);

/* 这是领域ABI名称登记，不是游戏规则或可执行字节码。 */
static int loot_flow_operation(const char *op, int branch) {
    static const char *const actions[] = {
        "collect_reset", "descriptor_preflight", "collect_append", "collect_next",
        "finalize_defeated", "battle_result_refresh", "collect_commit", "inventory_add_checked",
        "inventory_assert_full", "inventory_remove", "inventory_add", "message_value_set",
        "resource_add_message", "descriptor_next", "event_dispatch", "cell_init",
        "cell_mark_complete", "inventory_remember", "cell_store_remembered", "cell_tiles_apply",
        "loot_snapshot_clear", "reward_begin", "reward_common_set", "experience_clear",
        "ai_remainder_restore", "player_finish", "ai_finish", "pending_dispatch",
        "player_controller_resume", "item_validate", "cell_tiles_validate", "return_result",
        "wait_cell_preflight", "stationary_recover", "wait_cell_begin", "directions_reset",
        "actor_mark_acted", "magic_reward_normalize", "experience_store_normalized", "magic_list_reset",
        "item_selection_reset", "targets_reset", "command_release", "player_controller_complete",
        "pending_collect_final"
    };
    static const char *const queries[] = {
        "branch_has_descriptor", "branch_descriptor_mode", "branch_actor_side", "branch_inventory_capacity",
        "branch_cell_chest", "branch_descriptor_value", "branch_source", "branch_action_kind",
        "branch_resume", "branch_ai_common", "branch_ai_after_magic", "branch_last_result",
        "branch_battle_result", "branch_collect_more", "branch_collect_hidden", "branch_collect_hp",
        "branch_collect_mode", "branch_magic_collector", "branch_collect_count", "branch_cell_flags",
        "branch_cell_completed", "branch_stage", "branch_cell_index", "branch_slot_valid",
        "branch_unit_count", "branch_notice_count", "branch_actor_can_act", "branch_actor_active_side",
        "branch_actor_hp", "branch_actor_acted", "branch_path_length"
    };
    const char *const *names = branch ? queries : actions;
    size_t count = branch ? sizeof(queries) / sizeof(queries[0]) : sizeof(actions) / sizeof(actions[0]);
    for (size_t i = 0; i < count; i++) if (!strcmp(op, names[i])) return (int)i;
    return -1;
}

static int loot_descriptor_preflight(fd2_field_game *game, size_t recipient,
        const uint8_t descriptor[3]) {
    if (!game || !descriptor || recipient >= game->units.count) return -1;
    fd2_loot_flow_context ctx = {0}; ctx.game = game; ctx.actor = recipient;
    ctx.preflight = ctx.override = 1; memcpy(ctx.descriptor, descriptor, 3);
    ctx.message_number = game->message_number;
    /* G2：1aa1d 的4..255是逐项default，不触发物品/文本/回调预检；
     * defeated collector的255哨兵仍由collect.has_descriptor冷分支处理。
     * 原BIOS清键由宿主逐帧输入消费边界承担，不额外等键或耗RNG。 */
    return loot_flow_run(&ctx, fd2_static_flow_label(loot_flow(), "preflight.drop"));
}

/* 12263全部可能写点的资源安全门：只读枚举当前图格，不改变完成态。 */
static int loot_flow_cell_tiles_check(fd2_loot_flow_context *ctx) {
    fd2_field_game *g = ctx->game;
    for (int y = 0; y < g->map.height; y++) for (int x = 0; x < g->map.width; x++) {
        uint32_t c = fd2_field_map_cell(&g->map, x, y);
        uint16_t id = fd2_field_cell_terrain(c);
        if (id >= g->terrain.attr_count) return -1;
        if ((g->terrain.attrs[id].flags & 0x60u) == 0x20u &&
            (g->cell_action_completed[(c >> 16) & 31u] || ((c >> 16) & 31u) == ctx->cell_index)) {
            uint32_t changed = (c & UINT32_C(0xff000000)) | (uint16_t)((uint16_t)c + 1u);
            size_t frame;
            if (fd2_terrain_base_frame_from_cell(&g->terrain, changed, &frame) != 0) return -1;
        }
    }
    return 0;
}

static int execute_loot_callback(fd2_field_game *game) {
    const fd2_field_loot *l = &game->loot;
    if (l->count > FD2_FIELD_MAX_UNITS || l->cursor >= l->count ||
        l->actor_index >= game->units.count) return -1;
    fd2_field_event_notice e = {.kind = FD2_FIELD_EVENT_CELL, .turn_number = game->turn_number,
        .phase = game->active_side, .action = l->descriptors[l->cursor][1],
        .unit_index = (uint8_t)l->actor_index, .slot = SIZE_MAX};
    if (reserve_event_notice(game, &e) != 0) return -1;
    snapshot_event_presentation(game, &e);
    if (execute_known_turn_event(game, e.action) != 0) return -1;
    e.handled = e.presentation_deferred = 1;
    append_event_notice(game, &e);
    return 0;
}

static int loot_flow_action(fd2_loot_flow_context *ctx, int action) {
    fd2_field_game *g = ctx->game; fd2_field_loot *l = &g->loot;
    const uint8_t *d = loot_flow_descriptor(ctx);
    uint16_t value = (uint16_t)(d[1] | (uint16_t)d[2] << 8);
    fd2_field_unit *u = ctx->actor < g->units.count ? &g->units.items[ctx->actor] : NULL;
    switch (action) {
    case 0: ctx->scan = ctx->pending.count = 0; break;
    case 1:
        if (ctx->scan >= g->units.count) return -1;
        return loot_descriptor_preflight(g, ctx->actor, (const uint8_t *)&g->units.items[ctx->scan] + 0x31);
    case 2:
        if (ctx->scan >= g->units.count || ctx->pending.count >= FD2_FIELD_MAX_UNITS) return -1;
        memcpy(ctx->pending.descriptors[ctx->pending.count++], (const uint8_t *)&g->units.items[ctx->scan] + 0x31, 3); break;
    case 3: ctx->scan++; break;
    case 4: finalize_zero_hp_units(g); break;
    case 5: g->battle_result = fd2_field_game_battle_result(g); break;
    case 6: *l = ctx->pending; ctx->started = 1; break;
    case 7: { if (!u || value > UINT8_MAX || !fd2_field_item_record_get(d[1])) return -1;
        fd2_field_unit shadow = *u;
        if (inventory_first_empty_add(&shadow, d[1]) != 1) return -1;
        *u = shadow; break; }
    case 8: { if (!u) return -1; fd2_field_unit shadow = *u;
        if (inventory_first_empty_add(&shadow, d[1]) == 1) return -1;
        break; }
    case 9:
        if (!u || ctx->raw_slot < 0 || ctx->raw_slot >= 8 ||
            (((const uint8_t *)u)[0x0a + ctx->raw_slot * 2] & 0x80u)) return -1;
        inventory_slot_remove(u, (size_t)ctx->raw_slot); break;
    case 10:
        if (!u || value > UINT8_MAX || !fd2_field_item_record_get(d[1]) ||
            inventory_first_empty_add(u, d[1]) != 1) return -1;
        break;
    case 11:
        if (ctx->preflight) ctx->message_number = value; else g->message_number = value; break;
    case 12: g->resource_value += g->message_number; break;
    case 13: l->cursor++; break;
    case 14: return execute_loot_callback(g);
    case 15:
        if (ctx->cell_index >= FD2_FIELD_EVENT_SLOTS) return -1;
        memset(l, 0, sizeof(*l)); l->source = LOOT_SOURCE_CELL;
        l->actor_index = l->action_actor = ctx->actor; l->action_kind = PLAYER_ACTION_WAIT_COMPLETE;
        l->cell_index = ctx->cell_index; l->cell_flags = ctx->cell_flags; l->count = 1;
        memcpy(l->descriptors[0], ctx->descriptor, 3); ctx->override = 0; ctx->started = 1; break;
    case 16:
        if (l->cell_index >= FD2_FIELD_EVENT_SLOTS) return -1;
        g->cell_action_completed[l->cell_index] = 1; break;
    case 17:
        if (!u || ctx->raw_slot < 0 || ctx->raw_slot >= 8 ||
            (((const uint8_t *)u)[0x0a + ctx->raw_slot * 2] & 0x80u)) return -1;
        ctx->remembered = ((uint8_t *)u)[0x0b + ctx->raw_slot * 2]; break;
    case 18:
        if (l->cell_index >= FD2_FIELD_EVENT_SLOTS) return -1;
        g->metadata.cell_actions[l->cell_index].param = ctx->remembered; break;
    case 19: return complete_cell_tiles(g);
    case 20: ctx->saved = *l; memset(l, 0, sizeof(*l)); break;
    case 21:
        ctx->last = begin_reward(g, ctx->saved.actor_index, ctx->saved.resume, ctx->saved.action_actor);
        return ctx->last < 0 ? -1 : 0;
    case 22: g->progress.ai_common_tail = ctx->saved.ai_common_tail; break;
    case 23: g->experience_value = 0; break;
    case 24:
        ctx->saved.notice.kind = FD2_FIELD_LOOT_NONE; ctx->saved.source = LOOT_SOURCE_AI_REMAINDER;
        *l = ctx->saved; break;
    case 25:
        ctx->last = finish_unit_action_internal(g, (player_action_kind)ctx->saved.action_kind, ctx->saved.raw_reward);
        return ctx->last < 0 ? -1 : 0;
    case 26: return finish_ai_action(g, ctx->saved.action_actor);
    case 27: return dispatch_pending_cell_event(g, ctx->saved.action_actor);
    case 28: return fd2_field_game_resume_player_controller(g) < 0 ? -1 : 0;
    case 29:
        if (!u || value > UINT8_MAX || !fd2_field_item_record_get((uint8_t)value) || !inventory_visible_is_known(u)) return -1;
        break;
    case 30: return loot_flow_cell_tiles_check(ctx);
    case 32: return loot_flow_preflight_cell(g, ctx->actor);
    case 33: return recover_stationary_unit(g, ctx->actor) < 0 ? -1 : 0;
    case 34: ctx->last = begin_wait_cell(g, ctx->actor); return ctx->last < 0 ? -1 : 0;
    case 35:
        for (size_t i = 0; i < g->units.count; i++) g->units.items[i].direction = 0;
        break;
    case 36: if (!u) return -1; fd2_field_unit_set_acted(u, 1); break;
    case 37:
        if (!u) return -1;
        return fd2_field_magic_reward_normalize(u, ctx->saved.raw_reward, &ctx->normalized_reward) != 0 ? -1 : 0;
    case 38: g->experience_value = ctx->normalized_reward; break;
    case 39: g->magic_list_selected = 0; break;
    case 40: g->item_list_selected = 0; g->item_give_recipient = g->item_give_selected = 0; break;
    case 41:
        g->magic_target_id = 0; memset(g->magic_target_candidates, 0, sizeof(g->magic_target_candidates));
        g->magic_target_count = g->magic_target_cycle = 0; break;
    case 42:
        clear_move_query(g); g->selected_unit = g->command_selected = -1;
        g->interaction = FD2_FIELD_INTERACTION_BROWSE; g->ordinary_display_mode = 1; break;
    case 43:
        ctx->last = fd2_field_game_complete_player_controller(g, ctx->actor);
        return ctx->last < 0 ? -1 : 0;
    case 44: return collect_pending_cell_event(g, ctx->actor, 1) < 0 ? -1 : 0;
    default: return -1;
    }
    return 0;
}

static int loot_flow_run(fd2_loot_flow_context *ctx, int pc) {
    if (!ctx || !ctx->game) return -1;
    if (ctx->game->units.count > FD2_FIELD_MAX_UNITS ||
        ctx->game->loot.count > FD2_FIELD_MAX_UNITS ||
        ctx->game->loot.cursor > ctx->game->loot.count ||
        ctx->cell_index >= FD2_SAVE_BATTLE_CELL_STATE_SIZE) goto failure;
    const fd2_static_flow *flow = ctx->flow ? ctx->flow : loot_flow();
    for (unsigned budget = 0; budget < FD2_FLOW_STEP_BUDGET; budget++) {
        const fd2_static_flow_node *node = fd2_static_flow_at(flow, pc);
        if (!node || !(node->views & FD2_FLOW_LOGIC)) break;
        unsigned port = FD2_FLOW_NEXT;
        int query = loot_flow_operation(node->op, 1);
        int action = loot_flow_operation(node->op, 0);
        /* 冷图可改边，但纯预检不能因此获得写live owner的能力。 */
        if (ctx->preflight && !(query >= 0 || action == 11 || action == 29 || action == 30 ||
            !strcmp(node->op, "probe") || !strcmp(node->op, "probe_slots") ||
            !strcmp(node->op, "end") || !strcmp(node->op, "fail"))) goto failure;
        if (query >= 0) {
            int value;
            if (loot_flow_query(ctx, query, &value) != 0) port = FD2_FLOW_ERROR;
            else {
                int match;
                switch (node->args[0]) {
                case 0: match = value == node->args[1]; break;
                case 1: match = value != node->args[1]; break;
                case 2: match = value < node->args[1]; break;
                case 3: match = value >= node->args[1]; break;
                default: goto failure;
                }
                port = match ? FD2_FLOW_YES : FD2_FLOW_NO;
            }
        } else if (action >= 0) {
            if (action == 31) return ctx->last;
            if (loot_flow_action(ctx, action) < 0) port = FD2_FLOW_ERROR;
        } else if (!strcmp(node->op, "emit")) {
            /* action.finish是同步API；只有原loot图可以持有等待PC。 */
            if (ctx->flow) goto failure;
            ctx->game->loot.notice = loot_flow_notice(ctx, node);
            ctx->game->loot.step = (uint8_t)pc; ctx->game->loot.text_ready = 0;
            return 0;
        } else if (!strcmp(node->op, "ready")) {
            if (ctx->flow) goto failure;
            ctx->game->loot.step = (uint8_t)pc; ctx->game->loot.text_ready = 1;
            return 0;
        } else if (!strcmp(node->op, "probe") || !strcmp(node->op, "probe_slots")) {
            const fd2_static_flow_node *target = fd2_static_flow_at(flow, fd2_static_flow_edge(flow, node, FD2_FLOW_NEXT));
            if (!target || strcmp(target->op, "emit")) goto failure;
            fd2_field_loot_notice notice = loot_flow_notice(ctx, target);
            if (!strcmp(node->op, "probe_slots")) {
                if (ctx->actor >= ctx->game->units.count) goto failure;
                const uint8_t *raw = (const uint8_t *)&ctx->game->units.items[ctx->actor];
                for (size_t i = 0; i < 8; i++) if (!(raw[0x0a + i * 2] & 0x80u)) {
                    notice.old_item_id = raw[0x0b + i * 2];
                    if (!loot_resource_ready(ctx->game, &notice)) goto failure;
                }
            } else if (!loot_resource_ready(ctx->game, &notice)) goto failure;
            port = FD2_FLOW_YES;
        } else if (!strcmp(node->op, "end")) return node->args[0];
        else if (!strcmp(node->op, "fail")) goto failure;
        else goto failure;
        pc = fd2_static_flow_edge(flow, node, port);
    }
failure:
    fail_action(ctx->game, ctx->actor);
    return -1;
}

static int loot_flow_cell_context(fd2_loot_flow_context *ctx, fd2_field_game *g, size_t actor) {
    if (!g || actor >= g->units.count || !g->map.cells || !g->terrain.attrs) return -1;
    const fd2_field_unit *u = &g->units.items[actor];
    if (u->x >= g->map.width || u->y >= g->map.height) return -1;
    uint32_t cell = fd2_field_map_cell(&g->map, u->x, u->y);
    const fd2_terrain_attr *attr = fd2_terrain_attr_get(&g->terrain, fd2_field_cell_terrain(cell));
    if (!attr) return -1;
    memset(ctx, 0, sizeof(*ctx)); ctx->game = g; ctx->actor = actor;
    ctx->override = 1; ctx->cell_index = (uint8_t)((cell >> 16) & 31u); ctx->cell_flags = attr->flags & 0x60u;
    ctx->message_number = g->message_number;
    if (ctx->cell_index < FD2_FIELD_EVENT_SLOTS) {
        const fd2_field_cell_action *a = &g->metadata.cell_actions[ctx->cell_index];
        ctx->descriptor[0] = a->mode; ctx->descriptor[1] = (uint8_t)a->param; ctx->descriptor[2] = (uint8_t)(a->param >> 8);
    }
    return 0;
}

static int loot_flow_preflight_cell(fd2_field_game *g, size_t actor) {
    fd2_loot_flow_context ctx;
    if (loot_flow_cell_context(&ctx, g, actor) != 0) {
        (void)collect_pending_cell_event(g, actor, 1); return -1;
    }
    ctx.preflight = 1;
    return loot_flow_run(&ctx, fd2_static_flow_label(loot_flow(), "preflight.cell"));
}

static int begin_wait_cell(fd2_field_game *game, size_t actor) {
    fd2_loot_flow_context ctx;
    if (loot_flow_cell_context(&ctx, game, actor) != 0) return -1;
    int result = loot_flow_run(&ctx, fd2_static_flow_label(loot_flow(), "cell.begin"));
    return result < 0 ? -1 : ctx.started;
}

static int begin_defeated_loot(fd2_field_game *game, size_t recipient,
        size_t action_actor, player_action_kind kind, uint64_t raw_reward, int magic_collector) {
    if (!game || game->loot.notice.kind != FD2_FIELD_LOOT_NONE || recipient >= game->units.count) return -1;
    fd2_loot_flow_context ctx = {0}; ctx.game = game; ctx.actor = recipient; ctx.magic_collector = magic_collector;
    ctx.pending.actor_index = recipient; ctx.pending.action_actor = action_actor;
    ctx.pending.action_kind = (uint8_t)kind; ctx.pending.raw_reward = raw_reward; ctx.pending.source = LOOT_SOURCE_DEFEAT;
    int result = loot_flow_run(&ctx, fd2_static_flow_label(loot_flow(), "drop.begin"));
    return result < 0 ? -1 : ctx.started;
}

int fd2_field_game_loot_peek(const fd2_field_game *game, fd2_field_loot_notice *notice) {
    if (!game || !game->ready || !notice || game->failure != FD2_FIELD_FAILURE_NONE) return -1;
    if (game->loot.notice.kind == FD2_FIELD_LOOT_NONE) return 0;
    if (game->loot.notice.kind > FD2_FIELD_LOOT_FIELD_EVENT || game->loot.actor_index >= game->units.count) return -1;
    *notice = game->loot.notice; return 1;
}

static fd2_loot_flow_context loot_flow_reply_context(fd2_field_game *game, int slot) {
    fd2_loot_flow_context ctx = {0}; ctx.game = game; ctx.actor = game->loot.actor_index;
    ctx.cell_index = game->loot.cell_index; ctx.cell_flags = game->loot.cell_flags;
    ctx.remembered = game->loot.notice.old_item_id; ctx.raw_slot = slot;
    return ctx;
}

static int loot_flow_finish_unit_action(fd2_field_game *game, player_action_kind kind,
        uint64_t raw_reward) {
    fd2_loot_flow_context ctx = {0}; ctx.game = game; ctx.actor = (size_t)game->selected_unit;
    ctx.saved.action_kind = (uint8_t)kind; ctx.saved.raw_reward = raw_reward;
    ctx.flow = fd2_static_flow_lookup("loot", "action.finish");
    if (!ctx.flow) { fail_action(game, ctx.actor); return -1; }
    return loot_flow_run(&ctx, ctx.flow->entry);
}

/* VGA续行已经完成物理段且清除原loot owner后，由scheduler调用同一尾图。 */
static int loot_flow_resume_ai_tail(fd2_field_game *game, const fd2_field_loot *saved) {
    if (!game || !saved) return -1;
    fd2_loot_flow_context ctx = {0}; ctx.game = game; ctx.actor = saved->actor_index;
    ctx.saved = *saved;
    return loot_flow_run(&ctx, fd2_static_flow_label(loot_flow(), "finish.common"));
}

int fd2_field_game_loot_text_ready(fd2_field_game *game) {
    fd2_field_loot_notice n;
    if (fd2_field_game_loot_peek(game, &n) != 1 || n.kind != FD2_FIELD_LOOT_MESSAGE || game->loot.text_ready) return -1;
    const fd2_static_flow *flow = loot_flow();
    const fd2_static_flow_node *node = fd2_static_flow_at(flow, game->loot.step);
    int pc = fd2_static_flow_edge(flow, node, FD2_FLOW_TEXT_READY);
    if (pc < 0) return -1;
    fd2_loot_flow_context ctx = loot_flow_reply_context(game, -1);
    return loot_flow_run(&ctx, pc);
}

int fd2_field_game_loot_reply(fd2_field_game *game, fd2_field_loot_reply reply, int raw_slot) {
    fd2_field_loot_notice n;
    if (fd2_field_game_loot_peek(game, &n) != 1 || (n.kind == FD2_FIELD_LOOT_MESSAGE && !game->loot.text_ready)) return -1;
    int choice = n.kind == FD2_FIELD_LOOT_CHOICE;
    int inventory = n.kind == FD2_FIELD_LOOT_INVENTORY;
    if ((choice && reply != FD2_FIELD_LOOT_YES && reply != FD2_FIELD_LOOT_NO && reply != FD2_FIELD_LOOT_CANCEL) ||
        (inventory && reply != FD2_FIELD_LOOT_SLOT && reply != FD2_FIELD_LOOT_CANCEL) ||
        (!choice && !inventory && reply != FD2_FIELD_LOOT_ACK)) return -1;
    static const uint8_t ports[] = {FD2_FLOW_ACK, FD2_FLOW_YES, FD2_FLOW_NO, FD2_FLOW_CANCEL, FD2_FLOW_SLOT};
    if ((unsigned)reply >= sizeof(ports)) return -1;
    const fd2_static_flow *flow = loot_flow();
    const fd2_static_flow_node *node = fd2_static_flow_at(flow, game->loot.step);
    int pc = fd2_static_flow_edge(flow, node, ports[reply]);
    if (pc < 0) return -1;
    fd2_loot_flow_context ctx = loot_flow_reply_context(game, raw_slot);
    return loot_flow_run(&ctx, pc);
}
