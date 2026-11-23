/* 奖励单步领域操作：field_unit_experience_reward_execute @0x1e292、
 * field_growth_stat_present_apply @0x1e529（原登记同名）的写点。
 * 控制边与提示在progress.json；这里不持有Game、资源或持久状态。 */
#ifndef FD2_FIELD_REWARD_FLOW_H
#define FD2_FIELD_REWARD_FLOW_H
#include <limits.h>
#include "field_unit_base.h"
#include "settings.h"
#include "static_flow.h"
#include "static_data.h"

typedef struct {
    uint32_t total;
    fd2_field_unit_level_growth level;
    uint8_t growth_cursor, learn_cursor, step;
} fd2_reward_cursor;
typedef struct {
    uint8_t kind, magic_id, wait_ticks, set_message;
    uint16_t fragment;
    uint32_t value;
} fd2_reward_notice;

/* 原奖励贡献/临时total为signed DWORD，接口保存原bits；避免依赖uint32→int32
 * 越界转换的实现定义行为，也不把负贡献当巨额正奖励。 */
static inline int32_t fd2_reward_signed(uint32_t bits) {
    return bits <= INT32_MAX ? (int32_t)bits :
        (int32_t)(-1 - (int64_t)(UINT32_MAX - bits));
}

static inline const fd2_static_flow *fd2_reward_graph(void) {
    return fd2_static_flow_lookup("progress", "reward");
}

static inline int fd2_reward_ack(const fd2_static_flow *flow, fd2_reward_cursor *cursor) {
    const fd2_static_flow_node *node = fd2_static_flow_at(flow, cursor->step);
    if (!node || strcmp(node->op, "reward_notice")) return -1;
    int next = fd2_static_flow_edge(flow, node, FD2_FLOW_ACK);
    if (next < 0) return -1;
    cursor->step = (uint8_t)next;
    return 0;
}

