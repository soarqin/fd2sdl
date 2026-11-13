/* S2完整callback候选的真实Game/event_resume机制probe。
 * caller/共享尾原码oracle由convert_stage_callbacks.py独立执行；本probe不启动SDL。
 * 业务用正式consumer，呈现/输入叶仅显式ACK；检查初始actor前缀（追加构造区另域）、
 * 32B state、48B turn槽、count/roster/display/contribution与逐叶顺序。
 * FDFIELD只读；机制夹具不代表新stage自然入口或原生WGC验收。 */
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "archive.h"
#include "field_game.h"
#include "static_data.h"

/* 原80B布局的独立ABI断言；不能让Python字段表与候选图同错互证。 */
#define RAW_OFFSET(member, value) _Static_assert(offsetof(fd2_field_unit, member) == (value), "original80B " #member)
RAW_OFFSET(x, 0); RAW_OFFSET(y, 1); RAW_OFFSET(direction, 3); RAW_OFFSET(frame_phase, 4);
RAW_OFFSET(flags, 5); RAW_OFFSET(side, 6); RAW_OFFSET(unit_id, 7); RAW_OFFSET(text_id, 8);
RAW_OFFSET(attack_status, 34); RAW_OFFSET(defense_status, 35); RAW_OFFSET(hit_evasion_status, 36);
RAW_OFFSET(detail_status, 37); RAW_OFFSET(data_28_33, 40);
RAW_OFFSET(ai_behavior_raw, 52); RAW_OFFSET(ai_param_35, 53); RAW_OFFSET(ai_param_36, 54);
RAW_OFFSET(hp, 64); RAW_OFFSET(mp, 68);
_Static_assert(FD2_UNIT_DIRECTION == 2 && FD2_UNIT_FRAME_PHASE == 3 && FD2_UNIT_ID == 6 &&
    FD2_UNIT_TEXT_ID == 7 && FD2_UNIT_DESCRIPTOR == 14 && FD2_UNIT_HP == 18 && FD2_UNIT_MP == 19,
    "typed operation enum must match original field protocol");
_Static_assert(FD2_EVENT_ACTOR_TEXT == 2 && FD2_EVENT_ACTOR_ID == 3 && sizeof(((fd2_field_unit *)0)->hp) == 2 &&
    sizeof(((fd2_field_unit *)0)->mp) == 2, "query identity and original WORD width");

static int unhex(const char *text, void *out, size_t size) {
    if (strlen(text) != size * 2u) return -1;
    uint8_t *p = out;
    for (size_t i = 0; i < size; i++) {
        unsigned value = 0;
        for (unsigned n = 0; n < 2; n++) {
            unsigned char c = (unsigned char)text[i * 2u + n];
            unsigned digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10u : 99u;
            if (digit > 15) return -1;
            value = value * 16u + digit;
        }
        p[i] = (uint8_t)value;
    }
    return 0;
}

static void hex(const void *data, size_t size) {
    const uint8_t *p = data;
    for (size_t i = 0; i < size; i++) printf("%02x", p[i]);
}

static uint64_t byte_guard(const void *data, size_t size) {
    const uint8_t *p = data;
    uint64_t value = UINT64_C(14695981039346656037);
    for (size_t i = 0; i < size; i++) value = (value ^ p[i]) * UINT64_C(1099511628211);
    return value;
}

static void snapshot(const fd2_field_game *g, size_t prefix) {
    printf("\"count\":%u,\"roster_count\":%u,\"unit_prefix_guard\":\"%016" PRIx64 "\",\"state_hex\":\"",
        (unsigned)g->units.count, (unsigned)g->roster_count, byte_guard(g->units.items, prefix * 80u));
    hex(g->cell_action_completed, 32);
    printf("\",\"turn_slots_hex\":\""); hex(g->metadata.turn_events, 48);
    printf("\",\"display\":%u,\"contribution\":%u", (unsigned)g->ordinary_display_mode, g->experience_value);
}

static void close_fixture(fd2_field_game *g) {
    fd2_field_map_close(&g->map);
    fd2_field_metadata_close(&g->metadata);
    fd2_field_placements_close(&g->placements);
}

