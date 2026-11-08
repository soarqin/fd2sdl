/* 有限事件流程的只读节点与查询；原领域语义依据保留在各消费模块。
 * 节点来自 data/static/flows，执行游标仍归原 session/loot/progress。
 * 本层不执行任意代码、不拥有热状态、不解释表达式或原版资源。
 */
#ifndef FD2_STATIC_FLOW_H
#define FD2_STATIC_FLOW_H

#include <stddef.h>
#include <stdint.h>
#include <string.h>

enum {
    FD2_FLOW_NEXT = 0,
    FD2_FLOW_YES,
    FD2_FLOW_NO,
    FD2_FLOW_CANCEL,
    FD2_FLOW_ACK,
    FD2_FLOW_TEXT_READY,
    FD2_FLOW_SLOT,
    FD2_FLOW_ERROR,
    FD2_FLOW_PORT_COUNT
};
enum { FD2_FLOW_LOGIC = 1, FD2_FLOW_PRESENT = 2 };
/* 预算是损坏流程的停止门，不是脚本可配置的玩法参数。 */
#define FD2_FLOW_STEP_BUDGET 65536u

typedef struct {
    const char *id;
    const char *op;
    int32_t args[8];
    int16_t edges[FD2_FLOW_PORT_COUNT];
    uint8_t views;
} fd2_static_flow_node;

typedef struct {
    const char *id;
    const fd2_static_flow_node *nodes;
    uint16_t count;
    uint8_t entry;
} fd2_static_flow;

static inline const fd2_static_flow *fd2_static_flow_find(
        const fd2_static_flow *flows, size_t count, const char *id) {
    if (!flows || !id) return NULL;
    for (size_t i = 0; i < count; i++)
        if (!strcmp(flows[i].id, id)) return &flows[i];
    return NULL;
}

static inline const fd2_static_flow_node *fd2_static_flow_at(
        const fd2_static_flow *flow, int pc) {
    return flow && flow->nodes && pc >= 0 && (unsigned)pc < flow->count ? &flow->nodes[pc] : NULL;
}

static inline int fd2_static_flow_label(const fd2_static_flow *flow, const char *id) {
    if (!flow || !id) return -1;
    for (size_t i = 0; i < flow->count; i++)
        if (!strcmp(flow->nodes[i].id, id)) return (int)i;
    return -1;
}

static inline int fd2_static_flow_edge(const fd2_static_flow *flow,
        const fd2_static_flow_node *node, unsigned port) {
    if (!flow || !node || port >= FD2_FLOW_PORT_COUNT) return -1;
    int pc = node->edges[port];
    return fd2_static_flow_at(flow, pc) ? pc : -1;
}

#endif