/* 0=提示等待ACK，1=结束，2=入口早退，-1=错误；错误也保留已到达的写点。 */
static inline int fd2_reward_advance(const fd2_static_flow *flow, fd2_field_unit *unit, uint32_t gain,
        fd2_reward_cursor *cursor, fd2_reward_notice *notice,
        fd2_field_item_stat_lookup_fn lookup, void *lookup_data,
        fd2_field_unit_stat_scale_fn scale, void *scale_data,
        fd2_field_combat_rng_fn growth_rng, void *rng_data) {
    for (unsigned budget = 0; budget < FD2_FLOW_STEP_BUDGET; budget++) {
        const fd2_static_flow_node *n = fd2_static_flow_at(flow, cursor->step);
        if (!n || !(n->views & FD2_FLOW_LOGIC)) return -1;
        unsigned port = FD2_FLOW_NEXT;
        const char *op = n->op;
        if (!strcmp(op, "reward_gate")) {
            uint8_t limit = unit->unit_id == n->args[1] || unit->unit_id == n->args[2]
                ? (uint8_t)n->args[3] : (uint8_t)n->args[0];
            port = gain && !(unit->flags & FD2_FIELD_UNIT_FLAG_HIDDEN) && unit->level != limit
                ? FD2_FLOW_YES : FD2_FLOW_NO;
        } else if (!strcmp(op, "reward_validate")) {
            uint32_t total=(uint32_t)unit->experience + gain;
            /* 不升级时原函数不读growth/learning；负值仅经提示与EX byte写回。 */
            if (fd2_reward_signed(total) >= 100 &&
                !fd2_field_unit_growth_record_get(unit->unit_id)) return -1;
        } else if (!strcmp(op, "reward_total_init")) cursor->total = (uint32_t)unit->experience + gain;
        else if (!strcmp(op, "reward_total_at_least"))
            port = fd2_reward_signed(cursor->total) >= n->args[0] ? FD2_FLOW_YES : FD2_FLOW_NO;
        else if (!strcmp(op, "reward_level_prepare")) {
            if (fd2_field_unit_level_growth_prepare(unit, &cursor->level) != 0) return -1;
            unit->level = cursor->level.level;
        } else if (!strcmp(op, "reward_cursors_reset")) cursor->growth_cursor = cursor->learn_cursor = 0;
        else if (!strcmp(op, "reward_growth_available"))
            port = cursor->growth_cursor < n->args[0] ? FD2_FLOW_YES : FD2_FLOW_NO;
        else if (!strcmp(op, "reward_growth_nonzero")) {
            if (cursor->growth_cursor >= 5u) return -1;
            const uint8_t *record = fd2_field_unit_growth_record_get(unit->unit_id);
            if (!record) return -1;
            cursor->level.growth_delta = fd2_field_growth_delta(record + 2u * cursor->growth_cursor,
                fd2_get_settings()->level_up_growth, growth_rng, rng_data);
            port = cursor->level.growth_delta ? FD2_FLOW_YES : FD2_FLOW_NO;
        } else if (!strcmp(op, "reward_growth_add")) {
            static const uint8_t offsets[] = {0x37, 0x39, 0x3e, 0x42, 0x46};
            if (cursor->growth_cursor >= 5u) return -1;
            uint8_t *raw = (uint8_t *)unit;
            size_t at = offsets[cursor->growth_cursor];
            uint16_t value = (uint16_t)(raw[at] | (uint16_t)raw[at + 1u] << 8);
            value = (uint16_t)(value + cursor->level.growth_delta);
            raw[at] = (uint8_t)value; raw[at + 1u] = (uint8_t)(value >> 8);
        } else if (!strcmp(op, "reward_growth_next")) cursor->growth_cursor++;
        else if (!strcmp(op, "reward_learn_available"))
            port = cursor->learn_cursor < cursor->level.learned_count ? FD2_FLOW_YES : FD2_FLOW_NO;
        else if (!strcmp(op, "reward_learn_apply")) {
            if (cursor->learn_cursor >= cursor->level.learned_count) return -1;
            uint8_t id = cursor->level.learned_magic[cursor->learn_cursor];
            if (id >= 36u) return -1;
            ((uint8_t *)unit)[0x1au + id / 8u] |= (uint8_t)(1u << (id % 8u));
        } else if (!strcmp(op, "reward_learn_next")) cursor->learn_cursor++;
        else if (!strcmp(op, "reward_recompute")) {
            if (fd2_field_unit_combat_stats_recompute(unit, lookup, lookup_data, scale, scale_data) != 0) return -1;
        } else if (!strcmp(op, "reward_total_subtract")) {
            if (fd2_reward_signed(cursor->total) < n->args[0]) return -1;
            cursor->total -= (uint32_t)n->args[0];
        } else if (!strcmp(op, "reward_stop_level")) {
            /* 1e4f6：补丁替换共享的 cmp eax,1Eh 立即数，保留特殊ID的99级门。 */
            const int stop_level = fd2_get_settings()->fix_level_30_experience ? n->args[4] : n->args[0];
            port = unit->level == stop_level ||
                ((unit->unit_id == n->args[1] || unit->unit_id == n->args[2]) && unit->level == n->args[3])
                ? FD2_FLOW_YES : FD2_FLOW_NO;
        } else if (!strcmp(op, "reward_total_zero")) cursor->total = 0;
        else if (!strcmp(op, "reward_experience_commit")) unit->experience = (uint8_t)cursor->total;
        else if (!strcmp(op, "reward_finish")) return 1;
        else if (!strcmp(op, "reward_early_finish")) return 2;
        else if (!strcmp(op, "reward_notice")) {
            *notice = (fd2_reward_notice){0};
            notice->kind = (uint8_t)n->args[0]; notice->fragment = (uint16_t)n->args[1];
            notice->wait_ticks = (uint8_t)n->args[3]; notice->set_message = (uint8_t)n->args[4];
            switch (n->args[2]) {
            case 0: notice->value = gain; break;
            case 1: notice->value = unit->level; break;
            case 2:
                if (cursor->growth_cursor >= 5u) return -1;
                notice->value = cursor->level.growth_delta;
                notice->fragment = (uint16_t)(notice->fragment + cursor->growth_cursor); break;
            case 3:
                if (cursor->learn_cursor >= cursor->level.learned_count) return -1;
                notice->magic_id = cursor->level.learned_magic[cursor->learn_cursor]; break;
            case 4: break;
            default: return -1;
            }
            return 0;
        } else return -1;
        int next = fd2_static_flow_edge(flow, n, port);
        if (next < 0) return -1;
        cursor->step = (uint8_t)next;
    }
    return -1;
}
#endif