int main(int argc, char **argv) {
    if (argc != 2) return 1;
    fd2_archive field = {0};
    if (fd2_archive_open(&field, argv[1]) != 0) return 2;
    char flow[96], units_hex[15361], state_hex[65], slots_hex[97];
    unsigned id, stage, count, caller, turn, display, contribution, mode, cut;
    int read;
    while ((read = scanf("%u %95s %u %u %u %u %u %u %u %u %15360s %64s %96s",
            &id, flow, &stage, &count, &caller, &turn, &display, &contribution, &mode, &cut,
            units_hex, state_hex, slots_hex)) != EOF) {
        if (read != 13 || stage >= 33 || count > 96 || caller >= count || display > 1 || mode > 4) return 3;
        fd2_field_game *g = calloc(1, sizeof(*g)), *guard = malloc(sizeof(*guard));
        if (!g || !guard) return 4;
        if (fd2_field_map_open_stage(&g->map, &field, stage) != 0 ||
            fd2_field_metadata_open_stage(&g->metadata, &field, stage) != 0 ||
            fd2_field_placements_open_stage(&g->placements, &field, stage) != 0) return 5;
        if (unhex(units_hex, g->units.items, sizeof(g->units.items)) != 0 ||
            unhex(state_hex, g->cell_action_completed, 32) != 0 ||
            unhex(slots_hex, g->metadata.turn_events, 48) != 0) return 6;
        g->ready = 1; g->stage = stage; g->units.count = count; g->turn_number = turn;
        g->ordinary_display_mode = (uint8_t)display; g->experience_value = contribution;
        if (fd2_field_units_sprite_cache_restore(&g->units) != 0) return 7;
        *guard = *g;
        int begin = fd2_field_game_common_event_begin(g, flow, caller), status = begin, guarded = 0;
        if (mode == 4) {
            guarded = begin < 0 && !memcmp(guard, g, sizeof(*g));
            status = guarded ? 0 : -24;
        }
        unsigned leaves = 0;
        printf("{\"case_id\":%u,\"begin_result\":%d,\"checkpoints\":[", id, begin);
        while (!status && g->event_resume.active) {
            fd2_static_flow_node node; size_t actor;
            if (fd2_field_game_common_event_peek(g, &node, &actor) != 1 || actor != caller) { status = -20; break; }
            if (leaves++) printf(",");
            printf("{\"op\":\"%s\",\"args\":[", node.op);
            for (unsigned i = 0; i < 8; i++) printf("%s%d", i ? "," : "", node.args[i]);
            printf("],"); snapshot(g, count); printf("}");
            unsigned expected = !strcmp(node.op, "story") || !strcmp(node.op, "wait_key") ? FD2_FLOW_ACK : FD2_FLOW_NEXT;
            if (mode && leaves - 1u == cut) {
                if (mode == 2) g->runtime_host_quit = 1;
                *guard = *g;
                unsigned reply = mode == 1 ? (expected == FD2_FLOW_ACK ? FD2_FLOW_NEXT : FD2_FLOW_ACK) : mode == 3 ? FD2_FLOW_ERROR : expected;
                int answer = fd2_field_game_common_event_reply(g, reply);
                if (mode == 3) {
                    guarded = answer < 0 && !memcmp(guard->units.items, g->units.items, count * 80u) &&
                        !memcmp(guard->cell_action_completed, g->cell_action_completed, 32) &&
                        !memcmp(guard->metadata.turn_events, g->metadata.turn_events, 48) &&
                        guard->units.count == g->units.count && guard->roster_count == g->roster_count &&
                        guard->ordinary_display_mode == g->ordinary_display_mode && guard->experience_value == g->experience_value;
                } else guarded = answer < 0 && !memcmp(guard, g, sizeof(*g));
                status = guarded ? 0 : -21;
                break;
            }
            status = fd2_field_game_common_event_reply(g, expected);
            if (leaves > FD2_FLOW_STEP_BUDGET) { status = -22; break; }
        }
        int repeat_guard = 0;
        if (!mode && !status && !g->event_resume.active) {
            *guard = *g;
            repeat_guard = fd2_field_game_common_event_reply(g, FD2_FLOW_ACK) < 0 && !memcmp(guard, g, sizeof(*g));
            if (!repeat_guard) status = -23;
        }
        printf("],\"status\":%d,\"guarded\":%d,\"repeat_ack_guard\":%d,\"active\":%u,\"finish\":{",
            status, guarded, repeat_guard, (unsigned)g->event_resume.active);
        snapshot(g, count); printf("}}\n");
        close_fixture(g); free(guard); free(g);
    }
    fd2_archive_close(&field);
    return 0;
}
